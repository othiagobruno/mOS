/*
 * mOS — leitor de imagens DMG (UDIF).
 *
 * Um .dmg é: [fork de dados com chunks comprimidos] [plist XML] [trailer "koly" de 512 bytes].
 * O plist traz um array "blkx" com tabelas "mish" (base64) que mapeiam setores do disco
 * virtual para chunks. Aqui tudo vira um blkdev_t; cada chunk só é descomprimido quando
 * algum setor dele é lido, com um pequeno cache LRU.
 */
#include "fs.h"

enum {
    CH_ZERO = 0, CH_RAW = 1, CH_IGNORE = 2, CH_ADC = 0x80000004, CH_ZLIB = 0x80000005,
    CH_BZIP2 = 0x80000006, CH_LZFSE = 0x80000007, CH_LZMA = 0x80000008,
    CH_COMMENT = 0x7FFFFFFE, CH_END = 0xFFFFFFFF
};

typedef struct {
    u64 sector, count;   /* setores do disco virtual */
    u64 off, len;        /* posição dos dados no arquivo .dmg */
    u32 type;
} chunk_t;

#define CACHE_SLOTS 6
#define CACHE_BYTES (8ull << 20) /* teto do cache de chunks descomprimidos por imagem */

typedef struct {
    const u8 *file;
    u64 fsize;
    chunk_t *ch;
    u32 nch;
    struct { i64 idx; u8 *buf; u64 cap; u64 stamp; } cache[CACHE_SLOTS];
    u64 clock;
    char fmt[8];
    bool warned_lzma;
} dmg_t;

