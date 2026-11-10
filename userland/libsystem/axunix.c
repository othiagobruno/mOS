/* AX entre processos por AF_UNIX SOCK_DGRAM em /tmp/mos-ax/<pid>.
 * O vtable xpc-mach continua o primeiro. Este aqui entra quando cx_xpc_peer_ready falha.
 * call espera um datagrama com op AX_OP_REPLY. send é aviso. listen é o recvfrom. */
#include "axtransport.h"
#include "axproto.h"

#if defined(__linux__) && !defined(__APPLE__)
#define _DEFAULT_SOURCE
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#define AXU_LINUX 1
#else
#define AXU_LINUX 0
int socket(int domain, int type, int proto);
int bind(int fd, const void *addr, unsigned len);
int close(int fd);
int unlink(const char *path);
int mkdir(const char *path, int mode);
int getpid(void);
int usleep(unsigned us);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
void *memset(void *d, int c, unsigned long n);
typedef struct { long ret, rdx; } cx_sysret_t;
cx_sysret_t sendto(int fd, const void *b, unsigned long n, int fl, const void *a, unsigned al);
long recvfrom(int fd, void *b, unsigned long n, int fl, void *a, unsigned *al);
#endif

enum { AXU_CAP = 640, AXU_WAIT_MS = 2000 };

static int g_fd = -1;
static int g_pid;
static volatile int g_stop;
static volatile int g_wait; /* 0 livre, 1 esperando, 2 resposta pronta */
static int g_wait_from;
static int g_wait_n;
static unsigned char g_wait_buf[AXU_CAP];
static void (*g_fn)(int, const void *, int, void *);
static void *g_ctx;
static int g_thread;

static int path_pid(const char *path) {
    const char *s = path;
    const char *sl = path;
    int v = 0;
    if (!path || !path[0]) return 0;
    while (*s) { if (*s == '/') sl = s + 1; s++; }
    if (*sl < '0' || *sl > '9') return 0;
    while (*sl >= '0' && *sl <= '9') { v = v * 10 + (*sl - '0'); sl++; }
    return v;
}

static void pid_path(int pid, char *out, int cap) {
    static const char prefix[] = "/tmp/mos-ax/";
    int i = 0, d = 0;
    char tmp[16];
    int p = pid < 0 ? 0 : pid;
    while (prefix[i] && i + 1 < cap) { out[i] = prefix[i]; i++; }
    if (p == 0) tmp[d++] = '0';
    while (p && d < (int)sizeof tmp) { tmp[d++] = (char)('0' + (p % 10)); p /= 10; }
    while (d && i + 1 < cap) out[i++] = tmp[--d];
    out[i] = 0;
}

#if AXU_LINUX
static int fill_addr(struct sockaddr_un *u, const char *path, unsigned *len) {
    int i = 0;
    memset(u, 0, sizeof *u);
    u->sun_family = 1;
    while (path[i] && i < (int)sizeof u->sun_path - 1) { u->sun_path[i] = path[i]; i++; }
    *len = (unsigned)(2 + i + 1);
    return i > 0;
}
static int ax_send(int fd, const char *path, const void *b, int n) {
    struct sockaddr_un u;
    unsigned len = 0;
    ssize_t r;
    if (!fill_addr(&u, path, &len)) return -1;
    r = sendto(fd, b, (size_t)n, 0, (struct sockaddr *)&u, len);
    return r < 0 ? -1 : (int)r;
}
static int ax_recv(int fd, void *b, int cap, char *path, int pathcap) {
    struct sockaddr_un u;
    socklen_t len = sizeof u;
    ssize_t r = recvfrom(fd, b, (size_t)cap, 0, (struct sockaddr *)&u, &len);
    int i = 0;
    if (r < 0) return -1;
    if (path && pathcap) {
        while (i + 1 < pathcap && u.sun_path[i]) { path[i] = u.sun_path[i]; i++; }
        path[i] = 0;
    }
    return (int)r;
}
#else
static int fill_addr(unsigned char *buf, const char *path, unsigned *len) {
    int i = 0;
    memset(buf, 0, 2 + 104);
    buf[1] = 1;
    while (path[i] && i < 103) { buf[2 + i] = (unsigned char)path[i]; i++; }
    buf[0] = (unsigned char)(i + 2);
    *len = (unsigned)(i + 3);
    return i > 0;
}
static int ax_send(int fd, const char *path, const void *b, int n) {
    unsigned char addr[2 + 104];
    unsigned len = 0;
    cx_sysret_t r;
    if (!fill_addr(addr, path, &len)) return -1;
    r = sendto(fd, b, (unsigned long)n, 0, addr, len);
    return r.ret < 0 ? -1 : (int)r.ret;
}
static int ax_recv(int fd, void *b, int cap, char *path, int pathcap) {
    unsigned char addr[2 + 104];
    unsigned len = sizeof addr;
    long r = recvfrom(fd, b, (unsigned long)cap, 0, addr, &len);
    int i = 0;
    if (r < 0) return -1;
    if (path && pathcap) {
        while (i + 1 < pathcap && addr[2 + i]) { path[i] = (char)addr[2 + i]; i++; }
        path[i] = 0;
    }
    return (int)r;
}
#endif

