/* mOS — base comum do driver AMD (kernel/dev/amdgpu).
 *
 * Os módulos de lógica pura (EDID, cabeçalhos de firmware, ATOM, cálculos de modo) incluem só este
 * arquivo e compilam também no host com -DMOS_HOST_TEST (tools/amdgpu_host.c). Os módulos que tocam
 * no hardware incluem amdgpu.h, que puxa o kernel inteiro. */
#pragma once

#ifdef MOS_HOST_TEST
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef unsigned long long u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef long long i64;
typedef size_t   usize;
#ifndef ARRAY_LEN
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#else
#include "types.h"
void  *memcpy(void *d, const void *s, usize n);
void  *memset(void *d, int c, usize n);
int    memcmp(const void *a, const void *b, usize n);
usize  strlen(const char *s);
int    snprintf(char *buf, usize n, const char *fmt, ...);
#endif

static inline u16 amd_le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static inline u32 amd_le24(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16; }
static inline u32 amd_le32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
