/* mOS — certificados X.509 (RFC 5280) para autenticar servidores TLS.
 *
 *   - parser ASN.1 DER mínimo (tags de 1 byte, comprimentos definidos até 4 bytes)
 *   - X.509 v1/v3: tbsCertificate, emissor/sujeito (DER bruto, comparados byte a byte),
 *     validade (UTCTime/GeneralizedTime), SubjectPublicKeyInfo (rsaEncryption,
 *     id-ecPublicKey com P-256/P-384), extensões basicConstraints, keyUsage,
 *     extKeyUsage, subjectAltName (dNSName, iPAddress). nameConstraints e políticas
 *     são reconhecidas mas ignoradas; outra extensão crítica desconhecida reprova.
 *   - lista de confiança: bundle PEM da Mozilla (assets/certs/cacert.pem, embutido em
 *     kernel/arch/certs.S), decodificado e indexado (hash do sujeito) no primeiro uso.
 *   - cadeia: busca em profundidade folha -> intermediários (em qualquer ordem, como
 *     vieram na mensagem Certificate) -> raiz da lista; verifica assinaturas, datas,
 *     CA/keyUsage/pathLen dos emissores e, por fim, o nome do host (SAN).
 *
 *   - OCSP grampeado (x509_ocsp_check): CertID (SHA-1/SHA-2), assinatura do emissor ou de
 *     responder delegado com id-kp-OCSPSigning, thisUpdate/nextUpdate.
 *
 * Limitações: sem CRL nem consulta OCSP ativa (só o grampeado), sem Certificate Transparency, sem checagem de
 * políticas nem de nameConstraints; nomes comparados por bytes (sem normalização).
 */
#include "x509.h"

#define MAX_CHAIN 10
#define MAX_DEPTH 8

/* ======================================================================
 * DER
 * ====================================================================== */
typedef struct { const u8 *p, *end; } dr_t;
#define DLEN(d) ((u32)((d).end - (d).p))

static bool dr_next(dr_t *r, u8 *tag, dr_t *val, const u8 **start) {
    if (r->end - r->p < 2) return false;
    const u8 *s = r->p;
    u8 t = r->p[0];
    if ((t & 0x1f) == 0x1f) return false;          /* tags de vários bytes: não usadas em X.509 */
    u32 len = r->p[1];
    const u8 *q = r->p + 2;
    if (len & 0x80) {
        u32 nb = len & 0x7f;
        if (nb == 0 || nb > 4 || (u32)(r->end - q) < nb) return false;   /* sem comprimento indefinido */
        len = 0;
        for (u32 i = 0; i < nb; i++) len = len << 8 | *q++;
    }
    if ((u32)(r->end - q) < len) return false;
    val->p = q;
    val->end = q + len;
    r->p = q + len;
    *tag = t;
    if (start) *start = s;
    return true;
}

static bool dr_expect(dr_t *r, u8 tag, dr_t *val, const u8 **start) {
    dr_t save = *r;
    u8 t;
    if (!dr_next(r, &t, val, start) || t != tag) { *r = save; return false; }
    return true;
}

static bool dr_peek(const dr_t *r, u8 tag) { return r->p < r->end && r->p[0] == tag; }

/* INTEGER positivo sem os zeros à esquerda */
static bool dr_uint(dr_t *r, const u8 **p, u32 *len) {
    dr_t v;
    if (!dr_expect(r, 0x02, &v, NULL) || !DLEN(v) || (v.p[0] & 0x80)) return false;
    while (DLEN(v) > 1 && !v.p[0]) v.p++;
    *p = v.p;
    *len = DLEN(v);
    return true;
}

/* ---------------------------------------------------------------- OIDs */
#define OID(name, ...) static const u8 name[] = {__VA_ARGS__}
OID(OID_RSA,        0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01);
OID(OID_RSA_SHA256, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b);
OID(OID_RSA_SHA384, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0c);
OID(OID_RSA_SHA512, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0d);
OID(OID_RSA_PSS,    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0a);
OID(OID_MGF1,       0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x08);
OID(OID_SHA256,     0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01);
OID(OID_SHA384,     0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02);
OID(OID_SHA512,     0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03);
OID(OID_EC_PUB,     0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01);
OID(OID_P256,       0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07);
OID(OID_P384,       0x2b, 0x81, 0x04, 0x00, 0x22);
OID(OID_ECDSA_SHA256, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02);
OID(OID_ECDSA_SHA384, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x03);
OID(OID_ECDSA_SHA512, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x04);
OID(OID_SERVER_AUTH, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01);
OID(OID_OCSP_SIGNING, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x09);
OID(OID_OCSP_BASIC, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x01);
OID(OID_SHA1,       0x2b, 0x0e, 0x03, 0x02, 0x1a);
OID(OID_ANY_EKU,    0x55, 0x1d, 0x25, 0x00);
OID(OID_CN,         0x55, 0x04, 0x03);
OID(OID_O,          0x55, 0x04, 0x0a);

static bool oid_is(dr_t v, const u8 *oid, u32 len) { return DLEN(v) == len && !memcmp(v.p, oid, len); }
#define OID_IS(v, o) oid_is(v, o, sizeof(o))

static int hash_from_oid(dr_t v) {
    if (OID_IS(v, OID_SHA256)) return HASH_SHA256;
    if (OID_IS(v, OID_SHA384)) return HASH_SHA384;
    if (OID_IS(v, OID_SHA512)) return HASH_SHA512;
    return 0;
}

