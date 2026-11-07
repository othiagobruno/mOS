/*
 * mOS libSystem — o que faltava ao Antigravity (Electron 44 + o language_server em Go), ao Cursor e ao Grok Bot,
 * medido pelo tools/machocheck.c (build/apps/rel/antigravity.txt).
 * Regra da casa (cli.c, chrome.c): quando o mOS não tem o recurso, devolve o erro que a API documenta.
 *  - stat64/lstat64/fstat64/fstatat64: no x86_64 do macOS a struct stat já é a de 64 bits ($INODE64), então são
 *    as mesmas funções;
 *  - mlockall/munlockall: o mOS não pagina para o disco, então a memória já está "travada";
 *  - ptrace: só PT_DENY_ATTACH (o resto: EPERM, não há depurador no mOS);
 *  - memccpy, ctime_r, nice, __floattidf/__floatuntidf (int128 -> double, do compiler-rt);
 *  - CFTimeZoneCopyDefault, CTFontDescriptorCreateWithNameAndSize;
 *  - FSEvents: as variantes por dispositivo e as consultas do fluxo (o fluxo de cli.c nunca dispara);
 *  - IOHIDManager (sem dispositivos HID), classes vazias de MapKit/Contacts em vscode_fwclasses.c;
 *  - SecTrustSettingsSet/RemoveTrustSettings: o mOS não grava confiança do usuário (errSecUnimplemented).
 */
#include "sys.h"

typedef unsigned long size_t;
typedef void *id_;
extern int cx_errno;
#define CX_EPERM 1
#define CX_EINVAL 22

int stat(const char *p, void *st);
int lstat(const char *p, void *st);
int fstat(int fd, void *st);
int fstatat(int dfd, const char *p, void *st, int flag);
int stat64(const char *p, void *st) { return stat(p, st); }
int lstat64(const char *p, void *st) { return lstat(p, st); }
int fstat64(int fd, void *st) { return fstat(fd, st); }
int fstatat64(int dfd, const char *p, void *st, int flag) { return fstatat(dfd, p, st, flag); }

int mlockall(int flags) { (void)flags; return 0; }
int munlockall(void) { return 0; }

#define PT_DENY_ATTACH 31
int ptrace(int req, int pid, void *addr, int data) {
    (void)pid; (void)addr; (void)data;
    if (req == PT_DENY_ATTACH) return 0;
    cx_errno = CX_EPERM;
    return -1;
}

void *memccpy(void *restrict d, const void *restrict s, int c, size_t n) {
    unsigned char *dd = d;
    const unsigned char *ss = s;
    for (size_t i = 0; i < n; i++) {
        dd[i] = ss[i];
        if (ss[i] == (unsigned char)c) return dd + i + 1;
    }
    return 0;
}

/* o mOS não tem prioridade por processo: guardamos o valor para quem consulta com nice(0) */
static int cur_nice;
int nice(int inc) {
    int v = cur_nice + inc;
    if (v > 19) v = 19;
    if (v < -20) v = -20;
    if (v < cur_nice && v < 0) { cx_errno = CX_EPERM; return -1; }   /* baixar o nice exige root */
    cur_nice = v;
    return v;
}

typedef struct { int sec, min, hour, mday, mon, year, wday, yday, isdst; long gmtoff; char *zone; } cx_tm_t;
cx_tm_t *localtime_r(const long *t, cx_tm_t *tm);
size_t strftime(char *s, size_t max, const char *fmt, const cx_tm_t *tm);
char *asctime_r(const cx_tm_t *tm, char *buf) {
    if (!tm || !buf) return 0;
    strftime(buf, 26, "%a %b %e %H:%M:%S %Y\n", tm);   /* o formato do POSIX; o buffer tem pelo menos 26 bytes */
    return buf;
}
char *ctime_r(const long *t, char *buf) {
    cx_tm_t tm;
    if (!t || !buf || !localtime_r(t, &tm)) return 0;
    return asctime_r(&tm, buf);
}

