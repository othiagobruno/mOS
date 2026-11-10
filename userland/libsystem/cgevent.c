/*
 * mOS libSystem — CGEvent (CoreGraphics): eventos sintéticos de teclado, mouse e rolagem. CGEventPost manda o
 * evento para a fila global (GUI_DEVICE/DEV_INPUT_POST). CGEventPostToPid entrega na fila do processo
 * (DEV_INPUT_TO_PID) e não muda o foco nem o cursor. Kernel antigo sem DEV_INPUT_POST: a tecla do post global
 * vai só para a janela-chave deste app.
 */
#include "foundation.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"
#include "computeruse_win.h"

void *memset(void *d, int c, size_t n);
void *malloc(size_t n);
void free(void *p);
void cx_post_key_event(u16 mac, u64 flags, int down);
id objc_retain(id);

typedef struct { double x, y; } cgpt_t;
typedef struct {
    void *isa;
    u32 type;                   /* CGEventType */
    u16 key; u8 down, button;
    u64 flags, ts;
    cgpt_t loc;
    i64 clicks, user;
    i32 wheel1, wheel2, wheel3, units;
    double d1, d2, d3;          /* kCGScrollWheelEventFixedPtDeltaAxis1/2/3 (campos 93, 94, 95) */
    u8 dvalid;                  /* bits 0..2: o double correspondente foi gravado */
    u16 *uni; u32 nuni;         /* CGEventKeyboardSetUnicodeString */
} cgev_t;

cx_class_t CGEventMOS_c __asm__("OBJC_CLASS_$_CGEventMOS"), CGEventMOS_m __asm__("OBJC_METACLASS_$_CGEventMOS");
static int g_inited;
static void boot(void) { if (!g_inited++ && !cx_class_named("CGEventMOS")) cx_define_class(&CGEventMOS_c, &CGEventMOS_m, "CGEventMOS", cx_class_named("NSObject"), (u32)(sizeof(cgev_t) - 8)); }
static cgev_t *ev(void *e) { return e && g_inited && cx_is_subclass(object_getClass((id)e), &CGEventMOS_c) ? (cgev_t *)e : 0; }

/* posição atual do cursor em coordenadas globais (origem no canto superior esquerdo da tela principal) */
static cgpt_t g_cursor = {-1, -1};
static cgpt_t cursor_now(void) {
    if (g_cursor.x >= 0) return g_cursor;
    cgpt_t p = {0, 0};
    gui_wininfo_t si;
    memset(&si, 0, sizeof si);
    SYS1(GUI_SCREEN, &si);
    p.x = si.w / 2; p.y = si.h / 2;
    return p;
}

static cgev_t *mk(u32 type) {
    boot();
    cgev_t *e = (cgev_t *)cx_alloc_instance(&CGEventMOS_c);
    e->type = type;
    e->ts = (u64)SYS0(SYS_TIME) * 1000000ull;
    e->loc = cursor_now();
    return e;
}

