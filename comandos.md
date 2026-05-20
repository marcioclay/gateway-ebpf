## 🚀 Ciclo de Vida Principal (Gerenciamento)

Estes são os comandos que você usará 90% do tempo para colocar o laboratório de pé ou desligá-lo.

```
sudo containerlab deploy -t <arquivo.yml>
```

O que faz: Lê o arquivo de topologia, baixa as imagens Docker necessárias, cria as conexões de rede (veth pairs, bridges) e inicia os nós.

Dica: Use a flag --reconfigure para forçar a recriação dos containers caso tenha feito alterações no YAML.
```
sudo containerlab destroy -t <arquivo.yml>
```
O que faz: Remove todos os containers criados por aquela topologia e limpa as interfaces de rede do host.

Dica: Use a flag --cleanup (ou -a) para garantir uma limpeza profunda e evitar "fantasmas" de rede travando portas do seu host.
## 🔍 Inspeção e Diagnóstico

Para entender o que está acontecendo com os nós após o deploy.

```
sudo containerlab inspect -t <arquivo.yml>
```

O que faz: Mostra uma tabela resumida com o status de todos os nós (se estão rodando), os nomes dos containers, os endereços IP de gerência e as imagens utilizadas.

```
sudo containerlab graph -t <arquivo.yml>
```

O que faz: Sobe um servidor web local temporário (geralmente na porta :5000) com uma interface gráfica interativa exibindo o desenho da sua topologia de rede. Excelente para relatórios ou apresentações.

## 💾 Salvamento e utilitários

```
sudo containerlab save -t <arquivo.yml>
```
O que faz: Captura as configurações ativas que você fez "a quente" dentro dos nós e tenta salvá-las no diretório do laboratório (muito usado com nós de caixas de roteadores tradicionais como Nokia SR OS, Cisco, Arista).
```
containerlab version
```
O que faz: Mostra a versão instalada do Containerlab e verifica se há atualizations disponíveis.

--- 

## 1. Comandos para Acessar o Container (Entrar)

Para interagir com o terminal de dentro de um nó do Containerlab, você usa o Docker (já que o Containerlab usa containers Docker por baixo dos panos).

Acessar via terminal interativo (Bash/Sh):

```
sudo docker exec -it <nome-do-container> bash
```

Se o container for muito leve e não tiver bash (como algumas imagens Alpine), use sh:

```
sudo docker exec -it <nome-do-container> sh
```

Exemplo real no seu lab: 
```
sudo docker exec -it clab-gateway-ebpf-gateway sh
```

2. Comandos para Sair do Container
   
Quando você está com o terminal "preso" dentro do container e quer voltar para o seu terminal normal do WSL2:

Sair e manter o container rodando (Mais comum):
Digite apenas exit ou aperte as teclas Ctrl + D.
Isso fecha a sua sessão de visualização, mas os serviços e as interfaces de rede do container continuam funcionando normalmente no fundo.

3. Executar Comandos lá Dentro (Sem precisar Entrar)
Você não precisa necessariamente "entrar" no container para rodar comandos ou scripts. Você pode "injetar" o comando diretamente do seu host. Isso é excelente para automação.

Estrutura básica:

```
sudo docker exec -it <nome-do-container> <comando>
```
Exemplos práticos para o seu cenário:
Verificar as interfaces de rede de dentro do gateway:

```
sudo docker exec -it clab-gateway-ebpf-gateway ip addr
```
Instalar pacotes dentro do gateway (Alpine):

Bash
```
sudo docker exec -it clab-gateway-ebpf-gateway apk add bpftool
```
Ver o status do eBPF dentro do gateway (quando compartilhado):

Bash
```
sudo docker exec -it clab-gateway-ebpf-gateway bpftool prog show
```

### 🔀 Host vs Container: O que acontece em cada lugar?

Para clarear de vez onde rodar cada comando no seu projeto atual, veja este resumo prático:

