/* incluído por appkit_ime.c — composição, candidatos e ligação com o NSTextInputClient */

static int memcmp_(const void *a, const void *b, size_t n) {
    const u8 *x = a, *y = b;
    for (size_t i = 0; i < n; i++) if (x[i] != y[i]) return x[i] - y[i];
    return 0;
}
static void memmove_(void *d, const void *s, size_t n) {
    u8 *dd = d;
    const u8 *ss = s;
    if (dd < ss) for (size_t i = 0; i < n; i++) dd[i] = ss[i];
    else for (size_t i = n; i-- > 0;) dd[i] = ss[i];
}
static const char *strchr_(const char *s, char c) { for (; *s; s++) if (*s == c) return s; return 0; }

enum { SRC_KEYBOARD, SRC_JA, SRC_ZH, SRC_KO, SRC_COUNT };
static const char *src_ids[SRC_COUNT] = {
    "com.apple.keylayout.Brazilian-ABNT2", "com.apple.inputmethod.Kotoeri.RomajiTyping.Japanese",
    "com.apple.inputmethod.SCIM.ITABC", "com.apple.inputmethod.Korean.2SetKorean",
};

/* ================================================================ dicionários em arquivo */
typedef struct { const char *text; u64 len; } dict_t;
static dict_t ja_file, zh_file, ja_mem, zh_mem;
static BOOL dicts_ready;
static void load_dict(dict_t *d, const char *name) {
    static const char *dirs[] = {"/System/Library/Input Methods/mOS/", "/Library/Input Methods/mOS/"};
    for (int i = 0; i < 2 && !d->text; i++) {
        char p[160];
        p[0] = 0;
        cat(p, sizeof p, dirs[i]);
        cat(p, sizeof p, name);
        u64 n = 0;
        u8 *b = cx_read_file(p, &n);
        if (b && n) { d->text = (const char *)b; d->len = n; }
        else free(b);
    }
}
static void dicts_init(void) {
    if (dicts_ready) return;
    dicts_ready = YES;
    load_dict(&ja_file, "ja.dict");
    load_dict(&zh_file, "zh.dict");
    ja_mem.text = ja_builtin; ja_mem.len = sizeof ja_builtin - 1;
    zh_mem.text = zh_phrases; zh_mem.len = sizeof zh_phrases - 1;
}
/* candidatos de key: "key<TAB>c1 c2" ou SKK "key /c1/c2;nota/". Devolve quantos acrescentou em out. */
#define CMAX 48
#define CLEN 96
static int dict_lookup(const dict_t *d, const char *key, char out[][CLEN], int have, int max) {
    if (!d->text) return 0;
    size_t kl = strlen(key);
    const char *p = d->text, *end = d->text + d->len;
    int added = 0;
    while (p < end) {
        const char *nl = p;
        while (nl < end && *nl != '\n') nl++;
        if ((size_t)(nl - p) > kl && !memcmp_(p, key, kl) && (p[kl] == '\t' || (p[kl] == ' ' && kl + 1 < (size_t)(nl - p) && p[kl + 1] == '/'))) {
            BOOL skk = p[kl] == ' ';
            const char *q = p + kl + (skk ? 2 : 1);
            while (q < nl && have + added < max) {
                const char *e = q;
                while (e < nl && *e != (skk ? '/' : ' ') && *e != '\r') e++;
                const char *ce = e;
                if (skk) { const char *sc = q; while (sc < e && *sc != ';') sc++; ce = sc; }   /* tira a anotação */
                size_t cl = (size_t)(ce - q);
                if (cl && cl < CLEN) {
                    BOOL dup = NO;
                    for (int k = 0; k < have + added; k++) if (!strncmp(out[k], q, cl) && !out[k][cl]) { dup = YES; break; }
                    if (!dup) { memcpy(out[have + added], q, cl); out[have + added][cl] = 0; added++; }
                }
                q = e + 1;
                if (e >= nl || *e == '\r') break;
            }
            break;
        }
        p = nl + 1;
    }
    return added;
}
static int add_cand(char out[][CLEN], int have, const char *s) {
    if (!s || !*s || have >= CMAX || strlen(s) >= CLEN) return have;
    for (int k = 0; k < have; k++) if (!strcmp(out[k], s)) return have;
    strcpy(out[have], s);
    return have + 1;
}

/* ================================================================ estado da composição */
static struct {
    id client;
    int native;                 /* campo nativo do mOS: sem marked text, a composição vai na janelinha */
    int kind;                   /* fonte de entrada em que a composição começou */
    char raw[160];              /* letras digitadas (romaji/pinyin) ainda não convertidas */
    char done[640];             /* chinês: caracteres já escolhidos, ainda não confirmados */
    int L, V, T;                /* coreano: jamo inicial/vogal/final (-1/-1/0 = nenhum) */
    char cand[CMAX][CLEN];
    int use[CMAX];              /* chinês: bytes de raw que o candidato consome */
    int ncand, sel;
    BOOL showing;               /* japonês: lista de conversão aberta */
    int overlay_gid;
    BOOL marked;                /* o cliente está com marked text */
} st = {.L = -1, .V = -1};
static int g_src = -1;
static int source(void) {
    if (g_src < 0) { long r = SYS1(GUI_IME, IME_SOURCE_GET); g_src = r >= 0 && r < SRC_COUNT ? (int)r : 0; }
    return g_src;
}
static BOOL composing(void) { return st.raw[0] || st.done[0] || st.L >= 0 || st.V >= 0; }

