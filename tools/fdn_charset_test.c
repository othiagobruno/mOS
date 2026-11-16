/* Teste de host das faixas do NSCharacterSet (fdn_charset_core.c) com a ICU do Linux (FDN_ICU_HOST). */
#include <stdio.h>
#include <stdlib.h>
#include "fdn_charset_core.h"
static int ok, bad;
static void eq(const char *w, long a, long b) { if (a == b) ok++; else { bad++; printf("FALHOU %s: %ld (queria %ld)\n", w, a, b); } }
int main(void) {
    crs_t s = {0};
    crs_add(&s, 'a', 'z'); crs_add(&s, 'A', 'Z'); crs_add(&s, '0', '9');
    eq("n", s.n, 3); eq("has m", crs_has(&s, 'm'), 1); eq("has !", crs_has(&s, '!'), 0);
    crs_add(&s, '[', '`'); eq("merge Z..a", s.n, 2);
    crs_remove(&s, 'c', 'e'); eq("split", s.n, 3); eq("has d", crs_has(&s, 'd'), 0); eq("has f", crs_has(&s, 'f'), 1);
    crs_t inv = {0}; crs_copy(&inv, &s); crs_invert(&inv);
    eq("inv has d", crs_has(&inv, 'd'), 1); eq("inv has a", crs_has(&inv, 'a'), 0); eq("inv has max", crs_has(&inv, CR_MAX), 1);
    crs_invert(&inv); eq("inv twice", crs_equal(&inv, &s), 1);
    crs_t digits = {0}; crs_add(&digits, '0', '9');
    eq("superset", crs_superset(&s, &digits), 1); crs_add(&digits, '!', '!'); eq("not superset", crs_superset(&s, &digits), 0);
    crs_intersect(&digits, &s); eq("intersect", digits.n, 1); eq("intersect hi", digits.r[0].hi, '9');
    crs_add(&s, 0x1F600, 0x1F64F); eq("plane1", crs_has_plane(&s, 1), 1); eq("plane2", crs_has_plane(&s, 2), 0);
    uint32_t len; uint8_t *b = crs_bitmap(&s, &len); eq("bitmap len", len, 8192 + 8193);
    crs_t back = {0}; crs_from_bitmap(&back, b, len); eq("bitmap roundtrip", crs_equal(&back, &s), 1); free(b);
    crs_t L = {0};
    if (!crs_from_icu_pattern(&L, "[[:L:][:M:]]")) { printf("fdn_charset: sem ICU\n"); return 1; }
    eq("letter é", crs_has(&L, 0xE9), 1); eq("letter 漢", crs_has(&L, 0x6F22), 1); eq("letter combining", crs_has(&L, 0x301), 1);
    eq("letter 5", crs_has(&L, '5'), 0);
    crs_t ws = {0}; crs_from_icu_pattern(&ws, "[[:Zs:]\\u0009]");
    eq("ws nbsp", crs_has(&ws, 0xA0), 1); eq("ws tab", crs_has(&ws, 9), 1); eq("ws nl", crs_has(&ws, 10), 0);
    crs_t nd = {0}; crs_from_icu_pattern(&nd, "[:Nd:]");
    eq("digit arabic", crs_has(&nd, 0x663), 1); eq("digit x", crs_has(&nd, 'x'), 0);
    printf("fdn_charset: %d/%d\n", ok, ok + bad);
    return bad != 0;
}
