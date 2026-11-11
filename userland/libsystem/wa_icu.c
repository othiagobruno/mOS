/*
 * mOS libSystem: as seis funções da libicucore que o WhatsApp 2.26.45.13 importa
 * (machocheck, 10 nov. 2026). O mOS não tem a ICU inteira no libSystem.
 *  - u_strFromUTF8WithSub / u_strToUTF8WithSub: conversão UTF-8 <-> UTF-16 exata, com o
 *    caractere de substituição para sequências inválidas e a contagem de substituições;
 *    pre-flight (destino NULL ou curto) devolve o tamanho e U_BUFFER_OVERFLOW_ERROR, como na ICU.
 *  - u_tolower: Latin-1, Latin Extended-A, grego, cirílico, armênio e os de largura total.
 *  - u_ispunct: categorias P* (ASCII, Latin-1, pontuação geral, CJK e largura total).
 *  - u_hasBinaryProperty: White_Space, Emoji, Emoji_Presentation, Emoji_Modifier,
 *    Emoji_Modifier_Base, Emoji_Component, Regional_Indicator e Extended_Pictographic
 *    pelas faixas do emoji-data.txt; as outras propriedades devolvem falso.
 *  - u_getIntPropertyValue: General_Category (máscara simplificada) e East_Asian_Width.
 */
#ifdef WA_ICU_HOST
#include <string.h>
#else
#include "libc.h"
#endif

typedef unsigned short UChar;
typedef int UChar32;
typedef int UErrorCode;
enum { U_ZERO_ERROR = 0, U_ILLEGAL_ARGUMENT_ERROR = 1, U_INVALID_CHAR_FOUND = 10, U_BUFFER_OVERFLOW_ERROR = 15,
       U_STRING_NOT_TERMINATED_WARNING = -124 };

static int u_failure(const UErrorCode *e) { return e && *e > 0; }

