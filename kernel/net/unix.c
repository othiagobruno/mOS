/* mOS — AF_UNIX entre processos (SYS_UNIX, 61).
 *
 * O socket é um descritor do processo (kind FD_UNIX, o índice da tabela fica em fds[].off).
 * Por isso fork, dup, close e exec CLOEXEC passam pelos mesmos ganchos dos pipes
 * (unix_hold / unix_put em process.c). read e write são SYS_READ / SYS_WRITE.
 * O poll vê o fstat: is_dir 6, size = bytes prontos (ou conexões na fila), mtime = 1 se o
 * próximo read for fim de arquivo.
 *
 * Contrato estável (XPC e o transporte AX): docs/af-unix.md.
 */
#include "kernel.h"
#include "proc.h"
#include "syscall.h"
#include "vfs.h"

#define UNIX_MAX     128
#define UNIX_NAMES   64
#define UNIX_PATH    104          /* sockaddr_un.sun_path do Darwin, com o zero final */
#define UNIX_RX      (16 * 1024)  /* teto do buffer de um SOCK_STREAM */
#define UNIX_DGRAM   8192         /* um datagrama; ax_msg cabe (o servidor AX usa 640) */
#define UNIX_DGRAMS  32
#define UNIX_BACKLOG 32
#define UNIX_RIGHTS  16
#define UNIX_CMSG    512
#define UNIX_IOV     8

#define UNIX_SOL_LOCAL  0
#define UNIX_SOL_SOCKET 0xffff
#define UNIX_LOCAL_PEERCRED 0x001
#define UNIX_LOCAL_PEERPID  0x002
#define UNIX_LOCAL_PEEREPID 0x003
#define UNIX_SO_ACCEPTCONN  0x0002
#define UNIX_SO_SNDBUF      0x1001
#define UNIX_SO_RCVBUF      0x1002
#define UNIX_SO_ERROR       0x1007
#define UNIX_SO_TYPE        0x1008

#define UNIX_MSG_PEEK     0x2
#define UNIX_MSG_TRUNC    0x10
#define UNIX_MSG_CTRUNC   0x20
#define UNIX_MSG_DONTWAIT 0x80
#define UNIX_MSG_NOSIGNAL 0x80000

enum { W_NONE = 0, W_RECV = 1, W_ACCEPT = 2, W_SEND = 3, W_CONNECT = 4, W_WRITE = 5, W_READ = 6 };

#ifndef MOS_EDESTADDRREQ
#define MOS_EDESTADDRREQ 39
#endif

typedef struct umsg {
    struct umsg *next;
    int len, off, from_pid, nrights;
    int from_uid, from_gid;
    void *rights[UNIX_RIGHTS];
    char from_path[UNIX_PATH];
    u8 *data;
} umsg_t;

typedef struct uend {
    int used, refs, kind;             /* kind 1 = stream, 2 = dgram */
    int listening, backlog, qcount;
    int shut_rd, shut_wr, linked;
    int creator_pid, uid, gid;
    int dest_pid, last_pid, last_uid, last_gid;
    int rx_bytes, rx_count;
    int rop, wop, rfd, wfd;
    u64 r0, r1, r2, w0, w1, w2;
    char name[UNIX_PATH];
    char dest[UNIX_PATH];
    char wpath[UNIX_PATH];
    struct uend *peer;
    struct uend *queued_on;
    struct uend *q[UNIX_BACKLOG];
    umsg_t *head, *tail;
    thread_t *rwait, *wwait;
} uend_t;

static uend_t g_end[UNIX_MAX];
static struct { int used, slot; char path[UNIX_PATH]; } g_name[UNIX_NAMES];

void *proc_fd_snapshot(process_t *p, int fd);
int proc_fd_from_snapshot(process_t *p, const void *snap);
void proc_fd_snapshot_free(void *snap);

static int slot_of(uend_t *e) { return (int)(e - g_end); }

static uend_t *end_of(process_t *p, int fd) {
    if (!p || fd < 0 || fd >= PROC_MAX_FDS || !p->fds[fd].used || p->fds[fd].kind != FD_UNIX) return NULL;
    int s = (int)p->fds[fd].off;
    if (s < 0 || s >= UNIX_MAX || !g_end[s].used) return NULL;
    return &g_end[s];
}

static int nonblock(process_t *p, int fd, int flags) {
    if (flags & UNIX_MSG_DONTWAIT) return 1;
    return (p->fds[fd].flags & MOS_O_NONBLOCK) != 0;
}

static void ready(thread_t *t, i64 res) {
    if (!t || !t->proc || t->proc->exited || t->state == TH_DEAD) return;
    if (t->frame) ((int_frame_t *)t->frame)->rax = (u64)res;
    t->wait_pid = 0;
    t->state = TH_READY;
}

void unix_clear_waiter(thread_t *t) {
    if (!t) return;
    for (int i = 0; i < UNIX_MAX; i++) {
        if (g_end[i].rwait == t) { g_end[i].rwait = NULL; g_end[i].rop = W_NONE; }
        if (g_end[i].wwait == t) { g_end[i].wwait = NULL; g_end[i].wop = W_NONE; }
    }
}

static void drop_rights(umsg_t *m) {
    for (int i = 0; i < m->nrights; i++) proc_fd_snapshot_free(m->rights[i]);
    m->nrights = 0;
}

static void free_msg(umsg_t *m) {
    if (!m) return;
    drop_rights(m);
    kfree(m->data);
    kfree(m);
}

static void free_rx(uend_t *e) {
    while (e->head) { umsg_t *m = e->head; e->head = m->next; free_msg(m); }
    e->tail = NULL;
    e->rx_bytes = 0;
    e->rx_count = 0;
}

static int name_find(const char *path) {
    for (int i = 0; i < UNIX_NAMES; i++)
        if (g_name[i].used && !strcmp(g_name[i].path, path)) return i;
    return -1;
}

int unix_path_is_sock(const char *path) {
    return path && name_find(path) >= 0;
}

static void fail_connectors(const char *path, i64 err) {
    if (!path || !path[0]) return;
    for (int i = 0; i < UNIX_MAX; i++) {
        uend_t *e = &g_end[i];
        if (!e->used || e->wop != W_CONNECT || !e->wwait || strcmp(e->wpath, path)) continue;
        thread_t *t = e->wwait;
        e->wwait = NULL;
        e->wop = W_NONE;
        ready(t, err);
    }
}

void unix_unbind(const char *path) {
    int i = name_find(path);
    if (i < 0) return;
    int slot = g_name[i].slot;
    g_name[i].used = 0;
    if (slot >= 0 && slot < UNIX_MAX && g_end[slot].used) g_end[slot].name[0] = 0;
    fail_connectors(path, -MOS_ECONNREFUSED);
}

