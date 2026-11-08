/* mOS — Horizonte: navegador web (interface estilo Safari/Mojave, abas, histórico, formulários).
 * Com o Chromium instalado como motor (launchsvc.h, HORIZONTE_MOTOR), o Horizonte abre as páginas nele; o motor deste
 * arquivo fica de reserva. docs/navegador-chromium.md */
#include "apps.h"
#include "web.h"
#include "js.h"
#include "launchsvc.h"

#define MAX_TABS 12
#define HIST     48
#define ADDR_ID  9001
#define TOOLBAR  38
#define TABBAR   26
#define SEARCH_URL "http://lite.duckduckgo.com/lite/?q="

enum { M_NEWWIN = 1, M_NEWTAB, M_OPENLOC, M_CLOSETAB, M_RELOAD, M_ZOOMIN, M_ZOOMOUT, M_ZOOM0, M_BACK, M_FWD, M_HOME, M_HELP,
       M_JS, M_JSCON,
       M_FAV0 = 50 };

typedef struct { char url[1024]; int scroll; } hent_t;

typedef struct tab {
    char url[1024];
    char title[160];
    doc_t *doc;
    hent_t hist[HIST];
    int nhist, hpos;
    /* carregamento */
    http_req_t *req;
    char req_url[1024];
    bool req_record;
    int req_scroll;
    u64 load_t0;
    int scroll;
    float zoom;
    char frag[160];
    bool about;
    bool about_dark;
    bool img_dirty;
    u64 img_dirty_t;
    /* folhas de estilo externas (<link rel=stylesheet>) */
    struct { http_req_t *req; node_t *n; } css[8];
    int ncss;
    u64 css_t0;
    js_page_t *js;           /* motor JavaScript da página (NULL se desativado) */
} tab_t;

static void css_cancel(tab_t *t) {
    for (int i = 0; i < t->ncss; i++) if (t->css[i].req) http_free(t->css[i].req);
    t->ncss = 0;
}

static void css_set_text(doc_t *d, node_t *n, const u8 *data, usize len) {
    usize ul;
    char *u = charset_to_utf8(data, len, NULL, &ul);
    if (!u) return;
    n->text = arena_strndup(&d->arena, u, (int)ul);
    n->len = (int)ul;
    kfree(u);
}

static void css_find(tab_t *t, node_t *r) {
    for (node_t *c = r->first; c && t->ncss < (int)ARRAY_LEN(t->css); c = c->next) {
        if (c->type != N_ELEM) continue;
        if (c->tag == T_LINK) {
            const char *rel = node_attr(c, "rel"), *href = node_attr(c, "href");
            if (!rel || !href || !*href || strstr(rel, "alternate")) continue;
            char low[64];
            int k = 0;
            for (; rel[k] && k < 63; k++) low[k] = (char)tolower(rel[k]);
            low[k] = 0;
            if (!strstr(low, "stylesheet")) continue;
            char url[1024];
            url_resolve(t->doc->base, href, url, sizeof url);
            if (!strncmp(url, "file:", 5)) {
                char path[512];
                url_decode_path(url + 7, path, sizeof path);
                vnode_t *v = vfs_lookup(path);
                if (v && !v->dir) css_set_text(t->doc, c, v->data, v->size);
            } else if (!strncmp(url, "http", 4)) {
                http_req_t *rq = http_get(url);
                if (rq) { t->css[t->ncss].req = rq; t->css[t->ncss].n = c; t->ncss++; }
            }
        } else if (c->tag != T_SVG && c->tag != T_SCRIPT && c->tag != T_STYLE) css_find(t, c);
    }
}

/* inicia o download das folhas externas do documento recém-carregado */
static void css_start(tab_t *t) {
    css_cancel(t);
    if (!t->doc) return;
    css_find(t, t->doc->root);
    t->css_t0 = timer_ticks();
    /* folhas locais já estão prontas */
    if (t->doc->sheet) { css_free(t->doc->sheet); t->doc->sheet = NULL; }
    t->doc->need_style = true;
}

/* devolve true quando alguma folha terminou e o estilo precisa ser refeito */
static bool css_pump(tab_t *t) {
    if (!t->ncss || !t->doc) return false;
    bool all = true;
    for (int i = 0; i < t->ncss; i++) {
        if (!t->css[i].req) continue;
        int st = http_poll(t->css[i].req);
        if (st == 0) { all = false; continue; }
        http_req_t *r = t->css[i].req;
        if (st > 0 && r->body && r->body_len && r->status < 400) css_set_text(t->doc, t->css[i].n, r->body, r->body_len);
        http_free(r);
        t->css[i].req = NULL;
    }
    if (!all && timer_ticks() - t->css_t0 < 10000) return false;
    css_cancel(t);
    doc_t *d = t->doc;
    if (d->sheet) { css_free(d->sheet); d->sheet = NULL; }
    d->need_style = true;
    return true;
}

typedef struct bwin {
    window_t *w;
    tab_t *tabs[MAX_TABS];
    int ntabs, cur;
    char addr[1024];
    char status[600];
    node_t *hover_link;
    node_t *press_link;
    /* ações adiadas para depois da pintura */
    char nav_url[1024];
    int nav_kind;            /* 1 navegar, 2 nova aba */
    node_t *submit_form;
    ctrl_t *submitter;
    int close_tab;
    /* cache da página renderizada (evita redesenhar o documento a cada tecla/movimento) */
    surface_t *cache;
    doc_t *c_doc;
    u64 c_gen;
    int c_scroll;
    node_t *c_hover;
    bool c_dark;
    struct { ctrl_t *c; rect_t r; } rec[128];
    int nrec;
    /* eventos para o JavaScript, despachados depois da pintura dos controles */
    ctrl_t *js_clicked;
    struct { ctrl_t *c; int type; } jsq[16];
    int njsq;
    bool press_in_view;
    struct bwin *next;
} bwin_t;

static bwin_t *bwins;
static rect_t g_view;        /* área da página durante a pintura */
static bwin_t *g_paint_b;

static void tab_navigate(bwin_t *b, tab_t *t, const char *url, bool record);

/* ============================================================ páginas internas */
static void about_colors(const char **bg, const char **fg, const char **sub, const char **card, const char **line) {
    if (T.dark) { *bg = "#1f1f21"; *fg = "#ececec"; *sub = "#9a9aa0"; *card = "#2c2c2e"; *line = "#3a3a3c"; }
    else { *bg = "#f5f5f7"; *fg = "#1d1d1f"; *sub = "#6e6e73"; *card = "#ffffff"; *line = "#e1e1e6"; }
}

typedef struct { const char *name, *url, *letter, *color; } fav_t;
static const fav_t favs[] = {
    {"Wikipédia", "https://pt.m.wikipedia.org/", "W", "#5f6368"},
    {"DuckDuckGo Lite", "http://lite.duckduckgo.com/lite/", "D", "#de5833"},
    {"Hacker News", "https://news.ycombinator.com/", "Y", "#ff6600"},
    {"example.com", "http://example.com/", "E", "#5e5ce6"},
    {"NeverSSL", "http://neverssl.com/", "N", "#30a14e"},
    {"Primeiro site", "http://info.cern.ch/hypertext/WWW/TheProject.html", "C", "#0a84ff"},
    {"NPR Texto", "https://text.npr.org/", "n", "#d62021"},
    {"CNN Lite", "https://lite.cnn.com/", "C", "#b31010"},
    {"68k.news", "http://68k.news/", "6", "#8e44ad"},
    {"FrogFind", "http://frogfind.com/", "F", "#2ba84a"},
    {"Exemplo local", "file:///Users/Visitante/Documentos/Exemplo.html", "\xE2\x97\x86", "#0fa3b1"},
    {"Teste de JavaScript", "file:///Users/Visitante/Documentos/js.html", "JS", "#e0a800"},
    {"Ajuda do mOS", "about:ajuda", "?", "#ff9f0a"},
};

static void gen_inicio(sbuf_t *s) {
    const char *bg, *fg, *sub, *card, *line;
    about_colors(&bg, &fg, &sub, &card, &line);
    sb_printf(s, "<!DOCTYPE html><html><head><meta charset=utf-8><title>Favoritos</title><style>"
                 "body{margin:0;background:%s;color:%s;font-family:sans-serif}"
                 ".wrap{max-width:780px;margin:0 auto;padding:46px 30px 40px}"
                 "h1{font-size:26px;font-weight:bold;margin:0 0 4px}"
                 ".sub{color:%s;font-size:14px;margin:0 0 26px}"
                 ".grid{text-align:left;margin:0 -8px}"
                 "a.t{display:inline-block;width:106px;margin:0 8px 18px;text-align:center;text-decoration:none;color:%s;vertical-align:top}"
                 ".ic{display:block;width:72px;height:72px;margin:0 auto 8px;border-radius:16px;color:#ffffff;font-size:34px;font-weight:bold;line-height:72px;box-shadow:0 2px 6px rgba(0,0,0,.25)}"
                 ".lb{display:block;font-size:12px;line-height:1.3}"
                 "h2{font-size:15px;font-weight:600;margin:26px 0 10px;color:%s}"
                 ".card{background:%s;border-radius:12px;padding:16px 18px;border:1px solid %s}"
                 ".card p{margin:0 0 8px;font-size:13px;line-height:1.5;color:%s}"
                 ".card a{color:#0a84ff;text-decoration:none}"
                 "form{margin:0 0 30px}"
                 ".foot{margin-top:34px;font-size:12px;color:%s;text-align:center}"
                 "</style></head><body><div class=wrap>",
              bg, fg, sub, fg, fg, card, line, sub, sub);
    sb_printf(s, "<h1>Favoritos</h1><p class=sub>Bem-vindo ao Horizonte, o navegador do mOS.</p>"
                 "<form action=\"" SEARCH_URL "\" method=get><input type=search name=q size=48 placeholder=\"Pesquisar com DuckDuckGo\"> "
                 "<input type=submit value=\"Buscar\"></form><div class=grid>");
    for (u32 i = 0; i < ARRAY_LEN(favs); i++)
        sb_printf(s, "<a class=t href=\"%s\"><span class=ic style=\"background:%s\">%s</span><span class=lb>%s</span></a>",
                  favs[i].url, favs[i].color, favs[i].letter, favs[i].name);
    sb_printf(s, "</div><h2>Dicas</h2><div class=card>"
                 "<p>Digite um endereço ou palavras na barra no topo e tecle <b>Return</b>. Palavras soltas viram uma busca no DuckDuckGo.</p>"
                 "<p><b>⌘T</b> abre uma nova aba, <b>⌘L</b> vai para a barra de endereços, <b>⌘[</b> e <b>⌘]</b> voltam e avançam, "
                 "<b>⌘+</b> e <b>⌘−</b> ajustam o zoom.</p>"
                 "<p>O Horizonte executa JavaScript com o motor Brisa (desative em Visualizar &rsaquo; Ativar JavaScript); sites leves funcionam melhor. "
                 "Arquivos locais abrem com <a href=\"file:///Users/Visitante/\">file:///Users/Visitante/</a>.</p></div>"
                 "<p class=foot>Horizonte 1.0 &middot; motor web do mOS escrito do zero</p></div></body></html>");
}

