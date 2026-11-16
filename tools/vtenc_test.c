/* Teste de host do VTCompressionSession (userland/libsystem/vscode_media.c de verdade, codificador OpenH264 pela casca
 * cx_h264enc_*), com o runtime em volta substituído. Escreve o fluxo em Annex B para o script conferir com ffprobe/ffmpeg.
 * uso: vtenc_test fonte_nv12.yuv saida.264 perfil(Baseline|Main|High) bitrate forçar_idr | vtenc_test --bgra saida.264
 * Saída: "vtenc ok amostras=N idr=lista notsync=K" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int OSStatus;
typedef struct { long long value; int timescale; unsigned flags; long long epoch; } cm_time_t;
/* ---- runtime mínimo: classes, NSNumber/NSString/dicionário falsos */
typedef struct cls_s { unsigned extra; } cls_t;
void *cx_define_class(void *cls, void *meta, const char *name, void *super, unsigned extra) { (void)meta; (void)name; (void)super; ((cls_t *)cls)->extra = extra; return cls; }
void cx_add_method(void *c, const char *s, void *f) { (void)c; (void)s; (void)f; }
void *cx_lookup(void *c, const char *s) { (void)c; (void)s; return 0; }
static cls_t nsobject;
void *cx_class_named(const char *n) { (void)n; return &nsobject; }
void cx_objc_boot(unsigned long long a, unsigned long long b) { (void)a; (void)b; }
void *cx_alloc_instance(void *c) { void **o = calloc(1, 16 + ((cls_t *)c)->extra); o[0] = c; return o; }
const char *sel_registerName(const char *s) { return s; }
void objc_release(void *o) { (void)o; }
void *objc_retain(void *o) { return o; }
typedef struct { void *isa; int kind; double num; const char *str; int n; const char *keys[8]; void *vals[8]; } fo_t;   /* 1 número, 2 texto, 3 dicionário, 4 array */
static fo_t *num(double v) { fo_t *o = calloc(1, sizeof *o); o->kind = 1; o->num = v; return o; }
static fo_t *str(const char *s) { fo_t *o = calloc(1, sizeof *o); o->kind = 2; o->str = s; return o; }
void *cx_str(const char *s) { return str(s); }
const char *cx_cstr(void *o) { return o && ((fo_t *)o)->kind == 2 ? ((fo_t *)o)->str : 0; }
const void *kCFBooleanTrue, *kCFBooleanFalse;
const void *kCMSampleAttachmentKey_NotSync = "NotSync";
typedef struct { int notsync; } sb_att_t;
/* objc_msgSend: um trampolim que acha a função do seletor e salta para ela (os argumentos e o tipo de retorno ficam
 * os da chamada, inclusive double em xmm0), como o runtime de verdade */
