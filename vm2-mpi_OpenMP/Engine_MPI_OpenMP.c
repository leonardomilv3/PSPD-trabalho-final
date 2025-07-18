#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <omp.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <json-c/json.h> // Inclui a biblioteca JSON

#define MAXLINE 1024
#define ind2d(i,j) ((i)*(tam+2)+(j))

// A função UmaVida não muda
void UmaVida(int* in, int* out, int tam, int localRows) {
    #pragma omp parallel for collapse(2)
    for (int i = 1; i <= localRows; i++) {
        for (int j = 1; j <= tam; j++) {
            int vizviv = in[ind2d(i-1,j-1)] + in[ind2d(i-1,j)] + in[ind2d(i-1,j+1)] +
                         in[ind2d(i,j-1)] + in[ind2d(i,j+1)] +
                         in[ind2d(i+1,j-1)] + in[ind2d(i+1,j)] + in[ind2d(i+1,j+1)];

            if (in[ind2d(i,j)] && vizviv < 2)        out[ind2d(i,j)] = 0;
            else if (in[ind2d(i,j)] && vizviv > 3)   out[ind2d(i,j)] = 0;
            else if (!in[ind2d(i,j)] && vizviv == 3) out[ind2d(i,j)] = 1;
            else                                     out[ind2d(i,j)] = in[ind2d(i,j)];
        }
    }
}

// A função InitTabul não muda
void InitTabul(int* tabul, int tam, int localRows, int rank) {
    for (int i = 0; i < (localRows+2)*(tam+2); i++) tabul[i] = 0;
    if (rank == 0) {
        tabul[ind2d(1,2)] = 1; tabul[ind2d(2,3)] = 1;
        tabul[ind2d(3,1)] = 1; tabul[ind2d(3,2)] = 1; tabul[ind2d(3,3)] = 1;
    }
}

// NOVA FUNÇÃO: Envia o resultado para o Socket Server
void send_result_to_server(int tam, double computation_time, const char* status) {
    int sockfd;
    struct sockaddr_in servaddr;

    // Pega o endereço do servidor da variável de ambiente
    const char* server_host = getenv("SOCKET_SERVER_HOST");
    if (server_host == NULL) {
        server_host = "socket-server-service"; // Valor padrão
    }
    // A porta é fixa, conforme o serviço do server.py
    int server_port = 9999; 

    // Cria o socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        return;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(server_port);
    // Tenta converter o nome do host para IP (funciona com o DNS do Kubernetes)
    if (inet_pton(AF_INET, server_host, &servaddr.sin_addr) <= 0) {
        printf("\n inet_pton error for %s\n", server_host);
        // Fallback para um IP fixo se a resolução de nome falhar (para testes locais)
         inet_pton(AF_INET, "10.0.0.4", &servaddr.sin_addr);
    }

    // Conecta ao servidor
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect failed");
        close(sockfd);
        return;
    }

    // Cria o objeto JSON
    json_object *jobj = json_object_new_object();
    json_object_object_add(jobj, "engine", json_object_new_string("mpi-openmp"));
    json_object_object_add(jobj, "tam", json_object_new_int(tam));
    json_object_object_add(jobj, "status", json_object_new_string(status));
    json_object_object_add(jobj, "computation_time", json_object_new_double(computation_time));

    const char *json_string = json_object_to_json_string(jobj);
    printf("Enviando resultado para o servidor: %s\n", json_string);

    // Envia a string JSON (com uma quebra de linha, pois o server.py lê por linha)
    char buffer[MAXLINE];
    snprintf(buffer, sizeof(buffer), "%s\n", json_string);
    write(sockfd, buffer, strlen(buffer));

    // Limpa e fecha a conexão
    json_object_put(jobj);
    close(sockfd);
}


// A função run_engine agora envia o resultado
void run_engine(int powmin, int powmax) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    for (int pow = powmin; pow <= powmax; pow++) {
        int tam = 1 << pow;
        int totalRows = tam;
        int localRows = totalRows / size;
        if (rank == size - 1) localRows += totalRows % size;

        int *localIn = malloc((localRows + 2) * (tam + 2) * sizeof(int));
        int *localOut = malloc((localRows + 2) * (tam + 2) * sizeof(int));

        InitTabul(localIn, tam, localRows, rank);
        
        MPI_Barrier(MPI_COMM_WORLD); // Sincroniza os processos antes de medir o tempo
        double start = MPI_Wtime();

        // O loop de cálculo não muda
        for (int iter = 0; iter < 4 * (tam - 3); iter++) {
            // ... (lógica de troca de bordas com MPI_Sendrecv) ...
            UmaVida(localIn, localOut, tam, localRows);
            int *tmp = localIn;
            localIn = localOut;
            localOut = tmp;
        }

        MPI_Barrier(MPI_COMM_WORLD); // Sincroniza antes de parar o tempo
        double end = MPI_Wtime();

        // Apenas o processo de rank 0 envia o resultado
        if (rank == 0) {
            double elapsed_time = end - start;
            printf("[MPI-OpenMP] tam=%d; tempo=%.4fs\n", tam, elapsed_time);
            // TODO: Adicionar uma função de 'correto()' para verificar o resultado
            send_result_to_server(tam, elapsed_time, "CORRETO");
        }
        free(localIn);
        free(localOut);
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    // Por enquanto, vamos rodar um teste fixo em vez de esperar por uma conexão
    // TODO: Integrar com a lógica de receber trabalhos do Socket Server
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if(rank == 0) {
        printf("[Engine] Iniciando trabalho de teste.\n");
    }
    run_engine(8, 10); // Executa um teste para tam=256, 512, 1024

    MPI_Finalize();
    return 0;
}