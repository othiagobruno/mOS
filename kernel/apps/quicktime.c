/* mOS — QuickTime Player: Nova Gravação de Tela (GIF animado, codificador LZW próprio), Nova Gravação de
 * Áudio (WAV do microfone) e reprodução de WAV e GIF animados (decodificador LZW próprio), com barra de
 * controles de vidro, avançar/voltar e tempo. Gravações em ~/Filmes. */
#include "kit.h"

extern app_t app_quicktime;

/* ------------------------------------------------------------ GIF: codificação (paleta 6x7x6 fixa) */
typedef struct { u8 *p; int len, cap; u32 acc; int nbits; u8 blk[256]; int bl; } bw_t;
static void bw_byte(bw_t *b, u8 v) { if (b->len + 1 > b->cap) { b->cap = MAX(4096, b->cap * 2); b->p = krealloc(b->p, b->cap); } b->p[b->len++] = v; }
static void bw_flushblk(bw_t *b) { if (!b->bl) return; bw_byte(b, (u8)b->bl); for (int i = 0; i < b->bl; i++) bw_byte(b, b->blk[i]); b->bl = 0; }
static void bw_code(bw_t *b, int code, int size) {
    b->acc |= (u32)code << b->nbits;
    b->nbits += size;
    while (b->nbits >= 8) { b->blk[b->bl++] = (u8)b->acc; b->acc >>= 8; b->nbits -= 8; if (b->bl == 255) bw_flushblk(b); }
}
static u8 pal_lut[32768];
static bool pal_ready;
static u8 pal_index(u32 c) {
    if (!pal_ready) {
        for (int i = 0; i < 32768; i++) { int r = (i >> 10) * 255 / 31, g = ((i >> 5) & 31) * 255 / 31, b = (i & 31) * 255 / 31; pal_lut[i] = (u8)((r * 5 / 255) * 42 + (g * 6 / 255) * 6 + b * 5 / 255); }
        pal_ready = true;
    }
    return pal_lut[(C_R(c) >> 3) << 10 | (C_G(c) >> 3) << 5 | C_B(c) >> 3];
}
/* LZW com dicionário em tabela de dispersão */
static void lzw_encode(bw_t *b, const u8 *px, int n) {
    const int min = 8, clear = 256, eoi = 257;
    static i32 hk[8192];
    static i16 hv[8192];
    int next = 258, size = 9;
    for (int i = 0; i < 8192; i++) hk[i] = -1;
    bw_byte(b, min);
    bw_code(b, clear, size);
    int prefix = px[0];
    for (int i = 1; i < n; i++) {
        int c = px[i];
        i32 key = prefix << 8 | c;
        int h = (int)((u32)key * 2654435761u >> 19) & 8191;
        while (hk[h] != -1 && hk[h] != key) h = (h + 1) & 8191;
        if (hk[h] == key) { prefix = hv[h]; continue; }
        bw_code(b, prefix, size);
        if (next < 4096) {
            hk[h] = key; hv[h] = (i16)next++;
            if (next > (1 << size) && size < 12) size++;
        } else {
            bw_code(b, clear, size);
            for (int k = 0; k < 8192; k++) hk[k] = -1;
            next = 258; size = 9;
        }
        prefix = c;
    }
    bw_code(b, prefix, size);
    bw_code(b, eoi, size);
    if (b->nbits) { b->blk[b->bl++] = (u8)b->acc; if (b->bl == 255) bw_flushblk(b); }
    b->acc = 0; b->nbits = 0;
    bw_flushblk(b);
    bw_byte(b, 0);
}
static void gif_begin(bw_t *b, int w, int h) {
    const char *hdr = "GIF89a";
    for (int i = 0; i < 6; i++) bw_byte(b, (u8)hdr[i]);
    bw_byte(b, (u8)w); bw_byte(b, (u8)(w >> 8)); bw_byte(b, (u8)h); bw_byte(b, (u8)(h >> 8));
    bw_byte(b, 0xF7); bw_byte(b, 0); bw_byte(b, 0);   /* paleta global de 256 cores */
    for (int i = 0; i < 256; i++) {
        int r = i / 42, g = (i / 6) % 7, bl = i % 6;
        bw_byte(b, (u8)(i < 252 ? r * 255 / 5 : 0)); bw_byte(b, (u8)(i < 252 ? g * 255 / 6 : 0)); bw_byte(b, (u8)(i < 252 ? bl * 255 / 5 : 0));
    }
    /* repetir para sempre (NETSCAPE2.0) */
    const u8 ext[] = {0x21, 0xFF, 0x0B, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0', 3, 1, 0, 0, 0};
    for (int i = 0; i < (int)sizeof ext; i++) bw_byte(b, ext[i]);
}
static void gif_frame(bw_t *b, const u8 *idx, int w, int h, int delay_cs) {
    const u8 gce[] = {0x21, 0xF9, 4, 0, (u8)delay_cs, (u8)(delay_cs >> 8), 0, 0};
    for (int i = 0; i < 8; i++) bw_byte(b, gce[i]);
    bw_byte(b, 0x2C);
    for (int i = 0; i < 4; i++) bw_byte(b, 0);
    bw_byte(b, (u8)w); bw_byte(b, (u8)(w >> 8)); bw_byte(b, (u8)h); bw_byte(b, (u8)(h >> 8));
    bw_byte(b, 0);
    lzw_encode(b, idx, w * h);
}

/* ------------------------------------------------------------ GIF: decodificação de todos os quadros */
/* animação: os descritores dos quadros são lidos uma vez; cada quadro é decodificado sob demanda sobre a tela
 * acumulada (voltar no tempo recomeça do primeiro quadro) */
typedef struct { u32 off; int delay; } aframe_t;
typedef struct {
    const u8 *d;
    usize size;
    int W, H, n, done;        /* done = último quadro já composto (-1 nenhum) */
    u32 gpal[256];
    aframe_t *fr;
    int *delay;
    surface_t *canvas;
    u8 *idx, *data;
} anim_t;
static int lzw_decode(const u8 *data, int dlen, int minsize, u8 *out, int outcap) {
    static u16 prefix[4096];
    static u8 suffix[4096], stack[4097];
    int clear = 1 << minsize, eoi = clear + 1, size = minsize + 1, next = clear + 2, old = -1, n = 0;
    u8 first = 0;
    u32 acc = 0;
    int bits = 0;
    for (int i = 0; i < clear; i++) { prefix[i] = 0xFFFF; suffix[i] = (u8)i; }
    for (int pos = 0;;) {
        while (bits < size) { if (pos >= dlen) return n; acc |= (u32)data[pos++] << bits; bits += 8; }
        int code = (int)(acc & ((1u << size) - 1));
        acc >>= size; bits -= size;
        if (code == clear) { size = minsize + 1; next = clear + 2; old = -1; continue; }
        if (code == eoi) return n;
        int c = code, sp = 0;
        if (old < 0) { if (n < outcap) out[n++] = suffix[code]; first = suffix[code]; old = code; continue; }
        if (code >= next) { stack[sp++] = first; c = old; }
        while (c >= clear && sp < 4096) { stack[sp++] = suffix[c]; c = prefix[c]; }
        stack[sp++] = suffix[c];
        first = suffix[c];
        while (sp && n < outcap) out[n++] = stack[--sp];
        if (next < 4096) { prefix[next] = (u16)old; suffix[next] = first; next++; if (next == (1 << size) && size < 12) size++; }
        old = code;
    }
}
static bool gif_load(const char *path, anim_t *a) {
    memset(a, 0, sizeof *a);
    vnode_t *v = vfs_lookup(path);
    if (!v || v->size < 13) return false;
    vfs_ensure(v);
    const u8 *d = v->data, *end = v->data + v->size;
    if (memcmp(d, "GIF8", 4)) return false;
    a->d = d; a->size = v->size;
    a->W = d[6] | d[7] << 8; a->H = d[8] | d[9] << 8;
    if (a->W <= 0 || a->H <= 0 || a->W > 2048 || a->H > 2048) return false;
    const u8 *p = d + 13;
    if (d[10] & 0x80) { int ns = 2 << (d[10] & 7); for (int i = 0; i < ns && p + 3 <= end; i++, p += 3) a->gpal[i] = RGB(p[0], p[1], p[2]); }
    int delay = 10, cap = 0;
    while (p < end) {
        if (*p == 0x3B) break;
        if (*p == 0x21) {
            if (p + 6 < end && p[1] == 0xF9) delay = p[4] | p[5] << 8;
            p += 2;
            while (p < end && *p) p += *p + 1;
            p++;
            continue;
        }
        if (*p != 0x2C || p + 10 > end) break;
        if (a->n == cap) { cap = MAX(32, cap * 2); a->fr = krealloc(a->fr, sizeof(aframe_t) * cap); a->delay = krealloc(a->delay, sizeof(int) * cap); }
        a->fr[a->n].off = (u32)(p - d);
        a->delay[a->n] = MAX(2, delay) * 10;
        a->n++;
        int fl = p[9];
        p += 10;
        if (fl & 0x80) p += 3 * (2 << (fl & 7));
        p++;
        while (p < end && *p) p += *p + 1;
        p++;
    }
    if (!a->n) return false;
    a->canvas = surface_new(a->W, a->H);
    gfx_fill(a->canvas, 0, 0, a->W, a->H, RGB(0, 0, 0));
    a->idx = kmalloc((usize)a->W * a->H);
    a->data = kmalloc(a->size);
    a->done = -1;
    return true;
}
static void anim_decode(anim_t *a, int k) {
    const u8 *p = a->d + a->fr[k].off, *end = a->d + a->size;
    int fx = p[1] | p[2] << 8, fy = p[3] | p[4] << 8, fw = p[5] | p[6] << 8, fh = p[7] | p[8] << 8, fl = p[9];
    p += 10;
    u32 lpal[256], *pal = a->gpal;
    if (fl & 0x80) { int ns = 2 << (fl & 7); for (int i = 0; i < ns && p + 3 <= end; i++, p += 3) lpal[i] = RGB(p[0], p[1], p[2]); pal = lpal; }
    int minsize = *p++, dl = 0;
    while (p < end && *p) { int len = *p++; if (p + len > end) break; memcpy(a->data + dl, p, len); dl += len; p += len; }
    if (fw <= 0 || fh <= 0 || (usize)fw * fh > (usize)a->W * a->H) return;
    int got = lzw_decode(a->data, dl, minsize, a->idx, fw * fh);
    bool inter = fl & 0x40;
    for (int i = 0; i < got; i++) {
        int row = i / fw, col = i % fw;
        if (inter) {
            static const int st[4] = {0, 4, 2, 1}, sp[4] = {8, 8, 4, 2};
            int r = row, cnt = 0;
            for (int pass = 0; pass < 4; pass++) { int kk = (fh - st[pass] + sp[pass] - 1) / sp[pass]; if (r < cnt + kk) { row = st[pass] + (r - cnt) * sp[pass]; break; } cnt += kk; }
        }
        int x = fx + col, y = fy + row;
        if (x < a->W && y < a->H) a->canvas->px[(usize)y * a->canvas->stride + x] = pal[a->idx[i]] | 0xFF000000;
    }
}
static surface_t *anim_frame(anim_t *a, int k) {
    if (k < a->done) { a->done = -1; gfx_fill(a->canvas, 0, 0, a->W, a->H, RGB(0, 0, 0)); }
    while (a->done < k) anim_decode(a, ++a->done);
    return a->canvas;
}
static void anim_free(anim_t *a) {
    if (a->canvas) surface_free(a->canvas);
    kfree(a->fr); kfree(a->delay); kfree(a->idx); kfree(a->data);
    memset(a, 0, sizeof *a);
}

/* ------------------------------------------------------------ estado */
enum { Q_NONE, Q_VIDEO, Q_AUDIO, Q_RECSCREEN, Q_RECAUDIO };
static struct {
    window_t *w;
    int mode;
    char path[220], name[96];
    anim_t anim;
    int frame;
    u64 frame_t, play_t0, acc_ms;
    bool playing;
    i16 *pcm;
    u32 frames, pos, pos0;
    u64 fed;
    /* gravação de tela */
    bw_t gif;
    int gw, gh, nframes;
    u64 last_cap, rec_start;
    u8 *idx;
    /* gravação de áudio */
    i16 *rbuf;
    u32 rn, rcap;
} Q;

static const char *films(void) { static char d[200]; snprintf(d, sizeof d, "%s/Filmes", CUR_HOME); vfs_mkdir(d); return d; }
static void q_reset(void) {
    if (Q.anim.n) anim_free(&Q.anim);
    if (Q.pcm) { kfree(Q.pcm); Q.pcm = NULL; }
    Q.playing = false; Q.frame = 0; Q.pos = 0; Q.mode = Q_NONE;
}
static void q_open(const char *path) {
    q_reset();
    snprintf(Q.path, sizeof Q.path, "%s", path);
    snprintf(Q.name, sizeof Q.name, "%s", strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
    const char *e = vfs_ext(path);
    if (!strcasecmp(e, "gif")) { if (gif_load(path, &Q.anim)) Q.mode = Q_VIDEO; }
    else if (!strcasecmp(e, "wav")) { Q.pcm = kit_load_wav(path, &Q.frames); if (Q.pcm) Q.mode = Q_AUDIO; }
    if (Q.mode == Q_NONE) notify("QuickTime Player", "Formato não suportado (use GIF animado ou WAV).");
    if (Q.w) wm_set_title(Q.w, Q.name);
}
static u64 anim_total(void) { u64 t = 0; for (int i = 0; i < Q.anim.n; i++) t += Q.anim.delay[i]; return t; }
static void play(void) {
    Q.playing = true;
    Q.play_t0 = timer_ticks();
    Q.frame_t = timer_ticks();
    Q.pos0 = Q.pos; Q.fed = 0;
}
static void feed(void) {
    if (!Q.playing) return;
    if (Q.mode == Q_VIDEO && Q.anim.n) {
        while (timer_ticks() - Q.frame_t >= (u64)Q.anim.delay[Q.frame]) {
            Q.frame_t += Q.anim.delay[Q.frame];
            if (++Q.frame >= Q.anim.n) { Q.frame = 0; Q.playing = false; break; }
        }
    } else if (Q.mode == Q_AUDIO && Q.pcm) {
        u64 el = timer_ticks() - Q.play_t0;
        u32 cur = Q.pos0 + (u32)(el * 48);
        if (cur >= Q.frames) { Q.playing = false; Q.pos = 0; return; }
        Q.pos = cur;
        while (Q.fed < el + 300) {
            u32 from = Q.pos0 + (u32)(Q.fed * 48);
            if (from >= Q.frames) break;
            u32 n = MIN(4800u, Q.frames - from);
            audio_play(Q.pcm + (usize)from * 2, n);
            Q.fed += n / 48;
            if (n < 4800) break;
        }
    }
}

static void rec_screen_start(void) {
    q_reset();
    Q.gw = g_screen->w / 4; Q.gh = g_screen->h / 4;
    memset(&Q.gif, 0, sizeof Q.gif);
    gif_begin(&Q.gif, Q.gw, Q.gh);
    Q.idx = kmalloc((usize)Q.gw * Q.gh);
    Q.nframes = 0;
    Q.mode = Q_RECSCREEN;
    Q.rec_start = Q.last_cap = timer_ticks();
    notify("Gravação de Tela", "Gravando a tela inteira. Clique em Parar (ou ⌘.) para salvar.");
}
static void rec_screen_frame(void) {
    if (Q.mode != Q_RECSCREEN || timer_ticks() - Q.last_cap < 500) return;
    Q.last_cap = timer_ticks();
    for (int y = 0; y < Q.gh; y++)
        for (int x = 0; x < Q.gw; x++) {
            static const i8 bayer[4][4] = {{-8, 0, -6, 2}, {4, -4, 6, -2}, {-5, 3, -7, 1}, {7, -1, 5, -3}};
            u32 c = g_screen->px[(usize)(y * 4) * g_screen->stride + x * 4];
            int d = bayer[y & 3][x & 3] * 3;
            int r = CLAMP((int)C_R(c) + d, 0, 255), g = CLAMP((int)C_G(c) + d, 0, 255), b = CLAMP((int)C_B(c) + d, 0, 255);
            Q.idx[y * Q.gw + x] = pal_index(RGB(r, g, b));
        }
    gif_frame(&Q.gif, Q.idx, Q.gw, Q.gh, 50);
    Q.nframes++;
    if (Q.gif.len > 24 * 1024 * 1024 || Q.nframes >= 2400) { notify("Gravação de Tela", "Limite de tamanho atingido."); }
}
static void rec_screen_stop(void) {
    if (Q.mode != Q_RECSCREEN) return;
    bw_byte(&Q.gif, 0x3B);
    datetime_t d;
    rtc_now(&d);
    char p[240];
    snprintf(p, sizeof p, "%s/Gravação de Tela %04d-%02d-%02d às %02d.%02d.%02d.gif", films(), d.year, d.month, d.day, d.hour, d.minute, d.second);
    vfs_write_file(p, Q.gif.p, Q.gif.len);
    kfree(Q.gif.p);
    kfree(Q.idx);
    Q.mode = Q_NONE;
    q_open(p);
    notify("Gravação salva", strrchr(p, '/') + 1);
}
static void rec_audio_start(void) {
    if (!audio_input_available()) { notify("QuickTime Player", "Nenhum microfone disponível."); return; }
    q_reset();
    Q.rcap = 48000 * 60;
    Q.rbuf = kmalloc((usize)Q.rcap * 4);
    Q.rn = 0;
    Q.mode = Q_RECAUDIO;
    Q.rec_start = timer_ticks();
    i16 tmp[2048];
    while (audio_capture_read(tmp, 1024) > 0) {}
}
static void rec_audio_pump(void) {
    if (Q.mode != Q_RECAUDIO) return;
    i16 tmp[4096];
    u32 got;
    while ((got = audio_capture_read(tmp, 2048)) > 0) {
        if (Q.rn + got > Q.rcap) { if (Q.rcap >= 48000u * 600) break; Q.rcap *= 2; Q.rbuf = krealloc(Q.rbuf, (usize)Q.rcap * 4); }
        memcpy(Q.rbuf + (usize)Q.rn * 2, tmp, (usize)got * 4);
        Q.rn += got;
    }
}
static void rec_audio_stop(void) {
    if (Q.mode != Q_RECAUDIO) return;
    rec_audio_pump();
    datetime_t d;
    rtc_now(&d);
    char p[240];
    snprintf(p, sizeof p, "%s/Gravação de Áudio %04d-%02d-%02d às %02d.%02d.%02d.wav", films(), d.year, d.month, d.day, d.hour, d.minute, d.second);
    kit_save_wav(p, Q.rbuf, Q.rn, 2, 48000);
    kfree(Q.rbuf);
    Q.rbuf = NULL;
    Q.mode = Q_NONE;
    q_open(p);
}

/* ------------------------------------------------------------ janela */
static void fmt_ms(char *o, int n, u64 ms) { snprintf(o, n, "%d:%02d", (int)(ms / 60000), (int)(ms / 1000 % 60)); }
static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    feed();
    rec_audio_pump();
    gfx_fill(s, 0, 0, u->cw, u->ch, RGB(18, 18, 20));
    if (Q.mode == Q_RECSCREEN || Q.mode == Q_RECAUDIO) {
        u64 el = timer_ticks() - Q.rec_start;
        char t[32];
        fmt_ms(t, sizeof t, el);
        gfx_circle(s, u->cw / 2.0f, u->ch / 2.0f - 40, 34, RGB(255, 59, 48));
        ui_label_center(u, rect_make(0, u->ch / 2 + 10, u->cw, 30), t, FONT_MEDIUM, 26, RGB(255, 255, 255));
        ui_label_center(u, rect_make(0, u->ch / 2 + 46, u->cw, 18), Q.mode == Q_RECSCREEN ? "Gravando a tela" : "Gravando áudio", FONT_REGULAR, 13, RGB(200, 200, 204));
        rect_t sb = rect_make(u->cw / 2 - 60, u->ch - 70, 120, 32);
        bool stop_now = ui_button_style(u, UI_ID, sb, "Parar", BTN_FILLED_RED | BTN_CAPSULE, true, NULL) || (u->pressed && ui_hit(u, sb));
        if (u->key == KEY_ESC || ((u->mods & MOD_CMD) && u->key == '.')) { stop_now = true; u->key_used = true; }
        if (stop_now) { if (Q.mode == Q_RECSCREEN) rec_screen_stop(); else rec_audio_stop(); return; }
        u->repaint = true;
        return;
    }
    if (Q.mode == Q_NONE) {
        ui_label_center(u, rect_make(0, u->ch / 2 - 90, u->cw, 30), "QuickTime Player", FONT_BOLD, 22, RGB(240, 240, 244));
        int bw = 220, x = u->cw / 2 - bw / 2, y = u->ch / 2 - 40;
        if (ui_button_style(u, UI_ID, rect_make(x, y, bw, 32), "Nova Gravação de Tela", BTN_GRAY | BTN_CAPSULE, true, NULL)) rec_screen_start();
        if (ui_button_style(u, UI_ID, rect_make(x, y + 42, bw, 32), "Nova Gravação de Áudio", BTN_GRAY | BTN_CAPSULE, true, NULL)) rec_audio_start();
        /* recentes em ~/Filmes */
        vnode_t *d = vfs_lookup(films());
        int k = 0, yy = y + 100;
        for (vnode_t *c = d && d->dir ? d->child : NULL; c && k < 6; c = c->next) {
            if (c->dir || (strcasecmp(vfs_ext(c->name), "gif") && strcasecmp(vfs_ext(c->name), "wav"))) continue;
            rect_t r = rect_make(u->cw / 2 - 200, yy, 400, 26);
            if (ui_hit(u, r)) gfx_round_rect(s, r.x, r.y, r.w, r.h, 6, RGBA(255, 255, 255, 24));
            text_draw_ellipsis(s, font_get(FONT_REGULAR), 12.5f, r.x + 10, r.y + 18, r.w - 20, RGB(220, 220, 226), c->name);
            if (ui_clicked(u, 131000 + k, r)) { char p[330]; vfs_path(c, p, sizeof p); q_open(p); }
            yy += 28;
            k++;
        }
        return;
    }
    /* conteúdo */
    rect_t view = rect_make(0, 0, u->cw, u->ch);
    if (Q.mode == Q_VIDEO && Q.anim.n) {
        surface_t *f = anim_frame(&Q.anim, Q.frame);
        float sc = MIN((float)view.w / f->w, (float)view.h / f->h);
        int dw = (int)(f->w * sc), dh = (int)(f->h * sc);
        gfx_blit_scaled(s, f, (view.w - dw) / 2, (view.h - dh) / 2, dw, dh, 255);
    } else {
        /* áudio: forma de onda grande */
        int cy = u->ch / 2 - 20;
        for (int i = 0; i < 160 && Q.frames; i++) {
            u32 a = (u32)((u64)Q.frames * i / 160), b = (u32)((u64)Q.frames * (i + 1) / 160);
            int mx = 0;
            for (u32 k = a; k < b; k += 64) { int v = Q.pcm[k * 2]; if (v < 0) v = -v; if (v > mx) mx = v; }
            float h = MAX(2.0f, mx / 32768.0f * 200);
            gfx_round_rect(s, 40 + i * (u->cw - 80) / 160.0f, cy - h / 2, MAX(1.0f, (u->cw - 80) / 160.0f - 1), h, 1, (float)i / 160 < (float)Q.pos / MAX(1u, Q.frames) ? RGB(255, 255, 255) : RGB(100, 100, 108));
        }
    }
    /* barra de controles de vidro (aparece com o mouse sobre a janela) */
    if (ui_hit(u, view) || !Q.playing) {
        rect_t bar = rect_make(u->cw / 2 - 220, u->ch - 76, 440, 56);
        glass_panel(s, bar.x, bar.y, bar.w, bar.h, 16, GLASS_REGULAR);
        u64 cur, total;
        if (Q.mode == Q_VIDEO) { total = anim_total(); cur = 0; for (int i = 0; i < Q.frame; i++) cur += Q.anim.delay[i]; }
        else { total = Q.frames / 48; cur = Q.pos / 48; }
        float cx = bar.x + bar.w / 2.0f, cy = bar.y + 18;
        rect_t pb = rect_make((int)cx - 14, (int)cy - 12, 28, 24);
        if (Q.playing) { gfx_fill(s, (int)cx - 7, (int)cy - 8, 5, 16, T.glass_text); gfx_fill(s, (int)cx + 2, (int)cy - 8, 5, 16, T.glass_text); }
        else { float t[] = {cx - 6, cy - 9, cx + 9, cy, cx - 6, cy + 9}; gfx_polygon(s, t, 3, T.glass_text); }
        if (ui_clicked(u, UI_ID, pb) || (u->key == ' ' && !u->focus)) { if (Q.playing) Q.playing = false; else { if (Q.mode == Q_VIDEO && Q.frame >= Q.anim.n - 1) Q.frame = 0; play(); } u->key_used = true; }
        rect_t rw = rect_make((int)cx - 64, (int)cy - 12, 30, 24), ff = rect_make((int)cx + 34, (int)cy - 12, 30, 24);
        text_draw_center(s, font_get(FONT_SEMIBOLD), 12, rw, T.glass_text, "−5s");
        text_draw_center(s, font_get(FONT_SEMIBOLD), 12, ff, T.glass_text, "+5s");
        int jump = ui_clicked(u, UI_ID, rw) ? -5000 : ui_clicked(u, UI_ID, ff) ? 5000 : 0;
        if (u->key == KEY_LEFT && !u->focus) jump = -5000;
        if (u->key == KEY_RIGHT && !u->focus) jump = 5000;
        if (jump) {
            i64 t = (i64)cur + jump;
            t = CLAMP(t, 0, (i64)total);
            if (Q.mode == Q_VIDEO) { u64 acc = 0; Q.frame = 0; while (Q.frame < Q.anim.n - 1 && acc + Q.anim.delay[Q.frame] <= (u64)t) acc += Q.anim.delay[Q.frame++]; Q.frame_t = timer_ticks(); }
            else { Q.pos = (u32)(t * 48); if (Q.playing) play(); }
            u->key_used = true;
        }
        char a[16], b[16];
        fmt_ms(a, sizeof a, cur);
        fmt_ms(b, sizeof b, total);
        text_draw(s, font_get(FONT_REGULAR), 11, bar.x + 14, bar.y + 48, T.glass_text2, a);
        int bwd = text_width(font_get(FONT_REGULAR), 11, b);
        text_draw(s, font_get(FONT_REGULAR), 11, bar.x + bar.w - 14 - bwd, bar.y + 48, T.glass_text2, b);
        rect_t tr = rect_make(bar.x + 50, bar.y + 42, bar.w - 100, 4);
        gfx_round_rect(s, tr.x, tr.y, tr.w, tr.h, 2, RGBA(128, 128, 128, 90));
        if (total) gfx_round_rect(s, tr.x, tr.y, (int)(tr.w * cur / total), tr.h, 2, T.glass_text);
        rect_t hit = rect_make(tr.x, tr.y - 6, tr.w, 16);
        if (ui_hit(u, hit) && u->down && total) {
            u64 t = total * (u64)CLAMP(u->mx - tr.x, 0, tr.w) / tr.w;
            if (Q.mode == Q_VIDEO) { u64 acc = 0; Q.frame = 0; while (Q.frame < Q.anim.n - 1 && acc + Q.anim.delay[Q.frame] <= t) acc += Q.anim.delay[Q.frame++]; Q.frame_t = timer_ticks(); }
            else { Q.pos = (u32)(t * 48); if (Q.playing) play(); }
        }
    }
    if (Q.playing) u->repaint = true;
}

static void tick(app_t *a) { rec_screen_frame(); rec_audio_pump(); feed(); }
static void on_close(window_t *w) { if (Q.mode == Q_RECSCREEN) rec_screen_stop(); if (Q.mode == Q_RECAUDIO) rec_audio_stop(); q_reset(); Q.w = NULL; }
static void open_window(void) {
    if (Q.w) { wm_bring_app(&app_quicktime); wm_focus(Q.w); return; }
    Q.w = wm_create(&app_quicktime, Q.name[0] ? Q.name : "QuickTime Player", -1, -1, 760, 480, WIN_RESIZABLE | WIN_DARK_CHROME);
    if (!Q.w) return;
    Q.w->paint = paint;
    Q.w->on_close = on_close;
    Q.w->bg = RGB(18, 18, 20);
    Q.w->min_w = 480;
    Q.w->min_h = 320;
}
static void launch(app_t *a) { open_window(); }
void quicktime_open_file(const char *path) { app_quicktime.running = true; open_window(); q_open(path); }

enum { M_SCREEN = 1, M_AUDIO, M_STOP };
static menu_item_t m_file[] = {{"Nova Gravação de Tela", "⌃⌘N", M_SCREEN}, {"Nova Gravação de Áudio", "⌃⌥⌘N", M_AUDIO}, {NULL}, {"Parar Gravação", "⌘.", M_STOP}};
static menu_t menus[] = {{"Arquivo", m_file, 4}};
static void on_menu(app_t *a, int act) {
    if (act == M_SCREEN) rec_screen_start();
    else if (act == M_AUDIO) { open_window(); rec_audio_start(); }
    else if (act == M_STOP) { if (Q.mode == Q_RECSCREEN) rec_screen_stop(); else if (Q.mode == Q_RECAUDIO) rec_audio_stop(); if (Q.w) wm_focus(Q.w); }
}
static void icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(60, 60, 66), RGB(20, 20, 24));
    gfx_circle(s, x + 30 * u, y + 30 * u, 16 * u, RGB(100, 170, 255));
    gfx_circle(s, x + 30 * u, y + 30 * u, 11 * u, RGB(20, 20, 24));
    gfx_line(s, x + 41 * u, y + 41 * u, x + 50 * u, y + 50 * u, 6 * u, RGB(100, 170, 255));
    float t[] = {x + 26 * u, y + 24 * u, x + 37 * u, y + 30 * u, x + 26 * u, y + 36 * u};
    gfx_polygon(s, t, 3, RGB(255, 255, 255));
}
app_t app_quicktime = {.name = "QuickTime Player", .exec = "quicktime", .draw_icon = icon, .launch = launch, .tick = tick, .on_menu = on_menu, .menus = menus, .nmenus = 1, .always_tick = true};
