#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#define TARGET_PORT 1883 

// Estrutura rica para análise comportamental avançada
struct host_metrics {
    __u64 packet_count;
    __u64 byte_count;
    __u64 last_seen;    // Timestamp em nanosegundos (essencial para pegar ataques Slow)
};

/* * MAPA 1: Tabela comportamental rica. 
 * O Python usará (packet_count / tempo) para identificar a taxa "Slow".
 */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192); // Expandido para aguentar testes mais longos
    __type(key, __u32);        // IP de origem
    __type(value, struct host_metrics); 
} tcp_sessions SEC(".maps");

/* * MAPA 2: Estatísticas volumétricas globais alteradas para contar PACKETS e BYTES.
 * Key 0: Estatísticas UDP (DDoS Volumétrico)
 * Key 1: Estatísticas TCP porta 1883
 */
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 2);
    __type(key, __u32);
    __type(value, struct host_metrics); // Agora monitora volumetria de banda (bytes) também!
} proto_stats SEC(".maps");

SEC("xdp")
int xdp_monitor_prog(struct xdp_md *ctx) {
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    // 1. Camada 2: Ethernet
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end) return XDP_PASS;
    if (eth->h_proto != bpf_htons(ETH_P_IP)) return XDP_PASS;

    // 2. Camada 3: IP
    struct iphdr *iph = data + sizeof(struct ethhdr);
    if ((void *)(iph + 1) > data_end) return XDP_PASS;

    __u32 src_ip = iph->saddr;
    __u64 pkt_len = data_end - data; // Tamanho total do pacote em bytes (Métrica de Banda)
    __u64 now = bpf_ktime_get_ns();  // Hora exata do evento
    __u32 stat_key;
    struct host_metrics *stats;

    // --- CENÁRIO 1: DDoS Volumétrico (UDP Flood) ---
    if (iph->protocol == 17) { 
        stat_key = 0;
        stats = bpf_map_lookup_elem(&proto_stats, &stat_key);
        if (stats) {
            __sync_fetch_and_add(&stats->packet_count, 1);
            __sync_fetch_and_add(&stats->byte_count, pkt_len);
            stats->last_seen = now;
        }
        return XDP_PASS;
    }

    // --- CENÁRIO 2: Identificação de Slow DoS & Conexões MQTT ---
    if (iph->protocol == 6) { 
        int ip_hdr_len = iph->ihl * 4;
        if (ip_hdr_len < sizeof(struct iphdr)) return XDP_PASS;

        struct tcphdr *tcp = (void *)iph + ip_hdr_len;
        if ((void *)tcp + sizeof(struct tcphdr) > data_end) return XDP_PASS;

        if (tcp->dest == bpf_htons(TARGET_PORT)) {
            
            // A. Volumetria Global TCP (Porta 1883)
            stat_key = 1;
            stats = bpf_map_lookup_elem(&proto_stats, &stat_key);
            if (stats) {
                __sync_fetch_and_add(&stats->packet_count, 1);
                __sync_fetch_and_add(&stats->byte_count, pkt_len);
                stats->last_seen = now;
            }

            // B. Comportamento por Host (A análise fina do SlowITe)
            struct host_metrics *internal_stats = bpf_map_lookup_elem(&tcp_sessions, &src_ip);
            if (internal_stats) {
                __sync_fetch_and_add(&internal_stats->packet_count, 1);
                __sync_fetch_and_add(&internal_stats->byte_count, pkt_len);
                internal_stats->last_seen = now; // Atualiza o último sinal de vida
            } else {
                struct host_metrics new_host = {
                    .packet_count = 1,
                    .byte_count = pkt_len,
                    .last_seen = now
                };
                bpf_map_update_elem(&tcp_sessions, &src_ip, &new_host, BPF_ANY);
            }
        }
    }

    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
