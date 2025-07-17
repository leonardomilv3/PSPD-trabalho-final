import time
import numpy as np
import json
import os
from pyspark.sql import SparkSession
from kafka import KafkaConsumer, KafkaProducer

# --- Funções do Jogo da Vida (Otimizadas com NumPy) ---
def uma_vida_numpy(board, tam):
    """
    Calcula uma geração usando operações de array do NumPy.
    """
    new_board = board.copy()
    vizviv = (board[0:-2, 0:-2] + board[0:-2, 1:-1] + board[0:-2, 2:] +
              board[1:-1, 0:-2] + board[1:-1, 2:] +
              board[2:, 0:-2]   + board[2:, 1:-1]   + board[2:, 2:])
    mask_morrem = ((board[1:-1, 1:-1] == 1) & ((vizviv < 2) | (vizviv > 3)))
    new_board[1:-1, 1:-1][mask_morrem] = 0
    mask_vivem = ((board[1:-1, 1:-1] == 0) & (vizviv == 3))
    new_board[1:-1, 1:-1][mask_vivem] = 1
    return new_board

def init_tabul(tam):
    """Inicializa o tabuleiro como um array NumPy."""
    board = np.zeros((tam + 2, tam + 2), dtype=int)
    board[1, 2] = 1; board[2, 3] = 1; board[3, 1] = 1; board[3, 2] = 1; board[3, 3] = 1
    return board

def correto(board, tam):
    """Verifica se o resultado está correto usando NumPy."""
    pos_correta = (board[tam-2, tam-1] and board[tam-1, tam] and
                   board[tam, tam-2] and board[tam, tam-1] and board[tam, tam])
    return (board.sum() == 5 and pos_correta)

# --- Função Principal com Lógica Kafka ---
def main():
    # --- CONFIGURAÇÃO LIDA DE VARIÁVEIS DE AMBIENTE ---
    KAFKA_BROKER_URL = os.environ.get('KAFKA_BROKER_URL', '20.51.109.115:9092')
    TOPIC_JOBS = os.environ.get('KAFKA_JOBS_TOPIC', 'spark-jobs')
    TOPIC_RESULTS = os.environ.get('KAFKA_RESULTS_TOPIC', 'spark-results')

    print("Iniciando Engine Spark...")
    print(f"Conectando ao Kafka Broker em {KAFKA_BROKER_URL}")
    print(f"Tópico de trabalhos (consumidor): {TOPIC_JOBS}")
    print(f"Tópico de resultados (produtor): {TOPIC_RESULTS}")

    try:
        consumer = KafkaConsumer(
            TOPIC_JOBS,
            bootstrap_servers=KAFKA_BROKER_URL,
            value_deserializer=lambda v: json.loads(v.decode('utf-8')),
            # Tenta reconectar em caso de falha na conexão inicial
            reconnect_backoff_ms=5000 
        )
        producer = KafkaProducer(
            bootstrap_servers=KAFKA_BROKER_URL,
            value_serializer=lambda v: json.dumps(v).encode('utf-8')
        )
        print("Conectado ao Kafka. Aguardando por mensagens de trabalho...")
    except Exception as e:
        print(f"FATAL: Erro ao conectar ao Kafka. O serviço será encerrado. Erro: {e}")
        return

    spark = SparkSession.builder.appName("GameOfLifeKafkaEngine").getOrCreate()

    for message in consumer:
        job_data = message.value
        print(f"\nRecebido novo trabalho: {job_data}")

        try:
            POWMIN = int(job_data['powmin'])
            POWMAX = int(job_data['powmax'])

            for pow_val in range(POWMIN, POWMAX + 1):
                tam = 1 << pow_val
                print(f"Processando para tabuleiro de tamanho {tam}x{tam}...")
                
                # ... (Lógica de cálculo do Jogo da Vida) ...
                t_init_start = time.time()
                tabul_in = init_tabul(tam)
                t_init_end = time.time()
                t_comp_start = time.time()
                for _ in range(2 * (tam - 3)):
                    tabul_out = uma_vida_numpy(tabul_in, tam)
                    tabul_in = uma_vida_numpy(tabul_out, tam)
                t_comp_end = time.time()

                resultado_final = "CORRETO" if correto(tabul_in, tam) else "ERRADO"
                tempo_comp = t_comp_end - t_comp_start
                
                print(f"Resultado: {resultado_final}, Tempo: {tempo_comp:.4f}s. Enviando para o Kafka.")
                
                result_payload = {
                    'engine': 'spark-numpy',
                    'tam': tam,
                    'status': resultado_final,
                    'computation_time': tempo_comp,
                    'timestamp': time.time()
                }
                producer.send(TOPIC_RESULTS, result_payload)
                producer.flush()

        except Exception as e:
            print(f"Erro ao processar o trabalho: {e}")
            error_payload = {'engine': 'spark-numpy', 'status': 'ERRO_PROCESSAMENTO', 'details': str(e)}
            producer.send(TOPIC_RESULTS, error_payload)
            producer.flush()

    spark.stop()

if __name__ == "__main__":
    main()