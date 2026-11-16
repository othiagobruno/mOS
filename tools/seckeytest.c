/* mOS — teste de host do núcleo do SecKey (userland/libsystem/seckey_core.inc).
 *
 *   gcc -O2 -DMOS_HOST_TEST -Itools -Ikernel/include tools/seckeytest.c kernel/net/crypto.c kernel/net/pk.c -o /tmp/seckeytest
 *   python3 -I tools/seckeytest.py /tmp/seckeytest      (confere contra a biblioteca cryptography / OpenSSL)
 *
 * Comandos (as chaves estão no formato externo do macOS: PKCS#1 DER para RSA, X9.63 para EC):
 *   gen rsa|ec BITS saida            pub chave saida           spki chave saida
 *   import entrada saida             (qualquer formato aceito -> formato externo)
 *   sign ALG chave msg sig           verify ALG chave msg sig  (código de saída 0 = válida)
 *   enc ALG chave in out             dec ALG chave in out
 *   ecdh ALG priv pubpar out [tamanho [info]]
 *   self                             (autoteste sem arquivos) */
#include "tls_shim.h"
#include "crypto.h"
#include "x509.h"
#include <time.h>

u64 timer_ticks(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (u64)t.tv_sec * 1000 + (u64)t.tv_nsec / 1000000; }
void rtc_now(datetime_t *dt) { memset(dt, 0, sizeof *dt); }

#include "../userland/libsystem/seckey_core.inc"

static u8 *slurp(const char *p, usize *n) {
    FILE *f = fopen(p, "rb");
    if (!f) { perror(p); exit(2); }
    fseek(f, 0, SEEK_END);
    long l = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *b = malloc((usize)l + 1);
    if (fread(b, 1, (usize)l, f) != (usize)l) exit(2);
    fclose(f);
    *n = (usize)l;
    return b;
}
static void spit(const char *p, const u8 *b, usize n) {
    FILE *f = fopen(p, "wb");
    if (!f || fwrite(b, 1, n, f) != n) { perror(p); exit(2); }
    fclose(f);
}
static sk_key *load(const char *p) {
    usize n;
    u8 *b = slurp(p, &n);
    sk_key *k = sk_import(b, n, 0, -1);
    free(b);
    if (!k) { fprintf(stderr, "chave inválida: %s\n", p); exit(3); }
    return k;
}
static sk_alg alg_of(const char *s) {
    sk_alg a;
    if (sk_parse_alg(s, &a)) { fprintf(stderr, "algoritmo desconhecido: %s\n", s); exit(4); }
    return a;
}

