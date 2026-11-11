/* Cliente DNS em espaço de usuário: /etc/resolv.conf, UDP e TCP.
 * Sem o arquivo, usa o DNS do DHCP (MOS_NET_INFO) ou 10.0.2.3 (QEMU). */
#include <stddef.h>
#include <stdint.h>

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned short u16;

#ifdef MOS_RESOLV_HOST
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define mos_open open
#define mos_read read
#define mos_close close
static int mos_udp(unsigned ip, const void *q, int qn, u8 *ans, int cap, int ms) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(53);
    a.sin_addr.s_addr = htonl(ip);
    if (sendto(fd, q, (size_t)qn, 0, (struct sockaddr *)&a, sizeof a) != qn) { close(fd); return -1; }
    int n = (int)recvfrom(fd, ans, (size_t)cap, 0, 0, 0);
    close(fd);
    return n;
}
static int mos_tcp(unsigned ip, const void *q, int qn, u8 *ans, int cap) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(53);
    a.sin_addr.s_addr = htonl(ip);
    if (connect(fd, (struct sockaddr *)&a, sizeof a)) { close(fd); return -1; }
    u8 hdr[2] = { (u8)(qn >> 8), (u8)qn };
    if (write(fd, hdr, 2) != 2 || write(fd, q, (size_t)qn) != qn) { close(fd); return -1; }
    u8 lh[2];
    if (read(fd, lh, 2) != 2) { close(fd); return -1; }
    int n = (lh[0] << 8) | lh[1];
    if (n > cap) n = cap;
    int got = 0;
    while (got < n) {
        int k = (int)read(fd, ans + got, (size_t)(n - got));
        if (k <= 0) { close(fd); return -1; }
        got += k;
    }
    close(fd);
    return got;
}
static unsigned dhcp_dns(void) { return 0; }
#else
#include "sys.h"
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int open(const char *path, int flags, ...);
long read(int fd, void *b, unsigned long n);
int close(int fd);
int socket(int domain, int type, int proto);
typedef struct { long ret, rdx; } cx_sysret_t;
cx_sysret_t sendto(int fd, const void *b, size_t n, int fl, const void *a, u32 al);
long recvfrom(int fd, void *b, size_t n, int fl, void *a, u32 *al);
typedef struct { int fd; short events, revents; } cx_pollfd_t;
int poll(cx_pollfd_t *fds, unsigned n, int timeout);
#define mos_open(p, f) open(p, f)
#define mos_read read
#define mos_close close
static int mos_udp(unsigned ip, const void *q, int qn, u8 *ans, int cap, int ms) {
    int fd = socket(2, 2, 17);
    if (fd < 0) return -1;
    u8 sa[16];
    memset(sa, 0, sizeof sa);
    sa[0] = 16; sa[1] = 2; sa[2] = 0; sa[3] = 53;
    sa[4] = (u8)(ip >> 24); sa[5] = (u8)(ip >> 16); sa[6] = (u8)(ip >> 8); sa[7] = (u8)ip;
    if (sendto(fd, q, (size_t)qn, 0, sa, 16).ret != qn) { close(fd); return -1; }
    cx_pollfd_t p = { fd, 1, 0 };
    if (poll(&p, 1, ms) <= 0) { close(fd); return -1; }
    int n = (int)recvfrom(fd, ans, (size_t)cap, 0, 0, 0);
    close(fd);
    return n;
}
static int mos_tcp(unsigned ip, const void *q, int qn, u8 *ans, int cap) {
    int fd = socket(2, 1, 6);
    if (fd < 0) return -1;
    u8 sa[16];
    memset(sa, 0, sizeof sa);
    sa[0] = 16; sa[1] = 2; sa[2] = 0; sa[3] = 53;
    sa[4] = (u8)(ip >> 24); sa[5] = (u8)(ip >> 16); sa[6] = (u8)(ip >> 8); sa[7] = (u8)ip;
    int connect(int, const void *, unsigned);
    long send(int, const void *, unsigned long, int);
    long recv(int, void *, unsigned long, int);
    if (connect(fd, sa, 16)) { close(fd); return -1; }
    u8 hdr[2] = { (u8)(qn >> 8), (u8)qn };
    if (send(fd, hdr, 2, 0) != 2 || send(fd, q, (unsigned long)qn, 0) != qn) { close(fd); return -1; }
    u8 lh[2];
    if (recv(fd, lh, 2, 0) != 2) { close(fd); return -1; }
    int n = (lh[0] << 8) | lh[1];
    if (n > cap) n = cap;
    int got = 0;
    while (got < n) {
        long k = recv(fd, ans + got, (unsigned long)(n - got), 0);
        if (k <= 0) { close(fd); return -1; }
        got += (int)k;
    }
    close(fd);
    return got;
}
static unsigned dhcp_dns(void) {
    u32 info[4];
    if (SYS4(SYS_NET, MOS_NET_INFO, info, 0, 0) != 1 || !info[3]) return 0;
    return info[3];
}
#endif

