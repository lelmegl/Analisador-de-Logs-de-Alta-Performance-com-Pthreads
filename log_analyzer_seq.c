#define _POSIX_C_SOURCE 200809L // clock_gettime
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



//Declarações
typedef struct{
    //Nível 1 - básicas
    long long total_requests;
    long long total_404;
    long long total_200;
    long long total_bytes;
    double avg_bytes;
    double error_rate;

    // Nivel 2 - Intermediarias
    int requests_per_hour [24];
    long long status_dist [10];
    long long contador_http [5];
} LogStats;
LogStats stats = {0};

typedef struct{
    char string_url[1000];
    int contador_url;
}UrlCount;
UrlCount count[1000] = {0};

typedef struct {
    char string_ip[50];
    int contador_ip;
} IpCount;

IpCount count_ip[1000] = {0};
int ips_unicos = 0;



int urls_unicas = 0;
char arquivo[1024];
char ip[50];
char metodo[10];
int bytes;
int status;
int hora;
char url[1000];




//Função com switch para fazer verificação de status e incrementar os contadores
void VerificaStatus(int status){
    switch(status){
            case 200:
                stats.total_200++;
                stats.total_bytes += bytes;
                break;
            case 301:
                stats.status_dist[0]++;
                break;
            case 302:
                stats.status_dist[1]++;
                break;
            case 400:
                stats.status_dist[2]++;
                break;
            case 403:
                stats.status_dist[3]++;
                break;
            case 404:
                stats.total_404++;
                break;
            case 500:
                stats.status_dist[4]++;
                break;
            case 502:
                stats.status_dist[5]++;
                break;
            case 503:
                stats.status_dist[6]++;
                break;
            default:
                stats.status_dist[7]++;
        }
}



//Método para leitura e armazenamento dos logs
void AnalisaArquivo(){
    FILE * p;
    //Leitura do arquivo
    p = fopen("access_log_large.txt", "r");
    if (p == NULL){
        printf("Erro ao abrir o arquivo.\n");
        return;
    }
        //Lançar o arquivo no buffer
    while (fgets(arquivo, sizeof(arquivo), p) != NULL){
        //Lê também a hora: "[15/Sep/2025:HH:..." -> pula até o ':' e lê HH
        sscanf(arquivo, "%s - - [%*[^:]:%d:%*s %*s \"%s %s %*[^\"]\" %d %d", ip, &hora, metodo, url, &status, &bytes);
        //Contador para número de solicitações
        stats.total_requests += 1;
        //Contador por hora (0-23)
        if (hora >= 0 && hora < 24){
            stats.requests_per_hour[hora]++;
        }
        //Contador de método
        if (strcmp(metodo,"GET")==0){
            stats.contador_http[0]++;
        }
        else if (strcmp(metodo,"POST")==0){
            stats.contador_http[1]++;
        }
        else if (strcmp(metodo,"PUT")==0){
            stats.contador_http[2]++;
        }
        else if (strcmp(metodo,"DELETE")==0){
            stats.contador_http[3]++;
        }
        else{
            stats.contador_http[4]++;
        }
        VerificaStatus(status);

        //Verifica se a url já foi usada
        int encontrada = 0;
        for(int i =0; i<urls_unicas; i++){
            if(strcmp(url, count[i].string_url)==0){
                count[i].contador_url++;
                encontrada =1;
                break;
            }
        }

        //Se ainda não foi usada, adiciona ao vetor e inicia o contador
        if (encontrada == 0 && urls_unicas <1000){
            strcpy(count[urls_unicas].string_url, url);
            count[urls_unicas].contador_url = 1;
            urls_unicas++;
        }


        //verifica se o ip já fez alguma requisição
        int ip_encontrado = 0;
        for(int i = 0; i < ips_unicos; i++){
            if(strcmp(ip, count_ip[i].string_ip) == 0){
                count_ip[i].contador_ip++;
                ip_encontrado = 1;
                break;
            }
        }
        //Se ainda não fez nenhuma requisição, adiciona ao vetor e inicia o contador
        if (ip_encontrado == 0 && ips_unicos < 1000){
            strcpy(count_ip[ips_unicos].string_ip, ip);
            count_ip[ips_unicos].contador_ip = 1;
            ips_unicos++;
        }

    }
    fclose(p);
}


//Ordena por número de acessos (decrescente). Empate: ordem alfabética,
//igual às versões paralelas, para a saída ser a mesma
void OrdenaUrl(){
    UrlCount temp;
    for (int i=0; i<urls_unicas; i++ ){
        for(int j=i+1; j< urls_unicas; j++){
            if (count[i].contador_url < count[j].contador_url ||
                (count[i].contador_url == count[j].contador_url &&
                 strcmp(count[i].string_url, count[j].string_url) > 0)){
                temp = count[i];
                count[i] = count[j];
                count [j] = temp;
            }
        }
    }
}

void OrdenaIp() {
    IpCount temp;
    for (int i = 0; i < ips_unicos; i++) {
        for (int j = i + 1; j < ips_unicos; j++) {
            if (count_ip[i].contador_ip < count_ip[j].contador_ip ||
                (count_ip[i].contador_ip == count_ip[j].contador_ip &&
                 strcmp(count_ip[i].string_ip, count_ip[j].string_ip) > 0)) {
                temp = count_ip[i];
                count_ip[i] = count_ip[j];
                count_ip[j] = temp;
            }
        }
    }
}


