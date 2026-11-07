/* mOS — Editor de Script (JavaScript no motor do sistema: executar, resultado, mensagens do console,
 * scripts em ~/Documentos/Scripts) e Senhas (cofre cifrado com ChaCha20-Poly1305, chave derivada da senha
 * da conta; contas, códigos de verificação TOTP, gerador de senhas fortes, alertas de segurança). */
#include "kit.h"
#include "js.h"
#include "crypto.h"
#include "account.h"

extern app_t app_script, app_senhas;

/* ================================================================== Editor de Script */
static struct {
    window_t *w;
    textarea_t *ta;
    char path[200], name[96];
    js_page_t *page;
    doc_t *doc;
    int tab;                  /* 0 resultado, 1 mensagens */
    char result[2048];
    bool running;
    u64 started;
    int rscroll, sscroll;
} E;

static const char *sdir(void) { static char d[200]; snprintf(d, sizeof d, "%s/Documentos/Scripts", CUR_HOME); vfs_mkdir(d); return d; }
static void e_stop(void) {
    if (E.page) { js_page_free(E.page); E.page = NULL; }
    if (E.doc) { doc_free(E.doc); E.doc = NULL; }
    E.running = false;
}
static void e_run(void) {
    e_stop();
    E.result[0] = 0;
    static const char *html = "<html><head><title>Script</title></head><body></body></html>";
    E.doc = doc_parse(html, strlen(html), "about:script");
    if (!E.doc) return;
    E.page = js_page_new(E.doc, "about:script", 800, 600);
    if (!E.page) return;
    int len;
    const char *code = textarea_text(E.ta, &len);
    /* avalia como string para mostrar o valor da última expressão, como no Editor de Script */
    sbuf_t b = {0};
    sb_puts(&b, "var __r = eval(\"");
    for (int i = 0; i < len; i++) {
        char c = code[i];
        if (c == '"' || c == '\\') { sb_putc(&b, '\\'); sb_putc(&b, c); }
        else if (c == '\n') sb_puts(&b, "\\n");
        else if (c == '\r') {}
        else if (c == '\t') sb_puts(&b, "\\t");
        else sb_putc(&b, c);
    }
    sb_puts(&b, "\");\nif (__r !== undefined) console.log('⇒ ' + (typeof __r === 'object' && __r !== null ? JSON.stringify(__r) : String(__r)));\n");
    E.running = true;
    E.started = timer_ticks();
    js_page_eval(E.page, b.p, b.len, E.name[0] ? E.name : "Sem título");
    sb_free(&b);
    E.tab = js_page_errors(E.page) ? 1 : 0;
}
static void e_save(void) {
    if (!E.name[0]) {
        int k = 1;
        do snprintf(E.path, sizeof E.path, "%s/Script %d.js", sdir(), k++); while (vfs_lookup(E.path));
        snprintf(E.name, sizeof E.name, "%s", strrchr(E.path, '/') + 1);
    }
    int len;
    const char *t = textarea_text(E.ta, &len);
    vfs_write_file(E.path, t, len);
    textarea_dirty(E.ta, true);
}
static void e_open(const char *path) {
    char *b = kit_read(path, NULL);
    textarea_set(E.ta, b ? b : "", -1);
    if (b) kfree(b);
    snprintf(E.path, sizeof E.path, "%s", path);
    snprintf(E.name, sizeof E.name, "%s", strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
    e_stop();
    E.result[0] = 0;
}

static void e_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    int sb = w->sidebar_w;
    /* barra lateral: scripts salvos */
    int y = w->toolbar_h + 4;
    ui_label(u, 18, y + 2, "Scripts", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    vnode_t *d = vfs_lookup(sdir());
    int i = 0;
    for (vnode_t *c = d && d->dir ? d->child : NULL; c; c = c->next, i++) {
        if (c->dir) continue;
        if (ui_sidebar_item(u, 117000 + i, rect_make(10, y, sb - 20, 26), c->name, !strcmp(E.name, c->name), glyph_sb_document)) {
            if (textarea_dirty(E.ta, false) && E.name[0]) e_save();
            char p[300];
            vfs_path(c, p, sizeof p);
            e_open(p);
        }
        y += 28;
    }
    if (ui_button_style(u, UI_ID, rect_make(10, u->ch - 36, 130, 28), "Novo Script", BTN_PLAIN_GRAY, true, glyph_plus)) {
        if (textarea_dirty(E.ta, false) && E.name[0]) e_save();
        E.name[0] = E.path[0] = 0;
        textarea_set(E.ta, "// JavaScript\n", -1);
        e_stop();
        E.result[0] = 0;
    }
    /* barra de ferramentas */
    int tx = sb + 14;
    if (ui_button_style(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 96, 28), "Executar", BTN_FILLED, true, NULL)) e_run();
    if (ui_button_style(u, UI_ID, rect_make(tx + 104, TOOLBAR_Y(28), 80, 28), "Parar", BTN_GRAY, E.running, NULL)) e_stop();
    static const char *lang[1] = {"JavaScript"};
    ui_popup(u, UI_ID, rect_make(tx + 192, TOOLBAR_Y(28), 120, 28), lang, 1, 0);
    char title[120];
    snprintf(title, sizeof title, "%s%s", E.name[0] ? E.name : "Sem título", textarea_dirty(E.ta, false) ? " — Editado" : "");
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 90, TOOLBAR_Y(28), 90, 28), "Salvar", true)) e_save();
    wm_set_title(w, title);
    /* editor (fonte monoespaçada não existe: usa a Inter em 14) */
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    int eh = a.h * 3 / 5;
    textarea_paint(w, u, E.ta, rect_make(a.x, a.y, a.w, eh));
    /* painel inferior */
    rect_t p = rect_make(a.x, a.y + eh, a.w, a.h - eh);
    gfx_fill(s, p.x, p.y, p.w, p.h, T.dark ? RGB(30, 30, 32) : RGB(246, 246, 248));
    gfx_fill(s, p.x, p.y, p.w, 1, T.separator);
    static const char *tabs[2] = {"Resultado", "Mensagens"};
    E.tab = ui_segmented(u, UI_ID, rect_make(p.x + (p.w - 220) / 2, p.y + 8, 220, 24), tabs, 2, E.tab);
    if (E.page) {
        if (js_page_tick(E.page)) u->repaint = true;
        if (js_page_next_ms(E.page) < 0 && timer_ticks() - E.started > 300) E.running = false;
        else u->repaint = true;
    }
    const char *con = E.page ? js_page_console(E.page) : "";
    rect_t tv = rect_make(p.x + 14, p.y + 40, p.w - 28, p.h - 48);
    rect_t pc = surface_push_clip(s, tv);
    if (E.tab == 0) {
        /* só o valor final (linhas que começam com ⇒) */
        const char *r = strstr(con, "\xE2\x87\x92");
        const char *last = NULL;
        while (r) { last = r; r = strstr(r + 3, "\xE2\x87\x92"); }
        const char *show = last ? last + 4 : (E.page ? (js_page_errors(E.page) ? "Erro — veja Mensagens." : "") : "Clique em Executar para rodar o script.");
        text_wrap(s, font_get(FONT_MEDIUM), 13.5f, rect_make(tv.x, tv.y - E.rscroll, tv.w, 4000), 20, E.page && js_page_errors(E.page) && !last ? RGB(255, 69, 58) : T.text, show, true);
    } else {
        int h = text_wrap(s, font_get(FONT_REGULAR), 12.5f, rect_make(tv.x, tv.y - E.sscroll, tv.w, 20000), 18, T.text, con[0] ? con : "Sem mensagens.", true);
        surface_pop_clip(s, pc);
        ui_scroll(u, UI_ID, tv, h + 10, &E.sscroll);
        pc = surface_push_clip(s, tv);
    }
    surface_pop_clip(s, pc);
    if ((u->mods & MOD_CMD) && u->key == 'r') { e_run(); u->key_used = true; }
    if ((u->mods & MOD_CMD) && u->key == 's') { e_save(); u->key_used = true; }
    if ((u->mods & MOD_CMD) && u->key == '.') { e_stop(); u->key_used = true; }
}
static void e_close(window_t *w) { if (textarea_dirty(E.ta, false) && E.name[0]) e_save(); e_stop(); E.w = NULL; }
static void e_launch(app_t *a) {
    if (E.w) { wm_focus(E.w); return; }
    if (!E.ta) {
        E.ta = textarea_new(14, 0, 0, 14);
        char p[240];
        snprintf(p, sizeof p, "%s/Olá.js", sdir());
        if (!vfs_lookup(p)) {
            const char *demo = "// Editor de Script do mOS — JavaScript\n// ⌘R executa. console.log aparece em Mensagens.\n\n"
                               "function fatorial(n) { return n <= 1 ? 1 : n * fatorial(n - 1); }\n"
                               "for (let i = 1; i <= 5; i++) console.log(i + '! = ' + fatorial(i));\n\n"
                               "const hoje = new Date();\n['Olá', 'mOS', hoje.getFullYear()].join(' ')\n";
            vfs_write_file(p, demo, strlen(demo));
        }
        e_open(p);
    }
    E.w = wm_create(&app_script, "Editor de Script", -1, -1, 940, 640, WIN_RESIZABLE);
    if (!E.w) return;
    E.w->paint = e_paint;
    E.w->on_close = e_close;
    E.w->toolbar_h = TOOLBAR_H;
    E.w->sidebar_w = 200;
    E.w->title_x = -1;
}
static void e_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    gfx_shadow(s, (int)(x + 12 * u), (int)(y + 8 * u), (int)(40 * u), (int)(50 * u), 4 * u, 3 * u, 70, (int)u);
    gfx_round_rect_grad(s, x + 12 * u, y + 8 * u, 40 * u, 50 * u, 4 * u, RGB(250, 250, 252), RGB(222, 222, 228));
    for (int i = 0; i < 5; i++) gfx_fill(s, (int)(x + 18 * u), (int)(y + (16 + i * 6) * u), (int)((i % 2 ? 20 : 28) * u), MAX(1, (int)(1.4f * u)), RGB(150, 150, 160));
    /* pergaminho/caneta: um "S" estilizado */
    gfx_circle(s, x + 44 * u, y + 46 * u, 11 * u, RGB(90, 90, 100));
    text_draw_center(s, font_get(FONT_BOLD), 13 * u, rect_make((int)(x + 33 * u), (int)(y + 35 * u), (int)(22 * u), (int)(22 * u)), RGB(255, 255, 255), "JS");
}
app_t app_script = {.name = "Editor de Script", .exec = "script", .draw_icon = e_icon, .launch = e_launch};

