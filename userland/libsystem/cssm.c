/*
 * mOS libSystem — CDSA/CSSM (Security.framework legado) sobre o SecKey de seckey.c, as chaves simétricas de
 * sectransform.c e o CommonCrypto.
 *
 * CSP: SecKeyGetCSSMKey dá um CSSM_KEY de referência ligado ao SecKey (e o CSP aceita chaves cruas que o app monta:
 * PKCS#1, X.509/SPKI, PKCS#8, octetos EC e octetos simétricos). Contextos de assinatura (RSA PKCS#1 v1.5 com ou sem
 * DigestInfo, *WithRSA, ECDSA e *WithECDSA; assinatura ECDSA em DER, como o CSP da Apple), simétricos (AES, DES,
 * 3DES, RC2, RC4, Blowfish; ECB/CBC/CFB/OFB com ou sem padding PKCS#5/7), assimétricos (RSA PKCS#1, OAEP-SHA1 e
 * bruto) e de derivação (PBKDF2-HMAC-SHA1 e ECDH). Sign/Verify/Encrypt/Decrypt diretos e em etapas, DeriveKey,
 * WrapKey/UnwrapKey (nulo = exporta a chave crua; com contexto = cifra a chave) e FreeKey. Memória de saída com malloc
 * quando o app passa Data = NULL (o app solta com free, como faz com o alocador padrão).
 *
 * CL: CSSM_CL_CertCache e CertGetFirst(Cached)FieldValue para versão, serial, algoritmo de assinatura (e o do TBS),
 * assinatura, validade, nomes (DER e normalizado), algoritmo da chave pública e as extensões (KeyUsage, ExtendedKeyUsage,
 * BasicConstraints, SubjectKeyIdentifier interpretadas; as outras em BER). Cada valor é um bloco só: FreeFieldValue
 * solta tudo.
 */
#include "libc.h"

typedef void *id;
typedef int OSStatus;
typedef int CSSM_RETURN;
typedef unsigned short u16;
id objc_retain(id o);
void objc_release(id o);
void CFRelease(id o);
id CFDataCreate(void *alloc, const u8 *b, long n);
const u8 *CFDataGetBytePtr(id d);
long CFDataGetLength(id d);
u64 CFGetTypeID(id o);
u64 SecKeyGetTypeID(void);
u64 SecCertificateGetTypeID(void);
id SecKeyCopyAttributes(id key);
id SecKeyCopyExternalRepresentation(id key, id *err);
id SecKeyCreateSignature(id key, id alg, id data, id *err);
u8 SecKeyVerifySignature(id key, id alg, id data, id sig, id *err);
id SecKeyCreateEncryptedData(id key, id alg, id data, id *err);
id SecKeyCreateDecryptedData(id key, id alg, id data, id *err);
id SecKeyCopyKeyExchangeResult(id key, id alg, id pub, id params, id *err);
OSStatus SecKeyRawSign(id key, u32 pad, const u8 *d, size_t dl, u8 *sig, size_t *sl);
OSStatus SecKeyRawVerify(id key, u32 pad, const u8 *d, size_t dl, const u8 *sig, size_t sl);
size_t SecKeyGetBlockSize(id key);
id cx_seckey_from_spki(const u8 *der, u32 n);
id cx_seckey_import(const u8 *der, u32 n, int priv);
id cx_symkey_create(const void *type, const u8 *b, size_t n);
int cx_symkey_raw(id key, const u8 **b, size_t *n, int *ccalg);
id SecCertificateCopyNormalizedIssuerSequence(id cert);
id SecCertificateCopyNormalizedSubjectSequence(id cert);
int CCCryptorCreateWithMode(u32 op, u32 mode, u32 alg, u32 padding, const void *iv, const void *key, size_t klen,
                            const void *tweak, size_t tlen, int rounds, u32 options, void **ref);
int CCCryptorUpdate(void *ref, const void *in, size_t n, void *out, size_t avail, size_t *moved);
int CCCryptorFinal(void *ref, void *out, size_t avail, size_t *moved);
size_t CCCryptorGetOutputLength(void *ref, size_t n, u8 final);
int CCCryptorRelease(void *ref);
int CCKeyDerivationPBKDF(u32 alg, const char *pw, size_t pwlen, const u8 *salt, size_t saltlen, u32 prf, u32 rounds, u8 *dk, size_t dklen);
u8 *CC_MD5(const void *d, u32 n, u8 *md);
#define KX(n) extern const void *n;
KX(kSecAttrKeyType) KX(kSecAttrKeyTypeRSA) KX(kSecAttrKeyTypeAES) KX(kSecAttrKeyTypeDES) KX(kSecAttrKeyType3DES)
KX(kSecAttrKeyTypeRC2) KX(kSecAttrKeyTypeRC4) KX(kSecAttrKeyTypeCAST) KX(kSecAttrKeyClass) KX(kSecAttrKeyClassPrivate)
KX(kSecAttrKeyClassPublic) KX(kSecAttrKeyClassSymmetric) KX(kSecAttrKeySizeInBits)
KX(kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw) KX(kSecKeyAlgorithmRSASignatureRaw) KX(kSecKeyAlgorithmECDSASignatureDigestX962)
KX(kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1) KX(kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224)
KX(kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256) KX(kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384)
KX(kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512) KX(kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1)
KX(kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA224) KX(kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256)
KX(kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384) KX(kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512)
KX(kSecKeyAlgorithmECDSASignatureMessageX962SHA1) KX(kSecKeyAlgorithmECDSASignatureMessageX962SHA224)
KX(kSecKeyAlgorithmECDSASignatureMessageX962SHA256) KX(kSecKeyAlgorithmECDSASignatureMessageX962SHA384)
KX(kSecKeyAlgorithmECDSASignatureMessageX962SHA512) KX(kSecKeyAlgorithmRSAEncryptionPKCS1) KX(kSecKeyAlgorithmRSAEncryptionOAEPSHA1)
KX(kSecKeyAlgorithmRSAEncryptionRaw) KX(kSecKeyAlgorithmECDHKeyExchangeStandard) KX(kSecAttrKeyTypeECSECPrimeRandom)
id SecKeyCreateWithData(id data, id attrs, id *err);
void *cx_class_named(const char *name);
#define K_(v) ((id)(v))
id objc_msgSend(id self, void *op, ...);
void *sel_registerName(const char *name);
#define MSG1(T, o, s, a) ((T (*)(id, void *, __typeof__(a)))objc_msgSend)((id)(o), sel_registerName(s), (a))

/* ---- tipos do cssmtype.h (layout de 64 bits) */
typedef struct { size_t Length; u8 *Data; } cdata_t;
typedef struct { u32 Data1; u16 Data2, Data3; u8 Data4[8]; } guid_t;
typedef struct { u8 Year[4], Month[2], Day[2]; } cdate_t;
typedef struct {
    u32 HeaderVersion; guid_t CspId; u32 BlobType, Format, AlgorithmId, KeyClass, LogicalKeySizeInBits, KeyAttr, KeyUsage;
    cdate_t StartDate, EndDate; u32 WrapAlgorithmId, WrapMode, Reserved;
} keyhdr_t;
typedef struct { keyhdr_t h; cdata_t KeyData; } ckey_t;
typedef struct { u32 type, len; union { char *s; u32 u; void *p; ckey_t *key; cdata_t *data; } v; } cattr_t;
typedef struct { cdata_t Param; void *Callback; void *CallerCtx; } crypto_data_t;
typedef struct { cdata_t Passphrase; u32 PseudoRandomFunction; } pbkdf2_params_t;
_Static_assert(sizeof(keyhdr_t) == 76, "CSSM_KEYHEADER");
_Static_assert(__builtin_offsetof(ckey_t, KeyData) == 80, "CSSM_KEY");
_Static_assert(sizeof(cattr_t) == 16, "CSSM_CONTEXT_ATTRIBUTE");

enum { ALG_NONE = 0, ALG_MD2 = 5, ALG_MD4 = 6, ALG_MD5 = 7, ALG_SHA1 = 8, ALG_DES = 14, ALG_3DES = 17, ALG_RC2 = 23, ALG_RC4 = 25,
       ALG_CAST = 27, ALG_BLOWFISH = 28, ALG_RSA = 42, ALG_DSA = 43, ALG_MD5RSA = 44, ALG_MD2RSA = 45, ALG_SHA1RSA = 51, ALG_ECDSA = 73,
       ALG_SHA1ECDSA = 82, ALG_ECDH = 84, ALG_PBKDF2 = 103 };
#define AV 0x80000000u
enum { ALG_AES = AV + 1, ALG_SHA256 = AV + 14, ALG_SHA384 = AV + 15, ALG_SHA512 = AV + 16, ALG_SHA224 = AV + 18,
       ALG_SHA224RSA = AV + 19, ALG_SHA256RSA = AV + 20, ALG_SHA384RSA = AV + 21, ALG_SHA512RSA = AV + 22,
       ALG_SHA224ECDSA = AV + 24, ALG_SHA256ECDSA = AV + 25, ALG_SHA384ECDSA = AV + 26, ALG_SHA512ECDSA = AV + 27,
       ALG_ECDSA_SPECIFIED = AV + 28 };
enum { PAD_NONE = 0, PAD_PKCS5 = 6, PAD_PKCS7 = 7, PAD_PKCS1 = 10, PAD_SIGRAW = 11 };
enum { MODE_NONE = 0, MODE_ECB = 2, MODE_ECBPad = 3, MODE_CBC = 4, MODE_CBC_IV8 = 5, MODE_CBCPadIV8 = 6, MODE_CFB = 7, MODE_CFB_IV8 = 8,
       MODE_CFBPadIV8 = 9, MODE_OFB = 10, MODE_OFB_IV8 = 11, MODE_OFBPadIV8 = 12, MODE_OAEP = 39, MODE_V15 = 38 };
enum { BLOB_RAW = 0, BLOB_REF = 2, BLOB_WRAPPED = 3 };
enum { FMT_NONE = 0, FMT_PKCS1 = 1, FMT_PKCS8 = 10, FMT_OCTET = 12, FMT_X509 = 0x80000000u, FMT_WRAPPED_NONE = 0 };
enum { KC_PUBLIC = 0, KC_PRIVATE = 1, KC_SESSION = 2 };
enum { CC_SIG = 2, CC_SYM = 3, CC_ASYM = 8, CC_DERIVE = 10 };
enum { AT_KEY = 0x40000003, AT_IV = 0x20000004, AT_SALT = 0x20000005, AT_PADDING = 0x10000006, AT_PASSPHRASE = 0x30000009,
       AT_KEY_LENGTH = 0x1000000a, AT_MODE = 0x10000013, AT_ALG_ID = 0x1000001d, AT_ITER = 0x1000001e, AT_KEY_TYPE = 0x10000012,
       AT_SEED = 0x30000008, AT_EFFECTIVE_BITS = 0x10000014, AT_ROUNDS = 0x1000000e };
