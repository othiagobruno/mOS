/*
 * mOS libSystem — o resto pequeno do corpus (docs/lacunas/, 3 a 5 apps cada):
 *   - CoreServices: FSMoveObjectToTrashSync (pelo trashItemAtURL: do NSFileManager), TECGetTextEncodingFromInternetName,
 *     UCConvertUTCDateTimeToCFAbsoluteTime/UCConvertLongDateTimeToCFAbsoluteTime (relógio de 1904);
 *   - CoreFoundation: CFStringTokenizerGetCurrentSubTokens; CFNetwork: CFNetDiagnosticCreateWithURL;
 *   - CoreText: kCTFontMacintoshEncodingsAttribute; bzip2: BZ2_bzread/bzwrite/bzWriteClose (API "zlib" do libbz2);
 *   - libSystem: getmntinfo_r_np (o getmntinfo do attrlist.c com buffer do chamador).
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
typedef i32 OSStatus;
#define paramErr (-50)
#define fnfErr (-43)

/* ---- FSMoveObjectToTrashSync(const FSRef *src, FSRef *target, OptionBits) */
typedef struct { u8 b[80]; } FSRef;
int FSRefMakePath(const FSRef *ref, u8 *path, u32 max);
OSStatus FSPathMakeRef(const u8 *path, FSRef *ref, u8 *is_dir);
OSStatus FSMoveObjectToTrashSync(const FSRef *src, FSRef *target, u32 opts) {
    (void)opts;
    u8 path[1024];
    if (!src || FSRefMakePath(src, path, sizeof path)) return fnfErr;
    id fm = MSG0(id, (id)cx_class_named("NSFileManager"), "defaultManager");
    id url = MSG(id, (id)cx_class_named("NSURL"), "fileURLWithPath:", cx_str((const char *)path));
    id out = 0;
    BOOL ok = MSG(BOOL, fm, "trashItemAtURL:resultingItemURL:error:", url, &out, (id *)0);
    if (!ok) return -36;                           /* ioErr */
    if (target && out) {
        id p = MSG0(id, out, "path");
        if (p) FSPathMakeRef((const u8 *)cx_cstr(p), target, 0);
    }
    return 0;
}
OSStatus FSPathMoveObjectToTrashSync(const char *src, char **target, u32 opts) {
    (void)opts;
    if (!src) return paramErr;
    id fm = MSG0(id, (id)cx_class_named("NSFileManager"), "defaultManager");
    id url = MSG(id, (id)cx_class_named("NSURL"), "fileURLWithPath:", cx_str(src));
    id out = 0;
    if (!MSG(BOOL, fm, "trashItemAtURL:resultingItemURL:error:", url, &out, (id *)0)) return -36;
    if (target) {
        const char *p = out ? cx_cstr(MSG0(id, out, "path")) : 0;
        *target = p ? strdup(p) : 0;
    }
    return 0;
}

/* ---- TECGetTextEncodingFromInternetName(TextEncoding *enc, ConstStr255Param name) */
OSStatus TECGetTextEncodingFromInternetNameOrMIB(unsigned *enc, unsigned usage, const u8 *name, long mib);
OSStatus TECGetTextEncodingFromInternetName(unsigned *enc, const u8 *name) { return TECGetTextEncodingFromInternetNameOrMIB(enc, 0, name, -1); }

/* ---- UTCDateTime / LongDateTime: segundos desde 1/1/1904 (0x0000 de época do Mac). CFAbsoluteTime conta desde 2001. */
#define MAC1904_TO_2001 3061152000.0
typedef struct { u16 highSeconds; u32 lowSeconds; u16 fraction; } __attribute__((packed, aligned(2))) UTCDateTime;
OSStatus UCConvertUTCDateTimeToCFAbsoluteTime(const UTCDateTime *t, double *out) {
    if (!t || !out) return paramErr;
    double s = (double)(((u64)t->highSeconds << 32) | t->lowSeconds) + t->fraction / 65536.0;
    *out = s - MAC1904_TO_2001;
    return 0;
}
OSStatus UCConvertLongDateTimeToCFAbsoluteTime(i64 ldt, double *out) {
    if (!out) return paramErr;
    *out = (double)ldt - MAC1904_TO_2001;
    return 0;
}

