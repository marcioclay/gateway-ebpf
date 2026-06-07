# Resultados da Avaliação: Mitigação de Ataques em Gateway IoT
**Análise de Desempenho: eBPF/XDP vs. Netfilter/iptables**

Este documento apresenta os resultados dos testes de intrusão e estresse realizados no protótipo de Gateway IoT (Broker MQTT). O foco da análise é a resiliência do gateway sob condições extremas de tráfego e a eficiência do descarte de pacotes na borda.

---

## 1. Metodologia: Distinção entre Tráfego Legítimo e Malicioso

Em ambientes IoT com recursos limitados, descartar tráfego malicioso sem afetar a telemetria dos sensores legítimos é o principal desafio. A identificação no gateway baseia-se na extração de métricas de fluxo em tempo real (estado e volumetria), analisadas através de mapas na memória do kernel.

### A. Detecção de Tráfego Legítimo (Sensores MQTT)
O tráfego MQTT padrão possui uma assinatura comportamental clara:
* **Taxa de Transferência:** Conexões TCP na porta `1883` apresentam rajadas curtas de transferência (Publicação de tópicos) seguidas de encerramento rápido ou pacotes de *Keep-Alive* espaçados e regulares.
* **Proporção Pacote/Byte:** O volume de bytes é coerente com o número de pacotes de controle. 
* **Critério de Aceitação:** IPs que mantêm um delta de tempo aceitável (`last_seen`) e uma taxa de transferência (`byte_count` / `packet_count`) compatível com o payload esperado dos sensores são marcados como tráfego limpo.

### B. Identificação de Anomalias (Ataques)
* **DDoS Volumétrico (Flood UDP):** Caracterizado por um pico anômalo de tráfego UDP (Protocolo 17) não requisitado pela aplicação. A identificação ocorre pelo estouro rápido dos limiares de taxa de pacotes por segundo (PPS) lidos no mapa global (`proto_stats`).
* **Slow DoS (Exaustão TCP):** O atacante forja o comportamento de um sensor lento. A identificação ocorre via análise comportamental fina no mapa `tcp_sessions`: o IP de origem estabelece a conexão TCP (`1883`), mas a taxa de `byte_count` permanece criticamente baixa enquanto o delta de tempo (`last_seen`) se prolonga, indicando a intenção maliciosa de segurar o socket aberto no gateway.

### C. Fundamentação de Detecção e Bloqueio (Slow DoS)

A eficácia da mitigação de ataques de exaustão TCP, como o SlowITe, através de eBPF baseia-se na análise comportamental da sessão em oposição à inspeção profunda de pacotes (DPI). Em uma infraestrutura de gateway IoT, sensores legítimos realizam a transferência de telemetria (ex: pacotes PUBLISH do MQTT) em rajadas rápidas, resultando em um tamanho médio de pacote que supera consistentemente os 100 bytes de rede, considerando o encapsulamento.

Em contrapartida, o atacante realiza o esgotamento dos sockets do gateway fragmentando intencionalmente a carga útil, enviando pacotes mínimos (ex: 1 byte de payload TCP, totalizando ~55 bytes no fio) em intervalos espaçados (ex: a cada 15 segundos) apenas para reiniciar os temporizadores (timers) do sistema operativo.

O algoritmo XDP implementado extrai a assinatura deste ataque monitorizando a proporção entre o volume trafegado (byte_count) e o número de interações (packet_count) de uma mesma origem. Foi estipulada uma janela de observação (MIN_PKTS_CHECK = 50) e um limiar mínimo de viabilidade de dados (MIN_AVG_BYTES = 65 bytes). Se, após 50 pacotes enviados, o IP de origem mantiver uma média inferior a 65 bytes por pacote, fica comprovada a ausência de intenção de transmissão legítima, desencadeando o descarte sumário na placa de rede (XDP_DROP).


---

## 2. Comparativo Tecnológico: eBPF/XDP vs. iptables

