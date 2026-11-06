/* mOS — "Componentes": galeria do UI kit (botões, botões circulares, combo box e controles) */
#include "apps.h"

static window_t *gw;
static int page;
static char combo[5][32] = {"Valor", "Valor", "Valor", "Valor", "Valor"};
static char combo_f[32] = "Valor";
static bool sw1 = true, sw2, cb1 = true, cb2;
static int rad_sel;
static float sl = 0.4f;
static char tf[64], sf[64];
static int pop_sel;
static u64 attn_at;               /* demonstração: pedir atenção no Dock depois de alguns segundos */
extern app_t app_componentes;

static void icon_componentes(surface_t *s, int x, int y, int size) {
    float m = size * 0.06f, w = size - 2 * m, r = size * 0.225f;
    gfx_round_rect_grad(s, x + m, y + m, w, w, r, RGB(250, 250, 252), RGB(222, 224, 232));
    gfx_specular_rim(s, x + m, y + m, w, w, r, 160);
    float pw = w * 0.62f, ph = w * 0.15f, px = x + m + (w - pw) / 2;
    gfx_round_rect(s, px, y + m + w * 0.2f, pw, ph, ph * 0.3f, RGB(0, 136, 255));
    gfx_round_rect(s, px, y + m + w * 0.43f, pw, ph, ph * 0.3f, RGBA(0, 136, 255, 60));
    gfx_round_rect(s, px, y + m + w * 0.66f, pw * 0.56f, ph, ph / 2, RGB(52, 199, 89));
    gfx_circle(s, px + pw * 0.56f - ph / 2, y + m + w * 0.66f + ph / 2, ph / 2 - 1, RGB(255, 255, 255));
    gfx_circle(s, px + pw * 0.85f, y + m + w * 0.66f + ph / 2, ph / 2, RGB(230, 230, 234));
}

static void section(ui_t *u, int x, int y, const char *t) { ui_label(u, x, y, t, FONT_SEMIBOLD, 13, T.text); }

static void alert_done(int b, void *ctx) {}

static void paint_buttons(ui_t *u, int y0) {
    static const char *cols[] = {"Cinza", "Tingido", "Tingido destrutivo", "Preenchido", "Destrutivo", "Simples"};
    static const int st[] = {BTN_GRAY, BTN_TINTED, BTN_TINTED_RED, BTN_FILLED, BTN_FILLED_RED, BTN_PLAIN};
    static const char *rows[] = {"Normal", "Pressionado", "Desativado"};
    int x0 = 104, cw = 102, bw = 90, bh = 26;
    section(u, 24, y0, "Botões");
    y0 += 26;
    for (int c = 0; c < 6; c++) ui_label(u, x0 + c * cw, y0, cols[c], FONT_REGULAR, 11, T.text2);
    y0 += 20;
    for (int r = 0; r < 3; r++) {
        int y = y0 + r * 36;
        ui_label(u, 24, y + 6, rows[r], FONT_REGULAR, 12, T.text2);
        for (int c = 0; c < 6; c++) {
            int style = st[c] | (r == 1 ? BTN_SHOW_PRESSED : 0);
            ui_button_style(u, 1000 + r * 10 + c, rect_make(x0 + c * cw, y, bw, bh), "Rótulo", style, r != 2, NULL);
        }
    }
    y0 += 3 * 36 + 10;
    section(u, 24, y0, "Com ícone");
    y0 += 26;
    for (int r = 0; r < 3; r++) {
        int y = y0 + r * 36;
        int sp = r == 1 ? BTN_SHOW_PRESSED : 0;
        ui_label(u, 24, y + 6, rows[r], FONT_REGULAR, 12, T.text2);
        ui_button_style(u, 1100 + r * 2, rect_make(x0, y, 124, bh), "Favoritar", BTN_SUBTLE | sp, r != 2, glyph_heart);
        ui_button_style(u, 1101 + r * 2, rect_make(x0 + 136, y, 124, bh), "Favoritar", (r == 1 ? BTN_SUBTLE : BTN_GRAY_ACCENT) | sp, r != 2, glyph_heart_slash);
        ui_button_style(u, 1110 + r, rect_make(x0 + 290, y, 124, bh), "Compartilhar", BTN_TINTED | BTN_CAPSULE | sp, r != 2, NULL);
        ui_button_style(u, 1120 + r, rect_make(x0 + 426, y, 124, bh), "Continuar", BTN_FILLED | BTN_CAPSULE | sp, r != 2, NULL);
    }
    y0 += 3 * 36 + 10;
    section(u, 24, y0, "Botões circulares");
    y0 += 28;
    static const int ds[] = {16, 20, 24, 28, 36};
    for (int r = 0; r < 3; r++) {
        int cy = y0 + 18 + r * 0;
        int xb = x0 + r * 220;
        int x = xb;
        for (int i = 0; i < 5; i++) {
            ui_circle_button(u, 1200 + r * 10 + i, x + ds[i] / 2, cy, ds[i], glyph_updown, r != 2, r == 1);
            x += ds[i] + 8;
        }
        ui_label(u, xb, cy + 22, rows[r], FONT_REGULAR, 11, T.text2);
    }
    y0 += 68;
    section(u, 24, y0, "Alertas");
    y0 += 26;
    if (ui_button_style(u, 1300, rect_make(x0, y0, 150, bh), "Mostrar alerta…", BTN_FILLED, true, NULL)) {
        static const char *b[] = {"Cancelar", "OK"};
        alert_show("Salvar esta mensagem como rascunho?", "Esta mensagem não foi enviada e tem alterações não salvas. Você pode salvá-la como rascunho para continuar depois.", b, 2, alert_done, NULL);
    }
    if (ui_button_style(u, 1301, rect_make(x0 + 160, y0, 150, bh), "Alerta destrutivo…", BTN_TINTED_RED, true, NULL)) desktop_shutdown_dialog(0);
}

