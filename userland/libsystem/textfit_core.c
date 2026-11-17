/* mOS — encaixe de texto (ver textfit_core.h). Código original do mOS. */
#include "textfit_core.h"

static int len(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int next(const char *s, int i, int n) { if (i < n) i++; while (i < n && ((unsigned char)s[i] & 0xC0) == 0x80) i++; return i; }
static int prev(const char *s, int i) { if (i > 0) i--; while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--; return i; }

/* maior prefixo (em caracteres inteiros) de s[0..n) com largura <= w; 0 se nem o primeiro cabe */
static int fit_prefix(const char *s, int n, double w, tfit_measure_fn m, void *ctx) {
    int best = 0;
    for (int k = next(s, 0, n); k <= n && k > best; k = next(s, k, n)) {
        if (m(s, k, ctx) > w) break;
        best = k;
        if (k == n) break;
    }
    return best;
}

int tfit_break(const char *s, double w, int mode, tfit_measure_fn m, void *ctx, int *skip) {
    int e = 0;
    while (s[e] && s[e] != '\n') e++;
    *skip = s[e] == '\n' ? 1 : 0;
    if (e == 0) return 0;
    if (m(s, e, ctx) <= w) return e;
    if (mode == TFIT_WORD) {
        int best = 0;
        for (int i = 0; i < e; i++) {
            if (s[i] != ' ' || i == 0 || s[i - 1] == ' ') continue;   /* fim de palavra */
            if (m(s, i, ctx) > w) break;
            best = i;
        }
        if (best > 0) {
            int k = best;
            while (k < e && s[k] == ' ') k++;
            *skip = k - best;
            if (k == e && s[e] == '\n') (*skip)++;   /* só espaços até o fim da linha */
            return best;
        }
        /* a primeira palavra sozinha não cabe: quebra dentro dela, por caractere */
    }
    int k = fit_prefix(s, e, w, m, ctx);
    if (k == 0) k = next(s, 0, e);   /* pelo menos um caractere por linha */
    *skip = 0;
    if (k == e && s[e] == '\n') *skip = 1;
    return k;
}

int tfit_lines(const char *s, double w, int mode, tfit_measure_fn m, void *ctx) {
    int n = len(s), p = 0, lines = 0;
    for (;;) {
        int skip, k = tfit_break(s + p, w, mode, m, ctx, &skip);
        lines++;
        int was_nl = p + k + skip > 0 && s[p + k + skip - 1] == '\n' && skip > 0;
        p += k + skip;
        if (p >= n) { if (was_nl) lines++; break; }   /* \n no fim abre uma linha vazia */
        if (k + skip == 0) p++;                       /* segurança: sempre anda */
    }
    return lines;
}

int tfit_truncate(const char *s, int n, double w, int mode, tfit_measure_fn m, void *ctx, char *out, int cap) {
    static const char ell[] = "\xE2\x80\xA6";
    int o = 0;
#define PUT(src, cnt) do { for (int q_ = 0; q_ < (cnt) && o < cap - 1; q_++) out[o++] = (src)[q_]; } while (0)
    if (cap <= 0) return 0;
    if (m(s, n, ctx) <= w || mode < TFIT_HEAD || mode > TFIT_MIDDLE) {
        PUT(s, n);
        out[o] = 0;
        return o;
    }
    double we = m(ell, 3, ctx), room = w - we;
    if (mode == TFIT_TAIL) {
        int k = room > 0 ? fit_prefix(s, n, room, m, ctx) : 0;
        while (k > 0 && s[k - 1] == ' ') k--;   /* sem espaço antes das reticências */
        PUT(s, k); PUT(ell, 3);
    } else if (mode == TFIT_HEAD) {
        int j = n;
        while (j > 0) { int pj = prev(s, j); if (m(s + pj, n - pj, ctx) > room) break; j = pj; }
        while (j < n && s[j] == ' ') j++;
        PUT(ell, 3); PUT(s + j, n - j);
    } else {   /* meio: metade da largura para o começo, o resto para o fim */
        int k = room > 0 ? fit_prefix(s, n, room / 2, m, ctx) : 0;
        double left = room - (k ? m(s, k, ctx) : 0);
        int j = n;
        while (j > k) { int pj = prev(s, j); if (m(s + pj, n - pj, ctx) > left) break; j = pj; }
        PUT(s, k); PUT(ell, 3); PUT(s + j, n - j);
    }
#undef PUT
    out[o] = 0;
    return o;
}
