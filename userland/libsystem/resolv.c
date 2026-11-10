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
    const u8 *p = src;
    int out = 0, jumps = 0, consumed = 0, first = 1;
    if (!msg || !eom || !src || !dst || dstsiz < 1 || src < msg || src >= eom) return -1;
    for (;;) {
        if (p >= eom) return -1;
        u8 l = *p;
        if ((l & 0xC0) == 0xC0) {
            if (p + 1 >= eom) return -1;
            int off = ((l & 0x3F) << 8) | p[1];
            if (!first) consumed += 2;
            if (jumps++ > 16 || msg + off >= eom) return -1;
            p = msg + off;
            first = 0;
            continue;
        }
        if (!l) {
            if (first) consumed = (int)(p - src) + 1;
            dst[out] = 0;
            return consumed ? consumed : (int)(p + 1 - src);
        }
        if ((l & 0xC0) || p + 1 + l > eom) return -1;
        if (out && out + 1 < dstsiz) dst[out++] = '.';
        for (u8 i = 0; i < l; i++) {
            if (out + 1 >= dstsiz) return -1;
            dst[out++] = (char)p[1 + i];
        }
        p += 1 + l;
        if (first) consumed = (int)(p - src);
        first = 0;
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
