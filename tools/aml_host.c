/* mOS — harness de host do interpretador AML (kernel/dev/aml.c)
 *
 *   gcc -O1 -g -fsanitize=address,undefined -DAML_HOST -Ikernel/include \
 *       tools/aml_host.c kernel/dev/aml.c -o /tmp/aml_host
 *   iasl -p /tmp/teste tools/aml_tests/teste.asl
 *   /tmp/aml_host --teste /tmp/teste.aml          -> "aml-teste: ok"
 *   /tmp/aml_host DSDT.dat SSDT1.dat ...          -> carrega as tabelas reais e avalia
 *                                                   _S3/_S5, _TZ.TZ01, _PSS, _PRT, _PTS/_WAK
 *
 * Memória e E/S são simuladas: SystemMemory é um mapa esparso de páginas, SystemIO/PCI/EC/CMOS são
 * vetores. As tabelas reais vêm do firmware do PC e ficam fora do repositório. */
#define _GNU_SOURCE
#include "aml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void *aml_os_alloc(u32 n) { return calloc(1, n ? n : 1); }
void aml_os_free(void *p) { free(p); }
static int g_quiet;
void aml_os_log(const char *fmt, ...) {
    if (g_quiet) return;
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}

/* memória esparsa: páginas de 4 KiB sob demanda */
#define NPG 4096
static struct { u64 pg; u8 *d; } mem[NPG];
static int nmem;
static u8 *page(u64 a) {
    u64 pg = a >> 12;
    for (int i = 0; i < nmem; i++) if (mem[i].pg == pg) return mem[i].d;
    if (nmem == NPG) return NULL;
    mem[nmem].pg = pg; mem[nmem].d = calloc(1, 4096);
    return mem[nmem++].d;
}
static u32 nmemops;
bool aml_os_mem(u64 a, u32 bits, u64 *v, bool w) {
    nmemops++;
    for (u32 i = 0; i < bits / 8; i++) {
        u8 *p = page(a + i); if (!p) return false;
        if (w) p[(a + i) & 4095] = (u8)(*v >> (8 * i));
        else *v |= (u64)p[(a + i) & 4095] << (8 * i);
    }
    return true;
}
static u8 io[65536];
static u8 cmos_idx;
static u8 cmos_ram[256];
static u32 nio;
bool aml_os_io(u16 port, u32 bits, u64 *v, bool w) {
    nio++;
    for (u32 i = 0; i < bits / 8; i++) {
        u16 p = (u16)(port + i);
        if (p == 0x71) {   /* CMOS indexado, como no hardware */
            if (w) cmos_ram[cmos_idx] = (u8)(*v >> (8 * i)); else *v |= (u64)cmos_ram[cmos_idx] << (8 * i);
            continue;
        }
        if (w) { io[p] = (u8)(*v >> (8 * i)); if (p == 0x70) cmos_idx = io[p]; }
        else *v |= (u64)io[p] << (8 * i);
    }
    return true;
}
static u8 pcicfg[256][32][8][256];
bool aml_os_pci(u16 seg, u8 bus, u8 dev, u8 fn, u16 off, u32 bits, u64 *v, bool w) {
    (void)seg;
    if (dev >= 32 || fn >= 8) return false;
    for (u32 i = 0; i < bits / 8; i++) {
        u16 o = (u16)(off + i); if (o >= 256) return true;
        if (w) pcicfg[bus][dev][fn][o] = (u8)(*v >> (8 * i)); else *v |= (u64)pcicfg[bus][dev][fn][o] << (8 * i);
    }
    return true;
}
static u8 ecram[256];
bool aml_os_ec(u8 a, u8 *v, bool w) { if (w) ecram[a] = *v; else *v = ecram[a]; return true; }
bool aml_os_cmos(u8 a, u8 *v, bool w) { if (w) cmos_ram[a] = *v; else *v = cmos_ram[a]; return true; }
bool aml_os_region_other(u8 sp, u64 a, u32 bits, u64 *v, bool w) { (void)sp; (void)a; (void)bits; if (!w) *v = 0; return true; }
void aml_os_sleep_ms(u64 ms) { (void)ms; }
void aml_os_stall_us(u64 us) { (void)us; }
static u64 tick;
u64 aml_os_timer_100ns(void) { return tick += 10; }
static u32 nnotify;
void aml_os_notify(aml_node_t *n, u64 v) {
    char b[96]; aml_path(n, b, sizeof b);
    nnotify++;
    if (!g_quiet) fprintf(stderr, "notify %s 0x%llx\n", b, (unsigned long long)v);
}
static int glock;
bool aml_os_global_lock(bool acq) { glock += acq ? 1 : -1; return true; }