static void paint_fields(ui_t *u, int y0) {
    static const char *opts[] = {"Valor", "Opção A", "Opção B", "Outra opção"};
    static const char *rows[] = {"Normal", "Foco", "Desativado"};
    static const int hs[] = {20, 24, 28, 32, 40};
    int x0 = 104;
    section(u, 24, y0, "Combo box");
    y0 += 30;
    /* normal (editável, com lista), e um campo que começa em foco para mostrar o anel */
    /* três linhas: normal (interativo), foco (anel de destaque) e desativado; cinco tamanhos */
    for (int row = 0; row < 3; row++) {
        ui_label(u, 24, y0 + 12, rows[row], FONT_REGULAR, 12, T.text2);
        int x = x0;
        for (int i = 0; i < 5; i++) {
            int w = 60 + hs[i] * 2;
            rect_t r = rect_make(x, y0 + (40 - hs[i]) / 2, w, hs[i]);
            if (row == 1) ui_focus_ring(u, r, MAX(CTRL_RADIUS, r.h * 0.26f));
            ui_combobox(u, 2000 + row * 10 + i, r, row == 0 ? combo[i] : combo_f, 32, opts, 4, row != 2);
            x += w + 10;
        }
        y0 += 50;
    }
    y0 += 10;
    section(u, 24, y0, "Campos");
    y0 += 28;
    ui_textfield(u, 2200, rect_make(x0, y0, 220, 26), tf, sizeof tf, "Campo de texto");
    ui_searchfield(u, 2201, rect_make(x0 + 236, y0, 200, 26), sf, sizeof sf);
    static const char *pops[] = {"Formato Rich Text", "Texto Simples", "Documento Web"};
    pop_sel = ui_popup(u, 2202, rect_make(x0 + 452, y0, 190, 26), pops, 3, pop_sel);
    y0 += 52;
    section(u, 24, y0, "Controles");
    y0 += 30;
    ui_switch(u, 2300, x0, y0, &sw1);
    ui_switch(u, 2301, x0 + 56, y0, &sw2);
    ui_checkbox(u, 2302, x0 + 120, y0 + 3, "Marcado", &cb1);
    ui_checkbox(u, 2303, x0 + 220, y0 + 3, "Desmarcado", &cb2);
    if (ui_radio(u, 2304, x0 + 340, y0 + 3, "Opção 1", rad_sel == 0)) rad_sel = 0;
    if (ui_radio(u, 2305, x0 + 440, y0 + 3, "Opção 2", rad_sel == 1)) rad_sel = 1;
    y0 += 44;
    ui_slider(u, 2306, rect_make(x0, y0, 260, 24), &sl, 0, 1);
    ui_progress(u, rect_make(x0 + 290, y0 + 9, 220, 6), sl);
    static const char *segs[] = {"Dia", "Semana", "Mês", "Ano"};
    static int seg;
    y0 += 44;
    seg = ui_segmented(u, 2307, rect_make(x0, y0, 280, 26), segs, 4, seg);
    ui_spinner(u, x0 + 320, y0 + 13, 10);
}

