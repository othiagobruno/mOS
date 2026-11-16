/*
 * mOS libSystem — núcleo dos formatters da Foundation (sem Objective-C), sobre a ICU.
 * NSDateFormatter, NSISO8601DateFormatter, NSNumberFormatter, NSByteCountFormatter, NSDateComponentsFormatter,
 * NSRelativeDateTimeFormatter, NSListFormatter, NSDateIntervalFormatter e NSPersonNameComponentsFormatter
 * (fdn_formatters.c) montam a configuração e chamam estas funções. Textos UTF-8; datas em segundos desde 1970.
 */
#pragma once
#include <stdint.h>
#include "fdn_i18n_core.h"

/* ---- datas */
typedef struct {
    char locale[96], cal[24], tz[64];
    char pattern[256];         /* "" = sai dos estilos */
    int date_style, time_style;  /* NSDateFormatterStyle: 0 none, 1 short, 2 medium, 3 long, 4 full */
    int relative, lenient;
    int has_two_digit;
    double two_digit_start;
    int first_weekday, min_days;
    void *udat;                /* cache; fdn_df_reset quando a configuração muda */
} fdn_df;

void fdn_df_init(fdn_df *f);
void fdn_df_reset(fdn_df *f);
int fdn_df_format(fdn_df *f, double t, char *out, int cap);
int fdn_df_parse(fdn_df *f, const char *s, int32_t start, double *t, int32_t *end);
int fdn_df_pattern(fdn_df *f, char *out, int cap);
int fdn_df_template(const char *locale, const char *tmpl, char *out, int cap);
int fdn_iso_pattern(unsigned long options, char *out, int cap);

/* ---- números */
enum { FDN_NS_NONE, FDN_NS_DECIMAL, FDN_NS_CURRENCY, FDN_NS_PERCENT, FDN_NS_SCIENTIFIC, FDN_NS_SPELLOUT, FDN_NS_ORDINAL,
       FDN_NS_CURRENCY_ISO = 8, FDN_NS_CURRENCY_PLURAL, FDN_NS_CURRENCY_ACCOUNTING };
void *fdn_nf_open(int ns_style, const char *locale);
void fdn_nf_close(void *nf);
int fdn_nf_format_double(void *nf, double v, char *out, int cap);
int fdn_nf_format_int64(void *nf, int64_t v, char *out, int cap);
int fdn_nf_format_decimal(void *nf, const char *dec, char *out, int cap);
/* 1 com sucesso; *end recebe o índice (em UTF-16) até onde leu. is_int volta 1 se o texto era inteiro. */
int fdn_nf_parse(void *nf, const char *s, double *v, int64_t *iv, int *is_int, int32_t *end, int32_t *len16);
int fdn_nf_get_attr(void *nf, int attr);
void fdn_nf_set_attr(void *nf, int attr, int v);
double fdn_nf_get_dattr(void *nf, int attr);
void fdn_nf_set_dattr(void *nf, int attr, double v);
int fdn_nf_get_symbol(void *nf, int sym, char *out, int cap);
void fdn_nf_set_symbol(void *nf, int sym, const char *s);
int fdn_nf_get_text(void *nf, int which, char *out, int cap);
void fdn_nf_set_text(void *nf, int which, const char *s);
int fdn_nf_pattern(void *nf, char *out, int cap);
int fdn_nf_apply_pattern(void *nf, const char *p);
int fdn_currency_digits(const char *code, double *rounding);

/* ---- unidades e listas */
/* "duration-hour" etc.; width 0 = curto, 1 = estreito, 2 = completo */
int fdn_unit_format(const char *locale, const char *unit, double v, int width, int frac_digits, char *out, int cap);
int fdn_list_format(const char *locale, int type, int width, const char *const *items, int n, char *out, int cap);
int fdn_spellout(const char *locale, double v, char *out, int cap);
/* NSRelativeDateTimeFormatter: unidade URelativeDateTimeUnit, width 0 longo 1 curto 2 estreito */
int fdn_reldate(const char *locale, double v, int unit, int width, int named, int spellout, char *out, int cap);
int fdn_interval_format(const char *locale, const char *skeleton, const char *tz, double a, double b, char *out, int cap);
/* NSByteCountFormatter: estilo 0 arquivo, 1 memória, 2 decimal, 3 binário */
int fdn_bytes_format(const char *locale, int64_t bytes, unsigned allowed, int style, int nonnumeric, int unit, int count,
                     int actual, int adaptive, int zero_pad, char *out, int cap);
/* NSDateComponentsFormatter.unitsStyle e as partes já calculadas (só as unidades presentes) */
typedef struct { const char *unit; double value; } fdn_dur_part;
int fdn_duration_format(const char *locale, int style, const fdn_dur_part *p, int n, int negative, char *out, int cap);
