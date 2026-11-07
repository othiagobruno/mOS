/*
 * mOS — imagens de disco de teste para volumes graváveis (FAT e HFS+) e verificação no host.
 * Implementação própria, independente dos drivers do kernel (kernel/fs/fat.c, hfsplus*.c), para
 * conferir o que o mOS gravou sem depender do mesmo código (não há fsck_hfs/fsck.fat no Windows).
 *
 *   volimg mkdisk <img> <MB> <spec>...      disco GPT; spec = fat12|fat16|fat32|hfs|hfsx|hfsj[:ROTULO[:MB]]
 *                                           (hfsj = HFS+ com journal; cada volume ganha /seed.txt)
 *   volimg check  <img>                     verifica todos os volumes (estilo fsck); código 1 se houver erro
 *   volimg ls     <img> [n]                 lista recursiva do volume n (1 = primeiro)
 *   volimg cat    <img> <n> <caminho>       conteúdo de um arquivo (saída padrão)
 *   volimg jinject <img> <n> <caminho> <texto>
 *                                           HFS+ com journal: deixa no journal uma transação pendente que
 *                                           regrava o 1º bloco do arquivo com <texto> (simula queda)
 *
 * Compilar: zig cc -O2 -target x86_64-windows-gnu tools/volimg.c -o build/volimg.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

static u8 *disk;
static u64 disk_size;
static int errors, warnings;

#define ERR(...) do { errors++; printf("  ERRO: "); printf(__VA_ARGS__); printf("\n"); } while (0)
#define WARN(...) do { warnings++; printf("  aviso: "); printf(__VA_ARGS__); printf("\n"); } while (0)

static u16 rb16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }
static u32 rb32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u64 rb64(const u8 *p) { return (u64)rb32(p) << 32 | rb32(p + 4); }
static u16 rl16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u32 rl32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u64 rl64(const u8 *p) { return rl32(p) | (u64)rl32(p + 4) << 32; }
static void wb16(u8 *p, u16 v) { p[0] = v >> 8; p[1] = (u8)v; }
static void wb32(u8 *p, u32 v) { p[0] = v >> 24; p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }
static void wb64(u8 *p, u64 v) { wb32(p, (u32)(v >> 32)); wb32(p + 4, (u32)v); }
static void wl16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = v >> 8; }
static void wl32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static void wl64(u8 *p, u64 v) { wl32(p, (u32)v); wl32(p + 4, (u32)(v >> 32)); }

static bool load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return false; }
    fseek(f, 0, SEEK_END);
    disk_size = (u64)_ftelli64(f);
    fseek(f, 0, SEEK_SET);
    disk = malloc(disk_size);
    bool ok = disk && fread(disk, 1, disk_size, f) == disk_size;
    fclose(f);
    return ok;
}
static bool save(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return false; }
    bool ok = fwrite(disk, 1, disk_size, f) == disk_size;
    fclose(f);
    return ok;
}

/* ================================================================ partições (GPT ou volume direto) */
typedef struct { u64 off, size; } part_t;
static part_t parts[16];
static int nparts;

static void scan_parts(void) {
    nparts = 0;
    if (disk_size > 1024 && !memcmp(disk + 512, "EFI PART", 8)) {
        u64 lba = rl64(disk + 512 + 72);
        u32 cnt = rl32(disk + 512 + 80), esz = rl32(disk + 512 + 84);
        for (u32 i = 0; i < cnt && i < 128 && nparts < 16; i++) {
            const u8 *e = disk + lba * 512 + (u64)i * esz;
            static const u8 z[16];
            if (!memcmp(e, z, 16)) continue;
            u64 a = rl64(e + 32), b = rl64(e + 40);
            parts[nparts++] = (part_t){a * 512, (b - a + 1) * 512};
        }
    } else parts[nparts++] = (part_t){0, disk_size};
}

/* ---------------------------------------------------------------- CRC32 (GPT) */
static u32 crc32(const u8 *p, u64 n) {
    u32 c = 0xFFFFFFFF;
    for (u64 i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = c >> 1 ^ (0xEDB88320 & -(c & 1)); }
    return ~c;
}

static void guid_random(u8 *g) { for (int i = 0; i < 16; i++) g[i] = (u8)rand(); g[7] = (g[7] & 0x0F) | 0x40; g[8] = (g[8] & 0x3F) | 0x80; }

/* tipos: dados básicos da Microsoft (FAT) e Apple HFS+ */
static const u8 GUID_BASIC[16] = {0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7};
static const u8 GUID_HFS[16] = {0x00, 0x53, 0x46, 0x48, 0x00, 0x00, 0xAA, 0x11, 0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC};

static void write_gpt(part_t *p, const bool *hfs, int n) {
    u64 nlba = disk_size / 512;
    u8 *pmbr = disk;
    memset(pmbr + 446, 0, 64);
    pmbr[446 + 4] = 0xEE;
    wl32(pmbr + 446 + 8, 1);
    wl32(pmbr + 446 + 12, nlba - 1 > 0xFFFFFFFF ? 0xFFFFFFFF : (u32)(nlba - 1));
    pmbr[510] = 0x55; pmbr[511] = 0xAA;
    u8 *ents = calloc(128, 128);
    for (int i = 0; i < n; i++) {
        u8 *e = ents + i * 128;
        memcpy(e, hfs[i] ? GUID_HFS : GUID_BASIC, 16);
        guid_random(e + 16);
        wl64(e + 32, p[i].off / 512);
        wl64(e + 40, (p[i].off + p[i].size) / 512 - 1);
        const char *nm = hfs[i] ? "Apple_HFS" : "Dados";
        for (int k = 0; nm[k]; k++) wl16(e + 56 + k * 2, (u16)nm[k]);
    }
    u32 ecrc = crc32(ents, 128 * 128);
    for (int copy = 0; copy < 2; copy++) {
        u64 hlba = copy ? nlba - 1 : 1, elba = copy ? nlba - 33 : 2;
        u8 *h = disk + hlba * 512;
        memset(h, 0, 512);
        memcpy(h, "EFI PART", 8);
        wl32(h + 8, 0x00010000);
        wl32(h + 12, 92);
        wl64(h + 24, hlba);
        wl64(h + 32, copy ? 1 : nlba - 1);
        wl64(h + 40, 34);
        wl64(h + 48, nlba - 34);
        guid_random(h + 56);
        wl64(h + 72, elba);
        wl32(h + 80, 128);
        wl32(h + 84, 128);
        wl32(h + 88, ecrc);
        wl32(h + 16, crc32(h, 92));
        memcpy(disk + elba * 512, ents, 128 * 128);
    }
    free(ents);
}

/* ================================================================ FAT: formatação */
static void mkfat(u8 *v, u64 size, int bits, const char *label) {
    u32 bps = 512, tot = (u32)(size / bps), res = bits == 32 ? 32 : 1, nf = 2, rootents = bits == 32 ? 0 : 512;
    u32 spc = bits == 12 ? 4 : bits == 16 ? (size <= (128u << 20) ? 4 : 16) : (size <= (260u << 20) ? 1 : 8);
    u32 rsec = rootents * 32 / bps, fatsz = 1, nclus = 0;
    for (int it = 0; it < 32; it++) {
        nclus = (tot - res - nf * fatsz - rsec) / spc;
        u64 need = bits == 12 ? ((u64)(nclus + 2) * 3 / 2 + 1) : (u64)(nclus + 2) * (bits / 8);
        u32 f = (u32)((need + bps - 1) / bps);
        if (f == fatsz) break;
        fatsz = f;
    }
    memset(v, 0, (u64)(res + nf * fatsz + rsec + spc) * bps);
    v[0] = 0xEB; v[1] = bits == 32 ? 0x58 : 0x3C; v[2] = 0x90;
    memcpy(v + 3, "MSWIN4.1", 8);
    wl16(v + 11, (u16)bps); v[13] = (u8)spc; wl16(v + 14, (u16)res); v[16] = (u8)nf; wl16(v + 17, (u16)rootents);
    if (tot < 65536 && bits != 32) wl16(v + 19, (u16)tot); else wl32(v + 32, tot);
    v[21] = 0xF8; wl16(v + 24, 63); wl16(v + 26, 255);
    char lab[11];
    memset(lab, ' ', 11);
    for (int i = 0; label[i] && i < 11; i++) lab[i] = (char)toupper((u8)label[i]);
    u32 id = (u32)rand() << 16 ^ (u32)rand();
    if (bits == 32) {
        wl32(v + 36, fatsz); wl32(v + 44, 2); wl16(v + 48, 1); wl16(v + 50, 6);
        v[64] = 0x80; v[66] = 0x29; wl32(v + 67, id); memcpy(v + 71, lab, 11); memcpy(v + 82, "FAT32   ", 8);
        u8 *fi = v + bps;
        wl32(fi, 0x41615252); wl32(fi + 484, 0x61417272); wl32(fi + 488, nclus - 1); wl32(fi + 492, 3);
        fi[510] = 0x55; fi[511] = 0xAA;
        memcpy(v + 6 * bps, v, bps);
        memcpy(v + 7 * bps, fi, bps);
    } else {
        wl16(v + 22, (u16)fatsz);
        v[36] = 0x80; v[38] = 0x29; wl32(v + 39, id); memcpy(v + 43, lab, 11);
        memcpy(v + 54, bits == 12 ? "FAT12   " : "FAT16   ", 8);
    }
    v[510] = 0x55; v[511] = 0xAA;
    for (u32 f = 0; f < nf; f++) {
        u8 *fat = v + (u64)(res + f * fatsz) * bps;
        if (bits == 32) { wl32(fat, 0x0FFFFFF8); wl32(fat + 4, 0x0FFFFFFF); wl32(fat + 8, 0x0FFFFFFF); }
        else if (bits == 16) { wl16(fat, 0xFFF8); wl16(fat + 2, 0xFFFF); }
        else { fat[0] = 0xF8; fat[1] = 0xFF; fat[2] = 0xFF; }
    }
    u8 *root = v + (u64)(res + nf * fatsz) * bps;
    memcpy(root, lab, 11);
    root[11] = 0x08;
    /* /seed.txt (cluster 3 no FAT32, 2 no FAT12/16) */
    const char *seed = "semente do volimg\n";
    u32 sc = bits == 32 ? 3 : 2;
    u8 *e = root + 32;
    memcpy(e, "SEED    TXT", 11);
    e[11] = 0x20;
    e[12] = 0x18;                                       /* nome e extensão em minúsculas (NT) */
    wl16(e + 26, (u16)sc); wl16(e + 20, (u16)(sc >> 16)); wl32(e + 28, (u32)strlen(seed));
    wl16(e + 24, (u16)((2024 - 1980) << 9 | 1 << 5 | 1));
    u64 data_off = (u64)(res + nf * fatsz + rsec) * bps;
    memcpy(v + data_off + (u64)(sc - 2) * spc * bps, seed, strlen(seed));
    for (u32 f = 0; f < nf; f++) {
        u8 *fat = v + (u64)(res + f * fatsz) * bps;
        if (bits == 32) wl32(fat + sc * 4, 0x0FFFFFFF);
        else if (bits == 16) wl16(fat + sc * 2, 0xFFFF);
        else { u32 o = sc + sc / 2; u16 w = rl16(fat + o); w = sc & 1 ? (w & 0xF) | 0xFFF0 : (w & 0xF000) | 0xFFF; wl16(fat + o, w); }
    }
    if (bits == 32) { wl32(v + bps + 488, nclus - 2); wl32(v + 6 * bps + bps + 488, nclus - 2); }
    printf("FAT%d \"%s\": %u clusters de %u bytes\n", bits, label, nclus, spc * bps);
}

