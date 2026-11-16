/*
 * mOS libSystem — núcleo de locale, fuso e calendário da Foundation (sem Objective-C), sobre a ICU (fdn_icu.h).
 * As classes NSLocale, NSTimeZone, NSCalendar e NSDateComponents (fdn_i18n.c) são uma casca em cima disto.
 * Datas: segundos desde 1970-01-01 UTC (double). Textos: UTF-8.
 */
#pragma once
#include <stdint.h>
#include "fdn_icu.h"

#define FDN_UNDEF ((int64_t)0x7fffffffffffffffLL)   /* NSDateComponentUndefined */

/* ---- locale */
enum { FDN_LP_LANGUAGE, FDN_LP_SCRIPT, FDN_LP_REGION, FDN_LP_VARIANT, FDN_LP_CALENDAR, FDN_LP_COLLATION,
       FDN_LP_CURRENCY, FDN_LP_NUMBERS, FDN_LP_BASE };
int fdn_loc_canonical(const char *in, char *out, int cap);          /* "pt-BR" -> "pt_BR"; keywords em @ */
int fdn_loc_language_tag(const char *in, char *out, int cap);       /* "pt_BR" -> "pt-BR" */
int fdn_locale_part(const char *loc, int part, char *out, int cap);    /* "" quando não há */
int fdn_loc_with_keyword(const char *loc, const char *key, const char *val, char *out, int cap);
enum { FDN_LD_IDENT, FDN_LD_LANGUAGE, FDN_LD_REGION, FDN_LD_SCRIPT, FDN_LD_VARIANT, FDN_LD_CALENDAR,
       FDN_LD_COLLATION, FDN_LD_CURRENCY_NAME, FDN_LD_CURRENCY_SYMBOL };
int fdn_loc_display(const char *display_loc, int kind, const char *code, char *out, int cap);
enum { FDN_LN_DECIMAL, FDN_LN_GROUPING, FDN_LN_CURRENCY_SYMBOL, FDN_LN_CURRENCY_CODE, FDN_LN_QUOTE_BEGIN,
       FDN_LN_QUOTE_END, FDN_LN_ALT_QUOTE_BEGIN, FDN_LN_ALT_QUOTE_END, FDN_LN_CALENDAR_ID, FDN_LN_MEASUREMENT };
int fdn_loc_info(const char *loc, int what, char *out, int cap);
int fdn_loc_uses_metric(const char *loc);
int fdn_loc_direction(const char *loc, int line);                   /* NSLocaleLanguageDirection */
int fdn_loc_first_weekday(const char *loc);                         /* 1 = domingo */
int fdn_loc_min_days(const char *loc);
/* Listas: chamam cb(texto, ctx) para cada item. Devolvem a quantidade. */
typedef void (*fdn_each_fn)(const char *s, void *ctx);
int fdn_loc_each_available(fdn_each_fn cb, void *ctx);
int fdn_loc_each_iso_language(fdn_each_fn cb, void *ctx);
int fdn_loc_each_iso_region(fdn_each_fn cb, void *ctx);
int fdn_loc_each_currency(int common_only, fdn_each_fn cb, void *ctx);
/* Faixas do conjunto de caracteres de exemplo: cb(início, fim). */
int fdn_loc_exemplar(const char *loc, void (*cb)(uint32_t lo, uint32_t hi, void *ctx), void *ctx);

/* ---- fuso */
int fdn_tz_canonical(const char *name, char *out, int cap);         /* 0 = fuso desconhecido */
int fdn_tz_system(char *out, int cap);                              /* fuso do sistema */
long fdn_tz_offset(const char *name, double t, long *dst);          /* segundos a leste de UTC */
enum { FDN_TZN_STANDARD, FDN_TZN_SHORT_STANDARD, FDN_TZN_DST, FDN_TZN_SHORT_DST,
       FDN_TZN_GENERIC, FDN_TZN_SHORT_GENERIC };
int fdn_tz_name(const char *name, int style, const char *loc, double t, char *out, int cap);
int fdn_tz_abbreviation(const char *name, double t, char *out, int cap);
int fdn_tz_next_transition(const char *name, double after, double *out);
int fdn_tz_each_known(fdn_each_fn cb, void *ctx);
const char *fdn_tz_data_version(void);
/* "GMT", "GMT+0300", "UTC", "GMT-03:00", "+05:30": desvio fixo; -1 quando o nome não é desse tipo. */
int fdn_tz_parse_fixed(const char *name, long *secs);
void fdn_tz_fixed_name(long secs, char *out, int cap);              /* "GMT-0300" como o macOS */
const char *fdn_tz_from_abbreviation(const char *abbr);             /* tabela do macOS; NULL se não conhece */

