/*
 * CoreMedia que o OpenBot (cua-driver, Sunshine, onnx) importa e que antes só devolvia erro.
 * Bloco, formato e sample buffer guardam os bytes, o timing e a imagem. Não há decodificador:
 * parameter set de H.264/HEVC é memória guardada, não um quadro decodificado.
 */
#include "foundation.h"

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
id objc_retain(id o);
void objc_release(id o);
void cx_objc_boot(unsigned long long sects, unsigned long long n);
int h264_sps_wh(const u8 *nal, int n, int *w, int *h);

typedef struct { i64 value; i32 timescale; u32 flags; i64 epoch; } cm_time_t;
typedef struct { cm_time_t duration, presentationTimeStamp, decodeTimeStamp; } cm_timing_t;
typedef struct { i32 w, h; } cm_dim_t;
typedef struct { u32 mNumberChannels; u32 mDataByteSize; void *mData; } audio_buf_t;
typedef struct { u32 mNumberBuffers; audio_buf_t mBuffers[1]; } abl_t;

#define CM_BAD_BLOCK (-12702)
#define CM_BAD_FMT (-12710)
#define CM_BAD_SAMPLE (-12743)
#define CM_TIME_VALID 1u

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cm_cf_t;
#define CFCONST(var, text) \
    static cm_cf_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_s;

CFCONST(kCMFormatDescriptionExtensionKey_MetadataKeyTable, "MetadataKeyTable")
CFCONST(kCMFormatDescriptionExtension_VerbatimSampleDescription, "VerbatimSampleDescription")
CFCONST(kCMMetadataFormatDescriptionKey_Namespace, "Namespace")
CFCONST(kCMMetadataFormatDescriptionKey_Value, "Value")
CFCONST(kCMMetadataFormatDescriptionKey_LocalID, "LocalID")
CFCONST(kCMMetadataFormatDescriptionKey_DataType, "DataType")
CFCONST(kCMMetadataFormatDescriptionKey_DataTypeNamespace, "DataTypeNamespace")
CFCONST(kCMMetadataFormatDescriptionKey_ConformingDataTypes, "ConformingDataTypes")
CFCONST(kCMMetadataFormatDescriptionKey_LanguageTag, "LanguageTag")
CFCONST(kCMMetadataFormatDescriptionKey_SetupData, "SetupData")
CFCONST(kCMMetadataFormatDescriptionKey_StructuralDependency, "StructuralDependency")
CFCONST(kCMMetadataFormatDescriptionMetadataSpecificationKey_Identifier, "Identifier")
CFCONST(kCMMetadataFormatDescriptionMetadataSpecificationKey_DataType, "DataType")
CFCONST(kCMMetadataFormatDescriptionMetadataSpecificationKey_ExtendedLanguageTag, "ExtendedLanguageTag")
CFCONST(kCMMetadataFormatDescriptionMetadataSpecificationKey_SetupData, "SetupData")
CFCONST(kCMMetadataFormatDescriptionMetadataSpecificationKey_StructuralDependency, "StructuralDependency")
CFCONST(kCMMetadataFormatDescription_StructuralDependencyKey_DependencyIsInvalidFlag, "DependencyIsInvalidFlag")

enum { KIND_BB = 1, KIND_FD = 2, KIND_SB = 3 };

typedef struct {
    cx_class_t *isa;
    u32 kind;
    u8 *data;
    size_t len, cap;
} bb_t;

typedef struct {
    cx_class_t *isa;
    u32 kind, media, subtype;
    i32 w, h;
    u8 asbd[40];
    int has_asbd;
    id ext;
    id specs;
    u8 *pset;
    size_t *poff, *psz;
    size_t np, nal;
} fd_t;

typedef struct {
    cx_class_t *isa;
    u32 kind;
    int valid, ready;
    bb_t *data;
    id image;
    fd_t *fmt;
    cm_timing_t *times;
    size_t *sizes;
    long ns;
    id attach;
    cm_time_t opt;
    int has_opt;
} sb_t;

static cx_class_t BB_c, BB_m, FD_c, FD_m, SB_c, SB_m;
static int cm_ready;

