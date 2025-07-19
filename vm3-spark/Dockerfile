# Usa imagem Spark completa com suporte a Python
FROM bitnami/spark:3

# Define usuário root para poder instalar pacotes
USER root

# Instala as bibliotecas necessárias
RUN pip install pyspark numpy

# Cria diretório de trabalho e copia o script
WORKDIR /app
COPY engine_spark.py .

# Expõe a porta 5000 para o socket
EXPOSE 5000

# Comando para iniciar o worker socket com Spark
CMD ["python", "engine_spark.py"]
