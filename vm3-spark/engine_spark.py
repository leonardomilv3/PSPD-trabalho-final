import json
import time
import numpy as np
import socket
from pyspark.sql import SparkSession

def init_tabul(tam):
    board = np.zeros((tam + 2, tam + 2), dtype=int)
    board[1, 2] = 1
    board[2, 3] = 1
    board[3, 1] = 1
    board[3, 2] = 1
    board[3, 3] = 1
    return board

def uma_vida_numpy(board, tam):
    new_board = board.copy()
    vizviv = (board[0:-2, 0:-2] + board[0:-2, 1:-1] + board[0:-2, 2:] +
              board[1:-1, 0:-2] + board[1:-1, 2:] +
              board[2:, 0:-2] + board[2:, 1:-1] + board[2:, 2:])
    mask_morrem = ((board[1:-1, 1:-1] == 1) & ((vizviv < 2) | (vizviv > 3)))
    new_board[1:-1, 1:-1][mask_morrem] = 0
    mask_vivem = ((board[1:-1, 1:-1] == 0) & (vizviv == 3))
    new_board[1:-1, 1:-1][mask_vivem] = 1
    return new_board

def correto(board, tam):
    pos_correta = (board[tam-2, tam-1] and board[tam-1, tam] and
                   board[tam, tam-2] and board[tam, tam-1] and board[tam, tam])
    return (board.sum() == 5 and pos_correta)

def executar_simulacao(pow):
    tam = 1 << pow
    try:
        t0 = time.time()
        tabul = init_tabul(tam)
        for _ in range(2 * (tam - 3)):
            tabul_out = uma_vida_numpy(tabul, tam)
            tabul = uma_vida_numpy(tabul_out, tam)
        t1 = time.time()

        status = "CORRETO" if correto(tabul, tam) else "ERRADO"
        return {
            "tam": tam,
            "pow": pow,
            "status": status,
            "tempo": round(t1 - t0, 4)
        }
    except Exception as e:
        return {
            "tam": tam,
            "pow": pow,
            "status": "ERRO",
            "erro": str(e)
        }

def processar_com_spark(powmin, powmax):
    spark = SparkSession.builder.appName("JogoVidaSocket").getOrCreate()
    sc = spark.sparkContext

    pows = list(range(powmin, powmax + 1))
    rdd = sc.parallelize(pows)
    resultados = rdd.map(executar_simulacao).collect()
    spark.stop()
    return resultados

def start_socket_worker(host='0.0.0.0', port=5000):
    print(f"[Spark Engine] Escutando socket em {host}:{port}...")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((host, port))
        s.listen(5)
        while True:
            conn, addr = s.accept()
            with conn:
                print(f"[Spark Engine] Conexão de {addr}")
                try:
                    data = conn.recv(4096).decode().strip()
                    print(f"[Spark Engine] Recebido: {data}")
                    payload = json.loads(data)
                    powmin = int(payload.get("powMin"))
                    powmax = int(payload.get("powMax"))

                    resultados = processar_com_spark(powmin, powmax)

                    # Envia uma única linha JSON para o socket principal
                    resposta = json.dumps({
                        "engine": "spark",
                        "resultados": resultados
                    })

                except Exception as e:
                    resposta = json.dumps({
                        "engine": "spark",
                        "erro": str(e)
                    })

                conn.sendall((resposta + '\n').encode("utf-8"))
                print(f"[Spark Engine] Resultado enviado")

if __name__ == "__main__":
    start_socket_worker()
