/*
 * mOS libSystem — dispatch_io (canais de E/S da libdispatch) e dispatch_read/dispatch_write.
 * Cada canal tem uma fila serial própria: leituras, escritas e barreiras rodam em ordem, fora da thread de quem pediu.
 * Leitura entrega pedaços entre a marca baixa e a alta (done = false) e o resto no fim (done = true); com
 * SIZE_MAX lê até o fim do arquivo. DISPATCH_IO_STOP no close cancela o que falta (ECANCELED). O cleanup handler
 * roda quando o canal fecha e a última operação terminou; o fd aberto pelo próprio canal é fechado aí.
 */
#include "dispatch_plat.h"
#ifdef MOS_HOST_TEST
#include <fcntl.h>
#include <errno.h>
#define dio_open open
#define dio_read read
#define dio_write write
#define dio_pread pread
#define dio_pwrite pwrite
#define dio_close close
#else
typedef long ssize_t;
int open(const char *p, int f, ...);
long read(int fd, void *b, size_t n);
long pread(int fd, void *b, size_t n, long long off);
long pwrite(int fd, const void *b, size_t n, long long off);
int close(int fd);
#define dio_open open
#define dio_read read
#define dio_write write
#define dio_pread pread
#define dio_pwrite pwrite
#define dio_close close
#endif

extern char OS_dispatch_io_c[] __asm__("OBJC_CLASS_$_OS_dispatch_io");
void *dispatch_queue_create(const char *label, void *attr);
void *dispatch_get_global_queue(long id, unsigned long flags);
void *dispatch_data_create(const void *buf, size_t n, void *queue, void *destructor);
size_t dispatch_data_get_size(void *d);
void *dispatch_data_create_map(void *d, const void **buf, size_t *n);
void *dispatch_data_create_subrange(void *d, size_t off, size_t len);
extern dp_obj_t _dispatch_data_empty;

#define IO_STREAM 0
#define IO_RANDOM 1
#define IO_STOP 1
#define E_BADF 9
#define E_CANCELED 89
#define E_NOMEM 12

typedef struct dp_io {
    dp_obj_t h;
    int fd, own, type;
    volatile int closed, stopped, ops;
    size_t low, high;
    u64 interval;
    void *cq;                      /* fila serial do canal */
    void *cleanup;                 /* bloco (int error) */
    void *cleanup_q;
    int cleanup_sent;
    struct dp_io *parent;          /* dispatch_io_create_with_io: o fd é do outro canal */
    dp_lock_t lock;
} dp_io_t;

static void io_dispose(dp_obj_t *h) {
    dp_io_t *io = (dp_io_t *)h;
    if (io->cleanup) _Block_release(io->cleanup);
    if (io->cq) cx_dispatch_release(io->cq);
    if (io->cleanup_q) cx_dispatch_release(io->cleanup_q);
    if (io->parent) cx_dispatch_release(io->parent);
}
static void *qor(void *q) { return q ? q : dispatch_get_global_queue(0, 0); }

