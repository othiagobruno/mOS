/* mOS — Diário (entradas com data, título, texto, foto e marcador; sugestões de escrita; busca; marcadores;
 * dados de sequência e palavras) e Dicas (coleções de dicas do sistema com botões para experimentar). */
#include "kit.h"

extern app_t app_diario, app_dicas;

/* ================================================================== Diário */
#define MAXJ 400
typedef struct { bool used, mark; i64 when; char title[96]; char *text; char photo[200]; } jentry_t;
static struct {
    window_t *w;
    jentry_t e[MAXJ];
    bool loaded, editing, only_marks;
    int cur, scroll;
    textarea_t *ta;
    char title[96], search[64];
    int prompt;
    surface_t *thumbs[MAXJ];
} J = {.cur = -1};

static const char *prompts[] = {
    "Qual foi o melhor momento do seu dia?", "Pelo que você é grato hoje?", "O que você aprendeu esta semana?",
    "Descreva um lugar onde você se sente em paz.", "Que conversa ficou na sua cabeça hoje?", "O que você quer lembrar daqui a um ano?",
    "Qual pequeno hábito está fazendo diferença?", "Quem fez seu dia melhor e por quê?"};

static void j_save(void) {
    sbuf_t b = {0};
    sb_puts(&b, "# mOS Diário 1\n");
    for (int i = 0; i < MAXJ; i++) {
        jentry_t *x = &J.e[i];
        if (!x->used) continue;
        char t[96];
        kit_field(t, sizeof t, x->title);
        sb_printf(&b, "E\t%lld\t%d\t%s\t%s\t", (long long)x->when, x->mark, t, x->photo);
        kit_esc(&b, x->text ? x->text : "");
        sb_putc(&b, '\n');
    }
    char p[200];
    kit_save(kit_lib("Diário", "entradas.tsv", p, sizeof p), &b);
}
static jentry_t *j_new(void) {
    for (int i = 0; i < MAXJ; i++) if (!J.e[i].used) { memset(&J.e[i], 0, sizeof J.e[i]); J.e[i].used = true; J.e[i].when = kit_now_secs(); return &J.e[i]; }
    return NULL;
}
static void j_load(void) {
    if (J.loaded) return;
    J.loaded = true;
    char p[200];
    char *buf = kit_read(kit_lib("Diário", "entradas.tsv", p, sizeof p), NULL);
    if (!buf) {
        jentry_t *x = j_new();
        snprintf(x->title, sizeof x->title, "Meu primeiro registro");
        x->text = strdup("Hoje comecei a usar o Diário do mOS. Escrever um pouco todo dia ajuda a lembrar dos momentos que importam.");
        j_save();
        return;
    }
    char *cur = buf, *ln, *f[8];
    while ((ln = kit_line(&cur))) {
        if (ln[0] != 'E') continue;
        int n = kit_split(ln, f, 8);
        if (n < 6) continue;
        jentry_t *x = j_new();
        if (!x) break;
        x->when = strtol(f[1], NULL, 10);
        x->mark = atoi(f[2]);
        snprintf(x->title, sizeof x->title, "%s", f[3]);
        snprintf(x->photo, sizeof x->photo, "%s", f[4]);
        kit_unesc(f[5]);
        x->text = strdup(f[5]);
    }
    kfree(buf);
}
static int j_order[MAXJ];
static int j_list(void) {
    int n = 0;
    char q[64];
    snprintf(q, sizeof q, "%s", J.search);
    for (char *p = q; *p; p++) *p = (char)tolower(*p);
    for (int i = 0; i < MAXJ; i++) {
        jentry_t *x = &J.e[i];
        if (!x->used || (J.only_marks && !x->mark)) continue;
        if (q[0]) {
            char h[300];
            snprintf(h, sizeof h, "%s %.200s", x->title, x->text ? x->text : "");
            for (char *p = h; *p; p++) *p = (char)tolower(*p);
            if (!strstr(h, q)) continue;
        }
        j_order[n++] = i;
    }
    for (int i = 1; i < n; i++) { int v = j_order[i], k = i - 1; while (k >= 0 && J.e[j_order[k]].when < J.e[v].when) { j_order[k + 1] = j_order[k]; k--; } j_order[k + 1] = v; }
    return n;
}
static void j_edit(int idx) {
    J.cur = idx;
    J.editing = true;
    if (!J.ta) J.ta = textarea_new(15, 0, 0, 16);
    jentry_t *x = idx >= 0 ? &J.e[idx] : NULL;
    snprintf(J.title, sizeof J.title, "%s", x ? x->title : "");
    textarea_set(J.ta, x && x->text ? x->text : "", -1);
    textarea_focus(J.ta, true);
}
static void j_commit(void) {
    int len;
    const char *t = textarea_text(J.ta, &len);
    if (J.cur < 0) {
        if (!len && !J.title[0]) { J.editing = false; return; }
        jentry_t *x = j_new();
        if (!x) return;
        J.cur = (int)(x - J.e);
    }
    jentry_t *x = &J.e[J.cur];
    snprintf(x->title, sizeof x->title, "%s", J.title);
    if (x->text) kfree(x->text);
    x->text = kmalloc(len + 1);
    memcpy(x->text, t, len);
    x->text[len] = 0;
    J.editing = false;
    j_save();
}
static int words(const char *t) { int n = 0; bool in = false; for (; t && *t; t++) { bool sp = isspace((u8)*t); if (!sp && !in) n++; in = !sp; } return n; }

