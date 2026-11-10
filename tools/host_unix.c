/* AF_UNIX no host, com o contrato de docs/af-unix.md (SYS_UNIX = 61).
 * read/write/fstat ficam nos syscalls 17/1/19, como no kernel. */
#include "host_unix.h"
#include "../kernel/include/syscall.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#ifndef MOS_EDESTADDRREQ
#define MOS_EDESTADDRREQ 39
#endif

#define UNIX_MAX 128
#define UNIX_PATH 104
#define UNIX_RX (16 * 1024)
#define UNIX_DGRAM 8192
#define UNIX_DGRAMS 32
#define UNIX_BACKLOG 32
#define UNIX_RIGHTS 16
#define UNIX_IOV 8

#define UNIX_SOL_SOCKET 0xffff
#define UNIX_LOCAL_PEERCRED 0x001
#define UNIX_LOCAL_PEERPID 0x002
#define UNIX_LOCAL_PEEREPID 0x003
#define UNIX_SO_ACCEPTCONN 0x0002
#define UNIX_SO_SNDBUF 0x1001
#define UNIX_SO_RCVBUF 0x1002
#define UNIX_SO_ERROR 0x1007
#define UNIX_SO_TYPE 0x1008
#define UNIX_MSG_PEEK 0x2
#define UNIX_MSG_TRUNC 0x10
#define UNIX_MSG_CTRUNC 0x20
#define UNIX_MSG_DONTWAIT 0x80

typedef struct umsg {
    struct umsg *next;
    int len, off, nrights;
    int rights[UNIX_RIGHTS];
    unsigned char *data;
} umsg;

typedef struct uend {
    int used, refs, kind;
    int listening, backlog, qcount, shut_rd, shut_wr, nb;
    int rx_bytes, rx_count;
    char name[UNIX_PATH];
    char dest[UNIX_PATH];
    struct uend *peer;
    struct uend *q[UNIX_BACKLOG];
    umsg *head, *tail;
} uend;

static uend g_end[UNIX_MAX];
static host_unix_hooks g_hk;
static pthread_mutex_t g_mu;
static pthread_cond_t g_cv = PTHREAD_COND_INITIALIZER;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;
static void ux_init_mu(void) {
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_mu, &a);
}
static void ux_lock(void) { pthread_once(&g_once, ux_init_mu); pthread_mutex_lock(&g_mu); }
static void ux_unlock(void) { pthread_mutex_unlock(&g_mu); }

void host_unix_set_hooks(const host_unix_hooks *h) { if (h) g_hk = *h; }

static int slot_of_end(uend *e) { return (int)(e - g_end); }

static uend *end_fd(int fd) {
    if (!g_hk.fd_slot) return NULL;
    int s = g_hk.fd_slot(fd);
    if (s < 0 || s >= UNIX_MAX || !g_end[s].used) return NULL;
    return &g_end[s];
}

int host_unix_owns(int fd) { return end_fd(fd) != NULL; }

static int nb_fd(int fd, int flags) {
    uend *e = end_fd(fd);
    if (flags & UNIX_MSG_DONTWAIT) return 1;
    return e && e->nb;
}

static unsigned wake_gen;
static void wake(void) { wake_gen++; pthread_cond_broadcast(&g_cv); }

static void wait_nb(int nb) {
    if (nb) return;
    if (!g_hk.drop_proc) { pthread_cond_wait(&g_cv, &g_mu); return; }
    /* Solta as duas travas sem segurar uma enquanto pega a outra. O contador
     * cobre o wake que acontece no intervalo (senão o recv dorme para sempre). */
    unsigned seen = wake_gen;
    ux_unlock();
    g_hk.drop_proc();
    ux_lock();
    while (wake_gen == seen) pthread_cond_wait(&g_cv, &g_mu);
    ux_unlock();
    g_hk.take_proc();
    ux_lock();
}