/* lê um ponto de código UTF-8; devolve -1 em sequência inválida e avança pelo menos 1 byte */
static UChar32 utf8_next(const unsigned char *s, int len, int *i) {
    unsigned char c = s[*i];
    int need, k;
    UChar32 cp, min;
    (*i)++;
    if (c < 0x80) return c;
    if (c >= 0xC2 && c <= 0xDF) { need = 1; cp = c & 0x1F; min = 0x80; }
    else if (c >= 0xE0 && c <= 0xEF) { need = 2; cp = c & 0x0F; min = 0x800; }
    else if (c >= 0xF0 && c <= 0xF4) { need = 3; cp = c & 0x07; min = 0x10000; }
    else return -1;
    for (k = 0; k < need; k++) {
        if (*i >= len || (s[*i] & 0xC0) != 0x80) return -1;
        cp = (cp << 6) | (s[*i] & 0x3F);
        (*i)++;
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return -1;
    return cp;
}

UChar *u_strFromUTF8WithSub(UChar *dest, int cap, int *plen, const char *src, int srclen, UChar32 sub, int *nsub, UErrorCode *err) {
    int i = 0, n = 0, subs = 0, len;
    const unsigned char *s = (const unsigned char *)src;
    if (u_failure(err)) return 0;
    if ((!src && srclen != 0) || cap < 0 || (!dest && cap > 0) || sub > 0x10FFFF || (sub >= 0xD800 && sub <= 0xDFFF)) {
        if (err) *err = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    len = srclen < 0 ? (int)strlen(src) : srclen;
    while (i < len) {
        UChar32 cp = utf8_next(s, len, &i);
        if (cp < 0) {
            if (sub < 0) { if (err) *err = U_INVALID_CHAR_FOUND; return 0; }
            cp = sub;
            subs++;
        }
        if (cp >= 0x10000) {
            if (n + 1 < cap) { dest[n] = (UChar)(0xD800 + ((cp - 0x10000) >> 10)); dest[n + 1] = (UChar)(0xDC00 + ((cp - 0x10000) & 0x3FF)); }
            n += 2;
        } else {
            if (n < cap) dest[n] = (UChar)cp;
            n++;
        }
    }
    if (plen) *plen = n;
    if (nsub) *nsub = subs;
    if (n < cap) dest[n] = 0;
    else if (err && n > cap) *err = U_BUFFER_OVERFLOW_ERROR;
    else if (err && n == cap && *err <= 0) *err = U_STRING_NOT_TERMINATED_WARNING;
    return dest;
}

char *u_strToUTF8WithSub(char *dest, int cap, int *plen, const UChar *src, int srclen, UChar32 sub, int *nsub, UErrorCode *err) {
    int i = 0, n = 0, subs = 0, len = srclen;
    unsigned char b[4];
    if (u_failure(err)) return 0;
    if ((!src && srclen != 0) || cap < 0 || (!dest && cap > 0) || sub > 0x10FFFF || (sub >= 0xD800 && sub <= 0xDFFF)) {
        if (err) *err = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    if (len < 0) { len = 0; while (src[len]) len++; }
    while (i < len) {
        UChar32 cp = src[i++];
        int k, m;
        if (cp >= 0xD800 && cp <= 0xDBFF && i < len && src[i] >= 0xDC00 && src[i] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (src[i++] - 0xDC00);
        } else if (cp >= 0xD800 && cp <= 0xDFFF) {
            if (sub < 0) { if (err) *err = U_INVALID_CHAR_FOUND; return 0; }
            cp = sub;
            subs++;
        }
        if (cp < 0x80) { b[0] = (unsigned char)cp; m = 1; }
        else if (cp < 0x800) { b[0] = (unsigned char)(0xC0 | (cp >> 6)); b[1] = (unsigned char)(0x80 | (cp & 0x3F)); m = 2; }
        else if (cp < 0x10000) { b[0] = (unsigned char)(0xE0 | (cp >> 12)); b[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F)); b[2] = (unsigned char)(0x80 | (cp & 0x3F)); m = 3; }
        else { b[0] = (unsigned char)(0xF0 | (cp >> 18)); b[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F)); b[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F)); b[3] = (unsigned char)(0x80 | (cp & 0x3F)); m = 4; }
        for (k = 0; k < m; k++) { if (n + k < cap) dest[n + k] = (char)b[k]; }
        n += m;
    }
    if (plen) *plen = n;
    if (nsub) *nsub = subs;
    if (n < cap) dest[n] = 0;
    else if (err && n > cap) *err = U_BUFFER_OVERFLOW_ERROR;
    else if (err && n == cap && *err <= 0) *err = U_STRING_NOT_TERMINATED_WARNING;
    return dest;
}

UChar32 u_tolower(UChar32 c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c < 0xC0) return c;
    if ((c >= 0xC0 && c <= 0xDE && c != 0xD7)) return c + 32;
    if (c >= 0x100 && c <= 0x137) return c | 1;                         /* pares maiúscula/minúscula */
    if (c >= 0x139 && c <= 0x148) return (c & 1) ? c + 1 : c;
    if (c >= 0x14A && c <= 0x177) return c | 1;
    if (c == 0x178) return 0xFF;
    if (c >= 0x179 && c <= 0x17E) return (c & 1) ? c + 1 : c;
    if (c == 0x130) return 'i';
    if (c == 0x386) return 0x3AC;
    if (c >= 0x388 && c <= 0x38A) return c + 37;
    if (c == 0x38C) return 0x3CC;
    if (c == 0x38E || c == 0x38F) return c + 63;
    if (c >= 0x391 && c <= 0x3AB && c != 0x3A2) return c + 32;           /* grego */
    if (c >= 0x400 && c <= 0x40F) return c + 80;                          /* cirílico Ѐ-Џ */
    if (c >= 0x410 && c <= 0x42F) return c + 32;                          /* А-Я */
    if (c >= 0x460 && c <= 0x4FF && !(c >= 0x482 && c <= 0x489)) return (c >= 0x4C1 && c <= 0x4CE) ? ((c & 1) ? c + 1 : c) : (c | 1);
    if (c >= 0x531 && c <= 0x556) return c + 48;                          /* armênio */
    if (c >= 0x1E00 && c <= 0x1EFF) return c | 1;                         /* Latin Extended Additional */
    if (c >= 0xFF21 && c <= 0xFF3A) return c + 32;                        /* largura total */
    return c;
}

