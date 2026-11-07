/* mOS — Automator (fluxos de trabalho com arquivos: obter itens de uma pasta, filtrar por nome/extensão,
 * renomear em sequência, copiar, mover, criar pasta, redimensionar e converter imagens para BMP, notificar),
 * executados em sequência com relatório; e Captura de Imagem (importa fotos de câmeras e cartões de memória
 * montados como disco — pasta DCIM em /Volumes — ou de qualquer pasta escolhida, para Imagens). */
#include "kit.h"

extern app_t app_automator, app_imagecapture;

/* ================================================================== Automator */
enum { AA_GET, AA_FILTER_EXT, AA_FILTER_NAME, AA_RENAME, AA_COPY, AA_MOVE, AA_MKDIR, AA_RESIZE, AA_TOBMP, AA_NOTIFY, AA_KINDS };
static const char *aa_names[AA_KINDS] = {"Obter Itens da Pasta", "Filtrar por Extensão", "Filtrar por Nome", "Renomear em Sequência",
                                         "Copiar para Pasta", "Mover para Pasta", "Criar Pasta", "Redimensionar Imagens (%)", "Converter Imagens para BMP", "Mostrar Notificação"};
static const char *aa_hint[AA_KINDS] = {"Pasta (ex.: ~/Imagens)", "Extensões (ex.: png,jpg)", "Texto contido no nome", "Prefixo (ex.: Foto)",
                                        "Pasta de destino", "Pasta de destino", "Caminho da nova pasta", "Porcentagem (ex.: 50)", "", "Mensagem"};
#define MAXSTEP 16
#define MAXITEMS 256
typedef struct { int kind; char arg[160]; } astep_t;
static struct {
    window_t *w;
    astep_t st[MAXSTEP];
    int n, add_kind;
    char items[MAXITEMS][220];
    int nitems;
    char log[4096];
    char name[64];
    int scroll;
    bool loaded;
} AU;

