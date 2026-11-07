/*
 * mOS — libSystem: o que o Codex CLI (binário Rust, codex-x86_64-apple-darwin) importa e o mOS não tinha.
 * Medido com tools/machocheck.c (docs/compat-apps.md): 87 funções e constantes; vários servem também aos Electron
 * (FSEventStream* do policy-watcher do VS Code, IOPMAssertion*, CFError*, proxies do Chromium).
 *
 * Regra (a mesma de chrome.c/security.c): quando o mOS não tem o recurso, devolve o erro que a API documenta,
 * nunca sucesso sem preencher a saída. O que funciona de verdade: termios guardado por processo (tcgetattr/
 * tcsetattr/cfmakeraw), proc_pidpath/proc_listchildpids pela lista de processos do kernel, waitid sobre waitpid,
 * killpg, strsignal, umask, CFError, CFURL (as que faltavam em nsurl.c), CFTimeZone, CFPreferences vazio,
 * proxies = "sem proxy", FSEvents = fluxo que nunca dispara (o mOS ainda não notifica mudanças no disco).
 */
#include "sys.h"

size_t strlen(const char *s);
size_t strlcpy(char *d, const char *s, size_t n);
int strcmp(const char *a, const char *b);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
char *getenv(const char *name);
int isatty(int fd);
int getpid(void);
int access(const char *p, int m);
int waitpid(int pid, int *st, int opt);
int _NSGetExecutablePath(char *buf, u32 *size);
void srandom(unsigned s);
u32 arc4random(void);
int cx_snprintf(char *buf, size_t n, const char *fmt, ...);
extern int cx_errno;
#define CX_ENOTSUP 45
#define CX_ECHILD 10
#define CX_ENOATTR 93

/* ================================================================ termios (estrutura do macOS: tcflag_t e speed_t
 * são unsigned long) */
typedef unsigned long tcflag_t;
typedef struct { tcflag_t iflag, oflag, cflag, lflag; u8 cc[20]; unsigned long ispeed, ospeed; } cx_termios_t;
/* valores do BSD */
#define BRKINT 0x2
#define ICRNL 0x100
#define INPCK 0x10
#define ISTRIP 0x20
#define IXON 0x200
#define IGNBRK 0x1
#define PARMRK 0x8
#define INLCR 0x40
#define IGNCR 0x80
#define OPOST 0x1
#define ONLCR 0x2
#define CSIZE 0x300
#define CS8 0x300
#define CREAD 0x800
#define PARENB 0x1000
#define ECHO 0x8
#define ECHOE 0x2
#define ECHOK 0x4
#define ECHONL 0x10
#define ICANON 0x100
#define ISIG 0x80
#define IEXTEN 0x400
#define VEOF 0
#define VEOL 1
#define VERASE 3
#define VKILL 5
#define VINTR 8
#define VQUIT 9
#define VSUSP 10
#define VMIN 16
#define VTIME 17
static cx_termios_t tio;
static int tio_init;
static void tio_default(void) {
    if (tio_init) return;
    tio_init = 1;
    tio.iflag = BRKINT | ICRNL | IXON;
    tio.oflag = OPOST | ONLCR;
    tio.cflag = CS8 | CREAD;
    tio.lflag = ECHO | ECHOE | ECHOK | ICANON | ISIG | IEXTEN;
    tio.cc[VEOF] = 4; tio.cc[VEOL] = 0xff; tio.cc[VERASE] = 0x7f; tio.cc[VKILL] = 21; tio.cc[VINTR] = 3;
    tio.cc[VQUIT] = 28; tio.cc[VSUSP] = 26; tio.cc[VMIN] = 1; tio.cc[VTIME] = 0;
    tio.ispeed = tio.ospeed = 38400;
}
/* o terminal do mOS ainda não muda de modo pelo processo: o estado é guardado e devolvido (crossterm, rustyline
 * e afins leem de volta o que pediram), e a entrada continua chegando como o Terminal do mOS a entrega */
