
##  📊 Guia de Execução de Testes: Cenário 1 (DDoS Volumétrico)

![Tcpdump](https://img.shields.io/badge/Tcpdump-network%20analysis-orange?style=flat-square&logo=wireshark)
![Hping3](https://img.shields.io/badge/Hping3-packet%20crafting-red?style=flat-square&logo=linux)
![DDoS](https://img.shields.io/badge/DDoS%20%26%20Slow-attack%20simulation-darkred?style=flat-square&logo=apache)
![Cybersecurity](https://img.shields.io/badge/Cybersecurity-lab%20focus-blue?style=flat-square&logo=security)
![IoT](https://img.shields.io/badge/IoT-connected%20devices-lightblue?style=flat-square&logo=internetofthings)


Este guia orienta a validação do protótipo através do estabelecimento de tráfego legítimo, simulação de ataque de inundação e extração de metricas diretamente do plano de dados.

### Índice de Testes

### Passo 1: Inicializar a Infraestrutura de Rede

```
sudo containerlab deploy -t topologia.yml
```

Passo 2: Terminal A -  Monitoramento eBPF (ddos.py)

```
sudo docker exec -it -w /metricas clab-gateway-ebpf-gateway python3 ddos.py
```

Passo 3: Terminal B - Visualizar Inscrição (Subscription) MQTT

```
# Instalar MQTT
sudo docker exec -it clab-gateway-ebpf-gateway apk add --no-cache mosquitto-clients
```

```
sudo docker exec -it clab-gateway-ebpf-gateway mosquitto_sub -h 127.0.0.1 -t "v1/dispositivos/#" -v
```

Passo 4: Terminal C (Cliente/Sensor) - Iniciar Tráfego Legítimo
```
sudo docker exec -it -w /metricas clab-gateway-ebpf-cliente python3 sensor_mqtt.py
```

Passo 5: Terminal D (Atacante) - Disparar o Ataque DDoS Volumétrico
```
sudo docker exec -it clab-gateway-ebpf-atacante hping3 --udp --flood -p 80 -I eth1 10.0.0.1
```



### 1. Simulação de Ataque DDoS Volumétrico (Atacante)



2.1 Execução do UDP Flood via hping3

```
Instalar hping3
docker exec -it clab-ebpf-mqtt-atacante apk add --no-cache hping3
```

```
# Execução ataque DDos
sudo docker exec -it clab-ebpf-mqtt-atacante hping3 --udp -p 1883 --flood 10.0.0.1
```

 3. Extração de metricas do artigo para DDoS (Gateway)

3.1 Volumetria de Pacotes no XDP (Taxa PPS)

```
# Pacotes por segundo
docker exec -it clab-ebpf-mqtt-gateway ip -s link show eth1
```

3.2 Impacto de Recursos de Hardware (Consumo de CPU)

```
docker exec -it clab-ebpf-mqtt-gateway top
```

4. Resultado do teste 1 (DDoS Volumétrico - Apenas Monitoramento)

Durante o ataque de inundação UDP de alto volume (`hping3 --flood`), o comportamento de hardware do Gateway IoT apresentou alta estabilidade, validando a eficiência do eBPF em processar pacotes em baixo nível:

- **Consumo de CPU no Kernel (System):** ~8.4%
- **Consumo de CPU no Usuário (User):** ~2.9%
- **Capacidade Ociosa do Sistema (Idle):** ~82.9%

--- 
