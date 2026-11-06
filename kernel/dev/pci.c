/* mOS — enumeração do barramento PCI (mecanismo de configuração #1) */
#include "kernel.h"

static pci_dev_t devs[64];
static int ndevs;

static u32 cfg_read(u8 bus, u8 dev, u8 fn, u8 off) {
    outl(0xCF8, 0x80000000u | (bus << 16) | (dev << 11) | (fn << 8) | (off & 0xFC));
    return inl(0xCFC);
}
static void cfg_write(u8 bus, u8 dev, u8 fn, u8 off, u32 v) {
    outl(0xCF8, 0x80000000u | (bus << 16) | (dev << 11) | (fn << 8) | (off & 0xFC));
    outl(0xCFC, v);
}

u32 pci_read(pci_dev_t *d, u8 off) { return cfg_read(d->bus, d->dev, d->fn, off); }
void pci_write(pci_dev_t *d, u8 off, u32 v) { cfg_write(d->bus, d->dev, d->fn, off, v); }

void pci_enable_bus_master(pci_dev_t *d) {
    u32 c = pci_read(d, 0x04);
    c |= 0x07;          /* I/O, memória, bus master */
    c &= ~(1u << 10);   /* interrupções INTx habilitadas */
    pci_write(d, 0x04, c);
}

u64 pci_bar_addr(pci_dev_t *d, int i) {
    u32 b = d->bar[i];
    if (b & 1) return b & ~3u;
    u64 a = b & ~15u;
    if (((b >> 1) & 3) == 2 && i < 5) a |= (u64)d->bar[i + 1] << 32;
    return a;
}

static void scan_fn(u8 bus, u8 dev, u8 fn) {
    u32 id = cfg_read(bus, dev, fn, 0);
    if ((id & 0xFFFF) == 0xFFFF || ndevs >= (int)ARRAY_LEN(devs)) return;
    pci_dev_t *d = &devs[ndevs++];
    d->bus = bus; d->dev = dev; d->fn = fn;
    d->vendor = id & 0xFFFF; d->device = id >> 16;
    u32 cls = cfg_read(bus, dev, fn, 8);
    d->class_ = cls >> 24; d->subclass = (cls >> 16) & 0xFF; d->progif = (cls >> 8) & 0xFF;
    d->irq = cfg_read(bus, dev, fn, 0x3C) & 0xFF;
    u32 hdr = (cfg_read(bus, dev, fn, 0x0C) >> 16) & 0x7F;
    if (hdr == 0) for (int i = 0; i < 6; i++) d->bar[i] = cfg_read(bus, dev, fn, 0x10 + i * 4);
    if (d->class_ == 6 && d->subclass == 4) { /* ponte PCI-PCI: varre o barramento secundário */
        u8 sec = (cfg_read(bus, dev, fn, 0x18) >> 8) & 0xFF;
        if (sec && sec != bus) for (u8 dv = 0; dv < 32; dv++) for (u8 f = 0; f < 8; f++) {
            if (cfg_read(sec, dv, f, 0) != 0xFFFFFFFF) scan_fn(sec, dv, f);
            if (f == 0 && !(cfg_read(sec, dv, 0, 0x0C) & 0x800000)) break;
        }
    }
}

void pci_init(void) {
    for (u16 dev = 0; dev < 32; dev++) {
        if ((cfg_read(0, dev, 0, 0) & 0xFFFF) == 0xFFFF) continue;
        bool multi = cfg_read(0, dev, 0, 0x0C) & 0x800000;
        for (u8 fn = 0; fn < (multi ? 8 : 1); fn++) scan_fn(0, dev, fn);
    }
    for (int i = 0; i < ndevs; i++) {
        pci_dev_t *d = &devs[i];
        kprintf("PCI %02x:%02x.%x %04x:%04x %s (irq %d)\n", d->bus, d->dev, d->fn, d->vendor, d->device,
                pci_class_name(d->class_, d->subclass), d->irq);
    }
}

int pci_count(void) { return ndevs; }
pci_dev_t *pci_get(int i) { return i >= 0 && i < ndevs ? &devs[i] : NULL; }

pci_dev_t *pci_find_class(u8 cls, u8 sub, int nth) {
    for (int i = 0; i < ndevs; i++)
        if (devs[i].class_ == cls && devs[i].subclass == sub && nth-- == 0) return &devs[i];
    return NULL;
}

pci_dev_t *pci_find(u16 vendor, u16 device) {
    for (int i = 0; i < ndevs; i++)
        if (devs[i].vendor == vendor && (device == 0xFFFF || devs[i].device == device)) return &devs[i];
    return NULL;
}

const char *pci_class_name(u8 c, u8 s) {
    switch (c) {
    case 1: return s == 6 ? "Controlador SATA (AHCI)" : s == 8 ? "Controlador NVMe" : s == 1 ? "Controlador IDE" : "Armazenamento";
    case 2: return s == 0x80 ? "Rede sem fio" : "Controlador Ethernet";
    case 3: return "Controlador de vídeo";
    case 4: return s == 3 ? "Áudio HD" : s == 1 ? "Áudio" : "Multimídia";
    case 6: return s == 0 ? "Ponte host" : s == 1 ? "Ponte ISA" : s == 4 ? "Ponte PCI" : "Ponte";
    case 0x0C: return s == 3 ? "Controlador USB" : s == 5 ? "SMBus" : "Barramento serial";
    case 0x0D: return "Controlador sem fio";
    default: return "Dispositivo";
    }
}
