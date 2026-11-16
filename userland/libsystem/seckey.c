/*
 * mOS — libSystem: SecKey (Security.framework) sobre o núcleo de seckey_core.inc.
 *
 * O SecKeyRef é uma instância de __mOSSecKey (NSObject) com o sk_key do núcleo; CFGetTypeID dela é
 * SecKeyGetTypeID() (pelo -_cfTypeID que cfcollections.c consulta). RSA de 1024 a 8192 bits e EC P-256/384/521.
 * Formato externo igual ao do macOS (PKCS#1 para RSA, X9.63 para EC). Erros: CFError no domínio
 * NSOSStatusErrorDomain com o OSStatus que o macOS usa (errSecParam -50, errSecUnsupportedAlgorithm... -4).
 * Teste do núcleo: tools/seckeytest.c + tools/seckeytest.py (contra o OpenSSL).
 *
 * Não há Secure Enclave: kSecAttrTokenIDSecureEnclave devolve errSecUnimplemented (-4), como num Mac sem T2.
 * ECIES e OAEP+AESGCM ainda não (SecKeyIsAlgorithmSupported diz não). kSecAttrIsPermanent grava as metades no
 * Keychain (classe kSecClassKey de security.c); SecItemCopyMatching com kSecReturnRef devolve o SecKey.
 */
#define MOS_HOST_TEST 1                    /* cabeçalhos do kernel pelo shim de userland/libsystem/kshim */
#include "sys.h"
#include "kshim/crypto.h"
#include "kshim/x509.h"

typedef void *id_;
typedef int OSStatus;
typedef unsigned char Boolean;
void *malloc(size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);

#include "seckey_core.inc"

/* ---- ponte com o Foundation do mOS */
typedef struct cx_class cx_class_t;
cx_class_t *cx_class_named(const char *name);
cx_class_t *cx_define_class(cx_class_t *cls, cx_class_t *meta, const char *name, cx_class_t *super, u32 extra);
void cx_add_method(cx_class_t *cls, const char *sel, void *imp);
id_ cx_alloc_instance(cx_class_t *cls);
cx_class_t *object_getClass(id_ o);
void *cx_lookup(cx_class_t *cls, void *sel);
id_ objc_msgSend(id_ self, void *op, ...);
void *sel_registerName(const char *name);
id_ objc_retain(id_ o);
void objc_release(id_ o);
#define MSG0(T, o, s) ((T (*)(id_, void *))objc_msgSend)((id_)(o), sel_registerName(s))
#define MSG1(T, o, s, a) ((T (*)(id_, void *, __typeof__(a)))objc_msgSend)((id_)(o), sel_registerName(s), (a))

id_ CFDictionaryGetValue(id_ d, id_ key);
id_ CFDictionaryCreateMutable(void *alloc, long cap, const void *kcb, const void *vcb);
void CFDictionarySetValue(id_ d, id_ key, id_ val);
u8 CFEqual(id_ a, id_ b);
u64 CFGetTypeID(id_ o);
u64 CFDataGetTypeID(void);
u64 CFDictionaryGetTypeID(void);
long CFDataGetLength(id_ d);
const u8 *CFDataGetBytePtr(id_ d);
id_ cx_data_new(const void *b, u64 n);
id_ CFErrorCreate(void *alloc, id_ domain, long code, id_ info);
void CFRelease(id_ o);
id_ cx_num_i(long long v);
extern void *kCFBooleanTrue, *kCFBooleanFalse;
extern const void *kCFErrorDomainOSStatus;

extern const void *kSecAttrKeyType, *kSecAttrKeyTypeRSA, *kSecAttrKeyTypeEC, *kSecAttrKeyTypeECSECPrimeRandom;
extern const void *kSecAttrKeySizeInBits, *kSecAttrEffectiveKeySize, *kSecAttrKeyClass, *kSecAttrKeyClassPrivate;
extern const void *kSecAttrKeyClassPublic, *kSecAttrTokenID, *kSecAttrTokenIDSecureEnclave, *kSecPrivateKeyAttrs;
extern const void *kSecAttrApplicationLabel, *kSecAttrCanSign, *kSecAttrCanVerify, *kSecAttrCanEncrypt;
extern const void *kSecAttrCanDecrypt, *kSecAttrCanDerive, *kSecAttrCanWrap, *kSecAttrCanUnwrap, *kSecValueData;
extern const void *kSecAttrIsPermanent, *kSecAttrIsExtractable, *kSecAttrIsSensitive, *kSecClass, *kSecClassKey;
extern const void *kSecKeyKeyExchangeParameterRequestedSize, *kSecKeyKeyExchangeParameterSharedInfo;
#define K_(v) ((id_)(v))

