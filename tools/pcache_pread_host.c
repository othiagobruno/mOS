/* mOS — page cache (kernel/fs/pcache.c) contra pread, no host: 200 mil leituras de tamanhos e padrões variados, pelo
 * disco e por uma partição filha (base 1 MiB), comparadas com a leitura direta do arquivo de imagem.
 *   gcc -std=gnu11 -O1 -w -DMOS_HOST_TEST -Itools -Ikernel/include -I. tools/pcache_pread_host.c kernel/fs/pcache.c -o /tmp/pcpr
 *   /tmp/pcpr build/mOS.img */

#include "fs.h"
#include "syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
long long g_live_allocs, g_live_bytes;
void rtc_now(datetime_t *dt) { memset(dt, 0, sizeof *dt); }
u32 rand32(void) { return 1; }
static int fd;
static bool rd(blkdev_t *d, u64 off, void *buf, u64 len) { (void)d; return pread(fd, buf, len, off) == (ssize_t)len; }
int main(int argc, char **argv) {
    fd = open(argv[1], O_RDONLY);
    u64 sz = lseek(fd, 0, SEEK_END);
    static blkdev_t disk, part;
    disk.size = sz; disk.read = rd; disk.lbs = 512;
    part.size = sz - (1 << 20); part.base = 1 << 20; part.parent = &disk; part.read = rd; part.lbs = 512;
    u8 *a = malloc(1 << 20), *b = malloc(1 << 20);
    srand(7);
    int bad = 0;
    for (int k = 0; k < 200000 && bad < 5; k++) {
        blkdev_t *d = (k & 1) ? &part : &disk;
        u64 base = d == &part ? (1 << 20) : 0;
        u64 len, off;
        int mode = rand() % 4;
        if (mode == 0) { len = 512; off = (u64)(rand() % (int)(d->size / 512 - 1)) * 512; }
        else if (mode == 1) { len = 512 * (1 + rand() % 64); off = (u64)(rand() % 20000) * 512; }
        else if (mode == 2) { static u64 seq; len = 4096; off = seq; seq = (seq + 4096) % (d->size - 8192); }
        else { len = 1 + rand() % 70000; off = rand() % (int)(d->size - len); }
        if (off + len > d->size) continue;
        if (!blk_read(d, off, a, len)) { printf("blk_read falhou k=%d\n", k); bad++; continue; }
        if (pread(fd, b, len, base + off) != (ssize_t)len) continue;
        if (memcmp(a, b, len)) { u64 i = 0; while (a[i] == b[i]) i++; printf("DIFERENTE k=%d modo=%d %s off=%llu len=%llu primeiro byte errado em +%llu\n", k, mode, d == &part ? "part" : "disco", (unsigned long long)off, (unsigned long long)len, (unsigned long long)i); bad++; }
    }
    printf("%s\n", bad ? "FALHOU" : "ok: 200000 leituras iguais ao pread");
    return bad != 0;
}
