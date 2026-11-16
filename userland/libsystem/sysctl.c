/*
 * mOS libSystem — sysctl(3) completo: sysctl, sysctlbyname, sysctlnametomib e os OIDs internos {0,1..4}
 * (nome, próximo, nome→MIB e formato), com os valores reais do kernel (SYS_DARWIN, kernel/proc/darwin.c) e do CPUID.
 * docs/darwin-bsd-mach.md, fase F1.
 *
 * Regras do XNU que valem aqui:
 *   - old NULL: *oldlen recebe o tamanho; old curto: ENOMEM (KERN_PROC copia o que couber e também dá ENOMEM).
 *   - valor de 64 bits lido com *oldlen == 4 devolve 32 bits se couber (sysctl_io_number com bigcheck).
 *   - newp em chave só de leitura: EPERM. kern.hostname é gravável pelo root.
 *   - MIBs numéricas fixas onde o XNU tem (CTL_KERN, CTL_HW, CTL_VM, CTL_USER); as OID_AUTO recebem números
 *     estáveis a partir de 1000 dentro do primeiro nível.
 * Sem SYS_DARWIN (harness tools/machotest.c, kernel antigo) os valores do sistema caem nos padrões de antes.
 */
#include "sys.h"

size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int cx_fdprintf(int fd, const char *fmt, ...);
int clock_gettime(int, void *);
int cx_kinfo_proc(int *mib, unsigned n, void *old, size_t *oldlen);   /* pty.c: KERN_PROC_PID com terminal e grupo */

/* ------------------------------------------------------------------ dados do kernel */
static int dw_sysinfo(mos_dw_sysinfo_t *si) {
    memset(si, 0, sizeof *si);
    if (SYS2(SYS_DARWIN, MOS_DW_SYSINFO, si) == 0 && si->mem_total) return 1;
    long c = SYS0(SYS_CPUS);
    si->mem_total = 2ull << 30;                 /* padrão antigo sem o kernel novo */
    si->mem_free = 1ull << 30;
    si->ncpu = c > 0 ? (u32)c : 1;
    return 0;
}
static u32 ncpu_now(void) { long c = SYS0(SYS_CPUS); return c > 0 ? (u32)c : 1; }

/* ------------------------------------------------------------------ CPUID */
typedef struct {
    int ok;
    u32 max_basic, max_ext, sig, l1_ebx, l1_ecx, l1_edx, l7_ebx, l7_ecx, l7_edx, e1_ecx, e1_edx, e7_edx;
    u64 xcr0;
    char vendor[13], brand[49];
    u32 family, model, stepping, ext_family, ext_model, microcode;
    u32 tpc;                                    /* threads por núcleo */
    u64 l1d, l1i, l2, l3, line;
    u32 l2_share, l3_share;                     /* threads lógicas que dividem cada nível */
    u64 tsc_hz;
} cpu_t;
static cpu_t cpu;

#if defined(__x86_64__)
static void cpuid(u32 leaf, u32 sub, u32 r[4]) {
    __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(leaf), "c"(sub));
}
static u64 rdtsc(void) { u32 lo, hi; __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi)); return (u64)hi << 32 | lo; }
#endif
typedef struct { long s, ns; } sc_ts_t;
__attribute__((unused)) static u64 now_ns(void) { sc_ts_t t = {0, 0}; clock_gettime(6, &t); return (u64)t.s * 1000000000ull + (u64)t.ns; }

__attribute__((unused)) static void cpu_caches_leaf(u32 leaf) {
#if defined(__x86_64__)
    for (u32 i = 0; i < 16; i++) {
        u32 r[4];
        cpuid(leaf, i, r);
        u32 type = r[0] & 31;
        if (!type) break;
        u32 level = (r[0] >> 5) & 7;
        u64 ways = (r[1] >> 22) + 1, parts = ((r[1] >> 12) & 0x3ff) + 1, line = (r[1] & 0xfff) + 1, sets = (u64)r[2] + 1;
        u64 sz = ways * parts * line * sets;
        u32 share = ((r[0] >> 14) & 0xfff) + 1;
        if (!cpu.line) cpu.line = line;
        if (level == 1 && type == 1) cpu.l1d = sz;
        else if (level == 1 && type == 2) cpu.l1i = sz;
        else if (level == 2) { cpu.l2 = sz; cpu.l2_share = share; }
        else if (level == 3) { cpu.l3 = sz; cpu.l3_share = share; }
    }
#else
    (void)leaf;
#endif
}

static void cpu_init(void) {
    if (cpu.ok) return;
#if defined(__x86_64__)
    u32 r[4];
    cpuid(0, 0, r);
    cpu.max_basic = r[0];
    memcpy(cpu.vendor, &r[1], 4); memcpy(cpu.vendor + 4, &r[3], 4); memcpy(cpu.vendor + 8, &r[2], 4);
    cpuid(1, 0, r);
    cpu.sig = r[0]; cpu.l1_ebx = r[1]; cpu.l1_ecx = r[2]; cpu.l1_edx = r[3];
    u32 bf = (r[0] >> 8) & 15, bm = (r[0] >> 4) & 15;
    cpu.ext_family = (r[0] >> 20) & 0xff; cpu.ext_model = (r[0] >> 16) & 15;
    cpu.family = bf == 15 ? bf + cpu.ext_family : bf;
    cpu.model = (bf == 6 || bf == 15) ? bm | cpu.ext_model << 4 : bm;
    cpu.stepping = r[0] & 15;
    if (cpu.max_basic >= 7) { cpuid(7, 0, r); cpu.l7_ebx = r[1]; cpu.l7_ecx = r[2]; cpu.l7_edx = r[3]; }
    if (cpu.l1_ecx & (1u << 27)) {              /* OSXSAVE: o que o kernel salva na troca de contexto */
        u32 lo, hi;
        __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
        cpu.xcr0 = (u64)hi << 32 | lo;
    }
    cpuid(0x80000000u, 0, r);
    cpu.max_ext = r[0];
    if (cpu.max_ext >= 0x80000001u) { cpuid(0x80000001u, 0, r); cpu.e1_ecx = r[2]; cpu.e1_edx = r[3]; }
    if (cpu.max_ext >= 0x80000004u) {
        for (u32 i = 0; i < 3; i++) { cpuid(0x80000002u + i, 0, r); memcpy(cpu.brand + i * 16, r, 16); }
        cpu.brand[48] = 0;
        char *b = cpu.brand;
        while (*b == ' ') b++;
        if (b != cpu.brand) { size_t n = strlen(b); for (size_t i = 0; i <= n; i++) cpu.brand[i] = b[i]; }
        for (size_t n = strlen(cpu.brand); n && cpu.brand[n - 1] == ' '; n--) cpu.brand[n - 1] = 0;
    }
    if (cpu.max_ext >= 0x80000007u) { cpuid(0x80000007u, 0, r); cpu.e7_edx = r[3]; }
    int amd = !strcmp(cpu.vendor, "AuthenticAMD") || !strcmp(cpu.vendor, "HygonGenuine");
    /* caches: folha 4 (Intel) ou 0x8000001D (AMD com TOPOEXT); senão as folhas antigas da AMD */
    if (!amd && cpu.max_basic >= 4) cpu_caches_leaf(4);
    else if (amd && (cpu.e1_ecx & (1u << 22)) && cpu.max_ext >= 0x8000001Du) cpu_caches_leaf(0x8000001Du);
    if (amd && !cpu.l1d && cpu.max_ext >= 0x80000006u) {
        cpuid(0x80000005u, 0, r);
        cpu.l1d = (u64)(r[2] >> 24) << 10; cpu.l1i = (u64)(r[3] >> 24) << 10; cpu.line = r[2] & 0xff;
        cpuid(0x80000006u, 0, r);
        cpu.l2 = (u64)(r[2] >> 16) << 10; cpu.l3 = (u64)(r[3] >> 18) * (512u << 10);
    }
    /* threads por núcleo */
    cpu.tpc = 1;
    if (amd && (cpu.e1_ecx & (1u << 22)) && cpu.max_ext >= 0x8000001Eu) { cpuid(0x8000001Eu, 0, r); cpu.tpc = ((r[1] >> 8) & 0xff) + 1; }
    else if (!amd && cpu.max_basic >= 0xB) { cpuid(0xB, 0, r); if ((r[1] & 0xffff) && ((r[2] >> 8) & 0xff) == 1) cpu.tpc = r[1] & 0xffff; }
    if (!cpu.tpc) cpu.tpc = 1;
    /* frequência do TSC: folha 0x15 quando informa, senão medida contra o relógio monotônico (2 ms) */
    if (cpu.max_basic >= 0x15) { cpuid(0x15, 0, r); if (r[0] && r[1] && r[2]) cpu.tsc_hz = (u64)r[2] * r[1] / r[0]; }
    if (!cpu.tsc_hz) {
        u64 t0 = now_ns(), c0 = rdtsc(), t1;
        do t1 = now_ns(); while (t1 - t0 < 2000000ull && t1 >= t0);
        u64 c1 = rdtsc();
        if (t1 > t0) cpu.tsc_hz = (c1 - c0) * 1000000000ull / (t1 - t0);
        cpu.tsc_hz = (cpu.tsc_hz + 500000) / 1000000 * 1000000;        /* arredonda ao MHz */
    }
#else
    memcpy(cpu.vendor, "Apple", 6);
    memcpy(cpu.brand, "Apple processor", 16);
    cpu.tpc = 1;
#endif
    if (!cpu.line) cpu.line = 64;
    if (!cpu.l2_share) cpu.l2_share = cpu.tpc;
    if (!cpu.l3_share) cpu.l3_share = ncpu_now();
    if (!cpu.tsc_hz) cpu.tsc_hz = 2000000000ull;
    cpu.ok = 1;
}
static u32 phys_cpus(void) { cpu_init(); u32 n = ncpu_now(); u32 p = n / cpu.tpc; return p ? p : 1; }
static int ymm_ok(void) { return (cpu.l1_ecx & (1u << 28)) && (cpu.xcr0 & 6) == 6; }
static int zmm_ok(void) { return ymm_ok() && (cpu.xcr0 & 0xe0) == 0xe0; }

