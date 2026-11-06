/* mOS — Instalar mOS: copia o sistema em execução (pendrive) para um disco interno (SATA/NVMe/USB) */
#include "apps.h"
#include "storage.h"

enum { ST_INTRO, ST_PICK, ST_CONFIRM, ST_RUN, ST_DONE, ST_FAIL };

static window_t *iw;
static int step = ST_INTRO, sel = -1;
static char why[160];

static void icon_instalador_draw(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    icon_disk(s, x, y + (int)(8 * u), sz);
    /* seta para baixo entrando no disco */
    float cx = x + 32 * u;
    gfx_circle(s, cx, y + 16 * u, 13 * u, RGBA(0, 0, 0, 50));
    gfx_round_rect_grad(s, cx - 13 * u, y + 2 * u, 26 * u, 26 * u, 13 * u, RGB(90, 170, 255), RGB(20, 110, 230));
    gfx_line(s, cx, y + 8 * u, cx, y + 21 * u, 2.6f * u, RGB(255, 255, 255));
    gfx_line(s, cx - 5.5f * u, y + 16 * u, cx, y + 21.5f * u, 2.6f * u, RGB(255, 255, 255));
    gfx_line(s, cx + 5.5f * u, y + 16 * u, cx, y + 21.5f * u, 2.6f * u, RGB(255, 255, 255));
}

static void fmt_bytes(u64 b, char *o, int n) {
    if (b >= 1000000000ULL) snprintf(o, n, "%.1f GB", b / 1e9);
    else snprintf(o, n, "%llu MB", b / 1000000);
}

static void footer(ui_t *u, const char *left, bool left_on, const char *right, bool right_on, bool primary, int *clicked) {
    *clicked = 0;
    gfx_fill(u->s, 0, u->ch - 54, u->cw, 1, T.separator);
    if (left && ui_toolbar_button(u, 9101, rect_make(u->cw - 250, u->ch - 40, 110, 26), left, left_on)) *clicked = 1;
    if (right) {
        rect_t r = rect_make(u->cw - 130, u->ch - 40, 110, 26);
        if (right_on) { if (ui_button(u, 9102, r, right, primary)) *clicked = 2; }
        else ui_toolbar_button(u, 9103, r, right, false);
    }
}

