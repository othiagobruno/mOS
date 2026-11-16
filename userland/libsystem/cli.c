/*
 * mOS — libSystem: o que o Codex CLI (binário Rust, codex-x86_64-apple-darwin) importa e o mOS não tinha.
 * Medido com tools/machocheck.c (docs/compat-apps.md): 87 funções e constantes; vários servem também aos Electron
 * (FSEventStream* do policy-watcher do VS Code, IOPMAssertion*, CFError*, proxies do Chromium).
 *
 * Regra (a mesma de chrome.c/security.c): quando o mOS não tem o recurso, devolve o erro que a API documenta,
 * nunca sucesso sem preencher a saída. O que funciona de verdade: termios guardado por processo (tcgetattr/
 * tcsetattr/cfmakeraw), proc_pidpath/proc_listchildpids pela lista de processos do kernel, waitid sobre waitpid,
 * killpg, strsignal, umask, CFError, CFURL (as que faltavam em nsurl.c), CFTimeZone, CFPreferences (prefs.c),
 * proxies = "sem proxy". FSEvents (criar/editar/apagar) está em fsevents.c.
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
int open(const char *path, int flags, ...);
ssize_t read(int fd, void *b, size_t n);
ssize_t write(int fd, const void *b, size_t n);
int close(int fd);
int stat(const char *p, void *st);
int lstat(const char *p, void *st);
int fstat(int fd, void *st);
int kill(int pid, int sig);
int statfs(const char *path, void *buf);
int futimes(int fd, const void *tv);
char *strdup(const char *s);
const char *cx_at_path(int dirfd, const char *p, char *buf, size_t cap);
void cx_perm_set(const char *path, unsigned mode);

/* termios, pseudoterminais, getpgid e killpg: em pty.c (sobre o SYS_TTY do kernel) */

/* ================================================================ processos */
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
/* libproc (proc_pidinfo, proc_listpids, proc_pidpath, proc_name, proc_pid_rusage...): procinfo.c */
int proc_listpids(u32 type, u32 info, void *buf, int bytes);
int proc_pidpath(int pid, void *buf, u32 size);
unsigned getuid(void);
/* proc_listpidspath: os processos (do filtro de proc_listpids) que usam o caminho — arquivo aberto, pasta atual ou
 * o executável. Com PROC_LISTPIDSPATH_PATH_IS_VOLUME (1) vale qualquer caminho dentro daquele volume (prefixo).
 * O mOS só enxerga os descritores do próprio processo; dos outros conta o executável. Volta em bytes, como o libproc. */
char *realpath(const char *path, char *out);
int strncmp(const char *a, const char *b, size_t n);
char *getcwd(char *b, size_t n);
static int path_hits(const char *want, const char *have, int volume) {
    size_t n = strlen(want);
    if (!volume) return !strcmp(want, have);
    return !strncmp(want, have, n) && (have[n] == 0 || have[n] == '/' || (n && want[n - 1] == '/'));
}
int proc_listpidspath(u32 type, u32 info, const char *path, u32 pathflags, void *buf, int bytes) {
    char want[1024], have[1024], canon[1024];
    if (!path || !realpath(path, want)) return -1;
    int volume = (pathflags & 1) != 0;
    static int all[2048];
    int k = proc_listpids(type, info, all, sizeof all) / (int)sizeof(int);
    int n = 0, cap = buf ? bytes / (int)sizeof(int) : 0, *out = buf, me = getpid();
    for (int i = 0; i < k; i++) {
        int hit = 0;
        if (all[i] == me) {
            for (int fd = 0; fd < 1024 && !hit; fd++)
                if (SYS3(SYS_FDPATH, fd, have, sizeof have) >= 0 &&
                    path_hits(want, realpath(have, canon) ? canon : have, volume)) hit = 1;
            if (!hit && getcwd(have, sizeof have) && path_hits(want, realpath(have, canon) ? canon : have, volume)) hit = 1;
        }
        if (!hit && proc_pidpath(all[i], have, sizeof have) > 0 && path_hits(want, realpath(have, canon) ? canon : have, volume)) hit = 1;
        if (!hit) continue;
        if (n < cap) out[n] = all[i];
        n++;
    }
    if (!buf) return n * (int)sizeof(int);
    return (n < cap ? n : cap) * (int)sizeof(int);
}
/* ================================================================ arquivos */
static unsigned cur_umask = 022;
unsigned umask(unsigned m) { unsigned o = cur_umask; cur_umask = m & 0777; return o; }
/* travas de arquivo: o mOS tem um usuário e nenhum outro processo trava os mesmos arquivos; aceitar é o útil */
int flock(int fd, int op) { (void)fd; (void)op; return 0; }
int lockf(int fd, int cmd, long len) { (void)fd; (void)cmd; (void)len; return 0; }
/* SYS_FTRUNCATE (o SQLite trunca o journal a cada commit; antes isto dava ENOTSUP e o perfil do Chrome não abria) */
int ftruncate(int fd, long len) {
    if (len < 0) { cx_errno = CX_EINVAL; return -1; }
    long r = SYS2(SYS_FTRUNCATE, fd, len);
    if (r < 0) { cx_errno = (int)-r; return -1; }
    return 0;
}
/* fdopendir: libc.c (o caminho do fd vem do SYS_FDPATH) */
/* clonefile, clonefileat, fclonefileat: attrlist.c (cópia com metadados no kernel) */
/* fsctl: o caminho é conferido. Opcode de clone/snapshot não existe neste FAT; ENOTSUP é o que o
 * macOS devolve para um opcode que o volume não implementa. */