/* ================================================================ FAT: leitura e verificação */
typedef struct {
    u8 *v; u64 size;
    int bits;
    u32 bps, spc, clus, res, nf, fatsz, rootents, nclus, root;
    u64 fat_off, root_off, data_off;
} fat_t;

static bool fat_open(fat_t *f, u8 *v, u64 size) {
    memset(f, 0, sizeof *f);
    f->v = v; f->size = size;
    if (v[510] != 0x55 || v[511] != 0xAA || (v[0] != 0xEB && v[0] != 0xE9)) return false;
    f->bps = rl16(v + 11); f->spc = v[13]; f->res = rl16(v + 14); f->nf = v[16]; f->rootents = rl16(v + 17);
    if (!f->bps || !f->spc || !f->nf) return false;
    u32 tot = rl16(v + 19) ? rl16(v + 19) : rl32(v + 32);
    u32 fs16 = rl16(v + 22);
    f->fatsz = fs16 ? fs16 : rl32(v + 36);
    u32 rsec = (f->rootents * 32 + f->bps - 1) / f->bps;
    f->nclus = (tot - f->res - f->nf * f->fatsz - rsec) / f->spc;
    f->bits = (!fs16 && !f->rootents) ? 32 : f->nclus < 4085 ? 12 : 16;
    f->clus = f->bps * f->spc;
    f->fat_off = (u64)f->res * f->bps;
    f->root_off = f->fat_off + (u64)f->nf * f->fatsz * f->bps;
    f->data_off = f->root_off + (u64)rsec * f->bps;
    f->root = f->bits == 32 ? rl32(v + 44) : 0;
    return (u64)tot * f->bps <= size;
}

static u32 fat_ent(fat_t *f, int copy, u32 c) {
    const u8 *t = f->v + f->fat_off + (u64)copy * f->fatsz * f->bps;
    if (f->bits == 32) return rl32(t + c * 4) & 0x0FFFFFFF;
    if (f->bits == 16) { u32 x = rl16(t + c * 2); return x >= 0xFFF8 ? 0x0FFFFFFF : x == 0xFFF7 ? 0x0FFFFFF7 : x; }
    u32 w = rl16(t + c + c / 2), x = c & 1 ? w >> 4 : w & 0xFFF;
    return x >= 0xFF8 ? 0x0FFFFFFF : x == 0xFF7 ? 0x0FFFFFF7 : x;
}
static bool fat_eoc(u32 x) { return x >= 0x0FFFFFF8; }
static u8 *fat_cl(fat_t *f, u32 c) { return f->v + f->data_off + (u64)(c - 2) * f->clus; }

typedef struct { char name[1024]; u8 attr; u32 clus, size; } fent_t;

