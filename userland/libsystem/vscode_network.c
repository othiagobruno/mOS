/*
 * mOS libSystem — Network.framework: o monitor de caminho (nw_path_monitor) que o Chromium usa para saber
 * se há rede. O mOS tem uma interface (en0, 10.0.2.15 no QEMU) sempre ligada: o monitor entrega uma vez,
 * na fila pedida, um caminho "satisfied".
 */
#include "foundation.h"
void *malloc(size_t n);
void free(void *p);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_get_main_queue(void);
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, void *); } nw_block_t;
typedef struct { int status; } nw_path_t;
typedef struct { void *queue; nw_block_t *handler; int started, canceled; } nw_monitor_t;
static nw_path_t path_ok = {1};   /* nw_path_status_satisfied */
void *nw_path_monitor_create(void) { nw_monitor_t *m = malloc(sizeof *m); if (m) { m->queue = 0; m->handler = 0; m->started = m->canceled = 0; } return m; }
void nw_path_monitor_set_queue(void *mon, void *q) { nw_monitor_t *m = mon; if (m) m->queue = q; }
void nw_path_monitor_set_update_handler(void *mon, void *blk) { nw_monitor_t *m = mon; if (m) m->handler = blk ? _Block_copy(blk) : 0; }
static void deliver(void *arg) { nw_monitor_t *m = arg; if (!m->canceled && m->handler) m->handler->invoke(m->handler, &path_ok); }
void nw_path_monitor_start(void *mon) {
    nw_monitor_t *m = mon;
    if (!m || m->started) return;
    m->started = 1;
    dispatch_async_f(m->queue ? m->queue : dispatch_get_main_queue(), m, deliver);
}
void nw_path_monitor_cancel(void *mon) { nw_monitor_t *m = mon; if (m) m->canceled = 1; }
int nw_path_get_status(void *path) { return path ? ((nw_path_t *)path)->status : 2; /* unsatisfied */ }