/* ================================================================== Senhas */
#define MAXPW 200
typedef struct { bool used; char site[96], user[96], pass[96], notes[200], totp[64]; i64 created; } pw_t;
static struct {
    window_t *w;
    pw_t p[MAXPW];
    bool unlocked, loaded;
    u8 key[32];
    char lockpw[96], search[64];
    int sel, cat, scroll;
    bool reveal, editing, failed;
    pw_t backup;
} S = {.sel = -1};

/* HMAC-SHA1 (RFC 2104) para TOTP (RFC 6238) */
static void hmac_sha1(const u8 *key, int klen, const u8 *msg, int mlen, u8 out[20]) {
    u8 k[64] = {0}, ip[64 + 64], op[64 + 20];
    if (klen > 64) { sha1(key, klen, k); klen = 20; } else memcpy(k, key, klen);
    for (int i = 0; i < 64; i++) { ip[i] = k[i] ^ 0x36; op[i] = k[i] ^ 0x5c; }
    u8 *tmp = kmalloc(64 + mlen);
    memcpy(tmp, ip, 64);
    memcpy(tmp + 64, msg, mlen);
    u8 inner[20];
    sha1(tmp, 64 + mlen, inner);
    kfree(tmp);
    memcpy(op + 64, inner, 20);
    sha1(op, 84, out);
}
static int base32_decode(const char *in, u8 *out, int cap) {
    int bits = 0, val = 0, n = 0;
    for (; *in && n < cap; in++) {
        char c = (char)toupper(*in);
        int v = c >= 'A' && c <= 'Z' ? c - 'A' : c >= '2' && c <= '7' ? c - '2' + 26 : -1;
        if (v < 0) continue;
        val = (val << 5) | v;
        bits += 5;
        if (bits >= 8) { out[n++] = (u8)(val >> (bits - 8)); bits -= 8; }
    }
    return n;
}
extern int g_tz_offset_min;
static int totp_code(const char *secret, int *left) {
    u8 key[64];
    int kl = base32_decode(secret, key, sizeof key);
    if (kl <= 0) return -1;
    i64 utc = kit_now_secs() - g_tz_offset_min * 60;
    u64 ctr = (u64)utc / 30;
    if (left) *left = 30 - (int)(utc % 30);
    u8 msg[8];
    for (int i = 7; i >= 0; i--) { msg[i] = (u8)ctr; ctr >>= 8; }
    u8 h[20];
    hmac_sha1(key, kl, msg, 8, h);
    int o = h[19] & 15;
    u32 bin = ((u32)(h[o] & 0x7f) << 24) | (u32)h[o + 1] << 16 | (u32)h[o + 2] << 8 | h[o + 3];
    return (int)(bin % 1000000);
}

