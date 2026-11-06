/* mOS — "Sobre este mOS" */
#include "apps.h"
#include "net.h"
#include "gpu.h"

static window_t *aw;
static int tab;

static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    static const char *tabs[] = {"Visão Geral", "Monitores", "Armazenamento", "Memória"};
    tab = ui_segmented(u, UI_ID, rect_make(u->cw / 2 - 210, 8, 420, 22), tabs, 4, tab);
    if (tab == 0) {
        /* logo à esquerda */
        gfx_circle(s, 130, 150, 84, RGBA(0, 0, 0, 40));
        gfx_round_rect_grad(s, 46, 64, 168, 168, 84, color_lighten(T.accent, 50), T.accent);
        glyph_mos_logo(s, 130, 148, 60, RGB(255, 255, 255), WITH_ALPHA(T.accent, 0));
        int x = 260, y = 62;
        ui_label(u, x, y, "mOS", FONT_BOLD, 30, T.text);
        ui_label(u, x + text_width(font_get(FONT_BOLD), 30, "mOS") + 8, y, "Dunas", FONT_REGULAR, 30, T.text);
        y += 42;
        ui_label(u, x, y, "Versão 0.1 (compilação Nucleo-1)", FONT_REGULAR, 12, T.text2);
        y += 30;
        char vendor[16], brand[64], l[160];
        cpu_info(vendor, brand);
        struct { const char *k; char v[128]; } rows[5];
        rows[0].k = "Computador";
        snprintf(rows[0].v, 128, "PC x86_64 com firmware UEFI");
        rows[1].k = "Processador";
        snprintf(rows[1].v, 128, "%s", brand);
        rows[2].k = "Memória";
        snprintf(rows[2].v, 128, "%llu MB", pmm_total_pages() * 4096 / 1000000);
        rows[3].k = "Gráficos";
        if (gpu_count()) snprintf(rows[3].v, 128, "%s, %ux%u", gpu_summary(), g_boot->fb.width, g_boot->fb.height);
        else snprintf(rows[3].v, 128, "Framebuffer UEFI GOP %ux%u", g_boot->fb.width, g_boot->fb.height);
        rows[4].k = "Rede";
        snprintf(rows[4].v, 128, "%s", net_device_name());
        for (int i = 0; i < 5; i++) {
            snprintf(l, sizeof l, "%s", rows[i].k);
            ui_label(u, x, y, l, FONT_SEMIBOLD, 12.5f, T.text);
            text_draw_ellipsis(s, font_get(FONT_REGULAR), 12.5f, x + 96, y + text_ascent(font_get(FONT_REGULAR), 12.5f), u->cw - x - 110, T.text, rows[i].v);
            y += 21;
        }
        y += 16;
        if (ui_button(u, UI_ID, rect_make(x, y, 150, 24), "Relatório do Sistema…", false)) app_launch(&app_atividade);
        if (ui_button(u, UI_ID, rect_make(x + 160, y, 150, 24), "Atualização…", false)) prefs_open_pane(9);
        ui_label_center(u, rect_make(0, u->ch - 26, u->cw, 18), "Software livre sob a licença MIT. Fontes Inter sob SIL OFL 1.1.", FONT_REGULAR, 11, T.text3);
    } else if (tab == 1) {
        /* um desenho por monitor, lado a lado, na proporção de cada um */
        int n = display_count(), slot = (u->cw - 40) / n;
        for (int i = 0; i < n; i++) {
            rect_t d = display_rect(i);
            float bw = MIN(220.0f, slot - 24.0f), bh = bw * d.h / d.w;
            float mx = 20 + slot * i + (slot - bw) / 2, my = 60 + (140 - bh) / 2;
            gfx_round_rect(s, mx, my, bw, bh, 8, RGB(26, 26, 28));
            gfx_round_rect_grad(s, mx + 8, my + 8, bw - 16, bh - 16, 3, RGB(70, 110, 180), RGB(232, 150, 100));
            char r[96];
            snprintf(r, sizeof r, "%s (%dx%d)", i == 0 && n == 1 ? "Tela integrada" : display_name(i), d.w, d.h);
            ui_label_center(u, rect_make(20 + slot * i, 220, slot, 22), r, FONT_SEMIBOLD, n > 1 ? 11 : FONT_UI, T.text);
        }
        if (gpu_display_notice()[0])                  /* monitor plugado que o mOS ainda não liga, ou espelho */
            text_wrap(s, font_get(FONT_REGULAR), 11, rect_make(40, 250, u->cw - 80, 60), 15, T.text2, gpu_display_notice(), true);
    } else if (tab == 2) {
        icon_disk(s, 60, 60, 96);
        ui_label(u, 190, 70, "mOS HD", FONT_SEMIBOLD, 15, T.text);
        char l[96];
        snprintf(l, sizeof l, "%llu KB usados por arquivos em memória", (u64)vfs_total_bytes() / 1024);
        ui_label(u, 190, 94, l, FONT_REGULAR, 12, T.text2);
        rect_t bar = rect_make(190, 124, u->cw - 230, 22);
        gfx_round_rect(s, bar.x, bar.y, bar.w, bar.h, 5, T.dark ? RGB(70, 70, 72) : RGB(226, 226, 228));
        float frac = (float)vfs_total_bytes() / (128.0f * 1024 * 1024);
        gfx_round_rect(s, bar.x, bar.y, MAX(10, bar.w * frac), bar.h, 5, RGB(250, 180, 50));
        ui_label(u, 190, 156, "Pendrive de boot (FAT32). Alterações ficam na RAM nesta versão.", FONT_REGULAR, 11.5f, T.text2);
    } else {
        u64 tot = pmm_total_pages(), fr = pmm_free_pages();
        char l[96];
        snprintf(l, sizeof l, "%llu MB de memória instalada", tot * 4096 / 1000000);
        ui_label(u, 60, 70, l, FONT_SEMIBOLD, 15, T.text);
        rect_t bar = rect_make(60, 110, u->cw - 120, 26);
        gfx_round_rect(s, bar.x, bar.y, bar.w, bar.h, 5, T.dark ? RGB(70, 70, 72) : RGB(226, 226, 228));
        gfx_round_rect(s, bar.x, bar.y, MAX(10, (int)(bar.w * (float)(tot - fr) / tot)), bar.h, 5, T.accent);
        snprintf(l, sizeof l, "Em uso: %llu MB    Livre: %llu MB    Heap do kernel: %llu KB", (tot - fr) * 4 / 1024, fr * 4 / 1024, (u64)heap_used() / 1024);
        ui_label(u, 60, 150, l, FONT_REGULAR, 12, T.text2);
    }
}

static void on_close(window_t *w) { aw = NULL; }

void about_open(void) {
    if (aw) { wm_focus(aw); return; }
    aw = wm_create(&app_sobre, "", -1, -1, 620, 340, WIN_CENTER);
    if (!aw) return;
    aw->paint = paint;
    aw->on_close = on_close;
    tab = 0;
}

static void launch(app_t *a) { about_open(); }
app_t app_sobre = {.name = "Sobre este mOS", .exec = "sobre", .draw_icon = icon_prefs, .launch = launch};
