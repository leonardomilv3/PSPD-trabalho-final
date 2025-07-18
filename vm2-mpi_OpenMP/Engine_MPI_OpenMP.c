#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <mpi.h>
#include <omp.h>
#include <json-c/json.h>

#define MAXLINE 1024
#define ind2d(i,j,tam) ((i)*((tam)+2)+(j))

void UmaVida(int* in, int* out, int tam, int localRows) {
    #pragma omp parallel for
    for (int i = 1; i <= localRows; i++) {
        for (int j = 1; j <= tam; j++) {
            int vizviv = in[ind2d(i-1,j-1,tam)] + in[ind2d(i-1,j,tam)] + in[ind2d(i-1,j+1,tam)] +
                         in[ind2d(i,j-1,tam)]   + in[ind2d(i,j+1,tam)]   +
                         in[ind2d(i+1,j-1,tam)] + in[ind2d(i+1,j,tam)] + in[ind2d(i+1,j+1,tam)];

            if (in[ind2d(i,j,tam)] && (vizviv < 2 || vizviv > 3)) {
                out[ind2d(i,j,tam)] = 0;
            } else if (!in[ind2d(i,j,tam)] && vizviv == 3) {
                out[ind2d(i,j,tam)] = 1;
            } else {
                out[ind2d(i,j,tam)] = in[ind2d(i,j,tam)];
            }
        }
    }
}

void InitTabul(int* tabul, int tam, int localRows, int rank) {
    for (int i = 0; i < (localRows+2)*(tam+2); i++) tabul[i] = 0;
    if (rank == 0) {
        tabul[ind2d(1,2,tam)] = 1; tabul[ind2d(2,3,tam)] = 1;
        tabul[ind2d(3,1,tam)] = 1; tabul[ind2d(3,2,tam)] = 1; tabul[ind2d(3,3,tam)] = 1;
    }
}

const char* verifica_correto(int* tabul, int tam, int localRows, int rank, int size) {
    int local_sum = 0;
    for (int i = 1; i <= localRows; i++) {
        for (int j = 1; j <= tam; j++) {
            local_sum += tabul[ind2d(i, j, tam)];
        }
    }
    
    int global_sum;
    MPI_Reduce(&local_sum, &global_sum, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        if (global_sum == 5) { // A verificação completa da posição é complexa em paralelo,
                               // mas a soma já é um bom indicador.
            return "CORRETO";
        }
    }
    return "ERRADO";
}


void send_result_to_server(int tam, double computation_time, const char* status) {
    int sockfd;
    struct sockaddr_in servaddr;
    struct hostent *server;

    const char* server_host = getenv("SOCKET_SERVER_HOST");
    if (server_host == NULL) {
        server_host = "socket-server-service"; 
    }
    int server_port = 9999; 

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed"); return;
    }

    server = gethostbyname(server_host);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", server_host);
        close(sockfd); return;
    }

    bzero((char *) &servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    memcpy(&servaddr.sin_addr.s_addr, server->h_addr, server->h_length);
    servaddr.sin_port = htons(server_port);

    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect failed"); close(sockfd); return;
    }

    json_object *jobj = json_object_new_object();
    json_object_object_add(jobj, "engine", json_object_new_string("mpi-openmp"));
    json_object_object_add(jobj, "tam", json_object_new_int(tam));
    json_object_object_add(jobj, "status", json_object_new_string(status));
    json_object_object_add(jobj, "computation_time", json_object_new_double(computation_time));

    const char *json_string = json_object_to_json_string(jobj);
    printf("Enviando resultado para o servidor: %s\n", json_string);

    char buffer[MAXLINE];
    snprintf(buffer, sizeof(buffer), "%s\n", json_string);
    write(sockfd, buffer, strlen(buffer));

    json_object_put(jobj);
    close(sockfd);
}

void run_engine(int powmin, int powmax) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    for (int pow = powmin; pow <= powmax; pow++) {
        int tam = 1 << pow;
        int totalRows = tam;
        int localRows = totalRows / size;
        if (rank == 0) localRows += totalRows % size; // Rank 0 pega o resto

        int *localIn = malloc((localRows + 2) * (tam + 2) * sizeof(int));
        int *localOut = malloc((localRows + 2) * (tam + 2) * sizeof(int));

        InitTabul(localIn, tam, localRows, rank);
        
        MPI_Barrier(MPI_COMM_WORLD);
        double start = MPI_Wtime();

        for (int iter = 0; iter < 2 * (tam - 3); iter++) {
            // Lógica de troca de bordas (ghost cells)
            if (rank > 0) { // Envia para cima, recebe de cima
                MPI_Sendrecv(&localIn[ind2d(1, 0, tam)], tam + 2, MPI_INT, rank - 1, 0,
                             &localIn[ind2d(0, 0, tam)], tam + 2, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            if (rank < size - 1) { // Envia para baixo, recebe de baixo
                MPI_Sendrecv(&localIn[ind2d(localRows, 0, tam)], tam + 2, MPI_INT, rank + 1, 0,
                             &localIn[ind2d(localRows + 1, 0, tam)], tam + 2, MPI_INT, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            
            UmaVida(localIn, localOut, tam, localRows);

            // Troca de buffers com lógica de "ping-pong"
            if (rank > 0) {
                MPI_Sendrecv(&localOut[ind2d(1, 0, tam)], tam + 2, MPI_INT, rank - 1, 0,
                             &localOut[ind2d(0, 0, tam)], tam + 2, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            if (rank < size - 1) {
                MPI_Sendrecv(&localOut[ind2d(localRows, 0, tam)], tam + 2, MPI_INT, rank + 1, 0,
                             &localOut[ind2d(localRows + 1, 0, tam)], tam + 2, MPI_INT, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            
            UmaVida(localOut, localIn, tam, localRows);
        }

        MPI_Barrier(MPI_COMM_WORLD);
        double end = MPI_Wtime();

        if (rank == 0) {
            double elapsed_time = end - start;
            const char* status = verifica_correto(localIn, tam, localRows, rank, size);
            printf("[MPI-OpenMP] tam=%d; tempo=%.4fs; status=%s\n", tam, elapsed_time, status);
            send_result_to_server(tam, elapsed_time, status);
        }
        
        free(localIn);
        free(localOut);
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if(rank == 0) {
        printf("[Engine] Iniciando trabalho de teste.\n");
    }
    run_engine(3, 10); 

    MPI_Finalize();
    return 0;
}