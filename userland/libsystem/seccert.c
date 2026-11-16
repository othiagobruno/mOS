/*
 * mOS — libSystem: SecCertificate (acessores), SecItemImport/SecItemExport (PEM, DER, PKCS#8, PKCS#12) e
 * SecPKCS12Import. O SecCertificateRef do mOS é o próprio DER num CFData (security.c); o SecKeyRef é o de seckey.c.
 *
 * PKCS#12 (RFC 7292): PFX v3, sacos keyBag, pkcs8ShroudedKeyBag e certBag, atributos friendlyName e localKeyId,
 * MAC HMAC-SHA1/SHA-256/384/512 com a KDF do PKCS#12. Cifras: PBES2 (PBKDF2 + AES-128/192/256-CBC ou 3DES-CBC) e
 * as antigas pbeWithSHAAnd3-KeyTripleDES-CBC, pbeWithSHAAnd128BitRC2-CBC e pbeWithSHAAnd40BitRC2-CBC. Leitor BER
 * com comprimento indefinido e OCTET STRING construída (os .p12 do Acesso às Chaves e do OpenSSL antigo).
 * Como no macOS (e não no iOS), a identidade importada vai para o Keychain do usuário.
 * Teste de host: tools/seccerttest.py (gera .p12 e certificados com o OpenSSL e confere com o núcleo).
 */
#define MOS_HOST_TEST 1
#include "sys.h"
#include "kshim/crypto.h"
#include "kshim/x509.h"

typedef void *id_;
typedef int OSStatus;
typedef unsigned char Boolean;
void *malloc(size_t n);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int cx_snprintf(char *buf, size_t n, const char *fmt, ...);

#include "seccert_core.inc"

/* ---- ponte CF */
id_ objc_msgSend(id_ self, void *op, ...);
void *sel_registerName(const char *name);
void *cx_class_named(const char *name);
id_ objc_retain(id_ o);
void objc_release(id_ o);
#define MSG0(T, o, s) ((T (*)(id_, void *))objc_msgSend)((id_)(o), sel_registerName(s))
#define MSG1(T, o, s, a) ((T (*)(id_, void *, __typeof__(a)))objc_msgSend)((id_)(o), sel_registerName(s), (a))
id_ CFDictionaryGetValue(id_ d, id_ key);
id_ CFDictionaryCreateMutable(void *alloc, long cap, const void *kcb, const void *vcb);
void CFDictionarySetValue(id_ d, id_ key, id_ val);
id_ CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(id_ a, id_ v);
long CFArrayGetCount(id_ a);
id_ CFArrayGetValueAtIndex(id_ a, long i);
u8 CFEqual(id_ a, id_ b);
u64 CFGetTypeID(id_ o);
u64 CFDataGetTypeID(void);
u64 CFArrayGetTypeID(void);
u64 CFStringGetTypeID(void);
long CFDataGetLength(id_ d);
const u8 *CFDataGetBytePtr(id_ d);
id_ cx_data_new(const void *b, u64 n);
id_ CFDateCreate(void *alloc, double at);
void CFRelease(id_ o);
id_ CFRetain(id_ o);
id_ CFErrorCreate(void *alloc, id_ domain, long code, id_ info);
id_ cx_num_d(double v);
u64 SecKeyGetTypeID(void);
id_ SecKeyCopyExternalRepresentation(id_ key, id_ *err);
id_ SecKeyCopyPublicKey(id_ key);
id_ SecCertificateCopyKey(id_ cert);
id_ SecCertificateCreateWithData(void *alloc, id_ der);
id_ cx_seckey_import(const u8 *der, u32 n, int priv);
int cx_seckey_spki(id_ key, u8 **out, u32 *n);           /* seckey.c: SubjectPublicKeyInfo */
int cx_seckey_pkcs8(id_ key, u8 **out, u32 *n);          /* seckey.c: PrivateKeyInfo (só privada) */
OSStatus SecItemAdd(id_ attrs, id_ *result);
OSStatus SecIdentityCreateWithCertificate(void *kc, id_ cert, id_ *ident);
OSStatus SecTrustCreateWithCertificates(id_ certs, id_ policies, id_ *trust);
id_ SecPolicyCreateBasicX509(void);
extern void *kCFBooleanTrue;
extern const void *kCFErrorDomainOSStatus;
extern const void *kSecPropertyKeyLabel, *kSecPropertyKeyType, *kSecPropertyKeyValue, *kSecPropertyTypeSection,
    *kSecPropertyTypeString, *kSecPropertyTypeData, *kSecPropertyTypeDate, *kSecOIDX509V1SubjectName,
    *kSecOIDX509V1IssuerName, *kSecOIDX509V1SerialNumber, *kSecOIDX509V1ValidityNotBefore,
    *kSecOIDX509V1ValidityNotAfter, *kSecImportExportPassphrase, *kSecImportItemIdentity, *kSecImportItemCertChain,
    *kSecImportItemKeyID, *kSecImportItemLabel, *kSecImportItemTrust, *kSecClass, *kSecClassIdentity, *kSecValueData,
    *kSecAttrGeneric, *kSecAttrLabel, *kSecClassCertificate, *kSecClassKey, *kSecValueRef;
