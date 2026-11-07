/* mOS — Gravador: grava do microfone (48 kHz), lista as gravações, toca com avanço/retrocesso de 15 s,
 * barra de posição, forma de onda, renomear, apagar e "Melhorar Gravação" (normalização). Arquivos WAV
 * em ~/Música/Gravações (abrem também no Música e no Arquivos). */
#include "kit.h"

extern app_t app_gravador;
#define MAXREC 64
#define REC_MAX_SEC 600

typedef struct { char name[96], path[200]; u32 frames; u64 mtime; } rec_t;
static struct {
    window_t *w;
    rec_t r[MAXREC];
    int n, sel, scroll;
    /* gravação */
    bool recording, paused_rec;
    i16 *buf;
    u32 nframes, cap;
    u64 rec_start;
    float level[160];
    /* reprodução */
    i16 *play;
    u32 play_frames, pos;        /* em quadros */
    bool playing;
    u64 fed_until, play_t0;      /* ms */
    u32 play_pos0;
    float wave[400];
    int wave_for;
    bool renaming;
    char rename_buf[96];
} V = {.sel = -1, .wave_for = -1};

static const char *dir(void) { static char d[200]; snprintf(d, sizeof d, "%s/Música/Gravações", CUR_HOME); vfs_mkdir(d); return d; }

static void scan(void) {
    V.n = 0;
    vnode_t *d = vfs_lookup(dir());
    for (vnode_t *c = d && d->dir ? d->child : NULL; c && V.n < MAXREC; c = c->next) {
        if (c->dir || strcasecmp(vfs_ext(c->name), "wav")) continue;
        rec_t *r = &V.r[V.n++];
        snprintf(r->name, sizeof r->name, "%s", c->name);
        char *dot = strrchr(r->name, '.');
        if (dot) *dot = 0;
        vfs_path(c, r->path, sizeof r->path);
        r->frames = c->size > 44 ? (u32)((c->size - 44) / 4) : 0;
        r->mtime = c->mtime;
    }
    /* mais recentes primeiro */
    for (int i = 1; i < V.n; i++) { rec_t t = V.r[i]; int j = i - 1; while (j >= 0 && V.r[j].mtime < t.mtime) { V.r[j + 1] = V.r[j]; j--; } V.r[j + 1] = t; }
}

static void stop_play(void) { V.playing = false; }
static void load_sel(void) {
    stop_play();
    if (V.play) { kfree(V.play); V.play = NULL; }
    V.pos = 0;
    if (V.sel < 0 || V.sel >= V.n) return;
    V.play = kit_load_wav(V.r[V.sel].path, &V.play_frames);
    /* forma de onda resumida */
    for (int i = 0; i < 400; i++) V.wave[i] = 0;
    if (V.play && V.play_frames) {
        for (int i = 0; i < 400; i++) {
            u32 a = (u32)((u64)V.play_frames * i / 400), b = (u32)((u64)V.play_frames * (i + 1) / 400);
            int mx = 0;
            for (u32 k = a; k < b; k += 8) { int v = V.play[k * 2]; if (v < 0) v = -v; if (v > mx) mx = v; }
            V.wave[i] = mx / 32768.0f;
        }
    }
    V.wave_for = V.sel;
}

static void start_play(void) {
    if (!V.play || !V.play_frames || !audio_available()) return;
    if (V.pos >= V.play_frames) V.pos = 0;
    V.playing = true;
    V.play_t0 = timer_ticks();
    V.play_pos0 = V.pos;
    V.fed_until = 0;
}
/* alimenta o mixer em fatias de 100 ms, mantendo ~300 ms à frente */
static void feed(void) {
    if (!V.playing) return;
    u64 el = timer_ticks() - V.play_t0;
    u32 cur = V.play_pos0 + (u32)(el * 48);
    if (cur >= V.play_frames) { V.playing = false; V.pos = V.play_frames; return; }
    V.pos = cur;
    while (V.fed_until < el + 300) {
        u32 from = V.play_pos0 + (u32)(V.fed_until * 48);
        if (from >= V.play_frames) break;
        u32 n = MIN(4800u, V.play_frames - from);
        audio_play(V.play + (usize)from * 2, n);
        V.fed_until += n / 48;
        if (n < 4800) break;
    }
}

