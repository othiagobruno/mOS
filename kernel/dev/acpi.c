/* mOS — ACPI: localiza FADT/DSDT, desligamento (S5) e reinício */
#include "kernel.h"

typedef struct PACKED { char sig[8]; u8 csum; char oem[6]; u8 rev; u32 rsdt; u32 len; u64 xsdt; u8 xcsum, r[3]; } rsdp_t;
typedef struct PACKED { char sig[4]; u32 len; u8 rev, csum; char oem[6], oemtab[8]; u32 oemrev, creator, crev; } sdt_t;

static u16 pm1a_cnt, pm1b_cnt, slp_typa, slp_typb;
static bool s5_ok;
static u8 reset_space, reset_val;
static u64 reset_addr;

static sdt_t *find_table(const char *sig) {
    rsdp_t *r = g_boot->rsdp;
    if (!r) return NULL;
    if (r->rev >= 2 && r->xsdt) {
        sdt_t *x = (sdt_t *)r->xsdt;
        u32 n = (x->len - sizeof(sdt_t)) / 8;
        u64 *e = (u64 *)(x + 1);
        for (u32 i = 0; i < n; i++) {
            sdt_t *t = (sdt_t *)e[i];
            if (!memcmp(t->sig, sig, 4)) return t;
        }
    } else {
        sdt_t *x = (sdt_t *)(u64)r->rsdt;
        u32 n = (x->len - sizeof(sdt_t)) / 4;
        u32 *e = (u32 *)(x + 1);
        for (u32 i = 0; i < n; i++) {
            sdt_t *t = (sdt_t *)(u64)e[i];
            if (!memcmp(t->sig, sig, 4)) return t;
        }
    }
    return NULL;
}

void acpi_init(void) {
    sdt_t *fadt = find_table("FACP");
    if (!fadt) { kprintf("ACPI: FADT não encontrada\n"); return; }
    u8 *f = (u8 *)fadt;
    u32 dsdt32 = *(u32 *)(f + 40);
    u64 dsdt = dsdt32;
    if (fadt->len >= 148 && *(u64 *)(f + 140)) dsdt = *(u64 *)(f + 140);
    pm1a_cnt = (u16)*(u32 *)(f + 64);
    pm1b_cnt = (u16)*(u32 *)(f + 68);
    if (fadt->len >= 129) {
        reset_space = f[116];
        reset_addr = *(u64 *)(f + 120);
        reset_val = f[128];
    }
    /* procura o objeto \_S5_ no AML da DSDT: Name(_S5_, Package(){a, b, ...}) */
    sdt_t *d = (sdt_t *)dsdt;
    if (d && !memcmp(d->sig, "DSDT", 4)) {
        u8 *p = (u8 *)(d + 1), *end = (u8 *)d + d->len;
        for (; p < end - 8; p++) {
            if (memcmp(p, "_S5_", 4)) continue;
            if (!((p[-1] == 0x08) || (p[-2] == 0x08 && p[-1] == '\\'))) continue;
            p += 4;
            if (*p != 0x12) continue;
            p++;
            p += ((*p & 0xC0) >> 6) + 2; /* tamanho do pacote + nº de elementos */
            if (*p == 0x0A) p++;
            slp_typa = (u16)(*p++) << 10;
            if (*p == 0x0A) p++;
            slp_typb = (u16)(*p) << 10;
            s5_ok = true;
            break;
        }
    }
    kprintf("ACPI: PM1a=0x%x S5=%s reset=%s\n", pm1a_cnt, s5_ok ? "ok" : "não", reset_addr ? "ok" : "não");
}

void acpi_poweroff(void) {
    cli();
    if (s5_ok && pm1a_cnt) {
        outw(pm1a_cnt, slp_typa | (1 << 13));
        if (pm1b_cnt) outw(pm1b_cnt, slp_typb | (1 << 13));
    }
    outw(0x604, 0x2000);  /* QEMU */
    outw(0xB004, 0x2000); /* Bochs/QEMU antigo */
    outw(0x4004, 0x3400); /* VirtualBox */
    for (;;) hlt();
}

void acpi_reboot(void) {
    cli();
    if (reset_addr && reset_space == 1) outb((u16)reset_addr, reset_val);
    outb(0xCF9, 0x06);
    for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) {}
    outb(0x64, 0xFE);
    static struct PACKED { u16 l; u64 b; } null_idt = {0, 0};
    __asm__ volatile("lidt %0; int3" :: "m"(null_idt));
    for (;;) hlt();
}
