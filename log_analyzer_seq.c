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
        sscanf(arquivo, "%s - - %*s %*s \"%s %s %*[^\"]\" %d %d", ip, metodo, url, &status, &bytes);
        //Contador para número de solicitações
        stats.total_requests += 1;
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


void OrdenaUrl(){
    UrlCount temp;
    for (int i=0; i<urls_unicas; i++ ){
        for(int j=i+1; j< urls_unicas; j++){
            if (count[i].contador_url < count[j].contador_url){
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
            if (count_ip[i].contador_ip < count_ip[j].contador_ip) {
                temp = count_ip[i];
                count_ip[i] = count_ip[j];
                count_ip[j] = temp;
            }
        }
    }
}



int main() {
    clock_t tempo_inicio = clock();
    AnalisaArquivo();
    clock_t tempo_fim = clock();
    double tempo_execucao = (double)(tempo_fim - tempo_inicio) / CLOCKS_PER_SEC;
    stats.avg_bytes = (double)stats.total_bytes/stats.total_200;
    stats.error_rate = ((double)stats.total_404/stats.total_requests) * 100.0;




    printf("=============================================================\n");
    printf("ANALISADOR DE LOGS - RELATÓRIO COMPLETO\n");
    printf("=============================================================\n");
    printf(" \n");
    printf("ARQUIVO: access_log_large.txt\n");
    printf("THREADS: 1\n");
    printf("TEMPO DE EXECUÇÃO: %.2f segundos\n", tempo_execucao);
    printf(" \n");
    
    
    
    printf("=============================================================\n");
    printf("ESTATÍSTICAS BÁSICAS\n");
    printf("=============================================================\n");

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

    OrdenaUrl();
    for(int i = 0; i < 10 && i < urls_unicas; i++) {
        printf("%d. %s - %d acessos\n", i + 1, count[i].string_url, count[i].contador_url);
    }


    printf("=============================================================\n");
    printf("TOP 10 IPs  MAIS ATIVOS\n");
    printf("=============================================================\n");
    
    OrdenaIp();
    
    for (int i = 0; i < 10 && i < ips_unicos; i++) {
        printf("%d. %s - %d  requisições\n", i + 1, count_ip[i].string_ip, count_ip[i].contador_ip);
    }
    printf(" \n");


    printf("=============================================================\n");
    printf("DISTRIBUIÇÃO DE CÓDIGOS DE STATUS\n");
    printf("=============================================================\n");
    
    printf("200 OK:               %lld (%.2f%%)\n", stats.total_200, (double)stats.total_200 / stats.total_requests * 100.0);
    printf("301 Moved:             %lld (%.2f%%)\n", stats.status_dist[0], (double)stats.status_dist[0] / stats.total_requests * 100.0);
    printf("302 Found:             %lld (%.2f%%)\n", stats.status_dist[1], (double)stats.status_dist[1] / stats.total_requests * 100.0);
    printf("400 Bad Req:           %lld (%.2f%%)\n", stats.status_dist[2], (double)stats.status_dist[2] / stats.total_requests * 100.0);
    printf("403 Forbidden:         %lld (%.2f%%)\n", stats.status_dist[3], (double)stats.status_dist[3] / stats.total_requests * 100.0);
    printf("404 Not Found:         %lld (%.2f%%)\n", stats.total_404, (double)stats.total_404 / stats.total_requests * 100.0);
    printf("500 Internal:          %lld (%.2f%%)\n", stats.status_dist[4], (double)stats.status_dist[4] / stats.total_requests * 100.0);
    printf("502 Bad Gateway:       %lld (%.2f%%)\n", stats.status_dist[5], (double)stats.status_dist[5] / stats.total_requests * 100.0);
    printf("503 Unavail:            %lld (%.2f%%)\n", stats.status_dist[6], (double)stats.status_dist[6] / stats.total_requests * 100.0);
    printf("Outros:                 %lld (%.2f%%)\n", stats.status_dist[7], (double)stats.status_dist[7] / stats.total_requests * 100.0);
    printf(" \n");


    printf("=============================================================\n");
    printf("ANÁLISE DE MÉTODOS HTTP\n");
    printf("=============================================================\n");
    printf("GET:       %lld (%.2f%%)\n", stats.contador_http[0], (double)stats.contador_http[0] / stats.total_requests * 100.0);
    printf("POST:      %lld (%.2f%%)\n", stats.contador_http[1], (double)stats.contador_http[1] / stats.total_requests * 100.0);
    printf("PUT:       %lld (%.2f%%)\n", stats.contador_http[2], (double)stats.contador_http[2] / stats.total_requests * 100.0);
    printf("DELETE:    %lld (%.2f%%)\n", stats.contador_http[3], (double)stats.contador_http[3] / stats.total_requests * 100.0);
    printf("OUTROS:    %lld (%.2f%%)\n", stats.contador_http[4], (double)stats.contador_http[4] / stats.total_requests * 100.0);
    printf(" \n");

    return 0;
}