#define K_(v) ((id_)(v))

#define errSecSuccess 0
#define errSecUnimplemented (-4)
#define errSecParam (-50)
#define errSecAllocate (-108)
#define errSecAuthFailed (-25293)
#define errSecDuplicateItem (-25299)
#define errSecDecode (-26275)
#define errSecPassphraseRequired (-25260)
#define errSecUnknownFormat (-25257)
#define errSecPkcs12VerifyFailure (-25264)

static id_ nsstr(const char *s) {
    id_ r = MSG1(id_, (id_)cx_class_named("NSString"), "stringWithUTF8String:", s ? s : "");
    return r ? objc_retain(r) : 0;
}
static const char *utf8(id_ s) { return s ? MSG0(const char *, s, "UTF8String") : 0; }
static int is_cert(id_ c) { return c && CFGetTypeID(c) == CFDataGetTypeID() && CFDataGetLength(c) > 4 && CFDataGetBytePtr(c)[0] == 0x30; }
static int parse_cert(id_ c, x509_cert_t *x) { return is_cert(c) && !x509_parse(x, CFDataGetBytePtr(c), (u32)CFDataGetLength(c)) ? 0 : -1; }

/* ================================================================ SecCertificate */
id_ SecCertificateCopySubjectSummary(id_ cert) {
    x509_cert_t x;
    char b[256];
    if (parse_cert(cert, &x)) return 0;
    if (sc_name_attr(x.subject, x.subject_len, OIDS_CN, sizeof OIDS_CN, b, sizeof b) ||
        sc_name_attr(x.subject, x.subject_len, OIDS_EMAIL, sizeof OIDS_EMAIL, b, sizeof b) ||
        sc_san_first(x.san, x.san_len, b, sizeof b) ||
        sc_name_attr(x.subject, x.subject_len, OIDS_O, sizeof OIDS_O, b, sizeof b) ||
        sc_name_attr(x.subject, x.subject_len, OIDS_OU, sizeof OIDS_OU, b, sizeof b))
        return nsstr(b);
    return 0;
}
OSStatus SecCertificateCopyCommonName(id_ cert, id_ *cn) {
    x509_cert_t x;
    char b[256];
    if (!cn) return errSecParam;
    *cn = 0;
    if (parse_cert(cert, &x)) return errSecParam;
    if (sc_name_attr(x.subject, x.subject_len, OIDS_CN, sizeof OIDS_CN, b, sizeof b)) *cn = nsstr(b);
    return errSecSuccess;                                  /* sem CN: sucesso com NULL, como o macOS */
}
OSStatus SecCertificateCopyEmailAddresses(id_ cert, id_ *out) {
    x509_cert_t x;
    if (!out) return errSecParam;
    *out = 0;
    if (parse_cert(cert, &x)) return errSecParam;
    id_ a = CFArrayCreateMutable(0, 0, 0);
    char b[256];
    for (int i = 0; sc_name_attr_n(x.subject, x.subject_len, OIDS_EMAIL, sizeof OIDS_EMAIL, i, b, sizeof b); i++) {
        id_ s = nsstr(b); CFArrayAppendValue(a, s); CFRelease(s);
    }
    for (int i = 0; sc_san_n(x.san, x.san_len, 0x81, i, b, sizeof b); i++) { id_ s = nsstr(b); CFArrayAppendValue(a, s); CFRelease(s); }
    *out = a;
    return errSecSuccess;
}
id_ SecCertificateCopySerialNumberData(id_ cert, id_ *err) {
    x509_cert_t x;
    if (err) *err = 0;
    if (parse_cert(cert, &x)) { if (err) *err = CFErrorCreate(0, K_(kCFErrorDomainOSStatus), errSecParam, 0); return 0; }
    return cx_data_new(x.serial, x.serial_len);
}
id_ SecCertificateCopySerialNumber(id_ cert, id_ *err) { return SecCertificateCopySerialNumberData(cert, err); }
/* Sequência "normalizada": o Name em DER. O macOS normaliza maiúsculas e espaços; aqui o DER vem como está. */
id_ SecCertificateCopyNormalizedIssuerSequence(id_ cert) { x509_cert_t x; return parse_cert(cert, &x) ? 0 : cx_data_new(x.issuer, x.issuer_len); }
id_ SecCertificateCopyNormalizedSubjectSequence(id_ cert) { x509_cert_t x; return parse_cert(cert, &x) ? 0 : cx_data_new(x.subject, x.subject_len); }
id_ SecCertificateCopyIssuerContent(id_ cert, id_ *err) { if (err) *err = 0; return SecCertificateCopyNormalizedIssuerSequence(cert); }
id_ SecCertificateCopySubjectContent(id_ cert, id_ *err) { if (err) *err = 0; return SecCertificateCopyNormalizedSubjectSequence(cert); }
id_ SecCertificateCopyNotValidBeforeDate(id_ cert) { x509_cert_t x; return parse_cert(cert, &x) ? 0 : CFDateCreate(0, sc_abs_time(x.not_before)); }
id_ SecCertificateCopyNotValidAfterDate(id_ cert) { x509_cert_t x; return parse_cert(cert, &x) ? 0 : CFDateCreate(0, sc_abs_time(x.not_after)); }
double SecCertificateNotValidBefore(id_ cert) { x509_cert_t x; return parse_cert(cert, &x) ? 0 : sc_abs_time(x.not_before); }
double SecCertificateNotValidAfter(id_ cert) { x509_cert_t x; return parse_cert(cert, &x) ? 0 : sc_abs_time(x.not_after); }
id_ SecCertificateCopyShortDescription(void *alloc, id_ cert, id_ *err) { (void)alloc; if (err) *err = 0; return SecCertificateCopySubjectSummary(cert); }
id_ SecCertificateCopyLongDescription(void *alloc, id_ cert, id_ *err) { (void)alloc; if (err) *err = 0; return SecCertificateCopySubjectSummary(cert); }

