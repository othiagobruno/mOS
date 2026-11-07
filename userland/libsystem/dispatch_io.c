/*
 * mOS libSystem — dispatch_io (canais de E/S do libdispatch) e dispatch_read/dispatch_write, de forma simples: o
 * canal guarda o fd; uma leitura lê tudo o que foi pedido (até o fim, com SIZE_MAX), uma escrita grava tudo, e o
 * handler é chamado uma vez, na fila pedida e de forma assíncrona, com done = true. O objeto tem o isa
 * OS_dispatch_io (retain/release pelo dispatch.c, como os outros objetos do dispatch). O WhatsApp o usa na partida.
 */
#include "foundation.h"
id objc_retain(id o);
void objc_release(id o);
#include "libc.h"

typedef unsigned long usz;
typedef long long off64;
extern char OS_dispatch_io_c[] __asm__("OBJC_CLASS_$_OS_dispatch_io");
typedef struct { void *isa; u64 magic; int fd, closed, own; unsigned long type; void *cleanup; void *cleanup_q; } cx_io_t;
#define IOMAGIC 0x64696f31ull   /* "dio1" */
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, ...); } io_blk_t;

void *_Block_copy(const void *b);
void _Block_release(const void *b);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_get_global_queue(long prio, unsigned long flags);
void *dispatch_data_create(const void *buf, usz n, void *queue, void *destructor);
extern const void *_dispatch_data_destructor_free;
ssize_t pread(int fd, void *b, size_t n, off_t off);
ssize_t pwrite(int fd, const void *b, size_t n, off_t off);

static void *q_or_global(void *q) { return q ? q : dispatch_get_global_queue(0, 0); }

void *dispatch_io_create(unsigned long type, int fd, void *queue, void *cleanup) {
    cx_io_t *io = calloc(1, sizeof *io);
    if (!io) return 0;
    io->isa = OS_dispatch_io_c; io->magic = IOMAGIC; io->fd = fd; io->type = type;
    io->cleanup = cleanup ? _Block_copy(cleanup) : 0; io->cleanup_q = queue;
    return io;
}
void *dispatch_io_create_with_path(unsigned long type, const char *path, int oflag, unsigned mode, void *queue, void *cleanup) {
    int fd = open(path, oflag, mode);
    if (fd < 0) return 0;
    cx_io_t *io = dispatch_io_create(type, fd, queue, cleanup);
    if (io) io->own = 1;
    return io;
}
void *dispatch_io_create_with_io(unsigned long type, void *other, void *queue, void *cleanup) {
    cx_io_t *o = other;
    return o ? dispatch_io_create(type, o->fd, queue, cleanup) : 0;
}
void dispatch_io_set_low_water(void *io, usz n) { (void)io; (void)n; }
void dispatch_io_set_high_water(void *io, usz n) { (void)io; (void)n; }
void dispatch_io_set_interval(void *io, u64 ns, unsigned long flags) { (void)io; (void)ns; (void)flags; }
int dispatch_io_get_descriptor(void *io) { return io ? ((cx_io_t *)io)->fd : -1; }
void dispatch_io_barrier(void *io, void *block) { (void)io; if (block) ((io_blk_t *)block)->invoke(block); }

typedef struct { void *block; id data; int err; int kind; } io_call_t;   /* kind 0: (bool, data, int); 1: (data, int) */
static void io_deliver(void *p) {
    io_call_t *c = p;
    io_blk_t *b = c->block;
    if (c->kind == 0) ((void (*)(void *, int, id, int))b->invoke)(b, 1, c->data, c->err);
    else ((void (*)(void *, id, int))b->invoke)(b, c->data, c->err);
    if (c->data) objc_release(c->data);
    _Block_release(c->block);
    free(c);
}
static void io_post(void *queue, void *handler, id data, int err, int kind) {
    if (!handler) { if (data) objc_release(data); return; }
    io_call_t *c = malloc(sizeof *c);
    if (!c) return;
    c->block = _Block_copy(handler); c->data = data; c->err = err; c->kind = kind;
    dispatch_async_f(q_or_global(queue), c, io_deliver);
}
/* lê até n bytes (n = SIZE_MAX: até o fim); random: a partir de off, sem mexer na posição */
static id io_read_all(int fd, int random, off64 off, usz n, int *err) {
    usz cap = 64 * 1024, len = 0;
    unsigned char *buf = malloc(cap);
    *err = 0;
    if (!buf) { *err = 12; return 0; }
    while (len < n) {
        if (len == cap) {
            unsigned char *nb = realloc(buf, cap * 2);
            if (!nb) { *err = 12; break; }
            buf = nb; cap *= 2;
        }
        usz want = cap - len < n - len ? cap - len : n - len;
        ssize_t r = random ? pread(fd, buf + len, want, (off_t)(off + (off64)len)) : read(fd, buf + len, want);
        if (r < 0) { *err = 5; break; }
        if (r == 0) break;
        len += (usz)r;
    }
    id d = dispatch_data_create(buf, len, 0, 0);       /* copia */
    free(buf);
    return d;
}
void dispatch_io_read(void *io, off64 off, usz n, void *queue, void *handler) {
    cx_io_t *c = io;
    int err = 9;
    id d = 0;
    if (c && !c->closed) d = io_read_all(c->fd, c->type == 1, off, n, &err);
    io_post(queue, handler, d, err, 0);
}
static int io_write_all(int fd, int random, off64 off, id data) {
    usz n = data ? (usz)((u64 (*)(id, SEL))objc_msgSend)(data, sel_registerName("length")) : 0;
    const unsigned char *b = data ? ((const unsigned char *(*)(id, SEL))objc_msgSend)(data, sel_registerName("bytes")) : 0;
    usz done = 0;
    while (b && done < n) {
        ssize_t w = random ? pwrite(fd, b + done, n - done, (off_t)(off + (off64)done)) : write(fd, b + done, n - done);
        if (w <= 0) return 5;
        done += (usz)w;
    }
    return 0;
}
void dispatch_io_write(void *io, off64 off, id data, void *queue, void *handler) {
    cx_io_t *c = io;
    int err = c && !c->closed ? io_write_all(c->fd, c->type == 1, off, data) : 9;
    io_post(queue, handler, 0, err, 0);
}
void dispatch_io_close(void *io, unsigned long flags) {
    (void)flags;
    cx_io_t *c = io;
    if (!c || c->closed) return;
    c->closed = 1;
    if (c->own) close(c->fd);
    if (c->cleanup) {                                  /* cleanup_handler(int error) */
        io_blk_t *b = c->cleanup;
        ((void (*)(void *, int))b->invoke)(b, 0);
        _Block_release(c->cleanup);
        c->cleanup = 0;
    }
}
void dispatch_read(int fd, usz n, void *queue, void *handler) {
    int err;
    id d = io_read_all(fd, 0, 0, n, &err);
    io_post(queue, handler, d, err, 1);
}
void dispatch_write(int fd, id data, void *queue, void *handler) {
    int err = io_write_all(fd, 0, 0, data);
    io_post(queue, handler, 0, err, 1);
}
