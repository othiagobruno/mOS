/* mOS — tokens de cor do design system Cupertino (aparência clara e escura) */
#include "ui.h"

theme_t T;
int g_accent_index = 0;

const u32 g_accent_colors[8] = {
    RGB(0, 122, 255),   /* azul */
    RGB(149, 61, 150),  /* roxo */
    RGB(247, 79, 158),  /* rosa */
    RGB(224, 56, 62),   /* vermelho */
    RGB(247, 130, 27),  /* laranja */
    RGB(255, 198, 0),   /* amarelo */
    RGB(98, 186, 70),   /* verde */
    RGB(140, 140, 145), /* grafite */
};
const char *g_accent_names[8] = {"Azul", "Roxo", "Rosa", "Vermelho", "Laranja", "Amarelo", "Verde", "Grafite"};

void theme_set(bool dark, int accent) {
    g_accent_index = CLAMP(accent, 0, 7);
    u32 a = g_accent_colors[g_accent_index];
    T.dark = dark;
    T.accent = a;
    T.accent_hi = color_lighten(a, 30);
    T.focus_ring = WITH_ALPHA(a, 130);
    T.text_inv = RGB(255, 255, 255);
    if (!dark) {
        T.selection = WITH_ALPHA(color_mix(a, RGB(255, 255, 255), 150), 255);
        T.text = RGB(30, 30, 32);
        T.text2 = RGB(128, 128, 132);
        T.text3 = RGB(178, 178, 182);
        T.window_bg = RGB(236, 236, 236);
        T.content_bg = RGB(255, 255, 255);
        T.alt_row = RGB(244, 245, 245);
        T.separator = RGB(222, 222, 222);
        T.border = RGB(196, 196, 196);
        T.title_top = RGB(234, 232, 234);
        T.title_bot = RGB(212, 210, 212);
        T.title_text = RGB(77, 77, 77);
        T.title_inactive = RGB(246, 246, 246);
        T.control_top = RGB(255, 255, 255);
        T.control_bot = RGB(246, 246, 246);
        T.control_border = RGB(196, 196, 196);
        T.control_text = RGB(38, 38, 38);
        T.field_bg = RGB(255, 255, 255);
        T.field_border = RGB(190, 190, 190);
        T.sidebar_bg = RGBA(226, 226, 230, 235);
        T.sidebar_text = RGB(38, 38, 38);
        T.sidebar_head = RGB(140, 140, 144);
        T.menu_bg = RGBA(240, 240, 240, 238);
        T.menu_text = RGB(20, 20, 20);
        T.menu_sep = RGB(214, 214, 214);
        T.menubar_tint = RGBA(244, 244, 246, 196);
        T.dock_tint = RGBA(255, 255, 255, 96);
        T.dock_border = RGBA(255, 255, 255, 140);
        T.scroll_thumb = RGBA(0, 0, 0, 100);
        T.tooltip_bg = RGBA(252, 252, 252, 245);
    } else {
        T.selection = color_mix(a, RGB(30, 30, 30), 120);
        T.text = RGB(223, 223, 223);
        T.text2 = RGB(152, 152, 157);
        T.text3 = RGB(98, 98, 102);
        T.window_bg = RGB(50, 50, 50);
        T.content_bg = RGB(30, 30, 30);
        T.alt_row = RGB(38, 38, 38);
        T.separator = RGB(62, 62, 62);
        T.border = RGB(26, 26, 26);
        T.title_top = RGB(60, 59, 60);
        T.title_bot = RGB(46, 45, 46);
        T.title_text = RGB(200, 200, 200);
        T.title_inactive = RGB(44, 44, 44);
        T.control_top = RGB(104, 104, 104);
        T.control_bot = RGB(92, 92, 92);
        T.control_border = RGB(60, 60, 60);
        T.control_text = RGB(232, 232, 232);
        T.field_bg = RGB(40, 40, 40);
        T.field_border = RGB(82, 82, 82);
        T.sidebar_bg = RGBA(42, 42, 44, 230);
        T.sidebar_text = RGB(222, 222, 222);
        T.sidebar_head = RGB(120, 120, 124);
        T.menu_bg = RGBA(42, 42, 44, 240);
        T.menu_text = RGB(232, 232, 232);
        T.menu_sep = RGB(76, 76, 78);
        T.menubar_tint = RGBA(28, 28, 30, 184);
        T.dock_tint = RGBA(30, 30, 32, 120);
        T.dock_border = RGBA(255, 255, 255, 46);
        T.scroll_thumb = RGBA(255, 255, 255, 90);
        T.tooltip_bg = RGBA(58, 58, 60, 245);
    }
    /* vidro líquido */
    if (!dark) {
        T.glass_text = RGB(18, 18, 20);
        T.glass_text2 = RGBA(40, 40, 46, 170);
        T.glass_hairline = RGBA(0, 0, 0, 34);
        T.glass_fill = RGBA(255, 255, 255, 120);
        T.glass_fill_hi = RGBA(255, 255, 255, 175);
        T.glass_sel = WITH_ALPHA(a, 225);
        T.knob = RGB(255, 255, 255);
        /* janelas brancas, controles planos cinza-claro (padrão das referências) */
        T.text = RGB(28, 28, 30);
        T.text2 = RGB(134, 134, 139);
        T.text3 = RGB(186, 186, 190);
        T.window_bg = RGB(255, 255, 255);
        T.content_bg = RGB(255, 255, 255);
        T.title_top = T.title_bot = T.title_inactive = RGB(255, 255, 255);
        T.title_text = RGB(28, 28, 30);
        T.alt_row = RGB(244, 244, 246);
        T.separator = RGB(229, 229, 232);
        T.control_top = T.control_bot = RGB(228, 228, 232);
        T.control_text = RGB(28, 28, 30);
        T.field_bg = RGB(255, 255, 255);
        T.field_border = RGB(216, 216, 220);
        T.sidebar_bg = RGB(230, 230, 232);
        T.sidebar_text = RGB(28, 28, 30);
        T.sidebar_head = RGB(140, 140, 146);
        T.tool_bg = RGB(255, 255, 255);
        T.tool_border = RGBA(0, 0, 0, 22);
        T.sidebar_sel = RGBA(0, 0, 0, 16);
    } else {
        T.glass_text = RGB(245, 245, 247);
        T.glass_text2 = RGBA(235, 235, 245, 150);
        T.glass_hairline = RGBA(0, 0, 0, 90);
        T.glass_fill = RGBA(255, 255, 255, 34);
        T.glass_fill_hi = RGBA(255, 255, 255, 60);
        T.glass_sel = WITH_ALPHA(a, 215);
        T.knob = RGB(236, 236, 238);
        T.window_bg = RGB(30, 30, 32);
        T.title_top = T.title_bot = RGB(30, 30, 32);
        T.title_inactive = RGB(30, 30, 32);
        T.title_text = RGB(236, 236, 240);
        T.content_bg = RGB(30, 30, 32);
        T.alt_row = RGBA(255, 255, 255, 12);
        T.separator = RGBA(255, 255, 255, 22);
        T.control_top = T.control_bot = RGBA(255, 255, 255, 34);
        T.sidebar_bg = RGB(22, 22, 24);
        T.tool_bg = RGBA(255, 255, 255, 22);
        T.tool_border = RGBA(255, 255, 255, 26);
        T.sidebar_sel = RGBA(255, 255, 255, 18);
    }
    /*                    blur sat   tint                                   refr bezel rim sheen op */
    /* último campo = aberração cromática: forte no vidro claro (Dock, controles), fraca nos espessos,
     * nenhuma na barra lateral (quase opaca; a franja só sujaria o texto) */
    G[GLASS_REGULAR] = (glass_t){18, 330, dark ? RGBA(30, 30, 34, 150) : RGBA(250, 250, 252, 170), 6, 16, 150, 22, 0, 0.8f};
    G[GLASS_CLEAR]   = (glass_t){10, 360, dark ? RGBA(255, 255, 255, 26) : RGBA(255, 255, 255, 70), 9, 18, 190, 26, 0, 1.2f};
    G[GLASS_THICK]   = (glass_t){22, 300, dark ? RGBA(40, 40, 44, 205) : RGBA(250, 250, 252, 222), 5, 14, 170, 26, 0, 0.5f};
    G[GLASS_SIDEBAR] = (glass_t){20, 300, dark ? RGBA(38, 38, 40, 236) : RGBA(246, 246, 248, 236), 3, 10, 70, 6, 0, 0};
    G[GLASS_CONTROL] = (glass_t){8, 360, dark ? RGBA(60, 60, 66, 90) : RGBA(255, 255, 255, 110), 7, 14, 210, 30, 0, 1.0f};
    /* widgets da mesa: vidro fumê nos dois modos (texto sempre branco), pouco desfoque para o papel
     * de parede continuar legível através dele, borda bem acesa */
    G[GLASS_WIDGET]  = (glass_t){12, 320, dark ? RGBA(22, 22, 28, 120) : RGBA(30, 30, 38, 105), 7, 16, 200, 14, 0, 1.0f};
    /* opacidade global do vidro (controle do sistema): 50 = padrão; 0 = mais transparente; 100 = quase opaco
     * (equivale a "reduzir transparência"). Escala o alfa da tinta de todos os estilos de vidro. */
    for (int k = 0; k < GLASS_KINDS; k++) {
        int al = C_A(G[k].tint);
        if (g_glass_level >= 50) al += (250 - al) * (g_glass_level - 50) / 50;
        else al = al * (20 + g_glass_level * 80 / 50) / 100;
        G[k].tint = WITH_ALPHA(G[k].tint, CLAMP(al, 0, 255));
    }
}

glass_t G[GLASS_KINDS];
int g_glass_level = 50;

void glass_panel(surface_t *s, float x, float y, float w, float h, float r, int kind) {
    gfx_shadow(s, (int)x, (int)y, (int)w, (int)h, r, 28, T.dark ? 120 : 60, 10);
    gfx_glass(s, x, y, w, h, r, &G[kind]);
    gfx_round_rect_stroke(s, x - 0.5f, y - 0.5f, w + 1, h + 1, r + 0.5f, 1, T.glass_hairline);
}

/* "botão de vidro" desenhado sobre um fundo já desenhado (sem desfoque): preenchimento + brilho */
void glass_capsule_fill(surface_t *s, float x, float y, float w, float h, float r, u32 fill, int rim) {
    gfx_round_rect(s, x, y, w, h, r, fill);
    gfx_specular_rim(s, x, y, w, h, r, rim);
}
