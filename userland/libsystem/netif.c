/*
 * mOS — interfaces de rede para os apps: getifaddrs/freeifaddrs (AF_INET, AF_INET6 e AF_LINK com MAC e if_data),
 * if_nametoindex/if_indextoname/if_nameindex. A lista vem do kernel (MOS_NET_IFLIST): lo0 e, para cada placa
 * registrada, en0, en1... na ordem de registro, como o macOS. Só a interface ativa tem endereço e contadores.
 * cx_netif_list também serve ao SystemConfiguration (scnetwork.c) e ao Network.framework.
 */
#include "sys.h"
#include "libc.h"

typedef struct {
    char name[48];
    unsigned char mac[6], pad[2];
    unsigned int flags;
    unsigned int ip, mask, gw, dns;
    unsigned char ll6[16], global6[16];
    unsigned int prefix6, pad2;
    unsigned long long rx_bytes, tx_bytes, rx_packets, tx_packets;
} kif_t;                                                    /* = mos_net_if_t (kernel/include/syscall.h) */
enum { KIF_LINK = 1, KIF_ACTIVE = 2, KIF_WIFI = 4, KIF_CONF = 8, KIF_V6 = 16, KIF_V6G = 32 };

/* interface como os apps a veem */
typedef struct {
    char bsd[16];                 /* "lo0", "en0" */
    char driver[48];
    unsigned index, flags;        /* IFF_* */
    int loopback, wifi, link, active;
    unsigned char mac[6];
    unsigned ip, mask;            /* ordem de host */
    unsigned char ll6[16], global6[16]; int has_ll6, has_g6, prefix6;
    unsigned long long rx_bytes, tx_bytes, rx_packets, tx_packets;
} cx_netif_t;

#define IFF_UP 0x1
#define IFF_BROADCAST 0x2
#define IFF_LOOPBACK 0x8
#define IFF_RUNNING 0x40
#define IFF_SIMPLEX 0x800
#define IFF_MULTICAST 0x8000

/* preenche até max interfaces; devolve a quantidade (lo0 sempre primeiro) */
int cx_netif_list(cx_netif_t *out, int max) {
    int n = 0;
    if (max <= 0) return 0;
    memset(&out[0], 0, sizeof out[0]);
    strcpy(out[0].bsd, "lo0"); strcpy(out[0].driver, "Loopback");
    out[0].index = 1; out[0].loopback = 1; out[0].link = out[0].active = 1;
    out[0].flags = IFF_UP | IFF_LOOPBACK | IFF_RUNNING | IFF_MULTICAST;
    out[0].ip = 0x7f000001u; out[0].mask = 0xff000000u;
    out[0].ll6[0] = 0xfe; out[0].ll6[1] = 0x80; out[0].ll6[15] = 1; out[0].has_ll6 = 1;   /* fe80::1%lo0 */
    out[0].global6[15] = 1; out[0].has_g6 = 1; out[0].prefix6 = 128;                    /* ::1 */
    n = 1;
    kif_t k[8];
    long cnt = SYS3(SYS_NET, MOS_NET_IFLIST, (long)k, 8);
    for (long i = 0; i < cnt && n < max; i++, n++) {
        cx_netif_t *f = &out[n];
        memset(f, 0, sizeof *f);
        f->bsd[0] = 'e'; f->bsd[1] = 'n'; f->bsd[2] = (char)('0' + i); f->bsd[3] = 0;
        memcpy(f->driver, k[i].name, sizeof f->driver - 1);
        f->index = (unsigned)(n + 3);                        /* lo0 = 1; gif0/stf0 não existem; en0 = 4, como num Mac */
        f->wifi = (k[i].flags & KIF_WIFI) != 0;
        f->link = (k[i].flags & KIF_LINK) != 0;
        f->active = (k[i].flags & KIF_ACTIVE) != 0;
        memcpy(f->mac, k[i].mac, 6);
        f->flags = IFF_UP | IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST | (f->link ? IFF_RUNNING : 0);
        if (k[i].flags & KIF_CONF) { f->ip = k[i].ip; f->mask = k[i].mask; }
        if (k[i].flags & KIF_V6) { memcpy(f->ll6, k[i].ll6, 16); f->has_ll6 = 1; }
        if (k[i].flags & KIF_V6G) { memcpy(f->global6, k[i].global6, 16); f->has_g6 = 1; f->prefix6 = (int)k[i].prefix6; }
        f->rx_bytes = k[i].rx_bytes; f->tx_bytes = k[i].tx_bytes; f->rx_packets = k[i].rx_packets; f->tx_packets = k[i].tx_packets;
    }
    return n;
}

/* ---- estruturas do macOS */
typedef struct { u8 len, family; u16 port; u32 addr; u8 zero[8]; } sin_t;
typedef struct { u8 len, family; u16 port; u32 flow; u8 addr[16]; u32 scope; } sin6_t;
typedef struct { u8 len, family; u16 index; u8 type, nlen, alen, slen; char data[20]; } sdl_t;
typedef struct {
    u8 type, typelen, physical, addrlen, hdrlen, recvquota, xmitquota, unused1;
    u32 mtu, metric, baudrate, ipackets, ierrors, opackets, oerrors, collisions, ibytes, obytes, imcasts, omcasts, iqdrops,
        noproto, recvtiming, xmittiming;
    u32 lastchange_sec, lastchange_usec, unused2, hwassist, reserved1, reserved2;
} if_data_t;
typedef struct ifaddrs_ {
    struct ifaddrs_ *next; char *name; u32 flags; void *addr, *netmask, *dstaddr; void *data;
} ifaddrs_t;
static u32 be32(u32 v) { return (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24); }