static uend *alloc_end(int kind) {
    for (int i = 0; i < UNIX_MAX; i++) if (!g_end[i].used) {
        memset(&g_end[i], 0, sizeof g_end[i]);
        g_end[i].used = 1;
        g_end[i].refs = 1;
        g_end[i].kind = kind;
        return &g_end[i];
    }
    return NULL;
}

static void free_msg(umsg *m) {
    if (!m) return;
    free(m->data);
    free(m);
}

static void drop_q(uend *e) {
    while (e->head) {
        umsg *m = e->head;
        e->head = m->next;
        free_msg(m);
    }
    e->tail = NULL;
    e->rx_bytes = e->rx_count = 0;
}

static void put_end(uend *e) {
    if (!e || !e->used) return;
    if (--e->refs > 0) return;
    if (e->name[0] && g_hk.unlink_node) g_hk.unlink_node(e->name);
    if (e->peer && e->peer->peer == e) {
        e->peer->peer = NULL;
        e->peer->shut_rd = 1;
    }
    for (int i = 0; i < UNIX_MAX; i++) {
        uend *l = &g_end[i];
        if (!l->used || !l->listening) continue;
        for (int q = 0; q < l->qcount; q++) if (l->q[q] == e) {
            memmove(&l->q[q], &l->q[q + 1], (size_t)(l->qcount - q - 1) * sizeof l->q[0]);
            l->qcount--;
            break;
        }
    }
    drop_q(e);
    e->used = 0;
    wake();
}

void host_unix_close(int fd) {
    ux_lock();
    uend *e = end_fd(fd);
    if (e) put_end(e);
    ux_unlock();
}

void host_unix_hold(int fd) {
    ux_lock();
    uend *e = end_fd(fd);
    if (e) e->refs++;
    ux_unlock();
}

void host_unix_set_nb(int fd, int nb) {
    uend *e = end_fd(fd);
    if (e) e->nb = nb ? 1 : 0;
}

int host_unix_nb(int fd) {
    uend *e = end_fd(fd);
    return e ? e->nb : 0;
}

static int read_path(unsigned long long addr, unsigned long long len, char out[UNIX_PATH]) {
    memset(out, 0, UNIX_PATH);
    if (!addr) return -MOS_EINVAL;
    const unsigned char *p = (const unsigned char *)addr;
    if (len < 2 || p[1] != 1) return -MOS_EINVAL;
    size_t n = len > 2 ? (size_t)len - 2 : 0;
    if (n > UNIX_PATH) n = UNIX_PATH;
    memcpy(out, p + 2, n);
    out[UNIX_PATH - 1] = 0;
    return 0;
}

static int write_sun(unsigned long long addr, unsigned long long lenp, const char *path) {
    if (!addr || !lenp) return 0;
    unsigned int *lp = (unsigned int *)lenp;
    unsigned int cap = *lp;
    unsigned int need = 2 + (unsigned)strlen(path) + 1;
    if (need > UNIX_PATH + 2) need = UNIX_PATH + 2;
    if (cap < 2) return -MOS_EINVAL;
    unsigned char *d = (unsigned char *)addr;
    unsigned int wr = need < cap ? need : cap;
    memset(d, 0, wr);
    d[0] = (unsigned char)wr;
    d[1] = 1;
    if (wr > 2) memcpy(d + 2, path, wr - 2);
    *lp = need;
    return 0;
}

static uend *by_name(const char *path) {
    if (!path || !path[0]) return NULL;
    for (int i = 0; i < UNIX_MAX; i++)
        if (g_end[i].used && g_end[i].name[0] && !strcmp(g_end[i].name, path)) return &g_end[i];
    return NULL;
}

static int enqueue(uend *e, umsg *m) {
    int cap = e->kind == 2 ? UNIX_DGRAMS : UNIX_RX;
    int used = e->kind == 2 ? e->rx_count : e->rx_bytes;
    int add = e->kind == 2 ? 1 : m->len;
    if (used + add > cap) return -MOS_EAGAIN;
    m->next = NULL;
    if (e->tail) e->tail->next = m; else e->head = m;
    e->tail = m;
    e->rx_bytes += m->len;
    e->rx_count++;
    wake();
    return 0;
}

