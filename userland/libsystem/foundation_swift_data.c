/*
 * Símbolos Swift de Foundation que o VS Code e o OpenBot importam.
 *
 * Data no x86_64 (enum de 16 bytes, spare bits do ponteiro Swift 0xFF00000000000007):
 * a tag fica nos dois bits altos do último byte.
 *   0x00 inline: 14 bytes + comprimento no byte 14
 *   0x40 slice: Range<Int32> + __DataStorage (byte alto 0x40)
 *   0x80 large: RangeReference + __DataStorage (byte alto 0x80)
 *   0xC0 vazio
 * String pequena: 16 bytes, último 0xE0|n. String grande nossa: ponteiro em w0 e
 * w1 = 0x80<<56 | n. URL é um ponteiro para NSURL.
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#ifdef FDN_HOST
typedef void *id;
typedef void *SEL;
id fdn_host_msg(id o, const char *sel, uintptr_t a, uintptr_t b, int n);
#define MSG0(T, o, s) ((T)(uintptr_t)fdn_host_msg((o), (s), 0, 0, 0))
#define MSG(T, o, s, a, ...) ((T)(uintptr_t)fdn_host_msg((o), (s), (uintptr_t)(a), 0, 1))
id cx_str(const char *s);
id cx_str_n(const char *s, unsigned long n);
const char *cx_cstr(id o);
id objc_retain(id o);
void objc_release(id o);
#else
#include "foundation.h"
#include "libc.h"
id objc_retain(id o);
void objc_release(id o);
extern cx_class_t NSData_c __asm__("OBJC_CLASS_$_NSData");
extern cx_class_t NSURL_c __asm__("OBJC_CLASS_$_NSURL");
extern cx_class_t NSArray_c __asm__("OBJC_CLASS_$_NSArray");
extern cx_class_t NSMutableArray_c __asm__("OBJC_CLASS_$_NSMutableArray");
extern cx_class_t NSDictionary_c __asm__("OBJC_CLASS_$_NSDictionary");
extern cx_class_t NSMutableDictionary_c __asm__("OBJC_CLASS_$_NSMutableDictionary");
#endif

typedef struct { uint64_t w0, w1; } swstr;

typedef struct ds {
    void *isa;
    uint64_t rc;
    uint8_t *bytes;
    int64_t length;
    int64_t capacity;
    int64_t offset;
} ds_t;

typedef struct rr {
    void *isa;
    uint64_t rc;
    int64_t lower;
    int64_t upper;
} rr_t;

typedef struct i8arr {
    void *isa;
    uint64_t rc;
    int64_t count;
    int64_t cap;
    int8_t el[];
} i8arr_t;

static void *md_storage;
static void *md_range;
static void *md_url;
static void *md_enc;

void *fdn_storage_metadata(void) { return &md_storage; }
void *fdn_range_metadata(void) { return &md_range; }
void *fdn_url_metadata(void) { return &md_url; }
void *fdn_encoding_metadata(void) { return &md_enc; }

static void *tag_ptr(void *p, unsigned top) {
    uintptr_t v = (uintptr_t)p & 0x00FFFFFFFFFFFFFFULL;
    return (void *)(v | ((uintptr_t)top << 56));
}
static void *untag(const void *p) {
    return (void *)((uintptr_t)p & 0x00FFFFFFFFFFFFFFULL);
}
static unsigned rep_tag(const uint8_t *r) { return r[15] & 0xC0; }

static ds_t *ds_alloc(const void *src, int64_t n, int64_t off) {
    ds_t *d;
    int64_t cap;
    if (n < 0) n = 0;
    cap = n < 16 ? 16 : n;
    d = (ds_t *)calloc(1, sizeof *d);
    if (!d) return 0;
    d->isa = &md_storage;
    d->rc = 1;
    d->bytes = (uint8_t *)malloc((size_t)cap);
    if (!d->bytes) { free(d); return 0; }
    if (src && n) memcpy(d->bytes, src, (size_t)n);
    else memset(d->bytes, 0, (size_t)cap);
    d->length = n;
    d->capacity = cap;
    d->offset = off;
    return d;
}

static void ds_grow(ds_t *d, int64_t need) {
    uint8_t *p;
    int64_t cap;
    if (!d || need <= d->capacity) return;
    cap = d->capacity ? d->capacity : 16;
    while (cap < need) cap *= 2;
    p = (uint8_t *)realloc(d->bytes, (size_t)cap);
    if (!p) return;
    d->bytes = p;
    d->capacity = cap;
}

ds_t *fdn_storage_init(const void *bytes, int64_t length) {
    return ds_alloc(bytes, length, 0);
}
void *fdn_storage_bytes(ds_t *d) { return d ? d->bytes : 0; }
int64_t fdn_storage_length(ds_t *d) { return d ? d->length : 0; }
int64_t fdn_storage_offset(ds_t *d) { return d ? d->offset : 0; }

static void rep_clear(uint8_t *r) { memset(r, 0, 16); r[15] = 0xC0; }

static void rep_set_inline(uint8_t *r, const uint8_t *p, int n) {
    memset(r, 0, 16);
    if (n > 0 && p) memcpy(r, p, (size_t)n);
    r[14] = (uint8_t)n;
}

static void rep_set_slice(uint8_t *r, ds_t *d, int64_t lo, int64_t hi) {
    int32_t a = (int32_t)lo, b = (int32_t)hi;
    void *p = tag_ptr(d, 0x40);
    memset(r, 0, 16);
    memcpy(r, &a, 4);
    memcpy(r + 4, &b, 4);
    memcpy(r + 8, &p, 8);
}

static void rep_set_large(uint8_t *r, ds_t *d, int64_t lo, int64_t hi) {
    rr_t *rr = (rr_t *)calloc(1, sizeof *rr);
    void *p;
    memset(r, 0, 16);
    if (!rr) { rep_clear(r); return; }
    rr->isa = &md_range;
    rr->rc = 1;
    rr->lower = lo;
    rr->upper = hi;
    p = tag_ptr(d, 0x80);
    memcpy(r, &rr, 8);
    memcpy(r + 8, &p, 8);
}

static int rep_inline_len(const uint8_t *r) { return r[14]; }

static void rep_slice_bounds(const uint8_t *r, int64_t *lo, int64_t *hi, ds_t **d) {
    int32_t a, b;
    void *p;
    memcpy(&a, r, 4);
    memcpy(&b, r + 4, 4);
    memcpy(&p, r + 8, 8);
    *lo = a;
    *hi = b;
    *d = (ds_t *)untag(p);
}

static void rep_large_bounds(const uint8_t *r, int64_t *lo, int64_t *hi, ds_t **d) {
    rr_t *rr;
    void *p;
    memcpy(&rr, r, 8);
    memcpy(&p, r + 8, 8);
    *lo = rr ? rr->lower : 0;
    *hi = rr ? rr->upper : 0;
    *d = (ds_t *)untag(p);
}

static int rep_materialize(const uint8_t *r, const uint8_t **out, int64_t *n, uint8_t *tmp, int tmpn) {
    unsigned t = rep_tag(r);
    if (t == 0xC0) { *out = 0; *n = 0; return 1; }
    if (t == 0) {
        int n0 = rep_inline_len(r);
        if (n0 > tmpn) n0 = tmpn;
        memcpy(tmp, r, (size_t)n0);
        *out = tmp;
        *n = n0;
        return 1;
    }
    {
        int64_t lo, hi;
        ds_t *d;
        if (t == 0x40) rep_slice_bounds(r, &lo, &hi, &d);
        else rep_large_bounds(r, &lo, &hi, &d);
        if (!d || !d->bytes || hi < lo) { *out = 0; *n = 0; return 0; }
        *out = d->bytes + (lo - d->offset);
        *n = hi - lo;
        return 1;
    }
}

void fdn_rep_from_bytes(uint8_t r[16], const void *bytes, int64_t n) {
    if (n <= 0) { rep_clear(r); return; }
    if (n <= 14) { rep_set_inline(r, (const uint8_t *)bytes, (int)n); return; }
    if (n < 0x7fffffff) {
        ds_t *d = ds_alloc(bytes, n, 0);
        if (!d) { rep_clear(r); return; }
        rep_set_slice(r, d, 0, n);
        return;
    }
    {
        ds_t *d = ds_alloc(bytes, n, 0);
        if (!d) { rep_clear(r); return; }
        rep_set_large(r, d, 0, n);
    }
}

void fdn_rep_append(uint8_t r[16], const void *bytes, int64_t n) {
    uint8_t tmp[14];
    const uint8_t *cur;
    int64_t cn;
    uint8_t *joined;
    if (n <= 0) return;
    if (!rep_materialize(r, &cur, &cn, tmp, 14)) return;
    if (rep_tag(r) == 0 && cn + n <= 14) {
        uint8_t buf[14];
        if (cn && cur) memcpy(buf, cur, (size_t)cn);
        memcpy(buf + cn, bytes, (size_t)n);
        rep_set_inline(r, buf, (int)(cn + n));
        return;
    }
    if (rep_tag(r) == 0x40 || rep_tag(r) == 0x80) {
        int64_t lo, hi;
        ds_t *d;
        if (rep_tag(r) == 0x40) rep_slice_bounds(r, &lo, &hi, &d);
        else rep_large_bounds(r, &lo, &hi, &d);
        if (d && hi == d->length && lo == d->offset) {
            ds_grow(d, d->length + n);
            memcpy(d->bytes + d->length, bytes, (size_t)n);
            d->length += n;
            if (rep_tag(r) == 0x40 && d->length < 0x7fffffff) {
                rep_set_slice(r, d, lo, d->length);
                return;
            }
            if (rep_tag(r) == 0x80) {
                rr_t *rr;
                memcpy(&rr, r, 8);
                if (rr) rr->upper = d->length;
                return;
            }
        }
    }
    joined = (uint8_t *)malloc((size_t)(cn + n));
    if (!joined) return;
    if (cn && cur) memcpy(joined, cur, (size_t)cn);
    memcpy(joined + cn, bytes, (size_t)n);
    fdn_rep_from_bytes(r, joined, cn + n);
    free(joined);
}

int fdn_rep_copy(const uint8_t r[16], void *dst, int64_t dst_bytes, int64_t lo, int64_t hi, int has_range) {
    uint8_t tmp[14];
    const uint8_t *cur;
    int64_t cn, from, count;
    if (!dst || dst_bytes <= 0) return 0;
    if (!rep_materialize(r, &cur, &cn, tmp, 14)) return 0;
    if (cn <= 0 || !cur) return 0;
    if (!has_range) {
        from = 0;
        count = cn < dst_bytes ? cn : dst_bytes;
    } else {
        if (hi < lo) return 0;
        if (lo < 0) lo = 0;
        if (hi > cn) hi = cn;
        from = lo;
        count = hi - lo;
        if (count > dst_bytes) count = dst_bytes;
    }
    if (count <= 0) return 0;
    memcpy(dst, cur + from, (size_t)count);
    return (int)count;
}

swstr fdn_small(const char *s, int n) {
    unsigned char b[16];
    swstr r;
    memset(b, 0, 16);
    if (n < 0) n = 0;
    if (n > 15) n = 15;
    if (s && n) memcpy(b, s, (size_t)n);
    b[15] = (unsigned char)(0xE0 | n);
    memcpy(&r.w0, b, 8);
    memcpy(&r.w1, b + 8, 8);
    return r;
}

swstr fdn_pack(const char *s, int n) {
    struct { int64_t n; char b[]; } *h;
    swstr r;
    if (!s) n = 0;
    if (n < 0) n = (int)strlen(s);
    if (n <= 15) return fdn_small(s, n);
    h = malloc(sizeof *h + (size_t)n + 1);
    if (!h) return fdn_small(s, 15);
    h->n = n;
    memcpy(h->b, s, (size_t)n);
    h->b[n] = 0;
    r.w0 = (uint64_t)(uintptr_t)h;
    r.w1 = (0x80ULL << 56) | (uint64_t)n;
    return r;
}

int fdn_unpack(swstr s, char *buf, int cap) {
    unsigned char b[16];
    memcpy(b, &s.w0, 8);
    memcpy(b + 8, &s.w1, 8);
    if ((b[15] & 0xF0) == 0xE0) {
        int n = b[15] & 0x0F;
        if (n >= cap) n = cap - 1;
        if (n > 0) memcpy(buf, b, (size_t)n);
        buf[n] = 0;
        return n;
    }
    if ((s.w1 >> 56) == 0x80 && s.w0) {
        struct { int64_t n; char b[]; } *h = (void *)(uintptr_t)s.w0;
        int n = (int)h->n;
        if (n >= cap) n = cap - 1;
        if (n > 0) memcpy(buf, h->b, (size_t)n);
        buf[n] = 0;
        return n;
    }
    buf[0] = 0;
    return -1;
}

id fdn_bridge_string(swstr s) {
    char buf[4096];
    int n = fdn_unpack(s, buf, (int)sizeof buf);
    if (n < 0) return cx_str("");
    return cx_str_n(buf, (unsigned long)n);
}

swstr fdn_string_from_ns(id o) {
    const char *c;
    if (!o) return fdn_small("", 0);
    c = cx_cstr(o);
    if (!c) c = "";
    return fdn_pack(c, (int)strlen(c));
}

id fdn_bridge_url(id *self_slot) {
    id u = self_slot ? *self_slot : 0;
    if (u) objc_retain(u);
    return u;
}

void fdn_url_init_path(id *slot, swstr path) {
    id s = fdn_bridge_string(path);
    id u = 0;
#ifdef FDN_HOST
    if (s) u = (id)fdn_host_msg(0, "fileURLWithPath:", (uintptr_t)s, 0, 1);
#else
    if (s) u = MSG(id, (id)&NSURL_c, "fileURLWithPath:", s);
#endif
    if (slot) *slot = u;
}

swstr fdn_url_path(id *slot) {
    id u = slot ? *slot : 0;
    id p = u ? MSG0(id, u, "path") : 0;
    return fdn_string_from_ns(p);
}

id fdn_url_from_ns(id u) {
    if (u) objc_retain(u);
    return u;
}

static id fdn_empty_dict(void) {
#ifdef FDN_HOST
    return (id)fdn_host_msg(0, "dictionary", 0, 0, 0);
#else
    return MSG0(id, (id)&NSMutableDictionary_c, "dictionary");
#endif
}
static id fdn_empty_array(void) {
#ifdef FDN_HOST
    return (id)fdn_host_msg(0, "array", 0, 0, 0);
#else
    return MSG0(id, (id)&NSMutableArray_c, "array");
#endif
}

id fdn_bridge_dict(id d) {
    if (!d) return fdn_empty_dict();
    objc_retain(d);
    return d;
}

id fdn_bridge_array(id a) {
    if (!a) return fdn_empty_array();
    objc_retain(a);
    return a;
}

id fdn_array_from_ns(id a) {
    i8arr_t *buf;
    int64_t n, i;
    if (!a) return 0;
    n = MSG0(long, a, "count");
    if (n < 0) n = 0;
    buf = calloc(1, sizeof *buf + (size_t)n * sizeof(id));
    if (!buf) return 0;
    buf->count = n;
    buf->cap = n;
    for (i = 0; i < n; i++)
        ((id *)buf->el)[i] = MSG(id, a, "objectAtIndex:", (long)i);
    return (id)(void *)buf;
}

static id fdn_set_pair(id d, id key, id val) {
#ifdef FDN_HOST
    fdn_host_msg(d, "setObject:forKey:", (uintptr_t)val, (uintptr_t)key, 2);
#else
    MSG(void, d, "setObject:forKey:", val, key);
#endif
    return d;
}

id fdn_dict_literal_pairs(const unsigned char *elts, long count, id self) {
    long i;
    id d = self ? self : fdn_empty_dict();
    if (!elts || count <= 0) return d;
    for (i = 0; i < count; i++) {
        id key = *(id *)(elts + (size_t)i * 64);
        id val = *(id *)(elts + (size_t)i * 64 + 32);
        if (key && val) fdn_set_pair(d, key, val);
    }
    return d;
}

i8arr_t *fdn_cstring(swstr s, uint64_t enc) {
    char buf[4096];
    int n, i;
    i8arr_t *a;
    /* NSUTF8StringEncoding = 4, NSASCIIStringEncoding = 1, NSNEXTSTEP = 2 */
    if (enc != 4 && enc != 1 && enc != 2) return 0;
    n = fdn_unpack(s, buf, (int)sizeof buf);
    if (n < 0) return 0;
    a = calloc(1, sizeof *a + (size_t)n + 1);
    if (!a) return 0;
    a->count = n + 1;
    a->cap = n + 1;
    for (i = 0; i < n; i++) a->el[i] = (int8_t)buf[i];
    a->el[n] = 0;
    return a;
}

