#!/usr/bin/env python3
import subprocess
import json
import time
import struct
import socket

# Nomes dos mapas definidos no seu xdp_monitor.c
MAP_PROTO = "proto_stats"
MAP_SESS = "tcp_sessions"

def get_map_id(name):
    """Descobre o ID do mapa automaticamente pelo nome"""
    try:
        out = subprocess.check_output(["bpftool", "map", "show", "-j"], stderr=subprocess.DEVNULL)
        maps = json.loads(out)
        for m in maps:
            if m.get("name") == name:
                return m.get("id")
    except Exception:
        pass
    return None

def dump_map(map_id):
    """Lê os dados brutos do mapa em formato JSON"""
    if not map_id: return []
    try:
        out = subprocess.check_output(["bpftool", "map", "dump", "id", str(map_id), "-j"], stderr=subprocess.DEVNULL)
        return json.loads(out)
    except Exception:
        return []

def parse_metrics(value_list):
    """Converte os bytes brutos do kernel para inteiros (packet_count e byte_count)"""
    b = bytearray()
    for v in value_list:
        b.append(int(v, 16) if isinstance(v, str) else v)
    
    # A estrutura tem 24 bytes: 8 (pkts) + 8 (bytes) + 8 (last_seen)
    if len(b) >= 16:
        # Extrai os dois primeiros blocos de 64 bits (unsigned long long)
        return struct.unpack("<QQ", b[0:16])
    return 0, 0

def parse_key_idx(key_list):
    """Extrai o índice da chave (0 ou 1)"""
    b = bytearray([int(k, 16) if isinstance(k, str) else k for k in key_list])
    if len(b) >= 4:
        return struct.unpack("<I", b[0:4])[0]
    return -1

def parse_ip(key_list):
    """Converte a chave IP de bytes para string legível (ex: 10.0.0.1)"""
    b = bytearray([int(k, 16) if isinstance(k, str) else k for k in key_list])
    if len(b) >= 4:
        return socket.inet_ntoa(b[0:4])
    return "0.0.0.0"

def main():
    print("[*] Conectando aos mapas eBPF já carregados no Gateway...")
    
    id_proto = get_map_id(MAP_PROTO)
    id_sess = get_map_id(MAP_SESS)

    # Se não achar pelos nomes, tenta usar o ID 55 que você informou como fallback
    if not id_proto and not id_sess:
        print("[-] Mapas não encontrados pelo nome. Tentando ler ID 55...")
        id_proto = 55 

    while True:
        try:
            print("\033c", end="") # Limpa a tela
            agora = time.strftime("%H:%M:%S")
            print("="*65)
            print("  DASHBOARD MQTT GATEWAY (Leitura Direta)")
            print("="*65)
            print(f"  Atualizado em: {agora}\n")

            # --- 1. LER VOLUMETRIA GLOBAL (PROTO_STATS) ---
            dados_proto = dump_map(id_proto)
            pkts_udp = bytes_udp = pkts_tcp = bytes_tcp = 0

            for item in dados_proto:
                key = parse_key_idx(item['key'])
                pkts, bts = parse_metrics(item['value'])
                if key == 0:   # Key 0 = UDP
                    pkts_udp, bytes_udp = pkts, bts
                elif key == 1: # Key 1 = TCP 1883
                    pkts_tcp, bytes_tcp = pkts, bts

            print(" [CENÁRIO 1: DDoS VOLUMÉTRICO - Flood UDP]")
            if pkts_udp > 0:
                print(f"   -> Total Pacotes : {pkts_udp}")
                print(f"   -> Banda Gasta   : {bytes_udp} bytes")
            else:
                print("   -> Sem tráfego detectado.")

            print("\n" + "-"*65 + "\n")

            print(" [CENÁRIO 2: SLOW DoS - Global TCP/1883]")
            if pkts_tcp > 0:
                print(f"   -> Total Pacotes : {pkts_tcp}")
                print(f"   -> Banda Gasta   : {bytes_tcp} bytes")
            else:
                print("   -> Sem tráfego detectado.")

            # --- 2. LER COMPORTAMENTO POR HOST (TCP_SESSIONS) ---
            print("\n" + "-"*65)
            print("    Sessões TCP Ativas por IP de Origem:")
            
            if id_sess:
                dados_sess = dump_map(id_sess)
                if not dados_sess:
                    print("      Nenhuma sessão identificada no momento.")
                for item in dados_sess:
                    ip_str = parse_ip(item['key'])
                    pkts, bts = parse_metrics(item['value'])
                    print(f"      - IP: {ip_str:<15} | Pkts: {pkts:<6} | Bytes: {bts:<8}")
            else:
                print("      (Mapa de sessões comportamentais não localizado)")

            print("\n" + "="*65)
            time.sleep(5) # Atualiza a cada 1 segundo (igual ao watch -n 1)

        except KeyboardInterrupt:
            print("\n[*] Leitura interrompida pelo usuário.")
            break
        except Exception as e:
            print(f"[-] Erro na leitura dos mapas: {e}")
            time.sleep(2)

if __name__ == '__main__':
    main()
