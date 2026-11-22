/*
 * mOS libSystem — CommonCrypto assimétrico (CommonRSACryptor.h, CommonECCryptor.h) e o KDF novo (CommonKeyDerivationSPI):
 *   CCRSACryptor* (gerar, importar/exportar PKCS#1, cifrar/decifrar PKCS#1 v1.5/OAEP/raw, assinar/verificar PKCS#1 v1.5/PSS,
 *   componentes n/e/p/q e CRT), CCECCryptor* (gerar, importar/exportar X9.63 e DER, ECDH) e CCDeriveKey com HKDF/PBKDF2.
 * Tudo em cima do SecKey do seckey.c: o CCRSACryptorRef/CCECCryptorRef é a própria SecKeyRef (CF, retida).
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
void CFRelease(const void *o);
id cx_data_new(const void *b, u64 n);
long CFDataGetLength(id d);
const u8 *CFDataGetBytePtr(id d);
id SecKeyCreateRandomKey(id params, id *err);
id SecKeyCreateWithData(id data, id attrs, id *err);
id SecKeyCopyPublicKey(id key);
id SecKeyCopyExternalRepresentation(id key, id *err);
id SecKeyCopyAttributes(id key);
size_t SecKeyGetBlockSize(id key);
id SecKeyCreateSignature(id key, id alg, id data, id *err);
u8 SecKeyVerifySignature(id key, id alg, id data, id sig, id *err);
id SecKeyCreateEncryptedData(id key, id alg, id data, id *err);
id SecKeyCreateDecryptedData(id key, id alg, id data, id *err);
id SecKeyCopyKeyExchangeResult(id key, id alg, id pub, id params, id *err);
int cx_seckey_spki(id key, u8 **out, u32 *n);
int cx_seckey_pkcs8(id key, u8 **out, u32 *n);
id cx_seckey_import(const u8 *der, u32 n, int priv);
void CCHmac(u32 alg, const void *key, size_t klen, const void *d, size_t n, void *mac);
int CCKeyDerivationPBKDF(u32 alg, const char *pw, size_t pwlen, const u8 *salt, size_t saltlen, u32 prf, u32 rounds, u8 *dk, size_t dklen);

extern id kSecAttrKeyType, kSecAttrKeyTypeRSA, kSecAttrKeyTypeECSECPrimeRandom, kSecAttrKeySizeInBits, kSecAttrKeyClass,
    kSecAttrKeyClassPrivate, kSecAttrKeyClassPublic;
extern id kSecKeyAlgorithmRSAEncryptionRaw, kSecKeyAlgorithmRSAEncryptionPKCS1, kSecKeyAlgorithmRSAEncryptionOAEPSHA1,
    kSecKeyAlgorithmRSAEncryptionOAEPSHA224, kSecKeyAlgorithmRSAEncryptionOAEPSHA256, kSecKeyAlgorithmRSAEncryptionOAEPSHA384,
    kSecKeyAlgorithmRSAEncryptionOAEPSHA512, kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1,
    kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256,
    kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512,
    kSecKeyAlgorithmRSASignatureDigestPSSSHA1, kSecKeyAlgorithmRSASignatureDigestPSSSHA224, kSecKeyAlgorithmRSASignatureDigestPSSSHA256,
    kSecKeyAlgorithmRSASignatureDigestPSSSHA384, kSecKeyAlgorithmRSASignatureDigestPSSSHA512, kSecKeyAlgorithmECDHKeyExchangeStandard;

#define kCCSuccess 0
#define kCCParamError (-4300)
#define kCCBufferTooSmall (-4301)
#define kCCMemoryFailure (-4302)
#define kCCDecodeError (-4304)
#define kCCUnimplemented (-4305)
#define kCCInvalidKey (-4311)
enum { ccPaddingNone = 1000, ccPKCS1Padding = 1001, ccOAEPPadding = 1002, ccX931Padding = 1003, ccRSAPSSPadding = 1005 };
enum { kCCDigestSHA1 = 8, kCCDigestSHA224 = 9, kCCDigestSHA256 = 10, kCCDigestSHA384 = 11, kCCDigestSHA512 = 12 };

static id dict(void) { return MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary"); }
static void put(id d, id k, id v) { if (k && v) MSG(void, d, "setObject:forKey:", v, k); }
static int key_is_private(id key) {
    id a = SecKeyCopyAttributes(key);
    if (!a) return -1;
    id c = MSG(id, a, "objectForKey:", kSecAttrKeyClass);
    int r = c ? (MSG(BOOL, c, "isEqual:", kSecAttrKeyClassPrivate) ? 1 : 0) : -1;
    CFRelease(a);
    return r;
}
static int key_is_type(id key, id type) {
    id a = key ? SecKeyCopyAttributes(key) : 0;
    if (!a) return 0;
    id t = MSG(id, a, "objectForKey:", kSecAttrKeyType);
    int r = t && MSG(BOOL, t, "isEqual:", type);
    CFRelease(a);
    return r;
}
static int out_bytes(id data, void *out, size_t *len) {
    if (!data) return kCCDecodeError;
    size_t n = (size_t)CFDataGetLength(data);
    if (!len) { CFRelease(data); return kCCParamError; }
    if (!out || *len < n) { *len = n; CFRelease(data); return kCCBufferTooSmall; }
    memcpy(out, CFDataGetBytePtr(data), n);
    *len = n;
    CFRelease(data);
    return kCCSuccess;
}
static id gen(id type, size_t bits) {
    id p = dict();
    put(p, kSecAttrKeyType, type);
    put(p, kSecAttrKeySizeInBits, MSG(id, (id)cx_class_named("NSNumber"), "numberWithUnsignedLong:", (unsigned long)bits));
    return SecKeyCreateRandomKey(p, 0);
}
static id import(id type, const void *b, size_t n, int priv) {
    id a = dict();
    put(a, kSecAttrKeyType, type);
    put(a, kSecAttrKeyClass, priv ? kSecAttrKeyClassPrivate : kSecAttrKeyClassPublic);
    id d = cx_data_new(b, n);
    id k = SecKeyCreateWithData(d, a, 0);
    CFRelease(d);
    return k;
}

/* ================================================================ RSA */
int CCRSACryptorGeneratePair(size_t bits, u32 e, id *pub, id *priv) {
    if (!pub || !priv || bits < 512 || bits > 8192) return kCCParamError;
    if (e != 65537 && e != 0) return kCCUnimplemented;               /* o gerador do seckey.c usa 65537 */
    id k = gen(kSecAttrKeyTypeRSA, bits);
    if (!k) return kCCMemoryFailure;
    *priv = k;
    *pub = SecKeyCopyPublicKey(k);
    return kCCSuccess;
}
id CCRSACryptorGetPublicKeyFromPrivateKey(id priv) { return priv ? SecKeyCopyPublicKey(priv) : 0; }
void CCRSACryptorRelease(id k) { if (k) CFRelease(k); }
int CCRSAGetKeyType(id k) {
    if (!k || !key_is_type(k, kSecAttrKeyTypeRSA)) return 99;           /* ccRSABadKey */
    return key_is_private(k) == 1 ? 1 : 0;                               /* ccRSAKeyPrivate : ccRSAKeyPublic */
}
int CCRSACryptorImport(const void *pkg, size_t n, id *key) {
    if (!pkg || !n || !key) return kCCParamError;
    id k = import(kSecAttrKeyTypeRSA, pkg, n, 1);
    if (!k) k = import(kSecAttrKeyTypeRSA, pkg, n, 0);
    if (!k) k = cx_seckey_import(pkg, (u32)n, 1);                       /* PKCS#8 / SPKI */
    if (!k) k = cx_seckey_import(pkg, (u32)n, 0);
    if (!k) return kCCDecodeError;
    *key = k;
    return kCCSuccess;
}
int CCRSACryptorExport(id key, void *out, size_t *len) { return key ? out_bytes(SecKeyCopyExternalRepresentation(key, 0), out, len) : kCCParamError; }
static id oaep_alg(u32 dg) {
    switch (dg) {
    case kCCDigestSHA224: return kSecKeyAlgorithmRSAEncryptionOAEPSHA224;
    case kCCDigestSHA256: return kSecKeyAlgorithmRSAEncryptionOAEPSHA256;
    case kCCDigestSHA384: return kSecKeyAlgorithmRSAEncryptionOAEPSHA384;
    case kCCDigestSHA512: return kSecKeyAlgorithmRSAEncryptionOAEPSHA512;
    default: return kSecKeyAlgorithmRSAEncryptionOAEPSHA1;
    }
}
static id enc_alg(int pad, u32 dg) {
    return pad == ccPKCS1Padding ? kSecKeyAlgorithmRSAEncryptionPKCS1 : pad == ccOAEPPadding ? oaep_alg(dg) : pad == ccPaddingNone ? kSecKeyAlgorithmRSAEncryptionRaw : 0;
}
int CCRSACryptorEncrypt(id key, int pad, const void *in, size_t inlen, void *out, size_t *outlen, const void *tag, size_t taglen, u32 dg) {
    (void)tag;
    if (!key || !in || taglen) return kCCParamError;                    /* rótulo do OAEP não suportado */
    id alg = enc_alg(pad, dg);
    if (!alg) return kCCParamError;
    id d = cx_data_new(in, inlen);
    id c = SecKeyCreateEncryptedData(key, alg, d, 0);
    CFRelease(d);
    return out_bytes(c, out, outlen);
}
int CCRSACryptorDecrypt(id key, int pad, const void *in, size_t inlen, void *out, size_t *outlen, const void *tag, size_t taglen, u32 dg) {
    (void)tag;
    if (!key || !in || taglen) return kCCParamError;
    id alg = enc_alg(pad, dg);
    if (!alg) return kCCParamError;
    id d = cx_data_new(in, inlen);
    id p = SecKeyCreateDecryptedData(key, alg, d, 0);
    CFRelease(d);
    return out_bytes(p, out, outlen);
}
static id sig_alg(int pad, u32 dg) {
    if (pad == ccPKCS1Padding)
        switch (dg) {
        case kCCDigestSHA1: return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1;
        case kCCDigestSHA224: return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224;
        case kCCDigestSHA256: return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256;
        case kCCDigestSHA384: return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384;
        case kCCDigestSHA512: return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512;
        case 0: return kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw;
        }
    if (pad == ccRSAPSSPadding)
        switch (dg) {
        case kCCDigestSHA1: return kSecKeyAlgorithmRSASignatureDigestPSSSHA1;
        case kCCDigestSHA224: return kSecKeyAlgorithmRSASignatureDigestPSSSHA224;
        case kCCDigestSHA256: return kSecKeyAlgorithmRSASignatureDigestPSSSHA256;
        case kCCDigestSHA384: return kSecKeyAlgorithmRSASignatureDigestPSSSHA384;
        case kCCDigestSHA512: return kSecKeyAlgorithmRSASignatureDigestPSSSHA512;
        }
    return 0;
}
int CCRSACryptorSign(id key, int pad, const void *hash, size_t hlen, u32 dg, size_t salt, void *sig, size_t *siglen) {
    (void)salt;
    if (!key || !hash) return kCCParamError;
    id alg = sig_alg(pad, dg);
    if (!alg) return kCCParamError;
    id d = cx_data_new(hash, hlen);
    id s = SecKeyCreateSignature(key, alg, d, 0);
    CFRelease(d);
    return out_bytes(s, sig, siglen);
}
int CCRSACryptorVerify(id key, int pad, const void *hash, size_t hlen, u32 dg, size_t salt, const void *sig, size_t siglen) {
    (void)salt;
    if (!key || !hash || !sig) return kCCParamError;
    id alg = sig_alg(pad, dg);
    if (!alg) return kCCParamError;
    id d = cx_data_new(hash, hlen), s = cx_data_new(sig, siglen);
    u8 ok = SecKeyVerifySignature(key, alg, d, s, 0);
    CFRelease(d); CFRelease(s);
    return ok ? kCCSuccess : kCCDecodeError;
}

