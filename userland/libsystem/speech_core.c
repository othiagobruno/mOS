/* mOS — síntese de fala (ver speech_core.h). Código original do mOS: regras de pronúncia próprias, tabela de
 * formantes de valores fonéticos de manual e o sintetizador em cascata escrito aqui. */
#include "speech_core.h"

typedef unsigned int u32;
#define PI 3.14159265358979323846

/* ================================================================ fonemas */
enum { T_PAUSE, T_VOWEL, T_NASAL, T_LIQUID, T_GLIDE, T_FRIC, T_STOP, T_AFFR, T_ASP };
typedef struct {
    const char *sym; unsigned char type, voiced;
    short f1, f2, f3, ms;       /* formantes-alvo (voz masculina) e duração base */
    short nf, nbw, namp;        /* ruído: centro, banda, intensidade (0..100) */
} ph_info;
enum {
    P_SIL, P_A, P_AH, P_E, P_EH, P_I, P_O, P_OH, P_U, P_IH, P_UH, P_VV, P_AE, P_ER,
    P_P, P_B, P_T, P_D, P_K, P_G, P_F, P_V, P_S, P_Z, P_SH, P_ZH, P_X, P_H, P_TH, P_DH,
    P_M, P_N, P_NJ, P_NG, P_L, P_LJ, P_R, P_RR, P_W, P_J, P_TS, P_DZ, P_COUNT
};
static const ph_info PH[P_COUNT] = {
    [P_SIL] = {"_", T_PAUSE, 0, 0, 0, 0, 120, 0, 0, 0},
    [P_A] = {"a", T_VOWEL, 1, 730, 1250, 2500, 120, 0, 0, 0},
    [P_AH] = {"@", T_VOWEL, 1, 600, 1300, 2500, 75, 0, 0, 0},
    [P_E] = {"e", T_VOWEL, 1, 400, 2000, 2600, 110, 0, 0, 0},
    [P_EH] = {"E", T_VOWEL, 1, 550, 1800, 2500, 120, 0, 0, 0},
    [P_I] = {"i", T_VOWEL, 1, 280, 2250, 2900, 100, 0, 0, 0},
    [P_O] = {"o", T_VOWEL, 1, 420, 850, 2400, 110, 0, 0, 0},
    [P_OH] = {"O", T_VOWEL, 1, 570, 900, 2400, 120, 0, 0, 0},
    [P_U] = {"u", T_VOWEL, 1, 300, 800, 2300, 100, 0, 0, 0},
    [P_IH] = {"I", T_VOWEL, 1, 400, 1900, 2550, 80, 0, 0, 0},
    [P_UH] = {"U", T_VOWEL, 1, 450, 1050, 2250, 80, 0, 0, 0},
    [P_VV] = {"V", T_VOWEL, 1, 640, 1200, 2400, 90, 0, 0, 0},
    [P_AE] = {"ae", T_VOWEL, 1, 660, 1700, 2400, 120, 0, 0, 0},
    [P_ER] = {"3", T_VOWEL, 1, 490, 1350, 1700, 110, 0, 0, 0},
    [P_P] = {"p", T_STOP, 0, 400, 900, 2200, 85, 900, 800, 55},
    [P_B] = {"b", T_STOP, 1, 300, 900, 2200, 75, 900, 800, 40},
    [P_T] = {"t", T_STOP, 0, 400, 1700, 2600, 85, 4200, 1500, 60},
    [P_D] = {"d", T_STOP, 1, 300, 1700, 2600, 75, 4000, 1500, 45},
    [P_K] = {"k", T_STOP, 0, 400, 2000, 2500, 90, 2200, 1000, 60},
    [P_G] = {"g", T_STOP, 1, 300, 2000, 2500, 80, 2200, 1000, 45},
    [P_F] = {"f", T_FRIC, 0, 400, 1100, 2300, 100, 6500, 3000, 30},
    [P_V] = {"v", T_FRIC, 1, 300, 1100, 2300, 80, 6500, 3000, 18},
    [P_S] = {"s", T_FRIC, 0, 400, 1700, 2600, 110, 5800, 1600, 75},
    [P_Z] = {"z", T_FRIC, 1, 300, 1700, 2600, 85, 5800, 1600, 45},
    [P_SH] = {"S", T_FRIC, 0, 400, 1900, 2500, 110, 3300, 1300, 75},
    [P_ZH] = {"Z", T_FRIC, 1, 300, 1900, 2500, 85, 3300, 1300, 45},
    [P_X] = {"x", T_FRIC, 0, 500, 1300, 2400, 80, 1600, 1600, 35},
    [P_H] = {"h", T_ASP, 0, 500, 1400, 2500, 60, 0, 0, 30},
    [P_TH] = {"T", T_FRIC, 0, 400, 1400, 2600, 90, 6000, 3500, 22},
    [P_DH] = {"D", T_FRIC, 1, 300, 1400, 2600, 55, 6000, 3500, 12},
    [P_M] = {"m", T_NASAL, 1, 250, 1100, 2200, 70, 0, 0, 0},
    [P_N] = {"n", T_NASAL, 1, 250, 1500, 2500, 65, 0, 0, 0},
    [P_NJ] = {"J", T_NASAL, 1, 250, 2000, 2700, 75, 0, 0, 0},
    [P_NG] = {"N", T_NASAL, 1, 250, 2300, 2700, 70, 0, 0, 0},
    [P_L] = {"l", T_LIQUID, 1, 360, 1300, 2700, 60, 0, 0, 0},
    [P_LJ] = {"L", T_LIQUID, 1, 300, 2000, 2800, 75, 0, 0, 0},
    [P_R] = {"r", T_LIQUID, 1, 420, 1400, 2300, 30, 0, 0, 0},
    [P_RR] = {"R", T_LIQUID, 1, 420, 1300, 1650, 65, 0, 0, 0},
    [P_W] = {"w", T_GLIDE, 1, 300, 700, 2200, 50, 0, 0, 0},
    [P_J] = {"j", T_GLIDE, 1, 280, 2200, 2900, 50, 0, 0, 0},
    [P_TS] = {"tS", T_AFFR, 0, 400, 1900, 2500, 120, 3300, 1300, 70},
    [P_DZ] = {"dZ", T_AFFR, 1, 300, 1900, 2500, 105, 3300, 1300, 45},
};

