#!/bin/sh
# Teste no host do DirectoryService (userland/libsystem/dirservice.c) com contas falsas no lugar da SYS_ACCOUNT.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
cp "$ROOT/userland/libsystem/dirservice.c" "$T/"
echo '#pragma once' > "$T/libc.h"
cat > "$T/sys.h" <<'H'
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64; typedef int32_t i32;
typedef struct { char name[32]; char full[64]; u8 admin; u8 has_password; } mos_account_t;
enum { SYS_ACCOUNT = 107, MOS_ACCT_COUNT = 0, MOS_ACCT_GET, MOS_ACCT_CURRENT, MOS_ACCT_VERIFY };
long fake_sys(long n, long a, long b, long c);
#define SYS1(n, a) fake_sys(n, a, 0, 0)
#define SYS3(n, a, b, c) fake_sys(n, (long)(a), (long)(b), (long)(c))
H
cc -std=gnu11 -O1 -Wall -Wno-unused-parameter -Wno-builtin-declaration-mismatch -I"$T" "$T/dirservice.c" "$ROOT/tools/dirservice_host.c" -o "$T/t"
"$T/t"
