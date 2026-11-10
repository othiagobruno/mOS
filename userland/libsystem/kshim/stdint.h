/* Cabeçalhos mínimos para o alvo freestanding (audiodec.c e o minimp3/ALAC). */
#pragma once
#include "../sys.h"
typedef long long intmax_t;
typedef unsigned long long uintmax_t;
#ifndef UINT8_MAX
#define UINT8_MAX 255
#endif
#ifndef UINT16_MAX
#define UINT16_MAX 65535
#endif
#ifndef UINT32_MAX
#define UINT32_MAX 0xffffffffu
#endif
#ifndef UINT64_MAX
#define UINT64_MAX 18446744073709551615ULL
#endif
#ifndef INT32_MAX
#define INT32_MAX 2147483647
#endif
#ifndef INT64_MAX
#define INT64_MAX 9223372036854775807LL
#endif
#ifndef INT32_MIN
#define INT32_MIN (-2147483647-1)
#endif
#define UINT64_C(c) (c ## ULL)
#define INT64_C(c) (c ## LL)
#define UINT32_C(c) (c ## U)
#define INT32_C(c) (c)
