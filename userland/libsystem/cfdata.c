/*
 * mOS — libSystem: CFData e CFDate (CoreFoundation) em ponte com o NSData/NSDate do mOS.
 *
 * Medido com tools/machocheck.c (docs/compat-apps.md): os sete apps-alvo (VS Code, Cursor, Claude, Codex, Slack,
 * Grok Bot, WhatsApp) importam CFDataCreate/GetBytePtr/GetLength... e o mOS não tinha nenhuma. O Security
 * (security.c) também precisa: kSecValueData é um CFData.
 *
 * CFDataRef é um NSData do mOS (appkit.c: { isa, bytes, len }), então CFRetain/CFRelease, CFGetTypeID (TID_DATA,
 * cfcollections.c) e [data bytes]/[data length] funcionam nos dois sentidos. Os "mutáveis" usam o mesmo objeto: o
 * bloco é nosso (malloc), então dá para crescer com realloc. CFDataCreateWithBytesNoCopy copia (o NSData do mOS não
 * guarda quem libera o bloco).
 *
 * CFDateRef é um NSDate (CFAbsoluteTime = segundos desde 1/1/2001; NSDate guarda desde 1970 + 978307200).
 */
typedef unsigned char u8;
typedef unsigned long long u64;
typedef unsigned long size_t;
typedef void *id_;
typedef struct { long loc, len; } CFRange;

void *malloc(size_t n);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);

typedef struct cx_class cx_class_t;
cx_class_t *cx_class_named(const char *name);
id_ objc_msgSend(id_ self, void *op, ...);
void *sel_registerName(const char *name);
id_ objc_retain(id_ o);
#define MSG0(T, o, s) ((T (*)(id_, void *))objc_msgSend)((id_)(o), sel_registerName(s))
#define MSG1(T, o, s, a) ((T (*)(id_, void *, __typeof__(a)))objc_msgSend)((id_)(o), sel_registerName(s), (a))
#define MSG2(T, o, s, a, b) ((T (*)(id_, void *, __typeof__(a), __typeof__(b)))objc_msgSend)((id_)(o), sel_registerName(s), (a), (b))
#define CLS(n) ((id_)cx_class_named(n))

/* layout do NSData do mOS (appkit.c, "NSData e NSImage") */
typedef struct { void *isa; u8 *bytes; u64 len; } nsdata_t;

/* cria um NSData +1 (as funções Create do CF devolvem com retenção, como no macOS) */
id_ cx_data_new(const void *b, u64 n) {
    id_ d = MSG2(id_, CLS("NSData"), "dataWithBytes:length:", b ? b : "", n);
    return d ? objc_retain(d) : 0;
}

id_ CFDataCreate(void *alloc, const u8 *bytes, long n) { (void)alloc; return cx_data_new(bytes, n < 0 ? 0 : (u64)n); }
id_ CFDataCreateCopy(void *alloc, id_ d) {
    (void)alloc;
    return d ? cx_data_new(MSG0(const void *, d, "bytes"), MSG0(u64, d, "length")) : 0;
}
id_ CFDataCreateMutable(void *alloc, long cap) { (void)alloc; (void)cap; return cx_data_new("", 0); }
id_ CFDataCreateMutableCopy(void *alloc, long cap, id_ d) { (void)cap; return d ? CFDataCreateCopy(alloc, d) : cx_data_new("", 0); }

/* No mOS todos os alocadores CF são NULL (darwin.c), então kCFAllocatorNull ("não libere, o bloco é meu") não se
 * distingue do padrão: o bloco é copiado e NUNCA liberado aqui (um vazamento pequeno em vez de liberar memória
 * de quem pediu kCFAllocatorNull). */
void cx_cf_allocator_dispose_bytes(const void *alloc, void *bytes);   /* vscode_cf.c */
id_ CFDataCreateWithBytesNoCopy(void *alloc, const u8 *bytes, long n, void *dealloc) {
    id_ d = CFDataCreate(alloc, bytes, n);
    /* alocador personalizado (CFAllocatorCreate): os bytes já foram copiados, então o CFData não precisa mais deles e
     * o deallocate dele roda agora (no macOS rodaria quando o CFData fosse liberado) */
    if (d) cx_cf_allocator_dispose_bytes(dealloc, (void *)bytes);
    return d;
}

