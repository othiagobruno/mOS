/* mOS — Arquivos: gerenciador de arquivos */
#include "apps.h"
#include "account.h"
#include "compat.h"
#include "launchsvc.h"

enum { A_EJECT = 40, A_NEWFOLDER = 1, A_NEWWINDOW, A_TRASH, A_EMPTYTRASH, A_ICONS, A_LIST, A_GO_HOME, A_GO_DOCS, A_GO_APPS, A_GO_DL, A_NEWFILE, A_GETINFO,
       A_NEWTAB = 60, A_OPEN, A_RENAME, A_COLS, A_GALLERY, A_GROUPS, A_SORT0 = 70 /* +0..3 */, A_TAG0 = 80 /* +1..7 */ };

static menu_item_t m_file[] = {
    {"Nova Janela", "⌘N", A_NEWWINDOW}, {"Nova Aba", "⌘T", A_NEWTAB}, {"Nova Pasta", "⇧⌘N", A_NEWFOLDER}, {"Novo Documento de Texto", NULL, A_NEWFILE},
    {NULL}, {"Abrir", "⌘O", A_OPEN}, {"Renomear", NULL, A_RENAME}, {"Obter Informações", "⌘I", A_GETINFO},
    {NULL}, {"Mover para a Lixeira", "⌘⌫", A_TRASH}, {"Esvaziar Lixeira…", NULL, A_EMPTYTRASH}, {NULL}, {"Ejetar", "⌘E", A_EJECT},
};
static menu_item_t m_edit[] = {{"Desfazer", "⌘Z", 0, true}, {NULL}, {"Copiar", "⌘C", 0, true}, {"Colar", "⌘V", 0, true}, {"Selecionar Tudo", "⌘A", 0, true}};
static menu_item_t m_sort[] = {{"Nome", NULL, A_SORT0}, {"Tipo", NULL, A_SORT0 + 1}, {"Data de Modificação", NULL, A_SORT0 + 2}, {"Tamanho", NULL, A_SORT0 + 3}};
static menu_t sort_menu = {"Ordenar por", m_sort, ARRAY_LEN(m_sort)};
static menu_item_t m_view[] = {
    {"como Ícones", "⌘1", A_ICONS}, {"como Lista", "⌘2", A_LIST}, {"como Colunas", "⌘3", A_COLS}, {"como Galeria", "⌘4", A_GALLERY},
    {NULL}, {"Ordenar por", NULL, 0, false, false, NULL, false, &sort_menu}, {"Usar Grupos", "⌃⌘0", A_GROUPS},
};
static menu_item_t m_go[] = {{"Documentos", "⇧⌘O", A_GO_DOCS}, {"Mesa", NULL, A_GO_HOME}, {"Downloads", "⌥⌘L", A_GO_DL}, {"Aplicativos", "⇧⌘A", A_GO_APPS}};
static menu_t menus[] = {
    {"Arquivo", m_file, ARRAY_LEN(m_file)}, {"Editar", m_edit, ARRAY_LEN(m_edit)},
    {"Visualizar", m_view, ARRAY_LEN(m_view)}, {"Ir", m_go, ARRAY_LEN(m_go)},
};

typedef struct {
    char path[512];
    char hist[16][512];
    int nhist, hpos;
    int sel;
    int view;  /* 0 ícones, 1 lista, 2 colunas */
    int scroll;
    char search[64];
    bool info;
    bool ql;               /* Visualização Rápida aberta */
    char ql_path[256];     /* arquivo cuja imagem está em ql_img */
    surface_t *ql_img;
    /* seleção múltipla (⌘ clique, ⇧ clique, ⌘A, retângulo) e arrastar e soltar */
    bool msel[256];
    int anchor;
    int press_item, press_x, press_y;
    bool dragging, band;
    int band_x, band_y;
    rect_t band_r;
    struct { rect_t r; char path[256]; } drops[40];
    int ndrops;
    /* ordenação, grupos, renomear, colunas e etiquetas */
    int sort;              /* 0 nome, 1 tipo, 2 data, 3 tamanho */
    bool groups;           /* agrupar por tipo */
    int renaming;          /* item em renomeação (-1 nenhum) */
    char rename_buf[128];
    bool rename_started;
    int colw[3];           /* lista: larguras de Nome, Data e Tamanho (Tipo fica com o resto) */
    int col_drag, col_x0, col_w0;
    int colsw;             /* visualização em colunas: largura de cada coluna */
    int tag;               /* etiqueta mostrada no lugar de uma pasta (0 = nenhuma) */
    bool key_open, key_rename, key_info, key_trash;   /* pedidos dos menus, atendidos no próximo quadro */
} fstate_t;

extern void popup_open(int sx, int sy, int w, const char **items, int n, int sel, window_t *win, int id);
extern bool popup_take_result(window_t *win, int id, int *val);
extern void popup_open_menu(int sx, int sy, int w, const menu_item_t *items, int n, window_t *win, int id);
extern surface_t *image_decode(const u8 *d, usize n);
#define CTX_ITEM_ID  0x7A001
#define CTX_BG_ID    0x7A002

static void go(fstate_t *s, const char *path, bool record);
static bool is_sel(fstate_t *s, int i);
static void paint(window_t *w, ui_t *u);   /* janelas de navegação (as de "Obter Informações" pintam com outra) */

static void glyph_eject(surface_t *sf, int x, int y, int sz, u32 c) {
    float tri[] = {x + sz / 2.0f, y + 1.0f, x + sz - 1.0f, y + sz * 0.58f, x + 1.0f, y + sz * 0.58f};
    gfx_polygon(sf, tri, 3, c);
    gfx_fill(sf, x + 1, y + (int)(sz * 0.72f), sz - 2, MAX(2, sz / 6), c);
}

/* ejeta um volume: antes tira de dentro dele as janelas do Arquivos e o cwd do Terminal */
void eject_volume(vnode_t *vol) {
    if (!vol || !vfs_is_volume(vol)) return;
    char vp[512], name[256];
    vfs_path(vol, vp, sizeof vp);
    strncpy(name, vol->name, sizeof name - 1);
    name[sizeof name - 1] = 0;
    usize n = strlen(vp);
    extern window_t *wm_nth(int);
    for (int i = 0;; i++) {
        window_t *w = wm_nth(i);
        if (!w) break;
        if (w->app == &app_arquivos && w->data && w->paint == paint) {
            fstate_t *s = w->data;
            if (!strncmp(s->path, vp, n) && (s->path[n] == 0 || s->path[n] == '/')) go(s, home_sub("/Documentos"), true);
            for (int k = 0; k < s->nhist; k++)
                if (!strncmp(s->hist[k], vp, n)) strcpy(s->hist[k], home_sub("/Documentos"));
            wm_invalidate(w);
        }
    }
    extern void terminal_leave_path(const char *prefix);
    terminal_leave_path(vp);
    if (vfs_eject(vol)) {
        char msg[300];
        snprintf(msg, sizeof msg, "\"%s\" pode ser removido com segurança.", name);
        notify("Volume ejetado", msg);
    } else {
        notify("Não foi possível ejetar", name);
    }
    wm_damage_all();
}

static void go(fstate_t *s, const char *path, bool record) {
    vnode_t *n = vfs_lookup(path);
    if (!n || !n->dir) return;
    vfs_path(n, s->path, sizeof s->path);
    s->sel = -1;
    memset(s->msel, 0, sizeof s->msel);
    s->press_item = -1;
    s->dragging = s->band = false;
    s->scroll = 0;
    s->search[0] = 0;
    s->tag = 0;
    s->renaming = -1;
    if (record) {
        if (s->hpos < s->nhist - 1) s->nhist = s->hpos + 1;
        if (s->nhist == 16) { memmove(s->hist, s->hist + 1, sizeof(s->hist[0]) * 15); s->nhist--; }
        strcpy(s->hist[s->nhist++], s->path);
        s->hpos = s->nhist - 1;
    }
}

static const char *title_for(const char *path) {
    if (!strcmp(path, "/")) return "mOS HD";
    if (!strcmp(path, CUR_TRASH)) return "Lixeira";
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}

/* ---------------- etiquetas de cor: guardadas por caminho em /Sistema/Biblioteca/Preferencias/com.mos.tags.cfg
 * (uma linha por item: "<bits>\t<caminho>"; bit k-1 = etiqueta k) */
#define TAGS_CFG "/Sistema/Biblioteca/Preferencias/com.mos.tags.cfg"
static const char *tag_names[8] = {"", "Vermelho", "Laranja", "Amarelo", "Verde", "Azul", "Roxo", "Cinza"};
static const u32 tag_cols[8] = {0, RGB(255, 69, 58), RGB(255, 159, 10), RGB(255, 204, 0), RGB(48, 199, 88), RGB(10, 132, 255), RGB(175, 82, 222), RGB(152, 152, 157)};
static struct { char path[200]; u8 bits; } tagtab[128];
static int ntags = -1;

static void tags_load(void) {
    if (ntags >= 0) return;
    ntags = 0;
    vnode_t *f = vfs_lookup(TAGS_CFG);
    if (!f || f->dir || !f->data) return;
    const char *p = (const char *)f->data, *end = p + f->size;
    while (p < end && ntags < (int)ARRAY_LEN(tagtab)) {
        int bits = 0;
        while (p < end && *p >= '0' && *p <= '9') bits = bits * 10 + (*p++ - '0');
        if (p < end && *p == '\t') p++;
        int k = 0;
        while (p < end && *p != '\n' && k < 199) tagtab[ntags].path[k++] = *p++;
        tagtab[ntags].path[k] = 0;
        if (p < end) p++;
        if (k && bits) tagtab[ntags++].bits = (u8)bits;
    }
}
static void tags_save(void) {
    char *b = kmalloc(ntags * 210 + 16);
    if (!b) return;
    int n = 0;
    for (int i = 0; i < ntags; i++) if (tagtab[i].bits) n += snprintf(b + n, 210, "%d\t%s\n", tagtab[i].bits, tagtab[i].path);
    vfs_mkdir("/Sistema/Biblioteca/Preferencias");
    vfs_write_file(TAGS_CFG, b, n);
    kfree(b);
}
static u8 tag_bits_path(const char *path) {
    tags_load();
    for (int i = 0; i < ntags; i++) if (!strcmp(tagtab[i].path, path)) return tagtab[i].bits;
    return 0;
}
static u8 tag_bits(vnode_t *n) {
    if (!n) return 0;
    char p[256];
    vfs_path(n, p, sizeof p);
    return tag_bits_path(p);
}
static void tag_toggle(vnode_t *n, int t) {
    if (!n || t < 1 || t > 7) return;
    char p[256];
    vfs_path(n, p, sizeof p);
    tags_load();
    for (int i = 0; i < ntags; i++)
        if (!strcmp(tagtab[i].path, p)) { tagtab[i].bits ^= (u8)(1 << (t - 1)); tags_save(); return; }
    if (ntags >= (int)ARRAY_LEN(tagtab)) return;
    strncpy(tagtab[ntags].path, p, sizeof tagtab[0].path - 1);
    tagtab[ntags++].bits = (u8)(1 << (t - 1));
    tags_save();
}
/* bolinhas das etiquetas (sidebar, ícones e lista) */
static void tag_dot(surface_t *sf, float cx, float cy, float r, int t) {
    gfx_circle(sf, cx, cy, r + 1, T.dark ? RGB(40, 40, 44) : RGB(255, 255, 255));
    gfx_circle(sf, cx, cy, r, tag_cols[t]);
}
static void tag_dots(surface_t *sf, int x, int cy, u8 bits) {
    for (int t = 1; t <= 7; t++) if (bits & (1 << (t - 1))) { tag_dot(sf, x, cy, 4, t); x += 6; }
}
#define TAG_GLYPH(k) static void sb_tag##k(surface_t *s, int x, int y, int sz, u32 c) { (void)c; tag_dot(s, x + sz / 2.0f, y + sz / 2.0f, sz * 0.32f, k); }
TAG_GLYPH(1) TAG_GLYPH(2) TAG_GLYPH(3) TAG_GLYPH(4) TAG_GLYPH(5) TAG_GLYPH(6) TAG_GLYPH(7)
static void (*const sb_tags[8])(surface_t *, int, int, int, u32) = {NULL, sb_tag1, sb_tag2, sb_tag3, sb_tag4, sb_tag5, sb_tag6, sb_tag7};

