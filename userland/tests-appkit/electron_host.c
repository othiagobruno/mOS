/* gcc -std=c11 -Wall -Wextra userland/tests-appkit/electron_host.c -o /tmp/electron-host && /tmp/electron-host */
#include "../libsystem/appkit_electron.h"
#include <stdio.h>
#include <string.h>

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}

int main(void) {
    char b[32];
    double w;
    check(cx_textlist_marker("{decimal}.", 3, b, 32) > 0 && !strcmp(b, "3."), "marcador decimal");
    check(cx_textlist_marker("-", 2, b, 32) > 0 && !strcmp(b, "-"), "marcador hífen");
    cx_text_fragment(100, 5, 200, &w);
    check(w == 90, "a linha cabe na largura do container");
    cx_text_fragment(40, 5, 10, &w);
    check(w == 10, "uma linha curta não estica");
    check(CX_FONT_WIDTH_EXPANDED == 0.4, "NSFontWidthExpanded");
    printf("electron-appkit: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
