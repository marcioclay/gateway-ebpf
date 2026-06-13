
##  📊 Guia de Execução de Testes: Cenário 1 (DoS Volumétrico)

![Tcpdump](https://img.shields.io/badge/Tcpdump-network%20analysis-orange?style=flat-square&logo=wireshark)
![Hping3](https://img.shields.io/badge/Hping3-packet%20crafting-red?style=flat-square&logo=linux)
![DDoS](https://img.shields.io/badge/DDoS%20%26%20Slow-attack%20simulation-darkred?style=flat-square&logo=apache)
![Cybersecurity](https://img.shields.io/badge/Cybersecurity-lab%20focus-blue?style=flat-square&logo=security)
![IoT](https://img.shields.io/badge/IoT-connected%20devices-lightblue?style=flat-square&logo=internetofthings)


Este guia orienta a validação do protótipo através do estabelecimento de tráfego legítimo, simulação de ataque de inundação e extração de metricas diretamente do plano de dados.

### 1. Métricas para Ataques DoS (Volumétricos)
* **1.1. Taxa de Pacotes por Segundo (PPS - *Packets Per Second*)**
    * *Descrição:* Monitorização de picos súbitos no volume de pacotes recebidos pela interface de rede.
    * *Aplicação:* Identificação de saturação de infraestrutura e ataques de inundação (Flooding).
* **1.2. Consumo de Banda (Mbps / Gbps)**
    * *Descrição:* Volume total de tráfego de dados por unidade de tempo.
    * *Aplicação:* Análise de esgotamento do link de comunicação do gateway.

### Índice de Testes

Ao reiniciar o laboratório o Kernel do Linux é completamente zerado, isto significa que os contêineres foram parados e o programa eBPF foi apagado da memória. Caso esse seja o caso, siga essa etapas: 

```
sudo containerlab deploy -t topologia.yml --reconfigure
```
```
# executar o script para reinstalar o Mosquitto, o Python e a biblioteca do MQTT
./setup.sh
```
```
# 1. Garantir que o diretório de pins antigo seja limpo
sudo docker exec -it clab-lab-ebpf-gateway rm -f /sys/fs/bpf/xdp_monitor_test

# 2. Carregar e pinar o programa de novo
sudo docker exec -it clab-lab-ebpf-gateway bpftool prog load /lab/xdp_monitor.o /sys/fs/bpf/xdp_monitor_test type xdp

# 3. Anexar o filtro à interface eth1
sudo docker exec -it clab-lab-ebpf-gateway ip link set dev eth1 xdpgeneric pinned /sys/fs/bpf/xdp_monitor_test
```

### Passo 1: Ataque Dos

Terminal A -  Janela de Observação

```
# Iniciar dashboard em um terminal
docker exec -it clab-lab-ebpf-gateway python3 /lab/src/dashboard.py
```

Terminal B - Ativar ataque Dos no atacante

```
sudo docker exec -it clab-lab-ebpf-atacante hping3 --udp -p 1883 --flood 10.0.0.1
```


