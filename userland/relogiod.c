/* mOS — serviço de exemplo do init: grava a hora e o tempo ligado em /tmp/relogio.txt a cada N
 * segundos (argumento, padrão 10). Mostra um daemon de usuário dormindo sem gastar CPU. */
#include "mos.h"

void _start(const char *arg) {
    u64 every = arg && *arg ? (u64)atoi(arg) : 10;
    if (!every) every = 10;
    for (;;) {
        i64 t = unixtime();
        u64 up = time_ms() / 1000;
        char buf[128];
        int n = 0;
        const char *pre = "unix ";
        for (const char *s = pre; *s; s++) buf[n++] = *s;
        char d[24]; int k = 0; u64 v = (u64)t;
        do { d[k++] = (char)('0' + v % 10); v /= 10; } while (v);
        while (k) buf[n++] = d[--k];
        const char *mid = " | ligado há ";
        for (const char *s = mid; *s; s++) buf[n++] = *s;
        v = up;
        do { d[k++] = (char)('0' + v % 10); v /= 10; } while (v);
        while (k) buf[n++] = d[--k];
        buf[n++] = 's';
        buf[n++] = '\n';
        write_file("/tmp/relogio.txt", buf, (usize)n);
        sleep_ms(every * 1000);
    }
}
