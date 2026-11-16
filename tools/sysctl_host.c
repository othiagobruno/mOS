/*
 * mOS — teste de host do sysctl do libSystem (userland/libsystem/sysctl.c) contra um kernel falso que responde
 * SYS_DARWIN, SYS_CPUS e SYS_GETPID com dados conhecidos. Confere MIBs, OIDs internos, KERN_PROC, KERN_PROCARGS2,
 * bigcheck, escrita de kern.hostname e o CPUID real do host.
 *
 *   gcc -O1 -Wall -DMOS_LIBSYS_SYSCALL -Iuserland/libsystem -c userland/libsystem/sysctl.c -o /tmp/sysctl_ls.o
 *   gcc -O1 -Wall tools/sysctl_host.c /tmp/sysctl_ls.o -o /tmp/sysctl_host && /tmp/sysctl_host
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <cpuid.h>
#define MOS_SSIZE_T_DEFINED
#include "../kernel/include/syscall.h"

int sysctl(int *mib, unsigned n, void *old, size_t *oldlen, void *newp, size_t newlen);
int sysctlbyname(const char *name, void *old, size_t *oldlen, void *newp, size_t newlen);
int sysctlnametomib(const char *name, int *mib, size_t *n);

/* ---- kernel falso */
static int err;
int *__error(void) { return &err; }
int cx_fdprintf(int fd, const char *fmt, ...) { (void)fd; (void)fmt; return 0; }
static char *fake_argv[] = {"/Aplicativos/Teste.app/Contents/MacOS/Teste", "--flag", 0};
char **NXArgv = fake_argv;
int NXArgc = 2;
static char hostname[256] = "mOS-teste";
static int euid = 501;
typedef struct { int pid, ppid, pgid, sid, uid; const char *name; } fp_t;
static const fp_t fprocs[] = {{1, 0, 1, 1, 0, "init"}, {42, 1, 42, 42, 501, "Teste"}, {77, 42, 42, 42, 501, "Ajudante"},
                              {90, 1, 90, 90, 0, "launchd"}};
#define NFP 4
static const char argblk[] = "/Aplicativos/Teste.app/Contents/MacOS/Teste\0--flag\0HOME=/Users/Visitante\0";

int cx_kinfo_proc(int *mib, unsigned n, void *old, size_t *oldlen) {   /* o de pty.c: pid, nome, pai */
    (void)n;
    unsigned char *k = old;
    memset(k, 0, 648);
    for (int i = 0; i < NFP; i++) if (fprocs[i].pid == mib[3]) {
        memcpy(k + 40, &fprocs[i].pid, 4);
        memcpy(k + 243, fprocs[i].name, strlen(fprocs[i].name));
        memcpy(k + 560, &fprocs[i].ppid, 4);
        int nodev = -1;
        memcpy(k + 572, &nodev, 4);
    }
    *oldlen = 648;
    return 0;
}