/* nomes do machdep.cpu.features / leaf7_features / extfeatures, na ordem dos bits (como o XNU imprime) */
static const char *const f_edx[32] = {"FPU", "VME", "DE", "PSE", "TSC", "MSR", "PAE", "MCE", "CX8", "APIC", 0, "SEP", "MTRR",
    "PGE", "MCA", "CMOV", "PAT", "PSE36", 0, "CLFSH", 0, "DS", "ACPI", "MMX", "FXSR", "SSE", "SSE2", "SS", "HTT", "TM", 0, "PBE"};
static const char *const f_ecx[32] = {"SSE3", "PCLMULQDQ", "DTES64", "MON", "DSCPL", "VMX", "SMX", "EST", "TM2", "SSSE3", "CID",
    0, "FMA", "CX16", "TPR", "PDCM", 0, "PCID", "DCA", "SSE4.1", "SSE4.2", "x2APIC", "MOVBE", "POPCNT", "TSCTMR", "AES",
    "XSAVE", "OSXSAVE", "AVX1.0", "F16C", "RDRAND", "VMM"};
static const char *const f_l7b[32] = {"RDWRFSGS", "TSC_THREAD_OFFSET", "SGX", "BMI1", "HLE", "AVX2", 0, "SMEP", "BMI2", "ERMS",
    "INVPCID", "RTM", "PQM", "FPU_CSDS", "MPX", "PQE", "AVX512F", "AVX512DQ", "RDSEED", "ADX", "SMAP", "AVX512IFMA", 0,
    "CLFSOPT", "CLWB", "IPT", 0, 0, "AVX512CD", "SHA", "AVX512BW", "AVX512VL"};
static void feat_str(char *out, size_t cap, const char *const *names, u32 bits, const char *const *names2, u32 bits2) {
    size_t n = 0;
    out[0] = 0;
    for (int pass = 0; pass < 2; pass++) {
        const char *const *nm = pass ? names2 : names;
        u32 b = pass ? bits2 : bits;
        if (!nm) continue;
        for (int i = 0; i < 32; i++) {
            if (!(b & (1u << i)) || !nm[i]) continue;
            size_t k = strlen(nm[i]);
            if (n + k + 2 > cap) return;
            if (n) out[n++] = ' ';
            memcpy(out + n, nm[i], k);
            n += k;
            out[n] = 0;
        }
    }
}

/* ------------------------------------------------------------------ tabela */
enum { T_NODE = 1, T_INT = 2, T_STR = 3, T_QUAD = 4, T_STRUCT = 5 };
enum {
    V_FIX = 0, V_NCPU, V_PHYS, V_MEMSIZE, V_PHYSMEM, V_USERMEM, V_HOSTNAME, V_BOOTTIME, V_SWAP, V_LOADAVG, V_CLOCKRATE,
    V_CPUFREQ, V_TSCFREQ, V_L1D, V_L1I, V_L2, V_L3, V_LINE, V_CACHECONF, V_CACHESIZE, V_BRAND, V_VENDOR, V_FAMILY, V_MODEL,
    V_STEPPING, V_EXTFAM, V_EXTMODEL, V_SIG, V_FEATBITS, V_EXTFEATBITS, V_L7BITS, V_FEATS, V_L7FEATS, V_EXTFEATS,
    V_OPT, V_MAXBASIC, V_MAXEXT, V_TPC_CORES, V_NUMTASKS, V_NUMTHREADS, V_VMM, V_PRESSURE, V_MSLEVEL, V_FREEPAGES,
    V_SAFEBOOT, V_LOGICAL_PKG,
};
typedef struct { const char *name; u8 type, rw, v; u8 a; int mib1; u64 num; const char *str; } sc_ent_t;
/* a: parâmetro do valor (V_OPT: qual recurso). mib1: segundo nível da MIB (0 = OID_AUTO) */
enum { O_MMX, O_SSE, O_SSE2, O_SSE3, O_SSSE3, O_SSE41, O_SSE42, O_AES, O_AVX, O_AVX2, O_RDRAND, O_F16C, O_FMA, O_BMI1,
       O_BMI2, O_RTM, O_HLE, O_ADX, O_ERMS, O_POPCNT, O_AVX512F, O_AVX512CD, O_AVX512DQ, O_AVX512BW, O_AVX512VL,
       O_AVX512IFMA, O_SHA, O_SGX, O_MPX };