void unix_rename(const char *from, const char *to) {
    int i = name_find(from);
    if (i < 0 || !to || strlen(to) >= UNIX_PATH) return;
    int j = name_find(to);
    if (j >= 0 && j != i) g_name[j].used = 0;
    memcpy(g_name[i].path, to, strlen(to) + 1);
    int slot = g_name[i].slot;
    if (slot >= 0 && slot < UNIX_MAX && g_end[slot].used)
        memcpy(g_end[slot].name, to, strlen(to) + 1);
}

static int canon(process_t *p, const char *in, char *out) {
    if (!in || !in[0]) return -MOS_EINVAL;
    char tmp[UNIX_PATH];
    if (in[0] == '/') {
        if (strlen(in) >= UNIX_PATH) return -MOS_ENAMETOOLONG;
        memcpy(tmp, in, strlen(in) + 1);
    } else {
        usize cl = strlen(p->cwd), il = strlen(in);
        int slash = !(cl > 0 && p->cwd[cl - 1] == '/');
        if (cl + slash + il >= UNIX_PATH) return -MOS_ENAMETOOLONG;
        memcpy(tmp, p->cwd, cl);
        if (slash) tmp[cl++] = '/';
        memcpy(tmp + cl, in, il + 1);
    }
    memcpy(out, tmp, strlen(tmp) + 1);
    return 0;
}

static int read_sun(process_t *p, u64 addr, u64 len, char *path) {
    u8 buf[2 + UNIX_PATH];
    if (!addr || len < 2) return -MOS_EINVAL;
    u64 n = MIN(len, (u64)sizeof buf);
    memset(buf, 0, sizeof buf);
    if (!uvm_copy_in(p, buf, addr, n)) return -MOS_EFAULT;
    if (buf[1] != 1) return -MOS_EAFNOSUPPORT;
    int max = (int)n - 2;
    if (max > UNIX_PATH - 1) max = UNIX_PATH - 1;
    int i = 0;
    for (; i < max && buf[2 + i]; i++) path[i] = (char)buf[2 + i];
    path[i] = 0;
    if (!i) return -MOS_EINVAL;
    return canon(p, path, path);
}

static int write_sun(process_t *p, u64 addr, u64 lenp, const char *path) {
    u32 have = 0;
    if (!lenp || !uvm_copy_in(p, &have, lenp, 4)) return lenp ? -MOS_EFAULT : 0;
    int pl = path ? (int)strlen(path) : 0;
    if (pl > UNIX_PATH - 1) pl = UNIX_PATH - 1;
    u8 tmp[2 + UNIX_PATH];
    memset(tmp, 0, sizeof tmp);
    tmp[0] = (u8)(pl + 2);
    tmp[1] = 1;
    if (pl) memcpy(tmp + 2, path, (usize)pl);
    u32 need = (u32)(pl + 2);
    u32 copy = have < need + 1 ? have : need + 1;
    if (addr && copy && !uvm_copy_out(p, addr, tmp, copy)) return -MOS_EFAULT;
    if (!uvm_copy_out(p, lenp, &need, 4)) return -MOS_EFAULT;
    return 0;
}

static int parent_ok(const char *path) {
    char buf[UNIX_PATH];
    usize n = strlen(path);
    if (n >= sizeof buf) return -MOS_ENAMETOOLONG;
    memcpy(buf, path, n + 1);
    char *sl = strrchr(buf, '/');
    if (!sl || sl[1] == 0) return -MOS_EINVAL;
    if (sl == buf) return vfs_lookup("/") ? 0 : -MOS_ENOENT;
    *sl = 0;
    vnode_t *d = vfs_lookup(buf);
    if (!d) return -MOS_ENOENT;
    if (!d->dir) return -MOS_ENOTDIR;
    return 0;
}

static uend_t *alloc_end(int kind, process_t *p) {
    for (int i = 0; i < UNIX_MAX; i++) {
        if (g_end[i].used) continue;
        memset(&g_end[i], 0, sizeof g_end[i]);
        g_end[i].used = 1;
        g_end[i].refs = 1;
        g_end[i].kind = kind;
        g_end[i].creator_pid = p ? p->pid : 0;
        g_end[i].uid = 501;
        g_end[i].gid = 20;
        return &g_end[i];
    }
    return NULL;
}

static int install(process_t *p, uend_t *e, int flags) {
    for (int fd = 3; fd < PROC_MAX_FDS; fd++) {
        if (p->fds[fd].used) continue;
        memset(&p->fds[fd], 0, sizeof p->fds[fd]);
        p->fds[fd].used = true;
        p->fds[fd].kind = FD_UNIX;
        p->fds[fd].off = (u64)slot_of(e);
        p->fds[fd].flags = MOS_O_RDWR | flags;
        return fd;
    }
    return -MOS_EMFILE;
}

static int type_flags(u64 type, int *kind) {
    u32 t = (u32)type;
    int k = (int)(t & 0xF);
    if (k != 1 && k != 2) return -MOS_EPROTONOSUPPORT;
    *kind = k;
    int fl = 0;
    if (t & (0x800u | 0x10000000u | 0x40000000u)) fl |= MOS_O_NONBLOCK;
    if (t & (0x80000u | 0x20000000u | 0x80000000u)) fl |= MOS_O_CLOEXEC;
    return fl;
}

void unix_hold(int slot) {
    if (slot < 0 || slot >= UNIX_MAX || !g_end[slot].used) return;
    g_end[slot].refs++;
}

static void destroy(uend_t *e);

void unix_put(int slot) {
    if (slot < 0 || slot >= UNIX_MAX || !g_end[slot].used) return;
    if (--g_end[slot].refs > 0) return;
    g_end[slot].used = 0;
    destroy(&g_end[slot]);
}

static void dequeue(uend_t *ln, uend_t *srv) {
    for (int i = 0; i < ln->qcount; i++) {
        if (ln->q[i] != srv) continue;
        memmove(&ln->q[i], &ln->q[i + 1], (usize)(ln->qcount - i - 1) * sizeof(ln->q[0]));
        ln->qcount--;
        srv->queued_on = NULL;
        return;
    }
}

static i64 try_recv(process_t *p, int fd, uend_t *e, u64 msg, int flags);
static i64 try_send(process_t *p, int fd, uend_t *e, u64 msg, int flags);
static i64 try_accept(process_t *p, int fd, uend_t *e, u64 addr, u64 lenp);
static i64 try_connect(process_t *p, uend_t *e, const char *path);
static i64 try_raw_read(process_t *p, uend_t *e, u64 buf, u64 len);
static i64 try_raw_write(process_t *p, uend_t *e, u64 buf, u64 len);
static int fd_open(process_t *p, int fd) {
    return p && fd >= 0 && fd < PROC_MAX_FDS && p->fds[fd].used;
}
static i64 nosock(process_t *p, int fd) { return fd_open(p, fd) ? -MOS_ENOTSOCK : -MOS_EBADF; }

