/*
 * mOS libSystem — CMSDecoder e CMSEncoder (Security.framework, CMSDecoder.h/CMSEncoder.h) sobre cms_core.inc.
 * Decodificador: SignedData (PKCS#7/CMS, DER), conteúdo anexado ou destacado, certificados, assinantes; a assinatura de
 * cada assinante é conferida com a chave do certificado dele (SecKey: RSA PKCS#1 v1.5 ou ECDSA, SHA-1/256/384/512),
 * incluindo o messageDigest dos atributos assinados, e a cadeia pelo SecTrust com a política pedida. Carimbo de tempo
 * (RFC 3161, id-aa-timeStampToken): o genTime do TSTInfo e os certificados do token.
 * Codificador: SignedData com um ou mais assinantes (SecIdentity) assinando contentType, signingTime e messageDigest,
 * certificados conforme o modo de cadeia, conteúdo anexado ou destacado. Mensagem cifrada (EnvelopedData) é reconhecida
 * (IsContentEncrypted) mas não decifrada nem gerada: CMSEncoderAddRecipients devolve errSecUnimplemented. O codificador
 * assina com um assinante por mensagem.
 */
#include "foundation.h"
#include "libc.h"

typedef int OSStatus;
typedef u8 Boolean;
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
id CFRetain(id o);
void CFRelease(id o);
u64 CFGetTypeID(id o);
u64 CFArrayGetTypeID(void);
u64 CFDataGetTypeID(void);
long CFArrayGetCount(id a);
id CFArrayGetValueAtIndex(id a, long i);
id CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(id a, id v);
id CFDataCreate(void *alloc, const u8 *b, long n);
const u8 *CFDataGetBytePtr(id d);
long CFDataGetLength(id d);
extern const char kCFTypeArrayCallBacks[];
id SecCertificateCreateWithData(void *alloc, id der);
id SecCertificateCopyData(id cert);
id SecCertificateCopyKey(id cert);
OSStatus SecCertificateCopyEmailAddresses(id cert, id *out);
OSStatus SecIdentityCopyCertificate(id ident, id *cert);
OSStatus SecIdentityCopyPrivateKey(id ident, id *key);
id SecKeyCreateSignature(id key, id alg, id data, id *err);
Boolean SecKeyVerifySignature(id key, id alg, id data, id sig, id *err);
Boolean SecKeyIsAlgorithmSupported(id key, long op, id alg);
OSStatus SecTrustCreateWithCertificates(id certs, id policies, id *trust);
Boolean SecTrustEvaluateWithError(id trust, id *err);
OSStatus SecTrustGetCssmResultCode(id trust, int *code);
id SecPolicyCreateBasicX509(void);
int cx_cert_fields(const u8 *der, u32 n, const u8 **subj, u32 *sl, const u8 **issr, u32 *il, const u8 **ser, u32 *snl, char *label, u32 lcap);
unsigned char *CC_SHA1(const void *d, u32 n, unsigned char *md);
unsigned char *CC_SHA256(const void *d, u32 n, unsigned char *md);
unsigned char *CC_SHA384(const void *d, u32 n, unsigned char *md);
unsigned char *CC_SHA512(const void *d, u32 n, unsigned char *md);
double CFAbsoluteTimeGetCurrent(void);
extern const void *kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1, *kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256,
    *kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384, *kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512,
    *kSecKeyAlgorithmECDSASignatureMessageX962SHA1, *kSecKeyAlgorithmECDSASignatureMessageX962SHA256,
    *kSecKeyAlgorithmECDSASignatureMessageX962SHA384, *kSecKeyAlgorithmECDSASignatureMessageX962SHA512;

#include "cms_core.inc"

#define errSecSuccess 0
#define errSecUnimplemented (-4)
#define errSecParam (-50)
#define errSecAllocate (-108)
#define errSecDecode (-26275)
#define errSecUnknownFormat (-25257)
#define errSecTimestampMissing (-67908)
#define errSecSigningTimeMissing (-67894)
enum { kCMSSignerUnsigned, kCMSSignerValid, kCMSSignerNeedsDetachedContent, kCMSSignerInvalidSignature, kCMSSignerInvalidCert,
       kCMSSignerInvalidIndex };
