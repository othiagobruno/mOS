/*
 * mOS — Assistente de Configuração e Instalação (tela cheia, antes do login).
 *
 *   Mídia (live/pendrive):  Idioma → Teclado → Modo → { Live | Live com persistência (consentimento) | Instalar } → Usuário → Boas-vindas
 *   Instalar:               Destino → Layout (disco inteiro x particionar / tamanho no espaço livre) → Formatar antes? → Revisão
 *                           → Instalação → Concluída (reinicia sozinha em 15 s)
 *   Primeiro boot instalado: Usuário → Boas-vindas (o idioma/teclado gravados pelo instalador já valem; sem eles, Idioma →
 *                           Teclado antes)      (a conta é criada AQUI, sem o pendrive)
 *   App "Instalar mOS" (já no live): Destino → Layout → Formatar → Revisão → Instalação
 *
 * Regras que esta tela garante: nada é gravado em disco antes da confirmação digitada exata (instalação) ou do
 * consentimento explícito (persistência no pendrive); idioma e teclado valem antes de qualquer senha ser digitada;
 * a senha só é guardada como hash PBKDF2 (lib/account.c) e a tela NÃO promete criptografia nem permissões de arquivo.
 * Tudo funciona só com o teclado (Tab/Shift+Tab, setas, Espaço, Enter, Esc) e também com o mouse.
 */
#include "ui.h"
#include "apps.h"
#include "account.h"
#include "install.h"
#include "origin.h"
#include "sysstate.h"
#include "i18n.h"
#include "crypto.h"

enum { FL_MEDIUM, FL_FIRSTBOOT, FL_INSTALLER };
enum { SC_LANG, SC_KBD, SC_MODE, SC_PERSIST, SC_USER, SC_WELCOME, SC_TARGET, SC_LAYOUT, SC_FORMAT, SC_REVIEW, SC_RUN, SC_DONE, SC_FAIL };
enum { M_LIVE, M_PERSIST, M_INSTALL };
enum { K_BTN, K_ROW, K_FIELD, K_PW, K_CHECK };
enum { ID_BACK = 1, ID_NEXT, ID_EXTRA, ID_CHECK, ID_FNAME, ID_FUSER, ID_FPW1, ID_FPW2, ID_FTYPED, ID_FKBD, ID_MINUS, ID_PLUS, ID_ROW = 100 };
#define DONE_AUTO_MS 15000        /* tela "concluída" da mídia: reinicia sozinha depois disto */

typedef struct { int kind, id; rect_t r; bool en; } ctrl_t;

static bool active;
static int flow, step;
static int sel_lang, sel_kbd, sel_mode;
static char f_name[64], f_user[32], f_pw1[64], f_pw2[64], f_typed[40], f_kbd[64];
static char msg[200];
static bool consent_check, creating, session_ready;
static u64 blink0, last_repaint;
static surface_t *bg;
static ctrl_t ctrls[64];
static int nctrl, focus_id;
static int mx, my;
static int created_mode;          /* modo que levou ao usuário: M_LIVE ou M_PERSIST (para o texto de boas-vindas) */

/* ---- seleção de destino (instalação) */
typedef struct { int disk, part; /* part 0 = disco inteiro, -1 = espaço livre */ bool ok; int ir; gap_t gap; char t[110], s[170]; } trow_t;
static trow_t rows[64];
static int nrows, sel_row = -1, scroll;
static ptable_t *tabs[16];
static u8 sel_snap[32];
static int sel_mode_inst;
/* ---- 1º o DISCO (um item por disco físico), 2º o que fazer nele (opções daquele disco) */
static int drow[16], ndrow, sel_disk = -1; static bool dany[16];           /* índices em rows[] das linhas de disco (part == 0) */
enum { OP_WHOLE, OP_SPLIT, OP_GAP, OP_PART };
typedef struct { int kind, row; bool ok; char t[110], s[170]; } opt_t;
static opt_t opts[8];
static int nopt, sel_opt = -1;
/* ---- layout (disco inteiro x particionar; tamanho) e formatação */
static bool lay_split, zero_sel;
static u64 size_sel, size_min, size_max, size_step;
static u64 done_at;               /* quando a tela "concluída" apareceu (reinício automático) */
static bool done_hold;            /* o usuário pediu para ficar */

bool setup_active(void) { return active; }
bool setup_session_ready(void) { return session_ready; }

static int SW_(void) { return display_rect(0).w; }
static int SH_(void) { return display_rect(0).h; }

/* ------------------------------------------------------------------ utilidades */
static const char *lang_code(void) { return g_lang == LANG_EN ? "en" : "pt"; }
static const char *kbd_code(void) { return g_kbd_layout == 1 ? "us" : "abnt2"; }

static void fmt_size(u64 b, char *o, usize n) {
    if (b >= (1ull << 30)) snprintf(o, n, "%llu,%llu GB", b >> 30, ((b % (1ull << 30)) * 10) >> 30);
    else snprintf(o, n, "%llu MB", b >> 20);
}

static void free_tabs(void) {
    for (int i = 0; i < 16; i++) { kfree(tabs[i]); tabs[i] = NULL; }
}

/* ------------------------------------------------------------------ lista de destinos */
static void target_scan(void) {
    free_tabs();
    nrows = 0; sel_row = -1; scroll = 0;
    for (int d = 0; d < storage_disk_count() && d < 16 && nrows < 60; d++) {
        blkdev_t *bd = storage_disk(d);
        ptable_t *t = kmalloc(sizeof *t);
        bool scanned = t && partscan(bd, t);
        tabs[d] = scanned ? t : (kfree(t), NULL);
        char sz[32], kind[24];
        fmt_size(bd->size, sz, sizeof sz);
        const char *kn = !scanned ? "?" : t->kind == PT_GPT ? "GPT" : t->kind == PT_MBR ? "MBR" : t->kind == PT_BAD ? tr(S_TBL_BAD) : tr(S_TBL_NONE);
        snprintf(kind, sizeof kind, "%s", kn);
        trow_t *r = &rows[nrows++];
        memset(r, 0, sizeof *r);
        r->disk = d; r->part = 0;
        int ir = scanned ? install_check_disk(d, t) : IR_NO_TABLE;
        r->ok = ir == IR_OK; r->ir = ir;
        snprintf(r->t, sizeof r->t, "%s", storage_disk_desc(d));
        snprintf(r->s, sizeof r->s, "%s · %s · %s%s%s", storage_disk_name(d), sz, kind,
                 r->ok ? " · " : " — ", r->ok ? tr(S_ROW_WHOLE) : tr(install_reason_str(ir)));
        if (!scanned) continue;
        for (int i = 0; i < t->n && nrows < 62; i++) {
            const part_t *p = &t->p[i];
            trow_t *q = &rows[nrows++];
            memset(q, 0, sizeof *q);
            q->disk = d; q->part = p->number;
            int pr = install_check_part(d, t, i);
            q->ok = pr == IR_OK; q->ir = pr;
            char ps[32];
            fmt_size((p->last - p->first + 1) * 512, ps, sizeof ps);
            snprintf(q->t, sizeof q->t, "   %s %d — %s", tr(S_PARTITION), p->number, part_type_name(p));
            snprintf(q->s, sizeof q->s, "      %s · %s%s%s", ps, part_fs_name(p->fs),
                     q->ok ? " · " : " — ", q->ok ? tr(S_ROW_PART) : tr(install_reason_str(pr)));
        }
        /* espaços livres (GPT) de 64 MB ou mais: "particionar" sem apagar o que já existe */
        gap_t gs[8];
        int ng = part_free_gaps(t, gs, 8, (64ull << 20) / 512);
        for (int i = 0; i < ng && nrows < 63; i++) {
            trow_t *q = &rows[nrows++];
            memset(q, 0, sizeof *q);
            q->disk = d; q->part = -1; q->gap = gs[i];
            int gr = install_check_gap(d, t, &gs[i]);
            q->ok = gr == IR_OK; q->ir = gr;
            char ps[32];
            fmt_size((gs[i].last - gs[i].first + 1) * 512, ps, sizeof ps);
            snprintf(q->t, sizeof q->t, "   %s — %s", tr(S_ROW_GAP), ps);
            snprintf(q->s, sizeof q->s, "      %s%s", q->ok ? "" : "— ", q->ok ? tr(S_ROW_GAP_OK) : tr(install_reason_str(gr)));
        }
    }
    /* seletor de disco: um item por disco, com um resumo; o disco é elegível se ALGUMA opção nele for */
    ndrow = 0; sel_disk = -1;
    for (int i = 0; i < nrows && ndrow < 16; i++) {
        if (rows[i].part != 0) continue;
        int d = rows[i].disk, nparts = 0, firstbad = rows[i].ok ? -1 : rows[i].ir;
        u64 freeb = 0;
        bool any = rows[i].ok;
        for (int j = i + 1; j < nrows && rows[j].disk == d; j++) {
            if (rows[j].part > 0) nparts++;
            else freeb += (rows[j].gap.last - rows[j].gap.first + 1) * 512;
            if (rows[j].ok) any = true;
        }
        ptable_t *t = tabs[d];
        char sz[32], fr[32];
        fmt_size(storage_disk(d)->size, sz, sizeof sz);
        fmt_size(freeb, fr, sizeof fr);
        const char *kn = !t ? "?" : t->kind == PT_GPT ? "GPT" : t->kind == PT_MBR ? "MBR" : t->kind == PT_BAD ? tr(S_TBL_BAD) : tr(S_TBL_NONE);
        int o = snprintf(rows[i].s, sizeof rows[i].s, "%s · %s · %s", storage_disk_name(d), sz, kn);
        if (nparts) o += snprintf(rows[i].s + o, sizeof rows[i].s - o, " · %d %s", nparts, nparts == 1 ? tr(S_DISK_PART1) : tr(S_DISK_PARTN));
        if (freeb) o += snprintf(rows[i].s + o, sizeof rows[i].s - o, " · %s %s", fr, tr(S_DISK_FREE));
        if (!any) snprintf(rows[i].s + o, sizeof rows[i].s - o, " — %s", tr(install_reason_str(firstbad >= 0 ? firstbad : IR_NOT_DEDICATED)));
        dany[ndrow] = any;
        drow[ndrow++] = i;
        if (any && sel_disk < 0) sel_disk = ndrow - 1;
    }
}

