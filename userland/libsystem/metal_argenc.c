/* mOS libSystem — MTLArgumentEncoder do Metal por software (argument buffers).
 *
 * O interpretador lê um argument buffer como a struct do shader: ponteiro device/constant, textura e sampler são 8
 * bytes (o endereço do buffer, do msl_tex_t e do msl_samp_t), e os dados comuns ficam no seu tamanho e alinhamento.
 * O encoder grava esses valores nos deslocamentos de cada índice:
 *  - newArgumentEncoderWithBufferIndex: (MTLFunction): o layout é a struct do parâmetro [[buffer(n)]], com o índice
 *    de cada membro pelo [[id(n)]] (sem id, o seguinte ao anterior; arrays ocupam um índice por elemento);
 *  - newArgumentEncoderWithArguments: (MTLDevice): o layout vem dos MTLArgumentDescriptor, em ordem de índice. */
#include "metalint.h"
#include "mtlint.h"

u64 mtl_dget(id o, const char *name);
void qsort(void *b, size_t n, size_t sz, int (*cmp)(const void *, const void *));
id objc_retain(id o);
void objc_release(id o);

typedef struct { u32 index, off, stride, count, kind; } aslot_t;   /* kind: 0 dado, 1 ponteiro, 2 textura, 3 sampler, 4 objeto */
typedef struct {
    MHDR;
    aslot_t *s;
    u32 ns;
    u64 len, align;
    u8 *dst;                   /* setArgumentBuffer: (contents + offset) */
} maenc_t;
static cx_class_t MAEnc_c, MAEnc_m;

static aslot_t *slot_of(maenc_t *e, u64 index, u64 *elem) {
    for (u32 i = 0; i < e->ns; i++) if (index >= e->s[i].index && index < e->s[i].index + e->s[i].count) { *elem = index - e->s[i].index; return &e->s[i]; }
    return 0;
}
static void put_ptr(maenc_t *e, u64 index, const void *p) {
    u64 el;
    aslot_t *s = e->dst ? slot_of(e, index, &el) : 0;
    if (!s || s->kind == 0) return;
    u64 v = (u64)(uintptr_t)p;
    memcpy(e->dst + s->off + el * s->stride, &v, 8);
}
static void ae_set_ab(id self, SEL c, id buf, u64 off) {
    (void)c;
    maenc_t *e = (maenc_t *)self;
    mbuf_t *b = (mbuf_t *)buf;
    e->dst = b && b->p && off + e->len <= b->len ? b->p + off : 0;
}
static void ae_set_ab_el(id self, SEL c, id buf, u64 start, u64 el) { ae_set_ab(self, c, buf, start + el * ((maenc_t *)self)->len); }
static void ae_buf(id self, SEL c, id buf, u64 off, u64 i) { (void)c; mbuf_t *b = (mbuf_t *)buf; put_ptr((maenc_t *)self, i, b && b->p ? b->p + off : 0); }
typedef struct { u64 loc, len; } nsrange_t;
static void ae_bufs(id self, SEL c, const id *bufs, const u64 *offs, nsrange_t r) {
    for (u64 k = 0; k < r.len; k++) ae_buf(self, c, bufs ? bufs[k] : 0, offs ? offs[k] : 0, r.loc + k);
}
static void ae_tex(id self, SEL c, id tex, u64 i) { (void)c; put_ptr((maenc_t *)self, i, tex ? (void *)&((mtex_t *)tex)->t : 0); }
static void ae_texs(id self, SEL c, const id *t, nsrange_t r) { for (u64 k = 0; k < r.len; k++) ae_tex(self, c, t ? t[k] : 0, r.loc + k); }
static void ae_samp(id self, SEL c, id smp, u64 i) { (void)c; put_ptr((maenc_t *)self, i, smp ? (void *)&((msamp_t *)smp)->s : 0); }
static void ae_samps(id self, SEL c, const id *t, nsrange_t r) { for (u64 k = 0; k < r.len; k++) ae_samp(self, c, t ? t[k] : 0, r.loc + k); }
static void ae_obj(id self, SEL c, id o, u64 i) { (void)c; put_ptr((maenc_t *)self, i, o); }   /* pipelines, ICB, aceleração */
static void *ae_const(id self, SEL c, u64 i) {
    (void)c;
    maenc_t *e = (maenc_t *)self;
    u64 el;
    aslot_t *s = e->dst ? slot_of(e, i, &el) : 0;
    return s ? e->dst + s->off + el * s->stride : 0;
}
static u64 ae_len(id self, SEL c) { (void)c; return ((maenc_t *)self)->len; }
static u64 ae_align(id self, SEL c) { (void)c; return ((maenc_t *)self)->align; }
static void ae_dealloc(id self, SEL c) {
    free(((maenc_t *)self)->s);
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}
static maenc_t *ae_new(id dev) {
    maenc_t *e = (maenc_t *)cx_alloc_instance(&MAEnc_c);
    e->dev = dev;
    return e;
}
static int add_slot(maenc_t *e, aslot_t s) {
    aslot_t *n = realloc(e->s, sizeof *n * (e->ns + 1));
    if (!n) return 0;
    e->s = n;
    e->s[e->ns++] = s;
    return 1;
}

