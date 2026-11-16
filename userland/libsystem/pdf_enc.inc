/* mOS PDF — codificações de fontes simples (ISO 32000-1 anexo D) e nome de glifo → Unicode.
 * Incluído por cgpdf_draw.c. Os nomes das tabelas vêm da norma; o Unicode de cada nome sai da posição dele
 * no WinAnsi (= cp1252) ou no MacRoman, mais as formas uniXXXX / uXXXX[XX] e os nomes do Symbol. */

static const char *const enc_std[256] = {
    [32] = "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand", "quoteright", "parenleft", "parenright",
    "asterisk", "plus", "comma", "hyphen", "period", "slash", "zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
    "nine", "colon", "semicolon", "less", "equal", "greater", "question", "at", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K",
    "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "bracketleft", "backslash", "bracketright",
    "asciicircum", "underscore", "quoteleft", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r",
    "s", "t", "u", "v", "w", "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde",
    [161] = "exclamdown", "cent", "sterling", "fraction", "yen", "florin", "section", "currency", "quotesingle", "quotedblleft",
    "guillemotleft", "guilsinglleft", "guilsinglright", "fi", "fl", [177] = "endash", "dagger", "daggerdbl", "periodcentered",
    [182] = "paragraph", "bullet", "quotesinglbase", "quotedblbase", "quotedblright", "guillemotright", "ellipsis", "perthousand",
    [191] = "questiondown", [193] = "grave", "acute", "circumflex", "tilde", "macron", "breve", "dotaccent", "dieresis",
    [202] = "ring", "cedilla", [205] = "hungarumlaut", "ogonek", "caron", "emdash", [225] = "AE", [227] = "ordfeminine",
    [232] = "Lslash", "Oslash", "OE", "ordmasculine", [241] = "ae", [245] = "dotlessi", [248] = "lslash", "oslash", "oe", "germandbls"};
static const char *const enc_win[256] = {
    [32] = "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand", "quotesingle", "parenleft", "parenright",
    "asterisk", "plus", "comma", "hyphen", "period", "slash", "zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
    "nine", "colon", "semicolon", "less", "equal", "greater", "question", "at", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K",
    "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "bracketleft", "backslash", "bracketright",
    "asciicircum", "underscore", "grave", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s",
    "t", "u", "v", "w", "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde", "bullet",
    "Euro", "bullet", "quotesinglbase", "florin", "quotedblbase", "ellipsis", "dagger", "daggerdbl", "circumflex", "perthousand",
    "Scaron", "guilsinglleft", "OE", "bullet", "Zcaron", "bullet", "bullet", "quoteleft", "quoteright", "quotedblleft",
    "quotedblright", "bullet", "endash", "emdash", "tilde", "trademark", "scaron", "guilsinglright", "oe", "bullet", "zcaron",
    "Ydieresis", "space", "exclamdown", "cent", "sterling", "currency", "yen", "brokenbar", "section", "dieresis", "copyright",
    "ordfeminine", "guillemotleft", "logicalnot", "hyphen", "registered", "macron", "degree", "plusminus", "twosuperior",
    "threesuperior", "acute", "mu", "paragraph", "periodcentered", "cedilla", "onesuperior", "ordmasculine", "guillemotright",
    "onequarter", "onehalf", "threequarters", "questiondown", "Agrave", "Aacute", "Acircumflex", "Atilde", "Adieresis", "Aring",
    "AE", "Ccedilla", "Egrave", "Eacute", "Ecircumflex", "Edieresis", "Igrave", "Iacute", "Icircumflex", "Idieresis", "Eth",
    "Ntilde", "Ograve", "Oacute", "Ocircumflex", "Otilde", "Odieresis", "multiply", "Oslash", "Ugrave", "Uacute", "Ucircumflex",
    "Udieresis", "Yacute", "Thorn", "germandbls", "agrave", "aacute", "acircumflex", "atilde", "adieresis", "aring", "ae",
    "ccedilla", "egrave", "eacute", "ecircumflex", "edieresis", "igrave", "iacute", "icircumflex", "idieresis", "eth", "ntilde",
    "ograve", "oacute", "ocircumflex", "otilde", "odieresis", "divide", "oslash", "ugrave", "uacute", "ucircumflex", "udieresis",
    "yacute", "thorn", "ydieresis"};