/* lista de itens (inclui os apps virtuais em /Aplicativos) */
typedef struct { vnode_t *node; app_t *app; } item_t;

static const char *node_kind(vnode_t *n);
static int item_cmp(fstate_t *s, item_t *a, item_t *b) {
    if (!a->node || !b->node) return 0;
    int key = s->groups ? 1 : s->sort;
    if (key == 1) { int c = strcmp(node_kind(a->node), node_kind(b->node)); if (c) return c; }
    if (key == 2 && a->node->mtime != b->node->mtime) return a->node->mtime > b->node->mtime ? -1 : 1;     /* recentes primeiro */
    if (key == 3 && a->node->size != b->node->size) return a->node->size > b->node->size ? -1 : 1;        /* maiores primeiro */
    return strcasecmp(a->node->name, b->node->name);
}

static int list_items(fstate_t *s, item_t *out, int max) {
    int n = 0;
    if (s->tag) {
        /* etiqueta escolhida na barra lateral: todos os itens marcados com ela */
        tags_load();
        for (int i = 0; i < ntags && n < max; i++)
            if (tagtab[i].bits & (1 << (s->tag - 1))) { vnode_t *v = vfs_lookup(tagtab[i].path); if (v) out[n++] = (item_t){v, NULL}; }
        return n;
    }
    vnode_t *d = vfs_lookup(s->path);
    if (!d) return 0;
    if (!strcmp(s->path, "/Aplicativos")) {
        for (int i = 0; i < apps_count() && n < max; i++) {
            app_t *a = apps_get(i);
            if (s->search[0] && !strstr(a->name, s->search)) continue;
            out[n++] = (item_t){NULL, a};
        }
    }
    for (vnode_t *c = d->child; c && n < max; c = c->next) {
        if (c->name[0] == '.') continue;
        if (s->search[0]) {
            char a[256], b[256];
            int i;
            for (i = 0; c->name[i]; i++) a[i] = (char)tolower(c->name[i]);
            a[i] = 0;
            for (i = 0; s->search[i]; i++) b[i] = (char)tolower(s->search[i]);
            b[i] = 0;
            if (!strstr(a, b)) continue;
        }
        out[n++] = (item_t){c, NULL};
    }
    /* ordenação estável (inserção; no máximo 256 itens) */
    if (s->sort || s->groups)
        for (int a = 1; a < n; a++)
            for (int b = a; b > 0 && item_cmp(s, &out[b - 1], &out[b]) > 0; b--) { item_t t = out[b]; out[b] = out[b - 1]; out[b - 1] = t; }
    return n;
}

/* pasta .app com Contents/: é um app do macOS (abre, não entra; ícone e tipo de app) */
static bool is_bundle(vnode_t *n) {
    if (!n || !n->dir) return false;
    usize nl = strlen(n->name);
    return nl > 4 && !strcasecmp(n->name + nl - 4, ".app") && vfs_child(n, "Contents");
}
static bool is_folder(vnode_t *n) { return n && n->dir && !is_bundle(n); }

/* nome mostrado: apps sem a extensão .app, como no macOS */
static const char *shown_name(item_t *it) {
    if (it->app) return it->app->name;
    if (!is_bundle(it->node)) return it->node->name;
    static char rot[4][256];
    static int k;
    char *b = rot[k++ & 3];
    usize nl = strlen(it->node->name);
    memcpy(b, it->node->name, nl - 4);
    b[nl - 4] = 0;
    return b;
}

/* "Tipo": a descrição do Launch Services (o nome que o app que abre dá ao tipo, ou a do sistema) */
static const char *node_kind(vnode_t *n) {
    if (is_bundle(n)) return "Aplicativo";
    if (!n) return "";
    char p[512];
    vfs_path(n, p, sizeof p);
    return ls_kind_for_path(p);
}

/* ícone genérico de app do macOS (quando o bundle não tem um .icns legível) */
static void icon_mac_generic(surface_t *s, int x, int y, int sz) {
    float r = sz * 0.225f, m = sz * 0.08f;
    gfx_round_rect_grad(s, x + m, y + m, sz - 2 * m, sz - 2 * m, r, RGB(120, 128, 150), RGB(62, 66, 84));
    text_draw_center(s, font_get(FONT_SEMIBOLD), sz * 0.42f, rect_make(x, y, sz, sz), RGB(255, 255, 255), "⌘");
}

static void open_item(fstate_t *s, item_t *it) {
    if (it->app) { app_launch(it->app); return; }
    char p[256];
    vfs_path(it->node, p, sizeof p);
    if (is_folder(it->node)) go(s, p, true);
    else open_path(p);
}

static void draw_item_icon(surface_t *sf, item_t *it, int x, int y, int size) {
    if (it->app) { gfx_blend(sf, icon_for(it->app, size), x, y, 255); return; }
    vnode_t *n = it->node;
    if (is_bundle(n)) {
        char p[300];
        vfs_path(n, p, sizeof p);
        surface_t *ic = macho_bundle_icon(p, size);
        if (ic) gfx_blend(sf, ic, x, y, 255);
        else icon_mac_generic(sf, x, y, size);
    } else if (n->dir) icon_folder(sf, x, y, size, RGB(104, 182, 240));
    else {
        char dp[512];
        vfs_path(n, dp, sizeof dp);
        surface_t *di = ls_doc_icon(dp, size);   /* ícone do documento no app que o abre */
        if (di) { gfx_blend(sf, di, x, y, 255); return; }
        const char *e = vfs_ext(n->name);
        char up[8];
        int i = 0;
        for (; e[i] && i < 4; i++) up[i] = (char)toupper(e[i]);
        up[i] = 0;
        icon_document(sf, x, y, size, up[0] ? up : NULL);
    }
}

static void fmt_size(usize b, char *o, int n) {
    if (b < 1000) snprintf(o, n, "%llu bytes", (u64)b);
    else if (b < 1000000) snprintf(o, n, "%llu KB", (u64)(b + 500) / 1000);
    else snprintf(o, n, "%.1f MB", b / 1e6);
}

static void sidebar_glyph_folder(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_round_rect_stroke(s, x + 1, y + 4, sz - 2, sz - 6, 2, 1.4f, c);
    gfx_line(s, x + 1.5f, y + 4, x + sz * 0.45f, y + 4, 2.4f, c);
}
static void sidebar_glyph_app(surface_t *s, int x, int y, int sz, u32 c) {
    float cx = x + sz / 2.0f;
    gfx_line(s, cx - 5, y + sz - 2, cx, y + 2, 1.5f, c);
    gfx_line(s, cx + 5, y + sz - 2, cx, y + 2, 1.5f, c);
    gfx_line(s, cx - 3, y + sz * 0.62f, cx + 3, y + sz * 0.62f, 1.5f, c);
}
static void sidebar_glyph_disk(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_round_rect_stroke(s, x + 1, y + 4, sz - 2, sz - 8, 2, 1.4f, c);
    gfx_circle(s, x + sz - 5, y + sz - 7, 1.2f, c);
}
static void sidebar_glyph_trash(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_line(s, x + 2, y + 4, x + sz - 2, y + 4, 1.4f, c);
    gfx_round_rect_stroke(s, x + 4, y + 5, sz - 8, sz - 6, 1.5f, 1.4f, c);
}
static void sidebar_glyph_dl(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_circle_stroke(s, x + sz / 2.0f, y + sz / 2.0f, sz / 2.0f - 1, 1.4f, c);
    gfx_line(s, x + sz / 2.0f, y + 4, x + sz / 2.0f, y + sz - 5, 1.4f, c);
    gfx_line(s, x + sz / 2.0f - 3, y + sz - 8, x + sz / 2.0f, y + sz - 5, 1.4f, c);
    gfx_line(s, x + sz / 2.0f + 3, y + sz - 8, x + sz / 2.0f, y + sz - 5, 1.4f, c);
}

/* concatena com limite (o kernel não tem strncat) */
static void path_append(char *dst, usize cap, const char *src) {
    usize l = strlen(dst);
    while (*src && l + 1 < cap) dst[l++] = *src++;
    dst[l] = 0;
}

