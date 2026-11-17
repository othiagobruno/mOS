/* mOS — aparência (ver appearance_core.h). Paleta própria do mOS. Código original do mOS. */
#include "appearance_core.h"

static int eq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static int has(const char *s, const char *sub) {
    for (; *s; s++) { const char *p = s, *q = sub; while (*p && *q && *p == *q) { p++; q++; } if (!*q) return 1; }
    return 0;
}

typedef struct { const char *sel; double l[4], d[4]; int accent; } syscol_t;
/* accent: 0 fixa; 1 a cor de destaque; 2 destaque clareado (seleção de texto e de controle); 3 destaque com alfa 0,5 */
static const syscol_t k_cols[] = {
    {"labelColor", {0, 0, 0, 0.85}, {1, 1, 1, 0.85}, 0},
    {"textColor", {0, 0, 0, 1}, {1, 1, 1, 1}, 0},
    {"controlTextColor", {0, 0, 0, 0.85}, {1, 1, 1, 0.85}, 0},
    {"secondaryLabelColor", {0, 0, 0, 0.5}, {1, 1, 1, 0.55}, 0},
    {"tertiaryLabelColor", {0, 0, 0, 0.26}, {1, 1, 1, 0.25}, 0},
    {"quaternaryLabelColor", {0, 0, 0, 0.1}, {1, 1, 1, 0.1}, 0},
    {"placeholderTextColor", {0, 0, 0, 0.25}, {1, 1, 1, 0.25}, 0},
    {"disabledControlTextColor", {0, 0, 0, 0.25}, {1, 1, 1, 0.25}, 0},
    {"headerTextColor", {0, 0, 0, 0.85}, {1, 1, 1, 1}, 0},
    {"windowFrameTextColor", {0, 0, 0, 0.85}, {1, 1, 1, 0.85}, 0},
    {"windowBackgroundColor", {0.925, 0.925, 0.925, 1}, {0.196, 0.196, 0.196, 1}, 0},
    {"underPageBackgroundColor", {0.59, 0.59, 0.59, 1}, {0.157, 0.157, 0.157, 1}, 0},
    {"controlBackgroundColor", {1, 1, 1, 1}, {0.118, 0.118, 0.118, 1}, 0},
    {"textBackgroundColor", {1, 1, 1, 1}, {0.118, 0.118, 0.118, 1}, 0},
    {"controlColor", {1, 1, 1, 1}, {1, 1, 1, 0.25}, 0},
    {"separatorColor", {0, 0, 0, 0.1}, {1, 1, 1, 0.1}, 0},
    {"gridColor", {0.9, 0.9, 0.9, 1}, {0.1, 0.1, 0.1, 1}, 0},
    {"windowFrameColor", {0.667, 0.667, 0.667, 1}, {0.33, 0.33, 0.33, 1}, 0},
    {"selectedTextColor", {0, 0, 0, 1}, {1, 1, 1, 1}, 0},
    {"selectedTextBackgroundColor", {0, 0, 0, 1}, {0, 0, 0, 1}, 2},
    {"unemphasizedSelectedTextBackgroundColor", {0.863, 0.863, 0.863, 1}, {0.27, 0.27, 0.27, 1}, 0},
    {"unemphasizedSelectedTextColor", {0, 0, 0, 1}, {1, 1, 1, 1}, 0},
    {"unemphasizedSelectedContentBackgroundColor", {0.863, 0.863, 0.863, 1}, {0.27, 0.27, 0.27, 1}, 0},
    {"selectedContentBackgroundColor", {0, 0, 0, 1}, {0, 0, 0, 1}, 1},
    {"selectedControlColor", {0, 0, 0, 1}, {0, 0, 0, 1}, 2},
    {"selectedControlTextColor", {0, 0, 0, 0.85}, {1, 1, 1, 0.85}, 0},
    {"alternateSelectedControlTextColor", {1, 1, 1, 1}, {1, 1, 1, 1}, 0},
    {"selectedMenuItemTextColor", {1, 1, 1, 1}, {1, 1, 1, 1}, 0},
    {"keyboardFocusIndicatorColor", {0, 0, 0, 1}, {0, 0, 0, 1}, 3},
    {"findHighlightColor", {1, 1, 0, 1}, {1, 1, 0, 1}, 0},
    {"highlightColor", {1, 1, 1, 1}, {0.7, 0.7, 0.7, 1}, 0},
    {"shadowColor", {0, 0, 0, 1}, {0, 0, 0, 1}, 0},
    {"textInsertionPointColor", {0, 0, 0, 1}, {1, 1, 1, 1}, 0},
    {"controlHighlightColor", {1, 1, 1, 1}, {0.3, 0.3, 0.3, 1}, 0},
    {"controlLightHighlightColor", {1, 1, 1, 1}, {0.4, 0.4, 0.4, 1}, 0},
    {"controlShadowColor", {0.5, 0.5, 0.5, 1}, {0, 0, 0, 1}, 0},
    {"controlDarkShadowColor", {0, 0, 0, 1}, {0, 0, 0, 1}, 0},
    {"scrollBarColor", {0.667, 0.667, 0.667, 1}, {0.25, 0.25, 0.25, 1}, 0},
    {"knobColor", {0, 0, 0, 0.5}, {1, 1, 1, 0.5}, 0},
    {"selectedKnobColor", {0, 0, 0, 0.7}, {1, 1, 1, 0.7}, 0},
    {"linkColor", {0.03, 0.31, 0.82, 1}, {0.25, 0.61, 1, 1}, 0},
    {"controlAccentColor", {0, 0, 0, 1}, {0, 0, 0, 1}, 1},
    {"systemFillColor", {0, 0, 0, 0.1}, {1, 1, 1, 0.1}, 0},
    {"secondarySystemFillColor", {0, 0, 0, 0.08}, {1, 1, 1, 0.08}, 0},
    {"tertiarySystemFillColor", {0, 0, 0, 0.05}, {1, 1, 1, 0.05}, 0},
    {"quaternarySystemFillColor", {0, 0, 0, 0.03}, {1, 1, 1, 0.03}, 0},
    {"quinarySystemFillColor", {0, 0, 0, 0.02}, {1, 1, 1, 0.02}, 0},
    {"scrubberTexturedBackgroundColor", {0.5, 0.5, 0.5, 1}, {0.5, 0.5, 0.5, 1}, 0},
    {"systemRedColor", {1, 0.23, 0.19, 1}, {1, 0.27, 0.23, 1}, 0},
    {"systemGreenColor", {0.2, 0.78, 0.35, 1}, {0.19, 0.82, 0.35, 1}, 0},
    {"systemBlueColor", {0, 0.48, 1, 1}, {0.04, 0.52, 1, 1}, 0},
    {"systemOrangeColor", {1, 0.58, 0, 1}, {1, 0.62, 0.04, 1}, 0},
    {"systemYellowColor", {1, 0.8, 0, 1}, {1, 0.84, 0.04, 1}, 0},
    {"systemBrownColor", {0.64, 0.52, 0.37, 1}, {0.67, 0.56, 0.41, 1}, 0},
    {"systemPinkColor", {1, 0.18, 0.33, 1}, {1, 0.22, 0.37, 1}, 0},
    {"systemPurpleColor", {0.69, 0.32, 0.87, 1}, {0.75, 0.35, 0.95, 1}, 0},
    {"systemTealColor", {0.19, 0.69, 0.78, 1}, {0.25, 0.78, 0.88, 1}, 0},
    {"systemIndigoColor", {0.35, 0.34, 0.84, 1}, {0.37, 0.36, 0.9, 1}, 0},
    {"systemMintColor", {0, 0.78, 0.75, 1}, {0.4, 0.83, 0.81, 1}, 0},
    {"systemCyanColor", {0.33, 0.75, 0.94, 1}, {0.39, 0.82, 1, 1}, 0},
    {"systemGrayColor", {0.56, 0.56, 0.58, 1}, {0.6, 0.6, 0.62, 1}, 0},
};
#define NCOLS (int)(sizeof k_cols / sizeof k_cols[0])