long cx_syscall(long nr, long a, long b, long c, long d, long e) {
    (void)e;
    if (nr == SYS_CPUS) return 8;
    if (nr == SYS_GETPID) return 42;
    if (nr != SYS_DARWIN) return -38;
    switch (a) {
    case MOS_DW_SYSINFO: {
        mos_dw_sysinfo_t *s = (void *)b;
        memset(s, 0, sizeof *s);
        s->mem_total = 14ull << 30; s->mem_free = 6ull << 30;
        s->boot_unix_us = 1763200000ull * 1000000 + 250000; s->uptime_ms = 123456;
        s->swap_used = 10u << 20; s->swap_total = 64u << 20;
        s->ncpu = 8; s->nprocs = NFP; s->nthreads = 11; s->nrunnable = 3;
        return 0;
    }
    case MOS_DW_HOSTNAME: {
        size_t n = strlen(hostname);
        if (b && c) memcpy((void *)b, hostname, n + 1 <= (size_t)c ? n + 1 : (size_t)c);
        return (long)n;
    }
    case MOS_DW_SETHOSTNAME:
        if (euid) return -MOS_EPERM;
        memcpy(hostname, (void *)b, (size_t)c); hostname[c] = 0;
        return 0;
    case MOS_DW_PIDS: {
        int *o = (int *)b;
        for (int i = 0; i < NFP && i < c; i++) o[i] = fprocs[i].pid;
        return NFP;
    }
    case MOS_DW_PROC: {
        for (int i = 0; i < NFP; i++) if (fprocs[i].pid == b) {
            mos_dw_proc_t *p = (void *)c;
            memset(p, 0, sizeof *p);
            p->pid = fprocs[i].pid; p->ppid = fprocs[i].ppid; p->pgid = fprocs[i].pgid; p->sid = fprocs[i].sid;
            p->uid = p->euid = p->suid = fprocs[i].uid; p->gid = p->egid = p->sgid = 20;
            p->state = MOS_DW_ST_SLEEP; p->nthreads = 2;
            p->start_unix_us = 1763200100ull * 1000000 + 5;
            strcpy(p->name, fprocs[i].name);
            strcpy(p->path, "/x");
            return 0;
        }
        return -MOS_ESRCH;
    }
    case MOS_DW_ARGS: {
        if (b != 42) return b == 1 ? -MOS_EPERM : -MOS_ESRCH;
        const char *path = "/Aplicativos/Teste.app/Contents/MacOS/Teste";
        size_t head = (4 + strlen(path) + 1 + 7) & ~(size_t)7, body = sizeof argblk - 1, tot = head + body;
        if (!c) return (long)tot;
        if ((size_t)d < tot) return -MOS_ENOMEM;
        unsigned char *o = (void *)c;
        memset(o, 0, head);
        int argc = 2;
        memcpy(o, &argc, 4);
        memcpy(o + 4, path, strlen(path));
        memcpy(o + head, argblk, body);
        return (long)tot;
    }
    }
    return -22;
}

/* ---- verificações */
static int fails, oks;
#define CHECK(c, ...) do { if (c) oks++; else { fails++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint64_t q(const char *n) { uint64_t v = 0; size_t l = 8; int r = sysctlbyname(n, &v, &l, 0, 0); CHECK(r == 0 && l == 8, "%s q", n); return v; }
static int i32(const char *n) { int v = -1; size_t l = 4; int r = sysctlbyname(n, &v, &l, 0, 0); CHECK(r == 0 && l == 4, "%s i (r=%d errno=%d)", n, r, err); return v; }
static const char *s(const char *n) { static char b[600]; size_t l = sizeof b; int r = sysctlbyname(n, b, &l, 0, 0); CHECK(r == 0 && l == strlen(b) + 1, "%s s", n); return b; }