id fdn_nserror_from_error(void *box) {
    id e;
    if (!box) return 0;
    e = *(id *)box;
    if (e) objc_retain(e);
    return e;
}

void fdn_force_bridge_string(id ns, swstr *result) {
    if (result) *result = fdn_string_from_ns(ns);
}
int fdn_cond_bridge_string(id ns, swstr *result) {
    if (!ns || !result) return 0;
    *result = fdn_string_from_ns(ns);
    return 1;
}
int fdn_cond_bridge_array(id ns, id *result) {
    if (!result) return 0;
    *result = fdn_array_from_ns(ns);
    return ns ? 1 : 0;
}

void fdn_error_from_ns(void *dst, id err) {
    id *w = dst;
    if (!w) return;
    memset(w, 0, 5 * sizeof(id));
    w[0] = err;
    w[3] = (id)&md_storage;
}

swstr fdn_error_description(void *box) {
    id e = box ? *(id *)box : 0;
    id d = e ? MSG0(id, e, "localizedDescription") : 0;
    if (!d) return fdn_small("Erro", 4);
    return fdn_string_from_ns(d);
}

id fdn_data_bridge(const uint8_t r[16]) {
    uint8_t tmp[14];
    const uint8_t *p;
    int64_t n;
    if (!rep_materialize(r, &p, &n, tmp, 14)) n = 0;
#ifdef FDN_HOST
    return (id)fdn_host_msg(0, "dataWithBytes:length:", (uintptr_t)(p ? p : (const uint8_t *)""), (uintptr_t)n, 2);
#else
    return MSG(id, (id)&NSData_c, "dataWithBytes:length:", p ? p : (const uint8_t *)"", (unsigned long)n);
#endif
}

