/* mOS — teste de host do leitor CFF do kernel (kernel/gfx/font_cff.inc), sem VM:
 *   node tools\woff2otf.js "<fonte>.woff2" build\fonte.otf        (WOFF2 de sabor CFF -> OTF; ex.: RobotoSlab da doc do QEMU)
 *   zig cc -target x86_64-windows-gnu -O1 -Ikernel/gfx tools/cfftest.c -o build/cfftest.exe
 *   build\cfftest.exe build\fonte.otf build\cff.png "Hamburgefonstiv ijk"
 * Confere a fonte (cff_check), passa todos os glifos pelo interpretador Type 2, desenha o texto em 72 px e 15 px com o
 * mesmo rasterizador do kernel (cópia de raster_line/raster_finish de kernel/gfx/gfx.c) e grava um PNG. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t i8; typedef int16_t i16; typedef int32_t i32; typedef int64_t i64;
static inline u16 be16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }
static inline i16 bes16(const u8 *p) { return (i16)be16(p); }
static inline u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
#define kmalloc malloc
#define kfree free
#define krealloc realloc
static float fabsf_(float x) { return fabsf(x); }
static float fsqrt(float x) { return sqrtf(x); }
static float ffloor(float x) { return floorf(x); }
static float fceil(float x) { return ceilf(x); }
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
typedef struct { float *pts; int n, cap; } lines_t;
static void add_line(lines_t *l, float x0, float y0, float x1, float y1) {
    if (l->n + 4 > l->cap) { l->cap = l->cap ? l->cap * 2 : 512; l->pts = krealloc(l->pts, l->cap * sizeof(float)); }
    l->pts[l->n++] = x0; l->pts[l->n++] = y0; l->pts[l->n++] = x1; l->pts[l->n++] = y1;
}
#include "font_cff.inc"

/* cópia do rasterizador do kernel (kernel/gfx/gfx.c) */
static inline bool fin_(float f) { u32 b; memcpy(&b, &f, 4); return (b & 0x7F800000u) != 0x7F800000u; }
static void raster_line(float *acc, int w, int h, float x0, float y0, float x1, float y1) {
    if (w < 2 || h < 1 || !fin_(x0) || !fin_(y0) || !fin_(x1) || !fin_(y1)) return;
    if (fabsf_(y0 - y1) < 1e-6f) return;
    float dir = 1;
    if (y0 > y1) { float t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; dir = -1; }
    float dxdy = (x1 - x0) / (y1 - y0);
    float x = x0;
    if (y0 < 0) { x -= y0 * dxdy; y0 = 0; }
    if (y0 >= h) return;
    int ys = (int)y0, ye = (int)fceil(MIN((float)h, y1));
    for (int yy = ys; yy < ye; yy++) {
        int ls = yy * w;
        float dy = MIN((float)(yy + 1), y1) - MAX((float)yy, y0);
        float xn = x + dxdy * dy;
        float d = dy * dir;
        float a0x = x < xn ? x : xn, a1x = x < xn ? xn : x;
        if (!(a0x >= 0)) a0x = 0;
        if (!(a1x >= 0)) a1x = 0;
        if (a0x > w - 1.001f) a0x = w - 1.001f;
        if (a1x > w - 1.001f) a1x = w - 1.001f;
        float x0f = ffloor(a0x);
        int x0i = (int)x0f;
        float x1c = fceil(a1x);
        int x1i = (int)x1c;
        if (x1i <= x0i + 1) {
            float xmf = 0.5f * (a0x + a1x) - x0f;
            acc[ls + x0i] += d - d * xmf;
            acc[ls + x0i + 1] += d * xmf;
        } else {
            float sc = 1.0f / (a1x - a0x);
            float x0r = a0x - x0f;
            float aa0 = 0.5f * sc * (1 - x0r) * (1 - x0r);
            float x1r = a1x - x1c + 1;
            float am = 0.5f * sc * x1r * x1r;
            acc[ls + x0i] += d * aa0;
            if (x1i == x0i + 2) acc[ls + x0i + 1] += d * (1 - aa0 - am);
            else {
                float a1 = sc * (1.5f - x0r);
                acc[ls + x0i + 1] += d * (a1 - aa0);
                for (int xi = x0i + 2; xi < x1i - 1; xi++) acc[ls + xi] += d * sc;
                float a2 = a1 + (x1i - x0i - 3) * sc;
                acc[ls + x1i - 1] += d * (1 - a2 - am);
            }
            acc[ls + x1i] += d * am;
        }
        x = xn;
    }
}
static void raster_finish(const float *acc, int n, u8 *out) {
    float a = 0;
    for (int i = 0; i < n; i++) { a += acc[i]; float v = a < 0 ? -a : a; if (v > 1) v = 1; out[i] = (u8)(v * 255 + 0.5f); }
}