/* CSIdentityGetUUID: csidentity.c */

/* ---- CFStringTokenizerGetCurrentSubTokens: o tokenizador do mOS não divide subpalavras; nenhuma */
typedef struct { long loc, len; } cfrng_t;
long CFStringTokenizerGetCurrentSubTokens(id tok, cfrng_t *ranges, long max, id derived) {
    (void)tok; (void)ranges; (void)max; (void)derived;
    return 0;
}
/* ---- CFNetDiagnostic: o assistente de rede da Apple não existe; sem diagnóstico */
void *CFNetDiagnosticCreateWithURL(void *alloc, id url) { (void)alloc; (void)url; return 0; }
void *CFNetDiagnosticCreateWithStreams(void *alloc, void *rs, void *ws) { (void)alloc; (void)rs; (void)ws; return 0; }
i32 CFNetDiagnosticDiagnoseProblemInteractively(void *d) { (void)d; return -66560; }   /* kCFNetDiagnosticErr */
i32 CFNetDiagnosticCopyNetworkStatusPassively(void *d, id *desc) { (void)d; if (desc) *desc = 0; return 0; }  /* kCFNetDiagnosticNoErr */

/* ---- CoreText */
extern cx_class_t NSConstantString_c __asm__("__CFConstantStringClassReference");
#define KS(sym, val) static nsstr_t sym##_o = { &NSConstantString_c, 0, (char *)val, sizeof(val) - 1 }; id sym = (id)&sym##_o;
KS(kCTFontMacintoshEncodingsAttribute, "NSCTFontMacintoshEncodingsAttribute")

/* ---- bzip2, API de arquivo estilo zlib (BZ2_bzopen/bzread...): em cima do BZ2_bzRead/BZ2_bzWrite do libSystem */
int BZ2_bzRead(int *err, void *b, void *buf, int len);
void BZ2_bzWrite(int *err, void *b, void *buf, int len);
void BZ2_bzWriteClose64(int *err, void *b, int abandon, u32 *in_lo, u32 *in_hi, u32 *out_lo, u32 *out_hi);
int BZ2_bzread(void *b, void *buf, int len) {
    int err = 0;
    int n = BZ2_bzRead(&err, b, buf, len);
    return err == 0 || err == 4 ? n : -1;          /* BZ_OK / BZ_STREAM_END */
}
int BZ2_bzwrite(void *b, void *buf, int len) {
    int err = 0;
    BZ2_bzWrite(&err, b, buf, len);
    return err == 0 ? len : -1;
}
void BZ2_bzWriteClose(int *err, void *b, int abandon, u32 *in, u32 *out) {
    u32 ih = 0, oh = 0;
    BZ2_bzWriteClose64(err, b, abandon, in, &ih, out, &oh);
}

/* ---- getmntinfo_r_np: como getmntinfo, mas o buffer é do chamador (free) */
int getfsstat(void *buf, int size, int flags);
void *calloc(size_t n, size_t s);
int getmntinfo_r_np(void **mntbufp, int flags) {
    if (!mntbufp) { cx_errno = 22; return 0; }
    int n = getfsstat(0, 0, flags);
    if (n <= 0) { *mntbufp = 0; return n; }
    enum { STATFS64_SIZE = 2168 };                 /* struct statfs ($INODE64) do macOS */
    void *tab = calloc((size_t)n, STATFS64_SIZE);
    if (!tab) { cx_errno = 12; return 0; }
    n = getfsstat(tab, n * STATFS64_SIZE, flags);
    *mntbufp = tab;
    return n;
}