#ifdef MOS_RESOLV_HOST
int h_errno;
#else
extern int h_errno;
#endif
enum { HOST_NOT_FOUND = 1, TRY_AGAIN = 2, NO_RECOVERY = 3, NO_DATA = 4 };

typedef struct {
    int nscount, nsearch, ndots, retrans, retry, inited;
    unsigned ns[3];
    char search[6][64];
} mos_res_t;

static mos_res_t R;

static int dots_of(const char *s) {
    int n = 0;
    for (; *s; s++) if (*s == '.') n++;
    return n;
}

static void copy_name(char *d, const char *s, int cap) {
    int i = 0;
    while (s[i] && i + 1 < cap) { d[i] = s[i]; i++; }
    d[i] = 0;
}

static const char *conf_path(void) {
#ifdef MOS_RESOLV_HOST
    const char *e = getenv("MOS_RESOLV_CONF");
    return e && e[0] ? e : "/etc/resolv.conf";
#else
    return "/etc/resolv.conf";
#endif
}

static void parse_line(char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '#' || *line == ';' || !*line) return;
    char *sp = line;
    while (*sp && *sp != ' ' && *sp != '\t') sp++;
    if (*sp) { *sp++ = 0; while (*sp == ' ' || *sp == '\t') sp++; }
    if (!strcmp(line, "nameserver") && R.nscount < 3) {
        unsigned ip = 0, part = 0, dots = 0, dig = 0, ok = 1;
        for (char *p = sp;; p++) {
            if (*p >= '0' && *p <= '9') { part = part * 10 + (unsigned)(*p - '0'); if (part > 255 || ++dig > 3) ok = 0; }
            else if (*p == '.' || !*p) {
                if (!dig) ok = 0;
                ip = ip << 8 | part; part = 0; dig = 0;
                if (!*p) break;
                dots++;
            } else ok = 0;
            if (!ok) break;
        }
        if (ok && dots == 3) R.ns[R.nscount++] = ip;
    } else if (!strcmp(line, "search") || !strcmp(line, "domain")) {
        if (!strcmp(line, "domain")) R.nsearch = 0;
        while (*sp && R.nsearch < 6) {
            char *e = sp;
            while (*e && *e != ' ' && *e != '\t') e++;
            char save = *e; *e = 0;
            if (*sp) copy_name(R.search[R.nsearch++], sp, 64);
            if (!save) break;
            sp = e + 1;
            while (*sp == ' ' || *sp == '\t') sp++;
        }
    } else if (!strcmp(line, "options")) {
        while (*sp) {
            if (sp[0] == 'n' && sp[1] == 'd' && sp[2] == 'o' && sp[3] == 't' && sp[4] == 's' && sp[5] == ':') {
                int v = 0;
                for (char *p = sp + 6; *p >= '0' && *p <= '9'; p++) v = v * 10 + *p - '0';
                if (v > 15) v = 15;
                R.ndots = v;
            }
            while (*sp && *sp != ' ' && *sp != '\t') sp++;
            while (*sp == ' ' || *sp == '\t') sp++;
        }
    }
}

