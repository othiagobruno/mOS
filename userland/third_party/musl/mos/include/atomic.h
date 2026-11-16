/* mOS: o único pedaço do atomic.h do musl que a libm usa (fma.c) */
#ifndef _MOS_MUSL_ATOMIC_H
#define _MOS_MUSL_ATOMIC_H
#include <stdint.h>
static inline int a_clz_64(uint64_t x) { return x ? __builtin_clzll(x) : 64; }
#endif