typedef struct { void *blk; int err; } io_clean_t;
static void clean_run(void *p) {
    io_clean_t *c = p;
    ((void (*)(void *, int))((dp_block_t *)c->blk)->invoke)(c->blk, c->err);
    _Block_release(c->blk);
    free(c);
}
/* canal fechado e sem operação pendente: fecha o fd próprio e chama o cleanup */
static void io_maybe_finish(dp_io_t *io) {
    dp_lock(&io->lock);
    int go = io->closed && io->ops == 0 && !io->cleanup_sent;
    if (go) io->cleanup_sent = 1;
    void *blk = go ? io->cleanup : 0;
    if (go) io->cleanup = 0;
    dp_unlock(&io->lock);
    if (!go) return;
    if (io->own && io->fd >= 0) dio_close(io->fd);
    if (blk) {
        io_clean_t *c = malloc(sizeof *c);
        if (!c) abort();
        c->blk = blk; c->err = 0;
        dispatch_async_f(qor(io->cleanup_q), c, clean_run);
    }
}
static dp_io_t *io_new(int type, int fd, void *queue, void *cleanup) {
    if (type != IO_STREAM && type != IO_RANDOM) return 0;
    dp_io_t *io = calloc(1, sizeof *io);
    if (!io) return 0;
    dp_obj_init(io, OS_dispatch_io_c, DK_IO, io_dispose);
    io->fd = fd; io->type = type;
    io->low = 1; io->high = (size_t)-1;
    io->cq = dispatch_queue_create("com.apple.libdispatch-io.channelq", 0);
    if (cleanup) io->cleanup = _Block_copy(cleanup);
    if (queue) { cx_dispatch_retain(queue); io->cleanup_q = queue; }
    return io;
}
void *dispatch_io_create(unsigned long type, int fd, void *queue, void *cleanup) { return fd < 0 ? 0 : io_new((int)type, fd, queue, cleanup); }
void *dispatch_io_create_with_path(unsigned long type, const char *path, int oflag, unsigned mode, void *queue, void *cleanup) {
    if (!path || path[0] != '/') return 0;
    int fd = dio_open(path, oflag, mode);
    if (fd < 0) return 0;
    dp_io_t *io = io_new((int)type, fd, queue, cleanup);
    if (io) io->own = 1; else dio_close(fd);
    return io;
}
void *dispatch_io_create_with_io(unsigned long type, void *other, void *queue, void *cleanup) {
    dp_io_t *o = other;
    if (!o || o->closed) return 0;
    dp_io_t *io = io_new((int)type, o->fd, queue, cleanup);
    if (io) { cx_dispatch_retain(o); io->parent = o; }
    return io;
}
void dispatch_io_set_low_water(void *p, size_t n) { if (p) ((dp_io_t *)p)->low = n ? n : 1; }
void dispatch_io_set_high_water(void *p, size_t n) { if (p) ((dp_io_t *)p)->high = n ? n : 1; }
void dispatch_io_set_interval(void *p, u64 ns, unsigned long flags) { (void)flags; if (p) ((dp_io_t *)p)->interval = ns; }
int dispatch_io_get_descriptor(void *p) { dp_io_t *io = p; return io && !io->closed ? io->fd : -1; }