/* ================================================================ texto */
static int decode(const char *s, u32 *cp, int *at, int cap) {
    int n = 0;
    const unsigned char *p = (const unsigned char *)s;
    for (int i = 0; p[i] && n < cap;) {
        u32 c = p[i];
        int len = 1;
        if (c >= 0xF0 && p[i + 1] && p[i + 2] && p[i + 3]) { c = (c & 7) << 18 | (p[i + 1] & 63u) << 12 | (p[i + 2] & 63u) << 6 | (p[i + 3] & 63u); len = 4; }
        else if (c >= 0xE0 && p[i + 1] && p[i + 2]) { c = (c & 15) << 12 | (p[i + 1] & 63u) << 6 | (p[i + 2] & 63u); len = 3; }
        else if (c >= 0xC0 && p[i + 1]) { c = (c & 31) << 6 | (p[i + 1] & 63u); len = 2; }
        cp[n] = c; at[n] = i; n++;
        i += len;
    }
    return n;
}
static u32 lower(u32 c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
    return c;
}
static int is_letter(u32 c) { c = lower(c); return (c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFF && c != 0xF7) || c == '\''; }
static int is_digit(u32 c) { return c >= '0' && c <= '9'; }
/* letra sem acento e o acento: 1 agudo, 2 circunflexo, 3 til, 4 grave, 5 cedilha/trema */
static u32 base_of(u32 c, int *acc) {
    *acc = 0;
    switch (c) {
    case 0xE1: *acc = 1; return 'a'; case 0xE2: *acc = 2; return 'a'; case 0xE3: *acc = 3; return 'a'; case 0xE0: *acc = 4; return 'a';
    case 0xE9: *acc = 1; return 'e'; case 0xEA: *acc = 2; return 'e'; case 0xE8: *acc = 4; return 'e';
    case 0xED: *acc = 1; return 'i'; case 0xEC: *acc = 4; return 'i';
    case 0xF3: *acc = 1; return 'o'; case 0xF4: *acc = 2; return 'o'; case 0xF5: *acc = 3; return 'o'; case 0xF2: *acc = 4; return 'o';
    case 0xFA: *acc = 1; return 'u'; case 0xFC: *acc = 5; return 'u'; case 0xF9: *acc = 4; return 'u';
    case 0xE7: *acc = 5; return 'c'; case 0xF1: *acc = 3; return 'n';
    default: return c;
    }
}
static int is_vowel(u32 c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; }

typedef struct { sp_ph *out; int n, cap, loc, len; } emitter;
static void emit(emitter *e, int ph, int stress, int nasal) {
    if (e->n >= e->cap) return;
    sp_ph *p = &e->out[e->n++];
    p->ph = (unsigned char)ph; p->stress = (unsigned char)stress; p->nasal = (unsigned char)nasal; p->pause = 0;
    p->word_loc = e->loc; p->word_len = e->len;
}
static void emit_pause(emitter *e, int kind) {
    if (e->n >= e->cap) return;
    if (e->n && e->out[e->n - 1].ph == P_SIL) { if (kind > e->out[e->n - 1].pause) e->out[e->n - 1].pause = (unsigned char)kind; return; }
    sp_ph *p = &e->out[e->n++];
    p->ph = P_SIL; p->stress = 0; p->nasal = 0; p->pause = (unsigned char)kind; p->word_loc = e->loc; p->word_len = 0;
}

/* ---- números por extenso */
static const char *PT_U[] = {"zero", "um", "dois", "três", "quatro", "cinco", "seis", "sete", "oito", "nove", "dez", "onze", "doze", "treze",
                             "catorze", "quinze", "dezesseis", "dezessete", "dezoito", "dezenove"};