static void pump_r(uend_t *e) {
    if (!e->used || !e->rwait) return;
    thread_t *t = e->rwait;
    int op = e->rop, fd = e->rfd;
    u64 a = e->r0, b = e->r1;
    e->rwait = NULL;
    e->rop = W_NONE;
    process_t *p = t->proc;
    i64 r = -MOS_ECONNRESET;
    if (p && !p->exited) {
        if (op == W_ACCEPT) r = try_accept(p, fd, e, a, b);
        else if (op == W_READ) r = try_raw_read(p, e, a, b);
        else r = try_recv(p, fd, e, a, (int)b);
    }
    if (r == -MOS_EAGAIN) {
        if (!e->rwait) { e->rwait = t; e->rop = op; e->rfd = fd; e->r0 = a; e->r1 = b; }
        return;
    }
    ready(t, r);
}

static void pump_w(uend_t *e) {
    if (!e->used || !e->wwait || (e->wop != W_SEND && e->wop != W_WRITE)) return;
    thread_t *t = e->wwait;
    int op = e->wop, fd = e->wfd;
    u64 a = e->w0, b = e->w1, c = e->w2;
    e->wwait = NULL;
    e->wop = W_NONE;
    process_t *p = t->proc;
    i64 r = -MOS_EPIPE;
    if (p && !p->exited) {
        if (op == W_WRITE) r = try_raw_write(p, e, a, b);
        else r = try_send(p, fd, e, a, (int)b);
    }
    if (r == -MOS_EAGAIN) {
        if (!e->wwait) { e->wwait = t; e->wop = op; e->wfd = fd; e->w0 = a; e->w1 = b; e->w2 = c; }
        return;
    }
    ready(t, r);
    if (r == -MOS_EPIPE && !(c & UNIX_MSG_NOSIGNAL) && t->proc && !t->proc->exited)
        proc_signal(t->proc->pid, MOS_SIGPIPE);
}

static void wake_connector(const char *path) {
    if (!path || !path[0]) return;
    for (int i = 0; i < UNIX_MAX; i++) {
        uend_t *e = &g_end[i];
        if (!e->used || e->wop != W_CONNECT || !e->wwait || strcmp(e->wpath, path)) continue;
        thread_t *t = e->wwait;
        char path[UNIX_PATH];
        memcpy(path, e->wpath, UNIX_PATH);
        e->wwait = NULL;
        e->wop = W_NONE;
        process_t *p = t->proc;
        i64 r = p && !p->exited ? try_connect(p, e, path) : -MOS_ECONNREFUSED;
        if (r == -MOS_EAGAIN) {
            if (!e->wwait) { e->wwait = t; e->wop = W_CONNECT; memcpy(e->wpath, path, UNIX_PATH); }
            return;
        }
        ready(t, r);
        return;
    }
}

static void destroy(uend_t *e) {
    while (e->qcount) {
        uend_t *s = e->q[--e->qcount];
        s->queued_on = NULL;
        unix_put(slot_of(s));
    }
    uend_t *peer = e->peer;
    e->peer = NULL;
    if (peer && peer->peer == e) {
        peer->peer = NULL;
        if (peer->queued_on && peer->refs == 1) {
            uend_t *ln = peer->queued_on;
            dequeue(ln, peer);
            unix_put(slot_of(peer));
            peer = NULL;
        } else if (peer->used) {
            pump_r(peer);
            pump_w(peer);
        }
    }
    if (e->name[0]) {
        int i = name_find(e->name);
        if (i >= 0 && g_name[i].slot == slot_of(e)) g_name[i].slot = -1;
        fail_connectors(e->name, -MOS_ECONNREFUSED);
    }
    thread_t *rt = e->rwait, *wt = e->wwait;
    e->rwait = e->wwait = NULL;
    e->rop = e->wop = W_NONE;
    ready(rt, -MOS_ECONNRESET);
    ready(wt, -MOS_EPIPE);
    free_rx(e);
    memset(e, 0, sizeof *e);
}

static int enqueue_msg(uend_t *dst, umsg_t *m) {
    m->next = NULL;
    if (dst->tail) dst->tail->next = m;
    else dst->head = m;
    dst->tail = m;
    dst->rx_bytes += m->len - m->off;
    dst->rx_count++;
    pump_r(dst);
    return 0;
}

static i64 deliver_rights(process_t *p, umsg_t *m, u64 control, u32 *controllen, int *flags) {
    if (!m->nrights) { *controllen = 0; return 0; }
    u32 need = 12u + (u32)m->nrights * 4u;
    need = (need + 3u) & ~3u;
    if (!control || *controllen < 16) {
        *flags |= UNIX_MSG_CTRUNC;
        *controllen = 0;
        drop_rights(m);
        return 0;
    }
    int fit = (int)((*controllen - 12) / 4);
    if (fit > m->nrights) fit = m->nrights;
    if (fit < m->nrights) *flags |= UNIX_MSG_CTRUNC;
    u8 raw[UNIX_CMSG];
    u32 clen = 12u + (u32)fit * 4u;
    if (clen > sizeof raw) return -MOS_EINVAL;
    memset(raw, 0, clen);
    memcpy(raw, &clen, 4);
    int level = UNIX_SOL_SOCKET, type = 0x01;
    memcpy(raw + 4, &level, 4);
    memcpy(raw + 8, &type, 4);
    for (int i = 0; i < fit; i++) {
        int nfd = proc_fd_from_snapshot(p, m->rights[i]);
        if (nfd < 0) {
            *flags |= UNIX_MSG_CTRUNC;
            for (int j = i; j < m->nrights; j++) proc_fd_snapshot_free(m->rights[j]);
            m->nrights = 0;
            fit = i;
            clen = 12u + (u32)fit * 4u;
            memcpy(raw, &clen, 4);
            break;
        }
        memcpy(raw + 12 + (u32)i * 4u, &nfd, 4);
        proc_fd_snapshot_free(m->rights[i]);
    }
    m->nrights = 0;
    if (fit == 0) { *controllen = 0; return 0; }
    if (!uvm_copy_out(p, control, raw, clen)) return -MOS_EFAULT;
    *controllen = clen;
    return 0;
}

typedef struct {
    u64 name;
    u32 namelen, pad;
    u64 iov;
    int iovlen, pad2;
    u64 control;
    u32 controllen;
    int flags;
} kmsg_t;