static void utf16_to_utf8(const u16 *w, int n, char *o, int cap) {
    int k = 0;
    for (int i = 0; i < n && w[i] && k < cap - 5; i++) {
        u32 c = w[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < n) { c = 0x10000 + ((c - 0xD800) << 10) + (w[i + 1] - 0xDC00); i++; }
        if (c < 0x80) o[k++] = (char)c;
        else if (c < 0x800) { o[k++] = (char)(0xC0 | c >> 6); o[k++] = (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { o[k++] = (char)(0xE0 | c >> 12); o[k++] = (char)(0x80 | (c >> 6 & 63)); o[k++] = (char)(0x80 | (c & 63)); }
        else { o[k++] = (char)(0xF0 | c >> 18); o[k++] = (char)(0x80 | (c >> 12 & 63)); o[k++] = (char)(0x80 | (c >> 6 & 63)); o[k++] = (char)(0x80 | (c & 63)); }
    }
    o[k] = 0;
}

static u8 lfn_sum(const u8 *n) { u8 s = 0; for (int i = 0; i < 11; i++) s = (u8)(((s & 1) << 7) + (s >> 1) + n[i]); return s; }

/* lê um diretório inteiro (cadeia ou raiz fixa) para buffer */
static u8 *fat_dir(fat_t *f, u32 c, u32 *len) {
    if (c == 0 && f->bits != 32) { *len = f->rootents * 32; u8 *b = malloc(*len); memcpy(b, f->v + f->root_off, *len); return b; }
    u32 n = 0, x = c;
    while (x >= 2 && x <= f->nclus + 1 && n <= f->nclus) { n++; x = fat_ent(f, 0, x); if (fat_eoc(x)) break; }
    *len = n * f->clus;
    u8 *b = malloc(*len ? *len : 1);
    x = c;
    for (u32 i = 0; i < n; i++) { memcpy(b + (u64)i * f->clus, fat_cl(f, x), f->clus); x = fat_ent(f, 0, x); }
    return b;
}

/* percorre entradas; cb devolve false para parar. Confere LFN no caminho (check=true) */
typedef bool (*fat_cb)(fat_t *f, fent_t *e, void *ctx);
static void fat_entries(fat_t *f, u32 dirclus, bool check, const char *where, fat_cb cb, void *ctx) {
    u32 len;
    u8 *b = fat_dir(f, dirclus, &len);
    u16 lfn[260];
    int lcnt = 0, lseen = 0;
    u8 lsum = 0;
    bool inl = false;
    for (u32 i = 0; i < len / 32; i++) {
        u8 *e = b + i * 32;
        if (e[0] == 0) break;
        if (e[0] == 0xE5) { if (inl && check) ERR("%s: LFN órfão (entrada apagada no meio)", where); inl = false; continue; }
        if (e[11] == 0x0F) {
            int seq = e[0] & 0x1F;
            if (e[0] & 0x40) {
                if (inl && check) ERR("%s: LFN interrompido", where);
                inl = true; lcnt = seq; lseen = 0; lsum = e[13]; memset(lfn, 0, sizeof lfn);
            } else if (!inl || seq != lcnt - lseen || e[13] != lsum) { if (check) ERR("%s: sequência LFN inválida", where); inl = false; continue; }
            static const int P[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
            if (seq >= 1 && seq <= 20) for (int k = 0; k < 13; k++) lfn[(seq - 1) * 13 + k] = rl16(e + P[k]);
            lseen++;
            continue;
        }
        if (e[11] & 0x08) { inl = false; continue; }
        fent_t fe = {0};
        fe.attr = e[11];
        fe.clus = (u32)rl16(e + 20) << 16 | rl16(e + 26);
        if (f->bits != 32) fe.clus &= 0xFFFF;
        fe.size = rl32(e + 28);
        if (inl) {
            if (lseen != lcnt && check) ERR("%s: LFN incompleto", where);
            if (lfn_sum(e) != lsum && check) ERR("%s: checksum do LFN não confere com o nome 8.3", where);
            for (int k = 0; k < lcnt * 13; k++) if (lfn[k] == 0xFFFF) lfn[k] = 0;
            utf16_to_utf8(lfn, lcnt * 13, fe.name, sizeof fe.name);
        } else {
            int o = 0;
            for (int k = 0; k < 8 && e[k] != ' '; k++) fe.name[o++] = (char)((e[12] & 8) ? tolower(e[k]) : e[k]);
            if (e[8] != ' ') { fe.name[o++] = '.'; for (int k = 8; k < 11 && e[k] != ' '; k++) fe.name[o++] = (char)((e[12] & 0x10) ? tolower(e[k]) : e[k]); }
            fe.name[o] = 0;
            if ((u8)fe.name[0] == 0x05) fe.name[0] = (char)0xE5;
        }
        inl = false;
        if (!cb(f, &fe, ctx)) break;
    }
    free(b);
}

typedef struct { fat_t *f; u32 *owner; char path[2048]; int depth; u32 files, dirs; } fchk_t;

static bool fat_chain_check(fchk_t *c, u32 start, u32 id, const char *path, u32 *len) {
    fat_t *f = c->f;
    u32 n = 0, x = start;
    *len = 0;
    while (1) {
        if (x < 2 || x > f->nclus + 1) { ERR("%s: cluster %u fora do volume", path, x); return false; }
        if (c->owner[x]) { ERR("%s: cluster %u cruzado (já usado por outro item)", path, x); return false; }
        c->owner[x] = id;
        n++;
        u32 nx = fat_ent(f, 0, x);
        if (nx == 0) { ERR("%s: cadeia aponta para cluster livre", path); return false; }
        if (nx == 0x0FFFFFF7) { ERR("%s: cluster ruim na cadeia", path); return false; }
        if (fat_eoc(nx)) break;
        x = nx;
    }
    *len = n;
    return true;
}

static void fat_walk(fchk_t *c, u32 dir, u32 parent);
static bool fat_chk_cb(fat_t *f, fent_t *e, void *ctx) {
    fchk_t *c = ctx;
    if (!strcmp(e->name, ".") || !strcmp(e->name, "..")) return true;
    size_t pl = strlen(c->path);
    snprintf(c->path + pl, sizeof c->path - pl, "/%s", e->name);
    if (e->attr & 0x10) {
        c->dirs++;
        u32 n;
        if (!e->clus) ERR("%s: pasta sem cluster", c->path);
        else if (fat_chain_check(c, e->clus, e->clus, c->path, &n)) {
            /* "." e ".." */
            u8 *d = fat_cl(f, e->clus);
            u32 dot = (u32)rl16(d + 20) << 16 | rl16(d + 26), dd = (u32)rl16(d + 52) << 16 | rl16(d + 58);
            if (memcmp(d, ".          ", 11) || dot != e->clus) ERR("%s: entrada \".\" inválida", c->path);
            if (memcmp(d + 32, "..         ", 11)) ERR("%s: entrada \"..\" ausente", c->path);
            (void)dd;                                    /* o valor de ".." é conferido em dd_cb */
            if (c->depth < 32) { c->depth++; fat_walk(c, e->clus, 0); c->depth--; }
        }
    } else {
        c->files++;
        u32 n = 0;
        if (e->size && !e->clus) ERR("%s: %u bytes sem cluster", c->path, e->size);
        if (e->clus && fat_chain_check(c, e->clus, e->clus, c->path, &n)) {
            u32 need = (e->size + f->clus - 1) / f->clus;
            if (n != need) ERR("%s: cadeia com %u clusters, o tamanho (%u) pede %u", c->path, n, e->size, need);
        }
    }
    c->path[pl] = 0;
    return true;
}

static void fat_walk(fchk_t *c, u32 dir, u32 parent) {
    (void)parent;
    /* confere ".." do diretório: cluster do pai (0 = raiz) */
    fat_entries(c->f, dir, true, c->path[0] ? c->path : "/", fat_chk_cb, c);
}

/* confere ".." de cada subpasta: segunda passada simples */
typedef struct { fat_t *f; u32 self; char path[2048]; int depth; } dd_t;
static bool dd_cb(fat_t *f, fent_t *e, void *ctx) {
    dd_t *d = ctx;
    if (!(e->attr & 0x10) || !strcmp(e->name, ".") || !strcmp(e->name, "..") || !e->clus || e->clus > f->nclus + 1) return true;
    u8 *b = fat_cl(f, e->clus);
    u32 dd = (u32)rl16(b + 52) << 16 | rl16(b + 58);
    u32 exp = d->self == f->root ? 0 : d->self;
    if (!memcmp(b + 32, "..         ", 11) && dd != exp) ERR("%s/%s: \"..\" aponta para %u, devia ser %u", d->path, e->name, dd, exp);
    if (d->depth < 32) {
        dd_t s = *d;
        s.self = e->clus;
        s.depth++;
        snprintf(s.path, sizeof s.path, "%s/%s", d->path, e->name);
        fat_entries(f, e->clus, false, s.path, dd_cb, &s);
    }
    return true;
}

static void fat_check(u8 *v, u64 size) {
    fat_t f;
    if (!fat_open(&f, v, size)) { ERR("BPB FAT inválido"); return; }
    printf("FAT%d: %u clusters de %u bytes\n", f.bits, f.nclus, f.clus);
    for (u32 cp = 1; cp < f.nf; cp++)
        if (memcmp(v + f.fat_off, v + f.fat_off + (u64)cp * f.fatsz * f.bps, (u64)f.fatsz * f.bps)) ERR("cópia %u da FAT difere da 1ª", cp + 1);
    fchk_t *c = calloc(1, sizeof *c);
    c->f = &f;
    c->owner = calloc(f.nclus + 2, sizeof(u32));
    if (f.bits == 32) { u32 n; fat_chain_check(c, f.root, f.root, "/", &n); }
    fat_walk(c, f.bits == 32 ? f.root : 0, 0);
    dd_t d = {&f, f.bits == 32 ? f.root : 0, "", 0};
    fat_entries(&f, d.self, false, "/", dd_cb, &d);
    u32 lost = 0, freec = 0;
    for (u32 x = 2; x <= f.nclus + 1; x++) {
        u32 e = fat_ent(&f, 0, x);
        if (!e) freec++;
        else if (!c->owner[x] && e != 0x0FFFFFF7) lost++;
    }
    if (lost) ERR("%u cluster(s) perdido(s) (usados na FAT, sem dono)", lost);
    if (f.bits == 32) {
        u32 fs = rl16(v + 48);
        if (fs && fs < f.res && rl32(v + fs * f.bps) == 0x41615252) {
            u32 fc = rl32(v + fs * f.bps + 488);
            if (fc != 0xFFFFFFFF && fc != freec) WARN("FSInfo diz %u livres, a FAT tem %u", fc, freec);
        }
    }
    printf("  %u arquivos, %u pastas, %u clusters livres\n", c->files, c->dirs, freec);
    free(c->owner);
    free(c);
}

/* ---- ls/cat */
typedef struct { const char *want; fent_t found; bool ok; } ffind_t;
static int ci_eq(const char *a, const char *b) {
    for (; *a && *b; a++, b++) if (tolower((u8)*a) != tolower((u8)*b)) return 0;
    return !*a && !*b;
}
static bool ffind_cb(fat_t *f, fent_t *e, void *ctx) { ffind_t *x = ctx; if (ci_eq(e->name, x->want)) { x->found = *e; x->ok = true; return false; } return true; }
static bool ls_cb(fat_t *f, fent_t *e, void *ctx);
typedef struct { char path[2048]; int depth; } fls_t;
static bool ls_cb(fat_t *f, fent_t *e, void *ctx) {
    fls_t *l = ctx;
    if (!strcmp(e->name, ".") || !strcmp(e->name, "..")) return true;
    printf("%s/%s%s", l->path, e->name, e->attr & 0x10 ? "/\n" : "");
    if (!(e->attr & 0x10)) printf("  (%u bytes)\n", e->size);
    else if (l->depth < 16) {
        fls_t s;
        snprintf(s.path, sizeof s.path, "%s/%s", l->path, e->name);
        s.depth = l->depth + 1;
        fat_entries(f, e->clus, false, s.path, ls_cb, &s);
    }
    return true;
}
static bool fat_resolve(fat_t *f, const char *path, fent_t *out) {
    u32 dir = f->bits == 32 ? f->root : 0;
    char part[512];
    const char *p = path;
    fent_t cur = {.attr = 0x10, .clus = dir};
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        int n = 0;
        while (*p && *p != '/') part[n++] = *p++;
        part[n] = 0;
        ffind_t x = {part};
        fat_entries(f, cur.clus, false, "", ffind_cb, &x);
        if (!x.ok) return false;
        cur = x.found;
    }
    *out = cur;
    return true;
}

/* ================================================================ HFS+ */
#define HFS_DELTA 2082844800u
static const struct { u16 pre, base, mark; } DEC[] = {
    {0xC0,'A',0x300},{0xC1,'A',0x301},{0xC2,'A',0x302},{0xC3,'A',0x303},{0xC4,'A',0x308},{0xC5,'A',0x30A},{0xC7,'C',0x327},
    {0xC8,'E',0x300},{0xC9,'E',0x301},{0xCA,'E',0x302},{0xCB,'E',0x308},{0xCC,'I',0x300},{0xCD,'I',0x301},{0xCE,'I',0x302},
    {0xCF,'I',0x308},{0xD1,'N',0x303},{0xD2,'O',0x300},{0xD3,'O',0x301},{0xD4,'O',0x302},{0xD5,'O',0x303},{0xD6,'O',0x308},
    {0xD9,'U',0x300},{0xDA,'U',0x301},{0xDB,'U',0x302},{0xDC,'U',0x308},{0xDD,'Y',0x301},{0xE0,'a',0x300},{0xE1,'a',0x301},
    {0xE2,'a',0x302},{0xE3,'a',0x303},{0xE4,'a',0x308},{0xE5,'a',0x30A},{0xE7,'c',0x327},{0xE8,'e',0x300},{0xE9,'e',0x301},
    {0xEA,'e',0x302},{0xEB,'e',0x308},{0xEC,'i',0x300},{0xED,'i',0x301},{0xEE,'i',0x302},{0xEF,'i',0x308},{0xF1,'n',0x303},
    {0xF2,'o',0x300},{0xF3,'o',0x301},{0xF4,'o',0x302},{0xF5,'o',0x303},{0xF6,'o',0x308},{0xF9,'u',0x300},{0xFA,'u',0x301},
    {0xFB,'u',0x302},{0xFC,'u',0x308},{0xFD,'y',0x301},{0xFF,'y',0x308}};

static int to_u16(const char *s, u16 *o) {
    int n = 0;
    const u8 *p = (const u8 *)s;
    while (*p && n < 250) {
        u32 c = *p++;
        if (c >= 0xE0) { c = (c & 15) << 12 | (u32)(p[0] & 63) << 6 | (p[1] & 63); p += 2; }
        else if (c >= 0xC0) { c = (c & 31) << 6 | (p[0] & 63); p++; }
        if (c == ':') c = '/';
        u16 mark = 0;
        for (size_t i = 0; i < sizeof DEC / sizeof *DEC; i++) if (DEC[i].pre == c) { c = DEC[i].base; mark = DEC[i].mark; }
        o[n++] = (u16)c;
        if (mark) o[n++] = mark;
    }
    return n;
}

/* dobra de caixa (mesma especificação do HFS+, escrita de novo aqui) */
static u16 hfold(u16 c) {
    if (!c) return 0xFFFF;
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c < 0x80) return c;
    if ((c >= 0x200C && c <= 0x200F) || (c >= 0x202A && c <= 0x202E) || (c >= 0x206A && c <= 0x206F) || c == 0xFEFF) return 0;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
    if ((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177)) return c | 1;
    if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) return c & 1 ? c + 1 : c;
    if (c == 0x178) return 0xFF;
    if (c >= 0x391 && c <= 0x3A9 && c != 0x3A2) return c + 32;
    if (c >= 0x410 && c <= 0x42F) return c + 32;
    if (c >= 0x400 && c <= 0x40F) return c + 80;
    if (c >= 0xFF21 && c <= 0xFF3A) return c + 32;
    return c;
}

