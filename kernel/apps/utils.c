/* mOS — Monitor de Atividade, Console, Campo Minado, Visualizador e Música */
#include "apps.h"
#include "net.h"
#include "proc.h"

volatile u64 g_idle_ticks;

/* ================================================================== Monitor de Atividade */
static window_t *act_w;
static int act_tab;
static float cpu_hist[120], mem_hist[120];
static u64 last_sample, last_idle, last_tsc_ms;

static void act_sample(void) {
    u64 now = timer_ticks();
    if (now - last_sample < 500) return;
    u64 d = now - last_tsc_ms;
    float idle = d ? (float)(g_idle_ticks - last_idle) / d : 1;
    last_idle = g_idle_ticks;
    last_tsc_ms = now;
    last_sample = now;
    memmove(cpu_hist, cpu_hist + 1, sizeof(float) * 119);
    memmove(mem_hist, mem_hist + 1, sizeof(float) * 119);
    cpu_hist[119] = CLAMP(1.0f - idle, 0.0f, 1.0f);
    mem_hist[119] = 1.0f - (float)pmm_free_pages() / pmm_total_pages();
    if (act_w) wm_invalidate(act_w);
}

static void graph(surface_t *s, rect_t r, float *h, u32 c) {
    gfx_fill(s, r.x, r.y, r.w, r.h, RGB(20, 20, 22));
    for (int i = 1; i < 4; i++) gfx_fill(s, r.x, r.y + r.h * i / 4, r.w, 1, RGBA(255, 255, 255, 20));
    for (int i = 1; i < 120; i++) {
        float x0 = r.x + (i - 1) * r.w / 119.0f, x1 = r.x + i * r.w / 119.0f;
        gfx_line(s, x0, r.y + r.h - h[i - 1] * r.h, x1, r.y + r.h - h[i] * r.h, 1.5f, c);
    }
}