/* ================================================================ japonês: romaji → kana */
static const struct { const char *r, *k; } romaji[] = {
    {"a", "あ"}, {"i", "い"}, {"u", "う"}, {"e", "え"}, {"o", "お"},
    {"ka", "か"}, {"ki", "き"}, {"ku", "く"}, {"ke", "け"}, {"ko", "こ"}, {"kya", "きゃ"}, {"kyu", "きゅ"}, {"kyo", "きょ"}, {"kye", "きぇ"},
    {"ga", "が"}, {"gi", "ぎ"}, {"gu", "ぐ"}, {"ge", "げ"}, {"go", "ご"}, {"gya", "ぎゃ"}, {"gyu", "ぎゅ"}, {"gyo", "ぎょ"},
    {"sa", "さ"}, {"si", "し"}, {"shi", "し"}, {"su", "す"}, {"se", "せ"}, {"so", "そ"}, {"sha", "しゃ"}, {"shu", "しゅ"},
    {"sho", "しょ"}, {"she", "しぇ"}, {"sya", "しゃ"}, {"syu", "しゅ"}, {"syo", "しょ"},
    {"za", "ざ"}, {"zi", "じ"}, {"zu", "ず"}, {"ze", "ぜ"}, {"zo", "ぞ"}, {"zya", "じゃ"}, {"zyu", "じゅ"}, {"zyo", "じょ"},
    {"ja", "じゃ"}, {"ji", "じ"}, {"ju", "じゅ"}, {"je", "じぇ"}, {"jo", "じょ"}, {"jya", "じゃ"}, {"jyu", "じゅ"}, {"jyo", "じょ"},
    {"ta", "た"}, {"ti", "ち"}, {"chi", "ち"}, {"tu", "つ"}, {"tsu", "つ"}, {"te", "て"}, {"to", "と"}, {"cha", "ちゃ"},
    {"chu", "ちゅ"}, {"cho", "ちょ"}, {"che", "ちぇ"}, {"tya", "ちゃ"}, {"tyu", "ちゅ"}, {"tyo", "ちょ"}, {"cya", "ちゃ"},
    {"cyu", "ちゅ"}, {"cyo", "ちょ"}, {"thi", "てぃ"}, {"thu", "てゅ"}, {"tsa", "つぁ"}, {"twu", "とぅ"},
    {"da", "だ"}, {"di", "ぢ"}, {"du", "づ"}, {"de", "で"}, {"do", "ど"}, {"dhi", "でぃ"}, {"dhu", "でゅ"}, {"dya", "ぢゃ"},
    {"dyu", "ぢゅ"}, {"dyo", "ぢょ"}, {"dwu", "どぅ"},
    {"na", "な"}, {"ni", "に"}, {"nu", "ぬ"}, {"ne", "ね"}, {"no", "の"}, {"nya", "にゃ"}, {"nyu", "にゅ"}, {"nyo", "にょ"},
    {"n'", "ん"}, {"xn", "ん"},
    {"ha", "は"}, {"hi", "ひ"}, {"hu", "ふ"}, {"fu", "ふ"}, {"he", "へ"}, {"ho", "ほ"}, {"hya", "ひゃ"}, {"hyu", "ひゅ"},
    {"hyo", "ひょ"}, {"fa", "ふぁ"}, {"fi", "ふぃ"}, {"fe", "ふぇ"}, {"fo", "ふぉ"}, {"fyu", "ふゅ"},
    {"ba", "ば"}, {"bi", "び"}, {"bu", "ぶ"}, {"be", "べ"}, {"bo", "ぼ"}, {"bya", "びゃ"}, {"byu", "びゅ"}, {"byo", "びょ"},
    {"pa", "ぱ"}, {"pi", "ぴ"}, {"pu", "ぷ"}, {"pe", "ぺ"}, {"po", "ぽ"}, {"pya", "ぴゃ"}, {"pyu", "ぴゅ"}, {"pyo", "ぴょ"},
    {"ma", "ま"}, {"mi", "み"}, {"mu", "む"}, {"me", "め"}, {"mo", "も"}, {"mya", "みゃ"}, {"myu", "みゅ"}, {"myo", "みょ"},
    {"ya", "や"}, {"yu", "ゆ"}, {"yo", "よ"}, {"ye", "いぇ"},
    {"ra", "ら"}, {"ri", "り"}, {"ru", "る"}, {"re", "れ"}, {"ro", "ろ"}, {"rya", "りゃ"}, {"ryu", "りゅ"}, {"ryo", "りょ"},
    {"wa", "わ"}, {"wi", "うぃ"}, {"we", "うぇ"}, {"wo", "を"}, {"wyi", "ゐ"}, {"wye", "ゑ"},
    {"va", "ゔぁ"}, {"vi", "ゔぃ"}, {"vu", "ゔ"}, {"ve", "ゔぇ"}, {"vo", "ゔぉ"},
    {"xa", "ぁ"}, {"xi", "ぃ"}, {"xu", "ぅ"}, {"xe", "ぇ"}, {"xo", "ぉ"}, {"la", "ぁ"}, {"li", "ぃ"}, {"lu", "ぅ"},
    {"le", "ぇ"}, {"lo", "ぉ"}, {"xya", "ゃ"}, {"xyu", "ゅ"}, {"xyo", "ょ"}, {"lya", "ゃ"}, {"lyu", "ゅ"}, {"lyo", "ょ"},
    {"xtu", "っ"}, {"ltu", "っ"}, {"xtsu", "っ"}, {"ltsu", "っ"}, {"xwa", "ゎ"}, {"lwa", "ゎ"}, {"xka", "ゕ"}, {"xke", "ゖ"},
    {"-", "ー"}, {",", "、"}, {".", "。"}, {"[", "「"}, {"]", "」"}, {"?", "？"}, {"!", "！"}, {"~", "〜"}, {"/", "・"},
};
static BOOL is_vowel(char c) { return c == 'a' || c == 'i' || c == 'u' || c == 'e' || c == 'o'; }
static BOOL is_alpha(char c) { return c >= 'a' && c <= 'z'; }
/* final: confirmar ("n" sozinho no fim vira ん) */
static void to_hiragana(const char *raw, char *out, size_t cap, BOOL final) {
    out[0] = 0;
    size_t i = 0, n = strlen(raw);
    while (i < n) {
        char c = raw[i], d = raw[i + 1];
        if (is_alpha(c) && c != 'n' && !is_vowel(c) && d == c) { cat(out, cap, "っ"); i++; continue; }   /* kk, tt, ss... */
        if (c == 'n' && d == 'n') {
            cat(out, cap, "ん");
            i += (is_vowel(raw[i + 2]) || raw[i + 2] == 'y') ? 1 : 2;     /* "konnichi": ん + に */
            continue;
        }
        if (c == 'n' && d && d != '\'' && !is_vowel(d) && d != 'y') { cat(out, cap, "ん"); i++; continue; }
        int best = -1;
        size_t bl = 0;
        for (size_t k = 0; k < sizeof romaji / sizeof *romaji; k++) {
            size_t l = strlen(romaji[k].r);
            if (l > bl && l <= n - i && !strncmp(raw + i, romaji[k].r, l)) { best = (int)k; bl = l; }
        }
        if (best >= 0) { cat(out, cap, romaji[best].k); i += bl; continue; }
        if (c == 'n' && !d && final) { cat(out, cap, "ん"); i++; continue; }
        char one[2] = {c, 0};
        cat(out, cap, one);
        i++;
    }
}
static void to_katakana(const char *hira, char *out, size_t cap) {
    out[0] = 0;
    const char *p = hira;
    while (*p) {
        u32 cp = u8dec(&p);
        if (cp >= 0x3041 && cp <= 0x3096) cp += 0x60;
        else if (cp == 0x309D || cp == 0x309E) cp += 0x60;
        char b[4];
        int l = u8enc(cp, b);
        b[l] = 0;
        cat(out, cap, b);
    }
}
static int ja_lookup(const char *key, char out[][CLEN], int have) {
    have += dict_lookup(&ja_file, key, out, have, CMAX);
    have += dict_lookup(&ja_mem, key, out, have, CMAX);
    return have;
}
/* conversão gulosa: em cada posição, a leitura mais longa (≥ 2 kana) do dicionário; o resto (partículas,
 * okurigana) fica em kana. alt_first recebe as alternativas do primeiro trecho convertido. */