#define KERNVER "Darwin Kernel Version 23.0.0: mOS Nucleo; RELEASE_X86_64"
static const sc_ent_t tab[] = {
    /* CTL_KERN = 1 */
    {"kern.ostype", T_STR, 0, V_FIX, 0, 1, 0, "Darwin"},
    {"kern.osrelease", T_STR, 0, V_FIX, 0, 2, 0, "23.0.0"},
    {"kern.osrevision", T_INT, 0, V_FIX, 0, 3, 199506, 0},
    {"kern.version", T_STR, 0, V_FIX, 0, 4, 0, KERNVER},
    {"kern.maxvnodes", T_INT, 0, V_FIX, 0, 5, 263168, 0},
    {"kern.maxproc", T_INT, 0, V_FIX, 0, 6, 4176, 0},             /* o kernel não tem teto fixo; valor de um Mac de 16 GB */
    {"kern.maxfiles", T_INT, 0, V_FIX, 0, 7, 122880, 0},
    {"kern.argmax", T_INT, 0, V_FIX, 0, 8, 1048576, 0},          /* argv+env vão inteiros para a pilha (macho.c) */
    {"kern.securelevel", T_INT, 0, V_FIX, 0, 9, 0, 0},
    {"kern.hostname", T_STR, 1, V_HOSTNAME, 0, 10, 0, 0},
    {"kern.hostid", T_INT, 0, V_FIX, 0, 11, 0, 0},
    {"kern.clockrate", T_STRUCT, 0, V_CLOCKRATE, 0, 12, 0, 0},
    {"kern.posix1version", T_INT, 0, V_FIX, 0, 17, 200112, 0},
    {"kern.ngroups", T_INT, 0, V_FIX, 0, 18, 16, 0},
    {"kern.job_control", T_INT, 0, V_FIX, 0, 19, 1, 0},
    {"kern.saved_ids", T_INT, 0, V_FIX, 0, 20, 1, 0},
    {"kern.boottime", T_STRUCT, 0, V_BOOTTIME, 0, 21, 0, 0},
    {"kern.nisdomainname", T_STR, 0, V_FIX, 0, 22, 0, ""},
    {"kern.maxpartitions", T_INT, 0, V_FIX, 0, 23, 8, 0},
    {"kern.osreldate", T_INT, 0, V_FIX, 0, 26, 230000, 0},
    {"kern.bootfile", T_STR, 0, V_FIX, 0, 28, 0, "/System/Library/Kernels/kernel"},
    {"kern.maxfilesperproc", T_INT, 0, V_FIX, 0, 29, 256, 0},    /* PROC_MAX_FDS (kernel/include/proc.h) */
    {"kern.maxprocperuid", T_INT, 0, V_FIX, 0, 30, 2784, 0},
    {"kern.usrstack", T_INT, 0, V_FIX, 0, 35, 0, 0},
    {"kern.netboot", T_INT, 0, V_FIX, 0, 40, 0, 0},
    {"kern.aiomax", T_INT, 0, V_FIX, 0, 46, 90, 0},
    {"kern.aioprocmax", T_INT, 0, V_FIX, 0, 47, 16, 0},
    {"kern.aiothreads", T_INT, 0, V_FIX, 0, 48, 4, 0},
    {"kern.corefile", T_STR, 0, V_FIX, 0, 50, 0, "/cores/core.%P"},
    {"kern.coredump", T_INT, 0, V_FIX, 0, 51, 0, 0},
    {"kern.sugid_coredump", T_INT, 0, V_FIX, 0, 52, 0, 0},
    {"kern.usrstack64", T_QUAD, 0, V_FIX, 0, 59, 0x400040000000ull, 0},   /* USER_STACK_TOP */
    {"kern.nx", T_INT, 0, V_FIX, 0, 60, 1, 0},
    {"kern.osversion", T_STR, 0, V_FIX, 0, 65, 0, "23A344"},
    {"kern.safeboot", T_INT, 0, V_SAFEBOOT, 0, 66, 0, 0},
    {"kern.osproductversion", T_STR, 0, V_FIX, 0, 0, 0, "14.0"},
    {"kern.osproductversioncompat", T_STR, 0, V_FIX, 0, 0, 0, "14.0"},
    {"kern.iossupportversion", T_STR, 0, V_FIX, 0, 0, 0, "17.0"},   /* Mac Catalyst: iOSSupport do macOS 14 */
    {"kern.bootargs", T_STR, 0, V_FIX, 0, 0, 0, ""},
    {"kern.singleuser", T_INT, 0, V_FIX, 0, 0, 0, 0},
    {"kern.num_tasks", T_INT, 0, V_NUMTASKS, 0, 0, 0, 0},
    {"kern.num_threads", T_INT, 0, V_NUMTHREADS, 0, 0, 0, 0},
    {"kern.num_taskthreads", T_INT, 0, V_FIX, 0, 0, 8192, 0},
    {"kern.memorystatus_vm_pressure_level", T_INT, 0, V_PRESSURE, 0, 0, 0, 0},
    {"kern.memorystatus_level", T_INT, 0, V_MSLEVEL, 0, 0, 0, 0},
    {"kern.hv_support", T_INT, 0, V_FIX, 0, 0, 0, 0},
    {"kern.hv_vmm_present", T_INT, 0, V_VMM, 0, 0, 0, 0},
    {"kern.tcsm_available", T_INT, 0, V_FIX, 0, 0, 0, 0},
    {"kern.tcsm_enable", T_INT, 0, V_FIX, 0, 0, 0, 0},
    {"kern.secure_kernel", T_INT, 0, V_FIX, 0, 0, 0, 0},
    {"kern.osvariant_status", T_QUAD, 0, V_FIX, 0, 0, 0, 0},
    /* CTL_VM = 2 */
    {"vm.loadavg", T_STRUCT, 0, V_LOADAVG, 0, 2, 0, 0},
    {"vm.swapusage", T_STRUCT, 0, V_SWAP, 0, 5, 0, 0},
    {"vm.pagesize", T_INT, 0, V_FIX, 0, 0, 4096, 0},
    {"vm.page_free_count", T_INT, 0, V_FREEPAGES, 0, 0, 0, 0},
    {"vm.memory_pressure", T_INT, 0, V_FIX, 0, 0, 0, 0},
    {"vm.compressor_mode", T_INT, 0, V_FIX, 0, 0, 4, 0},
    /* CTL_HW = 6 */
    {"hw.machine", T_STR, 0, V_FIX, 0, 1, 0, "x86_64"},
    {"hw.model", T_STR, 0, V_FIX, 0, 2, 0, "mOS1,1"},
    {"hw.ncpu", T_INT, 0, V_NCPU, 0, 3, 0, 0},
    {"hw.byteorder", T_INT, 0, V_FIX, 0, 4, 1234, 0},
    {"hw.physmem", T_INT, 0, V_PHYSMEM, 0, 5, 0, 0},
    {"hw.usermem", T_INT, 0, V_USERMEM, 0, 6, 0, 0},
    {"hw.pagesize32", T_INT, 0, V_FIX, 0, 7, 4096, 0},
    {"hw.epoch", T_INT, 0, V_FIX, 0, 10, 0, 0},
    {"hw.floatingpoint", T_INT, 0, V_FIX, 0, 11, 1, 0},
    {"hw.machinearch", T_STR, 0, V_FIX, 0, 12, 0, "x86_64"},
    {"hw.vectorunit", T_INT, 0, V_FIX, 0, 13, 1, 0},
    {"hw.busfrequency", T_QUAD, 0, V_FIX, 0, 14, 100000000ull, 0},
    {"hw.cpufrequency", T_QUAD, 0, V_CPUFREQ, 0, 15, 0, 0},
    {"hw.cachelinesize", T_QUAD, 0, V_LINE, 0, 16, 0, 0},
    {"hw.l1icachesize", T_QUAD, 0, V_L1I, 0, 17, 0, 0},
    {"hw.l1dcachesize", T_QUAD, 0, V_L1D, 0, 18, 0, 0},
    {"hw.l2cachesize", T_QUAD, 0, V_L2, 0, 20, 0, 0},
    {"hw.l3cachesize", T_QUAD, 0, V_L3, 0, 22, 0, 0},
    {"hw.tbfrequency", T_QUAD, 0, V_FIX, 0, 23, 1000000000ull, 0},   /* mach_timebase_info 1/1 (libc.c) */
    {"hw.memsize", T_QUAD, 0, V_MEMSIZE, 0, 24, 0, 0},
    {"hw.activecpu", T_INT, 0, V_NCPU, 0, 25, 0, 0},
    {"hw.targettype", T_STR, 0, V_FIX, 0, 26, 0, "mOS"},
    {"hw.pagesize", T_QUAD, 0, V_FIX, 0, 0, 4096, 0},
    {"hw.cpufrequency_max", T_QUAD, 0, V_CPUFREQ, 0, 0, 0, 0},
    {"hw.cpufrequency_min", T_QUAD, 0, V_CPUFREQ, 0, 0, 0, 0},
    {"hw.busfrequency_max", T_QUAD, 0, V_FIX, 0, 0, 100000000ull, 0},
    {"hw.busfrequency_min", T_QUAD, 0, V_FIX, 0, 0, 100000000ull, 0},
    {"hw.logicalcpu", T_INT, 0, V_NCPU, 0, 0, 0, 0},
    {"hw.logicalcpu_max", T_INT, 0, V_NCPU, 0, 0, 0, 0},
    {"hw.physicalcpu", T_INT, 0, V_PHYS, 0, 0, 0, 0},
    {"hw.physicalcpu_max", T_INT, 0, V_PHYS, 0, 0, 0, 0},
    {"hw.packages", T_INT, 0, V_FIX, 0, 0, 1, 0},
    {"hw.cputype", T_INT, 0, V_FIX, 0, 0, 0x01000007, 0},
    {"hw.cpusubtype", T_INT, 0, V_FIX, 0, 0, 3, 0},
    {"hw.cpu64bit_capable", T_INT, 0, V_FIX, 0, 0, 1, 0},
    {"hw.cpufamily", T_INT, 0, V_FIX, 0, 0, 0, 0},                /* CPUFAMILY_UNKNOWN: não há código da Apple para AMD */
    {"hw.cacheconfig", T_STRUCT, 0, V_CACHECONF, 0, 0, 0, 0},
    {"hw.cachesize", T_STRUCT, 0, V_CACHESIZE, 0, 0, 0, 0},
    {"hw.nperflevels", T_INT, 0, V_FIX, 0, 0, 1, 0},
    {"hw.perflevel0.logicalcpu", T_INT, 0, V_NCPU, 0, 0, 0, 0},
    {"hw.perflevel0.logicalcpu_max", T_INT, 0, V_NCPU, 0, 0, 0, 0},
    {"hw.perflevel0.physicalcpu", T_INT, 0, V_PHYS, 0, 0, 0, 0},
    {"hw.perflevel0.physicalcpu_max", T_INT, 0, V_PHYS, 0, 0, 0, 0},
    {"hw.perflevel0.name", T_STR, 0, V_FIX, 0, 0, 0, "Performance"},
    {"hw.optional.floatingpoint", T_INT, 0, V_FIX, 0, 0, 1, 0},
    {"hw.optional.x86_64", T_INT, 0, V_FIX, 0, 0, 1, 0},
    {"hw.optional.arm64", T_INT, 0, V_FIX, 0, 0, 0, 0},
    {"hw.optional.mmx", T_INT, 0, V_OPT, O_MMX, 0, 0, 0},
    {"hw.optional.sse", T_INT, 0, V_OPT, O_SSE, 0, 0, 0},
    {"hw.optional.sse2", T_INT, 0, V_OPT, O_SSE2, 0, 0, 0},
    {"hw.optional.sse3", T_INT, 0, V_OPT, O_SSE3, 0, 0, 0},
    {"hw.optional.supplementalsse3", T_INT, 0, V_OPT, O_SSSE3, 0, 0, 0},
    {"hw.optional.sse4_1", T_INT, 0, V_OPT, O_SSE41, 0, 0, 0},
    {"hw.optional.sse4_2", T_INT, 0, V_OPT, O_SSE42, 0, 0, 0},
    {"hw.optional.aes", T_INT, 0, V_OPT, O_AES, 0, 0, 0},
    {"hw.optional.avx1_0", T_INT, 0, V_OPT, O_AVX, 0, 0, 0},
    {"hw.optional.avx2_0", T_INT, 0, V_OPT, O_AVX2, 0, 0, 0},
    {"hw.optional.rdrand", T_INT, 0, V_OPT, O_RDRAND, 0, 0, 0},
    {"hw.optional.f16c", T_INT, 0, V_OPT, O_F16C, 0, 0, 0},
    {"hw.optional.fma", T_INT, 0, V_OPT, O_FMA, 0, 0, 0},
    {"hw.optional.bmi1", T_INT, 0, V_OPT, O_BMI1, 0, 0, 0},
    {"hw.optional.bmi2", T_INT, 0, V_OPT, O_BMI2, 0, 0, 0},
    {"hw.optional.rtm", T_INT, 0, V_OPT, O_RTM, 0, 0, 0},
    {"hw.optional.hle", T_INT, 0, V_OPT, O_HLE, 0, 0, 0},
    {"hw.optional.adx", T_INT, 0, V_OPT, O_ADX, 0, 0, 0},
    {"hw.optional.enfstrg", T_INT, 0, V_OPT, O_ERMS, 0, 0, 0},
    {"hw.optional.popcnt", T_INT, 0, V_OPT, O_POPCNT, 0, 0, 0},
    {"hw.optional.avx512f", T_INT, 0, V_OPT, O_AVX512F, 0, 0, 0},
    {"hw.optional.avx512cd", T_INT, 0, V_OPT, O_AVX512CD, 0, 0, 0},
    {"hw.optional.avx512dq", T_INT, 0, V_OPT, O_AVX512DQ, 0, 0, 0},
    {"hw.optional.avx512bw", T_INT, 0, V_OPT, O_AVX512BW, 0, 0, 0},
    {"hw.optional.avx512vl", T_INT, 0, V_OPT, O_AVX512VL, 0, 0, 0},
    {"hw.optional.avx512ifma", T_INT, 0, V_OPT, O_AVX512IFMA, 0, 0, 0},
    {"hw.optional.sha", T_INT, 0, V_OPT, O_SHA, 0, 0, 0},
    {"hw.optional.sgx", T_INT, 0, V_OPT, O_SGX, 0, 0, 0},
    {"hw.optional.mpx", T_INT, 0, V_OPT, O_MPX, 0, 0, 0},
    /* CTL_MACHDEP = 7 */
    {"machdep.cpu.brand_string", T_STR, 0, V_BRAND, 0, 0, 0, 0},
    {"machdep.cpu.vendor", T_STR, 0, V_VENDOR, 0, 0, 0, 0},
    {"machdep.cpu.family", T_INT, 0, V_FAMILY, 0, 0, 0, 0},
    {"machdep.cpu.model", T_INT, 0, V_MODEL, 0, 0, 0, 0},
    {"machdep.cpu.extmodel", T_INT, 0, V_EXTMODEL, 0, 0, 0, 0},
    {"machdep.cpu.extfamily", T_INT, 0, V_EXTFAM, 0, 0, 0, 0},
    {"machdep.cpu.stepping", T_INT, 0, V_STEPPING, 0, 0, 0, 0},
    {"machdep.cpu.signature", T_INT, 0, V_SIG, 0, 0, 0, 0},
    {"machdep.cpu.feature_bits", T_QUAD, 0, V_FEATBITS, 0, 0, 0, 0},
    {"machdep.cpu.leaf7_feature_bits", T_QUAD, 0, V_L7BITS, 0, 0, 0, 0},
    {"machdep.cpu.extfeature_bits", T_QUAD, 0, V_EXTFEATBITS, 0, 0, 0, 0},
    {"machdep.cpu.features", T_STR, 0, V_FEATS, 0, 0, 0, 0},
    {"machdep.cpu.leaf7_features", T_STR, 0, V_L7FEATS, 0, 0, 0, 0},
    {"machdep.cpu.extfeatures", T_STR, 0, V_EXTFEATS, 0, 0, 0, 0},
    {"machdep.cpu.max_basic", T_INT, 0, V_MAXBASIC, 0, 0, 0, 0},
    {"machdep.cpu.max_ext", T_INT, 0, V_MAXEXT, 0, 0, 0, 0},
    {"machdep.cpu.microcode_version", T_INT, 0, V_FIX, 0, 0, 0, 0},
    {"machdep.cpu.core_count", T_INT, 0, V_PHYS, 0, 0, 0, 0},
    {"machdep.cpu.thread_count", T_INT, 0, V_NCPU, 0, 0, 0, 0},
    {"machdep.cpu.cores_per_package", T_INT, 0, V_TPC_CORES, 0, 0, 0, 0},
    {"machdep.cpu.logical_per_package", T_INT, 0, V_LOGICAL_PKG, 0, 0, 0, 0},
    {"machdep.tsc.frequency", T_QUAD, 0, V_TSCFREQ, 0, 0, 0, 0},
    /* CTL_USER = 8 */
    {"user.cs_path", T_STR, 0, V_FIX, 0, 1, 0, "/usr/bin:/bin:/usr/sbin:/sbin"},
    {"user.bc_base_max", T_INT, 0, V_FIX, 0, 2, 99, 0},
    {"user.bc_dim_max", T_INT, 0, V_FIX, 0, 3, 2048, 0},
    {"user.bc_scale_max", T_INT, 0, V_FIX, 0, 4, 99, 0},
    {"user.bc_string_max", T_INT, 0, V_FIX, 0, 5, 1000, 0},
    {"user.coll_weights_max", T_INT, 0, V_FIX, 0, 6, 2, 0},
    {"user.expr_nest_max", T_INT, 0, V_FIX, 0, 7, 32, 0},
    {"user.line_max", T_INT, 0, V_FIX, 0, 8, 2048, 0},
    {"user.re_dup_max", T_INT, 0, V_FIX, 0, 9, 255, 0},
    {"user.posix2_version", T_INT, 0, V_FIX, 0, 10, 200112, 0},
    {"user.stream_max", T_INT, 0, V_FIX, 0, 19, 20, 0},
    {"user.tzname_max", T_INT, 0, V_FIX, 0, 20, 255, 0},
    /* OID_AUTO fora da árvore numérica clássica */
    {"sysctl.proc_translated", T_INT, 0, V_FIX, 0, 0, 0, 0},
    {"sysctl.proc_native", T_INT, 0, V_FIX, 0, 0, 1, 0},
    {"security.mac.lockdown_mode_state", T_INT, 0, V_FIX, 0, 0, 0, 0},
    {"net.inet.ip.portrange.first", T_INT, 0, V_FIX, 0, 0, 49152, 0},
    {"net.inet.ip.portrange.last", T_INT, 0, V_FIX, 0, 0, 65535, 0},
    {"net.inet.ip.portrange.hifirst", T_INT, 0, V_FIX, 0, 0, 49152, 0},
    {"net.inet.ip.portrange.hilast", T_INT, 0, V_FIX, 0, 0, 65535, 0},
    {"net.inet.tcp.mssdflt", T_INT, 0, V_FIX, 0, 0, 512, 0},
};
#define NTAB ((int)(sizeof tab / sizeof tab[0]))

