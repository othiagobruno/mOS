#pragma once
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned long size_t;
typedef unsigned long uintptr_t;
#define INT32_MAX 2147483647
#define UINT32_MAX 4294967295u
#define INT64_MAX 9223372036854775807LL
#define UINT64_MAX 18446744073709551615ULL
#define INT32_MIN (-2147483647-1)
#define INT64_MIN (-9223372036854775807LL-1)
#define UINT64_C(c) (c ## ULL)
#define UINT32_C(c) (c ## U)
#define INT64_C(c) (c ## LL)
#define INT32_C(c) (c)
#define SIZE_MAX (~(size_t)0)