//Formata número com separador de milhar: 1234567 -> "1,234,567"
char *FormataMilhar(long long v, char *buf){
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%lld", v);
    int len = (int)strlen(tmp), j = 0;
    for (int i = 0; i < len; i++){
        buf[j++] = tmp[i];
        if ((len - i - 1) % 3 == 0 && i != len - 1) buf[j++] = ',';
    }
    buf[j] = '\0';
    return buf;
}

//Porcentagem em relação ao total de requisições
double Pct(long long parte){
    return stats.total_requests ? 100.0 * parte / stats.total_requests : 0.0;
}

#define LINHA_DUPLA   "============================================================\n"
#define LINHA_SIMPLES "------------------------------------------------------------\n"



int main() {
    //Tempo de parede (mesma medição das versões paralelas)
    struct timespec tempo_inicio, tempo_fim;
    clock_gettime(CLOCK_MONOTONIC, &tempo_inicio);
    AnalisaArquivo();
    clock_gettime(CLOCK_MONOTONIC, &tempo_fim);
    double tempo_execucao = (tempo_fim.tv_sec - tempo_inicio.tv_sec) +
                            (tempo_fim.tv_nsec - tempo_inicio.tv_nsec) / 1e9;
    stats.avg_bytes = stats.total_200 ? (double)stats.total_bytes/stats.total_200 : 0.0;
    stats.error_rate = Pct(stats.total_404);

    char b[32];

    //Relatório no formato do apêndice do enunciado (mesmo das versões paralelas)
    printf(LINHA_DUPLA "ANALISADOR DE LOGS - RELATÓRIO COMPLETO\n" LINHA_DUPLA);
    printf("ARQUIVO: access_log_large.txt\n");
    printf("THREADS: 1\n");
    printf("VERSÃO: sequencial\n");
    printf("TEMPO DE EXECUÇÃO: %.4f segundos\n", tempo_execucao);

    printf(LINHA_SIMPLES "ESTATÍSTICAS BÁSICAS\n" LINHA_SIMPLES);
    printf("Total de Requisições:         %s\n", FormataMilhar(stats.total_requests, b));
    printf("Requisições 200 (OK):         %s (%.2f%%)\n", FormataMilhar(stats.total_200, b), Pct(stats.total_200));
    printf("Requisições 404 (Not Found):  %s (%.2f%%)\n", FormataMilhar(stats.total_404, b), Pct(stats.total_404));
    printf("Total de Bytes (200):         %s\n", FormataMilhar(stats.total_bytes, b));
    printf("Média de Bytes/Req (200):     %s bytes\n", FormataMilhar((long long)(stats.avg_bytes + 0.5), b));
    printf("Taxa de Erro Geral:           %.2f%%\n", stats.error_rate);

    printf(LINHA_SIMPLES "TOP 10 URLs MAIS ACESSADAS\n" LINHA_SIMPLES);
    OrdenaUrl();
    for(int i = 0; i < 10 && i < urls_unicas; i++) {
        printf("%2d. %-35s %12s acessos\n", i + 1, count[i].string_url,
               FormataMilhar(count[i].contador_url, b));
    }

    printf(LINHA_SIMPLES "TOP 10 IPs MAIS ATIVOS\n" LINHA_SIMPLES);
    OrdenaIp();
    for (int i = 0; i < 10 && i < ips_unicos; i++) {
        printf("%2d. %-35s %12s requisições\n", i + 1, count_ip[i].string_ip,
               FormataMilhar(count_ip[i].contador_ip, b));
    }

    printf(LINHA_SIMPLES "DISTRIBUIÇÃO POR HORA\n" LINHA_SIMPLES);
    for (int h = 0; h < 24; h++) {
        printf("%02dh  %12s (%5.2f%%)\n", h, FormataMilhar(stats.requests_per_hour[h], b),
               Pct(stats.requests_per_hour[h]));
    }

    //Mesma ordem das versões paralelas
    const char *nomes_status[10] = {"200 OK", "301 Moved", "302 Found", "400 Bad Req",
        "403 Forbidden", "404 Not Found", "500 Internal", "502 Bad Gateway", "503 Unavail", "Outros"};
    long long valores_status[10] = {stats.total_200, stats.status_dist[0], stats.status_dist[1],
        stats.status_dist[2], stats.status_dist[3], stats.total_404, stats.status_dist[4],
        stats.status_dist[5], stats.status_dist[6], stats.status_dist[7]};
    printf(LINHA_SIMPLES "DISTRIBUIÇÃO DE CÓDIGOS DE STATUS\n" LINHA_SIMPLES);
    for (int i = 0; i < 10; i++) {
        printf("%-16s %12s (%5.2f%%)\n", nomes_status[i], FormataMilhar(valores_status[i], b),
               Pct(valores_status[i]));
    }

    const char *nomes_metodos[5] = {"GET", "POST", "PUT", "DELETE", "OUTROS"};
    printf(LINHA_SIMPLES "ANÁLISE DE MÉTODOS HTTP\n" LINHA_SIMPLES);
    for (int i = 0; i < 5; i++) {
        printf("%-16s %12s (%5.2f%%)\n", nomes_metodos[i], FormataMilhar(stats.contador_http[i], b),
               Pct(stats.contador_http[i]));
    }

    printf(LINHA_DUPLA "FIM DO RELATÓRIO\n" LINHA_DUPLA);

    return 0;
}
