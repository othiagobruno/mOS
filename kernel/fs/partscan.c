/* mOS — leitura e validação de tabelas de partição (GPT/MBR) e identificação do conteúdo das partições.
 * Estritamente somente leitura. Qualquer inconsistência vira PT_BAD: quem decide gravar deve tratar PT_BAD como
 * "não mexer neste disco" (nunca como "apagar e recomeçar"). */
#include "partscan.h"
#include "storage.h"
#include "crypto.h"

static const u8 ESP_TYPE[16] = {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B};
static const u8 ZERO16[16];

bool guid_is_esp(const u8 *g) { return !memcmp(g, ESP_TYPE, 16); }

void guid_to_str(const u8 *g, char *o) {
    snprintf(o, 37, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", le32(g), le16(g + 4), le16(g + 6), g[8], g[9],
             g[10], g[11], g[12], g[13], g[14], g[15]);
}

const char *part_fs_name(int f) {
    switch (f) {
    case FSK_EMPTY: return "vazia";
    case FSK_FAT: return "FAT";
    case FSK_NTFS: return "NTFS";
    case FSK_EXFAT: return "exFAT";
    case FSK_EXT: return "ext2/3/4";
    case FSK_HFS: return "HFS+";
    case FSK_APFS: return "APFS";
    case FSK_BITLOCKER: return "BitLocker";
    case FSK_LUKS: return "LUKS";
    case FSK_REFS: return "ReFS";
    case FSK_SWAP: return "swap";
    }
    return "desconhecido";
}

const char *part_type_name(const part_t *p) {
    static const struct { u8 g[16]; const char *n; } T[] = {
        {{0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7}, "Dados básicos"},
        {{0xAF, 0x3D, 0xC6, 0x0F, 0x83, 0x84, 0x72, 0x47, 0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4}, "Linux"},
        {{0x16, 0xE3, 0xC9, 0xE3, 0x5C, 0x0B, 0xB8, 0x4D, 0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE}, "Reservada Microsoft"},
        {{0xAA, 0x11, 0xAA, 0x11, 0x00, 0x30, 0x11, 0xAA, 0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}, "APFS"},
        {{0x00, 0x53, 0x46, 0x48, 0x00, 0x00, 0xAA, 0xAA, 0x11, 0xAA, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}, "HFS+"},
    };
    if (p->esp) return "Sistema EFI (ESP)";
    for (u32 i = 0; i < ARRAY_LEN(T); i++) if (!memcmp(p->type_guid, T[i].g, 16)) return T[i].n;
    return "Outro";
}

/* ------------------------------------------------------------------ conteúdo de uma partição */
int part_sniff(blkdev_t *disk, u64 off, u64 size) {
    u8 *b = kmalloc(4096);
    if (!b) return FSK_UNKNOWN;
    int r = FSK_UNKNOWN;
    if (size < 4096 || !blk_read(disk, off, b, 4096)) goto out;
    if (b[510] == 0x55 && b[511] == 0xAA && (!memcmp(b + 82, "FAT32   ", 8) || !memcmp(b + 54, "FAT16   ", 8) || !memcmp(b + 54, "FAT12   ", 8))) r = FSK_FAT;
    else if (!memcmp(b + 3, "NTFS    ", 8)) r = FSK_NTFS;
    else if (!memcmp(b + 3, "EXFAT   ", 8)) r = FSK_EXFAT;
    else if (!memcmp(b + 3, "-FVE-FS-", 8)) r = FSK_BITLOCKER;
    else if (!memcmp(b + 3, "ReFS", 4)) r = FSK_REFS;
    else if (!memcmp(b, "LUKS\xba\xbe", 6)) r = FSK_LUKS;
    else if (!memcmp(b + 32, "NXSB", 4)) r = FSK_APFS;
    else if (!memcmp(b + 1024, "H+", 2) || !memcmp(b + 1024, "HX", 2)) r = FSK_HFS;
    else if (le16(b + 1024 + 56) == 0xEF53) r = FSK_EXT;
    else if (!memcmp(b + 4086, "SWAPSPACE2", 10)) r = FSK_SWAP;
    if (r == FSK_UNKNOWN) {
        /* vazia = o primeiro MiB inteiro é zero */
        bool zero = true;
        u64 lim = MIN(size, 1ull << 20);
        for (u64 o = 0; o < lim && zero; o += 4096) {
            if (!blk_read(disk, off + o, b, 4096)) { zero = false; break; }
            for (int i = 0; i < 4096; i++) if (b[i]) { zero = false; break; }
        }
        if (zero) r = FSK_EMPTY;
    }
out:
    kfree(b);
    return r;
}

