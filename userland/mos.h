/* mOS — interface mínima para programas de usuário (ring 3). Syscalls via int 0x80:
 * número em rax, argumentos em rdi, rsi, rdx; resultado em rax. Programas são ELF64 estáticos
 * ligados em 0x400000000000 (build.ps1 compila userland\*.c para \mOS\Sistema\Programas\). */
#pragma once
typedef unsigned long long u64;
typedef long long i64;
typedef unsigned long usize;
#include "syscall.h"     /* kernel/include/syscall.h: números, flags O_*, errno */

static inline i64 sys3(u64 n, u64 a, u64 b, u64 c) {
    i64 r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c) : "memory", "rcx", "r8", "r9", "r10", "r11");
    return r;
}
static inline i64 sys4(u64 n, u64 a, u64 b, u64 c, u64 d) {
    i64 r;
    register u64 r10 __asm__("r10") = d;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10) : "memory", "rcx", "r8", "r9", "r11");
    return r;
}

static inline __attribute__((noreturn)) void exit(int code) { sys3(SYS_EXIT, (u64)code, 0, 0); for (;;) {} }
static inline usize strlen(const char *s) { usize n = 0; while (s[n]) n++; return n; }
/* escreve tudo (pipes podem aceitar só parte de cada vez) */
static inline i64 write(int fd, const void *b, usize n) {
    usize done = 0;
    while (done < n) {
        i64 k = sys3(SYS_WRITE, fd, (u64)b + done, n - done);
        if (k <= 0) return done ? (i64)done : k;
        done += (usize)k;
    }
    return (i64)done;
}
static inline void puts(const char *s) { write(1, s, strlen(s)); }
static inline int getpid(void) { return (int)sys3(SYS_GETPID, 0, 0, 0); }
static inline void sleep_ms(u64 ms) { sys3(SYS_SLEEP, ms, 0, 0); }
static inline void yield(void) { sys3(SYS_YIELD, 0, 0, 0); }
static inline u64 time_ms(void) { return (u64)sys3(SYS_TIME, 0, 0, 0); }
static inline u64 brk(u64 a) { return (u64)sys3(SYS_BRK, a, 0, 0); }
static inline i64 read_file(const char *p, void *b, usize max) { return sys3(SYS_READFILE, (u64)p, (u64)b, max); }
static inline i64 write_file(const char *p, const void *b, usize n) { return sys3(SYS_WRITEFILE, (u64)p, (u64)b, n); }
static inline i64 file_size(const char *p) { return sys3(SYS_FILESIZE, (u64)p, 0, 0); }
static inline int spawn(const char *p, const char *arg) { return (int)sys3(SYS_SPAWN, (u64)p, (u64)arg, 0); }
static inline int wait(int pid) { return (int)sys3(SYS_WAIT, (u64)pid, 0, 0); }
static inline int cpus(void) { return (int)sys3(SYS_CPUS, 0, 0, 0); }
static inline int kill(int pid) { return (int)sys3(SYS_KILL, (u64)pid, 0, 0); }
static inline int open(const char *p, int flags) { return (int)sys3(SYS_OPEN, (u64)p, (u64)flags, 0); }
static inline int close(int fd) { return (int)sys3(SYS_CLOSE, (u64)fd, 0, 0); }
static inline i64 read(int fd, void *b, usize n) { return sys3(SYS_READ, (u64)fd, (u64)b, n); }
static inline i64 lseek(int fd, i64 off, int whence) { return sys3(SYS_LSEEK, (u64)fd, (u64)off, (u64)whence); }
static inline int fstat(int fd, mos_stat_t *st) { return (int)sys3(SYS_FSTAT, (u64)fd, (u64)st, 0); }
static inline int stat(const char *p, mos_stat_t *st) { return (int)sys3(SYS_STAT, (u64)p, (u64)st, 0); }
static inline void *mmap(usize len) { i64 r = sys3(SYS_MMAP, len, 0, 0); return r < 0 ? 0 : (void *)r; }
static inline int munmap(void *a, usize len) { return (int)sys3(SYS_MUNMAP, (u64)a, len, 0); }
static inline i64 unixtime(void) { return sys3(SYS_UNIXTIME, 0, 0, 0); }
static inline int readdir(const char *p, int i, char *name, usize cap) { return (int)sys4(SYS_READDIR, (u64)p, (u64)i, (u64)name, cap); }
static inline int mkdir(const char *p) { return (int)sys3(SYS_MKDIR, (u64)p, 0, 0); }
static inline int unlink(const char *p) { return (int)sys3(SYS_UNLINK, (u64)p, 0, 0); }
static inline int rename(const char *a, const char *b) { return (int)sys3(SYS_RENAME, (u64)a, (u64)b, 0); }
static inline int waitany(int *status) { return (int)sys3(SYS_WAITANY, (u64)status, 0, 0); }
static inline int chdir(const char *p) { return (int)sys3(SYS_CHDIR, (u64)p, 0, 0); }
static inline i64 getcwd(char *b, usize cap) { return sys3(SYS_GETCWD, (u64)b, cap, 0); }
static inline int proclist(int i, mos_procinfo_t *pi) { return (int)sys3(SYS_PROCLIST, (u64)i, (u64)pi, 0); }
static inline int pipe(int fds[2]) { return (int)sys3(SYS_PIPE, (u64)fds, 0, 0); }
static inline int spawnio(const char *p, const char *arg, const int io[3]) { return (int)sys3(SYS_SPAWNIO, (u64)p, (u64)arg, (u64)io); }
static inline int dup2(int fd, int nfd) { return (int)sys3(SYS_DUP2, (u64)fd, (u64)nfd, 0); }
static inline int fork(void) { return (int)sys3(SYS_FORK, 0, 0, 0); }
static inline int gettid(void) { return (int)sys3(SYS_GETTID, 0, 0, 0); }
static inline __attribute__((noreturn)) void thread_exit(int code) { sys3(SYS_THREAD_EXIT, (u64)code, 0, 0); for (;;) {} }
static inline int thread_join(int tid) { return (int)sys3(SYS_THREAD_JOIN, (u64)tid, 0, 0); }
static inline int sigkill(int pid, int sig) { return (int)sys3(SYS_SIGKILL, (u64)pid, (u64)sig, 0); }