int main(void) {
    /* memória e CPUs do kernel */
    CHECK(q("hw.memsize") == 14ull << 30, "memsize");
    { uint32_t w; size_t l = 4; err = 0; CHECK(sysctlbyname("hw.memsize", &w, &l, 0, 0) == -1 && err == 12, "bigcheck >4G = ENOMEM"); }
    { uint32_t w; size_t l = 4; CHECK(sysctlbyname("hw.cachelinesize", &w, &l, 0, 0) == 0 && w >= 32, "bigcheck 4 bytes"); }
    CHECK(i32("hw.ncpu") == 8 && i32("hw.logicalcpu") == 8 && i32("hw.activecpu") == 8, "ncpu");
    int ph = i32("hw.physicalcpu");
    CHECK(ph >= 1 && ph <= 8 && 8 % ph == 0, "physicalcpu %d", ph);
    CHECK(i32("hw.physmem") == 0x7fffffff, "physmem capado");
    CHECK(i32("kern.maxfilesperproc") == 256, "maxfilesperproc");
    CHECK(i32("kern.num_tasks") == NFP && i32("kern.num_threads") == 11, "num_tasks/threads");
    CHECK(i32("kern.memorystatus_level") == 42, "memorystatus_level");
    CHECK(i32("kern.memorystatus_vm_pressure_level") == 1, "pressure");
    /* MIB numérica clássica */
    { int mib[2] = {6, 24}; uint64_t v = 0; size_t l = 8; CHECK(sysctl(mib, 2, &v, &l, 0, 0) == 0 && v == 14ull << 30, "MIB hw.memsize"); }
    { int mib[2] = {6, 3}; int v = 0; size_t l = 4; CHECK(sysctl(mib, 2, &v, &l, 0, 0) == 0 && v == 8, "MIB hw.ncpu"); }
    { int mib[2] = {1, 1}; char b[16]; size_t l = 16; CHECK(sysctl(mib, 2, b, &l, 0, 0) == 0 && !strcmp(b, "Darwin"), "MIB ostype"); }
    { int mib[2] = {1, 65}; char b[16]; size_t l = 16; CHECK(sysctl(mib, 2, b, &l, 0, 0) == 0 && !strcmp(b, "23A344"), "MIB osversion"); }
    { int mib[2] = {8, 1}; char b[64]; size_t l = 64; CHECK(sysctl(mib, 2, b, &l, 0, 0) == 0 && !strcmp(b, "/usr/bin:/bin:/usr/sbin:/sbin"), "user.cs_path"); }
    { int mib[2] = {6, 999}; size_t l = 4; int v; err = 0; CHECK(sysctl(mib, 2, &v, &l, 0, 0) == -1 && err == 2, "MIB desconhecida ENOENT"); }
    /* tamanho com old NULL e buffer curto */
    { size_t l = 0; CHECK(sysctlbyname("kern.version", 0, &l, 0, 0) == 0 && l == strlen(s("kern.version")) + 1, "tamanho"); }
    { char b[3]; size_t l = 3; err = 0; CHECK(sysctlbyname("kern.ostype", b, &l, 0, 0) == -1 && err == 12, "curto ENOMEM"); }
    { size_t l = 4; int v; err = 0; CHECK(sysctlbyname("nao.existe", &v, &l, 0, 0) == -1 && err == 2, "desconhecida"); }
    /* boottime, swap, loadavg, clockrate */
    { struct { int64_t sec; int32_t us, pad; } tv; size_t l = sizeof tv;
      CHECK(sysctlbyname("kern.boottime", &tv, &l, 0, 0) == 0 && l == 16 && tv.sec == 1763200000 && tv.us == 250000, "boottime"); }
    { uint64_t x[4]; size_t l = 32; CHECK(sysctlbyname("vm.swapusage", x, &l, 0, 0) == 0 && l == 32 && x[0] == 64u << 20 && x[2] == 10u << 20 && x[1] == 54u << 20, "swapusage"); }
    { uint32_t a[6]; size_t l = 24; CHECK(sysctlbyname("vm.loadavg", a, &l, 0, 0) == 0 && l == 24 && a[0] == 3 * 2048 && a[4] == 2048, "loadavg"); }
    { int c[5]; size_t l = 20; int mib[2] = {1, 12}; CHECK(sysctl(mib, 2, c, &l, 0, 0) == 0 && c[0] == 1000, "clockrate"); }
    /* hostname: leitura, escrita negada sem root, escrita como root */
    CHECK(!strcmp(s("kern.hostname"), "mOS-teste"), "hostname");
    { err = 0; CHECK(sysctlbyname("kern.hostname", 0, 0, "x", 1) == -1 && err == 1, "hostname EPERM"); }
    euid = 0;
    CHECK(sysctlbyname("kern.hostname", 0, 0, "maquina", 7) == 0 && !strcmp(s("kern.hostname"), "maquina"), "sethostname");
    { err = 0; CHECK(sysctlbyname("hw.ncpu", 0, 0, "x", 1) == -1 && err == 1, "só leitura EPERM"); }
    /* nametomib e OIDs internos */
    { int m[4]; size_t n = 4; CHECK(sysctlnametomib("hw.memsize", m, &n) == 0 && n == 2 && m[0] == 6 && m[1] == 24, "nametomib memsize"); }
    { int m[4]; size_t n = 4; CHECK(sysctlnametomib("kern.proc.pid", m, &n) == 0 && n == 3 && m[0] == 1 && m[1] == 14 && m[2] == 1, "nametomib proc.pid"); }
    { int m[4]; size_t n = 4; CHECK(sysctlnametomib("machdep.cpu.brand_string", m, &n) == 0 && n == 2 && m[0] == 7 && m[1] >= 1000, "nametomib auto"); }
    { int q3[2] = {0, 3}, out[4]; size_t l = sizeof out; const char *nm = "kern.osrelease";
      CHECK(sysctl(q3, 2, out, &l, (void *)nm, strlen(nm)) == 0 && l == 8 && out[0] == 1 && out[1] == 2, "{0,3}"); }
    { int q1[4] = {0, 1, 6, 24}; char b[64]; size_t l = 64; CHECK(sysctl(q1, 4, b, &l, 0, 0) == 0 && !strcmp(b, "hw.memsize"), "{0,1}"); }
    { int q4[4] = {0, 4, 6, 24}; unsigned char b[32]; size_t l = 32; unsigned k;
      CHECK(sysctl(q4, 4, b, &l, 0, 0) == 0 && (memcpy(&k, b, 4), (k & 15) == 4 && (k & 0x80000000u)) && !strcmp((char *)b + 4, "Q"), "{0,4}"); }
    { int q4[4] = {0, 4, 1, 10}; unsigned char b[32]; size_t l = 32; unsigned k;
      CHECK(sysctl(q4, 4, b, &l, 0, 0) == 0 && (memcpy(&k, b, 4), (k & 0x40000000u) && (k & 15) == 3), "{0,4} hostname gravável"); }
    {   /* {0,2}: percorre a árvore inteira em ordem crescente, sem repetir */
        int cur[2] = {0, 0}, n = 0, ok = 1, have = 0;
        for (;;) {
            int qn[4] = {0, 2, cur[0], cur[1]}, nx[2];
            size_t l = sizeof nx;
            if (sysctl(qn, have ? 4 : 2, nx, &l, 0, 0) != 0) break;
            if (have && (nx[0] < cur[0] || (nx[0] == cur[0] && nx[1] <= cur[1]))) { ok = 0; break; }
            cur[0] = nx[0]; cur[1] = nx[1]; have = 1;
            if (++n > 1000) { ok = 0; break; }
        }
        CHECK(ok && n > 150, "{0,2} percorreu %d", n);
    }
    /* KERN_PROC */
    {
        int mib[4] = {1, 14, 1, 42};
        unsigned char k[648]; size_t l = sizeof k;
        CHECK(sysctl(mib, 4, k, &l, 0, 0) == 0 && l == 648, "proc pid");
        int pid, uid, ppid, flag; int64_t st;
        memcpy(&pid, k + 40, 4); memcpy(&uid, k + 420, 4); memcpy(&ppid, k + 560, 4); memcpy(&flag, k + 32, 4); memcpy(&st, k, 8);
        CHECK(pid == 42 && uid == 501 && ppid == 1 && !strcmp((char *)k + 243, "Teste") && (flag & 4) && st == 1763200100 && k[36] == 3, "kinfo_proc campos (SSLEEP)");
        mib[3] = 4242; l = sizeof k;
        CHECK(sysctl(mib, 4, k, &l, 0, 0) == 0 && l == 0, "pid inexistente 0 bytes");
    }
    {
        int mib[3] = {1, 14, 0};
        size_t l = 0;
        CHECK(sysctl(mib, 3, 0, &l, 0, 0) == 0 && l == (NFP + 5) * 648, "proc all tamanho");
        unsigned char *b = malloc(l);
        CHECK(sysctl(mib, 3, b, &l, 0, 0) == 0 && l == NFP * 648, "proc all");
        size_t cur = 648;
        err = 0;
        CHECK(sysctl(mib, 3, b, &cur, 0, 0) == -1 && err == 12 && cur == 648, "proc all curto");
        int mu[4] = {1, 14, 5, 501};
        l = NFP * 648;
        CHECK(sysctl(mu, 4, b, &l, 0, 0) == 0 && l == 2 * 648, "proc uid 501");
        int mg[4] = {1, 14, 2, 42};
        l = NFP * 648;
        CHECK(sysctl(mg, 4, b, &l, 0, 0) == 0 && l == 2 * 648, "proc pgrp 42");
        int ms[4] = {1, 14, 3, 90};
        l = NFP * 648;
        CHECK(sysctl(ms, 4, b, &l, 0, 0) == 0 && l == 648, "proc session 90");
        free(b);
    }
    /* KERN_PROCARGS2 e KERN_PROCARGS */
    {
        int mib[3] = {1, 49, 42};
        size_t l = 0;
        CHECK(sysctl(mib, 3, 0, &l, 0, 0) == 0 && l > 50, "procargs2 tamanho");
        unsigned char b[512]; l = sizeof b;
        CHECK(sysctl(mib, 3, b, &l, 0, 0) == 0, "procargs2");
        int argc; memcpy(&argc, b, 4);
        size_t head = (4 + strlen((char *)b + 4) + 1 + 7) & ~(size_t)7;
        CHECK(argc == 2 && !strcmp((char *)b + 4, "/Aplicativos/Teste.app/Contents/MacOS/Teste") && !strcmp((char *)b + head + 44, "--flag"), "procargs2 conteúdo");
        int m1[3] = {1, 38, 42}; l = sizeof b;
        CHECK(sysctl(m1, 3, b, &l, 0, 0) == 0 && !strcmp((char *)b, "/Aplicativos/Teste.app/Contents/MacOS/Teste"), "procargs sem argc");
        int m2[3] = {1, 49, 1}; l = sizeof b; err = 0;
        CHECK(sysctl(m2, 3, b, &l, 0, 0) == -1 && err == 1, "procargs2 de outro uid EPERM");
        int m3[3] = {1, 49, 5555}; l = sizeof b; err = 0;
        CHECK(sysctl(m3, 3, b, &l, 0, 0) == -1 && err == 22, "procargs2 inexistente EINVAL");
    }
    /* CPUID do host */
    {
        unsigned a, b, c, d;
        __cpuid(0, a, b, c, d);
        char v[13]; memcpy(v, &b, 4); memcpy(v + 4, &d, 4); memcpy(v + 8, &c, 4); v[12] = 0;
        CHECK(!strcmp(s("machdep.cpu.vendor"), v), "vendor %s", v);
        CHECK(strlen(s("machdep.cpu.brand_string")) > 3, "brand");
        __cpuid(1, a, b, c, d);
        CHECK((unsigned)i32("machdep.cpu.signature") == a, "signature");
        CHECK(i32("hw.optional.sse4_2") == !!(c & (1u << 20)), "sse4_2");
        CHECK(i32("hw.optional.avx2_0") == !!__builtin_cpu_supports("avx2"), "avx2");
        CHECK(i32("hw.optional.aes") == !!(c & (1u << 25)), "aes");
        CHECK(strstr(s("machdep.cpu.features"), "SSE2") != 0, "features");
        uint64_t f = q("hw.cpufrequency");
        CHECK(f > 400000000ull && f < 8000000000ull, "cpufrequency %llu", (unsigned long long)f);
        CHECK(q("hw.l1dcachesize") >= 16384 && q("hw.l2cachesize") >= 65536, "caches");
        uint64_t cc[10]; size_t l = sizeof cc;
        CHECK(sysctlbyname("hw.cacheconfig", cc, &l, 0, 0) == 0 && l == 80 && cc[0] == 8, "cacheconfig");
        char ven[16]; strcpy(ven, s("machdep.cpu.vendor"));
        printf("cpu: %s / %s, %d núcleos físicos, TSC %llu Hz, L1d %llu L2 %llu L3 %llu\n", ven,
               s("machdep.cpu.brand_string"), ph, (unsigned long long)f, (unsigned long long)q("hw.l1dcachesize"),
               (unsigned long long)q("hw.l2cachesize"), (unsigned long long)q("hw.l3cachesize"));
    }
    printf("sysctl: %d/%d ok\n", oks, oks + fails);
    return fails != 0;
}