typedef struct {
    u8 *v; u64 size;
    u32 bs, tb;
    bool hfsx, binary;
    u8 *vh;
} hfs_t;

typedef struct { u32 start, count; } ext_t;

static int hname_cmp(hfs_t *h, const u8 *a, u32 al, const u8 *b, u32 bl) {
    if (h->binary) {
        for (u32 i = 0; i < al && i < bl; i++) { u16 x = rb16(a + 2 * i), y = rb16(b + 2 * i); if (x != y) return x < y ? -1 : 1; }
        return al == bl ? 0 : al < bl ? -1 : 1;
    }
    u32 i = 0, j = 0;
    for (;;) {
        u16 x = 0, y = 0;
        while (i < al) { x = hfold(rb16(a + 2 * i)); i++; if (x) break; }
        if (i >= al && !x) x = 0;
        while (j < bl) { y = hfold(rb16(b + 2 * j)); j++; if (y) break; }
        if (x != y) return x < y ? -1 : 1;
        if (!x) return 0;
    }
}

static int hkey_cmp(hfs_t *h, int tree, const u8 *a, const u8 *b) {
    if (tree == 0) {
        u32 x = rb32(a + 2), y = rb32(b + 2);
        if (x != y) return x < y ? -1 : 1;
        return hname_cmp(h, a + 8, rb16(a + 6), b + 8, rb16(b + 6));
    }
    u32 x = rb32(a + 4), y = rb32(b + 4);
    if (x != y) return x < y ? -1 : 1;
    if (tree == 1) {
        if (a[2] != b[2]) return a[2] < b[2] ? -1 : 1;
    } else {
        u32 na = rb16(a + 12), nb = rb16(b + 12);
        for (u32 i = 0; i < na && i < nb; i++) { u16 p = rb16(a + 14 + 2 * i), q = rb16(b + 14 + 2 * i); if (p != q) return p < q ? -1 : 1; }
        if (na != nb) return na < nb ? -1 : 1;
    }
    x = rb32(a + 8); y = rb32(b + 8);
    return x == y ? 0 : x < y ? -1 : 1;
}

/* fork lido para RAM (só as árvores e o bitmap; extents overflow resolvidos depois) */
typedef struct { u8 *data; u64 size; ext_t ext[512]; int n; } hfork_t;

static void hfork_exts(const u8 *fd, ext_t *e, int *n) {
    *n = 0;
    for (int i = 0; i < 8; i++) { u32 s = rb32(fd + 16 + 8 * i), c = rb32(fd + 20 + 8 * i); if (!c) break; e[(*n)++] = (ext_t){s, c}; }
}

static bool hfork_read(hfs_t *h, hfork_t *f, const u8 *fd) {
    f->size = rb64(fd);
    if (!f->n) hfork_exts(fd, f->ext, &f->n);
    f->data = calloc(1, f->size ? f->size : 1);
    u64 pos = 0;
    for (int i = 0; i < f->n && pos < f->size; i++) {
        u64 b = (u64)f->ext[i].start * h->bs, len = (u64)f->ext[i].count * h->bs;
        if (b + len > h->size) return false;
        u64 k = len < f->size - pos ? len : f->size - pos;
        memcpy(f->data + pos, h->v + b, k);
        pos += k;
    }
    return pos >= f->size;
}

/* ---- verificação de uma B-tree */
typedef struct { const char *name; int tree; hfork_t f; u32 ns, root, depth, leafrecs, first, last, total, freen, maxkey; bool varidx; } bt_t;

static u8 *bnode(bt_t *t, u32 i) { return (u64)(i + 1) * t->ns <= t->f.size ? t->f.data + (u64)i * t->ns : NULL; }
static u16 nrecs(const u8 *n) { return rb16(n + 10); }
static u16 roff(bt_t *t, const u8 *n, int i) { return rb16(n + t->ns - 2 * (i + 1)); }
static u32 kbytes(const u8 *r) { return (2u + rb16(r) + 1) & ~1u; }

/* percurso de leitura: chama cb(chave, dados, tamanho) em todas as folhas em ordem */
typedef void (*leaf_cb)(void *ctx, const u8 *k, const u8 *d, u32 dl);
static void bt_leaves(bt_t *t, leaf_cb cb, void *ctx) {
    u32 x = t->first;
    for (u32 g = 0; x && g < t->total; g++) {
        u8 *n = bnode(t, x);
        if (!n) return;
        for (int i = 0; i < nrecs(n); i++) {
            const u8 *r = n + roff(t, n, i);
            u32 len = roff(t, n, i + 1) - roff(t, n, i), kb = kbytes(r);
            cb(ctx, r, r + kb, len > kb ? len - kb : 0);
        }
        x = rb32(n);
    }
}

static bool quiet;                                         /* ls/cat: sem o resumo das árvores */
static void bt_check(hfs_t *h, bt_t *t, const u8 *fd) {
    if (!rb64(fd)) { t->ns = 0; return; }
    if (!hfork_read(h, &t->f, fd)) { ERR("%s: fork ilegível", t->name); return; }
    const u8 *hn = t->f.data;
    if ((int8_t)hn[8] != 1) { ERR("%s: nó 0 não é cabeçalho", t->name); return; }
    const u8 *hr = hn + 14;
    t->depth = rb16(hr); t->root = rb32(hr + 2); t->leafrecs = rb32(hr + 6); t->first = rb32(hr + 10); t->last = rb32(hr + 14);
    t->ns = rb16(hr + 18); t->maxkey = rb16(hr + 20); t->total = rb32(hr + 22); t->freen = rb32(hr + 26);
    t->varidx = (rb32(hr + 38) & 4) != 0;
    if (t->tree == 0 && hr[37] == 0xBC) h->binary = true;
    if ((u64)t->total * t->ns > t->f.size) { ERR("%s: totalNodes %u não cabe no fork", t->name, t->total); return; }
    u8 *used = calloc(t->total, 1);
    used[0] = 1;
    /* nós de mapa */
    for (u32 x = rb32(hn), g = 0; x && g < 64; g++) { if (x >= t->total) { ERR("%s: nó de mapa fora", t->name); break; } used[x] = 1; u8 *n = bnode(t, x); if ((int8_t)n[8] != 2) ERR("%s: nó %u devia ser de mapa", t->name, x); x = rb32(n); }
    /* percorre por níveis a partir da raiz */
    u32 *level = malloc(sizeof(u32) * t->total), *next = malloc(sizeof(u32) * t->total);
    u32 nl = 0, recs = 0, leaves_first = 0, leaves_last = 0;
    if (t->root) level[nl++] = t->root;
    else if (t->depth || t->leafrecs || t->first || t->last) ERR("%s: árvore vazia com cabeçalho inconsistente", t->name);
    u32 height = t->depth;
    u8 prevkey[1024];
    bool haveprev = false;
    for (u32 lvl = 0; nl; lvl++, height--) {
        u32 nn = 0;
        for (u32 i = 0; i < nl; i++) {
            u32 x = level[i];
            u8 *n = x < t->total ? bnode(t, x) : NULL;
            if (!n) { ERR("%s: nó %u fora da árvore", t->name, x); continue; }
            if (used[x]) { ERR("%s: nó %u alcançado duas vezes", t->name, x); continue; }
            used[x] = 1;
            int8_t kind = (int8_t)n[8];
            if (n[9] != height) ERR("%s: nó %u com altura %u, esperado %u", t->name, x, n[9], height);
            if ((height == 1) != (kind == -1) || (kind != -1 && kind != 0)) ERR("%s: nó %u com tipo %d na altura %u", t->name, x, kind, height);
            /* irmãos: o anterior/seguinte no mesmo nível */
            u32 bl = rb32(n + 4), fl = rb32(n);
            if (bl != (i ? level[i - 1] : 0)) ERR("%s: nó %u bLink %u, esperado %u", t->name, x, bl, i ? level[i - 1] : 0);
            if (fl != (i + 1 < nl ? level[i + 1] : 0)) ERR("%s: nó %u fLink %u, esperado %u", t->name, x, fl, i + 1 < nl ? level[i + 1] : 0);
            int nr = nrecs(n);
            if (!nr) ERR("%s: nó %u vazio", t->name, x);
            if (14 + 2 * (nr + 1) > (int)t->ns) { ERR("%s: nó %u com registros demais", t->name, x); continue; }
            u32 prevoff = 14;
            for (int r = 0; r <= nr; r++) {
                u32 o = roff(t, n, r);
                if (o < prevoff || o > t->ns - 2 * (nr + 1) || (o & 1)) { ERR("%s: nó %u deslocamento %d inválido", t->name, x, r); goto nextnode; }
                prevoff = o;
            }
            for (int r = 0; r < nr; r++) {
                const u8 *rec = n + roff(t, n, r);
                u32 len = roff(t, n, r + 1) - roff(t, n, r), kb = kbytes(rec);
                if (kb > len) { ERR("%s: nó %u registro %d com chave maior que o registro", t->name, x, r); continue; }
                if (kind == -1) {
                    recs++;
                    if (haveprev && hkey_cmp(h, t->tree, prevkey, rec) >= 0) ERR("%s: chaves fora de ordem no nó %u registro %d", t->name, x, r);
                    memcpy(prevkey, rec, kb < sizeof prevkey ? kb : sizeof prevkey);
                    haveprev = true;
                } else {
                    if (r && hkey_cmp(h, t->tree, n + roff(t, n, r - 1), rec) >= 0) ERR("%s: índice fora de ordem no nó %u", t->name, x);
                    if (!t->varidx && rb16(rec) != t->maxkey) ERR("%s: chave de índice de tamanho fixo inválida", t->name);
                    u32 child = rb32(rec + kb);
                    u8 *cn = child < t->total ? bnode(t, child) : NULL;
                    if (!cn || !nrecs(cn)) ERR("%s: índice aponta para nó %u inválido", t->name, child);
                    else if (hkey_cmp(h, t->tree, rec, cn + roff(t, cn, 0))) ERR("%s: chave de índice (nó %u) diferente da 1ª chave do filho %u", t->name, x, child);
                    next[nn++] = child;
                }
            }
            if (kind == -1) { if (!leaves_first) leaves_first = x; leaves_last = x; }
        nextnode:;
        }
        u32 *tmp = level; level = next; next = tmp;
        nl = nn;
        if (lvl > 20) { ERR("%s: profundidade absurda", t->name); break; }
    }
    if (t->root && height != 0) ERR("%s: profundidade %u no cabeçalho não confere", t->name, t->depth);
    if (recs != t->leafrecs) ERR("%s: leafRecords %u, contados %u", t->name, t->leafrecs, recs);
    if (leaves_first != t->first || leaves_last != t->last) ERR("%s: firstLeaf/lastLeaf %u/%u, esperados %u/%u", t->name, t->first, t->last, leaves_first, leaves_last);
    /* mapa de nós */
    u32 setbits = 0;
    u32 bitpos = 0;
    for (u32 x = 0, g = 0; g < 64; g++) {
        u8 *n = bnode(t, x);
        int r = x == 0 ? 2 : 0;
        const u8 *m = n + roff(t, n, r);
        u32 ml = roff(t, n, r + 1) - roff(t, n, r);
        for (u32 b = 0; b < ml * 8; b++, bitpos++) {
            bool bit = m[b / 8] & (0x80 >> (b % 8));
            if (bitpos >= t->total) { if (bit) ERR("%s: bit de mapa além de totalNodes", t->name); continue; }
            if (bit) setbits++;
            if (bit != used[bitpos]) ERR("%s: nó %u %s no mapa mas %s", t->name, bitpos, bit ? "marcado" : "livre", used[bitpos] ? "em uso" : "sem uso");
        }
        x = rb32(n);
        if (!x) break;
    }
    if (t->total - setbits != t->freen) ERR("%s: freeNodes %u, contados %u", t->name, t->freen, t->total - setbits);
    if (!quiet) printf("  %s: %u nós de %u bytes (%u livres), profundidade %u, %u registros\n", t->name, t->total, t->ns, t->freen, t->depth, recs);
    free(used); free(level); free(next);
}