int res_ninit(void *state) {
    (void)state;
    memset(&R, 0, sizeof R);
    R.ndots = 1; R.retrans = 2; R.retry = 2;
    int fd = mos_open(conf_path(), 0);
    if (fd >= 0) {
        char buf[1024];
        long n = mos_read(fd, buf, sizeof buf - 1);
        mos_close(fd);
        if (n < 0) n = 0;
        buf[n] = 0;
        char *p = buf;
        while (*p) {
            char *nl = p;
            while (*nl && *nl != '\n') nl++;
            char save = *nl; *nl = 0;
            if (nl > p && nl[-1] == '\r') nl[-1] = 0;
            parse_line(p);
            if (!save) break;
            p = nl + 1;
        }
    }
    if (!R.nscount) {
        unsigned d = dhcp_dns();
        R.ns[R.nscount++] = d ? d : 0x0A000203u; /* 10.0.2.3 */
    }
    R.inited = 1;
    if (state) {
        int *w = state;
        w[0] = R.retrans; w[1] = R.retry; w[2] = 0x81; w[3] = R.nscount;
    }
    return 0;
}
int res_init(void) { return res_ninit(0); }
int res_9_ninit(void *state) { return res_ninit(state); }
void res_9_ndestroy(void *state) { (void)state; R.inited = 0; }
void res_9_nclose(void *state) { res_9_ndestroy(state); }
int mos_res_ndots(void) { if (!R.inited) res_ninit(0); return R.ndots; }
int mos_res_nsearch(void) { if (!R.inited) res_ninit(0); return R.nsearch; }
const char *mos_res_search(int i) { return i >= 0 && i < R.nsearch ? R.search[i] : ""; }

int dn_expand(const u8 *msg, const u8 *eom, const u8 *src, char *dst, int dstsiz) {
    /* consumed: bytes do nome em src até o primeiro ponteiro (inclusive os 2 dele) ou até o 0 final */
    const u8 *p = src;
    int out = 0, jumps = 0, consumed = -1;
    if (!msg || !eom || !src || !dst || dstsiz < 1 || src < msg || src >= eom) return -1;
    for (;;) {
        if (p >= eom) return -1;
        u8 l = *p;
        if ((l & 0xC0) == 0xC0) {
            if (p + 1 >= eom) return -1;
            int off = ((l & 0x3F) << 8) | p[1];
            if (consumed < 0) consumed = (int)(p - src) + 2;
            if (jumps++ > 16 || msg + off >= eom) return -1;
            p = msg + off;
            continue;
        }
        if (!l) {
            if (consumed < 0) consumed = (int)(p - src) + 1;
            dst[out] = 0;
            return consumed;
        }
        if ((l & 0xC0) || p + 1 + l > eom) return -1;
        if (out && out + 1 < dstsiz) dst[out++] = '.';
        for (u8 i = 0; i < l; i++) {
            if (out + 1 >= dstsiz) return -1;
            dst[out++] = (char)p[1 + i];
        }
        p += 1 + l;
    }
}

static int put_name(u8 *b, int cap, int o, const char *name) {
    if (name[0] == '.' && !name[1]) { if (o >= cap) return -1; b[o] = 0; return o + 1; }
    while (*name) {
        const char *d = name;
        while (*d && *d != '.') d++;
        int l = (int)(d - name);
        if (l <= 0 || l > 63 || o + 1 + l >= cap) return -1;
        b[o++] = (u8)l;
        memcpy(b + o, name, (size_t)l);
        o += l;
        name = *d == '.' ? d + 1 : d;
    }
    if (o >= cap) return -1;
    b[o++] = 0;
    return o;
}

static int build_query(u8 *b, int cap, const char *name, int type) {
    if (cap < 18) return -1;
    memset(b, 0, 12);
    b[0] = 0x4d; b[1] = 0x51; b[2] = 1; b[5] = 1;
    int o = put_name(b, cap, 12, name);
    if (o < 0 || o + 4 > cap) return -1;
    b[o++] = (u8)(type >> 8); b[o++] = (u8)type; b[o++] = 0; b[o++] = 1;
    return o;
}

static int exchange(const u8 *q, int qn, u8 *ans, int cap) {
    if (!R.inited) res_ninit(0);
    for (int a = 0; a < R.retry; a++) {
        for (int i = 0; i < R.nscount; i++) {
            int n = mos_udp(R.ns[i], q, qn, ans, cap, R.retrans * 1000);
            if (n >= 12 && (ans[2] & 2)) {
                int t = mos_tcp(R.ns[i], q, qn, ans, cap);
                if (t >= 12) return t;
            }
            if (n >= 12) return n;
            if (n < 0) {
                int t = mos_tcp(R.ns[i], q, qn, ans, cap);
                if (t >= 12) return t;
            }
        }
    }
    h_errno = TRY_AGAIN;
    return -1;
}

