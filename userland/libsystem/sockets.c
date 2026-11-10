/*
 * mOS libSystem — sockets e links simbólicos, para o porte do VS Code/Electron (docs/porte-vscode.md).
 *
 * AF_UNIX é um descritor do kernel (SYS_UNIX, kernel/net/unix.c, docs/af-unix.md): o número cabe em
 * 0..255 e read/write/poll seguem o caminho normal do libc. AF_INET e AF_INET6 continuam em SOCK_FD_BASE
 * (SYS_NET, sockets_inet.inc).
 *
 * symlink/readlink: o FAT não tem links; o link vira um arquivo pequeno marcado com o destino.
 */
#include "sys.h"

size_t strlen(const char *s);
size_t strlcpy(char *d, const char *s, size_t n);
int strncmp(const char *a, const char *b, size_t n);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int open(const char *path, int flags, ...);
long read(int fd, void *b, size_t n);
long write(int fd, const void *b, size_t n);
int access(const char *p, int m);
int unlink(const char *p);
extern int cx_errno;

#define SOCK_FD_BASE 768   /* abaixo de 1024 (FD_SETSIZE): o git e o libcurl usam select/fd_set; acima dos 256 fds do kernel */
#define MAX_SOCK 64
#define CX_EAFNOSUPPORT 47
#define CX_ECONNREFUSED 61
#define CX_EAGAIN 35
#define CX_ENOTCONN 57
#define CX_EADDRINUSE 48
#define CX_EPROTONOSUPPORT 43
#define CX_EINPROGRESS 36
#define CX_EALREADY 37
#define CX_EISCONN 56
#define CX_ETIMEDOUT 60
#define CX_ENETUNREACH 51
#define CX_EPIPE 32
#define CX_ECONNRESET 54

typedef struct {
    int used, family, type, listening; char path[108];
    /* AF_INET/AF_INET6 (TCP pelo kernel, SYS_NET): conexão do kernel (-1 = nenhuma), modo, estado do connect.
     * Endereços em 16 bytes (IPv4 como ::ffff:a.b.c.d); kl = ouvinte do kernel (listen); proto = 1/58 nos sockets
     * de ping (SOCK_DGRAM + IPPROTO_ICMP/ICMPV6) */
    int kc, nonblock, connecting, err; unsigned short rport;
    int kl, v6only, bound, proto; unsigned short lport, icmp_id;
    unsigned char raddr[16], laddr[16];
    /* bytes já tirados do kernel por recv(MSG_PEEK) e ainda não entregues (o próximo recv os devolve primeiro) */
    unsigned short peek_len; unsigned char peek[512];
    /* UDP (SOCK_DGRAM, sockets_udp.inc): socket do kernel (-1 = ainda não aberto), connect() guardou um par,
     * prazo de recv (SO_RCVTIMEO, ms; 0 = sem) */
    int udp, ku, uconn, rcvtimeo;
} sock_t;
static sock_t socks[MAX_SOCK];

static sock_t *sock_of(int fd) {
    int i = fd - SOCK_FD_BASE;
    return i >= 0 && i < MAX_SOCK && socks[i].used ? &socks[i] : 0;
}

static long uerr(long r) { if (r < 0) { cx_errno = (int)-r; return -1; } return r; }

int socket(int domain, int type, int proto) {
    if (domain == 1) return (int)uerr(SYS4(SYS_UNIX, MOS_UNIX_SOCKET, domain, type, proto));
    if (domain != 2 && domain != 30) { cx_errno = CX_EAFNOSUPPORT; return -1; }   /* AF_INET, AF_INET6; AF_UNIX já voltou acima */
    int icmp = 0, udp = 0;
    if (domain != 1 && (type & 0xF) != 1) {
        /* fora o TCP: UDP (SOCK_DGRAM com protocolo 0 ou 17, sockets_udp.inc) e o "socket de ping" do Darwin
         * (SOCK_DGRAM + ICMP/ICMPv6) */
        if ((type & 0xF) == 2 && ((domain == 2 && proto == 1) || (domain == 30 && proto == 58))) icmp = proto;
        else if ((type & 0xF) == 2 && (proto == 0 || proto == 17)) udp = 1;
        else { cx_errno = CX_EPROTONOSUPPORT; return -1; }
    }
    for (int i = 0; i < MAX_SOCK; i++)
        if (!__sync_lock_test_and_set(&socks[i].used, 1)) {
            sock_t *s = &socks[i];
            int used = s->used;
            memset(s, 0, sizeof *s);
            s->used = used;
            s->family = domain; s->type = type;
            s->kc = -1; s->kl = -1; s->proto = icmp;
            s->udp = udp; s->ku = -1;
            return SOCK_FD_BASE + i;
        }
    cx_errno = CX_EMFILE;
    return -1;
}
int socketpair(int domain, int type, int proto, int sv[2]) {
    /* AF_UNIX (e AF_UNSPEC, que o Darwin trata como unix) nasce conectado no kernel e sobrevive ao fork.
     * Outro domínio continua nos dois pipes cruzados: o libuv liga o stdin dos filhos assim. */
    if (domain == 0 || domain == 1) return (int)uerr(SYS4(SYS_UNIX, MOS_UNIX_PAIR, type, proto, sv));
    (void)type; (void)proto;
    extern int cx_socketpair_fds(int *fds);
    return cx_socketpair_fds(sv);
}