/* ------------------------------------------------------------ base64 */
static int b64v(u8 c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static u64 b64_decode(const u8 *s, u64 n, u8 *out) {
    u32 acc = 0;
    int bits = 0;
    u64 o = 0;
    for (u64 i = 0; i < n; i++) {
        if (s[i] == '=') break;
        int v = b64v(s[i]);
        if (v < 0) continue;
        acc = acc << 6 | (u32)v;
        bits += 6;
        if (bits >= 8) { bits -= 8; out[o++] = (u8)(acc >> bits); }
    }
    return o;
}

static const u8 *find(const u8 *h, const u8 *end, const char *needle) {
    usize n = strlen(needle);
    for (const u8 *p = h; p + n <= end; p++)
        if (*p == (u8)needle[0] && !memcmp(p, needle, n)) return p;
    return NULL;
}

/* ------------------------------------------------------------ leitura */
static i64 chunk_find(dmg_t *g, u64 sector) {
    i64 lo = 0, hi = (i64)g->nch - 1;
    while (lo <= hi) {
        i64 mid = (lo + hi) / 2;
        chunk_t *c = &g->ch[mid];
        if (sector < c->sector) hi = mid - 1;
        else if (sector >= c->sector + c->count) lo = mid + 1;
        else return mid;
    }
    return -1;
}

static const u8 *chunk_data(dmg_t *g, i64 idx) {
    chunk_t *c = &g->ch[idx];
    for (int i = 0; i < CACHE_SLOTS; i++)
        if (g->cache[i].buf && g->cache[i].idx == idx) { g->cache[i].stamp = ++g->clock; return g->cache[i].buf; }
    int victim = 0;
    for (int i = 1; i < CACHE_SLOTS; i++)
        if (g->cache[i].stamp < g->cache[victim].stamp) victim = i;
    u64 bytes = c->count * 512;
    /* respeita o teto: libera os slots menos usados até caber */
    for (;;) {
        u64 total = bytes;
        int lru = -1;
        for (int i = 0; i < CACHE_SLOTS; i++) {
            if (i == victim || !g->cache[i].buf) continue;
            total += g->cache[i].cap;
            if (lru < 0 || g->cache[i].stamp < g->cache[lru].stamp) lru = i;
        }
        if (total <= CACHE_BYTES || lru < 0) break;
        kfree(g->cache[lru].buf);
        g->cache[lru].buf = NULL;
        g->cache[lru].cap = 0;
        g->cache[lru].idx = -1;
        g->cache[lru].stamp = 0;
    }
    if (g->cache[victim].cap < bytes || g->cache[victim].cap > bytes * 4) {
        kfree(g->cache[victim].buf);
        g->cache[victim].buf = NULL;
        g->cache[victim].cap = 0;
        g->cache[victim].buf = kmalloc(bytes);
        g->cache[victim].cap = g->cache[victim].buf ? bytes : 0;
        if (!g->cache[victim].buf) return NULL;
    }
    u8 *dst = g->cache[victim].buf;
    g->cache[victim].idx = -1;
    const u8 *src = g->file + c->off;
    i64 r = -1;
    switch (c->type) {
    case CH_ADC:   r = adc_decompress(src, c->len, dst, bytes); break;
    case CH_ZLIB:  r = zlib_decompress(src, c->len, dst, bytes); break;
    case CH_BZIP2: r = bzip2_decompress(src, c->len, dst, bytes); break;
    case CH_LZFSE: r = lzfse_decompress(src, c->len, dst, bytes); break;
    case CH_LZMA:  r = xz_decompress(src, c->len, dst, bytes); break;   /* ULMO: streams .xz/LZMA2 */
    default: return NULL;
    }
    if (r < 0) { kprintf("dmg: falha ao descomprimir chunk %lld (tipo %x)\n", idx, c->type); return NULL; }
    if ((u64)r < bytes) memset(dst + r, 0, bytes - r);
    g->cache[victim].idx = idx;
    g->cache[victim].stamp = ++g->clock;
    return dst;
}

static bool dmg_read(blkdev_t *d, u64 off, void *buf, u64 len) {
    dmg_t *g = d->ctx;
    u8 *out = buf;
    while (len) {
        u64 sector = off / 512;
        i64 idx = chunk_find(g, sector);
        if (idx < 0) { /* buraco no mapa: lê como zeros */
            u64 n = MIN(len, 512 - off % 512);
            memset(out, 0, n);
            out += n; off += n; len -= n;
            continue;
        }
        chunk_t *c = &g->ch[idx];
        u64 cstart = c->sector * 512, cend = (c->sector + c->count) * 512;
        u64 n = MIN(len, cend - off), in = off - cstart;
        switch (c->type) {
        case CH_ZERO: case CH_IGNORE: memset(out, 0, n); break;
        case CH_RAW:
            if (in >= c->len) memset(out, 0, n);
            else {
                u64 k = MIN(n, c->len - in);
                memcpy(out, g->file + c->off + in, k);
                if (k < n) memset(out + k, 0, n - k);
            }
            break;
        default: {
            const u8 *p = chunk_data(g, idx);
            if (!p) return false;
            memcpy(out, p + in, n);
        }
        }
        out += n; off += n; len -= n;
    }
    return true;
}

static void dmg_release(blkdev_t *d) {
    dmg_t *g = d->ctx;
    for (int i = 0; i < CACHE_SLOTS; i++) kfree(g->cache[i].buf);
    kfree(g->ch);
    kfree(g);
}

static int cmp_chunk(const chunk_t *a, const chunk_t *b) {
    return a->sector < b->sector ? -1 : a->sector > b->sector;
}

static void sort_chunks(chunk_t *c, u32 n) { /* insertion sort: os blkx já vêm quase ordenados */
    for (u32 i = 1; i < n; i++) {
        chunk_t t = c[i];
        u32 j = i;
        while (j > 0 && cmp_chunk(&c[j - 1], &t) > 0) { c[j] = c[j - 1]; j--; }
        c[j] = t;
    }
}

static bool add_mish(dmg_t *g, const u8 *m, u64 mlen, u64 data_fork_off, u32 *cap, u32 counts[8]) {
    if (mlen < 204 || be32(m) != 0x6D697368) return false; /* "mish" */
    u64 first = be64(m + 8), dataoff = be64(m + 24);
    u32 n = be32(m + 200);
    if (204 + (u64)n * 40 > mlen) return false;
    for (u32 i = 0; i < n; i++) {
        const u8 *e = m + 204 + i * 40;
        u32 type = be32(e);
        if (type == CH_COMMENT || type == CH_END) continue;
        u64 cnt = be64(e + 16);
        if (!cnt) continue;
        if (g->nch == *cap) {
            u32 nc = *cap ? *cap * 2 : 256;
            chunk_t *p = krealloc(g->ch, nc * sizeof(chunk_t));
            if (!p) return false;
            g->ch = p;
            *cap = nc;
        }
        chunk_t *c = &g->ch[g->nch];
        c->type = type;
        c->sector = first + be64(e + 8);
        c->count = cnt;
        c->off = data_fork_off + dataoff + be64(e + 24);
        c->len = be64(e + 32);
        if (type != CH_ZERO && type != CH_IGNORE && (c->off > g->fsize || c->len > g->fsize - c->off)) return false;
        if (type != CH_ZERO && type != CH_IGNORE && type != CH_RAW && c->count > 65536) return false;
        g->nch++;
        switch (type) {
        case CH_RAW: counts[0]++; break;
        case CH_ADC: counts[1]++; break;
        case CH_ZLIB: counts[2]++; break;
        case CH_BZIP2: counts[3]++; break;
        case CH_LZFSE: counts[4]++; break;
        case CH_LZMA: counts[5]++; break;
        }
    }
    return true;
}

blkdev_t *dmg_open(const u8 *data, u64 size, char *err, usize errlen) {
    if (size < 512) { snprintf(err, errlen, "arquivo muito pequeno"); return NULL; }
    const u8 *k = data + size - 512;
    if (be32(k) != 0x6B6F6C79) { /* "koly" ausente: imagem crua (UDRW/ISO/IMG) */
        blkdev_t *d = blk_from_memory(data, size);
        return d;
    }
    u64 dfork = be64(k + 24), xmloff = be64(k + 216), xmllen = be64(k + 224), sectors = be64(k + 492);
    if (be32(k + 60) > 1) { snprintf(err, errlen, "DMG segmentado não suportado"); return NULL; }
    if (!xmllen || xmloff > size || xmllen > size - xmloff) {
        snprintf(err, errlen, "DMG sem plist XML (formato antigo NDIF)");
        return NULL;
    }
    dmg_t *g = kcalloc(1, sizeof(*g));
    if (!g) return NULL;
    g->file = data;
    g->fsize = size;

    const u8 *x = data + xmloff, *xe = x + xmllen;
    const u8 *p = find(x, xe, "<key>blkx</key>");
    const u8 *arr_end = p ? find(p, xe, "</array>") : NULL;
    u32 cap = 0, counts[8] = {0};
    u8 *mish = NULL;
    u64 mcap = 0;
    bool ok = p && arr_end;
    while (ok) {
        const u8 *ds = find(p, arr_end, "<data>");
        if (!ds) break;
        ds += 6;
        const u8 *de = find(ds, arr_end, "</data>");
        if (!de) { ok = false; break; }
        u64 need = (u64)(de - ds) * 3 / 4 + 4;
        if (need > mcap) { kfree(mish); mish = kmalloc(need); mcap = mish ? need : 0; if (!mish) { ok = false; break; } }
        u64 ml = b64_decode(ds, (u64)(de - ds), mish);
        if (!add_mish(g, mish, ml, dfork, &cap, counts)) ok = false;
        p = de + 7;
    }
    kfree(mish);
    if (!ok || !g->nch) {
        snprintf(err, errlen, "tabela blkx inválida");
        kfree(g->ch);
        kfree(g);
        return NULL;
    }
    sort_chunks(g->ch, g->nch);
    /* nome do formato, no estilo do hdiutil */
    const char *f = "UDRW";
    if (counts[4]) f = "ULFO";
    else if (counts[5]) f = "ULMO";
    else if (counts[3]) f = "UDBZ";
    else if (counts[2]) f = "UDZO";
    else if (counts[1]) f = "UDCO";
    else if (counts[0]) f = "UDRO";
    strncpy(g->fmt, f, sizeof(g->fmt) - 1);

    u64 last = g->ch[g->nch - 1].sector + g->ch[g->nch - 1].count;
    if (sectors < last) sectors = last;
    blkdev_t *d = kcalloc(1, sizeof(*d));
    if (!d) { dmg_release(&(blkdev_t){.ctx = g}); return NULL; }
    d->size = sectors * 512;
    d->read = dmg_read;
    d->release = dmg_release;
    d->ctx = g;
    kprintf("dmg: %s, %u chunks, %llu MiB (raw %u, zlib %u, bzip2 %u, lzfse %u, adc %u, lzma %u)\n", f, g->nch,
            d->size >> 20, counts[0], counts[2], counts[3], counts[4], counts[1], counts[5]);
    return d;
}

const char *dmg_format_name(blkdev_t *d) {
    if (d && d->read == dmg_read) return ((dmg_t *)d->ctx)->fmt;
    return "RAW";
}