static void ja_greedy(const char *reading, char *out, size_t cap, char alt[][CLEN], int *nalt, int *first_len) {
    out[0] = 0;
    if (nalt) *nalt = 0;
    if (first_len) *first_len = 0;
    const char *p = reading;
    BOOL first = YES;
    while (*p) {
        /* posições dos próximos caracteres */
        const char *ends[40];
        int ne = 0;
        const char *q = p;
        while (*q && ne < 40) { u8dec(&q); ends[ne++] = q; }
        int hit = -1;
        char tmp[CMAX][CLEN];
        for (int k = ne; k >= 2; k--) {
            char key[CLEN];
            size_t kl = (size_t)(ends[k - 1] - p);
            if (kl >= CLEN) continue;
            memcpy(key, p, kl); key[kl] = 0;
            int n = ja_lookup(key, tmp, 0);
            if (n) {
                hit = k;
                cat(out, cap, tmp[0]);
                if (first && nalt) { for (int a = 1; a < n && *nalt < 12; a++) strcpy(alt[(*nalt)++], tmp[a]); if (first_len) *first_len = (int)kl; }
                first = NO;
                p = ends[k - 1];
                break;
            }
        }
        if (hit < 0) {
            char b[8];
            size_t l = (size_t)(ends[0] - p);
            memcpy(b, p, l); b[l] = 0;
            cat(out, cap, b);
            p = ends[0];
        }
    }
}
static void ja_build_candidates(void) {
    char reading[480], kata[480], greedy[640], alt[12][CLEN];
    to_hiragana(st.raw, reading, sizeof reading, YES);
    to_katakana(reading, kata, sizeof kata);
    int nalt = 0, flen = 0, n = 0;
    n = ja_lookup(reading, st.cand, 0);                     /* a leitura inteira é uma palavra */
    ja_greedy(reading, greedy, sizeof greedy, alt, &nalt, &flen);
    n = add_cand(st.cand, n, greedy);
    for (int a = 0; a < nalt; a++) {                         /* outras escolhas para o primeiro trecho */
        char s[CLEN * 2];
        s[0] = 0;
        cat(s, sizeof s, alt[a]);
        char rest[640];
        ja_greedy(reading + flen, rest, sizeof rest, 0, 0, 0);
        cat(s, sizeof s, rest);
        n = add_cand(st.cand, n, s);
    }
    n = add_cand(st.cand, n, reading);
    n = add_cand(st.cand, n, kata);
    st.ncand = n;
    st.sel = 0;
}

