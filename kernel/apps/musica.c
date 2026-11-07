/* mOS — Música: biblioteca de arquivos WAV em ~/Música (pastas viram álbuns; inclui as Gravações), busca,
 * playlists próprias, reprodução contínua com aleatório e repetir, barra de progresso arrastável, volume,
 * "Tocando Agora" e faixas de demonstração (melodias de domínio público sintetizadas na primeira abertura). */
#include "kit.h"

extern app_t app_musica;
#define MAXT 400
#define MAXPL 16

typedef struct { char path[220], title[96], album[64]; u32 frames; u64 mtime; } track_t;
typedef struct { char name[48]; int items[128]; int n; } playlist_t;
static struct {
    window_t *w;
    track_t t[MAXT];
    int nt;
    playlist_t pl[MAXPL];
    int npl;
    int view;                 /* 0 músicas, 1 álbuns, 2 recentes, 10+ playlist */
    int album_sel;
    char albums[48][64];
    int nalb;
    int sel, scroll;
    char search[64];
    /* reprodução */
    int cur;
    i16 *pcm;
    u32 frames, pos, pos0;
    bool playing, shuffle;
    int repeat;               /* 0 não, 1 todas, 2 uma */
    u64 t0, fed;
    float vol;
    bool renaming;
    char rename_buf[48];
    int queue[MAXT], nq, qi;  /* fila atual (índices de faixas) */
    int demo_next;            /* próxima faixa de demonstração a gerar (-1 = nenhuma) */
} MU = {.cur = -1, .sel = -1, .vol = -1, .demo_next = -1};

/* ------------------------------------------------------------ faixas de demonstração */
typedef struct { const char *name; const char *notes; int bpm; } song_t;
static const song_t demos[] = {
    {"Ode à Alegria", "E4 E4 F4 G4 G4 F4 E4 D4 C4 C4 D4 E4 E4. D4h D42 E4 E4 F4 G4 G4 F4 E4 D4 C4 C4 D4 E4 D4. C4h C42", 120},
    {"Brilha, Brilha Estrelinha", "C4 C4 G4 G4 A4 A4 G42 F4 F4 E4 E4 D4 D4 C42 G4 G4 F4 F4 E4 E4 D42 G4 G4 F4 F4 E4 E4 D42", 110},
    {"Für Elise", "E5h D#5h E5h D#5h E5h B4h D5h C5h A42 C4h E4h A4h B42 E4h G#4h B4h C52 E4h E5h D#5h E5h D#5h E5h B4h D5h C5h A42", 130},
    {"Greensleeves", "A4 C52 D5 E5. F5h E5 D52 B4 G4. A4h B4 C52 A4 A4. G#4h A4 B42 G#4 E42", 100},
};
static float note_freq(const char *n, int *len) {
    static const int semis[7] = {9, 11, 0, 2, 4, 5, 7};
    const char *p = n;
    if (*p == 'R') { *len = 4; return 0; }
    int s = semis[*p - 'A'];
    p++;
    if (*p == '#') { s++; p++; }
    int oct = *p++ - '0';
    int midi = 12 * (oct + 1) + s;
    *len = 4;
    if (*p == '2') *len = 8; else if (*p == 'h') *len = 2; else if (*p == '.') *len = 6;
    return 440.0f * fpow(2.0f, (midi - 69) / 12.0f);
}
/* piano simples: harmônicos com decaimento exponencial, em estéreo 48 kHz */
static void render_demo(const song_t *sg, const char *path) {
    /* tabela de seno (Q15) e fase em ponto fixo 22.10: barato até no QEMU */
    static i16 sine[1024];
    static bool ready;
    if (!ready) { for (int i = 0; i < 1024; i++) sine[i] = (i16)(fsin(i * 2 * PI_F / 1024) * 32767); ready = true; }
    int total = 0;
    const char *p = sg->notes;
    while (*p) { int l; note_freq(p, &l); total += l; while (*p && *p != ' ') p++; while (*p == ' ') p++; }
    u32 spb = 48000 * 60 / sg->bpm / 4;
    u32 frames = (u32)total * spb + 48000;
    i16 *o = kcalloc((usize)frames * 2, 2);
    if (!o) return;
    u32 at = 0;
    p = sg->notes;
    while (*p) {
        int l;
        float f = note_freq(p, &l);
        u32 dur = (u32)l * spb;
        if (f > 0) {
            u32 ring = MIN(frames - at, dur + 24000);
            u32 inc = (u32)(f * 1024.0f * 1024.0f / 48000.0f), ph = 0;
            float env = 0, decay = 0.999933f, release = 0.99975f;
            for (u32 i = 0; i < ring; i++) {
                env = i < 240 ? i / 240.0f : env * (i > dur ? decay * release : decay);
                u32 a = (ph >> 10) & 1023, b = ((ph * 2) >> 10) & 1023, c = ((ph * 3) >> 10) & 1023;
                int v = (int)((sine[a] * 0.62f + sine[b] * 0.25f + sine[c] * 0.1f) * env * 0.27f);
                ph += inc;
                int k = (int)(at + i);
                o[k * 2] = (i16)CLAMP(o[k * 2] + v, -32767, 32767);
                o[k * 2 + 1] = (i16)CLAMP(o[k * 2 + 1] + v * 15 / 16, -32767, 32767);
            }
        }
        at += dur;
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
    }
    kit_save_wav(path, o, frames, 2, 48000);
    kfree(o);
}

