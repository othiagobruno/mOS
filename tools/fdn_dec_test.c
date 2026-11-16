/* Teste de host do NSDecimal (fdn_decimal_core.c). Valores conferidos contra a Foundation do macOS. */
#include <stdio.h>
#include <string.h>
#include "fdn_decimal_core.h"
static int ok, bad;
static nsdec_t D(const char *s) { nsdec_t d; fdn_dec_parse(&d, s, "."); return d; }
static void chk(const char *what, const nsdec_t *d, const char *want) {
    char b[300]; fdn_dec_to_str(d, b, sizeof b, ".");
    if (!strcmp(b, want)) ok++; else { bad++; printf("FALHOU %s: %s (queria %s)\n", what, b, want); }
}
static void eq(const char *what, long long a, long long b) { if (a == b) ok++; else { bad++; printf("FALHOU %s: %lld (queria %lld)\n", what, a, b); } }
int main(void) {
    nsdec_t a = D("1.5"), b = D("2.25"), r;
    fdn_dec_add(&r, &a, &b, 0); chk("1.5+2.25", &r, "3.75");
    fdn_dec_sub(&r, &a, &b, 0); chk("1.5-2.25", &r, "-0.75");
    fdn_dec_mul(&r, &a, &b, 0); chk("1.5*2.25", &r, "3.375");
    nsdec_t one = D("1"), three = D("3"), two = D("2");
    eq("1/3 st", fdn_dec_div(&r, &one, &three, 0), DEC_PREC); chk("1/3", &r, "0.333333333333333333333333333333333333333");
    nsdec_t tw = D("2"); fdn_dec_div(&r, &tw, &three, 0); chk("2/3", &r, "0.66666666666666666666666666666666666667");
    fdn_dec_div(&r, &tw, &three, 1); chk("2/3 down", &r, "0.66666666666666666666666666666666666666");
    nsdec_t q = D("1"), f = D("4"); fdn_dec_div(&r, &q, &f, 0); chk("1/4", &r, "0.25");
    nsdec_t z = D("0"); eq("x/0", fdn_dec_div(&r, &one, &z, 0), DEC_DIV0); chk("x/0 nan", &r, "NaN");
    fdn_dec_pow(&r, &two, 10, 0); chk("2^10", &r, "1024");
    fdn_dec_pow(&r, &two, 127, 0); chk("2^127", &r, "170141183460469231731687303715884105728");
    fdn_dec_pow(&r, &two, 200, 0); chk("2^200", &r, "1606938044258990275541962092341162602520000000000000000000000");
    fdn_dec_pow10(&r, &a, 3, 0); chk("1.5e3", &r, "1500");
    nsdec_t x = D("2.345"); fdn_dec_round(&r, &x, 2, 0); chk("round plain", &r, "2.35");
    fdn_dec_round(&r, &x, 2, 1); chk("round down", &r, "2.34");
    nsdec_t nx = D("-2.345"); fdn_dec_round(&r, &nx, 2, 1); chk("round down neg", &r, "-2.35");
    fdn_dec_round(&r, &nx, 2, 2); chk("round up neg", &r, "-2.34");
    nsdec_t bk = D("2.345"); fdn_dec_round(&r, &bk, 2, 3); chk("bankers 2.345", &r, "2.34");
    nsdec_t bk2 = D("2.355"); fdn_dec_round(&r, &bk2, 2, 3); chk("bankers 2.355", &r, "2.36");
    nsdec_t bk3 = D("2.3451"); fdn_dec_round(&r, &bk3, 2, 3); chk("bankers 2.3451", &r, "2.35");
    nsdec_t big = D("123456"); fdn_dec_round(&r, &big, -2, 0); chk("round -2", &r, "123500");
    eq("cmp <", fdn_dec_cmp(&a, &b), -1); eq("cmp >", fdn_dec_cmp(&b, &a), 1);
    nsdec_t a2 = D("1.50"); eq("cmp =", fdn_dec_cmp(&a, &a2), 0);
    nsdec_t n1 = D("-3"), n2 = D("-20"); eq("cmp neg", fdn_dec_cmp(&n1, &n2), 1);
    nsdec_t t1 = D("1e-100"), t2 = D("1e100"); eq("cmp mag", fdn_dec_cmp(&t1, &t2), -1);
    fdn_dec_add(&r, &t2, &t1, 0); chk("1e100+1e-100", &r, "10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
    nsdec_t p1 = D("0.1"), p2 = D("0.2"); fdn_dec_add(&r, &p1, &p2, 0); chk("0.1+0.2", &r, "0.3");
    nsdec_t e1 = D("1.0000000000000000000000000000000000001"); chk("38 dig", &e1, "1.0000000000000000000000000000000000001");
    nsdec_t n = D("1.5000"), m = D("12"); eq("norm", fdn_dec_normalize(&n, &m, 0), 0);
    eq("norm exp a", n.exp, m.exp); eq("norm exp", n.exp, -1); chk("norm a", &n, "1.5"); chk("norm b", &m, "12.0");
    nsdec_t c = D("1500"); chk("compacto", &c, "1500"); eq("compact exp", c.exp, 2);
    nsdec_t nan = D("abc"); chk("nan", &nan, "NaN");
    nsdec_t sc = D("1.5e-3"); chk("e-3", &sc, "0.0015");
    nsdec_t neg0 = D("-0"); chk("-0", &neg0, "0");
    nsdec_t dd; fdn_dec_from_double(&dd, 0.1); chk("double 0.1", &dd, "0.1");
    fdn_dec_from_double(&dd, 1234.5678); chk("double", &dd, "1234.5678");
    eq("to_double", fdn_dec_to_double(&b) == 2.25, 1);
    eq("i64", fdn_dec_to_i64(&nx), -2); eq("i64 big", fdn_dec_to_i64(&c), 1500);
    nsdec_t mx = D("340282366920938463463374607431768211455e127"); chk("max", &mx, "340282366920938463463374607431768211455" "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
    eq("over", fdn_dec_mul(&r, &mx, &two, 0), DEC_OVER);
    nsdec_t tiny = D("1e-128"); eq("under", fdn_dec_mul(&r, &tiny, &tiny, 0), DEC_UNDER); chk("under 0", &r, "0");
    nsdec_t s1 = D("100"), s2 = D("0.001"); fdn_dec_sub(&r, &s1, &s2, 0); chk("100-0.001", &r, "99.999");
    printf("fdn_dec: %d/%d\n", ok, ok + bad);
    return bad != 0;
}