static void save_done(int b, const char *dir, const char *name, int fmt, void *ctx) {
    if (b == 2) { char m[200]; snprintf(m, sizeof m, "%s/%s", dir, name); notify("Salvar (demonstração)", m); }
}

/* inspetor: rótulo à esquerda, controle à direita, linhas separadas num painel arredondado */
static void paint_form(ui_t *u, int y0) {
    static bool chk_a = true, chk_b, sw = true;
    static int radio, stepv = 67, popsel;
    static char cbv[32] = "Valor", txt[48];
    static const char *opts[] = {"Valor", "Opção A", "Opção B"};
    static const char *pops[] = {"Rótulo", "Outro", "Mais um"};
    int rh = 42, n = 12;
    rect_t g = rect_make(24, y0, 380, 12 + n * rh);
    ui_group_box(u, g);
    static const char *names[] = {"Botão", "Caixas de seleção", "Combo box", "Detalhe + Abertura", "Detalhe", "Detalhe + Info",
                                  "Detalhe + Menu", "Botão pop-up", "Botão de rádio", "Stepper", "Interruptor", "Campo de texto"};
    for (int i = 0; i < n; i++) {
        rect_t r = ui_form_row(u, g, i, rh, n, names[i]);
        int right = r.x + r.w, cy = r.y + r.h / 2;
        switch (i) {
        case 0:
            ui_button_style(u, 3000, rect_make(right - 72, cy - 13, 72, 26), "Rótulo", BTN_GRAY, true, NULL);
            ui_button_style(u, 3001, rect_make(right - 72 - 66, cy - 13, 60, 26), "Rótulo", BTN_PLAIN_GRAY, true, NULL);
            break;
        case 1: {
            int lw = text_width(font_get(FONT_REGULAR), FONT_UI, "Rótulo") + 22;
            ui_checkbox(u, 3010, right - lw, cy - 8, "Rótulo", &chk_b);
            ui_checkbox(u, 3011, right - 2 * lw - 14, cy - 8, "Rótulo", &chk_a);
            break;
        }
        case 2: ui_combobox(u, 3020, rect_make(right - 150, cy - 14, 150, 28), cbv, 32, opts, 3, true); break;
        case 3: ui_detail(u, 3030, r, "Rótulo", DETAIL_DISCLOSURE); break;
        case 4: ui_detail(u, 3040, r, "Rótulo", DETAIL_NONE); break;
        case 5: ui_detail(u, 3050, r, "Rótulo", DETAIL_INFO); break;
        case 6: ui_detail(u, 3060, r, "Rótulo", DETAIL_MENU); break;
        case 7: popsel = ui_popup_inline(u, 3070, r, pops, 3, popsel); break;
        case 8: {
            int lw = text_width(font_get(FONT_REGULAR), FONT_UI, "Rótulo") + 22;
            if (ui_radio(u, 3080, right - lw, cy - 8, "Rótulo", radio == 1)) radio = 1;
            if (ui_radio(u, 3081, right - 2 * lw - 14, cy - 8, "Rótulo", radio == 0)) radio = 0;
            break;
        }
        case 9: {
            int lw = text_width(font_get(FONT_REGULAR), 11, "Rótulo");
            ui_stepper(u, 3090, rect_make(right - lw - 8 - 92, cy - 14, 92, 28), &stepv, 0, 999);
            ui_label(u, right - lw, cy - 7, "Rótulo", FONT_REGULAR, 11, T.text2);
            break;
        }
        case 10: ui_switch(u, 3100, right - 40, cy - 11, &sw); break;
        case 11: ui_textfield(u, 3110, rect_make(right - 150, cy - 14, 150, 28), txt, sizeof txt, NULL); break;
        }
    }
    int x = g.x + g.w + 28;
    section(u, x, y0, "Diálogos");
    if (ui_button_style(u, 3200, rect_make(x, y0 + 28, 200, 28), "Salvar documento…", BTN_FILLED, true, NULL)) {
        static const char *f[] = {"Texto Simples (.txt)", "Markdown (.md)", "Documento Rich Text"};
        save_dialog_show("Deseja manter este novo documento “Sem título”?",
                         "Você pode salvar as alterações ou apagar o documento agora. Não é possível desfazer.",
                         "Sem título", f, 3, true, save_done, NULL);
    }
    if (ui_button_style(u, 3201, rect_make(x, y0 + 66, 200, 28), "Mostrar alerta…", BTN_GRAY, true, NULL)) {
        static const char *b[] = {"Cancelar", "OK"};
        alert_show("Salvar esta mensagem como rascunho?", "Esta mensagem não foi enviada e tem alterações não salvas.", b, 2, alert_done, NULL);
    }
    section(u, x, y0 + 116, "Dock");
    if (ui_button_style(u, 3202, rect_make(x, y0 + 144, 96, 28), "Selo +1", BTN_GRAY, true, NULL))
        dock_set_badge(&app_componentes, app_componentes.badge + 1);
    if (ui_button_style(u, 3203, rect_make(x + 104, y0 + 144, 96, 28), "Sem selo", BTN_GRAY, app_componentes.badge > 0, NULL))
        dock_set_badge(&app_componentes, 0);
    if (ui_button_style(u, 3204, rect_make(x, y0 + 180, 200, 28), "Pedir atenção em 3 s", BTN_GRAY, true, NULL))
        attn_at = timer_ticks() + 3000;   /* dá tempo de trocar de app: o ícone pula até você voltar */
}

