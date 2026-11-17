/* teste de host do car_core (Assets.car): monta um catálogo BOM sintético e lê de volta
 * gcc -std=c11 -Wall -Wextra -Iuserland/libsystem userland/libsystem/car_core.c userland/tests-appkit/car_host.c -o /tmp/car-test && /tmp/car-test
 * Com um Assets.car real como argumento, lista os nomes e decodifica cada um (só os não comprimidos, sem LZFSE aqui). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "car_core.h"
static int ok, total;
static void check(int c, const char *m) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "FALHOU", m); }
/* ---- escritor de BOM mínimo */
static unsigned char buf[1 << 16];
static long pos = 512;
static unsigned addr[64], len_[64];
static int nb = 1;   /* o bloco 0 fica vazio, como no formato */
static void be32w(unsigned char *p, unsigned v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void be16w(unsigned char *p, unsigned v) { p[0] = v >> 8; p[1] = v; }
static void le32w(unsigned char *p, unsigned v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void le16w(unsigned char *p, unsigned v) { p[0] = v; p[1] = v >> 8; }
static int add_block(const void *d, long n) { memcpy(buf + pos, d, n); addr[nb] = pos; len_[nb] = n; pos += n; return nb++; }
static int tree_with(int *keys, int *vals, int n) {
    unsigned char leaf[12 + 8 * 8] = {0};
    be16w(leaf, 1); be16w(leaf + 2, n);
    for (int i = 0; i < n; i++) { be32w(leaf + 12 + 8 * i, vals[i]); be32w(leaf + 16 + 8 * i, keys[i]); }
    int lb = add_block(leaf, 12 + 8 * n);
    unsigned char t[21] = {'t', 'r', 'e', 'e'};
    be32w(t + 4, 1); be32w(t + 8, lb); be32w(t + 12, 4096); be32w(t + 16, n);
    return add_block(t, 21);
}
static long csi(unsigned char *o, const char *pf, int w, int h, int scale, int layout, const char *name, const unsigned char *tlv, long tl, const unsigned char *data, long dl) {
    memset(o, 0, 184);
    memcpy(o, "ISTC", 4); le32w(o + 4, 1); le32w(o + 12, w); le32w(o + 16, h); le32w(o + 20, scale);
    unsigned p4 = (unsigned)pf[0] << 24 | (unsigned)pf[1] << 16 | (unsigned)pf[2] << 8 | (unsigned)pf[3];
    le32w(o + 24, p4); le16w(o + 36, layout); strncpy((char *)o + 40, name, 127);
    le32w(o + 168, tl); le32w(o + 180, dl);
    memcpy(o + 184, tlv, tl); memcpy(o + 184 + tl, data, dl);
    return 184 + tl + dl;
}
int main(int argc, char **argv) {
    if (argc > 1) {   /* catálogo real: lista os nomes */
        FILE *f = fopen(argv[1], "rb"); static unsigned char d[1 << 24]; long n = f ? (long)fread(d, 1, sizeof d, f) : 0; if (f) fclose(f);
        car_file c; char nm[256];
        if (!car_open(&c, d, n)) { printf("não abriu\n"); return 1; }
        for (int i = 0; car_name_at(&c, i, nm, sizeof nm) >= 0; i++) { car_rendition r; uint16_t k[32]; int nk; int found = car_find(&c, nm, 200, 0, &r, k, &nk); printf("%-40s %d tipo %d %dx%d escala %d\n", nm, found, r.kind, r.width, r.height, r.scale); }
        return 0;
    }
    /* KEYFORMAT: aparência 7, elemento 1, parte 2, identificador 17, escala 12 */
    unsigned char kf[12 + 5 * 4] = {'t', 'm', 'f', 'k'};
    unsigned attrs[5] = {7, 1, 2, 17, 12};
    le32w(kf + 8, 5);
    for (int i = 0; i < 5; i++) le32w(kf + 12 + 4 * i, attrs[i]);
    int kfb = add_block(kf, sizeof kf);
    /* facets: "Cor" (id 10), "Seta" (id 20), "Dado" (id 30) */
    const char *names[3] = {"Cor", "Seta", "Dado"};
    int ids[3] = {10, 20, 30}, fk[3], fv[3];
    for (int i = 0; i < 3; i++) {
        fk[i] = add_block(names[i], (long)strlen(names[i]));
        unsigned char v[6 + 4] = {0};
        le16w(v + 4, 1); le16w(v + 6, 17); le16w(v + 8, ids[i]);
        fv[i] = add_block(v, sizeof v);
    }
    int facets = tree_with(fk, fv, 3);
    /* renditions */
    static unsigned char r[4][4096];
    unsigned char key[4][10];
    long rl[4];
    /* 1) cor sRGB (0.25, 0.5, 0.75, 1) */
    unsigned char col[16 + 32] = {'R', 'L', 'O', 'C'};
    le32w(col + 4, 1); le32w(col + 12, 4);
    double comps[4] = {0.25, 0.5, 0.75, 1};
    memcpy(col + 16, comps, 32);
    rl[0] = csi(r[0], "\0\0\0\0", 0, 0, 0, 1009, "Cor", 0, 0, col, sizeof col);   /* nos arquivos os bytes são "RLOC" */
    memset(key[0], 0, 10); le16w(key[0] + 6, 10);
    /* 2) atlas 4x4 ARGB sem compressão (MLEC v0, compressão 0): metade de cima vermelha, de baixo azul */
    unsigned char bm[16 + 64] = {'M', 'L', 'E', 'C'};
    le32w(bm + 12, 64);
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) { unsigned char *p = bm + 16 + (y * 4 + x) * 4; p[3] = 255; if (y < 2) p[2] = 255; else p[0] = 255; }
    rl[1] = csi(r[1], "ARGB", 4, 4, 100, 1004, "ZZZZPackedAsset-1.0.0-gamut0", 0, 0, bm, sizeof bm);
    memset(key[1], 0, 10); le16w(key[1] + 2, 9); le16w(key[1] + 4, 181); le16w(key[1] + 8, 1);
    /* 3) "Seta": 4x2 ligada ao atlas, retângulo y=0 (contado de baixo) = a metade azul */
    unsigned char tlv[8 + 30 + 12];
    le32w(tlv, 1010); le32w(tlv + 4, 30 + 12);
    unsigned char *ln = tlv + 8;
    memcpy(ln, "KLNI", 4); le32w(ln + 4, 0); le32w(ln + 8, 0); le32w(ln + 12, 0); le32w(ln + 16, 4); le32w(ln + 20, 2);
    le16w(ln + 24, 12); le32w(ln + 26, 12);
    le16w(ln + 30, 1); le16w(ln + 32, 9); le16w(ln + 34, 2); le16w(ln + 36, 181); le16w(ln + 38, 12); le16w(ln + 40, 1);
    rl[2] = csi(r[2], "ARGB", 4, 2, 100, 1003, "Seta.png", tlv, sizeof tlv, 0, 0);
    memset(key[2], 0, 10); le16w(key[2] + 6, 20); le16w(key[2] + 8, 1);
    /* 4) dado bruto (um PNG fictício de 8 bytes) */
    unsigned char raw[12 + 8] = {'D', 'W', 'A', 'R'};
    le32w(raw + 8, 8); memcpy(raw + 12, "\x89PNG\r\n\x1a\n", 8);
    rl[3] = csi(r[3], "DATA", 1, 1, 100, 1000, "Dado.png", 0, 0, raw, sizeof raw);
    memset(key[3], 0, 10); le16w(key[3] + 6, 30); le16w(key[3] + 8, 1);
    int rk[4], rv[4];
    for (int i = 0; i < 4; i++) { rk[i] = add_block(key[i], 10); rv[i] = add_block(r[i], rl[i]); }
    int rends = tree_with(rk, rv, 4);
    /* índice e variáveis */
    long ioff = pos;
    be32w(buf + pos, nb); pos += 4;
    for (int i = 0; i < nb; i++) { be32w(buf + pos, addr[i]); be32w(buf + pos + 4, len_[i]); pos += 8; }
    long voff = pos;
    const char *vn[3] = {"FACETKEYS", "RENDITIONS", "KEYFORMAT"};
    int vb[3] = {facets, rends, kfb};
    be32w(buf + pos, 3); pos += 4;
    for (int i = 0; i < 3; i++) { be32w(buf + pos, vb[i]); buf[pos + 4] = (unsigned char)strlen(vn[i]); memcpy(buf + pos + 5, vn[i], strlen(vn[i])); pos += 5 + strlen(vn[i]); }
    memcpy(buf, "BOMStore", 8); be32w(buf + 8, 1); be32w(buf + 12, nb); be32w(buf + 16, ioff); be32w(buf + 20, voff - ioff); be32w(buf + 24, voff); be32w(buf + 28, pos - voff);

    car_file c;
    check(car_open(&c, buf, pos) && c.nkeys == 5, "abre o BOM e lê o KEYFORMAT");
    char nm[64];
    check(car_name_at(&c, 0, nm, sizeof nm) == 3 && !strcmp(nm, "Cor") && car_name_at(&c, 3, nm, sizeof nm) < 0, "lista os nomes");
    car_rendition rd; uint16_t k[32]; int nk;
    check(car_find(&c, "Cor", 0, 0, &rd, k, &nk) && rd.kind == CAR_COLOR && rd.ncolor == 4 && rd.color[0] == 0.25 && rd.color[2] == 0.75, "cor do catálogo");
    check(car_find(&c, "Dado", 0, 0, &rd, k, &nk) && rd.kind == CAR_DATA && rd.data_len == 8 && !memcmp(rd.data, "\x89PNG", 4), "dados brutos (RAWD)");
    static unsigned char scratch[1 << 16];
    car_set_scratch(scratch, sizeof scratch);
    uint32_t px[16];
    check(car_find(&c, "Seta", 200, 0, &rd, k, &nk) && rd.kind == CAR_BITMAP && rd.width == 4 && rd.height == 2, "imagem ligada ao atlas");
    check(car_bitmap_argb(&c, k, nk, 0, px, 4, 2) && px[0] == 0xFF0000FFu && px[7] == 0xFF0000FFu, "recorte do atlas (y contado de baixo: a metade azul)");
    check(!car_find(&c, "Nada", 0, 0, &rd, k, &nk), "nome ausente");
    printf("%d/%d\n", ok, total);
    return ok == total ? 0 : 1;
}
