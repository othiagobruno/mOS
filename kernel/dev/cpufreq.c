/* mOS — P-states da CPU (ACPI _PSS/_PCT/_PPC) e limitação térmica passiva
 *
 * Ryzen (família 17h+): _PCT aponta para FFixedHW 0xC0010062 (PStateCtl); o valor de controle do _PSS
 * (0, 1, 2 no R2544 = 3350/2000/1600 MHz) é o número do P-state. Cada núcleo tem o próprio MSR: o BSP
 * escreve em cpufreq_poll() e os APs no tique do LAPIC (cpufreq_ap_tick, vetor 0xF0), que também mede
 * a carga do núcleo por MPERF (conta só em C0) contra o TSC.
 *
 * Política a cada 50 ms: a maior carga entre os núcleos decide (≥ 70 % sobe ao P0 já; ≤ 30 % desce um
 * estado), limitada pelo _PPC do firmware e pelo teto térmico. Teto térmico (ACPI 11.1.5): com a zona em
 * _PSV ou acima, ΔP = _TC1·(Tn − Tn−1) + _TC2·(Tn − Tpsv) em décimos de grau decide descer ou subir um
 * estado a cada _TSP. _PCT em SystemIO (outras máquinas) escreve o valor na porta.
 * Sem _PSS, lê os P-states direto dos MSRs 0xC0010064.. (bit 63 = habilitado).
 */
#include "kernel.h"
#include "aml.h"

#define MAX_PS 8
static struct {
    int n;
    u32 mhz[MAX_PS], mw[MAX_PS];
    u64 ctl[MAX_PS];
    u8 space;                 /* 0x7F FFixedHW (MSR), 1 SystemIO */
    u64 addr; u8 width;
    int ppc;                  /* limite do firmware (índice mínimo) */
    int therm;                /* teto térmico (índice mínimo) */
    bool ok;
    aml_node_t *cpu, *tz;
    int tc1, tc2, tsp, last_tz;
} P;

static volatile int g_target;                  /* índice do P-state (0 = mais rápido) */
static volatile int g_applied[MAX_CPUS];
static volatile u32 g_load[MAX_CPUS];          /* carga em ‰ do último intervalo */
static u64 g_mperf0[MAX_CPUS], g_tsc0[MAX_CPUS], g_t0[MAX_CPUS];
static u32 g_switches;
static bool g_forced;                          /* repouso: P-state mais baixo até acordar */

static inline u64 rdmsr(u32 m) { u32 lo, hi; __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(m)); return (u64)hi << 32 | lo; }
static inline void wrmsr(u32 m, u64 v) { __asm__ volatile("wrmsr" :: "c"(m), "a"((u32)v), "d"((u32)(v >> 32))); }
static inline u64 tsc(void) { u32 lo, hi; __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi)); return (u64)hi << 32 | lo; }

static bool cpuid_amd_zen(bool *aperf) {
    u32 a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
    bool amd = b == 0x68747541;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(6));
    *aperf = c & 1;                            /* APERF/MPERF */
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
    u32 fam = ((a >> 8) & 0xF) + ((a >> 20) & 0xFF);
    return amd && fam >= 0x17;
}
static bool g_zen, g_aperf;

static void apply_here(int idx) {
    if (!P.ok || idx < 0 || idx >= P.n) return;
    if (P.space == 0x7F) { if (g_zen) wrmsr((u32)P.addr, P.ctl[idx]); }
    else if (P.space == 1) {
        u16 port = (u16)P.addr;
        if (P.width <= 8) outb(port, (u8)P.ctl[idx]); else if (P.width <= 16) outw(port, (u16)P.ctl[idx]); else outl(port, (u32)P.ctl[idx]);
    }
}

static void measure(int cpu) {
    if (!g_aperf || cpu < 0 || cpu >= MAX_CPUS) return;
    u64 t = tsc(), m = rdmsr(0xE7);
    if (g_tsc0[cpu] && t > g_tsc0[cpu]) {
        u64 dt = t - g_tsc0[cpu], dm = m - g_mperf0[cpu];
        g_load[cpu] = (u32)(dm >= dt ? 1000 : dm * 1000 / dt);
    }
    g_tsc0[cpu] = t; g_mperf0[cpu] = m;
}

