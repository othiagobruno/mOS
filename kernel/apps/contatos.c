/* mOS — Contatos: lista alfabética com busca, grupos, cartão com edição, aniversários e importação/exportação
 * vCard 3.0. Dados em ~/Biblioteca/Contatos/contatos.vcf (arquivos .vcf abertos no Arquivos são importados). */
#include "kit.h"

#define MAXC 400
#define MAXG 12
typedef struct {
    bool used;
    char first[48], last[48], org[64], title[48];
    char phone[3][32], email[2][64];
    char street[96], city[48], zip[16];
    char url[96], notes[256];
    int by, bm, bd;
    u32 groups;                 /* bit i = grupo i */
} contact_t;

static const char *phone_lbl[3] = {"celular", "casa", "trabalho"};
static const char *email_lbl[2] = {"pessoal", "trabalho"};

static struct {
    window_t *w;
    contact_t c[MAXC];
    char gname[MAXG][32];
    int ng, group;              /* -1 = Todos os Contatos */
    int sel, scroll, cscroll;
    bool editing, loaded, renaming;
    char search[64], rename_buf[32];
    contact_t backup;
} K = {.group = -1, .sel = -1};

extern app_t app_contatos;

/* ------------------------------------------------------------ vCard */
static void vc_put(sbuf_t *b, const char *key, const char *v) {
    if (!v || !v[0]) return;
    sb_puts(b, key);
    sb_putc(b, ':');
    for (; *v; v++) {
        if (*v == '\n') sb_puts(b, "\\n");
        else if (*v == ',' || *v == ';' || *v == '\\') { sb_putc(b, '\\'); sb_putc(b, *v); }
        else if (*v != '\r') sb_putc(b, *v);
    }
    sb_puts(b, "\r\n");
}
static void vcard_write(sbuf_t *b, contact_t *c) {
    sb_puts(b, "BEGIN:VCARD\r\nVERSION:3.0\r\n");
    sb_printf(b, "N:%s;%s;;;\r\n", c->last, c->first);
    char fn[100];
    snprintf(fn, sizeof fn, "%s%s%s", c->first, c->first[0] && c->last[0] ? " " : "", c->last);
    vc_put(b, "FN", fn[0] ? fn : c->org);
    vc_put(b, "ORG", c->org);
    vc_put(b, "TITLE", c->title);
    static const char *pt[3] = {"TEL;TYPE=CELL", "TEL;TYPE=HOME", "TEL;TYPE=WORK"};
    for (int i = 0; i < 3; i++) vc_put(b, pt[i], c->phone[i]);
    static const char *et[2] = {"EMAIL;TYPE=HOME", "EMAIL;TYPE=WORK"};
    for (int i = 0; i < 2; i++) vc_put(b, et[i], c->email[i]);
    if (c->street[0] || c->city[0] || c->zip[0]) sb_printf(b, "ADR;TYPE=HOME:;;%s;%s;;%s;\r\n", c->street, c->city, c->zip);
    if (c->by) sb_printf(b, "BDAY:%04d-%02d-%02d\r\n", c->by, c->bm, c->bd);
    vc_put(b, "URL", c->url);
    vc_put(b, "NOTE", c->notes);
    if (c->groups) {
        sb_puts(b, "CATEGORIES:");
        bool first = true;
        for (int g = 0; g < K.ng; g++) if (c->groups & (1u << g)) { if (!first) sb_putc(b, ','); sb_puts(b, K.gname[g]); first = false; }
        sb_puts(b, "\r\n");
    }
    sb_puts(b, "END:VCARD\r\n");
}
/* valor com escapes do vCard; componente idx de uma lista separada por ';' */
static void vc_get(const char *v, int idx, char *out, int n) {
    int comp = 0, k = 0;
    for (; *v; v++) {
        if (*v == '\\' && v[1]) {
            v++;
            if (comp == idx && k < n - 1) out[k++] = *v == 'n' || *v == 'N' ? '\n' : *v;
            continue;
        }
        if (*v == ';') { comp++; continue; }
        if (comp == idx && k < n - 1) out[k++] = *v;
    }
    out[k] = 0;
}
static int group_find_or_add(const char *nm) {
    for (int g = 0; g < K.ng; g++) if (!strcasecmp(K.gname[g], nm)) return g;
    if (K.ng >= MAXG || !nm[0]) return -1;
    snprintf(K.gname[K.ng], 32, "%s", nm);
    return K.ng++;
}
static contact_t *contact_new(void) {
    for (int i = 0; i < MAXC; i++) if (!K.c[i].used) { memset(&K.c[i], 0, sizeof K.c[i]); K.c[i].used = true; return &K.c[i]; }
    return NULL;
}
/* importa todos os cartões de um texto vCard; devolve quantos */
static int vcard_import(char *buf) {
    int n = 0;
    contact_t *c = NULL;
    char *cur = buf, *ln;

    while ((ln = kit_line(&cur))) {
        if (!strncasecmp(ln, "BEGIN:VCARD", 11)) { c = contact_new(); continue; }
        if (!c) continue;
        if (!strncasecmp(ln, "END:VCARD", 9)) {
            if (!c->first[0] && !c->last[0] && !c->org[0]) c->used = false; else n++;
            c = NULL;
            continue;
        }
        char *colon = strchr(ln, ':');
        if (!colon) continue;
        *colon = 0;
        char *key = ln, *val = colon + 1;
        char up[64];
        snprintf(up, sizeof up, "%s", key);
        for (char *p = up; *p; p++) *p = (char)toupper(*p);
        if (!strncmp(up, "N", 2) || !strncmp(up, "N;", 2)) { vc_get(val, 0, c->last, 48); vc_get(val, 1, c->first, 48); }
        else if (!strncmp(up, "FN", 2) && !c->first[0] && !c->last[0]) {
            char fn[100];
            vc_get(val, 0, fn, sizeof fn);
            char *sp = strrchr(fn, ' ');
            if (sp) { *sp = 0; snprintf(c->first, 48, "%s", fn); snprintf(c->last, 48, "%s", sp + 1); }
            else snprintf(c->first, 48, "%s", fn);
        } else if (!strncmp(up, "ORG", 3)) vc_get(val, 0, c->org, 64);
        else if (!strncmp(up, "TITLE", 5)) vc_get(val, 0, c->title, 48);
        else if (!strncmp(up, "TEL", 3)) {
            int slot = strstr(up, "HOME") ? 1 : strstr(up, "WORK") ? 2 : 0;
            if (c->phone[slot][0]) { slot = -1; for (int i = 0; i < 3; i++) if (!c->phone[i][0]) { slot = i; break; } }
            if (slot >= 0) vc_get(val, 0, c->phone[slot], 32);

        } else if (!strncmp(up, "EMAIL", 5)) {
            int slot = strstr(up, "WORK") ? 1 : 0;
            if (c->email[slot][0]) slot = c->email[0][0] ? (c->email[1][0] ? -1 : 1) : 0;
            if (slot >= 0) vc_get(val, 0, c->email[slot], 64);

        } else if (!strncmp(up, "ADR", 3)) { vc_get(val, 2, c->street, 96); vc_get(val, 3, c->city, 48); vc_get(val, 5, c->zip, 16); }
        else if (!strncmp(up, "BDAY", 4)) {
            char d[16];
            vc_get(val, 0, d, sizeof d);
            int y = 0, m = 0, dd = 0;
            if (strlen(d) >= 10 && d[4] == '-') { y = atoi(d); m = atoi(d + 5); dd = atoi(d + 8); }
            else if (strlen(d) == 8) { char t[5] = {d[0], d[1], d[2], d[3], 0}; y = atoi(t); m = (d[4] - '0') * 10 + d[5] - '0'; dd = (d[6] - '0') * 10 + d[7] - '0'; }
            if (m >= 1 && m <= 12 && dd >= 1 && dd <= 31) { c->by = y; c->bm = m; c->bd = dd; }
        } else if (!strncmp(up, "URL", 3)) vc_get(val, 0, c->url, 96);
        else if (!strncmp(up, "NOTE", 4)) vc_get(val, 0, c->notes, 256);
        else if (!strncmp(up, "CATEGORIES", 10)) {
            char cats[200];
            vc_get(val, 0, cats, sizeof cats);
            char *p = cats;
            while (*p) {
                char *e = strchr(p, ',');
                if (e) *e = 0;
                int g = group_find_or_add(p);
                if (g >= 0) c->groups |= 1u << g;
                if (!e) break;
                p = e + 1;
            }
        }
    }
    if (c && !c->first[0] && !c->last[0]) c->used = false;
    return n;
}

