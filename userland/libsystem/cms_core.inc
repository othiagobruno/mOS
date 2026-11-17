/*
 * mOS libSystem — núcleo do CMS/PKCS#7 (RFC 5652) sem CF: lê ContentInfo/SignedData (conteúdo, certificados,
 * assinantes com os atributos assinados) e monta um SignedData com um assinante. A assinatura e a verificação de
 * chave ficam com quem inclui (cms.c usa SecKey; o teste de host usa kernel/net/pk.c). Incluído por cms.c e por
 * tools/cms_host.c. Quem inclui já declarou malloc/realloc/free/memcpy/memcmp/memset e os tipos u8/u32.
 */
typedef struct { const u8 *p; unsigned long n; } cms_span;
typedef struct {
    cms_span signed_attrs;      /* o elemento [0] inteiro (tag + tamanho + conteúdo) */
    cms_span sig, digest_oid, sig_oid;
    cms_span issuer, serial, ski;  /* identificação do assinante: emissor+série ou [0] SubjectKeyIdentifier */
    cms_span msg_digest, signing_time, content_type;   /* signing_time: o conteúdo; time_tag: 0x17 UTCTime ou 0x18 */
    u8 time_tag;
    int has_timestamp;          /* atributo não assinado id-aa-timeStampToken */
    cms_span timestamp;
} cms_signer;
#define CMS_MAX_CERTS 32
#define CMS_MAX_SIGNERS 8
typedef struct {
    cms_span content_type;      /* OID do ContentInfo externo */
    cms_span econtent_type, content;
    int has_content, encrypted;
    cms_span certs[CMS_MAX_CERTS];
    int ncerts;
    cms_signer signers[CMS_MAX_SIGNERS];
    int nsigners;
} cms_msg;