static int top_of(const char *n) {
    static const struct { const char *s; int id; } t[] = {
        {"sysctl", 0}, {"kern", 1}, {"vm", 2}, {"vfs", 3}, {"net", 4}, {"debug", 5}, {"hw", 6}, {"machdep", 7},
        {"user", 8}, {"security", 9},
    };
    for (u32 i = 0; i < sizeof t / sizeof t[0]; i++) {
        size_t k = strlen(t[i].s);
        if (!strncmp(n, t[i].s, k) && n[k] == '.') return t[i].id;
    }
    return 10;
}
/* MIB de uma entrada: {topo, mib1} ou {topo, 1000 + índice} (estável entre versões só pelo nome; use o nome) */
static void ent_mib(int i, int m[2]) { m[0] = top_of(tab[i].name); m[1] = tab[i].mib1 ? tab[i].mib1 : 1000 + i; }

static int opt_val(int o) {
    cpu_init();
    u32 e = cpu.l1_edx, c = cpu.l1_ecx, b = cpu.l7_ebx;
    switch (o) {
    case O_MMX: return !!(e & (1u << 23));      case O_SSE: return !!(e & (1u << 25));
    case O_SSE2: return !!(e & (1u << 26));     case O_SSE3: return !!(c & 1u);
    case O_SSSE3: return !!(c & (1u << 9));     case O_SSE41: return !!(c & (1u << 19));
    case O_SSE42: return !!(c & (1u << 20));    case O_AES: return !!(c & (1u << 25));
    case O_POPCNT: return !!(c & (1u << 23));   case O_RDRAND: return !!(c & (1u << 30));
    case O_AVX: return ymm_ok();
    case O_F16C: return ymm_ok() && (c & (1u << 29));
    case O_FMA: return ymm_ok() && (c & (1u << 12));
    case O_AVX2: return ymm_ok() && (b & (1u << 5));
    case O_BMI1: return !!(b & (1u << 3));      case O_BMI2: return !!(b & (1u << 8));
    case O_HLE: return !!(b & (1u << 4));       case O_RTM: return !!(b & (1u << 11));
    case O_ADX: return !!(b & (1u << 19));      case O_ERMS: return !!(b & (1u << 9));
    case O_SHA: return !!(b & (1u << 29));
    case O_AVX512F: return zmm_ok() && (b & (1u << 16));
    case O_AVX512DQ: return zmm_ok() && (b & (1u << 17));
    case O_AVX512IFMA: return zmm_ok() && (b & (1u << 21));
    case O_AVX512CD: return zmm_ok() && (b & (1u << 28));
    case O_AVX512BW: return zmm_ok() && (b & (1u << 30));
    case O_AVX512VL: return zmm_ok() && (b & (1u << 31));
    default: return 0;                          /* SGX e MPX: o kernel não habilita */
    }
}