/* ---- dono de cada bloco */
typedef struct { hfs_t *h; u32 *owner; } alloc_t;
static void mark_ext(alloc_t *a, u32 s, u32 c, u32 id, const char *what) {
    if ((u64)s + c > a->h->tb) { ERR("%s (id %u): extent %u+%u fora do volume", what, id, s, c); return; }
    for (u32 i = 0; i < c; i++) {
        if (a->owner[s + i]) { ERR("%s (id %u): bloco %u cruzado com id %u", what, id, s + i, a->owner[s + i]); return; }
        a->owner[s + i] = id;
    }
}

typedef struct { u32 id; u8 type; u32 start; ext_t e[8]; } ovf_t;
static ovf_t *ovf;
static int novf;
static void ovf_cb(void *ctx, const u8 *k, const u8 *d, u32 dl) {
    ovf = realloc(ovf, sizeof *ovf * (novf + 1));
    ovf_t *o = &ovf[novf++];
    o->id = rb32(k + 4); o->type = k[2]; o->start = rb32(k + 8);
    for (int i = 0; i < 8; i++) o->e[i] = (ext_t){rb32(d + 8 * i), rb32(d + 8 * i + 4)};
}

/* todos os extents de um fork: os 8 + overflow; *blocks = soma */
static int fork_all(const u8 *fd, u32 id, u8 type, ext_t *out, int max, u64 *blocks) {
    int n = 0;
    *blocks = 0;
    for (int i = 0; i < 8; i++) { u32 s = rb32(fd + 16 + 8 * i), c = rb32(fd + 20 + 8 * i); if (!c) break; out[n++] = (ext_t){s, c}; *blocks += c; }
    for (int i = 0; i < novf; i++) {
        if (ovf[i].id != id || ovf[i].type != type) continue;
        if (ovf[i].start != *blocks) continue;
        for (int j = 0; j < 8 && n < max; j++) { if (!ovf[i].e[j].count) break; out[n++] = ovf[i].e[j]; *blocks += ovf[i].e[j].count; }
        i = -1;                                             /* procura o próximo registro */
    }
    return n;
}

typedef struct { u32 id, parent; u16 type; u16 flags; u32 valence, fcount; u32 kids, kfolders; bool thread, rec; u8 tparent_ok; } cn_t;
typedef struct { hfs_t *h; cn_t *cn; u32 ncn; alloc_t *a; u32 files, folders, maxid; } catck_t;

static cn_t *cn_get(catck_t *c, u32 id) {
    if (id >= c->ncn) { u32 n = id + 1024; c->cn = realloc(c->cn, sizeof(cn_t) * n); memset(c->cn + c->ncn, 0, sizeof(cn_t) * (n - c->ncn)); c->ncn = n; }
    return &c->cn[id];
}

static void cat_cb(void *ctx, const u8 *k, const u8 *d, u32 dl) {
    catck_t *c = ctx;
    u16 type = rb16(d);
    u32 parent = rb32(k + 2), nl = rb16(k + 6);
    if (type == 1 || type == 2) {
        if (dl < (type == 1 ? 88u : 248u)) { ERR("registro de catálogo curto"); return; }
        u32 id = rb32(d + 8);
        cn_t *n = cn_get(c, id);
        if (n->rec) ERR("CNID %u com dois registros", id);
        n->rec = true; n->id = id; n->parent = parent; n->type = type; n->flags = rb16(d + 2);
        if (id > c->maxid) c->maxid = id;
        if (!nl) ERR("CNID %u sem nome", id);
        if (type == 1) { n->valence = rb32(d + 4); n->fcount = rb32(d + 84); if (id != 2) c->folders++; }
        else {
            c->files++;
            if (!(n->flags & 2)) ERR("arquivo %u sem a flag de thread", id);
            for (int fk = 0; fk < 2; fk++) {
                const u8 *fd = d + 88 + 80 * fk;
                ext_t e[2048];
                u64 blocks;
                int ne = fork_all(fd, id, fk ? 0xFF : 0, e, 2048, &blocks);
                if (blocks != rb32(fd + 12)) ERR("arquivo %u fork %d: totalBlocks %u, extents somam %llu", id, fk, rb32(fd + 12), (unsigned long long)blocks);
                if (rb64(fd) > blocks * c->h->bs) ERR("arquivo %u fork %d: %llu bytes em %llu blocos", id, fk, (unsigned long long)rb64(fd), (unsigned long long)blocks);
                for (int i = 0; i < ne; i++) mark_ext(c->a, e[i].start, e[i].count, id, "arquivo");
            }
        }
        if (parent != 1) {
            cn_t *p = cn_get(c, parent);
            p->kids++;
            if (type == 1) p->kfolders++;
        }
    } else if (type == 3 || type == 4) {
        if (nl) ERR("thread com nome na chave");
        cn_t *n = cn_get(c, parent);
        if (n->thread) ERR("CNID %u com dois threads", parent);
        n->thread = true;
        n->tparent_ok = 0;
        (void)dl;
    } else ERR("registro de catálogo de tipo %u", type);
}

/* segunda passada: thread aponta para (pai, nome) do registro */
typedef struct { catck_t *c; bt_t *t; } thr_t;
static void thr_cb(void *ctx, const u8 *k, const u8 *d, u32 dl) {
    thr_t *x = ctx;
    u16 type = rb16(d);
    if (type != 3 && type != 4) return;
    u32 id = rb32(k + 2);
    cn_t *n = cn_get(x->c, id);
    if (!n->rec) { ERR("thread do CNID %u sem registro", id); return; }
    if ((type == 3) != (n->type == 1)) ERR("thread do CNID %u com tipo errado", id);
    if (rb32(d + 4) != n->parent) ERR("thread do CNID %u diz pai %u, registro está em %u", id, rb32(d + 4), n->parent);
}

typedef struct { u32 id; const u16 *nm; int nl; hfs_t *h; u8 rec[600]; u32 rl; bool ok; } hfind_t;
static void hfind_cb(void *ctx, const u8 *k, const u8 *d, u32 dl) {
    hfind_t *f = ctx;
    if (f->ok || rb32(k + 2) != f->id || rb16(k + 6) != (u32)f->nl) return;
    u8 nb[520];
    for (int i = 0; i < f->nl; i++) wb16(nb + 2 * i, f->nm[i]);
    if (hname_cmp(f->h, k + 8, f->nl, nb, f->nl)) return;
    memcpy(f->rec, d, dl < sizeof f->rec ? dl : sizeof f->rec);
    f->rl = dl;
    f->ok = true;
}

static bool hfs_open(hfs_t *h, u8 *v, u64 size) {
    memset(h, 0, sizeof *h);
    h->v = v; h->size = size; h->vh = v + 1024;
    u16 sig = rb16(h->vh);
    if (sig != 0x482B && sig != 0x4858) return false;
    h->hfsx = sig == 0x4858;
    h->bs = rb32(h->vh + 40); h->tb = rb32(h->vh + 44);
    return h->bs >= 512 && (u64)h->tb * h->bs <= size;
}

static u32 jck(const u8 *p, int n) { u32 c = 0; for (int i = 0; i < n; i++) c = (c << 8) ^ (c + p[i]); return ~c; }

