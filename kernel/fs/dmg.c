/*
 * mOS — leitor de imagens DMG (UDIF).
 *
 * Um .dmg é: [fork de dados com chunks comprimidos] [plist XML] [trailer "koly" de 512 bytes].
 * O plist traz um array "blkx" com tabelas "mish" (base64) que mapeiam setores do disco
 * virtual para chunks. Aqui tudo vira um blkdev_t; cada chunk só é descomprimido quando
 * algum setor dele é lido, com um pequeno cache LRU.
 *
 * Gravável (dmg_open_rw): imagem crua (sem "koly") ou UDIF só com chunks crus/zerados (UDRW), como o
 * hdiutil do macOS monta para leitura e escrita. A escrita vai direto para os bytes do chunk cru dentro
 * do arquivo (que fica numa cópia própria em RAM); trechos ZERO/IGNORE só aceitam zeros. No flush, os
 * CRC32 do UDIF (de cada tabela blkx, do fork de dados e o mestre) são recalculados no lugar e o arquivo
 * inteiro vai para o "sink" (a ponte do VFS grava no .dmg de origem).
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

typedef struct {            /* tabela blkx dentro do plist: onde está o base64 da mish, para regravar o CRC */
    u64 b64off, b64len;     /* posição do texto entre <data> e </data> no arquivo */
    u64 first, count;       /* setores cobertos */
    u32 crc_type;
    u32 crc;
} blkx_t;