#define errSecSuccess 0
#define errSecUnimplemented (-4)
#define errSecParam (-50)
#define errSecAllocate (-108)
#define errSecDecode (-26275)
#define errSecVerifyFailed (-67808)
#define errSecInvalidKeyRef (-67712)
#define errSecUnsupportedKeySize (-67735)
#define errSecInvalidAlgorithm (-67747)

/* ---- classe */
typedef struct { cx_class_t *isa; sk_key *k; } seckey_obj;
static u8 SecKey_c[64] __attribute__((aligned(16))), SecKey_m[64] __attribute__((aligned(16)));
#define TID_SEC_KEY 0x5EC03
static u64 o_typeid(id_ self, void *c) { (void)self; (void)c; return TID_SEC_KEY; }
static void o_dealloc(id_ self, void *c) {
    sk_free(((seckey_obj *)self)->k);
    ((seckey_obj *)self)->k = 0;
    void *sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id_, void *))sup)(self, c);
}
static id_ o_description(id_ self, void *c) {
    (void)c;
    sk_key *k = ((seckey_obj *)self)->k;
    const char *t = !k ? "?" : k->type == SK_RSA ? (k->priv ? "<SecKeyRef algorithm id: 1, key type: RSAPrivateKey>"
                                                            : "<SecKeyRef algorithm id: 1, key type: RSAPublicKey>")
                                                 : (k->priv ? "<SecKeyRef curve type: kSecECCurveSecp, algorithm id: 3, key type: ECPrivateKey>"
                                                            : "<SecKeyRef curve type: kSecECCurveSecp, algorithm id: 3, key type: ECPublicKey>");
    return MSG1(id_, (id_)cx_class_named("NSString"), "stringWithUTF8String:", t);
}
static cx_class_t *key_class(void) {
    static int done;
    if (!done) {
        cx_class_t *c = (cx_class_t *)SecKey_c;
        cx_define_class(c, (cx_class_t *)SecKey_m, "__mOSSecKey", cx_class_named("NSObject"), (u32)(sizeof(seckey_obj) - 8));
        cx_add_method(c, "_cfTypeID", (void *)o_typeid);
        cx_add_method(c, "dealloc", (void *)o_dealloc);
        cx_add_method(c, "description", (void *)o_description);
        cx_add_method(c, "debugDescription", (void *)o_description);
        done = 1;
    }
    return (cx_class_t *)SecKey_c;
}
static id_ wrap(sk_key *k) {
    if (!k) return 0;
    seckey_obj *o = (seckey_obj *)cx_alloc_instance(key_class());
    if (!o) { sk_free(k); return 0; }
    o->k = k;
    return (id_)o;
}
static sk_key *unwrap(id_ key) {
    if (!key || object_getClass(key) != key_class()) return 0;
    return ((seckey_obj *)key)->k;
}
/* para security.c (SecItemAdd de kSecClassKey): classe, tipo (1 RSA, 2 EC) e kSecAttrApplicationLabel
 * (SHA-1 do formato externo da pública) a partir do formato externo guardado */
int cx_seckey_describe(const u8 *ext, u32 n, int *priv, int *type, u8 klbl[20]) {
    sk_key *k = sk_import(ext, n, 0, -1);
    if (!k) return -1;
    *priv = k->priv;
    *type = k->type;
    sk_key *pub = k->priv ? sk_public_of(k) : 0;
    u8 *b;
    usize bn;
    int r = sk_export(pub ? pub : k, &b, &bn);
    if (!r) { sha1(b, bn, klbl); free(b); }
    sk_free(pub);
    sk_free(k);
    return r;
}
/* para seccert.c (SecItemExport) */
int cx_seckey_spki(id_ key, u8 **out, u32 *n) { sk_key *k = unwrap(key); usize m; if (!k || sk_export_spki(k, out, &m)) return -1; *n = (u32)m; return 0; }
int cx_seckey_pkcs8(id_ key, u8 **out, u32 *n) { sk_key *k = unwrap(key); usize m; if (!k || sk_export_pkcs8(k, out, &m)) return -1; *n = (u32)m; return 0; }
/* para security.c (identidades, certificados) */
id_ cx_seckey_from_spki(const u8 *der, u32 n) { return wrap(sk_from_spki(der, n)); }
id_ cx_seckey_import(const u8 *der, u32 n, int priv) { return wrap(sk_import(der, n, 0, priv)); }

