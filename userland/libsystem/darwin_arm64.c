/*
 * mOS libSystem arm64 — o que um processo arm64 vê do "hardware": as chaves de sysctl do Apple Silicon coerentes com o
 * que o tradutor implementa (A64RT_SYSCTL_INTS/STRS em userland/a64rt/a64rt_abi.h e A64_FEATURES em userland/a64/a64.h:
 * hw.optional.arm.FEAT_* = 1 só para o que existe). darwin.c consulta esta tabela antes da sua (a do x86).
 */
#if defined(__aarch64__)
#include "../a64/a64.h"
#include "../a64rt/a64rt_abi.h"

typedef struct { const char *name; long long num; const char *str; } a64_sysctl_t;
#define I(n, v) {n, (long long)(v), 0},
#define S(n, v) {n, 0, v},
#define F(f) {A64RT_FEAT_NAME(f), 1, 0},
static const a64_sysctl_t tab[] = {
    A64RT_SYSCTL_INTS(I)
    A64RT_SYSCTL_STRS(S)
    A64_FEATURES(F)
    {"machdep.cpu.vendor", 0, "Apple"},
    {"kern.version", 0, "Darwin Kernel Version 23.0.0: mOS Nucleo; RELEASE_ARM64_T8112"},
};
#undef I
#undef S
#undef F

/* chaves de 64 bits no macOS (CTLTYPE_QUAD); o resto é int */
static int is_quad(const char *n) {
    static const char *q[] = {"hw.cachelinesize", "hw.l1icachesize", "hw.l1dcachesize", "hw.l2cachesize", "hw.pagesize",
                              "hw.tbfrequency", "hw.optional.arm.caps", 0};
    for (int i = 0; q[i]; i++) {
        const char *a = q[i], *b = n;
        while (*a && *a == *b) a++, b++;
        if (!*a && !*b) return 1;
    }
    return 0;
}
static int eq(const char *a, const char *b) { while (*a && *a == *b) a++, b++; return *a == *b; }
static unsigned long slen(const char *s) { unsigned long n = 0; while (s[n]) n++; return n; }

/* 0 = achou (old/oldlen preenchidos), -1 = buffer pequeno (ENOMEM), 1 = não é chave arm64 */
int cx_sysctl_arm64(const char *name, void *old, unsigned long *oldlen) {
    for (unsigned i = 0; i < sizeof tab / sizeof tab[0]; i++) {
        if (!eq(tab[i].name, name)) continue;
        unsigned long n = tab[i].str ? slen(tab[i].str) + 1 : is_quad(name) ? 8 : 4;
        if (!oldlen) return 0;
        if (!old) { *oldlen = n; return 0; }
        if (*oldlen < n) return -1;
        if (tab[i].str) { for (unsigned long k = 0; k < n; k++) ((char *)old)[k] = tab[i].str[k]; }
        else if (n == 8) *(long long *)old = tab[i].num;
        else *(int *)old = (int)tab[i].num;
        *oldlen = n;
        return 0;
    }
    return 1;
}

/* ---- desenrolar através do próprio libSystem (libunwind do LLVM, compilado no modo Apple pelo
 * tools/build-libsystem-arm64.ps1). As imagens Mach-O vêm de _dyld_find_unwind_sections (dlfcn.c); para um pc dentro
 * do libSystem (ELF) ela chama cx_ls_unwind_sections, que devolve o .eh_frame do blob. O deslocamento e o tamanho do
 * .eh_frame são gravados depois do link pelo script (procura o marcador abaixo): o blob não carrega cabeçalhos de seção. */
extern const char __ehdr_start[], _end[];
volatile unsigned long long cx_ls_ehframe[4] = {0x4d4f5345485f4652ULL /* "RF_HESOM" */, 0x4152344d34365f41ULL, 0, 0};
struct cx_unwind_sections { const void *mh, *dwarf; unsigned long dwarf_len; const void *compact; unsigned long compact_len; };
_Bool cx_ls_unwind_sections(const void *addr, struct cx_unwind_sections *info) {
    if ((const char *)addr < __ehdr_start || (const char *)addr >= _end || !cx_ls_ehframe[3]) return 0;
    info->mh = __ehdr_start;
    info->dwarf = __ehdr_start + cx_ls_ehframe[2];
    info->dwarf_len = (unsigned long)cx_ls_ehframe[3];
    info->compact = 0;
    info->compact_len = 0;
    return 1;
}
/* o libunwind (RWMutex.hpp) chama estes com o nome Mach-O dos cabeçalhos do Darwin (__DARWIN_ALIAS: "_pthread_...") */
int pthread_rwlock_rdlock(void *), pthread_rwlock_wrlock(void *), pthread_rwlock_unlock(void *);
int cx_rw_rdlock(void *l) __asm__("_pthread_rwlock_rdlock");
int cx_rw_wrlock(void *l) __asm__("_pthread_rwlock_wrlock");
int cx_rw_unlock(void *l) __asm__("_pthread_rwlock_unlock");
int cx_rw_rdlock(void *l) { return pthread_rwlock_rdlock(l); }
int cx_rw_wrlock(void *l) { return pthread_rwlock_wrlock(l); }
int cx_rw_unlock(void *l) { return pthread_rwlock_unlock(l); }
#endif
