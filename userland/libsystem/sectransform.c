/*
 * mOS libSystem — SecTransform (Security.framework, libsecurity_transform) e chaves simétricas.
 *
 * Transforms: Encode/Decode (base64 com quebra de linha, base32, base32FDE e zlib), Digest (MD2, MD4, MD5, SHA1, SHA2
 * 224/256/384/512 e HMAC-MD5/SHA1/SHA2), Sign/Verify (RSA PKCS#1 v1.5 e ECDSA sobre o SecKey de seckey.c, entrada
 * texto, digest ou bruta), Encrypt/Decrypt (simétrico pelo CCCryptor do CommonCrypto: AES, DES, 3DES, RC2, RC4,
 * Blowfish; modos ECB/CBC/CFB/OFB, PKCS#7 e IV padrão de zeros como o macOS; RSA PKCS#1, OAEP-SHA1 e bruto, em blocos),
 * leitura de CFReadStream, Null e grupos (SecTransformConnectTransforms liga a saída de um à entrada de outro; o
 * Execute do grupo roda a cadeia até o transform cuja saída ninguém consome). ExecuteAsync entrega o resultado e o
 * fim na fila. Representação externa: dicionário com tipo, nome e atributos (as chaves não saem, como no macOS).
 *
 * Chaves simétricas: SecKeyCreateFromData (bytes + kSecAttrKeyType), SecKeyGenerateSymmetric (aleatória) e
 * SecKeyDeriveFromPassword (PBKDF2). São SecKeyRef (mesmo CFTypeID) e seckey.c consulta cx_symkey_* para tamanho do
 * bloco, atributos e representação externa.
 */
#include "foundation.h"
#include "libc.h"
#define Z_SOLO 1
#include "../third_party/zlib/zlib.h"

typedef int OSStatus;
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
u64 CFGetTypeID(id o);
u64 CFDataGetTypeID(void);
u64 CFStringGetTypeID(void);
u64 CFNumberGetTypeID(void);
u8 CFEqual(id a, id b);
id CFErrorCreate(void *alloc, id domain, long code, id info);
id CFRetain(id o);
void CFRelease(id o);
id SecKeyCreateSignature(id key, id alg, id data, id *err);
u8 SecKeyVerifySignature(id key, id alg, id data, id sig, id *err);
id SecKeyCreateEncryptedData(id key, id alg, id data, id *err);
id SecKeyCreateDecryptedData(id key, id alg, id data, id *err);
OSStatus SecKeyRawSign(id key, u32 pad, const u8 *d, size_t dl, u8 *sig, size_t *sl);
OSStatus SecKeyRawVerify(id key, u32 pad, const u8 *d, size_t dl, const u8 *sig, size_t sl);
size_t SecKeyGetBlockSize(id key);
id SecKeyCopyAttributes(id key);
u64 SecKeyGetTypeID(void);
int SecRandomCopyBytes(void *rnd, size_t n, void *out);
int CCCryptorCreateWithMode(u32 op, u32 mode, u32 alg, u32 padding, const void *iv, const void *key, size_t klen,
                            const void *tweak, size_t tlen, int rounds, u32 options, void **ref);
int CCCryptorUpdate(void *ref, const void *in, size_t n, void *out, size_t avail, size_t *moved);
int CCCryptorFinal(void *ref, void *out, size_t avail, size_t *moved);
size_t CCCryptorGetOutputLength(void *ref, size_t n, u8 final);
int CCCryptorRelease(void *ref);
void CCHmac(u32 alg, const void *key, size_t klen, const void *d, size_t n, void *mac);
u8 *CC_MD2(const void *d, u32 n, u8 *md);
u8 *CC_MD4(const void *d, u32 n, u8 *md);
u8 *CC_MD5(const void *d, u32 n, u8 *md);
u8 *CC_SHA1(const void *d, u32 n, u8 *md);
u8 *CC_SHA224(const void *d, u32 n, u8 *md);
u8 *CC_SHA256(const void *d, u32 n, u8 *md);
u8 *CC_SHA384(const void *d, u32 n, u8 *md);
u8 *CC_SHA512(const void *d, u32 n, u8 *md);
int CCKeyDerivationPBKDF(u32 alg, const char *pw, size_t pwlen, const u8 *salt, size_t saltlen, u32 prf, u32 rounds, u8 *dk, size_t dklen);
u8 CFReadStreamOpen(id s);
long CFReadStreamRead(id s, u8 *buf, long n);
void CFReadStreamClose(id s);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void _Block_release(const void *b);
extern void *kCFBooleanTrue, *kCFBooleanFalse;

#define KX(n) extern const void *n;
KX(kSecTransformInputAttributeName) KX(kSecTransformOutputAttributeName) KX(kSecTransformTransformName) KX(kSecTransformErrorDomain)
KX(kSecTransformDebugAttributeName) KX(kSecTransformAbortAttributeName) KX(kSecEncodeTypeAttribute) KX(kSecDecodeTypeAttribute)
KX(kSecBase64Encoding) KX(kSecBase32Encoding) KX(kSecBase32FDEEncoding) KX(kSecZLibEncoding) KX(kSecEncodeLineLengthAttribute)
KX(kSecCompressionRatio) KX(kSecLineLength64) KX(kSecLineLength76) KX(kSecDigestTypeAttribute) KX(kSecDigestLengthAttribute)
KX(kSecDigestHMACKeyAttribute) KX(kSecDigestMD2) KX(kSecDigestMD4) KX(kSecDigestMD5) KX(kSecDigestSHA1) KX(kSecDigestSHA2)
KX(kSecDigestHMACMD5) KX(kSecDigestHMACSHA1) KX(kSecDigestHMACSHA2) KX(kSecKeyAttributeName) KX(kSecSignatureAttributeName)
KX(kSecInputIsAttributeName) KX(kSecInputIsPlainText) KX(kSecInputIsDigest) KX(kSecInputIsRaw) KX(kSecEncryptKey)
KX(kSecPaddingKey) KX(kSecPaddingNoneKey) KX(kSecPaddingPKCS1Key) KX(kSecPaddingPKCS5Key) KX(kSecPaddingPKCS7Key)
KX(kSecPaddingOAEPKey) KX(kSecEncryptionMode) KX(kSecModeNoneKey) KX(kSecModeECBKey) KX(kSecModeCBCKey) KX(kSecModeCFBKey)
KX(kSecModeOFBKey) KX(kSecIVKey) KX(kSecOAEPEncodingParametersAttributeName) KX(kSecOAEPMGF1DigestAlgorithmAttributeName)
KX(kSecOAEPMessageLengthAttributeName) KX(kSecAttrKeyType) KX(kSecAttrKeyTypeAES) KX(kSecAttrKeyTypeDES)
KX(kSecAttrKeyType3DES) KX(kSecAttrKeyTypeRC2) KX(kSecAttrKeyTypeRC4) KX(kSecAttrKeyTypeCAST) KX(kSecAttrKeyTypeRSA)
KX(kSecAttrKeyTypeEC) KX(kSecAttrKeyTypeECSECPrimeRandom) KX(kSecAttrKeySizeInBits) KX(kSecAttrEffectiveKeySize)
KX(kSecAttrKeyClass) KX(kSecAttrKeyClassSymmetric) KX(kSecAttrKeyClassPrivate) KX(kSecAttrSalt) KX(kSecAttrPRF) KX(kSecAttrRounds)
KX(kSecAttrPRFHmacAlgSHA1) KX(kSecAttrPRFHmacAlgSHA224) KX(kSecAttrPRFHmacAlgSHA256) KX(kSecAttrPRFHmacAlgSHA384)
KX(kSecAttrPRFHmacAlgSHA512) KX(kSecAttrLabel) KX(kSecAttrApplicationLabel) KX(kSecClass) KX(kSecClassKey) KX(kSecValueData)
KX(kSecAttrCanEncrypt) KX(kSecAttrCanDecrypt) KX(kSecAttrCanWrap) KX(kSecAttrCanUnwrap) KX(kSecAttrCanSign) KX(kSecAttrCanVerify)
KX(kSecAttrCanDerive) KX(kSecAttrIsPermanent) KX(kSecAttrIsExtractable) KX(kSecAttrIsSensitive)
KX(kSecKeyAlgorithmRSAEncryptionPKCS1) KX(kSecKeyAlgorithmRSAEncryptionOAEPSHA1) KX(kSecKeyAlgorithmRSAEncryptionRaw)
KX(kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw) KX(kSecKeyAlgorithmECDSASignatureDigestX962)
KX(kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1) KX(kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA224)
KX(kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256) KX(kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384)
KX(kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512) KX(kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1)
KX(kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224) KX(kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256)
KX(kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384) KX(kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512)
KX(kSecKeyAlgorithmECDSASignatureMessageX962SHA1) KX(kSecKeyAlgorithmECDSASignatureMessageX962SHA224)
KX(kSecKeyAlgorithmECDSASignatureMessageX962SHA256) KX(kSecKeyAlgorithmECDSASignatureMessageX962SHA384)
KX(kSecKeyAlgorithmECDSASignatureMessageX962SHA512) KX(kSecKeyAlgorithmECDSASignatureDigestX962SHA1)
KX(kSecKeyAlgorithmECDSASignatureDigestX962SHA224) KX(kSecKeyAlgorithmECDSASignatureDigestX962SHA256)
KX(kSecKeyAlgorithmECDSASignatureDigestX962SHA384) KX(kSecKeyAlgorithmECDSASignatureDigestX962SHA512)
#define K_(v) ((id)(v))

