/*
 * mOS — CoreMediaIO (DAL): objeto de sistema, dispositivos e streams.
 * Dispositivos: as câmeras USB do driver UVC (kernel/dev/uvc.c, DEV_CAMERA_*), cada uma com um stream de entrada
 * (formatos 'pfta'/'pft ', taxa 'nfrm'/'nfr#'; quadros em '2vuy' ou '420v' numa thread, na fila do stream), mais o que
 * um plug-in do próprio processo publicou (CMIOObjectCreate + CMIOObjectsPublishedAndDied) ou o que um
 * CMIOExtensionProvider acrescentou. Propriedades: kCMIOHardwarePropertyDevices/DefaultInputDevice/AllowScreenCaptureDevices/
 * AllowWirelessScreenCaptureDevices, nome, classe, dono, objetos, streams do dispositivo, UID, "rodando", direção e
 * formato do stream; ouvintes (proc e bloco) avisados quando um valor muda. Stream: fila de buffers (CMSimpleQueue),
 * start/stop, relógio (CMIOStreamClock*) e CMIOSampleBufferCreateForImageBuffer.
 * CMIOExtension*: os objetos guardam o que o app passa e publicam o dispositivo/stream no registro acima; o valor das
 * chaves CMIOExtensionProperty* é o próprio nome (o da Apple não foi medido). Código original do mOS (MIT).
 */
#include "foundation.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"

typedef int OSStatus;
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
int strcmp(const char *a, const char *b);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
typedef struct { i64 value; i32 timescale; u32 flags; i64 epoch; } cmt;
int CMSimpleQueueCreate(void *a, i32 cap, void **out);
int CMSimpleQueueEnqueue(void *q, const void *e);
int CMAudioClockCreate(void *a, void **out);
int CMSampleBufferCreateForImageBuffer(void *a, void *img, u8 ready, void *cb, void *ref, void *fd, const void *timing, void **out);
#define FCC(a, b, c, d) ((u32)(a) << 24 | (u32)(b) << 16 | (u32)(c) << 8 | (u32)(d))
#define BLK_FN(b) (((void **)(b))[2])
#define E_BADOBJ FCC('!', 'o', 'b', 'j')
#define E_UNKNOWN FCC('w', 'h', 'o', '?')
#define E_SIZE FCC('!', 's', 'i', 'z')
#define E_ILLEGAL FCC('n', 'o', 'p', 'e')
#define E_UNSUP FCC('u', 'n', 'o', 'p')
#define SYSOBJ 1u
enum { C_SYS = FCC('a', 's', 'y', 's'), C_DEV = FCC('a', 'd', 'e', 'v'), C_STREAM = FCC('a', 's', 't', 'r') };
typedef struct { u32 sel, scope, elem; } paddr;

struct hwcam;
typedef struct cobj {
    u32 id, cls, owner;
    int published, running, direction;
    id name, uid;
    void *fd;              /* CMFormatDescription do stream */
    void *queue;           /* CMSimpleQueue do stream */
    void (*altered)(u32, void *, void *); void *alt_ref;
    struct hwcam *hw;      /* câmera do kernel (dispositivo e stream) */
    struct cobj *next;
} cobj;
typedef struct lis { u32 obj; paddr a; void *proc, *ref, *block, *queue; struct lis *next; } lis;
static cobj *g_objs;
static lis *g_lis;
static u32 g_next = 0x100;
static u32 g_allow_screen, g_allow_wireless;
static cobj *find(u32 id) { for (cobj *o = g_objs; o; o = o->next) if (o->id == id) return o; return 0; }
static void hw_refresh(void);
static OSStatus hw_prop(cobj *o, u32 sel, u32 *size, void *data, u32 cap);
static OSStatus hw_set(cobj *o, u32 sel, u32 size, const void *data);
static OSStatus hw_start(cobj *s);
static void hw_stop(cobj *s);

