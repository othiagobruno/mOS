/* Teste de host da reprodução do lote de desenho. Roda no Linux, sem VM:
 *   gcc -std=c11 -Wall -Wextra -DCX_GCR_HOST -Iuserland/libsystem -Ikernel/include \
 *       userland/libsystem/appkit_gcreplay.c userland/tests-appkit/gcreplay_host.c \
 *       -o /tmp/appkit-gcreplay-test && /tmp/appkit-gcreplay-test
 * Monta o lote com os mesmos opcodes de compat_gui.h e confere o que chega ao destino. */
#include "appkit_gcreplay.h"
#include <stdio.h>
#include <string.h>

enum { GC_CLEAR = 1, GC_FILL, GC_RRECT, GC_RRECT_STROKE, GC_LINE, GC_POLY, GC_POLYLINE, GC_TEXT, GC_CLIP, GC_UNCLIP, GC_GRAD, GC_IMAGE, GC_COMP_FILL, GC_COMP_IMAGE };
static int ok, total;
static void check(int c, const char *w) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "  FALHA", w); }

static uint8_t buf[4096];
static uint32_t nb;
static void put(uint32_t op, const void *p, uint32_t n) { uint32_t len = 8 + n; memcpy(buf + nb, &op, 4); memcpy(buf + nb + 4, &len, 4); if (n) memcpy(buf + nb + 8, p, n); nb += len; }

static char log_[2048];
static void L(const char *s) { strcat(log_, s); strcat(log_, ";"); }
static char tmp[256];
static void s_fill(void *c, double x, double y, double w, double h, double r, uint32_t a) { (void)c; snprintf(tmp, sizeof tmp, "fill %g %g %g %g r%g %08x", x, y, w, h, r, a); L(tmp); }
static void s_stroke(void *c, double x, double y, double w, double h, double r, double t, uint32_t a) { (void)c; snprintf(tmp, sizeof tmp, "stroke %g %g %g %g r%g t%g %08x", x, y, w, h, r, t, a); L(tmp); }
static void s_line(void *c, double x0, double y0, double x1, double y1, double t, uint32_t a) { (void)c; snprintf(tmp, sizeof tmp, "line %g %g %g %g t%g %08x", x0, y0, x1, y1, t, a); L(tmp); }
static void s_poly(void *c, const double *xy, uint32_t n, uint32_t a, int f, double t) { (void)c; snprintf(tmp, sizeof tmp, "poly%s n%u %g,%g %g,%g t%g %08x", f ? "F" : "S", n, xy[0], xy[1], xy[2], xy[3], t, a); L(tmp); }
static void s_text(void *c, double x, double b, double sz, int w, uint32_t a, const char *s, uint32_t n) { (void)c; snprintf(tmp, sizeof tmp, "text %g %g %g w%d %08x %.*s", x, b, sz, w, a, (int)n, s); L(tmp); }
static void s_clip(void *c, double x, double y, double w, double h) { (void)c; snprintf(tmp, sizeof tmp, "clip %g %g %g %g", x, y, w, h); L(tmp); }
static void s_unclip(void *c) { (void)c; L("unclip"); }
static void s_image(void *c, double x, double y, double w, double h, const void *px, int pw, int ph, uint32_t op) { (void)c; uint32_t p0; memcpy(&p0, px, 4); snprintf(tmp, sizeof tmp, "image %g %g %g %g %dx%d op%u %08x", x, y, w, h, pw, ph, op, p0); L(tmp); }
static void s_comp(void *c, double x, double y, double w, double h, uint32_t a, uint32_t op) { (void)c; snprintf(tmp, sizeof tmp, "comp %g %g %g %g %08x op%u", x, y, w, h, a, op); L(tmp); }

