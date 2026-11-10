/* CGEventTap no processo e, por AF_UNIX, nos outros.
 * Cada tap escuta SOCK_DGRAM em /tmp/mos-cgevent/<pid>. CGEventPost manda um
 * pacote de 32 bytes para os pids em /tmp/mos-cgevent/pids. O pacote só acorda
 * o callback daqui: não volta para a fila do kernel. */
#if defined(__linux__) && !defined(__APPLE__)
#define _DEFAULT_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#define CGT_LINUX 1
#else
#define CGT_LINUX 0
int socket(int, int, int);
int bind(int, const void *, unsigned);
int close(int);
int unlink(const char *);
int mkdir(const char *, int);
int getpid(void);
int usleep(unsigned);
int pthread_create(unsigned long *, const void *, void *(*)(void *), void *);
int open(const char *, int, ...);
long read(int, void *, unsigned long);
long write(int, const void *, unsigned long);
void *memset(void *, int, unsigned long);
typedef struct { long ret, rdx; } cx_sysret_t;
cx_sysret_t sendto(int, const void *, unsigned long, int, const void *, unsigned);
long recvfrom(int, void *, unsigned long, int, void *, unsigned *);
#endif

enum { CGT_N = 8 };

typedef struct {
    int used, enabled;
    unsigned long long mask;
    void *(*cb)(void *proxy, unsigned type, void *event, void *info);
    void *info;
} cgt_t;

static cgt_t g_tap[CGT_N];
static int g_fd = -1;
static int g_pid;
static int g_thread;
static void (*g_wire)(unsigned type, int x, int y, int key, int button, int down, int wheel, unsigned flags);