static void notify(u32 obj, u32 sel) {
    paddr a = {sel, FCC('g', 'l', 'o', 'b'), 0};
    for (lis *l = g_lis; l; l = l->next) {
        if (l->obj != obj || (l->a.sel != sel && l->a.sel != FCC('*', '*', '*', '*'))) continue;
        if (l->proc) ((OSStatus (*)(u32, u32, const paddr *, void *))l->proc)(obj, 1, &a, l->ref);
        else if (l->block) ((void (*)(void *, u32, const paddr *))BLK_FN(l->block))(l->block, 1, &a);
    }
}
/* ids publicados de uma classe e de um dono (0 = qualquer) */
static u32 list_ids(u32 cls, u32 owner, u32 *out, u32 max) {
    u32 n = 0;
    for (cobj *o = g_objs; o; o = o->next) if (o->published && o->cls == cls && (!owner || o->owner == owner)) { if (out && n < max) out[n] = o->id; n++; }
    return n;
}
/* tamanho e valor de uma propriedade; data NULL só mede */
static OSStatus prop(u32 obj, const paddr *a, u32 *size, void *data, u32 cap) {
    if (!a) return E_ILLEGAL;
    u32 v32 = 0, need = 4, nl = 0;
    id idv = 0;
    u32 list[64];
    int is_list = 0, is_id = 0;
    if (obj == SYSOBJ) {
        if (a->sel == FCC('d', 'e', 'v', '#') || a->sel == FCC('d', 'I', 'n', ' ') || a->sel == FCC('o', 'w', 'n', 'd')) hw_refresh();
        switch (a->sel) {
        case FCC('d', 'e', 'v', '#'): nl = list_ids(C_DEV, 0, list, 64); is_list = 1; break;
        case FCC('d', 'I', 'n', ' '): { u32 n = list_ids(C_DEV, 0, list, 1); v32 = n ? list[0] : 0; break; }
        case FCC('y', 'e', 's', ' '): v32 = g_allow_screen; break;
        case FCC('w', 's', 'c', 'd'): v32 = g_allow_wireless; break;
        case FCC('c', 'l', 'a', 's'): v32 = C_SYS; break;
        case FCC('o', 'w', 'n', 'd'): nl = list_ids(C_DEV, 0, list, 64); is_list = 1; break;
        default: return E_UNKNOWN;
        }
    } else {
        cobj *o = find(obj);
        if (!o || !o->published) return E_BADOBJ;
        if (o->hw) { OSStatus r = hw_prop(o, a->sel, size, data, cap); if (r != E_UNKNOWN) return r; }
        switch (a->sel) {
        case FCC('c', 'l', 'a', 's'): v32 = o->cls; break;
        case FCC('s', 't', 'd', 'v'): v32 = o->owner ? o->owner : SYSOBJ; break;
        case FCC('l', 'n', 'a', 'm'): idv = o->name; is_id = 1; break;
        case FCC('o', 'w', 'n', 'd'): case FCC('s', 't', 'm', '#'):
            if (o->cls != C_DEV && a->sel == FCC('s', 't', 'm', '#')) return E_UNKNOWN;
            nl = list_ids(C_STREAM, o->id, list, 64); is_list = 1; break;
        case FCC('u', 'i', 'd', ' '): if (o->cls != C_DEV) return E_UNKNOWN; idv = o->uid; is_id = 1; break;
        case FCC('g', 'o', 'n', 'e'): if (o->cls != C_DEV) return E_UNKNOWN; v32 = 0; for (cobj *s = g_objs; s; s = s->next) if (s->owner == o->id && s->running) v32 = 1; break;
        case FCC('s', 'd', 'i', 'r'): if (o->cls != C_STREAM) return E_UNKNOWN; v32 = (u32)o->direction; break;
        case FCC('p', 'f', 't', ' '): if (o->cls != C_STREAM) return E_UNKNOWN; idv = (id)o->fd; is_id = 1; break;
        default: return E_UNKNOWN;
        }
    }
    if (is_list) need = nl * 4;
    else if (is_id) need = sizeof(id);
    if (!data) { *size = need; return 0; }
    if (cap < need && !is_list) return E_SIZE;
    if (is_list) { u32 k = cap / 4 < nl ? cap / 4 : nl; memcpy(data, list, k * 4); *size = k * 4; return 0; }
    if (is_id) { if (idv) objc_retain(idv); memcpy(data, &idv, sizeof idv); }
    else memcpy(data, &v32, 4);
    *size = need;
    return 0;
}
OSStatus CMIOObjectGetPropertyDataSize(u32 obj, const paddr *a, u32 qs, const void *q, u32 *size) { (void)qs; (void)q; if (!size) return E_ILLEGAL; return prop(obj, a, size, 0, 0); }
OSStatus CMIOObjectGetPropertyData(u32 obj, const paddr *a, u32 qs, const void *q, u32 sz, u32 *used, void *data) {
    (void)qs; (void)q;
    u32 got = 0;
    OSStatus rc = prop(obj, a, &got, data, sz);
    if (used) *used = rc ? 0 : got;
    return rc;
}
u8 CMIOObjectHasProperty(u32 obj, const paddr *a) { u32 s; return prop(obj, a, &s, 0, 0) == 0; }
OSStatus CMIOObjectIsPropertySettable(u32 obj, const paddr *a, u8 *out) {
    u32 s;
    OSStatus rc = prop(obj, a, &s, 0, 0);
    cobj *o = obj == SYSOBJ ? 0 : find(obj);
    if (out) *out = !rc && ((obj == SYSOBJ && (a->sel == FCC('y', 'e', 's', ' ') || a->sel == FCC('w', 's', 'c', 'd'))) ||
                            (o && o->hw && o->cls == C_STREAM && (a->sel == FCC('p', 'f', 't', ' ') || a->sel == FCC('n', 'f', 'r', 'm'))));
    return rc;
}
OSStatus CMIOObjectSetPropertyData(u32 obj, const paddr *a, u32 qs, const void *q, u32 size, const void *data) {
    (void)qs; (void)q;
    if (!a || !data) return E_ILLEGAL;
    if (obj == SYSOBJ && (a->sel == FCC('y', 'e', 's', ' ') || a->sel == FCC('w', 's', 'c', 'd'))) {
        if (size < 4) return E_SIZE;
        u32 v; memcpy(&v, data, 4);
        u32 *slot = a->sel == FCC('y', 'e', 's', ' ') ? &g_allow_screen : &g_allow_wireless;
        if (*slot != (v != 0)) { *slot = v != 0; notify(SYSOBJ, a->sel); }
        return 0;
    }
    cobj *o = find(obj);
    if (o && o->hw && o->published && (a->sel == FCC('p', 'f', 't', ' ') || a->sel == FCC('n', 'f', 'r', 'm'))) return hw_set(o, a->sel, size, data);
    u32 s;
    OSStatus rc = prop(obj, a, &s, 0, 0);
    return rc ? rc : E_UNSUP;   /* existe, mas só se lê */
}
static OSStatus add_lis(u32 obj, const paddr *a, void *proc, void *ref, void *block) {
    if (!a || (!proc && !block)) return E_ILLEGAL;
    if (obj != SYSOBJ && !find(obj)) return E_BADOBJ;
    lis *l = calloc(1, sizeof *l);
    if (!l) return -108;
    l->obj = obj; l->a = *a; l->proc = proc; l->ref = ref; l->block = block ? _Block_copy(block) : 0;
    l->next = g_lis; g_lis = l;
    return 0;
}
static OSStatus rm_lis(u32 obj, const paddr *a, void *proc, void *ref, void *block) {
    for (lis **pp = &g_lis; *pp; pp = &(*pp)->next) {
        lis *l = *pp;
        if (l->obj == obj && a && l->a.sel == a->sel && ((proc && l->proc == proc && l->ref == ref) || (block && l->block == block))) {
            *pp = l->next;
            if (l->block) _Block_release(l->block);
            free(l);
            return 0;
        }
    }
    return E_ILLEGAL;
}
OSStatus CMIOObjectAddPropertyListener(u32 obj, const paddr *a, void *proc, void *ref) { return add_lis(obj, a, proc, ref, 0); }
OSStatus CMIOObjectRemovePropertyListener(u32 obj, const paddr *a, void *proc, void *ref) { return rm_lis(obj, a, proc, ref, 0); }
/* o bloco guardado é a cópia: a remoção compara o bloco que o app passou de novo, como a Apple (mesmo ponteiro) */
OSStatus CMIOObjectAddPropertyListenerBlock(u32 obj, const paddr *a, void *queue, void *block) { (void)queue; return add_lis(obj, a, 0, 0, block); }
OSStatus CMIOObjectRemovePropertyListenerBlock(u32 obj, const paddr *a, void *queue, void *block) { (void)queue; return rm_lis(obj, a, 0, 0, block); }
void CMIOObjectShow(u32 obj) { (void)obj; }