/* AF_UNIX mora no kernel. Um fd que não é socket (o socketpair antigo, em pipes) continua aceitando
 * shutdown e getsockopt como feitos: o Crashpad do Chrome aborta com EBADF. */
static int fd_open(int fd) { extern int fcntl(int, int, ...); return fd >= 0 && fcntl(fd, 1 /* F_GETFD */) >= 0; }
static int un_bind(int fd, const void *addr, u32 len) { return (int)uerr(SYS4(SYS_UNIX, MOS_UNIX_BIND, fd, addr, len)); }
static int un_listen(int fd, int backlog) { return (int)uerr(SYS4(SYS_UNIX, MOS_UNIX_LISTEN, fd, backlog, 0)); }
static int un_connect(int fd, const void *addr, u32 len) { return (int)uerr(SYS4(SYS_UNIX, MOS_UNIX_CONNECT, fd, addr, len)); }
static int un_accept(int fd, void *addr, u32 *len) { return (int)uerr(SYS4(SYS_UNIX, MOS_UNIX_ACCEPT, fd, addr, len)); }
int shutdown(int fd, int how) {
    if (sock_of(fd)) return 0;
    long r = SYS4(SYS_UNIX, MOS_UNIX_SHUTDOWN, fd, how, 0);
    if (r == 0) return 0;
    if (r == -MOS_ENOTSOCK || r == -MOS_EBADF) return fd_open(fd) ? 0 : (cx_errno = CX_EBADF, -1);
    cx_errno = (int)-r;
    return -1;
}
static int un_getsockopt(int fd, int level, int name, void *v, u32 *len) {
    mos_unix_sockopt_t o;
    o.level = level;
    o.name = name;
    o.len = len ? *len : 0;
    o.val = v;
    long r = SYS3(SYS_UNIX, MOS_UNIX_GETSOCKOPT, fd, &o);
    if (r == 0) { if (len) *len = o.len; return 0; }
    if (r == -MOS_ENOTSOCK || r == -MOS_EBADF) {
        if (!fd_open(fd)) { cx_errno = CX_EBADF; return -1; }
        if (v && len && *len >= 4) { *(int *)v = 0; *len = 4; }
        return 0;
    }
    cx_errno = (int)-r;
    return -1;
}
static int un_getsockname(int fd, void *addr, u32 *len) { return (int)uerr(SYS4(SYS_UNIX, MOS_UNIX_SOCKNAME, fd, addr, len)); }
#include "sockets_inet.inc"
/* ---- links simbólicos como arquivos marcados */
#define LINK_MAGIC "#mOS-link:"
int symlink(const char *target, const char *path) {
    if (access(path, 0) == 0) { cx_errno = CX_EEXIST; return -1; }
    int f = open(path, 0x0202 | 0x0400, 0644);
    if (f < 0) return -1;
    write(f, LINK_MAGIC, sizeof LINK_MAGIC - 1);
    write(f, target, strlen(target));
    extern int close(int);
    close(f);
    return 0;
}
long readlink(const char *path, char *buf, size_t len) {
    int f = open(path, 0);
    if (f < 0) return -1;
    char tmp[1100];
    long n = read(f, tmp, sizeof tmp - 1);
    extern int close(int);
    close(f);
    size_t ml = sizeof LINK_MAGIC - 1;
    if (n < (long)ml || strncmp(tmp, LINK_MAGIC, ml)) { cx_errno = CX_EINVAL; return -1; }
    long t = n - (long)ml;
    if ((size_t)t > len) t = (long)len;
    memcpy(buf, tmp + ml, (size_t)t);
    return t;
}