static long copy_in(const void *user, unsigned long long len, unsigned char **out, int *olen) {
    unsigned char *b = NULL;
    if (len) {
        b = malloc((size_t)len);
        if (!b) return -MOS_ENOMEM;
        memcpy(b, user, (size_t)len);
    }
    *out = b;
    *olen = (int)len;
    return (long)len;
}

static long iov_gather(unsigned long long iov, int niov, unsigned char **out, int *olen) {
    if (niov < 0 || niov > UNIX_IOV) return -MOS_EMSGSIZE;
    typedef struct { unsigned long long b, n; } iv;
    unsigned long long total = 0;
    const iv *v = (const iv *)iov;
    for (int i = 0; i < niov; i++) total += v[i].n;
    if (total > UNIX_DGRAM && niov) {
        /* stream aceita até UNIX_RX; o teto de um envio é o buffer */
        if (total > UNIX_RX) return -MOS_EMSGSIZE;
    }
    unsigned char *b = total ? malloc((size_t)total) : NULL;
    if (total && !b) return -MOS_ENOMEM;
    unsigned long long o = 0;
    for (int i = 0; i < niov; i++) {
        if (v[i].n) memcpy(b + o, (void *)v[i].b, (size_t)v[i].n);
        o += v[i].n;
    }
    *out = b;
    *olen = (int)total;
    return (long)total;
}

static long iov_scatter(unsigned long long iov, int niov, const unsigned char *data, int len, int *took) {
    typedef struct { unsigned long long b, n; } iv;
    const iv *v = (const iv *)iov;
    int left = len, off = 0;
    if (niov < 0 || niov > UNIX_IOV) return -MOS_EINVAL;
    for (int i = 0; i < niov && left; i++) {
        int n = left < (int)v[i].n ? left : (int)v[i].n;
        if (n) memcpy((void *)v[i].b, data + off, (size_t)n);
        off += n;
        left -= n;
    }
    *took = off;
    return off;
}

typedef struct {
    unsigned long long name; unsigned int namelen, pad;
    unsigned long long iov; int iovlen, pad2;
    unsigned long long control; unsigned int controllen; int flags;
} kmsg;

static int take_rights(unsigned long long control, unsigned int clen, int *rights, int *nr) {
    *nr = 0;
    if (!control || clen < 16) return 0;
    const unsigned char *p = (const unsigned char *)control;
    unsigned int off = 0;
    while (off + 12 <= clen && *nr < UNIX_RIGHTS) {
        unsigned int cmsg_len, cmsg_level;
        int cmsg_type;
        memcpy(&cmsg_len, p + off, 4);
        memcpy(&cmsg_level, p + off + 4, 4);
        memcpy(&cmsg_type, p + off + 8, 4);
        if (cmsg_len < 12 || off + cmsg_len > clen) break;
        if (cmsg_level == 0xffff && cmsg_type == 0x01) {
            unsigned int nbytes = cmsg_len - 12;
            unsigned int nf = nbytes / 4;
            for (unsigned int i = 0; i < nf && *nr < UNIX_RIGHTS; i++) {
                int fd;
                memcpy(&fd, p + off + 12 + i * 4, 4);
                if (!g_hk.dup_fd) return -MOS_EINVAL;
                int nfd = g_hk.dup_fd(fd);
                if (nfd < 0) return nfd;
                rights[(*nr)++] = nfd;
            }
        }
        unsigned int step = (cmsg_len + 3u) & ~3u;
        if (!step) break;
        off += step;
    }
    return 0;
}

