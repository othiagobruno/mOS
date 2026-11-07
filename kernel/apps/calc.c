/* mOS — Calculadora: modos Básica, Científica e Programador; expressão completa com precedência e
 * parênteses, funções (sin, cos, tan, ln, log, √, xʸ, x!, …), graus/radianos, memória, histórico em
 * barra lateral e conversões de base (hex/dec/oct/bin) com operações de bits. */
#include "kit.h"

enum { MODE_BASIC, MODE_SCI, MODE_PROG };
#define HIST 40
static struct {
    window_t *w;
    int mode;
    char expr[160];           /* expressão em edição (texto como mostrado) */
    char result[48];          /* prévia/resultado */
    bool just_eval;           /* o último toque foi "=" */
    bool deg;                 /* graus (true) ou radianos */
    bool second;              /* 2nd: funções inversas */
    double mem;
    bool has_mem;
    int base;                 /* programador: 16, 10, 8, 2 */
    struct { char e[160], r[48]; } hist[HIST];
    int nhist;
    bool show_hist;
    int hscroll;
} C = {.deg = true, .base = 10};

extern app_t app_calculadora;

/* ------------------------------------------------------------ avaliação (descida recursiva) */
typedef struct { const char *p; bool err; } ps_t;
static double p_expr(ps_t *s);
static void skip(ps_t *s) { while (*s->p == ' ') s->p++; }
static bool eat(ps_t *s, const char *t) { skip(s); usize n = strlen(t); if (!strncmp(s->p, t, n)) { s->p += n; return true; } return false; }
static double fact(double x) {
    if (x < 0 || x != (double)(i64)x || x > 170) return 0.0 / 0.0;
    double r = 1;
    for (int i = 2; i <= (int)x; i++) r *= i;
    return r;
}
static double ang_in(double x) { return C.deg ? x * KIT_PI / 180 : x; }
static double ang_out(double x) { return C.deg ? x * 180 / KIT_PI : x; }
static double p_number(ps_t *s) {
    skip(s);
    if (C.mode == MODE_PROG) {
        i64 v = 0;
        int digits = 0;
        for (;;) {
            char c = (char)toupper(*s->p);
            int d = isdigit(c) ? c - '0' : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : 99;
            if (d >= C.base) break;
            v = v * C.base + d;
            s->p++;
            digits++;
        }
        if (!digits) s->err = true;
        return (double)v;
    }
    double v = 0, fr = 0.1;
    int digits = 0;
    while (isdigit(*s->p)) { v = v * 10 + (*s->p++ - '0'); digits++; }
    if (*s->p == ',' || *s->p == '.') { s->p++; while (isdigit(*s->p)) { v += (*s->p++ - '0') * fr; fr /= 10; digits++; } }
    if (!digits) { s->err = true; return 0; }
    if (*s->p == 'E' && (isdigit(s->p[1]) || s->p[1] == '-')) {
        s->p++;
        bool neg = *s->p == '-';
        if (neg) s->p++;
        int e = 0;
        while (isdigit(*s->p)) e = e * 10 + (*s->p++ - '0');
        v *= kit_pow(10, neg ? -e : e);
    }
    return v;
}
static double p_primary(ps_t *s) {
    skip(s);
    if (eat(s, "(")) { double v = p_expr(s); if (!eat(s, ")")) {} return v; }
    if (eat(s, "−") || eat(s, "-")) return -p_primary(s);
    if (eat(s, "π")) return KIT_PI;
    if (C.mode != MODE_PROG && eat(s, "e") && !isalpha(*s->p)) return 2.718281828459045;
    static const char *fn[] = {"sin⁻¹", "cos⁻¹", "tan⁻¹", "sinh", "cosh", "tanh", "sin", "cos", "tan", "ln", "log₂", "log", "√", "∛"};
    for (int i = 0; i < (int)ARRAY_LEN(fn); i++) {
        if (!eat(s, fn[i])) continue;
        double x = p_primary(s);
        switch (i) {
        case 0: return ang_out(kit_asin(x));
        case 1: return ang_out(kit_acos(x));
        case 2: return ang_out(kit_atan(x));
        case 3: return (kit_exp(x) - kit_exp(-x)) / 2;
        case 4: return (kit_exp(x) + kit_exp(-x)) / 2;
        case 5: { double a = kit_exp(x), b = kit_exp(-x); return (a - b) / (a + b); }
        case 6: return kit_sin(ang_in(x));
        case 7: return kit_cos(ang_in(x));
        case 8: return kit_tan(ang_in(x));
        case 9: return kit_log(x);
        case 10: return kit_log(x) / kit_log(2);
        case 11: return kit_log(x) / kit_log(10);
        case 12: return x < 0 ? 0.0 / 0.0 : kit_sqrt(x);
        case 13: return x < 0 ? -kit_pow(-x, 1.0 / 3) : kit_pow(x, 1.0 / 3);
        }
    }
    if (C.mode == MODE_PROG && eat(s, "~")) return (double)~(i64)p_primary(s);
    return p_number(s);
}
static double p_postfix(ps_t *s) {
    double v = p_primary(s);
    for (;;) {
        skip(s);
        if (eat(s, "!")) v = fact(v);
        else if (eat(s, "%")) v /= 100;
        else if (eat(s, "²")) v *= v;
        else if (eat(s, "³")) v = v * v * v;
        else break;
    }
    return v;
}
static double p_power(ps_t *s) {
    double b = p_postfix(s);
    if (eat(s, "^")) return kit_pow(b, p_power(s));   /* associativa à direita */
    if (eat(s, "ʸ√")) { double r = p_power(s); return kit_pow(b, 1 / r); }
    return b;
}
static double p_term(ps_t *s) {
    double v = p_power(s);
    for (;;) {
        if (eat(s, "×") || eat(s, "*")) v *= p_power(s);
        else if (eat(s, "÷") || eat(s, "/")) { double d = p_power(s); v = d == 0 ? 0.0 / 0.0 : v / d; }
        else if (C.mode == MODE_PROG && eat(s, "mod")) { i64 d = (i64)p_power(s); v = d ? (double)((i64)v % d) : 0.0 / 0.0; }
        else if (C.mode == MODE_PROG && eat(s, "<<")) v = (double)((i64)v << ((i64)p_power(s) & 63));
        else if (C.mode == MODE_PROG && eat(s, ">>")) v = (double)((i64)v >> ((i64)p_power(s) & 63));
        else { skip(s); if (*s->p == '(' || (C.mode != MODE_PROG && (!strncmp(s->p, "π", 2) || !strncmp(s->p, "√", 3)))) v *= p_power(s); else break; }
    }
    return v;
}
static double p_sum(ps_t *s) {
    double v = p_term(s);
    for (;;) {
        if (eat(s, "+")) v += p_term(s);
        else if (eat(s, "−") || eat(s, "-")) v -= p_term(s);
        else break;
    }
    return v;
}
static double p_expr(ps_t *s) {
    double v = p_sum(s);
    if (C.mode != MODE_PROG) return v;
    for (;;) {
        if (eat(s, "AND")) v = (double)((i64)v & (i64)p_sum(s));
        else if (eat(s, "XOR")) v = (double)((i64)v ^ (i64)p_sum(s));
        else if (eat(s, "OR")) v = (double)((i64)v | (i64)p_sum(s));
        else break;
    }
    return v;
}
static bool evaluate(const char *e, double *out) {
    char buf[200];
    snprintf(buf, sizeof buf, "%s", e);
    /* fecha parênteses abertos */
    int open = 0;
    for (char *p = buf; *p; p++) open += *p == '(' ? 1 : *p == ')' ? -1 : 0;
    while (open-- > 0 && strlen(buf) < sizeof buf - 2) strcat(buf, ")");
    ps_t s = {buf, false};
    double v = p_expr(&s);
    skip(&s);
    if (s.err || *s.p) return false;
    *out = v;
    return v == v;
}