static const u8 *D; static u32 N;
static u32 table(const char *tag, u32 *len) {
    u16 n = be16(D + 4);
    for (u16 i = 0; i < n; i++) { const u8 *r = D + 12 + i * 16; if (!memcmp(r, tag, 4)) { if (len) *len = be32(r + 12); return be32(r + 8); } }
    return 0;
}
static u32 cmap_sub; static int cmap_fmt;
static u16 gid_of(u32 cp) {
    const u8 *t = D + cmap_sub;
    if (cmap_fmt == 12) {
        u32 n = be32(t + 12), lo = 0, hi = n;
        while (lo < hi) { u32 mid = (lo + hi) / 2; const u8 *g = t + 16 + mid * 12; u32 s = be32(g), e = be32(g + 4);
            if (cp < s) hi = mid; else if (cp > e) lo = mid + 1; else return (u16)(be32(g + 8) + cp - s); }
        return 0;
    }
    if (cp > 0xFFFF) return 0;
    u16 segx2 = be16(t + 6);
    const u8 *ends = t + 14, *starts = ends + segx2 + 2, *deltas = starts + segx2, *ranges = deltas + segx2;
    for (u16 i = 0; i < segx2 / 2; i++) {
        if (be16(ends + i * 2) < cp) continue;
        u16 s = be16(starts + i * 2);
        if (s > cp) return 0;
        u16 ro = be16(ranges + i * 2);
        if (!ro) return (u16)(cp + bes16(deltas + i * 2));
        u16 g = be16(ranges + i * 2 + ro + (cp - s) * 2);
        return g ? (u16)(g + bes16(deltas + i * 2)) : 0;
    }
    return 0;
}

