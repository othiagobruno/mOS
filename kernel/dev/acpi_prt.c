/* mOS — roteamento de INTx PCI pelo _PRT (ACPI 6.2.13) em modo PIC (\_PIC(0))
 *
 * acpi_pci_irq(bus, dev, fn, pin) devolve a IRQ do 8259 (0..15) de um pino INTA..INTD:
 *  - barramento 0: _PRT da ponte raiz (PNP0A03/PNP0A08);
 *  - atrás de pontes: _PRT da própria ponte, ou a regra de rotação (pino + dispositivo) % 4 até achar um
 *    _PRT acima (especificação PCI-to-PCI Bridge 9.1);
 *  - fonte = link device (PNP0C0F): IRQ atual do _CRS; se desligado, a primeira do _PRS gravada por _SRS;
 *  - fonte = 0: o índice já é a interrupção global (só faz sentido abaixo de 16 no modo PIC).
 * Só depende do aml.h e de um leitor de configuração PCI (o kernel passa o dele; o harness, um falso).
 */
#include "aml.h"
#ifdef AML_HOST
#include <string.h>
#else
#include "kernel.h"
#endif

typedef u32 (*cfg_fn)(u8 bus, u8 dev, u8 fn, u8 off);

static aml_node_t *root_bridge(void) {
    aml_node_t *sb = aml_find(NULL, "\\_SB");
    for (aml_node_t *c = sb ? sb->child : NULL; c; c = c->next)
        if (c->type == AML_DEVICE && (aml_device_match(c, "PNP0A08") || aml_device_match(c, "PNP0A03"))) return c;
    return NULL;
}

static aml_node_t *child_adr(aml_node_t *p, u32 adr) {
    for (aml_node_t *c = p ? p->child : NULL; c; c = c->next) {
        if (c->type != AML_DEVICE) continue;
        u64 a;
        if (aml_find(c, "_ADR") && aml_eval_int(c, "_ADR", &a) == AML_OK && (u32)a == adr) return c;
    }
    return NULL;
}

/* IRQ de um link device: _CRS atual ou primeira opção do _PRS programada por _SRS */
static int res_first_irq_cb(const aml_res_t *r, void *ctx) {
    int *irq = ctx;
    if (r->kind == AML_RES_IRQ && r->count) { *irq = (int)r->irq[0]; return 1; }
    return 0;
}
/* penalidade de cada IRQ do 8259 (como o pci_link do Linux): legadas do ISA quase nunca, SCI evita */
static int g_penalty[16] = { 1000, 1000, 1000, 100, 1000, 0, 1000, 0, 1000, 50, 0, 0, 1000, 1000, 1000, 1000 };
typedef struct { int irq, best; } pick_t;
static int res_pick_cb(const aml_res_t *r, void *ctx) {
    pick_t *p = ctx;
    if (r->kind != AML_RES_IRQ) return 0;
    for (int i = 0; i < r->count; i++) {
        int q = (int)r->irq[i];
        if (q <= 0 || q > 15) continue;
        if (g_penalty[q] < p->best) { p->best = g_penalty[q]; p->irq = q; }
    }
    return 0;
}

static int link_irq(aml_node_t *lnk) {
    aml_val_t v;
    int irq = -1;
    u64 sta = 0x0F;
    aml_device_sta(lnk, &sta);
    if ((sta & 2) && aml_eval_path(lnk, "_CRS", NULL, 0, &v) == AML_OK) {
        if (v.type == AML_BUF) aml_res_walk(v.b->d, v.b->len, res_first_irq_cb, &irq);
        aml_val_free(&v);
        if (irq > 0) { g_penalty[irq] += 10; return irq; }
    }
    irq = -1;
    if (aml_eval_path(lnk, "_PRS", NULL, 0, &v) == AML_OK) {
        pick_t pk = { -1, 1 << 30 };
        if (v.type == AML_BUF) aml_res_walk(v.b->d, v.b->len, res_pick_cb, &pk);
        aml_val_free(&v);
        irq = pk.irq;
    }
    if (irq <= 0 || irq > 15) return -1;
    g_penalty[irq] += 10;                              /* espalha os próximos links */
    /* _SRS com um descritor IRQ de 3 bytes (nível, ativo baixo, compartilhado) + EndTag */
    u8 buf[6] = { 0x23, (u8)(1u << irq), (u8)((1u << irq) >> 8), 0x18, 0x79, 0 };
    aml_val_t arg; memset(&arg, 0, sizeof arg);
    if (aml_make_buffer(&arg, buf, sizeof buf) != AML_OK) return -1;
    aml_val_t r;
    int e = aml_eval_path(lnk, "_SRS", &arg, 1, &r);
    aml_val_free(&arg); aml_val_free(&r);
    if (e != AML_OK) return -1;
    /* confere: há firmware com _SRS errado (no PC do Thiago, LNKE..LNKH leem o PIRx atual em vez do
     * argumento); se o _CRS não mostrar a IRQ pedida, a rota não existe de verdade */
    int now = -1;
    if (aml_eval_path(lnk, "_CRS", NULL, 0, &v) == AML_OK) {
        if (v.type == AML_BUF) aml_res_walk(v.b->d, v.b->len, res_first_irq_cb, &now);
        aml_val_free(&v);
    }
    if (now != irq) {
        char nm[48]; aml_path(lnk, nm, sizeof nm);
        aml_os_log("ACPI: %s._SRS(IRQ %d) não teve efeito (_CRS = %d): firmware com defeito, sem rota\n", nm, irq, now);
        aml_val_t d; aml_eval_path(lnk, "_DIS", NULL, 0, &d); aml_val_free(&d);
        return -1;
    }
    return irq;
}

