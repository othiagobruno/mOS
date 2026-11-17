/*
 * mOS libSystem — núcleo do CoreServices antigo (CarbonCore) sem CF/ObjC, incluído por cs_legacy.c e pelo teste de
 * host tools/cs_legacy_host.c:
 *  - UCCollator (UnicodeUtilities.h) sobre a collation da ICU (fdn_icu.h): opções do Carbon viram força/atributos
 *    da ICU, e a chave de collation (UCCollationValue[]) é a sort key da ICU, um byte por valor;
 *  - Text Encoding Conversion (TextToUnicodeInfo, UnicodeToTextInfo, TECObjectRef) sobre fdn_encoding_core
 *    (TextEncoding do Carbon = CFStringEncoding, mesma numeração);
 *  - Memory Manager (Handle/Ptr com tamanho e estado), MemError;
 *  - tabela de erros do OSStatus (GetMacOSStatusErrorString/CommentString).
 * Quem inclui já declarou malloc/realloc/free/memcpy/memset/memcmp/strlen e incluiu fdn_encoding_core.h.
 */
typedef int cs_status;
typedef unsigned short cs_unichar;

/* ================================================================ UCCollator */
enum {
    kUCCollateComposeInsensitiveMask = 1u << 1, kUCCollateWidthInsensitiveMask = 1u << 2,
    kUCCollateCaseInsensitiveMask = 1u << 3, kUCCollateDiacritInsensitiveMask = 1u << 4,
    kUCCollatePunctuationSignificantMask = 1u << 15, kUCCollateDigitsOverrideMask = 1u << 16,
    kUCCollateDigitsAsNumberMask = 1u << 17,
};
#define kUCOutputBufferTooSmall (-25340)
#define cs_paramErr (-50)
#define cs_memFullErr (-108)
typedef struct { unsigned magic; unsigned opts; char locale[64]; void *col; } cs_collator;
#define CS_COL_MAGIC 0x55436f6cu   /* 'UCol' */

/* força: diacrítico ignorado = primária (0); só caixa ignorada = secundária (1); senão terciária (2).
 * Pontuação: sem kUCCollatePunctuationSignificantMask o Carbon a ignora (UCOL_ALTERNATE_HANDLING = SHIFTED). */