static void paint_act(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    static const char *tabs[] = {"CPU", "Memória", "Rede"};
    act_tab = ui_segmented(u, UI_ID, rect_make(u->cw / 2 - 120, TOOLBAR_Y(28), 240, 28), tabs, 3, act_tab);
    int top = w->toolbar_h;
    gfx_fill(s, 0, top, u->cw, u->ch - top, T.content_bg);
    /* tabela de processos */
    int y = top;
    gfx_fill(s, 0, y, u->cw, 22, T.dark ? RGB(42, 42, 42) : RGB(246, 246, 246));
    ui_label(u, 12, y + 4, "Nome do Processo", FONT_REGULAR, 12, T.text2);
    ui_label(u, u->cw - 300, y + 4, act_tab == 1 ? "Memória" : act_tab == 2 ? "Pacotes" : "Tempo de CPU", FONT_REGULAR, 12, T.text2);
    ui_label(u, u->cw - 170, y + 4, "Tipo", FONT_REGULAR, 12, T.text2);
    ui_label(u, u->cw - 70, y + 4, "PID", FONT_REGULAR, 12, T.text2);
    y += 22;
    /* processos reais: thread 0 (desktop, rede, USB, áudio), programas de usuário (ring 3) e apps da interface */
    typedef struct { char n[64]; int pid; const char *kind; u64 cpu_ms, mem_kb; int state; } prow_t;
    static prow_t rows[40];
    static int sel_pid = -1;
    char v[48];
    int np = 0;
    rows[np++] = (prow_t){"kernel_task", 0, "Kernel", (timer_ticks() - g_idle_ticks), heap_used() / 1024, 1};
    for (int i = 0; i < proc_count() && np < 32; i++) {
        prow_t *r = &rows[np];
        if (proc_info(i, &r->pid, r->n, sizeof r->n, &r->state, &r->mem_kb, &r->cpu_ms)) { r->kind = "Usuário (ring 3)"; np++; }
    }
    for (int i = 0; i < apps_count() && np < 40; i++) {
        app_t *a = apps_get(i);
        if (!a->running) continue;
        prow_t *r = &rows[np++];
        strncpy(r->n, a->name, sizeof r->n - 1);
        r->n[sizeof r->n - 1] = 0;
        r->pid = 0; r->kind = "App"; r->cpu_ms = 0; r->mem_kb = (u64)wm_count_for(a) * 2048; r->state = 1;
    }
    net_info_t *ni = net_info();
    static const char *stn[] = {"pronto", "rodando", "dormindo", "esperando"};
    for (int i = 0; i < np; i++) {
        prow_t *r = &rows[i];
        rect_t rr = rect_make(0, y, u->cw, 22);
        bool user = r->kind[0] == 'U';
        bool sel = user && r->pid == sel_pid;
        if (sel) gfx_fill(s, 0, y, u->cw, 22, T.accent);
        else if (i % 2) gfx_fill(s, 0, y, u->cw, 22, T.alt_row);
        u32 tc = sel ? RGB(255, 255, 255) : T.text;
        char name[96];
        if (user) snprintf(name, sizeof name, "%s  (%s)", r->n, stn[r->state & 3]); else snprintf(name, sizeof name, "%s", r->n);
        ui_label(u, 12, y + 4, name, FONT_REGULAR, 13, tc);
        if (act_tab == 0) {
            if (r->kind[0] == 'A') strcpy(v, "—");
            else snprintf(v, sizeof v, "%llu,%llu s", r->cpu_ms / 1000, (r->cpu_ms % 1000) / 100);
        } else if (act_tab == 1) snprintf(v, sizeof v, "%.1f MB", r->mem_kb / 1024.0);
        else snprintf(v, sizeof v, "%llu", i == 0 ? ni->rx_packets + ni->tx_packets : 0ULL);
        ui_label(u, u->cw - 300, y + 4, v, FONT_REGULAR, 13, tc);
        ui_label(u, u->cw - 170, y + 4, r->kind, FONT_REGULAR, 13, sel ? tc : T.text2);
        snprintf(v, sizeof v, "%d", r->pid);
        ui_label(u, u->cw - 70, y + 4, r->kind[0] == 'A' ? "—" : v, FONT_REGULAR, 13, tc);
        if (user && ui_clicked(u, 1500 + i, rr)) sel_pid = r->pid;
        y += 22;
    }
    bool sel_alive = false;
    for (int i = 0; i < np; i++) if (rows[i].kind[0] == 'U' && rows[i].pid == sel_pid) sel_alive = true;
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 8 - 96, TOOLBAR_Y(28), 96, 28), "Encerrar", sel_alive)) {
        if (proc_kill(sel_pid)) { char m[64]; snprintf(m, sizeof m, "Processo %d encerrado", sel_pid); notify("Monitor de Atividade", m); }
        sel_pid = -1;
    }
    u->repaint = true;    /* rodapé com gráfico */
    int fy = u->ch - 120;
    gfx_fill(s, 0, fy, u->cw, 120, T.window_bg);
    gfx_fill(s, 0, fy, u->cw, 1, T.separator);
    char l[96];
    if (act_tab == 0) {
        snprintf(l, sizeof l, "Sistema: %.1f%%", cpu_hist[119] * 100);
        ui_label(u, 20, fy + 20, l, FONT_REGULAR, 12, T.text);
        snprintf(l, sizeof l, "Ocioso: %.1f%%", (1 - cpu_hist[119]) * 100);
        ui_label(u, 20, fy + 40, l, FONT_REGULAR, 12, T.text);
        extern int smp_cpu_count(void);
        snprintf(l, sizeof l, "Núcleos ativos: %d   Processos: %d", smp_cpu_count(), proc_count() + 1);
        ui_label(u, 20, fy + 60, l, FONT_REGULAR, 12, T.text);
        graph(s, rect_make(u->cw - 270, fy + 14, 240, 90), cpu_hist, RGB(255, 80, 80));
    } else if (act_tab == 1) {
        snprintf(l, sizeof l, "Memória física: %llu MB", pmm_total_pages() / 256);
        ui_label(u, 20, fy + 20, l, FONT_REGULAR, 12, T.text);
        snprintf(l, sizeof l, "Em uso: %llu MB", (pmm_total_pages() - pmm_free_pages()) / 256);
        ui_label(u, 20, fy + 40, l, FONT_REGULAR, 12, T.text);
        graph(s, rect_make(u->cw - 270, fy + 14, 240, 90), mem_hist, RGB(90, 200, 120));
    } else {
        snprintf(l, sizeof l, "Pacotes recebidos: %llu (%llu KB)", ni->rx_packets, ni->rx_bytes / 1024);
        ui_label(u, 20, fy + 20, l, FONT_REGULAR, 12, T.text);
        snprintf(l, sizeof l, "Pacotes enviados: %llu (%llu KB)", ni->tx_packets, ni->tx_bytes / 1024);
        ui_label(u, 20, fy + 40, l, FONT_REGULAR, 12, T.text);
    }
}