static const char *PT_T[] = {"", "", "vinte", "trinta", "quarenta", "cinquenta", "sessenta", "setenta", "oitenta", "noventa"};
static const char *PT_H[] = {"", "cento", "duzentos", "trezentos", "quatrocentos", "quinhentos", "seiscentos", "setecentos", "oitocentos", "novecentos"};
static const char *EN_U[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve", "thirteen",
                             "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
static const char *EN_T[] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
static void cat(char *d, int cap, const char *s) {
    int n = 0;
    while (d[n]) n++;
    for (int i = 0; s[i] && n < cap - 1; i++) d[n++] = s[i];
    d[n] = 0;
}
static void pt_under_1000(char *d, int cap, long v) {
    if (v == 100) { cat(d, cap, "cem"); return; }
    int h = (int)(v / 100), r = (int)(v % 100);
    if (h) { cat(d, cap, PT_H[h]); if (r) cat(d, cap, " e "); }
    if (r >= 20) { cat(d, cap, PT_T[r / 10]); if (r % 10) { cat(d, cap, " e "); cat(d, cap, PT_U[r % 10]); } }
    else if (r || !h) cat(d, cap, PT_U[r]);
}
static void en_under_1000(char *d, int cap, long v) {
    int h = (int)(v / 100), r = (int)(v % 100);
    if (h) { cat(d, cap, EN_U[h]); cat(d, cap, " hundred"); if (r) cat(d, cap, " "); }
    if (r >= 20) { cat(d, cap, EN_T[r / 10]); if (r % 10) { cat(d, cap, " "); cat(d, cap, EN_U[r % 10]); } }
    else if (r || !h) cat(d, cap, EN_U[r]);
}
static void number_words(long v, int lang, char *d, int cap) {
    d[0] = 0;
    if (v >= 1000000000L) { /* números grandes: dígito a dígito */
        char tmp[24]; int n = 0; long x = v;
        while (x && n < 23) { tmp[n++] = (char)('0' + x % 10); x /= 10; }
        while (n--) { cat(d, cap, lang == SP_LANG_PT ? PT_U[tmp[n] - '0'] : EN_U[tmp[n] - '0']); cat(d, cap, " "); }
        return;
    }
    long mi = v / 1000000, th = (v / 1000) % 1000, un = v % 1000;
    if (lang == SP_LANG_PT) {
        if (mi) { pt_under_1000(d, cap, mi); cat(d, cap, mi == 1 ? " milhão" : " milhões"); if (th || un) cat(d, cap, (th == 0 && (un < 100 || un % 100 == 0)) ? " e " : " "); }
        if (th) { if (th != 1) { pt_under_1000(d, cap, th); cat(d, cap, " "); } cat(d, cap, "mil"); if (un) cat(d, cap, (un < 100 || un % 100 == 0) ? " e " : " "); }
        if (un || (!mi && !th)) pt_under_1000(d, cap, un);
    } else {
        if (mi) { en_under_1000(d, cap, mi); cat(d, cap, " million"); if (th || un) cat(d, cap, " "); }
        if (th) { en_under_1000(d, cap, th); cat(d, cap, " thousand"); if (un) cat(d, cap, " "); }
        if (un || (!mi && !th)) en_under_1000(d, cap, un);
    }
}

/* ---- português do Brasil */
static int pt_nuclei(const u32 *w, int n, int *pos, int cap) {
    int k = 0;
    for (int i = 0; i < n && k < cap; i++) {
        int acc;
        u32 c = base_of(w[i], &acc);
        if (!is_vowel(c)) continue;
        /* u de qu/gu antes de e/i não é núcleo */
        if (c == 'u' && i > 0 && (w[i - 1] == 'q' || w[i - 1] == 'g') && i + 1 < n && (base_of(w[i + 1], &acc) == 'e' || base_of(w[i + 1], &acc) == 'i')) continue;
        /* ditongo: i/u átono depois de vogal faz parte do mesmo núcleo */
        int a2;
        if (i > 0 && (c == 'i' || c == 'u') && acc == 0 && is_vowel(base_of(w[i - 1], &a2)) && !(i + 1 < n && (w[i + 1] == 'm' || w[i + 1] == 'n') && i + 2 >= n)) continue;
        pos[k++] = i;
    }
    return k;
}
static int pt_stress_index(const u32 *w, int n, const int *pos, int k) {
    if (k <= 1) return 0;
    for (int j = 0; j < k; j++) {
        for (int i = pos[j]; i < (j + 1 < k ? pos[j + 1] : n); i++) { int acc; base_of(w[i], &acc); if (acc == 1 || acc == 2) return j; }
    }
    for (int j = 0; j < k; j++) { int acc; base_of(w[pos[j]], &acc); if (acc == 3) return j; }
    /* paroxítona: termina em a, e, o (com ou sem s), am, em, ens */
    u32 last = n ? w[n - 1] : 0, prev = n > 1 ? w[n - 2] : 0;
    int acc;
    u32 lb = base_of(last, &acc), pb = base_of(prev, &acc);
    int parox = lb == 'a' || lb == 'e' || lb == 'o' || (lb == 's' && (pb == 'a' || pb == 'e' || pb == 'o')) ||
                (lb == 'm' && (pb == 'a' || pb == 'e')) || (n > 2 && lb == 's' && pb == 'n' && base_of(w[n - 3], &acc) == 'e');
    return parox ? k - 2 : k - 1;
}
/* palavras átonas (artigos, preposições, pronomes oblíquos, conjunções): sem sílaba tônica */
static int word_is(const u32 *w, int n, const char *s) { int i = 0; while (s[i] && i < n && w[i] == (u32)(unsigned char)s[i]) i++; return !s[i] && i == n; }
static int pt_clitic(const u32 *w, int n) {
    static const char *list[] = {"a", "o", "e", "as", "os", "de", "do", "da", "dos", "das", "que", "se", "me", "te", "lhe", "nos", "em",
                                 "no", "na", "nas", "um", "com", "por", "sem", "mas", "ou", "lo", "la", "ao", "aos", 0};
    for (int i = 0; list[i]; i++) if (word_is(w, n, list[i])) return 1;
    return 0;
}
static void g2p_pt(const u32 *w, int n, emitter *e) {
    int pos[64], k = pt_nuclei(w, n, pos, 64), st = pt_stress_index(w, n, pos, k);
    int stressed_at = k && !pt_clitic(w, n) ? pos[st] : -1, last_nucleus = k ? pos[k - 1] : -1;
    for (int i = 0; i < n; i++) {
        int acc, a2;
        u32 c = base_of(w[i], &acc);
        u32 nx = i + 1 < n ? base_of(w[i + 1], &a2) : 0, nx2 = i + 2 < n ? base_of(w[i + 2], &a2) : 0;
        u32 pv = i > 0 ? base_of(w[i - 1], &a2) : 0;
        int stress = i == stressed_at;
        if (is_vowel(c)) {
            /* nasal: vogal + m/n seguida de consoante ou fim, ou til */
            int nasal = acc == 3 || ((nx == 'm' || nx == 'n') && (i + 2 >= n || (!is_vowel(nx2) && nx2 != 'h')));
            int final_unstressed = i >= last_nucleus && !stress && i > stressed_at;
            if (c == 'u' && pv == 'q' && is_vowel(nx)) { emit(e, nx == 'e' || nx == 'i' ? -1 : P_W, 0, 0); if (e->n && e->out[e->n - 1].ph == (unsigned char)-1) e->n--; continue; }
            if (c == 'u' && pv == 'g' && (nx == 'e' || nx == 'i') && acc != 5) continue;
            int ph;
            switch (c) {
            case 'a': ph = (final_unstressed || (nasal && !stress && acc != 3)) ? P_AH : P_A; break;
            case 'e': ph = acc == 1 ? P_EH : final_unstressed && !nasal ? P_I : P_E; break;
            case 'o': ph = acc == 1 ? P_OH : final_unstressed && !nasal ? P_U : P_O; break;
            case 'i': ph = P_I; break;
            default: ph = P_U; break;
            }
            /* ditongos: i/u depois de vogal viram semivogais */
            if ((c == 'i' || c == 'u') && acc == 0 && i > 0 && is_vowel(pv) && !stress) ph = c == 'i' ? P_J : P_W;
            emit(e, ph, stress, nasal);
            if (c == 'a' && acc == 3 && nx == 'o') { emit(e, P_W, 0, 1); i++; }            /* ão */
            else if ((c == 'a' || c == 'o') && acc == 3 && nx == 'e') { emit(e, P_J, 0, 1); i++; }   /* ãe, õe */
            else if (c == 'e' && nasal && nx == 'm' && i + 2 >= n) emit(e, P_J, 0, 1);         /* -em final: ẽj */
            if (nasal && (nx == 'm' || nx == 'n')) i++;
            continue;
        }
        switch (c) {
        case 'b': emit(e, P_B, 0, 0); break;
        case 'c':
            if (nx == 'h') { emit(e, P_SH, 0, 0); i++; }
            else if (acc == 5 || nx == 'e' || nx == 'i') emit(e, P_S, 0, 0);
            else emit(e, P_K, 0, 0);
            break;
        case 'd': {
            /* d antes do som de i: dʒ (inclusive o e final átono) */
            int to_i = nx == 'i' || (nx == 'e' && i + 2 >= n && stressed_at != i + 1);
            emit(e, to_i ? P_DZ : P_D, 0, 0);
            break;
        }
        case 'f': emit(e, P_F, 0, 0); break;
        case 'g': emit(e, (nx == 'e' || nx == 'i') ? P_ZH : P_G, 0, 0); break;
        case 'h': break;   /* mudo (os dígrafos são tratados nas outras letras) */
        case 'j': emit(e, P_ZH, 0, 0); break;
        case 'k': emit(e, P_K, 0, 0); break;
        case 'l':
            if (nx == 'h') { emit(e, P_LJ, 0, 0); i++; }
            else if (!is_vowel(nx)) emit(e, P_W, 0, 0);   /* l em fim de sílaba: u */
            else emit(e, P_L, 0, 0);
            break;
        case 'm': emit(e, P_M, 0, 0); break;
        case 'n':
            if (nx == 'h') { emit(e, P_NJ, 0, 0); i++; }
            else emit(e, P_N, 0, 0);
            break;
        case 'p': emit(e, P_P, 0, 0); break;
        case 'q': emit(e, P_K, 0, 0); break;
        case 'r':
            if (nx == 'r') { emit(e, P_X, 0, 0); i++; }
            else if (i == 0 || pv == 'n' || pv == 'l' || pv == 's') emit(e, P_X, 0, 0);
            else if (!is_vowel(nx) && nx != 'h') emit(e, i + 1 >= n ? P_X : P_R, 0, 0);
            else emit(e, P_R, 0, 0);
            break;
        case 's':
            if (nx == 's') { emit(e, P_S, 0, 0); i++; }
            else if (nx == 'c' && (nx2 == 'e' || nx2 == 'i')) { emit(e, P_S, 0, 0); i++; }
            else if (i > 0 && is_vowel(pv) && is_vowel(nx)) emit(e, P_Z, 0, 0);
            else emit(e, P_S, 0, 0);
            break;
        case 't': {
            int to_i = nx == 'i' || (nx == 'e' && i + 2 >= n && stressed_at != i + 1);
            emit(e, to_i ? P_TS : P_T, 0, 0);
            break;
        }
        case 'v': emit(e, P_V, 0, 0); break;
        case 'w': emit(e, P_W, 0, 0); break;
        case 'x':
            if (i == 1 && pv == 'e' && is_vowel(nx)) emit(e, P_Z, 0, 0);   /* exame, exato */
            else emit(e, P_SH, 0, 0);
            break;
        case 'y': emit(e, P_I, 0, 0); break;
        case 'z': emit(e, i + 1 >= n ? P_S : P_Z, 0, 0); break;
        default: break;
        }
    }
}

/* ---- inglês: palavras frequentes e regras */
typedef struct { const char *w, *ph; } dict_t;
static const dict_t EN_DICT[] = {
    {"the", "D @"}, {"a", "@"}, {"of", "@ v"}, {"to", "t u"}, {"and", "ae n d"}, {"is", "I z"}, {"you", "j u"}, {"i", "a j"},
    {"it", "I t"}, {"in", "I n"}, {"that", "D ae t"}, {"was", "w V z"}, {"for", "f O R"}, {"are", "a R"}, {"with", "w I D"},
    {"he", "h i"}, {"she", "S i"}, {"we", "w i"}, {"they", "D e j"}, {"be", "b i"}, {"at", "ae t"}, {"one", "w V n"},
    {"have", "h ae v"}, {"this", "D I s"}, {"from", "f R V m"}, {"or", "O R"}, {"by", "b a j"}, {"but", "b V t"},
    {"what", "w V t"}, {"some", "s V m"}, {"can", "k ae n"}, {"out", "a w t"}, {"were", "w 3"}, {"all", "O l"},
    {"there", "D E R"}, {"their", "D E R"}, {"when", "w E n"}, {"your", "j O R"}, {"how", "h a w"}, {"said", "s E d"},
    {"do", "d u"}, {"does", "d V z"}, {"time", "t a j m"}, {"if", "I f"}, {"will", "w I l"}, {"about", "@ b a w t"},
    {"then", "D E n"}, {"them", "D E m"}, {"would", "w U d"}, {"could", "k U d"}, {"should", "S U d"}, {"so", "s o w"},
    {"no", "n o w"}, {"not", "n O t"}, {"my", "m a j"}, {"people", "p i p @ l"}, {"know", "n o w"}, {"water", "w O t 3"},
    {"who", "h u"}, {"been", "b I n"}, {"now", "n a w"}, {"hello", "h @ l o w"}, {"world", "w 3 l d"}, {"one", "w V n"},
    {"two", "t u"}, {"eight", "e j t"}, {"computer", "k @ m p j u t 3"}, {"please", "p l i z"}, {"thank", "T ae N k"},
    {"thanks", "T ae N k s"}, {"yes", "j E s"}, {"okay", "o w k e j"}, {"ok", "o w k e j"}, {"done", "d V n"},
    {"error", "E R 3"}, {"file", "f a j l"}, {"open", "o w p @ n"}, {"new", "n u"}, {"save", "s e j v"}, {"word", "w 3 d"},
    {"words", "w 3 d z"}, {"mac", "m ae k"}, {"apple", "ae p @ l"}, {"welcome", "w E l k @ m"}, {"good", "g U d"},
    {"day", "d e j"}, {"night", "n a j t"}, {"come", "k V m"}, {"some", "s V m"}, {"other", "V D 3"}, {"any", "E n i"},
    {"many", "m E n i"}, {"there's", "D E R z"}, {"it's", "I t s"}, {"don't", "d o w n t"}, {"can't", "k ae n t"},
};
static const int SYM_PH[][2] = {{0}};
static int sym_to_ph(const char *s, int len) {
    for (int i = 0; i < P_COUNT; i++) {
        const char *t = PH[i].sym;
        int k = 0;
        while (t[k] && k < len && t[k] == s[k]) k++;
        if (!t[k] && k == len) return i;
    }
    return -1;
}
static int en_dict(const u32 *w, int n, emitter *e) {
    for (unsigned d = 0; d < sizeof EN_DICT / sizeof EN_DICT[0]; d++) {
        const char *s = EN_DICT[d].w;
        int k = 0;
        while (s[k] && k < n && (u32)s[k] == w[k]) k++;
        if (s[k] || k != n) continue;
        const char *p = EN_DICT[d].ph;
        static const char *fn[] = {"the", "a", "of", "to", "and", "is", "it", "in", "that", "was", "for", "are", "with", "at", "or", "by",
                                   "but", "from", "be", "he", "she", "we", "you", "they", "them", "their", "your", "my", "can", "will", "do", 0};
        int first_vowel = 1;
        for (int f = 0; fn[f]; f++) if (word_is(w, n, fn[f])) first_vowel = 0;   /* palavra funcional: átona */
        while (*p) {
            while (*p == ' ') p++;
            int len = 0;
            while (p[len] && p[len] != ' ') len++;
            int ph = sym_to_ph(p, len);
            if (ph >= 0) {
                int stress = PH[ph].type == T_VOWEL && first_vowel;
                if (PH[ph].type == T_VOWEL) first_vowel = 0;
                emit(e, ph, stress, 0);
            }
            p += len;
        }
        return 1;
    }
    return 0;
}
static int en_is_v(u32 c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y'; }
static void g2p_en(const u32 *w, int n, emitter *e) {
    if (en_dict(w, n, e)) return;
    int first_vowel = 1;
    /* "e" final mudo depois de consoante deixa a vogal anterior longa (make, time, home, cute) */
    int magic = n >= 3 && w[n - 1] == 'e' && !en_is_v(w[n - 2]) && en_is_v(w[n - 3]);
    for (int i = 0; i < n; i++) {
        u32 c = w[i], nx = i + 1 < n ? w[i + 1] : 0, nx2 = i + 2 < n ? w[i + 2] : 0, pv = i ? w[i - 1] : 0;
        if (en_is_v(c) && !(c == 'y' && i == 0)) {
            int stress = first_vowel;
            first_vowel = 0;
            if (magic && i == n - 1) break;
            if (c == 'e' && nx == 'e') { emit(e, P_I, stress, 0); i++; continue; }
            if (c == 'e' && nx == 'a') { emit(e, P_I, stress, 0); i++; continue; }
            if (c == 'o' && nx == 'o') { emit(e, P_U, stress, 0); i++; continue; }
            if ((c == 'o' && nx == 'u') || (c == 'o' && nx == 'w')) { emit(e, P_A, stress, 0); emit(e, P_W, 0, 0); i++; continue; }
            if ((c == 'o' && nx == 'i') || (c == 'o' && nx == 'y')) { emit(e, P_OH, stress, 0); emit(e, P_J, 0, 0); i++; continue; }
            if ((c == 'a' && nx == 'i') || (c == 'a' && nx == 'y')) { emit(e, P_E, stress, 0); emit(e, P_J, 0, 0); i++; continue; }
            if (c == 'a' && (nx == 'u' || nx == 'w')) { emit(e, P_OH, stress, 0); i++; continue; }
            if (c == 'i' && nx == 'g' && nx2 == 'h') { emit(e, P_A, stress, 0); emit(e, P_J, 0, 0); i += 2; continue; }
            if (c == 'e' && nx == 'r' && i + 2 >= n) { emit(e, P_ER, stress && n <= 3, 0); i++; continue; }
            if ((c == 'a' || c == 'o' || c == 'u' || c == 'i') && nx == 'r' && !en_is_v(nx2)) {
                emit(e, c == 'a' ? P_A : c == 'o' ? P_OH : P_ER, stress, 0); emit(e, P_RR, 0, 0); i++; continue;
            }
            if (c == 'y') { emit(e, i == n - 1 ? (n <= 3 ? P_A : P_I) : P_IH, stress, 0); if (i == n - 1 && n <= 3) emit(e, P_J, 0, 0); continue; }
            int is_long = magic && i == n - 3;
            switch (c) {
            case 'a': if (is_long) { emit(e, P_E, stress, 0); emit(e, P_J, 0, 0); } else emit(e, stress ? P_AE : P_AH, stress, 0); break;
            case 'e': emit(e, is_long ? P_I : stress ? P_EH : P_IH, stress, 0); break;
            case 'i': if (is_long) { emit(e, P_A, stress, 0); emit(e, P_J, 0, 0); } else emit(e, P_IH, stress, 0); break;
            case 'o': if (is_long) { emit(e, P_O, stress, 0); emit(e, P_W, 0, 0); } else emit(e, stress ? P_A : P_AH, stress, 0); break;
            default: if (is_long) { emit(e, P_J, 0, 0); emit(e, P_U, stress, 0); } else emit(e, stress ? P_VV : P_AH, stress, 0); break;
            }
            continue;
        }
        switch (c) {
        case 't': if (nx == 'h') { emit(e, P_TH, 0, 0); i++; } else if (nx == 'i' && nx2 == 'o') { emit(e, P_SH, 0, 0); i++; } else emit(e, P_T, 0, 0); break;
        case 's': if (nx == 'h') { emit(e, P_SH, 0, 0); i++; } else emit(e, (pv && en_is_v(pv) && nx && en_is_v(nx)) || (i == n - 1 && pv && (en_is_v(pv) || pv == 'g' || pv == 'd' || pv == 'n' || pv == 'l')) ? P_Z : P_S, 0, 0); break;
        case 'c': if (nx == 'h') { emit(e, P_TS, 0, 0); i++; } else if (nx == 'k') { emit(e, P_K, 0, 0); i++; } else emit(e, (nx == 'e' || nx == 'i' || nx == 'y') ? P_S : P_K, 0, 0); break;
        case 'p': if (nx == 'h') { emit(e, P_F, 0, 0); i++; } else emit(e, P_P, 0, 0); break;
        case 'w': if (nx == 'h') { emit(e, P_W, 0, 0); i++; } else if (nx == 'r') { } else emit(e, P_W, 0, 0); break;
        case 'n': if (nx == 'g') { emit(e, P_NG, 0, 0); i++; } else if (nx == 'k') emit(e, P_NG, 0, 0); else emit(e, P_N, 0, 0); break;
        case 'q': emit(e, P_K, 0, 0); if (nx == 'u') { emit(e, P_W, 0, 0); i++; } break;
        case 'g': if (nx == 'h') { i++; } else emit(e, (nx == 'e' || nx == 'i' || nx == 'y') && i > 0 ? P_DZ : P_G, 0, 0); break;
        case 'k': if (!(i == 0 && nx == 'n')) emit(e, P_K, 0, 0); break;
        case 'x': emit(e, P_K, 0, 0); emit(e, P_S, 0, 0); break;
        case 'j': emit(e, P_DZ, 0, 0); break;
        case 'r': emit(e, P_RR, 0, 0); break;
        case 'l': if (nx != 'l') emit(e, P_L, 0, 0); break;
        case 'h': emit(e, P_H, 0, 0); break;
        case 'y': emit(e, P_J, 0, 0); break;
        case 'b': if (nx != 'b') emit(e, P_B, 0, 0); break;
        case 'd': if (nx != 'd') emit(e, P_D, 0, 0); break;
        case 'f': if (nx != 'f') emit(e, P_F, 0, 0); break;
        case 'm': if (nx != 'm') emit(e, P_M, 0, 0); break;
        case 'v': emit(e, P_V, 0, 0); break;
        case 'z': emit(e, P_Z, 0, 0); break;
        default: break;
        }
    }
}

int sp_guess_lang(const char *text) {
    int pt = 0, en = 0;
    const unsigned char *p = (const unsigned char *)text;
    for (int i = 0; p[i]; i++) {
        if (p[i] == 0xC3) pt += 2;   /* letras acentuadas */
        if ((p[i] == ' ' || i == 0) && p[i + 1]) {
            const char *w = (const char *)p + (i ? i + 1 : 0);
            static const char *ptw[] = {"de ", "que ", "não ", "uma ", "para ", "com ", "os ", "as ", "do ", "da ", "em "};
            static const char *enw[] = {"the ", "and ", "of ", "to ", "is ", "you ", "that ", "with ", "for ", "this "};
            for (unsigned k = 0; k < sizeof ptw / sizeof ptw[0]; k++) { const char *a = ptw[k]; int j = 0; while (a[j] && w[j] == a[j]) j++; if (!a[j]) pt++; }
            for (unsigned k = 0; k < sizeof enw / sizeof enw[0]; k++) { const char *a = enw[k]; int j = 0; while (a[j] && w[j] == a[j]) j++; if (!a[j]) en++; }
        }
    }
    return en > pt ? SP_LANG_EN : SP_LANG_PT;
}

static void word_to_ph(const u32 *w, int n, int lang, emitter *e) { if (lang == SP_LANG_EN) g2p_en(w, n, e); else g2p_pt(w, n, e); }
static volatile int g_text_lock;   /* os buffers de decodificação são de uso exclusivo */
static int text_to_phonemes(const char *text, int lang, sp_ph *out, int cap);
int sp_text_to_phonemes(const char *text, int lang, sp_ph *out, int cap) {
    while (__sync_lock_test_and_set(&g_text_lock, 1)) { }
    int n = text_to_phonemes(text, lang, out, cap);
    __sync_lock_release(&g_text_lock);
    return n;
}
static int text_to_phonemes(const char *text, int lang, sp_ph *out, int cap) {
    static u32 cp[16384];
    static int at[16384];
    int n = decode(text, cp, at, 16384);
    emitter e = {out, 0, cap, 0, 0};
    emit_pause(&e, 1);
    for (int i = 0; i < n;) {
        u32 c = cp[i];
        if (is_digit(c)) {
            long v = 0;
            int j = i;
            while (j < n && is_digit(cp[j]) && v < 100000000000L) v = v * 10 + (long)(cp[j++] - '0');
            char words[512];
            number_words(v, lang, words, sizeof words);
            u32 wc[512];
            int wa[512], wn = decode(words, wc, wa, 512);
            e.loc = at[i]; e.len = (j < n ? at[j] : at[n - 1] + 1) - at[i];
            for (int k = 0; k < wn;) {
                while (k < wn && !is_letter(wc[k])) k++;
                int s = k;
                u32 lw[64]; int ln = 0;
                while (k < wn && is_letter(wc[k]) && ln < 64) lw[ln++] = lower(wc[k++]);
                if (ln) word_to_ph(lw, ln, lang, &e);
                (void)s;
            }
            i = j;
            continue;
        }
        if (is_letter(c)) {
            int j = i;
            u32 lw[64]; int ln = 0;
            while (j < n && is_letter(cp[j])) { if (cp[j] != '\'' && ln < 64) lw[ln++] = lower(cp[j]); j++; }
            e.loc = at[i]; e.len = (j < n ? at[j] : at[n - 1] + (cp[n - 1] > 0x7FF ? 3 : cp[n - 1] > 0x7F ? 2 : 1)) - at[i];
            word_to_ph(lw, ln, lang, &e);
            i = j;
            continue;
        }
        e.loc = at[i]; e.len = 0;
        if (c == ',' || c == ';' || c == ':' || c == '(' || c == ')') emit_pause(&e, 2);
        else if (c == '.' || c == '!' || c == '\n') emit_pause(&e, 3);
        else if (c == '?') emit_pause(&e, 4);   /* pergunta: a frase sobe no fim */
        i++;
    }
    emit_pause(&e, 1);
    return e.n;
}
int sp_phonemes_string(const sp_ph *ph, int n, char *out, int cap) {
    int k = 0;
    for (int i = 0; i < n && k < cap - 4; i++) {
        const char *s = PH[ph[i].ph].sym;
        if (ph[i].stress && k < cap - 2) out[k++] = '1';
        for (int j = 0; s[j] && k < cap - 2; j++) out[k++] = s[j];
        if (ph[i].nasal && k < cap - 2) out[k++] = '~';
        if (k < cap - 1) out[k++] = ' ';
    }
    if (k && out[k - 1] == ' ') k--;
    out[k] = 0;
    return k;
}

/* ================================================================ síntese */
static double sqrt_(double v) { return __builtin_sqrt(v); }
static double exp_(double v) { return __builtin_exp(v); }
static double cos_(double v) { return __builtin_cos(v); }
static int pause_ms(int kind) { return kind == 1 ? 60 : kind == 2 ? 180 : kind == 3 ? 380 : kind == 4 ? 380 : 120; }
int sp_ph_samples(const sp_synth *s, int i) {
    const sp_ph *p = &s->ph[i];
    double ms = p->ph == P_SIL ? pause_ms(p->pause) : PH[p->ph].ms;
    if (PH[p->ph].type == T_VOWEL) ms *= p->stress ? 1.35 : 0.85;
    if (p->nasal && PH[p->ph].type == T_VOWEL) ms *= 1.1;
    double r = s->rate > 0.1 ? s->rate : 0.1;
    return (int)(ms / r * s->rate_hz / 1000.0);
}
void sp_synth_init(sp_synth *s, const sp_ph *ph, int n, const sp_voice *v, double rate, double rate_hz) {
    unsigned char *b = (unsigned char *)s;
    for (unsigned i = 0; i < sizeof *s; i++) b[i] = 0;
    s->ph = ph; s->n = n; s->v = *v; s->rate = rate > 0 ? rate : 1; s->rate_hz = rate_hz > 0 ? rate_hz : 48000;
    s->noise = 0x12345678u;
    s->mod = 0.35;
    for (int i = 0; i < 5; i++) { s->f[i] = 500.0 * (2 * i + 1); s->bw[i] = 80 + 40 * i; }
    s->cur = 0;
    s->sample_in_ph = 0;
    s->ph_len = n ? sp_ph_samples(s, 0) : 0;
    s->total_samples = 0;
    for (int i = 0; i < n; i++) s->total_samples += sp_ph_samples(s, i);
}
static double frand(sp_synth *s) { s->noise = s->noise * 1664525u + 1013904223u; return (double)(s->noise >> 8) / 8388608.0 - 1.0; }
static double reson(sp_synth *s, int k, double x, double f, double bw) {
    double fs = s->rate_hz;
    if (f >= fs * 0.45) f = fs * 0.45;
    double c = -exp_(-2 * PI * bw / fs), b = 2 * exp_(-PI * bw / fs) * cos_(2 * PI * f / fs), a = 1 - b - c;
    double y = a * x + b * s->y1[k] + c * s->y2[k];
    s->y2[k] = s->y1[k];
    s->y1[k] = y;
    return y;
}
/* alvo de formantes do fonema (a voz muda a escala) */
static void targets(const sp_synth *s, int i, double f[3]) {
    const ph_info *p = &PH[s->ph[i].ph];
    double k = s->v.formant_scale > 0 ? s->v.formant_scale : 1;
    if (p->type == T_PAUSE || p->type == T_STOP || p->type == T_FRIC || p->type == T_AFFR || p->type == T_ASP) {
        /* consoantes sem formantes próprios: as da vogal vizinha (a fala desliza entre elas) */
        int j = i + 1 < s->n && PH[s->ph[i + 1].ph].type == T_VOWEL ? i + 1 : i > 0 && PH[s->ph[i - 1].ph].type == T_VOWEL ? i - 1 : -1;
        const ph_info *q = j >= 0 ? &PH[s->ph[j].ph] : p;
        f[0] = (p->f1 + q->f1) / 2.0 * k; f[1] = (p->f2 + q->f2) / 2.0 * k; f[2] = (p->f3 + q->f3) / 2.0 * k;
        return;
    }
    f[0] = p->f1 * k; f[1] = p->f2 * k; f[2] = p->f3 * k;
    if (s->ph[i].nasal) { f[0] = f[0] * 0.85 + 40; }
}
/* altura da voz: cai ao longo da frase, sobe nas sílabas tônicas e no fim das perguntas */
static double pitch_at(const sp_synth *s) {
    double base = s->pitch > 0 ? s->pitch : s->v.f0;
    double t = s->total_samples ? (double)s->done_samples / s->total_samples : 0;
    double f0 = base * (1.12 - 0.22 * t * s->mod / 0.35);
    const sp_ph *p = &s->ph[s->cur];
    if (p->stress) f0 *= 1 + 0.12 * s->mod / 0.35;
    /* pergunta: os últimos fonemas antes da pausa 4 sobem */
    for (int j = s->cur; j < s->n && j < s->cur + 4; j++) if (s->ph[j].ph == P_SIL && s->ph[j].pause == 4) { f0 *= 1.0 + 0.25 * (1.0 - (j - s->cur) / 4.0); break; }
    return f0;
}
/* forma de onda glotal (pulso de Rosenberg) */
static double glottal(double ph) {
    if (ph < 0.4) return 0.5 * (1 - cos_(PI * ph / 0.4));
    if (ph < 0.56) return cos_(PI * (ph - 0.4) / 0.32);
    return 0;
}
int sp_synth_run(sp_synth *s, float *out, int nsamp) {
    int k = 0;
    double fs = s->rate_hz;
    while (k < nsamp && s->cur < s->n) {
        const sp_ph *cp = &s->ph[s->cur];
        const ph_info *p = &PH[cp->ph];
        double tgt[3];
        targets(s, s->cur, tgt);
        double t = s->ph_len ? (double)s->sample_in_ph / s->ph_len : 1;
        /* volume da fonte de voz e do ruído pelo tipo e pela fase dentro do fonema */
        double voice = 0, noise = 0, nf = p->nf, nbw = p->nbw > 0 ? p->nbw : 1000;
        switch (p->type) {
        case T_PAUSE: voice = 0; break;
        case T_VOWEL: voice = cp->stress ? 1.0 : 0.8; break;
        case T_NASAL: voice = 0.45; break;
        case T_LIQUID: voice = p->ms < 40 ? (t > 0.3 && t < 0.7 ? 0.35 : 0.7) : 0.65; break;   /* tepe: interrupção breve */
        case T_GLIDE: voice = 0.7; break;
        case T_FRIC: voice = p->voiced ? 0.35 : 0; noise = p->namp / 100.0; break;
        case T_ASP: voice = 0; noise = p->namp / 100.0; nf = tgt[1]; nbw = 2500; break;
        case T_STOP:
            /* oclusão (silêncio ou barra de sonoridade) e a explosão no fim */
            voice = p->voiced && t < 0.75 ? 0.18 : 0;
            noise = t >= 0.75 ? p->namp / 100.0 * (1 - (t - 0.75) / 0.25) : 0;
            break;
        case T_AFFR:
            voice = p->voiced ? 0.25 : 0;
            noise = t < 0.3 ? 0 : p->namp / 100.0;
            break;
        }
        /* deslize suave dos formantes e da amplitude (10 ms) */
        double glide = 1 - exp_(-1.0 / (0.010 * fs));
        for (int i = 0; i < 3; i++) s->f[i] += (tgt[i] - s->f[i]) * glide;
        s->amp += (voice - s->amp) * glide * 2;
        s->noise_amp += (noise - s->noise_amp) * glide * 3;
        s->noise_f += (nf - s->noise_f) * glide * 3;
        /* fonte glotal derivada (radiação dos lábios) + aspiração */
        double f0 = pitch_at(s);
        s->phase += f0 / fs;
        if (s->phase >= 1) s->phase -= 1;
        double g = glottal(s->phase), dg = g - s->prev_g;
        s->prev_g = g;
        double src = dg * 6.0 * s->amp + frand(s) * (s->v.breath * 0.08) * s->amp;
        /* trato vocal: F1..F3 da fala, F4/F5 fixos */
        double y = src;
        y = reson(s, 0, y, s->f[0], 60 + s->f[0] * 0.05);
        y = reson(s, 1, y, s->f[1], 80 + s->f[1] * 0.04);
        y = reson(s, 2, y, s->f[2], 120 + s->f[2] * 0.03);
        y = reson(s, 3, y, 3500 * (s->v.formant_scale > 0 ? s->v.formant_scale : 1), 250);
        if (cp->nasal || p->type == T_NASAL) y = y * 0.75 + reson(s, 4, src, 270, 100) * 0.6;   /* murmúrio nasal */
        /* ruído de fricção/explosão filtrado */
        double nz = 0;
        if (s->noise_amp > 0.001 && s->noise_f > 0) nz = reson(s, 5, frand(s), s->noise_f, nbw) * s->noise_amp * 2.2;
        else if (s->noise_amp > 0.001) nz = frand(s) * s->noise_amp * 0.3;
        double v = y * 0.9 + nz;
        /* passa-baixa leve para tirar o chiado do pulso */
        s->lp += (v - s->lp) * 0.6;
        double o = s->lp;
        if (o > 1) o = 1;
        if (o < -1) o = -1;
        out[k++] = (float)o;
        s->done_samples++;
        if (++s->sample_in_ph >= s->ph_len) {
            s->cur++;
            s->sample_in_ph = 0;
            s->ph_len = s->cur < s->n ? sp_ph_samples(s, s->cur) : 0;
        }
    }
    (void)sqrt_;
    (void)SYM_PH;
    return k;
}