/* ---------------------------------------------------------------- plug-in DAL do próprio processo */
OSStatus CMIOObjectCreate(void *plugin, u32 owner, u32 cls, u32 *out) {
    (void)plugin;
    if (!out || (cls != C_DEV && cls != C_STREAM)) return E_ILLEGAL;
    cobj *o = calloc(1, sizeof *o);
    if (!o) return -108;
    o->id = g_next++; o->cls = cls; o->owner = owner == SYSOBJ ? 0 : owner;
    o->next = g_objs; g_objs = o;
    *out = o->id;
    return 0;
}
OSStatus CMIOObjectsPublishedAndDied(void *plugin, u32 owner, u32 np, const u32 *pub, u32 nd, const u32 *died) {
    (void)plugin;
    int devs = 0;
    for (u32 i = 0; i < np; i++) { cobj *o = find(pub[i]); if (!o) return E_BADOBJ; o->published = 1; if (o->cls == C_DEV) devs = 1; }
    for (u32 i = 0; i < nd; i++) {
        for (cobj **pp = &g_objs; *pp; pp = &(*pp)->next)
            if ((*pp)->id == died[i]) { cobj *o = *pp; *pp = o->next; if (o->cls == C_DEV) devs = 1; if (o->name) objc_release(o->name); if (o->uid) objc_release(o->uid); free(o); break; }
    }
    if (devs) notify(SYSOBJ, FCC('d', 'e', 'v', '#'));
    if (owner && owner != SYSOBJ) notify(owner, FCC('s', 't', 'm', '#'));
    return 0;
}
/* nome/UID/formato vindos do plug-in ou da extensão */
void cx_cmio_set_info(u32 obj, id name, id uid, void *fd, int direction) {
    cobj *o = find(obj);
    if (!o) return;
    if (name) { if (o->name) objc_release(o->name); o->name = objc_retain(name); }
    if (uid) { if (o->uid) objc_release(o->uid); o->uid = objc_retain(uid); }
    if (fd) o->fd = fd;
    o->direction = direction;
}
OSStatus CMIOStreamCopyBufferQueue(u32 stream, void *altered, void *ref, void **queue) {
    cobj *o = find(stream);
    if (!o || o->cls != C_STREAM || !o->published) return E_BADOBJ;
    if (!queue) return E_ILLEGAL;
    if (!o->queue && CMSimpleQueueCreate(0, 30, &o->queue)) return -108;
    o->altered = altered; o->alt_ref = ref;
    *queue = o->queue;
    return 0;
}
OSStatus CMIODeviceStartStream(u32 dev, u32 stream) {
    cobj *d = find(dev), *s = find(stream);
    if (!d || !s || d->cls != C_DEV || s->owner != dev) return E_BADOBJ;
    if (!s->running) {
        if (s->hw) { OSStatus r = hw_start(s); if (r) return r; }
        s->running = 1;
        notify(dev, FCC('g', 'o', 'n', 'e'));
    }
    return 0;
}
OSStatus CMIODeviceStopStream(u32 dev, u32 stream) {
    cobj *d = find(dev), *s = find(stream);
    if (!d || !s || d->cls != C_DEV || s->owner != dev) return E_BADOBJ;
    if (s->running) {
        s->running = 0;
        if (s->hw) hw_stop(s);
        notify(dev, FCC('g', 'o', 'n', 'e'));
    }
    return 0;
}
/* entrega de um quadro ao consumidor do stream (plug-in/extensão): entra na fila e avisa */
int cx_cmio_stream_push(u32 stream, void *sbuf) {
    cobj *o = find(stream);
    if (!o || !o->queue || !o->running) return -1;
    if (CMSimpleQueueEnqueue(o->queue, objc_retain((id)sbuf))) { objc_release((id)sbuf); return -1; }
    if (o->altered) o->altered(stream, sbuf, o->alt_ref);
    return 0;
}