typedef struct { u64 base, len; } kiov_t;
_Static_assert(sizeof(kmsg_t) == 48, "msghdr Darwin LP64");
_Static_assert(sizeof(mos_unix_sockopt_t) == 24, "getsockopt");

static i64 copy_out_iov(process_t *p, kiov_t *iov, int niov, const u8 *src, int n) {
    int left = n;
    for (int i = 0; i < niov && left; i++) {
        int k = left;
        if ((u64)k > iov[i].len) k = (int)iov[i].len;
        if (k && !uvm_copy_out(p, iov[i].base, src, (usize)k)) return -MOS_EFAULT;
        src += k;
        left -= k;
    }
    return n - left;
}


static i64 gather(process_t *p, kiov_t *iov, int niov, u8 *dst, int cap, int *outn) {
    int n = 0;
    for (int i = 0; i < niov; i++) {
        u64 left = iov[i].len;
        u64 va = iov[i].base;
        while (left) {
            if (n >= cap) { *outn = n; return 0; }
            int k = cap - n;
            if ((u64)k > left) k = (int)left;
            if (!uvm_copy_in(p, dst + n, va, (usize)k)) return -MOS_EFAULT;
            n += k;
            va += (u64)k;
            left -= (u64)k;
        }
    }
    *outn = n;
    return 0;
}

static i64 take_rights(process_t *p, u64 control, u32 clen, void **out, int *nout) {
    *nout = 0;
    if (!control || !clen) return 0;
    if (clen > UNIX_CMSG) return -MOS_EINVAL;
    u8 raw[UNIX_CMSG];
    if (!uvm_copy_in(p, raw, control, clen)) return -MOS_EFAULT;
    u32 o = 0;
    while (o + 12 <= clen) {
        u32 L; int level, type;
        memcpy(&L, raw + o, 4);
        memcpy(&level, raw + o + 4, 4);
        memcpy(&type, raw + o + 8, 4);
        if (L < 12 || o + L > clen) { for (int i = 0; i < *nout; i++) proc_fd_snapshot_free(out[i]); return -MOS_EINVAL; }
        if (level == UNIX_SOL_SOCKET && type == 0x01) {
            int nf = (int)((L - 12) / 4);
            for (int i = 0; i < nf; i++) {
                int rfd;
                memcpy(&rfd, raw + o + 12 + (u32)i * 4u, 4);
                if (*nout >= UNIX_RIGHTS) { for (int j = 0; j < *nout; j++) proc_fd_snapshot_free(out[j]); return -MOS_EINVAL; }
                void *s = proc_fd_snapshot(p, rfd);
                if (!s) { for (int j = 0; j < *nout; j++) proc_fd_snapshot_free(out[j]); return -MOS_EBADF; }
                out[(*nout)++] = s;
            }
        }
        u32 step = (L + 3u) & ~3u;
        if (!step) break;
        o += step;
    }
    return 0;
}

static i64 pipe_sig(process_t *p, int flags, i64 err) {
    if (err == -MOS_EPIPE && !(flags & UNIX_MSG_NOSIGNAL) && p && !p->exited)
        proc_signal(p->pid, MOS_SIGPIPE);
    return err;
}

static i64 try_send(process_t *p, int fd, uend_t *e, u64 msg, int flags) {
    (void)fd;
    kmsg_t h;
    if (!msg || !uvm_copy_in(p, &h, msg, sizeof h)) return -MOS_EFAULT;
    int niov = h.iovlen;
    if (niov < 0 || niov > UNIX_IOV) return -MOS_EINVAL;
    kiov_t iov[UNIX_IOV];
    if (niov && !uvm_copy_in(p, iov, h.iov, (usize)niov * sizeof iov[0])) return -MOS_EFAULT;
    u64 total = 0;
    for (int i = 0; i < niov; i++) total += iov[i].len;
    void *rights[UNIX_RIGHTS];
    int nrights = 0;
    i64 tr = take_rights(p, h.control, h.controllen, rights, &nrights);
    if (tr < 0) return tr;

    if (e->kind == 1) {
        if (!e->linked) { for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]); return -MOS_ENOTCONN; }
        if (e->shut_wr || !e->peer || !e->peer->used || e->peer->shut_rd) {
            for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]);
            (void)p; (void)flags;
            return -MOS_EPIPE;
        }
        int freeb = UNIX_RX - e->peer->rx_bytes;
        if (freeb < 0) freeb = 0;
        int n = (int)total;
        if (nrights) {
            if ((u64)n != total || n > freeb) {
                for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]);
                return -MOS_EAGAIN;
            }
        } else if (n > freeb) n = freeb;
        if (n == 0 && !nrights) return 0;
        if (n == 0 && nrights && freeb == 0) {
            for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]);
            return -MOS_EAGAIN;
        }
        if ((u64)n > total) n = (int)total;
        u8 *buf = NULL;
        if (n) {
            buf = kmalloc((usize)n);
            if (!buf) { for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]); return -MOS_ENOMEM; }
            int got = 0;
            /* gather só os primeiros n bytes */
            int left = n;
            for (int i = 0; i < niov && left; i++) {
                int k = left;
                if ((u64)k > iov[i].len) k = (int)iov[i].len;
                if (k && !uvm_copy_in(p, buf + (n - left), iov[i].base, (usize)k)) {
                    kfree(buf);
                    for (int j = 0; j < nrights; j++) proc_fd_snapshot_free(rights[j]);
                    return -MOS_EFAULT;
                }
                left -= k;
            }
            got = n - left;
            n = got;
        }
        umsg_t *m = kmalloc(sizeof *m);
        if (!m) {
            kfree(buf);
            for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]);
            return -MOS_ENOMEM;
        }
        memset(m, 0, sizeof *m);
        m->data = buf;
        m->len = n;
        m->from_pid = e->creator_pid;
        m->nrights = nrights;
        for (int i = 0; i < nrights; i++) m->rights[i] = rights[i];
        enqueue_msg(e->peer, m);
        return n;
    }

    /* datagrama */
    char path[UNIX_PATH];
    path[0] = 0;
    if (h.name && h.namelen) {
        i64 rs = read_sun(p, h.name, h.namelen, path);
        if (rs < 0) { for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]); return rs; }
    } else if (e->linked && e->dest[0]) {
        memcpy(path, e->dest, UNIX_PATH);
    } else {
        for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]);
        return -MOS_EDESTADDRREQ;
    }
    if ((u64)total > UNIX_DGRAM || total > 0x7fffffff) {
        for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]);
        return -MOS_EMSGSIZE;
    }
    int ni = name_find(path);
    if (ni < 0) {
        for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]);
        return vfs_lookup(path) ? -MOS_ECONNREFUSED : -MOS_ENOENT;
    }
    if (g_name[ni].slot < 0) {
        for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]);
        return -MOS_ECONNREFUSED;
    }
    uend_t *dst = &g_end[g_name[ni].slot];
    if (!dst->used || dst->kind != 2) {
        for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]);
        return -MOS_EPROTOTYPE;
    }
    if (dst->rx_count >= UNIX_DGRAMS || dst->shut_rd) {
        for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]);
        return dst->shut_rd ? -MOS_ECONNREFUSED : -MOS_EAGAIN;
    }
    int n = (int)total;
    u8 *buf = NULL;
    if (n) {
        buf = kmalloc((usize)n);
        if (!buf) { for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]); return -MOS_ENOMEM; }
        int got = 0;
        if (gather(p, iov, niov, buf, n, &got) < 0 || got != n) {
            kfree(buf);
            for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]);
            return got != n ? -MOS_EFAULT : -MOS_EFAULT;
        }
    }
    umsg_t *m = kmalloc(sizeof *m);
    if (!m) { kfree(buf); for (int i = 0; i < nrights; i++) proc_fd_snapshot_free(rights[i]); return -MOS_ENOMEM; }
    memset(m, 0, sizeof *m);
    m->data = buf;
    m->len = n;
    m->from_pid = e->creator_pid;
    m->from_uid = e->uid;
    m->from_gid = e->gid;
    if (e->name[0]) memcpy(m->from_path, e->name, UNIX_PATH);
    m->nrights = nrights;
    for (int i = 0; i < nrights; i++) m->rights[i] = rights[i];
    e->dest_pid = dst->creator_pid;
    enqueue_msg(dst, m);
    return n;
}

