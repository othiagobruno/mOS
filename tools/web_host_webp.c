/* mOS — teste de host do decodificador WebP (kernel/web/webp.c) contra o libwebp (referências do ref.py).
 * Sem perdas (VP8L) e alfa sem perdas: igualdade exata. Com perdas (VP8): diferença máxima pequena por canal
 * (o libwebp e o mOS fazem a mesma conta inteira; só a reamostragem do croma pode diferir em 1). */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "tools/web_host.h"

static const char *nomes[] = {
    "ll_foto", "ll_alfa", "ll_pal2", "ll_pal4", "ll_pal16", "ll_pal200", "ll_lento",
    "ly_q90", "ly_q50", "ly_q5", "ly_alfa", "ly_m6", "anim",
};

static void um(const char *nome) {
    const char *src = getenv("WEB_HOST_DIR"), *ref = getenv("WEB_HOST_REF");
    char p1[512], p2[512];
    snprintf(p1, sizeof p1, "%s/webp/%s.webp", src, nome);
    snprintf(p2, sizeof p2, "%s/%s.rgba", ref ? ref : "/nao-existe", nome);
    usize n1 = 0, n2 = 0;
    u8 *w = host_read_file(p1, &n1), *r = host_read_file(p2, &n2);
    CHECK(w != NULL, "%s: sem o arquivo de teste", nome);
    if (!w) return;
    int ww = 0, hh = 0;
    u32 *argb = webp_decode_argb(w, n1, &ww, &hh);
    CHECK(argb != NULL, "%s: não decodificou", nome);
    if (!argb || !r) { if (!r) host_printf("  (%s: sem referência, só a decodificação conta)\n", nome); kfree(w); if (argb) kfree(argb); if (r) kfree(r); return; }
    int rw = (int)(r[0] | r[1] << 8 | r[2] << 16 | (u32)r[3] << 24), rh = (int)(r[4] | r[5] << 8 | r[6] << 16 | (u32)r[7] << 24);
    CHECK(rw == ww && rh == hh, "%s: tamanho %dx%d, esperado %dx%d", nome, ww, hh, rw, rh);
    if (rw == ww && rh == hh) {
        bool lossless = nome[0] == 'l' && nome[1] == 'l';
        if (!strcmp(nome, "anim")) lossless = true;
        int maxd = 0, nd = 0;
        long soma = 0;
        for (int i = 0; i < ww * hh; i++) {
            const u8 *q = r + 8 + i * 4;
            u32 c = argb[i];
            int dv[4] = { (int)((c >> 16) & 255) - q[0], (int)((c >> 8) & 255) - q[1], (int)(c & 255) - q[2], (int)(c >> 24) - q[3] };
            for (int k = 0; k < 4; k++) {
                int a = dv[k] < 0 ? -dv[k] : dv[k];
                if (q[3] == 0 && k < 3) a = 0;          /* cor de pixel transparente não importa */
                if (a > maxd) maxd = a;
                if (a) nd++;
                soma += a;
            }
        }
        if (lossless) CHECK(maxd == 0, "%s: %d canais diferentes (máx %d)", nome, nd, maxd);
        else CHECK(maxd <= 3, "%s: diferença máxima %d, média %.3f", nome, maxd, (double)soma / (ww * hh * 4));
        if (web_host_fail && getenv("WEB_HOST_VERBOSE")) host_printf("  %s: máx %d, %d canais\n", nome, maxd, nd);
    }
    /* e pelo image_decode do motor (surface pré-multiplicado) */
    surface_t *s = image_decode(w, n1);
    CHECK(s && s->w == ww && s->h == hh, "%s: image_decode", nome);
    if (s) surface_free(s);
    kfree(argb); kfree(w); kfree(r);
}

WEB_HOST_CASE(webp_formatos) {
    for (unsigned i = 0; i < sizeof nomes / sizeof nomes[0]; i++) um(nomes[i]);
}

WEB_HOST_CASE(webp_lixo) {
    /* entradas truncadas e corrompidas não podem travar nem estourar */
    const char *src = getenv("WEB_HOST_DIR");
    char p[512];
    snprintf(p, sizeof p, "%s/webp/ly_q50.webp", src);
    usize n = 0;
    u8 *w = host_read_file(p, &n);
    CHECK(w != NULL, "sem ly_q50");
    if (!w) return;
    int ww, hh;
    for (usize cut = 0; cut < n; cut += n / 23 + 1) { u32 *a = webp_decode_argb(w, cut, &ww, &hh); if (a) kfree(a); }
    u32 seed = 99;
    for (int k = 0; k < 40; k++) {
        u8 *c = kmalloc(n);
        memcpy(c, w, n);
        for (int j = 0; j < 8; j++) { seed = seed * 1103515245 + 12345; usize at = 20 + (seed >> 8) % (n - 20); c[at] ^= (u8)(seed >> 3); }
        u32 *a = webp_decode_argb(c, n, &ww, &hh);
        if (a) kfree(a);
        kfree(c);
    }
    snprintf(p, sizeof p, "%s/webp/ll_lento.webp", src);
    u8 *l = host_read_file(p, &n);
    for (int k = 0; l && k < 40; k++) {
        u8 *c = kmalloc(n);
        memcpy(c, l, n);
        for (int j = 0; j < 4; j++) { seed = seed * 1103515245 + 12345; usize at = 21 + (seed >> 8) % (n - 21); c[at] ^= (u8)(seed >> 3); }
        u32 *a = webp_decode_argb(c, n, &ww, &hh);
        if (a) kfree(a);
        kfree(c);
    }
    CHECK(true, "sobreviveu");
    kfree(w);
    if (l) kfree(l);
}