static long deliver_rights(umsg *m, unsigned long long control, unsigned int *clen, int *outflags) {
    if (!m->nrights) { if (clen) *clen = 0; return 0; }
    unsigned int need = 12u + (unsigned)m->nrights * 4u;
    if (!control || !clen || *clen < need) {
        *outflags |= UNIX_MSG_CTRUNC;
        if (clen) *clen = 0;
        return 0;
    }
    unsigned char *p = (unsigned char *)control;
    memset(p, 0, need);
    memcpy(p, &need, 4);
    unsigned int level = 0xffff; int type = 0x01;
    memcpy(p + 4, &level, 4);
    memcpy(p + 8, &type, 4);
    for (int i = 0; i < m->nrights; i++) {
        int nfd = g_hk.dup_fd ? g_hk.dup_fd(m->rights[i]) : -MOS_EBADF;
        if (nfd < 0) return nfd;
        memcpy(p + 12 + i * 4, &nfd, 4);
    }
    *clen = need;
    return 0;
}

static long do_send(int fd, unsigned long long msgp, int raw, const void *rawbuf, unsigned long long rawlen, int xflags) {
    uend *e = end_fd(fd);
    if (!e) return -MOS_EBADF;
    if (e->shut_wr) return -MOS_EPIPE;
    kmsg h; memset(&h, 0, sizeof h);
    unsigned char *data = NULL; int dlen = 0; int rights[UNIX_RIGHTS]; int nr = 0;
    char toname[UNIX_PATH]; memset(toname, 0, sizeof toname);
    if (raw) {
        if (rawlen > (e->kind == 2 ? UNIX_DGRAM : UNIX_RX)) return -MOS_EMSGSIZE;
        long c = copy_in(rawbuf, rawlen, &data, &dlen);
        if (c < 0) return c;
    } else {
        if (!msgp) return -MOS_EFAULT;
        memcpy(&h, (void *)msgp, sizeof h);
        long c = iov_gather(h.iov, h.iovlen, &data, &dlen);
        if (c < 0) return c;
        int tr = take_rights(h.control, h.controllen, rights, &nr);
        if (tr < 0) { free(data); return tr; }
        if (h.name) {
            int rp = read_path(h.name, h.namelen, toname);
            if (rp < 0) { free(data); return rp; }
        }
    }
    if (e->kind == 2 && dlen > UNIX_DGRAM) { free(data); return -MOS_EMSGSIZE; }
    uend *dst = e->peer;
    if (e->kind == 2) {
        const char *nm = toname[0] ? toname : e->dest;
        if (!e->peer) dst = by_name(nm);
        if (!dst) { free(data); return toname[0] || e->dest[0] ? -MOS_ENOENT : -MOS_EDESTADDRREQ; }
    } else if (!dst) { free(data); return -MOS_ENOTCONN; }
    int nb = nb_fd(fd, h.flags | xflags);
    for (;;) {
        if (e->kind == 1 && e->rx_bytes + dlen > UNIX_RX && dst) {
            /* o teto é do destino */
        }
        umsg *m = calloc(1, sizeof *m);
        if (!m) { free(data); return -MOS_ENOMEM; }
        m->data = data; m->len = dlen; m->nrights = nr;
        memcpy(m->rights, rights, sizeof rights);
        int er = enqueue(dst, m);
        if (er == 0) return dlen;
        free(m);
        if (nb) return -MOS_EAGAIN;
        wait_nb(0);
        if (!end_fd(fd) || e->shut_wr) { free(data); return -MOS_EPIPE; }
        dst = e->kind == 2 && !e->peer ? by_name(toname[0] ? toname : e->dest) : e->peer;
        if (!dst) { free(data); return -MOS_EPIPE; }
    }
}