static void fmt_result(double v, char *out, int n) {
    if (v != v) { snprintf(out, n, "Erro"); return; }
    if (C.mode == MODE_PROG) {
        i64 x = (i64)v;
        if (C.base == 10) snprintf(out, n, "%lld", (long long)x);
        else if (C.base == 16) snprintf(out, n, "%llX", (unsigned long long)x);
        else if (C.base == 8) snprintf(out, n, "%llo", (unsigned long long)x);
        else {
            char b[70];
            int k = 0;
            u64 u = (u64)x;
            if (!u) b[k++] = '0';
            while (u && k < 64) { b[k++] = (char)('0' + (u & 1)); u >>= 1; }
            for (int i = 0; i < k; i++) out[i] = b[k - 1 - i];
            out[MIN(k, n - 1)] = 0;
        }
        return;
    }
    double a = kit_fabs(v);
    if (a != 0 && (a >= 1e15 || a < 1e-9)) {
        int e = 0;
        double m = a;
        while (m >= 10) { m /= 10; e++; }
        while (m < 1) { m *= 10; e--; }
        char mm[32];
        kit_fmt_num(mm, sizeof mm, v < 0 ? -m : m, 8);
        snprintf(out, n, "%se%d", mm, e);
        return;
    }
    kit_fmt_num(out, n, v, 10);
    /* separador de milhar */
    char tmp[64];
    snprintf(tmp, sizeof tmp, "%s", out);
    char *comma = strchr(tmp, ',');
    int intlen = comma ? (int)(comma - tmp) : (int)strlen(tmp);
    int start = tmp[0] == '-' ? 1 : 0, k = 0;
    for (int i = 0; tmp[i] && k < n - 1; i++) {
        if (i > start && i < intlen && (intlen - i) % 3 == 0) out[k++] = '.';
        out[k++] = tmp[i];
    }
    out[k] = 0;
}

