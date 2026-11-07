/* mOS — Dicionário (definições do Wikcionário em português e inglês, resumo da Wikipédia, histórico),
 * Acesso às Chaves (certificados raiz confiáveis do sistema com validade, chave e uso; senhas ficam no app
 * Senhas) e Configuração de Áudio e MIDI (dispositivos de entrada e saída, volume, monitoração, teste). */
#include "kit.h"
#include "net.h"
#include "x509.h"

extern app_t app_dicionario, app_chaves, app_audiomidi;
void synth_note(float freq, int ms, int wave, float vol);

/* ================================================================== Dicionário */
#define MAXDEF 40
typedef struct { char pos[48], lang[32]; char text[600]; } def_t;
static struct {
    window_t *w;
    char q[64], shown[64];
    int src;                     /* 0 Wikcionário (pt), 1 Wiktionary (en), 2 Wikipédia */
    http_req_t *req;
    def_t d[MAXDEF];
    int nd;
    char summary[4000], err[96];
    char hist[20][64];
    int nhist, scroll;
} DI;

static void strip_html(const char *in, char *out, int n) {
    int k = 0;
    bool tag = false;
    for (; *in && k < n - 1; in++) {
        if (*in == '<') { tag = true; continue; }
        if (*in == '>') { tag = false; continue; }
        if (tag) continue;
        if (*in == '&') {
            if (!strncmp(in, "&amp;", 5)) { out[k++] = '&'; in += 4; continue; }
            if (!strncmp(in, "&lt;", 4)) { out[k++] = '<'; in += 3; continue; }
            if (!strncmp(in, "&gt;", 4)) { out[k++] = '>'; in += 3; continue; }
            if (!strncmp(in, "&quot;", 6)) { out[k++] = '"'; in += 5; continue; }
            if (!strncmp(in, "&#160;", 6) || !strncmp(in, "&nbsp;", 6)) { out[k++] = ' '; in += 5; continue; }
        }
        out[k++] = *in;
    }
    out[k] = 0;
}
static void di_lookup(const char *word) {
    if (!word[0]) return;
    if (DI.req) { http_free(DI.req); DI.req = NULL; }
    snprintf(DI.shown, sizeof DI.shown, "%s", word);
    DI.nd = 0; DI.summary[0] = 0; DI.err[0] = 0; DI.scroll = 0;
    sbuf_t u = {0};
    if (DI.src == 2) sb_puts(&u, "https://pt.wikipedia.org/api/rest_v1/page/summary/");
    else sb_puts(&u, DI.src == 0 ? "https://pt.wiktionary.org/api/rest_v1/page/definition/" : "https://en.wiktionary.org/api/rest_v1/page/definition/");
    /* espaço vira sublinhado; o resto é codificado */
    char w2[64];
    snprintf(w2, sizeof w2, "%s", word);
    for (char *p = w2; *p; p++) if (*p == ' ') *p = '_';
    sbuf_t e = {0};
    sb_urlenc(&e, w2);
    sb_puts(&u, e.p ? e.p : "");
    sb_free(&e);
    DI.req = http_get(u.p);
    sb_free(&u);
    /* histórico */
    for (int i = 0; i < DI.nhist; i++) if (!strcasecmp(DI.hist[i], word)) { memmove(DI.hist[i], DI.hist[i + 1], 64 * (DI.nhist - i - 1)); DI.nhist--; break; }
    if (DI.nhist == 20) DI.nhist--;
    memmove(DI.hist[1], DI.hist[0], 64 * DI.nhist);
    snprintf(DI.hist[0], 64, "%s", word);
    DI.nhist++;
}
static void di_poll(void) {
    if (!DI.req) return;
    int st = http_poll(DI.req);
    if (!st) return;
    if (st > 0 && DI.req->status == 200 && DI.req->body) {
        char *j = kmalloc(DI.req->body_len + 1);
        memcpy(j, DI.req->body, DI.req->body_len);
        j[DI.req->body_len] = 0;
        if (DI.src == 2) {
            char t[4000];
            kit_json_str(j, "extract", t, sizeof t);
            snprintf(DI.summary, sizeof DI.summary, "%s", t);
            if (!t[0]) snprintf(DI.err, sizeof DI.err, "Nenhum artigo encontrado.");
        } else {
            /* idiomas: português primeiro, depois os demais da resposta */
            static const char *langs[] = {"pt", "en", "es", "fr", "it", "la", "de"};
            for (int L = 0; L < 7 && DI.nd < MAXDEF; L++) {
                for (int i = 0; i < 8 && DI.nd < MAXDEF; i++) {
                    char p[64], pos[48], lang[32];
                    snprintf(p, sizeof p, "%s[%d].partOfSpeech", langs[L], i);
                    if (!kit_json_str(j, p, pos, sizeof pos)) break;
                    snprintf(p, sizeof p, "%s[%d].language", langs[L], i);
                    kit_json_str(j, p, lang, sizeof lang);
                    for (int k = 0; k < 8 && DI.nd < MAXDEF; k++) {
                        char raw[1200];
                        snprintf(p, sizeof p, "%s[%d].definitions[%d].definition", langs[L], i, k);
                        if (!kit_json_str(j, p, raw, sizeof raw)) break;
                        def_t *d = &DI.d[DI.nd];
                        strip_html(raw, d->text, sizeof d->text);
                        if (!d->text[0]) continue;
                        snprintf(d->pos, sizeof d->pos, "%s", pos);
                        snprintf(d->lang, sizeof d->lang, "%s", lang);
                        DI.nd++;
                    }
                }
            }
            if (!DI.nd) snprintf(DI.err, sizeof DI.err, "Nenhuma definição encontrada.");
        }
        kfree(j);
    } else snprintf(DI.err, sizeof DI.err, st < 0 ? "Sem conexão com a internet." : DI.req->status == 404 ? "Nenhum resultado para esta palavra." : "O serviço não respondeu.");
    http_free(DI.req);
    DI.req = NULL;
    if (DI.w) wm_invalidate(DI.w);
}