static long do_recv(int fd, unsigned long long msgp, int raw, void *rawbuf, unsigned long long rawlen, int xflags) {
    uend *e = end_fd(fd);
    if (!e) return -MOS_EBADF;
    kmsg h; memset(&h, 0, sizeof h);
    if (!raw) {
        if (!msgp) return -MOS_EFAULT;
        memcpy(&h, (void *)msgp, sizeof h);
    }
    int nb = nb_fd(fd, h.flags | xflags);
    int peek = !raw && ((h.flags | xflags) & UNIX_MSG_PEEK);
    for (;;) {
        if (!e->head) {
            if (e->shut_rd || (e->kind == 1 && e->peer == NULL && e->name[0] == 0 && !e->listening)) return 0;
            if (e->kind == 1 && !e->peer && !e->listening) return 0;
            if (nb) return -MOS_EAGAIN;
            wait_nb(0);
            if (!end_fd(fd)) return -MOS_EBADF;
            continue;
        }
        umsg *m = e->head;
        int avail = m->len - m->off;
        int took = 0;
        long got;
        if (raw) {
            int n = avail < (int)rawlen ? avail : (int)rawlen;
            if (n) memcpy(rawbuf, m->data + m->off, (size_t)n);
            took = n; got = n;
        } else {
            got = iov_scatter(h.iov, h.iovlen, m->data + m->off, avail, &took);
            if (got < 0) return got;
        }
        int outflags = 0;
        if (e->kind == 2 && took < avail) outflags |= UNIX_MSG_TRUNC;
        unsigned int clen = h.controllen;
        if (!peek) {
            if (!raw) {
                long cr = deliver_rights(m, h.control, &clen, &outflags);
                if (cr < 0) return cr;
            }
            if (e->kind == 2 || m->off + took >= m->len) {
                e->head = m->next;
                if (!e->head) e->tail = NULL;
                e->rx_bytes -= avail;
                e->rx_count--;
                free_msg(m);
            } else {
                m->off += took;
                e->rx_bytes -= took;
            }
        } else clen = 0;
        if (!raw && msgp) {
            memcpy((unsigned char *)msgp + 40, &clen, 4);
            memcpy((unsigned char *)msgp + 44, &outflags, 4);
            if (h.name && e->kind == 2) write_sun(h.name, (unsigned long long)((unsigned char *)msgp + 8), "");
        }
        return got;
    }
}

long host_unix_read(int fd, void *buf, unsigned long long len) {
    ux_lock();
    long r = do_recv(fd, 0, 1, buf, len, 0);
    ux_unlock();
    return r;
}

long host_unix_write(int fd, const void *buf, unsigned long long len) {
    ux_lock();
    long r = do_send(fd, 0, 1, buf, len, 0);
    ux_unlock();
    return r;
}

void host_unix_fstat(int fd, unsigned long long *size, int *is_dir, long long *mtime) {
    uend *e = end_fd(fd);
    if (!e) return;
    *is_dir = 6;
    *size = e->listening ? (unsigned long long)e->qcount : (unsigned long long)e->rx_bytes;
    *mtime = (!e->head && (e->shut_rd || (e->kind == 1 && !e->peer && !e->listening))) ? 1 : 0;
}

static int sock_type(unsigned long long t) {
    unsigned low = (unsigned)t & 0xff;
    if (low != 1 && low != 2) return -MOS_EPROTOTYPE;
    return (int)low;
}

static int sock_nb(unsigned long long t) {
    return (t & (0x800ull | 0x10000000ull | 0x40000000ull)) ? 1 : 0;
}

static int sock_clo(unsigned long long t) {
    return (t & (0x80000ull | 0x20000000ull | 0x80000000ull)) ? 1 : 0;
}

static long op_socket(unsigned long long dom, unsigned long long type, unsigned long long proto) {
    if (dom != 1) return -MOS_EAFNOSUPPORT;
    if (proto != 0) return -MOS_EPROTONOSUPPORT;
    int k = sock_type(type);
    if (k < 0) return k;
    uend *e = alloc_end(k);
    if (!e) return -MOS_ENOMEM;
    e->nb = sock_nb(type);
    if (!g_hk.alloc_fd) { e->used = 0; return -MOS_EINVAL; }
    int fd = g_hk.alloc_fd(slot_of_end(e), e->nb, sock_clo(type));
    if (fd < 0) { e->used = 0; return fd; }
    return fd;
}

