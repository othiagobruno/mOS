/* mOS — Livros: biblioteca de EPUB e TXT (~/Livros, Documentos, Downloads), capas geradas, leitor paginado
 * em duas colunas com tamanho de fonte, temas (Original, Sépia, Noturno), sumário por capítulos, marcador e
 * posição lembrada. EPUB: ZIP (deflate) + container.xml + OPF (metadados e spine) + XHTML convertido em texto. */
#include "kit.h"

extern app_t app_livros;
#define MAXB 64
#define MAXCH 200

typedef struct { char path[200], title[96], author[96]; bool epub; int pos; u32 color; } book_t;
static struct {
    window_t *w;
    book_t b[MAXB];
    int nb;
    bool loaded;
    int open;                 /* livro aberto (-1 biblioteca) */
    char *text;               /* texto inteiro do livro aberto */
    int tlen;
    int chap_off[MAXCH];
    char chap_name[MAXCH][64];
    int nchap;
    /* paginação */
    int *lines;               /* início de cada linha */
    int nlines, lcap;
    float fsize;
    int lay_w;
    int lines_per_page, page;
    int theme;
    bool toc;
    int scroll, toc_scroll;
} L = {.open = -1, .fsize = 17};

/* ------------------------------------------------------------ ZIP */
static const u8 *zip_find(const u8 *z, usize n, const char *name, u32 *size, u32 *csize, int *method) {
    /* fim do diretório central */
    if (n < 22) return NULL;
    usize eocd = 0;
    for (usize i = n - 22; i > 0 && i + 65557 > n; i--) if (z[i] == 'P' && z[i + 1] == 'K' && z[i + 2] == 5 && z[i + 3] == 6) { eocd = i; break; }
    if (!eocd) return NULL;
    u32 cd = kit_get32(z + eocd + 16);
    u16 cnt = z[eocd + 10] | z[eocd + 11] << 8;
    usize p = cd;
    for (int i = 0; i < cnt && p + 46 <= n; i++) {
        if (kit_get32(z + p) != 0x02014b50) return NULL;
        int meth = z[p + 10] | z[p + 11] << 8;
        u32 cs = kit_get32(z + p + 20), us = kit_get32(z + p + 24);
        int nl = z[p + 28] | z[p + 29] << 8, el = z[p + 30] | z[p + 31] << 8, cl = z[p + 32] | z[p + 33] << 8;
        u32 lho = kit_get32(z + p + 42);
        if ((int)strlen(name) == nl && !memcmp(z + p + 46, name, nl) && lho + 30 <= n) {
            int lnl = z[lho + 26] | z[lho + 27] << 8, lel = z[lho + 28] | z[lho + 29] << 8;
            const u8 *data = z + lho + 30 + lnl + lel;
            if (data + cs > z + n) return NULL;
            *size = us; *csize = cs; *method = meth;
            return data;
        }
        p += 46 + nl + el + cl;
    }
    return NULL;
}
/* arquivo do zip descomprimido (kfree depois), terminado em 0 */
static char *zip_read(const u8 *z, usize n, const char *name, int *len) {
    u32 us, cs;
    int m;
    const u8 *d = zip_find(z, n, name, &us, &cs, &m);
    if (!d || us > 32 * 1024 * 1024) return NULL;
    char *out = kmalloc(us + 1);
    if (!out) return NULL;
    usize got = 0;
    if (m == 0) { memcpy(out, d, MIN(us, cs)); got = MIN(us, cs); }
    else if (m == 8) raw_inflate(d, cs, (u8 *)out, us, &got);
    else { kfree(out); return NULL; }
    out[got] = 0;
    if (len) *len = (int)got;
    return out;
}
/* valor de atributo num trecho de XML */
static bool xml_attr(const char *tag, const char *attr, char *out, int n) {
    char k[40];
    snprintf(k, sizeof k, "%s=\"", attr);
    const char *p = strstr(tag, k);
    const char *end = strchr(tag, '>');
    if (!p || (end && p > end)) return false;
    p += strlen(k);
    int i = 0;
    while (*p && *p != '"' && i < n - 1) out[i++] = *p++;
    out[i] = 0;
    return true;
}
static void xml_text(const char *xml, const char *tagname, char *out, int n) {
    char open[48];
    snprintf(open, sizeof open, "<%s", tagname);
    const char *p = strstr(xml, open);
    out[0] = 0;
    if (!p) return;
    p = strchr(p, '>');
    if (!p) return;
    p++;
    int i = 0;
    while (*p && *p != '<' && i < n - 1) out[i++] = *p++;
    out[i] = 0;
}
static void resolve(const char *base, const char *href, char *out, int n) {
    const char *sl = strrchr(base, '/');
    if (!sl) { snprintf(out, n, "%s", href); return; }
    snprintf(out, n, "%.*s/%s", (int)(sl - base), base, href);
    /* ../ simples */
    char *dd;
    while ((dd = strstr(out, "/../"))) {
        char *prev = dd - 1;
        while (prev > out && *prev != '/') prev--;
        memmove(prev == out ? out : prev + 1, dd + 4, strlen(dd + 4) + 1);
    }
    /* %20 */
    char *q = out, *o = out;
    while (*q) { if (q[0] == '%' && q[1] && q[2]) { *o++ = (char)strtol((char[3]){q[1], q[2], 0}, NULL, 16); q += 3; } else *o++ = *q++; }
    *o = 0;
}
/* XHTML → texto com parágrafos */
static void xhtml_text(const char *in, sbuf_t *o) {
    bool tag = false, skip = false, sp = true;
    for (const char *p = in; *p; p++) {
        if (*p == '<') {
            tag = true;
            if (!strncasecmp(p, "<p", 2) || !strncasecmp(p, "</p", 3) || !strncasecmp(p, "<br", 3) || !strncasecmp(p, "<div", 4) || !strncasecmp(p, "<h", 2) || !strncasecmp(p, "</h", 3) || !strncasecmp(p, "<li", 3)) {
                if (o->len && o->p[o->len - 1] != '\n') sb_putc(o, '\n');
                sp = true;
            }
            if (!strncasecmp(p, "<head", 5) || !strncasecmp(p, "<style", 6) || !strncasecmp(p, "<script", 7)) skip = true;
            if (!strncasecmp(p, "</head", 6) || !strncasecmp(p, "</style", 7) || !strncasecmp(p, "</script", 8)) skip = false;
            continue;
        }
        if (*p == '>') { tag = false; continue; }
        if (tag || skip) continue;
        if (*p == '&') {
            static const struct { const char *e, *r; } ents[] = {{"&nbsp;", " "}, {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&#8212;", "—"}, {"&mdash;", "—"}, {"&#8220;", "“"}, {"&#8221;", "”"}, {"&#8217;", "’"}, {"&#160;", " "}};
            bool done = false;
            for (int i = 0; i < (int)ARRAY_LEN(ents); i++) { int l = (int)strlen(ents[i].e); if (!strncmp(p, ents[i].e, l)) { sb_puts(o, ents[i].r); p += l - 1; done = true; sp = false; break; } }
            if (done) continue;
        }
        if (*p == '\n' || *p == '\r' || *p == '\t' || *p == ' ') { if (!sp) { sb_putc(o, ' '); sp = true; } continue; }
        sb_putc(o, *p);
        sp = false;
    }
}

/* ------------------------------------------------------------ biblioteca */
static void meta_epub(book_t *b) {
    vnode_t *v = vfs_lookup(b->path);
    if (!v) return;
    vfs_ensure(v);
    char *cont = zip_read(v->data, v->size, "META-INF/container.xml", NULL);
    if (!cont) return;
    char opf_path[200] = "";
    char *rf = strstr(cont, "<rootfile");
    if (rf) xml_attr(rf, "full-path", opf_path, sizeof opf_path);
    kfree(cont);
    char *opf = opf_path[0] ? zip_read(v->data, v->size, opf_path, NULL) : NULL;
    if (!opf) return;
    char t[96];
    xml_text(opf, "dc:title", t, sizeof t);
    if (t[0]) snprintf(b->title, 96, "%s", t);
    xml_text(opf, "dc:creator", t, sizeof t);
    if (t[0]) snprintf(b->author, 96, "%s", t);
    kfree(opf);
}
static void scan_dir(const char *dir) {
    vnode_t *d = vfs_lookup(dir);
    for (vnode_t *c = d && d->dir ? d->child : NULL; c && L.nb < MAXB; c = c->next) {
        if (c->dir) continue;
        const char *e = vfs_ext(c->name);
        bool ep = !strcasecmp(e, "epub");
        usize dl = strlen(dir);
        if (!ep && !(strcasecmp(e, "txt") == 0 && dl >= 7 && !strcmp(dir + dl - 7, "/Livros"))) continue;
        book_t *b = &L.b[L.nb++];
        memset(b, 0, sizeof *b);
        vfs_path(c, b->path, sizeof b->path);
        snprintf(b->title, 96, "%.*s", (int)(strlen(c->name) - strlen(e) - 1), c->name);
        snprintf(b->author, 96, "%s", ep ? "Autor desconhecido" : "Texto");
        b->epub = ep;
        b->color = (u32)(rand32() % 6);
        u32 h = 0;
        for (const char *q = c->name; *q; q++) h = h * 31 + (u8)*q;
        b->color = h % 6;
        if (ep) meta_epub(b);
    }
}
static void lib_scan(void) {
    L.nb = 0;
    char d[200];
    snprintf(d, sizeof d, "%s/Livros", CUR_HOME);
    if (!vfs_lookup(d)) {
        vfs_mkdir(d);
        /* livro de exemplo em domínio público */
        const char *t =
            "Dom Casmurro\nMachado de Assis (1899) — trecho, domínio público\n\n"
            "CAPÍTULO PRIMEIRO — DO TÍTULO\n\n"
            "Uma noite destas, vindo da cidade para o Engenho Novo, encontrei no trem da Central um rapaz aqui do bairro, que eu conheço de vista e de chapéu. "
            "Cumprimentou-me, sentou-se ao pé de mim, falou da Lua e dos ministros, e acabou recitando-me versos. A viagem era curta, e os versos pode ser que não fossem inteiramente maus. "
            "Sucedeu, porém, que, como eu estava cansado, fechei os olhos três ou quatro vezes; tanto bastou para que ele interrompesse a leitura e metesse os versos no bolso.\n\n"
            "— Continue, disse eu acordando.\n\n— Já acabei, murmurou ele.\n\n— São muito bonitos.\n\n"
            "Vi-lhe fazer um gesto para tirá-los outra vez do bolso, mas não passou do gesto; estava amuado. No dia seguinte entrou a dizer de mim nomes feios, "
            "e acabou alcunhando-me Dom Casmurro. Os vizinhos, que não gostam dos meus hábitos reclusos e calados, deram curso à alcunha, que afinal pegou.\n\n"
            "CAPÍTULO II — DO LIVRO\n\n"
            "Agora que expliquei o título, passo a escrever o livro. Antes disso, porém, digamos os motivos que me põem a pena na mão. "
            "Vivo só, com um criado. A casa em que moro é própria; fi-la construir de propósito, levado de um desejo tão particular que me vexa imprimi-lo, mas vá lá. "
            "Um dia, há bastantes anos, lembrou-me reproduzir no Engenho Novo a casa em que me criei na antiga Rua de Mata-cavalos, dando-lhe o mesmo aspecto e economia daquela outra, que desapareceu.\n";
        char p[240];
        snprintf(p, sizeof p, "%s/Dom Casmurro (trecho).txt", d);
        vfs_write_file(p, t, strlen(t));
    }
    scan_dir(d);
    snprintf(d, sizeof d, "%s/Documentos", CUR_HOME); scan_dir(d);
    snprintf(d, sizeof d, "%s/Downloads", CUR_HOME); scan_dir(d);
    /* posições salvas */
    char p[200];
    char *buf = kit_read(kit_lib("Livros", "posicoes.tsv", p, sizeof p), NULL);
    if (buf) {
        char *cur = buf, *ln, *f[3];
        while ((ln = kit_line(&cur))) { int n = kit_split(ln, f, 3); if (n >= 2) for (int i = 0; i < L.nb; i++) if (!strcmp(L.b[i].path, f[0])) L.b[i].pos = atoi(f[1]); }
        kfree(buf);
    }
    L.loaded = true;
}
static void pos_save(void) {
    sbuf_t b = {0};
    for (int i = 0; i < L.nb; i++) if (L.b[i].pos) sb_printf(&b, "%s\t%d\n", L.b[i].path, L.b[i].pos);
    char p[200];
    kit_save(kit_lib("Livros", "posicoes.tsv", p, sizeof p), &b);
}

/* ------------------------------------------------------------ abrir livro */
static void close_book(void) {
    if (L.open >= 0 && L.lines && L.nlines) { L.b[L.open].pos = L.lines[MIN(L.nlines - 1, L.page * L.lines_per_page)]; pos_save(); }
    if (L.text) { kfree(L.text); L.text = NULL; }
    if (L.lines) { kfree(L.lines); L.lines = NULL; }
    L.nlines = L.lcap = 0;
    L.open = -1;
}
static void open_book(int i) {
    close_book();
    book_t *b = &L.b[i];
    sbuf_t o = {0};
    L.nchap = 0;
    if (b->epub) {
        vnode_t *v = vfs_lookup(b->path);
        if (v) {
            vfs_ensure(v);
            char *cont = zip_read(v->data, v->size, "META-INF/container.xml", NULL);
            char opf_path[200] = "";
            if (cont) { char *rf = strstr(cont, "<rootfile"); if (rf) xml_attr(rf, "full-path", opf_path, sizeof opf_path); kfree(cont); }
            char *opf = opf_path[0] ? zip_read(v->data, v->size, opf_path, NULL) : NULL;
            if (opf) {
                /* spine: itemref idref → manifest item href */
                char *sp = strstr(opf, "<spine");
                char *p = sp;
                while (p && (p = strstr(p, "<itemref")) && L.nchap < MAXCH) {
                    char idref[96];
                    if (!xml_attr(p, "idref", idref, sizeof idref)) { p += 8; continue; }
                    p += 8;
                    char want[120];
                    snprintf(want, sizeof want, "id=\"%s\"", idref);
                    char *it = strstr(opf, want);
                    while (it && it > opf && *it != '<') it--;
                    char href[200];
                    if (!it || !xml_attr(it, "href", href, sizeof href)) continue;
                    char full[300];
                    resolve(opf_path, href, full, sizeof full);
                    char *x = zip_read(v->data, v->size, full, NULL);
                    if (!x) continue;
                    char title[64] = "";
                    xml_text(x, "title", title, sizeof title);
                    if (!title[0]) { char *h = strstr(x, "<h1"); if (!h) h = strstr(x, "<h2"); if (h) { h = strchr(h, '>'); if (h) { int k = 0; h++; while (*h && *h != '<' && k < 63) title[k++] = *h++; title[k] = 0; } } }
                    L.chap_off[L.nchap] = o.len;
                    snprintf(L.chap_name[L.nchap], 64, "%s", title[0] ? title : "Seção");
                    snprintf(L.chap_name[L.nchap], 64, "%s", title[0] ? title : "Seção");
                    L.nchap++;
                    xhtml_text(x, &o);
                    sb_puts(&o, "\n\n");
                    kfree(x);
                }
                kfree(opf);
            }
        }
        if (!o.len) sb_puts(&o, "Não foi possível ler este EPUB.");
    } else {
        char *t = kit_read(b->path, NULL);
        sb_puts(&o, t ? t : "");
        if (t) kfree(t);
        /* capítulos: linhas em maiúsculas que começam com "CAPÍTULO" */
        char *p = o.p;
        while (p && (p = strstr(p, "CAPÍTULO")) && L.nchap < MAXCH) {
            if (p == o.p || p[-1] == '\n') {
                L.chap_off[L.nchap] = (int)(p - o.p);
                int k = 0;
                while (p[k] && p[k] != '\n' && k < 63) { L.chap_name[L.nchap][k] = p[k]; k++; }
                L.chap_name[L.nchap][k] = 0;
                L.nchap++;
            }
            p += 8;
        }
    }
    L.text = o.p ? o.p : strdup("");
    L.tlen = o.len;
    L.open = i;
    L.lay_w = 0;
    L.page = 0;
}

/* quebra de linhas para a largura da coluna */
static void layout(int colw) {
    font_t *f = font_get(FONT_REGULAR);
    L.nlines = 0;
    int i = 0;
    while (i < L.tlen) {
        if (L.nlines + 2 >= L.lcap) { L.lcap = MAX(1024, L.lcap * 2); L.lines = krealloc(L.lines, sizeof(int) * L.lcap); }
        L.lines[L.nlines++] = i;
        int ls = i, last_sp = -1, j = i;
        while (j < L.tlen && L.text[j] != '\n') {
            int nj = utf8_next(L.text, j);
            if (L.text[j] == ' ') last_sp = j;
            if (text_width_n(f, L.fsize, L.text + ls, nj - ls) > colw && j > ls) { if (last_sp > ls) j = last_sp + 1; break; }
            j = nj;
        }
        i = (j < L.tlen && L.text[j] == '\n') ? j + 1 : j;
        if (i == ls) i++;
    }
    L.lay_w = colw;
}

static void themes(u32 *bg, u32 *fg) {
    if (L.theme == 1) { *bg = RGB(248, 240, 222); *fg = RGB(70, 54, 36); }
    else if (L.theme == 2) { *bg = RGB(24, 24, 26); *fg = RGB(200, 200, 204); }
    else { *bg = T.dark ? RGB(30, 30, 32) : RGB(255, 255, 255); *fg = T.text; }
}

static void cover(surface_t *s, rect_t r, book_t *b) {
    static const u32 c1[6] = {RGB(180, 60, 50), RGB(40, 90, 160), RGB(50, 130, 90), RGB(150, 90, 40), RGB(110, 60, 140), RGB(60, 60, 70)};
    gfx_shadow(s, r.x, r.y, r.w, r.h, 4, 10, 60, 4);
    gfx_round_rect_grad(s, r.x, r.y, r.w, r.h, 4, color_lighten(c1[b->color], 20), c1[b->color]);
    gfx_fill(s, r.x + 6, r.y, 2, r.h, RGBA(0, 0, 0, 40));
    rect_t tr = rect_make(r.x + 14, r.y + 18, r.w - 24, r.h / 2);
    text_wrap(s, font_get(FONT_BOLD), MAX(11.0f, r.w / 9.0f), tr, (int)(r.w / 7.5f), RGB(255, 255, 255), b->title, true);
    text_wrap(s, font_get(FONT_REGULAR), MAX(9.0f, r.w / 13.0f), rect_make(r.x + 14, r.y + r.h - 42, r.w - 24, 40), (int)(r.w / 10.0f), RGBA(255, 255, 255, 210), b->author, true);
}

static void paint_library(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    gfx_fill(s, 0, w->toolbar_h, u->cw, u->ch - w->toolbar_h, T.content_bg);
    ui_label(u, 30, w->toolbar_h + 18, "Biblioteca", FONT_BOLD, 26, T.text);
    char sub[80];
    snprintf(sub, sizeof sub, "%d livro%s · coloque arquivos .epub em Livros, Documentos ou Downloads", L.nb, L.nb == 1 ? "" : "s");
    ui_label(u, 30, w->toolbar_h + 52, sub, FONT_REGULAR, 12.5f, T.text2);
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 100, TOOLBAR_Y(28), 100, 28), "Atualizar", true)) lib_scan();
    int cw = 150, chh = 220, gap = 28, x0 = 30, y0 = w->toolbar_h + 88;
    int cols = MAX(1, (u->cw - 60 + gap) / (cw + gap));
    rect_t view = rect_make(0, y0 - 10, u->cw, u->ch - y0 + 10);
    rect_t pc = surface_push_clip(s, view);
    for (int i = 0; i < L.nb; i++) {
        rect_t r = rect_make(x0 + (i % cols) * (cw + gap), y0 + (i / cols) * (chh + 70) - L.scroll, cw, chh);
        cover(s, r, &L.b[i]);
        text_draw_ellipsis(s, font_get(FONT_SEMIBOLD), 13, r.x, r.y + chh + 20, cw, T.text, L.b[i].title);
        char pr[32];
        snprintf(pr, sizeof pr, "%s", L.b[i].pos ? "Continuar lendo" : "Novo");
        text_draw(s, font_get(FONT_REGULAR), 11.5f, r.x, r.y + chh + 38, L.b[i].pos ? T.accent : T.text2, pr);
        if (ui_clicked(u, 127000 + i, r)) open_book(i);
    }
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, (L.nb + cols - 1) / cols * (chh + 70) + 30, &L.scroll);
}