/* ------------------------------------------------------------------ operações */
typedef struct { void *blk; void *data; int done, err, kind; } io_call_t;   /* kind 0: (bool, data, int); 1: (data, int) */
static void call_run(void *p) {
    io_call_t *c = p;
    void *d = c->data ? c->data : (void *)&_dispatch_data_empty;
    if (c->kind == 0) ((void (*)(void *, _Bool, void *, int))((dp_block_t *)c->blk)->invoke)(c->blk, c->done != 0, d, c->err);
    else ((void (*)(void *, void *, int))((dp_block_t *)c->blk)->invoke)(c->blk, d, c->err);
    if (c->data) cx_dispatch_release(c->data);
    free(c);
}
static void post(void *q, void *blk, void *data, int done, int err, int kind) {
    io_call_t *c = malloc(sizeof *c);
    if (!c) abort();
    c->blk = blk; c->data = data; c->done = done; c->err = err; c->kind = kind;
    dispatch_async_f(qor(q), c, call_run);
}
typedef struct { dp_io_t *io; int fd, random, kind; long long off; size_t len; void *data; void *q; void *blk; } io_op_t;
static void op_end(io_op_t *o) {
    _Block_release(o->blk);
    if (o->q) cx_dispatch_release(o->q);
    if (o->data) cx_dispatch_release(o->data);
    dp_io_t *io = o->io;
    free(o);
    if (io) {
        __sync_fetch_and_sub(&io->ops, 1);
        io_maybe_finish(io);
        cx_dispatch_release(io);
    }
}
/* o bloco do handler fica vivo até a última entrega: cada post leva uma cópia */
static void *bcopy_(void *b) { return _Block_copy(b); }
static void op_read(void *p) {
    io_op_t *o = p;
    size_t high = o->io ? o->io->high : (size_t)-1;
    size_t cap = 64 * 1024, len = 0, total = 0;
    unsigned char *buf = malloc(cap);
    int err = 0;
    if (!buf) err = E_NOMEM;
    if (o->fd < 0) err = E_BADF;
    while (!err && total < o->len) {
        if (o->io && o->io->stopped) { err = E_CANCELED; break; }
        if (len == cap) { unsigned char *nb = realloc(buf, cap * 2); if (!nb) { err = E_NOMEM; break; } buf = nb; cap *= 2; }
        size_t want = cap - len;
        if (want > o->len - total) want = o->len - total;
        long r = o->random ? dio_pread(o->fd, buf + len, want, o->off + (long long)total) : dio_read(o->fd, buf + len, want);
        if (r < 0) { err = 5; break; }
        if (r == 0) break;
        len += (size_t)r;
        total += (size_t)r;
        while (o->kind == 0 && high != (size_t)-1 && len >= high && total < o->len) {   /* pedaço parcial (marca alta) */
            post(o->q, bcopy_(o->blk), dispatch_data_create(buf, high, 0, 0), 0, 0, 0);
            memmove(buf, buf + high, len - high);
            len -= high;
        }
    }
    void *d = len ? dispatch_data_create(buf, len, 0, 0) : 0;
    free(buf);
    post(o->q, bcopy_(o->blk), d, 1, err, o->kind);
    op_end(o);
}
static void op_write(void *p) {
    io_op_t *o = p;
    const void *b = 0;
    size_t n = 0, done = 0;
    void *map = o->data ? dispatch_data_create_map(o->data, &b, &n) : 0;
    int err = o->fd < 0 ? E_BADF : 0;
    while (!err && done < n) {
        if (o->io && o->io->stopped) { err = E_CANCELED; break; }
        long w = o->random ? dio_pwrite(o->fd, (const char *)b + done, n - done, o->off + (long long)done)
                           : dio_write(o->fd, (const char *)b + done, n - done);
        if (w <= 0) { err = 5; break; }
        done += (size_t)w;
    }
    void *rest = done < n ? dispatch_data_create_subrange(o->data, done, n - done) : 0;
    if (map) cx_dispatch_release(map);
    post(o->q, bcopy_(o->blk), rest, 1, err, o->kind);
    op_end(o);
}
static void op_submit(dp_io_t *io, int fd, int random, long long off, size_t len, void *data, void *q, void *blk, int kind, int write) {
    if (!blk) return;
    io_op_t *o = calloc(1, sizeof *o);
    if (!o) abort();
    o->io = io; o->fd = fd; o->random = random; o->off = off; o->len = len; o->kind = kind;
    o->blk = _Block_copy(blk);
    if (q) { cx_dispatch_retain(q); o->q = q; }
    if (data) { cx_dispatch_retain(data); o->data = data; }
    if (io) {
        if (io->closed) { post(q, bcopy_(o->blk), 0, 1, E_CANCELED, kind); io = o->io = 0; op_end(o); return; }
        cx_dispatch_retain(io);
        __sync_fetch_and_add(&io->ops, 1);
        dispatch_async_f(io->cq, o, write ? op_write : op_read);
    } else dispatch_async_f(dispatch_get_global_queue(0, 0), o, write ? op_write : op_read);
}
void dispatch_io_read(void *p, long long off, size_t n, void *q, void *handler) {
    dp_io_t *io = p;
    if (!io) return;
    op_submit(io, io->fd, io->type == IO_RANDOM, off, n, 0, q, handler, 0, 0);
}
void dispatch_io_write(void *p, long long off, void *data, void *q, void *handler) {
    dp_io_t *io = p;
    if (!io) return;
    op_submit(io, io->fd, io->type == IO_RANDOM, off, 0, data, q, handler, 0, 1);
}
typedef struct { dp_io_t *io; void *blk; } io_bar_t;
static void bar_run(void *p) {
    io_bar_t *b = p;
    DP_RUN_BLOCK(b->blk);
    _Block_release(b->blk);
    __sync_fetch_and_sub(&b->io->ops, 1);
    io_maybe_finish(b->io);
    cx_dispatch_release(b->io);
    free(b);
}
void dispatch_io_barrier(void *p, void *block) {
    dp_io_t *io = p;
    if (!io || !block) return;
    io_bar_t *b = malloc(sizeof *b);
    if (!b) abort();
    b->io = io; b->blk = _Block_copy(block);
    cx_dispatch_retain(io);
    __sync_fetch_and_add(&io->ops, 1);
    dispatch_async_f(io->cq, b, bar_run);
}
void dispatch_io_close(void *p, unsigned long flags) {
    dp_io_t *io = p;
    if (!io || io->closed) return;
    if (flags & IO_STOP) io->stopped = 1;
    io->closed = 1;
    io_maybe_finish(io);
}
void dispatch_read(int fd, size_t n, void *q, void *handler) { op_submit(0, fd, 0, 0, n, 0, q, handler, 1, 0); }
void dispatch_write(int fd, void *data, void *q, void *handler) { op_submit(0, fd, 0, 0, 0, data, q, handler, 1, 1); }
