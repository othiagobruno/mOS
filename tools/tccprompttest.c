#define _POSIX_C_SOURCE 200809L
/* gcc -std=c11 -Wall -Wextra -O1 tools/tccprompttest.c -o /tmp/tccprompttest && /tmp/tccprompttest
 * pending -> diálogo -> tcc_decide (miolo de cx_tcc_decide) -> estado no arquivo.
 * CGPreflightScreenCaptureAccess e AXIsProcessTrusted são esse preflight (cgevent.c, slack_frameworks.c). */
#define TCC_HOST 1
#include "../userland/libsystem/tcc_core.inc"
#include "../kernel/ui/tcc_prompt_core.inc"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int read_all(const char *path, char *dst, int cap) {
    FILE *f = fopen(path, "rb");
    int n;
    if (!f) { dst[0] = 0; return 0; }
    n = (int)fread(dst, 1, (size_t)cap - 1, f);
    fclose(f);
    dst[n] = 0;
    return n;
}

static void load_ui(const char *path, tcc_prompt_ui *ui) {
    char raw[32768];
    tcc_prompt_rec recs[TCC_PROMPT_CAP];
    int n;
    read_all(path, raw, (int)sizeof raw);
    n = tcc_prompt_parse(raw, recs, TCC_PROMPT_CAP);
    tcc_prompt_sync(ui, recs, n);
}

/* O que CGPreflightScreenCaptureAccess devolve: granted de kTCCServiceScreenCapture. */
static int CGPreflightScreenCaptureAccess_arquivo(const char *path, const char *bundle) {
    return tcc_preflight(path, bundle, TCC_PROMPT_SCREEN);
}
/* O que AXIsProcessTrusted devolve: granted de kTCCServiceAccessibility. */
static int AXIsProcessTrusted_arquivo(const char *path, const char *bundle) {
    return tcc_preflight(path, bundle, TCC_PROMPT_AX);
}

static int via_tcc_decide(const char *bundle, const char *service, int granted, void *ctx) {
    return tcc_decide((const char *)ctx, bundle, service, granted);
}

static int decide_calls;
static int decide_recusa(const char *bundle, const char *service, int granted, void *ctx) {
    (void)bundle; (void)service; (void)granted; (void)ctx;
    decide_calls++;
    return -1;
}

/* O clique chama tcc_decide e o texto gravado tem de ser o de tcc_prompt_set, que é o que o window server escreve. */
static void click(const char *path, tcc_prompt_ui *ui, int button) {
    char before[32768], expect[32768], after[32768];
    tcc_prompt_rec recs[TCC_PROMPT_CAP];
    int nb, n, g, m, na;
    nb = read_all(path, before, (int)sizeof before);
    CHECK(nb >= 0);
    n = tcc_prompt_parse(before, recs, TCC_PROMPT_CAP);
    g = tcc_prompt_granted(button);
    CHECK(g == 0 || g == 1);
    CHECK(tcc_prompt_set(recs, &n, TCC_PROMPT_CAP, ui->view.bundle, ui->view.service, g) == 0);
    m = tcc_prompt_format(expect, (int)sizeof expect, recs, n);
    CHECK(m > 0);
    CHECK(tcc_prompt_choose(ui, button, via_tcc_decide, (void *)path) == 0);
    CHECK(!ui->open);
    na = read_all(path, after, (int)sizeof after);
    if (m > 0 && (na != m || memcmp(after, expect, (size_t)m) != 0))
        fprintf(stderr, "arquivo:\n%.*s\nesperado:\n%.*s\n", na, after, m, expect);
    CHECK(m > 0 && na == m && memcmp(after, expect, (size_t)m) == 0);
}

static void roundtrip(const char *path) {
    char raw[32768], back[32768];
    tcc_prompt_rec recs[TCC_PROMPT_CAP];
    int nread, n, m;
    nread = read_all(path, raw, (int)sizeof raw);
    n = tcc_prompt_parse(raw, recs, TCC_PROMPT_CAP);
    m = tcc_prompt_format(back, (int)sizeof back, recs, n);
    CHECK(m == nread);
    CHECK(m >= 0 && memcmp(raw, back, (size_t)m) == 0);
}