static void di_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    di_poll();
    int sb = w->sidebar_w;
    int y = w->toolbar_h + 4;
    ui_label(u, 18, y + 2, "Histórico", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    for (int i = 0; i < DI.nhist; i++) {
        if (ui_sidebar_item(u, 119000 + i, rect_make(10, y, sb - 20, 26), DI.hist[i], !strcasecmp(DI.hist[i], DI.shown), NULL)) { char t[64]; snprintf(t, 64, "%s", DI.hist[i]); snprintf(DI.q, 64, "%s", t); di_lookup(t); }
        y += 28;
    }
    if (ui_searchfield(u, UI_ID, rect_make(u->cw - 12 - 240, TOOLBAR_Y(28), 240, 28), DI.q, sizeof DI.q)) di_lookup(DI.q);
    static const char *srcs[3] = {"Português", "Inglês", "Wikipédia"};
    int ns = ui_segmented(u, UI_ID, rect_make(sb + 14, TOOLBAR_Y(28), 300, 28), srcs, 3, DI.src);
    if (ns != DI.src) { DI.src = ns; if (DI.shown[0]) { char t[64]; snprintf(t, 64, "%s", DI.shown); di_lookup(t); } }
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg);
    if (!DI.shown[0]) {
        ui_label_center(u, rect_make(a.x, a.y + a.h / 2 - 30, a.w, 30), "Dicionário", FONT_BOLD, 24, T.text3);
        ui_label_center(u, rect_make(a.x, a.y + a.h / 2 + 4, a.w, 20), "Digite uma palavra no campo de busca e tecle Return.", FONT_REGULAR, 13, T.text3);
        return;
    }
    rect_t pc = surface_push_clip(s, a);
    int cy = a.y + 24 - DI.scroll, x0 = a.x + 36, tw = MIN(680, a.w - 72);
    text_draw(s, font_get(FONT_BOLD), 32, x0, cy + 30, T.text, DI.shown);
    cy += 52;
    if (DI.req) { ui_label(u, x0, cy, "Buscando…", FONT_REGULAR, 14, T.text2); u->repaint = true; }
    else if (DI.err[0]) ui_label(u, x0, cy, DI.err, FONT_REGULAR, 14, T.text2);
    else if (DI.src == 2) {
        cy += text_wrap(s, font_get(FONT_REGULAR), 15, rect_make(x0, cy, tw, 20000), 24, T.text, DI.summary, true);
        cy += 20;
        ui_label(u, x0, cy, "Fonte: Wikipédia (CC BY-SA 4.0)", FONT_REGULAR, 11.5f, T.text3);
        cy += 24;
    } else {
        char last_pos[48] = "", last_lang[32] = "";
        int num = 0;
        for (int i = 0; i < DI.nd; i++) {
            def_t *d = &DI.d[i];
            if (strcmp(d->lang, last_lang)) {
                snprintf(last_lang, 32, "%s", d->lang);
                cy += 6;
                text_draw(s, font_get(FONT_BOLD), 13, x0, cy + 14, T.accent, d->lang);
                cy += 22;
                last_pos[0] = 0;
            }
            if (strcmp(d->pos, last_pos)) {
                snprintf(last_pos, 48, "%s", d->pos);
                text_draw(s, font_get(FONT_SEMIBOLD), 14, x0, cy + 16, T.text2, d->pos);
                cy += 26;
                num = 0;
            }
            char n[8];
            snprintf(n, sizeof n, "%d", ++num);
            text_draw(s, font_get(FONT_BOLD), 14, x0, cy + 15, T.text, n);
            cy += text_wrap(s, font_get(FONT_REGULAR), 15, rect_make(x0 + 24, cy, tw - 24, 4000), 22, T.text, d->text, true) + 8;
        }
        cy += 12;
        ui_label(u, x0, cy, DI.src == 0 ? "Fonte: Wikcionário (CC BY-SA 4.0)" : "Fonte: Wiktionary (CC BY-SA 4.0)", FONT_REGULAR, 11.5f, T.text3);
        cy += 24;
    }
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, a, cy + DI.scroll - a.y + 10, &DI.scroll);
    if ((u->mods & MOD_CMD) && u->key == 'f') { u->focus = 0; u->key_used = true; }
}
static void di_tick(app_t *a) { di_poll(); }
static void di_close(window_t *w) { DI.w = NULL; }
static void di_launch(app_t *a) {
    if (DI.w) { wm_focus(DI.w); return; }
    DI.w = wm_create(&app_dicionario, "Dicionário", -1, -1, 880, 600, WIN_RESIZABLE);
    if (!DI.w) return;
    DI.w->paint = di_paint;
    DI.w->on_close = di_close;
    DI.w->toolbar_h = TOOLBAR_H;
    DI.w->sidebar_w = 200;
}
static void di_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    gfx_shadow(s, (int)(x + 10 * u), (int)(y + 6 * u), (int)(44 * u), (int)(52 * u), 4 * u, 3 * u, 70, (int)u);
    gfx_round_rect_grad(s, x + 10 * u, y + 6 * u, 44 * u, 52 * u, 4 * u, RGB(200, 60, 60), RGB(150, 30, 30));
    gfx_round_rect(s, x + 14 * u, y + 10 * u, 36 * u, 44 * u, 2 * u, RGB(250, 246, 236));
    text_draw_center(s, font_get(FONT_BOLD), 22 * u, rect_make((int)(x + 14 * u), (int)(y + 12 * u), (int)(36 * u), (int)(40 * u)), RGB(60, 50, 40), "Aa");
}
app_t app_dicionario = {.name = "Dicionário", .exec = "dicionario", .draw_icon = di_icon, .launch = di_launch, .tick = di_tick};

