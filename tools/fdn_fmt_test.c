/* Teste de host do núcleo dos formatters (userland/libsystem/fdn_fmt_core.c) contra a ICU do Linux:
 *   gcc -O1 -Wall -DFDN_ICU_HOST -Iuserland/libsystem userland/libsystem/fdn_icu.c userland/libsystem/fdn_i18n_core.c \
 *       userland/libsystem/fdn_fmt_core.c tools/fdn_fmt_test.c -ldl -lm -o /tmp/fdn_fmt_test && /tmp/fdn_fmt_test
 * Os textos esperados são os do macOS 15 para as mesmas configurações (os espaços estreitos da ICU são normalizados).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fdn_fmt_core.h"
static int ok, bad;
/* troca U+202F, U+2009 e U+00A0 por espaço comum para comparar */
static const char *norm(const char *s) {
    static char b[1024];
    int o = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p && o < 1000; p++) {
        if (p[0] == 0xE2 && p[1] == 0x80 && (p[2] == 0xAF || p[2] == 0x89)) { b[o++] = ' '; p += 2; continue; }
        if (p[0] == 0xC2 && p[1] == 0xA0) { b[o++] = ' '; p += 1; continue; }
        b[o++] = (char)*p;
    }
    b[o] = 0;
    return b;
}
#define STR(got, want) do { const char *g_ = norm(got); if (!strcmp(g_, want)) ok++; else { bad++; printf("FALHA %d: '%s' != '%s'\n", __LINE__, g_, want); } } while (0)
#define CHK(c, m) do { if (c) ok++; else { bad++; printf("FALHA %d: %s\n", __LINE__, m); } } while (0)
int main(void) {
    char b[512];
    double t = 1742043600.0;   /* 2025-03-15 13:00:00 UTC */
    fdn_df f;
    fdn_df_init(&f);
    strcpy(f.locale, "en_US"); strcpy(f.tz, "America/New_York");
    f.date_style = 2; f.time_style = 1;
    fdn_df_format(&f, t, b, sizeof b); STR(b, "Mar 15, 2025, 9:00 AM");
    fdn_df_reset(&f); strcpy(f.locale, "pt_BR"); strcpy(f.tz, "America/Sao_Paulo"); f.date_style = 4; f.time_style = 0;
    fdn_df_format(&f, t, b, sizeof b); STR(b, "sábado, 15 de março de 2025");
    fdn_df_reset(&f); f.date_style = 1; fdn_df_format(&f, t, b, sizeof b); STR(b, "15/03/2025");
    fdn_df_reset(&f); f.date_style = 0; f.time_style = 0; CHK(fdn_df_format(&f, t, b, sizeof b) == 0 && !b[0], "sem estilo = vazio");
    fdn_df_reset(&f); strcpy(f.locale, "en_US_POSIX"); strcpy(f.tz, "GMT"); strcpy(f.pattern, "yyyy-MM-dd'T'HH:mm:ssZZZZZ");
    fdn_df_format(&f, t, b, sizeof b); STR(b, "2025-03-15T13:00:00Z");
    double p; int32_t end;
    CHK(fdn_df_parse(&f, "2024-02-29T23:59:58Z", 0, &p, &end) && p == 1709251198.0, "parse ISO");
    CHK(!fdn_df_parse(&f, "lixo", 0, &p, &end), "parse falha");
    fdn_df_reset(&f); strcpy(f.pattern, "EEE, dd MMM yyyy HH:mm:ss zzz"); fdn_df_format(&f, t, b, sizeof b); STR(b, "Sat, 15 Mar 2025 13:00:00 GMT");
    fdn_df_reset(&f); strcpy(f.locale, "en_US"); f.pattern[0] = 0; f.date_style = 2; f.time_style = 0; fdn_df_pattern(&f, b, sizeof b); STR(b, "MMM d, y");
    fdn_df_template("en_US", "MMMMd", b, sizeof b); STR(b, "MMMM d");
    fdn_df_template("pt_BR", "MMMMd", b, sizeof b); STR(b, "d 'de' MMMM");
    fdn_df_reset(&f); strcpy(f.locale, "th_TH"); strcpy(f.cal, "buddhist"); strcpy(f.pattern, "y"); fdn_df_format(&f, t, b, sizeof b); STR(b, "2568");
    fdn_df_reset(&f);
    fdn_iso_pattern(1907, b, sizeof b); STR(b, "yyyy-MM-dd'T'HH:mm:ssXXXXX");
    fdn_iso_pattern(1907 | 2048, b, sizeof b); STR(b, "yyyy-MM-dd'T'HH:mm:ss.SSSXXXXX");
    fdn_iso_pattern(275, b, sizeof b); STR(b, "yyyy-MM-dd");
    fdn_iso_pattern(1 | 4 | 16 | 256, b, sizeof b); STR(b, "YYYY-'W'ww-ee");

    void *nf = fdn_nf_open(FDN_NS_DECIMAL, "pt_BR");
    fdn_nf_format_double(nf, 1234567.891, b, sizeof b); STR(b, "1.234.567,891");
    double v; int64_t iv; int isint; int32_t len16;
    CHK(fdn_nf_parse(nf, "1.234,5", &v, &iv, &isint, &end, &len16) && v == 1234.5 && end == len16, "parse decimal pt");
    fdn_nf_get_symbol(nf, 0, b, sizeof b); STR(b, ",");
    fdn_nf_close(nf);
    nf = fdn_nf_open(FDN_NS_CURRENCY, "pt_BR"); fdn_nf_format_double(nf, 1234.5, b, sizeof b); STR(b, "R$ 1.234,50"); fdn_nf_close(nf);
    nf = fdn_nf_open(FDN_NS_CURRENCY, "en_US"); fdn_nf_format_double(nf, -3.5, b, sizeof b); STR(b, "-$3.50"); fdn_nf_close(nf);
    nf = fdn_nf_open(FDN_NS_CURRENCY_ACCOUNTING, "en_US"); fdn_nf_format_double(nf, -3.5, b, sizeof b); STR(b, "($3.50)"); fdn_nf_close(nf);
    nf = fdn_nf_open(FDN_NS_PERCENT, "en_US"); fdn_nf_format_double(nf, 0.256, b, sizeof b); STR(b, "26%"); fdn_nf_close(nf);
    nf = fdn_nf_open(FDN_NS_NONE, "en_US"); fdn_nf_format_double(nf, 12345.6, b, sizeof b); STR(b, "12346"); fdn_nf_close(nf);
    nf = fdn_nf_open(FDN_NS_SCIENTIFIC, "en_US"); fdn_nf_format_double(nf, 12345, b, sizeof b); STR(b, "1.2345E4"); fdn_nf_close(nf);
    nf = fdn_nf_open(FDN_NS_ORDINAL, "en_US"); fdn_nf_format_int64(nf, 22, b, sizeof b); STR(b, "22nd"); fdn_nf_close(nf);
    nf = fdn_nf_open(FDN_NS_CURRENCY_ISO, "en_US"); fdn_nf_format_double(nf, 1, b, sizeof b); STR(b, "USD 1.00"); fdn_nf_close(nf);
    fdn_spellout("pt_BR", 42, b, sizeof b); STR(b, "quarenta e dois");
    nf = fdn_nf_open(FDN_NS_DECIMAL, "en_US"); fdn_nf_apply_pattern(nf, "#,##0.00;(#,##0.00)"); fdn_nf_format_double(nf, -1234.5, b, sizeof b); STR(b, "(1,234.50)");
    fdn_nf_set_attr(nf, 6, 4); fdn_nf_set_attr(nf, 7, 4); fdn_nf_format_double(nf, 1.5, b, sizeof b); STR(b, "1.5000");
    fdn_nf_close(nf);
    double rnd; CHK(fdn_currency_digits("JPY", &rnd) == 0 && fdn_currency_digits("BRL", &rnd) == 2, "dígitos da moeda");

    fdn_unit_format("en_US", "duration-hour", 2, 2, 0, b, sizeof b); STR(b, "2 hours");
    fdn_unit_format("pt_BR", "duration-minute", 1, 2, 0, b, sizeof b); STR(b, "1 minuto");
    const char *items[3] = {"maçã", "pera", "uva"};
    fdn_list_format("pt_BR", 0, 0, items, 3, b, sizeof b); STR(b, "maçã, pera e uva");
    fdn_list_format("en_US", 0, 0, items, 3, b, sizeof b); STR(b, "maçã, pera, and uva");
    fdn_reldate("en_US", -1, 4, 0, 1, 0, b, sizeof b); STR(b, "yesterday");
    fdn_reldate("en_US", -3, 5, 0, 0, 0, b, sizeof b); STR(b, "3 hours ago");
    fdn_reldate("pt_BR", 2, 4, 0, 0, 0, b, sizeof b); STR(b, "em 2 dias");
    fdn_interval_format("en_US", "yMMMd", "GMT", t, t + 3 * 86400, b, sizeof b); STR(b, "Mar 15 – 18, 2025");

    fdn_bytes_format("en_US", 0, 0, 0, 1, 1, 1, 0, 1, 0, b, sizeof b); STR(b, "Zero KB");
    fdn_bytes_format("en_US", 1, 0, 0, 1, 1, 1, 0, 1, 0, b, sizeof b); STR(b, "1 byte");
    fdn_bytes_format("en_US", 999, 0, 0, 1, 1, 1, 0, 1, 0, b, sizeof b); STR(b, "999 bytes");
    fdn_bytes_format("en_US", 1500000, 0, 0, 1, 1, 1, 0, 1, 0, b, sizeof b); STR(b, "1.5 MB");
    fdn_bytes_format("en_US", 1073741824, 0, 1, 1, 1, 1, 0, 1, 0, b, sizeof b); STR(b, "1 GB");
    fdn_bytes_format("pt_BR", 2345678901LL, 0, 0, 1, 1, 1, 0, 1, 0, b, sizeof b); STR(b, "2,35 GB");
    fdn_bytes_format("en_US", 123456, 4, 0, 1, 1, 1, 0, 1, 0, b, sizeof b); STR(b, "0.1 MB");
    fdn_bytes_format("en_US", 2048, 0, 0, 1, 1, 1, 1, 1, 0, b, sizeof b); STR(b, "2 KB (2,048 bytes)");

    fdn_dur_part dp[3] = {{"duration-hour", 1}, {"duration-minute", 2}, {"duration-second", 3}};
    fdn_duration_format("en_US", 0, dp, 3, 0, b, sizeof b); STR(b, "1:02:03");
    fdn_duration_format("en_US", 1, dp, 3, 0, b, sizeof b); STR(b, "1h 2m 3s");
    fdn_duration_format("en_US", 2, dp, 3, 0, b, sizeof b); STR(b, "1 hr, 2 min, 3 sec");
    fdn_duration_format("en_US", 3, dp, 3, 0, b, sizeof b); STR(b, "1 hour, 2 minutes, 3 seconds");
    fdn_duration_format("pt_BR", 3, dp, 2, 0, b, sizeof b); STR(b, "1 hora e 2 minutos");
    fdn_duration_format("en_US", 5, dp, 2, 0, b, sizeof b); STR(b, "1hr 2min");
    fdn_duration_format("en_US", 4, dp, 1, 0, b, sizeof b); STR(b, "one hour");
    printf("fdn-fmt: %d/%d ok\n", ok, ok + bad);
    return bad != 0;
}