/* AlgorithmIdentifier de assinatura. Algoritmo desconhecido NÃO é erro de parse. */
static bool parse_sigalg(dr_t alg, int *kind, int *halg, int *salt) {
    dr_t oid;
    *kind = X509_SIG_NONE; *halg = 0; *salt = -1;
    if (!dr_expect(&alg, 0x06, &oid, NULL)) return false;
    if (OID_IS(oid, OID_RSA_SHA256)) { *kind = X509_SIG_RSA_PKCS1; *halg = HASH_SHA256; }
    else if (OID_IS(oid, OID_RSA_SHA384)) { *kind = X509_SIG_RSA_PKCS1; *halg = HASH_SHA384; }
    else if (OID_IS(oid, OID_RSA_SHA512)) { *kind = X509_SIG_RSA_PKCS1; *halg = HASH_SHA512; }
    else if (OID_IS(oid, OID_ECDSA_SHA256)) { *kind = X509_SIG_ECDSA; *halg = HASH_SHA256; }
    else if (OID_IS(oid, OID_ECDSA_SHA384)) { *kind = X509_SIG_ECDSA; *halg = HASH_SHA384; }
    else if (OID_IS(oid, OID_ECDSA_SHA512)) { *kind = X509_SIG_ECDSA; *halg = HASH_SHA512; }
    else if (OID_IS(oid, OID_RSA_PSS)) {
        /* RSASSA-PSS-params ::= SEQUENCE { [0] hash (padrão SHA-1), [1] MGF, [2] saltLength, [3] trailer } */
        dr_t prm, x, a, o;
        int h = 0, mh = 0;
        long sl = 20;
        if (!dr_expect(&alg, 0x30, &prm, NULL)) return true;            /* SHA-1 implícito: não suportado */
        if (dr_expect(&prm, 0xa0, &x, NULL)) {
            if (!dr_expect(&x, 0x30, &a, NULL) || !dr_expect(&a, 0x06, &o, NULL)) return false;
            h = hash_from_oid(o);
        }
        if (dr_expect(&prm, 0xa1, &x, NULL)) {
            dr_t m, mo, ha, ho;
            if (!dr_expect(&x, 0x30, &m, NULL) || !dr_expect(&m, 0x06, &mo, NULL)) return false;
            if (!OID_IS(mo, OID_MGF1)) return true;
            if (!dr_expect(&m, 0x30, &ha, NULL) || !dr_expect(&ha, 0x06, &ho, NULL)) return false;
            mh = hash_from_oid(ho);
        }
        if (dr_expect(&prm, 0xa2, &x, NULL)) {
            const u8 *ip; u32 il;
            if (!dr_uint(&x, &ip, &il) || il > 2) return false;
            sl = 0;
            for (u32 i = 0; i < il; i++) sl = sl << 8 | ip[i];
        }
        if (h && h == mh) { *kind = X509_SIG_RSA_PSS; *halg = h; *salt = (int)sl; }
    }
    return true;
}

/* ---------------------------------------------------------------- tempo */
static bool parse_time(dr_t *r, u64 *out) {
    u8 t;
    dr_t v;
    if (!dr_next(r, &t, &v, NULL)) return false;
    const u8 *s = v.p;
    u32 len = DLEN(v), year;
    u8 frac[15];
    if (t == 0x18 && len > 16 && s[14] == '.' && s[len - 1] == 'Z') {
        /* GeneralizedTime com fração de segundo (aparece em respostas OCSP): descarta a fração */
        for (u32 i = 15; i + 1 < len; i++) if (s[i] < '0' || s[i] > '9') return false;
        memcpy(frac, s, 14);
        frac[14] = 'Z';
        s = frac;
        len = 15;
    }
    for (u32 i = 0; i + 1 < len; i++) if (s[i] < '0' || s[i] > '9') return false;
    if (t == 0x17 && len == 13 && s[12] == 'Z') {
        u32 yy = (u32)(s[0] - '0') * 10 + (u32)(s[1] - '0');
        year = yy < 50 ? 2000 + yy : 1900 + yy;
        s += 2;
    } else if (t == 0x18 && len == 15 && s[14] == 'Z') {
        year = 0;
        for (int i = 0; i < 4; i++) year = year * 10 + (u32)(s[i] - '0');
        s += 4;
    } else return false;
    u64 v64 = year;
    for (int i = 0; i < 5; i++) v64 = v64 * 100 + (u64)((s[2 * i] - '0') * 10 + (s[2 * i + 1] - '0'));
    *out = v64;
    return true;
}

/* ---------------------------------------------------------------- extensões */
static bool parse_extensions(x509_cert_t *c, dr_t exts) {
    dr_t seq;
    if (!dr_expect(&exts, 0x30, &seq, NULL)) return false;
    while (seq.p < seq.end) {
        dr_t ext, oid, crit, val;
        if (!dr_expect(&seq, 0x30, &ext, NULL) || !dr_expect(&ext, 0x06, &oid, NULL)) return false;
        bool critical = false;
        if (dr_expect(&ext, 0x01, &crit, NULL)) critical = DLEN(crit) == 1 && crit.p[0];
        if (!dr_expect(&ext, 0x04, &val, NULL)) return false;
        bool known = true;
        if (DLEN(oid) == 3 && oid.p[0] == 0x55 && oid.p[1] == 0x1d) {
            switch (oid.p[2]) {
            case 19: {                                  /* basicConstraints */
                dr_t bc, b;
                if (!dr_expect(&val, 0x30, &bc, NULL)) return false;
                c->has_bc = true;
                if (dr_expect(&bc, 0x01, &b, NULL)) c->ca = DLEN(b) == 1 && b.p[0];
                const u8 *ip; u32 il;
                if (dr_peek(&bc, 0x02)) {
                    if (!dr_uint(&bc, &ip, &il) || il > 2) return false;
                    c->pathlen = il == 1 ? ip[0] : (ip[0] << 8 | ip[1]);
                }
                break;
            }
            case 15: {                                  /* keyUsage */
                dr_t bs;
                if (!dr_expect(&val, 0x03, &bs, NULL) || !DLEN(bs)) return false;
                c->has_ku = true;
                c->ku = 0;
                for (u32 i = 1; i < DLEN(bs) && i < 4; i++)
                    for (int k = 0; k < 8; k++)
                        if (bs.p[i] & (0x80 >> k)) c->ku |= 1u << ((i - 1) * 8 + k);
                break;
            }
            case 37: {                                  /* extKeyUsage */
                dr_t l, o;
                if (!dr_expect(&val, 0x30, &l, NULL)) return false;
                c->has_eku = true;
                while (l.p < l.end) {
                    if (!dr_expect(&l, 0x06, &o, NULL)) return false;
                    if (OID_IS(o, OID_SERVER_AUTH) || OID_IS(o, OID_ANY_EKU)) c->eku_server = true;
                    if (OID_IS(o, OID_OCSP_SIGNING)) c->eku_ocsp = true;
                }
                break;
            }
            case 17: {                                  /* subjectAltName */
                dr_t l;
                if (!dr_expect(&val, 0x30, &l, NULL)) return false;
                c->san = l.p;
                c->san_len = DLEN(l);
                break;
            }
            case 14: case 35:                           /* SKI / AKI */
            case 30:                                    /* nameConstraints (ignorada) */
            case 32: case 33: case 36: case 54:         /* políticas (ignoradas) */
            case 31: case 46:                           /* CRL distribution points / freshest CRL */
                break;
            default:
                known = false;
            }
        } else known = false;
        if (critical && !known) c->unknown_critical = true;
    }
    return true;
}