static void gen_ajuda(sbuf_t *s) {
    const char *bg, *fg, *sub, *card, *line;
    about_colors(&bg, &fg, &sub, &card, &line);
    sb_printf(s, "<!DOCTYPE html><html><head><meta charset=utf-8><title>Ajuda do mOS</title><style>"
                 "body{margin:0;background:%s;color:%s;font-family:sans-serif;line-height:1.5}"
                 ".wrap{max-width:760px;margin:0 auto;padding:40px 30px 50px}"
                 ".hero{background:linear-gradient(#3d8bfd,#1d5fd0);color:white;border-radius:14px;padding:22px 26px;margin-bottom:26px}"
                 ".hero h1{font-size:28px;margin:0 0 4px;font-weight:bold}.hero p{margin:0;font-size:14px;color:#e8f0ff}"
                 "h2{font-size:18px;font-weight:600;margin:28px 0 10px;padding-bottom:6px;border-bottom:1px solid %s}"
                 "p,li{font-size:14px}ul{padding-left:22px;margin:8px 0}li{margin:0 0 4px}"
                 "table{border-collapse:collapse;width:100%%;font-size:13px;background:%s;border-radius:10px}"
                 "td,th{padding:7px 12px;border-bottom:1px solid %s;text-align:left;vertical-align:top}"
                 "th{color:%s;font-weight:600;font-size:12px}"
                 "kbd{font-family:monospace;background:%s;border:1px solid %s;border-radius:4px;padding:0 5px;font-size:12px}"
                 "a{color:#0a84ff;text-decoration:none}.note{color:%s;font-size:13px}"
                 "</style></head><body><div class=wrap>",
              bg, fg, line, card, line, sub, card, line, sub);
    sb_puts(s, "<div class=hero><h1>Ajuda do mOS</h1><p>Um sistema operacional de brinquedo, feito do zero, com cara de Mojave.</p></div>"
               "<h2>Primeiros passos</h2><ul>"
               "<li>O <b>Dock</b>, embaixo, abre os aplicativos. Um ponto sob o ícone indica que o app está aberto.</li>"
               "<li>A <b>barra de menus</b> mostra os menus do app em primeiro plano. O logotipo <b>m</b> traz Sobre, Preferências, aparência clara/escura, reiniciar e desligar.</li>"
               "<li>Use <kbd>⌘ Espaço</kbd> para a <b>Busca</b>: encontre apps, arquivos e faça contas rápidas.</li>"
               "<li>Arraste janelas pela barra de título; redimensione pelas bordas; os botões coloridos fecham, minimizam e ampliam.</li></ul>"
               "<h2>Aplicativos</h2><table><tr><th>App</th><th>Para que serve</th></tr>"
               "<tr><td><b>Arquivos</b></td><td>Navega pelas pastas (Documentos, Imagens, Downloads...), cria pastas e move itens para a Lixeira.</td></tr>"
               "<tr><td><b>Horizonte</b></td><td>Navegador web com abas, histórico, formulários, imagens PNG/JPEG/GIF e páginas locais.</td></tr>"
               "<tr><td><b>Terminal</b></td><td>Shell com comandos como <kbd>ls</kbd>, <kbd>cd</kbd>, <kbd>cat</kbd>, <kbd>ping</kbd> e <kbd>help</kbd>.</td></tr>"
               "<tr><td><b>Notas</b> e <b>Editor de Texto</b></td><td>Anotações rápidas e edição de arquivos de texto.</td></tr>"
               "<tr><td><b>Calculadora</b> e <b>Calendário</b></td><td>Contas do dia a dia e o mês atual.</td></tr>"
               "<tr><td><b>Música</b></td><td>Sintetizador com piano e melodias clássicas.</td></tr>"
               "<tr><td><b>Campo Minado</b></td><td>O jogo clássico.</td></tr>"
               "<tr><td><b>Visualizador</b></td><td>Abre imagens PNG, JPEG, GIF, BMP e PPM.</td></tr>"
               "<tr><td><b>Monitor de Atividade</b> e <b>Console</b></td><td>Uso de CPU/memória e mensagens do sistema.</td></tr>"
               "<tr><td><b>Preferências do Sistema</b></td><td>Aparência, cor de destaque, Dock, som, rede, data e hora.</td></tr></table>"
               "<h2>Atalhos do sistema</h2><table><tr><th>Teclas</th><th>Ação</th></tr>"
               "<tr><td><kbd>⌘ Espaço</kbd></td><td>Abrir a Busca</td></tr>"
               "<tr><td><kbd>⌘ Q</kbd></td><td>Encerrar o aplicativo</td></tr>"
               "<tr><td><kbd>⌘ W</kbd> / <kbd>⌘ M</kbd> / <kbd>⌘ H</kbd></td><td>Fechar, minimizar ou ocultar janelas</td></tr>"
               "<tr><td><kbd>⌘ Tab</kbd></td><td>Alternar entre aplicativos</td></tr>"
               "<tr><td><kbd>⌘ ,</kbd></td><td>Preferências</td></tr>"
               "<tr><td><kbd>⌥ ⌘ ⎋</kbd></td><td>Forçar encerramento</td></tr></table>"
               "<h2>Atalhos do Horizonte</h2><table><tr><th>Teclas</th><th>Ação</th></tr>"
               "<tr><td><kbd>⌘ T</kbd> / <kbd>⌘ W</kbd></td><td>Nova aba / fechar aba</td></tr>"
               "<tr><td><kbd>⌘ L</kbd></td><td>Ir para a barra de endereços</td></tr>"
               "<tr><td><kbd>⌘ R</kbd></td><td>Recarregar a página</td></tr>"
               "<tr><td><kbd>⌘ [</kbd> / <kbd>⌘ ]</kbd></td><td>Voltar / avançar</td></tr>"
               "<tr><td><kbd>⌘ +</kbd> / <kbd>⌘ −</kbd> / <kbd>⌘ 0</kbd></td><td>Zoom maior, menor e tamanho real</td></tr>"
               "<tr><td><kbd>Espaço</kbd>, <kbd>PgUp</kbd>, <kbd>PgDn</kbd>, setas</td><td>Rolar a página</td></tr></table>"
               "<p class=note>No mOS, a tecla <kbd>⌘</kbd> é a tecla Windows; <kbd>Ctrl</kbd> também funciona no Horizonte.</p>"
               "<h2>Sobre o Horizonte</h2><p>O motor web foi escrito do zero: tokenizador HTML5 tolerante, CSS com cascata e seletores, "
               "layout de blocos, linhas e tabelas, e decodificadores próprios de PNG (com inflate), JPEG e GIF. "
               "O motor JavaScript Brisa (interpretador, coletor de lixo, DOM e eventos) também foi escrito do zero; "
               "quando ele está desativado, o conteúdo de <code>&lt;noscript&gt;</code> é exibido.</p>"
               "<p>Experimente o <a href=\"file:///Users/Visitante/Documentos/Exemplo.html\">documento de exemplo</a> ou volte aos "
               "<a href=\"about:inicio\">Favoritos</a>.</p></div></body></html>");
}

static void gen_error(sbuf_t *s, const char *url, const char *msg, const char *detail) {
    const char *bg, *fg, *sub, *card, *line;
    about_colors(&bg, &fg, &sub, &card, &line);
    sb_printf(s, "<!DOCTYPE html><html><head><meta charset=utf-8><title>Falha ao abrir a página</title><style>"
                 "body{margin:0;background:%s;color:%s;font-family:sans-serif;text-align:center}"
                 ".box{max-width:560px;margin:0 auto;padding:90px 24px 40px}"
                 ".ic{display:block;width:76px;height:76px;margin:0 auto 24px;border-radius:38px;background:#ff9f0a;color:#ffffff;font-size:46px;font-weight:bold;line-height:76px}"
                 "h1{font-size:23px;font-weight:600;margin:0 0 12px}"
                 "p{color:%s;font-size:14px;line-height:1.5;margin:0 0 6px}"
                 ".url{font-size:12px;color:%s;margin-top:10px}"
                 "a.btn{display:inline-block;margin-top:24px;padding:7px 20px;border-radius:7px;background:#0a84ff;color:#ffffff;text-decoration:none;font-size:14px;font-weight:500}"
                 "</style></head><body><div class=box><span class=ic>!</span><h1>O Horizonte não consegue abrir a página</h1><p>",
              bg, fg, sub, sub);
    sb_html(s, msg);
    sb_puts(s, "</p>");
    if (detail && *detail) { sb_puts(s, "<p>Detalhes: "); sb_html(s, detail); sb_puts(s, "</p>"); }
    sb_puts(s, "<p class=url>");
    sb_html(s, url);
    sb_puts(s, "</p><a class=btn href=\"");
    sb_html(s, url);
    sb_puts(s, "\">Tentar Novamente</a></div></body></html>");
}

/* ============================================================ utilidades */
static int b64v(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}

/* decodifica uma URL data:; devolve bytes (kmalloc) e o tipo MIME */
static u8 *data_url(const char *url, usize *len, char *mime, int mcap) {
    const char *p = url + 5;
    const char *comma = strchr(p, ',');
    if (!comma) return NULL;
    bool b64 = false;
    int k = 0;
    for (const char *q = p; q < comma && k < mcap - 1; q++) {
        if (*q == ';') { if (!strncmp(q, ";base64", 7)) b64 = true; break; }
        mime[k++] = (char)tolower(*q);
    }
    mime[k] = 0;
    if (!k) strcpy(mime, "text/plain");
    const char *s = comma + 1;
    usize n = strlen(s);
    u8 *out = kmalloc(n + 1);
    if (!out) return NULL;
    usize o = 0;
    if (b64) {
        u32 acc = 0;
        int bits = 0;
        for (usize i = 0; i < n; i++) {
            int v = b64v(s[i]);
            if (s[i] == '%' && i + 2 < n) { i += 2; continue; }
            if (v < 0) continue;
            acc = acc << 6 | (u32)v;
            bits += 6;
            if (bits >= 8) { bits -= 8; out[o++] = (u8)(acc >> bits); }
        }
    } else {
        char tmp[2048];
        url_decode_path(s, tmp, sizeof tmp);
        o = strlen(tmp);
        memcpy(out, tmp, o);
    }
    out[o] = 0;
    *len = o;
    return out;
}

static bool is_image_name(const char *n) {
    const char *e = vfs_ext(n);
    return !strcasecmp(e, "png") || !strcasecmp(e, "jpg") || !strcasecmp(e, "jpeg") || !strcasecmp(e, "gif") || !strcasecmp(e, "bmp") || !strcasecmp(e, "ppm");
}

static void tab_set_doc(tab_t *t, doc_t *d) {
    css_cancel(t);
    if (t->js) { js_page_free(t->js); t->js = NULL; }
    if (t->doc) doc_free(t->doc);
    t->doc = d;
    t->img_dirty = false;
    if (d) {
        d->zoom = t->zoom;
        strncpy(t->title, d->title, sizeof t->title - 1);
        t->title[sizeof t->title - 1] = 0;
    }
}