/* PNG em tons de cinza com deflate "armazenado" */
static u32 crc_tab[256];
static u32 crc(const u8 *b, size_t n, u32 c) { for (size_t i = 0; i < n; i++) c = crc_tab[(c ^ b[i]) & 255] ^ (c >> 8); return c; }
static void be32w(u8 *p, u32 v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void chunk(FILE *f, const char *t, const u8 *d, u32 n) {
    u8 h[8]; be32w(h, n); memcpy(h + 4, t, 4); fwrite(h, 1, 8, f);
    if (n) fwrite(d, 1, n, f);
    u32 c = crc((const u8 *)t, 4, 0xFFFFFFFF); c = crc(d, n, c) ^ 0xFFFFFFFF; be32w(h, c); fwrite(h, 1, 4, f);
}
static void png(const char *path, const u8 *img, int w, int h) {
    for (u32 i = 0; i < 256; i++) { u32 c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >> 1) : c >> 1; crc_tab[i] = c; }
    size_t raw_n = (size_t)(w + 1) * h;
    u8 *raw = malloc(raw_n);
    for (int y = 0; y < h; y++) { raw[y * (w + 1)] = 0; for (int x = 0; x < w; x++) raw[y * (w + 1) + 1 + x] = 255 - img[y * w + x]; }
    size_t nb = (raw_n + 65534) / 65535, zn = 2 + raw_n + nb * 5 + 4;
    u8 *z = malloc(zn), *q = z;
    *q++ = 0x78; *q++ = 0x01;
    u32 a = 1, b = 0;
    for (size_t i = 0; i < raw_n; i++) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw_n; off += 65535) {
        u32 len = raw_n - off > 65535 ? 65535 : (u32)(raw_n - off);
        *q++ = off + len >= raw_n; *q++ = len & 255; *q++ = len >> 8; *q++ = ~len & 255; *q++ = (~len >> 8) & 255;
        memcpy(q, raw + off, len); q += len;
    }
    be32w(q, (b << 16) | a); q += 4;
    FILE *f = fopen(path, "wb");
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    u8 ih[13]; be32w(ih, w); be32w(ih + 4, h); ih[8] = 8; ih[9] = 0; ih[10] = 0; ih[11] = 0; ih[12] = 0;
    chunk(f, "IHDR", ih, 13); chunk(f, "IDAT", z, (u32)(q - z)); chunk(f, "IEND", 0, 0);
    fclose(f); free(raw); free(z);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "uso: cfftest fonte.otf saida.png [texto]\n"); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END); N = (u32)ftell(f); fseek(f, 0, SEEK_SET);
    u8 *buf = malloc(N); fread(buf, 1, N, f); fclose(f); D = buf;
    u32 cl = 0, co = table("CFF ", &cl), maxp = table("maxp", 0), head = table("head", 0), hhea = table("hhea", 0), hmtx = table("hmtx", 0), cmap = table("cmap", 0);
    u32 ng = be16(D + maxp + 4), upem = be16(D + head + 18), nh = be16(D + hhea + 34);
    printf("CFF %u bytes, %u glifos, upem %u\n", cl, ng, upem);
    printf("cff_check: %s\n", cff_check(D + co, cl, ng) ? "ok" : "RECUSADA");
    cff_t *c = (cff_t *)cff_load(D + co, cl, ng);
    if (!c) return 1;
    printf("charstrings %u, gsubrs %u, lsubrs %u, FDs %u\n", c->cs.count, c->gsubr.count, c->lsubr[0].count, c->nfd);
    int bad = 0, empty = 0;
    for (u32 g = 0; g < ng; g++) {
        lines_t l = {0};
        if (!cff_outline(c, g, 0.05f, 0, 0, &l)) bad++;
        else if (!l.n) empty++;
        free(l.pts);
    }
    printf("glifos: %d com erro, %d vazios\n", bad, empty);
    /* cmap: 3,10 (12) ou 3,1 (4) */
    u16 nsub = be16(D + cmap + 2);
    for (u16 i = 0; i < nsub; i++) {
        const u8 *r = D + cmap + 4 + i * 8;
        u32 off = cmap + be32(r + 4);
        u16 fmt = be16(D + off), pid = be16(r), eid = be16(r + 2);
        if (fmt == 12 && (pid == 0 || eid == 10)) { cmap_sub = off; cmap_fmt = 12; break; }
        if (fmt == 4 && (pid == 0 || eid == 1)) { cmap_sub = off; cmap_fmt = 4; }
    }
    const char *text = argc > 3 ? argv[3] : "Hamburgefonstiv 0123 Slack mOS";
    int W = 1500, H = 260;
    float *acc = calloc((size_t)W * H + 4, sizeof(float));
    float sizes[2] = {72, 15};
    float baselines[2] = {110, 200};
    for (int k = 0; k < 2; k++) {
        float scale = sizes[k] / upem, pen = 20;
        for (const char *s = text; *s; s++) {
            u16 g = gid_of((u8)*s);
            lines_t l = {0};
            cff_outline(c, g, scale, pen, baselines[k], &l);
            for (int i = 0; i < l.n; i += 4) raster_line(acc, W, H, l.pts[i], l.pts[i + 1], l.pts[i + 2], l.pts[i + 3]);
            free(l.pts);
            u16 gi = g < nh ? g : nh - 1;
            pen += be16(D + hmtx + gi * 4) * scale;
        }
    }
    u8 *img = malloc((size_t)W * H);
    raster_finish(acc, W * H, img);
    png(argv[2], img, W, H);
    printf("imagem: %s\n", argv[2]);
    return 0;
}