void *CGEventSourceCreate(i32 state) { (void)state; return objc_retain(MSG0(id, (id)cx_class_named("NSObject"), "new")); }
void CGEventSourceSetLocalEventsSuppressionInterval(void *src, double s) { (void)src; (void)s; }
void CGEventSourceSetUserData(void *src, i64 v) { (void)src; (void)v; }
void *CGEventCreate(void *src) { (void)src; return mk(0); }
void *CGEventCreateKeyboardEvent(void *src, u16 key, u8 down) {
    (void)src;
    cgev_t *e = mk(down ? 10 : 11);
    e->key = key; e->down = down;
    return e;
}
void *CGEventCreateMouseEvent(void *src, u32 type, cgpt_t loc, u32 button) {
    (void)src;
    cgev_t *e = mk(type);
    e->loc = loc; e->button = (u8)button; e->clicks = 1;
    return e;
}
void *CGEventCreateScrollWheelEvent2(void *src, i32 units, u32 count, i32 w1, i32 w2, i32 w3) {
    (void)src;
    cgev_t *e = mk(22);
    e->units = units;
    e->wheel1 = w1;
    e->wheel2 = count > 1 ? w2 : 0;
    e->wheel3 = count > 2 ? w3 : 0;
    return e;
}
/* A forma sem o 2 é variádica: (source, units, wheelCount, wheel1, ...). */
void *CGEventCreateScrollWheelEvent(void *src, i32 units, u32 count, ...) {
    __builtin_va_list ap;
    i32 w1 = 0, w2 = 0, w3 = 0;
    __builtin_va_start(ap, count);
    if (count >= 1) w1 = __builtin_va_arg(ap, i32);
    if (count >= 2) w2 = __builtin_va_arg(ap, i32);
    if (count >= 3) w3 = __builtin_va_arg(ap, i32);
    __builtin_va_end(ap);
    return CGEventCreateScrollWheelEvent2(src, units, count, w1, w2, w3);
}
u32 CGEventGetType(void *e) { cgev_t *x = ev(e); return x ? x->type : 0; }
void CGEventSetType(void *e, u32 t) { cgev_t *x = ev(e); if (x) { x->type = t; if (t == 10 || t == 11) x->down = t == 10; } }
u64 CGEventGetFlags(void *e) { cgev_t *x = ev(e); return x ? x->flags : 0; }
void CGEventSetFlags(void *e, u64 f) { cgev_t *x = ev(e); if (x) x->flags = f; }
u64 CGEventGetTimestamp(void *e) { cgev_t *x = ev(e); return x ? x->ts : (u64)SYS0(SYS_TIME) * 1000000ull; }
void CGEventSetTimestamp(void *e, u64 t) { cgev_t *x = ev(e); if (x) x->ts = t; }
cgpt_t CGEventGetLocation(void *e) { cgev_t *x = ev(e); if (x) return x->loc; return cursor_now(); }
void CGEventSetLocation(void *e, cgpt_t p) { cgev_t *x = ev(e); if (x) x->loc = p; }
/* campos (CGEventField): 1 kCGMouseEventClickState, 3 kCGMouseEventButtonNumber, 9 kCGKeyboardEventKeycode,
 * 11/12 kCGScrollWheelEventDeltaAxis1/2, 42 kCGEventSourceUserData, 96/97 PointDelta (pixels) */
i64 CGEventGetIntegerValueField(void *e, u32 f) {
    cgev_t *x = ev(e);
    if (!x) return 0;
    switch (f) {
    case 1: return x->clicks; case 3: return x->button; case 9: return x->key;
    case 11: case 96: return x->wheel1; case 12: case 97: return x->wheel2; case 42: return x->user;
    }
    return 0;
}
double CGEventGetDoubleValueField(void *e, u32 f) {
    cgev_t *x = ev(e);
    if (!x) return 0;
    if (f == 93) return (x->dvalid & 1) ? x->d1 : (double)x->wheel1;
    if (f == 94) return (x->dvalid & 2) ? x->d2 : (double)x->wheel2;
    if (f == 95) return (x->dvalid & 4) ? x->d3 : (double)x->wheel3;
    return (double)CGEventGetIntegerValueField(e, f);
}
void CGEventSetDoubleValueField(void *e, u32 f, double v) {
    cgev_t *x = ev(e);
    if (!x) return;
    if (f == 93) { x->d1 = v; x->dvalid |= 1; }
    else if (f == 94) { x->d2 = v; x->dvalid |= 2; }
    else if (f == 95) { x->d3 = v; x->dvalid |= 4; }
}
void CGEventSetIntegerValueField(void *e, u32 f, i64 v) {
    cgev_t *x = ev(e);
    if (!x) return;
    switch (f) {
    case 1: x->clicks = v; break; case 3: x->button = (u8)v; break; case 9: x->key = (u16)v; break;
    case 11: x->wheel1 = (i32)v; x->units = 1; break; case 12: x->wheel2 = (i32)v; x->units = 1; break;
    case 96: x->wheel1 = (i32)v; x->units = 0; break; case 97: x->wheel2 = (i32)v; x->units = 0; break;
    case 42: x->user = v; break;
    }
}
void CGEventKeyboardSetUnicodeString(void *e, u64 n, const u16 *s) {
    cgev_t *x = ev(e);
    if (!x) return;
    free(x->uni); x->uni = 0; x->nuni = 0;
    if (!n || !s) return;
    x->uni = malloc(n * 2);
    if (!x->uni) return;
    for (u64 i = 0; i < n; i++) x->uni[i] = s[i];
    x->nuni = (u32)n;
}
void CGEventKeyboardGetUnicodeString(void *e, u64 max, u64 *len, u16 *out) {
    cgev_t *x = ev(e);
    u64 n = x ? (x->nuni < max ? x->nuni : max) : 0;
    for (u64 i = 0; i < n; i++) out[i] = x->uni[i];
    if (len) *len = n;
}
u8 CGEventTapIsEnabled(void *tap) { (void)tap; return 0; }

