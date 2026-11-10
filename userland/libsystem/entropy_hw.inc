/* Bytes para getentropy / chave do Keychain.
 * Host Linux: getrandom(2) do kernel. mOS: RDSEED e, se faltar, RDRAND.
 * Sem bytes da fonte, devolve -1 e zera o buffer. Não chama arc4random.
 */
#ifdef KC_HOST
#include <sys/random.h>
static int kc_entropy_fail;
static int kc_hwentropy(void *buf, size_t n) {
    if (!buf || !n || n > 256) return -1;
    if (kc_entropy_fail) { memset(buf, 0, n); return -1; }
    unsigned char *o = (unsigned char *)buf;
    size_t got = 0;
    while (got < n) {
        ssize_t k = getrandom(o + got, n - got, 0);
        if (k <= 0) { memset(buf, 0, n); return -1; }
        got += (size_t)k;
    }
    return 0;
}
#elif defined(__aarch64__)
static int kc_hwentropy(void *buf, size_t n) {
    if (buf && n && n <= 256) memset(buf, 0, n);
    return -1;
}
#else
static int kc_cpuid(unsigned leaf, unsigned sub, unsigned *a, unsigned *b, unsigned *c, unsigned *d) {
    unsigned aa, bb, cc, dd;
    __asm__ volatile("cpuid" : "=a"(aa), "=b"(bb), "=c"(cc), "=d"(dd) : "a"(leaf), "c"(sub));
    if (a) *a = aa;
    if (b) *b = bb;
    if (c) *c = cc;
    if (d) *d = dd;
    return 0;
}
static int kc_rdseed64(unsigned long long *v) {
    unsigned char ok;
    __asm__ volatile("rdseed %0; setc %1" : "=r"(*v), "=qm"(ok) :: "cc");
    return ok;
}
static int kc_rdrand64(unsigned long long *v) {
    unsigned char ok;
    __asm__ volatile("rdrand %0; setc %1" : "=r"(*v), "=qm"(ok) :: "cc");
    return ok;
}
static int kc_hwentropy(void *buf, size_t n) {
    unsigned a, b, c, d, maxleaf;
    int have_seed = 0, have_rand = 0;
    if (!buf || !n || n > 256) return -1;
    kc_cpuid(0, 0, &maxleaf, 0, 0, 0);
    kc_cpuid(1, 0, &a, &b, &c, &d);
    have_rand = (c & (1u << 30)) != 0;
    if (maxleaf >= 7) {
        kc_cpuid(7, 0, &a, &b, &c, &d);
        have_seed = (b & (1u << 18)) != 0;
    }
    if (!have_seed && !have_rand) { memset(buf, 0, n); return -1; }
    unsigned char *o = (unsigned char *)buf;
    size_t got = 0;
    while (got < n) {
        unsigned long long v = 0;
        int ok = 0;
        int tries;
        for (tries = 0; tries < 20 && !ok; tries++) {
            if (have_seed) ok = kc_rdseed64(&v);
            if (!ok && have_rand) ok = kc_rdrand64(&v);
        }
        if (!ok) { memset(buf, 0, n); return -1; }
        size_t take = n - got;
        if (take > 8) take = 8;
        memcpy(o + got, &v, take);
        got += take;
        v = 0;
    }
    return 0;
}
#endif