enum { kCMSAttrSigningTime = 8 };
enum { kCMSCertificateNone, kCMSCertificateSignerOnly, kCMSCertificateChain, kCMSCertificateChainWithRoot };

/* objetos: classe ObjC com o estado num ponteiro depois do isa (CFRelease funciona) */
#define ST(T, o) (*(T **)((u8 *)(o) + 8))
static cx_class_t Dec_c, Dec_m, Enc_c, Enc_m;
typedef struct { cms_buf in; id detached; int final_rc; cms_msg m; } dec_t;
typedef struct { cms_buf content; id signers, certs, recips, ctype; int detached, chain, sha1; u32 attrs; } enc_t;
static void dec_dealloc(id s, SEL c) {
    dec_t *d = ST(dec_t, s);
    if (d) { free(d->in.b); if (d->detached) CFRelease(d->detached); free(d); }
    ((void (*)(id, SEL))cx_lookup(cx_class_named("NSObject"), c))(s, c);
}
static void enc_dealloc(id s, SEL c) {
    enc_t *e = ST(enc_t, s);
    if (e) {
        free(e->content.b);
        id *v[] = {&e->signers, &e->certs, &e->recips, &e->ctype};
        for (int i = 0; i < 4; i++) if (*v[i]) CFRelease(*v[i]);
        free(e);
    }
    ((void (*)(id, SEL))cx_lookup(cx_class_named("NSObject"), c))(s, c);
}
static void boot(void) {
    static int done;
    if (done) return;
    done = 1;
    cx_class_t *ns = cx_class_named("NSObject");
    cx_define_class(&Dec_c, &Dec_m, "__CMSDecoder", ns, 8);
    cx_add_method(&Dec_c, "dealloc", (void *)dec_dealloc);
    cx_define_class(&Enc_c, &Enc_m, "__CMSEncoder", ns, 8);
    cx_add_method(&Enc_c, "dealloc", (void *)enc_dealloc);
}
static dec_t *dec_of(id o) { return o && object_getClass(o) == &Dec_c ? ST(dec_t, o) : 0; }
static enc_t *enc_of(id o) { return o && object_getClass(o) == &Enc_c ? ST(enc_t, o) : 0; }
u64 CMSDecoderGetTypeID(void) { return 0x434d5344; }
u64 CMSEncoderGetTypeID(void) { return 0x434d5345; }