static void preview(void) {
    double v;
    C.result[0] = 0;
    if (C.expr[0] && evaluate(C.expr, &v)) fmt_result(v, C.result, sizeof C.result);
}

/* ------------------------------------------------------------ teclas */
static bool is_op_end(void) {
    int n = (int)strlen(C.expr);
    if (!n) return false;
    const char *ops[] = {"+", "−", "×", "÷", "^", "AND", "OR", "XOR", "<<", ">>", "mod"};
    for (int i = 0; i < (int)ARRAY_LEN(ops); i++) { int l = (int)strlen(ops[i]); if (n >= l && !strcmp(C.expr + n - l, ops[i])) return true; }
    return false;
}
static void append(const char *t) {
    if (strlen(C.expr) + strlen(t) >= sizeof C.expr - 1) return;
    strcat(C.expr, t);
}
static void backspace(void) {
    int n = (int)strlen(C.expr);
    if (!n) return;
    /* apaga um token inteiro (funções, operadores de várias letras) */
    static const char *toks[] = {"sin⁻¹(", "cos⁻¹(", "tan⁻¹(", "sinh(", "cosh(", "tanh(", "sin(", "cos(", "tan(", "log₂(", "log(", "ln(", "√(", "∛(", "AND", "XOR", "OR", "<<", ">>", "mod", "ʸ√"};
    for (int i = 0; i < (int)ARRAY_LEN(toks); i++) { int l = (int)strlen(toks[i]); if (n >= l && !strcmp(C.expr + n - l, toks[i])) { C.expr[n - l] = 0; return; } }
    C.expr[utf8_prev(C.expr, n)] = 0;
}
static void equals(void) {
    double v;
    if (!C.expr[0]) return;
    if (!evaluate(C.expr, &v)) { snprintf(C.result, sizeof C.result, "Erro"); C.just_eval = true; return; }
    fmt_result(v, C.result, sizeof C.result);
    if (C.nhist == HIST) { memmove(C.hist, C.hist + 1, sizeof C.hist[0] * (HIST - 1)); C.nhist--; }
    snprintf(C.hist[C.nhist].e, 160, "%s", C.expr);
    snprintf(C.hist[C.nhist].r, 48, "%s", C.result);
    C.nhist++;
    /* o resultado vira o começo da próxima expressão (sem separador de milhar) */
    char raw[64];
    if (C.mode == MODE_PROG) snprintf(raw, sizeof raw, "%s", C.result);
    else { kit_fmt_num(raw, sizeof raw, v, 10); if (strchr(raw, 'e')) raw[0] = 0; }
    for (char *p = raw; *p; p++) if (*p == '-') { memmove(p + 3, p + 1, strlen(p)); memcpy(p, "−", 3); break; }
    snprintf(C.expr, sizeof C.expr, "%s", raw);
    C.just_eval = true;
}
static void press(const char *k) {
    bool digit = isdigit((u8)k[0]) || (C.mode == MODE_PROG && k[0] >= 'A' && k[0] <= 'F' && !k[1]);
    if (C.just_eval && (digit || !strcmp(k, ",") || !strcmp(k, "π") || !strcmp(k, "e"))) C.expr[0] = 0;
    C.just_eval = false;
    if (!strcmp(k, "AC")) { C.expr[0] = 0; C.result[0] = 0; return; }
    if (!strcmp(k, "⌫")) { backspace(); preview(); return; }
    if (!strcmp(k, "=")) { equals(); return; }
    if (!strcmp(k, "±")) {
        if (!strncmp(C.expr, "−(", 4)) { memmove(C.expr, C.expr + 4, strlen(C.expr + 4) + 1); int n = (int)strlen(C.expr); if (n && C.expr[n - 1] == ')') C.expr[n - 1] = 0; }
        else if (C.expr[0]) { char t[160]; snprintf(t, sizeof t, "−(%s)", C.expr); snprintf(C.expr, sizeof C.expr, "%s", t); }
        preview();
        return;
    }
    if (!strcmp(k, "Rad") || !strcmp(k, "Graus")) { C.deg = !C.deg; preview(); return; }
    if (!strcmp(k, "2nd")) { C.second = !C.second; return; }
    if (!strcmp(k, "mc")) { C.mem = 0; C.has_mem = false; return; }
    if (!strcmp(k, "mr")) { if (C.has_mem) { char r[48]; kit_fmt_num(r, 48, C.mem, 10); append(r); preview(); } return; }
    if (!strcmp(k, "m+") || !strcmp(k, "m−")) {
        double v;
        if (C.expr[0] && evaluate(C.expr, &v)) { C.mem += k[1] == '+' ? v : -v; C.has_mem = true; }
        return;
    }
    if (!strcmp(k, "Rand")) { char r[32]; kit_fmt_num(r, 32, (rand32() % 1000000) / 1000000.0, 6); append(r); preview(); return; }
    /* funções com parêntese */
    static const struct { const char *key, *ins; } fx[] = {
        {"sin", "sin("}, {"cos", "cos("}, {"tan", "tan("}, {"sin⁻¹", "sin⁻¹("}, {"cos⁻¹", "cos⁻¹("}, {"tan⁻¹", "tan⁻¹("},
        {"sinh", "sinh("}, {"cosh", "cosh("}, {"tanh", "tanh("}, {"ln", "ln("}, {"log₁₀", "log("}, {"log₂", "log₂("},
        {"√x", "√("}, {"³√x", "∛("}, {"x²", "²"}, {"x³", "³"}, {"xʸ", "^"}, {"eˣ", "e^"}, {"10ˣ", "10^"}, {"2ˣ", "2^"},
        {"1/x", "1÷"}, {"x!", "!"}, {"EE", "E"}, {"ʸ√x", "ʸ√"}, {"NOT", "~"}, {"X<<Y", "<<"}, {"X>>Y", ">>"},
    };
    for (int i = 0; i < (int)ARRAY_LEN(fx); i++)
        if (!strcmp(k, fx[i].key)) {
            if (!strcmp(k, "1/x")) { char t[160]; snprintf(t, sizeof t, "1÷(%s)", C.expr[0] ? C.expr : "1"); snprintf(C.expr, sizeof C.expr, "%s", t); }
            else append(fx[i].ins);
            preview();
            return;
        }
    bool op = !strcmp(k, "+") || !strcmp(k, "−") || !strcmp(k, "×") || !strcmp(k, "÷") || !strcmp(k, "AND") || !strcmp(k, "OR") || !strcmp(k, "XOR") || !strcmp(k, "mod");
    if (op && is_op_end()) backspace();               /* troca o operador */
    if (op && !C.expr[0] && strcmp(k, "−")) append("0");
    append(k);
    preview();
}