/* int128 -> double, com arredondamento certo: os 64 bits de cima com um bit "pegajoso" para o que sobrou,
 * convertidos pelo hardware (um só arredondamento), depois escalados. Escrito à mão porque um cast de __int128
 * aqui chamaria a própria função. */
static double u64_to_d(u64 v) {
    double r;
    __asm__("cvtsi2sdq %1, %0" : "=x"(r) : "r"((long long)(v >> 1 | (v & 1))));
    if ((long long)v < 0) return r * 2.0;
    __asm__("cvtsi2sdq %1, %0" : "=x"(r) : "r"((long long)v));
    return r;
}
static double scale2(double x, int e) { while (e >= 60) { x *= 1152921504606846976.0; e -= 60; } while (e-- > 0) x *= 2.0; return x; }
double __floatuntidf(unsigned __int128 a) {
    u64 hi = (u64)(a >> 64), lo = (u64)a;
    if (!hi) return u64_to_d(lo);
    int sh = 64 - __builtin_clzll(hi);                 /* quantos bits passam de 64 */
    u64 top = (u64)(a >> sh), rest = sh ? lo & ((1ULL << sh) - 1) : 0;
    if (rest) top |= 1;                                /* bit pegajoso: o hardware arredonda uma vez só */
    return scale2(u64_to_d(top), sh);
}
double __floattidf(__int128 a) {
    if (a < 0) return -__floatuntidf((unsigned __int128)0 - (unsigned __int128)a);
    return __floatuntidf((unsigned __int128)a);
}

/* ---- CoreFoundation / CoreText */
id_ CFTimeZoneCopySystem(void);
id_ CFTimeZoneCopyDefault(void) { return CFTimeZoneCopySystem(); }

id_ CTFontDescriptorCreateWithAttributes(id_ attrs);
id_ objc_getClass(const char *n);
id_ sel_registerName(const char *n);
id_ objc_msgSend(id_ self, id_ sel, ...);
void objc_release(id_ o);
extern id_ kCTFontNameAttribute, kCTFontSizeAttribute;
id_ CTFontDescriptorCreateWithNameAndSize(id_ name, double size) {
    id_ d = ((id_(*)(id_, id_))objc_msgSend)(objc_getClass("NSMutableDictionary"), sel_registerName("dictionary"));
    if (name) ((void (*)(id_, id_, id_, id_))objc_msgSend)(d, sel_registerName("setObject:forKey:"), name, kCTFontNameAttribute);
    if (size > 0) {
        id_ n = ((id_(*)(id_, id_, double))objc_msgSend)(objc_getClass("NSNumber"), sel_registerName("numberWithDouble:"), size);
        ((void (*)(id_, id_, id_, id_))objc_msgSend)(d, sel_registerName("setObject:forKey:"), n, kCTFontSizeAttribute);
    }
    return CTFontDescriptorCreateWithAttributes(d);
}

/* ---- FSEvents (o fluxo é o de cli.c) */
void *FSEventStreamCreate(void *alloc, void *cb, void *ctx, id_ paths, u64 since, double latency, u32 flags);
void *FSEventStreamCreateRelativeToDevice(void *alloc, void *cb, void *ctx, long dev, id_ paths, u64 since, double latency,
                                          u32 flags) {
    (void)dev;
    return FSEventStreamCreate(alloc, cb, ctx, paths, since, latency, flags);
}
id_ CFArrayCreateMutable(void *alloc, long cap, const void *cb);
id_ CFStringCreateWithCString(void *alloc, const char *s, u32 enc);
id_ FSEventStreamCopyPathsBeingWatched(void *s) { (void)s; return CFArrayCreateMutable(0, 0, 0); }
id_ FSEventStreamCopyDescription(void *s) { (void)s; return CFStringCreateWithCString(0, "FSEventStream (mOS: sem eventos)", 0x08000100); }
id_ FSEventsCopyUUIDForDevice(long dev) { (void)dev; return 0; }
u64 FSEventsGetLastEventIdForDeviceBeforeTime(long dev, double t) { (void)dev; (void)t; return 0; }

