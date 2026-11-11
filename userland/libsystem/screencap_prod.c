/*
 * Ganchos de produção da captura contínua: TCC e DEV_SCREEN_CAPTURE (o mesmo do PR #20),
 * e o embrulho do quadro em IOSurface + CVPixelBuffer + CMSampleBuffer.
 * O relógio do kernel é milissegundos; o intervalo mínimo efetivo acompanha esse passo.
 */
#include "foundation.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"
#include "screencap_run.h"

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
id objc_retain(id o);
void objc_release(id o);
int cx_tcc_preflight(const char *service);
unsigned long long mach_absolute_time(void);

extern const void *kIOSurfaceWidth, *kIOSurfaceHeight, *kIOSurfaceBytesPerRow, *kIOSurfacePixelFormat;
id IOSurfaceCreate(id props);
int IOSurfaceLock(id s, unsigned opts, unsigned *seed);
int IOSurfaceUnlock(id s, unsigned opts, unsigned *seed);
void *IOSurfaceGetBaseAddress(id s);
unsigned long long IOSurfaceGetBytesPerRow(id s);

int CVPixelBufferCreateWithIOSurface(void *alloc, id surf, id attrs, void **out);

typedef struct { long long value; int timescale; unsigned flags; long long epoch; } cm_time_t;
typedef struct { cm_time_t duration, pts, dts; } cm_timing_t;
int CMSampleBufferCreateReadyWithImageBuffer(void *a, void *img, void *fd, const void *timing, void **out);
void CMSetAttachment(void *target, id key, id value, unsigned mode);

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; unsigned long long flags; const char *s; unsigned long long n; } cap_cf_t;
#define CFCONST(var, text) \
    static cap_cf_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_s;
CFCONST(SCStreamFrameInfoStatus, "SCStreamFrameInfoStatus")
CFCONST(SCStreamFrameInfoDisplayTime, "SCStreamFrameInfoDisplayTime")
CFCONST(SCStreamFrameInfoScaleFactor, "SCStreamFrameInfoScaleFactor")
CFCONST(SCStreamFrameInfoContentScale, "SCStreamFrameInfoContentScale")
CFCONST(SCStreamFrameInfoContentRect, "SCStreamFrameInfoContentRect")
CFCONST(SCStreamFrameInfoDirtyRects, "SCStreamFrameInfoDirtyRects")
CFCONST(SCStreamFrameInfoBoundingRect, "SCStreamFrameInfoBoundingRect")   /* área com conteúdo: o quadro inteiro */

int cx_cap_allowed(void) { return cx_tcc_preflight("kTCCServiceScreenCapture") == 1; }

int cx_cap_read(int x, int y, int w, int h, unsigned *dst, int dst_px, int *ow, int *oh) {
    if (!dst || dst_px <= 0) return 0;
    unsigned long wh = (unsigned long)(unsigned)w | ((unsigned long)(unsigned)h << 32);
    unsigned long xy = (unsigned long)(unsigned)x | ((unsigned long)(unsigned)y << 32);
    long sz = (long)SYS4(79 /* GUI_DEVICE */, 4 /* DEV_SCREEN_CAPTURE */, xy, wh, 0);
    if (sz <= 0) return 0;
    unsigned cw = (unsigned)sz, ch = (unsigned)((unsigned long)sz >> 32);
    if (!cw || !ch || (unsigned long)cw * ch > (unsigned long)dst_px) return 0;
    long got = (long)SYS4(79, 4, xy, (unsigned long)cw | ((unsigned long)ch << 32), (unsigned long)dst);
    if (got <= 0) return 0;
    if (ow) *ow = (int)cw;
    if (oh) *oh = (int)ch;
    return 1;
}

static void put_num(id d, id key, long long v) {
    id n = cx_num_i(v);
    MSG(void, d, "setObject:forKey:", n, key);
    objc_release(n);
}
static id num_kept(id d, const char *k, long long v) {
    id key = cx_str(k);
    put_num(d, key, v);
    objc_release(key);
    return d;
}

static id rect_dict(int x, int y, int w, int h) {
    id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    if (!d) return 0;
    num_kept(d, "X", x);
    num_kept(d, "Y", y);
    num_kept(d, "Width", w);
    num_kept(d, "Height", h);
    return d;
}