/* ================================================================ chinês: pinyin */
typedef struct { char py[8]; const char *chars; int clen; } syl_t;
static syl_t syls[440];
static int nsyl;
static void syl_init(void) {
    if (nsyl) return;
    const char *p = zh_syllables;
    while (*p && nsyl < (int)(sizeof syls / sizeof *syls)) {
        const char *sp = p;
        while (*sp && *sp != ' ') sp++;
        const char *nl = sp;
        while (*nl && *nl != '\n') nl++;
        size_t pl = (size_t)(sp - p);
        if (pl && pl < sizeof syls[0].py && *sp == ' ') {
            memcpy(syls[nsyl].py, p, pl);
            syls[nsyl].py[pl] = 0;
            syls[nsyl].chars = sp + 1;
            syls[nsyl].clen = (int)(nl - sp - 1);
            nsyl++;
        }
        p = *nl ? nl + 1 : nl;
    }
}
static int syl_find(const char *s, size_t l) {
    for (int i = 0; i < nsyl; i++) if (strlen(syls[i].py) == l && !strncmp(syls[i].py, s, l)) return i;
    return -1;
}
/* sílabas de raw: começo e tamanho; *partial = bytes no fim que não formam sílaba (prefixo, ex.: "zh") */
static int zh_segment(const char *raw, int *starts, int *lens, int max, int *partial) {
    int n = 0;
    size_t i = 0, len = strlen(raw);
    *partial = 0;
    while (i < len && n < max) {
        if (raw[i] == '\'') { i++; continue; }
        int best = 0;
        for (int l = 6; l >= 1; l--) {
            if (i + (size_t)l > len) continue;
            BOOL ok = YES;
            for (int k = 0; k < l; k++) if (raw[i + k] == '\'') ok = NO;
            if (ok && syl_find(raw + i, (size_t)l) >= 0) { best = l; break; }
        }
        if (!best) { *partial = (int)(len - i); break; }
        starts[n] = (int)i; lens[n] = best; n++;
        i += (size_t)best;
    }
    return n;
}
static void syl_chars(int si, char out[][CLEN], int *n, int *use, int u, int limit) {
    const char *p = syls[si].chars, *e = p + syls[si].clen;
    int got = 0;
    while (p < e && *n < CMAX && got < limit) {
        const char *q = p;
        u8dec(&q);
        char b[8];
        size_t l = (size_t)(q - p);
        memcpy(b, p, l); b[l] = 0;
        int before = *n;
        *n = add_cand(out, *n, b);
        if (*n > before) { use[*n - 1] = u; got++; }
        p = q;
    }
}
static void zh_build_candidates(void) {
    syl_init();
    int starts[32], lens[32], partial;
    int ns = zh_segment(st.raw, starts, lens, 32, &partial);
    int n = 0;
    char key[160];
    /* palavras: da mais longa para a mais curta, a partir do começo */
    for (int k = ns; k >= 1; k--) {
        int end = starts[k - 1] + lens[k - 1], kl = 0;
        for (int i = 0; i < end && kl < (int)sizeof key - 1; i++) if (st.raw[i] != '\'') key[kl++] = st.raw[i];
        key[kl] = 0;
        int before = n;
        if (k >= 2 || zh_file.text) n += dict_lookup(&zh_file, key, st.cand, n, CMAX);
        if (k >= 2) n += dict_lookup(&zh_mem, key, st.cand, n, CMAX);
        for (int i = before; i < n; i++) st.use[i] = end;
    }
    if (ns) {
        int si = syl_find(st.raw + starts[0], (size_t)lens[0]);
        if (si >= 0) syl_chars(si, st.cand, &n, st.use, starts[0] + lens[0], 40);
    } else if (partial) {
        /* sílaba incompleta ("zh", "x"): os primeiros caracteres das sílabas que começam assim */
        size_t pl = 0;
        while (st.raw[pl] && st.raw[pl] != '\'') pl++;
        for (int i = 0; i < nsyl && n < 27; i++)
            if (strlen(syls[i].py) >= pl && !strncmp(syls[i].py, st.raw, pl)) syl_chars(i, st.cand, &n, st.use, (int)pl, 2);
    }
    st.ncand = n;
    if (st.sel >= n) st.sel = 0;
}
/* o que aparece sublinhado: escolhidos + pinyin separado por sílaba */
static void zh_preedit(char *out, size_t cap) {
    out[0] = 0;
    cat(out, cap, st.done);
    int starts[32], lens[32], partial;
    int ns = zh_segment(st.raw, starts, lens, 32, &partial);
    for (int i = 0; i < ns; i++) {
        char b[8];
        memcpy(b, st.raw + starts[i], (size_t)lens[i]); b[lens[i]] = 0;
        if (i) cat(out, cap, " ");
        cat(out, cap, b);
    }
    if (partial) { if (ns) cat(out, cap, " "); cat(out, cap, st.raw + strlen(st.raw) - partial); }
}
static const char *zh_punct(char c) {
    switch (c) {
    case ',': return "，"; case '.': return "。"; case '?': return "？"; case '!': return "！";
    case ':': return "："; case ';': return "；"; case '\\': return "、"; case '(': return "（"; case ')': return "）";
    case '[': return "【"; case ']': return "】"; case '<': return "《"; case '>': return "》"; case '~': return "～";
    case '"': return "“"; case '$': return "￥";
    }
    return 0;
}