/* ---- PKCS#1 em DER: SEQUENCE { INTEGER ... } */
typedef struct { const u8 *p; size_t n; } der_int;
static int der_len(const u8 *b, size_t n, size_t *pos, size_t *len) {
    if (*pos >= n) return -1;
    u8 c = b[(*pos)++];
    if (c < 0x80) { *len = c; return 0; }
    int k = c & 0x7f;
    if (k < 1 || k > 4) return -1;
    size_t v = 0;
    for (int i = 0; i < k; i++) { if (*pos >= n) return -1; v = v << 8 | b[(*pos)++]; }
    *len = v;
    return 0;
}
static int pkcs1_ints(const u8 *b, size_t n, der_int *out, int max) {
    size_t pos = 0, len;
    if (n < 2 || b[pos++] != 0x30 || der_len(b, n, &pos, &len) || pos + len > n) return -1;
    int k = 0;
    while (pos < n && k < max) {
        if (b[pos++] != 0x02 || der_len(b, n, &pos, &len) || pos + len > n) return -1;
        const u8 *p = b + pos;
        size_t l = len;
        while (l > 1 && *p == 0) { p++; l--; }                         /* tira o zero de sinal */
        out[k].p = p; out[k].n = l;
        k++;
        pos += len;
    }
    return k;
}
static int put_int(const der_int *v, void *out, size_t *len) {
    if (!len) return kCCParamError;
    if (!out || *len < v->n) { *len = v->n; return kCCBufferTooSmall; }
    memcpy(out, v->p, v->n);
    *len = v->n;
    return kCCSuccess;
}
/* público: n, e; privado: n, d (o expoente privado), p, q — como no CommonCrypto */
int CCRSAGetKeyComponents(id key, u8 *mod, size_t *modlen, u8 *exp, size_t *explen, u8 *p, size_t *plen, u8 *q, size_t *qlen) {
    if (!key) return kCCParamError;
    id d = SecKeyCopyExternalRepresentation(key, 0);
    if (!d) return kCCDecodeError;
    der_int v[9];
    int n = pkcs1_ints(CFDataGetBytePtr(d), (size_t)CFDataGetLength(d), v, 9);
    int st = kCCDecodeError;
    if (n == 2) {
        st = put_int(&v[0], mod, modlen);
        if (!st) st = put_int(&v[1], exp, explen);
        if (plen) *plen = 0;
        if (qlen) *qlen = 0;
    } else if (n >= 6) {                                                 /* version, n, e, d, p, q, dp, dq, qinv */
        st = put_int(&v[1], mod, modlen);
        if (!st) st = put_int(&v[3], exp, explen);
        if (!st && p) st = put_int(&v[4], p, plen);
        if (!st && q) st = put_int(&v[5], q, qlen);
    }
    CFRelease(d);
    return st;
}
int CCRSAGetCRTComponentsSizes(id key, size_t *dp, size_t *dq, size_t *qinv) {
    if (!key || key_is_private(key) != 1) return kCCParamError;
    id d = SecKeyCopyExternalRepresentation(key, 0);
    der_int v[9];
    int n = d ? pkcs1_ints(CFDataGetBytePtr(d), (size_t)CFDataGetLength(d), v, 9) : -1;
    if (d) CFRelease(d);
    if (n < 9) return kCCDecodeError;
    if (dp) *dp = v[6].n;
    if (dq) *dq = v[7].n;
    if (qinv) *qinv = v[8].n;
    return kCCSuccess;
}
int CCRSAGetCRTComponents(id key, void *dp, size_t dpn, void *dq, size_t dqn, void *qinv, size_t qn) {
    if (!key || key_is_private(key) != 1 || !dp || !dq || !qinv) return kCCParamError;
    id d = SecKeyCopyExternalRepresentation(key, 0);
    der_int v[9];
    int n = d ? pkcs1_ints(CFDataGetBytePtr(d), (size_t)CFDataGetLength(d), v, 9) : -1;
    int st = kCCDecodeError;
    if (n >= 9) {
        if (dpn < v[6].n || dqn < v[7].n || qn < v[8].n) st = kCCBufferTooSmall;
        else { memcpy(dp, v[6].p, v[6].n); memcpy(dq, v[7].p, v[7].n); memcpy(qinv, v[8].p, v[8].n); st = kCCSuccess; }
    }
    if (d) CFRelease(d);
    return st;
}
/* só a chave pública sai de componentes (a privada pediria aritmética de inteiros grandes que o seckey.c não expõe) */
static void der_put_len(u8 *o, size_t *k, size_t len) {
    if (len < 0x80) o[(*k)++] = (u8)len;
    else if (len < 0x100) { o[(*k)++] = 0x81; o[(*k)++] = (u8)len; }
    else { o[(*k)++] = 0x82; o[(*k)++] = (u8)(len >> 8); o[(*k)++] = (u8)len; }
}
int CCRSACryptorCreateFromData(int type, const u8 *mod, size_t ml, const u8 *exp, size_t el, const u8 *p, size_t pl, const u8 *q, size_t ql, id *key) {
    (void)p; (void)pl; (void)q; (void)ql;
    if (!mod || !exp || !key || !ml || !el || ml > 1024 || el > 1024) return kCCParamError;
    if (type != 0) return kCCUnimplemented;
    u8 *o = malloc(ml + el + 32);
    if (!o) return kCCMemoryFailure;
    size_t k = 0, il = ml + (mod[0] & 0x80 ? 1 : 0), jl = el + (exp[0] & 0x80 ? 1 : 0);
    size_t body = 1 + (il < 0x80 ? 1 : il < 0x100 ? 2 : 3) + il + 1 + (jl < 0x80 ? 1 : jl < 0x100 ? 2 : 3) + jl;
    o[k++] = 0x30; der_put_len(o, &k, body);
    o[k++] = 0x02; der_put_len(o, &k, il); if (il > ml) o[k++] = 0; memcpy(o + k, mod, ml); k += ml;
    o[k++] = 0x02; der_put_len(o, &k, jl); if (jl > el) o[k++] = 0; memcpy(o + k, exp, el); k += el;
    id kk = import(kSecAttrKeyTypeRSA, o, k, 0);
    free(o);
    if (!kk) return kCCDecodeError;
    *key = kk;
    return kCCSuccess;
}

