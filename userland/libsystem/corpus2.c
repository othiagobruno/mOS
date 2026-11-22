/*
 * mOS libSystem — lacunas pedidas por 2 apps do corpus (docs/lacunas-rodada-2026-11-22.md):
 *   - libSystem: link_ntoa, lfind, sys_siglist, swap_* do Mach-O, statx_np/fstatx_np, filesec_*, clock_get_attributes,
 *     malloc_printf, DNSServiceConstructFullName, chave AES embrulhada (RFC 3394: CCSymmetricKeyWrap/Unwrap) e o resto
 *     pequeno (unmount/acct sem suporte, __truncdfhf2, __floatuntixf, _Unwind_Resume_or_Rethrow, xpc_shmem_*);
 *   - CoreFoundation: CFTree, kCFAllocatorUseContext; Foundation: NSZoneName, chaves do NSMetadata;
 *   - CoreGraphics: regiões do CGS, modos de tela do CGS, CGContextGetType; IOKit: I2C, HID e kexts sem nada a mostrar;
 *   - libpcap: pcap_open_dead e os arquivos .pcap (pcap_dump_*), nomes de DLT e de carimbo de tempo, filtro vazio;
 *   - CoreServices/CoreVideo/AudioToolbox/QuartzCore/VideoToolbox/SystemConfiguration/CoreText/ImageIO/CFNetwork:
 *     constantes e funções pequenas; libsasl2 e libpam sem mecanismo; Metal, AppKit, PrintCore, CGPSConverter.
 * Entra no libSystem.mos pelo glob de tools/build.sh. Teste: tools/corpus2_hosttest.sh e o corpusteste no mOS.
 */
#include "foundation.h"
#include "libc.h"


id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
int vsnprintf(char *b, size_t n, const char *f, va_list ap);
long write(int fd, const void *b, size_t n);
typedef i32 OSStatus;

extern cx_class_t NSConstantString_c __asm__("__CFConstantStringClassReference");
#define KS(sym, val) static nsstr_t sym##_o = { &NSConstantString_c, 0, (char *)val, sizeof(val) - 1 }; id sym = (id)&sym##_o;

/* ================================================================ libSystem */
const char *const sys_siglist[32] = {
    "Signal 0", "Hangup", "Interrupt", "Quit", "Illegal instruction", "Trace/BPT trap", "Abort trap", "EMT trap",
    "Floating point exception", "Killed", "Bus error", "Segmentation fault", "Bad system call", "Broken pipe",
    "Alarm clock", "Terminated", "Urgent I/O condition", "Suspended (signal)", "Suspended", "Continued",
    "Child exited", "Stopped (tty input)", "Stopped (tty output)", "I/O possible", "Cputime limit exceeded",
    "Filesize limit exceeded", "Virtual timer expired", "Profiling timer expired", "Window size changes",
    "Information request", "User defined signal 1", "User defined signal 2",
};

u32 htonl(u32 x) { return __builtin_bswap32(x); }

/* sockaddr_dl: "en0:0.1c.42.a.b.c" (o nome, ':' e os bytes do endereço em hexa sem zero à esquerda, como no BSD) */
typedef struct { u8 len, family; u16 index; u8 type, nlen, alen, slen; char data[12]; } sdl_t;
char *link_ntoa(const sdl_t *s) {
    static char buf[64];
    static const char hx[] = "0123456789abcdef";
    char *o = buf, *end = buf + sizeof buf - 4;
    if (!s) { buf[0] = 0; return buf; }
    const u8 *d = (const u8 *)s->data;
    for (int i = 0; i < s->nlen && o < end; i++) *o++ = (char)d[i];
    if (s->alen) {
        if (s->nlen) *o++ = ':';
        for (int i = 0; i < s->alen && o < end; i++) {
            u8 b = d[s->nlen + i];
            if (i) *o++ = '.';
            if (b >> 4) *o++ = hx[b >> 4];
            *o++ = hx[b & 15];
        }
    }
    *o = 0;
    return buf;
}

void *lfind(const void *key, const void *base, size_t *nmemb, size_t size, int (*cmp)(const void *, const void *)) {
    if (!nmemb || !cmp) return 0;
    const u8 *p = (const u8 *)base;
    for (size_t i = 0; i < *nmemb; i++, p += size) if (!cmp(key, p)) return (void *)p;
    return 0;
}

typedef struct { int dummy; } locale_dummy;
size_t wcstombs(char *dst, const int *src, size_t len);
size_t wcstombs_l(char *dst, const int *src, size_t len, void *loc) { (void)loc; return wcstombs(dst, src, len); }

int unmount(const char *dir, int flags) { (void)dir; (void)flags; cx_errno = 1; return -1; }        /* EPERM */
int acct(const char *path) { (void)path; cx_errno = 78; return -1; }                               /* ENOSYS */
void arc4random_addrandom(u8 *dat, int n) { (void)dat; (void)n; }
int pthread_self_is_exiting_np(void) { return 0; }
u8 os_trace_debug_enabled(void) { return 0; }
void _os_trace_with_buffer(void *dso, const char *msg, u8 type, const void *buf, size_t len, void *pc) {
    (void)dso; (void)msg; (void)type; (void)buf; (void)len; (void)pc;
}
const char *dyld_shared_cache_file_path(void) { return 0; }       /* o mOS não tem shared cache */
u8 _dyld_get_shared_cache_uuid(u8 *uuid) { if (uuid) memset(uuid, 0, 16); return 0; }
typedef struct { const char *fname; void *fbase; const char *sname; void *saddr; } c2_dlinfo_t;
int dladdr(const void *a, void *i);
const char *dyld_image_path_containing_address(const void *addr) { c2_dlinfo_t i; return dladdr(addr, &i) ? i.fname : 0; }
const int SANDBOX_CHECK_NO_REPORT = 0x40000000;

void malloc_printf(const char *fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof b - 1) n = (int)sizeof b - 1;
    if (n > 0) write(2, b, (size_t)n);
}

/* clock_get_attributes(clock_serv_t, flavor, clock_attr_t, *count): resolução em ns */
int clock_get_attributes(u32 clock, int flavor, int *attr, u32 *count) {
    (void)clock;
    if (!attr || !count || *count < 1) return 4;                         /* KERN_INVALID_ARGUMENT */
    switch (flavor) {
    case 1: case 3: case 4: case 5: attr[0] = 1000; break;               /* GET_TIME_RES, ALARM_CURRES/MINRES/MAXRES */
    default: return 4;
    }
    *count = 1;
    return 0;
}

/* ---- Mach-O: troca de ordem dos bytes dos cabeçalhos (mach-o/swap.h). Todos os campos citados são de 32 bits. */
static void swap32s(u32 *p, int n) { for (int i = 0; i < n; i++) p[i] = __builtin_bswap32(p[i]); }
void swap_mach_header(void *mh, int order) { (void)order; if (mh) swap32s((u32 *)mh, 7); }
void swap_load_command(void *lc, int order) { (void)order; if (lc) swap32s((u32 *)lc, 2); }
void swap_segment_command(void *sg, int order) {
    (void)order;
    if (!sg) return;
    u32 *p = (u32 *)sg;
    swap32s(p, 2);                      /* cmd, cmdsize */
    swap32s(p + 6, 8);                  /* (segname[16]) vmaddr, vmsize, fileoff, filesize, maxprot, initprot, nsects, flags */
}

/* ---- filesec_t: dono, grupo, modo e ACL de um arquivo (sys/fcntl.h, usado com statx_np/openx_np) */
typedef struct { int have; u32 uid, gid; u16 mode; u8 owner_uuid[16], grp_uuid[16]; void *acl; } filesec_s;
enum { FSP_OWNER = 1, FSP_GROUP = 2, FSP_UUID = 3, FSP_MODE = 4, FSP_ACL = 5, FSP_GRPUUID = 6 };
void *filesec_init(void) { return calloc(1, sizeof(filesec_s)); }
void filesec_free(void *f) { free(f); }
void *filesec_dup(void *f) { filesec_s *n = filesec_init(); if (n && f) memcpy(n, f, sizeof *n); return n; }
int filesec_get_property(void *f, int prop, void *out) {
    filesec_s *s = (filesec_s *)f;
    if (!s || !out) { cx_errno = 22; return -1; }
    if (!(s->have & (1 << prop))) { cx_errno = 2; return -1; }           /* ENOENT: propriedade não definida */
    switch (prop) {
    case FSP_OWNER: *(u32 *)out = s->uid; return 0;
    case FSP_GROUP: *(u32 *)out = s->gid; return 0;
    case FSP_MODE: *(u16 *)out = s->mode; return 0;
    case FSP_UUID: memcpy(out, s->owner_uuid, 16); return 0;
    case FSP_GRPUUID: memcpy(out, s->grp_uuid, 16); return 0;
    case FSP_ACL: *(void **)out = s->acl; return 0;
    }
    cx_errno = 22;
    return -1;
}
int filesec_set_property(void *f, int prop, const void *in) {
    filesec_s *s = (filesec_s *)f;
    if (!s || prop < 1 || prop > 6) { cx_errno = 22; return -1; }
    if (!in) { s->have &= ~(1 << prop); return 0; }
    switch (prop) {
    case FSP_OWNER: s->uid = *(const u32 *)in; break;
    case FSP_GROUP: s->gid = *(const u32 *)in; break;
    case FSP_MODE: s->mode = *(const u16 *)in; break;
    case FSP_UUID: memcpy(s->owner_uuid, in, 16); break;
    case FSP_GRPUUID: memcpy(s->grp_uuid, in, 16); break;
    case FSP_ACL: s->acl = *(void *const *)in; break;
    }
    s->have |= 1 << prop;
    return 0;
}
/* statx_np/fstatx_np: o stat de sempre; o filesec recebe dono, grupo e modo (a ACL fica no acl_get_file) */
typedef struct { i32 dev; u16 mode, nlink; u64 ino; u32 uid, gid; } stat_head_t;     /* começo da struct stat (64 bits) */
int stat(const char *p, void *st);
int fstat(int fd, void *st);
static void fsec_fill(void *f, const void *st) {
    if (!f) return;
    const stat_head_t *h = (const stat_head_t *)st;
    u16 m = h->mode & 07777;
    filesec_set_property(f, FSP_OWNER, &h->uid);
    filesec_set_property(f, FSP_GROUP, &h->gid);
    filesec_set_property(f, FSP_MODE, &m);
}
/* (o $INODE64 do nome no Mach-O cai na resolução por nome) */
int statx_np(const char *p, void *st, void *fsec) { int r = stat(p, st); if (!r) fsec_fill(fsec, st); return r; }
int fstatx_np(int fd, void *st, void *fsec) { int r = fstat(fd, st); if (!r) fsec_fill(fsec, st); return r; }
int fstatfs(int fd, void *s);
int fstatfs64(int fd, void *s) { return fstatfs(fd, s); }