/* SecCertificateCopyValues: dicionário OID -> {label, type, value}, no formato do macOS */
static id_ prop(const char *label, id_ type, id_ value) {
    id_ d = CFDictionaryCreateMutable(0, 0, 0, 0);
    id_ l = nsstr(label);
    CFDictionarySetValue(d, K_(kSecPropertyKeyLabel), l);
    CFDictionarySetValue(d, K_(kSecPropertyKeyType), type);
    if (value) CFDictionarySetValue(d, K_(kSecPropertyKeyValue), value);
    CFRelease(l);
    return d;
}
static id_ name_section(const char *label, const u8 *name, u32 n) {
    id_ arr = CFArrayCreateMutable(0, 0, 0);
    sc_attr_iter it;
    sc_attr_begin(&it, name, n);
    char oid[64], val[256];
    while (sc_attr_next(&it, oid, sizeof oid, val, sizeof val)) {
        id_ v = nsstr(val), d = prop(oid, K_(kSecPropertyTypeString), v);
        CFArrayAppendValue(arr, d);
        CFRelease(v); CFRelease(d);
    }
    id_ r = prop(label, K_(kSecPropertyTypeSection), arr);
    CFRelease(arr);
    return r;
}
static int wants(id_ keys, const void *k) {
    if (!keys) return 1;
    for (long i = 0; i < CFArrayGetCount(keys); i++) if (CFEqual(CFArrayGetValueAtIndex(keys, i), K_(k))) return 1;
    return 0;
}
id_ SecCertificateCopyValues(id_ cert, id_ keys, id_ *err) {
    x509_cert_t x;
    if (err) *err = 0;
    if (parse_cert(cert, &x)) { if (err) *err = CFErrorCreate(0, K_(kCFErrorDomainOSStatus), errSecParam, 0); return 0; }
    id_ d = CFDictionaryCreateMutable(0, 0, 0, 0), v;
    if (wants(keys, kSecOIDX509V1SubjectName)) { v = name_section("Subject Name", x.subject, x.subject_len); CFDictionarySetValue(d, K_(kSecOIDX509V1SubjectName), v); CFRelease(v); }
    if (wants(keys, kSecOIDX509V1IssuerName)) { v = name_section("Issuer Name", x.issuer, x.issuer_len); CFDictionarySetValue(d, K_(kSecOIDX509V1IssuerName), v); CFRelease(v); }
    if (wants(keys, kSecOIDX509V1SerialNumber)) {
        id_ s = cx_data_new(x.serial, x.serial_len);
        v = prop("Serial Number", K_(kSecPropertyTypeData), s);
        CFDictionarySetValue(d, K_(kSecOIDX509V1SerialNumber), v);
        CFRelease(s); CFRelease(v);
    }
    if (wants(keys, kSecOIDX509V1ValidityNotBefore)) {
        id_ n = cx_num_d(sc_abs_time(x.not_before));
        v = prop("Not Valid Before", K_(kSecPropertyTypeDate), n);
        CFDictionarySetValue(d, K_(kSecOIDX509V1ValidityNotBefore), v);
        objc_release(n); CFRelease(v);
    }
    if (wants(keys, kSecOIDX509V1ValidityNotAfter)) {
        id_ n = cx_num_d(sc_abs_time(x.not_after));
        v = prop("Not Valid After", K_(kSecPropertyTypeDate), n);
        CFDictionarySetValue(d, K_(kSecOIDX509V1ValidityNotAfter), v);
        objc_release(n); CFRelease(v);
    }
    return d;
}
/* para security.c: atributos de um certificado guardado no Keychain (subj, issr, slnr, labl) */
int cx_cert_fields(const u8 *der, u32 n, const u8 **subj, u32 *sl, const u8 **issr, u32 *il, const u8 **ser, u32 *snl,
                   char *label, u32 lcap) {
    x509_cert_t x;
    if (x509_parse(&x, der, n)) return -1;
    *subj = x.subject; *sl = x.subject_len;
    *issr = x.issuer; *il = x.issuer_len;
    *ser = x.serial; *snl = x.serial_len;
    label[0] = 0;
    if (!sc_name_attr(x.subject, x.subject_len, OIDS_CN, sizeof OIDS_CN, label, lcap))
        sc_name_attr(x.subject, x.subject_len, OIDS_O, sizeof OIDS_O, label, lcap);
    return 0;
}