/* ---------------------------------------------------------------- câmeras USB (DEV_CAMERA_*) */
int CMVideoFormatDescriptionCreate(void *a, u32 codec, i32 w, i32 h, id ext, void **out);
typedef struct { cmt duration, pts, dts; } cmtiming;
cmt CMClockGetTime(void *clock);
void *CMClockGetHostTimeClock(void);
void *cx_avcap_frame_pb(u32 fourcc, const u8 *data, u32 len, int w, int h, u32 fmt);
void *malloc(size_t n);
int snprintf(char *s, size_t n, const char *f, ...);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int usleep(unsigned us);
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *p);
OSStatus CMIOSampleBufferCreateForImageBuffer(void *a, void *img, void *fd, const void *timing, u64 seq, u32 disc, void **out);
#define DEV_OP(op, a, b, c) ((long)SYS4(79 /* GUI_DEVICE */, (op), (a), (b), (c)))
typedef struct hwcam {
    int cam, alive;
    char key[48];
    u32 dev, stream;
    gui_camera_info_t info;
    void *fds[GUI_CAMERA_MODES];
    int cur;
    u32 interval;
    volatile int run, done;
    unsigned long th;
    u8 *buf; u32 cap;
    u64 seq;
    gui_camera_info_t open;
    struct hwcam *next;
} hwcam;
static hwcam *g_hw;
static u32 out_fmt(u32 fourcc) { return fourcc == 0x3231564eu /* NV12 */ ? FCC('4', '2', '0', 'v') : FCC('2', 'v', 'u', 'y'); }
static void hw_publish(hwcam *h) {
    cobj *d = calloc(1, sizeof *d), *st = calloc(1, sizeof *st);
    if (!d || !st) { free(d); free(st); return; }
    d->id = g_next++; d->cls = C_DEV; d->published = 1; d->hw = h;
    st->id = g_next++; st->cls = C_STREAM; st->owner = d->id; st->published = 1; st->direction = 1; st->hw = h;
    d->name = objc_retain(cx_str(h->info.name));
    d->uid = objc_retain(cx_str(h->key));
    st->name = objc_retain(cx_str(h->info.name));
    for (int i = 0; i < h->info.nmode; i++)
        CMVideoFormatDescriptionCreate(0, out_fmt(h->info.mode[i].fourcc), h->info.mode[i].width, h->info.mode[i].height, 0, &h->fds[i]);
    h->cur = 0;
    for (int i = 0; i < h->info.nmode; i++) if (h->info.mode[i].width == 1280 && h->info.mode[i].height == 720) { h->cur = i; break; }
    h->interval = h->info.nmode ? h->info.mode[h->cur].def_interval : 333333;
    st->fd = h->info.nmode ? h->fds[h->cur] : 0;
    h->dev = d->id; h->stream = st->id;
    st->next = g_objs; d->next = st; g_objs = d;
}
static void hw_unpublish(hwcam *h) {
    for (cobj **pp = &g_objs; *pp;) {
        cobj *o = *pp;
        if (o->hw == h) {
            if (o->cls == C_STREAM && o->running) { o->running = 0; hw_stop(o); }
            *pp = o->next;
            if (o->name) objc_release(o->name);
            if (o->uid) objc_release(o->uid);
            free(o);
        } else pp = &o->next;
    }
}
/* a lista acompanha o que o kernel tem agora; câmera nova ou que saiu avisa o ouvinte de 'dev#' */
static void hw_refresh(void) {
    gui_camera_info_t *info = calloc(1, sizeof *info);
    if (!info) return;
    for (hwcam *h = g_hw; h; h = h->next) h->alive = 0;
    int changed = 0;
    for (int k = 0; k < 8 && DEV_OP(DEV_CAMERA_INFO, k, info, 0) == 1; k++) {
        char key[48];
        snprintf(key, sizeof key, "uvc-%s-%04x%04x", info->path, info->vid, info->pid);
        hwcam *h = g_hw;
        while (h && strcmp(h->key, key)) h = h->next;
        if (!h) {
            h = calloc(1, sizeof *h);
            if (!h) break;
            memcpy(h->key, key, sizeof key);
            h->info = *info;
            h->next = g_hw; g_hw = h;
            hw_publish(h);
            changed = 1;
        }
        h->cam = k;
        h->alive = 1;
    }
    free(info);
    for (hwcam **pp = &g_hw; *pp;) {
        hwcam *h = *pp;
        if (!h->alive && !h->run) { hw_unpublish(h); *pp = h->next; free(h->buf); free(h); changed = 1; }
        else pp = &h->next;
    }
    if (changed) notify(SYSOBJ, FCC('d', 'e', 'v', '#'));
}
static double rate_of(u32 iv) { return iv ? 10000000.0 / iv : 0; }
static OSStatus hw_prop(cobj *o, u32 sel, u32 *size, void *data, u32 cap) {
    hwcam *h = o->hw;
    if (o->cls == C_DEV) {
        u32 v;
        switch (sel) {
        case FCC('t', 'r', 'a', 'n'): v = FCC('u', 's', 'b', ' '); break;
        case FCC('m', 'u', 'i', 'd'): {
            char m[64];
            snprintf(m, sizeof m, "UVC Camera VendorID_%u ProductID_%u", h->info.vid, h->info.pid);
            if (!data) { *size = sizeof(id); return 0; }
            if (cap < sizeof(id)) return E_SIZE;
            id s = objc_retain(cx_str(m));
            memcpy(data, &s, sizeof s); *size = sizeof s;
            return 0;
        }
        default: return E_UNKNOWN;
        }
        if (!data) { *size = 4; return 0; }
        if (cap < 4) return E_SIZE;
        memcpy(data, &v, 4); *size = 4;
        return 0;
    }
    switch (sel) {
    case FCC('p', 'f', 't', 'a'): {   /* CFArray com um CMFormatDescription por modo */
        if (!data) { *size = sizeof(id); return 0; }
        if (cap < sizeof(id)) return E_SIZE;
        id a = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
        for (int i = 0; i < h->info.nmode; i++) if (h->fds[i]) MSG(void, a, "addObject:", (id)h->fds[i]);
        objc_retain(a);
        memcpy(data, &a, sizeof a); *size = sizeof a;
        return 0;
    }
    case FCC('n', 'f', 'r', 'm'): {
        double r = rate_of(h->interval);
        if (!data) { *size = 8; return 0; }
        if (cap < 8) return E_SIZE;
        memcpy(data, &r, 8); *size = 8;
        return 0;
    }
    case FCC('n', 'f', 'r', '#'): {   /* taxas do modo atual: a mínima e a máxima (e a padrão, se no meio) */
        gui_camera_mode_t *m = &h->info.mode[h->cur];
        double r[3]; u32 n = 0;
        r[n++] = rate_of(m->min_interval ? m->min_interval : m->def_interval);
        if (m->def_interval != m->min_interval && m->def_interval != m->max_interval) r[n++] = rate_of(m->def_interval);
        if (m->max_interval && m->max_interval != m->min_interval) r[n++] = rate_of(m->max_interval);
        if (!data) { *size = n * 8; return 0; }
        u32 k = cap / 8 < n ? cap / 8 : n;
        memcpy(data, r, k * 8); *size = k * 8;
        return 0;
    }
    }
    return E_UNKNOWN;
}
static OSStatus hw_set(cobj *o, u32 sel, u32 size, const void *data) {
    hwcam *h = o->hw;
    if (o->cls != C_STREAM) return E_UNSUP;
    if (h->run) return FCC('n', 'o', 't', 'r');   /* kCMIOHardwareNotStoppedError: troca com o stream parado */
    if (sel == FCC('p', 'f', 't', ' ')) {
        if (size < sizeof(void *)) return E_SIZE;
        void *fd; memcpy(&fd, data, sizeof fd);
        for (int i = 0; i < h->info.nmode; i++) if (h->fds[i] == fd) {
            h->cur = i; h->interval = h->info.mode[i].def_interval; o->fd = fd;
            notify(o->id, sel); notify(o->id, FCC('n', 'f', 'r', 'm'));
            return 0;
        }
        return FCC('f', 'm', 't', '?');   /* kCMIOHardwareUnsupportedFormatError */
    }
    if (size < 8) return E_SIZE;
    double r; memcpy(&r, data, 8);
    gui_camera_mode_t *m = &h->info.mode[h->cur];
    if (r <= 0) return E_ILLEGAL;
    u32 iv = (u32)(10000000.0 / r + 0.5);
    if (m->min_interval && iv < m->min_interval) iv = m->min_interval;
    if (m->max_interval && iv > m->max_interval) iv = m->max_interval;
    h->interval = iv;
    notify(o->id, sel);
    return 0;
}
static void *hw_thread(void *a) {
    cobj *st = a;
    hwcam *h = st->hw;
    u32 fmt = out_fmt(h->open.cur.fourcc);
    void *fd = 0;
    CMVideoFormatDescriptionCreate(0, fmt, h->open.cur.width, h->open.cur.height, 0, &fd);
    while (h->run) {
        long n = DEV_OP(DEV_CAMERA_READ, h->cam, h->buf, h->cap);
        if (n < 0) break;   /* desligada: o refresh tira da lista */
        if (!n) { usleep(3000); continue; }
        void *pool = objc_autoreleasePoolPush();
        void *pb = cx_avcap_frame_pb(h->open.cur.fourcc, h->buf, (u32)n, h->open.cur.width, h->open.cur.height, fmt);
        if (pb) {
            cmt now = CMClockGetTime(CMClockGetHostTimeClock());
            cmtiming t = {{h->open.cur_interval ? h->open.cur_interval : 333333, 10000000, 1, 0}, now, {0, 0, 0, 0}};
            void *sb = 0;
            if (fd && !CMIOSampleBufferCreateForImageBuffer(0, pb, fd, &t, h->seq++, 0, &sb) && sb) {
                cx_cmio_stream_push(st->id, sb);
                objc_release((id)sb);
            }
            objc_release((id)pb);
        }
        objc_autoreleasePoolPop(pool);
    }
    if (fd) objc_release((id)fd);
    DEV_OP(DEV_CAMERA_CLOSE, h->cam, 0, 0);
    h->done = 1;
    return 0;
}
static OSStatus hw_start(cobj *s) {
    hwcam *h = s->hw;
    if (!h->info.nmode) return E_ILLEGAL;
    gui_camera_mode_t *m = &h->info.mode[h->cur];
    if (DEV_OP(DEV_CAMERA_OPEN, h->cam, (u64)m->width | (u64)m->height << 32, (u64)m->fourcc | (u64)h->interval << 32) < 0)
        return FCC('w', 'h', 'a', 't');   /* kCMIOHardwareUnspecifiedError: outra captura segura a câmera */
    if (DEV_OP(DEV_CAMERA_INFO, h->cam, &h->open, 0) != 1) { DEV_OP(DEV_CAMERA_CLOSE, h->cam, 0, 0); return E_ILLEGAL; }
    u32 need = h->open.cur.max_size, raw = (u32)h->open.cur.width * h->open.cur.height * 2;
    if (need < raw) need = raw;
    need += 65536;
    if (need > h->cap) { free(h->buf); h->buf = malloc(need); h->cap = h->buf ? need : 0; }
    if (!h->buf) { DEV_OP(DEV_CAMERA_CLOSE, h->cam, 0, 0); return -108; }
    h->run = 1; h->done = 0;
    if (pthread_create(&h->th, 0, hw_thread, s)) { h->run = 0; DEV_OP(DEV_CAMERA_CLOSE, h->cam, 0, 0); return -108; }
    return 0;
}
static void hw_stop(cobj *s) {
    hwcam *h = s->hw;
    if (!h->run) return;
    h->run = 0;
    for (int i = 0; i < 500 && !h->done; i++) usleep(1000);
}

