#pragma once
#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif
struct timeval { time_t tv_sec; suseconds_t tv_usec; };   /* Darwin: suseconds_t de 32 bits */
struct timezone { int tz_minuteswest, tz_dsttime; };
int gettimeofday(struct timeval *tv, void *tz);
#ifdef __cplusplus
}
#endif