/* ---------------------------------------------------------------- certificado */
static bool parse_spki(x509_cert_t *c, dr_t spki) {
    dr_t alg, oid, bits;
    if (!dr_expect(&spki, 0x30, &alg, NULL) || !dr_expect(&alg, 0x06, &oid, NULL)) return false;
    if (!dr_expect(&spki, 0x03, &bits, NULL) || !DLEN(bits) || bits.p[0] != 0) return false;
    bits.p++;
    if (OID_IS(oid, OID_RSA)) {
        dr_t k, seq;
        k = bits;
        if (!dr_expect(&k, 0x30, &seq, NULL)) return false;
        if (!dr_uint(&seq, &c->rsa_n, &c->rsa_n_len) || !dr_uint(&seq, &c->rsa_e, &c->rsa_e_len)) return false;
        c->key_type = X509_KEY_RSA;
    } else if (OID_IS(oid, OID_EC_PUB)) {
        dr_t curve;
        if (!dr_expect(&alg, 0x06, &curve, NULL)) return true;      /* curva explícita: não suportada */
        c->curve = OID_IS(curve, OID_P256) ? EC_P256 : OID_IS(curve, OID_P384) ? EC_P384 : 0;
        if (c->curve) {
            c->key_type = X509_KEY_EC;
            c->ec_pub = bits.p;
            c->ec_pub_len = DLEN(bits);
        }
    }
    return true;                                       /* outras chaves: key_type = NONE */
}

int x509_parse(x509_cert_t *c, const u8 *der, u32 len) {
    memset(c, 0, sizeof(*c));
    c->pathlen = -1;
    dr_t r = {der, der + len}, cert, tbs, sa, sv, x;
    const u8 *tbs_start;
    if (!dr_expect(&r, 0x30, &cert, NULL)) return X509_E_PARSE;
    c->raw = der;
    c->raw_len = (u32)(cert.end - der);
    if (!dr_expect(&cert, 0x30, &tbs, &tbs_start)) return X509_E_PARSE;
    c->tbs = tbs_start;
    c->tbs_len = (u32)(tbs.end - tbs_start);
    if (!dr_expect(&cert, 0x30, &sa, NULL) || !dr_expect(&cert, 0x03, &sv, NULL) || cert.p != cert.end)
        return X509_E_PARSE;
    if (!parse_sigalg(sa, &c->sig_kind, &c->sig_hash, &c->pss_salt)) return X509_E_PARSE;
    if (!DLEN(sv) || sv.p[0] != 0) return X509_E_PARSE;
    c->sig = sv.p + 1;
    c->sig_len = DLEN(sv) - 1;

    int version = 0;
    if (dr_expect(&tbs, 0xa0, &x, NULL)) {
        const u8 *vp; u32 vl;
        if (!dr_uint(&x, &vp, &vl) || vl != 1) return X509_E_PARSE;
        version = vp[0];
    }
    const u8 *ip; u32 il;
    dr_t serial;
    if (!dr_expect(&tbs, 0x02, &serial, NULL) || !DLEN(serial)) return X509_E_PARSE;
    (void)ip; (void)il;
    c->serial = serial.p;
    c->serial_len = DLEN(serial);
    dr_t ia;
    if (!dr_expect(&tbs, 0x30, &ia, NULL)) return X509_E_PARSE;
    if (DLEN(ia) != DLEN(sa) || memcmp(ia.p, sa.p, DLEN(sa))) return X509_E_PARSE;   /* algoritmos divergentes */
    dr_t name, val;
    const u8 *ns;
    if (!dr_expect(&tbs, 0x30, &name, &ns)) return X509_E_PARSE;
    c->issuer = ns;
    c->issuer_len = (u32)(name.end - ns);
    if (!dr_expect(&tbs, 0x30, &val, NULL)) return X509_E_PARSE;
    if (!parse_time(&val, &c->not_before) || !parse_time(&val, &c->not_after)) return X509_E_PARSE;
    if (!dr_expect(&tbs, 0x30, &name, &ns)) return X509_E_PARSE;
    c->subject = ns;
    c->subject_len = (u32)(name.end - ns);
    dr_t spki;
    if (!dr_expect(&tbs, 0x30, &spki, &ns)) return X509_E_PARSE;
    c->spki = ns;
    c->spki_len = (u32)(spki.end - ns);
    if (!parse_spki(c, spki)) return X509_E_PARSE;
    dr_expect(&tbs, 0x81, &x, NULL);                   /* issuerUniqueID */
    dr_expect(&tbs, 0x82, &x, NULL);                   /* subjectUniqueID */
    if (dr_expect(&tbs, 0xa3, &x, NULL)) {
        if (version != 2 || !parse_extensions(c, x)) return X509_E_PARSE;
    }
    if (tbs.p != tbs.end) return X509_E_PARSE;
    return X509_OK;
}

void x509_name_cn(const u8 *name, u32 len, char *out, u32 outlen) {
    if (!outlen) return;
    out[0] = 0;
    dr_t r = {name, name + len}, seq;
    if (!dr_expect(&r, 0x30, &seq, NULL)) return;
    dr_t best = {0, 0};
    int best_rank = 0;
    while (seq.p < seq.end) {
        dr_t set, atv, oid, v;
        u8 t;
        if (!dr_expect(&seq, 0x31, &set, NULL)) return;
        while (set.p < set.end) {
            if (!dr_expect(&set, 0x30, &atv, NULL) || !dr_expect(&atv, 0x06, &oid, NULL) || !dr_next(&atv, &t, &v, NULL))
                return;
            int rank = OID_IS(oid, OID_CN) ? 2 : OID_IS(oid, OID_O) ? 1 : 0;
            if (rank > best_rank) { best_rank = rank; best = v; }
        }
    }
    u32 n = 0;
    for (const u8 *p = best.p; p && p < best.end && n + 1 < outlen; p++)
        if (*p >= 0x20 && *p != 0x7f) out[n++] = (char)*p;
    out[n] = 0;
    if (!n) snprintf(out, outlen, "?");
}

/* ======================================================================
 * assinaturas
 * ====================================================================== */
int x509_verify_with_key(const x509_cert_t *key, int kind, int halg, int saltlen,
                         const u8 *msg, u32 mlen, const u8 *sig, u32 siglen) {
    if (!halg) return X509_E_UNSUPPORTED;
    u8 h[HASH_MAX_LEN];
    hash_buf(halg, msg, mlen, h);
    switch (kind) {
    case X509_SIG_RSA_PKCS1:
        if (key->key_type != X509_KEY_RSA) return X509_E_UNSUPPORTED;
        return rsa_verify_pkcs1(key->rsa_n, key->rsa_n_len, key->rsa_e, key->rsa_e_len, halg, h, sig, siglen)
            ? X509_E_BAD_SIG : X509_OK;
    case X509_SIG_RSA_PSS:
        if (key->key_type != X509_KEY_RSA) return X509_E_UNSUPPORTED;
        return rsa_verify_pss(key->rsa_n, key->rsa_n_len, key->rsa_e, key->rsa_e_len, halg, h, sig, siglen, saltlen)
            ? X509_E_BAD_SIG : X509_OK;
    case X509_SIG_ECDSA: {
        if (key->key_type != X509_KEY_EC) return X509_E_UNSUPPORTED;
        dr_t r = {sig, sig + siglen}, seq;
        const u8 *rp, *sp;
        u32 rl, sl;
        if (!dr_expect(&r, 0x30, &seq, NULL) || r.p != r.end) return X509_E_BAD_SIG;
        if (!dr_uint(&seq, &rp, &rl) || !dr_uint(&seq, &sp, &sl) || seq.p != seq.end) return X509_E_BAD_SIG;
        return ecdsa_verify(key->curve, key->ec_pub, key->ec_pub_len, h, hash_len(halg), rp, rl, sp, sl)
            ? X509_E_BAD_SIG : X509_OK;
    }
    }
    return X509_E_UNSUPPORTED;
}

