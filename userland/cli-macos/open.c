/*
 * mOS — /usr/bin/open (Mach-O): abre URLs, arquivos e apps como o open(1) do macOS, pelo LaunchServices do mOS
 * (userland/libsystem/openurl.c). É o que o Terminal, os scripts e os testes usam para mandar slack://... ou
 * claude://... a um app, ou https://... ao navegador.
 *   open <URL | arquivo | App.app> ...     cada alvo com o app que o atende (esquema em CFBundleURLSchemes)
 *   open -a <App> [alvo ...]               com o app escolhido: nome ("Slack"), "Slack.app" ou caminho
 *   open -b <identificador> [alvo ...]     idem, pelo CFBundleIdentifier
 * Se o app não está aberto, ele é iniciado e a URL chega assim que ele termina de abrir (até 2 min).
 * Aceita e ignora -W, -n, -g, -j, -F, -R (o mOS ainda não os distingue); tudo depois de --args vai para o app
 * só no macOS (aqui, ignorado). Sai com 1 se algum alvo não pôde ser aberto.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int cx_open_target(const char *target, const char *app, int by_ident, int timeout_ms);   /* libSystem (SPI do mOS) */

static void usage(void) {
    fprintf(stderr, "uso: open [-a App | -b identificador] [URL | arquivo | App.app ...]\n");
}

int main(int argc, char **argv) {
    const char *app = 0;
    int by_ident = 0, n = 0, err = 0;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if ((!strcmp(a, "-a") || !strcmp(a, "-b")) && i + 1 < argc) { by_ident = a[1] == 'b'; app = argv[++i]; continue; }
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(); return 0; }
        if (!strcmp(a, "--args")) break;
        if (a[0] == '-' && a[1] && !a[2]) continue;   /* -W -n -g -j -F -R ... */
        n++;
        /* caminho relativo: absoluto, para virar file://... */
        char full[2048];
        int is_url = 0;
        for (const char *s = a; *s && *s != '/'; s++) if (*s == ':') { is_url = s > a + 1; break; }
        if (!is_url && a[0] != '/' && strlen(a) < 1024 && getcwd(full, 1000)) {
            size_t l = strlen(full);
            snprintf(full + l, sizeof full - l, "/%s", a);
            a = full;
        }
        if (cx_open_target(a, app, by_ident, 120000) != 0) {
            fprintf(stderr, "open: não foi possível abrir %s\n", argv[i]);
            err = 1;
        }
    }
    if (!n) {
        if (!app) { usage(); return 1; }
        err = cx_open_target(0, app, by_ident, 120000) != 0;
        if (err) fprintf(stderr, "open: não foi possível abrir o app %s\n", app);
    }
    return err;
}