long CFDataGetLength(id_ d) { return d ? (long)MSG0(u64, d, "length") : 0; }
const u8 *CFDataGetBytePtr(id_ d) { return d ? MSG0(const u8 *, d, "bytes") : 0; }
u8 *CFDataGetMutableBytePtr(id_ d) { return d ? ((nsdata_t *)d)->bytes : 0; }
void CFDataGetBytes(id_ d, CFRange r, u8 *out) {
    if (!d || !out || r.loc < 0 || r.len <= 0) return;
    u64 n = MSG0(u64, d, "length");
    if ((u64)r.loc >= n) return;
    if ((u64)(r.loc + r.len) > n) r.len = (long)(n - (u64)r.loc);
    memcpy(out, MSG0(const u8 *, d, "bytes") + r.loc, (size_t)r.len);
}

static void set_len(nsdata_t *d, u64 n) {
    if (n > d->len) {
        u8 *nb = realloc(d->bytes, n ? n : 1);
        if (!nb) return;
        memset(nb + d->len, 0, n - d->len);     /* CFDataSetLength/IncreaseLength zeram o que cresce */
        d->bytes = nb;
    }
    d->len = n;
}
void CFDataSetLength(id_ d, long n) { if (d && n >= 0) set_len((nsdata_t *)d, (u64)n); }
void CFDataIncreaseLength(id_ d, long extra) { if (d && extra > 0) set_len((nsdata_t *)d, ((nsdata_t *)d)->len + (u64)extra); }
void CFDataAppendBytes(id_ d, const u8 *b, long n) {
    if (!d || !b || n <= 0) return;
    nsdata_t *x = (nsdata_t *)d;
    u64 old = x->len;
    set_len(x, old + (u64)n);
    if (x->len == old + (u64)n) memcpy(x->bytes + old, b, (size_t)n);
}
void CFDataReplaceBytes(id_ d, CFRange r, const u8 *nb, long nlen) {
    if (!d || r.loc < 0 || r.len < 0 || nlen < 0) return;
    nsdata_t *x = (nsdata_t *)d;
    if ((u64)r.loc > x->len) return;
    if ((u64)(r.loc + r.len) > x->len) r.len = (long)(x->len - (u64)r.loc);
    u64 tail = x->len - (u64)(r.loc + r.len);
    u64 nl = (u64)r.loc + (u64)nlen + tail;
    if (nl > x->len) set_len(x, nl);
    if (x->len < nl) return;                                     /* realloc falhou */
    memmove(x->bytes + r.loc + nlen, x->bytes + r.loc + r.len, (size_t)tail);
    if (nlen && nb) memcpy(x->bytes + r.loc, nb, (size_t)nlen);
    x->len = nl;
}
void CFDataDeleteBytes(id_ d, CFRange r) { CFDataReplaceBytes(d, r, 0, 0); }

/* ---- CFDate */
#define CF_EPOCH_1970 978307200.0
id_ CFDateCreate(void *alloc, double at) {
    (void)alloc;
    id_ d = MSG1(id_, CLS("NSDate"), "dateWithTimeIntervalSince1970:", at + CF_EPOCH_1970);
    return d ? objc_retain(d) : 0;
}
double CFDateGetAbsoluteTime(id_ d) { return d ? MSG0(double, d, "timeIntervalSince1970") - CF_EPOCH_1970 : 0; }
long CFDateCompare(id_ a, id_ b, void *ctx) {
    (void)ctx;
    double x = CFDateGetAbsoluteTime(a), y = CFDateGetAbsoluteTime(b);
    return x < y ? -1 : x > y ? 1 : 0;
}
double CFDateGetTimeIntervalSinceDate(id_ a, id_ b) { return CFDateGetAbsoluteTime(a) - CFDateGetAbsoluteTime(b); }