int main(void) {
    gcr_sink_t s = {0, 0, s_fill, s_stroke, s_line, s_poly, s_text, s_clip, s_unclip, 0, s_image, s_comp};
    float rr[5] = {10, 20, 30, 40, 4}; uint32_t col = 0xFF112233; uint8_t b[64];
    memcpy(b, rr, 20); memcpy(b + 20, &col, 4); put(GC_RRECT, b, 24);
    float st[6] = {1, 2, 3, 4, 0, 2}; memcpy(b, st, 24); memcpy(b + 24, &col, 4); put(GC_RRECT_STROKE, b, 28);
    float ln[5] = {0, 0, 10, 10, 1}; memcpy(b, ln, 20); memcpy(b + 20, &col, 4); put(GC_LINE, b, 24);
    uint32_t np = 2; float xy[4] = {5, 5, 15, 25}; memcpy(b, &np, 4); memcpy(b + 4, &col, 4); memcpy(b + 8, xy, 16); put(GC_POLY, b, 24);
    float tx[3] = {7, 50, 13}; uint32_t tu[3] = {3, 0xFF000000, 2}; memcpy(b, tx, 12); memcpy(b + 12, tu, 12); memcpy(b + 24, "Oi", 2); put(GC_TEXT, b, 26);
    int32_t cl[4] = {0, 10, 50, 20}; put(GC_CLIP, cl, 16);
    put(GC_UNCLIP, 0, 0);
    int32_t im[4] = {2, 3, 1, 1}; uint32_t px = 0xFFABCDEF; memcpy(b, im, 16); memcpy(b + 16, &px, 4); put(GC_IMAGE, b, 20);
    float cf[4] = {0, 0, 5, 5}; uint32_t cfa[2] = {0x80FF0000, 1}; memcpy(b, cf, 16); memcpy(b + 16, cfa, 8); put(GC_COMP_FILL, b, 24);
    put(99, "x", 1);   /* desconhecido: pula */
    int n = gcr_replay(buf, nb, 100, 1, &s);
    check(n == 10, "reproduziu os 10 comandos (o desconhecido conta e é pulado)");
    check(strstr(log_, "fill 10 40 30 40 r4 ff112233") != 0, "retângulo arredondado: y virado (100 - 20 - 40)");
    check(strstr(log_, "stroke 1 94 3 4 r0 t2") != 0, "contorno virado");
    check(strstr(log_, "line 0 100 10 90 t1") != 0, "linha: pontos virados");
    check(strstr(log_, "polyF n2 5,95 15,75") != 0, "polígono virado");
    check(strstr(log_, "text 7 50 13 w3 ff000000 Oi") != 0, "texto: linha de base virada");
    check(strstr(log_, "clip 0 70 50 20") != 0 && strstr(log_, "unclip") != 0, "recorte virado e desfeito");
    check(strstr(log_, "image 2 96 1 1 1x1 op2 ffabcdef") != 0, "imagem com os pixels");
    check(strstr(log_, "comp 0 95 5 5 80ff0000 op1") != 0, "preenchimento com operação");
    log_[0] = 0;
    gcr_replay(buf, nb, 100, 0, &s);
    check(strstr(log_, "fill 10 20 30 40") != 0, "sem virar: coordenadas do lote");
    check(gcr_replay(buf, nb - 1, 100, 1, &s) == -1, "lote cortado no meio é recusado");
    uint8_t bad[16]; uint32_t op = GC_POLY, len = 16, big = 1000000; memcpy(bad, &op, 4); memcpy(bad + 4, &len, 4); memcpy(bad + 8, &big, 4); memcpy(bad + 12, &col, 4);
    check(gcr_replay(bad, 16, 100, 1, &s) == -1, "polígono com n maior que o lote é recusado");
    uint8_t bi[28]; op = GC_IMAGE; len = 28; int32_t d[4] = {0, 0, 100, 100}; memcpy(bi, &op, 4); memcpy(bi + 4, &len, 4); memcpy(bi + 8, d, 16);
    check(gcr_replay(bi, 28, 100, 1, &s) == -1, "imagem maior que os bytes é recusada");
    uint8_t z[8]; op = GC_UNCLIP; len = 4; memcpy(z, &op, 4); memcpy(z + 4, &len, 4);
    check(gcr_replay(z, 8, 100, 1, &s) == -1, "tamanho de comando menor que o cabeçalho é recusado");
    gcr_sink_t none = {0};
    check(gcr_replay(buf, nb, 100, 1, &none) == 10, "destino sem funções: só percorre");
    printf("appkit-gcreplay: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
