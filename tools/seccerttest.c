/* mOS — teste de host de seccert_core.inc (PKCS#12, PEM, nomes X.509) e de cx_sectrust_eval (sectrust.c).
 * tools/seccerttest.py compila (com o CCCrypt do libSystem) e confere tudo contra a biblioteca cryptography.
 *
 *   seccerttest p12 ARQ SENHA|- PASTA       -> "certs N keys M", PASTA/cert<i>.der, key<i>.der, nomes e ids
 *   seccerttest pem ARQ PASTA               -> rótulos; PASTA/pem<i>.der
 *   seccerttest name CERT.der               -> CN, O, e-mail, atributos em OID pontilhado, notBefore (abs)
 *   seccerttest trust POLITICA HOST|- UNIX|-1 SISTEMA(0/1) RAIZES.pem|- CERT.der [CERT.der...] [-- ANCORA.der...]
 *                                           -> código X509_E_* e a mensagem */
#include "tls_shim.h"
#include "crypto.h"
#include "x509.h"
#include <time.h>

u64 timer_ticks(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (u64)t.tv_sec * 1000; }
void rtc_now(datetime_t *dt) { memset(dt, 0, sizeof *dt); }
u64 mach_absolute_time(void) { return timer_ticks() * 1000000ull; }
int cx_snprintf(char *b, size_t n, const char *f, ...) { va_list a; va_start(a, f); int r = vsnprintf(b, n, f, a); va_end(a); return r; }

#include "../userland/libsystem/seccert_core.inc"

/* sectrust.c no host: as raízes vêm de um arquivo PEM (no mOS vêm do kernel por MOS_NET_TLS_ROOTS) */
static char *g_roots;
static long g_roots_n;
long cx_syscall(long n, long a, long b, long c, long d, long e) {
    (void)n; (void)a; (void)d; (void)e;            /* (SYS_NET, MOS_NET_TLS_ROOTS, buf, max) */
    if (!g_roots) return -1;
    if (!b) return g_roots_n;
    memcpy((void *)b, g_roots, (size_t)(c < g_roots_n ? c : g_roots_n));
    return g_roots_n;
}
int cx_sectrust_eval(const u8 *const *ders, const u32 *lens, unsigned n, const u8 *const *anc, const u32 *alens,
                     unsigned na, int use_system, int policy, const char *host, long long at_unix, char *err,
                     unsigned errlen);

static u8 *slurp(const char *p, u32 *n) {
    FILE *f = fopen(p, "rb");
    if (!f) { perror(p); exit(2); }
    fseek(f, 0, SEEK_END);
    long l = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *b = malloc((size_t)l + 1);
    if (fread(b, 1, (size_t)l, f) != (size_t)l) exit(2);
    fclose(f);
    b[l] = 0;
    *n = (u32)l;
    return b;
}
static void spit(const char *dir, const char *name, int i, const u8 *b, u32 n) {
    char p[512];
    snprintf(p, sizeof p, "%s/%s%d.der", dir, name, i);
    FILE *f = fopen(p, "wb");
    fwrite(b, 1, n, f);
    fclose(f);
}
static void hex(const u8 *b, u32 n) { for (u32 i = 0; i < n; i++) printf("%02x", b[i]); }

/* commoncrypto.c traz o backtrace() do execinfo; o desenrolador fica em libc_bsd.c, fora deste teste */
int backtrace_from_fp(void *startfp, void **array, int size) { (void)startfp; (void)array; (void)size; return 0; }

