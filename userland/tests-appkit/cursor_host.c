/* Teste de host de userland/libsystem/cursor_core.c.
 * gcc -O1 -Wall -Wextra -I userland/libsystem userland/tests-appkit/cursor_host.c userland/libsystem/cursor_core.c */
#include <stdio.h>
#include <stdint.h>
#include "cursor_core.h"

static int fails, total;
#define CHECK(c, m) do { total++; if (!(c)) { fails++; printf("FALHOU: %s\n", m); } } while (0)

static uint32_t px(const uint32_t *o, int x, int y) { return o[y * CUR_PX + x]; }
static int alpha(uint32_t p) { return (int)(p >> 24); }
static int red(uint32_t p) { return (int)((p >> 16) & 255); }
static int green(uint32_t p) { return (int)((p >> 8) & 255); }

int main(void) {
    uint32_t o[CUR_PX * CUR_PX];
    int hx, hy;

    CHECK(!cur_draw(99, o, &hx, &hy), "forma desconhecida devolve 0");

    CHECK(cur_draw(CUR_NOT_ALLOWED, o, &hx, &hy) && hx == 2 && hy == 1, "proibido: ponto quente na ponta da seta");
    CHECK(alpha(px(o, 3, 8)) == 255 && red(px(o, 3, 8)) < 40, "proibido: miolo da seta preto");
    CHECK(alpha(px(o, 1, 8)) > 100 && red(px(o, 1, 8)) > 200, "proibido: contorno branco à esquerda da seta");
    { uint32_t ring = px(o, 23, 17); CHECK(alpha(ring) == 255 && red(ring) > 200 && green(ring) < 90, "proibido: anel vermelho"); }
    { uint32_t mid = px(o, 23, 23); CHECK(red(mid) > 200 && green(mid) < 90, "proibido: barra vermelha no centro"); }
    { uint32_t hole = px(o, 21, 26); CHECK(red(hole) > 200 && green(hole) > 200, "proibido: branco dentro do anel, fora da barra"); }
    CHECK(alpha(px(o, 31, 0)) == 0 && alpha(px(o, 0, 31)) == 0, "proibido: cantos transparentes");

    CHECK(cur_draw(CUR_LINK, o, &hx, &hy) && hx == 2, "atalho desenhado");
    CHECK(red(px(o, 16, 23)) < 40 && alpha(px(o, 16, 23)) > 200, "atalho: borda preta do quadrado");
    CHECK(red(px(o, 28, 28)) > 200, "atalho: fundo branco do quadrado");
    CHECK(red(px(o, 26, 23)) < 60, "atalho: ponta da flecha preta");

    CHECK(cur_draw(CUR_CONTEXT, o, &hx, &hy), "menu de contexto desenhado");
    CHECK(red(px(o, 23, 18)) < 60 && red(px(o, 23, 20)) > 200, "menu: linha preta e espaço branco");

    CHECK(cur_draw(CUR_POOF, o, &hx, &hy) && hx == 16 && hy == 16, "sumir: ponto quente no centro");
    CHECK(alpha(px(o, 16, 13)) == 255 && red(px(o, 16, 13)) == 255, "sumir: nuvem branca");
    CHECK(alpha(px(o, 2, 2)) == 0, "sumir: canto vazio");

    CHECK(cur_draw(CUR_IBEAM_VERTICAL, o, &hx, &hy) && hx == 16 && hy == 16, "I-beam vertical: ponto quente no centro");
    CHECK(alpha(px(o, 16, 16)) == 255 && red(px(o, 16, 16)) < 60, "I-beam vertical: haste preta");
    CHECK(red(px(o, 16, 15)) > 180 && alpha(px(o, 16, 15)) > 100, "I-beam vertical: contorno branco");
    CHECK(red(px(o, 9, 12)) < 60 && red(px(o, 23, 20)) < 60, "I-beam vertical: serifas pretas");
    CHECK(alpha(px(o, 16, 8)) == 0, "I-beam vertical: fora da forma vazio");

    /* redução: 64x32 vermelho opaco com metade transparente; ponto quente acompanha a escala */
    static uint32_t big[64 * 32];
    for (int y = 0; y < 32; y++) for (int x = 0; x < 64; x++) big[y * 64 + x] = x < 32 ? 0xFFFF0000u : 0;
    uint32_t sm[CUR_PX * CUR_PX];
    int w, h;
    double fx = 40, fy = 10;
    CHECK(cur_fit(big, 64, 32, sm, &w, &h, &fx, &fy) && w == 32 && h == 16, "redução mantém a proporção (64x32 -> 32x16)");
    CHECK(fx > 19.9 && fx < 20.1 && fy > 4.9 && fy < 5.1, "ponto quente na mesma escala");
    CHECK(sm[0] == 0xFFFF0000u && sm[31] == 0, "cor não pré-multiplicada preservada");
    CHECK(cur_fit(big, 16, 8, sm, &w, &h, 0, 0) && w == 16 && h == 8, "imagem pequena não é ampliada");
    CHECK(!cur_fit(0, 4, 4, sm, &w, &h, 0, 0), "sem imagem devolve 0");
    /* média de área: borda meio a meio entre vermelho opaco e transparente fica com alfa 128 e cor vermelha */
    static uint32_t two[2 * 1] = {0xFFFF0000u, 0};
    static uint32_t wide[64];
    for (int x = 0; x < 64; x++) wide[x] = two[x & 1];
    CHECK(cur_fit(wide, 64, 1, sm, &w, &h, 0, 0) && w == 32 && h == 1 && (sm[0] >> 24) == 128 && red(sm[0]) == 255, "média de área sem escurecer");

    printf("cursor_host: %d/%d\n", total - fails, total);
    return fails != 0;
}
