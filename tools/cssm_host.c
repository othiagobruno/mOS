/* Teste no host do userland/libsystem/cssm.c (rodar por tools/cssm-hosttest.py): CL sobre um certificado real e o CSP
 * simétrico/PBKDF2 sobre o CommonCrypto do libSystem. As partes RSA/EC usam o SecKey (testado em tools/seckeytest). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void *id;
typedef struct { size_t Length; uint8_t *Data; } cdata_t;
typedef struct {
    uint32_t HeaderVersion; uint8_t CspId[16]; uint32_t BlobType, Format, AlgorithmId, KeyClass, LogicalKeySizeInBits, KeyAttr, KeyUsage;
    uint8_t StartDate[8], EndDate[8]; uint32_t WrapAlgorithmId, WrapMode, Reserved;
} keyhdr_t;
typedef struct { keyhdr_t h; cdata_t KeyData; } ckey_t;
typedef struct { cdata_t Passphrase; uint32_t PseudoRandomFunction; } pbkdf2_params_t;

/* ---- camada mínima de CF/SecKey que o cssm.c usa */
typedef struct { int tag; uint8_t *b; long n; int ccalg; } obj_t;
const void *kSecAttrKeyType = "type", *kSecAttrKeyTypeRSA = "42", *kSecAttrKeyTypeAES = "aes", *kSecAttrKeyTypeDES = "des",
           *kSecAttrKeyType3DES = "3des", *kSecAttrKeyTypeRC2 = "rc2", *kSecAttrKeyTypeRC4 = "rc4", *kSecAttrKeyTypeCAST = "cast",
           *kSecAttrKeyClass = "c", *kSecAttrKeyClassPrivate = "p", *kSecAttrKeyClassPublic = "u", *kSecAttrKeyClassSymmetric = "s",
           *kSecAttrKeySizeInBits = "b", *kSecAttrKeyTypeECSECPrimeRandom = "ec";
