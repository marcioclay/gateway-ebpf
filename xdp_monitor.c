#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <bpf/bpf_helpers.h>

/* Mapa para contabilizar pacotes por IP de origem */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, __u32);   // IP de Origem
    __type(value, __u64); // Contador de Pacotes
} ip_stats SEC(".maps");

/* Mapa para métricas específicas de protocolos */
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 3);
    __type(key, __u32);   // 0: ICMP, 1: UDP_FLOOD, 2: MQTT_TCP
    __type(value, __u64);
} proto_stats SEC(".maps");

SEC("xdp")
int xdp_monitor_prog(struct xdp_md *ctx) {
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end) return XDP_PASS;

    if (eth->h_proto != __constant_htons(ETH_P_IP)) return XDP_PASS;

    struct iphdr *iph = data + sizeof(struct ethhdr);
    if ((void *)(iph + 1) > data_end) return XDP_PASS;

    __u32 src_ip = iph->saddr;
    __u32 proto_key;
    __u64 *cnt, *ip_cnt;

    /* 1. Atualiza estatísticas globais por IP */
    ip_cnt = bpf_map_lookup_elem(&ip_stats, &src_ip);
    if (ip_cnt) {
        __sync_fetch_and_add(ip_cnt, 1);
    } else {
        __u64 init_val = 1;
        bpf_map_update_elem(&ip_stats, &src_ip, &init_val, BPF_ANY);
    }

    /* 2. Detecção de Protocolos (Baseado no Artigo + Adicional MQTT) */
    if (iph->protocol == IPPROTO_ICMP) {
        proto_key = 0;
        cnt = bpf_map_lookup_elem(&proto_stats, &proto_key);
        if (cnt) __sync_fetch_and_add(cnt, 1);
    } 
    else if (iph->protocol == IPPROTO_UDP) {
        proto_key = 1;
        cnt = bpf_map_lookup_elem(&proto_stats, &proto_key);
        if (cnt) __sync_fetch_and_add(cnt, 1);
    } 
    else if (iph->protocol == IPPROTO_TCP) {
        struct tcphdr *tcp = (void *)iph + sizeof(struct iphdr);
        if ((void *)(tcp + 1) <= data_end) {
            // Monitoramento de tráfego destinado ao Broker MQTT (Porta 1883)
            if (tcp->dest == __constant_htons(1883)) {
                proto_key = 2;
                cnt = bpf_map_lookup_elem(&proto_stats, &proto_key);
                if (cnt) __sync_fetch_and_add(cnt, 1);
                
                // Aqui você pode expandir para detectar Slow DoS 
                // analisando flags TCP como SYN/FIN/PSH
            }
        }
    }

    /* Mantemos XDP_PASS para focar apenas na observabilidade/detecção */
    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