static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    int top = w->toolbar_h + 8;
    gfx_fill(s, 0, top - 8, u->cw, u->ch - top + 8, T.window_bg);
    /* coluna esquerda: etapas */
    static const char *steps[] = {"Introdução", "Destino", "Confirmação", "Instalação", "Resumo"};
    int cur = step == ST_FAIL ? 3 : step > ST_DONE ? 4 : step;
    gfx_fill(s, 0, top - 8, 190, u->ch - top + 8, T.dark ? RGBA(255, 255, 255, 8) : RGBA(0, 0, 0, 8));
    for (int i = 0; i < 5; i++) {
        int y = top + 20 + i * 30;
        u32 c = i == cur ? T.accent : i < cur ? RGB(60, 190, 90) : T.text3;
        gfx_circle(s, 30, y + 8, 6, c);
        if (i < cur) glyph_check(s, 24, y + 2, 12, RGB(255, 255, 255));
        ui_label(u, 46, y, steps[i], i == cur ? FONT_SEMIBOLD : FONT_REGULAR, FONT_UI, i == cur ? T.text : T.text2);
    }
    int x0 = 216, cw = u->cw - x0 - 24;
    int clicked;
    if (step == ST_INTRO) {
        icon_instalador_draw(s, x0, top + 10, 72);
        ui_label(u, x0 + 90, top + 22, "Bem-vindo ao Instalador do mOS", FONT_BOLD, 18, T.text);
        ui_label(u, x0 + 90, top + 48, MOS_NAME " " MOS_VERSION, FONT_REGULAR, FONT_UI, T.text2);
        const char *txt =
            "Este assistente copia o mOS em execução — incluindo seus documentos, configurações e "
            "arquivos criados nesta sessão — para um disco interno ou outro pendrive. Depois da "
            "instalação, o computador poderá iniciar o mOS sem o pendrive original.\n\n"
            "Atenção: o disco de destino será apagado por completo e recebe uma tabela GPT com uma "
            "partição de sistema EFI (FAT32). Faça backup antes de continuar.";
        text_wrap(s, font_get(FONT_REGULAR), 13, rect_make(x0, top + 110, cw, 300), 19, T.text, txt, true);
        if (persist_disk_index() < 0)
            text_wrap(s, font_get(FONT_MEDIUM), 12.5f, rect_make(x0, top + 270, cw, 60), 17, RGB(220, 120, 0),
                      "O sistema está rodando só na RAM (o disco de origem não foi encontrado). A instalação não é possível neste modo.", true);
        footer(u, NULL, false, "Continuar", persist_disk_index() >= 0, true, &clicked);
        if (clicked == 2) { step = ST_PICK; sel = -1; }
    } else if (step == ST_PICK) {
        ui_label(u, x0, top + 14, "Selecione o disco onde o mOS será instalado", FONT_BOLD, 16, T.text);
        int n = storage_disk_count(), y = top + 52;
        int src = persist_disk_index();
        for (int i = 0; i < n; i++) {
            rect_t r = rect_make(x0, y, cw, 58);
            char reason[128];
            bool ok = install_can_target(i, reason, sizeof reason);
            bool hov = ui_hit(u, r);
            u32 bg = sel == i ? WITH_ALPHA(T.accent, 40) : hov && ok ? (T.dark ? RGBA(255, 255, 255, 14) : RGBA(0, 0, 0, 8)) : 0;
            if (bg) gfx_round_rect(s, r.x, r.y, r.w, r.h, 8, bg);
            gfx_round_rect_stroke(s, r.x, r.y, r.w, r.h, 8, sel == i ? 2 : 1, sel == i ? T.accent : T.separator);
            icon_disk(s, r.x + 10, r.y + 5, 48);
            char title[128];
            snprintf(title, sizeof title, "%s%s", storage_disk_desc(i), i == src ? "  (sistema em execução)" : "");
            ui_label(u, r.x + 70, r.y + 10, title, FONT_SEMIBOLD, FONT_UI, ok ? T.text : T.text3);
            char sub[200], sz[32];
            blkdev_t *bd = storage_disk(i);
            fmt_bytes(bd ? bd->size : 0, sz, sizeof sz);
            snprintf(sub, sizeof sub, "%s · %s%s%s", storage_disk_name(i), sz, ok ? "" : " — ", ok ? "" : reason);
            text_draw_ellipsis(s, font_get(FONT_REGULAR), 12, r.x + 70, r.y + 44, r.w - 80, ok ? T.text2 : RGB(200, 90, 60), sub);
            if (ok && ui_clicked(u, 9200 + i, r)) { sel = i; }
            y += 66;
        }
        if (!n) ui_label(u, x0, y, "Nenhum disco encontrado (SATA, NVMe ou USB).", FONT_REGULAR, FONT_UI, T.text2);
        footer(u, "Voltar", true, "Continuar", sel >= 0, true, &clicked);
        if (clicked == 1) step = ST_INTRO;
        if (clicked == 2 && sel >= 0) step = ST_CONFIRM;
    } else if (step == ST_CONFIRM) {
        ui_label(u, x0, top + 14, "Apagar o disco e instalar?", FONT_BOLD, 16, T.text);
        char msg[400], sz[32];
        blkdev_t *bd = storage_disk(sel);
        fmt_bytes(bd ? bd->size : 0, sz, sizeof sz);
        snprintf(msg, sizeof msg,
                 "Todo o conteúdo de \"%s\" (%s, %s) será apagado permanentemente. Uma nova tabela de partições "
                 "GPT será criada com a partição \"MOS\" (FAT32), e o mOS será copiado para ela.\n\n"
                 "Esta ação não pode ser desfeita.",
                 storage_disk_desc(sel), storage_disk_name(sel), sz);
        gfx_round_rect(s, x0, top + 52, cw, 150, 8, T.dark ? RGBA(255, 80, 60, 30) : RGBA(255, 80, 60, 22));
        text_wrap(s, font_get(FONT_REGULAR), 13, rect_make(x0 + 16, top + 66, cw - 32, 140), 19, T.text, msg, true);
        footer(u, "Voltar", true, "Apagar e Instalar", true, true, &clicked);
        if (clicked == 1) step = ST_PICK;
        if (clicked == 2) {
            if (install_can_target(sel, why, sizeof why) && install_start(sel)) step = ST_RUN;
            else { if (!why[0]) strcpy(why, install_status()->error); step = ST_FAIL; }
        }
    } else if (step == ST_RUN) {
        const install_status_t *st = install_status();
        ui_label(u, x0, top + 14, "Instalando o mOS…", FONT_BOLD, 16, T.text);
        ui_label(u, x0, top + 50, st->step, FONT_REGULAR, FONT_UI, T.text2);
        ui_progress(u, rect_make(x0, top + 78, cw, 8), st->percent / 100.0f);
        char l[128], a[32], b[32];
        fmt_bytes(st->bytes_done, a, sizeof a);
        fmt_bytes(st->bytes_total, b, sizeof b);
        snprintf(l, sizeof l, "%d%% — %d de %d arquivos (%s de %s)", st->percent, st->files_done, st->files_total, a, b);
        ui_label(u, x0, top + 96, l, FONT_REGULAR, 12, T.text2);
        ui_label(u, x0, top + 140, "Não desligue o computador nem remova discos.", FONT_MEDIUM, 12.5f, T.text);
        ui_spinner(u, x0 + cw - 14, top + 22, 10);
        u->repaint = true;
    } else if (step == ST_DONE) {
        gfx_circle(s, x0 + 36, top + 50, 32, RGB(60, 190, 90));
        glyph_check(s, x0 + 12, top + 26, 48, RGB(255, 255, 255));
        ui_label(u, x0 + 90, top + 30, "A instalação foi concluída", FONT_BOLD, 18, T.text);
        char l[200];
        snprintf(l, sizeof l, "O mOS foi instalado em \"%s\". Remova o pendrive e reinicie; escolha o disco no menu de boot do firmware se necessário.", storage_disk_desc(sel));
        text_wrap(s, font_get(FONT_REGULAR), 13, rect_make(x0, top + 110, cw, 120), 19, T.text, l, true);
        footer(u, "Fechar", true, "Reiniciar", true, true, &clicked);
        if (clicked == 1) wm_close(w);
        if (clicked == 2) desktop_shutdown_dialog(1);
    } else {
        gfx_circle(s, x0 + 36, top + 50, 32, RGB(230, 70, 60));
        text_draw_center(s, font_get(FONT_BOLD), 34, rect_make(x0 + 4, top + 18, 64, 64), RGB(255, 255, 255), "!");
        ui_label(u, x0 + 90, top + 30, "Não foi possível instalar o mOS", FONT_BOLD, 18, T.text);
        text_wrap(s, font_get(FONT_REGULAR), 13, rect_make(x0, top + 110, cw, 120), 19, T.text, why[0] ? why : "Erro desconhecido.", true);
        footer(u, "Fechar", true, "Tentar de Novo", true, false, &clicked);
        if (clicked == 1) wm_close(w);
        if (clicked == 2) { step = ST_PICK; why[0] = 0; }
    }
}

static void tick(app_t *a) {
    if (step != ST_RUN) return;
    install_poll();
    const install_status_t *st = install_status();
    if (st->state == INSTALL_DONE) { step = ST_DONE; notify("Instalação concluída", "O mOS foi instalado com sucesso."); }
    else if (st->state == INSTALL_ERROR) { strncpy(why, st->error, sizeof why - 1); step = ST_FAIL; }
    if (iw) wm_invalidate(iw);
}

static void on_close(window_t *w) { iw = NULL; if (step != ST_RUN) step = ST_INTRO; }

static void launch(app_t *a) {
    if (iw) { wm_focus(iw); return; }
    iw = wm_create(&app_instalador, "Instalar mOS", -1, -1, 720, 460, WIN_NO_ZOOM | WIN_CENTER);
    if (!iw) return;
    iw->paint = paint;
    iw->on_close = on_close;
    iw->toolbar_h = TOOLBAR_H;
    if (step != ST_RUN) step = ST_INTRO;
}

app_t app_instalador = {.name = "Instalar mOS", .exec = "instalador", .draw_icon = icon_instalador_draw, .launch = launch, .tick = tick};
