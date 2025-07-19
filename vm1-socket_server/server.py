import asyncio
import json
import os
from elasticsearch import AsyncElasticsearch
    
# --- Configuração ---
SPARK_ENGINE_HOST = os.environ.get("SPARK_ENGINE_HOST", "52.233.90.114")
SPARK_ENGINE_PORT = int(os.environ.get("SPARK_ENGINE_PORT", 5000))

OMP_ENGINE_HOST = os.environ.get("OMP_ENGINE_HOST", "20.57.128.36");
OMP_ENGINE_PORT = int(os.environ.get("OMP_ENGINE_PORT", 5000));


SERVER_PORT = int(os.environ.get("SERVER_PORT", 5000))
ELASTIC_HOST = os.environ.get("ELASTIC_HOST", "elasticsearch-service")
ES_URL = f"http://{ELASTIC_HOST}:9200"

# --- Elasticsearch ---
es_client = es_client = AsyncElasticsearch(
    ES_URL,
    headers={"Accept": "application/vnd.elasticsearch+json; compatible-with=8",
             "Content-Type": "application/vnd.elasticsearch+json; compatible-with=8"}
)


# --- Comunicação com as engines ---
async def communicate_with_engine(host, port, payload, engine_name):
    try:
        reader, writer = await asyncio.open_connection(host, port)
        writer.write((payload + '\n').encode())
        await writer.drain()

        response = await reader.readline()
        writer.close()
        await writer.wait_closed()

        response_text = response.decode().strip()
        print(f"🔁 Resposta da engine {engine_name}: {response_text}")

        # Indexa no Elasticsearch com debug
        try:
            res = await es_client.index(index="engine_results", document={
                "engine": engine_name,
                "input": json.loads(payload),
                "output": response_text
            })
            print(f"📥 Indexado no Elasticsearch ({engine_name}): {res}")
        except Exception as es_error:
            print(f"❌ Erro ao indexar no Elasticsearch: {es_error}")

        return response_text

    except Exception as e:
        error_msg = f"Erro com engine {engine_name}: {str(e)}"
        print(error_msg)

        try:
            await es_client.index(index="engine_results", document={
                "engine": engine_name,
                "input": json.loads(payload),
                "output": error_msg
            })
        except Exception as es_error:
            print(f"❌ Erro ao indexar falha no Elasticsearch: {es_error}")

        return error_msg


# --- Lógica principal do servidor ---
async def handle_client(reader, writer):
    addr = writer.get_extra_info('peername')
    print(f"Nova conexão de {addr}")

    try:
        data = await reader.readline()
        if not data:
            return

        message = data.decode().strip()
        print(f"Recebido de {addr}: {message}")

        try:
            payload = json.loads(message)
            powMin = payload.get("powMin")
            powMax = payload.get("powMax")

            if powMin is None or powMax is None:
                raise ValueError("Faltam parâmetros powMin ou powMax.")

            payload_json = json.dumps({"powMin": powMin, "powMax": powMax})

            # Envia para ambas as engines em paralelo
            result_spark = await asyncio.gather(
                communicate_with_engine(SPARK_ENGINE_HOST, SPARK_ENGINE_PORT, payload_json, "spark"),
                communicate_with_engine(OMP_ENGINE_HOST, OMP_ENGINE_PORT, payload_json, "omp_mpi")
            )

            final_response = json.dumps({
                "spark_result": result_spark[0],
                "omp_mpi_result": result_spark[1]
            })

        except Exception as e:
            final_response = json.dumps({"error": str(e)})

        writer.write((final_response + '\n').encode())
        await writer.drain()

    finally:
        print(f"Fechando conexão com {addr}")
        writer.close()
        await writer.wait_closed()

# --- Inicialização ---
async def main():
    print(f"Iniciando servidor socket na porta {SERVER_PORT}")
    server = await asyncio.start_server(handle_client, '0.0.0.0', SERVER_PORT)
    async with server:
        try:
            await server.serve_forever()
        finally:
            await es_client.close()


if __name__ == '__main__':
    asyncio.run(main())
