/*
 * log_common.h
 * Código compartilhado entre as versões paralelas do analisador de logs:
 *   - parsing de uma linha do log
 *   - tabela hash (URLs e IPs) com crescimento dinâmico
 *   - seleção Top K
 *   - leitura de um bloco [inicio, fim) do arquivo em bytes
 *   - impressão do relatório
 *
 * IMPORTANTE: incluir este header ANTES de qualquer outro include,
 * por causa das macros de configuração abaixo.
 */
#ifndef LOG_COMMON_H
#define LOG_COMMON_H

#define _FILE_OFFSET_BITS 64          /* arquivos > 2 GB no Linux */
#define _POSIX_C_SOURCE 200809L       /* clock_gettime, fseeko, ftello */
#define __USE_MINGW_ANSI_STDIO 1      /* %lld correto no MinGW (Windows) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* fseek/ftell de 64 bits (long é 32 bits no Windows) */
#ifdef _WIN32
#define FSEEK _fseeki64
#define FTELL _ftelli64
#else
#define FSEEK fseeko
#define FTELL ftello
#endif

#define MAX_LINE   4096
#define TOP_K      10
#define N_STATUS   10
#define N_METODOS  5
#define ARQUIVO_PADRAO "access_log_large.txt"

enum { ST_200, ST_301, ST_302, ST_400, ST_403, ST_404,
       ST_500, ST_502, ST_503, ST_OUTROS };
enum { M_GET, M_POST, M_PUT, M_DELETE, M_OUTROS };

/* ======================================================================
 * Parsing
 * ====================================================================== */
typedef struct {
    char ip[64];
    char metodo[16];
    char url[1024];
    int hora;
    int status;
    long long bytes;
} LogEntry;

/*
 * Formato: IP - - [15/Sep/2025:15:30:00 -0300] "GET /url HTTP/1.1" 200 1500 "UA"
 * %*[^:]: pula "[15/Sep/2025" e o ':'  ->  %d lê a hora
 * Retorna 1 se a linha é válida, 0 caso contrário.
 * sscanf é reentrante (não usa estado global), então pode ser usado por threads.
 */
static inline int parse_linha(const char *linha, LogEntry *e) {
    int n = sscanf(linha,
                   "%63s - - [%*[^:]:%d:%*s %*s \"%15s %1023s %*[^\"]\" %d %lld",
                   e->ip, &e->hora, e->metodo, e->url, &e->status, &e->bytes);
    return n == 6 && e->hora >= 0 && e->hora < 24;
}

static inline int indice_status(int status) {
    switch (status) {
        case 200: return ST_200;
        case 301: return ST_301;
        case 302: return ST_302;
        case 400: return ST_400;
        case 403: return ST_403;
        case 404: return ST_404;
        case 500: return ST_500;
        case 502: return ST_502;
        case 503: return ST_503;
        default:  return ST_OUTROS;
    }
}

static inline int indice_metodo(const char *m) {
    if (strcmp(m, "GET") == 0)    return M_GET;
    if (strcmp(m, "POST") == 0)   return M_POST;
    if (strcmp(m, "PUT") == 0)    return M_PUT;
    if (strcmp(m, "DELETE") == 0) return M_DELETE;
    return M_OUTROS;
}

/* ======================================================================
 * Tabela hash (endereçamento aberto, sondagem linear, FNV-1a)
 * Troca a busca linear O(k) por linha da versão sequencial por O(1) médio.
 * ====================================================================== */
typedef struct {
    char *chave;          /* NULL = slot vazio */
    long long cont;
} Entrada;

typedef struct {
    Entrada *slots;
    size_t cap;           /* sempre potência de 2 */
    size_t tam;
} TabelaHash;

static inline uint64_t fnv1a(const char *s) {
    uint64_t h = 1469598103934665603ULL;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 1099511628211ULL;
    }
    return h;
}