static void rec_start(void) {
    if (!audio_input_available()) { notify("Gravador", "Nenhum microfone disponível."); return; }
    stop_play();
    V.cap = 48000 * 30;
    V.buf = kmalloc((usize)V.cap * 4);
    if (!V.buf) return;
    V.nframes = 0;
    V.recording = true;
    V.paused_rec = false;
    V.rec_start = timer_ticks();
    i16 tmp[4096];
    while (audio_capture_read(tmp, 1024) > 0) {}   /* descarta o que estava no buffer */
    memset(V.level, 0, sizeof V.level);
}
static void rec_pump(void) {
    if (!V.recording) return;
    i16 tmp[2048 * 2];
    u32 got;
    while ((got = audio_capture_read(tmp, 2048)) > 0) {
        if (V.paused_rec) continue;
        if (V.nframes + got > V.cap) {
            if (V.cap >= 48000u * REC_MAX_SEC) break;
            u32 nc = MIN(V.cap * 2, 48000u * REC_MAX_SEC);
            i16 *nb = krealloc(V.buf, (usize)nc * 4);
            if (!nb) break;
            V.buf = nb; V.cap = nc;
        }
        got = MIN(got, V.cap - V.nframes);
        memcpy(V.buf + (usize)V.nframes * 2, tmp, (usize)got * 4);
        V.nframes += got;
        int mx = 0;
        for (u32 i = 0; i < got; i++) { int v = tmp[i * 2]; if (v < 0) v = -v; if (v > mx) mx = v; }
        memmove(V.level, V.level + 1, sizeof(float) * 159);
        V.level[159] = MIN(1.0f, mx / 20000.0f);
    }
}
static void rec_stop(void) {
    if (!V.recording) return;
    rec_pump();
    V.recording = false;
    if (V.nframes > 4800) {
        int k = 1;
        char p[240];
        for (;; k++) { snprintf(p, sizeof p, "%s/Gravação nº %d.wav", dir(), k); if (!vfs_lookup(p)) break; }
        if (kit_save_wav(p, V.buf, V.nframes, 2, 48000)) {
            scan();
            for (int i = 0; i < V.n; i++) if (!strcmp(V.r[i].path, p)) { V.sel = i; load_sel(); }
        }
    }
    kfree(V.buf);
    V.buf = NULL;
}

static void delete_sel(void) {
    if (V.sel < 0) return;
    stop_play();
    vnode_t *v = vfs_lookup(V.r[V.sel].path);
    if (v) {
        char t[300];
        vfs_mkdir(CUR_TRASH);
        snprintf(t, sizeof t, "%s.wav", V.r[V.sel].name);
        vfs_move(v, vfs_lookup(CUR_TRASH), t);
    }
    V.sel = -1;
    if (V.play) { kfree(V.play); V.play = NULL; }
    scan();
}
static void enhance(void) {
    if (!V.play || !V.play_frames) return;
    int mx = 1;
    for (u32 i = 0; i < V.play_frames * 2; i++) { int v = V.play[i]; if (v < 0) v = -v; if (v > mx) mx = v; }
    float g = 30000.0f / mx;
    if (g < 1.05f) { notify("Gravador", "A gravação já está no volume máximo."); return; }
    for (u32 i = 0; i < V.play_frames * 2; i++) V.play[i] = (i16)CLAMP((int)(V.play[i] * g), -32767, 32767);
    kit_save_wav(V.r[V.sel].path, V.play, V.play_frames, 2, 48000);
    load_sel();
    notify("Gravação melhorada", "O volume foi normalizado.");
}

static void fmt_dur(char *o, int n, u32 frames, bool tenths) {
    u32 ms = frames / 48;
    if (tenths) snprintf(o, n, "%02u:%02u,%02u", ms / 60000, ms / 1000 % 60, ms / 10 % 100);
    else snprintf(o, n, "%u:%02u", ms / 60000, ms / 1000 % 60);
}