void fdn_data_from_ns(uint8_t r[16], id data) {
    const void *p;
    unsigned long n;
    if (!data) { rep_clear(r); return; }
    p = MSG0(const void *, data, "bytes");
    n = MSG0(unsigned long, data, "length");
    fdn_rep_from_bytes(r, p, (int64_t)n);
}

/* withUnsafeBytes: chama o closure (fn, ctx) com (ptr, count). */
typedef uint64_t (*fdn_body)(void *ctx, const void *ptr, int64_t n, uint64_t *hi);
uint64_t fdn_with_unsafe(const void *ptr, int64_t n, void *fn, void *ctx, uint64_t *hi) {
    if (!fn) return 0;
    return ((fdn_body)fn)(ctx, ptr, n, hi);
}

#ifdef FDN_HOST
/* definido no teste */
#else

__asm__(
".text\n"
".globl \"$s10Foundation13__DataStorageC5bytes6lengthACSVSg_Sitcfc\"\n"
"\"$s10Foundation13__DataStorageC5bytes6lengthACSVSg_Sitcfc\":\n"
"  movq %rdi, %rdi\n"
"  movq %rsi, %rsi\n"
"  call fdn_storage_init\n"
"  ret\n"
".globl \"$s10Foundation13__DataStorageC6_bytesSvSgvg\"\n"
"\"$s10Foundation13__DataStorageC6_bytesSvSgvg\":\n"
"  movq %r13, %rdi\n"
"  jmp fdn_storage_bytes\n"
".globl \"$s10Foundation13__DataStorageC7_lengthSivg\"\n"
"\"$s10Foundation13__DataStorageC7_lengthSivg\":\n"
"  movq %r13, %rdi\n"
"  jmp fdn_storage_length\n"
".globl \"$s10Foundation13__DataStorageC7_offsetSivg\"\n"
"\"$s10Foundation13__DataStorageC7_offsetSivg\":\n"
"  movq %r13, %rdi\n"
"  jmp fdn_storage_offset\n"
".globl \"$s10Foundation13__DataStorageCMa\"\n"
"\"$s10Foundation13__DataStorageCMa\":\n"
"  call fdn_storage_metadata\n"
"  ret\n"
".globl \"$s10Foundation4DataV14RangeReferenceCMa\"\n"
"\"$s10Foundation4DataV14RangeReferenceCMa\":\n"
"  call fdn_range_metadata\n"
"  ret\n"
".globl \"$s10Foundation4DataV15_RepresentationO6append10contentsOfySW_tF\"\n"
"\"$s10Foundation4DataV15_RepresentationO6append10contentsOfySW_tF\":\n"
"  movq %r13, %rdx\n"
"  jmp fdn_rep_append\n"
".globl \"$s10Foundation4DataV19_bridgeToObjectiveCSo6NSDataCyF\"\n"
"\"$s10Foundation4DataV19_bridgeToObjectiveCSo6NSDataCyF\":\n"
"  movq %r13, %rdi\n"
"  movq %r14, %rsi\n"
"  jmp fdn_data_bridge_regs\n"
".globl \"$s10Foundation4DataV36_unconditionallyBridgeFromObjectiveCyACSo6NSDataCSgFZ\"\n"
"\"$s10Foundation4DataV36_unconditionallyBridgeFromObjectiveCyACSo6NSDataCSgFZ\":\n"
"  jmp fdn_data_from_ns_regs\n"
".globl \"$s10Foundation4DataV9copyBytes2to4fromSiSryxG_SnySiGSgtlF\"\n"
"\"$s10Foundation4DataV9copyBytes2to4fromSiSryxG_SnySiGSgtlF\":\n"
"  movq %r13, %r8\n"
"  jmp fdn_copy_bytes_thunk\n"
".globl \"$s10Foundation15ContiguousBytesP010withUnsafeC0yqd__qd__SWKXEKlFTj\"\n"
"\"$s10Foundation15ContiguousBytesP010withUnsafeC0yqd__qd__SWKXEKlFTj\":\n"
"  movq %r13, %rdi\n"
"  movq %r14, %rsi\n"
"  movq %rdi, %rdx\n"
"  movq %rsi, %rcx\n"
"  jmp fdn_with_unsafe\n"
);

