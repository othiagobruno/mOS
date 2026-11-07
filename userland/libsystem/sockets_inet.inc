/* ================================================================ AF_INET (TCP) pelo kernel: SYS_NET
 * (kernel/net/usersock.c). O kernel é não-bloqueante; aqui o modo bloqueante espera em fatias de 1–10 ms.
 * O que os apps-alvo usam (docs/compat-apps.md): connect bloqueante ou com O_NONBLOCK + poll/kqueue + SO_ERROR
 * (Chromium, libuv, tokio/mio do Codex), send/recv/read/write, sendmsg/recvmsg com iovec, getpeername. Ainda não:
 * servidor TCP (bind/listen/accept em AF_INET = EOPNOTSUPP), UDP, IPv6, loopback (127.x = ECONNREFUSED: o kernel
 * não tem escuta local). */
#define CX_EOPNOTSUPP 102
static long net(long op, long a, long b, long c) { return SYS4(SYS_NET, op, a, b, c); }
static int is_inet(sock_t *s) { return s && s->family == 2; }
static unsigned be32(const u8 *p) { return (unsigned)p[0] << 24 | (unsigned)p[1] << 16 | (unsigned)p[2] << 8 | p[3]; }
static void put_sin(void *addr, u32 *len, unsigned ip, unsigned short port) {
    if (!addr || !len) return;
    u8 sin[16] = {16, 2, (u8)(port >> 8), (u8)port, (u8)(ip >> 24), (u8)(ip >> 16), (u8)(ip >> 8), (u8)ip};
    u32 n = *len < 16 ? *len : 16;
    memcpy(addr, sin, n);
    *len = 16;
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
    const u8 *a = addr;
    if (!a || len < 8 || a[1] != 2) { cx_errno = CX_EAFNOSUPPORT; return -1; }
    if (s->kc >= 0) { cx_errno = s->connecting ? CX_EALREADY : CX_EISCONN; return -1; }
    unsigned short port = (unsigned short)(a[2] << 8 | a[3]);
    unsigned ip = be32(a + 4);
    if ((ip >> 24) == 127 || !ip) { cx_errno = CX_ECONNREFUSED; return -1; }
    long k = net(MOS_NET_CONNECT, ip, port, 0);
    if (k < 0) { cx_errno = k == -MOS_EMFILE ? CX_EMFILE : CX_ENETUNREACH; return -1; }
    s->kc = (int)k; s->rip = ip; s->rport = port; s->connecting = 1; s->err = 0;
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

static long in_recv(sock_t *s, void *b, size_t n) {
    int c = conn_check(s);
    if (c == 0) { cx_errno = CX_EAGAIN; return -1; }
    if (c < 0) { cx_errno = s->err ? s->err : CX_ENOTCONN; return -1; }
    if (!n) return 0;
    for (;;) {
        long k = net(MOS_NET_RECV, s->kc, (long)b, (long)n);
        if (k > 0) return k;
        if (k < 0) return 0;                                   /* fim do fluxo (FIN) */
        if (s->nonblock) { cx_errno = CX_EAGAIN; return -1; }
        SYS1(SYS_SLEEP, 2);
    }
}

/* ---- funções públicas: AF_UNIX segue o caminho de antes (un_*) */
int bind(int fd, const void *addr, u32 len) {
    sock_t *s = sock_of(fd);
    if (is_inet(s)) return 0;                                  /* porta local efêmera: o kernel escolhe */
    return un_bind(fd, addr, len);
}
int listen(int fd, int backlog) {
    sock_t *s = sock_of(fd);
    if (is_inet(s)) { cx_errno = CX_EOPNOTSUPP; return -1; }
    return un_listen(fd, backlog);
}
int accept(int fd, void *addr, u32 *len) {
    sock_t *s = sock_of(fd);
    if (is_inet(s)) { cx_errno = CX_EOPNOTSUPP; return -1; }
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
int getsockopt(int fd, int level, int name, void *v, u32 *len) {
    sock_t *s = sock_of(fd);
    if (is_inet(s) && level == SOL_SOCKET && v && len && *len >= 4) {
        if (name == SO_ERROR) { conn_check(s); *(int *)v = s->connecting ? 0 : s->err; s->err = 0; *len = 4; return 0; }
        if (name == SO_TYPE) { *(int *)v = 1; *len = 4; return 0; }
    }
    return un_getsockopt(fd, level, name, v, len);
}
int getsockname(int fd, void *addr, u32 *len) {
    sock_t *s = sock_of(fd);
    if (is_inet(s)) {
        unsigned info[4] = {0};
        net(MOS_NET_INFO, (long)info, 0, 0);
        put_sin(addr, len, info[0], 0);
        return 0;
    }
    return un_getsockname(fd, addr, len);
}
int getpeername(int fd, void *addr, u32 *len) {
    sock_t *s = sock_of(fd);
    if (!s) { cx_errno = CX_EBADF; return -1; }
    if (!is_inet(s) || s->kc < 0 || conn_check(s) <= 0) { cx_errno = CX_ENOTCONN; return -1; }
    put_sin(addr, len, s->rip, s->rport);
    return 0;
}
long send(int fd, const void *b, size_t n, int fl) {
    (void)fl;
    sock_t *s = sock_of(fd);
    if (!s) { cx_errno = CX_EBADF; return -1; }
    if (!is_inet(s)) { cx_errno = CX_ENOTCONN; return -1; }
    return in_send(s, b, n);
}
long recv(int fd, void *b, size_t n, int fl) {
    (void)fl;                                                  /* MSG_PEEK/MSG_WAITALL: ainda não */
    sock_t *s = sock_of(fd);
    if (!s) { cx_errno = CX_EBADF; return -1; }
    if (!is_inet(s)) { cx_errno = CX_ENOTCONN; return -1; }
    return in_recv(s, b, n);
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
long sendto(int fd, const void *b, size_t n, int fl, const void *a, u32 al) { (void)a; (void)al; return send(fd, b, n, fl); }
long recvfrom(int fd, void *b, size_t n, int fl, void *a, u32 *al) {
    long r = recv(fd, b, n, fl);
    sock_t *s = sock_of(fd);
    if (r >= 0 && a && al && is_inet(s)) put_sin(a, al, s->rip, s->rport);
    else if (al) *al = 0;
    return r;
}

/* ---- ganchos do libc (read/write/fcntl/ioctl) e do kqueue/poll (cx_fd_readable) para descritores de socket */
int cx_sock_is(int fd) { return sock_of(fd) != 0; }
long cx_sock_read(int fd, void *b, size_t n) { return recv(fd, b, n, 0); }
long cx_sock_write(int fd, const void *b, size_t n) { return send(fd, b, n, 0); }
void cx_sock_set_nonblock(int fd, int on) { sock_t *s = sock_of(fd); if (s) s->nonblock = on != 0; }
int cx_sock_get_nonblock(int fd) { sock_t *s = sock_of(fd); return s && s->nonblock; }
/* legível: dados chegando, fim do fluxo ou erro de connect; *eof = 1 quando não virá mais nada */
int cx_sock_readable(int fd, long *avail, int *eof) {
    sock_t *s = sock_of(fd);
    *avail = 0; *eof = 0;
    if (!is_inet(s) || s->kc < 0) return 0;
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
    if (s->kc < 0) return 0;
    return conn_check(s) != 0;
}

/* close() do libc entrega aqui os descritores que não são do kernel (sockets e kqueues) */
int cx_kqueue_close(int fd);
int cx_fake_fd_close(int fd) {
    sock_t *s = sock_of(fd);
    if (s) {
        if (is_inet(s) && s->kc >= 0) net(MOS_NET_CLOSE, s->kc, 0, 0);
        s->kc = -1;
        s->used = 0;
        return 0;
    }
    if (cx_kqueue_close(fd) == 0) return 0;
    cx_errno = CX_EBADF;
    return -1;
}

/* ================================================================ resolução de nomes: getaddrinfo e cia.
 * Só IPv4 (registros A do DNS do kernel). struct addrinfo do macOS: flags family socktype protocol (4 int),
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
static int resolve_host(const char *host, unsigned *ip) {
    if (parse_ipv4(host, ip)) return 0;
    if (!strcmp(host, "localhost")) { *ip = 0x7F000001; return 0; }
    u64 end = (u64)SYS0(SYS_TIME) + 10000;                      /* o DNS do kernel tenta 4 vezes a cada 1,5 s */
    for (;;) {
        long r = net(MOS_NET_RESOLVE, (long)host, (long)ip, 0);
        if (r == 1) return 0;
        if (r < 0) return EAI_NONAME;
        if ((u64)SYS0(SYS_TIME) >= end) return EAI_AGAIN;
        SYS1(SYS_SLEEP, 10);
    }
}
int getaddrinfo(const char *host, const char *svc, const cx_addrinfo_t *hints, cx_addrinfo_t **res) {
    if (!res) return EAI_FAIL;
    *res = 0;
    int fam = hints ? hints->family : 0, flags = hints ? hints->flags : 0;
    if (fam != 0 && fam != 2) return EAI_FAMILY;               /* AF_UNSPEC ou AF_INET */
    unsigned short port;
    if (!parse_port(svc, &port)) return EAI_SERVICE;
    unsigned ip;
    if (!host || !*host) ip = (flags & AI_PASSIVE) ? 0 : 0x7F000001;
    else if (flags & AI_NUMERICHOST) { if (!parse_ipv4(host, &ip)) return EAI_NONAME; }
    else { int e = resolve_host(host, &ip); if (e) return e; }
    /* um resultado por tipo de socket pedido (sem pedido: TCP e UDP, como o macOS) */
    int types[2] = {1, 2}, ntypes = 2;
    if (hints && hints->socktype) { types[0] = hints->socktype; ntypes = 1; }
    cx_addrinfo_t *head = 0, **tail = &head;
    for (int i = 0; i < ntypes; i++) {
        cx_addrinfo_t *ai = calloc(1, sizeof *ai + 16);
        if (!ai) { extern void freeaddrinfo(cx_addrinfo_t *); freeaddrinfo(head); return EAI_MEMORY; }
        ai->family = 2;
        ai->socktype = types[i];
        ai->protocol = types[i] == 1 ? 6 : types[i] == 2 ? 17 : 0;
        ai->addrlen = 16;
        ai->addr = (u8 *)(ai + 1);
        u32 l = 16;
        put_sin(ai->addr, &l, ip, port);
        if (i == 0 && (flags & AI_CANONNAME) && host) ai->canonname = strdup(host);
        *tail = ai; tail = &ai->next;
    }
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
    if (!a || salen < 8 || a[1] != 2) return EAI_FAMILY;
    if (host && hlen && !fmt_ipv4(be32(a + 4), host, hlen)) return EAI_FAIL;
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