static void save(void) {
    sbuf_t b = {0};
    /* grupos vazios também persistem (linha X-MOS-GROUPS no começo) */
    sb_puts(&b, "X-MOS-GROUPS:");
    for (int g = 0; g < K.ng; g++) { if (g) sb_putc(&b, ','); sb_puts(&b, K.gname[g]); }
    sb_puts(&b, "\r\n");
    for (int i = 0; i < MAXC; i++) if (K.c[i].used) vcard_write(&b, &K.c[i]);
    char p[200];
    kit_save(kit_lib("Contatos", "contatos.vcf", p, sizeof p), &b);
}

static void load(void) {
    if (K.loaded) return;
    K.loaded = true;
    char p[200];
    char *buf = kit_read(kit_lib("Contatos", "contatos.vcf", p, sizeof p), NULL);
    if (!buf) {
        static const char *demo =
            "X-MOS-GROUPS:Família,Trabalho\r\n"
            "BEGIN:VCARD\r\nN:Souza;Ana;;;\r\nTEL;TYPE=CELL:+55 11 98765-4321\r\nEMAIL;TYPE=HOME:ana.souza@exemplo.com\r\nBDAY:1990-03-14\r\nCATEGORIES:Família\r\nEND:VCARD\r\n"
            "BEGIN:VCARD\r\nN:Lima;Bruno;;;\r\nORG:Estúdio Dunas\r\nTITLE:Designer\r\nTEL;TYPE=WORK:+55 21 3333-1000\r\nEMAIL;TYPE=WORK:bruno@dunas.exemplo\r\nCATEGORIES:Trabalho\r\nEND:VCARD\r\n"
            "BEGIN:VCARD\r\nN:Ferreira;Carla;;;\r\nTEL;TYPE=CELL:+55 31 99999-0000\r\nADR;TYPE=HOME:;;Rua das Flores, 123;Belo Horizonte;;30000-000;\r\nEND:VCARD\r\n";
        buf = strdup(demo);
        K.ng = 0;
        char *g = strstr(buf, "X-MOS-GROUPS:");
        (void)g;
        group_find_or_add("Família");
        group_find_or_add("Trabalho");
        vcard_import(buf);
        kfree(buf);
        save();
        return;
    }
    if (!strncmp(buf, "X-MOS-GROUPS:", 13)) {
        char *e = buf + 13;
        while (*e && *e != '\r' && *e != '\n') e++;
        char tmp[400];
        int l = e ? (int)(e - buf - 13) : (int)strlen(buf + 13);
        l = MIN(l, 399);
        memcpy(tmp, buf + 13, l);
        tmp[l] = 0;
        char *q = tmp;
        while (*q) { char *c = strchr(q, ','); if (c) *c = 0; group_find_or_add(q); if (!c) break; q = c + 1; }
    }
    vcard_import(buf);
    kfree(buf);
}