/* ================================================================ coreano: hangul 2-set */
static const char ko_cons_keys[] = "rRseEfaqQtTdwWczxvg";            /* índice = choseong */
static const struct { char k; int v; } ko_vowel_keys[] = {
    {'k', 0}, {'o', 1}, {'i', 2}, {'O', 3}, {'j', 4}, {'p', 5}, {'u', 6}, {'P', 7}, {'h', 8}, {'y', 12},
    {'n', 13}, {'b', 17}, {'m', 18}, {'l', 20},
};
static const int cho2jong[19] = {1, 2, 4, 7, 0, 8, 16, 17, 0, 19, 20, 21, 22, 0, 23, 24, 25, 26, 27};
static const u16 cho_compat[19] = {0x3131, 0x3132, 0x3134, 0x3137, 0x3138, 0x3139, 0x3141, 0x3142, 0x3143, 0x3145,
                                   0x3146, 0x3147, 0x3148, 0x3149, 0x314A, 0x314B, 0x314C, 0x314D, 0x314E};
/* final composta = (final, inicial que entra) */
static const struct { int a, c, r; } jong_pairs[] = {
    {1, 9, 3}, {4, 12, 5}, {4, 18, 6}, {8, 0, 9}, {8, 6, 10}, {8, 7, 11}, {8, 9, 12}, {8, 16, 13}, {8, 17, 14}, {8, 18, 15}, {17, 9, 18},
};
static const struct { int a, b, r; } vowel_pairs[] = {
    {8, 0, 9}, {8, 1, 10}, {8, 20, 11}, {13, 4, 14}, {13, 5, 15}, {13, 20, 16}, {18, 20, 19},
};
static int jong2cho(int t) {
    static const int m[28] = {-1, 0, 1, -1, 2, -1, -1, 3, 5, -1, -1, -1, -1, -1, -1, -1, 6, 7, -1, 9, 10, 11, 12, 14, 15, 16, 17, 18};
    return t >= 0 && t < 28 ? m[t] : -1;
}
static int ko_key(char c, int *is_vowel_) {
    for (int i = 0; ko_cons_keys[i]; i++) if (ko_cons_keys[i] == c) { *is_vowel_ = 0; return i; }
    for (size_t i = 0; i < sizeof ko_vowel_keys / sizeof *ko_vowel_keys; i++) if (ko_vowel_keys[i].k == c) { *is_vowel_ = 1; return ko_vowel_keys[i].v; }
    if (c >= 'A' && c <= 'Z') return ko_key((char)(c + 32), is_vowel_);   /* maiúscula sem forma dupla = minúscula */
    return -1;
}
static void ko_syllable(char *out) {
    u32 cp = 0;
    if (st.L >= 0 && st.V >= 0) cp = 0xAC00 + (u32)((st.L * 21 + st.V) * 28 + st.T);
    else if (st.L >= 0) cp = cho_compat[st.L];
    else if (st.V >= 0) cp = 0x314F + (u32)st.V;
    int l = cp ? u8enc(cp, out) : 0;
    out[l] = 0;
}

