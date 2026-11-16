/*
 * mOS libSystem — codificações do CFString/NSString (ver fdn_encoding_core.h).
 */
#include "fdn_encoding_core.h"

void *malloc(size_t n);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);

/* tipo: 0 ICU, 1 UTF-8, 2 UTF-16 (BOM/máquina), 3 UTF-16BE, 4 UTF-16LE, 5 UTF-32, 6 UTF-32BE, 7 UTF-32LE, 8 ASCII, 9 NonLossy */
typedef struct { uint32_t cf; uint8_t kind, stateful; uint16_t win; const char *icu, *name; } enc_t;
static const enc_t T[] = {
    { ENC_UTF8, 1, 0, 65001, 0, "Unicode (UTF-8)" },
    { ENC_UTF16, 2, 0, 0, 0, "Unicode (UTF-16)" },
    { ENC_UTF16BE, 3, 0, 1201, 0, "Unicode (UTF-16BE)" },
    { ENC_UTF16LE, 4, 0, 1200, 0, "Unicode (UTF-16LE)" },
    { ENC_UTF32, 5, 0, 0, 0, "Unicode (UTF-32)" },
    { ENC_UTF32BE, 6, 0, 12001, 0, "Unicode (UTF-32BE)" },
    { ENC_UTF32LE, 7, 0, 12000, 0, "Unicode (UTF-32LE)" },
    { ENC_ASCII, 8, 0, 20127, 0, "Western (ASCII)" },
    { ENC_NONLOSSY, 9, 0, 0, 0, "Non-lossy ASCII" },
    { 0x04000100, 0, 1, 65000, "UTF-7", "Unicode (UTF-7)" },
    { ENC_MACROMAN, 0, 0, 10000, "macos-0_2-10.2", "Western (Mac OS Roman)" },
    { 0x1d, 0, 0, 10029, "macos-29-10.2", "Central European (Mac OS)" },
    { 0x23, 0, 0, 10081, "macos-35-10.2", "Turkish (Mac OS)" },
    { 0x06, 0, 0, 10006, "macos-6_2-10.4", "Greek (Mac OS)" },
    { 0x07, 0, 0, 10007, "macos-7_3-10.2", "Cyrillic (Mac OS)" },
    { ENC_LATIN1, 0, 0, 28591, "ISO-8859-1", "Western (ISO Latin 1)" },
    { 0x0202, 0, 0, 28592, "ISO-8859-2", "Central European (ISO Latin 2)" },
    { 0x0203, 0, 0, 28593, "ISO-8859-3", "Western (ISO Latin 3)" },
    { 0x0204, 0, 0, 28594, "ISO-8859-4", "Central European (ISO Latin 4)" },
    { 0x0205, 0, 0, 28595, "ISO-8859-5", "Cyrillic (ISO 8859-5)" },
    { 0x0206, 0, 0, 28596, "ISO-8859-6", "Arabic (ISO 8859-6)" },
    { 0x0207, 0, 0, 28597, "ISO-8859-7", "Greek (ISO 8859-7)" },
    { 0x0208, 0, 0, 28598, "ISO-8859-8", "Hebrew (ISO 8859-8)" },
    { 0x0209, 0, 0, 28599, "ISO-8859-9", "Turkish (ISO Latin 5)" },
    { 0x020A, 0, 0, 0, "ISO-8859-10", "Nordic (ISO Latin 6)" },
    { 0x020B, 0, 0, 0, "ISO-8859-11", "Thai (ISO 8859-11)" },
    { 0x020D, 0, 0, 28603, "ISO-8859-13", "Baltic (ISO Latin 7)" },
    { 0x020E, 0, 0, 0, "ISO-8859-14", "Celtic (ISO Latin 8)" },
    { 0x020F, 0, 0, 28605, "ISO-8859-15", "Western (ISO Latin 9)" },
    { 0x0210, 0, 0, 0, "ISO-8859-16", "Romanian (ISO Latin 10)" },
    { 0x0400, 0, 0, 437, "ibm-437", "Latin-US (DOS)" },
    { 0x0405, 0, 0, 737, "ibm-737", "Greek (DOS)" },
    { 0x0406, 0, 0, 775, "ibm-775", "Baltic (DOS)" },
    { 0x0410, 0, 0, 850, "ibm-850", "Western (DOS Latin 1)" },
    { 0x0412, 0, 0, 852, "ibm-852", "Central European (DOS Latin 2)" },
    { 0x0413, 0, 0, 855, "ibm-855", "Cyrillic (DOS)" },
    { 0x0414, 0, 0, 857, "ibm-857", "Turkish (DOS)" },
    { 0x0415, 0, 0, 863, "ibm-863", "Canadian French (DOS)" },
    { 0x0416, 0, 0, 864, "ibm-864", "Arabic (DOS)" },
    { 0x0417, 0, 0, 865, "ibm-865", "Nordic (DOS)" },
    { 0x0418, 0, 0, 866, "ibm-866", "Russian (DOS)" },
    { 0x0419, 0, 0, 869, "ibm-869", "Greek (DOS Greek 2)" },
    { 0x041A, 0, 0, 874, "windows-874-2000", "Thai (Windows, DOS)" },
    { 0x0420, 0, 0, 932, "ibm-943_P15A-2003", "Japanese (Windows, DOS)" },
    { 0x0421, 0, 0, 936, "windows-936-2000", "Simplified Chinese (Windows, DOS)" },
    { 0x0422, 0, 0, 949, "windows-949-2000", "Korean (Windows, DOS)" },
    { 0x0423, 0, 0, 950, "windows-950-2000", "Traditional Chinese (Windows, DOS)" },
    { ENC_WIN1252, 0, 0, 1252, "windows-1252", "Western (Windows Latin 1)" },
    { 0x0501, 0, 0, 1250, "windows-1250", "Central European (Windows Latin 2)" },
    { 0x0502, 0, 0, 1251, "windows-1251", "Cyrillic (Windows)" },
    { 0x0503, 0, 0, 1253, "windows-1253", "Greek (Windows)" },
    { 0x0504, 0, 0, 1254, "windows-1254", "Turkish (Windows Latin 5)" },
    { 0x0505, 0, 0, 1255, "windows-1255", "Hebrew (Windows)" },
    { 0x0506, 0, 0, 1256, "windows-1256", "Arabic (Windows)" },
    { 0x0507, 0, 0, 1257, "windows-1257", "Baltic (Windows)" },
    { 0x0508, 0, 0, 1258, "windows-1258", "Vietnamese (Windows)" },
    { 0x0631, 0, 0, 0, "GBK", "Chinese (GBK)" },
    { 0x0632, 0, 0, 54936, "GB18030", "Chinese (GB 18030)" },
    { 0x0820, 0, 1, 50220, "ISO_2022,locale=ja,version=0", "Japanese (ISO 2022-JP)" },
    { 0x0821, 0, 1, 0, "ISO_2022,locale=ja,version=2", "Japanese (ISO 2022-JP-2)" },
    { 0x0830, 0, 1, 0, "ISO_2022,locale=zh,version=0", "Chinese (ISO 2022-CN)" },
    { 0x0840, 0, 1, 50225, "ISO_2022,locale=ko,version=0", "Korean (ISO 2022-KR)" },
    { 0x0920, 0, 0, 51932, "EUC-JP", "Japanese (EUC)" },
    { 0x0930, 0, 0, 51936, "GB2312", "Simplified Chinese (GB 2312)" },
    { 0x0931, 0, 0, 0, "EUC-TW", "Traditional Chinese (EUC)" },
    { 0x0940, 0, 0, 51949, "EUC-KR", "Korean (EUC)" },
    { 0x0A01, 0, 0, 0, "ibm-943_P130-1999", "Japanese (Shift JIS)" },
    { 0x0A02, 0, 0, 20866, "KOI8-R", "Cyrillic (KOI8-R)" },
    { 0x0A03, 0, 0, 0, "Big5", "Traditional Chinese (Big 5)" },
    { 0x0A05, 0, 1, 52936, "HZ", "Simplified Chinese (HZ GB 2312)" },
    { 0x0A06, 0, 0, 0, "Big5-HKSCS", "Traditional Chinese (Big 5 HKSCS)" },
    { 0x0A08, 0, 0, 21866, "KOI8-U", "Ukrainian (KOI8-U)" },
    { 0x0C01, 0, 0, 37, "ibm-37", "Western (EBCDIC Latin 1)" },
};
#define NT (sizeof T / sizeof T[0])