/* ------------------------------------------------------------ biblioteca */
static const char *mdir(void) { static char d[200]; snprintf(d, sizeof d, "%s/Música", CUR_HOME); return d; }
static void scan_dir(const char *dir, const char *album, int depth) {
    vnode_t *d = vfs_lookup(dir);
    for (vnode_t *c = d && d->dir ? d->child : NULL; c && MU.nt < MAXT; c = c->next) {
        char sub[220];
        snprintf(sub, sizeof sub, "%s/%s", dir, c->name);
        if (c->dir) { if (depth < 3 && c->name[0] != '.') scan_dir(sub, c->name, depth + 1); continue; }
        if (strcasecmp(vfs_ext(c->name), "wav")) continue;
        track_t *t = &MU.t[MU.nt++];
        snprintf(t->path, sizeof t->path, "%s", sub);
        snprintf(t->title, sizeof t->title, "%.*s", (int)(strlen(c->name) - 4), c->name);
        snprintf(t->album, sizeof t->album, "%s", album);
        /* duração pelo cabeçalho (PCM 16 bits) */
        vfs_ensure(c);
        u32 rate = c->size > 44 ? kit_get32(c->data + 24) : 48000, ch = c->size > 44 ? c->data[22] : 2;
        t->frames = c->size > 44 && rate && ch ? (u32)((u64)(c->size - 44) / (ch * 2) * 48000 / rate) : 0;
        t->mtime = c->mtime;
    }
}
static void pl_save(void) {
    sbuf_t b = {0};
    for (int i = 0; i < MU.npl; i++) {
        char nm[48];
        kit_field(nm, 48, MU.pl[i].name);
        sb_printf(&b, "P\t%s", nm);
        for (int k = 0; k < MU.pl[i].n; k++) sb_printf(&b, "\t%s", MU.t[MU.pl[i].items[k]].path);
        sb_putc(&b, '\n');
    }
    char p[200];
    kit_save(kit_lib("Música", "playlists.tsv", p, sizeof p), &b);
}
static void library_scan(void) {
    char d[220];
    snprintf(d, sizeof d, "%s/Demos", mdir());
    if (!vfs_lookup(d)) {
        vfs_mkdir(d);
        MU.demo_next = 0;   /* as faixas são geradas uma por vez no tick, com a janela já aberta */
    }
    MU.nt = 0;
    scan_dir(mdir(), "Música", 0);
    /* álbuns = pastas */
    MU.nalb = 0;
    for (int i = 0; i < MU.nt; i++) {
        bool have = false;
        for (int k = 0; k < MU.nalb; k++) if (!strcmp(MU.albums[k], MU.t[i].album)) have = true;
        if (!have && MU.nalb < 48) snprintf(MU.albums[MU.nalb++], 64, "%s", MU.t[i].album);
    }
    /* playlists */
    char p[200];
    char *buf = kit_read(kit_lib("Música", "playlists.tsv", p, sizeof p), NULL);
    MU.npl = 0;
    if (buf) {
        char *cur = buf, *ln, *f[130];
        while ((ln = kit_line(&cur)) && MU.npl < MAXPL) {
            int n = kit_split(ln, f, 130);
            if (strcmp(f[0], "P") || n < 2) continue;
            playlist_t *pl = &MU.pl[MU.npl++];
            snprintf(pl->name, 48, "%s", f[1]);
            pl->n = 0;
            for (int k = 2; k < n && pl->n < 128; k++) for (int i = 0; i < MU.nt; i++) if (!strcmp(MU.t[i].path, f[k])) { pl->items[pl->n++] = i; break; }
        }
        kfree(buf);
    }
}
static bool in_view(int i) {
    track_t *t = &MU.t[i];
    if (MU.search[0]) {
        char h[200], q[64];
        snprintf(h, sizeof h, "%s %s", t->title, t->album);
        snprintf(q, sizeof q, "%s", MU.search);
        for (char *c = h; *c; c++) *c = (char)tolower(*c);
        for (char *c = q; *c; c++) *c = (char)tolower(*c);
        if (!strstr(h, q)) return false;
    }
    if (MU.view == 1) return MU.album_sel >= 0 && !strcmp(t->album, MU.albums[MU.album_sel]);
    if (MU.view == 2) return t->mtime && timer_ticks() - t->mtime < 3600000ull * 24 * 30;
    if (MU.view >= 10) { playlist_t *pl = &MU.pl[MU.view - 10]; for (int k = 0; k < pl->n; k++) if (pl->items[k] == i) return true; return false; }
    return true;
}

