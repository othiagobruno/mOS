/* Teste de host do leitor de QR (userland/frameworks/CoreImage/qrread.c) com o gerador qrgen.c. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../userland/frameworks/CoreImage/qrgen.h"
#include "../userland/frameworks/CoreImage/qrread.h"
static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("FALHOU %s\n", m); fails++; } } while (0)
static mos_qr_result res;
/* desenha a matriz com 4 módulos de margem, s px por módulo, girada de ang graus, com ruído de ±noise */
static uint8_t *render(const uint8_t *mat, int n, int s, double ang, int noise, int *W, int *H) {
    int side = (n + 8) * s;
    int W0 = (int)(side * 1.5), H0 = W0;
    uint8_t *img = malloc((size_t)W0 * H0);
    double c = cos(ang * M_PI / 180), sn = sin(ang * M_PI / 180), cx = W0 / 2.0, cy = H0 / 2.0;
    srand(7);
    for (int y = 0; y < H0; y++)
        for (int x = 0; x < W0; x++) {
            double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
            double u = (c * dx + sn * dy) / s + (n + 8) / 2.0 - 4, v = (-sn * dx + c * dy) / s + (n + 8) / 2.0 - 4;   /* em módulos */
            int mx = (int)floor(u), my = (int)floor(v);
            int dark = mx >= 0 && my >= 0 && mx < n && my < n && mat[my * n + mx];
            int g = dark ? 30 : 225;
            if (noise) g += rand() % (2 * noise + 1) - noise;
            img[(size_t)y * W0 + x] = (uint8_t)(g < 0 ? 0 : g > 255 ? 255 : g);
        }
    *W = W0; *H = H0;
    return img;
}
static int roundtrip(const char *msg, int level, int version, int s, double ang, int noise, int flips, const char *name) {
    uint8_t mat[177 * 177];
    int n = 0;
    int v = mos_qr_encode((const uint8_t *)msg, (int)strlen(msg), level, version, -1, mat, &n);
    if (v < 0) { printf("FALHOU gerar %s\n", name); fails++; return 0; }
    srand(99);
    for (int i = 0; i < flips; i++) { int x = 9 + rand() % (n - 18), y = 9 + rand() % (n - 18); mat[y * n + x] ^= 1; }   /* fora dos padrões */
    int W, H;
    uint8_t *img = render(mat, n, s, ang, noise, &W, &H);
    memset(&res, 0, sizeof res);
    int ok = mos_qr_read(img, W, H, &res);
    free(img);
    int good = ok && res.len == (int)strlen(msg) && !memcmp(res.text, msg, res.len) && res.version == v && res.level == level;
    if (!good) { printf("FALHOU %s (ok=%d v=%d/%d len=%d)\n", name, ok, res.version, v, res.len); fails++; }
    return good;
}
int main(void) {
    roundtrip("https://example.com", 1, 0, 4, 0, 0, 0, "v2 M reto");
    roundtrip("mOS QR", 0, 1, 6, 0, 0, 0, "v1 L");
    roundtrip("Olá, mundo! Câmera, áudio e vídeo no mOS.", 2, 0, 5, 12, 20, 0, "v4 Q girado 12°, ruído");
    roundtrip("0123456789abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghij", 3, 0, 4, -25, 10, 0, "H girado -25°");
    roundtrip("versão 7 com informação de versão: bits BCH(18,6) nos dois cantos do símbolo, lidos e conferidos.", 1, 7, 3, 0, 0, 0, "v7 com versão");
    roundtrip("erros corrigidos pelo Reed-Solomon", 3, 5, 4, 0, 0, 12, "v5 H com 12 módulos trocados");
    CHECK(res.corrected > 0, "Reed-Solomon corrigiu bytes");
    char big[600];
    for (int i = 0; i < 599; i++) big[i] = (char)('a' + i % 26);
    big[599] = 0;
    roundtrip(big, 0, 0, 3, 3, 0, 0, "v15+ byte longo");
    /* modos numérico, alfanumérico e kanji montados à mão (v1) */
    uint8_t d[19] = {0};
    int bit = 0;
#define PUT(val, nb) for (int _i = (nb) - 1; _i >= 0; _i--, bit++) if (((val) >> _i) & 1) d[bit >> 3] |= (uint8_t)(0x80 >> (bit & 7))
    PUT(1, 4); PUT(8, 10); PUT(12, 10); PUT(345, 10); PUT(67, 7);   /* "01234567" */
    PUT(2, 4); PUT(3, 9); PUT(10 * 45 + 36, 11); PUT(44, 6);       /* "A :" */
    PUT(8, 4); PUT(1, 8); PUT(0x0D9F, 13);                          /* kanji 0x935F */
    PUT(0, 4);
#undef PUT
    memset(&res, 0, sizeof res);
    CHECK(mos_qr_parse_data(d, 19, 1, &res) == 0 && res.len == 13 && !memcmp(res.text, "01234567A :", 11) && res.kanji &&
          res.text[11] == 0x93 && res.text[12] == 0x5F, "numérico, alfanumérico e kanji");
    /* imagem sem código */
    uint8_t blank[64 * 64];
    memset(blank, 200, sizeof blank);
    CHECK(!mos_qr_read(blank, 64, 64, &res), "imagem vazia não acha nada");
    printf(fails ? "qrread: %d falhas\n" : "qrread: ok\n", fails);
    return fails != 0;
}