static double m_double(fo_t *o, const char *s) { (void)s; return o->kind == 1 ? o->num : 0; }
static long m_bool(fo_t *o, const char *s) { (void)s; return o == (fo_t *)kCFBooleanTrue || (o->kind == 1 && o->num != 0); }
static void *m_allkeys(fo_t *o, const char *s) { (void)s; fo_t *a = calloc(1, sizeof *a); a->kind = 4; a->n = o->n; for (int i = 0; i < o->n; i++) a->vals[i] = str(o->keys[i]); return a; }
static long m_count(fo_t *o, const char *s) { (void)s; return o->n; }
static void *m_at(fo_t *o, const char *s, unsigned long i) { (void)s; return o->vals[i]; }
static void *m_for(fo_t *o, const char *s, void *k) { (void)s; const char *ks = cx_cstr(k); for (int i = 0; i < o->n; i++) if (ks && !strcmp(o->keys[i], ks)) return o->vals[i]; return 0; }
static void *m_numd(void *c, const char *s, double v) { (void)c; (void)s; return num(v); }
static void *m_numi(void *c, const char *s, int v) { (void)c; (void)s; return num(v); }
static void m_set(fo_t *o, const char *s, void *v, void *k) { (void)s; if (k == kCMSampleAttachmentKey_NotSync && v == kCFBooleanTrue) ((sb_att_t *)o)->notsync = 1; }
static long m_zero(void) { return 0; }
void *mos_lookup_imp(void *self, const char *sel) {
    if (!self) return (void *)m_zero;
    if (!strcmp(sel, "doubleValue")) return (void *)m_double;
    if (!strcmp(sel, "boolValue")) return (void *)m_bool;
    if (!strcmp(sel, "allKeys")) return (void *)m_allkeys;
    if (!strcmp(sel, "count")) return (void *)m_count;
    if (!strcmp(sel, "objectAtIndex:")) return (void *)m_at;
    if (!strcmp(sel, "objectForKey:")) return (void *)m_for;
    if (!strcmp(sel, "numberWithDouble:")) return (void *)m_numd;
    if (!strcmp(sel, "numberWithInt:")) return (void *)m_numi;
    if (!strcmp(sel, "setObject:forKey:")) return (void *)m_set;
    return (void *)m_zero;
}
__asm__(".text\n.globl objc_msgSend\nobjc_msgSend:\n"
        "  sub $0x88, %rsp\n"
        "  mov %rdi, 0(%rsp)\n  mov %rsi, 8(%rsp)\n  mov %rdx, 16(%rsp)\n  mov %rcx, 24(%rsp)\n  mov %r8, 32(%rsp)\n  mov %r9, 40(%rsp)\n"
        "  movsd %xmm0, 48(%rsp)\n  movsd %xmm1, 56(%rsp)\n"
        "  call mos_lookup_imp\n  mov %rax, %r11\n"
        "  mov 0(%rsp), %rdi\n  mov 8(%rsp), %rsi\n  mov 16(%rsp), %rdx\n  mov 24(%rsp), %rcx\n  mov 32(%rsp), %r8\n  mov 40(%rsp), %r9\n"
        "  movsd 48(%rsp), %xmm0\n  movsd 56(%rsp), %xmm1\n"
        "  add $0x88, %rsp\n  jmp *%r11\n");
static fo_t *dict(int n, const char **k, void **v) { fo_t *d = calloc(1, sizeof *d); d->kind = 3; d->n = n; for (int i = 0; i < n; i++) { d->keys[i] = k[i]; d->vals[i] = v[i]; } return d; }
/* CoreVideo / CoreMedia */
typedef struct { unsigned fmt; int w, h; unsigned char *p0, *p1; unsigned long b0, b1; } pb_t;
unsigned long long CVPixelBufferGetWidth(void *p) { return ((pb_t *)p)->w; }
unsigned long long CVPixelBufferGetHeight(void *p) { return ((pb_t *)p)->h; }
unsigned CVPixelBufferGetPixelFormatType(void *p) { return ((pb_t *)p)->fmt; }
unsigned long long CVPixelBufferGetPlaneCount(void *p) { return ((pb_t *)p)->fmt == 0x42475241u ? 0 : 2; }
int CVPixelBufferLockBaseAddress(void *p, unsigned long long f) { (void)p; (void)f; return 0; }
int CVPixelBufferUnlockBaseAddress(void *p, unsigned long long f) { (void)p; (void)f; return 0; }
void *CVPixelBufferGetBaseAddress(void *p) { return ((pb_t *)p)->p0; }
unsigned long long CVPixelBufferGetBytesPerRow(void *p) { return ((pb_t *)p)->b0; }
void *CVPixelBufferGetBaseAddressOfPlane(void *p, unsigned long long i) { return i ? ((pb_t *)p)->p1 : ((pb_t *)p)->p0; }
unsigned long long CVPixelBufferGetBytesPerRowOfPlane(void *p, unsigned long long i) { return i ? ((pb_t *)p)->b1 : ((pb_t *)p)->b0; }
typedef struct { unsigned char *d; unsigned long n; } bb_t;
int CMBlockBufferCreateWithMemoryBlock(void *a, void *mem, unsigned long len, void *ba, const void *cb, unsigned long off, unsigned long dlen, unsigned flags, void **out) {
    (void)a; (void)ba; (void)cb; (void)off; (void)flags; (void)len; bb_t *b = malloc(sizeof *b); b->n = dlen; b->d = malloc(dlen); memcpy(b->d, mem, dlen); *out = b; return 0;
}
typedef struct { unsigned char *sps, *pps; unsigned long sn, pn; int nal; } fd_t;
int CMVideoFormatDescriptionCreateFromH264ParameterSets(void *a, unsigned long n, const unsigned char *const *p, const unsigned long *s, int nal, void **out) {
    (void)a; if (n != 2) return -1; fd_t *f = calloc(1, sizeof *f); f->sps = malloc(s[0]); memcpy(f->sps, p[0], s[0]); f->sn = s[0]; f->pps = malloc(s[1]); memcpy(f->pps, p[1], s[1]); f->pn = s[1]; f->nal = nal; *out = f; return 0;
}
typedef struct { bb_t *bb; fd_t *fd; cm_time_t pts; sb_att_t att; } sb_t;
int CMSampleBufferCreate(void *a, void *db, unsigned char ready, void *cb, void *ref, void *fd, long ns, long nt, const void *t, long nsz, const unsigned long *sz, void **out) {
    (void)a; (void)ready; (void)cb; (void)ref; (void)ns; (void)nt; (void)nsz; (void)sz; sb_t *s = calloc(1, sizeof *s); s->bb = db; s->fd = fd; s->pts = ((const cm_time_t *)t)[1]; *out = s; return 0;
}
void *CMSampleBufferGetSampleAttachmentsArray(void *s, unsigned char c) { (void)c; static fo_t arr; arr.kind = 4; arr.n = 1; arr.vals[0] = &((sb_t *)s)->att; return &arr; }
void *CFDictionaryCreateMutable(void *a, long c, const void *k, const void *v) { (void)a; (void)c; (void)k; (void)v; return 0; }
cm_time_t CMTimeMake(long long v, int ts) { cm_time_t t = {v, ts, 1, 0}; return t; }

