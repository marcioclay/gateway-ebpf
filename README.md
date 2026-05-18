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

Este protótipo apresenta o desenvolvimento de uma Sonda de Monitoramento de Segurança para redes IoT, desenvolvida para a disciplina de Redes Programáveis do Mestrado em Computação Aplicada. O objetivo é a detecção e análise de tráfego anômalo (DDoS e Slow DoS) em tempo real, utilizando as tecnologias eBPF (extended Berkeley Packet Filter) e XDP (eXpress Data Path).

## O que este protótipo demonstra:

- Observabilidade em Nível de Kernel: Implementação de um programa eBPF em C para inspeção de cabeçalhos e extração de metadados de tráfego em tempo real.

- Análise de Fluxo na Borda: Uso do XDP como uma sonda de entrada para capturar estatísticas de rede antes da alocação de buffers de socket (sk_buff), permitindo uma medição fiel da carga de ataque.
- Orquestração de Cenário de Teste: 
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


## 🌐 Topologia da Rede


```
┌────────────────────────────────────────────────────────┐
│                      MÁQUINA HOST                      │
│                                                        │
│  ┌────────────────────────┐    ┌────────────────────┐  │
│  │    clab-mqtt-sensor    │    │ clab-mqtt-atacante │  │
│  │     (10.0.0.20/24)     │    │   (10.0.0.10/24)   │  │
│  └───────────┬────────────┘    └──────────┬─────────┘  │
│              │ eth1                       │ eth1       │
│              └──────────────┬─────────────┘            │
│                             ▼                          │
│                  ┌────────────────────┐                │
│                  │  clab-mqtt-bridge  │                │
│                  │  (Switch Virtual)  │                │
│                  └──────────┬─────────┘                │
│                             │ eth1                     │
│                             ▼                          │
│                  ┌────────────────────┐                │
│                  │ clab-mqtt-gateway  │                │
│                  │   (10.0.0.1/24)    │                │
│                  │ [Filtro eBPF/XDP]  │                │
│                  └────────────────────┘                │
└────────────────────────────────────────────────────────┘

```


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

## 🐝 Passo 3 - Configuração e Inicialização do Ambiente

Após clonar o repositório e subir a topologia com `sudo clab deploy -t topology.yml`, execute os comandos abaixo para preparar os ambientes internos de cada container.

### A. Configurar o Gateway (Broker Mosquitto)
Entre no Gateway, instale as dependências de rede e force a inicialização correta do Broker corrigindo as pastas de permissão do sistema:
```
# Instala as ferramentas necessárias
docker exec -it clab-ebpf-mqtt-gateway apt-get update
docker exec -it clab-ebpf-mqtt-gateway apt-get install -y mosquitto iproute2 iputils-ping tcpdump libbpf-dev

# Corrige os diretórios do Mosquitto e inicia o serviço
docker exec -it clab-ebpf-mqtt-gateway sh -c "mkdir -p /run/mosquitto && chown -R mosquitto:mosquitto /run/mosquitto"
docker exec -it clab-ebpf-mqtt-gateway sh -c "echo 'listener 1883\nallow_anonymous true' > /etc/mosquitto/mosquitto.conf"
docker exec -it clab-ebpf-mqtt-gateway mosquitto -d -c /etc/mosquitto/mosquitto.conf
```

B. Configurar o Atacante (Ferramentas de Ataque)
Instale o hping3 para os testes volumétricos:

```
docker exec -it clab-ebpf-mqtt-atacante apk add --no-cache hping3
```

C. Configurar o Sensor Legítimo (Biblioteca MQTT)

Instale o gerenciador Python contornando a restrição de ambiente gerenciado (PEP 668):

```
docker exec -it clab-ebpf-mqtt-sensor sh -c "apk add --no-cache py3-pip && pip3 in
```

## 🐝 Passo 4 — Verificar Conectividade Inicial

Antes de ativar o filtro XDP, confirme que os nós se comunicam normalmente:

```bash
docker exec clab-ebpf-mqtt-sensor ping -c 3 10.0.0.1
```

**Resultado esperado:** `0% packet loss`  

---

## 🐝 Passo 5 — Ativar o Filtro XDP

### 4.1 Ativar a Sonda eBPF/XDP

```
# Carrega o programa nativamente no Gateway via iproute2
docker exec -it clab-ebpf-mqtt-gateway ip link set dev eth1 xdpgeneric obj /src/xdp_monitor.o sec xdp
```

4.2 Verificar se o programa está ativo

```
docker exec -it clab-ebpf-mqtt-gateway ip link show eth1
```

4.3 Possíveis problemas no virtual box

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

---

## 🧹 Limpeza

Para destruir o laboratório e remover todos os containers:

```
sudo containerlab destroy -t topologia.yml
```



## 📂 Estrutura do Projeto - ALTERAR

```
.
├── clab-ebpf-mqtt
│   ├── ansible-inventory.yml
│   ├── authorized_keys
│   ├── nornir-simple-inventory.yml
│   └── topology-data.json
├── compile.sh
├── README.md
├── src
│   ├── sensor.py
│   └── slow_attack.py
├── topologia.yml
├── xdp_monitor.c
└── xdp_monitor.o

```

---

## 📚 Referências 

- [Documentação Oficial do eBPF](https://ebpf.io/what-is-ebpf/)
- [Documentação do Containerlab](https://containerlab.dev/quickstart/)
- [Tutorial XDP (kernel.org)](https://github.com/xdp-project/xdp-tutorial)
- [libbpf GitHub](https://github.com/libbpf/libbpf)
- [nicolaka/netshoot — Container de diagnóstico de rede](https://github.com/nicolaka/netshoot)
- [Tutorial de artigo ataque slow](https://github.com/gianluca2414/MQTT_SlowITe )