/* ================================================================ cliente */
static void hide_overlay(void) {
    if (st.overlay_gid) { SYS2(GUI_IME, IME_HIDE, st.overlay_gid); st.overlay_gid = 0; }
}
static BOOL client_has_marked(id c) { return !st.native && (resp(c, "setMarkedText:selectedRange:replacementRange:") || resp(c, "setMarkedText:selectedRange:")); }
static void set_marked(const char *utf8) {
    id c = st.client;
    if (!client_has_marked(c)) return;
    id s = cx_str(utf8);
    u64 n = MSG0(u64, s, "length");
    rng_t sel = {n, 0}, rep = {NOT_FOUND, 0};
    if (resp(c, "setMarkedText:selectedRange:replacementRange:"))
        ((void (*)(id, SEL, id, rng_t, rng_t))objc_msgSend)(c, sel_registerName("setMarkedText:selectedRange:replacementRange:"), s, sel, rep);
    else ((void (*)(id, SEL, id, rng_t))objc_msgSend)(c, sel_registerName("setMarkedText:selectedRange:"), s, sel);
    st.marked = n > 0;
}
static void insert_into_client(const char *utf8) {
    id c = st.client;
    if (!c || !utf8 || !*utf8) return;
    if (st.native) { cx_native_insert_text(c, utf8); return; }
    id s = cx_str(utf8);
    if (resp(c, "insertText:replacementRange:"))
        ((void (*)(id, SEL, id, rng_t))objc_msgSend)(c, sel_registerName("insertText:replacementRange:"), s, (rng_t){NOT_FOUND, 0});
    else if (resp(c, "insertText:")) MSG(void, c, "insertText:", s);
}
/* onde desenhar a lista: logo abaixo do texto em composição, em pixels do conteúdo (origem em cima) */
static BOOL caret_point(id c, id win, double *x, double *y) {
    double vx, vy, vw, vh;
    if (!st.native && resp(c, "firstRectForCharacterRange:actualRange:")) {
        rng_t r = {0, 0}, actual = {0, 0};
        if (resp(c, "markedRange")) r = ((rng_t (*)(id, SEL))objc_msgSend)(c, sel_registerName("markedRange"));
        if (r.loc == NOT_FOUND && resp(c, "selectedRange")) r = ((rng_t (*)(id, SEL))objc_msgSend)(c, sel_registerName("selectedRange"));
        if (r.loc == NOT_FOUND) r.loc = 0;
        NSRect_ rc = ((NSRect_ (*)(id, SEL, rng_t, rng_t *))objc_msgSend_stret)(c, sel_registerName("firstRectForCharacterRange:actualRange:"), r, &actual);
        if (rc.x != 0 || rc.y != 0 || rc.w != 0 || rc.h != 0) {
            NSRect_ wf = cx_window_screen_frame(win);
            *x = rc.x - wf.x;
            *y = wf.h - (rc.y - wf.y);                     /* borda de baixo do texto, com origem em cima */
            return YES;
        }
    }
    if (cx_view_rect_in_window(c, &vx, &vy, &vw, &vh)) { *x = vx + 4; *y = vy + (vh > 28 ? 22 : vh); return YES; }
    return NO;
}
static void show_overlay(const char *comp, BOOL with_comp) {
    id c = st.client;
    id win = resp(c, "window") ? MSG0(id, c, "window") : 0;
    int gid = cx_window_gid(win);
    if (!gid) return;
    int page = st.sel / 9, first = page * 9, n = st.ncand - first;
    if (n > 9) n = 9;
    if (n < 0) n = 0;
    if (!n && !with_comp) { hide_overlay(); return; }
    char buf[GUI_IME_MAX];
    u32 hdr[3] = {(u32)(st.sel - first), (u32)n, with_comp ? 1u : 0u};
    memcpy(buf, hdr, 12);
    u32 o = 12;
    size_t cl = strlen(comp);
    if (cl > 1024) cl = 1024;
    memcpy(buf + o, comp, cl); o += (u32)cl; buf[o++] = 0;
    for (int i = 0; i < n; i++) {
        size_t l = strlen(st.cand[first + i]);
        if (o + l + 1 > sizeof buf) break;
        memcpy(buf + o, st.cand[first + i], l); o += (u32)l; buf[o++] = 0;
    }
    double x = 20, y = 40;
    caret_point(c, win, &x, &y);
    if (st.overlay_gid && st.overlay_gid != gid) hide_overlay();
    u64 pos = (u64)(u32)(int)x | ((u64)(u32)(int)y << 32);
    if (SYS5(GUI_IME, IME_SHOW, gid, buf, o, pos) == 0) st.overlay_gid = gid;
}
static void reset_state(void) {
    hide_overlay();
    st.raw[0] = 0; st.done[0] = 0;
    st.L = -1; st.V = -1; st.T = 0;
    st.ncand = 0; st.sel = 0; st.showing = NO;
    st.marked = NO;
}
static void update(void) {
    char comp[1024];
    BOOL list = NO;
    switch (st.kind) {
    case SRC_JA:
        if (st.showing && st.ncand) { strcpy(comp, st.cand[st.sel]); list = YES; }
        else to_hiragana(st.raw, comp, sizeof comp, NO);
        break;
    case SRC_ZH:
        zh_build_candidates();
        zh_preedit(comp, sizeof comp);
        list = st.ncand > 0;
        break;
    case SRC_KO:
        ko_syllable(comp);
        break;
    default:
        comp[0] = 0;
    }
    if (!composing()) { if (st.marked) set_marked(""); reset_state(); return; }
    set_marked(comp);
    BOOL no_marked = !client_has_marked(st.client);
    if (list || no_marked) {
        if (st.kind != SRC_ZH && !list) st.ncand = 0;
        show_overlay(comp, no_marked);
    } else hide_overlay();
}
/* texto que a composição atual confirma */
static void current_text(char *out, size_t cap) {
    out[0] = 0;
    switch (st.kind) {
    case SRC_JA:
        if (st.showing && st.ncand) cat(out, cap, st.cand[st.sel]);
        else to_hiragana(st.raw, out, cap, YES);
        break;
    case SRC_ZH:
        cat(out, cap, st.done);
        cat(out, cap, st.raw);
        break;
    case SRC_KO:
        ko_syllable(out);
        break;
    }
}
static void commit_text(const char *t) {
    if (*t) insert_into_client(t);
    else if (st.marked) set_marked("");
    reset_state();
}
static void commit_current(void) {
    char t[1024];
    current_text(t, sizeof t);
    /* pinyin confirmado com pontuação/Enter: as letras ficam como letras, sem as aspas de separação */
    if (st.kind == SRC_ZH) { char *w = t; for (char *r = t; *r; r++) if (*r != '\'') *w++ = *r; *w = 0; }
    commit_text(t);
}
static void cancel(void) {
    if (st.marked) set_marked("");
    reset_state();
}

/* ================================================================ teclas */
enum { K_RETURN = 36, K_TAB = 48, K_SPACE = 49, K_DELETE = 51, K_ESC = 53, K_LEFT = 123, K_RIGHT = 124, K_DOWN = 125, K_UP = 126 };

static void zh_pick(int i) {
    if (i < 0 || i >= st.ncand) return;
    cat(st.done, sizeof st.done, st.cand[i]);
    int u = st.use[i];
    size_t rl = strlen(st.raw);
    if (u > (int)rl) u = (int)rl;
    memmove_(st.raw, st.raw + u, rl - (size_t)u + 1);
    while (st.raw[0] == '\'') memmove_(st.raw, st.raw + 1, strlen(st.raw));
    st.sel = 0;
    if (!st.raw[0]) { char t[640]; strcpy(t, st.done); commit_text(t); return; }
    update();
}

