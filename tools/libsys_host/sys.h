/* shim de host: compila arquivos do userland/libsystem (attrlist.c, fsmeta.c) contra o kernel real (tools/attrlist_test.sh) */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64; typedef int32_t i32; typedef int64_t i64;
#include "syscall.h"
extern int cx_errno;
long host_sys(long n, long a, long b, long c);
#define SYS1(n, a) host_sys(n, (long)(a), 0, 0)
#define SYS2(n, a, b) host_sys(n, (long)(a), (long)(b), 0)
#define SYS3(n, a, b, c) host_sys(n, (long)(a), (long)(b), (long)(c))
size_t strlcpy(char *d, const char *s, size_t n);
size_t strlcat(char *d, const char *s, size_t n);
