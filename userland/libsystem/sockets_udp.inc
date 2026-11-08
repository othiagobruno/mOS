/* ================================================================ UDP (SOCK_DGRAM) pelo kernel: SYS_NET MOS_NET_UDP_*
 * (kernel/net/userudp.c). Incluído por sockets_inet.inc. Semântica do BSD:
 *  - bind com porta 0 escolhe uma efêmera (getsockname devolve); sendto sem bind também (bind implícito);
 *  - connect guarda o par: send/recv sem endereço passam a ir e vir dele, e datagramas de outras origens são
 *    descartados; connect com AF_UNSPEC desfaz; sendto com endereço num socket conectado dá EISCONN;
 *  - recv bloqueia (em fatias de 2 ms) salvo O_NONBLOCK/MSG_DONTWAIT; SO_RCVTIMEO dá EAGAIN no prazo;
 *  - um datagrama maior que o buffer é truncado e o resto descartado (MSG_TRUNC em recvmsg), MSG_PEEK não tira;
 *  - AF_INET6 é dual-stack (v4 como ::ffff:a.b.c.d) salvo IPV6_V6ONLY.
 * O laço local (127.x, ::1) e o próprio endereço não saem pela placa: processos do mOS conversam por UDP sem rede. */
#define CX_EDESTADDRREQ 39
#define CX_MSG_TRUNC 0x10
void *malloc(size_t n);
void free(void *p);
int memcmp(const void *a, const void *b, size_t n);

static int udp_errno(long k) {
    switch (-k) {
    case MOS_EMSGSIZE: return CX_EMSGSIZE;
    case MOS_EBADF: return CX_EBADF;
    case MOS_ENOMEM: return 55;   /* ENOBUFS */
    default: return errno_of(k);
    }
}

static int udp_open(sock_t *s) {
    if (s->ku >= 0) return 0;
    int fam = s->family == 2 ? MOS_NET_L_V4 : (s->v6only ? MOS_NET_L_V6 : MOS_NET_L_V4 | MOS_NET_L_V6);
    long k = net(MOS_NET_UDP_OPEN, fam, 0, 0);
    if (k < 0) { cx_errno = udp_errno(k); return -1; }
    s->ku = (int)k;
    return 0;
}

/* porta e endereço locais que o kernel escolheu (bind implícito do sendto/connect) */
static void udp_refresh_local(sock_t *s) {
    mos_net_addrs_t ad;
    if (s->ku >= 0 && net(MOS_NET_UDP_ADDRS, s->ku, (long)&ad, 0) == 0 && ad.lport) {
        memcpy(s->laddr, ad.lip, 16);
        s->lport = ad.lport;
        s->bound = 1;
    }
}

static int udp_bind(sock_t *s, const void *addr, u32 len) {
    u8 a16[16];
    unsigned short port;
    int e = get_sa(s, addr, len, a16, &port);
    if (e) { cx_errno = e; return -1; }
    if (s->bound) { cx_errno = CX_EINVAL_; return -1; }
    if (udp_open(s)) return -1;
    long k = net(MOS_NET_UDP_BIND, s->ku, (long)a16, port);
    if (k < 0) { cx_errno = udp_errno(k); return -1; }
    memcpy(s->laddr, a16, 16);
    s->lport = (unsigned short)k;
    s->bound = 1;
    return 0;
}