static void derive(const char *pw) {
    /* derivação lenta: 20 000 rodadas de SHA-256 sobre (sal ‖ senha ‖ anterior) */
    u8 h[32] = {0};
    char salt[64];
    snprintf(salt, sizeof salt, "mOS-Senhas:%s", account_current() ? account_current() : "");
    sha256_ctx c;
    for (int i = 0; i < 20000; i++) {
        sha256_init(&c);
        sha256_update(&c, salt, strlen(salt));
        sha256_update(&c, pw, strlen(pw));
        sha256_update(&c, h, 32);
        sha256_final(&c, h);
    }
    memcpy(S.key, h, 32);
    secure_zero(h, 32);
}
static void pw_save(void) {
    if (!S.unlocked) return;
    sbuf_t b = {0};
    for (int i = 0; i < MAXPW; i++) {
        pw_t *x = &S.p[i];
        if (!x->used) continue;
        char a[96], c[96], d[96], t[64];
        kit_field(a, 96, x->site); kit_field(c, 96, x->user); kit_field(d, 96, x->pass); kit_field(t, 64, x->totp);
        sb_printf(&b, "P\t%s\t%s\t%s\t%s\t%lld\t", a, c, d, t, (long long)x->created);
        kit_esc(&b, x->notes);
        sb_putc(&b, '\n');
    }
    /* arquivo: "MOSPW1" | nonce(12) | tag(16) | dados cifrados */
    usize n = b.len;
    u8 *out = kmalloc(6 + 12 + 16 + n + 1);
    memcpy(out, "MOSPW1", 6);
    u8 *nonce = out + 6, *tag = out + 18;
    for (int i = 0; i < 12; i++) nonce[i] = (u8)rand32();
    chacha20poly1305_seal(S.key, nonce, (const u8 *)"MOSPW1", 6, (const u8 *)(b.p ? b.p : ""), n, out + 34, tag);
    char p[200];
    vfs_write_file(kit_lib("Senhas", "cofre.bin", p, sizeof p), out, 34 + n);
    secure_zero(b.p, b.len);
    sb_free(&b);
    kfree(out);
}
/* devolve false se a senha não abre o cofre */
static bool pw_load(void) {
    char p[200];
    vnode_t *v = vfs_lookup(kit_lib("Senhas", "cofre.bin", p, sizeof p));
    memset(S.p, 0, sizeof S.p);
    if (!v || v->size < 34) return true;   /* cofre novo */
    if (memcmp(v->data, "MOSPW1", 6)) return false;
    usize n = v->size - 34;
    char *plain = kmalloc(n + 1);
    if (chacha20poly1305_open(S.key, v->data + 6, (const u8 *)"MOSPW1", 6, v->data + 34, n, (u8 *)plain, v->data + 18)) { kfree(plain); return false; }
    plain[n] = 0;
    char *cur = plain, *ln, *f[8];
    int k = 0;
    while ((ln = kit_line(&cur)) && k < MAXPW) {
        int nf = kit_split(ln, f, 8);
        if (strcmp(f[0], "P") || nf < 7) continue;
        pw_t *x = &S.p[k++];
        x->used = true;
        snprintf(x->site, 96, "%s", f[1]); snprintf(x->user, 96, "%s", f[2]); snprintf(x->pass, 96, "%s", f[3]); snprintf(x->totp, 64, "%s", f[4]);
        x->created = strtol(f[5], NULL, 10);
        kit_unesc(f[6]);
        snprintf(x->notes, 200, "%s", f[6]);
    }
    secure_zero(plain, n);
    kfree(plain);
    return true;
}
static void unlock(const char *pw) {
    if (!account_verify(account_current(), pw)) { S.failed = true; return; }
    derive(pw);
    if (!pw_load()) { S.failed = true; secure_zero(S.key, 32); return; }
    S.unlocked = true;
    S.failed = false;
}
static void lock(void) { if (S.unlocked) pw_save(); S.unlocked = false; secure_zero(S.key, 32); memset(S.p, 0, sizeof S.p); S.sel = -1; S.lockpw[0] = 0; S.editing = false; }