/* ------------------------------------------------------------ reprodução */
static void stop(void) { MU.playing = false; }
static void play_track(int i) {
    if (i < 0 || i >= MU.nt) return;
    if (MU.pcm) { kfree(MU.pcm); MU.pcm = NULL; }
    MU.pcm = kit_load_wav(MU.t[i].path, &MU.frames);
    MU.cur = i;
    MU.pos = 0;
    if (!MU.pcm) { MU.playing = false; notify("Música", "Não foi possível tocar este arquivo (use WAV PCM de 16 bits)."); return; }
    MU.playing = audio_available();
    MU.t0 = timer_ticks(); MU.pos0 = 0; MU.fed = 0;
}
static void build_queue(int start) {
    MU.nq = 0;
    for (int i = 0; i < MU.nt; i++) if (in_view(i)) MU.queue[MU.nq++] = i;
    if (MU.shuffle) for (int i = MU.nq - 1; i > 0; i--) { int j = rand32() % (i + 1); int t = MU.queue[i]; MU.queue[i] = MU.queue[j]; MU.queue[j] = t; }
    MU.qi = 0;
    for (int k = 0; k < MU.nq; k++) if (MU.queue[k] == start) MU.qi = k;
}
static void next(int dir) {
    if (!MU.nq) return;
    if (dir < 0 && MU.pos > 48000 * 3) { play_track(MU.cur); return; }
    MU.qi += dir;
    if (MU.qi >= MU.nq) { if (MU.repeat == 1) MU.qi = 0; else { MU.qi = MU.nq - 1; stop(); MU.pos = 0; return; } }
    if (MU.qi < 0) MU.qi = MU.repeat == 1 ? MU.nq - 1 : 0;
    play_track(MU.queue[MU.qi]);
}
static void seek(u32 frame) { MU.pos = MIN(frame, MU.frames); if (MU.playing) { MU.t0 = timer_ticks(); MU.pos0 = MU.pos; MU.fed = 0; } }
static void feed(void) {
    if (!MU.playing || !MU.pcm) return;
    u64 el = timer_ticks() - MU.t0;
    u32 cur = MU.pos0 + (u32)(el * 48);
    if (cur >= MU.frames) {
        MU.playing = false;
        if (MU.repeat == 2) play_track(MU.cur); else next(1);
        return;
    }
    MU.pos = cur;
    while (MU.fed < el + 300) {
        u32 from = MU.pos0 + (u32)(MU.fed * 48);
        if (from >= MU.frames) break;
        u32 n = MIN(4800u, MU.frames - from);
        audio_play(MU.pcm + (usize)from * 2, n);
        MU.fed += n / 48;
        if (n < 4800) break;
    }
}

