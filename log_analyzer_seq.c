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
        stats.total_requests += 1;
        if(status == 404){stats.total_404++; stats.total_bytes += bytes;}
        else if(status == 200){stats.total_200++; stats.total_bytes += bytes; }
    }
    
    fclose(p);
    printf("%lld\n", stats.total_requests);
    return 0;


}
