/* Host: userland/libsystem/avcomposition.c pela API (AVMutableComposition/Track) com um runtime mínimo e AVAsset/
 * AVAssetTrack falsos apontando para os arquivos de verdade do teste do export (DIR/src.mp4 e DIR/src2.mp4).
 * Exporta DIR/comp2.mp4 pelo avx_compose; o tools/avwriter_test.sh confere quadro a quadro. → "avcomposition: N/N ok" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avexport.h"
#include "mp4mux.h"

typedef struct { long long value; int timescale; unsigned flags; long long epoch; } cmt;
typedef struct { cmt start, duration; } cmrange;
typedef struct { cmrange source, target; } cmmapping;
typedef struct { double w, h; } szd;
typedef struct { double a, b, c, d, tx, ty; } aff;
extern char AVMutableComposition_c[] __asm__("OBJC_CLASS_$_AVMutableComposition");
extern char AVMutableCompositionTrack_c[] __asm__("OBJC_CLASS_$_AVMutableCompositionTrack");
extern char AVCompositionTrackSegment_c[] __asm__("OBJC_CLASS_$_AVCompositionTrackSegment");
void cx_avcomposition_init(void);
int cx_avcomposition_export(void *a, const char *out, int container, int fast, double start, double dur, int want, avx_ctl *ctl, const char **why);

/* ---- runtime mínimo */
static char FAKE, ASSET_K, TRACK_K, NSOBJ;
enum { K_STR = 1, K_ARR, K_DICT, K_ERR, K_ASSET, K_TRACK, K_CLASS };
typedef struct fo { void *isa; int kind; const char *s; struct fo **v; int n, cap; struct fo *asset; const char *media; int tid; double dur; } fo;
static fo *fnew(int k) { fo *o = calloc(1, sizeof *o); o->isa = &FAKE; o->kind = k; return o; }
void *cx_str(const char *s) { fo *o = fnew(K_STR); o->s = strdup(s); return o; }
const char *cx_cstr(void *o) { return o && ((fo *)o)->isa == &FAKE && ((fo *)o)->kind == K_STR ? ((fo *)o)->s : 0; }
const char *cx_audio_url_path(void *u) { return cx_cstr(u); }
static fo cls_arr = {&FAKE, K_CLASS, "NSMutableArray"}, cls_dict = {&FAKE, K_CLASS, "NSMutableDictionary"}, cls_err = {&FAKE, K_CLASS, "NSError"};
typedef struct { void *key; const char *name; unsigned extra; void *super; const char *sels[80]; void *imps[80]; int n; } cls_t;
static cls_t tab[16]; static int ntab;
static cls_t *Kc(void *c) { for (int i = 0; i < ntab; i++) if (tab[i].key == c) return &tab[i]; tab[ntab].key = c; return &tab[ntab++]; }
static cls_t *Kf(void *c) { for (int i = 0; i < ntab; i++) if (tab[i].key == c) return &tab[i]; return 0; }
void *cx_class_named(const char *n) {
    if (!strcmp(n, "NSObject")) return &NSOBJ;
    if (!strcmp(n, "AVAsset")) return &ASSET_K;
    if (!strcmp(n, "AVAssetTrack")) return &TRACK_K;
    if (!strcmp(n, "NSMutableArray")) return &cls_arr;
    if (!strcmp(n, "NSMutableDictionary")) return &cls_dict;
    if (!strcmp(n, "NSError")) return &cls_err;
    for (int i = 0; i < ntab; i++) if (tab[i].name && !strcmp(tab[i].name, n)) return tab[i].key;
    return 0;
}
void *cx_define_class(void *c, void *m, const char *name, void *super, unsigned extra) { (void)m; cls_t *k = Kc(c); k->name = name; k->super = super; k->extra = extra; return c; }
void cx_add_method(void *c, const char *s, void *f) { cls_t *k = Kc(c); k->sels[k->n] = s; k->imps[k->n++] = f; }
void cx_add_cmethod(void *c, const char *s, void *f) { cx_add_method(c, s, f); }
void *cx_alloc_instance(void *c) { void **o = calloc(1, 256); o[0] = c; return o; }
static void *imp(void *c, const char *s) { for (cls_t *k = Kf(c); k; k = Kf(k->super)) for (int i = 0; i < k->n; i++) if (!strcmp(k->sels[i], s)) return k->imps[i]; return 0; }
void *class_getMethodImplementation(void *c, const char *s) { (void)c; (void)s; return 0; }
const char *sel_registerName(const char *s) { return s; }
void *objc_retain(void *o) { return o; }
void objc_release(void *o) { (void)o; }
void *objc_autorelease(void *o) { return o; }
static long r0(void) { return 0; }
static void *f_arr(void *k, const char *c) { (void)k; (void)c; return fnew(K_ARR); }
static void *f_dict(void *k, const char *c) { (void)k; (void)c; return fnew(K_DICT); }
static void *f_err(void *k, const char *c, void *d, long code, void *info) { (void)k; (void)c; (void)d; (void)code; (void)info; return fnew(K_ERR); }
static void push(fo *a, fo *x) { if (a->n == a->cap) { a->cap = a->cap ? a->cap * 2 : 8; a->v = realloc(a->v, a->cap * sizeof *a->v); } a->v[a->n++] = x; }
static void f_add(fo *a, const char *c, fo *x) { (void)c; push(a, x); }
static void f_ins(fo *a, const char *c, fo *x, unsigned long i) { (void)c; push(a, x); memmove(a->v + i + 1, a->v + i, (a->n - 1 - i) * sizeof *a->v); a->v[i] = x; }
static unsigned long f_count(fo *a, const char *c) { (void)c; return (unsigned long)a->n; }
static void *f_at(fo *a, const char *c, unsigned long i) { (void)c; return a->v[i]; }
static void f_set(fo *d, const char *c, void *v, void *k) { (void)d; (void)c; (void)v; (void)k; }
static void *t_asset(fo *t, const char *c) { (void)c; return t->asset; }
static void *t_media(fo *t, const char *c) { (void)c; return cx_str(t->media); }
static int t_tid(fo *t, const char *c) { (void)c; return t->tid; }
static cmt t_dur(fo *t, const char *c) { (void)c; cmt r = {(long long)(t->dur * 600), 600, 1, 0}; return r; }
static szd t_size(fo *t, const char *c) { (void)c; szd z = {64, 48}; return z; }
static aff t_tf(fo *t, const char *c) { (void)t; (void)c; aff a = {1, 0, 0, 1, 0, 0}; return a; }
static void *a_url(fo *a, const char *c) { (void)c; return cx_str(a->s); }
static void *a_tracks(fo *a, const char *c) { (void)c; fo *r = fnew(K_ARR); for (int i = 0; i < a->n; i++) push(r, a->v[i]); return r; }
void *fake_lookup(void *self, const char *sel) {
    if (!self) return (void *)r0;
    void *f = imp(self, sel);
    if (f) return f;
    void *isa = *(void **)self;
    if (isa != &FAKE) { f = imp(isa, sel); return f ? f : (void *)r0; }
    fo *o = self;
    if (o == &cls_arr && !strcmp(sel, "array")) return (void *)f_arr;
    if (o == &cls_dict && !strcmp(sel, "dictionary")) return (void *)f_dict;
    if (o == &cls_err && !strcmp(sel, "errorWithDomain:code:userInfo:")) return (void *)f_err;
    if (o->kind == K_ARR) {
        if (!strcmp(sel, "addObject:")) return (void *)f_add;
        if (!strcmp(sel, "insertObject:atIndex:")) return (void *)f_ins;
        if (!strcmp(sel, "count")) return (void *)f_count;
        if (!strcmp(sel, "objectAtIndex:")) return (void *)f_at;
    }
    if (o->kind == K_DICT && !strcmp(sel, "setObject:forKey:")) return (void *)f_set;
    if (o->kind == K_TRACK) {
        if (!strcmp(sel, "asset")) return (void *)t_asset;
        if (!strcmp(sel, "mediaType")) return (void *)t_media;
        if (!strcmp(sel, "trackID")) return (void *)t_tid;
        if (!strcmp(sel, "duration")) return (void *)t_dur;
        if (!strcmp(sel, "naturalSize")) return (void *)t_size;
        if (!strcmp(sel, "preferredTransform")) return (void *)t_tf;
    }
    if (o->kind == K_ASSET) { if (!strcmp(sel, "URL")) return (void *)a_url; if (!strcmp(sel, "tracks")) return (void *)a_tracks; }
    fprintf(stderr, "seletor: %s (%d)\n", sel, o->kind);
    return (void *)r0;
}
__asm__(".text\n.globl objc_msgSend\nobjc_msgSend:\npush %rbp\nmov %rsp,%rbp\nsub $0x60,%rsp\n"
        "mov %rdi,0(%rsp)\nmov %rsi,8(%rsp)\nmov %rdx,16(%rsp)\nmov %rcx,24(%rsp)\nmov %r8,32(%rsp)\nmov %r9,40(%rsp)\n"
        "movdqu %xmm0,48(%rsp)\nmovdqu %xmm1,64(%rsp)\ncall fake_lookup\nmov %rax,%r11\n"
        "mov 0(%rsp),%rdi\nmov 8(%rsp),%rsi\nmov 16(%rsp),%rdx\nmov 24(%rsp),%rcx\nmov 32(%rsp),%r8\nmov 40(%rsp),%r9\n"
        "movdqu 48(%rsp),%xmm0\nmovdqu 64(%rsp),%xmm1\nleave\njmp *%r11\n");
