import asyncio
import json
import os
from elasticsearch import AsyncElasticsearch

# --- Configuração ---
# Lê as configurações das variáveis de ambiente para maior flexibilidade
ELASTIC_HOST = os.environ.get('ELASTIC_HOST', 'elasticsearch-service')
SERVER_PORT = int(os.environ.get('SERVER_PORT', 9999))
ES_URL = f"http://{ELASTIC_HOST}:9200"

# --- Cliente Elasticsearch ---
try:
    es_client = AsyncElasticsearch(ES_URL)
    print(f"Tentando conectar ao Elasticsearch em {ES_URL}...")
except Exception as e:
    print(f"Erro ao inicializar cliente Elasticsearch: {e}")
    es_client = None

# --- Lógica do Servidor ---
async def handle_client(reader, writer):
    """Função chamada para cada nova conexão de cliente."""
    addr = writer.get_extra_info('peername')
    print(f"Nova conexão de {addr}")
    
    try:
        while True:
            # Lê uma linha de dados da conexão
            data = await reader.readline()
            if not data:
                break # Conexão fechada pelo cliente

            message = data.decode().strip()
            print(f"Recebido de {addr}: {message}")
            
            # Tenta processar a mensagem como JSON e enviar ao Elasticsearch
            try:
                log_data = json.loads(message)
                # Se for um resultado de um engine, envia para o Elasticsearch
                if 'engine' in log_data and es_client:
                    await es_client.index(index="engine_results", document=log_data)
                    print(f"Resultado do engine '{log_data.get('engine')}' enviado ao Elasticsearch.")
            except Exception as e:
                print(f"Aviso: a mensagem não é um JSON válido ou falhou ao enviar para o ES. Erro: {e}")

    except asyncio.CancelledError:
        pass # Tarefa cancelada
    finally:
        print(f"Fechando conexão com {addr}")
        writer.close()
        await writer.wait_closed()

async def main():
    """Função principal que inicia o servidor."""
    server = await asyncio.start_server(handle_client, '0.0.0.0', SERVER_PORT)
    addrs = ', '.join(str(sock.getsockname()) for sock in server.sockets)
    print(f'Socket Server iniciado e escutando em {addrs}...')
    
    async with server:
        await server.serve_forever()

if __name__ == '__main__':
    asyncio.run(main()) 