/* ======================================================================
 * nome do host (RFC 6125): SAN dNSName / iPAddress; sem recurso ao CN
 * ====================================================================== */
static int lc(int ch) { return ch >= 'A' && ch <= 'Z' ? ch + 32 : ch; }

static bool eq_ci(const u8 *a, u32 al, const char *b, u32 bl) {
    if (al != bl) return false;
    for (u32 i = 0; i < al; i++) if (lc(a[i]) != lc((u8)b[i])) return false;
    return true;
}

static bool dns_match(const u8 *pat, u32 pl, const char *host, u32 hl) {
    if (pl && pat[pl - 1] == '.') pl--;
    if (hl && host[hl - 1] == '.') hl--;
    if (!pl || !hl) return false;
    for (u32 i = 0; i < pl; i++) if (pat[i] < 0x21 || pat[i] > 0x7e) return false;
    if (pl > 2 && pat[0] == '*' && pat[1] == '.') {
        /* curinga só como rótulo inteiro mais à esquerda; o resto precisa ter >= 2 rótulos */
        const u8 *rest = pat + 1;
        u32 rl = pl - 1;
        bool dot = false;
        for (u32 i = 1; i < rl; i++) {
            if (rest[i] == '*') return false;
            if (rest[i] == '.') dot = true;
        }
        if (!dot) return false;
        u32 k = 0;
        while (k < hl && host[k] != '.') k++;
        if (k == 0 || k == hl) return false;             /* rótulo vazio ou host de um rótulo só */
        return eq_ci(rest, rl, host + k, hl - k);
    }
    for (u32 i = 0; i < pl; i++) if (pat[i] == '*') return false;   /* curinga parcial: nunca */
    return eq_ci(pat, pl, host, hl);
}

static int parse_ipv4(const char *s, u8 out[4]) {
    for (int i = 0; i < 4; i++) {
        if (*s < '0' || *s > '9') return 0;
        u32 v = 0, d = 0;
        while (*s >= '0' && *s <= '9') { v = v * 10 + (u32)(*s++ - '0'); if (++d > 3) return 0; }
        if (v > 255) return 0;
        out[i] = (u8)v;
        if (i < 3 && *s++ != '.') return 0;
    }
    return *s == 0 ? 4 : 0;
}

static int hexv(int ch) {
    ch = lc(ch);
    return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : -1;
}

static int parse_ipv6(const char *s, u8 out[16]) {
    u16 g[8];
    int n = 0, gap = -1;
    if (s[0] == '[') s++;
    if (s[0] == ':' && s[1] == ':') { gap = 0; s += 2; }
    while (*s && *s != ']') {
        if (n == 8) return 0;
        u32 v = 0, d = 0;
        int h;
        while ((h = hexv(*s)) >= 0) { v = v << 4 | (u32)h; s++; if (++d > 4) return 0; }
        if (!d) return 0;
        g[n++] = (u16)v;
        if (*s == ':') {
            s++;
            if (*s == ':') { if (gap >= 0) return 0; gap = n; s++; }
            else if (!*s) return 0;
        } else if (*s && *s != ']') return 0;
    }
    if (gap < 0 ? n != 8 : n > 7) return 0;
    memset(out, 0, 16);
    int tail = gap < 0 ? 0 : n - gap;
    for (int i = 0; i < n; i++) {
        int pos = gap >= 0 && i >= gap ? 8 - tail + (i - gap) : i;
        out[2 * pos] = (u8)(g[i] >> 8);
        out[2 * pos + 1] = (u8)g[i];
    }
    return 16;
}

int x509_match_host(const x509_cert_t *c, const char *host) {
    if (!host || !*host || !c->san) return 0;
    u8 ip[16];
    int iplen = parse_ipv4(host, ip);
    if (!iplen) {
        bool colon = false;
        for (const char *p = host; *p; p++) if (*p == ':') colon = true;
        if (colon) iplen = parse_ipv6(host, ip);
    }
    u32 hl = (u32)strlen(host);
    dr_t r = {c->san, c->san + c->san_len}, v;
    u8 t;
    while (dr_next(&r, &t, &v, NULL)) {
        if (iplen && t == 0x87 && DLEN(v) == (u32)iplen && !memcmp(v.p, ip, iplen)) return 1;
        if (!iplen && t == 0x82 && dns_match(v.p, DLEN(v), host, hl)) return 1;
    }
    return 0;
}

/* ======================================================================
 * lista de confiança
 * ====================================================================== */
typedef struct { x509_cert_t c; u32 h; } anchor_t;
typedef struct { anchor_t *a; u32 n; u8 *der; } trust_t;
static trust_t *g_trust;