static void gen_password(char *out, int n) {
    static const char *set = "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    int k = 0;
    for (int g = 0; g < 3 && k < n - 8; g++) {
        for (int i = 0; i < 6; i++) out[k++] = set[rand32() % 57];
        if (g < 2) out[k++] = '-';
    }
    out[k] = 0;
}
static int strength(const char *p) {
    int l = (int)strlen(p), cls = 0;
    bool lo = false, up = false, di = false, sy = false;
    for (; *p; p++) { if (*p >= 'a' && *p <= 'z') lo = true; else if (*p >= 'A' && *p <= 'Z') up = true; else if (isdigit((u8)*p)) di = true; else sy = true; }
    cls = lo + up + di + sy;
    return l >= 14 && cls >= 3 ? 2 : l >= 10 && cls >= 2 ? 1 : 0;
}
static bool reused(int i) { for (int k = 0; k < MAXPW; k++) if (k != i && S.p[k].used && S.p[i].pass[0] && !strcmp(S.p[k].pass, S.p[i].pass)) return true; return false; }
static bool in_cat(int i) {
    pw_t *x = &S.p[i];
    if (!x->used) return false;
    if (S.search[0]) {
        char h[200], q[64];
        snprintf(h, sizeof h, "%s %s", x->site, x->user);
        snprintf(q, sizeof q, "%s", S.search);
        for (char *c = h; *c; c++) *c = (char)tolower(*c);
        for (char *c = q; *c; c++) *c = (char)tolower(*c);
        return strstr(h, q) != NULL;
    }
    switch (S.cat) {
    case 1: return x->pass[0];
    case 2: return x->totp[0];
    case 3: return x->pass[0] && (strength(x->pass) == 0 || reused(i));
    default: return true;
    }
}

