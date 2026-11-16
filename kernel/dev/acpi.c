/* mOS — ACPI da plataforma: tabelas, FADT, interpretador AML (dev/aml.c), SCI, GPE, EC, botão de energia,
 * desligar (S5) e reiniciar pelo firmware, temperatura.
 *
 * Ordem no boot (acpi_init, antes do pci_init): acha RSDP/XSDT, lê a FADT inteira (blocos PM1/GPE em GAS,
 * registro de reset, FACS, século do RTC), liga o modo ACPI (SMI_CMD ← ACPI_ENABLE) se o SCI_EN estiver
 * desligado, carrega a DSDT e todas as SSDT no namespace, chama \_PIC(0) (o mOS usa o 8259 para IRQs
 * legadas), roda _REG das regiões (EC só depois de achar o PNP0C09) e _STA/_INI de cada dispositivo.
 * Depois instala o tratador do SCI, liga os GPEs de tempo de execução (os que têm _Lxx/_Exx e não são só
 * de despertar via _PRW) e o botão de energia de hardware fixo.
 *
 * O tratador do SCI roda na IRQ: só lê e limpa o PM1, mascara os GPEs que dispararam e marca pendência.
 * Os métodos AML (_Lxx, _Exx, _Qxx do EC) rodam em acpi_poll(), no laço principal, com a trava grande.
 */
#include "kernel.h"
#include "aml.h"
#include "ui.h"

typedef struct PACKED { char sig[8]; u8 csum; char oem[6]; u8 rev; u32 rsdt; u32 len; u64 xsdt; u8 xcsum, r[3]; } rsdp_t;
typedef struct PACKED { char sig[4]; u32 len; u8 rev, csum; char oem[6], oemtab[8]; u32 oemrev, creator, crev; } sdt_t;
typedef struct { u8 space, width, offset, access; u64 addr; } gas_t;

#define PM1_TMR    (1u << 0)
#define PM1_GBL    (1u << 5)
#define PM1_PWRBTN (1u << 8)
#define PM1_SLPBTN (1u << 9)
#define PM1_RTC    (1u << 10)
#define PM1_WAK    (1u << 15)
#define CNT_SCI_EN (1u << 0)
#define CNT_SLP_EN (1u << 13)

static struct {
    bool ok, hw_reduced, reset_ok, cm_pwrbtn;
    u16 sci;
    u32 smi_cmd; u8 acpi_enable;
    gas_t pm1a_evt, pm1b_evt, pm1a_cnt, pm1b_cnt, pm_tmr, gpe0, gpe1, reset;
    u8 pm1_evt_len, gpe0_len, gpe1_len, gpe1_base, reset_val, century;
    bool tmr_32;
    u32 *facs;
    u64 dsdt;
    u8 slp_a[6], slp_b[6]; bool slp_ok[6];
    int ntables;
} F;

static volatile u32 g_pending_pm1;           /* bits de PM1 vistos no SCI */
static volatile u8 g_pending_gpe[32];        /* bytes de status dos GPEs mascarados no SCI */
static u8 g_gpe_enabled[32];                 /* máscara de GPEs de tempo de execução */
static u8 g_gpe_wake[32];                    /* GPEs que só servem para despertar (_PRW) */
static u8 g_gpe_edge[32];                    /* _Exx (borda) vs _Lxx (nível) */
static u32 g_sci_count, g_gpe_count;
static int g_ec_gpe = -1;
static u16 g_ec_data = 0x62, g_ec_cmd = 0x66;
static bool g_ec_ok;
static aml_node_t *g_ec_node;
static aml_node_t *g_pwrb, *g_tz[8];
static int g_ntz;
static bool g_aml_ok;
static u64 g_btn_last;

/* ------------------------------------------------------------------ tabelas */
static rsdp_t *rsdp(void) { return g_boot ? (rsdp_t *)g_boot->rsdp : NULL; }

static const sdt_t *table_at(int i) {
    rsdp_t *r = rsdp();
    if (!r) return NULL;
    if (r->rev >= 2 && r->xsdt) {
        const sdt_t *x = (const sdt_t *)r->xsdt;
        u32 n = (x->len - sizeof(sdt_t)) / 8;
        if (i < 0 || (u32)i >= n) return NULL;
        u64 a; memcpy(&a, (const u8 *)(x + 1) + 8 * i, 8);
        return (const sdt_t *)a;
    }
    const sdt_t *x = (const sdt_t *)(u64)r->rsdt;
    u32 n = (x->len - sizeof(sdt_t)) / 4;
    if (i < 0 || (u32)i >= n) return NULL;
    u32 a; memcpy(&a, (const u8 *)(x + 1) + 4 * i, 4);
    return (const sdt_t *)(u64)a;
}

static bool sum_ok(const void *t, u32 len) { u8 s = 0; for (u32 i = 0; i < len; i++) s += ((const u8 *)t)[i]; return s == 0; }

const void *acpi_find_table(const char *sig, int nth) {
    if (!memcmp(sig, "DSDT", 4)) return F.dsdt ? (const void *)F.dsdt : NULL;
    for (int i = 0;; i++) {
        const sdt_t *t = table_at(i);
        if (!t) return NULL;
        if (!memcmp(t->sig, sig, 4) && nth-- == 0) return t;
    }
}

