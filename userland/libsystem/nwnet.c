/*
 * mOS — Network.framework, API C (nw_*). Objetos são objetos ObjC (como os OS_object do macOS): nw_retain/nw_release
 * são retain/release do runtime e o ARC do app funciona. Transporte: o mesmo do NSURLSession (urlsession_io.c, TCP e
 * TLS pelo SecureTransport com ALPN), UDP por socket, WebSocket pelo mnet_ws. Cada conexão e cada listener têm um fio
 * próprio; os blocos do app rodam na fila que o app deu (nw_*_set_queue), em ordem.
 *
 * Coberto: endpoints (host, endereço, URL, Bonjour como dados), parâmetros (TCP/UDP/TLS, pilha de protocolos,
 * interface exigida/proibida, cara/restrita, endpoint local, reuso), opções TCP/UDP/TLS/WebSocket, conexão (estados,
 * send/receive/receive_message, contexto, metadados TLS e WS, caminho), listener TCP/UDP/WS (TLS de servidor: o motor
 * ainda não tem lado servidor, falha com erro TLS), monitor de caminho com interfaces reais, erros.
 * Fora: nw_browser/Bonjour (etapa de mDNS) e nw_framer.
 */
#include "foundation.h"
#include "libc.h"
#include "mnet.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *p);
void _Block_release(const void *b);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_get_main_queue(void);
void *dispatch_get_global_queue(long p, unsigned long f);
void dispatch_retain(void *o);
void dispatch_release(void *o);
void *dispatch_data_create(const void *buf, size_t n, void *queue, void *destructor);
size_t dispatch_data_get_size(void *d);
void *dispatch_data_create_map(void *d, const void **buf, size_t *n);
extern char _dispatch_data_empty[];
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
void os_unfair_lock_lock(void *l);
void os_unfair_lock_unlock(void *l);
int socket(int, int, int);
int bind(int, const void *, unsigned);
int listen(int, int);
int accept(int, void *, unsigned *);
int connect(int, const void *, unsigned);
int getsockname(int, void *, unsigned *);
int getpeername(int, void *, unsigned *);
int setsockopt(int, int, int, const void *, unsigned);
long send(int, const void *, unsigned long, int);
long recv(int, void *, unsigned long, int);
long sendto(int, const void *, unsigned long, int, const void *, unsigned);
long recvfrom(int, void *, unsigned long, int, void *, unsigned *);
int close(int);
int fcntl(int, int, ...);
typedef struct { int fd; short events, revents; } pfd_t;
int poll(pfd_t *, unsigned, int);
int *__error(void);
struct mos_ai { int flags, family, socktype, protocol; unsigned addrlen; char *canon; void *addr; struct mos_ai *next; };
int getaddrinfo(const char *, const char *, const void *, void **);
void freeaddrinfo(void *);
int inet_pton(int af, const char *s, void *dst);
const char *inet_ntop(int af, const void *src, char *dst, unsigned n);
void arc4random_buf(void *b, size_t n);
id CFErrorCreate(void *alloc, id domain, long code, id info);

typedef struct {
    char bsd[16]; char driver[48]; unsigned index, flags; int loopback, wifi, link, active;
    unsigned char mac[6]; unsigned ip, mask; unsigned char ll6[16], global6[16]; int has_ll6, has_g6, prefix6;
    unsigned long long rx_bytes, tx_bytes, rx_packets, tx_packets;
} cx_netif_t;
int cx_netif_list(cx_netif_t *out, int max);

/* blocos */
typedef struct { void *isa; int flags, reserved; void *invoke; void *desc; } blk_t;
#define BLK(b) ((blk_t *)(b))
static void *bcopy_(void *b) { return b ? _Block_copy(b) : 0; }
static void brel(void *b) { if (b) _Block_release(b); }

static unsigned g_lock;
static void LK(void) { os_unfair_lock_lock(&g_lock); }
static void ULK(void) { os_unfair_lock_unlock(&g_lock); }
static id retain(id o) { return o ? objc_retain(o) : 0; }
static void rel(id o) { if (o) objc_release(o); }
static char *dupc(const char *s) { return s ? mnet_strndup(s, strlen(s)) : 0; }

/* ---------------------------------------------------------------- classes */
#define CLASS(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
CLASS(OS_nw_object) CLASS(OS_nw_endpoint) CLASS(OS_nw_parameters) CLASS(OS_nw_protocol_stack) CLASS(OS_nw_protocol_options)
CLASS(OS_nw_protocol_definition) CLASS(OS_nw_protocol_metadata) CLASS(OS_nw_connection) CLASS(OS_nw_listener)
CLASS(OS_nw_path) CLASS(OS_nw_path_monitor) CLASS(OS_nw_interface) CLASS(OS_nw_content_context) CLASS(OS_nw_error)
CLASS(OS_sec_protocol_options) CLASS(OS_sec_protocol_metadata) CLASS(OS_nw_ws_request) CLASS(OS_nw_ws_response)
static int g_ready;
static void nw_init_classes(void);
static id nwnew(cx_class_t *k, size_t sz) { nw_init_classes(); id o = cx_alloc_instance(k); memset((u8 *)o + 8, 0, sz - 8); return o; }

void *nw_retain(void *o) { return o ? (void *)objc_retain((id)o) : 0; }
void nw_release(void *o) { if (o) objc_release((id)o); }

/* ---------------------------------------------------------------- erros */
enum { ED_INVALID, ED_POSIX, ED_DNS, ED_TLS };
typedef struct { cx_class_t *isa; int domain, code; } NWError_t;
#define ER(o) ((NWError_t *)(o))
static id err_new(int dom, int code) { id e = nwnew(&OS_nw_error_c, sizeof(NWError_t)); ER(e)->domain = dom; ER(e)->code = code; return e; }
int nw_error_get_error_domain(void *e) { return e ? ER(e)->domain : ED_INVALID; }
int nw_error_get_error_code(void *e) { return e ? ER(e)->code : 0; }
id nw_error_copy_cf_error(void *e) {
    if (!e) return 0;
    const char *d = ER(e)->domain == ED_POSIX ? "NSPOSIXErrorDomain" : ER(e)->domain == ED_DNS ? "kNWErrorDomainDNS" :
                    ER(e)->domain == ED_TLS ? "kNWErrorDomainTLS" : "kNWErrorDomainPOSIX";
    return CFErrorCreate(0, cx_str(d), ER(e)->code, 0);
}
static id err_desc(id s, SEL c) {
    (void)c;
    char b[96];
    snprintf(b, sizeof b, "%s(%d)", ER(s)->domain == ED_POSIX ? "POSIXErrorCode" : ER(s)->domain == ED_DNS ? "DNSError" : "TLSError", ER(s)->code);
    return cx_str(b);
}
static int mnet_to_posix(int e) {
    switch (e) {
    case MNET_E_TIMEOUT: return 60;           /* ETIMEDOUT */
    case MNET_E_CONNECT: return 61;           /* ECONNREFUSED */
    case MNET_E_LOST: return 54;              /* ECONNRESET */
    case MNET_E_CANCELLED: return 89;         /* ECANCELED */
    case MNET_E_OFFLINE: return 50;           /* ENETDOWN */
    }
    return 5;                                 /* EIO */
}

/* ---------------------------------------------------------------- endpoints */
enum { EP_INVALID, EP_ADDRESS, EP_HOST, EP_BONJOUR, EP_URL };
typedef struct { cx_class_t *isa; int type; char *host, *url, *bj_name, *bj_type, *bj_domain; unsigned short port; u8 sa[28]; id iface; } NWEndpoint_t;
#define EP(o) ((NWEndpoint_t *)(o))
static void ep_dealloc(id s, SEL c) {
    free(EP(s)->host); free(EP(s)->url); free(EP(s)->bj_name); free(EP(s)->bj_type); free(EP(s)->bj_domain); rel(EP(s)->iface);
    IMP sup = cx_lookup(&OS_nw_object_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c);
}
id nw_endpoint_create_host(const char *host, const char *port) {
    id e = nwnew(&OS_nw_endpoint_c, sizeof(NWEndpoint_t));
    EP(e)->type = EP_HOST;
    EP(e)->host = dupc(host ? host : "");
    EP(e)->port = (unsigned short)(port ? atoi(port) : 0);
    /* literal de endereço vira endpoint de endereço, como no macOS */
    u8 b[16];
    if (host && inet_pton(2, host, b) == 1) {
        EP(e)->type = EP_ADDRESS;
        EP(e)->sa[0] = 16; EP(e)->sa[1] = 2; EP(e)->sa[2] = (u8)(EP(e)->port >> 8); EP(e)->sa[3] = (u8)EP(e)->port; memcpy(EP(e)->sa + 4, b, 4);
    } else if (host && inet_pton(30, host, b) == 1) {
        EP(e)->type = EP_ADDRESS;
        EP(e)->sa[0] = 28; EP(e)->sa[1] = 30; EP(e)->sa[2] = (u8)(EP(e)->port >> 8); EP(e)->sa[3] = (u8)EP(e)->port; memcpy(EP(e)->sa + 8, b, 16);
    }
    return e;
}
id nw_endpoint_create_host_with_numeric_port(const char *host, unsigned short port) {
    char p[8]; snprintf(p, sizeof p, "%u", port);
    return nw_endpoint_create_host(host, p);
}
id nw_endpoint_create_address(const u8 *sa) {
    if (!sa || (sa[1] != 2 && sa[1] != 30)) return 0;
    id e = nwnew(&OS_nw_endpoint_c, sizeof(NWEndpoint_t));
    EP(e)->type = EP_ADDRESS;
    memcpy(EP(e)->sa, sa, sa[1] == 2 ? 16 : 28);
    EP(e)->port = (unsigned short)(sa[2] << 8 | sa[3]);
    char t[64];
    inet_ntop(sa[1], sa[1] == 2 ? sa + 4 : sa + 8, t, sizeof t);
    EP(e)->host = dupc(t);
    return e;
}
id nw_endpoint_create_url(const char *url) {
    mnet_url_t u;
    if (!url || mnet_url_parse(url, &u)) return 0;
    id e = nwnew(&OS_nw_endpoint_c, sizeof(NWEndpoint_t));
    EP(e)->type = EP_URL; EP(e)->url = dupc(url); EP(e)->host = dupc(u.host); EP(e)->port = (unsigned short)u.port;
    mnet_url_free(&u);
    return e;
}
id nw_endpoint_create_bonjour_service(const char *name, const char *type, const char *domain) {
    id e = nwnew(&OS_nw_endpoint_c, sizeof(NWEndpoint_t));
    EP(e)->type = EP_BONJOUR; EP(e)->bj_name = dupc(name); EP(e)->bj_type = dupc(type); EP(e)->bj_domain = dupc(domain ? domain : "local.");
    return e;
}
int nw_endpoint_get_type(void *e) { return e ? EP(e)->type : EP_INVALID; }
const char *nw_endpoint_get_hostname(void *e) { return e && EP(e)->type == EP_HOST ? EP(e)->host : 0; }
unsigned short nw_endpoint_get_port(void *e) { return e ? EP(e)->port : 0; }
char *nw_endpoint_copy_port_string(void *e) { char b[8]; snprintf(b, sizeof b, "%u", e ? EP(e)->port : 0); return dupc(b); }
char *nw_endpoint_copy_address_string(void *e) { return e && EP(e)->type == EP_ADDRESS ? dupc(EP(e)->host) : 0; }
const u8 *nw_endpoint_get_address(void *e) { return e && EP(e)->type == EP_ADDRESS ? EP(e)->sa : 0; }
const char *nw_endpoint_get_url(void *e) { return e && EP(e)->type == EP_URL ? EP(e)->url : 0; }
const char *nw_endpoint_get_bonjour_service_name(void *e) { return e && EP(e)->type == EP_BONJOUR ? EP(e)->bj_name : 0; }
const char *nw_endpoint_get_bonjour_service_type(void *e) { return e && EP(e)->type == EP_BONJOUR ? EP(e)->bj_type : 0; }
const char *nw_endpoint_get_bonjour_service_domain(void *e) { return e && EP(e)->type == EP_BONJOUR ? EP(e)->bj_domain : 0; }
static id ep_desc(id s, SEL c) {
    (void)c;
    char b[400];
    if (EP(s)->type == EP_BONJOUR) snprintf(b, sizeof b, "%s.%s%s", EP(s)->bj_name, EP(s)->bj_type, EP(s)->bj_domain);
    else if (EP(s)->type == EP_URL) snprintf(b, sizeof b, "%s", EP(s)->url);
    else snprintf(b, sizeof b, strchr(EP(s)->host ? EP(s)->host : "", ':') ? "[%s]:%u" : "%s:%u", EP(s)->host ? EP(s)->host : "", EP(s)->port);
    return cx_str(b);
}

