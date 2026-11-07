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