static inline char *duplica_str(const char *s) {
    size_t n = strlen(s) + 1;
    char *c = malloc(n);
    if (!c) { perror("malloc"); exit(1); }
    memcpy(c, s, n);
    return c;
}

static inline void th_init(TabelaHash *t, size_t cap) {
    t->cap = cap;
    t->tam = 0;
    t->slots = calloc(cap, sizeof(Entrada));
    if (!t->slots) { perror("calloc"); exit(1); }
}

static inline void th_libera(TabelaHash *t) {
    for (size_t i = 0; i < t->cap; i++) free(t->slots[i].chave);
    free(t->slots);
    t->slots = NULL;
    t->cap = t->tam = 0;
}

/* Insere sem duplicar a string (usado no redimensionamento) */
static inline void th_insere_ptr(TabelaHash *t, char *chave, long long cont) {
    size_t mask = t->cap - 1;
    size_t i = (size_t)fnv1a(chave) & mask;
    while (t->slots[i].chave) i = (i + 1) & mask;
    t->slots[i].chave = chave;
    t->slots[i].cont = cont;
    t->tam++;
}

static inline void th_redimensiona(TabelaHash *t) {
    TabelaHash nova;
    th_init(&nova, t->cap * 2);
    for (size_t i = 0; i < t->cap; i++)
        if (t->slots[i].chave)
            th_insere_ptr(&nova, t->slots[i].chave, t->slots[i].cont);
    free(t->slots);
    *t = nova;
}

/* Soma 'qtd' ao contador da chave (cria a entrada se não existir) */
static inline void th_incrementa(TabelaHash *t, const char *chave, long long qtd) {
    if ((t->tam + 1) * 4 > t->cap * 3)      /* fator de carga máx. 75% */
        th_redimensiona(t);
    size_t mask = t->cap - 1;
    size_t i = (size_t)fnv1a(chave) & mask;
    while (t->slots[i].chave) {
        if (strcmp(t->slots[i].chave, chave) == 0) {
            t->slots[i].cont += qtd;
            return;
        }
        i = (i + 1) & mask;
    }
    t->slots[i].chave = duplica_str(chave);
    t->slots[i].cont = qtd;
    t->tam++;
}

/* ======================================================================
 * Top K: mantém um vetor ordenado de tamanho K (inserção) -> O(n * K)
 * Empate: ordem alfabética, para a saída ser determinística.
 * ====================================================================== */
typedef struct {
    const char *chave;
    long long cont;
} ItemTop;

static inline int vem_antes(long long ca, const char *ka, long long cb, const char *kb) {
    if (ca != cb) return ca > cb;
    return strcmp(ka, kb) < 0;
}

static inline int th_top_k(const TabelaHash *t, ItemTop *top, int k) {
    int n = 0;
    for (size_t s = 0; s < t->cap; s++) {
        const Entrada *e = &t->slots[s];
        if (!e->chave) continue;
        if (n == k && !vem_antes(e->cont, e->chave, top[k - 1].cont, top[k - 1].chave))
            continue;
        int j = (n < k) ? n++ : k - 1;
        while (j > 0 && vem_antes(e->cont, e->chave, top[j - 1].cont, top[j - 1].chave)) {
            top[j] = top[j - 1];
            j--;
        }
        top[j].chave = e->chave;
        top[j].cont = e->cont;
    }
    return n;
}

/* ======================================================================
 * Estatísticas
 * ====================================================================== */
typedef struct {
    /* Nível 1 */
    long long total_requests;
    long long total_404;
    long long total_200;
    long long total_bytes;          /* apenas requisições 200 */
    /* Nível 2 */
    long long requests_per_hour[24];
    long long status_dist[N_STATUS];
    long long method_dist[N_METODOS];
    TabelaHash urls;
    TabelaHash ips;
    long long linhas_invalidas;
} LogStats;

static inline void stats_init(LogStats *s) {
    memset(s, 0, sizeof(*s));
    th_init(&s->urls, 256);
    th_init(&s->ips, 256);
}