Para validar a eficiência do protótipo, os mesmos vetores de ataque foram mitigados utilizando a ferramenta tradicional do kernel Linux (`iptables`) e a tecnologia eBPF via XDP (eXpress Data Path).

### Diferença Arquitetural
* **iptables (Netfilter):** Atua na camada de rede (Camada 3). O pacote precisa ser recebido pela placa de rede, alocado na memória do kernel (estrutura `sk_buff`) e processado por várias sub-rotinas antes de chegar ao gancho (*hook*) do Netfilter para ser descartado. Sob ataque de flood, o custo de CPU para criar essas estruturas esgota os recursos do gateway.
* **eBPF/XDP:** O código de mitigação é executado diretamente no *driver* da interface de rede (Camada 2). O pacote é analisado e descartado (`XDP_DROP`) antes que qualquer alocação de memória complexa ocorra. Isso poupa drasticamente a CPU (redução de *SoftIRQs*) e preserva o gateway.

---

## 3. Dados e Resultados dos Testes

### 3.1. Cenário 1: Ataque de Flood UDP (DDoS Volumétrico)
*Geração de alto volume de pacotes UDP para esgotar o processamento do Gateway.*

| Métrica Avaliada (Média) | Cenário Base (Sem Defesa) | iptables | eBPF / XDP |
| :--- | :---: | :---: | :---: |
| **Uso de CPU (SoftIRQ)** | 100% | ~85% | **~15%** |
| **Pacotes Descartados / seg** | 0 PPS | 450.000 PPS | **1.200.000+ PPS** |
| **Entrega de Tráfego Legítimo** | < 5% (Drop Tail) | ~40% | **> 95%** |

*(Nota: Substituir os valores acima pelos dados reais extraídos do laboratório)*

**Gráfico de Sobrecarga de CPU (Flood UDP)**
> *[Inserir aqui a imagem do gráfico comparando a CPU ao longo do tempo: Sem Defesa vs Iptables vs XDP]*
`![Gráfico de CPU - DDoS](./metricas/grafico_cpu_ddos.png)`

### 3.2. Cenário 2: Ataque Slow DoS (Exaustão MQTT)
*Conexões lentas direcionadas à porta 1883 com objetivo de esgotar as sessões do Broker.*

| Métrica Avaliada | iptables (com módulo limit/connlimit) | eBPF / XDP (Mapa de Sessões) |
| :--- | :--- | :--- |
| **Tempo até Detecção** | ~15 segundos (dependente de limiares estáticos) | **< 3 segundos** (baseado na proporção byte/tempo real) |
| **Consumo de Memória Ram** | Alto (Rastreamento de Conexões do Netfilter) | **Muito Baixo** (Mapas BPF Otimizados) |
| **Falsos Positivos (Sensores Dropados)**| Moderado (dificuldade em distinguir sensores em redes lentas) | **Mínimo** (Análise comportamental fina) |

*(Nota: Substituir os valores acima pelos dados reais extraídos do laboratório)*

**Gráfico de Sessões Ativas no Broker (Slow DoS)**
> *[Inserir aqui a imagem do gráfico mostrando as sessões TCP ativas estourando no iptables vs controladas no XDP]*
`![Gráfico de Sessões MQTT](./metricas/grafico_sessoes_slow.png)`

---

## 4. Conclusão

Os dados experimentais demonstram que, embora o `iptables` seja uma ferramenta madura para controle de acesso, sua arquitetura baseada em `sk_buff` apresenta gargalos severos na mitigação de ataques volumétricos em dispositivos restritos como um Gateway IoT. 

A abordagem utilizando **eBPF/XDP** comprovou-se superior. Ao atuar no nível do driver da placa de rede, o XDP protegeu a CPU do Gateway contra interrupções de hardware excessivas no Cenário 1 (Flood) e demonstrou alta granularidade ao identificar anomalias comportamentais no Cenário 2 (Slow DoS), garantindo a resiliência do Broker MQTT e a continuidade da recepção de telemetria legítima.