static bool disk_eligible(int k) { return k >= 0 && k < ndrow && dany[k]; }

static u64 disk_usable(int disk);
/* opções do disco escolhido: inteiro, particionar, cada espaço livre, cada partição existente */
static void opts_build(void) {
    nopt = 0; sel_opt = -1;
    if (sel_disk < 0) return;
    int i = drow[sel_disk], d = rows[i].disk;
    u64 need = install_min_bytes();
    char sz[32];
    fmt_size(storage_disk(d)->size, sz, sizeof sz);
    opt_t *o = &opts[nopt++];
    o->kind = OP_WHOLE; o->row = i; o->ok = rows[i].ok;
    snprintf(o->t, sizeof o->t, "%s (%s)", tr(S_LAYOUT_WHOLE), sz);
    snprintf(o->s, sizeof o->s, "%s", o->ok ? tr(S_LAYOUT_WHOLE_SUB) : tr(install_reason_str(rows[i].ir)));
    o = &opts[nopt++];
    o->kind = OP_SPLIT; o->row = i;
    bool fits = disk_usable(d) >= need + INSTALL_DATA_MIN;
    o->ok = rows[i].ok && fits;
    snprintf(o->t, sizeof o->t, "%s", tr(S_LAYOUT_SPLIT));
    snprintf(o->s, sizeof o->s, "%s", o->ok ? tr(S_LAYOUT_SPLIT_SUB) : !rows[i].ok ? opts[0].s : tr(S_IR_TOO_SMALL));
    for (int j = i + 1; j < nrows && rows[j].disk == d && nopt < (int)ARRAY_LEN(opts); j++) {
        o = &opts[nopt++];
        o->kind = rows[j].part < 0 ? OP_GAP : OP_PART; o->row = j; o->ok = rows[j].ok;
        const char *t = rows[j].t, *s = rows[j].s;
        while (*t == ' ') t++;
        while (*s == ' ') s++;
        snprintf(o->t, sizeof o->t, "%s", t);
        snprintf(o->s, sizeof o->s, "%s", s);
    }
}

static void size_setup(const trow_t *r);
static void opt_choose(int k) {
    if (k < 0 || k >= nopt || !opts[k].ok) return;
    bool changed = k != sel_opt;
    sel_opt = k;
    sel_row = opts[k].row;
    int kind = opts[k].kind;
    sel_mode_inst = kind == OP_GAP ? INST_NEWPART : kind == OP_PART ? INST_PART : INST_DISK;
    lay_split = kind == OP_SPLIT;
    if (changed) size_setup(&rows[sel_row]);
}

/* limites do tamanho da partição do mOS para o destino escolhido */
static u64 disk_usable(int disk) { return ((storage_disk(disk)->size / 512 - 34 + 1) / 2048 * 2048 - 2048) * 512; }
static void size_setup(const trow_t *r) {
    size_min = install_min_bytes();
    size_max = r->part < 0 ? (r->gap.last - r->gap.first + 1) * 512 : disk_usable(r->disk) - INSTALL_DATA_MIN;
    if (size_max < size_min) size_max = size_min;
    size_step = size_max >= (8ull << 30) ? (1ull << 30) : (64ull << 20);
    if (r->part < 0) size_sel = size_max;                                   /* espaço livre: por padrão, todo ele */
    else {
        size_sel = MAX(size_min, disk_usable(r->disk) / 2 / size_step * size_step);   /* particionar: metade */
        if (size_sel > size_max) size_sel = size_max;
    }
}
static void size_adjust(int dir) {
    u64 v = size_sel;
    if (dir > 0) v = v / size_step * size_step + size_step;
    else v = v % size_step ? v / size_step * size_step : v - size_step;
    if (dir < 0 && v > size_sel) v = size_min;     /* estouro para baixo */
    size_sel = CLAMP(v, size_min, size_max);
}

static void current_plan(install_plan_t *pl) {
    trow_t *r = &rows[sel_row];
    memset(pl, 0, sizeof *pl);
    pl->mode = sel_mode_inst;
    pl->disk = r->disk;
    pl->part_number = r->part > 0 ? r->part : 0;
    if (sel_mode_inst == INST_DISK && lay_split) pl->mos_bytes = size_sel;
    if (sel_mode_inst == INST_NEWPART) { pl->mos_bytes = size_sel; pl->gap_first = r->gap.first; pl->gap_last = r->gap.last; }
    pl->zero_fill = zero_sel;
}

/* ------------------------------------------------------------------ desenho: controles */
static bool rng_ok(void) { return crypto_rng_hw(); }
static bool user_blocked(void) { return !rng_ok() && (flow == FL_FIRSTBOOT || sysstate() == SYS_LIVE_PERSIST || created_mode == M_PERSIST); }

static bool hover(rect_t r) { return rect_contains(r, mx, my); }

static void add_ctrl(int kind, int id, rect_t r, bool en) {
    if (nctrl < (int)ARRAY_LEN(ctrls)) ctrls[nctrl++] = (ctrl_t){kind, id, r, en};
}