static doc_t *doc_from_html(const u8 *data, usize n, const char *url, const char *ctype) {
    char cs[40] = "";
    if (ctype) {
        const char *c = strstr(ctype, "charset=");
        if (c) { c += 8; int k = 0; while (*c && *c != ';' && *c != ' ' && k < 39) { if (*c != '"') cs[k++] = *c; c++; } cs[k] = 0; }
    }
    if (!cs[0]) html_sniff_charset(data, n, cs, sizeof cs);
    usize ulen;
    char *u = charset_to_utf8(data, n, cs, &ulen);
    if (!u) return NULL;
    doc_t *d = doc_parse(u, ulen, url);
    kfree(u);
    return d;
}

static doc_t *doc_from_text(const u8 *data, usize n, const char *url, const char *title) {
    sbuf_t s = {0};
    sb_puts(&s, "<html><head><title>");
    sb_html(&s, title);
    sb_puts(&s, "</title></head><body style=\"margin:18px 24px\"><pre style=\"white-space:pre-wrap;font-size:13px;margin:0\">");
    usize ulen;
    char *u = charset_to_utf8(data, n, NULL, &ulen);
    if (u) {
        for (usize i = 0; i < ulen; i++) {
            char c = u[i];
            if (c == '&') sb_puts(&s, "&amp;");
            else if (c == '<') sb_puts(&s, "&lt;");
            else sb_putc(&s, c);
        }
        kfree(u);
    }
    sb_puts(&s, "</pre></body></html>");
    doc_t *d = doc_parse(s.p ? s.p : "", (usize)s.len, url);
    sb_free(&s);
    return d;
}

static doc_t *doc_from_image(const u8 *data, usize n, const char *url, const char *name) {
    surface_t *img = image_decode(data, n);
    sbuf_t s = {0};
    sb_puts(&s, "<html><head><title>");
    sb_html(&s, name);
    if (img) sb_printf(&s, " (%d×%d pixels)", img->w, img->h);
    sb_printf(&s, "</title></head><body style=\"margin:0;background:%s;text-align:center\">", T.dark ? "#1e1e1e" : "#f0f0f0");
    if (img) { sb_puts(&s, "<img style=\"max-width:100%\" src=\""); sb_html(&s, url); sb_puts(&s, "\">"); }
    else sb_puts(&s, "<p style=\"padding:60px;color:#888\">Formato de imagem não suportado.</p>");
    sb_puts(&s, "</body></html>");
    doc_t *d = doc_parse(s.p ? s.p : "", (usize)s.len, url);
    sb_free(&s);
    if (d && img) {
        img_t *im = kcalloc(1, sizeof(img_t));
        if (im) {
            strncpy(im->url, url, sizeof im->url - 1);
            im->state = IMG_OK;
            im->surf = img;
            d->images = im;
            d->nimages = 1;
        } else surface_free(img);
    } else surface_free(img);
    return d;
}

static doc_t *doc_dir_listing(vnode_t *dir, const char *path, const char *url) {
    sbuf_t s = {0};
    const char *bg, *fg, *sub, *card, *line;
    about_colors(&bg, &fg, &sub, &card, &line);
    sb_puts(&s, "<html><head><meta charset=utf-8><title>");
    sb_html(&s, path);
    sb_printf(&s, "</title><style>body{margin:0;background:%s;color:%s;font-family:sans-serif}.w{max-width:760px;margin:0 auto;padding:34px 28px}"
                  "h1{font-size:22px;margin:0 0 16px}table{width:100%%;border-collapse:collapse;background:%s;border-radius:10px;font-size:13px}"
                  "td,th{padding:7px 12px;border-bottom:1px solid %s;text-align:left}th{color:%s;font-size:12px}a{color:#0a84ff;text-decoration:none}"
                  ".k{color:%s}</style></head><body><div class=w><h1>Índice de ",
              bg, fg, card, line, sub, sub);
    sb_html(&s, path);
    sb_puts(&s, "</h1><table><tr><th>Nome</th><th>Tipo</th><th>Tamanho</th></tr>");
    if (strcmp(path, "/")) sb_puts(&s, "<tr><td><a href=\"../\">&larr; Pasta acima</a></td><td></td><td></td></tr>");
    for (vnode_t *c = dir->child; c; c = c->next) {
        if (c->name[0] == '.') continue;
        sb_puts(&s, "<tr><td><a href=\"");
        sbuf_t enc = {0};
        for (const char *p = c->name; *p; p++) {
            if (*p == ' ') sb_puts(&enc, "%20");
            else if (*p == '"') sb_puts(&enc, "%22");
            else if (*p == '#') sb_puts(&enc, "%23");
            else if (*p == '?') sb_puts(&enc, "%3F");
            else sb_putc(&enc, *p);
        }
        sb_puts(&s, enc.p ? enc.p : "");
        sb_free(&enc);
        if (c->dir) sb_putc(&s, '/');
        sb_puts(&s, "\">");
        sb_html(&s, c->name);
        if (c->dir) sb_putc(&s, '/');
        sb_printf(&s, "</a></td><td class=k>%s</td><td class=k>", vfs_kind(c));
        if (!c->dir) {
            if (c->size < 1000) sb_printf(&s, "%llu bytes", (u64)c->size);
            else sb_printf(&s, "%llu KB", (u64)(c->size + 500) / 1000);
        }
        sb_puts(&s, "</td></tr>");
    }
    sb_puts(&s, "</table></div></body></html>");
    (void)url;
    doc_t *d = doc_parse(s.p ? s.p : "", (usize)s.len, url);
    sb_free(&s);
    return d;
}

/* ============================================================ navegação */
static void normalize_input(const char *in, char *out, int cap) {
    while (isspace(*in)) in++;
    char tmp[1024];
    strncpy(tmp, in, sizeof tmp - 1);
    tmp[sizeof tmp - 1] = 0;
    int l = (int)strlen(tmp);
    while (l && isspace(tmp[l - 1])) tmp[--l] = 0;
    char sch[16];
    if (url_scheme(tmp, sch, sizeof sch) && (!strcmp(sch, "http") || !strcmp(sch, "https") || !strcmp(sch, "file") ||
                                             !strcmp(sch, "about") || !strcmp(sch, "data"))) {
        strncpy(out, tmp, cap - 1);
        out[cap - 1] = 0;
        return;
    }
    if (tmp[0] == '/') { snprintf(out, cap, "file://%s", tmp); return; }
    bool space = strchr(tmp, ' ') != NULL;
    const char *slash = strchr(tmp, '/');
    int hostlen = slash ? (int)(slash - tmp) : l;
    bool dot = false;
    for (int i = 0; i < hostlen; i++) if (tmp[i] == '.') dot = true;
    if (!space && (dot || !strncmp(tmp, "localhost", 9)) && hostlen > 0 && tmp[0] != '.' && tmp[hostlen - 1] != '.') {
        snprintf(out, cap, "http://%s", tmp);
        return;
    }
    sbuf_t s = {0};
    sb_puts(&s, SEARCH_URL);
    sb_urlenc(&s, tmp);
    strncpy(out, s.p ? s.p : SEARCH_URL, cap - 1);
    out[cap - 1] = 0;
    sb_free(&s);
}

static void push_history(tab_t *t, const char *url) {
    if (t->nhist && t->hpos >= 0 && !strcmp(t->hist[t->hpos].url, url)) return;
    if (t->hpos < t->nhist - 1) t->nhist = t->hpos + 1;
    if (t->nhist == HIST) { memmove(t->hist, t->hist + 1, sizeof(hent_t) * (HIST - 1)); t->nhist--; }
    strncpy(t->hist[t->nhist].url, url, sizeof t->hist[0].url - 1);
    t->hist[t->nhist].url[sizeof t->hist[0].url - 1] = 0;
    t->hist[t->nhist].scroll = 0;
    t->hpos = t->nhist++;
}

static void commit(tab_t *t, doc_t *d, const char *url, bool record, int scroll) {
    tab_set_doc(t, d);
    css_start(t);
    strncpy(t->url, url, sizeof t->url - 1);
    t->url[sizeof t->url - 1] = 0;
    t->about = !strncmp(url, "about:", 6);
    t->about_dark = T.dark;
    t->scroll = scroll;
    const char *fr = url_fragment(url);
    if (fr && *fr) { strncpy(t->frag, fr, sizeof t->frag - 1); t->frag[sizeof t->frag - 1] = 0; }
    else t->frag[0] = 0;
    if (record) push_history(t, url);
    else if (t->nhist && t->hpos >= 0) { strncpy(t->hist[t->hpos].url, url, sizeof t->hist[0].url - 1); }
    /* JavaScript: executa os scripts da página (os externos continuam no tick) */
    if (d && !t->about && js_enabled) t->js = js_page_new(d, url, 0, 0);
    if (!t->title[0]) {
        char host[128];
        if (url_host(url, host, sizeof host)) strncpy(t->title, host, sizeof t->title - 1);
        else { const char *sl = strrchr(url, '/'); strncpy(t->title, sl && sl[1] ? sl + 1 : url, sizeof t->title - 1); }
    }
}

static void show_error(tab_t *t, const char *url, const char *msg, const char *detail, bool record) {
    sbuf_t s = {0};
    gen_error(&s, url, msg, detail);
    doc_t *d = doc_parse(s.p ? s.p : "", (usize)s.len, url);
    sb_free(&s);
    commit(t, d, url, record, 0);
    t->about = false;
}

static void cancel_load(tab_t *t) {
    if (t->req) { http_free(t->req); t->req = NULL; }
}

static bool load_local(tab_t *t, const char *url, bool record, int scroll) {
    if (!strncmp(url, "about:", 6)) {
        sbuf_t s = {0};
        const char *name = url + 6;
        if (!strncmp(name, "inicio", 6) || !strncmp(name, "home", 4) || !strncmp(name, "favoritos", 9) || !*name) gen_inicio(&s);
        else if (!strncmp(name, "ajuda", 5) || !strncmp(name, "help", 4)) gen_ajuda(&s);
        else if (!strncmp(name, "blank", 5)) sb_puts(&s, "<html><body></body></html>");
        else { show_error(t, url, "Esta página interna não existe.", NULL, record); return true; }
        doc_t *d = doc_parse(s.p ? s.p : "", (usize)s.len, url);
        sb_free(&s);
        commit(t, d, url, record, scroll);
        return true;
    }
    if (!strncmp(url, "file:", 5)) {
        const char *p = url + 5;
        if (!strncmp(p, "//", 2)) { p += 2; if (strncmp(p, "/", 1)) { const char *sl = strchr(p, '/'); p = sl ? sl : "/"; } }
        char path[512];
        url_decode_path(p, path, sizeof path);
        int pl = (int)strlen(path);
        if (pl > 1 && path[pl - 1] == '/') path[pl - 1] = 0;
        vnode_t *n = vfs_lookup(path[0] ? path : "/");
        if (!n) { show_error(t, url, "O arquivo não foi encontrado no disco.", path, record); return true; }
        doc_t *d;
        if (n->dir) {
            char u2[1024];
            snprintf(u2, sizeof u2, "file://%s%s", path, strcmp(path, "/") && url[strlen(url) - 1] != '/' ? "/" : "");
            if (!strcmp(path, "/")) strcpy(u2, "file:///");
            d = doc_dir_listing(n, path[0] ? path : "/", u2);
            commit(t, d, u2, record, scroll);
            return true;
        }
        const char *e = vfs_ext(n->name);
        if (!strcasecmp(e, "html") || !strcasecmp(e, "htm") || !strcasecmp(e, "xhtml")) d = doc_from_html(n->data, n->size, url, NULL);
        else if (is_image_name(n->name)) d = doc_from_image(n->data, n->size, url, n->name);
        else d = doc_from_text(n->data, n->size, url, n->name);
        commit(t, d, url, record, scroll);
        return true;
    }
    if (!strncmp(url, "data:", 5)) {
        char mime[64];
        usize len;
        u8 *data = data_url(url, &len, mime, sizeof mime);
        if (!data) { show_error(t, "data:", "URL de dados inválida.", NULL, record); return true; }
        doc_t *d;
        if (!strncmp(mime, "image/", 6)) d = doc_from_image(data, len, url, "imagem");
        else if (strstr(mime, "html")) d = doc_from_html(data, len, url, NULL);
        else d = doc_from_text(data, len, url, "texto");
        kfree(data);
        commit(t, d, url, record, scroll);
        return true;
    }
    return false;
}