static void au_save(void) {
    sbuf_t b = {0};
    sb_printf(&b, "N\t%s\n", AU.name);
    for (int i = 0; i < AU.n; i++) { char a[160]; kit_field(a, 160, AU.st[i].arg); sb_printf(&b, "S\t%d\t%s\n", AU.st[i].kind, a); }
    char p[200];
    kit_save(kit_lib("Automator", "fluxo.tsv", p, sizeof p), &b);
}
static void au_load(void) {
    if (AU.loaded) return;
    AU.loaded = true;
    char p[200];
    char *buf = kit_read(kit_lib("Automator", "fluxo.tsv", p, sizeof p), NULL);
    if (!buf) {
        snprintf(AU.name, 64, "Organizar Capturas de Tela");
        AU.st[0] = (astep_t){AA_GET, "~/Imagens"};
        AU.st[1] = (astep_t){AA_FILTER_NAME, "Captura"};
        AU.st[2] = (astep_t){AA_MKDIR, "~/Imagens/Capturas"};
        AU.st[3] = (astep_t){AA_MOVE, "~/Imagens/Capturas"};
        AU.st[4] = (astep_t){AA_NOTIFY, "Capturas organizadas"};
        AU.n = 5;
        return;
    }
    char *cur = buf, *ln, *f[4];
    while ((ln = kit_line(&cur))) {
        int n = kit_split(ln, f, 4);
        if (!strcmp(f[0], "N") && n >= 2) snprintf(AU.name, 64, "%s", f[1]);
        else if (!strcmp(f[0], "S") && n >= 3 && AU.n < MAXSTEP) { AU.st[AU.n].kind = CLAMP(atoi(f[1]), 0, AA_KINDS - 1); snprintf(AU.st[AU.n].arg, 160, "%s", f[2]); AU.n++; }
    }
    kfree(buf);
}
static void expand(const char *in, char *out, int n) {
    if (in[0] == '~') snprintf(out, n, "%s%s", CUR_HOME, in + 1);
    else snprintf(out, n, "%s", in);
    int l = (int)strlen(out);
    while (l > 1 && out[l - 1] == '/') out[--l] = 0;
}
static void logf_(const char *fmt, ...) {
    char t[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(t, sizeof t, fmt, ap);
    va_end(ap);
    usize l = strlen(AU.log);
    snprintf(AU.log + l, sizeof AU.log - l, "%s\n", t);
}
static const char *base(const char *p) { const char *s = strrchr(p, '/'); return s ? s + 1 : p; }
static bool copy_file(const char *src, const char *dir, char *outp, int n) {
    vnode_t *v = vfs_lookup(src);
    if (!v || v->dir) return false;
    vfs_ensure(v);
    snprintf(outp, n, "%s/%s", dir, v->name);
    return vfs_write_file(outp, v->data, v->size) != NULL;
}
static void au_run(void) {
    AU.log[0] = 0;
    AU.nitems = 0;
    logf_("Executando \"%s\"", AU.name);
    for (int s = 0; s < AU.n; s++) {
        astep_t *a = &AU.st[s];
        char arg[220];
        expand(a->arg, arg, sizeof arg);
        int before = AU.nitems, done = 0;
        switch (a->kind) {
        case AA_GET: {
            vnode_t *d = vfs_lookup(arg);
            if (!d || !d->dir) { logf_("✗ %d. pasta não encontrada: %s", s + 1, arg); return; }
            AU.nitems = 0;
            for (vnode_t *c = d->child; c && AU.nitems < MAXITEMS; c = c->next) if (!c->dir && c->name[0] != '.') snprintf(AU.items[AU.nitems++], 220, "%s/%s", arg, c->name);
            done = AU.nitems;
        } break;
        case AA_FILTER_EXT: case AA_FILTER_NAME: {
            int k = 0;
            for (int i = 0; i < AU.nitems; i++) {
                const char *nm = base(AU.items[i]);
                bool keep = false;
                if (a->kind == AA_FILTER_NAME) {
                    char l1[200], l2[160];
                    snprintf(l1, sizeof l1, "%s", nm); snprintf(l2, sizeof l2, "%s", a->arg);
                    for (char *c = l1; *c; c++) *c = (char)tolower(*c);
                    for (char *c = l2; *c; c++) *c = (char)tolower(*c);
                    keep = strstr(l1, l2) != NULL;
                } else {
                    const char *e = vfs_ext(nm);
                    char list[160];
                    snprintf(list, sizeof list, "%s", a->arg);
                    for (char *t = list; *t;) {
                        char *c = strchr(t, ',');
                        if (c) *c = 0;
                        while (*t == ' ' || *t == '.') t++;
                        if (*t && !strcasecmp(t, e)) keep = true;
                        if (!c) break;
                        t = c + 1;
                    }
                }
                if (keep) { if (k != i) memcpy(AU.items[k], AU.items[i], 220); k++; }
            }
            AU.nitems = k;
            done = k;
        } break;
        case AA_RENAME:
            for (int i = 0; i < AU.nitems; i++) {
                vnode_t *v = vfs_lookup(AU.items[i]);
                if (!v) continue;
                char nn[200];
                const char *e = vfs_ext(v->name);
                snprintf(nn, sizeof nn, "%s %03d%s%s", a->arg[0] ? a->arg : "Item", i + 1, e[0] ? "." : "", e);
                if (vfs_move(v, v->parent, nn)) { char *sl = strrchr(AU.items[i], '/'); snprintf(sl + 1, 220 - (sl + 1 - AU.items[i]), "%s", nn); done++; }
            }
            break;
        case AA_MKDIR: vfs_mkdir(arg); done = vfs_lookup(arg) ? 1 : 0; break;
        case AA_COPY: case AA_MOVE: {
            vnode_t *dd = vfs_mkdir(arg);
            for (int i = 0; i < AU.nitems; i++) {
                vnode_t *v = vfs_lookup(AU.items[i]);
                if (!v) continue;
                if (a->kind == AA_MOVE) { char nm[200]; snprintf(nm, sizeof nm, "%s", v->name); if (vfs_move(v, dd, nm)) { snprintf(AU.items[i], 220, "%s/%s", arg, nm); done++; } }
                else { char np[220]; if (copy_file(AU.items[i], arg, np, sizeof np)) { snprintf(AU.items[i], 220, "%s", np); done++; } }
            }
        } break;
        case AA_RESIZE: case AA_TOBMP: {
            int pct = a->kind == AA_RESIZE ? CLAMP(atoi(a->arg), 1, 400) : 100;
            for (int i = 0; i < AU.nitems; i++) {
                if (!kit_is_image(AU.items[i])) continue;
                surface_t *im = kit_load_image(AU.items[i]);
                if (!im) continue;
                int nw = MAX(1, im->w * pct / 100), nh = MAX(1, im->h * pct / 100);
                surface_t *o = surface_new(nw, nh);
                gfx_blit_scaled(o, im, 0, 0, nw, nh, 255);
                char out[230];
                snprintf(out, sizeof out, "%s", AU.items[i]);
                char *dot = strrchr(out, '.');
                if (dot) *dot = 0;
                strcat(out, a->kind == AA_RESIZE ? " (redimensionada).bmp" : ".bmp");
                if (kit_save_bmp(out, o)) { snprintf(AU.items[i], 220, "%s", out); done++; }
                surface_free(o);
                surface_free(im);
            }
        } break;
        case AA_NOTIFY: notify(AU.name, a->arg); done = 1; break;
        }
        logf_("✓ %d. %s — %d %s", s + 1, aa_names[a->kind], done, a->kind <= AA_FILTER_NAME ? "itens" : a->kind == AA_NOTIFY || a->kind == AA_MKDIR ? "feito" : "processados");
        (void)before;
    }
    logf_("Fluxo concluído: %d item(ns) no resultado.", AU.nitems);
    sound_alert();
}

static void au_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    au_load();
    int sb = w->sidebar_w;
    /* biblioteca de ações */
    int y = w->toolbar_h + 4;
    ui_label(u, 18, y + 2, "Biblioteca de Ações", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    for (int i = 0; i < AA_KINDS; i++) {
        rect_t r = rect_make(10, y, sb - 20, 26);
        if (ui_sidebar_item(u, 132000 + i, r, aa_names[i], false, NULL) && AU.n < MAXSTEP) { AU.st[AU.n].kind = i; AU.st[AU.n].arg[0] = 0; AU.n++; au_save(); }
        y += 27;
    }
    ui_label(u, 18, y + 8, "Clique numa ação para adicioná-la.", FONT_REGULAR, 11, T.text3);
    /* barra de ferramentas */
    ui_textfield(u, UI_ID, rect_make(sb + 14, TOOLBAR_Y(28), 260, 28), AU.name, sizeof AU.name, "Nome do fluxo");
    if (ui_button_style(u, UI_ID, rect_make(u->cw - 12 - 100, TOOLBAR_Y(28), 100, 28), "Executar", BTN_FILLED, AU.n > 0, NULL)) { au_save(); au_run(); }
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.dark ? RGB(30, 30, 32) : RGB(240, 240, 244));
    int logh = 150;
    rect_t view = rect_make(a.x, a.y, a.w, a.h - logh);
    rect_t pc = surface_push_clip(s, view);
    int cy = view.y + 16 - AU.scroll, cw = MIN(620, a.w - 40), cx = a.x + (a.w - cw) / 2;
    int del = -1, up = -1;
    for (int i = 0; i < AU.n; i++) {
        astep_t *st = &AU.st[i];
        bool has_arg = st->kind != AA_TOBMP;
        int h = has_arg ? 82 : 48;
        rect_t r = rect_make(cx, cy, cw, h);
        gfx_shadow(s, r.x, r.y, r.w, r.h, 10, 6, 30, 2);
        gfx_round_rect(s, r.x, r.y, r.w, r.h, 10, T.content_bg);
        gfx_round_rect_ex(s, r.x, r.y, r.w, 32, 10, 10, 0, 0, T.dark ? RGB(56, 56, 60) : RGB(228, 230, 236), T.dark ? RGB(50, 50, 54) : RGB(220, 222, 228));
        char t[96];
        snprintf(t, sizeof t, "%d  %s", i + 1, aa_names[st->kind]);
        text_draw(s, font_get(FONT_SEMIBOLD), 13, r.x + 12, r.y + 21, T.text, t);
        if (ui_button_style(u, 132100 + i, rect_make(r.x + r.w - 34, r.y + 3, 26, 26), "×", BTN_PLAIN_GRAY, true, NULL)) del = i;
        if (i && ui_button_style(u, 132200 + i, rect_make(r.x + r.w - 64, r.y + 3, 26, 26), "↑", BTN_PLAIN_GRAY, true, NULL)) up = i;
        if (has_arg) ui_textfield(u, 132300 + i, rect_make(r.x + 12, r.y + 42, r.w - 24, 28), st->arg, sizeof st->arg, aa_hint[st->kind]);
        cy += h;
        if (i < AU.n - 1) { gfx_line(s, cx + cw / 2.0f, cy, cx + cw / 2.0f, cy + 14, 2, T.text3); cy += 14; }
    }
    if (!AU.n) ui_label_center(u, rect_make(a.x, view.y + 60, a.w, 20), "Arraste… ou clique em ações à esquerda para montar o fluxo", FONT_REGULAR, 14, T.text3);
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, cy + AU.scroll - view.y + 20, &AU.scroll);
    if (del >= 0) { memmove(&AU.st[del], &AU.st[del + 1], sizeof(astep_t) * (AU.n - del - 1)); AU.n--; au_save(); }
    if (up > 0) { astep_t t = AU.st[up]; AU.st[up] = AU.st[up - 1]; AU.st[up - 1] = t; au_save(); }
    /* registro */
    rect_t lg = rect_make(a.x, a.y + a.h - logh, a.w, logh);
    gfx_fill(s, lg.x, lg.y, lg.w, lg.h, T.content_bg);
    gfx_fill(s, lg.x, lg.y, lg.w, 1, T.separator);
    ui_label(u, lg.x + 16, lg.y + 8, "Registro", FONT_BOLD, 12, T.text2);
    text_wrap(s, font_get(FONT_REGULAR), 12, rect_make(lg.x + 16, lg.y + 28, lg.w - 32, logh - 34), 17, T.text, AU.log[0] ? AU.log : "Execute o fluxo para ver o resultado de cada ação.", true);
}
static void au_close(window_t *w) { au_save(); AU.w = NULL; }
static void au_launch(app_t *a) {
    if (AU.w) { wm_focus(AU.w); return; }
    AU.w = wm_create(&app_automator, "Automator", -1, -1, 980, 640, WIN_RESIZABLE);
    if (!AU.w) return;
    AU.w->paint = au_paint;
    AU.w->on_close = au_close;
    AU.w->toolbar_h = TOOLBAR_H;
    AU.w->sidebar_w = 250;
    AU.w->title_x = -1;
}
static void au_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(180, 180, 190), RGB(110, 110, 122));
    gfx_circle(s, x + 32 * u, y + 26 * u, 13 * u, RGB(230, 230, 236));
    gfx_circle(s, x + 27 * u, y + 24 * u, 2.5f * u, RGB(40, 40, 50));
    gfx_circle(s, x + 37 * u, y + 24 * u, 2.5f * u, RGB(40, 40, 50));
    gfx_round_rect(s, x + 20 * u, y + 38 * u, 24 * u, 14 * u, 3 * u, RGB(230, 230, 236));
    gfx_line(s, x + 32 * u, y + 13 * u, x + 32 * u, y + 8 * u, 2 * u, RGB(230, 230, 236));
}
app_t app_automator = {.name = "Automator", .exec = "automator", .draw_icon = au_icon, .launch = au_launch};

