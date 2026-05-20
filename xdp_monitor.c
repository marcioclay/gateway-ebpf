#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h> // Inclui os macros modernos de endianness como bpf_htons

#define TARGET_PORT 80  // Porta do servidor leve simulado (substituindo o MQTT)

/* * MAPA 1: Tabela comportamental de conexões TCP ativas por IP de Origem.
 * Usado para detectar Slow DoS através do rastreamento de IPs persistentes 
 * que mantêm atividade estendida de baixo volume na porta alvo.
 */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 4096);
    __type(key, __u32);   // IP de origem
    __type(value, __u64); // Contador de pacotes TCP por host nesta porta
} tcp_sessions SEC(".maps");

/* * MAPA 2: Estatísticas globais volumétricas do plano de dados (Overhead/Métricas).
 * Key 0: Total de pacotes UDP (Métrica de inundação / DDoS Volumétrico)
 * Key 1: Total de pacotes TCP na porta do serviço alvo (Métrica de Slow DoS)
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

    // 1. Camada 2: Validação do Cabeçalho Ethernet
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end) 
        return XDP_PASS;
        
    // Filtra para processar apenas pacotes IPv4 (usando bpf_htons)
    if (eth->h_proto != bpf_htons(ETH_P_IP)) 
        return XDP_PASS;

    // 2. Camada 3: Validação do Cabeçalho IPv4
    struct iphdr *iph = data + sizeof(struct ethhdr);
    if ((void *)(iph + 1) > data_end) 
        return XDP_PASS;

    __u32 src_ip = iph->saddr;
    __u32 stat_key;
    __u64 *cnt;

    // --- CENÁRIO 1: DDoS Volumétrico (UDP Flood via hping3/iperf3) ---
    if (iph->protocol == 17) { // Protocolo UDP
        stat_key = 0;
        cnt = bpf_map_lookup_elem(&proto_stats, &stat_key);
        if (cnt) {
            __sync_fetch_and_add(cnt, 1);
        }
        return XDP_PASS; // Apenas detecção: permite a passagem do pacote
    }

    // --- CENÁRIO 2: Identificação de Slow DoS (Análise Comportamental TCP) ---
    if (iph->protocol == 6) { // Protocolo TCP
        
        // Proteção do Verificador: Isola o cálculo do tamanho do cabeçalho IP
        int ip_hdr_len = iph->ihl * 4;
        if (ip_hdr_len < sizeof(struct iphdr))
            return XDP_PASS;

        // Calcula a posição do ponteiro TCP de forma segura para o Verificador
        struct tcphdr *tcp = (void *)iph + ip_hdr_len;
        if ((void *)tcp + sizeof(struct tcphdr) > data_end) 
            return XDP_PASS;

        // Filtra o tráfego destinado à porta do serviço monitorado (usando bpf_htons)
        if (tcp->dest == bpf_htons(TARGET_PORT)) {
            
            // A. Incrementa o contador volumétrico global de pacotes TCP no serviço
            stat_key = 1;
            cnt = bpf_map_lookup_elem(&proto_stats, &stat_key);
            if (cnt) {
                __sync_fetch_and_add(cnt, 1);
            }

            // B. Rastreia o comportamento do host (Sessão TCP Ativa) no mapa HASH
            __u64 *tcp_cnt = bpf_map_lookup_elem(&tcp_sessions, &src_ip);
            if (tcp_cnt) {
                // Se o IP já existe, incrementa a atividade dele na sessão
                __sync_fetch_and_add(tcp_cnt, 1);
            } else {
                // Se for um novo IP iniciando conexão, cria o registro inicial
                __u64 init_val = 1;
                bpf_map_update_elem(&tcp_sessions, &src_ip, &init_val, BPF_ANY);
            }
        }
    }

    return XDP_PASS; // Mantém a ação XDP_PASS para focar estritamente em monitoramento
}

char _license[] SEC("license") = "GPL";
