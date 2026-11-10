/* Token de _AXUIElementCreateWithRemoteToken neste sistema.
 * 12 bytes: "mAX1", pid em u32 little-endian, número da janela em u32 little-endian.
 * Janela 0 é o elemento da aplicação. O formato da Apple não é este; um token que não
 * começa com mAX1 não vira elemento. */
#pragma once
#include <stdint.h>

static inline int cx_ax_token_read(const void *bytes, long n, int *pid, unsigned *wid) {
    const unsigned char *p = (const unsigned char *)bytes;
    unsigned u;
    if (!p || n < 12 || p[0] != 'm' || p[1] != 'A' || p[2] != 'X' || p[3] != '1') return 0;
    u = (unsigned)p[4] | ((unsigned)p[5] << 8) | ((unsigned)p[6] << 16) | ((unsigned)p[7] << 24);
    if (pid) *pid = (int)u;
    u = (unsigned)p[8] | ((unsigned)p[9] << 8) | ((unsigned)p[10] << 16) | ((unsigned)p[11] << 24);
    if (wid) *wid = u;
    return 1;
}