/* ------------------------------------------------------------ desenho */
static void fmt_t(char *o, int n, u32 frames) { u32 s = frames / 48000; snprintf(o, n, "%u:%02u", s / 60, s % 60); }
static void art(surface_t *s, rect_t r, const char *seed) {
    u32 h = 5381;
    for (const char *p = seed; *p; p++) h = h * 33 + (u8)*p;
    u32 c1 = RGB(80 + h % 150, 60 + (h >> 8) % 150, 90 + (h >> 16) % 150), c2 = RGB(40 + (h >> 4) % 120, 30 + (h >> 12) % 120, 60 + (h >> 20) % 150);
    gfx_round_rect_grad(s, r.x, r.y, r.w, r.h, r.w * 0.12f, c1, c2);
    gfx_circle(s, r.x + r.w * 0.5f, r.y + r.h * 0.5f, r.w * 0.22f, RGBA(255, 255, 255, 60));
    gfx_circle(s, r.x + r.w * 0.5f, r.y + r.h * 0.5f, r.w * 0.06f, RGBA(255, 255, 255, 120));
}
static void g_note(surface_t *s, int x, int y, int sz, u32 c) { glyph_sb_music(s, x, y, sz, c); }
static void g_album(surface_t *s, int x, int y, int sz, u32 c) { gfx_round_rect_stroke(s, x + sz * 0.15f, y + sz * 0.15f, sz * 0.7f, sz * 0.7f, 2, 1.5f, c); gfx_circle(s, x + sz * 0.5f, y + sz * 0.5f, sz * 0.12f, c); }
static void g_clock(surface_t *s, int x, int y, int sz, u32 c) { glyph_sb_recents(s, x, y, sz, c); }
static void g_list(surface_t *s, int x, int y, int sz, u32 c) { glyph_list(s, x, y, sz, c); }
static void g_shuffle(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_line(s, x + sz * 0.15f, y + sz * 0.3f, x + sz * 0.75f, y + sz * 0.7f, 1.6f, c);
    gfx_line(s, x + sz * 0.15f, y + sz * 0.7f, x + sz * 0.75f, y + sz * 0.3f, 1.6f, c);
    float a[] = {x + sz * 0.9f, y + sz * 0.3f, x + sz * 0.7f, y + sz * 0.18f, x + sz * 0.7f, y + sz * 0.42f}, b[] = {x + sz * 0.9f, y + sz * 0.7f, x + sz * 0.7f, y + sz * 0.58f, x + sz * 0.7f, y + sz * 0.82f};
    gfx_polygon(s, a, 3, c);
    gfx_polygon(s, b, 3, c);
}

