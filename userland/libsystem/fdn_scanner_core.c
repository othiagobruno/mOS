/*
 * mOS libSystem — núcleo do NSScanner (ver fdn_scanner_core.h). Busca de texto com caixa e equivalência
 * canônica pelo fdn_text_core (a mesma do NSString); números com a sintaxe e a saturação do macOS.
 */
#include "fdn_scanner_core.h"
#include "fdn_text_core.h"

double strtod(const char *s, char **end);
double ldexp(double x, int e);

static uint32_t cp(const UChar *s, int32_t n, int32_t i, int32_t *w) {
    uint32_t c = s[i];
    *w = 1;
    if (c >= 0xD800 && c <= 0xDBFF && i + 1 < n && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF) {
        c = 0x10000 + ((c - 0xD800) << 10) + (s[i + 1] - 0xDC00);
        *w = 2;
    }
    return c;
}

int32_t scn_skipped(const scn_t *sc) {
    int32_t i = sc->pos, w;
    if (i < 0) i = 0;
    if (!sc->skip) return i;
    while (i < sc->n && sc->skip(sc->skip_ctx, cp(sc->s, sc->n, i, &w))) i += w;
    return i;
}

int scn_at_end(const scn_t *sc) { return scn_skipped(sc) >= sc->n; }

int scn_string(scn_t *sc, const UChar *str, int32_t n, int32_t *start, int32_t *len) {
    int32_t i = scn_skipped(sc), l = 0, m = 0;
    if (n <= 0 || i >= sc->n) return 0;
    unsigned o = TX_ANCHORED | (sc->case_sensitive ? 0 : TX_CASE);
    if (!tx_search(sc->s + i, sc->n - i, str, n, o, &l, &m) || l != 0 || m <= 0) return 0;
    *start = i; *len = m;
    sc->pos = i + m;
    return 1;
}

int scn_upto_string(scn_t *sc, const UChar *str, int32_t n, int32_t *start, int32_t *len) {
    int32_t i = scn_skipped(sc), l = 0, m = 0;
    if (i >= sc->n) return 0;
    unsigned o = sc->case_sensitive ? 0 : TX_CASE;
    int32_t end = (n > 0 && tx_search(sc->s + i, sc->n - i, str, n, o, &l, &m)) ? i + l : sc->n;
    if (end <= i) return 0;
    *start = i; *len = end - i;
    sc->pos = end;
    return 1;
}

static int run(scn_t *sc, scn_member_fn set, void *ctx, int want, int32_t *start, int32_t *len) {
    int32_t i = scn_skipped(sc), j = i, w;
    while (j < sc->n && (set(ctx, cp(sc->s, sc->n, j, &w)) != 0) == want) j += w;
    if (j <= i) return 0;
    *start = i; *len = j - i;
    sc->pos = j;
    return 1;
}
int scn_chars(scn_t *sc, scn_member_fn set, void *ctx, int32_t *start, int32_t *len) { return run(sc, set, ctx, 1, start, len); }
int scn_upto_chars(scn_t *sc, scn_member_fn set, void *ctx, int32_t *start, int32_t *len) { return run(sc, set, ctx, 0, start, len); }

