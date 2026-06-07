### Teste com iptables
   
Configuração do Baseline de Defesa (iptables / Netfilter)

Comparação contra o mecanismo eBPF/XDP, o firewall nativo do Linux (iptables) foi configurado como Sistema de Prevenção de Intrusões (IPS). 

Devido às limitações do Netfilter em realizar inspeções de estado focadas na proporção matemática de pacotes/bytes (heurística utilizada no XDP), foram aplicadas as técnicas tradicionais de mitigação recomendadas pela literatura para servidores de borda:

Mitigação de Flood UDP (DDoS): Utilizou-se o módulo limit para restringir o processamento de pacotes UDP na chain INPUT a uma taxa máxima de 10 pacotes por segundo. Qualquer tráfego UDP volumétrico que exceda este limiar é sumariamente descartado (DROP).

Mitigação de Exaustão de Sockets (Slow DoS / TCP 1883): Para impedir que nós atacantes esgotem a tabela de conexões do Broker MQTT, empregou-se o módulo connlimit. A regra restringe a um máximo de 5 conexões simultâneas permitidas por endereço IP de origem na porta 1883. Tentativas de conexão excedentes são rejeitadas preventivamente (REJECT --reject-with tcp-reset).

#### Laboratório
   
regras_iptables.sh está do diretório /lab

Passo 1: Aceder ao Gateway
No terminal do seu servidor (host), entre no contentor do gateway:

```
docker exec -it clab-lab-ebpf-gateway bash
```

Passo 2: Dar permissões de execução

```
chmod +x regras_iptables.sh
```

Passo 3: Executar o script
iniciar o teste comparativo (com o eBPF/XDP desligado), basta correr:

```
./regras_iptables.sh
```

Após o ataque terminar, recolher os dados de pacotes descartados e preencher as tabelas: 
```
iptables -L -n -v
```


#### Como conduzir o Comparativo Prático
Para preencher aquela tabela do ficheiro resultado.md, execute a seguinte rotina de testes:

Passo A: Teste com iptables (XDP Desligado)

- Certifique-se de que o XDP está desligado: 
```
ip link set dev eth1 xdpgeneric off
```

- Execute o script iptables.

- Inicie os seus ataques a partir do node atacante.

- No Gateway, abra o utilitário de sistema htop ou top e observe a % de CPU (especialmente a barra vermelha ou si - Soft Interrupts).
  Anote o valor médio (ex: 85%).

- Pare o ataque e execute iptables -L -n -v. A coluna "pkts" e "bytes" ao lado das regras de DROP vai mostrar exatamente quantos pacotes o iptables conseguiu bloquear.

Passo B: Teste com eBPF/XDP (iptables Zerado)

Zere o iptables: iptables -F

Carregue o seu programa XDP na interface 
```
sudo docker exec -it clab-lab-ebpf-gateway ip link set dev eth1 xdpgeneric pinned /sys/fs/bpf/xdp_monitor_test
```

Inicie os exatos mesmos ataques a partir do node atacante.

Abra o htop no Gateway. A magia acontece aqui: você vai notar que a CPU quase não se mexe (ficará perto de 0% a 5%), mesmo debaixo de um ataque massivo de UDP.

Use o seu Dashboard Python (dashboard.py) para ler quantos pacotes o XDP bloqueou.