static void put_u32(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
static unsigned get_u32(const unsigned char *p) {
    return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}
static void pid_path(int pid, char *out, int cap) {
    static const char prefix[] = "/tmp/mos-cgevent/";
    char tmp[16];
    int i = 0, d = 0, p = pid < 0 ? 0 : pid;
    while (prefix[i] && i + 1 < cap) out[i] = prefix[i], i++;
    if (!p) tmp[d++] = '0';
    while (p && d < (int)sizeof tmp) tmp[d++] = (char)('0' + (p % 10)), p /= 10;
    while (d && i + 1 < cap) out[i++] = tmp[--d];
    out[i] = 0;
}

#if CGT_LINUX
static int fill_addr(struct sockaddr_un *u, const char *path, unsigned *len) {
    int i = 0;
    memset(u, 0, sizeof *u);
    u->sun_family = 1;
    while (path[i] && i < (int)sizeof u->sun_path - 1) u->sun_path[i] = path[i], i++;
    *len = (unsigned)(2 + i + 1);
    return i > 0;
}
static int cg_send(int fd, const char *path, const void *b, int n) {
    struct sockaddr_un u;
    unsigned len = 0;
    if (!fill_addr(&u, path, &len)) return -1;
    return sendto(fd, b, (size_t)n, 0, (struct sockaddr *)&u, len) < 0 ? -1 : 0;
}
#else
static int fill_addr(unsigned char *buf, const char *path, unsigned *len) {
    int i = 0;
    memset(buf, 0, 106);
    buf[1] = 1;
    while (path[i] && i < 103) buf[2 + i] = (unsigned char)path[i], i++;
    buf[0] = (unsigned char)(i + 2);
    *len = (unsigned)(i + 3);
    return i > 0;
}
static int cg_send(int fd, const char *path, const void *b, int n) {
    unsigned char addr[106];
    unsigned len = 0;
    cx_sysret_t r;
    if (!fill_addr(addr, path, &len)) return -1;
    r = sendto(fd, b, (unsigned long)n, 0, addr, len);
    return r.ret < 0 ? -1 : 0;
}
#endif

static int ensure(void) {
    char path[64];
    int pid = getpid();
    if (g_fd >= 0 && g_pid == pid) return 0;
    if (g_fd >= 0) { close(g_fd); g_fd = -1; g_thread = 0; }
    mkdir("/tmp", 0755);
    mkdir("/tmp/mos-cgevent", 0755);
    pid_path(getpid(), path, (int)sizeof path);
    unlink(path);
    g_pid = pid;
    g_fd = socket(1, 2, 0);
    if (g_fd < 0) return -1;
#if CGT_LINUX
    {
        struct sockaddr_un u;
        unsigned len = 0;
        if (!fill_addr(&u, path, &len) || bind(g_fd, (struct sockaddr *)&u, len) != 0) {
            close(g_fd); g_fd = -1; return -1;
        }
    }
#else
    {
        unsigned char addr[106];
        unsigned len = 0;
        if (!fill_addr(addr, path, &len) || bind(g_fd, addr, len) != 0) {
            close(g_fd); g_fd = -1; return -1;
        }
    }
#endif
    return 0;
}

static void remember_pid(void) {
#if CGT_LINUX
    int fd = open("/tmp/mos-cgevent/pids", O_RDWR | O_CREAT | O_APPEND, 0644);
#else
    int fd = open("/tmp/mos-cgevent/pids", 0x0002 | 0x0200 | 0x0008, 0644);
#endif
    char line[32];
    char buf[1024];
    int n = 0, pid = getpid(), i = 0;
    long got;
    if (fd < 0) return;
    got = read(fd, buf, sizeof buf);
    if (got < 0) got = 0;
    while (pid && n < (int)sizeof line - 2) { line[n++] = (char)('0' + (pid % 10)); pid /= 10; }
    if (!n) line[n++] = '0';
    for (i = 0; i < n / 2; i++) { char t = line[i]; line[i] = line[n - 1 - i]; line[n - 1 - i] = t; }
    line[n++] = '\n';
    line[n] = 0;
    {
        int have = 0, k = 0;
        while (k + n <= (int)got) {
            int m = 0;
            while (m < n && buf[k + m] == line[m]) m++;
            if (m == n) { have = 1; break; }
            while (k < (int)got && buf[k] != '\n') k++;
            if (k < (int)got) k++;
        }
        if (!have) write(fd, line, (unsigned long)n);
    }
    close(fd);
}

static void *wire_thread(void *arg) {
    (void)arg;
    while (g_fd >= 0) {
        unsigned char b[32];
#if CGT_LINUX
        struct sockaddr_un u;
        socklen_t l = sizeof u;
        ssize_t r = recvfrom(g_fd, b, sizeof b, 0, (struct sockaddr *)&u, &l);
#else
        unsigned char addr[106];
        unsigned l = sizeof addr;
        long r = recvfrom(g_fd, b, sizeof b, 0, addr, &l);
#endif
        if (r < 32 || !g_wire) continue;
        g_wire(get_u32(b), (int)get_u32(b + 4), (int)get_u32(b + 8), (int)get_u32(b + 12),
               (int)get_u32(b + 16), (int)get_u32(b + 20), (int)get_u32(b + 24), get_u32(b + 28));
    }
    return 0;
}

static int start_thread(void) {
    if (g_thread) return 0;
#if CGT_LINUX
    pthread_t tid;
    if (pthread_create(&tid, 0, wire_thread, 0) != 0) return -1;
#else
    unsigned long tid = 0;
    if (pthread_create(&tid, 0, wire_thread, 0) != 0) return -1;
#endif
    g_thread = 1;
    return 0;
}

void cx_cgtap_set_wire(void (*fn)(unsigned, int, int, int, int, int, int, unsigned)) { g_wire = fn; }

void *cx_cgtap_create(unsigned long long mask, void *cb, void *info) {
    int i;
    if (ensure() != 0 || start_thread() != 0) return 0;
    remember_pid();
    for (i = 0; i < CGT_N; i++) if (!g_tap[i].used) {
        g_tap[i].used = 1;
        g_tap[i].enabled = 1;
        g_tap[i].mask = mask;
        g_tap[i].cb = cb;
        g_tap[i].info = info;
        return &g_tap[i];
    }
    return 0;
}

void cx_cgtap_enable(void *tap, int on) {
    cgt_t *t = tap;
    if (!t) return;
    for (int i = 0; i < CGT_N; i++) if (t == &g_tap[i] && t->used) t->enabled = on ? 1 : 0;
}

int cx_cgtap_is_on(void *tap) {
    cgt_t *t = tap;
    if (!t) return 0;
    for (int i = 0; i < CGT_N; i++) if (t == &g_tap[i] && t->used) return t->enabled;
    return 0;
}

void *cx_cgtap_offer(unsigned type, void *event) {
    int i;
    unsigned long long bit = type < 64 ? (1ull << type) : 0;
    for (i = 0; i < CGT_N; i++) {
        cgt_t *t = &g_tap[i];
        void *r;
        if (!t->used || !t->enabled || !t->cb) continue;
        if (bit && !(t->mask & bit)) continue;
        r = t->cb(t, type, event, t->info);
        if (!r) return 0;
        event = r;
    }
    return event;
}

void cx_cgtap_fanout(unsigned type, int x, int y, int key, int button, int down, int wheel, unsigned flags) {
    unsigned char pkt[32];
    char buf[1024];
    int fd, self = getpid();
    long n;
    int i = 0;
    if (ensure() != 0) return;
    put_u32(pkt, type); put_u32(pkt + 4, (unsigned)x); put_u32(pkt + 8, (unsigned)y);
    put_u32(pkt + 12, (unsigned)key); put_u32(pkt + 16, (unsigned)button);
    put_u32(pkt + 20, (unsigned)down); put_u32(pkt + 24, (unsigned)wheel); put_u32(pkt + 28, flags);
    fd = open("/tmp/mos-cgevent/pids", 0, 0);
    if (fd < 0) return;
    n = read(fd, buf, sizeof buf);
    close(fd);
    if (n < 0) return;
    while (i < (int)n) {
        int pid = 0, any = 0;
        char path[64];
        while (i < (int)n && buf[i] >= '0' && buf[i] <= '9') { pid = pid * 10 + (buf[i] - '0'); i++; any = 1; }
        if (any && pid != self && pid > 0) {
            pid_path(pid, path, (int)sizeof path);
            cg_send(g_fd, path, pkt, 32);
        }
        while (i < (int)n && buf[i] != '\n') i++;
        if (i < (int)n && buf[i] == '\n') i++;
    }
}