static void draw_button(surface_t *s, int id, rect_t r, const char *label, bool primary, bool en) {
    add_ctrl(K_BTN, id, r, en);
    bool f = focus_id == id, h = en && hover(r);
    u32 bg_c = !en ? (T.dark ? RGBA(255, 255, 255, 14) : RGBA(0, 0, 0, 10)) : primary ? (h ? color_lighten(T.accent, 20) : T.accent) : (h ? (T.dark ? RGBA(255, 255, 255, 40) : RGBA(0, 0, 0, 24)) : (T.dark ? RGBA(255, 255, 255, 24) : RGBA(0, 0, 0, 14)));
    gfx_round_rect(s, r.x, r.y, r.w, r.h, 8, bg_c);
    if (f) gfx_round_rect_stroke(s, r.x - 2, r.y - 2, r.w + 4, r.h + 4, 10, 2, WITH_ALPHA(T.accent, 200));
    text_draw_center(s, font_get(FONT_SEMIBOLD), 13.5f, r, !en ? T.text3 : primary ? RGB(255, 255, 255) : T.text, label);
}

static void draw_row(surface_t *s, int id, rect_t r, const char *title, const char *sub, bool selected, bool en) {
    add_ctrl(K_ROW, id, r, en);
    bool f = focus_id == id, h = en && hover(r);
    if (selected) gfx_round_rect(s, r.x, r.y, r.w, r.h, 8, WITH_ALPHA(T.accent, 38));
    else if (h) gfx_round_rect(s, r.x, r.y, r.w, r.h, 8, T.dark ? RGBA(255, 255, 255, 14) : RGBA(0, 0, 0, 8));
    gfx_round_rect_stroke(s, r.x, r.y, r.w, r.h, 8, selected ? 2 : 1, selected ? T.accent : T.separator);
    if (f) gfx_round_rect_stroke(s, r.x - 2, r.y - 2, r.w + 4, r.h + 4, 10, 2, WITH_ALPHA(T.accent, 200));
    int ty = sub && sub[0] ? r.y + 22 : r.y + r.h / 2 + 5;
    text_draw_ellipsis(s, font_get(FONT_SEMIBOLD), 14, r.x + 14, ty, r.w - 28, en ? T.text : T.text3, title);
    if (sub && sub[0]) text_draw_ellipsis(s, font_get(FONT_REGULAR), 12, r.x + 14, r.y + r.h - 12, r.w - 28, en ? T.text2 : RGB(190, 100, 70), sub);
}

static void draw_field(surface_t *s, int id, rect_t r, const char *label, const char *buf, bool secure, const char *ph) {
    add_ctrl(secure ? K_PW : K_FIELD, id, r, true);
    bool f = focus_id == id;
    if (label) text_draw(s, font_get(FONT_MEDIUM), 12, r.x, r.y - 6, T.text2, label);
    if (f) gfx_round_rect_stroke(s, r.x - 2, r.y - 2, r.w + 4, r.h + 4, 8, 2, WITH_ALPHA(T.accent, 200));
    gfx_round_rect(s, r.x, r.y, r.w, r.h, 6, T.field_border);
    gfx_round_rect(s, r.x + 1, r.y + 1, r.w - 2, r.h - 2, 5, T.field_bg);
    int n = 0;
    for (const char *p = buf; *p; p++) if (((u8)*p & 0xC0) != 0x80) n++;
    if (!n && ph) text_draw(s, font_get(FONT_REGULAR), 13.5f, r.x + 10, r.y + r.h / 2 + 5, T.text3, ph);
    else if (secure) { for (int i = 0; i < n && i < 28; i++) gfx_circle(s, r.x + 16 + i * 10, r.y + r.h / 2.0f, 3.2f, T.text); }
    else text_draw_ellipsis(s, font_get(FONT_REGULAR), 13.5f, r.x + 10, r.y + r.h / 2 + 5, r.w - 20, T.text, buf);
    if (f && ((timer_ticks() - blink0) / 530) % 2 == 0) {
        int cx = secure ? r.x + 12 + n * 10 : r.x + 10 + text_width(font_get(FONT_REGULAR), 13.5f, buf);
        gfx_fill(s, MIN(cx, r.x + r.w - 6), r.y + 6, 1, r.h - 12, T.text);
    }
}

static void draw_check(surface_t *s, int id, int x, int y, int w, const char *label, bool on) {
    rect_t r = rect_make(x, y, w, 40);
    add_ctrl(K_CHECK, id, r, true);
    bool f = focus_id == id;
    gfx_round_rect(s, x, y + 4, 20, 20, 5, on ? T.accent : (T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 20)));
    if (on) glyph_check(s, x + 2, y + 6, 16, RGB(255, 255, 255));
    if (f) gfx_round_rect_stroke(s, x - 2, y + 2, 24, 24, 7, 2, WITH_ALPHA(T.accent, 200));
    text_wrap(s, font_get(FONT_REGULAR), 13, rect_make(x + 30, y + 3, w - 30, 40), 18, T.text, label, true);
}

static void para(surface_t *s, rect_t r, const char *t, float size, u32 col) { text_wrap(s, font_get(FONT_REGULAR), size, r, (int)(size * 1.45f), col, t, true); }

/* ------------------------------------------------------------------ ações */
static void to_step(int n) { step = n; msg[0] = 0; focus_id = 0; blink0 = timer_ticks(); if (n == SC_DONE) { done_at = timer_ticks(); done_hold = false; } wm_damage_all(); }

static void apply_lang(void) { g_lang = sel_lang; }
static void apply_kbd(void) { g_kbd_layout = sel_kbd; }

static void finish_session(void) {
    session_ready = true;
    active = false;
    free_tabs();
    if (bg) { surface_free(bg); bg = NULL; }
    memset(f_pw1, 0, sizeof f_pw1); memset(f_pw2, 0, sizeof f_pw2);
    char m[128];
    snprintf(m, sizeof m, "%s, %s", tr(S_WELCOME_TITLE), account_fullname());
    notify(m, created_mode == M_PERSIST ? tr(S_NOTE_PERSIST) : sysstate() == SYS_EPHEMERAL ? tr(S_NOTE_EPHEMERAL) : tr(S_NOTE_INSTALLED));
    sound_startup_chime();
    wm_damage_all();
}

static void do_create_user(void) {
    char err[160] = "";
    const char *u = f_user, *full = f_name[0] ? f_name : f_user;
    if (!f_user[0]) { snprintf(msg, sizeof msg, "%s", tr(S_ERR_USER_EMPTY)); creating = false; return; }
    if (user_blocked()) { snprintf(msg, sizeof msg, "%s", tr(S_RNG_BLOCK)); creating = false; return; }
    if (!rng_ok()) { f_pw1[0] = f_pw2[0] = 0; }          /* live efêmero sem gerador de hardware: sessão sem senha, só na RAM */
    else if (strcmp(f_pw1, f_pw2)) { snprintf(msg, sizeof msg, "%s", tr(S_ERR_PW_MISMATCH)); creating = false; return; }
    if (rng_ok() && !account_password_ok(f_pw1, err, sizeof err)) { snprintf(msg, sizeof msg, "%s", tr(S_ERR_PW_POLICY)); creating = false; return; }
    if (!account_create(u, full, f_pw1, true, err, sizeof err)) { snprintf(msg, sizeof msg, "%s", tr(S_ERR_USER_INVALID)); creating = false; return; }
    account_set_current(u);
    sysprefs_save();
    memset(f_pw1, 0, sizeof f_pw1); memset(f_pw2, 0, sizeof f_pw2);
    creating = false;
    if (flow == FL_FIRSTBOOT) persist_set_state(SYS_INSTALLED);
    to_step(SC_WELCOME);
}

static void start_install(void) {
    char phrase[40];
    install_plan_t pl;
    current_plan(&pl);
    install_confirm_phrase(&pl, phrase, sizeof phrase);
    if (strcmp(f_typed, phrase)) { snprintf(msg, sizeof msg, "%s", tr(S_ERR_CONFIRM)); return; }
    if (!install_begin(&pl, sel_snap, f_typed, lang_code(), kbd_code())) {
        snprintf(msg, sizeof msg, "%s", install_status()->error);
        to_step(SC_FAIL);
        return;
    }
    to_step(SC_RUN);
}

