/*
 * log_analyzer_par_optimized.c
 * Versão paralela com REDUÇÃO LOCAL (Desafio 1) + GRANULARIDADE (Desafio 2).
 *
 * - Cada thread acumula tudo em uma LogStats LOCAL (na própria pilha),
 *   sem nenhum lock durante o processamento.
 * - No fim, cada thread chama merge_stats(), que trava o mutex global
 *   UMA única vez para somar seu resultado parcial ao global.
 *
 * Divisão do trabalho:
 *   - sem block_size: estática, 1 bloco de tam/N bytes por thread (igual à versão mutex)
 *   - com block_size: dinâmica, blocos de block_size bytes distribuídos
 *     por uma fila (contador compartilhado); cada thread pega o próximo
 *     bloco livre até acabar o arquivo.
 *
 * Uso: ./log_analyzer_par_optimized <num_threads> [arquivo] [block_size_bytes]
 *   ex: ./log_analyzer_par_optimized 4
 *       ./log_analyzer_par_optimized 4 access_log_large.txt 65536
 */
#include "log_common.h"   /* deve vir primeiro */
#include <pthread.h>

#define MAX_THREADS 256

typedef struct {
    LogStats stats;
    pthread_mutex_t mutex;
} GlobalStats;

static GlobalStats global;

/* Fila de blocos (modo dinâmico) */
static long long tam_arquivo;
static long long tam_bloco;
static long long proximo_bloco = 0;
static pthread_mutex_t mtx_fila = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int id;
    const char *arquivo;
    int dinamico;
    long long inicio;    /* usados só no modo estático */
    long long fim;
    long long blocos_processados;
} ArgThread;

/* Redução: soma o resultado local no global (uma seção crítica por thread) */
static void merge_stats(GlobalStats *g, const LogStats *local) {
    pthread_mutex_lock(&g->mutex);

    g->stats.total_requests   += local->total_requests;
    g->stats.total_200        += local->total_200;
    g->stats.total_404        += local->total_404;
    g->stats.total_bytes      += local->total_bytes;
    g->stats.linhas_invalidas += local->linhas_invalidas;
    for (int h = 0; h < 24; h++)
        g->stats.requests_per_hour[h] += local->requests_per_hour[h];
    for (int i = 0; i < N_STATUS; i++)
        g->stats.status_dist[i] += local->status_dist[i];
    for (int i = 0; i < N_METODOS; i++)
        g->stats.method_dist[i] += local->method_dist[i];

    /* Mescla as tabelas hash: soma os contadores de cada URL/IP.
       Não dá para mesclar só os Top 10 locais: uma URL que é a 11ª em
       todas as threads pode ser a 1ª no total. */
    for (size_t i = 0; i < local->urls.cap; i++)
        if (local->urls.slots[i].chave)
            th_incrementa(&g->stats.urls, local->urls.slots[i].chave, local->urls.slots[i].cont);
    for (size_t i = 0; i < local->ips.cap; i++)
        if (local->ips.slots[i].chave)
            th_incrementa(&g->stats.ips, local->ips.slots[i].chave, local->ips.slots[i].cont);

    pthread_mutex_unlock(&g->mutex);
}

/* Pega o próximo bloco da fila. Retorna 0 quando não há mais blocos. */
static int pega_bloco(long long *inicio, long long *fim) {
    pthread_mutex_lock(&mtx_fila);
    long long b = proximo_bloco++;
    pthread_mutex_unlock(&mtx_fila);

    *inicio = b * tam_bloco;
    if (*inicio >= tam_arquivo) return 0;
    *fim = *inicio + tam_bloco;
    if (*fim > tam_arquivo) *fim = tam_arquivo;
    return 1;
}

/* Processa um bloco acumulando na estrutura local: SEM LOCKS */
static void processa_bloco(LeitorBloco *l, LogStats *local) {
    LogEntry e;
    while (leitor_proxima(l)) {
        if (parse_linha(l->linha, &e))
            stats_registra(local, &e);
        else
            local->linhas_invalidas++;
    }
}

static void *trabalhador(void *arg) {
    ArgThread *a = (ArgThread *)arg;
    LeitorBloco l;

    /* Estrutura local na PILHA da thread: evita false sharing
       (se ficasse num vetor global, contadores de threads diferentes
       poderiam cair na mesma linha de cache). */
    LogStats local;
    stats_init(&local);

    if (!leitor_abre(&l, a->arquivo)) {
        perror("thread: erro ao abrir arquivo");
        stats_libera(&local);
        return NULL;
    }

    if (a->dinamico) {
        long long ini, fim;
        while (pega_bloco(&ini, &fim)) {
            leitor_posiciona(&l, ini, fim);
            processa_bloco(&l, &local);
            a->blocos_processados++;
        }
    } else {
        leitor_posiciona(&l, a->inicio, a->fim);
        processa_bloco(&l, &local);
        a->blocos_processados = 1;
    }

    leitor_fecha(&l);
    merge_stats(&global, &local);
    stats_libera(&local);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <num_threads> [arquivo] [block_size_bytes]\n", argv[0]);
        return 1;
    }
    int n = atoi(argv[1]);
    const char *arquivo = (argc >= 3) ? argv[2] : ARQUIVO_PADRAO;
    long long bs = (argc >= 4) ? atoll(argv[3]) : 0;

    if (n < 1 || n > MAX_THREADS) {
        fprintf(stderr, "Número de threads deve estar entre 1 e %d\n", MAX_THREADS);
        return 1;
    }
    if (argc >= 4 && bs <= 0) {
        fprintf(stderr, "block_size deve ser > 0\n");
        return 1;
    }

    tam_arquivo = tamanho_arquivo(arquivo);
    if (tam_arquivo < 0) {
        perror(arquivo);
        return 1;
    }

    int dinamico = (bs > 0);
    tam_bloco = bs;

    stats_init(&global.stats);
    pthread_mutex_init(&global.mutex, NULL);

    pthread_t threads[MAX_THREADS];
    ArgThread args[MAX_THREADS];
    long long bloco_estatico = tam_arquivo / n;

    double t0 = agora();

    for (int i = 0; i < n; i++) {
        args[i].id = i;
        args[i].arquivo = arquivo;
        args[i].dinamico = dinamico;
        args[i].inicio = (long long)i * bloco_estatico;
        args[i].fim = (i == n - 1) ? tam_arquivo : (long long)(i + 1) * bloco_estatico;
        args[i].blocos_processados = 0;
        if (pthread_create(&threads[i], NULL, trabalhador, &args[i]) != 0) {
            fprintf(stderr, "Erro ao criar thread %d\n", i);
            return 1;
        }
    }
    for (int i = 0; i < n; i++)
        pthread_join(threads[i], NULL);

    double tempo = agora() - t0;

    char extra[128];
    if (dinamico)
        snprintf(extra, sizeof(extra),
                 "VERSÃO: paralela (redução local)\nBLOCO: %lld bytes (distribuição dinâmica)", bs);
    else
        snprintf(extra, sizeof(extra),
                 "VERSÃO: paralela (redução local)\nBLOCO: estático (1 por thread)");

    imprime_relatorio(arquivo, n, extra, tempo, &global.stats);

    stats_libera(&global.stats);
    pthread_mutex_destroy(&global.mutex);
    pthread_mutex_destroy(&mtx_fila);
    return 0;
}