/* ================================================================ SecItemImport / SecItemExport */
enum { kSecFormatUnknown = 0, kSecFormatOpenSSL = 1, kSecFormatSSH = 2, kSecFormatBSAFE = 3, kSecFormatRawKey = 4,
       kSecFormatWrappedPKCS8 = 5, kSecFormatWrappedOpenSSL = 6, kSecFormatWrappedSSH = 7, kSecFormatWrappedLSH = 8,
       kSecFormatX509Cert = 9, kSecFormatPEMSequence = 10, kSecFormatPKCS7 = 11, kSecFormatPKCS12 = 12,
       kSecFormatNetscapeCertSequence = 13, kSecFormatSSHv2 = 14 };
enum { kSecItemTypeUnknown = 0, kSecItemTypePrivateKey = 1, kSecItemTypePublicKey = 2, kSecItemTypeSessionKey = 3,
       kSecItemTypeCertificate = 4, kSecItemTypeAggregate = 5 };
enum { kSecItemPemArmour = 1 };
/* SecItemImportExportKeyParameters: version, flags, passphrase, alertTitle, alertPrompt, accessRef, keyUsage, keyAttributes */
typedef struct { u32 version, flags; id_ passphrase, title, prompt, access, usage, attrs; } kparams_t;

static void add_to_keychain(id_ item, int cert) {
    id_ d = CFDictionaryCreateMutable(0, 0, 0, 0);
    CFDictionarySetValue(d, K_(kSecClass), cert ? K_(kSecClassCertificate) : K_(kSecClassKey));
    CFDictionarySetValue(d, K_(kSecValueRef), item);
    SecItemAdd(d, 0);                                      /* duplicata não é erro na importação */
    CFRelease(d);
}
/* um bloco DER: certificado, chave privada (PKCS#1, PKCS#8, SEC1) ou pública (SPKI) */
static id_ import_der(const u8 *b, u32 n, int *type, int hint_priv) {
    x509_cert_t x;
    if (hint_priv != 1 && !x509_parse(&x, b, n)) {
        id_ d = cx_data_new(b, n), c = SecCertificateCreateWithData(0, d);
        CFRelease(d);
        if (c) { *type = kSecItemTypeCertificate; return c; }
    }
    if (hint_priv != 0) { id_ k = cx_seckey_import(b, n, 1); if (k) { *type = kSecItemTypePrivateKey; return k; } }
    if (hint_priv != 1) { id_ k = cx_seckey_import(b, n, 0); if (k) { *type = kSecItemTypePublicKey; return k; } }
    return 0;
}
static OSStatus pkcs12_import(const u8 *b, u32 n, id_ pass, id_ *items, int add_kc, id_ *raw_out);