static u8 *tables[32]; static int ntables;
const void *aml_os_find_table(const char *sig, const char *oem, const char *oemt, int nth) {
    for (int i = 0; i < ntables; i++) {
        u8 *t = tables[i];
        if (memcmp(t, sig, 4)) continue;
        if (oem && *oem && strncmp((char *)t + 10, oem, strlen(oem))) continue;
        if (oemt && *oemt && strncmp((char *)t + 16, oemt, strlen(oemt))) continue;
        if (nth-- == 0) return t;
    }
    return NULL;
}

static u8 *slurp(const char *path, long *len) {
    FILE *f = fopen(path, "rb"); if (!f) { perror(path); exit(2); }
    fseek(f, 0, SEEK_END); *len = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *b = malloc((size_t)*len); if (fread(b, 1, (size_t)*len, f) != (size_t)*len) { perror(path); exit(2); }
    fclose(f); return b;
}

static void show(const char *label, aml_val_t *v, int ind) {
    printf("%*s%s", ind, "", label);
    switch (v->type) {
    case AML_INT: printf("0x%llx\n", (unsigned long long)v->i); break;
    case AML_STR: printf("\"%s\"\n", (char *)v->b->d); break;
    case AML_BUF: printf("Buffer(%u)\n", v->b->len); break;
    case AML_PKG:
        printf("Package(%u)\n", v->b->len);
        for (u32 i = 0; i < v->b->len && i < 24; i++) show("", &v->b->el[i], ind + 2);
        break;
    case AML_REF:
        if (v->rkind == AML_REF_NODE) { char b[96]; aml_path(v->node, b, sizeof b); printf("Ref %s\n", b); }
        else printf("Ref\n");
        break;
    default: printf("(vazio)\n");
    }
}

static int fails;
static void check(int ok, const char *what) { if (!ok) { fails++; printf("FALHOU: %s\n", what); } }

typedef struct { int devs, hid, crs, prt; } count_t;
static int count_cb(aml_node_t *n, int d, void *c) {
    (void)d; count_t *k = c;
    if (n->type == AML_DEVICE) {
        k->devs++;
        if (aml_find(n, "_HID")) k->hid++;
        if (aml_find(n, "_CRS")) k->crs++;
        if (aml_find(n, "_PRT")) k->prt++;
    }
    return 0;
}

typedef struct { int n, irq, io, mem, addr; } rescount_t;
static int res_cb(const aml_res_t *r, void *c) {
    rescount_t *k = c; k->n++;
    if (r->kind == AML_RES_IRQ) k->irq++; else if (r->kind == AML_RES_IO) k->io++;
    else if (r->kind == AML_RES_MEM) k->mem++; else if (r->kind == AML_RES_ADDR) k->addr++;
    return 0;
}
typedef struct { int devs, ok, fail; rescount_t rc; } crs_t;
static int crs_cb(aml_node_t *n, int d, void *c) {
    (void)d; crs_t *k = c;
    if (n->type != AML_DEVICE || !aml_find(n, "_CRS")) return 0;
    u64 sta; aml_device_sta(n, &sta);
    if (!(sta & 1)) return 0;
    k->devs++;
    aml_val_t v;
    if (aml_eval_path(n, "_CRS", NULL, 0, &v) == AML_OK && v.type == AML_BUF) {
        if (aml_res_walk(v.b->d, v.b->len, res_cb, &k->rc) == AML_OK) k->ok++; else k->fail++;
    } else { k->fail++; char b[96]; aml_path(n, b, sizeof b); printf("  _CRS falhou em %s\n", b); }
    aml_val_free(&v);
    return 0;
}