/* chamado pelo Arquivos ao abrir um .vcf */
void contacts_import_file(const char *path) {
    load();
    char *buf = kit_read(path, NULL);
    if (!buf) return;
    int n = vcard_import(buf);
    kfree(buf);
    save();
    char m[80];
    snprintf(m, sizeof m, "%d contato%s importado%s.", n, n == 1 ? "" : "s", n == 1 ? "" : "s");
    notify("Contatos", m);
    app_launch(&app_contatos);
}

/* ------------------------------------------------------------ ordenação e filtro */
static void contact_name(contact_t *c, char *out, int n) {
    if (c->first[0] || c->last[0]) snprintf(out, n, "%s%s%s", c->first, c->first[0] && c->last[0] ? " " : "", c->last);
    else if (c->org[0]) snprintf(out, n, "%s", c->org);
    else snprintf(out, n, "Sem Nome");
}
static const char *sort_key(contact_t *c) { return c->last[0] ? c->last : c->first[0] ? c->first : c->org; }
static bool visible(contact_t *c) {
    if (!c->used) return false;
    if (K.group >= 0 && !(c->groups & (1u << K.group))) return false;
    if (K.search[0]) {
        char a[600];
        snprintf(a, sizeof a, "%s %s %s %s %s %s %s %s", c->first, c->last, c->org, c->phone[0], c->phone[1], c->phone[2], c->email[0], c->email[1]);
        for (char *p = a; *p; p++) *p = (char)tolower(*p);
        char q[64];
        snprintf(q, sizeof q, "%s", K.search);
        for (char *p = q; *p; p++) *p = (char)tolower(*p);
        return strstr(a, q) != NULL;
    }
    return true;
}
static int order[MAXC];
static int build_order(void) {
    int n = 0;
    for (int i = 0; i < MAXC; i++) if (visible(&K.c[i])) order[n++] = i;
    for (int i = 1; i < n; i++) {   /* inserção: poucas centenas */
        int v = order[i], j = i - 1;
        while (j >= 0 && strcasecmp(sort_key(&K.c[order[j]]), sort_key(&K.c[v])) > 0) { order[j + 1] = order[j]; j--; }
        order[j + 1] = v;
    }
    return n;
}