/* erros: base do CSSM 0x80010000, CSP +0x800, CL +0x1800 */
#define E_CSSM(x) ((CSSM_RETURN)(0x80010000u + (x)))
#define E_CSP(x) ((CSSM_RETURN)(0x80010800u + (x)))
#define E_CL(x) ((CSSM_RETURN)(0x80011800u + (x)))
enum { CSSM_OK = 0 };
#define ERR_POINTER E_CSSM(0x004)
#define ERR_ADDIN_HANDLE E_CSSM(0x101)
#define ERR_INTERNAL E_CSP(0x001)
#define ERR_MEMORY E_CSP(0x002)
#define ERR_NOT_IMPL E_CSP(0x007)
#define ERR_CONTEXT E_CSP(0x040)
#define ERR_DATA E_CSP(0x046)
#define ERR_INPUT_LEN E_CSP(0x101)
#define ERR_OUTPUT_LEN E_CSP(0x102)
#define ERR_KEY E_CSP(0x110)
#define ERR_KEY_REF E_CSP(0x111)
#define ERR_KEY_SIZE E_CSP(0x118)
#define ERR_ALG E_CSP(0x131)
#define ERR_ATTR_KEY E_CSP(0x136)
#define ERR_ATTR_IV E_CSP(0x138)
#define ERR_ATTR_SALT E_CSP(0x13a)
#define ERR_ATTR_PADDING E_CSP(0x13c)
#define ERR_ATTR_PASSPHRASE E_CSP(0x142)
#define ERR_SIGNATURE E_CSP(0x14b)
#define ERR_VERIFY E_CSP(0x14a)
#define ERR_ATTR_MODE E_CSP(0x16e)
#define ERR_CL_CERT E_CL(0x043)
#define ERR_CL_FIELD E_CL(0x045)
#define ERR_CL_TAG E_CL(0x04f)
#define ERR_CL_CACHE E_CL(0x102)
#define ERR_CL_RESULTS E_CL(0x103)
#define ERR_CL_NO_VALUES E_CL(0x107)

#define CSP_HANDLE ((intptr_t)0x4353500001)                  /* um CSP e um CL só; o valor só precisa ser estável */
#define CL_HANDLE ((intptr_t)0x434c00001)

CSSM_RETURN CSSM_Init(const void *ver, u32 scope, const void *guid, u32 hier, u32 *pvc, const void *reserved) {
    (void)ver; (void)scope; (void)guid; (void)hier; (void)reserved;
    if (pvc) *pvc = 0;
    return CSSM_OK;
}
CSSM_RETURN CSSM_Terminate(void) { return CSSM_OK; }

/* ================================================================ chaves */
typedef struct kreg { struct kreg *next; id key; ckey_t ck; } kreg_t;      /* CSSM_KEY de referência de cada SecKey */
static kreg_t *g_keys;
static id key_attrs_type(id key, int *rsa, int *priv, int *bits) {
    id a = SecKeyCopyAttributes(key);
    *rsa = 0; *priv = 0; *bits = 0;
    if (!a) return 0;
    id t = MSG1(id, a, "objectForKey:", K_(kSecAttrKeyType));
    id c = MSG1(id, a, "objectForKey:", K_(kSecAttrKeyClass));
    id b = MSG1(id, a, "objectForKey:", K_(kSecAttrKeySizeInBits));
    *rsa = t && MSG1(u8, t, "isEqual:", K_(kSecAttrKeyTypeRSA));
    *priv = c && MSG1(u8, c, "isEqual:", K_(kSecAttrKeyClassPrivate)) ? 1 : c && MSG1(u8, c, "isEqual:", K_(kSecAttrKeyClassSymmetric)) ? 2 : 0;
    *bits = b ? ((int (*)(id, void *))objc_msgSend)(b, sel_registerName("intValue")) : 0;
    return a;
}
static const struct { const void *const *type; u32 alg; int ccalg; } g_symalg[] = {
    {&kSecAttrKeyTypeAES, ALG_AES, 0}, {&kSecAttrKeyTypeDES, ALG_DES, 1}, {&kSecAttrKeyType3DES, ALG_3DES, 2},
    {&kSecAttrKeyTypeCAST, ALG_CAST, 3}, {&kSecAttrKeyTypeRC4, ALG_RC4, 4}, {&kSecAttrKeyTypeRC2, ALG_RC2, 5},
};
static void fill_hdr(keyhdr_t *h, id key) {
    memset(h, 0, sizeof *h);
    h->HeaderVersion = 2;
    int rsa, priv, bits;
    id a = key_attrs_type(key, &rsa, &priv, &bits);
    const u8 *kb; size_t kn; int cc;
    if (!cx_symkey_raw(key, &kb, &kn, &cc)) {
        h->KeyClass = KC_SESSION;
        for (int i = 0; i < 6; i++) if (g_symalg[i].ccalg == cc) h->AlgorithmId = g_symalg[i].alg;
        h->LogicalKeySizeInBits = (u32)kn * 8;
        h->KeyUsage = 0x1 | 0x2 | 0x40 | 0x80;                                  /* ENCRYPT DECRYPT WRAP UNWRAP */
    } else {
        h->AlgorithmId = rsa ? ALG_RSA : ALG_ECDSA;
        h->KeyClass = priv == 1 ? KC_PRIVATE : KC_PUBLIC;
        h->LogicalKeySizeInBits = (u32)bits;
        h->KeyUsage = priv == 1 ? (0x4 | 0x2 | 0x80 | 0x100) : (0x8 | 0x1 | 0x40);
    }
    h->KeyAttr = 0x20;                                                           /* EXTRACTABLE */
    if (a) objc_release(a);
}
OSStatus SecKeyGetCSSMKey(id key, const ckey_t **out) {
    if (!key || !out || CFGetTypeID(key) != SecKeyGetTypeID()) return -50;
    for (kreg_t *r = g_keys; r; r = r->next) if (r->key == key) { *out = &r->ck; return 0; }
    kreg_t *r = calloc(1, sizeof *r);
    if (!r) return -108;
    r->key = objc_retain(key);
    fill_hdr(&r->ck.h, key);
    r->ck.h.BlobType = BLOB_REF;
    r->ck.h.Format = 0;                                                          /* CSSM_KEYBLOB_REF_FORMAT_INTEGER */
    r->ck.KeyData.Length = sizeof(id);
    r->ck.KeyData.Data = (u8 *)&r->key;
    r->next = g_keys; g_keys = r;
    *out = &r->ck;
    return 0;
}
OSStatus SecKeyGetCSPHandle(id key, intptr_t *csp) { if (!key || !csp) return -50; *csp = CSP_HANDLE; return 0; }
OSStatus SecKeychainGetCSPHandle(id kc, intptr_t *csp) { (void)kc; if (!csp) return -50; *csp = CSP_HANDLE; return 0; }
OSStatus SecCertificateGetCLHandle(id cert, intptr_t *cl) { if (!cert || !cl) return -50; *cl = CL_HANDLE; return 0; }
static u8 g_creds[256];                                                         /* CSSM_ACCESS_CREDENTIALS vazio: não há ACL no mOS */
OSStatus SecKeyGetCredentials(id key, u32 op, u32 type, const void **creds) {
    (void)op; (void)type;
    if (!key || !creds) return -50;
    *creds = g_creds;
    return 0;
}
/* SecKey (+1) de um CSSM_KEY: de referência (registro) ou cru */
static id key_of(const ckey_t *k) {
    if (!k) return 0;
    for (kreg_t *r = g_keys; r; r = r->next) if (&r->ck == k || (k->h.BlobType == BLOB_REF && k->KeyData.Data == (u8 *)&r->key)) return objc_retain(r->key);
    if (k->h.BlobType != BLOB_RAW || !k->KeyData.Data || !k->KeyData.Length) return 0;
    const u8 *b = k->KeyData.Data;
    u32 n = (u32)k->KeyData.Length;
    if (k->h.KeyClass == KC_SESSION) {
        for (int i = 0; i < 6; i++) if (g_symalg[i].alg == k->h.AlgorithmId) return cx_symkey_create(*g_symalg[i].type, b, n);
        if (k->h.AlgorithmId == ALG_3DES) return cx_symkey_create(kSecAttrKeyType3DES, b, n);
        return 0;
    }
    if (k->h.Format == FMT_X509) return cx_seckey_from_spki(b, n);
    return cx_seckey_import(b, n, k->h.KeyClass == KC_PRIVATE);
}
CSSM_RETURN CSSM_FreeKey(intptr_t csp, const void *creds, ckey_t *key, int del) {
    (void)creds; (void)del;
    if (csp != CSP_HANDLE) return ERR_ADDIN_HANDLE;
    if (!key) return ERR_POINTER;
    for (kreg_t *r = g_keys; r; r = r->next) if (&r->ck == key) return CSSM_OK;  /* pertence ao SecKey */
    if (key->h.BlobType != BLOB_REF && key->KeyData.Data) { memset(key->KeyData.Data, 0, key->KeyData.Length); free(key->KeyData.Data); }
    key->KeyData.Data = 0; key->KeyData.Length = 0;
    return CSSM_OK;
}

