/*
 * mOS libSystem — CoreFoundation/Foundation em C que o Postico 2 (binário Swift + SwiftUI da Apple) chama e o
 * libSystem ainda não tinha (inventário do machocheck, docs/swift-objc-runtime.md, fase 7): CFString mutável
 * (CreateMutable, AppendFormat, Lowercase, Trim), CFArraySortValues, CFBitVector (sobre NSMutableData),
 * CFAttributedStringCreate, CFPropertyListIsValid, observador de run loop com bloco, constantes de locale e
 * NSEdgeInsetsZero/NSIntersectsRect. As CFString do mOS são NSString (UTF-8), como em darwin.c.
 */
#include "foundation.h"

void *malloc(size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int strncmp(const char *a, const char *b, size_t n);
void qsort_r(void *base, size_t n, size_t sz, void *thunk, int (*cmp)(void *, const void *, const void *));
id objc_retain(id o);
void objc_release(id o);
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");

id objc_getClass(const char *n);
static id cls_(const char *n) { return objc_getClass(n); }

/* ---- CFString mutável */
id CFStringCreateMutable(void *alloc, long max) {
    (void)alloc; (void)max;
    return MSG0(id, MSG0(id, cls_("NSMutableString"), "alloc"), "init");
}
void CFStringAppendFormat(id m, void *opts, id fmt, ...) {
    (void)opts;
    if (!m || !fmt) return;
    va_list ap;
    va_start(ap, fmt);
    id s = cx_str_vfmt(fmt, ap);
    va_end(ap);
    if (s) MSG(void, m, "appendString:", s);
}
/* tira as repetições de `trim` do começo e do fim */

/* CFArraySortValues: cfcollections.c (troca só os ponteiros, vale para ponteiros crus) */

/* ---- CFBitVector: um NSMutableData com a contagem de bits nos 8 primeiros bytes */
static u64 *bv_count(id bv) { return (u64 *)MSG0(void *, bv, "mutableBytes"); }
id CFBitVectorCreateMutable(void *alloc, long capacity) {
    (void)alloc;
    id d = MSG(id, cls_("NSMutableData"), "dataWithLength:", (unsigned long)(8 + (capacity > 0 ? (capacity + 7) / 8 : 0)));
    return objc_retain(d);
}
long CFBitVectorGetCount(id bv) { return bv ? (long)*bv_count(bv) : 0; }
void CFBitVectorSetCount(id bv, long count) {
    if (!bv || count < 0) return;
    unsigned long need = 8 + (unsigned long)(count + 7) / 8;
    if (MSG0(unsigned long, bv, "length") < need) MSG(void, bv, "setLength:", need);
    *bv_count(bv) = (u64)count;
}
u32 CFBitVectorGetBitAtIndex(id bv, long i) {
    if (!bv || i < 0 || i >= CFBitVectorGetCount(bv)) return 0;
    const u8 *b = (const u8 *)bv_count(bv) + 8;
    return (b[i / 8] >> (7 - i % 8)) & 1;   /* bit 0 é o mais significativo, como no CF */
}
id CFBitVectorCreateCopy(void *alloc, id bv) { (void)alloc; return bv ? MSG0(id, bv, "mutableCopy") : 0; }
id CFBitVectorCreate(void *alloc, const u8 *bytes, long n) {
    id bv = CFBitVectorCreateMutable(alloc, n);
    if (!bv) return 0;
    u64 *cnt = bv_count(bv);
    *cnt = (u64)(n > 0 ? n : 0);
    if (bytes && n > 0) memcpy((u8 *)cnt + 8, bytes, (size_t)(n + 7) / 8);
    return bv;
}
typedef struct { long loc, len; } bv_range_t;
/* copia os bits do intervalo para 'out', empacotados a partir do bit mais significativo do primeiro byte */
void CFBitVectorGetBits(id bv, bv_range_t r, u8 *out) {
    if (!bv || !out || r.len <= 0) return;
    memset(out, 0, (size_t)(r.len + 7) / 8);
    for (long i = 0; i < r.len; i++) if (CFBitVectorGetBitAtIndex(bv, r.loc + i)) out[i / 8] |= (u8)(0x80 >> (i % 8));
}
long CFBitVectorGetFirstIndexOfBit(id bv, bv_range_t r, u32 bit) {
    for (long i = r.loc; i < r.loc + r.len; i++) if (CFBitVectorGetBitAtIndex(bv, i) == (bit & 1)) return i;
    return -1;   /* kCFNotFound */
}
long CFBitVectorGetLastIndexOfBit(id bv, bv_range_t r, u32 bit) {
    for (long i = r.loc + r.len - 1; i >= r.loc; i--) if (CFBitVectorGetBitAtIndex(bv, i) == (bit & 1)) return i;
    return -1;
}
long CFBitVectorGetCountOfBit(id bv, bv_range_t r, u32 bit) {
    long k = 0;
    for (long i = r.loc; i < r.loc + r.len; i++) if (CFBitVectorGetBitAtIndex(bv, i) == (bit & 1)) k++;
    return k;
}
void CFBitVectorSetBitAtIndex(id bv, long i, u32 bit) {
    if (!bv || i < 0) return;
    if (i >= CFBitVectorGetCount(bv)) CFBitVectorSetCount(bv, i + 1);
    u8 *b = (u8 *)bv_count(bv) + 8;
    if (bit) b[i / 8] |= (u8)(0x80 >> (i % 8)); else b[i / 8] &= (u8)~(0x80 >> (i % 8));
}

/* ---- outros */
u8 CFPropertyListIsValid(id plist, long format) { (void)format; return plist != 0; }

/* observador com bloco: o bloco recebe (observador, atividade) */
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, void *, unsigned long); } cx_block_t;
void *CFRunLoopObserverCreate(void *alloc, unsigned long activities, unsigned char repeats, long order,
                              void (*cb)(void *, unsigned long, void *), void *ctx);
