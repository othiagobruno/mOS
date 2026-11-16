/* mOS — o mínimo do kernel/include/fs.h para os descompressores do kernel (inflate.c, xz.c, bzip2.c) dentro do
 * motor de pacotes (userland/pkg/pkgengine.c), no mOS (Mach-O) e no teste de host. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t i8; typedef int16_t i16; typedef int32_t i32; typedef int64_t i64;
typedef size_t usize;
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
static inline void *kmalloc(size_t n) { return malloc(n ? n : 1); }
static inline void *kcalloc(size_t n, size_t s) { return calloc(n ? n : 1, s ? s : 1); }
static inline void kfree(void *p) { free(p); }
#define kprintf(...) ((void)0)
typedef struct { int year, month, day, hour, minute, second, weekday; } datetime_t;
static inline u16 le16(const void *p) { const u8 *b = p; return (u16)(b[0] | b[1] << 8); }
static inline u32 le32(const void *p) { const u8 *b = p; return (u32)b[0] | (u32)b[1] << 8 | (u32)b[2] << 16 | (u32)b[3] << 24; }
static inline u64 le64(const void *p) { return (u64)le32(p) | (u64)le32((const u8 *)p + 4) << 32; }
static inline u16 be16(const void *p) { const u8 *b = p; return (u16)(b[0] << 8 | b[1]); }
static inline u32 be32(const void *p) { const u8 *b = p; return (u32)b[0] << 24 | (u32)b[1] << 16 | (u32)b[2] << 8 | b[3]; }
static inline u64 be64(const void *p) { return (u64)be32(p) << 32 | be32((const u8 *)p + 4); }
i64 zlib_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen);
i64 bzip2_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen);
i64 xz_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen);
i64 lzfse_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen);
i64 lzvn_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen);
i64 adc_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen);