/* ---- xpc_shmem: o mOS entrega a XPC dentro do processo; a região vira uma cópia mapeada no xpc_shmem_map */
typedef void *xpc_object_t;
xpc_object_t xpc_data_create(const void *p, size_t n);
const void *xpc_data_get_bytes_ptr(xpc_object_t o);
size_t xpc_data_get_length(xpc_object_t o);
void *mmap(void *a, size_t n, int prot, int flags, int fd, long off);
xpc_object_t xpc_shmem_create(void *region, size_t len) { return region && len ? xpc_data_create(region, len) : 0; }
size_t xpc_shmem_map(xpc_object_t o, void **region) {
    if (!o || !region) return 0;
    size_t n = xpc_data_get_length(o);
    const void *src = xpc_data_get_bytes_ptr(o);
    if (!n || !src) return 0;
    void *m = mmap(0, n, 3, 0x1002, -1, 0);                             /* PROT_READ|WRITE, MAP_ANON|MAP_PRIVATE */
    if (m == (void *)-1) return 0;
    memcpy(m, src, n);
    *region = m;
    return n;
}

/* ---- compiler-rt: double -> half (só no x86_64; no arm64 o compilador usa a instrução) e u128 -> long double (x87) */
#if defined(__x86_64__)
static u16 d2h(double d) {
    u64 b; memcpy(&b, &d, 8);
    u16 sign = (u16)((b >> 48) & 0x8000);
    int e = (int)((b >> 52) & 0x7ff);
    u64 m = b & 0xFFFFFFFFFFFFFull;
    if (e == 0x7ff) return sign | 0x7c00 | (m ? 0x200 : 0);              /* inf / NaN quieto */
    int he = e - 1023 + 15;
    if (he >= 31) return sign | 0x7c00;
    if (he <= 0) {                                                       /* subnormal ou zero */
        if (he < -10) return sign;
        m |= 1ull << 52;
        int sh = 1 - he + 42;                                            /* 52 - 10 + (1 - he) */
        u64 q = m >> sh, rem = m & ((1ull << sh) - 1), half = 1ull << (sh - 1);
        if (rem > half || (rem == half && (q & 1))) q++;
        return sign | (u16)q;
    }
    u64 q = m >> 42, rem = m & ((1ull << 42) - 1), half = 1ull << 41;
    u32 r = ((u32)he << 10) | (u32)q;
    if (rem > half || (rem == half && (q & 1))) r++;                     /* o carry pode subir para o expoente (ou inf) */
    return sign | (u16)r;
}
_Float16 __truncdfhf2(double d) { u16 h = d2h(d); _Float16 r; memcpy(&r, &h, 2); return r; }
long double __floatuntixf(unsigned __int128 a) {
    u64 hi = (u64)(a >> 64), lo = (u64)a;
    return (long double)hi * 18446744073709551616.0L + (long double)lo;
}
#endif

#if defined(__x86_64__)        /* no arm64 vem do libunwind (UnwindLevel1-gcc-ext.c) */
int _Unwind_RaiseException(void *e);
int _Unwind_Resume_or_Rethrow(void *e) { return _Unwind_RaiseException(e); }
#endif

/* ---- DNS-SD */
#define kDNSServiceErr_BadParam (-65540)
int DNSServiceReconfirmRecord(u32 flags, u32 iface, const char *full, u16 t, u16 c, u16 len, const void *rd) {
    (void)flags; (void)iface; (void)full; (void)t; (void)c; (void)len; (void)rd;
    return -65563;                                                       /* kDNSServiceErr_ServiceNotRunning */
}
/* "Minha Impressora._ipp._tcp.local." com '.' e '\' do nome escapados e bytes de controle como \DDD */
int DNSServiceConstructFullName(char *full, const char *service, const char *regtype, const char *domain) {
    if (!full || !regtype || !domain) return kDNSServiceErr_BadParam;
    char *o = full, *end = full + 1009;                                  /* kDNSServiceMaxDomainName = 1009 */
    if (service && *service) {
        for (const u8 *s = (const u8 *)service; *s; s++) {
            if (o + 5 >= end) return kDNSServiceErr_BadParam;
            if (*s == '.' || *s == '\\') { *o++ = '\\'; *o++ = (char)*s; }
            else if (*s <= ' ' || *s == 0x7f) { *o++ = '\\'; *o++ = (char)('0' + *s / 100); *o++ = (char)('0' + *s / 10 % 10); *o++ = (char)('0' + *s % 10); }
            else *o++ = (char)*s;
        }
        *o++ = '.';
    }
    const char *parts[2] = {regtype, domain};
    for (int k = 0; k < 2; k++) {
        size_t n = strlen(parts[k]);
        if (!n) continue;
        if (o + n + 2 >= end) return kDNSServiceErr_BadParam;
        memcpy(o, parts[k], n); o += n;
        if (o[-1] != '.') *o++ = '.';
    }
    *o = 0;
    return 0;
}

/* ---- CommonCrypto: RFC 3394 (AES key wrap) sobre o CCCrypt em ECB */
int CCCrypt(u32 op, u32 alg, u32 options, const void *key, size_t klen, const void *iv, const void *in, size_t inlen,
            void *out, size_t outcap, size_t *moved);
const u8 CCrfc3394_iv[8] = {0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6};
const size_t CCrfc3394_ivLen = 8;
#define kCCParamError (-4300)
#define kCCDecodeError (-4304)
static int aes_blk(int dec, const u8 *kek, size_t kl, const u8 *in, u8 *out) {
    size_t mv = 0;
    return CCCrypt(dec ? 1 : 0, 0, 2, kek, kl, 0, in, 16, out, 16, &mv) == 0 && mv == 16 ? 0 : -1;
}
size_t CCSymmetricWrappedSize(u32 alg, size_t raw) { (void)alg; return raw + 8; }
size_t CCSymmetricUnwrappedSize(u32 alg, size_t wrapped) { (void)alg; return wrapped >= 8 ? wrapped - 8 : 0; }
int CCSymmetricKeyWrap(u32 alg, const u8 *iv, size_t ivlen, const u8 *kek, size_t kl, const u8 *raw, size_t rl, u8 *out, size_t *ol) {
    if (alg != 1 || !iv || ivlen != 8 || !kek || !raw || !out || !ol || rl < 16 || rl % 8 || *ol < rl + 8) return kCCParamError;
    size_t n = rl / 8;
    u8 a[8], b[16];
    memcpy(a, iv, 8);
    memmove(out + 8, raw, rl);
    for (int j = 0; j <= 5; j++)
        for (size_t i = 1; i <= n; i++) {
            memcpy(b, a, 8); memcpy(b + 8, out + 8 * i, 8);
            if (aes_blk(0, kek, kl, b, b)) return kCCParamError;
            u64 t = (u64)n * (u64)j + i;
            memcpy(a, b, 8);
            for (int k = 0; k < 8; k++) a[7 - k] ^= (u8)(t >> (8 * k));
            memcpy(out + 8 * i, b + 8, 8);
        }
    memcpy(out, a, 8);
    *ol = rl + 8;
    return 0;
}
int CCSymmetricKeyUnwrap(u32 alg, const u8 *iv, size_t ivlen, const u8 *kek, size_t kl, const u8 *wr, size_t wl, u8 *out, size_t *ol) {
    if (alg != 1 || !iv || ivlen != 8 || !kek || !wr || !out || !ol || wl < 24 || wl % 8 || *ol < wl - 8) return kCCParamError;
    size_t n = wl / 8 - 1;
    u8 a[8], b[16];
    memcpy(a, wr, 8);
    memmove(out, wr + 8, wl - 8);
    for (int j = 5; j >= 0; j--)
        for (size_t i = n; i >= 1; i--) {
            u64 t = (u64)n * (u64)j + i;
            for (int k = 0; k < 8; k++) a[7 - k] ^= (u8)(t >> (8 * k));
            memcpy(b, a, 8); memcpy(b + 8, out + 8 * (i - 1), 8);
            if (aes_blk(1, kek, kl, b, b)) return kCCParamError;
            memcpy(a, b, 8);
            memcpy(out + 8 * (i - 1), b + 8, 8);
        }
    if (memcmp(a, iv, 8)) { memset(out, 0, wl - 8); return kCCDecodeError; }
    *ol = wl - 8;
    return 0;
}

/* ================================================================ CoreFoundation */
const void *const kCFAllocatorUseContext = (const void *)0x03ab;