/* código de tecla virtual do Mac -> tecla do mOS (o inverso do mac_keycode do AppKit) */
static int mos_key(u16 mac) {
    static const char map[] = "asdfhgzxcv\0bqweryt123465=97-80]ou[ip\0lj'k;\\,/nm.\0\0`";
    if (mac < sizeof map - 1 && map[mac]) return (u8)map[mac];
    switch (mac) {
    case 36: case 76: return 10; case 48: return 9; case 49: return ' '; case 51: return 8; case 53: return 27;
    case 117: return 0x108; case 126: return 0x100; case 125: return 0x101; case 123: return 0x102; case 124: return 0x103;
    case 115: return 0x104; case 119: return 0x105; case 116: return 0x106; case 121: return 0x107;
    case 122: return 0x10A; case 120: return 0x10B; case 99: return 0x10C; case 118: return 0x10D;
    case 96: return 0x10E; case 97: return 0x10F; case 98: return 0x110; case 100: return 0x111;
    }
    return -1;
}
static u32 mos_mods(u64 f) { return ((f >> 17) & 1 ? 1 : 0) | ((f >> 18) & 1 ? 2 : 0) | ((f >> 19) & 1 ? 4 : 0) | ((f >> 20) & 1 ? 8 : 0); }

enum { K_EV_KEY_DOWN = 1, K_EV_KEY_UP = 2, K_EV_MOUSE_ABS = 4 };     /* input_event_t do kernel (kernel.h) */
static int inject(u32 type, u32 buttons, u32 mods, i32 wheel, i32 key, i32 x, i32 y) {
    u64 b = type | (u64)buttons << 8 | (u64)(mods & 0xFFFF) << 16 | (u64)(u32)wheel << 32;
    u64 d = (u64)(u32)x | (u64)(u32)y << 32;
    return (int)SYS4(GUI_DEVICE, DEV_INPUT_POST, b, (u64)(u32)key, d);
}
/* tipo CG no byte baixo: 1 é mouse-down no CGEvent e key-down na fila global, então o pid tem opcode próprio */
static int inject_pid(i32 pid, u32 cg_type, u32 buttons, u32 mods, i32 wheel, i32 key, i32 x, i32 y) {
    u64 b = cg_type | (u64)buttons << 8 | (u64)(mods & 0xFFFF) << 16 | (u64)(u32)wheel << 32;
    u64 c = (u64)(u32)key | (u64)(u32)pid << 32;
    u64 d = (u64)(u32)x | (u64)(u32)y << 32;
    return (int)SYS4(GUI_DEVICE, DEV_INPUT_TO_PID, b, c, d);
}
static i32 wheel_lines(cgev_t *x) {
    i32 src = x->wheel1;
    if (!src && (x->dvalid & 1)) {
        double v = x->units == 0 ? x->d1 / 10.0 : x->d1;
        if (v > 0 && v < 1) return 1;
        if (v < 0 && v > -1) return -1;
        src = (i32)v;
        if (!src && v) src = v > 0 ? 1 : -1;
        return src;
    }
    i32 d = x->units == 0 ? src / 10 : src;
    if (!d && src) d = src > 0 ? 1 : -1;
    return d;
}