static bool mode_enabled(int m, int *why) {
    const origin_t *o = origin_get();
    *why = 0;
    if (m == M_LIVE) return true;
    if (sysstate() != SYS_EPHEMERAL) { *why = S_MODE_WHY_STATE; return false; }
    if (o->status != ORIGIN_TRUSTED) { *why = S_MODE_WHY_ORIGIN; return false; }
    if (m == M_PERSIST) {
        if (o->bus != PHYS_USB) { *why = S_MODE_WHY_NOTUSB; return false; }
        if (!rng_ok()) { *why = S_MODE_WHY_RNG; return false; }
        blkdev_t *d = storage_disk(o->disk);
        if (!d || !blk_hw_writable(d)) { *why = S_IR_RO; return false; }
        return true;
    }
    if (!install_payload()->ok) { *why = S_IR_NO_PAYLOAD; return false; }
    return true;
}

static void go_next(void) {
    int why;
    switch (step) {
    case SC_LANG: apply_lang(); to_step(SC_KBD); break;
    case SC_KBD: apply_kbd(); to_step(flow == FL_FIRSTBOOT ? SC_USER : SC_MODE); break;
    case SC_MODE:
        if (!mode_enabled(sel_mode, &why)) break;
        if (sel_mode == M_LIVE) { created_mode = M_LIVE; to_step(SC_USER); }
        else if (sel_mode == M_PERSIST) { consent_check = false; to_step(SC_PERSIST); }
        else { target_scan(); to_step(SC_TARGET); }
        break;
    case SC_PERSIST:
        if (!consent_check) break;
        if (!persist_consent_live()) { snprintf(msg, sizeof msg, "%s", tr(S_ERR_PERSIST)); break; }
        created_mode = M_PERSIST;
        to_step(SC_USER);
        break;
    case SC_USER:
        if (creating || user_blocked()) break;
        creating = true;                    /* a conta (PBKDF2) é criada no próximo quadro, depois de mostrar "aguarde" */
        msg[0] = 0;
        wm_damage_all();
        break;
    case SC_WELCOME: finish_session(); break;
    case SC_TARGET:
        if (!disk_eligible(sel_disk)) break;
        {
            int d = rows[drow[sel_disk]].disk;
            if (tabs[d]) memcpy(sel_snap, tabs[d]->snap, 32);
            f_typed[0] = 0;
            zero_sel = false;
            opts_build();
            for (int k = 0; k < nopt; k++) if (opts[k].ok) { opt_choose(k); break; }   /* 1ª opção possível já marcada */
            to_step(SC_LAYOUT);
        }
        break;
    case SC_LAYOUT: if (sel_opt >= 0 && opts[sel_opt].ok) to_step(SC_FORMAT); break;
    case SC_FORMAT: f_typed[0] = 0; to_step(SC_REVIEW); break;
    case SC_REVIEW: start_install(); break;
    case SC_DONE: if (flow == FL_INSTALLER) desktop_shutdown_dialog(1); else desktop_request_restart(); break;
    case SC_FAIL: to_step(SC_TARGET); target_scan(); break;
    }
}

static void go_back(void) {
    switch (step) {
    case SC_KBD: to_step(SC_LANG); break;
    case SC_MODE: to_step(SC_KBD); break;
    case SC_PERSIST: to_step(SC_MODE); break;
    case SC_USER: if (!creating) to_step(flow == FL_FIRSTBOOT ? SC_KBD : SC_MODE); break;
    case SC_TARGET:
        if (flow == FL_INSTALLER) { active = false; free_tabs(); if (bg) { surface_free(bg); bg = NULL; } wm_damage_all(); }
        else to_step(SC_MODE);
        break;
    case SC_LAYOUT: to_step(SC_TARGET); break;
    case SC_FORMAT: to_step(SC_LAYOUT); break;
    case SC_REVIEW: to_step(SC_FORMAT); break;
    case SC_DONE: if (flow == FL_INSTALLER) { active = false; free_tabs(); if (bg) { surface_free(bg); bg = NULL; } wm_damage_all(); } else to_step(SC_MODE); break;
    case SC_FAIL: if (flow == FL_INSTALLER) { active = false; free_tabs(); wm_damage_all(); } else to_step(SC_MODE); break;
    default: break;
    }
}

/* ------------------------------------------------------------------ telas */
static void footer(surface_t *s, rect_t p, bool back, const char *next_label, bool next_en, const char *extra) {
    int y = p.y + p.h - 58;
    gfx_fill(s, p.x + 28, y - 10, p.w - 56, 1, T.separator);
    if (back) draw_button(s, ID_BACK, rect_make(p.x + 28, y, 120, 34), tr(S_BACK), false, true);
    if (extra) draw_button(s, ID_EXTRA, rect_make(p.x + p.w - 28 - 190 - 16 - 190, y, 190, 34), extra, false, true);
    if (next_label) draw_button(s, ID_NEXT, rect_make(p.x + p.w - 28 - 190, y, 190, 34), next_label, true, next_en);
}

static void screen_title(surface_t *s, rect_t p, const char *t, const char *sub) {
    text_draw(s, font_get(FONT_BOLD), 22, p.x + 28, p.y + 50, T.text, t);
    if (sub && sub[0]) para(s, rect_make(p.x + 28, p.y + 62, p.w - 56, 50), sub, 13, T.text2);
}

static void draw_lang(surface_t *s, rect_t p) {
    screen_title(s, p, "Idioma · Language", tr(S_LANG_SUB));
    static const char *names[] = {"Português (Brasil)", "English"};
    for (int i = 0; i < 2; i++) {
        if (focus_id == 0) focus_id = ID_ROW + sel_lang;
        draw_row(s, ID_ROW + i, rect_make(p.x + 28, p.y + 120 + i * 64, p.w - 56, 54), names[i], i == 0 ? "pt-BR" : "en", sel_lang == i, true);
    }
    para(s, rect_make(p.x + 28, p.y + 262, p.w - 56, 80), tr(S_LANG_NOTE), 12.5f, T.text2);
    footer(s, p, false, tr(S_CONTINUE), true, NULL);
}

static void draw_kbd(surface_t *s, rect_t p) {
    screen_title(s, p, tr(S_KBD_TITLE), tr(S_KBD_SUB));
    static const char *names[] = {"Português (ABNT2)", "English (US)"};
    for (int i = 0; i < 2; i++) {
        if (focus_id == 0) focus_id = ID_ROW + sel_kbd;
        draw_row(s, ID_ROW + i, rect_make(p.x + 28, p.y + 120 + i * 64, p.w - 56, 54), names[i], i == 0 ? "ç ã é ~ ^ ´ `" : "US QWERTY", sel_kbd == i, true);
    }
    draw_field(s, ID_FKBD, rect_make(p.x + 28, p.y + 280, p.w - 56, 34), tr(S_KBD_TEST), f_kbd, false, tr(S_KBD_TEST_PH));
    footer(s, p, true, tr(S_CONTINUE), true, NULL);
}

static void draw_mode(surface_t *s, rect_t p) {
    screen_title(s, p, tr(S_MODE_TITLE), tr(S_MODE_SUB));
    static const int tt[] = {S_MODE_LIVE, S_MODE_PERSIST, S_MODE_INSTALL}, ss[] = {S_MODE_LIVE_SUB, S_MODE_PERSIST_SUB, S_MODE_INSTALL_SUB};
    for (int i = 0; i < 3; i++) {
        int why;
        bool en = mode_enabled(i, &why);
        if (focus_id == 0 && i == sel_mode) focus_id = ID_ROW + i;
        draw_row(s, ID_ROW + i, rect_make(p.x + 28, p.y + 118 + i * 82, p.w - 56, 72), tr(tt[i]), en ? tr(ss[i]) : tr(why), sel_mode == i, en);
    }
    int why;
    footer(s, p, true, tr(S_CONTINUE), mode_enabled(sel_mode, &why), NULL);
}