/* CFTree: o pai retém os filhos; o contexto (info + retain/release) é do chamador, como no CF */
typedef struct { long version; void *info; const void *(*retain)(const void *); void (*release)(const void *); id (*desc)(const void *); } tree_ctx_t;
typedef struct c2tree { cx_class_t *isa; struct c2tree *parent, *first, *last, *next; long nchild; tree_ctx_t ctx; } c2tree_t;
cx_class_t CFTreeMOS_c __asm__("OBJC_CLASS_$_CFTreeMOS"), CFTreeMOS_m __asm__("OBJC_METACLASS_$_CFTreeMOS");
static int tree_ready;
#define CF_TREE_TYPEID 0x2B1
void CFTreeRemoveAllChildren(c2tree_t *t);
id object_dispose(id o);
static u64 tree_tid(id self, SEL s) { (void)self; (void)s; return CF_TREE_TYPEID; }
static void tree_dealloc(id self, SEL s) {
    (void)s;
    c2tree_t *t = (c2tree_t *)self;
    CFTreeRemoveAllChildren(t);
    if (t->ctx.release && t->ctx.info) t->ctx.release(t->ctx.info);
    object_dispose(self);                            /* memória do runtime (cx_alloc_instance), não do malloc */
}
static c2tree_t *tree_of(const void *p) { return p && tree_ready && cx_is_subclass(object_getClass((id)p), &CFTreeMOS_c) ? (c2tree_t *)p : 0; }
u64 CFTreeGetTypeID(void) { return CF_TREE_TYPEID; }
static void tree_setctx(c2tree_t *t, const tree_ctx_t *c) {
    void *old = t->ctx.info;
    void (*oldrel)(const void *) = t->ctx.release;
    if (c) { t->ctx = *c; if (t->ctx.retain && t->ctx.info) t->ctx.info = (void *)t->ctx.retain(t->ctx.info); }
    else memset(&t->ctx, 0, sizeof t->ctx);
    if (oldrel && old) oldrel(old);
}
void *CFTreeCreate(void *alloc, const tree_ctx_t *c) {
    (void)alloc;
    if (!tree_ready) {
        cx_class_t *ns = cx_class_named("NSObject");
        if (!ns) return 0;
        cx_define_class(&CFTreeMOS_c, &CFTreeMOS_m, "CFTreeMOS", ns, (u32)(sizeof(c2tree_t) - sizeof(void *)));
        cx_add_method(&CFTreeMOS_c, "_cfTypeID", (void *)tree_tid);
        cx_add_method(&CFTreeMOS_c, "dealloc", (void *)tree_dealloc);
        tree_ready = 1;
    }
    c2tree_t *t = (c2tree_t *)cx_alloc_instance(&CFTreeMOS_c);
    if (t) tree_setctx(t, c);
    return t;
}
void CFTreeGetContext(c2tree_t *t, tree_ctx_t *c) { t = tree_of(t); if (t && c) *c = t->ctx; else if (c) memset(c, 0, sizeof *c); }
void CFTreeSetContext(c2tree_t *t, const tree_ctx_t *c) { t = tree_of(t); if (t) tree_setctx(t, c); }
long CFTreeGetChildCount(c2tree_t *t) { t = tree_of(t); return t ? t->nchild : 0; }
void CFTreeGetChildren(c2tree_t *t, void **out) { t = tree_of(t); if (!t || !out) return; long i = 0; for (c2tree_t *c = t->first; c; c = c->next) out[i++] = c; }
void *CFTreeGetParent(c2tree_t *t) { t = tree_of(t); return t ? t->parent : 0; }
void *CFTreeGetFirstChild(c2tree_t *t) { t = tree_of(t); return t ? t->first : 0; }
void *CFTreeGetNextSibling(c2tree_t *t) { t = tree_of(t); return t ? t->next : 0; }
void *CFTreeGetRoot(c2tree_t *t) { t = tree_of(t); while (t && t->parent) t = t->parent; return t; }
void *CFTreeGetChildAtIndex(c2tree_t *t, long i) {
    t = tree_of(t);
    if (!t || i < 0) return 0;
    for (c2tree_t *c = t->first; c; c = c->next) if (!i--) return c;
    return 0;
}
static void tree_unlink(c2tree_t *c) {
    c2tree_t *p = c->parent;
    if (!p) return;
    if (p->first == c) p->first = c->next;
    else for (c2tree_t *x = p->first; x; x = x->next) if (x->next == c) { x->next = c->next; if (p->last == c) p->last = x; break; }
    if (p->last == c && p->first == 0) p->last = 0;
    else if (p->last == c) { c2tree_t *x = p->first; while (x->next) x = x->next; p->last = x; }
    p->nchild--;
    c->parent = c->next = 0;
}
void CFTreeAppendChild(c2tree_t *t, c2tree_t *c) {
    t = tree_of(t); c = tree_of(c);
    if (!t || !c || c->parent) return;
    objc_retain((id)c);
    c->parent = t;
    if (t->last) t->last->next = c; else t->first = c;
    t->last = c;
    t->nchild++;
}
void CFTreePrependChild(c2tree_t *t, c2tree_t *c) {
    t = tree_of(t); c = tree_of(c);
    if (!t || !c || c->parent) return;
    objc_retain((id)c);
    c->parent = t;
    c->next = t->first;
    t->first = c;
    if (!t->last) t->last = c;
    t->nchild++;
}
void CFTreeInsertSibling(c2tree_t *t, c2tree_t *s) {
    t = tree_of(t); s = tree_of(s);
    if (!t || !s || !t->parent || s->parent) return;
    objc_retain((id)s);
    s->parent = t->parent;
    s->next = t->next;
    t->next = s;
    if (t->parent->last == t) t->parent->last = s;
    t->parent->nchild++;
}
void CFTreeRemove(c2tree_t *t) { t = tree_of(t); if (t && t->parent) { tree_unlink(t); objc_release((id)t); } }
void CFTreeRemoveAllChildren(c2tree_t *t) {
    if (!t) return;
    c2tree_t *c = t->first;
    t->first = t->last = 0;
    t->nchild = 0;
    while (c) { c2tree_t *n = c->next; c->parent = c->next = 0; objc_release((id)c); c = n; }
}
void CFTreeApplyFunctionToChildren(c2tree_t *t, void (*f)(const void *, void *), void *ctx) {
    t = tree_of(t);
    if (!t || !f) return;
    for (c2tree_t *c = t->first; c; ) { c2tree_t *n = c->next; f(c, ctx); c = n; }
}
void *CFTreeFindRoot(c2tree_t *t) { return CFTreeGetRoot(t); }

/* ================================================================ Foundation */
id NSZoneName(void *zone) { (void)zone; return cx_str("default"); }
BOOL NSShouldRetainWithZone(id obj, void *zone) { (void)obj; (void)zone; return YES; }
KS(NSMetadataUbiquitousItemURLInLocalContainerKey, "NSMetadataUbiquitousItemURLInLocalContainerKey")
KS(NSMetadataQueryUbiquitousDocumentsScope, "NSMetadataQueryUbiquitousDocumentsScope")
KS(NSMetadataQueryNetworkScope, "NSMetadataQueryNetworkScope")
KS(NSMetadataQueryIndexedNetworkScope, "NSMetadataQueryIndexedNetworkScope")
KS(NSMetadataQueryIndexedLocalComputerScope, "NSMetadataQueryIndexedLocalComputerScope")
KS(NSMetadataQueryAccessibleUbiquitousExternalDocumentsScope, "NSMetadataQueryAccessibleUbiquitousExternalDocumentsScope")
KS(NSMetadataItemLastUsedDateKey, "kMDItemLastUsedDate")
KS(NSMetadataItemContentTypeKey, "kMDItemContentType")
KS(NSMetadataItemContentModificationDateKey, "kMDItemContentModificationDate")
KS(NSMetadataItemContentCreationDateKey, "kMDItemContentCreationDate")
KS(NSMetadataItemCFBundleIdentifierKey, "kMDItemCFBundleIdentifier")

/* ================================================================ CoreServices */
KS(kUTTypeResolvable, "com.apple.resolvable")
KS(kLSItemContentType, "LSItemContentType")
id MDItemCopyAttributes(id item, id names);
/* MDItemCopyAttributes(item, nome, nome, ..., NULL) */
id __MDItemCopyAttributesEllipsis1(id item, ...) {
    id arr = cx_arr_new(0);
    va_list ap;
    va_start(ap, item);
    for (id n; (n = va_arg(ap, id)) != 0; ) cx_arr_add(arr, n);
    va_end(ap);
    return MDItemCopyAttributes(item, arr);
}
OSStatus TECCountAvailableSniffers(unsigned long *n) { if (!n) return -50; *n = 0; return 0; }
OSStatus TECGetAvailableSniffers(u32 *buf, unsigned long max, unsigned long *n) { (void)buf; (void)max; if (!n) return -50; *n = 0; return 0; }
OSStatus GetIconRefFromTypeInfo(u32 creator, u32 type, id ext, id mime, unsigned usage, void **out);
/* o ícone pelo tipo da extensão do nome (o mOS não guarda ícone por arquivo no catálogo) */
OSStatus GetIconRefFromFileInfo(const void *ref, unsigned long nlen, const u16 *name, u32 bitmap, const void *info,
                                unsigned usage, void **out, i16 *label) {
    (void)ref; (void)bitmap; (void)info;
    if (label) *label = 0;
    if (!out) return -50;
    id ext = 0;
    if (name && nlen) {
        long dot = -1;
        for (unsigned long i = 0; i < nlen; i++) if (name[i] == '.') dot = (long)i;
        if (dot >= 0 && (unsigned long)dot + 1 < nlen) {
            char e[32]; unsigned long k = 0;
            for (unsigned long i = (unsigned long)dot + 1; i < nlen && k < sizeof e - 1; i++) e[k++] = name[i] < 128 ? (char)name[i] : '_';
            e[k] = 0;
            ext = cx_str(e);
        }
    }
    return GetIconRefFromTypeInfo(0, 0, ext, 0, usage, out);
}

/* ================================================================ CoreVideo */
KS(kCVPixelFormatOpenGLCompatibility, "OpenGLCompatibility")
KS(kCVPixelBufferExtendedPixelsRightKey, "ExtendedPixelsRight")
KS(kCVPixelBufferExtendedPixelsBottomKey, "ExtendedPixelsBottom")
KS(kCVImageBufferFieldDetailTemporalTopFirst, "TemporalTopFirst")
KS(kCVImageBufferFieldDetailTemporalBottomFirst, "TemporalBottomFirst")
void *CVImageBufferGetColorSpace(void *buf) { (void)buf; return 0; }     /* sem espaço de cor anexado: o chamador usa o sRGB */

