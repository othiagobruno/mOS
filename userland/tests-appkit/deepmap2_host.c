/* Teste de host do deepmap2 e do deepmap antigo de userland/libsystem/car_core.c com dados sintéticos: um codificador mínimo aqui monta os
 * contêineres (padrão com os quatro preditores, cru, sem perdas, paleta e blocos KCBC) e o "descompressor" só copia.
 * gcc -O1 -Wall -Wextra -I userland/libsystem userland/tests-appkit/deepmap2_host.c userland/libsystem/car_core.c */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "car_core.h"

static int fails, total;
#define CHECK(c, m) do { total++; if (!(c)) { fails++; printf("FALHOU: %s\n", m); } } while (0)

static long copy_dec(int kind, const uint8_t *in, long n, uint8_t *out, long cap) {
    (void)kind;
    if (n > cap) return 0;
    memcpy(out, in, (size_t)n);
    return n;
}
static void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }

/* cabeçalho do contêiner + dmp2; devolve o tamanho do cabeçalho (32) */
static int header(uint8_t *b, int method, int pf, int w, int h, uint32_t desc, long body) {
    put32(b, 1); put32(b + 4, 0); put32(b + 8, (uint32_t)(16 + body)); put32(b + 12, 0);
    memcpy(b + 16, "dmp2", 4);
    b[20] = (uint8_t)method; b[21] = 1; b[22] = 0; b[23] = (uint8_t)pf;
    put16(b + 24, (uint16_t)w); put16(b + 26, (uint16_t)h);
    put32(b + 28, desc);
    return 32;
}

/* imagem de teste: cada pixel vem de (y, co, cg) inteiros; a inversa do decodificador dá r, g, b */
#define W 5
#define H 4
static int Y[H][W], CO[H][W], CG[H][W];
static uint8_t AL[H][W];
static void rgb_of(int y, int co, int cg, int *r, int *g, int *b) {
    int t = y - cg; *g = t + 2 * cg; *b = t - co; *r = *b + 2 * co;
}
static void make_image(void) {
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            Y[y][x] = 90 + 7 * x + 5 * y; CO[y][x] = 10 - x; CG[y][x] = 3 + y - x;
            AL[y][x] = (uint8_t)(255 - 10 * x);
        }
}
static int pred_val(int comp, int y, int x) { return comp == 0 ? Y[y][x] : comp == 1 ? CO[y][x] : CG[y][x]; }
static int iabs(int v) { return v < 0 ? -v : v; }
/* resíduo de (y, x, comp) para o preditor p (a mesma regra que o decodificador desfaz) */
static int residual(int p, int y, int x, int comp) {
    int v = pred_val(comp, y, x), up = y > 0 ? pred_val(comp, y - 1, x) : 0;
    switch (p) {
    case 0: return v;
    case 3: return v - up;
    case 2: return x ? v - pred_val(comp, y, x - 1) : v;
    case 1:
        if (!x) return v - up;
        {
            int ly = Y[y][x - 1], uy = y > 0 ? Y[y - 1][x] : 0, uly = y > 0 ? Y[y - 1][x - 1] : 0;
            int src = iabs(uy - uly) > iabs(ly - uly) ? up : pred_val(comp, y, x - 1);
            return v - src;
        }
    case 4:
        if (!x) return v - up;
        { int sum = pred_val(comp, y, x - 1) + up + 1; if (sum < 0) sum++; return v - (sum >> 1); }
    }
    return 0;
}
static long build_default(uint8_t *b, const int *preds) {
    long npx = W * H, body = npx + H + 6 * npx;
    uint8_t *p = b + header(b, 2, 4, W, H, (uint32_t)body, body), *alpha = p, *pr = p + npx, *hi = pr + H, *lo = hi + 3 * npx;
    for (int y = 0; y < H; y++) {
        pr[y] = (uint8_t)preds[y];
        for (int x = 0; x < W; x++) {
            alpha[y * W + x] = AL[y][x];
            for (int k = 0; k < 3; k++) {
                int r = residual(preds[y], y, x, k);
                uint16_t e = (uint16_t)((iabs(r) << 1) | (r < 0));
                hi[(y * W + x) * 3 + k] = e >> 8; lo[(y * W + x) * 3 + k] = e & 255;
            }
        }
    }
    put32(b + 8, (uint32_t)(16 + body + 4));   /* comprimento do contêiner inclui o descritor */
    return 32 + body;
}
static int check_rgba(const uint8_t *out) {
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int r, g, b;
            rgb_of(Y[y][x], CO[y][x], CG[y][x], &r, &g, &b);
            const uint8_t *o = out + (y * W + x) * 4;
            if (o[0] != (uint8_t)b || o[1] != (uint8_t)g || o[2] != (uint8_t)r || o[3] != AL[y][x]) return 0;
        }
    return 1;
}

