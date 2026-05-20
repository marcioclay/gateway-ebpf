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
3. 
Quando você está com o terminal "preso" dentro do container e quer voltar para o seu terminal normal do WSL2:

Sair e manter o container rodando (Mais comum):
Digite apenas exit ou aperte as teclas Ctrl + D.
Isso fecha a sua sessão de visualização, mas os serviços e as interfaces de rede do container continuam funcionando normalmente no fundo.

3. Executar Comandos lá Dentro (Sem precisar Entrar)
Você não precisa necessariamente "entrar" no container para rodar comandos ou scripts. Você pode "injetar" o comando diretamente do seu host. Isso é excelente para automação.

Estrutura básica:

Bash
sudo docker exec -it <nome-do-container> <comando>
Exemplos práticos para o seu cenário:
Verificar as interfaces de rede de dentro do gateway:

Bash
sudo docker exec -it clab-gateway-ebpf-gateway ip addr
Instalar pacotes dentro do gateway (Alpine):

Bash
sudo docker exec -it clab-gateway-ebpf-gateway apk add bpftool
Ver o status do eBPF dentro do gateway (quando compartilhado):

Bash
sudo docker exec -it clab-gateway-ebpf-gateway bpftool prog show