void *_Block_copy(const void *b);
static void obs_block_cb(void *obs, unsigned long act, void *info) {
    cx_block_t *b = info;
    if (b && b->invoke) b->invoke(b, obs, act);
}
void *CFRunLoopObserverCreateWithHandler(void *alloc, unsigned long activities, unsigned char repeats, long order, void *block) {
    struct { long version; void *info; void *retain, *release, *desc; } ctx = {0, _Block_copy(block), 0, 0, 0};
    return CFRunLoopObserverCreate(alloc, activities, repeats, order, obs_block_cb, &ctx);
}

/* ---- constantes */
typedef struct { void *isa; u64 flags; const char *s; u64 n; } pk_const_t;
#define PK(var, text) static pk_const_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
PK(NSLocaleDecimalSeparator, "decimal")
PK(NSLocaleGroupingSeparator, "grouping")
PK(NSURLCanonicalPathKey, "NSURLCanonicalPathKey")
PK(NSURLCreationDateKey, "NSURLCreationDateKey")
PK(NSURLIsRegularFileKey, "NSURLIsRegularFileKey")
PK(NSURLNameKey, "NSURLNameKey")
PK(NSURLPathKey, "_NSURLPathKey")
PK(NSURLVolumeIsInternalKey, "NSURLVolumeIsInternalKey")
PK(NSURLVolumeIsLocalKey, "NSURLVolumeIsLocalKey")
PK(NSURLVolumeNameKey, "NSURLVolumeNameKey")
PK(NSURLVolumeURLKey, "NSURLVolumeURLKey")
PK(NSFileExtensionHidden, "NSFileExtensionHidden")
PK(NSMachErrorDomain, "NSMachErrorDomain")
PK(NSNegateBooleanTransformerName, "NSNegateBoolean")
PK(NSRecoveryAttempterErrorKey, "NSRecoveryAttempter")
PK(kCFGetAddrInfoFailureKey, "kCFGetAddrInfoFailureKey")
/* CFHost e NSHost estão em cfhost.c. kCFErrorDomainCFNetwork está em vscode_consts.c. Não redefinir. */

typedef struct { double top, left, bottom, right; } ns_edge_insets_t;
const ns_edge_insets_t NSEdgeInsetsZero = {0, 0, 0, 0};
typedef struct { double x, y, w, h; } ns_rect_t;
u8 NSIntersectsRect(ns_rect_t a, ns_rect_t b) {
    return !(a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0 || a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y);
}

/* ---- mach_vm_read_overwrite: o crash reporter do Postico lê a própria memória (só a task atual, como o vm_read_overwrite) */
typedef unsigned int mach_port_t;
typedef int kern_return_t;
kern_return_t vm_read_overwrite(mach_port_t task, u64 addr, u64 size, u64 dst, u64 *out);
kern_return_t mach_vm_read_overwrite(mach_port_t task, u64 addr, u64 size, u64 dst, u64 *out) {
    return vm_read_overwrite(task, addr, size, dst, out);
}