static void set_mode(int m);
/* ------------------------------------------------------------ desenho */
typedef struct { const char *k; int kind; } key_t;   /* kind: 0 número, 1 função (cinza escuro), 2 operador (laranja) */

static bool calc_key(ui_t *u, int id, rect_t r, const char *label, int kind, bool active) {
    surface_t *s = u->s;
    u32 bg = kind == 2 ? RGB(255, 159, 10) : kind == 1 ? RGB(80, 80, 84) : RGB(52, 52, 56);
    if (kind == 2 && active) bg = RGB(255, 255, 255);
    if (kind == 3) bg = RGB(64, 64, 68);
    bool hov = ui_hit(u, r), held = hov && u->down;
    if (held) bg = color_lighten(bg, 50);
    else if (hov) bg = color_lighten(bg, 14);
    float rad = MIN(r.w, r.h) / 2.0f;
    gfx_round_rect(s, r.x, r.y, r.w, r.h, rad, bg);
    gfx_specular_rim(s, r.x, r.y, r.w, r.h, rad, 40);
    u32 fg = kind == 2 && active ? RGB(255, 159, 10) : RGB(255, 255, 255);
    float fs = strlen(label) > 4 ? 12 : (kind == 0 && isdigit((u8)label[0])) ? 22 : 17;
    text_draw_center(s, font_get(kind == 0 ? FONT_REGULAR : FONT_MEDIUM), fs, r, fg, label);
    return ui_clicked(u, id, r);
}