static void draw_persist(surface_t *s, rect_t p) {
    screen_title(s, p, tr(S_PERSIST_TITLE), NULL);
    const origin_t *o = origin_get();
    char t[400];
    snprintf(t, sizeof t, "%s %s (%s %d).", tr(S_PERSIST_WHERE), storage_disk_desc(o->disk), tr(S_PARTITION), o->part_number);
    para(s, rect_make(p.x + 28, p.y + 70, p.w - 56, 50), t, 13.5f, T.text);
    para(s, rect_make(p.x + 28, p.y + 120, p.w - 56, 170), tr(S_PERSIST_BODY), 13, T.text2);
    draw_check(s, ID_CHECK, p.x + 28, p.y + 300, p.w - 56, tr(S_PERSIST_CHECK), consent_check);
    if (msg[0]) para(s, rect_make(p.x + 28, p.y + 350, p.w - 56, 40), msg, 13, RGB(220, 70, 60));
    if (focus_id == 0) focus_id = ID_CHECK;
    footer(s, p, true, tr(S_PERSIST_BTN), consent_check, NULL);
}

static void draw_user(surface_t *s, rect_t p) {
    screen_title(s, p, tr(S_USER_TITLE), flow == FL_FIRSTBOOT ? tr(S_USER_SUB_INSTALLED) : created_mode == M_PERSIST ? tr(S_USER_SUB_PERSIST) : tr(S_USER_SUB_LIVE));
    int x = p.x + 28, w = p.w - 56, y = p.y + 128;
    bool nopw = !rng_ok(), blocked = user_blocked();
    if (focus_id == 0) focus_id = ID_FNAME;
    if (!blocked) {
        draw_field(s, ID_FNAME, rect_make(x, y, w, 32), tr(S_USER_FULL), f_name, false, "Ana Silva");
        draw_field(s, ID_FUSER, rect_make(x, y + 62, w, 32), tr(S_USER_ACCOUNT), f_user, false, "ana");
    }
    if (!nopw) {
        draw_field(s, ID_FPW1, rect_make(x, y + 124, w / 2 - 8, 32), tr(S_USER_PW), f_pw1, true, NULL);
        draw_field(s, ID_FPW2, rect_make(x + w / 2 + 8, y + 124, w / 2 - 8, 32), tr(S_USER_PW2), f_pw2, true, NULL);
        para(s, rect_make(x, y + 168, w, 60), tr(S_USER_PWNOTE), 12, T.text2);
    } else {
        /* sem gerador de hardware: aviso explícito; nunca pede parâmetros criptográficos nem cria senha com jitter/relógios */
        para(s, rect_make(x, blocked ? y : y + 124, w, 56), tr(S_RNG_WARN), 12.5f, RGB(220, 70, 60));
        para(s, rect_make(x, blocked ? y + 64 : y + 184, w, 40), blocked ? tr(S_RNG_BLOCK) : tr(S_RNG_LIVE_NOPW), 13, T.text);
    }
    if (creating) para(s, rect_make(x, y + 226, w, 24), tr(S_CREATING), 13, T.text);
    else if (msg[0]) para(s, rect_make(x, y + 226, w, 40), msg, 13, RGB(220, 70, 60));
    footer(s, p, !creating && sysstate() != SYS_LIVE_PERSIST, tr(S_USER_BTN), !creating && !blocked, NULL);   /* depois do consentimento não se volta atrás */
}

static void draw_welcome(surface_t *s, rect_t p) {
    gfx_circle(s, p.x + p.w / 2.0f, p.y + 120, 40, T.accent);
    text_draw_center(s, font_get(FONT_BOLD), 40, rect_make(p.x + p.w / 2 - 40, p.y + 80, 80, 80), RGB(255, 255, 255), "m");
    char t[160];
    snprintf(t, sizeof t, "%s, %s", tr(S_WELCOME_TITLE), account_fullname());
    text_draw_center(s, font_get(FONT_BOLD), 26, rect_make(p.x, p.y + 180, p.w, 40), T.text, t);
    const char *b = created_mode == M_PERSIST ? tr(S_NOTE_PERSIST) : sysstate() == SYS_EPHEMERAL ? tr(S_NOTE_EPHEMERAL) : tr(S_NOTE_INSTALLED);
    para(s, rect_make(p.x + 80, p.y + 236, p.w - 160, 120), b, 13.5f, T.text2);
    if (focus_id == 0) focus_id = ID_NEXT;
    footer(s, p, false, tr(S_START), true, NULL);
}

static void draw_target(surface_t *s, rect_t p) {
    screen_title(s, p, tr(S_TARGET_TITLE), tr(S_TARGET_SUB));
    int y0 = p.y + 112, rh = 54, vis = (p.h - 112 - 76) / (rh + 6);
    if (focus_id == 0) for (int k = 0; k < ndrow; k++) if (disk_eligible(k)) { focus_id = ID_ROW + k; break; }   /* o foco começa no 1º disco elegível */
    int fr = focus_id >= ID_ROW ? focus_id - ID_ROW : sel_disk;
    if (fr >= 0) { if (fr < scroll) scroll = fr; if (fr >= scroll + vis) scroll = fr - vis + 1; }
    if (!ndrow) para(s, rect_make(p.x + 28, y0, p.w - 56, 40), tr(S_TARGET_NONE), 13.5f, T.text2);
    for (int i = 0; i < vis && scroll + i < ndrow; i++) {
        int k = scroll + i;
        trow_t *r = &rows[drow[k]];
        rect_t rr = rect_make(p.x + 28, y0 + i * (rh + 6), p.w - 56, rh);
        draw_row(s, ID_ROW + k, rr, r->t, r->s, sel_disk == k, disk_eligible(k));
    }
    if (ndrow > vis) para(s, rect_make(p.x + p.w - 150, p.y + 82, 130, 20), "↑ ↓", 12, T.text3);
    footer(s, p, true, tr(S_CONTINUE), disk_eligible(sel_disk), NULL);
}

/* − valor + (tamanho da partição do mOS) */
static void draw_size(surface_t *s, rect_t p, int y, const char *label) {
    int x = p.x + 28, w = p.w - 56;
    char v[40], a[32], b[32], line[200];
    text_draw(s, font_get(FONT_MEDIUM), 12.5f, x, y, T.text2, label);
    draw_button(s, ID_MINUS, rect_make(x, y + 12, 44, 34), "−", false, size_sel > size_min);
    fmt_size(size_sel, v, sizeof v);
    text_draw_center(s, font_get(FONT_BOLD), 18, rect_make(x + 52, y + 12, 160, 34), T.text, v);
    draw_button(s, ID_PLUS, rect_make(x + 220, y + 12, 44, 34), "+", false, size_sel < size_max);
    trow_t *r = &rows[sel_row];
    u64 rest = r->part < 0 ? size_max - size_sel : disk_usable(r->disk) - size_sel;
    fmt_size(rest, a, sizeof a);
    fmt_size(size_min, b, sizeof b);
    snprintf(line, sizeof line, "%s %s   ·   %s %s", r->part < 0 ? tr(S_LAYOUT_LEFT) : tr(S_LAYOUT_DATA), rest ? a : "—", tr(S_LAYOUT_RANGE), b);
    text_draw_ellipsis(s, font_get(FONT_REGULAR), 12.5f, x + 284, y + 34, w - 284, T.text2, line);
    para(s, rect_make(x, y + 56, w, 20), tr(S_LAYOUT_KEYS), 11.5f, T.text3);
}