static const enc_t *find(uint32_t cf) {
    for (size_t i = 0; i < NT; i++) if (T[i].cf == cf) return &T[i];
    return 0;
}
static void *open_cnv(const enc_t *e) {
    const fdn_icu_t *I = fdn_icu();
    if (!e || !e->icu || !I || !I->ok || !I->ucnv_open) return 0;
    UErrorCode er = U_ZERO_ERROR;
    void *c = I->ucnv_open(e->icu, &er);
    if (!c || U_FAILURE(er)) { if (c) I->ucnv_close(c); return 0; }
    return c;
}

int enc_available(uint32_t cf) {
    const enc_t *e = find(cf);
    if (!e) return 0;
    if (e->kind) return 1;
    void *c = open_cnv(e);
    if (!c) return 0;
    fdn_icu()->ucnv_close(c);
    return 1;
}
const uint32_t *enc_list(void) {
    static uint32_t list[NT + 1];
    static int done;
    if (!done) {
        size_t k = 0;
        for (size_t i = 0; i < NT; i++) if (enc_available(T[i].cf)) list[k++] = T[i].cf;
        list[k] = ENC_INVALID;
        done = 1;
    }
    return list;
}
const char *enc_name(uint32_t cf) { const enc_t *e = find(cf); return e ? e->name : 0; }
int32_t enc_max_bytes(uint32_t cf, int32_t units) {
    const enc_t *e = find(cf);
    if (!e) return 0;
    switch (e->kind) {
    case 1: return units * 3;
    case 2: case 3: case 4: return units * 2;
    case 5: case 6: case 7: return units * 4;
    case 8: return units;
    case 9: return units * 6;
    }
    void *c = open_cnv(e);
    int m = c ? fdn_icu()->ucnv_getMaxCharSize(c) : 4;
    if (c) fdn_icu()->ucnv_close(c);
    return units * (m > 0 ? m : 4) + (e->stateful ? 8 : 0);
}
uint32_t enc_windows_codepage(uint32_t cf) { const enc_t *e = find(cf); return e ? e->win : 0; }
uint32_t enc_from_windows_codepage(uint32_t cp) {
    if (!cp) return ENC_INVALID;
    for (size_t i = 0; i < NT; i++) if (T[i].win == cp) return T[i].cf;
    return ENC_INVALID;
}
uint32_t enc_mac_compatible(uint32_t cf) {
    switch (cf) {
    case 0x0202: case 0x0204: case 0x0501: case 0x0412: case 0x1d: return 0x1d;
    case 0x0205: case 0x0502: case 0x0413: case 0x0418: case 0x0A02: case 0x0A08: case 0x07: return 0x07;
    case 0x0207: case 0x0503: case 0x0405: case 0x0419: case 0x06: return 0x06;
    case 0x0209: case 0x0504: case 0x0414: case 0x23: return 0x23;
    case 0x0920: case 0x0A01: case 0x0420: case 0x0820: case 0x0821: return 0x01;   /* MacJapanese */
    case 0x0930: case 0x0421: case 0x0631: case 0x0632: case 0x0A05: case 0x0830: return 0x19;   /* MacChineseSimp */
    case 0x0931: case 0x0423: case 0x0A03: case 0x0A06: return 0x02;   /* MacChineseTrad */
    case 0x0940: case 0x0422: case 0x0840: return 0x03;   /* MacKorean */
    case 0x0206: case 0x0506: case 0x0416: return 0x04;   /* MacArabic */
    case 0x0208: case 0x0505: return 0x05;   /* MacHebrew */
    case 0x020B: case 0x041A: return 0x15;   /* MacThai */
    }
    return find(cf) ? 0 : ENC_INVALID;      /* Unicode e latinas: Mac OS Roman */
}