static const char *const enc_mac[256] = {
    [32] = "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand", "quotesingle", "parenleft", "parenright",
    "asterisk", "plus", "comma", "hyphen", "period", "slash", "zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
    "nine", "colon", "semicolon", "less", "equal", "greater", "question", "at", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K",
    "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "bracketleft", "backslash", "bracketright",
    "asciicircum", "underscore", "grave", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s",
    "t", "u", "v", "w", "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde", 0,
    "Adieresis", "Aring", "Ccedilla", "Eacute", "Ntilde", "Odieresis", "Udieresis", "aacute", "agrave", "acircumflex", "adieresis",
    "atilde", "aring", "ccedilla", "eacute", "egrave", "ecircumflex", "edieresis", "iacute", "igrave", "icircumflex", "idieresis",
    "ntilde", "oacute", "ograve", "ocircumflex", "odieresis", "otilde", "uacute", "ugrave", "ucircumflex", "udieresis", "dagger",
    "degree", "cent", "sterling", "section", "bullet", "paragraph", "germandbls", "registered", "copyright", "trademark", "acute",
    "dieresis", "notequal", "AE", "Oslash", "infinity", "plusminus", "lessequal", "greaterequal", "yen", "mu", "partialdiff",
    "summation", "product", "pi", "integral", "ordfeminine", "ordmasculine", "Omega", "ae", "oslash", "questiondown", "exclamdown",
    "logicalnot", "radical", "florin", "approxequal", "Delta", "guillemotleft", "guillemotright", "ellipsis", "space", "Agrave",
    "Atilde", "Otilde", "OE", "oe", "endash", "emdash", "quotedblleft", "quotedblright", "quoteleft", "quoteright", "divide",
    "lozenge", "ydieresis", "Ydieresis", "fraction", "currency", "guilsinglleft", "guilsinglright", "fi", "fl", "daggerdbl",
    "periodcentered", "quotesinglbase", "quotedblbase", "perthousand", "Acircumflex", "Ecircumflex", "Aacute", "Edieresis",
    "Egrave", "Iacute", "Icircumflex", "Idieresis", "Igrave", "Oacute", "Ocircumflex", "apple", "Ograve", "Uacute", "Ucircumflex",
    "Ugrave", "dotlessi", "circumflex", "tilde", "macron", "breve", "dotaccent", "ring", "cedilla", "hungarumlaut", "ogonek",
    "caron"};
/* cp1252 0x80..0x9F → Unicode */
static const u16 cp1252_hi[32] = {0x20AC, 0x2022, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039,
                                  0x0152, 0x2022, 0x017D, 0x2022, 0x2022, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
                                  0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x2022, 0x017E, 0x0178};
static const u16 mac_hi[128] = {
    0xC4, 0xC5, 0xC7, 0xC9, 0xD1, 0xD6, 0xDC, 0xE1, 0xE0, 0xE2, 0xE4, 0xE3, 0xE5, 0xE7, 0xE9, 0xE8, 0xEA, 0xEB, 0xED, 0xEC, 0xEE, 0xEF,
    0xF1, 0xF3, 0xF2, 0xF4, 0xF6, 0xF5, 0xFA, 0xF9, 0xFB, 0xFC, 0x2020, 0xB0, 0xA2, 0xA3, 0xA7, 0x2022, 0xB6, 0xDF, 0xAE, 0xA9, 0x2122,
    0xB4, 0xA8, 0x2260, 0xC6, 0xD8, 0x221E, 0xB1, 0x2264, 0x2265, 0xA5, 0xB5, 0x2202, 0x2211, 0x220F, 0x3C0, 0x222B, 0xAA, 0xBA, 0x3A9,
    0xE6, 0xF8, 0xBF, 0xA1, 0xAC, 0x221A, 0x192, 0x2248, 0x2206, 0xAB, 0xBB, 0x2026, 0xA0, 0xC0, 0xC3, 0xD5, 0x152, 0x153, 0x2013,
    0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0xF7, 0x25CA, 0xFF, 0x178, 0x2044, 0xA4, 0x2039, 0x203A, 0xFB01, 0xFB02, 0x2021, 0xB7,
    0x201A, 0x201E, 0x2030, 0xC2, 0xCA, 0xC1, 0xCB, 0xC8, 0xCD, 0xCE, 0xCF, 0xCC, 0xD3, 0xD4, 0xF8FF, 0xD2, 0xDA, 0xDB, 0xD9, 0x131,
    0x2C6, 0x2DC, 0xAF, 0x2D8, 0x2D9, 0x2DA, 0xB8, 0x2DD, 0x2DB, 0x2C7};
