/* mOS — Monitor de Atividade, Console e Campo Minado */
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

static struct { char q[64]; bool errors_only, paused; usize base, frozen_len; } CO;
static const char *memchr_kit(const char *p, const char *end) { while (p < end && *p != '\n') p++; return p; }
static bool con_is_err(const char *l) {
    return strstr(l, "EXCE") || strstr(l, "falha") || strstr(l, "erro") || strstr(l, "FAIL") || strstr(l, "error") || strstr(l, "inválid");
}
static void paint_con(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    int top = w->toolbar_h;
    gfx_fill(s, 0, top, u->cw, u->ch - top, T.content_bg);
    /* barra de ferramentas: pausar, limpar, salvar, só erros e busca */
    int tx = 96;
    if (ui_toolbar_button(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 90, 28), CO.paused ? "Retomar" : "Pausar", true)) { CO.paused = !CO.paused; CO.frozen_len = klog_len; }
    if (ui_toolbar_button(u, UI_ID, rect_make(tx + 98, TOOLBAR_Y(28), 80, 28), "Limpar", true)) { CO.base = klog_len; con_scroll = 0; }
    if (ui_toolbar_button(u, UI_ID, rect_make(tx + 186, TOOLBAR_Y(28), 80, 28), "Salvar", true)) {
        char p[200];
        snprintf(p, sizeof p, "%s/kernel.log", home_sub("/Mesa"));
        vfs_write_file(p, klog_buf, klog_len);
        notify("Console", "Registro salvo na Mesa (kernel.log).");
    }
    static const char *flt[2] = {"Todas", "Erros"};
    CO.errors_only = ui_segmented(u, UI_ID, rect_make(tx + 274, TOOLBAR_Y(28), 140, 28), flt, 2, CO.errors_only);
    ui_searchfield(u, UI_ID, rect_make(u->cw - 12 - 200, TOOLBAR_Y(28), 200, 28), CO.q, sizeof CO.q);
    usize len = CO.paused ? CO.frozen_len : klog_len;
    if (CO.base > len) CO.base = 0;
    int lh = 16, cols = (u->cw - 20) / mono_cell_w(12);
    char q[64];
    snprintf(q, sizeof q, "%s", CO.q);
    for (char *c = q; *c; c++) *c = (char)tolower(*c);
    /* conta as linhas que passam no filtro */
    static int last_shown;
    rect_t view = rect_make(0, top, u->cw, u->ch - top);
    int shown = 0, first = con_scroll / lh, rows = view.h / lh + 2;
    rect_t pc = surface_push_clip(s, view);
    char buf[256], low[256];
    const char *p = klog_buf + CO.base, *end = klog_buf + len;
    while (p < end) {
        const char *e = memchr_kit(p, end);
        int n = (int)(e - p), k = MIN(n, 255);
        memcpy(buf, p, k);
        buf[k] = 0;
        bool ok = true;
        if (CO.errors_only && !con_is_err(buf)) ok = false;
        if (ok && q[0]) { for (int i = 0; i <= k; i++) low[i] = (char)tolower(buf[i]); if (!strstr(low, q)) ok = false; }
        if (ok) {
            if (shown >= first && shown < first + rows) {
                u32 c = con_is_err(buf) ? RGB(230, 80, 70) : strstr(buf, "aviso") ? RGB(220, 150, 30) : T.text;
                mono_draw(s, 12, 10, top + 14 + (shown - first) * lh - con_scroll % lh, c, buf, cols);
            }
            shown++;
        }
        p = e < end ? e + 1 : end;
    }
    surface_pop_clip(s, pc);
    int content = shown * lh + 20;
    if (shown != last_shown && !CO.paused && !q[0]) { con_scroll = MAX(0, content - view.h); }
    last_shown = shown;
    ui_scroll(u, UI_ID, view, content, &con_scroll);
    char st[64];
    snprintf(st, sizeof st, "%d mensage%s", shown, shown == 1 ? "m" : "ns");
    int sw = text_width(font_get(FONT_REGULAR), 11, st);
    text_draw(s, font_get(FONT_REGULAR), 11, u->cw - 22 - sw, u->ch - 8, T.text3, st);
}
static void con_tick(app_t *a) {
    static usize last;
    if (con_w && klog_len != last && !CO.paused) { last = klog_len; wm_invalidate(con_w); }
}
static void con_close(window_t *w) { con_w = NULL; }
static void con_launch(app_t *a) {
    if (con_w) { wm_focus(con_w); return; }
    con_w = wm_create(&app_console, "Console", -1, -1, 900, 520, WIN_RESIZABLE);
    if (!con_w) return;
    con_w->paint = paint_con;
    con_w->on_close = con_close;
    con_w->toolbar_h = TOOLBAR_H;
    con_w->title_x = -1;
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