static void start_request(tab_t *t, const char *url, bool record, int scroll, const char *post, int post_len) {
    cancel_load(t);
    t->req = post ? http_post(url, "application/x-www-form-urlencoded", post, (u32)post_len) : http_get(url);
    strncpy(t->req_url, url, sizeof t->req_url - 1);
    t->req_url[sizeof t->req_url - 1] = 0;
    t->req_record = record;
    t->req_scroll = scroll;
    t->load_t0 = timer_ticks();
    if (!t->req) show_error(t, url, "Não foi possível iniciar a conexão.", NULL, record);
}

static void tab_navigate(bwin_t *b, tab_t *t, const char *input, bool record) {
    char url[1024];
    normalize_input(input, url, sizeof url);
    if (!url[0]) return;
    /* mesma página, só muda o fragmento */
    if (t->doc && url_same_doc(url, t->url) && url_fragment(url) && !t->req) {
        const char *fr = url_fragment(url);
        int y = doc_anchor_y(t->doc, fr);
        if (record) { if (t->nhist) t->hist[t->hpos].scroll = t->scroll; push_history(t, url); }
        strncpy(t->url, url, sizeof t->url - 1);
        t->scroll = y >= 0 ? y : 0;
        return;
    }
    if (t->doc && !strcmp(url, t->url)) record = false; /* recarregar */
    if (record && t->nhist) t->hist[t->hpos].scroll = t->scroll;
    cancel_load(t);
    if (b) b->w->ui.focus = 0;
    if (load_local(t, url, record, 0)) return;
    start_request(t, url, record, 0, NULL, 0);
    if (!t->doc) strncpy(t->title, "Carregando…", sizeof t->title - 1);
}

static void tab_go_history(bwin_t *b, tab_t *t, int delta) {
    int np = t->hpos + delta;
    if (np < 0 || np >= t->nhist) return;
    t->hist[t->hpos].scroll = t->scroll;
    t->hpos = np;
    hent_t *h = &t->hist[np];
    cancel_load(t);
    if (b) b->w->ui.focus = 0;
    if (t->doc && url_same_doc(h->url, t->url)) {
        strncpy(t->url, h->url, sizeof t->url - 1);
        t->scroll = h->scroll;
        return;
    }
    if (load_local(t, h->url, false, h->scroll)) return;
    start_request(t, h->url, false, h->scroll, NULL, 0);
}

static void tab_reload(bwin_t *b, tab_t *t) {
    if (t->req) { cancel_load(t); return; }
    int sc = t->scroll;
    if (load_local(t, t->url, false, sc)) return;
    start_request(t, t->url, false, sc, NULL, 0);
}

/* conclusão de uma requisição principal */
static void finish_request(tab_t *t) {
    http_req_t *r = t->req;
    int st = http_poll(r);
    if (st == 0) return;
    char url[1024];
    strncpy(url, r->location[0] ? r->location : t->req_url, sizeof url - 1);
    url[sizeof url - 1] = 0;
    /* preserva o fragmento pedido */
    const char *fr = url_fragment(t->req_url);
    if (fr && !strchr(url, '#') && strlen(url) + strlen(fr) + 2 < sizeof url) { strcat(url, "#"); strcat(url, fr); }
    bool record = t->req_record;
    int scroll = t->req_scroll;
    t->req = NULL;
    if (st < 0) {
        const char *e = r->error[0] ? r->error : "erro desconhecido";
        char msg[200];
        char el[128];
        int k = 0;
        for (; e[k] && k < 127; k++) el[k] = (char)tolower(e[k]);
        el[k] = 0;
        if (!net_configured() || strstr(el, "indispon") || strstr(el, "sem rede"))
            snprintf(msg, sizeof msg, "O seu computador não parece estar conectado à Internet.");
        else if (strstr(el, "dns") || strstr(el, "resol") || strstr(el, "host"))
            snprintf(msg, sizeof msg, "Não foi possível encontrar o servidor.");
        else if (strstr(el, "tls") || strstr(el, "certific") || strstr(el, "segur"))
            snprintf(msg, sizeof msg, "Não foi possível estabelecer uma conexão segura com o servidor.");
        else if (strstr(el, "tempo") || strstr(el, "timeout"))
            snprintf(msg, sizeof msg, "O servidor demorou demais para responder.");
        else snprintf(msg, sizeof msg, "A conexão com o servidor falhou.");
        show_error(t, url, msg, e, record);
        http_free(r);
        return;
    }
    const char *ct = r->content_type;
    doc_t *d = NULL;
    if (!r->body || !r->body_len) {
        if (r->status >= 400) {
            char msg[96];
            snprintf(msg, sizeof msg, "O servidor respondeu com o erro HTTP %d.", r->status);
            show_error(t, url, msg, NULL, record);
            http_free(r);
            return;
        }
        d = doc_parse("", 0, url);
    } else if (!strncasecmp(ct, "image/", 6)) {
        const char *sl = strrchr(url, '/');
        d = doc_from_image(r->body, r->body_len, url, sl && sl[1] ? sl + 1 : "imagem");
    } else if (!strncasecmp(ct, "text/plain", 10) || !strncasecmp(ct, "application/json", 16) || !strncasecmp(ct, "text/css", 8) ||
               !strncasecmp(ct, "application/javascript", 22) || !strncasecmp(ct, "text/javascript", 15)) {
        const char *sl = strrchr(url, '/');
        d = doc_from_text(r->body, r->body_len, url, sl && sl[1] ? sl + 1 : url);
    } else {
        /* sem tipo: tenta adivinhar imagem pela assinatura */
        surface_t *probe = NULL;
        if (!ct[0] && r->body_len > 4 && (r->body[0] == 0x89 || r->body[0] == 0xFF || r->body[0] == 'G')) probe = image_decode(r->body, r->body_len);
        if (probe) { surface_free(probe); d = doc_from_image(r->body, r->body_len, url, "imagem"); }
        else d = doc_from_html(r->body, r->body_len, url, ct);
    }
    if (!d) { show_error(t, url, "Memória insuficiente para exibir a página.", NULL, record); http_free(r); return; }
    commit(t, d, url, record, scroll);
    http_free(r);
}

/* ============================================================ imagens */
static bool pump_images(tab_t *t) {
    doc_t *d = t->doc;
    if (!d) return false;
    bool changed = false;
    int active = 0;
    for (img_t *im = d->images; im; im = im->next) {
        if (im->state != IMG_LOADING || !im->req) continue;
        int st = http_poll(im->req);
        if (st == 0) { active++; continue; }
        if (st > 0 && im->req->body && im->req->body_len && im->req->status < 400) im->surf = image_decode(im->req->body, im->req->body_len);
        im->state = im->surf ? IMG_OK : IMG_ERROR;
        http_free(im->req);
        im->req = NULL;
        changed = true;
    }
    for (img_t *im = d->images; im && active < 4; im = im->next) {
        if (im->state != IMG_PENDING) continue;
        if (!strncmp(im->url, "file:", 5)) {
            const char *p = im->url + 5;
            if (!strncmp(p, "//", 2)) p += 2;
            char path[512];
            url_decode_path(p, path, sizeof path);
            vnode_t *n = vfs_lookup(path);
            im->surf = n && !n->dir ? image_decode(n->data, n->size) : NULL;
            im->state = im->surf ? IMG_OK : IMG_ERROR;
            changed = true;
        } else if (!strncmp(im->url, "data:", 5)) {
            char mime[64];
            usize len;
            u8 *data = data_url(im->url, &len, mime, sizeof mime);
            im->surf = data ? image_decode(data, len) : NULL;
            kfree(data);
            im->state = im->surf ? IMG_OK : IMG_ERROR;
            changed = true;
        } else if (!strncmp(im->url, "http", 4)) {
            const char *e = vfs_ext(im->url);
            if (!strncasecmp(e, "svg", 3) || !strncasecmp(e, "webp", 4) || !strncasecmp(e, "avif", 4)) { im->state = IMG_ERROR; changed = true; continue; }
            im->req = http_get(im->url);
            if (im->req) { im->state = IMG_LOADING; active++; }
            else { im->state = IMG_ERROR; changed = true; }
        } else { im->state = IMG_ERROR; changed = true; }
    }
    if (changed) { t->img_dirty = true; t->img_dirty_t = timer_ticks(); }
    return changed;
}

static bool images_loading(tab_t *t) {
    if (!t->doc) return false;
    for (img_t *im = t->doc->images; im; im = im->next) if (im->state == IMG_LOADING || im->state == IMG_PENDING) return true;
    return false;
}

/* ============================================================ formulários */
static void submit_form(bwin_t *b, tab_t *t, node_t *form, ctrl_t *submitter) {
    doc_t *d = t->doc;
    if (!d || !form) return;
    const char *action = node_attr(form, "action");
    const char *method = node_attr(form, "method");
    bool post = method && !strcasecmp(method, "post");
    char url[1024];
    url_resolve(d->base, action && *action ? action : d->url, url, sizeof url);
    sbuf_t q = {0};
    for (int i = 0; i < d->nctrls; i++) {
        ctrl_t *c = d->ctrls[i];
        if (c->form != form) continue;
        const char *name = node_attr(c->node, "name");
        if (!name || !*name) continue;
        const char *val = NULL;
        switch (c->kind) {
        case C_TEXT: case C_PASSWORD: case C_HIDDEN: case C_TEXTAREA: val = c->buf; break;
        case C_CHECK: case C_RADIO: if (c->checked) val = c->buf[0] ? c->buf : "on"; break;
        case C_SELECT: if (c->nopts) val = c->vals[CLAMP(c->sel, 0, c->nopts - 1)]; break;
        case C_SUBMIT: if (c == submitter) { const char *v = node_attr(c->node, "value"); val = v ? v : c->label; } break;
        case C_IMAGE:
            if (c == submitter) {
                if (q.len) sb_putc(&q, '&');
                sb_urlenc(&q, name); sb_puts(&q, ".x=1&"); sb_urlenc(&q, name); sb_puts(&q, ".y=1");
            }
            break;
        default: break;
        }
        if (!val) continue;
        if (q.len) sb_putc(&q, '&');
        sb_urlenc(&q, name);
        sb_putc(&q, '=');
        sb_urlenc(&q, val);
    }
    if (post && !strncmp(url, "http", 4)) {
        if (t->nhist) t->hist[t->hpos].scroll = t->scroll;
        start_request(t, url, true, 0, q.p ? q.p : "", q.len);
    } else {
        char *qm = strchr(url, '?');
        if (qm) *qm = 0;
        char *hm = strchr(url, '#');
        if (hm) *hm = 0;
        char full[1024];
        snprintf(full, sizeof full, "%s?%s", url, q.p ? q.p : "");
        tab_navigate(b, t, full, true);
    }
    sb_free(&q);
}

