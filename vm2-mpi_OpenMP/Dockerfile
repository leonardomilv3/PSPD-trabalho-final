# Usa uma imagem base oficial e estável do Ubuntu
FROM ubuntu:22.04

# Desativa prompts interativos durante instalação de pacotes
ENV DEBIAN_FRONTEND=noninteractive

# Instala dependências: compilador C, OpenMPI e JSON-C
RUN apt-get update && \
    apt-get install -y \
    build-essential \
    openmpi-bin \
    libopenmpi-dev \
    libjson-c-dev \
    && rm -rf /var/lib/apt/lists/*

# Define diretório de trabalho dentro do container
WORKDIR /app

# Copia os arquivos do diretório local (onde está o Dockerfile) para o container
COPY . .

# Compila o código com MPI, OpenMP e json-c
RUN mpicc -fopenmp -o engine Engine_MPI_OpenMP.c -ljson-c

# Comando padrão: executa o engine com mpirun
CMD ["mpirun", "--allow-run-as-root", "-np", "1", "./engine"]