/* ================================================================ decodificador */
OSStatus CMSDecoderCreate(id *out) {
    if (!out) return errSecParam;
    boot();
    id o = cx_alloc_instance(&Dec_c);
    dec_t *d = calloc(1, sizeof *d);
    if (!o || !d) return errSecAllocate;
    d->final_rc = 1;                                                          /* ainda não finalizado */
    ST(dec_t, o) = d;
    *out = o;
    return errSecSuccess;
}
OSStatus CMSDecoderUpdateMessage(id dec, const void *b, size_t n) {
    dec_t *d = dec_of(dec);
    if (!d || (!b && n) || d->final_rc != 1) return errSecParam;
    cms_put(&d->in, b, n);
    return d->in.err ? errSecAllocate : errSecSuccess;
}
OSStatus CMSDecoderFinalizeMessage(id dec) {
    dec_t *d = dec_of(dec);
    if (!d || d->final_rc != 1) return errSecParam;
    int r = cms_parse(d->in.b, d->in.n, &d->m);
    d->final_rc = r;
    return r == 0 ? errSecSuccess : r == -2 ? errSecUnknownFormat : errSecDecode;
}
OSStatus CMSDecoderSetDetachedContent(id dec, id data) {
    dec_t *d = dec_of(dec);
    if (!d || !data) return errSecParam;
    if (d->detached) CFRelease(d->detached);
    d->detached = CFRetain(data);
    return errSecSuccess;
}
OSStatus CMSDecoderCopyDetachedContent(id dec, id *out) {
    dec_t *d = dec_of(dec);
    if (!d || !out) return errSecParam;
    *out = d->detached ? CFRetain(d->detached) : 0;
    return errSecSuccess;
}
static int ready(dec_t *d) { return d && d->final_rc == 0; }
OSStatus CMSDecoderIsContentEncrypted(id dec, Boolean *enc) {
    dec_t *d = dec_of(dec);
    if (!ready(d) || !enc) return errSecParam;
    *enc = (Boolean)d->m.encrypted;
    return errSecSuccess;
}
OSStatus CMSDecoderGetNumSigners(id dec, size_t *n) {
    dec_t *d = dec_of(dec);
    if (!ready(d) || !n) return errSecParam;
    *n = (size_t)d->m.nsigners;
    return errSecSuccess;
}
OSStatus CMSDecoderCopyContent(id dec, id *out) {
    dec_t *d = dec_of(dec);
    if (!ready(d) || !out) return errSecParam;
    *out = d->m.has_content ? CFDataCreate(0, d->m.content.p, (long)d->m.content.n) : 0;
    return errSecSuccess;
}
OSStatus CMSDecoderCopyEncapsulatedContentType(id dec, id *out) {
    dec_t *d = dec_of(dec);
    if (!ready(d) || !out) return errSecParam;
    *out = d->m.econtent_type.n ? CFDataCreate(0, d->m.econtent_type.p, (long)d->m.econtent_type.n) : 0;
    return errSecSuccess;
}
static id cert_of(cms_span c) {
    id der = CFDataCreate(0, c.p, (long)c.n);
    id cert = der ? SecCertificateCreateWithData(0, der) : 0;
    if (der) CFRelease(der);
    return cert;
}
OSStatus CMSDecoderCopyAllCerts(id dec, id *out) {
    dec_t *d = dec_of(dec);
    if (!ready(d) || !out) return errSecParam;
    *out = 0;
    if (!d->m.ncerts) return errSecSuccess;
    id a = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    for (int i = 0; i < d->m.ncerts; i++) { id c = cert_of(d->m.certs[i]); if (c) { CFArrayAppendValue(a, c); CFRelease(c); } }
    *out = a;
    return errSecSuccess;
}
/* o certificado do assinante: emissor+série (ou SKI) iguais aos do SignerInfo */
static int signer_cert_index(cms_msg *m, cms_signer *s) {
    for (int i = 0; i < m->ncerts; i++) {
        const u8 *sj, *is, *se;
        u32 sl, il, sn;
        char lab[8];
        if (cx_cert_fields(m->certs[i].p, (u32)m->certs[i].n, &sj, &sl, &is, &il, &se, &sn, lab, sizeof lab)) continue;
        if (s->issuer.n && il == s->issuer.n && !memcmp(is, s->issuer.p, il) && sn == s->serial.n && !memcmp(se, s->serial.p, sn)) return i;
        if (s->ski.n) {
            /* SubjectKeyIdentifier: procura o OCTET STRING da extensão 2.5.29.14 no DER do certificado */
            static const u8 ski_oid[] = {0x06, 0x03, 0x55, 0x1d, 0x0e};
            for (unsigned long k = 0; k + sizeof ski_oid + 4 + s->ski.n <= m->certs[i].n; k++)
                if (!memcmp(m->certs[i].p + k, ski_oid, sizeof ski_oid)) {
                    const u8 *q = m->certs[i].p + k + sizeof ski_oid;
                    if (q[0] == 0x04 && q[2] == 0x04 && q[3] == s->ski.n && !memcmp(q + 4, s->ski.p, s->ski.n)) return i;
                }
        }
    }
    return -1;
}
OSStatus CMSDecoderCopySignerCert(id dec, size_t idx, id *out) {
    dec_t *d = dec_of(dec);
    if (!ready(d) || !out || idx >= (size_t)d->m.nsigners) return errSecParam;
    int ci = signer_cert_index(&d->m, &d->m.signers[idx]);
    *out = ci >= 0 ? cert_of(d->m.certs[ci]) : 0;
    return *out ? errSecSuccess : errSecParam;
}
OSStatus CMSDecoderCopySignerEmailAddress(id dec, size_t idx, id *out) {
    if (!out) return errSecParam;
    *out = 0;
    id c = 0;
    OSStatus st = CMSDecoderCopySignerCert(dec, idx, &c);
    if (st) return st;
    id list = 0;
    if (!SecCertificateCopyEmailAddresses(c, &list) && list && CFArrayGetCount(list)) *out = CFRetain(CFArrayGetValueAtIndex(list, 0));
    if (list) CFRelease(list);
    CFRelease(c);
    return errSecSuccess;
}
static void digest_of(int dl, const u8 *p, unsigned long n, u8 *out) {
    if (dl == 20) CC_SHA1(p, (u32)n, out);
    else if (dl == 48) CC_SHA384(p, (u32)n, out);
    else if (dl == 64) CC_SHA512(p, (u32)n, out);
    else CC_SHA256(p, (u32)n, out);
}
static id alg_for(id key, int dl) {
    const void *rsa = dl == 20 ? kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1 : dl == 48 ? kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384
                    : dl == 64 ? kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512 : kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256;
    const void *ecd = dl == 20 ? kSecKeyAlgorithmECDSASignatureMessageX962SHA1 : dl == 48 ? kSecKeyAlgorithmECDSASignatureMessageX962SHA384
                    : dl == 64 ? kSecKeyAlgorithmECDSASignatureMessageX962SHA512 : kSecKeyAlgorithmECDSASignatureMessageX962SHA256;
    return SecKeyIsAlgorithmSupported(key, 1 /* verify */, (id)rsa) ? (id)rsa : (id)ecd;
}
OSStatus CMSDecoderCopySignerStatus(id dec, size_t idx, id policy, Boolean evaluate, int *status, id *trust_out, OSStatus *cert_rc) {
    dec_t *d = dec_of(dec);
    if (status) *status = kCMSSignerInvalidIndex;
    if (trust_out) *trust_out = 0;
    if (cert_rc) *cert_rc = 0;
    if (!ready(d)) return errSecParam;
    if (idx >= (size_t)d->m.nsigners) return errSecSuccess;
    cms_signer *s = &d->m.signers[idx];
    const u8 *data = d->m.content.p;
    unsigned long dn = d->m.content.n;
    if (!d->m.has_content) {
        if (!d->detached) { if (status) *status = kCMSSignerNeedsDetachedContent; return errSecSuccess; }
        data = CFDataGetBytePtr(d->detached); dn = (unsigned long)CFDataGetLength(d->detached);
    }
    int dl = cms_digest_len(s->digest_oid);
    int ci = signer_cert_index(&d->m, s);
    int st = kCMSSignerInvalidSignature;
    id cert = ci >= 0 ? cert_of(d->m.certs[ci]) : 0;
    if (!cert) st = kCMSSignerInvalidCert;
    else if (dl) {
        u8 md[64];
        digest_of(dl, data, dn, md);
        u8 *signed_bytes = (u8 *)data;
        unsigned long sbn = dn;
        int ok = 1;
        if (s->signed_attrs.n) {
            ok = s->msg_digest.n == (unsigned long)dl && !memcmp(s->msg_digest.p, md, (size_t)dl);
            if (cms_attrs_to_set(s->signed_attrs, &signed_bytes, &sbn)) ok = 0;
        }
        id key = SecCertificateCopyKey(cert);
        if (ok && key) {
            id msg = CFDataCreate(0, signed_bytes, (long)sbn), sig = CFDataCreate(0, s->sig.p, (long)s->sig.n);
            ok = SecKeyVerifySignature(key, alg_for(key, dl), msg, sig, 0);
            CFRelease(msg); CFRelease(sig);
        } else ok = 0;
        if (key) CFRelease(key);
        if (signed_bytes != data) free(signed_bytes);
        st = ok ? kCMSSignerValid : kCMSSignerInvalidSignature;
    }
    /* confiança: o certificado do assinante primeiro, os outros da mensagem como intermediários */
    if (cert && (trust_out || evaluate)) {
        id certs = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
        CFArrayAppendValue(certs, cert);
        for (int i = 0; i < d->m.ncerts; i++) if (i != ci) { id c = cert_of(d->m.certs[i]); if (c) { CFArrayAppendValue(certs, c); CFRelease(c); } }
        id pol = policy ? CFRetain(policy) : SecPolicyCreateBasicX509(), t = 0;
        if (!SecTrustCreateWithCertificates(certs, pol, &t) && t) {
            if (evaluate) {
                id e = 0;
                if (!SecTrustEvaluateWithError(t, &e)) {
                    int rc = 0;
                    SecTrustGetCssmResultCode(t, &rc);
                    if (cert_rc) *cert_rc = rc;
                    if (st == kCMSSignerValid) st = kCMSSignerInvalidCert;
                }
                if (e) CFRelease(e);
            }
            if (trust_out) *trust_out = t; else CFRelease(t);
        }
        CFRelease(pol);
        CFRelease(certs);
    }
    if (cert) CFRelease(cert);
    if (status) *status = st;
    return errSecSuccess;
}
OSStatus CMSDecoderCopySignerSigningTime(id dec, size_t idx, double *t) {
    dec_t *d = dec_of(dec);
    if (!ready(d) || !t || idx >= (size_t)d->m.nsigners) return errSecParam;
    cms_signer *s = &d->m.signers[idx];
    if (!s->signing_time.n) return errSecSigningTimeMissing;
    *t = cms_der_time(s->signing_time, s->time_tag);
    return errSecSuccess;
}
/* carimbo RFC 3161: o token é um SignedData cujo conteúdo é o TSTInfo; genTime é o 5º campo */
static int tst_of(cms_signer *s, cms_msg *tm, double *gen) {
    if (!s->has_timestamp) return -1;
    u8 t;
    cms_span tok;
    cms_it it = cms_in(s->timestamp);
    if (cms_next(&it, &t, &tok, 0) || cms_parse(tok.p, tok.n, tm) || !tm->has_content) return -1;
    cms_span tst;
    if (cms_tlv(tm->content.p, tm->content.n, &t, 0, &tst) || t != 0x30) return -1;
    cms_it f = cms_in(tst);
    cms_span x;
    for (int i = 0; i < 5; i++) if (cms_next(&f, &t, 0, &x)) return -1;
    if (t != 0x18) return -1;
    if (gen) *gen = cms_der_time(x, t);
    return 0;
}
OSStatus CMSDecoderCopySignerTimestamp(id dec, size_t idx, double *t) {
    dec_t *d = dec_of(dec);
    if (!ready(d) || !t || idx >= (size_t)d->m.nsigners) return errSecParam;
    cms_msg tm;
    return tst_of(&d->m.signers[idx], &tm, t) ? errSecTimestampMissing : errSecSuccess;
}
OSStatus CMSDecoderCopySignerTimestampWithPolicy(id dec, id policy, size_t idx, double *t) { (void)policy; return CMSDecoderCopySignerTimestamp(dec, idx, t); }
OSStatus CMSDecoderCopySignerTimestampCertificates(id dec, size_t idx, id *out) {
    dec_t *d = dec_of(dec);
    if (!ready(d) || !out || idx >= (size_t)d->m.nsigners) return errSecParam;
    *out = 0;
    cms_msg tm;
    if (tst_of(&d->m.signers[idx], &tm, 0)) return errSecTimestampMissing;
    id a = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    for (int i = 0; i < tm.ncerts; i++) { id c = cert_of(tm.certs[i]); if (c) { CFArrayAppendValue(a, c); CFRelease(c); } }
    *out = a;
    return errSecSuccess;
}

