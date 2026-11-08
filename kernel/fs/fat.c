/*
 * mOS — driver FAT32 com leitura e escrita (nomes longos VFAT/LFN, UTF-8 <-> UTF-16).
 *
 * Modelo simples e robusto: diretórios são lidos inteiros para a RAM, alterados e gravados de
 * volta; a FAT passa por um cache de blocos de 4 KiB gravado em todas as cópias no fat_sync().
 * Ao substituir um arquivo, a nova cadeia de clusters é gravada antes de a antiga ser liberada.
 *
 * Também registra volumes FAT12/16/32 no dfs (/Volumes), exceto o volume do sistema. Em discos
 * graváveis eles aceitam escrita pelo VFS (vfs_bridge.c); partições EFI ficam só leitura.
 */
#include "fs.h"
#ifndef MOS_HOST_TEST
#include "crypto.h"
#endif

#define FAT_EOC      0x0FFFFFF8u
#define FAT_MASK     0x0FFFFFFFu
#define FCACHE       32
#define FBLK         4096u
#define ATTR_RO      0x01
#define ATTR_HIDDEN  0x02
#define ATTR_SYSTEM  0x04
#define ATTR_LABEL   0x08
#define ATTR_DIR     0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LFN     0x0F

struct fatfs {
    blkdev_t *dev;
    u32 bps, spc, clus;          /* bytes/setor, setores/cluster, bytes/cluster */
    u64 fat_off, data_off;       /* deslocamentos em bytes */
    u32 fat_bytes, nfats;
    u32 root, nclus;             /* clusters válidos: 2 .. nclus+1; FAT12/16: root = 0 (região fixa) */
    u32 fsinfo_sec, hint;
    int type;                    /* 12, 16 ou 32 */
    u64 root_off;                /* FAT12/16: deslocamento e tamanho da raiz fixa */
    u32 root_bytes;
    bool rw;
    char label[12];
    struct { u32 blk; bool valid, dirty; u8 *data; } cache[FCACHE];
};

typedef struct { u32 clus; u8 *buf; u32 len; } dirbuf_t;

typedef struct {
    char name[768];
    u8   attr;
    u32  clus, size;
    int  first, sfn;             /* índices de entrada (LFN inicial e entrada 8.3) */
    i64  mtime;
} fent_t;

/* ================================================================ FAT (tabela) */
static u8 *fat_block(fatfs_t *fs, u32 blk) {
    int s = blk % FCACHE;
    if (fs->cache[s].valid && fs->cache[s].blk == blk) return fs->cache[s].data;
    if (fs->cache[s].valid && fs->cache[s].dirty) {
        u32 off = fs->cache[s].blk * FBLK, n = MIN(FBLK, fs->fat_bytes - off);
        for (u32 f = 0; f < fs->nfats; f++)
            blk_write(fs->dev, fs->fat_off + (u64)f * fs->fat_bytes + off, fs->cache[s].data, n);
    }
    if (!fs->cache[s].data && !(fs->cache[s].data = kmalloc(FBLK))) return NULL;
    u32 off = blk * FBLK, n = MIN(FBLK, fs->fat_bytes - off);
    memset(fs->cache[s].data, 0, FBLK);
    if (!blk_read(fs->dev, fs->fat_off + off, fs->cache[s].data, n)) { fs->cache[s].valid = false; return NULL; }
    fs->cache[s].blk = blk;
    fs->cache[s].valid = true;
    fs->cache[s].dirty = false;
    return fs->cache[s].data;
}

/* acesso byte a byte à tabela (entradas de FAT12 podem cruzar a fronteira de um bloco do cache) */
static int fat_byte(fatfs_t *fs, u32 off) {
    u8 *b = fat_block(fs, off / FBLK);
    return b ? b[off % FBLK] : -1;
}
static void fat_byte_set(fatfs_t *fs, u32 off, u8 v) {
    u32 blk = off / FBLK;
    u8 *b = fat_block(fs, blk);
    if (!b) return;
    b[off % FBLK] = v;
    fs->cache[blk % FCACHE].dirty = true;
}

/* valores são normalizados para a convenção do FAT32: >= FAT_EOC = fim de cadeia (ou ruim) */
static u32 fat_get(fatfs_t *fs, u32 c) {
    if (c < 2 || c > fs->nclus + 1) return FAT_EOC;
    if (fs->type == 32) {
        u8 *b = fat_block(fs, c * 4 / FBLK);
        return b ? le32(b + (c * 4) % FBLK) & FAT_MASK : FAT_EOC;
    }
    u32 v;
    if (fs->type == 16) {
        int lo = fat_byte(fs, c * 2), hi = fat_byte(fs, c * 2 + 1);
        if (lo < 0 || hi < 0) return FAT_EOC;
        v = (u32)(lo | hi << 8);
        return v >= 0xFFF7 ? FAT_EOC : v;
    }
    u32 off = c + c / 2;
    int lo = fat_byte(fs, off), hi = fat_byte(fs, off + 1);
    if (lo < 0 || hi < 0) return FAT_EOC;
    v = (u32)(lo | hi << 8);
    v = c & 1 ? v >> 4 : v & 0xFFF;
    return v >= 0xFF7 ? FAT_EOC : v;
}

static bool fat_set(fatfs_t *fs, u32 c, u32 v) {
    if (c < 2 || c > fs->nclus + 1) return false;
    if (fs->type == 32) {
        u32 blk = c * 4 / FBLK;
        u8 *b = fat_block(fs, blk);
        if (!b) return false;
        u8 *p = b + (c * 4) % FBLK;
        u32 nv = (le32(p) & ~FAT_MASK) | (v & FAT_MASK);
        p[0] = (u8)nv; p[1] = (u8)(nv >> 8); p[2] = (u8)(nv >> 16); p[3] = (u8)(nv >> 24);
        fs->cache[blk % FCACHE].dirty = true;
        return true;
    }
    if (fs->type == 16) {
        u32 nv = v >= FAT_EOC ? 0xFFFF : v & 0xFFFF;
        fat_byte_set(fs, c * 2, (u8)nv);
        fat_byte_set(fs, c * 2 + 1, (u8)(nv >> 8));
        return true;
    }
    u32 nv = v >= FAT_EOC ? 0xFFF : v & 0xFFF, off = c + c / 2;
    int lo = fat_byte(fs, off), hi = fat_byte(fs, off + 1);
    if (lo < 0 || hi < 0) return false;
    u32 w = (u32)(lo | hi << 8);
    w = c & 1 ? (w & 0x000F) | nv << 4 : (w & 0xF000) | nv;
    fat_byte_set(fs, off, (u8)w);
    fat_byte_set(fs, off + 1, (u8)(w >> 8));
    return true;
}