/* ================================================================ EC (P-256/384/521) */
int CCECCryptorGeneratePair(size_t bits, id *pub, id *priv) {
    if (!pub || !priv || (bits != 256 && bits != 384 && bits != 521)) return kCCParamError;
    id k = gen(kSecAttrKeyTypeECSECPrimeRandom, bits);
    if (!k) return kCCMemoryFailure;
    *priv = k;
    *pub = SecKeyCopyPublicKey(k);
    return kCCSuccess;
}
void CCECCryptorRelease(id k) { if (k) CFRelease(k); }
/* formato: kCCImportKeyBinary (0) = X9.63 (04||X||Y[||D]); kCCImportKeyDER (1) = SPKI/PKCS#8. tipo: 0 público, 1 privado */
int CCECCryptorImportKey(int fmt, const void *pkg, size_t n, int type, id *key) {
    if (!pkg || !n || !key || (type != 0 && type != 1)) return kCCParamError;
    id k = fmt == 0 ? import(kSecAttrKeyTypeECSECPrimeRandom, pkg, n, type) : fmt == 1 ? cx_seckey_import(pkg, (u32)n, type) : 0;
    if (!k) return kCCDecodeError;
    *key = k;
    return kCCSuccess;
}
int CCECCryptorExportKey(int fmt, void *out, size_t *len, int type, id key) {
    if (!key || !len || (type != 0 && type != 1)) return kCCParamError;
    int priv = key_is_private(key);
    if (type == 1 && priv != 1) return kCCParamError;
    id k = type == 0 && priv == 1 ? SecKeyCopyPublicKey(key) : objc_retain(key);
    int st;
    if (fmt == 0) st = out_bytes(SecKeyCopyExternalRepresentation(k, 0), out, len);
    else if (fmt == 1) {
        u8 *b = 0; u32 m = 0;
        if ((type ? cx_seckey_pkcs8(k, &b, &m) : cx_seckey_spki(k, &b, &m)) || !b) st = kCCDecodeError;
        else { st = out_bytes(cx_data_new(b, m), out, len); memset(b, 0, m); free(b); }
    } else st = kCCParamError;
    CFRelease(k);
    return st;
}
int CCECCryptorComputeSharedSecret(id priv, id pub, void *out, size_t *len) {
    if (!priv || !pub || !len) return kCCParamError;
    id p = dict();
    id s = SecKeyCopyKeyExchangeResult(priv, kSecKeyAlgorithmECDHKeyExchangeStandard, pub, p, 0);
    return out_bytes(s, out, len);
}