/* ------------------------------------------------------------------ GAS e PCI cru */
static u32 pci_raw(u8 bus, u8 dev, u8 fn, u16 off) {
    outl(0xCF8, 0x80000000u | (u32)bus << 16 | (u32)dev << 11 | (u32)fn << 8 | (off & 0xFC));
    return inl(0xCFC);
}
static void pci_raw_w(u8 bus, u8 dev, u8 fn, u16 off, u32 v) {
    outl(0xCF8, 0x80000000u | (u32)bus << 16 | (u32)dev << 11 | (u32)fn << 8 | (off & 0xFC));
    outl(0xCFC, v);
}

static void map_phys(u64 a, u32 len) {
    static u64 done[16]; static int nd;
    if (a + len <= (4ull << 30)) return;            /* abaixo de 4 GiB o mapa de identidade já cobre (UC fora da RAM) */
    u64 c = ALIGN_DOWN(a, 0x200000);
    for (int i = 0; i < nd; i++) if (done[i] == c) return;
    vmm_map_mmio(a, len);
    if (nd < 16) done[nd++] = c;
}

static bool mem_rw(u64 a, u32 bits, u64 *v, bool w) {
    map_phys(a, bits / 8);
    switch (bits) {
    case 8:  if (w) *(volatile u8 *)a = (u8)*v; else *v = *(volatile u8 *)a; return true;
    case 16: if (w) *(volatile u16 *)a = (u16)*v; else *v = *(volatile u16 *)a; return true;
    case 32: if (w) *(volatile u32 *)a = (u32)*v; else *v = *(volatile u32 *)a; return true;
    case 64: if (w) *(volatile u64 *)a = *v; else *v = *(volatile u64 *)a; return true;
    }
    return false;
}
static bool io_rw(u16 p, u32 bits, u64 *v, bool w) {
    switch (bits) {
    case 8:  if (w) outb(p, (u8)*v); else *v = inb(p); return true;
    case 16: if (w) outw(p, (u16)*v); else *v = inw(p); return true;
    case 32: if (w) outl(p, (u32)*v); else *v = inl(p); return true;
    case 64: if (w) { outl(p, (u32)*v); outl((u16)(p + 4), (u32)(*v >> 32)); } else *v = inl(p) | (u64)inl((u16)(p + 4)) << 32; return true;
    }
    return false;
}
static bool pci_rw(u8 bus, u8 dev, u8 fn, u16 off, u32 bits, u64 *v, bool w) {
    if (dev >= 32 || fn >= 8 || off >= 256) return false;
    if (bits == 64) {
        u64 lo = (u32)*v, hi = *v >> 32;
        if (!pci_rw(bus, dev, fn, off, 32, &lo, w) || !pci_rw(bus, dev, fn, (u16)(off + 4), 32, &hi, w)) return false;
        if (!w) *v = lo | hi << 32;
        return true;
    }
    u32 sh = (off & 3) * 8, mask = bits == 32 ? 0xFFFFFFFFu : (1u << bits) - 1;
    u32 d = pci_raw(bus, dev, fn, off);
    if (!w) { *v = (d >> sh) & mask; return true; }
    d = (d & ~(mask << sh)) | (((u32)*v & mask) << sh);
    pci_raw_w(bus, dev, fn, off, d);
    return true;
}

static void gas_from(gas_t *g, const u8 *p) { g->space = p[0]; g->width = p[1]; g->offset = p[2]; g->access = p[3]; memcpy(&g->addr, p + 4, 8); }
static void gas_io(gas_t *g, u32 port, u8 len) { memset(g, 0, sizeof *g); if (port) { g->space = 1; g->addr = port; g->width = (u8)(len * 8); } }

static bool gas_rw(const gas_t *g, u32 off, u32 bits, u64 *v, bool w) {
    if (!g->addr) return false;
    if (!w) *v = 0;
    switch (g->space) {
    case 0: return mem_rw(g->addr + off, bits, v, w);
    case 1: return io_rw((u16)(g->addr + off), bits, v, w);
    case 2: return pci_rw(0, (u8)(g->addr >> 32), (u8)(g->addr >> 16), (u16)((g->addr & 0xFFFF) + off), bits, v, w);
    }
    return false;
}
static u32 gas_read(const gas_t *g, u32 off, u32 bits) { u64 v = 0; gas_rw(g, off, bits, &v, false); return (u32)v; }
static void gas_write(const gas_t *g, u32 off, u32 bits, u32 val) { u64 v = val; gas_rw(g, off, bits, &v, true); }

/* PM1 é a junção de A e B (OR na leitura, mesma escrita nos dois) */
static u16 pm1_sts(void) { u16 a = (u16)gas_read(&F.pm1a_evt, 0, 16); if (F.pm1b_evt.addr) a |= (u16)gas_read(&F.pm1b_evt, 0, 16); return a; }
static void pm1_sts_clear(u16 bits) { gas_write(&F.pm1a_evt, 0, 16, bits); if (F.pm1b_evt.addr) gas_write(&F.pm1b_evt, 0, 16, bits); }
static u16 pm1_en(void) { return (u16)gas_read(&F.pm1a_evt, F.pm1_evt_len / 2, 16); }
static void pm1_en_set(u16 v) { gas_write(&F.pm1a_evt, F.pm1_evt_len / 2, 16, v); if (F.pm1b_evt.addr) gas_write(&F.pm1b_evt, F.pm1_evt_len / 2, 16, v); }
static u16 pm1_cnt(void) { return (u16)gas_read(&F.pm1a_cnt, 0, 16); }

