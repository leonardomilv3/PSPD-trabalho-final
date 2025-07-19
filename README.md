# PSPD-trabalho-final

Repositório para o Trabalho Final da disciplina **Programação para Sistemas Paralelos e Distribuídos (PSPD)** – Faculdade do Gama (FGA/UnB).

## Alunos

Aluno | Matrícula
--|--
Artur Vinicius Dias Nunes | 190142421
Henrique Hida | 180113569
João Manoel Barreto Neto | 211039519 
Leonardo Milomes Vitoriano | 201000379
Miguel Matos Costa de Frias Barbosa | 211039635

## Descrição

O projeto consiste em uma aplicação distribuída desenvolvida para orquestração via **Kubernetes** em um ambiente *self-hosted*, utilizando **máquinas virtuais (VMs)** provisionadas na plataforma Microsoft Azure.

O objetivo principal é implementar um sistema de processamento paralelo do **Jogo da Vida (Game of Life)**, suportando múltiplos motores de execução:

- **Apache Spark** para processamento distribuído baseado em dados.
- **OpenMP/MPI** para computação de alto desempenho (HPC) com foco em CPU e memória compartilhada/distribuída.

Todo o sistema é monitorado em tempo real com métricas de desempenho armazenadas no **Elasticsearch** e visualizadas via **Kibana**.

## Arquitetura

A arquitetura é composta por três máquinas virtuais principais:

### VM1 – Intermediação e Visualização

- **Função principal**: Ponto central de comunicação e armazenamento.
- **Serviços hospedados**:
  - Servidor de socket em Python que recebe resultados das VMs de processamento.
  - Stack de análise ELK (Elasticsearch + Kibana) para indexação e visualização de dados.
- **Comunicação**: TCP sockets com dados em JSON.

### VM2 – Engine OpenMP/MPI

- **Função principal**: Processamento de alto desempenho utilizando paralelismo híbrido.
- **Tecnologias utilizadas**:
  - Implementação em C com paralelismo híbrido: MPI (memória distribuída) + OpenMP (memória compartilhada).
  - Comunicação via socket TCP com servidor na VM1.
- **Containerização**: Aplicação empacotada via Docker, com bibliotecas específicas (OpenMPI, JSON-C).

### VM3 – Engine Apache Spark

- **Função principal**: Processamento massivamente paralelo de grandes volumes de dados com Spark.
- **Tecnologias utilizadas**:
  - Simulação em Python com NumPy para otimização vetorial.
  - Comunicação com Kafka para ingestão e entrega de jobs.
- **Containerização**: Dockerfile com pyspark, kafka-python e demais dependências.

<p align="center">
  <img src="./assets/Arquitetura3VMs.png" alt="Figura 1 - Arquitetura geral" width="100%">
</p>

<p align="center"><strong>Figura 1</strong> - Arquitetura geral</p>

## Metodologia de Desenvolvimento

- **Divisão inicial da equipe**:
  - Socket Server / Kafka: João Barreto
  - Kubernetes: Leonardo Milomes
  - MPI/OMP Engine: Artur Vinicius
  - Spark Engine: Miguel Matos
  - Elasticsearch / Testes: Henrique Hida

- **Gestão e comunicação**: Discord e WhatsApp.
- **Desenvolvimento colaborativo**: Pair Programming, com revisão cruzada e testes integrados.
- **Ambiente de cluster**: VMs interligadas com kubeadm e balanceamento de carga automático via Kubernetes.

## Elasticidade e Resiliência

O projeto foi pensado para escalar horizontalmente. As principais estratégias incluem:

### Kubernetes

- Cluster self-hosted com múltiplos nós.
- Deployments com múltiplas réplicas configuráveis.
- Uso de probes (liveness/readiness) para auto-recuperação.
- Afinidades e namespaces para isolamento de cargas.

### Spark

- Uso de `spark.dynamicAllocation` para escalar executores conforme a carga.
- Jobs sem estado, com possibilidade de restart automático após falha.

### MPI/OMP

- Containers com paralelismo fixo via Jobs Kubernetes.
- IPs descobertos dinamicamente.
- Fallback para reexecução completa em caso de falha de um pod.

## Análise Comparativa

### Coleta e Indexação de Dados

- Utilização do Elasticsearch como repositório central.
- Inserções feitas via cliente Python logo após o processamento.
- Kibana configurado para geração de dashboards e gráficos em tempo real.

### Métricas Avaliadas