static long op_pair(unsigned long long type, unsigned long long proto, unsigned long long out) {
    if (proto != 0) return -MOS_EPROTONOSUPPORT;
    int k = sock_type(type);
    if (k < 0) return k;
    uend *a = alloc_end(k), *b = alloc_end(k);
    if (!a || !b) return -MOS_ENOMEM;
    a->peer = b; b->peer = a;
    a->nb = b->nb = sock_nb(type);
    int clo = sock_clo(type);
    int fa = g_hk.alloc_fd(slot_of_end(a), a->nb, clo);
    int fb = g_hk.alloc_fd(slot_of_end(b), b->nb, clo);
    if (fa < 0 || fb < 0) return fa < 0 ? fa : fb;
    int *p = (int *)out;
    p[0] = fa; p[1] = fb;
    return 0;
}

static long op_bind(int fd, unsigned long long addr, unsigned long long len) {
    uend *e = end_fd(fd);
    if (!e) return -MOS_EBADF;
    if (e->name[0]) return -MOS_EINVAL;
    char path[UNIX_PATH];
    int r = read_path(addr, len, path);
    if (r < 0) return r;
    if (!path[0]) return -MOS_EINVAL;
    if (by_name(path)) return -MOS_EADDRINUSE;
    if (!g_hk.parent_ok || !g_hk.parent_ok(path)) return -MOS_ENOENT;
    if (g_hk.create_node) {
        int c = g_hk.create_node(path);
        if (c < 0) return c;
    }
    memcpy(e->name, path, UNIX_PATH);
    return 0;
}

static long op_connect(int fd, unsigned long long addr, unsigned long long len) {
    uend *e = end_fd(fd);
    if (!e) return -MOS_EBADF;
    char path[UNIX_PATH];
    int r = read_path(addr, len, path);
    if (r < 0) return r;
    uend *dst = by_name(path);
    if (!dst) return -MOS_ENOENT;
    if (e->kind == 2) {
        memcpy(e->dest, path, UNIX_PATH);
        return 0;
    }
    if (!dst->listening) return -MOS_ECONNREFUSED;
    for (;;) {
        if (dst->qcount < dst->backlog) break;
        if (e->nb) return -MOS_EAGAIN;
        wait_nb(0);
        if (!end_fd(fd)) return -MOS_EBADF;
        dst = by_name(path);
        if (!dst || !dst->listening) return -MOS_ECONNREFUSED;
    }
    uend *srv = alloc_end(1);
    if (!srv) return -MOS_ENOMEM;
    srv->peer = e; e->peer = srv;
    dst->q[dst->qcount++] = srv;
    wake();
    return 0;
}

static long op_listen(int fd, int backlog) {
    uend *e = end_fd(fd);
    if (!e) return -MOS_EBADF;
    if (e->kind != 1) return -MOS_EOPNOTSUPP;
    if (!e->name[0]) return -MOS_EINVAL;
    if (backlog < 1) backlog = 1;
    if (backlog > UNIX_BACKLOG) backlog = UNIX_BACKLOG;
    e->listening = 1;
    e->backlog = backlog;
    return 0;
}

static long op_accept(int fd, unsigned long long addr, unsigned long long lenp) {
    uend *e = end_fd(fd);
    if (!e) return -MOS_EBADF;
    if (e->kind != 1 || !e->listening) return -MOS_EINVAL;
    for (;;) {
        if (e->qcount) break;
        if (e->nb) return -MOS_EAGAIN;
        wait_nb(0);
        if (!end_fd(fd)) return -MOS_EBADF;
    }
    uend *srv = e->q[0];
    memmove(&e->q[0], &e->q[1], (size_t)(e->qcount - 1) * sizeof e->q[0]);
    e->qcount--;
    int nfd = g_hk.alloc_fd(slot_of_end(srv), e->nb, 0);
    if (nfd < 0) return nfd;
    if (addr && lenp) {
        const char *pn = srv->peer && srv->peer->name[0] ? srv->peer->name : "";
        write_sun(addr, lenp, pn);
    }
    return nfd;
}