static void *cs_col_open(const char *loc, unsigned opts) {
    const fdn_icu_t *I = fdn_icu();
    if (!I || !I->ok || !I->ucol_open) return 0;
    UErrorCode e = U_ZERO_ERROR;
    void *c = I->ucol_open(loc && *loc ? loc : "", &e);
    if (!c || U_FAILURE(e)) { if (c) I->ucol_close(c); return 0; }
    int32_t strength = (opts & kUCCollateDiacritInsensitiveMask) ? 0 : (opts & kUCCollateCaseInsensitiveMask) ? 1 : 2;
    I->ucol_setStrength(c, strength);
    I->ucol_setAttribute(c, 1 /* UCOL_ALTERNATE_HANDLING */, (opts & kUCCollatePunctuationSignificantMask) ? 21 : 20, &e);
    I->ucol_setAttribute(c, 7 /* UCOL_NUMERIC_COLLATION */, (opts & kUCCollateDigitsAsNumberMask) ? 17 : 16, &e);
    I->ucol_setAttribute(c, 4 /* UCOL_NORMALIZATION_MODE */, 17, &e);       /* composto = decomposto, como o Carbon */
    return c;
}
static cs_status cs_collator_new(const char *loc, unsigned opts, cs_collator **out) {
    if (!out) return cs_paramErr;
    *out = 0;
    cs_collator *k = calloc(1, sizeof *k);
    if (!k) return cs_memFullErr;
    k->magic = CS_COL_MAGIC;
    k->opts = opts;
    if (loc) { size_t n = strlen(loc); if (n >= sizeof k->locale) n = sizeof k->locale - 1; memcpy(k->locale, loc, n); }
    k->col = cs_col_open(k->locale, opts);
    if (!k->col) { free(k); return -25341; }                   /* kUCInvalidCollatorRef: sem ICU */
    *out = k;
    return 0;
}
static void cs_collator_free(cs_collator *k) {
    if (!k || k->magic != CS_COL_MAGIC) return;
    const fdn_icu_t *I = fdn_icu();
    if (k->col && I) I->ucol_close(k->col);
    k->magic = 0;
    free(k);
}
static cs_status cs_collate(cs_collator *k, const cs_unichar *a, unsigned long an, const cs_unichar *b, unsigned long bn,
                            unsigned char *equiv, int *order) {
    if (!k || k->magic != CS_COL_MAGIC || (!a && an) || (!b && bn)) return cs_paramErr;
    const fdn_icu_t *I = fdn_icu();
    int32_t r = I->ucol_strcoll(k->col, a, (int32_t)an, b, (int32_t)bn);
    if (order) *order = r < 0 ? -1 : r > 0;
    if (equiv) *equiv = r == 0;
    return 0;
}
static cs_status cs_collation_key(cs_collator *k, const cs_unichar *t, unsigned long n, unsigned long max, unsigned long *got,
                                  unsigned *key) {
    if (!k || k->magic != CS_COL_MAGIC || (!t && n) || !got) return cs_paramErr;
    const fdn_icu_t *I = fdn_icu();
    uint8_t buf[512], *p = buf;
    int32_t need = I->ucol_getSortKey(k->col, t, (int32_t)n, buf, sizeof buf);
    if (need > (int32_t)sizeof buf) {
        p = malloc((size_t)need);
        if (!p) return cs_memFullErr;
        I->ucol_getSortKey(k->col, t, (int32_t)n, p, need);
    }
    if (need > 0 && !p[need - 1]) need--;                       /* o 0 do fim não entra na chave */
    *got = (unsigned long)need;
    cs_status st = 0;
    if ((unsigned long)need > max) st = kUCOutputBufferTooSmall;
    else for (int32_t i = 0; i < need; i++) key[i] = p[i];
    if (p != buf) free(p);
    return st;
}
static void cs_compare_keys(const unsigned *a, unsigned long an, const unsigned *b, unsigned long bn, unsigned char *equiv, int *order) {
    unsigned long i = 0;
    int r = 0;
    for (; i < an && i < bn && !r; i++) if (a[i] != b[i]) r = a[i] < b[i] ? -1 : 1;
    if (!r && an != bn) r = an < bn ? -1 : 1;
    if (order) *order = r;
    if (equiv) *equiv = r == 0;
}

