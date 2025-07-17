import time
from pyspark.sql import SparkSession

# Função para inicializar o tabuleiro com o "veleiro"
def init_tabul(tam):
    board = [[0] * (tam + 2) for _ in range(tam + 2)]
    board[1][2] = 1
    board[2][3] = 1
    board[3][1] = 1
    board[3][2] = 1
    board[3][3] = 1
    return board

# Função que calcula uma geração do Jogo da Vida
# Esta é a função que será distribuída pelo Spark
def uma_vida(rdd, tam):
    def get_neighbors_sum(row_idx, col_idx, board_broadcast):
        board = board_broadcast.value
        s = (board[row_idx-1][col_idx-1] + board[row_idx-1][col_idx] + board[row_idx-1][col_idx+1] +
             board[row_idx][col_idx-1]   + board[row_idx][col_idx+1]   +
             board[row_idx+1][col_idx-1] + board[row_idx+1][col_idx] + board[row_idx+1][col_idx+1])
        return s

    def apply_rules(cell):
        row_idx, col_idx, value = cell
        # Não processa as bordas
        if row_idx == 0 or row_idx > tam or col_idx == 0 or col_idx > tam:
            return (row_idx, col_idx, value)

        vizviv = get_neighbors_sum(row_idx, col_idx, rdd.context.broadcast(current_board_b))
        
        new_value = value
        if value == 1 and (vizviv < 2 or vizviv > 3):
            new_value = 0
        elif value == 0 and vizviv == 3:
            new_value = 1
        
        return (row_idx, col_idx, new_value)

    # Coleta o tabuleiro atual para usar na transformação
    current_board = [[cell[2] for cell in sorted(row, key=lambda x: x[1])] for row in rdd.glom().collect()]
    current_board_b = rdd.context.broadcast(current_board)
    
    # Aplica as regras a cada célula
    new_rdd = rdd.map(apply_rules)
    return new_rdd

# Função para verificar se o resultado está correto
def correto(board, tam):
    cnt = sum(sum(row) for row in board)
    return (cnt == 5 and board[tam-2][tam-1] and board[tam-1][tam] and
            board[tam-2][tam] and board[tam][tam-2] and board[tam][tam-1])

if __name__ == "__main__":
    # TODO: Integrar com o Kafka para receber POWMIN e POWMAX
    # Por enquanto, vamos usar valores fixos para teste
    POWMIN = 8 
    POWMAX = 8

    spark = SparkSession.builder.appName("GameOfLife").getOrCreate()
    sc = spark.sparkContext

    for pow_val in range(POWMIN, POWMAX + 1):
        tam = 1 << pow_val
        
        print(f"Iniciando Jogo da Vida para tabuleiro de tamanho {tam}x{tam}")
        
        # Inicializa o tabuleiro
        t_init_start = time.time()
        initial_board = init_tabul(tam)
        
        # Paraleliza o tabuleiro em um RDD
        # Cada elemento é uma tupla (linha, coluna, valor)
        flat_board = [(r, c, initial_board[r][c]) for r in range(tam + 2) for c in range(tam + 2)]
        rdd = sc.parallelize(flat_board)
        t_init_end = time.time()

        # Executa as gerações
        t_comp_start = time.time()
        for i in range(2 * (tam - 3)):
            rdd = uma_vida(rdd, tam)
            # A cada 20 iterações, forçamos a materialização para evitar um plano de execução muito longo
            if i % 20 == 0:
                rdd.cache()
                rdd.count() 

        final_board_flat = rdd.collect()
        t_comp_end = time.time()

        # Reconstrói o tabuleiro para verificação
        final_board = [[0] * (tam + 2) for _ in range(tam + 2)]
        for r, c, val in final_board_flat:
            final_board[r][c] = val
            
        if correto(final_board, tam):
            print("**Ok, RESULTADO CORRETO**")
        else:
            print("**Nok, RESULTADO ERRADO**")
        
        print(f"tam={tam}; tempos: init={t_init_end - t_init_start:.7f}, comp={t_comp_end - t_comp_start:.7f}")

    spark.stop()