/* APs: no tique de 1 ms do LAPIC */
void cpufreq_ap_tick(void) {
    int c = this_cpu();
    if (!P.ok || c <= 0 || c >= MAX_CPUS) return;
    int t = g_target;
    if (g_applied[c] != t) { apply_here(t); g_applied[c] = t; }
    u64 now = timer_ticks();
    if (now - g_t0[c] >= 50) { g_t0[c] = now; measure(c); }
}

static int lim(void) { int m = P.ppc > P.therm ? P.ppc : P.therm; return m < P.n ? m : P.n - 1; }

static void set_target(int idx) {
    int m = lim();
    if (idx < m) idx = m;
    if (idx >= P.n) idx = P.n - 1;
    if (idx != g_target) { g_target = idx; g_switches++; }
}

static int pkg_int(aml_val_t *pkg, u32 i, u64 *out) {
    if (pkg->type != AML_PKG || i >= pkg->b->len) return -1;
    return aml_val_to_int(&pkg->b->el[i], out);
}

/* registro genérico (ACPI 6.4.3.7): 0x82, len, space, width, offset, access, addr64 */
static bool parse_reg(aml_val_t *v, u8 *space, u64 *addr, u8 *width) {
    if (v->type != AML_BUF || v->b->len < 15 || v->b->d[0] != 0x82) return false;
    const u8 *d = v->b->d;
    *space = d[3]; *width = d[4];
    u64 a = 0; for (int i = 0; i < 8; i++) a |= (u64)d[7 + i] << (8 * i);
    *addr = a;
    return true;
}

typedef struct { aml_node_t *first; } find_t;
static int find_cpu(aml_node_t *n, int d, void *c) {
    (void)d;
    find_t *f = c;
    if (f->first) return -1;
    if ((n->type == AML_PROCESSOR || (n->type == AML_DEVICE && aml_device_match(n, "ACPI0007"))) && aml_find(n, "_PSS")) { f->first = n; return -1; }
    return 0;
}
static int find_tz(aml_node_t *n, int d, void *c) {
    (void)d;
    aml_node_t **t = c;
    if (n->type == AML_THERMAL && aml_find(n, "_PSV")) { *t = n; return -1; }
    return 0;
}

static bool from_msrs(void) {
    if (!g_zen) return false;
    u32 maxv = (u32)((rdmsr(0xC0010061) >> 4) & 7);
    for (u32 i = 0; i <= maxv && i < MAX_PS; i++) {
        u64 v = rdmsr(0xC0010064 + i);
        if (!(v >> 63)) break;
        u32 fid = (u32)(v & 0xFF), did = (u32)((v >> 8) & 0x3F);
        if (!did) break;
        P.mhz[P.n] = fid * 200 / did; P.mw[P.n] = 0; P.ctl[P.n] = i; P.n++;
    }
    P.space = 0x7F; P.addr = 0xC0010062; P.width = 64;
    return P.n > 0;
}

