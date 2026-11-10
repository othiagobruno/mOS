#define _POSIX_C_SOURCE 200809L
/* gcc -std=c11 -Wall -Wextra -O1 tools/fmttest.c -o /tmp/fmttest && /tmp/fmttest */
#define FMT_HOST 1
#include "../userland/libsystem/fmt_core.inc"
#include "../userland/libsystem/storekit_stub.inc"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void eq(const char *got, const char *want) {
    checks++;
    if (!got || !want || strcmp(got, want) != 0) {
        fprintf(stderr, "FAIL got \"%s\" want \"%s\"\n", got ? got : "(null)", want ? want : "(null)");
        fails++;
    }
}

static int near(double a, double b) {
    double d = a - b;
    if (d < 0) d = -d;
    return d < 1e-9;
}

int main(void) {
    char b[128];
    CHECK(mf_integer("en_US", 1234567, 1, b, sizeof b) >= 0); eq(b, "1,234,567");
    CHECK(mf_integer("pt_BR", 1234567, 1, b, sizeof b) >= 0); eq(b, "1.234.567");
    CHECK(mf_integer("de_DE", -42, 1, b, sizeof b) >= 0); eq(b, "-42");
    CHECK(mf_integer("fr_FR", 1234567, 1, b, sizeof b) >= 0); eq(b, "1 234 567");
    CHECK(mf_integer("en_US", 1234567, 0, b, sizeof b) >= 0); eq(b, "1234567");
    CHECK(mf_decimal("en_US", 1234.5, 2, 1, b, sizeof b) >= 0); eq(b, "1,234.50");
    CHECK(mf_decimal("pt_BR", 1234.5, 2, 1, b, sizeof b) >= 0); eq(b, "1.234,50");
    CHECK(mf_decimal("de_DE", 1234.5, 2, 1, b, sizeof b) >= 0); eq(b, "1.234,50");
    CHECK(mf_decimal("fr_FR", 1234.5, 2, 1, b, sizeof b) >= 0); eq(b, "1 234,50");
    CHECK(mf_decimal("en_US", 2.5, 0, 1, b, sizeof b) >= 0); eq(b, "3");
    CHECK(mf_decimal("en_US", -2.5, 0, 1, b, sizeof b) >= 0); eq(b, "-3");
    CHECK(mf_date("en_US", 2024, 3, 5, MF_DATE_NUMERIC, b, sizeof b) >= 0); eq(b, "3/5/2024");
    CHECK(mf_date("pt_BR", 2024, 3, 5, MF_DATE_NUMERIC, b, sizeof b) >= 0); eq(b, "05/03/2024");
    CHECK(mf_date("de_DE", 2024, 3, 5, MF_DATE_NUMERIC, b, sizeof b) >= 0); eq(b, "05.03.2024");
    CHECK(mf_date("en_US", 2024, 3, 5, MF_DATE_ABBREV, b, sizeof b) >= 0); eq(b, "Mar 5, 2024");
    CHECK(mf_date("pt_BR", 2024, 3, 5, MF_DATE_ABBREV, b, sizeof b) >= 0); eq(b, "5 de mar. de 2024");
    CHECK(mf_date("en_US", 2024, 3, 5, MF_DATE_LONG, b, sizeof b) >= 0); eq(b, "March 5, 2024");
    CHECK(mf_date("pt_BR", 2024, 3, 5, MF_DATE_LONG, b, sizeof b) >= 0); eq(b, "5 de março de 2024");
    CHECK(mf_date("de_DE", 2024, 3, 5, MF_DATE_LONG, b, sizeof b) >= 0); eq(b, "5. März 2024");
    CHECK(mf_date("fr_FR", 2024, 3, 5, MF_DATE_LONG, b, sizeof b) >= 0); eq(b, "5 mars 2024");
    CHECK(mf_date("en_US", 2024, 3, 5, MF_DATE_COMPLETE, b, sizeof b) >= 0); eq(b, "Tuesday, March 5, 2024");
    CHECK(mf_date("pt_BR", 2024, 3, 5, MF_DATE_COMPLETE, b, sizeof b) >= 0); eq(b, "terça-feira, 5 de março de 2024");
    CHECK(mf_time("en_US", 15, 4, 5, MF_TIME_SHORT, b, sizeof b) >= 0); eq(b, "3:04 PM");
    CHECK(mf_time("pt_BR", 15, 4, 5, MF_TIME_SHORT, b, sizeof b) >= 0); eq(b, "15:04");
    CHECK(mf_time("en_US", 15, 4, 5, MF_TIME_STANDARD, b, sizeof b) >= 0); eq(b, "3:04:05 PM");
    CHECK(mf_time("de_DE", 15, 4, 5, MF_TIME_STANDARD, b, sizeof b) >= 0); eq(b, "15:04:05");
    const char *items[] = {"Ana", "Bia", "Caio"};
    CHECK(mf_list("en_US", items, 3, b, sizeof b) >= 0); eq(b, "Ana, Bia, and Caio");
    CHECK(mf_list("en_US", items, 2, b, sizeof b) >= 0); eq(b, "Ana and Bia");
    CHECK(mf_list("pt_BR", items, 3, b, sizeof b) >= 0); eq(b, "Ana, Bia e Caio");
    CHECK(mf_list("pt_BR", items, 2, b, sizeof b) >= 0); eq(b, "Ana e Bia");
    CHECK(mf_list("de_DE", items, 3, b, sizeof b) >= 0); eq(b, "Ana, Bia und Caio");
    CHECK(mf_list("fr_FR", items, 3, b, sizeof b) >= 0); eq(b, "Ana, Bia et Caio");
    CHECK(mf_list("en_US", items, 0, b, sizeof b) == 0); eq(b, "");
    double m = 0;
    CHECK(mf_convert("m", "km", 1500, &m) == 0 && near(m, 1.5));
    CHECK(mf_convert("in", "ft", 12, &m) == 0 && near(m, 1.0));
    CHECK(mf_convert("g", "kg", 1000, &m) == 0 && near(m, 1.0));
    CHECK(mf_convert("m", "kg", 1, &m) == -1);
    CHECK(mf_dec_add("0.1", "0.2", b, sizeof b) == 0); eq(b, "0.3");
    CHECK(mf_dec_add("1.20", "0.01", b, sizeof b) == 0); eq(b, "1.21");
    CHECK(mf_dec_add("-1.5", "1.5", b, sizeof b) == 0); eq(b, "0");
    CHECK(mf_dec_add("-1.5", "0.5", b, sizeof b) == 0); eq(b, "-1");
    CHECK(mf_dec_add("999", "1", b, sizeof b) == 0); eq(b, "1000");
    int Y = 0, M = 0, D = 0, hh = 0, mm = 0, ss = 0;
    mf_civil_from_unix(1709596800LL, &Y, &M, &D, &hh, &mm, &ss);
    CHECK(Y == 2024 && M == 3 && D == 5 && hh == 0 && mm == 0 && ss == 0);
    CHECK(mf_datetime("en", 2024, 3, 5, 0, 0, 0, MF_DATE_NUMERIC, 1, b, sizeof b) >= 0); eq(b, "3/5/2024, 12:00 AM");
    CHECK(mf_datetime("en", 2024, 3, 5, 15, 4, 5, MF_DATE_ABBREV, 0, b, sizeof b) >= 0); eq(b, "Mar 5, 2024");
    CHECK(mf_datetime("pt", 2024, 3, 5, 15, 4, 5, -1, 1, b, sizeof b) >= 0); eq(b, "15:04");
    mf_span sp[8];
    char plain[64], hrefs[64];
    int ns = mf_markdown("**oi** e `x` e [a](https://mos)", plain, sizeof plain, sp, 8, hrefs, sizeof hrefs);
    CHECK(ns == 5);
    eq(plain, "oi e x e a");
    CHECK(ns >= 1 && sp[0].kind == MF_BOLD && sp[0].len == 2);
    CHECK(ns >= 3 && sp[2].kind == MF_CODE && sp[2].len == 1);
    CHECK(ns >= 5 && sp[4].kind == MF_LINK && sp[4].href >= 0 && strcmp(hrefs + sp[4].href, "https://mos") == 0);
    char err[160];
    CHECK(sk_verify("recibo", 6, err, sizeof err) == -1);
    CHECK(strstr(err, "não verificado") != 0);
    if (fails) { fprintf(stderr, "fmttest: %d falhas em %d\n", fails, checks); return 1; }
    printf("fmttest: %d/%d PASS\n", checks, checks);
    return 0;
}
