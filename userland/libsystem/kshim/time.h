#pragma once
typedef long time_t;
struct tm {
    int tm_sec, tm_min, tm_hour;
    int tm_mday, tm_mon, tm_year;
    int tm_wday, tm_yday, tm_isdst;
};
#include <stddef.h>
time_t time(time_t *t);
size_t strftime(char *s, size_t n, const char *fmt, const struct tm *tm);
struct tm *gmtime(const time_t *t);
struct tm *localtime(const time_t *t);