/* valor da entrada em out (até 512 bytes); devolve o tamanho */
static size_t ent_value(const sc_ent_t *e, u8 *out) {
    u64 v = e->num;
    const char *s = e->str;
    char sbuf[512];
    mos_dw_sysinfo_t si;
    switch (e->v) {
    case V_FIX: break;
    case V_NCPU: v = ncpu_now(); break;
    case V_PHYS: v = phys_cpus(); break;
    case V_TPC_CORES: v = phys_cpus(); break;
    case V_LOGICAL_PKG: v = ncpu_now(); break;
    case V_MEMSIZE: dw_sysinfo(&si); v = si.mem_total; break;
    case V_PHYSMEM: dw_sysinfo(&si); v = si.mem_total > 0x7fffffffull ? 0x7fffffffull : si.mem_total; break;
    case V_USERMEM: dw_sysinfo(&si); v = si.mem_free > 0x7fffffffull ? 0x7fffffffull : si.mem_free; break;
    case V_FREEPAGES: dw_sysinfo(&si); v = si.mem_free / 4096; break;
    case V_NUMTASKS: dw_sysinfo(&si); v = si.nprocs; break;
    case V_NUMTHREADS: dw_sysinfo(&si); v = si.nthreads; break;
    case V_MSLEVEL: dw_sysinfo(&si); v = si.mem_total ? si.mem_free * 100 / si.mem_total : 100; break;
    case V_PRESSURE: {   /* 1 normal, 2 aviso, 4 crítico (kVMPressure* do XNU) */
        mos_vmstats_t vs;   /* o nível do próprio kernel (piso do vm_pageout, vmpolicy.h), o mesmo que dispara o jetsam */
        if (SYS2(SYS_VMCTL, MOS_VM_STATS, &vs) == 0) { v = vs.pressure_level; break; }
        dw_sysinfo(&si);
        u64 pct = si.mem_total ? si.mem_free * 100 / si.mem_total : 100;
        v = pct > 15 ? 1 : pct > 5 ? 2 : 4;
        break;
    }
    case V_SAFEBOOT: v = 0; break;
    case V_HOSTNAME: {
        long n = SYS3(SYS_DARWIN, MOS_DW_HOSTNAME, sbuf, sizeof sbuf);
        if (n < 0) memcpy(sbuf, "mOS", 4);
        else sbuf[n < (long)sizeof sbuf ? n : (long)sizeof sbuf - 1] = 0;
        s = sbuf;
        break;
    }
    case V_BOOTTIME: {   /* struct timeval {long tv_sec; int tv_usec;} */
        dw_sysinfo(&si);
        u64 b = si.boot_unix_us;
        if (!b) { sc_ts_t t = {0, 0}; clock_gettime(0, &t); b = (u64)t.s * 1000000ull - si.uptime_ms * 1000; }
        memset(out, 0, 16);
        i64 sec = (i64)(b / 1000000); i32 us = (i32)(b % 1000000);
        memcpy(out, &sec, 8); memcpy(out + 8, &us, 4);
        return 16;
    }
    case V_CLOCKRATE: {  /* struct clockinfo {hz, tick, tickadj, stathz, profhz}: relógio de 1 ms (timer_ticks) */
        i32 c[5] = {1000, 1000, 1, 1000, 1000};
        memcpy(out, c, sizeof c);
        return sizeof c;
    }
    case V_SWAP: {       /* struct xsw_usage {u64 total, avail, used; u32 pagesize; u32 encrypted} */
        dw_sysinfo(&si);
        u64 x[3] = {si.swap_total, si.swap_total - si.swap_used, si.swap_used};
        u32 y[2] = {4096, 0};
        memcpy(out, x, 24); memcpy(out + 24, y, 8);
        return 32;
    }
    case V_LOADAVG: {    /* struct loadavg {u32 ldavg[3]; long fscale}: threads prontas agora × FSCALE (sem média móvel) */
        dw_sysinfo(&si);
        u32 l = si.nrunnable * 2048u;
        u32 a[4] = {l, l, l, 0};
        i64 fs = 2048;
        memcpy(out, a, 16); memcpy(out + 16, &fs, 8);
        return 24;
    }
    case V_CACHECONF: case V_CACHESIZE: {   /* u64[10]: [0] memória/CPUs, [1] L1, [2] L2, [3] L3 */
        cpu_init();
        u64 a[10];
        memset(a, 0, sizeof a);
        if (e->v == V_CACHECONF) { a[0] = ncpu_now(); a[1] = cpu.tpc; a[2] = cpu.l2_share; a[3] = cpu.l3 ? cpu.l3_share : 0; }
        else { dw_sysinfo(&si); a[0] = si.mem_total; a[1] = cpu.l1d; a[2] = cpu.l2; a[3] = cpu.l3; }
        memcpy(out, a, sizeof a);
        return sizeof a;
    }
    case V_CPUFREQ: case V_TSCFREQ: cpu_init(); v = cpu.tsc_hz; break;
    case V_L1D: cpu_init(); v = cpu.l1d; break;
    case V_L1I: cpu_init(); v = cpu.l1i; break;
    case V_L2: cpu_init(); v = cpu.l2; break;
    case V_L3: cpu_init(); v = cpu.l3; break;
    case V_LINE: cpu_init(); v = cpu.line; break;
    case V_BRAND: cpu_init(); s = cpu.brand; break;
    case V_VENDOR: cpu_init(); s = cpu.vendor; break;
    case V_FAMILY: cpu_init(); v = cpu.family; break;
    case V_MODEL: cpu_init(); v = cpu.model; break;
    case V_EXTMODEL: cpu_init(); v = cpu.ext_model; break;
    case V_EXTFAM: cpu_init(); v = cpu.ext_family; break;
    case V_STEPPING: cpu_init(); v = cpu.stepping; break;
    case V_SIG: cpu_init(); v = cpu.sig; break;
    case V_MAXBASIC: cpu_init(); v = cpu.max_basic; break;
    case V_MAXEXT: cpu_init(); v = cpu.max_ext; break;
    case V_VMM: cpu_init(); v = !!(cpu.l1_ecx & (1u << 31)); break;
    case V_FEATBITS: cpu_init(); v = (u64)cpu.l1_edx | (u64)cpu.l1_ecx << 32; break;
    case V_L7BITS: cpu_init(); v = (u64)cpu.l7_ebx | (u64)cpu.l7_ecx << 32; break;
    case V_EXTFEATBITS: cpu_init(); v = (u64)cpu.e1_edx | (u64)cpu.e1_ecx << 32; break;
    case V_FEATS: cpu_init(); feat_str(sbuf, sizeof sbuf, f_edx, cpu.l1_edx, f_ecx, cpu.l1_ecx); s = sbuf; break;
    case V_L7FEATS: cpu_init(); feat_str(sbuf, sizeof sbuf, f_l7b, cpu.l7_ebx, 0, 0); s = sbuf; break;
    case V_EXTFEATS: {
        cpu_init();
        static const char *const xd[32] = {[11] = "SYSCALL", [20] = "XD", [26] = "1GBPAGE", [27] = "RDTSCP", [29] = "EM64T"};
        static const char *const xc[32] = {[0] = "LAHF", [5] = "LZCNT", [8] = "PREFETCHW"};
        feat_str(sbuf, sizeof sbuf, xd, cpu.e1_edx, xc, cpu.e1_ecx);
        if (cpu.e7_edx & (1u << 8)) { size_t n = strlen(sbuf); if (n + 6 < sizeof sbuf) memcpy(sbuf + n, " TSCI", 6); }
        s = sbuf;
        break;
    }
    case V_OPT: v = (u64)opt_val(e->a); break;
    }
    if (e->type == T_STR) { size_t n = strlen(s ? s : "") + 1; memcpy(out, s ? s : "", n); return n; }
    if (e->type == T_QUAD) { memcpy(out, &v, 8); return 8; }
    u32 w = (u32)v;
    memcpy(out, &w, 4);
    return 4;
}