typedef void (*enc_cb)(void *ref, void *frame_ref, OSStatus st, unsigned flags, void *sb);
OSStatus VTCompressionSessionCreate(void *a, int w, int h, unsigned codec, void *spec, void *src, void *ca, void *cb, void *ref, void **out);
OSStatus VTSessionSetProperty(void *s, void *key, void *val);
OSStatus VTSessionCopyProperty(void *s, void *key, void *a, void *out);
OSStatus VTCompressionSessionEncodeFrame(void *s, void *img, cm_time_t pts, cm_time_t dur, void *props, void *ref, unsigned *info);
OSStatus VTCompressionSessionCompleteFrames(void *s, cm_time_t until);
void VTCompressionSessionInvalidate(void *s);

static FILE *out; static int nsamp, notsync, drops, errs; static char idrs[256]; static fd_t *last_fd;
static void put(const unsigned char *d, unsigned long n) { static const unsigned char sc[4] = {0, 0, 0, 1}; fwrite(sc, 1, 4, out); fwrite(d, 1, n, out); }
static void on_enc(void *ref, void *frame_ref, OSStatus st, unsigned flags, void *sbp) {
    (void)ref;
    if (st) { errs++; return; }
    if (!sbp) { if (flags & 2) drops++; return; }
    sb_t *sb = sbp;
    if (sb->fd != last_fd) { last_fd = sb->fd; put(sb->fd->sps, sb->fd->sn); put(sb->fd->pps, sb->fd->pn); }
    for (unsigned long o = 0; o + 4 <= sb->bb->n;) {
        unsigned long l = (unsigned long)sb->bb->d[o] << 24 | sb->bb->d[o + 1] << 16 | sb->bb->d[o + 2] << 8 | sb->bb->d[o + 3];
        put(sb->bb->d + o + 4, l); o += 4 + l;
    }
    if (sb->att.notsync) notsync++;
    else { char t[12]; snprintf(t, sizeof t, "%s%d", idrs[0] ? "," : "", (int)(long)frame_ref); strcat(idrs, t); }
    nsamp++;
}
int main(int argc, char **argv) {
    static fo_t T = {0, 1, 1, 0, 0, {0}, {0}}, F = {0, 1, 0, 0, 0, {0}, {0}};
    kCFBooleanTrue = &T; kCFBooleanFalse = &F;
    int bgra = !strcmp(argv[1], "--bgra");
    const int w = 352, h = 288;
    void *s = 0;
    if (VTCompressionSessionCreate(0, w, h, 0x61766331u, 0, 0, 0, (void *)on_enc, 0, &s)) { puts("vtenc FALHA Create"); return 1; }
    out = fopen(argv[2], "wb");
    int nf = bgra ? 10 : 75, force = bgra ? -1 : atoi(argv[5]);
    if (!bgra) {
        VTSessionSetProperty(s, str("kVTCompressionPropertyKey_AverageBitRate"), num(atof(argv[4])));
        VTSessionSetProperty(s, str("ExpectedFrameRate"), num(25));
        VTSessionSetProperty(s, str("MaxKeyFrameInterval"), num(25));
        char pl[64]; snprintf(pl, sizeof pl, "H264_%s_AutoLevel", argv[3]);
        if (VTSessionSetProperty(s, str("ProfileLevel"), str(pl))) { puts("vtenc FALHA ProfileLevel"); return 1; }
        void *got = 0;
        if (VTSessionCopyProperty(s, str("AverageBitRate"), 0, &got) || !got || ((fo_t *)got)->num != atof(argv[4])) { puts("vtenc FALHA CopyProperty"); return 1; }
        if (VTSessionSetProperty(s, str("NaoExiste"), num(1)) != -12900) { puts("vtenc FALHA propriedade desconhecida"); return 1; }
    }
    FILE *in = bgra ? 0 : fopen(argv[1], "rb");
    pb_t pb = {bgra ? 0x42475241u : 0x34323076u, w, h, 0, 0, 0, 0};
    unsigned char *yuv = malloc(w * h * 3 / 2);
    if (bgra) { pb.b0 = w * 4; pb.p0 = malloc(pb.b0 * h); for (int i = 0; i < w * h; i++) { pb.p0[4 * i] = 50; pb.p0[4 * i + 1] = 100; pb.p0[4 * i + 2] = 200; pb.p0[4 * i + 3] = 255; } }
    else { pb.b0 = pb.b1 = w + 32; pb.p0 = malloc(pb.b0 * h); pb.p1 = malloc(pb.b1 * h / 2); }
    for (int i = 0; i < nf; i++) {
        if (!bgra) {
            if (fread(yuv, 1, w * h * 3 / 2, in) != (size_t)(w * h * 3 / 2)) break;
            for (int r = 0; r < h; r++) memcpy(pb.p0 + r * pb.b0, yuv + r * w, w);
            for (int r = 0; r < h / 2; r++) for (int x = 0; x < w / 2; x++) { pb.p1[r * pb.b1 + 2 * x] = yuv[w * h + r * w / 2 + x]; pb.p1[r * pb.b1 + 2 * x + 1] = yuv[w * h * 5 / 4 + r * w / 2 + x]; }
        }
        cm_time_t pts = {i * 40, 1000, 1, 0}, dur = {40, 1000, 1, 0};
        const char *k[1] = {"kVTEncodeFrameOptionKey_ForceKeyFrame"}; void *v[1] = {(void *)kCFBooleanTrue};
        if (VTCompressionSessionEncodeFrame(s, &pb, pts, dur, i == force ? dict(1, k, v) : 0, (void *)(long)i, 0)) { printf("vtenc FALHA quadro %d\n", i); return 1; }
    }
    VTCompressionSessionCompleteFrames(s, CMTimeMake(0, 0));
    VTCompressionSessionInvalidate(s);
    fclose(out);
    if (errs || nsamp != nf - drops || nsamp == 0) { printf("vtenc FALHA amostras %d de %d, descartes %d, erros %d\n", nsamp, nf, drops, errs); return 1; }
    printf("vtenc ok amostras=%d idr=%s notsync=%d\n", nsamp, idrs, notsync);
    return 0;
}
