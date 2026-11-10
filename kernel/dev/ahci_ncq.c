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