/* A primeira try_recv libera a mensagem e depois lê m->from_pid. Esta é a versão que vale. */
static i64 try_recv(process_t *p, int fd, uend_t *e, u64 msg, int flags) {
    (void)fd;
    kmsg_t h;
    if (!msg || !uvm_copy_in(p, &h, msg, sizeof h)) return -MOS_EFAULT;
    int niov = h.iovlen;
    if (niov < 0 || niov > UNIX_IOV) return -MOS_EINVAL;
    kiov_t iov[UNIX_IOV];
    if (niov && !uvm_copy_in(p, iov, h.iov, (usize)niov * sizeof iov[0])) return -MOS_EFAULT;
    u64 want = 0;
    for (int i = 0; i < niov; i++) want += iov[i].len;
    if (e->listening && e->kind == 1) return -MOS_EINVAL;
    if (!e->head) {
        if (e->shut_rd) return 0;
        if (e->kind == 1 && !e->linked) return -MOS_ENOTCONN;
        if (e->kind == 1 && (!e->peer || !e->peer->used || e->peer->shut_wr)) return 0;
        return -MOS_EAGAIN;
    }
    umsg_t *m = e->head;
    int from_pid = m->from_pid, from_uid = m->from_uid, from_gid = m->from_gid;
    char from_path[UNIX_PATH];
    memcpy(from_path, m->from_path, UNIX_PATH);
    int avail = m->len - m->off;
    int take = avail;
    if ((u64)take > want) take = (int)want;
    i64 got = take;
    if (take) {
        got = copy_out_iov(p, iov, niov, m->data + m->off, take);
        if (got < 0) return got;
    }
    int outflags = 0;
    if (e->kind == 2 && (int)got < avail) outflags |= UNIX_MSG_TRUNC;
    u32 clen = h.controllen;
    if (!(flags & UNIX_MSG_PEEK)) {
        i64 cr = deliver_rights(p, m, h.control, &clen, &outflags);
        if (cr < 0) return cr;
        if (e->kind == 2) {
            /* o resto do datagrama que não coube é descartado */
            e->head = m->next;
            if (!e->head) e->tail = NULL;
            e->rx_bytes -= avail;
            e->rx_count--;
            free_msg(m);
        } else if (m->off + (int)got >= m->len) {
            e->head = m->next;
            if (!e->head) e->tail = NULL;
            e->rx_bytes -= avail;
            e->rx_count--;
            free_msg(m);
        } else {
            m->off += (int)got;
            e->rx_bytes -= (int)got;
        }
        if (e->peer && e->peer->used) pump_w(e->peer);
    } else clen = 0;
    if (e->kind == 2) {
        e->last_pid = from_pid;
        e->last_uid = from_uid;
        e->last_gid = from_gid;
        if (h.name) {
            i64 wr = write_sun(p, h.name, msg + 8, from_path);
            if (wr < 0) return wr;
        }
    }
    if (msg) {
        uvm_copy_out(p, msg + 40, &clen, 4);
        uvm_copy_out(p, msg + 44, &outflags, 4);
    }
    return got;
}

static i64 try_accept(process_t *p, int fd, uend_t *ln, u64 addr, u64 lenp) {
    if (ln->kind != 1) return -MOS_EOPNOTSUPP;
    if (!ln->listening) return -MOS_EINVAL;
    if (!ln->qcount) return -MOS_EAGAIN;
    uend_t *srv = ln->q[0];
    memmove(&ln->q[0], &ln->q[1], (usize)(ln->qcount - 1) * sizeof(ln->q[0]));
    ln->qcount--;
    srv->queued_on = NULL;
    int fl = 0;
    if (fd >= 0 && fd < PROC_MAX_FDS && (p->fds[fd].flags & MOS_O_NONBLOCK)) fl = MOS_O_NONBLOCK;
    int nfd = install(p, srv, fl);
    if (nfd < 0) {
        /* devolve à fila: a referência continua sendo a da fila */
        memmove(&ln->q[1], &ln->q[0], (usize)ln->qcount * sizeof(ln->q[0]));
        ln->q[0] = srv;
        ln->qcount++;
        srv->queued_on = ln;
        return nfd;
    }
    if (addr && lenp) {
        const char *pn = srv->peer && srv->peer->used ? srv->peer->name : "";
        i64 wr = write_sun(p, addr, lenp, pn);
        if (wr < 0) return nfd; /* o fd já saiu; o endereço é best-effort */
    }
    if (ln->name[0]) wake_connector(ln->name);
    return nfd;
}