static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    feed();
    if (MU.vol < 0) MU.vol = (float)audio_get_volume();
    int sb = w->sidebar_w;
    int y = w->toolbar_h + 4;
    ui_searchfield(u, UI_ID, rect_make(12, y, sb - 24, 28), MU.search, sizeof MU.search);
    y += 40;
    ui_label(u, 18, y, "Biblioteca", FONT_BOLD, 11.5f, T.text2);
    y += 20;
    static const char *lib[3] = {"Músicas", "Álbuns", "Adicionadas Recentemente"};
    static glyph_fn gi[3] = {g_note, g_album, g_clock};
    for (int i = 0; i < 3; i++) {
        if (ui_sidebar_item(u, 129000 + i, rect_make(10, y, sb - 20, 28), lib[i], MU.view == i, gi[i])) { MU.view = i; MU.scroll = 0; if (i == 1) MU.album_sel = -1; }
        y += 30;
    }
    y += 8;
    ui_label(u, 18, y, "Playlists", FONT_BOLD, 11.5f, T.text2);
    y += 20;
    for (int i = 0; i < MU.npl; i++) {
        rect_t r = rect_make(10, y, sb - 20, 28);
        if (MU.renaming && MU.view == 10 + i) {
            if (ui_textfield(u, 129900, rect_make(r.x + 30, r.y + 2, r.w - 34, 24), MU.rename_buf, sizeof MU.rename_buf, "Nome")) {
                if (MU.rename_buf[0]) snprintf(MU.pl[i].name, 48, "%s", MU.rename_buf);
                MU.renaming = false;
                pl_save();
            } else if (u->focus != 129900) u->focus = 129900;
        } else if (ui_sidebar_item(u, 129100 + i, r, MU.pl[i].name, MU.view == 10 + i, g_list)) {
            if (MU.view == 10 + i && u->dblclick) { MU.renaming = true; snprintf(MU.rename_buf, 48, "%s", MU.pl[i].name); }
            MU.view = 10 + i; MU.scroll = 0;
        }
        y += 30;
    }
    if (ui_button_style(u, UI_ID, rect_make(10, u->ch - 36, 140, 28), "Nova Playlist", BTN_PLAIN_GRAY, MU.npl < MAXPL, glyph_plus)) {
        snprintf(MU.pl[MU.npl].name, 48, "Playlist %d", MU.npl + 1);
        MU.pl[MU.npl].n = 0;
        MU.view = 10 + MU.npl++;
        MU.renaming = true;
        snprintf(MU.rename_buf, 48, "%s", MU.pl[MU.view - 10].name);
        pl_save();
    }
    /* reprodutor na barra de ferramentas (como no Música do Tahoe) */
    int px = sb + 16;
    rect_t bprev = rect_make(px, TOOLBAR_Y(28), 30, 28), bplay = rect_make(px + 34, TOOLBAR_Y(28), 34, 28), bnext = rect_make(px + 72, TOOLBAR_Y(28), 30, 28);
    { float t1[] = {bprev.x + 15.0f, bprev.y + 8.0f, bprev.x + 15.0f, bprev.y + 20.0f, bprev.x + 7.0f, bprev.y + 14.0f}, t2[] = {bprev.x + 24.0f, bprev.y + 8.0f, bprev.x + 24.0f, bprev.y + 20.0f, bprev.x + 16.0f, bprev.y + 14.0f}; gfx_polygon(s, t1, 3, T.text); gfx_polygon(s, t2, 3, T.text); }
    if (MU.playing) { gfx_fill(s, bplay.x + 11, bplay.y + 7, 4, 14, T.text); gfx_fill(s, bplay.x + 19, bplay.y + 7, 4, 14, T.text); }
    else { float t[] = {bplay.x + 11.0f, bplay.y + 6.0f, bplay.x + 25.0f, bplay.y + 14.0f, bplay.x + 11.0f, bplay.y + 22.0f}; gfx_polygon(s, t, 3, T.text); }
    { float t1[] = {bnext.x + 6.0f, bnext.y + 8.0f, bnext.x + 6.0f, bnext.y + 20.0f, bnext.x + 14.0f, bnext.y + 14.0f}, t2[] = {bnext.x + 15.0f, bnext.y + 8.0f, bnext.x + 15.0f, bnext.y + 20.0f, bnext.x + 23.0f, bnext.y + 14.0f}; gfx_polygon(s, t1, 3, T.text); gfx_polygon(s, t2, 3, T.text); }
    if (ui_clicked(u, UI_ID, bprev)) next(-1);
    if (ui_clicked(u, UI_ID, bplay) || (u->key == ' ' && !u->focus)) {
        if (MU.playing) stop();
        else if (MU.pcm) { MU.playing = audio_available(); MU.t0 = timer_ticks(); MU.pos0 = MU.pos; MU.fed = 0; }
        else { int st = MU.sel >= 0 ? MU.sel : -1; if (st < 0) for (int i = 0; i < MU.nt; i++) if (in_view(i)) { st = i; break; } build_queue(st); play_track(st); }
        u->key_used = true;
    }
    if (ui_clicked(u, UI_ID, bnext)) next(1);
    /* cápsula "Tocando Agora" */
    int cw = MIN(380, u->cw - sb - 500);
    rect_t cap = rect_make(sb + 130 + (u->cw - sb - 130 - 210 - cw) / 2, TOOLBAR_Y(34), cw, 34);
    gfx_round_rect(s, cap.x, cap.y, cap.w, cap.h, 8, T.dark ? RGBA(255, 255, 255, 18) : RGBA(0, 0, 0, 8));
    if (MU.cur >= 0) {
        art(s, rect_make(cap.x + 3, cap.y + 3, 28, 28), MU.t[MU.cur].album);
        ui_label_center(u, rect_make(cap.x + 34, cap.y + 2, cap.w - 40, 16), MU.t[MU.cur].title, FONT_SEMIBOLD, 11.5f, T.text);
        rect_t pb = rect_make(cap.x + 40, cap.y + 24, cap.w - 52, 4);
        gfx_round_rect(s, pb.x, pb.y, pb.w, pb.h, 2, T.dark ? RGBA(255, 255, 255, 40) : RGBA(0, 0, 0, 25));
        if (MU.frames) gfx_round_rect(s, pb.x, pb.y, (int)((u64)pb.w * MU.pos / MU.frames), pb.h, 2, T.text2);
        rect_t hit = rect_make(pb.x, pb.y - 6, pb.w, 16);
        if (ui_hit(u, hit) && u->down && MU.frames) seek((u32)((u64)MU.frames * CLAMP(u->mx - pb.x, 0, pb.w) / pb.w));
    } else {
        glyph_sb_music(s, cap.x + cap.w / 2 - 9, cap.y + 8, 18, T.text3);
    }
    /* aleatório, repetir, volume */
    int rx = u->cw - 12 - 110;
    ui_slider(u, UI_ID, rect_make(rx, TOOLBAR_Y(22), 110, 22), &MU.vol, 0, 100);
    if ((int)MU.vol != audio_get_volume()) audio_set_volume((int)MU.vol);
    rx -= 8 + 36;
    static const char *rep[3] = {"↻", "↻ 1", "↻ 1"};
    if (ui_button_style(u, UI_ID, rect_make(rx, TOOLBAR_Y(28), 36, 28), MU.repeat == 2 ? "1" : "↻", MU.repeat ? BTN_TINTED : BTN_PLAIN_GRAY, true, NULL)) MU.repeat = (MU.repeat + 1) % 3;
    (void)rep;
    rx -= 8 + 36;
    if (ui_button_style(u, UI_ID, rect_make(rx, TOOLBAR_Y(28), 36, 28), "", MU.shuffle ? BTN_TINTED : BTN_PLAIN_GRAY, true, g_shuffle)) { MU.shuffle = !MU.shuffle; if (MU.cur >= 0) build_queue(MU.cur); }
    /* conteúdo */
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg);
    if (MU.view == 1 && MU.album_sel < 0) {
        /* grade de álbuns */
        ui_label(u, a.x + 24, a.y + 14, "Álbuns", FONT_BOLD, 24, T.text);
        int cwd = 170, gap = 22, cols = MAX(1, (a.w - 48 + gap) / (cwd + gap));
        for (int i = 0; i < MU.nalb; i++) {
            rect_t r = rect_make(a.x + 24 + (i % cols) * (cwd + gap), a.y + 56 + (i / cols) * (cwd + 60), cwd, cwd);
            art(s, r, MU.albums[i]);
            text_draw_ellipsis(s, font_get(FONT_SEMIBOLD), 13, r.x, r.y + cwd + 20, cwd, T.text, MU.albums[i]);
            if (ui_clicked(u, 129200 + i, r)) MU.album_sel = i;
        }
        if (MU.pcm && MU.playing) u->repaint = true;
        return;
    }
    const char *title = MU.view == 0 ? "Músicas" : MU.view == 1 ? MU.albums[MU.album_sel] : MU.view == 2 ? "Adicionadas Recentemente" : MU.pl[MU.view - 10].name;
    if (MU.view == 1 && ui_button_style(u, UI_ID, rect_make(a.x + 20, a.y + 14, 30, 26), "‹", BTN_GRAY, true, NULL)) { MU.album_sel = -1; return; }
    ui_label(u, a.x + (MU.view == 1 ? 60 : 24), a.y + 14, title, FONT_BOLD, 24, T.text);
    int hy = a.y + 56;
    static const char *cols[3] = {"Nome", "Álbum", "Duração"};
    int cx[3] = {a.x + 56, a.x + a.w / 2, a.x + a.w - 100};
    for (int i = 0; i < 3; i++) text_draw(s, font_get(FONT_SEMIBOLD), 11.5f, cx[i], hy + 14, T.text2, cols[i]);
    gfx_fill(s, a.x + 16, hy + 22, a.w - 32, 1, T.separator);
    rect_t view = rect_make(a.x, hy + 24, a.w, a.h - (hy + 24 - a.y));
    rect_t pc = surface_push_clip(s, view);
    int ry = view.y - MU.scroll, n = 0;
    for (int i = 0; i < MU.nt; i++) {
        if (!in_view(i)) continue;
        track_t *t = &MU.t[i];
        rect_t r = rect_make(a.x, ry, a.w, 30);
        bool sel = MU.sel == i, cur = MU.cur == i;
        ui_list_row(u, 129300 + i, r, sel, n % 2);
        u32 tc = sel ? RGB(255, 255, 255) : cur ? T.accent : T.text;
        if (cur && MU.playing) {
            /* barras animadas de "tocando" */
            for (int k = 0; k < 3; k++) { float hh = 4 + 8 * kit_fabs(fsin(timer_ticks() / 180.0f + k * 1.3f)); gfx_fill(s, a.x + 26 + k * 5, (int)(ry + 22 - hh), 3, (int)hh, tc); }
        } else if (cur) glyph_sb_music(s, a.x + 24, ry + 7, 16, tc);
        text_draw_ellipsis(s, font_get(FONT_REGULAR), 13, cx[0], ry + 20, cx[1] - cx[0] - 10, tc, t->title);
        text_draw_ellipsis(s, font_get(FONT_REGULAR), 13, cx[1], ry + 20, cx[2] - cx[1] - 10, sel ? tc : T.text2, t->album);
        char d[16];
        fmt_t(d, sizeof d, t->frames);
        text_draw(s, font_get(FONT_REGULAR), 13, cx[2], ry + 20, sel ? tc : T.text2, d);
        if (ui_clicked(u, 129300 + i, r)) { MU.sel = i; if (u->dblclick) { build_queue(i); play_track(i); } }
        ry += 30;
        n++;
    }
    if (!n) ui_label_center(u, rect_make(a.x, view.y + 40, a.w, 20), MU.search[0] ? "Nenhum resultado" : "Coloque arquivos .wav na pasta Música", FONT_SEMIBOLD, 14, T.text3);
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, n * 30 + 20, &MU.scroll);
    if (u->key == KEY_ENTER && !u->focus && MU.sel >= 0) { build_queue(MU.sel); play_track(MU.sel); u->key_used = true; }
    if ((u->mods & MOD_CMD) && u->key == KEY_RIGHT) { next(1); u->key_used = true; }
    if ((u->mods & MOD_CMD) && u->key == KEY_LEFT) { next(-1); u->key_used = true; }
    if (MU.playing) u->repaint = true;
}