/* ================================================================ Text Encoding Conversion */
/* TextEncoding = base | variante << 16 | formato << 26 (TextCommon.h) */
static unsigned cs_text_encoding(unsigned base, unsigned variant, unsigned format) { return (base & 0xffff) | (variant & 0x3ff) << 16 | (format & 0x3f) << 26; }
#define kTextUnsupportedEncodingErr (-8738)
#define kTextMalformedInputErr (-8739)
#define kTECPartialCharErr (-8753)
#define kTECUnmappableElementErr (-8754)
#define kTECOutputBufferFullStatus (-8785)
#define kTECUsedFallbacksStatus (-8783)
#define kTextEncodingUnicodeDefault 0x0100
/* Unicode do lado "de dentro": a numeração do Carbon trata UTF-16 nativo como kTextEncodingUnicodeDefault */
static unsigned cs_cf_enc(unsigned te) {
    unsigned base = te & 0xffff, fmt = (te >> 26) & 0x3f;
    if (base == 0x0100 || base == 0x0103 || base == 0x0104) {                 /* Unicode 2.x/3.x/4.x/default */
        if (fmt == 2) return ENC_UTF8;                                      /* kUnicodeUTF8Format */
        if (fmt == 3) return ENC_UTF32;                                     /* kUnicodeUTF32Format */
        return ENC_UTF16;
    }
    return te & ~(0x3fu << 26);                                             /* o CF guarda variante, não formato */
}
typedef struct { unsigned magic, from, to; uint8_t pend[8]; int npend; } cs_tec;
#define CS_TEC_MAGIC 0x54454321u
/* quantos bytes do fim podem ser o começo de um caractere incompleto (UTF-8, UTF-16, UTF-32 e multibyte ICU) */
static int cs_tail_partial(const uint8_t *b, int32_t n, unsigned cf) {
    if (cf == ENC_UTF8) {
        int k = 0;
        for (int i = n - 1; i >= 0 && i >= n - 3; i--) {
            k++;
            if ((b[i] & 0xc0) != 0x80) {
                int need = b[i] >= 0xf0 ? 4 : b[i] >= 0xe0 ? 3 : b[i] >= 0xc0 ? 2 : 1;
                return need > k ? k : 0;
            }
        }
        return 0;
    }
    if (cf == ENC_UTF16 || cf == ENC_UTF16BE || cf == ENC_UTF16LE) return n & 1;
    if (cf == ENC_UTF32 || cf == ENC_UTF32BE || cf == ENC_UTF32LE) return n & 3;
    return 0;
}
/* quantos bytes da codificação cf geram as k primeiras unidades UTF-16 (recodificando-as) */
static unsigned long cs_src_bytes(const UChar *u, int32_t k, unsigned cf) {
    if (k <= 0) return 0;
    int32_t bytes = 0;
    enc_encode(u, k, cf, 0, 0, 0, 0, &bytes);                   /* devolve as unidades; os bytes saem em bytes */
    return bytes > 0 ? (unsigned long)bytes : 0;
}
/* bytes de "from" -> UTF-16. read = bytes consumidos (o resto incompleto fica para a próxima chamada) */
static cs_status cs_to_unicode(unsigned from, const uint8_t *src, unsigned long n, unsigned long *read, cs_unichar *out,
                               unsigned long outBytes, unsigned long *outLen) {
    unsigned cf = cs_cf_enc(from);
    if (read) *read = 0;
    if (outLen) *outLen = 0;
    if (!enc_available(cf)) return kTextUnsupportedEncodingErr;
    int32_t keep = cs_tail_partial(src, (int32_t)n, cf), take = (int32_t)n - keep, un = 0;
    UChar *u = take ? enc_decode(src, take, cf, &un) : 0;
    /* multibyte (Shift-JIS, EUC, GB...): sem o fim, decodifica? então o fim é um caractere incompleto */
    for (int k = 1; take && !u && k <= 3 && k < take; k++) {
        u = enc_decode(src, take - k, cf, &un);
        if (u) { keep += k; take -= k; }
    }
    if (take && !u) return kTextMalformedInputErr;
    cs_status st = keep ? kTECPartialCharErr : 0;
    unsigned long cap = outBytes / 2;
    if ((unsigned long)un > cap) {
        /* não cabe: entrega cap unidades (sem partir um par substituto) e conta os bytes de entrada que elas gastam */
        int32_t k = (int32_t)cap;
        if (k > 0 && (u[k - 1] & 0xfc00) == 0xd800) k--;
        if (k) memcpy(out, u, (size_t)k * 2);
        if (read) *read = cs_src_bytes(u, k, cf);
        if (outLen) *outLen = (unsigned long)k * 2;
        free(u);
        return kTECOutputBufferFullStatus;
    }
    if (un) memcpy(out, u, (size_t)un * 2);
    free(u);
    if (read) *read = (unsigned long)take;
    if (outLen) *outLen = (unsigned long)un * 2;
    return st;
}
/* UTF-16 -> bytes de "to". Sem fallback, caractere que não existe na codificação = kTECUnmappableElementErr */
static cs_status cs_from_unicode(unsigned to, const cs_unichar *src, unsigned long srcBytes, int lossy, unsigned long *read,
                                 uint8_t *out, unsigned long outCap, unsigned long *outLen) {
    unsigned cf = cs_cf_enc(to);
    unsigned long n = srcBytes / 2;
    if (read) *read = 0;
    if (outLen) *outLen = 0;
    if (!enc_available(cf)) return kTextUnsupportedEncodingErr;
    if (n && (src[n - 1] & 0xfc00) == 0xd800) n--;             /* metade de um par substituto fica para depois */
    if (!lossy && !enc_lossless(src, (int32_t)n, cf)) return kTECUnmappableElementErr;
    int32_t bytes = 0, units = enc_encode(src, (int32_t)n, cf, lossy ? '?' : 0, 0, out, (int32_t)outCap, &bytes);
    if (units < 0) return kTextMalformedInputErr;
    if (read) *read = (unsigned long)units * 2;
    if (outLen) *outLen = (unsigned long)bytes;
    if ((unsigned long)units < n) return kTECOutputBufferFullStatus;
    return n * 2 < srcBytes ? kTECPartialCharErr : 0;
}
/* TEC: de uma codificação para outra, passando por UTF-16 */
static cs_status cs_tec_convert(cs_tec *t, const uint8_t *in, unsigned long n, unsigned long *read, uint8_t *out, unsigned long cap,
                                unsigned long *wrote) {
    if (read) *read = 0;
    if (wrote) *wrote = 0;
    if (!t || t->magic != CS_TEC_MAGIC) return cs_paramErr;
    unsigned long tot = (unsigned long)t->npend + n;
    uint8_t *buf = malloc(tot ? tot : 1);
    if (!buf) return cs_memFullErr;
    memcpy(buf, t->pend, (size_t)t->npend);
    if (n) memcpy(buf + t->npend, in, n);
    cs_unichar *u = malloc((tot + 2) * 4);
    if (!u) { free(buf); return cs_memFullErr; }
    unsigned long rd = 0, ul = 0, ur = 0, w = 0;
    cs_status st = cs_to_unicode(t->from, buf, tot, &rd, u, (tot + 2) * 4, &ul);
    if (st == kTECPartialCharErr) st = 0;
    if (st) { free(buf); free(u); return st; }
    st = cs_from_unicode(t->to, u, ul, 0, &ur, out, cap, &w);
    if (st == kTECOutputBufferFullStatus || ur < ul) {
        rd = cs_src_bytes(u, (int32_t)(ur / 2), cs_cf_enc(t->from));   /* entrada gasta pelas unidades que saíram */
        st = kTECOutputBufferFullStatus;
    } else if (st) { free(buf); free(u); return st; }
    /* o que sobrou sem converter (caractere incompleto no fim) fica guardado no conversor */
    unsigned long left = tot - rd;
    if (st != kTECOutputBufferFullStatus && left <= sizeof t->pend) { memcpy(t->pend, buf + rd, left); t->npend = (int)left; rd = tot; }
    else t->npend = 0;
    unsigned long from_in = rd > (unsigned long)0 ? rd : 0;
    if (read) *read = from_in >= tot - n ? from_in - (tot - n) : 0;
    if (wrote) *wrote = w;
    free(buf); free(u);
    return st;
}