/* ================================================================ contextos */
typedef struct ctx {
    struct ctx *next;
    u32 type, alg, padding, mode, derive_type, derive_bits, iter;
    id key;                                                                      /* SecKey (+1) */
    u8 iv[16]; int has_iv;
    u8 *salt; size_t saltn;
    u8 *pass; size_t passn;
    void *cryptor; int crypt_dir;                                                /* Encrypt/DecryptDataInit */
    u8 *acc; size_t accn, acccap;                                                /* Sign/VerifyDataInit e RSA em etapas */
} ctx_t;
static ctx_t *g_ctx;
static ctx_t *ctx_of(u64 h) { for (ctx_t *c = g_ctx; c; c = c->next) if ((u64)(uintptr_t)c == h) return c; return 0; }
static ctx_t *ctx_new(u32 type, u32 alg) {
    ctx_t *c = calloc(1, sizeof *c);
    if (!c) return 0;
    c->type = type; c->alg = alg;
    c->next = g_ctx; g_ctx = c;
    return c;
}
static void acc_add(ctx_t *c, const u8 *b, size_t n) {
    if (c->accn + n > c->acccap) {
        size_t cap = c->acccap ? c->acccap * 2 : 256;
        while (cap < c->accn + n) cap *= 2;
        u8 *p = realloc(c->acc, cap);
        if (!p) return;
        c->acc = p; c->acccap = cap;
    }
    memcpy(c->acc + c->accn, b, n);
    c->accn += n;
}
static void acc_reset(ctx_t *c) { if (c->acc) memset(c->acc, 0, c->acccap); free(c->acc); c->acc = 0; c->accn = c->acccap = 0; }
static void set_iv(ctx_t *c, const cdata_t *iv) {
    c->has_iv = iv && iv->Data && iv->Length;
    memset(c->iv, 0, 16);
    if (c->has_iv) memcpy(c->iv, iv->Data, iv->Length < 16 ? iv->Length : 16);
}
CSSM_RETURN CSSM_DeleteContext(u64 h) {
    ctx_t *c = ctx_of(h);
    if (!c) return ERR_CONTEXT;
    for (ctx_t **pp = &g_ctx; *pp; pp = &(*pp)->next) if (*pp == c) { *pp = c->next; break; }
    if (c->key) objc_release(c->key);
    if (c->cryptor) CCCryptorRelease(c->cryptor);
    acc_reset(c);
    if (c->salt) free(c->salt);
    if (c->pass) { memset(c->pass, 0, c->passn); free(c->pass); }
    memset(c, 0, sizeof *c);
    free(c);
    return CSSM_OK;
}
static CSSM_RETURN ctx_key(ctx_t *c, const ckey_t *k) {
    if (c->key) { objc_release(c->key); c->key = 0; }
    if (!k) return CSSM_OK;
    c->key = key_of(k);
    return c->key ? CSSM_OK : ERR_KEY;
}
CSSM_RETURN CSSM_CSP_CreateSignatureContext(intptr_t csp, u32 alg, const void *creds, const ckey_t *key, u64 *out) {
    (void)creds;
    if (csp != CSP_HANDLE) return ERR_ADDIN_HANDLE;
    if (!out || !key) return ERR_POINTER;
    ctx_t *c = ctx_new(CC_SIG, alg);
    if (!c) return ERR_MEMORY;
    c->padding = PAD_PKCS1;
    CSSM_RETURN r = ctx_key(c, key);
    if (r) { CSSM_DeleteContext((u64)(uintptr_t)c); return r; }
    *out = (u64)(uintptr_t)c;
    return CSSM_OK;
}
CSSM_RETURN CSSM_CSP_CreateSymmetricContext(intptr_t csp, u32 alg, u32 mode, const void *creds, const ckey_t *key, const cdata_t *iv,
                                            u32 padding, void *reserved, u64 *out) {
    (void)creds; (void)reserved;
    if (csp != CSP_HANDLE) return ERR_ADDIN_HANDLE;
    if (!out) return ERR_POINTER;
    ctx_t *c = ctx_new(CC_SYM, alg);
    if (!c) return ERR_MEMORY;
    c->mode = mode; c->padding = padding;
    set_iv(c, iv);
    CSSM_RETURN r = alg == ALG_NONE && !key ? CSSM_OK : ctx_key(c, key);        /* ALG_NONE sem chave = contexto de wrap nulo */
    if (r) { CSSM_DeleteContext((u64)(uintptr_t)c); return r; }
    *out = (u64)(uintptr_t)c;
    return CSSM_OK;
}
CSSM_RETURN CSSM_CSP_CreateAsymmetricContext(intptr_t csp, u32 alg, const void *creds, const ckey_t *key, u32 padding, u64 *out) {
    (void)creds;
    if (csp != CSP_HANDLE) return ERR_ADDIN_HANDLE;
    if (!out || !key) return ERR_POINTER;
    ctx_t *c = ctx_new(CC_ASYM, alg);
    if (!c) return ERR_MEMORY;
    c->padding = padding;
    CSSM_RETURN r = ctx_key(c, key);
    if (r) { CSSM_DeleteContext((u64)(uintptr_t)c); return r; }
    *out = (u64)(uintptr_t)c;
    return CSSM_OK;
}
CSSM_RETURN CSSM_CSP_CreateDeriveKeyContext(intptr_t csp, u32 alg, u32 dtype, u32 dbits, const void *creds, const ckey_t *base,
                                            u32 iter, const cdata_t *salt, const crypto_data_t *seed, u64 *out) {
    (void)creds;
    if (csp != CSP_HANDLE) return ERR_ADDIN_HANDLE;
    if (!out) return ERR_POINTER;
    ctx_t *c = ctx_new(CC_DERIVE, alg);
    if (!c) return ERR_MEMORY;
    c->derive_type = dtype; c->derive_bits = dbits; c->iter = iter;
    if (salt && salt->Data && salt->Length) { c->salt = malloc(salt->Length); memcpy(c->salt, salt->Data, salt->Length); c->saltn = salt->Length; }
    if (seed && seed->Param.Data && seed->Param.Length) { c->pass = malloc(seed->Param.Length); memcpy(c->pass, seed->Param.Data, seed->Param.Length); c->passn = seed->Param.Length; }
    CSSM_RETURN r = base ? ctx_key(c, base) : CSSM_OK;
    if (r) { CSSM_DeleteContext((u64)(uintptr_t)c); return r; }
    *out = (u64)(uintptr_t)c;
    return CSSM_OK;
}
CSSM_RETURN CSSM_UpdateContextAttributes(u64 h, u32 n, const cattr_t *a) {
    ctx_t *c = ctx_of(h);
    if (!c) return ERR_CONTEXT;
    if (n && !a) return ERR_POINTER;
    for (u32 i = 0; i < n; i++) {
        switch (a[i].type) {
        case AT_PADDING: c->padding = a[i].v.u; break;
        case AT_MODE: c->mode = a[i].v.u; break;
        case AT_ALG_ID: c->alg = a[i].v.u; break;
        case AT_ITER: case AT_ROUNDS: c->iter = a[i].v.u; break;
        case AT_KEY_LENGTH: c->derive_bits = a[i].v.u; break;
        case AT_KEY_TYPE: c->derive_type = a[i].v.u; break;
        case AT_IV: set_iv(c, a[i].v.data); break;
        case AT_KEY: { CSSM_RETURN r = ctx_key(c, a[i].v.key); if (r) return r; break; }
        case AT_SALT:
            free(c->salt); c->salt = 0; c->saltn = 0;
            if (a[i].v.data && a[i].v.data->Data) { c->salt = malloc(a[i].v.data->Length); memcpy(c->salt, a[i].v.data->Data, a[i].v.data->Length); c->saltn = a[i].v.data->Length; }
            break;
        case AT_PASSPHRASE: case AT_SEED: {
            const crypto_data_t *cd = a[i].v.p;
            if (c->pass) { memset(c->pass, 0, c->passn); free(c->pass); c->pass = 0; c->passn = 0; }
            if (cd && cd->Param.Data) { c->pass = malloc(cd->Param.Length); memcpy(c->pass, cd->Param.Data, cd->Param.Length); c->passn = cd->Param.Length; }
            break;
        }
        default: break;                                                          /* atributos sem efeito aqui (rótulo, datas...) */
        }
    }
    return CSSM_OK;
}

/* ---- saída: usa o buffer do app se couber; Data = NULL ganha malloc */
static CSSM_RETURN put_out(cdata_t *o, const u8 *b, size_t n) {
    if (!o) return ERR_POINTER;
    if (o->Data) {
        if (o->Length < n) return ERR_OUTPUT_LEN;
    } else {
        o->Data = malloc(n ? n : 1);
        if (!o->Data) return ERR_MEMORY;
    }
    memcpy(o->Data, b, n);
    o->Length = n;
    return CSSM_OK;
}
static id data_of(const u8 *b, size_t n) { return CFDataCreate(0, b, (long)n); }

