/* Avaliação de cadeia no userspace, com o mesmo x509.c do kernel.
 * As raízes são as da Mozilla que o kernel já embute (MOS_NET_TLS_ROOTS).
 * Não há modo que aceite certificado inválido. */
#define MOS_HOST_TEST 1
#include "../../kernel/net/x509.c"
#include "../../kernel/include/syscall.h"

extern long cx_syscall(long n, long a, long b, long c, long d, long e);
extern void *malloc(size_t n);
extern void free(void *p);
extern long time(long *t);

static int roots_loaded;

static int ensure_roots(void) {
    if (x509_trust_count()) return 1;
    if (roots_loaded) return 0;
    roots_loaded = 1;
    long n = cx_syscall(SYS_NET, MOS_NET_TLS_ROOTS, 0, 0, 0, 0);
    if (n <= 0) return 0;
    char *pem = malloc((size_t)n + 1);
    if (!pem) return 0;
    if (cx_syscall(SYS_NET, MOS_NET_TLS_ROOTS, (long)pem, n, 0, 0) != n) { free(pem); return 0; }
    pem[n] = 0;
    int k = x509_trust_load_pem(pem, (u32)n);
    free(pem);
    return k > 0;
}

static void unix_to_dt(long s, datetime_t *dt) {
    long days = s / 86400, rem = s % 86400;
    if (rem < 0) { rem += 86400; days--; }
    dt->hour = (int)(rem / 3600);
    dt->minute = (int)(rem % 3600 / 60);
    dt->second = (int)(rem % 60);
    long y = 1970;
    for (;;) {
        int leap = (y % 4 == 0 && y % 100) || y % 400 == 0;
        long n = leap ? 366 : 365;
        if (days < n) break;
        days -= n;
        y++;
    }
    static const int md[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int leap = (y % 4 == 0 && y % 100) || y % 400 == 0, m = 0;
    while (m < 12) {
        int n = md[m] + (m == 1 && leap);
        if (days < n) break;
        days -= n;
        m++;
    }
    dt->year = (int)y;
    dt->month = m + 1;
    dt->day = (int)days + 1;
    dt->weekday = 0;
}

int cx_sectrust_verify(const u8 *const *ders, const u32 *lens, unsigned n, const char *host) {
    if (!n || !ders || !host || !host[0]) return 0;
    if (!ensure_roots()) return 0;
    datetime_t now;
    unix_to_dt(time(0), &now);
    char err[160];
    return x509_verify_chain(ders, lens, n, host, &now, err, sizeof err, 0, 0) == X509_OK;
}