static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    rec_pump();
    feed();
    int sb = w->sidebar_w;
    /* lista de gravações na barra lateral */
    rect_t lv = rect_make(0, w->toolbar_h, sb, u->ch - w->toolbar_h - 90);
    rect_t pc = surface_push_clip(s, lv);
    int y = lv.y + 4 - V.scroll;
    for (int i = 0; i < V.n; i++) {
        rect_t r = rect_make(10, y, sb - 20, 58);
        bool sel = V.sel == i;
        if (sel) gfx_round_rect(s, r.x, r.y, r.w, r.h, 9, T.sidebar_sel);
        text_draw_ellipsis(s, font_get(FONT_SEMIBOLD), 13.5f, r.x + 12, r.y + 22, r.w - 24, T.text, V.r[i].name);
        char d[16];
        fmt_dur(d, sizeof d, V.r[i].frames, false);
        text_draw(s, font_get(FONT_REGULAR), 12, r.x + 12, r.y + 42, T.text2, "Gravação");
        int tw = text_width(font_get(FONT_REGULAR), 12, d);
        text_draw(s, font_get(FONT_REGULAR), 12, r.x + r.w - 12 - tw, r.y + 42, T.text2, d);
        if (ui_clicked(u, 105000 + i, r) && !V.recording) { if (V.sel != i) { V.sel = i; load_sel(); V.renaming = false; } else if (u->dblclick) { V.renaming = true; snprintf(V.rename_buf, 96, "%s", V.r[i].name); } }
        gfx_fill(s, r.x + 12, r.y + r.h, r.w - 24, 1, T.separator);
        y += 60;
    }
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, lv, V.n * 60 + 10, &V.scroll);
    /* botão de gravar */
    float cx = sb / 2.0f, cy = u->ch - 46;
    rect_t rb = rect_make((int)cx - 26, (int)cy - 26, 52, 52);
    gfx_circle(s, cx, cy, 26, T.dark ? RGB(80, 80, 84) : RGB(210, 210, 214));
    gfx_circle(s, cx, cy, 23, T.dark ? RGB(30, 30, 32) : RGB(255, 255, 255));
    if (V.recording) gfx_round_rect(s, cx - 10, cy - 10, 20, 20, 4, RGB(255, 59, 48));
    else gfx_circle(s, cx, cy, ui_hit(u, rb) && u->down ? 17 : 19, RGB(255, 59, 48));
    if (ui_clicked(u, UI_ID, rb)) { if (V.recording) rec_stop(); else rec_start(); }
    /* área principal */
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg);
    if (V.recording) {
        rec_pump();
        ui_label(u, a.x + 24, a.y + 20, "Nova Gravação", FONT_BOLD, 22, T.text);
        char t[24];
        fmt_dur(t, sizeof t, V.nframes, true);
        ui_label_center(u, rect_make(a.x, a.y + a.h / 2 + 70, a.w, 40), t, FONT_REGULAR, 34, T.text);
        int cyw = a.y + a.h / 2 - 20, bw = (a.w - 60) / 160;
        for (int i = 0; i < 160; i++) {
            float h = MAX(2.0f, V.level[i] * 120);
            gfx_round_rect(s, a.x + 30 + i * bw, cyw - h / 2, MAX(1, bw - 1), h, 1, i > 150 ? RGB(255, 59, 48) : T.text2);
        }
        gfx_fill(s, a.x + 30 + 160 * bw - 2, cyw - 70, 2, 140, RGB(255, 59, 48));
        if (ui_button_style(u, UI_ID, rect_make(a.x + a.w / 2 - 130, a.y + a.h - 60, 120, 32), V.paused_rec ? "Retomar" : "Pausar", BTN_GRAY | BTN_CAPSULE, true, NULL)) V.paused_rec = !V.paused_rec;
        if (ui_button_style(u, UI_ID, rect_make(a.x + a.w / 2 + 10, a.y + a.h - 60, 120, 32), "OK", BTN_FILLED | BTN_CAPSULE, true, NULL)) rec_stop();
        u->repaint = true;
        return;
    }
    if (V.sel < 0 || V.sel >= V.n) {
        ui_label_center(u, rect_make(a.x, a.y + a.h / 2 - 40, a.w, 30), V.n ? "Selecione uma gravação" : "Nenhuma Gravação", FONT_SEMIBOLD, 18, T.text3);
        ui_label_center(u, rect_make(a.x, a.y + a.h / 2 - 8, a.w, 20), audio_input_available() ? "Clique no botão vermelho para gravar." : "Nenhum microfone foi encontrado.", FONT_REGULAR, 13, T.text3);
        return;
    }
    if (V.wave_for != V.sel) load_sel();
    rec_t *r = &V.r[V.sel];
    if (V.renaming) {
        if (ui_textfield(u, 105900, rect_make(a.x + 24, a.y + 16, a.w - 200, 30), V.rename_buf, sizeof V.rename_buf, "Nome")) {
            char np[300];
            snprintf(np, sizeof np, "%s.wav", V.rename_buf);
            vnode_t *v = vfs_lookup(r->path);
            if (v && V.rename_buf[0] && vfs_move(v, v->parent, np)) { scan(); for (int i = 0; i < V.n; i++) if (!strcmp(V.r[i].name, V.rename_buf)) V.sel = i; }
            V.renaming = false;
        } else if (u->focus != 105900) u->focus = 105900;
    } else ui_label(u, a.x + 24, a.y + 20, r->name, FONT_BOLD, 22, T.text);
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 90, TOOLBAR_Y(28), 90, 28), "Apagar", true)) { delete_sel(); return; }
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 90 - 8 - 170, TOOLBAR_Y(28), 170, 28), "Melhorar Gravação", V.play != NULL)) enhance();
    /* forma de onda com cursor */
    rect_t wv = rect_make(a.x + 30, a.y + 80, a.w - 60, a.h - 230);
    int cyw = wv.y + wv.h / 2;
    float frac = V.play_frames ? (float)V.pos / V.play_frames : 0;
    for (int i = 0; i < 400; i++) {
        int x = wv.x + i * wv.w / 400;
        float h = MAX(2.0f, V.wave[i] * wv.h * 0.9f);
        gfx_fill(s, x, (int)(cyw - h / 2), MAX(1, wv.w / 400), (int)h, (float)i / 400 < frac ? RGB(255, 59, 48) : T.text3);
    }
    gfx_fill(s, wv.x + (int)(frac * wv.w), wv.y, 2, wv.h, RGB(255, 59, 48));
    if (ui_hit(u, wv) && u->pressed) {
        V.pos = (u32)((u64)V.play_frames * (u->mx - wv.x) / wv.w);
        if (V.playing) start_play();
    }
    /* tempo e controles */
    char t1[24], t2[24];
    fmt_dur(t1, sizeof t1, V.pos, true);
    fmt_dur(t2, sizeof t2, V.play_frames, false);
    ui_label_center(u, rect_make(a.x, wv.y + wv.h + 14, a.w, 30), t1, FONT_REGULAR, 26, T.text);
    char rest[24];
    snprintf(rest, sizeof rest, "duração %s", t2);
    ui_label_center(u, rect_make(a.x, wv.y + wv.h + 46, a.w, 16), rest, FONT_REGULAR, 12, T.text2);
    int by = a.y + a.h - 60, bx = a.x + a.w / 2;
    if (ui_button_style(u, UI_ID, rect_make(bx - 130, by, 70, 32), "−15 s", BTN_PLAIN_GRAY, true, NULL)) { V.pos = V.pos > 48000 * 15 ? V.pos - 48000 * 15 : 0; if (V.playing) start_play(); }
    rect_t pb = rect_make(bx - 24, by - 8, 48, 48);
    gfx_circle(s, bx, by + 16, 24, T.text);
    if (V.playing) { gfx_fill(s, bx - 8, by + 6, 6, 20, T.content_bg); gfx_fill(s, bx + 3, by + 6, 6, 20, T.content_bg); }
    else { float tri[] = {bx - 6.0f, by + 5.0f, bx + 11.0f, by + 16.0f, bx - 6.0f, by + 27.0f}; gfx_polygon(s, tri, 3, T.content_bg); }
    if (ui_clicked(u, UI_ID, pb) || (u->key == ' ' && !u->focus)) { if (V.playing) stop_play(); else start_play(); u->key_used = true; }
    if (ui_button_style(u, UI_ID, rect_make(bx + 60, by, 70, 32), "+15 s", BTN_PLAIN_GRAY, true, NULL)) { V.pos = MIN(V.play_frames, V.pos + 48000 * 15); if (V.playing) start_play(); }
    if ((u->key == KEY_BACKSPACE || u->key == KEY_DELETE) && !u->focus) { delete_sel(); u->key_used = true; }
    if (V.playing) u->repaint = true;
}