/* ================================================================ AudioToolbox */
void CAShow(void *obj) {
    char b[64];
    int n = snprintf(b, sizeof b, "<CAShow %p>\n", obj);
    if (n > 0) write(2, b, (size_t)n);
}
OSStatus AudioObjectGetPropertyData(u32 obj, const void *a, u32 qs, const void *q, u32 *size, void *data);
OSStatus AudioObjectSetPropertyData(u32 obj, const void *a, u32 qs, const void *q, u32 size, const void *data);
u8 AudioObjectHasProperty(u32 obj, const void *a);
OSStatus AudioHardwareServiceGetPropertyData(u32 o, const void *a, u32 qs, const void *q, u32 *sz, void *d) { return AudioObjectGetPropertyData(o, a, qs, q, sz, d); }
OSStatus AudioHardwareServiceSetPropertyData(u32 o, const void *a, u32 qs, const void *q, u32 sz, const void *d) { return AudioObjectSetPropertyData(o, a, qs, q, sz, d); }
u8 AudioHardwareServiceHasProperty(u32 o, const void *a) { return AudioObjectHasProperty(o, a); }
/* AudioFileOpenWithCallbacks: lê tudo pelos callbacks para um arquivo temporário e abre com o AudioFileOpenURL */
OSStatus AudioFileOpenURL(id url, i8 perm, u32 hint, void **out);
int mkstemp(char *t);
int close(int fd);
int unlink(const char *p);
typedef OSStatus (*af_read_t)(void *ud, i64 pos, u32 n, void *buf, u32 *got);
typedef i64 (*af_size_t)(void *ud);
OSStatus AudioFileOpenWithCallbacks(void *ud, af_read_t rd, void *wr, af_size_t gs, void *ss, u32 hint, void **out) {
    (void)wr; (void)ss;
    if (!rd || !gs || !out) return -50;
    i64 total = gs(ud);
    if (total <= 0) return 0x7479703f;                                   /* kAudioFileUnsupportedFileTypeError 'typ?' */
    char path[] = "/tmp/mos-audiofile-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) return -36;
    u8 *buf = malloc(65536);
    i64 pos = 0;
    OSStatus st = 0;
    while (buf && pos < total) {
        u32 want = (u32)(total - pos > 65536 ? 65536 : total - pos), got = 0;
        st = rd(ud, pos, want, buf, &got);
        if (st || !got) break;
        if (write(fd, buf, got) != (long)got) { st = -36; break; }
        pos += got;
    }
    free(buf);
    close(fd);
    if (st || pos == 0) { unlink(path); return st ? st : -36; }
    id url = MSG(id, (id)cx_class_named("NSURL"), "fileURLWithPath:", cx_str(path));
    st = AudioFileOpenURL(url, 1, hint, out);
    unlink(path);                                                        /* o descritor aberto mantém os dados */
    return st;
}

/* ================================================================ CoreGraphics / CGS */
KS(kCGWindowWorkspace, "kCGWindowWorkspace")
KS(kCGWindowStoreType, "kCGWindowStoreType")
KS(kCGWindowBackingLocationVideoMemory, "kCGWindowBackingLocationVideoMemory")
KS(kCGDisplayStreamYCbCrMatrix_ITU_R_709_2, "ITU_R_709_2")
KS(kCGDisplayStreamSourceRect, "kCGDisplayStreamSourceRect")
KS(kCGDisplayStreamQueueDepth, "kCGDisplayStreamQueueDepth")
typedef struct { double x, y, w, h; } c2rect_t;
typedef struct { c2rect_t *r; int n; int cur; } cgs_region_t;
int CGSNewRegionWithRect(const c2rect_t *r, void **out) {
    if (!out) return 1000;                                               /* kCGErrorFailure */
    cgs_region_t *g = calloc(1, sizeof *g);
    if (!g) return 1000;
    if (r && r->w > 0 && r->h > 0) { g->r = malloc(sizeof *r); *g->r = *r; g->n = 1; }
    *out = g;
    return 0;
}
int CGSReleaseRegion(void *g) { cgs_region_t *x = g; if (x) { free(x->r); free(x); } return 0; }
void *CGSRegionEnumerator(void *g) {
    cgs_region_t *x = g, *e = calloc(1, sizeof *e);
    if (e && x && x->n) { e->r = malloc(sizeof(c2rect_t) * (size_t)x->n); memcpy(e->r, x->r, sizeof(c2rect_t) * (size_t)x->n); e->n = x->n; }
    return e;
}
c2rect_t *CGSNextRect(void *en) { cgs_region_t *e = en; return e && e->cur < e->n ? &e->r[e->cur++] : 0; }
int CGSReleaseRegionEnumerator(void *en) { return CGSReleaseRegion(en); }
int CGSRegisterNotifyProc(void *proc, u32 type, void *ud) { (void)proc; (void)type; (void)ud; return 0; }
int CGSRegisterConnectionNotifyProc(int cid, void *proc, u32 type, void *ud) { (void)cid; (void)proc; (void)type; (void)ud; return 0; }
int CGSSetSymbolicHotKeyValue(int hk, u16 ch, u16 key, u32 mods) { (void)hk; (void)ch; (void)key; (void)mods; return 0; }
void CGSMoveWindowsToManagedSpace(int cid, id windows, u64 space) { (void)cid; (void)windows; (void)space; }
int CGSGetCursorScale(int cid, float *s) { (void)cid; if (s) *s = 1.0f; return 0; }
size_t CGDisplayPixelsWide(u32 d);
size_t CGDisplayPixelsHigh(u32 d);
/* CGSDisplayModeDescription: número, flags, largura, altura, profundidade; ... frequência em 0xBC (u16) e densidade */
int CGSGetNumberOfDisplayModes(u32 d, int *n) { (void)d; if (n) *n = 1; return 0; }
int CGSGetCurrentDisplayMode(u32 d, int *m) { (void)d; if (m) *m = 0; return 0; }
int CGSGetDisplayModeDescriptionOfLength(u32 d, int idx, void *desc, int len) {
    if (!desc || len < 20 || idx != 0) return 1001;                      /* kCGErrorIllegalArgument */
    memset(desc, 0, (size_t)len);
    u32 *p = desc;
    p[0] = 0; p[1] = 0x00000007 | 0x02000000;
    p[2] = (u32)CGDisplayPixelsWide(d ? d : 1); p[3] = (u32)CGDisplayPixelsHigh(d ? d : 1); p[4] = 4;
    if (len >= 0xBE) *(u16 *)((u8 *)desc + 0xBC) = 60;
    if (len >= 0xD0) *(float *)((u8 *)desc + 0xCC) = 1.0f;
    return 0;
}
int CGSConfigureDisplayMode(void *cfg, u32 d, int mode) { (void)cfg; (void)d; return mode == 0 ? 0 : 1001; }
id CFArrayCreate(void *alloc, const id *vals, long n, const void *cb);
extern const char kCFTypeArrayCallBacks[];
id CGSCopyWindowsWithOptionsAndTags(int cid, int owner, id spaces, int opts, u64 *set, u64 *clear) {
    (void)cid; (void)owner; (void)spaces; (void)opts;
    if (set) *set = 0;
    if (clear) *clear = 0;
    return CFArrayCreate(0, 0, 0, kCFTypeArrayCallBacks);
}
int CGContextGetType(void *ctx) { return ctx ? 4 : 0; }                  /* kCGContextTypeBitmap: os contextos do mOS são bitmaps */