/* barra lateral em três tamanhos + busca + progresso */
static void sidebar_demo(ui_t *u, int x, int y, int w, int rh, int *sel, int base_id) {
    struct { const char *l; glyph_fn g; } top[] = {{"Recentes", glyph_sb_recents}, {"Compartilhado", glyph_sb_shared}};
    struct { const char *l; glyph_fn g; } fav[] = {{"Mesa", glyph_sb_desktop}, {"Documentos", glyph_sb_document}, {"Downloads", glyph_sb_download}};
    struct { const char *l; glyph_fn g; } loc[] = {{"Nuvem", glyph_sb_cloud}, {"Visitante", glyph_sb_home}, {"Rede", glyph_globe}, {"Lixeira", glyph_sb_trash}};
    int k = 0, gap = rh / 6 + 2;
    for (int i = 0; i < 2; i++, k++) { if (ui_sidebar_item(u, base_id + k, rect_make(x, y, w, rh), top[i].l, *sel == k, top[i].g)) *sel = k; y += rh + gap; }
    y += rh / 3;
    ui_sidebar_header(u, rect_make(x, y, w, rh), "Favoritos"); y += rh + gap;
    for (int i = 0; i < 3; i++, k++) { if (ui_sidebar_item(u, base_id + k, rect_make(x, y, w, rh), fav[i].l, *sel == k, fav[i].g)) *sel = k; y += rh + gap; }
    y += rh / 3;
    ui_sidebar_header(u, rect_make(x, y, w, rh), "Locais"); y += rh + gap;
    for (int i = 0; i < 4; i++, k++) { if (ui_sidebar_item(u, base_id + k, rect_make(x, y, w, rh), loc[i].l, *sel == k, loc[i].g)) *sel = k; y += rh + gap; }
    y += rh / 3;
    ui_sidebar_header(u, rect_make(x, y, w, rh), "Etiquetas");
}