static void j_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    j_load();
    int sb = w->sidebar_w;
    /* barra lateral: resumo e filtros */
    int y = w->toolbar_h + 4;
    ui_searchfield(u, UI_ID, rect_make(12, y, sb - 24, 28), J.search, sizeof J.search);
    y += 42;
    if (ui_sidebar_item(u, 110000, rect_make(10, y, sb - 20, 28), "Todas as Entradas", !J.only_marks, glyph_sb_document)) J.only_marks = false;
    y += 30;
    if (ui_sidebar_item(u, 110001, rect_make(10, y, sb - 20, 28), "Marcadas", J.only_marks, glyph_sb_recents)) J.only_marks = true;
    y += 44;
    /* "Insights": sequência de dias, total do ano, palavras */
    int total = 0, year_n = 0, wsum = 0, ty, tm, td;
    kit_today(&ty, &tm, &td);
    static u8 days_with[400];
    memset(days_with, 0, sizeof days_with);
    i64 today = kit_now_secs() / 86400;
    for (int i = 0; i < MAXJ; i++) {
        if (!J.e[i].used) continue;
        total++;
        int yy, mm, dd;
        kit_from_days(J.e[i].when / 86400, &yy, &mm, &dd);
        if (yy == ty) year_n++;
        wsum += words(J.e[i].text);
        i64 ago = today - J.e[i].when / 86400;
        if (ago >= 0 && ago < 400) days_with[ago] = 1;
    }
    int streak = 0;
    for (int i = days_with[0] ? 0 : 1; i < 400 && days_with[i]; i++) streak++;
    ui_label(u, 18, y, "Resumo", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    struct { const char *l; int v; u32 c; } st[3] = {{"dias seguidos", streak, RGB(255, 149, 0)}, {"entradas em " , year_n, RGB(175, 82, 222)}, {"palavras", wsum, RGB(10, 132, 255)}};
    for (int i = 0; i < 3; i++) {
        rect_t r = rect_make(12, y, sb - 24, 50);
        gfx_round_rect(s, r.x, r.y, r.w, r.h, 10, T.dark ? RGBA(255, 255, 255, 18) : RGBA(255, 255, 255, 200));
        char v[16], l[40];
        snprintf(v, sizeof v, "%d", st[i].v);
        if (i == 1) snprintf(l, sizeof l, "entradas em %d", ty); else snprintf(l, sizeof l, "%s", st[i].l);
        text_draw(s, font_get(FONT_BOLD), 20, r.x + 12, r.y + 28, st[i].c, v);
        text_draw(s, font_get(FONT_REGULAR), 11.5f, r.x + 12, r.y + 43, T.text2, l);
        y += 58;
    }
    (void)total;
    /* área principal */
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.dark ? RGB(28, 28, 30) : RGB(244, 242, 248));
    if (J.editing) {
        if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 80, TOOLBAR_Y(28), 80, 28), "OK", true)) { j_commit(); return; }
        if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 80 - 8 - 90, TOOLBAR_Y(28), 90, 28), "Cancelar", true)) { J.editing = false; return; }
        rect_t card = rect_make(a.x + 30, a.y + 20, a.w - 60, a.h - 40);
        gfx_shadow(s, card.x, card.y, card.w, card.h, 14, 18, 40, 4);
        gfx_round_rect(s, card.x, card.y, card.w, card.h, 14, T.content_bg);
        i64 when = J.cur >= 0 ? J.e[J.cur].when : kit_now_secs();
        int yy, mm, dd;
        kit_from_days(when / 86400, &yy, &mm, &dd);
        char d[64];
        snprintf(d, sizeof d, "%s, %d de %s de %d", kit_wday[kit_dow(yy, mm, dd)], dd, kit_month[mm - 1], yy);
        ui_label(u, card.x + 20, card.y + 16, d, FONT_SEMIBOLD, 12, T.text2);
        ui_textfield(u, UI_ID, rect_make(card.x + 16, card.y + 38, card.w - 32, 30), J.title, sizeof J.title, "Título");
        int len;
        textarea_text(J.ta, &len);
        if (!len && !textarea_focused(J.ta)) ui_label(u, card.x + 32, card.y + 92, prompts[J.prompt % 8], FONT_REGULAR, 15, T.text3);
        textarea_paint(w, u, J.ta, rect_make(card.x + 4, card.y + 76, card.w - 8, card.h - 120));
        char wc[32];
        snprintf(wc, sizeof wc, "%d palavras", words(textarea_text(J.ta, NULL)));
        ui_label(u, card.x + 20, card.y + card.h - 32, wc, FONT_REGULAR, 11.5f, T.text3);
        if (ui_button_style(u, UI_ID, rect_make(card.x + card.w - 16 - 150, card.y + card.h - 38, 150, 26), "Outra sugestão", BTN_PLAIN, true, NULL)) J.prompt++;
        if ((u->mods & MOD_CMD) && u->key == 's') { j_commit(); u->key_used = true; }
        return;
    }
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 110, TOOLBAR_Y(28), 110, 28), "Nova Entrada", true)) j_edit(-1);
    int n = j_list();
    rect_t view = rect_make(a.x, a.y, a.w, a.h);
    rect_t pc = surface_push_clip(s, view);
    int cy = a.y + 18 - J.scroll, cw = MIN(640, a.w - 60), cx = a.x + (a.w - cw) / 2;
    /* cartão de sugestão no topo */
    if (!J.search[0] && !J.only_marks) {
        rect_t r = rect_make(cx, cy, cw, 70);
        gfx_round_rect_grad(s, r.x, r.y, r.w, r.h, 14, RGB(140, 100, 230), RGB(100, 80, 200));
        ui_label(u, r.x + 18, r.y + 14, "Sugestão de escrita", FONT_SEMIBOLD, 11.5f, RGBA(255, 255, 255, 200));
        ui_label(u, r.x + 18, r.y + 34, prompts[J.prompt % 8], FONT_SEMIBOLD, 15, RGB(255, 255, 255));
        if (ui_clicked(u, UI_ID, r)) j_edit(-1);
        cy += 84;
    }
    int del = -1;
    for (int i = 0; i < n; i++) {
        jentry_t *x = &J.e[j_order[i]];
        int th = 0;
        const char *t = x->text ? x->text : "";
        int lines = text_wrap(s, font_get(FONT_REGULAR), 13.5f, rect_make(0, 0, cw - 36, 2000), 20, T.text, t, false) / 20;
        lines = MIN(lines, 5);
        th = 74 + lines * 20;
        rect_t r = rect_make(cx, cy, cw, th);
        if (cy + th > view.y && cy < view.y + view.h) {
            gfx_shadow(s, r.x, r.y, r.w, r.h, 14, 10, 26, 2);
            gfx_round_rect(s, r.x, r.y, r.w, r.h, 14, T.content_bg);
            text_draw_ellipsis(s, font_get(FONT_BOLD), 16, r.x + 18, r.y + 28, r.w - 60, T.text, x->title[0] ? x->title : "Sem título");
            rect_t tr = rect_make(r.x + 18, r.y + 40, r.w - 36, lines * 20);
            rect_t pc2 = surface_push_clip(s, tr);
            text_wrap(s, font_get(FONT_REGULAR), 13.5f, rect_make(tr.x, tr.y, tr.w, 2000), 20, T.text, t, true);
            surface_pop_clip(s, pc2);
            int yy, mm, dd;
            kit_from_days(x->when / 86400, &yy, &mm, &dd);
            char d[48];
            snprintf(d, sizeof d, "%s, %d de %s de %d", kit_wday[kit_dow(yy, mm, dd)], dd, kit_month[mm - 1], yy);
            text_draw(s, font_get(FONT_REGULAR), 11.5f, r.x + 18, r.y + r.h - 12, T.text2, d);
            rect_t mk = rect_make(r.x + r.w - 40, r.y + 10, 26, 26);
            if (x->mark || ui_hit(u, r)) {
                u32 mc = x->mark ? RGB(255, 59, 48) : T.text3;
                float bm[] = {mk.x + 7.0f, mk.y + 4.0f, mk.x + 19.0f, mk.y + 4.0f, mk.x + 19.0f, mk.y + 22.0f, mk.x + 13.0f, mk.y + 17.0f, mk.x + 7.0f, mk.y + 22.0f};
                gfx_polygon(s, bm, 5, mc);
            }
            if (ui_hit(u, r)) {
                rect_t db = rect_make(r.x + r.w - 110, r.y + r.h - 32, 90, 24);
                if (ui_button_style(u, 110500 + i, db, "Apagar", BTN_TINTED_RED, true, NULL)) del = j_order[i];
            }
            if (ui_clicked(u, 110200 + i, mk)) { x->mark = !x->mark; j_save(); }
            else if (ui_clicked(u, 110300 + i, r)) j_edit(j_order[i]);
        }
        cy += th + 14;
    }
    if (!n) ui_label_center(u, rect_make(a.x, cy + 30, a.w, 24), J.search[0] ? "Nenhum Resultado" : "Nenhuma Entrada", FONT_SEMIBOLD, 17, T.text3);
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, cy + J.scroll - a.y + 20, &J.scroll);
    if (del >= 0) { if (J.e[del].text) kfree(J.e[del].text); J.e[del].used = false; j_save(); }
    if ((u->mods & MOD_CMD) && u->key == 'n') { j_edit(-1); u->key_used = true; }
}
static void j_close(window_t *w) { if (J.editing) j_commit(); J.w = NULL; }
static void j_launch(app_t *a) {
    if (J.w) { wm_focus(J.w); return; }
    j_load();
    J.prompt = (int)(rand32() % 8);
    J.w = wm_create(&app_diario, "Diário", -1, -1, 900, 620, WIN_RESIZABLE);
    if (!J.w) return;
    J.w->paint = j_paint;
    J.w->on_close = j_close;
    J.w->toolbar_h = TOOLBAR_H;
    J.w->sidebar_w = 230;
    J.w->min_w = 640;
    J.w->min_h = 460;
}
static void j_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(255, 130, 160), RGB(150, 90, 230));
    gfx_round_rect(s, x + 17 * u, y + 13 * u, 30 * u, 38 * u, 4 * u, RGB(255, 255, 255));
    gfx_fill(s, (int)(x + 17 * u), (int)(y + 13 * u), (int)(5 * u), (int)(38 * u), RGB(230, 225, 245));
    for (int i = 0; i < 4; i++) gfx_fill(s, (int)(x + 26 * u), (int)(y + (22 + i * 6) * u), (int)((i == 3 ? 10 : 16) * u), MAX(1, (int)(1.4f * u)), RGB(170, 150, 220));
}
app_t app_diario = {.name = "Diário", .exec = "diario", .draw_icon = j_icon, .launch = j_launch};

