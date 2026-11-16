/* shim de host para compilar userland/libsystem/acl.c (tools/acl_test.c) */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint8_t u8; typedef uint32_t u32; typedef uint64_t u64;
#include "syscall.h"
extern int cx_errno;
long host_syscall(long n, long a, long b);
#define SYS2(n, a, b) host_syscall(n, (long)(a), (long)(b))
