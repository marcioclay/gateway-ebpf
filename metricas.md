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

```

### 2. Simulação de Ataque DDoS Volumétrico (Atacante)

2.1 Execução do UDP Flood via hping3

 - Extração de eMetrics do Artigo para DDoS (Gateway)

3.1 Volumetria de Pacotes no XDP (Taxa PPS)

3.2 Impacto de Recursos de Hardware (Consumo de CPU)

--- 