/* ---- DiskArbitration: os dicionários de filtro prontos (CFDictionaryRef). NULL = "todos os discos", o que o
 * DARegister*Callback do mOS faria de qualquer forma (ele nunca chama de volta: não há discos removíveis) */
const void *kDADiskDescriptionMatchVolumeMountable = 0;
const void *kDADiskDescriptionWatchVolumePath = 0;

/* ---- IOKit HID: o mOS não expõe dispositivos HID (o sand-webauthn-signer do Grok procura chaves de segurança) */
void IOHIDManagerSetDeviceMatching(void *m, id_ dict) { (void)m; (void)dict; }
void IOHIDManagerRegisterInputReportCallback(void *m, void *cb, void *ctx) { (void)m; (void)cb; (void)ctx; }

/* ---- Security: confiança gravada pelo usuário */
#define errSecUnimplemented (-4)
int SecTrustSettingsSetTrustSettings(id_ cert, int domain, id_ settings) { (void)cert; (void)domain; (void)settings; return errSecUnimplemented; }
int SecTrustSettingsRemoveTrustSettings(id_ cert, int domain) { (void)cert; (void)domain; return errSecUnimplemented; }

/* ---- Codex desktop (os .node de native e o binário codex): o resto do que é C */
const double kCFCoreFoundationVersionNumber = 2048.0;   /* da ordem do macOS 14, a versão que o mOS informa */
u64 CGEventSourceFlagsState(int state) { (void)state; return 0; }      /* nenhuma tecla modificadora pressionada */
unsigned char CGPreflightListenEventAccess(void) { return 0; }        /* sem monitoração de entrada global */
unsigned char CGRequestListenEventAccess(void) { return 0; }
void CGContextSetInterpolationQuality(void *ctx, int q) { (void)ctx; (void)q; }   /* o rasterizador do mOS é um só */
void IOHIDDeviceRegisterRemovalCallback(void *d, void *cb, void *ctx) { (void)d; (void)cb; (void)ctx; }
id_ IOHIDManagerCopyDevices(void *m) { (void)m; return 0; }            /* NULL: nenhum dispositivo, como a Apple */
u32 IORegistryEntryFromPath(u32 master, const char *path) { (void)master; (void)path; return 0; }   /* MACH_PORT_NULL */
typedef int wchar32;
wchar32 *wcsncpy(wchar32 *d, const wchar32 *s, size_t n) {
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cfk_t;
extern char cx_cfconst_isa2[] __asm__("__CFConstantStringClassReference");
static cfk_t kCIInputRadiusKey_s = {cx_cfconst_isa2, 0x7c8, "inputRadius", 11};
const void *kCIInputRadiusKey = &kCIInputRadiusKey_s;

/* CFBundleGetVersionNumber: o CFBundleVersion "M.m.b" no formato NumVersion do Carbon (BCD: major, minor<<4|bug,
 * estágio final 0x80, 0). Sem versão legível: 0 (cursor_proclist.node). */
id_ CFBundleGetValueForInfoDictionaryKey(id_ bundle, id_ key);
id_ CFStringCreateWithCString(void *alloc, const char *s, u32 enc);
const char *cx_cstr(id_ s);
void CFRelease(id_ o);
u32 CFBundleGetVersionNumber(id_ bundle) {
    id_ k = CFStringCreateWithCString(0, "CFBundleVersion", 0x08000100);
    id_ v = bundle && k ? CFBundleGetValueForInfoDictionaryKey(bundle, k) : 0;
    if (k) CFRelease(k);
    const char *s = v ? cx_cstr(v) : 0;
    if (!s) return 0;
    unsigned part[3] = {0, 0, 0}, n = 0;
    for (; *s && n < 3; s++) {
        if (*s >= '0' && *s <= '9') part[n] = part[n] * 10 + (unsigned)(*s - '0');
        else if (*s == '.') n++;
        else break;
    }
    unsigned ma = part[0] % 100, mi = part[1] % 10, bu = part[2] % 10;
    return (u32)(((ma / 10) << 4 | (ma % 10)) << 24 | (mi << 4 | bu) << 16 | 0x80u << 8);
}

/* CGContextDrawTiledImage: repete a imagem com o tamanho de rect a partir da origem dele, cobrindo o contexto inteiro
 * (o mOS não guarda a área de recorte; com a CTM identidade o espaço do usuário é o do bitmap). O overlay Swift
 * CoreGraphics o usa em CGContext.draw(_:in:byTiling:) (módulos nativos do Codex desktop). */
typedef struct { double x, y, w, h; } cg_rect_t;
void CGContextDrawImage(void *c, cg_rect_t r, void *img);
size_t CGBitmapContextGetWidth(void *c);
size_t CGBitmapContextGetHeight(void *c);
void CGContextDrawTiledImage(void *c, cg_rect_t r, void *img) {
    if (!c || !img || r.w <= 0 || r.h <= 0) return;
    double W = (double)CGBitmapContextGetWidth(c), H = (double)CGBitmapContextGetHeight(c);
    if (W <= 0 || H <= 0) { CGContextDrawImage(c, r, img); return; }
    long kx = (long)(r.x / r.w), ky = (long)(r.y / r.h);
    double x0 = r.x - (double)kx * r.w, y0 = r.y - (double)ky * r.h;
    while (x0 > 0) x0 -= r.w;
    while (y0 > 0) y0 -= r.h;
    int n = 0;
    for (double y = y0; y < H && n < 16384; y += r.h)
        for (double x = x0; x < W && n < 16384; x += r.w, n++) {
            cg_rect_t t = {x, y, r.w, r.h};
            CGContextDrawImage(c, t, img);
        }
}

/* ---- o que o Sparkle (atualizador do Codex desktop e de outros apps) importa */
id_ objc_getClass(const char *n);
/* Info.plist de um bundle pela URL (Contents/Info.plist, ou Info.plist na raiz para bundles rasos) */
id_ CFBundleCopyInfoDictionaryInDirectory(id_ url) {
    if (!url) return 0;
    id_ path = ((id_(*)(id_, id_))objc_msgSend)(url, sel_registerName("path"));
    if (!path) return 0;
    static const char *const sub[] = {"Contents/Info.plist", "Info.plist"};
    for (int i = 0; i < 2; i++) {
        id_ s = CFStringCreateWithCString(0, sub[i], 0x08000100);
        id_ p = ((id_(*)(id_, id_, id_))objc_msgSend)(path, sel_registerName("stringByAppendingPathComponent:"), s);
        CFRelease(s);
        id_ d = ((id_(*)(id_, id_, id_))objc_msgSend)(objc_getClass("NSDictionary"), sel_registerName("dictionaryWithContentsOfFile:"), p);
        if (d) return ((id_(*)(id_, id_))objc_msgSend)(d, sel_registerName("retain"));
    }
    return 0;
}
/* nome IANA -> CFStringEncoding (os que o mOS converte); desconhecido: kCFStringEncodingInvalidId */
const char *cx_cstr(id_ s);
int strcasecmp(const char *a, const char *b);
u32 CFStringConvertIANACharSetNameToEncoding(id_ name) {
    const char *n = name ? cx_cstr(name) : 0;
    if (!n) return 0xffffffffu;
    static const struct { const char *n; u32 e; } t[] = {
        {"utf-8", 0x08000100}, {"utf8", 0x08000100}, {"us-ascii", 0x0600}, {"ascii", 0x0600},
        {"iso-8859-1", 0x0201}, {"latin1", 0x0201}, {"windows-1252", 0x0500}, {"cp1252", 0x0500},
        {"utf-16", 0x0100}, {"utf-16be", 0x10000100}, {"utf-16le", 0x14000100}, {"macintosh", 0},
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) if (!strcasecmp(n, t[i].n)) return t[i].e;
    return 0xffffffffu;
}
/* asserções de energia por processo: nenhuma (o mOS não as guarda), num dicionário vazio */
int IOPMCopyAssertionsByProcess(id_ *out) {
    if (!out) return (int)0xe00002c2;   /* kIOReturnBadArgument */
    *out = ((id_(*)(id_, id_))objc_msgSend)(objc_getClass("NSMutableDictionary"), sel_registerName("new"));
    return 0;
}
/* usuário do console: o único do mOS */
id_ SCDynamicStoreCopyConsoleUser(void *store, u32 *uid, u32 *gid) {
    (void)store;
    if (uid) *uid = 501;
    if (gid) *gid = 20;
    return CFStringCreateWithCString(0, "visitante", 0x08000100);
}

/* ---- libbz2, lado da compressão: o mOS só descomprime (bzip2.c). Quem comprime recebe BZ_CONFIG_ERROR (-9), o erro
 * de "biblioteca sem essa função"; o Sparkle usa isto só para montar patches delta e cai para a atualização inteira. */
void *BZ2_bzWriteOpen(int *bzerror, void *f, int blockSize100k, int verbosity, int workFactor) {
    (void)f; (void)blockSize100k; (void)verbosity; (void)workFactor;
    if (bzerror) *bzerror = -9;
    return 0;
}
void BZ2_bzWrite(int *bzerror, void *b, void *buf, int len) { (void)b; (void)buf; (void)len; if (bzerror) *bzerror = -9; }
void BZ2_bzWriteClose64(int *bzerror, void *b, int abandon, u32 *in_lo, u32 *in_hi, u32 *out_lo, u32 *out_hi) {
    (void)b; (void)abandon;
    if (in_lo) *in_lo = 0; if (in_hi) *in_hi = 0; if (out_lo) *out_lo = 0; if (out_hi) *out_hi = 0;
    if (bzerror) *bzerror = -9;
}
int BZ2_bzBuffToBuffCompress(char *dest, unsigned *destlen, char *src, unsigned srclen, int b, int v, int w) {
    (void)dest; (void)destlen; (void)src; (void)srclen; (void)b; (void)v; (void)w;
    return -9;
}

/* ---- libxar (arquivos .xar dos patches delta do Sparkle): o mOS não lê nem grava xar. xar_open devolve NULL, como
 * para um arquivo inválido, e o resto responde "vazio" sem tocar em nada; o Sparkle trata e usa a atualização inteira. */
void *xar_open(const char *file, int flags) { (void)file; (void)flags; return 0; }
int xar_close(void *x) { (void)x; return 0; }
int xar_opt_set(void *x, const char *opt, const char *val) { (void)x; (void)opt; (void)val; return -1; }
void *xar_add_frompath(void *x, void *parent, const char *name, const char *realpath) { (void)x; (void)parent; (void)name; (void)realpath; return 0; }
void *xar_add_frombuffer(void *x, void *parent, const char *name, char *buf, size_t len) { (void)x; (void)parent; (void)name; (void)buf; (void)len; return 0; }
int xar_extract_tofile(void *x, void *f, const char *path) { (void)x; (void)f; (void)path; return -1; }
void *xar_iter_new(void) { return 0; }
void xar_iter_free(void *i) { (void)i; }
void *xar_file_first(void *x, void *i) { (void)x; (void)i; return 0; }
void *xar_file_next(void *i) { (void)i; return 0; }
char *xar_get_path(void *f) { (void)f; return 0; }
int xar_prop_get(void *f, const char *key, const char **value) { (void)f; (void)key; if (value) *value = 0; return -1; }
int xar_prop_set(void *f, const char *key, const char *value) { (void)f; (void)key; (void)value; return -1; }
void *xar_subdoc_first(void *x) { (void)x; return 0; }
void *xar_subdoc_next(void *s) { (void)s; return 0; }
const char *xar_subdoc_name(void *s) { (void)s; return 0; }
void *xar_subdoc_new(void *x, const char *name) { (void)x; (void)name; return 0; }
int xar_subdoc_prop_get(void *s, const char *key, const char **value) { (void)s; (void)key; if (value) *value = 0; return -1; }
int xar_subdoc_prop_set(void *s, const char *key, const char *value) { (void)s; (void)key; (void)value; return -1; }
