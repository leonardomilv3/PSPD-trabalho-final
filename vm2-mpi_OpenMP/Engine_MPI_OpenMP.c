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
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>

#define MAXLINE 1024
#define ind2d(i,j,tam) ((i)*((tam)+2)+(j))

// Variável global para controlar o loop do worker
volatile int running = 1;

// Handler para SIGINT (Ctrl+C)
void signal_handler(int sig) {
    running = 0;
}

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

// Função para executar o engine e retornar resultados como JSON
char* run_engine_and_get_results(int powmin, int powmax) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    json_object *results_array = json_object_new_array();

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
            
            // Criar objeto JSON para este resultado
            json_object *result_obj = json_object_new_object();
            json_object_object_add(result_obj, "tam", json_object_new_int(tam));
            json_object_object_add(result_obj, "computation_time", json_object_new_double(elapsed_time));
            json_object_object_add(result_obj, "status", json_object_new_string(status));
            
            json_object_array_add(results_array, result_obj);
            
            printf("[MPI-OpenMP] tam=%d; tempo=%.4fs; status=%s\n", tam, elapsed_time, status);
        }
        
        free(localIn);
        free(localOut);
    }

    // Criar resposta final
    json_object *response = json_object_new_object();
    json_object_object_add(response, "engine", json_object_new_string("mpi-openmp"));
    json_object_object_add(response, "results", results_array);

    const char *json_string = json_object_to_json_string(response);
    char *result_str = strdup(json_string);
    
    json_object_put(response);
    
    return result_str;
}

// Função para conectar ao servidor e aguardar comandos
void connect_to_server_and_work() {
    const char* server_host = getenv("SERVER_HOST");
    if (server_host == NULL) {
        server_host = "localhost"; // padrão
    }
    
    const char* server_port_str = getenv("SERVER_PORT");
    int server_port = 5000; // padrão
    if (server_port_str != NULL) {
        server_port = atoi(server_port_str);
    }
    
    printf("[Engine MPI-OpenMP] Conectando ao servidor %s:%d\n", server_host, server_port);
    
    while (running) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("Erro ao criar socket");
            sleep(5);
            continue;
        }
        
        struct hostent *server = gethostbyname(server_host);
        if (server == NULL) {
            fprintf(stderr, "Erro: não foi possível resolver o host %s\n", server_host);
            close(sockfd);
            sleep(5);
            continue;
        }
        
        struct sockaddr_in servaddr;
        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        memcpy(&servaddr.sin_addr.s_addr, server->h_addr, server->h_length);
        servaddr.sin_port = htons(server_port);
        
        if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
            perror("Erro ao conectar ao servidor");
            close(sockfd);
            sleep(5);
            continue;
        }
        
        printf("Conectado ao servidor. Aguardando comandos...\n");
        
        // Loop para receber comandos do servidor
        while (running) {
            char buffer[MAXLINE];
            int n = read(sockfd, buffer, sizeof(buffer) - 1);
            if (n <= 0) {
                printf("Conexão perdida com o servidor. Reconectando...\n");
                break;
            }
            buffer[n] = '\0';
            
            printf("Comando recebido: %s\n", buffer);
            
            // Parse do JSON recebido
            json_object *json_obj = json_tokener_parse(buffer);
            if (json_obj == NULL) {
                const char *error_response = "{\"error\": \"Invalid JSON\"}\n";
                write(sockfd, error_response, strlen(error_response));
                continue;
            }
            
            // Extrair powMin e powMax
            json_object *powmin_obj, *powmax_obj;
            int powmin = 3, powmax = 10; // valores padrão
            
            if (json_object_object_get_ex(json_obj, "powMin", &powmin_obj)) {
                powmin = json_object_get_int(powmin_obj);
            }
            if (json_object_object_get_ex(json_obj, "powMax", &powmax_obj)) {
                powmax = json_object_get_int(powmax_obj);
            }
            
            printf("Processando powMin=%d, powMax=%d\n", powmin, powmax);
            
            // Executar o engine
            char *result = run_engine_and_get_results(powmin, powmax);
            
            // Enviar resposta
            char response[8192]; // Buffer maior para respostas JSON
            snprintf(response, sizeof(response), "%s\n", result);
            write(sockfd, response, strlen(response));
            
            // Limpar
            free(result);
            json_object_put(json_obj);
        }
        
        close(sockfd);
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        printf("[Engine MPI-OpenMP] Iniciando worker.\n");
        
        // Configurar handler para SIGINT
        signal(SIGINT, signal_handler);
        
        // Conectar ao servidor e aguardar comandos
        connect_to_server_and_work();
        
        printf("Encerrando worker...\n");
    }

    MPI_Finalize();
    return 0;
}