static void draw_layout(surface_t *s, rect_t p) {
    int d = rows[drow[sel_disk]].disk;
    char t[200];
    snprintf(t, sizeof t, "%s (%s) — %s", storage_disk_desc(d), storage_disk_name(d), tr(S_LAYOUT_SUB));
    screen_title(s, p, tr(S_LAYOUT_TITLE), t);
    int y0 = p.y + 112, rh = 52, shown = MIN(nopt, 5);
    if (focus_id == 0) focus_id = ID_ROW + MAX(sel_opt, 0);
    for (int k = 0; k < shown; k++)
        draw_row(s, ID_ROW + k, rect_make(p.x + 28, y0 + k * (rh + 4), p.w - 56, rh), opts[k].t, opts[k].s, sel_opt == k, opts[k].ok);
    if (sel_opt >= 0 && (opts[sel_opt].kind == OP_SPLIT || opts[sel_opt].kind == OP_GAP))
        draw_size(s, p, y0 + shown * (rh + 4) + 14, tr(S_LAYOUT_SIZE));
    footer(s, p, true, tr(S_CONTINUE), sel_opt >= 0 && opts[sel_opt].ok, NULL);
}

static void draw_format(surface_t *s, rect_t p) {
    screen_title(s, p, tr(S_FMT_TITLE), tr(S_FMT_SUB));
    if (focus_id == 0) focus_id = ID_ROW + (zero_sel ? 1 : 0);
    draw_row(s, ID_ROW + 0, rect_make(p.x + 28, p.y + 118, p.w - 56, 60), tr(S_FMT_QUICK), tr(S_FMT_QUICK_SUB), !zero_sel, true);
    draw_row(s, ID_ROW + 1, rect_make(p.x + 28, p.y + 186, p.w - 56, 60), tr(S_FMT_FULL), tr(S_FMT_FULL_SUB), zero_sel, true);
    if (zero_sel) {
        char t[200], sz[32];
        install_plan_t pl;
        current_plan(&pl);
        u64 n = pl.mode == INST_DISK ? storage_disk(pl.disk)->size : pl.mode == INST_NEWPART ? size_sel : 0;
        if (pl.mode == INST_PART && tabs[pl.disk])
            for (int i = 0; i < tabs[pl.disk]->n; i++) if (tabs[pl.disk]->p[i].number == pl.part_number) n = (tabs[pl.disk]->p[i].last - tabs[pl.disk]->p[i].first + 1) * 512;
        fmt_size(n, sz, sizeof sz);
        snprintf(t, sizeof t, "%s (%s).", pl.mode == INST_DISK ? tr(S_FMT_WHOLE) : tr(S_FMT_PART), sz);
        para(s, rect_make(p.x + 28, p.y + 262, p.w - 56, 40), t, 13, RGB(220, 120, 0));
    }
    footer(s, p, true, tr(S_CONTINUE), true, NULL);
}

static void draw_review(surface_t *s, rect_t p) {
    trow_t *r = &rows[sel_row];
    blkdev_t *bd = storage_disk(r->disk);
    char sz[32], t[1400];
    fmt_size(bd->size, sz, sizeof sz);
    screen_title(s, p, sel_mode_inst == INST_DISK ? tr(S_REV_TITLE_DISK) : sel_mode_inst == INST_NEWPART ? tr(S_REV_TITLE_NEW) : tr(S_REV_TITLE_PART), NULL);
    gfx_round_rect(s, p.x + 28, p.y + 66, p.w - 56, 196, 10, T.dark ? RGBA(255, 80, 60, 30) : RGBA(255, 80, 60, 22));
    ptable_t *tb = tabs[r->disk];
    int o = 0;
    if (sel_mode_inst == INST_DISK) {
        o += snprintf(t + o, sizeof t - o, "%s \"%s\" (%s, %s).\n%s\n", tr(S_REV_ERASE_DISK), storage_disk_desc(r->disk), storage_disk_name(r->disk), sz, tr(S_REV_ERASE_LIST));
        for (int i = 0; tb && i < tb->n && i < 3; i++)
            o += snprintf(t + o, sizeof t - o, " • %s %d: %s, %s\n", tr(S_PARTITION), tb->p[i].number, part_type_name(&tb->p[i]), part_fs_name(tb->p[i].fs));
        if (tb && tb->n > 3) o += snprintf(t + o, sizeof t - o, " • … +%d\n", tb->n - 3);
        if (!tb || !tb->n) o += snprintf(t + o, sizeof t - o, " • %s\n", tr(S_REV_NOPARTS));
        if (lay_split) {
            char a[32], b[32];
            fmt_size(size_sel, a, sizeof a);
            fmt_size(disk_usable(r->disk) - size_sel, b, sizeof b);
            o += snprintf(t + o, sizeof t - o, "%s %s %s %s.\n", tr(S_REV_LAYOUT_SPLIT), a, tr(S_REV_LAYOUT_DATA), b);
        }
        o += snprintf(t + o, sizeof t - o, "%s", lay_split ? tr(S_REV_META_SPLIT) : tr(S_REV_META_DISK));
    } else if (sel_mode_inst == INST_NEWPART) {
        char a[32];
        fmt_size(size_sel, a, sizeof a);
        o += snprintf(t + o, sizeof t - o, "%s %s %s \"%s\" (%s).\n", tr(S_REV_NEW1), a, tr(S_REV_NEW2), storage_disk_desc(r->disk), storage_disk_name(r->disk));
        o += snprintf(t + o, sizeof t - o, "%s %d %s\n", tr(S_REV_KEEP1), tb ? tb->n : 0, tr(S_REV_KEEP2));
        o += snprintf(t + o, sizeof t - o, "%s", tr(S_REV_META_NEW));
    } else {
        const part_t *pp = NULL;
        for (int i = 0; tb && i < tb->n; i++) if (tb->p[i].number == r->part) pp = &tb->p[i];
        char ps[32];
        fmt_size(pp ? (pp->last - pp->first + 1) * 512 : 0, ps, sizeof ps);
        o += snprintf(t + o, sizeof t - o, "%s %d (%s, %s) %s \"%s\".\n", tr(S_REV_ERASE_PART), r->part, ps, pp ? part_fs_name(pp->fs) : "?", tr(S_REV_ON), storage_disk_name(r->disk));
        o += snprintf(t + o, sizeof t - o, "%s %d %s\n", tr(S_REV_KEEP1), tb ? tb->n - 1 : 0, tr(S_REV_KEEP2));
        o += snprintf(t + o, sizeof t - o, "%s", tr(S_REV_META_PART));
    }
    o += snprintf(t + o, sizeof t - o, "\n%s", zero_sel ? tr(S_REV_FMT_FULL) : tr(S_REV_FMT_QUICK));
    if (!rng_ok()) o += snprintf(t + o, sizeof t - o, "\n%s", tr(S_RNG_REVIEW));
    para(s, rect_make(p.x + 44, p.y + 76, p.w - 88, 184), t, 12.5f, T.text);
    char phrase[40];
    install_plan_t pl;
    current_plan(&pl);
    install_confirm_phrase(&pl, phrase, sizeof phrase);
    char lab[160];
    snprintf(lab, sizeof lab, "%s \"%s\"", tr(S_REV_TYPE), phrase);
    if (focus_id == 0) focus_id = ID_FTYPED;
    draw_field(s, ID_FTYPED, rect_make(p.x + 28, p.y + 294, p.w - 56, 34), lab, f_typed, false, phrase);
    if (msg[0]) para(s, rect_make(p.x + 28, p.y + 336, p.w - 56, 30), msg, 13, RGB(220, 70, 60));
    footer(s, p, true, tr(S_REV_BTN), !strcmp(f_typed, phrase), NULL);
}