id fdn_data_bridge_regs(uint64_t lo, uint64_t hi) {
    uint8_t r[16];
    memcpy(r, &lo, 8);
    memcpy(r + 8, &hi, 8);
    return fdn_data_bridge(r);
}

swstr fdn_data_from_ns_regs(id data) {
    uint8_t r[16];
    swstr s;
    fdn_data_from_ns(r, data);
    memcpy(&s.w0, r, 8);
    memcpy(&s.w1, r + 8, 8);
    return s;
}

/*
 * copyBytes: rdi = ponteiro do buffer, rsi = count, rdx = stride,
 * rcx = lo, r8 = hi, r9 = has_range, r13 = Data*.
 * O trampolim só move r13 para o 5º argumento inteiro (r8) e o resto já está
 * em rdi/rsi/rdx/rcx. has_range e hi ficam na pilha se precisarem; o teste de
 * host chama fdn_rep_copy direto. O trampolim cobre o caso sem range:
 * copia o mínimo entre o buffer e o Data.
 */
int fdn_copy_bytes_thunk(void *dst, int64_t count, int64_t stride, int64_t ignored, const uint8_t *rep) {
    int64_t nbytes;
    (void)ignored;
    if (stride <= 0) stride = 1;
    nbytes = count * stride;
    if (!rep) return 0;
    return fdn_rep_copy(rep, dst, nbytes, 0, 0, 0);
}

