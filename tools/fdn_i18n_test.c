/* Teste de host do núcleo de locale/fuso/calendário (userland/libsystem/fdn_i18n_core.c) contra a ICU do Linux:
 *   gcc -O1 -Wall -DFDN_ICU_HOST -Iuserland/libsystem userland/libsystem/fdn_icu.c userland/libsystem/fdn_i18n_core.c \
 *       tools/fdn_i18n_test.c -ldl -lm -o /tmp/fdn_i18n_test && /tmp/fdn_i18n_test
 * Os valores esperados são os do macOS para as mesmas chamadas.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "fdn_i18n_core.h"
static int ok, bad;
#define CHK(c, ...) do { if (c) ok++; else { bad++; printf("FALHA %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define STR(got, want) CHK(!strcmp(got, want), "'%s' != '%s'", got, want)
static int cnt;
static void count_cb(const char *s, void *c) { (void)s; (void)c; cnt++; }
static char last[64];
static void last_cb(const char *s, void *c) { (void)c; snprintf(last, sizeof last, "%s", s); }
static int ranges;
static void range_cb(uint32_t lo, uint32_t hi, void *c) { (void)c; if (lo <= 'a' && hi >= 'a') ranges++; }
int main(void) {
    char b[256];
    const fdn_icu_t *I = fdn_icu();
    CHK(I->ok, "ICU não carregou");
    if (!I->ok) return 1;
    fdn_loc_canonical("pt-BR", b, sizeof b); STR(b, "pt_BR");
    fdn_loc_canonical("zh-Hant-TW", b, sizeof b); STR(b, "zh_Hant_TW");
    fdn_loc_canonical("en_US@calendar=buddhist", b, sizeof b); STR(b, "en_US@calendar=buddhist");
    fdn_loc_language_tag("pt_BR", b, sizeof b); STR(b, "pt-BR");
    fdn_locale_part("zh_Hant_TW", FDN_LP_SCRIPT, b, sizeof b); STR(b, "Hant");
    fdn_locale_part("pt_BR", FDN_LP_REGION, b, sizeof b); STR(b, "BR");
    fdn_locale_part("en_US@calendar=japanese", FDN_LP_CALENDAR, b, sizeof b); STR(b, "japanese");
    fdn_loc_display("en_US", FDN_LD_IDENT, "pt_BR", b, sizeof b); STR(b, "Portuguese (Brazil)");
    fdn_loc_display("pt_BR", FDN_LD_LANGUAGE, "en", b, sizeof b); STR(b, "inglês");
    fdn_loc_display("pt_BR", FDN_LD_REGION, "US", b, sizeof b); STR(b, "Estados Unidos");
    fdn_loc_display("en_US", FDN_LD_CURRENCY_NAME, "BRL", b, sizeof b); STR(b, "Brazilian Real");
    fdn_loc_display("pt_BR", FDN_LD_CURRENCY_SYMBOL, "BRL", b, sizeof b); STR(b, "R$");
    fdn_loc_display("en_US", FDN_LD_CALENDAR, "buddhist", b, sizeof b); STR(b, "Buddhist Calendar");
    fdn_loc_info("pt_BR", FDN_LN_DECIMAL, b, sizeof b); STR(b, ",");
    fdn_loc_info("pt_BR", FDN_LN_GROUPING, b, sizeof b); STR(b, ".");
    fdn_loc_info("en_US", FDN_LN_DECIMAL, b, sizeof b); STR(b, ".");
    fdn_loc_info("pt_BR", FDN_LN_CURRENCY_CODE, b, sizeof b); STR(b, "BRL");
    fdn_loc_info("ja_JP", FDN_LN_CURRENCY_SYMBOL, b, sizeof b); STR(b, "￥");
    fdn_loc_info("en_US", FDN_LN_QUOTE_BEGIN, b, sizeof b); STR(b, "\xe2\x80\x9c");
    fdn_loc_info("de_DE", FDN_LN_QUOTE_BEGIN, b, sizeof b); STR(b, "\xe2\x80\x9e");
    fdn_loc_info("th_TH@calendar=buddhist", FDN_LN_CALENDAR_ID, b, sizeof b); STR(b, "buddhist");
    fdn_loc_info("pt_BR", FDN_LN_CALENDAR_ID, b, sizeof b); STR(b, "gregorian");
    fdn_loc_info("en_US", FDN_LN_MEASUREMENT, b, sizeof b); STR(b, "U.S.");
    CHK(fdn_loc_uses_metric("pt_BR") && !fdn_loc_uses_metric("en_US"), "métrico");
    CHK(fdn_loc_direction("ar", 0) == 2 && fdn_loc_direction("pt", 0) == 1, "direção");
    CHK(fdn_loc_first_weekday("pt_BR") == 1 && fdn_loc_first_weekday("de_DE") == 2, "primeiro dia da semana");
    CHK(fdn_loc_min_days("de_DE") == 4, "mínimo de dias na 1ª semana");
    cnt = 0; fdn_loc_each_available(count_cb, 0); CHK(cnt > 500, "locales disponíveis %d", cnt);
    cnt = 0; fdn_loc_each_currency(1, count_cb, 0); CHK(cnt > 100 && cnt < 250, "moedas comuns %d", cnt);
    ranges = 0; fdn_loc_exemplar("pt_BR", range_cb, 0); CHK(ranges == 1, "exemplar contém a");

    long secs, dst;
    CHK(fdn_tz_parse_fixed("GMT-0300", &secs) == 0 && secs == -10800, "GMT-0300");
    CHK(fdn_tz_parse_fixed("+05:30", &secs) == 0 && secs == 19800, "+05:30");
    CHK(fdn_tz_parse_fixed("America/Sao_Paulo", &secs) == -1, "nome não é fixo");
    fdn_tz_fixed_name(-10800, b, sizeof b); STR(b, "GMT-0300");
    CHK(fdn_tz_canonical("America/Sao_Paulo", b, sizeof b) > 0, "fuso conhecido");
    CHK(fdn_tz_canonical("Marte/Olympus", b, sizeof b) == 0, "fuso inventado");
    double jan = 1735689600.0;  /* 2025-01-01 00:00 UTC */
    double jul = 1751328000.0;  /* 2025-07-01 00:00 UTC */
    CHK(fdn_tz_offset("America/Sao_Paulo", jan, &dst) == -10800 && dst == 0, "SP");
    CHK(fdn_tz_offset("America/New_York", jul, &dst) == -14400 && dst == 3600, "NY verão");
    CHK(fdn_tz_offset("Asia/Kolkata", jan, 0) == 19800, "Índia");
    fdn_tz_abbreviation("America/New_York", jul, b, sizeof b); STR(b, "EDT");
    fdn_tz_abbreviation("America/Sao_Paulo", jan, b, sizeof b); STR(b, "GMT-3");
    fdn_tz_name("America/Sao_Paulo", FDN_TZN_STANDARD, "pt_BR", jan, b, sizeof b); STR(b, "Horário Padrão de Brasília");
    double tr;
    CHK(fdn_tz_next_transition("America/New_York", jan, &tr) && fabs(tr - 1741503600.0) < 1, "transição NY %f", tr);
    STR(fdn_tz_from_abbreviation("PST"), "America/Los_Angeles");
    cnt = 0; fdn_tz_each_known(count_cb, 0); CHK(cnt > 300, "fusos conhecidos %d", cnt);

    fdn_cal k;
    fdn_comps c;
    fdn_cal_init(&k, "gregorian", "pt_BR", "America/Sao_Paulo");
    fdn_cal_components(&k, 0xFFFFFFFF, jan + 0.25, &c);   /* 2024-12-31 21:00:00.25 em SP */
    CHK(c.year == 2024 && c.month == 12 && c.day == 31 && c.hour == 21 && c.minute == 0, "comps %lld-%lld-%lld %lld",
        (long long)c.year, (long long)c.month, (long long)c.day, (long long)c.hour);
    CHK(c.weekday == 3 && c.era == 1 && c.quarter == 4 && c.day_of_year == 366, "terça, era, trimestre, dia do ano");
    CHK(llabs(c.nanosecond - 250000000) < 1000, "nanos %lld", (long long)c.nanosecond);
    CHK(c.year_for_week == 2025 && c.week_of_year == 1, "semana 1 de 2025 (%lld/%lld)", (long long)c.year_for_week, (long long)c.week_of_year);
    double d;
    fdn_comps_clear(&c); c.year = 2025; c.month = 3; c.day = 15; c.hour = 10;
    CHK(fdn_cal_date(&k, &c, &d) && fabs(d - 1742043600.0) < 1e-6, "dateFromComponents %f", d);
    fdn_comps_clear(&c); c.year = 2025; c.month = 2; c.day = 30;   /* normaliza para 2 de março */
    fdn_cal_date(&k, &c, &d); fdn_cal_components(&k, FDN_U_MONTH | FDN_U_DAY, d, &c);
    CHK(c.month == 3 && c.day == 2, "normalização");
    fdn_comps_clear(&c); c.year = 2025; c.month = 11; c.weekday = 5; c.weekday_ordinal = 4;  /* Ação de Graças: 27/11 */
    fdn_cal_date(&k, &c, &d); fdn_cal_components(&k, FDN_U_DAY, d, &c); CHK(c.day == 27, "4ª quinta de novembro %lld", (long long)c.day);
    fdn_comps_clear(&c);
    CHK(fdn_cal_date(&k, &c, &d), "vazio");
    fdn_cal_components(&k, FDN_U_YEAR | FDN_U_MONTH | FDN_U_DAY, d, &c); CHK(c.year == 1 && c.month == 1 && c.day == 1, "ano 1");
    fdn_comps add; fdn_comps_clear(&add); add.month = 1;
    fdn_comps_clear(&c); c.year = 2025; c.month = 1; c.day = 31; fdn_cal_date(&k, &c, &d);
    fdn_cal_add(&k, &add, d, 0, &d); fdn_cal_components(&k, FDN_U_MONTH | FDN_U_DAY, d, &c);
    CHK(c.month == 2 && c.day == 28, "31/jan + 1 mês = 28/fev");
    fdn_comps_clear(&add); add.day = 40;
    fdn_comps_clear(&c); c.year = 2025; c.month = 1; c.day = 10; fdn_cal_date(&k, &c, &d);
    fdn_cal_add(&k, &add, d, FDN_CO_WRAP, &d); fdn_cal_components(&k, FDN_U_MONTH | FDN_U_DAY, d, &c);
    CHK(c.month == 1 && c.day == 19, "wrap de dia %lld/%lld", (long long)c.day, (long long)c.month);
    double a, z;
    fdn_comps_clear(&c); c.year = 2024; c.month = 1; c.day = 15; fdn_cal_date(&k, &c, &a);
    fdn_comps_clear(&c); c.year = 2025; c.month = 3; c.day = 20; c.hour = 5; fdn_cal_date(&k, &c, &z);
    fdn_cal_difference(&k, FDN_U_YEAR | FDN_U_MONTH | FDN_U_DAY | FDN_U_HOUR, a, z, &c);
    CHK(c.year == 1 && c.month == 2 && c.day == 5 && c.hour == 5, "diferença %lld %lld %lld %lld", (long long)c.year, (long long)c.month, (long long)c.day, (long long)c.hour);
    long lo, len;
    fdn_comps_clear(&c); c.year = 2024; c.month = 2; c.day = 10; fdn_cal_date(&k, &c, &d);
    CHK(fdn_cal_range(&k, FDN_U_DAY, FDN_U_MONTH, d, &lo, &len) && lo == 1 && len == 29, "fev/2024 tem 29");
    CHK(fdn_cal_range(&k, FDN_U_DAY, FDN_U_YEAR, d, &lo, &len) && len == 366, "2024 tem 366");
    CHK(fdn_cal_range(&k, FDN_U_HOUR, FDN_U_DAY, d, &lo, &len) && lo == 0 && len == 24, "horas");
    CHK(fdn_cal_ordinality(&k, FDN_U_DAY, FDN_U_YEAR, d) == 41, "dia 41 do ano");
    CHK(fdn_cal_ordinality(&k, FDN_U_DAY, FDN_U_WEEK_OF_YEAR, d) == 7, "sábado é o 7º dia (domingo primeiro)");
    double st, il;
    CHK(fdn_cal_interval(&k, FDN_U_MONTH, d, &st, &il) && fabs(il - 29 * 86400.0) < 1e-6, "intervalo do mês");
    fdn_cal_components(&k, FDN_U_DAY | FDN_U_HOUR, st, &c); CHK(c.day == 1 && c.hour == 0, "início do mês");
    CHK(fdn_cal_interval(&k, FDN_U_WEEK_OF_YEAR, d, &st, &il) && fabs(il - 7 * 86400.0) < 1e-6, "semana");
    fdn_cal_components(&k, FDN_U_WEEKDAY, st, &c); CHK(c.weekday == 1, "semana começa no domingo");
    CHK(fdn_cal_is_weekend(&k, d), "sábado é fim de semana");
    CHK(fdn_cal_weekend_range(&k, d, 0, &st, &il) && fabs(il - 2 * 86400.0) < 1e-6, "fim de semana de 2 dias");
    fdn_comps w; fdn_comps_clear(&w); w.hour = 9; w.minute = 30;
    CHK(fdn_cal_next_match(&k, d, &w, 0, &st), "próxima 9:30");
    fdn_cal_components(&k, FDN_U_DAY | FDN_U_HOUR | FDN_U_MINUTE, st, &c); CHK(c.day == 10 && c.hour == 9 && c.minute == 30, "dia 10 9:30 (%lld %lld:%lld)", (long long)c.day, (long long)c.hour, (long long)c.minute);
    fdn_comps_clear(&w); w.month = 12; w.day = 25;
    CHK(fdn_cal_next_match(&k, d, &w, 0, &st), "próximo Natal");
    fdn_cal_components(&k, FDN_U_YEAR | FDN_U_MONTH | FDN_U_DAY, st, &c); CHK(c.year == 2024 && c.month == 12 && c.day == 25, "Natal 2024");
    cnt = 0; fdn_cal_symbols(&k, FDN_SYM_MONTHS, count_cb, 0); CHK(cnt == 12, "12 meses (%d)", cnt);
    fdn_cal_symbols(&k, FDN_SYM_MONTHS, last_cb, 0); STR(last, "dezembro");
    fdn_cal_symbols(&k, FDN_SYM_SHORT_WEEKDAYS, last_cb, 0); STR(last, "sáb.");
    cnt = 0; fdn_cal_symbols(&k, FDN_SYM_WEEKDAYS, count_cb, 0); CHK(cnt == 7, "7 dias");
    fdn_cal_init(&k, "buddhist", "th_TH", "Asia/Bangkok");
    fdn_cal_components(&k, FDN_U_YEAR, jan, &c); CHK(c.year == 2568, "ano budista %lld", (long long)c.year);
    fdn_cal_init(&k, "iso8601", "en_US", "GMT");
    CHK(fdn_cal_first_weekday(&k) == 2 && fdn_cal_min_days(&k) == 4, "iso8601");
    fdn_cal_init(&k, "japanese", "ja_JP", "Asia/Tokyo");
    fdn_cal_components(&k, FDN_U_ERA | FDN_U_YEAR, jan, &c); CHK(c.year == 7 && c.era == 236, "Reiwa 7 (%lld/%lld)", (long long)c.era, (long long)c.year);
    CHK(fdn_cal_limit(&k, FDN_U_MONTH, 1, &lo, &len) && lo == 1 && len == 12, "limites do mês");
    printf("fdn-i18n: %d/%d ok\n", ok, ok + bad);
    return bad != 0;
}
