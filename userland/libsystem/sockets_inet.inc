/* ================================================================ AF_INET e AF_INET6 (TCP) pelo kernel: SYS_NET
 * (kernel/net/usersock.c). O kernel é não-bloqueante; aqui o modo bloqueante espera em fatias de 1–10 ms.
 * O que os apps-alvo usam (docs/compat-apps.md): connect bloqueante ou com O_NONBLOCK + poll/kqueue + SO_ERROR
 * (Chromium, libuv, tokio/mio do Codex), send/recv/read/write, sendmsg/recvmsg com iovec, getpeername.
 * Servidor TCP: bind/listen/accept (porta 0 = efêmera, getsockname devolve a escolhida). AF_INET6 é dual-stack como no
 * macOS (net.inet6.ip6.v6only = 0): um ouvinte em :: aceita IPv4, que aparece como ::ffff:a.b.c.d; IPV6_V6ONLY desliga.
 * O laço local (127.x, ::1) é do kernel. Os endereços ficam em 16 bytes (IPv4 mapeado) e viram sockaddr_in ou
 * sockaddr_in6 conforme a família do socket. Ping: SOCK_DGRAM + IPPROTO_ICMP/ICMPV6, como o do Darwin (sem root).
 * Ainda não: UDP. */
#define CX_EOPNOTSUPP 102
#define CX_EADDRNOTAVAIL 49
#define CX_EINVAL_ 22
static long net(long op, long a, long b, long c) { return SYS4(SYS_NET, op, a, b, c); }
static int is_inet(sock_t *s) { return s && (s->family == 2 || s->family == 30); }
static unsigned be32(const u8 *p) { return (unsigned)p[0] << 24 | (unsigned)p[1] << 16 | (unsigned)p[2] << 8 | p[3]; }
static void put_sin(void *addr, u32 *len, unsigned ip, unsigned short port) {
    if (!addr || !len) return;
    u8 sin[16] = {16, 2, (u8)(port >> 8), (u8)port, (u8)(ip >> 24), (u8)(ip >> 16), (u8)(ip >> 8), (u8)ip};
    u32 n = *len < 16 ? *len : 16;
    memcpy(addr, sin, n);
    *len = 16;
}
static const u8 V4MAPPED[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
static int is_mapped(const u8 *a) { for (int i = 0; i < 12; i++) if (a[i] != V4MAPPED[i]) return 0; return 1; }
static void map4(unsigned ip, u8 *o) {
    memcpy(o, V4MAPPED, 12);
    o[12] = (u8)(ip >> 24); o[13] = (u8)(ip >> 16); o[14] = (u8)(ip >> 8); o[15] = (u8)ip;
}
/* sockaddr_in6 do macOS: len(1) family(1)=30 port(2) flowinfo(4) addr(16) scope_id(4) = 28 */
static void put_sin6(void *addr, u32 *len, const u8 *a16, unsigned short port) {
    if (!addr || !len) return;
    u8 s6[28];
    memset(s6, 0, sizeof s6);
    s6[0] = 28; s6[1] = 30; s6[2] = (u8)(port >> 8); s6[3] = (u8)port;
    memcpy(s6 + 8, a16, 16);
    u32 n = *len < 28 ? *len : 28;
    memcpy(addr, s6, n);
    *len = 28;
}
/* endereço de 16 bytes no formato da família do socket */
static void put_sa(sock_t *s, void *addr, u32 *len, const u8 *a16, unsigned short port) {
    if (s->family == 30) put_sin6(addr, len, a16, port);
    else put_sin(addr, len, is_mapped(a16) ? be32(a16 + 12) : 0, port);
}
/* sockaddr do app -> 16 bytes; 0 ou o errno */
static int get_sa(sock_t *s, const void *addr, u32 len, u8 *a16, unsigned short *port) {
    const u8 *a = addr;
    if (!a || len < 2) return CX_EINVAL_;
    if (a[1] == 2) {
        if (len < 8) return CX_EINVAL_;
        map4(be32(a + 4), a16);
    } else if (a[1] == 30) {
        if (len < 24) return CX_EINVAL_;
        if (s->family == 2) return CX_EAFNOSUPPORT;
        memcpy(a16, a + 8, 16);
        if (s->v6only && is_mapped(a16)) return CX_EAFNOSUPPORT;
    } else return CX_EAFNOSUPPORT;
    *port = (unsigned short)(a[2] << 8 | a[3]);
    return 0;
}
static int errno_of(long k) {
    switch (-k) {
    case MOS_EMFILE: return CX_EMFILE;
    case MOS_ECONNREFUSED: return CX_ECONNREFUSED;
    case MOS_EADDRINUSE: return CX_EADDRINUSE;
    case MOS_EADDRNOTAVAIL: return CX_EADDRNOTAVAIL;
    case MOS_EINVAL: return CX_EINVAL_;
    default: return CX_ENETUNREACH;
    }
}

/* atualiza o estado de um connect em andamento; devolve 1 se conectou, 0 se ainda não, -1 se falhou */
static int conn_check(sock_t *s) {
    if (!s->connecting) return s->kc >= 0 && !s->err ? 1 : -1;
    long st = net(MOS_NET_STATE, s->kc, 0, 0);
    if (st == MOS_NET_ST_CONNECTING) return 0;
    s->connecting = 0;
    if (st == MOS_NET_ST_OPEN || st == MOS_NET_ST_PEER_CLOSED) return 1;
    s->err = CX_ECONNREFUSED;
    return -1;
}

static int in_connect(sock_t *s, const void *addr, u32 len) {
    u8 a16[16];
    unsigned short port;
    int e = get_sa(s, addr, len, a16, &port);
    if (e) { cx_errno = e; return -1; }
    if (s->proto) { memcpy(s->raddr, a16, 16); s->rport = 0; s->kc = 0; return 0; }   /* ping: só guarda o destino */
    if (s->kc >= 0) { cx_errno = s->connecting ? CX_EALREADY : CX_EISCONN; return -1; }
    if (s->kl >= 0) { cx_errno = CX_EINVAL_; return -1; }
    long k = net(MOS_NET_CONNECT6, (long)a16, port, 0);       /* o laço local sem escuta já volta recusado */
    if (k < 0) { cx_errno = errno_of(k); return -1; }
    s->kc = (int)k; memcpy(s->raddr, a16, 16); s->rport = port; s->connecting = 1; s->err = 0;
    if (s->nonblock) { cx_errno = CX_EINPROGRESS; return -1; }
    u64 end = (u64)SYS0(SYS_TIME) + 75000;                     /* prazo do connect do BSD */
    for (;;) {
        int r = conn_check(s);
        if (r > 0) return 0;
        if (r < 0) { cx_errno = s->err; s->err = 0; return -1; }
        if ((u64)SYS0(SYS_TIME) >= end) { cx_errno = CX_ETIMEDOUT; return -1; }
        SYS1(SYS_SLEEP, 5);
    }
}

static long in_send(sock_t *s, const void *b, size_t n) {
    int c = conn_check(s);
    if (c == 0) { cx_errno = CX_EAGAIN; return -1; }
    if (c < 0) { cx_errno = s->err ? s->err : CX_ENOTCONN; return -1; }
    size_t done = 0;
    while (done < n) {
        long k = net(MOS_NET_SEND, s->kc, (long)((const u8 *)b + done), (long)(n - done));
        if (k < 0) { if (done) break; cx_errno = CX_EPIPE; return -1; }
        done += (size_t)k;
        if (done < n) {
            if (s->nonblock) break;
            if (k == 0) SYS1(SYS_SLEEP, 2);                    /* buffer de envio do kernel cheio */
        }
    }
    if (!done && n && s->nonblock) { cx_errno = CX_EAGAIN; return -1; }
    return (long)done;
}

/* flags de recv do macOS */
#define CX_MSG_PEEK 0x2
#define CX_MSG_WAITALL 0x40
#define CX_MSG_DONTWAIT 0x80
/* entrega primeiro os bytes guardados por um MSG_PEEK anterior; com MSG_PEEK eles ficam guardados */
static size_t peek_take(sock_t *s, void *b, size_t n, int keep) {
    size_t k = n < s->peek_len ? n : s->peek_len;
    memcpy(b, s->peek, k);
    if (!keep) {
        s->peek_len = (unsigned short)(s->peek_len - k);
        for (size_t i = 0; i < s->peek_len; i++) s->peek[i] = s->peek[k + i];
    }
    return k;
}
/* O kernel não sabe "espiar": o MSG_PEEK tira os bytes do kernel para s->peek e devolve uma cópia. O Chromium usa
 * recv(fd, &c, 1, MSG_PEEK) em SocketPosix::IsConnectedAndIdle ao reaproveitar um socket do pool; consumir esse byte
 * desalinhava o fluxo TLS (record_overflow / bad_record_mac / wrong_version_number = ERR_SSL_PROTOCOL_ERROR). */
static long in_recv(sock_t *s, void *b, size_t n, int fl) {
    int peek = (fl & CX_MSG_PEEK) != 0, nb = s->nonblock || (fl & CX_MSG_DONTWAIT);
    if (s->peek_len && n && (peek || !(fl & CX_MSG_WAITALL) || n <= s->peek_len))
        return (long)peek_take(s, b, n, peek);
    int c = conn_check(s);
    if (c == 0) { cx_errno = CX_EAGAIN; return -1; }
    if (c < 0 && !s->peek_len) { cx_errno = s->err ? s->err : CX_ENOTCONN; return -1; }
    if (!n) return 0;
    size_t got = 0;
    if (!peek && s->peek_len) got = peek_take(s, b, n, 0);       /* MSG_WAITALL: começa pelo guardado */
    for (;;) {
        long k;
        if (peek) {
            size_t room = sizeof s->peek - s->peek_len;
            k = room ? net(MOS_NET_RECV, s->kc, (long)(s->peek + s->peek_len), (long)room) : 0;
            if (k > 0) s->peek_len = (unsigned short)(s->peek_len + k);
            if (s->peek_len) return (long)peek_take(s, b, n, 1);
        } else {
            k = net(MOS_NET_RECV, s->kc, (long)((u8 *)b + got), (long)(n - got));
            if (k > 0) {
                got += (size_t)k;
                if (!(fl & CX_MSG_WAITALL) || got >= n) return (long)got;
                continue;
            }
        }
        if (k < 0) return (long)got;                           /* fim do fluxo (FIN) */
        if (got && (nb || !(fl & CX_MSG_WAITALL))) return (long)got;
        if (nb) { cx_errno = CX_EAGAIN; return -1; }
        SYS1(SYS_SLEEP, 2);
    }
}

/* ---- ping (SOCK_DGRAM + IPPROTO_ICMP/ICMPV6). A resposta fica em s->peek: [16 bytes de origem][mensagem ICMP] */
static int icmp_fetch(sock_t *s) {
    if (s->peek_len) return 1;
    long k = net(MOS_NET_ICMP_RECV, s->icmp_id, (long)s->peek, (long)sizeof s->peek);
    if (k > 16) { s->peek_len = (unsigned short)k; return 1; }
    return 0;
}
static long icmp_sendto(sock_t *s, const void *b, size_t n, const void *a, u32 al) {
    u8 a16[16];
    unsigned short port;
    if (a) { int e = get_sa(s, a, al, a16, &port); if (e) { cx_errno = e; return -1; } }
    else if (s->kc == 0) memcpy(a16, s->raddr, 16);
    else { cx_errno = CX_ENOTCONN; return -1; }
    if (n < 8) { cx_errno = CX_EINVAL_; return -1; }
    s->icmp_id = (unsigned short)(((const u8 *)b)[4] << 8 | ((const u8 *)b)[5]);
    long k = net(MOS_NET_ICMP_SEND, (long)a16, (long)b, (long)n);
    if (k < 0) { cx_errno = errno_of(k); return -1; }
    return k;
}
/* no IPv4 o Darwin entrega o cabeçalho IP antes do ICMP; no ICMPv6, só a mensagem */
static long icmp_recvfrom(sock_t *s, void *b, size_t n, int fl, void *a, u32 *al) {
    while (!icmp_fetch(s)) {
        if (s->nonblock || (fl & CX_MSG_DONTWAIT)) { cx_errno = CX_EAGAIN; return -1; }
        SYS1(SYS_SLEEP, 5);
    }
    const u8 *from = s->peek, *msg = s->peek + 16;
    size_t mlen = (size_t)s->peek_len - 16, off = 0;
    u8 out[20 + sizeof s->peek];
    if (s->family == 2) {
        unsigned info[4] = {0};
        net(MOS_NET_INFO, (long)info, 0, 0);
        memset(out, 0, 20);
        out[0] = 0x45; out[2] = (u8)((20 + mlen) >> 8); out[3] = (u8)(20 + mlen); out[8] = 64; out[9] = 1;
        memcpy(out + 12, from + 12, 4);
        out[16] = (u8)(info[0] >> 24); out[17] = (u8)(info[0] >> 16); out[18] = (u8)(info[0] >> 8); out[19] = (u8)info[0];
        off = 20;
    }
    memcpy(out + off, msg, mlen);
    size_t tot = off + mlen, k = n < tot ? n : tot;
    memcpy(b, out, k);
    if (a && al) put_sa(s, a, al, from, 0);
    if (!(fl & CX_MSG_PEEK)) s->peek_len = 0;
    return (long)k;
}

/* ---- servidor */
static int in_bind(sock_t *s, const void *addr, u32 len) {
    u8 a16[16];
    unsigned short port;
    int e = get_sa(s, addr, len, a16, &port);
    if (e) { cx_errno = e; return -1; }
    if (s->bound || s->kc >= 0 || s->kl >= 0) { cx_errno = CX_EINVAL_; return -1; }
    memcpy(s->laddr, a16, 16);
    s->lport = port;
    s->bound = 1;
    return 0;
}
static int in_listen(sock_t *s, int backlog) {
    if (s->kl >= 0) return 0;                                  /* listen de novo: só muda o backlog no BSD */
    if (s->kc >= 0 || s->proto) { cx_errno = CX_EINVAL_; return -1; }
    u8 any[16];
    const u8 *a = s->laddr;
    if (!s->bound) { memset(any, 0, 16); if (s->family == 2) map4(0, any); a = any; }
    int fam = s->family == 2 ? MOS_NET_L_V4 : (s->v6only ? MOS_NET_L_V6 : MOS_NET_L_V4 | MOS_NET_L_V6);
    if (s->family == 30 && is_mapped(a)) fam = MOS_NET_L_V4;     /* bind em ::ffff:x: só IPv4 */
    if (backlog <= 0 || backlog > 0xFFFF) backlog = 128;
    long k = net(MOS_NET_LISTEN, (long)a, s->lport, (long)(backlog | fam << 16));
    if (k < 0) { cx_errno = errno_of(k); return -1; }
    s->kl = (int)k;
    s->listening = 1;
    mos_net_addrs_t ad;
    if (net(MOS_NET_ADDRS, s->kl, (long)&ad, 0) == 0) s->lport = ad.lport;   /* porta 0: a que o kernel escolheu */
    return 0;
}
static int in_accept(sock_t *s, void *addr, u32 *len) {
    if (s->kl < 0) { cx_errno = CX_EINVAL_; return -1; }
    long k;
    for (;;) {
        k = net(MOS_NET_ACCEPT, s->kl, 0, 0);
        if (k >= 0) break;
        if (k != -MOS_EAGAIN) { cx_errno = errno_of(k); return -1; }
        if (s->nonblock) { cx_errno = CX_EAGAIN; return -1; }
        SYS1(SYS_SLEEP, 5);
    }
    int fd = socket(s->family, 1, 6);
    sock_t *c = sock_of(fd);
    if (!c) { net(MOS_NET_CLOSE, k, 0, 0); return -1; }
    c->kc = (int)k;
    c->nonblock = s->nonblock;                                /* o BSD herda o O_NONBLOCK do ouvinte */
    c->v6only = s->v6only;
    mos_net_addrs_t ad;
    if (net(MOS_NET_ADDRS, c->kc, (long)&ad, 0) == 0) {
        memcpy(c->raddr, ad.rip, 16); c->rport = ad.rport;
        memcpy(c->laddr, ad.lip, 16); c->lport = ad.lport;
        c->bound = 1;
    }
    if (addr && len) put_sa(c, addr, len, c->raddr, c->rport);
    return fd;
}

/* ---- funções públicas: AF_UNIX segue o caminho de antes (un_*) */
int bind(int fd, const void *addr, u32 len) {
    sock_t *s = sock_of(fd);
    if (is_inet(s)) return in_bind(s, addr, len);
    return un_bind(fd, addr, len);
}
int listen(int fd, int backlog) {
    sock_t *s = sock_of(fd);
    if (is_inet(s)) return in_listen(s, backlog);
    return un_listen(fd, backlog);
}
int accept(int fd, void *addr, u32 *len) {
    sock_t *s = sock_of(fd);
    if (is_inet(s)) return in_accept(s, addr, len);
    return un_accept(fd, addr, len);
}
int connect(int fd, const void *addr, u32 len) {
    sock_t *s = sock_of(fd);
    if (is_inet(s)) return in_connect(s, addr, len);
    return un_connect(fd, addr, len);
}
#define SOL_SOCKET 0xffff
#define SO_ERROR 0x1007
#define SO_TYPE 0x1008
#define CX_IPPROTO_IPV6 41
#define CX_IPV6_V6ONLY 27
int setsockopt(int fd, int level, int name, const void *v, u32 len) {
    sock_t *s = sock_of(fd);
    if (s && s->family == 30 && level == CX_IPPROTO_IPV6 && name == CX_IPV6_V6ONLY && v && len >= 4) {
        if (s->kl >= 0) { cx_errno = CX_EINVAL_; return -1; }
        s->v6only = *(const int *)v != 0;
        return 0;
    }
    return s || fd_open(fd) ? 0 : (cx_errno = CX_EBADF, -1);   /* o resto vale como feito (veja fd_open) */
}
int getsockopt(int fd, int level, int name, void *v, u32 *len) {
    sock_t *s = sock_of(fd);
    if (is_inet(s) && level == SOL_SOCKET && v && len && *len >= 4) {
        if (name == SO_ERROR) { if (s->kc > 0 || !s->proto) conn_check(s); *(int *)v = s->connecting ? 0 : s->err; s->err = 0; *len = 4; return 0; }
        if (name == SO_TYPE) { *(int *)v = s->proto ? 2 : 1; *len = 4; return 0; }
    }
    if (is_inet(s) && s->family == 30 && level == CX_IPPROTO_IPV6 && name == CX_IPV6_V6ONLY && v && len && *len >= 4) {
        *(int *)v = s->v6only; *len = 4; return 0;
    }
    return un_getsockopt(fd, level, name, v, len);
}
int getsockname(int fd, void *addr, u32 *len) {
    sock_t *s = sock_of(fd);
    if (is_inet(s)) {
        mos_net_addrs_t ad;
        int h = s->kl >= 0 ? s->kl : !s->proto ? s->kc : -1;
        if (h >= 0 && net(MOS_NET_ADDRS, h, (long)&ad, 0) == 0) { put_sa(s, addr, len, ad.lip, ad.lport); return 0; }
        if (s->bound) { put_sa(s, addr, len, s->laddr, s->lport); return 0; }
        u8 a16[16];
        memset(a16, 0, 16);
        if (s->family == 2) {
            unsigned info[4] = {0};
            net(MOS_NET_INFO, (long)info, 0, 0);
            map4(info[0], a16);
        }
        put_sa(s, addr, len, a16, 0);
        return 0;
    }
    return un_getsockname(fd, addr, len);
}
int getpeername(int fd, void *addr, u32 *len) {
    sock_t *s = sock_of(fd);
    if (!s) { cx_errno = CX_EBADF; return -1; }
    if (!is_inet(s) || s->kc < 0 || (!s->proto && conn_check(s) <= 0)) { cx_errno = CX_ENOTCONN; return -1; }
    put_sa(s, addr, len, s->raddr, s->rport);
    return 0;
}
long send(int fd, const void *b, size_t n, int fl) {
    (void)fl;
    sock_t *s = sock_of(fd);
    if (!s) { cx_errno = CX_EBADF; return -1; }
    if (!is_inet(s)) { cx_errno = CX_ENOTCONN; return -1; }
    if (s->proto) return icmp_sendto(s, b, n, 0, 0);
    return in_send(s, b, n);
}
long recv(int fd, void *b, size_t n, int fl) {
    /* MSG_PEEK, MSG_WAITALL e MSG_DONTWAIT: in_recv */
    sock_t *s = sock_of(fd);
    if (!s) { cx_errno = CX_EBADF; return -1; }
    if (!is_inet(s)) { cx_errno = CX_ENOTCONN; return -1; }
    if (s->proto) return icmp_recvfrom(s, b, n, fl, 0, 0);
    return in_recv(s, b, n, fl);
}
/* struct msghdr do macOS: name(8) namelen(4)+pad iov(8) iovlen(4)+pad control(8) controllen(4) flags(4) */
typedef struct { void *base; size_t len; } cx_iovec_t;
typedef struct { void *name; u32 namelen; cx_iovec_t *iov; int iovlen; void *control; u32 controllen; int flags; } cx_msghdr_t;
long sendmsg(int fd, const void *m, int fl) {
    const cx_msghdr_t *h = m;
    long total = 0;
    for (int i = 0; h && i < h->iovlen; i++) {
        if (!h->iov[i].len) continue;
        long k = send(fd, h->iov[i].base, h->iov[i].len, fl);
        if (k < 0) return total ? total : -1;
        total += k;
        if ((size_t)k < h->iov[i].len) break;
    }
    return total;
}
long recvmsg(int fd, void *m, int fl) {
    cx_msghdr_t *h = m;
    if (h) { h->controllen = 0; h->flags = 0; }
    for (int i = 0; h && i < h->iovlen; i++)
        if (h->iov[i].len) return recv(fd, h->iov[i].base, h->iov[i].len, fl);   /* o primeiro bloco com espaço */
    return 0;
}
/* sendto devolve o resultado em RAX e 0 em RDX, como o XNU (rval[1]). O Chromium 15x, no macOS de 13.3 a 26.1, confere o
 * RDX depois de sendto(): se não for 0, supõe o bug do kernel em que a chamada não aconteceu e devolve
 * ERR_NETWORK_CHANGED (-21) a toda escrita de socket (TCP e UDP), o "Slack cannot connect -21". Uma função C comum
 * deixa lixo no RDX; uma struct de dois inteiros volta em RAX:RDX pelo ABI x86-64. */
typedef struct { long ret, rdx; } cx_sysret_t;
cx_sysret_t sendto(int fd, const void *b, size_t n, int fl, const void *a, u32 al) {
    sock_t *s = sock_of(fd);
    if (s && s->proto) return (cx_sysret_t){icmp_sendto(s, b, n, a, al), 0};
    return (cx_sysret_t){send(fd, b, n, fl), 0};
}
long recvfrom(int fd, void *b, size_t n, int fl, void *a, u32 *al) {
    sock_t *s = sock_of(fd);
    if (s && s->proto) return icmp_recvfrom(s, b, n, fl, a, al);
    long r = recv(fd, b, n, fl);
    if (r >= 0 && a && al && is_inet(s)) put_sa(s, a, al, s->raddr, s->rport);
    else if (al) *al = 0;
    return r;
}

/* ---- ganchos do libc (read/write/fcntl/ioctl) e do kqueue/poll (cx_fd_readable) para descritores de socket */
int cx_sock_is(int fd) { return sock_of(fd) != 0; }
long cx_sock_read(int fd, void *b, size_t n) { return recv(fd, b, n, 0); }
long cx_sock_write(int fd, const void *b, size_t n) { return send(fd, b, n, 0); }
void cx_sock_set_nonblock(int fd, int on) { sock_t *s = sock_of(fd); if (s) s->nonblock = on != 0; }
int cx_sock_get_nonblock(int fd) { sock_t *s = sock_of(fd); return s && s->nonblock; }
/* legível: dados chegando, fim do fluxo, erro de connect, conexão para accept ou resposta de ping;
 * *eof = 1 quando não virá mais nada */
int cx_sock_readable(int fd, long *avail, int *eof) {
    sock_t *s = sock_of(fd);
    *avail = 0; *eof = 0;
    if (!is_inet(s)) return 0;
    if (s->proto) { if (icmp_fetch(s)) { *avail = s->peek_len - 16; return 1; } return 0; }
    if (s->kl >= 0) { long n = net(MOS_NET_AVAIL, s->kl, 0, 0); if (n > 0) { *avail = n; return 1; } return 0; }
    if (s->kc < 0) return 0;
    if (s->peek_len) { *avail = s->peek_len + (conn_check(s) > 0 ? net(MOS_NET_AVAIL, s->kc, 0, 0) : 0); return 1; }
    int c = conn_check(s);
    if (c == 0) return 0;
    if (c < 0) { *eof = 1; return 1; }
    long n = net(MOS_NET_AVAIL, s->kc, 0, 0);
    if (n > 0) { *avail = n; return 1; }
    long st = net(MOS_NET_STATE, s->kc, 0, 0);
    if (st == MOS_NET_ST_OPEN || st == MOS_NET_ST_CONNECTING) return 0;   /* aberto e sem dados: não acorda */
    *eof = 1;                                                  /* fechado pelo outro lado ou erro, e vazio */
    return 1;
}
/* gravável: conectado (o buffer do kernel tem 64 KiB) ou connect que terminou (com erro também, para o SO_ERROR) */
int cx_sock_writable(int fd) {
    sock_t *s = sock_of(fd);
    if (!is_inet(s)) return 1;
    if (s->proto) return 1;
    if (s->kc < 0) return 0;
    return conn_check(s) != 0;
}

/* close() do libc entrega aqui os descritores que não são do kernel (sockets e kqueues) */
int cx_kqueue_close(int fd);
int cx_fake_fd_close(int fd) {
    sock_t *s = sock_of(fd);
    if (s) {
        if (is_inet(s) && !s->proto && s->kc >= 0) net(MOS_NET_CLOSE, s->kc, 0, 0);
        if (is_inet(s) && s->kl >= 0) net(MOS_NET_CLOSE, s->kl, 0, 0);
        s->kc = -1; s->kl = -1;
        s->used = 0;
        return 0;
    }
    if (cx_kqueue_close(fd) == 0) return 0;
    cx_errno = CX_EBADF;
    return -1;
}

/* ================================================================ resolução de nomes: getaddrinfo e cia.
 * Registros A e AAAA do DNS do kernel (ordem: getaddrinfo abaixo). struct addrinfo do macOS: flags family socktype protocol (4 int),
 * addrlen (u32), canonname, addr, next (ponteiros) — canonname vem ANTES de addr, ao contrário do Linux. */
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
char *strdup(const char *s);
int strcmp(const char *a, const char *b);
typedef struct cx_addrinfo {
    int flags, family, socktype, protocol;
    u32 addrlen;
    char *canonname;
    void *addr;
    struct cx_addrinfo *next;
} cx_addrinfo_t;
#define EAI_AGAIN 2
#define EAI_FAIL 4
#define EAI_FAMILY 5
#define EAI_MEMORY 6
#define EAI_NONAME 8
#define EAI_SERVICE 9
#define EAI_SYSTEM 11
#define AI_PASSIVE 0x1
#define AI_CANONNAME 0x2
#define AI_NUMERICHOST 0x4

static int parse_ipv4(const char *s, unsigned *ip) {
    unsigned v = 0, part = 0;
    int dots = 0, digits = 0;
    for (;; s++) {
        if (*s >= '0' && *s <= '9') { part = part * 10 + (unsigned)(*s - '0'); if (part > 255 || ++digits > 3) return 0; }
        else if (*s == '.' || !*s) {
            if (!digits) return 0;
            v = v << 8 | part; part = 0; digits = 0;
            if (!*s) break;
            if (++dots > 3) return 0;
        } else return 0;
    }
    if (dots != 3) return 0;
    *ip = v;
    return 1;
}
/* serviço: número, ou os nomes comuns (o mOS não tem /etc/services) */
static int parse_port(const char *svc, unsigned short *port) {
    if (!svc || !*svc) { *port = 0; return 1; }
    static const struct { const char *n; unsigned short p; } known[] = {
        {"http", 80}, {"https", 443}, {"ftp", 21}, {"ssh", 22}, {"smtp", 25}, {"domain", 53}, {"imap", 143}, {"imaps", 993},
    };
    for (unsigned i = 0; i < sizeof known / sizeof known[0]; i++) if (!strcmp(svc, known[i].n)) { *port = known[i].p; return 1; }
    unsigned v = 0;
    for (const char *p = svc; *p; p++) { if (*p < '0' || *p > '9') return 0; v = v * 10 + (unsigned)(*p - '0'); if (v > 65535) return 0; }
    *port = (unsigned short)v;
    return 1;
}
#define AI_ALL 0x100
#define AI_V4MAPPED_CFG 0x200
#define AI_ADDRCONFIG 0x400
#define AI_V4MAPPED 0x800
static int parse_ipv6(const char *s, u8 out[16]);
/* "fe80::1%en0" e "[::1]": tira os colchetes e a zona */
static int parse_ipv6_host(const char *h, u8 out[16]) {
    char t[64];
    size_t n = strlen(h);
    if (n >= sizeof t) return 0;
    if (h[0] == '[' && n > 2 && h[n - 1] == ']') { memcpy(t, h + 1, n - 2); t[n - 2] = 0; }
    else memcpy(t, h, n + 1);
    for (char *p = t; *p; p++) if (*p == '%') { *p = 0; break; }
    return parse_ipv6(t, out);
}
/* A e AAAA do DNS do kernel, pedidos juntos; got4 e got6 dizem o que veio. Devolve 0 ou EAI_x */
static int resolve_host(const char *host, int want4, int want6, unsigned *ip, int *got4, u8 *ip6, int *got6) {
    *got4 = *got6 = 0;
    if (!strcmp(host, "localhost")) {
        if (want4) { *ip = 0x7F000001; *got4 = 1; }
        if (want6) { memset(ip6, 0, 16); ip6[15] = 1; *got6 = 1; }
        return 0;
    }
    int done4 = !want4, done6 = !want6;
    u64 end = (u64)SYS0(SYS_TIME) + 10000;                      /* o DNS do kernel tenta 4 vezes a cada 1,5 s */
    for (;;) {
        if (!done4) { long r = net(MOS_NET_RESOLVE, (long)host, (long)ip, 0); if (r) { done4 = 1; *got4 = r == 1; } }
        if (!done6) { long r = net(MOS_NET_RESOLVE6, (long)host, (long)ip6, 0); if (r) { done6 = 1; *got6 = r == 1; } }
        if (done4 && done6) return *got4 || *got6 ? 0 : EAI_NONAME;
        if ((u64)SYS0(SYS_TIME) >= end) return *got4 || *got6 ? 0 : EAI_AGAIN;
        SYS1(SYS_SLEEP, 10);
    }
}
static int ai_add(cx_addrinfo_t ***tail, int fam, int socktype, const u8 *a16, unsigned short port, const char *canon) {
    cx_addrinfo_t *ai = calloc(1, sizeof *ai + 28);
    if (!ai) return EAI_MEMORY;
    ai->family = fam;
    ai->socktype = socktype;
    ai->protocol = socktype == 1 ? 6 : socktype == 2 ? 17 : 0;
    ai->addr = (u8 *)(ai + 1);
    u32 l = 28;
    if (fam == 30) put_sin6(ai->addr, &l, a16, port);
    else put_sin(ai->addr, &l, be32(a16 + 12), port);
    ai->addrlen = l;
    if (canon) ai->canonname = strdup(canon);
    **tail = ai; *tail = &ai->next;
    return 0;
}
/* Ordem e famílias (RFC 6724 resumido, pensando nos apps Chromium):
 *  - AF_UNSPEC: IPv4 primeiro, depois IPv6 — e AAAA só quando há IPv6 global com roteador (AI_ADDRCONFIG do
 *    macOS, que é o padrão sem dicas). Assim um AAAA inalcançável (rede sem IPv6, ou o user-net do QEMU com ipv6=on
 *    num host sem IPv6 de saída) nunca é tentado antes do A. Numa rede só IPv6 sobra o AAAA.
 *  - AF_INET6: AAAA; com AI_V4MAPPED, A mapeados (::ffff:a.b.c.d) quando não há AAAA, ou sempre com AI_ALL.
 *  - "localhost" e host vazio têm os dois (127.0.0.1 e ::1). */
int getaddrinfo(const char *host, const char *svc, const cx_addrinfo_t *hints, cx_addrinfo_t **res) {
    if (!res) return EAI_FAIL;
    *res = 0;
    int fam = hints ? hints->family : 0, flags = hints ? hints->flags : (AI_V4MAPPED_CFG | AI_ADDRCONFIG);
    if (fam != 0 && fam != 2 && fam != 30) return EAI_FAMILY;  /* AF_UNSPEC, AF_INET ou AF_INET6 */
    unsigned short port;
    if (!parse_port(svc, &port)) return EAI_SERVICE;
    unsigned ip = 0;
    u8 ip6[16];
    int got4 = 0, got6 = 0;
    int want4 = fam != 30 || (flags & (AI_V4MAPPED | AI_V4MAPPED_CFG)), want6 = fam != 2;
    mos_net6_info_t n6;
    int v6route = net(MOS_NET_INFO6, (long)&n6, 0, 0) == 1;
    if (!host || !*host) {
        got4 = fam != 30; got6 = fam != 2;
        memset(ip6, 0, 16);
        if (!(flags & AI_PASSIVE)) { ip = 0x7F000001; ip6[15] = 1; }
    } else if (parse_ipv4(host, &ip)) {
        got4 = 1;
        if (fam == 30 && !(flags & (AI_V4MAPPED | AI_V4MAPPED_CFG))) return EAI_NONAME;
    } else if (parse_ipv6_host(host, ip6)) {
        if (fam == 2) return EAI_NONAME;
        got6 = 1;
    } else if (flags & AI_NUMERICHOST) return EAI_NONAME;
    else {
        int ask6 = want6 && (fam == 30 || v6route || !(flags & AI_ADDRCONFIG) || !strcmp(host, "localhost"));
        if (fam == 0 && !v6route && strcmp(host, "localhost")) ask6 = 0;   /* AF_UNSPEC sem rota IPv6: só A */
        int e = resolve_host(host, want4, ask6, &ip, &got4, ip6, &got6);
        if (e) return e;
    }
    /* um resultado por tipo de socket pedido (sem pedido: TCP e UDP, como o macOS) */
    int types[2] = {1, 2}, ntypes = 2;
    if (hints && hints->socktype) { types[0] = hints->socktype; ntypes = 1; }
    const char *canon = (flags & AI_CANONNAME) && host ? host : 0;
    cx_addrinfo_t *head = 0, **tail = &head;
    u8 m4[16];
    map4(ip, m4);
    int e = 0;
    for (int pass = 0; pass < 2 && !e; pass++) {
        /* pass 0: IPv4 (ou A mapeado em AF_INET6); pass 1: IPv6 */
        for (int i = 0; i < ntypes && !e; i++) {
            if (pass == 0 && got4 && fam != 30) e = ai_add(&tail, 2, types[i], m4, port, canon);
            else if (pass == 1 && got6 && fam != 2) e = ai_add(&tail, 30, types[i], ip6, port, canon);
            else if (pass == 1 && fam == 30 && got4 && (!got6 || (flags & AI_ALL))) e = ai_add(&tail, 30, types[i], m4, port, canon);
            if (head) canon = 0;
        }
    }
    if (e) { extern void freeaddrinfo(cx_addrinfo_t *); freeaddrinfo(head); return e; }
    if (!head) return EAI_NONAME;
    *res = head;
    return 0;
}
void freeaddrinfo(cx_addrinfo_t *ai) {
    while (ai) { cx_addrinfo_t *n = ai->next; free(ai->canonname); free(ai); ai = n; }
}
const char *gai_strerror(int e) {
    switch (e) {
    case 0: return "Success";
    case EAI_AGAIN: return "Temporary failure in name resolution";
    case EAI_FAIL: return "Non-recoverable failure in name resolution";
    case EAI_FAMILY: return "ai_family not supported";
    case EAI_MEMORY: return "Memory allocation failure";
    case EAI_NONAME: return "nodename nor servname provided, or not known";
    case EAI_SERVICE: return "servname not supported for ai_socktype";
    case EAI_SYSTEM: return "System error";
    default: return "Unknown error";
    }
}
static int fmt_ipv4(unsigned ip, char *out, size_t cap) {
    char t[16];
    int n = 0;
    for (int i = 3; i >= 0; i--) {
        unsigned b = (ip >> (i * 8)) & 255;
        if (b >= 100) t[n++] = (char)('0' + b / 100);
        if (b >= 10) t[n++] = (char)('0' + b / 10 % 10);
        t[n++] = (char)('0' + b % 10);
        if (i) t[n++] = '.';
    }
    t[n] = 0;
    if ((size_t)n + 1 > cap) return 0;
    memcpy(out, t, (size_t)n + 1);
    return 1;
}
int getnameinfo(const void *sa, u32 salen, char *host, u32 hlen, char *serv, u32 slen, int flags) {
    (void)flags;                                               /* sem DNS reverso: sempre numérico */
    const u8 *a = sa;
    if (!a || salen < 8 || (a[1] != 2 && a[1] != 30) || (a[1] == 30 && salen < 24)) return EAI_FAMILY;
    if (host && hlen) {
        extern const char *inet_ntop(int af, const void *src, char *dst, u32 size);
        if (a[1] == 2 ? !fmt_ipv4(be32(a + 4), host, hlen) : !inet_ntop(30, a + 8, host, hlen)) return EAI_FAIL;
    }
    if (serv && slen) {
        unsigned p = (unsigned)(a[2] << 8 | a[3]);
        char t[8]; int n = 0;
        do { t[n++] = (char)('0' + p % 10); p /= 10; } while (p);
        if ((u32)n + 1 > slen) return EAI_FAIL;
        for (int i = 0; i < n; i++) serv[i] = t[n - 1 - i];
        serv[n] = 0;
    }
    return 0;
}
/* IPv6 em texto (RFC 4291: grupos hexadecimais, um "::" e IPv4 no fim). 1 = válido, 0 = não é IPv6.
 * O curl testa "inet_pton(AF_INET6, host) != 0" para saber se o host é um endereço: devolver -1 aqui fazia
 * todo nome de host virar "endereço IPv6" e a verificação do certificado TLS falhar. */
static int parse_ipv6(const char *s, u8 out[16]) {
    u16 g[8];
    int n = 0, gap = -1;
    if (s[0] == ':') { if (s[1] != ':') return 0; s++; }
    while (*s) {
        if (*s == ':') {
            if (gap >= 0) return 0;
            gap = n;
            s++;
            if (!*s) break;
            continue;
        }
        const char *dot = 0;
        for (const char *q = s; *q && *q != ':'; q++) if (*q == '.') { dot = q; break; }
        if (dot) {                                            /* IPv4 embutido: só no fim */
            unsigned ip;
            if (n > 6 || !parse_ipv4(s, &ip)) return 0;
            g[n++] = (u16)(ip >> 16); g[n++] = (u16)ip;
            s += strlen(s);
            break;
        }
        unsigned v = 0;
        int k = 0;
        for (; k < 4; k++, s++) {
            char c = *s;
            int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (d < 0) break;
            v = v << 4 | (unsigned)d;
        }
        if (!k || n >= 8) return 0;
        g[n++] = (u16)v;
        if (*s == ':') { s++; if (!*s) return 0; if (*s == ':') continue; }
        else if (*s) return 0;
    }
    if (gap < 0 ? n != 8 : n > 7) return 0;
    u16 full[8] = {0};
    if (gap < 0) memcpy(full, g, sizeof full);
    else {
        for (int i = 0; i < gap; i++) full[i] = g[i];
        for (int i = gap; i < n; i++) full[8 - (n - i)] = g[i];
    }
    for (int i = 0; i < 8; i++) { out[2 * i] = (u8)(full[i] >> 8); out[2 * i + 1] = (u8)full[i]; }
    return 1;
}
int inet_pton(int af, const char *src, void *dst) {
    unsigned ip;
    if (af == 30) return parse_ipv6(src, dst);                /* AF_INET6 */
    if (af != 2) { cx_errno = CX_EAFNOSUPPORT; return -1; }
    if (!parse_ipv4(src, &ip)) return 0;
    u8 *d = dst;
    d[0] = (u8)(ip >> 24); d[1] = (u8)(ip >> 16); d[2] = (u8)(ip >> 8); d[3] = (u8)ip;
    return 1;
}
const char *inet_ntop(int af, const void *src, char *dst, u32 size) {
    if (af == 30) {                                           /* AF_INET6: grupos em hexa, a maior sequência de zeros vira "::" */
        const u8 *a = src;
        if (is_mapped(a)) {                                   /* ::ffff:10.0.2.15, como o macOS */
            char v4[16];
            fmt_ipv4(be32(a + 12), v4, sizeof v4);
            if (strlen(v4) + 8 > size) { cx_errno = 28; return 0; }
            memcpy(dst, "::ffff:", 7);
            memcpy(dst + 7, v4, strlen(v4) + 1);
            return dst;
        }
        u16 g[8];
        for (int i = 0; i < 8; i++) g[i] = (u16)(a[2 * i] << 8 | a[2 * i + 1]);
        int bs = -1, bl = 0;
        for (int i = 0; i < 8;) {
            if (g[i]) { i++; continue; }
            int j = i;
            while (j < 8 && !g[j]) j++;
            if (j - i > bl && j - i > 1) { bs = i; bl = j - i; }
            i = j;
        }
        char buf[48];
        u32 o = 0;
        static const char hx[] = "0123456789abcdef";
        for (int i = 0; i < 8; i++) {
            if (i == bs) { buf[o++] = ':'; i += bl - 1; continue; }   /* o ':' do grupo seguinte completa o "::" */
            if (i) buf[o++] = ':';
            int started = 0;
            for (int sh = 12; sh >= 0; sh -= 4) {
                int d = (g[i] >> sh) & 15;
                if (d || started || sh == 0) { buf[o++] = hx[d]; started = 1; }
            }
        }
        if (bs >= 0 && bs + bl == 8) buf[o++] = ':';          /* "1::" e "::" terminam com o par */
        buf[o] = 0;
        if (o + 1 > size) { cx_errno = 28; return 0; }
        memcpy(dst, buf, o + 1);
        return dst;
    }
    if (af != 2) { cx_errno = CX_EAFNOSUPPORT; return 0; }
    if (!fmt_ipv4(be32(src), dst, size)) { cx_errno = 28; return 0; }   /* ENOSPC */
    return dst;
}
unsigned inet_addr(const char *s) {
    unsigned ip;
    if (!parse_ipv4(s, &ip)) return 0xFFFFFFFFu;               /* INADDR_NONE */
    return (ip >> 24) | ((ip >> 8) & 0xFF00) | ((ip << 8) & 0xFF0000) | (ip << 24);   /* ordem de rede */
}