OSStatus SecItemImport(id_ data, id_ ext, u32 *fmt, u32 *type, u32 flags, const void *params, void *kc, id_ *out) {
    (void)ext; (void)flags;
    if (out) *out = 0;
    if (!data || CFGetTypeID(data) != CFDataGetTypeID()) return errSecParam;
    const u8 *b = CFDataGetBytePtr(data);
    u32 n = (u32)CFDataGetLength(data);
    const kparams_t *kp = params;
    id_ pass = kp ? kp->passphrase : 0;
    u32 want = fmt ? *fmt : kSecFormatUnknown;
    id_ arr = CFArrayCreateMutable(0, 0, 0);
    int ty = kSecItemTypeUnknown, nitems = 0;
    u32 got_fmt = want;
    if (want == kSecFormatPKCS12 || (want == kSecFormatUnknown && sc_is_pkcs12(b, n))) {
        id_ list = 0;
        OSStatus st = pkcs12_import(b, n, pass, &list, kc != 0, &arr);
        if (list) CFRelease(list);
        if (st) { CFRelease(arr); return st; }
        got_fmt = kSecFormatPKCS12;
        ty = kSecItemTypeAggregate;
        nitems = (int)CFArrayGetCount(arr);
    } else if (sc_is_pem(b, n)) {
        sc_pem_iter it;
        sc_pem_begin(&it, b, n);
        const char *label;
        u8 *der;
        u32 dn;
        int encrypted = 0;
        while (sc_pem_next(&it, &label, &der, &dn, &encrypted)) {
            if (encrypted) { free(der); CFRelease(arr); return errSecPassphraseRequired; }
            int priv = sc_has(label, "PRIVATE") ? 1 : sc_has(label, "PUBLIC") ? 0 : -1;
            int t1 = 0;
            id_ item = import_der(der, dn, &t1, priv);
            free(der);
            if (!item) continue;
            if (kc) add_to_keychain(item, t1 == kSecItemTypeCertificate);
            CFArrayAppendValue(arr, item);
            CFRelease(item);
            ty = nitems && ty != t1 ? kSecItemTypeAggregate : t1;
            nitems++;
        }
        got_fmt = nitems > 1 ? kSecFormatPEMSequence : ty == kSecItemTypeCertificate ? kSecFormatX509Cert : kSecFormatOpenSSL;
    } else {
        int t1 = 0;
        id_ item = import_der(b, n, &t1, -1);
        if (item) {
            if (kc) add_to_keychain(item, t1 == kSecItemTypeCertificate);
            CFArrayAppendValue(arr, item);
            CFRelease(item);
            ty = t1;
            nitems = 1;
            got_fmt = t1 == kSecItemTypeCertificate ? kSecFormatX509Cert : kSecFormatOpenSSL;
        }
    }
    if (!nitems) { CFRelease(arr); return errSecUnknownFormat; }
    if (fmt) *fmt = got_fmt;
    if (type) *type = (u32)ty;
    if (out) *out = arr; else CFRelease(arr);
    return errSecSuccess;
}
OSStatus SecKeychainItemImport(id_ data, id_ ext, u32 *fmt, u32 *type, u32 flags, const void *params, void *kc, id_ *out) {
    return SecItemImport(data, ext, fmt, type, flags, params, kc, out);
}
/* exportação: certificados em DER/PEM, chaves em PKCS#1/SEC1/SPKI (OpenSSL) ou PKCS#8 */
static int export_one(id_ item, u32 fmt, u8 **der, u32 *dn, const char **label) {
    if (is_cert(item)) {
        *dn = (u32)CFDataGetLength(item);
        *der = malloc(*dn);
        if (!*der) return -1;
        memcpy(*der, CFDataGetBytePtr(item), *dn);
        *label = "CERTIFICATE";
        return 0;
    }
    if (!item || CFGetTypeID(item) != SecKeyGetTypeID()) return -1;
    id_ pub = SecKeyCopyPublicKey(item);
    int is_priv = pub && pub != item;
    if (pub) CFRelease(pub);
    if (!is_priv) { *label = "PUBLIC KEY"; return cx_seckey_spki(item, der, dn); }
    if (fmt == kSecFormatWrappedPKCS8 || fmt == kSecFormatUnknown) { *label = "PRIVATE KEY"; return cx_seckey_pkcs8(item, der, dn); }
    id_ ext = SecKeyCopyExternalRepresentation(item, 0);
    if (!ext) return -1;
    *dn = (u32)CFDataGetLength(ext);
    *der = malloc(*dn);
    if (*der) memcpy(*der, CFDataGetBytePtr(ext), *dn);
    const u8 *e = CFDataGetBytePtr(ext);
    *label = e[0] == 0x04 ? "EC PRIVATE KEY" : "RSA PRIVATE KEY";
    CFRelease(ext);
    if (e[0] == 0x04 && *der) {                             /* X9.63 -> ECPrivateKey (SEC1) */
        u8 *s; u32 sn;
        if (sc_x963_to_sec1(*der, *dn, &s, &sn)) { free(*der); return -1; }
        free(*der);
        *der = s; *dn = sn;
    }
    return *der ? 0 : -1;
}
OSStatus SecItemExport(id_ items, u32 fmt, u32 flags, const void *params, id_ *out) {
    (void)params;
    if (!out || !items) return errSecParam;
    *out = 0;
    if (fmt == kSecFormatPKCS12 || fmt == kSecFormatPKCS7 || fmt == kSecFormatSSH || fmt == kSecFormatSSHv2) return errSecUnimplemented;
    int arr = CFGetTypeID(items) == CFArrayGetTypeID();
    long n = arr ? CFArrayGetCount(items) : 1;
    int pem = (flags & kSecItemPemArmour) || fmt == kSecFormatPEMSequence;
    if (!pem && n != 1) return errSecParam;
    sk_buf acc = {0};
    for (long i = 0; i < n; i++) {
        id_ it = arr ? CFArrayGetValueAtIndex(items, i) : items;
        u8 *der; u32 dn; const char *label;
        if (export_one(it, fmt, &der, &dn, &label)) { free(acc.b); return errSecParam; }
        if (pem) sc_pem_write(&acc, label, der, dn); else sb_put(&acc, der, dn);
        memset(der, 0, dn);
        free(der);
    }
    if (acc.err) { free(acc.b); return errSecAllocate; }
    *out = cx_data_new(acc.b, acc.n);
    memset(acc.b, 0, acc.n);
    free(acc.b);
    return errSecSuccess;
}
OSStatus SecKeychainItemExport(id_ items, u32 fmt, u32 flags, const void *params, id_ *out) { return SecItemExport(items, fmt, flags, params, out); }

