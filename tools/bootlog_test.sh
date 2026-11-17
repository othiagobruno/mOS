#!/bin/sh
# Log de boot (kernel/fs/bootlog.c): 3 boots num disco falso, lidos pelo tools/ler-boot-log.py.
# 1 e 2: N sobe e o slot alterna; 3: passa de 2 MiB e para com "...[cortado]"; nenhuma escrita fora do boot.log.
set -e
cd "$(dirname "$0")/.."
D=$(mktemp -d)
trap 'rm -rf "$D"' EXIT
cat > "$D/kernel.h" <<'H'
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64; typedef int64_t i64; typedef size_t usize;
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define kprintf(...) printf(__VA_ARGS__)
static inline void kfree(void *p) { free(p); }
static inline void *kmalloc(usize n) { return malloc(n); }
void *kmalloc_pages(usize n);
static inline u64 irq_save(void) { return 0; }
static inline void irq_restore(u64 f) { (void)f; }
u64 timer_ticks(void);
H
cat > "$D/fs.h" <<'H'
#pragma once
#include "kernel.h"
typedef struct blkdev { u64 size; bool (*read)(struct blkdev *, u64, void *, u64); bool (*write)(struct blkdev *, u64, const void *, u64); bool (*flush)(struct blkdev *); } blkdev_t;
typedef struct fatfs fatfs_t;
int fat_extents(fatfs_t *fs, const char *path, u64 *off, u64 *len, int max, u64 *size);
H
cat > "$D/storage.h" <<'H'
#pragma once
#include "fs.h"
blkdev_t *storage_disk(int i);
bool storage_allow_log(blkdev_t *disk, u64 off, u64 len);
H
cat > "$D/update.h" <<'H'
#pragma once
const char *update_current_version(void);
H
gcc -O1 -g -w -fsanitize=address,undefined -I"$D" tools/bootlog_test.c kernel/fs/bootlog.c -o "$D/t"
# o boot.log no disco: partição em 1 MiB, faixas [1M+256K, +3M) e [1M+4M, +1M): monta o arquivo como o Ubuntu o veria
extrai() { python3 - "$D/disco" "$D/boot.log" <<'P'
import sys
d = open(sys.argv[1], 'rb').read(); P = 1 << 20
open(sys.argv[2], 'wb').write(d[P + (256 << 10):P + (256 << 10) + (3 << 20)] + d[P + (4 << 20):P + (5 << 20)])
P
}
"$D/t" "$D/disco" 100 >/dev/null
"$D/t" "$D/disco" 5000 >/dev/null
extrai
python3 tools/ler-boot-log.py --todos "$D/boot.log" > "$D/l2.txt"
grep -q '^===== mOS boot 2 0.1-teste =====$' "$D/l2.txt"
grep -q '^===== mOS boot 1 0.1-teste =====$' "$D/l2.txt"
[ "$(grep -c 'fim do boot' "$D/l2.txt")" = 2 ]
[ "$(grep -c 'mOS: iniciando o Nucleo' "$D/l2.txt")" = 2 ]
"$D/t" "$D/disco" 3000000 >/dev/null                    # boot 3, no slot 1: maior que o slot
extrai
python3 tools/ler-boot-log.py "$D/boot.log" > "$D/l3.txt"
head -1 "$D/l3.txt" | grep -q '^===== mOS boot 3 0.1-teste =====$'
tail -1 "$D/l3.txt" | grep -q '^\.\.\.\[cortado\]$'
! grep -q 'fim do boot' "$D/l3.txt"
python3 tools/ler-boot-log.py --todos "$D/boot.log" | grep -q '^===== mOS boot 2 '   # o slot do boot 2 ficou intacto
python3 - "$D/disco" <<'P'
import sys
d = open(sys.argv[1], 'rb').read(); P = 1 << 20
fora = d[:P + (256 << 10)] + d[P + (256 << 10) + (3 << 20):P + (4 << 20)] + d[P + (5 << 20):]
assert fora.count(0) == len(fora), "byte fora do boot.log mudou"
P
echo "bootlog: 3 boots, slots alternados, corte em 2 MiB, nada fora do boot.log: ok"