static void set_err(id_ *err, OSStatus st) { if (err) *err = CFErrorCreate(0, K_(kCFErrorDomainOSStatus), st, 0); }
static const char *utf8(id_ s) { return s ? MSG0(const char *, s, "UTF8String") : 0; }
static int type_of(id_ v) {
    if (!v) return 0;
    if (CFEqual(v, K_(kSecAttrKeyTypeRSA))) return SK_RSA;
    if (CFEqual(v, K_(kSecAttrKeyTypeEC)) || CFEqual(v, K_(kSecAttrKeyTypeECSECPrimeRandom))) return SK_EC;
    const char *s = utf8(v);                           /* "42"/"73" como CFString ou CFNumber */
    long n = MSG0(long, v, "integerValue");
    if (n == 42 || (s && s[0] == 'R')) return SK_RSA;
    if (n == 73) return SK_EC;
    return -1;
}
static long num_of(id_ v) { return v ? MSG0(long, v, "integerValue") : 0; }

/* ================================================================ criar */
OSStatus SecItemAdd(id_ attrs, id_ *result);
extern const void *kSecValueRef, *kSecPublicKeyAttrs, *kSecAttrApplicationTag, *kSecAttrLabel, *kSecAttrAccessGroup;
static int flag_of(id_ d, const void *k) { id_ v = d ? CFDictionaryGetValue(d, K_(k)) : 0; return v && MSG0(Boolean, v, "boolValue"); }
static id_ attr_of(id_ sub, id_ top, const void *k) {
    id_ v = sub ? CFDictionaryGetValue(sub, K_(k)) : 0;
    return v ? v : top ? CFDictionaryGetValue(top, K_(k)) : 0;
}
id_ SecKeyCopyPublicKey(id_ key);
/* kSecAttrIsPermanent (no topo ou em kSecPrivateKeyAttrs/kSecPublicKeyAttrs): grava as duas metades no Keychain
 * com a tag, o rótulo e o grupo pedidos, como o SecKeyCreateRandomKey do macOS */