static bool is_eoc(u32 v) { return v >= FAT_EOC || v < 2; }

static u32 alloc_cluster(fatfs_t *fs) {
    for (u32 i = 0; i < fs->nclus; i++) {
        u32 c = 2 + (fs->hint - 2 + i) % fs->nclus;
        if (fat_get(fs, c) == 0) {
            fat_set(fs, c, FAT_MASK);
            fs->hint = c + 1 > fs->nclus + 1 ? 2 : c + 1;
            return c;
        }
    }
    return 0;
}

static void free_chain(fatfs_t *fs, u32 c) {
    for (u32 guard = 0; !is_eoc(c) && guard <= fs->nclus; guard++) {
        u32 nx = fat_get(fs, c);
        fat_set(fs, c, 0);
        if (c < fs->hint) fs->hint = c;
        c = nx;
    }
}

/* aloca n clusters encadeados; 0 se faltar espaço (nada fica alocado) */
static u32 alloc_chain(fatfs_t *fs, u32 n) {
    u32 first = 0, prev = 0;
    for (u32 i = 0; i < n; i++) {
        u32 c = alloc_cluster(fs);
        if (!c) { if (first) free_chain(fs, first); return 0; }
        if (prev) fat_set(fs, prev, c); else first = c;
        prev = c;
    }
    return first;
}

static u64 clus_off(fatfs_t *fs, u32 c) { return fs->data_off + (u64)(c - 2) * fs->clus; }

/* ================================================================ dados */
/* lê len bytes a partir do deslocamento off de uma cadeia */
static bool chain_read(fatfs_t *fs, u32 c, u64 off, u8 *out, u64 len) {
    u32 guard = 0;
    while (off >= fs->clus) {
        if (is_eoc(c) || ++guard > fs->nclus) return false;
        c = fat_get(fs, c); off -= fs->clus;
    }
    while (len) {
        if (is_eoc(c) || ++guard > fs->nclus + 1) return false;
        /* junta clusters contíguos numa única leitura */
        u32 run = 1;
        u64 avail = fs->clus - off;
        while (avail < len) {
            u32 nx = fat_get(fs, c + run - 1);
            if (nx != c + run) break;
            run++; avail += fs->clus;
        }
        u64 k = MIN(len, avail);
        if (!blk_read(fs->dev, clus_off(fs, c) + off, out, k)) return false;
        out += k; len -= k; off = 0;
        u32 last = c + run - 1;
        c = fat_get(fs, last);
    }
    return true;
}

static bool chain_write(fatfs_t *fs, u32 c, const u8 *src, u64 len) {
    u32 guard = 0;
    u8 *pad = NULL;
    while (len) {
        if (is_eoc(c) || ++guard > fs->nclus + 1) { kfree(pad); return false; }
        u32 run = 1;
        while ((u64)run * fs->clus < len && fat_get(fs, c + run - 1) == c + run) run++;
        u64 bytes = MIN(len, (u64)run * fs->clus);
        u64 whole = bytes / fs->clus * fs->clus;
        if (whole && !blk_write(fs->dev, clus_off(fs, c), src, whole)) { kfree(pad); return false; }
        if (bytes > whole) {                 /* último cluster parcial: completa com zeros */
            if (!pad && !(pad = kmalloc(fs->clus))) return false;
            memset(pad, 0, fs->clus);
            memcpy(pad, src + whole, bytes - whole);
            if (!blk_write(fs->dev, clus_off(fs, c) + whole, pad, fs->clus)) { kfree(pad); return false; }
        }
        src += bytes; len -= bytes;
        c = fat_get(fs, c + run - 1);
    }
    kfree(pad);
    return true;
}

static u32 chain_len(fatfs_t *fs, u32 c) {
    u32 n = 0;
    while (!is_eoc(c) && n <= fs->nclus) { n++; c = fat_get(fs, c); }
    return n;
}

/* ================================================================ diretórios */
static bool dir_load(fatfs_t *fs, u32 clus, dirbuf_t *d) {
    d->clus = clus;
    if (clus == 0 && fs->type != 32) {            /* raiz fixa do FAT12/16 */
        d->len = fs->root_bytes;
        d->buf = kmalloc(d->len);
        if (!d->buf) return false;
        if (!blk_read(fs->dev, fs->root_off, d->buf, d->len)) { kfree(d->buf); d->buf = NULL; return false; }
        return true;
    }
    u32 n = chain_len(fs, clus);
    if (!n || (u64)n * fs->clus > (2u << 20)) return false;   /* FAT limita diretórios a 65536 entradas */
    d->len = n * fs->clus;
    d->buf = kmalloc(d->len);
    if (!d->buf) return false;
    if (!chain_read(fs, clus, 0, d->buf, d->len)) { kfree(d->buf); d->buf = NULL; return false; }
    return true;
}

static bool dir_store(fatfs_t *fs, dirbuf_t *d) {
    if (d->clus == 0 && fs->type != 32) return blk_write(fs->dev, fs->root_off, d->buf, d->len);
    return chain_write(fs, d->clus, d->buf, d->len);
}

static bool dir_grow(fatfs_t *fs, dirbuf_t *d) {
    if (d->clus == 0 && fs->type != 32) return false;   /* a raiz do FAT12/16 tem tamanho fixo */
    u32 c = alloc_cluster(fs);
    if (!c) return false;
    u8 *nb = kmalloc(d->len + fs->clus);
    if (!nb) { fat_set(fs, c, 0); return false; }
    memcpy(nb, d->buf, d->len);
    memset(nb + d->len, 0, fs->clus);
    u32 last = d->clus;
    while (!is_eoc(fat_get(fs, last))) last = fat_get(fs, last);
    fat_set(fs, last, c);
    kfree(d->buf);
    d->buf = nb;
    d->len += fs->clus;
    return true;
}

/* ---- texto */
static int utf8_next(const char **s) {
    const u8 *p = (const u8 *)*s;
    int c = *p++;
    if (c >= 0xF0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        c = (c & 7) << 18 | (p[0] & 63) << 12 | (p[1] & 63) << 6 | (p[2] & 63); p += 3;
    } else if (c >= 0xE0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80) {
        c = (c & 15) << 12 | (p[0] & 63) << 6 | (p[1] & 63); p += 2;
    } else if (c >= 0xC0 && (p[0] & 0xC0) == 0x80) {
        c = (c & 31) << 6 | (p[0] & 63); p += 1;
    } else if (c >= 0x80) c = '?';
    *s = (const char *)p;
    return c;
}