static u32 g_buttons;
static void post(cgev_t *x) {
    if (!x) return;
    u32 t = x->type, mods = mos_mods(x->flags);
    if (t == 10 || t == 11) {
        if (x->nuni) {                                           /* texto: cada caractere como uma tecla (Latin-1) */
            if (t != 10) return;
            for (u32 i = 0; i < x->nuni; i++) {
                u16 ch = x->uni[i];
                int k = ch == '\r' || ch == '\n' ? 10 : ch == '\t' ? 9 : ch < 0x100 ? ch : -1;
                if (k < 0) continue;
                inject(K_EV_KEY_DOWN, g_buttons, mods, 0, k, 0, 0);
                inject(K_EV_KEY_UP, g_buttons, mods, 0, k, 0, 0);
            }
            return;
        }
        int k = mos_key(x->key);
        if (k < 0) return;
        if ((mods & 1) && k >= 'a' && k <= 'z') k -= 32;
        if (inject(t == 10 ? K_EV_KEY_DOWN : K_EV_KEY_UP, g_buttons, mods, 0, k, 0, 0) < 0) cx_post_key_event(x->key, x->flags, t == 10);
        return;
    }
    u32 bit = 0;
    switch (t) {
    case 1: case 2: case 6: bit = 1; break;                      /* esquerdo: down, up, dragged */
    case 3: case 4: case 7: bit = 2; break;                      /* direito */
    case 25: case 26: case 27: bit = 4; break;                   /* outro (meio) */
    }
    if (t == 1 || t == 3 || t == 25) g_buttons |= bit;
    else if (t == 2 || t == 4 || t == 26) g_buttons &= ~bit;
    i32 wheel = 0;
    if (t == 22) {
        wheel = -wheel_lines(x);                                 /* CG: positivo = para cima; kernel: positivo = para baixo */
        if (!wheel) return;
    }
    cgpt_t p = t == 22 ? cursor_now() : x->loc;
    if (t != 22) g_cursor = p;
    inject(K_EV_MOUSE_ABS, g_buttons, mods, wheel, 0, (i32)p.x, (i32)p.y);
}
/* Entrega ao pid. Não mexe em g_cursor, g_buttons nem na fila global. A roda já vai com o sinal do kernel. */
static void post_pid(i32 pid, cgev_t *x) {
    if (!x || pid <= 0) return;
    u32 t = x->type, mods = mos_mods(x->flags);
    if (t == 10 || t == 11) {
        if (x->nuni) {
            if (t != 10) return;
            for (u32 i = 0; i < x->nuni; i++) {
                u16 ch = x->uni[i];
                int k = ch == '\r' || ch == '\n' ? 10 : ch == '\t' ? 9 : ch < 0x100 ? ch : -1;
                if (k < 0) continue;
                inject_pid(pid, 10, 0, mods, 0, k, 0, 0);
                inject_pid(pid, 11, 0, mods, 0, k, 0, 0);
            }
            return;
        }
        int k = mos_key(x->key);
        if (k < 0) return;
        if ((mods & 1) && k >= 'a' && k <= 'z') k -= 32;
        inject_pid(pid, t, 0, mods, 0, k, 0, 0);
        return;
    }
    u32 bit = 0, buttons = 0;
    switch (t) {
    case 1: case 2: case 6: bit = 1; break;
    case 3: case 4: case 7: bit = 2; break;
    case 25: case 26: case 27: bit = 4; break;
    }
    if (t == 1 || t == 3 || t == 25) buttons = bit;
    i32 wheel = 0;
    if (t == 22) {
        wheel = -wheel_lines(x);
        if (!wheel) return;
    }
    cgpt_t p = t == 22 ? cursor_now() : x->loc;
    inject_pid(pid, t, buttons, mods, wheel, 0, (i32)p.x, (i32)p.y);
}
/* kCGHIDEventTap (0), kCGSessionEventTap (1) e kCGAnnotatedSessionEventTap (2) entram na mesma fila.
 * O kernel tem uma fila só: a tecla vai para a janela em foco e o clique para o que está sob o cursor. */
void CGEventPost(u32 tap, void *e) { (void)tap; post(ev(e)); }
void CGEventPostToPid(i32 pid, void *e) { post_pid(pid, ev(e)); }
void CGEventPostToPSN(void *psn, void *e) { (void)psn; post(ev(e)); }
i32 CGWarpMouseCursorPosition(cgpt_t p) { g_cursor = p; inject(K_EV_MOUSE_ABS, g_buttons, 0, 0, 0, (i32)p.x, (i32)p.y); return 0; }

/* ---- captura de tela (GUI_DEVICE/DEV_SCREEN_CAPTURE): a tela composta, com todos os monitores num espaço só como o
 * g_screen do kernel. Pixels só saem se o TCC local concedeu kTCCServiceScreenCapture (tcc.c). A Interface
 * responde o prompt com cx_tcc_decide; o padrão é negado. */