static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    u32 bg = RGB(28, 28, 30);
    gfx_fill(s, 0, 0, u->cw, u->ch, bg);
    int hw = C.show_hist ? 220 : 0;
    /* histórico */
    if (hw) {
        gfx_fill(s, u->cw - hw, 0, hw, u->ch, RGB(38, 38, 40));
        ui_label(u, u->cw - hw + 14, 34, "Histórico", FONT_BOLD, 13, RGB(235, 235, 240));
        rect_t view = rect_make(u->cw - hw, 56, hw, u->ch - 96);
        rect_t pc = surface_push_clip(s, view);
        int y = view.y - C.hscroll;
        for (int i = C.nhist - 1; i >= 0; i--) {
            rect_t r = rect_make(view.x + 6, y, hw - 12, 48);
            if (ui_hit(u, r)) gfx_round_rect(s, r.x, r.y, r.w, r.h, 8, RGBA(255, 255, 255, 18));
            text_draw_ellipsis(s, font_get(FONT_REGULAR), 12, r.x + 8, r.y + 18, r.w - 16, RGB(160, 160, 166), C.hist[i].e);
            text_draw_ellipsis(s, font_get(FONT_MEDIUM), 17, r.x + 8, r.y + 40, r.w - 16, RGB(250, 250, 250), C.hist[i].r);
            if (ui_clicked(u, 90000 + i, r)) { snprintf(C.expr, sizeof C.expr, "%s", C.hist[i].e); preview(); C.just_eval = false; }
            y += 52;
        }
        surface_pop_clip(s, pc);
        ui_scroll(u, UI_ID, view, C.nhist * 52, &C.hscroll);
        if (C.nhist && ui_button_style(u, UI_ID, rect_make(u->cw - hw + 12, u->ch - 36, hw - 24, 26), "Limpar Histórico", BTN_GRAY, true, NULL)) C.nhist = 0;
    }
    int aw = u->cw - hw;
    /* visor: expressão (pequena) e resultado/prévia (grande) */
    int top = 30;
    font_t *f = font_get(FONT_REGULAR);
    const char *big = C.just_eval ? C.result : (C.expr[0] ? C.expr : "0");
    const char *small = C.just_eval ? (C.nhist ? C.hist[C.nhist - 1].e : "") : C.result;
    float fs = 46;
    while (fs > 18 && text_width(f, fs, big) > aw - 28) fs -= 2;
    int tw = text_width(f, fs, big);
    text_draw(s, f, fs, aw - 14 - tw, top + 70, RGB(255, 255, 255), big);
    if (small[0] && strcmp(small, big)) {
        int sw = text_width(f, 15, small);
        text_draw_ellipsis(s, f, 15, MAX(14, aw - 14 - sw), top + 22, aw - 28, RGB(150, 150, 156), small);
    }
    if (C.mode == MODE_SCI) text_draw(s, font_get(FONT_MEDIUM), 11, 14, top + 22, RGB(150, 150, 156), C.deg ? "Graus" : "Rad");
    if (C.has_mem) text_draw(s, font_get(FONT_MEDIUM), 11, C.mode == MODE_SCI ? 60 : 14, top + 22, RGB(150, 150, 156), "M");
    int ky = top + 92;
    if (C.mode == MODE_PROG) {
        /* bases e bits */
        double v = 0;
        bool ok = C.expr[0] && evaluate(C.just_eval ? C.expr : C.expr, &v);
        i64 x = ok ? (i64)v : 0;
        static const char *bl[4] = {"HEX", "DEC", "OCT", "BIN"};
        static const int bv[4] = {16, 10, 8, 2};
        int bw = (aw - 24) / 4;
        for (int i = 0; i < 4; i++) {
            rect_t r = rect_make(12 + i * bw, ky, bw - 6, 24);
            bool on = C.base == bv[i];
            gfx_round_rect(s, r.x, r.y, r.w, r.h, 12, on ? RGB(255, 159, 10) : RGB(52, 52, 56));
            text_draw_center(s, font_get(FONT_SEMIBOLD), 11, r, RGB(255, 255, 255), bl[i]);
            if (ui_clicked(u, 91000 + i, r) && !on) {
                int ob = C.base;
                C.base = bv[i];
                if (ok) { fmt_result(v, C.expr, sizeof C.expr); C.just_eval = true; fmt_result(v, C.result, sizeof C.result); }
                else C.expr[0] = 0;
                (void)ob;
            }
        }
        ky += 32;
        /* 32 bits em 4 grupos */
        for (int b = 31; b >= 0; b--) {
            int idx = 31 - b, grp = idx / 8, in = idx % 8;
            int bx = 14 + grp * ((aw - 28) / 4) + in * ((aw - 28) / 4 - 8) / 8;
            rect_t r = rect_make(bx, ky, 10, 16);
            char d[2] = {(char)('0' + ((x >> b) & 1)), 0};
            text_draw_center(s, font_get(FONT_MEDIUM), 12, r, (x >> b) & 1 ? RGB(255, 255, 255) : RGB(110, 110, 116), d);
            if (ui_clicked(u, 92000 + b, r)) {
                x ^= (i64)1 << b;
                fmt_result((double)x, C.expr, sizeof C.expr);
                preview();
                C.just_eval = false;
            }
        }
        ky += 26;
    }
    /* teclado */
    static const key_t basic[5][4] = {
        {{"⌫", 1}, {"AC", 1}, {"%", 1}, {"÷", 2}}, {{"7", 0}, {"8", 0}, {"9", 0}, {"×", 2}}, {{"4", 0}, {"5", 0}, {"6", 0}, {"−", 2}},
        {{"1", 0}, {"2", 0}, {"3", 0}, {"+", 2}}, {{"±", 0}, {"0", 0}, {",", 0}, {"=", 2}}};
    static const key_t sci[5][6] = {
        {{"(", 3}, {")", 3}, {"mc", 3}, {"m+", 3}, {"m−", 3}, {"mr", 3}},
        {{"2nd", 3}, {"x²", 3}, {"x³", 3}, {"xʸ", 3}, {"eˣ", 3}, {"10ˣ", 3}},
        {{"1/x", 3}, {"√x", 3}, {"³√x", 3}, {"ʸ√x", 3}, {"ln", 3}, {"log₁₀", 3}},
        {{"x!", 3}, {"sin", 3}, {"cos", 3}, {"tan", 3}, {"e", 3}, {"EE", 3}},
        {{"Rad", 3}, {"sinh", 3}, {"cosh", 3}, {"tanh", 3}, {"π", 3}, {"Rand", 3}}};
    static const key_t prog[5][3] = {
        {{"AND", 3}, {"OR", 3}, {"XOR", 3}}, {{"NOT", 3}, {"X<<Y", 3}, {"X>>Y", 3}}, {{"D", 0}, {"E", 0}, {"F", 0}},
        {{"A", 0}, {"B", 0}, {"C", 0}}, {{"mod", 3}, {"(", 3}, {")", 3}}};
    int extra = C.mode == MODE_SCI ? 6 : C.mode == MODE_PROG ? 3 : 0;
    int cols = 4 + extra, gap = 8;
    int kw = (aw - 16 - gap * (cols - 1)) / cols, kh = MIN(kw, (u->ch - ky - 12 - gap * 4) / 5);
    const char *opk = NULL;
    int n = (int)strlen(C.expr);
    if (n) { const char *o[] = {"+", "−", "×", "÷"}; for (int i = 0; i < 4; i++) if (n >= (int)strlen(o[i]) && !strcmp(C.expr + n - strlen(o[i]), o[i])) opk = o[i]; }
    for (int row = 0; row < 5; row++) {
        for (int c = 0; c < cols; c++) {
            key_t k;
            if (c < extra) {
                if (C.mode == MODE_SCI) {
                    k = sci[row][c];
                    if (C.second) {
                        if (!strcmp(k.k, "sin")) k.k = "sin⁻¹";
                        else if (!strcmp(k.k, "cos")) k.k = "cos⁻¹";
                        else if (!strcmp(k.k, "tan")) k.k = "tan⁻¹";
                        else if (!strcmp(k.k, "eˣ")) k.k = "2ˣ";
                        else if (!strcmp(k.k, "log₁₀")) k.k = "log₂";
                    }
                    if (!strcmp(k.k, "Rad") && !C.deg) k.k = "Graus";
                } else k = prog[row][c];
            } else k = basic[row][c - extra];
            if (C.mode == MODE_PROG && !strcmp(k.k, ",")) k.k = "00";
            if (C.mode == MODE_PROG && !strcmp(k.k, "%")) k.k = "mod";
            rect_t r = rect_make(8 + c * (kw + gap), ky + row * (kh + gap), kw, kh);
            bool en = true;
            if (C.mode == MODE_PROG && strlen(k.k) == 1) {
                char ch = k.k[0];
                int d = isdigit((u8)ch) ? ch - '0' : (ch >= 'A' && ch <= 'F') ? ch - 'A' + 10 : -1;
                if (d >= C.base) en = false;
            }
            bool act = (opk && !strcmp(k.k, opk)) || (!strcmp(k.k, "2nd") && C.second);
            if (!en) {
                gfx_round_rect(s, r.x, r.y, r.w, r.h, MIN(r.w, r.h) / 2.0f, RGB(40, 40, 44));
                text_draw_center(s, font_get(FONT_REGULAR), 20, r, RGB(90, 90, 96), k.k);
                continue;
            }
            if (calc_key(u, 93000 + row * 16 + c, r, k.k, k.kind, act)) {
                if (!strcmp(k.k, "00")) { press("0"); press("0"); }
                else press(k.k);
            }
        }
    }
    /* teclado físico */
    if (u->key && !(u->mods & MOD_CMD)) {
        int c = u->key;
        char k[8] = {0};
        if (isdigit(c)) k[0] = (char)c;
        else if (C.mode == MODE_PROG && isalpha(c) && toupper(c) <= 'F') k[0] = (char)toupper(c);
        else if (c == ',' || c == '.') k[0] = ',';
        else if (c == '+') k[0] = '+';
        else if (c == '-') strcpy(k, "−");
        else if (c == '*' || c == 'x') strcpy(k, "×");
        else if (c == '/') strcpy(k, "÷");
        else if (c == '(' || c == ')' || c == '%' || c == '^' || c == '!') k[0] = (char)c;
        else if (c == KEY_ENTER || c == '=') k[0] = '=';
        else if (c == KEY_ESC) strcpy(k, "AC");
        else if (c == KEY_BACKSPACE || c == KEY_DELETE) strcpy(k, "⌫");
        if (k[0] == '^') strcpy(k, "xʸ");
        if (k[0]) { press(k); u->key_used = true; }
    }
    if ((u->mods & MOD_CMD) && u->key >= '1' && u->key <= '3') { set_mode(u->key - '1'); u->key_used = true; return; }
    if ((u->mods & MOD_CMD) && u->key == 'l') { C.show_hist = !C.show_hist; set_mode(C.mode); u->key_used = true; return; }
    if ((u->mods & MOD_CMD) && u->key == 'c') { clipboard_set(C.just_eval ? C.result : C.expr, -1); u->key_used = true; }
    if ((u->mods & MOD_CMD) && u->key == 'v') { const char *t = clipboard_get(); for (; t && *t; t++) if (isdigit((u8)*t) || *t == ',') { char d[2] = {*t, 0}; press(d); } u->key_used = true; }
}

