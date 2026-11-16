/*
 * Teste de host do motor do ColorSync (userland/libsystem/colorsync.c, sem a parte CF).
 *   gcc -O1 -DCS_HOST -Iuserland/libsystem tools/colorsync_host.c userland/libsystem/colorsync.c -lm -o /tmp/colorsync_host
 *   python3 tools/colorsync_host.py      (compara com o LittleCMS do PIL nos perfis ICC do sistema)
 * Uso: colorsync_host srgb <arquivo.icc> v1 v2 v3 [v4]   → "r g b" (sRGB 0..1)
 *      colorsync_host write <nome> <saida.icc>            → grava o perfil embutido
 *      colorsync_host self                                  → verificações sem arquivo
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "colorsync.h"

static unsigned char *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long s = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc((size_t)s);
    if (fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return 0; }
    fclose(f);
    *n = (size_t)s;
    return b;
}
static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("FALHA %s\n", m); fails++; } else printf("ok   %s\n", m); } while (0)

int main(int argc, char **argv) {
    if (argc >= 6 && !strcmp(argv[1], "srgb")) {
        size_t n;
        unsigned char *d = slurp(argv[2], &n);
        cs_prof p;
        const char *it = getenv("CS_INTENT");
        if (!d || cs_parse_intent(d, n, it ? atoi(it) : 0, &p)) { fprintf(stderr, "não leu %s\n", argv[2]); return 2; }
        double in[4] = {atof(argv[3]), atof(argv[4]), atof(argv[5]), argc > 6 ? atof(argv[6]) : 0}, rgb[3];
        if (p.lab) { in[0] *= 100; in[1] = in[1] * 255 - 128; in[2] = in[2] * 255 - 128; }
        cs_to_srgb(&p, in, rgb);
        printf("%.6f %.6f %.6f\n", rgb[0], rgb[1], rgb[2]);
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "write")) {
        cs_prof p;
        unsigned char *o;
        size_t on;
        if (cs_builtin(argv[2], &p) || cs_write_icc(&p, &o, &on)) return 3;
        FILE *f = fopen(argv[3], "wb");
        fwrite(o, 1, on, f);
        fclose(f);
        return 0;
    }
    /* self */
    cs_prof s, p3, rt;
    cs_builtin("kCGColorSpaceSRGB", &s);
    CHECK(cs_is_srgb(&s), "sRGB embutido é sRGB");
    cs_builtin("kCGColorSpaceDisplayP3", &p3);
    CHECK(!cs_is_srgb(&p3), "Display P3 não é sRGB");
    double white[3] = {1, 1, 1}, w[3], red[3] = {1, 0, 0}, r[3];
    cs_to_srgb(&p3, white, w);
    CHECK(fabs(w[0] - 1) < 0.002 && fabs(w[1] - 1) < 0.002 && fabs(w[2] - 1) < 0.002, "branco do P3 → branco do sRGB");
    cs_to_srgb(&p3, red, r);
    CHECK(r[0] > 0.999 && r[1] < 0.01 && r[2] < 0.01, "vermelho do P3 sai do gamut do sRGB (recortado)");
    double xyz[3];
    cs_to_xyz(&s, white, xyz);
    CHECK(fabs(xyz[0] - 0.9642) < 0.001 && fabs(xyz[1] - 1) < 0.001 && fabs(xyz[2] - 0.8249) < 0.001, "branco do sRGB = D50 no PCS");
    unsigned char *icc;
    size_t n;
    CHECK(cs_write_icc(&s, &icc, &n) == 0 && cs_parse(icc, n, &rt) == 0 && cs_is_srgb(&rt), "sRGB gravado e lido de volta");
    unsigned char id[16];
    cs_profile_id(icc, n, id);
    CHECK(!memcmp(id, icc + 84, 16), "ID do perfil (MD5) gravado bate");
    unsigned char md[16];
    cs_md5((const unsigned char *)"abc", 3, md);
    CHECK(md[0] == 0x90 && md[1] == 0x01 && md[15] == 0x72, "MD5(\"abc\") = 900150983cd24fb0d6963f7d28e17f72");
    double gin[1] = {0.5}, g[3];
    cs_prof gr;
    cs_builtin("kCGColorSpaceGenericGrayGamma2_2", &gr);
    cs_to_srgb(&gr, gin, g);
    CHECK(fabs(g[0] - g[1]) < 1e-9 && g[0] > 0.45 && g[0] < 0.55, "cinza 2.2 meio-tom ≈ meio-tom do sRGB");
    cs_prof lab;
    memset(&lab, 0, sizeof lab);
    cs_lab(&lab, 0, 0);
    double L[3] = {100, 0, 0}, lw[3];
    cs_to_srgb(&lab, L, lw);
    CHECK(lw[0] > 0.998 && lw[1] > 0.998 && lw[2] > 0.998, "Lab L=100 → branco");
    double back[3], dev[3] = {0.2, 0.6, 0.9};
    cs_to_xyz(&p3, dev, xyz);
    cs_from_xyz(&p3, xyz, back);
    CHECK(fabs(back[0] - 0.2) < 1e-6 && fabs(back[1] - 0.6) < 1e-6 && fabs(back[2] - 0.9) < 1e-6, "P3 → XYZ → P3 ida e volta");
    printf("colorsync_host self: %s (%d falhas)\n", fails ? "FALHOU" : "ok", fails);
    return fails != 0;
}