| Ação | Onde Executar? | Comando Exemplo |
| :--- | :--- | :--- |
| **Compilar** o código C do eBPF (`clang`) | **Host** (WSL2) | `clang -O2 -target bpf -c xdp_monitor.c -o xdp_monitor.o` |
| **Subir/Derrubar** o laboratório | **Host** (WSL2) | `sudo containerlab deploy -t topologia.yml` |
| **Carregar o XDP** (Bypass de travas) | **Host** (WSL2) | `sudo ip link set dev vethxxxx xdpgeneric object ./xdp_monitor.o section xdp` |
| **Gerar tráfego** de ataque (MQTT/Ping) | **Dentro** do Atacante | `sudo docker exec -it clab-gateway-ebpf-atacante ping 10.0.0.1` |
| **Inspecionar logs/métricas** locais | **Dentro** do Gateway | `sudo docker exec -it clab-gateway-ebpf-gateway tcpdump -i eth1` |
| **Monitorar mapas** com `bpftool` | **Host** (WSL2) | `sudo bpftool map dump id <ID_DO_MAPA>` |


O comando docker exec funciona como um "portal": ele pega um comando que você digitou no Host, atravessa a barreira de isolamento do Container, executa lá dentro no namespace de rede correto e devolve o resultado na sua tela do WSL2. 

--- 

## 1. O Comando para Listar as Interfaces
Execute este comando no terminal do seu host:

```
sudo docker exec -it clab-gateway-ebpf-gateway ip --brief addr show
```
Por que usar a flag --brief? Ela limpa a saída poluída do Linux e te mostra um resumo direto: o nome da interface, o status (UP ou DOWN) e o endereço IP associado. Excelente para colocar em relatórios!

## 2. O que você deve esperar ver na saída (Análise da Topologia)
Ao rodar o comando, o Containerlab terá estruturado o seu gateway com as seguintes interfaces padrão: 


### 🌐 Interfaces de Rede do Gateway no Containerlab

| Interface | Tipo / Função | Descrição Técnica |
| :--- | :--- | :--- |
| **`lo`** | Loopback | Interface interna local (`127.0.0.1`). Padrão de isolamento de qualquer sistema operacional Linux. |
| **`eth0`** | Gerência (Management) | Criada automaticamente pelo Docker/Containerlab. Possui um IP da rede interna do Docker (ex: `172.20.20.x`) e serve para o Host se comunicar com o container, além de prover acesso à internet (NAT). |
| **`eth1`** *(ou eth2...)* | Link de Dados (Data Plane) | **A interface mais importante para o projeto.** É a ponta interna do par `veth` que o Containerlab conectou à topologia, interligando o Gateway aos outros nós (como Atacante ou Sensor) para tráfego de dados. |


3. Como correlacionar com o Host (Para o eBPF)
Se você precisar descobrir qual é o nome "verdadeiro" da interface no Host para carregar seu programa XDP (aquele comando sudo ip link set dev veth...), você pode rodar:

```
sudo docker exec -it clab-gateway-ebpf-gateway ip -d link show eth1
```

--- 

## Purga Completa do Ambiente (Reset Total)
Script sequencial para desalocar buffers do Kernel, destruir topologias órfãs, limpar pontes virtuais e efetuar o reset completo do motor Docker.

# 1. Destruição da topologia Containerlab e limpeza de veths associadas
```
sudo containerlab destroy -t topologia.yml --cleanup
```

# 2. Exclusão de logs residuais de infraestrutura
```
sudo rm -rf clab-gateway-ebpf/
```

# 3. Purga completa de binários do Containerlab do sistema
```
sudo containerlab version purge
sudo rm -f /usr/local/bin/containerlab
sudo rm -rf /etc/containerlab/
```

# 4. Paragem e expurgo de infraestrutura Docker (Containers, Redes e Caches)
```
sudo docker stop $(sudo docker ps -aq) 2>/dev/null
sudo docker system prune -a --volumes -f
```

# 5. Flush das tabelas de interfaces virtuais do Kernel do Host e desmontagem do BPF FS
```
sudo ip link flush type veth
sudo umount /sys/fs/bpf 2>/dev/null
```







