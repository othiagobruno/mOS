/* Freestanding. O corpo está em libc.c (mac_tm_t tem o mesmo layout). */
#pragma once
typedef long time_t;
struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
    long tm_gmtoff;
    const char *tm_zone;
};
long time(long *t);
struct tm *gmtime(const long *t);
struct tm *localtime(const long *t);
unsigned long strftime(char *s, unsigned long max, const char *fmt, const struct tm *tm);
