##  📊 Guia de Execução de Testes: Cenário 1 (DDoS Volumétrico)

![Tcpdump](https://img.shields.io/badge/Tcpdump-network%20analysis-orange?style=flat-square&logo=wireshark)
![Hping3](https://img.shields.io/badge/Hping3-packet%20crafting-red?style=flat-square&logo=linux)
![DDoS](https://img.shields.io/badge/DDoS%20%26%20Slow-attack%20simulation-darkred?style=flat-square&logo=apache)
![Cybersecurity](https://img.shields.io/badge/Cybersecurity-lab%20focus-blue?style=flat-square&logo=security)
![IoT](https://img.shields.io/badge/IoT-connected%20devices-lightblue?style=flat-square&logo=internetofthings)


Este guia orienta a validação do protótipo através do estabelecimento de tráfego legítimo, simulação de ataque de inundação e extração de metricas diretamente do plano de dados.

| Arquivo / Diretório | Camada de Execução | Descrição Técnica |
| :--- | :--- | :--- |
| **`xdp_monitor.c`** | Kernel Space | filtragem de pacotes na placa de rede, inspeciona os cabeçalhos L3/L4. |
| **`topologia.yml`** | Infraestrutura | automatiza o deploy do laboratório. |
| **`ddos.py`** | User Space | Script que consome a API do `bpftool`, extrai os contadores e calcula a taxa de pacotes por segundo (PPS). |

--- 
## 📋 Pré-requisitos e Instalação

```
# Atualize o índice de pacotes
sudo apt update

# Instale o compilador Clang e as ferramentas essenciais de compilação
sudo apt install -y clang llvm make

# Instale os cabeçalhos do Kernel correspondentes à sua versão atual
sudo apt install -y linux-headers-$(uname -r)
```
---
## 🔄 Fluxo de Execução e Ciclo de Vida do Pacote

O diagrama abaixo descreve o fluxo de execução assíncrono entre o Plano de Dados (Kernel) e o Plano de Controle (User Space) a cada pacote recebido na interface `eth1` do Gateway IoT:

<img width="570" height="313" alt="image" src="https://github.com/user-attachments/assets/f5a012d9-3a03-4cbb-b9f2-84d79e8c1476" />

---
## 📊 Métricas Coletadas e Modelagem de Dados no XDP

  ### 1. Estrutura do Mapa eBPF (`proto_stats`) -  **Métricas Nativas do Kernel**

O programa Plano de Dados (`xdp_monitor.c`) utiliza um mapa do tipo `BPF_MAP_TYPE_ARRAY` de tamanho fixo. Este tipo de mapa foi escolhido por possuir complexidade de busca constante $O(1)$, garantindo que o incremento dos contadores não adicione latência ao processamento de pacotes no driver de rede.

* **Nome do Mapa no Código C:** `proto_stats`
* **Caminho do Vínculo (*Pinned Path*):** `/sys/fs/bpf/proto_stats`
* **Estrutura de Dados Base:**
  * `Key`: `__u32` (4 bytes) — Índice da métrica.
  * `Value`: `__u64` (8 bytes) — Contador atômico de pacotes de 64 bits (evita *overflow* mesmo em ataques de grande escala).

---

### 2. Matriz de Métricas e Chaves do Mapa

Mapeamento contabilizado por cada índice do mapa XDP:

| Métrica Coletada | Chave (*Key*) | Estrutura XDP / Tipo de Dado | Descrição Técnica no Kernel Space |
| :--- | :---: | :--- | :--- |
| **Volume Total UDP** | `0` | `BPF_MAP_TYPE_ARRAY` (`__u64`) | Contador (*UDP Flood*). |
| **Volume Total TCP** | `1` | `BPF_MAP_TYPE_ARRAY` (`__u64`) | Contador que registra cada pacote IPv4 do protocolo TCP, mostra resiliência do tráfego legítimo (ex: tráfego MQTT/HTTP) durante a detecção. |

---

### 3. Métricas Derivadas (Plano de Controle em Python)

O script `ddos.py` consome os dados puros do mapa acima a cada intervalo estrito de 1 segundo e calcula as seguintes métricas de desempenho de rede para a composição dos gráficos:

#### A. Vazão de Ataque (Taxa PPS - *Packets Per Second*)
Indica a intensidade instantânea do tráfego que está atingindo o Gateway a cada segundo. É calculada através da variação temporal do contador UDP:

$$\text{PPS}_{\text{Atual}} = \text{Total UDP}_{(t)} - \text{Total UDP}_{(t-1)}$$

* **Aplicação Científica:** Demonstra em tempo real o momento exato em que a ferramenta de ataque (`hping3`) inicia a inundação e a capacidade do XDP de sustentar e processar essa taxa sem travar o sistema operacional.

#### B. Linha de Base de Tráfego Legítimo (Variação TCP)
Mede a estabilidade das conexões orientadas a fluxo durante o cenário de crise:

$$\text{Delta TCP} = \text{Total TCP}_{(t)} - \text{Total TCP}_{(t-1)}$$

* **Aplicação Científica:** Comprova o isolamento de tráfego. Se o Delta TCP se mantiver estável enquanto o PPS UDP explode, fica provado empiricamente que o mecanismo de descarte ou contabilização precoce do XDP protegeu os recursos do barramento de comunicação IoT.

---

### 4. Consumo de CPU
* **Objetivo:** Comprovar o baixo custo computacional do processamento em nível de driver (XDP) em comparação com abordagens tradicionais em Espaço de Usuário ou Netfilter/Iptables.
* **Métrica:** Porcentagem de uso de CPU do container do Gateway sob estresse de ataque.
* **Validação:** Sob ataque massivo, firewalls comuns tendem a elevar o consumo de CPU para 100% (causando negação de serviço legítimo). O XDP deve manter o consumo de CPU em níveis mínimos.

### 5. Resiliência do Tráfego Legítimo IoT (Mensageria MQTT)
* **Objetivo:** Demonstrar o isolamento de desempenho. Provar que o tráfego malicioso em L3/L4 (UDP Flood) é mitigado de forma tão precoce que não afeta a estabilidade das conexões persistentes de sensores reais.
* **Métrica:** Taxa de entrega de mensagens (*Publish*) e persistência da conexão TCP do protocolo MQTT (Porta 1883) do nó Cliente em direção ao Broker.
* **Validação:** Manutenção do canal de comunicação do sensor ativo e funcional, sem quedas por *timeout* ou perda de pacotes legítimos durante a inundação.

### 6. Latência de Passagem (RTT - Round Trip Time)
* **Objetivo:** Avaliar o atraso introduzido pelo filtro eBPF no tráfego que recebe o veredito de encaminhamento.
* **Métrica:** Tempo de ida e volta em milissegundos (ms) dos pacotes legítimos que atravessam o Gateway.
* **Validação:** A latência do tráfego legítimo deve se manter estável (sub-milissegundos), provando que a execução do código BPF no Kernel não gera gargalos na rede.

---

### 📊 Matriz de Correlação de Métricas para Cenários de Teste

A avaliação do ecossistema para a composição dos resultados do artigo adota a seguinte matriz estruturada:

| Dimensão Avaliada | Métrica Principal | Unidade | Componente Impactado | Ferramenta de Coleta |
| :--- | :--- | :---: | :--- | :--- |
| **Intensidade do Ataque** | Vazão Volumétrica | PPS | Interface de Rede (`eth1`) | `ddos.py` (eBPF Map Key 0) |
| **Sobrevivência IoT** | Integridade do Fluxo | % Mensagens | Conectividade TCP MQTT | Logs do Cliente Mosquitto |
| **Custo de Hardware** | Sobrecarga de Sistema | % CPU | Processador do Gateway | `docker stats` / `top` |
| **Atraso de Rede** | Tempo de Trânsito | ms | Pilha de Protocolos | `ping` / Latência de aplicação |



---



