#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <omp.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <json-c/json.h>

#define MAX_LINE 4096

void init_tabul(int *board, int tam) {
    memset(board, 0, (tam + 2) * (tam + 2) * sizeof(int));
    board[(1)*(tam+2)+2] = 1;
    board[(2)*(tam+2)+3] = 1;
    board[(3)*(tam+2)+1] = 1;
    board[(3)*(tam+2)+2] = 1;
    board[(3)*(tam+2)+3] = 1;
}

void uma_vida_omp(int *board, int *new_board, int tam) {
    #pragma omp parallel for
    for(int i = 1; i <= tam; i++) {
        for(int j = 1; j <= tam; j++) {
            int vizviv = board[(i-1)*(tam+2)+(j-1)] + board[(i-1)*(tam+2)+j] + board[(i-1)*(tam+2)+(j+1)]
                       + board[i*(tam+2)+(j-1)] + board[i*(tam+2)+(j+1)]
                       + board[(i+1)*(tam+2)+(j-1)] + board[(i+1)*(tam+2)+j] + board[(i+1)*(tam+2)+(j+1)];

            if(board[i*(tam+2)+j] == 1) {
                new_board[i*(tam+2)+j] = (vizviv < 2 || vizviv > 3) ? 0 : 1;
            } else {
                new_board[i*(tam+2)+j] = (vizviv == 3) ? 1 : 0;
            }
        }
    }
}

int correto(int *board, int tam) {
    int pos_correta = board[(tam-2)*(tam+2)+(tam-1)] &&
                      board[(tam-1)*(tam+2)+tam] &&
                      board[(tam)*(tam+2)+(tam-2)] &&
                      board[(tam)*(tam+2)+(tam-1)] &&
                      board[(tam)*(tam+2)+tam];
    int soma = 0;
    for(int i = 0; i < (tam+2)*(tam+2); i++) soma += board[i];
    return (soma == 5 && pos_correta);
}

void executar_simulacao(int pow, char *saida_json, size_t saida_size) {
    int tam = 1 << pow;
    printf("[OMP+MPI Engine] Iniciando simulação pow = %d (tam = %d)\n", pow, tam);

    int *tabul = malloc((tam+2)*(tam+2)*sizeof(int));
    int *tabul_out = malloc((tam+2)*(tam+2)*sizeof(int));

    if(!tabul || !tabul_out) {
        snprintf(saida_json, saida_size, "{\"pow\": %d, \"status\": \"ERRO\", \"erro\": \"malloc failed\"}", pow);
        free(tabul); free(tabul_out);
        return;
    }

    init_tabul(tabul, tam);
    double t0 = MPI_Wtime();

    for(int k = 0; k < 2*(tam-3); k++) {
        uma_vida_omp(tabul, tabul_out, tam);
        uma_vida_omp(tabul_out, tabul, tam);
    }

    double t1 = MPI_Wtime();
    int status_ok = correto(tabul, tam);

    printf("[OMP+MPI Engine] Simulação pow = %d finalizada. Status: %s\n", pow, status_ok ? "CORRETO" : "ERRADO");

    snprintf(saida_json, saida_size,
        "{\"tam\": %d, \"pow\": %d, \"status\": \"%s\", \"tempo\": %.4f}",
        tam, pow, status_ok ? "CORRETO" : "ERRADO", t1 - t0);

    free(tabul);
    free(tabul_out);
}

int read_line(int sockfd, char *buffer, int maxlen) {
    int n, total = 0;
    char c;
    while(total < maxlen -1) {
        n = recv(sockfd, &c, 1, 0);
        if(n <= 0) break;
        if(c == '\n') break;
        buffer[total++] = c;
    }
    buffer[total] = '\0';
    return total;
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    omp_set_num_threads(4); // ajuste conforme necessário

    int server_fd = -1, new_socket = -1;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int port = 5000;

    if(rank == 0) {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        bind(server_fd, (struct sockaddr *)&address, sizeof(address));
        listen(server_fd, 3);

        printf("[OMP+MPI Engine] Escutando socket em 0.0.0.0:%d...\n", port);
    }

    while(1) {
        int powMin = 0, powMax = 0;
        if(rank == 0) {
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            if(new_socket < 0) continue;

            char buffer[MAX_LINE];
            read_line(new_socket, buffer, MAX_LINE);
            printf("[OMP+MPI Engine] Conexão recebida\n");
            printf("[OMP+MPI Engine] Recebido: %s\n", buffer);

            struct json_object *parsed_json, *powMin_obj, *powMax_obj;
            parsed_json = json_tokener_parse(buffer);

            if(!parsed_json || 
               !json_object_object_get_ex(parsed_json, "powMin", &powMin_obj) ||
               !json_object_object_get_ex(parsed_json, "powMax", &powMax_obj)) {
                char *erro = "{\"engine\": \"omp_mpi\", \"erro\": \"Parâmetros inválidos\"}\n";
                send(new_socket, erro, strlen(erro), 0);
                close(new_socket);
                if(parsed_json) json_object_put(parsed_json);
                continue;
            }

            powMin = json_object_get_int(powMin_obj);
            powMax = json_object_get_int(powMax_obj);
            json_object_put(parsed_json);
        }

        MPI_Bcast(&powMin, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&powMax, 1, MPI_INT, 0, MPI_COMM_WORLD);

        int total_pows = powMax - powMin + 1;
        int padded = ((total_pows + size - 1) / size) * size;
        int chunk = padded / size;

        int *pows = NULL;
        if(rank == 0) {
            pows = calloc(padded, sizeof(int));
            for(int i = 0; i < total_pows; i++)
                pows[i] = powMin + i;
        }

        int *recv_pows = malloc(chunk * sizeof(int));
        MPI_Scatter(pows, chunk, MPI_INT, recv_pows, chunk, MPI_INT, 0, MPI_COMM_WORLD);

        char *send_buffer = malloc(chunk * 256);
        for(int i = 0; i < chunk; i++) {
            char *ptr = send_buffer + i * 256;
            if(recv_pows[i] != 0)
                executar_simulacao(recv_pows[i], ptr, 256);
            else
                snprintf(ptr, 256, "{}");
        }

        char *recv_buffer = NULL;
        if(rank == 0)
            recv_buffer = malloc(padded * 256);

        MPI_Gather(send_buffer, chunk * 256, MPI_CHAR,
                   recv_buffer, chunk * 256, MPI_CHAR,
                   0, MPI_COMM_WORLD);

        if(rank == 0) {
            char resposta_final[8192] = "{\"engine\": \"omp_mpi\", \"resultados\": [";
            for(int i = 0; i < total_pows; i++) {
                strncat(resposta_final, recv_buffer + i * 256, 256);
                if(i < total_pows - 1) strcat(resposta_final, ",");
            }
            strcat(resposta_final, "]}");

            send(new_socket, resposta_final, strlen(resposta_final), 0);
            printf("[OMP+MPI Engine] Resultado enviado\n");
            close(new_socket);
            free(pows);
            free(recv_buffer);
        }

        free(recv_pows);
        free(send_buffer);
    }

    MPI_Finalize();
    return 0;
}