static void on_menu(app_t *a, int act);
static void tick(app_t *a) {
    feed();
    if (MU.demo_next >= 0 && MU.demo_next < (int)ARRAY_LEN(demos) && !MU.playing) {
        char p[260];
        snprintf(p, sizeof p, "%s/Demos/%s.wav", mdir(), demos[MU.demo_next].name);
        render_demo(&demos[MU.demo_next], p);
        if (++MU.demo_next == (int)ARRAY_LEN(demos)) MU.demo_next = -1;
        int keep = MU.cur;
        library_scan();
        MU.cur = keep;
        if (MU.w) wm_invalidate(MU.w);
    }
}
static void on_close(window_t *w) { MU.w = NULL; }   /* continua tocando com a janela fechada, como no macOS */
static void launch(app_t *a) {
    if (MU.w) { wm_focus(MU.w); return; }
    library_scan();
    MU.w = wm_create(&app_musica, "Música", -1, -1, 1040, 640, WIN_RESIZABLE);
    if (!MU.w) return;
    MU.w->paint = paint;
    MU.w->on_close = on_close;
    MU.w->toolbar_h = TOOLBAR_H;
    MU.w->sidebar_w = 230;
    MU.w->min_w = 820;
    MU.w->min_h = 460;
    MU.w->title_x = -1;
    on_menu(a, 0);
}
static void on_quit(app_t *a) { stop(); }