/* ------------------------------------------------------------------ GPT */
static void bad(ptable_t *t, const char *m) { t->kind = PT_BAD; snprintf(t->err, sizeof t->err, "%s", m); }

static bool hdr_ok(const u8 *h, u32 *hsz) {
    if (memcmp(h, "EFI PART", 8) || le32(h + 8) != 0x00010000) return false;
    u32 sz = le32(h + 12);
    if (sz < 92 || sz > 512) return false;
    u8 tmp[512];
    memcpy(tmp, h, sz);
    memset(tmp + 16, 0, 4);
    if (crc32_calc(tmp, sz) != le32(h + 16)) return false;
    *hsz = sz;
    return true;
}

static void utf16_ascii(const u8 *p, int units, char *out, int cap) {
    int k = 0;
    for (int i = 0; i < units && k < cap - 1; i++) {
        u16 c = le16(p + i * 2);
        if (!c) break;
        out[k++] = c < 128 && c >= 32 ? (char)c : '?';
    }
    out[k] = 0;
}

static bool scan_gpt(blkdev_t *disk, ptable_t *t, const u8 *s0, const u8 *h1) {
    u32 hsz;
    t->kind = PT_GPT;
    if (!hdr_ok(h1, &hsz)) { bad(t, "Cabeçalho GPT primário inválido (assinatura, versão ou CRC)."); return true; }
    u64 nsec = t->total_sectors;
    u64 my = le64(h1 + 24), alt = le64(h1 + 32), fu = le64(h1 + 40), lu = le64(h1 + 48), elba = le64(h1 + 72);
    u32 ne = le32(h1 + 80), esz = le32(h1 + 84), ecrc = le32(h1 + 88);
    if (my != 1) { bad(t, "GPT: MyLBA do cabeçalho primário diferente de 1."); return true; }
    if (esz < 128 || esz > 512 || (esz & 7)) { bad(t, "GPT: tamanho de entrada de partição não suportado."); return true; }
    if (ne == 0 || ne > PT_MAX_PARTS) { bad(t, "GPT: número de entradas não suportado (máximo 128)."); return true; }
    if (fu < 2 || lu >= nsec || fu > lu || alt >= nsec || alt <= lu) { bad(t, "GPT: limites usáveis inconsistentes com o tamanho do disco."); return true; }
    u64 elen = (u64)ne * esz;
    if (elba < 2 || elba + (elen + 511) / 512 > fu) { bad(t, "GPT: tabela de entradas fora da área reservada."); return true; }
    u8 *e = kmalloc(elen + 512);
    if (!e) return false;
    if (!blk_read(disk, elba * 512, e, elen)) { kfree(e); return false; }
    if (crc32_calc(e, elen) != ecrc) { kfree(e); bad(t, "GPT: CRC da tabela de entradas não confere."); return true; }
    memcpy(t->disk_guid, h1 + 56, 16);
    t->first_usable = fu; t->last_usable = lu;
    int n = 0;
    for (u32 i = 0; i < ne; i++) {
        const u8 *x = e + (u64)i * esz;
        if (!memcmp(x, ZERO16, 16)) continue;
        u64 f = le64(x + 32), l = le64(x + 40);
        if (f > l || f < fu || l > lu) { kfree(e); bad(t, "GPT: partição fora dos limites usáveis do disco."); return true; }
        part_t *p = &t->p[n++];
        memset(p, 0, sizeof *p);
        p->number = (int)i + 1;
        p->first = f; p->last = l;
        memcpy(p->type_guid, x, 16);
        memcpy(p->uniq, x + 16, 16);
        p->esp = guid_is_esp(x);
        utf16_ascii(x + 56, (int)((esz - 56) / 2), p->name, sizeof p->name);
    }
    kfree(e);
    t->n = n;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (t->p[i].first <= t->p[j].last && t->p[j].first <= t->p[i].last) { bad(t, "GPT: partições sobrepostas."); return true; }
    /* cabeçalho de backup: precisa existir no fim do disco, válido e com as mesmas entradas */
    u8 bh[512];
    t->backup_ok = false;
    if (alt == nsec - 1 && blk_read(disk, alt * 512, bh, 512) && hdr_ok(bh, &hsz) && le64(bh + 24) == alt && le64(bh + 32) == 1 &&
        le32(bh + 88) == ecrc && le32(bh + 80) == ne && le32(bh + 84) == esz && !memcmp(bh + 56, h1 + 56, 16))
        t->backup_ok = true;
    /* resumo dos metadados, para revalidar antes de gravar */
    sha256_ctx c;
    sha256_init(&c);
    sha256_update(&c, s0, 512);
    sha256_update(&c, h1, 512);
    u8 *e2 = kmalloc(elen + 512);
    if (e2 && blk_read(disk, elba * 512, e2, elen)) sha256_update(&c, e2, elen);
    kfree(e2);
    if (blk_read(disk, alt * 512, bh, 512)) sha256_update(&c, bh, 512);
    sha256_final(&c, t->snap);
    return true;
}

