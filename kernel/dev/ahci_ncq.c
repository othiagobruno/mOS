#include "ahci_ncq.h"

void ahci_tags_init(ahci_tags_t *t, int n) {
    if (n < 1) n = 1;
    if (n > AHCI_NCQ_TAGS) n = AHCI_NCQ_TAGS;
    t->used = 0;
    t->ntags = (u8)n;
    t->next = 0;
}

int ahci_tag_get(ahci_tags_t *t) {
    if (!t || !t->ntags) return -1;
    for (int i = 0; i < t->ntags; i++) {
        int s = (t->next + i) % t->ntags;
        if (!(t->used & (1u << s))) {
            t->used |= 1u << s;
            t->next = (u8)((s + 1) % t->ntags);
            return s;
        }
    }
    return -1;
}

void ahci_tag_put(ahci_tags_t *t, int tag) {
    if (!t || tag < 0 || tag >= t->ntags) return;
    t->used &= ~(1u << tag);
}

void ahci_batch_init(ahci_batch_t *b, int ntags) {
    if (!b) return;
    b->posted = 0;
    b->done = 0;
    if (ntags < 1) ntags = 1;
    if (ntags > AHCI_NCQ_TAGS) ntags = AHCI_NCQ_TAGS;
    b->ntags = (u8)ntags;
}

int ahci_batch_post(ahci_batch_t *b, int tag) {
    if (!b || tag < 0 || tag >= b->ntags) return -1;
    u32 bit = 1u << tag;
    if (b->posted & bit) return -1;
    b->posted |= bit;
    b->done &= ~bit;
    return 0;
}

void ahci_batch_irq(ahci_batch_t *b, u32 ci) {
    if (!b) return;
    b->done |= b->posted & ~ci;
}

u32 ahci_batch_pending(const ahci_batch_t *b) {
    return b ? (b->posted & ~b->done) : 0;
}

void ahci_batch_release(ahci_batch_t *b, int tag) {
    if (!b || tag < 0 || tag >= b->ntags) return;
    u32 bit = 1u << tag;
    b->posted &= ~bit;
    b->done &= ~bit;
}

void ahci_fpdma_fis(u8 *f, int write, u64 lba, u16 count, u8 tag) {
    for (int i = 0; i < 20; i++) f[i] = 0;
    f[0] = 0x27;
    f[1] = 0x80;
    f[2] = (u8)(write ? ATA_WRITE_FPDMA : ATA_READ_FPDMA);
    f[3] = (u8)count;
    f[4] = (u8)lba;
    f[5] = (u8)(lba >> 8);
    f[6] = (u8)(lba >> 16);
    f[7] = 0x40;
    f[8] = (u8)(lba >> 24);
    f[9] = (u8)(lba >> 32);
    f[10] = (u8)(lba >> 40);
    f[11] = (u8)(count >> 8);
    f[12] = (u8)(tag << 3);
}