static void tick(app_t *a) { rec_pump(); feed(); }
static void on_close(window_t *w) { rec_stop(); stop_play(); V.w = NULL; }
static void launch(app_t *a) {
    if (V.w) { wm_focus(V.w); return; }
    scan();
    V.w = wm_create(&app_gravador, "Gravador", -1, -1, 860, 540, WIN_RESIZABLE);
    if (!V.w) return;
    V.w->paint = paint;
    V.w->on_close = on_close;
    V.w->toolbar_h = TOOLBAR_H;
    V.w->sidebar_w = 270;
    V.w->min_w = 640;
    V.w->min_h = 420;
    V.wave_for = -1;
}
static void icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(40, 40, 44), RGB(14, 14, 16));
    static const float h[] = {6, 14, 22, 30, 18, 26, 12, 20, 8};
    for (int i = 0; i < 9; i++) {
        float hh = h[i] * u;
        gfx_round_rect(s, x + (13 + i * 4.6f) * u, y + 32 * u - hh / 2, 2.6f * u, hh, 1.3f * u, i == 4 ? RGB(255, 59, 48) : RGB(245, 245, 245));
    }
}
app_t app_gravador = {.name = "Gravador", .exec = "gravador", .draw_icon = icon, .launch = launch, .tick = tick};
