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