static u32 fnv(const u8 *p, u32 n) {
    u32 h = 2166136261u;
    for (u32 i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

static int b64v(int ch) {
    if (ch >= 'A' && ch <= 'Z') return ch - 'A';
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9') return ch - '0' + 52;
    if (ch == '+') return 62;
    if (ch == '/') return 63;
    return -1;
}

static const char *find_str(const char *p, const char *end, const char *s) {
    u32 n = (u32)strlen(s);
    for (; p + n <= end; p++)
        if (*p == *s && !memcmp(p, s, n)) return p;
    return NULL;
}

static trust_t *trust_parse(const char *pem, u32 len) {
    static const char BEGIN[] = "-----BEGIN CERTIFICATE-----", END[] = "-----END CERTIFICATE-----";
    const char *p = pem, *end = pem + len;
    u32 count = 0;
    for (const char *q = p; (q = find_str(q, end, BEGIN)); q += sizeof(BEGIN) - 1) count++;
    trust_t *t = kmalloc(sizeof(trust_t));
    if (!t) return NULL;
    t->a = kmalloc((count ? count : 1) * sizeof(anchor_t));
    t->der = kmalloc(len / 4 * 3 + 16);
    t->n = 0;
    if (!t->a || !t->der) { kfree(t->a); kfree(t->der); kfree(t); return NULL; }
    u8 *out = t->der;
    u32 bad = 0;
    while ((p = find_str(p, end, BEGIN))) {
        p += sizeof(BEGIN) - 1;
        const char *e = find_str(p, end, END);
        if (!e) break;
        u8 *start = out;
        u32 acc = 0, bits = 0;
        for (; p < e; p++) {
            int v = b64v((u8)*p);
            if (v < 0) continue;                       /* espaços, quebras de linha, '=' */
            acc = acc << 6 | (u32)v;
            bits += 6;
            if (bits >= 8) { bits -= 8; *out++ = (u8)(acc >> bits); }
        }
        p = e + sizeof(END) - 1;
        anchor_t *a = &t->a[t->n];
        if (x509_parse(&a->c, start, (u32)(out - start)) != X509_OK) { bad++; continue; }
        a->h = fnv(a->c.subject, a->c.subject_len);
        t->n++;
    }
    if (bad) kprintf("[x509] %u raízes ignoradas (formato não suportado)\n", bad);
    return t;
}

int x509_trust_load_pem(const char *pem, u32 len) {
    trust_t *t = trust_parse(pem, len);
    if (!t) return -1;
    trust_t *old = __atomic_exchange_n(&g_trust, t, __ATOMIC_ACQ_REL);
    (void)old;                         /* pode estar em uso por outra thread: não liberamos */
    return (int)t->n;
}

#ifndef MOS_HOST_TEST
extern const char mos_cacert_pem[], mos_cacert_pem_end[];   /* kernel/arch/certs.S */
#endif

static const trust_t *trust_get(void) {
    trust_t *t = __atomic_load_n(&g_trust, __ATOMIC_ACQUIRE);
    if (t) return t;
#ifndef MOS_HOST_TEST
    u64 t0 = timer_ticks();
    trust_t *nt = trust_parse(mos_cacert_pem, (u32)(mos_cacert_pem_end - mos_cacert_pem));
    if (!nt) return NULL;
    trust_t *expected = NULL;
    if (!__atomic_compare_exchange_n(&g_trust, &expected, nt, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        kfree(nt->a); kfree(nt->der); kfree(nt);     /* outra thread venceu a corrida */
        return expected;
    }
    kprintf("[x509] lista de confiança Mozilla carregada: %u raízes (%u ms)\n", nt->n,
            (u32)(timer_ticks() - t0));
    return nt;
#else
    return NULL;
#endif
}

u32 x509_trust_count(void) {
    const trust_t *t = trust_get();
    return t ? t->n : 0;
}

/* raiz confiável i (Acesso às Chaves) */
const x509_cert_t *x509_trust_get(u32 i) {
    const trust_t *t = trust_get();
    return t && i < t->n ? &t->a[i].c : NULL;
}

/* ======================================================================
 * cadeia
 * ====================================================================== */
typedef struct {
    x509_cert_t *c;
    u32 n;
    bool used[MAX_CHAIN];
    bool check_time;
    u64 now;
    const trust_t *t;
    const x509_cert_t *err_cert;
    const x509_cert_t *anchor;
} vctx_t;

static bool name_eq(const u8 *a, u32 al, const u8 *b, u32 bl) { return al == bl && !memcmp(a, b, al); }

static int err_rank(int e) {
    return e == X509_E_UNKNOWN_ISSUER ? 1 : (e == X509_E_SELF_SIGNED || e == X509_E_UNTRUSTED_ROOT) ? 2 : 3;
}

static int check_dates(const vctx_t *v, const x509_cert_t *c) {
    if (!v->check_time) return X509_OK;
    if (v->now < c->not_before) return X509_E_NOT_YET_VALID;
    if (v->now > c->not_after) return X509_E_EXPIRED;
    return X509_OK;
}

static int key_ok(const x509_cert_t *c) {
    if (c->key_type == X509_KEY_RSA) {
        u32 b = rsa_modulus_bits(c->rsa_n, c->rsa_n_len);
        if (b < 2048) return X509_E_WEAK_KEY;
        return b > 8192 ? X509_E_UNSUPPORTED : X509_OK;
    }
    return c->key_type == X509_KEY_EC ? X509_OK : X509_E_UNSUPPORTED;
}

static const x509_cert_t *anchor_same(const trust_t *t, const x509_cert_t *c) {
    u32 h = fnv(c->subject, c->subject_len);
    for (u32 i = 0; i < t->n; i++) {
        const x509_cert_t *a = &t->a[i].c;
        if (t->a[i].h == h && name_eq(a->subject, a->subject_len, c->subject, c->subject_len) &&
            name_eq(a->spki, a->spki_len, c->spki, c->spki_len))
            return a;
    }
    return NULL;
}

/* `iss` pode ter assinado `cur`? (cur está na profundidade `depth`; folha = 0) */
static int try_issuer(vctx_t *v, const x509_cert_t *cur, const x509_cert_t *iss, int depth, bool anchor,
                      const x509_cert_t **errc) {
    int e;
    *errc = iss;
    if (!anchor) {
        if (!iss->has_bc || !iss->ca) return X509_E_NOT_CA;
        if (iss->has_ku && !(iss->ku & (1u << 5))) return X509_E_KEY_USAGE;
        if (iss->pathlen >= 0 && depth > iss->pathlen) return X509_E_PATHLEN;
        if (iss->unknown_critical) return X509_E_CRITICAL_EXT;
        if ((e = check_dates(v, iss))) return e;
    }
    if ((e = key_ok(iss))) return e;
    *errc = cur;
    return x509_verify_with_key(iss, cur->sig_kind, cur->sig_hash, cur->pss_salt,
                                cur->tbs, cur->tbs_len, cur->sig, cur->sig_len);
}

static int build(vctx_t *v, const x509_cert_t *cur, int depth) {
    const x509_cert_t *a;
    if (depth > 0 && (a = anchor_same(v->t, cur))) { v->anchor = a; return X509_OK; }
    if (depth >= MAX_DEPTH) { v->err_cert = cur; return X509_E_TOO_LONG; }
    int best = X509_E_UNKNOWN_ISSUER;
    const x509_cert_t *bestc = cur, *ec;
    u32 h = fnv(cur->issuer, cur->issuer_len);
    for (u32 i = 0; i < v->t->n; i++) {
        a = &v->t->a[i].c;
        if (v->t->a[i].h != h || !name_eq(a->subject, a->subject_len, cur->issuer, cur->issuer_len)) continue;
        int e = try_issuer(v, cur, a, depth, true, &ec);
        if (!e) { v->anchor = a; return X509_OK; }
        if (err_rank(e) > err_rank(best)) { best = e; bestc = ec; }
    }
    for (u32 i = 1; i < v->n; i++) {
        const x509_cert_t *c = &v->c[i];
        if (v->used[i] || c == cur || !name_eq(c->subject, c->subject_len, cur->issuer, cur->issuer_len)) continue;
        int e = try_issuer(v, cur, c, depth, false, &ec);
        if (!e) {
            v->used[i] = true;
            e = build(v, c, depth + 1);
            v->used[i] = false;
            if (!e) return X509_OK;
            ec = v->err_cert;
        }
        if (err_rank(e) > err_rank(best) || (err_rank(e) == err_rank(best) && e != X509_E_UNKNOWN_ISSUER)) {
            best = e; bestc = ec;
        }
    }
    if (best == X509_E_UNKNOWN_ISSUER && name_eq(cur->issuer, cur->issuer_len, cur->subject, cur->subject_len))
        best = depth == 0 ? X509_E_SELF_SIGNED : X509_E_UNTRUSTED_ROOT;
    v->err_cert = bestc;
    return best;
}

static void fmt_date(u64 t, char *b, u32 n) {
    u32 s = (u32)(t / 1000000);                    /* AAAAMMDDhhmmss -> AAAAMMDD */
    snprintf(b, n, "%04u-%02u-%02u", s / 10000, s / 100 % 100, s % 100);
}

static void format_err(int e, const vctx_t *v, const char *host, char *err, u32 errlen) {
    const x509_cert_t *c = v->err_cert ? v->err_cert : &v->c[0];
    bool leaf = c == &v->c[0];
    char cn[96], d[16];
    x509_name_cn(c->subject, c->subject_len, cn, sizeof(cn));
    switch (e) {
    case X509_E_UNKNOWN_ISSUER: {
        char icn[96];
        x509_name_cn(c->issuer, c->issuer_len, icn, sizeof(icn));
        snprintf(err, errlen, "certificado não confiável: emissor desconhecido (%s)", icn);
        break;
    }
    case X509_E_SELF_SIGNED:
        snprintf(err, errlen, "certificado não confiável: autoassinado (%s)", cn); break;
    case X509_E_UNTRUSTED_ROOT:
        snprintf(err, errlen, "certificado não confiável: emissor desconhecido (raiz \"%s\" fora da lista de confiança)", cn);
        break;
    case X509_E_EXPIRED:
        fmt_date(c->not_after, d, sizeof(d));
        if (leaf) snprintf(err, errlen, "certificado expirado (válido até %s)", d);
        else snprintf(err, errlen, "certificado intermediário expirado: %s (válido até %s)", cn, d);
        break;
    case X509_E_NOT_YET_VALID:
        fmt_date(c->not_before, d, sizeof(d));
        snprintf(err, errlen, "certificado%s ainda não é válido (%s; válido a partir de %s)",
                 leaf ? "" : " intermediário", cn, d);
        break;
    case X509_E_BAD_SIG:
        snprintf(err, errlen, "assinatura inválida na cadeia de certificados (%s)", cn); break;
    case X509_E_UNSUPPORTED:
        snprintf(err, errlen, "algoritmo de chave/assinatura não suportado no certificado (%s)", cn); break;
    case X509_E_NOT_CA:
        snprintf(err, errlen, "certificado intermediário não é autoridade certificadora (%s)", cn); break;
    case X509_E_PATHLEN:
        snprintf(err, errlen, "cadeia excede o limite pathLen do certificado %s", cn); break;
    case X509_E_NAME:
        snprintf(err, errlen, "o nome do certificado não corresponde a %s", host ? host : "(sem nome)"); break;
    case X509_E_KEY_USAGE:
        snprintf(err, errlen, "uso de chave do certificado não permite servidor TLS (%s)", cn); break;
    case X509_E_WEAK_KEY:
        snprintf(err, errlen, "chave RSA fraca demais (< 2048 bits) no certificado %s", cn); break;
    case X509_E_CRITICAL_EXT:
        snprintf(err, errlen, "extensão crítica desconhecida no certificado %s", cn); break;
    case X509_E_TOO_LONG:
        snprintf(err, errlen, "cadeia de certificados longa demais"); break;
    case X509_E_NO_MEM:
        snprintf(err, errlen, "sem memória para validar certificados"); break;
    default:
        snprintf(err, errlen, "certificado do servidor malformado (DER/X.509 inválido)"); break;
    }
}

int x509_verify_chain(const u8 *const *ders, const u32 *lens, u32 count, const char *host,
                      const datetime_t *now, char *err, u32 errlen, char *anchor_cn, u32 cnlen) {
    if (anchor_cn && cnlen) anchor_cn[0] = 0;
    vctx_t v;
    memset(&v, 0, sizeof(v));
    if (!count) { snprintf(err, errlen, "o servidor não enviou certificado"); return X509_E_PARSE; }
    if (count > MAX_CHAIN) count = MAX_CHAIN;
    v.c = kmalloc(count * sizeof(x509_cert_t));
    if (!v.c) { format_err(X509_E_NO_MEM, &v, host, err, errlen); return X509_E_NO_MEM; }
    v.n = count;
    int e = x509_parse(&v.c[0], ders[0], lens[0]);
    if (e) { snprintf(err, errlen, "certificado do servidor malformado (DER/X.509 inválido)"); goto out; }
    v.used[0] = true;
    for (u32 i = 1; i < count; i++)
        if (x509_parse(&v.c[i], ders[i], lens[i]) != X509_OK) v.used[i] = true;   /* ignora os ilegíveis */
    if (now) {
        v.check_time = true;
        v.now = (u64)now->year * 10000000000ull + (u64)now->month * 100000000ull + (u64)now->day * 1000000ull +
                (u64)now->hour * 10000ull + (u64)now->minute * 100ull + (u64)now->second;
    }
    v.t = trust_get();
    if (!v.t || !v.t->n) {
        snprintf(err, errlen, "certificado não confiável: lista de autoridades confiáveis indisponível");
        e = X509_E_UNKNOWN_ISSUER;
        goto out;
    }
    const x509_cert_t *leaf = &v.c[0];
    v.err_cert = leaf;
    if ((e = key_ok(leaf)) || (leaf->unknown_critical && (e = X509_E_CRITICAL_EXT))) goto fmt;
    if ((e = build(&v, leaf, 0))) goto fmt;
    v.err_cert = leaf;
    if ((e = check_dates(&v, leaf))) goto fmt;
    if ((leaf->has_eku && !leaf->eku_server) || (leaf->has_ku && !(leaf->ku & 1))) { e = X509_E_KEY_USAGE; goto fmt; }
    if (!x509_match_host(leaf, host)) { e = X509_E_NAME; goto fmt; }
    if (anchor_cn && v.anchor) x509_name_cn(v.anchor->subject, v.anchor->subject_len, anchor_cn, cnlen);
    e = X509_OK;
    goto out;
fmt:
    format_err(e, &v, host, err, errlen);
out:
    kfree(v.c);
    return e;
}

/* ======================================================================
 * OCSP grampeado (RFC 6960; entregue via status_request, RFC 6066 / RFC 8446)
 * ====================================================================== */
#define OCSP_SKEW (5 * 60)                 /* tolerância de relógio, em segundos */

/* AAAAMMDDhhmmss -> segundos desde 1970-01-01 (days_from_civil de H. Hinnant) */
static i64 ymd_secs(u64 t) {
    i64 y = (i64)(t / 10000000000ull), mo = (i64)(t / 100000000ull % 100), d = (i64)(t / 1000000ull % 100);
    i64 h = (i64)(t / 10000 % 100), mi = (i64)(t / 100 % 100), s = (i64)(t % 100);
    y -= mo <= 2;
    i64 era = (y >= 0 ? y : y - 399) / 400;
    i64 yoe = y - era * 400;
    i64 doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    i64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (era * 146097 + doe - 719468) * 86400 + h * 3600 + mi * 60 + s;
}

/* bits da chave pública (conteúdo do BIT STRING do SPKI, sem o byte de bits não usados) */
static bool spki_key_bits(const x509_cert_t *c, const u8 **p, u32 *n) {
    dr_t r = {c->spki, c->spki + c->spki_len}, s, alg, bits;
    if (!dr_expect(&r, 0x30, &s, NULL) || !dr_expect(&s, 0x30, &alg, NULL) || !dr_expect(&s, 0x03, &bits, NULL) ||
        !DLEN(bits))
        return false;
    *p = bits.p + 1;
    *n = DLEN(bits) - 1;
    return true;
}

/* halg < 0 = SHA-1 */
static u32 id_hash(int halg, const u8 *d, u32 n, u8 *out) {
    if (halg < 0) { sha1(d, n, out); return 20; }
    hash_buf(halg, d, n, out);
    return hash_len(halg);
}

static const char *crl_reason(int r) {
    switch (r) {
    case 0: return "não especificado";
    case 1: return "chave comprometida";
    case 2: return "autoridade certificadora comprometida";
    case 3: return "mudança de afiliação";
    case 4: return "substituído";
    case 5: return "cessação de operação";
    case 6: return "suspenso";
    case 8: return "removido da CRL";
    case 9: return "privilégio retirado";
    case 10: return "autoridade de atributos comprometida";
    default: return "desconhecido";
    }
}

#define OCSP_FAIL(code, ...) do { snprintf(msg, msglen, __VA_ARGS__); ret = (code); goto out; } while (0)

int x509_ocsp_check(const u8 *resp, u32 rlen, const u8 *const *ders, const u32 *lens, u32 count,
                    const datetime_t *now, char *msg, u32 msglen) {
    int ret = OCSP_UNUSABLE;
    x509_cert_t *cs = kmalloc(3 * sizeof(x509_cert_t));      /* folha, emissor, responder */
    if (!cs) { snprintf(msg, msglen, "sem memória para conferir OCSP"); return OCSP_UNUSABLE; }
    x509_cert_t *leaf = &cs[0], *ib = &cs[1], *rc = &cs[2];
    const x509_cert_t *iss = NULL;
    if (!count || x509_parse(leaf, ders[0], lens[0]) != X509_OK)
        OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP ignorada: certificado do servidor ilegível");

    /* emissor da folha: nos certificados enviados ou, se não estiver lá, na lista de confiança */
    for (u32 i = 1; i < count && !iss; i++) {
        if (x509_parse(ib, ders[i], lens[i]) != X509_OK) continue;
        if (!name_eq(ib->subject, ib->subject_len, leaf->issuer, leaf->issuer_len)) continue;
        if (x509_verify_with_key(ib, leaf->sig_kind, leaf->sig_hash, leaf->pss_salt, leaf->tbs, leaf->tbs_len,
                                 leaf->sig, leaf->sig_len) == X509_OK)
            iss = ib;
    }
    const trust_t *t = iss ? NULL : trust_get();
    for (u32 i = 0; t && i < t->n && !iss; i++) {
        const x509_cert_t *a = &t->a[i].c;
        if (name_eq(a->subject, a->subject_len, leaf->issuer, leaf->issuer_len) &&
            x509_verify_with_key(a, leaf->sig_kind, leaf->sig_hash, leaf->pss_salt, leaf->tbs, leaf->tbs_len,
                                 leaf->sig, leaf->sig_len) == X509_OK)
            iss = a;
    }
    if (!iss) OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP ignorada: emissor do certificado não encontrado");

    /* OCSPResponse ::= SEQUENCE { responseStatus ENUMERATED, responseBytes [0] EXPLICIT ResponseBytes } */
    dr_t r = {resp, resp + rlen}, top, x, rb, oid, oct;
    if (!dr_expect(&r, 0x30, &top, NULL) || !dr_expect(&top, 0x0a, &x, NULL) || DLEN(x) != 1)
        OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP grampeada malformada");
    if (x.p[0] != 0) OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP grampeada sem sucesso (responseStatus %u)", x.p[0]);
    if (!dr_expect(&top, 0xa0, &x, NULL) || !dr_expect(&x, 0x30, &rb, NULL) || !dr_expect(&rb, 0x06, &oid, NULL) ||
        !dr_expect(&rb, 0x04, &oct, NULL))
        OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP grampeada malformada");
    if (!OID_IS(oid, OID_OCSP_BASIC)) OCSP_FAIL(OCSP_UNUSABLE, "tipo de resposta OCSP não suportado");

    /* BasicOCSPResponse ::= SEQUENCE { tbsResponseData, signatureAlgorithm, signature, certs [0] } */
    dr_t basic, tbs, sa, sv;
    const u8 *tbs_start;
    if (!dr_expect(&oct, 0x30, &basic, NULL) || !dr_expect(&basic, 0x30, &tbs, &tbs_start) ||
        !dr_expect(&basic, 0x30, &sa, NULL) || !dr_expect(&basic, 0x03, &sv, NULL) || !DLEN(sv) || sv.p[0])
        OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP grampeada malformada");
    int skind, shash, ssalt;
    if (!parse_sigalg(sa, &skind, &shash, &ssalt)) OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP grampeada malformada");
    if (skind == X509_SIG_NONE) OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP com algoritmo de assinatura não suportado");
    u32 tbs_len = (u32)(tbs.end - tbs_start);
    const u8 *sig = sv.p + 1;
    u32 siglen = DLEN(sv) - 1;

    /* ResponseData ::= SEQUENCE { version [0], responderID, producedAt, responses, ext [1] } */
    dr_t rid, resps;
    u8 tg;
    u64 produced;
    if (dr_peek(&tbs, 0xa0)) dr_expect(&tbs, 0xa0, &x, NULL);
    if (!dr_next(&tbs, &tg, &rid, NULL) || (tg != 0xa1 && tg != 0xa2) || !parse_time(&tbs, &produced) ||
        !dr_expect(&tbs, 0x30, &resps, NULL))
        OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP grampeada malformada");

    /* SingleResponse do nosso certificado (CertID: hash do nome e da chave do emissor + série) */
    const u8 *kb;
    u32 kbl;
    if (!spki_key_bits(iss, &kb, &kbl)) OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP ignorada: chave do emissor ilegível");
    bool found = false;
    int status = 0, reason = -1;
    u64 this_up = 0, next_up = 0, rev_time = 0;
    while (resps.p < resps.end && !found) {
        dr_t sr, cid, ha, ho, inh, ikh, ser, st;
        if (!dr_expect(&resps, 0x30, &sr, NULL) || !dr_expect(&sr, 0x30, &cid, NULL) ||
            !dr_expect(&cid, 0x30, &ha, NULL) || !dr_expect(&ha, 0x06, &ho, NULL) || !dr_expect(&cid, 0x04, &inh, NULL) ||
            !dr_expect(&cid, 0x04, &ikh, NULL) || !dr_expect(&cid, 0x02, &ser, NULL))
            OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP grampeada malformada");
        int halg = OID_IS(ho, OID_SHA1) ? -1 : hash_from_oid(ho);
        if (!halg) continue;
        u8 h1[HASH_MAX_LEN], h2[HASH_MAX_LEN];
        u32 hl = id_hash(halg, leaf->issuer, leaf->issuer_len, h1);
        id_hash(halg, kb, kbl, h2);
        if (DLEN(inh) != hl || DLEN(ikh) != hl || memcmp(inh.p, h1, hl) || memcmp(ikh.p, h2, hl) ||
            DLEN(ser) != leaf->serial_len || memcmp(ser.p, leaf->serial, leaf->serial_len))
            continue;
        if (!dr_next(&sr, &tg, &st, NULL)) OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP grampeada malformada");
        if (tg == 0x80) status = OCSP_GOOD;
        else if (tg == 0xa1) {
            status = OCSP_REVOKED;
            if (!parse_time(&st, &rev_time)) OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP grampeada malformada");
            dr_t rr, en;
            if (dr_expect(&st, 0xa0, &rr, NULL) && dr_expect(&rr, 0x0a, &en, NULL) && DLEN(en) == 1) reason = en.p[0];
        } else if (tg == 0x82) status = OCSP_UNKNOWN;
        else OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP grampeada malformada");
        if (!parse_time(&sr, &this_up)) OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP grampeada malformada");
        if (dr_expect(&sr, 0xa0, &x, NULL) && !parse_time(&x, &next_up))
            OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP grampeada malformada");
        found = true;
    }
    if (!found) OCSP_FAIL(OCSP_UNUSABLE, "resposta OCSP grampeada não se refere a este certificado");

    u64 nowv = 0;
    if (now)
        nowv = (u64)now->year * 10000000000ull + (u64)now->month * 100000000ull + (u64)now->day * 1000000ull +
               (u64)now->hour * 10000ull + (u64)now->minute * 100ull + (u64)now->second;

    /* quem assinou: o próprio emissor ou um responder delegado (emitido pelo emissor,
     * com id-kp-OCSPSigning e dentro da validade) incluído em certs [0] */
    char who[192];
    x509_name_cn(iss->subject, iss->subject_len, who, sizeof(who));
    if (x509_verify_with_key(iss, skind, shash, ssalt, tbs_start, tbs_len, sig, siglen) != X509_OK) {
        bool ok = false;
        const char *why = "assinatura inválida";
        dr_t certs, cv;
        const u8 *cstart;
        if (dr_expect(&basic, 0xa0, &x, NULL) && dr_expect(&x, 0x30, &certs, NULL)) {
            while (!ok && dr_expect(&certs, 0x30, &cv, &cstart)) {
                if (x509_parse(rc, cstart, (u32)(cv.end - cstart)) != X509_OK) continue;
                if (!name_eq(rc->issuer, rc->issuer_len, iss->subject, iss->subject_len) ||
                    x509_verify_with_key(iss, rc->sig_kind, rc->sig_hash, rc->pss_salt, rc->tbs, rc->tbs_len,
                                         rc->sig, rc->sig_len) != X509_OK) {
                    why = "responder não foi emitido pela autoridade do certificado";
                    continue;
                }
                if (!rc->has_eku || !rc->eku_ocsp) { why = "responder sem id-kp-OCSPSigning"; continue; }
                if (now && (nowv < rc->not_before || nowv > rc->not_after)) { why = "certificado do responder fora da validade"; continue; }
                if (key_ok(rc) != X509_OK) { why = "chave do responder não suportada"; continue; }
                if (x509_verify_with_key(rc, skind, shash, ssalt, tbs_start, tbs_len, sig, siglen) == X509_OK) {
                    ok = true;
                    char rn[128];
                    x509_name_cn(rc->subject, rc->subject_len, rn, sizeof(rn));
                    snprintf(who, sizeof(who), "responder delegado %s", rn);
                } else why = "assinatura inválida";
            }
        }
        if (!ok) OCSP_FAIL(OCSP_BAD_SIG, "resposta OCSP grampeada rejeitada: %s", why);
    }

    char d1[16], d2[16];
    if (status == OCSP_REVOKED) {
        fmt_date(rev_time, d1, sizeof(d1));
        if (reason >= 0)
            OCSP_FAIL(OCSP_REVOKED, "certificado revogado em %s (motivo: %s; OCSP de %s)", d1, crl_reason(reason), who);
        OCSP_FAIL(OCSP_REVOKED, "certificado revogado em %s (OCSP de %s)", d1, who);
    }
    if (now) {
        i64 n = ymd_secs(nowv);
        if (ymd_secs(this_up) > n + OCSP_SKEW) {
            fmt_date(this_up, d1, sizeof(d1));
            OCSP_FAIL(OCSP_STALE, "resposta OCSP grampeada com data no futuro (%s)", d1);
        }
        if (next_up && ymd_secs(next_up) < n - OCSP_SKEW) {
            fmt_date(next_up, d1, sizeof(d1));
            OCSP_FAIL(OCSP_STALE, "resposta OCSP grampeada expirada (válida até %s)", d1);
        }
        if (!next_up && ymd_secs(this_up) < n - 7 * 86400) {
            fmt_date(this_up, d1, sizeof(d1));
            OCSP_FAIL(OCSP_STALE, "resposta OCSP grampeada antiga demais (de %s, sem nextUpdate)", d1);
        }
    }
    fmt_date(this_up, d1, sizeof(d1));
    if (next_up) fmt_date(next_up, d2, sizeof(d2));
    else snprintf(d2, sizeof(d2), "?");
    if (status == OCSP_UNKNOWN)
        OCSP_FAIL(OCSP_UNKNOWN, "OCSP: status desconhecido para o certificado (%s)", who);
    OCSP_FAIL(OCSP_GOOD, "OCSP: certificado não revogado (%s; %s..%s)", who, d1, d2);
out:
    kfree(cs);
    return ret;
}