/*
 * mOS libSystem — funções da libc do macOS que apps grandes usam e que faltavam (porte do VS Code,
 * docs/porte-vscode.md). O mOS só tem a codificação UTF-8; as variantes _l ignoram o locale.
 */
#include "sys.h"

size_t strlen(const char *s);
int cx_fdprintf(int fd, const char *fmt, ...);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
void *malloc(size_t n);
void free(void *p);


/* ---- diagnóstico do porte (/Sistema/Biblioteca/dyld-registrar): o carregador liga os stubs de
 * funções ausentes a esta função, que avisa uma vez por nome e devolve 0 */
/* RendererMainPlatformDelegate::EnableSandbox chama estes três e exige que existam
 * (o retorno de CGSSetDenyWindowServerConnections tem de ser kCGErrorSuccess, em coregraphics.c).
 * Não há LaunchServices nem CoreServices da Apple: o efeito pedido é "este processo não fala com eles". */
void SetApplicationIsDaemon(int is_daemon) { (void)is_daemon; }
void _LSSetApplicationLaunchServicesServerConnectionStatus(int status, void *err_block) { (void)status; (void)err_block; }
void _CSCheckFixDisable(void) {}

long cx_unresolved_log(const char *name) {
    static const char *seen[1024];
    static int nseen;
    for (int i = 0; i < nseen; i++) if (seen[i] == name) return 0;
    if (nseen < 1024) seen[nseen++] = name;
    cx_fdprintf(2, "dyld[mOS]: ausente (seguindo com 0): %s\n", name);
    return 0;
}

/* ---- thread_local por thread (chamado por cx_tlv_bootstrap, tlv.S). desc = {thunk, key, offset};
 * key = registro da imagem {modelo, tamanho inicializado, tamanho total, índice}. Os blocos de cada
 * thread ficam numa tabela no slot TSD_TLV da TSD (extra.c: cx_tsd). */
/* slots fixos da TSD (abaixo de KEY_FIRST = 128, extra.c), um por uso — NÃO repetir:
 *   100 TSD_THREAD_DICT (nsurl.c)   101 TSD_THREAD_OBJ (nsurl.c)   102 TSD_MIG_REPLY (mach.c)   103 TSD_ERRNO (libc.c)
 *   104 TSD_POOL (objc.c: autorelease pool da thread)
 *   110 TSD_TLV (aqui)
 * O TLV já esteve no 101 e colidia com o NSThread: um helper do Chrome que chamava +[NSThread currentThread]
 * tinha a tabela de thread_local sobrescrita pelo objeto e caía no próximo acesso a uma variável thread_local. */
#define TSD_TLV 110
#define TLV_IMAGES 128      /* índices de imagem (MAX_IMAGES de kernel/compat/macho.c, inclui as do dlopen) */
void **cx_tsd(void);
void *calloc(size_t n, size_t s);
void *cx_tlv_get(const u64 *desc) {
    const u64 *rec = (const u64 *)desc[1];
    void **t = cx_tsd();
    void **tab = t[TSD_TLV];
    if (!tab) { tab = calloc(TLV_IMAGES, sizeof(void *)); t[TSD_TLV] = tab; }
    u64 idx = rec[3] < TLV_IMAGES ? rec[3] : 0;
    if (!tab[idx]) {
        u64 total = rec[2] ? rec[2] : 16;
        u8 *b = calloc(1, (size_t)total);                 /* calloc: a parte __thread_bss já vem zerada */
        if (!b) {
            cx_fdprintf(2, "tlv[mOS]: sem memória para o TLS da thread (imagem %llu, %llu bytes, registro %llx)\n",
                        (unsigned long long)idx, (unsigned long long)total, (unsigned long long)(u64)rec);
            return (u8 *)0 + desc[2];
        }
        if (rec[0] && rec[1]) memcpy(b, (const void *)rec[0], (size_t)rec[1]);
        tab[idx] = b;
    }
    return (u8 *)tab[idx] + desc[2];
}

/* ---- os_log, syslog e asl: oslog.c (vão ao logd) */
/* objetos os_* (os_log, os_activity...): os do mOS são estáticos, então retain/release não fazem nada */
void *os_retain(void *o) { return o; }
void os_release(void *o) { (void)o; }

/* ---- CoreFoundation em ponte com o Foundation do mOS (CFStringRef = NSString*, CFBundleRef = NSBundle*...) */
typedef void *id_;
typedef struct cx_class cx_class_t;
cx_class_t *cx_class_named(const char *name);
id_ objc_msgSend(id_ self, void *op, ...);
void *sel_registerName(const char *name);
id_ objc_retain(id_ o);
void objc_release(id_ o);
#define MSG0_(T, o, s) ((T (*)(id_, void *))objc_msgSend)((id_)(o), sel_registerName(s))
#define MSG1_(T, o, s, a) ((T (*)(id_, void *, __typeof__(a)))objc_msgSend)((id_)(o), sel_registerName(s), (a))

/* abaixo de 64 KiB não há objeto: são as constantes falsas do mOS (kCFAllocatorMallocZone = 8...), que no macOS são
 * objetos estáticos em que retain/release não fazem nada */
id_ CFRetain(id_ o) { return (u64)o >= 0x10000 ? objc_retain(o) : o; }
void CFRelease(id_ o) { if ((u64)o >= 0x10000) objc_release(o); }

id_ CFBundleGetMainBundle(void) { return MSG0_(id_, cx_class_named("NSBundle"), "mainBundle"); }
/* regra "Create/Copy": quem chama é dono (+1) */
id_ CFStringCreateWithFileSystemRepresentation(void *alloc, const char *s) {
    (void)alloc;
    if (!s) return 0;
    /* +1 sem autorelease, como o CF: CFRetain em seguida dá retainCount 2 */
    return MSG1_(id_, MSG0_(id_, cx_class_named("NSString"), "alloc"), "initWithUTF8String:", s);
}

