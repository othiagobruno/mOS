/* mOS — identificação confiável da mídia de origem (ver include/origin.h) */
#include "origin.h"
#include "partscan.h"

static origin_t org;

const origin_t *origin_get(void) { return &org; }

static void derr(devpath_t *o, const char *m) { snprintf(o->err, sizeof o->err, "%s", m); }

bool origin_parse_devpath(const u8 *dp, usize len, devpath_t *o) {
    memset(o, 0, sizeof *o);
    if (!dp || len < 4) { derr(o, "caminho de dispositivo UEFI ausente"); return false; }
    int msg_kinds = 0;
    usize n = 0;
    bool ended = false;
    for (int nodes = 0; nodes < 64 && n + 4 <= len; nodes++) {
        u8 ty = dp[n], sub = dp[n + 1];
        u16 nl = le16(dp + n + 2);
        if (nl < 4 || n + nl > len) { derr(o, "nó de caminho malformado"); return false; }
        const u8 *x = dp + n;
        if (ty == 0x7F && sub == 0xFF) { ended = true; break; }
        if (ty == 0x01 && sub == 0x01 && nl >= 6) {                       /* hardware: PCI */
            if (o->phys.npci >= 8) { derr(o, "cadeia PCI longa demais"); return false; }
            o->phys.pci[o->phys.npci][0] = x[5];                          /* dispositivo */
            o->phys.pci[o->phys.npci][1] = x[4];                          /* função */
            o->phys.npci++;
        } else if (ty == 0x02) {                                          /* ACPI (PciRoot): ignorado */
        } else if (ty == 0x03 && sub == 0x12 && nl >= 10) {               /* SATA */
            if (le16(x + 6) != 0xFFFF) o->unsupported = true;             /* multiplicador de porta */
            o->phys.kind = PHYS_SATA;
            o->phys.sata_port = le16(x + 4);
            msg_kinds++;
        } else if (ty == 0x03 && sub == 0x17 && nl >= 16) {               /* NVMe */
            o->phys.kind = PHYS_NVME;
            o->phys.nsid = le32(x + 4);
            msg_kinds++;
        } else if (ty == 0x03 && sub == 0x05 && nl >= 6) {                /* USB(porta, interface) */
            o->phys.kind = PHYS_USB;
            if (o->phys.nusb >= 8) { derr(o, "cadeia USB longa demais"); return false; }
            o->phys.usb_port[o->phys.nusb++] = (u8)(x[4] + USB_PORT_BIAS);
            msg_kinds = msg_kinds ? msg_kinds : 1;
        } else if (ty == 0x03 && sub == 0x11 && nl >= 6) {                /* Unit (LUN) */
            if (le16(x + 4) != 0) o->unsupported = true;
        } else if (ty == 0x04 && sub == 0x01 && nl >= 42) {               /* Media: HD(partição) */
            o->have_part = true;
            o->part_no = le32(x + 4);
            o->part_start = le64(x + 8);
            o->part_size = le64(x + 16);
            memcpy(o->sig, x + 24, 16);
            o->sig_type = x[41];
        } else if (ty == 0x04) {
            o->unsupported = true;                                        /* CD-ROM, arquivo, etc. */
        } else o->unsupported = true;
        n += nl;
    }
    if (!ended) { derr(o, "caminho sem nó final"); return false; }
    if (msg_kinds != 1 || !o->phys.kind) { derr(o, "tipo de barramento não identificado"); return false; }
    if (o->phys.kind == PHYS_USB && !o->phys.nusb) { derr(o, "caminho USB sem porta"); return false; }
    o->valid = true;
    return true;
}

bool origin_phys_match(const diskphys_t *d, const diskphys_t *w) {
    if (!d->kind || d->kind != w->kind) return false;
    if (!d->npci || d->npci != w->npci) return false;
    for (int i = 0; i < d->npci; i++)
        if (d->pci[i][0] != w->pci[i][0] || d->pci[i][1] != w->pci[i][1]) return false;
    switch (d->kind) {
    case PHYS_SATA: return d->sata_port == w->sata_port;
    case PHYS_NVME: return d->nsid == w->nsid;
    case PHYS_USB:
        if (!d->nusb || d->nusb != w->nusb) return false;
        return !memcmp(d->usb_port, w->usb_port, d->nusb);
    }
    return false;
}

static void amb(const char *fmt, const char *a) {
    org.status = ORIGIN_AMBIGUOUS;
    snprintf(org.why, sizeof org.why, fmt, a ? a : "");
    kprintf("origem: AMBÍGUA/NÃO CONFIÁVEL — %s\n", org.why);
}