int fsctl(const char *p, unsigned long req, void *data, unsigned opts) {
    (void)req; (void)data; (void)opts;
    char st[144];
    if (!p) { cx_errno = CX_EFAULT; return -1; }
    if (stat(p, st) < 0) return -1;
    cx_errno = CX_ENOTSUP;
    return -1;
}
/* setattrlist, fsetattrlist, setattrlistat: attrlist.c */

#define CP_MAGIC 0x43504653u
typedef struct {
    u32 magic;
    int src_fd, dst_fd;
    char *src, *dst;
    void *quar, *status_cb, *status_ctx;
    long long copied;
    int cloned;
    u32 bsize, src_bsize, dst_bsize;
} cpstate_t;
void *copyfile_state_alloc(void) {
    cpstate_t *s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->magic = CP_MAGIC;
    s->src_fd = s->dst_fd = -1;
    return s;
}
int copyfile_state_free(void *p) {
    cpstate_t *s = p;
    if (!s || s->magic != CP_MAGIC) { cx_errno = CX_EINVAL; return -1; }
    free(s->src); free(s->dst);
    s->magic = 0;
    free(s);
    return 0;
}
static int cp_ok(void *p, cpstate_t **o) {
    cpstate_t *s = p;
    if (!s || s->magic != CP_MAGIC) { cx_errno = CX_EINVAL; return -1; }
    *o = s;
    return 0;
}
int copyfile_state_get(void *p, u32 flag, void *dst) {
    cpstate_t *s;
    if (cp_ok(p, &s) < 0 || !dst) { cx_errno = CX_EINVAL; return -1; }
    switch (flag) {
    case 1: *(int *)dst = s->src_fd; break;
    case 2: *(char **)dst = s->src; break;
    case 3: *(int *)dst = s->dst_fd; break;
    case 4: *(char **)dst = s->dst; break;
    case 5: *(void **)dst = s->quar; break;
    case 6: *(void **)dst = s->status_cb; break;
    case 7: *(void **)dst = s->status_ctx; break;
    case 8: *(long long *)dst = s->copied; break;
    case 9: *(char **)dst = 0; break;          /* xattr em cópia: o FAT não tem */
    case 10: *(int *)dst = s->cloned; break;
    case 11: *(u32 *)dst = s->src_bsize; break;
    case 12: *(u32 *)dst = s->dst_bsize; break;
    case 13: *(u32 *)dst = s->bsize; break;
    default: cx_errno = CX_EINVAL; return -1;
    }
    return 0;
}
int copyfile_state_set(void *p, u32 flag, const void *src) {
    cpstate_t *s;
    if (cp_ok(p, &s) < 0) return -1;
    switch (flag) {
    case 1: if (!src) { cx_errno = CX_EINVAL; return -1; } s->src_fd = *(const int *)src; break;
    case 2: free(s->src); s->src = src ? strdup((const char *)src) : 0; break;
    case 3: if (!src) { cx_errno = CX_EINVAL; return -1; } s->dst_fd = *(const int *)src; break;
    case 4: free(s->dst); s->dst = src ? strdup((const char *)src) : 0; break;
    case 5: s->quar = (void *)src; break;
    case 6: s->status_cb = (void *)src; break;
    case 7: s->status_ctx = (void *)src; break;
    case 8: if (!src) { cx_errno = CX_EINVAL; return -1; } s->copied = *(const long long *)src; break;
    case 10: if (!src) { cx_errno = CX_EINVAL; return -1; } s->cloned = *(const int *)src; break;
    case 11: if (!src) { cx_errno = CX_EINVAL; return -1; } s->src_bsize = *(const u32 *)src; break;
    case 12: if (!src) { cx_errno = CX_EINVAL; return -1; } s->dst_bsize = *(const u32 *)src; break;
    case 13: if (!src) { cx_errno = CX_EINVAL; return -1; } s->bsize = *(const u32 *)src; break;
    default: cx_errno = CX_EINVAL; return -1;
    }
    return 0;
}
void cx_copyfile_note(void *p, int src_fd, const char *src, int dst_fd, const char *dst, long copied) {
    cpstate_t *s = p;
    if (!s || s->magic != CP_MAGIC) return;
    if (src_fd >= 0) s->src_fd = src_fd;
    if (dst_fd >= 0) s->dst_fd = dst_fd;
    if (src) { free(s->src); s->src = strdup(src); }
    if (dst) { free(s->dst); s->dst = strdup(dst); }
    if (copied >= 0) { s->copied = copied; s->cloned = 0; }
}