static int selftest(void) {
    int fails = 0;
    /* divisão e exponenciação contra valores pequenos conferíveis */
    rb_t a, b, q, r, e, m;
    u8 x[64];
    for (int i = 0; i < 64; i++) x[i] = (u8)(i * 37 + 11);
    rb_from_be(&a, x, 64);
    rb_from_be(&b, x + 3, 20);
    rb_divmod(&q, &r, &a, &b);
    rb_t chk;
    rb_mul(&chk, &q, &b);
    rb_add(&chk, &chk, &r);
    if (rb_cmp(&chk, &a) || rb_cmp(&r, &b) >= 0) { puts("FALHA divmod"); fails++; }
    rb_set(&b, 3); rb_set(&e, 200); rb_set(&m, 1000003);
    rb_modexp(&r, &b, &e, &m);
    u64 v = 1;
    for (int i = 0; i < 200; i++) v = v * 3 % 1000003;
    if (r.n != 1 || r.d[0] != v) { printf("FALHA modexp %u %llu\n", r.d[0], (unsigned long long)v); fails++; }
    /* SHA-224("abc") */
    static const u8 s224[28] = {0x23, 0x09, 0x7d, 0x22, 0x34, 0x05, 0xd8, 0x22, 0x86, 0x42, 0xa4, 0x77, 0xbd, 0xa2,
                                0x55, 0xb3, 0x2a, 0xad, 0xbc, 0xe4, 0xbd, 0xa0, 0xb3, 0xf7, 0xe3, 0x6c, 0x9d, 0xa7};
    sk_hash(SKH_SHA224, (const u8 *)"abc", 3, x);
    if (memcmp(x, s224, 28)) { puts("FALHA sha224"); fails++; }
    /* RSA 1024 ida e volta em todos os paddings; EC nas três curvas */
    sk_key *k = sk_rsa_generate(1024);
    if (!k) { puts("FALHA gerar RSA"); return fails + 1; }
    const char *algs[] = {"algid:RSASignatureMessagePKCS1v15SHA256", "algid:RSASignatureMessagePSSSHA384",
                          "algid:RSASignatureDigestPKCS1v15Raw", "algid:sign:RSA:message-PSS:SHA1"};
    for (unsigned i = 0; i < 4; i++) {
        sk_alg al = alg_of(algs[i]);
        u8 *sig; usize sn;
        if (sk_sign(k, &al, (const u8 *)"mOS", 3, &sig, &sn) || sk_verify(k, &al, (const u8 *)"mOS", 3, sig, sn) ||
            !sk_verify(k, &al, (const u8 *)"mOs", 3, sig, sn)) { printf("FALHA %s\n", algs[i]); fails++; }
        else free(sig);
    }
    const char *encs[] = {"algid:RSAEncryptionPKCS1", "algid:RSAEncryptionOAEPSHA256", "algid:RSAEncryptionOAEPSHA1"};
    for (unsigned i = 0; i < 3; i++) {
        sk_alg al = alg_of(encs[i]);
        u8 *c, *p; usize cn, pn;
        if (sk_encrypt(k, &al, (const u8 *)"segredo", 7, &c, &cn) || sk_decrypt(k, &al, c, cn, &p, &pn) || pn != 7 ||
            memcmp(p, "segredo", 7)) { printf("FALHA %s\n", encs[i]); fails++; }
    }
    u8 *ex; usize exn;
    sk_export(k, &ex, &exn);
    sk_key *k2 = sk_import(ex, exn, SK_RSA, 1);
    if (!k2 || rb_cmp(k2->d, k->d)) { puts("FALHA export/import RSA"); fails++; }
    for (u32 bits = 256; bits <= 521; bits = bits == 256 ? 384 : bits == 384 ? 521 : 999) {
        sk_key *ek = sk_ec_generate(bits);
        sk_alg al = alg_of("algid:ECDSASignatureMessageX962SHA256");
        u8 *sig; usize sn;
        if (!ek || sk_sign(ek, &al, (const u8 *)"x", 1, &sig, &sn) || sk_verify(ek, &al, (const u8 *)"x", 1, sig, sn)) {
            printf("FALHA ECDSA %u\n", bits); fails++; continue;
        }
        sk_key *ek2 = sk_ec_generate(bits);
        sk_alg dh = alg_of("algid:ECDHKeyExchangeStandardX963SHA256");
        u8 *s1, *s2; usize n1, n2;
        if (sk_exchange(ek, &dh, ek2, (const u8 *)"i", 1, 40, &s1, &n1) || sk_exchange(ek2, &dh, ek, (const u8 *)"i", 1, 40, &s2, &n2) ||
            n1 != 40 || memcmp(s1, s2, 40)) { printf("FALHA ECDH %u\n", bits); fails++; }
        sk_export(ek, &ex, &exn);
        sk_key *ek3 = sk_import(ex, exn, SK_EC, 1);
        if (!ek3 || memcmp(ek3->k, ek->k, ek->B)) { printf("FALHA export/import EC %u\n", bits); fails++; }
    }
    printf("seckey self: %s\n", fails ? "FALHOU" : "ok");
    return fails;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    const char *c = argv[1];
    if (!strcmp(c, "self")) return selftest() ? 1 : 0;
    if (!strcmp(c, "gen") && argc == 5) {
        u32 bits = (u32)atoi(argv[3]);
        sk_key *k = !strcmp(argv[2], "rsa") ? sk_rsa_generate(bits) : sk_ec_generate(bits);
        if (!k) return 5;
        u8 *b; usize n;
        if (sk_export(k, &b, &n)) return 5;
        spit(argv[4], b, n);
        return 0;
    }
    if ((!strcmp(c, "pub") || !strcmp(c, "spki") || !strcmp(c, "pkcs8") || !strcmp(c, "import")) && argc == 4) {
        sk_key *k = load(argv[2]);
        u8 *b; usize n;
        if (!strcmp(c, "spki")) { if (sk_export_spki(k, &b, &n)) return 5; }
        else if (!strcmp(c, "pkcs8")) { if (sk_export_pkcs8(k, &b, &n)) return 5; }
        else {
            sk_key *p = !strcmp(c, "pub") ? sk_public_of(k) : k;
            if (sk_export(p, &b, &n)) return 5;
        }
        spit(argv[3], b, n);
        return 0;
    }
    if ((!strcmp(c, "sign") || !strcmp(c, "verify") || !strcmp(c, "enc") || !strcmp(c, "dec")) && argc == 6) {
        sk_alg a = alg_of(argv[2]);
        sk_key *k = load(argv[3]);
        usize mn, sn;
        u8 *m = slurp(argv[4], &mn), *o;
        usize on;
        if (!strcmp(c, "verify")) {
            u8 *s = slurp(argv[5], &sn);
            return sk_verify(k, &a, m, mn, s, sn) ? 1 : 0;
        }
        int rc = !strcmp(c, "sign") ? sk_sign(k, &a, m, mn, &o, &on)
               : !strcmp(c, "enc") ? sk_encrypt(k, &a, m, mn, &o, &on) : sk_decrypt(k, &a, m, mn, &o, &on);
        if (rc) return 6;
        spit(argv[5], o, on);
        return 0;
    }
    if (!strcmp(c, "ecdh") && argc >= 6) {
        sk_alg a = alg_of(argv[2]);
        sk_key *k = load(argv[3]), *p = load(argv[4]);
        usize want = argc > 6 ? (usize)atoi(argv[6]) : 0;
        const char *info = argc > 7 ? argv[7] : "";
        u8 *o; usize on;
        if (sk_exchange(k, &a, p, (const u8 *)info, strlen(info), want, &o, &on)) return 6;
        spit(argv[5], o, on);
        return 0;
    }
    return 1;
}