/* ============================================================ controles da página */
static bool mouse_in_view(ui_t *u) { return rect_contains(g_view, u->mx, u->my); }

static void paint_textarea(ui_t *u, ctrl_t *c, rect_t r) {
    bool hover = u->inside && rect_contains(r, u->mx, u->my);
    if (hover) u->cursor = CURSOR_IBEAM;
    if (u->pressed) {
        if (hover) u->focus = (u32)c->id;
        else if (u->focus == (u32)c->id) u->focus = 0;
    }
    bool f = u->focus == (u32)c->id;
    if (f && u->key) {
        int len = (int)strlen(c->buf);
        bool cmd = u->mods & (MOD_CMD | MOD_CTRL);
        if (u->key == KEY_BACKSPACE) { if (len) c->buf[utf8_prev(c->buf, len)] = 0; u->key_used = true; }
        else if (u->key == KEY_ENTER) { if (len + 1 < c->cap) { c->buf[len] = '\n'; c->buf[len + 1] = 0; } u->key_used = true; }
        else if (u->text[0] && !cmd) {
            int tl = (int)strlen(u->text);
            if (len + tl < c->cap) memcpy(c->buf + len, u->text, (usize)tl + 1);
            u->key_used = true;
        }
        u->caret_blink = u->now;
    }
    if (f) ui_focus_ring(u, r, 3);
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, 3, T.field_border);
    gfx_round_rect(u->s, r.x + 1, r.y + 1, r.w - 2, r.h - 2, 2, T.field_bg);
    rect_t pc = surface_push_clip(u->s, rect_make(r.x + 3, r.y + 2, r.w - 6, r.h - 4));
    font_t *fn = font_get(FONT_REGULAR);
    int h = text_wrap(u->s, fn, 13, rect_make(r.x + 6, r.y + 4, r.w - 12, 100000), 17, T.text, c->buf, false);
    int off = h > r.h - 8 ? h - (r.h - 8) : 0;
    text_wrap(u->s, fn, 13, rect_make(r.x + 6, r.y + 4 - off, r.w - 12, 100000), 17, T.text, c->buf, true);
    if (f && ((u->now - u->caret_blink) / 530) % 2 == 0) {
        /* cursor ao fim do texto */
        const char *last = strrchr(c->buf, '\n');
        const char *ls = last ? last + 1 : c->buf;
        int lw = text_width(fn, 13, ls);
        int cy = r.y + 4 - off + MAX(0, h - 17);
        if (c->buf[0] && c->buf[strlen(c->buf) - 1] == '\n') { lw = 0; cy += 17; }
        if (lw > r.w - 14) lw = r.w - 14;
        gfx_fill(u->s, r.x + 6 + lw, cy + 1, 1, 15, T.dark ? RGB(230, 230, 230) : RGB(20, 20, 20));
    }
    surface_pop_clip(u->s, pc);
    if (f) u->repaint = true;
}

static bool g_record;

static void paint_control_real(ui_t *u, doc_t *d, ctrl_t *c, rect_t r);

void web_paint_control(ui_t *u, doc_t *d, ctrl_t *c, rect_t r) {
    bwin_t *b = g_paint_b;
    if (!b) { if (web_paint_control_hook) web_paint_control_hook(u, d, c, r); return; }   /* WKWebView (webview.c) */
    if (g_record) {
        /* durante a renderização do cache só registramos o controle; ele é desenhado por cima a cada quadro */
        if (b->nrec < (int)ARRAY_LEN(b->rec)) { b->rec[b->nrec].c = c; b->rec[b->nrec].r = r; b->nrec++; }
        return;
    }
    paint_control_real(u, d, c, r);
}

static void js_queue(bwin_t *b, ctrl_t *c, int type) {
    if (b->njsq < (int)ARRAY_LEN(b->jsq)) { b->jsq[b->njsq].c = c; b->jsq[b->njsq].type = type; b->njsq++; }
}

static u32 buf_sum(const char *s) { u32 h = 5381; for (; *s; s++) h = h * 33 + (u8)*s; return h; }

static void paint_control_real(ui_t *u, doc_t *d, ctrl_t *c, rect_t r) {
    bwin_t *b = g_paint_b;
    u32 sum0 = c->buf ? buf_sum(c->buf) : 0;
    bool chk0 = c->checked;
    int sel0 = c->sel;
    /* controles fora da área visível não recebem cliques */
    bool saved_inside = u->inside;
    if (!mouse_in_view(u)) u->inside = false;
    switch (c->kind) {
    case C_TEXT: case C_PASSWORD: {
        const char *ph = node_attr(c->node, "placeholder");
        char masked[512];
        if (c->kind == C_PASSWORD && u->focus != (u32)c->id) {
            int n = 0;
            for (const char *p = c->buf; *p && n < 500; p++) if (((u8)*p & 0xC0) != 0x80) { masked[n++] = '*'; }
            masked[n] = 0;
            char save[512];
            strncpy(save, c->buf, sizeof save - 1);
            save[sizeof save - 1] = 0;
            if (ui_textfield(u, c->id, r, masked, sizeof masked, ph)) {}
            if (u->focus == (u32)c->id) { strncpy(c->buf, save, (usize)c->cap - 1); u->caret = (int)strlen(c->buf); u->sel_anchor = -1; }
            break;
        }
        if (ui_textfield(u, c->id, r, c->buf, c->cap, ph ? ph : "")) {
            if (c->form) { b->submit_form = c->form; b->submitter = NULL; }
        }
        break;
    }
    case C_SUBMIT: case C_IMAGE:
        if (ui_button(u, c->id, r, c->label, false)) {
            b->js_clicked = c;
            if (c->form) { b->submit_form = c->form; b->submitter = c; }
        }
        break;
    case C_RESET: case C_BUTTON:
        if (ui_button(u, c->id, r, c->label[0] ? c->label : " ", false)) b->js_clicked = c;
        break;
    case C_CHECK: ui_checkbox(u, c->id, r.x, r.y, NULL, &c->checked); break;
    case C_RADIO:
        if (ui_radio(u, c->id, r.x, r.y, NULL, c->checked)) {
            const char *nm = node_attr(c->node, "name");
            for (int i = 0; i < d->nctrls; i++) {
                ctrl_t *o = d->ctrls[i];
                if (o->kind == C_RADIO && o->form == c->form && nm && node_attr(o->node, "name") && !strcmp(node_attr(o->node, "name"), nm)) o->checked = false;
            }
            c->checked = true;
        }
        break;
    case C_SELECT:
        if (c->nopts) c->sel = ui_popup(u, c->id, r, c->opts, c->nopts, CLAMP(c->sel, 0, c->nopts - 1));
        break;
    case C_TEXTAREA: paint_textarea(u, c, r); break;
    default: break;
    }
    u->inside = saved_inside;
    if (c->buf && buf_sum(c->buf) != sum0 && (c->kind == C_TEXT || c->kind == C_PASSWORD || c->kind == C_TEXTAREA)) js_queue(b, c, JSE_INPUT);
    if (c->checked != chk0 || c->sel != sel0) { js_queue(b, c, JSE_INPUT); js_queue(b, c, JSE_CHANGE); }
}

/* ============================================================ janelas e abas */
static tab_t *tab_new(void) {
    tab_t *t = kcalloc(1, sizeof(tab_t));
    if (!t) return NULL;
    t->zoom = 1;
    t->hpos = -1;
    return t;
}

static void tab_free(tab_t *t) {
    cancel_load(t);
    css_cancel(t);
    if (t->js) { js_page_free(t->js); t->js = NULL; }
    if (t->doc) doc_free(t->doc);
    kfree(t);
}

static tab_t *cur_tab(bwin_t *b) { return b->ntabs ? b->tabs[b->cur] : NULL; }

static void add_tab(bwin_t *b, const char *url, bool focus_addr) {
    if (b->ntabs >= MAX_TABS) return;
    tab_t *t = tab_new();
    if (!t) return;
    b->tabs[b->ntabs] = t;
    b->cur = b->ntabs++;
    tab_navigate(b, t, url, true);
    b->addr[0] = 0;
    if (focus_addr) { b->w->ui.focus = ADDR_ID; b->w->ui.caret = 0; b->w->ui.sel_anchor = -1; }
    else b->w->ui.focus = 0;
    wm_invalidate(b->w);
}

static void close_tab(bwin_t *b, int i) {
    if (i < 0 || i >= b->ntabs) return;
    if (b->ntabs == 1) { wm_close(b->w); return; }
    tab_free(b->tabs[i]);
    for (int k = i; k < b->ntabs - 1; k++) b->tabs[k] = b->tabs[k + 1];
    b->ntabs--;
    if (b->cur >= b->ntabs) b->cur = b->ntabs - 1;
    else if (b->cur > i) b->cur--;
    b->w->ui.focus = 0;
    b->hover_link = b->press_link = NULL;
}

static void win_close(window_t *w) {
    bwin_t *b = w->data;
    if (!b) return;
    for (int i = 0; i < b->ntabs; i++) tab_free(b->tabs[i]);
    surface_free(b->cache);
    for (bwin_t **pp = &bwins; *pp; pp = &(*pp)->next) if (*pp == b) { *pp = b->next; break; }
    kfree(b);
    w->data = NULL;
}

static void paint(window_t *w, ui_t *u);

static bwin_t *new_window(const char *url) {
    window_t *w = wm_create(&app_horizonte, "Horizonte", -1, -1, 1080, 700, WIN_RESIZABLE);
    if (!w) return NULL;
    bwin_t *b = kcalloc(1, sizeof(bwin_t));
    if (!b) { wm_close(w); return NULL; }
    b->w = w;
    b->close_tab = -1;
    w->data = b;
    w->paint = paint;
    w->on_close = win_close;
    w->toolbar_h = TOOLBAR;
    w->title_x = -1;
    w->min_w = 520;
    w->min_h = 320;
    b->next = bwins;
    bwins = b;
    add_tab(b, url, !strncmp(url, "about:inicio", 12));
    return b;
}

static bwin_t *front_bwin(void) {
    window_t *f = wm_focused();
    for (bwin_t *b = bwins; b; b = b->next) if (b->w == f && !b->w->closing) return b;
    for (bwin_t *b = bwins; b; b = b->next) if (!b->w->closing) return b;
    return NULL;
}