static void lock_screen(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    gfx_fill(s, 0, 0, u->cw, u->ch, T.content_bg);
    int cx = u->cw / 2, cy = u->ch / 2 - 60;
    gfx_round_rect_grad(s, cx - 34, cy - 34, 68, 68, 16, RGB(90, 200, 120), RGB(40, 150, 90));
    /* chave */
    gfx_circle_stroke(s, cx - 8, cy - 4, 10, 4, RGB(255, 255, 255));
    gfx_line(s, cx + 2, cy + 4, cx + 18, cy + 18, 4, RGB(255, 255, 255));
    gfx_line(s, cx + 12, cy + 14, cx + 18, cy + 8, 4, RGB(255, 255, 255));
    ui_label_center(u, rect_make(0, cy + 48, u->cw, 26), "Senhas está bloqueado", FONT_BOLD, 20, T.text);
    ui_label_center(u, rect_make(0, cy + 78, u->cw, 18), "Digite a senha da sua conta para desbloquear.", FONT_REGULAR, 13, T.text2);
    bool has_pw = false;
    char nm[64], fn[64];
    bool adm;
    for (int i = 0; i < account_count(); i++) if (account_get(i, nm, 64, fn, 64, &adm, &has_pw) && !strcmp(nm, account_current())) break;
    rect_t f = rect_make(cx - 140, cy + 110, 280, 30);
    bool go = ui_textfield(u, 118900, f, S.lockpw, sizeof S.lockpw, has_pw ? "Senha" : "Conta sem senha: tecle Return");
    if (u->focus != 118900 && !u->pressed) u->focus = 118900;
    if (S.failed) ui_label_center(u, rect_make(0, cy + 146, u->cw, 18), "Senha incorreta.", FONT_SEMIBOLD, 12.5f, RGB(255, 69, 58));
    if (go || ui_button_style(u, UI_ID, rect_make(cx - 60, cy + 170, 120, 30), "Desbloquear", BTN_FILLED, true, NULL)) { unlock(S.lockpw); secure_zero(S.lockpw, sizeof S.lockpw); }
}