static void act_tick(app_t *a) { act_sample(); }
static void act_close(window_t *w) { act_w = NULL; }
static void act_launch(app_t *a) {
    if (act_w) { wm_focus(act_w); return; }
    act_w = wm_create(&app_atividade, "Monitor de Atividade", -1, -1, 640, 460, WIN_RESIZABLE);
    if (!act_w) return;
    act_w->paint = paint_act;
    act_w->on_close = act_close;
    act_w->toolbar_h = TOOLBAR_H;
    act_w->title_x = -1; /* o nome já aparece na barra de menus; a barra de ferramentas tem o seletor */
}
app_t app_atividade = {.name = "Monitor de Atividade", .exec = "atividade", .draw_icon = icon_atividade, .launch = act_launch, .tick = act_tick};

/* ================================================================== Console */
static window_t *con_w;
static int con_scroll;
extern char klog_buf[];
extern usize klog_len;

static void paint_con(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    gfx_fill(s, 0, 0, u->cw, u->ch, T.content_bg);
    int lh = 16, cols = (u->cw - 20) / mono_cell_w(12);
    int nl = 0;
    for (usize i = 0; i < klog_len; i++) if (klog_buf[i] == '\n') nl++;
    int content = nl * lh + 20;
    int rows = u->ch / lh;
    static int last_nl;
    if (nl != last_nl) { con_scroll = MAX(0, content - u->ch); last_nl = nl; }
    if (u->wheel) con_scroll = CLAMP(con_scroll + u->wheel * 48, 0, MAX(0, content - u->ch));
    int first = con_scroll / lh, line = 0;
    char buf[256];
    const char *p = klog_buf;
    while (*p && line < first + rows + 1) {
        const char *e = strchr(p, '\n');
        int n = e ? (int)(e - p) : (int)strlen(p);
        if (line >= first) {
            int k = MIN(n, 255);
            memcpy(buf, p, k);
            buf[k] = 0;
            u32 c = strstr(buf, "EXCE") || strstr(buf, "falha") || strstr(buf, "erro") ? RGB(230, 80, 70) : T.text;
            mono_draw(s, 12, 10, 14 + (line - first) * lh - con_scroll % lh, c, buf, cols);
        }
        line++;
        if (!e) break;
        p = e + 1;
    }
    ui_scroll(u, UI_ID, rect_make(0, 0, u->cw, u->ch), content, &con_scroll);
}
static void con_tick(app_t *a) {
    static usize last;
    if (con_w && klog_len != last) { last = klog_len; wm_invalidate(con_w); }
}
static void con_close(window_t *w) { con_w = NULL; }
static void con_launch(app_t *a) {
    if (con_w) { wm_focus(con_w); return; }
    con_w = wm_create(&app_console, "Console — kernel.log", -1, -1, 760, 440, WIN_RESIZABLE);
    if (!con_w) return;
    con_w->paint = paint_con;
    con_w->on_close = con_close;
}
app_t app_console = {.name = "Console", .exec = "console", .draw_icon = icon_console, .launch = con_launch, .tick = con_tick};

/* ================================================================== Campo Minado */
#define MW 16
#define MH 12
#define MINES 30
static struct { u8 mine[MH][MW], open[MH][MW], flag[MH][MW], n[MH][MW]; bool over, won, started; u64 t0, t1; window_t *w; } MSW;

