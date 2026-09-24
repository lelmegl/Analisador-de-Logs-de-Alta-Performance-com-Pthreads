#include <stdio.h>
#include <stdlib.h>

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

} LogStats;





int main() {
    LogStats stats = {0};
    FILE * p;
    char arquivo[1024];

    //Leitura do arquivo
    p = fopen("access_log_large.txt", "r");
    if (p == NULL){
        printf("Erro ao abrir o arquivo.\n");
        return 1;
    }
    char ip[50];
    char metodo[10];
    int status;
    int bytes;
    //Lançar o arquivo no buffer
    while (fgets(arquivo, sizeof(arquivo), p) != NULL){
        sscanf(arquivo, "%s - - %*s %*s \"%s %*[^\"]\" %d %d", ip, metodo, &status, &bytes);
        //Contador para número de solicitações
        stats.total_requests += 1;
        //Switch para fazer verificação de status e incrementar os contadores
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
    stats.avg_bytes = (double)stats.total_bytes/stats.total_requests;
    stats.error_rate = ((double)stats.total_404/stats.total_200) * 100.0;

    fclose(p);
    printf("=============================================================\n");
    printf("ANALISADOR DE LOGS - RELATÓRIO COMPLETO\n");
    printf("=============================================================\n");
    printf(" \n");
    
    
    
    printf("=============================================================\n");
    printf("ESTATÍSTICAS BÁSICAS\n");
    printf("=============================================================\n");
    printf(" \n");
    printf("Total de Requisições:             %lld\n", stats.total_requests);
    printf("Requisições 200 (OK):             %lld\n", stats.total_200);
    printf("Requisições 404 (Not Found):      %lld\n", stats.total_404);
    printf("Total de Bytes (Requisições 200): %lld\n", stats.total_bytes);
    printf("Média de Bytes/Req:               %.0f bytes\n", stats.avg_bytes);
    printf("Taxa de Erro Geral:               %.2f%%\n", stats.error_rate);
    printf(" \n");
    printf("=============================================================\n");
    printf("TOP 10 URLs MAIS ACESSADAS\n");
    printf("=============================================================\n");
    printf(" \n");
    return 0;


}
