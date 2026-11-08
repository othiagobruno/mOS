/* mOS — escrita de tabela de partições GPT (MBR protetor + cabeçalhos primário e de backup).
 *   gpt_create():        tabela NOVA com 1..4 partições (APAGA a tabela anterior; o instalador "disco inteiro" usa)
 *   gpt_add_partition(): acrescenta UMA partição numa entrada vazia de uma GPT válida, sem tocar nas demais
 *                        (instalador "espaço livre"); reescreve só as entradas e os dois cabeçalhos. */
#include "fs.h"
#include "crypto.h"
#include "storage.h"

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
    crypto_random(g, 16);                           /* CSPRNG (net/crypto.c): GUIDs únicos mesmo entre instalações no mesmo hardware */
    g[7] = (g[7] & 0x0F) | 0x40;   /* versão 4 */
    g[8] = (g[8] & 0x3F) | 0x80;
}

/* EFI System Partition: C12A7328-F81F-11D2-BA4B-00A0C93EC93B; Microsoft Basic Data: EBD0A0A2-B9E5-4433-87C0-68B6B72699C7
 * (campos iniciais little-endian) */
const u8 GPT_TYPE_ESP[16] = {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B};
const u8 GPT_TYPE_DATA[16] = {0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7};

static void fill_entry(u8 *ent, const gpt_new_t *p) {
    memset(ent, 0, 128);
    memcpy(ent, p->type, 16);
    rand_guid(ent + 16);
    put64(ent + 32, p->first);
    put64(ent + 40, p->last);
    for (int i = 0; p->name && p->name[i] && i < 36; i++) ent[56 + i * 2] = (u8)p->name[i];   /* UTF-16LE (ASCII) */
}

bool gpt_create(blkdev_t *disk, const gpt_new_t *parts, int n) {
    const u64 ss = 512;
    u64 nsec = disk->size / ss;
    if (!blk_writable(disk) || n < 1 || n > 4 || nsec < 2048 + 34 + 2048) return false;
    u64 last_usable = nsec - 34;
    for (int i = 0; i < n; i++) {
        if (parts[i].first < 34 || parts[i].last > last_usable || parts[i].first > parts[i].last) return false;
        for (int j = 0; j < i; j++) if (parts[i].first <= parts[j].last && parts[j].first <= parts[i].last) return false;
    }
    u8 *ent = kcalloc(128, 128);                    /* 128 entradas de 128 bytes = 32 setores */
    u8 *hdr = kcalloc(1, ss);
    u8 *mbr = kcalloc(1, ss);
    if (!ent || !hdr || !mbr) { kfree(ent); kfree(hdr); kfree(mbr); return false; }
    for (int i = 0; i < n; i++) fill_entry(ent + i * 128, &parts[i]);
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
    return ok;
}

/* uma ESP ocupando o disco todo a partir de 1 MiB, terminando alinhada a 1 MiB */
bool gpt_create_single_esp(blkdev_t *disk, const char *name, u64 *part_off, u64 *part_size) {
    u64 nsec = disk->size / 512;
    if (nsec < 2048 + 34 + 70000) return false;
    u64 first = 2048, last = (nsec - 34 + 1) / 2048 * 2048 - 1;
    if (last < first) last = nsec - 34;
    gpt_new_t p = {first, last, GPT_TYPE_ESP, name};
    if (!gpt_create(disk, &p, 1)) return false;
    *part_off = first * 512;
    *part_size = (last - first + 1) * 512;
    return true;
}

static bool hdr_valid(const u8 *h) {
    if (memcmp(h, "EFI PART", 8)) return false;
    u32 sz = le32(h + 12);
    if (sz < 92 || sz > 512) return false;
    u8 tmp[512];
    memcpy(tmp, h, sz);
    memset(tmp + 16, 0, 4);
    return crc32_calc(tmp, sz) == le32(h + 16);
}

/* Acrescenta uma partição [first, last] numa entrada vazia. Recusa (sem gravar nada) se a GPT não for válida dos dois
 * lados, se a faixa sair dos limites usáveis ou cruzar uma partição existente. *number = número da nova (1..n). */
bool gpt_add_partition(blkdev_t *disk, const gpt_new_t *np, int *number) {
    const u64 ss = 512;
    u64 nsec = disk->size / ss;
    u8 *h = kmalloc(ss), *bh = kmalloc(ss);
    u8 *ent = NULL;
    bool ok = false;
    if (!h || !bh || !blk_read(disk, ss, h, ss) || !hdr_valid(h)) goto out;
    u64 alt = le64(h + 32), fu = le64(h + 40), lu = le64(h + 48), elba = le64(h + 72);
    u32 ne = le32(h + 80), esz = le32(h + 84), hsz = le32(h + 12);
    if (alt != nsec - 1 || esz < 128 || esz > 512 || ne == 0 || ne > 128 || np->first < fu || np->last > lu || np->first > np->last) goto out;
    if (!blk_read(disk, alt * ss, bh, ss) || !hdr_valid(bh) || le32(bh + 88) != le32(h + 88)) goto out;
    u64 belba = le64(bh + 72), elen = (u64)ne * esz;
    ent = kmalloc(elen);
    if (!ent || !blk_read(disk, elba * ss, ent, elen) || crc32_calc(ent, elen) != le32(h + 88)) goto out;
    static const u8 zero[16];
    int slot = -1;
    for (u32 i = 0; i < ne; i++) {
        u8 *x = ent + (u64)i * esz;
        if (!memcmp(x, zero, 16)) { if (slot < 0) slot = (int)i; continue; }
        if (np->first <= le64(x + 40) && le64(x + 32) <= np->last) goto out;    /* cruza uma partição existente */
    }
    if (slot < 0) goto out;
    u8 *x = ent + (u64)slot * esz;
    memset(x, 0, esz);
    fill_entry(x, np);                              /* os 128 primeiros bytes; o resto da entrada fica zerado */
    u32 ecrc = crc32_calc(ent, elen);
    /* backup primeiro (entradas + cabeçalho), depois o primário: uma queda no meio deixa ao menos um lado consistente */
    put32(bh + 88, ecrc); put32(bh + 16, 0); put32(bh + 16, crc32_calc(bh, le32(bh + 12) <= 512 ? le32(bh + 12) : 92));
    put32(h + 88, ecrc);  put32(h + 16, 0);  put32(h + 16, crc32_calc(h, hsz));
    ok = blk_write(disk, belba * ss, ent, elen) && blk_write(disk, alt * ss, bh, ss) && blk_flush(disk) &&
         blk_write(disk, elba * ss, ent, elen) && blk_write(disk, ss, h, ss) && blk_flush(disk);
    if (ok && number) *number = slot + 1;
out:
    kfree(h); kfree(bh); kfree(ent);
    return ok;
}