/* ------------------------------------------------------------ ações */
static void new_contact(void) {
    contact_t *c = contact_new();
    if (!c) return;
    if (K.group >= 0) c->groups = 1u << K.group;
    K.sel = (int)(c - K.c);
    K.backup = *c;
    K.editing = true;
    K.search[0] = 0;
    save();
}
static void delete_cb(int b, void *ctx) {
    if (b != 1 || K.sel < 0) return;
    K.c[K.sel].used = false;
    K.sel = -1;
    K.editing = false;
    save();
}
static void delete_contact(void) {
    if (K.sel < 0) return;
    static const char *b[] = {"Cancelar", "Apagar"};
    char nm[100], t[140];
    contact_name(&K.c[K.sel], nm, sizeof nm);
    snprintf(t, sizeof t, "Tem certeza de que deseja apagar o cartão de \"%s\"?", nm);
    alert_show(t, "Esta ação não pode ser desfeita.", b, 2, delete_cb, NULL);
}
static void export_vcf(void) {
    if (K.sel < 0) return;
    sbuf_t b = {0};
    vcard_write(&b, &K.c[K.sel]);
    char nm[100], p[200];
    contact_name(&K.c[K.sel], nm, sizeof nm);
    snprintf(p, sizeof p, "%s/Mesa/%s.vcf", CUR_HOME, nm);
    vfs_write_file(p, b.p, b.len);
    sb_free(&b);
    notify("Cartão exportado", p);
}

/* ------------------------------------------------------------ desenho */
static void g_person(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_circle_stroke(s, x + sz * 0.5f, y + sz * 0.34f, sz * 0.16f, MAX(1.2f, sz * 0.08f), c);
    gfx_ring_arc(s, x + sz * 0.5f, y + sz * 0.9f, sz * 0.32f, MAX(1.2f, sz * 0.08f), PI_F, 2 * PI_F, c);
}
static void g_group(surface_t *s, int x, int y, int sz, u32 c) {
    g_person(s, x - sz * 0.14f, y + sz * 0.04f, (int)(sz * 0.9f), c);
    g_person(s, x + sz * 0.26f, y, (int)(sz * 0.8f), c);
}

static void paint_sidebar(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    int sb = w->sidebar_w, y = w->toolbar_h + 4;
    if (ui_sidebar_item(u, 20000, rect_make(10, y, sb - 20, 30), "Todos os Contatos", K.group < 0, g_person)) { K.group = -1; K.sel = -1; K.editing = false; }
    y += 40;
    ui_sidebar_header(u, rect_make(10, y, sb - 20, 22), "Grupos");
    y += 24;
    for (int g = 0; g < K.ng; g++) {
        rect_t r = rect_make(10, y, sb - 20, 30);
        if (K.renaming && K.group == g) {
            if (ui_textfield(u, 20200, rect_make(r.x + 30, r.y + 3, r.w - 36, 24), K.rename_buf, sizeof K.rename_buf, "Nome do grupo")) {
                if (K.rename_buf[0]) snprintf(K.gname[g], 32, "%s", K.rename_buf);
                K.renaming = false;
                save();
            } else if (u->focus != 20200) u->focus = 20200;
        } else if (ui_sidebar_item(u, 20100 + g, r, K.gname[g], K.group == g, g_group)) {
            if (K.group == g && u->dblclick) { K.renaming = true; snprintf(K.rename_buf, 32, "%s", K.gname[g]); }
            K.group = g; K.sel = -1; K.editing = false;
        }
        y += 32;
    }
    if (ui_button_style(u, UI_ID, rect_make(10, u->ch - 36, 130, 28), "Novo Grupo", BTN_PLAIN_GRAY, K.ng < MAXG, glyph_plus)) {
        int g = group_find_or_add("Novo Grupo");
        if (g >= 0) { K.group = g; K.renaming = true; snprintf(K.rename_buf, 32, "Novo Grupo"); save(); }
    }
    (void)s;
}

static void paint_list(window_t *w, ui_t *u, rect_t a) {
    surface_t *s = u->s;
    gfx_fill(s, a.x, 0, a.w, u->ch, T.content_bg);
    gfx_fill(s, a.x + a.w - 1, 0, 1, u->ch, T.separator);
    ui_searchfield(u, UI_ID, rect_make(a.x + 12, a.y + 6, a.w - 24, 28), K.search, sizeof K.search);
    int n = build_order();
    rect_t view = rect_make(a.x, a.y + 42, a.w, a.h - 42);
    rect_t pc = surface_push_clip(s, view);
    int y = view.y - K.scroll;
    char last = 0;
    for (int i = 0; i < n; i++) {
        contact_t *c = &K.c[order[i]];
        char L = (char)toupper(sort_key(c)[0]);
        if (!isalpha(L)) L = '#';
        if (L != last && !K.search[0]) {
            last = L;
            char h[2] = {L, 0};
            text_draw(s, font_get(FONT_BOLD), 12, a.x + 20, y + 17, T.text2, h);
            gfx_fill(s, a.x + 20, y + 23, a.w - 34, 1, T.separator);
            y += 26;
        }
        rect_t r = rect_make(a.x, y, a.w, 30);
        bool sel = K.sel == order[i];
        ui_list_row(u, 21000 + order[i], r, sel, false);
        if (ui_clicked(u, 21000 + order[i], r)) { if (K.editing) save(); K.sel = order[i]; K.editing = false; K.cscroll = 0; }
        u32 tc = sel ? RGB(255, 255, 255) : T.text;
        int x = a.x + 20;
        if (c->first[0] && c->last[0]) {
            x += text_draw(s, font_get(FONT_REGULAR), 13.5f, x, y + 20, tc, c->first) + 4;
            text_draw_ellipsis(s, font_get(FONT_BOLD), 13.5f, x, y + 20, a.x + a.w - 16 - x, tc, c->last);
        } else {
            char nm[100];
            contact_name(c, nm, sizeof nm);
            text_draw_ellipsis(s, font_get(FONT_BOLD), 13.5f, x, y + 20, a.w - 36, tc, nm);
        }
        y += 30;
    }
    if (!n) ui_label_center(u, rect_make(a.x, view.y + 40, a.w, 24), K.search[0] ? "Nenhum Resultado" : "Nenhum Contato", FONT_SEMIBOLD, 15, T.text3);
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, y + K.scroll - view.y + 10, &K.scroll);
}