static void mines_reset(void) { memset(&MSW.mine, 0, sizeof MSW.mine * 4); MSW.over = MSW.won = MSW.started = false; }
static void mines_place(int sx, int sy) {
    int placed = 0;
    while (placed < MINES) {
        int x = rand32() % MW, y = rand32() % MH;
        if (MSW.mine[y][x] || (ABS_DIFF(x, sx) <= 1 && ABS_DIFF(y, sy) <= 1)) continue;
        MSW.mine[y][x] = 1;
        placed++;
    }
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) {
        int c = 0;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            int nx = x + dx, ny = y + dy;
            if (nx >= 0 && ny >= 0 && nx < MW && ny < MH) c += MSW.mine[ny][nx];
        }
        MSW.n[y][x] = (u8)c;
    }
    MSW.started = true;
    MSW.t0 = timer_ticks();
}
static void mines_open(int x, int y) {
    if (x < 0 || y < 0 || x >= MW || y >= MH || MSW.open[y][x] || MSW.flag[y][x]) return;
    MSW.open[y][x] = 1;
    if (MSW.mine[y][x]) { MSW.over = true; MSW.t1 = timer_ticks(); sound_alert(); return; }
    if (!MSW.n[y][x]) for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) mines_open(x + dx, y + dy);
}
static void paint_mines(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    int cell = 28, ox = (u->cw - MW * cell) / 2, oy = 48;
    gfx_fill(s, 0, 0, u->cw, u->ch, T.window_bg);
    int flags = 0, opened = 0;
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) { flags += MSW.flag[y][x]; opened += MSW.open[y][x]; }
    if (!MSW.over && MSW.started && opened == MW * MH - MINES) { MSW.won = true; MSW.over = true; MSW.t1 = timer_ticks(); notify("Campo Minado", "Você venceu!"); }
    char l[64];
    u64 secs = MSW.started ? ((MSW.over ? MSW.t1 : u->now) - MSW.t0) / 1000 : 0;
    snprintf(l, sizeof l, "Minas: %d    Tempo: %llu s", MINES - flags, secs);
    ui_label(u, ox, 16, l, FONT_MEDIUM, 14, T.text);
    if (ui_button(u, UI_ID, rect_make(ox + MW * cell - 110, 12, 110, 24), MSW.over ? (MSW.won ? "Venceu! De novo" : "Tentar de novo") : "Novo Jogo", MSW.over)) mines_reset();
    static const u32 numc[9] = {0, RGB(40, 100, 230), RGB(40, 150, 60), RGB(220, 50, 50), RGB(30, 40, 150), RGB(140, 40, 40), RGB(20, 140, 140), RGB(30, 30, 30), RGB(120, 120, 120)};
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) {
        rect_t r = rect_make(ox + x * cell, oy + y * cell, cell - 2, cell - 2);
        if (MSW.open[y][x]) {
            gfx_round_rect(s, r.x, r.y, r.w, r.h, 3, T.dark ? RGB(70, 70, 72) : RGB(230, 226, 220));
            if (MSW.mine[y][x]) { gfx_round_rect(s, r.x, r.y, r.w, r.h, 3, RGB(230, 70, 60)); gfx_circle(s, r.x + r.w / 2.0f, r.y + r.h / 2.0f, 7, RGB(20, 20, 20)); }
            else if (MSW.n[y][x]) { char d[2] = {(char)('0' + MSW.n[y][x]), 0}; text_draw_center(s, font_get(FONT_BOLD), 15, r, numc[MSW.n[y][x]], d); }
        } else {
            bool hov = ui_hit(u, r) && !MSW.over;
            gfx_round_rect_grad(s, r.x, r.y, r.w, r.h, 3, hov ? RGB(150, 215, 120) : RGB(130, 200, 105), hov ? RGB(110, 180, 90) : RGB(95, 170, 80));
            if (MSW.flag[y][x]) {
                gfx_line(s, r.x + 10, r.y + 6, r.x + 10, r.y + 21, 1.6f, RGB(30, 30, 30));
                float fl[] = {r.x + 10.5f, r.y + 6.0f, r.x + 20.0f, r.y + 10.0f, r.x + 10.5f, r.y + 14.0f};
                gfx_polygon(s, fl, 3, RGB(230, 50, 40));
            }
            if (MSW.over && MSW.mine[y][x]) gfx_circle(s, r.x + r.w / 2.0f, r.y + r.h / 2.0f, 6, RGB(30, 30, 30));
            if (!MSW.over && ui_hit(u, r)) {
                if (u->rpressed) MSW.flag[y][x] ^= 1;
                else if (u->released) { if (!MSW.started) mines_place(x, y); mines_open(x, y); }
            }
        }
    }
    if (MSW.started && !MSW.over) u->repaint = true;
}
static void mines_close(window_t *w) { MSW.w = NULL; }
static void mines_launch(app_t *a) {
    if (MSW.w) { wm_focus(MSW.w); return; }
    mines_reset();
    MSW.w = wm_create(&app_minas, "Campo Minado", -1, -1, MW * 28 + 40, MH * 28 + 70, WIN_NO_ZOOM);
    if (!MSW.w) return;
    MSW.w->paint = paint_mines;
    MSW.w->on_close = mines_close;
}
app_t app_minas = {.name = "Campo Minado", .exec = "minas", .draw_icon = icon_minas, .launch = mines_launch, .pinned = true};