/* ================================================================ IOKit */
#define kIOReturnUnsupported ((int)0xe00002c7)
#define kIOReturnBadArgument ((int)0xe00002c2)
id KextManagerCopyLoadedKextInfo(id ids, id keys) { (void)ids; (void)keys; return objc_retain(MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary")); }
int IOServiceRequestProbe(u32 svc, u32 opts) { (void)svc; (void)opts; return 0; }
int IORegistryCreateIterator(u32 port, const char *plane, u32 opts, u32 *it) { (void)port; (void)plane; (void)opts; if (it) *it = 0; return 0; }
int IOPMAssertionSetProperty(u32 a, id key, id val) { (void)a; (void)key; (void)val; return 0; }
int IOI2CInterfaceOpen(u32 iface, u32 opts, void **conn) { (void)iface; (void)opts; if (conn) *conn = 0; return kIOReturnUnsupported; }
int IOI2CInterfaceClose(void *conn, u32 opts) { (void)conn; (void)opts; return 0; }
int IOI2CSendRequest(void *conn, u32 opts, void *req) { (void)conn; (void)opts; (void)req; return kIOReturnUnsupported; }
int IOFBGetI2CInterfaceCount(u32 fb, u32 *n) { (void)fb; if (n) *n = 0; return 0; }
int IOFBCopyI2CInterfaceForBus(u32 fb, u32 bus, u32 *iface) { (void)fb; (void)bus; if (iface) *iface = 0; return kIOReturnBadArgument; }
void IOHIDServiceClientRegisterRemovalBlock(void *svc, void *blk, void *t, void *r) { (void)svc; (void)blk; (void)t; (void)r; }
void IOHIDEventSystemClientScheduleWithDispatchQueue(void *c, void *q) { (void)c; (void)q; }
void IOHIDEventSystemClientUnscheduleFromDispatchQueue(void *c, void *q) { (void)c; (void)q; }
void IOHIDEventSystemClientRegisterDeviceMatchingBlock(void *c, void *blk, void *t, void *r) { (void)c; (void)blk; (void)t; (void)r; }
void IOHIDEventSystemClientUnregisterDeviceMatchingBlock(void *c) { (void)c; }
void IOHIDEventSystemClientSetMatching(void *client, id matching);
void IOHIDEventSystemClientSetMatchingMultiple(void *c, id arr) {
    id first = arr && MSG0(u64, arr, "count") ? MSG(id, arr, "objectAtIndex:", (u64)0) : 0;
    IOHIDEventSystemClientSetMatching(c, first);
}
id IOHIDEventSystemClientCopyProperty(void *c, id key) { (void)c; (void)key; return 0; }
void IOHIDEventSetFloatValue(void *ev, u32 field, double v) { (void)ev; (void)field; (void)v; }
u8 IOHIDElementHasPreferredState(void *el) { (void)el; return 1; }
int IOHIDDeviceSetReportWithCallback(void *dev, int type, long id_, const u8 *rep, long len, double timeout, void *cb, void *ctx) {
    (void)dev; (void)type; (void)id_; (void)rep; (void)len; (void)timeout; (void)cb; (void)ctx;
    return kIOReturnUnsupported;
}

/* ================================================================ libpcap: pcap_open_dead e arquivos de captura */
typedef struct { int dead; int linktype; int snaplen; char err[256]; } c2pcap_t;
typedef struct { long sec, usec; u32 caplen, len; } c2pkthdr_t;            /* struct pcap_pkthdr (timeval de 64 bits) */
typedef struct { u16 code; u8 jt, jf; u32 k; } bpf_insn_t;
typedef struct { u32 len; bpf_insn_t *insns; } bpf_prog_t;
FILE_ *fopen(const char *p, const char *m);
size_t fwrite(const void *b, size_t s, size_t n, FILE_ *f);
int fclose(FILE_ *f);
int fflush(FILE_ *f);
void *pcap_open_dead(int linktype, int snaplen) {
    c2pcap_t *p = calloc(1, sizeof *p);
    if (p) { p->dead = 0x50434150; p->linktype = linktype; p->snaplen = snaplen > 0 ? snaplen : 262144; }
    return p;
}
void *pcap_open_dead_with_tstamp_precision(int linktype, int snaplen, u32 prec) { (void)prec; return pcap_open_dead(linktype, snaplen); }
static c2pcap_t *pc(void *p) { return p && ((c2pcap_t *)p)->dead == 0x50434150 ? (c2pcap_t *)p : 0; }
int pcap_datalink(void *p) { return pc(p) ? pc(p)->linktype : -3; }       /* PCAP_ERROR_NOT_ACTIVATED */
int pcap_snapshot(void *p) { return pc(p) ? pc(p)->snaplen : -3; }
int pcap_list_datalinks(void *p, int **out) {
    if (!pc(p) || !out) return -1;
    *out = malloc(sizeof(int));
    if (!*out) return -1;
    **out = pc(p)->linktype;
    return 1;
}
void pcap_free_datalinks(int *l) { free(l); }
int pcap_set_datalink(void *p, int dlt) { return pc(p) && pc(p)->linktype == dlt ? 0 : -1; }
int pcap_list_tstamp_types(void *p, int **out) { (void)p; if (out) *out = 0; return 0; }
void pcap_free_tstamp_types(int *l) { free(l); }
int pcap_set_tstamp_type(void *p, int t) { (void)p; (void)t; return 3; }   /* PCAP_WARNING_TSTAMP_TYPE_NOTSUP */
int pcap_set_rfmon(void *p, int on) { (void)p; (void)on; return -1; }
int pcap_can_set_rfmon(void *p) { (void)p; return 0; }
void pcap_breakloop(void *p) { (void)p; }
int pcap_stats(void *p, void *st) { (void)st; if (pc(p)) strcpy(pc(p)->err, "estatísticas indisponíveis numa captura morta"); return -1; }
int pcap_lookupnet(const char *dev, u32 *net, u32 *mask, char *err) {
    (void)dev;
    if (net) *net = 0;
    if (mask) *mask = 0;
    if (err) strcpy(err, "sem interface de captura no mOS");
    return -1;
}
/* o único filtro que o mOS compila é o vazio: aceita tudo (BPF_RET|BPF_K snaplen) */
int pcap_compile(void *p, bpf_prog_t *prog, const char *expr, int opt, u32 mask) {
    (void)opt; (void)mask;
    if (!prog) return -1;
    while (expr && (*expr == ' ' || *expr == '\t')) expr++;
    if (expr && *expr) { if (pc(p)) strcpy(pc(p)->err, "o mOS não compila filtros BPF"); return -1; }
    prog->insns = malloc(sizeof(bpf_insn_t));
    if (!prog->insns) return -1;
    prog->insns[0] = (bpf_insn_t){0x06, 0, 0, pc(p) ? (u32)pc(p)->snaplen : 262144};
    prog->len = 1;
    return 0;
}
void pcap_freecode(bpf_prog_t *prog) { if (prog) { free(prog->insns); prog->insns = 0; prog->len = 0; } }
int pcap_setfilter(void *p, bpf_prog_t *prog) { (void)prog; return pc(p) ? 0 : -1; }
/* pcap_dump_*: o formato clássico (magia a1b2c3d4, versão 2.4, cabeçalho de pacote com segundos/micros de 32 bits) */
void *pcap_dump_open(void *p, const char *fname) {
    c2pcap_t *x = pc(p);
    if (!x || !fname) return 0;
    FILE_ *f = fopen(fname, "wb");
    if (!f) { strcpy(x->err, "não consegui criar o arquivo"); return 0; }
    u32 h[6] = {0xa1b2c3d4, 0x00040002, 0, 0, (u32)x->snaplen, (u32)x->linktype};
    fwrite(h, 1, sizeof h, f);
    return f;
}
void pcap_dump(u8 *user, const c2pkthdr_t *hdr, const u8 *data) {
    if (!user || !hdr) return;
    u32 r[4] = {(u32)hdr->sec, (u32)hdr->usec, hdr->caplen, hdr->len};
    fwrite(r, 1, sizeof r, (FILE_ *)user);
    if (data && hdr->caplen) fwrite(data, 1, hdr->caplen, (FILE_ *)user);
}
int pcap_dump_flush(void *d) { return d ? fflush((FILE_ *)d) : -1; }
void pcap_dump_close(void *d) { if (d) fclose((FILE_ *)d); }
static const struct { int v; const char *name, *desc; } dlts[] = {
    {0, "NULL", "BSD loopback"}, {1, "EN10MB", "Ethernet"}, {9, "PPP", "PPP"}, {12, "RAW", "Raw IP"},
    {105, "IEEE802_11", "802.11"}, {108, "LOOP", "OpenBSD loopback"}, {113, "LINUX_SLL", "Linux cooked v1"},
    {127, "IEEE802_11_RADIO", "802.11 plus radiotap header"}, {147, "USER0", "USER0"}, {228, "IPV4", "Raw IPv4"},
    {229, "IPV6", "Raw IPv6"}, {276, "LINUX_SLL2", "Linux cooked v2"},
};
static int ci_eq(const char *a, const char *b) {
    for (; *a && *b; a++, b++) { char x = *a >= 'a' && *a <= 'z' ? *a - 32 : *a, y = *b >= 'a' && *b <= 'z' ? *b - 32 : *b; if (x != y) return 0; }
    return *a == *b;
}
const char *pcap_datalink_val_to_name(int v) { for (unsigned i = 0; i < sizeof dlts / sizeof dlts[0]; i++) if (dlts[i].v == v) return dlts[i].name; return 0; }
const char *pcap_datalink_val_to_description(int v) { for (unsigned i = 0; i < sizeof dlts / sizeof dlts[0]; i++) if (dlts[i].v == v) return dlts[i].desc; return 0; }
int pcap_datalink_name_to_val(const char *n) { if (n) for (unsigned i = 0; i < sizeof dlts / sizeof dlts[0]; i++) if (ci_eq(n, dlts[i].name)) return dlts[i].v; return -1; }
static const char *const tst_names[] = {"host", "host_lowprec", "host_hiprec", "adapter", "adapter_unsynced", "host_hiprec_unsynced"};
static const char *const tst_desc[] = {"Host", "Host, low precision", "Host, high precision, synced with the system clock",
                                       "Adapter", "Adapter, not synced with the system clock", "Host, high precision, not synced with the system clock"};
const char *pcap_tstamp_type_val_to_name(int v) { return v >= 0 && v < 6 ? tst_names[v] : 0; }
const char *pcap_tstamp_type_val_to_description(int v) { return v >= 0 && v < 6 ? tst_desc[v] : 0; }
int pcap_tstamp_type_name_to_val(const char *n) { if (n) for (int i = 0; i < 6; i++) if (ci_eq(n, tst_names[i])) return i; return -1; }

/* ================================================================ OpenCL: sem plataforma (clGetPlatformIDs dá -1001) */
#define CL_INVALID_VALUE (-30)
#define CL_INVALID_DEVICE (-33)
#define CL_INVALID_CONTEXT (-34)
#define CL_INVALID_COMMAND_QUEUE (-36)
#define CL_INVALID_MEM_OBJECT (-38)
#define CL_INVALID_SAMPLER (-41)
#define CL_INVALID_PROGRAM (-44)
#define CL_INVALID_KERNEL (-48)
#define CL_INVALID_EVENT (-58)
#define CL_INVALID_PLATFORM (-32)
static void *cl_err(i32 *e, i32 c) { if (e) *e = c; return 0; }
static i32 cl_info(size_t *ret, i32 c) { if (ret) *ret = 0; return c; }
i32 clUnloadPlatformCompiler(void *p) { (void)p; return CL_INVALID_PLATFORM; }
i32 clSetUserEventStatus(void *e, i32 s) { (void)e; (void)s; return CL_INVALID_EVENT; }
i32 clSetEventCallback(void *e, i32 t, void *cb, void *ud) { (void)e; (void)t; (void)cb; (void)ud; return CL_INVALID_EVENT; }
i32 clRetainSampler(void *s) { (void)s; return CL_INVALID_SAMPLER; }
i32 clReleaseSampler(void *s) { (void)s; return CL_INVALID_SAMPLER; }
i32 clRetainDevice(void *d) { (void)d; return CL_INVALID_DEVICE; }
i32 clReleaseDevice(void *d) { (void)d; return CL_INVALID_DEVICE; }
i32 clGetSamplerInfo(void *s, u32 n, size_t sz, void *v, size_t *r) { (void)s; (void)n; (void)sz; (void)v; return cl_info(r, CL_INVALID_SAMPLER); }
i32 clGetProgramInfo(void *p, u32 n, size_t sz, void *v, size_t *r) { (void)p; (void)n; (void)sz; (void)v; return cl_info(r, CL_INVALID_PROGRAM); }
i32 clGetKernelWorkGroupInfo(void *k, void *d, u32 n, size_t sz, void *v, size_t *r) { (void)k; (void)d; (void)n; (void)sz; (void)v; return cl_info(r, CL_INVALID_KERNEL); }
i32 clGetKernelInfo(void *k, u32 n, size_t sz, void *v, size_t *r) { (void)k; (void)n; (void)sz; (void)v; return cl_info(r, CL_INVALID_KERNEL); }
i32 clGetKernelArgInfo(void *k, u32 i, u32 n, size_t sz, void *v, size_t *r) { (void)k; (void)i; (void)n; (void)sz; (void)v; return cl_info(r, CL_INVALID_KERNEL); }
i32 clGetEventInfo(void *e, u32 n, size_t sz, void *v, size_t *r) { (void)e; (void)n; (void)sz; (void)v; return cl_info(r, CL_INVALID_EVENT); }
i32 clGetContextInfo(void *c, u32 n, size_t sz, void *v, size_t *r) { (void)c; (void)n; (void)sz; (void)v; return cl_info(r, CL_INVALID_CONTEXT); }
i32 clGetCommandQueueInfo(void *q, u32 n, size_t sz, void *v, size_t *r) { (void)q; (void)n; (void)sz; (void)v; return cl_info(r, CL_INVALID_COMMAND_QUEUE); }
i32 clEnqueueTask(void *q, void *k, u32 n, const void *w, void **e) { (void)q; (void)k; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE; }
i32 clEnqueueMigrateMemObjects(void *q, u32 nm, const void *m, u64 f, u32 n, const void *w, void **e) { (void)q; (void)nm; (void)m; (void)f; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE; }
i32 clEnqueueMarkerWithWaitList(void *q, u32 n, const void *w, void **e) { (void)q; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE; }
i32 clEnqueueBarrierWithWaitList(void *q, u32 n, const void *w, void **e) { (void)q; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE; }
i32 clEnqueueFillImage(void *q, void *img, const void *c, const size_t *o, const size_t *r, u32 n, const void *w, void **e) { (void)q; (void)img; (void)c; (void)o; (void)r; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE; }
i32 clEnqueueCopyImageToBuffer(void *q, void *img, void *buf, const size_t *o, const size_t *r, size_t off, u32 n, const void *w, void **e) { (void)q; (void)img; (void)buf; (void)o; (void)r; (void)off; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE; }
i32 clEnqueueCopyBuffer(void *q, void *s, void *d, size_t so, size_t dof, size_t sz, u32 n, const void *w, void **e) { (void)q; (void)s; (void)d; (void)so; (void)dof; (void)sz; (void)n; (void)w; if (e) *e = 0; return CL_INVALID_COMMAND_QUEUE; }
void *clCreateUserEvent(void *ctx, i32 *e) { (void)ctx; return cl_err(e, CL_INVALID_CONTEXT); }
i32 clCreateSubDevices(void *d, const void *props, u32 n, void **out, u32 *num) { (void)d; (void)props; (void)n; (void)out; if (num) *num = 0; return CL_INVALID_DEVICE; }
void *clCreateSubBuffer(void *b, u64 f, u32 t, const void *info, i32 *e) { (void)b; (void)f; (void)t; (void)info; return cl_err(e, CL_INVALID_MEM_OBJECT); }
void *clCreateSampler(void *ctx, u32 norm, u32 addr, u32 filt, i32 *e) { (void)ctx; (void)norm; (void)addr; (void)filt; return cl_err(e, CL_INVALID_CONTEXT); }
void *clCreateProgramWithBuiltInKernels(void *ctx, u32 n, void *const *d, const char *names, i32 *e) { (void)ctx; (void)n; (void)d; (void)names; return cl_err(e, CL_INVALID_CONTEXT); }
void *clCreateProgramWithBinary(void *ctx, u32 n, void *const *d, const size_t *len, const u8 **bin, i32 *st, i32 *e) { (void)ctx; (void)n; (void)d; (void)len; (void)bin; (void)st; return cl_err(e, CL_INVALID_CONTEXT); }
i32 clCreateKernelsInProgram(void *p, u32 n, void **k, u32 *num) { (void)p; (void)n; (void)k; if (num) *num = 0; return CL_INVALID_PROGRAM; }

/* ================================================================ constantes de vários frameworks */
KS(kCAFilterSoftLightBlendMode, "softLightBlendMode") KS(kCAFilterScreenBlendMode, "screenBlendMode")
KS(kCAFilterOverlayBlendMode, "overlayBlendMode") KS(kCAFilterMultiplyBlendMode, "multiplyBlendMode")
KS(kCAFilterLightenBlendMode, "lightenBlendMode") KS(kCAFilterHardLightBlendMode, "hardLightBlendMode")
KS(kCAFilterGaussianBlur, "gaussianBlur") KS(kCAFilterExclusionBlendMode, "exclusionBlendMode")
KS(kCAFilterDifferenceBlendMode, "differenceBlendMode") KS(kCAFilterDarkenBlendMode, "darkenBlendMode")
KS(kCAFilterColorSaturate, "colorSaturate") KS(kCAFilterColorDodgeBlendMode, "colorDodgeBlendMode")
KS(kCAFilterColorBurnBlendMode, "colorBurnBlendMode")
KS(kSCValNetIPv6ConfigMethodManual, "Manual") KS(kSCValNetIPv6ConfigMethodLinkLocal, "LinkLocal")
KS(kSCPropNetIPv6ConfigMethod, "ConfigMethod") KS(kSCEntNetModem, "Modem")
KS(kVTProfileLevel_H264_Extended_5_0, "H264_Extended_5_0")
KS(kVTDecompressionResolutionKey_Width, "Width") KS(kVTDecompressionResolutionKey_Height, "Height")
KS(kVTDecompressionProperty_OnlyTheseFrames_KeyFrames, "KeyFrames")
KS(kVTDecompressionPropertyKey_ReducedResolutionDecode, "ReducedResolutionDecode")
KS(kVTDecompressionPropertyKey_OnlyTheseFrames, "OnlyTheseFrames")
KS(kVTCompressionPropertyKey_FieldDetail, "FieldDetail") KS(kVTCompressionPropertyKey_FieldCount, "FieldCount")
KS(kCTFontPriorityAttribute, "NSCTFontPriorityAttribute") KS(kCTFontOpticalSizeAttribute, "NSCTFontOpticalSizeAttribute")
KS(kCTFontFixedAdvanceAttribute, "NSCTFontFixedAdvanceAttribute")
KS(kCGImagePropertyTIFFTileWidth, "TileWidth") KS(kCGImagePropertyTIFFTileLength, "TileLength")
const long kCFStreamErrorDomainWinSock = 10;

/* SystemConfiguration: serviço sem protocolo configurável; conexões (PPP/VPN) não existem no mOS */
void *SCNetworkServiceCopyProtocol(void *svc, id type) { (void)svc; (void)type; return 0; }
id SCNetworkProtocolGetConfiguration(void *proto) { (void)proto; return 0; }
u8 SCNetworkConnectionStart(void *c, id opts, u8 linger) { (void)c; (void)opts; (void)linger; return 0; }
u8 SCNetworkConnectionStop(void *c, u8 force) { (void)c; (void)force; return 0; }
int SCNetworkConnectionGetStatus(void *c) { (void)c; return -1; }       /* kSCNetworkConnectionInvalid */

/* CFNetwork: o mDNS não roda no mOS, então os browsers não acham nada (e não há onde registrar cliente) */
u8 CFNetServiceSetClient(void *svc, void *cb, void *ctx) { (void)svc; (void)ctx; return cb == 0; }
void *CFNetServiceBrowserCreate(void *alloc, void *cb, void *ctx) { (void)alloc; (void)cb; (void)ctx; return 0; }

/* ================================================================ libobjc */
static struct { cx_class_t *c; int v; } cls_ver[64];
static int ncls_ver;
void class_setVersion(cx_class_t *c, int v) {
    if (!c) return;
    for (int i = 0; i < ncls_ver; i++) if (cls_ver[i].c == c) { cls_ver[i].v = v; return; }
    if (ncls_ver < 64) { cls_ver[ncls_ver].c = c; cls_ver[ncls_ver++].v = v; }
}
int class_getVersion(cx_class_t *c) {
    for (int i = 0; i < ncls_ver; i++) if (cls_ver[i].c == c) return cls_ver[i].v;
    return 0;
}
/* a classe dos objetos Protocol (objcrt.c põe objc_getClass("Protocol") no isa de cada protocolo) */
cx_class_t Protocol_c __asm__("OBJC_CLASS_$_Protocol"), Protocol_m __asm__("OBJC_METACLASS_$_Protocol");
void cx_corpus2_init(void) {
    cx_class_t *ns = cx_class_named("NSObject");
    if (ns && !cx_class_named("Protocol")) cx_define_class(&Protocol_c, &Protocol_m, "Protocol", ns, 0);
}

/* ================================================================ libbz2: BZ2_bzopen/bzflush/bzclose */
void *BZ2_bzReadOpen(int *err, FILE_ *f, int verbosity, int small, void *unused, int nunused);
void BZ2_bzReadClose(int *err, void *b);
void *BZ2_bzWriteOpen(int *err, FILE_ *f, int block, int verbosity, int work);
void BZ2_bzWriteClose64(int *err, void *b, int abandon, u32 *in_lo, u32 *in_hi, u32 *out_lo, u32 *out_hi);
FILE_ *fdopen(int fd, const char *m);
static struct { void *bz; FILE_ *f; int writing; } bzfiles[16];
static void *bz_open(FILE_ *f, const char *mode) {
    if (!f) return 0;
    int w = 0, level = 9;
    for (const char *m = mode; m && *m; m++) { if (*m == 'w') w = 1; if (*m >= '1' && *m <= '9') level = *m - '0'; }
    int err = 0;
    void *bz = w ? BZ2_bzWriteOpen(&err, f, level, 0, 30) : BZ2_bzReadOpen(&err, f, 0, 0, 0, 0);
    if (!bz) { fclose(f); return 0; }
    for (int i = 0; i < 16; i++) if (!bzfiles[i].bz) { bzfiles[i].bz = bz; bzfiles[i].f = f; bzfiles[i].writing = w; return bz; }
    if (w) BZ2_bzWriteClose64(&err, bz, 1, 0, 0, 0, 0); else BZ2_bzReadClose(&err, bz);
    fclose(f);
    return 0;
}
void *BZ2_bzopen(const char *path, const char *mode) {
    if (!path || !mode) return 0;
    int w = 0;
    for (const char *m = mode; *m; m++) if (*m == 'w') w = 1;
    return bz_open(fopen(path, w ? "wb" : "rb"), mode);
}
void *BZ2_bzdopen(int fd, const char *mode) {
    if (fd < 0 || !mode) return 0;
    int w = 0;
    for (const char *m = mode; *m; m++) if (*m == 'w') w = 1;
    return bz_open(fdopen(fd, w ? "wb" : "rb"), mode);
}
int BZ2_bzflush(void *b) { (void)b; return 0; }
void BZ2_bzclose(void *b) {
    if (!b) return;
    int err = 0;
    for (int i = 0; i < 16; i++)
        if (bzfiles[i].bz == b) {
            if (bzfiles[i].writing) BZ2_bzWriteClose64(&err, b, 0, 0, 0, 0, 0); else BZ2_bzReadClose(&err, b);
            fclose(bzfiles[i].f);
            bzfiles[i].bz = 0;
            return;
        }
}

/* ================================================================ Security */
typedef struct { u32 count; u32 *tag; u32 *format; } kc_attrinfo_t;
OSStatus SecKeychainAttributeInfoForItemID(void *kc, u32 item, kc_attrinfo_t **info) {
    (void)kc; (void)item;
    if (!info) return -50;
    *info = calloc(1, sizeof **info);
    return *info ? 0 : -108;
}
OSStatus SecKeychainFreeAttributeInfo(kc_attrinfo_t *info) { if (info) { free(info->tag); free(info->format); free(info); } return 0; }

/* ================================================================ AppKit */
/* NSConvertGlyphsToPackedGlyphs: no NSNativeShortGlyphPacking (5) cada glifo vira 2 bytes big-endian */
long NSConvertGlyphsToPackedGlyphs(const u32 *glyphs, long n, long packing, char *out) {
    if (!glyphs || n <= 0) return 0;
    if (packing == 5 || packing == 0) {
        if (out) for (long i = 0; i < n; i++) { out[2 * i] = (char)(glyphs[i] >> 8); out[2 * i + 1] = (char)glyphs[i]; }
        return n * 2;
    }
    if (out) for (long i = 0; i < n; i++) out[i] = (char)glyphs[i];
    return n;
}
typedef struct { double x, y; } c2pt_t;
typedef struct { c2pt_t o; double w, h; } c2r_t;
/* ponto da view em coordenadas de tela (as de acessibilidade têm a origem no canto inferior esquerdo, como no AppKit) */
c2pt_t NSAccessibilityPointInView(id view, c2pt_t p) {
    if (!view) return p;
    c2pt_t w = MSG(c2pt_t, view, "convertPoint:toView:", p, (id)0);
    id win = MSG0(id, view, "window");
    if (!win) return w;
    c2r_t r = {w, 0, 0};
    c2r_t s = MSG(c2r_t, win, "convertRectToScreen:", r);
    return s.o;
}

/* ================================================================ Metal */
/* MTLTextureSwizzleChannels são 4 bytes (r, g, b, a); a chave empacota cada um em 8 bits */
typedef struct { u8 r, g, b, a; } swz_t;
u32 MTLTextureSwizzleChannelsToKey(swz_t c) { return (u32)c.r | (u32)c.g << 8 | (u32)c.b << 16 | (u32)c.a << 24; }
swz_t MTLTextureSwizzleKeyToChannels(u32 k) { swz_t c = {(u8)k, (u8)(k >> 8), (u8)(k >> 16), (u8)(k >> 24)}; return c; }
void MTLSetShaderCachePath(const char *p) { (void)p; }
void MTLMakeShaderCacheWritableByAllUsers(void) {}

/* ================================================================ PrintCore / CGPSConverter */
OSStatus PMSetPageFormatExtendedData(void *pf, u32 id_, u32 size, void *data) { (void)id_; (void)size; (void)data; return pf ? 0 : -30840; }
OSStatus PMPaperGetPrinterID(void *paper, id *out) { if (out) *out = 0; return paper ? -9779 : -30840; }   /* kPMInvalidPaper */
void *CGPSConverterCreate(void *info, const void *cb, id opts) { (void)info; (void)cb; (void)opts; return 0; }   /* sem interpretador PostScript */
u8 CGPSConverterConvert(void *conv, void *prov, void *cons, id opts) { (void)conv; (void)prov; (void)cons; (void)opts; return 0; }

/* ================================================================ libsasl2: cliente sem mecanismo */
static const char *sasl_msg(int c) {
    switch (c) {
    case 0: return "successful result";
    case 1: return "another step is needed in authentication";
    case -1: return "generic failure";
    case -2: return "no memory available";
    case -4: return "no mechanism available";
    case -7: return "bad parameter supplied";
    case -12: return "not initialized";
    case -13: return "authentication failure";
    case -14: return "authorization failure";
    default: return "unknown error";
    }
}
const char *sasl_errstring(int c, const char *langs, const char **outlang) { (void)langs; if (outlang) *outlang = "en-us"; return sasl_msg(c); }
const char *sasl_errdetail(void *conn) { (void)conn; return "SASL: nenhum mecanismo disponível no mOS"; }
int sasl_client_init(const void *cb) { (void)cb; return 0; }
int sasl_client_done(void) { return 0; }
int sasl_client_new(const char *svc, const char *host, const char *ipl, const char *ipr, const void *cb, unsigned flags, void **conn) {
    (void)svc; (void)host; (void)ipl; (void)ipr; (void)cb; (void)flags;
    if (!conn) return -7;
    *conn = calloc(1, 16);
    return *conn ? 0 : -2;
}
int sasl_client_start(void *conn, const char *mechs, void **prompt, const char **out, unsigned *outlen, const char **mech) {
    (void)conn; (void)mechs; (void)prompt;
    if (out) *out = 0;
    if (outlen) *outlen = 0;
    if (mech) *mech = 0;
    return -4;                                                           /* SASL_NOMECH */
}
int sasl_client_step(void *conn, const char *in, unsigned inlen, void **prompt, const char **out, unsigned *outlen) {
    (void)conn; (void)in; (void)inlen; (void)prompt;
    if (out) *out = 0;
    if (outlen) *outlen = 0;
    return -1;
}

/* ================================================================ libpam: só a conta (getpwnam); autenticar fica com quem já faz */
void *getpwnam(const char *n);
typedef struct { char *svc, *user; } c2pam_t;
int pam_start(const char *svc, const char *user, const void *conv, void **h) {
    (void)conv;
    if (!h || !svc) return 4;                                            /* PAM_SYSTEM_ERR */
    c2pam_t *p = calloc(1, sizeof *p);
    if (!p) return 5;
    p->svc = strdup(svc);
    p->user = user ? strdup(user) : 0;
    *h = p;
    return 0;
}
int pam_end(void *h, int st) { (void)st; c2pam_t *p = h; if (!p) return 4; free(p->svc); free(p->user); free(p); return 0; }
int pam_acct_mgmt(void *h, int flags) {
    (void)flags;
    c2pam_t *p = h;
    if (!p) return 4;                                                    /* PAM_SYSTEM_ERR */
    return p->user && getpwnam(p->user) ? 0 : 10;                         /* PAM_USER_UNKNOWN */
}
const char *pam_strerror(void *h, int e) {
    (void)h;
    static const char *const m[] = {"Success", "Failed to load module", "Symbol not found", "Error in service module",
                                    "System error", "Memory buffer error", "Permission denied", "Authentication failure",
                                    "Insufficient credentials to access authentication data", "Authentication service cannot retrieve authentication info",
                                    "User not known to the underlying authentication module", "Have exhausted maximum number of retries for service"};
    return e >= 0 && e < (int)(sizeof m / sizeof m[0]) ? m[e] : "Unknown PAM error";
}

/* ================================================================ constantes de mídia e de frameworks pequenos */
KS(kCMTextMarkupAttribute_ForegroundColorARGB, "ForegroundColorARGB")
KS(kCMTextMarkupAttribute_BackgroundColorARGB, "BackgroundColorARGB")
KS(kCMTextMarkupAttribute_CharacterBackgroundColorARGB, "CharacterBackgroundColorARGB")
KS(kCMTextMarkupAttribute_BoldStyle, "BoldStyle") KS(kCMTextMarkupAttribute_ItalicStyle, "ItalicStyle")
KS(kCMTextMarkupAttribute_UnderlineStyle, "UnderlineStyle") KS(kCMTextMarkupAttribute_FontFamilyName, "FontFamilyName")
KS(kCMTextMarkupAttribute_GenericFontFamilyName, "GenericFontFamilyName")
KS(kCMTextMarkupAttribute_BaseFontSizePercentageRelativeToVideoHeight, "BaseFontSizePercentageRelativeToVideoHeight")
KS(kCMTextMarkupAttribute_RelativeFontSize, "RelativeFontSize") KS(kCMTextMarkupAttribute_VerticalLayout, "VerticalLayout")
KS(kCMTextMarkupAttribute_Alignment, "Alignment")
KS(kCMTextMarkupAttribute_TextPositionPercentageRelativeToWritingDirection, "TextPositionPercentageRelativeToWritingDirection")
KS(kCMTextMarkupAttribute_OrthogonalLinePositionPercentageRelativeToWritingDirection, "OrthogonalLinePositionPercentageRelativeToWritingDirection")
KS(kCMTextMarkupGenericFontName_Default, "default") KS(kCMTextMarkupGenericFontName_Serif, "serif")
KS(kCMTextMarkupGenericFontName_SansSerif, "sans-serif") KS(kCMTextMarkupGenericFontName_Monospace, "monospace")
KS(kCMTextMarkupGenericFontName_ProportionalSerif, "proportional-serif")
KS(kCMTextMarkupGenericFontName_ProportionalSansSerif, "proportional-sans-serif")
KS(kCMTextMarkupGenericFontName_MonospaceSerif, "monospace-serif")
KS(kCMTextMarkupGenericFontName_MonospaceSansSerif, "monospace-sans-serif")
KS(kCMTextMarkupGenericFontName_Casual, "casual") KS(kCMTextMarkupGenericFontName_Cursive, "cursive")
KS(kCMTextMarkupGenericFontName_Fantasy, "fantasy") KS(kCMTextMarkupGenericFontName_SmallCapital, "small-capital")
KS(kCMTextMarkupAlignmentType_Start, "start") KS(kCMTextMarkupAlignmentType_Middle, "middle")
KS(kCMTextMarkupAlignmentType_End, "end") KS(kCMTextMarkupAlignmentType_Left, "left") KS(kCMTextMarkupAlignmentType_Right, "right")
KS(kCMTextVerticalLayout_LeftToRight, "vertical-lr") KS(kCMTextVerticalLayout_RightToLeft, "vertical-rl")
KS(AVVideoYCbCrMatrix_ITU_R_601_4, "ITU_R_601_4") KS(AVVideoColorPrimaries_SMPTE_C, "SMPTE_C")
KS(AVVideoProfileLevelH264Main41, "H264_Main_4_1") KS(AVVideoProfileLevelH264Main32, "H264_Main_3_2")
KS(AVVideoProfileLevelH264Main31, "H264_Main_3_1") KS(AVVideoProfileLevelH264Main30, "H264_Main_3_0")
KS(AVVideoProfileLevelH264Baseline31, "H264_Baseline_3_1") KS(AVVideoProfileLevelH264Baseline30, "H264_Baseline_3_0")
KS(AVVideoCodecHEVC, "hvc1")
KS(AVMetadataQuickTimeMetadataKeyInformation, "com.apple.quicktime.information")
KS(AVMetadataQuickTimeMetadataKeyDescription, "com.apple.quicktime.description")
KS(AVMetadataQuickTimeMetadataKeyCopyright, "com.apple.quicktime.copyright")
KS(AVMetadataQuickTimeMetadataKeyArtwork, "com.apple.quicktime.artwork")
KS(AVMetadataCommonKeySoftware, "software") KS(AVMediaTypeClosedCaption, "clcp")
KS(AVMediaCharacteristicContainsOnlyForcedSubtitles, "public.subtitles.forced-only")
KS(AVFileTypeProfileMPEG4AppleHLS, "MPEG4AppleHLS")
KS(AVSampleRateConverterAlgorithm_Mastering, "AVSampleRateConverterAlgorithm_Mastering")
KS(NSFileProviderWorkingSetContainerItemIdentifier, "NSFileProviderWorkingSetContainerItemIdentifier")
KS(NSFileProviderTrashContainerItemIdentifier, "NSFileProviderTrashContainerItemIdentifier")
KS(NSFileProviderRootContainerItemIdentifier, "NSFileProviderRootContainerItemIdentifier")
KS(NSFileProviderErrorDomain, "NSFileProviderErrorDomain") KS(NSFileProviderDomainDidChange, "NSFileProviderDomainDidChange")
KS(PHLivePhotoInfoIsDegradedKey, "PHLivePhotoInfoIsDegradedKey") KS(PHLivePhotoInfoCancelledKey, "PHLivePhotoInfoCancelledKey")
KS(NEVPNConnectionErrorDomain, "NEVPNConnectionErrorDomain") KS(MLModelCreatorDefinedKey, "MLModelCreatorDefinedKey")
KS(kColorSyncProfileURL, "com.apple.ColorSync.ProfileURL")
const char *const vmnet_nat66_prefix_key = "nat66_prefix";

/* ================================================================ IOReport: o mOS não publica canais */
static id ior_channels(id arr) {
    id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", arr ? arr : cx_arr_new(0), cx_str("IOReportChannels"));
    return objc_retain(d);
}
id IOReportCopyChannelsInGroup(id group, id subgroup, u64 a, u64 b, u64 c) { (void)group; (void)subgroup; (void)a; (void)b; (void)c; return ior_channels(0); }
void IOReportMergeChannels(id a, id b, id unused) {
    (void)unused;
    if (!a || !b) return;
    id ca = MSG(id, a, "objectForKey:", cx_str("IOReportChannels")), cb = MSG(id, b, "objectForKey:", cx_str("IOReportChannels"));
    if (cb && MSG0(u64, cb, "count")) {
        id m = ca ? MSG0(id, ca, "mutableCopy") : cx_arr_new(0);
        MSG(void, m, "addObjectsFromArray:", cb);
        MSG(void, a, "setObject:forKey:", m, cx_str("IOReportChannels"));
    }
}
void *IOReportCreateSubscription(void *a, id channels, id *subbed, u64 b, id c) {
    (void)a; (void)b; (void)c;
    if (subbed) *subbed = channels ? MSG0(id, channels, "mutableCopy") : ior_channels(0);
    return ior_channels(0);                                              /* um objeto CF qualquer: CFRelease funciona */
}
id IOReportCreateSamples(void *sub, id channels, id unused) { (void)sub; (void)channels; (void)unused; return ior_channels(0); }
id IOReportCreateSamplesDelta(id a, id b, id unused) { (void)a; (void)b; (void)unused; return ior_channels(0); }
id IOReportChannelGetGroup(id ch) { (void)ch; return cx_str(""); }
id IOReportChannelGetSubGroup(id ch) { (void)ch; return cx_str(""); }
id IOReportChannelGetChannelName(id ch) { (void)ch; return cx_str(""); }
id IOReportChannelGetUnitLabel(id ch) { (void)ch; return cx_str(""); }
int IOReportStateGetCount(id ch) { (void)ch; return 0; }
i64 IOReportStateGetResidency(id ch, int i) { (void)ch; (void)i; return 0; }
id IOReportStateGetNameForIndex(id ch, int i) { (void)ch; (void)i; return 0; }

/* ================================================================ MultitouchSupport: sem trackpad multitoque */
id MTDeviceCreateList(void) { return CFArrayCreate(0, 0, 0, kCFTypeArrayCallBacks); }
void MTRegisterContactFrameCallback(void *dev, void *cb) { (void)dev; (void)cb; }
void MTUnregisterContactFrameCallback(void *dev, void *cb) { (void)dev; (void)cb; }
int MTDeviceStart(void *dev, int mode) { (void)dev; (void)mode; return dev ? 0 : -1; }
int MTDeviceStop(void *dev) { (void)dev; return 0; }

/* ================================================================ avulsos */
int ldap_unbind(void *ld) { (void)ld; return 0; }
int ldap_result(void *ld, int msgid, int all, void *timeout, void **res) { (void)ld; (void)msgid; (void)all; (void)timeout; if (res) *res = 0; return -1; }
int ldap_count_entries(void *ld, void *res) { (void)ld; (void)res; return -1; }
int _SLPSSetFrontProcessWithOptions(const void *psn, u32 wid, u32 mode) { (void)psn; (void)wid; (void)mode; return 0; }
int SLPSPostEventRecordTo(const void *psn, const u8 *bytes) { (void)psn; (void)bytes; return 0; }
i32 hv_vcpu_read_fpstate(u32 vcpu, void *buf, size_t n) { (void)vcpu; (void)buf; (void)n; return (i32)0xfae9400f; }
i32 hv_vcpu_write_fpstate(u32 vcpu, const void *buf, size_t n) { (void)vcpu; (void)buf; (void)n; return (i32)0xfae9400f; }
int pm_sample_task(u32 task, void *out, u64 now, u32 flags) { (void)task; (void)now; (void)flags; if (out) memset(out, 0, 64); return 5; }   /* KERN_FAILURE */
double pm_energy_impact(const void *sample) { (void)sample; return 0.0; }
void exsltRegisterAll(void) {}
i32 SCardControl132(i32 card, u32 code, const void *in, u32 inlen, void *out, u32 outlen, u32 *ret) {
    (void)card; (void)code; (void)in; (void)inlen; (void)out; (void)outlen;
    if (ret) *ret = 0;
    return (i32)0x80100003;                                              /* SCARD_E_INVALID_HANDLE */
}
int MACaptionAppearanceGetDisplayType(int domain) { (void)domain; return 0; }   /* kMACaptionAppearanceDisplayTypeForcedOnly */
/* "00-1a-7d-da-71-13" ou com ':' -> BluetoothDeviceAddress (6 bytes) */
int IOBluetoothNSStringToDeviceAddress(id s, u8 *addr) {
    const char *p = s && cx_is_str(s) ? cx_cstr(s) : 0;
    if (!p || !addr) return (int)0xe00002c2;                             /* kIOReturnBadArgument */
    for (int i = 0; i < 6; i++) {
        int v = 0;
        for (int k = 0; k < 2; k++, p++) {
            char c = *p;
            int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (d < 0) return (int)0xe00002c2;
            v = v * 16 + d;
        }
        addr[i] = (u8)v;
        if (i < 5) { if (*p != '-' && *p != ':') return (int)0xe00002c2; p++; }
    }
    return *p ? (int)0xe00002c2 : 0;
}

/* ================================================================ libxslt: sem processador XSLT (as transformações falham com erro) */
static void xslt_default_err(void *ctx, const char *msg, ...) {
    (void)ctx;
    if (msg) write(2, msg, strlen(msg));
}
void (*xsltGenericError)(void *, const char *, ...) = xslt_default_err;
void *xsltGenericErrorContext;
static void xslt_fail(const char *what) {
    if (xsltGenericError) xsltGenericError(xsltGenericErrorContext, "libxslt: %s indisponível no mOS (sem processador XSLT)\n", what);
}
void *xsltNewSecurityPrefs(void) { return calloc(1, 64); }
void xsltFreeSecurityPrefs(void *p) { free(p); }
int xsltSetSecurityPrefs(void *p, int option, void *fn) { (void)option; (void)fn; return p ? 0 : -1; }
int xsltSetCtxtSecurityPrefs(void *p, void *ctxt) { (void)p; (void)ctxt; return 0; }
int xsltSecurityForbid(void *p, void *ctxt, const char *value) { (void)p; (void)ctxt; (void)value; return 0; }
int xsltSecurityAllow(void *p, void *ctxt, const char *value) { (void)p; (void)ctxt; (void)value; return 1; }
void *xsltParseStylesheetDoc(void *doc) { (void)doc; xslt_fail("xsltParseStylesheetDoc"); return 0; }
void xsltFreeStylesheet(void *style) { (void)style; }
void *xsltNewTransformContext(void *style, void *doc) { (void)doc; return style ? calloc(1, 64) : 0; }
void xsltFreeTransformContext(void *ctxt) { free(ctxt); }
void *xsltApplyStylesheet(void *style, void *doc, const char **params) { (void)style; (void)doc; (void)params; xslt_fail("xsltApplyStylesheet"); return 0; }
void *xsltApplyStylesheetUser(void *style, void *doc, const char **params, const char *output, void *profile, void *ctxt) {
    (void)style; (void)doc; (void)params; (void)output; (void)profile; (void)ctxt;
    xslt_fail("xsltApplyStylesheetUser");
    return 0;
}
int xsltSaveResultToString(u8 **out, int *len, void *result, void *style) { (void)result; (void)style; if (out) *out = 0; if (len) *len = 0; return -1; }
int xsltSaveResultTo(void *buf, void *result, void *style) { (void)buf; (void)result; (void)style; return -1; }