/* thread nova: fn(arg) roda numa pilha própria de 64 KiB; o retorno de fn vira o código de saída */
typedef int (*thread_fn)(void *);
static void thread_trampoline(void **pair) { thread_exit(((thread_fn)pair[0])(pair[1])); }   /* rdi = {fn, arg} */
static inline int thread_create(thread_fn fn, void *arg) {
    unsigned char *stack = (unsigned char *)sys3(SYS_MMAP, 65536, 0, 0);
    if ((i64)stack < 0) return -1;
    u64 *sp = (u64 *)(stack + 65536 - 32);
    sp[0] = 0;                                          /* "endereço de retorno" (o trampolim nunca volta) */
    sp[1] = (u64)fn; sp[2] = (u64)arg;
    return (int)sys3(SYS_THREAD_CREATE, (u64)thread_trampoline, (u64)sp, (u64)(sp + 1));
}

/* sinais: o "restorer" volta do tratador para onde o programa estava */
__attribute__((naked, used)) static void sig_restorer(void) {
    __asm__ volatile("mov %0, %%eax\n\tint $0x80\n\tud2" :: "i"(SYS_SIGRETURN));
}
typedef void (*sig_handler_t)(int);
static inline i64 signal(int sig, sig_handler_t h) { return sys3(SYS_SIGACTION, (u64)sig, (u64)h, (u64)sig_restorer); }

