#!/bin/bash

# Interrompe o script se houver algum erro
set -e

echo "=================================================="
echo "1. CONFIGURANDO O GATEWAY (Broker MQTT Mosquitto)"
echo "=================================================="
docker exec clab-lab-ebpf-gateway apt-get update
docker exec clab-lab-ebpf-gateway apt-get install -y mosquitto
# Inicia o broker em segundo plano apontando para o ficheiro de configuração partilhado
docker exec clab-lab-ebpf-gateway mosquitto -d -c /lab/mosquitto.conf

echo "=================================================="
echo "2. CONFIGURANDO O ATACANTE (Python & Paho-MQTT)"
echo "=================================================="
docker exec clab-lab-ebpf-atacante apt-get update
docker exec clab-lab-ebpf-atacante apt-get install -y python3 python3-pip
docker exec clab-lab-ebpf-atacante pip3 install paho-mqtt

echo "=================================================="
echo "3. CONFIGURANDO O SENSOR LEGÍTIMO (Python & Paho-MQTT)"
echo "=================================================="
docker exec clab-lab-ebpf-sensor apt-get update
docker exec clab-lab-ebpf-sensor apt-get install -y python3 python3-pip
docker exec clab-lab-ebpf-sensor pip3 install paho-mqtt

echo "=================================================="
echo "🎉 INFRAESTRUTURA COMPLETA E PRONTA PARA USO!"
echo "=================================================="