static int dig(UChar c) { return c >= '0' && c <= '9' ? c - '0' : -1; }
static int hexd(UChar c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int scn_i64(scn_t *sc, long long lo, long long hi, long long *out) {
    int32_t i = scn_skipped(sc);
    int neg = 0;
    if (i < sc->n && (sc->s[i] == '-' || sc->s[i] == '+')) { neg = sc->s[i] == '-'; i++; }
    if (i >= sc->n || dig(sc->s[i]) < 0) return 0;
    /* acumula em magnitude sem sinal; satura no limite do tipo pedido */
    unsigned long long lim = neg ? (unsigned long long)(-(lo + 1)) + 1 : (unsigned long long)hi, v = 0;
    int over = 0;
    for (; i < sc->n && dig(sc->s[i]) >= 0; i++) {
        unsigned d = (unsigned)dig(sc->s[i]);
        if (over) continue;
        if (v > (lim - d) / 10) over = 1; else v = v * 10 + d;
    }
    long long r;
    if (over) r = neg ? lo : hi;
    else r = neg ? (v == lim ? lo : -(long long)v) : (long long)v;
    if (out) *out = r;
    sc->pos = i;
    return 1;
}

int scn_u64(scn_t *sc, unsigned long long *out) {
    int32_t i = scn_skipped(sc);
    int neg = 0;
    if (i < sc->n && (sc->s[i] == '-' || sc->s[i] == '+')) { neg = sc->s[i] == '-'; i++; }
    if (i >= sc->n || dig(sc->s[i]) < 0) return 0;
    unsigned long long v = 0;
    int over = 0;
    for (; i < sc->n && dig(sc->s[i]) >= 0; i++) {
        unsigned d = (unsigned)dig(sc->s[i]);
        if (over) continue;
        if (v > (~0ULL - d) / 10) over = 1; else v = v * 10 + d;
    }
    if (over) v = ~0ULL;
    else if (neg) v = 0ULL - v;   /* como o strtoull do macOS: "-1" vira o complemento */
    if (out) *out = v;
    sc->pos = i;
    return 1;
}

int scn_hex(scn_t *sc, unsigned long long max, unsigned long long *out) {
    int32_t i = scn_skipped(sc);
    /* "0x" só é prefixo se vier dígito depois; senão o "0" sozinho é o número */
    if (i + 2 < sc->n && sc->s[i] == '0' && (sc->s[i + 1] == 'x' || sc->s[i + 1] == 'X') && hexd(sc->s[i + 2]) >= 0)
        i += 2;
    if (i >= sc->n || hexd(sc->s[i]) < 0) return 0;
    unsigned long long v = 0;
    int over = 0;
    for (; i < sc->n && hexd(sc->s[i]) >= 0; i++) {
        if (over) continue;
        if (v > (max >> 4)) { over = 1; continue; }
        v = (v << 4) | (unsigned)hexd(sc->s[i]);
        if (v > max) over = 1;
    }
    if (out) *out = over ? max : v;
    sc->pos = i;
    return 1;
}

int scn_double(scn_t *sc, double *out) {
    int32_t i = scn_skipped(sc), j = i, digits = 0;
    char buf[512];
    int k = 0;
    if (j < sc->n && (sc->s[j] == '-' || sc->s[j] == '+')) buf[k++] = (char)sc->s[j++];
    for (; j < sc->n && dig(sc->s[j]) >= 0; j++, digits++) if (k < 500) buf[k++] = (char)sc->s[j];
    if (j < sc->n && sc->s[j] == sc->dec_sep) {
        int32_t q = j + 1, fd = 0;
        while (q < sc->n && dig(sc->s[q]) >= 0) q++, fd++;
        if (digits || fd) {
            if (k < 500) buf[k++] = '.';
            for (j++; j < q; j++) if (k < 500) buf[k++] = (char)sc->s[j];
            digits += fd;
        }
    }
    if (!digits) return 0;
    /* expoente só conta se tiver dígito: "1e" lê 1 e para no e */
    if (j < sc->n && (sc->s[j] == 'e' || sc->s[j] == 'E')) {
        int32_t q = j + 1;
        if (q < sc->n && (sc->s[q] == '-' || sc->s[q] == '+')) q++;
        if (q < sc->n && dig(sc->s[q]) >= 0) {
            buf[k++] = 'e';
            if (q > j + 1) buf[k++] = (char)sc->s[j + 1];
            int ed = 0;
            for (j = q; j < sc->n && dig(sc->s[j]) >= 0; j++) 
                if (ed < 6 && (ed || sc->s[j] != '0')) { buf[k++] = (char)sc->s[j]; ed++; }
        }
    }
    buf[k] = 0;
    if (out) *out = strtod(buf, 0);
    sc->pos = j;
    return 1;
}

int scn_hex_double(scn_t *sc, double *out) {
    int32_t i = scn_skipped(sc), j = i;
    int neg = 0, digits = 0, shift = 0, sticky = 0;
    if (j < sc->n && (sc->s[j] == '-' || sc->s[j] == '+')) { neg = sc->s[j] == '-'; j++; }
    if (j + 1 < sc->n && sc->s[j] == '0' && (sc->s[j + 1] == 'x' || sc->s[j + 1] == 'X')) {
        int32_t q = j + 2;
        if ((q < sc->n && hexd(sc->s[q]) >= 0) || (q + 1 < sc->n && sc->s[q] == sc->dec_sep && hexd(sc->s[q + 1]) >= 0)) j = q;
    }
    /* mantissa em 60 bits; o que sobra vira expoente (e um bit "grudento" para o arredondamento) */
    unsigned long long m = 0;
    for (; j < sc->n && hexd(sc->s[j]) >= 0; j++, digits++) {
        if (m >> 56) { shift += 4; if (hexd(sc->s[j])) sticky = 1; }
        else m = (m << 4) | (unsigned)hexd(sc->s[j]);
    }
    if (j < sc->n && sc->s[j] == sc->dec_sep) {
        int32_t q = j + 1;
        int fd = 0;
        while (q < sc->n && hexd(sc->s[q]) >= 0) q++, fd++;
        if (digits || fd) {
            for (j++; j < q; j++, digits++) {
                if (m >> 56) { if (hexd(sc->s[j])) sticky = 1; }
                else { m = (m << 4) | (unsigned)hexd(sc->s[j]); shift -= 4; }
            }
        }
    }
    if (!digits) return 0;
    if (j < sc->n && (sc->s[j] == 'p' || sc->s[j] == 'P')) {
        int32_t q = j + 1;
        int eneg = 0;
        if (q < sc->n && (sc->s[q] == '-' || sc->s[q] == '+')) { eneg = sc->s[q] == '-'; q++; }
        if (q < sc->n && dig(sc->s[q]) >= 0) {
            long e = 0;
            for (; q < sc->n && dig(sc->s[q]) >= 0; q++) if (e < 100000) e = e * 10 + dig(sc->s[q]);
            shift += eneg ? -(int)e : (int)e;
            j = q;
        }
    }
    if (sticky) m |= 1;
    double v = ldexp((double)m, shift);
    if (out) *out = neg ? -v : v;
    sc->pos = j;
    return 1;
}

int32_t scn_ascii_prefix(const scn_t *sc, char *buf, int32_t cap, int32_t *from) {
    int32_t i = scn_skipped(sc), k = 0;
    *from = i;
    for (; i < sc->n && k + 1 < cap; i++) {
        UChar c = sc->s[i];
        if (c == sc->dec_sep) c = '.';
        else if (c == '.' && sc->dec_sep != '.') break;
        if (c >= 0x80) break;
        buf[k++] = (char)c;
    }
    buf[k] = 0;
    return k;
}