/* ---------------- visualização em colunas: uma por nível do caminho; a última lista a pasta atual */
static int paint_columns(ui_t *u, fstate_t *s, item_t *items, int n, rect_t view, bool *hit_any, bool *ctx_item) {
    surface_t *sf = u->s;
    char comps[24][128];
    int nc = 0;
    for (const char *p = s->path; *p && nc < 24;) {
        while (*p == '/') p++;
        if (!*p) break;
        int k = 0;
        while (*p && *p != '/' && k < 127) comps[nc][k++] = *p++;
        comps[nc][k] = 0;
        nc++;
        while (*p && *p != '/') p++;
    }
    int colw = s->colsw, ncols = nc + 1;
    int maxcols = MAX(1, view.w / colw), first = MAX(0, ncols - maxcols);
    font_t *f = font_get(FONT_REGULAR);
    /* arrastar uma divisória muda a largura das colunas (alça "||" no pé, como no sistema) */
    if (s->col_drag == 10) {
        if (u->down) s->colsw = CLAMP(s->col_w0 + (u->mx - s->col_x0), 140, 480);
        else s->col_drag = -1;
        u->cursor = CURSOR_RESIZE_COL;
        u->repaint = true;
    }
    for (int c = first; c < ncols; c++) {
        int x = view.x + (c - first) * colw;
        gfx_fill(sf, x + colw - 1, view.y, 1, view.h, T.separator);
        rect_t dr = rect_make(x + colw - 5, view.y, 9, view.h);
        gfx_fill(sf, x + colw - 4, view.y + view.h - 16, 1, 10, T.text3);
        gfx_fill(sf, x + colw + 1, view.y + view.h - 16, 1, 10, T.text3);
        if (ui_hit(u, dr)) u->cursor = CURSOR_RESIZE_COL;
        if (ui_hit(u, dr) && u->pressed) { s->col_drag = 10; s->col_x0 = u->mx; s->col_w0 = s->colsw; *hit_any = true; return view.h; }
        char dir[512] = "/";
        for (int k = 0; k < c; k++) { if (k) strcat(dir, "/"); path_append(dir, sizeof dir, comps[k]); }
        int y = view.y + 8;
        if (c == ncols - 1) {
            /* coluna da pasta atual: mesmos itens (e seleção) das outras visualizações */
            for (int i = 0; i < n && y < view.y + view.h; i++, y += 24) {
                rect_t r = rect_make(x + 6, y, colw - 12, 22);
                bool sel = is_sel(s, i);
                if (sel) gfx_round_rect(sf, r.x, r.y, r.w, r.h, 6, T.accent);
                draw_item_icon(sf, &items[i], r.x + 6, r.y + 3, 16);
                const char *name = shown_name(&items[i]);
                text_draw_ellipsis(sf, f, 13, r.x + 28, r.y + 16, r.w - 50, sel ? RGB(255, 255, 255) : T.text, name);
                bool isdir = is_folder(items[i].node);
                if (isdir) glyph_chevron(sf, r.x + r.w - 16, r.y + 5, 12, sel ? RGB(255, 255, 255) : T.text3, 0);
                if (ui_hit(u, r) && u->pressed) {
                    s->sel = i; *hit_any = true;
                    if (isdir) { char p[512]; vfs_path(items[i].node, p, sizeof p); go(s, p, true); return view.h; }
                    if (u->dblclick) open_item(s, &items[i]);
                }
                if (ui_hit(u, r) && u->rpressed) { s->sel = i; *hit_any = true; *ctx_item = true; }
            }
        } else {
            /* níveis acima: a pasta escolhida fica destacada em cinza */
            vnode_t *d = vfs_lookup(dir);
            for (vnode_t *ch = d ? d->child : NULL; ch && y < view.y + view.h; ch = ch->next, y += 24) {
                if (ch->name[0] == '.') { y -= 24; continue; }
                rect_t r = rect_make(x + 6, y, colw - 12, 22);
                bool on = !strcmp(ch->name, comps[c]);
                if (on) gfx_round_rect(sf, r.x, r.y, r.w, r.h, 6, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 22));
                item_t it = {ch, NULL};
                draw_item_icon(sf, &it, r.x + 6, r.y + 3, 16);
                text_draw_ellipsis(sf, f, 13, r.x + 28, r.y + 16, r.w - 50, T.text, shown_name(&it));
                if (is_folder(ch)) glyph_chevron(sf, r.x + r.w - 16, r.y + 5, 12, T.text3, 0);
                if (ui_hit(u, r) && u->pressed) {
                    *hit_any = true;
                    char p[512];
                    vfs_path(ch, p, sizeof p);
                    if (is_folder(ch)) { go(s, p, true); return view.h; }
                }
            }
        }
    }
    return view.h;
}

/* ---------------- barra de caminho clicável (rodapé) */
static void paint_path_bar(ui_t *u, fstate_t *s, rect_t r) {
    surface_t *sf = u->s;
    gfx_fill(sf, r.x, r.y, r.w, 1, T.separator);
    font_t *f = font_get(FONT_REGULAR);
    float fs = 11.5f;
    int x = r.x + 12, bl = r.y + 16;
    char acc[512] = "/";
    const char *p = s->path;
    int k = 0;
    /* raiz */
    {
        int tw = text_width(f, fs, "mOS HD");
        glyph_sb_disk(sf, x, r.y + 5, 14, T.text2);
        rect_t hr = rect_make(x, r.y, tw + 20, r.h);
        if (ui_clicked(u, 800, hr)) go(s, "/", true);
        text_draw(sf, f, fs, x + 18, bl, ui_hit(u, hr) ? T.text : T.text2, "mOS HD");
        x += tw + 22;
    }
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        char name[128];
        int j = 0;
        while (*p && *p != '/' && j < 127) name[j++] = *p++;
        name[j] = 0;
        if (strcmp(acc, "/")) strcat(acc, "/");
        path_append(acc, sizeof acc, name);
        glyph_chevron(sf, x, r.y + 7, 10, T.text3, 0);
        x += 14;
        int tw = text_width(f, fs, name);
        if (x + tw > r.x + r.w - 160) break;
        rect_t hr = rect_make(x - 2, r.y, tw + 4, r.h);
        char target[512];
        strcpy(target, acc);
        if (ui_clicked(u, 801 + k, hr)) { go(s, target, true); return; }
        text_draw(sf, f, fs, x, bl, ui_hit(u, hr) ? T.text : T.text2, name);
        x += tw + 6;
        k++;
    }
}

/* ---------------- Visualização Rápida (Espaço): imagem, texto ou ícone grande */
static bool is_ext(const char *name, const char *const *exts) {
    const char *e = vfs_ext(name);
    for (int i = 0; exts[i]; i++) if (!strcasecmp(e, exts[i])) return true;
    return false;
}

static void paint_quicklook(ui_t *u, fstate_t *s, item_t *it) {
    int w = MIN(u->cw - 80, 560), h = MIN(u->ch - 100, 420);
    rect_t p = ui_sheet(u, w, h, 60);
    surface_t *sf = u->s;
    const char *name = shown_name(it);
    text_draw_center(sf, font_get(FONT_SEMIBOLD), 13, rect_make(p.x, p.y + 6, p.w, 24), T.text, name);
    gfx_fill(sf, p.x + 12, p.y + 34, p.w - 24, 1, T.separator);
    rect_t body = rect_make(p.x + 16, p.y + 44, p.w - 32, p.h - 60);
    static const char *const img_ext[] = {"png", "jpg", "jpeg", "bmp", "ppm", "gif", NULL};
    static const char *const txt_ext[] = {"txt", "md", "html", "htm", "c", "h", "s", "json", "css", "js", "sh", "csv", "log", NULL};
    if (it->node && !it->node->dir && is_ext(it->node->name, img_ext)) {
        char path[256];
        vfs_path(it->node, path, sizeof path);
        if (strcmp(path, s->ql_path)) {
            if (s->ql_img) surface_free(s->ql_img);
            s->ql_img = it->node->data ? image_decode(it->node->data, it->node->size) : NULL;
            strncpy(s->ql_path, path, sizeof s->ql_path - 1);
        }
        if (s->ql_img) {
            float k = MIN((float)body.w / s->ql_img->w, (float)body.h / s->ql_img->h);
            if (k > 1) k = 1;
            int iw = (int)(s->ql_img->w * k), ih = (int)(s->ql_img->h * k);
            gfx_blit_scaled(sf, s->ql_img, body.x + (body.w - iw) / 2, body.y + (body.h - ih) / 2, iw, ih, 255);
        } else {
            ui_label_center(u, body, "Não foi possível ler a imagem", FONT_REGULAR, 13, T.text2);
        }
    } else if (it->node && !it->node->dir && is_ext(it->node->name, txt_ext) && it->node->data) {
        char buf[2048];
        usize nb = MIN(it->node->size, sizeof buf - 1);
        memcpy(buf, it->node->data, nb);
        buf[nb] = 0;
        rect_t pc = surface_push_clip(sf, body);
        text_wrap(sf, font_get(FONT_REGULAR), 12.5f, body, 17, T.text, buf, true);
        surface_pop_clip(sf, pc);
    } else {
        draw_item_icon(sf, it, p.x + (p.w - 128) / 2, body.y + 20, 128);
        char info[96], sz[32] = "";
        if (it->node && !it->node->dir) fmt_size(it->node->size, sz, sizeof sz);
        snprintf(info, sizeof info, "%s%s%s", it->app ? "Aplicativo" : node_kind(it->node), sz[0] ? " — " : "", sz);
        ui_label_center(u, rect_make(p.x, body.y + 160, p.w, 20), info, FONT_REGULAR, 13, T.text2);
    }
    if ((u->pressed && !rect_contains(p, u->mx, u->my)) || u->key == KEY_ESC || u->key == ' ') { s->ql = false; u->key_used = true; }
    ui_sheet_end(u);
}

/* ---------------- seleção múltipla */
static bool is_sel(fstate_t *s, int i) { return s->sel == i || (i >= 0 && i < 256 && s->msel[i]); }

static int collect_selected(fstate_t *s, item_t *items, int n, vnode_t **out, int max) {
    int k = 0;
    for (int i = 0; i < n && k < max; i++) if (is_sel(s, i) && items[i].node) out[k++] = items[i].node;
    return k;
}

/* clique num item: simples seleciona só ele; ⌘ alterna; ⇧ seleciona o intervalo desde a âncora */
static void sel_click(ui_t *u, fstate_t *s, int i, item_t *items) {
    if (u->mods & MOD_CMD) {
        if (s->sel >= 0 && s->sel < 256) s->msel[s->sel] = true;
        bool on = !s->msel[i];
        s->msel[i] = on;
        s->sel = on ? i : -1;
        s->anchor = i;
    } else if ((u->mods & MOD_SHIFT) && s->anchor >= 0) {
        memset(s->msel, 0, sizeof s->msel);
        int a = MIN(s->anchor, i), b = MAX(s->anchor, i);
        for (int k = a; k <= b && k < 256; k++) s->msel[k] = true;
        s->sel = i;
    } else {
        if (!is_sel(s, i)) memset(s->msel, 0, sizeof s->msel); /* clicar num item já selecionado mantém o grupo (para arrastar) */
        s->sel = i;
        s->anchor = i;
        if (u->dblclick) open_item(s, &items[i]);
    }
    s->press_item = i;
    s->press_x = u->mx;
    s->press_y = u->my;
}

static void add_drop(fstate_t *s, rect_t r, const char *path) {
    if (s->ndrops >= (int)ARRAY_LEN(s->drops)) return;
    s->drops[s->ndrops].r = r;
    strncpy(s->drops[s->ndrops].path, path, sizeof s->drops[0].path - 1);
    s->drops[s->ndrops].path[sizeof s->drops[0].path - 1] = 0;
    s->ndrops++;
}

static void move_to_trash(fstate_t *s, item_t *it);
/* move todos os itens selecionados para o Lixo */
static void trash_selected(fstate_t *s, item_t *items, int n) {
    vnode_t *nodes[256];
    int k = collect_selected(s, items, n, nodes, 256);
    for (int i = 0; i < k; i++) { item_t it = {nodes[i], NULL}; move_to_trash(s, &it); }
    memset(s->msel, 0, sizeof s->msel);
    s->sel = -1;
}

