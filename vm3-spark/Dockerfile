# Usar uma imagem base do Spark oficial que já vem com tudo configurado
FROM bitnami/spark:3

# Instalar a biblioteca Python para conectar ao Kafka
USER root
RUN pip install kafka-python numpy


# Copiar nosso script do Jogo da Vida para dentro da imagem
COPY engine_spark.py /app/

# Definir o diretório de trabalho
WORKDIR /app

# Comando para executar a aplicação quando o container iniciar
CMD ["spark-submit", "engine_spark.py"]