#define A(n) const void *n = #n;
A(kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw) A(kSecKeyAlgorithmRSASignatureRaw) A(kSecKeyAlgorithmECDSASignatureDigestX962)
A(kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1) A(kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224) A(kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256)
A(kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384) A(kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512) A(kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1)
A(kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA224) A(kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256) A(kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384)
A(kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512) A(kSecKeyAlgorithmECDSASignatureMessageX962SHA1) A(kSecKeyAlgorithmECDSASignatureMessageX962SHA224)
A(kSecKeyAlgorithmECDSASignatureMessageX962SHA256) A(kSecKeyAlgorithmECDSASignatureMessageX962SHA384) A(kSecKeyAlgorithmECDSASignatureMessageX962SHA512)
A(kSecKeyAlgorithmRSAEncryptionPKCS1) A(kSecKeyAlgorithmRSAEncryptionOAEPSHA1) A(kSecKeyAlgorithmRSAEncryptionRaw) A(kSecKeyAlgorithmECDHKeyExchangeStandard)
static obj_t *mk(int tag, const uint8_t *b, long n) { obj_t *o = calloc(1, sizeof *o); o->tag = tag; o->b = malloc(n ? n : 1); memcpy(o->b, b, n); o->n = n; return o; }
id CFDataCreate(void *a, const uint8_t *b, long n) { (void)a; return mk(1, b, n); }
const uint8_t *CFDataGetBytePtr(id d) { return ((obj_t *)d)->b; }
long CFDataGetLength(id d) { return ((obj_t *)d)->n; }
void CFRelease(id o) { (void)o; }
id objc_retain(id o) { return o; }
void objc_release(id o) { (void)o; }
uint64_t CFGetTypeID(id o) { return ((obj_t *)o)->tag == 2 ? 77 : 1; }
uint64_t SecKeyGetTypeID(void) { return 77; }
uint64_t SecCertificateGetTypeID(void) { return 1; }
id cx_symkey_create(const void *type, const uint8_t *b, size_t n) {
    obj_t *o = mk(2, b, (long)n);
    o->ccalg = type == kSecAttrKeyTypeAES ? 0 : type == kSecAttrKeyTypeDES ? 1 : type == kSecAttrKeyType3DES ? 2 : type == kSecAttrKeyTypeRC4 ? 4 : 5;
    return o;
}
int cx_symkey_raw(id key, const uint8_t **b, size_t *n, int *cc) {
    obj_t *o = key;
    if (!o || o->tag != 2) return -1;
    *b = o->b; *n = (size_t)o->n; *cc = o->ccalg;
    return 0;
}
id SecKeyCopyAttributes(id k) { (void)k; return 0; }
id SecKeyCopyExternalRepresentation(id k, id *e) { (void)k; if (e) *e = 0; return 0; }
id SecKeyCreateSignature(id k, id a, id d, id *e) { (void)k; (void)a; (void)d; if (e) *e = 0; return 0; }
unsigned char SecKeyVerifySignature(id k, id a, id d, id s, id *e) { (void)k; (void)a; (void)d; (void)s; if (e) *e = 0; return 0; }
id SecKeyCreateEncryptedData(id k, id a, id d, id *e) { (void)k; (void)a; (void)d; if (e) *e = 0; return 0; }
id SecKeyCreateDecryptedData(id k, id a, id d, id *e) { (void)k; (void)a; (void)d; if (e) *e = 0; return 0; }
id SecKeyCopyKeyExchangeResult(id k, id a, id p, id q, id *e) { (void)k; (void)a; (void)p; (void)q; if (e) *e = 0; return 0; }
id SecKeyCreateWithData(id d, id a, id *e) { (void)d; (void)a; if (e) *e = 0; return 0; }
int SecKeyRawSign(id k, uint32_t p, const uint8_t *d, size_t dl, uint8_t *s, size_t *sl) { (void)k; (void)p; (void)d; (void)dl; (void)s; (void)sl; return -1; }
int SecKeyRawVerify(id k, uint32_t p, const uint8_t *d, size_t dl, const uint8_t *s, size_t sl) { (void)k; (void)p; (void)d; (void)dl; (void)s; (void)sl; return -1; }
size_t SecKeyGetBlockSize(id k) { (void)k; return 0; }
id cx_seckey_from_spki(const uint8_t *d, uint32_t n) { (void)d; (void)n; return 0; }
id cx_seckey_import(const uint8_t *d, uint32_t n, int p) { (void)d; (void)n; (void)p; return 0; }
id SecCertificateCopyNormalizedIssuerSequence(id c) { (void)c; return 0; }
id SecCertificateCopyNormalizedSubjectSequence(id c) { (void)c; return 0; }
id objc_msgSend(id s, void *op, ...) { (void)s; (void)op; return 0; }
void *sel_registerName(const char *n) { return (void *)n; }
void *cx_class_named(const char *n) { (void)n; return 0; }
void backtrace_from_fp(void) {}
uint64_t mach_absolute_time(void) { return 0; }

int CSSM_CL_CertCache(intptr_t, const cdata_t *, intptr_t *);
int CSSM_CL_CertAbortCache(intptr_t, intptr_t);
int CSSM_CL_CertGetFirstCachedFieldValue(intptr_t, intptr_t, const cdata_t *, intptr_t *, uint32_t *, cdata_t **);
int CSSM_CL_CertGetNextCachedFieldValue(intptr_t, intptr_t, cdata_t **);
int CSSM_CL_CertGetFirstFieldValue(intptr_t, const cdata_t *, const cdata_t *, intptr_t *, uint32_t *, cdata_t **);
int CSSM_CL_CertAbortQuery(intptr_t, intptr_t);
int CSSM_CL_FreeFieldValue(intptr_t, const cdata_t *, cdata_t *);
int SecCertificateGetCLHandle(id, intptr_t *);
int SecKeyGetCSPHandle(id, intptr_t *);
int SecKeyGetCSSMKey(id, const ckey_t **);
int CSSM_CSP_CreateSymmetricContext(intptr_t, uint32_t, uint32_t, const void *, const ckey_t *, const cdata_t *, uint32_t, void *, uint64_t *);
int CSSM_CSP_CreateDeriveKeyContext(intptr_t, uint32_t, uint32_t, uint32_t, const void *, const ckey_t *, uint32_t, const cdata_t *, const void *, uint64_t *);
int CSSM_EncryptData(uint64_t, const cdata_t *, uint32_t, cdata_t *, uint32_t, size_t *, cdata_t *);
int CSSM_DecryptData(uint64_t, const cdata_t *, uint32_t, cdata_t *, uint32_t, size_t *, cdata_t *);
int CSSM_DecryptDataInit(uint64_t);
int CSSM_DecryptDataUpdate(uint64_t, const cdata_t *, uint32_t, cdata_t *, uint32_t, size_t *);
int CSSM_DecryptDataFinal(uint64_t, cdata_t *);
int CSSM_DeriveKey(uint64_t, cdata_t *, uint32_t, uint32_t, const cdata_t *, const void *, ckey_t *);
int CSSM_WrapKey(uint64_t, const void *, const ckey_t *, const cdata_t *, ckey_t *);
int CSSM_DeleteContext(uint64_t);
int CSSM_FreeKey(intptr_t, const void *, ckey_t *, int);