/* ---------------- arrastar e soltar: itens selecionados para uma pasta (da área ou da barra lateral) ou o Lixo */
static void drag_and_drop(ui_t *u, fstate_t *s, item_t *items, int n) {
    if (s->press_item < 0 || s->press_item >= n) { s->press_item = -1; s->dragging = false; return; }
    if (u->down && !s->dragging && (ABS_DIFF(u->mx, s->press_x) > 4 || ABS_DIFF(u->my, s->press_y) > 4)) s->dragging = true;
    if (!s->dragging) { if (!u->down) s->press_item = -1; return; }
    int hit = -1;
    for (int k = 0; k < s->ndrops; k++) if (rect_contains(s->drops[k].r, u->mx, u->my)) hit = k;
    surface_t *sf = u->s;
    if (hit >= 0) gfx_round_rect_stroke(sf, s->drops[hit].r.x, s->drops[hit].r.y, s->drops[hit].r.w, s->drops[hit].r.h, 7, 2, T.accent);
    /* "fantasma": ícone do item pego + contador quando há vários */
    vnode_t *nodes[256];
    int cnt = collect_selected(s, items, n, nodes, 256);
    draw_item_icon(sf, &items[s->press_item], u->mx - 22, u->my - 22, 44);
    if (cnt > 1) {
        char num[8];
        snprintf(num, sizeof num, "%d", cnt);
        gfx_circle(sf, u->mx + 20, u->my - 20, 10, RGB(255, 59, 48));
        text_draw_center(sf, font_get(FONT_BOLD), 11, rect_make(u->mx + 10, u->my - 31, 20, 20), RGB(255, 255, 255), num);
    }
    u->cursor = CURSOR_COPY;
    if (!u->down) {
        if (hit >= 0) {
            if (!strcmp(s->drops[hit].path, CUR_TRASH)) {
                trash_selected(s, items, n);
            } else {
                vnode_t *dst = vfs_lookup(s->drops[hit].path);
                int moved = 0;
                if (dst && dst->dir && !vfs_is_read_only(dst)) {
                    for (int i = 0; i < cnt; i++) {
                        if (nodes[i] == dst || vfs_is_read_only(nodes[i])) continue;
                        vfs_move(nodes[i], dst, NULL);
                        moved++;
                    }
                }
                if (moved) {
                    char msg[300];
                    snprintf(msg, sizeof msg, "%d %s para %s", moved, moved == 1 ? "item movido" : "itens movidos", title_for(s->drops[hit].path));
                    notify("Arquivos", msg);
                } else {
                    notify("Não foi possível mover", "O destino ou os itens são somente leitura.");
                }
                memset(s->msel, 0, sizeof s->msel);
                s->sel = -1;
            }
        }
        s->dragging = false;
        s->press_item = -1;
        u->clicked_any = true;
    }
}

/* ---------------- menus de contexto: abre e trata o resultado */
static void move_to_trash(fstate_t *s, item_t *it) {
    if (!it->node) return;
    if (vfs_is_read_only(it->node)) { notify("O volume é somente leitura", "Não é possível apagar itens de uma imagem de disco."); return; }
    vnode_t *t = vfs_mkdir(CUR_TRASH);
    if (!strcmp(s->path, CUR_TRASH)) vfs_delete(it->node); else vfs_move(it->node, t, NULL);
    s->sel = -1;
}

/* ---------------- Obter Informações (⌘I): janela pequena com geral, etiquetas e permissões */
typedef struct { char path[256]; app_t *app; } info_t;

/* "Abrir com:" do Obter Informações: o app deste documento e "Alterar Tudo…" para todos do mesmo tipo */
static char ci_path[256];
static ls_app_t *ci_app;
static void change_all_answer(int button, void *ctx) {
    (void)ctx;
    if (button == 0 && ci_app) ls_set_default_for_path_type(ci_path, ci_app);
}
void finder_choose_app(const char *path);
static int info_open_with(ui_t *u, info_t *in, vnode_t *n, int y) {
    surface_t *sf = u->s;
    gfx_fill(sf, 12, y, u->cw - 24, 1, T.separator);
    y += 10;
    text_draw(sf, font_get(FONT_SEMIBOLD), 12.5f, 16, y + 12, T.text, "Abrir com:");
    y += 22;
    static ls_app_t *list[20];
    static char labels[22][160];
    static const char *items[22];
    int k = ls_handlers_for_path(in->path, LS_ROLE_ALL, list, 20);
    ls_app_t *cur = k ? list[0] : NULL;
    for (int i = 0; i < k; i++) {
        snprintf(labels[i], sizeof labels[i], i == 0 ? "%s (padrão)" : "%s", ls_app_name(list[i]));
        items[i] = labels[i];
    }
    items[k] = "Outro…";
    rect_t pr = rect_make(46, y, u->cw - 62, 26);
    if (cur) ls_app_draw_icon(cur, sf, 18, y + 2, 22);
    int sel = ui_popup(u, 0x7C001, pr, items, k + 1, cur ? 0 : k);
    if (sel == k && cur) finder_choose_app(in->path);
    else if (sel > 0 && sel < k) ls_set_handler_for_file(in->path, list[sel]);   /* só este documento */
    y += 34;
    text_draw(sf, font_get(FONT_REGULAR), 11.5f, 18, y + 10, T.text2, "Use este app para todos os documentos assim.");
    y += 22;
    if (ui_button_style(u, 0x7C002, rect_make(u->cw - 132, y, 116, 26), "Alterar Tudo…", BTN_GRAY, cur != NULL, NULL) && cur) {
        snprintf(ci_path, sizeof ci_path, "%s", in->path);
        ci_app = cur;
        static char msg[400];
        snprintf(msg, sizeof msg, "Tem certeza de que deseja alterar todos os documentos semelhantes para que sejam abertos com o app “%s”?", ls_app_name(cur));
        static const char *btn[] = {"Continuar", "Cancelar"};
        alert_show(msg, "Esta alteração será aplicada a todos os documentos com o mesmo tipo.", btn, 2, change_all_answer, NULL);
    }
    (void)n;
    return y + 36;
}
static void info_paint(window_t *w, ui_t *u) {
    info_t *in = w->data;
    surface_t *sf = u->s;
    vnode_t *n = in->app ? NULL : vfs_lookup(in->path);
    if (!in->app && !n) { ui_label_center(u, rect_make(0, 0, u->cw, u->ch), "O item não existe mais", FONT_REGULAR, 13, T.text2); return; }
    item_t it = {n, in->app};
    draw_item_icon(sf, &it, 16, 14, 56);
    const char *name = shown_name(&it);
    text_draw_ellipsis(sf, font_get(FONT_BOLD), 15, 84, 38, u->cw - 100, T.text, name);
    char sz[96] = "--";
    bundle_info_t bi;
    if (is_bundle(n) && macho_bundle_info(in->path, &bi) && bi.version[0]) snprintf(sz, sizeof sz, "Versão %s", bi.version);
    else if (n && !n->dir) fmt_size(n->size, sz, sizeof sz);
    else if (n) snprintf(sz, sizeof sz, "%d %s", vfs_count(n), vfs_count(n) == 1 ? "item" : "itens");
    text_draw(sf, font_get(FONT_REGULAR), 12, 84, 58, T.text2, sz);
    gfx_fill(sf, 12, 84, u->cw - 24, 1, T.separator);
    /* Geral */
    int y = 96;
    text_draw(sf, font_get(FONT_SEMIBOLD), 12.5f, 16, y + 12, T.text, "Geral");
    y += 22;
    char where[256] = "/Aplicativos", when[40] = "--";
    if (n) {
        vfs_path(n, where, sizeof where);
        char *sl = strrchr(where, '/');
        if (sl && sl != where) *sl = 0; else strcpy(where, "/");
        datetime_t dt;
        rtc_now(&dt);
        if (n->mtime) snprintf(when, sizeof when, "Hoje, %02d:%02d", dt.hour, dt.minute); else strcpy(when, "No disco");
    }
    struct { const char *k, *v; } rows[] = {{"Tipo:", in->app ? "Aplicativo" : node_kind(n)}, {"Tamanho:", sz}, {"Onde:", where}, {"Modificado:", when}};
    for (u32 i = 0; i < ARRAY_LEN(rows); i++, y += 20) {
        int kw = text_width(font_get(FONT_SEMIBOLD), 12, rows[i].k);
        text_draw(sf, font_get(FONT_SEMIBOLD), 12, 104 - kw, y + 13, T.text2, rows[i].k);
        text_draw_ellipsis(sf, font_get(FONT_REGULAR), 12, 110, y + 13, u->cw - 124, T.text, rows[i].v);
    }
    if (n && !n->dir) y = info_open_with(u, in, n, y + 10) - 10;
    /* Etiquetas: clicar alterna */
    y += 10;
    gfx_fill(sf, 12, y, u->cw - 24, 1, T.separator);
    y += 10;
    text_draw(sf, font_get(FONT_SEMIBOLD), 12.5f, 16, y + 12, T.text, "Etiquetas");
    y += 24;
    u8 bits = tag_bits(n);
    for (int t = 1; t <= 7 && n; t++) {
        rect_t r = rect_make(18 + (t - 1) * 34, y, 26, 26);
        tag_dot(sf, r.x + 13, r.y + 13, 9, t);
        if (bits & (1 << (t - 1))) { gfx_line(sf, r.x + 8, r.y + 13, r.x + 12, r.y + 17, 1.8f, RGB(255, 255, 255)); gfx_line(sf, r.x + 12, r.y + 17, r.x + 19, r.y + 9, 1.8f, RGB(255, 255, 255)); }
        if (ui_clicked(u, 9100 + t, r)) tag_toggle(n, t);
    }
    y += 40;
    gfx_fill(sf, 12, y, u->cw - 24, 1, T.separator);
    y += 10;
    text_draw(sf, font_get(FONT_SEMIBOLD), 12.5f, 16, y + 12, T.text, "Compartilhamento e Permissões");
    y += 24;
    bool ro = n && vfs_is_read_only(n);
    text_draw(sf, font_get(FONT_REGULAR), 12, 18, y + 12, T.text2, in->app || ro ? "Você só pode ler." : "Você pode ler e gravar.");
}
static void info_close(window_t *w) { kfree(w->data); }
static void open_info(item_t *it) {
    if (!it) return;
    info_t *in = kcalloc(1, sizeof(info_t));
    if (!in) return;
    in->app = it->app;
    if (it->node) vfs_path(it->node, in->path, sizeof in->path);
    char t[160];
    snprintf(t, sizeof t, "Informações de %s", shown_name(it));
    window_t *w = wm_create(&app_arquivos, t, -1, -1, 300, it->node && !it->node->dir ? 500 : 380, 0);
    if (!w) { kfree(in); return; }
    w->data = in;
    w->paint = info_paint;
    w->on_close = info_close;
}