static void paint_more(ui_t *u, int y0) {
    static int s1, s2;
    section(u, 24, y0, "Barra lateral");
    sidebar_demo(u, 24, y0 + 26, 170, 24, &s1, 5000);
    sidebar_demo(u, 214, y0 + 26, 190, 30, &s2, 5100);
    int x = 430;
    section(u, x, y0, "Busca");
    static char q1[32], q2[32] = "Buscar";
    ui_searchfield(u, 5200, rect_make(x, y0 + 26, 140, 26), q1, sizeof q1);
    ui_searchfield(u, 5201, rect_make(x + 156, y0 + 26, 140, 26), q2, sizeof q2);
    section(u, x, y0 + 74, "Progresso");
    static const float vals[] = {0.05f, 0.25f, 0.55f, 0.8f, 1.0f};
    int y = y0 + 100;
    for (int i = 0; i < 5; i++) ui_progress_bar(u, rect_make(x, y + i * 18, 130, 10), vals[i], true);
    for (int i = 0; i < 5; i++) ui_progress_bar(u, rect_make(x + 150, y + i * 18, 130, 5), vals[i], true);
    y += 5 * 18 + 6;
    for (int i = 0; i < 5; i++) ui_progress_bar(u, rect_make(x, y + i * 18, 130, 10), vals[i], false);
    for (int i = 0; i < 5; i++) ui_progress_bar(u, rect_make(x + 150, y + i * 18, 130, 5), vals[i], false);
    y += 5 * 18 + 10;
    ui_progress_bar(u, rect_make(x, y, 130, 10), -1, true);
    ui_progress_bar(u, rect_make(x + 150, y + 2, 130, 5), -1, false);
    y += 28;
    for (int i = 0; i < 5; i++) {
        ui_progress_ring(u, x + 10 + i * 30, y + 10, 18, vals[i], true);
        ui_progress_ring(u, x + 10 + i * 30, y + 44, 18, vals[i], false);
    }
    y += 70;
    for (int i = 0; i < 4; i++) ui_progress_ring(u, x + 18 + i * 46, y + 18, 34, vals[i + 1], i % 2 == 0);
    ui_spinner(u, x + 18 + 4 * 46, y + 18, 15);
}

/* switches (5 tamanhos × normal/pressionado/desativado, desligado e ligado) e checkboxes */
static void paint_toggles(ui_t *u, int y0) {
    static const int hs[] = {14, 18, 22, 26, 32};
    section(u, 24, y0, "Interruptores");
    int y = y0 + 30;
    for (int on = 0; on < 2; on++)
        for (int st = 0; st < 3; st++) {
            int x = 24;
            for (int i = 0; i < 5; i++) {
                bool v = on;
                ui_force_state = st;
                ui_switch_ex(u, 6000 + on * 100 + st * 10 + i, x, y + (34 - hs[i]) / 2, hs[i], &v, true);
                ui_force_state = -1;
                x += (int)(hs[i] * 2.25f) + 18;
            }
            y += 44;
        }
    /* um interruptor de verdade, para testar o arrasto da lente */
    static bool live = true;
    ui_label(u, 24, y + 4, "Interativo:", FONT_REGULAR, 12, T.text2);
    ui_switch_ex(u, 6500, 100, y, 26, &live, true);

    int cx = 470;
    section(u, cx, y0, "Caixas de seleção");
    static const char *rows[] = {"Normal", "Pressionado", "Desativado"};
    int cy = y0 + 30;
    for (int key = 1; key >= 0; key--) {
        for (int st = 0; st < 3; st++) {
            ui_force_state = st;
            ui_force_key = key;
            for (int k = 0; k < 3; k++) {
                int state = k == 0 ? 1 : k == 1 ? 2 : 0;
                ui_checkbox_ex(u, 6600 + key * 100 + st * 10 + k, cx + k * 90, cy, "Rótulo", &state, true);
            }
            ui_force_state = ui_force_key = -1;
            (void)rows;
            cy += 32;
        }
        cy += 16;
        if (key) ui_label(u, cx, cy - 10, "Janela sem foco:", FONT_REGULAR, 11, T.text2);
        cy += 8;
    }
    static bool c1 = true, c2;
    ui_checkbox(u, 6800, cx, cy + 8, "Interativo", &c1);
    ui_checkbox(u, 6801, cx + 120, cy + 8, "Outro", &c2);
}