/* ---------------------------------------------------------------- decodificação */
typedef struct { UChar *u; int32_t n, cap; } ubuf;
static void put(ubuf *b, uint32_t c) {
    if (b->n + 2 > b->cap) { b->cap = b->cap ? b->cap * 2 : 64; b->u = realloc(b->u, (size_t)b->cap * sizeof(UChar)); }
    if (c >= 0x10000) { c -= 0x10000; b->u[b->n++] = (UChar)(0xD800 | (c >> 10)); b->u[b->n++] = (UChar)(0xDC00 | (c & 0x3FF)); }
    else b->u[b->n++] = (UChar)c;
}
static UChar *done(ubuf *b, int32_t *on) {
    if (!b->u) b->u = malloc(sizeof(UChar));
    b->u[b->n] = 0;
    *on = b->n;
    return b->u;
}
static UChar *fail(ubuf *b) { free(b->u); return 0; }

static UChar *dec_utf8(const uint8_t *p, int32_t n, int32_t *on) {
    ubuf b = { 0, 0, 0 };
    b.cap = n + 1; b.u = malloc((size_t)b.cap * sizeof(UChar));
    int32_t i = 0;
    if (n >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) i = 3;
    while (i < n) {
        uint32_t c = p[i];
        int w;
        if (c < 0x80) w = 1;
        else if (c >= 0xC2 && c <= 0xDF) { c &= 0x1F; w = 2; }
        else if (c >= 0xE0 && c <= 0xEF) { c &= 0x0F; w = 3; }
        else if (c >= 0xF0 && c <= 0xF4) { c &= 0x07; w = 4; }
        else return fail(&b);
        if (i + w > n) return fail(&b);
        for (int k = 1; k < w; k++) {
            if ((p[i + k] & 0xC0) != 0x80) return fail(&b);
            c = (c << 6) | (p[i + k] & 0x3F);
        }
        if ((w == 3 && (c < 0x800 || (c >= 0xD800 && c <= 0xDFFF))) || (w == 4 && (c < 0x10000 || c > 0x10FFFF))) return fail(&b);
        put(&b, c);
        i += w;
    }
    return done(&b, on);
}
static UChar *dec_utf16(const uint8_t *p, int32_t n, int be, int bom, int32_t *on) {
    if (n % 2) return 0;
    ubuf b = { 0, 0, 0 };
    int32_t i = 0;
    if (bom && n >= 2) {
        if (p[0] == 0xFE && p[1] == 0xFF) { be = 1; i = 2; }
        else if (p[0] == 0xFF && p[1] == 0xFE) { be = 0; i = 2; }
    }
    b.cap = n / 2 + 1; b.u = malloc((size_t)b.cap * sizeof(UChar));
    for (; i + 1 < n; i += 2) b.u[b.n++] = be ? (UChar)(p[i] << 8 | p[i + 1]) : (UChar)(p[i] | p[i + 1] << 8);
    return done(&b, on);
}
static UChar *dec_utf32(const uint8_t *p, int32_t n, int be, int bom, int32_t *on) {
    if (n % 4) return 0;
    ubuf b = { 0, 0, 0 };
    int32_t i = 0;
    if (bom && n >= 4) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 0xFE && p[3] == 0xFF) { be = 1; i = 4; }
        else if (p[0] == 0xFF && p[1] == 0xFE && p[2] == 0 && p[3] == 0) { be = 0; i = 4; }
    }
    for (; i + 3 < n; i += 4) {
        uint32_t c = be ? (uint32_t)p[i] << 24 | (uint32_t)p[i + 1] << 16 | (uint32_t)p[i + 2] << 8 | p[i + 3]
                        : p[i] | (uint32_t)p[i + 1] << 8 | (uint32_t)p[i + 2] << 16 | (uint32_t)p[i + 3] << 24;
        if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) return fail(&b);
        put(&b, c);
    }
    return done(&b, on);
}
static int hexv(uint8_t c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
/* NonLossyASCII: \\ é a barra; \ooo (octal) até 0xFF; \uXXXX acima */
static UChar *dec_nonlossy(const uint8_t *p, int32_t n, int32_t *on) {
    ubuf b = { 0, 0, 0 };
    for (int32_t i = 0; i < n;) {
        if (p[i] >= 0x80) return fail(&b);
        if (p[i] != '\\') { put(&b, p[i++]); continue; }
        if (i + 1 < n && p[i + 1] == '\\') { put(&b, '\\'); i += 2; continue; }
        if (i + 5 < n + 0 && (p[i + 1] == 'u' || p[i + 1] == 'U')) {
            int v = 0, ok = 1;
            for (int k = 2; k < 6; k++) { int h = i + k < n ? hexv(p[i + k]) : -1; if (h < 0) { ok = 0; break; } v = v * 16 + h; }
            if (ok) {
                if (b.n + 1 > b.cap) { b.cap = b.cap ? b.cap * 2 : 64; b.u = realloc(b.u, (size_t)b.cap * sizeof(UChar)); }
                b.u[b.n++] = (UChar)v;   /* unidade a unidade: os pares substitutos vêm em dois \u */
                i += 6;
                continue;
            }
        }
        if (i + 3 < n && p[i + 1] >= '0' && p[i + 1] <= '3' && p[i + 2] >= '0' && p[i + 2] <= '7' && p[i + 3] >= '0' && p[i + 3] <= '7') {
            put(&b, (uint32_t)((p[i + 1] - '0') * 64 + (p[i + 2] - '0') * 8 + (p[i + 3] - '0')));
            i += 4;
            continue;
        }
        put(&b, '\\'); i++;     /* barra sozinha fica como está */
    }
    return done(&b, on);
}

UChar *enc_decode(const uint8_t *p, int32_t n, uint32_t cf, int32_t *on) {
    const enc_t *e = find(cf);
    *on = 0;
    if (!e || n < 0 || (!p && n)) return 0;
    switch (e->kind) {
    case 1: return dec_utf8(p, n, on);
    case 2: return dec_utf16(p, n, 0, 1, on);
    case 3: return dec_utf16(p, n, 1, 0, on);
    case 4: return dec_utf16(p, n, 0, 0, on);
    case 5: return dec_utf32(p, n, 0, 1, on);
    case 6: return dec_utf32(p, n, 1, 0, on);
    case 7: return dec_utf32(p, n, 0, 0, on);
    case 8: { ubuf b = { 0, 0, 0 }; for (int32_t i = 0; i < n; i++) { if (p[i] >= 0x80) return fail(&b); put(&b, p[i]); } return done(&b, on); }
    case 9: return dec_nonlossy(p, n, on);
    }
    void *c = open_cnv(e);
    if (!c) return 0;
    const fdn_icu_t *I = fdn_icu();
    UErrorCode er = U_ZERO_ERROR;
    I->ucnv_setToUCallBack(c, (const void *)I->UCNV_TO_U_CALLBACK_STOP, 0, 0, 0, &er);
    er = U_ZERO_ERROR;
    int32_t cap = n * 2 + 4;
    UChar *u = malloc((size_t)cap * sizeof(UChar));
    int32_t m = I->ucnv_toUChars(c, u, cap, (const char *)p, n, &er);
    if (er == U_BUFFER_OVERFLOW_ERROR) {
        free(u); cap = m + 1; u = malloc((size_t)cap * sizeof(UChar)); er = U_ZERO_ERROR;
        m = I->ucnv_toUChars(c, u, cap, (const char *)p, n, &er);
    }
    I->ucnv_close(c);
    if (U_FAILURE(er) && er != -124) { free(u); return 0; }   /* -124 = U_STRING_NOT_TERMINATED_WARNING */
    if (m < 0) m = 0;
    if (m >= cap) { u = realloc(u, (size_t)(m + 1) * sizeof(UChar)); }
    u[m] = 0;
    *on = m;
    return u;
}

/* ---------------------------------------------------------------- codificação */
typedef struct { uint8_t *buf; int32_t max, used; int full; } obuf;
static int emit(obuf *o, const uint8_t *t, int32_t k) {
    if (o->buf) {
        if (o->used + k > o->max) { o->full = 1; return 0; }
        memcpy(o->buf + o->used, t, (size_t)k);
    }
    o->used += k;
    return 1;
}
static uint32_t scalar(const UChar *u, int32_t n, int32_t i, int *w) {
    uint32_t c = u[i];
    *w = 1;
    if (c >= 0xD800 && c <= 0xDBFF && i + 1 < n && u[i + 1] >= 0xDC00 && u[i + 1] <= 0xDFFF) {
        c = 0x10000 + ((c - 0xD800) << 10) + (u[i + 1] - 0xDC00);
        *w = 2;
    }
    return c;
}

int32_t enc_encode(const UChar *u, int32_t n, uint32_t cf, uint8_t loss, int bom, uint8_t *buf, int32_t max, int32_t *used) {
    const enc_t *e = find(cf);
    obuf o = { buf, max, 0, 0 };
    int32_t i = 0;
    if (used) *used = 0;
    if (!e) return 0;
    if (bom && (e->kind == 2 || e->kind == 5)) {
        static const uint8_t b16[2] = { 0xFF, 0xFE }, b32[4] = { 0xFF, 0xFE, 0, 0 };
        if (!(e->kind == 2 ? emit(&o, b16, 2) : emit(&o, b32, 4))) { if (used) *used = 0; return 0; }
    }
    if (e->kind >= 1 && e->kind <= 9) {
        while (i < n) {
            int w;
            uint32_t c = scalar(u, n, i, &w);
            uint8_t t[24];
            int32_t k = 0;
            int lone = c >= 0xD800 && c <= 0xDFFF;
            switch (e->kind) {
            case 1:
                if (lone) { if (!loss) goto stop; t[k++] = loss; break; }
                if (c < 0x80) t[k++] = (uint8_t)c;
                else if (c < 0x800) { t[k++] = (uint8_t)(0xC0 | c >> 6); t[k++] = (uint8_t)(0x80 | (c & 0x3F)); }
                else if (c < 0x10000) { t[k++] = (uint8_t)(0xE0 | c >> 12); t[k++] = (uint8_t)(0x80 | ((c >> 6) & 0x3F)); t[k++] = (uint8_t)(0x80 | (c & 0x3F)); }
                else { t[k++] = (uint8_t)(0xF0 | c >> 18); t[k++] = (uint8_t)(0x80 | ((c >> 12) & 0x3F)); t[k++] = (uint8_t)(0x80 | ((c >> 6) & 0x3F)); t[k++] = (uint8_t)(0x80 | (c & 0x3F)); }
                break;
            case 2: case 4: case 3:
                w = 1;   /* UTF-16 passa unidade a unidade, até as soltas */
                if (e->kind == 3) { t[0] = (uint8_t)(u[i] >> 8); t[1] = (uint8_t)u[i]; }
                else { t[0] = (uint8_t)u[i]; t[1] = (uint8_t)(u[i] >> 8); }
                k = 2;
                break;
            case 5: case 6: case 7:
                if (lone) { if (!loss) goto stop; c = loss; }
                if (e->kind == 6) { t[0] = (uint8_t)(c >> 24); t[1] = (uint8_t)(c >> 16); t[2] = (uint8_t)(c >> 8); t[3] = (uint8_t)c; }
                else { t[0] = (uint8_t)c; t[1] = (uint8_t)(c >> 8); t[2] = (uint8_t)(c >> 16); t[3] = (uint8_t)(c >> 24); }
                k = 4;
                break;
            case 8:
                if (c >= 0x80) { if (!loss) goto stop; t[k++] = loss; } else t[k++] = (uint8_t)c;
                break;
            case 9: {
                static const char hx[] = "0123456789abcdef";
                w = 1;
                uint32_t v = u[i];
                if (v == '\\') { t[k++] = '\\'; t[k++] = '\\'; }
                else if (v < 0x80) t[k++] = (uint8_t)v;
                else if (v < 0x100) { t[k++] = '\\'; t[k++] = (uint8_t)('0' + (v >> 6)); t[k++] = (uint8_t)('0' + ((v >> 3) & 7)); t[k++] = (uint8_t)('0' + (v & 7)); }
                else { t[k++] = '\\'; t[k++] = 'u'; t[k++] = (uint8_t)hx[v >> 12]; t[k++] = (uint8_t)hx[(v >> 8) & 15]; t[k++] = (uint8_t)hx[(v >> 4) & 15]; t[k++] = (uint8_t)hx[v & 15]; }
                break;
            }
            }
            if (!emit(&o, t, k)) break;
            i += w;
        }
    stop:
        if (used) *used = o.used;
        return i;
    }
    /* ICU */
    void *c = open_cnv(e);
    if (!c) return 0;
    const fdn_icu_t *I = fdn_icu();
    UErrorCode er = U_ZERO_ERROR;
    I->ucnv_setFromUCallBack(c, (const void *)I->UCNV_FROM_U_CALLBACK_STOP, 0, 0, 0, &er);
    if (e->stateful) {
        /* com estado (ISO-2022, HZ, UTF-7): o texto todo de uma vez */
        er = U_ZERO_ERROR;
        int32_t need = I->ucnv_fromUChars(c, 0, 0, u, n, &er);
        if (er == U_BUFFER_OVERFLOW_ERROR || er == U_ZERO_ERROR || er == -124) {
            if (!buf) { o.used = need; i = n; }
            else if (need <= max) {
                er = U_ZERO_ERROR;
                char *tmp = malloc((size_t)need + 1);
                I->ucnv_fromUChars(c, tmp, need + 1, u, n, &er);
                memcpy(buf, tmp, (size_t)need);
                free(tmp);
                o.used = need; i = n;
            }
        } else if (loss) {
            char sub = (char)loss;
            er = U_ZERO_ERROR;
            I->ucnv_setFromUCallBack(c, 0, 0, 0, 0, &er);   /* volta ao padrão: substitui */
            er = U_ZERO_ERROR;
            I->ucnv_setSubstChars(c, &sub, 1, &er);
            er = U_ZERO_ERROR;
            int32_t m = I->ucnv_fromUChars(c, 0, 0, u, n, &er);
            if (!buf) { o.used = m; i = n; }
            else if (m <= max) {
                char *tmp = malloc((size_t)m + 1);
                er = U_ZERO_ERROR;
                I->ucnv_fromUChars(c, tmp, m + 1, u, n, &er);
                memcpy(buf, tmp, (size_t)m);
                free(tmp);
                o.used = m; i = n;
            }
        }
        I->ucnv_close(c);
        if (used) *used = o.used;
        return i;
    }
    while (i < n) {
        int w;
        scalar(u, n, i, &w);
        char t[16];
        er = U_ZERO_ERROR;
        int32_t k = I->ucnv_fromUChars(c, t, sizeof t, u + i, w, &er);
        if (U_FAILURE(er) || k <= 0) {
            if (!loss) break;
            t[0] = (char)loss; k = 1;
        }
        if (!emit(&o, (const uint8_t *)t, k)) break;
        i += w;
    }
    I->ucnv_close(c);
    if (used) *used = o.used;
    return i;
}

int enc_lossless(const UChar *u, int32_t n, uint32_t cf) {
    if (!find(cf)) return 0;
    return enc_encode(u, n, cf, 0, 0, 0, 0, 0) == n;
}

uint32_t enc_detect(const uint8_t *b, int32_t n, const uint32_t *sugg, int nsugg, int only_sugg) {
    if (n >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) return ENC_UTF8;
    if (n >= 4 && ((b[0] == 0xFF && b[1] == 0xFE && b[2] == 0 && b[3] == 0) || (b[0] == 0 && b[1] == 0 && b[2] == 0xFE && b[3] == 0xFF))) return ENC_UTF32;
    if (n >= 2 && ((b[0] == 0xFF && b[1] == 0xFE) || (b[0] == 0xFE && b[1] == 0xFF))) return ENC_UTF16;
    int32_t m;
    for (int k = 0; k < nsugg; k++) {
        UChar *u = enc_decode(b, n, sugg[k], &m);
        if (u) { free(u); return sugg[k]; }
    }
    if (only_sugg) return ENC_INVALID;
    UChar *u = enc_decode(b, n, ENC_UTF8, &m);
    if (u) { free(u); return ENC_UTF8; }
    return ENC_INVALID;
}
