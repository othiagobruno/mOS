/* Cabeçalhos mínimos para o alvo freestanding (audiodec.c e o minimp3/ALAC). */
#pragma once
#include "../sys.h"
typedef long long intmax_t;
typedef unsigned long long uintmax_t;
#define UINT32_MAX 0xffffffffu
#define INT32_MAX 2147483647
#define UINT64_C(c) (c ## ULL)
#define INT64_C(c) (c ## LL)
#define UINT32_C(c) (c ## U)
#define INT32_C(c) (c)