/* nomes do Symbol (letras gregas e matemática) → Unicode */
static const struct { const char *n; u16 u; } sym_names[] = {
    {"Alpha", 0x391}, {"Beta", 0x392}, {"Chi", 0x3A7}, {"Delta", 0x394}, {"Epsilon", 0x395}, {"Phi", 0x3A6}, {"Gamma", 0x393},
    {"Eta", 0x397}, {"Iota", 0x399}, {"theta1", 0x3D1}, {"Kappa", 0x39A}, {"Lambda", 0x39B}, {"Mu", 0x39C}, {"Nu", 0x39D},
    {"Omicron", 0x39F}, {"Pi", 0x3A0}, {"Theta", 0x398}, {"Rho", 0x3A1}, {"Sigma", 0x3A3}, {"Tau", 0x3A4}, {"Upsilon", 0x3A5},
    {"sigma1", 0x3C2}, {"Omega", 0x3A9}, {"Xi", 0x39E}, {"Psi", 0x3A8}, {"Zeta", 0x396}, {"alpha", 0x3B1}, {"beta", 0x3B2},
    {"chi", 0x3C7}, {"delta", 0x3B4}, {"epsilon", 0x3B5}, {"phi", 0x3C6}, {"gamma", 0x3B3}, {"eta", 0x3B7}, {"iota", 0x3B9},
    {"phi1", 0x3D5}, {"kappa", 0x3BA}, {"lambda", 0x3BB}, {"mu", 0x3BC}, {"nu", 0x3BD}, {"omicron", 0x3BF}, {"pi", 0x3C0},
    {"theta", 0x3B8}, {"rho", 0x3C1}, {"sigma", 0x3C3}, {"tau", 0x3C4}, {"upsilon", 0x3C5}, {"omega1", 0x3D6}, {"omega", 0x3C9},
    {"xi", 0x3BE}, {"psi", 0x3C8}, {"zeta", 0x3B6}, {"universal", 0x2200}, {"existential", 0x2203}, {"suchthat", 0x220B},
    {"asteriskmath", 0x2217}, {"minus", 0x2212}, {"congruent", 0x2245}, {"therefore", 0x2234}, {"perpendicular", 0x22A5},
    {"similar", 0x223C}, {"Upsilon1", 0x3D2}, {"minute", 0x2032}, {"second", 0x2033}, {"infinity", 0x221E}, {"club", 0x2663},
    {"diamond", 0x2666}, {"heart", 0x2665}, {"spade", 0x2660}, {"arrowboth", 0x2194}, {"arrowleft", 0x2190}, {"arrowup", 0x2191},
    {"arrowright", 0x2192}, {"arrowdown", 0x2193}, {"proportional", 0x221D}, {"partialdiff", 0x2202}, {"bullet", 0x2022},
    {"notequal", 0x2260}, {"equivalence", 0x2261}, {"approxequal", 0x2248}, {"ellipsis", 0x2026}, {"carriagereturn", 0x21B5},
    {"aleph", 0x2135}, {"Ifraktur", 0x2111}, {"Rfraktur", 0x211C}, {"weierstrass", 0x2118}, {"circlemultiply", 0x2297},
    {"circleplus", 0x2295}, {"emptyset", 0x2205}, {"intersection", 0x2229}, {"union", 0x222A}, {"propersuperset", 0x2283},
    {"reflexsuperset", 0x2287}, {"notsubset", 0x2284}, {"propersubset", 0x2282}, {"reflexsubset", 0x2286}, {"element", 0x2208},
    {"notelement", 0x2209}, {"angle", 0x2220}, {"gradient", 0x2207}, {"product", 0x220F}, {"radical", 0x221A}, {"dotmath", 0x22C5},
    {"logicaland", 0x2227}, {"logicalor", 0x2228}, {"arrowdblboth", 0x21D4}, {"arrowdblleft", 0x21D0}, {"arrowdblup", 0x21D1},
    {"arrowdblright", 0x21D2}, {"arrowdbldown", 0x21D3}, {"lozenge", 0x25CA}, {"angleleft", 0x2329}, {"summation", 0x2211},
    {"angleright", 0x232A}, {"integral", 0x222B}, {"lessequal", 0x2264}, {"greaterequal", 0x2265}, {"Euro", 0x20AC},
    {"fi", 0xFB01}, {"fl", 0xFB02}, {"ff", 0xFB00}, {"ffi", 0xFB03}, {"ffl", 0xFB04}, {"dotlessi", 0x131}, {"Lslash", 0x141},
    {"lslash", 0x142}, {"fraction", 0x2044}, {"quotesingle", 0x27}, {"grave", 0x60}, {"hungarumlaut", 0x2DD}, {"ogonek", 0x2DB},
    {"caron", 0x2C7}, {"breve", 0x2D8}, {"dotaccent", 0x2D9}, {"ring", 0x2DA}, {"nbspace", 0xA0}, {"sfthyphen", 0xAD},
    {"minus", 0x2212}, {"periodcentered", 0xB7}, {"Delta", 0x2206}, {"Omega", 0x2126}};
