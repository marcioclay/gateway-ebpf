📊 Guia de Suporte 


Este guia orienta a validação do protótipo através do estabelecimento de tráfego legítimo, simulação de ataque de inundação e extração de metricas diretamente do plano de dados.

## 1. Visualização dos mapas utilizados para observação no xdp:

Mapas - proto_stats e tcp_sessions
Após reiniciar o containerlab o id dos mapas é alterado. O dashboard já mostra os dados retirados dos mapas, porém caso queira ver o mapas
siga as orientações abaixo.

```
# Deixe este comando rodando na tela. Ele vai atualizar sozinho.
watch -n 1 sudo docker exec -it clab-lab-ebpf-gateway bpftool map dump id 55
```


#### Caso tenha reiniciado será necessário atualizar o id do mapa.
```
sudo docker exec -it clab-lab-ebpf-gateway bpftool map list
```
```
# Mensagem na tela
120: hash  name proto_stats  flags 0x0
	key 4B  value 8B  max_entries 10  memlock 4096B
121: hash  name ip_stats     flags 0x0
	key 4B  value 16B  max_entries 1024  memlock 8192B
# Procure pelos nomes dos seus mapas (como proto_stats).
# O número que aparece logo no início da linha (ex: 120, 121) é o seu novo ID.
```
---

```
# Mostra o staus de cpu
sudo docker stats clab-lab-ebpf-gateway clab-lab-ebpf-atacante
```
#### Observação: "Em ambientes virtualizados (Containerlab/Docker), a taxa de injeção de pacotes via software (veth) gera um gargalo no próprio nó atacante (exibindo consumo de até 180% de CPU para a geração do tráfego malicioso). Devido à eficiência da pilha de rede do Linux no descarte de pacotes UDP direcionados a portas fechadas, o nó Gateway consegue processar o volume gerado em modo XDP_PASS sem atingir a exaustão de recursos."

| CONTAINER ID |            NAME          | CPU % | MEM USAGE / LIMIT   | MEM % | NET I/O       | BLOCK I/O       | PIDS |
| b235ea9d8da8 | clab-lab-ebpf-gateway    | 0.05% | 8.086MiB / 2.832GiB | 0.28% | 14.4MB / 61kB | 12.6MB / 41.8MB | 2 |
| ee11dc9f8119 | clab-lab-ebpf-atacante | 185.94% | 46.23MiB / 2.832GiB | 1.59% | 152MB / 377kB | 6.35MB / 624MB | 2 | 

---

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
