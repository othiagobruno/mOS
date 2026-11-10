#include "commpage.h"
#ifdef MOS_HOST_TEST
#include <string.h>
#else
#include "kernel.h"
#endif

void commpage_fill(void *page) {
    unsigned char *p = page;
    const char *sig = "commpage 64-bit";
    int i;
    memset(p, 0, 4096);
    for (i = 0; sig[i]; i++) p[i] = (unsigned char)sig[i];
    *(unsigned short *)(p + 0x01E) = 7;
    p[0x022] = 1;
    p[0x025] = 14;
    *(unsigned short *)(p + 0x026) = 64;
}