typedef struct {
    const u8 *file;
    u64 fsize;
    chunk_t *ch;
    u32 nch;
    /* modo gravável */
    u8 *wfile;              /* = file, mas mutável (cópia do chamador) */
    bool rw, raw, dirty;
    blkx_t *bx;
    u32 nbx;
    u64 dfork, dforklen;    /* fork de dados (para o CRC do koly) */
    dmg_sink_t sink;
    void *sink_arg;
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

static u32 crc_tab[256];
static u32 crc32_upd(u32 c, const u8 *p, u64 n) {
    if (!crc_tab[1])
        for (u32 i = 0; i < 256; i++) { u32 x = i; for (int k = 0; k < 8; k++) x = x & 1 ? 0xEDB88320u ^ (x >> 1) : x >> 1; crc_tab[i] = x; }
    c = ~c;
    for (u64 i = 0; i < n; i++) c = crc_tab[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return ~c;
}

/* regrava no lugar o base64 de s[0..n): mesmos bytes decodificados => mesmo número de dígitos; espaços e
 * quebras de linha do plist ficam onde estavam */
static bool b64_rewrite(u8 *s, u64 n, const u8 *bin, u64 blen) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    u64 j = 0, need = (blen + 2) / 3 * 4;
    for (u64 i = 0; i < n; i++) {
        if (s[i] != '=' && b64v(s[i]) < 0) continue;
        if (j >= need) return false;
        u64 g = j / 4 * 3;
        u32 v = (u32)bin[g] << 16 | (g + 1 < blen ? (u32)bin[g + 1] << 8 : 0) | (g + 2 < blen ? bin[g + 2] : 0);
        int k = (int)(j % 4);
        u8 c = k == 0 ? T[v >> 18] : k == 1 ? T[(v >> 12) & 63] : k == 2 ? (g + 1 < blen ? T[(v >> 6) & 63] : '=') : (g + 2 < blen ? T[v & 63] : '=');
        s[i] = c;
        j++;
    }
    return j == need;
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
    kfree(g->bx);
    if (g->rw) { kfree(g->wfile); kfree(g->sink_arg); }
    kfree(g);
}

/* ------------------------------------------------------------ escrita (UDRW / cru) */
static bool all_zero(const u8 *p, u64 n) {
    for (u64 i = 0; i < n; i++) if (p[i]) return false;
    return true;
}

static bool rw_read(blkdev_t *d, u64 off, void *buf, u64 len) {
    dmg_t *g = d->ctx;
    if (g->raw) { memcpy(buf, g->file + off, len); return true; }
    return dmg_read(d, off, buf, len);
}

static bool rw_write(blkdev_t *d, u64 off, const void *buf, u64 len) {
    dmg_t *g = d->ctx;
    const u8 *in = buf;
    if (off > d->size || len > d->size - off) return false;
    if (g->raw) { memcpy(g->wfile + off, in, len); g->dirty = true; return true; }
    while (len) {
        i64 idx = chunk_find(g, off / 512);
        u64 n;
        if (idx < 0) {                                  /* buraco no mapa: só cabem zeros */
            n = MIN(len, 512 - off % 512);
            if (!all_zero(in, n)) return false;
        } else {
            chunk_t *c = &g->ch[idx];
            u64 cstart = c->sector * 512, in_off = off - cstart;
            n = MIN(len, (c->sector + c->count) * 512 - off);
            if (c->type == CH_RAW) {
                u64 k = in_off < c->len ? MIN(n, c->len - in_off) : 0;
                if (k) memcpy(g->wfile + c->off + in_off, in, k);
                if (k < n && !all_zero(in + k, n - k)) return false;
            } else if (!all_zero(in, n)) return false;  /* ZERO/IGNORE não têm bytes no arquivo */
        }
        g->dirty = true;
        in += n; off += n; len -= n;
    }
    return true;
}

/* CRC32 dos setores [first, first+count) do disco virtual */
static bool region_crc(blkdev_t *d, u64 first, u64 count, u32 *out) {
    u8 *b = kmalloc(65536);
    if (!b) return false;
    u32 c = 0;
    u64 off = first * 512, end = (first + count) * 512;
    bool ok = true;
    while (ok && off < end) {
        u64 n = MIN(65536, end - off);
        ok = dmg_read(d, off, b, n);
        c = crc32_upd(c, b, n);
        off += n;
    }
    kfree(b);
    *out = c;
    return ok;
}

static void wbe32(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }

static bool rw_flush(blkdev_t *d) {
    dmg_t *g = d->ctx;
    if (!g->dirty) return true;
    if (!g->raw) {
        u8 *k = g->wfile + g->fsize - 512;
        u8 *mish = NULL;
        u64 mcap = 0;
        for (u32 i = 0; i < g->nbx; i++) {
            blkx_t *b = &g->bx[i];
            if (b->crc_type != 2) continue;
            u32 crc;
            if (!region_crc(d, b->first, b->count, &crc)) { kfree(mish); return false; }
            if (crc == b->crc) continue;
            u64 need = b->b64len * 3 / 4 + 4;
            if (need > mcap) { kfree(mish); mish = kmalloc(need); mcap = mish ? need : 0; if (!mish) return false; }
            u64 ml = b64_decode(g->wfile + b->b64off, b->b64len, mish);
            if (ml < 76) { kfree(mish); return false; }
            wbe32(mish + 72, crc);
            if (!b64_rewrite(g->wfile + b->b64off, b->b64len, mish, ml)) { kfree(mish); return false; }
            b->crc = crc;
        }
        kfree(mish);
        if (be32(k + 80) == 2 && g->dforklen && g->dfork + g->dforklen <= g->fsize - 512)
            wbe32(k + 88, crc32_upd(0, g->wfile + g->dfork, g->dforklen));
        if (be32(k + 352) == 2) {                       /* mestre: CRC32 dos CRCs das tabelas, em big-endian */
            u32 mc = 0;
            for (u32 i = 0; i < g->nbx; i++) {
                if (g->bx[i].crc_type != 2) continue;
                u8 be[4];
                wbe32(be, g->bx[i].crc);
                mc = crc32_upd(mc, be, 4);
            }
            wbe32(k + 360, mc);
        }
    }
    if (g->sink && !g->sink(g->sink_arg, g->wfile, g->fsize)) return false;
    g->dirty = false;
    return true;
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

static blkdev_t *open_img(const u8 *data, u64 size, char *err, usize errlen, u8 *wdata, dmg_sink_t sink, void *arg) {
    if (size < 512) { snprintf(err, errlen, "arquivo muito pequeno"); return NULL; }
    const u8 *k = data + size - 512;
    if (be32(k) != 0x6B6F6C79) { /* "koly" ausente: imagem crua (UDRW/ISO/IMG) */
        if (!wdata) return blk_from_memory(data, size);
        dmg_t *g = kcalloc(1, sizeof(*g));
        blkdev_t *d = g ? kcalloc(1, sizeof(*d)) : NULL;
        if (!d) { kfree(g); return NULL; }
        g->file = g->wfile = wdata; g->fsize = size; g->rw = g->raw = true;
        g->sink = sink; g->sink_arg = arg;
        strncpy(g->fmt, "RAW", sizeof(g->fmt) - 1);
        d->size = size & ~511ull;
        d->read = rw_read; d->write = rw_write; d->flush = rw_flush; d->release = dmg_release;
        d->ctx = g;
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
    g->dfork = dfork;
    g->dforklen = be64(k + 32);
    u32 bxcap = 0;

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
        else if (wdata) {                                /* lembra onde a tabela está, para regravar o CRC */
            if (g->nbx == bxcap) {
                u32 nc = bxcap ? bxcap * 2 : 16;
                blkx_t *nb = krealloc(g->bx, nc * sizeof(blkx_t));
                if (!nb) { ok = false; break; }
                g->bx = nb; bxcap = nc;
            }
            blkx_t *bx = &g->bx[g->nbx++];
            bx->b64off = (u64)(ds - data); bx->b64len = (u64)(de - ds);
            bx->first = be64(mish + 8); bx->count = be64(mish + 16);
            bx->crc_type = be32(mish + 64); bx->crc = be32(mish + 72);
        }
        p = de + 7;
    }
    kfree(mish);
    if (!ok || !g->nch) {
        snprintf(err, errlen, "tabela blkx inválida");
        kfree(g->ch);
        kfree(g->bx);
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

    if (wdata) {                                         /* gravável só se todos os chunks forem crus ou zerados */
        bool can = true;
        for (u32 i = 0; i < g->nch && can; i++) can = g->ch[i].type == CH_RAW || g->ch[i].type == CH_ZERO || g->ch[i].type == CH_IGNORE;
        if (!can) {
            snprintf(err, errlen, "%s só pode ser lido", f);
            kfree(g->ch); kfree(g->bx); kfree(g);
            return NULL;
        }
        g->rw = true;
        g->wfile = wdata;
        g->sink = sink; g->sink_arg = arg;
    }
    u64 last = g->ch[g->nch - 1].sector + g->ch[g->nch - 1].count;
    if (sectors < last) sectors = last;
    blkdev_t *d = kcalloc(1, sizeof(*d));
    if (!d) { g->rw = false; dmg_release(&(blkdev_t){.ctx = g}); return NULL; }   /* rw = false: data ainda é do chamador */
    d->size = sectors * 512;
    d->read = dmg_read;
    if (g->rw) { d->write = rw_write; d->flush = rw_flush; }
    d->release = dmg_release;
    d->ctx = g;
    kprintf("dmg: %s%s, %u chunks, %llu MiB (raw %u, zlib %u, bzip2 %u, lzfse %u, adc %u, lzma %u)\n", f, g->rw ? " (leitura e escrita)" : "", g->nch,
            d->size >> 20, counts[0], counts[2], counts[3], counts[4], counts[1], counts[5]);
    return d;
}

blkdev_t *dmg_open(const u8 *data, u64 size, char *err, usize errlen) { return open_img(data, size, err, errlen, NULL, NULL, NULL); }

blkdev_t *dmg_open_rw(u8 *data, u64 size, dmg_sink_t sink, void *arg, char *err, usize errlen) {
    if (!data) return NULL;
    return open_img(data, size, err, errlen, data, sink, arg);
}

const char *dmg_format_name(blkdev_t *d) {
    if (d && d->release == dmg_release) return ((dmg_t *)d->ctx)->fmt;
    return "RAW";
}