/* getfsstat, getmntinfo, statfs, fstatfs: attrlist.c (um por volume) */

/* ACL: acl.c (kauth_filesec no kernel, SYS_FSMETA) */

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
static const char *utf8(id_ s) { return s ? MSG0(const char *, s, "UTF8String") : 0; }

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cfconst_t;
#define K(var, text) \
    static cfconst_t var##_cs = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_cs;

K(kSCPropNetProxiesHTTPEnable, "HTTPEnable") K(kSCPropNetProxiesHTTPProxy, "HTTPProxy") K(kSCPropNetProxiesHTTPPort, "HTTPPort")
K(kSCPropNetProxiesHTTPSEnable, "HTTPSEnable") K(kSCPropNetProxiesHTTPSProxy, "HTTPSProxy") K(kSCPropNetProxiesHTTPSPort, "HTTPSPort")

K(kSCDynamicStoreUseSessionKeys, "UseSessionKeys")
K(NSPasteboardTypeHTML, "public.html") K(NSPasteboardTypeTIFF, "public.tiff")
K(NSPasteboardURLReadingFileURLsOnlyKey, "NSPasteboardURLReadingFileURLsOnlyKey")

/* proxies: CFNetworkCopyProxiesForURL e as chaves kCFProxy* estão em cfproxy.c */
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
/* CFURLCreateWithBytes: os bytes na codificação dada viram a string da URL, relativa à base se houver (o codex da
 * OpenAI, binário Rust, monta as URLs assim) */
id_ CFStringCreateWithBytes(void *alloc, const u8 *b, long n, u32 enc, u8 ext);
void CFRelease(id_ o);
id_ CFURLCreateWithBytes(void *alloc, const u8 *bytes, long len, u32 enc, id_ base) {
    (void)alloc;
    if (!bytes || len < 0) return 0;
    id_ s = CFStringCreateWithBytes(0, bytes, len, enc, 0);
    if (!s) return 0;
    id_ u = base ? MSG2(id_, CLS("NSURL"), "URLWithString:relativeToURL:", s, base) : MSG1(id_, CLS("NSURL"), "URLWithString:", s);
    CFRelease(s);
    return u ? objc_retain(u) : 0;
}
id_ CFURLCopyAbsoluteURL(id_ u) { return u ? CFRetain(u) : 0; }
/* a mesma URL resolvida contra a base (absoluteURL); o modo de compatibilidade (RFC 1808) não muda nada aqui */
id_ CFURLCreateAbsoluteURLWithBytes(void *alloc, const u8 *bytes, long len, u32 enc, id_ base, u8 compat) {
    (void)compat;
    id_ u = CFURLCreateWithBytes(alloc, bytes, len, enc, base);
    if (!u) return 0;
    id_ a = MSG0(id_, u, "absoluteURL");
    if (a) a = objc_retain(a);
    CFRelease(u);
    return a;
}
/* CFURLCreateFilePathURL, CFURLCreateFileReferenceURL: bookmark.c (/.file/id=) */
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
/* CFPreferencesCopyAppValue / CFPreferencesAppSynchronize: prefs.c */

/* FSEvents: userland/libsystem/fsevents.c (varredura stat/readdir; o kernel não tem fila de vnode). */