__asm__(
".text\n"
".globl \"$s10Foundation22_convertErrorToNSErrorySo0E0Cs0C0_pF\"\n"
"\"$s10Foundation22_convertErrorToNSErrorySo0E0Cs0C0_pF\":\n"
"  movq %rdi, %rdi\n"
"  jmp fdn_nserror_from_error\n"
".globl \"$s10Foundation22_convertNSErrorToErrorys0E0_pSo0C0CSgF\"\n"
"\"$s10Foundation22_convertNSErrorToErrorys0E0_pSo0C0CSgF\":\n"
"  movq %rsi, %rsi\n"
"  jmp fdn_error_from_ns\n"
".globl \"$s10Foundation3URLV15fileURLWithPathACSSh_tcfC\"\n"
"\"$s10Foundation3URLV15fileURLWithPathACSSh_tcfC\":\n"
"  movq %r13, %rdx\n"
"  jmp fdn_url_init_path_sw\n"
".globl \"$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF\"\n"
"\"$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF\":\n"
"  movq %r13, %rdi\n"
"  jmp fdn_bridge_url\n"
".globl \"$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ\"\n"
"\"$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ\":\n"
"  jmp fdn_url_from_ns\n"
".globl \"$s10Foundation3URLV4pathSSvg\"\n"
"\"$s10Foundation3URLV4pathSSvg\":\n"
"  movq %r13, %rdi\n"
"  jmp fdn_url_path\n"
".globl \"$s10Foundation3URLVMa\"\n"
"\"$s10Foundation3URLVMa\":\n"
"  call fdn_url_metadata\n"
"  ret\n"
".globl \"$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF\"\n"
"\"$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF\":\n"
"  movq %r13, %rdi\n"
"  movq %r14, %rsi\n"
"  jmp fdn_bridge_string\n"
".globl \"$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ\"\n"
"\"$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ\":\n"
"  jmp fdn_string_from_ns\n"
".globl \"$sSS10FoundationE8EncodingV4utf8ACvgZ\"\n"
"\"$sSS10FoundationE8EncodingV4utf8ACvgZ\":\n"
"  mov $4, %rax\n"
"  ret\n"
".globl \"$sSS10FoundationE8EncodingVMa\"\n"
"\"$sSS10FoundationE8EncodingVMa\":\n"
"  call fdn_encoding_metadata\n"
"  ret\n"
".globl \"$sSD10FoundationE19_bridgeToObjectiveCSo12NSDictionaryCyF\"\n"
"\"$sSD10FoundationE19_bridgeToObjectiveCSo12NSDictionaryCyF\":\n"
"  movq %r13, %rdi\n"
"  jmp fdn_bridge_dict\n"
".globl \"$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF\"\n"
"\"$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF\":\n"
"  movq %r13, %rdi\n"
"  jmp fdn_bridge_array\n"
".globl \"$sSa10FoundationE36_unconditionallyBridgeFromObjectiveCySayxGSo7NSArrayCSgFZ\"\n"
"\"$sSa10FoundationE36_unconditionallyBridgeFromObjectiveCySayxGSo7NSArrayCSgFZ\":\n"
"  jmp fdn_array_from_ns\n"
".globl \"$sSo12NSDictionaryC10FoundationE17dictionaryLiteralAByp_yptd_tcfC\"\n"
"\"$sSo12NSDictionaryC10FoundationE17dictionaryLiteralAByp_yptd_tcfC\":\n"
"  movq %r13, %rdx\n"
"  jmp fdn_dict_literal_pairs\n"
".globl \"$sSy10FoundationE7cString5usingSays4Int8VGSgSSAAE8EncodingV_tF\"\n"
"\"$sSy10FoundationE7cString5usingSays4Int8VGSgSSAAE8EncodingV_tF\":\n"
"  movq %r13, %rsi\n"
"  xchg %rdi, %rsi\n"
"  movq %r14, %rdx\n"
"  jmp fdn_cstring_sw\n"
".globl \"$ss5ErrorP10FoundationE20localizedDescriptionSSvg\"\n"
"\"$ss5ErrorP10FoundationE20localizedDescriptionSSvg\":\n"
"  movq %r13, %rdi\n"
"  jmp fdn_error_description\n"
);