static int put_bytes(const void *src, size_t n, void *old, size_t *oldlen) {
    if (!oldlen) return 0;
    if (!old) { *oldlen = n; return 0; }
    if (*oldlen < n) { cx_errno = CX_ENOMEM; return -1; }
    memcpy(old, src, n);
    *oldlen = n;
    return 0;
}

static int ent_io(const sc_ent_t *e, void *old, size_t *oldlen, const void *newp, size_t newlen) {
    if (newp) {
        if (!e->rw) { cx_errno = CX_EPERM; return -1; }
        if (e->v == V_HOSTNAME) {
            long r = SYS3(SYS_DARWIN, MOS_DW_SETHOSTNAME, newp, newlen);
            if (r < 0) { cx_errno = r == -MOS_EPERM ? CX_EPERM : CX_EINVAL; return -1; }
        }
        if (!old) { if (oldlen) *oldlen = 0; return 0; }
    }
    u8 buf[512];
    size_t n = ent_value(e, buf);
    if (e->type == T_QUAD && old && oldlen && *oldlen == 4) {   /* bigcheck do XNU: 64 bits num int, se couber */
        u64 v;
        memcpy(&v, buf, 8);
        if (v > 0xffffffffull) { cx_errno = CX_ENOMEM; return -1; }
        u32 w = (u32)v;
        memcpy(old, &w, 4);
        return 0;
    }
    return put_bytes(buf, n, old, oldlen);
}