static OSStatus persist(id_ key, id_ params) {
    id_ pa = CFDictionaryGetValue(params, K_(kSecPrivateKeyAttrs)), ua = CFDictionaryGetValue(params, K_(kSecPublicKeyAttrs));
    int top = flag_of(params, kSecAttrIsPermanent), p_priv = top || flag_of(pa, kSecAttrIsPermanent), p_pub = top || flag_of(ua, kSecAttrIsPermanent);
    for (int half = 0; half < 2; half++) {
        if (!(half ? p_pub : p_priv)) continue;
        id_ sub = half ? ua : pa;
        id_ obj = half ? SecKeyCopyPublicKey(key) : objc_retain(key);
        id_ d = CFDictionaryCreateMutable(0, 0, 0, 0);
        CFDictionarySetValue(d, K_(kSecClass), K_(kSecClassKey));
        CFDictionarySetValue(d, K_(kSecValueRef), obj);
        const void *keys[] = {kSecAttrApplicationTag, kSecAttrLabel, kSecAttrAccessGroup};
        for (int i = 0; i < 3; i++) { id_ v = attr_of(sub, params, keys[i]); if (v) CFDictionarySetValue(d, K_(keys[i]), v); }
        OSStatus st = SecItemAdd(d, 0);
        CFRelease(d);
        objc_release(obj);
        if (st && st != -25299) return st;            /* errSecDuplicateItem: a mesma chave já estava lá */
    }
    return errSecSuccess;
}
id_ SecKeyCreateRandomKey(id_ params, id_ *err) {
    if (err) *err = 0;
    if (!params || CFGetTypeID(params) != CFDictionaryGetTypeID()) { set_err(err, errSecParam); return 0; }
    id_ tok = CFDictionaryGetValue(params, K_(kSecAttrTokenID));
    if (tok && CFEqual(tok, K_(kSecAttrTokenIDSecureEnclave))) { set_err(err, errSecUnimplemented); return 0; }
    int t = type_of(CFDictionaryGetValue(params, K_(kSecAttrKeyType)));
    long bits = num_of(CFDictionaryGetValue(params, K_(kSecAttrKeySizeInBits)));
    if (t <= 0) { set_err(err, errSecParam); return 0; }
    if (!bits) bits = t == SK_RSA ? 2048 : 256;
    sk_key *k = t == SK_RSA ? sk_rsa_generate((u32)bits) : sk_ec_generate((u32)bits);
    if (!k) { set_err(err, errSecParam); return 0; }
    id_ key = wrap(k);
    if (key) {
        OSStatus st = persist(key, params);
        if (st) { objc_release(key); set_err(err, st); return 0; }
    }
    return key;
}
OSStatus SecKeyGeneratePair(id_ params, id_ *pub, id_ *priv) {
    if (pub) *pub = 0;
    if (priv) *priv = 0;
    id_ e = 0;
    id_ k = SecKeyCreateRandomKey(params, &e);
    if (!k) {
        long code = e ? MSG0(long, e, "code") : errSecParam;
        if (e) CFRelease(e);
        return (OSStatus)code;
    }
    if (pub) *pub = wrap(sk_public_of(unwrap(k)));
    if (priv) *priv = k;
    else objc_release(k);
    return errSecSuccess;
}
id_ SecKeyCreateWithData(id_ data, id_ attrs, id_ *err) {
    if (err) *err = 0;
    if (!data || CFGetTypeID(data) != CFDataGetTypeID() || !attrs) { set_err(err, errSecParam); return 0; }
    int t = type_of(CFDictionaryGetValue(attrs, K_(kSecAttrKeyType)));
    id_ cls = CFDictionaryGetValue(attrs, K_(kSecAttrKeyClass));
    int priv = !cls ? -1 : CFEqual(cls, K_(kSecAttrKeyClassPrivate)) ? 1 : CFEqual(cls, K_(kSecAttrKeyClassPublic)) ? 0 : -2;
    if (t < 0 || priv == -2) { set_err(err, errSecParam); return 0; }
    sk_key *k = sk_import(CFDataGetBytePtr(data), (usize)CFDataGetLength(data), t, priv);
    long bits = num_of(CFDictionaryGetValue(attrs, K_(kSecAttrKeySizeInBits)));
    if (k && bits && k->type == SK_EC && (u32)bits != k->bits) { sk_free(k); k = 0; }
    if (!k) { set_err(err, errSecDecode); return 0; }
    return wrap(k);
}
id_ SecKeyCopyPublicKey(id_ key) {
    sk_key *k = unwrap(key);
    if (!k) return 0;
    if (!k->priv) return objc_retain(key);
    return wrap(sk_public_of(k));
}