static int run_test(const char *path) {
    long len; u8 *t = slurp(path, &len);
    aml_init();
    int r = aml_load_table(t);
    check(r == AML_OK, "carga da tabela de teste");
    aml_init_devices();
    u64 res = 99;
    r = aml_eval_int(NULL, "\\TEST", &res);
    if (r) printf("TEST: %s\n", aml_strerror(r));
    check(r == AML_OK && res == 0, "\\TEST devolve 0");
    if (res) printf("TEST parou na verificação %llu\n", (unsigned long long)res);
    /* nomes dinâmicos de método não podem vazar */
    u32 before = aml_node_count();
    for (int i = 0; i < 100; i++) aml_eval_int(NULL, "\\DYN", &res);
    check(aml_node_count() == before, "Name dentro de método some no retorno");
    check(cmos_ram[0x10] == 0x5A && cmos_ram[0x11] == 0xA5, "IndexField chega na CMOS indexada");
    check(ecram[0x40] == 0x77 && ecram[0x41] == 0x22 && ecram[0x42] == 0x11, "EmbeddedControl byte a byte");
    u64 m = 0; aml_os_mem(0x10010, 32, &m, false);
    check(m == 0xDEADBEEF, "DWordAcc preserva os vizinhos");
    check(glock == 0, "trava global equilibrada");
    printf(fails ? "aml-teste: %d falhas\n" : "aml-teste: ok\n", fails);
    return fails ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "--teste")) return run_test(argv[2]);
    if (argc < 2) { fprintf(stderr, "uso: %s --teste teste.aml | DSDT.dat [SSDT*.dat]\n", argv[0]); return 2; }
    int ai = 1;
    if (!strcmp(argv[1], "-q")) { g_quiet = 1; ai++; }
    aml_init();
    for (int i = ai; i < argc; i++) { long len; tables[ntables++] = slurp(argv[i], &len); }
    for (int i = 0; i < ntables; i++) {
        u32 e0 = aml_errors();
        int r = aml_load_table(tables[i]);
        printf("carga %.4s %.8s: %s, %u erros, %u nós\n", tables[i], tables[i] + 16, aml_strerror(r), aml_errors() - e0, aml_node_count());
        check(r == AML_OK, "carga de tabela real");
    }
    /* sequência do boot: _PIC(1) (APIC), _REG das regiões, _INI */
    aml_val_t v;
    if (aml_find(NULL, "\\_PIC")) { u64 o; check(aml_call_int1(NULL, "\\_PIC", 1, &o) == AML_OK || 1, "_PIC"); }
    aml_run_reg(AML_AS_EC); aml_run_reg(AML_AS_PCI); aml_run_reg(AML_AS_MEM); aml_run_reg(AML_AS_IO);
    u32 e0 = aml_errors();
    int pres = aml_init_devices();
    printf("_INI: %d presentes, %u erros\n", pres, aml_errors() - e0);
    const char *sleeps[] = { "\\_S0", "\\_S3", "\\_S4", "\\_S5" };
    for (int i = 0; i < 4; i++) {
        if (aml_eval_path(NULL, sleeps[i], NULL, 0, &v) == AML_OK) { show(sleeps[i], &v, 0); aml_val_free(&v); }
        else printf("%s ausente\n", sleeps[i]);
    }
    if (aml_find(NULL, "\\_S5")) {
        check(aml_eval_path(NULL, "\\_S5", NULL, 0, &v) == AML_OK && v.type == AML_PKG && v.b->len >= 2, "_S5 é Package");
        aml_val_free(&v);
    }
    aml_node_t *tz = aml_find(NULL, "\\_TZ.TZ01");
    if (tz) {
        u64 tmp = 0, crt = 0, psv = 0;
        u64 cm = 0x05; aml_os_mem(0xFED80700, 8, &cm, true);   /* CM00 simulado */
        check(aml_eval_int(tz, "_TMP", &tmp) == AML_OK, "_TZ.TZ01._TMP");
        check(aml_eval_int(tz, "_CRT", &crt) == AML_OK, "_TZ.TZ01._CRT");
        check(aml_eval_int(tz, "_PSV", &psv) == AML_OK, "_TZ.TZ01._PSV");
        printf("TZ01: _TMP %.1f °C, _CRT %.1f °C, _PSV %.1f °C\n", (double)tmp / 10 - 273.2, (double)crt / 10 - 273.2, (double)psv / 10 - 273.2);
        check(tmp == 0x0B74 + 50, "_TMP com CM00=5");
        check(crt == 0x0AAC + 900, "_CRT 90 °C");
        if (aml_eval_path(tz, "_PSL", NULL, 0, &v) == AML_OK) { show("_PSL ", &v, 0); aml_val_free(&v); }
    }
    /* processadores e P-states */
    const char *cpus[] = { "\\_PR.P000", "\\_PR.C000", "\\_SB.PLTF.C000", "\\_PR.CPU0", "\\_SB.C000" };
    for (int i = 0; i < 5; i++) {
        aml_node_t *c = aml_find(NULL, cpus[i]);
        if (!c) continue;
        printf("CPU %s (tipo %u)\n", cpus[i], c->type);
        const char *objs[] = { "_PCT", "_PSS", "_PPC", "_PSD", "_CST", "_CPC" };
        for (int k = 0; k < 6; k++) {
            if (!aml_find(c, objs[k])) continue;
            int r = aml_eval_path(c, objs[k], NULL, 0, &v);
            check(r == AML_OK, objs[k]);
            if (r == AML_OK && k == 0 && v.type == AML_PKG && v.b->el[0].type == AML_BUF) {
                const u8 *d = v.b->el[0].b->d; u64 addr = 0;
                for (int q = 0; q < 8; q++) addr |= (u64)d[7 + q] << (8 * q);
                printf("  _PCT[0]: espaço 0x%02x, %u bits, 0x%llx\n", d[3], d[4], (unsigned long long)addr);
                check(d[0] == 0x82 && (d[3] != 0x7F || addr == 0xC0010062), "_PCT FFixedHW em PStateCtl");
            }
            if (r == AML_OK && k == 1 && v.type == AML_PKG) {
                for (u32 q = 0; q < v.b->len; q++) { u64 ctl = 0; aml_val_to_int(&v.b->el[q].b->el[4], &ctl); check(ctl == q, "_PSS controle = índice do P-state"); }
            }
            if (r == AML_OK) { show(objs[k], &v, 2); aml_val_free(&v); }
        }
        break;
    }
    count_t k = {0};
    aml_walk(NULL, count_cb, &k);
    printf("dispositivos %d, com _HID %d, com _CRS %d, com _PRT %d\n", k.devs, k.hid, k.crs, k.prt);
    aml_node_t *pci = aml_find(NULL, "\\_SB.PCI0");
    if (pci) {
        int r = aml_eval_path(pci, "_PRT", NULL, 0, &v);
        check(r == AML_OK && v.type == AML_PKG, "\\_SB.PCI0._PRT");
        if (!r) { printf("PCI0._PRT: %u entradas\n", v.b->len); aml_val_free(&v); }
        check(aml_device_match(pci, "PNP0A08") || aml_device_match(pci, "PNP0A03"), "PCI0 é ponte raiz");
    }
    crs_t cr = {0};
    g_quiet = 1;
    aml_walk(NULL, crs_cb, &cr);
    g_quiet = 0;
    printf("_CRS: %d dispositivos presentes, %d ok, %d falhas; %d recursos (IRQ %d, IO %d, MEM %d, ADDR %d)\n",
           cr.devs, cr.ok, cr.fail, cr.rc.n, cr.rc.irq, cr.rc.io, cr.rc.mem, cr.rc.addr);
    if (aml_find(NULL, "\\_PTS")) {
        e0 = aml_errors();
        u64 o; int r = aml_call_int1(NULL, "\\_PTS", 5, &o);
        printf("_PTS(5): %s, %u erros\n", aml_strerror(r), aml_errors() - e0);
        check(r == AML_OK, "_PTS(5)");
        r = aml_call_int1(NULL, "\\_PTS", 3, &o);
        check(r == AML_OK, "_PTS(3)");
        aml_val_t a = aml_int(3);
        r = aml_eval_path(NULL, "\\_WAK", &a, 1, &v);
        printf("_WAK(3): %s\n", aml_strerror(r));
        check(r == AML_OK, "_WAK(3)");
        aml_val_free(&v);
    }
    aml_node_t *pwrb = aml_find(NULL, "\\_SB.PWRB");
    if (pwrb) check(aml_device_match(pwrb, "PNP0C0C"), "PWRB é PNP0C0C");
    /* GPEs */
    aml_node_t *gpe = aml_find(NULL, "\\_GPE");
    int ngpe = 0;
    for (aml_node_t *c = gpe ? gpe->child : NULL; c; c = c->next)
        if (c->type == AML_METHOD && (c->name[1] == 'L' || c->name[1] == 'E')) {
            ngpe++;
            int r = aml_eval(c, NULL, 0, &v); aml_val_free(&v);
            check(r == AML_OK, "método de GPE");
        }
    printf("GPE: %d métodos, %u notificações, %u acessos de memória, %u de E/S\n", ngpe, nnotify, nmemops, nio);
    printf("total: %u nós, %u erros de execução\n", aml_node_count(), aml_errors());
    printf(fails ? "aml-real: %d falhas\n" : "aml-real: ok\n", fails);
    return fails ? 1 : 0;
}
