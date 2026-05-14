# 1. Cria a bridge chamada br-iot
sudo ip link add name br-iot type bridge

# 2. Liga (sobe) a interface da bridge
sudo ip link set dev br-iot up
