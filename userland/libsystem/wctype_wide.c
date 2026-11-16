/*
 * mOS libSystem — classificação e troca de caixa de caracteres largos (wint_t) com as tabelas Unicode do musl
 * 1.2.5 (MIT, userland/third_party/musl/src/ctype): iswalpha/iswupper/.../wcwidth/towupper/towlower, as funções
 * de classe (wctype/iswctype/wctrans/towctrans) e os nomes "extendidos" da Apple que o <ctype.h>/<wctype.h> dela
 * chama para code points >= 128: ___runetype, ___tolower, ___toupper (e as variantes _l). Antes o libSystem só
 * classificava ASCII (acentos, grego, cirílico, CJK vinham errados). As tabelas são puro dado; a lógica é a do musl.
 *
 * O "C locale" do mOS classifica todo o Unicode (como a libc do macOS, que não limita isw* ao locale corrente).
 * Teste: tools/wctype_hosttest.sh.
 */
#include "sys.h"

int strcmp(const char *a, const char *b);

typedef int wint_t_;                                 /* wint_t do macOS = int (wchar_t de 32 bits) */

/* ---- tabelas de dados do musl (MIT) ---- */
static const unsigned char alpha_tab[] = {
#include "../third_party/musl/src/ctype/alpha.h"
};
static const unsigned char punct_tab[] = {
#include "../third_party/musl/src/ctype/punct.h"
};
static const unsigned char nonspacing_tab[] = {
#include "../third_party/musl/src/ctype/nonspacing.h"
};
static const unsigned char wide_tab[] = {
#include "../third_party/musl/src/ctype/wide.h"
};

/* ================================================================ classes de caractere */
int iswalpha(wint_t_ wc) {
    if ((unsigned)wc < 0x20000U) return (alpha_tab[alpha_tab[wc >> 8] * 32 + ((wc & 255) >> 3)] >> (wc & 7)) & 1;
    return (unsigned)wc < 0x2fffeU;
}
int iswdigit(wint_t_ wc) { return (unsigned)wc - '0' < 10; }
int iswalnum(wint_t_ wc) { return iswdigit(wc) || iswalpha(wc); }
int iswpunct(wint_t_ wc) {
    if ((unsigned)wc < 0x20000U) return (punct_tab[punct_tab[wc >> 8] * 32 + ((wc & 255) >> 3)] >> (wc & 7)) & 1;
    return 0;
}
int iswspace(wint_t_ wc) {
    /* White_Space do Unicode menos os espaços sem quebra e caracteres de script (igual ao musl) */
    static const wint_t_ sp[] = {' ', '\t', '\n', '\r', 11, 12, 0x85, 0x2000, 0x2001, 0x2002, 0x2003, 0x2004,
                                 0x2005, 0x2006, 0x2008, 0x2009, 0x200a, 0x2028, 0x2029, 0x205f, 0x3000, 0};
    if (!wc) return 0;
    for (const wint_t_ *p = sp; *p; p++) if (*p == wc) return 1;
    return 0;
}
int iswcntrl(wint_t_ wc) {
    return (unsigned)wc < 32 || (unsigned)(wc - 0x7f) < 33 || (unsigned)(wc - 0x2028) < 2 || (unsigned)(wc - 0xfff9) < 3;
}
int iswprint(wint_t_ wc) {
    if ((unsigned)wc < 0xffU) return ((wc + 1) & 0x7f) >= 0x21;
    if ((unsigned)wc < 0x2028U || (unsigned)(wc - 0x202a) < 0xd800 - 0x202a || (unsigned)(wc - 0xe000) < 0xfff9 - 0xe000) return 1;
    if ((unsigned)(wc - 0xfffc) > 0x10ffff - 0xfffc || (wc & 0xfffe) == 0xfffe) return 0;
    return 1;
}
int iswgraph(wint_t_ wc) { return !iswspace(wc) && iswprint(wc); }
int iswblank(wint_t_ wc) { return wc == ' ' || wc == '\t'; }
int iswxdigit(wint_t_ wc) { return (unsigned)(wc - '0') < 10 || (unsigned)((wc | 32) - 'a') < 6; }

/* ================================================================ troca de caixa (casemap do musl) */
static const unsigned char tab[];
static const unsigned char rulebases[512];
static const int rules[];
static const unsigned char exceptions[][2];
#include "../third_party/musl/src/ctype/casemap.h"

static int casemap(unsigned c, int dir) {
    unsigned b, x, y, v, rt, xb, xn;
    int r, rd, c0 = c;
    if (c >= 0x20000) return c;
    b = c >> 8;
    c &= 255;
    x = c / 3;
    y = c % 3;
    v = tab[tab[b] * 86 + x];
    static const int mt[] = {2048, 342, 57};
    v = (v * mt[y] >> 11) % 6;
    r = rules[rulebases[b] + v];
    rt = r & 255;
    rd = r >> 8;
    if (rt < 2) return c0 + (rd & -(rt ^ dir));
    xn = rd & 0xff;
    xb = (unsigned)rd >> 8;
    while (xn) {
        unsigned t = exceptions[xb + xn / 2][0];
        if (t == c) {
            r = rules[exceptions[xb + xn / 2][1]];
            rt = r & 255;
            rd = r >> 8;
            if (rt < 2) return c0 + (rd & -(rt ^ dir));
            return c0 + (dir ? -1 : 1);              /* os quatro titlecase excepcionais */
        } else if (t > c) {
            xn /= 2;
        } else {
            xb += xn / 2;
            xn -= xn / 2;
        }
    }
    return c0;
}
wint_t_ towlower(wint_t_ wc) { return casemap(wc, 0); }
wint_t_ towupper(wint_t_ wc) { return casemap(wc, 1); }
int iswlower(wint_t_ wc) { return towupper(wc) != wc; }
int iswupper(wint_t_ wc) { return towlower(wc) != wc; }