static void hfs_check(u8 *v, u64 size) {
    hfs_t h;
    if (!hfs_open(&h, v, size)) { ERR("cabeçalho HFS+ inválido"); return; }
    const u8 *vh = h.vh;
    u32 attr = rb32(vh + 4);
    printf("%s: %u blocos de %u bytes, atributos %08x, lastMounted '%.4s'\n", h.hfsx ? "HFSX" : "HFS+", h.tb, h.bs, attr, (const char *)vh + 8);
    if (!(attr & 0x100)) WARN("volume não marcado como desmontado corretamente");
    if (attr & 0x800) ERR("volume marcado como inconsistente");
    if (memcmp(vh, v + (u64)h.tb * h.bs - 1024, 512)) {
        /* o cabeçalho alternativo pode ficar atrás em datas/contadores; confere o essencial */
        const u8 *alt = v + (u64)h.tb * h.bs - 1024;
        if (rb16(alt) != rb16(vh) || memcmp(alt + 112, vh + 112, 400)) WARN("cabeçalho alternativo diferente do principal");
    }
    bt_t ext = {"extents", 1}, cat = {"catálogo", 0}, at = {"atributos", 2};
    bt_check(&h, &ext, vh + 192);
    novf = 0; free(ovf); ovf = NULL;
    if (ext.ns && ext.root) bt_leaves(&ext, ovf_cb, NULL);
    /* o catálogo pode ter extents overflow (id 4) */
    {
        u64 b;
        cat.f.n = fork_all(vh + 272, 4, 0, cat.f.ext, 512, &b);
    }
    bt_check(&h, &cat, vh + 272);
    at.f.n = 0;
    { u64 b; if (rb64(vh + 352)) at.f.n = fork_all(vh + 352, 8, 0, at.f.ext, 512, &b); }
    bt_check(&h, &at, vh + 352);
    alloc_t a = {&h, calloc(h.tb, sizeof(u32))};
    /* reservados: 1ºs 1536 bytes e últimos 1024 */
    for (u32 b = 0; (u64)b * h.bs < 1536; b++) a.owner[b] = 0xFFFFFFF0;
    for (u64 o = (u64)h.tb * h.bs - 1024; o < (u64)h.tb * h.bs; o += h.bs) a.owner[o / h.bs] = 0xFFFFFFF0;
    static const struct { u32 off, id; const char *n; } SP[] = {{112, 6, "alocação"}, {192, 3, "extents"}, {272, 4, "catálogo"}, {352, 8, "atributos"}, {432, 7, "startup"}};
    for (int i = 0; i < 5; i++) {
        ext_t e[512];
        u64 blocks;
        int n = fork_all(vh + SP[i].off, SP[i].id, 0, e, 512, &blocks);
        if (blocks != rb32(vh + SP[i].off + 12)) ERR("%s: totalBlocks não confere com os extents", SP[i].n);
        for (int k = 0; k < n; k++) mark_ext(&a, e[k].start, e[k].count, SP[i].id, SP[i].n);
    }
    catck_t cc = {&h, NULL, 0, &a};
    if (cat.ns) bt_leaves(&cat, cat_cb, &cc);
    thr_t tt = {&cc, &cat};
    if (cat.ns) bt_leaves(&cat, thr_cb, &tt);
    for (u32 id = 0; id < cc.ncn; id++) {
        cn_t *n = &cc.cn[id];
        if (n->rec && !n->thread) ERR("CNID %u sem thread", id);
        if (n->kids && !n->rec) ERR("pai %u inexistente com %u filhos", id, n->kids);
        if (n->rec && n->type == 1) {
            if (n->valence != n->kids) ERR("pasta %u: valência %u, filhos %u", id, n->valence, n->kids);
            if ((n->flags & 0x10) && n->fcount != n->kfolders) ERR("pasta %u: folderCount %u, subpastas %u", id, n->fcount, n->kfolders);
        }
        if (n->rec && n->parent != 1) {
            cn_t *p = n->parent < cc.ncn ? &cc.cn[n->parent] : NULL;
            if (!p || !p->rec || p->type != 1) ERR("CNID %u dentro de %u, que não é pasta", id, n->parent);
        }
    }
    if (!cc.ncn || !cc.cn[2].rec) ERR("pasta raiz (CNID 2) ausente");
    if (cc.files != rb32(vh + 32)) ERR("fileCount %u, contados %u", rb32(vh + 32), cc.files);
    if (cc.folders != rb32(vh + 36)) ERR("folderCount %u, contadas %u", rb32(vh + 36), cc.folders);
    if (rb32(vh + 64) <= cc.maxid) ERR("nextCatalogID %u <= maior CNID %u", rb32(vh + 64), cc.maxid);
    for (int i = 0; i < novf; i++) {
        u32 id = ovf[i].id;
        bool special = id >= 3 && id <= 8;
        if (!special && (id >= cc.ncn || !cc.cn[id].rec || cc.cn[id].type != 2)) ERR("extents overflow do id %u sem arquivo", id);
    }
    /* atributos com fork próprio */
    if (at.ns && at.root) {
        u32 x = at.first;
        for (u32 g = 0; x && g < at.total; g++) {
            u8 *n = bnode(&at, x);
            for (int r = 0; r < nrecs(n); r++) {
                const u8 *rec = n + roff(&at, n, r), *d = rec + kbytes(rec);
                u32 id = rb32(rec + 4);
                if (id >= cc.ncn || !cc.cn[id].rec) ERR("atributo do id %u sem arquivo", id);
                if (rb32(d) == 0x20) for (int e = 0; e < 8; e++) { u32 s = rb32(d + 8 + 16 + 8 * e), c = rb32(d + 8 + 20 + 8 * e); if (c) mark_ext(&a, s, c, id, "atributo"); }
                if (rb32(d) == 0x30) for (int e = 0; e < 8; e++) { u32 s = rb32(d + 8 + 8 * e), c = rb32(d + 12 + 8 * e); if (c) mark_ext(&a, s, c, id, "atributo"); }
            }
            x = rb32(n);
        }
    }
    /* bitmap no disco x calculado */
    hfork_t bm = {0};
    { u64 b; bm.n = fork_all(vh + 112, 6, 0, bm.ext, 512, &b); }
    u32 used = 0, diff = 0;
    if (hfork_read(&h, &bm, vh + 112)) {
        for (u32 b = 0; b < h.tb; b++) {
            bool on = bm.data[b / 8] & (0x80 >> (b % 8));
            bool want = a.owner[b] != 0;
            if (want) used++;
            if (on != want && diff++ < 8) ERR("bloco %u: bitmap diz %s, mas está %s", b, on ? "usado" : "livre", want ? "em uso" : "sem dono");
        }
        if (diff > 8) ERR("... %u divergências no bitmap ao todo", diff);
    } else ERR("bitmap ilegível");
    if (h.tb - used != rb32(vh + 48)) ERR("freeBlocks %u, contados %u", rb32(vh + 48), h.tb - used);
    /* journal */
    if (attr & 0x2000) {
        u32 jb = rb32(vh + 12);
        const u8 *jib = v + (u64)jb * h.bs;
        if (!jb || !a.owner[jb]) ERR("bloco de informações do journal não alocado");
        u64 joff = rb64(jib + 36), jsz = rb64(jib + 44);
        const u8 *jh = v + joff;
        bool le = rl32(jh) == 0x4a4e4c78;
        u32 (*g32)(const u8 *) = le ? rl32 : rb32;
        u64 (*g64)(const u8 *) = le ? rl64 : rb64;
        if (g32(jh) != 0x4a4e4c78 || g32(jh + 4) != 0x12345678) ERR("cabeçalho do journal inválido");
        else {
            u8 tmp[44];
            memcpy(tmp, jh, 44);
            memset(tmp + 36, 0, 4);
            if (jck(tmp, 44) != g32(jh + 36)) ERR("checksum do cabeçalho do journal");
            if (g64(jh + 24) != jsz) ERR("tamanho do journal difere do JIB");
            if (g64(jh + 8) != g64(jh + 16)) WARN("journal com transações pendentes (start %llu, end %llu)", (unsigned long long)g64(jh + 8), (unsigned long long)g64(jh + 16));
            else printf("  journal: %llu KB, limpo\n", (unsigned long long)(jsz >> 10));
        }
        for (u64 o = joff; o < joff + jsz; o += h.bs) if (!a.owner[o / h.bs]) { ERR("área do journal com bloco livre (%llu)", (unsigned long long)(o / h.bs)); break; }
    }
    printf("  %u arquivos, %u pastas, %u blocos livres\n", cc.files, cc.folders, rb32(vh + 48));
    free(a.owner);
    free(cc.cn);
}

/* ---- ls/cat do HFS+ */
static void u16name(const u8 *k, char *out, int cap) {
    u16 w[256];
    int n = rb16(k + 6);
    for (int i = 0; i < n && i < 255; i++) { w[i] = rb16(k + 8 + 2 * i); if (w[i] == '/') w[i] = ':'; }
    utf16_to_utf8(w, n, out, cap);
}
typedef struct { u32 parent; char path[2048]; int depth; bt_t *t; hfs_t *h; } hls_t;
static void hls_cb(void *ctx, const u8 *k, const u8 *d, u32 dl);
static void hls_dir(hls_t *l) { bt_leaves(l->t, hls_cb, l); }
static void hls_cb(void *ctx, const u8 *k, const u8 *d, u32 dl) {
    hls_t *l = ctx;
    if (rb32(k + 2) != l->parent || !rb16(k + 6)) return;
    u16 type = rb16(d);
    char nm[1024];
    u16name(k, nm, sizeof nm);
    if (nm[0] == 0 || (u8)nm[0] < 0x20) return;            /* pastas privadas */
    if (type == 1) {
        printf("%s/%s/\n", l->path, nm);
        if (l->depth < 16) { hls_t s = *l; s.parent = rb32(d + 8); s.depth++; snprintf(s.path, sizeof s.path, "%s/%s", l->path, nm); hls_dir(&s); }
    } else if (type == 2) printf("%s/%s  (%llu bytes)\n", l->path, nm, (unsigned long long)rb64(d + 88));
}

static bool hfs_cat_tree(hfs_t *h, bt_t *cat, bt_t *ext) {
    novf = 0; free(ovf); ovf = NULL;
    bt_check(h, ext, h->vh + 192);
    if (ext->ns && ext->root) bt_leaves(ext, ovf_cb, NULL);
    u64 b;
    cat->f.n = fork_all(h->vh + 272, 4, 0, cat->f.ext, 512, &b);
    bt_check(h, cat, h->vh + 272);
    return cat->ns != 0;
}