int main(void) {
    static uint8_t buf[1 << 16], out[W * H * 4 + 64], work[1 << 16];
    make_image();
    int all[5][H] = {{0, 0, 0, 0}, {3, 3, 3, 3}, {2, 2, 2, 2}, {0, 1, 1, 1}, {0, 4, 4, 4}};
    const char *names[5] = {"preditor 0 (nenhum)", "preditor 3 (cima)", "preditor 2 (esquerda)", "preditor 1 (cima ou esquerda)", "preditor 4 (média)"};
    for (int t = 0; t < 5; t++) {
        long n = build_default(buf, all[t]);
        memset(out, 0, sizeof out);
        int ok = car_deepmap2(buf, n, 0, copy_dec, W, H, 4, out, W * 4, W * H * 4, work, sizeof work);
        CHECK(ok && check_rgba(out), names[t]);
    }
    {   /* preditores mistos, linha a linha */
        int mix[H] = {0, 2, 4, 1};
        long n = build_default(buf, mix);
        int ok = car_deepmap2(buf, n, 0, copy_dec, W, H, 4, out, W * 4, W * H * 4, work, sizeof work);
        CHECK(ok && check_rgba(out), "preditores diferentes por linha");
    }
    {   /* cru */
        long body = W * H * 4;
        int hl = header(buf, 1, 4, W, H, 0, body) - 4;   /* o cru não tem descritor */
        for (long i = 0; i < body; i++) buf[hl + i] = (uint8_t)(i * 7);
        put32(buf + 8, (uint32_t)(12 + body));
        int ok = car_deepmap2(buf, hl + body, 0, copy_dec, W, H, 4, out, W * 4, W * H * 4, work, sizeof work);
        CHECK(ok && out[0] == 0 && out[5] == 35 && out[W * H * 4 - 1] == (uint8_t)((W * H * 4 - 1) * 7), "método 1 (cru)");
    }
    {   /* sem perdas em dois segmentos (o 2º com o tamanho antes) */
        long half = W * 2 * 4, body = half + 4 + half;
        uint8_t *p = buf + header(buf, 3, 4, W, H, (uint32_t)half, body);
        for (long i = 0; i < half; i++) p[i] = (uint8_t)(i + 1);
        put32(p + half, (uint32_t)half);
        for (long i = 0; i < half; i++) p[half + 4 + i] = (uint8_t)(200 - i);
        int ok = car_deepmap2(buf, 32 + body, 0, copy_dec, W, H, 4, out, W * 4, W * H * 4, work, sizeof work);
        CHECK(ok && out[0] == 1 && out[half - 1] == (uint8_t)half && out[half] == 200, "método 3 (sem perdas, dois segmentos)");
    }
    {   /* paleta: 2 cores com as 3 primeiras componentes; a 4ª (alfa) num plano à parte */
        long npx = W * H, body = 2 * 4 + 4 + npx + npx;
        uint8_t *p = buf + header(buf, 4, 4, W, H, 2 | (3u << 16), body);
        uint8_t pal[8] = {10, 20, 30, 0, 40, 50, 60, 0};
        memcpy(p, pal, 8);
        put32(p + 8, (uint32_t)(npx + npx));
        uint8_t *planes = p + 12, *idx = planes + npx;
        for (long i = 0; i < npx; i++) { planes[i] = (uint8_t)(100 + i); idx[i] = (uint8_t)(i & 1); }
        int ok = car_deepmap2(buf, 32 + body, 0, copy_dec, W, H, 4, out, W * 4, W * H * 4, work, sizeof work);
        CHECK(ok && out[0] == 10 && out[2] == 30 && out[3] == 100 && out[4] == 40 && out[7] == 101, "método 4 (paleta + plano de alfa)");
        idx[3] = 5;
        CHECK(!car_deepmap2(buf, 32 + body, 0, copy_dec, W, H, 4, out, W * 4, W * H * 4, work, sizeof work), "índice fora da paleta é recusado");
    }
    {   /* blocos KCBC: dois contêineres crus de 2 linhas cada */
        long body = W * 2 * 4, cl = 28 + body;
        uint8_t *p = buf;
        for (int blk = 0; blk < 2; blk++) {
            memcpy(p, "KCBC", 4); put32(p + 4, 0); put32(p + 8, 0); put32(p + 12, 2); put32(p + 16, (uint32_t)cl);
            uint8_t *c = p + 20;
            header(c, 1, 4, W, 2, 0, body);
            put32(c + 8, (uint32_t)(12 + body));
            for (long i = 0; i < body; i++) c[28 + i] = (uint8_t)(blk * 100 + (i & 63));
            p += 20 + cl;
        }
        int ok = car_deepmap2(buf, p - buf, 2, copy_dec, W, H, 4, out, W * 4, W * H * 4, work, sizeof work);
        CHECK(ok && out[0] == 0 && out[W * 2 * 4] == 100, "blocos KCBC em sequência");
    }
    {   /* recusas: largura errada, contêiner truncado, formato de 16 bits */
        int z[H] = {0, 0, 0, 0};
        long n = build_default(buf, z);
        CHECK(!car_deepmap2(buf, n, 0, copy_dec, W + 1, H, 4, out, (W + 1) * 4, sizeof out, work, sizeof work), "largura diferente é recusada");
        CHECK(!car_deepmap2(buf, 30, 0, copy_dec, W, H, 4, out, W * 4, W * H * 4, work, sizeof work), "contêiner truncado é recusado");
        buf[23] = 20;
        CHECK(!car_deepmap2(buf, n, 0, copy_dec, W, H, 4, out, W * 4, W * H * 4, work, sizeof work), "formato 20 (16 bits) não é lido");
    }
    {   /* deepmap antigo: 260x2 em dois blocos (256 + 4 de largura), sem perdas */
        static uint8_t big[300000], bout[260 * 2 * 4];
        enum { BW = 260, BH = 2 };
        uint8_t *b = big;
        memset(b, 0, 28); memcpy(b + 16, "dmap", 4); b[20] = 3; b[21] = 1; b[23] = 4;
        long t0 = 256L * BH * 4, t1 = 4L * BH * 4;
        put32(b + 24, (uint32_t)t0);
        uint8_t *q = b + 28;
        for (long i = 0; i < t0; i++) q[i] = (uint8_t)(i % 251);
        q += t0; put32(q, (uint32_t)t1); q += 4;
        for (long i = 0; i < t1; i++) q[i] = (uint8_t)(200 + i);
        q += t1;
        put32(b + 8, (uint32_t)(q - b - 16));
        int ok = car_deepmap1(b, q - b, 0, copy_dec, BW, BH, 4, bout, BW * 4, sizeof bout, work, sizeof work);
        CHECK(ok && bout[0] == 0 && bout[255 * 4 + 3] == (uint8_t)((255 * 4 + 3) % 251) && bout[256 * 4] == 200
              && bout[BW * 4 + 256 * 4] == 216, "deepmap antigo: dois blocos sem perdas costurados");
        /* padrão: um bloco 3x2 com preditor 0 (os resíduos são os próprios valores YCoCg) */
        enum { SW = 3, SH = 2 };
        long npx = SW * SH, need = npx + SH + 6 * npx, padded = (need + 7) & ~7L;
        memset(b, 0, 28 + padded); memcpy(b + 16, "dmap", 4); b[20] = 2; b[21] = 1; b[23] = 4;
        put32(b + 24, (uint32_t)padded);
        uint8_t *al = b + 28, *pr = al + npx, *hi = pr + SH, *lo = hi + 3 * npx;
        for (long i = 0; i < npx; i++) {
            al[i] = 255;
            int v[3] = {100 + (int)i, -3, 2};   /* Y, Co/2, Cg/2 */
            for (int k = 0; k < 3; k++) { uint16_t e = (uint16_t)((iabs(v[k]) << 1) | (v[k] < 0)); hi[i * 3 + k] = e >> 8; lo[i * 3 + k] = e & 255; }
        }
        put32(b + 8, (uint32_t)(12 + padded));
        ok = car_deepmap1(b, 28 + padded, 0, copy_dec, SW, SH, 4, bout, SW * 4, sizeof bout, work, sizeof work);
        int r, g, bl;
        rgb_of(105, -3, 2, &r, &g, &bl);
        CHECK(ok && bout[(SW + 2) * 4] == (uint8_t)bl && bout[(SW + 2) * 4 + 1] == (uint8_t)g && bout[(SW + 2) * 4 + 2] == (uint8_t)r,
              "deepmap antigo: bloco padrão pelo transformador do deepmap2");
        b[16] = 'x';
        CHECK(!car_deepmap1(b, 28 + padded, 0, copy_dec, SW, SH, 4, bout, SW * 4, sizeof bout, work, sizeof work), "sem \"dmap\" é recusado");
    }
    printf("deepmap2_host: %d/%d\n", total - fails, total);
    return fails != 0;
}