static int to_utf16(const char *s, u16 *out, int max) {
    int n = 0;
    while (*s && n < max) {
        int c = utf8_next(&s);
        if (c >= 0x10000) {
            if (n + 2 > max) break;
            c -= 0x10000;
            out[n++] = (u16)(0xD800 | (c >> 10));
            out[n++] = (u16)(0xDC00 | (c & 0x3FF));
        } else out[n++] = (u16)c;
    }
    return n;
}

static void from_utf16(const u16 *w, int n, char *dst, usize cap) {
    usize o = 0;
    for (int i = 0; i < n && w[i] && o + 4 < cap; i++) {
        u32 c = w[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < n && w[i + 1] >= 0xDC00 && w[i + 1] < 0xE000) {
            c = 0x10000 + ((c - 0xD800) << 10) + (w[i + 1] - 0xDC00); i++;
        }
        if (c < 0x80) dst[o++] = (char)c;
        else if (c < 0x800) { dst[o++] = (char)(0xC0 | c >> 6); dst[o++] = (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { dst[o++] = (char)(0xE0 | c >> 12); dst[o++] = (char)(0x80 | ((c >> 6) & 63)); dst[o++] = (char)(0x80 | (c & 63)); }
        else { dst[o++] = (char)(0xF0 | c >> 18); dst[o++] = (char)(0x80 | ((c >> 12) & 63)); dst[o++] = (char)(0x80 | ((c >> 6) & 63)); dst[o++] = (char)(0x80 | (c & 63)); }
    }
    dst[o] = 0;
}

static u8 sfn_checksum(const u8 *n) {
    u8 s = 0;
    for (int i = 0; i < 11; i++) s = (u8)(((s & 1) << 7) + (s >> 1) + n[i]);
    return s;
}

static const u8 LFN_POS[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};

static i64 fat_time(const u8 *e) {
    u16 t = le16(e + 22), d = le16(e + 24);
    int y = 1980 + (d >> 9), m = (d >> 5) & 15, dd = d & 31;
    if (m < 1 || m > 12 || dd < 1) return 0;
    /* dias desde 1970 (algoritmo civil) */
    int yy = y - (m <= 2), era = yy / 400, yoe = yy - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + dd - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    i64 days = (i64)era * 146097 + doe - 719468;
    return days * 86400 + (t >> 11) * 3600 + ((t >> 5) & 63) * 60 + (t & 31) * 2;
}

static void stamp(u8 *e, bool created) {
    datetime_t dt;
    rtc_now(&dt);
    u16 t = (u16)(dt.hour << 11 | dt.minute << 5 | dt.second / 2);
    u16 d = (u16)((MAX(dt.year, 1980) - 1980) << 9 | dt.month << 5 | dt.day);
    e[22] = (u8)t; e[23] = (u8)(t >> 8); e[24] = (u8)d; e[25] = (u8)(d >> 8);
    e[18] = (u8)d; e[19] = (u8)(d >> 8);
    if (created) { e[14] = (u8)t; e[15] = (u8)(t >> 8); e[16] = (u8)d; e[17] = (u8)(d >> 8); }
}

/* percorre as entradas; devolve a próxima válida a partir de *pos */
static bool dir_next(dirbuf_t *d, u32 *pos, fent_t *f) {
    u16 lfn[260];
    int lfn_start = -1, lfn_cnt = 0, lfn_seen = 0;
    u8 lfn_ck = 0;
    u32 n = d->len / 32;
    for (u32 i = *pos; i < n; i++) {
        u8 *e = d->buf + i * 32;
        if (e[0] == 0) return false;
        if (e[0] == 0xE5) { lfn_start = -1; continue; }
        if (e[11] == ATTR_LFN) {
            int seq = e[0] & 0x1F;
            if (e[0] & 0x40) { lfn_start = (int)i; lfn_cnt = seq; lfn_seen = 0; lfn_ck = e[13]; memset(lfn, 0, sizeof lfn); }
            if (lfn_start < 0 || seq < 1 || seq > 20 || e[13] != lfn_ck) { lfn_start = -1; continue; }
            for (int k = 0; k < 13; k++) lfn[(seq - 1) * 13 + k] = (u16)le16(e + LFN_POS[k]);
            lfn_seen++;
            continue;
        }
        if (e[11] & ATTR_LABEL) { lfn_start = -1; continue; }
        f->attr = e[11];
        f->clus = (u32)le16(e + 20) << 16 | le16(e + 26);
        f->size = le32(e + 28);
        f->mtime = fat_time(e);
        f->sfn = (int)i;
        if (lfn_start >= 0 && lfn_seen == lfn_cnt && sfn_checksum(e) == lfn_ck) {
            from_utf16(lfn, lfn_cnt * 13, f->name, sizeof f->name);
            f->first = lfn_start;
        } else {
            int o = 0;
            bool lb = e[12] & 0x08, le = e[12] & 0x10;
            for (int k = 0; k < 8 && e[k] != ' '; k++) f->name[o++] = (char)(lb ? tolower(e[k] == 0x05 ? 0xE5 : e[k]) : (e[k] == 0x05 ? 0xE5 : e[k]));
            if (e[8] != ' ') {
                f->name[o++] = '.';
                for (int k = 8; k < 11 && e[k] != ' '; k++) f->name[o++] = (char)(le ? tolower(e[k]) : e[k]);
            }
            f->name[o] = 0;
            f->first = (int)i;
        }
        *pos = i + 1;
        return true;
    }
    return false;
}

static bool dir_find(dirbuf_t *d, const char *name, fent_t *f) {
    u32 pos = 0;
    while (dir_next(d, &pos, f))
        if (!strcasecmp(f->name, name)) return true;
    return false;
}

static bool sfn_char_ok(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr("!#$%&'()-@^_`{}~", c);
}

static bool sfn_exists(dirbuf_t *d, const u8 *sfn) {
    for (u32 i = 0; i < d->len / 32; i++) {
        u8 *e = d->buf + i * 32;
        if (e[0] == 0) break;
        if (e[0] != 0xE5 && e[11] != ATTR_LFN && !memcmp(e, sfn, 11)) return true;
    }
    return false;
}

/* gera o nome 8.3; devolve true se o nome longo for necessário */
static bool make_sfn(dirbuf_t *d, const char *name, u8 *sfn) {
    memset(sfn, ' ', 11);
    const char *dot = strrchr(name, '.');
    if (dot == name) dot = NULL;
    bool lossy = false;
    int bl = 0, el = 0;
    for (const char *p = name; *p && (!dot || p < dot); p++) {
        int c = (u8)*p;
        if (c == ' ' || c == '.') { lossy = true; continue; }
        if (c >= 0x80) { lossy = true; c = '_'; while ((((u8)p[1]) & 0xC0) == 0x80) p++; }
        else if (c >= 'a' && c <= 'z') { lossy = true; c -= 32; }
        else if (!sfn_char_ok(c)) { lossy = true; c = '_'; }
        if (bl < 8) sfn[bl++] = (u8)c; else lossy = true;
    }
    if (dot) for (const char *p = dot + 1; *p; p++) {
        int c = (u8)*p;
        if (c == ' ' || c == '.') { lossy = true; continue; }
        if (c >= 0x80) { lossy = true; c = '_'; while ((((u8)p[1]) & 0xC0) == 0x80) p++; }
        else if (c >= 'a' && c <= 'z') { lossy = true; c -= 32; }
        else if (!sfn_char_ok(c)) { lossy = true; c = '_'; }
        if (el < 3) sfn[8 + el++] = (u8)c; else lossy = true;
    }
    if (!bl) { sfn[0] = '_'; bl = 1; lossy = true; }
    if (sfn[0] == 0xE5) sfn[0] = 0x05;
    if (!lossy && !sfn_exists(d, sfn)) return false;
    for (u32 n = 1; n < 1000000; n++) {
        char tail[10];
        snprintf(tail, sizeof tail, "~%u", n);
        int tl = (int)strlen(tail), keep = MIN(bl, 8 - tl);
        u8 t[11];
        memcpy(t, sfn, 11);
        for (int k = keep; k < 8; k++) t[k] = ' ';
        memcpy(t + keep, tail, tl);
        if (!sfn_exists(d, t)) { memcpy(sfn, t, 11); return true; }
    }
    return true;
}

static bool valid_name(const char *n) {
    if (!*n || !strcmp(n, ".") || !strcmp(n, "..") || strlen(n) > 255) return false;
    for (const char *p = n; *p; p++) if ((u8)*p < 0x20 || strchr("\\/:*?\"<>|", *p)) return false;
    return true;
}

/* insere uma entrada (LFN + 8.3); devolve o índice da entrada 8.3 ou -1 */
static int dir_add(fatfs_t *fs, dirbuf_t *d, const char *name, u8 attr, u32 clus, u32 size) {
    u8 sfn[11];
    bool need_lfn = make_sfn(d, name, sfn);
    u16 w[260];
    int wn = need_lfn ? to_utf16(name, w, 255) : 0;
    int nl = need_lfn ? (wn + 12) / 13 : 0, need = nl + 1;
    int start = -1;
    for (;;) {
        int run = 0;
        for (u32 i = 0; i < d->len / 32; i++) {
            u8 c = d->buf[i * 32];
            if (c == 0 || c == 0xE5) { if (!run++) start = (int)i; if (run == need) break; }
            else run = 0;
        }
        if (run >= need) break;
        if (!dir_grow(fs, d)) return -1;
    }
    u8 ck = sfn_checksum(sfn);
    for (int k = 0; k < nl; k++) {
        u8 *e = d->buf + (start + k) * 32;
        int seq = nl - k;
        memset(e, 0, 32);
        e[0] = (u8)(seq | (k == 0 ? 0x40 : 0));
        e[11] = ATTR_LFN;
        e[13] = ck;
        for (int j = 0; j < 13; j++) {
            int idx = (seq - 1) * 13 + j;
            u16 ch = idx < wn ? w[idx] : idx == wn ? 0 : 0xFFFF;
            e[LFN_POS[j]] = (u8)ch; e[LFN_POS[j] + 1] = (u8)(ch >> 8);
        }
    }
    u8 *e = d->buf + (start + nl) * 32;
    memset(e, 0, 32);
    memcpy(e, sfn, 11);
    e[11] = attr;
    e[20] = (u8)(clus >> 16); e[21] = (u8)(clus >> 24);
    e[26] = (u8)clus; e[27] = (u8)(clus >> 8);
    e[28] = (u8)size; e[29] = (u8)(size >> 8); e[30] = (u8)(size >> 16); e[31] = (u8)(size >> 24);
    stamp(e, true);
    return start + nl;
}

static void dir_del(dirbuf_t *d, const fent_t *f) {
    for (int i = f->first; i <= f->sfn; i++) d->buf[i * 32] = 0xE5;
}

/* ================================================================ caminhos */
/* separa "/a/b/c" em pai "/a/b" e nome "c" */
static bool split(const char *path, char *parent, usize pcap, const char **leaf) {
    const char *s = strrchr(path, '/');
    if (!s || !s[1]) return false;
    usize n = (usize)(s - path);
    if (n + 1 > pcap) return false;
    memcpy(parent, path, n);
    parent[n] = 0;
    *leaf = s + 1;
    return true;
}

/* resolve um caminho; a raiz devolve attr DIR e clus = raiz */
static bool resolve(fatfs_t *fs, const char *path, fent_t *f) {
    memset(f, 0, sizeof *f);
    f->attr = ATTR_DIR;
    f->clus = fs->root;
    const char *p = path;
    char part[256];
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        int n = 0;
        while (*p && *p != '/') { if (n < 255) part[n++] = *p; p++; }
        part[n] = 0;
        if (!(f->attr & ATTR_DIR)) return false;
        dirbuf_t d;
        if (!dir_load(fs, f->clus ? f->clus : fs->root, &d)) return false;
        bool ok = dir_find(&d, part, f);
        kfree(d.buf);
        if (!ok) return false;
        if ((f->attr & ATTR_DIR) && f->clus == 0) f->clus = fs->root; /* ".." para a raiz */
    }
    return true;
}

static bool open_parent(fatfs_t *fs, const char *path, dirbuf_t *d, const char **leaf) {
    char parent[1024];
    fent_t pf;
    if (!split(path, parent, sizeof parent, leaf) || !valid_name(*leaf)) return false;
    if (!resolve(fs, parent, &pf) || !(pf.attr & ATTR_DIR)) return false;
    return dir_load(fs, pf.clus, d);
}

/* ================================================================ API */
bool fat_sync(fatfs_t *fs) {
    if (!fs->rw) return true;
    bool ok = true;
    for (int s = 0; s < FCACHE; s++) {
        if (!fs->cache[s].valid || !fs->cache[s].dirty) continue;
        u32 off = fs->cache[s].blk * FBLK, n = MIN(FBLK, fs->fat_bytes - off);
        for (u32 f = 0; f < fs->nfats; f++)
            ok &= blk_write(fs->dev, fs->fat_off + (u64)f * fs->fat_bytes + off, fs->cache[s].data, n);
        fs->cache[s].dirty = false;
    }
    if (fs->fsinfo_sec) {                 /* contagem livre = desconhecida, dica do próximo livre */
        u8 *s = kmalloc(fs->bps);
        if (s && blk_read(fs->dev, (u64)fs->fsinfo_sec * fs->bps, s, fs->bps) &&
            le32(s) == 0x41615252 && le32(s + 484) == 0x61417272) {
            memset(s + 488, 0xFF, 4);
            s[492] = (u8)fs->hint; s[493] = (u8)(fs->hint >> 8); s[494] = (u8)(fs->hint >> 16); s[495] = (u8)(fs->hint >> 24);
            ok &= blk_write(fs->dev, (u64)fs->fsinfo_sec * fs->bps, s, fs->bps);
        }
        kfree(s);
    }
    return blk_flush(fs->dev) && ok;
}

fatfs_t *fat_mount(blkdev_t *dev) {
    u8 *b = kmalloc(512);
    if (!b || dev->size < 512 * 64 || !blk_read(dev, 0, b, 512)) { kfree(b); return NULL; }
    u32 bps = le16(b + 11), spc = b[13], res = le16(b + 14), nf = b[16];
    u32 rootents = le16(b + 17), fs16 = le16(b + 22), fs32 = le32(b + 36);
    u32 tot = le16(b + 19) ? le16(b + 19) : le32(b + 32);
    u32 fatsz = fs16 ? fs16 : fs32;
    bool ok = b[510] == 0x55 && b[511] == 0xAA && (bps == 512 || bps == 1024 || bps == 2048 || bps == 4096) &&
              spc && !(spc & (spc - 1)) && res && nf >= 1 && nf <= 4 && fatsz && (b[0] == 0xEB || b[0] == 0xE9);
    if (!ok) { kfree(b); return NULL; }
    u32 root_secs = (rootents * 32 + bps - 1) / bps;      /* só depois de validar bps */
    if (tot <= res + nf * fatsz + root_secs) { kfree(b); return NULL; }
    u32 nclus = (tot - res - nf * fatsz - root_secs) / spc;
    /* FAT32 se o BPB for de FAT32 (como o Linux: mesmo com poucos clusters); senão FAT12/16 pela contagem */
    int type = (fs16 == 0 && rootents == 0) ? 32 : nclus < 4085 ? 12 : 16;
    if ((type != 32 && !rootents) || (u64)tot * bps > dev->size + bps) { kfree(b); return NULL; }
    fatfs_t *fs = kcalloc(1, sizeof *fs);
    if (!fs) { kfree(b); return NULL; }
    fs->dev = dev;
    fs->type = type;
    fs->bps = bps; fs->spc = spc; fs->clus = bps * spc;
    fs->fat_off = (u64)res * bps;
    fs->fat_bytes = fatsz * bps;
    fs->nfats = nf;
    fs->root_off = fs->fat_off + (u64)nf * fs->fat_bytes;
    fs->root_bytes = root_secs * bps;
    fs->data_off = fs->root_off + fs->root_bytes;
    u32 max_entries = type == 32 ? fs->fat_bytes / 4 : type == 16 ? fs->fat_bytes / 2 : fs->fat_bytes * 2 / 3;
    fs->nclus = MIN(nclus, max_entries - 2);
    fs->hint = 2;
    fs->rw = blk_writable(dev);
    if (type == 32) {
        fs->root = le32(b + 44);
        fs->fsinfo_sec = le16(b + 48);
        if (fs->fsinfo_sec == 0 || fs->fsinfo_sec == 0xFFFF || fs->fsinfo_sec >= res) fs->fsinfo_sec = 0;
        memcpy(fs->label, b + 71, 11);
    } else {
        fs->root = 0;
        if (b[38] == 0x29) memcpy(fs->label, b + 43, 11);
    }
    for (int i = 10; i >= 0 && (fs->label[i] == ' ' || !fs->label[i]); i--) fs->label[i] = 0;
    if (!strcmp(fs->label, "NO NAME")) fs->label[0] = 0;
    kfree(b);
    if (type == 32 && (fs->root < 2 || fs->root > fs->nclus + 1)) { kfree(fs); return NULL; }
    if (fs->fsinfo_sec) {
        u8 *s = kmalloc(bps);
        if (s && blk_read(dev, (u64)fs->fsinfo_sec * bps, s, bps) && le32(s) == 0x41615252) {
            u32 h = le32(s + 492);
            if (h >= 2 && h <= fs->nclus + 1) fs->hint = h;
        }
        kfree(s);
    }
    return fs;
}

void fat_unmount(fatfs_t *fs) {
    if (!fs) return;
    fat_sync(fs);
    for (int s = 0; s < FCACHE; s++) kfree(fs->cache[s].data);
    kfree(fs);
}

const char *fat_label(fatfs_t *fs) { return fs->label; }
int fat_type(fatfs_t *fs) { return fs->type; }

/* ================================================================ formatação (FAT32) */
bool fat_format(blkdev_t *dev, const char *label) {
    const u32 bps = 512, res = 32, nf = 2;
    u64 tot64 = dev->size / bps;
    if (!blk_writable(dev) || tot64 < 66600 || tot64 > 0xFFFFFFFFull) return false;  /* ~33 MB .. 2 TB */
    u32 tot = (u32)tot64;
    u32 spc = tot64 <= 532480 ? 1 : tot64 <= 16777216 ? 8 : tot64 <= 33554432 ? 16 : tot64 <= 67108864 ? 32 : 64;
    /* tamanho da FAT: resolve fatsz = ceil((nclus + 2) * 4 / bps) com nclus = (tot - res - nf*fatsz) / spc */
    u32 fatsz = 1;
    for (int it = 0; it < 32; it++) {
        u32 nclus = (tot - res - nf * fatsz) / spc;
        u32 need = (u32)(((u64)nclus + 2) * 4 + bps - 1) / bps;
        if (need == fatsz) break;
        fatsz = need;
    }
    u32 nclus = (tot - res - nf * fatsz) / spc;
    if (nclus < 65525) spc = 1, nclus = (tot - res - nf * fatsz) / spc;
    if (nclus < 65525) return false;

    u8 *bs = kcalloc(1, bps);
    u8 *z = kcalloc(1, 65536);
    if (!bs || !z) { kfree(bs); kfree(z); return false; }
    /* setor de boot */
    bs[0] = 0xEB; bs[1] = 0x58; bs[2] = 0x90;
    memcpy(bs + 3, "MSWIN4.1", 8);
    bs[11] = (u8)bps; bs[12] = (u8)(bps >> 8);
    bs[13] = (u8)spc;
    bs[14] = (u8)res; bs[15] = 0;
    bs[16] = (u8)nf;
    bs[21] = 0xF8;                                   /* mídia fixa */
    bs[24] = 63; bs[26] = 255;                       /* geometria nominal */
    bs[32] = (u8)tot; bs[33] = (u8)(tot >> 8); bs[34] = (u8)(tot >> 16); bs[35] = (u8)(tot >> 24);
    bs[36] = (u8)fatsz; bs[37] = (u8)(fatsz >> 8); bs[38] = (u8)(fatsz >> 16); bs[39] = (u8)(fatsz >> 24);
    bs[44] = 2;                                      /* raiz no cluster 2 */
    bs[48] = 1;                                      /* FSInfo */
    bs[50] = 6;                                      /* cópia do setor de boot */
    bs[64] = 0x80;
    bs[66] = 0x29;
#ifdef MOS_HOST_TEST
    u32 id = rand32();
#else
    u32 id;
    crypto_random(&id, sizeof id);                   /* número de série do volume: CSPRNG */
#endif
    bs[67] = (u8)id; bs[68] = (u8)(id >> 8); bs[69] = (u8)(id >> 16); bs[70] = (u8)(id >> 24);
    char lab[11];
    memset(lab, ' ', 11);
    for (int i = 0; label && label[i] && i < 11; i++) lab[i] = (char)toupper((u8)label[i]);
    memcpy(bs + 71, lab, 11);
    memcpy(bs + 82, "FAT32   ", 8);
    bs[510] = 0x55; bs[511] = 0xAA;
    /* FSInfo */
    u8 *fi = kcalloc(1, bps);
    bool ok = fi != NULL;
    if (ok) {
        fi[0] = 0x52; fi[1] = 0x52; fi[2] = 0x61; fi[3] = 0x41;
        fi[484] = 0x72; fi[485] = 0x72; fi[486] = 0x41; fi[487] = 0x61;
        u32 freec = nclus - 1;
        fi[488] = (u8)freec; fi[489] = (u8)(freec >> 8); fi[490] = (u8)(freec >> 16); fi[491] = (u8)(freec >> 24);
        fi[492] = 3;
        fi[510] = 0x55; fi[511] = 0xAA;
    }
    /* zera área reservada, FATs e o cluster da raiz */
    u64 end = ((u64)res + (u64)nf * fatsz + spc) * bps;
    for (u64 off = 0; ok && off < end; off += 65536) ok = blk_write(dev, off, z, MIN(65536, end - off));
    ok = ok && blk_write(dev, 0, bs, bps) && blk_write(dev, 6 * bps, bs, bps) &&
         blk_write(dev, bps, fi, bps) && blk_write(dev, 7 * bps, fi, bps);
    /* FAT[0..2]: mídia, EOC, raiz */
    static const u8 f0[12] = {0xF8, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0x0F};
    for (u32 f = 0; ok && f < nf; f++) ok = blk_write(dev, ((u64)res + (u64)f * fatsz) * bps, f0, sizeof f0);
    if (ok && label && *label) {                     /* rótulo também como entrada na raiz */
        u8 e[32] = {0};
        memcpy(e, lab, 11);
        e[11] = ATTR_LABEL;
        ok = blk_write(dev, ((u64)res + (u64)nf * fatsz) * bps, e, 32);
    }
    ok = ok && blk_flush(dev);
    kfree(bs); kfree(z); kfree(fi);
    kprintf("fat: formatado FAT32 \"%s\": %u clusters de %u bytes%s\n", label ? label : "", nclus, spc * bps, ok ? "" : " (ERRO)");
    return ok;
}

bool fat_stat(fatfs_t *fs, const char *path, bool *is_dir, u64 *size) {
    fent_t f;
    if (!resolve(fs, path, &f)) return false;
    if (is_dir) *is_dir = f.attr & ATTR_DIR;
    if (size) *size = f.size;
    return true;
}

int fat_list(fatfs_t *fs, const char *path, fat_list_cb cb, void *ctx) {
    fent_t f;
    if (!resolve(fs, path, &f) || !(f.attr & ATTR_DIR)) return -1;
    dirbuf_t d;
    if (!dir_load(fs, f.clus, &d)) return -1;
    u32 pos = 0;
    int n = 0;
    fent_t *c = kmalloc(sizeof *c);
    while (c && dir_next(&d, &pos, c)) {
        if (!strcmp(c->name, ".") || !strcmp(c->name, "..")) continue;
        cb(ctx, c->name, (c->attr & ATTR_DIR) != 0, c->size);
        n++;
    }
    kfree(c);
    kfree(d.buf);
    return n;
}

u8 *fat_read_file(fatfs_t *fs, const char *path, u64 *len) {
    fent_t f;
    if (!resolve(fs, path, &f) || (f.attr & ATTR_DIR)) return NULL;
    u8 *buf = kmalloc((usize)f.size + 1);
    if (!buf) return NULL;
    if (f.size && !chain_read(fs, f.clus, 0, buf, f.size)) { kfree(buf); return NULL; }
    buf[f.size] = 0;
    *len = f.size;
    return buf;
}

bool fat_write_file(fatfs_t *fs, const char *path, const void *data, u64 len) {
    if (!fs->rw || len > 0xFFFFFFFFull) return false;
    dirbuf_t d;
    const char *leaf;
    if (!open_parent(fs, path, &d, &leaf)) return false;
    u32 n = (u32)((len + fs->clus - 1) / fs->clus);
    u32 first = n ? alloc_chain(fs, n) : 0;
    if (n && !first) {
        /* disco cheio: substituir um arquivo grande pede o dobro do espaço. Solta a cadeia antiga antes
         * (perde a garantia de "a versão velha sobrevive a uma falha", mas o arquivo cabe) */
        fent_t *o = kmalloc(sizeof *o);
        if (o && dir_find(&d, leaf, o) && !(o->attr & ATTR_DIR) && o->clus) {
            u8 *e = d.buf + o->sfn * 32;
            e[20] = e[21] = e[26] = e[27] = 0;
            e[28] = e[29] = e[30] = e[31] = 0;
            if (dir_store(fs, &d)) { free_chain(fs, o->clus); first = alloc_chain(fs, n); }
        }
        kfree(o);
    }
    bool ok = !n || (first && chain_write(fs, first, data, len));
    fent_t *f = ok ? kmalloc(sizeof *f) : NULL;
    if (ok && f) {
        if (dir_find(&d, leaf, f)) {
            if (f->attr & ATTR_DIR) ok = false;
            else {
                u8 *e = d.buf + f->sfn * 32;
                e[20] = (u8)(first >> 16); e[21] = (u8)(first >> 24);
                e[26] = (u8)first; e[27] = (u8)(first >> 8);
                e[28] = (u8)len; e[29] = (u8)(len >> 8); e[30] = (u8)(len >> 16); e[31] = (u8)(len >> 24);
                e[11] |= ATTR_ARCHIVE;
                stamp(e, false);
                ok = dir_store(fs, &d);
                if (ok && f->clus) free_chain(fs, f->clus);   /* a cadeia antiga só sai depois */
            }
        } else {
            ok = dir_add(fs, &d, leaf, ATTR_ARCHIVE, first, (u32)len) >= 0 && dir_store(fs, &d);
        }
    } else ok = false;
    if (!ok && first) free_chain(fs, first);
    kfree(f);
    kfree(d.buf);
    return fat_sync(fs) && ok;
}

bool fat_mkdir(fatfs_t *fs, const char *path) {
    if (!fs->rw) return false;
    dirbuf_t d;
    const char *leaf;
    if (!open_parent(fs, path, &d, &leaf)) return false;
    fent_t *f = kmalloc(sizeof *f);
    bool ok = false;
    if (f && dir_find(&d, leaf, f)) ok = (f->attr & ATTR_DIR) != 0;   /* já existe */
    else if (f) {
        u32 c = alloc_cluster(fs);
        u8 *z = c ? kcalloc(1, fs->clus) : NULL;
        if (z) {
            memcpy(z, ".          ", 11); z[11] = ATTR_DIR;
            z[20] = (u8)(c >> 16); z[21] = (u8)(c >> 24); z[26] = (u8)c; z[27] = (u8)(c >> 8);
            stamp(z, true);
            u32 pc = d.clus == fs->root ? 0 : d.clus;
            memcpy(z + 32, "..         ", 11); z[32 + 11] = ATTR_DIR;
            z[32 + 20] = (u8)(pc >> 16); z[32 + 21] = (u8)(pc >> 24); z[32 + 26] = (u8)pc; z[32 + 27] = (u8)(pc >> 8);
            stamp(z + 32, true);
            ok = blk_write(fs->dev, clus_off(fs, c), z, fs->clus) &&
                 dir_add(fs, &d, leaf, ATTR_DIR, c, 0) >= 0 && dir_store(fs, &d);
            kfree(z);
        }
        if (!ok && c) fat_set(fs, c, 0);
    }
    kfree(f);
    kfree(d.buf);
    return fat_sync(fs) && ok;
}

/* libera o conteúdo de um diretório (recursivo) */
static void free_dir_tree(fatfs_t *fs, u32 clus, int depth) {
    dirbuf_t d;
    if (depth > 32 || !dir_load(fs, clus, &d)) return;
    fent_t *f = kmalloc(sizeof *f);
    u32 pos = 0;
    while (f && dir_next(&d, &pos, f)) {
        if (!strcmp(f->name, ".") || !strcmp(f->name, "..") || !f->clus) continue;
        if (f->attr & ATTR_DIR) free_dir_tree(fs, f->clus, depth + 1);
        free_chain(fs, f->clus);
    }
    kfree(f);
    kfree(d.buf);
}

bool fat_remove(fatfs_t *fs, const char *path) {
    if (!fs->rw) return false;
    dirbuf_t d;
    const char *leaf;
    if (!open_parent(fs, path, &d, &leaf)) return false;
    fent_t *f = kmalloc(sizeof *f);
    bool ok = f && dir_find(&d, leaf, f);
    if (ok) {
        dir_del(&d, f);
        ok = dir_store(fs, &d);
        if (ok && f->clus) {
            if (f->attr & ATTR_DIR) free_dir_tree(fs, f->clus, 0);
            free_chain(fs, f->clus);
        }
    }
    kfree(f);
    kfree(d.buf);
    return fat_sync(fs) && ok;
}

bool fat_rename(fatfs_t *fs, const char *from, const char *to) {
    if (!fs->rw) return false;
    char pfrom[1024], pto[1024];
    const char *lfrom, *lto;
    if (!split(from, pfrom, sizeof pfrom, &lfrom) || !split(to, pto, sizeof pto, &lto) || !valid_name(lto)) return false;
    fent_t *f = kmalloc(sizeof *f), *g = kmalloc(sizeof *g);
    dirbuf_t ds = {0}, dd = {0};
    bool ok = f && g && resolve(fs, pfrom, g) && (g->attr & ATTR_DIR) && dir_load(fs, g->clus, &ds) && dir_find(&ds, lfrom, f);
    u32 src_parent = ds.clus;
    if (ok) ok = resolve(fs, pto, g) && (g->attr & ATTR_DIR);
    /* pasta não pode ir para dentro dela mesma: sobe pelos ".." do destino até a raiz */
    if (ok && (f->attr & ATTR_DIR) && f->clus) {
        u32 c = g->clus;
        for (int guard = 0; ok && c && c != fs->root && guard < 4096; guard++) {
            if (c == f->clus) { ok = false; break; }
            dirbuf_t up;
            if (!dir_load(fs, c, &up)) break;
            u32 pc = up.len >= 64 && !memcmp(up.buf + 32, "..         ", 11) ? (u32)le16(up.buf + 52) << 16 | le16(up.buf + 58) : 0;
            kfree(up.buf);
            c = pc;
        }
    }
    if (ok) {
        bool same = g->clus == src_parent;
        dirbuf_t *dst = same ? &ds : &dd;
        if (!same) ok = dir_load(fs, g->clus, &dd);
        fent_t *x = kmalloc(sizeof *x);
        /* destino ocupado por outra entrada (diferente só na caixa é a mesma entrada) */
        if (ok && x && dir_find(dst, lto, x) && !(same && x->sfn == f->sfn)) ok = false;
        kfree(x);
        if (ok) {
            u8 save[32];
            memcpy(save, ds.buf + f->sfn * 32, 32);
            dir_del(&ds, f);                                    /* no mesmo buffer se same */
            int idx = dir_add(fs, dst, lto, save[11], f->clus, f->size);
            ok = idx >= 0;
            if (ok) {                                           /* preserva datas originais */
                u8 *e = dst->buf + idx * 32;
                memcpy(e + 13, save + 13, 7);
                memcpy(e + 22, save + 22, 4);
                ok = dir_store(fs, dst) && (same || dir_store(fs, &ds));
            }
            if (ok && !same && (f->attr & ATTR_DIR) && f->clus) { /* atualiza ".." */
                dirbuf_t sub;
                if (dir_load(fs, f->clus, &sub)) {
                    u8 *e = sub.buf + 32;
                    if (!memcmp(e, "..         ", 11)) {
                        u32 pc = g->clus == fs->root ? 0 : g->clus;
                        e[20] = (u8)(pc >> 16); e[21] = (u8)(pc >> 24); e[26] = (u8)pc; e[27] = (u8)(pc >> 8);
                        dir_store(fs, &sub);
                    }
                    kfree(sub.buf);
                }
            }
        }
    }
    kfree(ds.buf); kfree(dd.buf); kfree(f); kfree(g);
    return fat_sync(fs) && ok;
}

/* ================================================================ dfs (volumes FAT em /Volumes) */
static fatfs_t *s_system;               /* volume do sistema: não aparece em /Volumes */
void fat_set_system(fatfs_t *fs) { s_system = fs; }
void fat_enable_rw(fatfs_t *fs) { fs->rw = blk_writable(fs->dev); }

static void root_of(blkdev_t *d, blkdev_t **root, u64 *base) {
    u64 b = 0;
    while (d->parent) { b += d->base; d = d->parent; }
    *root = d; *base = b;
}

static bool dfat_readdir(dmount_t *m, dnode_t *dir) {
    fatfs_t *fs = m->priv;
    dirbuf_t d;
    if (!dir_load(fs, (u32)dir->ino, &d)) return false;
    fent_t *f = kmalloc(sizeof *f);
    u32 pos = 0;
    while (f && dir_next(&d, &pos, f)) {
        if (!strcmp(f->name, ".") || !strcmp(f->name, "..")) continue;
        if ((f->attr & (ATTR_HIDDEN | ATTR_SYSTEM)) == (ATTR_HIDDEN | ATTR_SYSTEM)) continue;
        bool isdir = f->attr & ATTR_DIR;
        if (isdir && !f->clus) continue;
        dnode_t *c = dfs_new_child(dir, f->name, isdir ? DN_DIR : DN_FILE, isdir ? 0 : f->size);
        if (!c) break;
        c->ino = f->clus;
        c->mtime = f->mtime;
        if (f->attr & ATTR_HIDDEN) c->hidden = true;
    }
    kfree(f);
    kfree(d.buf);
    return true;
}

static i64 dfat_read(dmount_t *m, dnode_t *f, u64 off, void *buf, u64 len) {
    if (off >= f->size) return 0;
    len = MIN(len, f->size - off);
    return chain_read(m->priv, (u32)f->ino, off, buf, len) ? (i64)len : -1;
}

static void dfat_unmount(dmount_t *m) {
    fatfs_t *fs = m->priv;
    if (fs->rw && !fat_sync(fs)) kprintf("fat: \"%s\": erro ao gravar a FAT ao desmontar\n", m->volname);
    for (int s = 0; s < FCACHE; s++) kfree(fs->cache[s].data);
    kfree(fs);
    m->priv = NULL;
}

/* escrita em /Volumes: as funções de caminho acima já deixam o volume consistente a cada chamada */
static bool dfat_write(dmount_t *m, const char *p, const void *data, u64 len) { return fat_write_file(m->priv, p, data, len); }
static bool dfat_mkdir(dmount_t *m, const char *p) { return fat_mkdir(m->priv, p); }
static bool dfat_remove(dmount_t *m, const char *p) { return fat_remove(m->priv, p); }
static bool dfat_rename(dmount_t *m, const char *a, const char *b) { return fat_rename(m->priv, a, b); }
static bool dfat_sync(dmount_t *m) { return fat_sync(m->priv); }

static bool dfat_enable_write(dmount_t *m);
static const dfs_ops_t FAT_OPS = {"FAT", dfat_readdir, dfat_read, dfat_unmount, NULL, NULL,
                                  dfat_write, dfat_mkdir, dfat_remove, dfat_rename, dfat_sync, dfat_enable_write};

/* partição de sistema EFI (a do Windows/macOS/Linux de outro disco): fica só leitura para não
 * estragar o boot de outro sistema por engano. Critério: rótulo típico e pasta /EFI na raiz. */
static bool looks_like_esp(fatfs_t *fs) {
    bool dir = false;
    if (!fat_stat(fs, "/EFI", &dir, NULL) || !dir) return false;
    return !fs->label[0] || !strcasecmp(fs->label, "EFI") || !strcasecmp(fs->label, "SYSTEM") ||
           !strcasecmp(fs->label, "ESP") || fat_stat(fs, "/EFI/Microsoft", NULL, NULL) || fat_stat(fs, "/EFI/APPLE", NULL, NULL);
}

/* consentimento do usuário (por volume): FAT montado só leitura passa a gravar; partição EFI continua só leitura */
static bool dfat_enable_write(dmount_t *m) {
    fatfs_t *fs = m->priv;
    if (!fs || looks_like_esp(fs)) return false;
    fat_enable_rw(fs);
    m->writable = fs->rw;
    return fs->rw;
}

int fat_probe(blkdev_t *dev, const char *source, const char *fmt) {
    if (s_system) {
        blkdev_t *r1, *r2;
        u64 b1, b2;
        root_of(dev, &r1, &b1);
        root_of(s_system->dev, &r2, &b2);
        if (r1 == r2 && b1 == b2) return 0;
    }
    fatfs_t *fs = fat_mount(dev);
    if (!fs) return 0;
    /* fat_mount liga rw se o dispositivo aceita escrita (disco físico); imagens em RAM ficam só leitura */
    if (fs->rw && looks_like_esp(fs)) {
        kprintf("fat: \"%s\" parece uma partição EFI: montada só leitura\n", fs->label[0] ? fs->label : "sem nome");
        fs->rw = false;
    }
    dmount_t *m = dfs_attach(&FAT_OPS, dev, fs->label[0] ? fs->label : "Sem Título", source, fmt, fs, fs->root);
    if (!m) { fs->rw = false; dfat_unmount(&(dmount_t){.priv = fs}); return 0; }
    m->writable = fs->rw;
    if (fs->rw) kprintf("fat: \"%s\" (FAT%d) gravável\n", m->volname, fs->type);
    return 1;
}
