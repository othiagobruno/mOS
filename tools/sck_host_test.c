/* Host: userland/libsystem/screencapturekit.c + sck_core.c + avcap_conv.c com runtime mínimo (trampolim do objc_msgSend),
 * compositor falso atrás do GUI_DEVICE (lista de janelas, composição filtrada, captura de janela, áudio do sistema). → "sck: N/N ok" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
typedef int32_t i32; typedef uint32_t u32; typedef uint64_t u64; typedef int64_t i64; typedef uint8_t u8; typedef uint16_t u16; typedef int16_t i16;
#include "../kernel/include/compat_gui.h"

#define SHELL(n) char n##_c[512] __asm__("OBJC_CLASS_$_" #n), n##_m[512] __asm__("OBJC_METACLASS_$_" #n);
SHELL(SCDisplay) SHELL(SCWindow) SHELL(SCContentFilter) SHELL(SCShareableContent) SHELL(SCStreamConfiguration) SHELL(SCScreenshotManager)
char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
const void *SCStreamFrameInfoStatus = "Status", *SCStreamFrameInfoDisplayTime = "DT", *SCStreamFrameInfoScaleFactor = "SF", *SCStreamFrameInfoContentScale = "CS",
           *SCStreamFrameInfoContentRect = "CR", *SCStreamFrameInfoDirtyRects = "DR", *SCStreamFrameInfoBoundingRect = "BR";
void cx_screencapturekit_init(void);

/* ---------------------------------------------------------------- runtime */
typedef struct { void *key; const char *name; unsigned extra; void *super; const char *sels[200]; void *imps[200]; int n; } cls_t;
static cls_t tab[48]; static int ntab;
static char NSObj, ARRC, NCC, STRC, NUMC;
static cls_t *Kc(void *c) { for (int i = 0; i < ntab; i++) if (tab[i].key == c) return &tab[i]; tab[ntab].key = c; return &tab[ntab++]; }
static cls_t *Kf(void *c) { for (int i = 0; i < ntab; i++) if (tab[i].key == c) return &tab[i]; return 0; }
void *cx_class_named(const char *n) {
    if (!strcmp(n, "NSObject")) return &NSObj;
    if (!strcmp(n, "NSMutableArray") || !strcmp(n, "NSArray") || !strcmp(n, "NSMutableDictionary") || !strcmp(n, "NSDictionary")) return &ARRC;
    if (!strcmp(n, "NSError")) return &NCC;
    for (int i = 0; i < ntab; i++) if (tab[i].name && !strcmp(tab[i].name, n)) return tab[i].key;
    return 0;
}
void *cx_define_class(void *c, void *m, const char *name, void *super, unsigned extra) { (void)m; cls_t *k = Kc(c); k->name = name; k->super = super; k->extra = extra; return c; }
void cx_add_method(void *c, const char *s, void *f) { cls_t *k = Kc(c); k->sels[k->n] = s; k->imps[k->n++] = f; }
void cx_add_cmethod(void *c, const char *s, void *f) { cx_add_method(c, s, f); }
static unsigned size_of(void *c) { unsigned s = 0; for (cls_t *k = Kf(c); k; k = Kf(k->super)) s += k->extra; return s; }
void *cx_alloc_instance(void *c) { void **o = calloc(1, 16 + size_of(c) + 64); o[0] = c; return o; }
static void *imp(void *c, const char *s) { for (cls_t *k = Kf(c); k; k = Kf(k->super)) for (int i = 0; i < k->n; i++) if (!strcmp(k->sels[i], s)) return k->imps[i]; return 0; }
void *cx_lookup(void *c, const char *s) { (void)c; (void)s; return 0; }
const char *sel_registerName(const char *s) { return s; }
void *objc_retain(void *o) { return o; }
void objc_release(void *o) { (void)o; }
void *objc_autorelease(void *o) { return o; }
void *objc_autoreleasePoolPush(void) { return 0; }
void objc_autoreleasePoolPop(void *p) { (void)p; }
void *_Block_copy(const void *b) { return (void *)b; }
void _Block_release(const void *b) { (void)b; }
void *object_getClass(void *o) { return o ? *(void **)o : 0; }
signed char cx_responds(void *c, const char *s) { return imp(c, s) != 0; }
typedef struct { void *isa; const char *s; } str_t;
typedef struct { void *isa; long long v; double d; } num_t;
typedef struct { void *isa; void *it[64]; int n; } arr_t;
void *cx_str(const char *s) { str_t *o = calloc(1, sizeof *o); o->isa = &STRC; o->s = strdup(s); return o; }
const char *cx_cstr(void *o) { return o ? ((str_t *)o)->s : ""; }
void *cx_num_i(long long v) { num_t *o = calloc(1, sizeof *o); o->isa = &NUMC; o->v = v; return o; }
void *cx_num_d(double v) { num_t *o = calloc(1, sizeof *o); o->isa = &NUMC; o->d = v; return o; }
static void *f_arr(void *k, const char *c) { (void)k; (void)c; arr_t *a = calloc(1, sizeof *a); a->isa = &ARRC; return a; }
static void *f_arr1(void *k, const char *c, void *o) { arr_t *a = f_arr(k, c); a->it[a->n++] = o; return a; }
static void f_add(arr_t *a, const char *c, void *o) { (void)c; a->it[a->n++] = o; }
static unsigned long long f_count(arr_t *a, const char *c) { (void)c; return (unsigned long long)a->n; }
static void *f_at(arr_t *a, const char *c, unsigned long long i) { (void)c; return a->it[i]; }
static void f_nop(void) {}
static void *f_err(void *k, const char *c, void *dom, long code, void *ui) { (void)k; (void)c; (void)dom; (void)ui; return cx_num_i(code); }
static long f_ret0(void) { return 0; }
void *fake_lookup(void *self, const char *sel) {
    if (!self) return (void *)f_ret0;
    if (self == &ARRC) return !strcmp(sel, "arrayWithObject:") ? (void *)f_arr1 : (void *)f_arr;
    if (self == &NCC) return (void *)f_err;
    void *f = imp(self, sel);
    if (f) return f;
    void *isa = *(void **)self;
    if (isa == &ARRC) return !strcmp(sel, "addObject:") ? (void *)f_add : !strcmp(sel, "count") ? (void *)f_count : !strcmp(sel, "objectAtIndex:") ? (void *)f_at : (void *)f_nop;
    f = imp(isa, sel);
    return f ? f : (void *)f_ret0;
}
__asm__(".text\n.globl objc_msgSend\nobjc_msgSend:\npush %rbp\nmov %rsp,%rbp\nsub $0x40,%rsp\n"
        "mov %rdi,0(%rsp)\nmov %rsi,8(%rsp)\nmov %rdx,16(%rsp)\nmov %rcx,24(%rsp)\nmov %r8,32(%rsp)\nmov %r9,40(%rsp)\n"
        "movdqu %xmm0,48(%rsp)\ncall fake_lookup\nmov %rax,%r11\n"
        "mov 0(%rsp),%rdi\nmov 8(%rsp),%rsi\nmov 16(%rsp),%rdx\nmov 24(%rsp),%rcx\nmov 32(%rsp),%r8\nmov 40(%rsp),%r9\nmovdqu 48(%rsp),%xmm0\n"
        "leave\njmp *%r11\n");