static bool hfs_lookup(hfs_t *h, bt_t *cat, const char *path, u8 *rec) {
    u32 cur = 2;
    char part[512];
    const char *p = path;
    bool any = false;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        int n = 0;
        while (*p && *p != '/') part[n++] = *p++;
        part[n] = 0;
        u16 nm[256];
        int nl = to_u16(part, nm);
        hfind_t f = {cur, nm, nl, h};
        bt_leaves(cat, hfind_cb, &f);
        if (!f.ok) return false;
        memcpy(rec, f.rec, 600);
        any = true;
        if (rb16(f.rec) == 1) cur = rb32(f.rec + 8);
    }
    return any;
}

/* ================================================================ HFS+: formatação */
static u8 *hv;
static u32 hbs;
static void hmark(u8 *bm, u32 s, u32 c) { for (u32 i = 0; i < c; i++) bm[(s + i) / 8] |= 0x80 >> ((s + i) % 8); }

static void tree_init(u8 *t, u32 ns, u32 total, u32 maxkey, u32 attrs, u8 cmptype, u32 clump) {
    memset(t, 0, (u64)ns * total);
    t[8] = 1;                                              /* cabeçalho */
    wb16(t + 10, 3);
    u8 *hr = t + 14;
    wb16(hr + 18, (u16)ns);
    wb16(hr + 20, (u16)maxkey);
    wb32(hr + 22, total);
    wb32(hr + 26, total - 1);
    wb32(hr + 32, clump);
    hr[37] = cmptype;
    wb32(hr + 38, attrs);
    wb16(t + ns - 2, 14);
    wb16(t + ns - 4, 14 + 106);
    wb16(t + ns - 6, 14 + 106 + 128);
    wb16(t + ns - 8, (u16)(ns - 8));
    t[14 + 106 + 128] = 0x80;                              /* nó 0 em uso */
}

typedef struct { u8 k[520]; u32 kl; u8 d[520]; u32 dl; } hrec_t;
static hfs_t mk;                                          /* para comparar chaves ao ordenar */
static int hrec_cmp(const void *a, const void *b) { return hkey_cmp(&mk, 0, ((const hrec_t *)a)->k, ((const hrec_t *)b)->k); }

static u32 ckey(u8 *k, u32 parent, const char *ascii_name, int prefix_nuls) {
    int n = 0;
    wb32(k + 2, parent);
    for (int i = 0; i < prefix_nuls; i++) wb16(k + 8 + 2 * n++, 0);
    for (const char *s = ascii_name; s && *s; s++) wb16(k + 8 + 2 * n++, (u8)*s);
    wb16(k + 6, (u16)n);
    wb16(k, (u16)(6 + 2 * n));
    return 8 + 2 * n;
}

static void mkhfs(u8 *v, u64 size, int kind, const char *label) {
    bool hfsx = kind == 1, jnl = kind == 2;
    const u32 bs = 4096, ns = 4096;
    u32 tb = (u32)(size / bs);
    memset(v, 0, size);
    hv = v; hbs = bs;
    memset(&mk, 0, sizeof mk);
    mk.binary = false;
    u32 next = 1;                                         /* bloco 0: cabeçalho */
    u32 bm_blocks = (tb / 8 + bs - 1) / bs;
    u32 alloc_s = next; next += bm_blocks;
    u32 ext_nodes = 4, cat_nodes = 8, attr_nodes = 4;
    u32 ext_s = next; next += ext_nodes * ns / bs;
    u32 cat_s = next; next += cat_nodes * ns / bs;
    u32 attr_s = next; next += attr_nodes * ns / bs;
    u32 jib_s = 0, j_s = 0, j_blocks = 0;
    if (jnl) { jib_s = next++; j_blocks = (1u << 20) / bs; j_s = next; next += j_blocks; }
    u32 seed_s = next++;
    u8 *bm = v + (u64)alloc_s * bs;
    hmark(bm, 0, next);
    hmark(bm, tb - 1, 1);                                 /* cabeçalho alternativo */
    /* B-trees */
    tree_init(v + (u64)ext_s * bs, ns, ext_nodes, 10, 2, 0, ext_nodes * ns);
    tree_init(v + (u64)attr_s * bs, ns, attr_nodes, 266, 6, 0, attr_nodes * ns);
    u8 *ct = v + (u64)cat_s * bs;
    tree_init(ct, ns, cat_nodes, 516, 6, hfsx ? 0xBC : 0xCF, cat_nodes * ns);
    if (hfsx) mk.binary = true;
    u32 now = (u32)time(NULL) + HFS_DELTA;
    hrec_t r[8];
    int nr = 0;
    u16 vn[256];
    int vnl = to_u16(label, vn);
    /* raiz: registro (1, rótulo) e thread (2, "") */
    {
        hrec_t *x = &r[nr++];
        wb32(x->k + 2, 1); wb16(x->k + 6, (u16)vnl); for (int i = 0; i < vnl; i++) wb16(x->k + 8 + 2 * i, vn[i]);
        wb16(x->k, (u16)(6 + 2 * vnl)); x->kl = 8 + 2 * vnl;
        memset(x->d, 0, 88);
        wb16(x->d, 1); wb16(x->d + 2, hfsx ? 0x10 : 0); wb32(x->d + 4, jnl ? 3 : 1); wb32(x->d + 8, 2);
        for (int i = 12; i < 28; i += 4) wb32(x->d + i, now);
        wb32(x->d + 32, 0); wb32(x->d + 36, 80); wb16(x->d + 42, 040755);
        x->dl = 88;
        hrec_t *t = &r[nr++];
        t->kl = ckey(t->k, 2, NULL, 0);
        memset(t->d, 0, 520);
        wb16(t->d, 3); wb32(t->d + 4, 1); wb16(t->d + 8, (u16)vnl); for (int i = 0; i < vnl; i++) wb16(t->d + 10 + 2 * i, vn[i]);
        t->dl = 10 + 2 * vnl;
    }
    /* arquivos: seed.txt e (com journal) .journal_info_block e .journal */
    struct { const char *n; u32 id, s, c; u64 len; } files[3];
    int nf = 0;
    const char *seed = "semente do volimg\n";
    memcpy(v + (u64)seed_s * bs, seed, strlen(seed));
    u32 id = 16;
    if (jnl) {
        files[nf++] = (typeof(files[0])){".journal_info_block", id++, jib_s, 1, bs};
        files[nf++] = (typeof(files[0])){".journal", id++, j_s, j_blocks, (u64)j_blocks * bs};
    }
    files[nf++] = (typeof(files[0])){"seed.txt", id++, seed_s, 1, strlen(seed)};
    for (int i = 0; i < nf; i++) {
        hrec_t *x = &r[nr++];
        x->kl = ckey(x->k, 2, files[i].n, 0);
        memset(x->d, 0, 248);
        wb16(x->d, 2); wb16(x->d + 2, 2); wb32(x->d + 8, files[i].id);
        for (int k = 12; k < 28; k += 4) wb32(x->d + k, now);
        wb16(x->d + 42, 0100644); wb32(x->d + 32, files[i].n[0] == '.' ? 0 : 99); wb32(x->d + 36, 99);
        if (files[i].n[0] == '.') { x->d[40] = 0; x->d[41] = 0x02; wb16(x->d + 56, 0x4000); }   /* invisível, imutável */
        wb64(x->d + 88, files[i].len); wb32(x->d + 100, files[i].c); wb32(x->d + 104, files[i].s); wb32(x->d + 108, files[i].c);
        x->dl = 248;
        hrec_t *t = &r[nr++];
        t->kl = ckey(t->k, files[i].id, NULL, 0);
        memset(t->d, 0, 520);
        wb16(t->d, 4); wb32(t->d + 4, 2);
        int l = 0;
        for (const char *s = files[i].n; *s; s++) wb16(t->d + 10 + 2 * l++, (u8)*s);
        wb16(t->d + 8, (u16)l);
        t->dl = 10 + 2 * l;
    }
    wb32(r[0].d + 4, (u32)nf);                             /* valência da raiz */
    qsort(r, nr, sizeof *r, hrec_cmp);
    u8 *leaf = ct + ns;
    leaf[8] = 0xFF; leaf[9] = 1; wb16(leaf + 10, (u16)nr);
    u32 off = 14;
    for (int i = 0; i < nr; i++) {
        wb16(leaf + ns - 2 * (i + 1), (u16)off);
        memcpy(leaf + off, r[i].k, r[i].kl); off += r[i].kl;
        memcpy(leaf + off, r[i].d, r[i].dl); off += r[i].dl;
    }
    wb16(leaf + ns - 2 * (nr + 1), (u16)off);
    u8 *hr = ct + 14;
    wb16(hr, 1); wb32(hr + 2, 1); wb32(hr + 6, nr); wb32(hr + 10, 1); wb32(hr + 14, 1); wb32(hr + 26, cat_nodes - 2);
    ct[14 + 106 + 128] = 0xC0;
    /* journal */
    if (jnl) {
        u8 *jib = v + (u64)jib_s * bs;
        wb32(jib, 1);
        wb64(jib + 36, (u64)j_s * bs);
        wb64(jib + 44, (u64)j_blocks * bs);
        u8 *jh = v + (u64)j_s * bs;
        wl32(jh, 0x4a4e4c78); wl32(jh + 4, 0x12345678); wl64(jh + 8, 512); wl64(jh + 16, 512); wl64(jh + 24, (u64)j_blocks * bs);
        wl32(jh + 32, 4096); wl32(jh + 40, 512); wl32(jh + 44, 1);
        wl32(jh + 36, jck(jh, 44));
    }
    /* cabeçalho do volume */
    u8 *vh = v + 1024;
    wb16(vh, hfsx ? 0x4858 : 0x482B); wb16(vh + 2, hfsx ? 5 : 4);
    wb32(vh + 4, 0x100 | (jnl ? 0x2000 : 0));
    memcpy(vh + 8, jnl ? "HFSJ" : "10.0", 4);
    wb32(vh + 12, jib_s);
    wb32(vh + 16, now); wb32(vh + 20, now); wb32(vh + 28, now);
    wb32(vh + 32, (u32)nf); wb32(vh + 36, 0);
    wb32(vh + 40, bs); wb32(vh + 44, tb); wb32(vh + 48, tb - next - 1); wb32(vh + 52, next);
    wb32(vh + 56, 65536); wb32(vh + 60, 65536); wb32(vh + 64, id);
    wb64(vh + 72, 1);
    struct { u32 off, s, c; } F[4] = {{112, alloc_s, bm_blocks}, {192, ext_s, ext_nodes}, {272, cat_s, cat_nodes}, {352, attr_s, attr_nodes}};
    for (int i = 0; i < 4; i++) {
        u32 blocks = i ? F[i].c * ns / bs : F[i].c;
        wb64(vh + F[i].off, i ? (u64)F[i].c * ns : (u64)blocks * bs);
        wb32(vh + F[i].off + 8, i ? F[i].c * ns : bs);
        wb32(vh + F[i].off + 12, blocks);
        wb32(vh + F[i].off + 16, F[i].s);
        wb32(vh + F[i].off + 20, blocks);
    }
    memcpy(v + (u64)tb * bs - 1024, vh, 512);
    printf("%s \"%s\": %u blocos de %u bytes%s\n", hfsx ? "HFSX" : "HFS+", label, tb, bs, jnl ? ", journal de 1 MB" : "");
}