static inline int strcmp(const char *a, const char *b) { while (*a && *a == *b) a++, b++; return (unsigned char)*a - (unsigned char)*b; }
static inline int strncmp(const char *a, const char *b, usize n) { for (; n && *a && *a == *b; n--) a++, b++; return n ? (unsigned char)*a - (unsigned char)*b : 0; }
static inline char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) {} return r; }
static inline char *strchr(const char *s, int c) { for (; *s; s++) if (*s == (char)c) return (char *)s; return c ? 0 : (char *)s; }
static inline int atoi(const char *s) { int n = 0, neg = *s == '-'; if (neg) s++; while (*s >= '0' && *s <= '9') n = n * 10 + *s++ - '0'; return neg ? -n : n; }

/* malloc simples sobre mmap (blocos com cabeçalho de tamanho; free devolve à lista) */
typedef struct mblk { usize size; struct mblk *next; } mblk_t;
static mblk_t *m_free;
static void *malloc(usize n) {
    n = (n + 15) & ~(usize)15;
    for (mblk_t **pp = &m_free; *pp; pp = &(*pp)->next)
        if ((*pp)->size >= n) { mblk_t *b = *pp; *pp = b->next; return b + 1; }
    usize total = n + sizeof(mblk_t);
    mblk_t *b = (mblk_t *)mmap(total < 65536 ? 65536 : total);
    if (!b) return 0;
    b->size = (total < 65536 ? 65536 : total) - sizeof(mblk_t);
    return b + 1;
}
static void free(void *p) { if (!p) return; mblk_t *b = (mblk_t *)p - 1; b->next = m_free; m_free = b; }

/* lê uma linha de stdin (sem o '\n'); devolve o tamanho ou -1 no fim da entrada */
static i64 readline(char *buf, usize cap) {
    usize n = 0;
    while (n + 1 < cap) {
        i64 k = read(0, buf + n, cap - 1 - n);
        if (k <= 0) { if (!n) return -1; break; }
        n += (usize)k;
        if (buf[n - 1] == '\n') { n--; break; }
    }
    buf[n] = 0;
    return (i64)n;
}

/* o compilador pode gerar chamadas a estas */
void *memset(void *d, int c, usize n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *s, usize n) { unsigned char *a = d; const unsigned char *b = s; while (n--) *a++ = *b++; return d; }
int memcmp(const void *x, const void *y, usize n) {
    const unsigned char *a = x, *b = y;
    for (; n; n--, a++, b++) if (*a != *b) return *a - *b;
    return 0;
}

/* formatação simples: %d (i64) %u %x (u64) %s %c %%, com largura e '-' (ex.: %5d, %-9s) */
static void print(const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    char out[1024];
    usize o = 0;
    for (const char *f = fmt; *f && o < sizeof out - 64; f++) {
        if (*f != '%') { out[o++] = *f; continue; }
        f++;
        int left = 0, width = 0;
        if (*f == '-') { left = 1; f++; }
        while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0');
        char tmp[64];
        int n = 0;
        if (*f == '%') { out[o++] = '%'; continue; }
        if (*f == 's') {
            const char *s = __builtin_va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (s[n] && n < 63) { tmp[n] = s[n]; n++; }
        } else if (*f == 'c') {
            tmp[n++] = (char)__builtin_va_arg(ap, int);
        } else {
            u64 v;
            int neg = 0, base = *f == 'x' ? 16 : 10;
            if (*f == 'd') { i64 s = __builtin_va_arg(ap, i64); if (s < 0) { neg = 1; v = (u64)-s; } else v = (u64)s; }
            else v = __builtin_va_arg(ap, u64);
            char rev[24];
            int k = 0;
            do { rev[k++] = "0123456789abcdef"[v % base]; v /= base; } while (v);
            if (neg) tmp[n++] = '-';
            while (k) tmp[n++] = rev[--k];
        }
        int pad = width > n ? width - n : 0;
        if (!left) while (pad-- > 0 && o < sizeof out - 2) out[o++] = ' ';
        for (int i = 0; i < n && o < sizeof out - 2; i++) out[o++] = tmp[i];
        if (left) while (pad-- > 0 && o < sizeof out - 2) out[o++] = ' ';
    }
    __builtin_va_end(ap);
    write(1, out, o);
}