static void set_mode(int m) {
    C.mode = m;
    C.expr[0] = 0; C.result[0] = 0; C.just_eval = false;
    if (!C.w) return;
    int wdt = m == MODE_BASIC ? 260 : m == MODE_SCI ? 600 : 420;
    int hgt = m == MODE_PROG ? 470 : 420;
    C.w->w = wdt + (C.show_hist ? 220 : 0);
    C.w->h = hgt;
    wm_invalidate(C.w);
    wm_damage_all();
}

static void calc_close(window_t *w) { C.w = NULL; }
static void calc_launch(app_t *a) {
    if (C.w) { wm_focus(C.w); return; }
    C.w = wm_create(&app_calculadora, "", -1, -1, 260, 420, WIN_DARK_CHROME | WIN_NO_ZOOM);
    if (!C.w) return;
    C.w->paint = paint;
    C.w->on_close = calc_close;
    C.w->bg = RGB(28, 28, 30);
    C.w->title_bg = RGB(28, 28, 30);
    set_mode(C.mode);
}

enum { M_BASIC = 1, M_SCI, M_PROG, M_HIST, M_COPY, M_PASTE, M_RPN };
static menu_item_t m_view[] = {{"Básica", "⌘1", M_BASIC}, {"Científica", "⌘2", M_SCI}, {"Programador", "⌘3", M_PROG}, {NULL}, {"Mostrar Histórico", "⌘L", M_HIST}};
static menu_item_t m_edit[] = {{"Copiar", "⌘C", M_COPY}, {"Colar", "⌘V", M_PASTE}};
static menu_t menus[] = {{"Editar", m_edit, 2}, {"Visualizar", m_view, 5}};
static void on_menu(app_t *a, int act) {
    if (act >= M_BASIC && act <= M_PROG) set_mode(act - M_BASIC);
    else if (act == M_HIST) { C.show_hist = !C.show_hist; m_view[4].checked = C.show_hist; set_mode(C.mode); }
    else if (act == M_COPY) clipboard_set(C.just_eval ? C.result : C.expr, -1);
    else if (act == M_PASTE) { const char *t = clipboard_get(); for (; t && *t; t++) if (isdigit((u8)*t) || *t == ',') { char d[2] = {*t, 0}; press(d); } }
    for (int i = 0; i < 3; i++) m_view[i].checked = C.mode == i;
}

app_t app_calculadora = {.name = "Calculadora", .exec = "calculadora", .draw_icon = icon_calc, .launch = calc_launch, .on_menu = on_menu,
                         .menus = menus, .nmenus = 2, .pinned = true};
