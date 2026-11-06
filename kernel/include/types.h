/* mOS — tipos fundamentais (freestanding) */
#pragma once

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef signed char        i8;
typedef signed short       i16;
typedef signed int         i32;
typedef signed long long   i64;
typedef u64                usize;
typedef i64                isize;
typedef u64                uptr;
typedef _Bool              bool;

#define true  1
#define false 0
#define NULL  ((void *)0)

#define PACKED      __attribute__((packed))
#define ALIGNED(n)  __attribute__((aligned(n)))
#define NORETURN    __attribute__((noreturn))
#define UNUSED      __attribute__((unused))
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS_DIFF(a, b) ((a) > (b) ? (a) - (b) : (b) - (a))
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
#define ALIGN_UP(v, a)   (((v) + ((a) - 1)) & ~((u64)(a) - 1))
#define ALIGN_DOWN(v, a) ((v) & ~((u64)(a) - 1))

typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v)      __builtin_va_end(v)
#define va_arg(v, t)   __builtin_va_arg(v, t)
