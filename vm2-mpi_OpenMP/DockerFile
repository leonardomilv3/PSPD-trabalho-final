# Usa uma imagem base oficial e estável do Ubuntu
FROM ubuntu:22.04

# Instala as dependências: compilador C, OpenMPI e a biblioteca JSON-C
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y build-essential openmpi-bin libopenmpi-dev libjson-c-dev && \
    rm -rf /var/lib/apt/lists/*

# Define o diretório de trabalho dentro do container
WORKDIR /app

# Copia o código da engine para o container
COPY . .

# Compila o código DENTRO do container, linkando com a biblioteca json-c
RUN mpicc -fopenmp -o engine Engine_MPI_OpenMP.c -ljson-c

# O EXPOSE foi removido, pois este container não escuta por conexões.

# Comando padrão: executa UMA instância da engine
CMD ["./engine"]