static bool part_identity_match(const devpath_t *w, const ptable_t *t, const part_t *p) {
    if (p->number != (int)w->part_no || p->first != w->part_start || p->last - p->first + 1 != w->part_size) return false;
    if (w->sig_type == 2) return t->kind == PT_GPT && !memcmp(p->uniq, w->sig, 16);
    if (w->sig_type == 1) return t->kind == PT_MBR && t->mbr_sig == le32(w->sig);
    return false;
}

void origin_resolve(void) {
    memset(&org, 0, sizeof org);
    org.status = ORIGIN_UNKNOWN;
    org.disk = -1;
    if (g_boot->boot_dp_len == 0) { amb("%s", "o firmware não informou o dispositivo de boot"); return; }
    if (g_boot->files_dp_len == 0) { amb("%s", "volume que forneceu \\mOS\\ sem caminho de dispositivo"); return; }
    if (g_boot->boot_dp_len != g_boot->files_dp_len || memcmp(g_boot->boot_dp, g_boot->files_dp, g_boot->boot_dp_len)) {
        amb("%s", "o kernel e \\mOS\\ foram carregados de volumes diferentes");
        return;
    }
    devpath_t w;
    if (!origin_parse_devpath(g_boot->boot_dp, g_boot->boot_dp_len, &w)) { amb("caminho UEFI: %s", w.err); return; }
    if (w.unsupported) { amb("%s", "caminho UEFI com nós que não sabemos casar"); return; }
    if (!w.have_part) { amb("%s", "caminho UEFI sem nó de partição"); return; }
    if (w.sig_type != 1 && w.sig_type != 2) { amb("%s", "partição sem assinatura (MBR/GPT) no caminho UEFI"); return; }
    /* (1) identidade física: exatamente um disco registrado */
    int cand = -1, ncand = 0;
    for (int i = 0; i < storage_disk_count(); i++)
        if (origin_phys_match(storage_disk_phys(i), &w.phys)) { cand = i; ncand++; }
    if (ncand != 1) { amb(ncand ? "%s" : "%s", ncand ? "mais de um disco casa com o caminho físico" : "nenhum disco registrado casa com o caminho físico"); return; }
    blkdev_t *disk = storage_disk(cand);
    if (disk->lbs && disk->lbs != 512) { amb("%s", "disco com setor lógico diferente de 512 bytes (não suportado)"); return; }
    /* (2) identidade da partição: exatamente uma partição do disco */
    ptable_t *t = kmalloc(sizeof *t);
    if (!t) { amb("%s", "sem memória"); return; }
    if (!partscan(disk, t)) { kfree(t); amb("%s", "falha ao ler a tabela de partições do disco"); return; }
    if (t->kind != PT_GPT && t->kind != PT_MBR) { kfree(t); amb("%s", "disco sem tabela de partições válida"); return; }
    int hit = -1, nhit = 0;
    for (int i = 0; i < t->n; i++) if (part_identity_match(&w, t, &t->p[i])) { hit = i; nhit++; }
    if (nhit != 1) { kfree(t); amb("%s", nhit ? "mais de uma partição casa com o nó HD()" : "nenhuma partição do disco casa com o nó HD() (número, início, tamanho e GUID)"); return; }
    part_t *p = &t->p[hit];
    org.part_number = p->number;
    org.part_off = p->first * 512;
    org.part_size = (p->last - p->first + 1) * 512;
    /* (3) cópia da mesma assinatura em outro disco (pendrive clonado): não dá para ter certeza */
    for (int i = 0; i < storage_disk_count(); i++) {
        if (i == cand) continue;
        ptable_t *o = kmalloc(sizeof *o);
        if (!o) continue;
        if (partscan(storage_disk(i), o)) {
            for (int k = 0; k < o->n; k++) {
                bool dup = (w.sig_type == 2 && o->kind == PT_GPT && !memcmp(o->p[k].uniq, w.sig, 16)) ||
                           (w.sig_type == 1 && o->kind == PT_MBR && o->mbr_sig == le32(w.sig));
                if (dup) { kfree(o); kfree(t); amb("a mesma assinatura de partição também existe em %s (cópia/clone presente)", storage_disk_name(i)); return; }
            }
        }
        kfree(o);
    }
    kfree(t);
    org.status = ORIGIN_TRUSTED;
    org.disk = cand;
    org.bus = w.phys.kind;
    snprintf(org.why, sizeof org.why, "%s partição %d", storage_disk_name(cand), org.part_number);
    kprintf("origem: CONFIÁVEL — %s (%s), partição %d [%llu, +%llu)\n", storage_disk_name(cand),
            org.bus == PHYS_USB ? "USB" : org.bus == PHYS_NVME ? "NVMe" : "SATA", org.part_number, org.part_off, org.part_size);
}