static void draw_run(surface_t *s, rect_t p) {
    const install_status_t *st = install_status();
    screen_title(s, p, tr(S_RUN_TITLE), NULL);
    text_draw_ellipsis(s, font_get(FONT_REGULAR), 13, p.x + 28, p.y + 110, p.w - 56, T.text2, st->step);
    gfx_round_rect(s, p.x + 28, p.y + 128, p.w - 56, 8, 4, T.dark ? RGB(60, 60, 64) : RGB(222, 222, 226));
    int fw = (int)((p.w - 56) * (st->percent / 100.0f));
    if (fw > 8) gfx_round_rect(s, p.x + 28, p.y + 128, fw, 8, 4, T.accent);
    char l[160], a[32], b[32];
    fmt_size(st->bytes_done, a, sizeof a);
    fmt_size(st->bytes_total, b, sizeof b);
    snprintf(l, sizeof l, "%d%% — %d/%d (%s / %s)", st->percent, st->files_done, st->files_total, a, b);
    text_draw(s, font_get(FONT_REGULAR), 12, p.x + 28, p.y + 160, T.text2, l);
    para(s, rect_make(p.x + 28, p.y + 200, p.w - 56, 60), tr(S_RUN_WARN), 13, T.text);
}

static void draw_done(surface_t *s, rect_t p) {
    gfx_circle(s, p.x + 64, p.y + 90, 32, RGB(60, 190, 90));
    glyph_check(s, p.x + 40, p.y + 66, 48, RGB(255, 255, 255));
    text_draw(s, font_get(FONT_BOLD), 22, p.x + 112, p.y + 98, T.text, tr(S_DONE_TITLE));
    para(s, rect_make(p.x + 28, p.y + 150, p.w - 56, 200), tr(S_DONE_BODY), 13.5f, T.text);
    bool autor = flow != FL_INSTALLER && !done_hold;   /* da mídia: reinicia sozinho para o primeiro boot do sistema instalado */
    if (autor) {
        u64 el = timer_ticks() - done_at;
        int left = el >= DONE_AUTO_MS ? 0 : (int)((DONE_AUTO_MS - el + 999) / 1000);
        char t[96];
        snprintf(t, sizeof t, "%s %d s…", tr(S_DONE_AUTO), left);
        text_draw(s, font_get(FONT_SEMIBOLD), 13.5f, p.x + 28, p.y + p.h - 84, T.accent, t);
    }
    if (focus_id == 0) focus_id = ID_NEXT;
    footer(s, p, flow == FL_INSTALLER, tr(S_RESTART), true, autor ? tr(S_DONE_STAY) : NULL);
}

static void draw_fail(surface_t *s, rect_t p) {
    const install_status_t *st = install_status();
    gfx_circle(s, p.x + 64, p.y + 90, 32, RGB(230, 70, 60));
    text_draw_center(s, font_get(FONT_BOLD), 34, rect_make(p.x + 32, p.y + 58, 64, 64), RGB(255, 255, 255), "!");
    text_draw(s, font_get(FONT_BOLD), 22, p.x + 112, p.y + 98, T.text, tr(S_FAIL_TITLE));
    para(s, rect_make(p.x + 28, p.y + 150, p.w - 56, 60), msg[0] ? msg : st->error, 13.5f, T.text);
    para(s, rect_make(p.x + 28, p.y + 214, p.w - 56, 120), st->wrote ? tr(S_FAIL_WROTE) : tr(S_FAIL_CLEAN), 13.5f, st->wrote ? RGB(220, 120, 0) : T.text2);
    if (focus_id == 0) focus_id = ID_NEXT;
    footer(s, p, true, tr(S_TRY_AGAIN), true, NULL);
}

void setup_draw(surface_t *s) {
    if (!active) return;
    int W = SW_(), H = SH_();
    if (!bg) {
        bg = surface_new(W, H);
        desktop_draw_background(bg, rect_make(0, 0, W, H));
        gfx_blur(bg, rect_make(0, 0, W, H), 10);
        gfx_fill(bg, 0, 0, W, H, RGBA(0, 0, 0, 70));
    }
    gfx_blit_rect(s, bg, s->clip, s->clip.x, s->clip.y);
    int pw = MIN(820, W - 40), ph = MIN(560, H - 40);
    rect_t p = rect_make((W - pw) / 2, (H - ph) / 2, pw, ph);
    gfx_round_rect(s, p.x - 1, p.y - 1, p.w + 2, p.h + 2, 22, RGBA(0, 0, 0, 60));
    gfx_round_rect(s, p.x, p.y, p.w, p.h, 20, T.dark ? RGBA(36, 36, 40, 246) : RGBA(246, 246, 248, 246));
    nctrl = 0;
    switch (step) {
    case SC_LANG: draw_lang(s, p); break;
    case SC_KBD: draw_kbd(s, p); break;
    case SC_MODE: draw_mode(s, p); break;
    case SC_PERSIST: draw_persist(s, p); break;
    case SC_USER: draw_user(s, p); break;
    case SC_WELCOME: draw_welcome(s, p); break;
    case SC_TARGET: draw_target(s, p); break;
    case SC_REVIEW: draw_review(s, p); break;
    case SC_LAYOUT: draw_layout(s, p); break;
    case SC_FORMAT: draw_format(s, p); break;
    case SC_RUN: draw_run(s, p); break;
    case SC_DONE: draw_done(s, p); break;
    case SC_FAIL: draw_fail(s, p); break;
    }
    /* rodapé do painel: origem do sistema (transparência sobre o que o assistente sabe) */
    const origin_t *o = origin_get();
    char st[200];
    snprintf(st, sizeof st, "mOS %s · %s · %s", MOS_VERSION, o->status == ORIGIN_TRUSTED ? o->why : tr(S_ORIGIN_AMBIGUOUS), sysstate_name());
    text_draw_ellipsis(s, font_get(FONT_REGULAR), 11, p.x + 28, p.y + p.h - 8, p.w - 56, T.text3, st);
}

/* ------------------------------------------------------------------ eventos */
static void activate(int id) {
    if (id == ID_BACK) { go_back(); return; }
    if (id == ID_NEXT) { go_next(); return; }
    if (id == ID_CHECK) { consent_check = !consent_check; return; }
    if (id == ID_MINUS) { size_adjust(-1); return; }
    if (id == ID_PLUS) { size_adjust(1); return; }
    if (id == ID_EXTRA && step == SC_DONE) { done_hold = true; to_step(SC_MODE); return; }
    if (id >= ID_ROW) {
        int i = id - ID_ROW;
        switch (step) {
        case SC_LANG: sel_lang = i; apply_lang(); break;
        case SC_KBD: sel_kbd = i; apply_kbd(); break;
        case SC_MODE: { int why; if (mode_enabled(i, &why)) sel_mode = i; break; }
        case SC_TARGET: if (disk_eligible(i)) sel_disk = i; break;
        case SC_LAYOUT: opt_choose(i); break;
        case SC_FORMAT: zero_sel = i == 1; break;
        }
    }
}

static char *field_buf(int id, int *cap) {
    switch (id) {
    case ID_FNAME: *cap = sizeof f_name; return f_name;
    case ID_FUSER: *cap = sizeof f_user; return f_user;
    case ID_FPW1: *cap = sizeof f_pw1; return f_pw1;
    case ID_FPW2: *cap = sizeof f_pw2; return f_pw2;
    case ID_FTYPED: *cap = sizeof f_typed; return f_typed;
    case ID_FKBD: *cap = sizeof f_kbd; return f_kbd;
    }
    return NULL;
}

static void move_focus(int dir) {
    if (!nctrl) return;
    int cur = -1;
    for (int i = 0; i < nctrl; i++) if (ctrls[i].id == focus_id) cur = i;
    for (int k = 1; k <= nctrl; k++) {
        int i = ((cur < 0 ? (dir > 0 ? -1 : 0) : cur) + dir * k + nctrl * 2) % nctrl;
        if (ctrls[i].en) { focus_id = ctrls[i].id; return; }
    }
}

