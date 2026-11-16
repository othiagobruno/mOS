/*
 * mOS — imagens de disco UDIF (.dmg) para o /usr/bin/hdiutil: leitura (todos os tipos de bloco do kernel/fs/dmg.c,
 * com conferência dos CRC32) e escrita (UDRW, UDRO, UDZO, ULFO, ULMO) com layout NONE ou GPT (GPTSPUD).
 * Formato público do UDIF: data fork com os blocos, plist XML com "resource-fork"/"blkx" (tabelas "mish" em base64),
 * trailer "koly" de 512 bytes no fim. Escrito do zero; os descompressores são os do kernel.
 */
#ifndef UDIF_C
#define UDIF_C
#include "pkgengine.c"          /* fs.h, inflate/xz/bzip2 do kernel, utilidades de arquivo */
#include "../../kernel/fs/lzfse.c"

#define U_ZERO 0x00000000u
#define U_RAW 0x00000001u
#define U_IGNORE 0x00000002u
#define U_ADC 0x80000004u
#define U_ZLIB 0x80000005u
#define U_BZIP2 0x80000006u
#define U_LZFSE 0x80000007u
#define U_LZMA 0x80000008u
#define U_COMMENT 0x7FFFFFFEu
#define U_END 0xFFFFFFFFu

static u32 crc_tab[256];
static u32 u_crc32(u32 c, const u8 *p, u64 n) {
    if (!crc_tab[1]) for (u32 i = 0; i < 256; i++) { u32 x = i; for (int k = 0; k < 8; k++) x = x & 1 ? 0xEDB88320u ^ (x >> 1) : x >> 1; crc_tab[i] = x; }
    c = ~c;
    for (u64 i = 0; i < n; i++) c = crc_tab[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return ~c;
}
static void wbe32(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }
static void wbe64(u8 *p, u64 v) { wbe32(p, (u32)(v >> 32)); wbe32(p + 4, (u32)v); }

/* ======================================================================== leitura */
typedef struct { u32 type; u64 sector, count, off, len; } uchunk_t;
typedef struct {
    char name[160];
    u64 first, count;            /* setores do disco */
    u32 crc_type, crc;           /* checksum dos dados descomprimidos da tabela (tipo 2 = CRC32) */
    uchunk_t *c; int nc;
} ublkx_t;
typedef struct {
    u8 *file; u64 fsize;
    u64 sectors, dfork_off, dfork_len, xml_off, xml_len;
    u32 data_crc_type, data_crc, master_crc_type, master_crc;
    ublkx_t *b; int nb;
    char format[8];
} udif_t;
static void udif_close(udif_t *u) { for (int i = 0; i < u->nb; i++) free(u->b[i].c); free(u->b); free(u->file); memset(u, 0, sizeof *u); }
static int b64v(int c) { return c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52 : c == '+' ? 62 : c == '/' ? 63 : -1; }
static u64 b64dec(const char *s, u8 *o) {
    u64 n = 0; u32 acc = 0; int bits = 0;
    for (; *s; s++) { int v = b64v((u8)*s); if (v < 0) continue; acc = acc << 6 | (u32)v; bits += 6; if (bits >= 8) { bits -= 8; o[n++] = (u8)(acc >> bits); } }
    return n;
}
static int udif_open(udif_t *u, const char *path) {
    memset(u, 0, sizeof *u);
    u->file = read_file(path, &u->fsize);
    if (!u->file) return PKERR("não foi possível ler %s", path);
    if (u->fsize < 512 || rb32(u->file + u->fsize - 512) != 0x6B6F6C79) {   /* sem koly: imagem crua (.img, .iso, UDTO) */
        u->sectors = u->fsize / 512;
        snprintf(u->format, sizeof u->format, "RAW");
        return 0;
    }
    const u8 *k = u->file + u->fsize - 512;
    if (rb32(k + 60) > 1) { udif_close(u); return PKERR("imagem segmentada não suportada"); }
    u->dfork_off = rb64(k + 24); u->dfork_len = rb64(k + 32);
    u->data_crc_type = rb32(k + 80); u->data_crc = rb32(k + 88);
    u->xml_off = rb64(k + 216); u->xml_len = rb64(k + 224);
    u->master_crc_type = rb32(k + 352); u->master_crc = rb32(k + 360);
    u->sectors = rb64(k + 492);
    if (!u->xml_len || u->xml_off + u->xml_len > u->fsize) { udif_close(u); return PKERR("imagem sem plist (NDIF antigo)"); }
    xnode *x = xparse((char *)u->file + u->xml_off, (size_t)u->xml_len);
    xnode *rf = 0;
    for (xnode *kk = xfind(x, "dict") ? xfind(x, "dict")->child : 0; kk; kk = kk->next)
        if (!strcmp(kk->name, "key") && !strcmp(xtext(kk), "resource-fork") && kk->next) { rf = kk->next; break; }
    xnode *arr = 0;
    for (xnode *kk = rf ? rf->child : 0; kk; kk = kk->next)
        if (!strcmp(kk->name, "key") && !strcmp(xtext(kk), "blkx") && kk->next) { arr = kk->next; break; }
    int counts[9] = {0};
    for (xnode *d = arr ? arr->child : 0; d; d = d->next) {
        if (strcmp(d->name, "dict")) continue;
        const char *name = "", *data = 0;
        for (xnode *kk = d->child; kk; kk = kk->next) {
            if (strcmp(kk->name, "key") || !kk->next) continue;
            if (!strcmp(xtext(kk), "Name") || !strcmp(xtext(kk), "CFName")) name = xtext(kk->next);
            if (!strcmp(xtext(kk), "Data")) data = xtext(kk->next);
            kk = kk->next;
        }
        if (!data) continue;
        u8 *m = malloc(strlen(data));
        u64 ml = m ? b64dec(data, m) : 0;
        if (ml < 204 || rb32(m) != 0x6D697368) { free(m); continue; }
        ublkx_t *nb = realloc(u->b, sizeof *nb * (size_t)(u->nb + 1));
        if (!nb) { free(m); break; }
        u->b = nb;
        ublkx_t *b = &u->b[u->nb++];
        memset(b, 0, sizeof *b);
        snprintf(b->name, sizeof b->name, "%s", name);
        b->first = rb64(m + 8); b->count = rb64(m + 16);
        b->crc_type = rb32(m + 64); b->crc = rb32(m + 72);
        u32 n = rb32(m + 200);
        if (204 + (u64)n * 40 > ml) n = (u32)((ml - 204) / 40);
        b->c = calloc(n ? n : 1, sizeof *b->c);
        for (u32 i = 0; i < n; i++) {
            const u8 *e = m + 204 + 40 * i;
            uchunk_t *c = &b->c[b->nc++];
            c->type = rb32(e);
            c->sector = b->first + rb64(e + 8); c->count = rb64(e + 16);
            c->off = u->dfork_off + rb64(e + 24); c->len = rb64(e + 32);
            if (c->type == U_RAW) counts[1]++; else if (c->type == U_ZLIB) counts[2]++; else if (c->type == U_BZIP2) counts[3]++;
            else if (c->type == U_LZFSE) counts[4]++; else if (c->type == U_LZMA) counts[5]++; else if (c->type == U_ADC) counts[6]++;
            else if (c->type == U_ZERO || c->type == U_IGNORE) counts[0]++;
        }
        free(m);
    }
    xfree(x);
    if (!u->nb) { udif_close(u); return PKERR("tabela blkx vazia"); }
    const char *f = counts[4] ? "ULFO" : counts[5] ? "ULMO" : counts[3] ? "UDBZ" : counts[2] ? "UDZO" : counts[6] ? "UDCO" : "UDRW";
    snprintf(u->format, sizeof u->format, "%s", f);
    return 0;
}
/* o disco inteiro descomprimido (sectors * 512 bytes); crc_ok = 1 se todos os checksums bateram */
static u8 *udif_expand(udif_t *u, int *crc_ok) {
    if (crc_ok) *crc_ok = 1;
    u64 total = u->sectors * 512;
    if (!u->nb) { u8 *r = malloc((size_t)u->fsize + 1); if (r) memcpy(r, u->file, (size_t)u->fsize); return r; }
    u8 *disk = calloc(1, (size_t)total + 1);
    if (!disk) { PKERR("sem memória para %llu bytes", (unsigned long long)total); return 0; }
    for (int bi = 0; bi < u->nb; bi++) {
        ublkx_t *b = &u->b[bi];
        for (int i = 0; i < b->nc; i++) {
            uchunk_t *c = &b->c[i];
            if (c->type == U_END || c->type == U_COMMENT || c->type == U_ZERO || c->type == U_IGNORE) continue;
            u64 want = c->count * 512;
            if ((c->sector + c->count) * 512 > total || c->off + c->len > u->fsize) { free(disk); PKERR("bloco fora da imagem em %s", b->name); return 0; }
            u8 *dst = disk + c->sector * 512;
            const u8 *src = u->file + c->off;
            i64 r = -1;
            switch (c->type) {
            case U_RAW: r = (i64)(c->len < want ? c->len : want); memcpy(dst, src, (size_t)r); r = (i64)want; break;
            case U_ZLIB: r = zlib_decompress(src, c->len, dst, want); break;
            case U_BZIP2: r = bzip2_decompress(src, c->len, dst, want); break;
            case U_LZFSE: r = lzfse_decompress(src, c->len, dst, want); break;
            case U_LZMA: r = xz_decompress(src, c->len, dst, want); break;
            case U_ADC: r = adc_decompress(src, c->len, dst, want); break;
            default: break;
            }
            if (r != (i64)want) { free(disk); PKERR("bloco do tipo 0x%x não descomprime em %s", c->type, b->name); return 0; }
        }
        if (crc_ok && b->crc_type == 2 && b->count && u_crc32(0, disk + b->first * 512, b->count * 512) != b->crc) *crc_ok = 0;
    }
    if (crc_ok && u->data_crc_type == 2 && u->dfork_off + u->dfork_len <= u->fsize &&
        u_crc32(0, u->file + u->dfork_off, u->dfork_len) != u->data_crc) *crc_ok = 0;
    return disk;
}

/* ======================================================================== escrita */
#ifdef UDIF_HOST
#include <zlib.h>
#include <lzma.h>
static i64 enc_deflate(const u8 *s, u64 n, u8 *d, u64 cap) {
    z_stream z; memset(&z, 0, sizeof z);
    if (deflateInit2(&z, 9, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) return -1;
    z.next_in = (u8 *)s; z.avail_in = (uInt)n; z.next_out = d; z.avail_out = (uInt)cap;
    int r = deflate(&z, Z_FINISH);
    i64 o = r == Z_STREAM_END ? (i64)z.total_out : -1;
    deflateEnd(&z);
    return o;
}
static i64 enc_xz(const u8 *s, u64 n, u8 *d, u64 cap) { size_t o = 0; return lzma_easy_buffer_encode(6, LZMA_CHECK_CRC64, 0, s, (size_t)n, d, &o, (size_t)cap) == LZMA_OK ? (i64)o : -1; }
/* LZFSE no host: blocos "bvx-" (sem compressão, válidos no formato) — o mOS usa o codificador do libcompression */
static i64 enc_lzfse(const u8 *s, u64 n, u8 *d, u64 cap) {
    if (n + 16 > cap) return -1;
    memcpy(d, "bvx-", 4);
    d[4] = (u8)n; d[5] = (u8)(n >> 8); d[6] = (u8)(n >> 16); d[7] = (u8)(n >> 24);
    memcpy(d + 8, s, (size_t)n);
    memcpy(d + 8 + n, "bvx$", 4);
    return (i64)n + 12;
}
#else
size_t compression_encode_buffer(u8 *dst, size_t dn, const u8 *src, size_t sn, void *scratch, int algo);
static i64 enc_deflate(const u8 *s, u64 n, u8 *d, u64 cap) { size_t r = compression_encode_buffer(d, (size_t)cap, s, (size_t)n, 0, 0x205); return r ? (i64)r : -1; }
static i64 enc_xz(const u8 *s, u64 n, u8 *d, u64 cap) { size_t r = compression_encode_buffer(d, (size_t)cap, s, (size_t)n, 0, 0x306); return r ? (i64)r : -1; }
static i64 enc_lzfse(const u8 *s, u64 n, u8 *d, u64 cap) { size_t r = compression_encode_buffer(d, (size_t)cap, s, (size_t)n, 0, 0x801); return r ? (i64)r : -1; }
#endif
/* zlib (o que o UDZO guarda): cabeçalho 78 9C, deflate cru, Adler-32 */
static i64 enc_zlib(const u8 *s, u64 n, u8 *d, u64 cap) {
    if (cap < 6) return -1;
    i64 r = enc_deflate(s, n, d + 2, cap - 6);
    if (r < 0) return -1;
    d[0] = 0x78; d[1] = 0x9C;
    u32 a = 1, b = 0;
    for (u64 i = 0; i < n; i++) { a = (a + s[i]) % 65521; b = (b + a) % 65521; }
    wbe32(d + 2 + r, b << 16 | a);
    return r + 6;
}

typedef struct { u8 *b; u64 n, cap; } ubuf_t;
static int ub_put(ubuf_t *o, const void *p, u64 n) {
    if (o->n + n > o->cap) {
        u64 nc = (o->cap ? o->cap * 2 : 1 << 20) + n;
        u8 *x = realloc(o->b, (size_t)nc);
        if (!x) return -1;
        o->b = x; o->cap = nc;
    }
    memcpy(o->b + o->n, p, (size_t)n);
    o->n += n;
    return 0;
}
static int ub_str(ubuf_t *o, const char *s) { return ub_put(o, s, strlen(s)); }
static void b64enc(ubuf_t *o, const u8 *p, u64 n) {
    static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char line[80]; int k = 0;
    for (u64 i = 0; i < n; i += 3) {
        u32 v = (u32)p[i] << 16 | (i + 1 < n ? (u32)p[i + 1] << 8 : 0) | (i + 2 < n ? p[i + 2] : 0);
        line[k++] = t[v >> 18]; line[k++] = t[(v >> 12) & 63];
        line[k++] = i + 1 < n ? t[(v >> 6) & 63] : '=';
        line[k++] = i + 2 < n ? t[v & 63] : '=';
        if (k >= 68) { line[k++] = '\n'; ub_put(o, "\t\t\t", 3); ub_put(o, line, (u64)k); k = 0; }
    }
    if (k) { line[k++] = '\n'; ub_put(o, "\t\t\t", 3); ub_put(o, line, (u64)k); }
}
/* uma região do disco vira uma tabela mish; formato: U_RAW, U_ZLIB, U_LZFSE ou U_LZMA */
typedef struct { const char *name; u64 first, count; } uregion_t;
static int udif_write(const char *path, const u8 *disk, u64 sectors, u32 fmt, const uregion_t *reg, int nreg) {
    ubuf_t data = {0}, xml = {0};
    u8 *tmp = malloc((1 << 20) + 65536);
    u8 **mish = calloc((size_t)nreg, sizeof *mish);
    u64 *mlen = calloc((size_t)nreg, sizeof *mlen);
    u32 *crcs = calloc((size_t)nreg, sizeof *crcs);
    int ok = tmp && mish && mlen && crcs;
    const u64 CH = 2048;                                  /* 1 MiB por bloco, como o hdiutil */
    for (int r = 0; ok && r < nreg; r++) {
        u64 nch = (reg[r].count + CH - 1) / CH;
        u64 ml = 204 + 40 * (nch + 1);
        u8 *m = calloc(1, (size_t)ml);
        if (!m) { ok = 0; break; }
        mish[r] = m; mlen[r] = ml;
        wbe32(m, 0x6D697368); wbe32(m + 4, 1);
        wbe64(m + 8, reg[r].first); wbe64(m + 16, reg[r].count);
        wbe32(m + 36, 2056);                              /* BuffersNeeded */
        wbe32(m + 40, (u32)(r - 1));                      /* BlockDescriptors (ID) */
        u32 crc = u_crc32(0, disk + reg[r].first * 512, reg[r].count * 512);
        crcs[r] = crc;
        wbe32(m + 64, 2); wbe32(m + 68, 32); wbe32(m + 72, crc);
        wbe32(m + 200, (u32)(nch + 1));
        for (u64 i = 0; i < nch; i++) {
            u64 s = i * CH, cnt = reg[r].count - s < CH ? reg[r].count - s : CH;
            const u8 *src = disk + (reg[r].first + s) * 512;
            u64 bytes = cnt * 512;
            int zero = 1;
            for (u64 k = 0; k < bytes && zero; k++) zero = !src[k];
            u8 *e = m + 204 + 40 * i;
            u32 type = fmt;
            i64 clen = 0;
            if (zero && fmt != U_RAW) type = U_IGNORE;    /* trecho zerado: sem dados (o hdiutil usa "ignore") */
            else if (fmt == U_ZLIB) clen = enc_zlib(src, bytes, tmp, (1 << 20) + 65536);
            else if (fmt == U_LZMA) clen = enc_xz(src, bytes, tmp, (1 << 20) + 65536);
            else if (fmt == U_LZFSE) clen = enc_lzfse(src, bytes, tmp, (1 << 20) + 65536);
            if (fmt != U_RAW && type != U_IGNORE && (clen < 0 || (u64)clen >= bytes)) { type = U_RAW; }   /* não comprime: cru */
            wbe32(e, type);
            wbe64(e + 8, s); wbe64(e + 16, cnt);
            wbe64(e + 24, data.n);
            if (type == U_RAW) { ok &= ub_put(&data, src, bytes) == 0; wbe64(e + 32, bytes); }
            else if (type != U_IGNORE) { ok &= ub_put(&data, tmp, (u64)clen) == 0; wbe64(e + 32, (u64)clen); }
        }
        u8 *end = m + 204 + 40 * nch;
        wbe32(end, U_END);
        wbe64(end + 8, reg[r].count);
        wbe64(end + 24, data.n);
    }
    if (ok) {
        ub_str(&xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                     "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">\n<dict>\n\t<key>resource-fork</key>\n\t<dict>\n"
                     "\t\t<key>blkx</key>\n\t\t<array>\n");
        for (int r = 0; r < nreg; r++) {
            char h[600];
            snprintf(h, sizeof h, "\t\t\t<dict>\n\t\t\t\t<key>Attributes</key>\n\t\t\t\t<string>0x0050</string>\n\t\t\t\t<key>CFName</key>\n"
                                  "\t\t\t\t<string>%s</string>\n\t\t\t\t<key>Data</key>\n\t\t\t\t<data>\n", reg[r].name);
            ub_str(&xml, h);
            b64enc(&xml, mish[r], mlen[r]);
            snprintf(h, sizeof h, "\t\t\t\t</data>\n\t\t\t\t<key>ID</key>\n\t\t\t\t<string>%d</string>\n\t\t\t\t<key>Name</key>\n"
                                  "\t\t\t\t<string>%s</string>\n\t\t\t</dict>\n", r - 1, reg[r].name);
            ub_str(&xml, h);
        }
        ub_str(&xml, "\t\t</array>\n\t</dict>\n</dict>\n</plist>\n");
    }
    u8 k[512] = {0};
    if (ok) {
        wbe32(k, 0x6B6F6C79); wbe32(k + 4, 4); wbe32(k + 8, 512); wbe32(k + 12, 1);
        wbe64(k + 24, 0); wbe64(k + 32, data.n);
        wbe32(k + 56, 1); wbe32(k + 60, 1);
        for (int i = 0; i < 16; i++) k[64 + i] = (u8)(rand() & 0xFF);
        wbe32(k + 80, 2); wbe32(k + 84, 32); wbe32(k + 88, u_crc32(0, data.b, data.n));
        wbe64(k + 216, data.n); wbe64(k + 224, xml.n);
        /* checksum mestre: CRC32 dos checksums das tabelas, em big-endian, em sequência */
        u32 mc = 0;
        for (int r = 0; r < nreg; r++) { u8 be[4]; wbe32(be, crcs[r]); mc = u_crc32(mc, be, 4); }
        wbe32(k + 352, 2); wbe32(k + 356, 32); wbe32(k + 360, mc);
        wbe32(k + 488, 1);
        wbe64(k + 492, sectors);
    }
    int fd = ok ? open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644) : -1;
    if (fd >= 0) {
        ok = write(fd, data.b, (size_t)data.n) == (ssize_t)data.n && write(fd, xml.b, (size_t)xml.n) == (ssize_t)xml.n && write(fd, k, 512) == 512;
        close(fd);
    } else ok = 0;
    for (int r = 0; mish && r < nreg; r++) free(mish[r]);
    free(mish); free(mlen); free(crcs); free(tmp); free(data.b); free(xml.b);
    return ok ? 0 : PKERR("não foi possível gravar %s", path);
}

/* ======================================================================== GPT (layout GPTSPUD do hdiutil) */
static const u8 GUID_HFS[16] = {0x00, 0x53, 0x46, 0x48, 0x00, 0x00, 0xAA, 0x11, 0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC};
static const u8 GUID_BASIC[16] = {0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7};
static void wle32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static void wle64(u8 *p, u64 v) { wle32(p, (u32)v); wle32(p + 4, (u32)(v >> 32)); }
static void rand_guid(u8 *g) { for (int i = 0; i < 16; i++) g[i] = (u8)(rand() & 0xFF); g[7] = (u8)((g[7] & 0x0F) | 0x40); g[8] = (u8)((g[8] & 0x3F) | 0x80); }
/* disco de `sectors` setores com uma partição em [40, sectors-34) do tipo pedido (HFS ou Basic Data); nome UTF-16 */
static void gpt_write(u8 *d, u64 sectors, int hfs, const char *pname) {
    u64 first = 40, last = sectors - 34;
    /* MBR protetor */
    d[446 + 4] = 0xEE; d[446 + 1] = 0; d[446 + 2] = 2; d[446 + 3] = 0; d[446 + 5] = 0xFF; d[446 + 6] = 0xFF; d[446 + 7] = 0xFF;
    wle32(d + 446 + 8, 1);
    wle32(d + 446 + 12, sectors - 1 > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)(sectors - 1));
    d[510] = 0x55; d[511] = 0xAA;
    u8 *ent = calloc(1, 128 * 128);
    if (!ent) return;
    memcpy(ent, hfs ? GUID_HFS : GUID_BASIC, 16);
    rand_guid(ent + 16);
    wle64(ent + 32, first); wle64(ent + 40, last - 1);
    for (int i = 0; pname && pname[i] && i < 36; i++) { ent[56 + 2 * i] = (u8)pname[i]; }
    u32 ecrc = u_crc32(0, ent, 128 * 128);
    u8 disk_guid[16];
    rand_guid(disk_guid);
    for (int copy = 0; copy < 2; copy++) {
        u64 hdr_lba = copy ? sectors - 1 : 1, ents_lba = copy ? sectors - 33 : 2;
        u8 *h = d + hdr_lba * 512;
        memset(h, 0, 512);
        memcpy(h, "EFI PART", 8);
        wle32(h + 8, 0x00010000); wle32(h + 12, 92);
        wle64(h + 24, hdr_lba); wle64(h + 32, copy ? 1 : sectors - 1);
        wle64(h + 40, 34); wle64(h + 48, sectors - 34);
        memcpy(h + 56, disk_guid, 16);
        wle64(h + 72, ents_lba); wle32(h + 80, 128); wle32(h + 84, 128); wle32(h + 88, ecrc);
        wle32(h + 16, u_crc32(0, h, 92));
        memcpy(d + ents_lba * 512, ent, 128 * 128);
    }
    free(ent);
}
#endif