/* ================================================================== Captura de Imagem */
#define MAXIC 300
static struct {
    window_t *w;
    char devs[16][200], devname[16][96];
    int ndev, dev;
    char files[MAXIC][120];
    int nf;
    bool sel[MAXIC];
    surface_t *thumb[MAXIC];
    char dest[200];
    bool del_after;
    int scroll;
    u64 last_scan;
} IC;

static void ic_add_dev(const char *path, const char *name) {
    if (IC.ndev >= 16) return;
    for (int i = 0; i < IC.ndev; i++) if (!strcmp(IC.devs[i], path)) return;
    snprintf(IC.devs[IC.ndev], 200, "%s", path);
    snprintf(IC.devname[IC.ndev], 96, "%s", name);
    IC.ndev++;
}
static void scan_dcim(const char *dir, int depth) {
    vnode_t *d = vfs_lookup(dir);
    for (vnode_t *c = d && d->dir ? d->child : NULL; c; c = c->next) {
        if (!c->dir) continue;
        char p[200];
        snprintf(p, sizeof p, "%s/%s", dir, c->name);
        if (!strcasecmp(c->name, "DCIM")) ic_add_dev(p, depth == 0 ? c->name : dir + (strrchr(dir, '/') ? strrchr(dir, '/') - dir + 1 : 0));
        else if (depth < 1) scan_dcim(p, depth + 1);
    }
}
static void ic_scan_devices(void) {
    IC.ndev = 0;
    scan_dcim("/Volumes", 0);
    /* volumes sem DCIM também servem (cartões e pendrives com fotos soltas) */
    vnode_t *v = vfs_lookup("/Volumes");
    for (vnode_t *c = v && v->dir ? v->child : NULL; c; c = c->next) if (c->dir) { char p[200]; snprintf(p, sizeof p, "/Volumes/%s", c->name); ic_add_dev(p, c->name); }
    char dl[200];
    snprintf(dl, sizeof dl, "%s/Downloads", CUR_HOME);
    ic_add_dev(dl, "Downloads");
    IC.dev = CLAMP(IC.dev, 0, IC.ndev - 1);
}
static void ic_collect(const char *dir, int depth) {
    vnode_t *d = vfs_lookup(dir);
    for (vnode_t *c = d && d->dir ? d->child : NULL; c && IC.nf < MAXIC; c = c->next) {
        char p[260];
        snprintf(p, sizeof p, "%s/%s", dir, c->name);
        if (c->dir) { if (depth < 3) ic_collect(p, depth + 1); continue; }
        if (!kit_is_image(c->name) && strcasecmp(vfs_ext(c->name), "ppm")) continue;
        snprintf(IC.files[IC.nf], 120, "%s", p + strlen(IC.devs[IC.dev]) + 1);
        IC.sel[IC.nf] = false;
        IC.thumb[IC.nf] = NULL;
        IC.nf++;
    }
}
static void ic_load_dev(void) {
    for (int i = 0; i < IC.nf; i++) if (IC.thumb[i]) surface_free(IC.thumb[i]);
    IC.nf = 0;
    if (IC.ndev) ic_collect(IC.devs[IC.dev], 0);
}
static int ic_import(bool all) {
    char dest[200];
    expand(IC.dest[0] ? IC.dest : "~/Imagens", dest, sizeof dest);
    vfs_mkdir(dest);
    int n = 0;
    for (int i = 0; i < IC.nf; i++) {
        if (!all && !IC.sel[i]) continue;
        char src[330], out[260];
        snprintf(src, sizeof src, "%s/%s", IC.devs[IC.dev], IC.files[i]);
        if (copy_file(src, dest, out, sizeof out)) {
            n++;
            if (IC.del_after) { vnode_t *v = vfs_lookup(src); if (v && !vfs_is_read_only(v)) vfs_delete(v); }
        }
    }
    char m[96];
    snprintf(m, sizeof m, "%d foto%s importada%s para %s.", n, n == 1 ? "" : "s", n == 1 ? "" : "s", base(dest));
    notify("Captura de Imagem", m);
    if (IC.del_after) ic_load_dev();
    return n;
}
static void ic_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    if (timer_ticks() - IC.last_scan > 3000) {
        int old = IC.ndev;
        char cur[200];
        snprintf(cur, sizeof cur, "%s", IC.ndev ? IC.devs[IC.dev] : "");
        ic_scan_devices();
        IC.last_scan = timer_ticks();
        if (old != IC.ndev || (IC.ndev && strcmp(cur, IC.devs[IC.dev]))) ic_load_dev();
    }
    int sb = w->sidebar_w, y = w->toolbar_h + 4;
    ui_label(u, 18, y + 2, "Dispositivos", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    for (int i = 0; i < IC.ndev; i++) {
        if (ui_sidebar_item(u, 133000 + i, rect_make(10, y, sb - 20, 28), IC.devname[i], IC.dev == i, glyph_sb_photo)) { IC.dev = i; ic_load_dev(); }
        y += 30;
    }
    ui_label(u, 18, y + 10, "Câmeras e cartões aparecem aqui quando montados.", FONT_REGULAR, 11, T.text3);
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg);
    int bar = 50;
    rect_t view = rect_make(a.x, a.y, a.w, a.h - bar);
    rect_t pc = surface_push_clip(s, view);
    int ts = 130, gap = 14, cols = MAX(1, (a.w - 30 + gap) / (ts + gap)), dec = 0, nsel = 0;
    for (int i = 0; i < IC.nf; i++) {
        if (IC.sel[i]) nsel++;
        rect_t r = rect_make(a.x + 16 + (i % cols) * (ts + gap), view.y + 14 + (i / cols) * (ts + 34) - IC.scroll, ts, ts);
        if (r.y + ts < view.y || r.y > view.y + view.h) continue;
        if (!IC.thumb[i] && dec < 2) {
            char p[330];
            snprintf(p, sizeof p, "%s/%s", IC.devs[IC.dev], IC.files[i]);
            surface_t *im = kit_load_image(p);
            dec++;
            if (im) { float sc = MIN((float)ts / im->w, (float)ts / im->h); int tw = MAX(1, (int)(im->w * sc)), th = MAX(1, (int)(im->h * sc)); IC.thumb[i] = surface_new(tw, th); gfx_blit_scaled(IC.thumb[i], im, 0, 0, tw, th, 255); surface_free(im); }
            else IC.thumb[i] = surface_new(1, 1);
        }
        if (IC.sel[i]) gfx_round_rect(s, r.x - 4, r.y - 4, r.w + 8, r.h + 30, 10, T.sidebar_sel);
        if (IC.thumb[i] && IC.thumb[i]->w > 1) gfx_blit(s, IC.thumb[i], r.x + (ts - IC.thumb[i]->w) / 2, r.y + (ts - IC.thumb[i]->h) / 2);
        else gfx_round_rect(s, r.x + 20, r.y + 20, ts - 40, ts - 40, 8, T.alt_row);
        text_draw_ellipsis(s, font_get(FONT_REGULAR), 11, r.x, r.y + ts + 16, ts, T.text2, base(IC.files[i]));
        if (ui_clicked(u, 133100 + i, rect_make(r.x - 4, r.y - 4, r.w + 8, r.h + 30))) { if (!(u->mods & (MOD_CMD | MOD_SHIFT))) for (int k = 0; k < IC.nf; k++) IC.sel[k] = false; IC.sel[i] = !IC.sel[i]; }
    }
    if (!IC.nf) ui_label_center(u, rect_make(a.x, view.y + view.h / 2 - 20, a.w, 24), IC.ndev ? "Nenhuma imagem neste dispositivo" : "Nenhum dispositivo conectado", FONT_SEMIBOLD, 16, T.text3);
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, (IC.nf + cols - 1) / cols * (ts + 34) + 20, &IC.scroll);
    if (dec) u->repaint = true;
    /* barra inferior: destino e importar */
    int by = a.y + a.h - bar + 10;
    gfx_fill(s, a.x, by - 10, a.w, 1, T.separator);
    ui_label(u, a.x + 16, by + 6, "Importar para:", FONT_REGULAR, 12.5f, T.text2);
    ui_textfield(u, UI_ID, rect_make(a.x + 110, by, 200, 28), IC.dest, sizeof IC.dest, "~/Imagens");
    ui_checkbox(u, UI_ID, a.x + 324, by + 6, "Apagar depois", &IC.del_after);
    if (ui_button_style(u, UI_ID, rect_make(a.x + a.w - 16 - 120, by, 120, 28), "Importar Tudo", BTN_FILLED, IC.nf > 0, NULL)) ic_import(true);
    if (ui_button_style(u, UI_ID, rect_make(a.x + a.w - 16 - 120 - 8 - 100, by, 100, 28), "Importar", BTN_GRAY, nsel > 0, NULL)) ic_import(false);
}
static void ic_close(window_t *w) { IC.w = NULL; }
static void ic_launch(app_t *a) {
    if (IC.w) { wm_focus(IC.w); return; }
    IC.w = wm_create(&app_imagecapture, "Captura de Imagem", -1, -1, 960, 620, WIN_RESIZABLE);
    if (!IC.w) return;
    IC.w->paint = ic_paint;
    IC.w->on_close = ic_close;
    IC.w->toolbar_h = TOOLBAR_H;
    IC.w->sidebar_w = 230;
    IC.last_scan = 0;
}
static void ic_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(230, 230, 236), RGB(180, 182, 190));
    gfx_round_rect(s, x + 12 * u, y + 22 * u, 40 * u, 26 * u, 5 * u, RGB(60, 60, 66));
    gfx_round_rect(s, x + 24 * u, y + 17 * u, 16 * u, 7 * u, 2 * u, RGB(60, 60, 66));
    gfx_circle(s, x + 32 * u, y + 35 * u, 8 * u, RGB(200, 200, 206));
    gfx_circle(s, x + 32 * u, y + 35 * u, 5 * u, RGB(70, 120, 200));
}
app_t app_imagecapture = {.name = "Captura de Imagem", .exec = "imagecapture", .draw_icon = ic_icon, .launch = ic_launch};
