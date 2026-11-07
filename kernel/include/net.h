/* mOS — pilha de rede: driver Ethernet, IPv4, ARP, ICMP, UDP, DHCP, DNS, TCP, HTTP(S)
 * Toda a API é não-bloqueante e baseada em polling (net_poll() é chamado pelo loop principal). */
#pragma once
#include "kernel.h"

typedef struct {
    u8  mac[6];
    u32 ip, mask, gw, dns;      /* ordem de host: 10.0.2.15 = 0x0A00020F */
    bool link, configured;
    u64 rx_packets, tx_packets, rx_bytes, tx_bytes;
    char driver[48];
} net_info_t;

net_info_t *net_info(void);
void net_dhcp_renew(void);
void ip_to_str(u32 ip, char *buf);           /* "10.0.2.15" */
bool str_to_ip(const char *s, u32 *ip);

/* driver de placa: registrado pelo driver (e1000, rtl8139, virtio-net...) */
typedef struct {
    const char *name;
    bool (*send)(const void *frame, u16 len);
    int  (*recv)(void *frame, u16 max);       /* 0 se nada pendente */
    bool (*link_up)(void);
    u8   mac[6];
} netdev_t;
void net_register_device(netdev_t *d);
void net_set_active(netdev_t *d);          /* força a interface ativa (renova o DHCP) */
int net_device_count(void);
netdev_t *net_device_at(int i);

/* trava da pilha (net.c): quem chama tcp_* ou dns_* fora do laço da área de trabalho segura ela */
extern spinlock_t g_net_lock;

/* IPv6 (net/ipv6.c). Endereços de 16 bytes em ordem de rede; o IPv4 aparece como ::ffff:a.b.c.d (mapeado). */
typedef struct {
    u8 ll[16];            /* fe80::/64 + EUI-64 do MAC */
    u8 global[16];        /* SLAAC: prefixo do RA + EUI-64 */
    u8 router[16];        /* roteador padrão (link-local, do RA) */
    u8 dns[16];           /* RDNSS do RA */
    u8 prefix_len;
    bool ll_ok, global_ok, has_router, dns_ok, ra_seen;
    u64 router_until;
} net6_info_t;
net6_info_t *net6_info(void);
extern const u8 IP6_ANY[16], IP6_LOOPBACK[16];
void ip6_to_str(const u8 *a, char *buf);     /* buf: 46 bytes */
bool str_to_ip6(const char *s, u8 *out);
void ip6_from_v4(u32 ip, u8 *out);
bool ip6_is_v4mapped(const u8 *a);
u32  ip6_to_v4(const u8 *a);
bool ip6_eq(const u8 *a, const u8 *b);
bool ip6_is_any(const u8 *a);
bool ip6_is_local(const u8 *a);              /* 127.x, ::1 ou um endereço desta máquina */
bool ip6_has_global_route(void);             /* endereço global + roteador padrão */

/* DNS (assíncrono): 1 resolvido, 0 pendente, -1 falha */
int dns_resolve(const char *host, u32 *ip);
int dns_resolve6(const char *host, u8 *ip16);  /* registros AAAA */

/* ICMP echo */
bool icmp_ping_send(u32 ip, u16 seq);
bool icmp_ping_reply(u16 seq, u64 *rtt_ms);
/* eco de qualquer família montado por quem chama (ping/ping6 dos apps); respostas ficam num anel por identificador */
bool icmp_echo_send(const u8 *dst16, const void *msg, u16 len);
int  icmp_echo_recv(u16 id, u8 *from16, void *buf, int max);

/* UDP */
typedef void (*udp_handler_t)(u32 src_ip, u16 src_port, const u8 *data, u16 len);
bool udp_bind(u16 port, udp_handler_t h);
bool udp_send(u32 dst_ip, u16 src_port, u16 dst_port, const void *data, u16 len);

/* TCP (sockets não-bloqueantes) */
enum { TCP_CLOSED, TCP_CONNECTING, TCP_OPEN, TCP_PEER_CLOSED, TCP_ERROR };
int  tcp_connect(u32 ip, u16 port);           /* devolve id ou -1 */
int  tcp_connect6(const u8 *ip16, u16 port);  /* qualquer família (v4 mapeado); -1 sem rota, -2 recusada (local sem escuta) */
/* abertura passiva: ouvintes têm ids a partir de TCP_LISTEN_BASE */
#define TCP_LISTEN_BASE 0x20000
#define TCP_L_V4   1      /* aceita IPv4 (mapeado) */
#define TCP_L_V6   2      /* aceita IPv6 */
int  tcp_listen(const u8 *lip16, u16 port, int backlog, int fam_mask);  /* lip16 NULL/:: = qualquer; porta 0 = efêmera; -1 erro, -2 em uso */
int  tcp_accept(int l);                       /* conexão pronta ou -1 */
int  tcp_pending(int l);                      /* conexões prontas para accept */
bool tcp_endpoints(int s, u8 *lip16, u16 *lport, u8 *rip16, u16 *rport);
int  tcp_state(int s);
int  tcp_send(int s, const void *data, int len);  /* bytes aceitos */
int  tcp_recv(int s, void *buf, int max);     /* bytes lidos; 0 nada; -1 fechado e vazio */
void tcp_close(int s);
int  tcp_avail(int s);                          /* bytes recebidos esperando leitura */
/* "sockets de memória" (net/usersock.c): ids >= TCP_MEM_BASE não são TCP, são os canais do TLS dos apps
 * (SecureTransport do libSystem). tcp_state/send/recv desviam para eles. */
#define TCP_MEM_BASE 0x10000
int  memsock_state(int s);
int  memsock_send(int s, const void *data, int len);
int  memsock_recv(int s, void *buf, int max);

/* TLS 1.3 sobre TCP (cliente) */
typedef struct tls_conn tls_conn_t;
tls_conn_t *tls_new(int tcp_sock, const char *sni);
int  tls_handshake(tls_conn_t *c);            /* 1 pronto, 0 pendente, -1 erro */
int  tls_send(tls_conn_t *c, const void *data, int len);
int  tls_recv(tls_conn_t *c, void *buf, int max); /* como tcp_recv */
void tls_free(tls_conn_t *c);
const char *tls_error(tls_conn_t *c);

/* HTTP/1.1 cliente assíncrono (http:// e https://) */
typedef struct http_req {
    char url[512];
    int status;               /* código HTTP */
    char content_type[96];
    char location[512];       /* redirecionamento final seguido */
    u8 *body;
    u32 body_len;
    char error[128];
    int state;                /* interno */
    void *priv;
} http_req_t;
http_req_t *http_get(const char *url);
http_req_t *http_post(const char *url, const char *content_type, const void *body, u32 len);
int  http_poll(http_req_t *r);                /* 0 em andamento, 1 concluído, -1 erro */
void http_free(http_req_t *r);
float http_progress(http_req_t *r);           /* 0..1 (estimado) */
