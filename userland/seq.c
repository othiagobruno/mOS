/* mOS — seq N: escreve os números de 1 a N, um por linha (gerador para testar pipes) */
#include "mos.h"

void _start(const char *arg) {
    int n = arg ? atoi(arg) : 10;
    char buf[4096];
    usize o = 0;
    for (int i = 1; i <= n; i++) {
        char d[16];
        int k = 0, v = i;
        do { d[k++] = (char)('0' + v % 10); v /= 10; } while (v);
        while (k) buf[o++] = d[--k];
        buf[o++] = '\n';
        if (o > sizeof buf - 16) { write(1, buf, o); o = 0; }
    }
    if (o) write(1, buf, o);
    exit(0);
}
