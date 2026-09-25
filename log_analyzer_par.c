/*
 * log_analyzer_par.c
 * Versão paralela com estado compartilhado + mutex global.
 *
 * - O arquivo é dividido em N blocos de bytes (um por thread) com fseek.
 * - Todas as threads atualizam a mesma estrutura global.
 * - Cada atualização de estatística é uma seção crítica: trava o mutex,
 *   atualiza, destrava (6 seções críticas por linha).
 *
 * Uso: ./log_analyzer_par <num_threads> [arquivo]
 */
#include "log_common.h"   /* deve vir primeiro */
#include <pthread.h>

#define MAX_THREADS 256

static LogStats global;                                   /* compartilhado */
static pthread_mutex_t mtx_global = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int id;
    const char *arquivo;
    long long inicio;
    long long fim;
} ArgThread;

static void *trabalhador(void *arg) {
    ArgThread *a = (ArgThread *)arg;
    LeitorBloco l;
    LogEntry e;

    if (!leitor_abre(&l, a->arquivo)) {
        perror("thread: erro ao abrir arquivo");
        return NULL;
    }
    leitor_posiciona(&l, a->inicio, a->fim);

    while (leitor_proxima(&l)) {
        /* Parsing é trabalho independente: fica FORA da seção crítica */
        if (!parse_linha(l.linha, &e)) {
            pthread_mutex_lock(&mtx_global);
            global.linhas_invalidas++;
            pthread_mutex_unlock(&mtx_global);
            continue;
        }
        int st = indice_status(e.status);
        int m = indice_metodo(e.metodo);

        /* 1) Total de requisições */
        pthread_mutex_lock(&mtx_global);
        global.total_requests++;
        pthread_mutex_unlock(&mtx_global);

        /* 2) Status (+ contadores 200/404 e bytes) */
        pthread_mutex_lock(&mtx_global);
        global.status_dist[st]++;
        if (st == ST_200) {
            global.total_200++;
            global.total_bytes += e.bytes;
        } else if (st == ST_404) {
            global.total_404++;
        }
        pthread_mutex_unlock(&mtx_global);

        /* 3) Método HTTP */
        pthread_mutex_lock(&mtx_global);
        global.method_dist[m]++;
        pthread_mutex_unlock(&mtx_global);

        /* 4) Distribuição por hora */
        pthread_mutex_lock(&mtx_global);
        global.requests_per_hour[e.hora]++;
        pthread_mutex_unlock(&mtx_global);

        /* 5) Contagem de URLs (tabela hash compartilhada) */
        pthread_mutex_lock(&mtx_global);
        th_incrementa(&global.urls, e.url, 1);
        pthread_mutex_unlock(&mtx_global);

        /* 6) Contagem de IPs (tabela hash compartilhada) */
        pthread_mutex_lock(&mtx_global);
        th_incrementa(&global.ips, e.ip, 1);
        pthread_mutex_unlock(&mtx_global);
    }

    leitor_fecha(&l);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <num_threads> [arquivo]\n", argv[0]);
        return 1;
    }
    int n = atoi(argv[1]);
    const char *arquivo = (argc >= 3) ? argv[2] : ARQUIVO_PADRAO;
    if (n < 1 || n > MAX_THREADS) {
        fprintf(stderr, "Número de threads deve estar entre 1 e %d\n", MAX_THREADS);
        return 1;
    }

    long long tam = tamanho_arquivo(arquivo);
    if (tam < 0) {
        perror(arquivo);
        return 1;
    }

    stats_init(&global);
    pthread_t threads[MAX_THREADS];
    ArgThread args[MAX_THREADS];
    long long bloco = tam / n;

    double t0 = agora();

    for (int i = 0; i < n; i++) {
        args[i].id = i;
        args[i].arquivo = arquivo;
        args[i].inicio = (long long)i * bloco;
        args[i].fim = (i == n - 1) ? tam : (long long)(i + 1) * bloco;
        if (pthread_create(&threads[i], NULL, trabalhador, &args[i]) != 0) {
            fprintf(stderr, "Erro ao criar thread %d\n", i);
            return 1;
        }
    }
    for (int i = 0; i < n; i++)
        pthread_join(threads[i], NULL);

    double tempo = agora() - t0;

    imprime_relatorio(arquivo, n, "VERSÃO: paralela (mutex global)", tempo, &global);

    stats_libera(&global);
    pthread_mutex_destroy(&mtx_global);
    return 0;
}