/* ================================================================ PKCS#12 */
static OSStatus pkcs12_import(const u8 *b, u32 n, id_ pass, id_ *items, int add_kc, id_ *raw_out) {
    const char *pw = pass ? utf8(pass) : 0;
    p12_out_t res;
    int r = p12_parse(b, n, pw, &res);
    if (r == P12_E_MAC) return errSecAuthFailed;
    if (r == P12_E_UNSUPPORTED) return errSecUnimplemented;
    if (r) return errSecDecode;
    id_ list = CFArrayCreateMutable(0, 0, 0);
    id_ chain = CFArrayCreateMutable(0, 0, 0);
    /* a folha é o certificado cujo localKeyId (ou chave pública) bate com a chave; os demais formam a cadeia */
    int leaf = -1, key = -1;
    for (int k = 0; k < res.nkeys && leaf < 0; k++)
        for (int c = 0; c < res.ncerts; c++)
            if (p12_cert_matches_key(&res, c, k)) { leaf = c; key = k; break; }
    if (leaf < 0 && res.ncerts) leaf = 0;
    if (key < 0 && res.nkeys && leaf >= 0) key = 0;   /* sem localKeyId: a única chave vai com a folha */
    if (leaf >= 0) {
        id_ d = cx_data_new(res.certs[leaf].der, res.certs[leaf].n);
        CFArrayAppendValue(chain, d);
        if (raw_out && *raw_out) CFArrayAppendValue(*raw_out, d);
        CFRelease(d);
    }
    for (int c = 0; c < res.ncerts; c++) {
        if (c == leaf) continue;
        id_ d = cx_data_new(res.certs[c].der, res.certs[c].n);
        CFArrayAppendValue(chain, d);
        if (raw_out && *raw_out) CFArrayAppendValue(*raw_out, d);
        if (add_kc) add_to_keychain(d, 1);
        CFRelease(d);
    }
    id_ dict = CFDictionaryCreateMutable(0, 0, 0, 0);
    const char *fname = key >= 0 && res.keys[key].name[0] ? res.keys[key].name : leaf >= 0 ? res.certs[leaf].name : "";
    if (fname && fname[0]) { id_ l = nsstr(fname); CFDictionarySetValue(dict, K_(kSecImportItemLabel), l); CFRelease(l); }
    if (key >= 0 && res.keys[key].idn) {
        id_ kid = cx_data_new(res.keys[key].id, res.keys[key].idn);
        CFDictionarySetValue(dict, K_(kSecImportItemKeyID), kid);
        CFRelease(kid);
    }
    CFDictionarySetValue(dict, K_(kSecImportItemCertChain), chain);
    if (CFArrayGetCount(chain)) {
        id_ pol = SecPolicyCreateBasicX509(), trust = 0;
        if (!SecTrustCreateWithCertificates(chain, pol, &trust) && trust) { CFDictionarySetValue(dict, K_(kSecImportItemTrust), trust); CFRelease(trust); }
        CFRelease(pol);
    }
    if (key >= 0 && leaf >= 0) {
        id_ sk = cx_seckey_import(res.keys[key].der, res.keys[key].n, 1);
        id_ ext = sk ? SecKeyCopyExternalRepresentation(sk, 0) : 0;
        if (ext) {
            /* a identidade do mOS é um item kSecClassIdentity no Keychain: o DER do certificado e a chave */
            id_ attrs = CFDictionaryCreateMutable(0, 0, 0, 0);
            id_ cd = cx_data_new(res.certs[leaf].der, res.certs[leaf].n);
            CFDictionarySetValue(attrs, K_(kSecClass), K_(kSecClassIdentity));
            CFDictionarySetValue(attrs, K_(kSecValueData), cd);
            CFDictionarySetValue(attrs, K_(kSecAttrGeneric), ext);
            if (fname && fname[0]) { id_ l = nsstr(fname); CFDictionarySetValue(attrs, K_(kSecAttrLabel), l); CFRelease(l); }
            OSStatus st = SecItemAdd(attrs, 0);
            CFRelease(attrs);
            id_ ident = 0;
            if (st == errSecSuccess || st == errSecDuplicateItem) SecIdentityCreateWithCertificate(0, cd, &ident);
            if (ident) { CFDictionarySetValue(dict, K_(kSecImportItemIdentity), ident); CFRelease(ident); }
            if (raw_out && *raw_out) CFArrayAppendValue(*raw_out, sk);
            CFRelease(cd);
            CFRelease(ext);
        }
        if (sk) CFRelease(sk);
    }
    CFArrayAppendValue(list, dict);
    CFRelease(dict);
    CFRelease(chain);
    p12_free(&res);
    *items = list;
    return errSecSuccess;
}
OSStatus SecPKCS12Import(id_ data, id_ opts, id_ *items_) {
    if (items_) *items_ = 0;
    if (!data || CFGetTypeID(data) != CFDataGetTypeID()) return errSecParam;
    id_ pass = opts ? CFDictionaryGetValue(opts, K_(kSecImportExportPassphrase)) : 0;
    id_ list = 0;
    OSStatus st = pkcs12_import(CFDataGetBytePtr(data), (u32)CFDataGetLength(data), pass, &list, 1, 0);
    if (st) return st;
    if (items_) *items_ = list; else CFRelease(list);
    return errSecSuccess;
}

