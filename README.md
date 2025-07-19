# PSPD-trabalho-final
Repositório para o Trabalho Final da disciplina PSPD da FCTE


Aluno | Matricula
--|--
Artur Vinicius Dias Nunes | 190142421
Henrique Hida | 180113569
João Manoel Barreto Neto | 211039519 
Leonardo Milomes Vitoriano | 201000379
Miguel Matos Costa de Frias Barbosa | 211039635


## Descrição


O projeto consiste em uma aplicação distribuída desenvolvida para orquestração via Kubernetes em um ambiente self-hosted, 
utilizando máquinas virtuais (VMs) provisionadas na plataforma Microsoft Azure. O objetivo principal é implementar um sistema 
de processamento paralelo do "Jogo da Vida" (Game of Life), que pode ser executado com diferentes motores de execução: 
Apache Spark para o processamento distribuído e OpenMP/MPI para processamento paralelo em nível de CPU.


## Arquitetura

A arquitetura do sistema é composta por uma VM principal (vm1) que atua como nó de controle (control-plane) do cluster Kubernetes, responsável por expor uma interface de entrada por meio de conexões socket. Esta interface permite que o usuário envie requisições que contenham os parâmetros do jogo, como tamanho da matriz, número de gerações e motor de execução desejado. As VMs secundárias (vm2, vm3 e vm4) são adicionadas como nós workers ao cluster e são responsáveis por executar as simulações de forma escalável, conforme a disponibilidade de recursos.

O sistema utiliza um cluster Kubernetes autogerenciado (self-hosted), configurado manualmente em cada VM com `kubeadm`. Os pods do cluster são agendados de forma automática para os nós disponíveis, e cada motor de execução (Spark, OpenMP ou MPI) é encapsulado em containers executando jobs especializados. As métricas de desempenho e execução são exportadas em tempo real e armazenadas no Elasticsearch, com visualização e análise feita por meio do Kibana, facilitando a avaliação e comparação entre diferentes técnicas de paralelismo.

A comunicação entre os serviços é realizada por meio de sockets TCP e enviada periodicamente ao stack ELK (ElasticSearch - Kibana). O projeto tem como foco principal a análise de desempenho de algoritmos paralelos em ambientes distribuídos, oferecendo uma estrutura prática e extensível para avaliação de workloads científicos em escala.

A Figura 1 ilustra de forma completa toda a infraestrutura e comunicação entre os serviços.

<p align="center">
  <img src="./assets/Arquitetura3VMs.png" alt="Figura 1 - Arquitetura geral" width="100%">
</p>

<p align="center"><strong>Figura 1</strong> - Arquitetura geral</p>



## Confi

Exececutar todos os comandos abaixo, tanto no nó mestre quanto nos nós workers.

```bash

# Atualizar pacotes
sudo apt update && sudo apt upgrade -y

# Desabilitar swap
sudo swapoff -a
sudo sed -i '/ swap / s/^/#/' /etc/fstab

# Instalar dependências
sudo apt install -y apt-transport-https ca-certificates curl

# Adicionar repositório Docker
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker.gpg
echo "deb [arch=amd64 signed-by=/usr/share/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | \
sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# Instalar Docker
sudo apt update
sudo apt install -y docker-ce docker-ce-cli containerd.io

# Habilitar e iniciar Docker
sudo systemctl enable docker
sudo systemctl start docker

# Adicionar repositório Kubernetes
sudo curl -fsSL https://packages.cloud.google.com/apt/doc/apt-key.gpg | sudo apt-key add -
echo "deb https://apt.kubernetes.io/ kubernetes-xenial main" | \
sudo tee /etc/apt/sources.list.d/kubernetes.list

# Instalar kubeadm, kubelet e kubectl
sudo snap install kubelet --classic
sudo snap install kubectl --classic
sudo snap install kubeadm --classic



```


## Intalar CRI (Container Run Time)


Instale containerd (ou CRI-O) e crictl



CHECK kubectl kubelet kubeadm




## Instalar kubeadm kubectl kubelet


kubeadm: the command to bootstrap the cluster.

kubelet: the component that runs on all of the machines in your cluster and does things like starting pods and containers.

kubectl: the command line util to talk to your cluster.


Para instalar `kubeadm`, `kubectl` e `kubelet` foi seguido o tutorial de instalação
oferecido pela documentação oficial do Kubernertes, seguindo o link abaixo.

- [Instalar kubeadm kubectl kubelet](https://kubernetes.io/docs/setup/production-environment/tools/kubeadm/install-kubeadm/)



## 2. Aplique a rede de pods (Flannel)

A rede de pods utilizada foi a Flannel, então no nó master foi executado o comando:

```bash

kubectl apply -f https://raw.githubusercontent.com/flannel-io/flannel/master/Documentation/kube-flannel.yml

```