/* ================================================================== Dicas */
typedef struct { const char *title, *text, *app; } tip_t;
typedef struct { const char *name; u32 c1, c2; const tip_t *tips; int n; } tipcol_t;
static const tip_t t_inicio[] = {
    {"Abra qualquer app com a Busca", "Pressione ⌘ Espaço, digite o começo do nome do app ou arquivo e tecle Return. A Busca também faz contas: experimente digitar 12*8.", NULL},
    {"Personalize a mesa", "Nos Ajustes do Sistema, escolha o papel de parede, o modo claro ou escuro e a cor de destaque. O vidro das janelas se adapta ao fundo.", "preferencias"},
    {"Organize-se com o Lembretes", "Crie listas, defina datas e horários e receba avisos mesmo com o app fechado. A lista Hoje reúne o que vence no dia.", "lembretes"},
    {"Tudo no Calendário", "Clique duas vezes num dia para criar um evento. Os feriados nacionais e os aniversários dos Contatos aparecem sozinhos.", "calendario"},
};
static const tip_t t_teclas[] = {
    {"⌘Q, ⌘W e ⌘M", "Encerre o app, feche a janela ou minimize para o Dock.", NULL},
    {"⌘Tab troca de app", "Segure ⌘ e toque Tab para passar pelos apps abertos.", NULL},
    {"⌘⇧5 tira capturas", "Escolha tela inteira, uma janela ou uma seleção. As capturas vão para Imagens e aparecem no Fotos.", "captura"},
    {"⌥⌘Esc força o encerramento", "Se um app parar de responder, encerre-o pela janela Forçar Encerramento.", NULL},
    {"⌃← e ⌃→ trocam de Space", "Apps em tela cheia ganham um Space próprio; use as setas com Control para navegar.", NULL},
};
static const tip_t t_apps[] = {
    {"Grave áudio com o Gravador", "O Gravador usa o microfone do computador. Melhorar Gravação ajusta o volume automaticamente.", "gravador"},
    {"Desenhe funções no Grapher", "Digite y = sin(x) ou y = x^2 e arraste para navegar; a roda do mouse aproxima.", "grapher"},
    {"Jogue Xadrez contra o mOS", "Escolha a dificuldade e desafie o computador. ⌘Z desfaz lances.", "xadrez"},
    {"Conheça o seu computador", "Informações do Sistema mostra processador, memória, discos e rede, e exporta um relatório.", "sysinfo"},
    {"Notas que ficam na mesa", "As Notas Autoadesivas guardam posição, cor e texto. Use o menu Cor para mudar a cor.", "stickies"},
};
static const tipcol_t cols[] = {
    {"Primeiros Passos", RGB(10, 132, 255), RGB(90, 200, 250), t_inicio, ARRAY_LEN(t_inicio)},
    {"Atalhos de Teclado", RGB(255, 149, 0), RGB(255, 204, 0), t_teclas, ARRAY_LEN(t_teclas)},
    {"Apps do Sistema", RGB(175, 82, 222), RGB(255, 100, 160), t_apps, ARRAY_LEN(t_apps)},
};
static struct { window_t *w; int col, scroll; } D;