/* ================================================================ codificador */
OSStatus CMSEncoderCreate(id *out) {
    if (!out) return errSecParam;
    boot();
    id o = cx_alloc_instance(&Enc_c);
    enc_t *e = calloc(1, sizeof *e);
    if (!o || !e) return errSecAllocate;
    e->chain = kCMSCertificateChain;
    ST(enc_t, o) = e;
    *out = o;
    return errSecSuccess;
}
static void add_items(id *dst, id one_or_array) {
    if (!*dst) *dst = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    if (CFGetTypeID(one_or_array) == CFArrayGetTypeID())
        for (long i = 0; i < CFArrayGetCount(one_or_array); i++) CFArrayAppendValue(*dst, CFArrayGetValueAtIndex(one_or_array, i));
    else CFArrayAppendValue(*dst, one_or_array);
}
static OSStatus copy_list(id src, id *out) { if (!out) return errSecParam; *out = src && CFArrayGetCount(src) ? CFRetain(src) : 0; return errSecSuccess; }
OSStatus CMSEncoderAddSigners(id enc, id s) { enc_t *e = enc_of(enc); if (!e || !s) return errSecParam; add_items(&e->signers, s); return errSecSuccess; }
OSStatus CMSEncoderCopySigners(id enc, id *out) { enc_t *e = enc_of(enc); return e ? copy_list(e->signers, out) : errSecParam; }
OSStatus CMSEncoderAddSupportingCerts(id enc, id c) { enc_t *e = enc_of(enc); if (!e || !c) return errSecParam; add_items(&e->certs, c); return errSecSuccess; }
OSStatus CMSEncoderCopySupportingCerts(id enc, id *out) { enc_t *e = enc_of(enc); return e ? copy_list(e->certs, out) : errSecParam; }
/* cifra para destinatários (EnvelopedData): ainda não existe no mOS */
OSStatus CMSEncoderAddRecipients(id enc, id r) { (void)r; return enc_of(enc) ? errSecUnimplemented : errSecParam; }
OSStatus CMSEncoderCopyRecipients(id enc, id *out) { enc_t *e = enc_of(enc); return e ? copy_list(e->recips, out) : errSecParam; }
OSStatus CMSEncoderSetHasDetachedContent(id enc, Boolean det) { enc_t *e = enc_of(enc); if (!e) return errSecParam; e->detached = det != 0; return errSecSuccess; }
OSStatus CMSEncoderGetHasDetachedContent(id enc, Boolean *det) { enc_t *e = enc_of(enc); if (!e || !det) return errSecParam; *det = (Boolean)e->detached; return errSecSuccess; }
OSStatus CMSEncoderSetCertificateChainMode(id enc, u32 mode) { enc_t *e = enc_of(enc); if (!e || mode > 4) return errSecParam; e->chain = (int)mode; return errSecSuccess; }
OSStatus CMSEncoderGetCertificateChainMode(id enc, u32 *mode) { enc_t *e = enc_of(enc); if (!e || !mode) return errSecParam; *mode = (u32)e->chain; return errSecSuccess; }
OSStatus CMSEncoderAddSignedAttributes(id enc, u32 attrs) { enc_t *e = enc_of(enc); if (!e) return errSecParam; e->attrs |= attrs; return errSecSuccess; }
/* "sha1" ou "sha256" (kCMSEncoderDigestAlgorithmSHA1/SHA256) */
OSStatus CMSEncoderSetSignerAlgorithm(id enc, id alg) {
    enc_t *e = enc_of(enc);
    const char *a = alg && cx_is_str(alg) ? cx_cstr(alg) : 0;
    if (!e || !a) return errSecParam;
    if (!strcmp(a, "sha1")) e->sha1 = 1; else if (!strcmp(a, "sha256")) e->sha1 = 0; else return errSecParam;
    return errSecSuccess;
}
/* OID do conteúdo: CFData com o DER do OID (sem tag) ou CFString pontilhada */
static int oid_der(const char *dot, u8 *out, int cap) {
    unsigned long v[32];
    int n = 0;
    for (const char *p = dot; *p && n < 32;) { v[n++] = strtoul(p, (char **)&p, 10); if (*p == '.') p++; else if (*p) return -1; }
    if (n < 2) return -1;
    int k = 0;
    out[k++] = (u8)(v[0] * 40 + v[1]);
    for (int i = 2; i < n; i++) {
        u8 t[10];
        int m = 0;
        unsigned long x = v[i];
        do { t[m++] = (u8)(x & 0x7f); x >>= 7; } while (x);
        if (k + m > cap) return -1;
        while (m) { m--; out[k++] = (u8)(t[m] | (m ? 0x80 : 0)); }
    }
    return k;
}
OSStatus CMSEncoderSetEncapsulatedContentTypeOID(id enc, id oid) {
    enc_t *e = enc_of(enc);
    if (!e || !oid) return errSecParam;
    id d = 0;
    if (cx_is_str(oid)) {
        u8 b[64];
        int n = oid_der(cx_cstr(oid), b, sizeof b);
        if (n < 0) return errSecParam;
        d = CFDataCreate(0, b, n);
    } else if (CFGetTypeID(oid) == CFDataGetTypeID()) d = CFRetain(oid);
    else return errSecParam;
    if (e->ctype) CFRelease(e->ctype);
    e->ctype = d;
    return errSecSuccess;
}
OSStatus CMSEncoderCopyEncapsulatedContentType(id enc, id *out) {
    enc_t *e = enc_of(enc);
    if (!e || !out) return errSecParam;
    *out = e->ctype ? CFRetain(e->ctype) : CFDataCreate(0, OID_DATA, sizeof OID_DATA);
    return errSecSuccess;
}
OSStatus CMSEncoderUpdateContent(id enc, const void *b, size_t n) {
    enc_t *e = enc_of(enc);
    if (!e || (!b && n)) return errSecParam;
    cms_put(&e->content, b, n);
    return e->content.err ? errSecAllocate : errSecSuccess;
}
static void utc_now(char out[14]) {
    long s = (long)(CFAbsoluteTimeGetCurrent() + 978307200.0);
    long days = s / 86400, r = s % 86400;
    days += 719468;
    long era = days / 146097, doe = days - era * 146097, yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    long d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2) y++;
    snprintf(out, 14, "%02ld%02ld%02ld%02ld%02ld%02ldZ", y % 100, m, d, r / 3600, r / 60 % 60, r % 60);
}
/* SignedData com um assinante (o uso comum: assinar um perfil, um recibo, um e-mail). Mais de um assinante numa mesma
 * mensagem ainda não: errSecUnimplemented. */