static void s_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    if (!S.unlocked) { w->sidebar_w = 0; lock_screen(w, u); return; }
    w->sidebar_w = 230;
    int sb = w->sidebar_w;
    int y = w->toolbar_h + 4;
    ui_searchfield(u, UI_ID, rect_make(12, y, sb - 24, 28), S.search, sizeof S.search);
    y += 40;
    static const char *cats[4] = {"Todos", "Senhas", "Códigos", "Segurança"};
    static const u32 cc[4] = {RGB(10, 132, 255), RGB(142, 142, 147), RGB(48, 199, 89), RGB(255, 69, 58)};
    int cw = (sb - 32) / 2;
    for (int i = 0; i < 4; i++) {
        rect_t r = rect_make(12 + (i % 2) * (cw + 8), y + (i / 2) * 64, cw, 56);
        bool sel = S.cat == i && !S.search[0];
        gfx_round_rect(s, r.x, r.y, r.w, r.h, 10, sel ? cc[i] : (T.dark ? RGBA(255, 255, 255, 22) : RGBA(255, 255, 255, 200)));
        int n = 0, save_cat = S.cat;
        char sq[64];
        snprintf(sq, sizeof sq, "%s", S.search);
        S.search[0] = 0;
        S.cat = i;
        for (int k = 0; k < MAXPW; k++) if (in_cat(k)) n++;
        S.cat = save_cat;
        snprintf(S.search, sizeof S.search, "%s", sq);
        char nn[8];
        snprintf(nn, sizeof nn, "%d", n);
        int tw = text_width(font_get(FONT_BOLD), 18, nn);
        text_draw(s, font_get(FONT_BOLD), 18, r.x + r.w - 10 - tw, r.y + 24, sel ? RGB(255, 255, 255) : T.text, nn);
        gfx_circle(s, r.x + 18, r.y + 18, 9, sel ? RGBA(255, 255, 255, 80) : cc[i]);
        text_draw(s, font_get(FONT_SEMIBOLD), 12.5f, r.x + 10, r.y + 47, sel ? RGB(255, 255, 255) : T.text2, cats[i]);
        if (ui_clicked(u, 118000 + i, r)) { S.cat = i; S.search[0] = 0; S.sel = -1; S.editing = false; }
    }
    y += 2 * 64 + 8;
    /* lista */
    int lw = 260;
    rect_t lv = rect_make(sb, w->toolbar_h, lw, u->ch - w->toolbar_h);
    gfx_fill(s, lv.x, lv.y, lv.w, lv.h, T.content_bg);
    gfx_fill(s, lv.x + lv.w - 1, 0, 1, u->ch, T.separator);
    rect_t pc = surface_push_clip(s, lv);
    int ly = lv.y + 6 - S.scroll, n = 0;
    for (int i = 0; i < MAXPW; i++) {
        if (!in_cat(i)) continue;
        pw_t *x = &S.p[i];
        rect_t r = rect_make(lv.x, ly, lv.w, 50);
        bool sel = S.sel == i;
        ui_list_row(u, 118100 + i, r, sel, false);
        if (ui_clicked(u, 118100 + i, r)) { if (S.editing) pw_save(); S.sel = i; S.reveal = false; S.editing = false; }
        char ini[4] = {(char)toupper(x->site[0] ? x->site[0] : '?'), 0};
        gfx_round_rect(s, r.x + 18, r.y + 9, 32, 32, 8, sel ? RGBA(255, 255, 255, 60) : RGB(160, 165, 180));
        text_draw_center(s, font_get(FONT_BOLD), 15, rect_make(r.x + 18, r.y + 9, 32, 32), RGB(255, 255, 255), ini);
        text_draw_ellipsis(s, font_get(FONT_SEMIBOLD), 13.5f, r.x + 60, r.y + 22, r.w - 76, sel ? RGB(255, 255, 255) : T.text, x->site[0] ? x->site : "Sem nome");
        text_draw_ellipsis(s, font_get(FONT_REGULAR), 12, r.x + 60, r.y + 39, r.w - 76, sel ? RGBA(255, 255, 255, 220) : T.text2, x->user);
        ly += 52;
        n++;
    }
    if (!n) ui_label_center(u, rect_make(lv.x, lv.y + 40, lv.w, 20), "Nenhuma senha", FONT_SEMIBOLD, 14, T.text3);
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, lv, n * 52 + 12, &S.scroll);
    /* barra de ferramentas */
    if (ui_button_icon(u, UI_ID, rect_make(u->cw - 12 - 32, TOOLBAR_Y(28), 32, 28), glyph_plus, true) || ((u->mods & MOD_CMD) && u->key == 'n' && !S.editing)) {
        u->key_used = true;
        for (int i = 0; i < MAXPW; i++) if (!S.p[i].used) {
            memset(&S.p[i], 0, sizeof S.p[i]);
            S.p[i].used = true;
            S.p[i].created = kit_now_secs();
            gen_password(S.p[i].pass, sizeof S.p[i].pass);
            S.sel = i; S.editing = true; S.backup = S.p[i]; S.cat = 0; S.search[0] = 0;
            break;
        }
    }
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 32 - 8 - 100, TOOLBAR_Y(28), 100, 28), "Bloquear", true)) { lock(); return; }
    /* detalhe */
    rect_t a = rect_make(sb + lw, w->toolbar_h, u->cw - sb - lw, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.dark ? RGB(30, 30, 32) : RGB(246, 246, 248));
    if (S.sel < 0 || !S.p[S.sel].used) { ui_label_center(u, rect_make(a.x, a.y + a.h / 2 - 12, a.w, 24), "Nenhum item selecionado", FONT_SEMIBOLD, 16, T.text3); return; }
    pw_t *x = &S.p[S.sel];
    int dy = a.y + 24, dx = a.x + 24, dw = a.w - 48;
    ui_label(u, dx, dy, x->site[0] ? x->site : "Novo Item", FONT_BOLD, 22, T.text);
    dy += 44;
    rect_t g = rect_make(dx, dy, dw, (S.editing ? 5 : 4) * 40);
    ui_group_box(u, g);
    int rows = S.editing ? 5 : 4;
    rect_t c = ui_form_row(u, g, 0, 40, rows, "Site ou app");
    if (S.editing) ui_textfield(u, UI_ID, rect_make(c.x + 8, c.y + 6, c.w - 16, 28), x->site, 96, "exemplo.com");
    else ui_detail(u, UI_ID, c, x->site, DETAIL_NONE);
    c = ui_form_row(u, g, 1, 40, rows, "Usuário");
    if (S.editing) ui_textfield(u, UI_ID, rect_make(c.x + 8, c.y + 6, c.w - 16, 28), x->user, 96, "nome@exemplo.com");
    else if (ui_detail(u, UI_ID, c, x->user, DETAIL_NONE)) { clipboard_set(x->user, -1); notify("Senhas", "Usuário copiado."); }
    c = ui_form_row(u, g, 2, 40, rows, "Senha");
    if (S.editing) {
        ui_textfield(u, UI_ID, rect_make(c.x + 8, c.y + 6, c.w - 16 - 96, 28), x->pass, 96, "Senha");
        if (ui_button_style(u, UI_ID, rect_make(c.x + c.w - 92, c.y + 6, 86, 28), "Gerar", BTN_TINTED, true, NULL)) gen_password(x->pass, sizeof x->pass);
    } else {
        char masked[100];
        int l = (int)strlen(x->pass);
        for (int i = 0; i < MIN(l, 20); i++) masked[i] = '*';
        masked[MIN(l, 20)] = 0;
        if (ui_detail(u, UI_ID, c, S.reveal ? x->pass : masked, DETAIL_NONE)) S.reveal = !S.reveal;
    }
    c = ui_form_row(u, g, 3, 40, rows, "Código de verificação");
    if (S.editing) ui_textfield(u, UI_ID, rect_make(c.x + 8, c.y + 6, c.w - 16, 28), x->totp, 64, "Chave de configuração (base32)");
    else if (x->totp[0]) {
        int left = 0, code = totp_code(x->totp, &left);
        char cs[32];
        if (code >= 0) snprintf(cs, sizeof cs, "%03d %03d  (%ds)", code / 1000, code % 1000, left); else snprintf(cs, sizeof cs, "chave inválida");
        if (ui_detail(u, UI_ID, c, cs, DETAIL_NONE) && code >= 0) { char cp[8]; snprintf(cp, 8, "%06d", code); clipboard_set(cp, -1); notify("Senhas", "Código copiado."); }
        u->repaint = true;
    } else ui_detail(u, UI_ID, c, "—", DETAIL_NONE);
    if (S.editing) { c = ui_form_row(u, g, 4, 40, rows, "Notas"); ui_textfield(u, UI_ID, rect_make(c.x + 8, c.y + 6, c.w - 16, 28), x->notes, 200, "Notas"); }
    dy += g.h + 16;
    if (!S.editing && x->pass[0]) {
        int st = strength(x->pass);
        bool ru = reused(S.sel);
        const char *msg = ru ? "Esta senha é usada em outra conta. Troque-a para manter as contas seguras." : st == 0 ? "Senha fraca: use 14 ou mais caracteres misturando letras, números e símbolos." : st == 1 ? "Senha razoável." : "Senha forte.";
        u32 col = ru || st == 0 ? RGB(255, 69, 58) : st == 1 ? RGB(255, 159, 10) : RGB(48, 199, 89);
        gfx_circle(s, dx + 7, dy + 9, 6, col);
        text_wrap(s, font_get(FONT_REGULAR), 12.5f, rect_make(dx + 20, dy, dw - 20, 40), 18, T.text2, msg, true);
        if (x->notes[0]) text_wrap(s, font_get(FONT_REGULAR), 13, rect_make(dx, dy + 40, dw, 80), 19, T.text, x->notes, true);
    }
    int by = a.y + a.h - 44;
    if (S.editing) {
        if (ui_button_style(u, UI_ID, rect_make(a.x + a.w - 24 - 80, by, 80, 28), "OK", BTN_FILLED | BTN_DEFAULT, true, NULL)) { S.editing = false; pw_save(); }
        if (ui_button_style(u, UI_ID, rect_make(a.x + a.w - 24 - 80 - 8 - 90, by, 90, 28), "Cancelar", BTN_GRAY, true, NULL)) {
            *x = S.backup;
            if (!x->site[0] && !x->user[0]) x->used = false;
            S.editing = false;
            pw_save();
        }
    } else {
        if (ui_button_style(u, UI_ID, rect_make(a.x + a.w - 24 - 80, by, 80, 28), "Editar", BTN_GRAY, true, NULL)) { S.backup = *x; S.editing = true; }
        if (ui_button_style(u, UI_ID, rect_make(a.x + 24, by, 90, 28), "Apagar", BTN_TINTED_RED, true, NULL)) { secure_zero(x, sizeof *x); S.sel = -1; pw_save(); }
        if (ui_button_style(u, UI_ID, rect_make(a.x + a.w - 24 - 80 - 8 - 130, by, 130, 28), "Copiar Senha", BTN_GRAY, x->pass[0], NULL)) { clipboard_set(x->pass, -1); notify("Senhas", "Senha copiada."); }
    }
}
static void s_close(window_t *w) { lock(); S.w = NULL; }
static void s_launch(app_t *a) {
    if (S.w) { wm_focus(S.w); return; }
    S.w = wm_create(&app_senhas, "Senhas", -1, -1, 960, 600, WIN_RESIZABLE);
    if (!S.w) return;
    S.w->paint = s_paint;
    S.w->on_close = s_close;
    S.w->toolbar_h = TOOLBAR_H;
    S.w->min_w = 820;
    S.w->min_h = 480;
    S.failed = false;
}
static void s_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(110, 220, 140), RGB(40, 160, 90));
    gfx_circle_stroke(s, x + 26 * u, y + 28 * u, 9 * u, 4 * u, RGB(255, 255, 255));
    gfx_line(s, x + 33 * u, y + 35 * u, x + 46 * u, y + 48 * u, 4 * u, RGB(255, 255, 255));
    gfx_line(s, x + 41 * u, y + 43 * u, x + 46 * u, y + 38 * u, 4 * u, RGB(255, 255, 255));
}
app_t app_senhas = {.name = "Senhas", .exec = "senhas", .draw_icon = s_icon, .launch = s_launch};

/* Launch Services: documento de script aberto pelo Arquivos ou por outro app */
void script_open_file(const char *path) { app_launch(&app_script); if (E.ta) e_open(path); }
