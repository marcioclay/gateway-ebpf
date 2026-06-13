
| Dimensão / Métrica de Detecção | Tipo de Ataque Alvo | Mapa `tcp_session` (Micro / Por IP) | Mapa `proto_stats` (Macro / Global) | Mecanismo de Funcionamento / Limitação no XDP |
| :--- | :--- | :--- | :--- | :--- |
| **Taxa de Pacotes por Segundo (PPS)** | DDoS (Volumétrico) | **Sim**. Incrementa o contador associado ao IP de origem a cada pacote intercetado. | **Sim**. Incrementa o contador global do protocolo (ex: TCP) na rede. | Mede surtos de tráfego bruto diretamente no driver, antes da alocação do `sk_buff` pelo Kernel. |
| **Consumo de Banda (Mbps)** | DDoS (Volumétrico) | **Sim**. Acumula o tamanho em bytes dos pacotes recebidos por aquele IP específico. | **Sim**. Acumula a volumetria total de bytes que trafegam pelo protocolo correspondente. | Calculado em tempo de execução através da diferença de ponteiros de memória (`ctx->data_end - ctx->data`). |
| **Conexões Simultâneas por IP** | Slow DoS (SlowITe) | **Não diretamente**. Rastreia pacotes de transporte (L4), mas não valida se a aplicação (L7) aceitou a sessão. | **Não**. Agrega os dados por protocolo, perdendo a visibilidade individual de cada socket. | **Limitação Crítica:** O XDP atua antes do handshake MQTT. Se o Broker rejeitar o pacote truncado, o mapa eBPF registará tráfego, gerando um falso positivo. |
| **Persistência / Tempo de Vida** | Slow DoS (SlowITe) | **Sim**. Atualiza o campo `last_seen` com o timestamp do último pacote recebido daquele IP. | **Não**. O mapa macro não possui granularidade para monitorizar o ciclo de vida de fluxos isolados. | Utiliza a função auxiliar do Kernel `bpf_ktime_get_ns()` para calcular a duração da inatividade da sessão. |
| **Tamanho Médio dos Pacotes** | Slow DoS (SlowITe) | **Sim (por inferência)**. Permite calcular a média individual (`Bytes / Pacotes`) no componente em *user space*. | **Sim (Alerta Global)**. Evidencia anomalias se a média global de tamanho de pacotes da rede cair drasticamente. | Permite identificar o envio de pacotes malformados ou incompletos, contendo apenas bytes de controlo (ex: `\x10`). |
| **Densidade / Frequência** | Slow DoS (SlowITe) | **Sim**. Rastreia o intervalo de tempo (*delta*) entre pacotes consecutivos de um mesmo endereço de origem. | **Não**. O tráfego legítimo dos sensores mistura-se com o tráfego do ataque, mascarando a frequência. | Deteta a cadência calculada do atacante, desenhada especificamente para evitar os mecanismos de *timeout* da aplicação. | 


---

+-------------------------------------------------------+
       |                  DASHBOARD (Python)                   |
       +---------------------------+---------------------------+
                                   |
                +------------------+------------------+
                |                                     |
                v                                     v
   [ Fonte 1: Mapas eBPF ]               [ Fonte 2: Kernel Linux ]
   Via: bpftool map dump                 Via: /proc/stat (ou psutil)
   Métricas: Pacotes e Bytes             Métricas: % CPU Total, % SoftIRQ