/* componentes avançados: abas, tabela, árvore, data, cor, tokens, dica, popover e folha */
static void paint_adv(ui_t *u, int y0) {
    /* abas */
    static int tab;
    static const char *tabs[] = {"Geral", "Rede", "Avançado"};
    rect_t tv = rect_make(24, y0 + 18, 390, 110);
    tab = ui_tabview(u, 7000, tv, tabs, 3, tab);
    static const char *tabtxt[] = {"Conteúdo da aba Geral.", "Configurações de rede ficariam aqui.", "Opções avançadas."};
    ui_label(u, tv.x + 20, tv.y + 30, tabtxt[tab], FONT_REGULAR, 13, T.text);
    static bool tsw = true;
    ui_label(u, tv.x + 20, tv.y + 62, "Ativado", FONT_REGULAR, 13, T.text);
    ui_switch(u, 7001, tv.x + tv.w - 60, tv.y + 58, &tsw);

    /* tabela com ordenação */
    struct row { const char *n; int kb; const char *k; };
    static struct row data[] = {{"Relatório.txt", 12, "Texto"}, {"Foto.png", 820, "Imagem"}, {"Notas.md", 4, "Markdown"},
                                {"Música.wav", 3400, "Áudio"}, {"Projeto", 0, "Pasta"}};
    static int sort_col = 0, sel = 1;
    static bool asc = true;
    int ty = y0 + 146;
    section(u, 24, ty, "Tabela");
    rect_t th = rect_make(24, ty + 22, 390, 26);
    int xs[3] = {th.x + 40, th.x + 220, th.x + 300};
    static const char *cols[] = {"Nome", "Tamanho", "Tipo"};
    ui_table_header(u, 7010, th, cols, xs, 3, &sort_col, &asc);
    int order[5] = {0, 1, 2, 3, 4};
    for (int i = 0; i < 5; i++)
        for (int j = i + 1; j < 5; j++) {
            struct row *a = &data[order[i]], *b = &data[order[j]];
            int cmp = sort_col == 0 ? strcmp(a->n, b->n) : sort_col == 1 ? a->kb - b->kb : strcmp(a->k, b->k);
            if (asc ? cmp > 0 : cmp < 0) { int t = order[i]; order[i] = order[j]; order[j] = t; }
        }
    for (int i = 0; i < 5; i++) {
        struct row *d = &data[order[i]];
        rect_t rr = rect_make(th.x - 8, th.y + th.h + 4 + i * 24, th.w + 16, 24);
        bool s = sel == order[i];
        if (ui_list_row(u, 7020 + order[i], rr, s, i % 2 == 0)) sel = order[i];
        u32 c = s ? RGB(255, 255, 255) : T.text, c2 = s ? RGB(255, 255, 255) : T.text2;
        char sz[24];
        if (d->kb) snprintf(sz, sizeof sz, "%d KB", d->kb); else strcpy(sz, "--");
        int bl = rr.y + 17;
        text_draw(u->s, font_get(FONT_REGULAR), 13, xs[0], bl, c, d->n);
        text_draw(u->s, font_get(FONT_REGULAR), 13, xs[1], bl, c2, sz);
        text_draw(u->s, font_get(FONT_REGULAR), 13, xs[2], bl, c2, d->k);
    }

    /* lista em árvore */
    int oy = th.y + th.h + 4 + 5 * 24 + 18;
    section(u, 24, oy, "Lista em árvore");
    static bool o1 = true, o2;
    static int osel = 2;
    struct { int depth; bool exp; bool *open; const char *l; glyph_fn g; int parent_open; } tree[] = {
        {0, true, &o1, "Documentos", glyph_sb_document, 1}, {1, true, &o2, "Projetos", glyph_sb_apps, 1},
        {2, false, NULL, "mOS.txt", glyph_sb_document, 2}, {1, false, NULL, "Notas.md", glyph_sb_document, 1},
        {0, false, NULL, "Downloads", glyph_sb_download, 0},
    };
    int yy = oy + 22;
    for (int i = 0; i < 5; i++) {
        bool visible = tree[i].parent_open == 0 || (tree[i].parent_open == 1 && o1) || (tree[i].parent_open == 2 && o1 && o2);
        if (!visible) continue;
        if (ui_outline_row(u, 7100 + i * 4, rect_make(16, yy, 406, 24), tree[i].depth, tree[i].exp, tree[i].open, tree[i].l, tree[i].g, osel == i)) osel = i;
        yy += 24;
    }

    /* coluna direita */
    int x = 450;
    section(u, x, y0, "Data");
    static int dy = 2026, dm = 11, dd = 6, cy = 2026, cm = 11, cd = 6;
    ui_date_field(u, 7200, rect_make(x, y0 + 22, 150, 28), &dy, &dm, &dd);
    ui_calendar(u, 7210, rect_make(x, y0 + 60, 224, 210), &cy, &cm, &cd);

    int ry = y0 + 286;
    section(u, x, ry, "Cor");
    static u32 col = RGB(0, 136, 255);
    ui_color_well(u, 7300, rect_make(x, ry + 22, 52, 28), &col);
    gfx_round_rect(u->s, x + 64, ry + 22, 120, 28, 8, col);

    section(u, x, ry + 66, "Tokens");
    static char toks[8][24] = {"design", "mOS"};
    static int ntok = 2;
    static char tbuf[24];
    ui_token_field(u, 7400, rect_make(x, ry + 88, 330, 30), toks, &ntok, 8, tbuf, sizeof tbuf);

    section(u, x, ry + 134, "Dica, popover e folha");
    rect_t bt = rect_make(x, ry + 156, 110, 28);
    ui_button_style(u, 7500, bt, "Passe o mouse", BTN_GRAY, true, NULL);
    ui_tooltip(u, 7501, bt, "Esta é uma dica de ferramenta");
    static bool popo, sheet;
    rect_t pb = rect_make(x + 118, ry + 156, 100, 28);
    if (ui_button_style(u, 7502, pb, "Popover", BTN_TINTED, true, NULL)) popo = !popo;
    if (ui_button_style(u, 7503, rect_make(x + 226, ry + 156, 100, 28), "Folha…", BTN_FILLED, true, NULL)) sheet = true;
    if (popo) {
        rect_t p = ui_popover_frame(u, pb, 220, 84);
        ui_label(u, p.x + 14, p.y + 12, "Popover", FONT_SEMIBOLD, 13, T.text);
        ui_label(u, p.x + 14, p.y + 34, "Painel leve ancorado num botão.", FONT_REGULAR, 12, T.text2);
        if (u->pressed && !rect_contains(p, u->mx, u->my) && !rect_contains(pb, u->mx, u->my)) popo = false;
    }
    if (sheet) {
        rect_t s = ui_sheet(u, 360, 150, TOOLBAR_H + 4); /* desce da base do cabeçalho */
        ui_label(u, s.x + 20, s.y + 20, "Folha modal", FONT_BOLD, 14, T.text);
        ui_label(u, s.x + 20, s.y + 46, "Presa à janela; o resto fica bloqueado.", FONT_REGULAR, 12.5f, T.text2);
        if (ui_button_style(u, 7510, rect_make(s.x + s.w - 20 - 90, s.y + s.h - 20 - 28, 90, 28), "OK", BTN_FILLED | BTN_CAPSULE | BTN_DEFAULT, true, NULL)) sheet = false;
        if (ui_button_style(u, 7511, rect_make(s.x + s.w - 20 - 90 - 8 - 90, s.y + s.h - 20 - 28, 90, 28), "Cancelar", BTN_GRAY | BTN_CAPSULE, true, NULL)) sheet = false;
        ui_sheet_end(u);
    }
}