/* linha do cartão: rótulo à direita da coluna, valor à esquerda; no modo edição, campo de texto */
static int card_row(ui_t *u, int id, int x, int y, int w, const char *label, char *val, int cap, const char *ph, u32 vcolor) {
    if (!K.editing && !val[0]) return y;
    surface_t *s = u->s;
    int lw = 110;
    int tw = text_width(font_get(FONT_REGULAR), 12, label);
    text_draw(s, font_get(FONT_REGULAR), 12, x + lw - tw, y + 18, T.text2, label);
    if (K.editing) ui_textfield(u, id, rect_make(x + lw + 12, y, w - lw - 12, 26), val, cap, ph);
    else text_draw_ellipsis(s, font_get(FONT_REGULAR), 13.5f, x + lw + 12, y + 18, w - lw - 12, vcolor ? vcolor : T.text, val);
    return y + (K.editing ? 32 : 28);
}

static void action_btn(ui_t *u, int id, int cx, int y, const char *label, glyph_fn g, bool enabled) {
    rect_t r = rect_make(cx - 34, y, 68, 52);
    bool hov = enabled && ui_hit(u, r);
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, 12, enabled ? (T.dark ? RGBA(255, 255, 255, hov ? 36 : 24) : RGBA(0, 0, 0, hov ? 16 : 9)) : RGBA(128, 128, 128, 14));
    g(u->s, cx - 10, y + 7, 20, enabled ? T.accent : T.text3);
    ui_label_center(u, rect_make(r.x, y + 30, r.w, 18), label, FONT_REGULAR, 11, enabled ? T.accent : T.text3);
    if (enabled && ui_clicked(u, id, r)) {
        char nm[100];
        contact_name(&K.c[K.sel], nm, sizeof nm);
        char m[160];
        snprintf(m, sizeof m, "%s — %s", label, nm);
        notify("Contatos", m);
    }
}
static void g_msg(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_round_rect(s, x + sz * 0.1f, y + sz * 0.16f, sz * 0.8f, sz * 0.58f, sz * 0.28f, c);
    float t[] = {x + sz * 0.24f, y + sz * 0.62f, x + sz * 0.42f, y + sz * 0.7f, x + sz * 0.18f, y + sz * 0.86f};
    gfx_polygon(s, t, 3, c);
}
static void g_phone(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_ring_arc(s, x + sz * 0.62f, y + sz * 0.38f, sz * 0.42f, sz * 0.16f, PI_F * 0.55f, PI_F * 1.05f, c);
    gfx_circle(s, x + sz * 0.26f, y + sz * 0.2f, sz * 0.12f, c);
    gfx_circle(s, x + sz * 0.8f, y + sz * 0.74f, sz * 0.12f, c);
}
static void g_video(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_round_rect(s, x + sz * 0.06f, y + sz * 0.28f, sz * 0.6f, sz * 0.46f, sz * 0.1f, c);
    float t[] = {x + sz * 0.7f, y + sz * 0.5f, x + sz * 0.94f, y + sz * 0.3f, x + sz * 0.94f, y + sz * 0.72f};
    gfx_polygon(s, t, 3, c);
}
static void g_mail(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_round_rect(s, x + sz * 0.08f, y + sz * 0.24f, sz * 0.84f, sz * 0.56f, sz * 0.1f, c);
    gfx_line(s, x + sz * 0.14f, y + sz * 0.3f, x + sz * 0.5f, y + sz * 0.56f, MAX(1.2f, sz * 0.07f), RGB(255, 255, 255));
    gfx_line(s, x + sz * 0.86f, y + sz * 0.3f, x + sz * 0.5f, y + sz * 0.56f, MAX(1.2f, sz * 0.07f), RGB(255, 255, 255));
}