/* ============================================================ desenho da interface */
static void glyph_xmark(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_line(s, x + 2, y + 2, x + sz - 2, y + sz - 2, 1.5f, c);
    gfx_line(s, x + sz - 2, y + 2, x + 2, y + sz - 2, 1.5f, c);
}

static void glyph_lock(surface_t *s, int x, int y, u32 c) {
    gfx_round_rect_stroke(s, x + 2.5f, y, 6, 8, 3, 1.3f, c);
    gfx_round_rect(s, x, y + 5, 11, 8, 2, c);
}

static void draw_seg_button(surface_t *s, rect_t r, float rl, float rr, bool held) {
    u32 top = T.control_top, bot = T.control_bot;
    if (held) { top = color_lighten(top, T.dark ? 18 : -20); bot = color_lighten(bot, T.dark ? 18 : -20); }
    gfx_round_rect_ex(s, r.x, r.y, r.w, r.h, rl, rr, rr, rl, top, bot);
}

static bool tool_button(ui_t *u, int id, rect_t r, void (*g)(surface_t *, int, int, int, u32), int gsz, bool enabled) {
    bool hover = ui_hit(u, r);
    bool c = enabled && ui_clicked(u, id, r);
    bool held = enabled && u->active == (u32)id && hover && u->down;
    gfx_round_rect(u->s, r.x, r.y + 0.5f, r.w, r.h, 5, T.dark ? RGBA(0, 0, 0, 60) : RGBA(0, 0, 0, 34));
    draw_seg_button(u->s, r, 5, 5, held);
    gfx_round_rect_stroke(u->s, r.x, r.y, r.w, r.h, 5, 1, T.dark ? RGBA(255, 255, 255, 26) : RGBA(0, 0, 0, 40));
    g(u->s, r.x + (r.w - gsz) / 2, r.y + (r.h - gsz) / 2, gsz, enabled ? T.control_text : T.text3);
    return c;
}

static void glyph_plus2(surface_t *s, int x, int y, int sz, u32 c) {
    float m = sz / 2.0f;
    gfx_line(s, x + m, y + 2, x + m, y + sz - 2, 1.6f, c);
    gfx_line(s, x + 2, y + m, x + sz - 2, y + m, 1.6f, c);
}

static void glyph_home2(surface_t *s, int x, int y, int sz, u32 c) { glyph_house(s, x, y, sz, c); }

static const char *display_host(tab_t *t, char *buf, int cap, bool *secure) {
    if (t->req) {
        *secure = !strncmp(t->req_url, "https:", 6);
        if (url_host(t->req_url, buf, cap)) return buf;
    }
    *secure = !strncmp(t->url, "https:", 6);
    if (!strncmp(t->url, "about:inicio", 12)) return "";
    if (!strncmp(t->url, "about:ajuda", 11)) return "Ajuda do mOS";
    if (!strncmp(t->url, "file:", 5)) {
        char path[512];
        url_decode_path(t->url + 7, path, sizeof path);
        int l = (int)strlen(path);
        if (l > 1 && path[l - 1] == '/') path[--l] = 0;
        const char *sl = strrchr(path, '/');
        strncpy(buf, sl && sl[1] ? sl + 1 : path, cap - 1);
        buf[cap - 1] = 0;
        return buf;
    }
    if (url_host(t->url, buf, cap)) {
        if (!strncmp(buf, "www.", 4)) memmove(buf, buf + 4, strlen(buf) - 3);
        return buf;
    }
    strncpy(buf, t->url, cap - 1);
    buf[cap - 1] = 0;
    return buf;
}

static void draw_toolbar(bwin_t *b, tab_t *t, ui_t *u) {
    surface_t *s = u->s;
    int cw = u->cw;
    int by = (TOOLBAR - 24) / 2;
    /* voltar / avançar */
    bool can_back = t && t->hpos > 0, can_fwd = t && t->hpos < t->nhist - 1;
    rect_t rb = rect_make(84, by, 30, 24), rf = rect_make(114, by, 30, 24);
    bool hb = can_back && u->active == 7001 && ui_hit(u, rb) && u->down, hf = can_fwd && u->active == 7002 && ui_hit(u, rf) && u->down;
    gfx_round_rect(s, rb.x, rb.y + 0.5f, 60, 24, 5, T.dark ? RGBA(0, 0, 0, 60) : RGBA(0, 0, 0, 34));
    draw_seg_button(s, rb, 5, 0, hb);
    draw_seg_button(s, rf, 0, 5, hf);
    gfx_round_rect_stroke(s, rb.x, rb.y, 60, 24, 5, 1, T.dark ? RGBA(255, 255, 255, 26) : RGBA(0, 0, 0, 40));
    gfx_fill(s, rf.x, rf.y + 4, 1, 16, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 40));
    glyph_chevron(s, rb.x + 7, rb.y + 4, 16, can_back ? T.control_text : T.text3, 1);
    glyph_chevron(s, rf.x + 7, rf.y + 4, 16, can_fwd ? T.control_text : T.text3, 0);
    if (can_back && ui_clicked(u, 7001, rb)) tab_go_history(b, t, -1);
    if (can_fwd && ui_clicked(u, 7002, rf)) tab_go_history(b, t, +1);
    /* página inicial */
    if (tool_button(u, 7003, rect_make(154, by, 32, 24), glyph_home2, 14, true)) { b->nav_kind = 1; strcpy(b->nav_url, "about:inicio"); }
    /* nova aba */
    if (tool_button(u, 7004, rect_make(cw - 44, by, 32, 24), glyph_plus2, 12, true)) b->nav_kind = 2, strcpy(b->nav_url, "about:inicio");
    /* zoom */
    if (t && (t->zoom < 0.99f || t->zoom > 1.01f)) {
        char z[16];
        snprintf(z, sizeof z, "%d%%", (int)(t->zoom * 100 + 0.5f));
        if (ui_toolbar_button(u, 7005, rect_make(cw - 104, by, 52, 24), z, true)) { t->zoom = 1; if (t->doc) t->doc->need_layout = true; }
    }
    /* campo de endereço */
    int fw = MIN(cw - 2 * 140, 720);
    if (fw < 180) fw = MAX(120, cw - 200);
    int fx = (cw - fw) / 2;
    if (fx < 200) fx = 200;
    if (fx + fw > cw - 60) fw = cw - 60 - fx;
    rect_t fr = rect_make(fx, by, fw, 24);
    rect_t reload = rect_make(fr.x + fr.w - 24, fr.y + 2, 20, 20);
    bool focused = u->focus == ADDR_ID;
    bool just = false;
    if (!focused && u->pressed && ui_hit(u, fr) && !ui_hit(u, reload)) {
        u->focus = ADDR_ID;
        strncpy(b->addr, t && strncmp(t->url, "about:inicio", 12) ? t->url : "", sizeof b->addr - 1);
        b->addr[sizeof b->addr - 1] = 0;
        u->sel_anchor = 0;
        u->caret = (int)strlen(b->addr);
        u->caret_blink = u->now;
        u->active = ADDR_ID;
        focused = just = true;
    }
    if (focused && !just) {
        if (u->key == KEY_ESC) {
            u->focus = 0;
            u->key_used = true;
            focused = false;
        } else if (ui_textfield(u, ADDR_ID, fr, b->addr, sizeof b->addr, "Pesquise ou digite um endereço")) {
            b->nav_kind = 1;
            strncpy(b->nav_url, b->addr, sizeof b->nav_url - 1);
            u->focus = 0;
        }
        focused = u->focus == ADDR_ID;
    }
    if (just) {
        ui_focus_ring(u, fr, 3);
        gfx_round_rect(s, fr.x, fr.y, fr.w, fr.h, 3, T.field_border);
        gfx_round_rect(s, fr.x + 1, fr.y + 1, fr.w - 2, fr.h - 2, 2, T.field_bg);
        font_t *f = font_get(FONT_REGULAR);
        int tw = text_width(f, FONT_UI, b->addr);
        gfx_fill(s, fr.x + 6, fr.y + 4, MIN(tw, fr.w - 12), 16, T.selection);
        rect_t pc = surface_push_clip(s, rect_make(fr.x + 2, fr.y, fr.w - 4, fr.h));
        text_draw(s, f, FONT_UI, fr.x + 6, fr.y + 17, T.text, b->addr);
        surface_pop_clip(s, pc);
    } else if (!focused) {
        gfx_round_rect(s, fr.x, fr.y + 0.5f, fr.w, fr.h, 6, T.dark ? RGBA(0, 0, 0, 50) : RGBA(0, 0, 0, 22));
        gfx_round_rect(s, fr.x, fr.y, fr.w, fr.h, 6, T.dark ? RGB(80, 80, 82) : RGB(255, 255, 255));
        gfx_round_rect_stroke(s, fr.x, fr.y, fr.w, fr.h, 6, 1, T.dark ? RGBA(255, 255, 255, 20) : RGBA(0, 0, 0, 36));
        /* progresso dentro do campo, como no Safari */
        if (t && t->req) {
            float p = http_progress(t->req);
            float tp = 1.0f - fexp(-(float)(timer_ticks() - t->load_t0) / 1800.0f);
            if (p < tp * 0.85f) p = tp * 0.85f;
            int pw = (int)((fr.w - 4) * CLAMP(p, 0.04f, 1.0f));
            gfx_round_rect(s, fr.x + 2, fr.y + fr.h - 4, pw, 2.5f, 1.2f, T.accent);
        }
        char host[200];
        bool secure;
        const char *h = t ? display_host(t, host, sizeof host, &secure) : "";
        font_t *f = font_get(FONT_REGULAR);
        rect_t pc = surface_push_clip(s, rect_make(fr.x + 4, fr.y, fr.w - 30, fr.h));
        if (h[0]) {
            int tw = text_width(f, FONT_UI, h) + (secure ? 16 : 0);
            int tx = fr.x + (fr.w - tw) / 2;
            if (tx < fr.x + 8) tx = fr.x + 8;
            if (secure) { glyph_lock(s, tx, fr.y + 6, T.text2); tx += 16; }
            text_draw(s, f, FONT_UI, tx, fr.y + 17, T.text, h);
        } else {
            const char *ph = "Pesquise ou digite um endereço";
            int tw = text_width(f, FONT_UI, ph);
            glyph_search(s, fr.x + (fr.w - tw) / 2 - 18, fr.y + 6, 12, T.text2);
            text_draw(s, f, FONT_UI, fr.x + (fr.w - tw) / 2, fr.y + 17, T.text3, ph);
        }
        surface_pop_clip(s, pc);
        /* recarregar / parar */
        bool hov = ui_hit(u, reload);
        if (hov) gfx_round_rect(s, reload.x, reload.y, reload.w, reload.h, 4, T.dark ? RGBA(255, 255, 255, 24) : RGBA(0, 0, 0, 16));
        if (t && t->req) glyph_xmark(s, reload.x + 4, reload.y + 4, 12, T.text2);
        else glyph_reload(s, reload.x + 3, reload.y + 3, 14, T.text2);
        if (ui_clicked(u, 7006, reload) && t) tab_reload(b, t);
    }
}