static void paint(window_t *w, ui_t *u) {
    static const char *pages[] = {"Botões", "Campos", "Formulário", "Mais", "Seletores", "Avançado"};
    page = ui_segmented(u, UI_ID, rect_make(u->cw - 8 - 480, TOOLBAR_Y(28), 480, 28), pages, 6, page);
    static const char *modes[] = {"Claro", "Escuro"};
    int m = ui_segmented(u, UI_ID, rect_make(u->cw - 8 - 480 - 8 - 130, TOOLBAR_Y(28), 130, 28), modes, 2, T.dark ? 1 : 0);
    /* atalhos de teclado (sem campo em foco): 1/2 trocam a página, E alterna claro/escuro */
    if (u->key && !u->focus) {
        if (u->key == '1') { page = 0; u->key_used = true; }
        else if (u->key == '2') { page = 1; u->key_used = true; }
        else if (u->key == '3') { page = 2; u->key_used = true; }
        else if (u->key == '4') { page = 3; u->key_used = true; }
        else if (u->key == '5') { page = 4; u->key_used = true; }
        else if (u->key == '6') { page = 5; u->key_used = true; }
        else if (u->key == 's' || u->key == 'S') {
            static const char *f[] = {"Texto Simples (.txt)", "Markdown (.md)", "Documento Rich Text"};
            save_dialog_show("Deseja manter este novo documento “Sem título”?",
                             "Você pode salvar as alterações ou apagar o documento agora. Não é possível desfazer.",
                             "Sem título", f, 3, true, save_done, NULL);
            u->key_used = true;
        }
        else if (u->key == 'e' || u->key == 'E') { m = T.dark ? 0 : 1; u->key_used = true; }
    }
    if (m != (T.dark ? 1 : 0)) {
        theme_set(m == 1, g_accent_index);
        desktop_rebuild_wallpaper();
        for (int i = 0;; i++) { extern window_t *wm_nth(int); window_t *x = wm_nth(i); if (!x) break; x->needs_paint = true; }
    }
    int y0 = w->toolbar_h + 16;
    if (page == 0) paint_buttons(u, y0); else if (page == 1) paint_fields(u, y0); else if (page == 2) paint_form(u, y0); else if (page == 3) paint_more(u, y0); else if (page == 4) paint_toggles(u, y0); else paint_adv(u, y0);
}