/* ---------------------------------------------------------------- relógio do stream e sample buffer */
OSStatus CMIOStreamClockCreate(void *a, id name, const void *src, cmt min_interval, u32 nev, u32 navg, void **clock) {
    (void)name; (void)src; (void)min_interval; (void)nev; (void)navg;
    return CMAudioClockCreate(a, clock);   /* anda no relógio do host, ancorado pelos eventos */
}
OSStatus CMIOStreamClockPostTimingEvent(cmt event, u64 host, u8 resync, void *clock) { (void)event; (void)host; (void)resync; return clock ? 0 : E_ILLEGAL; }
OSStatus CMIOStreamClockInvalidate(void *clock) { return clock ? 0 : E_ILLEGAL; }
OSStatus CMIOSampleBufferCreateForImageBuffer(void *a, void *img, void *fd, const void *timing, u64 seq, u32 disc, void **out) {
    (void)seq; (void)disc;
    if (!img || !fd || !timing || !out) return E_ILLEGAL;
    return CMSampleBufferCreateForImageBuffer(a, img, 1, 0, 0, fd, timing, out);
}

/* ---------------------------------------------------------------- CMIOExtension* (modelo de dados + publicação) */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cmc_t;
#define SAME(var) static cmc_t var##_s = {cx_cfconst_isa, 0x7c8, #var, sizeof(#var) - 1}; const void *var = &var##_s;
SAME(CMIOExtensionPropertyDeviceModel) SAME(CMIOExtensionPropertyDeviceTransportType) SAME(CMIOExtensionPropertyProviderManufacturer)
SAME(CMIOExtensionPropertyProviderName) SAME(CMIOExtensionPropertyStreamActiveFormatIndex) SAME(CMIOExtensionPropertyStreamFrameDuration)
SAME(CMIOExtensionPropertyStreamSinkBufferQueueSize) SAME(CMIOExtensionPropertyStreamSinkBufferUnderrunCount)
SAME(CMIOExtensionPropertyStreamSinkBuffersRequiredForStartup) SAME(CMIOExtensionPropertyStreamSinkEndOfData)
typedef struct { cx_class_t *isa; id dict; } xprops_t;   /* *Properties: dicionário de propriedade -> valor */
typedef struct { cx_class_t *isa; id name, uid, model, source, items; u32 cmio; i64 direction; void *fd; id formats; } xobj_t;
#define XP(o) ((xprops_t *)(o))
#define XO(o) ((xobj_t *)(o))
cx_class_t CMIOExtensionProvider_c __asm__("OBJC_CLASS_$_CMIOExtensionProvider"), CMIOExtensionProvider_m __asm__("OBJC_METACLASS_$_CMIOExtensionProvider");
cx_class_t CMIOExtensionDevice_c __asm__("OBJC_CLASS_$_CMIOExtensionDevice"), CMIOExtensionDevice_m __asm__("OBJC_METACLASS_$_CMIOExtensionDevice");
cx_class_t CMIOExtensionStream_c __asm__("OBJC_CLASS_$_CMIOExtensionStream"), CMIOExtensionStream_m __asm__("OBJC_METACLASS_$_CMIOExtensionStream");
cx_class_t CMIOExtensionStreamFormat_c __asm__("OBJC_CLASS_$_CMIOExtensionStreamFormat"), CMIOExtensionStreamFormat_m __asm__("OBJC_METACLASS_$_CMIOExtensionStreamFormat");
cx_class_t CMIOExtensionProviderProperties_c __asm__("OBJC_CLASS_$_CMIOExtensionProviderProperties"), CMIOExtensionProviderProperties_m __asm__("OBJC_METACLASS_$_CMIOExtensionProviderProperties");
cx_class_t CMIOExtensionDeviceProperties_c __asm__("OBJC_CLASS_$_CMIOExtensionDeviceProperties"), CMIOExtensionDeviceProperties_m __asm__("OBJC_METACLASS_$_CMIOExtensionDeviceProperties");
cx_class_t CMIOExtensionStreamProperties_c __asm__("OBJC_CLASS_$_CMIOExtensionStreamProperties"), CMIOExtensionStreamProperties_m __asm__("OBJC_METACLASS_$_CMIOExtensionStreamProperties");
cx_class_t CMIOExtensionScheduledOutput_c __asm__("OBJC_CLASS_$_CMIOExtensionScheduledOutput"), CMIOExtensionScheduledOutput_m __asm__("OBJC_METACLASS_$_CMIOExtensionScheduledOutput");
static id cls(const char *n) { return (id)cx_class_named(n); }
static void setobj(id *s, id v) { if (*s == v) return; if (v) objc_retain(v); if (*s) objc_release(*s); *s = v; }
static id props_with(id k, SEL c, id dict) {   /* mutableCopy já vem com +1 */
    (void)c;
    id o = cx_alloc_instance((cx_class_t *)k);
    XP(o)->dict = dict ? MSG0(id, dict, "mutableCopy") : objc_retain(MSG0(id, cls("NSMutableDictionary"), "dictionary"));
    return objc_autorelease(o);
}
static id props_dict(id s, SEL c) { (void)c; return XP(s)->dict; }
static void props_set_dict(id s, SEL c, id d) { (void)c; id cp = d ? MSG0(id, d, "mutableCopy") : 0; setobj(&XP(s)->dict, cp); if (cp) objc_release(cp); }
static void props_set_value(id s, SEL c, id v, id key) { (void)c; if (!XP(s)->dict) XP(s)->dict = objc_retain(MSG0(id, cls("NSMutableDictionary"), "dictionary")); if (v) MSG(void, XP(s)->dict, "setObject:forKey:", v, key); else MSG(void, XP(s)->dict, "removeObjectForKey:", key); }
static id items_of(id s) { if (!XO(s)->items) XO(s)->items = objc_retain(MSG0(id, cls("NSMutableArray"), "array")); return XO(s)->items; }
static id prov_init(id s, SEL c, id source, i64 cq) { (void)c; (void)cq; setobj(&XO(s)->source, source); return s; }
static id prov_devices(id s, SEL c) { (void)c; return items_of(s); }
static id prov_source(id s, SEL c) { (void)c; return XO(s)->source; }
/* addDevice: publica o dispositivo e os streams dele no registro do CMIO */
static BOOL prov_add(id s, SEL c, id dev, id *err) {
    (void)c;
    if (err) *err = 0;
    MSG(void, items_of(s), "addObject:", dev);
    u32 d = 0;
    if (CMIOObjectCreate(0, SYSOBJ, C_DEV, &d)) return NO;
    XO(dev)->cmio = d;
    cx_cmio_set_info(d, XO(dev)->name, XO(dev)->uid, 0, 0);
    u32 pub[33]; u32 np = 0;
    pub[np++] = d;
    id streams = items_of(dev);
    for (u64 i = 0, n = MSG0(u64, streams, "count"); i < n && np < 33; i++) {
        id st = MSG(id, streams, "objectAtIndex:", i);
        u32 sid = 0;
        if (CMIOObjectCreate(0, d, C_STREAM, &sid)) continue;
        XO(st)->cmio = sid;
        cx_cmio_set_info(sid, XO(st)->name, XO(st)->uid, XO(st)->fd, (int)XO(st)->direction);
        pub[np++] = sid;
    }
    return CMIOObjectsPublishedAndDied(0, SYSOBJ, np, pub, 0, 0) == 0;
}
static BOOL prov_remove(id s, SEL c, id dev, id *err) {
    (void)c;
    if (err) *err = 0;
    u32 died[33]; u32 nd = 0;
    id streams = items_of(dev);
    for (u64 i = 0, n = MSG0(u64, streams, "count"); i < n && nd < 32; i++) { u32 sid = XO(MSG(id, streams, "objectAtIndex:", i))->cmio; if (sid) died[nd++] = sid; }
    if (XO(dev)->cmio) died[nd++] = XO(dev)->cmio;
    MSG(void, items_of(s), "removeObject:", dev);
    return CMIOObjectsPublishedAndDied(0, SYSOBJ, 0, 0, nd, died) == 0;
}
static id dev_init(id s, SEL c, id name, id uid, id legacy, id source) { (void)c; (void)legacy; setobj(&XO(s)->name, name); setobj(&XO(s)->uid, uid); setobj(&XO(s)->source, source); return s; }
static id dev_name(id s, SEL c) { (void)c; return XO(s)->name; }
static id dev_uid(id s, SEL c) { (void)c; return XO(s)->uid; }
static id dev_streams(id s, SEL c) { (void)c; return items_of(s); }
static BOOL dev_add_stream(id s, SEL c, id st, id *err) { (void)c; if (err) *err = 0; MSG(void, items_of(s), "addObject:", st); return YES; }
static id dev_source(id s, SEL c) { (void)c; return XO(s)->source; }
static id st_init(id s, SEL c, id name, id uid, i64 dir, i64 clock_type, id source) {
    (void)c; (void)clock_type;
    setobj(&XO(s)->name, name); setobj(&XO(s)->uid, uid); setobj(&XO(s)->source, source);
    XO(s)->direction = dir;
    id fmts = source ? MSG0(id, source, "formats") : 0;   /* o formato ativo é o primeiro que a fonte oferece */
    if (fmts && MSG0(u64, fmts, "count")) XO(s)->fd = MSG0(void *, MSG(id, fmts, "objectAtIndex:", (u64)0), "formatDescription");
    return s;
}
static id st_name(id s, SEL c) { (void)c; return XO(s)->name; }
static i64 st_direction(id s, SEL c) { (void)c; return XO(s)->direction; }
static id st_source(id s, SEL c) { (void)c; return XO(s)->source; }
/* sendSampleBuffer: entra na fila do stream publicado (se alguém está consumindo) */
static void st_send(id s, SEL c, void *sb, u64 discontinuity, u64 host) { (void)c; (void)discontinuity; (void)host; if (XO(s)->cmio) cx_cmio_stream_push(XO(s)->cmio, sb); }
static id fmt_init(id s, SEL c, void *fd, cmt maxd, cmt mind, id valid) { (void)c; (void)maxd; (void)mind; (void)valid; XO(s)->fd = fd; if (fd) objc_retain((id)fd); return s; }
static void *fmt_fd(id s, SEL c) { (void)c; return XO(s)->fd; }
static id sched_init(id s, SEL c, u64 seq, u64 host) { (void)c; (void)seq; (void)host; return s; }