void fdn_url_init_path_sw(uint64_t w0, uint64_t w1, id *slot) {
    swstr s;
    s.w0 = w0;
    s.w1 = w1;
    fdn_url_init_path(slot, s);
}

i8arr_t *fdn_cstring_sw(uint64_t enc, uint64_t w0, uint64_t w1) {
    swstr s;
    s.w0 = w0;
    s.w1 = w1;
    return fdn_cstring(s, enc);
}

__asm__(
".text\n"
".globl \"$sSS10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSStringC_SSSgztFZ\"\n"
"\"$sSS10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSStringC_SSSgztFZ\":\n"
"  jmp fdn_force_bridge_string\n"
".globl \"$sSS10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSStringC_SSSgztFZ\"\n"
"\"$sSS10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSStringC_SSSgztFZ\":\n"
"  jmp fdn_cond_bridge_string\n"
".globl \"$sSa10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo7NSArrayC_SayxGSgztFZ\"\n"
"\"$sSa10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo7NSArrayC_SayxGSgztFZ\":\n"
"  jmp fdn_cond_bridge_array\n"
);

/* Descritores de conformidade: o símbolo Mc é dado, não uma função vazia. */
typedef struct {
    int32_t protocol_off;
    int32_t type_off;
    int32_t witness_off;
    uint32_t flags;
} fdn_conf_t;

