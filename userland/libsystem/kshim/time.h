#pragma once
/* Freestanding: o sqlite3.c inclui <time.h>. Sem a libc do host. */
typedef long time_t;
struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
};
static inline time_t time(time_t *t) { if (t) *t = 0; return 0; }
static inline struct tm *gmtime(const time_t *t) { (void)t; return 0; }
static inline struct tm *localtime(const time_t *t) { (void)t; return 0; }
static inline unsigned long strftime(char *s, unsigned long max, const char *fmt, const struct tm *tm) {
    (void)fmt; (void)tm;
    if (max) s[0] = 0;
    return 0;
}