id_ CFBundleGetValueForInfoDictionaryKey(id_ bundle, id_ key) {
    return bundle && key ? MSG1_(id_, bundle, "objectForInfoDictionaryKey:", key) : 0;
}

/* ---- constantes CFString do CoreFoundation, no formato de @"..." ({isa, flags, ptr, len}) */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cx_cfconst_t;
#define CFCONST(var, text) \
    static cx_cfconst_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_s;
CFCONST(kCFBundleNameKey, "CFBundleName")
CFCONST(kCFBundleIdentifierKey, "CFBundleIdentifier")
CFCONST(kCFBundleVersionKey, "CFBundleVersion")
CFCONST(kCFBundleExecutableKey, "CFBundleExecutable")
CFCONST(kCFBundleInfoDictionaryVersionKey, "CFBundleInfoDictionaryVersion")
CFCONST(kCFBundleDevelopmentRegionKey, "CFBundleDevelopmentRegion")
CFCONST(kCFBundleLocalizationsKey, "CFBundleLocalizations")
CFCONST(kCFRunLoopDefaultMode, "kCFRunLoopDefaultMode")
CFCONST(kCFRunLoopCommonModes, "kCFRunLoopCommonModes")
CFCONST(NSDefaultRunLoopMode, "kCFRunLoopDefaultMode")
CFCONST(NSRunLoopCommonModes, "kCFRunLoopCommonModes")
CFCONST(NSLocalizedDescriptionKey, "NSLocalizedDescription")
CFCONST(NSLocalizedFailureReasonErrorKey, "NSLocalizedFailureReason")
CFCONST(NSLocalizedRecoverySuggestionErrorKey, "NSLocalizedRecoverySuggestion")
/* quarentena (LaunchServices): o Homebrew grava com.apple.quarantine nos downloads dos casks pela Fiddle */
CFCONST(kLSQuarantineAgentNameKey, "LSQuarantineAgentName")
CFCONST(kLSQuarantineAgentBundleIdentifierKey, "LSQuarantineAgentBundleIdentifier")
CFCONST(kLSQuarantineTimeStampKey, "LSQuarantineTimeStamp")
CFCONST(kCFURLQuarantinePropertiesKey, "NSURLQuarantinePropertiesKey")
/* o VFS não guarda atributos estendidos: a propriedade (ex.: a quarentena) é aceita e não fica gravada */
int CFURLSetResourcePropertyForKey(const void *url, const void *key, const void *value, void **err) { (void)url; (void)key; (void)value; if (err) *err = 0; return 1; }
CFCONST(NSUnderlyingErrorKey, "NSUnderlyingError")
CFCONST(NSFilePathErrorKey, "NSFilePath")
CFCONST(NSCocoaErrorDomain, "NSCocoaErrorDomain")
CFCONST(NSPOSIXErrorDomain, "NSPOSIXErrorDomain")
CFCONST(NSOSStatusErrorDomain, "NSOSStatusErrorDomain")
CFCONST(NSInvalidArgumentException, "NSInvalidArgumentException")
CFCONST(NSInternalInconsistencyException, "NSInternalInconsistencyException")
CFCONST(NSGenericException, "NSGenericException")
CFCONST(NSRangeException, "NSRangeException")
CFCONST(kCFErrorDomainPOSIX, "NSPOSIXErrorDomain")
CFCONST(kCFErrorDomainOSStatus, "NSOSStatusErrorDomain")
CFCONST(kCFErrorDomainCocoa, "NSCocoaErrorDomain")
CFCONST(kCFErrorLocalizedDescriptionKey, "NSLocalizedDescription")
CFCONST(kCFErrorUnderlyingErrorKey, "NSUnderlyingError")
CFCONST(kCFLocaleIdentifier, "locale:id")
CFCONST(kCFPreferencesCurrentApplication, "kCFPreferencesCurrentApplication")
CFCONST(kCFPreferencesAnyApplication, "kCFPreferencesAnyApplication")
CFCONST(kCFPreferencesCurrentUser, "kCFPreferencesCurrentUser")
CFCONST(kCFPreferencesAnyUser, "kCFPreferencesAnyUser")
CFCONST(kCFPreferencesCurrentHost, "kCFPreferencesCurrentHost")
CFCONST(kCFPreferencesAnyHost, "kCFPreferencesAnyHost")
/* kCFAllocatorDefault e kCFAllocatorSystemDefault são NULL de propósito: "use o padrão" */
const void *kCFAllocatorDefault = 0, *kCFAllocatorSystemDefault = 0, *kCFAllocatorMalloc = 0, *kCFAllocatorNull = 0;
id_ CFBundleGetIdentifier(id_ bundle) { return bundle ? MSG0_(id_, bundle, "bundleIdentifier") : 0; }

/* CFString: o NSString do mOS guarda UTF-8; os índices (CFRange) são tratados como bytes, o que vale
 * para ASCII (chaves, caminhos e nomes que o Chromium converte ao iniciar) */
#define ENC_UTF8  0x08000100u
#define ENC_ASCII 0x0600u
#define ENC_UTF16 0x0100u
/* as outras que o Chromium e o Node pedem: UTF-16 com ordem explícita (o base::SysNSStringToUTF16 usa LE), UTF-32,
 * Latin-1 e Mac Roman */