/* ================================================================ jinject */
static int jinject(hfs_t *h, const char *path, const char *text) {
    bt_t cat = {"catálogo", 0}, ext = {"extents", 1};
    if (!hfs_cat_tree(h, &cat, &ext)) return 1;
    u8 rec[600];
    if (!hfs_lookup(h, &cat, path, rec) || rb16(rec) != 2) { fprintf(stderr, "não achei %s\n", path); return 1; }
    u32 blk = rb32(rec + 88 + 16);
    if (!(rb32(h->vh + 4) & 0x2000)) { fprintf(stderr, "volume sem journal\n"); return 1; }
    const u8 *jib = h->v + (u64)rb32(h->vh + 12) * h->bs;
    u64 joff = rb64(jib + 36);
    u8 *jh = h->v + joff;
    u32 jhs = rl32(jh + 40), bls = rl32(jh + 32), seq = rl32(jh + 44);
    u64 end = rl64(jh + 16), jsize = rl64(jh + 24);
    if (end + bls + h->bs > jsize) { fprintf(stderr, "journal sem espaço contíguo (teste simples)\n"); return 1; }
    u8 *bl = h->v + joff + end;
    memset(bl, 0, bls);
    wl16(bl, (u16)(bls / 16 - 1)); wl16(bl + 2, 2); wl32(bl + 4, bls + h->bs); wl32(bl + 12, 2);
    wl32(bl + 28, seq + 1);
    wl64(bl + 32, (u64)blk * h->bs / jhs); wl32(bl + 40, h->bs);
    wl32(bl + 8, jck(bl, 32));
    u8 *data = bl + bls;
    memset(data, 0, h->bs);
    memcpy(data, text, strlen(text));
    wl64(jh + 16, end + bls + h->bs);
    wl32(jh + 44, seq + 1);
    wl32(jh + 36, 0);
    wl32(jh + 36, jck(jh, 44));
    /* o tamanho lógico passa a ser o do texto: isso já está no registro? não — o teste usa um
     * arquivo cujo tamanho é igual ao do texto (seed.txt = 18 bytes) */
    printf("transação pendente: bloco %u <- \"%s\"\n", blk, text);
    return 0;
}

/* ================================================================ main */
static int detect(u8 *v, u64 size) {
    if (size > 1536 && (rb16(v + 1024) == 0x482B || rb16(v + 1024) == 0x4858)) return 2;
    fat_t f;
    if (fat_open(&f, v, size)) return 1;
    return 0;
}

#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <shellapi.h>
int main(int argc, char **argv) {
    srand((unsigned)time(NULL));
    /* argv em UTF-8 (o CRT entrega na página de código ANSI) e saída binária (cat) */
    {
        int wn = 0;
        wchar_t **wv = CommandLineToArgvW(GetCommandLineW(), &wn);
        if (wv && wn == argc) for (int i = 0; i < argc; i++) {
            int n = WideCharToMultiByte(CP_UTF8, 0, wv[i], -1, NULL, 0, NULL, NULL);
            argv[i] = malloc(n);
            WideCharToMultiByte(CP_UTF8, 0, wv[i], -1, argv[i], n, NULL, NULL);
        }
        _setmode(_fileno(stdout), _O_BINARY);
    }
    quiet = argc > 1 && strcmp(argv[1], "check");
    if (argc < 3) {
        fprintf(stderr, "uso: volimg mkdisk <img> <MB> <spec>... | check <img> | ls <img> [n] | cat <img> <n> <caminho> | jinject <img> <n> <caminho> <texto>\n");
        return 2;
    }
    const char *cmd = argv[1], *img = argv[2];
    if (!strcmp(cmd, "mkdisk")) {
        if (argc < 5) return 2;
        disk_size = (u64)atoi(argv[3]) << 20;
        disk = calloc(1, disk_size);
        int n = argc - 4;
        part_t p[16];
        bool hfs[16];
        u64 cur = 1 << 20, avail = disk_size - (2 << 20);
        int nfixed = 0;
        u64 fixed = 0;
        for (int i = 0; i < n; i++) { const char *s = strchr(argv[4 + i], ':'); s = s ? strchr(s + 1, ':') : NULL; if (s) { fixed += (u64)atoi(s + 1) << 20; nfixed++; } }
        for (int i = 0; i < n && i < 16; i++) {
            char spec[64], label[32] = "", *c;
            snprintf(spec, sizeof spec, "%s", argv[4 + i]);
            u64 sz = n > nfixed ? (avail - fixed) / (n - nfixed) : 0;
            if ((c = strchr(spec, ':'))) { *c++ = 0; char *d = strchr(c, ':'); if (d) { *d++ = 0; sz = (u64)atoi(d) << 20; } snprintf(label, sizeof label, "%s", c); }
            sz = sz / 4096 * 4096;
            p[i] = (part_t){cur, sz};
            hfs[i] = !strncmp(spec, "hfs", 3);
            u8 *v = disk + cur;
            if (!strcmp(spec, "fat12")) mkfat(v, sz, 12, label[0] ? label : "FAT12");
            else if (!strcmp(spec, "fat16")) mkfat(v, sz, 16, label[0] ? label : "FAT16");
            else if (!strcmp(spec, "fat32")) mkfat(v, sz, 32, label[0] ? label : "FAT32");
            else if (!strcmp(spec, "hfs")) mkhfs(v, sz, 0, label[0] ? label : "HFS Teste");
            else if (!strcmp(spec, "hfsx")) mkhfs(v, sz, 1, label[0] ? label : "HFSX Teste");
            else if (!strcmp(spec, "hfsj")) mkhfs(v, sz, 2, label[0] ? label : "HFSJ Teste");
            else { fprintf(stderr, "spec desconhecida: %s\n", spec); return 2; }
            cur += sz;
        }
        write_gpt(p, hfs, n);
        return save(img) ? 0 : 1;
    }
    if (!load(img)) return 1;
    scan_parts();
    if (!strcmp(cmd, "check")) {
        for (int i = 0; i < nparts; i++) {
            printf("volume %d (%llu MB em %llu):\n", i + 1, (unsigned long long)(parts[i].size >> 20), (unsigned long long)parts[i].off);
            u8 *v = disk + parts[i].off;
            int t = detect(v, parts[i].size);
            if (t == 2) hfs_check(v, parts[i].size);
            else if (t == 1) fat_check(v, parts[i].size);
            else printf("  (sistema de arquivos desconhecido)\n");
        }
        printf("%d erro(s), %d aviso(s)\n", errors, warnings);
        return errors ? 1 : 0;
    }
    int n = argc > 3 ? atoi(argv[3]) - 1 : 0;
    if (n < 0 || n >= nparts) { fprintf(stderr, "volume inexistente\n"); return 1; }
    u8 *v = disk + parts[n].off;
    u64 vs = parts[n].size;
    int t = detect(v, vs);
    if (!strcmp(cmd, "ls")) {
        if (t == 1) { fat_t f; fat_open(&f, v, vs); fls_t l = {"", 0}; fat_entries(&f, f.bits == 32 ? f.root : 0, false, "/", ls_cb, &l); }
        else if (t == 2) { hfs_t h; hfs_open(&h, v, vs); bt_t cat = {"catálogo", 0}, ext = {"extents", 1}; if (hfs_cat_tree(&h, &cat, &ext)) { hls_t l = {2, "", 0, &cat, &h}; hls_dir(&l); } }
        return 0;
    }
    if (!strcmp(cmd, "cat") && argc > 4) {
        if (t == 1) {
            fat_t f;
            fat_open(&f, v, vs);
            fent_t e;
            if (!fat_resolve(&f, argv[4], &e) || (e.attr & 0x10)) { fprintf(stderr, "não achei %s\n", argv[4]); return 1; }
            u32 x = e.clus, left = e.size;
            while (left && x >= 2) { u32 k = left < f.clus ? left : f.clus; fwrite(fat_cl(&f, x), 1, k, stdout); left -= k; x = fat_ent(&f, 0, x); }
            return 0;
        }
        if (t == 2) {
            hfs_t h;
            hfs_open(&h, v, vs);
            bt_t cat = {"catálogo", 0}, ext = {"extents", 1};
            u8 rec[600];
            if (!hfs_cat_tree(&h, &cat, &ext) || !hfs_lookup(&h, &cat, argv[4], rec) || rb16(rec) != 2) { fprintf(stderr, "não achei %s\n", argv[4]); return 1; }
            ext_t e[4096];
            u64 blocks;
            int ne = fork_all(rec + 88, rb32(rec + 8), 0, e, 4096, &blocks);
            u64 left = rb64(rec + 88);
            for (int i = 0; i < ne && left; i++) {
                u64 k = (u64)e[i].count * h.bs;
                if (k > left) k = left;
                fwrite(v + (u64)e[i].start * h.bs, 1, k, stdout);
                left -= k;
            }
            return 0;
        }
        return 1;
    }
    if (!strcmp(cmd, "jinject") && argc > 5 && t == 2) {
        hfs_t h;
        hfs_open(&h, v, vs);
        int r = jinject(&h, argv[4], argv[5]);
        return r || !save(img);
    }
    fprintf(stderr, "comando inválido\n");
    return 2;
}