/* ================================================================ exportar e atributos */
id_ SecKeyCopyExternalRepresentation(id_ key, id_ *err) {
    if (err) *err = 0;
    sk_key *k = unwrap(key);
    u8 *b;
    usize n;
    if (!k) { set_err(err, errSecParam); return 0; }
    if (sk_export(k, &b, &n)) { set_err(err, errSecAllocate); return 0; }
    id_ d = cx_data_new(b, n);
    memset(b, 0, n);
    free(b);
    return d;
}
size_t SecKeyGetBlockSize(id_ key) {
    sk_key *k = unwrap(key);
    if (!k) return 0;
    if (k->type == SK_RSA) return sk_block_size(k);
    return k->B;                                      /* EC: bytes de uma coordenada, como o SecECPublicKeyBlockSize */
}
id_ SecKeyCopyAttributes(id_ key) {
    sk_key *k = unwrap(key);
    if (!k) return 0;
    id_ d = CFDictionaryCreateMutable(0, 0, 0, 0);
    id_ t = k->type == SK_RSA ? K_(kSecAttrKeyTypeRSA) : K_(kSecAttrKeyTypeECSECPrimeRandom);
    id_ yes = (id_)kCFBooleanTrue, no = (id_)kCFBooleanFalse;
    CFDictionarySetValue(d, K_(kSecClass), K_(kSecClassKey));
    CFDictionarySetValue(d, K_(kSecAttrKeyType), t);
    CFDictionarySetValue(d, K_(kSecAttrKeyClass), k->priv ? K_(kSecAttrKeyClassPrivate) : K_(kSecAttrKeyClassPublic));
    id_ nb = cx_num_i(k->bits);                       /* cx_num_i devolve +1 */
    CFDictionarySetValue(d, K_(kSecAttrKeySizeInBits), nb);
    CFDictionarySetValue(d, K_(kSecAttrEffectiveKeySize), nb);
    objc_release(nb);
    CFDictionarySetValue(d, K_(kSecAttrCanSign), k->priv ? yes : no);
    CFDictionarySetValue(d, K_(kSecAttrCanVerify), k->priv ? no : yes);
    CFDictionarySetValue(d, K_(kSecAttrCanEncrypt), k->priv ? no : yes);
    CFDictionarySetValue(d, K_(kSecAttrCanDecrypt), k->priv ? yes : no);
    CFDictionarySetValue(d, K_(kSecAttrCanDerive), k->type == SK_EC && k->priv ? yes : no);
    CFDictionarySetValue(d, K_(kSecAttrCanWrap), k->type == SK_RSA && !k->priv ? yes : no);
    CFDictionarySetValue(d, K_(kSecAttrCanUnwrap), k->type == SK_RSA && k->priv ? yes : no);
    CFDictionarySetValue(d, K_(kSecAttrIsPermanent), no);
    CFDictionarySetValue(d, K_(kSecAttrIsExtractable), yes);
    CFDictionarySetValue(d, K_(kSecAttrIsSensitive), no);
    /* kSecAttrApplicationLabel = SHA-1 da chave pública (o formato externo da pública), como o macOS */
    sk_key *pub = k->priv ? sk_public_of(k) : 0;
    u8 *b;
    usize n;
    if (!sk_export(pub ? pub : k, &b, &n)) {
        u8 h[20];
        sha1(b, n, h);
        id_ lbl = cx_data_new(h, 20);
        CFDictionarySetValue(d, K_(kSecAttrApplicationLabel), lbl);
        objc_release(lbl);
        free(b);
    }
    sk_free(pub);
    id_ ext = SecKeyCopyExternalRepresentation(key, 0);
    if (ext) { CFDictionarySetValue(d, K_(kSecValueData), ext); objc_release(ext); }
    return d;
}