/* ================================================================== Acesso às Chaves */
static struct { window_t *w; int sel, scroll; char search[64]; } KC = {.sel = -1};

static void fmt_x509_date(u64 v, char *o, int n) {
    int y = (int)(v / 10000000000ull), m = (int)(v / 100000000ull % 100), d = (int)(v / 1000000ull % 100);
    snprintf(o, n, "%d de %s de %d", d, m >= 1 && m <= 12 ? kit_month[m - 1] : "?", y);
}
static void kc_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    int sb = w->sidebar_w;
    int y = w->toolbar_h + 4;
    ui_label(u, 18, y + 2, "Chaves de Sistema", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    ui_sidebar_item(u, 120000, rect_make(10, y, sb - 20, 28), "Raízes do Sistema", true, glyph_sb_disk);
    y += 34;
    ui_label(u, 18, y + 2, "Senhas e códigos ficam no app Senhas.", FONT_REGULAR, 11, T.text3);
    if (ui_button_style(u, UI_ID, rect_make(10, y + 24, sb - 20, 26), "Abrir Senhas", BTN_GRAY, true, NULL)) app_launch(apps_find("senhas"));
    ui_searchfield(u, UI_ID, rect_make(u->cw - 12 - 220, TOOLBAR_Y(28), 220, 28), KC.search, sizeof KC.search);
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg);
    /* detalhe do selecionado no topo */
    int dh = 150;
    rect_t det = rect_make(a.x, a.y, a.w, dh);
    gfx_fill(s, det.x, det.y + dh - 1, det.w, 1, T.separator);
    datetime_t now;
    rtc_now_utc(&now);
    u64 nowv = (u64)now.year * 10000000000ull + (u64)now.month * 100000000ull + (u64)now.day * 1000000ull;
    if (KC.sel >= 0 && x509_trust_get(KC.sel)) {
        const x509_cert_t *c = x509_trust_get(KC.sel);
        char cn[128], iss[128], d1[48], d2[48];
        x509_name_cn(c->subject, c->subject_len, cn, sizeof cn);
        x509_name_cn(c->issuer, c->issuer_len, iss, sizeof iss);
        fmt_x509_date(c->not_before, d1, sizeof d1);
        fmt_x509_date(c->not_after, d2, sizeof d2);
        /* selo do certificado */
        gfx_round_rect(s, det.x + 24, det.y + 20, 52, 64, 4, RGB(250, 246, 230));
        gfx_round_rect_stroke(s, det.x + 24, det.y + 20, 52, 64, 4, 1, RGB(200, 190, 160));
        gfx_circle(s, det.x + 50, det.y + 66, 10, RGB(220, 160, 40));
        ui_label(u, det.x + 92, det.y + 18, cn, FONT_BOLD, 17, T.text);
        char l2[200];
        snprintf(l2, sizeof l2, "Autoridade de certificação raiz · emitido por %s", iss);
        ui_label(u, det.x + 92, det.y + 44, l2, FONT_REGULAR, 12, T.text2);
        snprintf(l2, sizeof l2, "Válido de %s até %s", d1, d2);
        ui_label(u, det.x + 92, det.y + 62, l2, FONT_REGULAR, 12, T.text2);
        bool ok = c->not_after > nowv;
        gfx_circle(s, det.x + 98, det.y + 92, 6, ok ? RGB(48, 199, 89) : RGB(255, 69, 58));
        ui_label(u, det.x + 110, det.y + 84, ok ? "Este certificado é válido" : "Este certificado expirou", FONT_SEMIBOLD, 12.5f, T.text);
        const char *kt = c->key_type == X509_KEY_RSA ? "RSA" : c->key_type == X509_KEY_EC ? "Curva elíptica" : "?";
        if (c->key_type == X509_KEY_RSA) snprintf(l2, sizeof l2, "Chave pública: %s, %u bits", kt, rsa_modulus_bits(c->rsa_n, c->rsa_n_len));
        else snprintf(l2, sizeof l2, "Chave pública: %s (%u bytes)", kt, c->ec_pub_len);
        ui_label(u, det.x + 92, det.y + 108, l2, FONT_REGULAR, 12, T.text2);
    } else ui_label_center(u, rect_make(det.x, det.y + 60, det.w, 24), "Selecione um certificado", FONT_SEMIBOLD, 15, T.text3);
    /* tabela */
    rect_t hdr = rect_make(a.x, a.y + dh, a.w, 26);
    static const char *cols[3] = {"Nome", "Tipo", "Expira em"};
    int xs[3] = {hdr.x + 16, hdr.x + hdr.w / 2, hdr.x + hdr.w * 3 / 4};
    for (int i = 0; i < 3; i++) text_draw(s, font_get(FONT_SEMIBOLD), 12, xs[i], hdr.y + 18, T.text2, cols[i]);
    gfx_fill(s, hdr.x, hdr.y + hdr.h - 1, hdr.w, 1, T.separator);
    rect_t view = rect_make(a.x, hdr.y + hdr.h, a.w, a.h - dh - hdr.h);
    rect_t pc = surface_push_clip(s, view);
    int ry = view.y - KC.scroll, n = 0;
    u32 total = x509_trust_count();
    char q[64];
    snprintf(q, sizeof q, "%s", KC.search);
    for (char *p = q; *p; p++) *p = (char)tolower(*p);
    for (u32 i = 0; i < total; i++) {
        const x509_cert_t *c = x509_trust_get(i);
        char cn[128];
        x509_name_cn(c->subject, c->subject_len, cn, sizeof cn);
        if (q[0]) { char l[128]; snprintf(l, sizeof l, "%s", cn); for (char *p = l; *p; p++) *p = (char)tolower(*p); if (!strstr(l, q)) continue; }
        if (ry + 24 > view.y && ry < view.y + view.h) {
            rect_t r = rect_make(a.x, ry, a.w, 24);
            bool sel = KC.sel == (int)i;
            ui_list_row(u, 120100 + (int)i, r, sel, n % 2);
            if (ui_clicked(u, 120100 + (int)i, r)) KC.sel = (int)i;
            u32 tc = sel ? RGB(255, 255, 255) : T.text;
            text_draw_ellipsis(s, font_get(FONT_REGULAR), 12.5f, xs[0], ry + 17, xs[1] - xs[0] - 10, tc, cn);
            text_draw(s, font_get(FONT_REGULAR), 12.5f, xs[1], ry + 17, sel ? tc : T.text2, "certificado raiz");
            char d[48];
            fmt_x509_date(c->not_after, d, sizeof d);
            text_draw_ellipsis(s, font_get(FONT_REGULAR), 12.5f, xs[2], ry + 17, a.x + a.w - xs[2] - 10, sel ? tc : T.text2, d);
        }
        ry += 24;
        n++;
    }
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, n * 24 + 10, &KC.scroll);
    char cnt[48];
    snprintf(cnt, sizeof cnt, "%d itens", n);
    ui_title(u, sb + 18, 0, 36, 300, "Raízes do Sistema", cnt);
}
static void kc_close(window_t *w) { KC.w = NULL; }
static void kc_launch(app_t *a) {
    if (KC.w) { wm_focus(KC.w); return; }
    KC.w = wm_create(&app_chaves, "Acesso às Chaves", -1, -1, 960, 620, WIN_RESIZABLE);
    if (!KC.w) return;
    KC.w->paint = kc_paint;
    KC.w->on_close = kc_close;
    KC.w->toolbar_h = TOOLBAR_H;
    KC.w->sidebar_w = 220;
    KC.w->title_x = -1;
}
static void kc_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(200, 200, 206), RGB(140, 140, 148));
    gfx_round_rect(s, x + 18 * u, y + 28 * u, 28 * u, 22 * u, 4 * u, RGB(240, 200, 80));
    gfx_ring_arc(s, x + 32 * u, y + 28 * u, 9 * u, 4 * u, PI_F, 2 * PI_F, RGB(220, 220, 228));
    gfx_fill(s, (int)(x + 22 * u), (int)(y + 22 * u), (int)(3.5f * u), (int)(7 * u), RGB(220, 220, 228));
    gfx_fill(s, (int)(x + 38.5f * u), (int)(y + 22 * u), (int)(3.5f * u), (int)(7 * u), RGB(220, 220, 228));
    gfx_circle(s, x + 32 * u, y + 38 * u, 3 * u, RGB(120, 90, 20));
}
app_t app_chaves = {.name = "Acesso às Chaves", .exec = "chaves", .draw_icon = kc_icon, .launch = kc_launch};