static i64 try_connect(process_t *p, uend_t *e, const char *path) {
    (void)p;
    if (e->kind == 2) {
        int ni = name_find(path);
        if (ni < 0) return vfs_lookup(path) ? -MOS_ECONNREFUSED : -MOS_ENOENT;
        if (g_name[ni].slot < 0 || !g_end[g_name[ni].slot].used) return -MOS_ECONNREFUSED;
        if (g_end[g_name[ni].slot].kind != 2) return -MOS_EPROTOTYPE;
        memcpy(e->dest, path, UNIX_PATH);
        e->dest_pid = g_end[g_name[ni].slot].creator_pid;
        e->linked = 1;
        return 0;
    }
    if (e->linked) return -MOS_EISCONN;
    int ni = name_find(path);
    if (ni < 0) return vfs_lookup(path) ? -MOS_ECONNREFUSED : -MOS_ENOENT;
    if (g_name[ni].slot < 0) return -MOS_ECONNREFUSED;
    uend_t *ln = &g_end[g_name[ni].slot];
    if (!ln->used || !ln->listening || ln->kind != 1) return -MOS_ECONNREFUSED;
    if (ln->qcount >= ln->backlog) return -MOS_EAGAIN;
    uend_t *srv = alloc_end(1, NULL);
    if (!srv) return -MOS_ENOMEM;
    srv->creator_pid = ln->creator_pid;
    srv->uid = ln->uid;
    srv->gid = ln->gid;
    srv->linked = 1;
    srv->peer = e;
    e->peer = srv;
    e->linked = 1;
    srv->queued_on = ln;
    ln->q[ln->qcount++] = srv;
    pump_r(ln);
    return 0;
}

static u64 block_on(int_frame_t *f, thread_t *t, uend_t *e, int rd, int op, int fd, u64 a, u64 b, u64 c) {
    if (rd) {
        if (e->rwait) { f->rax = (u64)-MOS_EAGAIN; return (u64)f; }
        e->rwait = t; e->rop = op; e->rfd = fd; e->r0 = a; e->r1 = b; e->r2 = c;
        t->wait_pid = -3;
    } else {
        if (e->wwait) { f->rax = (u64)-MOS_EAGAIN; return (u64)f; }
        e->wwait = t; e->wop = op; e->wfd = fd; e->w0 = a; e->w1 = b; e->w2 = c;
        t->wait_pid = -4;
    }
    t->frame = (u64)f;
    t->state = TH_WAITING;
    return sched_switch(f);
}

static i64 op_socket(process_t *p, u64 domain, u64 type, u64 proto) {
    if ((int)domain != 1) return -MOS_EAFNOSUPPORT;
    if ((int)proto != 0) return -MOS_EPROTONOSUPPORT;
    int kind = 0;
    int fl = type_flags(type, &kind);
    if (fl < 0) return fl;
    uend_t *e = alloc_end(kind, p);
    if (!e) return -MOS_ENOMEM;
    int fd = install(p, e, fl);
    if (fd < 0) { e->refs = 0; e->used = 0; memset(e, 0, sizeof *e); }
    return fd;
}

static i64 op_bind(process_t *p, int fd, u64 addr, u64 len) {
    uend_t *e = end_of(p, fd);
    if (!e) return nosock(p, fd);
    if (e->name[0]) return -MOS_EINVAL;
    char path[UNIX_PATH];
    i64 rs = read_sun(p, addr, len, path);
    if (rs < 0) return rs;
    if (name_find(path) >= 0 || vfs_lookup(path)) return -MOS_EADDRINUSE;
    i64 pk = parent_ok(path);
    if (pk < 0) return pk;
    int slot = -1;
    for (int i = 0; i < UNIX_NAMES; i++) if (!g_name[i].used) { slot = i; break; }
    if (slot < 0) return -MOS_ENOSPC;
    if (!vfs_create(path)) return -MOS_ENOSPC;
    g_name[slot].used = 1;
    g_name[slot].slot = slot_of(e);
    memcpy(g_name[slot].path, path, UNIX_PATH);
    memcpy(e->name, path, UNIX_PATH);
    return 0;
}

static i64 op_listen(process_t *p, int fd, int backlog) {
    uend_t *e = end_of(p, fd);
    if (!e) return nosock(p, fd);
    if (e->kind != 1) return -MOS_EOPNOTSUPP;
    if (e->linked) return -MOS_EINVAL;
    if (!e->name[0]) return -MOS_EINVAL;
    if (backlog < 1) backlog = 1;
    if (backlog > UNIX_BACKLOG) backlog = UNIX_BACKLOG;
    e->backlog = backlog;
    e->listening = 1;
    wake_connector(e->name);
    return 0;
}

static i64 op_shutdown(process_t *p, int fd, int how) {
    uend_t *e = end_of(p, fd);
    if (!e) return nosock(p, fd);
    if (how < 0 || how > 2) return -MOS_EINVAL;
    if (how != 1) { e->shut_rd = 1; free_rx(e); pump_r(e); }
    if (how != 0) {
        e->shut_wr = 1;
        if (e->peer && e->peer->used) pump_r(e->peer);
    }
    return 0;
}

static i64 op_name(process_t *p, int fd, u64 addr, u64 lenp, int peer) {
    uend_t *e = end_of(p, fd);
    if (!e) return nosock(p, fd);
    const char *path = e->name;
    if (peer) {
        if (e->kind == 1) {
            if (!e->linked || !e->peer || !e->peer->used) return -MOS_ENOTCONN;
            path = e->peer->name;
        } else if (!e->linked) return -MOS_ENOTCONN;
        else path = e->dest;
    }
    return write_sun(p, addr, lenp, path ? path : "");
}

static i64 op_pair(process_t *p, u64 type, u64 proto, u64 outp) {
    if ((int)proto != 0) return -MOS_EPROTONOSUPPORT;
    int kind = 0;
    int fl = type_flags(type, &kind);
    if (fl < 0) return fl;
    uend_t *a = alloc_end(kind, p);
    uend_t *b = a ? alloc_end(kind, p) : NULL;
    if (!b) {
        if (a) { a->used = 0; a->refs = 0; memset(a, 0, sizeof *a); }
        return -MOS_ENOMEM;
    }
    a->peer = b; b->peer = a; a->linked = b->linked = 1;
    int fa = install(p, a, fl);
    int fb = fa < 0 ? -1 : install(p, b, fl);
    if (fb < 0) {
        if (fa >= 0) { p->fds[fa].used = false; }
        a->used = b->used = 0;
        a->refs = b->refs = 0;
        memset(a, 0, sizeof *a);
        memset(b, 0, sizeof *b);
        return fa < 0 ? fa : -MOS_EMFILE;
    }
    int pair[2] = { fa, fb };
    if (!uvm_copy_out(p, outp, pair, sizeof pair)) {
        p->fds[fa].used = false;
        p->fds[fb].used = false;
        a->used = b->used = 0;
        memset(a, 0, sizeof *a);
        memset(b, 0, sizeof *b);
        return -MOS_EFAULT;
    }
    return 0;
}