static void draw_tabs(bwin_t *b, ui_t *u, int y) {
    surface_t *s = u->s;
    int cw = u->cw;
    gfx_fill(s, 0, y, cw, TABBAR, T.dark ? RGB(28, 28, 29) : RGB(204, 204, 206));
    gfx_fill(s, 0, y + TABBAR - 1, cw, 1, T.dark ? RGB(16, 16, 16) : RGB(178, 178, 180));
    int plus_w = 28;
    int tw = (cw - plus_w) / b->ntabs;
    font_t *f = font_get(FONT_REGULAR);
    for (int i = 0; i < b->ntabs; i++) {
        tab_t *t = b->tabs[i];
        rect_t r = rect_make(i * tw, y, tw, TABBAR - 1);
        bool act = i == b->cur;
        bool hov = ui_hit(u, r);
        if (act) gfx_fill(s, r.x, r.y, r.w, r.h + 1, T.dark ? RGB(52, 52, 54) : RGB(232, 232, 234));
        else if (hov) gfx_fill(s, r.x, r.y, r.w, r.h, T.dark ? RGB(36, 36, 38) : RGB(196, 196, 198));
        if (i > 0) gfx_fill(s, r.x, r.y + 4, 1, r.h - 8, T.dark ? RGB(56, 56, 58) : RGB(176, 176, 178));
        const char *title = t->title[0] ? t->title : "Sem título";
        if (!strncmp(t->url, "about:inicio", 12)) title = "Favoritos";
        int maxw = r.w - 40;
        int w2 = MIN(text_width(f, 12, title), maxw);
        text_draw_ellipsis(s, f, 12, r.x + (r.w - w2) / 2, r.y + 17, maxw, act ? T.text : T.text2, title);
        if (t->req) ui_spinner(u, r.x + r.w - 14, r.y + r.h / 2, 6);
        rect_t xr = rect_make(r.x + 6, r.y + 5, 15, 15);
        if (hov) {
            bool xh = ui_hit(u, xr);
            if (xh) gfx_round_rect(s, xr.x, xr.y, xr.w, xr.h, 3, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 24));
            glyph_xmark(s, xr.x + 2, xr.y + 2, 11, T.text2);
        }
        if (ui_clicked(u, 7100 + i * 2, xr)) b->close_tab = i;
        else if (ui_clicked(u, 7101 + i * 2, r)) { if (b->cur != i) { b->cur = i; u->focus = 0; b->hover_link = b->press_link = NULL; } }
    }
    rect_t pr = rect_make(cw - plus_w, y, plus_w, TABBAR - 1);
    if (ui_hit(u, pr)) gfx_fill(s, pr.x, pr.y, pr.w, pr.h, T.dark ? RGB(36, 36, 38) : RGB(196, 196, 198));
    gfx_fill(s, pr.x, pr.y + 4, 1, pr.h - 8, T.dark ? RGB(56, 56, 58) : RGB(176, 176, 178));
    glyph_plus2(s, pr.x + 8, pr.y + 6, 13, T.text2);
    if (ui_clicked(u, 7099, pr)) { b->nav_kind = 2; strcpy(b->nav_url, "about:inicio"); }
}

static void draw_status(ui_t *u, rect_t view, const char *text) {
    if (!text[0]) return;
    font_t *f = font_get(FONT_REGULAR);
    int tw = MIN(text_width(f, 12, text), view.w / 2 + 120);
    int w = tw + 16, h = 21;
    int x = view.x - 1, y = view.y + view.h - h + 1;
    gfx_round_rect_ex(u->s, x, y, w, h, 0, 5, 0, 0, T.dark ? RGBA(46, 46, 48, 245) : RGBA(246, 246, 246, 245), T.dark ? RGBA(46, 46, 48, 245) : RGBA(246, 246, 246, 245));
    gfx_round_rect_stroke(u->s, x, y, w + 1, h + 6, 5, 1, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 40));
    text_draw_ellipsis(u->s, f, 12, x + 8, y + 15, tw, T.text2, text);
}

static void paint(window_t *w, ui_t *u) {
    bwin_t *b = w->data;
    if (!b) return;
    tab_t *t = cur_tab(b);
    if (!t) return;
    g_paint_b = b;
    surface_t *s = u->s;
    /* tema mudou: regenera páginas internas */
    for (int i = 0; i < b->ntabs; i++) {
        tab_t *x = b->tabs[i];
        if (x->about && x->about_dark != T.dark && !x->req) {
            int sc = x->scroll;
            char url[1024];
            strcpy(url, x->url);
            load_local(x, url, false, sc);
        }
    }
    /* atalhos de teclado */
    if (u->key && (u->mods & (MOD_CMD | MOD_CTRL))) {
        int k = tolower(u->key);
        bool used = true;
        if (k == 't') { b->nav_kind = 2; strcpy(b->nav_url, "about:inicio"); }
        else if (k == 'n') { new_window("about:inicio"); }
        else if (k == 'w') b->close_tab = b->cur;
        else if (k == 'l') {
            u->focus = ADDR_ID;
            strncpy(b->addr, strncmp(t->url, "about:inicio", 12) ? t->url : "", sizeof b->addr - 1);
            u->sel_anchor = 0;
            u->caret = (int)strlen(b->addr);
            u->caret_blink = u->now;
        }
        else if (k == 'r') tab_reload(b, t);
        else if (k == '[' || u->key == KEY_LEFT) tab_go_history(b, t, -1);
        else if (k == ']' || u->key == KEY_RIGHT) tab_go_history(b, t, +1);
        else if (k == '=' || k == '+') { t->zoom = MIN(3.0f, t->zoom * 1.1f); }
        else if (k == '-') { t->zoom = MAX(0.5f, t->zoom / 1.1f); }
        else if (k == '0') { t->zoom = 1; }
        else if (u->key == KEY_TAB) { b->cur = (b->cur + 1) % b->ntabs; u->focus = 0; }
        else used = false;
        if (used) { u->key = 0; u->key_used = true; u->text[0] = 0; }
        t = cur_tab(b);
    }
    int tabs_h = b->ntabs > 1 ? TABBAR : 0;
    rect_t view = rect_make(0, TOOLBAR + tabs_h, u->cw, u->ch - TOOLBAR - tabs_h);
    g_view = view;
    doc_t *d = t->doc;
    if (t->js) js_page_viewport(t->js, view.w, view.h, t->scroll);
    /* keydown para o JavaScript (campo em foco ou corpo da página) */
    if (t->js && d && u->key && !u->key_used && u->focus != ADDR_ID) {
        node_t *tg = d->body;
        for (int i = 0; i < d->nctrls; i++) if ((u32)d->ctrls[i]->id == u->focus) { tg = d->ctrls[i]->node; break; }
        if (js_page_event(t->js, tg, JSE_KEYDOWN, u->key, u->mods)) { u->key = 0; u->text[0] = 0; u->key_used = true; }
        d = t->doc;
    }
    /* layout em cache: só refaz quando largura/zoom/conteúdo mudam */
    if (d) {
        u64 gap = MAX(400, (u64)d->layout_ms * 4);
        bool img_ready = t->img_dirty && timer_ticks() - d->last_layout > gap && (timer_ticks() - t->img_dirty_t > 120 || !images_loading(t));
        if (img_ready) { t->img_dirty = false; d->need_layout = true; }
        if (d->need_layout || d->layout_w != view.w || d->zoom != t->zoom || d->need_style) {
            d->zoom = t->zoom;
            doc_layout(d, view.w, view.h, t->zoom);
            if (t->frag[0]) {
                int y = doc_anchor_y(d, t->frag);
                if (y >= 0) t->scroll = y;
                t->frag[0] = 0;
            }
        }
        if (strcmp(t->title, d->title) && d->title[0]) strncpy(t->title, d->title, sizeof t->title - 1);
    }
    int content_h = d ? d->height : 0;
    int maxs = MAX(0, content_h - view.h);
    /* teclado de rolagem (só sem campo em foco) */
    if (u->key && !u->key_used && !u->focus && d) {
        int k = u->key;
        bool used = true;
        if (k == ' ' && !(u->mods & MOD_SHIFT)) t->scroll += view.h - 40;
        else if (k == ' ' || k == KEY_PGUP) t->scroll -= view.h - 40;
        else if (k == KEY_PGDN) t->scroll += view.h - 40;
        else if (k == KEY_DOWN) t->scroll += 40;
        else if (k == KEY_UP) t->scroll -= 40;
        else if (k == KEY_HOME) t->scroll = 0;
        else if (k == KEY_END) t->scroll = maxs;
        else if (k == KEY_BACKSPACE) tab_go_history(b, t, (u->mods & MOD_SHIFT) ? 1 : -1);
        else used = false;
        if (used) { u->key_used = true; u->key = 0; }
    }
    t->scroll = CLAMP(t->scroll, 0, maxs);
    /* página */
    node_t *hover = NULL;
    b->status[0] = 0;
    if (d && u->inside && rect_contains(view, u->mx, u->my)) {
        frag_t *f = doc_hit(d, u->mx - view.x, u->my - view.y + t->scroll, NULL);
        if (f && f->node && f->node->link) {
            hover = f->node->link;
            const char *href = node_attr(hover, "href");
            if (href) {
                char abs[600];
                url_resolve(d->base, href, abs, sizeof abs);
                strncpy(b->status, abs, sizeof b->status - 1);
            }
        }
    }
    b->hover_link = hover;
    if (d) {
        bool need = !b->cache || b->cache->w != view.w || b->cache->h != view.h || b->c_doc != d || b->c_gen != d->last_layout ||
                    b->c_scroll != t->scroll || b->c_hover != hover || b->c_dark != T.dark;
        if (need && view.w > 0 && view.h > 0) {
            if (!b->cache) b->cache = surface_new(view.w, view.h);
            else surface_resize(b->cache, view.w, view.h);
            ui_t tmp = *u;
            tmp.s = b->cache;
            surface_reset_clip(b->cache);
            b->nrec = 0;
            g_record = true;
            doc_paint(d, &tmp, rect_make(0, 0, view.w, view.h), t->scroll, hover);
            g_record = false;
            b->c_doc = d; b->c_gen = d->last_layout; b->c_scroll = t->scroll; b->c_hover = hover; b->c_dark = T.dark;
        }
        if (b->cache) {
            gfx_blit(s, b->cache, view.x, view.y);
            rect_t pc = surface_push_clip(s, view);
            for (int i = 0; i < b->nrec; i++) {
                rect_t r = b->rec[i].r;
                r.x += view.x; r.y += view.y;
                paint_control_real(u, d, b->rec[i].c, r);
            }
            surface_pop_clip(s, pc);
        }
    }
    else gfx_fill(s, view.x, view.y, view.w, view.h, T.dark ? RGB(30, 30, 30) : RGB(255, 255, 255));
    if (hover && rect_contains(view, u->mx, u->my) && u->cursor == CURSOR_ARROW) u->cursor = CURSOR_HAND;
    if (u->pressed && rect_contains(view, u->mx, u->my)) { b->press_link = hover; b->press_in_view = true; }
    /* eventos de controles e cliques para o JavaScript */
    bool js_prevented = false;
    if (t->js && d) {
        for (int i = 0; i < b->njsq; i++) js_page_event(t->js, b->jsq[i].c->node, b->jsq[i].type, 0, u->mods);
        if (u->released && b->press_in_view && rect_contains(view, u->mx, u->my)) {
            node_t *tg = b->js_clicked ? b->js_clicked->node : js_hit_node(d, u->mx - view.x, u->my - view.y + t->scroll);
            if (js_page_event(t->js, tg, JSE_CLICK, 0, u->mods)) {
                js_prevented = true;
                if (b->js_clicked && b->submitter == b->js_clicked) { b->submit_form = NULL; b->submitter = NULL; }
            }
            u->repaint = true;
        }
    }
    b->njsq = 0;
    b->js_clicked = NULL;
    if (u->released) b->press_in_view = false;
    if (js_prevented) b->press_link = NULL;
    if (u->released && b->press_link && b->press_link == hover && d) {
        const char *href = node_attr(hover, "href");
        if (href && *href) {
            char abs[1024];
            url_resolve(d->base, href, abs, sizeof abs);
            while (*href == ' ') href++;
            if (!strncasecmp(href, "javascript:", 11)) { if (t->js) js_page_eval_url(t->js, href + 11); }
            else if (!strncasecmp(abs, "javascript:", 11)) {}
            else {
                b->nav_kind = (u->mods & (MOD_CMD | MOD_CTRL)) ? 2 : 1;
                strncpy(b->nav_url, abs, sizeof b->nav_url - 1);
                b->nav_url[sizeof b->nav_url - 1] = 0;
            }
        }
        b->press_link = NULL;
    }
    if (u->released) b->press_link = NULL;
    ui_scroll(u, 7200, view, MAX(content_h, view.h), &t->scroll);
    /* barra de abas e de ferramentas */
    if (tabs_h) draw_tabs(b, u, TOOLBAR);
    draw_toolbar(b, t, u);
    if (t->req && !b->status[0]) {
        char host[128];
        snprintf(b->status, sizeof b->status, "Carregando %s…", url_host(t->req_url, host, sizeof host) ? host : t->req_url);
    }
    if (!b->status[0] && t->js && js_page_errors(t->js))
        snprintf(b->status, sizeof b->status, "JavaScript: %d erro(s) nesta página (Visualizar > Console JavaScript)", js_page_errors(t->js));
    draw_status(u, view, b->status);
    /* título da janela */
    char wt[96];
    const char *title = t->title[0] ? t->title : "Horizonte";
    if (!strncmp(t->url, "about:inicio", 12)) title = "Favoritos";
    snprintf(wt, sizeof wt, "%s", title);
    wm_set_title(w, wt);
    if (t->req || images_loading(t)) u->repaint = true;
    /* ações adiadas */
    if (b->submit_form) {
        node_t *f = b->submit_form;
        ctrl_t *sb = b->submitter;
        b->submit_form = NULL;
        b->submitter = NULL;
        /* o evento submit pode ser cancelado pelo script (validação de formulário) */
        if (!(t->js && js_page_event(t->js, f, JSE_SUBMIT, 0, 0))) submit_form(b, t, f, sb);
        u->clicked_any = true;
    }
    if (b->nav_kind) {
        int k = b->nav_kind;
        b->nav_kind = 0;
        if (k == 2) add_tab(b, b->nav_url, !strcmp(b->nav_url, "about:inicio"));
        else tab_navigate(b, t, b->nav_url, true);
        b->hover_link = b->press_link = NULL;
        u->clicked_any = true;
    }
    if (b->close_tab >= 0) {
        int i = b->close_tab;
        b->close_tab = -1;
        close_tab(b, i);
        u->clicked_any = true;
    }
    g_paint_b = NULL;
}