static void paint_card(window_t *w, ui_t *u, rect_t a) {
    surface_t *s = u->s;
    gfx_fill(s, a.x, 0, a.w, u->ch, T.content_bg);
    if (K.sel < 0 || !K.c[K.sel].used) {
        ui_label_center(u, rect_make(a.x, a.y + a.h / 2 - 20, a.w, 30), "Nenhum Cartão Selecionado", FONT_SEMIBOLD, 17, T.text3);
        return;
    }
    contact_t *c = &K.c[K.sel];
    rect_t view = rect_make(a.x, a.y, a.w, a.h - 48);
    rect_t pc = surface_push_clip(s, view);
    int y = a.y + 20 - K.cscroll, cx = a.x + a.w / 2;
    char nm[100];
    contact_name(c, nm, sizeof nm);
    kit_avatar(s, cx, y + 46, 46, nm);
    y += 104;
    if (K.editing) {
        int fw = MIN(380, a.w - 60), fx = cx - fw / 2;
        ui_textfield(u, UI_ID, rect_make(fx, y, fw / 2 - 4, 28), c->first, 48, "Nome");
        ui_textfield(u, UI_ID, rect_make(fx + fw / 2 + 4, y, fw / 2 - 4, 28), c->last, 48, "Sobrenome");
        y += 34;
        ui_textfield(u, UI_ID, rect_make(fx, y, fw / 2 - 4, 26), c->org, 64, "Empresa");
        ui_textfield(u, UI_ID, rect_make(fx + fw / 2 + 4, y, fw / 2 - 4, 26), c->title, 48, "Cargo");
        y += 40;
    } else {
        ui_label_center(u, rect_make(a.x, y, a.w, 30), nm, FONT_BOLD, 24, T.text);
        y += 32;
        if (c->org[0] || c->title[0]) {
            char o[120];
            snprintf(o, sizeof o, "%s%s%s", c->title, c->title[0] && c->org[0] ? " — " : "", c->org);
            ui_label_center(u, rect_make(a.x, y, a.w, 20), o, FONT_REGULAR, 13, T.text2);
            y += 24;
        }
        y += 8;
        int bw = 76;
        action_btn(u, 22001, cx - bw * 3 / 2, y, "mensagem", g_msg, c->phone[0][0] || c->phone[1][0]);
        action_btn(u, 22002, cx - bw / 2, y, "ligar", g_phone, c->phone[0][0] || c->phone[1][0] || c->phone[2][0]);
        action_btn(u, 22003, cx + bw / 2, y, "vídeo", g_video, c->phone[0][0] != 0);
        action_btn(u, 22004, cx + bw * 3 / 2, y, "e-mail", g_mail, c->email[0][0] || c->email[1][0]);
        y += 70;
    }
    int fw = MIN(460, a.w - 40), fx = cx - fw / 2;
    int y0 = y;
    for (int i = 0; i < 3; i++) y = card_row(u, 22100 + i, fx, y, fw, phone_lbl[i], c->phone[i], 32, "Telefone", T.accent);
    for (int i = 0; i < 2; i++) y = card_row(u, 22110 + i, fx, y, fw, email_lbl[i], c->email[i], 64, "E-mail", T.accent);
    if (y != y0) y += 8;
    /* aniversário */
    if (K.editing || c->by) {
        int lw = 110, tw = text_width(font_get(FONT_REGULAR), 12, "aniversário");
        text_draw(s, font_get(FONT_REGULAR), 12, fx + lw - tw, y + 18, T.text2, "aniversário");
        if (K.editing) {
            bool has = c->by != 0;
            if (ui_switch_ex(u, UI_ID, fx + lw + 12, y + 4, 18, &has, true)) { if (has) { kit_today(&c->by, &c->bm, &c->bd); c->by -= 30; } else c->by = 0; }
            if (c->by) ui_date_field(u, UI_ID, rect_make(fx + lw + 64, y, 130, 26), &c->by, &c->bm, &c->bd);
            y += 32;
        } else {
            char d[48];
            kit_fmt_date(d, sizeof d, c->by, c->bm, c->bd);
            text_draw(s, font_get(FONT_REGULAR), 13.5f, fx + lw + 12, y + 18, T.text, d);
            y += 28;
        }
    }
    y = card_row(u, 22120, fx, y, fw, "endereço", c->street, 96, "Rua e número", 0);
    if (K.editing || c->city[0] || c->zip[0]) {
        if (K.editing) {
            int lw = 110;
            ui_textfield(u, UI_ID, rect_make(fx + lw + 12, y, (fw - lw - 12) * 2 / 3 - 4, 26), c->city, 48, "Cidade");
            ui_textfield(u, UI_ID, rect_make(fx + lw + 12 + (fw - lw - 12) * 2 / 3 + 4, y, (fw - lw - 12) / 3 - 4, 26), c->zip, 16, "CEP");
            y += 32;
        } else {
            char ct[80];
            snprintf(ct, sizeof ct, "%s%s%s", c->city, c->city[0] && c->zip[0] ? "  " : "", c->zip);
            text_draw(s, font_get(FONT_REGULAR), 13.5f, fx + 122, y + 18, T.text, ct);
            y += 28;
        }
    }
    y = card_row(u, 22130, fx, y, fw, "página", c->url, 96, "URL", T.accent);
    y = card_row(u, 22140, fx, y, fw, "notas", c->notes, 256, "Notas", 0);
    if (K.editing) {
        /* grupos */
        int lw = 110, tw = text_width(font_get(FONT_REGULAR), 12, "grupos");
        text_draw(s, font_get(FONT_REGULAR), 12, fx + lw - tw, y + 16, T.text2, "grupos");
        int gx = fx + lw + 12, gy = y;
        for (int g = 0; g < K.ng; g++) {
            bool on = (c->groups >> g) & 1;
            int wdt = text_width(font_get(FONT_REGULAR), 13, K.gname[g]) + 34;
            if (gx + wdt > fx + fw) { gx = fx + lw + 12; gy += 26; }
            if (ui_checkbox(u, 22200 + g, gx, gy + 3, K.gname[g], &on)) c->groups = on ? c->groups | (1u << g) : c->groups & ~(1u << g);
            gx += wdt + 10;
        }
        y = gy + 32;
    }
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, y + K.cscroll - a.y + 20, &K.cscroll);
    /* barra inferior: + / Editar / Compartilhar */
    int by = a.y + a.h - 40;
    gfx_fill(s, a.x, by - 8, a.w, 1, T.separator);
    if (ui_button_icon(u, UI_ID, rect_make(a.x + 12, by, 32, 28), glyph_plus, !K.editing)) new_contact();
    if (K.editing) {
        if (ui_button_style(u, UI_ID, rect_make(a.x + a.w - 12 - 72, by, 72, 28), "OK", BTN_FILLED | BTN_DEFAULT, true, NULL)) { K.editing = false; save(); }
        if (ui_button_style(u, UI_ID, rect_make(a.x + a.w - 12 - 72 - 8 - 90, by, 90, 28), "Cancelar", BTN_GRAY, true, NULL)) {
            *c = K.backup;
            K.editing = false;
            if (!c->first[0] && !c->last[0] && !c->org[0] && !c->phone[0][0] && !c->email[0][0]) { c->used = false; K.sel = -1; }
            save();
        }
    } else {
        if (ui_button_style(u, UI_ID, rect_make(a.x + a.w - 12 - 80, by, 80, 28), "Editar", BTN_GRAY, true, NULL)) { K.backup = *c; K.editing = true; }
        if (ui_button_style(u, UI_ID, rect_make(a.x + a.w - 12 - 80 - 8 - 110, by, 110, 28), "Exportar…", BTN_GRAY, true, NULL)) export_vcf();
    }
}