static BOOL handle_ja(u16 code, char ch) {
    if (st.showing) {
        int n = st.ncand;
        if (code == K_SPACE || code == K_DOWN) { st.sel = (st.sel + 1) % n; update(); return YES; }
        if (code == K_UP) { st.sel = (st.sel + n - 1) % n; update(); return YES; }
        if (ch >= '1' && ch <= '9') {
            int i = st.sel / 9 * 9 + (ch - '1');
            if (i < n) { st.sel = i; commit_current(); }
            return YES;
        }
        if (code == K_RETURN) { commit_current(); return YES; }
        if (code == K_ESC || code == K_DELETE) { st.showing = NO; st.ncand = 0; update(); return YES; }
        if (code == K_LEFT || code == K_RIGHT) return YES;
        commit_current();                             /* outra tecla confirma e segue como entrada nova */
    }
    BOOL letter = ch >= 'a' && ch <= 'z', punct = ch && strchr_(composing() ? ",.-[]?!~/'" : ",.-[]?!~/", ch);
    if (!composing()) {
        if (!letter && !punct) return NO;
    } else {
        if (code == K_DELETE) { size_t l = strlen(st.raw); if (l) st.raw[l - 1] = 0; update(); return YES; }
        if (code == K_ESC) { cancel(); return YES; }
        if (code == K_RETURN) { commit_current(); return YES; }
        if (code == K_SPACE) { ja_build_candidates(); st.showing = st.ncand > 0; update(); return YES; }
        if (code == K_TAB) { commit_current(); return NO; }
        if (code >= K_LEFT && code <= K_UP) return YES;
        if (!letter && !punct && !(ch >= '0' && ch <= '9')) { commit_current(); return NO; }
    }
    size_t l = strlen(st.raw);
    if (l + 1 < sizeof st.raw) { st.raw[l] = ch; st.raw[l + 1] = 0; }
    update();
    return YES;
}

static BOOL handle_zh(u16 code, char ch) {
    BOOL letter = ch >= 'a' && ch <= 'z';
    if (!composing()) {
        if (letter) { st.raw[0] = ch; st.raw[1] = 0; st.sel = 0; update(); return YES; }
        const char *pu = zh_punct(ch);
        if (pu) { insert_into_client(pu); return YES; }
        return NO;
    }
    int n = st.ncand;
    if (letter || (ch == '\'' && st.raw[0])) {
        size_t l = strlen(st.raw);
        if (l + 1 < sizeof st.raw) { st.raw[l] = ch; st.raw[l + 1] = 0; }
        st.sel = 0;
        update();
        return YES;
    }
    if (code == K_SPACE) { if (n) zh_pick(st.sel); else commit_current(); return YES; }
    if (ch >= '1' && ch <= '9') { int i = st.sel / 9 * 9 + (ch - '1'); if (i < n) zh_pick(i); return YES; }
    if (code == K_DOWN || code == K_RIGHT) { if (n) st.sel = (st.sel + 1) % n; update(); return YES; }
    if (code == K_UP || code == K_LEFT) { if (n) st.sel = (st.sel + n - 1) % n; update(); return YES; }
    if (ch == '=' || ch == ']') { if (st.sel / 9 * 9 + 9 < n) st.sel = st.sel / 9 * 9 + 9; update(); return YES; }
    if (ch == '-' || ch == '[') { st.sel = st.sel >= 9 ? st.sel / 9 * 9 - 9 : 0; update(); return YES; }
    if (code == K_DELETE) {
        size_t l = strlen(st.raw);
        if (l) st.raw[l - 1] = 0;
        else if (st.done[0]) st.done[u8last(st.done, (int)strlen(st.done))] = 0;
        st.sel = 0;
        update();
        return YES;
    }
    if (code == K_ESC) { cancel(); return YES; }
    if (code == K_RETURN) { commit_current(); return YES; }
    const char *pu = zh_punct(ch);
    if (pu) {                                         /* pontuação: confirma o primeiro candidato e insere */
        while (st.raw[0] && st.ncand) zh_pick(0);
        if (composing()) commit_current();
        insert_into_client(pu);
        return YES;
    }
    commit_current();
    return NO;
}

static void ko_commit(void) { char t[8]; ko_syllable(t); commit_text(t); st.kind = SRC_KO; }
static BOOL handle_ko(u16 code, char ch) {
    int vow = 0, j = ch ? ko_key(ch, &vow) : -1;
    if (j < 0) {
        if (code == K_DELETE && composing()) {
            if (st.T) {
                int base = 0;
                for (size_t i = 0; i < sizeof jong_pairs / sizeof *jong_pairs; i++) if (jong_pairs[i].r == st.T) base = jong_pairs[i].a;
                st.T = base;
            } else if (st.V >= 0) {
                int base = -1;
                for (size_t i = 0; i < sizeof vowel_pairs / sizeof *vowel_pairs; i++) if (vowel_pairs[i].r == st.V) base = vowel_pairs[i].a;
                st.V = base;
            } else st.L = -1;
            update();
            return YES;
        }
        if (composing()) ko_commit();
        return NO;
    }
    if (!vow) {
        int c = j;
        if (st.V < 0) {                               /* só consoante (ou nada): começa outra sílaba */
            if (st.L >= 0) ko_commit();
            st.L = c;
        } else if (st.L < 0) {                        /* vogal sozinha */
            ko_commit();
            st.L = c;
        } else if (!st.T) {
            if (cho2jong[c]) st.T = cho2jong[c];
            else { ko_commit(); st.L = c; }
        } else {
            int comb = 0;
            for (size_t i = 0; i < sizeof jong_pairs / sizeof *jong_pairs; i++) if (jong_pairs[i].a == st.T && jong_pairs[i].c == c) comb = jong_pairs[i].r;
            if (comb) st.T = comb;
            else { ko_commit(); st.L = c; }
        }
    } else {
        int v = j;
        if (st.T) {                                   /* a final passa a ser a inicial da próxima sílaba */
            int keep = 0, next = -1;
            for (size_t i = 0; i < sizeof jong_pairs / sizeof *jong_pairs; i++) if (jong_pairs[i].r == st.T) { keep = jong_pairs[i].a; next = jong_pairs[i].c; }
            if (next < 0) { keep = 0; next = jong2cho(st.T); }
            st.T = keep;
            ko_commit();
            st.L = next; st.V = v;
        } else if (st.V >= 0) {
            int comb = -1;
            for (size_t i = 0; i < sizeof vowel_pairs / sizeof *vowel_pairs; i++) if (vowel_pairs[i].a == st.V && vowel_pairs[i].b == v) comb = vowel_pairs[i].r;
            if (comb >= 0) st.V = comb;
            else { ko_commit(); st.V = v; }
        } else st.V = v;
    }
    update();
    return YES;
}