/* ================================================================== Configuração de Áudio e MIDI */
static struct { window_t *w; int sel; float vol, in_vol; bool mon; } AM = {.vol = -1};
static void glyph_speaker_small(surface_t *s, int x, int y, int sz, u32 c) { glyph_speaker(s, x, y, sz, c, 2); }

static void am_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    int sb = w->sidebar_w;
    int y = w->toolbar_h + 4;
    bool has_in = audio_input_available();
    if (ui_sidebar_item(u, 121000, rect_make(10, y, sb - 20, 44), audio_device_name(), AM.sel == 0, glyph_speaker_small)) AM.sel = 0;
    text_draw(s, font_get(FONT_REGULAR), 11, 48, y + 38, T.text2, "0 entradas / 2 saídas");
    y += 50;
    if (has_in) {
        if (ui_sidebar_item(u, 121001, rect_make(10, y, sb - 20, 44), audio_input_name(), AM.sel == 1, NULL)) AM.sel = 1;
        text_draw(s, font_get(FONT_REGULAR), 11, 48, y + 38, T.text2, "2 entradas / 0 saídas");
    }
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg);
    if (AM.vol < 0) { AM.vol = (float)audio_get_volume(); AM.in_vol = (float)audio_get_input_volume(); AM.mon = audio_get_input_monitor(); }
    int x0 = a.x + 24, yy = a.y + 20;
    ui_label(u, x0, yy, AM.sel == 0 ? audio_device_name() : audio_input_name(), FONT_BOLD, 18, T.text);
    yy += 30;
    ui_label(u, x0, yy, "Origem do relógio: padrão · Formato: 2 canais, 16 bits, 48,0 kHz", FONT_REGULAR, 12.5f, T.text2);
    yy += 36;
    rect_t g = rect_make(x0, yy, a.w - 48, 3 * 40);
    ui_group_box(u, g);
    rect_t c = ui_form_row(u, g, 0, 40, 3, AM.sel == 0 ? "Volume principal" : "Ganho de entrada");
    float *v = AM.sel == 0 ? &AM.vol : &AM.in_vol;
    float old = *v;
    ui_slider(u, UI_ID, rect_make(c.x + 10, c.y + 9, c.w - 80, 22), v, 0, AM.sel == 0 ? 100 : 200);
    char t[16];
    snprintf(t, sizeof t, "%d%%", (int)*v);
    ui_detail(u, UI_ID, rect_make(c.x + c.w - 64, c.y, 64, c.h), t, DETAIL_NONE);
    if (old != *v) { if (AM.sel == 0) audio_set_volume((int)*v); else audio_set_input_volume((int)*v); }
    c = ui_form_row(u, g, 1, 40, 3, "Formato");
    static const char *fm[1] = {"2 canais, 16 bits, 48,0 kHz"};
    ui_popup_inline(u, UI_ID, c, fm, 1, 0);
    c = ui_form_row(u, g, 2, 40, 3, AM.sel == 0 ? "Teste de alto-falantes" : "Ouvir a entrada");
    if (AM.sel == 0) {
        if (ui_button_style(u, UI_ID, rect_make(c.x + c.w - 190, c.y + 6, 90, 28), "Esquerdo", BTN_GRAY, true, NULL)) synth_note(220, 400, 0, 0.6f);
        if (ui_button_style(u, UI_ID, rect_make(c.x + c.w - 94, c.y + 6, 90, 28), "Direito", BTN_GRAY, true, NULL)) synth_note(1760, 400, 0, 0.6f);
    } else {
        bool m = AM.mon;
        if (ui_switch_ex(u, UI_ID, c.x + c.w - 50, c.y + 10, 20, &m, true)) { AM.mon = m; audio_set_input_monitor(m); }
    }
    yy += g.h + 20;
    /* medidor */
    if (AM.sel == 1 && has_in) {
        ui_label(u, x0, yy, "Nível de entrada", FONT_SEMIBOLD, 12.5f, T.text);
        static float lvl;
        i16 buf[1024 * 2];
        u32 got, mx = 0;
        if (!AM.mon) while ((got = audio_capture_read(buf, 1024)) > 0) for (u32 i = 0; i < got * 2; i++) { int x = buf[i] < 0 ? -buf[i] : buf[i]; if ((u32)x > mx) mx = x; }
        lvl = MAX(mx / 32768.0f, lvl * 0.9f);
        rect_t m = rect_make(x0, yy + 22, a.w - 48, 12);
        gfx_round_rect(s, m.x, m.y, m.w, m.h, 6, T.dark ? RGBA(255, 255, 255, 24) : RGBA(0, 0, 0, 14));
        gfx_round_rect(s, m.x, m.y, MAX(12, (int)(m.w * lvl)), m.h, 6, lvl > 0.8f ? RGB(255, 69, 58) : RGB(48, 199, 89));
        u->repaint = true;
    }
    ui_label(u, x0, a.y + a.h - 30, "Dispositivos MIDI: nenhum conectado.", FONT_REGULAR, 12, T.text3);
}
static void am_close(window_t *w) { AM.w = NULL; AM.vol = -1; }
static void am_launch(app_t *a) {
    if (AM.w) { wm_focus(AM.w); return; }
    AM.w = wm_create(&app_audiomidi, "Dispositivos de Áudio", -1, -1, 820, 460, WIN_RESIZABLE);
    if (!AM.w) return;
    AM.w->paint = am_paint;
    AM.w->on_close = am_close;
    AM.w->toolbar_h = TOOLBAR_H;
    AM.w->sidebar_w = 260;
}
static void am_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(220, 222, 228), RGB(170, 172, 180));
    for (int i = 0; i < 7; i++) gfx_fill(s, (int)(x + (13 + i * 6) * u), (int)(y + 36 * u), (int)(5 * u), (int)(16 * u), RGB(250, 250, 250));
    for (int i = 0; i < 6; i++) if (i != 2) gfx_fill(s, (int)(x + (16.5f + i * 6) * u), (int)(y + 36 * u), (int)(3 * u), (int)(9 * u), RGB(30, 30, 34));
    for (int i = 0; i < 3; i++) gfx_circle(s, x + (20 + i * 12) * u, y + 22 * u, 4 * u, RGB(60, 60, 66));
}
app_t app_audiomidi = {.name = "Configuração de Áudio e MIDI", .exec = "audiomidi", .draw_icon = am_icon, .launch = am_launch};