int res_nquery(void *state, const char *name, int cls, int type, u8 *ans, int anslen) {
    (void)state;
    if (!name || !ans || anslen < 12 || cls != 1) { h_errno = NO_RECOVERY; return -1; }
    u8 q[512];
    int qn = build_query(q, sizeof q, name, type);
    if (qn < 0) { h_errno = NO_RECOVERY; return -1; }
    int n = exchange(q, qn, ans, anslen);
    if (n < 0) return -1;
    int rcode = ans[3] & 15;
    int an = (ans[6] << 8) | ans[7];
    if (rcode == 3) { h_errno = HOST_NOT_FOUND; return -1; }
    if (rcode) { h_errno = NO_RECOVERY; return -1; }
    if (!an) { h_errno = NO_DATA; return -1; }
    return n;
}
int res_query(const char *name, int cls, int type, u8 *ans, int anslen) {
    return res_nquery(0, name, cls, type, ans, anslen);
}

static int join_search(char *out, int cap, const char *name, const char *suf) {
    size_t ln = strlen(name), ls = strlen(suf);
    if (ln && name[ln - 1] == '.') ln--;
    if (ln > 240 || ls > 63 || ln + 1 + ls + 1 > (size_t)cap) return -1;
    memcpy(out, name, ln);
    out[ln] = '.';
    memcpy(out + ln + 1, suf, ls + 1);
    return 0;
}

int res_nsearch(void *state, const char *name, int cls, int type, u8 *ans, int anslen) {
    if (!name) { h_errno = NO_RECOVERY; return -1; }
    if (!R.inited) res_ninit(0);
    int abs = name[0] && name[strlen(name) - 1] == '.';
    int nd = dots_of(name);
    char names[8][256];
    int n = 0;
    if ((abs || nd >= R.ndots) && n < 8) copy_name(names[n++], name, 256);
    if (!abs) {
        for (int i = 0; i < R.nsearch && n < 8; i++)
            if (join_search(names[n], 256, name, R.search[i]) == 0) n++;
    }
    if (!(abs || nd >= R.ndots) && n < 8) copy_name(names[n++], name, 256);
    int last = HOST_NOT_FOUND;
    for (int i = 0; i < n; i++) {
        int r = res_nquery(state, names[i], cls, type, ans, anslen);
        if (r >= 0) return r;
        last = h_errno;
        if (last != HOST_NOT_FOUND && last != NO_DATA) return -1;
    }
    h_errno = last;
    return -1;
}
int res_search(const char *name, int cls, int type, u8 *ans, int anslen) {
    return res_nsearch(0, name, cls, type, ans, anslen);
}
int res_9_nsearch(void *state, const char *name, int cls, int type, u8 *ans, int anslen) {
    return res_nsearch(state, name, cls, type, ans, anslen);
}
int res_9_search(const char *name, int cls, int type, u8 *ans, int anslen) {
    return res_search(name, cls, type, ans, anslen);
}
/* libresolv.9 com o prefixo res_9_ (resolv.h do macOS): a mesma consulta e a mesma expansão de nome */
int res_9_nquery(void *state, const char *name, int cls, int type, u8 *ans, int anslen) {
    return res_nquery(state, name, cls, type, ans, anslen);
}
int res_9_dn_expand(const u8 *msg, const u8 *eom, const u8 *src, char *dst, int dstsiz) {
    return dn_expand(msg, eom, src, dst, dstsiz);
}

static int first_addr(const u8 *msg, int n, int type, u8 *out, int outlen) {
    if (n < 12) return 0;
    const u8 *eom = msg + n, *p = msg + 12;
    char tmp[256];
    int qd = (msg[4] << 8) | msg[5];
    int an = (msg[6] << 8) | msg[7];
    for (int i = 0; i < qd; i++) {
        int k = dn_expand(msg, eom, p, tmp, sizeof tmp);
        if (k < 0 || p + k + 4 > eom) return 0;
        p += k + 4;
    }
    for (int i = 0; i < an; i++) {
        int k = dn_expand(msg, eom, p, tmp, sizeof tmp);
        if (k < 0 || p + k + 10 > eom) return 0;
        int ty = (p[k] << 8) | p[k + 1];
        int rd = (p[k + 8] << 8) | p[k + 9];
        const u8 *rdata = p + k + 10;
        if (rdata + rd > eom) return 0;
        if (ty == type && rd == outlen) { memcpy(out, rdata, (size_t)rd); return 1; }
        p = rdata + rd;
    }
    return 0;
}

