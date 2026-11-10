/* gcc -std=c11 -Wall -Wextra -DCX_AK_HOST -Iuserland/libsystem \
 *   userland/libsystem/appkit_public.c userland/tests-appkit/public_host.c -lm \
 *   -o /tmp/appkit-public-test && /tmp/appkit-public-test
 */
#include "appkit_public.h"
#include <stdio.h>
#include <string.h>

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}

static int has_sel(int id, const char *sel, void *ctx) {
    const char **map = ctx;
    return map[id] && strcmp(map[id], sel) == 0;
}

int main(void) {
    char parts[8][CX_PATH_PART];
    int n, next[4];
    const char *map[4];

    check(cx_step_apply(1, 0, 3, 1, 1) == 2, "stepper sobe");
    check(cx_step_apply(3, 0, 3, 1, 1) == 3, "stepper não passa do máximo");
    check(cx_step_apply(0, 0, 3, 1, 0) == 0, "stepper não passa do mínimo");
    check(cx_step_apply(1.5, 0, 10, 0.5, 0) == 1.0, "stepper desce meio passo");

    n = cx_path_split("/Users/thiago/db.sqlite", parts, 8);
    check(n == 3 && !strcmp(parts[0], "Users") && !strcmp(parts[2], "db.sqlite"), "caminho absoluto");
    n = cx_path_split("file:///tmp/a/", parts, 8);
    check(n == 2 && !strcmp(parts[0], "tmp") && !strcmp(parts[1], "a"), "file URL");
    check(cx_path_split("", parts, 8) == 0, "caminho vazio");

    check(!strcmp(cx_ax_role("NSButton"), "AXButton"), "papel do botão");
    check(!strcmp(cx_ax_role("NSTextField"), "AXTextField"), "papel do campo");
    check(!strcmp(cx_ax_role("NSWindow"), "AXWindow"), "papel da janela");
    check(!strcmp(cx_ax_role("NSStepper"), "AXSlider"), "papel do stepper");
    check(!strcmp(cx_ax_role("NSView"), "AXGroup"), "papel genérico");

    /* view 0 → superview 1 → janela 2 → app 3. Só a janela implementa performClose:. */
    next[0] = 1; next[1] = 2; next[2] = 3; next[3] = -1;
    map[0] = "mouseDown:"; map[1] = 0; map[2] = "performClose:"; map[3] = 0;
    check(cx_chain_index(next, 0, 4, has_sel, map, "performClose:") == 2, "cadeia acha a janela");
    check(cx_chain_index(next, 0, 4, has_sel, map, "missing:") == -1, "cadeia termina sem o seletor");

    printf("appkit-public: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
