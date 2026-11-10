/* Teste de host do NSDockTile e do plugin de Dock.
 *   gcc -std=c11 -Wall -Wextra -DCX_DOCK_HOST -Iuserland/libsystem -Ikernel/ui \
 *       userland/libsystem/appkit_docktile.c userland/tests-appkit/docktile_host.c \
 *       -o /tmp/docktile-host && /tmp/docktile-host
 */
#include "appkit_docktile.h"
#include "dockbadge.h"
#include <stdio.h>
#include <string.h>

static int ok, total;
static char published[64];
static int publishes;

static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}

static void sink(const char *label, void *ctx) {
    (void)ctx;
    snprintf(published, sizeof published, "%s", label ? label : "");
    publishes++;
}

static void expect_dock(const char *label, int badge, const char *drawn) {
    int n = -1;
    char text[8];
    dock_badge_apply(&n, text, label);
    check(n == badge && strcmp(text, drawn) == 0, "o Dock desenha o selo publicado");
}

/* O CodexDockTilePlugin chama estes seletores em setDockTile:. Um seletor
 * que o tile não tem passa por cx_dock_unknown e não aborta. */
static void codex_plugin(void *plugin, void *tile) {
    static const char *need[] = {
        "setBadgeLabel:", "badgeLabel", "setContentView:", "contentView",
        "display", "setShowsApplicationBadge:", "showsApplicationBadge",
        "size", "owner", 0
    };
    double w = 0, h = 0;
    int i;
    (void)plugin;
    check(tile != 0, "setDockTile: recebe o tile");
    for (i = 0; need[i]; i++) check(cx_dock_selector(need[i]) == 1, need[i]);
    check(cx_dock_selector("setCustomIcon:") == 0, "seletor desconhecido não existe");
    check(cx_dock_unknown("setCustomIcon:") == 0, "seletor desconhecido não aborta");
    cx_dock_set_shows_app_badge(1);
    cx_dock_set_badge("7");
    cx_dock_set_content((void *)0x1);
    cx_dock_size(&w, &h);
    check(w == 128 && h == 128, "o tile mede 128 por 128");
    cx_dock_display();
}

int main(void) {
    void *view = (void *)0x42;
    cx_dock_reset();
    cx_dock_set_sink(sink, 0);
    cx_dock_set_owner((void *)0x11);

    check(cx_dock_shows_app_badge() == 1, "showsApplicationBadge nasce ligado");
    check(cx_dock_owner() == (void *)0x11, "o tile guarda o dono");

    cx_dock_set_badge("4");
    check(strcmp(cx_dock_badge(), "4") == 0 && strcmp(cx_dock_shown(), "4") == 0, "setBadgeLabel: publica 4 no Dock");
    check(cx_dock_displayed() == 1, "publicar marca o tile para desenhar");
    expect_dock(cx_dock_shown(), 4, "4");

    cx_dock_set_badge("NEW");
    check(strcmp(cx_dock_shown(), "NEW") == 0, "rótulo que não é número também vai ao Dock");
    expect_dock(cx_dock_shown(), 1, "NEW");

    cx_dock_set_shows_app_badge(0);
    check(cx_dock_shown()[0] == 0 && strcmp(cx_dock_badge(), "NEW") == 0, "showsApplicationBadge:NO esconde o selo e guarda o texto");
    expect_dock(cx_dock_shown(), 0, "");

    cx_dock_set_shows_app_badge(1);
    check(strcmp(cx_dock_shown(), "NEW") == 0, "ligar de novo mostra o selo guardado");

    cx_dock_set_badge("");
    check(cx_dock_shown()[0] == 0, "rótulo vazio tira o selo");
    expect_dock("0", 0, "");
    expect_dock("000", 0, "");
    expect_dock("12", 12, "12");

    cx_dock_set_content(view);
    check(cx_dock_content() == view, "contentView guarda a view");
    cx_dock_set_badge("3");
    publishes = 0;
    cx_dock_display();
    check(publishes == 1 && strcmp(published, "3") == 0, "display republica o selo");

    check(cx_dock_load_path(0) == 0, "caminho vazio não carrega");
    check(cx_dock_load_path("") == 0, "caminho vazio em string não carrega");
    check(cx_dock_load_path("/Aplicativos/ChatGPT.app/Contents/PlugIns/CodexDockTilePlugin.docktileplugin") == 0,
          "plugin ausente devolve 0 sem queda");

    check(cx_dock_load_plugin(0, 0) == 1, "plugin sem setDockTile: carrega sem queda");
    cx_dock_set_badge("");
    check(cx_dock_load_plugin(codex_plugin, (void *)0x99) == 1, "CodexDockTilePlugin carrega sem queda");
    check(strcmp(cx_dock_shown(), "7") == 0 && cx_dock_content() == (void *)0x1, "o plugin deixa o selo 7 no Dock");
    expect_dock(cx_dock_shown(), 7, "7");
    check(cx_dock_unknown_count() == 1 && cx_dock_load_count() == 2, "houve uma chamada desconhecida e dois carregamentos");

    printf("docktile-host: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