static ifaddrs_t *node(const cx_netif_t *f, size_t extra) {
    ifaddrs_t *a = calloc(1, sizeof *a + extra);
    if (!a) return 0;
    a->name = malloc(strlen(f->bsd) + 1);
    if (a->name) strcpy(a->name, f->bsd);
    a->flags = f->flags;
    return a;
}
int getifaddrs(ifaddrs_t **out) {
    cx_netif_t f[9];
    int n = cx_netif_list(f, 9);
    ifaddrs_t *head = 0, **tail = &head;
    for (int i = 0; i < n; i++) {
        /* AF_LINK: MAC e contadores (if_data) */
        ifaddrs_t *a = node(&f[i], sizeof(sdl_t) + sizeof(if_data_t));
        if (!a) break;
        sdl_t *dl = (sdl_t *)(a + 1);
        if_data_t *d = (if_data_t *)(dl + 1);
        size_t nl = strlen(f[i].bsd);
        dl->len = (u8)sizeof *dl; dl->family = 18; dl->index = (u16)f[i].index;
        dl->type = f[i].loopback ? 24 : 6; dl->nlen = (u8)nl; dl->alen = f[i].loopback ? 0 : 6;
        memcpy(dl->data, f[i].bsd, nl);
        if (!f[i].loopback) memcpy(dl->data + nl, f[i].mac, 6);
        d->type = dl->type; d->addrlen = dl->alen; d->hdrlen = f[i].loopback ? 0 : 14;
        d->mtu = f[i].loopback ? 16384 : 1500;
        d->baudrate = f[i].loopback ? 0 : f[i].wifi ? 433000000u : 1000000000u;
        d->ipackets = (u32)f[i].rx_packets; d->opackets = (u32)f[i].tx_packets;
        d->ibytes = (u32)f[i].rx_bytes; d->obytes = (u32)f[i].tx_bytes;
        a->addr = dl; a->data = d;
        *tail = a; tail = &a->next;
        if (f[i].ip) {                                         /* AF_INET */
            a = node(&f[i], 3 * sizeof(sin_t));
            if (!a) break;
            sin_t *ad = (sin_t *)(a + 1), *mk = ad + 1, *bc = ad + 2;
            ad->len = mk->len = bc->len = 16; ad->family = mk->family = bc->family = 2;
            ad->addr = be32(f[i].ip); mk->addr = be32(f[i].mask);
            a->addr = ad; a->netmask = mk;
            if (!f[i].loopback) { bc->addr = be32(f[i].ip | ~f[i].mask); a->dstaddr = bc; }
            *tail = a; tail = &a->next;
        }
        for (int v = 0; v < 2; v++) {                          /* AF_INET6: link-local e global */
            if (v == 0 ? !f[i].has_ll6 : !f[i].has_g6) continue;
            a = node(&f[i], 2 * sizeof(sin6_t));
            if (!a) break;
            sin6_t *ad = (sin6_t *)(a + 1), *mk = ad + 1;
            ad->len = mk->len = (u8)sizeof *ad; ad->family = mk->family = 30;
            memcpy(ad->addr, v == 0 ? f[i].ll6 : f[i].global6, 16);
            int plen = v == 0 ? 64 : f[i].prefix6 ? f[i].prefix6 : 64;
            for (int b = 0; b < plen && b < 128; b++) mk->addr[b / 8] |= (u8)(0x80 >> (b % 8));
            if (v == 0) ad->scope = f[i].index;
            a->addr = ad; a->netmask = mk;
            *tail = a; tail = &a->next;
        }
    }
    *out = head;
    return 0;
}
void freeifaddrs(ifaddrs_t *a) {
    while (a) { ifaddrs_t *n = a->next; free(a->name); free(a); a = n; }
}
unsigned if_nametoindex(const char *name) {
    if (!name) return 0;
    cx_netif_t f[9];
    int n = cx_netif_list(f, 9);
    for (int i = 0; i < n; i++) if (!strcmp(f[i].bsd, name)) return f[i].index;
    return 0;
}
char *if_indextoname(unsigned idx, char *buf) {
    cx_netif_t f[9];
    int n = cx_netif_list(f, 9);
    for (int i = 0; i < n; i++) if (f[i].index == idx) { strcpy(buf, f[i].bsd); return buf; }
    cx_errno = 6;                                              /* ENXIO */
    return 0;
}
typedef struct { unsigned index; char *name; } if_nameindex_t;
if_nameindex_t *if_nameindex(void) {
    cx_netif_t f[9];
    int n = cx_netif_list(f, 9);
    if_nameindex_t *v = calloc((size_t)n + 1, sizeof *v + 16);
    if (!v) return 0;
    char *names = (char *)(v + n + 1);
    for (int i = 0; i < n; i++) { v[i].index = f[i].index; v[i].name = names + 16 * i; strcpy(v[i].name, f[i].bsd); }
    return v;
}
void if_freenameindex(if_nameindex_t *v) { free(v); }
