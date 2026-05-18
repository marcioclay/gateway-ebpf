##  📊 Guia de Execução de Testes: Cenário 1 (DDoS Volumétrico)
Este guia orienta a validação do protótipo através do estabelecimento de tráfego legítimo, simulação de ataque de inundação e extração de metricas diretamente do plano de dados.

### Índice de Testes
### 1. Estabelecimento de Tráfego MQTT Legítimo (Sensor)

1.1 Iniciar o Sensor Simulador

```
docker exec -it clab-ebpf-mqtt-sensor python3 /src/sensor.py
```

1.2 Validação de Conexão (CONNECT) e Inscrição (SUBSCRIBE) no Broker 

```
# Verificação da porta MQTT
docker exec -it clab-ebpf-mqtt-gateway ss -tlnp | grep 1883
```

```
# log do Mosquitto no Gateway
docker exec -it clab-ebpf-mqtt-gateway tail -n 20 /var/log/mosquitto/mosquitto.log
```

---

Caso o comando acima não funcione, execute os códigos abaixo.

```
# Encerrar MQTT
docker exec -it clab-ebpf-mqtt-gateway pkill mosquitto
```

```
# Iniciar MQTT (sem a flag -d):
# Terminal deve esta aberto, ele vai mostrar a "tela de log":
docker exec -it clab-ebpf-mqtt-gateway mosquitto -c /etc/mosquitto/mosquitto.conf
```

```
# Em outro terminal execute o sensor.py
docker exec -it clab-ebpf-mqtt-sensor python3 /src/sensor.py
```


### 2. Simulação de Ataque DDoS Volumétrico (Atacante)

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


