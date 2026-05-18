#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <bpf/bpf_helpers.h>

#define MQTT_PORT 1883
/* Coleta de dados para DDos(contador volumetrico) e Slow Dos(estado comportamental) */
/* MAPA 1: Tabela de conexões TCP ativas por IP na porta MQTT (Métrica para Slow DoS) */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 2048);
    __type(key, __u32);   // IP de origem
    __type(value, __u64); // Contador de pacotes TCP nesta conexão
} mqtt_sessions SEC(".maps");

/* MAPA 2: Estatísticas globais do plano de dados (metricas do artigo) 
   Key 0: Total pacotes UDP (Foco do DDoS hping3)
   Key 1: Total pacotes TCP MQTT (Foco do Slow DoS)
*/
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 2);
    __type(key, __u32);
    __type(value, __u64);
} proto_stats SEC(".maps");

SEC("xdp")
int xdp_monitor_prog(struct xdp_md *ctx) {
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    // Camada 2: Ethernet
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end) return XDP_PASS;
    if (eth->h_proto != __constant_htons(ETH_P_IP)) return XDP_PASS;

    // Camada 3: IPv4
    struct iphdr *iph = data + sizeof(struct ethhdr);
    if ((void *)(iph + 1) > data_end) return XDP_PASS;

    __u32 src_ip = iph->saddr;
    __u32 stat_key;
    __u64 *cnt;

    // Cenário 1: Identificação de DDoS Volumétrico (UDP Flood)
    if (iph->protocol == 17) { // UDP
        stat_key = 0;
        cnt = bpf_map_lookup_elem(&proto_stats, &stat_key);
        if (cnt) __sync_fetch_and_add(cnt, 1);
        return XDP_PASS;
    }

    // Cenário 2: Identificação de Slow DoS (TCP na porta MQTT)
    if (iph->protocol == 6) { // TCP
        // Calcula o offset do cabeçalho TCP dinamicamente para o verificador
        struct tcphdr *tcp = (void *)iph + (iph->ihl * 4);
        if ((void *)(tcp + 1) > data_end) return XDP_PASS;

        // Filtra apenas o tráfego destinado ao Broker MQTT (Porta 1883)
        if (tcp->dest == __constant_htons(MQTT_PORT)) {
            
            // Incrementa o contador global de pacotes MQTT TCP
            stat_key = 1;
            cnt = bpf_map_lookup_elem(&proto_stats, &stat_key);
            if (cnt) __sync_fetch_and_add(cnt, 1);

            // Rastreia a persistência do IP no mapa de sessões
            __u64 *tcp_cnt = bpf_map_lookup_elem(&mqtt_sessions, &src_ip);
            if (tcp_cnt) {
                __sync_fetch_and_add(tcp_cnt, 1);
            } else {
                __u64 init_val = 1;
                bpf_map_update_elem(&mqtt_sessions, &src_ip, &init_val, BPF_ANY);
            }
        }
    }

    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