/* objc_msgSend_stret(ret, self, op, ...): procura por (self, op) e salta com os registradores intactos */
__asm__(".text\n.globl objc_msgSend_stret\nobjc_msgSend_stret:\npush %rbp\nmov %rsp,%rbp\nsub $0x40,%rsp\n"
        "mov %rdi,0(%rsp)\nmov %rsi,8(%rsp)\nmov %rdx,16(%rsp)\nmov %rcx,24(%rsp)\nmov %r8,32(%rsp)\nmov %r9,40(%rsp)\n"
        "mov %rsi,%rdi\nmov %rdx,%rsi\ncall fake_lookup\nmov %rax,%r11\n"
        "mov 0(%rsp),%rdi\nmov 8(%rsp),%rsi\nmov 16(%rsp),%rdx\nmov 24(%rsp),%rcx\nmov 32(%rsp),%r8\nmov 40(%rsp),%r9\n"
        "leave\njmp *%r11\n");

static int fails, total;
static void expect(int c, const char *m) { total++; if (!c) { printf("FALHOU: %s\n", m); fails++; } }
static cmt T(double s) { cmt t = {(long long)(s * 600 + 0.5), 600, 1, 0}; return t; }
static cmrange RG(double a, double d) { cmrange r = {T(a), T(d)}; return r; }
static double S(cmt t) { return t.timescale ? (double)t.value / t.timescale : -1; }
static fo *mk_asset(const char *path) {
    fo *a = fnew(K_ASSET); a->s = path;
    fo *v = fnew(K_TRACK), *s = fnew(K_TRACK);
    v->asset = a; v->media = "vide"; v->tid = 2; v->dur = 2.0;
    s->asset = a; s->media = "soun"; s->tid = 1; s->dur = 1.5;
    push(a, v); push(a, s);
    return a;
}
static int segs(void *trk, double *starts, int max) {
    fo *arr = ((void *(*)(void *, const char *))imp(AVMutableCompositionTrack_c, "segments"))(trk, "segments");
    for (int i = 0; i < arr->n && i < max; i++) {
        cmmapping m = ((cmmapping (*)(void *, const char *))imp(AVCompositionTrackSegment_c, "timeMapping"))(arr->v[i], "timeMapping");
        starts[i] = S(m.target.start);
    }
    return arr->n;
}
int main(int argc, char **argv) {
    if (argc < 2) return 2;
    char pa[600], pb[600], out[600], bad[600];
    snprintf(pa, sizeof pa, "%s/src.mp4", argv[1]); snprintf(pb, sizeof pb, "%s/src2.mp4", argv[1]);
    snprintf(out, sizeof out, "%s/comp2.mp4", argv[1]); snprintf(bad, sizeof bad, "%s/scaled.mp4", argv[1]);
    cx_avcomposition_init();
    void *comp = ((void *(*)(void *, const char *))imp(AVMutableComposition_c, "composition"))(AVMutableComposition_c, "composition");
#define C(o, s, R, ...) ((R (*)())imp(*(void **)(o), s))(o, s, ##__VA_ARGS__)
    void *vt = C(comp, "addMutableTrackWithMediaType:preferredTrackID:", void *, cx_str("vide"), 0);
    void *at = C(comp, "addMutableTrackWithMediaType:preferredTrackID:", void *, cx_str("soun"), 0);
    expect(vt && at && C(vt, "trackID", int) == 1 && C(at, "trackID", int) == 2, "faixas criadas");
    fo *A = mk_asset(pa), *B = mk_asset(pb);
    void *err = 0;
    signed char ok1 = ((signed char (*)(void *, const char *, cmrange, void *, cmt, void **))imp(AVMutableCompositionTrack_c, "insertTimeRange:ofTrack:atTime:error:"))(vt, "", RG(0.4, 0.4), A->v[0], T(0), &err);
    signed char ok2 = ((signed char (*)(void *, const char *, cmrange, void *, cmt, void **))imp(AVMutableCompositionTrack_c, "insertTimeRange:ofTrack:atTime:error:"))(vt, "", RG(0.2, 0.4), B->v[0], T(0.4), &err);
    signed char ok3 = ((signed char (*)(void *, const char *, cmrange, void *, cmt, void **))imp(AVMutableCompositionTrack_c, "insertTimeRange:ofTrack:atTime:error:"))(at, "", RG(0, 0.8), A->v[1], T(0), &err);
    signed char no1 = ((signed char (*)(void *, const char *, cmrange, void *, cmt, void **))imp(AVMutableCompositionTrack_c, "insertTimeRange:ofTrack:atTime:error:"))(vt, "", RG(0, 0.1), A->v[1], T(0), &err);
    double st[8];
    cmt d = ((cmt (*)(void *, const char *))imp(AVMutableComposition_c, "duration"))(comp, "duration");
    expect(ok1 && ok2 && ok3 && !no1 && err, "inserir trechos (e recusar áudio em faixa de vídeo)");
    expect(segs(vt, st, 8) == 2 && st[0] == 0 && st[1] == 0.4 && S(d) == 0.8, "dois trechos e duração 0,8 s");
    ((void (*)(void *, const char *, cmrange))imp(AVMutableCompositionTrack_c, "insertEmptyTimeRange:"))(vt, "", RG(0.4, 0.1));
    int n3 = segs(vt, st, 8);
    ((void (*)(void *, const char *, cmrange))imp(AVMutableCompositionTrack_c, "removeTimeRange:"))(vt, "", RG(0.4, 0.1));
    expect(n3 == 3 && st[2] == 0.5 && segs(vt, st, 8) == 2 && st[1] == 0.4, "inserir vazio empurra; remover puxa de volta");
    ((signed char (*)(void *, const char *, cmrange, void *, cmt, void **))imp(AVMutableCompositionTrack_c, "insertTimeRange:ofTrack:atTime:error:"))(vt, "", RG(0, 0.2), A->v[0], T(0.2), &err);
    int n4 = segs(vt, st, 8);
    double s4 = st[3];
    ((void (*)(void *, const char *, cmrange))imp(AVMutableCompositionTrack_c, "removeTimeRange:"))(vt, "", RG(0.2, 0.2));
    int n5 = segs(vt, st, 8);
    expect(n4 == 4 && s4 == 0.6 && n5 == 3 && st[0] == 0 && st[1] == 0.2 && st[2] == 0.4, "inserir no meio parte o trecho; remover devolve");
    const char *why = 0;
    int rc = cx_avcomposition_export(comp, out, MP4W_MP4, 0, 0, -1, AVX_VIDEO | AVX_AUDIO, 0, &why);
    expect(rc == 0, "exportar a composição");
    if (rc) printf("  %d %s\n", rc, why ? why : "");
    ((void (*)(void *, const char *, cmrange, cmt))imp(AVMutableCompositionTrack_c, "scaleTimeRange:toDuration:"))(at, "", RG(0, 0.4), T(0.8));
    rc = cx_avcomposition_export(comp, bad, MP4W_MP4, 0, 0, -1, AVX_VIDEO | AVX_AUDIO, 0, &why);
    FILE *f = fopen(bad, "rb");
    expect(rc == AVX_ERR_CODEC && why && strstr(why, "escalado") && !f, "trecho escalado: recusado com o motivo");
    if (f) fclose(f);
    printf("avcomposition: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