/* ================================================================ CCKDFParameters + CCDeriveKey */
typedef struct { u32 magic; int kind; u32 rounds; u8 *salt; size_t saltn; u8 *ctx; size_t ctxn; } kdfp_t;
#define KDF_MAGIC 0x4b444650u
static int kdf_new(kdfp_t **out, int kind, const void *salt, size_t sn, const void *ctx, size_t cn, u32 rounds) {
    if (!out) return kCCParamError;
    kdfp_t *k = calloc(1, sizeof *k);
    if (!k) return kCCMemoryFailure;
    k->magic = KDF_MAGIC; k->kind = kind; k->rounds = rounds;
    if (sn) { k->salt = malloc(sn); memcpy(k->salt, salt, sn); k->saltn = sn; }
    if (cn) { k->ctx = malloc(cn); memcpy(k->ctx, ctx, cn); k->ctxn = cn; }
    *out = k;
    return kCCSuccess;
}
int CCKDFParametersCreateHkdf(kdfp_t **out, const void *salt, size_t sn, const void *info, size_t in) { return kdf_new(out, 1, salt, sn, info, in, 0); }
int CCKDFParametersCreatePbkdf2(kdfp_t **out, u32 rounds, const void *salt, size_t sn) { return rounds ? kdf_new(out, 2, salt, sn, 0, 0, rounds) : kCCParamError; }
void CCKDFParametersDestroy(kdfp_t *k) {
    if (!k || k->magic != KDF_MAGIC) return;
    if (k->salt) { memset(k->salt, 0, k->saltn); free(k->salt); }
    free(k->ctx);
    k->magic = 0;
    free(k);
}
/* digest do CommonDigestSPI -> (algoritmo do CCHmac, tamanho) */
static int hmac_of(u32 dg, u32 *alg, size_t *hl) {
    switch (dg) {
    case kCCDigestSHA1: *alg = 0; *hl = 20; return 0;
    case kCCDigestSHA224: *alg = 5; *hl = 28; return 0;
    case kCCDigestSHA256: *alg = 2; *hl = 32; return 0;
    case kCCDigestSHA384: *alg = 3; *hl = 48; return 0;
    case kCCDigestSHA512: *alg = 4; *hl = 64; return 0;
    }
    return -1;
}
/* RFC 5869: PRK = HMAC(salt, IKM); T(i) = HMAC(PRK, T(i-1) | info | i) */
int CCDeriveKey(const kdfp_t *k, u32 dg, const void *ikm, size_t ikmn, void *out, size_t outn) {
    if (!k || k->magic != KDF_MAGIC || !out || !outn || (!ikm && ikmn)) return kCCParamError;
    u32 alg; size_t hl;
    if (hmac_of(dg, &alg, &hl)) return kCCParamError;
    if (k->kind == 2) {
        u32 prf = alg == 0 ? 1 : alg == 5 ? 2 : alg == 2 ? 3 : alg == 3 ? 4 : 5;   /* kCCPRFHmacAlgSHA1..SHA512 */
        return CCKeyDerivationPBKDF(2, (const char *)ikm, ikmn, k->salt, k->saltn, prf, k->rounds, out, outn) ? kCCParamError : kCCSuccess;
    }
    if (outn > 255 * hl) return kCCParamError;
    u8 zero[64] = {0}, prk[64], t[64];
    CCHmac(alg, k->saltn ? k->salt : zero, k->saltn ? k->saltn : hl, ikm, ikmn, prk);
    size_t bl = hl + k->ctxn + 1;
    u8 *buf = malloc(bl);
    if (!buf) return kCCMemoryFailure;
    size_t done = 0, tl = 0;
    for (u8 i = 1; done < outn; i++) {
        memcpy(buf, t, tl);
        if (k->ctxn) memcpy(buf + tl, k->ctx, k->ctxn);
        buf[tl + k->ctxn] = i;
        CCHmac(alg, prk, hl, buf, tl + k->ctxn + 1, t);
        tl = hl;
        size_t c = outn - done < hl ? outn - done : hl;
        memcpy((u8 *)out + done, t, c);
        done += c;
    }
    memset(prk, 0, sizeof prk); memset(t, 0, sizeof t); memset(buf, 0, bl);
    free(buf);
    return kCCSuccess;
}