static void paint(window_t *w, ui_t *u) {
    paint_sidebar(w, u);
    int sb = w->sidebar_w, lw = 250;
    paint_list(w, u, rect_make(sb, w->toolbar_h, lw, u->ch - w->toolbar_h));
    paint_card(w, u, rect_make(sb + lw, w->toolbar_h, u->cw - sb - lw, u->ch - w->toolbar_h));
    /* toolbar */
    if (ui_button_icon(u, UI_ID, rect_make(u->cw - 12 - 32, TOOLBAR_Y(28), 32, 28), glyph_plus, true)) new_contact();
    if (u->key && !u->focus) {
        if ((u->key == KEY_BACKSPACE || u->key == KEY_DELETE) && K.sel >= 0 && !K.editing) { delete_contact(); u->key_used = true; }
        if ((u->key == KEY_UP || u->key == KEY_DOWN) && !K.editing) {
            int n = build_order(), k = -1;
            for (int i = 0; i < n; i++) if (order[i] == K.sel) k = i;
            k = CLAMP(k + (u->key == KEY_UP ? -1 : 1), 0, n - 1);
            if (n) K.sel = order[k];
            u->key_used = true;
        }
    }
    if ((u->mods & MOD_CMD) && u->key == 'n') { new_contact(); u->key_used = true; }
    if ((u->mods & MOD_CMD) && u->key == 'l' && K.sel >= 0) { if (K.editing) save(); else K.backup = K.c[K.sel]; K.editing = !K.editing; u->key_used = true; }
}

