// engine_socket_mpi_openmp.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <omp.h>
#include <unistd.h>
#include <netinet/in.h>

#define PORT 8080
#define MAXLINE 1024
#define ind2d(i,j) ((i)*(tam+2)+(j))

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

void InitTabul(int* tabul, int tam, int localRows, int rank) {
    for (int i = 0; i < (localRows+2)*(tam+2); i++) {
        tabul[i] = 0;
    }
    if (rank == 0) {
        tabul[ind2d(1,2)] = 1; tabul[ind2d(2,3)] = 1;
        tabul[ind2d(3,1)] = 1; tabul[ind2d(3,2)] = 1; tabul[ind2d(3,3)] = 1;
    }
}

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
        double start = MPI_Wtime();

        for (int iter = 0; iter < 2 * (tam - 3); iter++) {
            if (rank > 0) {
                MPI_Sendrecv(&localIn[ind2d(1, 0)], tam+2, MPI_INT, rank - 1, 0,
                             &localIn[ind2d(0, 0)], tam+2, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            if (rank < size - 1) {
                MPI_Sendrecv(&localIn[ind2d(localRows, 0)], tam+2, MPI_INT, rank + 1, 0,
                             &localIn[ind2d(localRows+1, 0)], tam+2, MPI_INT, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            UmaVida(localIn, localOut, tam, localRows);
            int *tmp = localIn;
            localIn = localOut;
            localOut = tmp;
        }

        double end = MPI_Wtime();
        if (rank == 0) {
            printf("[MPI-OpenMP] tam=%d; tempo=%.4fs\n", tam, end - start);
        }
        free(localIn);
        free(localOut);
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;
    char buffer[MAXLINE];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    listen(sockfd, 5);
    len = sizeof(cli);
    printf("[Engine] Aguardando conexao de cliente na porta %d...\n", PORT);
    connfd = accept(sockfd, (struct sockaddr*)&cli, (socklen_t*)&len);

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        read(connfd, buffer, sizeof(buffer));
        if (strncmp("exit", buffer, 4) == 0) break;

        int powmin, powmax;
        sscanf(buffer, "%d %d", &powmin, &powmax);
        printf("[Engine] Trabalho recebido: powmin=%d, powmax=%d\n", powmin, powmax);

        run_engine(powmin, powmax);
    }

    close(connfd);
    close(sockfd);
    MPI_Finalize();
    return 0;
}