int tcgetattr(int fd, cx_termios_t *t) {
    if (!isatty(fd)) { cx_errno = CX_ENOTTY; return -1; }
    if (!t) { cx_errno = CX_EINVAL; return -1; }
    tio_default();
    *t = tio;
    return 0;
}
int tcsetattr(int fd, int act, const cx_termios_t *t) {
    (void)act;
    if (!isatty(fd)) { cx_errno = CX_ENOTTY; return -1; }
    if (!t) { cx_errno = CX_EINVAL; return -1; }
    tio_default();
    tio = *t;
    return 0;
}
void cfmakeraw(cx_termios_t *t) {
    if (!t) return;
    t->iflag &= ~(tcflag_t)(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    t->oflag &= ~(tcflag_t)OPOST;
    t->lflag &= ~(tcflag_t)(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    t->cflag &= ~(tcflag_t)(CSIZE | PARENB);
    t->cflag |= CS8;
    t->cc[VMIN] = 1; t->cc[VTIME] = 0;
}
int tcflush(int fd, int q) { (void)q; if (!isatty(fd)) { cx_errno = CX_ENOTTY; return -1; } return 0; }
int tcgetpgrp(int fd) { if (!isatty(fd)) { cx_errno = CX_ENOTTY; return -1; } return getpid(); }
int ttyname_r(int fd, char *buf, size_t n) {
    if (!isatty(fd)) return CX_ENOTTY;
    if (strlcpy(buf, "/dev/ttys000", n) >= n) return CX_ERANGE;
    return 0;
}
/* pseudoterminais: o kernel ainda não tem */
int openpty(int *m, int *s, char *name, const void *t, const void *ws) {
    (void)m; (void)s; (void)name; (void)t; (void)ws;
    cx_errno = CX_ENOENT;
    return -1;
}

/* ================================================================ processos */
int getpgid(int pid) { return pid ? pid : getpid(); }        /* cada processo é o próprio grupo */
int killpg(int pg, int sig) {
    if (pg <= 0) { cx_errno = CX_EINVAL; return -1; }
    extern int kill(int, int);   /* libc.c: traduz o número do sinal para o do kernel */
    long r = kill(pg, sig);
    if (r < 0) { cx_errno = CX_ESRCH; return -1; }
    return 0;
}
/* waitid(idtype, id, siginfo_t*, options) sobre waitpid. siginfo_t do macOS: signo errno code pid uid status ... */
#define P_ALL 0
#define P_PID 1
#define WEXITED 4
#define WNOHANG 1
#define WNOWAIT 0x20
int waitid(int idtype, unsigned id, int *info, int options) {
    if (options & WNOWAIT) { cx_errno = CX_EINVAL; return -1; }   /* não dá para espiar sem colher no mOS */
    int pid = idtype == P_PID ? (int)id : -1;
    if (idtype != P_PID && idtype != P_ALL) { cx_errno = CX_EINVAL; return -1; }
    int st = 0;
    int r = waitpid(pid, &st, options & WNOHANG);
    if (r < 0) return -1;
    if (info) {
        memset(info, 0, 104);                                   /* sizeof(siginfo_t) no macOS */
        if (r > 0) {
            info[0] = 20;                                       /* SIGCHLD */
            int sig = st & 0x7f;
            info[2] = sig ? 2 : 1;                              /* CLD_KILLED : CLD_EXITED */
            info[3] = r;
            info[4] = 501;
            info[5] = sig ? sig : (st >> 8) & 0xff;
        }
    }
    return 0;
}
static const char *const signames[32] = {
    "Unknown signal: 0", "Hangup: 1", "Interrupt: 2", "Quit: 3", "Illegal instruction: 4", "Trace/BPT trap: 5",
    "Abort trap: 6", "EMT trap: 7", "Floating point exception: 8", "Killed: 9", "Bus error: 10",
    "Segmentation fault: 11", "Bad system call: 12", "Broken pipe: 13", "Alarm clock: 14", "Terminated: 15",
    "Urgent I/O condition: 16", "Suspended (signal): 17", "Suspended: 18", "Continued: 19", "Child exited: 20",
    "Stopped (tty input): 21", "Stopped (tty output): 22", "I/O possible: 23", "Cputime limit exceeded: 24",
    "Filesize limit exceeded: 25", "Virtual timer expired: 26", "Profiling timer expired: 27",
    "Window size changes: 28", "Information request: 29", "User defined signal 1: 30", "User defined signal 2: 31",
};
char *strsignal(int sig) {
    static char other[32];
    if (sig >= 0 && sig < 32) return (char *)signames[sig];
    cx_snprintf(other, sizeof other, "Unknown signal: %d", sig);
    return other;
}
/* libproc: pela lista de processos do kernel (SYS_PROCLIST) */
static int proc_find(int pid, mos_procinfo_t *out) {
    mos_procinfo_t pi;
    for (long i = 0; SYS2(SYS_PROCLIST, i, &pi) == 1; i++) if (pi.pid == pid) { if (out) *out = pi; return 1; }
    return 0;
}
int proc_pidpath(int pid, char *buf, u32 size) {
    if (!buf || !size) { cx_errno = CX_EINVAL; return 0; }
    if (pid == getpid()) {
        u32 n = size;
        if (_NSGetExecutablePath(buf, &n) != 0) { cx_errno = CX_ENOMEM; return 0; }
        return (int)strlen(buf);
    }
    mos_procinfo_t pi;
    if (!proc_find(pid, &pi)) { cx_errno = CX_ESRCH; return 0; }
    pi.name[sizeof pi.name - 1] = 0;
    return (int)strlcpy(buf, pi.name, size) < (int)size ? (int)strlen(buf) : 0;   /* o kernel só guarda o nome curto */
}
int proc_listchildpids(int ppid, int *buf, int bytes) {
    int n = 0, cap = buf ? bytes / (int)sizeof(int) : 0;
    mos_procinfo_t pi;
    for (long i = 0; SYS2(SYS_PROCLIST, i, &pi) == 1; i++)
        if (pi.ppid == ppid) { if (n < cap) buf[n] = pi.pid; n++; }
    return buf ? (n < cap ? n : cap) * (int)sizeof(int) : n;   /* com buf: bytes preenchidos (como o libproc) */
}
int proc_listpgrppids(int pgrp, int *buf, int bytes) {
    /* cada processo é o próprio grupo */
    if (!proc_find(pgrp, 0)) return 0;
    if (buf && bytes >= (int)sizeof(int)) { buf[0] = pgrp; return (int)sizeof(int); }
    return 1;
}
int proc_pidinfo(int pid, int flavor, u64 arg, void *buf, int size) {
    (void)pid; (void)flavor; (void)arg; (void)buf; (void)size;
    cx_errno = CX_ENOTSUP;                                      /* sem PROC_PIDTASKINFO e afins: 0 = falha */
    return 0;
}
int proc_pid_rusage(int pid, int flavor, void *buf) { (void)pid; (void)flavor; (void)buf; cx_errno = CX_ENOTSUP; return -1; }

/* ================================================================ arquivos */
static unsigned cur_umask = 022;
unsigned umask(unsigned m) { unsigned o = cur_umask; cur_umask = m & 0777; return o; }
/* travas de arquivo: o mOS tem um usuário e nenhum outro processo trava os mesmos arquivos; aceitar é o útil */
int flock(int fd, int op) { (void)fd; (void)op; return 0; }
int lockf(int fd, int cmd, long len) { (void)fd; (void)cmd; (void)len; return 0; }
/* o kernel ainda não muda o tamanho de um arquivo aberto */
int ftruncate(int fd, long len) {
    mos_stat_t st;
    if (SYS2(SYS_FSTAT, fd, &st) < 0) { cx_errno = CX_EBADF; return -1; }
    if ((unsigned long long)len == st.size) return 0;
    cx_errno = CX_ENOTSUP;
    return -1;
}
/* fdopendir: libc.c (o caminho do fd vem do SYS_FDPATH) */
int fclonefileat(int src, int dirfd, const char *dst, unsigned flags) { (void)src; (void)dirfd; (void)dst; (void)flags; cx_errno = CX_ENOTSUP; return -1; }
int fsctl(const char *p, unsigned long req, void *data, unsigned opts) { (void)p; (void)req; (void)data; (void)opts; cx_errno = CX_ENOTSUP; return -1; }
int fsetattrlist(int fd, void *al, void *buf, size_t n, unsigned opts) { (void)fd; (void)al; (void)buf; (void)n; (void)opts; cx_errno = CX_ENOTSUP; return -1; }
/* copyfile: o estado existe; consultas não têm o que devolver */
void *copyfile_state_alloc(void) { return calloc(1, 64); }
int copyfile_state_free(void *s) { free(s); return 0; }
int copyfile_state_get(void *s, u32 flag, void *dst) { (void)s; (void)flag; (void)dst; cx_errno = CX_EINVAL; return -1; }
/* ACLs: o FAT não tem; "sem ACL" = ENOENT em acl_get_fd, como num arquivo sem ACL no macOS */
void *acl_get_fd(int fd) { (void)fd; cx_errno = CX_ENOENT; return 0; }
void *acl_init(int n) { (void)n; return calloc(1, 16); }
int acl_free(void *a) { free(a); return 0; }
int acl_set_fd(int fd, void *a) { (void)fd; (void)a; return 0; }
int acl_get_entry(void *a, int id, void **e) { (void)a; (void)id; if (e) *e = 0; cx_errno = CX_EINVAL; return -1; }
int acl_get_tag_type(void *e, int *t) { (void)e; if (t) *t = 0; cx_errno = CX_EINVAL; return -1; }
int acl_get_permset_mask_np(void *e, u64 *m) { (void)e; if (m) *m = 0; cx_errno = CX_EINVAL; return -1; }

/* ================================================================ diversos */
void srandomdev(void) { srandom(arc4random()); }
/* identidade da máquina: estável por instalação (o mOS não tem número de série) */
int gethostuuid(u8 *uuid, const void *timeout) {
    (void)timeout;
    static const u8 id[16] = {0x6d, 0x4f, 0x53, 0x00, 0x4d, 0x4f, 0x40, 0x53, 0x8a, 0x21, 0x37, 0x10, 0x51, 0x0c, 0x2e, 0x01};
    memcpy(uuid, id, 16);
    return 0;
}
int getpeereid(int fd, unsigned *uid, unsigned *gid) { (void)fd; if (uid) *uid = 501; if (gid) *gid = 20; return 0; }
unsigned if_nametoindex(const char *name) {
    if (!name) return 0;
    if (!strcmp(name, "lo0")) return 1;
    if (!strcmp(name, "en0")) return 4;
    return 0;
}
/* gerência de energia: aceita (o mOS não dorme sozinho com apps abertos) */
int IOPMAssertionCreateWithName(void *type, u32 level, void *name, u32 *id) { (void)type; (void)level; (void)name; if (id) *id = 1; return 0; }
int IOPMAssertionRelease(u32 id) { (void)id; return 0; }

/* ================================================================ CoreFoundation, CFNetwork, SystemConfiguration */
typedef void *id_;
typedef struct cx_class cx_class_t;
cx_class_t *cx_class_named(const char *name);
id_ objc_msgSend(id_ self, void *op, ...);
void *sel_registerName(const char *name);
id_ objc_retain(id_ o);
#define MSG0(T, o, s) ((T (*)(id_, void *))objc_msgSend)((id_)(o), sel_registerName(s))
#define MSG1(T, o, s, a) ((T (*)(id_, void *, __typeof__(a)))objc_msgSend)((id_)(o), sel_registerName(s), (a))
#define MSG2(T, o, s, a, b) ((T (*)(id_, void *, __typeof__(a), __typeof__(b)))objc_msgSend)((id_)(o), sel_registerName(s), (a), (b))
#define CLS(n) ((id_)cx_class_named(n))
id_ CFRetain(id_ o);
id_ CFDictionaryCreateMutable(void *alloc, long cap, const void *kcb, const void *vcb);
void CFDictionarySetValue(id_ d, id_ key, id_ val);
id_ CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(id_ a, id_ v);
id_ CFArrayCreateCopy(void *alloc, id_ src);
static id_ str1(const char *s) { id_ r = MSG1(id_, CLS("NSString"), "stringWithUTF8String:", s); return r ? objc_retain(r) : 0; }
static const char *utf8(id_ s) { return s ? MSG0(const char *, s, "UTF8String") : 0; }

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cfconst_t;
#define K(var, text) \
    static cfconst_t var##_cs = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_cs;
K(kCFProxyTypeKey, "kCFProxyTypeKey") K(kCFProxyTypeNone, "kCFProxyTypeNone") K(kCFProxyTypeHTTP, "kCFProxyTypeHTTP")
K(kCFProxyTypeHTTPS, "kCFProxyTypeHTTPS") K(kCFProxyTypeSOCKS, "kCFProxyTypeSOCKS") K(kCFProxyHostNameKey, "kCFProxyHostNameKey")
K(kCFProxyPortNumberKey, "kCFProxyPortNumberKey") K(kCFProxyAutoConfigurationURLKey, "kCFProxyAutoConfigurationURLKey")
K(kCFProxyAutoConfigurationJavaScriptKey, "kCFProxyAutoConfigurationJavaScriptKey")
K(kSCPropNetProxiesHTTPEnable, "HTTPEnable") K(kSCPropNetProxiesHTTPProxy, "HTTPProxy") K(kSCPropNetProxiesHTTPPort, "HTTPPort")
K(kSCPropNetProxiesHTTPSEnable, "HTTPSEnable") K(kSCPropNetProxiesHTTPSProxy, "HTTPSProxy") K(kSCPropNetProxiesHTTPSPort, "HTTPSPort")
K(kSCDynamicStoreUseSessionKeys, "UseSessionKeys")
K(NSPasteboardTypeHTML, "public.html") K(NSPasteboardTypeTIFF, "public.tiff")
K(NSPasteboardURLReadingFileURLsOnlyKey, "NSPasteboardURLReadingFileURLsOnlyKey")

/* proxies: o mOS não tem configuração de proxy; a resposta é a lista com um "sem proxy", como num Mac sem proxy */
id_ CFNetworkCopyProxiesForURL(id_ url, id_ settings) {
    (void)url; (void)settings;
    id_ d = CFDictionaryCreateMutable(0, 0, 0, 0);
    CFDictionarySetValue(d, (id_)kCFProxyTypeKey, (id_)kCFProxyTypeNone);
    id_ a = CFArrayCreateMutable(0, 0, 0);
    CFArrayAppendValue(a, d);
    return a;
}
id_ CFNetworkExecuteProxyAutoConfigurationScript(id_ script, id_ url, void *cb, void *ctx) { (void)script; (void)url; (void)cb; (void)ctx; return 0; }
id_ CFNetworkExecuteProxyAutoConfigurationURL(id_ pac, id_ url, void *cb, void *ctx) { (void)pac; (void)url; (void)cb; (void)ctx; return 0; }

/* CFError: um NSError do mOS (CFGetTypeID = TID_ERROR); domínio e código numa tabela ao lado, porque o NSError do
 * mOS não guarda campos. A tabela é circular (256 erros vivos bastam: erros CF são curtos). */
static struct { id_ e; id_ domain; long code; } errs[256];
static int nerrs;
static volatile int errs_lock;
id_ CFErrorCreate(void *alloc, id_ domain, long code, id_ info) {
    (void)alloc; (void)info;
    id_ e = MSG0(id_, CLS("NSError"), "alloc");
    if (e) e = MSG0(id_, e, "init");
    if (!e) return 0;
    while (__sync_lock_test_and_set(&errs_lock, 1)) ;
    int i = nerrs++ & 255;
    errs[i].e = e; errs[i].domain = domain ? CFRetain(domain) : 0; errs[i].code = code;
    __sync_lock_release(&errs_lock);
    return e;
}
static int err_find(id_ e, id_ *domain, long *code) {
    int r = 0;
    while (__sync_lock_test_and_set(&errs_lock, 1)) ;
    for (int i = 0; i < 256; i++)
        if (errs[i].e == e && e) { if (domain) *domain = errs[i].domain; if (code) *code = errs[i].code; r = 1; break; }
    __sync_lock_release(&errs_lock);
    return r;
}
long CFErrorGetCode(id_ e) { long c = 0; err_find(e, 0, &c); return c; }
id_ CFErrorGetDomain(id_ e) { id_ d = 0; err_find(e, &d, 0); return d; }

/* CFURL: as que nsurl.c (sessão do VS Code) ainda não tem */
id_ CFURLCreateWithString(void *alloc, id_ s, id_ base) {
    (void)alloc; (void)base;
    if (!s) return 0;
    id_ u = MSG1(id_, CLS("NSURL"), "URLWithString:", s);
    return u ? objc_retain(u) : 0;
}
id_ CFURLCopyAbsoluteURL(id_ u) { return u ? CFRetain(u) : 0; }
id_ CFURLCreateFilePathURL(void *alloc, id_ u, id_ *err) { (void)alloc; if (err) *err = 0; return u ? CFRetain(u) : 0; }
id_ CFURLCreateFileReferenceURL(void *alloc, id_ u, id_ *err) { (void)alloc; if (err) *err = 0; return u ? CFRetain(u) : 0; }
id_ CFURLCreateCopyAppendingPathComponent(void *alloc, id_ u, id_ comp, unsigned char dir) {
    (void)alloc;
    if (!u || !comp) return 0;
    id_ r = MSG2(id_, u, "URLByAppendingPathComponent:isDirectory:", comp, (signed char)dir);
    return r ? objc_retain(r) : 0;
}
id_ CFURLCreateCopyDeletingLastPathComponent(void *alloc, id_ u) {
    (void)alloc;
    if (!u) return 0;
    id_ r = MSG0(id_, u, "URLByDeletingLastPathComponent");
    return r ? objc_retain(r) : 0;
}
unsigned char CFURLResourceIsReachable(id_ u, id_ *err) {
    if (err) *err = 0;
    const char *p = u ? utf8(MSG0(id_, u, "path")) : 0;
    return p && access(p, 0) == 0;
}

/* idiomas, fuso, preferências */
id_ CFLocaleCopyPreferredLanguages(void) {
    id_ a = MSG0(id_, CLS("NSLocale"), "preferredLanguages");
    return a ? CFArrayCreateCopy(0, a) : CFArrayCreateMutable(0, 0, 0);
}
/* CFTimeZone = o nome IANA (CFString); TZ do ambiente, senão o do Brasil (padrão do mOS) */
id_ CFTimeZoneCopySystem(void) {
    const char *tz = getenv("TZ");
    if (tz && *tz == ':') tz++;
    return str1(tz && *tz ? tz : "America/Sao_Paulo");
}
id_ CFTimeZoneGetName(id_ tz) { return tz; }
void CFTimeZoneResetSystem(void) {}
id_ CFPreferencesCopyAppValue(id_ key, id_ app) { (void)key; (void)app; return 0; }
unsigned char CFPreferencesAppSynchronize(id_ app) { (void)app; return 1; }

/* ================================================================ FSEvents: o fluxo existe e pode ser iniciado, mas
 * nunca entrega eventos (o mOS ainda não notifica mudanças no disco). Quem precisa de eventos verifica de novo
 * por conta própria ou só perde o recarregamento automático. */
typedef struct { u32 magic; int started; } fsstream_t;
void *FSEventStreamCreate(void *alloc, void *cb, void *ctx, id_ paths, u64 since, double latency, u32 flags) {
    (void)alloc; (void)cb; (void)ctx; (void)paths; (void)since; (void)latency; (void)flags;
    fsstream_t *s = calloc(1, sizeof *s);
    if (s) s->magic = 0x46534576;
    return s;
}
unsigned char FSEventStreamStart(void *s) { if (!s) return 0; ((fsstream_t *)s)->started = 1; return 1; }
void FSEventStreamStop(void *s) { if (s) ((fsstream_t *)s)->started = 0; }
void FSEventStreamInvalidate(void *s) { (void)s; }
void FSEventStreamRelease(void *s) { free(s); }
void FSEventStreamScheduleWithRunLoop(void *s, void *rl, id_ mode) { (void)s; (void)rl; (void)mode; }
void FSEventStreamUnscheduleFromRunLoop(void *s, void *rl, id_ mode) { (void)s; (void)rl; (void)mode; }
void FSEventStreamSetDispatchQueue(void *s, void *q) { (void)s; (void)q; }
void FSEventStreamFlushSync(void *s) { (void)s; }
void FSEventStreamFlushAsync(void *s) { (void)s; }
int FSEventStreamGetDeviceBeingWatched(void *s) { (void)s; return 0; }
u64 FSEventStreamGetLatestEventId(void *s) { (void)s; return 0; }
u64 FSEventsGetCurrentEventId(void) { return 0; }
unsigned char FSEventsPurgeEventsForDeviceUpToEventId(int dev, u64 id) { (void)dev; (void)id; return 1; }