static void paint_reader(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    u32 bg, fg;
    themes(&bg, &fg);
    gfx_fill(s, 0, w->toolbar_h, u->cw, u->ch - w->toolbar_h, bg);
    book_t *b = &L.b[L.open];
    if (ui_button_style(u, UI_ID, rect_make(96, TOOLBAR_Y(28), 100, 28), "Biblioteca", BTN_GRAY, true, NULL)) { close_book(); return; }
    if (ui_toolbar_button(u, UI_ID, rect_make(204, TOOLBAR_Y(28), 80, 28), "Sumário", L.nchap > 0)) L.toc = !L.toc;
    static const char *th[3] = {"Original", "Sépia", "Noturno"};
    L.theme = ui_segmented(u, UI_ID, rect_make(u->cw - 12 - 240, TOOLBAR_Y(28), 240, 28), th, 3, L.theme);
    float fs = L.fsize;
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 240 - 8 - 36, TOOLBAR_Y(28), 36, 28), "A+", L.fsize < 30)) L.fsize += 1.5f;
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 240 - 8 - 36 - 4 - 36, TOOLBAR_Y(28), 36, 28), "A−", L.fsize > 11)) L.fsize -= 1.5f;
    /* duas colunas como um livro aberto */
    int margin = 60, top = w->toolbar_h + 50, bottom = 50;
    bool two = u->cw > 900;
    int colgap = 70;
    int colw = two ? (u->cw - 2 * margin - colgap) / 2 : u->cw - 2 * margin;
    int lh = (int)(L.fsize * 1.55f);
    int ph = u->ch - top - bottom;
    int lpp = MAX(1, ph / lh);
    int keep = L.nlines ? L.lines[MIN(L.nlines - 1, L.page * L.lines_per_page * (two ? 2 : 1))] : b->pos;
    if (L.lay_w != colw || fs != L.fsize) {
        layout(colw);
        /* volta à mesma posição do texto */
        L.lines_per_page = lpp;
        int ln = 0;
        while (ln + 1 < L.nlines && L.lines[ln + 1] <= keep) ln++;
        L.page = ln / (lpp * (two ? 2 : 1));
    }
    L.lines_per_page = lpp;
    int per_view = lpp * (two ? 2 : 1);
    int pages = MAX(1, (L.nlines + per_view - 1) / per_view);
    L.page = CLAMP(L.page, 0, pages - 1);
    ui_label_center(u, rect_make(0, w->toolbar_h + 14, u->cw, 18), b->title, FONT_SEMIBOLD, 12.5f, color_alpha(fg, 150));
    font_t *f = font_get(FONT_REGULAR);
    for (int c = 0; c < (two ? 2 : 1); c++) {
        int x = margin + c * (colw + colgap);
        for (int k = 0; k < lpp; k++) {
            int li = L.page * per_view + c * lpp + k;
            if (li >= L.nlines) break;
            int st = L.lines[li], en = li + 1 < L.nlines ? L.lines[li + 1] : L.tlen;
            while (en > st && (L.text[en - 1] == '\n' || L.text[en - 1] == ' ')) en--;
            text_draw_n(s, f, L.fsize, x, top + k * lh + (int)L.fsize, fg, L.text + st, en - st);
        }
    }
    if (two) gfx_fill(s, u->cw / 2, top, 1, ph, color_alpha(fg, 30));
    /* rodapé: progresso */
    char pg[48];
    snprintf(pg, sizeof pg, "%d de %d", L.page + 1, pages);
    ui_label_center(u, rect_make(0, u->ch - 34, u->cw, 16), pg, FONT_REGULAR, 12, color_alpha(fg, 150));
    gfx_fill(s, margin, u->ch - 12, u->cw - 2 * margin, 2, color_alpha(fg, 30));
    gfx_fill(s, margin, u->ch - 12, (u->cw - 2 * margin) * (L.page + 1) / pages, 2, T.accent);
    /* virar páginas: setas, espaço ou clique nas margens */
    rect_t lft = rect_make(0, top, margin, ph), rgt = rect_make(u->cw - margin, top, margin, ph);
    if (ui_clicked(u, UI_ID, lft) || (u->key == KEY_LEFT && !u->focus)) { L.page = MAX(0, L.page - 1); u->key_used = true; }
    if (ui_clicked(u, UI_ID, rgt) || ((u->key == KEY_RIGHT || u->key == ' ') && !u->focus)) { L.page = MIN(pages - 1, L.page + 1); u->key_used = true; }
    if (ui_hit(u, lft) || ui_hit(u, rgt)) u->cursor = CURSOR_HAND;
    if (u->wheel) { L.page = CLAMP(L.page - (u->wheel > 0 ? 1 : -1), 0, pages - 1); u->wheel = 0; }
    b->pos = L.lines && L.nlines ? L.lines[MIN(L.nlines - 1, L.page * per_view)] : 0;
    /* sumário */
    if (L.toc) {
        rect_t p = rect_make(204, w->toolbar_h + 4, 300, MIN(u->ch - w->toolbar_h - 20, 40 + L.nchap * 30));
        glass_panel(s, p.x, p.y, p.w, p.h, 14, GLASS_REGULAR);
        u->modal_next = rect_make(0, 0, u->cw, u->ch);
        u->in_overlay = true;
        rect_t lv = rect_make(p.x + 6, p.y + 8, p.w - 12, p.h - 16);
        rect_t pc = surface_push_clip(s, lv);
        for (int i = 0; i < L.nchap; i++) {
            rect_t r = rect_make(lv.x, lv.y + i * 30 - L.toc_scroll, lv.w, 28);
            if (ui_hit(u, r)) gfx_round_rect(s, r.x, r.y, r.w, r.h, 6, T.glass_sel);
            text_draw_ellipsis(s, font_get(FONT_REGULAR), 13, r.x + 10, r.y + 19, r.w - 20, T.glass_text, L.chap_name[i]);
            if (ui_clicked(u, 127500 + i, r)) {
                int ln = 0;
                while (ln + 1 < L.nlines && L.lines[ln + 1] <= L.chap_off[i]) ln++;
                L.page = ln / per_view;
                L.toc = false;
            }
        }
        surface_pop_clip(s, pc);
        ui_scroll(u, UI_ID, lv, L.nchap * 30, &L.toc_scroll);
        if (u->pressed && !rect_contains(p, u->mx, u->my)) L.toc = false;
        u->in_overlay = false;
    }
}