int mos_resolv_host(const char *host, int want4, int want6, unsigned *ip, int *got4, u8 *ip6, int *got6) {
    *got4 = *got6 = 0;
    u8 ans[1024];
    if (want4) {
        int n = res_search(host, 1, 1, ans, sizeof ans);
        u8 a4[4];
        if (n > 0 && first_addr(ans, n, 1, a4, 4)) {
            *ip = (unsigned)a4[0] << 24 | (unsigned)a4[1] << 16 | (unsigned)a4[2] << 8 | a4[3];
            *got4 = 1;
        } else if (h_errno == HOST_NOT_FOUND && !want6) return 8;
    }
    if (want6) {
        int n = res_search(host, 1, 28, ans, sizeof ans);
        if (n > 0 && first_addr(ans, n, 28, ip6, 16)) *got6 = 1;
        else if (!*got4 && h_errno == HOST_NOT_FOUND) return 8;
    }
    return *got4 || *got6 ? 0 : (h_errno == TRY_AGAIN ? 2 : 8);
}

typedef void (*dns_addr_cb)(void *sd, u32 flags, u32 iface, int err, const char *host, const void *sa, u32 ttl, void *ctx);
int DNSServiceGetAddrInfo(void **sd, u32 flags, u32 iface, u32 proto, const char *host, dns_addr_cb cb, void *ctx) {
    (void)flags; (void)iface;
    if (sd) *sd = (void *)1;
    if (!cb || !host) return -65540;
    unsigned ip = 0; u8 ip6[16]; int g4 = 0, g6 = 0;
    int want4 = !(proto & 2) || (proto & 1) || !proto;
    int want6 = (proto & 2) || !proto;
    int e = mos_resolv_host(host, want4, want6, &ip, &g4, ip6, &g6);
    if (e) { cb(sd ? *sd : 0, 0, 0, e == 2 ? -65563 : -65554, host, 0, 0, ctx); return 0; }
    if (g4) {
        u8 sa[16];
        memset(sa, 0, 16);
        sa[0] = 16; sa[1] = 2;
        sa[4] = (u8)(ip >> 24); sa[5] = (u8)(ip >> 16); sa[6] = (u8)(ip >> 8); sa[7] = (u8)ip;
        cb(sd ? *sd : 0, g6 ? 1 : 0, 0, 0, host, sa, 60, ctx);
    }
    if (g6) {
        u8 sa[28];
        memset(sa, 0, 28);
        sa[0] = 28; sa[1] = 30;
        memcpy(sa + 8, ip6, 16);
        cb(sd ? *sd : 0, 0, 0, 0, host, sa, 60, ctx);
    }
    return 0;
}

/* ---- res_9_mkquery e a API dns_util.h (dns_parse_packet / dns_free_reply) ----
 * O WhatsApp monta a consulta com res_9_mkquery e lê a resposta com dns_parse_packet.
 * As structs seguem o dns_util.h da Apple; os dados de cada registro vão por tipo
 * (A, AAAA, NS/CNAME/PTR, MX, TXT, SRV; o resto como bytes crus). */
int res_9_mkquery(int op, const char *dname, int cls, int type, const u8 *data, int datalen, const u8 *newrr, u8 *buf, int buflen) {
    (void)data; (void)datalen; (void)newrr;
    if (op != 0 || !dname || !buf || cls < 0) return -1;            /* só QUERY */
    int n = build_query(buf, buflen, dname, type);
    if (n < 0) return -1;
    static u16 xid = 0x4d51;
    xid = (u16)(xid * 1103515245u + 12345u);
    buf[0] = (u8)(xid >> 8); buf[1] = (u8)xid;
    buf[n - 2] = (u8)(cls >> 8); buf[n - 1] = (u8)cls;
    return n;
}
int res_mkquery(int op, const char *dname, int cls, int type, const u8 *data, int datalen, const u8 *newrr, u8 *buf, int buflen) {
    return res_9_mkquery(op, dname, cls, type, data, datalen, newrr, buf, buflen);
}