int u_ispunct(UChar32 c) {
    if (c < 0x80) return (c >= 0x21 && c <= 0x2F && c != '$' && c != '+') || (c >= 0x3A && c <= 0x40 && c != '<' && c != '=' && c != '>')
                      || (c >= 0x5B && c <= 0x60 && c != '^' && c != '`') || c == '{' || c == '}';
    if (c == 0xA1 || c == 0xA7 || c == 0xAB || c == 0xB6 || c == 0xB7 || c == 0xBB || c == 0xBF) return 1;
    if (c == 0x37E || c == 0x387 || (c >= 0x55A && c <= 0x55F) || c == 0x589 || c == 0x5BE || c == 0x5C0 || c == 0x5F3 || c == 0x5F4) return 1;
    if (c == 0x60C || c == 0x60D || c == 0x61B || c == 0x61F || (c >= 0x66A && c <= 0x66D) || c == 0x6D4) return 1;
    if (c == 0x964 || c == 0x965 || c == 0x970 || c == 0xE4F || c == 0xE5A || c == 0xE5B) return 1;
    if ((c >= 0x2010 && c <= 0x2027) || (c >= 0x2030 && c <= 0x2043) || (c >= 0x2045 && c <= 0x2051) || (c >= 0x2053 && c <= 0x205E)) return 1;
    if (c == 0x207D || c == 0x207E || c == 0x208D || c == 0x208E || (c >= 0x2308 && c <= 0x230B) || c == 0x2329 || c == 0x232A) return 1;
    if ((c >= 0x2768 && c <= 0x2775) || (c >= 0x27E6 && c <= 0x27EF) || (c >= 0x2E00 && c <= 0x2E4F)) return 1;
    if ((c >= 0x3001 && c <= 0x3003) || (c >= 0x3008 && c <= 0x3011) || (c >= 0x3014 && c <= 0x301F) || c == 0x3030 || c == 0x303D || c == 0x30A0 || c == 0x30FB) return 1;
    if ((c >= 0xFE10 && c <= 0xFE19) || (c >= 0xFE30 && c <= 0xFE52) || (c >= 0xFE54 && c <= 0xFE61) || c == 0xFE63 || c == 0xFE68 || c == 0xFE6A || c == 0xFE6B) return 1;
    if ((c >= 0xFF01 && c <= 0xFF03) || (c >= 0xFF05 && c <= 0xFF0A) || (c >= 0xFF0C && c <= 0xFF0F) || c == 0xFF1A || c == 0xFF1B || c == 0xFF1F || c == 0xFF20
        || (c >= 0xFF3B && c <= 0xFF3D) || c == 0xFF3F || c == 0xFF5B || c == 0xFF5D || (c >= 0xFF5F && c <= 0xFF65)) return 1;
    return 0;
}

/* ---- propriedades binárias (UProperty) ---- */
enum { UCHAR_WHITE_SPACE = 31, UCHAR_EMOJI = 57, UCHAR_EMOJI_PRESENTATION = 58, UCHAR_EMOJI_MODIFIER = 59,
       UCHAR_EMOJI_MODIFIER_BASE = 60, UCHAR_EMOJI_COMPONENT = 61, UCHAR_REGIONAL_INDICATOR = 62,
       UCHAR_EXTENDED_PICTOGRAPHIC = 64 };
