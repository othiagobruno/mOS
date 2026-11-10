#pragma once
typedef unsigned long size_t;
typedef long time_t;
struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
};
static inline time_t time(time_t *t) { if (t) *t = 0; return 0; }
struct tm *gmtime(const time_t *t);
size_t strftime(char *s, size_t n, const char *fmt, const struct tm *tm);