/* ================================================================ assinatura */
/* algoritmo do SecKey para o contexto + o digest que o app diz ter aplicado; md5 = 1 quando sai pelo SecKeyRaw* */
static id sig_alg(ctx_t *c, u32 digest, int *md5) {
    *md5 = 0;
    static const void *const *rd[] = {&kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1, &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224,
                                      &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256, &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384,
                                      &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512};
    static const void *const *rm[] = {&kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1, &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA224,
                                      &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256, &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384,
                                      &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512};
    static const void *const *em[] = {&kSecKeyAlgorithmECDSASignatureMessageX962SHA1, &kSecKeyAlgorithmECDSASignatureMessageX962SHA224,
                                      &kSecKeyAlgorithmECDSASignatureMessageX962SHA256, &kSecKeyAlgorithmECDSASignatureMessageX962SHA384,
                                      &kSecKeyAlgorithmECDSASignatureMessageX962SHA512};
    int di = digest == ALG_SHA1 ? 0 : digest == ALG_SHA224 ? 1 : digest == ALG_SHA256 ? 2 : digest == ALG_SHA384 ? 3 : digest == ALG_SHA512 ? 4 : -1;
    switch (c->alg) {
    case ALG_RSA:
        if (digest == ALG_NONE) return K_(c->padding == PAD_NONE || c->padding == PAD_SIGRAW ? kSecKeyAlgorithmRSASignatureRaw : kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw);
        if (digest == ALG_MD5) { *md5 = 2; return K_(kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw); }
        return di >= 0 ? K_(*rd[di]) : 0;
    case ALG_ECDSA: case ALG_ECDSA_SPECIFIED: return K_(kSecKeyAlgorithmECDSASignatureDigestX962);
    case ALG_SHA1RSA: return K_(*rm[0]);
    case ALG_SHA224RSA: return K_(*rm[1]);
    case ALG_SHA256RSA: return K_(*rm[2]);
    case ALG_SHA384RSA: return K_(*rm[3]);
    case ALG_SHA512RSA: return K_(*rm[4]);
    case ALG_MD5RSA: *md5 = 1; return K_(kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw);
    case ALG_SHA1ECDSA: return K_(*em[0]);
    case ALG_SHA224ECDSA: return K_(*em[1]);
    case ALG_SHA256ECDSA: return K_(*em[2]);
    case ALG_SHA384ECDSA: return K_(*em[3]);
    case ALG_SHA512ECDSA: return K_(*em[4]);
    }
    return 0;
}
static CSSM_RETURN do_sign(ctx_t *c, const u8 *b, size_t n, u32 digest, cdata_t *sig) {
    int md5;
    id alg = sig_alg(c, digest, &md5);
    if (!alg) return ERR_ALG;
    if (md5) {                                                                   /* MD5: DigestInfo pelo SecKeyRawSign */
        u8 md[16];
        if (md5 == 1) { CC_MD5(b, (u32)n, md); b = md; n = 16; }
        size_t sl = SecKeyGetBlockSize(c->key);
        u8 *s = malloc(sl ? sl : 1);
        if (!s) return ERR_MEMORY;
        OSStatus st = SecKeyRawSign(c->key, 0x8001, b, n, s, &sl);
        CSSM_RETURN r = st ? ERR_DATA : put_out(sig, s, sl);
        free(s);
        return r;
    }
    id in = data_of(b, n), e = 0;
    id s = SecKeyCreateSignature(c->key, alg, in, &e);
    CFRelease(in);
    if (e) CFRelease(e);
    if (!s) return ERR_DATA;
    CSSM_RETURN r = put_out(sig, CFDataGetBytePtr(s), (size_t)CFDataGetLength(s));
    CFRelease(s);
    return r;
}
static CSSM_RETURN do_verify(ctx_t *c, const u8 *b, size_t n, u32 digest, const cdata_t *sig) {
    if (!sig || !sig->Data) return ERR_POINTER;
    int md5;
    id alg = sig_alg(c, digest, &md5);
    if (!alg) return ERR_ALG;
    if (md5) {
        u8 md[16];
        if (md5 == 1) { CC_MD5(b, (u32)n, md); b = md; n = 16; }
        return SecKeyRawVerify(c->key, 0x8001, b, n, sig->Data, sig->Length) ? ERR_VERIFY : CSSM_OK;
    }
    id in = data_of(b, n), sd = data_of(sig->Data, sig->Length), e = 0;
    u8 ok = SecKeyVerifySignature(c->key, alg, in, sd, &e);
    CFRelease(in); CFRelease(sd);
    if (e) CFRelease(e);
    return ok ? CSSM_OK : ERR_VERIFY;
}
static int gather(const cdata_t *bufs, u32 count, u8 **out, size_t *n) {
    size_t tot = 0;
    for (u32 i = 0; i < count; i++) tot += bufs[i].Length;
    u8 *p = malloc(tot ? tot : 1);
    if (!p) return -1;
    size_t o = 0;
    for (u32 i = 0; i < count; i++) { if (bufs[i].Length) memcpy(p + o, bufs[i].Data, bufs[i].Length); o += bufs[i].Length; }
    *out = p; *n = tot;
    return 0;
}
CSSM_RETURN CSSM_SignData(u64 h, const cdata_t *bufs, u32 count, u32 digest, cdata_t *sig) {
    ctx_t *c = ctx_of(h);
    if (!c || c->type != CC_SIG) return ERR_CONTEXT;
    if ((!bufs && count) || !sig) return ERR_POINTER;
    u8 *b; size_t n;
    if (gather(bufs, count, &b, &n)) return ERR_MEMORY;
    CSSM_RETURN r = do_sign(c, b, n, digest, sig);
    free(b);
    return r;
}
CSSM_RETURN CSSM_SignDataInit(u64 h) { ctx_t *c = ctx_of(h); if (!c || c->type != CC_SIG) return ERR_CONTEXT; acc_reset(c); return CSSM_OK; }
CSSM_RETURN CSSM_SignDataUpdate(u64 h, const cdata_t *bufs, u32 count) {
    ctx_t *c = ctx_of(h);
    if (!c || c->type != CC_SIG) return ERR_CONTEXT;
    for (u32 i = 0; i < count; i++) acc_add(c, bufs[i].Data, bufs[i].Length);
    return CSSM_OK;
}
CSSM_RETURN CSSM_SignDataFinal(u64 h, cdata_t *sig) {
    ctx_t *c = ctx_of(h);
    if (!c || c->type != CC_SIG) return ERR_CONTEXT;
    CSSM_RETURN r = do_sign(c, c->acc ? c->acc : (const u8 *)"", c->accn, ALG_NONE, sig);
    acc_reset(c);
    return r;
}
CSSM_RETURN CSSM_VerifyData(u64 h, const cdata_t *bufs, u32 count, u32 digest, const cdata_t *sig) {
    ctx_t *c = ctx_of(h);
    if (!c || c->type != CC_SIG) return ERR_CONTEXT;
    if (!bufs && count) return ERR_POINTER;
    u8 *b; size_t n;
    if (gather(bufs, count, &b, &n)) return ERR_MEMORY;
    CSSM_RETURN r = do_verify(c, b, n, digest, sig);
    free(b);
    return r;
}
CSSM_RETURN CSSM_VerifyDataInit(u64 h) { return CSSM_SignDataInit(h); }
CSSM_RETURN CSSM_VerifyDataUpdate(u64 h, const cdata_t *bufs, u32 count) { return CSSM_SignDataUpdate(h, bufs, count); }
CSSM_RETURN CSSM_VerifyDataFinal(u64 h, const cdata_t *sig) {
    ctx_t *c = ctx_of(h);
    if (!c || c->type != CC_SIG) return ERR_CONTEXT;
    CSSM_RETURN r = do_verify(c, c->acc ? c->acc : (const u8 *)"", c->accn, ALG_NONE, sig);
    acc_reset(c);
    return r;
}