/* GPE: bloco 0 cobre os bytes 0..len/2-1, bloco 1 começa em GPE1_BASE */
static int gpe_bytes(void) { return F.gpe0_len / 2 + F.gpe1_len / 2; }
static bool gpe_reg(int byte, const gas_t **g, u32 *sts_off, u32 *en_off) {
    if (byte < F.gpe0_len / 2) { *g = &F.gpe0; *sts_off = (u32)byte; *en_off = (u32)(F.gpe0_len / 2 + byte); return true; }
    byte -= F.gpe0_len / 2;
    if (byte < F.gpe1_len / 2) { *g = &F.gpe1; *sts_off = (u32)byte; *en_off = (u32)(F.gpe1_len / 2 + byte); return true; }
    return false;
}
static int gpe_num_to_byte(int n, int *bit) {
    if (n < F.gpe0_len * 4) { *bit = n & 7; return n / 8; }
    if (F.gpe1_len && n >= F.gpe1_base && n < F.gpe1_base + F.gpe1_len * 4) { *bit = (n - F.gpe1_base) & 7; return F.gpe0_len / 2 + (n - F.gpe1_base) / 8; }
    return -1;
}
static int gpe_byte_to_num(int byte, int bit) {
    if (byte < F.gpe0_len / 2) return byte * 8 + bit;
    return F.gpe1_base + (byte - F.gpe0_len / 2) * 8 + bit;
}
static u8 gpe_rd(int byte, bool en) { const gas_t *g; u32 s, e; if (!gpe_reg(byte, &g, &s, &e)) return 0; return (u8)gas_read(g, en ? e : s, 8); }
static void gpe_wr(int byte, bool en, u8 v) { const gas_t *g; u32 s, e; if (gpe_reg(byte, &g, &s, &e)) gas_write(g, en ? e : s, 8, v); }

/* ------------------------------------------------------------------ controlador embarcado (ACPI 12) */
#define EC_OBF 0x01
#define EC_IBF 0x02
#define EC_SCI 0x20
static bool ec_wait(u8 mask, u8 want) {
    for (int i = 0; i < 20000; i++) {
        if ((inb(g_ec_cmd) & mask) == want) return true;
        for (int k = 0; k < 50; k++) __asm__ volatile("pause");
    }
    return false;
}
static bool ec_cmd(u8 c) { if (!ec_wait(EC_IBF, 0)) return false; outb(g_ec_cmd, c); return true; }
static bool ec_out(u8 d) { if (!ec_wait(EC_IBF, 0)) return false; outb(g_ec_data, d); return true; }
static bool ec_in(u8 *d) { if (!ec_wait(EC_OBF, EC_OBF)) return false; *d = inb(g_ec_data); return true; }
static bool ec_read(u8 a, u8 *v) { return ec_cmd(0x80) && ec_out(a) && ec_in(v); }
static bool ec_write(u8 a, u8 v) { return ec_cmd(0x81) && ec_out(a) && ec_out(v); }

/* ------------------------------------------------------------------ serviços para o aml.c */
void *aml_os_alloc(u32 n) { void *p = kmalloc(n ? n : 1); if (p) memset(p, 0, n ? n : 1); return p; }
void aml_os_free(void *p) { kfree(p); }
void aml_os_log(const char *fmt, ...) {
    char b[256]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    kprintf("%s", b);
}
bool aml_os_mem(u64 a, u32 bits, u64 *v, bool w) { return mem_rw(a, bits, v, w); }
bool aml_os_io(u16 p, u32 bits, u64 *v, bool w) { return io_rw(p, bits, v, w); }
bool aml_os_pci(u16 seg, u8 bus, u8 dev, u8 fn, u16 off, u32 bits, u64 *v, bool w) {
    if (seg) return false;
    return pci_rw(bus, dev, fn, off, bits, v, w);
}
bool aml_os_ec(u8 a, u8 *v, bool w) {
    if (!g_ec_ok) { if (!w) *v = 0; return true; }
    return w ? ec_write(a, *v) : ec_read(a, v);
}
bool aml_os_cmos(u8 a, u8 *v, bool w) {
    u16 base = a < 0x80 ? 0x70 : 0x72;
    outb(base, a & 0x7F);
    if (w) outb((u16)(base + 1), *v); else *v = inb((u16)(base + 1));
    return true;
}
bool aml_os_region_other(u8 sp, u64 a, u32 bits, u64 *v, bool w) {
    (void)a; (void)bits;
    static u32 warned;
    if (!(warned & (1u << (sp & 31)))) { warned |= 1u << (sp & 31); kprintf("ACPI: região do espaço %u sem tratador (lida como 0)\n", sp); }
    if (!w) *v = 0;
    return true;
}
void aml_os_sleep_ms(u64 ms) { if (ms) sleep_ms(ms > 2000 ? 2000 : ms); }
void aml_os_stall_us(u64 us) {
    u64 t0 = hpet_ns();
    if (t0) { while (hpet_ns() - t0 < us * 1000) __asm__ volatile("pause"); return; }
    for (u64 i = 0; i < us; i++) io_wait();   /* ~1 µs por escrita na porta 0x80 */
}
u64 aml_os_timer_100ns(void) { u64 ns = hpet_ns(); return ns ? ns / 100 : timer_ticks() * 10000; }