static inline void stats_libera(LogStats *s) {
    th_libera(&s->urls);
    th_libera(&s->ips);
}

/* Atualiza TODAS as estatísticas com uma linha (sem nenhuma sincronização) */
static inline void stats_registra(LogStats *s, const LogEntry *e) {
    int st = indice_status(e->status);
    s->total_requests++;
    s->status_dist[st]++;
    if (st == ST_200) {
        s->total_200++;
        s->total_bytes += e->bytes;
    } else if (st == ST_404) {
        s->total_404++;
    }
    s->method_dist[indice_metodo(e->metodo)]++;
    s->requests_per_hour[e->hora]++;
    th_incrementa(&s->urls, e->url, 1);
    th_incrementa(&s->ips, e->ip, 1);
}

/* Mesmas definições da versão sequencial */
static inline double media_bytes(const LogStats *s) {
    return s->total_200 ? (double)s->total_bytes / s->total_200 : 0.0;
}
static inline double taxa_erro(const LogStats *s) {
    return s->total_requests ? 100.0 * s->total_404 / s->total_requests : 0.0;
}

/* ======================================================================
 * Leitura de um bloco [inicio, fim) do arquivo
 * Regra: uma linha pertence ao bloco em que ela COMEÇA.
 * Se inicio > 0, volta 1 byte e descarta até o '\n': assim, se o bloco
 * começa exatamente no início de uma linha, ela não é perdida, e se começa
 * no meio de uma linha, esse pedaço fica com o bloco anterior.
 * ====================================================================== */
typedef struct {
    FILE *f;
    long long pos;          /* offset (bytes) do início da próxima linha */
    long long fim;
    char linha[MAX_LINE];
} LeitorBloco;

static inline int leitor_abre(LeitorBloco *l, const char *arquivo) {
    l->f = fopen(arquivo, "rb");     /* binário: offsets exatos também no Windows */
    return l->f != NULL;
}

static inline void leitor_fecha(LeitorBloco *l) {
    if (l->f) fclose(l->f);
    l->f = NULL;
}

static inline void leitor_posiciona(LeitorBloco *l, long long inicio, long long fim) {
    l->fim = fim;
    if (inicio <= 0) {
        FSEEK(l->f, 0, SEEK_SET);
        l->pos = 0;
        return;
    }
    FSEEK(l->f, inicio - 1, SEEK_SET);
    l->pos = inicio - 1;
    int c;
    while ((c = getc(l->f)) != EOF) {
        l->pos++;
        if (c == '\n') break;
    }
}

/* Lê a próxima linha do bloco em l->linha. Retorna 0 quando o bloco acaba. */
static inline int leitor_proxima(LeitorBloco *l) {
    if (l->pos >= l->fim) return 0;
    if (!fgets(l->linha, MAX_LINE, l->f)) return 0;
    l->pos += (long long)strlen(l->linha);
    return 1;
}

static inline long long tamanho_arquivo(const char *arquivo) {
    FILE *f = fopen(arquivo, "rb");
    if (!f) return -1;
    FSEEK(f, 0, SEEK_END);
    long long t = (long long)FTELL(f);
    fclose(f);
    return t;
}

/* ======================================================================
 * Utilidades
 * ====================================================================== */
static inline double agora(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);     /* tempo de parede, não de CPU */
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* 1234567 -> "1,234,567" */
static inline const char *fmt_milhar(long long v, char *buf) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%lld", v < 0 ? -v : v);
    int len = (int)strlen(tmp), j = 0;
    if (v < 0) buf[j++] = '-';
    for (int i = 0; i < len; i++) {
        buf[j++] = tmp[i];
        if ((len - i - 1) % 3 == 0 && i != len - 1) buf[j++] = ',';
    }
    buf[j] = '\0';
    return buf;
}

static inline double pct(long long parte, long long total) {
    return total ? 100.0 * parte / total : 0.0;
}

#define LINHA_DUPLA "============================================================\n"
#define LINHA_SIMPLES "------------------------------------------------------------\n"