static const u8 OID_SIGNED_DATA[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x02};
static const u8 OID_DATA[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01};
static const u8 OID_ENVELOPED[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x03};
static const u8 OID_ATTR_CTYPE[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x03};
static const u8 OID_ATTR_DIGEST[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x04};
static const u8 OID_ATTR_TIME[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x05};
static const u8 OID_ATTR_TST[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x10, 0x02, 0x0e};
static const u8 OID_SHA1[] = {0x2b, 0x0e, 0x03, 0x02, 0x1a};
static const u8 OID_SHA256[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01};
static const u8 OID_SHA384[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02};
static const u8 OID_SHA512[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03};
static const u8 OID_RSA[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
static const u8 OID_ECDSA_SHA256[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02};

static int cms_eq(cms_span s, const u8 *o, unsigned long n) { return s.n == n && !memcmp(s.p, o, n); }
/* um TLV: devolve 0 e preenche tag, o elemento inteiro e o conteúdo */
static int cms_tlv(const u8 *p, unsigned long n, u8 *tag, cms_span *all, cms_span *val) {
    if (n < 2) return -1;
    unsigned long len = p[1], h = 2;
    if (len & 0x80) {
        int k = (int)(len & 0x7f);
        if (k < 1 || k > 4 || n < 2 + (unsigned long)k) return -1;          /* sem tamanho indefinido (BER) */
        len = 0;
        for (int i = 0; i < k; i++) len = len << 8 | p[2 + i];
        h = 2 + (unsigned long)k;
    }
    if (h + len > n) return -1;
    *tag = p[0];
    if (all) { all->p = p; all->n = h + len; }
    if (val) { val->p = p + h; val->n = len; }
    return 0;
}
/* percorre os filhos de um construído */
typedef struct { const u8 *p, *e; } cms_it;
static int cms_next(cms_it *it, u8 *tag, cms_span *all, cms_span *val) {
    if (it->p >= it->e) return -1;
    cms_span a;
    if (cms_tlv(it->p, (unsigned long)(it->e - it->p), tag, &a, val)) return -1;
    if (all) *all = a;
    it->p += a.n;
    return 0;
}
static cms_it cms_in(cms_span v) { cms_it it = {v.p, v.p + v.n}; return it; }

static int cms_parse_signer(cms_span v, cms_signer *s) {
    cms_it it = cms_in(v);
    u8 t;
    cms_span a, x;
    memset(s, 0, sizeof *s);
    if (cms_next(&it, &t, 0, &x) || t != 0x02) return -1;                     /* version */
    if (cms_next(&it, &t, &a, &x)) return -1;                                 /* sid */
    if (t == 0x30) {
        cms_it j = cms_in(x);
        u8 t2;
        cms_span iss, ser;
        if (cms_next(&j, &t2, &iss, 0) || t2 != 0x30 || cms_next(&j, &t2, 0, &ser) || t2 != 0x02) return -1;
        s->issuer = iss; s->serial = ser;
    } else if (t == 0x80) s->ski = x;
    else return -1;
    if (cms_next(&it, &t, 0, &x) || t != 0x30) return -1;                     /* digestAlgorithm */
    { cms_it j = cms_in(x); u8 t2; if (cms_next(&j, &t2, 0, &s->digest_oid) || t2 != 0x06) return -1; }
    if (cms_next(&it, &t, &a, &x)) return -1;
    if (t == 0xa0) {                                                          /* signedAttrs */
        s->signed_attrs = a;
        cms_it j = cms_in(x);
        u8 t2;
        cms_span attr;
        while (!cms_next(&j, &t2, 0, &attr)) {
            cms_it k = cms_in(attr);
            u8 t3;
            cms_span oid, set;
            if (cms_next(&k, &t3, 0, &oid) || t3 != 0x06 || cms_next(&k, &t3, 0, &set) || t3 != 0x31) return -1;
            cms_it m = cms_in(set);
            u8 t4;
            cms_span first;
            if (cms_next(&m, &t4, 0, &first)) continue;
            if (cms_eq(oid, OID_ATTR_DIGEST, sizeof OID_ATTR_DIGEST) && t4 == 0x04) s->msg_digest = first;
            else if (cms_eq(oid, OID_ATTR_TIME, sizeof OID_ATTR_TIME) && (t4 == 0x17 || t4 == 0x18)) { s->signing_time = first; s->time_tag = t4; }
            else if (cms_eq(oid, OID_ATTR_CTYPE, sizeof OID_ATTR_CTYPE) && t4 == 0x06) s->content_type = first;
        }
        if (cms_next(&it, &t, 0, &x)) return -1;
    }
    if (t != 0x30) return -1;                                                 /* signatureAlgorithm */
    { cms_it j = cms_in(x); u8 t2; if (cms_next(&j, &t2, 0, &s->sig_oid) || t2 != 0x06) return -1; }
    if (cms_next(&it, &t, 0, &s->sig) || t != 0x04) return -1;
    if (!cms_next(&it, &t, 0, &x) && t == 0xa1) {                             /* unsignedAttrs */
        cms_it j = cms_in(x);
        u8 t2;
        cms_span attr;
        while (!cms_next(&j, &t2, 0, &attr)) {
            cms_it k = cms_in(attr);
            u8 t3;
            cms_span oid, set;
            if (cms_next(&k, &t3, 0, &oid) || cms_next(&k, &t3, 0, &set)) break;
            if (cms_eq(oid, OID_ATTR_TST, sizeof OID_ATTR_TST)) { s->has_timestamp = 1; s->timestamp = set; }
        }
    }
    return 0;
}

/* 0 = ok; -1 = DER inválido; -2 = não é SignedData nem EnvelopedData */
static int cms_parse(const u8 *der, unsigned long n, cms_msg *m) {
    u8 t;
    cms_span ci, x;
    memset(m, 0, sizeof *m);
    if (cms_tlv(der, n, &t, &ci, &x) || t != 0x30) return -1;
    cms_it it = cms_in(x);
    if (cms_next(&it, &t, 0, &m->content_type) || t != 0x06) return -1;
    if (cms_eq(m->content_type, OID_ENVELOPED, sizeof OID_ENVELOPED)) { m->encrypted = 1; return 0; }
    if (!cms_eq(m->content_type, OID_SIGNED_DATA, sizeof OID_SIGNED_DATA)) return -2;
    cms_span exp, sd;
    if (cms_next(&it, &t, 0, &exp) || t != 0xa0) return -1;
    cms_it e = cms_in(exp);
    if (cms_next(&e, &t, 0, &sd) || t != 0x30) return -1;
    cms_it s = cms_in(sd);
    if (cms_next(&s, &t, 0, &x) || t != 0x02) return -1;                      /* version */
    if (cms_next(&s, &t, 0, &x) || t != 0x31) return -1;                      /* digestAlgorithms */
    if (cms_next(&s, &t, 0, &x) || t != 0x30) return -1;                      /* encapContentInfo */
    {
        cms_it j = cms_in(x);
        u8 t2;
        cms_span ec;
        if (cms_next(&j, &t2, 0, &m->econtent_type) || t2 != 0x06) return -1;
        if (!cms_next(&j, &t2, 0, &ec) && t2 == 0xa0) {
            cms_it k = cms_in(ec);
            u8 t3;
            cms_span oct;
            if (cms_next(&k, &t3, 0, &oct)) return -1;
            if (t3 == 0x04) { m->content = oct; m->has_content = 1; }
            else if (t3 == 0x24) return -1;                                   /* OCTET STRING construído (BER): não */
            else { m->content = ec; m->has_content = 1; }                     /* PKCS#7 v1.5: conteúdo como o tipo dele */
        }
    }
    while (!cms_next(&s, &t, 0, &x)) {
        if (t == 0xa0) {                                                      /* certificates */
            cms_it j = cms_in(x);
            u8 t2;
            cms_span c;
            while (!cms_next(&j, &t2, &c, 0)) if (t2 == 0x30 && m->ncerts < CMS_MAX_CERTS) m->certs[m->ncerts++] = c;
        } else if (t == 0xa1) {
            continue;                                                         /* crls */
        } else if (t == 0x31) {                                               /* signerInfos */
            cms_it j = cms_in(x);
            u8 t2;
            cms_span si;
            while (!cms_next(&j, &t2, 0, &si))
                if (t2 == 0x30 && m->nsigners < CMS_MAX_SIGNERS && !cms_parse_signer(si, &m->signers[m->nsigners])) m->nsigners++;
        }
    }
    return 0;
}

/* ---- montagem (DER) num buffer que cresce */
typedef struct { u8 *b; unsigned long n, cap; int err; } cms_buf;
static void cms_put(cms_buf *o, const void *p, unsigned long n) {
    if (o->err) return;
    if (o->n + n > o->cap) {
        unsigned long c = (o->cap + n) * 2 + 64;
        u8 *nb = realloc(o->b, c);
        if (!nb) { o->err = 1; return; }
        o->b = nb; o->cap = c;
    }
    memcpy(o->b + o->n, p, n);
    o->n += n;
}
static void cms_hdr(cms_buf *o, u8 tag, unsigned long len) {
    u8 h[6];
    int k = 0;
    h[k++] = tag;
    if (len < 128) h[k++] = (u8)len;
    else if (len < 256) { h[k++] = 0x81; h[k++] = (u8)len; }
    else if (len < 65536) { h[k++] = 0x82; h[k++] = (u8)(len >> 8); h[k++] = (u8)len; }
    else { h[k++] = 0x83; h[k++] = (u8)(len >> 16); h[k++] = (u8)(len >> 8); h[k++] = (u8)len; }
    cms_put(o, h, (unsigned long)k);
}
static void cms_tlvw(cms_buf *o, u8 tag, const void *p, unsigned long n) { cms_hdr(o, tag, n); cms_put(o, p, n); }
static void cms_wrap(cms_buf *o, u8 tag, cms_buf *in) { cms_tlvw(o, tag, in->b, in->n); if (in->err) o->err = 1; free(in->b); in->b = 0; in->n = in->cap = 0; }
static int cms_cmp_der(const void *a, const void *b) {
    const cms_span *x = a, *y = b;
    unsigned long n = x->n < y->n ? x->n : y->n;
    int r = memcmp(x->p, y->p, n);
    return r ? r : (x->n < y->n ? -1 : x->n > y->n);
}

/* atributos assinados (SET OF em ordem DER): contentType, signingTime (UTCTime "AAMMDDhhmmssZ"), messageDigest.
 * Devolve o SET com a tag 0x31 (o que se assina); no SignerInfo ele vai com a tag [0] (0xa0). */
static int cms_signed_attrs(cms_buf *out, const u8 *ctype, unsigned long ctn, const char *utc, const u8 *digest, unsigned long dn) {
    cms_buf a[3] = {{0}, {0}, {0}}, t = {0};
    cms_tlvw(&t, 0x06, OID_ATTR_CTYPE, sizeof OID_ATTR_CTYPE);
    { cms_buf v = {0}; cms_tlvw(&v, 0x06, ctype, ctn); cms_wrap(&t, 0x31, &v); }
    cms_wrap(&a[0], 0x30, &t);
    cms_tlvw(&t, 0x06, OID_ATTR_TIME, sizeof OID_ATTR_TIME);
    { cms_buf v = {0}; cms_tlvw(&v, 0x17, utc, 13); cms_wrap(&t, 0x31, &v); }
    cms_wrap(&a[1], 0x30, &t);
    cms_tlvw(&t, 0x06, OID_ATTR_DIGEST, sizeof OID_ATTR_DIGEST);
    { cms_buf v = {0}; cms_tlvw(&v, 0x04, digest, dn); cms_wrap(&t, 0x31, &v); }
    cms_wrap(&a[2], 0x30, &t);
    cms_span s[3] = {{a[0].b, a[0].n}, {a[1].b, a[1].n}, {a[2].b, a[2].n}};
    /* ordena os 3 por bytes DER (inserção) */
    for (int i = 1; i < 3; i++) for (int j = i; j > 0 && cms_cmp_der(&s[j - 1], &s[j]) > 0; j--) { cms_span x = s[j]; s[j] = s[j - 1]; s[j - 1] = x; }
    cms_buf set = {0};
    for (int i = 0; i < 3; i++) cms_put(&set, s[i].p, s[i].n);
    cms_wrap(out, 0x31, &set);
    for (int i = 0; i < 3; i++) free(a[i].b);
    return out->err ? -1 : 0;
}
/* SignedData de um assinante. sattrs = o SET (0x31) de cms_signed_attrs; certs[0] = o do assinante.
 * issuer = o Name DER do emissor do certificado do assinante; serial = o conteúdo do INTEGER. */
static int cms_build(cms_buf *out, const u8 *content, unsigned long cn, int detached, const u8 *ctype, unsigned long ctn,
                     const u8 *dig_oid, unsigned long don, const u8 *sig_oid, unsigned long son, int sig_null_params,
                     const cms_span *sattrs, const u8 *sig, unsigned long sn, const cms_span *certs, int ncerts,
                     cms_span issuer, cms_span serial) {
    cms_buf sd = {0}, x = {0}, y = {0};
    static const u8 v1 = 1, nul[2] = {0x05, 0x00};
    cms_tlvw(&sd, 0x02, &v1, 1);
    cms_tlvw(&y, 0x06, dig_oid, don);
    cms_wrap(&x, 0x30, &y);
    cms_wrap(&sd, 0x31, &x);                                                  /* digestAlgorithms */
    cms_tlvw(&x, 0x06, ctype, ctn);
    if (!detached) { cms_tlvw(&y, 0x04, content, cn); cms_wrap(&x, 0xa0, &y); }
    cms_wrap(&sd, 0x30, &x);                                                  /* encapContentInfo */
    for (int i = 0; i < ncerts; i++) cms_put(&x, certs[i].p, certs[i].n);
    if (ncerts) cms_wrap(&sd, 0xa0, &x); else free(x.b), x.b = 0, x.n = x.cap = 0;
    /* SignerInfo */
    cms_buf si = {0};
    cms_tlvw(&si, 0x02, &v1, 1);
    cms_put(&x, issuer.p, issuer.n);
    cms_tlvw(&x, 0x02, serial.p, serial.n);
    cms_wrap(&si, 0x30, &x);                                                  /* IssuerAndSerialNumber */
    cms_tlvw(&y, 0x06, dig_oid, don);
    cms_wrap(&si, 0x30, &y);
    cms_hdr(&si, 0xa0, sattrs->n - (sattrs->p[1] & 0x80 ? 2 + (sattrs->p[1] & 0x7f) : 2));
    cms_put(&si, sattrs->p + (sattrs->p[1] & 0x80 ? 2 + (sattrs->p[1] & 0x7f) : 2),
            sattrs->n - (sattrs->p[1] & 0x80 ? 2 + (sattrs->p[1] & 0x7f) : 2));   /* [0] IMPLICIT */
    cms_tlvw(&y, 0x06, sig_oid, son);
    if (sig_null_params) cms_put(&y, nul, 2);
    cms_wrap(&si, 0x30, &y);
    cms_tlvw(&si, 0x04, sig, sn);
    cms_wrap(&x, 0x30, &si);
    cms_wrap(&sd, 0x31, &x);                                                  /* signerInfos */
    cms_buf ci = {0}, sdw = {0};
    cms_wrap(&sdw, 0x30, &sd);
    cms_tlvw(&ci, 0x06, OID_SIGNED_DATA, sizeof OID_SIGNED_DATA);
    cms_wrap(&ci, 0xa0, &sdw);
    cms_wrap(out, 0x30, &ci);
    return out->err ? -1 : 0;
}
/* o que se assina com atributos: o elemento [0] com a tag trocada para SET (0x31) */
static int cms_attrs_to_set(cms_span a, u8 **out, unsigned long *n) {
    *out = malloc(a.n ? a.n : 1);
    if (!*out) return -1;
    memcpy(*out, a.p, a.n);
    (*out)[0] = 0x31;
    *n = a.n;
    return 0;
}
/* digestAlgorithm -> tamanho do resumo (0 = desconhecido) */
static int cms_digest_len(cms_span oid) {
    if (cms_eq(oid, OID_SHA1, sizeof OID_SHA1)) return 20;
    if (cms_eq(oid, OID_SHA256, sizeof OID_SHA256)) return 32;
    if (cms_eq(oid, OID_SHA384, sizeof OID_SHA384)) return 48;
    if (cms_eq(oid, OID_SHA512, sizeof OID_SHA512)) return 64;
    return 0;
}

/* UTCTime "AAMMDDhhmmssZ" ou GeneralizedTime "AAAAMMDDhhmmss[.fff]Z" -> CFAbsoluteTime */
static double cms_der_time(cms_span t, u8 tag) {
    const u8 *p = t.p;
    int i = 0, y;
    if (tag == 0x17) { if (t.n < 13) return 0; y = (p[0] - '0') * 10 + (p[1] - '0'); y += y < 50 ? 2000 : 1900; i = 2; }
    else { if (t.n < 15) return 0; y = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0'); i = 4; }
#define D2(k) ((p[i + (k)] - '0') * 10 + (p[i + (k) + 1] - '0'))
    int mo = D2(0), da = D2(2), h = D2(4), mi = D2(6), se = D2(8);
#undef D2
    /* dias desde 1/1/1970 (Howard Hinnant) */
    int yy = y - (mo <= 2);
    long era = (yy >= 0 ? yy : yy - 399) / 400;
    long yoe = yy - era * 400, doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + da - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long days = era * 146097 + doe - 719468;
    double unix_ = (double)days * 86400 + h * 3600 + mi * 60 + se;
    return unix_ - 978307200.0;
}