static i64 op_getsockopt(process_t *p, int fd, u64 blk) {
    mos_unix_sockopt_t o;
    if (!uvm_copy_in(p, &o, blk, sizeof o)) return -MOS_EFAULT;
    uend_t *e = end_of(p, fd);
    if (!e) return nosock(p, fd);
    u8 tmp[76];
    memset(tmp, 0, sizeof tmp);
    u32 n = 4;
    int v = 0;
    int pid = 0, uid = 501, gid = 20;
    int cred = -MOS_ENOTCONN;
    if (e->kind == 2 && e->linked && e->dest_pid) { pid = e->dest_pid; cred = 0; }
    else if (e->kind == 2 && e->last_pid) { pid = e->last_pid; uid = e->last_uid ? e->last_uid : 501; gid = e->last_gid ? e->last_gid : 20; cred = 0; }
    else if (e->peer && e->peer->used) { pid = e->peer->creator_pid; uid = e->peer->uid; gid = e->peer->gid; cred = 0; }

    if (o.name == UNIX_LOCAL_PEERCRED && (o.level == UNIX_SOL_LOCAL || o.level == UNIX_SOL_SOCKET)) {
        if (cred < 0) return cred;
        u32 ver = 0, u = (u32)uid, g = (u32)gid;
        u16 ng = 1;
        memcpy(tmp, &ver, 4);
        memcpy(tmp + 4, &u, 4);
        memcpy(tmp + 8, &ng, 2);
        memcpy(tmp + 12, &g, 4);
        n = 76;
    } else if ((o.name == UNIX_LOCAL_PEERPID || o.name == UNIX_LOCAL_PEEREPID) &&
               (o.level == UNIX_SOL_LOCAL || o.level == UNIX_SOL_SOCKET)) {
        if (cred < 0) return cred;
        v = pid;
        memcpy(tmp, &v, 4);
    } else if (o.level == UNIX_SOL_SOCKET && o.name == UNIX_SO_ERROR) {
        memcpy(tmp, &v, 4);
    } else if (o.level == UNIX_SOL_SOCKET && o.name == UNIX_SO_TYPE) {
        v = e->kind;
        memcpy(tmp, &v, 4);
    } else if (o.level == UNIX_SOL_SOCKET && o.name == UNIX_SO_ACCEPTCONN) {
        v = e->listening ? 1 : 0;
        memcpy(tmp, &v, 4);
    } else if (o.level == UNIX_SOL_SOCKET && (o.name == UNIX_SO_SNDBUF || o.name == UNIX_SO_RCVBUF)) {
        v = e->kind == 2 ? UNIX_DGRAM : UNIX_RX;
        memcpy(tmp, &v, 4);
    } else return -MOS_ENOPROTOOPT;
    u32 copy = o.len < n ? o.len : n;
    if (o.val && copy && !uvm_copy_out(p, (u64)o.val, tmp, copy)) return -MOS_EFAULT;
    o.len = n;
    if (!uvm_copy_out(p, blk, &o, sizeof o)) return -MOS_EFAULT;
    return 0;
}

static i64 rx_ready(uend_t *e) {
    if (e->listening) return e->qcount;
    if (!e->head) return 0;
    if (e->kind == 2) return e->head->len - e->head->off;
    return e->rx_bytes;
}

static int rx_eof(uend_t *e) {
    if (e->listening) return 0;
    if (e->head) return 0;
    if (e->shut_rd) return 1;
    if (e->kind == 1 && e->linked && (!e->peer || !e->peer->used || e->peer->shut_wr)) return 1;
    return 0;
}

int unix_fd_path(process_t *p, int fd, char *out, int cap) {
    uend_t *e = end_of(p, fd);
    if (!e) return nosock(p, fd);
    int n = (int)strlen(e->name);
    if (n + 1 > cap) return -MOS_EINVAL;
    memcpy(out, e->name, (usize)n + 1);
    return n;
}

int unix_fstat(process_t *p, int fd, void *stp) {
    uend_t *e = end_of(p, fd);
    if (!e) return -MOS_ENOTSOCK;
    mos_stat_t st;
    memset(&st, 0, sizeof st);
    st.is_dir = 6;
    i64 n = rx_ready(e);
    st.size = n > 0 ? (u64)n : 0;
    st.mtime = rx_eof(e) ? 1 : 0;
    memcpy(stp, &st, sizeof st);
    return 0;
}

/* read() não entrega SCM_RIGHTS: um recado só de descritores é descartado. Um recv não atravessa
 * o fim do send (leitura curta). Datagrama: o que não cabe é descartado. */
static i64 try_raw_read(process_t *p, uend_t *e, u64 buf, u64 len) {
    if (e->listening && e->kind == 1) return -MOS_EINVAL;
    for (;;) {
        while (e->head && e->head->len == e->head->off) {
            umsg_t *z = e->head;
            e->head = z->next;
            if (!e->head) e->tail = NULL;
            e->rx_count--;
            free_msg(z);
        }
        if (!e->head) {
            if (e->shut_rd) return 0;
            if (e->kind == 1 && !e->linked) return -MOS_ENOTCONN;
            if (e->kind == 1 && (!e->peer || !e->peer->used || e->peer->shut_wr)) return 0;
            return -MOS_EAGAIN;
        }
        umsg_t *m = e->head;
        int avail = m->len - m->off;
        int take = avail;
        if ((u64)take > len) take = (int)len;
        if (take && !uvm_copy_out(p, buf, m->data + m->off, (usize)take)) return -MOS_EFAULT;
        if (e->kind == 2) {
            e->last_pid = m->from_pid;
            e->last_uid = m->from_uid;
            e->last_gid = m->from_gid;
            e->head = m->next;
            if (!e->head) e->tail = NULL;
            e->rx_bytes -= avail;
            e->rx_count--;
            free_msg(m);
        } else if (m->off + take >= m->len) {
            e->head = m->next;
            if (!e->head) e->tail = NULL;
            e->rx_bytes -= avail;
            e->rx_count--;
            free_msg(m);
        } else {
            m->off += take;
            e->rx_bytes -= take;
        }
        if (e->peer && e->peer->used) pump_w(e->peer);
        return take;
    }
}