- Tempo de execução por tamanho da matriz.
- Escalabilidade com variação do número de threads/instâncias.
- Comparação direta Spark vs MPI/OMP.

### Tolerância a Falhas

- Testes com interrupção forçada de pods para avaliar resiliência.
- Spark demonstrou maior adaptabilidade com substituição dinâmica de executores.
- MPI reinicia job completo em caso de falha parcial.

# Guia de Instalação das VMs - PSPD

Todos os comandos devem ser executados com privilégios de superusuário (`sudo`), tanto no **nó mestre (VM1)** quanto nos **nós workers (VM2, VM3)**, exceto onde indicado.

## Etapas Comuns a Todas as VMs

```bash
# 1. Atualizar pacotes
sudo apt update && sudo apt upgrade -y

# 2. Desabilitar swap (obrigatório para o Kubernetes)
sudo swapoff -a
sudo sed -i '/ swap / s/^/#/' /etc/fstab

# 3. Instalar dependências
sudo apt install -y apt-transport-https ca-certificates curl gnupg lsb-release

# 4. Instalar Docker
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker.gpg
echo "deb [arch=amd64 signed-by=/usr/share/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | \
sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

sudo apt update
sudo apt install -y docker-ce docker-ce-cli containerd.io

# Habilitar o Docker
sudo systemctl enable docker
sudo systemctl start docker

# 5. Instalar CRI (Container Runtime Interface)
sudo apt install -y containerd
sudo systemctl enable containerd
sudo systemctl start containerd
```

---

## Instalar Kubernetes (kubeadm, kubelet, kubectl)

```bash
# Adicionar chave e repositório
curl -s https://packages.cloud.google.com/apt/doc/apt-key.gpg | sudo apt-key add -

echo "deb https://apt.kubernetes.io/ kubernetes-xenial main" | \
sudo tee /etc/apt/sources.list.d/kubernetes.list

# Instalar pacotes
sudo apt update
sudo apt install -y kubelet kubeadm kubectl

# Impedir atualizações automáticas (evita conflitos)
sudo apt-mark hold kubelet kubeadm kubectl
```

## VM1 – Nó Mestre

### 1. Inicializar o Cluster

```bash
sudo kubeadm init --pod-network-cidr=10.244.0.0/16
```

> O parâmetro `--pod-network-cidr=10.244.0.0/16` é essencial para compatibilidade com o Flannel.

### 2. Configurar kubectl

```bash
mkdir -p $HOME/.kube
sudo cp -i /etc/kubernetes/admin.conf $HOME/.kube/config
sudo chown $(id -u):$(id -g) $HOME/.kube/config
```

### 3. Aplicar rede de pods (Flannel)

```bash
kubectl apply -f https://raw.githubusercontent.com/flannel-io/flannel/master/Documentation/kube-flannel.yml
```

### 4. Gerar token de acesso para os workers

```bash
kubeadm token create --print-join-command
```

Copie o comando exibido (ex: `kubeadm join <IP>:6443 --token ...`) e utilize nas VMs Worker.

## VM2 – Worker (Engine MPI/OpenMP)

### 1. Executar comando de join (gerado na VM1)

```bash
# Substitua pelo comando real gerado no master
sudo kubeadm join <MASTER-IP>:6443 --token <TOKEN> --discovery-token-ca-cert-hash sha256:<HASH>
```

### 2. Verificar conexão no master (VM1)

Na VM1:

```bash
kubectl get nodes
```

A VM2 deverá aparecer como "Ready".

---

## VM3 – Worker (Engine Apache Spark)

### 1. Executar comando de join (gerado na VM1)

```bash
# Substitua pelo comando real gerado no master
sudo kubeadm join <MASTER-IP>:6443 --token <TOKEN> --discovery-token-ca-cert-hash sha256:<HASH>
```

### 2. Verificar estado no cluster

Na VM1:

```bash
kubectl get nodes
```

A VM3 também deverá aparecer como "Ready".

---

## Verificação Final

Na VM1 (master), use:

```bash
kubectl get nodes
kubectl get pods --all-namespaces
```

Todos os nós devem estar com status `Ready` e a rede do Flannel ativa. Os pods do sistema Kubernetes devem estar em estado `Running`.

---

## Referência Oficial

- [Instalar kubeadm, kubelet e kubectl - Kubernetes Docs](https://kubernetes.io/docs/setup/production-environment/tools/kubeadm/install-kubeadm/)