/* surf, pb e sb saem com retain +1. Os pixels são BGRA (o u32 AARRGGBB little-endian do compositor). */
int cx_cap_wrap(const unsigned *px, int w, int h, long long pts_us, id *surf_out, id *pb_out, id *sb_out) {
    if (surf_out) *surf_out = 0;
    if (pb_out) *pb_out = 0;
    if (sb_out) *sb_out = 0;
    if (!px || w <= 0 || h <= 0) return 0;
    id props = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    if (!props) return 0;
    put_num(props, (id)kIOSurfaceWidth, w);
    put_num(props, (id)kIOSurfaceHeight, h);
    put_num(props, (id)kIOSurfaceBytesPerRow, (long long)w * 4);
    put_num(props, (id)kIOSurfacePixelFormat, 0x42475241); /* 'BGRA' */
    id surf = IOSurfaceCreate(props);
    if (!surf) return 0;
    if (IOSurfaceLock(surf, 0, 0) != 0) { objc_release(surf); return 0; }
    unsigned char *base = IOSurfaceGetBaseAddress(surf);
    unsigned long long bpr = IOSurfaceGetBytesPerRow(surf);
    if (!base || bpr < (unsigned long long)w * 4) {
        IOSurfaceUnlock(surf, 0, 0);
        objc_release(surf);
        return 0;
    }
    for (int r = 0; r < h; r++)
        memcpy(base + (size_t)r * (size_t)bpr, (const unsigned char *)px + (size_t)r * (size_t)w * 4, (size_t)w * 4);
    IOSurfaceUnlock(surf, 0, 0);

    id pb = 0;
    if (CVPixelBufferCreateWithIOSurface(0, surf, 0, (void **)&pb) != 0 || !pb) {
        objc_release(surf);
        return 0;
    }
    cm_time_t tick = {pts_us, 1000000, 1, 0};
    cm_timing_t timing = {tick, tick, tick};
    id sb = 0;
    if (CMSampleBufferCreateReadyWithImageBuffer(0, pb, 0, &timing, (void **)&sb) != 0 || !sb) {
        objc_release(pb);
        objc_release(surf);
        return 0;
    }
    id status = cx_num_i(0);
    id dtime = cx_num_i((long long)mach_absolute_time());
    id one = cx_num_d(1.0);
    id rect = rect_dict(0, 0, w, h);
    id dirty = rect ? MSG(id, (id)cx_class_named("NSArray"), "arrayWithObject:", rect) : 0;
    CMSetAttachment(sb, (id)SCStreamFrameInfoStatus, status, 1);
    CMSetAttachment(sb, (id)SCStreamFrameInfoDisplayTime, dtime, 1);
    CMSetAttachment(sb, (id)SCStreamFrameInfoScaleFactor, one, 1);
    CMSetAttachment(sb, (id)SCStreamFrameInfoContentScale, one, 1);
    CMSetAttachment(sb, (id)SCStreamFrameInfoContentRect, rect, 1);
    CMSetAttachment(sb, (id)SCStreamFrameInfoDirtyRects, dirty, 1);
    CMSetAttachment(sb, (id)SCStreamFrameInfoBoundingRect, rect, 1);
    if (status) objc_release(status);
    if (dtime) objc_release(dtime);
    if (one) objc_release(one);
    if (surf_out) *surf_out = surf; else objc_release(surf);
    if (pb_out) *pb_out = pb; else objc_release(pb);
    if (sb_out) *sb_out = sb; else objc_release(sb);
    return 1;
}

int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int usleep(unsigned us);

static long long cap_now_us(void) { return (long long)SYS0(SYS_TIME) * 1000; }

static void *cap_thread(void *arg) {
    cx_cap_run *r = arg;
    while (r->run) {
        int rc = cx_cap_take(&r->cap, cap_now_us(), r->x, r->y, r->w, r->h);
        if (rc == 1 && r->on_frame) {
            id surf = 0, pb = 0, sb = 0;
            if (cx_cap_wrap(r->cap.px, r->cap.w, r->cap.h, cap_now_us(), &surf, &pb, &sb)) {
                r->on_frame(r->ctx, surf, pb, sb);
                if (sb) objc_release(sb);
                if (pb) objc_release(pb);
                if (surf) objc_release(surf);
            }
        } else if (rc < 0) usleep(20000);
        else usleep(2000);
    }
    r->done = 1;
    return 0;
}

int cx_cap_start(cx_cap_run *r) {
    if (!r || !r->cap.px || r->run) return -1;
    r->done = 0;
    r->run = 1;
    if (pthread_create(&r->th, 0, cap_thread, r) != 0) { r->run = 0; return -1; }
    return 0;
}
void cx_cap_stop(cx_cap_run *r) {
    if (!r) return;
    r->run = 0;
    for (int i = 0; i < 40 && !r->done; i++) usleep(1000);
}