static const u16 *enc_unicode_table(int which) {
    static u16 win[256], mac[256], ready;
    if (!ready) {
        for (int i = 0; i < 256; i++) {
            win[i] = (u16)(i >= 0x80 && i < 0xA0 ? cp1252_hi[i - 0x80] : i);
            mac[i] = (u16)(i >= 0x80 ? mac_hi[i - 0x80] : i);
        }
        ready = 1;
    }
    return which ? mac : win;
}
static u32 hexval(const char *s, int n) {
    u32 v = 0;
    for (int i = 0; i < n; i++) {
        int c = s[i], d = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (d < 0) return 0xFFFFFFFFu;
        v = v << 4 | (u32)d;
    }
    return v;
}
/* nome de glifo → Unicode (0 = desconhecido). "a.sc", "f_f" e sufixos: a parte antes do ponto */
static u32 glyph_unicode(const char *name) {
    if (!name || !name[0]) return 0;
    char base[64];
    size_t k = 0;
    while (name[k] && name[k] != '.' && k < sizeof base - 1) { base[k] = name[k]; k++; }
    base[k] = 0;
    if (!strncmp(base, "uni", 3) && strlen(base) >= 7) { u32 v = hexval(base + 3, 4); if (v != 0xFFFFFFFFu) return v; }
    if (base[0] == 'u' && (strlen(base) == 5 || strlen(base) == 6 || strlen(base) == 7)) { u32 v = hexval(base + 1, (int)strlen(base) - 1); if (v != 0xFFFFFFFFu) return v; }
    for (u32 i = 0; i < sizeof sym_names / sizeof sym_names[0]; i++) if (!strcmp(base, sym_names[i].n)) return sym_names[i].u;
    const u16 *w = enc_unicode_table(0), *m = enc_unicode_table(1);
    for (int i = 32; i < 256; i++) if (enc_win[i] && !strcmp(enc_win[i], base) && !(i >= 0x80 && i < 0xA0 && w[i] == 0x2022 && strcmp(base, "bullet"))) return w[i];
    for (int i = 32; i < 256; i++) if (enc_mac[i] && !strcmp(enc_mac[i], base)) return m[i];
    for (int i = 32; i < 256; i++) if (enc_std[i] && !strcmp(enc_std[i], base)) {
        /* nomes que só o Standard tem */
        if (!strcmp(base, "quoteright")) return 0x2019;
        if (!strcmp(base, "quoteleft")) return 0x2018;
    }
    if (!base[1]) return (u8)base[0];
    if (base[0] == 'g' && base[1] >= '0' && base[1] <= '9') return 0;       /* "g123": sem Unicode */
    return 0;
}