/* ================================================================ Memory Manager */
/* Handle = ponteiro para o ponteiro-mestre. O bloco do mestre guarda tamanho e estado (bit 7 = travado). */
typedef struct { char *p; unsigned long size; signed char state; } cs_master;
static short cs_memerr;
#define cs_nilHandleErr (-109)
static char **cs_new_handle(unsigned long size, int clear) {
    cs_master *m = calloc(1, sizeof *m);
    if (!m) { cs_memerr = -108; return 0; }
    m->p = size ? (clear ? calloc(1, size) : malloc(size)) : 0;
    if (size && !m->p) { free(m); cs_memerr = -108; return 0; }
    m->size = size;
    cs_memerr = 0;
    return &m->p;
}
static cs_master *cs_master_of(char **h) { return (cs_master *)h; }   /* p é o primeiro campo */
static void cs_dispose_handle(char **h) { if (!h) { cs_memerr = cs_nilHandleErr; return; } cs_master *m = cs_master_of(h); free(m->p); free(m); cs_memerr = 0; }
static long cs_handle_size(char **h) { if (!h) { cs_memerr = cs_nilHandleErr; return 0; } cs_memerr = 0; return (long)cs_master_of(h)->size; }
static void cs_set_handle_size(char **h, long size) {
    if (!h || size < 0) { cs_memerr = h ? -50 : cs_nilHandleErr; return; }
    cs_master *m = cs_master_of(h);
    if ((m->state & 0x80) && (unsigned long)size > m->size) { cs_memerr = -117; return; }   /* memLockedErr: travado não cresce */
    char *n = size ? realloc(m->p, (size_t)size) : (free(m->p), (char *)0);
    if (size && !n) { cs_memerr = -108; return; }
    if ((unsigned long)size > m->size) memset(n + m->size, 0, (size_t)size - m->size);
    m->p = n; m->size = (unsigned long)size;
    cs_memerr = 0;
}
/* Ptr: tamanho guardado antes dos dados (16 bytes para manter o alinhamento) */
static char *cs_new_ptr(unsigned long size, int clear) {
    unsigned long *b = clear ? calloc(1, size + 16) : malloc(size + 16);
    if (!b) { cs_memerr = -108; return 0; }
    b[0] = size;
    cs_memerr = 0;
    return (char *)(b + 2);
}
static unsigned long cs_ptr_size(char *p) { return p ? ((unsigned long *)p)[-2] : 0; }
static void cs_dispose_ptr(char *p) { if (p) free((unsigned long *)p - 2); cs_memerr = 0; }