/* ---------------------------------------------------------------- CoreMedia / CV / CG de mentira */
typedef struct { long long value; int timescale; unsigned flags; long long epoch; } cmt_t;
typedef struct { unsigned fmt; int w, h, np; unsigned char *base[2]; } pb_t;
typedef struct { pb_t *pb; int status; unsigned frames, ch; } sb_t;
static volatile long long clk = 1000000000;
cmt_t CMClockGetTime(void *c) { (void)c; cmt_t t = {__atomic_add_fetch(&clk, 1000000, __ATOMIC_RELAXED), 1000000000, 1, 0}; return t; }
void *CMClockGetHostTimeClock(void) { return (void *)1; }
int CVPixelBufferCreateWithBytes(void *a, u64 w, u64 h, u32 fmt, void *base, u64 bpr, void *rel, void *ctx, void *at, void **o) {
    (void)a; (void)bpr; (void)rel; (void)ctx; (void)at; pb_t *p = calloc(1, sizeof *p); p->fmt = fmt; p->w = (int)w; p->h = (int)h; p->np = 1; p->base[0] = base; *o = p; return 0;
}
int CVPixelBufferCreateWithPlanarBytes(void *a, u64 w, u64 h, u32 fmt, void *d, u64 ds, u64 np, void **pb, u64 *pw, u64 *ph, u64 *pr, void *rel, void *ctx, void *at, void **o) {
    (void)a; (void)d; (void)ds; (void)pw; (void)ph; (void)pr; (void)rel; (void)ctx; (void)at;
    pb_t *p = calloc(1, sizeof *p); p->fmt = fmt; p->w = (int)w; p->h = (int)h; p->np = (int)np; p->base[0] = pb[0]; p->base[1] = pb[1]; *o = p; return 0;
}
int CMVideoFormatDescriptionCreateForImageBuffer(void *a, void *img, void **o) { (void)a; (void)img; *o = (void *)1; return 0; }
int CMSampleBufferCreateReadyWithImageBuffer(void *a, void *img, void *fd, const void *t, void **o) { (void)a; (void)fd; (void)t; sb_t *s = calloc(1, sizeof *s); s->pb = img; s->status = -1; *o = s; return 0; }
void CMSetAttachment(void *target, void *key, void *v, unsigned m) { (void)m; if (key == SCStreamFrameInfoStatus) ((sb_t *)target)->status = (int)((num_t *)v)->v; }
void *cx_cm_audio_float_planar(const float *p, unsigned frames, unsigned ch, double rate, cmt_t pts) { (void)p; (void)rate; (void)pts; sb_t *s = calloc(1, sizeof *s); s->frames = frames; s->ch = ch; return s; }
static int img_w, img_h;
void *CGDataProviderCreateWithData(void *i, const void *d, size_t n, void *r) { (void)i; (void)n; (void)r; return (void *)d; }
void *CGImageCreate(size_t w, size_t h, size_t a, size_t b, size_t c, const void *d, unsigned e, void *f, const void *g, _Bool hh, int i) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)hh; (void)i; img_w = (int)w; img_h = (int)h; return malloc(8);
}
void *CGColorSpaceCreateDeviceRGB(void) { return (void *)1; }
void CGColorSpaceRelease(void *c) { (void)c; }
void CGDataProviderRelease(void *p) { (void)p; }
void CGImageRelease(void *i) { (void)i; }
void *CGColorRetain(void *c) { return c; }
void CGColorRelease(void *c) { (void)c; }
static double bgcol[4] = {0, 0, 1, 1};
size_t CGColorGetNumberOfComponents(const void *c) { (void)c; return 4; }
const double *CGColorGetComponents(const void *c) { (void)c; return bgcol; }
void *dispatch_get_global_queue(long p, unsigned long f) { (void)p; (void)f; return (void *)9; }
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *)) { (void)q; fn(ctx); }
unsigned long long mach_absolute_time(void) { return 5; }
static int tcc_ok = 1;
int cx_tcc_preflight(const char *s) { (void)s; return tcc_ok; }
unsigned avf_copy_capture(short *dst, unsigned frames, int *err) { (void)dst; (void)frames; *err = 0; return 0; }
int getpid(void) { return 100; }

