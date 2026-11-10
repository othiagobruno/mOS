#pragma once
typedef long time_t;
struct tm { int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst; };
struct tm *gmtime(const time_t *t);
unsigned long strftime(char *s, unsigned long max, const char *fmt, const struct tm *tm);
