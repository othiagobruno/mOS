/* mOS — escrita de tabela de partições GPT (MBR protetor + cabeçalhos primário e de backup) */
#include "fs.h"

static u32 crc_tab[256];
u32 crc32_calc(const void *data, usize n) {
    if (!crc_tab[1])
        for (u32 i = 0; i < 256; i++) {
            u32 c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            crc_tab[i] = c;
        }
    const u8 *p = data;
    u32 c = ~0u;
    for (usize i = 0; i < n; i++) c = crc_tab[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return ~c;
}

static void put32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static void put64(u8 *p, u64 v) { put32(p, (u32)v); put32(p + 4, (u32)(v >> 32)); }
static void rand_guid(u8 *g) {
    for (int i = 0; i < 16; i += 4) put32(g + i, rand32() ^ (u32)rdtsc());
    g[7] = (g[7] & 0x0F) | 0x40;   /* versão 4 */
    g[8] = (g[8] & 0x3F) | 0x80;
}

/* EFI System Partition: C12A7328-F81F-11D2-BA4B-00A0C93EC93B (campos iniciais little-endian) */
static const u8 ESP_GUID[16] = {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B};

/* Apaga a tabela atual e cria uma GPT com uma única partição EFI (FAT32 vai dentro dela) ocupando o
 * disco todo a partir de 1 MiB. Devolve o deslocamento e o tamanho da partição em bytes. */
bool gpt_create_single_esp(blkdev_t *disk, const char *name, u64 *part_off, u64 *part_size) {
    const u64 ss = 512;
    u64 nsec = disk->size / ss;
    if (!blk_writable(disk) || nsec < 2048 + 34 + 70000) return false;
    u64 first = 2048, last_usable = nsec - 34, last = last_usable;
    last = (last + 1) / 2048 * 2048 - 1;            /* termina alinhada a 1 MiB */
    if (last < first) last = last_usable;
    u8 *ent = kcalloc(128, 128);                    /* 128 entradas de 128 bytes = 32 setores */
    u8 *hdr = kcalloc(1, ss);
    u8 *mbr = kcalloc(1, ss);
    if (!ent || !hdr || !mbr) { kfree(ent); kfree(hdr); kfree(mbr); return false; }
    memcpy(ent, ESP_GUID, 16);
    rand_guid(ent + 16);
    put64(ent + 32, first);
    put64(ent + 40, last);
    for (int i = 0; name && name[i] && i < 36; i++) ent[56 + i * 2] = (u8)name[i];   /* UTF-16LE (ASCII) */
    u32 ecrc = crc32_calc(ent, 128 * 128);

    /* MBR protetor */
    mbr[446 + 4] = 0xEE;
    mbr[446 + 1] = 0x00; mbr[446 + 2] = 0x02;
    mbr[446 + 5] = 0xFF; mbr[446 + 6] = 0xFF; mbr[446 + 7] = 0xFF;
    put32(mbr + 446 + 8, 1);
    put32(mbr + 446 + 12, nsec - 1 > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)(nsec - 1));
    mbr[510] = 0x55; mbr[511] = 0xAA;

    u8 disk_guid[16];
    rand_guid(disk_guid);
    bool ok = true;
    for (int copy = 0; copy < 2 && ok; copy++) {
        memset(hdr, 0, ss);
        memcpy(hdr, "EFI PART", 8);
        put32(hdr + 8, 0x00010000);
        put32(hdr + 12, 92);
        u64 my = copy ? nsec - 1 : 1, alt = copy ? 1 : nsec - 1, elba = copy ? nsec - 33 : 2;
        put64(hdr + 24, my);
        put64(hdr + 32, alt);
        put64(hdr + 40, 34);
        put64(hdr + 48, last_usable);
        memcpy(hdr + 56, disk_guid, 16);
        put64(hdr + 72, elba);
        put32(hdr + 80, 128);
        put32(hdr + 84, 128);
        put32(hdr + 88, ecrc);
        put32(hdr + 16, crc32_calc(hdr, 92));
        ok = blk_write(disk, elba * ss, ent, 128 * 128) && blk_write(disk, my * ss, hdr, ss);
    }
    ok = ok && blk_write(disk, 0, mbr, ss) && blk_flush(disk);
    kfree(ent); kfree(hdr); kfree(mbr);
    if (ok) { *part_off = first * ss; *part_size = (last - first + 1) * ss; }
    return ok;
}