/* ------------------------------------------------------------------ KERN_PROC e KERN_PROCARGS(2) */
enum { KP_SIZE = 648 };
static void kp_fill(int pid, u8 *k) {
    int mib[4] = {1, 14, 1, pid};
    size_t len = KP_SIZE;
    cx_kinfo_proc(mib, 4, k, &len);             /* nome, pai, grupo e terminal (pty.c) */
    mos_dw_proc_t d;
    if (SYS3(SYS_DARWIN, MOS_DW_PROC, pid, &d) != 0) return;
    i64 sec = (i64)(d.start_unix_us / 1000000); i32 us = (i32)(d.start_unix_us % 1000000);
    memcpy(k + 0, &sec, 8); memcpy(k + 8, &us, 4);                   /* p_starttime */
    u32 fl;
    memcpy(&fl, k + 32, 4);
    fl |= 0x4;                                                        /* P_LP64 */
    if (d.flags & 2) fl |= 0x2;                                       /* P_CONTROLT */
    memcpy(k + 32, &fl, 4);
    static const u8 st[] = {2, 2, 3, 4, 5};                           /* SRUN SRUN SSLEEP SSTOP SZOMB */
    k[36] = st[d.state >= 0 && d.state <= 4 ? d.state : 0];
    memcpy(k + 40, &d.pid, 4);
    size_t ln = strlen(d.name);
    memset(k + 243, 0, 17);
    memcpy(k + 243, d.name, ln > 16 ? 16 : ln);                       /* p_comm */
    memcpy(k + 392, &d.uid, 4); memcpy(k + 396, &d.suid, 4);          /* e_pcred: p_ruid, p_svuid */
    memcpy(k + 400, &d.gid, 4); memcpy(k + 404, &d.sgid, 4);          /* p_rgid, p_svgid */
    i32 one = 1;
    memcpy(k + 416, &one, 4);                                         /* e_ucred.cr_ref */
    memcpy(k + 420, &d.euid, 4);                                      /* cr_uid */
    i16 ng = 1;
    memcpy(k + 424, &ng, 2);
    memcpy(k + 426, &d.egid, 4);                                      /* cr_groups[0] */
    i32 ppid = d.ppid > 0 ? d.ppid : (d.pid == 1 ? 0 : 1);
    memcpy(k + 560, &ppid, 4);
    i32 pg;
    memcpy(&pg, k + 564, 4);
    if (!pg) memcpy(k + 564, &d.pgid, 4);
}

static int kern_proc(int *mib, unsigned n, void *old, size_t *oldlen) {
    if (n < 3) { cx_errno = CX_EINVAL; return -1; }
    int what = mib[2], arg = n >= 4 ? mib[3] : 0;
    if (what == 1) {                                                   /* KERN_PROC_PID: um só, 0 bytes se não existe */
        if (n < 4) { cx_errno = CX_EINVAL; return -1; }
        if (!oldlen) return 0;
        if (!old) { *oldlen = KP_SIZE; return 0; }
        if (*oldlen < KP_SIZE) { cx_errno = CX_ENOMEM; return -1; }
        mos_dw_proc_t d;
        long r = SYS3(SYS_DARWIN, MOS_DW_PROC, arg, &d);
        if (r == -MOS_ESRCH) { *oldlen = 0; return 0; }
        if (r != 0) return cx_kinfo_proc(mib, n, old, oldlen);       /* kernel sem SYS_DARWIN */
        memset(old, 0, KP_SIZE);
        kp_fill(arg, old);
        *oldlen = KP_SIZE;
        return 0;
    }
    if (what < 0 || what > 6) { cx_errno = CX_EINVAL; return -1; }
    int pids[1024];
    long np = SYS3(SYS_DARWIN, MOS_DW_PIDS, pids, 1024);
    if (np < 0) {                                                      /* sem SYS_DARWIN: só o próprio processo */
        np = 1;
        pids[0] = (int)SYS0(SYS_GETPID);
    }
    if (np > 1024) np = 1024;
    if (!oldlen) return 0;
    size_t cap = old ? *oldlen : 0, used = 0, need = 0;
    u8 k[KP_SIZE];
    for (long i = 0; i < np; i++) {
        memset(k, 0, KP_SIZE);
        kp_fill(pids[i], k);
        i32 v = 0;
        int keep = 1;
        switch (what) {
        case 2: memcpy(&v, k + 564, 4); keep = v == arg; break;       /* PGRP */
        case 3: { mos_dw_proc_t d; keep = SYS3(SYS_DARWIN, MOS_DW_PROC, pids[i], &d) == 0 && d.sid == arg; break; }
        case 4: memcpy(&v, k + 572, 4); keep = v == arg; break;       /* TTY (dev_t) */
        case 5: memcpy(&v, k + 420, 4); keep = v == arg; break;       /* UID efetivo */
        case 6: memcpy(&v, k + 392, 4); keep = v == arg; break;       /* RUID */
        }
        if (!keep) continue;
        need += KP_SIZE;
        if (old && used + KP_SIZE <= cap) { memcpy((u8 *)old + used, k, KP_SIZE); used += KP_SIZE; }
    }
    if (!old) { *oldlen = need + 5 * KP_SIZE; return 0; }             /* KERN_PROCSLOP */
    *oldlen = used;
    if (used < need) { cx_errno = CX_ENOMEM; return -1; }
    return 0;
}

static int kern_procargs(int pid, int v2, void *old, size_t *oldlen) {
    if (!oldlen) return 0;
    long need = SYS4(SYS_DARWIN, MOS_DW_ARGS, pid, 0, 0);
    if (need == -MOS_ESRCH) { cx_errno = CX_EINVAL; return -1; }       /* o XNU dá EINVAL para pid inexistente */
    if (need == -MOS_EPERM) { cx_errno = CX_EPERM; return -1; }
    if (need < 0) {                                                    /* sem SYS_DARWIN: só o próprio, pelo NXArgv */
        extern char **NXArgv;
        extern int NXArgc;
        if (pid != (int)SYS0(SYS_GETPID) || !NXArgv) { cx_errno = CX_EINVAL; return -1; }
        u8 tmp[4096];
        size_t o = 0;
        i32 ac = NXArgc;
        if (v2) { memcpy(tmp, &ac, 4); o = 4; }
        const char *p0 = NXArgv[0] ? NXArgv[0] : "";
        size_t l = strlen(p0) + 1;
        if (o + l > sizeof tmp) l = sizeof tmp - o;
        memcpy(tmp + o, p0, l); o += l;
        while (o & 7) tmp[o++] = 0;
        for (int i = 0; i < ac && NXArgv[i]; i++) {
            size_t m = strlen(NXArgv[i]) + 1;
            if (o + m > sizeof tmp) break;
            memcpy(tmp + o, NXArgv[i], m); o += m;
        }
        return put_bytes(tmp, o, old, oldlen);
    }
    size_t want = (size_t)need;
    if (!old) { *oldlen = want; return 0; }
    if (*oldlen < want) { cx_errno = CX_ENOMEM; return -1; }
    long r = SYS4(SYS_DARWIN, MOS_DW_ARGS, pid, old, *oldlen);
    if (r < 0) { cx_errno = r == -MOS_EPERM ? CX_EPERM : CX_EFAULT; return -1; }
    if (!v2) {                                                         /* KERN_PROCARGS: sem o argc na frente */
        u8 *b = old;
        for (long i = 4; i < r; i++) b[i - 4] = b[i];
        r -= 4;
    }
    *oldlen = (size_t)r;
    return 0;
}