/* ================================================================ operações */
static int alg_of(id_ alg, sk_alg *a) { return sk_parse_alg(utf8(alg), a); }
Boolean SecKeyIsAlgorithmSupported(id_ key, long op, id_ alg) {
    sk_key *k = unwrap(key);
    sk_alg a;
    if (!k || alg_of(alg, &a)) return 0;
    return (Boolean)sk_supports(k, (int)op, &a);
}
static id_ run_op(int op, id_ key, id_ alg, id_ data, id_ *err) {
    if (err) *err = 0;
    sk_key *k = unwrap(key);
    sk_alg a;
    if (!k || !data || CFGetTypeID(data) != CFDataGetTypeID()) { set_err(err, errSecParam); return 0; }
    if (alg_of(alg, &a) || !sk_supports(k, op, &a)) { set_err(err, errSecUnimplemented); return 0; }
    u8 *o;
    usize on;
    const u8 *b = CFDataGetBytePtr(data);
    usize n = (usize)CFDataGetLength(data);
    int rc = op == SKO_SIGN ? sk_sign(k, &a, b, n, &o, &on) : op == SKO_ENCRYPT ? sk_encrypt(k, &a, b, n, &o, &on)
                                                                                : sk_decrypt(k, &a, b, n, &o, &on);
    if (rc) { set_err(err, op == SKO_DECRYPT ? errSecDecode : errSecParam); return 0; }
    id_ d = cx_data_new(o, on);
    memset(o, 0, on);
    free(o);
    return d;
}
id_ SecKeyCreateSignature(id_ key, id_ alg, id_ data, id_ *err) { return run_op(SKO_SIGN, key, alg, data, err); }
id_ SecKeyCreateEncryptedData(id_ key, id_ alg, id_ data, id_ *err) { return run_op(SKO_ENCRYPT, key, alg, data, err); }
id_ SecKeyCreateDecryptedData(id_ key, id_ alg, id_ data, id_ *err) { return run_op(SKO_DECRYPT, key, alg, data, err); }
Boolean SecKeyVerifySignature(id_ key, id_ alg, id_ data, id_ sig, id_ *err) {
    if (err) *err = 0;
    sk_key *k = unwrap(key);
    sk_alg a;
    if (!k || !data || !sig || CFGetTypeID(data) != CFDataGetTypeID() || CFGetTypeID(sig) != CFDataGetTypeID()) {
        set_err(err, errSecParam);
        return 0;
    }
    if (alg_of(alg, &a) || !sk_supports(k, SKO_VERIFY, &a)) { set_err(err, errSecUnimplemented); return 0; }
    if (sk_verify(k, &a, CFDataGetBytePtr(data), (usize)CFDataGetLength(data), CFDataGetBytePtr(sig),
                  (usize)CFDataGetLength(sig))) {
        set_err(err, errSecVerifyFailed);
        return 0;
    }
    return 1;
}
id_ SecKeyCopyKeyExchangeResult(id_ key, id_ alg, id_ pub, id_ params, id_ *err) {
    if (err) *err = 0;
    sk_key *k = unwrap(key), *p = unwrap(pub);
    sk_alg a;
    if (!k || !p) { set_err(err, errSecParam); return 0; }
    if (alg_of(alg, &a) || !sk_supports(k, SKO_EXCHANGE, &a)) { set_err(err, errSecUnimplemented); return 0; }
    usize want = 0, in = 0;
    const u8 *info = 0;
    if (params && CFGetTypeID(params) == CFDictionaryGetTypeID()) {
        want = (usize)num_of(CFDictionaryGetValue(params, K_(kSecKeyKeyExchangeParameterRequestedSize)));
        id_ si = CFDictionaryGetValue(params, K_(kSecKeyKeyExchangeParameterSharedInfo));
        if (si && CFGetTypeID(si) == CFDataGetTypeID()) { info = CFDataGetBytePtr(si); in = (usize)CFDataGetLength(si); }
    }
    if (a.x963 && !want) { set_err(err, errSecParam); return 0; }   /* o macOS exige o tamanho com a KDF */
    u8 *o;
    usize on;
    if (sk_exchange(k, &a, p, info, in, want, &o, &on)) { set_err(err, errSecParam); return 0; }
    id_ d = cx_data_new(o, on);
    memset(o, 0, on);
    free(o);
    return d;
}

/* ================================================================ API antiga (SecKeyRaw*, SecKeyEncrypt/Decrypt) */
enum { kSecPaddingNone = 0, kSecPaddingPKCS1 = 1, kSecPaddingOAEP = 2, kSecPaddingSigRaw = 0x4000,
       kSecPaddingPKCS1MD5 = 0x8001, kSecPaddingPKCS1SHA1 = 0x8002, kSecPaddingPKCS1SHA224 = 0x8003,
       kSecPaddingPKCS1SHA256 = 0x8004, kSecPaddingPKCS1SHA384 = 0x8005, kSecPaddingPKCS1SHA512 = 0x8006 };