/* ---- pela struct do shader */
static u32 kind_of_ty(ty_t *t) { return t->k == K_PTR ? 1 : t->k == K_TEX ? 2 : t->k == K_SAMP ? 3 : 0; }
static id fn_argenc(id self, SEL c, u64 bufidx) {
    (void)c;
    mfunc_t *f = (mfunc_t *)self;
    msl_fn_t *fn = f ? f->fn : 0;
    if (!fn) return 0;
    ty_t *st = 0;
    for (int i = 0; i < fn->np; i++) {
        var_t *p = fn->params[i];
        if (p->attr != A_BUFFER || (u64)p->idx != bufidx) continue;
        st = p->t->k == K_PTR ? p->t->e : p->t;
    }
    if (!st || st->k != K_STRUCT) return 0;
    maenc_t *e = ae_new(f->dev);
    u32 next = 0;
    for (int i = 0; i < st->nf; i++) {
        fld_t *fl = &st->f[i];
        ty_t *t = fl->t;
        u32 count = 1, stride = (u32)t->size;
        if (t->k == K_ARRAY) { count = (u32)t->n; t = t->e; stride = (u32)t->size; }
        u32 index = fl->attr == A_ARG_ID ? (u32)fl->idx : next;
        aslot_t s = {index, (u32)fl->off, stride, count, kind_of_ty(t)};
        if (!add_slot(e, s)) { objc_release((id)e); return 0; }
        next = index + count;
    }
    e->len = (u64)st->size;
    e->align = (u64)(st->align > 8 ? st->align : 8);
    return (id)e;
}
static id fn_argenc_refl(id self, SEL c, u64 bufidx, id *refl) { if (refl) *refl = 0; return fn_argenc(self, c, bufidx); }