/* ================================================================ cifra */
enum { kCCModeECB = 1, kCCModeCBC = 2, kCCModeCFB = 3, kCCModeOFB = 7, kCCModeRC4 = 9 };
static CSSM_RETURN sym_open(ctx_t *c, int decrypt) {
    const u8 *kb; size_t kn; int cc;
    if (!c->key || cx_symkey_raw(c->key, &kb, &kn, &cc)) return ERR_KEY;
    u32 m;
    int pad = c->padding == PAD_PKCS5 || c->padding == PAD_PKCS7;
    switch (c->mode) {
    case MODE_NONE: case MODE_ECB: m = kCCModeECB; break;
    case MODE_ECBPad: m = kCCModeECB; pad = 1; break;
    case MODE_CBC: case MODE_CBC_IV8: m = kCCModeCBC; break;
    case MODE_CBCPadIV8: m = kCCModeCBC; pad = 1; break;
    case MODE_CFB: case MODE_CFB_IV8: case MODE_CFBPadIV8: m = kCCModeCFB; pad = 0; break;
    case MODE_OFB: case MODE_OFB_IV8: case MODE_OFBPadIV8: m = kCCModeOFB; pad = 0; break;
    default: return ERR_ATTR_MODE;
    }
    if (cc == 4) { m = kCCModeRC4; pad = 0; }                                    /* RC4 é fluxo */
    else if (m != kCCModeECB && !c->has_iv) return ERR_ATTR_IV;
    if (c->cryptor) { CCCryptorRelease(c->cryptor); c->cryptor = 0; }
    int r = CCCryptorCreateWithMode((u32)decrypt, m, (u32)cc, pad ? 1u : 0u, c->iv, kb, kn, 0, 0, 0, 0, &c->cryptor);
    if (r) return r == -4305 ? ERR_ALG : ERR_KEY;
    c->crypt_dir = decrypt;
    return CSSM_OK;
}
static CSSM_RETURN sym_step(ctx_t *c, const u8 *in, size_t n, int final, u8 **out, size_t *on) {
    size_t cap = CCCryptorGetOutputLength(c->cryptor, n, (u8)final) + 32, a = 0, b = 0;
    u8 *o = malloc(cap);
    if (!o) return ERR_MEMORY;
    int r = n ? CCCryptorUpdate(c->cryptor, in, n, o, cap, &a) : 0;
    if (!r && final) r = CCCryptorFinal(c->cryptor, o + a, cap - a, &b);
    if (r) { free(o); return r == -4303 ? ERR_INPUT_LEN : ERR_DATA; }
    *out = o; *on = a + b;
    return CSSM_OK;
}
/* RSA: um bloco por vez (PKCS#1, OAEP-SHA1 ou bruto) */
static CSSM_RETURN rsa_crypt(ctx_t *c, const u8 *in, size_t n, int decrypt, u8 **out, size_t *on) {
    id alg = K_(kSecKeyAlgorithmRSAEncryptionPKCS1);
    size_t k = SecKeyGetBlockSize(c->key), room = k > 11 ? k - 11 : 0;
    if (c->mode == MODE_OAEP) { alg = K_(kSecKeyAlgorithmRSAEncryptionOAEPSHA1); room = k > 42 ? k - 42 : 0; }
    else if (c->padding == PAD_NONE && c->mode != MODE_V15) { alg = K_(kSecKeyAlgorithmRSAEncryptionRaw); room = k; }
    else if (c->padding != PAD_PKCS1 && c->padding != PAD_NONE) return ERR_ATTR_PADDING;
    size_t step = decrypt ? k : room;
    if (!step || (decrypt && n % k)) return ERR_INPUT_LEN;
    u8 *o = malloc(n / step * k + k + 1);
    size_t o_n = 0;
    if (!o) return ERR_MEMORY;
    for (size_t off = 0; off < n; off += step) {
        size_t cl = n - off < step ? n - off : step;
        id d = data_of(in + off, cl), e = 0;
        id r = decrypt ? SecKeyCreateDecryptedData(c->key, alg, d, &e) : SecKeyCreateEncryptedData(c->key, alg, d, &e);
        CFRelease(d);
        if (e) CFRelease(e);
        if (!r) { free(o); return ERR_DATA; }
        memcpy(o + o_n, CFDataGetBytePtr(r), (size_t)CFDataGetLength(r));
        o_n += (size_t)CFDataGetLength(r);
        CFRelease(r);
    }
    *out = o; *on = o_n;
    return CSSM_OK;
}
/* espalha a saída: primeiro nos buffers do app (Data = NULL no primeiro ganha tudo), o resto em rem */
static CSSM_RETURN scatter(const u8 *b, size_t n, cdata_t *bufs, u32 count, size_t *done, cdata_t *rem) {
    size_t o = 0;
    if (done) *done = 0;
    if (count && bufs && !bufs[0].Data) {
        CSSM_RETURN r = put_out(&bufs[0], b, n);
        if (!r && done) *done = n;
        if (rem) rem->Length = 0;
        return r;
    }
    for (u32 i = 0; i < count && o < n; i++) {
        size_t m = bufs[i].Length < n - o ? bufs[i].Length : n - o;
        memcpy(bufs[i].Data, b + o, m);
        bufs[i].Length = m;
        o += m;
    }
    if (done) *done = o;
    if (o < n) {
        if (!rem) return ERR_OUTPUT_LEN;
        if (rem->Data && rem->Length < n - o) return ERR_OUTPUT_LEN;
        CSSM_RETURN r = put_out(rem, b + o, n - o);
        if (!r && done) *done = n;
        return r;
    }
    if (rem && rem->Data) rem->Length = 0;
    return CSSM_OK;
}
static CSSM_RETURN crypt_all(u64 h, const cdata_t *in, u32 inc, cdata_t *out, u32 outc, size_t *done, cdata_t *rem, int decrypt) {
    ctx_t *c = ctx_of(h);
    if (!c || (c->type != CC_SYM && c->type != CC_ASYM)) return ERR_CONTEXT;
    if (!in && inc) return ERR_POINTER;
    u8 *b, *o = 0;
    size_t n, on = 0;
    if (gather(in, inc, &b, &n)) return ERR_MEMORY;
    CSSM_RETURN r;
    if (c->type == CC_ASYM) r = rsa_crypt(c, b, n, decrypt, &o, &on);
    else { r = sym_open(c, decrypt); if (!r) r = sym_step(c, b, n, 1, &o, &on); if (c->cryptor) { CCCryptorRelease(c->cryptor); c->cryptor = 0; } }
    if (!r) r = scatter(o, on, out, outc, done, rem);
    if (o) { memset(o, 0, on); free(o); }
    memset(b, 0, n); free(b);
    return r;
}
CSSM_RETURN CSSM_EncryptData(u64 h, const cdata_t *in, u32 inc, cdata_t *out, u32 outc, size_t *done, cdata_t *rem) { return crypt_all(h, in, inc, out, outc, done, rem, 0); }
CSSM_RETURN CSSM_DecryptData(u64 h, const cdata_t *in, u32 inc, cdata_t *out, u32 outc, size_t *done, cdata_t *rem) { return crypt_all(h, in, inc, out, outc, done, rem, 1); }
static CSSM_RETURN crypt_init(u64 h, int decrypt) {
    ctx_t *c = ctx_of(h);
    if (!c || (c->type != CC_SYM && c->type != CC_ASYM)) return ERR_CONTEXT;
    acc_reset(c);
    c->crypt_dir = decrypt;
    return c->type == CC_SYM ? sym_open(c, decrypt) : CSSM_OK;
}
static CSSM_RETURN crypt_update(u64 h, const cdata_t *in, u32 inc, cdata_t *out, u32 outc, size_t *done, int decrypt) {
    ctx_t *c = ctx_of(h);
    if (!c || (c->type != CC_SYM && c->type != CC_ASYM) || c->crypt_dir != decrypt) return ERR_CONTEXT;
    if (done) *done = 0;
    if (c->type == CC_ASYM) { for (u32 i = 0; i < inc; i++) acc_add(c, in[i].Data, in[i].Length); return CSSM_OK; }
    if (!c->cryptor) return ERR_CONTEXT;
    u8 *b, *o = 0;
    size_t n, on = 0;
    if (gather(in, inc, &b, &n)) return ERR_MEMORY;
    CSSM_RETURN r = sym_step(c, b, n, 0, &o, &on);
    if (!r) r = scatter(o, on, out, outc, done, 0);
    if (o) { memset(o, 0, on); free(o); }
    free(b);
    return r;
}
static CSSM_RETURN crypt_final(u64 h, cdata_t *rem, int decrypt) {
    ctx_t *c = ctx_of(h);
    if (!c || (c->type != CC_SYM && c->type != CC_ASYM) || c->crypt_dir != decrypt) return ERR_CONTEXT;
    u8 *o = 0;
    size_t on = 0;
    CSSM_RETURN r;
    if (c->type == CC_ASYM) { r = rsa_crypt(c, c->acc ? c->acc : (const u8 *)"", c->accn, decrypt, &o, &on); acc_reset(c); }
    else {
        if (!c->cryptor) return ERR_CONTEXT;
        r = sym_step(c, 0, 0, 1, &o, &on);
        CCCryptorRelease(c->cryptor); c->cryptor = 0;
    }
    if (!r) r = put_out(rem, o ? o : (const u8 *)"", on);
    if (o) { memset(o, 0, on); free(o); }
    return r;
}
CSSM_RETURN CSSM_EncryptDataInit(u64 h) { return crypt_init(h, 0); }
CSSM_RETURN CSSM_DecryptDataInit(u64 h) { return crypt_init(h, 1); }
CSSM_RETURN CSSM_EncryptDataUpdate(u64 h, const cdata_t *in, u32 inc, cdata_t *out, u32 outc, size_t *done) { return crypt_update(h, in, inc, out, outc, done, 0); }
CSSM_RETURN CSSM_DecryptDataUpdate(u64 h, const cdata_t *in, u32 inc, cdata_t *out, u32 outc, size_t *done) { return crypt_update(h, in, inc, out, outc, done, 1); }
CSSM_RETURN CSSM_EncryptDataFinal(u64 h, cdata_t *rem) { return crypt_final(h, rem, 0); }
CSSM_RETURN CSSM_DecryptDataFinal(u64 h, cdata_t *rem) { return crypt_final(h, rem, 1); }

/* ================================================================ derivação, wrap e unwrap */
static void raw_key_out(ckey_t *out, u32 alg, u32 cls, const u8 *b, size_t n, u32 usage, u32 attr, u32 fmt) {
    memset(&out->h, 0, sizeof out->h);
    out->h.HeaderVersion = 2;
    out->h.BlobType = BLOB_RAW;
    out->h.Format = fmt;
    out->h.AlgorithmId = alg;
    out->h.KeyClass = cls;
    out->h.LogicalKeySizeInBits = (u32)n * 8;
    out->h.KeyUsage = usage;
    out->h.KeyAttr = (attr & 0xffff) | 0x20;
    out->KeyData.Data = malloc(n ? n : 1);
    if (out->KeyData.Data) memcpy(out->KeyData.Data, b, n);
    out->KeyData.Length = out->KeyData.Data ? n : 0;
}
CSSM_RETURN CSSM_DeriveKey(u64 h, cdata_t *param, u32 usage, u32 attr, const cdata_t *label, const void *acl, ckey_t *out) {
    (void)label; (void)acl;
    ctx_t *c = ctx_of(h);
    if (!c || c->type != CC_DERIVE) return ERR_CONTEXT;
    if (!out) return ERR_POINTER;
    size_t n = (c->derive_bits + 7) / 8;
    if (c->alg == ALG_PBKDF2) {
        if (!param || !param->Data || param->Length < sizeof(pbkdf2_params_t)) return ERR_ATTR_PASSPHRASE;
        const pbkdf2_params_t *p = (const pbkdf2_params_t *)param->Data;
        if (p->PseudoRandomFunction != 0) return ERR_ALG;                        /* só HMAC-SHA1, como o CSP da Apple */
        if (!c->salt) return ERR_ATTR_SALT;
        if (!n || n > 1024) return ERR_KEY_SIZE;
        u8 *dk = malloc(n);
        if (!dk) return ERR_MEMORY;
        int r = CCKeyDerivationPBKDF(2, (const char *)p->Passphrase.Data, p->Passphrase.Length, c->salt, c->saltn, 1, c->iter ? c->iter : 1, dk, n);
        if (!r) raw_key_out(out, c->derive_type, KC_SESSION, dk, n, usage, attr, FMT_OCTET);
        memset(dk, 0, n); free(dk);
        return r ? ERR_DATA : CSSM_OK;
    }
    if (c->alg == ALG_ECDH) {                                                    /* Param = pública do par (X9.63) */
        if (!c->key || !param || !param->Data) return ERR_POINTER;
        id dict = ((id (*)(id, void *))objc_msgSend)((id)cx_class_named("NSMutableDictionary"), sel_registerName("dictionary"));
        ((void (*)(id, void *, id, id))objc_msgSend)(dict, sel_registerName("setObject:forKey:"), K_(kSecAttrKeyTypeECSECPrimeRandom), K_(kSecAttrKeyType));
        ((void (*)(id, void *, id, id))objc_msgSend)(dict, sel_registerName("setObject:forKey:"), K_(kSecAttrKeyClassPublic), K_(kSecAttrKeyClass));
        id pd = data_of(param->Data, param->Length), e = 0;
        id pub = SecKeyCreateWithData(pd, dict, &e);
        CFRelease(pd);
        if (e) { CFRelease(e); e = 0; }
        if (!pub) return ERR_KEY;
        id z = SecKeyCopyKeyExchangeResult(c->key, K_(kSecKeyAlgorithmECDHKeyExchangeStandard), pub, 0, &e);
        objc_release(pub);
        if (e) CFRelease(e);
        if (!z) return ERR_DATA;
        size_t zn = (size_t)CFDataGetLength(z);
        if (n && n < zn) zn = n;
        raw_key_out(out, c->derive_type, KC_SESSION, CFDataGetBytePtr(z), zn, usage, attr, FMT_OCTET);
        CFRelease(z);
        return CSSM_OK;
    }
    return ERR_ALG;
}
/* bytes crus de uma chave + formato do blob */
static int key_raw(id key, u8 **b, size_t *n, u32 *fmt) {
    const u8 *kb; size_t kn; int cc;
    if (!cx_symkey_raw(key, &kb, &kn, &cc)) { *b = malloc(kn); memcpy(*b, kb, kn); *n = kn; *fmt = FMT_OCTET; return 0; }
    id e = 0, d = SecKeyCopyExternalRepresentation(key, &e);
    if (e) CFRelease(e);
    if (!d) return -1;
    int rsa, priv, bits;
    id a = key_attrs_type(key, &rsa, &priv, &bits);
    if (a) objc_release(a);
    *n = (size_t)CFDataGetLength(d);
    *b = malloc(*n);
    memcpy(*b, CFDataGetBytePtr(d), *n);
    CFRelease(d);
    *fmt = rsa ? FMT_PKCS1 : FMT_OCTET;
    return 0;
}
CSSM_RETURN CSSM_WrapKey(u64 h, const void *creds, const ckey_t *key, const cdata_t *desc, ckey_t *out) {
    (void)creds; (void)desc;
    ctx_t *c = ctx_of(h);
    if (!c || (c->type != CC_SYM && c->type != CC_ASYM)) return ERR_CONTEXT;
    if (!key || !out) return ERR_POINTER;
    id k = key_of(key);
    if (!k) return ERR_KEY;
    u8 *b; size_t n; u32 fmt;
    int kr = key_raw(k, &b, &n, &fmt);
    keyhdr_t h0;
    fill_hdr(&h0, k);
    objc_release(k);
    if (kr) return ERR_KEY;
    if (c->alg == ALG_NONE) {                                                    /* wrap nulo: a chave crua */
        raw_key_out(out, h0.AlgorithmId, h0.KeyClass, b, n, h0.KeyUsage, h0.KeyAttr, fmt);
        out->h.LogicalKeySizeInBits = h0.LogicalKeySizeInBits;
        memset(b, 0, n); free(b);
        return CSSM_OK;
    }
    u8 *o = 0; size_t on = 0;
    CSSM_RETURN r;
    if (c->type == CC_ASYM) r = rsa_crypt(c, b, n, 0, &o, &on);
    else { r = sym_open(c, 0); if (!r) r = sym_step(c, b, n, 1, &o, &on); if (c->cryptor) { CCCryptorRelease(c->cryptor); c->cryptor = 0; } }
    memset(b, 0, n); free(b);
    if (r) return r;
    raw_key_out(out, h0.AlgorithmId, h0.KeyClass, o, on, h0.KeyUsage, h0.KeyAttr, FMT_WRAPPED_NONE);
    out->h.BlobType = BLOB_WRAPPED;
    out->h.LogicalKeySizeInBits = h0.LogicalKeySizeInBits;
    out->h.WrapAlgorithmId = c->alg;
    out->h.WrapMode = c->mode;
    memset(o, 0, on); free(o);
    return CSSM_OK;
}
CSSM_RETURN CSSM_UnwrapKey(u64 h, const ckey_t *pub, const ckey_t *wrapped, u32 usage, u32 attr, const cdata_t *label,
                           const void *acl, ckey_t *out, cdata_t *desc) {
    (void)pub; (void)label; (void)acl;
    ctx_t *c = ctx_of(h);
    if (!c || (c->type != CC_SYM && c->type != CC_ASYM)) return ERR_CONTEXT;
    if (!wrapped || !out || !wrapped->KeyData.Data) return ERR_POINTER;
    if (desc) desc->Length = 0;
    const keyhdr_t *wh = &wrapped->h;
    if (c->alg == ALG_NONE || wh->BlobType == BLOB_RAW) {
        raw_key_out(out, wh->AlgorithmId, wh->KeyClass, wrapped->KeyData.Data, wrapped->KeyData.Length, usage, attr, wh->Format);
        out->h.LogicalKeySizeInBits = wh->LogicalKeySizeInBits ? wh->LogicalKeySizeInBits : out->h.LogicalKeySizeInBits;
        return CSSM_OK;
    }
    u8 *o = 0; size_t on = 0;
    CSSM_RETURN r;
    if (c->type == CC_ASYM) r = rsa_crypt(c, wrapped->KeyData.Data, wrapped->KeyData.Length, 1, &o, &on);
    else { r = sym_open(c, 1); if (!r) r = sym_step(c, wrapped->KeyData.Data, wrapped->KeyData.Length, 1, &o, &on); if (c->cryptor) { CCCryptorRelease(c->cryptor); c->cryptor = 0; } }
    if (r) return r;
    u32 fmt = wh->KeyClass == KC_SESSION ? FMT_OCTET : wh->AlgorithmId == ALG_RSA ? FMT_PKCS1 : FMT_OCTET;
    raw_key_out(out, wh->AlgorithmId, wh->KeyClass, o, on, usage, attr, fmt);
    if (wh->LogicalKeySizeInBits) out->h.LogicalKeySizeInBits = wh->LogicalKeySizeInBits;
    memset(o, 0, on); free(o);
    return CSSM_OK;
}