typedef struct { UChar32 lo, hi; } urange_t;
static int in_ranges(UChar32 c, const urange_t *r, int n) { for (int i = 0; i < n; i++) if (c >= r[i].lo && c <= r[i].hi) return 1; return 0; }
/* Extended_Pictographic (emoji-data.txt, Unicode 15): blocos de símbolos e pictogramas */
static const urange_t ext_pict[] = {
    {0xA9, 0xA9}, {0xAE, 0xAE}, {0x203C, 0x203C}, {0x2049, 0x2049}, {0x2122, 0x2122}, {0x2139, 0x2139}, {0x2194, 0x2199},
    {0x21A9, 0x21AA}, {0x231A, 0x231B}, {0x2328, 0x2328}, {0x2388, 0x2388}, {0x23CF, 0x23CF}, {0x23E9, 0x23F3}, {0x23F8, 0x23FA},
    {0x24C2, 0x24C2}, {0x25AA, 0x25AB}, {0x25B6, 0x25B6}, {0x25C0, 0x25C0}, {0x25FB, 0x25FE}, {0x2600, 0x2605}, {0x2607, 0x2612},
    {0x2614, 0x2685}, {0x2690, 0x2705}, {0x2708, 0x2712}, {0x2714, 0x2714}, {0x2716, 0x2716}, {0x271D, 0x271D}, {0x2721, 0x2721},
    {0x2728, 0x2728}, {0x2733, 0x2734}, {0x2744, 0x2744}, {0x2747, 0x2747}, {0x274C, 0x274C}, {0x274E, 0x274E}, {0x2753, 0x2755},
    {0x2757, 0x2757}, {0x2763, 0x2767}, {0x2795, 0x2797}, {0x27A1, 0x27A1}, {0x27B0, 0x27B0}, {0x27BF, 0x27BF}, {0x2934, 0x2935},
    {0x2B05, 0x2B07}, {0x2B1B, 0x2B1C}, {0x2B50, 0x2B50}, {0x2B55, 0x2B55}, {0x3030, 0x3030}, {0x303D, 0x303D}, {0x3297, 0x3297},
    {0x3299, 0x3299}, {0x1F000, 0x1F0FF}, {0x1F10D, 0x1F10F}, {0x1F12F, 0x1F12F}, {0x1F16C, 0x1F171}, {0x1F17E, 0x1F17F},
    {0x1F18E, 0x1F18E}, {0x1F191, 0x1F19A}, {0x1F1AD, 0x1F1E5}, {0x1F201, 0x1F20F}, {0x1F21A, 0x1F21A}, {0x1F22F, 0x1F22F},
    {0x1F232, 0x1F23A}, {0x1F23C, 0x1F23F}, {0x1F249, 0x1F3FA}, {0x1F400, 0x1F53D}, {0x1F546, 0x1F64F}, {0x1F680, 0x1F6FF},
    {0x1F774, 0x1F77F}, {0x1F7D5, 0x1F7FF}, {0x1F80C, 0x1F80F}, {0x1F848, 0x1F84F}, {0x1F85A, 0x1F85F}, {0x1F888, 0x1F88F},
    {0x1F8AE, 0x1F8FF}, {0x1F90C, 0x1F93A}, {0x1F93C, 0x1F945}, {0x1F947, 0x1FAFF}, {0x1FC00, 0x1FFFD},
};
/* Emoji_Presentation: os que já saem coloridos sem o seletor U+FE0F */
static const urange_t emoji_pres[] = {
    {0x231A, 0x231B}, {0x23E9, 0x23EC}, {0x23F0, 0x23F0}, {0x23F3, 0x23F3}, {0x25FD, 0x25FE}, {0x2614, 0x2615}, {0x2648, 0x2653},
    {0x267F, 0x267F}, {0x2693, 0x2693}, {0x26A1, 0x26A1}, {0x26AA, 0x26AB}, {0x26BD, 0x26BE}, {0x26C4, 0x26C5}, {0x26CE, 0x26CE},
    {0x26D4, 0x26D4}, {0x26EA, 0x26EA}, {0x26F2, 0x26F3}, {0x26F5, 0x26F5}, {0x26FA, 0x26FA}, {0x26FD, 0x26FD}, {0x2705, 0x2705},
    {0x270A, 0x270B}, {0x2728, 0x2728}, {0x274C, 0x274C}, {0x274E, 0x274E}, {0x2753, 0x2755}, {0x2757, 0x2757}, {0x2795, 0x2797},
    {0x27B0, 0x27B0}, {0x27BF, 0x27BF}, {0x2B1B, 0x2B1C}, {0x2B50, 0x2B50}, {0x2B55, 0x2B55}, {0x1F004, 0x1F004}, {0x1F0CF, 0x1F0CF},
    {0x1F18E, 0x1F18E}, {0x1F191, 0x1F19A}, {0x1F1E6, 0x1F1FF}, {0x1F201, 0x1F201}, {0x1F21A, 0x1F21A}, {0x1F22F, 0x1F22F},
    {0x1F232, 0x1F236}, {0x1F238, 0x1F23A}, {0x1F250, 0x1F251}, {0x1F300, 0x1F320}, {0x1F32D, 0x1F335}, {0x1F337, 0x1F37C},
    {0x1F37E, 0x1F393}, {0x1F3A0, 0x1F3CA}, {0x1F3CF, 0x1F3D3}, {0x1F3E0, 0x1F3F0}, {0x1F3F4, 0x1F3F4}, {0x1F3F8, 0x1F43E},
    {0x1F440, 0x1F440}, {0x1F442, 0x1F4FC}, {0x1F4FF, 0x1F53D}, {0x1F54B, 0x1F54E}, {0x1F550, 0x1F567}, {0x1F57A, 0x1F57A},
    {0x1F595, 0x1F596}, {0x1F5A4, 0x1F5A4}, {0x1F5FB, 0x1F64F}, {0x1F680, 0x1F6C5}, {0x1F6CC, 0x1F6CC}, {0x1F6D0, 0x1F6D2},
    {0x1F6D5, 0x1F6D7}, {0x1F6DC, 0x1F6DF}, {0x1F6EB, 0x1F6EC}, {0x1F6F4, 0x1F6FC}, {0x1F7E0, 0x1F7EB}, {0x1F7F0, 0x1F7F0},
    {0x1F90C, 0x1F93A}, {0x1F93C, 0x1F945}, {0x1F947, 0x1F9FF}, {0x1FA70, 0x1FA7C}, {0x1FA80, 0x1FA89}, {0x1FA8F, 0x1FAC6},
    {0x1FACE, 0x1FADC}, {0x1FADF, 0x1FAE9}, {0x1FAF0, 0x1FAF8},
};
/* Emoji_Modifier_Base: quem aceita tom de pele */
static const urange_t emoji_mod_base[] = {
    {0x261D, 0x261D}, {0x26F9, 0x26F9}, {0x270A, 0x270D}, {0x1F385, 0x1F385}, {0x1F3C2, 0x1F3C4}, {0x1F3C7, 0x1F3C7},
    {0x1F3CA, 0x1F3CC}, {0x1F442, 0x1F443}, {0x1F446, 0x1F450}, {0x1F466, 0x1F478}, {0x1F47C, 0x1F47C}, {0x1F481, 0x1F483},
    {0x1F485, 0x1F487}, {0x1F48F, 0x1F48F}, {0x1F491, 0x1F491}, {0x1F4AA, 0x1F4AA}, {0x1F574, 0x1F575}, {0x1F57A, 0x1F57A},
    {0x1F590, 0x1F590}, {0x1F595, 0x1F596}, {0x1F645, 0x1F647}, {0x1F64B, 0x1F64F}, {0x1F6A3, 0x1F6A3}, {0x1F6B4, 0x1F6B6},
    {0x1F6C0, 0x1F6C0}, {0x1F6CC, 0x1F6CC}, {0x1F90C, 0x1F90C}, {0x1F90F, 0x1F90F}, {0x1F918, 0x1F91F}, {0x1F926, 0x1F926},
    {0x1F930, 0x1F939}, {0x1F93C, 0x1F93E}, {0x1F977, 0x1F977}, {0x1F9B5, 0x1F9B6}, {0x1F9B8, 0x1F9B9}, {0x1F9BB, 0x1F9BB},
    {0x1F9CD, 0x1F9CF}, {0x1F9D1, 0x1F9DD}, {0x1FAC3, 0x1FAC5}, {0x1FAF0, 0x1FAF8},
};
static int is_white_space(UChar32 c) {
    return (c >= 9 && c <= 13) || c == 0x20 || c == 0x85 || c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A)
        || c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
}
int u_hasBinaryProperty(UChar32 c, int which) {
    switch (which) {
    case UCHAR_WHITE_SPACE: return is_white_space(c);
    case UCHAR_REGIONAL_INDICATOR: return c >= 0x1F1E6 && c <= 0x1F1FF;
    case UCHAR_EMOJI_MODIFIER: return c >= 0x1F3FB && c <= 0x1F3FF;
    case UCHAR_EMOJI_MODIFIER_BASE: return in_ranges(c, emoji_mod_base, (int)(sizeof emoji_mod_base / sizeof *emoji_mod_base));
    case UCHAR_EMOJI_PRESENTATION: return in_ranges(c, emoji_pres, (int)(sizeof emoji_pres / sizeof *emoji_pres));
    case UCHAR_EXTENDED_PICTOGRAPHIC: return in_ranges(c, ext_pict, (int)(sizeof ext_pict / sizeof *ext_pict));
    case UCHAR_EMOJI_COMPONENT:
        return c == '#' || c == '*' || (c >= '0' && c <= '9') || c == 0x200D || c == 0x20E3 || c == 0xFE0F
            || (c >= 0x1F1E6 && c <= 0x1F1FF) || (c >= 0x1F3FB && c <= 0x1F3FF) || (c >= 0x1F9B0 && c <= 0x1F9B3) || (c >= 0xE0020 && c <= 0xE007F);
    case UCHAR_EMOJI:
        if (c == '#' || c == '*' || (c >= '0' && c <= '9')) return 1;
        if (c >= 0x1F1E6 && c <= 0x1F1FF) return 1;
        if (c >= 0x1F3FB && c <= 0x1F3FF) return 1;
        if (c >= 0x1F000 && c <= 0x1FFFD && !in_ranges(c, ext_pict, (int)(sizeof ext_pict / sizeof *ext_pict))) return 0;
        return in_ranges(c, ext_pict, (int)(sizeof ext_pict / sizeof *ext_pict)) && !(c >= 0x1FC00 && c <= 0x1FFFD) && !(c >= 0x1F000 && c <= 0x1F003)
            && !(c >= 0x1F005 && c <= 0x1F0CE) && !(c >= 0x1F0D0 && c <= 0x1F0FF);
    default: return 0;
    }
}

