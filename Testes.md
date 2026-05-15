##  Guia de Testes: Detecção de DDoS Volumétrico com eBPF/XDP

Este guia descreve os procedimentos para validar a eficácia da sonda eBPF no monitoramento de ataques de inundação em ambientes IoT.  

Índice de Testes 

1. Configuração do Ambiente e Conectividade
2. Ativação da Sonda eBPF (Monitoramento)
3. Execução do Ataque de Inundação (UDP Flood)
4. Coleta de eMetrics e Validação

### A. Configuração do Ambiente e Conectividade
     
Verificação de conectividade na rede 10.0.0.0/24.

- Validar Conectividade:
  ```
  # nó sensor, pingue o gateway
  docker exec -it clab-ebpf-mqtt-sensor ping -c 5 10.0.0.1
  ```

- Preparar o Atacante:
  ```
  # Garantir que o hping3 está instalado(Apenas se o hping3 não executar)
  sudo docker exec -it clab-ebpf-mqtt-atacante apk add hping3
  ```

  B. Ativação da Sonda eBPF (Monitoramento)
A sonda xdp_monitor.c deve ser carregada na interface eth1 do Gateway para iniciar a contagem de pacotes por IP e protocolo.

Ajustar MTU e Carregar XDP:

```
# Ajuste de MTU necessário para veth/XDP(Use apenas se o comando "Carregar o programa" não funcionar.
docker exec -it clab-ebpf-mqtt-gateway ip link set dev eth1 mtu 1400
sudo ip link set dev switch mtu 1400

# Carregar o programa
docker exec -it clab-ebpf-mqtt-gateway ip link set dev eth1 xdpgeneric obj /xdp_monitor.o sec xdp
```

### C. Execução do Ataque de Inundação (UDP Flood)
Simulação de um ataque de negação de serviço volumétrico para estressar o Gateway e validar a detecção em tempo real.

- Comando de Ataque:

  ```
  # Flood UDP direcionado à porta do Broker MQTT (1883)
  sudo docker exec -it clab-ebpf-mqtt-atacante hping3 --udp -p 1883 --flood 10.0.0.1
  ```

 ### D. Coleta de eMetrics e Validação
Análise do comportamento do sistema sob ataque, comparando métricas de rede e hardware.

- Monitorar Recursos (CPU/RAM):
  ```
  docker exec -it clab-ebpf-mqtt-gateway top
  ```

- Verificar Mapas eBPF (Contagem de Pacotes):

  ```
  # Ver contadores globais por protocolo (UDP/TCP/ICMP)
  docker exec -it clab-ebpf-mqtt-gateway /usr/sbin/bpflist -m proto_stats
  ```
- Teste de Disponibilidade:
  
Enquanto o ataque ocorre, o sensor deve tentar realizar um ping ou publicar uma mensagem MQTT para verificar se o serviço permanece responsivo.


MODIFICAR
Este teste foca na Seção 4.1 do artigo, utilizando a simulação Docker para medir o impacto da detecção precoce na camada XDP.
O próximo estágio incluirá a detecção de ataques de baixa taxa (SlowTT)