/* ================================================================ CL: campos do certificado */
typedef struct { u8 tag; const u8 *start, *cont; size_t clen; const u8 *next; } tlv_t;
static int rd(const u8 *p, const u8 *end, tlv_t *t) {
    if (end - p < 2) return -1;
    t->start = p;
    t->tag = *p++;
    size_t len = *p++;
    if (len & 0x80) {
        int k = (int)(len & 0x7f);
        if (!k || k > 4 || end - p < k) return -1;
        len = 0;
        while (k--) len = len << 8 | *p++;
    }
    if ((size_t)(end - p) < len) return -1;
    t->cont = p; t->clen = len; t->next = p + len;
    return 0;
}
typedef struct {
    u8 *der; size_t n;
    tlv_t version, serial, tbs_alg, issuer, nbefore, nafter, subject, spki, spki_alg, exts, sig_alg, sig;
    int has_version, has_exts;
} cert_t;
static int parse_cert(cert_t *c) {
    tlv_t cert, tbs, t;
    const u8 *end = c->der + c->n;
    if (rd(c->der, end, &cert) || cert.tag != 0x30) return -1;
    if (rd(cert.cont, cert.next, &tbs) || tbs.tag != 0x30) return -1;
    const u8 *p = tbs.cont, *te = tbs.next;
    if (rd(p, te, &t)) return -1;
    if (t.tag == 0xa0) { if (rd(t.cont, t.next, &c->version)) return -1; c->has_version = 1; p = t.next; if (rd(p, te, &t)) return -1; }
    c->serial = t; p = t.next;
    if (rd(p, te, &c->tbs_alg)) return -1;
    p = c->tbs_alg.next;
    if (rd(p, te, &c->issuer)) return -1;
    p = c->issuer.next;
    tlv_t val;
    if (rd(p, te, &val) || rd(val.cont, val.next, &c->nbefore) || rd(c->nbefore.next, val.next, &c->nafter)) return -1;
    p = val.next;
    if (rd(p, te, &c->subject)) return -1;
    p = c->subject.next;
    if (rd(p, te, &c->spki) || rd(c->spki.cont, c->spki.next, &c->spki_alg)) return -1;
    p = c->spki.next;
    while (p < te && !rd(p, te, &t)) {
        if (t.tag == 0xa3 && !rd(t.cont, t.next, &c->exts)) c->has_exts = 1;
        p = t.next;
    }
    if (rd(tbs.next, cert.next, &c->sig_alg) || rd(c->sig_alg.next, cert.next, &c->sig)) return -1;
    return 0;
}
/* bloco de saída: [CSSM_DATA][struct][bytes]; os ponteiros internos apontam para dentro do bloco */
typedef struct { u8 *blk; size_t used, cap; } ob_t;
static void *ob_take(ob_t *o, size_t n) {
    size_t at = (o->used + 7) & ~(size_t)7;
    if (at + n > o->cap) return 0;
    o->used = at + n;
    return o->blk + at;
}
static cdata_t *ob_new(ob_t *o, size_t cap) {
    o->cap = cap + 64; o->used = 0;
    o->blk = calloc(1, o->cap);
    return o->blk ? ob_take(o, sizeof(cdata_t)) : 0;
}
static int ob_copy(ob_t *o, cdata_t *d, const u8 *b, size_t n) {
    d->Data = ob_take(o, n ? n : 1);
    if (!d->Data) return -1;
    if (n) memcpy(d->Data, b, n);
    d->Length = n;
    return 0;
}
static int alg_id(ob_t *o, const tlv_t *seq, cdata_t *out_struct) {    /* CSSM_X509_ALGORITHM_IDENTIFIER {oid, params} */
    tlv_t oid;
    if (rd(seq->cont, seq->next, &oid) || oid.tag != 0x06) return -1;
    cdata_t *ai = (cdata_t *)ob_take(o, 2 * sizeof(cdata_t));
    if (!ai) return -1;
    if (ob_copy(o, &ai[0], oid.cont, oid.clen)) return -1;
    if (oid.next < seq->next) { if (ob_copy(o, &ai[1], oid.next, (size_t)(seq->next - oid.next))) return -1; }
    out_struct->Data = (u8 *)ai; out_struct->Length = 2 * sizeof(cdata_t);
    return 0;
}
/* acha a extensão de OID igual a want; *crit, *val (conteúdo do OCTET STRING) */
static int find_ext(const cert_t *c, const u8 *want, size_t wn, int *crit, tlv_t *val) {
    if (!c->has_exts) return -1;
    const u8 *p = c->exts.cont, *e = c->exts.next;
    tlv_t ext, oid, t;
    while (p < e && !rd(p, e, &ext)) {
        p = ext.next;
        if (rd(ext.cont, ext.next, &oid) || oid.tag != 0x06) continue;
        if (oid.clen != wn || memcmp(oid.cont, want, wn)) continue;
        *crit = 0;
        if (rd(oid.next, ext.next, &t)) return -1;
        if (t.tag == 0x01) { *crit = t.clen && t.cont[0]; if (rd(t.next, ext.next, &t)) return -1; }
        if (t.tag != 0x04) return -1;
        *val = t;
        return 0;
    }
    return -1;
}
static const u8 R08_CERT[] = {0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x4d, 0x02, 0x01, 0x01, 0x01};
static const u8 R08_SIGN[] = {0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x4d, 0x02, 0x01, 0x03, 0x02};
/* CSSM_X509_EXTENSION: extnId 0, critical 16, format 20, value 24, BERvalue 32 (48 bytes) */
typedef struct { cdata_t extnId; int critical; u32 format; void *value; cdata_t BERvalue; } x509ext_t;
_Static_assert(sizeof(x509ext_t) == 48, "CSSM_X509_EXTENSION");
static CSSM_RETURN field_value(id certref, const cdata_t *oid, cdata_t **out) {
    cert_t c;
    memset(&c, 0, sizeof c);
    c.der = (u8 *)CFDataGetBytePtr(certref); c.n = (size_t)CFDataGetLength(certref);
    if (parse_cert(&c)) return ERR_CL_CERT;
    const u8 *o = oid->Data;
    size_t on = oid->Length;
    ob_t ob;
    cdata_t *v = ob_new(&ob, c.n * 2 + 256);
    if (!v) return ERR_MEMORY;
    int ok = -1, known = 1;
    if (on == sizeof R08_CERT + 1 && !memcmp(o, R08_CERT, sizeof R08_CERT)) {
        switch (o[on - 1]) {
        case 2: {                                                                /* versão: v1 = 0 quando não vem */
            u8 zero = 0;
            ok = c.has_version ? ob_copy(&ob, v, c.version.cont, c.version.clen) : ob_copy(&ob, v, &zero, 1);
            break;
        }
        case 3: ok = ob_copy(&ob, v, c.serial.cont, c.serial.clen); break;
        case 5: case 23: {                                                       /* emissor (5 = normalizado, 23 = DER) */
            id n = o[on - 1] == 5 ? SecCertificateCopyNormalizedIssuerSequence(certref) : 0;
            ok = n ? ob_copy(&ob, v, CFDataGetBytePtr(n), (size_t)CFDataGetLength(n)) : ob_copy(&ob, v, c.issuer.start, (size_t)(c.issuer.next - c.issuer.start));
            if (n) CFRelease(n);
            break;
        }
        case 8: case 22: {
            id n = o[on - 1] == 8 ? SecCertificateCopyNormalizedSubjectSequence(certref) : 0;
            ok = n ? ob_copy(&ob, v, CFDataGetBytePtr(n), (size_t)CFDataGetLength(n)) : ob_copy(&ob, v, c.subject.start, (size_t)(c.subject.next - c.subject.start));
            if (n) CFRelease(n);
            break;
        }
        case 6: case 7: {                                                        /* CSSM_X509_TIME {u8 timeType; CSSM_DATA time} */
            const tlv_t *t = o[on - 1] == 6 ? &c.nbefore : &c.nafter;
            u8 *tm = ob_take(&ob, 24);
            if (!tm) break;
            tm[0] = t->tag;
            ok = ob_copy(&ob, (cdata_t *)(tm + 8), t->cont, t->clen);
            v->Data = tm; v->Length = 24;
            break;
        }
        case 9: ok = alg_id(&ob, &c.spki_alg, v); break;
        default: known = 0;
        }
    } else if (on == sizeof R08_SIGN + 1 && !memcmp(o, R08_SIGN, sizeof R08_SIGN)) {
        switch (o[on - 1]) {
        case 1: ok = alg_id(&ob, &c.sig_alg, v); break;
        case 10: ok = alg_id(&ob, &c.tbs_alg, v); break;
        case 2: ok = c.sig.clen ? ob_copy(&ob, v, c.sig.cont + 1, c.sig.clen - 1) : -1; break;
        default: known = 0;
        }
    } else {                                                                     /* uma extensão pelo OID dela */
        int crit;
        tlv_t val;
        if (find_ext(&c, o, on, &crit, &val)) { free(ob.blk); return ERR_CL_NO_VALUES; }
        x509ext_t *x = ob_take(&ob, sizeof *x);
        if (!x || ob_copy(&ob, &x->extnId, o, on) || ob_copy(&ob, &x->BERvalue, val.cont, val.clen)) { free(ob.blk); return ERR_MEMORY; }
        x->critical = crit;
        x->format = 1;                                                           /* CSSM_X509_DATAFORMAT_PARSED */
        tlv_t in;
        int parsed = 0;
        static const u8 ku[] = {0x55, 0x1d, 0x0f}, eku[] = {0x55, 0x1d, 0x25}, bc[] = {0x55, 0x1d, 0x13}, ski[] = {0x55, 0x1d, 0x0e};
        if (on == 3 && !memcmp(o, ku, 3) && !rd(val.cont, val.next, &in) && in.tag == 0x03 && in.clen >= 2) {
            u16 *k = ob_take(&ob, sizeof(u16));                                  /* CE_KeyUsage: bit 0 da BIT STRING = 0x8000 */
            if (k) { *k = (u16)(in.cont[1] << 8 | (in.clen > 2 ? in.cont[2] : 0)); x->value = k; parsed = 1; }
        } else if (on == 3 && !memcmp(o, eku, 3) && !rd(val.cont, val.next, &in) && in.tag == 0x30) {
            u32 cnt = 0;                                                         /* CE_ExtendedKeyUsage {u32 n; CSSM_OID *purposes} */
            tlv_t t;
            for (const u8 *p = in.cont; p < in.next && !rd(p, in.next, &t); p = t.next) cnt++;
            u8 *e = ob_take(&ob, 16);
            cdata_t *ps = cnt ? ob_take(&ob, cnt * sizeof(cdata_t)) : 0;
            if (e && (ps || !cnt)) {
                u32 i = 0;
                for (const u8 *p = in.cont; p < in.next && !rd(p, in.next, &t); p = t.next) if (t.tag == 0x06 && !ob_copy(&ob, &ps[i], t.cont, t.clen)) i++;
                *(u32 *)e = i;
                *(cdata_t **)(e + 8) = ps;
                x->value = e; parsed = 1;
            }
        } else if (on == 3 && !memcmp(o, bc, 3) && !rd(val.cont, val.next, &in) && in.tag == 0x30) {
            u32 *b = ob_take(&ob, 12);                                           /* CE_BasicConstraints {cA, pathLenPresent, pathLen} */
            if (b) {
                tlv_t t;
                const u8 *p = in.cont;
                if (p < in.next && !rd(p, in.next, &t) && t.tag == 0x01) { b[0] = t.clen && t.cont[0] ? 1 : 0; p = t.next; }
                if (p < in.next && !rd(p, in.next, &t) && t.tag == 0x02) { u32 l = 0; for (size_t i = 0; i < t.clen && i < 4; i++) l = l << 8 | t.cont[i]; b[1] = 1; b[2] = l; }
                x->value = b; parsed = 1;
            }
        } else if (on == 3 && !memcmp(o, ski, 3) && !rd(val.cont, val.next, &in) && in.tag == 0x04) {
            cdata_t *k = ob_take(&ob, sizeof *k);                                /* CE_SubjectKeyID = CSSM_DATA */
            if (k && !ob_copy(&ob, k, in.cont, in.clen)) { x->value = k; parsed = 1; }
        }
        if (!parsed) {                                                           /* CSSM_X509_DATAFORMAT_ENCODED: TAGandVALUE {u8 tag; CSSM_DATA} */
            u8 *tv = ob_take(&ob, 24);
            if (tv) { tv[0] = 0x04; *(cdata_t *)(tv + 8) = x->BERvalue; }
            x->format = 0;
            x->value = tv;
        }
        v->Data = (u8 *)x; v->Length = sizeof *x;
        ok = 0;
    }
    if (ok) { free(ob.blk); return known ? ERR_CL_NO_VALUES : ERR_CL_TAG; }
    *out = v;
    return CSSM_OK;
}
typedef struct cache { struct cache *next; id cert; } cache_t;
static cache_t *g_cache;
static int g_results;                                                           /* resultados abertos (cada consulta dá um valor) */
CSSM_RETURN CSSM_CL_CertCache(intptr_t cl, const cdata_t *cert, intptr_t *handle) {
    if (cl != CL_HANDLE) return ERR_ADDIN_HANDLE;
    if (!cert || !cert->Data || !handle) return ERR_POINTER;
    cache_t *c = calloc(1, sizeof *c);
    if (!c) return ERR_MEMORY;
    c->cert = CFDataCreate(0, cert->Data, (long)cert->Length);
    cert_t pc;
    memset(&pc, 0, sizeof pc);
    pc.der = (u8 *)cert->Data; pc.n = cert->Length;
    if (!c->cert || parse_cert(&pc)) { if (c->cert) CFRelease(c->cert); free(c); return ERR_CL_CERT; }
    c->next = g_cache; g_cache = c;
    *handle = (intptr_t)c;
    return CSSM_OK;
}
CSSM_RETURN CSSM_CL_CertAbortCache(intptr_t cl, intptr_t handle) {
    if (cl != CL_HANDLE) return ERR_ADDIN_HANDLE;
    for (cache_t **pp = &g_cache; *pp; pp = &(*pp)->next)
        if ((intptr_t)*pp == handle) { cache_t *c = *pp; *pp = c->next; CFRelease(c->cert); free(c); return CSSM_OK; }
    return ERR_CL_CACHE;
}
static CSSM_RETURN first_value(id cert, const cdata_t *field, intptr_t *results, u32 *nmatch, cdata_t **value) {
    if (!field || !field->Data || !value) return ERR_POINTER;
    if (nmatch) *nmatch = 0;
    if (results) *results = 0;
    *value = 0;
    CSSM_RETURN r = field_value(cert, field, value);
    if (r) return r;
    if (nmatch) *nmatch = 1;
    if (results) { *results = (intptr_t)0x52534c54 + ++g_results; }
    return CSSM_OK;
}
CSSM_RETURN CSSM_CL_CertGetFirstCachedFieldValue(intptr_t cl, intptr_t handle, const cdata_t *field, intptr_t *results, u32 *nmatch, cdata_t **value) {
    if (cl != CL_HANDLE) return ERR_ADDIN_HANDLE;
    for (cache_t *c = g_cache; c; c = c->next) if ((intptr_t)c == handle) return first_value(c->cert, field, results, nmatch, value);
    return ERR_CL_CACHE;
}
CSSM_RETURN CSSM_CL_CertGetFirstFieldValue(intptr_t cl, const cdata_t *cert, const cdata_t *field, intptr_t *results, u32 *nmatch, cdata_t **value) {
    if (cl != CL_HANDLE) return ERR_ADDIN_HANDLE;
    if (!cert || !cert->Data) return ERR_POINTER;
    id d = CFDataCreate(0, cert->Data, (long)cert->Length);
    CSSM_RETURN r = first_value(d, field, results, nmatch, value);
    CFRelease(d);
    return r;
}
/* cada campo tem um valor só: não há próximo */
CSSM_RETURN CSSM_CL_CertGetNextCachedFieldValue(intptr_t cl, intptr_t results, cdata_t **value) {
    (void)results;
    if (cl != CL_HANDLE) return ERR_ADDIN_HANDLE;
    if (value) *value = 0;
    return ERR_CL_NO_VALUES;
}
CSSM_RETURN CSSM_CL_CertGetNextFieldValue(intptr_t cl, intptr_t results, cdata_t **value) { return CSSM_CL_CertGetNextCachedFieldValue(cl, results, value); }
CSSM_RETURN CSSM_CL_CertAbortQuery(intptr_t cl, intptr_t results) {
    if (cl != CL_HANDLE) return ERR_ADDIN_HANDLE;
    return results ? CSSM_OK : ERR_CL_RESULTS;
}
CSSM_RETURN CSSM_CL_FreeFieldValue(intptr_t cl, const cdata_t *oid, cdata_t *value) {
    (void)oid;
    if (cl != CL_HANDLE) return ERR_ADDIN_HANDLE;
    if (!value) return ERR_POINTER;
    free(value);                                                                 /* o CSSM_DATA é o começo do bloco */
    return CSSM_OK;
}

