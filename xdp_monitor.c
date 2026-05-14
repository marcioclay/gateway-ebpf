#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <bpf/bpf_helpers.h>

/* MAPA 1: Estatísticas por IP (DDoS Volumétrico) */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, __u32);   
    __type(value, __u64); 
} ip_stats SEC(".maps");

/* MAPA 2: Métricas de Comportamento (Artigo + Slow DoS)
   Key 0: ICMP total
   Key 1: UDP total (Flood DDoS)
   Key 2: TCP SYN na 1883 (Início de conexão - Monitoramento Slow)
   Key 3: TCP PSH na 1883 (Tráfego de dados MQTT real)
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

    // Camada L2
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end) return XDP_PASS;
    if (eth->h_proto != __constant_htons(0x0800)) return XDP_PASS; 

    // Camada L3
    struct iphdr *iph = data + sizeof(struct ethhdr);
    if ((void *)(iph + 1) > data_end) return XDP_PASS;

    __u32 src_ip = iph->saddr;
    __u32 proto_key;
    __u64 *cnt, *ip_cnt;

    // Registro por IP para métrica de volume (DDoS)
    ip_cnt = bpf_map_lookup_elem(&ip_stats, &src_ip);
    if (ip_cnt) {
        __sync_fetch_and_add(ip_cnt, 1);
    } else {
        __u64 init_val = 1;
        bpf_map_update_elem(&ip_stats, &src_ip, &init_val, BPF_ANY);
    }

    // Identificação de Protocolos usando valores numéricos diretos (Hardcoded para evitar erros de header)
    
    // 1 = ICMP
    if (iph->protocol == 1) {
        proto_key = 0;
        cnt = bpf_map_lookup_elem(&proto_stats, &proto_key);
        if (cnt) __sync_fetch_and_add(cnt, 1);
    } 
    // 17 = UDP
    else if (iph->protocol == 17) {
        proto_key = 1;
        cnt = bpf_map_lookup_elem(&proto_stats, &proto_key);
        if (cnt) __sync_fetch_and_add(cnt, 1);
    } 
    // 6 = TCP
    else if (iph->protocol == 6) {
        struct tcphdr *tcp = (void *)iph + sizeof(struct iphdr);
        if ((void *)(tcp + 1) <= data_end) {
            
            // Porta MQTT 1883
            if (tcp->dest == __constant_htons(1883)) {
                
                // Flag SYN: Tentativa de conexão (Monitora SlowITe/SlowTT)
                if (tcp->syn) {
                    proto_key = 2;
                    cnt = bpf_map_lookup_elem(&proto_stats, &proto_key);
                    if (cnt) __sync_fetch_and_add(cnt, 1);
                }
                
                // Flag PSH: Envio de dados (Monitora se há atividade real)
                if (tcp->psh) {
                    proto_key = 3;
                    cnt = bpf_map_lookup_elem(&proto_stats, &proto_key);
                    if (cnt) __sync_fetch_and_add(cnt, 1);
                }
            }
        }
    }

    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