/* SecTransform.h */
enum { kSecTransformErrorAttributeNotFound = 1, kSecTransformErrorInvalidOperation = 2, kSecTransformErrorNotInitializedCorrectly = 3,
       kSecTransformErrorMoreThanOneOutput = 4, kSecTransformErrorInvalidInputDictionary = 5, kSecTransformErrorInvalidAlgorithm = 6,
       kSecTransformErrorInvalidLength = 7, kSecTransformErrorInvalidType = 8, kSecTransformErrorInvalidInput = 10,
       kSecTransformOperationNotSupportedOnGroup = 13, kSecTransformErrorMissingParameter = 14, kSecTransformErrorInvalidConnection = 15,
       kSecTransformInvalidArgument = 21 };
enum { errSecSuccess = 0, errSecUnimplemented = -4, errSecParam = -50, errSecAllocate = -108, errSecDecode = -26275,
       errSecUnsupportedKeySize = -67735, errSecInvalidKeyRef = -67712 };

static id cls(const char *n) { return (id)cx_class_named(n); }
static void set_err(id *err, long code, const char *msg) {
    if (!err) return;
    id info = 0;
    if (msg) info = MSG(id, cls("NSDictionary"), "dictionaryWithObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    *err = CFErrorCreate(0, K_(kSecTransformErrorDomain), code, info);
}
static int is_data(id v) { return v && CFGetTypeID(v) == CFDataGetTypeID(); }
static int is_cfstr(id v) { return v && CFGetTypeID(v) == CFStringGetTypeID(); }
static int eq(id a, const void *b) { return a && CFEqual(a, K_(b)); }
static id data_new(const void *b, size_t n) { return MSG(id, cls("NSData"), "dataWithBytes:length:", b, (u64)n); }
static id mdata(void) { return MSG0(id, cls("NSMutableData"), "data"); }
static void dappend(id d, const void *b, size_t n) { MSG(void, d, "appendBytes:length:", b, (u64)n); }
static const u8 *dbytes(id d) { return MSG0(const u8 *, d, "bytes"); }
static size_t dlen(id d) { return (size_t)MSG0(u64, d, "length"); }
static long num(id v) { return v ? MSG0(long, v, "integerValue") : 0; }

/* ================================================================ chaves simétricas */
enum { kCCAlgorithmAES = 0, kCCAlgorithmDES = 1, kCCAlgorithm3DES = 2, kCCAlgorithmCAST = 3, kCCAlgorithmRC4 = 4,
       kCCAlgorithmRC2 = 5, kCCAlgorithmBlowfish = 6 };
typedef struct { int ccalg; const void *const *type; size_t n; u8 b[]; } symkey_t;
#define TID_SEC_KEY 0x5EC03
static cx_class_t Sym_c, Sym_m;
static u64 sym_typeid(id self, SEL c) { (void)self; (void)c; return TID_SEC_KEY; }
static void sym_dealloc(id self, SEL c) {
    symkey_t *k = *(symkey_t **)((u8 *)self + 8);
    if (k) { memset(k->b, 0, k->n); free(k); }
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}
static id sym_desc(id self, SEL c) {
    (void)c;
    symkey_t *k = *(symkey_t **)((u8 *)self + 8);
    char b[96];
    snprintf(b, sizeof b, "<SecKeyRef algorithm id: %s, key type: SymmetricKey, size: %d bits>", cx_cstr(K_(*k->type)), (int)(k->n * 8));
    return cx_str(b);
}
static cx_class_t *sym_class(void) {
    static int done;
    if (!done) {
        cx_define_class(&Sym_c, &Sym_m, "__mOSSecSymmetricKey", cx_class_named("NSObject"), 8);
        cx_add_method(&Sym_c, "_cfTypeID", (void *)sym_typeid);
        cx_add_method(&Sym_c, "dealloc", (void *)sym_dealloc);
        cx_add_method(&Sym_c, "description", (void *)sym_desc);
        cx_add_method(&Sym_c, "debugDescription", (void *)sym_desc);
        done = 1;
    }
    return &Sym_c;
}
static symkey_t *sym_of(id key) { return key && object_getClass(key) == sym_class() ? *(symkey_t **)((u8 *)key + 8) : 0; }
/* tipo da chave → algoritmo do CommonCrypto e tamanhos válidos (bytes) */
static const struct { const void *const *type; int ccalg; int min, max, step, def; } g_sym[] = {
    {&kSecAttrKeyTypeAES, kCCAlgorithmAES, 16, 32, 8, 16},
    {&kSecAttrKeyTypeDES, kCCAlgorithmDES, 8, 8, 1, 8},
    {&kSecAttrKeyType3DES, kCCAlgorithm3DES, 24, 24, 1, 24},
    {&kSecAttrKeyTypeRC2, kCCAlgorithmRC2, 1, 128, 1, 16},
    {&kSecAttrKeyTypeRC4, kCCAlgorithmRC4, 1, 512, 1, 16},
    {&kSecAttrKeyTypeCAST, kCCAlgorithmCAST, 5, 16, 1, 16},
};
static int sym_index(id type) {
    if (!type) return -1;
    for (int i = 0; i < 6; i++) if (CFEqual(type, K_(*g_sym[i].type))) return i;
    long n = num(type);                                                          /* "2147483649" etc. como número */
    if (n) for (int i = 0; i < 6; i++) if (num(K_(*g_sym[i].type)) == n) return i;
    return -1;
}
static id sym_new(int ti, const u8 *b, size_t n, id *err) {
    if (n < (size_t)g_sym[ti].min || n > (size_t)g_sym[ti].max || (n - (size_t)g_sym[ti].min) % (size_t)g_sym[ti].step) {
        if (err) *err = CFErrorCreate(0, cx_str("NSOSStatusErrorDomain"), errSecUnsupportedKeySize, 0);
        return 0;
    }
    symkey_t *k = malloc(sizeof *k + n);
    if (!k) return 0;
    k->ccalg = g_sym[ti].ccalg; k->type = g_sym[ti].type; k->n = n;
    memcpy(k->b, b, n);
    id o = cx_alloc_instance(sym_class());
    *(symkey_t **)((u8 *)o + 8) = k;
    return o;
}
id SecKeyCreateFromData(id params, id keyData, id *err) {
    if (err) *err = 0;
    if (!params || !is_data(keyData)) { if (err) *err = CFErrorCreate(0, cx_str("NSOSStatusErrorDomain"), errSecParam, 0); return 0; }
    int ti = sym_index(MSG(id, params, "objectForKey:", K_(kSecAttrKeyType)));
    if (ti < 0) { if (err) *err = CFErrorCreate(0, cx_str("NSOSStatusErrorDomain"), errSecParam, 0); return 0; }
    long bits = num(MSG(id, params, "objectForKey:", K_(kSecAttrKeySizeInBits)));
    if (bits && (size_t)bits != dlen(keyData) * 8) { if (err) *err = CFErrorCreate(0, cx_str("NSOSStatusErrorDomain"), errSecUnsupportedKeySize, 0); return 0; }
    return sym_new(ti, dbytes(keyData), dlen(keyData), err);
}
id SecKeyGenerateSymmetric(id params, id *err) {
    if (err) *err = 0;
    int ti = params ? sym_index(MSG(id, params, "objectForKey:", K_(kSecAttrKeyType))) : -1;
    if (ti < 0) { if (err) *err = CFErrorCreate(0, cx_str("NSOSStatusErrorDomain"), errSecParam, 0); return 0; }
    long bits = num(MSG(id, params, "objectForKey:", K_(kSecAttrKeySizeInBits)));
    size_t n = bits ? (size_t)(bits + 7) / 8 : (size_t)g_sym[ti].def;
    u8 b[512];
    if (n > sizeof b || SecRandomCopyBytes(0, n, b)) { if (err) *err = CFErrorCreate(0, cx_str("NSOSStatusErrorDomain"), errSecParam, 0); return 0; }
    id k = sym_new(ti, b, n, err);
    memset(b, 0, sizeof b);
    return k;
}
/* PBKDF2: kSecAttrSalt (obrigatório), kSecAttrPRF (padrão SHA1), kSecAttrRounds (padrão 0 = o que o macOS calibra; aqui 100000) */
id SecKeyDeriveFromPassword(id password, id params, id *err) {
    if (err) *err = 0;
    id salt = params ? MSG(id, params, "objectForKey:", K_(kSecAttrSalt)) : 0;
    int ti = params ? sym_index(MSG(id, params, "objectForKey:", K_(kSecAttrKeyType))) : -1;
    if (!password || !cx_is_str(password) || !is_data(salt) || ti < 0) { if (err) *err = CFErrorCreate(0, cx_str("NSOSStatusErrorDomain"), errSecParam, 0); return 0; }
    id prf = MSG(id, params, "objectForKey:", K_(kSecAttrPRF));
    u32 p = 1;                                                                   /* kCCPRFHmacAlgSHA1 */
    if (eq(prf, kSecAttrPRFHmacAlgSHA224)) p = 2;
    else if (eq(prf, kSecAttrPRFHmacAlgSHA256)) p = 3;
    else if (eq(prf, kSecAttrPRFHmacAlgSHA384)) p = 4;
    else if (eq(prf, kSecAttrPRFHmacAlgSHA512)) p = 5;
    long rounds = num(MSG(id, params, "objectForKey:", K_(kSecAttrRounds)));
    long bits = num(MSG(id, params, "objectForKey:", K_(kSecAttrKeySizeInBits)));
    size_t n = bits ? (size_t)(bits + 7) / 8 : (size_t)g_sym[ti].def;
    u8 dk[512];
    const char *pw = cx_cstr(password);
    if (n > sizeof dk || CCKeyDerivationPBKDF(2, pw, strlen(pw), dbytes(salt), dlen(salt), p, rounds > 0 ? (u32)rounds : 100000u, dk, n)) {
        if (err) *err = CFErrorCreate(0, cx_str("NSOSStatusErrorDomain"), errSecParam, 0);
        return 0;
    }
    id k = sym_new(ti, dk, n, err);
    memset(dk, 0, sizeof dk);
    return k;
}
/* para seckey.c */
size_t cx_symkey_block_size(id key) {
    symkey_t *k = sym_of(key);
    if (!k) return 0;
    switch (k->ccalg) { case kCCAlgorithmAES: return 16; case kCCAlgorithmRC4: return 1; default: return 8; }
}
id cx_symkey_data(id key) { symkey_t *k = sym_of(key); return k ? objc_retain(data_new(k->b, k->n)) : 0; }
id cx_symkey_attrs(id key) {
    symkey_t *k = sym_of(key);
    if (!k) return 0;
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    id y = (id)kCFBooleanTrue, no = (id)kCFBooleanFalse;
    MSG(void, d, "setObject:forKey:", K_(kSecClassKey), K_(kSecClass));
    MSG(void, d, "setObject:forKey:", K_(*k->type), K_(kSecAttrKeyType));
    MSG(void, d, "setObject:forKey:", K_(kSecAttrKeyClassSymmetric), K_(kSecAttrKeyClass));
    MSG(void, d, "setObject:forKey:", cx_num_i((i64)k->n * 8), K_(kSecAttrKeySizeInBits));
    MSG(void, d, "setObject:forKey:", cx_num_i((i64)k->n * 8), K_(kSecAttrEffectiveKeySize));
    const void *const *yes_keys[] = {&kSecAttrCanEncrypt, &kSecAttrCanDecrypt, &kSecAttrCanWrap, &kSecAttrCanUnwrap, &kSecAttrIsExtractable};
    const void *const *no_keys[] = {&kSecAttrCanSign, &kSecAttrCanVerify, &kSecAttrCanDerive, &kSecAttrIsPermanent, &kSecAttrIsSensitive};
    for (int i = 0; i < 5; i++) { MSG(void, d, "setObject:forKey:", y, K_(*yes_keys[i])); MSG(void, d, "setObject:forKey:", no, K_(*no_keys[i])); }
    MSG(void, d, "setObject:forKey:", data_new(k->b, k->n), K_(kSecValueData));
    return objc_retain(d);
}

/* ================================================================ objeto transform */
enum { T_ENC = 1, T_DEC, T_DIGEST, T_SIGN, T_VERIFY, T_ENCRYPT, T_DECRYPT, T_READ, T_NULL, T_GROUP };
static const char *const g_kind_name[] = {"", "Encoder", "Decoder", "Digest", "Sign", "Verify", "Encrypt", "Decrypt", "ReadStream", "Null", "Group"};
typedef struct { int kind; id attrs, links, members; } tr_t;                   /* links: atributo destino → [origem, atributo] */
static cx_class_t Tr_c, Tr_m;
#define TID_TRANSFORM 0x5EC20
static u64 tr_typeid(id self, SEL c) { (void)c; tr_t *t = *(tr_t **)((u8 *)self + 8); return TID_TRANSFORM + (u64)(t ? t->kind : 0); }
static void tr_dealloc(id self, SEL c) {
    tr_t *t = *(tr_t **)((u8 *)self + 8);
    if (t) { objc_release(t->attrs); objc_release(t->links); if (t->members) objc_release(t->members); free(t); }
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}
static id tr_desc(id self, SEL c) {
    (void)c;
    tr_t *t = *(tr_t **)((u8 *)self + 8);
    char b[160];
    id nm = t ? MSG(id, t->attrs, "objectForKey:", K_(kSecTransformTransformName)) : 0;
    snprintf(b, sizeof b, "<SecTransform %s: %s>", t ? g_kind_name[t->kind] : "?", nm ? cx_cstr(nm) : "");
    return cx_str(b);
}
static cx_class_t *tr_class(void) {
    static int done;
    if (!done) {
        cx_define_class(&Tr_c, &Tr_m, "__mOSSecTransform", cx_class_named("NSObject"), 8);
        cx_add_method(&Tr_c, "_cfTypeID", (void *)tr_typeid);
        cx_add_method(&Tr_c, "dealloc", (void *)tr_dealloc);
        cx_add_method(&Tr_c, "description", (void *)tr_desc);
        cx_add_method(&Tr_c, "debugDescription", (void *)tr_desc);
        done = 1;
    }
    return &Tr_c;
}
static tr_t *tr_of(id o) { return o && object_getClass(o) == tr_class() ? *(tr_t **)((u8 *)o + 8) : 0; }
static id tr_new(int kind) {
    static unsigned seq;
    tr_t *t = calloc(1, sizeof *t);
    if (!t) return 0;
    t->kind = kind;
    t->attrs = objc_retain(MSG0(id, cls("NSMutableDictionary"), "dictionary"));
    t->links = objc_retain(MSG0(id, cls("NSMutableDictionary"), "dictionary"));
    if (kind == T_GROUP) t->members = objc_retain(MSG0(id, cls("NSMutableArray"), "array"));
    char nm[64];
    snprintf(nm, sizeof nm, "com.apple.security.%s.%u", g_kind_name[kind], ++seq);
    MSG(void, t->attrs, "setObject:forKey:", cx_str(nm), K_(kSecTransformTransformName));
    id o = cx_alloc_instance(tr_class());
    *(tr_t **)((u8 *)o + 8) = t;
    return o;
}
static void put_attr(id o, const void *k, id v) { tr_t *t = tr_of(o); if (t && v) MSG(void, t->attrs, "setObject:forKey:", v, K_(k)); }
static id attr(tr_t *t, id run, const void *k) {                                /* run: valores vindos das ligações nesta execução */
    id v = run ? MSG(id, run, "objectForKey:", K_(k)) : 0;
    return v ? v : MSG(id, t->attrs, "objectForKey:", K_(k));
}
u64 SecTransformGetTypeID(void) { return TID_TRANSFORM + T_NULL; }
u64 SecGroupTransformGetTypeID(void) { return TID_TRANSFORM + T_GROUP; }
u64 SecDigestTransformGetTypeID(void) { return TID_TRANSFORM + T_DIGEST; }
u64 SecEncryptTransformGetTypeID(void) { return TID_TRANSFORM + T_ENCRYPT; }
u64 SecDecryptTransformGetTypeID(void) { return TID_TRANSFORM + T_DECRYPT; }
u64 SecSignTransformGetTypeID(void) { return TID_TRANSFORM + T_SIGN; }
u64 SecVerifyTransformGetTypeID(void) { return TID_TRANSFORM + T_VERIFY; }
u64 SecEncodeTransformGetTypeID(void) { return TID_TRANSFORM + T_ENC; }
u64 SecDecodeTransformGetTypeID(void) { return TID_TRANSFORM + T_DEC; }

/* ---- criação */
static int enc_type_ok(id type) {
    return eq(type, kSecBase64Encoding) || eq(type, kSecBase32Encoding) || eq(type, kSecBase32FDEEncoding) || eq(type, kSecZLibEncoding);
}
id SecEncodeTransformCreate(id type, id *err) {
    if (err) *err = 0;
    if (!enc_type_ok(type)) { set_err(err, kSecTransformErrorInvalidInput, "Unsupported encode type"); return 0; }
    id o = tr_new(T_ENC);
    put_attr(o, kSecEncodeTypeAttribute, type);
    return o;
}
id SecDecodeTransformCreate(id type, id *err) {
    if (err) *err = 0;
    if (!enc_type_ok(type)) { set_err(err, kSecTransformErrorInvalidInput, "Unsupported decode type"); return 0; }
    id o = tr_new(T_DEC);
    put_attr(o, kSecDecodeTypeAttribute, type);
    return o;
}
/* tamanho do digest; 0 = inválido */
static int digest_len(id type, long len) {
    if (eq(type, kSecDigestMD2) || eq(type, kSecDigestMD4) || eq(type, kSecDigestMD5) || eq(type, kSecDigestHMACMD5)) return !len || len == 128 ? 16 : 0;
    if (eq(type, kSecDigestSHA1) || eq(type, kSecDigestHMACSHA1)) return !len || len == 160 ? 20 : 0;
    if (eq(type, kSecDigestSHA2) || eq(type, kSecDigestHMACSHA2)) {
        if (!len) return 64;
        return len == 224 || len == 256 || len == 384 || len == 512 ? (int)(len / 8) : 0;
    }
    return 0;
}
id SecDigestTransformCreate(id type, long len, id *err) {
    if (err) *err = 0;
    if (!type) type = K_(kSecDigestSHA2);                                        /* NULL = SHA-2 de 512 bits, como o macOS */
    if (!digest_len(type, len)) {
        int known = eq(type, kSecDigestMD2) || eq(type, kSecDigestMD4) || eq(type, kSecDigestMD5) || eq(type, kSecDigestSHA1) ||
                    eq(type, kSecDigestSHA2) || eq(type, kSecDigestHMACMD5) || eq(type, kSecDigestHMACSHA1) || eq(type, kSecDigestHMACSHA2);
        set_err(err, known ? kSecTransformErrorInvalidLength : kSecTransformErrorInvalidAlgorithm, known ? "Invalid digest length" : "Invalid digest type");
        return 0;
    }
    id o = tr_new(T_DIGEST);
    put_attr(o, kSecDigestTypeAttribute, type);
    put_attr(o, kSecDigestLengthAttribute, cx_num_i(len));
    return o;
}
static id sign_like(int kind, id key, id sig, id *err) {
    if (err) *err = 0;
    if (!key || CFGetTypeID(key) != SecKeyGetTypeID() || sym_of(key)) { set_err(err, kSecTransformInvalidArgument, "Expected an asymmetric SecKeyRef"); return 0; }
    id o = tr_new(kind);
    put_attr(o, kSecKeyAttributeName, key);
    put_attr(o, kSecDigestTypeAttribute, K_(kSecDigestSHA1));
    put_attr(o, kSecDigestLengthAttribute, cx_num_i(0));
    put_attr(o, kSecInputIsAttributeName, K_(kSecInputIsPlainText));
    if (sig) put_attr(o, kSecSignatureAttributeName, sig);
    return o;
}
id SecSignTransformCreate(id key, id *err) { return sign_like(T_SIGN, key, 0, err); }
id SecVerifyTransformCreate(id key, id sig, id *err) { return sign_like(T_VERIFY, key, sig, err); }
static id crypt_like(int kind, id key, id *err) {
    if (err) *err = 0;
    if (!key || CFGetTypeID(key) != SecKeyGetTypeID()) { set_err(err, kSecTransformInvalidArgument, "Expected a SecKeyRef"); return 0; }
    id o = tr_new(kind);
    put_attr(o, kSecEncryptKey, key);
    return o;
}
id SecEncryptTransformCreate(id key, id *err) { return crypt_like(T_ENCRYPT, key, err); }
id SecDecryptTransformCreate(id key, id *err) { return crypt_like(T_DECRYPT, key, err); }
id SecTransformCreateReadTransformWithReadStream(id stream) {
    if (!stream) return 0;
    id o = tr_new(T_READ);
    put_attr(o, kSecTransformInputAttributeName, stream);
    return o;
}
id SecNullTransformCreate(void) { return tr_new(T_NULL); }
id SecTransformCreateGroupTransform(void) { return tr_new(T_GROUP); }

/* ---- atributos e ligações */
u8 SecTransformSetAttribute(id o, id key, id value, id *err) {
    if (err) *err = 0;
    tr_t *t = tr_of(o);
    if (!t || !key) { set_err(err, kSecTransformInvalidArgument, "Invalid transform or attribute"); return 0; }
    if (t->kind == T_GROUP) { set_err(err, kSecTransformOperationNotSupportedOnGroup, "Can't set attributes on a group"); return 0; }
    if (value) MSG(void, t->attrs, "setObject:forKey:", value, key);
    else MSG(void, t->attrs, "removeObjectForKey:", key);
    return 1;
}
id SecTransformGetAttribute(id o, id key) { tr_t *t = tr_of(o); return t && key ? MSG(id, t->attrs, "objectForKey:", key) : 0; }
static void add_member(tr_t *g, id o) { if (!MSG(BOOL, g->members, "containsObject:", o)) MSG(void, g->members, "addObject:", o); }
id SecTransformConnectTransforms(id src, id srcAttr, id dst, id dstAttr, id group, id *err) {
    if (err) *err = 0;
    tr_t *g = tr_of(group), *s = tr_of(src), *d = tr_of(dst);
    if (!g || g->kind != T_GROUP) { set_err(err, kSecTransformInvalidArgument, "A group transform is required"); return 0; }
    if (!s || !d || !srcAttr || !dstAttr || src == dst || s->kind == T_GROUP || d->kind == T_GROUP) {
        set_err(err, kSecTransformErrorInvalidConnection, "Invalid connection");
        return 0;
    }
    id objs[2] = {src, srcAttr};
    id pair = MSG(id, cls("NSArray"), "arrayWithObjects:count:", (id *)objs, (u64)2);
    MSG(void, d->links, "setObject:forKey:", pair, dstAttr);
    add_member(g, src);
    add_member(g, dst);
    return group;
}
id SecTransformFindByName(id group, id name) {
    tr_t *g = tr_of(group);
    if (!g || g->kind != T_GROUP || !name) return 0;
    u64 n = MSG0(u64, g->members, "count");
    for (u64 i = 0; i < n; i++) {
        id m = MSG(id, g->members, "objectAtIndex:", i);
        id nm = MSG(id, tr_of(m)->attrs, "objectForKey:", K_(kSecTransformTransformName));
        if (nm && CFEqual(nm, name)) return m;
    }
    return 0;
}

/* ================================================================ operações */
static const char b64a[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char b32a[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
static const char b32fde[] = "ABCDEFGH8JKLMNOPQR9TUVWXYZ234567";
static id radix_encode(const u8 *in, size_t n, const char *alfa, int w, int grupo, long line) {
    id out = mdata();
    unsigned long acc = 0;
    int nb = 0;
    long col = 0;
    size_t emit = 0;
    char c;
#define EMIT(ch) do { c = (ch); if (line > 0 && col >= line) { dappend(out, "\n", 1); col = 0; } dappend(out, &c, 1); col++; emit++; } while (0)
    for (size_t i = 0; i < n; i++) {
        acc = acc << 8 | in[i];
        nb += 8;
        while (nb >= w) { EMIT(alfa[(acc >> (nb - w)) & ((1u << w) - 1)]); nb -= w; }
    }
    if (nb) EMIT(alfa[(acc << (w - nb)) & ((1u << w) - 1)]);
    while (emit % (size_t)grupo) EMIT('=');
#undef EMIT
    return out;
}
static id radix_decode(const u8 *in, size_t n, const char *alfa, int w) {
    id out = mdata();
    unsigned long acc = 0;
    int nb = 0;
    for (size_t i = 0; i < n; i++) {
        int c = in[i];
        if (c == '=') break;
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        int v = -1;
        for (int j = 0; alfa[j]; j++) if (alfa[j] == c) { v = j; break; }
        if (v < 0 && w == 5 && c >= 'a' && c <= 'z') for (int j = 0; alfa[j]; j++) if (alfa[j] == c - 32) { v = j; break; }
        if (v < 0) return 0;
        acc = acc << w | (unsigned)v;
        nb += w;
        if (nb >= 8) { u8 b = (u8)(acc >> (nb - 8)); dappend(out, &b, 1); nb -= 8; }
    }
    return out;
}
static void *z_alloc(void *q, unsigned a, unsigned b) { (void)q; return calloc(a, b); }
static void z_free(void *q, void *p) { (void)q; free(p); }
/* zlib com cabeçalho (deflateInit/inflateInit), como o kSecZLibEncoding do macOS */
static id zlib_run(const u8 *in, size_t n, int encode, double *ratio) {
    z_stream z;
    memset(&z, 0, sizeof z);
    z.zalloc = z_alloc; z.zfree = z_free;
    if ((encode ? deflateInit(&z, Z_DEFAULT_COMPRESSION) : inflateInit(&z)) != Z_OK) return 0;
    id out = mdata();
    u8 buf[16384];
    z.next_in = (u8 *)in; z.avail_in = (unsigned)n;
    int r;
    do {
        z.next_out = buf; z.avail_out = sizeof buf;
        r = encode ? deflate(&z, Z_FINISH) : inflate(&z, Z_NO_FLUSH);
        if (r != Z_OK && r != Z_STREAM_END && r != Z_BUF_ERROR) break;
        dappend(out, buf, sizeof buf - z.avail_out);
        if (r == Z_BUF_ERROR && z.avail_in == 0 && !encode) break;
    } while (r != Z_STREAM_END);
    if (ratio) *ratio = z.total_in ? (double)z.total_out / (double)z.total_in : 0;
    encode ? deflateEnd(&z) : inflateEnd(&z);
    return r == Z_STREAM_END ? out : 0;
}
static long line_len(id v) {
    if (!v) return 0;
    if (eq(v, kSecLineLength64)) return 64;
    if (eq(v, kSecLineLength76)) return 76;
    return num(v);
}
static id op_code(tr_t *t, id run, id in, int encode, id *err) {
    id type = attr(t, run, encode ? kSecEncodeTypeAttribute : kSecDecodeTypeAttribute);
    const u8 *b = dbytes(in);
    size_t n = dlen(in);
    id r = 0;
    if (eq(type, kSecZLibEncoding)) {
        double ratio = 0;
        r = zlib_run(b, n, encode, &ratio);
        if (r) MSG(void, t->attrs, "setObject:forKey:", cx_num_d(ratio), K_(kSecCompressionRatio));
    } else if (eq(type, kSecBase64Encoding)) {
        if (encode) {
            long l = line_len(attr(t, run, kSecEncodeLineLengthAttribute));
            r = radix_encode(b, n, b64a, 6, 4, l > 0 ? l - l % 4 : 0);
        } else r = radix_decode(b, n, b64a, 6);
    } else {
        const char *alfa = eq(type, kSecBase32FDEEncoding) ? b32fde : b32a;
        if (encode) {
            long l = line_len(attr(t, run, kSecEncodeLineLengthAttribute));
            r = radix_encode(b, n, alfa, 5, 8, l > 0 ? l - l % 8 : 0);
        } else r = radix_decode(b, n, alfa, 5);
    }
    if (!r) set_err(err, kSecTransformErrorInvalidInput, encode ? "Encoding failed" : "Invalid encoded data");
    return r;
}
enum { kCCHmacAlgSHA1, kCCHmacAlgMD5, kCCHmacAlgSHA256, kCCHmacAlgSHA384, kCCHmacAlgSHA512, kCCHmacAlgSHA224 };
static id op_digest(tr_t *t, id run, id in, id *err) {
    id type = attr(t, run, kSecDigestTypeAttribute);
    long len = num(attr(t, run, kSecDigestLengthAttribute));
    int L = digest_len(type, len);
    if (!L) { set_err(err, kSecTransformErrorInvalidLength, "Invalid digest length"); return 0; }
    const u8 *b = dbytes(in);
    u32 n = (u32)dlen(in);
    u8 md[64];
    int hmac = eq(type, kSecDigestHMACMD5) || eq(type, kSecDigestHMACSHA1) || eq(type, kSecDigestHMACSHA2);
    if (hmac) {
        id key = attr(t, run, kSecDigestHMACKeyAttribute);
        if (!is_data(key)) { set_err(err, kSecTransformErrorMissingParameter, "HMAC key not set"); return 0; }
        u32 alg = eq(type, kSecDigestHMACMD5) ? kCCHmacAlgMD5 : eq(type, kSecDigestHMACSHA1) ? kCCHmacAlgSHA1
                : L == 28 ? kCCHmacAlgSHA224 : L == 32 ? kCCHmacAlgSHA256 : L == 48 ? kCCHmacAlgSHA384 : kCCHmacAlgSHA512;
        CCHmac(alg, dbytes(key), dlen(key), b, n, md);
    } else if (eq(type, kSecDigestMD2)) CC_MD2(b, n, md);
    else if (eq(type, kSecDigestMD4)) CC_MD4(b, n, md);
    else if (eq(type, kSecDigestMD5)) CC_MD5(b, n, md);
    else if (eq(type, kSecDigestSHA1)) CC_SHA1(b, n, md);
    else if (L == 28) CC_SHA224(b, n, md);
    else if (L == 32) CC_SHA256(b, n, md);
    else if (L == 48) CC_SHA384(b, n, md);
    else CC_SHA512(b, n, md);
    return data_new(md, (size_t)L);
}
/* algoritmo do SecKey para assinar/verificar; *md5 = 1 quando é RSA com MD5 (sai pelo SecKeyRawSign com DigestInfo) */
static id sig_alg(id key, id dtype, long dlen_bits, id input_is, int *md5, id *err) {
    *md5 = 0;
    id ka = SecKeyCopyAttributes(key);
    id kt = ka ? MSG(id, ka, "objectForKey:", K_(kSecAttrKeyType)) : 0;
    int rsa = eq(kt, kSecAttrKeyTypeRSA);
    if (ka) objc_release(ka);
    int raw = eq(input_is, kSecInputIsRaw), dig = eq(input_is, kSecInputIsDigest);
    if (raw) return K_(rsa ? kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw : kSecKeyAlgorithmECDSASignatureDigestX962);
    int L = 0;
    if (eq(dtype, kSecDigestSHA1)) L = (!dlen_bits || dlen_bits == 160) ? 20 : 0;
    else if (eq(dtype, kSecDigestSHA2)) L = !dlen_bits ? 64 : (dlen_bits == 224 || dlen_bits == 256 || dlen_bits == 384 || dlen_bits == 512) ? (int)(dlen_bits / 8) : 0;
    else if (eq(dtype, kSecDigestMD5) && rsa && (!dlen_bits || dlen_bits == 128)) { *md5 = 1; return K_(kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw); }
    if (!L) {
        set_err(err, kSecTransformErrorInvalidAlgorithm, rsa ? "Invalid digest algorithm for RSA signature, choose one of: SHA1, SHA2 (512bits, 384bits, 256bits, or 224 bits), or MD5"
                                                             : "Invalid digest algorithm for ECDSA signature, choose one of: SHA1, or SHA2 (512bits, 384bits, 256bits, or 224 bits)");
        return 0;
    }
    int i = L == 20 ? 0 : L == 28 ? 1 : L == 32 ? 2 : L == 48 ? 3 : 4;
    static const void *const *rm[] = {&kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1, &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA224,
                                      &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256, &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384,
                                      &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512};
    static const void *const *rd[] = {&kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1, &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224,
                                      &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256, &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384,
                                      &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512};
    static const void *const *em[] = {&kSecKeyAlgorithmECDSASignatureMessageX962SHA1, &kSecKeyAlgorithmECDSASignatureMessageX962SHA224,
                                      &kSecKeyAlgorithmECDSASignatureMessageX962SHA256, &kSecKeyAlgorithmECDSASignatureMessageX962SHA384,
                                      &kSecKeyAlgorithmECDSASignatureMessageX962SHA512};
    static const void *const *ed[] = {&kSecKeyAlgorithmECDSASignatureDigestX962SHA1, &kSecKeyAlgorithmECDSASignatureDigestX962SHA224,
                                      &kSecKeyAlgorithmECDSASignatureDigestX962SHA256, &kSecKeyAlgorithmECDSASignatureDigestX962SHA384,
                                      &kSecKeyAlgorithmECDSASignatureDigestX962SHA512};
    return K_(*(rsa ? (dig ? rd : rm) : (dig ? ed : em))[i]);
}
enum { kSecPaddingPKCS1MD5 = 0x8001 };
static id op_sign(tr_t *t, id run, id in, int verify, id *err) {
    id key = attr(t, run, kSecKeyAttributeName);
    int md5;
    id alg = sig_alg(key, attr(t, run, kSecDigestTypeAttribute), num(attr(t, run, kSecDigestLengthAttribute)), attr(t, run, kSecInputIsAttributeName), &md5, err);
    if (!alg) return 0;
    id e = 0;
    if (md5) {                                                                   /* RSA + MD5: DigestInfo pelo SecKeyRaw* */
        u8 md[16];
        const u8 *b = dbytes(in);
        size_t n = dlen(in);
        if (!eq(attr(t, run, kSecInputIsAttributeName), kSecInputIsDigest)) { CC_MD5(b, (u32)n, md); b = md; n = 16; }
        if (verify) {
            id sig = attr(t, run, kSecSignatureAttributeName);
            if (!is_data(sig)) { set_err(err, kSecTransformErrorMissingParameter, "Signature not set"); return 0; }
            return (id)(SecKeyRawVerify(key, kSecPaddingPKCS1MD5, b, n, dbytes(sig), dlen(sig)) == 0 ? kCFBooleanTrue : kCFBooleanFalse);
        }
        size_t sl = SecKeyGetBlockSize(key);
        u8 *s = malloc(sl ? sl : 1);
        OSStatus st = SecKeyRawSign(key, kSecPaddingPKCS1MD5, b, n, s, &sl);
        id r = st ? 0 : data_new(s, sl);
        free(s);
        if (!r) set_err(err, kSecTransformErrorInvalidInput, "Signing failed");
        return r;
    }
    if (verify) {
        id sig = attr(t, run, kSecSignatureAttributeName);
        if (!is_data(sig)) { set_err(err, kSecTransformErrorMissingParameter, "Signature not set"); return 0; }
        u8 ok = SecKeyVerifySignature(key, alg, in, sig, &e);
        if (e) CFRelease(e);
        return (id)(ok ? kCFBooleanTrue : kCFBooleanFalse);
    }
    id r = SecKeyCreateSignature(key, alg, in, &e);
    if (!r) { if (err) *err = e; else if (e) CFRelease(e); return 0; }
    return objc_autorelease(r);
}
enum { kCCModeECB = 1, kCCModeCBC = 2, kCCModeCFB = 3, kCCModeOFB = 7, kCCModeRC4 = 9 };
static id op_crypt(tr_t *t, id run, id in, int decrypt, id *err) {
    id key = attr(t, run, kSecEncryptKey);
    id pad = attr(t, run, kSecPaddingKey), mode = attr(t, run, kSecEncryptionMode), iv = attr(t, run, kSecIVKey);
    symkey_t *sk = sym_of(key);
    if (sk) {
        int p = !pad || eq(pad, kSecPaddingPKCS7Key) || eq(pad, kSecPaddingPKCS5Key);
        if (pad && !p && !eq(pad, kSecPaddingNoneKey)) { set_err(err, kSecTransformErrorInvalidInput, "Unsupported padding for a symmetric key"); return 0; }
        u32 m = kCCModeCBC;
        if (sk->ccalg == kCCAlgorithmRC4) { m = kCCModeRC4; p = 0; }
        else if (eq(mode, kSecModeNoneKey) || eq(mode, kSecModeECBKey)) m = kCCModeECB;
        else if (eq(mode, kSecModeCFBKey)) { m = kCCModeCFB; p = 0; }
        else if (eq(mode, kSecModeOFBKey)) { m = kCCModeOFB; p = 0; }
        else if (mode && !eq(mode, kSecModeCBCKey)) { set_err(err, kSecTransformErrorInvalidInput, "Unsupported encryption mode"); return 0; }
        size_t bs = cx_symkey_block_size(key);
        u8 zero[16] = {0};
        const void *ivp = zero;
        if (is_data(iv)) {
            if (m != kCCModeECB && m != kCCModeRC4 && dlen(iv) < bs) { set_err(err, kSecTransformErrorInvalidLength, "IV is shorter than the block size"); return 0; }
            ivp = dbytes(iv);
        }
        void *ref = 0;
        int r = CCCryptorCreateWithMode(decrypt, m, (u32)sk->ccalg, p ? 1u : 0u, ivp, sk->b, sk->n, 0, 0, 0, 0, &ref);
        if (r) { set_err(err, r == -4305 ? kSecTransformErrorInvalidAlgorithm : kSecTransformErrorInvalidInput, "Cipher setup failed"); return 0; }
        size_t n = dlen(in), cap = CCCryptorGetOutputLength(ref, n, 1) + bs;
        u8 *o = malloc(cap ? cap : 1);
        size_t a = 0, b = 0;
        r = o ? CCCryptorUpdate(ref, dbytes(in), n, o, cap, &a) : -4302;
        if (!r) r = CCCryptorFinal(ref, o + a, cap - a, &b);
        CCCryptorRelease(ref);
        id res = r ? 0 : data_new(o, a + b);
        if (o) { memset(o, 0, cap); free(o); }
        if (!res) set_err(err, kSecTransformErrorInvalidInput, r == -4304 ? "Decryption failed: bad padding" : r == -4303 ? "Input is not a multiple of the block size" : "Cipher failed");
        return res;
    }
    /* RSA: PKCS#1 (padrão), OAEP-SHA1 ou bruto; dados maiores que um bloco vão em pedaços */
    id ka = SecKeyCopyAttributes(key);
    int rsa = ka && eq(MSG(id, ka, "objectForKey:", K_(kSecAttrKeyType)), kSecAttrKeyTypeRSA);
    if (ka) objc_release(ka);
    if (!rsa) { set_err(err, kSecTransformErrorInvalidAlgorithm, "Only RSA and symmetric keys can encrypt"); return 0; }
    id alg = K_(kSecKeyAlgorithmRSAEncryptionPKCS1);
    size_t k = SecKeyGetBlockSize(key), room = k > 11 ? k - 11 : 0;
    if (eq(pad, kSecPaddingOAEPKey)) {
        id label = attr(t, run, kSecOAEPEncodingParametersAttributeName), mgf = attr(t, run, kSecOAEPMGF1DigestAlgorithmAttributeName);
        if ((is_data(label) && dlen(label)) || (mgf && !eq(mgf, kSecDigestSHA1))) { set_err(err, kSecTransformErrorInvalidAlgorithm, "Only OAEP with SHA-1 and an empty label is supported"); return 0; }
        alg = K_(kSecKeyAlgorithmRSAEncryptionOAEPSHA1);
        room = k > 42 ? k - 42 : 0;
    } else if (eq(pad, kSecPaddingNoneKey)) { alg = K_(kSecKeyAlgorithmRSAEncryptionRaw); room = k; }
    else if (pad && !eq(pad, kSecPaddingPKCS1Key)) { set_err(err, kSecTransformErrorInvalidInput, "Unsupported padding for an RSA key"); return 0; }
    size_t step = decrypt ? k : room;
    if (!step) { set_err(err, kSecTransformErrorInvalidLength, "Key too small"); return 0; }
    const u8 *b = dbytes(in);
    size_t n = dlen(in);
    if (decrypt && n % k) { set_err(err, kSecTransformErrorInvalidLength, "Ciphertext is not a multiple of the key size"); return 0; }
    id out = mdata();
    for (size_t off = 0; off < n || (!n && !off); off += step) {
        size_t c = n - off < step ? n - off : step;
        id e = 0, chunk = data_new(b + off, c);
        id r = decrypt ? SecKeyCreateDecryptedData(key, alg, chunk, &e) : SecKeyCreateEncryptedData(key, alg, chunk, &e);
        if (!r) { if (err) *err = e; else if (e) CFRelease(e); return 0; }
        dappend(out, dbytes(r), dlen(r));
        objc_release(r);
        if (!n) break;
    }
    return out;
}
static id op_read(tr_t *t, id *err) {
    id s = MSG(id, t->attrs, "objectForKey:", K_(kSecTransformInputAttributeName));
    if (!s || !CFReadStreamOpen(s)) { set_err(err, kSecTransformErrorInvalidInput, "Could not open the read stream"); return 0; }
    id out = mdata();
    u8 buf[8192];
    long r;
    while ((r = CFReadStreamRead(s, buf, sizeof buf)) > 0) dappend(out, buf, (size_t)r);
    CFReadStreamClose(s);
    if (r < 0) { set_err(err, kSecTransformErrorInvalidInput, "Read stream error"); return 0; }
    return out;
}

/* ---- execução: resolve as ligações (recursivo) e roda o transform */
static id run_one(id o, id *err, int depth);
static id resolve_links(tr_t *t, id *err, int depth) {
    id run = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    id keys = MSG0(id, t->links, "allKeys");
    u64 n = MSG0(u64, keys, "count");
    for (u64 i = 0; i < n; i++) {
        id dst = MSG(id, keys, "objectAtIndex:", i);
        id pair = MSG(id, t->links, "objectForKey:", dst);
        id src = MSG(id, pair, "objectAtIndex:", (u64)0), sattr = MSG(id, pair, "objectAtIndex:", (u64)1);
        id v = run_one(src, err, depth + 1);
        if (!v) return 0;
        if (!CFEqual(sattr, K_(kSecTransformOutputAttributeName))) {
            v = MSG(id, tr_of(src)->attrs, "objectForKey:", sattr);
            if (!v) { set_err(err, kSecTransformErrorAttributeNotFound, "Connected attribute has no value"); return 0; }
        }
        MSG(void, run, "setObject:forKey:", v, dst);
    }
    return run;
}
static id run_one(id o, id *err, int depth) {
    tr_t *t = tr_of(o);
    if (!t) { set_err(err, kSecTransformInvalidArgument, "Not a transform"); return 0; }
    if (depth > 64) { set_err(err, kSecTransformErrorInvalidConnection, "Transform cycle"); return 0; }
    if (t->kind == T_READ) return op_read(t, err);
    id run = resolve_links(t, err, depth);
    if (!run) return 0;
    id in = attr(t, run, kSecTransformInputAttributeName);
    if (t->kind == T_NULL) {
        if (!in) set_err(err, kSecTransformErrorMissingParameter, "INPUT not set");
        return in;
    }
    if (!in || !is_data(in)) {
        if (in && is_cfstr(in) && (t->kind == T_ENC || t->kind == T_DIGEST || t->kind == T_SIGN || t->kind == T_VERIFY || t->kind == T_ENCRYPT))
            in = MSG(id, in, "dataUsingEncoding:", (u64)4);                       /* texto entra como UTF-8 */
        else { set_err(err, kSecTransformErrorMissingParameter, "INPUT must be a CFData"); return 0; }
    }
    switch (t->kind) {
    case T_ENC: return op_code(t, run, in, 1, err);
    case T_DEC: return op_code(t, run, in, 0, err);
    case T_DIGEST: return op_digest(t, run, in, err);
    case T_SIGN: return op_sign(t, run, in, 0, err);
    case T_VERIFY: return op_sign(t, run, in, 1, err);
    case T_ENCRYPT: return op_crypt(t, run, in, 0, err);
    case T_DECRYPT: return op_crypt(t, run, in, 1, err);
    }
    set_err(err, kSecTransformErrorInvalidOperation, "Unknown transform");
    return 0;
}
/* no grupo, o terminal é o membro cuja saída ninguém consome */
static id group_terminal(tr_t *g, id *err) {
    u64 n = MSG0(u64, g->members, "count");
    id found = 0;
    int count = 0;
    for (u64 i = 0; i < n; i++) {
        id m = MSG(id, g->members, "objectAtIndex:", i);
        int used = 0;
        for (u64 j = 0; j < n && !used; j++) {
            tr_t *o = tr_of(MSG(id, g->members, "objectAtIndex:", j));
            id vals = MSG0(id, o->links, "allValues");
            for (u64 k = 0; k < MSG0(u64, vals, "count"); k++) {
                id pair = MSG(id, vals, "objectAtIndex:", k);
                if (MSG(id, pair, "objectAtIndex:", (u64)0) == m && CFEqual(MSG(id, pair, "objectAtIndex:", (u64)1), K_(kSecTransformOutputAttributeName))) { used = 1; break; }
            }
        }
        if (!used) { found = m; count++; }
    }
    if (count != 1) { set_err(err, count ? kSecTransformErrorMoreThanOneOutput : kSecTransformErrorInvalidConnection, count ? "More than one output" : "Group has no output"); return 0; }
    return found;
}
id SecTransformExecute(id o, id *err) {
    if (err) *err = 0;
    tr_t *t = tr_of(o);
    if (!t) { set_err(err, kSecTransformInvalidArgument, "Not a transform"); return 0; }
    if (t->kind == T_GROUP) {
        id term = group_terminal(t, err);
        if (!term) return 0;
        o = term;
    }
    id r = run_one(o, err, 0);
    return r ? objc_retain(r) : 0;
}
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, id, id, u8); } blk_t;
typedef struct { id t; blk_t *blk; } async_t;
static void async_run(void *ctx) {
    async_t *a = ctx;
    id e = 0, r = SecTransformExecute(a->t, &e);
    a->blk->invoke(a->blk, r, e, 0);
    a->blk->invoke(a->blk, 0, 0, 1);
    if (r) objc_release(r);
    if (e) CFRelease(e);
    objc_release(a->t);
    _Block_release(a->blk);
    free(a);
}
void SecTransformExecuteAsync(id o, void *queue, void *block) {
    if (!o || !queue || !block) return;
    async_t *a = malloc(sizeof *a);
    if (!a) return;
    a->t = objc_retain(o);
    a->blk = _Block_copy(block);
    dispatch_async_f(queue, a, async_run);
}

/* ---- representação externa: {TYPE, ATTRS} ou, para o grupo, {TRANSFORMS, CONNECTIONS} */
static int plist_ok(id v) {
    u64 t = CFGetTypeID(v);
    return t == CFDataGetTypeID() || t == CFStringGetTypeID() || t == CFNumberGetTypeID() || v == (id)kCFBooleanTrue || v == (id)kCFBooleanFalse;
}
static id ext_one(tr_t *t) {
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    id a = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    id keys = MSG0(id, t->attrs, "allKeys");
    for (u64 i = 0; i < MSG0(u64, keys, "count"); i++) {
        id k = MSG(id, keys, "objectAtIndex:", i), v = MSG(id, t->attrs, "objectForKey:", k);
        if (plist_ok(v)) MSG(void, a, "setObject:forKey:", v, k);
    }
    MSG(void, d, "setObject:forKey:", cx_str(g_kind_name[t->kind]), cx_str("TYPE"));
    MSG(void, d, "setObject:forKey:", a, cx_str("ATTRS"));
    return d;
}
id SecTransformCopyExternalRepresentation(id o) {
    tr_t *t = tr_of(o);
    if (!t) return 0;
    if (t->kind != T_GROUP) return objc_retain(ext_one(t));
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    id list = MSG0(id, cls("NSMutableArray"), "array"), conns = MSG0(id, cls("NSMutableArray"), "array");
    u64 n = MSG0(u64, t->members, "count");
    for (u64 i = 0; i < n; i++) {
        tr_t *m = tr_of(MSG(id, t->members, "objectAtIndex:", i));
        MSG(void, list, "addObject:", ext_one(m));
        id keys = MSG0(id, m->links, "allKeys");
        for (u64 j = 0; j < MSG0(u64, keys, "count"); j++) {
            id dk = MSG(id, keys, "objectAtIndex:", j), pair = MSG(id, m->links, "objectForKey:", dk);
            u64 si = MSG(u64, t->members, "indexOfObject:", MSG(id, pair, "objectAtIndex:", (u64)0));
            id c = MSG0(id, cls("NSMutableDictionary"), "dictionary");
            MSG(void, c, "setObject:forKey:", cx_num_i((i64)si), cx_str("SRC"));
            MSG(void, c, "setObject:forKey:", MSG(id, pair, "objectAtIndex:", (u64)1), cx_str("SRCATTR"));
            MSG(void, c, "setObject:forKey:", cx_num_i((i64)i), cx_str("DST"));
            MSG(void, c, "setObject:forKey:", dk, cx_str("DSTATTR"));
            MSG(void, conns, "addObject:", c);
        }
    }
    MSG(void, d, "setObject:forKey:", list, cx_str("TRANSFORMS"));
    MSG(void, d, "setObject:forKey:", conns, cx_str("CONNECTIONS"));
    return objc_retain(d);
}
static id from_one(id d, id *err) {
    id type = MSG(id, d, "objectForKey:", cx_str("TYPE")), a = MSG(id, d, "objectForKey:", cx_str("ATTRS"));
    int kind = 0;
    for (int i = 1; i <= T_NULL; i++) if (type && cx_is_str(type) && !strcmp(cx_cstr(type), g_kind_name[i])) kind = i;
    if (!kind || kind == T_READ) { set_err(err, kSecTransformErrorInvalidInputDictionary, "Unknown transform type"); return 0; }
    id o = tr_new(kind);
    if (a) MSG(void, tr_of(o)->attrs, "addEntriesFromDictionary:", a);
    return o;
}
id SecTransformCreateFromExternalRepresentation(id d, id *err) {
    if (err) *err = 0;
    if (!d) { set_err(err, kSecTransformErrorInvalidInputDictionary, "No representation"); return 0; }
    id list = MSG(id, d, "objectForKey:", cx_str("TRANSFORMS"));
    if (!list) { id o = from_one(d, err); return o; }
    id g = tr_new(T_GROUP);
    tr_t *gt = tr_of(g);
    u64 n = MSG0(u64, list, "count");
    for (u64 i = 0; i < n; i++) {
        id o = from_one(MSG(id, list, "objectAtIndex:", i), err);
        if (!o) { objc_release(g); return 0; }
        MSG(void, gt->members, "addObject:", o);
        objc_release(o);
    }
    id conns = MSG(id, d, "objectForKey:", cx_str("CONNECTIONS"));
    for (u64 i = 0; conns && i < MSG0(u64, conns, "count"); i++) {
        id c = MSG(id, conns, "objectAtIndex:", i);
        u64 si = (u64)num(MSG(id, c, "objectForKey:", cx_str("SRC"))), di = (u64)num(MSG(id, c, "objectForKey:", cx_str("DST")));
        if (si >= n || di >= n) { set_err(err, kSecTransformErrorInvalidInputDictionary, "Bad connection"); objc_release(g); return 0; }
        id src = MSG(id, gt->members, "objectAtIndex:", si), dst = MSG(id, gt->members, "objectAtIndex:", di);
        id objs[2] = {src, MSG(id, c, "objectForKey:", cx_str("SRCATTR"))};
        id pair = MSG(id, cls("NSArray"), "arrayWithObjects:count:", (id *)objs, (u64)2);
        MSG(void, tr_of(dst)->links, "setObject:forKey:", pair, MSG(id, c, "objectForKey:", cx_str("DSTATTR")));
    }
    return g;
}
