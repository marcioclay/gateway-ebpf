## 📊 Guia de Testes e Coleta de Métricas
Este guia descreve os procedimentos para validar a sonda de detecção eBPF/XDP e coletar as métricas de desempenho (eMetrics) baseadas no artigo de Tolay, com o adicional de monitoramento do protocolo MQTT.

1. Preparação do Ambiente
Deploy da topologia e carregar o programa eBPF no Gateway.
    ```
    # No host, verifique se os containers estão rodando
    docker ps
    
    # Acesse o nó Vítima(Gateway) para verificar o programa XDP
    docker exec -it clab-ebpf-mqtt-lab-gateway bpftool net list
    ```

2. Cenário de Linha de Base (Baseline)

O objetivo é medir o consumo de recursos com tráfego legítimo antes do ataque.

  A. No Cliente Benigno (client): Inicie uma subscrição MQTT para monitorar a chegada de mensagens.

    ```
    docker exec -it clab-ebpf-mqtt-sensor mosquitto_sub -h 10.0.0.1 -t "sensor/data"
    ```
  B. No Gateway: Monitore o uso de CPU do Gateway.
    
    ```
    docker stats clab-ebpf-mqtt-lab-gateway
    ```
  C. No Atacante (attacker): Envie 10 mensagens legítimas.
    
    ```
      docker exec -it clab-ebpf-mqtt-lab-atacante mosquitto_pub -h 10.0.0.1 -t "sensor/data" -m "status_ok"
    ```
  

  3. Teste de Inundação (DDoS UDP) - Reprodução do Artigo

  Simula o cenário principal do artigo para validar a capacidade de detecção volumétrica.

  A. Inicie o Ataque: No nó atacante, execute um flood UDP na porta do Broker.

    ```
      docker exec -it clab-ebpf-mqtt-lab-atacante hping3 --flood --udp -p 1883 10.0.0.1
    ```

  B. Colete as Métricas:

  CPU: Observe o docker stats no host. O eBPF deve manter o consumo estável mesmo processando milhares de pacotes.

  Detecção: No nó gateway(vítima), verifique os contadores do mapa eBPF:
      ```
      docker exec -it clab-ebpf-mqtt-gateway bpftool map dump name proto_stats
      ```

  C. Disponibilidade e Latência: No nó sensor, realize um ping para a Vítima e observe se há aumento no RTT (Round-Trip Time).

4. Teste de Slow DoS (Adicional de Mestrado)

Simula a exaustão de recursos do Broker MQTT mantendo conexões TCP abertas.

A. Execução do Ataque: No nó atacante, utilize um script ou comando para abrir múltiplas conexões sem enviar o payload completo.

B. Métricas de Observabilidade:

- Verifique no Gateway quantas conexões estão no estado ESTABLISHED mas inativas:

  ```
  docker exec -it clab-ebpf-mqtt-lab-gateway ss -tp | grep 1883
  ```
  
- Consulte o Mapa BPF de IPs para identificar qual endereço está retendo o maior número de pacotes de controle TCP (SYN/ACK).

5. Tabela de Resultados Esperados

  | Cenário              | Taxa de Pacotes (pps) | Uso de CPU (Victim) | Detecção eBPF | Impacto no Cliente   |
|----------------------|-----------------------|---------------------|---------------|----------------------|
| Repouso              | Baixa                 | ~1-2%               | Inativo       | Latência Normal      |
| DDoS (Sem eBPF)      | Alta                  | Elevado (Kernel)    | N/A           | Perda de Pacotes     |
| DDoS (Com eBPF)      | Alta                  | Baixo/Médio         | Sucesso       | Latência Estável     |
| Slow DoS             | Baixa                 | Baixo               | Sucesso       | Conexão Lenta        |

6. Limpeza do Experimento

   Para resetar os contadores dos mapas sem reiniciar o container:
```
   # Exemplo de remoção de elemento do mapa via IP (hexadecimal)
docker exec -it clab-ebpf-mqtt-lab-gateway bpftool map delete name ip_stats key <IP_HEX>
```

Para destruir a topologia:

```
sudo clab destroy -t topologia.yml
```



