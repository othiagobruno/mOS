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

/* ---- avaliação geral do SecTrust (security.c): âncoras do app, só âncoras, data, política básica ou SSL.
 * policy: 0 = X.509 básica (sem nome nem EKU), 1 = servidor SSL (host pode ser NULL: sem conferir o nome),
 * 2 = cliente SSL. at_unix < 0 = agora. Devolve um X509_E_* (0 = confiável) e a mensagem em err. */
int cx_sectrust_eval(const u8 *const *ders, const u32 *lens, unsigned n, const u8 *const *anc, const u32 *alens,
                     unsigned na, int use_system, int policy, const char *host, long long at_unix, char *err,
                     unsigned errlen) {
    if (err && errlen) err[0] = 0;
    if (!n || !ders) return X509_E_PARSE;
    if (n > MAX_CHAIN) n = MAX_CHAIN;
    const trust_t *sys = 0;
    if (use_system && ensure_roots()) sys = trust_get();
    u32 total = na + (sys ? sys->n : 0);
    trust_t t;
    t.n = 0;
    t.der = 0;
    t.a = malloc((total ? total : 1) * sizeof(anchor_t));
    vctx_t v;
    memset(&v, 0, sizeof v);
    v.c = malloc(n * sizeof(x509_cert_t));
    int e;
    if (!t.a || !v.c) { e = X509_E_NO_MEM; goto out; }
    for (unsigned i = 0; i < na; i++) {
        anchor_t *a = &t.a[t.n];
        if (x509_parse(&a->c, anc[i], alens[i]) != X509_OK) continue;
        a->h = fnv(a->c.subject, a->c.subject_len);
        t.n++;
    }
    for (u32 i = 0; sys && i < sys->n; i++) t.a[t.n++] = sys->a[i];
    v.n = n;
    e = x509_parse(&v.c[0], ders[0], lens[0]);
    if (e) { if (err) snprintf(err, errlen, "certificado malformado (DER/X.509 inválido)"); goto out; }
    v.used[0] = true;
    for (u32 i = 1; i < n; i++)
        if (x509_parse(&v.c[i], ders[i], lens[i]) != X509_OK) v.used[i] = true;
    datetime_t now;
    unix_to_dt(at_unix >= 0 ? (long)at_unix : time(0), &now);
    v.check_time = true;
    v.now = (u64)now.year * 10000000000ull + (u64)now.month * 100000000ull + (u64)now.day * 1000000ull +
            (u64)now.hour * 10000ull + (u64)now.minute * 100ull + (u64)now.second;
    v.t = &t;
    const x509_cert_t *leaf = &v.c[0];
    v.err_cert = leaf;
    if (!t.n) { e = X509_E_UNKNOWN_ISSUER; goto fmt; }
    if ((e = key_ok(leaf)) || (leaf->unknown_critical && (e = X509_E_CRITICAL_EXT))) goto fmt;
    if ((e = check_dates(&v, leaf))) goto fmt;
    if (!anchor_same(&t, leaf) && (e = build(&v, leaf, 0))) goto fmt;
    v.err_cert = leaf;
    if (policy == 1 && ((leaf->has_eku && !leaf->eku_server) || (leaf->has_ku && !(leaf->ku & 1)))) { e = X509_E_KEY_USAGE; goto fmt; }
    if (policy == 1 && host && host[0] && !x509_match_host(leaf, host)) { e = X509_E_NAME; goto fmt; }
    e = X509_OK;
    goto out;
fmt:
    if (err) format_err(e, &v, host, err, errlen);
out:
    free(v.c);
    free(t.a);
    return e;
}
