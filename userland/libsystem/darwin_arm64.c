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
#endif
