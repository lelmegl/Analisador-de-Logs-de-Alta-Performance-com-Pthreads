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
    int i = 0;
    //Lançar o arquivo no buffer
    while (fgets(arquivo, sizeof(arquivo), p) != NULL){
        i++;
        sscanf(arquivo, "%s - - %*s %*s \"%s %*[^\"]\" %d %d", ip, metodo, &status, &bytes);
        printf("%s | %s | %d | %d\n", ip, metodo, status, bytes);
        if (i == 10) break;
    }
    
    fclose(p);

    return 0;


}