static void bb_dealloc(id s, SEL c) {
    bb_t *b = (bb_t *)s;
    free(b->data);
    b->data = 0;
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static void fd_dealloc(id s, SEL c) {
    fd_t *f = (fd_t *)s;
    if (f->ext) objc_release(f->ext);
    if (f->specs) objc_release(f->specs);
    free(f->pset);
    free(f->poff);
    free(f->psz);
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static void sb_dealloc(id s, SEL c) {
    sb_t *b = (sb_t *)s;
    if (b->data) objc_release((id)b->data);
    if (b->image) objc_release(b->image);
    if (b->fmt) objc_release((id)b->fmt);
    if (b->attach) objc_release(b->attach);
    free(b->times);
    free(b->sizes);
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(s, c);
}

static void cm_ensure(void) {
    if (cm_ready) return;
    cx_objc_boot(0, 0);
    cx_class_t *ob = cx_class_named("NSObject");
    if (!ob) return;
    cx_define_class(&BB_c, &BB_m, "CMBlockBuffer", ob, (u32)(sizeof(bb_t) - sizeof(void *)));
    cx_add_method(&BB_c, "dealloc", (void *)bb_dealloc);
    cx_define_class(&FD_c, &FD_m, "CMFormatDescription", ob, (u32)(sizeof(fd_t) - sizeof(void *)));
    cx_add_method(&FD_c, "dealloc", (void *)fd_dealloc);
    cx_define_class(&SB_c, &SB_m, "CMSampleBuffer", ob, (u32)(sizeof(sb_t) - sizeof(void *)));
    cx_add_method(&SB_c, "dealloc", (void *)sb_dealloc);
    cm_ready = 1;
}

static int is_a(void *p, cx_class_t *k, u32 kind) {
    if (!p || ((unsigned long long)p & 7)) return 0;
    return *(cx_class_t **)p == k && ((bb_t *)p)->kind == kind;
}
static bb_t *as_bb(void *p) { return is_a(p, &BB_c, KIND_BB) ? (bb_t *)p : 0; }
static fd_t *as_fd(void *p) { return is_a(p, &FD_c, KIND_FD) ? (fd_t *)p : 0; }
static sb_t *as_sb(void *p) { return is_a(p, &SB_c, KIND_SB) ? (sb_t *)p : 0; }

static bb_t *bb_new(void) {
    cm_ensure();
    bb_t *b = (bb_t *)cx_alloc_instance(&BB_c);
    if (b) b->kind = KIND_BB;
    return b;
}
static fd_t *fd_new(u32 media, u32 sub) {
    cm_ensure();
    fd_t *f = (fd_t *)cx_alloc_instance(&FD_c);
    if (!f) return 0;
    f->kind = KIND_FD;
    f->media = media;
    f->subtype = sub;
    f->ext = objc_retain(MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary"));
    return f;
}
static int bb_grow(bb_t *b, size_t need) {
    if (need <= b->cap) return 0;
    size_t ncap = b->cap ? b->cap : 64;
    while (ncap < need) ncap *= 2;
    u8 *p = realloc(b->data, ncap);
    if (!p) return -1;
    b->data = p;
    b->cap = ncap;
    return 0;
}

int CMBlockBufferCreateEmpty(void *a, u32 cap, u32 flags, void **out) {
    (void)a; (void)flags;
    if (!out) return CM_BAD_BLOCK;
    bb_t *b = bb_new();
    if (!b) return CM_BAD_BLOCK;
    if (cap && bb_grow(b, cap)) { objc_release((id)b); return CM_BAD_BLOCK; }
    *out = b;
    return 0;
}
int CMBlockBufferCreateWithMemoryBlock(void *a, void *mem, size_t len, void *ba, const void *cb, size_t off, size_t dlen, u32 flags, void **out) {
    (void)a; (void)ba; (void)cb; (void)flags;
    if (!out) return CM_BAD_BLOCK;
    size_t n = dlen ? dlen : len;
    if (off > len || n > len - off) return CM_BAD_BLOCK;
    bb_t *b = bb_new();
    if (!b) return CM_BAD_BLOCK;
    if (n && bb_grow(b, n)) { objc_release((id)b); return CM_BAD_BLOCK; }
    if (n && mem) memcpy(b->data, (u8 *)mem + off, n);
    else if (n) memset(b->data, 0, n);
    b->len = n;
    *out = b;
    return 0;
}
int CMBlockBufferCreateContiguous(void *a, void *src, void *ba, const void *cb, size_t off, size_t len, u32 flags, void **out) {
    bb_t *s = as_bb(src);
    if (!s) { if (out) *out = 0; return CM_BAD_BLOCK; }
    size_t n = len ? len : (off < s->len ? s->len - off : 0);
    return CMBlockBufferCreateWithMemoryBlock(a, s->data, s->len, ba, cb, off, n, flags, out);
}
int CMBlockBufferAppendMemoryBlock(void *b, void *mem, size_t len, void *ba, const void *cb, size_t off, size_t dlen, u32 flags) {
    (void)ba; (void)cb; (void)flags;
    bb_t *x = as_bb(b);
    if (!x) return CM_BAD_BLOCK;
    size_t n = dlen ? dlen : len;
    if (mem && (off > len || n > len - off)) return CM_BAD_BLOCK;
    if (bb_grow(x, x->len + n)) return CM_BAD_BLOCK;
    if (n && mem) memcpy(x->data + x->len, (u8 *)mem + off, n);
    else if (n) memset(x->data + x->len, 0, n);
    x->len += n;
    return 0;
}
int CMBlockBufferGetDataPointer(void *b, size_t off, size_t *lenAt, size_t *total, char **ptr) {
    bb_t *x = as_bb(b);
    if (!x || off > x->len) return CM_BAD_BLOCK;
    if (lenAt) *lenAt = x->len - off;
    if (total) *total = x->len;
    if (ptr) *ptr = (char *)(x->data ? x->data + off : 0);
    return 0;
}
size_t CMBlockBufferGetDataLength(void *b) { bb_t *x = as_bb(b); return x ? x->len : 0; }
u8 CMBlockBufferIsEmpty(void *b) { bb_t *x = as_bb(b); return !x || x->len == 0; }
u8 CMBlockBufferIsRangeContiguous(void *b, size_t off, size_t len) {
    bb_t *x = as_bb(b);
    if (!x) return 0;
    if (!len) len = off < x->len ? x->len - off : 0;
    return off <= x->len && len <= x->len - off;
}
int CMBlockBufferReplaceDataBytes(const void *src, void *b, size_t off, size_t len) {
    bb_t *x = as_bb(b);
    if (!x || !src || off > x->len || len > x->len - off) return CM_BAD_BLOCK;
    memcpy(x->data + off, src, len);
    return 0;
}
int CMBlockBufferCopyDataBytes(void *b, size_t off, size_t len, void *dst) {
    bb_t *x = as_bb(b);
    if (!x || !dst || off > x->len || len > x->len - off) return CM_BAD_BLOCK;
    if (len) memcpy(dst, x->data + off, len);
    return 0;
}
int CMBlockBufferAssureBlockMemory(void *b) {
    bb_t *x = as_bb(b);
    if (!x) return CM_BAD_BLOCK;
    if (!x->data && x->cap && bb_grow(x, x->cap)) return CM_BAD_BLOCK;
    return 0;
}

static int fd_store_sets(fd_t *f, size_t n, const u8 *const *p, const size_t *sz, int nal) {
    size_t total = 0;
    for (size_t i = 0; i < n; i++) total += sz ? sz[i] : 0;
    u8 *buf = total ? malloc(total) : 0;
    size_t *off = n ? calloc(n, sizeof(size_t)) : 0;
    size_t *ls = n ? calloc(n, sizeof(size_t)) : 0;
    if (n && (!off || !ls || (total && !buf))) { free(buf); free(off); free(ls); return -1; }
    size_t at = 0;
    for (size_t i = 0; i < n; i++) {
        off[i] = at;
        ls[i] = sz[i];
        if (sz[i] && p && p[i]) memcpy(buf + at, p[i], sz[i]);
        at += sz[i];
    }
    free(f->pset); free(f->poff); free(f->psz);
    f->pset = buf; f->poff = off; f->psz = ls; f->np = n; f->nal = (size_t)nal;
    return 0;
}
static int fd_get_set(fd_t *f, size_t i, const u8 **p, size_t *s, size_t *n, int *nal) {
    if (!f || i >= f->np) return CM_BAD_FMT;
    if (p) *p = f->pset ? f->pset + f->poff[i] : 0;
    if (s) *s = f->psz[i];
    if (n) *n = f->np;
    if (nal) *nal = (int)f->nal;
    return 0;
}

u64 CVPixelBufferGetWidth(void *p);
u64 CVPixelBufferGetHeight(void *p);
u32 CVPixelBufferGetPixelFormatType(void *p);

int CMVideoFormatDescriptionCreate(void *a, u32 codec, i32 w, i32 h, id ext, void **out) {
    (void)a;
    if (!out || w < 0 || h < 0) return CM_BAD_FMT;
    fd_t *f = fd_new(0x76696465 /* vide */, codec);
    if (!f) return CM_BAD_FMT;
    f->w = w; f->h = h;
    if (ext) {
        objc_release(f->ext);
        f->ext = objc_retain(ext);
    }
    *out = f;
    return 0;
}
int CMVideoFormatDescriptionCreateForImageBuffer(void *a, void *img, void **out) {
    if (!img) return CM_BAD_FMT;
    u32 fmt = CVPixelBufferGetPixelFormatType(img);
    return CMVideoFormatDescriptionCreate(a, fmt ? fmt : 0x42475241, (i32)CVPixelBufferGetWidth(img), (i32)CVPixelBufferGetHeight(img), 0, out);
}
int CMVideoFormatDescriptionCreateFromH264ParameterSets(void *a, size_t n, const u8 *const *p, const size_t *s, int nal, void **out) {
    (void)a;
    if (!out || !n || !p || !s) return CM_BAD_FMT;
    fd_t *f = fd_new(0x76696465, 0x61766331 /* avc1 */);
    if (!f || fd_store_sets(f, n, p, s, nal)) { if (f) objc_release((id)f); return CM_BAD_FMT; }
    for (size_t i = 0; i < n; i++) {
        if (!p[i] || s[i] < 1 || (p[i][0] & 31) != 7) continue;
        int sw = 0, sh = 0;
        if (h264_sps_wh(p[i], (int)s[i], &sw, &sh) == 0) { f->w = sw; f->h = sh; }
        break;
    }
    *out = f;
    return 0;
}
int CMVideoFormatDescriptionCreateFromHEVCParameterSets(void *a, size_t n, const u8 *const *p, const size_t *s, int nal, id ext, void **out) {
    (void)a; (void)ext;
    if (!out || !n || !p || !s) return CM_BAD_FMT;
    fd_t *f = fd_new(0x76696465, 0x68766331 /* hvc1 */);
    if (!f || fd_store_sets(f, n, p, s, nal)) { if (f) objc_release((id)f); return CM_BAD_FMT; }
    if (ext) { objc_release(f->ext); f->ext = objc_retain(ext); }
    *out = f;
    return 0;
}
int CMVideoFormatDescriptionGetH264ParameterSetAtIndex(void *d, size_t i, const u8 **p, size_t *s, size_t *n, int *nal) {
    fd_t *f = as_fd(d);
    if (!f || f->subtype != 0x61766331) return CM_BAD_FMT;
    return fd_get_set(f, i, p, s, n, nal);
}
int CMVideoFormatDescriptionGetHEVCParameterSetAtIndex(void *d, size_t i, const u8 **p, size_t *s, size_t *n, int *nal) {
    fd_t *f = as_fd(d);
    if (!f || f->subtype != 0x68766331) return CM_BAD_FMT;
    return fd_get_set(f, i, p, s, n, nal);
}
cm_dim_t CMVideoFormatDescriptionGetDimensions(void *d) {
    fd_t *f = as_fd(d);
    cm_dim_t r = {f ? f->w : 0, f ? f->h : 0};
    return r;
}
int CMAudioFormatDescriptionCreate(void *a, const void *asbd, size_t ls, const void *l, size_t cs, const void *c, id ext, void **out) {
    (void)a; (void)ls; (void)l; (void)cs; (void)c;
    if (!out || !asbd) return CM_BAD_FMT;
    fd_t *f = fd_new(0x736F756E /* soun */, 0);
    if (!f) return CM_BAD_FMT;
    memcpy(f->asbd, asbd, sizeof f->asbd);
    f->has_asbd = 1;
    if (ext) { objc_release(f->ext); f->ext = objc_retain(ext); }
    *out = f;
    return 0;
}
const void *CMAudioFormatDescriptionGetStreamBasicDescription(void *d) {
    fd_t *f = as_fd(d);
    return f && f->has_asbd ? f->asbd : 0;
}
u32 CMFormatDescriptionGetMediaSubType(void *d) { fd_t *f = as_fd(d); return f ? f->subtype : 0; }
u32 CMFormatDescriptionGetMediaType(void *d) { fd_t *f = as_fd(d); return f ? f->media : 0; }
id CMFormatDescriptionGetExtensions(void *d) { fd_t *f = as_fd(d); return f ? f->ext : 0; }
id CMFormatDescriptionGetExtension(void *d, id key) {
    fd_t *f = as_fd(d);
    if (!f || !f->ext || !key) return 0;
    return MSG(id, f->ext, "objectForKey:", key);
}

static fd_t *meta_new(u32 type, id specs) {
    fd_t *f = fd_new(0x6D657461 /* meta */, type);
    if (!f) return 0;
    if (specs) f->specs = objc_retain(specs);
    if (f->ext && specs) MSG(void, f->ext, "setObject:forKey:", specs, (id)kCMFormatDescriptionExtensionKey_MetadataKeyTable);
    return f;
}
int CMMetadataFormatDescriptionCreateWithMetadataSpecifications(void *a, u32 type, id specs, void **out) {
    (void)a;
    if (!out) return CM_BAD_FMT;
    fd_t *f = meta_new(type, specs);
    if (!f) return CM_BAD_FMT;
    *out = f;
    return 0;
}
int CMMetadataFormatDescriptionCreateWithKeys(void *a, u32 type, id keys, void **out) {
    return CMMetadataFormatDescriptionCreateWithMetadataSpecifications(a, type, keys, out);
}
int CMMetadataFormatDescriptionCreateWithMetadataFormatDescriptionAndMetadataSpecifications(void *a, void *base, id specs, void **out) {
    (void)a;
    fd_t *src = as_fd(base);
    if (!out || !src) return CM_BAD_FMT;
    id merged = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    if (src->specs) {
        u64 n = MSG0(u64, src->specs, "count");
        for (u64 i = 0; i < n; i++) {
            id it = MSG(id, src->specs, "objectAtIndex:", i);
            MSG(void, merged, "addObject:", it);
        }
    }
    if (specs) {
        u64 n = MSG0(u64, specs, "count");
        for (u64 i = 0; i < n; i++) {
            id it = MSG(id, specs, "objectAtIndex:", i);
            MSG(void, merged, "addObject:", it);
        }
    }
    fd_t *f = meta_new(src->subtype, merged);
    if (!f) return CM_BAD_FMT;
    *out = f;
    return 0;
}
int CMMetadataFormatDescriptionCreateByMergingMetadataFormatDescriptions(void *a, void *a1, void *a2, void **out) {
    fd_t *s2 = as_fd(a2);
    return CMMetadataFormatDescriptionCreateWithMetadataFormatDescriptionAndMetadataSpecifications(a, a1, s2 ? s2->specs : 0, out);
}
id CMMetadataFormatDescriptionGetIdentifiers(void *d) {
    fd_t *f = as_fd(d);
    if (!f || !f->specs) return 0;
    id arr = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    u64 n = MSG0(u64, f->specs, "count");
    for (u64 i = 0; i < n; i++) {
        id spec = MSG(id, f->specs, "objectAtIndex:", i);
        id ident = spec ? MSG(id, spec, "objectForKey:", (id)kCMMetadataFormatDescriptionMetadataSpecificationKey_Identifier) : 0;
        if (!ident) ident = spec ? MSG(id, spec, "objectForKey:", (id)kCMMetadataFormatDescriptionKey_Value) : 0;
        if (ident) MSG(void, arr, "addObject:", ident);
    }
    return arr;
}
id CMMetadataFormatDescriptionGetKeyWithLocalID(void *d, i32 local) {
    fd_t *f = as_fd(d);
    if (!f || !f->specs) return 0;
    u64 n = MSG0(u64, f->specs, "count");
    for (u64 i = 0; i < n; i++) {
        id spec = MSG(id, f->specs, "objectAtIndex:", i);
        id lid = spec ? MSG(id, spec, "objectForKey:", (id)kCMMetadataFormatDescriptionKey_LocalID) : 0;
        if (lid && MSG0(i64, lid, "longLongValue") == local) return spec;
    }
    return 0;
}

static cm_time_t time_invalid(void) { cm_time_t t = {0, 0, 0, 0}; return t; }
static sb_t *sb_new(void) {
    cm_ensure();
    sb_t *s = (sb_t *)cx_alloc_instance(&SB_c);
    if (!s) return 0;
    s->kind = KIND_SB;
    s->valid = 1;
    s->ready = 1;
    s->attach = objc_retain(MSG0(id, (id)cx_class_named("NSMutableArray"), "array"));
    id one = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    if (s->attach && one) MSG(void, s->attach, "addObject:", one);
    return s;
}
static int sb_copy_timing(sb_t *s, long ns, long nt, const cm_timing_t *t, long nsz, const size_t *sz) {
    if (ns < 0) ns = 0;
    if (!ns) ns = 1;
    s->ns = ns;
    s->times = calloc((size_t)ns, sizeof(cm_timing_t));
    s->sizes = calloc((size_t)ns, sizeof(size_t));
    if (!s->times || !s->sizes) return -1;
    for (long i = 0; i < ns; i++) {
        if (t && nt > 0) s->times[i] = t[nt == 1 ? 0 : (i < nt ? i : nt - 1)];
        else s->times[i].duration = s->times[i].presentationTimeStamp = s->times[i].decodeTimeStamp = time_invalid();
        if (sz && nsz == 1) s->sizes[i] = sz[0];
        else if (sz && i < nsz) s->sizes[i] = sz[i];
    }
    return 0;
}
int CMSampleBufferCreate(void *a, void *db, u8 ready, void *cb, void *ref, void *fd, long ns, long nt, const void *t, long nsz, const size_t *sz, void **out) {
    (void)a; (void)cb; (void)ref;
    if (!out) return CM_BAD_SAMPLE;
    sb_t *s = sb_new();
    if (!s) return CM_BAD_SAMPLE;
    s->ready = ready ? 1 : 0;
    if (db && as_bb(db)) s->data = (bb_t *)objc_retain((id)db);
    if (fd && as_fd(fd)) s->fmt = (fd_t *)objc_retain((id)fd);
    if (sb_copy_timing(s, ns, nt, (const cm_timing_t *)t, nsz, sz)) { objc_release((id)s); return CM_BAD_SAMPLE; }
    *out = s;
    return 0;
}
/* PCM s16 estéreo intercalado do AVCapture. O bloco copia os bytes; não há quadro de vídeo aqui. */
void *cx_cm_audio_sample(const short *pcm, unsigned frames, double rate) {
    if (!pcm || !frames) return 0;
    void *bb = 0;
    size_t nbytes = (size_t)frames * 4;
    if (CMBlockBufferCreateWithMemoryBlock(0, (void *)pcm, nbytes, 0, 0, 0, nbytes, 0, &bb) || !bb) return 0;
    cm_timing_t timing;
    memset(&timing, 0, sizeof timing);
    i32 scale = rate > 0 && rate < 200000 ? (i32)(rate + 0.5) : 48000;
    timing.duration.value = (i64)frames;
    timing.duration.timescale = scale;
    timing.duration.flags = 1;
    timing.presentationTimeStamp.timescale = scale;
    timing.presentationTimeStamp.flags = 1;
    timing.decodeTimeStamp = timing.presentationTimeStamp;
    size_t sz = nbytes;
    void *out = 0;
    if (CMSampleBufferCreate(0, bb, 1, 0, 0, 0, (long)frames, 1, &timing, 1, &sz, &out)) {
        objc_release((id)bb);
        return 0;
    }
    objc_release((id)bb);
    return out;
}
int CMSampleBufferCreateForImageBuffer(void *a, void *img, u8 ready, void *cb, void *ref, void *fd, const void *t, void **out) {
    (void)cb; (void)ref;
    if (!out || !img) return CM_BAD_SAMPLE;
    void *fmt = fd;
    int own = 0;
    if (!fmt) {
        if (CMVideoFormatDescriptionCreateForImageBuffer(a, img, &fmt) != 0) return CM_BAD_SAMPLE;
        own = 1;
    }
    size_t one = 1;
    int rc = CMSampleBufferCreate(a, 0, ready, 0, 0, fmt, 1, t ? 1 : 0, t, 1, &one, out);
    if (own && fmt) objc_release((id)fmt);
    if (rc == 0 && *out) {
        sb_t *s = as_sb(*out);
        if (s) s->image = objc_retain((id)img);
    }
    return rc;
}
int CMSampleBufferCreateReadyWithImageBuffer(void *a, void *img, void *fd, const void *timing, void **out) {
    return CMSampleBufferCreateForImageBuffer(a, img, 1, 0, 0, fd, timing, out);
}
int CMSampleBufferCreateCopyWithNewTiming(void *a, void *src, long nt, const void *t, void **out) {
    sb_t *s = as_sb(src);
    if (!s || !out) return CM_BAD_SAMPLE;
    int rc = CMSampleBufferCreate(a, s->data, (u8)s->ready, 0, 0, s->fmt, s->ns, nt, t, s->ns, s->sizes, out);
    if (rc == 0 && *out && s->image) ((sb_t *)*out)->image = objc_retain(s->image);
    return rc;
}

long CMSampleBufferGetNumSamples(void *s) { sb_t *b = as_sb(s); return b ? b->ns : 0; }
void *CMSampleBufferGetFormatDescription(void *s) { sb_t *b = as_sb(s); return b ? b->fmt : 0; }
void *CMSampleBufferGetDataBuffer(void *s) { sb_t *b = as_sb(s); return b ? b->data : 0; }
void *CMSampleBufferGetImageBuffer(void *s) { sb_t *b = as_sb(s); return b ? b->image : 0; }
cm_time_t CMSampleBufferGetPresentationTimeStamp(void *s) {
    sb_t *b = as_sb(s);
    return b && b->times ? b->times[0].presentationTimeStamp : time_invalid();
}
cm_time_t CMSampleBufferGetDecodeTimeStamp(void *s) {
    sb_t *b = as_sb(s);
    return b && b->times ? b->times[0].decodeTimeStamp : time_invalid();
}
cm_time_t CMSampleBufferGetDuration(void *s) {
    sb_t *b = as_sb(s);
    return b && b->times ? b->times[0].duration : time_invalid();
}
cm_time_t CMSampleBufferGetOutputPresentationTimeStamp(void *s) {
    sb_t *b = as_sb(s);
    if (!b) return time_invalid();
    if (b->has_opt) return b->opt;
    return CMSampleBufferGetPresentationTimeStamp(s);
}
int CMSampleBufferSetOutputPresentationTimeStamp(void *s, cm_time_t t) {
    sb_t *b = as_sb(s);
    if (!b) return CM_BAD_SAMPLE;
    b->opt = t;
    b->has_opt = 1;
    return 0;
}
int CMSampleBufferGetSampleTimingInfo(void *s, long i, cm_timing_t *out) {
    sb_t *b = as_sb(s);
    if (!b || !out || i < 0 || i >= b->ns || !b->times) return CM_BAD_SAMPLE;
    *out = b->times[i];
    return 0;
}
size_t CMSampleBufferGetSampleSize(void *s, long i) {
    sb_t *b = as_sb(s);
    if (!b || i < 0 || i >= b->ns) return 0;
    if (b->sizes && b->sizes[i]) return b->sizes[i];
    return b->ns ? CMBlockBufferGetDataLength(b->data) / (size_t)b->ns : 0;
}
size_t CMSampleBufferGetTotalSampleSize(void *s) {
    sb_t *b = as_sb(s);
    if (!b) return 0;
    size_t n = 0;
    for (long i = 0; i < b->ns; i++) n += CMSampleBufferGetSampleSize(s, i);
    if (!n) n = CMBlockBufferGetDataLength(b->data);
    return n;
}
u8 CMSampleBufferDataIsReady(void *s) { sb_t *b = as_sb(s); return b && b->valid && b->ready; }
u8 CMSampleBufferIsValid(void *s) { sb_t *b = as_sb(s); return b && b->valid; }
int CMSampleBufferMakeDataReady(void *s) {
    sb_t *b = as_sb(s);
    if (!b || !b->valid) return CM_BAD_SAMPLE;
    b->ready = 1;
    return 0;
}
int CMSampleBufferInvalidate(void *s) {
    sb_t *b = as_sb(s);
    if (!b) return CM_BAD_SAMPLE;
    b->valid = 0;
    b->ready = 0;
    return 0;
}
id CMSampleBufferGetSampleAttachmentsArray(void *s, u8 create) {
    sb_t *b = as_sb(s);
    if (!b) return 0;
    if (!b->attach && create) b->attach = objc_retain(MSG0(id, (id)cx_class_named("NSMutableArray"), "array"));
    return b->attach;
}

static id sb_dict(sb_t *b) {
    if (!b) return 0;
    if (!b->attach) b->attach = objc_retain(MSG0(id, (id)cx_class_named("NSMutableArray"), "array"));
    id d = b->attach ? MSG(id, b->attach, "objectAtIndex:", (u64)0) : 0;
    if (!d && b->attach) {
        d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
        if (d) MSG(void, b->attach, "addObject:", d);
    }
    return d;
}
void CMSetAttachment(void *target, id key, id value, u32 mode) {
    (void)mode;
    sb_t *b = as_sb(target);
    if (b) {
        id d = sb_dict(b);
        if (!d || !key) return;
        if (value) MSG(void, d, "setObject:forKey:", value, key);
        else MSG(void, d, "removeObjectForKey:", key);
        return;
    }
    fd_t *f = as_fd(target);
    if (f && f->ext && key) {
        if (value) MSG(void, f->ext, "setObject:forKey:", value, key);
        else MSG(void, f->ext, "removeObjectForKey:", key);
    }
}
id CMCopyDictionaryOfAttachments(void *a, void *target, u32 mode) {
    (void)a; (void)mode;
    id src = 0;
    sb_t *b = as_sb(target);
    if (b) src = sb_dict(b);
    fd_t *f = as_fd(target);
    if (f) src = f->ext;
    cx_class_t *mk = cx_class_named("NSMutableDictionary");
    id d = mk ? cx_alloc_instance(mk) : 0;
    if (src && d) {
        id keys = MSG0(id, src, "allKeys");
        u64 n = keys ? MSG0(u64, keys, "count") : 0;
        for (u64 i = 0; i < n; i++) {
            id k = MSG(id, keys, "objectAtIndex:", i);
            id v = MSG(id, src, "objectForKey:", k);
            MSG(void, d, "setObject:forKey:", v, k);
        }
    }
    return d;
}

static int pcm_copy_out(sb_t *b, abl_t *list) {
    if (!b->data || !list || !list->mNumberBuffers) return CM_BAD_SAMPLE;
    size_t len = b->data->len;
    audio_buf_t *dst = &list->mBuffers[0];
    if (!dst->mData || dst->mDataByteSize < len) return CM_BAD_SAMPLE;
    if (len) memcpy(dst->mData, b->data->data, len);
    dst->mDataByteSize = (u32)len;
    return 0;
}
int CMSampleBufferCopyPCMDataIntoAudioBufferList(void *s, long frame, long frames, abl_t *list) {
    (void)frame; (void)frames;
    sb_t *b = as_sb(s);
    if (!b || !b->valid) return CM_BAD_SAMPLE;
    return pcm_copy_out(b, list);
}
int CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(void *s, size_t *need, abl_t *list, size_t listSize, void *sa, void *ma, u32 flags, void **blockOut) {
    (void)sa; (void)ma; (void)flags;
    sb_t *b = as_sb(s);
    if (!b || !b->data) return CM_BAD_SAMPLE;
    size_t bytes = sizeof(u32) + sizeof(audio_buf_t);
    if (need) *need = bytes;
    if (!list) return 0;
    if (listSize < bytes) return CM_BAD_SAMPLE;
    list->mNumberBuffers = 1;
    list->mBuffers[0].mNumberChannels = 0;
    list->mBuffers[0].mDataByteSize = (u32)b->data->len;
    list->mBuffers[0].mData = b->data->data;
    if (blockOut) *blockOut = objc_retain((id)b->data);
    return 0;
}