/* ---- pelos MTLArgumentDescriptor (MTLDataType) */
static void dt_size(u64 dt, u32 *size, u32 *align, u32 *kind) {
    *kind = 0;
    switch (dt) {
    case 60: *size = *align = 8; *kind = 1; return;                       /* Pointer */
    case 58: *size = *align = 8; *kind = 2; return;                       /* Texture */
    case 59: *size = *align = 8; *kind = 3; return;                       /* Sampler */
    case 78: case 79: case 80: case 116: *size = *align = 8; *kind = 4; return;   /* pipelines, ICB, aceleração */
    }
    /* escalares e vetores: Float 3..6, Half 16..19, Int 29..32, UInt 33..36, Short 37..40, UShort 41..44,
     * Char 45..48, UChar 49..52, Bool 53..56, Long 81..84, ULong 85..88 */
    struct { u64 a, b; u32 sz; } R[] = {{3, 6, 4}, {16, 19, 2}, {29, 32, 4}, {33, 36, 4}, {37, 40, 2}, {41, 44, 2}, {45, 48, 1}, {49, 52, 1}, {53, 56, 1}, {81, 84, 8}, {85, 88, 8}};
    for (unsigned i = 0; i < sizeof R / sizeof R[0]; i++)
        if (dt >= R[i].a && dt <= R[i].b) {
            u32 n = (u32)(dt - R[i].a) + 1, nn = n == 3 ? 4 : n;   /* vetor de 3 ocupa o de 4 */
            *size = R[i].sz * nn;
            *align = *size;
            return;
        }
    /* matrizes Float2x2..4x4 (7..15): colunas de vetor */
    if (dt >= 7 && dt <= 15) { u32 cols = (u32)(dt - 7) / 3 + 2, rows = (u32)(dt - 7) % 3 + 2, rr = rows == 3 ? 4 : rows; *size = cols * rr * 4; *align = rr * 4; return; }
    *size = *align = 4;
}
static int cmp_idx(const void *a, const void *b) { u64 x = ((const u64 *)a)[0], y = ((const u64 *)b)[0]; return x < y ? -1 : x > y; }
static id dev_argenc(id self, SEL c, id arr) {
    (void)c;
    u64 n = arr ? MSG0(u64, arr, "count") : 0;
    if (!n || n > 1024) return 0;
    u64 (*d)[2] = malloc(sizeof *d * n);
    if (!d) return 0;
    for (u64 i = 0; i < n; i++) { id a = MSG(id, arr, "objectAtIndex:", i); d[i][0] = mtl_dget(a, "index"); d[i][1] = (u64)(uintptr_t)a; }
    qsort(d, n, sizeof *d, cmp_idx);
    maenc_t *e = ae_new(self);
    u64 off = 0, al = 8;
    for (u64 i = 0; i < n; i++) {
        id a = (id)(uintptr_t)d[i][1];
        u32 size, align, kind;
        dt_size(mtl_dget(a, "dataType"), &size, &align, &kind);
        u64 cnt = mtl_dget(a, "arrayLength");
        if (!cnt) cnt = 1;
        off = (off + align - 1) / align * align;
        aslot_t s = {(u32)d[i][0], (u32)off, size, (u32)cnt, kind};
        if (!add_slot(e, s)) { free(d); objc_release((id)e); return 0; }
        off += (u64)size * cnt;
        if (align > al) al = align;
    }
    free(d);
    e->len = (off + al - 1) / al * al;
    e->align = al;
    return (id)e;
}

void mtl_argenc_register(cx_class_t *obj, cx_class_t *dev, cx_class_t *fn) {
    cx_define_class(&MAEnc_c, &MAEnc_m, "MTLSoftArgumentEncoder", obj, sizeof(maenc_t) - sizeof(mobj_t));
    cx_add_method(&MAEnc_c, "dealloc", (void *)ae_dealloc);
    cx_add_method(&MAEnc_c, "encodedLength", (void *)ae_len);
    cx_add_method(&MAEnc_c, "alignment", (void *)ae_align);
    cx_add_method(&MAEnc_c, "setArgumentBuffer:offset:", (void *)ae_set_ab);
    cx_add_method(&MAEnc_c, "setArgumentBuffer:startOffset:arrayElement:", (void *)ae_set_ab_el);
    cx_add_method(&MAEnc_c, "setBuffer:offset:atIndex:", (void *)ae_buf);
    cx_add_method(&MAEnc_c, "setBuffers:offsets:withRange:", (void *)ae_bufs);
    cx_add_method(&MAEnc_c, "setTexture:atIndex:", (void *)ae_tex);
    cx_add_method(&MAEnc_c, "setTextures:withRange:", (void *)ae_texs);
    cx_add_method(&MAEnc_c, "setSamplerState:atIndex:", (void *)ae_samp);
    cx_add_method(&MAEnc_c, "setSamplerStates:withRange:", (void *)ae_samps);
    cx_add_method(&MAEnc_c, "setRenderPipelineState:atIndex:", (void *)ae_obj);
    cx_add_method(&MAEnc_c, "setComputePipelineState:atIndex:", (void *)ae_obj);
    cx_add_method(&MAEnc_c, "setIndirectCommandBuffer:atIndex:", (void *)ae_obj);
    cx_add_method(&MAEnc_c, "setAccelerationStructure:atIndex:", (void *)ae_obj);
    cx_add_method(&MAEnc_c, "setVisibleFunctionTable:atIndex:", (void *)ae_obj);
    cx_add_method(&MAEnc_c, "setIntersectionFunctionTable:atIndex:", (void *)ae_obj);
    cx_add_method(&MAEnc_c, "constantDataAtIndex:", (void *)ae_const);
    cx_add_method(dev, "newArgumentEncoderWithArguments:", (void *)dev_argenc);
    cx_add_method(fn, "newArgumentEncoderWithBufferIndex:", (void *)fn_argenc);
    cx_add_method(fn, "newArgumentEncoderWithBufferIndex:reflection:", (void *)fn_argenc_refl);
}