/* ---- calendário */
typedef struct {
    char ident[24];      /* "gregorian", "buddhist", "japanese", "islamic-umalqura", "hebrew", "iso8601"... */
    char locale[64];
    char tz[64];
    int first_weekday;   /* 1..7; 0 = do locale */
    int min_days;        /* 1..7; 0 = do locale */
} fdn_cal;

/* NSCalendarUnit */
enum { FDN_U_ERA = 2, FDN_U_YEAR = 4, FDN_U_MONTH = 8, FDN_U_DAY = 16, FDN_U_HOUR = 32, FDN_U_MINUTE = 64,
       FDN_U_SECOND = 128, FDN_U_WEEKDAY = 512, FDN_U_WEEKDAY_ORDINAL = 1024, FDN_U_QUARTER = 2048,
       FDN_U_WEEK_OF_MONTH = 4096, FDN_U_WEEK_OF_YEAR = 8192, FDN_U_YEAR_FOR_WEEK = 16384,
       FDN_U_NANOSECOND = 32768, FDN_U_DAY_OF_YEAR = 65536, FDN_U_CALENDAR = 1 << 20, FDN_U_TIMEZONE = 1 << 21 };

typedef struct {
    int64_t era, year, month, day, hour, minute, second, nanosecond, weekday, weekday_ordinal, quarter,
            week_of_month, week_of_year, year_for_week, day_of_year;
    int leap_month;      /* -1 indefinido, 0/1 */
} fdn_comps;

void fdn_comps_clear(fdn_comps *c);
int64_t *fdn_comps_field(fdn_comps *c, uint64_t unit);              /* NULL para unidade sem campo */
void fdn_cal_init(fdn_cal *k, const char *ident, const char *locale, const char *tz);
int fdn_cal_ident_ok(const char *ident);                            /* identificador conhecido */
int fdn_cal_first_weekday(const fdn_cal *k);
int fdn_cal_min_days(const fdn_cal *k);
int fdn_cal_components(const fdn_cal *k, uint64_t units, double t, fdn_comps *out);
int fdn_cal_date(const fdn_cal *k, const fdn_comps *c, double *out);
enum { FDN_CO_WRAP = 1, FDN_CO_STRICT = 2, FDN_CO_PREV_TIME = 1 << 8, FDN_CO_NEXT_TIME = 1 << 9,
       FDN_CO_NEXT_TIME_PRESERVE = 1 << 10, FDN_CO_PREV_TIME_PRESERVE = 1 << 11, FDN_CO_FIRST = 1 << 12,
       FDN_CO_LAST = 1 << 13, FDN_CO_BACKWARDS = 1 << 2 };
int fdn_cal_add(const fdn_cal *k, const fdn_comps *c, double t, uint64_t opts, double *out);
int fdn_cal_difference(const fdn_cal *k, uint64_t units, double from, double to, fdn_comps *out);
int fdn_cal_range(const fdn_cal *k, uint64_t smaller, uint64_t larger, double t, long *loc, long *len);
long fdn_cal_ordinality(const fdn_cal *k, uint64_t smaller, uint64_t larger, double t);
int fdn_cal_interval(const fdn_cal *k, uint64_t unit, double t, double *start, double *len);
int fdn_cal_limit(const fdn_cal *k, uint64_t unit, int maximum, long *loc, long *len);
int fdn_cal_is_weekend(const fdn_cal *k, double t);
int fdn_cal_weekend_range(const fdn_cal *k, double t, int next, double *start, double *len);
int fdn_cal_next_match(const fdn_cal *k, double after, const fdn_comps *match, uint64_t opts, double *out);
/* símbolos: udat_getSymbols do locale e calendário; cb para cada um */
enum { FDN_SYM_ERAS, FDN_SYM_LONG_ERAS, FDN_SYM_MONTHS, FDN_SYM_SHORT_MONTHS, FDN_SYM_VERY_SHORT_MONTHS,
       FDN_SYM_SA_MONTHS, FDN_SYM_SA_SHORT_MONTHS, FDN_SYM_SA_VERY_SHORT_MONTHS, FDN_SYM_WEEKDAYS,
       FDN_SYM_SHORT_WEEKDAYS, FDN_SYM_VERY_SHORT_WEEKDAYS, FDN_SYM_SA_WEEKDAYS, FDN_SYM_SA_SHORT_WEEKDAYS,
       FDN_SYM_SA_VERY_SHORT_WEEKDAYS, FDN_SYM_QUARTERS, FDN_SYM_SHORT_QUARTERS, FDN_SYM_SA_QUARTERS,
       FDN_SYM_SA_SHORT_QUARTERS, FDN_SYM_AM, FDN_SYM_PM };
int fdn_cal_symbols(const fdn_cal *k, int kind, fdn_each_fn cb, void *ctx);