void aml_os_notify(aml_node_t *n, u64 v) {
    char b[64]; aml_path(n, b, sizeof b);
    if (n == g_pwrb && v == 0x80) { g_pending_pm1 |= PM1_PWRBTN; return; }
    for (int i = 0; i < g_ntz; i++) if (n == g_tz[i]) return;   /* mudança de temperatura: o acpi_poll relê */
    if (v != 0x02) kprintf("ACPI: Notify(%s, 0x%llx)\n", b, v);
}

/* trava global da FACS (ACPI 5.2.10.1): bit 0 pendente, bit 1 dona */
bool aml_os_global_lock(bool acq) {
    if (!F.facs) return true;
    volatile u32 *gl = &F.facs[4];
    if (acq) {
        for (int tries = 0; tries < 1000000; tries++) {
            u32 o = *gl, n = (o & ~3u) | ((o >> 1) & 1) | 2;
            if (__atomic_compare_exchange_n(gl, &o, n, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED) && !(n & 1)) return true;
            __asm__ volatile("pause");
        }
        kprintf("ACPI: trava global não veio\n");
        return false;
    }
    u32 o, n;
    do { o = *gl; n = o & ~3u; } while (!__atomic_compare_exchange_n(gl, &o, n, false, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
    if (o & 1) { u16 c = pm1_cnt(); gas_write(&F.pm1a_cnt, 0, 16, c | (1u << 2)); }   /* GBL_RLS */
    return true;
}

const void *aml_os_find_table(const char *sig, const char *oem, const char *oemt, int nth) {
    for (int i = 0;; i++) {
        const sdt_t *t = table_at(i);
        if (!t) return NULL;
        if (memcmp(t->sig, sig, 4)) continue;
        if (oem && *oem && strncmp(t->oem, oem, strlen(oem))) continue;
        if (oemt && *oemt && strncmp(t->oemtab, oemt, strlen(oemt))) continue;
        if (nth-- == 0) return t;
    }
}

/* ------------------------------------------------------------------ FADT */
static void parse_fadt(const u8 *f, u32 len) {
    u32 v32;
#define U32(o) (memcpy(&v32, f + (o), 4), v32)
    F.sci = (u16)(f[46] | f[47] << 8);
    F.smi_cmd = U32(48); F.acpi_enable = f[52];
    gas_io(&F.pm1a_evt, U32(56), f[88]); gas_io(&F.pm1b_evt, U32(60), f[88]);
    gas_io(&F.pm1a_cnt, U32(64), f[89]); gas_io(&F.pm1b_cnt, U32(68), f[89]);
    gas_io(&F.pm_tmr, U32(76), f[91]);
    gas_io(&F.gpe0, U32(80), f[92]); gas_io(&F.gpe1, U32(84), f[93]);
    F.pm1_evt_len = f[88]; F.gpe0_len = f[92]; F.gpe1_len = f[93]; F.gpe1_base = f[94];
    F.century = len > 108 ? f[108] : 0;
    u32 flags = len >= 116 ? U32(112) : 0;
    F.tmr_32 = (flags >> 8) & 1;
    F.cm_pwrbtn = (flags >> 4) & 1;
    F.hw_reduced = (flags >> 20) & 1;
    F.reset_ok = (flags >> 10) & 1;
    F.facs = (u32 *)(u64)U32(36);
    F.dsdt = U32(40);
    if (len >= 129) { gas_from(&F.reset, f + 116); F.reset_val = f[128]; }
    if (len >= 148) {
        u64 x; memcpy(&x, f + 132, 8); if (x) F.facs = (u32 *)x;
        memcpy(&x, f + 140, 8); if (x) F.dsdt = x;
    }
    /* campos X_ (GAS de 64 bits) têm prioridade quando preenchidos */
    static const struct { u32 off; gas_t *g; } xs[] = {
        {148, &F.pm1a_evt}, {160, &F.pm1b_evt}, {172, &F.pm1a_cnt}, {184, &F.pm1b_cnt},
        {208, &F.pm_tmr}, {220, &F.gpe0}, {232, &F.gpe1},
    };
    for (u32 i = 0; i < ARRAY_LEN(xs); i++) {
        if (len < xs[i].off + 12) break;
        gas_t g; gas_from(&g, f + xs[i].off);
        if (g.addr) { u8 w = xs[i].g->width; *xs[i].g = g; if (!g.width) xs[i].g->width = w; }
    }
    if (F.facs && memcmp(F.facs, "FACS", 4)) F.facs = NULL;
#undef U32
}

static void acpi_enable_mode(void) {
    if (F.hw_reduced || !F.pm1a_cnt.addr) return;
    if (pm1_cnt() & CNT_SCI_EN) return;
    if (!F.smi_cmd || !F.acpi_enable) { kprintf("ACPI: SCI_EN desligado e sem SMI_CMD\n"); return; }
    outb((u16)F.smi_cmd, F.acpi_enable);
    u64 t0 = timer_ticks();
    while (!(pm1_cnt() & CNT_SCI_EN) && timer_ticks() - t0 < 3000) sleep_ms(1);
    kprintf("ACPI: modo ACPI %s\n", (pm1_cnt() & CNT_SCI_EN) ? "ligado" : "NÃO ligou");
}

/* ------------------------------------------------------------------ estados de sono */
static void read_sleep_types(void) {
    static const char *const names[6] = { "\\_S0", "\\_S1", "\\_S2", "\\_S3", "\\_S4", "\\_S5" };
    for (int s = 0; s < 6; s++) {
        aml_val_t v;
        if (aml_eval_path(NULL, names[s], NULL, 0, &v) != AML_OK) continue;
        if (v.type == AML_PKG && v.b->len >= 1) {
            u64 a = 0, b = 0;
            aml_val_to_int(&v.b->el[0], &a);
            if (v.b->len >= 2) aml_val_to_int(&v.b->el[1], &b);
            F.slp_a[s] = (u8)(a & 7); F.slp_b[s] = (u8)(b & 7); F.slp_ok[s] = true;
        }
        aml_val_free(&v);
    }
}
/* AML ausente (DSDT quebrada): procura o _S5_ cru, como fazia o driver antigo */
static void scan_s5_raw(void) {
    const sdt_t *d = (const sdt_t *)F.dsdt;
    if (!d || memcmp(d->sig, "DSDT", 4)) return;
    const u8 *p = (const u8 *)(d + 1), *end = (const u8 *)d + d->len;
    for (; p + 8 < end; p++) {
        if (memcmp(p, "_S5_", 4) || !(p[-1] == 0x08 || (p[-2] == 0x08 && p[-1] == '\\'))) continue;
        p += 4;
        if (*p++ != 0x12) continue;
        p += ((*p & 0xC0) >> 6) + 2;
        if (*p == 0x0A) p++;
        F.slp_a[5] = *p++ & 7;
        if (*p == 0x0A) p++;
        F.slp_b[5] = *p & 7;
        F.slp_ok[5] = true;
        return;
    }
}

bool acpi_sleep_type(int state, u8 *a, u8 *b) {
    if (state < 0 || state > 5 || !F.slp_ok[state]) return false;
    *a = F.slp_a[state]; *b = F.slp_b[state];
    return true;
}

static void eval_arg(const char *path, u64 arg) {
    if (!g_aml_ok || !aml_find(NULL, path)) return;
    u64 r; int e = aml_call_int1(NULL, path, arg, &r);
    if (e) kprintf("ACPI: %s(%llu): %s\n", path, arg, aml_strerror(e));
}

/* escreve SLP_TYP|SLP_EN; volta só se o hardware não desligar */
void acpi_enter_sleep_regs(int state) {
    u16 a = (u16)(F.pm1a_cnt.addr ? pm1_cnt() : 0) & ~(7u << 10 | CNT_SLP_EN);
    u16 b = (u16)(F.pm1b_cnt.addr ? gas_read(&F.pm1b_cnt, 0, 16) : 0) & ~(7u << 10 | CNT_SLP_EN);
    pm1_sts_clear(PM1_WAK);
    a |= (u16)(F.slp_a[state] << 10); b |= (u16)(F.slp_b[state] << 10);
    gas_write(&F.pm1a_cnt, 0, 16, a);
    if (F.pm1b_cnt.addr) gas_write(&F.pm1b_cnt, 0, 16, b);
    __asm__ volatile("wbinvd" ::: "memory");
    gas_write(&F.pm1a_cnt, 0, 16, a | CNT_SLP_EN);
    if (F.pm1b_cnt.addr) gas_write(&F.pm1b_cnt, 0, 16, b | CNT_SLP_EN);
}

void acpi_poweroff(void) {
    kprintf("ACPI: desligando (S5)\n");
    if (F.ok && F.slp_ok[5]) {
        eval_arg("\\_TTS", 5);
        eval_arg("\\_PTS", 5);
        eval_arg("\\_SI._SST", 0);
        cli();
        pm1_en_set(0);
        for (int i = 0; i < gpe_bytes(); i++) gpe_wr(i, true, 0);
        acpi_enter_sleep_regs(5);
        for (int i = 0; i < 2000000; i++) __asm__ volatile("pause");
    }
    cli();
    outw(0x604, 0x2000);  /* QEMU */
    outw(0xB004, 0x2000); /* Bochs/QEMU antigo */
    outw(0x4004, 0x3400); /* VirtualBox */
    for (;;) hlt_raw();
}

void acpi_reboot(void) {
    kprintf("ACPI: reiniciando\n");
    eval_arg("\\_TTS", 5);
    cli();
    if (F.reset_ok && F.reset.addr && (F.reset.space <= 2)) {
        gas_write(&F.reset, 0, 8, F.reset_val);
        for (int i = 0; i < 1000000; i++) __asm__ volatile("pause");
    }
    outb(0xCF9, 0x02); outb(0xCF9, 0x06);
    for (int i = 0; i < 1000000; i++) __asm__ volatile("pause");
    for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) {}
    outb(0x64, 0xFE);
    static struct PACKED { u16 l; u64 b; } null_idt = {0, 0};
    __asm__ volatile("lidt %0; int3" :: "m"(null_idt));
    for (;;) hlt_raw();
}

/* ------------------------------------------------------------------ SCI */
static void sci_irq(int_frame_t *f) {
    (void)f;
    g_sci_count++;
    u16 st = pm1_sts(), en = pm1_en();
    u16 hit = st & en & (PM1_PWRBTN | PM1_SLPBTN | PM1_RTC | PM1_GBL | PM1_TMR);
    if (hit) { pm1_sts_clear(hit); g_pending_pm1 |= hit; }
    for (int i = 0; i < gpe_bytes(); i++) {
        u8 s = gpe_rd(i, false) & g_gpe_enabled[i];
        if (!s) continue;
        gpe_wr(i, true, g_gpe_enabled[i] & ~s & ~g_pending_gpe[i]);   /* mascara até o acpi_poll tratar */
        g_pending_gpe[i] |= s;
    }
}

static void ec_query(void) {
    for (int n = 0; n < 8; n++) {
        if (!(inb(g_ec_cmd) & EC_SCI)) return;
        u8 q = 0;
        if (!ec_cmd(0x84) || !ec_in(&q) || !q) return;
        char m[5] = { '_', 'Q', "0123456789ABCDEF"[q >> 4], "0123456789ABCDEF"[q & 15], 0 };
        if (g_ec_node && aml_find(g_ec_node, m)) { aml_val_t r; aml_eval_path(g_ec_node, m, NULL, 0, &r); aml_val_free(&r); }
    }
}

static void run_gpe(int num, bool edge) {
    char m[5] = { '_', edge ? 'E' : 'L', "0123456789ABCDEF"[(num >> 4) & 15], "0123456789ABCDEF"[num & 15], 0 };
    aml_node_t *g = aml_find(NULL, "\\_GPE");
    if (g && aml_find(g, m)) { aml_val_t r; int e = aml_eval_path(g, m, NULL, 0, &r); aml_val_free(&r); if (e) kprintf("ACPI: %s: %s\n", m, aml_strerror(e)); }
}

static int cpu_temp_dC(void);

void acpi_poll(void) {
    input_media_poll();
    if (!F.ok) return;
    u32 pm = __atomic_exchange_n(&g_pending_pm1, 0, __ATOMIC_ACQ_REL);
    if (pm & (PM1_PWRBTN)) {
        u64 now = timer_ticks();
        if (now - g_btn_last > 800) { kprintf("ACPI: botão de energia\n"); desktop_shutdown_dialog(0); }
        g_btn_last = now;
    }
    for (int i = 0; i < gpe_bytes(); i++) {
        u64 fl = irq_save();
        u8 s = g_pending_gpe[i]; g_pending_gpe[i] = 0;
        irq_restore(fl);
        if (!s) continue;
        for (int b = 0; b < 8; b++) {
            if (!(s & (1u << b))) continue;
            int num = gpe_byte_to_num(i, b);
            bool edge = (g_gpe_edge[i] >> b) & 1;
            g_gpe_count++;
            if (edge) gpe_wr(i, false, (u8)(1u << b));
            if (num == g_ec_gpe) ec_query(); else run_gpe(num, edge);
            if (!edge) gpe_wr(i, false, (u8)(1u << b));
        }
        u64 f2 = irq_save();
        gpe_wr(i, true, g_gpe_enabled[i] & ~g_pending_gpe[i]);
        irq_restore(f2);
    }
    cpufreq_poll();
    /* térmica a cada 5 s (ACPI 11.1.4): zona em _CRT ou acima, ou o Tctl do Zen além de 105 °C, desliga já */
    static u64 last_t;
    u64 now = timer_ticks();
    if (now - last_t >= 5000) {
        last_t = now;
        const char *why = NULL;
        int t = 0, crit = 0;
        for (int i = 0; i < g_ntz && !why; i++) {
            u64 k = 0, c = 0;
            if (aml_eval_int(g_tz[i], "_TMP", &k) == AML_OK && aml_eval_int(g_tz[i], "_CRT", &c) == AML_OK && c > 2732 && k >= c) {
                t = (int)k - 2732; crit = (int)c - 2732; why = "zona térmica";
            }
        }
        int tc = cpu_temp_dC();
        if (!why && tc > 1050) { t = tc; crit = 1050; why = "Tctl da CPU"; }
        if (why) {
            kprintf("ACPI: temperatura crítica (%s) %d.%d °C, limite %d.%d: desligando\n", why, t / 10, t % 10, crit / 10, crit % 10);
            extern void persist_sync(void);
            persist_sync();
            acpi_poweroff();
        }
    }
}

/* ------------------------------------------------------------------ temperatura */
/* Zen: SMN 0x00059800 (THM_TCON_CUR_TMP) pela janela 0x60/0x64 do 00:00.0 — mesmo dado do k10temp */
static u32 smn_read(u32 a) { pci_raw_w(0, 0, 0, 0x60, a); return pci_raw(0, 0, 0, 0x64); }
static bool is_amd_zen(void) {
    u32 a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
    if (b != 0x68747541) return false;   /* "Auth"enticAMD */
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
    u32 fam = ((a >> 8) & 0xF) + ((a >> 20) & 0xFF);
    return fam >= 0x17;
}
static int cpu_temp_dC(void) {
    if (is_amd_zen()) {
        u32 r = smn_read(0x00059800);
        int t = (int)((r >> 21) * 10 / 8);         /* 0,125 °C por unidade */
        if (r & (1u << 19)) t -= 490;               /* faixa estendida (−49 °C) */
        if (r && r != 0xFFFFFFFF) return t;
    }
    for (int i = 0; i < g_ntz; i++) {
        u64 k = 0;
        if (aml_eval_int(g_tz[i], "_TMP", &k) == AML_OK && k > 2000) return (int)k - 2732;
    }
    return -1;
}
int acpi_cpu_temp_dC(void) { return cpu_temp_dC(); }

int acpi_thermal_zones(int *temp_dC, int *crit_dC, int max) {
    int n = 0;
    for (int i = 0; i < g_ntz && n < max; i++) {
        u64 t = 0, c = 0;
        if (aml_eval_int(g_tz[i], "_TMP", &t) != AML_OK) continue;
        aml_eval_int(g_tz[i], "_CRT", &c);
        temp_dC[n] = (int)t - 2732; crit_dC[n] = c ? (int)c - 2732 : 0;
        n++;
    }
    return n;
}

/* ------------------------------------------------------------------ descoberta no namespace */
typedef struct { aml_node_t *ec; } scan_t;
static int scan_cb(aml_node_t *n, int depth, void *c) {
    (void)depth;
    scan_t *s = c;
    if (n->type == AML_THERMAL && g_ntz < 8) g_tz[g_ntz++] = n;
    if (n->type != AML_DEVICE) return 0;
    if (!s->ec && aml_device_match(n, "PNP0C09")) s->ec = n;
    if (!g_pwrb && aml_device_match(n, "PNP0C0C")) g_pwrb = n;
    /* _PRW: o GPE (Package[0] inteiro, ou Package(dispositivo, índice)) é de despertar */
    aml_val_t v;
    if (aml_find(n, "_PRW") && aml_eval_path(n, "_PRW", NULL, 0, &v) == AML_OK) {
        if (v.type == AML_PKG && v.b->len >= 1 && v.b->el[0].type == AML_INT) {
            int bit, by = gpe_num_to_byte((int)v.b->el[0].i, &bit);
            if (by >= 0 && by < 32) g_gpe_wake[by] |= (u8)(1u << bit);
        }
        aml_val_free(&v);
    }
    return 0;
}

static int ec_res_cb(const aml_res_t *r, void *c) {
    int *k = c;
    if (r->kind == AML_RES_IO) { if (*k == 0) g_ec_data = (u16)r->min; else if (*k == 1) g_ec_cmd = (u16)r->min; (*k)++; }
    return 0;
}

static void setup_ec(aml_node_t *ec) {
    /* ECDT tem prioridade: dá as portas antes do namespace estar pronto */
    const u8 *ecdt = acpi_find_table("ECDT", 0);
    if (ecdt) {
        gas_t c, d; gas_from(&c, ecdt + 36); gas_from(&d, ecdt + 48);
        if (c.space == 1 && d.space == 1) { g_ec_cmd = (u16)c.addr; g_ec_data = (u16)d.addr; }
    } else if (ec) {
        aml_val_t v;
        if (aml_eval_path(ec, "_CRS", NULL, 0, &v) == AML_OK && v.type == AML_BUF) { int k = 0; aml_res_walk(v.b->d, v.b->len, ec_res_cb, &k); }
        aml_val_free(&v);
    }
    if (!ec && !ecdt) return;
    u8 st = inb(g_ec_cmd);
    if (st == 0xFF) { kprintf("ACPI: EC sem resposta em 0x%x/0x%x\n", g_ec_data, g_ec_cmd); return; }
    g_ec_ok = true;
    g_ec_node = ec;
    u64 gpe = 0;
    if (ec && aml_eval_int(ec, "_GPE", &gpe) == AML_OK) g_ec_gpe = (int)gpe;
    else if (ecdt) g_ec_gpe = ecdt[64];
    kprintf("ACPI: EC em 0x%x/0x%x, GPE %d\n", g_ec_data, g_ec_cmd, g_ec_gpe);
}

static void enable_runtime_gpes(void) {
    aml_node_t *g = aml_find(NULL, "\\_GPE");
    int n = 0;
    for (aml_node_t *c = g ? g->child : NULL; c; c = c->next) {
        if (c->type != AML_METHOD || c->name[0] != '_' || (c->name[1] != 'L' && c->name[1] != 'E')) continue;
        int hi = c->name[2] >= 'A' ? c->name[2] - 'A' + 10 : c->name[2] - '0';
        int lo = c->name[3] >= 'A' ? c->name[3] - 'A' + 10 : c->name[3] - '0';
        if (hi < 0 || hi > 15 || lo < 0 || lo > 15) continue;
        int num = hi * 16 + lo, bit, by = gpe_num_to_byte(num, &bit);
        if (by < 0 || by >= 32) continue;
        if (c->name[1] == 'E') g_gpe_edge[by] |= (u8)(1u << bit);
        if (g_gpe_wake[by] & (1u << bit)) continue;   /* só de despertar: fica desligado em S0 */
        g_gpe_enabled[by] |= (u8)(1u << bit);
        n++;
    }
    if (g_ec_ok && g_ec_gpe >= 0) {
        int bit, by = gpe_num_to_byte(g_ec_gpe, &bit);
        if (by >= 0 && by < 32) { g_gpe_enabled[by] |= (u8)(1u << bit); g_gpe_edge[by] |= (u8)(1u << bit); n++; }
    }
    for (int i = 0; i < gpe_bytes(); i++) { gpe_wr(i, false, 0xFF); gpe_wr(i, true, g_gpe_enabled[i]); }
    kprintf("ACPI: %d GPEs de tempo de execução ligados (de %d bytes)\n", n, gpe_bytes());
}

/* ------------------------------------------------------------------ boot */
void acpi_init(void) {
    const u8 *fadt = acpi_find_table("FACP", 0);
    if (!fadt) { kprintf("ACPI: FADT não encontrada\n"); return; }
    u32 flen; memcpy(&flen, fadt + 4, 4);
    if (!sum_ok(fadt, flen)) kprintf("ACPI: soma da FADT não confere (seguindo assim mesmo)\n");
    parse_fadt(fadt, flen);
    F.ok = true;
    acpi_enable_mode();

    aml_init();
    const sdt_t *dsdt = (const sdt_t *)F.dsdt;
    if (dsdt && !memcmp(dsdt->sig, "DSDT", 4)) {
        int e = aml_load_table(dsdt);
        g_aml_ok = e == AML_OK;
        F.ntables++;
        if (e) kprintf("ACPI: DSDT: %s\n", aml_strerror(e));
    }
    for (int i = 0;; i++) {
        const sdt_t *t = acpi_find_table("SSDT", i);
        if (!t) break;
        int e = aml_load_table(t);
        F.ntables++;
        if (e) kprintf("ACPI: SSDT %d (%.8s): %s\n", i, t->oemtab, aml_strerror(e));
    }
    if (g_aml_ok) {
        eval_arg("\\_PIC", 0);                       /* modo 8259: o mOS usa o PIC para as IRQs legadas */
        aml_run_reg(AML_AS_MEM); aml_run_reg(AML_AS_IO); aml_run_reg(AML_AS_PCI); aml_run_reg(AML_AS_CMOS);
        scan_t s = { NULL };
        aml_walk(NULL, scan_cb, &s);
        setup_ec(s.ec);
        if (g_ec_ok) aml_run_reg(AML_AS_EC);
        aml_init_devices();
        read_sleep_types();
    }
    if (!F.slp_ok[5]) scan_s5_raw();

    if (!F.hw_reduced && F.sci && F.sci < 16) {
        pm1_sts_clear(0xFFFF & ~PM1_WAK);
        if (g_aml_ok) enable_runtime_gpes();
        u16 en = 0;
        if (!F.cm_pwrbtn) en |= PM1_PWRBTN;
        pm1_en_set(en);
        /* SCI é nível, ativo baixo e compartilhável: o 8259 precisa da IRQ em nível no ELCR (0x4D0/0x4D1) */
        if (F.sci >= 3 && F.sci != 8 && F.sci != 13) {
            u16 port = F.sci < 8 ? 0x4D0 : 0x4D1;
            outb(port, (u8)(inb(port) | (1u << (F.sci & 7))));
        }
        irq_install(F.sci, sci_irq);
    }
    cpufreq_init();
    kprintf("ACPI: %d tabelas AML, %u nós, %u erros; SCI %u, PM1a 0x%llx, GPE0 0x%llx/%u, S3 %s, S5 %s, reset %s 0x%llx, %d zonas térmicas, CPU %d °C\n",
            F.ntables, aml_node_count(), aml_errors(), F.sci, F.pm1a_cnt.addr, F.gpe0.addr, F.gpe0_len,
            F.slp_ok[3] ? "ok" : "não", F.slp_ok[5] ? "ok" : "não",
            F.reset_ok ? "ok" : "não", F.reset.addr, g_ntz, cpu_temp_dC() / 10);
}

/* para os outros módulos (RTC: século; suspensão: FACS e PM1) */
u8 acpi_rtc_century(void) { return F.century; }
u32 *acpi_facs(void) { return F.facs; }
u32 acpi_pm_timer(void) { return F.pm_tmr.addr ? gas_read(&F.pm_tmr, 0, 32) & (F.tmr_32 ? 0xFFFFFFFFu : 0xFFFFFFu) : 0; }
bool acpi_ready(void) { return F.ok && g_aml_ok; }
const char *acpi_status(void) {
    static char b[160];
    snprintf(b, sizeof b, "ACPI: %d tabelas, %u nós, SCI %u (%u), GPE %u, EC %s, CPU %d °C",
             F.ntables, aml_node_count(), F.sci, g_sci_count, g_gpe_count, g_ec_ok ? "sim" : "não", cpu_temp_dC() / 10);
    return b;
}