/* ---------------- renomear no lugar: Return no item selecionado; Return confirma, Esc cancela */
static void rename_begin(ui_t *u, fstate_t *s, item_t *items, int n) {
    if (s->view > 1) s->view = 0;   /* o campo de renomear existe nas visualizações em ícones e lista */
    if (s->sel < 0 || s->sel >= n || !items[s->sel].node || vfs_is_read_only(items[s->sel].node) || !strcmp(s->path, "/Aplicativos")) return;
    s->renaming = s->sel;
    strncpy(s->rename_buf, items[s->sel].node->name, sizeof s->rename_buf - 1);
    s->rename_buf[sizeof s->rename_buf - 1] = 0;
    s->rename_started = false;
    u->focus = 0x7A100;
    /* seleciona o nome sem a extensão, como no sistema */
    const char *dot = strrchr(s->rename_buf, '.');
    u->sel_anchor = 0;
    u->caret = dot && dot != s->rename_buf && !items[s->sel].node->dir ? (int)(dot - s->rename_buf) : (int)strlen(s->rename_buf);
}
static void rename_commit(fstate_t *s, item_t *items, int n) {
    int i = s->renaming;
    s->renaming = -1;
    if (i < 0 || i >= n || !items[i].node) return;
    vnode_t *v = items[i].node;
    if (!s->rename_buf[0] || strchr(s->rename_buf, '/') || !strcmp(s->rename_buf, v->name)) return;
    if (vfs_child(v->parent, s->rename_buf)) { notify("Esse nome já está em uso", s->rename_buf); return; }
    char oldp[256], newp[256];
    vfs_path(v, oldp, sizeof oldp);
    u8 bits = tag_bits_path(oldp);
    if (vfs_move(v, v->parent, s->rename_buf) && bits) {
        /* a etiqueta acompanha o novo nome */
        vfs_path(v, newp, sizeof newp);
        for (int k = 0; k < ntags; k++) if (!strcmp(tagtab[k].path, oldp)) { strncpy(tagtab[k].path, newp, sizeof tagtab[k].path - 1); tags_save(); break; }
    }
}
/* campo de renomear sobre o rótulo do item; devolve true enquanto estiver aberto */
static void rename_field(ui_t *u, fstate_t *s, item_t *items, int n, rect_t r) {
    bool enter = ui_textfield(u, 0x7A100, r, s->rename_buf, sizeof s->rename_buf, NULL);
    if (u->focus == 0x7A100) s->rename_started = true;
    if (enter) { rename_commit(s, items, n); u->focus = 0; u->key_used = true; }
    else if (u->key == KEY_ESC) { s->renaming = -1; u->focus = 0; u->key_used = true; }
    else if (s->rename_started && u->focus != 0x7A100) rename_commit(s, items, n);   /* clicou fora: confirma */
}

/* imagem para Visualização Rápida e Galeria (decodificada uma vez por arquivo) */
static const char *const img_exts[] = {"png", "jpg", "jpeg", "bmp", "ppm", "gif", NULL};
static surface_t *preview_image(fstate_t *s, vnode_t *n) {
    if (!n || n->dir || !n->data) return NULL;
    const char *e = vfs_ext(n->name);
    bool img = false;
    for (int i = 0; img_exts[i]; i++) if (!strcasecmp(e, img_exts[i])) img = true;
    if (!img) return NULL;
    char path[256];
    vfs_path(n, path, sizeof path);
    if (strcmp(path, s->ql_path)) {
        if (s->ql_img) surface_free(s->ql_img);
        s->ql_img = image_decode(n->data, n->size);
        strncpy(s->ql_path, path, sizeof s->ql_path - 1);
    }
    return s->ql_img;
}

/* ---------------- "Abrir com": apps que o Launch Services diz que abrem o item, o padrão primeiro */
#define OW_MAX 20
static ls_app_t *ow_apps[OW_MAX];
static int ow_n;
static char ow_path[512];
static char ow_labels[OW_MAX][160];
#define OW_ICON(i) static void ow_icon##i(surface_t *s, int x, int y, int sz, u32 c) { (void)c; ls_app_draw_icon(ow_apps[i], s, x - 1, y - 1, sz + 2); }
OW_ICON(0) OW_ICON(1) OW_ICON(2) OW_ICON(3) OW_ICON(4) OW_ICON(5) OW_ICON(6) OW_ICON(7) OW_ICON(8) OW_ICON(9)
OW_ICON(10) OW_ICON(11) OW_ICON(12) OW_ICON(13) OW_ICON(14) OW_ICON(15) OW_ICON(16) OW_ICON(17) OW_ICON(18) OW_ICON(19)
static void (*const ow_icons[OW_MAX])(surface_t *, int, int, int, u32) = {
    ow_icon0, ow_icon1, ow_icon2, ow_icon3, ow_icon4, ow_icon5, ow_icon6, ow_icon7, ow_icon8, ow_icon9,
    ow_icon10, ow_icon11, ow_icon12, ow_icon13, ow_icon14, ow_icon15, ow_icon16, ow_icon17, ow_icon18, ow_icon19};
enum { OW_ACT = 1000, OW_OTHER = 1999 };

/* monta o submenu (padrão, separador, os outros em ordem alfabética, separador, Outro…) */
static int ow_build(const char *path, menu_item_t *out, int max) {
    snprintf(ow_path, sizeof ow_path, "%s", path);
    ow_n = ls_handlers_for_path(path, LS_ROLE_ALL, ow_apps, OW_MAX);
    for (int i = 2; i < ow_n; i++)         /* o padrão fica no topo; o resto por nome */
        for (int j = i; j > 1 && strcasecmp(ls_app_name(ow_apps[j]), ls_app_name(ow_apps[j - 1])) < 0; j--) { ls_app_t *t = ow_apps[j]; ow_apps[j] = ow_apps[j - 1]; ow_apps[j - 1] = t; }
    int k = 0;
    for (int i = 0; i < ow_n && k < max - 3; i++) {
        if (i == 0) snprintf(ow_labels[i], sizeof ow_labels[i], "%s (padrão)", ls_app_name(ow_apps[i]));
        else snprintf(ow_labels[i], sizeof ow_labels[i], "%s", ls_app_name(ow_apps[i]));
        out[k] = (menu_item_t){ow_labels[i], NULL, OW_ACT + i};
        out[k].icon = ow_icons[i];
        k++;
        if (i == 0 && ow_n > 1) { out[k] = (menu_item_t){NULL}; out[k].disabled = true; k++; }
    }
    if (k) { out[k] = (menu_item_t){NULL}; out[k].disabled = true; k++; }
    out[k++] = (menu_item_t){"Outro…", NULL, OW_OTHER};
    return k;
}

/* ---------------- Escolher Aplicativo (Outro… / "Não há um aplicativo definido") */
typedef struct { char path[512]; int sel, scroll, all; int always; ls_app_t *list[96]; int n; } choose_t;
static void choose_fill(choose_t *c) {
    c->n = 0;
    if (!c->all) c->n = ls_handlers_for_path(c->path, LS_ROLE_ALL, c->list, 96);
    if (c->all || !c->n) {
        c->n = 0;
        for (int i = 0; i < ls_app_count() && c->n < 96; i++) {
            ls_app_t *a = ls_app_at(i);
            if (!strcmp(ls_app_id(a), "com.apple.finder")) continue;
            c->list[c->n++] = a;
        }
        for (int i = 1; i < c->n; i++)
            for (int j = i; j > 0 && strcasecmp(ls_app_name(c->list[j]), ls_app_name(c->list[j - 1])) < 0; j--) { ls_app_t *t = c->list[j]; c->list[j] = c->list[j - 1]; c->list[j - 1] = t; }
    }
    if (c->sel >= c->n) c->sel = c->n ? 0 : -1;
}
static void choose_paint(window_t *w, ui_t *u) {
    choose_t *c = w->data;
    surface_t *sf = u->s;
    const char *nm = strrchr(c->path, '/') ? strrchr(c->path, '/') + 1 : c->path;
    char msg[400];
    snprintf(msg, sizeof msg, "Escolha um aplicativo para abrir o documento “%s”.", nm);
    text_draw_ellipsis(sf, font_get(FONT_REGULAR), 13, 20, 28, u->cw - 40, T.text, msg);
    static const char *modes[] = {"Aplicativos Recomendados", "Todos os Aplicativos"};
    ui_label(u, 20, 52, "Habilitar:", FONT_REGULAR, 12.5f, T.text2);
    int prev = c->all;
    c->all = ui_popup(u, 0x7B001, rect_make(90, 38, 230, 24), modes, 2, c->all);
    if (c->all != prev || !c->n) choose_fill(c);
    rect_t list = rect_make(20, 72, u->cw - 40, u->ch - 72 - 92);
    ui_group_box(u, list);
    int rh = 30, ch = c->n * rh;
    ui_scroll(u, 0x7B002, list, ch, &c->scroll);
    rect_t clip0 = surface_push_clip(sf, list);
    for (int i = 0; i < c->n; i++) {
        rect_t r = rect_make(list.x + 4, list.y + 4 + i * rh - c->scroll, list.w - 8, rh - 2);
        if (r.y + r.h < list.y || r.y > list.y + list.h) continue;
        if (ui_list_row(u, 0x7B100 + i, r, i == c->sel, false)) {
            if (c->sel == i && u->dblclick) { c->sel = i; goto open_now; }
            c->sel = i;
        }
        ls_app_draw_icon(c->list[i], sf, r.x + 6, r.y + 3, 22);
        text_draw_ellipsis(sf, font_get(FONT_REGULAR), 13, r.x + 36, r.y + 19, r.w - 44, i == c->sel ? RGB(255, 255, 255) : T.text, ls_app_name(c->list[i]));
    }
    surface_pop_clip(sf, clip0);
    ui_checkbox_ex(u, 0x7B003, 20, u->ch - 80, "Sempre Abrir com", &c->always, true);
    if (ui_button_style(u, 0x7B004, rect_make(u->cw - 200, u->ch - 44, 84, 28), "Cancelar", BTN_GRAY, true, NULL)) { wm_close(w); return; }
    if (ui_button_style(u, 0x7B005, rect_make(u->cw - 108, u->ch - 44, 88, 28), "Abrir", BTN_FILLED | BTN_DEFAULT, c->sel >= 0, NULL)) goto open_now;
    if (u->key == KEY_ESC) { wm_close(w); return; }
    return;
open_now:
    if (c->sel < 0 || c->sel >= c->n) return;
    {
        ls_app_t *a = c->list[c->sel];
        if (c->always) ls_set_handler_for_file(c->path, a);   /* "Sempre Abrir com": este documento */
        ls_open(c->path, a);
        wm_close(w);
    }
}
static void choose_close(window_t *w) { kfree(w->data); }
void finder_choose_app(const char *path) {
    choose_t *c = kcalloc(1, sizeof *c);
    if (!c) return;
    snprintf(c->path, sizeof c->path, "%s", path);
    c->sel = -1;
    choose_fill(c);
    window_t *w = wm_create(&app_arquivos, "Escolher Aplicativo", -1, -1, 460, 480, 0);
    if (!w) { kfree(c); return; }
    w->data = c;
    w->paint = choose_paint;
    w->on_close = choose_close;
}

/* Comprimir: os itens selecionados viram um .zip na mesma pasta (Utilitário de Compactação, coreservices.c) */
bool archive_compress(const char *const *paths, int npaths, char *out, usize cap);
static void compress_selected(fstate_t *s, item_t *items, int n) {
    static char paths[64][512];
    const char *pp[64];
    int k = 0;
    for (int i = 0; i < n && k < 64; i++) {
        if (!items[i].node || !is_sel(s, i)) continue;
        vfs_path(items[i].node, paths[k], sizeof paths[k]);
        pp[k] = paths[k];
        k++;
    }
    char out[512];
    if (k) archive_compress(pp, k, out, sizeof out);
}

static void ow_result(int v) {
    if (v == OW_OTHER) { finder_choose_app(ow_path); return; }
    int i = v - OW_ACT;
    if (i >= 0 && i < ow_n) ls_open(ow_path, ow_apps[i]);
}