/* ------------------------------------------------------------------ MBR */
static bool scan_mbr(blkdev_t *disk, ptable_t *t, const u8 *s0) {
    t->kind = PT_MBR;
    t->mbr_sig = le32(s0 + 440);
    int n = 0;
    for (int i = 0; i < 4; i++) {
        const u8 *x = s0 + 446 + i * 16;
        u8 st = x[0], ty = x[4];
        u64 f = le32(x + 8), cnt = le32(x + 12);
        if (st != 0x00 && st != 0x80) { bad(t, "MBR: byte de estado de partição inválido."); return true; }
        if (ty == 0) continue;
        if (ty == 0xEE) { bad(t, "MBR protetor (0xEE) sem uma tabela GPT válida."); return true; }
        if (!cnt || f + cnt > t->total_sectors) { bad(t, "MBR: partição fora dos limites do disco."); return true; }
        if (ty == 0x05 || ty == 0x0F || ty == 0x85) t->has_extended = true;
        part_t *p = &t->p[n++];
        memset(p, 0, sizeof *p);
        p->number = i + 1;
        p->first = f; p->last = f + cnt - 1;
        p->mbr_type = ty;
        p->esp = ty == 0xEF;
        snprintf(p->name, sizeof p->name, "tipo 0x%02X", ty);
    }
    t->n = n;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (t->p[i].first <= t->p[j].last && t->p[j].first <= t->p[i].last) { bad(t, "MBR: partições sobrepostas."); return true; }
    sha256(s0, 512, t->snap);
    return true;
}

bool partscan(blkdev_t *disk, ptable_t *t) {
    memset(t, 0, sizeof *t);
    t->total_sectors = disk->size / 512;
    u8 *b = kmalloc(1024);
    if (!b) return false;
    bool ok = true;
    if (t->total_sectors < 2 || !blk_read(disk, 0, b, 1024)) { kfree(b); return false; }
    if (!memcmp(b + 512, "EFI PART", 8)) ok = scan_gpt(disk, t, b, b + 512);
    else if (b[510] == 0x55 && b[511] == 0xAA &&
             !(b[0] == 0xEB || b[0] == 0xE9 ? (!memcmp(b + 82, "FAT32   ", 8) || !memcmp(b + 54, "FAT16   ", 8) || !memcmp(b + 54, "FAT12   ", 8) || !memcmp(b + 3, "NTFS", 4)) : false))
        ok = scan_mbr(disk, t, b);
    else {
        t->kind = PT_NONE;
        sha256(b, 512, t->snap);
        int f = part_sniff(disk, 0, disk->size);
        t->whole_fs = f;
        t->whole_volume = f != FSK_EMPTY && f != FSK_UNKNOWN;
    }
    kfree(b);
    if (!ok) return false;
    if (t->kind == PT_GPT || t->kind == PT_MBR) {
        for (int i = 0; i < t->n; i++) {
            part_t *p = &t->p[i];
            p->fs = part_sniff(disk, p->first * 512, (p->last - p->first + 1) * 512);
        }
    }
    return true;
}

/* ------------------------------------------------------------------ espaço livre */
int part_free_gaps(const ptable_t *t, gap_t *out, int max, u64 min_sectors) {
    if (t->kind != PT_GPT || !t->last_usable) return 0;
    int ord[PT_MAX_PARTS], n = t->n, k = 0;           /* partições em ordem de início */
    for (int i = 0; i < n; i++) ord[i] = i;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && t->p[ord[j]].first < t->p[ord[j - 1]].first; j--) { int x = ord[j]; ord[j] = ord[j - 1]; ord[j - 1] = x; }
    u64 cur = t->first_usable;
    for (int i = 0; i <= n && k < max; i++) {
        u64 end = i < n ? t->p[ord[i]].first : t->last_usable + 1;     /* exclusivo */
        u64 f = (cur + 2047) / 2048 * 2048, l = end / 2048 * 2048;     /* [f, l) alinhado a 1 MiB */
        if (l > f && l - f >= 2048 && l - f >= min_sectors) { out[k].first = f; out[k].last = l - 1; k++; }
        if (i < n && t->p[ord[i]].last + 1 > cur) cur = t->p[ord[i]].last + 1;
    }
    return k;
}