/* ================================================================== Visualizador (BMP/PPM) */
static surface_t *decode_bmp(const u8 *d, usize n) {
    if (n < 54 || d[0] != 'B' || d[1] != 'M') return NULL;
    u32 off = *(u32 *)(d + 10);
    i32 w = *(i32 *)(d + 18), h = *(i32 *)(d + 22);
    u16 bpp = *(u16 *)(d + 28);
    if (w <= 0 || w > 8192 || h == 0 || (bpp != 24 && bpp != 32)) return NULL;
    bool flip = h > 0;
    if (h < 0) h = -h;
    if (h > 8192) return NULL;
    surface_t *s = surface_new(w, h);
    int rs = ((w * bpp / 8) + 3) & ~3;
    for (int y = 0; y < h; y++) {
        const u8 *row = d + off + (usize)(flip ? h - 1 - y : y) * rs;
        if (row + w * bpp / 8 > d + n) break;
        for (int x = 0; x < w; x++) {
            const u8 *p = row + x * (bpp / 8);
            s->px[y * w + x] = RGB(p[2], p[1], p[0]);
        }
    }
    return s;
}
static surface_t *decode_ppm(const u8 *d, usize n) {
    if (n < 8 || d[0] != 'P' || d[1] != '6') return NULL;
    const char *p = (const char *)d + 2;
    int vals[3], k = 0;
    while (k < 3 && p < (const char *)d + n) {
        while (isspace(*p)) p++;
        if (*p == '#') { while (*p && *p != '\n') p++; continue; }
        vals[k++] = (int)strtol(p, (char **)&p, 10);
    }
    p++;
    int w = vals[0], h = vals[1];
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || (usize)((const u8 *)p - d) + (usize)w * h * 3 > n) return NULL;
    surface_t *s = surface_new(w, h);
    const u8 *q = (const u8 *)p;
    for (int i = 0; i < w * h; i++, q += 3) s->px[i] = RGB(q[0], q[1], q[2]);
    return s;
}
/* PNG/JPEG/BMP/GIF ficam em kernel/web/image.c; BMP e PPM daqui servem de reserva */
surface_t *image_decode(const u8 *d, usize n);   /* kernel/web/image.c */
static surface_t *viewer_decode(const u8 *d, usize n) {
    surface_t *s = image_decode(d, n);
    if (!s) s = decode_bmp(d, n);
    return s ? s : decode_ppm(d, n);
}