static int is_reply(const unsigned char *b, int n) {
    if (n < 6) return 0;
    if (b[0] != 0x41 || b[1] != 0x4d || b[2] != 0x58 || b[3] != 0x31) return 0;
    return b[4] == AX_OP_REPLY && b[5] == 0;
}

static void *axu_thread(void *arg) {
    (void)arg;
    while (!g_stop) {
        unsigned char buf[AXU_CAP];
        char path[108];
        int n = ax_recv(g_fd, buf, AXU_CAP, path, (int)sizeof path);
        int from;
        if (n < 0) continue;
        from = path_pid(path);
        if (g_wait == 1 && from == g_wait_from && is_reply(buf, n)) {
            int c = n > AXU_CAP ? AXU_CAP : n;
            int i;
            for (i = 0; i < c; i++) g_wait_buf[i] = buf[i];
            g_wait_n = c;
            g_wait = 2;
            continue;
        }
        if (g_fn) g_fn(from, buf, n, g_ctx);
    }
    return 0;
}

static int ensure(void) {
    char path[64];
#if AXU_LINUX
    struct sockaddr_un u;
    unsigned len = 0;
#else
    unsigned char addr[2 + 104];
    unsigned len = 0;
#endif
    if (g_fd >= 0 && g_pid == getpid()) return 0;
    if (g_fd >= 0) { close(g_fd); g_fd = -1; g_thread = 0; }
    mkdir("/tmp/mos-ax", 0755);
    g_pid = getpid();
    pid_path(g_pid, path, (int)sizeof path);
    unlink(path);
    g_fd = socket(1, 2, 0);
    if (g_fd < 0) return -1;
#if AXU_LINUX
    if (!fill_addr(&u, path, &len) || bind(g_fd, (struct sockaddr *)&u, len) != 0) {
#else
    if (!fill_addr(addr, path, &len) || bind(g_fd, addr, len) != 0) {
#endif
        close(g_fd);
        g_fd = -1;
        return -1;
    }
    return 0;
}

static int unix_ready(void) { return ensure() == 0; }

static int unix_send(int pid, const void *bytes, int n) {
    char path[64];
    if (ensure() != 0 || pid <= 0 || n < 0 || n > AXU_CAP || (n && !bytes)) return 61;
    pid_path(pid, path, (int)sizeof path);
    return ax_send(g_fd, path, bytes, n) < 0 ? 61 : 0;
}

static int start_thread(void) {
#if AXU_LINUX
    pthread_t tid;
    return pthread_create(&tid, 0, axu_thread, 0);
#else
    unsigned long tid = 0;
    return pthread_create(&tid, 0, axu_thread, 0);
#endif
}

static int unix_listen(void (*fn)(int, const void *, int, void *), void *ctx) {
    if (ensure() != 0) return 61;
    g_fn = fn;
    g_ctx = ctx;
    if (g_thread) return 0;
    if (start_thread() != 0) return 61;
    g_thread = 1;
    return 0;
}

static int unix_call(int pid, const void *req, int n, void *reply, int cap, int *out_n) {
    int i;
    if (out_n) *out_n = 0;
    if (unix_listen(g_fn, g_ctx) != 0) return 61;
    if (pid <= 0 || n < 0 || n > AXU_CAP || cap < 0 || (n && !req)) return 22;
    g_wait_from = pid;
    g_wait_n = 0;
    g_wait = 1;
    if (unix_send(pid, req, n) != 0) { g_wait = 0; return 61; }
    for (i = 0; i < AXU_WAIT_MS; i++) {
        if (g_wait == 2) {
            int c = g_wait_n < cap ? g_wait_n : cap;
            int k;
            unsigned char *dst = reply;
            if (dst) for (k = 0; k < c; k++) dst[k] = g_wait_buf[k];
            if (out_n) *out_n = c;
            g_wait = 0;
            return 0;
        }
        usleep(1000);
    }
    g_wait = 0;
    return 60;
}

static const cx_ax_transport g_unix_ax = {
    "af-unix",
    unix_ready,
    unix_call,
    unix_send,
    unix_listen,
};

void cx_ax_unix_bind(void) { cx_ax_transport_bind(&g_unix_ax); }
