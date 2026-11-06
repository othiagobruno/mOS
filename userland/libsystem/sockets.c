/*
 * mOS libSystem — sockets e links simbólicos, para o porte do VS Code/Electron (docs/porte-vscode.md).
 *
 * O kernel do mOS ainda não tem a API de sockets. AF_UNIX existe só dentro do processo: socket/bind/
 * listen funcionam (bind cria o arquivo do socket), e connect sem servidor neste processo devolve
 * ECONNREFUSED — o que basta para o VS Code e o Chromium concluírem "sou a primeira instância".
 * AF_INET/AF_INET6 devolvem EAFNOSUPPORT por enquanto. Os descritores ficam a partir de SOCK_FD_BASE
 * e não passam pelo kernel; close() os entrega a cx_fake_fd_close.
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

#define SOCK_FD_BASE 0x5000
#define MAX_SOCK 64
#define CX_EAFNOSUPPORT 47
#define CX_ECONNREFUSED 61
#define CX_EAGAIN 35
#define CX_ENOTCONN 57
#define CX_EADDRINUSE 48

typedef struct { int used, family, type, listening; char path[108]; } sock_t;
static sock_t socks[MAX_SOCK];

static sock_t *sock_of(int fd) {
    int i = fd - SOCK_FD_BASE;
    return i >= 0 && i < MAX_SOCK && socks[i].used ? &socks[i] : 0;
}

int socket(int domain, int type, int proto) {
    (void)proto;
    if (domain != 1) { cx_errno = CX_EAFNOSUPPORT; return -1; }   /* só AF_UNIX */
    for (int i = 0; i < MAX_SOCK; i++)
        if (!__sync_lock_test_and_set(&socks[i].used, 1)) {
            socks[i].family = domain; socks[i].type = type; socks[i].listening = 0; socks[i].path[0] = 0;
            return SOCK_FD_BASE + i;
        }
    cx_errno = CX_EMFILE;
    return -1;
}
int socketpair(int domain, int type, int proto, int sv[2]) {
    (void)domain; (void)type; (void)proto;
    /* um pipe do kernel em cada sentido não cabe num par só; usa um pipe (meio-duplex) */
    extern int pipe(int *fds);
    return pipe(sv);
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

int bind(int fd, const void *addr, u32 len) {
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
int listen(int fd, int backlog) {
    (void)backlog;
    sock_t *s = sock_of(fd);
    if (!s) { cx_errno = CX_EBADF; return -1; }
    s->listening = 1;
    return 0;
}
int connect(int fd, const void *addr, u32 len) {
    sock_t *s = sock_of(fd);
    if (!s) { cx_errno = CX_EBADF; return -1; }
    const char *p = un_path(addr, len);
    (void)listener_at;
    (void)p;
    cx_errno = CX_ECONNREFUSED;                                  /* ainda sem conexões dentro do processo */
    return -1;
}
int accept(int fd, void *addr, u32 *len) { (void)addr; (void)len; cx_errno = sock_of(fd) ? CX_EAGAIN : CX_EBADF; return -1; }
int shutdown(int fd, int how) { (void)how; return sock_of(fd) ? 0 : (cx_errno = CX_EBADF, -1); }
int setsockopt(int fd, int level, int name, const void *v, u32 len) { (void)level; (void)name; (void)v; (void)len; return sock_of(fd) ? 0 : (cx_errno = CX_EBADF, -1); }
int getsockopt(int fd, int level, int name, void *v, u32 *len) {
    (void)level; (void)name;
    if (!sock_of(fd)) { cx_errno = CX_EBADF; return -1; }
    if (v && len && *len >= 4) { *(int *)v = 0; *len = 4; }
    return 0;
}
int getsockname(int fd, void *addr, u32 *len) {
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
int getpeername(int fd, void *addr, u32 *len) { (void)addr; (void)len; cx_errno = sock_of(fd) ? CX_ENOTCONN : CX_EBADF; return -1; }
long send(int fd, const void *b, size_t n, int fl) { (void)b; (void)n; (void)fl; cx_errno = sock_of(fd) ? CX_ENOTCONN : CX_EBADF; return -1; }
long recv(int fd, void *b, size_t n, int fl) { (void)b; (void)n; (void)fl; cx_errno = sock_of(fd) ? CX_ENOTCONN : CX_EBADF; return -1; }
long sendmsg(int fd, const void *m, int fl) { (void)m; (void)fl; cx_errno = sock_of(fd) ? CX_ENOTCONN : CX_EBADF; return -1; }
long recvmsg(int fd, void *m, int fl) { (void)m; (void)fl; cx_errno = sock_of(fd) ? CX_ENOTCONN : CX_EBADF; return -1; }
long sendto(int fd, const void *b, size_t n, int fl, const void *a, u32 al) { (void)a; (void)al; return send(fd, b, n, fl); }
long recvfrom(int fd, void *b, size_t n, int fl, void *a, u32 *al) { (void)a; (void)al; return recv(fd, b, n, fl); }

/* close() do libc entrega aqui os descritores que não são do kernel (sockets e kqueues) */
int cx_kqueue_close(int fd);
int cx_fake_fd_close(int fd) {
    sock_t *s = sock_of(fd);
    if (s) { s->used = 0; return 0; }
    if (cx_kqueue_close(fd) == 0) return 0;
    cx_errno = CX_EBADF;
    return -1;
}

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
