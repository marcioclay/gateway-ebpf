## ERRO MTU XDP
Esse erro é um clássico ao trabalhar com eBPF em ambientes de rede virtual (veth), como o Containerlab. O XDP precisa de um espaço extra no cabeçalho do pacote para processamento, e se o MTU (Maximum Transmission Unit) estiver no padrão de 1500, não sobra espaço para os metadados do XDP.

Para resolver, você precisa reduzir o MTU da interface para algo como 1400.

🛠️ Como resolver (Comando Direto)
Execute estes comandos para ajustar o MTU tanto no gateway quanto na bridge:

Bash
# 1. Reduz o MTU dentro do container gateway
docker exec -it clab-ebpf-mqtt-gateway ip link set dev eth1 mtu 1400

# 2. Reduz o MTU na bridge do seu host (importante!)
sudo ip link set dev br-iot mtu 1400
🚀 Tente carregar o programa novamente:
Agora que você liberou espaço nos pacotes, o comando de carregamento deve funcionar:

Bash
docker exec -it clab-ebpf-mqtt-gateway ip link set dev eth1 xdp obj /src/xdp_monitor.o sec xdp
🔍 Se o erro persistir (Modo Genérico)
Alguns drivers de rede virtual não suportam o modo "Native XDP". Se o erro mudar ou continuar, force o modo genérico (SKB mode), que funciona em qualquer interface:

Bash
docker exec -it clab-ebpf-mqtt-gateway ip link set dev eth1 xdpgeneric obj /src/xdp_monitor.o sec xdp
Por que isso acontece?
O XDP reserva um espaço chamado headroom no início do buffer do pacote. Em interfaces virtuais (veth), se o pacote já ocupa 1500 bytes (o padrão da Ethernet), o kernel bloqueia a ativação do XDP para evitar que o pacote exceda o limite físico da interface ao adicionar os metadados do eBPF.

Dica para o Mestrado: No seu texto, você pode mencionar que "foi necessário o ajuste do MTU para 1400 bytes a fim de acomodar o headroom exigido pelo processamento XDP em interfaces veth". Isso mostra domínio técnico sobre a infraestrutura.

Conseguiu anexar agora? Rode um docker exec clab-ebpf-mqtt-gateway ip link show eth1 para confirmar se aparece a flag xdp!
 

 ______________________________________________________________
 1. Cria a bridge chamada br-iot
sudo ip link add name br-iot type bridge

 2. Liga (sobe) a interface da bridge
sudo ip link set dev br-iot up

 1. Derruba a topologia atual
sudo clab destroy -t topologia.yaml --cleanup

2. Sobe a nova versão (ele vai baixar o ping automaticamente desta vez)
sudo clab deploy -t topologia.yaml

1. Atualize a lista de pacotes dentro do gateway
docker exec -it clab-ebpf-mqtt-gateway apt-get update

2. Instale o comando ping (iputils-ping) e ferramentas de rede (iproute2)
docker exec -it clab-ebpf-mqtt-gateway apt-get install -y iputils-ping iproute2
