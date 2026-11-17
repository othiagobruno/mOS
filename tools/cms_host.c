/* mOS — teste de host do núcleo CMS (userland/libsystem/cms_core.inc); tools/cms-hosttest.py o dirige e confere com o
 * openssl e a biblioteca cryptography. Subcomandos:
 *   parse ARQ.der                       -> campos em hex, um por linha
 *   attrs CTYPE_HEX DIGEST_HEX UTC      -> o SET de atributos assinados em hex
 *   build CONT_HEX DETACHED SET_HEX SIG_HEX CERT.der ISSUER_HEX SERIAL_HEX rsa|ec SAIDA.der
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char u8;
typedef unsigned int u32;
#include "cms_core.inc"
static void hx(const char *k, cms_span s) { printf("%s ", k); for (unsigned long i = 0; i < s.n; i++) printf("%02x", s.p[i]); printf("\n"); }
static u8 *unhex(const char *h, unsigned long *n) {
    unsigned long l = strlen(h) / 2;
    u8 *b = malloc(l ? l : 1);
    for (unsigned long i = 0; i < l; i++) sscanf(h + 2 * i, "%2hhx", &b[i]);
    *n = l;
    return b;
}
static u8 *rd(const char *p, unsigned long *n) {
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); *n = (unsigned long)ftell(f); fseek(f, 0, SEEK_SET);
    u8 *b = malloc(*n ? *n : 1);
    *n = fread(b, 1, *n, f);
    fclose(f);
    return b;
}
int main(int argc, char **argv) {
    if (argc > 2 && !strcmp(argv[1], "parse")) {
        unsigned long n;
        u8 *b = rd(argv[2], &n);
        cms_msg m;
        int r = b ? cms_parse(b, n, &m) : -1;
        printf("rc %d\n", r);
        if (r) return 0;
        printf("encrypted %d\nhas_content %d\nncerts %d\nnsigners %d\n", m.encrypted, m.has_content, m.ncerts, m.nsigners);
        hx("content", m.content);
        hx("econtent_type", m.econtent_type);
        for (int i = 0; i < m.ncerts; i++) hx("cert", m.certs[i]);
        for (int i = 0; i < m.nsigners; i++) {
            cms_signer *s = &m.signers[i];
            u8 *set = 0;
            unsigned long sn = 0;
            if (s->signed_attrs.n) cms_attrs_to_set(s->signed_attrs, &set, &sn);
            cms_span ss = {set, sn};
            hx("digest_oid", s->digest_oid); hx("sig_oid", s->sig_oid); hx("msg_digest", s->msg_digest); hx("signed_set", ss);
            hx("sig", s->sig); hx("issuer", s->issuer); hx("serial", s->serial); hx("time", s->signing_time);
            printf("digest_len %d\n", cms_digest_len(s->digest_oid));
            if (s->signing_time.n) printf("abs_time %.0f\n", cms_der_time(s->signing_time, s->time_tag));
            free(set);
        }
        return 0;
    }
    if (argc > 4 && !strcmp(argv[1], "attrs")) {
        unsigned long cn, dn;
        u8 *ct = unhex(argv[2], &cn), *dg = unhex(argv[3], &dn);
        cms_buf o = {0};
        if (cms_signed_attrs(&o, ct, cn, argv[4], dg, dn)) return 1;
        cms_span s = {o.b, o.n};
        hx("set", s);
        return 0;
    }
    if (argc > 10 && !strcmp(argv[1], "build")) {
        unsigned long cn, sn, gn, in, rn, kn;
        u8 *c = unhex(argv[2], &cn), *set = unhex(argv[4], &sn), *sig = unhex(argv[5], &gn), *iss = unhex(argv[7], &in), *ser = unhex(argv[8], &rn);
        u8 *cert = rd(argv[6], &kn);
        cms_span sa = {set, sn}, cs = {cert, kn}, is = {iss, in}, rs = {ser, rn};
        int ec = !strcmp(argv[9], "ec");
        static const u8 RSA_SHA256[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
        cms_buf o = {0};
        if (cms_build(&o, c, cn, atoi(argv[3]), OID_DATA, sizeof OID_DATA, OID_SHA256, sizeof OID_SHA256,
                      ec ? OID_ECDSA_SHA256 : RSA_SHA256, ec ? sizeof OID_ECDSA_SHA256 : sizeof RSA_SHA256, !ec, &sa, sig, gn, &cs, 1, is, rs))
            return 1;
        FILE *f = fopen(argv[10], "wb");
        fwrite(o.b, 1, o.n, f);
        fclose(f);
        return 0;
    }
    return 2;
}