/* ================================================================ entrada */
BOOL cx_ime_key(id client, id e, int native) {
    if (!client || !e) return NO;
    if (st.client && st.client != client && composing()) commit_current();   /* foco mudou: confirma no cliente antigo */
    int src = source();
    if (src == SRC_KEYBOARD && !composing()) return NO;
    u64 mods = MSG0(u64, e, "modifierFlags");
    if (mods & (MOD_CMD | MOD_CTRL)) { if (composing()) commit_current(); return NO; }
    st.client = client;
    st.native = native;
    if (composing() && st.kind != src) commit_current();
    st.kind = src;
    if (src == SRC_KEYBOARD) return NO;
    dicts_init();
    u16 code = MSG0(u16, e, "keyCode");
    id chars = MSG0(id, e, "characters");
    const char *cs = chars ? cx_cstr(chars) : "";
    char ch = cs && (u8)cs[0] < 0x80 ? cs[0] : 0;
    if (ch >= 'A' && ch <= 'Z' && src != SRC_KO) {
        /* maiúscula: no japonês vira minúscula (como o Kotoeri); no chinês entra como letra latina */
        if (src == SRC_JA) ch = (char)(ch + 32);
        else { if (composing()) commit_current(); return NO; }
    }
    switch (src) {
    case SRC_JA: return handle_ja(code, ch);
    case SRC_ZH: return handle_zh(code, ch);
    case SRC_KO: return handle_ko(code, ch);
    }
    return NO;
}
BOOL cx_ime_native_key(id view, id event) { return cx_ime_key(view, event, 1); }
void cx_ime_client_gone(id client) { if (st.client == client) { reset_state(); st.client = 0; } }
const char *cx_ime_source_id(void) { return src_ids[source()]; }
void cx_ime_select_source_id(const char *sid) {
    for (int i = 0; i < SRC_COUNT; i++)
        if (sid && !strcmp(sid, src_ids[i])) {
            if (composing()) commit_current();
            SYS2(GUI_IME, IME_SOURCE_SET, i);
            g_src = i;
        }
}
void cx_ime_source_changed(int s) {
    if (composing() && st.client) commit_current();
    g_src = s >= 0 && s < SRC_COUNT ? s : 0;
    cx_post(cx_note(cx_str("NSTextInputContextKeyboardSelectionDidChangeNotification"), 0, 0));
}

/* testes de unidade (harness tools/machotest.c): composição sem cliente */
int cx_ime_selftest(char *out, size_t cap) {
    char a[256], b[256];
    out[0] = 0;
    to_hiragana("konnichiha", a, sizeof a, YES); cat(out, cap, a); cat(out, cap, "|");
    to_hiragana("kitte", a, sizeof a, YES); cat(out, cap, a); cat(out, cap, "|");
    to_hiragana("shinbun", a, sizeof a, YES); to_katakana(a, b, sizeof b); cat(out, cap, b); cat(out, cap, "|");
    dicts_init();
    strcpy(st.raw, "watashihagakuseidesu"); st.kind = SRC_JA;
    ja_build_candidates(); cat(out, cap, st.ncand ? st.cand[0] : "?"); cat(out, cap, "|");
    strcpy(st.raw, "nihao"); st.kind = SRC_ZH;
    zh_build_candidates(); cat(out, cap, st.ncand ? st.cand[0] : "?"); cat(out, cap, "|");
    zh_preedit(a, sizeof a); cat(out, cap, a); cat(out, cap, "|");
    st.raw[0] = 0; st.ncand = 0;
    /* 한국어: gksrnrdj */
    const char *keys = "gksrnrdj";
    char res[64] = "";
    st.kind = SRC_KO; st.L = -1; st.V = -1; st.T = 0;
    for (const char *k = keys; *k; k++) {
        int vow = 0, j = ko_key(*k, &vow);
        /* mesma lógica do handle_ko, sem cliente: as sílabas confirmadas vão para res */
        if (!vow) {
            if (st.V < 0) { if (st.L >= 0) { ko_syllable(a); cat(res, sizeof res, a); st.L = -1; } st.L = j; }
            else if (st.L < 0) { ko_syllable(a); cat(res, sizeof res, a); st.V = -1; st.L = j; }
            else if (!st.T) { if (cho2jong[j]) st.T = cho2jong[j]; else { ko_syllable(a); cat(res, sizeof res, a); st.L = j; st.V = -1; } }
            else { ko_syllable(a); cat(res, sizeof res, a); st.L = j; st.V = -1; st.T = 0; }
        } else {
            if (st.T) { int nx = jong2cho(st.T); st.T = 0; ko_syllable(a); cat(res, sizeof res, a); st.L = nx; st.V = j; }
            else if (st.V >= 0) { ko_syllable(a); cat(res, sizeof res, a); st.L = -1; st.V = j; }
            else st.V = j;
        }
    }
    ko_syllable(a); cat(res, sizeof res, a);
    st.L = -1; st.V = -1; st.T = 0;
    cat(out, cap, res);
    return (int)strlen(out);
}
