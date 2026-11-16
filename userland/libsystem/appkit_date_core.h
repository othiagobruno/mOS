/* mOS — núcleo do NSDatePicker (sem Objective-C): data civil <-> segundos, campos, passo por campo, grade do mês.
 * Teste de host: userland/tests-appkit/controls_host.c (-DCX_DT_HOST). */
#pragma once
typedef struct { int y, mo, d, h, mi, s; } dt_civil_t;
enum { DT_YEAR, DT_MONTH, DT_DAY, DT_HOUR, DT_MINUTE, DT_SECOND, DT_NFIELDS };
/* NSDatePickerElementFlags */
#define DT_EL_HOUR_MINUTE 0x000c
#define DT_EL_HOUR_MINUTE_SECOND 0x000e
#define DT_EL_YEAR_MONTH 0x00c0
#define DT_EL_YEAR_MONTH_DAY 0x00e0
int dt_days_in_month(int y, int mo);
long long dt_days_from_civil(int y, int mo, int d);        /* dias desde 1970-01-01 */
void dt_from_seconds(long long t, dt_civil_t *c);           /* t = segundos locais desde 1970 */
long long dt_to_seconds(const dt_civil_t *c);
int dt_weekday(int y, int mo, int d);                        /* 0 = domingo */
/* campos visíveis na ordem do pt-BR (dd/mm/aaaa hh:mm:ss) para os flags; devolve quantos */
int dt_fields(unsigned flags, int out[DT_NFIELDS]);
/* passo no campo: +1/-1 com volta dentro do campo (como o Mac); o dia se ajusta ao mês */
void dt_step(dt_civil_t *c, int field, int delta);
/* digitação no campo: acumula dígitos; devolve 1 quando o campo ficou completo (vai ao próximo) */
int dt_type_digit(dt_civil_t *c, int field, int digit, int *acc, int *nacc);
/* texto "dd/mm/aaaa hh:mm(:ss)"; spans[i] = início/fim de cada campo em bytes */
int dt_format(const dt_civil_t *c, unsigned flags, char *out, int cap, int spans[][2]);
/* grade do mês (6 semanas, começa no domingo): cell[42] = dia (negativo = mês vizinho) */
void dt_month_grid(int y, int mo, int cell[42]);
long long dt_clamp(long long t, long long mn, long long mx, int has_min, int has_max);
