#pragma once
#include <stddef.h>
#include <stdint.h>
typedef long ssize_t;
typedef int pid_t;
typedef long long off_t;
typedef unsigned int useconds_t;
typedef int suseconds_t;
typedef long time_t;
#ifndef MOS_TIMESPEC
#define MOS_TIMESPEC
struct timespec { time_t tv_sec; long tv_nsec; };
#endif