static void paint(window_t *w, ui_t *u) {
    if (!L.loaded) lib_scan();
    if (L.open >= 0) paint_reader(w, u); else paint_library(w, u);
}
static void on_close(window_t *w) { close_book(); L.w = NULL; }
static void launch(app_t *a) {
    if (L.w) { wm_focus(L.w); return; }
    lib_scan();
    L.w = wm_create(&app_livros, "Livros", -1, -1, 1060, 700, WIN_RESIZABLE);
    if (!L.w) return;
    L.w->paint = paint;
    L.w->on_close = on_close;
    L.w->toolbar_h = TOOLBAR_H;
    L.w->min_w = 600;
    L.w->min_h = 460;
}
/* abrir um .epub pelo Arquivos */
void books_open_file(const char *path) {
    app_launch(&app_livros);
    if (!L.loaded) lib_scan();
    for (int i = 0; i < L.nb; i++) if (!strcmp(L.b[i].path, path)) { open_book(i); return; }
    if (L.nb < MAXB) {
        book_t *b = &L.b[L.nb++];
        memset(b, 0, sizeof *b);
        snprintf(b->path, 200, "%s", path);
        snprintf(b->title, 96, "%s", strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
        b->epub = true;
        meta_epub(b);
        open_book(L.nb - 1);
    }
}
static void icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(255, 160, 60), RGB(240, 110, 20));
    float l[] = {x + 12 * u, y + 20 * u, x + 31 * u, y + 24 * u, x + 31 * u, y + 48 * u, x + 12 * u, y + 44 * u};
    float r[] = {x + 33 * u, y + 24 * u, x + 52 * u, y + 20 * u, x + 52 * u, y + 44 * u, x + 33 * u, y + 48 * u};
    gfx_polygon(s, l, 4, RGB(255, 255, 255));
    gfx_polygon(s, r, 4, RGB(255, 255, 255));
}
app_t app_livros = {.name = "Livros", .exec = "livros", .draw_icon = icon, .launch = launch};