static void context_menus(ui_t *u, fstate_t *s, item_t *items, int n, bool ctx_item, bool ctx_bg) {
    static const char *const tag_labels[] = {"Etiqueta Vermelho", "Etiqueta Laranja", "Etiqueta Amarelo", "Etiqueta Verde", "Etiqueta Azul", "Etiqueta Roxo", "Etiqueta Cinza"};
    static const char *bg_menu[] = {"Nova Pasta", "Visualizar como Ícones", "Visualizar como Lista", "Visualizar como Colunas"};
    rect_t cr = wm_content_rect(u->win);
    if (ctx_item) {
        item_t *it = &items[s->sel >= 0 && s->sel < n ? s->sel : 0];
        static menu_item_t mi[24], sub[24];
        static menu_t sub_menu = {"Abrir com", sub, 0};
        int k = 0;
        mi[k++] = (menu_item_t){"Abrir", NULL, 0};
        char p[512] = "";
        if (it->node) vfs_path(it->node, p, sizeof p);
        /* documentos: "Abrir com ›" (pastas e apps não têm) */
        if (it->node && !it->node->dir) {
            sub_menu.count = ow_build(p, sub, 24);
            mi[k] = (menu_item_t){"Abrir com", NULL, -1};
            mi[k++].submenu = &sub_menu;
        }
        mi[k] = (menu_item_t){NULL}; mi[k++].disabled = true;
        mi[k++] = (menu_item_t){"Visualização Rápida", NULL, 1};
        mi[k++] = (menu_item_t){"Renomear", NULL, 2};
        mi[k++] = (menu_item_t){"Obter Informações", NULL, 3};
        if (it->node) {
            /* "Comprimir “X”" (ou "Comprimir N Itens" com seleção múltipla), como no Finder */
            static char comp_label[300];
            int nsel = 0;
            for (int i = 0; i < n; i++) if (items[i].node && is_sel(s, i)) nsel++;
            if (nsel > 1) snprintf(comp_label, sizeof comp_label, "Comprimir %d Itens", nsel);
            else snprintf(comp_label, sizeof comp_label, "Comprimir “%s”", it->node->name);
            mi[k++] = (menu_item_t){comp_label, NULL, 12};
            mi[k++] = (menu_item_t){"Mover para o Lixo", NULL, 4};
            mi[k] = (menu_item_t){NULL}; mi[k++].disabled = true;
            for (int t = 0; t < 7; t++) mi[k++] = (menu_item_t){tag_labels[t], NULL, 5 + t};
        }
        popup_open_menu(cr.x + u->mx, cr.y + u->my, 220, mi, k, u->win, CTX_ITEM_ID);
    }
    if (ctx_bg) popup_open(cr.x + u->mx, cr.y + u->my, 210, bg_menu, 4, -1, u->win, CTX_BG_ID);
    int v = -1;
    bool got = popup_take_result(u->win, CTX_ITEM_ID, &v);
    if (got && v >= OW_ACT) { ow_result(v); u->clicked_any = true; }
    else if (got && v >= 0 && s->sel >= 0 && s->sel < n) {
        item_t *it = &items[s->sel];
        if (v == 0) open_item(s, it);
        else if (v == 1) s->ql = true;
        else if (v == 2) rename_begin(u, s, items, n);
        else if (v == 3) open_info(it);
        else if (v == 4) trash_selected(s, items, n);
        else if (v == 12) compress_selected(s, items, n);
        else if (v >= 5 && v <= 11 && it->node) tag_toggle(it->node, v - 4);
        u->clicked_any = true;
    }
    if (popup_take_result(u->win, CTX_BG_ID, &v)) {
        if (v == 0 && strcmp(s->path, "/Aplicativos") && !vfs_is_read_only(vfs_lookup(s->path))) {
            char p[256];
            snprintf(p, sizeof p, "%s/sem título", strcmp(s->path, "/") ? s->path : "");
            int k = 2;
            while (vfs_lookup(p)) snprintf(p, sizeof p, "%s/sem título %d", strcmp(s->path, "/") ? s->path : "", k++);
            vfs_mkdir(p);
        } else if (v >= 1) s->view = v - 1;
        u->clicked_any = true;
    }
}