/* ================================================================ largura na tela */
int wcwidth(wint_t_ wc) {
    if ((unsigned)wc < 0xffU) return ((wc + 1) & 0x7f) >= 0x21 ? 1 : wc ? -1 : 0;
    if (((unsigned)wc & 0xfffeffffU) < 0xfffe) {
        if ((nonspacing_tab[nonspacing_tab[wc >> 8] * 32 + ((wc & 255) >> 3)] >> (wc & 7)) & 1) return 0;
        if ((wide_tab[wide_tab[wc >> 8] * 32 + ((wc & 255) >> 3)] >> (wc & 7)) & 1) return 2;
        return 1;
    }
    if ((wc & 0xfffe) == 0xfffe) return -1;
    if ((unsigned)wc - 0x20000U < 0x20000) return 2;
    if (wc == 0xe0001 || (unsigned)(wc - 0xe0020) < 0x5f || (unsigned)(wc - 0xe0100) < 0xef) return 0;
    return 1;
}
int wcswidth(const wint_t_ *s, size_t n) {
    int w = 0;
    for (; n && *s; n--, s++) { int k = wcwidth(*s); if (k < 0) return -1; w += k; }
    return w;
}

/* ================================================================ classes por nome (wctype / wctrans) */
typedef unsigned long wctype_t_;
typedef int wctrans_t_;
int iswctype(wint_t_ wc, wctype_t_ type) {
    switch (type) {
    case 1: return iswalnum(wc);
    case 2: return iswalpha(wc);
    case 3: return iswblank(wc);
    case 4: return iswcntrl(wc);
    case 5: return iswdigit(wc);
    case 6: return iswgraph(wc);
    case 7: return iswlower(wc);
    case 8: return iswprint(wc);
    case 9: return iswpunct(wc);
    case 10: return iswspace(wc);
    case 11: return iswupper(wc);
    case 12: return iswxdigit(wc);
    }
    return 0;
}
wctype_t_ wctype(const char *s) {
    static const char names[] = "alnum\0alpha\0blank\0cntrl\0digit\0graph\0lower\0print\0punct\0space\0upper\0xdigit";
    int i;
    const char *p;
    for (i = 1, p = names; *p; i++, p += 6) if (*s == *p && !strcmp(s, p)) return (wctype_t_)i;
    return 0;
}
wctrans_t_ wctrans(const char *class) {
    if (!strcmp(class, "toupper")) return 1;
    if (!strcmp(class, "tolower")) return 2;
    return 0;
}
wint_t_ towctrans(wint_t_ wc, wctrans_t_ trans) { return trans == 1 ? towupper(wc) : trans == 2 ? towlower(wc) : wc; }

/* ================================================================ nomes extendidos da Apple
 * O <ctype.h>/<wctype.h> do macOS, para um rune >= 128 (fora do cache de 256), chama ___runetype(c) e usa a
 * máscara _CTYPE_* de volta; e chama ___tolower/___toupper para a troca de caixa. Montamos a máscara a partir das
 * classes acima, com os mesmos bits de libc.c (ctype_init). O byte baixo leva o valor do dígito/xdigit, como na Apple. */
#define _CT_A 0x00000100u /* alpha */
#define _CT_C 0x00000200u /* control */
#define _CT_D 0x00000400u /* digit */
#define _CT_G 0x00000800u /* graph */
#define _CT_L 0x00001000u /* lower */
#define _CT_P 0x00002000u /* punct */
#define _CT_S 0x00004000u /* space */
#define _CT_U 0x00008000u /* upper */
#define _CT_X 0x00010000u /* xdigit */
#define _CT_B 0x00020000u /* blank */
#define _CT_R 0x00040000u /* print */
unsigned long ___runetype(wint_t_ c) {
    unsigned long t = 0;
    if (iswalpha(c)) t |= _CT_A;
    if (iswcntrl(c)) t |= _CT_C;
    if (iswdigit(c)) t |= _CT_D | (unsigned)(c - '0');
    if (iswgraph(c)) t |= _CT_G;
    if (iswlower(c)) t |= _CT_L;
    if (iswpunct(c)) t |= _CT_P;
    if (iswspace(c)) t |= _CT_S;
    if (iswupper(c)) t |= _CT_U;
    if (iswblank(c)) t |= _CT_B;
    if (iswprint(c)) t |= _CT_R;
    if (iswxdigit(c)) { t |= _CT_X; if (!(t & _CT_D)) t |= (unsigned)(10 + ((c | 32) - 'a')); }
    return t;
}
unsigned long ___runetype_l(wint_t_ c, void *loc) { (void)loc; return ___runetype(c); }
wint_t_ ___tolower(wint_t_ c) { return towlower(c); }
wint_t_ ___toupper(wint_t_ c) { return towupper(c); }
wint_t_ ___tolower_l(wint_t_ c, void *loc) { (void)loc; return towlower(c); }
wint_t_ ___toupper_l(wint_t_ c, void *loc) { (void)loc; return towupper(c); }
