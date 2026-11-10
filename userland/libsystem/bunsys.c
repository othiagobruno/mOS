/*
 * mOS libSystem — o que faltava do libSystem.B.dylib para o Claude Code (o binário único do Bun com o JavaScriptCore;
 * inventário do tools/machocheck.c no ClaudeCode.app, 20 funções). Como no vscode_libc.c: cada função faz o que o macOS
 * faz quando dá para fazer aqui, e o que o mOS não tem (cwd por thread, DNS-SD, clonefile) devolve o erro que o macOS
 * devolve num ambiente restrito, que é o caminho que o Bun já trata.
 */
#include "sys.h"
#include "libc.h"

typedef int kern_return_t;
typedef u32 mach_port_t;
#define KERN_SUCCESS 0
#define E_NOTSUP 45
#define E_NOSYS 78
#define E_INVAL 22
#define E_BADF 9

int statfs(const char *path, void *s);
int fchdir(int fd);
int chdir(const char *path);
char *getcwd(char *buf, size_t n);
off_t lseek(int fd, off_t off, int whence);
unsigned int sleep(unsigned int s);
u64 mach_absolute_time(void);
u64 mach_continuous_time(void);
int __ulock_wait(u32 op, void *addr, u64 value, u32 timeout_us);
static void set_errno(int e) { cx_errno = e; }

/* ================================================================ memória e texto */
void memset_pattern4(void *b, const void *pattern4, size_t len) {
    u8 *d = b; const u8 *p = pattern4;
    for (size_t i = 0; i < len; i++) d[i] = p[i & 3];
}
void memset_pattern8(void *b, const void *pattern8, size_t len) {
    u8 *d = b; const u8 *p = pattern8;
    for (size_t i = 0; i < len; i++) d[i] = p[i & 7];
}
/* wchar_t do macOS: 4 bytes */
u32 *wmemmove(u32 *d, const u32 *s, size_t n) { memmove(d, s, n * 4); return d; }

