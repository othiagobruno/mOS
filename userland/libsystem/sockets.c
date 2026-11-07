/*
 * mOS libSystem — sockets e links simbólicos, para o porte do VS Code/Electron (docs/porte-vscode.md).
 *
 * O kernel do mOS ainda não tem a API de sockets. AF_UNIX existe só dentro do processo: socket/bind/
 * listen funcionam (bind cria o arquivo do socket), e connect sem servidor neste processo devolve
 * ECONNREFUSED — o que basta para o VS Code e o Chromium concluírem "sou a primeira instância".
 * AF_INET e AF_INET6 (TCP, cliente e servidor) passam pela pilha de rede do kernel (SYS_NET, sockets_inet.inc). Os
 * descritores ficam a partir de SOCK_FD_BASE; read/write/fcntl/ioctl/close do libc e o kqueue/poll os entregam aqui.
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
} sock_t;
static sock_t socks[MAX_SOCK];

static sock_t *sock_of(int fd) {
    int i = fd - SOCK_FD_BASE;
    return i >= 0 && i < MAX_SOCK && socks[i].used ? &socks[i] : 0;
}

int socket(int domain, int type, int proto) {
    if (domain != 1 && domain != 2 && domain != 30) { cx_errno = CX_EAFNOSUPPORT; return -1; }   /* AF_UNIX, AF_INET, AF_INET6 */
    int icmp = 0;
    if (domain != 1 && (type & 0xF) != 1) {
        /* fora o TCP, só o "socket de ping" do Darwin (SOCK_DGRAM + ICMP/ICMPv6). UDP ainda não: o teste de IPv6 do
         * Chromium (socket UDP para 2001:4860:4860::8888) falha e ele pede só registros A, como antes */
        if ((type & 0xF) == 2 && ((domain == 2 && proto == 1) || (domain == 30 && proto == 58))) icmp = proto;
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
            return SOCK_FD_BASE + i;
        }
    cx_errno = CX_EMFILE;
    return -1;
}
int socketpair(int domain, int type, int proto, int sv[2]) {
    (void)domain; (void)type; (void)proto;
    /* par bidirecional do kernel (dois pipes cruzados, kernel/proc/process.c): o libuv liga o stdin dos filhos assim */
    extern int cx_socketpair_fds(int *fds);
    return cx_socketpair_fds(sv);
}

/* sockaddr_un do macOS: {u8 len; u8 family; char path[104]} */
static const char *un_path(const void *addr, u32 len) {
    if (!addr || len < 3) return 0;
    return (const char *)addr + 2;
}
static sock_t *listener_at(const char *path) {
    for (int i = 0; i < MAX_SOCK; i++)
        if (socks[i].used && socks[i].listening && !strncmp(socks[i].path, path, sizeof socks[i].path)) return &socks[i];
    return 0;
}

static int un_bind(int fd, const void *addr, u32 len) {
    sock_t *s = sock_of(fd);
    if (!s) { cx_errno = CX_EBADF; return -1; }
    const char *p = un_path(addr, len);
    if (!p || !*p) { cx_errno = CX_EINVAL; return -1; }
    if (access(p, 0) == 0) { cx_errno = CX_EADDRINUSE; return -1; }
    strlcpy(s->path, p, sizeof s->path);
    int f = open(p, 0x0202, 0600);                               /* cria o "arquivo de socket" */
    if (f >= 0) { extern int close(int); close(f); }
    return 0;
}
static int un_listen(int fd, int backlog) {
    (void)backlog;
    sock_t *s = sock_of(fd);
    if (!s) { cx_errno = CX_EBADF; return -1; }
    s->listening = 1;
    return 0;
}
static int un_connect(int fd, const void *addr, u32 len) {
    sock_t *s = sock_of(fd);
    if (!s) { cx_errno = CX_EBADF; return -1; }
    const char *p = un_path(addr, len);
    (void)listener_at;
    (void)p;
    cx_errno = CX_ECONNREFUSED;                                  /* ainda sem conexões dentro do processo */
    return -1;
}
static int un_accept(int fd, void *addr, u32 *len) { (void)addr; (void)len; cx_errno = sock_of(fd) ? CX_EAGAIN : CX_EBADF; return -1; }
/* as pontas de um socketpair são pipes do kernel (acima): para elas shutdown/setsockopt valem como feitos,
 * senão o Crashpad do Chrome (child_port_handshake.cc) aborta com EBADF. (O fcntl(F_GETFD) do mOS ainda não
 * distingue fd fechado, então na prática só fd negativo dá EBADF.) */
static int fd_open(int fd) { extern int fcntl(int, int, ...); return fd >= 0 && fcntl(fd, 1 /* F_GETFD */) >= 0; }
int shutdown(int fd, int how) { (void)how; return sock_of(fd) || fd_open(fd) ? 0 : (cx_errno = CX_EBADF, -1); }
/* setsockopt: sockets_inet.inc (IPV6_V6ONLY; o resto vale como feito) */
static int un_getsockopt(int fd, int level, int name, void *v, u32 *len) {
    (void)level; (void)name;
    if (!sock_of(fd)) { cx_errno = CX_EBADF; return -1; }
    if (v && len && *len >= 4) { *(int *)v = 0; *len = 4; }
    return 0;
}
static int un_getsockname(int fd, void *addr, u32 *len) {
    sock_t *s = sock_of(fd);
    if (!s) { cx_errno = CX_EBADF; return -1; }
    if (addr && len && *len >= 3) {
        u8 *a = addr;
        size_t n = strlen(s->path);
        if (n + 3 > *len) n = *len - 3;
        a[0] = (u8)(n + 2); a[1] = 1;
        memcpy(a + 2, s->path, n);
        a[2 + n] = 0;
        *len = (u32)(n + 3);
    }
    return 0;
}
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