int ac_count(void) { return NCOLS; }
const char *ac_name(int i) { return i >= 0 && i < NCOLS ? k_cols[i].sel : 0; }
int ac_index(const char *sel) {
    for (int i = 0; sel && i < NCOLS; i++) if (eq(sel, k_cols[i].sel)) return i;
    return -1;
}
void ac_resolve(int i, int dark, unsigned accent_rgb, double out[4]) {
    if (i < 0 || i >= NCOLS) { out[0] = out[1] = out[2] = 0.5; out[3] = 1; return; }
    const syscol_t *e = &k_cols[i];
    const double *v = dark ? e->d : e->l;
    for (int k = 0; k < 4; k++) out[k] = v[k];
    if (!e->accent) return;
    out[0] = ((accent_rgb >> 16) & 255) / 255.0; out[1] = ((accent_rgb >> 8) & 255) / 255.0; out[2] = (accent_rgb & 255) / 255.0; out[3] = 1;
    if (e->accent == 2) {   /* mistura com o fundo: clara no tema claro, escura no escuro */
        double keep = dark ? 0.5 : 0.35, base = dark ? 0 : 1;
        for (int k = 0; k < 3; k++) out[k] = out[k] * keep + base * (1 - keep);
    } else if (e->accent == 3) out[3] = 0.5;
}
int ac_is_dark(const char *name) { return name && has(name, "Dark"); }
int ac_best_match(const char *mine, const char *const *names, int n) {
    int dark = ac_is_dark(mine), vib = mine && has(mine, "Vibrant"), hc = mine && has(mine, "HighContrast");
    int best = -1, best_score = -1;
    for (int i = 0; i < n; i++) {
        const char *s = names[i];
        if (!s) continue;
        if (mine && eq(s, mine)) return i;
        if (ac_is_dark(s) != dark) continue;
        int score = 1 + (has(s, "Vibrant") == vib) * 2 + (has(s, "HighContrast") == hc);
        if (score > best_score) { best_score = score; best = i; }
    }
    return best;
}
