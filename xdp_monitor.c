#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <bpf/bpf_helpers.h>

/* MAPA 1: Contador de pacotes por IP de origem (Métrica principal de DDoS) */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, __u32);   // IP de origem
    __type(value, __u64); // Quantidade de pacotes
} ip_stats SEC(".maps");

/* MAPA 2: Estatísticas globais por protocolo (eMetrics do Artigo) 
   Key 0: Total ICMP
   Key 1: Total UDP (Foco do ataque hping3)
   Key 2: Total TCP
*/
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 3);
    __type(key, __u32);
    __type(value, __u64);
} proto_stats SEC(".maps");

SEC("xdp")
int xdp_monitor_prog(struct xdp_md *ctx) {
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    // L2: Ethernet
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end) return XDP_PASS;
    
    // Filtra apenas IPv4 (0x0800)
    if (eth->h_proto != __constant_htons(ETH_P_IP)) return XDP_PASS;

    // L3: IP
    struct iphdr *iph = data + sizeof(struct ethhdr);
    if ((void *)(iph + 1) > data_end) return XDP_PASS;

    __u32 src_ip = iph->saddr;
    __u32 proto_key;
    __u64 *cnt, *ip_cnt;

    // 1. Atualiza contador por IP (Identifica quem está gerando o volume)
    ip_cnt = bpf_map_lookup_elem(&ip_stats, &src_ip);
    if (ip_cnt) {
        __sync_fetch_and_add(ip_cnt, 1);
    } else {
        __u64 init_val = 1;
        bpf_map_update_elem(&ip_stats, &src_ip, &init_val, BPF_ANY);
    }

    // 2. Classificação por Protocolo (Métricas de inundação)
    if (iph->protocol == 1) {      // ICMP
        proto_key = 0;
    } else if (iph->protocol == 17) { // UDP (Alvo do hping3 no artigo)
        proto_key = 1;
    } else if (iph->protocol == 6) {  // TCP
        proto_key = 2;
    } else {
        return XDP_PASS;
    }

    cnt = bpf_map_lookup_elem(&proto_stats, &proto_key);
    if (cnt) {
        __sync_fetch_and_add(cnt, 1);
    }

    return XDP_PASS; // Apenas monitorando (Detecção), sem descartar ainda
}

char _license[] SEC("license") = "GPL";
