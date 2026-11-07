/* mOS — definições internas da pilha de rede (não exportadas para apps) */
#pragma once
#include "net.h"

/* ---------- ordem de bytes ---------- */
static inline u16 bswap16(u16 v) { return (u16)((v >> 8) | (v << 8)); }
static inline u32 bswap32(u32 v) { return __builtin_bswap32(v); }
#define htons(x) bswap16((u16)(x))
#define ntohs(x) bswap16((u16)(x))
#define htonl(x) bswap32((u32)(x))
#define ntohl(x) bswap32((u32)(x))

static inline u16 rd16(const u8 *p) { return (u16)((p[0] << 8) | p[1]); }
static inline u32 rd32(const u8 *p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }
static inline void wr16(u8 *p, u16 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
static inline void wr32(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }

/* ---------- constantes ---------- */
#define ETH_HLEN      14
#define ETH_MTU       1500
#define ETH_FRAME_MAX 1518
#define ETH_P_IP      0x0800
#define ETH_P_ARP     0x0806
#define ETH_P_IPV6    0x86DD
#define IP6_HLEN      40
#define IPPROTO_ICMPV6 58
#define IP_HLEN       20
#define IPPROTO_ICMP  1
#define IPPROTO_TCP   6
#define IPPROTO_UDP   17
#define IP_BROADCAST  0xFFFFFFFFu

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DNS_PORT         53

/* ---------- estado global (net.c) ---------- */
extern net_info_t g_net;
extern netdev_t  *g_netdev;

/* núcleo */
u32  inet_csum_add(u32 sum, const void *data, usize len);
u16  inet_csum_fold(u32 sum);
u32  inet_pseudo_sum(u32 src, u32 dst, u8 proto, u16 len);
bool ipv4_send(u32 dst, u8 proto, const void *payload, u16 len);
bool ipv4_send_from(u32 src, u32 dst, u8 proto, const void *payload, u16 len);   /* src 0 = o da interface */
void ipv4_input(const u8 *p, u16 len);
bool net_eth_send(const u8 *dst_mac, u16 type, const void *payload, u16 len);
void udp_deliver(u32 src, u16 sport, u16 dport, const u8 *data, u16 len);

/* IPv6 e laço local (ipv6.c) */
extern net6_info_t g6;
u32  l4_pseudo_sum(const u8 *src16, const u8 *dst16, u8 proto, u16 len);
bool ip6_source_for(const u8 *dst16, u8 *src16);
bool ipv6_send(const u8 *src16, const u8 *dst16, u8 nh, const void *payload, u16 len);   /* src NULL = escolhe */
bool ip_send_any(const u8 *src16, const u8 *dst16, u8 proto, const void *payload, u16 len);
bool udp6_send(const u8 *dst16, u16 sport, u16 dport, const void *data, u16 len);
void ipv6_input(const u8 *p, u16 len);        /* do laço local */
void ipv6_eth_input(const u8 *p, u16 len);    /* da placa */
void ipv6_start(void);
void ipv6_tick(u64 now);
bool net_loop_enqueue(bool v6, const void *pkt, u16 len);
void net_loop_drain(void);
void icmp_echo_deliver(const u8 *from16, const u8 *msg, u16 len);
void tcp_input6(const u8 *src16, const u8 *dst16, const u8 *seg, u16 len);
u16  net_ephemeral_port(void);

/* sub-módulos */
void dhcp_init(void);
void dhcp_tick(u64 now);
void dns_init(void);
void dns_tick(u64 now);
void tcp_input(u32 src, u32 dst, const u8 *seg, u16 len);
void tcp_tick(u64 now);
void udp_input(u32 src, u32 dst, const u8 *pkt, u16 len);
const char *tcp_error_reason(int s);   /* motivo do estado TCP_ERROR (pt-BR) */
void net_on_configured(void);

/* drivers */
bool e1000_probe(void);
bool rtl8139_probe(void);