int main(void) {
    char dir[] = "/tmp/mos-tcc-prompt-XXXXXX";
    char path[512];
    const char *b = "com.grok.bot";
    const char *outro = "com.mos.app";
    tcc_prompt_ui ui;
    tcc_prompt_ui de_novo;
    int hover;
    if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
    snprintf(path, sizeof path, "%s/decisions.txt", dir);

    CHECK((int)TCC_PROMPT_PENDING == (int)TCC_PENDING);
    CHECK((int)TCC_PROMPT_PENDING == 3);
    CHECK((int)TCC_PROMPT_DENIED == (int)TCC_DENIED);
    CHECK((int)TCC_PROMPT_GRANTED == (int)TCC_GRANTED);
    CHECK((int)TCC_PROMPT_MISSING == (int)TCC_MISSING);
    CHECK(TCC_PROMPT_KEY_ENTER == 10);
    CHECK(TCC_PROMPT_KEY_ESC == 0x1b);
    CHECK(!strcmp(TCC_PROMPT_FILE, "/Users/Visitante/Library/Application Support/com.mos.tcc/decisions.txt"));
    CHECK(tcc_prompt_kind("userNotifications") == TCC_PROMPT_KIND_NONE);
    CHECK(tcc_prompt_kind(TCC_PROMPT_SCREEN) == TCC_PROMPT_KIND_SCREEN);
    CHECK(tcc_prompt_kind(TCC_PROMPT_AX) == TCC_PROMPT_KIND_AX);
    CHECK(tcc_prompt_granted(0) == 0 && tcc_prompt_granted(1) == 1 && tcc_prompt_granted(2) == -1);

    memset(&ui, 0, sizeof ui);
    ui.hover = ui.pressed = -1;
    CHECK(tcc_preflight(path, b, TCC_PROMPT_SCREEN) == 0);
    CHECK(tcc_state_at(path, b, TCC_PROMPT_SCREEN) == TCC_MISSING);
    CHECK(CGPreflightScreenCaptureAccess_arquivo(path, b) == 0);
    CHECK(AXIsProcessTrusted_arquivo(path, b) == 0);
    CHECK(tcc_request(path, b, TCC_PROMPT_SCREEN) == 0);
    CHECK(tcc_state_at(path, b, TCC_PROMPT_SCREEN) == TCC_PENDING);
    roundtrip(path);
    load_ui(path, &ui);
    CHECK(ui.open);
    CHECK(ui.view.kind == TCC_PROMPT_KIND_SCREEN);
    CHECK(!strcmp(ui.view.app, "Bot"));
    CHECK(!strcmp(ui.view.eyebrow, "mOS"));
    CHECK(!strcmp(ui.view.deny, "Negar"));
    CHECK(!strcmp(ui.view.allow, "Permitir"));
    CHECK(strstr(ui.view.title, "gravar a tela do mOS") != 0);
    CHECK(strstr(ui.view.title, "Bot") == ui.view.title);
    CHECK(strstr(ui.view.message, b) != 0);
    CHECK(strstr(ui.view.title, "would like") == 0);
    CHECK(tcc_prompt_button_for_key(&ui, TCC_PROMPT_KEY_ENTER) == 1);
    CHECK(tcc_prompt_button_for_key(&ui, TCC_PROMPT_KEY_ESC) == 0);
    CHECK(tcc_prompt_button_for_key(&ui, 'a') == -1);
    hover = ui.hover;
    ui.hover = 1;
    ui.pressed = 0;
    load_ui(path, &ui);
    CHECK(ui.open && ui.hover == 1 && ui.pressed == 0);
    ui.hover = hover;

    decide_calls = 0;
    CHECK(tcc_prompt_choose(&ui, 2, decide_recusa, 0) == -1);
    CHECK(decide_calls == 0 && ui.open);
    CHECK(tcc_prompt_choose(&ui, 1, decide_recusa, (void *)path) == -1);
    CHECK(decide_calls == 1 && ui.open);
    CHECK(tcc_state_at(path, b, TCC_PROMPT_SCREEN) == TCC_PENDING);

    click(path, &ui, 1);
    CHECK(tcc_preflight(path, b, TCC_PROMPT_SCREEN) == 1);
    CHECK(tcc_request(path, b, TCC_PROMPT_SCREEN) == 1);
    CHECK(CGPreflightScreenCaptureAccess_arquivo(path, b) == 1);
    CHECK(AXIsProcessTrusted_arquivo(path, b) == 0);
    memset(&de_novo, 0, sizeof de_novo);
    de_novo.hover = de_novo.pressed = -1;
    load_ui(path, &de_novo);
    CHECK(!de_novo.open);
    CHECK(CGPreflightScreenCaptureAccess_arquivo(path, b) == 1);

    CHECK(tcc_request(path, b, TCC_PROMPT_AX) == 0);
    CHECK(tcc_state_at(path, b, TCC_PROMPT_AX) == TCC_PENDING);
    memset(&ui, 0, sizeof ui);
    ui.hover = ui.pressed = -1;
    load_ui(path, &ui);
    CHECK(ui.open);
    CHECK(ui.view.kind == TCC_PROMPT_KIND_AX);
    CHECK(strstr(ui.view.title, "controlar a interface do mOS") != 0);
    CHECK(strstr(ui.view.message, "acessibilidade") != 0);
    CHECK(CGPreflightScreenCaptureAccess_arquivo(path, b) == 1);
    click(path, &ui, 0);
    CHECK(tcc_state_at(path, b, TCC_PROMPT_AX) == TCC_DENIED);
    CHECK(AXIsProcessTrusted_arquivo(path, b) == 0);
    CHECK(tcc_request(path, b, TCC_PROMPT_AX) == 0);
    CHECK(tcc_state_at(path, b, TCC_PROMPT_AX) == TCC_DENIED);
    memset(&de_novo, 0, sizeof de_novo);
    load_ui(path, &de_novo);
    CHECK(!de_novo.open);
    CHECK(CGPreflightScreenCaptureAccess_arquivo(path, b) == 1);
    CHECK(AXIsProcessTrusted_arquivo(path, b) == 0);

    {
        char fresh[] = "/tmp/mos-tcc-prompt-b-XXXXXX";
        char p2[512];
        tcc_prompt_ui fila;
        CHECK(mkdtemp(fresh) != 0);
        snprintf(p2, sizeof p2, "%s/decisions.txt", fresh);
        CHECK(tcc_request(p2, b, TCC_PROMPT_SCREEN) == 0);
        CHECK(tcc_request(p2, b, TCC_PROMPT_AX) == 0);
        CHECK(tcc_request(p2, b, "userNotifications") == 0);
        memset(&fila, 0, sizeof fila);
        fila.hover = fila.pressed = -1;
        load_ui(p2, &fila);
        CHECK(fila.open && fila.view.kind == TCC_PROMPT_KIND_SCREEN);
        CHECK(strcmp(fila.view.service, "userNotifications") != 0);
        click(p2, &fila, 0);
        load_ui(p2, &fila);
        CHECK(fila.open && fila.view.kind == TCC_PROMPT_KIND_AX);
        CHECK(fila.hover == -1);
        click(p2, &fila, 1);
        load_ui(p2, &fila);
        CHECK(!fila.open);
        CHECK(CGPreflightScreenCaptureAccess_arquivo(p2, b) == 0);
        CHECK(AXIsProcessTrusted_arquivo(p2, b) == 1);
        CHECK(tcc_state_at(p2, b, "userNotifications") == TCC_PENDING);
        memset(&de_novo, 0, sizeof de_novo);
        load_ui(p2, &de_novo);
        CHECK(!de_novo.open);
        {
            FILE *f = fopen(p2, "wb");
            CHECK(f != 0);
            if (f) {
                fprintf(f, "# nota\n%s %s pending\n", outro, TCC_PROMPT_SCREEN);
                fclose(f);
            }
            memset(&fila, 0, sizeof fila);
            load_ui(p2, &fila);
            CHECK(fila.open);
            CHECK(!strcmp(fila.view.app, outro));
            CHECK(strstr(fila.view.title, "com.mos.app quer gravar a tela") == fila.view.title);
            click(p2, &fila, 1);
            CHECK(CGPreflightScreenCaptureAccess_arquivo(p2, outro) == 1);
            CHECK(CGPreflightScreenCaptureAccess_arquivo(p2, b) == 0);
        }
        {
            char raw[512];
            read_all(p2, raw, (int)sizeof raw);
            CHECK(strstr(raw, "#") == 0);
            CHECK(strstr(raw, "granted") != 0);
        }
        {
            char cmd[640];
            snprintf(cmd, sizeof cmd, "rm -rf '%s'", fresh);
            if (system(cmd) != 0) fprintf(stderr, "aviso: não apaguei %s\n", fresh);
        }
    }

    {
        char cmd[640];
        snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
        if (system(cmd) != 0) fprintf(stderr, "aviso: não apaguei %s\n", dir);
    }
    if (fails) { fprintf(stderr, "tccprompttest: %d falhas em %d\n", fails, checks); return 1; }
    printf("tccprompttest: %d/%d PASS\n", checks, checks);
    return 0;
}
