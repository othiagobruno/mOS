#pragma once
#include "../shim/stdint.h"
typedef long intptr_t;
typedef long long intmax_t;
typedef unsigned long long uintmax_t;
#ifndef INT16_MAX
#define INT8_MAX 127
#define INT8_MIN (-128)
#define UINT8_MAX 255
#define INT16_MAX 32767
#define INT16_MIN (-32768)
#define UINT16_MAX 65535
#endif
