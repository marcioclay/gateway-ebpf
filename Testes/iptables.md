##  📊 Guia de Execução de Testes: Cenário Comparativo (Iptables)

![Hping3](https://img.shields.io/badge/Hping3-packet%20crafting-red?style=flat-square&logo=linux)
![DDoS](https://img.shields.io/badge/DDoS%20%26%20Slow-attack%20simulation-darkred?style=flat-square&logo=apache)
![Cybersecurity](https://img.shields.io/badge/Cybersecurity-lab%20focus-blue?style=flat-square&logo=security)
![IoT](https://img.shields.io/badge/IoT-connected%20devices-lightblue?style=flat-square&logo=internetofthings)
![Iptables](https://img.shields.io/badge/Iptables-firewall%20rules-orange?style=flat-square&logo=linux)

### Teste com iptables
   
Configuração do Baseline de Defesa 

Comparação contra o mecanismo eBPF/XDP, o firewall nativo do Linux (iptables) foi configurado como Sistema de Prevenção de Intrusões (IPS). 

Devido às limitações do Netfilter em realizar inspeções de estado focadas na proporção matemática de pacotes/bytes (heurística utilizada no XDP), foram aplicadas as técnicas tradicionais de mitigação recomendadas pela literatura para servidores de borda:

- Mitigação de Flood UDP (DDoS): Utilizou-se o módulo limit para restringir o processamento de pacotes UDP na chain INPUT a uma taxa máxima de 10 pacotes por segundo. Qualquer tráfego UDP volumétrico que exceda este limiar é sumariamente descartado (DROP).

- Mitigação de Exaustão de Sockets (Slow DoS / TCP 1883): Para impedir que nós atacantes esgotem a tabela de conexões do Broker MQTT, empregou-se o módulo connlimit. A regra restringe a um máximo de 5 conexões simultâneas permitidas por endereço IP de origem na porta 1883. Tentativas de conexão excedentes são rejeitadas preventivamente (REJECT --reject-with tcp-reset).

#### Laboratório

Passo A: Teste com iptables (XDP Desligado)

- Certifique-se de que o XDP está desligado: 

```
sudo docker exec -it clab-lab-ebpf-gateway ip link set dev eth1 xdpgeneric off
```

- Aplique as regras do iptables.
```
# Acessar o gateway
 docker exec -it clab-lab-ebpf-gateway bash
```
```
# Executar o script
cd /lab
./regras_iptables.sh
```

- Inicie os seus ataques a partir do node atacante.

- No Gateway, abra o utilitário de sistema htop ou top e observe a % de CPU. (especialmente a barra vermelha ou si - Soft Interrupts). Anote o valor médio (ex: 85%).

- Pare o ataque e execute:
   ```
   # Analisar a coluna "pkts" e "bytes" ao lado das regras de DROP - pacotes bloqueados
      iptables -L -n -v
   ```

 Passo B: Teste com eBPF/XDP (iptables Zerado)
 
- Zere o iptables:
   ```
     iptables -F
   ```

- Carregue o XDP na interface eth1
  ```
   sudo docker exec -it clab-lab-ebpf-gateway ip link set dev eth1 xdpgeneric pinned /sys/fs/bpf/xdp_monitor_test
  ```

- Inicie os mesmos ataques a partir do node atacante.

Abra o htop no Gateway e monitore a CPU.

- Use o seu Dashboard
   ```
   # quantidade de pacotes bloqueados
   dashboard.py
   ``` 
 