enum { M_PLAY = 1, M_NEXT, M_PREV, M_SHUF, M_REP, M_ADDPL = 100 };
static menu_item_t m_ctl[] = {{"Reproduzir/Pausar", "Espaço", M_PLAY}, {"Próxima", "⌘→", M_NEXT}, {"Anterior", "⌘←", M_PREV}, {NULL}, {"Aleatório", NULL, M_SHUF}, {"Repetir", NULL, M_REP}};
static menu_item_t m_song[MAXPL + 1];
static menu_t menus[] = {{"Controles", m_ctl, 6}, {"Adicionar à Playlist", m_song, 0}};
static void on_menu(app_t *a, int act) {
    if (act == M_PLAY) { if (MU.playing) stop(); else if (MU.pcm) { MU.playing = true; MU.t0 = timer_ticks(); MU.pos0 = MU.pos; MU.fed = 0; } }
    else if (act == M_NEXT) next(1);
    else if (act == M_PREV) next(-1);
    else if (act == M_SHUF) MU.shuffle = !MU.shuffle;
    else if (act == M_REP) MU.repeat = (MU.repeat + 1) % 3;
    else if (act >= M_ADDPL && act < M_ADDPL + MAXPL && MU.sel >= 0) {
        playlist_t *pl = &MU.pl[act - M_ADDPL];
        if (pl->n < 128) { pl->items[pl->n++] = MU.sel; pl_save(); notify("Adicionada à playlist", pl->name); }
    }
    /* submenu de playlists atualizado */
    int k = 0;
    for (int i = 0; i < MU.npl && k < MAXPL; i++) m_song[k++] = (menu_item_t){MU.pl[i].name, NULL, M_ADDPL + i};
    menus[1].count = k;
    menus[1].title = "Adicionar à Playlist";
}
static void icon(surface_t *s, int x, int y, int sz) { icon_musica(s, x, y, sz); }
app_t app_musica = {.name = "Música", .exec = "musica", .draw_icon = icon, .launch = launch, .tick = tick, .on_quit = on_quit, .on_menu = on_menu,
                    .menus = menus, .nmenus = 2, .pinned = true, .always_tick = true};