/* aniversários do dia: uma notificação por dia */
static void tick(app_t *a) {
    static int last_day = -1;
    static u64 last;
    if (timer_ticks() - last < 5000) return;
    last = timer_ticks();
    int y, m, d;
    kit_today(&y, &m, &d);
    if (d == last_day) return;
    last_day = d;
    load();
    for (int i = 0; i < MAXC; i++) {
        contact_t *c = &K.c[i];
        if (c->used && c->by && c->bm == m && c->bd == d) {
            char nm[100], t[160];
            contact_name(c, nm, sizeof nm);
            snprintf(t, sizeof t, "Hoje é aniversário de %s.", nm);
            notify("Aniversário", t);
        }
    }
}

static void on_close(window_t *w) { save(); K.w = NULL; K.editing = false; }
static void launch(app_t *a) {
    if (K.w) { wm_focus(K.w); return; }
    load();
    K.w = wm_create(&app_contatos, "Contatos", -1, -1, 920, 600, WIN_RESIZABLE);
    if (!K.w) return;
    K.w->paint = paint;
    K.w->on_close = on_close;
    K.w->toolbar_h = TOOLBAR_H;
    K.w->sidebar_w = 210;
    K.w->min_w = 760;
    K.w->min_h = 440;
    K.w->title_x = -1;
    if (K.sel < 0) { int n = build_order(); if (n) K.sel = order[0]; }
}

enum { M_NEW = 1, M_NEWGROUP, M_EDIT, M_DEL, M_EXPORT, M_IMPORT };
static menu_item_t m_file[] = {{"Novo Cartão", "⌘N", M_NEW}, {"Novo Grupo", "⇧⌘N", M_NEWGROUP}, {NULL},
                               {"Exportar vCard…", NULL, M_EXPORT}, {"Importar da Mesa…", NULL, M_IMPORT}};
static menu_item_t m_edit[] = {{"Editar Cartão", "⌘L", M_EDIT}, {"Apagar Cartão", "⌫", M_DEL}};
static menu_t menus[] = {{"Arquivo", m_file, 5}, {"Editar", m_edit, 2}};
static void on_menu(app_t *a, int act) {
    switch (act) {
    case M_NEW: new_contact(); break;
    case M_NEWGROUP: { int g = group_find_or_add("Novo Grupo"); if (g >= 0) { K.group = g; K.renaming = true; snprintf(K.rename_buf, 32, "Novo Grupo"); save(); } } break;
    case M_EDIT: if (K.sel >= 0) { K.backup = K.c[K.sel]; K.editing = true; } break;
    case M_DEL: delete_contact(); break;
    case M_EXPORT: export_vcf(); break;
    case M_IMPORT: {
        /* importa todos os .vcf da Mesa */
        char p[200];
        snprintf(p, sizeof p, "%s/Mesa", CUR_HOME);
        vnode_t *d = vfs_lookup(p);
        int n = 0;
        for (vnode_t *c = d && d->dir ? d->child : NULL; c; c = c->next) {
            if (c->dir || strcasecmp(vfs_ext(c->name), "vcf")) continue;
            char q[300];
            vfs_path(c, q, sizeof q);
            char *b = kit_read(q, NULL);
            if (b) { n += vcard_import(b); kfree(b); }
        }
        save();
        char m[64];
        snprintf(m, sizeof m, "%d contato(s) importado(s) da Mesa.", n);
        notify("Contatos", m);
    } break;
    }
    if (K.w) wm_invalidate(K.w);
}

static void icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(196, 160, 112), RGB(150, 112, 70));
    gfx_round_rect(s, x + 13 * u, y + 10 * u, 40 * u, 44 * u, 4 * u, RGB(250, 246, 238));
    gfx_fill(s, (int)(x + 13 * u), (int)(y + 10 * u), (int)(6 * u), (int)(44 * u), RGB(220, 200, 170));
    gfx_circle(s, x + 35 * u, y + 26 * u, 6.5f * u, RGB(140, 140, 150));
    gfx_ring_arc(s, x + 35 * u, y + 47 * u, 11 * u, 5 * u, PI_F, 2 * PI_F, RGB(140, 140, 150));
}

app_t app_contatos = {.name = "Contatos", .exec = "contatos", .draw_icon = icon, .launch = launch, .tick = tick,
                      .on_menu = on_menu, .menus = menus, .nmenus = 2, .pinned = true, .always_tick = true};

/* aniversariantes do dia (para o Calendário): nomes separados por \n em out; devolve quantos */
int contacts_birthdays_on(int m, int d, char *out, int cap) {
    load();
    int n = 0, k = 0;
    if (out && cap) out[0] = 0;
    for (int i = 0; i < MAXC; i++) {
        contact_t *c = &K.c[i];
        if (!c->used || !c->by || c->bm != m || c->bd != d) continue;
        n++;
        if (!out) continue;
        char nm[100];
        contact_name(c, nm, sizeof nm);
        int l = snprintf(out + k, cap - k, "%s%s", k ? "\n" : "", nm);
        if (l > 0 && k + l < cap) k += l;
    }
    return n;
}
