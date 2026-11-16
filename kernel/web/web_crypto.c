/* mOS — crypto.subtle (Web Cryptography API) do motor web sobre a criptografia do kernel (kernel/net/crypto.c,
 * kernel/net/pk.c, ed25519_impl.h): digest SHA-1/256/384/512; HMAC; AES-GCM e AES-CTR (128/256); PBKDF2 e HKDF;
 * ECDSA e ECDH em P-256/P-384; Ed25519; X25519; verificação RSASSA-PKCS1-v1_5 e RSA-PSS; formatos raw, jwk, spki
 * e pkcs8; generateKey, importKey, exportKey, sign, verify, encrypt, decrypt, deriveBits, deriveKey, wrapKey e
 * unwrapKey. As operações devolvem promessas, como a especificação manda. */
#include "js_int.h"
#include "web.h"
#include "crypto.h"
#include "x509.h"
#define ED_TYPES_DEFINED
typedef u8 ed_u8;
typedef u32 ed_u32;
typedef u64 ed_u64;
typedef i64 ed_i64;
typedef usize ed_size;
#include "ed25519_impl.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

static bool has_proto(jsobj *o, jsobj *p) {
    for (int i = 0; o && p && i < 32; i++, o = o->proto) if (o->proto == p) return true;
    return false;
}