typedef struct { surface_t *img; char name[64]; } view_t;
static void paint_view(window_t *w, ui_t *u) {
    view_t *v = w->data;
    gfx_fill(u->s, 0, 0, u->cw, u->ch, T.dark ? RGB(30, 30, 30) : RGB(236, 236, 236));
    if (!v->img) { ui_label_center(u, rect_make(0, 0, u->cw, u->ch), "Formato de imagem não suportado", FONT_REGULAR, 15, T.text2); return; }
    float sc = MIN((float)u->cw / v->img->w, (float)u->ch / v->img->h);
    if (sc > 1) sc = 1;
    int dw = (int)(v->img->w * sc), dh = (int)(v->img->h * sc);
    gfx_blit_scaled(u->s, v->img, (u->cw - dw) / 2, (u->ch - dh) / 2, dw, dh, 255);
}
static void view_close(window_t *w) { view_t *v = w->data; surface_free(v->img); kfree(v); }
void viewer_open_file(const char *path) {
    vnode_t *n = vfs_lookup(path);
    if (!n) return;
    view_t *v = kcalloc(1, sizeof(view_t));
    v->img = viewer_decode(n->data, n->size);
    strncpy(v->name, n->name, 63);
    int w = v->img ? MIN(v->img->w, 1000) : 400, h = v->img ? MIN(v->img->h, 640) : 300;
    window_t *win = wm_create(&app_visualizador, n->name, -1, -1, MAX(w, 300), MAX(h, 200), WIN_RESIZABLE);
    if (!win) return;
    app_visualizador.running = true;
    win->data = v;
    win->paint = paint_view;
    win->on_close = view_close;
}
static void view_launch(app_t *a) { finder_open_path(home_sub("/Imagens")); }
app_t app_visualizador = {.name = "Visualizador", .exec = "visualizador", .draw_icon = icon_visualizador, .launch = view_launch};

/* ================================================================== Música: sintetizador e piano */
static window_t *mus_w;
static int mus_song = -1;
static u64 mus_next;
static int mus_pos;
static int mus_wave;

typedef struct { const char *name; const char *notes; int bpm; } song_t;
/* notação: nota+oitava+duração(1=semínima,2=mínima,h=colcheia); "R" = pausa. Melodias de domínio público. */
static const song_t songs[] = {
    {"Ode à Alegria — Beethoven", "E4 E4 F4 G4 G4 F4 E4 D4 C4 C4 D4 E4 E4. D4h D42 E4 E4 F4 G4 G4 F4 E4 D4 C4 C4 D4 E4 D4. C4h C42", 120},
    {"Brilha, Brilha Estrelinha", "C4 C4 G4 G4 A4 A4 G42 F4 F4 E4 E4 D4 D4 C42 G4 G4 F4 F4 E4 E4 D42 G4 G4 F4 F4 E4 E4 D42", 110},
    {"Für Elise (início) — Beethoven", "E5h D#5h E5h D#5h E5h B4h D5h C5h A42 C4h E4h A4h B42 E4h G#4h B4h C52 E4h E5h D#5h E5h D#5h E5h B4h D5h C5h A42", 130},
    {"Greensleeves (trad.)", "A4 C52 D5 E5. F5h E5 D52 B4 G4. A4h B4 C52 A4 A4. G#4h A4 B42 G#4 E42", 100},
};

static int note_freq(const char *n, int *len) {
    static const int semis[7] = {9, 11, 0, 2, 4, 5, 7}; /* A B C D E F G */
    const char *p = n;
    if (*p == 'R') { *len = 1; return 0; }
    int s = semis[*p - 'A'];
    p++;
    if (*p == '#') { s++; p++; }
    int oct = *p++ - '0';
    int midi = 12 * (oct + 1) + s;
    float f = 440.0f * fpow(2.0f, (midi - 69) / 12.0f);
    *len = 4;
    if (*p == '2') *len = 8;
    else if (*p == 'h') *len = 2;
    else if (*p == '.') *len = 6;
    return (int)f;
}

void synth_note(float freq, int ms, int wave, float vol);

static void mus_tick(app_t *a) {
    if (mus_song < 0) return;
    u64 now = timer_ticks();
    if (now < mus_next) return;
    const char *p = songs[mus_song].notes;
    int idx = 0;
    while (*p && idx < mus_pos) { while (*p && *p != ' ') p++; while (*p == ' ') p++; idx++; }
    if (!*p) { mus_song = -1; if (mus_w) wm_invalidate(mus_w); return; }
    int len;
    int f = note_freq(p, &len);
    int ms = 60000 / songs[mus_song].bpm * len / 4;
    if (f) synth_note((float)f, ms - 20, mus_wave, 0.5f);
    mus_next = now + ms;
    mus_pos++;
    if (mus_w) wm_invalidate(mus_w);
}