/* ============================================================ app */
static void tick(app_t *a) {
    static u64 last;
    u64 now = timer_ticks();
    if (now - last < 10) return;
    last = now;
    for (bwin_t *b = bwins; b; b = b->next) {
        if (b->w->closing) continue;
        bool changed = false;
        for (int i = 0; i < b->ntabs; i++) {
            tab_t *t = b->tabs[i];
            if (t->req) {
                finish_request(t);
                if (!t->req) changed = true;
            }
            if (i == b->cur && pump_images(t)) changed = true;
            if (css_pump(t)) changed = true;
            if (t->js) {
                if (js_page_tick(t->js)) changed = true;
                int sc = js_page_take_scroll(t->js);
                if (sc >= 0) { t->scroll = sc; changed = true; }
                node_t *f = js_page_take_submit(t->js);
                if (f) { submit_form(b, t, f, NULL); changed = true; }
            }
            if (t->js) {
                bool rl = false;
                const char *nu = js_page_take_nav(t->js, &rl);
                if (nu) {
                    char url[1024];
                    strncpy(url, nu, sizeof url - 1);
                    url[sizeof url - 1] = 0;
                    if (rl) tab_reload(b, t); else tab_navigate(b, t, url, true);
                    changed = true;
                }
            }
        }
        tab_t *t = cur_tab(b);
        if (t && t->img_dirty && now - t->img_dirty_t > 150) changed = true;
        if (changed) wm_invalidate(b->w);
    }
}

void browser_open_url(const char *url) {
    bwin_t *b = front_bwin();
    if (!b) { new_window(url); return; }
    tab_t *t = cur_tab(b);
    if (t) tab_navigate(b, t, url, true);
    wm_focus(b->w);
    wm_invalidate(b->w);
}

/* O motor próprio do Horizonte, de reserva: o principal é o Chromium (launchsvc.c, horizonte_motor_abrir), que abre as
 * janelas dele; este vale quando o Chromium não está instalado, não inicia ou cai ao abrir, e para as páginas about:. */
void horizonte_legado_abrir(const char *url) {
    app_horizonte.running = true;
    browser_open_url(url);
}

static void launch(app_t *a) {
    if (horizonte_motor_abrir(NULL)) return;   /* o motor Chromium: abre ou traz as janelas dele */
    bwin_t *b = front_bwin();
    if (b) {
        /* já aberto: nova aba na janela da frente */
        add_tab(b, "about:inicio", true);
        wm_focus(b->w);
        return;
    }
    new_window("about:inicio");
}

static menu_item_t m_file[] = {
    {"Nova Janela", "⌘N", M_NEWWIN}, {"Nova Aba", "⌘T", M_NEWTAB}, {"Abrir Local…", "⌘L", M_OPENLOC}, {NULL},
    {"Fechar Aba", "⌘W", M_CLOSETAB},
};
static menu_item_t m_view[] = {
    {"Recarregar Página", "⌘R", M_RELOAD}, {NULL}, {"Aumentar Zoom", "⌘+", M_ZOOMIN}, {"Reduzir Zoom", "⌘−", M_ZOOMOUT},
    {"Tamanho Real", "⌘0", M_ZOOM0}, {NULL}, {"Ativar JavaScript", NULL, M_JS, false, true}, {"Console JavaScript", NULL, M_JSCON},
};
static menu_item_t m_hist[] = {
    {"Voltar", "⌘[", M_BACK}, {"Avançar", "⌘]", M_FWD}, {NULL}, {"Favoritos", "⇧⌘H", M_HOME}, {"Ajuda do mOS", NULL, M_HELP},
};
static menu_item_t m_favs[] = {
    {"Wikipédia", NULL, M_FAV0 + 0}, {"DuckDuckGo Lite", NULL, M_FAV0 + 1}, {"Hacker News", NULL, M_FAV0 + 2},
    {"example.com", NULL, M_FAV0 + 3}, {"NeverSSL", NULL, M_FAV0 + 4}, {"Exemplo local", NULL, M_FAV0 + 10},
};
static menu_t menus[] = {
    {"Arquivo", m_file, ARRAY_LEN(m_file)}, {"Visualizar", m_view, ARRAY_LEN(m_view)},
    {"Histórico", m_hist, ARRAY_LEN(m_hist)}, {"Favoritos", m_favs, ARRAY_LEN(m_favs)},
};

static void on_menu(app_t *a, int act) {
    if (act == 2001) { about_open(); return; }
    if (act == M_NEWWIN) { new_window("about:inicio"); return; }
    bwin_t *b = front_bwin();
    if (!b) { new_window("about:inicio"); b = front_bwin(); if (!b) return; }
    tab_t *t = cur_tab(b);
    if (!t) return;
    switch (act) {
    case M_NEWTAB: add_tab(b, "about:inicio", true); break;
    case M_OPENLOC:
        b->w->ui.focus = ADDR_ID;
        strncpy(b->addr, strncmp(t->url, "about:inicio", 12) ? t->url : "", sizeof b->addr - 1);
        b->w->ui.sel_anchor = 0;
        b->w->ui.caret = (int)strlen(b->addr);
        break;
    case M_CLOSETAB: close_tab(b, b->cur); break;
    case M_RELOAD: tab_reload(b, t); break;
    case M_ZOOMIN: t->zoom = MIN(3.0f, t->zoom * 1.1f); break;
    case M_ZOOMOUT: t->zoom = MAX(0.5f, t->zoom / 1.1f); break;
    case M_ZOOM0: t->zoom = 1; break;
    case M_JS:
        js_enabled = !js_enabled;
        for (u32 i = 0; i < ARRAY_LEN(m_view); i++) if (m_view[i].action == M_JS) m_view[i].checked = js_enabled;
        tab_reload(b, t);
        break;
    case M_JSCON: {
        sbuf_t s = {0};
        sb_puts(&s, t->js ? js_page_console(t->js) : "(o JavaScript não está ativo nesta página)\n");
        if (t->js) sb_printf(&s, "\n-- %d erro(s); heap do motor: %u KiB\n", js_page_errors(t->js), (u32)(js_heap_bytes() / 1024));
        add_tab(b, "about:blank", false);
        tab_t *nt = cur_tab(b);
        doc_t *dd = doc_from_text((const u8 *)(s.p ? s.p : ""), (usize)s.len, "about:console", "Console JavaScript");
        sb_free(&s);
        if (nt && dd) { commit(nt, dd, "about:console", false, 0); strcpy(nt->title, "Console JavaScript"); }
        break;
    }
    case M_BACK: tab_go_history(b, t, -1); break;
    case M_FWD: tab_go_history(b, t, 1); break;
    case M_HOME: tab_navigate(b, t, "about:inicio", true); break;
    case M_HELP: tab_navigate(b, t, "about:ajuda", true); break;
    default:
        if (act >= M_FAV0 && act < M_FAV0 + (int)ARRAY_LEN(favs)) tab_navigate(b, t, favs[act - M_FAV0].url, true);
    }
    if (b->w) wm_invalidate(b->w);
}

static void on_quit(app_t *a) { (void)a; horizonte_motor_encerrar(); }   /* o motor Chromium encerra junto */

app_t app_horizonte = {
    .name = "Horizonte", .exec = "horizonte", .draw_icon = icon_horizonte, .launch = launch, .tick = tick, .on_menu = on_menu,
    .on_quit = on_quit, .menus = menus, .nmenus = ARRAY_LEN(menus), .pinned = true,
};
