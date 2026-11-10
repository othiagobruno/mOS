/* Host: pixels do CoreImage contra valores calculados à parte.
 * O teste liga o imageio_decode.c só para fornecer mos_img_decode. O framework não o define.
 * cc -O2 tools/test-coreimage.c userland/frameworks/CoreImage/coreimage_cpu.c \
 *    userland/frameworks/ImageIO/imageio_decode.c -lm -I userland/frameworks/CoreImage -o /tmp/test-coreimage
 */
#include "coreimage_cpu.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
static void expect(int cond, const char *msg) {
    if (!cond) { fprintf(stderr, "FALHOU: %s\n", msg); fails++; }
    else printf("ok %s\n", msg);
}

static int iround(double v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (int)(v + 0.5);
}

static uint32_t color_ref(uint32_t p, double sat, double bri, double con) {
    double r = ((p >> 16) & 255) / 255.0;
    double g = ((p >> 8) & 255) / 255.0;
    double b = (p & 255) / 255.0;
    r += bri; g += bri; b += bri;
    r = (r - 0.5) * con + 0.5;
    g = (g - 0.5) * con + 0.5;
    b = (b - 0.5) * con + 0.5;
    double l = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    r = l + sat * (r - l);
    g = l + sat * (g - l);
    b = l + sat * (b - l);
    return 0xFF000000u | ((uint32_t)iround(r * 255.0) << 16) | ((uint32_t)iround(g * 255.0) << 8) | (uint32_t)iround(b * 255.0);
}

static uint32_t over_ref(uint32_t s, uint32_t d) {
    double sa = ((s >> 24) & 255) / 255.0;
    double da = ((d >> 24) & 255) / 255.0;
    double oa = sa + da * (1.0 - sa);
    if (oa <= 0) return 0;
    double sc[3] = { ((s >> 16) & 255) / 255.0, ((s >> 8) & 255) / 255.0, (s & 255) / 255.0 };
    double dc[3] = { ((d >> 16) & 255) / 255.0, ((d >> 8) & 255) / 255.0, (d & 255) / 255.0 };
    int ch[3];
    for (int i = 0; i < 3; i++) ch[i] = iround(((sc[i] * sa + dc[i] * da * (1.0 - sa)) / oa) * 255.0);
    return ((uint32_t)iround(oa * 255.0) << 24) | ((uint32_t)ch[0] << 16) | ((uint32_t)ch[1] << 8) | (uint32_t)ch[2];
}

/* PNG 2x2 do teste do ImageIO: R, G 50%, B, branco. */
static const unsigned char rgba_png[] = {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,2,8,6,0,0,0,114,182,13,36,0,0,0,20,73,68,65,84,120,156,99,248,207,192,240,31,8,27,24,128,52,24,0,0,68,210,9,120,182,135,35,75,0,0,0,0,73,69,78,68,174,66,96,130};

