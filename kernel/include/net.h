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

/* DNS (assíncrono): 1 resolvido, 0 pendente, -1 falha */
int dns_resolve(const char *host, u32 *ip);

/* ICMP echo */
bool icmp_ping_send(u32 ip, u16 seq);
bool icmp_ping_reply(u16 seq, u64 *rtt_ms);

/* UDP */
typedef void (*udp_handler_t)(u32 src_ip, u16 src_port, const u8 *data, u16 len);
bool udp_bind(u16 port, udp_handler_t h);
bool udp_send(u32 dst_ip, u16 src_port, u16 dst_port, const void *data, u16 len);

/* TCP (sockets não-bloqueantes) */
enum { TCP_CLOSED, TCP_CONNECTING, TCP_OPEN, TCP_PEER_CLOSED, TCP_ERROR };
int  tcp_connect(u32 ip, u16 port);           /* devolve id ou -1 */
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