/* ================================================================ digest, geração de chave e módulos (corpus) */
/* GUIDs dos módulos da Apple (cssmapple.h): os apps passam estes ao CSSM_ModuleLoad/Attach */
const guid_t gGuidAppleFileDL = {0x87191ca0, 0x0fc9, 0x11d4, {0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22}};
const guid_t gGuidAppleCSPDL = {0x87191ca1, 0x0fc9, 0x11d4, {0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22}};
const guid_t gGuidAppleCSP = {0x87191ca2, 0x0fc9, 0x11d4, {0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22}};
const guid_t gGuidAppleX509CL = {0x87191ca3, 0x0fc9, 0x11d4, {0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22}};
const guid_t gGuidAppleX509TP = {0x87191ca4, 0x0fc9, 0x11d4, {0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22}};
const guid_t gGuidAppleDotMacTP = {0x87191ca5, 0x0fc9, 0x11d4, {0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22}};
const guid_t gGuidCssm = {0x87191ca6, 0x0fc9, 0x11d4, {0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22}};
CSSM_RETURN CSSM_ModuleLoad(const guid_t *guid, u32 level, void *cb, void *ctx) { (void)level; (void)cb; (void)ctx; return guid ? CSSM_OK : ERR_POINTER; }
CSSM_RETURN CSSM_ModuleUnload(const guid_t *guid, void *cb, void *ctx) { (void)cb; (void)ctx; return guid ? CSSM_OK : ERR_POINTER; }
/* o CSP e o CSPDL respondem com o CSP do mOS; o X509CL com o CL; o resto não tem módulo */
CSSM_RETURN CSSM_ModuleAttach(const guid_t *guid, const void *ver, const void *mem, u32 sub, u32 type, u32 flags, u32 key_hier,
                              void *fn_tab, u32 nfn, const void *reserved, intptr_t *out) {
    (void)ver; (void)mem; (void)sub; (void)type; (void)flags; (void)key_hier; (void)fn_tab; (void)nfn; (void)reserved;
    if (!guid || !out) return ERR_POINTER;
    if (!memcmp(guid, &gGuidAppleCSP, sizeof *guid) || !memcmp(guid, &gGuidAppleCSPDL, sizeof *guid)) { *out = CSP_HANDLE; return CSSM_OK; }
    if (!memcmp(guid, &gGuidAppleX509CL, sizeof *guid)) { *out = CL_HANDLE; return CSSM_OK; }
    *out = 0;
    return E_CSSM(0x10a);                                 /* CSSMERR_CSSM_INVALID_GUID */
}
CSSM_RETURN CSSM_ModuleDetach(intptr_t h) { return h == CSP_HANDLE || h == CL_HANDLE ? CSSM_OK : ERR_ADDIN_HANDLE; }
/* CSSM_API_MEMORY_FUNCS: malloc/free/realloc/calloc + AllocRef; os do mOS são os da libc */
typedef struct { void *(*malloc_func)(size_t, void *); void (*free_func)(void *, void *); void *(*realloc_func)(void *, size_t, void *);
                 void *(*calloc_func)(u32, size_t, void *); void *ref; } cssm_mem_t;
