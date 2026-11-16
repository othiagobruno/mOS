/* mOS — núcleo do NSDatePicker (ver appkit_date_core.h). Teste de host: -DCX_DT_HOST. */
#ifdef CX_DT_HOST
#include <string.h>
#else
#include "sys.h"
#include "libc.h"
#endif
#include "appkit_date_core.h"

int dt_days_in_month(int y, int mo) {
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (mo < 1 || mo > 12) return 30;
    if (mo == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return d[mo - 1];
}
long long dt_days_from_civil(int y, int mo, int d) {   /* algoritmo de H. Hinnant */
    y -= mo <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (unsigned)(mo + (mo > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}
void dt_from_seconds(long long t, dt_civil_t *c) {
    long long days = t >= 0 ? t / 86400 : -((-t + 86399) / 86400), sec = t - days * 86400;
    long long z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097), yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = (long long)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    c->d = (int)(doy - (153 * mp + 2) / 5 + 1);
    c->mo = (int)(mp < 10 ? mp + 3 : mp - 9);
    c->y = (int)(y + (c->mo <= 2));
    c->h = (int)(sec / 3600); c->mi = (int)(sec / 60 % 60); c->s = (int)(sec % 60);
}
long long dt_to_seconds(const dt_civil_t *c) { return dt_days_from_civil(c->y, c->mo, c->d) * 86400 + c->h * 3600LL + c->mi * 60LL + c->s; }
int dt_weekday(int y, int mo, int d) { long long n = dt_days_from_civil(y, mo, d); return (int)((n % 7 + 11) % 7); }   /* 1970-01-01 foi quinta */

int dt_fields(unsigned f, int out[DT_NFIELDS]) {
    int n = 0;
    if ((f & DT_EL_YEAR_MONTH_DAY) == DT_EL_YEAR_MONTH_DAY) out[n++] = DT_DAY;
    if (f & DT_EL_YEAR_MONTH) { out[n++] = DT_MONTH; out[n++] = DT_YEAR; }
    if (f & DT_EL_HOUR_MINUTE) { out[n++] = DT_HOUR; out[n++] = DT_MINUTE; }
    if ((f & DT_EL_HOUR_MINUTE_SECOND) == DT_EL_HOUR_MINUTE_SECOND) out[n++] = DT_SECOND;
    return n;
}
static int wrap(int v, int lo, int hi) { int span = hi - lo + 1; v = (v - lo) % span; if (v < 0) v += span; return v + lo; }
void dt_step(dt_civil_t *c, int f, int delta) {
    switch (f) {
    case DT_YEAR: c->y += delta; if (c->y < 1) c->y = 1; if (c->y > 9999) c->y = 9999; break;
    case DT_MONTH: c->mo = wrap(c->mo + delta, 1, 12); break;
    case DT_DAY: c->d = wrap(c->d + delta, 1, dt_days_in_month(c->y, c->mo)); break;
    case DT_HOUR: c->h = wrap(c->h + delta, 0, 23); break;
    case DT_MINUTE: c->mi = wrap(c->mi + delta, 0, 59); break;
    case DT_SECOND: c->s = wrap(c->s + delta, 0, 59); break;
    }
    int dim = dt_days_in_month(c->y, c->mo);
    if (c->d > dim) c->d = dim;
}
int dt_type_digit(dt_civil_t *c, int f, int digit, int *acc, int *nacc) {
    if (digit < 0 || digit > 9) return 0;
    *acc = *acc * 10 + digit; (*nacc)++;
    int width = f == DT_YEAR ? 4 : 2, v = *acc, done = *nacc >= width;
    switch (f) {
    case DT_YEAR: if (done) c->y = v < 1 ? 1 : v; else c->y = v; break;
    case DT_MONTH: if (v > 12) { v = digit; *acc = digit; *nacc = 1; } if (v >= 1) c->mo = v; if (v > 1) done = 1; break;
    case DT_DAY: { int dim = dt_days_in_month(c->y, c->mo); if (v > dim) { v = digit; *acc = digit; *nacc = 1; } if (v >= 1) c->d = v; if (v * 10 > dim) done = 1; break; }
    case DT_HOUR: if (v > 23) { v = digit; *acc = digit; *nacc = 1; } c->h = v; if (v > 2) done = 1; break;
    case DT_MINUTE: if (v > 59) { v = digit; *acc = digit; *nacc = 1; } c->mi = v; if (v > 5) done = 1; break;
    case DT_SECOND: if (v > 59) { v = digit; *acc = digit; *nacc = 1; } c->s = v; if (v > 5) done = 1; break;
    }
    if (done) { *acc = 0; *nacc = 0; }
    int dim = dt_days_in_month(c->y, c->mo);
    if (c->d > dim) c->d = dim;
    return done;
}
static int put2(char *o, int v) { o[0] = (char)('0' + v / 10 % 10); o[1] = (char)('0' + v % 10); return 2; }
int dt_format(const dt_civil_t *c, unsigned flags, char *out, int cap, int spans[][2]) {
    int fl[DT_NFIELDS], n = dt_fields(flags, fl), o = 0;
    if (cap < 24) { out[0] = 0; return 0; }
    for (int i = 0; i < n; i++) {
        int f = fl[i];
        if (i > 0) out[o++] = (f == DT_MONTH || f == DT_YEAR) ? '/' : (f == DT_HOUR) ? ' ' : ':';
        spans[i][0] = o;
        switch (f) {
        case DT_DAY: o += put2(out + o, c->d); break;
        case DT_MONTH: o += put2(out + o, c->mo); break;
        case DT_YEAR: { int y = c->y; out[o++] = (char)('0' + y / 1000 % 10); out[o++] = (char)('0' + y / 100 % 10); o += put2(out + o, y % 100); break; }
        case DT_HOUR: o += put2(out + o, c->h); break;
        case DT_MINUTE: o += put2(out + o, c->mi); break;
        case DT_SECOND: o += put2(out + o, c->s); break;
        }
        spans[i][1] = o;
    }
    out[o] = 0;
    return n;
}
void dt_month_grid(int y, int mo, int cell[42]) {
    int first = dt_weekday(y, mo, 1), dim = dt_days_in_month(y, mo);
    int py = mo == 1 ? y - 1 : y, pm = mo == 1 ? 12 : mo - 1, pdim = dt_days_in_month(py, pm);
    for (int i = 0; i < 42; i++) {
        int d = i - first + 1;
        if (d < 1) cell[i] = -(pdim + d);
        else if (d > dim) cell[i] = -(d - dim);
        else cell[i] = d;
    }
}
long long dt_clamp(long long t, long long mn, long long mx, int hmin, int hmax) {
    if (hmin && t < mn) t = mn;
    if (hmax && t > mx) t = mx;
    return t;
}
