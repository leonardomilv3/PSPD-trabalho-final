import time
import numpy as np
from pyspark.sql import SparkSession

def uma_vida_numpy(board, tam):
    """
    Calcula uma geração usando operações de array do NumPy, que são muito mais rápidas.
    """
    # Cria uma cópia do tabuleiro para o resultado
    new_board = board.copy()
    
    # Usando fatiamento de array para somar todos os 8 vizinhos de uma só vez
    # Isso evita os laços for em Python, que são lentos.
    vizviv = (board[0:-2, 0:-2] + board[0:-2, 1:-1] + board[0:-2, 2:] +
              board[1:-1, 0:-2] + board[1:-1, 2:] +
              board[2:, 0:-2]   + board[2:, 1:-1]   + board[2:, 2:])

    # Aplicando as regras do Jogo da Vida usando "boolean masking" do NumPy
    
    # 1. Células vivas com menos de 2 vizinhos morrem (solidão)
    # 2. Células vivas com mais de 3 vizinhos morrem (superpopulação)
    mask_morrem = ((board[1:-1, 1:-1] == 1) & ((vizviv < 2) | (vizviv > 3)))
    new_board[1:-1, 1:-1][mask_morrem] = 0

    # 3. Células mortas com exatamente 3 vizinhos se tornam vivas (reprodução)
    mask_vivem = ((board[1:-1, 1:-1] == 0) & (vizviv == 3))
    new_board[1:-1, 1:-1][mask_vivem] = 1

    return new_board

def init_tabul(tam):
    """Inicializa o tabuleiro como um array NumPy."""
    board = np.zeros((tam + 2, tam + 2), dtype=int)
    board[1, 2] = 1
    board[2, 3] = 1
    board[3, 1] = 1
    board[3, 2] = 1
    board[3, 3] = 1
    return board

def correto(board, tam):
    """Verifica se o resultado está correto usando NumPy."""
    # O código C usa 1-based, então board[tam] em C é board[tam] em Python
    # pois o tabuleiro já tem as bordas de 1 célula de cada lado.
    pos_correta = (board[tam-2, tam-1] and board[tam-1, tam] and
                   board[tam, tam-2] and board[tam, tam-1] and board[tam, tam])
    return (board.sum() == 5 and pos_correta)

if __name__ == "__main__":
    spark = SparkSession.builder.appName("GameOfLifeOptimized").getOrCreate()

    POWMIN = 8 
    POWMAX = 12 # Vamos até 4096x4096 para ver a performance

    for pow_val in range(POWMIN, POWMAX + 1):
        tam = 1 << pow_val
        print(f"Iniciando Jogo da Vida para tabuleiro de tamanho {tam}x{tam}")
        
        t_init_start = time.time()
        tabul_in = init_tabul(tam)
        t_init_end = time.time()

        t_comp_start = time.time()
        # O número correto de gerações para o veleiro chegar ao fim
        for _ in range(2 * (tam - 3)):
            tabul_out = uma_vida_numpy(tabul_in, tam)
            tabul_in = uma_vida_numpy(tabul_out, tam)
            
        t_comp_end = time.time()

        if correto(tabul_in, tam):
            print(f"**Ok, RESULTADO CORRETO** para tam={tam}")
        else:
            print(f"**Nok, RESULTADO ERRADO** para tam={tam}")
        
        print(f"tam={tam}; tempos: init={t_init_end - t_init_start:.7f}, comp={t_comp_end - t_comp_start:.7f}")

    spark.stop()