/* ================================================================ OSStatus -> nome e comentário (MacErrors.h) */
static const struct { int code; const char *name, *comment; } cs_errs[] = {
    {0, "noErr", "No error"},
    {-34, "dskFulErr", "disk full"},
    {-35, "nsvErr", "no such volume"},
    {-36, "ioErr", "I/O error (bummers)"},
    {-37, "bdNamErr", "bad file name passed to routine; there may be no bad names in the final system!"},
    {-39, "eofErr", "End of file"},
    {-43, "fnfErr", "File not found"},
    {-47, "fBsyErr", "File is busy (delete)"},
    {-48, "dupFNErr", "duplicate filename (rename)"},
    {-49, "opWrErr", "file already open with with write permission"},
    {-50, "paramErr", "error in user parameter list"},
    {-54, "permErr", "permissions error (on file open)"},
    {-61, "wrPermErr", "write permissions error"},
    {-108, "memFullErr", "Not enough room in heap zone"},
    {-109, "nilHandleErr", "Master Pointer was NIL in HandleZone or other"},
    {-117, "memLockedErr", "trying to move a locked block (MoveHHi)"},
    {-120, "dirNFErr", "Directory not found"},
    {-128, "userCanceledErr", ""},
    {-192, "resNotFound", "Resource not found"},
    {-1700, "errAECoercionFail", "bad parameter data or unable to coerce the data supplied"},
    {-1708, "errAEEventNotHandled", "the AppleEvent was not handled by any handler"},
    {-1712, "errAETimeout", "the AppleEvent timed out"},
    {-1728, "errAENoSuchObject", "e.g.,: specifier asked for the 3rd, but there are only 2. Basically, this indicates a run-time resolution error."},
    {-1743, "errAEEventNotPermitted", ""},
    {-5551, "gestaltUndefSelectorErr", "undefined selector was passed to Gestalt"},
    {-8738, "kTextUnsupportedEncodingErr", "specified encoding not supported for this operation"},
    {-8739, "kTextMalformedInputErr", "in DBCS, for example, high byte followed by invalid low byte"},
    {-10810, "kLSUnknownErr", ""},
    {-10814, "kLSApplicationNotFoundErr", ""},
    {-25291, "errSecNotAvailable", "No keychain is available. You may need to restart your computer."},
    {-25300, "errSecItemNotFound", "The specified item could not be found in the keychain."},
};
static int cs_err_index(int code) {
    for (unsigned i = 0; i < sizeof cs_errs / sizeof cs_errs[0]; i++) if (cs_errs[i].code == code) return (int)i;
    return -1;
}