void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*release)(void *, const void *, size_t));
void CGDataProviderRelease(void *p);
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, u32 info,
                    void *provider, const void *decode, u8 interp, int intent);
typedef struct { double x, y, w, h; } cgrect_t;
int cx_tcc_preflight(const char *service);
int cx_tcc_request(const char *service);
static void *capture(i64 x, i64 y, i64 w, i64 h) {
    if (!cx_tcc_preflight("kTCCServiceScreenCapture")) return 0;
    long sz = (long)SYS4(GUI_DEVICE, DEV_SCREEN_CAPTURE, (u64)(u32)x | (u64)(u32)y << 32, (u64)(u32)w | (u64)(u32)h << 32, 0);
    if (sz < 0) return 0;
    u32 cw = (u32)sz, ch = (u32)((u64)sz >> 32);
    if (!cw || !ch) return 0;
    u32 *px = malloc((size_t)cw * ch * 4);
    if (!px) return 0;
    if ((long)SYS4(GUI_DEVICE, DEV_SCREEN_CAPTURE, (u64)(u32)x | (u64)(u32)y << 32, (u64)cw | (u64)ch << 32, px) < 0) { free(px); return 0; }
    void *prov = CGDataProviderCreateWithData(0, px, (size_t)cw * ch * 4, 0);
    /* AARRGGBB em little-endian = kCGImageAlphaNoneSkipFirst (6) | kCGBitmapByteOrder32Little (0x2000) */
    void *img = prov ? CGImageCreate(cw, ch, 8, 32, (size_t)cw * 4, 0, 6 | 0x2000, prov, 0, 0, 0) : 0;
    if (prov) CGDataProviderRelease(prov);
    free(px);                                               /* o CGImageCreate do mOS copia os pixels */
    return img;
}
static int rect_ok(cgrect_t r) { return r.w > 0 && r.h > 0 && r.w < 1e7 && r.h < 1e7 && r.x > -1e7 && r.x < 1e7 && r.y > -1e7 && r.y < 1e7; }
u8 CGPreflightScreenCaptureAccess(void) { return (u8)cx_tcc_preflight("kTCCServiceScreenCapture"); }
u8 CGRequestScreenCaptureAccess(void) { return (u8)cx_tcc_request("kTCCServiceScreenCapture"); }
void *CGDisplayCreateImage(u32 display) { (void)display; return capture(0, 0, 0, 0); }
void *CGDisplayCreateImageForRect(u32 display, cgrect_t r) { (void)display; return rect_ok(r) ? capture((i64)r.x, (i64)r.y, (i64)r.w, (i64)r.h) : capture(0, 0, 0, 0); }
/* Janela pedida: o retângulo dela no compositor (pixels já compostos, inclusive o que estiver por cima).
 * Sem id, o retângulo pedido ou a tela inteira. A permissão continua dentro de capture(). */
static void *capture_window(u32 win, cgrect_t r) {
    gui_cgwindow_t info;
    int found = 0;
    for (int i = 0; i < 64; i++) {
        if (!cx_cg_window_at(i, &info)) break;
        if (info.number == win) { found = 1; break; }
    }
    if (!found || info.w <= 0 || info.h <= 0) return 0;
    i64 x = info.x, y = info.y, w = info.w, h = info.h;
    if (rect_ok(r)) {
        i64 x2 = x + w, y2 = y + h, rx2 = (i64)r.x + (i64)r.w, ry2 = (i64)r.y + (i64)r.h;
        i64 nx = x > (i64)r.x ? x : (i64)r.x;
        i64 ny = y > (i64)r.y ? y : (i64)r.y;
        i64 nx2 = x2 < rx2 ? x2 : rx2;
        i64 ny2 = y2 < ry2 ? y2 : ry2;
        if (nx2 <= nx || ny2 <= ny) return 0;
        x = nx; y = ny; w = nx2 - nx; h = ny2 - ny;
    }
    return capture(x, y, w, h);
}
void *CGWindowListCreateImage(cgrect_t r, u32 opt, u32 win, u32 imgopt) {
    (void)opt; (void)imgopt;
    if (win) return capture_window(win, r);
    return rect_ok(r) ? capture((i64)r.x, (i64)r.y, (i64)r.w, (i64)r.h) : capture(0, 0, 0, 0);
}