/* ================================================================ ajustes de confiança do usuário e do admin
 * ("Confiar sempre" do Acesso às Chaves). Um arquivo de texto por domínio, uma linha "<resultado> <DER em hex>":
 * usuário em ~/Library/Keychains/mos-trust-user.txt, admin em /Library/Keychains/mos-trust-admin.txt.
 * Resultados (kSecTrustSettingsResult*): 1 TrustRoot, 2 TrustAsRoot, 3 Deny, 4 Unspecified. O SecTrust
 * (security.c) usa os 1/2 como âncoras e recusa uma cadeia com um certificado 3. */
int open(const char *path, int flags, ...);
long read(int fd, void *b, size_t n);
long write(int fd, const void *b, size_t n);
int close(int fd);
int rename(const char *a, const char *b);
int mkdir(const char *p, unsigned mode);
char *getenv(const char *name);
extern const void *kSecTrustSettingsResult;
id_ cx_num_i(long long v);
#define errSecItemNotFound (-25300)
#define errSecNoTrustSettings (-25263)
enum { TS_USER = 0, TS_ADMIN = 1 };
typedef struct { int result; u8 *der; u32 n; } ts_ent;
static void ts_path(int domain, char *p, size_t n, int mk) {
    if (domain == TS_ADMIN) { if (mk) { mkdir("/Library", 0755); mkdir("/Library/Keychains", 0755); } cx_snprintf(p, n, "/Library/Keychains/mos-trust-admin.txt"); return; }
    const char *h = getenv("HOME");
    if (!h || !*h) h = "/Users/Visitante";
    if (mk) {
        char d[400];
        cx_snprintf(d, sizeof d, "%s/Library", h); mkdir(d, 0755);
        cx_snprintf(d, sizeof d, "%s/Library/Keychains", h); mkdir(d, 0700);
    }
    cx_snprintf(p, n, "%s/Library/Keychains/mos-trust-user.txt", h);
}
static int hexd(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }
static int ts_load(int domain, ts_ent **out) {
    char p[512];
    ts_path(domain, p, sizeof p, 0);
    *out = 0;
    int fd = open(p, 0);
    if (fd < 0) return 0;
    sk_buf b = {0};
    char tmp[4096];
    long k;
    while ((k = read(fd, tmp, sizeof tmp)) > 0) sb_put(&b, tmp, (usize)k);
    close(fd);
    int n = 0, cap = 0;
    ts_ent *a = 0;
    for (usize i = 0; i < b.n;) {
        usize e = i;
        while (e < b.n && b.b[e] != '\n') e++;
        if (e - i > 3 && b.b[i + 1] == ' ') {
            u32 len = (u32)(e - i - 2) / 2;
            u8 *der = malloc(len ? len : 1);
            int bad = !der;
            for (u32 j = 0; der && j < len; j++) {
                int h = hexd(b.b[i + 2 + 2 * j]), l = hexd(b.b[i + 3 + 2 * j]);
                if (h < 0 || l < 0) { bad = 1; break; }
                der[j] = (u8)(h << 4 | l);
            }
            if (!bad) {
                if (n == cap) { cap = cap ? cap * 2 : 8; a = realloc(a, (usize)cap * sizeof *a); }
                a[n].result = b.b[i] - '0';
                a[n].der = der;
                a[n].n = len;
                n++;
            } else free(der);
        }
        i = e + 1;
    }
    free(b.b);
    *out = a;
    return n;
}
static void ts_free(ts_ent *a, int n) { for (int i = 0; i < n; i++) free(a[i].der); free(a); }
static OSStatus ts_save(int domain, ts_ent *a, int n) {
    char p[512], t[520];
    ts_path(domain, p, sizeof p, 1);
    cx_snprintf(t, sizeof t, "%s.tmp", p);
    sk_buf b = {0};
    static const char H[] = "0123456789abcdef";
    for (int i = 0; i < n; i++) {
        char r[2] = {(char)('0' + a[i].result), ' '};
        sb_put(&b, r, 2);
        for (u32 j = 0; j < a[i].n; j++) { char x[2] = {H[a[i].der[j] >> 4], H[a[i].der[j] & 15]}; sb_put(&b, x, 2); }
        sb_put(&b, "\n", 1);
    }
    int fd = open(t, 0x601, 0644);                          /* O_WRONLY | O_CREAT | O_TRUNC (Darwin) */
    if (fd < 0) { free(b.b); return -61; }                  /* errSecWrPerm */
    long w = b.n ? write(fd, b.b, b.n) : 0;
    close(fd);
    free(b.b);
    if (w != (long)b.n || rename(t, p)) return -61;
    return errSecSuccess;
}
static int ts_find(ts_ent *a, int n, const u8 *der, u32 dn) {
    for (int i = 0; i < n; i++) if (a[i].n == dn && !memcmp(a[i].der, der, dn)) return i;
    return -1;
}
static int settings_result(id_ s) {
    if (!s) return 1;
    id_ d = s;
    if (CFGetTypeID(s) == CFArrayGetTypeID()) { if (!CFArrayGetCount(s)) return 1; d = CFArrayGetValueAtIndex(s, 0); }
    id_ v = CFDictionaryGetValue(d, K_(kSecTrustSettingsResult));
    long r = v ? MSG0(long, v, "integerValue") : 1;
    return r >= 1 && r <= 4 ? (int)r : 1;
}
OSStatus SecTrustSettingsSetTrustSettings(id_ cert, int domain, id_ settings) {
    if (!is_cert(cert) || (domain != TS_USER && domain != TS_ADMIN)) return errSecParam;
    ts_ent *a;
    int n = ts_load(domain, &a), r = settings_result(settings);
    int i = ts_find(a, n, CFDataGetBytePtr(cert), (u32)CFDataGetLength(cert));
    if (i >= 0) a[i].result = r;
    else {
        a = realloc(a, (usize)(n + 1) * sizeof *a);
        a[n].result = r;
        a[n].n = (u32)CFDataGetLength(cert);
        a[n].der = malloc(a[n].n);
        memcpy(a[n].der, CFDataGetBytePtr(cert), a[n].n);
        n++;
    }
    OSStatus st = ts_save(domain, a, n);
    ts_free(a, n);
    return st;
}
OSStatus SecTrustSettingsRemoveTrustSettings(id_ cert, int domain) {
    if (!is_cert(cert) || (domain != TS_USER && domain != TS_ADMIN)) return errSecParam;
    ts_ent *a;
    int n = ts_load(domain, &a);
    int i = ts_find(a, n, CFDataGetBytePtr(cert), (u32)CFDataGetLength(cert));
    if (i < 0) { ts_free(a, n); return errSecItemNotFound; }
    free(a[i].der);
    a[i] = a[n - 1];
    OSStatus st = ts_save(domain, a, n - 1);
    ts_free(a, n - 1);
    return st;
}
/* para securetransport.c (domínios usuário e admin) */
OSStatus cx_ts_copy_certs(int domain, id_ *certs) {
    ts_ent *a;
    int n = ts_load(domain, &a);
    if (!n) { ts_free(a, n); return errSecNoTrustSettings; }
    id_ arr = CFArrayCreateMutable(0, 0, 0);
    for (int i = 0; i < n; i++) { id_ d = cx_data_new(a[i].der, a[i].n); CFArrayAppendValue(arr, d); CFRelease(d); }
    ts_free(a, n);
    if (certs) *certs = arr; else CFRelease(arr);
    return errSecSuccess;
}
OSStatus cx_ts_copy_settings(id_ cert, int domain, id_ *settings) {
    if (!is_cert(cert)) return errSecParam;
    ts_ent *a;
    int n = ts_load(domain, &a);
    int i = ts_find(a, n, CFDataGetBytePtr(cert), (u32)CFDataGetLength(cert));
    int r = i >= 0 ? a[i].result : 0;
    ts_free(a, n);
    if (i < 0) return errSecItemNotFound;
    id_ arr = CFArrayCreateMutable(0, 0, 0);
    if (r != 1) {                                           /* TrustRoot = array vazio, como o macOS */
        id_ d = CFDictionaryCreateMutable(0, 0, 0, 0), num = cx_num_i(r);
        CFDictionarySetValue(d, K_(kSecTrustSettingsResult), num);
        objc_release(num);
        CFArrayAppendValue(arr, d);
        CFRelease(d);
    }
    if (settings) *settings = arr; else CFRelease(arr);
    return errSecSuccess;
}
/* para security.c: âncoras do usuário/admin (resultado 1 ou 2) e se algum DER está negado (3) */
int cx_ts_anchors(const u8 **ders, u32 *lens, int max, u8 ***own) {
    int k = 0;
    *own = malloc(sizeof(u8 *) * (usize)(max > 0 ? max : 1));
    for (int dom = 0; dom < 2; dom++) {
        ts_ent *a;
        int n = ts_load(dom, &a);
        for (int i = 0; i < n; i++) {
            if ((a[i].result == 1 || a[i].result == 2) && k < max) { ders[k] = a[i].der; lens[k] = a[i].n; (*own)[k] = a[i].der; k++; a[i].der = 0; }
        }
        ts_free(a, n);
    }
    return k;
}
int cx_ts_denied(const u8 *der, u32 n) {
    for (int dom = 0; dom < 2; dom++) {
        ts_ent *a;
        int m = ts_load(dom, &a);
        int i = ts_find(a, m, der, n), deny = i >= 0 && a[i].result == 3;
        ts_free(a, m);
        if (deny) return 1;
    }
    return 0;
}
OSStatus SecCertificateAddToKeychain(id_ cert, void *kc) {
    (void)kc;
    if (!is_cert(cert)) return errSecParam;
    id_ d = CFDictionaryCreateMutable(0, 0, 0, 0);
    CFDictionarySetValue(d, K_(kSecClass), K_(kSecClassCertificate));
    CFDictionarySetValue(d, K_(kSecValueRef), cert);
    OSStatus st = SecItemAdd(d, 0);
    CFRelease(d);
    return st;
}
