/* Fórmulas que o cua-driver passa a usar: mistura de pixel e token de acessibilidade.
 * Compila no Linux, sem o app e sem login:
 *   gcc -std=c11 -Wall -Wextra userland/tests-appkit/cuadriver_host.c -o /tmp/cuadriver-test && /tmp/cuadriver-test
 */
#include "../libsystem/cg_blend.h"
#include "../libsystem/ax_token.h"
#include <stdio.h>
#include <string.h>

static int ok, total;

static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}

int main(void) {
    uint32_t dst = 0xFF204060u;
    uint32_t src = 0xFF0000FFu;
    check(cx_cg_blend_px(0, src, dst) == src, "normal opaco cobre o destino");
    check(cx_cg_blend_px(0, 0x00000000u, dst) == dst, "normal transparente mantém o destino");
    check(cx_cg_blend_px(1, 0x00000000u, dst) == 0, "copy grava o pixel mesmo sem alfa");
    check(cx_cg_blend_px(16, src, dst) == 0, "clear zera o pixel");
    check(cx_cg_blend_px(2, 0xFFFFFFFFu, dst) == dst, "multiply por branco preserva");
    check(cx_cg_blend_px(2, 0xFF000000u, dst) == 0xFF000000u, "multiply por preto zera a cor");
    check(cx_cg_blend_px(3, 0xFF000000u, dst) == dst, "screen por preto preserva");
    check(cx_cg_blend_px(9, src, dst) == src, "modo desconhecido usa o normal");

    unsigned char tok[12];
    int pid = 0;
    unsigned wid = 0;
    memset(tok, 0, sizeof tok);
    memcpy(tok, "mAX1", 4);
    unsigned p = 42, w = 7;
    memcpy(tok + 4, &p, 4);
    memcpy(tok + 8, &w, 4);
    check(cx_ax_token_read(tok, 12, &pid, &wid) && pid == 42 && wid == 7, "token mAX1 devolve pid e janela");
    check(!cx_ax_token_read(tok, 11, &pid, &wid), "token curto é rejeitado");
    tok[0] = 'X';
    check(!cx_ax_token_read(tok, 12, &pid, &wid), "token sem mAX1 é rejeitado");

    printf("cuadriver: %d/%d %s\n", ok, total, ok == total ? "ok" : "FALHA");
    return ok == total ? 0 : 1;
}