typedef struct { u16 xid, flags, qdcount, ancount, nscount, arcount; } dns_header_t;
typedef struct { char *name; u16 dnstype, dnsclass; } dns_question_t;
typedef struct { u8 b[4]; } dns_in_addr_t;
typedef struct { u8 b[16]; } dns_in6_addr_t;
typedef struct { dns_in_addr_t addr; } dns_address_record_t;
typedef struct { dns_in6_addr_t addr; } dns_in6_address_record_t;
typedef struct { char *name; } dns_domain_name_record_t;
typedef struct { u16 preference; char *name; } dns_MX_record_t;
typedef struct { u32 string_count; char **strings; } dns_TXT_record_t;
typedef struct { u16 priority, weight, port; char *target; } dns_SRV_record_t;
typedef struct { u16 length; char *data; } dns_raw_resource_record_t;
typedef struct { char *name; u16 dnstype, dnsclass; u32 ttl; void *data; } dns_resource_record_t;
typedef struct {
    u32 status;
    dns_header_t *header;
    dns_question_t **question;
    dns_resource_record_t **answer, **authority, **additional;
    void *server;
} dns_reply_t;

static char *dns_dup(const char *s) { size_t n = strlen(s); char *d = malloc(n + 1); if (d) memcpy(d, s, n + 1); return d; }
static int dns_name(const u8 *msg, const u8 *eom, const u8 **p, char **out) {
    char tmp[256];
    int n = dn_expand(msg, eom, *p, tmp, sizeof tmp);
    if (n < 0) return -1;
    *p += n;
    *out = dns_dup(tmp);
    return *out ? 0 : -1;
}
static u16 rd16(const u8 *p) { return (u16)((p[0] << 8) | p[1]); }
static u32 rd32(const u8 *p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }

static void dns_free_rr(dns_resource_record_t *r) {
    if (!r) return;
    if (r->data) {
        switch (r->dnstype) {
        case 2: case 5: case 12: free(((dns_domain_name_record_t *)r->data)->name); break;
        case 15: free(((dns_MX_record_t *)r->data)->name); break;
        case 33: free(((dns_SRV_record_t *)r->data)->target); break;
        case 16: {
            dns_TXT_record_t *t = r->data;
            for (u32 i = 0; i < t->string_count; i++) free(t->strings[i]);
            free(t->strings);
            break;
        }
        case 1: case 28: break;
        default: free(((dns_raw_resource_record_t *)r->data)->data); break;
        }
        free(r->data);
    }
    free(r->name);
    free(r);
}
static dns_resource_record_t *dns_parse_rr(const u8 *msg, const u8 *eom, const u8 **pp) {
    const u8 *p = *pp;
    dns_resource_record_t *r = calloc(1, sizeof *r);
    if (!r) return 0;
    if (dns_name(msg, eom, &p, &r->name) || p + 10 > eom) goto bad;
    r->dnstype = rd16(p); r->dnsclass = rd16(p + 2); r->ttl = rd32(p + 4);
    u16 rdlen = rd16(p + 8);
    p += 10;
    if (p + rdlen > eom) goto bad;
    const u8 *rd = p, *end = p + rdlen;
    switch (r->dnstype) {
    case 1:
        if (rdlen != 4 || !(r->data = calloc(1, sizeof(dns_address_record_t)))) goto bad;
        memcpy(r->data, rd, 4);
        break;
    case 28:
        if (rdlen != 16 || !(r->data = calloc(1, sizeof(dns_in6_address_record_t)))) goto bad;
        memcpy(r->data, rd, 16);
        break;
    case 2: case 5: case 12: {
        dns_domain_name_record_t *d = calloc(1, sizeof *d);
        const u8 *q = rd;
        if (!(r->data = d) || dns_name(msg, eom, &q, &d->name)) goto bad;
        break;
    }
    case 15: {
        dns_MX_record_t *m = calloc(1, sizeof *m);
        const u8 *q = rd + 2;
        if (!(r->data = m) || rdlen < 3) goto bad;
        m->preference = rd16(rd);
        if (dns_name(msg, eom, &q, &m->name)) goto bad;
        break;
    }
    case 33: {
        dns_SRV_record_t *s = calloc(1, sizeof *s);
        const u8 *q = rd + 6;
        if (!(r->data = s) || rdlen < 7) goto bad;
        s->priority = rd16(rd); s->weight = rd16(rd + 2); s->port = rd16(rd + 4);
        if (dns_name(msg, eom, &q, &s->target)) goto bad;
        break;
    }
    case 16: {
        dns_TXT_record_t *t = calloc(1, sizeof *t);
        if (!(r->data = t)) goto bad;
        for (const u8 *q = rd; q < end; q += 1 + *q) t->string_count++;
        t->strings = calloc(t->string_count ? t->string_count : 1, sizeof(char *));
        if (!t->strings) goto bad;
        u32 i = 0;
        for (const u8 *q = rd; q < end && i < t->string_count; q += 1 + *q, i++) {
            u8 l = *q;
            if (q + 1 + l > end) { t->string_count = i; break; }
            t->strings[i] = malloc((size_t)l + 1);
            if (!t->strings[i]) { t->string_count = i; goto bad; }
            memcpy(t->strings[i], q + 1, l);
            t->strings[i][l] = 0;
        }
        break;
    }
    default: {
        dns_raw_resource_record_t *w = calloc(1, sizeof *w);
        if (!(r->data = w)) goto bad;
        w->length = rdlen;
        w->data = malloc(rdlen ? rdlen : 1);
        if (!w->data) goto bad;
        memcpy(w->data, rd, rdlen);
        break;
    }
    }
    *pp = end;
    return r;
bad:
    dns_free_rr(r);
    return 0;
}
static dns_resource_record_t **dns_parse_section(const u8 *msg, const u8 *eom, const u8 **p, u16 n) {
    dns_resource_record_t **v = calloc((size_t)n + 1, sizeof *v);
    if (!v) return 0;
    for (u16 i = 0; i < n; i++) {
        v[i] = dns_parse_rr(msg, eom, p);
        if (!v[i]) { for (u16 k = 0; k < i; k++) dns_free_rr(v[k]); free(v); return 0; }
    }
    return v;
}
void dns_free_reply(dns_reply_t *r) {
    if (!r) return;
    if (r->question) { for (u32 i = 0; r->question[i]; i++) { free(r->question[i]->name); free(r->question[i]); } free(r->question); }
    dns_resource_record_t **s[3] = {r->answer, r->authority, r->additional};
    for (int k = 0; k < 3; k++) if (s[k]) { for (u32 i = 0; s[k][i]; i++) dns_free_rr(s[k][i]); free(s[k]); }
    free(r->header);
    free(r->server);
    free(r);
}
dns_reply_t *dns_parse_packet(const char *buf, u32 len) {
    const u8 *msg = (const u8 *)buf, *eom, *p;
    dns_reply_t *r;
    if (!buf || len < 12) return 0;
    eom = msg + len;
    r = calloc(1, sizeof *r);
    if (!r || !(r->header = calloc(1, sizeof *r->header))) { free(r); return 0; }
    r->header->xid = rd16(msg); r->header->flags = rd16(msg + 2);
    r->header->qdcount = rd16(msg + 4); r->header->ancount = rd16(msg + 6);
    r->header->nscount = rd16(msg + 8); r->header->arcount = rd16(msg + 10);
    p = msg + 12;
    r->question = calloc((size_t)r->header->qdcount + 1, sizeof *r->question);
    if (!r->question) goto bad;
    for (u16 i = 0; i < r->header->qdcount; i++) {
        dns_question_t *q = calloc(1, sizeof *q);
        if (!q) goto bad;
        r->question[i] = q;
        if (dns_name(msg, eom, &p, &q->name) || p + 4 > eom) goto bad;
        q->dnstype = rd16(p); q->dnsclass = rd16(p + 2);
        p += 4;
    }
    if (!(r->answer = dns_parse_section(msg, eom, &p, r->header->ancount))) goto bad;
    if (!(r->authority = dns_parse_section(msg, eom, &p, r->header->nscount))) goto bad;
    if (!(r->additional = dns_parse_section(msg, eom, &p, r->header->arcount))) goto bad;
    return r;
bad:
    dns_free_reply(r);
    return 0;
}