static void move_selection(int dir) {
    if (step == SC_LANG) { sel_lang = CLAMP(sel_lang + dir, 0, 1); apply_lang(); focus_id = ID_ROW + sel_lang; }
    else if (step == SC_KBD && focus_id >= ID_ROW) { sel_kbd = CLAMP(sel_kbd + dir, 0, 1); apply_kbd(); focus_id = ID_ROW + sel_kbd; }
    else if (step == SC_MODE) {
        int why, n = sel_mode;
        for (int k = 0; k < 3; k++) { n = CLAMP(n + dir, 0, 2); if (mode_enabled(n, &why)) { sel_mode = n; break; } }
        focus_id = ID_ROW + sel_mode;
    } else if (step == SC_LAYOUT) {
        int n = focus_id >= ID_ROW ? focus_id - ID_ROW : sel_opt;
        for (int k = 0; k < nopt; k++) { n += dir; if (n < 0 || n >= MIN(nopt, 5)) break; if (opts[n].ok) { opt_choose(n); focus_id = ID_ROW + n; break; } }
    } else if (step == SC_FORMAT) { zero_sel = dir > 0; focus_id = ID_ROW + (zero_sel ? 1 : 0);
    } else if (step == SC_TARGET) {
        int n = focus_id >= ID_ROW ? focus_id - ID_ROW : sel_disk >= 0 ? sel_disk : (dir > 0 ? -1 : ndrow);
        for (int k = 0; k < ndrow; k++) { n += dir; if (n < 0 || n >= ndrow) break; if (disk_eligible(n)) { sel_disk = n; focus_id = ID_ROW + n; break; } }
    }
}

bool setup_handle_event(input_event_t *e) {
    if (!active) return false;
    if (e->type == EV_KEY_DOWN) {
        crypto_rng_add_entropy(&e->key, sizeof e->key);          /* temporização das teclas alimenta o RNG (a senha é digitada aqui) */
        int cap = 0;
        char *fb = field_buf(focus_id, &cap);
        blink0 = timer_ticks();
        if (step == SC_RUN) return true;
        if (e->key == KEY_TAB) { move_focus((e->mods & MOD_SHIFT) ? -1 : 1); }
        else if (e->key == KEY_ESC) { if (!creating && step != SC_WELCOME) go_back(); }
        else if (fb && e->key == KEY_BACKSPACE) { int l = (int)strlen(fb); if (l) fb[utf8_prev(fb, l)] = 0; msg[0] = 0; }
        else if (fb && e->key == KEY_ENTER) {
            /* Enter num campo: vai ao próximo campo; no último, avança a tela */
            int cur = focus_id;
            if (cur == ID_FNAME) focus_id = ID_FUSER;
            else if (cur == ID_FUSER) { if (rng_ok()) focus_id = ID_FPW1; else go_next(); }   /* sem gerador de hardware não há campos de senha */
            else if (cur == ID_FPW1) focus_id = ID_FPW2;
            else if (cur != ID_FKBD) go_next();
        }
        else if (fb && e->key >= 32 && e->key < 0x100 && !(e->mods & (MOD_CMD | MOD_CTRL))) {
            int l = (int)strlen(fb);
            if (l + 4 < cap) { l += utf8_encode((u32)e->key, fb + l); fb[l] = 0; msg[0] = 0; }
        }
        else if ((e->key == KEY_LEFT || e->key == KEY_RIGHT) && (focus_id == ID_MINUS || focus_id == ID_PLUS)) size_adjust(e->key == KEY_RIGHT ? 1 : -1);
        else if (e->key == KEY_UP || e->key == KEY_LEFT) { if (focus_id >= ID_ROW) move_selection(-1); else move_focus(-1); }
        else if (e->key == KEY_DOWN || e->key == KEY_RIGHT) { if (focus_id >= ID_ROW) move_selection(1); else move_focus(1); }
        else if (e->key == KEY_ENTER || e->key == ' ') {
            if (focus_id >= ID_ROW || focus_id == ID_CHECK) { activate(focus_id); if (e->key == KEY_ENTER && focus_id >= ID_ROW) go_next(); }
            else if (focus_id == ID_BACK || focus_id == ID_NEXT || focus_id == ID_EXTRA || focus_id == ID_MINUS || focus_id == ID_PLUS) activate(focus_id);
            else go_next();
        }
        wm_damage_all();
        return true;
    }
    if (e->type == EV_MOUSE) {
        static int lastb;
        mx = e->dx; my = e->dy;
        bool lp = (e->buttons & 1) && !(lastb & 1);
        lastb = e->buttons;
        if (lp && step != SC_RUN) {
            for (int i = nctrl - 1; i >= 0; i--) {
                if (!ctrls[i].en || !rect_contains(ctrls[i].r, mx, my)) continue;
                focus_id = ctrls[i].id;
                blink0 = timer_ticks();
                if (ctrls[i].kind != K_FIELD && ctrls[i].kind != K_PW) activate(ctrls[i].id);
                break;
            }
        }
        wm_damage_all();
        return true;
    }
    return true;
}

void setup_tick(void) {
    if (!active) return;
    if (step == SC_RUN) {
        install_poll();
        const install_status_t *st = install_status();
        if (st->state == INSTALL_DONE) to_step(SC_DONE);
        else if (st->state == INSTALL_ERROR) to_step(SC_FAIL);
        wm_damage_all();
        return;
    }
    if (step == SC_DONE && flow != FL_INSTALLER && !done_hold && timer_ticks() - done_at >= DONE_AUTO_MS) { done_hold = true; desktop_request_restart(); }
    if (step == SC_USER && creating) { do_create_user(); wm_damage_all(); return; }
    u64 now = timer_ticks();
    if (now - last_repaint > 250) { last_repaint = now; wm_damage_all(); }
}

/* ------------------------------------------------------------------ entrada */
static void reset_fields(void) {
    f_name[0] = f_user[0] = f_pw1[0] = f_pw2[0] = f_typed[0] = f_kbd[0] = msg[0] = 0;
    consent_check = creating = false;
    sel_mode = M_LIVE;
    sel_lang = g_lang;
    sel_kbd = g_kbd_layout;
}

static void open_common(int fl, int first) {
    reset_fields();
    flow = fl;
    active = true;
    session_ready = false;
    if (bg) { surface_free(bg); bg = NULL; }
    to_step(first);
}

/* decide, no boot, se o assistente roda. true = ele abriu (o login normal só vem depois, e só se o assistente não
 * criar a sessão). */
bool setup_boot(void) {
    sysprefs_load();
    int st = sysstate();
    kprintf("setup: estado %s, %d conta(s)\n", sysstate_name(), account_count());
    if (st == SYS_FIRSTBOOT) { open_common(FL_FIRSTBOOT, sysprefs_present() ? SC_USER : SC_LANG); return true; }
    if (st == SYS_INSTALLED || st == SYS_LIVE_PERSIST) {
        if (account_count() == 0) { open_common(FL_FIRSTBOOT, sysprefs_present() ? SC_USER : SC_LANG); return true; }   /* nunca entra sem conta */
        return false;
    }
    open_common(FL_MEDIUM, SC_LANG);
    return true;
}

/* app "Instalar mOS": só no live com origem confiável */
void setup_open_installer(void) {
    if (active) return;
    if (sysstate() != SYS_EPHEMERAL && sysstate() != SYS_LIVE_PERSIST) { notify(tr(S_APP_INSTALL), tr(S_APP_INSTALL_ONLY_LIVE)); return; }   /* da mídia Live (também a persistente) */
    if (origin_get()->status != ORIGIN_TRUSTED) { notify(tr(S_APP_INSTALL), tr(S_MODE_WHY_ORIGIN)); return; }
    open_common(FL_INSTALLER, SC_TARGET);
    target_scan();
}