/* ---------------------------------------------------------------- compositor falso */
static gui_cgwindow_t wl[3];
static int nwl = 3;
int cx_cg_window_at(int i, gui_cgwindow_t *o) { if (i < 0 || i >= nwl) return 0; *o = wl[i]; return 1; }
static gui_sc_compose_t last_rq; static int composes, plain_caps, win_caps;
static unsigned fill = 0xFF102030; static int win_gone;
static long dims(int w, int h) { return (long)((unsigned long)(unsigned)w | (unsigned long)(unsigned)h << 32); }
long cx_syscall(long n, long a, long b, long c, long d, long e) {
    (void)e;
    if (n != 79) return -1;
    switch (a) {
    case DEV_SCREEN_CAPTURE: {
        int w = (int)(unsigned)c, h = (int)((unsigned long)c >> 32);
        if (w <= 0) { w = 64; h = 32; }
        if (d) { plain_caps++; for (int i = 0; i < w * h; i++) ((unsigned *)d)[i] = fill; }
        return dims(w, h);
    }
    case DEV_SCREEN_COMPOSE: {
        gui_sc_compose_t *r = (gui_sc_compose_t *)b;
        last_rq = *r;
        int w = r->w > 0 ? r->w : 64, h = r->h > 0 ? r->h : 32;
        if (c) { composes++; for (int i = 0; i < w * h; i++) ((unsigned *)c)[i] = fill; }
        return dims(w, h);
    }
    case DEV_WINDOW_CAPTURE:
        if (win_gone) return -3;
        if (c) { win_caps++; for (int i = 0; i < 6 * 3; i++) ((unsigned *)c)[i] = 0xFF000000u | (unsigned)i; }
        return dims(6, 3);
    case DEV_AUDIO_SYSTEM: {
        unsigned long long *pos = (unsigned long long *)d;
        long nf = c > 480 ? 480 : c;
        memset((void *)b, 0, (size_t)nf * 4);
        *pos += (unsigned long long)nf;
        return nf;
    }
    }
    return -1;
}