static inline void imprime_relatorio(const char *arquivo, int threads, const char *extra,
                              double tempo, const LogStats *s) {
    char b1[32];
    long long tot = s->total_requests;

    printf(LINHA_DUPLA "ANALISADOR DE LOGS - RELATÓRIO COMPLETO\n" LINHA_DUPLA);
    printf("ARQUIVO: %s\n", arquivo);
    printf("THREADS: %d\n", threads);
    if (extra) printf("%s\n", extra);
    printf("TEMPO DE EXECUÇÃO: %.4f segundos\n", tempo);

    printf(LINHA_SIMPLES "ESTATÍSTICAS BÁSICAS\n" LINHA_SIMPLES);
    printf("Total de Requisições:         %s\n", fmt_milhar(tot, b1));
    printf("Requisições 200 (OK):         %s (%.2f%%)\n", fmt_milhar(s->total_200, b1), pct(s->total_200, tot));
    printf("Requisições 404 (Not Found):  %s (%.2f%%)\n", fmt_milhar(s->total_404, b1), pct(s->total_404, tot));
    printf("Total de Bytes (200):         %s\n", fmt_milhar(s->total_bytes, b1));
    printf("Média de Bytes/Req (200):     %s bytes\n", fmt_milhar((long long)(media_bytes(s) + 0.5), b1));
    printf("Taxa de Erro Geral:           %.2f%%\n", taxa_erro(s));
    if (s->linhas_invalidas)
        printf("Linhas inválidas ignoradas:   %s\n", fmt_milhar(s->linhas_invalidas, b1));

    ItemTop top[TOP_K];
    int n;

    printf(LINHA_SIMPLES "TOP 10 URLs MAIS ACESSADAS\n" LINHA_SIMPLES);
    n = th_top_k(&s->urls, top, TOP_K);
    for (int i = 0; i < n; i++)
        printf("%2d. %-35s %12s acessos\n", i + 1, top[i].chave, fmt_milhar(top[i].cont, b1));

    printf(LINHA_SIMPLES "TOP 10 IPs MAIS ATIVOS\n" LINHA_SIMPLES);
    n = th_top_k(&s->ips, top, TOP_K);
    for (int i = 0; i < n; i++)
        printf("%2d. %-35s %12s requisições\n", i + 1, top[i].chave, fmt_milhar(top[i].cont, b1));

    printf(LINHA_SIMPLES "DISTRIBUIÇÃO POR HORA\n" LINHA_SIMPLES);
    for (int h = 0; h < 24; h++)
        printf("%02dh  %12s (%5.2f%%)\n", h, fmt_milhar(s->requests_per_hour[h], b1),
               pct(s->requests_per_hour[h], tot));

    static const char *nomes_status[N_STATUS] = {
        "200 OK", "301 Moved", "302 Found", "400 Bad Req", "403 Forbidden",
        "404 Not Found", "500 Internal", "502 Bad Gateway", "503 Unavail", "Outros"};
    printf(LINHA_SIMPLES "DISTRIBUIÇÃO DE CÓDIGOS DE STATUS\n" LINHA_SIMPLES);
    for (int i = 0; i < N_STATUS; i++)
        printf("%-16s %12s (%5.2f%%)\n", nomes_status[i], fmt_milhar(s->status_dist[i], b1),
               pct(s->status_dist[i], tot));

    static const char *nomes_metodos[N_METODOS] = {"GET", "POST", "PUT", "DELETE", "OUTROS"};
    printf(LINHA_SIMPLES "ANÁLISE DE MÉTODOS HTTP\n" LINHA_SIMPLES);
    for (int i = 0; i < N_METODOS; i++)
        printf("%-16s %12s (%5.2f%%)\n", nomes_metodos[i], fmt_milhar(s->method_dist[i], b1),
               pct(s->method_dist[i], tot));

    printf(LINHA_DUPLA "FIM DO RELATÓRIO\n" LINHA_DUPLA);
}

#endif /* LOG_COMMON_H */