void cx_cmio_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
#define DEF(k, n, sz) if (!cx_class_named(n)) cx_define_class(&k##_c, &k##_m, n, ob, (u32)(sz))
    DEF(CMIOExtensionProvider, "CMIOExtensionProvider", sizeof(xobj_t) - 8);
    DEF(CMIOExtensionDevice, "CMIOExtensionDevice", sizeof(xobj_t) - 8);
    DEF(CMIOExtensionStream, "CMIOExtensionStream", sizeof(xobj_t) - 8);
    DEF(CMIOExtensionStreamFormat, "CMIOExtensionStreamFormat", sizeof(xobj_t) - 8);
    DEF(CMIOExtensionScheduledOutput, "CMIOExtensionScheduledOutput", sizeof(xobj_t) - 8);
    DEF(CMIOExtensionProviderProperties, "CMIOExtensionProviderProperties", sizeof(xprops_t) - 8);
    DEF(CMIOExtensionDeviceProperties, "CMIOExtensionDeviceProperties", sizeof(xprops_t) - 8);
    DEF(CMIOExtensionStreamProperties, "CMIOExtensionStreamProperties", sizeof(xprops_t) - 8);
#undef DEF
#define M(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
#define CM(k, s, f) cx_add_cmethod(&k##_c, s, (void *)(f))
    cx_class_t *pc[3] = {&CMIOExtensionProviderProperties_c, &CMIOExtensionDeviceProperties_c, &CMIOExtensionStreamProperties_c};
    for (int i = 0; i < 3; i++) {
        cx_add_cmethod(pc[i], "providerPropertiesWithDictionary:", (void *)props_with);
        cx_add_cmethod(pc[i], "devicePropertiesWithDictionary:", (void *)props_with);
        cx_add_cmethod(pc[i], "streamPropertiesWithDictionary:", (void *)props_with);
        cx_add_method(pc[i], "propertiesDictionary", (void *)props_dict);
        cx_add_method(pc[i], "setPropertiesDictionary:", (void *)props_set_dict);
        cx_add_method(pc[i], "setPropertyState:forProperty:", (void *)props_set_value);
    }
    M(CMIOExtensionProvider, "initWithSource:clientQueue:", prov_init); M(CMIOExtensionProvider, "devices", prov_devices);
    M(CMIOExtensionProvider, "source", prov_source); M(CMIOExtensionProvider, "addDevice:error:", prov_add); M(CMIOExtensionProvider, "removeDevice:error:", prov_remove);
    M(CMIOExtensionDevice, "initWithLocalizedName:deviceID:legacyDeviceID:source:", dev_init);
    M(CMIOExtensionDevice, "localizedName", dev_name); M(CMIOExtensionDevice, "deviceID", dev_uid); M(CMIOExtensionDevice, "streams", dev_streams);
    M(CMIOExtensionDevice, "addStream:error:", dev_add_stream); M(CMIOExtensionDevice, "source", dev_source);
    M(CMIOExtensionStream, "initWithLocalizedName:streamID:direction:clockType:source:", st_init);
    M(CMIOExtensionStream, "localizedName", st_name); M(CMIOExtensionStream, "direction", st_direction); M(CMIOExtensionStream, "source", st_source);
    M(CMIOExtensionStream, "sendSampleBuffer:discontinuity:hostTimeInNanoseconds:", st_send);
    M(CMIOExtensionStreamFormat, "initWithFormatDescription:maxFrameDuration:minFrameDuration:validFrameDurations:", fmt_init);
    M(CMIOExtensionStreamFormat, "formatDescription", fmt_fd);
    M(CMIOExtensionScheduledOutput, "initWithSequenceNumber:hostTimeInNanoseconds:", sched_init);
#undef M
#undef CM
}
