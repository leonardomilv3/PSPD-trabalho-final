FROM python:3.9-slim

WORKDIR /app

# Instala a biblioteca do Elasticsearch que o server.py usa
RUN pip install elasticsearch aiohttp

# Copia o código do servidor para dentro do container
COPY server.py .

# Informa que o container irá escutar na porta 5000
EXPOSE 5000

# Comando para iniciar o servidor quando o container rodar
CMD ["python", "server.py"]