static void paint(window_t *w, ui_t *u) {
    fstate_t *s = w->data;
    surface_t *sf = u->s;
    s->ndrops = 0; /* alvos de soltura são recolhidos a cada quadro */
    int tb = w->toolbar_h, sbw = w->sidebar_w;
    /* ---------------- barra de ferramentas */
    bool back = s->hpos > 0, fwd = s->hpos < s->nhist - 1;
    /* botões de navegação desenhados manualmente (chevrons) */
    /* barra unificada: [‹ ›] Título … [Ícones|Lista] [Nova Pasta] [Buscar] — à direita da barra lateral */
    /* espaçamento do cabeçalho (como no sistema): 16 px após a barra lateral/semáforos, 8 px entre grupos,
     * 8 px entre o grupo e o título e 8 px de margem à direita */
    int tbx = sbw > 0 ? sbw + 16 : 129; /* recolhida: 8 px depois do botão da barra lateral */
    rect_t rb = rect_make(tbx, 0, 36, 36), rf = rect_make(tbx + 36, 0, 36, 36);
    {
        u32 c1 = back ? T.control_text : T.text3, c2 = fwd ? T.control_text : T.text3;
        ui_tool_group(u, rect_make(rb.x, rb.y, rb.w + rf.w, rb.h));
        gfx_fill(sf, rf.x, rf.y + 7, 1, rf.h - 14, T.separator);
        if (back && ui_hit(u, rb)) gfx_round_rect(sf, rb.x + 2, rb.y + 2, rb.w - 3, rb.h - 4, 11, T.dark ? RGBA(255, 255, 255, 20) : RGBA(0, 0, 0, 12));
        if (fwd && ui_hit(u, rf)) gfx_round_rect(sf, rf.x + 1, rf.y + 2, rf.w - 3, rf.h - 4, 11, T.dark ? RGBA(255, 255, 255, 20) : RGBA(0, 0, 0, 12));
        glyph_chevron(sf, rb.x + 10, rb.y + 10, 16, c1, 1);
        glyph_chevron(sf, rf.x + 10, rf.y + 10, 16, c2, 0);
        if (back && ui_clicked(u, UI_ID, rb)) { s->hpos--; go(s, s->hist[s->hpos], false); }
        if (fwd && ui_clicked(u, UI_ID, rf)) { s->hpos++; go(s, s->hist[s->hpos], false); }
    }
    /* janela estreita: "Nova Pasta" sai da barra (fica no menu, ⇧⌘N) para o título caber */
    bool show_nf = u->cw - (tbx + 84) - 192 - 236 - 104 - 40 >= 140;
    int srch_x = u->cw - 8 - 192, nf_x = show_nf ? srch_x - 8 - 104 : srch_x, seg_x = nf_x - 8 - 236;
    static const char *views[] = {"Ícones", "Lista", "Colunas", "Galeria"};
    s->view = ui_segmented(u, UI_ID, rect_make(seg_x, 0, 236, 36), views, 4, s->view);
    /* marcas nos menus Visualizar e Ordenar por refletem esta janela */
    for (int i = 0; i < 4; i++) { m_view[i].checked = s->view == i; m_sort[i].checked = s->sort == i && !s->groups; }
    m_view[6].checked = s->groups;
    if (!s->colw[0]) { s->colw[0] = MAX(160, (u->cw - sbw) / 2 - 40); s->colw[1] = 150; s->colw[2] = 90; }
    if (!s->colsw) s->colsw = 220;
    if (show_nf && ui_toolbar_button(u, UI_ID, rect_make(nf_x, 0, 104, 36), "Nova Pasta", strcmp(s->path, "/Aplicativos") != 0 && !vfs_is_read_only(vfs_lookup(s->path)))) {
        char p[256];
        snprintf(p, sizeof p, "%s/sem título", strcmp(s->path, "/") ? s->path : "");
        int k = 2;
        while (vfs_lookup(p)) snprintf(p, sizeof p, "%s/sem título %d", strcmp(s->path, "/") ? s->path : "", k++);
        vfs_mkdir(p);
    }
    ui_searchfield(u, UI_ID, rect_make(srch_x, 0, 192, 36), s->search, sizeof s->search);

    /* ---------------- barra lateral (painel flutuante; semáforos e botão de recolher no topo dela) */
    if (sbw > 0) {
        int ix = 10, iw = sbw - 20, rh = 24;
        int y = tb + 2;
        ui_sidebar_header(u, rect_make(ix, y, iw, 24), "Favoritos");
        y += 27;
        struct { const char *label, *path; void (*g)(surface_t *, int, int, int, u32); } fav[] = {
            {"Mesa", home_sub("/Mesa"), glyph_sb_desktop}, {"Documentos", home_sub("/Documentos"), glyph_sb_document},
            {"Downloads", home_sub("/Downloads"), glyph_sb_download}, {"Imagens", home_sub("/Imagens"), glyph_sb_photo},
            {"Música", home_sub("/Música"), glyph_sb_music}, {"Aplicativos", "/Aplicativos", glyph_sb_apps},
        };
        for (u32 i = 0; i < ARRAY_LEN(fav); i++) {
            add_drop(s, rect_make(ix, y, iw, rh), fav[i].path);
            if (ui_sidebar_item(u, 100 + i, rect_make(ix, y, iw, rh), fav[i].label, !strcmp(s->path, fav[i].path) && !s->tag, fav[i].g)) go(s, fav[i].path, true);
            y += rh;
        }
        y += 12;
        ui_sidebar_header(u, rect_make(ix, y, iw, 24), "Locais");
        y += 27;
        if (ui_sidebar_item(u, 122, rect_make(ix, y, iw, rh), account_current(), !strcmp(s->path, CUR_HOME), glyph_sb_home)) go(s, CUR_HOME, true);
        y += rh;
        if (ui_sidebar_item(u, 120, rect_make(ix, y, iw, rh), "mOS HD", !strcmp(s->path, "/"), glyph_sb_disk)) go(s, "/", true);
        y += rh;
        vnode_t *vols = vfs_child(vfs_root(), "Volumes");
        int vi = 0;
        for (vnode_t *v = vols ? vols->child : NULL; v && vi < 8; v = v->next, vi++) {
            char vp[128];
            snprintf(vp, sizeof vp, "/Volumes/%s", v->name);
            if (ui_sidebar_item(u, 130 + vi, rect_make(ix, y, iw - 22, rh), v->name, !strcmp(s->path, vp), glyph_sb_disk)) go(s, vp, true);
            rect_t er = rect_make(ix + iw - 20, y + (rh - 16) / 2, 16, 16);
            bool hov = ui_hit(u, er);
            glyph_eject(u->s, er.x + 2, er.y + 2, 12, hov ? T.text : T.text2);
            if (vfs_is_volume(v) && ui_clicked(u, 150 + vi, er)) { eject_volume(v); return; }
            y += rh;
        }
        add_drop(s, rect_make(ix, y, iw, rh), CUR_TRASH);
        if (ui_sidebar_item(u, 121, rect_make(ix, y, iw, rh), "Lixeira", !strcmp(s->path, CUR_TRASH) && !s->tag, glyph_sb_trash)) go(s, CUR_TRASH, true);
        y += rh + 12;
        /* Etiquetas: clicar mostra tudo o que tem aquela cor */
        if (y + 27 + 7 * rh < u->ch) {
            ui_sidebar_header(u, rect_make(ix, y, iw, 24), "Etiquetas");
            y += 27;
            for (int t = 1; t <= 7; t++, y += rh)
                if (ui_sidebar_item(u, 160 + t, rect_make(ix, y, iw, rh), tag_names[t], s->tag == t, sb_tags[t])) {
                    go(s, s->path, true);
                    s->tag = t;
                }
        }
    }

    /* ---------------- conteúdo */
    item_t items[256];
    int n = list_items(s, items, 256);
    /* retângulo de seleção: acompanha o ponteiro enquanto o botão está pressionado */
    if (s->band) {
        if (!u->down) s->band = false;
        else s->band_r = rect_make(MIN(s->band_x, u->mx), MIN(s->band_y, u->my), ABS_DIFF(u->mx, s->band_x), ABS_DIFF(u->my, s->band_y));
    }
    int cx0 = sbw + 1, cy0 = tb, cw = u->cw - cx0, ch = u->ch - tb - 24;
    rect_t view = rect_make(cx0, cy0, cw, ch);
    gfx_fill(sf, view.x, view.y, view.w, view.h, T.content_bg);
    rect_t pc = surface_push_clip(sf, view);
    bool hit_any = false, ctx_item = false, ctx_bg = false;
    int content_h;
    if (s->view == 0) {
        int cell_w = 104, cell_h = 96;
        int cols = MAX(1, (cw - 32) / cell_w);
        content_h = ((n + cols - 1) / cols) * cell_h + 32;
        for (int i = 0; i < n; i++) {
            int col = i % cols, row = i / cols;
            int x = cx0 + 16 + col * cell_w, yy = cy0 + 16 + row * cell_h - s->scroll;
            if (yy > cy0 + ch || yy + cell_h < cy0) continue;
            rect_t r = rect_make(x, yy, cell_w, cell_h - 4);
            bool sel = is_sel(s, i);
            if (sel) gfx_round_rect(sf, x + (cell_w - 62) / 2, yy, 62, 60, 5, T.dark ? RGBA(255, 255, 255, 40) : RGBA(0, 0, 0, 34));
            draw_item_icon(sf, &items[i], x + (cell_w - 52) / 2, yy + 4, 52);
            const char *name = shown_name(&items[i]);
            font_t *f = font_get(FONT_REGULAR);
            u8 tb_ = tag_bits(items[i].node);
            int ntb = 0;
            for (int t = 0; t < 7; t++) if (tb_ & (1 << t)) ntb++;
            int dots_w = ntb ? 6 * ntb + 4 : 0;
            int tw = MIN(text_width(f, 12, name), cell_w - 6 - dots_w);
            int lx = x + (cell_w - tw - dots_w) / 2 + dots_w;
            if (s->renaming == i) {
                rename_field(u, s, items, n, rect_make(x + 2, yy + 62, cell_w - 4, 20));
            } else {
                if (sel) gfx_round_rect(sf, lx - 4, yy + 63, tw + 8, 17, 4, T.accent);
                if (ntb) tag_dots(sf, lx - dots_w + 4, yy + 71, tb_);
                text_draw_ellipsis(sf, f, 12, lx, yy + 76, cell_w - 6 - dots_w, sel ? RGB(255, 255, 255) : T.text, name);
            }
            if (ui_hit(u, r) && u->pressed) { sel_click(u, s, i, items); hit_any = true; }
            if (s->band) s->msel[i] = !rect_empty(rect_intersect(r, s->band_r));
            if (is_folder(items[i].node) && !is_sel(s, i)) { char dp[256]; vfs_path(items[i].node, dp, sizeof dp); add_drop(s, r, dp); }
            if (ui_hit(u, r) && u->rpressed) { if (!is_sel(s, i)) { memset(s->msel, 0, sizeof s->msel); s->sel = i; } hit_any = true; ctx_item = true; }
        }
    } else if (s->view == 1) {
        /* lista no padrão atual: cabeçalho discreto e linhas em "pílula" alternadas */
        int rh = 26, hh = 28, gh = 28;
        gfx_fill(sf, cx0 + 12, cy0 + hh - 1, cw - 24, 1, T.separator);
        int c1 = cx0 + 40, c2 = c1 + s->colw[0], c3 = c2 + s->colw[1], c4 = c3 + s->colw[2];
        ui_label(u, c1, cy0 + 7, "Nome", FONT_SEMIBOLD, 12, T.text2);
        ui_label(u, c2, cy0 + 7, "Data de Modificação", FONT_SEMIBOLD, 12, T.text2);
        ui_label(u, c3, cy0 + 7, "Tamanho", FONT_SEMIBOLD, 12, T.text2);
        ui_label(u, c4, cy0 + 7, "Tipo", FONT_SEMIBOLD, 12, T.text2);
        /* divisórias do cabeçalho: arrastar muda a largura da coluna à esquerda */
        int divs[3] = {c2 - 8, c3 - 8, c4 - 8};
        for (int k = 0; k < 3; k++) {
            rect_t dr = rect_make(divs[k] - 4, cy0 + 4, 9, hh - 8);
            gfx_fill(sf, divs[k], cy0 + 7, 1, hh - 14, T.separator);
            if (ui_hit(u, dr) || s->col_drag == k) u->cursor = CURSOR_RESIZE_COL;
            if (ui_hit(u, dr) && u->pressed) { s->col_drag = k; s->col_x0 = u->mx; s->col_w0 = s->colw[k]; hit_any = true; }
        }
        if (s->col_drag >= 0) {
            if (u->down) s->colw[s->col_drag] = CLAMP(s->col_w0 + (u->mx - s->col_x0), 60, cw - 120);
            else s->col_drag = -1;
            u->repaint = true;
        }
        int extra = 0;
        const char *last_kind = NULL;
        for (int i = 0; i < n; i++) {
            if (s->groups && items[i].node) {
                /* "Usar Grupos": cabeçalho a cada tipo novo */
                const char *k = node_kind(items[i].node);
                if (!last_kind || strcmp(k, last_kind)) {
                    int gy = cy0 + hh + 4 + i * rh + extra - s->scroll;
                    if (gy > cy0 + hh - gh && gy < cy0 + ch) {
                        text_draw(sf, font_get(FONT_BOLD), 12.5f, cx0 + 20, gy + 19, T.text, k);
                        gfx_fill(sf, cx0 + 16, gy + gh - 3, cw - 32, 1, T.separator);
                    }
                    extra += gh;
                    last_kind = k;
                }
            }
            int yy = cy0 + hh + 4 + i * rh + extra - s->scroll;
            if (yy > cy0 + ch || yy + rh < cy0 + hh) continue;
            rect_t r = rect_make(cx0, yy, cw, rh);
            bool sel = is_sel(s, i);
            if (sel) gfx_round_rect(sf, r.x + 10, r.y, r.w - 20, r.h, 6, T.accent);
            else if (i % 2 == 0) gfx_round_rect(sf, r.x + 10, r.y, r.w - 20, r.h, 6, T.alt_row);
            draw_item_icon(sf, &items[i], cx0 + 16, yy + 4, 18);
            u32 tc = sel ? RGB(255, 255, 255) : T.text, t2 = sel ? RGB(255, 255, 255) : T.text2;
            const char *name = shown_name(&items[i]);
            u8 tb_ = tag_bits(items[i].node);
            if (s->renaming == i) rename_field(u, s, items, n, rect_make(c1 - 4, yy + 2, c2 - c1 - 8, rh - 4));
            else {
                int nw = MIN(text_width(font_get(FONT_REGULAR), 13.5f, name), c2 - c1 - 30);
                text_draw_ellipsis(sf, font_get(FONT_REGULAR), 13.5f, c1, yy + 18, c2 - c1 - 30, tc, name);
                if (tb_) tag_dots(sf, c1 + nw + 10, yy + rh / 2, tb_);
            }
            char sz[32] = "--", when[40] = "--", kind[40];
            if (items[i].app) strcpy(kind, "Aplicativo");
            else {
                strcpy(kind, node_kind(items[i].node));
                if (!items[i].node->dir) fmt_size(items[i].node->size, sz, sizeof sz);
                datetime_t dt;
                rtc_now(&dt);
                if (items[i].node->mtime) snprintf(when, sizeof when, "Hoje, %02d:%02d", dt.hour, dt.minute);
                else snprintf(when, sizeof when, "—");
            }
            text_draw(sf, font_get(FONT_REGULAR), 13, c2, yy + 18, t2, when);
            text_draw(sf, font_get(FONT_REGULAR), 13, c3, yy + 18, t2, sz);
            text_draw_ellipsis(sf, font_get(FONT_REGULAR), 13, c4, yy + 18, cx0 + cw - c4 - 16, t2, kind);
            if (ui_hit(u, r) && u->pressed) { sel_click(u, s, i, items); hit_any = true; }
            if (s->band) s->msel[i] = !rect_empty(rect_intersect(r, s->band_r));
            if (is_folder(items[i].node) && !is_sel(s, i)) { char dp[256]; vfs_path(items[i].node, dp, sizeof dp); add_drop(s, r, dp); }
            if (ui_hit(u, r) && u->rpressed) { if (!is_sel(s, i)) { memset(s->msel, 0, sizeof s->msel); s->sel = i; } hit_any = true; ctx_item = true; }
        }
        content_h = n * rh + hh + 8 + extra;
    } else if (s->view == 2) {
        content_h = paint_columns(u, s, items, n, view, &hit_any, &ctx_item);
    } else {
        /* Galeria: prévia grande do item escolhido e a fita de miniaturas embaixo */
        content_h = view.h;
        int strip_h = 92, pv_h = ch - strip_h - 44;
        if (n && s->sel < 0) s->sel = 0;
        if (n && s->sel < n) {
            item_t *it = &items[s->sel];
            rect_t pv = rect_make(cx0 + 24, cy0 + 16, cw - 48, pv_h - 16);
            surface_t *img = it->node ? preview_image(s, it->node) : NULL;
            if (img) {
                float k = MIN((float)pv.w / img->w, (float)pv.h / img->h);
                if (k > 1) k = 1;
                int iw = (int)(img->w * k), ih = (int)(img->h * k);
                gfx_shadow(sf, pv.x + (pv.w - iw) / 2, pv.y + (pv.h - ih) / 2, iw, ih, 4, 14, T.dark ? 120 : 50, 4);
                gfx_blit_scaled(sf, img, pv.x + (pv.w - iw) / 2, pv.y + (pv.h - ih) / 2, iw, ih, 255);
            } else {
                int isz = MIN(192, MIN(pv.w, pv.h) - 20);
                draw_item_icon(sf, it, pv.x + (pv.w - isz) / 2, pv.y + (pv.h - isz) / 2, isz);
            }
            const char *name = shown_name(it);
            text_draw_center(sf, font_get(FONT_SEMIBOLD), 14, rect_make(cx0, cy0 + pv_h, cw, 22), T.text, name);
            char info[96], szs[32] = "";
            if (it->node && !it->node->dir) fmt_size(it->node->size, szs, sizeof szs);
            snprintf(info, sizeof info, "%s%s%s", it->app ? "Aplicativo" : node_kind(it->node), szs[0] ? " — " : "", szs);
            text_draw_center(sf, font_get(FONT_REGULAR), 12, rect_make(cx0, cy0 + pv_h + 20, cw, 18), T.text2, info);
            if (ui_hit(u, pv) && u->pressed) { hit_any = true; if (u->dblclick) open_item(s, it); }
            if (ui_hit(u, pv) && u->rpressed) { hit_any = true; ctx_item = true; }
        }
        /* fita: o item escolhido fica no meio */
        int ty = cy0 + ch - strip_h + 10, step = 76;
        gfx_fill(sf, cx0, ty - 10, cw, 1, T.separator);
        int x0 = cx0 + cw / 2 - 32 - MAX(0, s->sel) * step;
        for (int i = 0; i < n; i++) {
            int x = x0 + i * step;
            if (x + 64 < cx0 || x > cx0 + cw) continue;
            rect_t r = rect_make(x - 4, ty - 4, 72, 72);
            if (i == s->sel) gfx_round_rect(sf, r.x, r.y, r.w, r.h, 10, T.dark ? RGBA(255, 255, 255, 40) : RGBA(0, 0, 0, 30));
            draw_item_icon(sf, &items[i], x, ty, 64);
            if (ui_hit(u, r) && u->pressed) { s->sel = i; memset(s->msel, 0, sizeof s->msel); hit_any = true; if (u->dblclick) open_item(s, &items[i]); }
            if (ui_hit(u, r) && u->rpressed) { s->sel = i; hit_any = true; ctx_item = true; }
        }
    }
    if (u->pressed && ui_hit(u, view) && !hit_any) {
        /* clique no vazio: limpa a seleção e começa o retângulo de seleção */
        s->sel = -1;
        if (!(u->mods & (MOD_CMD | MOD_SHIFT))) memset(s->msel, 0, sizeof s->msel);
        if (s->view < 2) { s->band = true; s->band_x = u->mx; s->band_y = u->my; s->band_r = rect_make(u->mx, u->my, 0, 0); }
    }
    if (s->band && s->band_r.w > 2 && s->band_r.h > 2) {
        gfx_fill(sf, s->band_r.x, s->band_r.y, s->band_r.w, s->band_r.h, WITH_ALPHA(T.accent, 40));
        gfx_rect(sf, s->band_r.x, s->band_r.y, s->band_r.w, s->band_r.h, WITH_ALPHA(T.accent, 170));
    }
    if (u->rpressed && ui_hit(u, view) && !hit_any) { s->sel = -1; ctx_bg = true; }
    if (n == 0) ui_label_center(u, view, s->search[0] ? "Nenhum resultado" : "Pasta vazia", FONT_REGULAR, 15, T.text3);
    surface_pop_clip(sf, pc);
    ui_scroll(u, UI_ID, view, content_h, &s->scroll);

    /* ---------------- barra de status */
    int by = u->ch - 24;
    char st[96], fr[32];
    fmt_size((pmm_free_pages() * 4096), fr, sizeof fr);
    snprintf(st, sizeof st, "%d %s, %s disponíveis", n, n == 1 ? "item" : "itens", fr);
    /* rodapé: caminho clicável à esquerda, contagem/espaço livre à direita */
    paint_path_bar(u, s, rect_make(cx0, by, cw, 24));
    {
        font_t *ff = font_get(FONT_REGULAR);
        int sw = text_width(ff, 11.5f, st);
        text_draw(sf, ff, 11.5f, cx0 + cw - 12 - sw, by + 16, T.text2, st);
    }
    /* título + subtítulo no cabeçalho, entre os botões de navegação e os controles da direita */
    {
        char sub[48];
        snprintf(sub, sizeof sub, "%d %s", n, n == 1 ? "item" : "itens");
        ui_title(u, tbx + 72 + 12, 0, 36, seg_x - (tbx + 72 + 12) - 16, s->tag ? tag_names[s->tag] : title_for(s->path), sub);
    }
    if (!strcmp(s->path, CUR_TRASH) && n > 0 && ui_toolbar_button(u, UI_ID, rect_make(u->cw - 90, by + 2, 82, 20), "Esvaziar", true)) {
        vnode_t *t = vfs_lookup(CUR_TRASH);
        while (t->child) vfs_delete(t->child);
    }

    /* ---------------- teclado */
    if (u->key && (u->mods & MOD_CMD) && (u->key == 'a' || u->key == 'A') && !u->focus) {
        for (int i = 0; i < n && i < 256; i++) s->msel[i] = true;
        s->sel = n ? 0 : -1;
        u->key_used = true;
    }
    /* pedidos dos menus (precisam da lista de itens deste quadro) */
    if (s->key_open && s->sel >= 0 && s->sel < n) open_item(s, &items[s->sel]);
    if (s->key_rename) rename_begin(u, s, items, n);
    if (s->key_info && s->sel >= 0 && s->sel < n) open_info(&items[s->sel]);
    if (s->key_trash && s->sel >= 0 && s->sel < n && items[s->sel].node) trash_selected(s, items, n);
    s->key_open = s->key_rename = s->key_info = s->key_trash = false;
    if (u->key && !u->key_used && s->sel >= 0 && s->sel < n && s->renaming < 0 && !u->focus) {
        /* Return renomeia (como no sistema); ⌘O ou ⌘↓ abrem */
        if (u->key == KEY_ENTER) { rename_begin(u, s, items, n); u->key_used = true; }
        else if (u->key == KEY_DOWN && (u->mods & MOD_CMD)) { open_item(s, &items[s->sel]); u->key_used = true; }
        else if (u->key == KEY_BACKSPACE && (u->mods & MOD_CMD) && items[s->sel].node && vfs_is_read_only(items[s->sel].node)) {
            notify("O volume é somente leitura", "Não é possível apagar itens de uma imagem de disco.");
            u->key_used = true;
        } else if (u->key == KEY_BACKSPACE && (u->mods & MOD_CMD) && items[s->sel].node) {
            trash_selected(s, items, n); /* ⌘⌫ move toda a seleção para o Lixo */
            u->key_used = true;
        } else if (u->key == KEY_DOWN || u->key == KEY_RIGHT) { s->sel = MIN(s->sel + 1, n - 1); u->key_used = true; }
        else if (u->key == KEY_UP || u->key == KEY_LEFT) { s->sel = MAX(s->sel - 1, 0); u->key_used = true; }
    }
    /* menus de contexto e Visualização Rápida (Espaço) — por último, por cima do conteúdo */
    context_menus(u, s, items, n, ctx_item, ctx_bg);
    drag_and_drop(u, s, items, n);
    if (u->key == ' ' && !u->key_used && u->focus == 0 && s->sel >= 0 && s->sel < n && !s->ql) { s->ql = true; u->key_used = true; }
    else if (s->ql && s->sel >= 0 && s->sel < n) paint_quicklook(u, s, &items[s->sel]);
    else s->ql = false;
    wm_set_title(w, s->tag ? tag_names[s->tag] : title_for(s->path)); /* no fim: reflete a navegação feita neste quadro */
}

