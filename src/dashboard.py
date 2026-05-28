#!/usr/bin/env python3
import os
import sys
import time
import json
import subprocess
import socket
import struct

# Caminhos dos mapas pinados pelo bpftool
MAP_PROTO_STATS = "/sys/fs/bpf/my_lab/proto_stats"
MAP_TCP_SESSIONS = "/sys/fs/bpf/my_lab/tcp_sessions"

def int_to_ip(ip_int):
    """Converte o inteiro do IP vindo do eBPF para string legível (A.B.C.D)"""
    try:
        # Trata o Endianness (geralmente Little Endian na leitura do bpftool em x86)
        return socket.inet_ntoa(struct.pack("<I", ip_int))
    except Exception:
        return "Desconhecido"

def ler_mapa_bpf(caminho_mapa):
    """Executa o bpftool para extrair o mapa estruturado em formato JSON"""
    if not os.path.exists(caminho_mapa):
        return None
    try:
        cmd = ["bpftool", "-j", "map", "dump", "pinned", caminho_mapa]
        resultado = subprocess.run(cmd, capture_output=True, text=True, check=True)
        return json.loads(resultado.stdout)
    except Exception as e:
        return None

def main():
    # Dicionários para armazenar o estado anterior e calcular taxas por segundo (Delta)
    prev_global = {}
    prev_hosts = {}
    last_time = time.time()

    print("=== Aguardando inicialização dos mapas eBPF em /sys/fs/bpf/my_lab/... ===")
    
    while True:
        try:
            time.sleep(1.0)
            now_time = time.time()
            dt = now_time - last_time
            if dt <= 0:
                continue
            
            stats_globais = ler_mapa_bpf(MAP_PROTO_STATS)
            stats_hosts = ler_mapa_bpf(MAP_TCP_SESSIONS)
            
            if stats_globais is None:
                continue

            # Limpa a tela para o efeito de Dashboard em tempo real
            os.system('clear')
            print("=========================================================================")
            print("            DASHBOARD DE MONITORAMENTO IOT - GATEWAY eBPF/XDP            ")
            print("=========================================================================")
            print(f"Tempo de Execução: {time.strftime('%H:%M:%S')} | Janela: {dt:.2f}s\n")

            # --- 1. PROCESSAMENTO DE MÉTRICAS GLOBAIS (DDoS Volumétrico) ---
            print("[ MÁSTRICAS GLOBAIS DO PLANO DE DADOS ]")
            print(f"{'Protocolo/Serviço':<25} | {'Pacotes Totais':<15} | {'Taxa (PPS)':<12} | {'Banda (Mbps)':<12}")
            print("-" * 73)

            for item in stats_globais:
                key = item.get("key")
                val = item.get("value", {})
                
                pkt_total = val.get("packet_count", 0)
                bytes_total = val.get("byte_count", 0)
                
                label = "UDP (DDoS Flood Target)" if key == 0 else "TCP (Porta 1883 - MQTT)"
                
                # Cálculos de taxa (Delta)
                old_pkt, old_bytes = prev_global.get(key, (pkt_total, bytes_total))
                pps = (pkt_total - old_pkt) / dt
                mbps = (((bytes_total - old_bytes) * 8) / 1024 / 1024) / dt
                
                prev_global[key] = (pkt_total, bytes_total)
                
                print(f"{label:<25} | {pkt_total:<15} | {pps:<12.2f} | {mbps:<12.4f}")
            
            print("\n" + "="*73 + "\n")

            # --- 2. PROCESSAMENTO COMPORTAMENTAL POR HOST (Detecção de Slow DoS) ---
            print("[ ANÁLISE COMPORTAMENTAL DE SESSÕES TCP (PORTA 1883) ]")
            print(f"{'IP Origem':<16} | {'Pacotes':<8} | {'Volume (Bytes)':<15} | {'Tam. Médio':<10} | {'Status/Alerta':<15}")
            print("-" * 73)

            if not stats_hosts:
                print("Nenhuma sessão TCP ativa mapeada pelo Kernel no momento.")
            else:
                for host in stats_hosts:
                    raw_ip = host.get("key")
                    # No JSON do bpftool, chaves de mapas Hash podem vir como inteiros ou sub-objetos
                    ip_int = int(raw_ip) if isinstance(raw_ip, (int, str)) else raw_ip.get("value", 0)
                    
                    ip_str = int_to_ip(ip_int)
                    val = host.get("value", {})
                    
                    pkts = val.get("packet_count", 0)
                    bytes_cnt = val.get("byte_count", 0)
                    
                    # Ignora IPs zerados de controle
                    if ip_str == "0.0.0.0":
                        continue

                    # Métrica de assinatura: Tamanho médio do pacote (Payload + Headers)
                    avg_packet_size = bytes_cnt / pkts if pkts > 0 else 0
                    
                    # HEURÍSTICA DE DETECÇÃO (Nível de Dissertação):
                    # Se o host enviou pacotes, mas o tamanho médio é muito baixo (ex: < 70 bytes,
                    # o que significa apenas a troca de flags TCP SYN/ACK/FIN sem payload MQTT real)
                    # E mantém a atividade insistente, ele entra como suspeito de Slow DoS.
                    status = "LEGÍTIMO"
                    if pkts > 5 and avg_packet_size < 74:
                        status = "⚠️ SUSPEITO SLOW"
                    elif pps > 5000: # Se um único IP estourar pacotes sozinho
                        status = "🚨 ATACANTE DDoS"

                    print(f"{ip_str:<16} | {pkts:<8} | {bytes_cnt:<15} | {avg_packet_size:<10.1f} | {status:<15}")

            last_time = now_time

        except KeyboardInterrupt:
            print("\nEncerrando Dashboard...")
            sys.exit(0)
        except Exception as e:
            print(f"Erro no loop do Dashboard: {e}")
            time.sleep(2)

if __name__ == "__main__":
    main()
