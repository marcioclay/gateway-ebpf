#include <linux/in.h> 
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <bpf/bpf_helpers.h>

/* MAPA 1: Estatísticas por IP (Foco em DDoS Volumétrico)
   Monitora a carga total de cada nó.
*/
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, __u32);   
    __type(value, __u64); 
} ip_stats SEC(".maps");

/* MAPA 2: Métricas de Comportamento (Foco em Slow DoS e MQTT)
   Key 0: ICMP total
   Key 1: UDP total (Flood)
   Key 2: TCP SYN na 1883 (Tentativas de conexão - Slow DoS)
   Key 3: TCP PSH na 1883 (Envio de dados reais/MQTT)
*/
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 4);
    __type(key, __u32);
    __type(value, __u64);
} proto_stats SEC(".maps");

SEC("xdp")
int xdp_monitor_prog(struct xdp_md *ctx) {
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    // --- Camada Ethernet ---
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end) return XDP_PASS;
    if (eth->h_proto != __constant_htons(0x0800)) return XDP_PASS; // ETH_P_IP

    // --- Camada IP ---
    struct iphdr *iph = data + sizeof(struct ethhdr);
    if ((void *)(iph + 1) > data_end) return XDP_PASS;

    __u32 src_ip = iph->saddr;
    __u32 proto_key;
    __u64 *cnt, *ip_cnt;

    // 1. Registro Global por IP (DDoS Detection)
    ip_cnt = bpf_map_lookup_elem(&ip_stats, &src_ip);
    if (ip_cnt) {
        __sync_fetch_and_add(ip_cnt, 1);
    } else {
        __u64 init_val = 1;
        bpf_map_update_elem(&ip_stats, &src_ip, &init_val, BPF_ANY);
    }

    // 2. Lógica de Detecção por Protocolo e Comportamento
    
    // ICMP (1)
    if (iph->protocol == 1) {
        proto_key = 0;
        cnt = bpf_map_lookup_elem(&proto_stats, &proto_key);
        if (cnt) __sync_fetch_and_add(cnt, 1);
    } 
    // UDP (17) - Flood Volumétrico
    else if (iph->protocol == 17) {
        proto_key = 1;
        cnt = bpf_map_lookup_elem(&proto_stats, &proto_key);
        if (cnt) __sync_fetch_and_add(cnt, 1);
    } 
    // TCP (6) - Slow DoS / MQTT
    else if (iph->protocol == 6) {
        struct tcphdr *tcp = (void *)iph + sizeof(struct iphdr);
        if ((void *)(tcp + 1) <= data_end) {
            
            // Filtro porta MQTT (1883)
            if (tcp->dest == __constant_htons(1883)) {
                
                // Se a flag SYN está ativa (Nova tentativa de conexão)
                if (tcp->syn) {
                    proto_key = 2; // Slow DoS Monitoring
                    cnt = bpf_map_lookup_elem(&proto_stats, &proto_key);
                    if (cnt) __sync_fetch_and_add(cnt, 1);
                }
                
                // Se a flag PSH está ativa (Envio de Payload/Mensagem MQTT)
                if (tcp->psh) {
                    proto_key = 3; // Tráfego Útil
                    cnt = bpf_map_lookup_elem(&proto_stats, &proto_key);
                    if (cnt) __sync_fetch_and_add(cnt, 1);
                }
            }
        }
    }

    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
   