static long op_shutdown(int fd, int how) {
    uend *e = end_fd(fd);
    if (!e) return -MOS_EBADF;
    if (how == 0 || how == 2) e->shut_rd = 1;
    if (how == 1 || how == 2) e->shut_wr = 1;
    wake();
    return 0;
}

static long op_name(int fd, int peer, unsigned long long addr, unsigned long long lenp) {
    uend *e = end_fd(fd);
    if (!e) return -MOS_EBADF;
    const char *p = peer ? (e->peer && e->peer->name[0] ? e->peer->name : e->dest) : e->name;
    if (peer && e->kind == 1 && !e->peer) return -MOS_ENOTCONN;
    return write_sun(addr, lenp, p ? p : "");
}

static long op_getsockopt(int fd, unsigned long long sp) {
    uend *e = end_fd(fd);
    if (!e || !sp) return e ? -MOS_EFAULT : -MOS_EBADF;
    typedef struct { int level, name; unsigned len; void *val; } gs;
    gs g;
    memcpy(&g, (void *)sp, sizeof g);
    if (!g.val || g.len < 4) return -MOS_EINVAL;
    int v = 0; unsigned n = 4;
    if (g.level == 0) {
        if (g.name == UNIX_LOCAL_PEERPID || g.name == UNIX_LOCAL_PEEREPID) v = 42;
        else if (g.name == UNIX_LOCAL_PEERCRED) {
            if (g.len < 76) return -MOS_EINVAL;
            unsigned char cred[76];
            memset(cred, 0, sizeof cred);
            unsigned int uid = 501, gid = 20;
            memcpy(cred, &uid, 4);
            memcpy(cred + 4, &gid, 4);
            memcpy(g.val, cred, 76);
            g.len = 76;
            memcpy((void *)sp, &g, sizeof g);
            return 0;
        } else return -MOS_ENOPROTOOPT;
    } else if (g.level == UNIX_SOL_SOCKET) {
        if (g.name == UNIX_SO_TYPE) v = e->kind;
        else if (g.name == UNIX_SO_ACCEPTCONN) v = e->listening;
        else if (g.name == UNIX_SO_SNDBUF) v = e->kind == 2 ? UNIX_DGRAM : UNIX_RX;
        else if (g.name == UNIX_SO_RCVBUF) v = e->kind == 2 ? UNIX_DGRAM : UNIX_RX;
        else if (g.name == UNIX_SO_ERROR) v = 0;
        else return -MOS_ENOPROTOOPT;
    } else return -MOS_ENOPROTOOPT;
    memcpy(g.val, &v, 4);
    g.len = n;
    memcpy((void *)sp, &g, sizeof g);
    return 0;
}

long host_unix_op(unsigned long long op, unsigned long long a, unsigned long long b, unsigned long long c) {
    ux_lock();
    long r = -MOS_EINVAL;
    switch ((unsigned)op) {
    case MOS_UNIX_SOCKET: r = op_socket(a, b, c); break;
    case MOS_UNIX_PAIR: r = op_pair(a, b, c); break;
    case MOS_UNIX_BIND: r = op_bind((int)a, b, c); break;
    case MOS_UNIX_CONNECT: r = op_connect((int)a, b, c); break;
    case MOS_UNIX_LISTEN: r = op_listen((int)a, (int)b); break;
    case MOS_UNIX_ACCEPT: r = op_accept((int)a, b, c); break;
    case MOS_UNIX_SENDMSG: r = do_send((int)a, b, 0, NULL, 0, (int)c); break;
    case MOS_UNIX_RECVMSG: r = do_recv((int)a, b, 0, NULL, 0, (int)c); break;
    case MOS_UNIX_SHUTDOWN: r = op_shutdown((int)a, (int)b); break;
    case MOS_UNIX_SOCKNAME: r = op_name((int)a, 0, b, c); break;
    case MOS_UNIX_PEERNAME: r = op_name((int)a, 1, b, c); break;
    case MOS_UNIX_GETSOCKOPT: r = op_getsockopt((int)a, b); break;
    default: r = -MOS_EINVAL; break;
    }
    ux_unlock();
    return r;
}
