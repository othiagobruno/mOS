/* Regras puras do texto e da largura de fonte que o AppKit dos apps Electron importa. */
#pragma once

#define CX_FONT_WIDTH_EXPANDED 0.4

/* Marcador de NSTextList. "{decimal}" vira o número; hífen fica "-". */
static inline int cx_textlist_marker(const char *fmt, int n, char *out, int cap) {
    int i = 0, k = 0;
    if (!out || cap < 2) return 0;
    if (!fmt) fmt = "{decimal}.";
    if (n < 1) n = 1;
    while (fmt[i] && k < cap - 1) {
        if (fmt[i] == '{' ) {
            const char *e = fmt + i + 1;
            while (*e && *e != '}') e++;
            if (e[0] == '}' && e - (fmt + i) == 8 && fmt[i + 1] == 'd') {
                char num[16];
                int p = 0, v = n;
                if (!v) num[p++] = '0';
                while (v && p < 15) { num[p++] = (char)('0' + v % 10); v /= 10; }
                while (p && k < cap - 1) out[k++] = num[--p];
                i = (int)(e - fmt) + 1;
                continue;
            }
        }
        out[k++] = fmt[i++];
    }
    out[k] = 0;
    return k;
}

/* Retângulo de linha limitado à largura do NSTextContainer, descontando o padding. */
static inline void cx_text_fragment(double cw, double pad, double w, double *ow) {
    double maxw = cw - pad * 2;
    if (maxw < 0) maxw = 0;
    if (w > maxw) w = maxw;
    if (w < 0) w = 0;
    *ow = w;
}