/* ---------------------------------------------------------------- delegate e saída */
static char OUTC[512], OUTM[512];
static volatile int vframes, aframes, stopped; static sb_t *last_v, *last_a; static long stop_code;
static void o_out(void *s, const char *c, void *stream, sb_t *sb, long type) { (void)s; (void)c; (void)stream; if (type == 0) { last_v = sb; vframes++; } else if (type == 1) { last_a = sb; aframes++; } }
static void o_stop(void *s, const char *c, void *stream, num_t *err) { (void)s; (void)c; (void)stream; stop_code = err ? err->v : 0; stopped++; }
static void *got_content, *got_img; static num_t *got_err;
static void blk_ct(void *b, void *ct, void *err) { (void)b; got_content = ct; got_err = err; }
static struct { void *isa; int fl, re; void (*inv)(void *, void *, void *); } blk2 = {0, 0, 0, blk_ct};
static num_t *start_err; static int start_called;
static void blk_e(void *b, num_t *err) { (void)b; start_err = err; start_called++; }
static struct { void *isa; int fl, re; void (*inv)(void *, num_t *); } blk1 = {0, 0, 0, blk_e};
static void blk_img(void *b, void *img, void *err) { (void)b; got_img = img; got_err = err; }
static struct { void *isa; int fl, re; void (*inv)(void *, void *, void *); } blkimg = {0, 0, 0, blk_img};

static int fails, total;
static void expect(int c, const char *m) { total++; if (!c) { printf("FALHOU: %s\n", m); fails++; } }
#define CALL(o, sel, R, ...) ((R (*)())imp(*(void **)(o), sel))(o, sel, ##__VA_ARGS__)
#define CCALL(k, sel, R, ...) ((R (*)())imp(k, sel))(k, sel, ##__VA_ARGS__)
static void wait_for(volatile int *v, int n) { for (int i = 0; i < 2000 && *v < n; i++) usleep(1000); }
typedef struct { double x, y, w, h; } rc_t;