static void *m_malloc(size_t n, void *r) { (void)r; return malloc(n); }
static void m_free(void *p, void *r) { (void)r; free(p); }
static void *m_realloc(void *p, size_t n, void *r) { (void)r; return realloc(p, n); }
static void *m_calloc(u32 k, size_t n, void *r) { (void)r; return calloc(k, n); }
CSSM_RETURN CSSM_GetAPIMemoryFunctions(intptr_t h, cssm_mem_t *f) {
    (void)h;
    if (!f) return ERR_POINTER;
    f->malloc_func = m_malloc; f->free_func = m_free; f->realloc_func = m_realloc; f->calloc_func = m_calloc; f->ref = 0;
    return CSSM_OK;
}
/* digest: o contexto junta os bytes (acc) e o Final calcula pelo CommonCrypto */
enum { CC_DIGEST = 4, CC_KEYGEN = 6 };
unsigned char *CC_MD5(const void *d, u32 n, unsigned char *md);
unsigned char *CC_SHA1(const void *d, u32 n, unsigned char *md);
unsigned char *CC_SHA224(const void *d, u32 n, unsigned char *md);
unsigned char *CC_SHA256(const void *d, u32 n, unsigned char *md);
unsigned char *CC_SHA384(const void *d, u32 n, unsigned char *md);
unsigned char *CC_SHA512(const void *d, u32 n, unsigned char *md);
static int digest_len(u32 alg) {
    switch (alg) {
    case ALG_MD5: return 16;
    case ALG_SHA1: return 20;
    case ALG_SHA224: return 28;
    case ALG_SHA256: return 32;
    case ALG_SHA384: return 48;
    case ALG_SHA512: return 64;
    default: return 0;
    }
}
CSSM_RETURN CSSM_CSP_CreateDigestContext(intptr_t csp, u32 alg, u64 *out) {
    if (csp != CSP_HANDLE) return ERR_ADDIN_HANDLE;
    if (!out) return ERR_POINTER;
    if (!digest_len(alg)) return ERR_ALG;
    ctx_t *c = ctx_new(CC_DIGEST, alg);
    if (!c) return ERR_MEMORY;
    *out = (u64)(uintptr_t)c;
    return CSSM_OK;
}
CSSM_RETURN CSSM_DigestDataInit(u64 h) { ctx_t *c = ctx_of(h); if (!c || c->type != CC_DIGEST) return ERR_CONTEXT; acc_reset(c); return CSSM_OK; }
CSSM_RETURN CSSM_DigestDataUpdate(u64 h, const cdata_t *bufs, u32 count) {
    ctx_t *c = ctx_of(h);
    if (!c || c->type != CC_DIGEST) return ERR_CONTEXT;
    for (u32 i = 0; i < count; i++) if (bufs && bufs[i].Data) acc_add(c, bufs[i].Data, bufs[i].Length);
    return CSSM_OK;
}
CSSM_RETURN CSSM_DigestDataFinal(u64 h, cdata_t *out) {
    ctx_t *c = ctx_of(h);
    if (!c || c->type != CC_DIGEST) return ERR_CONTEXT;
    if (!out) return ERR_POINTER;
    int n = digest_len(c->alg);
    u8 md[64];
    const u8 *d = c->acc ? c->acc : (const u8 *)"";
    u32 len = (u32)c->accn;
    switch (c->alg) {
    case ALG_MD5: CC_MD5(d, len, md); break;
    case ALG_SHA1: CC_SHA1(d, len, md); break;
    case ALG_SHA224: CC_SHA224(d, len, md); break;
    case ALG_SHA256: CC_SHA256(d, len, md); break;
    case ALG_SHA384: CC_SHA384(d, len, md); break;
    default: CC_SHA512(d, len, md); break;
    }
    acc_reset(c);
    if (!out->Data) { out->Data = malloc((size_t)n); if (!out->Data) return ERR_MEMORY; }
    else if (out->Length < (size_t)n) return ERR_OUTPUT_LEN;
    memcpy(out->Data, md, (size_t)n);
    out->Length = (size_t)n;
    return CSSM_OK;
}
CSSM_RETURN CSSM_DigestData(u64 h, const cdata_t *bufs, u32 count, cdata_t *out) {
    CSSM_RETURN r = CSSM_DigestDataInit(h);
    if (!r) r = CSSM_DigestDataUpdate(h, bufs, count);
    return r ? r : CSSM_DigestDataFinal(h, out);
}
/* geração de chave simétrica (AES, DES, 3DES, RC2, RC4, Blowfish): bytes do gerador do sistema, blob cru */
int SecRandomCopyBytes(void *rnd, size_t n, void *buf);
CSSM_RETURN CSSM_CSP_CreateKeyGenContext(intptr_t csp, u32 alg, u32 bits, const cdata_t *seed, const cdata_t *salt, const void *start,
                                         const void *end, const cdata_t *params, u64 *out) {
    (void)seed; (void)salt; (void)start; (void)end; (void)params;
    if (csp != CSP_HANDLE) return ERR_ADDIN_HANDLE;
    if (!out) return ERR_POINTER;
    if (alg != ALG_AES && alg != ALG_DES && alg != ALG_3DES && alg != ALG_RC2 && alg != ALG_RC4 && alg != ALG_BLOWFISH && alg != ALG_CAST)
        return ERR_ALG;                                  /* pares RSA/EC: SecKeyCreateRandomKey */
    ctx_t *c = ctx_new(CC_KEYGEN, alg);
    if (!c) return ERR_MEMORY;
    c->derive_bits = bits;
    *out = (u64)(uintptr_t)c;
    return CSSM_OK;
}
CSSM_RETURN CSSM_GenerateKey(u64 h, u32 usage, u32 attr, const cdata_t *label, const void *acl, ckey_t *out) {
    (void)label; (void)acl;
    ctx_t *c = ctx_of(h);
    if (!c || c->type != CC_KEYGEN) return ERR_CONTEXT;
    if (!out) return ERR_POINTER;
    size_t n = (c->derive_bits + 7) / 8;
    if (c->alg == ALG_AES && n != 16 && n != 24 && n != 32) return ERR_KEY_SIZE;
    if (c->alg == ALG_DES) n = 8;
    if (c->alg == ALG_3DES) n = 24;
    if (!n || n > 256) return ERR_KEY_SIZE;
    u8 k[256];
    SecRandomCopyBytes(0, n, k);
    raw_key_out(out, c->alg, KC_SESSION, k, n, usage, attr, FMT_OCTET);
    memset(k, 0, sizeof k);
    return CSSM_OK;
}
/* SecKeychain e SSL que vinham junto */
u64 SecKeychainGetTypeID(void) { return 0x2F0; }
int SSLGetClientCertificateState(void *ctx, int *state) { (void)ctx; if (!state) return -50; *state = 0; return 0; }   /* kSSLClientCertNone */