int main(void) {
    uint32_t src[4] = { 0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu, 0xFFFFFFFFu };
    mos_ci *im = mos_ci_bitmap(src, 2, 2, 0, 0);
    uint32_t out[4];
    expect(mos_ci_render(im, 0, 0, 2, 2, out) == 0 && !memcmp(out, src, sizeof src), "bitmap igual");

    mos_ci *crop = mos_ci_crop(im, 1, 0, 1, 2);
    double x, y, w, h;
    expect(mos_ci_extent(crop, &x, &y, &w, &h) && x == 1 && y == 0 && w == 1 && h == 2, "extent do corte");
    uint32_t col[2];
    expect(mos_ci_render(crop, 1, 0, 1, 2, col) == 0 && col[0] == 0xFF00FF00u && col[1] == 0xFFFFFFFFu, "corte coluna da direita");

    mos_ci *gray = mos_ci_color(im, 0, 0, 1);
    uint32_t gpx[4];
    mos_ci_render(gray, 0, 0, 2, 2, gpx);
    int cok = 1;
    for (int i = 0; i < 4; i++) if (gpx[i] != color_ref(src[i], 0, 0, 1)) cok = 0;
    expect(cok && gpx[0] == color_ref(0xFFFF0000u, 0, 0, 1), "saturação 0 vira luminância");
    expect(color_ref(0xFFFF0000u, 0, 0, 1) != 0xFFFF0000u, "vermelho saturado muda");

    uint32_t black = 0xFF000000u;
    mos_ci *blk = mos_ci_bitmap(&black, 1, 1, 0, 0);
    mos_ci *bright = mos_ci_color(blk, 1, 0.5, 1);
    uint32_t bp;
    mos_ci_render(bright, 0, 0, 1, 1, &bp);
    expect(bp == color_ref(black, 1, 0.5, 1) && (bp & 0xFFFFFFu) == 0x808080u, "brilho 0,5 no preto");

    uint32_t fg = 0x80FF0000u, bg = 0xFF0000FFu;
    mos_ci *fgi = mos_ci_bitmap(&fg, 1, 1, 0, 0);
    mos_ci *bgi = mos_ci_bitmap(&bg, 1, 1, 0, 0);
    mos_ci *over = mos_ci_over(fgi, bgi);
    uint32_t op;
    mos_ci_render(over, 0, 0, 1, 1, &op);
    expect(op == over_ref(fg, bg), "source-over vermelho 128 sobre azul");
    expect(op != fg && op != bg, "source-over não é cópia");

    uint32_t white = 0xFFFFFFFFu;
    mos_ci *dot = mos_ci_bitmap(&white, 1, 1, 0, 0);
    mos_ci *blur = mos_ci_blur(dot, 1);
    expect(mos_ci_extent(blur, &x, &y, &w, &h) && w == 7 && h == 7 && x == -3 && y == -3, "desfoque alarga o extent");
    uint32_t tile[49];
    expect(mos_ci_render(blur, -3, -3, 7, 7, tile) == 0, "render do desfoque");
    int rad = 3;
    double wt[7], sum = 0;
    for (int i = -rad; i <= rad; i++) { wt[i + rad] = exp(-0.5 * (double)(i * i)); sum += wt[i + rad]; }
    for (int i = 0; i < 7; i++) wt[i] /= sum;
    int center_a = iround(wt[3] * wt[3] * 255.0);
    int corner_a = (tile[0] >> 24) & 255;
    int mid_a = (tile[3 * 7 + 3] >> 24) & 255;
    expect(mid_a == center_a && (tile[3 * 7 + 3] & 0xFFFFFFu) == 0xFFFFFFu, "centro do gaussiano");
    expect(corner_a == iround(wt[0] * wt[0] * 255.0) && corner_a < mid_a, "canto do gaussiano mais fraco");

    mos_ci *same = mos_ci_lanczos(im, 1, 1);
    uint32_t idn[4];
    mos_ci_render(same, 0, 0, 2, 2, idn);
    expect(!memcmp(idn, src, sizeof src), "lanczos escala 1 é a própria imagem");

    uint32_t solid = 0xFF112233u;
    uint32_t plate[64];
    for (int i = 0; i < 64; i++) plate[i] = solid;
    mos_ci *flat = mos_ci_bitmap(plate, 8, 8, 0, 0);
    mos_ci *up = mos_ci_lanczos(flat, 2, 1);
    expect(mos_ci_extent(up, &x, &y, &w, &h) && w == 16 && h == 16, "lanczos dobra o tamanho");
    uint32_t center;
    mos_ci_render(up, 8, 8, 1, 1, &center);
    expect(center == solid, "lanczos no miolo de cor chapada");

    mos_ci *shift = mos_ci_affine(im, 1, 0, 0, 1, 2, 0);
    uint32_t sh[4];
    expect(mos_ci_extent(shift, &x, &y, &w, &h) && x == 2, "translação anda o extent");
    mos_ci_render(shift, 2, 0, 2, 2, sh);
    expect(!memcmp(sh, src, sizeof src), "translação inteira preserva pixels");

    uint8_t raw[8];
    mos_ci *fmt = mos_ci_bitmap(&fg, 1, 1, 0, 0);
    expect(mos_ci_render_format(fmt, 0, 0, 1, 1, MOS_CI_RGBA8, raw, 4) == 0, "render RGBA8");
    expect(raw[0] == 255 && raw[1] == 0 && raw[2] == 0 && raw[3] == 128, "bytes RGBA8");

    mos_ci *dec = mos_ci_from_encoded(rgba_png, sizeof rgba_png);
    uint32_t decpx[4];
    uint32_t want[4] = { 0xFFFF0000u, 0x8000FF00u, 0xFF0000FFu, 0xFFFFFFFFu };
    expect(dec && mos_ci_render(dec, 0, 0, 2, 2, decpx) == 0 && !memcmp(decpx, want, sizeof want), "PNG do ImageIO vira CIImage");
    expect(mos_ci_from_encoded((const uint8_t *)"GIF89a", 6) == 0, "GIF não decodifica");

    mos_ci *byname = mos_ci_apply("CIGaussianBlur", dot, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0);
    uint32_t bypx[49];
    expect(byname && mos_ci_render(byname, -3, -3, 7, 7, bypx) == 0 && !memcmp(bypx, tile, sizeof tile),
           "CIFilter CIGaussianBlur pelo nome");
    expect(mos_ci_apply("CIComicEffect", im, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0) == 0, "filtro desconhecido é nil");
    mos_ci_release(byname);

    mos_ci_release(im); mos_ci_release(crop); mos_ci_release(gray); mos_ci_release(blk);
    mos_ci_release(bright); mos_ci_release(fgi); mos_ci_release(bgi); mos_ci_release(over);
    mos_ci_release(dot); mos_ci_release(blur); mos_ci_release(same); mos_ci_release(flat);
    mos_ci_release(up); mos_ci_release(shift); mos_ci_release(fmt); mos_ci_release(dec);
    if (fails) { fprintf(stderr, "%d falhas\n", fails); return 1; }
    printf("tudo ok\n");
    return 0;
}
