# 🐝 Protótipo XDP/eBPF e MQTT com Containerlab

> Protótipo de **deteção de pacotes em um Gateway com MQTT** usando **eBPF/XDP** em um ambiente de rede virtualizado com **Containerlab**.

[![Containerlab](https://img.shields.io/badge/Containerlab-v0.50+-blue?logo=linux)](https://containerlab.dev)
[![Docker](https://img.shields.io/badge/Docker-required-blue?logo=docker)](https://www.docker.com)
[![eBPF](https://img.shields.io/badge/eBPF-XDP-orange)](https://ebpf.io)
[![Licença](https://img.shields.io/badge/licença-GPL--2.0-green)](LICENSE)
![MQTT](https://img.shields.io/badge/Protocol-MQTT-blue)
[![Linguagem](https://img.shields.io/badge/linguagem-C-blue)](https://en.wikipedia.org/wiki/C_(programming_language))
[![OS](https://img.shields.io/badge/OS-Ubuntu-orange)](https://ubuntu.com/)

---
## 📖 Visão Geral

Este protótipo apresenta o desenvolvimento de uma Sonda de Monitoramento de Segurança para redes IoT, desenvolvida para a disciplina de Redes Programáveis do Mestrado em Computação Aplicada. O objetivo central é a detecção e análise de tráfego anômalo (DDoS e Slow DoS) em tempo real, utilizando as tecnologias eBPF (extended Berkeley Packet Filter) e XDP (eXpress Data Path).

Diferente das abordagens tradicionais baseadas em instâncias de usuário ou logs de aplicação, este protótipo utiliza o eBPF para inspecionar pacotes diretamente no nível mais baixo do kernel Linux. Isso permite uma visibilidade profunda e de altíssima performance sobre o tráfego que chega ao Broker MQTT (Mosquitto), permitindo identificar padrões de ataque antes mesmo que o sistema operacional processe os dados, garantindo telemetria de precisão com custo computacional mínimo.

## 🚀 O que este protótipo demonstra:

- Observabilidade em Nível de Kernel: Implementação de um programa eBPF em C para inspeção de cabeçalhos e extração de metadados de tráfego em tempo real.

- Análise de Fluxo na Borda: Uso do XDP como uma sonda de entrada para capturar estatísticas de rede antes da alocação de buffers de socket (sk_buff), permitindo uma medição fiel da carga de ataque.

- Orquestração de Cenário de Teste: Deploy de uma infraestrutura virtualizada com Containerlab, simulando um ambiente IoT onde um Gateway (Vítima) monitora o tráfego vindo de sensores legítimos e dispositivos comprometidos.
- Identificação de Vetores de Ataque:
  - Monitoramento Volumétrico (DDoS): Detecção de inundações UDP/TCP através de contadores de taxa de pacotes por IP.
  - Análise de Comportamento (Slow DoS): Rastreamento de estados de conexões MQTT para identificar tentativas de exaustão de recursos por     conexões persistentes e lentas.
- Extração de Métricas via BPF Maps: Uso de mapas do tipo HASH e ARRAY para comunicar as estatísticas detectadas no kernel com o espaço de usuário, permitindo visualização e alertas.

  Ajustes Técnicos no Protótipo:
requisito "detecção", o fluxo de trabalho do laboratório será focado em:

1. Ação XDP: O programa eBPF utilizará exclusivamente o retorno XDP_PASS. Isso garante que todos os pacotes (benignos ou maliciosos) continuem sua jornada para o Broker, mas não antes de serem contabilizados e analisados pela lógica de detecção.
2. Métricas de Desempenho: o foco será o Overhead de Observabilidade. Monitorar 100% do tráfego sob ataque pesado sem degradar a CPU do Gateway. 

---

## Topologia


INSERIR IMAGEM



- atacante: Máquina Linux usando a imagem nicolaka/netshoot (distro focada em ferramentas de rede).
- sensor: Máquina Linux usando a imagem nicolaka/netshoot (distro focada em ferramentas de rede).
- gateway: Máquina Linux nicolaka/netshoot com um bind, montando o arquivo xdp_drop.o do host diretamente para a raiz do container (/xdp_drop.o).


| Nó     | Endereço IP  | Função                                      |
|--------|-------------|---------------------------------------------|
| atacante | `10.0.0.10`  | Emissor de pacotes - ilegítimo        |
| sensor | `10.0.0.20`  | Sensor — emissor pacotes ICMP legítimo      |
| gateway | `10.0.0.1`  | Filtro XDP — MQTT - GATEWAY          |


---

## 🔧 Pré-requisitos


### 0. Requisitos do Sistema

Os seguintes requisitos devem ser atendidos para que a ferramenta containerlab seja executada com sucesso (https://containerlab.dev/install/):

- Um usuário com privilégios de sudo para executar o containerlab.

- Um servidor Linux, pode ser WSL2 (https://learn.microsoft.com/pt-br/windows/wsl/install).

### 1. Instalar o Docker

```bash
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER
```

> Saia e entre novamente na sessão após adicionar seu usuário ao grupo `docker`.

### 2. Instalar o Containerlab

```bash
bash -c "$(curl -sL https://get.containerlab.dev)"
```

Verifique a instalação:

```bash
containerlab version
```

---

## ⏬ Obtendo o Laboratório

Clone o repositório e acesse o diretório do laboratório:

```bash
git clone https://github.com/marcioclay/ebpf-mqtt.git
cd ebpf-mqtt
```

> 📁 Arquivos principais:
> - `topologia.yml` — Definição da topologia Containerlab
> - `xdp_monitor.c` — Código-fonte eBPF/XDP
> - `compile.sh` — Script de compilação via Docker

---

## ⚙️ Passo 1 — Compilar o Programa eBPF

O script `compile.sh` usa um **container Ubuntu 22.04 como ambiente de build**, dispensando a instalação de ferramentas de compilação no host.Isso evita que você precise instalar localmente todas as dependências de eBPF (que podem ser pesadas ou conflitar) diretamente no seu sistema host.

```bash
# Se não estiver no diretório do lab:
# cd ~/redes/ebpf-lab
chmod +x compile.sh
./compile.sh
```

<details>
<summary>O que o compile.sh faz?</summary>

Ele sobe um container Docker temporário que:
1. Instala `clang`, `llvm`, `libbpf-dev` e `gcc-multilib`.
2. Compila `xdp_monitor.c` gerando bytecode para a **máquina virtual BPF** (`-target bpf`).
3. Gera o arquivo objeto `xdp_monitor.o` no diretório atual.
4. Remove o container de build automaticamente (`--rm`).

</details>

**Saída esperada:**
```
Success! xdp_monitor.o created. 😱😱😱
```

---

## 🐝 Passo 2 — Deploy da Topologia

```
# 1. Cria a bridge definida no YAML
sudo ip link add name switch type bridge

# 2. Ativa a interface
sudo ip link set dev switch up
```

```bash
sudo containerlab deploy -t topologia.yml --reconfigure
```

Isso irá:
- Criar três containers Linux (`gateway` , `atacante` e `sensor`).
- Configurar os IPs nas interfaces `eth1` de cada nó.
- Montar o `xdp_monitor.o` dentro do `gateway` em `/xdp_monitor.o`.
- Criar um barramento virtual através de uma bridge conectando as interfaces eth1 dos nós atacante, sensor e gateway.

Verifique se o lab está rodando:

```bash
docker ps --filter "label=containerlab=ebpf-mqtt"
```

---

## 🐝 Passo 3 — Verificar Conectividade Inicial

Antes de ativar o filtro XDP, confirme que os nós se comunicam normalmente:

```bash
docker exec clab-ebpf-mqtt-sensor ping -c 3 10.0.0.20
```

**Resultado esperado:** `0% packet loss`  

---

## 🐝 Passo 4 — Ativar o Filtro XDP

### 4.1 Instalar o bpftool no gateway

```
# Atualiza os repositórios e instala ferramentas de rede e suporte a BPF
docker exec -it clab-ebpf-mqtt-gateway apt-get update
docker exec -it clab-ebpf-mqtt-gateway apt-get install -y iproute2 iputils-ping bpfcc-tools tcpdump
```

### 4.2 Carregar e pinar o programa XDP

```
# 1. Ajustar o MTU (erro 'Peer MTU is too large')
docker exec -it clab-ebpf-mqtt-gateway ip link set dev eth1 mtu 1400
sudo ip link set dev switch mtu 1400

# 2. Carregar o programa eBPF diretamente na interface eth1
# Usamos 'xdpgeneric' para máxima compatibilidade com drivers virtuais (veth)
docker exec -it clab-ebpf-mqtt-gateway ip link set dev eth1 xdpgeneric obj /xdp_monitor.o sec xdp

```
4.3 Verificar se o programa está ativo

```
docker exec -it clab-ebpf-mqtt-gateway ip link show eth1
```

4.4 Possíveis problemas no virtual box

Nó switch apaga ao reiniciar amaquina virtual
```
# Verifica se a bridge 'switch' aparece na lista
ip link show switch

```
Cria o switch
```
sudo ip link add name switch type bridge
sudo ip link set dev switch up
```
Sobe a topologia
```
sudo clab deploy -t topologia.yml --reconfigure
```

## 🐝 Passo 5 — Teste e Verificação

### 5.1 Confirmar que o ICMP está bloqueado

```
sudo docker exec clab-ebpf-mqtt-gateway ping -c 5 10.0.0.1
```

**Resultado esperado:** `100% packet loss` 🚫

### 5.2 Ler o contador de drops do BPF Map

```bash
sudo docker exec clab-ebpf-mqtt-gateway bpftool map dump name packet_count_ma
```

> *(O nome do mapa pode aparecer truncado como `packet_count_ma`; você pode usar `bpftool map show` para ver o ID.)*

**Resultado esperado:**
```json
[{
    "key": 0,
    "value": 5
}]
```

> O contador incrementa atomicamente a cada pacote ICMP descartado — seguro até em múltiplos núcleos de CPU.

---

## 🔓 Passo 6 — Desativar o Filtro

Para restaurar o tráfego ICMP normal:

```bash
sudo docker exec clab-ebpf-mqtt-gateway ip link set dev eth1 xdpgeneric off
```

Verifique que a conectividade foi restaurada:

```bash
sudo docker exec clab-ebpf-mqtt-gateway ping -c 3 10.0.0.1
```

**Resultado esperado:** `0% packet loss` 

---

## 🧹 Limpeza

Para destruir o laboratório e remover todos os containers:

```bash
sudo containerlab destroy -t topologia.yml
```

---

## 📂 Estrutura do Projeto - ALTERAR

```
ebpf-mqtt/
├── lab-ebpf.clab.yml        # Definição da topologia Containerlab
├── xdp_drop.c               # Código-fonte eBPF/XDP (drop ICMP + contador)
├── xdp_drop.o               # Bytecode BPF compilado (gerado pelo compile.sh)
├── compile.sh               # Script de compilação eBPF via Docker
├── ativation-test.md        # Guia de referência rápida
└── clab-ebpf-lab/           # Arquivos de runtime gerados pelo Containerlab
    ├── ansible-inventory.yml
    ├── nornir-simple-inventory.yml
    ├── authorized_keys
    └── topology-data.json
```

---

## 📚 Referências - INSERIR

- [Documentação Oficial do eBPF](https://ebpf.io/what-is-ebpf/)
- [Documentação do Containerlab](https://containerlab.dev/quickstart/)
- [Tutorial XDP (kernel.org)](https://github.com/xdp-project/xdp-tutorial)
- [libbpf GitHub](https://github.com/libbpf/libbpf)
- [nicolaka/netshoot — Container de diagnóstico de rede](https://github.com/nicolaka/netshoot)