static void paint_mus(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    gfx_fill(s, 0, 0, u->cw, u->ch, T.content_bg);
    ui_label(u, 20, 14, "Biblioteca", FONT_BOLD, 18, T.text);
    for (int i = 0; i < (int)ARRAY_LEN(songs); i++) {
        rect_t r = rect_make(0, 46 + i * 30, u->cw, 30);
        bool sel = i == mus_song;
        if (ui_list_row(u, 900 + i, r, sel, i % 2)) { mus_song = i; mus_pos = 0; mus_next = 0; }
        ui_label(u, 20, r.y + 7, songs[i].name, sel ? FONT_SEMIBOLD : FONT_REGULAR, 13, sel ? RGB(255, 255, 255) : T.text);
    }
    int y = 46 + ARRAY_LEN(songs) * 30 + 14;
    static const char *waves[] = {"Piano", "Órgão", "Sintetizador"};
    ui_label(u, 20, y + 3, "Timbre:", FONT_REGULAR, 13, T.text);
    mus_wave = ui_segmented(u, UI_ID, rect_make(80, y, 270, 22), waves, 3, mus_wave);
    if (mus_song >= 0 && ui_button(u, UI_ID, rect_make(u->cw - 110, y, 90, 24), "Parar", false)) mus_song = -1;
    /* teclado de piano: 2 oitavas */
    int ky = y + 40, kh = u->ch - ky - 12, nw = 14, kw = (u->cw - 40) / nw;
    static const int white_semi[7] = {0, 2, 4, 5, 7, 9, 11};
    for (int i = 0; i < nw; i++) {
        rect_t r = rect_make(20 + i * kw, ky, kw - 1, kh);
        bool held = ui_hit(u, r) && u->down;
        gfx_round_rect_ex(s, r.x, r.y, r.w, r.h, 0, 0, 4, 4, held ? RGB(210, 210, 214) : RGB(252, 252, 252), held ? RGB(190, 190, 196) : RGB(236, 236, 236));
        gfx_round_rect_stroke(s, r.x, r.y, r.w, r.h, 4, 1, RGBA(0, 0, 0, 60));
        if (ui_hit(u, r) && u->pressed) {
            int midi = 60 + (i / 7) * 12 + white_semi[i % 7];
            synth_note(440.0f * fpow(2.0f, (midi - 69) / 12.0f), 600, mus_wave, 0.6f);
        }
    }
    static const int black_after[5] = {0, 1, 3, 4, 5};
    static const int black_semi[5] = {1, 3, 6, 8, 10};
    for (int o = 0; o < 2; o++)
        for (int b = 0; b < 5; b++) {
            int i = o * 7 + black_after[b];
            rect_t r = rect_make(20 + (i + 1) * kw - kw / 3, ky, kw * 2 / 3, kh * 6 / 10);
            gfx_round_rect_ex(s, r.x, r.y, r.w, r.h, 0, 0, 3, 3, RGB(50, 50, 54), RGB(20, 20, 22));
            if (ui_hit(u, r) && u->pressed) {
                int midi = 60 + o * 12 + black_semi[b];
                synth_note(440.0f * fpow(2.0f, (midi - 69) / 12.0f), 600, mus_wave, 0.6f);
            }
        }
    if (!audio_available()) ui_label(u, 20, u->ch - 28, "Sem saída de áudio disponível.", FONT_REGULAR, 11, T.text2);
}
static void mus_close(window_t *w) { mus_w = NULL; mus_song = -1; }
static void mus_launch(app_t *a) {
    if (mus_w) { wm_focus(mus_w); return; }
    mus_w = wm_create(&app_musica, "Música", -1, -1, 620, 440, 0);
    if (!mus_w) return;
    mus_w->paint = paint_mus;
    mus_w->on_close = mus_close;
}
app_t app_musica = {.name = "Música", .exec = "musica", .draw_icon = icon_musica, .launch = mus_launch, .tick = mus_tick, .pinned = true};