#define ENC_UTF16BE 0x10000100u
#define ENC_UTF16LE 0x14000100u
#define ENC_UTF32   0x0c000100u
#define ENC_UTF32BE 0x18000100u
#define ENC_UTF32LE 0x1c000100u
#define ENC_LATIN1  0x0201u
#define ENC_MACROMAN 0u
static const char *cf_utf8(id_ s) { return s ? MSG0_(const char *, s, "UTF8String") : 0; }
/* UTF-8 -> UTF-16 (sem terminador): devolve o número de unidades; out NULL só conta */
static long cf_u16(const char *s, u16 *out) {
    long n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (p && *p) {
        u32 c;
        int k;
        if (*p < 0x80) { c = *p; k = 1; }
        else if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) { c = (u32)(*p & 0x1F) << 6 | (p[1] & 0x3F); k = 2; }
        else if ((*p & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            c = (u32)(*p & 0x0F) << 12 | (u32)(p[1] & 0x3F) << 6 | (p[2] & 0x3F); k = 3;
        } else if ((*p & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
            c = (u32)(*p & 0x07) << 18 | (u32)(p[1] & 0x3F) << 12 | (u32)(p[2] & 0x3F) << 6 | (p[3] & 0x3F); k = 4;
        } else { c = 0xFFFD; k = 1; }
        p += k;
        if (c >= 0x10000) {
            if (out) { c -= 0x10000; out[n] = (u16)(0xD800 | (c >> 10)); out[n + 1] = (u16)(0xDC00 | (c & 0x3FF)); }
            n += 2;
        } else { if (out) out[n] = (u16)c; n++; }
    }
    return n;
}
/* em unidades UTF-16, como no CoreFoundation (e como o -[NSString length]); era em bytes UTF-8 */
long CFStringGetLength(id_ s) { const char *u = cf_utf8(s); return u ? cf_u16(u, 0) : 0; }
/* CFStringGetCStringPtr, CFStringGetCString e CFStringGetBytes: fdn_string_enc.c (codificações da ICU) */
unsigned char CFStringGetCString(id_ s, char *buf, long size, u32 enc);
/* tamanho máximo do caminho em UTF-8 (+ NUL) */
long CFStringGetMaximumSizeOfFileSystemRepresentation(id_ s) { return CFStringGetLength(s) * 3 + 1; }
u8 CFStringGetFileSystemRepresentation(id_ s, char *buf, long max) { return CFStringGetCString(s, buf, max, ENC_UTF8); }
/* CFStringCompare e as outras funções de texto: fdn_cfstring.c */
/* CFDictionary/CFArray/CFNumber em ponte com NSDictionary/NSArray/NSNumber */
id_ CFDictionaryGetValue(id_ d, id_ key) { return d && key ? MSG1_(id_, d, "objectForKey:", key) : 0; }
long CFDictionaryGetCount(id_ d) { return d ? MSG0_(long, d, "count") : 0; }
long CFArrayGetCount(id_ a) { return a ? MSG0_(long, a, "count") : 0; }
id_ CFArrayGetValueAtIndex(id_ a, long i) { return a ? MSG1_(id_, a, "objectAtIndex:", (u64)i) : 0; }

/* CFNotificationCenter: cfnotify.c (local, Darwin via notify_post, distributed só neste processo) */

/* SystemConfiguration (SCDynamicStore, proxies, CFNetworkCopySystemProxySettings): sysconfig.c, sobre o configd */

u8 CFEqual(id_ a, id_ b) { return a == b || (a && b && MSG1_(signed char, a, "isEqual:", b)); }
u64 CFHash(id_ a) { return a ? MSG0_(u64, a, "hash") : 0; }

/* "NoCopy": o mOS copia mesmo assim (o desalocador do chamador não é usado) */
id_ CFStringCreateWithBytes(void *alloc, const u8 *b, long n, u32 enc, u8 ext);
id_ CFStringCreateWithBytesNoCopy(void *alloc, const u8 *b, long n, u32 enc, u8 ext, void *dealloc) {
    (void)dealloc;
    return CFStringCreateWithBytes(alloc, b, n, enc, ext);
}

/* CFStringCreateWithCString e CFStringCreateWithBytes: fdn_string_enc.c */
/* ---- sysctl: informações do sistema. O mOS se apresenta como Darwin 23 (macOS 14) para apps que
 * exigem uma versão mínima; os números de CPU vêm do kernel. */
extern int cx_errno;
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
/* statvfs/statfs: o volume do sistema (FAT32); números aproximados, suficientes para "há espaço" */
typedef struct { u64 bsize, frsize; u32 blocks, bfree, bavail, files, ffree, favail; u64 fsid, flag, namemax; } cx_statvfs_t;
int statvfs(const char *path, void *out) {
    (void)path;
    if (!out) return -1;
    memset(out, 0, sizeof(cx_statvfs_t));
    cx_statvfs_t *s = out;
    s->bsize = s->frsize = 4096;
    s->blocks = 131072; s->bfree = s->bavail = 65536;        /* 512 MiB, metade livre */
    s->files = s->ffree = s->favail = 65536;
    s->namemax = 255;
    return 0;
}
int fstatvfs(int fd, void *out) { (void)fd; return statvfs("/", out); }
/* res_9_ninit e o resto do resolvedor estão em resolv.c. */

/* confstr: pastas do usuário do Darwin (o Chromium monta o sandbox dos processos filhos com elas) */
int mkdir(const char *p, unsigned mode);
size_t strlcpy(char *d, const char *s, size_t n);
size_t confstr(int name, char *buf, size_t len) {
    const char *v;
    switch (name) {
    case 65536: v = "/Users/Visitante/"; break;                       /* _CS_DARWIN_USER_DIR */
    case 65537: v = "/Users/Visitante/tmp/"; break;                   /* _CS_DARWIN_USER_TEMP_DIR */
    case 65538: v = "/Users/Visitante/Library/Caches/"; break;        /* _CS_DARWIN_USER_CACHE_DIR */
    case 1: v = "/usr/bin:/bin:/Sistema/Programas"; break;            /* _CS_PATH */
    default: cx_errno = CX_EINVAL; return 0;
    }
    if (name == 65537) mkdir("/Users/Visitante/tmp", 0700);
    if (name == 65538) { mkdir("/Users/Visitante/Library", 0700); mkdir("/Users/Visitante/Library/Caches", 0700); }
    size_t n = strlen(v) + 1;
    if (buf && len) strlcpy(buf, v, len);
    return n;
}

/* ---- arquivos temporários: troca os X finais por letras aleatórias até achar um nome livre */
int open(const char *path, int flags, ...);
int access(const char *p, int m);
void arc4random_buf(void *b, size_t n);
static int fill_template(char *t, int suffix) {
    size_t n = strlen(t);
    if ((size_t)suffix > n) return -1;
    size_t end = n - (size_t)suffix, x = 0;
    while (x < end && t[end - 1 - x] == 'X') x++;
    if (x < 6) { cx_errno = CX_EINVAL; return -1; }
    static const char al[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    u8 r[32];
    arc4random_buf(r, sizeof r);
    for (size_t i = 0; i < x; i++) t[end - x + i] = al[r[i % 32] % 62];
    return 0;
}
int mkstemps(char *t, int suffix) {
    for (int tries = 0; tries < 100; tries++) {
        if (fill_template(t, suffix) < 0) return -1;
        if (access(t, 0) == 0) continue;
        return open(t, 0x0202 | 0x0800, 0600);               /* O_RDWR | O_CREAT | O_EXCL (macOS) */
    }
    cx_errno = CX_EEXIST;
    return -1;
}
int mkstemp(char *t) { return mkstemps(t, 0); }
int mkostemp(char *t, int flags) { (void)flags; return mkstemps(t, 0); }
char *mktemp(char *t) { return fill_template(t, 0) < 0 ? 0 : t; }
char *mkdtemp(char *t) {
    for (int tries = 0; tries < 100; tries++) {
        if (fill_template(t, 0) < 0) return 0;
        if (access(t, 0) == 0) continue;
        return mkdir(t, 0700) == 0 ? t : 0;
    }
    return 0;
}

/* ---- poll: escrita sempre pronta; leitura pela consulta ao kernel (cx_fd_readable, kqueue.c).
 * Espera em fatias de 1 ms até o prazo (timeout < 0 = sem prazo). */
typedef struct { int fd; short events, revents; } cx_pollfd_t;
int cx_fd_readable(int fd, long *avail, int *eof);
int cx_fd_writable(int fd);                                    /* kqueue.c: sockets em connect não estão prontos */
int cx_kqueue_peek(int fd);
int poll(cx_pollfd_t *fds, unsigned n, int timeout) {
    u64 end = (u64)SYS0(SYS_TIME) + (u64)(timeout < 0 ? 0 : timeout);
    for (;;) {
        int ready = 0;
        for (unsigned i = 0; i < n; i++) {
            fds[i].revents = (short)((fds[i].events & 0x4) && cx_fd_writable(fds[i].fd) ? 0x4 : 0);   /* POLLOUT */
            long avail; int eof;
            int kq = cx_kqueue_peek(fds[i].fd);               /* fd de kqueue: legível com evento pronto */
            if (kq >= 0) { if ((fds[i].events & 0x1) && kq) fds[i].revents |= 0x1; }
            else if ((fds[i].events & 0x1) && cx_fd_readable(fds[i].fd, &avail, &eof)) fds[i].revents |= eof && !avail ? 0x11 : 0x1;  /* POLLIN (+POLLHUP) */
            if (fds[i].revents) ready++;
        }
        if (ready || timeout == 0 || (timeout > 0 && (u64)SYS0(SYS_TIME) >= end)) return ready;
        /* um sinal com tratador interrompe o cochilo (o kernel devolve -EINTR): poll/select voltam com EINTR, como no
         * macOS. O readline do bash espera em pselect e só trata ^C e SIGWINCH quando ele volta assim */
        if (SYS1(SYS_SLEEP, 1) == -4) { cx_errno = 4; return -1; }
    }
}

/* ---- select sobre o poll acima (fd_set do macOS: 1024 bits em int32; timeval {long sec; int usec}) */
typedef struct { long sec; int usec; } cx_timeval_t;
#define CX_FDSET_MAX 1024
static int fdset_has(const void *s, int fd) { return s && (((const u32 *)s)[fd / 32] >> (fd % 32)) & 1; }
static void fdset_put(void *s, int fd) { ((u32 *)s)[fd / 32] |= 1u << (fd % 32); }
int select(int nfds, void *rd, void *wr, void *ex, cx_timeval_t *tv) {
    cx_pollfd_t p[64];               /* até 64 descritores marcados (o polling do Node usa um) */
    unsigned n = 0;
    if (nfds > CX_FDSET_MAX) nfds = CX_FDSET_MAX;
    for (int fd = 0; fd < nfds && n < 64; fd++) {
        short ev = (short)((fdset_has(rd, fd) ? 0x1 : 0) | (fdset_has(wr, fd) ? 0x4 : 0));
        if (ev) { p[n].fd = fd; p[n].events = ev; p[n].revents = 0; n++; }
    }
    int timeout = tv ? (int)(tv->sec * 1000 + tv->usec / 1000) : -1;
    int r = poll(p, n, timeout);
    unsigned words = (unsigned)(nfds + 31) / 32;
    for (unsigned w = 0; w < words; w++) { if (rd) ((u32 *)rd)[w] = 0; if (wr) ((u32 *)wr)[w] = 0; if (ex) ((u32 *)ex)[w] = 0; }
    int count = 0;
    for (unsigned i = 0; r > 0 && i < n; i++) {
        if (rd && (p[i].revents & 0x11)) { fdset_put(rd, p[i].fd); count++; }
        if (wr && (p[i].revents & 0x4)) { fdset_put(wr, p[i].fd); count++; }
    }
    return r < 0 ? r : count;
}
int select$1050(int nfds, void *rd, void *wr, void *ex, cx_timeval_t *tv) { return select(nfds, rd, wr, ex, tv); }
int select$DARWIN_EXTSN(int nfds, void *rd, void *wr, void *ex, cx_timeval_t *tv) { return select(nfds, rd, wr, ex, tv); }
/* FD_SET do SDK novo pergunta se o fd cabe no fd_set */
int __darwin_check_fd_set_overflow(int fd, const void *set, int unlimited) { (void)set; return unlimited || (fd >= 0 && fd < CX_FDSET_MAX); }

/* file actions de posix_spawn ficam em extra.c (dup2, close, open, chdir) */
int posix_spawnattr_setbinpref_np(void *a, size_t n, int *pref, size_t *ocount) { (void)a; (void)pref; if (ocount) *ocount = n; return 0; }
/* porta especial do PRÓXIMO filho de posix_spawn (bootstrap, which = 4): o mach.c/kernel usam no
 * machipc_spawn. Movido para o kernel (MACH_OP_SPAWN_SPECIAL); o objeto `a` do atributo não é usado. */
int posix_spawnattr_setspecialport_np(void *a, u32 port, int which) {
    (void)a;
    u64 args[8] = {(u64)which, port, 0, 0, 0, 0, 0, 0};
    cx_syscall(SYS_MACH_PORT, (long)MACH_OP_SPAWN_SPECIAL, (long)args, 0, 0, 0);
    return 0;
}

/* bootstrap (launchd), task_get/set_special_port e bootstrap_port: agora em userland/libsystem/mach.c,
 * usando as syscalls de portas Mach do kernel (registro de serviços global entre processos). */

/* ---- sandbox e Keychain: o mOS não tem; "permitido" e "sem keychain" */
/* Sandbox (Seatbelt): o mOS não restringe nada, mas aceita os perfis e lembra que um foi aplicado.
 * sandbox_check(pid, NULL, 0) ("o processo está em sandbox?") responde sim depois de um perfil aplicado,
 * ou num helper do Chromium (--type=...) sem --no-sandbox: no macOS ele recebe o perfil do processo
 * principal na partida (Seatbelt exec) e o renderer faz CHECK disso. Operações específicas: permitidas. */
char *strdup(const char *s);
static volatile int sandbox_applied;
static int helper_expects_sandbox(void) {
    extern char **NXArgv;
    int helper = 0, nosb = 0;
    for (int i = 1; NXArgv && NXArgv[i]; i++) {
        if (!strncmp(NXArgv[i], "--type=", 7)) helper = 1;
        if (!strcmp(NXArgv[i], "--no-sandbox")) nosb = 1;
    }
    return helper && !nosb;
}
int sandbox_check(int pid, const char *op, int type, ...) {
    (void)type;
    if (op) return 0;                                          /* operação permitida */
    if (pid != (int)SYS0(SYS_GETPID)) return 0;
    return sandbox_applied || helper_expects_sandbox();
}
int sandbox_init(const char *profile, u64 flags, char **err) { (void)profile; (void)flags; if (err) *err = 0; sandbox_applied = 1; return 0; }
int sandbox_init_with_parameters(const char *profile, u64 flags, const char *const params[], char **err) {
    (void)profile; (void)flags; (void)params; if (err) *err = 0; sandbox_applied = 1; return 0;
}
void sandbox_free_error(char *err) { (void)err; }
/* API privada usada pelo sandbox::Seatbelt do Chromium. O perfil compilado NÃO é opaco: o Chromium lê os campos de
 * struct sandbox_profile {builtin, data, size} (sandbox/mac/seatbelt.cc) e copia `size` bytes de `data` para mandar ao
 * helper. Antes era uma struct de 8 bytes: o Chrome lia o "size" 16 bytes adiante, na memória estática vizinha (a classe
 * OS_dispatch_queue_main), e pedia uma alocação de ~70 TiB — o PartitionAlloc derrubava o processo por falta de memória.
 * O mOS não aplica sandbox (sandbox_apply só marca): o bytecode é um marcador pequeno. */
typedef struct { const char *builtin; const u8 *data; size_t size; } sb_profile_t;
static const u8 sb_bytecode[8] = {'m', 'O', 'S', '-', 's', 'b', 0, 0};
static sb_profile_t sb_profile = {0, sb_bytecode, sizeof sb_bytecode};
typedef struct { u64 magic, pad[7]; } sb_params_t;          /* parâmetros: opacos para quem chama */
static sb_params_t sb_params = {0x5350, {0}};
void *sandbox_create_params(void) { return &sb_params; }
int sandbox_set_param(void *params, const char *key, const char *value) { (void)params; (void)key; (void)value; return 0; }
void sandbox_free_params(void *params) { (void)params; }
void *sandbox_compile_string(const char *profile, void *params, char **err) { (void)profile; (void)params; if (err) *err = 0; return &sb_profile; }
void sandbox_free_profile(void *profile) { (void)profile; }
int sandbox_apply(void *profile) { (void)profile; sandbox_applied = 1; return 0; }
char *sandbox_extension_issue_file(const char *ext_class, const char *path, u32 flags) {
    (void)ext_class; (void)flags;
    return strdup(path ? path : "");                           /* "token" da extensão: o próprio caminho */
}
/* extensões: o token é "<classe>;<alvo>" (o macOS dá um texto opaco assinado; quem recebe só o repassa ao consume).
 * O consume devolve um handle positivo novo a cada chamada; release com handle inválido dá EINVAL, como no macOS.
 * Nada é negado no mOS, então a extensão só registra o que foi pedido (docs/darwin-bsd-mach.md, F10). */
static volatile long sb_next_handle = 1;
static char *sb_token(const char *cls, const char *what) {
    size_t a = cls ? strlen(cls) : 0, b = what ? strlen(what) : 0;
    char *t = malloc(a + b + 2);
    if (!t) return 0;
    if (a) memcpy(t, cls, a);
    t[a] = ';';
    if (b) memcpy(t + a + 1, what, b);
    t[a + b + 1] = 0;
    return t;
}
char *sandbox_extension_issue_mach(const char *ext_class, const char *name, u32 flags) { (void)flags; return name ? sb_token(ext_class, name) : 0; }
char *sandbox_extension_issue_generic(const char *ext_class, u32 flags) { (void)flags; return sb_token(ext_class, ""); }
char *sandbox_extension_issue_file_to_process(const char *ext_class, const char *path, u32 flags, const u32 *tok) { (void)tok; return sandbox_extension_issue_file(ext_class, path, flags); }
char *sandbox_extension_issue_mach_to_process(const char *ext_class, const char *name, u32 flags, const u32 *tok) { (void)tok; return sandbox_extension_issue_mach(ext_class, name, flags); }
int sandbox_issue_fs_extension(const char *path, u64 flags, char **token) { if (!path || !token) { cx_errno = CX_EINVAL; return -1; } *token = sb_token("com.apple.app-sandbox.read-write", path); (void)flags; return *token ? 0 : -1; }
long sandbox_extension_consume(const char *token) { if (!token) { cx_errno = CX_EINVAL; return -1; } return __sync_fetch_and_add(&sb_next_handle, 1); }
int sandbox_extension_release(long handle) { if (handle <= 0 || handle >= sb_next_handle) { cx_errno = CX_EINVAL; return -1; } return 0; }
int sandbox_extension_release_file(long handle, const char *path) { (void)path; return sandbox_extension_release(handle); }
int sandbox_extension_update_file(long handle, const char *path) { (void)path; return handle > 0 ? 0 : -1; }
/* audit token: pid no índice 5 */
int sandbox_check_by_audit_token(const u32 *tok, const char *op, int type, ...) {
    if (!tok) { cx_errno = CX_EINVAL; return -1; }
    return sandbox_check((int)tok[5], op, type);
}
int sandbox_check_by_uniqueid(u32 uid, u64 uniqueid, const char *op, int type, ...) { (void)uid; (void)uniqueid; (void)op; (void)type; return 0; }
/* contêiner: só existe para um processo em sandbox de app; o mOS não troca o HOME, então nenhum processo tem um
 * (o macOS devolve erro para quem não está em contêiner) */
int sandbox_container_path_for_pid(int pid, char *buf, size_t size) { (void)pid; if (buf && size) buf[0] = 0; cx_errno = CX_ESRCH; return -1; }
int sandbox_container_path_for_audit_token(const u32 *tok, char *buf, size_t size) { return sandbox_container_path_for_pid(tok ? (int)tok[5] : 0, buf, size); }
/* atributos de spawn: guardados para o filho (o mOS não aplica o perfil; o helper só pergunta sandbox_check) */
typedef struct { char profile[128]; char container[128]; u32 flags; } sb_spawnattrs_t;
void sandbox_spawnattrs_init(void *a) { if (a) memset(a, 0, sizeof(sb_spawnattrs_t)); }
int sandbox_spawnattrs_setprofilename(void *a, const char *n) { if (!a || !n) return CX_EINVAL; strlcpy(((sb_spawnattrs_t *)a)->profile, n, 128); return 0; }
int sandbox_spawnattrs_setcontainer(void *a, const char *c) { if (!a || !c) return CX_EINVAL; strlcpy(((sb_spawnattrs_t *)a)->container, c, 128); return 0; }
int sandbox_note(const char *note) { (void)note; return 0; }
int sandbox_suspend(int pid) { (void)pid; return 0; }
int sandbox_unsuspend(void) { return 0; }
int sandbox_query_approval_policy_for_path(const char *op, const char *path, char **out) { (void)op; (void)path; if (out) *out = 0; return 0; }
int sandbox_query_user_intent_for_process_with_audit_token(const char *op, const u32 *tok, int *out) { (void)op; (void)tok; if (out) *out = 1; return 0; }

/* csops: o binário não é assinado (sem CS_VALID, igual ao SecCode). ADHOC + GET_TASK_ALLOW +
 * DEBUGGED deixam o V8 e o Crashpad seguirem sem tratar o processo como inválido. */
int csops(int pid, unsigned int ops, void *useraddr, size_t usersize) {
    (void)pid;
    if (ops == 0) {                                                                 /* CS_OPS_STATUS */
        if (!useraddr || usersize < 4) { cx_errno = CX_EINVAL; return -1; }
        *(u32 *)useraddr = 0x2u | 0x4u | 0x10000000u | 0x40000000u;                 /* ADHOC | GET_TASK_ALLOW | DEBUGGED | DEV_CODE */
        return 0;
    }
    if (ops == 5 && useraddr && usersize >= 20) { memset(useraddr, 0, 20); return 0; } /* CS_OPS_CDHASH */
    /* CS_OPS_TEAMID: sem Team ID (o XNU responde ENOENT); CS_OPS_VALIDATION_CATEGORY: CS_VALIDATION_CATEGORY_NONE.
     * O ProcessRequirement do Chromium (rendezvous Mach com os helpers) trata isso como build sem assinatura e
     * pula a validação; com ENOSYS o requisito falhava e os renderers não recebiam as portas ("about:blank
     * demorou demais" e um renderer novo atrás do outro). */
    if (ops == 14) { cx_errno = CX_ENOENT; return -1; }
    if (ops == 17 && useraddr && usersize >= 4) { *(u32 *)useraddr = 10; return 0; }
    if (ops == 4 && useraddr && usersize > 1) {                                     /* CS_OPS_PIDPATH */
        extern char **NXArgv;
        const char *p = NXArgv && NXArgv[0] ? NXArgv[0] : "/app";
        size_t n = strlen(p);
        if (n + 1 > usersize) { cx_errno = CX_ENOMEM; return -1; }
        memcpy(useraddr, p, n + 1);
        return 0;
    }
    cx_errno = CX_ENOSYS;
    return -1;
}
int csops_audittoken(int pid, unsigned int ops, void *useraddr, size_t usersize, void *token) {
    (void)token;
    return csops(pid, ops, useraddr, usersize);
}
int SecKeychainAddCallback(void *cb, u32 mask, void *ctx) { (void)cb; (void)mask; (void)ctx; return 0; }
int SecKeychainRemoveCallback(void *cb) { (void)cb; return 0; }

/* ---- NSSearchPathForDirectoriesInDomains: pastas do usuário do mOS */
id_ NSSearchPathForDirectoriesInDomains(unsigned long dir, unsigned long mask, u8 expand) {
    (void)expand;
    /* NSApplicationDirectory (1), NSAdminApplicationDirectory (4), NSAllApplicationsDirectory (100): os apps do mOS
     * ficam em /Aplicativos (domínio local, máscara 2) e ~/Applications (usuário, 1). O Electron pergunta aqui no
     * app.isInApplicationsFolder(); com ~/Library, o Grok Bot abria o "Mover para a pasta Aplicativos?" e parava */
    if (dir == 1 || dir == 4 || dir == 100) {
        id_ a = MSG0_(id_, cx_class_named("NSMutableArray"), "array");
        id_ str = cx_class_named("NSString");
        if (mask & 1) MSG1_(void, a, "addObject:", MSG1_(id_, str, "stringWithUTF8String:", dir == 4 ? "/Users/Visitante/Applications/Utilities" : "/Users/Visitante/Applications"));
        if (mask & 2) MSG1_(void, a, "addObject:", MSG1_(id_, str, "stringWithUTF8String:", dir == 4 ? "/Aplicativos/Utilitários" : "/Aplicativos"));
        if (dir == 100 && (mask & 2)) MSG1_(void, a, "addObject:", MSG1_(id_, str, "stringWithUTF8String:", "/Aplicativos/Utilitários"));
        return a;
    }
    const char *p;
    switch (dir) {
    case 5: p = "/Users/Visitante/Library"; break;                        /* NSLibraryDirectory */
    case 9: p = "/Users/Visitante/Documentos"; break;                     /* NSDocumentDirectory */
    case 12: p = "/Users/Visitante/Mesa"; break;                          /* NSDesktopDirectory */
    case 13: p = "/Users/Visitante/Library/Caches"; break;                /* NSCachesDirectory */
    case 14: p = "/Users/Visitante/Library/Application Support"; break;   /* NSApplicationSupportDirectory */
    case 15: p = "/Users/Visitante/Downloads"; break;                     /* NSDownloadsDirectory */
    default: p = "/Users/Visitante/Library"; break;
    }
    if (dir == 14 || dir == 13) { mkdir("/Users/Visitante/Library", 0700); mkdir(p, 0700); }
    id_ a = MSG0_(id_, cx_class_named("NSMutableArray"), "array");
    MSG1_(void, a, "addObject:", MSG1_(id_, cx_class_named("NSString"), "stringWithUTF8String:", p));
    return a;
}

/* variantes _l (locale): o mOS só tem o locale C/UTF-8 */
int vsnprintf(char *b, size_t n, const char *fmt, __builtin_va_list ap);
int snprintf_l(char *b, size_t n, void *loc, const char *fmt, ...) {
    (void)loc;
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int r = vsnprintf(b, n, fmt, ap);
    __builtin_va_end(ap);
    return r;
}
int pthread_getname_np(void *t, char *buf, size_t len) { (void)t; if (buf && len) buf[0] = 0; return 0; }
/* sysctl, sysctlbyname e sysctlnametomib: sysctl.c */

/* ---- atributos de pthread que faltavam (o pthread_create do mOS usa pilhas de 512 KiB) */
int pthread_attr_getstacksize(const void *a, size_t *s) { (void)a; if (s) *s = 512 << 10; return 0; }
int pthread_attr_getstack(const void *a, void **addr, size_t *s) { (void)a; if (addr) *addr = 0; if (s) *s = 512 << 10; return 0; }
int pthread_attr_setstack(void *a, void *addr, size_t s) { (void)a; (void)addr; (void)s; return 0; }
int pthread_attr_setdetachstate(void *a, int st) { (void)a; (void)st; return 0; }
int pthread_attr_getdetachstate(const void *a, int *st) { (void)a; if (st) *st = 1; return 0; }   /* JOINABLE */
size_t cx_attr_getguard(const void *a);   /* extra.c: 16 KiB no arm64 (página guest); 4096 no x86_64 */
int cx_attr_setguard(void *a, size_t s);
int pthread_attr_setguardsize(void *a, size_t s) { return cx_attr_setguard(a, s); }
int pthread_attr_getguardsize(const void *a, size_t *s) {
#if defined(__aarch64__)
    if (!s) return 22;   /* EINVAL, como no macOS */
#else
    if (!s) return 0;    /* x86_64: o comportamento anterior, ponteiro nulo ignorado */
#endif
    *s = cx_attr_getguard(a);
    return 0;
}
int pthread_attr_setscope(void *a, int s) { (void)a; (void)s; return 0; }
int pthread_attr_setschedpolicy(void *a, int p) { (void)a; (void)p; return 0; }
int pthread_attr_setschedparam(void *a, const void *p) { (void)a; (void)p; return 0; }
int pthread_attr_getschedparam(const void *a, int *p) { (void)a; if (p) *p = 31; return 0; }
int pthread_attr_setinheritsched(void *a, int i) { (void)a; (void)i; return 0; }
int pthread_attr_set_qos_class_np(void *a, unsigned q, int r) { (void)a; (void)q; (void)r; return 0; }
/* QoS por thread (tid -> classe); 0 no tid = vaga livre. A thread principal nasce user-interactive, as outras default. */
#define QOS_SLOTS 512
static struct { volatile int tid; unsigned qos; int rel; } qos_tab[QOS_SLOTS];
static int qos_slot(int tid, int make) {
    unsigned h = (unsigned)tid % QOS_SLOTS;
    for (int n = 0; n < QOS_SLOTS; n++, h = (h + 1) % QOS_SLOTS) {
        if (qos_tab[h].tid == tid) return (int)h;
        if (!qos_tab[h].tid) {
            if (!make) return -1;
            if (__sync_bool_compare_and_swap(&qos_tab[h].tid, 0, tid)) return (int)h;
            if (qos_tab[h].tid == tid) return (int)h;
        }
    }
    return -1;
}
extern int cx_main_tid;
static unsigned qos_of(int tid, int *rel) {
    int i = qos_slot(tid, 0);
    if (rel) *rel = i >= 0 ? qos_tab[i].rel : 0;
    if (i >= 0) return qos_tab[i].qos;
    return cx_main_tid && tid == cx_main_tid ? 0x21 : 0x15;
}
int pthread_set_qos_class_self_np(unsigned q, int r) {
    if (q != 0x21 && q != 0x19 && q != 0x15 && q != 0x11 && q != 0x09 && q != 0x05) return 22;   /* EINVAL */
    if (r > 0 || r < -15) return 22;
    int tid = (int)SYS0(SYS_GETTID);
    int i = qos_slot(tid, 1);
    if (i >= 0) { qos_tab[i].qos = q; qos_tab[i].rel = r; }
    SYS3(60, 5, 0, q);                                            /* SYS_THREADCTL, MOS_THR_SETQOS, tid 0 = atual */
    return 0;
}
unsigned qos_class_self(void) { return qos_of((int)SYS0(SYS_GETTID), 0); }
long dw_pthread_tid(void *t) __asm__("cx_pthread_tid");         /* extra.c (devolve long) */
int pthread_get_qos_class_np(void *t, unsigned *q, int *r) {
    unsigned v = qos_of((int)dw_pthread_tid(t), r);
    if (q) *q = v;
    return 0;
}
unsigned qos_class_main(void) { return 0x21; }
/* prioridade Mach da thread (SYS_THREADCTL MOS_THR_SETPRIO, kernel/proc/sched.c). SCHED_FIFO (4) e SCHED_RR (2)
 * ficam fixas, sem decaimento; SCHED_OTHER (1) é timeshare. A última escolha de cada thread fica numa tabela pequena
 * para o getschedparam. */
extern int cx_pthread_tid(void *t);
static struct { int tid, pol, pri; } sched_memo[64];
int pthread_setschedparam(void *t, int pol, const void *p) {
    if (!p) return CX_EINVAL;
    int pri = *(const int *)p, tid = t ? cx_pthread_tid(t) : 0;
    if (pri < 0 || pri > 127 || (pol != 1 && pol != 2 && pol != 4)) return CX_EINVAL;
    long r = SYS3(60, 8 /* MOS_THR_SETPRIO */, tid, (unsigned)pri | (pol != 1 ? 0x100u : 0));
    if (r < 0) return (int)-r;
    int slot = tid & 63;
    sched_memo[slot].tid = tid; sched_memo[slot].pol = pol; sched_memo[slot].pri = pri;
    return 0;
}
int pthread_getschedparam(void *t, int *pol, int *p) {
    int tid = t ? cx_pthread_tid(t) : 0, slot = tid & 63;
    int hit = sched_memo[slot].tid == tid && sched_memo[slot].pri;
    if (pol) *pol = hit ? sched_memo[slot].pol : 1;
    if (p) *p = hit ? sched_memo[slot].pri : 31;
    return 0;
}
int sched_get_priority_min(int p) { (void)p; return 15; }
int sched_get_priority_max(int p) { (void)p; return 47; }

/* ---- limites de recursos */
typedef struct { u64 cur, max; } cx_rlimit_t;
#define RLIM_INFINITY 0x7FFFFFFFFFFFFFFFull
/* SYS_VMCTL (kernel/include/vmpolicy.h): o kernel guarda os limites por processo e o fork/spawn herda */
int getrlimit(int res, cx_rlimit_t *r) {
    if (!r) { cx_errno = CX_EFAULT; return -1; }
    long e = SYS3(SYS_VMCTL, MOS_VM_GETRLIM, res, r);
    if (e < 0) { cx_errno = (int)-e; return -1; }
    return 0;
}
int setrlimit(int res, const cx_rlimit_t *r) {
    if (!r) { cx_errno = CX_EFAULT; return -1; }
    long e = SYS3(SYS_VMCTL, MOS_VM_SETRLIM, res, r);
    if (e < 0) { cx_errno = (int)-e; return -1; }
    return 0;
}

/* MB_CUR_MAX no macOS é ___mb_cur_max() */
int __mb_cur_max = 4;
int ___mb_cur_max(void) { return 4; }
int ___mb_cur_max_l(void *loc) { (void)loc; return 4; }