/* ---------- imagem responsiva e <img> WebP na página */
static node_t *first_img(node_t *n) {
    if (n->type == N_ELEM && n->tag == T_IMG) return n;
    for (node_t *c = n->first; c; c = c->next) { node_t *r = first_img(c); if (r) return r; }
    return NULL;
}
static const char *pick(const char *html, int vw, float dpr) {
    static char buf[512];
    doc_t *d = doc_parse(html, strlen(html), "http://teste/p.html");
    node_t *img = first_img(d->root);
    const char *r = img ? img_pick_src(img, vw, dpr, buf, sizeof buf) : NULL;
    static char out[512];
    snprintf(out, sizeof out, "%s", r ? r : "(nada)");
    doc_free(d);
    return out;
}

WEB_HOST_CASE(img_srcset_picture) {
    const char *r;
    r = pick("<img src=a.png srcset='b.png 1x, c.png 2x'>", 800, 1);
    CHECK(!strcmp(r, "b.png"), "1x: %s", r);
    r = pick("<img src=a.png srcset='b.png 1x, c.png 2x'>", 800, 2);
    CHECK(!strcmp(r, "c.png"), "2x: %s", r);
    r = pick("<img src=a.png srcset='p.jpg 320w, m.jpg 800w, g.jpg 1600w' sizes='100vw'>", 700, 1);
    CHECK(!strcmp(r, "m.jpg"), "w: %s", r);
    r = pick("<img src=a.png srcset='p.jpg 320w, m.jpg 800w'>", 2000, 1);
    CHECK(!strcmp(r, "m.jpg"), "w maior disponível: %s", r);
    r = pick("<picture><source type=image/avif srcset=x.avif><source type=image/webp srcset='x.webp'><img src=x.jpg></picture>", 800, 1);
    CHECK(!strcmp(r, "x.webp"), "picture webp: %s", r);
    r = pick("<picture><source media='(min-width: 1000px)' srcset=g.webp><source srcset=p.webp><img src=x.jpg></picture>", 800, 1);
    CHECK(!strcmp(r, "p.webp"), "picture media: %s", r);
    r = pick("<picture><source media='(min-width: 1000px)' srcset=g.webp><img src=x.jpg></picture>", 1200, 1);
    CHECK(!strcmp(r, "g.webp"), "picture media 2: %s", r);
    r = pick("<picture><source type=image/jxl srcset=x.jxl><img src=x.jpg></picture>", 800, 1);
    CHECK(!strcmp(r, "x.jpg"), "fallback: %s", r);
    r = pick("<img data-srcset='l.png 1x'>", 800, 1);
    CHECK(!strcmp(r, "l.png"), "data-srcset: %s", r);
    r = pick("<img srcset='https://cdn.x/a,b.png 1x'>", 800, 1);
    CHECK(!strcmp(r, "https://cdn.x/a,b.png"), "vírgula dentro da URL: %s", r);
}

WEB_HOST_CASE(img_webp_na_pagina) {
    const char *html = "<html><body style='margin:0'><img src='webp/ll_pal2.webp' style='display:block'></body></html>";
    doc_t *d = doc_parse(html, strlen(html), "http://teste/p.html");
    doc_layout(d, 200, 100, 1.0f);
    int loaded = 0;
    for (img_t *im = d->images; im; im = im->next) {
        http_req_t *q = http_get(im->url);
        if (q && q->status == 200) { im->surf = image_decode(q->body, q->body_len); im->state = im->surf ? IMG_OK : IMG_ERROR; loaded += im->surf != NULL; }
        http_free(q);
    }
    CHECK(loaded == 1, "imagem webp carregada: %d", loaded);
    d->need_layout = true;
    doc_layout(d, 200, 100, 1.0f);
    surface_t *s = surface_new(200, 100);
    gfx_fill(s, 0, 0, 200, 100, RGB(255, 255, 255));
    ui_t u;
    memset(&u, 0, sizeof u);
    u.s = s;
    doc_paint(d, &u, rect_make(0, 0, 200, 100), 0, NULL);
    /* referência: o mesmo arquivo decodificado direto */
    usize n = 0;
    char p[512];
    snprintf(p, sizeof p, "%s/webp/ll_pal2.webp", getenv("WEB_HOST_DIR"));
    u8 *w = host_read_file(p, &n);
    int ww, hh;
    u32 *ref = webp_decode_argb(w, n, &ww, &hh);
    int bad = 0;
    for (int y = 0; y < hh; y++) for (int x = 0; x < ww; x++) if ((s->px[y * s->stride + x] & 0xFFFFFF) != (ref[y * ww + x] & 0xFFFFFF)) bad++;
    CHECK(bad == 0 && (s->px[30 * s->stride + 10] & 0xFFFFFF) == 0xFFFFFF, "pixels da imagem na página: %d diferentes", bad);
    kfree(ref); kfree(w);
    surface_free(s);
    doc_free(d);
}