/* ============================================================ erros como valores (para rejeitar) */
static jsv dom_err(const char *name, const char *fmt, ...) {
    char b[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    js_throw_dom(name, b);
    return JS_EXC;
}

/* ============================================================ bytes */
typedef struct { u8 *d; u32 n; } bytes_t;
static void bfree(bytes_t *b) { if (b->d) { secure_zero(b->d, b->n); kfree(b->d); } b->d = NULL; b->n = 0; }
static bool bset(bytes_t *b, const u8 *d, u32 n) { b->d = kmalloc(n ? n : 1); if (!b->d) return false; if (n) memcpy(b->d, d, n); b->n = n; return true; }

/* BufferSource → bytes (cópia) */
static bool buffer_source(jsv v, bytes_t *out) {
    memset(out, 0, sizeof *out);
    const u8 *src = NULL;
    u32 len = 0;
    if (jv_iscls(v, CL_ARRAYBUFFER)) { src = jv_o(v)->u.buf.data; len = jv_o(v)->u.buf.detached ? 0 : jv_o(v)->u.buf.len; }
    else if (jv_iscls(v, CL_TYPED) || jv_iscls(v, CL_DATAVIEW)) {
        jsobj *o = jv_o(v);
        if (o->u.ta.buffer && !o->u.ta.buffer->u.buf.detached) {
            src = o->u.ta.buffer->u.buf.data + o->u.ta.off;
            len = o->cls == CL_DATAVIEW ? o->u.ta.len : js_typed_length(o) * (u32)js_typed_elsize(o->u.ta.kind);
        }
    } else { js_throw_err(E_TYPE, "é preciso um ArrayBuffer ou uma view"); return false; }
    if (!bset(out, src, len)) { js_throw_err(E_RANGE, "sem memória"); return false; }
    return true;
}
static jsv to_abuf(const u8 *d, u32 n) {
    jsobj *b = js_abuf_new(n);
    if (!b) return JS_EXC;
    if (n) memcpy(b->u.buf.data, d, n);
    return jv_obj(b);
}

/* base64url sem preenchimento */
static jsstr *b64url(const u8 *d, u32 n) {
    static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    sbuf_t o = { 0 };
    for (u32 i = 0; i < n; i += 3) {
        u32 v = (u32)d[i] << 16 | (i + 1 < n ? (u32)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        sb_putc(&o, t[v >> 18]);
        sb_putc(&o, t[(v >> 12) & 63]);
        if (i + 1 < n) sb_putc(&o, t[(v >> 6) & 63]);
        if (i + 2 < n) sb_putc(&o, t[v & 63]);
    }
    jsstr *r = js_strn(o.p ? o.p : "", o.len);
    sb_free(&o);
    return r;
}
static bool unb64url(const char *s, bytes_t *out) {
    usize n = strlen(s);
    out->d = kmalloc(n * 3 / 4 + 4);
    out->n = 0;
    if (!out->d) return false;
    u32 acc = 0;
    int bits = 0;
    for (usize i = 0; i < n; i++) {
        char c = s[i];
        int v = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52 : c == '-' || c == '+' ? 62 : c == '_' || c == '/' ? 63 : c == '=' ? -2 : -1;
        if (v == -2) break;
        if (v < 0) { bfree(out); return false; }
        acc = acc << 6 | (u32)v;
        bits += 6;
        if (bits >= 8) { out->d[out->n++] = (u8)(acc >> (bits - 8)); bits -= 8; }
    }
    return true;
}

/* ============================================================ hashes e HMAC genéricos */
enum { H_NONE, H_SHA1, H_SHA256, H_SHA384, H_SHA512 };
static const char *hash_names[] = { "", "SHA-1", "SHA-256", "SHA-384", "SHA-512" };
static u32 hlen(int h) { return h == H_SHA1 ? 20 : h == H_SHA256 ? 32 : h == H_SHA384 ? 48 : 64; }
static u32 hblock(int h) { return h == H_SHA384 || h == H_SHA512 ? 128 : 64; }
static int kalg(int h) { return h == H_SHA256 ? HASH_SHA256 : h == H_SHA384 ? HASH_SHA384 : HASH_SHA512; }

static void digest(int h, const u8 *d, usize n, u8 *out) {
    if (h == H_SHA1) sha1(d, n, out);
    else hash_buf(kalg(h), d, n, out);
}

static void hmac_any(int h, const u8 *key, u32 klen, const u8 *msg, usize mlen, u8 *out) {
    if (h != H_SHA1) { hmac(kalg(h), key, klen, msg, mlen, out); return; }
    /* HMAC-SHA1 (RFC 2104) sobre o SHA-1 de uma chamada só */
    u8 k[64] = { 0 }, ik[20];
    if (klen > 64) sha1(key, klen, k); else memcpy(k, key, klen);
    u8 *buf = kmalloc(64 + (mlen > 20 ? mlen : 20));
    if (!buf) { memset(out, 0, 20); return; }
    for (int i = 0; i < 64; i++) buf[i] = k[i] ^ 0x36;
    memcpy(buf + 64, msg, mlen);
    sha1(buf, 64 + mlen, ik);
    for (int i = 0; i < 64; i++) buf[i] = k[i] ^ 0x5c;
    memcpy(buf + 64, ik, 20);
    sha1(buf, 84, out);
    secure_zero(buf, 64);
    kfree(buf);
    secure_zero(k, sizeof k);
}

static bool pbkdf2_any(int h, const u8 *pw, u32 pwl, const u8 *salt, u32 sl, u32 iters, u8 *out, u32 dkl) {
    if (h == H_SHA256) { pbkdf2_hmac_sha256(pw, pwl, salt, sl, iters, out, dkl); return true; }
    u32 hl = hlen(h);
    u8 *s = kmalloc(sl + 4);
    if (!s) return false;
    memcpy(s, salt, sl);
    u8 u[64], t[64];
    for (u32 blk = 1, off = 0; off < dkl; blk++, off += hl) {
        s[sl] = (u8)(blk >> 24); s[sl + 1] = (u8)(blk >> 16); s[sl + 2] = (u8)(blk >> 8); s[sl + 3] = (u8)blk;
        hmac_any(h, pw, pwl, s, sl + 4, u);
        memcpy(t, u, hl);
        for (u32 i = 1; i < iters; i++) { hmac_any(h, pw, pwl, u, hl, u); for (u32 k = 0; k < hl; k++) t[k] ^= u[k]; }
        memcpy(out + off, t, MIN(hl, dkl - off));
    }
    kfree(s);
    return true;
}

static bool hkdf_any(int h, const u8 *ikm, u32 il, const u8 *salt, u32 sl, const u8 *info, u32 nl, u8 *out, u32 ol) {
    u32 hl = hlen(h);
    if (ol > 255 * hl) return false;
    u8 prk[64], zero[64] = { 0 };
    hmac_any(h, sl ? salt : zero, sl ? sl : hl, ikm, il, prk);
    u8 *buf = kmalloc(hl + nl + 1), t[64];
    if (!buf) return false;
    u32 tl = 0;
    for (u32 i = 1, off = 0; off < ol; i++) {
        memcpy(buf, t, tl);
        memcpy(buf + tl, info, nl);
        buf[tl + nl] = (u8)i;
        hmac_any(h, prk, hl, buf, tl + nl + 1, t);
        tl = hl;
        memcpy(out + off, t, MIN(hl, ol - off));
        off += hl;
    }
    kfree(buf);
    secure_zero(prk, sizeof prk);
    return true;
}

/* ============================================================ algoritmos */
enum { A_NONE, A_SHA, A_HMAC, A_AESGCM, A_AESCTR, A_PBKDF2, A_HKDF, A_ECDSA, A_ECDH, A_ED25519, A_X25519, A_RSAPKCS1, A_RSAPSS };
static const char *alg_names[] = { "", "SHA", "HMAC", "AES-GCM", "AES-CTR", "PBKDF2", "HKDF", "ECDSA", "ECDH", "Ed25519", "X25519",
                                   "RSASSA-PKCS1-v1_5", "RSA-PSS" };

typedef struct {
    int alg, hash, curve;     /* curve: EC_P256/EC_P384 */
    u32 length;               /* bits (AES, HMAC, deriveBits) */
    jsv obj;                  /* objeto original */
} algo_t;

static int hash_of(const char *s) {
    for (int i = 1; i <= 4; i++) if (!strcasecmp(s, hash_names[i])) return i;
    return H_NONE;
}

/* nome e parâmetros comuns; para "SHA-x" alg = A_SHA e hash */
static bool norm_alg(jsv v, algo_t *a) {
    memset(a, 0, sizeof *a);
    a->obj = v;
    jsv nm = v;
    if (jv_isobj(v)) { nm = js_getn(v, "name"); if (js_pending()) return false; if (nm == JS_UNDEF) { js_throw_err(E_TYPE, "algoritmo sem name"); return false; } }
    jsstr *s = js_tostr(nm);
    if (!s) return false;
    int h = hash_of(s->d);
    if (h) { a->alg = A_SHA; a->hash = h; return true; }
    for (int i = 2; i < (int)ARRAY_LEN(alg_names); i++) if (!strcasecmp(s->d, alg_names[i])) a->alg = i;
    if (!a->alg) { dom_err("NotSupportedError", "algoritmo não suportado: %s", s->d); return false; }
    if (!jv_isobj(v)) return true;
    jsv hv = js_getn(v, "hash"), lv = js_getn(v, "length"), cv = js_getn(v, "namedCurve");
    if (js_pending()) return false;
    if (hv != JS_UNDEF) {
        algo_t ha;
        if (!norm_alg(hv, &ha)) return false;
        if (ha.alg != A_SHA) { dom_err("NotSupportedError", "hash inválido"); return false; }
        a->hash = ha.hash;
    }
    if (lv != JS_UNDEF) { double d = js_tonum(lv); if (js_pending()) return false; a->length = d > 0 && d < 1e6 ? (u32)d : 0; }
    if (cv != JS_UNDEF) {
        jsstr *c = js_tostr(cv);
        if (!c) return false;
        if (!strcmp(c->d, "P-256")) a->curve = EC_P256;
        else if (!strcmp(c->d, "P-384")) a->curve = EC_P384;
        else { dom_err("NotSupportedError", "curva não suportada: %s", c->d); return false; }
    }
    return true;
}

/* ============================================================ CryptoKey */
enum { K_TYPE, K_EXTRACT, K_ALG, K_USAGES, K_SECRET, K_PUBLIC, K_RSAN, K_RSAE, K_NSLOTS };
static jsobj *key_x(jsv v) {
    if (!jv_iscls(v, CL_HOST) || !has_proto(jv_o(v), J.realm->p_cryptokey) || !jv_isobj(jv_o(v)->u.host.extra)) return NULL;
    return jv_o(jv_o(v)->u.host.extra);
}
static const char *key_type(jsobj *x) { return jv_s(js_arr_get(x, K_TYPE))->d; }
static int key_alg(jsobj *x) {
    jsv n = js_getn(js_arr_get(x, K_ALG), "name");
    for (int i = 2; i < (int)ARRAY_LEN(alg_names); i++) if (jv_isstr(n) && !strcmp(jv_s(n)->d, alg_names[i])) return i;
    return A_NONE;
}
static int key_hash(jsobj *x) {
    jsv h = js_getn(js_arr_get(x, K_ALG), "hash");
    return jv_isobj(h) ? hash_of(jv_s(js_getn(h, "name"))->d) : H_NONE;
}
static int key_curve(jsobj *x) {
    jsv c = js_getn(js_arr_get(x, K_ALG), "namedCurve");
    return jv_isstr(c) ? (!strcmp(jv_s(c)->d, "P-384") ? EC_P384 : EC_P256) : 0;
}
static bool key_bytes(jsobj *x, int slot, const u8 **d, u32 *n) {
    jsv b = js_arr_get(x, slot);
    if (!jv_iscls(b, CL_ARRAYBUFFER)) return false;
    *d = jv_o(b)->u.buf.data;
    *n = jv_o(b)->u.buf.len;
    return true;
}
static bool has_usage(jsobj *x, const char *u) {
    jsobj *l = jv_o(js_arr_get(x, K_USAGES));
    for (u32 i = 0; i < l->alen; i++) if (!strcmp(jv_s(l->arr[i])->d, u)) return true;
    return false;
}

static jsobj *alg_object(int alg, int hash, u32 length, int curve) {
    jsobj *o = js_newobj(J.realm->p_object);
    js_defn(o, "name", jv_str(js_strz(alg_names[alg])), 0);
    if (hash) { jsobj *h = js_newobj(J.realm->p_object); js_defn(h, "name", jv_str(js_strz(hash_names[hash])), 0); js_defn(o, "hash", jv_obj(h), 0); }
    if (length) js_defn(o, "length", jv_int((int)length), 0);
    if (curve) js_defn(o, "namedCurve", jv_str(js_strz(curve == EC_P384 ? "P-384" : "P-256")), 0);
    return o;
}

static jsobj *key_new(const char *type, bool extractable, jsobj *alg, jsobj *usages, const u8 *secret, u32 sn, const u8 *pub, u32 pn) {
    jsobj *k = js_newobj_cls(CL_HOST, J.realm->p_cryptokey);
    k->u.host.page = js_cur_page();
    jsv s[K_NSLOTS] = { jv_str(js_strz(type)), jv_bool(extractable), jv_obj(alg), jv_obj(usages), JS_NULL, JS_NULL, JS_NULL, JS_NULL };
    if (secret) s[K_SECRET] = to_abuf(secret, sn);
    if (pub) s[K_PUBLIC] = to_abuf(pub, pn);
    k->u.host.extra = jv_obj(js_arr_from(s, K_NSLOTS));
    return k;
}

/* usos permitidos por algoritmo e tipo de chave */
static const char *allowed_usages(int alg, const char *type) {
    switch (alg) {
    case A_HMAC: return "sign verify";
    case A_AESGCM: case A_AESCTR: return "encrypt decrypt wrapKey unwrapKey";
    case A_PBKDF2: case A_HKDF: return "deriveKey deriveBits";
    case A_ECDSA: case A_ED25519: case A_RSAPKCS1: case A_RSAPSS: return !strcmp(type, "private") ? "sign" : "verify";
    case A_ECDH: case A_X25519: return !strcmp(type, "private") ? "deriveKey deriveBits" : "";
    }
    return "";
}

/* filtra os usos pedidos; SyntaxError se algum não vale */
static jsobj *usages_for(jsv list, int alg, const char *type, bool *ok) {
    *ok = false;
    jsobj *out = js_newarr(0);
    u32 n = js_lengthof(list);
    if (js_pending()) return NULL;
    const char *allowed = allowed_usages(alg, type);
    static const char *order[] = { "encrypt", "decrypt", "sign", "verify", "deriveKey", "deriveBits", "wrapKey", "unwrapKey" };
    bool want[8] = { false };
    for (u32 i = 0; i < n; i++) {
        jsstr *u = js_tostr(js_getidx(list, i));
        if (!u) return NULL;
        int idx = -1;
        for (int k = 0; k < 8; k++) if (!strcmp(u->d, order[k])) idx = k;
        if (idx < 0) { js_throw_err(E_TYPE, "uso de chave desconhecido: %s", u->d); return NULL; }
        const char *f = strstr(allowed, u->d);
        if (!f || (f[strlen(u->d)] != ' ' && f[strlen(u->d)] != 0)) { dom_err("SyntaxError", "uso '%s' não vale para %s %s", u->d, alg_names[alg], type); return NULL; }
        want[idx] = true;
    }
    for (int k = 0; k < 8; k++) if (want[k]) js_arr_push(out, jv_str(js_strz(order[k])));
    *ok = true;
    return out;
}

/* ============================================================ DER mínimo (spki / pkcs8) */
typedef struct { const u8 *p, *e; } der_t;
static bool der_next(der_t *d, u8 *tag, const u8 **val, u32 *len) {
    if (d->e - d->p < 2) return false;
    *tag = *d->p++;
    u32 l = *d->p++;
    if (l & 0x80) {
        int nb = l & 0x7F;
        if (nb < 1 || nb > 3 || d->e - d->p < nb) return false;
        l = 0;
        while (nb--) l = l << 8 | *d->p++;
    }
    if ((u32)(d->e - d->p) < l) return false;
    *val = d->p;
    *len = l;
    d->p += l;
    return true;
}
static const u8 OID_EC[] = { 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01 }, OID_P256[] = { 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07 },
                OID_P384[] = { 0x2B, 0x81, 0x04, 0x00, 0x22 }, OID_ED[] = { 0x2B, 0x65, 0x70 }, OID_X[] = { 0x2B, 0x65, 0x6E },
                OID_RSA[] = { 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01 };

static void der_put(sbuf_t *o, u8 tag, const u8 *v, u32 n) {
    sb_putc(o, (char)tag);
    if (n < 128) sb_putc(o, (char)n);
    else if (n < 256) { sb_putc(o, (char)0x81); sb_putc(o, (char)n); }
    else { sb_putc(o, (char)0x82); sb_putc(o, (char)(n >> 8)); sb_putc(o, (char)n); }
    if (n) sb_putn(o, (const char *)v, (int)n);
}

/* AlgorithmIdentifier de um algoritmo de chave */
static void der_algid(sbuf_t *o, int alg, int curve) {
    sbuf_t a = { 0 };
    if (alg == A_ECDSA || alg == A_ECDH) { der_put(&a, 0x06, OID_EC, sizeof OID_EC); if (curve == EC_P384) der_put(&a, 0x06, OID_P384, sizeof OID_P384); else der_put(&a, 0x06, OID_P256, sizeof OID_P256); }
    else if (alg == A_ED25519) der_put(&a, 0x06, OID_ED, sizeof OID_ED);
    else if (alg == A_X25519) der_put(&a, 0x06, OID_X, sizeof OID_X);
    else { der_put(&a, 0x06, OID_RSA, sizeof OID_RSA); der_put(&a, 0x05, NULL, 0); }
    der_put(o, 0x30, (const u8 *)a.p, (u32)a.len);
    sb_free(&a);
}

/* ============================================================ operações */
static jsobj *new_promise(void) { return js_promise_new(); }
static jsv resolve(jsobj *pr, jsv v) { js_promise_resolve(pr, v); return jv_obj(pr); }
static jsv reject_pending(jsobj *pr) { js_promise_reject(pr, js_catch()); return jv_obj(pr); }

/* --------------------------------------------------------- digest */
NATIVE(sc_digest) {
    UNUSED_ARGS;
    jsobj *pr = new_promise();
    algo_t a;
    bytes_t d;
    if (!norm_alg(ARG(0), &a)) return reject_pending(pr);
    if (a.alg != A_SHA) { dom_err("NotSupportedError", "digest: só SHA-1/256/384/512"); return reject_pending(pr); }
    if (!buffer_source(ARG(1), &d)) return reject_pending(pr);
    u8 out[64];
    digest(a.hash, d.d, d.n, out);
    bfree(&d);
    return resolve(pr, to_abuf(out, hlen(a.hash)));
}

/* --------------------------------------------------------- geração de chaves */
static bool ec_size(int curve, u32 *b) { *b = curve == EC_P384 ? 48 : 32; return true; }

static jsv make_pair(jsobj *pub, jsobj *priv) {
    jsobj *o = js_newobj(J.realm->p_object);
    js_defn(o, "publicKey", jv_obj(pub), 0);
    js_defn(o, "privateKey", jv_obj(priv), 0);
    return jv_obj(o);
}

/* usos de um par: separa os de assinar/derivar (privada) e verificar (pública) */
static bool pair_usages(jsv list, int alg, jsobj **pub, jsobj **priv) {
    *pub = js_newarr(0);
    *priv = js_newarr(0);
    u32 n = js_lengthof(list);
    if (js_pending()) return false;
    for (u32 i = 0; i < n; i++) {
        jsstr *u = js_tostr(js_getidx(list, i));
        if (!u) return false;
        bool sig = alg == A_ECDSA || alg == A_ED25519;
        if (sig && !strcmp(u->d, "sign")) js_arr_push(*priv, jv_str(u));
        else if (sig && !strcmp(u->d, "verify")) js_arr_push(*pub, jv_str(u));
        else if (!sig && (!strcmp(u->d, "deriveKey") || !strcmp(u->d, "deriveBits"))) js_arr_push(*priv, jv_str(u));
        else { dom_err("SyntaxError", "uso '%s' não vale para %s", u->d, alg_names[alg]); return false; }
    }
    if (!(*priv)->alen) { dom_err("SyntaxError", "generateKey: a chave privada precisa de algum uso"); return false; }
    return true;
}

static jsv gen_key(algo_t *a, bool ext, jsv usages) {
    bool ok;
    switch (a->alg) {
    case A_HMAC: case A_AESGCM: case A_AESCTR: {
        u32 bits;
        if (a->alg == A_HMAC) {
            if (!a->hash) { js_throw_err(E_TYPE, "HMAC precisa de hash"); return JS_EXC; }
            bits = a->length ? a->length : hblock(a->hash) * 8;
            if (bits % 8) return dom_err("OperationError", "comprimento de HMAC precisa ser múltiplo de 8");
        } else {
            bits = a->length;
            if (bits != 128 && bits != 256) return dom_err(bits == 192 ? "NotSupportedError" : "OperationError", "AES: comprimento %u não suportado", bits);
        }
        jsobj *u = usages_for(usages, a->alg, "secret", &ok);
        if (!ok) return JS_EXC;
        if (!u->alen) return dom_err("SyntaxError", "generateKey: a chave precisa de algum uso");
        u8 k[128];
        crypto_random(k, bits / 8);
        jsobj *key = key_new("secret", ext, alg_object(a->alg, a->alg == A_HMAC ? a->hash : 0, bits, 0), u, k, bits / 8, NULL, 0);
        secure_zero(k, sizeof k);
        return jv_obj(key);
    }
    case A_ECDSA: case A_ECDH: case A_ED25519: case A_X25519: {
        jsobj *pu, *pv;
        if (!pair_usages(usages, a->alg, &pu, &pv)) return JS_EXC;
        u8 priv[48], pub[97];
        u32 sn, pn;
        if (a->alg == A_ECDSA || a->alg == A_ECDH) {
            if (!a->curve) { js_throw_err(E_TYPE, "%s precisa de namedCurve", alg_names[a->alg]); return JS_EXC; }
            ec_size(a->curve, &sn);
            if (ecdh_keygen(a->curve, priv, pub)) return dom_err("OperationError", "falha ao gerar a chave EC");
            pn = 1 + 2 * sn;
        } else {
            crypto_random(priv, 32);
            sn = pn = 32;
            if (a->alg == A_ED25519) ed25519_pubkey(pub, priv);
            else x25519_base(pub, priv);
        }
        jsobj *algo = alg_object(a->alg, 0, 0, a->curve);
        jsobj *kpub = key_new("public", true, algo, pu, NULL, 0, pub, pn), *kpriv = key_new("private", ext, algo, pv, priv, sn, pub, pn);
        secure_zero(priv, sizeof priv);
        return make_pair(kpub, kpriv);
    }
    case A_RSAPKCS1: case A_RSAPSS: return dom_err("NotSupportedError", "geração de chaves RSA ainda não existe no mOS (só verificação)");
    }
    return dom_err("NotSupportedError", "generateKey não vale para %s", alg_names[a->alg]);
}

NATIVE(sc_generate) {
    UNUSED_ARGS;
    jsobj *pr = new_promise();
    algo_t a;
    if (!norm_alg(ARG(0), &a)) return reject_pending(pr);
    jsv k = gen_key(&a, js_truthy(ARG(1)), ARG(2));
    if (k == JS_EXC) return reject_pending(pr);
    return resolve(pr, k);
}

/* --------------------------------------------------------- importação */
static jsv jwk_str(jsv jwk, const char *k) { jsv v = js_getn(jwk, k); return jv_isstr(v) ? v : JS_UNDEF; }
static bool jwk_bytes(jsv jwk, const char *k, bytes_t *b) {
    jsv v = jwk_str(jwk, k);
    if (!jv_isstr(v) || !unb64url(jv_s(v)->d, b)) { dom_err("DataError", "JWK sem '%s' válido", k); return false; }
    return true;
}

static jsv import_key(const char *fmt, jsv data, algo_t *a, bool ext, jsv usages) {
    bool ok;
    bytes_t raw = { 0 };
    bool is_jwk = !strcmp(fmt, "jwk");
    if (strcmp(fmt, "raw") && strcmp(fmt, "jwk") && strcmp(fmt, "spki") && strcmp(fmt, "pkcs8")) { js_throw_err(E_TYPE, "formato desconhecido: %s", fmt); return JS_EXC; }
    if (is_jwk && !jv_isobj(data)) { js_throw_err(E_TYPE, "importKey jwk: objeto esperado"); return JS_EXC; }
    if (!is_jwk && !buffer_source(data, &raw)) return JS_EXC;
    jsv result = JS_EXC;
    switch (a->alg) {
    case A_HMAC: case A_AESGCM: case A_AESCTR: case A_PBKDF2: case A_HKDF: {
        if (strcmp(fmt, "raw") && !is_jwk) { result = dom_err("NotSupportedError", "%s só aceita raw ou jwk", alg_names[a->alg]); break; }
        if (is_jwk) {
            if (a->alg == A_PBKDF2 || a->alg == A_HKDF) { result = dom_err("NotSupportedError", "%s só aceita raw", alg_names[a->alg]); break; }
            jsv kty = jwk_str(data, "kty");
            if (!jv_isstr(kty) || strcmp(jv_s(kty)->d, "oct")) { result = dom_err("DataError", "JWK precisa de kty 'oct'"); break; }
            if (!jwk_bytes(data, "k", &raw)) break;
        }
        if ((a->alg == A_PBKDF2 || a->alg == A_HKDF) && ext) { result = dom_err("SyntaxError", "%s não pode ser extraível", alg_names[a->alg]); break; }
        u32 bits = raw.n * 8;
        if (a->alg == A_AESGCM || a->alg == A_AESCTR) {
            if (raw.n != 16 && raw.n != 32) { result = dom_err(raw.n == 24 ? "NotSupportedError" : "DataError", "AES: chave de %u bytes", raw.n); break; }
        }
        if (a->alg == A_HMAC) {
            if (!a->hash) { js_throw_err(E_TYPE, "HMAC precisa de hash"); break; }
            if (!raw.n) { result = dom_err("DataError", "chave HMAC vazia"); break; }
            if (a->length) { if (a->length > bits || a->length <= bits - 8) { result = dom_err("DataError", "length não confere"); break; } bits = a->length; }
        }
        jsobj *u = usages_for(usages, a->alg, "secret", &ok);
        if (!ok) break;
        result = jv_obj(key_new("secret", ext, alg_object(a->alg, a->alg == A_HMAC ? a->hash : 0, (a->alg == A_HMAC || a->alg == A_AESGCM || a->alg == A_AESCTR) ? bits : 0, 0),
                                u, raw.d, raw.n, NULL, 0));
        break;
    }
    case A_ECDSA: case A_ECDH: {
        int curve = a->curve;
        u8 pub[97], priv[48];
        u32 pn = 0, sn = 0;
        bool private_key = false;
        if (!strcmp(fmt, "raw")) {
            if (!curve) { js_throw_err(E_TYPE, "namedCurve obrigatória"); break; }
            if (raw.n != (curve == EC_P384 ? 97u : 65u) || raw.d[0] != 4) { result = dom_err("DataError", "ponto EC bruto inválido"); break; }
            memcpy(pub, raw.d, raw.n); pn = raw.n;
        } else if (!strcmp(fmt, "spki") || !strcmp(fmt, "pkcs8")) {
            der_t d = { raw.d, raw.d + raw.n };
            u8 t; const u8 *v; u32 l;
            if (!der_next(&d, &t, &v, &l) || t != 0x30) { result = dom_err("DataError", "DER inválido"); break; }
            der_t s = { v, v + l };
            if (!strcmp(fmt, "pkcs8")) { if (!der_next(&s, &t, &v, &l) || t != 0x02) { result = dom_err("DataError", "PKCS#8 inválido"); break; } }
            if (!der_next(&s, &t, &v, &l) || t != 0x30) { result = dom_err("DataError", "AlgorithmIdentifier inválido"); break; }
            der_t ai = { v, v + l };
            const u8 *oid; u32 ol;
            if (!der_next(&ai, &t, &oid, &ol) || t != 0x06 || ol != sizeof OID_EC || memcmp(oid, OID_EC, ol)) { result = dom_err("DataError", "não é uma chave EC"); break; }
            if (!der_next(&ai, &t, &oid, &ol) || t != 0x06) { result = dom_err("DataError", "curva ausente"); break; }
            int c2 = ol == sizeof OID_P256 && !memcmp(oid, OID_P256, ol) ? EC_P256 : ol == sizeof OID_P384 && !memcmp(oid, OID_P384, ol) ? EC_P384 : 0;
            if (!c2 || (curve && c2 != curve)) { result = dom_err("DataError", "curva não confere"); break; }
            curve = c2;
            if (!der_next(&s, &t, &v, &l)) { result = dom_err("DataError", "chave ausente"); break; }
            u32 B = curve == EC_P384 ? 48 : 32;
            if (!strcmp(fmt, "spki")) {
                if (t != 0x03 || l != 2 + 2 * B || v[0] != 0 || v[1] != 4) { result = dom_err("DataError", "ponto público inválido"); break; }
                memcpy(pub, v + 1, l - 1); pn = l - 1;
            } else {
                if (t != 0x04) { result = dom_err("DataError", "PKCS#8 inválido"); break; }
                der_t e = { v, v + l };
                if (!der_next(&e, &t, &v, &l) || t != 0x30) { result = dom_err("DataError", "ECPrivateKey inválido"); break; }
                der_t ep = { v, v + l };
                if (!der_next(&ep, &t, &v, &l) || t != 0x02 || !der_next(&ep, &t, &v, &l) || t != 0x04 || l != B) { result = dom_err("DataError", "escalar privado inválido"); break; }
                memcpy(priv, v, B); sn = B;
                private_key = true;
                if (ec_public_key(curve, priv, pub)) { result = dom_err("DataError", "escalar privado fora da curva"); break; }
                pn = 1 + 2 * B;
            }
        } else {
            jsv kty = jwk_str(data, "kty"), crv = jwk_str(data, "crv");
            if (!jv_isstr(kty) || strcmp(jv_s(kty)->d, "EC") || !jv_isstr(crv)) { result = dom_err("DataError", "JWK precisa de kty 'EC' e crv"); break; }
            int c2 = !strcmp(jv_s(crv)->d, "P-256") ? EC_P256 : !strcmp(jv_s(crv)->d, "P-384") ? EC_P384 : 0;
            if (!c2 || (curve && c2 != curve)) { result = dom_err("DataError", "crv não confere"); break; }
            curve = c2;
            u32 B = curve == EC_P384 ? 48 : 32;
            bytes_t x = { 0 }, y = { 0 }, dd = { 0 };
            if (!jwk_bytes(data, "x", &x) || !jwk_bytes(data, "y", &y) || x.n != B || y.n != B) { bfree(&x); bfree(&y); if (!js_pending()) dom_err("DataError", "coordenadas inválidas"); break; }
            pub[0] = 4; memcpy(pub + 1, x.d, B); memcpy(pub + 1 + B, y.d, B); pn = 1 + 2 * B;
            bfree(&x); bfree(&y);
            if (jv_isstr(jwk_str(data, "d"))) {
                if (!jwk_bytes(data, "d", &dd) || dd.n != B) { bfree(&dd); if (!js_pending()) dom_err("DataError", "d inválido"); break; }
                memcpy(priv, dd.d, B); sn = B; bfree(&dd);
                private_key = true;
                u8 chk[97];
                if (ec_public_key(curve, priv, chk) || memcmp(chk, pub, pn)) { result = dom_err("DataError", "chave privada não confere com a pública"); break; }
            }
        }
        const char *type = private_key ? "private" : "public";
        jsobj *u = usages_for(usages, a->alg, type, &ok);
        if (!ok) break;
        result = jv_obj(key_new(type, private_key ? ext : true, alg_object(a->alg, 0, 0, curve), u, private_key ? priv : NULL, sn, pub, pn));
        secure_zero(priv, sizeof priv);
        break;
    }
    case A_ED25519: case A_X25519: {
        u8 pub[32], seed[32];
        bool private_key = false;
        const u8 *myoid = a->alg == A_ED25519 ? OID_ED : OID_X;
        if (!strcmp(fmt, "raw")) {
            if (raw.n != 32) { result = dom_err("DataError", "chave de 32 bytes esperada"); break; }
            memcpy(pub, raw.d, 32);
        } else if (!strcmp(fmt, "spki") || !strcmp(fmt, "pkcs8")) {
            der_t d = { raw.d, raw.d + raw.n };
            u8 t; const u8 *v; u32 l;
            if (!der_next(&d, &t, &v, &l) || t != 0x30) { result = dom_err("DataError", "DER inválido"); break; }
            der_t s = { v, v + l };
            if (!strcmp(fmt, "pkcs8") && (!der_next(&s, &t, &v, &l) || t != 0x02)) { result = dom_err("DataError", "PKCS#8 inválido"); break; }
            if (!der_next(&s, &t, &v, &l) || t != 0x30) { result = dom_err("DataError", "AlgorithmIdentifier inválido"); break; }
            der_t ai = { v, v + l };
            const u8 *oid; u32 ol;
            if (!der_next(&ai, &t, &oid, &ol) || t != 0x06 || ol != 3 || memcmp(oid, myoid, 3)) { result = dom_err("DataError", "OID não confere"); break; }
            if (!der_next(&s, &t, &v, &l)) { result = dom_err("DataError", "chave ausente"); break; }
            if (!strcmp(fmt, "spki")) {
                if (t != 0x03 || l != 33 || v[0]) { result = dom_err("DataError", "chave pública inválida"); break; }
                memcpy(pub, v + 1, 32);
            } else {
                if (t != 0x04 || l != 34 || v[0] != 0x04 || v[1] != 32) { result = dom_err("DataError", "chave privada inválida"); break; }
                memcpy(seed, v + 2, 32);
                private_key = true;
            }
        } else {
            jsv kty = jwk_str(data, "kty"), crv = jwk_str(data, "crv");
            if (!jv_isstr(kty) || strcmp(jv_s(kty)->d, "OKP") || !jv_isstr(crv) || strcmp(jv_s(crv)->d, alg_names[a->alg])) { result = dom_err("DataError", "JWK precisa de kty 'OKP' e crv '%s'", alg_names[a->alg]); break; }
            bytes_t x = { 0 }, dd = { 0 };
            if (!jwk_bytes(data, "x", &x) || x.n != 32) { bfree(&x); if (!js_pending()) dom_err("DataError", "x inválido"); break; }
            memcpy(pub, x.d, 32);
            bfree(&x);
            if (jv_isstr(jwk_str(data, "d"))) {
                if (!jwk_bytes(data, "d", &dd) || dd.n != 32) { bfree(&dd); if (!js_pending()) dom_err("DataError", "d inválido"); break; }
                memcpy(seed, dd.d, 32); bfree(&dd);
                private_key = true;
            }
        }
        if (private_key) {
            u8 chk[32];
            if (a->alg == A_ED25519) ed25519_pubkey(chk, seed); else x25519_base(chk, seed);
            if (!strcmp(fmt, "pkcs8")) memcpy(pub, chk, 32);
            else if (memcmp(chk, pub, 32)) { result = dom_err("DataError", "chave privada não confere com a pública"); break; }
        }
        const char *type = private_key ? "private" : "public";
        jsobj *u = usages_for(usages, a->alg, type, &ok);
        if (!ok) break;
        result = jv_obj(key_new(type, private_key ? ext : true, alg_object(a->alg, 0, 0, 0), u, private_key ? seed : NULL, 32, pub, 32));
        secure_zero(seed, sizeof seed);
        break;
    }
    case A_RSAPKCS1: case A_RSAPSS: {
        if (!a->hash || a->hash == H_SHA1) { result = dom_err("NotSupportedError", "RSA: hash SHA-256/384/512 obrigatório"); break; }
        bytes_t n = { 0 }, e = { 0 };
        if (is_jwk) {
            jsv kty = jwk_str(data, "kty");
            if (!jv_isstr(kty) || strcmp(jv_s(kty)->d, "RSA")) { result = dom_err("DataError", "JWK precisa de kty 'RSA'"); break; }
            if (jv_isstr(jwk_str(data, "d"))) { result = dom_err("NotSupportedError", "chave privada RSA ainda não existe no mOS (só verificação)"); break; }
            if (!jwk_bytes(data, "n", &n) || !jwk_bytes(data, "e", &e)) { bfree(&n); bfree(&e); break; }
        } else if (!strcmp(fmt, "spki")) {
            der_t d = { raw.d, raw.d + raw.n };
            u8 t; const u8 *v; u32 l;
            if (!der_next(&d, &t, &v, &l) || t != 0x30) { result = dom_err("DataError", "DER inválido"); break; }
            der_t s = { v, v + l };
            if (!der_next(&s, &t, &v, &l) || t != 0x30) { result = dom_err("DataError", "AlgorithmIdentifier inválido"); break; }
            der_t ai = { v, v + l };
            const u8 *oid; u32 ol;
            if (!der_next(&ai, &t, &oid, &ol) || t != 0x06 || ol != sizeof OID_RSA || memcmp(oid, OID_RSA, ol)) { result = dom_err("DataError", "não é RSA"); break; }
            if (!der_next(&s, &t, &v, &l) || t != 0x03 || !l || v[0]) { result = dom_err("DataError", "BIT STRING inválida"); break; }
            der_t k = { v + 1, v + l };
            if (!der_next(&k, &t, &v, &l) || t != 0x30) { result = dom_err("DataError", "RSAPublicKey inválida"); break; }
            der_t rk = { v, v + l };
            const u8 *nv, *ev; u32 nl, el;
            if (!der_next(&rk, &t, &nv, &nl) || t != 0x02 || !der_next(&rk, &t, &ev, &el) || t != 0x02) { result = dom_err("DataError", "n/e inválidos"); break; }
            bset(&n, nv, nl); bset(&e, ev, el);
        } else { result = dom_err("NotSupportedError", "RSA: só spki ou jwk públicos"); break; }
        jsobj *u = usages_for(usages, a->alg, "public", &ok);
        if (!ok) { bfree(&n); bfree(&e); break; }
        jsobj *algo = alg_object(a->alg, a->hash, 0, 0);
        js_defn(algo, "modulusLength", jv_int((int)rsa_modulus_bits(n.d, n.n)), 0);
        jsv pe = to_abuf(e.d, e.n);
        js_defn(algo, "publicExponent", jv_obj(js_typed_new(TA_UINT8, jv_o(pe), 0, e.n)), 0);
        jsobj *key = key_new("public", true, algo, u, NULL, 0, NULL, 0);
        js_arr_set(jv_o(key->u.host.extra), K_RSAN, to_abuf(n.d, n.n));
        js_arr_set(jv_o(key->u.host.extra), K_RSAE, to_abuf(e.d, e.n));
        bfree(&n); bfree(&e);
        result = jv_obj(key);
        break;
    }
    default: result = dom_err("NotSupportedError", "importKey não vale para %s", alg_names[a->alg]);
    }
    bfree(&raw);
    return result;
}

NATIVE(sc_import) {
    UNUSED_ARGS;
    jsobj *pr = new_promise();
    jsstr *fmt = js_tostr(ARG(0));
    algo_t a;
    if (!fmt || !norm_alg(ARG(2), &a)) return reject_pending(pr);
    jsv k = import_key(fmt->d, ARG(1), &a, js_truthy(ARG(3)), ARG(4));
    if (k == JS_EXC) return reject_pending(pr);
    return resolve(pr, k);
}

/* --------------------------------------------------------- exportação */
static jsv export_key(const char *fmt, jsv key) {
    jsobj *x = key_x(key);
    if (!x) { js_throw_err(E_TYPE, "exportKey: CryptoKey esperada"); return JS_EXC; }
    if (!js_truthy(js_arr_get(x, K_EXTRACT))) return dom_err("InvalidAccessError", "a chave não é extraível");
    int alg = key_alg(x);
    const char *type = key_type(x);
    const u8 *sec = NULL, *pub = NULL;
    u32 sn = 0, pn = 0;
    key_bytes(x, K_SECRET, &sec, &sn);
    key_bytes(x, K_PUBLIC, &pub, &pn);
    if (!strcmp(fmt, "raw")) {
        if (!strcmp(type, "secret")) return to_abuf(sec, sn);
        if (!strcmp(type, "public") && pub) return to_abuf(pub, pn);
        return dom_err("InvalidAccessError", "raw só vale para chaves secretas ou públicas");
    }
    if (!strcmp(fmt, "jwk")) {
        jsobj *j = js_newobj(J.realm->p_object);
        jsobj *ops = js_arr_from(jv_o(js_arr_get(x, K_USAGES))->arr, (int)jv_o(js_arr_get(x, K_USAGES))->alen);
        if (!strcmp(type, "secret")) {
            js_defn(j, "kty", jv_str(js_strz("oct")), 0);
            js_defn(j, "k", jv_str(b64url(sec, sn)), 0);
            char an[16];
            if (alg == A_HMAC) snprintf(an, sizeof an, "HS%d", key_hash(x) == H_SHA1 ? 1 : (int)hlen(key_hash(x)) * 8);
            else snprintf(an, sizeof an, "A%u%s", sn * 8, alg == A_AESGCM ? "GCM" : "CTR");
            if (alg == A_HMAC || alg == A_AESGCM || alg == A_AESCTR) js_defn(j, "alg", jv_str(js_strz(an)), 0);
        } else if (alg == A_ECDSA || alg == A_ECDH) {
            u32 B = (pn - 1) / 2;
            js_defn(j, "kty", jv_str(js_strz("EC")), 0);
            js_defn(j, "crv", jv_str(js_strz(B == 48 ? "P-384" : "P-256")), 0);
            js_defn(j, "x", jv_str(b64url(pub + 1, B)), 0);
            js_defn(j, "y", jv_str(b64url(pub + 1 + B, B)), 0);
            if (!strcmp(type, "private")) js_defn(j, "d", jv_str(b64url(sec, sn)), 0);
        } else if (alg == A_ED25519 || alg == A_X25519) {
            js_defn(j, "kty", jv_str(js_strz("OKP")), 0);
            js_defn(j, "crv", jv_str(js_strz(alg_names[alg])), 0);
            js_defn(j, "x", jv_str(b64url(pub, pn)), 0);
            if (!strcmp(type, "private")) js_defn(j, "d", jv_str(b64url(sec, sn)), 0);
        } else if (alg == A_RSAPKCS1 || alg == A_RSAPSS) {
            const u8 *n, *e; u32 nl, el;
            key_bytes(x, K_RSAN, &n, &nl);
            key_bytes(x, K_RSAE, &e, &el);
            js_defn(j, "kty", jv_str(js_strz("RSA")), 0);
            js_defn(j, "n", jv_str(b64url(n, nl)), 0);
            js_defn(j, "e", jv_str(b64url(e, el)), 0);
        } else return dom_err("NotSupportedError", "jwk não vale para %s", alg_names[alg]);
        js_defn(j, "key_ops", jv_obj(ops), 0);
        js_defn(j, "ext", JS_TRUE, 0);
        return jv_obj(j);
    }
    if (!strcmp(fmt, "spki")) {
        if (strcmp(type, "public")) return dom_err("InvalidAccessError", "spki só vale para chaves públicas");
        sbuf_t body = { 0 }, o = { 0 };
        if (alg == A_RSAPKCS1 || alg == A_RSAPSS) {
            const u8 *n, *e; u32 nl, el;
            key_bytes(x, K_RSAN, &n, &nl);
            key_bytes(x, K_RSAE, &e, &el);
            sbuf_t ints = { 0 }, seq = { 0 };
            u8 *nn = kmalloc(nl + 1);
            if (!nn) return JS_EXC;
            u32 nnl = 0;
            if (n[0] & 0x80) nn[nnl++] = 0;
            memcpy(nn + nnl, n, nl); nnl += nl;
            der_put(&ints, 0x02, nn, nnl);
            kfree(nn);
            der_put(&ints, 0x02, e, el);
            der_put(&seq, 0x30, (const u8 *)ints.p, (u32)ints.len);
            sb_free(&ints);
            der_algid(&body, alg, 0);
            sbuf_t bit = { 0 };
            sb_putc(&bit, 0);
            sb_putn(&bit, seq.p, seq.len);
            sb_free(&seq);
            der_put(&body, 0x03, (const u8 *)bit.p, (u32)bit.len);
            sb_free(&bit);
        } else {
            der_algid(&body, alg, key_curve(x));
            sbuf_t bit = { 0 };
            sb_putc(&bit, 0);
            sb_putn(&bit, (const char *)pub, (int)pn);
            der_put(&body, 0x03, (const u8 *)bit.p, (u32)bit.len);
            sb_free(&bit);
        }
        der_put(&o, 0x30, (const u8 *)body.p, (u32)body.len);
        sb_free(&body);
        jsv r = to_abuf((const u8 *)o.p, (u32)o.len);
        sb_free(&o);
        return r;
    }
    if (!strcmp(fmt, "pkcs8")) {
        if (strcmp(type, "private")) return dom_err("InvalidAccessError", "pkcs8 só vale para chaves privadas");
        sbuf_t body = { 0 }, o = { 0 }, inner = { 0 };
        u8 zero = 0, one = 1;
        der_put(&body, 0x02, &zero, 1);
        der_algid(&body, alg, key_curve(x));
        if (alg == A_ECDSA || alg == A_ECDH) {
            sbuf_t ec = { 0 }, bit = { 0 }, tagged = { 0 };
            der_put(&ec, 0x02, &one, 1);
            der_put(&ec, 0x04, sec, sn);
            sb_putc(&bit, 0);
            sb_putn(&bit, (const char *)pub, (int)pn);
            der_put(&tagged, 0x03, (const u8 *)bit.p, (u32)bit.len);
            der_put(&ec, 0xA1, (const u8 *)tagged.p, (u32)tagged.len);
            der_put(&inner, 0x30, (const u8 *)ec.p, (u32)ec.len);
            sb_free(&ec); sb_free(&bit); sb_free(&tagged);
        } else der_put(&inner, 0x04, sec, sn);
        der_put(&body, 0x04, (const u8 *)inner.p, (u32)inner.len);
        der_put(&o, 0x30, (const u8 *)body.p, (u32)body.len);
        sb_free(&inner); sb_free(&body);
        jsv r = to_abuf((const u8 *)o.p, (u32)o.len);
        sb_free(&o);
        return r;
    }
    js_throw_err(E_TYPE, "formato desconhecido: %s", fmt);
    return JS_EXC;
}

NATIVE(sc_export) {
    UNUSED_ARGS;
    jsobj *pr = new_promise();
    jsstr *fmt = js_tostr(ARG(0));
    if (!fmt) return reject_pending(pr);
    jsv r = export_key(fmt->d, ARG(1));
    if (r == JS_EXC) return reject_pending(pr);
    return resolve(pr, r);
}

/* --------------------------------------------------------- sign / verify */
static jsv sign_verify(algo_t *a, jsv key, bytes_t *data, bytes_t *sig, bool verify) {
    jsobj *x = key_x(key);
    if (!x) { js_throw_err(E_TYPE, "CryptoKey esperada"); return JS_EXC; }
    int alg = key_alg(x);
    if (alg != a->alg) return dom_err("InvalidAccessError", "a chave é de %s, não de %s", alg_names[alg], alg_names[a->alg]);
    if (!has_usage(x, verify ? "verify" : "sign")) return dom_err("InvalidAccessError", "a chave não permite %s", verify ? "verify" : "sign");
    const u8 *sec = NULL, *pub = NULL;
    u32 sn = 0, pn = 0;
    key_bytes(x, K_SECRET, &sec, &sn);
    key_bytes(x, K_PUBLIC, &pub, &pn);
    switch (alg) {
    case A_HMAC: {
        u8 mac[64];
        int h = key_hash(x);
        hmac_any(h, sec, sn, data->d, data->n, mac);
        u32 ml = hlen(h);
        if (!verify) return to_abuf(mac, ml);
        return jv_bool(sig->n == ml && ct_memeq(mac, sig->d, ml));
    }
    case A_ECDSA: {
        if (!a->hash) { js_throw_err(E_TYPE, "ECDSA precisa de hash"); return JS_EXC; }
        int curve = key_curve(x);
        u32 B = curve == EC_P384 ? 48 : 32;
        u8 dg[64];
        digest(a->hash, data->d, data->n, dg);
        u32 dl = hlen(a->hash);
        if (!verify) {
            u8 rs[96];
            if (ecdsa_sign(curve, sec, dg, dl, rs, rs + B)) return dom_err("OperationError", "falha na assinatura ECDSA");
            return to_abuf(rs, 2 * B);
        }
        if (sig->n != 2 * B) return JS_FALSE;
        return jv_bool(!ecdsa_verify(curve, pub, pn, dg, dl, sig->d, B, sig->d + B, B));
    }
    case A_ED25519: {
        if (!verify) {
            u8 s[64];
            ed25519_sign(s, data->d, data->n, sec, pub);
            return to_abuf(s, 64);
        }
        if (sig->n != 64) return JS_FALSE;
        return jv_bool(ed25519_verify(sig->d, data->d, data->n, pub) == 1);
    }
    case A_RSAPKCS1: case A_RSAPSS: {
        if (!verify) return dom_err("NotSupportedError", "assinatura RSA ainda não existe no mOS (só verificação)");
        const u8 *n, *e; u32 nl, el;
        key_bytes(x, K_RSAN, &n, &nl);
        key_bytes(x, K_RSAE, &e, &el);
        int h = key_hash(x);
        u8 dg[64];
        digest(h, data->d, data->n, dg);
        if (alg == A_RSAPKCS1) return jv_bool(!rsa_verify_pkcs1(n, nl, e, el, kalg(h), dg, sig->d, sig->n));
        jsv sl = js_getn(a->obj, "saltLength");
        if (js_pending()) return JS_EXC;
        int salt = sl == JS_UNDEF ? -1 : js_toint32(sl);
        return jv_bool(!rsa_verify_pss(n, nl, e, el, kalg(h), dg, sig->d, sig->n, salt));
    }
    }
    return dom_err("InvalidAccessError", "%s não assina", alg_names[alg]);
}

NATIVE(sc_sign) {
    UNUSED_ARGS;
    bool verify = self->u.nat.magic == 1;
    jsobj *pr = new_promise();
    algo_t a;
    bytes_t data = { 0 }, sig = { 0 };
    if (!norm_alg(ARG(0), &a)) return reject_pending(pr);
    if (verify && !buffer_source(ARG(2), &sig)) return reject_pending(pr);
    if (!buffer_source(ARG(verify ? 3 : 2), &data)) { bfree(&sig); return reject_pending(pr); }
    jsv r = sign_verify(&a, ARG(1), &data, &sig, verify);
    bfree(&data); bfree(&sig);
    if (r == JS_EXC) return reject_pending(pr);
    return resolve(pr, r);
}

/* --------------------------------------------------------- encrypt / decrypt */
static void ctr_xor(const aes_ctx *k, const u8 ctr0[16], u32 len_bits, const u8 *in, u8 *out, u32 n) {
    u8 ctr[16], ks[16];
    memcpy(ctr, ctr0, 16);
    for (u32 off = 0; off < n; off += 16) {
        aes_encrypt_block(k, ctr, ks);
        for (u32 i = 0; i < 16 && off + i < n; i++) out[off + i] = in[off + i] ^ ks[i];
        /* incrementa só os len_bits de baixo do contador */
        for (int i = 15, bits = (int)len_bits; i >= 0 && bits > 0; i--, bits -= 8) {
            u8 mask = bits >= 8 ? 0xFF : (u8)((1u << bits) - 1);
            u8 v = (u8)((ctr[i] & mask) + 1) & mask;
            ctr[i] = (u8)((ctr[i] & ~mask) | v);
            if (v) break;
        }
    }
}

static jsv crypt(algo_t *a, jsv key, bytes_t *data, bool dec) {
    jsobj *x = key_x(key);
    if (!x) { js_throw_err(E_TYPE, "CryptoKey esperada"); return JS_EXC; }
    int alg = key_alg(x);
    if (alg != a->alg) return dom_err("InvalidAccessError", "a chave é de %s, não de %s", alg_names[alg], alg_names[a->alg]);
    if (!has_usage(x, dec ? "decrypt" : "encrypt") && !has_usage(x, dec ? "unwrapKey" : "wrapKey"))
        return dom_err("InvalidAccessError", "a chave não permite %s", dec ? "decrypt" : "encrypt");
    const u8 *sec; u32 sn;
    key_bytes(x, K_SECRET, &sec, &sn);
    if (alg == A_AESGCM) {
        jsv ivv = js_getn(a->obj, "iv"), adv = js_getn(a->obj, "additionalData"), tlv = js_getn(a->obj, "tagLength");
        if (js_pending()) return JS_EXC;
        bytes_t iv = { 0 }, ad = { 0 };
        if (!buffer_source(ivv, &iv)) return JS_EXC;
        if (adv != JS_UNDEF && !buffer_source(adv, &ad)) { bfree(&iv); return JS_EXC; }
        int tl = tlv == JS_UNDEF ? 128 : js_toint32(tlv);
        jsv r = JS_EXC;
        if (iv.n != 12) r = dom_err("NotSupportedError", "AES-GCM no mOS usa IV de 12 bytes");
        else if (tl != 128) r = dom_err("NotSupportedError", "AES-GCM no mOS usa tag de 128 bits");
        else {
            aes_gcm_ctx g;
            aes_gcm_init(&g, sec, sn);
            if (!dec) {
                u8 *out = kmalloc(data->n + 16);
                if (out) { aes_gcm_seal(&g, iv.d, ad.d, ad.n, data->d, data->n, out, out + data->n); r = to_abuf(out, data->n + 16); kfree(out); }
            } else if (data->n < 16) r = dom_err("OperationError", "dados cifrados curtos demais");
            else {
                u8 *out = kmalloc(data->n);
                if (out) {
                    if (aes_gcm_open(&g, iv.d, ad.d, ad.n, data->d, data->n - 16, out, data->d + data->n - 16)) r = dom_err("OperationError", "a autenticação AES-GCM falhou");
                    else r = to_abuf(out, data->n - 16);
                    kfree(out);
                }
            }
            secure_zero(&g, sizeof g);
        }
        bfree(&iv); bfree(&ad);
        return r;
    }
    if (alg == A_AESCTR) {
        jsv cv = js_getn(a->obj, "counter");
        if (js_pending()) return JS_EXC;
        bytes_t c = { 0 };
        if (!buffer_source(cv, &c)) return JS_EXC;
        u32 lb = a->length;
        if (c.n != 16 || lb < 1 || lb > 128) { bfree(&c); return dom_err("OperationError", "AES-CTR: counter de 16 bytes e length de 1 a 128"); }
        aes_ctx k;
        aes_setkey(&k, sec, sn);
        u8 *out = kmalloc(data->n ? data->n : 1);
        jsv r = JS_EXC;
        if (out) { ctr_xor(&k, c.d, lb, data->d, out, data->n); r = to_abuf(out, data->n); kfree(out); }
        secure_zero(&k, sizeof k);
        bfree(&c);
        return r;
    }
    return dom_err("InvalidAccessError", "%s não cifra", alg_names[alg]);
}

NATIVE(sc_crypt) {
    UNUSED_ARGS;
    jsobj *pr = new_promise();
    algo_t a;
    bytes_t data = { 0 };
    if (!norm_alg(ARG(0), &a) || !buffer_source(ARG(2), &data)) return reject_pending(pr);
    jsv r = crypt(&a, ARG(1), &data, self->u.nat.magic == 1);
    bfree(&data);
    if (r == JS_EXC) return reject_pending(pr);
    return resolve(pr, r);
}

/* --------------------------------------------------------- deriveBits / deriveKey */
static jsv derive_bits(algo_t *a, jsv key, u32 bits, bool from_derive_key) {
    jsobj *x = key_x(key);
    if (!x) { js_throw_err(E_TYPE, "CryptoKey esperada"); return JS_EXC; }
    int alg = key_alg(x);
    if (alg != a->alg) return dom_err("InvalidAccessError", "a chave é de %s, não de %s", alg_names[alg], alg_names[a->alg]);
    if (!has_usage(x, from_derive_key ? "deriveKey" : "deriveBits")) return dom_err("InvalidAccessError", "a chave não permite derivação");
    const u8 *sec; u32 sn;
    key_bytes(x, K_SECRET, &sec, &sn);
    switch (alg) {
    case A_PBKDF2: case A_HKDF: {
        if (!bits || bits % 8) return dom_err("OperationError", "length precisa ser múltiplo de 8 e maior que 0");
        if (!a->hash) { js_throw_err(E_TYPE, "%s precisa de hash", alg_names[alg]); return JS_EXC; }
        jsv sv = js_getn(a->obj, "salt");
        if (js_pending()) return JS_EXC;
        bytes_t salt = { 0 };
        if (!buffer_source(sv, &salt)) return JS_EXC;
        u8 *out = kmalloc(bits / 8);
        if (!out) { bfree(&salt); return JS_EXC; }
        jsv r;
        if (alg == A_PBKDF2) {
            jsv it = js_getn(a->obj, "iterations");
            double iters = js_tonum(it);
            if (js_pending()) { kfree(out); bfree(&salt); return JS_EXC; }
            if (!(iters >= 1) || iters > 100000000) r = dom_err("OperationError", "PBKDF2: iterations inválido");
            else if (!pbkdf2_any(a->hash, sec, sn, salt.d, salt.n, (u32)iters, out, bits / 8)) r = JS_EXC;
            else r = to_abuf(out, bits / 8);
        } else {
            jsv iv = js_getn(a->obj, "info");
            bytes_t info = { 0 };
            if (js_pending() || !buffer_source(iv, &info)) { kfree(out); bfree(&salt); return JS_EXC; }
            if (!hkdf_any(a->hash, sec, sn, salt.d, salt.n, info.d, info.n, out, bits / 8)) r = dom_err("OperationError", "HKDF: length grande demais");
            else r = to_abuf(out, bits / 8);
            bfree(&info);
        }
        secure_zero(out, bits / 8);
        kfree(out);
        bfree(&salt);
        return r;
    }
    case A_ECDH: case A_X25519: {
        jsv pv = js_getn(a->obj, "public");
        if (js_pending()) return JS_EXC;
        jsobj *px = key_x(pv);
        if (!px || strcmp(key_type(px), "public") || key_alg(px) != alg) return dom_err("InvalidAccessError", "public precisa ser uma chave pública de %s", alg_names[alg]);
        const u8 *pp; u32 pl;
        key_bytes(px, K_PUBLIC, &pp, &pl);
        u8 shared[48];
        u32 B;
        if (alg == A_ECDH) {
            int curve = key_curve(x);
            if (key_curve(px) != curve) return dom_err("InvalidAccessError", "as curvas não conferem");
            B = curve == EC_P384 ? 48 : 32;
            if (ecdh_shared(curve, shared, sec, pp, pl)) return dom_err("OperationError", "ECDH falhou");
        } else {
            B = 32;
            x25519(shared, sec, pp);
            u8 z = 0;
            for (int i = 0; i < 32; i++) z |= shared[i];
            if (!z) return dom_err("OperationError", "X25519: ponto de ordem baixa");
        }
        if (bits == 0xFFFFFFFFu) bits = B * 8;
        if (bits > B * 8) return dom_err("OperationError", "length maior que o segredo");
        u32 nb = (bits + 7) / 8;
        if (bits % 8) shared[nb - 1] &= (u8)(0xFF << (8 - bits % 8));
        jsv r = to_abuf(shared, nb);
        secure_zero(shared, sizeof shared);
        return r;
    }
    }
    return dom_err("InvalidAccessError", "%s não deriva", alg_names[alg]);
}

NATIVE(sc_derivebits) {
    UNUSED_ARGS;
    jsobj *pr = new_promise();
    algo_t a;
    if (!norm_alg(ARG(0), &a)) return reject_pending(pr);
    u32 bits = 0xFFFFFFFFu;
    if (!jv_isnullish(ARG(2))) { double d = js_tonum(ARG(2)); if (js_pending()) return reject_pending(pr); bits = d >= 0 && d < 1e7 ? (u32)d : 0; }
    if ((a.alg == A_PBKDF2 || a.alg == A_HKDF) && bits == 0xFFFFFFFFu) { dom_err("OperationError", "length obrigatório"); return reject_pending(pr); }
    jsv r = derive_bits(&a, ARG(1), bits, false);
    if (r == JS_EXC) return reject_pending(pr);
    return resolve(pr, r);
}

NATIVE(sc_derivekey) {
    UNUSED_ARGS;
    jsobj *pr = new_promise();
    algo_t a, d;
    if (!norm_alg(ARG(0), &a) || !norm_alg(ARG(2), &d)) return reject_pending(pr);
    u32 bits;
    if (d.alg == A_AESGCM || d.alg == A_AESCTR) bits = d.length;
    else if (d.alg == A_HMAC) bits = d.length ? d.length : (d.hash ? hblock(d.hash) * 8 : 0);
    else if (d.alg == A_HKDF || d.alg == A_PBKDF2) bits = 0xFFFFFFFFu;
    else { dom_err("NotSupportedError", "deriveKey: alvo %s não suportado", alg_names[d.alg]); return reject_pending(pr); }
    if (bits == 0xFFFFFFFFu) { dom_err("NotSupportedError", "deriveKey: alvo sem comprimento"); return reject_pending(pr); }
    jsv raw = derive_bits(&a, ARG(1), bits, true);
    if (raw == JS_EXC) return reject_pending(pr);
    jsv k = import_key("raw", raw, &d, js_truthy(ARG(3)), ARG(4));
    if (k == JS_EXC) return reject_pending(pr);
    return resolve(pr, k);
}

/* --------------------------------------------------------- wrapKey / unwrapKey */
NATIVE(sc_wrap) {
    UNUSED_ARGS;
    jsobj *pr = new_promise();
    jsstr *fmt = js_tostr(ARG(0));
    algo_t a;
    if (!fmt || !norm_alg(ARG(3), &a)) return reject_pending(pr);
    jsobj *wx = key_x(ARG(2));
    if (!wx || !has_usage(wx, "wrapKey")) { dom_err("InvalidAccessError", "a chave de embrulho não permite wrapKey"); return reject_pending(pr); }
    jsv ex = export_key(fmt->d, ARG(1));
    if (ex == JS_EXC) return reject_pending(pr);
    bytes_t data = { 0 };
    if (!strcmp(fmt->d, "jwk")) {
        jsv json = js_getn(jv_obj(J.realm->global), "JSON");
        jsv txt = js_invoke(json, js_intern("stringify", -1), 1, &ex);
        if (txt == JS_EXC || !jv_isstr(txt)) return reject_pending(pr);
        bset(&data, (const u8 *)jv_s(txt)->d, jv_s(txt)->len);
    } else if (!buffer_source(ex, &data)) return reject_pending(pr);
    jsv r = crypt(&a, ARG(2), &data, false);
    bfree(&data);
    if (r == JS_EXC) return reject_pending(pr);
    return resolve(pr, r);
}

NATIVE(sc_unwrap) {
    UNUSED_ARGS;
    jsobj *pr = new_promise();
    jsstr *fmt = js_tostr(ARG(0));
    algo_t a, ka;
    if (!fmt || !norm_alg(ARG(3), &a) || !norm_alg(ARG(4), &ka)) return reject_pending(pr);
    jsobj *wx = key_x(ARG(2));
    if (!wx || !has_usage(wx, "unwrapKey")) { dom_err("InvalidAccessError", "a chave de desembrulho não permite unwrapKey"); return reject_pending(pr); }
    bytes_t data = { 0 };
    if (!buffer_source(ARG(1), &data)) return reject_pending(pr);
    jsv plain = crypt(&a, ARG(2), &data, true);
    bfree(&data);
    if (plain == JS_EXC) return reject_pending(pr);
    jsv kd = plain;
    if (!strcmp(fmt->d, "jwk")) {
        bool bad;
        jsstr *s = js_utf8_decode(jv_o(plain)->u.buf.data, jv_o(plain)->u.buf.len, false, &bad);
        kd = s ? js_json_parse(s, JS_UNDEF) : JS_EXC;
        if (kd == JS_EXC) { js_catch(); dom_err("DataError", "o JWK desembrulhado é inválido"); return reject_pending(pr); }
    }
    jsv k = import_key(fmt->d, kd, &ka, js_truthy(ARG(5)), ARG(6));
    if (k == JS_EXC) return reject_pending(pr);
    return resolve(pr, k);
}

/* --------------------------------------------------------- CryptoKey: propriedades */
NATIVE(ck_get) {
    UNUSED_ARGS;
    jsobj *x = key_x(thisv);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal: não é uma CryptoKey");
    int m = self->u.nat.magic;
    if (m == K_USAGES) { jsobj *u = jv_o(js_arr_get(x, K_USAGES)); return jv_obj(js_arr_from(u->arr, (int)u->alen)); }
    return js_arr_get(x, m);
}

NATIVE(illegal) { UNUSED_ARGS; return js_throw_err(E_TYPE, "Construtor ilegal"); }

jsobj *js_subtle_init(jsrealm *r) {
    jsobj *g = r->global, *op = r->p_object;
    r->p_cryptokey = js_newobj(op);
    js_defn(g, "CryptoKey", jv_obj(js_newctor(illegal, "CryptoKey", 0, r->p_cryptokey)), PF_NOENUM);
    js_defprop(r->p_cryptokey, A[A_sym_toStringTag], jv_str(js_intern("CryptoKey", -1)), PF_NOENUM | PF_RO);
    js_defgetter(r->p_cryptokey, "type", ck_get, K_TYPE);
    js_defgetter(r->p_cryptokey, "extractable", ck_get, K_EXTRACT);
    js_defgetter(r->p_cryptokey, "algorithm", ck_get, K_ALG);
    js_defgetter(r->p_cryptokey, "usages", ck_get, K_USAGES);
    jsobj *sp = js_newobj(op);
    js_defn(g, "SubtleCrypto", jv_obj(js_newctor(illegal, "SubtleCrypto", 0, sp)), PF_NOENUM);
    js_defprop(sp, A[A_sym_toStringTag], jv_str(js_intern("SubtleCrypto", -1)), PF_NOENUM | PF_RO);
    js_defnative(sp, "digest", sc_digest, 2, 0);
    js_defnative(sp, "generateKey", sc_generate, 3, 0);
    js_defnative(sp, "importKey", sc_import, 5, 0);
    js_defnative(sp, "exportKey", sc_export, 2, 0);
    js_defnative(sp, "sign", sc_sign, 3, 0);
    js_defnative(sp, "verify", sc_sign, 4, 1);
    js_defnative(sp, "encrypt", sc_crypt, 3, 0);
    js_defnative(sp, "decrypt", sc_crypt, 3, 1);
    js_defnative(sp, "deriveBits", sc_derivebits, 3, 0);
    js_defnative(sp, "deriveKey", sc_derivekey, 5, 0);
    js_defnative(sp, "wrapKey", sc_wrap, 4, 0);
    js_defnative(sp, "unwrapKey", sc_unwrap, 7, 0);
    return js_newobj(sp);
}