/* ================================================================ rede */
u16 ntohs(u16 x) { return (u16)((x >> 8) | (x << 8)); }
/* inet_aton: "a.b.c.d" (também a, a.b e a.b.c, em decimal, octal ou hexa, como o BSD); 1 = válido */
int inet_aton(const char *cp, u32 *addr) {
    u32 part[4]; int n = 0;
    const char *s = cp;
    if (!s || !*s) return 0;
    for (;;) {
        u64 v = 0; int base = 10, digits = 0;
        if (*s == '0') { base = 8; s++; digits = 1; if (*s == 'x' || *s == 'X') { base = 16; s++; digits = 0; } }
        for (;; s++) {
            int c = *s, d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (base == 16 && c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (base == 16 && c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else break;
            if (d >= base) return 0;
            v = v * (u64)base + (u64)d;
            if (v > 0xFFFFFFFFull) return 0;
            digits++;
        }
        if (!digits || n == 4) return 0;
        part[n++] = (u32)v;
        if (*s == '.') { s++; continue; }
        if (*s && *s != ' ' && *s != '\t' && *s != '\n') return 0;
        break;
    }
    u32 r;
    switch (n) {
    case 1: r = part[0]; break;
    case 2: if (part[0] > 255 || part[1] > 0xFFFFFF) return 0; r = part[0] << 24 | part[1]; break;
    case 3: if (part[0] > 255 || part[1] > 255 || part[2] > 0xFFFF) return 0; r = part[0] << 24 | part[1] << 16 | part[2]; break;
    default:
        for (int i = 0; i < 4; i++) if (part[i] > 255) return 0;
        r = part[0] << 24 | part[1] << 16 | part[2] << 8 | part[3];
    }
    if (addr) *addr = __builtin_bswap32(r);     /* ordem de rede */
    return 1;
}

/* DNS-SD (dns_sd.h): o mOS não tem o mDNSResponder; como no macOS com o serviço parado, devolve
 * kDNSServiceErr_ServiceNotRunning, e o Bun resolve pelo getaddrinfo */
#define kDNSServiceErr_ServiceNotRunning (-65563)
int DNSServiceCreateConnection(void **sd) { if (sd) *sd = 0; return kDNSServiceErr_ServiceNotRunning; }
int DNSServiceQueryRecord(void **sd, u32 flags, u32 iface, const char *name, u16 rrtype, u16 rrclass, void *cb, void *ctx) {
    (void)flags; (void)iface; (void)name; (void)rrtype; (void)rrclass; (void)cb; (void)ctx;
    if (sd) *sd = 0;
    return kDNSServiceErr_ServiceNotRunning;
}
int DNSServiceRefSockFD(void *sd) { (void)sd; return -1; }
int DNSServiceProcessResult(void *sd) { (void)sd; return kDNSServiceErr_ServiceNotRunning; }
void DNSServiceRefDeallocate(void *sd) { (void)sd; }
int DNSServiceRegister(void **sd, u32 flags, u32 iface, const char *name, const char *regtype, const char *domain,
                       const char *host, u16 port, u16 txtLen, const void *txt, void *cb, void *ctx) {
    (void)flags; (void)iface; (void)name; (void)regtype; (void)domain; (void)host; (void)port; (void)txtLen; (void)txt; (void)cb; (void)ctx;
    if (sd) *sd = 0;
    return kDNSServiceErr_ServiceNotRunning;
}

/* ================================================================ arquivos */
int statfs64(const char *path, void *s) { return statfs(path, s); }   /* no x86_64 o statfs já é o de 64 bits */
/* clonefileat: o VFS do mOS não tem cópia por referência; ENOTSUP leva o Bun ao copyfile/leitura e escrita */
int clonefileat(int sfd, const char *src, int dfd, const char *dst, u32 flags) {
    (void)sfd; (void)src; (void)dfd; (void)dst; (void)flags;
    set_errno(E_NOTSUP);
    return -1;
}
/* strtofflags(3): "uchg,hidden,..." para os bits de chflags; o mOS não guarda flags, mas os nomes conhecidos passam */
int strtofflags(char **stringp, u32 *setp, u32 *clrp) {
    static const struct { const char *name; u32 bit; } fl[] = {
        {"arch", 0x10000}, {"archived", 0x10000}, {"nodump", 0x1}, {"opaque", 0x8}, {"sappnd", 0x40000},
        {"sappend", 0x40000}, {"schg", 0x20000}, {"schange", 0x20000}, {"simmutable", 0x20000}, {"uappnd", 0x4},
        {"uappend", 0x4}, {"uchg", 0x2}, {"uchange", 0x2}, {"uimmutable", 0x2}, {"hidden", 0x8000},
    };
    if (setp) *setp = 0;
    if (clrp) *clrp = 0;
    char *s = stringp ? *stringp : 0;
    while (s && *s) {
        while (*s == ',' || *s == ' ' || *s == '\t') s++;
        if (!*s) break;
        char *e = s;
        while (*e && *e != ',' && *e != ' ' && *e != '\t') e++;
        size_t n = (size_t)(e - s);
        int neg = n > 2 && s[0] == 'n' && s[1] == 'o', found = 0;
        for (unsigned i = 0; i < sizeof fl / sizeof fl[0]; i++) {
            const char *w = fl[i].name;
            if (strlen(w) == n - (neg ? 2 : 0) && !strncmp(s + (neg ? 2 : 0), w, n - (neg ? 2 : 0))) {
                if (neg) { if (clrp) *clrp |= fl[i].bit; } else if (setp) *setp |= fl[i].bit;
                found = 1;
                break;
            }
        }
        if (!found) { *stringp = s; return 1; }
        s = e;
    }
    return 0;
}

/* __getdirentries64(fd, buf, nbytes, *basep): o readdir do Bun lê a pasta direto do descritor. Registros struct dirent de
 * 64 bits (d_ino, d_seekoff, d_reclen, d_namlen, d_type, d_name), cada um com o tamanho arredondado para 8 bytes, como
 * o DIRENT64_LEN do kernel do macOS. A posição do descritor (lseek) guarda o índice da próxima entrada; 0 = fim. */
typedef struct __attribute__((packed)) { u64 d_ino, d_seekoff; u16 d_reclen, d_namlen; u8 d_type; char d_name[]; } bun_dirent_t;
ssize_t __getdirentries64(int fd, char *buf, size_t nbytes, off_t *basep) {
    char path[1024], name[1024];
    if (SYS3(SYS_FDPATH, fd, path, sizeof path) < 0) { set_errno(E_BADF); return -1; }
    off_t idx = lseek(fd, 0, 1 /* SEEK_CUR */);
    if (idx < 0) idx = 0;
    if (basep) *basep = idx;
    size_t used = 0;
    for (;;) {
        if (SYS4(SYS_READDIR, path, (long)idx, name, sizeof name) != 1) break;
        size_t nl = strlen(name), rec = (sizeof(bun_dirent_t) + nl + 1 + 7) & ~(size_t)7;
        if (used + rec > nbytes) {
            if (!used) { set_errno(E_INVAL); return -1; }   /* nem um registro cabe */
            break;
        }
        char full[2048];
        size_t pl = strlen(path);
        memcpy(full, path, pl);
        if (pl && full[pl - 1] != '/') full[pl++] = '/';
        memcpy(full + pl, name, nl + 1);
        mos_stat_t st = {0};
        SYS2(SYS_STAT, full, &st);
        bun_dirent_t *e = (bun_dirent_t *)(buf + used);
        memset(e, 0, rec);
        e->d_ino = (u64)idx + 2;                  /* nunca 0: o readdir do macOS pula entradas com d_ino 0 */
        e->d_seekoff = (u64)idx + 1;
        e->d_reclen = (u16)rec;
        e->d_namlen = (u16)nl;
        e->d_type = st.is_dir ? 4 /* DT_DIR */ : 8 /* DT_REG */;
        memcpy(e->d_name, name, nl + 1);
        used += rec;
        idx++;
    }
    lseek(fd, idx, 0 /* SEEK_SET */);
    return (ssize_t)used;
}

/* __pthread_fchdir: no macOS, muda a pasta só da thread (o Bun a usa em volta do posix_spawn, para o filho nascer na
 * pasta pedida, e desfaz com -1). O mOS não tem pasta por thread: muda a do processo e guarda a anterior, e o -1 volta
 * para ela. */
static char saved_cwd[1024];
static int have_saved;
int __pthread_fchdir(int fd) {
    if (fd == -1) {
        if (!have_saved) return 0;
        have_saved = 0;
        return chdir(saved_cwd);
    }
    if (!have_saved && getcwd(saved_cwd, sizeof saved_cwd)) have_saved = 1;
    return fchdir(fd);
}

/* ================================================================ tempo, ulock e Mach */
u64 mach_approximate_time(void) { return mach_absolute_time(); }
u64 mach_continuous_approximate_time(void) { return mach_continuous_time(); }
/* __ulock_wait2(op, addr, valor, timeout_ns, valor2): o __ulock_wait com prazo em ns (0 = sem prazo) */
int __ulock_wait2(u32 op, void *addr, u64 value, u64 timeout_ns, u64 value2) {
    (void)value2;
    u64 us = timeout_ns / 1000;
    if (timeout_ns && !us) us = 1;
    return __ulock_wait(op, addr, value, us > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)us);
}
/* vm_inherit: o fork do mOS copia (ou compartilha em COW) o espaço todo; o pedido de herança não muda nada */
kern_return_t vm_inherit(mach_port_t task, u64 addr, u64 size, u32 inherit) {
    (void)task; (void)addr; (void)size; (void)inherit;
    return KERN_SUCCESS;
}
/* mach_msg_server_once: o servidor de exceções Mach de uma thread de diagnóstico. O mOS não entrega exceções por porta, então
 * nenhuma mensagem chega: a thread fica esperando, como no macOS sem falhas. */
kern_return_t mach_msg_server_once(void *demux, u32 max_size, mach_port_t rcv_name, u32 options) {
    (void)demux; (void)max_size; (void)rcv_name; (void)options;
    for (;;) sleep(3600);
}
