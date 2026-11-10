/* mOS — tags NCQ (até 32) e FIS FPDMA, sem MMIO. */
#pragma once
#ifdef MOS_HOST_TEST
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#else
#include "types.h"
#endif

#define AHCI_NCQ_TAGS 32
#define ATA_READ_FPDMA  0x60
#define ATA_WRITE_FPDMA 0x61

typedef struct {
    u32 used;
    u8  ntags;
    u8  next;
} ahci_tags_t;

void ahci_tags_init(ahci_tags_t *t, int n);
int  ahci_tag_get(ahci_tags_t *t);
void ahci_tag_put(ahci_tags_t *t, int tag);

/* Register H2D de 20 bytes: READ/WRITE FPDMA QUEUED. `count` 0 = 65536 setores. */
void ahci_fpdma_fis(u8 *fis, int write, u64 lba, u16 count, u8 tag);