static void on_close(window_t *w) { gw = NULL; }

static void launch(app_t *a) {
    if (gw) { wm_focus(gw); return; }
    gw = wm_create(&app_componentes, "Componentes", -1, -1, 900, 700, WIN_RESIZABLE);
    if (!gw) return;
    gw->paint = paint;
    gw->on_close = on_close;
    gw->toolbar_h = TOOLBAR_H;
    gw->min_w = 700; gw->min_h = 480;
}

/* ------------------------------------------------------------ menus de demonstração (padrão do sistema) */
static int city_sel = 3;
static bool sym_check[4] = {true, false, true, false};
static menu_item_t cities_items[] = {
    {"Chicago", NULL, 4101}, {"Cupertino", NULL, 4102}, {"Nova York", NULL, 4103}, {"São Francisco", NULL, 4104},
};
static menu_t cities_menu = {"Cidades", cities_items, 4};
static menu_item_t edit_items[] = {
    {"Recortar", "⌘X", 4001}, {"Copiar", "⌘C", 4002}, {"Colar", "⌘V", 4003}, {"Duplicar", "⌘D", 4004},
    {NULL},
    {"Cabeçalho", NULL, 0, false, false, NULL, true},
    {"Item com símbolo", NULL, 4005, false, false, glyph_globe},
    {NULL},
    {"Copiar Estilo", "⌥⌘C", 4006}, {"Colar Estilo", "⌥⌘V", 4007, true}, {"Todos os Atalhos", "⌘A", 4008},
    {NULL},
    {"Cidades", NULL, 0, false, false, NULL, false, &cities_menu},
};
static menu_item_t sym_items[] = {
    {"Recortar", "⌘X", 4201, false, true, glyph_copy}, {"Copiar", "⌘C", 4202, false, false, glyph_copy},
    {"Colar", "⌘V", 4203, false, true, glyph_copy}, {"Duplicar", "⌘D", 4204, false, false, glyph_copy},
    {NULL},
    {"Cabeçalho", NULL, 0, false, false, NULL, true},
    {"Item com símbolo", "⌘A", 4205, false, false, glyph_copy},
    {"Desativado", "⌘A", 4206, true, false, glyph_copy},
    {NULL},
    {"Cidades", NULL, 0, false, false, glyph_copy, false, &cities_menu},
};
static menu_t comp_menus[] = {{"Editar", edit_items, ARRAY_LEN(edit_items)}, {"Símbolos", sym_items, ARRAY_LEN(sym_items)}};

static void on_menu(app_t *a, int act) {
    if (act >= 4101 && act <= 4104) {
        city_sel = act - 4101;
        for (int i = 0; i < 4; i++) cities_items[i].checked = i == city_sel;
        notify("Cidade escolhida", cities_items[city_sel].label);
    } else if (act >= 4201 && act <= 4204) {
        int k = act - 4201;
        sym_check[k] = !sym_check[k];
        sym_items[k].checked = sym_check[k];
    } else if (act >= 4001 && act <= 4008) {
        static const char *n[] = {"Recortar", "Copiar", "Colar", "Duplicar", "Item com símbolo", "Copiar Estilo", "Colar Estilo", "Todos os Atalhos"};
        notify("Menu", n[act - 4001]);
    }
}

static void comp_tick(app_t *a) {
    if (attn_at && timer_ticks() >= attn_at) { attn_at = 0; dock_request_attention(a); }
}

app_t app_componentes = {.name = "Componentes", .exec = "componentes", .draw_icon = icon_componentes, .launch = launch,
                         .tick = comp_tick, .on_menu = on_menu, .menus = comp_menus, .nmenus = 2};