static window_t *new_window(const char *path) {
    window_t *w = wm_create(&app_arquivos, "Arquivos", -1, -1, 900, 580, WIN_RESIZABLE);
    if (!w) return NULL;
    fstate_t *s = kcalloc(1, sizeof(fstate_t));
    s->sel = -1;
    s->renaming = -1;
    s->col_drag = -1;
    w->data = s;
    w->paint = paint;
    w->toolbar_h = TOOLBAR_H;
    w->sidebar_w = 220;
    w->bg = 0;          /* segue o tema (claro/escuro) */
    w->title_x = -1;    /* o app desenha título e subtítulo no cabeçalho */
    w->min_w = 520; w->min_h = 300;
    go(s, path, true);
    return w;
}

static void close_cb(window_t *w) { kfree(w->data); }

void finder_open_path(const char *path) {
    window_t *w = new_window(path);
    if (w) w->on_close = close_cb;
}
void finder_open_trash(void) { finder_open_path(CUR_TRASH); }

static void launch(app_t *a) { finder_open_path(home_sub("/Documentos")); }

static void on_menu(app_t *a, int act) {
    window_t *w = wm_focused();
    fstate_t *s = (w && w->app == &app_arquivos && w->paint == paint) ? w->data : NULL;
    switch (act) {
    case A_NEWWINDOW: finder_open_path(home_sub("/Documentos")); break;
    case A_NEWTAB: if (w && s) wm_new_tab(w); else finder_open_path(home_sub("/Documentos")); break;
    case A_OPEN: if (s) s->key_open = true; break;
    case A_RENAME: if (s) s->key_rename = true; break;
    case A_GETINFO: if (s) s->key_info = true; break;
    case A_TRASH: if (s) s->key_trash = true; break;
    case A_COLS: if (s) s->view = 2; break;
    case A_GALLERY: if (s) s->view = 3; break;
    case A_GROUPS: if (s) s->groups = !s->groups; break;
    case A_SORT0: case A_SORT0 + 1: case A_SORT0 + 2: case A_SORT0 + 3: if (s) s->sort = act - A_SORT0; break;
    case A_NEWFOLDER:
        if (s && vfs_is_read_only(vfs_lookup(s->path))) { notify("O volume é somente leitura", s->path); break; }
        if (s) {
            char p[256];
            snprintf(p, sizeof p, "%s/sem título", strcmp(s->path, "/") ? s->path : "");
            int k = 2;
            while (vfs_lookup(p)) snprintf(p, sizeof p, "%s/sem título %d", strcmp(s->path, "/") ? s->path : "", k++);
            vfs_mkdir(p);
        }
        break;
    case A_NEWFILE:
        if (s && vfs_is_read_only(vfs_lookup(s->path))) { notify("O volume é somente leitura", s->path); break; }
        if (s) {
            char p[256];
            snprintf(p, sizeof p, "%s/Sem título.txt", s->path);
            vfs_write_file(p, "", 0);
        }
        break;
    case A_EMPTYTRASH: { vnode_t *t = vfs_lookup(CUR_TRASH); while (t && t->child) vfs_delete(t->child); break; }
    case A_ICONS: if (s) s->view = 0; break;
    case A_LIST: if (s) s->view = 1; break;
    case A_GO_DOCS: if (s) go(s, home_sub("/Documentos"), true); else finder_open_path(home_sub("/Documentos")); break;
    case A_GO_HOME: if (s) go(s, home_sub("/Mesa"), true); else finder_open_path(home_sub("/Mesa")); break;
    case A_GO_DL: if (s) go(s, home_sub("/Downloads"), true); else finder_open_path(home_sub("/Downloads")); break;
    case A_GO_APPS: if (s) go(s, "/Aplicativos", true); else finder_open_path("/Aplicativos"); break;
    case A_EJECT:
        if (s) {
            /* ejeta o volume que contém a pasta atual */
            if (!strncmp(s->path, "/Volumes/", 9)) {
                char vp[512];
                strncpy(vp, s->path, sizeof vp - 1);
                vp[sizeof vp - 1] = 0;
                char *sl = strchr(vp + 9, '/');
                if (sl) *sl = 0;
                eject_volume(vfs_lookup(vp));
            }
        }
        break;
    case 2001: about_open(); break;
    }
    if (w) wm_invalidate(w);
}

app_t app_arquivos = {
    .name = "Arquivos", .exec = "arquivos", .draw_icon = icon_arquivos, .launch = launch, .on_menu = on_menu,
    .menus = menus, .nmenus = ARRAY_LEN(menus), .pinned = true,
};