static int prt_lookup(aml_node_t *br, u8 dev, u8 pin) {
    aml_val_t prt;
    if (!aml_find(br, "_PRT") || aml_eval_path(br, "_PRT", NULL, 0, &prt) != AML_OK) return -2;
    int irq = -1;
    if (prt.type == AML_PKG) {
        for (u32 i = 0; i < prt.b->len; i++) {
            aml_val_t *e = &prt.b->el[i];
            if (e->type != AML_PKG || e->b->len < 4) continue;
            u64 adr = 0, p = 0, idx = 0;
            aml_val_to_int(&e->b->el[0], &adr); aml_val_to_int(&e->b->el[1], &p); aml_val_to_int(&e->b->el[3], &idx);
            if (((adr >> 16) & 0xFFFF) != dev || p != pin) continue;
            aml_val_t *src = &e->b->el[2];
            if (src->type == AML_REF && src->rkind == AML_REF_NODE) irq = link_irq(src->node);
            else if (src->type == AML_STR) {           /* nome ainda não definido quando o Package foi criado: busca subindo */
                aml_node_t *n = NULL;
                for (aml_node_t *s = br; s && !n; s = s->parent) n = aml_find(s, (const char *)src->b->d);
                irq = n ? link_irq(n) : -1;
            }
            else irq = (int)idx;
            break;
        }
    }
    aml_val_free(&prt);
    return irq;
}

/* nó ACPI e (bus) da ponte que leva ao barramento `bus`; profundidade limitada */
static int route(cfg_fn cfg, aml_node_t *rb, u8 bus, u8 dev, u8 pin, int depth) {
    if (depth > 8) return -1;
    if (bus == 0) return prt_lookup(rb, dev, pin);
    /* acha a ponte cujo barramento secundário é `bus`, varrendo os barramentos anteriores */
    for (int b = 0; b < bus; b++) {
        for (u8 d = 0; d < 32; d++) for (u8 f = 0; f < 8; f++) {
            u32 id = cfg((u8)b, d, f, 0);
            if ((id & 0xFFFF) == 0xFFFF) { if (f == 0) break; continue; }
            u32 hdr = (cfg((u8)b, d, f, 0x0C) >> 16) & 0x7F;
            if (hdr != 1) { if (f == 0 && !((cfg((u8)b, d, f, 0x0C) >> 23) & 1)) break; continue; }
            u32 busr = cfg((u8)b, d, f, 0x18);
            if (((busr >> 8) & 0xFF) != bus) continue;
            /* nó ACPI da ponte: debaixo do nó do barramento pai */
            aml_node_t *parent = rb;
            if (b != 0) parent = NULL;   /* pais aninhados: resolve pelo _PRT de cima com rotação */
            aml_node_t *bn = parent ? child_adr(parent, (u32)d << 16 | f) : NULL;
            int r = bn ? prt_lookup(bn, dev, pin) : -2;
            if (r != -2) return r;
            return route(cfg, rb, (u8)b, d, (u8)((pin + dev) % 4), depth + 1);   /* rotação da ponte */
        }
    }
    return -1;
}

int acpi_pci_irq_cfg(cfg_fn cfg, u8 bus, u8 dev, u8 fn, u8 pin) {
    (void)fn;
    if (pin > 3) return -1;
    aml_node_t *rb = root_bridge();
    if (!rb) return -1;
    int r = route(cfg, rb, bus, dev, pin, 0);
    return r >= 0 && r < 16 ? r : -1;
}