/* UCHAR_GENERAL_CATEGORY (0x1005) e UCHAR_EAST_ASIAN_WIDTH (0x1004). Categorias: Lu 1, Ll 2, Nd 9,
 * Zs 12, Cc 15, Pd 19, Po 23, Sm 25, So 28 (UCharCategory); largura: N 0, A 1, H 2, F 3, Na 4, W 5. */
int u_getIntPropertyValue(UChar32 c, int which) {
    if (which == 0x1004) {
        if (c >= 0xFF01 && c <= 0xFF60) return 3;
        if (c >= 0xFFE0 && c <= 0xFFE6) return 3;
        if (c >= 0xFF61 && c <= 0xFFDC) return 2;
        if ((c >= 0x1100 && c <= 0x115F) || (c >= 0x2E80 && c <= 0x303E) || (c >= 0x3041 && c <= 0x33FF) || (c >= 0x3400 && c <= 0x4DBF)
            || (c >= 0x4E00 && c <= 0x9FFF) || (c >= 0xA000 && c <= 0xA4CF) || (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0xF900 && c <= 0xFAFF)
            || (c >= 0xFE30 && c <= 0xFE4F) || (c >= 0x20000 && c <= 0x3FFFD)
            || in_ranges(c, emoji_pres, (int)(sizeof emoji_pres / sizeof *emoji_pres))) return 5;
        if (c >= 0x20 && c <= 0x7E) return 4;
        return 0;
    }
    if (which == 0x1005) {
        if (c < 0x20 || (c >= 0x7F && c < 0xA0)) return 15;
        if (is_white_space(c) && c != 0x2028 && c != 0x2029) return 12;
        if (c >= '0' && c <= '9') return 9;
        if (u_tolower(c) != c) return 1;
        if ((c >= 'a' && c <= 'z') || (c >= 0xDF && c <= 0xFF && c != 0xF7)) return 2;
        if (c == '-' || (c >= 0x2010 && c <= 0x2015)) return 19;
        if (u_ispunct(c)) return 23;
        if (c == '+' || c == '<' || c == '=' || c == '>' || c == '|' || c == '~' || c == 0xAC || c == 0xB1 || c == 0xD7 || c == 0xF7) return 25;
        if (in_ranges(c, ext_pict, (int)(sizeof ext_pict / sizeof *ext_pict))) return 28;
        return 0;
    }
    if (which < 0x1000) return u_hasBinaryProperty(c, which);
    return 0;
}