static int raw_sig_alg(const sk_key *k, u32 pad, sk_alg *a) {
    memset(a, 0, sizeof *a);
    if (k->type == SK_EC) { a->kind = SKA_ECDSA_X962; return 0; }   /* EC: o dado é o digest, saída DER */
    switch (pad) {
    case kSecPaddingNone: case kSecPaddingSigRaw: a->kind = SKA_RSA_SIG_RAW; return 0;
    case kSecPaddingPKCS1: a->kind = SKA_RSA_SIG_PKCS1; return 0;
    case kSecPaddingPKCS1MD5: a->hash = SKH_MD5; break;
    case kSecPaddingPKCS1SHA1: a->hash = SKH_SHA1; break;
    case kSecPaddingPKCS1SHA224: a->hash = SKH_SHA224; break;
    case kSecPaddingPKCS1SHA256: a->hash = SKH_SHA256; break;
    case kSecPaddingPKCS1SHA384: a->hash = SKH_SHA384; break;
    case kSecPaddingPKCS1SHA512: a->hash = SKH_SHA512; break;
    default: return -1;
    }
    a->kind = SKA_RSA_SIG_PKCS1;
    return 0;
}
OSStatus SecKeyRawSign(id_ key, u32 pad, const u8 *d, size_t dl, u8 *sig, size_t *sl) {
    sk_key *k = unwrap(key);
    sk_alg a;
    if (!k || !d || !sig || !sl) return errSecParam;
    if (raw_sig_alg(k, pad, &a) || !sk_supports(k, SKO_SIGN, &a)) return errSecParam;
    u8 *o;
    usize on;
    if (sk_sign(k, &a, d, dl, &o, &on)) return errSecParam;
    if (on > *sl) { free(o); return errSecParam; }
    memcpy(sig, o, on);
    *sl = on;
    free(o);
    return errSecSuccess;
}
OSStatus SecKeyRawVerify(id_ key, u32 pad, const u8 *d, size_t dl, const u8 *sig, size_t sl) {
    sk_key *k = unwrap(key);
    sk_alg a;
    if (!k || !d || !sig) return errSecParam;
    if (raw_sig_alg(k, pad, &a) || !sk_supports(k, SKO_VERIFY, &a)) return errSecParam;
    return sk_verify(k, &a, d, dl, sig, sl) ? errSecVerifyFailed : errSecSuccess;
}
static int raw_enc_alg(u32 pad, sk_alg *a) {
    memset(a, 0, sizeof *a);
    if (pad == kSecPaddingNone) a->kind = SKA_RSA_ENC_RAW;
    else if (pad == kSecPaddingPKCS1) a->kind = SKA_RSA_ENC_PKCS1;
    else if (pad == kSecPaddingOAEP) { a->kind = SKA_RSA_ENC_OAEP; a->hash = SKH_SHA1; }
    else return -1;
    return 0;
}
static OSStatus raw_crypt(int op, id_ key, u32 pad, const u8 *in, size_t inl, u8 *out, size_t *outl) {
    sk_key *k = unwrap(key);
    sk_alg a;
    if (!k || !in || !out || !outl || k->type != SK_RSA || raw_enc_alg(pad, &a) || !sk_supports(k, op, &a)) return errSecParam;
    u8 *o;
    usize on;
    int rc = op == SKO_ENCRYPT ? sk_encrypt(k, &a, in, inl, &o, &on) : sk_decrypt(k, &a, in, inl, &o, &on);
    if (rc) return op == SKO_DECRYPT ? errSecDecode : errSecParam;
    if (on > *outl) { memset(o, 0, on); free(o); return errSecParam; }
    memcpy(out, o, on);
    *outl = on;
    memset(o, 0, on);
    free(o);
    return errSecSuccess;
}
OSStatus SecKeyEncrypt(id_ k, u32 pad, const u8 *p, size_t pl, u8 *c, size_t *cl) { return raw_crypt(SKO_ENCRYPT, k, pad, p, pl, c, cl); }
OSStatus SecKeyDecrypt(id_ k, u32 pad, const u8 *c, size_t cl, u8 *p, size_t *pl) { return raw_crypt(SKO_DECRYPT, k, pad, c, cl, p, pl); }

/* ================================================================ chave de um certificado ou de uma confiança */
id_ SecCertificateCopyKey(id_ cert) {
    if (!cert || CFGetTypeID(cert) != CFDataGetTypeID()) return 0;
    x509_cert_t c;
    if (x509_parse(&c, CFDataGetBytePtr(cert), (u32)CFDataGetLength(cert))) return 0;
    return wrap(sk_from_spki(c.spki, c.spki_len));
}
OSStatus SecCertificateCopyPublicKey(id_ cert, id_ *key) {
    if (!key) return errSecParam;
    *key = SecCertificateCopyKey(cert);
    return *key ? errSecSuccess : errSecInvalidKeyRef;
}
long CFArrayGetCount(id_ a);
id_ CFArrayGetValueAtIndex(id_ a, long i);
id_ SecTrustCopyKey(id_ trust) {
    if (!trust || CFArrayGetCount(trust) < 1) return 0;
    return SecCertificateCopyKey(CFArrayGetValueAtIndex(trust, 0));
}
id_ SecTrustCopyPublicKey(id_ trust) { return SecTrustCopyKey(trust); }