int main(void) {
    wl[0] = (gui_cgwindow_t){5, 100, 0, 1, 10, 10, 300, 200, "Notas", "Editor", ""};
    wl[1] = (gui_cgwindow_t){6, 200, 0, 1, 50, 50, 300, 200, "Terminal", "Terminal", ""};
    wl[2] = (gui_cgwindow_t){1, 0, -20, 1, 0, 0, 64, 32, "Desktop", "", ""};
    cx_define_class(OUTC, OUTM, "Saida", &NSObj, 0);
    cx_add_method(OUTC, "stream:didOutputSampleBuffer:ofType:", (void *)o_out);
    cx_add_method(OUTC, "stream:didStopWithError:", (void *)o_stop);
    void *out = cx_alloc_instance(OUTC);
    cx_define_class(SCScreenshotManager_c, SCScreenshotManager_m, "SCScreenshotManager", &NSObj, 32);   /* a casca do vscode_fwclasses.c */
    extern void cx_sck_classes_init(void);
    cx_sck_classes_init();
    cx_screencapturekit_init();

    /* conteúdo */
    CCALL(SCShareableContent_c, "getShareableContentWithCompletionHandler:", void, &blk2);
    arr_t *wins = got_content ? CALL(got_content, "windows", arr_t *) : 0, *apps = got_content ? CALL(got_content, "applications", arr_t *) : 0;
    expect(wins && wins->n == 3 && apps && apps->n == 2, "três janelas e dois apps");
    expect(wins && CALL(wins->it[0], "windowID", unsigned) == 5 && CALL(wins->it[0], "isActive", signed char) && !CALL(wins->it[1], "isActive", signed char), "a da frente é a ativa");
    void *w5 = wins ? wins->it[0] : 0, *w6 = wins ? wins->it[1] : 0;
    expect(apps && CALL(apps->it[0], "processID", long) == 100 && !strcmp(cx_cstr(CALL(apps->it[0], "applicationName", void *)), "Notas"), "app dono");
    CCALL(SCShareableContent_c, "getShareableContentExcludingDesktopWindows:onScreenWindowsOnly:completionHandler:", void, (signed char)1, (signed char)1, &blk2);
    expect(CALL(got_content, "windows", arr_t *)->n == 2, "sem as janelas do desktop");
    CCALL(SCShareableContent_c, "getShareableContentExcludingDesktopWindows:onScreenWindowsOnlyAboveWindow:completionHandler:", void, (signed char)0, w6, &blk2);
    arr_t *above = CALL(got_content, "windows", arr_t *);
    expect(above->n == 1 && CALL(above->it[0], "windowID", unsigned) == 5, "acima da 6 só a 5");
    CCALL(SCShareableContent_c, "getShareableContentExcludingDesktopWindows:onScreenWindowsOnlyBelowWindow:completionHandler:", void, (signed char)1, w5, &blk2);
    arr_t *below = CALL(got_content, "windows", arr_t *);
    expect(below->n == 1 && CALL(below->it[0], "windowID", unsigned) == 6, "abaixo da 5, sem desktop: a 6");
    tcc_ok = 0;
    CCALL(SCShareableContent_c, "getShareableContentWithCompletionHandler:", void, &blk2);
    expect(CALL(got_content, "windows", arr_t *)->n == 0 && CALL(got_content, "displays", arr_t *)->n == 1, "sem permissão: só o display");
    tcc_ok = 1;
    void *disp = CALL(got_content, "displays", arr_t *)->it[0];
    expect(CALL(disp, "width", long) == 64 && CALL(disp, "height", long) == 32, "display com o tamanho da tela");

    /* filtro excluindo a 5, saída 32x16 com escala: a composição vai ao kernel com a lista */
    void *flt = cx_alloc_instance(SCContentFilter_c);
    CALL(flt, "initWithDisplay:excludingWindows:", void *, disp, f_arr1(0, 0, w5));
    void *cfg = cx_alloc_instance(SCStreamConfiguration_c);
    CALL(cfg, "init", void *);
    CALL(cfg, "setWidth:", void, 32L); CALL(cfg, "setHeight:", void, 16L);
    CALL(cfg, "setScalesToFit:", void, (signed char)1);
    expect(CALL(cfg, "showsCursor", signed char) && CALL(cfg, "pixelFormat", unsigned) == 0x42475241 && CALL(cfg, "queueDepth", long) == 8, "padrões da configuração");
    CCALL(SCScreenshotManager_c, "captureImageWithFilter:configuration:completionHandler:", void, flt, cfg, &blkimg);
    expect(got_img && img_w == 32 && img_h == 16 && composes == 1 && last_rq.nwin == 1 && last_rq.win[0] == 5 &&
           last_rq.flags == (GUI_SC_DESKTOP | GUI_SC_CURSOR), "screenshot: composição sem a janela 5, saída 32x16");
    /* sem filtro de janelas e com cursor: a tela do compositor como está */
    void *fall = cx_alloc_instance(SCContentFilter_c);
    CALL(fall, "initWithDisplay:excludingWindows:", void *, disp, f_arr(0, 0));
    CCALL(SCScreenshotManager_c, "captureImageWithFilter:configuration:completionHandler:", void, fall, cfg, &blkimg);
    expect(plain_caps == 1 && composes == 1, "tela inteira com cursor: captura direta");
    /* incluindo o app 200 */
    void *finc = cx_alloc_instance(SCContentFilter_c);
    CALL(finc, "initWithDisplay:includingApplications:exceptingWindows:", void *, disp, f_arr1(0, 0, apps->it[1]), f_arr(0, 0));
    CALL(cfg, "setShowsCursor:", void, (signed char)0);
    CCALL(SCScreenshotManager_c, "captureImageWithFilter:configuration:completionHandler:", void, finc, cfg, &blkimg);
    expect(last_rq.flags == GUI_SC_INCLUDE && last_rq.nwin == 1 && last_rq.win[0] == 6, "incluindo o app 200: só a janela 6, sem fundo nem cursor");
    rc_t cr = ((rc_t (*)(void *, const char *))imp(SCContentFilter_c, "contentRect"))(finc, "");
    expect(cr.w == 64 && CALL(finc, "style", long) == 2, "contentRect e estilo do display");

    /* stream 420v 8x4 com a saída de tela e de áudio */
    void *cfg2 = cx_alloc_instance(SCStreamConfiguration_c);
    CALL(cfg2, "init", void *);
    CALL(cfg2, "setWidth:", void, 8L); CALL(cfg2, "setHeight:", void, 4L); CALL(cfg2, "setScalesToFit:", void, (signed char)1);
    CALL(cfg2, "setPixelFormat:", void, 0x34323076u);
    CALL(cfg2, "setCapturesAudio:", void, (signed char)1);
    cmt_t iv = {1, 100, 1, 0};
    ((void (*)(void *, const char *, cmt_t))imp(SCStreamConfiguration_c, "setMinimumFrameInterval:"))(cfg2, "", iv);
    void *st = cx_alloc_instance(cx_class_named("SCStream"));
    CALL(st, "initWithFilter:configuration:delegate:", void *, flt, cfg2, out);
    expect(CALL(st, "addStreamOutput:type:sampleHandlerQueue:error:", signed char, out, 0L, (void *)3, (void **)0) &&
           CALL(st, "addStreamOutput:type:sampleHandlerQueue:error:", signed char, out, 1L, (void *)3, (void **)0), "saídas de tela e áudio");
    CALL(st, "stopCaptureWithCompletionHandler:", void, &blk1);
    expect(start_err && start_err->v == -3809, "parar sem rodar: erro");
    CALL(st, "startCaptureWithCompletionHandler:", void, &blk1);
    expect(start_called == 2 && !start_err, "start sem erro");
    wait_for(&vframes, 1);
    wait_for(&aframes, 2);
    expect(vframes >= 1 && last_v && last_v->pb->fmt == 0x34323076 && last_v->pb->np == 2 && last_v->pb->w == 8 && last_v->status == 0, "quadro 420v 8x4 completo");
    expect(aframes >= 2 && last_a && last_a->ch == 2 && last_a->frames > 0, "áudio do sistema em Float32");
    int before = vframes;
    usleep(60000);
    expect(vframes == before, "tela parada: nenhum quadro novo");
    fill = 0xFF405060;
    wait_for(&vframes, before + 1);
    expect(vframes == before + 1, "a tela mudou: um quadro novo");
    /* trocar o filtro com o stream rodando: janela independente, recorte 2x2 */
    void *fw = cx_alloc_instance(SCContentFilter_c);
    CALL(fw, "initWithDesktopIndependentWindow:", void *, w6);
    void *cfg3 = cx_alloc_instance(SCStreamConfiguration_c);
    CALL(cfg3, "init", void *);
    rc_t src = {1, 1, 2, 2};
    ((void (*)(void *, const char *, rc_t))imp(SCStreamConfiguration_c, "setSourceRect:"))(cfg3, "", src);
    CALL(st, "updateConfiguration:completionHandler:", void, cfg3, &blk1);
    before = vframes;
    CALL(st, "updateContentFilter:completionHandler:", void, fw, &blk1);
    for (int i = 0; i < 2000 && !(last_v && last_v->pb->w == 2); i++) usleep(1000);
    expect(win_caps >= 1 && last_v->pb->fmt == 0x42475241 && last_v->pb->w == 2 && last_v->pb->h == 2, "janela independente recortada pelo sourceRect");
    unsigned *wp = (unsigned *)last_v->pb->base[0];
    expect(wp && wp[0] == (0xFF000000u | 7) && wp[3] == (0xFF000000u | 14), "pixels da superfície da janela");
    win_gone = 1;
    wait_for(&stopped, 1);
    expect(stopped == 1 && stop_code == -3815, "janela fechou: stream:didStopWithError:");
    CALL(st, "stopCaptureWithCompletionHandler:", void, &blk1);
    /* sem permissão: start falha */
    tcc_ok = 0;
    void *st2 = cx_alloc_instance(cx_class_named("SCStream"));
    CALL(st2, "initWithFilter:configuration:delegate:", void *, flt, cfg2, out);
    CALL(st2, "startCaptureWithCompletionHandler:", void, &blk1);
    expect(start_err && start_err->v == -3801, "sem permissão: -3801");
    printf("sck: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
