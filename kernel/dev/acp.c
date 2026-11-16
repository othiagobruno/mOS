/* mOS — co-processador de áudio AMD (ACP, 1022:15e2)
 *
 * O ACP do Raven (revisão 0x00, ACP3x) só serve áudio por I2S/PDM quando a placa liga um codec ou microfone
 * digital nele, e o firmware descreve esse codec como dispositivo ACPI filho do nó do ACP (ou com HID de
 * máquina AMDI*). No PC do Thiago o nó \_SB.PCI0.GP17.ACP só tem _ADR: todo o áudio (fone, microfone,
 * HDMI) vem pelos dois controladores HDA (dev/hda.c, dev/hda_hdmi.c), e o Linux também não cria placa de
 * som para o ACP nesta máquina. Então o driver confere isso no namespace e põe o ACP em D3hot pela
 * capacidade de gerência de energia do PCI (economia de energia), refazendo depois do S3.
 * Se um dia o firmware listar um codec no ACP, o log diz qual e o ACP fica em D0.
 */
#include "kernel.h"
#include "aml.h"
#include "pm.h"

static pci_dev_t *g_acp;
static u8 g_pmcap;
static char g_status[96] = "ACP: ausente";
static char g_endpoint[48];

static u8 pm_cap(pci_dev_t *d) {
    if (!((pci_read(d, 0x04) >> 16) & 0x10)) return 0;
    u8 p = (u8)(pci_read(d, 0x34) & 0xFC);
    for (int i = 0; p && i < 48; i++) {
        u32 v = pci_read(d, p);
        if ((v & 0xFF) == 0x01) return p;
        p = (u8)((v >> 8) & 0xFC);
    }
    return 0;
}

static void set_dstate(int ds) {
    if (!g_acp || !g_pmcap) return;
    u32 v = pci_read(g_acp, (u8)(g_pmcap + 4));
    if ((int)(v & 3) == ds) return;
    pci_write(g_acp, (u8)(g_pmcap + 4), (v & ~3u) | (u32)ds);
    sleep_ms(10);                                   /* D0 <-> D3hot: 10 ms (PCI PM 1.2, 5.6.1) */
}

/* algum codec ou microfone descrito para o ACP? (filhos do nó do ACP ou HIDs de máquina de áudio AMD) */
typedef struct { aml_node_t *acp; bool found; } scan_t;
static bool under(aml_node_t *n, aml_node_t *a) { for (; n; n = n->parent) if (n == a) return true; return false; }
static int scan_cb(aml_node_t *n, int d, void *c) {
    (void)d;
    scan_t *s = c;
    if (n->type != AML_DEVICE || s->found) return s->found ? -1 : 0;
    bool child = s->acp && n != s->acp && under(n, s->acp);
    static const char *const hids[] = { "AMDI0040", "AMDI1019", "AMDI5682", "AMDI1015", "AMDI0030", NULL };
    bool hid = false;
    for (int i = 0; hids[i] && !hid; i++) hid = aml_device_match(n, hids[i]);
    if (!child && !hid) return 0;
    u64 sta = 0x0F;
    aml_device_sta(n, &sta);
    if (!(sta & 1)) return 0;
    s->found = true;
    aml_path(n, g_endpoint, sizeof g_endpoint);
    return -1;
}

static int acp_suspend(int state) { (void)state; return 0; }
static void acp_resume(int state) { if (state == 3 && !g_endpoint[0]) set_dstate(3); }
static const pm_ops_t acp_pm = { "ACP", 0, acp_suspend, acp_resume, NULL };

void acp_init(void) {
    g_acp = pci_find(0x1022, 0x15e2);
    if (!g_acp) return;
    u8 rev = (u8)pci_read(g_acp, 0x08);
    g_pmcap = pm_cap(g_acp);
    scan_t s = { NULL, false };
    if (acpi_ready()) {
        /* nó ACPI do ACP: _ADR = função 5 debaixo da ponte que leva ao barramento do ACP */
        aml_node_t *sb = aml_find(NULL, "\\_SB");
        for (aml_node_t *r = sb ? sb->child : NULL; r && !s.acp; r = r->next) {
            if (r->type != AML_DEVICE) continue;
            for (aml_node_t *b = r->child; b && !s.acp; b = b->next)
                for (aml_node_t *c = b->child; c && !s.acp; c = c->next) {
                    u64 adr;
                    if (c->type == AML_DEVICE && aml_find(c, "_ADR") && aml_eval_int(c, "_ADR", &adr) == AML_OK &&
                        adr == ((u64)g_acp->dev << 16 | g_acp->fn) && b->type == AML_DEVICE) {
                        u64 badr; u8 sec = 0;
                        if (aml_eval_int(b, "_ADR", &badr) == AML_OK) {
                            pci_dev_t bd = { 0 }; bd.bus = 0; bd.dev = (u8)(badr >> 16); bd.fn = (u8)badr;
                            sec = (u8)(pci_read(&bd, 0x18) >> 8);
                        }
                        if (sec == g_acp->bus) s.acp = c;
                    }
                }
        }
        aml_walk(NULL, scan_cb, &s);
    }
    if (s.found) {
        snprintf(g_status, sizeof g_status, "ACP %02x:%02x.%x rev %02x: codec %s (D0)", g_acp->bus, g_acp->dev, g_acp->fn, rev, g_endpoint);
        kprintf("ACP: rev %02x com codec descrito em %s: fica em D0\n", rev, g_endpoint);
        return;
    }
    set_dstate(3);
    pm_register(&acp_pm);
    snprintf(g_status, sizeof g_status, "ACP %02x:%02x.%x rev %02x: sem I2S/PDM na placa, em D3hot", g_acp->bus, g_acp->dev, g_acp->fn, rev);
    kprintf("ACP: rev %02x%s, sem codec I2S nem microfone PDM descrito no firmware: em D3hot (áudio pelo HDA)\n",
            rev, s.acp ? "" : " (nó ACPI não achado)");
}

const char *acp_status(void) { return g_status; }