int main(int argc, char **argv) {
    if (argc < 3) return 1;
    u32 n;
    if (!strcmp(argv[1], "p12") && argc == 5) {
        u8 *b = slurp(argv[2], &n);
        p12_out_t o;
        int r = p12_parse(b, n, strcmp(argv[3], "-") ? argv[3] : NULL, &o);
        if (r) { printf("erro %d\n", r); return 0; }
        printf("certs %d keys %d\n", o.ncerts, o.nkeys);
        for (int i = 0; i < o.ncerts; i++) { spit(argv[4], "cert", i, o.certs[i].der, o.certs[i].n); printf("cert %d nome=%s id=", i, o.certs[i].name); hex(o.certs[i].id, o.certs[i].idn); puts(""); }
        for (int i = 0; i < o.nkeys; i++) { spit(argv[4], "key", i, o.keys[i].der, o.keys[i].n); printf("key %d nome=%s id=", i, o.keys[i].name); hex(o.keys[i].id, o.keys[i].idn); puts(""); }
        for (int c = 0; c < o.ncerts; c++) for (int k = 0; k < o.nkeys; k++) if (p12_cert_matches_key(&o, c, k)) printf("par %d %d\n", c, k);
        return 0;
    }
    if (!strcmp(argv[1], "pem") && argc == 4) {
        u8 *b = slurp(argv[2], &n);
        sc_pem_iter it;
        const char *label;
        u8 *der;
        u32 dn;
        int enc, i = 0;
        sc_pem_begin(&it, b, n);
        while (sc_pem_next(&it, &label, &der, &dn, &enc)) { printf("%s%s\n", label, enc ? " (cifrado)" : ""); spit(argv[3], "pem", i++, der, dn); }
        sk_buf out = {0};
        sc_pem_write(&out, "CERTIFICATE", (const u8 *)"abc", 3);
        fwrite(out.b, 1, out.n, stderr);
        return 0;
    }
    if (!strcmp(argv[1], "name") && argc == 3) {
        u8 *b = slurp(argv[2], &n);
        x509_cert_t x;
        if (x509_parse(&x, b, n)) return 3;
        char v[256], oid[64];
        printf("CN=%s\n", sc_name_attr(x.subject, x.subject_len, OIDS_CN, sizeof OIDS_CN, v, sizeof v) ? v : "");
        printf("O=%s\n", sc_name_attr(x.subject, x.subject_len, OIDS_O, sizeof OIDS_O, v, sizeof v) ? v : "");
        printf("EMAIL=%s\n", sc_name_attr(x.subject, x.subject_len, OIDS_EMAIL, sizeof OIDS_EMAIL, v, sizeof v) ? v : "");
        printf("SAN=%s\n", sc_san_first(x.san, x.san_len, v, sizeof v) ? v : "");
        sc_attr_iter it;
        sc_attr_begin(&it, x.subject, x.subject_len);
        while (sc_attr_next(&it, oid, sizeof oid, v, sizeof v)) printf("ATTR %s=%s\n", oid, v);
        printf("NB=%.0f\nNA=%.0f\n", sc_abs_time(x.not_before), sc_abs_time(x.not_after));
        return 0;
    }
    if (!strcmp(argv[1], "trust") && argc >= 8) {
        int policy = atoi(argv[2]);
        const char *host = strcmp(argv[3], "-") ? argv[3] : NULL;
        long long at = atoll(argv[4]);
        int sys = atoi(argv[5]);
        if (strcmp(argv[6], "-")) { u32 rn; g_roots = (char *)slurp(argv[6], &rn); g_roots_n = rn; }
        const u8 *ders[10], *anc[10];
        u32 lens[10], alens[10];
        unsigned nc = 0, na = 0;
        int in_anc = 0;
        for (int i = 7; i < argc; i++) {
            if (!strcmp(argv[i], "--")) { in_anc = 1; continue; }
            u32 l;
            u8 *d = slurp(argv[i], &l);
            if (in_anc) { anc[na] = d; alens[na++] = l; } else { ders[nc] = d; lens[nc++] = l; }
        }
        char msg[200];
        int r = cx_sectrust_eval(ders, lens, nc, anc, alens, na, sys, policy, host, at, msg, sizeof msg);
        printf("%d %s\n", r, msg);
        return 0;
    }
    return 1;
}