/* ---------------------------------------------------------------- opções de protocolo */
enum { PR_TCP = 1, PR_UDP, PR_TLS, PR_WS, PR_IP, PR_QUIC };
typedef struct {
    cx_class_t *isa; int kind;
    /* tcp */ int no_delay, keepalive, ka_idle, ka_count, ka_interval, conn_timeout, no_push, fast_open, persist_timeout, drop_time;
    /* udp */ int prefer_no_checksum;
    /* tls */ id sec;
    /* ws */ int ws_version, auto_reply_ping, skip_handshake; size_t max_msg; mnet_hdrs_t ws_hdrs; char **ws_protos; int nprotos;
    void *ws_handler; void *ws_queue;
    /* ip */ int ip_version, hop_limit, use_minimum_mtu, disable_fragmentation;
} NWOptions_t;
#define PO(o) ((NWOptions_t *)(o))
typedef struct {
    cx_class_t *isa;
    char **alpn; int nalpn;
    int min_ver, max_ver, peer_auth_required;
    char sni[256];
    void *verify_block, *verify_queue, *challenge_block, *challenge_queue;
    int tickets, false_start;
} SecOpts_t;
#define SO(o) ((SecOpts_t *)(o))
static void po_dealloc(id s, SEL c) {
    rel(PO(s)->sec); mnet_hdrs_free(&PO(s)->ws_hdrs);
    for (int i = 0; i < PO(s)->nprotos; i++) free(PO(s)->ws_protos[i]);
    free(PO(s)->ws_protos); brel(PO(s)->ws_handler);
    IMP sup = cx_lookup(&OS_nw_object_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static void so_dealloc(id s, SEL c) {
    for (int i = 0; i < SO(s)->nalpn; i++) free(SO(s)->alpn[i]);
    free(SO(s)->alpn); brel(SO(s)->verify_block); brel(SO(s)->challenge_block);
    IMP sup = cx_lookup(&OS_nw_object_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static id opts_new(int kind) {
    id o = nwnew(&OS_nw_protocol_options_c, sizeof(NWOptions_t));
    PO(o)->kind = kind;
    if (kind == PR_TLS) { PO(o)->sec = nwnew(&OS_sec_protocol_options_c, sizeof(SecOpts_t)); SO(PO(o)->sec)->min_ver = 0x0303; SO(PO(o)->sec)->max_ver = 0x0304; SO(PO(o)->sec)->peer_auth_required = 1; }
    if (kind == PR_WS) { PO(o)->ws_version = 1; PO(o)->auto_reply_ping = 0; }
    return o;
}
id nw_tcp_create_options(void) { return opts_new(PR_TCP); }
id nw_udp_create_options(void) { return opts_new(PR_UDP); }
id nw_tls_create_options(void) { return opts_new(PR_TLS); }
id nw_ws_create_options(int version) { id o = opts_new(PR_WS); PO(o)->ws_version = version; return o; }
id nw_quic_create_options(void) { return opts_new(PR_QUIC); }
static int is_opts(void *o, int k) { return o && object_getClass((id)o) == &OS_nw_protocol_options_c && (!k || PO(o)->kind == k); }
void nw_tcp_options_set_no_delay(void *o, u8 v) { if (is_opts(o, PR_TCP)) PO(o)->no_delay = v; }
void nw_tcp_options_set_no_push(void *o, u8 v) { if (is_opts(o, PR_TCP)) PO(o)->no_push = v; }
void nw_tcp_options_set_enable_keepalive(void *o, u8 v) { if (is_opts(o, PR_TCP)) PO(o)->keepalive = v; }
void nw_tcp_options_set_keepalive_count(void *o, u32 v) { if (is_opts(o, PR_TCP)) PO(o)->ka_count = (int)v; }
void nw_tcp_options_set_keepalive_idle_time(void *o, u32 v) { if (is_opts(o, PR_TCP)) PO(o)->ka_idle = (int)v; }
void nw_tcp_options_set_keepalive_interval(void *o, u32 v) { if (is_opts(o, PR_TCP)) PO(o)->ka_interval = (int)v; }
void nw_tcp_options_set_connection_timeout(void *o, u32 v) { if (is_opts(o, PR_TCP)) PO(o)->conn_timeout = (int)v; }
void nw_tcp_options_set_persist_timeout(void *o, u32 v) { if (is_opts(o, PR_TCP)) PO(o)->persist_timeout = (int)v; }
void nw_tcp_options_set_retransmit_connection_drop_time(void *o, u32 v) { if (is_opts(o, PR_TCP)) PO(o)->drop_time = (int)v; }
void nw_tcp_options_set_enable_fast_open(void *o, u8 v) { if (is_opts(o, PR_TCP)) PO(o)->fast_open = v; }
void nw_tcp_options_set_maximum_segment_size(void *o, u32 v) { (void)o; (void)v; }          /* a pilha do kernel negocia o MSS */
void nw_tcp_options_set_disable_ack_stretching(void *o, u8 v) { (void)o; (void)v; }
void nw_tcp_options_set_disable_ecn(void *o, u8 v) { (void)o; (void)v; }
void nw_tcp_options_set_retransmit_fin_drop(void *o, u8 v) { (void)o; (void)v; }
void nw_udp_options_set_prefer_no_checksum(void *o, u8 v) { if (is_opts(o, PR_UDP)) PO(o)->prefer_no_checksum = v; }
id nw_tls_copy_sec_protocol_options(void *o) { return is_opts(o, PR_TLS) ? retain(PO(o)->sec) : 0; }
/* WebSocket */
void nw_ws_options_add_additional_header(void *o, const char *n, const char *v) { if (is_opts(o, PR_WS) && n && v) mnet_hdrs_add(&PO(o)->ws_hdrs, n, strlen(n), v, strlen(v)); }
void nw_ws_options_add_subprotocol(void *o, const char *p) {
    if (!is_opts(o, PR_WS) || !p) return;
    PO(o)->ws_protos = realloc(PO(o)->ws_protos, (size_t)(PO(o)->nprotos + 1) * sizeof(char *));
    PO(o)->ws_protos[PO(o)->nprotos++] = dupc(p);
}
void nw_ws_options_set_auto_reply_ping(void *o, u8 v) { if (is_opts(o, PR_WS)) PO(o)->auto_reply_ping = v; }
void nw_ws_options_set_maximum_message_size(void *o, size_t v) { if (is_opts(o, PR_WS)) PO(o)->max_msg = v; }
void nw_ws_options_set_skip_handshake(void *o, u8 v) { if (is_opts(o, PR_WS)) PO(o)->skip_handshake = v; }
void nw_ws_options_set_client_request_handler(void *o, void *q, void *h) { if (!is_opts(o, PR_WS)) return; brel(PO(o)->ws_handler); PO(o)->ws_handler = bcopy_(h); PO(o)->ws_queue = q; }
/* IP */
void nw_ip_options_set_version(void *o, int v) { if (is_opts(o, PR_IP)) PO(o)->ip_version = v; }
void nw_ip_options_set_hop_limit(void *o, u8 v) { if (is_opts(o, PR_IP)) PO(o)->hop_limit = v; }
void nw_ip_options_set_use_minimum_mtu(void *o, u8 v) { if (is_opts(o, PR_IP)) PO(o)->use_minimum_mtu = v; }
void nw_ip_options_set_disable_fragmentation(void *o, u8 v) { if (is_opts(o, PR_IP)) PO(o)->disable_fragmentation = v; }
void nw_ip_options_set_calculate_receive_time(void *o, u8 v) { (void)o; (void)v; }
void nw_ip_options_set_local_address_preference(void *o, int v) { (void)o; (void)v; }
/* sec_protocol_options (Security, parte TLS do Network) */
static int is_sec(void *o) { return o && object_getClass((id)o) == &OS_sec_protocol_options_c; }
void sec_protocol_options_add_tls_application_protocol(void *o, const char *p) {
    if (!is_sec(o) || !p) return;
    LK();
    SO(o)->alpn = realloc(SO(o)->alpn, (size_t)(SO(o)->nalpn + 1) * sizeof(char *));
    SO(o)->alpn[SO(o)->nalpn++] = dupc(p);
    ULK();
}
void sec_protocol_options_set_tls_server_name(void *o, const char *n) { if (is_sec(o) && n) snprintf(SO(o)->sni, sizeof SO(o)->sni, "%s", n); }
void sec_protocol_options_set_min_tls_protocol_version(void *o, int v) { if (is_sec(o)) SO(o)->min_ver = v; }
void sec_protocol_options_set_max_tls_protocol_version(void *o, int v) { if (is_sec(o)) SO(o)->max_ver = v; }
void sec_protocol_options_set_tls_min_version(void *o, int v) { if (is_sec(o)) SO(o)->min_ver = v; }
void sec_protocol_options_set_tls_max_version(void *o, int v) { if (is_sec(o)) SO(o)->max_ver = v; }
void sec_protocol_options_set_peer_authentication_required(void *o, u8 v) { if (is_sec(o)) SO(o)->peer_auth_required = v; }
void sec_protocol_options_set_verify_block(void *o, void *b, void *q) { if (!is_sec(o)) return; brel(SO(o)->verify_block); SO(o)->verify_block = bcopy_(b); SO(o)->verify_queue = q; }
void sec_protocol_options_set_challenge_block(void *o, void *b, void *q) { if (!is_sec(o)) return; brel(SO(o)->challenge_block); SO(o)->challenge_block = bcopy_(b); SO(o)->challenge_queue = q; }
void sec_protocol_options_set_tls_tickets_enabled(void *o, u8 v) { if (is_sec(o)) SO(o)->tickets = v; }
void sec_protocol_options_set_tls_false_start_enabled(void *o, u8 v) { if (is_sec(o)) SO(o)->false_start = v; }
void sec_protocol_options_append_tls_ciphersuite(void *o, int s) { (void)o; (void)s; }          /* o motor escolhe entre as suas */
void sec_protocol_options_append_tls_ciphersuite_group(void *o, int g) { (void)o; (void)g; }
void sec_protocol_options_add_pre_shared_key(void *o, void *psk, void *ident) { (void)o; (void)psk; (void)ident; }
void sec_protocol_options_set_local_identity(void *o, void *ident) { (void)o; (void)ident; }
u8 sec_protocol_options_are_equal(void *a, void *b) {
    if (!is_sec(a) || !is_sec(b)) return a == b;
    if (SO(a)->nalpn != SO(b)->nalpn || SO(a)->min_ver != SO(b)->min_ver || SO(a)->max_ver != SO(b)->max_ver || strcmp(SO(a)->sni, SO(b)->sni)) return 0;
    for (int i = 0; i < SO(a)->nalpn; i++) if (strcmp(SO(a)->alpn[i], SO(b)->alpn[i])) return 0;
    return 1;
}
/* metadados de TLS de uma conexão pronta */
typedef struct { cx_class_t *isa; char alpn[33]; int version, suite; id trust; char sni[256]; } SecMeta_t;
#define SM(o) ((SecMeta_t *)(o))
const char *sec_protocol_metadata_get_negotiated_protocol(void *m) { return m && object_getClass((id)m) == &OS_sec_protocol_metadata_c && SM(m)->alpn[0] ? SM(m)->alpn : 0; }
int sec_protocol_metadata_get_negotiated_tls_protocol_version(void *m) { return m && object_getClass((id)m) == &OS_sec_protocol_metadata_c ? SM(m)->version : 0; }
int sec_protocol_metadata_get_negotiated_protocol_version(void *m) { int v = sec_protocol_metadata_get_negotiated_tls_protocol_version(m); return v == 0x0304 ? 10 : v == 0x0303 ? 8 : 0; }
int sec_protocol_metadata_get_negotiated_tls_ciphersuite(void *m) { return m && object_getClass((id)m) == &OS_sec_protocol_metadata_c ? SM(m)->suite : 0; }
const char *sec_protocol_metadata_get_server_name(void *m) { return m && object_getClass((id)m) == &OS_sec_protocol_metadata_c ? SM(m)->sni : 0; }
void *sec_trust_copy_ref(void *t) { return t ? (void *)retain((id)t) : 0; }
void *sec_trust_create(void *t) { return t ? (void *)retain((id)t) : 0; }

/* definições de protocolo (comparar opções/metadados) */
typedef struct { cx_class_t *isa; int kind; } NWDef_t;
static id defs[8];
static id def_of(int kind) {
    LK();
    if (!defs[kind]) { defs[kind] = nwnew(&OS_nw_protocol_definition_c, sizeof(NWDef_t)); ((NWDef_t *)defs[kind])->kind = kind; }
    ULK();
    return defs[kind];
}
id nw_protocol_copy_tcp_definition(void) { return retain(def_of(PR_TCP)); }
id nw_protocol_copy_udp_definition(void) { return retain(def_of(PR_UDP)); }
id nw_protocol_copy_tls_definition(void) { return retain(def_of(PR_TLS)); }
id nw_protocol_copy_ws_definition(void) { return retain(def_of(PR_WS)); }
id nw_protocol_copy_ip_definition(void) { return retain(def_of(PR_IP)); }
id nw_protocol_copy_quic_definition(void) { return retain(def_of(PR_QUIC)); }
id nw_protocol_options_copy_definition(void *o) { return is_opts(o, 0) ? retain(def_of(PO(o)->kind)) : 0; }
u8 nw_protocol_definition_is_equal(void *a, void *b) { return a == b; }
u8 nw_protocol_options_is_tcp(void *o) { return is_opts(o, PR_TCP); }
u8 nw_protocol_options_is_udp(void *o) { return is_opts(o, PR_UDP); }
u8 nw_protocol_options_is_tls(void *o) { return is_opts(o, PR_TLS); }
u8 nw_protocol_options_is_ws(void *o) { return is_opts(o, PR_WS); }
u8 nw_protocol_options_is_quic(void *o) { return is_opts(o, PR_QUIC); }

/* ---------------------------------------------------------------- pilha e parâmetros */
typedef struct { cx_class_t *isa; id app[4]; int napp; id transport, internet; } NWStack_t;
#define ST(o) ((NWStack_t *)(o))
typedef struct {
    cx_class_t *isa; id stack; id local; int req_type, proh[5], nproh, expensive_ok, constrained_ok, reuse, p2p, fast_open,
    service_class, local_only, prefer_no_proxy, multipath; id req_iface; int udp;
} NWParams_t;
#define PA(o) ((NWParams_t *)(o))
static void st_dealloc(id s, SEL c) { for (int i = 0; i < ST(s)->napp; i++) rel(ST(s)->app[i]); rel(ST(s)->transport); rel(ST(s)->internet); IMP sup = cx_lookup(&OS_nw_object_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c); }
static void pa_dealloc(id s, SEL c) { rel(PA(s)->stack); rel(PA(s)->local); rel(PA(s)->req_iface); IMP sup = cx_lookup(&OS_nw_object_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c); }
/* constantes de bloco: _nw_parameters_configure_protocol_default_configuration / _disable_protocol */
static void cfg_noop(void *b, void *o) { (void)b; (void)o; }
static struct { unsigned long r, size; } blk_desc = {0, sizeof(blk_t)};
extern char _NSConcreteGlobalBlock[];
blk_t _nw_parameters_configure_protocol_default_configuration = {_NSConcreteGlobalBlock, 1 << 28, 0, (void *)cfg_noop, &blk_desc};
blk_t _nw_parameters_configure_protocol_disable = {_NSConcreteGlobalBlock, 1 << 28, 0, (void *)cfg_noop, &blk_desc};
static void configure(void *b, id opts) {
    if (!b || b == &_nw_parameters_configure_protocol_default_configuration) return;
    ((void (*)(void *, id))BLK(b)->invoke)(b, opts);
}
static id params_new(int udp, void *tls_cfg, void *transport_cfg) {
    id p = nwnew(&OS_nw_parameters_c, sizeof(NWParams_t));
    PA(p)->stack = nwnew(&OS_nw_protocol_stack_c, sizeof(NWStack_t));
    PA(p)->expensive_ok = PA(p)->constrained_ok = 1;
    PA(p)->udp = udp;
    PA(p)->req_type = -1;
    ST(PA(p)->stack)->transport = opts_new(udp ? PR_UDP : PR_TCP);
    ST(PA(p)->stack)->internet = opts_new(PR_IP);
    configure(transport_cfg, ST(PA(p)->stack)->transport);
    if (tls_cfg && tls_cfg != &_nw_parameters_configure_protocol_disable) {
        id t = opts_new(PR_TLS);
        configure(tls_cfg, t);
        ST(PA(p)->stack)->app[ST(PA(p)->stack)->napp++] = t;
    }
    return p;
}
id nw_parameters_create_secure_tcp(void *tls, void *tcp) { return params_new(0, tls, tcp); }
id nw_parameters_create_secure_udp(void *dtls, void *udp) { return params_new(1, dtls, udp); }
id nw_parameters_create(void) { return params_new(0, &_nw_parameters_configure_protocol_disable, 0); }
id nw_parameters_create_quic(void *cfg) { id p = params_new(1, &_nw_parameters_configure_protocol_disable, 0); id q = opts_new(PR_QUIC); configure(cfg, q); rel(ST(PA(p)->stack)->transport); ST(PA(p)->stack)->transport = q; return p; }
id nw_parameters_copy(void *src) {
    if (!src) return 0;
    id p = nwnew(&OS_nw_parameters_c, sizeof(NWParams_t));
    memcpy((u8 *)p + 8, (u8 *)src + 8, sizeof(NWParams_t) - 8);
    id st = nwnew(&OS_nw_protocol_stack_c, sizeof(NWStack_t));
    NWStack_t *o = ST(PA(src)->stack);
    for (int i = 0; i < o->napp; i++) ST(st)->app[i] = retain(o->app[i]);
    ST(st)->napp = o->napp; ST(st)->transport = retain(o->transport); ST(st)->internet = retain(o->internet);
    PA(p)->stack = st; PA(p)->local = retain(PA(src)->local); PA(p)->req_iface = retain(PA(src)->req_iface);
    return p;
}
id nw_parameters_copy_default_protocol_stack(void *p) { return p ? retain(PA(p)->stack) : 0; }
void nw_protocol_stack_prepend_application_protocol(void *s, void *o) {
    if (!s || !o || ST(s)->napp >= 4) return;
    memmove(ST(s)->app + 1, ST(s)->app, (size_t)ST(s)->napp * sizeof(id));
    ST(s)->app[0] = retain((id)o); ST(s)->napp++;
}
void nw_protocol_stack_clear_application_protocols(void *s) { if (!s) return; for (int i = 0; i < ST(s)->napp; i++) rel(ST(s)->app[i]); ST(s)->napp = 0; }
void nw_protocol_stack_iterate_application_protocols(void *s, void *b) {
    if (!s || !b) return;
    for (int i = 0; i < ST(s)->napp; i++) ((void (*)(void *, id))BLK(b)->invoke)(b, ST(s)->app[i]);
}
id nw_protocol_stack_copy_transport_protocol(void *s) { return s ? retain(ST(s)->transport) : 0; }
void nw_protocol_stack_set_transport_protocol(void *s, void *o) { if (!s) return; id old = ST(s)->transport; ST(s)->transport = retain((id)o); rel(old); }
id nw_protocol_stack_copy_internet_protocol(void *s) { return s ? retain(ST(s)->internet) : 0; }
void nw_parameters_set_local_endpoint(void *p, void *e) { if (!p) return; id o = PA(p)->local; PA(p)->local = retain((id)e); rel(o); }
id nw_parameters_copy_local_endpoint(void *p) { return p ? retain(PA(p)->local) : 0; }
void nw_parameters_set_required_interface_type(void *p, int t) { if (p) PA(p)->req_type = t; }
int nw_parameters_get_required_interface_type(void *p) { return p && PA(p)->req_type >= 0 ? PA(p)->req_type : 0; }
void nw_parameters_require_interface(void *p, void *i) { if (!p) return; id o = PA(p)->req_iface; PA(p)->req_iface = retain((id)i); rel(o); }
id nw_parameters_copy_required_interface(void *p) { return p ? retain(PA(p)->req_iface) : 0; }
void nw_parameters_prohibit_interface_type(void *p, int t) { if (p && PA(p)->nproh < 5) PA(p)->proh[PA(p)->nproh++] = t; }
void nw_parameters_clear_prohibited_interface_types(void *p) { if (p) PA(p)->nproh = 0; }
void nw_parameters_set_prohibit_expensive(void *p, u8 v) { if (p) PA(p)->expensive_ok = !v; }
u8 nw_parameters_get_prohibit_expensive(void *p) { return p && !PA(p)->expensive_ok; }
void nw_parameters_set_prohibit_constrained(void *p, u8 v) { if (p) PA(p)->constrained_ok = !v; }
u8 nw_parameters_get_prohibit_constrained(void *p) { return p && !PA(p)->constrained_ok; }
void nw_parameters_set_reuse_local_address(void *p, u8 v) { if (p) PA(p)->reuse = v; }
u8 nw_parameters_get_reuse_local_address(void *p) { return p && PA(p)->reuse; }
void nw_parameters_set_include_peer_to_peer(void *p, u8 v) { if (p) PA(p)->p2p = v; }
u8 nw_parameters_get_include_peer_to_peer(void *p) { return p && PA(p)->p2p; }
void nw_parameters_set_fast_open_enabled(void *p, u8 v) { if (p) PA(p)->fast_open = v; }
u8 nw_parameters_get_fast_open_enabled(void *p) { return p && PA(p)->fast_open; }
void nw_parameters_set_service_class(void *p, int v) { if (p) PA(p)->service_class = v; }
int nw_parameters_get_service_class(void *p) { return p ? PA(p)->service_class : 0; }
void nw_parameters_set_multipath_service(void *p, int v) { if (p) PA(p)->multipath = v; }
int nw_parameters_get_multipath_service(void *p) { return p ? PA(p)->multipath : 0; }
void nw_parameters_set_local_only(void *p, u8 v) { if (p) PA(p)->local_only = v; }
u8 nw_parameters_get_local_only(void *p) { return p && PA(p)->local_only; }
void nw_parameters_set_prefer_no_proxy(void *p, u8 v) { if (p) PA(p)->prefer_no_proxy = v; }
u8 nw_parameters_get_prefer_no_proxy(void *p) { return p && PA(p)->prefer_no_proxy; }
void nw_parameters_set_expired_dns_behavior(void *p, int v) { (void)p; (void)v; }
static id find_app(id params, int kind) {
    NWStack_t *s = params ? ST(PA(params)->stack) : 0;
    for (int i = 0; s && i < s->napp; i++) if (PO(s->app[i])->kind == kind) return s->app[i];
    return 0;
}

/* ---------------------------------------------------------------- interfaces e caminhos */
enum { IT_OTHER, IT_WIFI, IT_CELLULAR, IT_WIRED, IT_LOOPBACK };
typedef struct { cx_class_t *isa; cx_netif_t f; } NWIface_t;
#define IFC(o) ((NWIface_t *)(o))
int nw_interface_get_type(void *i) { return !i ? IT_OTHER : IFC(i)->f.loopback ? IT_LOOPBACK : IFC(i)->f.wifi ? IT_WIFI : IT_WIRED; }
const char *nw_interface_get_name(void *i) { return i ? IFC(i)->f.bsd : 0; }
u32 nw_interface_get_index(void *i) { return i ? IFC(i)->f.index : 0; }
static id iface_new(const cx_netif_t *f) { id o = nwnew(&OS_nw_interface_c, sizeof(NWIface_t)); IFC(o)->f = *f; return o; }

enum { PS_INVALID, PS_SATISFIED, PS_UNSATISFIED, PS_SATISFIABLE };
typedef struct { cx_class_t *isa; int status, reason, v4, v6, dns; id ifs[9]; int nifs; id local, remote; } NWPath_t;
#define PT(o) ((NWPath_t *)(o))
static void pt_dealloc(id s, SEL c) { for (int i = 0; i < PT(s)->nifs; i++) rel(PT(s)->ifs[i]); rel(PT(s)->local); rel(PT(s)->remote); IMP sup = cx_lookup(&OS_nw_object_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c); }
static int iface_allowed(const cx_netif_t *f, int req_type, const int *proh, int nproh) {
    int t = f->loopback ? IT_LOOPBACK : f->wifi ? IT_WIFI : IT_WIRED;
    if (req_type > 0 && t != req_type) return 0;
    for (int i = 0; i < nproh; i++) if (proh[i] == t) return 0;
    return 1;
}
/* foto do estado atual; sig recebe uma assinatura para detectar mudança */
static id path_snapshot(int req_type, const int *proh, int nproh, unsigned long long *sig) {
    cx_netif_t f[9];
    int n = cx_netif_list(f, 9);
    id p = nwnew(&OS_nw_path_c, sizeof(NWPath_t));
    unsigned long long s = 1469598103934665603ull;
    int up = 0;
    for (int i = 0; i < n; i++) {
        if (!iface_allowed(&f[i], req_type, proh, nproh)) continue;
        int usable = f[i].loopback || (f[i].link && f[i].active && (f[i].ip || f[i].has_g6));
        if (!f[i].loopback && !f[i].link) continue;
        PT(p)->ifs[PT(p)->nifs++] = iface_new(&f[i]);
        if (usable && !f[i].loopback) { up = 1; if (f[i].ip) PT(p)->v4 = 1; if (f[i].has_g6) PT(p)->v6 = 1; PT(p)->dns = 1; }
        s = (s ^ (f[i].ip + f[i].index * 31u + (unsigned)usable * 7u + (unsigned)f[i].has_g6)) * 1099511628211ull;
    }
    /* com interface exigida só de loopback (ex.: listener local), o caminho também vale */
    if (!up && req_type == IT_LOOPBACK) up = 1;
    PT(p)->status = up ? PS_SATISFIED : PS_UNSATISFIED;
    if (sig) *sig = s ^ (unsigned)up;
    return p;
}
int nw_path_get_status(void *p) { return p ? PT(p)->status : PS_INVALID; }
int nw_path_get_unsatisfied_reason(void *p) { (void)p; return 0; }   /* nw_path_unsatisfied_reason_not_available */
u8 nw_path_is_expensive(void *p) { (void)p; return 0; }          /* cabo e Wi-Fi: não é rede tarifada */
u8 nw_path_is_constrained(void *p) { (void)p; return 0; }
u8 nw_path_has_ipv4(void *p) { return p && PT(p)->v4; }
u8 nw_path_has_ipv6(void *p) { return p && PT(p)->v6; }
u8 nw_path_has_dns(void *p) { return p && PT(p)->dns; }
u8 nw_path_uses_interface_type(void *p, int t) {
    if (!p) return 0;
    for (int i = 0; i < PT(p)->nifs; i++) if (nw_interface_get_type(PT(p)->ifs[i]) == t && (t == IT_LOOPBACK || IFC(PT(p)->ifs[i])->f.active)) return 1;
    return 0;
}
void nw_path_enumerate_interfaces(void *p, void *b) {
    if (!p || !b) return;
    for (int i = 0; i < PT(p)->nifs; i++) if (!((u8 (*)(void *, id))BLK(b)->invoke)(b, PT(p)->ifs[i])) break;
}
u8 nw_path_is_equal(void *a, void *b) { return a && b && PT(a)->status == PT(b)->status && PT(a)->nifs == PT(b)->nifs && PT(a)->v4 == PT(b)->v4 && PT(a)->v6 == PT(b)->v6; }
id nw_path_copy_effective_local_endpoint(void *p) { return p ? retain(PT(p)->local) : 0; }
id nw_path_copy_effective_remote_endpoint(void *p) { return p ? retain(PT(p)->remote) : 0; }
void nw_path_enumerate_gateways(void *p, void *b) { (void)p; (void)b; }

typedef struct { cx_class_t *isa; void *queue, *update, *cancel_h; int req_type, proh[5], nproh, running, cancelled; unsigned long long sig; } NWMonitor_t;
#define MO(o) ((NWMonitor_t *)(o))
static void mo_dealloc(id s, SEL c) { brel(MO(s)->update); brel(MO(s)->cancel_h); if (MO(s)->queue) dispatch_release(MO(s)->queue); IMP sup = cx_lookup(&OS_nw_object_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c); }
id nw_path_monitor_create(void) { id m = nwnew(&OS_nw_path_monitor_c, sizeof(NWMonitor_t)); MO(m)->req_type = -1; return m; }
id nw_path_monitor_create_with_type(int t) { id m = nw_path_monitor_create(); MO(m)->req_type = t; return m; }
void nw_path_monitor_prohibit_interface_type(void *m, int t) { if (m && MO(m)->nproh < 5) MO(m)->proh[MO(m)->nproh++] = t; }
void nw_path_monitor_set_queue(void *m, void *q) { if (!m) return; if (q) dispatch_retain(q); if (MO(m)->queue) dispatch_release(MO(m)->queue); MO(m)->queue = q; }
void nw_path_monitor_set_update_handler(void *m, void *b) { if (!m) return; brel(MO(m)->update); MO(m)->update = bcopy_(b); }
void nw_path_monitor_set_cancel_handler(void *m, void *b) { if (!m) return; brel(MO(m)->cancel_h); MO(m)->cancel_h = bcopy_(b); }
typedef struct { id obj; id arg; void *blk; } mev_t;
static void mev_update(void *x) { mev_t *e = x; NWMonitor_t *m = MO(e->obj); if (!m->cancelled && m->update) ((void (*)(void *, id))BLK(m->update)->invoke)(m->update, e->arg); rel(e->arg); rel(e->obj); free(e); }
static void mev_cancel(void *x) { mev_t *e = x; NWMonitor_t *m = MO(e->obj); if (m->cancel_h) ((void (*)(void *))BLK(m->cancel_h)->invoke)(m->cancel_h); rel(e->obj); free(e); }
static void *mo_thread(void *arg) {
    id m = arg;
    int first = 1;
    while (!MO(m)->cancelled) {
        unsigned long long sig;
        id p = path_snapshot(MO(m)->req_type, MO(m)->proh, MO(m)->nproh, &sig);
        if (first || sig != MO(m)->sig) {
            MO(m)->sig = sig; first = 0;
            mev_t *e = calloc(1, sizeof *e); e->obj = retain(m); e->arg = p;
            dispatch_async_f(MO(m)->queue ? MO(m)->queue : dispatch_get_main_queue(), e, mev_update);
        } else rel(p);
        for (int i = 0; i < 20 && !MO(m)->cancelled; i++) usleep(100000);     /* olha a cada 2 s */
    }
    mev_t *e = calloc(1, sizeof *e); e->obj = retain(m);
    dispatch_async_f(MO(m)->queue ? MO(m)->queue : dispatch_get_main_queue(), e, mev_cancel);
    rel(m);
    return 0;
}
void nw_path_monitor_start(void *m) {
    if (!m || MO(m)->running) return;
    MO(m)->running = 1;
    retain((id)m);
    unsigned long t;
    if (!pthread_create(&t, 0, mo_thread, m)) pthread_detach(t); else rel((id)m);
}
void nw_path_monitor_cancel(void *m) { if (m) MO(m)->cancelled = 1; }

/* ---------------------------------------------------------------- contextos */
typedef struct { cx_class_t *isa; char *ident; int is_final; double expiration, relative_priority; id metas[4]; int nmetas; id antecedent; } NWCtx_t;
#define CT(o) ((NWCtx_t *)(o))
static void ct_dealloc(id s, SEL c) { free(CT(s)->ident); for (int i = 0; i < CT(s)->nmetas; i++) rel(CT(s)->metas[i]); rel(CT(s)->antecedent); IMP sup = cx_lookup(&OS_nw_object_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c); }
id nw_content_context_create(const char *ident) { id c = nwnew(&OS_nw_content_context_c, sizeof(NWCtx_t)); CT(c)->ident = dupc(ident ? ident : ""); CT(c)->relative_priority = 0.5; return c; }
/* NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, _FINAL_MESSAGE_CONTEXT e _DEFAULT_STREAM_CONTEXT: objetos globais */
id _nw_content_context_default_message, _nw_content_context_final_send, _nw_content_context_default_stream;
const char *nw_content_context_get_identifier(void *c) { return c ? CT(c)->ident : 0; }
u8 nw_content_context_get_is_final(void *c) { return c && CT(c)->is_final; }
void nw_content_context_set_is_final(void *c, u8 v) { if (c && c != (void *)_nw_content_context_default_message && c != (void *)_nw_content_context_default_stream) CT(c)->is_final = v; }
u64 nw_content_context_get_expiration_milliseconds(void *c) { return c ? (u64)CT(c)->expiration : 0; }
void nw_content_context_set_expiration_milliseconds(void *c, u64 v) { if (c) CT(c)->expiration = (double)v; }
double nw_content_context_get_relative_priority(void *c) { return c ? CT(c)->relative_priority : 0.5; }
void nw_content_context_set_relative_priority(void *c, double v) { if (c) CT(c)->relative_priority = v; }
void nw_content_context_set_metadata_for_protocol(void *c, void *m) { if (c && m && CT(c)->nmetas < 4) CT(c)->metas[CT(c)->nmetas++] = retain((id)m); }
id nw_content_context_copy_protocol_metadata(void *c, void *def) {
    for (int i = 0; c && i < CT(c)->nmetas; i++) {
        id m = CT(c)->metas[i];
        int kind = object_getClass(m) == &OS_sec_protocol_metadata_c ? PR_TLS : ((NWOptions_t *)m)->kind;
        if (!def || ((NWDef_t *)def)->kind == kind) return retain(m);
    }
    return 0;
}
void nw_content_context_foreach_protocol_metadata(void *c, void *b) {
    for (int i = 0; c && b && i < CT(c)->nmetas; i++) {
        id m = CT(c)->metas[i];
        int kind = object_getClass(m) == &OS_sec_protocol_metadata_c ? PR_TLS : ((NWOptions_t *)m)->kind;
        ((void (*)(void *, id, id))BLK(b)->invoke)(b, def_of(kind), m);
    }
}
void nw_content_context_set_antecedent(void *c, void *a) { if (!c) return; id o = CT(c)->antecedent; CT(c)->antecedent = retain((id)a); rel(o); }
id nw_content_context_copy_antecedent(void *c) { return c ? retain(CT(c)->antecedent) : 0; }

/* metadados de WebSocket (opcode e código de fechamento) */
typedef struct { cx_class_t *isa; int kind; int opcode, close_code; } NWWsMeta_t;
#define WSM(o) ((NWWsMeta_t *)(o))
enum { WSO_INVALID = -1, WSO_CONT = 0, WSO_TEXT = 1, WSO_BINARY = 2, WSO_CLOSE = 8, WSO_PING = 9, WSO_PONG = 10 };
id nw_ws_create_metadata(int opcode) { id m = nwnew(&OS_nw_protocol_metadata_c, sizeof(NWWsMeta_t)); WSM(m)->kind = PR_WS; WSM(m)->opcode = opcode; WSM(m)->close_code = 1000; return m; }
int nw_ws_metadata_get_opcode(void *m) { return m && object_getClass((id)m) == &OS_nw_protocol_metadata_c ? WSM(m)->opcode : WSO_INVALID; }
void nw_ws_metadata_set_close_code(void *m, int c) { if (m && object_getClass((id)m) == &OS_nw_protocol_metadata_c) WSM(m)->close_code = c; }
int nw_ws_metadata_get_close_code(void *m) { return m && object_getClass((id)m) == &OS_nw_protocol_metadata_c ? WSM(m)->close_code : 0; }
void nw_ws_metadata_set_pong_handler(void *m, void *q, void *b) { (void)m; (void)q; (void)b; }
u8 nw_protocol_metadata_is_ws(void *m) { return m && object_getClass((id)m) == &OS_nw_protocol_metadata_c && WSM(m)->kind == PR_WS; }
u8 nw_protocol_metadata_is_tls(void *m) { return m && object_getClass((id)m) == &OS_sec_protocol_metadata_c; }
u8 nw_protocol_metadata_is_tcp(void *m) { (void)m; return 0; }
u8 nw_protocol_metadata_is_udp(void *m) { (void)m; return 0; }
id nw_tls_copy_sec_protocol_metadata(void *m) { return nw_protocol_metadata_is_tls(m) ? retain((id)m) : 0; }

/* ---------------------------------------------------------------- conexões */
enum { CS_INVALID, CS_WAITING, CS_PREPARING, CS_READY, CS_FAILED, CS_CANCELLED };
typedef struct sendq { void *data; id ctx; int complete; void *done; struct sendq *next; } sendq_t;
typedef struct recvq { size_t min, max; int message; void *done; struct recvq *next; } recvq_t;
typedef struct {
    cx_class_t *isa;
    id endpoint, params, path, tls_meta, error;
    void *queue, *state_h, *viab_h, *better_h, *path_h;
    int state, started, cancel, force;
    /* transporte */
    void *tconn;                    /* mnet io (TCP/TLS) */
    int udp_fd;
    mnet_wsconn_t *ws;
    int accepted_fd;                /* conexão que veio de um listener */
    int is_ws, eof_in;
    sendq_t *sq, *sq_tail;
    recvq_t *rq, *rq_tail;
    mnet_buf_t inbuf;               /* bytes recebidos ainda não entregues (TCP) */
    struct dgram { mnet_buf_t b; int op; struct dgram *next; } *dq, *dq_tail;   /* datagramas/mensagens */
    u8 local_sa[28], remote_sa[28];
    unsigned long thread;
} NWConn_t;
#define CN(o) ((NWConn_t *)(o))
static void cn_dealloc(id s, SEL c) {
    NWConn_t *n = CN(s);
    rel(n->endpoint); rel(n->params); rel(n->path); rel(n->tls_meta); rel(n->error);
    brel(n->state_h); brel(n->viab_h); brel(n->better_h); brel(n->path_h);
    if (n->queue) dispatch_release(n->queue);
    mnet_buf_free(&n->inbuf);
    while (n->dq) { struct dgram *d = n->dq; n->dq = d->next; mnet_buf_free(&d->b); free(d); }
    IMP sup = cx_lookup(&OS_nw_object_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c);
}
id nw_connection_create(void *endpoint, void *params) {
    if (!endpoint || !params) return 0;
    id c = nwnew(&OS_nw_connection_c, sizeof(NWConn_t));
    CN(c)->endpoint = retain((id)endpoint);
    CN(c)->params = nw_parameters_copy(params);
    CN(c)->udp_fd = -1; CN(c)->accepted_fd = -1;
    CN(c)->state = CS_INVALID;
    return c;
}
void nw_connection_set_queue(void *c, void *q) { if (!c) return; if (q) dispatch_retain(q); if (CN(c)->queue) dispatch_release(CN(c)->queue); CN(c)->queue = q; }
void nw_connection_set_state_changed_handler(void *c, void *b) { if (!c) return; brel(CN(c)->state_h); CN(c)->state_h = bcopy_(b); }
void nw_connection_set_viability_changed_handler(void *c, void *b) { if (!c) return; brel(CN(c)->viab_h); CN(c)->viab_h = bcopy_(b); }
void nw_connection_set_better_path_available_handler(void *c, void *b) { if (!c) return; brel(CN(c)->better_h); CN(c)->better_h = bcopy_(b); }
void nw_connection_set_path_changed_handler(void *c, void *b) { if (!c) return; brel(CN(c)->path_h); CN(c)->path_h = bcopy_(b); }
id nw_connection_copy_endpoint(void *c) { return c ? retain(CN(c)->endpoint) : 0; }
id nw_connection_copy_parameters(void *c) { return c ? retain(CN(c)->params) : 0; }
id nw_connection_copy_current_path(void *c) { LK(); id p = c ? retain(CN(c)->path) : 0; ULK(); return p; }
u32 nw_connection_get_maximum_datagram_size(void *c) { return c && PA(CN(c)->params)->udp ? 1472 : 0; }
id nw_connection_copy_protocol_metadata(void *c, void *def) {
    if (!c) return 0;
    if (def && ((NWDef_t *)def)->kind == PR_TLS) return retain(CN(c)->tls_meta);
    if (def && ((NWDef_t *)def)->kind == PR_WS && CN(c)->is_ws) return nw_ws_create_metadata(WSO_BINARY);
    return 0;
}
id nw_connection_copy_description(void *c) { (void)c; return 0; }
static void *cq(id c) { return CN(c)->queue ? CN(c)->queue : dispatch_get_main_queue(); }
typedef struct { id c; int state; id err; } sev_t;
static void sev_run(void *x) {
    sev_t *e = x;
    if (CN(e->c)->state_h) ((void (*)(void *, int, id))BLK(CN(e->c)->state_h)->invoke)(CN(e->c)->state_h, e->state, e->err);
    if (e->state == CS_READY && CN(e->c)->viab_h) ((void (*)(void *, u8))BLK(CN(e->c)->viab_h)->invoke)(CN(e->c)->viab_h, 1);
    if (e->state == CS_READY && CN(e->c)->path_h && CN(e->c)->path) ((void (*)(void *, id))BLK(CN(e->c)->path_h)->invoke)(CN(e->c)->path_h, CN(e->c)->path);
    rel(e->err); rel(e->c); free(e);
}
static void set_state(id c, int st, id err) {
    if (CN(c)->state == st || CN(c)->state == CS_CANCELLED) { rel(err); return; }
    if (CN(c)->state == CS_FAILED && st != CS_CANCELLED) { rel(err); return; }
    CN(c)->state = st;
    if (err) { rel(CN(c)->error); CN(c)->error = retain(err); }
    sev_t *e = calloc(1, sizeof *e); e->c = retain(c); e->state = st; e->err = err;
    dispatch_async_f(cq(c), e, sev_run);
}
/* constante NW_CONNECTION_SEND_IDEMPOTENT_CONTENT: bloco global que não faz nada */
blk_t _nw_connection_send_idempotent_content = {_NSConcreteGlobalBlock, 1 << 28, 0, (void *)cfg_noop, &blk_desc};
void nw_connection_send(void *c, void *content, void *ctx, u8 is_complete, void *done) {
    if (!c) return;
    sendq_t *s = calloc(1, sizeof *s);
    if (content) { dispatch_retain(content); s->data = content; }
    s->ctx = retain(ctx ? (id)ctx : _nw_content_context_default_message);
    s->complete = is_complete;
    s->done = done == &_nw_connection_send_idempotent_content ? 0 : bcopy_(done);
    LK(); if (CN(c)->sq_tail) CN(c)->sq_tail->next = s; else CN(c)->sq = s; CN(c)->sq_tail = s; ULK();
}
void nw_connection_receive(void *c, u32 min, u32 max, void *done) {
    if (!c || !done) return;
    recvq_t *r = calloc(1, sizeof *r);
    r->min = min ? min : 1; r->max = max ? max : 65536; r->done = bcopy_(done);
    LK(); if (CN(c)->rq_tail) CN(c)->rq_tail->next = r; else CN(c)->rq = r; CN(c)->rq_tail = r; ULK();
}
void nw_connection_receive_message(void *c, void *done) {
    if (!c || !done) return;
    recvq_t *r = calloc(1, sizeof *r);
    r->min = 1; r->max = (size_t)-1; r->message = 1; r->done = bcopy_(done);
    LK(); if (CN(c)->rq_tail) CN(c)->rq_tail->next = r; else CN(c)->rq = r; CN(c)->rq_tail = r; ULK();
}
void nw_connection_batch(void *c, void *b) { (void)c; if (b) ((void (*)(void *))BLK(b)->invoke)(b); }
typedef struct { void *blk; id err; void *data; id ctx; int complete; int is_send; } cev_t;
static void cev_run(void *x) {
    cev_t *e = x;
    if (e->is_send) ((void (*)(void *, id))BLK(e->blk)->invoke)(e->blk, e->err);
    else ((void (*)(void *, void *, id, u8, id))BLK(e->blk)->invoke)(e->blk, e->data, e->ctx, (u8)e->complete, e->err);
    brel(e->blk); rel(e->err); rel(e->ctx);
    if (e->data) dispatch_release(e->data);
    free(e);
}
static void post_send_done(id c, sendq_t *s, id err) {
    if (s->done) { cev_t *e = calloc(1, sizeof *e); e->blk = s->done; e->err = retain(err); e->is_send = 1; dispatch_async_f(cq(c), e, cev_run); }
    if (s->data) dispatch_release(s->data);
    rel(s->ctx); free(s);
}
static void post_recv(id c, recvq_t *r, const u8 *d, size_t n, id ctx, int complete, id err) {
    cev_t *e = calloc(1, sizeof *e);
    e->blk = r->done;
    e->data = d || n ? dispatch_data_create(d ? (const void *)d : "", n, 0, 0) : 0;
    e->ctx = retain(ctx); e->complete = complete; e->err = retain(err);
    dispatch_async_f(cq(c), e, cev_run);
    free(r);
}
static void ws_in(void *x, int op, const u8 *d, size_t n);
static int nwws_msg(void *ctx, int op, const u8 *d, size_t n) { ws_in(ctx, op, d, n); return 0; }
static void ws_in(void *x, int op, const u8 *d, size_t n) {
    id c = x;
    struct dgram *g = calloc(1, sizeof *g);
    mnet_buf_put(&g->b, d, n);
    g->op = op;
    LK(); if (CN(c)->dq_tail) CN(c)->dq_tail->next = g; else CN(c)->dq = g; CN(c)->dq_tail = g; ULK();
}
static void nwws_rand(void *ctx, uint8_t *b, size_t n) { (void)ctx; arc4random_buf(b, n); }
/* entrega o que já dá para entregar */
static void pump_recv(id c) {
    NWConn_t *n = CN(c);
    for (;;) {
        LK();
        recvq_t *r = n->rq;
        if (!r) { ULK(); return; }
        int dgram = PA(n->params)->udp || n->is_ws;
        if (dgram) {
            struct dgram *g = n->dq;
            if (!g && !n->eof_in) { ULK(); return; }
            n->rq = r->next; if (!n->rq) n->rq_tail = 0;
            if (g) { n->dq = g->next; if (!n->dq) n->dq_tail = 0; }
            ULK();
            if (g) {
                id ctx = nw_content_context_create("nw");
                CT(ctx)->is_final = 0;
                if (n->is_ws) { id m = nw_ws_create_metadata(g->op == MNET_WS_TEXT ? WSO_TEXT : WSO_BINARY); nw_content_context_set_metadata_for_protocol(ctx, m); rel(m); }
                size_t k = g->b.n < r->max ? g->b.n : r->max;
                post_recv(c, r, g->b.p, k, ctx, 1, 0);
                rel(ctx);
                mnet_buf_free(&g->b); free(g);
            } else post_recv(c, r, 0, 0, _nw_content_context_final_send, 1, 0);
            continue;
        }
        /* fluxo: receive_message espera o fim; receive entrega entre min e max */
        int ready = r->message ? n->eof_in : (n->inbuf.n >= r->min || (n->eof_in));
        if (!ready) { ULK(); return; }
        n->rq = r->next; if (!n->rq) n->rq_tail = 0;
        size_t k = n->inbuf.n < r->max ? n->inbuf.n : r->max;
        mnet_buf_t take = {0};
        mnet_buf_put(&take, n->inbuf.p, k);
        mnet_buf_consume(&n->inbuf, k);
        int fin = n->eof_in && !n->inbuf.n;
        ULK();
        post_recv(c, r, take.p, take.n, fin ? _nw_content_context_final_send : _nw_content_context_default_stream, fin, 0);
        mnet_buf_free(&take);
    }
}
static void fail_pending(id c, id err) {
    NWConn_t *n = CN(c);
    for (;;) { LK(); sendq_t *s = n->sq; if (s) { n->sq = s->next; if (!n->sq) n->sq_tail = 0; } ULK(); if (!s) break; post_send_done(c, s, err); }
    for (;;) { LK(); recvq_t *r = n->rq; if (r) { n->rq = r->next; if (!n->rq) n->rq_tail = 0; } ULK(); if (!r) break; post_recv(c, r, 0, 0, 0, 0, err); }
}
void *dispatch_semaphore_create(long v);
long dispatch_semaphore_wait(void *s, unsigned long long t);
long dispatch_semaphore_signal(void *s);
typedef struct { blk_t b; void *sem; int ok; } vb_t;
static void vb_complete(void *b, u8 ok) { vb_t *v = b; v->ok = ok; dispatch_semaphore_signal(v->sem); }
typedef struct { void *blk; id meta; void *trust; vb_t *done; } vrun_t;
static void verify_run(void *x) {
    vrun_t *v = x;
    ((void (*)(void *, id, void *, void *))BLK(v->blk)->invoke)(v->blk, v->meta, v->trust, v->done);
    free(v);
}
static long io_write_all(id c, const u8 *d, size_t len) {
    NWConn_t *n = CN(c);
    size_t off = 0;
    while (off < len) {
        long k;
        if (n->udp_fd >= 0) return send(n->udp_fd, d, len, 0);
        k = cx_mnet_io()->write(0, n->tconn, d + off, len - off, 30000);
        if (k <= 0) return -1;
        off += (size_t)k;
    }
    return (long)off;
}
static void *conn_thread(void *arg) {
    id c = arg;
    NWConn_t *n = CN(c);
    void *pool = objc_autoreleasePoolPush();
    const mnet_io_t *io = cx_mnet_io();
    id tls = find_app(n->params, PR_TLS), ws = find_app(n->params, PR_WS);
    n->is_ws = ws != 0;
    set_state(c, CS_PREPARING, 0);
    /* caminho */
    unsigned long long sig;
    id path = path_snapshot(PA(n->params)->req_type, PA(n->params)->proh, PA(n->params)->nproh, &sig);
    LK(); rel(n->path); n->path = path; ULK();
    char host[256] = ""; int port = n->endpoint ? EP(n->endpoint)->port : 0;
    if (n->endpoint) snprintf(host, sizeof host, "%s", EP(n->endpoint)->host ? EP(n->endpoint)->host : "");
    int err = 0;
    if (n->accepted_fd >= 0) {
        /* veio do listener: o socket já está conectado */
        int fl = fcntl(n->accepted_fd, 3, 0); fcntl(n->accepted_fd, 4, fl | 4);
        if (PA(n->params)->udp) n->udp_fd = n->accepted_fd;
        else { typedef struct { int fd; void *ssl; } io_conn_t; io_conn_t *ic = calloc(1, sizeof *ic); ic->fd = n->accepted_fd; n->tconn = ic; }
        n->accepted_fd = -1;
        if (tls) err = MNET_E_TLS;                              /* TLS de servidor: o motor ainda não tem */
    } else if (!host[0] || EP(n->endpoint)->type == EP_BONJOUR) {
        err = MNET_E_NO_HOST;
    } else if (PA(n->params)->udp) {
        char ps[8]; snprintf(ps, sizeof ps, "%d", port);
        struct mos_ai hint, *res = 0;
        memset(&hint, 0, sizeof hint); hint.socktype = 2;
        if (getaddrinfo(host, ps, &hint, (void **)&res) || !res) err = MNET_E_NO_HOST;
        else {
            n->udp_fd = socket(res->family, 2, 17);
            if (n->udp_fd < 0 || connect(n->udp_fd, res->addr, res->addrlen)) err = MNET_E_CONNECT;
            else { int fl = fcntl(n->udp_fd, 3, 0); fcntl(n->udp_fd, 4, fl | 4); }
            freeaddrinfo(res);
        }
    } else if (ws) {
        n->ws = calloc(1, sizeof *n->ws);
        n->ws->ws.ctx = c; n->ws->ws.on_message = nwws_msg; n->ws->ws.random = nwws_rand;
        n->ws->ws.max_msg = PO(ws)->max_msg;
        char url[600];
        const char *path_s = EP(n->endpoint)->type == EP_URL ? 0 : "/";
        if (EP(n->endpoint)->type == EP_URL) snprintf(url, sizeof url, "%s", EP(n->endpoint)->url);
        else snprintf(url, sizeof url, "%s://%s:%d%s", tls ? "wss" : "ws", host, port, path_s);
        mnet_buf_t pb = {0};
        for (int i = 0; i < PO(ws)->nprotos; i++) { if (i) mnet_buf_put(&pb, ", ", 2); mnet_buf_puts(&pb, PO(ws)->ws_protos[i]); }
        mnet_req_t q = {0};
        q.url = url; q.hdrs = &PO(ws)->ws_hdrs; q.timeout_ms = 30000; q.no_cookies = 1; q.cancel = &n->cancel;
        char msg[160];
        err = mnet_ws_open(cx_mnet_client(), &q, (char *)pb.p, n->ws, msg, sizeof msg);
        mnet_buf_free(&pb);
        if (!err) n->tconn = n->ws->conn;
    } else {
        int e = 0;
        n->tconn = io->connect(0, host, port, PO(ST(PA(n->params)->stack)->transport)->conn_timeout > 0 ? PO(ST(PA(n->params)->stack)->transport)->conn_timeout * 1000 : 30000, &e);
        if (!n->tconn) err = e ? e : MNET_E_CONNECT;
        else if (tls) {
            SecOpts_t *so = SO(PO(tls)->sec);
            mnet_buf_t al = {0};
            LK();
            for (int i = 0; i < so->nalpn; i++) { size_t l = strlen(so->alpn[i]); if (l && l < 256) { mnet_buf_putc(&al, (int)l); mnet_buf_put(&al, so->alpn[i], l); } }
            ULK();
            char got[33] = "", msg[160];
            err = io->starttls(0, n->tconn, so->sni[0] ? so->sni : host, al.p, al.n, got, 30000, msg, sizeof msg);
            mnet_buf_free(&al);
            if (!err) {
                id m = nwnew(&OS_sec_protocol_metadata_c, sizeof(SecMeta_t));
                memcpy(SM(m)->alpn, got, sizeof got);
                snprintf(SM(m)->sni, sizeof SM(m)->sni, "%s", so->sni[0] ? so->sni : host);
                SM(m)->version = 0x0304;
                n->tls_meta = m;
                /* bloco de verificação do app (pinning): o kernel já validou a cadeia; o app ainda pode recusar */
                if (so->verify_block) {
                    void *tr = io->copy_trust ? io->copy_trust(0, n->tconn) : 0;
                    vb_t *vb = calloc(1, sizeof *vb);
                    vb->b.isa = _NSConcreteGlobalBlock; vb->b.flags = 1 << 28; vb->b.invoke = (void *)vb_complete; vb->b.desc = &blk_desc;
                    vb->sem = dispatch_semaphore_create(0);
                    vrun_t *vr = calloc(1, sizeof *vr);
                    vr->blk = so->verify_block; vr->meta = m; vr->trust = tr; vr->done = vb;
                    dispatch_async_f(so->verify_queue ? so->verify_queue : dispatch_get_global_queue(0, 0), vr, verify_run);
                    dispatch_semaphore_wait(vb->sem, ~0ull);
                    if (!vb->ok) err = MNET_E_CERT_UNTRUSTED;
                    /* vb fica vivo: o app pode ter copiado o bloco (é global, Block_copy devolve o mesmo) */
                    rel((id)tr);
                }
            }
        }
    }
    if (err) {
        int dom = err == MNET_E_TLS || err == MNET_E_CERT_UNTRUSTED ? ED_TLS : err == MNET_E_NO_HOST ? ED_DNS : ED_POSIX;
        int code = dom == ED_TLS ? -9807 : dom == ED_DNS ? -65554 : mnet_to_posix(err);
        id e = err_new(dom, code);
        fail_pending(c, e);
        set_state(c, n->cancel ? CS_CANCELLED : CS_FAILED, e);
        goto out;
    }
    set_state(c, CS_READY, 0);
    u8 buf[32768];
    while (!n->cancel) {
        /* envios */
        for (;;) {
            LK(); sendq_t *s = n->sq; if (s) { n->sq = s->next; if (!n->sq) n->sq_tail = 0; } ULK();
            if (!s) break;
            const void *p = 0; size_t len = 0;
            void *map = s->data ? dispatch_data_create_map(s->data, &p, &len) : 0;
            id e = 0;
            if (n->is_ws) {
                int op = MNET_WS_BINARY;
                id m = nw_content_context_copy_protocol_metadata(s->ctx, def_of(PR_WS));
                if (m) { int o = WSM(m)->opcode; op = o == WSO_TEXT ? MNET_WS_TEXT : o == WSO_PING ? MNET_WS_PING : o == WSO_PONG ? MNET_WS_PONG : o == WSO_CLOSE ? MNET_WS_CLOSE : MNET_WS_BINARY;
                         if (op == MNET_WS_CLOSE) mnet_ws_close(&n->ws->ws, WSM(m)->close_code, 0); rel(m); }
                if (op == MNET_WS_PING) mnet_ws_ping(&n->ws->ws, p, len);
                else if (op != MNET_WS_CLOSE && (len || s->complete)) mnet_ws_send(&n->ws->ws, op, p, len);
                if (mnet_wsconn_flush(n->ws, 30000)) e = err_new(ED_POSIX, 32);
            } else if (len && io_write_all(c, p, len) < 0) e = err_new(ED_POSIX, 32);   /* EPIPE */
            if (map) dispatch_release(map);
            post_send_done(c, s, e);
            rel(e);
        }
        /* leitura */
        if (!n->eof_in) {
            long k;
            if (n->is_ws) {
                k = mnet_wsconn_poll(n->ws, 50);
                if (k < 0 || n->ws->ws.close_recv) n->eof_in = 1;
            } else if (n->udp_fd >= 0) {
                pfd_t pf = {n->udp_fd, 1, 0};
                k = poll(&pf, 1, 50) > 0 ? recv(n->udp_fd, buf, sizeof buf, 0) : 0;
                if (k > 0) ws_in(c, 0, buf, (size_t)k);
            } else {
                k = io->read(0, n->tconn, buf, sizeof buf, 50);
                if (k > 0) { LK(); mnet_buf_put(&n->inbuf, buf, (size_t)k); ULK(); }
                else if (k == 0 || k == -1) n->eof_in = 1;
            }
        } else usleep(20000);
        pump_recv(c);
    }
    fail_pending(c, 0);
    set_state(c, CS_CANCELLED, 0);
out:
    if (n->ws) { mnet_wsconn_free(n->ws); free(n->ws); n->ws = 0; n->tconn = 0; }
    if (n->tconn) { io->close(0, n->tconn); n->tconn = 0; }
    if (n->udp_fd >= 0) { close(n->udp_fd); n->udp_fd = -1; }
    objc_autoreleasePoolPop(pool);
    rel(c);
    return 0;
}
void nw_connection_start(void *c) {
    if (!c || CN(c)->started) return;
    CN(c)->started = 1;
    retain((id)c);
    unsigned long t;
    if (!pthread_create(&t, 0, conn_thread, c)) pthread_detach(t); else { set_state((id)c, CS_FAILED, err_new(ED_POSIX, 35)); rel((id)c); }
}
void nw_connection_restart(void *c) { (void)c; }        /* o estado "waiting" não é usado: falha vai direto para failed */
void nw_connection_cancel(void *c) {
    if (!c) return;
    CN(c)->cancel = 1;
    if (!CN(c)->started) { CN(c)->started = 1; set_state((id)c, CS_CANCELLED, 0); }
}
void nw_connection_force_cancel(void *c) { if (c) CN(c)->force = 1; nw_connection_cancel(c); }
void nw_connection_cancel_current_endpoint(void *c) { nw_connection_cancel(c); }
void nw_connection_access_establishment_report(void *c, void *q, void *b) { (void)c; if (b) ((void (*)(void *, id))BLK(b)->invoke)(b, 0); (void)q; }

/* ---------------------------------------------------------------- listener */
enum { LS_INVALID, LS_WAITING, LS_READY, LS_FAILED, LS_CANCELLED };
typedef struct { cx_class_t *isa; id params; void *queue, *state_h, *conn_h, *adv_h; int fd, port, state, cancel, started, limit; } NWListener_t;
#define LI(o) ((NWListener_t *)(o))
static void li_dealloc(id s, SEL c) {
    rel(LI(s)->params); brel(LI(s)->state_h); brel(LI(s)->conn_h); brel(LI(s)->adv_h);
    if (LI(s)->queue) dispatch_release(LI(s)->queue);
    IMP sup = cx_lookup(&OS_nw_object_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static id listener_new(const char *port, void *params) {
    if (!params) return 0;
    id l = nwnew(&OS_nw_listener_c, sizeof(NWListener_t));
    LI(l)->params = nw_parameters_copy(params);
    LI(l)->port = port ? atoi(port) : 0;
    LI(l)->fd = -1;
    LI(l)->limit = -1;
    return l;
}
id nw_listener_create(void *params) { id l = listener_new(0, params); if (l && PA(LI(l)->params)->local) LI(l)->port = EP(PA(LI(l)->params)->local)->port; return l; }
id nw_listener_create_with_port(const char *port, void *params) { return listener_new(port, params); }
void nw_listener_set_queue(void *l, void *q) { if (!l) return; if (q) dispatch_retain(q); if (LI(l)->queue) dispatch_release(LI(l)->queue); LI(l)->queue = q; }
void nw_listener_set_state_changed_handler(void *l, void *b) { if (!l) return; brel(LI(l)->state_h); LI(l)->state_h = bcopy_(b); }
void nw_listener_set_new_connection_handler(void *l, void *b) { if (!l) return; brel(LI(l)->conn_h); LI(l)->conn_h = bcopy_(b); }
void nw_listener_set_advertised_endpoint_changed_handler(void *l, void *b) { if (!l) return; brel(LI(l)->adv_h); LI(l)->adv_h = bcopy_(b); }
void nw_listener_set_advertise_descriptor(void *l, void *d) { (void)l; (void)d; }      /* anúncio Bonjour: etapa de mDNS */
void nw_listener_set_new_connection_limit(void *l, u32 n) { if (l) LI(l)->limit = (int)n; }
u32 nw_listener_get_new_connection_limit(void *l) { return l ? (u32)LI(l)->limit : 0; }
unsigned short nw_listener_get_port(void *l) { return l ? (unsigned short)LI(l)->port : 0; }
static void *lq(id l) { return LI(l)->queue ? LI(l)->queue : dispatch_get_main_queue(); }
typedef struct { id l; int state; id err; id conn; } lev_t;
static void lev_run(void *x) {
    lev_t *e = x;
    if (e->conn) { if (LI(e->l)->conn_h) ((void (*)(void *, id))BLK(LI(e->l)->conn_h)->invoke)(LI(e->l)->conn_h, e->conn); }
    else if (LI(e->l)->state_h) ((void (*)(void *, int, id))BLK(LI(e->l)->state_h)->invoke)(LI(e->l)->state_h, e->state, e->err);
    rel(e->conn); rel(e->err); rel(e->l); free(e);
}
static void lstate(id l, int st, id err) {
    if (LI(l)->state == st) { rel(err); return; }
    LI(l)->state = st;
    lev_t *e = calloc(1, sizeof *e); e->l = retain(l); e->state = st; e->err = err;
    dispatch_async_f(lq(l), e, lev_run);
}
static void *listener_thread(void *arg) {
    id l = arg;
    NWListener_t *L = LI(l);
    int udp = PA(L->params)->udp;
    /* IPv6 com IPv4 mapeado (o mesmo socket aceita os dois) */
    u8 sa[28] = {28, 30};
    sa[2] = (u8)(L->port >> 8); sa[3] = (u8)L->port;
    if (PA(L->params)->local && EP(PA(L->params)->local)->type == EP_ADDRESS && EP(PA(L->params)->local)->sa[1] == 2) {
        memset(sa, 0, sizeof sa); memcpy(sa, EP(PA(L->params)->local)->sa, 16); sa[2] = (u8)(L->port >> 8); sa[3] = (u8)L->port;
    }
    L->fd = socket(sa[1], udp ? 2 : 1, udp ? 17 : 6);
    int one = 1;
    if (L->fd >= 0 && PA(L->params)->reuse) setsockopt(L->fd, 0xffff, 4, &one, sizeof one);       /* SO_REUSEADDR */
    if (L->fd < 0 || bind(L->fd, sa, sa[0]) || (!udp && listen(L->fd, 16))) {
        lstate(l, LS_FAILED, err_new(ED_POSIX, *__error() ? *__error() : 48));
        if (L->fd >= 0) close(L->fd);
        rel(l);
        return 0;
    }
    u8 got[28]; unsigned gl = sizeof got;
    if (!getsockname(L->fd, got, &gl)) L->port = got[2] << 8 | got[3];
    int fl = fcntl(L->fd, 3, 0); fcntl(L->fd, 4, fl | 4);
    lstate(l, LS_READY, 0);
    while (!L->cancel) {
        pfd_t pf = {L->fd, 1, 0};
        if (poll(&pf, 1, 100) <= 0) continue;
        if (L->limit == 0) { usleep(50000); continue; }
        u8 peer[28]; unsigned pl = sizeof peer;
        int cfd;
        if (udp) {
            /* UDP: cada remetente novo vira uma conexão (socket conectado ao remetente na mesma porta) */
            u8 d[2048];
            long k = recvfrom(L->fd, d, sizeof d, 0, peer, &pl);
            if (k < 0) continue;
            cfd = socket(peer[1], 2, 17);
            setsockopt(cfd, 0xffff, 0x200, &one, sizeof one);   /* SO_REUSEPORT */
            bind(cfd, got, got[0]);
            connect(cfd, peer, pl);
            id c = nw_connection_create(nw_endpoint_create_address(peer), L->params);
            CN(c)->accepted_fd = cfd;
            ws_in(c, 0, d, (size_t)k);
            lev_t *e = calloc(1, sizeof *e); e->l = retain(l); e->conn = c;
            dispatch_async_f(lq(l), e, lev_run);
        } else {
            cfd = accept(L->fd, peer, &pl);
            if (cfd < 0) continue;
            id ep = nw_endpoint_create_address(peer);
            id c = nw_connection_create(ep, L->params);
            rel(ep);
            CN(c)->accepted_fd = cfd;
            lev_t *e = calloc(1, sizeof *e); e->l = retain(l); e->conn = c;
            dispatch_async_f(lq(l), e, lev_run);
        }
        if (L->limit > 0) L->limit--;
    }
    close(L->fd); L->fd = -1;
    lstate(l, LS_CANCELLED, 0);
    rel(l);
    return 0;
}
void nw_listener_start(void *l) {
    if (!l || LI(l)->started) return;
    LI(l)->started = 1;
    retain((id)l);
    unsigned long t;
    if (!pthread_create(&t, 0, listener_thread, l)) pthread_detach(t); else rel((id)l);
}
void nw_listener_cancel(void *l) { if (l) LI(l)->cancel = 1; }

/* ---------------------------------------------------------------- registro das classes */
void cx_nw_init(void) { nw_init_classes(); }       /* chamado no início do processo (nsurlsession.c): os contextos globais já valem */
static void nw_init_classes(void) {
    if (g_ready) return;
    LK();
    if (!g_ready) {
        cx_class_t *ob = cx_class_named("NSObject");
        cx_define_class(&OS_nw_object_c, &OS_nw_object_m, "OS_nw_object", ob, 0);
#define D(N, T) cx_define_class(&N##_c, &N##_m, #N, &OS_nw_object_c, (u32)(sizeof(T) - 8))
        D(OS_nw_endpoint, NWEndpoint_t); D(OS_nw_parameters, NWParams_t); D(OS_nw_protocol_stack, NWStack_t);
        D(OS_nw_protocol_options, NWOptions_t); D(OS_nw_protocol_definition, NWDef_t); D(OS_nw_protocol_metadata, NWWsMeta_t);
        D(OS_nw_connection, NWConn_t); D(OS_nw_listener, NWListener_t); D(OS_nw_path, NWPath_t); D(OS_nw_path_monitor, NWMonitor_t);
        D(OS_nw_interface, NWIface_t); D(OS_nw_content_context, NWCtx_t); D(OS_nw_error, NWError_t);
        D(OS_sec_protocol_options, SecOpts_t); D(OS_sec_protocol_metadata, SecMeta_t);
        D(OS_nw_ws_request, NWDef_t); D(OS_nw_ws_response, NWDef_t);
#undef D
        cx_add_method(&OS_nw_endpoint_c, "dealloc", (void *)ep_dealloc); cx_add_method(&OS_nw_endpoint_c, "description", (void *)ep_desc);
        cx_add_method(&OS_nw_protocol_options_c, "dealloc", (void *)po_dealloc);
        cx_add_method(&OS_sec_protocol_options_c, "dealloc", (void *)so_dealloc);
        cx_add_method(&OS_nw_protocol_stack_c, "dealloc", (void *)st_dealloc);
        cx_add_method(&OS_nw_parameters_c, "dealloc", (void *)pa_dealloc);
        cx_add_method(&OS_nw_path_c, "dealloc", (void *)pt_dealloc);
        cx_add_method(&OS_nw_path_monitor_c, "dealloc", (void *)mo_dealloc);
        cx_add_method(&OS_nw_content_context_c, "dealloc", (void *)ct_dealloc);
        cx_add_method(&OS_nw_connection_c, "dealloc", (void *)cn_dealloc);
        cx_add_method(&OS_nw_listener_c, "dealloc", (void *)li_dealloc);
        cx_add_method(&OS_nw_error_c, "description", (void *)err_desc);
        __sync_synchronize();
        g_ready = 1;
    }
    ULK();
    if (!_nw_content_context_default_message) {
        id a = nw_content_context_create("default_message"); CT(a)->is_final = 0;
        id b = nw_content_context_create("final_send"); CT(b)->is_final = 1;
        id s = nw_content_context_create("default_stream"); CT(s)->is_final = 0;
        _nw_content_context_default_stream = s; _nw_content_context_final_send = b;
        __sync_synchronize();
        _nw_content_context_default_message = a;
    }
}