static void hex(const char *tag, const uint8_t *b, size_t n) { printf("%s ", tag); for (size_t i = 0; i < n; i++) printf("%02x", b[i]); printf("\n"); }
static size_t unhex(const char *h, uint8_t *o) { size_t n = 0; for (; h[0] && h[1]; h += 2) { unsigned v; sscanf(h, "%2x", &v); o[n++] = (uint8_t)v; } return n; }
static const uint8_t R08C[] = {0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x4d, 0x02, 0x01, 0x01, 0x01}, R08S[] = {0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x4d, 0x02, 0x01, 0x03, 0x02};
static cdata_t foid(const uint8_t *pre, int last) { static uint8_t b[8][16]; static int k; uint8_t *p = b[k++ & 7]; memcpy(p, pre, 11); p[11] = (uint8_t)last; return (cdata_t){12, p}; }

int main(int argc, char **argv) {
    if (argc < 5) return 2;
    /* argv: cert.der  aeskey(hex)  iv(hex)  plaintext(hex) */
    FILE *f = fopen(argv[1], "rb");
    static uint8_t der[8192];
    size_t dn = fread(der, 1, sizeof der, f);
    fclose(f);
    obj_t *cert = mk(1, der, (long)dn);
    intptr_t cl, h, res;
    uint32_t nm;
    cdata_t *v;
    printf("clh %d\n", SecCertificateGetCLHandle(cert, &cl));
    cdata_t cd = {dn, der};
    printf("cache %d\n", CSSM_CL_CertCache(cl, &cd, &h));
    cdata_t o = foid(R08S, 1);
    printf("sigalg %d\n", CSSM_CL_CertGetFirstCachedFieldValue(cl, h, &o, &res, &nm, &v));
    cdata_t *ai = (cdata_t *)v->Data;
    hex("sigoid", ai[0].Data, ai[0].Length);
    hex("sigparams", ai[1].Data, ai[1].Length);
    printf("next %d\n", CSSM_CL_CertGetNextCachedFieldValue(cl, res, &v) != 0);
    CSSM_CL_CertAbortQuery(cl, res);
    o = foid(R08C, 3);
    CSSM_CL_CertGetFirstCachedFieldValue(cl, h, &o, &res, &nm, &v);
    hex("serial", v->Data, v->Length);
    CSSM_CL_FreeFieldValue(cl, &o, v);
    o = foid(R08C, 2);
    CSSM_CL_CertGetFirstCachedFieldValue(cl, h, &o, &res, &nm, &v);
    hex("version", v->Data, v->Length);
    o = foid(R08C, 6);
    CSSM_CL_CertGetFirstCachedFieldValue(cl, h, &o, &res, &nm, &v);
    printf("nbtag %02x\n", v->Data[0]);
    cdata_t *tm = (cdata_t *)(v->Data + 8);
    printf("nb %.*s\n", (int)tm->Length, tm->Data);
    o = foid(R08C, 22);
    CSSM_CL_CertGetFirstCachedFieldValue(cl, h, &o, &res, &nm, &v);
    hex("subject", v->Data, v->Length);
    o = foid(R08C, 9);
    CSSM_CL_CertGetFirstCachedFieldValue(cl, h, &o, &res, &nm, &v);
    hex("spkialg", ((cdata_t *)v->Data)[0].Data, ((cdata_t *)v->Data)[0].Length);
    o = foid(R08S, 2);
    CSSM_CL_CertGetFirstCachedFieldValue(cl, h, &o, &res, &nm, &v);
    hex("sig", v->Data, v->Length);
    uint8_t ku[] = {0x55, 0x1d, 0x0f}, eku[] = {0x55, 0x1d, 0x25}, bc[] = {0x55, 0x1d, 0x13}, ski[] = {0x55, 0x1d, 0x0e}, san[] = {0x55, 0x1d, 0x11}, crl[] = {0x55, 0x1d, 0x1f};
    o = (cdata_t){3, ku};
    CSSM_CL_CertGetFirstCachedFieldValue(cl, h, &o, &res, &nm, &v);
    uint8_t *x = v->Data;
    printf("ku crit=%d fmt=%u val=%04x\n", *(int *)(x + 16), *(uint32_t *)(x + 20), **(uint16_t **)(x + 24));
    o = (cdata_t){3, eku};
    CSSM_CL_CertGetFirstCachedFieldValue(cl, h, &o, &res, &nm, &v);
    x = v->Data;
    uint8_t *e = *(uint8_t **)(x + 24);
    uint32_t np = *(uint32_t *)e;
    cdata_t *ps = *(cdata_t **)(e + 8);
    printf("eku n=%u\n", np);
    for (uint32_t i = 0; i < np; i++) hex("ekuoid", ps[i].Data, ps[i].Length);
    o = (cdata_t){3, bc};
    CSSM_CL_CertGetFirstCachedFieldValue(cl, h, &o, &res, &nm, &v);
    uint32_t *b = *(uint32_t **)(v->Data + 24);
    printf("bc ca=%u pl=%u/%u crit=%d\n", b[0], b[1], b[2], *(int *)(v->Data + 16));
    o = (cdata_t){3, ski};
    CSSM_CL_CertGetFirstCachedFieldValue(cl, h, &o, &res, &nm, &v);
    cdata_t *sk = *(cdata_t **)(v->Data + 24);
    hex("ski", sk->Data, sk->Length);
    o = (cdata_t){3, crl};
    CSSM_CL_CertGetFirstCachedFieldValue(cl, h, &o, &res, &nm, &v);
    printf("crl fmt=%u tag=%02x\n", *(uint32_t *)(v->Data + 20), **(uint8_t **)(v->Data + 24));
    hex("crlber", ((cdata_t *)(v->Data + 32))->Data, ((cdata_t *)(v->Data + 32))->Length);
    o = (cdata_t){3, san};
    int rs = CSSM_CL_CertGetFirstCachedFieldValue(cl, h, &o, &res, &nm, &v) == (int)0x80011907;
    printf("san %d n=%u\n", rs, nm);
    o = foid(R08C, 99);
    printf("tag %d\n", CSSM_CL_CertGetFirstCachedFieldValue(cl, h, &o, &res, &nm, &v) == (int)0x8001184f);
    o = foid(R08S, 10);
    printf("nocache %d\n", CSSM_CL_CertGetFirstFieldValue(cl, &cd, &o, &res, &nm, &v));
    int a1 = CSSM_CL_CertAbortCache(cl, h), a2 = CSSM_CL_CertAbortCache(cl, h) == (int)0x80011902;
    printf("abort %d %d\n", a1, a2);

    /* ---- CSP: AES-CBC-PKCS7 */
    uint8_t kb[32], iv[16], pt[256];
    size_t kn = unhex(argv[2], kb), ivn = unhex(argv[3], iv), pn = unhex(argv[4], pt);
    ckey_t k;
    memset(&k, 0, sizeof k);
    k.h.HeaderVersion = 2; k.h.BlobType = 0; k.h.Format = 12; k.h.AlgorithmId = 0x80000001u; k.h.KeyClass = 2;
    k.h.LogicalKeySizeInBits = (uint32_t)kn * 8; k.KeyData.Data = kb; k.KeyData.Length = kn;
    intptr_t csp;
    SecKeyGetCSPHandle((id)cert, &csp);
    uint64_t cc;
    cdata_t ivd = {ivn, iv};
    printf("symctx %d\n", CSSM_CSP_CreateSymmetricContext(csp, 0x80000001u, 6, 0, &k, &ivd, 7, 0, &cc));
    cdata_t in = {pn, pt}, out = {0, 0}, rem = {0, 0};
    size_t done = 0;
    printf("enc %d\n", CSSM_EncryptData(cc, &in, 1, &out, 1, &done, &rem));
    hex("ct", out.Data, out.Length);
    cdata_t back = {0, 0};
    CSSM_DecryptData(cc, &out, 1, &back, 1, &done, &rem);
    printf("roundtrip %d\n", back.Length == pn && !memcmp(back.Data, pt, pn));
    /* em etapas, com buffer do app pequeno */
    uint8_t buf[512];
    size_t tot = 0, d2 = 0;
    printf("init %d\n", CSSM_DecryptDataInit(cc));
    for (size_t off = 0; off < out.Length; off += 7) {
        cdata_t piece = {out.Length - off < 7 ? out.Length - off : 7, out.Data + off}, ob = {sizeof buf - tot, buf + tot};
        if (CSSM_DecryptDataUpdate(cc, &piece, 1, &ob, 1, &d2)) { printf("upd falhou\n"); break; }
        tot += d2;
    }
    cdata_t fin = {0, 0};
    printf("final %d\n", CSSM_DecryptDataFinal(cc, &fin));
    memcpy(buf + tot, fin.Data, fin.Length);
    tot += fin.Length;
    printf("stream %d\n", tot == pn && !memcmp(buf, pt, pn));
    CSSM_DeleteContext(cc);
    printf("deleted %d\n", CSSM_EncryptData(cc, &in, 1, &out, 1, &done, &rem) == (int)0x80010840);

    /* ---- PBKDF2 */
    uint8_t salt[] = "sal-do-mOS";
    cdata_t sd = {10, salt};
    printf("dctx %d\n", CSSM_CSP_CreateDeriveKeyContext(csp, 103, 0x80000001u, 256, 0, 0, 1000, &sd, 0, &cc));
    pbkdf2_params_t pp = {{5, (uint8_t *)"senha"}, 0};
    cdata_t pd = {sizeof pp, (uint8_t *)&pp};
    ckey_t dk;
    memset(&dk, 0, sizeof dk);
    printf("derive %d\n", CSSM_DeriveKey(cc, &pd, 1, 0, 0, 0, &dk));
    hex("dk", dk.KeyData.Data, dk.KeyData.Length);
    printf("dkhdr %u %u %u\n", dk.h.AlgorithmId == 0x80000001u, dk.h.KeyClass, dk.h.LogicalKeySizeInBits);
    CSSM_DeleteContext(cc);
    /* ---- wrap nulo devolve a chave crua; FreeKey solta */
    CSSM_CSP_CreateSymmetricContext(csp, 0, 0, 0, 0, 0, 0, 0, &cc);
    ckey_t wk;
    memset(&wk, 0, sizeof wk);
    printf("wrap %d\n", CSSM_WrapKey(cc, 0, &k, 0, &wk));
    printf("wrapraw %d\n", wk.KeyData.Length == kn && !memcmp(wk.KeyData.Data, kb, kn));
    int fr = CSSM_FreeKey(csp, 0, &wk, 0);
    printf("free %d %d\n", fr, wk.KeyData.Data == 0);
    CSSM_DeleteContext(cc);
    /* SecKeyGetCSSMKey de uma chave simétrica: referência estável */
    id sym = cx_symkey_create(kSecAttrKeyTypeAES, kb, kn);
    const ckey_t *r1, *r2;
    SecKeyGetCSSMKey(sym, &r1);
    SecKeyGetCSSMKey(sym, &r2);
    printf("ref %d %u %u %u\n", r1 == r2, r1->h.BlobType, r1->h.KeyClass, r1->h.LogicalKeySizeInBits);
    printf("refctx %d\n", CSSM_CSP_CreateSymmetricContext(csp, 0x80000001u, 6, 0, r1, &ivd, 7, 0, &cc));
    cdata_t out2 = {0, 0};
    CSSM_EncryptData(cc, &in, 1, &out2, 1, &done, &rem);
    printf("refenc %d\n", out2.Length == out.Length && !memcmp(out2.Data, out.Data, out.Length));
    return 0;
}
