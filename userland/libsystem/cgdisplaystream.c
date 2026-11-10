/*
 * CGDisplayStream em cima de cx_cap_take (TCC + DEV_SCREEN_CAPTURE).
 * O handler do macOS é um bloco. A entrega corre numa thread do libSystem;
 * CGDisplayStreamGetRunLoopSource devolve NULL de propósito: não há CFRunLoopSource.
 * O formato pedido que não seja BGRA continua BGRA, que é o do compositor.
 */
#include "foundation.h"
#include "sys.h"

void *calloc(size_t n, size_t s);
void free(void *p);
id objc_retain(id o);
void objc_release(id o);
int usleep(unsigned us);
unsigned long long mach_absolute_time(void);

#include "screencap_run.h"
extern const void *kCGDisplayStreamMinimumFrameTime;

typedef struct { double x, y, w, h; } cgrect_t;
typedef struct {
    cx_cap_run run;
    unsigned *px;
    void *handler;
    cgrect_t rect;
    cgrect_t moved_none;
} cgd_t;

static void cg_fire(void *ctx, id surf, id pb, id sb) {
    (void)pb; (void)sb;
    cgd_t *g = ctx;
    if (!g->handler || !surf) return;
    g->rect.x = 0;
    g->rect.y = 0;
    g->rect.w = g->run.cap.w;
    g->rect.h = g->run.cap.h;
    void *inv = *(void **)((unsigned char *)g->handler + 16);
    if (!inv) return;
    ((void (*)(void *, int, unsigned long long, id, void *))inv)(g->handler, 0, mach_absolute_time(), surf, g);
}

void *CGDisplayStreamCreate(unsigned display, size_t w, size_t h, int fmt, id props, void *handler) {
    (void)display; (void)fmt;
    if (!handler) return 0;
    int pw = (int)w, ph = (int)h;
    int cap = pw > 0 && ph > 0 ? pw * ph : 4096 * 2160;
    if (cap <= 0 || cap > 4096 * 2160) cap = 4096 * 2160;
    cgd_t *g = calloc(1, sizeof *g);
    unsigned *px = calloc((size_t)cap, sizeof(unsigned));
    if (!g || !px) { free(g); free(px); return 0; }
    g->px = px;
    g->handler = handler;
    g->run.cap.px = px;
    g->run.cap.cap = cap;
    g->run.w = pw;
    g->run.h = ph;
    g->run.on_frame = cg_fire;
    g->run.ctx = g;
    id v = props ? MSG(id, props, "objectForKey:", (id)kCGDisplayStreamMinimumFrameTime) : 0;
    double sec = v ? MSG0(double, v, "doubleValue") : 0;
    if (sec > 0) g->run.cap.interval_us = (long long)(sec * 1000000.0);
    g->rect.w = pw;
    g->rect.h = ph;
    return g;
}
void *CGDisplayStreamGetRunLoopSource(void *s) { (void)s; return 0; }
int CGDisplayStreamStart(void *s) {
    cgd_t *g = s;
    if (!g) return 1001;
    g->rect.w = g->run.w;
    g->rect.h = g->run.h;
    return cx_cap_start(&g->run) == 0 ? 0 : 1001;
}
int CGDisplayStreamStop(void *s) {
    cgd_t *g = s;
    if (!g) return 0;
    cx_cap_stop(&g->run);
    return 0;
}
const cgrect_t *CGDisplayStreamUpdateGetRects(void *u, int type, size_t *count) {
    cgd_t *g = u;
    if (count) *count = 0;
    if (!g) return 0;
    if (type == 2) return &g->moved_none;
    if (count) *count = 1;
    if (g->rect.w <= 0) g->rect.w = g->run.cap.w;
    if (g->rect.h <= 0) g->rect.h = g->run.cap.h;
    return &g->rect;
}