OSStatus CMSEncoderCopyEncodedContent(id enc, id *out) {
    enc_t *e = enc_of(enc);
    if (!e || !out) return errSecParam;
    *out = 0;
    if (!e->signers || !CFArrayGetCount(e->signers)) return errSecParam;      /* sem assinante nem destinatário */
    if (CFArrayGetCount(e->signers) > 1) return errSecUnimplemented;
    id ident = (id)CFArrayGetValueAtIndex(e->signers, 0), cert = 0, key = 0;
    if (SecIdentityCopyCertificate(ident, &cert) || SecIdentityCopyPrivateKey(ident, &key)) { if (cert) CFRelease(cert); return errSecParam; }
    id cder = SecCertificateCopyData(cert);
    const u8 *sj, *is, *se;
    u32 sl, il, sn;
    char lab[8];
    OSStatus st = errSecParam;
    if (cder && !cx_cert_fields(CFDataGetBytePtr(cder), (u32)CFDataGetLength(cder), &sj, &sl, &is, &il, &se, &sn, lab, sizeof lab)) {
        int dl = e->sha1 ? 20 : 32;
        u8 md[32];
        digest_of(dl, e->content.b ? e->content.b : (const u8 *)"", e->content.n, md);
        const u8 *ct = e->ctype ? CFDataGetBytePtr(e->ctype) : OID_DATA;
        unsigned long ctn = e->ctype ? (unsigned long)CFDataGetLength(e->ctype) : sizeof OID_DATA;
        char utc[14];
        utc_now(utc);
        cms_buf set = {0};
        if (!cms_signed_attrs(&set, ct, ctn, utc, md, (unsigned long)dl)) {
            int rsa = SecKeyIsAlgorithmSupported(key, 0 /* sign */, (id)(e->sha1 ? kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1 : kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256));
            id alg = rsa ? (id)(e->sha1 ? kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1 : kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256)
                         : (id)(e->sha1 ? kSecKeyAlgorithmECDSASignatureMessageX962SHA1 : kSecKeyAlgorithmECDSASignatureMessageX962SHA256);
            id msg = CFDataCreate(0, set.b, (long)set.n);
            id sig = SecKeyCreateSignature(key, alg, msg, 0);
            CFRelease(msg);
            if (sig) {
                static const u8 ECDSA_SHA1[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x01};
                cms_span certs[CMS_MAX_CERTS];
                id keep[CMS_MAX_CERTS];
                int nc = 0;
                if (e->chain != kCMSCertificateNone) { certs[nc] = (cms_span){CFDataGetBytePtr(cder), (unsigned long)CFDataGetLength(cder)}; keep[nc++] = 0; }
                for (long i = 0; e->chain >= kCMSCertificateChain && e->certs && i < CFArrayGetCount(e->certs) && nc < CMS_MAX_CERTS; i++) {
                    id d = SecCertificateCopyData((id)CFArrayGetValueAtIndex(e->certs, i));
                    if (d) { certs[nc] = (cms_span){CFDataGetBytePtr(d), (unsigned long)CFDataGetLength(d)}; keep[nc++] = d; }
                }
                cms_span sa = {set.b, set.n}, iss = {is, il}, ser = {se, sn};
                cms_buf o = {0};
                if (!cms_build(&o, e->content.b ? e->content.b : (const u8 *)"", e->content.n, e->detached, ct, ctn,
                               e->sha1 ? OID_SHA1 : OID_SHA256, e->sha1 ? sizeof OID_SHA1 : sizeof OID_SHA256,
                               rsa ? OID_RSA : e->sha1 ? ECDSA_SHA1 : OID_ECDSA_SHA256,
                               rsa ? sizeof OID_RSA : e->sha1 ? sizeof ECDSA_SHA1 : sizeof OID_ECDSA_SHA256, rsa, &sa,
                               CFDataGetBytePtr(sig), (unsigned long)CFDataGetLength(sig), certs, nc, iss, ser)) {
                    *out = CFDataCreate(0, o.b, (long)o.n);
                    st = *out ? errSecSuccess : errSecAllocate;
                }
                free(o.b);
                for (int i = 0; i < nc; i++) if (keep[i]) CFRelease(keep[i]);
                CFRelease(sig);
            }
        }
        free(set.b);
    }
    if (cder) CFRelease(cder);
    CFRelease(cert);
    CFRelease(key);
    return st;
}
OSStatus CMSEncoderCopySignerTimestamp(id enc, size_t idx, double *t) { (void)idx; (void)t; return enc_of(enc) ? errSecTimestampMissing : errSecParam; }
OSStatus CMSEncoderCopySignerTimestampWithPolicy(id enc, id pol, size_t idx, double *t) { (void)pol; return CMSEncoderCopySignerTimestamp(enc, idx, t); }
/* de uma vez só */
OSStatus CMSEncodeContent(id signers, id recipients, id ctype, Boolean detached, u32 attrs, const void *content, size_t n, id *out) {
    if (!out) return errSecParam;
    *out = 0;
    id enc = 0;
    OSStatus st = CMSEncoderCreate(&enc);
    if (st) return st;
    if (signers) st = CMSEncoderAddSigners(enc, signers);
    if (!st && recipients) st = CMSEncoderAddRecipients(enc, recipients);
    if (!st && ctype) st = CMSEncoderSetEncapsulatedContentTypeOID(enc, ctype);
    if (!st) st = CMSEncoderSetHasDetachedContent(enc, detached);
    if (!st) st = CMSEncoderAddSignedAttributes(enc, attrs);
    if (!st) st = CMSEncoderUpdateContent(enc, content, n);
    if (!st) st = CMSEncoderCopyEncodedContent(enc, out);
    CFRelease(enc);
    return st;
}