/* ------------------------------------------------------------------ OIDs internos {0, x} */
static int mib_cmp(const int *a, unsigned na, const int *b, unsigned nb) {
    for (unsigned i = 0; i < na && i < nb; i++) if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return na < nb ? -1 : na > nb ? 1 : 0;
}
static int find_mib(const int *mib, unsigned n) {
    if (n != 2) return -1;
    for (int i = 0; i < NTAB; i++) { int m[2]; ent_mib(i, m); if (m[0] == mib[0] && m[1] == mib[1]) return i; }
    return -1;
}
static int find_name(const char *name) {
    for (int i = 0; i < NTAB; i++) if (!strcmp(tab[i].name, name)) return i;
    return -1;
}
/* nomes de nós dinâmicos que o XNU também resolve pelo {0,3} */
static const struct { const char *s; int n; int m[3]; } dyn_names[] = {
    {"kern.proc", 2, {1, 14}}, {"kern.proc.all", 3, {1, 14, 0}}, {"kern.proc.pid", 3, {1, 14, 1}},
    {"kern.proc.pgrp", 3, {1, 14, 2}}, {"kern.proc.session", 3, {1, 14, 3}}, {"kern.proc.tty", 3, {1, 14, 4}},
    {"kern.proc.uid", 3, {1, 14, 5}}, {"kern.proc.ruid", 3, {1, 14, 6}},
    {"kern.procargs", 2, {1, 38}}, {"kern.procargs2", 2, {1, 49}},
    {"kern", 1, {1}}, {"vm", 1, {2}}, {"hw", 1, {6}}, {"machdep", 1, {7}}, {"user", 1, {8}},
};

static int name2oid(const char *name, int *mib, size_t *n) {
    int i = find_name(name);
    if (i >= 0) {
        if (*n < 2) { cx_errno = CX_ENOMEM; return -1; }
        ent_mib(i, mib);
        *n = 2;
        return 0;
    }
    for (u32 j = 0; j < sizeof dyn_names / sizeof dyn_names[0]; j++)
        if (!strcmp(dyn_names[j].s, name)) {
            if (*n < (size_t)dyn_names[j].n) { cx_errno = CX_ENOMEM; return -1; }
            for (int k = 0; k < dyn_names[j].n; k++) mib[k] = dyn_names[j].m[k];
            *n = (size_t)dyn_names[j].n;
            return 0;
        }
    cx_errno = CX_ENOENT;
    return -1;
}

static int internal(int *mib, unsigned n, void *old, size_t *oldlen, const void *newp, size_t newlen) {
    switch (mib[1]) {
    case 1: {   /* {0,1,<mib>}: nome */
        int i = find_mib(mib + 2, n - 2);
        if (i < 0) {
            for (u32 j = 0; j < sizeof dyn_names / sizeof dyn_names[0]; j++)
                if (!mib_cmp(mib + 2, n - 2, dyn_names[j].m, (unsigned)dyn_names[j].n))
                    return put_bytes(dyn_names[j].s, strlen(dyn_names[j].s) + 1, old, oldlen);
            cx_errno = CX_ENOENT;
            return -1;
        }
        return put_bytes(tab[i].name, strlen(tab[i].name) + 1, old, oldlen);
    }
    case 2: {   /* {0,2,<mib>}: próxima folha em ordem de MIB */
        int best = -1, bm[2] = {0, 0};
        for (int i = 0; i < NTAB; i++) {
            int m[2];
            ent_mib(i, m);
            if (n > 2 && mib_cmp(m, 2, mib + 2, n - 2) <= 0) continue;
            if (best < 0 || mib_cmp(m, 2, bm, 2) < 0) { best = i; bm[0] = m[0]; bm[1] = m[1]; }
        }
        if (best < 0) { cx_errno = CX_ENOENT; return -1; }
        return put_bytes(bm, sizeof bm, old, oldlen);
    }
    case 3: {   /* {0,3}: nome (em newp) -> MIB */
        if (!newp || !newlen || newlen > 256) { cx_errno = CX_EINVAL; return -1; }
        char nm[257];
        memcpy(nm, newp, newlen);
        nm[newlen] = 0;
        int out[4];
        size_t cnt = 4;
        if (name2oid(nm, out, &cnt) < 0) return -1;
        return put_bytes(out, cnt * sizeof(int), old, oldlen);
    }
    case 4: {   /* {0,4,<mib>}: u32 tipo|flags + formato */
        int i = find_mib(mib + 2, n - 2);
        u8 buf[32];
        u32 kind;
        const char *fmt;
        if (i < 0) {
            int dyn = 0;
            for (u32 j = 0; j < sizeof dyn_names / sizeof dyn_names[0]; j++)
                if (!mib_cmp(mib + 2, n - 2, dyn_names[j].m, (unsigned)dyn_names[j].n)) dyn = 1;
            if (!dyn) { cx_errno = CX_ENOENT; return -1; }
            kind = T_NODE | 0x80000000u; fmt = "N";
        } else {
            kind = tab[i].type | 0x80000000u | (tab[i].rw ? 0x40000000u : 0);
            fmt = tab[i].type == T_STR ? "A" : tab[i].type == T_QUAD ? "Q" : tab[i].type == T_INT ? "I" :
                  tab[i].v == V_BOOTTIME ? "S,timeval" : tab[i].v == V_LOADAVG ? "S,loadavg" :
                  tab[i].v == V_SWAP ? "S,xsw_usage" : tab[i].v == V_CLOCKRATE ? "S,clockinfo" : "Q";
        }
        size_t fl = strlen(fmt) + 1;
        memcpy(buf, &kind, 4);
        memcpy(buf + 4, fmt, fl);
        return put_bytes(buf, 4 + fl, old, oldlen);
    }
    }
    cx_errno = CX_ENOENT;
    return -1;
}

/* ------------------------------------------------------------------ API pública */
int sysctl(int *mib, unsigned n, void *old, size_t *oldlen, void *newp, size_t newlen) {
    if (!mib || n < 2 || n > 12) { cx_errno = CX_EINVAL; return -1; }
    if (mib[0] == 0) return internal(mib, n, old, oldlen, newp, newlen);
    if (mib[0] == 1 && mib[1] == 14) return kern_proc(mib, n, old, oldlen);
    if (mib[0] == 1 && (mib[1] == 49 || mib[1] == 38)) {
        if (n < 3) { cx_errno = CX_EINVAL; return -1; }
        return kern_procargs(mib[2], mib[1] == 49, old, oldlen);
    }
    int i = find_mib(mib, n);
    if (i < 0) { cx_errno = n == 2 ? CX_ENOENT : CX_ENOTDIR; return -1; }
#if defined(__aarch64__)
    { extern int cx_sysctl_arm64(const char *, void *, unsigned long *);
      int r = cx_sysctl_arm64(tab[i].name, old, (unsigned long *)oldlen);
      if (r <= 0) { if (r < 0) { cx_errno = CX_ENOMEM; return -1; } return 0; } }
#endif
    return ent_io(&tab[i], old, oldlen, newp, newlen);
}

int sysctlbyname(const char *name, void *old, size_t *oldlen, void *newp, size_t newlen) {
    if (!name) { cx_errno = CX_EINVAL; return -1; }
#if defined(__aarch64__)   /* processo arm64: as chaves do Apple Silicon (darwin_arm64.c) antes das do x86 */
    { extern int cx_sysctl_arm64(const char *, void *, unsigned long *);
      int r = cx_sysctl_arm64(name, old, (unsigned long *)oldlen);
      if (r <= 0) { if (r < 0) { cx_errno = CX_ENOMEM; return -1; } return 0; } }
#endif
    int i = find_name(name);
    if (i >= 0) return ent_io(&tab[i], old, oldlen, newp, newlen);
    int mib[12];
    size_t cnt = 12;
    if (name2oid(name, mib, &cnt) == 0) return sysctl(mib, (unsigned)cnt, old, oldlen, newp, newlen);
    cx_fdprintf(2, "[sysctl] chave desconhecida no mOS: %s\n", name);
    cx_errno = CX_ENOENT;
    return -1;
}

int sysctlnametomib(const char *name, int *mib, size_t *n) {
    if (!name || !mib || !n) { cx_errno = CX_EINVAL; return -1; }
    return name2oid(name, mib, n);
}