void cpufreq_init(void) {
    g_zen = cpuid_amd_zen(&g_aperf);
    memset(&P, 0, sizeof P);
    find_t f = { NULL };
    if (acpi_ready()) aml_walk(NULL, find_cpu, &f);
    P.cpu = f.first;
    if (P.cpu) {
        aml_val_t pss, pct;
        if (aml_eval_path(P.cpu, "_PSS", NULL, 0, &pss) == AML_OK && pss.type == AML_PKG) {
            for (u32 i = 0; i < pss.b->len && P.n < MAX_PS; i++) {
                aml_val_t *e = &pss.b->el[i];
                u64 mhz, mw, ctl;
                if (pkg_int(e, 0, &mhz) || pkg_int(e, 1, &mw) || pkg_int(e, 4, &ctl)) continue;
                P.mhz[P.n] = (u32)mhz; P.mw[P.n] = (u32)mw; P.ctl[P.n] = ctl; P.n++;
            }
        }
        aml_val_free(&pss);
        if (aml_eval_path(P.cpu, "_PCT", NULL, 0, &pct) == AML_OK && pct.type == AML_PKG && pct.b->len >= 1)
            parse_reg(&pct.b->el[0], &P.space, &P.addr, &P.width);
        aml_val_free(&pct);
        u64 ppc = 0;
        if (aml_find(P.cpu, "_PPC") && aml_eval_int(P.cpu, "_PPC", &ppc) == AML_OK) P.ppc = (int)ppc;
    }
    if (!P.n && !from_msrs()) { kprintf("cpufreq: sem P-states\n"); return; }
    if (P.space == 0x7F && !g_zen) { kprintf("cpufreq: _PCT FFixedHW fora de um Zen: não mexo\n"); return; }
    if (P.space != 0x7F && P.space != 1) { kprintf("cpufreq: _PCT no espaço %u não tratado\n", P.space); return; }
    P.ok = true;
    aml_node_t *tz = NULL;
    if (acpi_ready()) aml_walk(NULL, find_tz, &tz);
    P.tz = tz;
    if (tz) {
        u64 v = 0;
        P.tc1 = aml_eval_int(tz, "_TC1", &v) == AML_OK ? (int)v : 1;
        P.tc2 = aml_eval_int(tz, "_TC2", &v) == AML_OK ? (int)v : 2;
        P.tsp = aml_eval_int(tz, "_TSP", &v) == AML_OK && v ? (int)v * 100 : 2000;   /* _TSP em décimos de s */
    }
    for (int i = 0; i < MAX_CPUS; i++) g_applied[i] = -1;
    g_target = 0;
    apply_here(0); g_applied[0] = 0;
    char b[96]; int o = 0;
    for (int i = 0; i < P.n && o < 80; i++) o += snprintf(b + o, sizeof b - o, "%s%u", i ? "/" : "", P.mhz[i]);
    kprintf("cpufreq: %d P-states (%s MHz) por %s 0x%llx, _PPC %d, passivo %s, MPERF %s\n", P.n, b,
            P.space == 0x7F ? "MSR" : "porta", P.addr, P.ppc, tz ? "pela zona térmica" : "não", g_aperf ? "sim" : "não");
}

void cpufreq_poll(void) {
    if (!P.ok) return;
    static u64 last, last_t, last_ppc;
    u64 now = timer_ticks();
    if (now - last >= 50) {
        last = now;
        measure(0);
        u32 mx = 0;
        int n = smp_cpu_count();
        for (int i = 0; i < n && i < MAX_CPUS; i++) if (g_load[i] > mx) mx = g_load[i];
        if (!g_aperf) mx = 1000;                       /* sem medida: desempenho máximo */
        if (g_forced) mx = 0;
        if (g_forced) set_target(P.n - 1);
        else if (mx >= 700) set_target(0);
        else if (mx <= 300) set_target(g_target + 1);
        if (g_applied[0] != g_target) { apply_here(g_target); g_applied[0] = g_target; }
    }
    if (P.cpu && now - last_ppc >= 1000) {             /* _PPC pode mudar (Notify 0x80 no processador) */
        last_ppc = now;
        u64 ppc;
        if (aml_find(P.cpu, "_PPC") && aml_eval_int(P.cpu, "_PPC", &ppc) == AML_OK) P.ppc = (int)ppc;
    }
    if (P.tz && now - last_t >= (u64)P.tsp) {
        last_t = now;
        u64 t = 0, psv = 0;
        if (aml_eval_int(P.tz, "_TMP", &t) == AML_OK && aml_eval_int(P.tz, "_PSV", &psv) == AML_OK) {
            int tn = (int)t, dp = P.tc1 * (tn - (P.last_tz ? P.last_tz : tn)) + P.tc2 * (tn - (int)psv);
            P.last_tz = tn;
            if (tn >= (int)psv && dp > 0 && P.therm < P.n - 1) P.therm++;
            else if ((tn < (int)psv || dp < 0) && P.therm > 0) P.therm--;
            set_target(g_target);
        }
    }
}

void cpufreq_force_lowest(bool on) {
    if (!P.ok) return;
    g_forced = on;
    int t = on ? P.n - 1 : lim();
    g_target = t;
    apply_here(t); g_applied[0] = t;
}

int cpufreq_current_mhz(void) { return P.ok ? (int)P.mhz[g_target] : 0; }
int cpufreq_states(u32 *mhz, int max) { int n = P.n < max ? P.n : max; for (int i = 0; i < n; i++) mhz[i] = P.mhz[i]; return n; }
u32 cpufreq_switches(void) { return g_switches; }