static void d_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    int sb = w->sidebar_w, y = w->toolbar_h + 4;
    ui_label(u, 18, y + 2, "Coleções", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    for (int i = 0; i < (int)ARRAY_LEN(cols); i++) {
        if (ui_sidebar_item(u, 111000 + i, rect_make(10, y, sb - 20, 28), cols[i].name, D.col == i, NULL)) { D.col = i; D.scroll = 0; }
        y += 30;
    }
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg);
    const tipcol_t *c = &cols[D.col];
    rect_t pc = surface_push_clip(s, a);
    int cy = a.y - D.scroll;
    gfx_grad_h(s, a.x, cy, a.w, 150, c->c1, c->c2);
    ui_label(u, a.x + 30, cy + 54, c->name, FONT_BOLD, 30, RGB(255, 255, 255));
    char sub[48];
    snprintf(sub, sizeof sub, "%d dicas", c->n);
    ui_label(u, a.x + 32, cy + 98, sub, FONT_SEMIBOLD, 13, RGBA(255, 255, 255, 210));
    cy += 170;
    int cw = MIN(620, a.w - 60), cx = a.x + (a.w - cw) / 2;
    for (int i = 0; i < c->n; i++) {
        const tip_t *t = &c->tips[i];
        int th = text_wrap(s, font_get(FONT_REGULAR), 14, rect_make(0, 0, cw - 40, 1000), 21, T.text, t->text, false);
        int h = 62 + th + (t->app ? 40 : 0);
        rect_t r = rect_make(cx, cy, cw, h);
        gfx_shadow(s, r.x, r.y, r.w, r.h, 14, 12, 30, 3);
        gfx_round_rect(s, r.x, r.y, r.w, r.h, 14, T.dark ? RGB(44, 44, 48) : RGB(255, 255, 255));
        gfx_round_rect(s, r.x + 20, r.y + 18, 6, 22, 3, c->c1);
        text_draw(s, font_get(FONT_BOLD), 17, r.x + 36, r.y + 36, T.text, t->title);
        text_wrap(s, font_get(FONT_REGULAR), 14, rect_make(r.x + 20, r.y + 50, cw - 40, th + 10), 21, T.text2, t->text, true);
        if (t->app && ui_button_style(u, 111100 + D.col * 20 + i, rect_make(r.x + 20, r.y + h - 38, 120, 26), "Experimentar", BTN_TINTED | BTN_CAPSULE, true, NULL))
            app_launch(apps_find(t->app));
        cy += h + 16;
    }
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, a, cy + D.scroll - a.y + 20, &D.scroll);
}
static void d_close(window_t *w) { D.w = NULL; }
static void d_launch(app_t *a) {
    if (D.w) { wm_focus(D.w); return; }
    D.w = wm_create(&app_dicas, "Dicas", -1, -1, 880, 620, WIN_RESIZABLE);
    if (!D.w) return;
    D.w->paint = d_paint;
    D.w->on_close = d_close;
    D.w->toolbar_h = TOOLBAR_H;
    D.w->sidebar_w = 220;
}
static void d_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(255, 214, 10), RGB(255, 160, 0));
    gfx_circle(s, x + 32 * u, y + 28 * u, 13 * u, RGB(255, 255, 255));
    gfx_round_rect(s, x + 26 * u, y + 38 * u, 12 * u, 10 * u, 2 * u, RGB(255, 255, 255));
    gfx_fill(s, (int)(x + 27 * u), (int)(y + 42 * u), (int)(10 * u), MAX(1, (int)u), RGB(255, 180, 0));
}
app_t app_dicas = {.name = "Dicas", .exec = "dicas", .draw_icon = d_icon, .launch = d_launch};