static const void *fdn_cb_witness[] = { (void *)fdn_with_unsafe };
static const void *fdn_dp_witness[] = { (void *)fdn_with_unsafe };
static const int fdn_cb_proto;
static const int fdn_dp_proto;
static const int fdn_ubp_type;

fdn_conf_t fdn_cb_conf;
fdn_conf_t fdn_dp_conf;
__asm__(
".globl \"$sSRyxG10Foundation15ContiguousBytesABs5UInt8VRszlMc\"\n"
".set \"$sSRyxG10Foundation15ContiguousBytesABs5UInt8VRszlMc\", fdn_cb_conf\n"
".globl \"$sSRyxG10Foundation12DataProtocolABs5UInt8VRszlMc\"\n"
".set \"$sSRyxG10Foundation12DataProtocolABs5UInt8VRszlMc\", fdn_dp_conf\n"
);

void fdn_swift_data_init(void); 
static void fdn_fill_conf(void) {
    fdn_cb_conf.protocol_off = (int32_t)((const char *)&fdn_cb_proto - (const char *)&fdn_cb_conf);
    fdn_cb_conf.type_off = (int32_t)((const char *)&fdn_ubp_type - ((const char *)&fdn_cb_conf + 4));
    fdn_cb_conf.witness_off = (int32_t)((const char *)fdn_cb_witness - ((const char *)&fdn_cb_conf + 8));
    fdn_dp_conf.protocol_off = (int32_t)((const char *)&fdn_dp_proto - (const char *)&fdn_dp_conf);
    fdn_dp_conf.type_off = (int32_t)((const char *)&fdn_ubp_type - ((const char *)&fdn_dp_conf + 4));
    fdn_dp_conf.witness_off = (int32_t)((const char *)fdn_dp_witness - ((const char *)&fdn_dp_conf + 8));
}
void fdn_swift_data_init(void) { fdn_fill_conf(); }
static const int fdn_cb_proto = 1;
static const int fdn_dp_proto = 1;
static const int fdn_ubp_type = 1;

#endif
