/* Shim só do teste de host (tools/libcx86test.c). Não entra no build do mOS. */
#pragma once
#include <stddef.h>
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char i8;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef unsigned long usize;
typedef long ssize_t;
typedef long off_t;
extern int cx_errno;
enum { CX_EBADF = 9, CX_EINVAL = 22, CX_ENOMEM = 12, CX_ERANGE = 34 };
#define MAC_O_WRONLY 0x0001
#define MAC_O_RDWR   0x0002
#define MAC_O_APPEND 0x0008
#define MAC_O_CREAT  0x0200
#define MAC_O_TRUNC  0x0400
#define MAC_O_EXCL   0x0800