static i64 try_raw_write(process_t *p, uend_t *e, u64 buf, u64 len) {
    if (e->kind == 1) {
        if (!e->linked) return -MOS_ENOTCONN;
        if (e->shut_wr || !e->peer || !e->peer->used || e->peer->shut_rd) return -MOS_EPIPE;
        int freeb = UNIX_RX - e->peer->rx_bytes;
        if (freeb < 0) freeb = 0;
        int n = (int)MIN(len, (u64)freeb);
        if (n == 0 && len) return -MOS_EAGAIN;
        u8 *data = NULL;
        if (n) {
            data = kmalloc((usize)n);
            if (!data) return -MOS_ENOMEM;
            if (!uvm_copy_in(p, data, buf, (usize)n)) { kfree(data); return -MOS_EFAULT; }
        }
        umsg_t *m = kmalloc(sizeof *m);
        if (!m) { kfree(data); return -MOS_ENOMEM; }
        memset(m, 0, sizeof *m);
        m->data = data;
        m->len = n;
        m->from_pid = e->creator_pid;
        enqueue_msg(e->peer, m);
        return n;
    }
    if (!(e->linked && e->dest[0])) return -MOS_EDESTADDRREQ;
    int ni = name_find(e->dest);
    if (ni < 0 || g_name[ni].slot < 0 || !g_end[g_name[ni].slot].used || g_end[g_name[ni].slot].kind != 2)
        return ni < 0 && !vfs_lookup(e->dest) ? -MOS_ENOENT : -MOS_ECONNREFUSED;
    uend_t *dst = &g_end[g_name[ni].slot];
    if (len > UNIX_DGRAM) return -MOS_EMSGSIZE;
    if (dst->shut_rd) return -MOS_ECONNREFUSED;
    if (dst->rx_count >= UNIX_DGRAMS) return -MOS_EAGAIN;
    int n = (int)len;
    u8 *data = NULL;
    if (n) {
        data = kmalloc((usize)n);
        if (!data) return -MOS_ENOMEM;
        if (!uvm_copy_in(p, data, buf, (usize)n)) { kfree(data); return -MOS_EFAULT; }
    }
    umsg_t *m = kmalloc(sizeof *m);
    if (!m) { kfree(data); return -MOS_ENOMEM; }
    memset(m, 0, sizeof *m);
    m->data = data;
    m->len = n;
    m->from_pid = e->creator_pid;
    m->from_uid = e->uid;
    m->from_gid = e->gid;
    if (e->name[0]) memcpy(m->from_path, e->name, UNIX_PATH);
    e->dest_pid = dst->creator_pid;
    enqueue_msg(dst, m);
    return n;
}

u64 unix_rw(int_frame_t *f, thread_t *t, process_t *p, int fd, int rd, u64 buf, u64 len) {
    uend_t *e = end_of(p, fd);
    if (!e) { f->rax = (u64)-MOS_EBADF; return (u64)f; }
    i64 r = rd ? try_raw_read(p, e, buf, len) : try_raw_write(p, e, buf, len);
    if (!rd && r == -MOS_EPIPE) {
        r = pipe_sig(p, 0, r);
        if (p->exited) return sched_switch(f);
    }
    if (r == -MOS_EAGAIN && !nonblock(p, fd, 0))
        return block_on(f, t, e, rd, rd ? W_READ : W_WRITE, fd, buf, len, 0);
    f->rax = (u64)r;
    return (u64)f;
}

u64 unix_syscall(int_frame_t *f, thread_t *t, process_t *p) {
    u64 op = f->rdi, a = f->rsi, b = f->rdx, c = f->r10;
    switch (op) {
    case MOS_UNIX_SOCKET:
        f->rax = (u64)op_socket(p, a, b, c);
        return (u64)f;
    case MOS_UNIX_BIND:
        f->rax = (u64)op_bind(p, (int)a, b, c);
        return (u64)f;
    case MOS_UNIX_LISTEN:
        f->rax = (u64)op_listen(p, (int)a, (int)b);
        return (u64)f;
    case MOS_UNIX_ACCEPT: {
        uend_t *e = end_of(p, (int)a);
        if (!e) { f->rax = (u64)nosock(p, (int)a); return (u64)f; }
        i64 r = try_accept(p, (int)a, e, b, c);
        if (r == -MOS_EAGAIN && !nonblock(p, (int)a, 0)) return block_on(f, t, e, 1, W_ACCEPT, (int)a, b, c, 0);
        f->rax = (u64)r;
        return (u64)f;
    }
    case MOS_UNIX_CONNECT: {
        uend_t *e = end_of(p, (int)a);
        if (!e) { f->rax = (u64)nosock(p, (int)a); return (u64)f; }
        char path[UNIX_PATH];
        i64 rs = read_sun(p, b, c, path);
        if (rs < 0) { f->rax = (u64)rs; return (u64)f; }
        i64 r = try_connect(p, e, path);
        if (r == -MOS_EAGAIN && !nonblock(p, (int)a, 0)) {
            memcpy(e->wpath, path, UNIX_PATH);
            return block_on(f, t, e, 0, W_CONNECT, (int)a, b, c, 0);
        }
        f->rax = (u64)r;
        return (u64)f;
    }
    case MOS_UNIX_SHUTDOWN:
        f->rax = (u64)op_shutdown(p, (int)a, (int)b);
        return (u64)f;
    case MOS_UNIX_SOCKNAME:
        f->rax = (u64)op_name(p, (int)a, b, c, 0);
        return (u64)f;
    case MOS_UNIX_PEERNAME:
        f->rax = (u64)op_name(p, (int)a, b, c, 1);
        return (u64)f;
    case MOS_UNIX_PAIR:
        f->rax = (u64)op_pair(p, a, b, c);
        return (u64)f;
    case MOS_UNIX_SENDMSG: {
        uend_t *e = end_of(p, (int)a);
        if (!e) { f->rax = (u64)(fd_open(p, (int)a) ? -MOS_ENOTSOCK : -MOS_EBADF); return (u64)f; }
        i64 r = try_send(p, (int)a, e, b, (int)c);
        if (r == -MOS_EAGAIN && !nonblock(p, (int)a, (int)c))
            return block_on(f, t, e, 0, W_SEND, (int)a, b, c, c);
        if (r == -MOS_EPIPE) {
            r = pipe_sig(p, (int)c, r);
            if (p->exited) return sched_switch(f);
        }
        f->rax = (u64)r;
        return (u64)f;
    }
    case MOS_UNIX_RECVMSG: {
        uend_t *e = end_of(p, (int)a);
        if (!e) { f->rax = (u64)nosock(p, (int)a); return (u64)f; }
        i64 r = try_recv(p, (int)a, e, b, (int)c);
        if (r == -MOS_EAGAIN && !nonblock(p, (int)a, (int)c))
            return block_on(f, t, e, 1, W_RECV, (int)a, b, c, 0);
        f->rax = (u64)r;
        return (u64)f;
    }
    case MOS_UNIX_GETSOCKOPT:
        f->rax = (u64)op_getsockopt(p, (int)a, b);
        return (u64)f;
    default:
        f->rax = (u64)-MOS_EINVAL;
        return (u64)f;
    }
}

/* fd aberto que não é unix: ENOTSOCK. fd inexistente: EBADF. */