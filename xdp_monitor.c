#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/in.h> 
#include <bpf/bpf_helpers.h>


/* Mapa para contabilizar pacotes por IP de origem.
   Utilizado para identificar o nó atacante (10.0.0.10) vs sensor (10.0.0.20).
*/
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, __u32);   // IP de Origem
    __type(value, __u64); // Contador de Pacotes
} ip_stats SEC(".maps");

/* Mapa para métricas específicas de protocolos conforme o artigo.
   Key 0: ICMP (Ping)
   Key 1: UDP (Flood)
   Key 2: MQTT_TCP (Porta 1883)
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

    // 1. Camada Ethernet
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end) return XDP_PASS;

    // Filtrar apenas tráfego IPv4
    if (eth->h_proto != __constant_htons(ETH_P_IP)) return XDP_PASS;

    // 2. Camada IP
    struct iphdr *iph = data + sizeof(struct ethhdr);
    if ((void *)(iph + 1) > data_end) return XDP_PASS;

    __u32 src_ip = iph->saddr;
    __u32 proto_key;
    __u64 *cnt, *ip_cnt;

    // Atualiza estatísticas globais por IP de origem (Métrica de fluxo)
    ip_cnt = bpf_map_lookup_elem(&ip_stats, &src_ip);
    if (ip_cnt) {
        __sync_fetch_and_add(ip_cnt, 1);
    } else {
        __u64 init_val = 1;
        bpf_map_update_elem(&ip_stats, &src_ip, &init_val, BPF_ANY);
    }

    // 3. Verificação de Protocolos L4
    
    // ICMP (Protocolo 1)
    if (iph->protocol == 1) {
        proto_key = 0;
        cnt = bpf_map_lookup_elem(&proto_stats, &proto_key);
        if (cnt) __sync_fetch_and_add(cnt, 1);
    } 
    // UDP (Protocolo 17) - Foco no artigo (DDoS Flood)
    else if (iph->protocol == 17) {
        proto_key = 1;
        cnt = bpf_map_lookup_elem(&proto_stats, &proto_key);
        if (cnt) __sync_fetch_and_add(cnt, 1);
    } 
    // TCP (Protocolo 6) - Foco no adicional de Mestrado (MQTT/Slow DoS)
    else if (iph->protocol == 6) {
        struct tcphdr *tcp = (void *)iph + sizeof(struct iphdr);
        if ((void *)(tcp + 1) <= data_end) {
            // Monitoramento específico da porta MQTT (1883)
            if (tcp->dest == __constant_htons(1883)) {
                proto_key = 2;
                cnt = bpf_map_lookup_elem(&proto_stats, &proto_key);
                if (cnt) __sync_fetch_and_add(cnt, 1);
            }
        }
    }

    /* Retornamos XDP_PASS para que o pacote continue para a pilha do Linux.
       Isso permite medir o overhead de monitoramento sem bloquear o tráfego.
    */
    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";   