static int udp_connect(sock_t *s, const void *addr, u32 len) {
    const u8 *a = addr;
    if (a && len >= 2 && a[1] == 0) { s->uconn = 0; memset(s->raddr, 0, 16); s->rport = 0; return 0; }   /* AF_UNSPEC: desfaz */
    u8 a16[16];
    unsigned short port;
    int e = get_sa(s, addr, len, a16, &port);
    if (e) { cx_errno = e; return -1; }
    /* sem rota, o connect UDP do BSD já falha (o Chromium testa o IPv6 assim: connect para 2001:4860:4860::8888) */
    static const u8 LOOP6[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    if (!is_mapped(a16) && memcmp(a16, LOOP6, 16) && a16[0] != 0xFF && !(a16[0] == 0xFE && (a16[1] & 0xC0) == 0x80)) {
        mos_net6_info_t n6;
        if (net(MOS_NET_INFO6, (long)&n6, 0, 0) != 1) { cx_errno = CX_ENETUNREACH; return -1; }
    } else if (is_mapped(a16) && a16[12] != 127) {
        unsigned info[4] = {0};
        if (net(MOS_NET_INFO, (long)info, 0, 0) != 1) { cx_errno = CX_ENETUNREACH; return -1; }
    }
    if (udp_open(s)) return -1;
    if (!s->bound) {   /* o connect do BSD já liga o socket a uma porta local */
        u8 any[16];
        memset(any, 0, 16);
        if (s->family == 2) map4(0, any);
        long k = net(MOS_NET_UDP_BIND, s->ku, (long)any, 0);
        if (k < 0) { cx_errno = udp_errno(k); return -1; }
        memcpy(s->laddr, any, 16);
        s->lport = (unsigned short)k;
        s->bound = 1;
    }
    memcpy(s->raddr, a16, 16);
    s->rport = port;
    s->uconn = 1;
    return 0;
}

static long udp_sendto(sock_t *s, const void *b, size_t n, const void *a, u32 al) {
    mos_net_dgram_t d;
    memset(&d, 0, sizeof d);
    if (a && al) {
        if (s->uconn) { cx_errno = CX_EISCONN; return -1; }
        unsigned short port;
        int e = get_sa(s, a, al, d.addr, &port);
        if (e) { cx_errno = e; return -1; }
        d.port = port;
    } else if (s->uconn) {
        memcpy(d.addr, s->raddr, 16);
        d.port = s->rport;
    } else { cx_errno = CX_EDESTADDRREQ; return -1; }
    if (!d.port) { cx_errno = CX_EADDRNOTAVAIL; return -1; }
    if (udp_open(s)) return -1;
    d.buf = (unsigned long long)(unsigned long)b;
    d.len = (unsigned)n;
    long k = net(MOS_NET_UDP_SENDTO, s->ku, (long)&d, 0);
    if (k < 0) { cx_errno = udp_errno(k); return -1; }
    if (!s->bound) udp_refresh_local(s);
    return k;
}

/* recebe um datagrama: *trunc = 1 se não coube; a origem em from16/fport */
static long udp_recv1(sock_t *s, void *b, size_t n, int fl, u8 *from16, unsigned short *fport, int *trunc) {
    if (udp_open(s)) return -1;
    int nb = s->nonblock || (fl & CX_MSG_DONTWAIT);
    u64 end = s->rcvtimeo ? (u64)SYS0(SYS_TIME) + (u64)s->rcvtimeo : 0;
    for (;;) {
        mos_net_dgram_t d;
        memset(&d, 0, sizeof d);
        d.buf = (unsigned long long)(unsigned long)b;
        d.cap = (unsigned)n;
        d.flags = (fl & CX_MSG_PEEK) ? MOS_NET_DG_PEEK : 0;
        long k = net(MOS_NET_UDP_RECVFROM, s->ku, (long)&d, 0);
        if (k >= 0) {
            /* conectado: só o par conta; os outros são descartados (tirados da fila mesmo num MSG_PEEK) */
            if (s->uconn && (memcmp(d.addr, s->raddr, 16) || d.port != s->rport)) {
                if (fl & CX_MSG_PEEK) {
                    mos_net_dgram_t x;
                    memset(&x, 0, sizeof x);
                    net(MOS_NET_UDP_RECVFROM, s->ku, (long)&x, 0);
                }
                continue;
            }
            if (from16) memcpy(from16, d.addr, 16);
            if (fport) *fport = d.port;
            if (trunc) *trunc = (d.flags & MOS_NET_DG_TRUNC) != 0;
            return k;
        }
        if (k != -MOS_EAGAIN) { cx_errno = udp_errno(k); return -1; }
        if (nb) { cx_errno = CX_EAGAIN; return -1; }
        if (end && (u64)SYS0(SYS_TIME) >= end) { cx_errno = CX_EAGAIN; return -1; }
        SYS1(SYS_SLEEP, 2);
    }
}

static long udp_recvfrom(sock_t *s, void *b, size_t n, int fl, void *a, u32 *al) {
    u8 from[16];
    unsigned short port = 0;
    int tr = 0;
    long k = udp_recv1(s, b, n, fl, from, &port, &tr);
    if (k < 0) return -1;
    if (a && al) put_sa(s, a, al, from, port);
    else if (al) *al = 0;
    return k;
}

/* sendmsg/recvmsg com msg_name e vários iovec: o datagrama é um bloco só */
static long udp_sendmsg(sock_t *s, const cx_msghdr_t *h, int fl) {
    (void)fl;
    size_t tot = 0;
    for (int i = 0; i < h->iovlen; i++) tot += h->iov[i].len;
    u8 small[2048], *buf = tot <= sizeof small ? small : malloc(tot);
    if (!buf) { cx_errno = 55; return -1; }
    size_t o = 0;
    for (int i = 0; i < h->iovlen; i++) { if (h->iov[i].len) memcpy(buf + o, h->iov[i].base, h->iov[i].len); o += h->iov[i].len; }
    long k = udp_sendto(s, buf, tot, h->name, h->namelen);
    if (buf != small) free(buf);
    return k;
}
static long udp_recvmsg(sock_t *s, cx_msghdr_t *h, int fl) {
    size_t tot = 0;
    for (int i = 0; i < h->iovlen; i++) tot += h->iov[i].len;
    u8 small[2048], *buf = tot <= sizeof small ? small : malloc(tot);
    if (!buf) { cx_errno = 55; return -1; }
    u8 from[16];
    unsigned short port = 0;
    int tr = 0;
    long k = udp_recv1(s, buf, tot, fl, from, &port, &tr);
    if (k >= 0) {
        size_t o = 0;
        for (int i = 0; i < h->iovlen && o < (size_t)k; i++) {
            size_t c = h->iov[i].len < (size_t)k - o ? h->iov[i].len : (size_t)k - o;
            memcpy(h->iov[i].base, buf + o, c);
            o += c;
        }
        if (h->name && h->namelen) put_sa(s, h->name, &h->namelen, from, port);
        else h->namelen = 0;
        h->controllen = 0;
        h->flags = tr ? CX_MSG_TRUNC : 0;
    }
    if (buf != small) free(buf);
    return k;
}

static int udp_readable(sock_t *s, long *avail) {
    if (s->ku < 0) return 0;
    long n = net(MOS_NET_UDP_AVAIL, s->ku, 0, 0);
    if (n < 0) return 0;
    *avail = n;
    return 1;
}

#define CX_SO_RCVTIMEO 0x1006
static int udp_setsockopt(sock_t *s, int level, int name, const void *v, u32 len) {
    if (level == 0xffff && name == CX_SO_RCVTIMEO && v && len >= 16) {
        const long *tv = v;   /* struct timeval do macOS: tv_sec (8) + tv_usec (4 + preenchimento) */
        long ms = tv[0] * 1000 + (long)(*(const int *)((const u8 *)v + 8)) / 1000;
        s->rcvtimeo = ms > 0 ? (int)ms : 0;
        return 0;
    }
    return 0;   /* SO_BROADCAST, SO_REUSEADDR/PORT, IP_MULTICAST_*, IP_ADD_MEMBERSHIP, SO_RCVBUF...: valem como feitos */
}
