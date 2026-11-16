/* mOS libSystem — núcleo de seccert.c sem CF: nomes X.509, PEM, leitor BER e PKCS#12 (RFC 7292).
 * Incluído por seccert.c e por tools/seccerttest.c. Quem inclui fornece u8/u32/u64, malloc/realloc/free/mem*,
 * crypto.h e x509.h; as cifras vêm do CCCrypt do libSystem (commoncrypto2.c). */

int CCCrypt(u32 op, u32 alg, u32 options, const void *key, size_t klen, const void *iv, const void *in, size_t inlen,
            void *out, size_t outcap, size_t *moved);

/* ================================================================ buffer */
typedef struct { u8 *b; usize n, cap; int err; } sk_buf;
static void sb_put(sk_buf *s, const void *d, usize n) {
    if (s->err) return;
    if (s->n + n > s->cap) {
        usize nc = s->cap ? s->cap * 2 : 256;
        while (nc < s->n + n) nc *= 2;
        u8 *nb = realloc(s->b, nc);
        if (!nb) { s->err = 1; return; }
        s->b = nb;
        s->cap = nc;
    }
    memcpy(s->b + s->n, d, n);
    s->n += n;
}
static int sc_has(const char *s, const char *w) {
    usize wl = strlen(w);
    for (; s && *s; s++) if (!memcmp(s, w, wl)) return 1;
    return 0;
}

/* ================================================================ leitor BER (DER + comprimento indefinido) */
typedef struct { const u8 *p, *end; } be_rd;
static int be_len_ind(const u8 *p, const u8 *end, usize *total);
/* um TLV: conteúdo em v/n; cons = construído; em comprimento indefinido o conteúdo vai até o 00 00 */
static int be_get(be_rd *r, u8 *tag, const u8 **v, usize *n, int *cons) {
    if (r->end - r->p < 2) return -1;
    const u8 *start = r->p;
    *tag = *r->p++;
    if ((*tag & 0x1f) == 0x1f) return -1;                  /* tags de vários bytes: não usadas no PKCS#12 */
    if (cons) *cons = (*tag & 0x20) != 0;
    usize len = *r->p++;
    if (len == 0x80) {
        if (!(*tag & 0x20)) return -1;
        usize total;
        if (be_len_ind(start, r->end, &total)) return -1;
        *v = r->p;
        *n = (usize)(start + total - 2 - r->p);
        r->p = start + total;
        return 0;
    }
    if (len & 0x80) {
        int k = (int)(len & 0x7f);
        if (k < 1 || k > 4 || r->end - r->p < k) return -1;
        len = 0;
        while (k--) len = len << 8 | *r->p++;
    }
    if ((usize)(r->end - r->p) < len) return -1;
    *v = r->p;
    *n = len;
    r->p += len;
    return 0;
}
/* tamanho total (cabeçalho + conteúdo + 00 00) de um TLV de comprimento indefinido que começa em p */
static int be_len_ind(const u8 *p, const u8 *end, usize *total) {
    const u8 *q = p + 2;
    for (int guard = 0; guard < 100000; guard++) {
        if (end - q < 2) return -1;
        if (q[0] == 0 && q[1] == 0) { *total = (usize)(q + 2 - p); return 0; }
        be_rd r = {q, end};
        u8 t;
        const u8 *v;
        usize n;
        if (be_get(&r, &t, &v, &n, 0)) return -1;
        q = r.p;
    }
    return -1;
}
static int be_expect(be_rd *r, u8 want, const u8 **v, usize *n) {
    u8 t;
    if (be_get(r, &t, v, n, 0) || t != want) return -1;
    return 0;
}
/* OCTET STRING primitiva ou construída: junta os pedaços (malloc) */
static int be_octets_cat(const u8 *v, usize n, int cons, sk_buf *out) {
    if (!cons) { sb_put(out, v, n); return out->err ? -1 : 0; }
    be_rd r = {v, v + n};
    while (r.p < r.end) {
        u8 t;
        const u8 *cv;
        usize cn;
        int cc;
        if (be_get(&r, &t, &cv, &cn, &cc) || (t & 0x1f) != 0x04) return -1;
        if (be_octets_cat(cv, cn, cc, out)) return -1;
    }
    return 0;
}
static int be_get_octets(be_rd *r, u8 want_tag_low, u8 **out, usize *on) {
    u8 t;
    const u8 *v;
    usize n;
    int cons;
    if (be_get(r, &t, &v, &n, &cons) || (t & 0xdf) != want_tag_low) return -1;
    sk_buf b = {0};
    if (be_octets_cat(v, n, cons, &b)) { free(b.b); return -1; }
    if (!b.b) b.b = malloc(1);
    *out = b.b;
    *on = b.n;
    return 0;
}
static int be_get_uint(be_rd *r, u32 *val) {
    const u8 *v;
    usize n;
    if (be_expect(r, 0x02, &v, &n) || !n || n > 5 || (v[0] & 0x80)) return -1;
    u64 x = 0;
    for (usize i = 0; i < n; i++) x = x << 8 | v[i];
    if (x > 0xffffffffu) return -1;
    *val = (u32)x;
    return 0;
}
static int oid_is(const u8 *v, usize n, const u8 *o, usize on) { return n == on && !memcmp(v, o, n); }

/* ================================================================ nomes X.509 */
static const u8 OIDS_CN[] = {0x55, 0x04, 0x03}, OIDS_O[] = {0x55, 0x04, 0x0a}, OIDS_OU[] = {0x55, 0x04, 0x0b};
static const u8 OIDS_EMAIL[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01};
/* valor de string ASN.1 -> UTF-8 */
static void sc_str_utf8(u8 tag, const u8 *v, usize n, char *out, usize cap) {
    usize o = 0;
    if (!cap) return;
    if (tag == 0x1e) {                                      /* BMPString (UTF-16BE) */
        for (usize i = 0; i + 1 < n && o + 4 < cap; i += 2) {
            u32 c = (u32)v[i] << 8 | v[i + 1];
            if (c >= 0xd800 && c < 0xdc00 && i + 3 < n) { u32 lo = (u32)v[i + 2] << 8 | v[i + 3]; c = 0x10000 + ((c - 0xd800) << 10) + (lo - 0xdc00); i += 2; }
            if (c < 0x80) out[o++] = (char)c;
            else if (c < 0x800) { out[o++] = (char)(0xc0 | c >> 6); out[o++] = (char)(0x80 | (c & 63)); }
            else if (c < 0x10000) { out[o++] = (char)(0xe0 | c >> 12); out[o++] = (char)(0x80 | ((c >> 6) & 63)); out[o++] = (char)(0x80 | (c & 63)); }
            else { out[o++] = (char)(0xf0 | c >> 18); out[o++] = (char)(0x80 | ((c >> 12) & 63)); out[o++] = (char)(0x80 | ((c >> 6) & 63)); out[o++] = (char)(0x80 | (c & 63)); }
        }
    } else if (tag == 0x14 || tag == 0x1c) {                /* T61 (como Latin-1) e Universal (UTF-32BE): só BMP básico */
        usize step = tag == 0x1c ? 4 : 1;
        for (usize i = 0; i + step - 1 < n && o + 3 < cap; i += step) {
            u32 c = step == 4 ? ((u32)v[i + 2] << 8 | v[i + 3]) : v[i];
            if (c < 0x80) out[o++] = (char)c;
            else if (c < 0x800) { out[o++] = (char)(0xc0 | c >> 6); out[o++] = (char)(0x80 | (c & 63)); }
            else { out[o++] = (char)(0xe0 | c >> 12); out[o++] = (char)(0x80 | ((c >> 6) & 63)); out[o++] = (char)(0x80 | (c & 63)); }
        }
    } else {
        for (usize i = 0; i < n && o + 1 < cap; i++) out[o++] = (char)v[i];
    }
    out[o] = 0;
}
static void oid_dotted(const u8 *v, usize n, char *out, usize cap) {
    usize o = 0;
    u64 x = 0;
    int first = 1;
    out[0] = 0;
    for (usize i = 0; i < n; i++) {
        x = x << 7 | (v[i] & 0x7f);
        if (v[i] & 0x80) continue;
        char tmp[48];
        int k;
        if (first) { u64 a = x < 80 ? x / 40 : 2, b = x - a * 40; k = cx_snprintf(tmp, sizeof tmp, "%llu.%llu", a, b); first = 0; }
        else k = cx_snprintf(tmp, sizeof tmp, ".%llu", x);
        for (int j = 0; j < k && o + 1 < cap; j++) out[o++] = tmp[j];
        out[o] = 0;
        x = 0;
    }
}
typedef struct { be_rd sets; be_rd cur; int in_set; } sc_attr_iter;
static void sc_attr_begin(sc_attr_iter *it, const u8 *name, u32 n) {
    be_rd r = {name, name + n};
    const u8 *v;
    usize vn;
    it->in_set = 0;
    if (be_expect(&r, 0x30, &v, &vn)) { it->sets.p = it->sets.end = name; return; }
    it->sets.p = v;
    it->sets.end = v + vn;
}
/* próximo par (OID pontilhado, valor UTF-8); devolve 0 no fim. raw_oid opcional. */
static int sc_attr_next_raw(sc_attr_iter *it, const u8 **oidv, usize *oidn, char *val, usize vcap) {
    for (;;) {
        if (!it->in_set) {
            const u8 *v;
            usize vn;
            if (it->sets.p >= it->sets.end || be_expect(&it->sets, 0x31, &v, &vn)) return 0;
            it->cur.p = v;
            it->cur.end = v + vn;
            it->in_set = 1;
        }
        if (it->cur.p >= it->cur.end) { it->in_set = 0; continue; }
        const u8 *sv, *ov, *vv;
        usize sn, on, vn;
        u8 tag;
        if (be_expect(&it->cur, 0x30, &sv, &sn)) return 0;
        be_rd s = {sv, sv + sn};
        if (be_expect(&s, 0x06, &ov, &on) || be_get(&s, &tag, &vv, &vn, 0)) return 0;
        *oidv = ov;
        *oidn = on;
        sc_str_utf8(tag, vv, vn, val, vcap);
        return 1;
    }
}
static int sc_attr_next(sc_attr_iter *it, char *oid, usize ocap, char *val, usize vcap) {
    const u8 *ov;
    usize on;
    if (!sc_attr_next_raw(it, &ov, &on, val, vcap)) return 0;
    oid_dotted(ov, on, oid, ocap);
    return 1;
}
static int sc_name_attr_n(const u8 *name, u32 n, const u8 *oid, usize oidn, int idx, char *out, usize cap) {
    sc_attr_iter it;
    const u8 *ov;
    usize on;
    sc_attr_begin(&it, name, n);
    while (sc_attr_next_raw(&it, &ov, &on, out, cap))
        if (oid_is(ov, on, oid, oidn) && idx-- == 0) return 1;
    out[0] = 0;
    return 0;
}
static int sc_name_attr(const u8 *name, u32 n, const u8 *oid, usize oidn, char *out, usize cap) {
    return sc_name_attr_n(name, n, oid, oidn, 0, out, cap);
}
/* GeneralNames: tag 0x81 = rfc822Name, 0x82 = dNSName, 0x86 = URI */
static int sc_san_n(const u8 *san, u32 n, u8 tag, int idx, char *out, usize cap) {
    be_rd r = {san, san + n};
    out[0] = 0;
    while (san && r.p < r.end) {
        u8 t;
        const u8 *v;
        usize vn;
        if (be_get(&r, &t, &v, &vn, 0)) return 0;
        if ((t == tag || (tag == 0 && (t == 0x81 || t == 0x82))) && idx-- == 0) {
            usize k = vn < cap - 1 ? vn : cap - 1;
            memcpy(out, v, k);
            out[k] = 0;
            return 1;
        }
    }
    return 0;
}
static int sc_san_first(const u8 *san, u32 n, char *out, usize cap) { return sc_san_n(san, n, 0, 0, out, cap); }
/* AAAAMMDDhhmmss -> CFAbsoluteTime (segundos desde 2001-01-01 UTC) */
static double sc_abs_time(u64 t) {
    i64 y = (i64)(t / 10000000000ull), mo = (i64)(t / 100000000ull % 100), d = (i64)(t / 1000000ull % 100);
    i64 h = (i64)(t / 10000 % 100), mi = (i64)(t / 100 % 100), s = (i64)(t % 100);
    y -= mo <= 2;
    i64 era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400;
    i64 doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    i64 unix_s = (era * 146097 + doe - 719468) * 86400 + h * 3600 + mi * 60 + s;
    return (double)(unix_s - 978307200);
}

/* ================================================================ PEM */
static int sc_is_pem(const u8 *b, u32 n) {
    for (u32 i = 0; i + 11 <= n && i < 4096; i++) if (!memcmp(b + i, "-----BEGIN ", 11)) return 1;
    return 0;
}
typedef struct { const u8 *p, *end; char label[64]; } sc_pem_iter;
static void sc_pem_begin(sc_pem_iter *it, const u8 *b, u32 n) { it->p = b; it->end = b + n; }
static int b64val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    return c == '+' ? 62 : c == '/' ? 63 : -1;
}
static const u8 *find_bytes(const u8 *p, const u8 *end, const char *s) {
    usize n = strlen(s);
    for (; p + n <= end; p++) if (*p == (u8)s[0] && !memcmp(p, s, n)) return p;
    return 0;
}
static int sc_pem_next(sc_pem_iter *it, const char **label, u8 **der, u32 *dn, int *encrypted) {
    const u8 *b = find_bytes(it->p, it->end, "-----BEGIN ");
    if (!b) return 0;
    b += 11;
    const u8 *le = find_bytes(b, it->end, "-----");
    if (!le) return 0;
    usize ln = (usize)(le - b) < sizeof it->label - 1 ? (usize)(le - b) : sizeof it->label - 1;
    memcpy(it->label, b, ln);
    it->label[ln] = 0;
    const u8 *body = le + 5;
    const u8 *e = find_bytes(body, it->end, "-----END ");
    if (!e) return 0;
    *encrypted = sc_has(it->label, "ENCRYPTED");
    /* cabeçalhos do formato antigo do OpenSSL (Proc-Type: 4,ENCRYPTED / DEK-Info) */
    const u8 *hdr = find_bytes(body, e, "Proc-Type:");
    if (hdr && find_bytes(hdr, e, "ENCRYPTED")) *encrypted = 1;
    const u8 *dek = find_bytes(body, e, "DEK-Info:");
    if (dek) { const u8 *nl = find_bytes(dek, e, "\n\n"); if (!nl) nl = find_bytes(dek, e, "\r\n\r\n"); if (nl) body = nl; }
    u8 *out = malloc((usize)(e - body) / 4 * 3 + 4);
    if (!out) return 0;
    u32 acc = 0, bits = 0, o = 0;
    for (const u8 *p = body; p < e; p++) {
        int v = b64val(*p);
        if (v < 0) continue;
        acc = acc << 6 | (u32)v;
        bits += 6;
        if (bits >= 8) { bits -= 8; out[o++] = (u8)(acc >> bits); }
    }
    const u8 *after = find_bytes(e + 9, it->end, "-----");
    it->p = after ? after + 5 : it->end;
    *label = it->label;
    *der = out;
    *dn = o;
    return 1;
}
static void sc_pem_write(sk_buf *s, const char *label, const u8 *der, u32 n) {
    static const char A[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    sb_put(s, "-----BEGIN ", 11); sb_put(s, label, strlen(label)); sb_put(s, "-----\n", 6);
    u32 col = 0;
    for (u32 i = 0; i < n; i += 3) {
        u32 v = (u32)der[i] << 16 | (i + 1 < n ? (u32)der[i + 1] << 8 : 0) | (i + 2 < n ? der[i + 2] : 0);
        char q[4] = {A[v >> 18], A[(v >> 12) & 63], i + 1 < n ? A[(v >> 6) & 63] : '=', i + 2 < n ? A[v & 63] : '='};
        sb_put(s, q, 4);
        col += 4;
        if (col == 64) { sb_put(s, "\n", 1); col = 0; }
    }
    if (col) sb_put(s, "\n", 1);
    sb_put(s, "-----END ", 9); sb_put(s, label, strlen(label)); sb_put(s, "-----\n", 6);
}

/* ================================================================ DER (escrita) */
static void dw_len(sk_buf *s, usize n) {
    u8 t[8];
    int k = 0;
    if (n < 128) { u8 c = (u8)n; sb_put(s, &c, 1); return; }
    while (n) { t[k++] = (u8)n; n >>= 8; }
    u8 c = (u8)(0x80 | k);
    sb_put(s, &c, 1);
    while (k) sb_put(s, &t[--k], 1);
}
static void dw_tlv(sk_buf *s, u8 tag, const void *v, usize n) { sb_put(s, &tag, 1); dw_len(s, n); sb_put(s, v, n); }
/* X9.63 (04||X||Y||K) -> ECPrivateKey SEC1 com a curva e a pública */
static int sc_x963_to_sec1(const u8 *x, u32 n, u8 **out, u32 *on) {
    static const u8 p256[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07}, p384[] = {0x2b, 0x81, 0x04, 0x00, 0x22},
                    p521[] = {0x2b, 0x81, 0x04, 0x00, 0x23};
    u32 B = n == 97 ? 32 : n == 145 ? 48 : n == 199 ? 66 : 0;
    if (!B || x[0] != 4) return -1;
    const u8 *oid = B == 32 ? p256 : B == 48 ? p384 : p521;
    usize oidn = B == 32 ? sizeof p256 : sizeof p384;
    sk_buf in = {0}, c0 = {0}, c1 = {0}, all = {0};
    static const u8 one = 1;
    dw_tlv(&in, 0x02, &one, 1);
    dw_tlv(&in, 0x04, x + 1 + 2 * B, B);
    dw_tlv(&c0, 0x06, oid, oidn);
    dw_tlv(&in, 0xa0, c0.b, c0.n);
    u8 z = 0;
    sb_put(&c1, &z, 1);
    sb_put(&c1, x, 1 + 2 * B);
    sk_buf bits = {0};
    dw_tlv(&bits, 0x03, c1.b, c1.n);
    dw_tlv(&in, 0xa1, bits.b, bits.n);
    dw_tlv(&all, 0x30, in.b, in.n);
    free(in.b); free(c0.b); free(c1.b); free(bits.b);
    if (all.err) { free(all.b); return -1; }
    *out = all.b;
    *on = (u32)all.n;
    return 0;
}

/* ================================================================ hashes, HMAC, PBKDF2 */
enum { PH_SHA1 = 1, PH_SHA224, PH_SHA256, PH_SHA384, PH_SHA512 };
static u32 ph_len(int h) { return h == PH_SHA1 ? 20 : h == PH_SHA224 ? 28 : h == PH_SHA256 ? 32 : h == PH_SHA384 ? 48 : h == PH_SHA512 ? 64 : 0; }
static u32 ph_block(int h) { return h == PH_SHA384 || h == PH_SHA512 ? 128 : 64; }
static void ph_hash(int h, const u8 *d, usize n, u8 *out) {
    if (h == PH_SHA1) { sha1(d, n, out); return; }
    if (h == PH_SHA256 || h == PH_SHA224) {
        sha256_ctx c;
        sha256_init(&c);
        if (h == PH_SHA224) {
            static const u32 iv[8] = {0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939, 0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4};
            memcpy(c.h, iv, sizeof iv);
        }
        sha256_update(&c, d, n);
        u8 t[32];
        sha256_final(&c, t);
        memcpy(out, t, ph_len(h));
        return;
    }
    sha512_ctx c;
    if (h == PH_SHA384) sha384_init(&c); else sha512_init(&c);
    sha512_update(&c, d, n);
    sha512_final(&c, out);
}
static void ph_hmac(int h, const u8 *key, usize kl, const u8 *m, usize ml, u8 *out) {
    u32 bs = ph_block(h), hl = ph_len(h);
    u8 k[128], kh[64];
    memset(k, 0, sizeof k);
    if (kl > bs) { ph_hash(h, key, kl, kh); memcpy(k, kh, hl); } else if (kl) memcpy(k, key, kl);
    u8 *buf = malloc(bs + (ml > hl ? ml : hl));
    if (!buf) { memset(out, 0, hl); return; }
    for (u32 i = 0; i < bs; i++) buf[i] = k[i] ^ 0x36;
    memcpy(buf + bs, m, ml);
    u8 inner[64];
    ph_hash(h, buf, bs + ml, inner);
    for (u32 i = 0; i < bs; i++) buf[i] = k[i] ^ 0x5c;
    memcpy(buf + bs, inner, hl);
    ph_hash(h, buf, bs + hl, out);
    memset(buf, 0, bs + (ml > hl ? ml : hl));
    free(buf);
}
static void ph_pbkdf2(int h, const u8 *pw, usize pl, const u8 *salt, usize sl, u32 iter, u8 *out, usize ol) {
    u32 hl = ph_len(h);
    u8 *s = malloc(sl + 4), u[64], t[64];
    if (!s) return;
    memcpy(s, salt, sl);
    for (u32 blk = 1; ol; blk++) {
        s[sl] = (u8)(blk >> 24); s[sl + 1] = (u8)(blk >> 16); s[sl + 2] = (u8)(blk >> 8); s[sl + 3] = (u8)blk;
        ph_hmac(h, pw, pl, s, sl + 4, u);
        memcpy(t, u, hl);
        for (u32 r = 1; r < iter; r++) { ph_hmac(h, pw, pl, u, hl, u); for (u32 i = 0; i < hl; i++) t[i] ^= u[i]; }
        usize k = ol < hl ? ol : hl;
        memcpy(out, t, k);
        out += k;
        ol -= k;
    }
    free(s);
}
/* KDF do PKCS#12 (RFC 7292, apêndice B.2). pw = BMPString com o 00 00 final (ou NULL/0 = senha ausente) */
static int p12_kdf(int h, const u8 *pw, usize pl, const u8 *salt, usize sl, u32 iter, u8 id, u8 *out, usize n) {
    u32 u = ph_len(h), v = ph_block(h);
    usize slen = sl ? v * ((sl + v - 1) / v) : 0, plen = pl ? v * ((pl + v - 1) / v) : 0, ilen = slen + plen;
    u8 *I = malloc(ilen + 1), *buf = malloc(v + ilen + 1), A[64], B[128];
    if (!I || !buf) { free(I); free(buf); return -1; }
    for (usize i = 0; i < slen; i++) I[i] = salt[i % sl];
    for (usize i = 0; i < plen; i++) I[slen + i] = pw[i % pl];
    while (n) {
        memset(buf, id, v);
        memcpy(buf + v, I, ilen);
        ph_hash(h, buf, v + ilen, A);
        for (u32 r = 1; r < iter; r++) ph_hash(h, A, u, A);
        usize k = n < u ? n : u;
        memcpy(out, A, k);
        out += k;
        n -= k;
        if (!n) break;
        for (u32 i = 0; i < v; i++) B[i] = A[i % u];
        for (usize j = 0; j < ilen; j += v) {               /* I_j = (I_j + B + 1) mod 2^(8v) */
            u32 c = 1;
            for (int i = (int)v - 1; i >= 0; i--) { c += (u32)I[j + (usize)i] + B[i]; I[j + (usize)i] = (u8)c; c >>= 8; }
        }
    }
    memset(I, 0, ilen); free(I); free(buf);
    return 0;
}
/* senha UTF-8 -> BMPString (UTF-16BE) com o terminador 00 00 */
static u8 *p12_bmp(const char *pw, usize *n) {
    usize l = pw ? strlen(pw) : 0;
    u8 *o = malloc(2 * l + 4);
    usize k = 0;
    if (!o) return 0;
    for (usize i = 0; i < l;) {
        u32 c = (u8)pw[i];
        int extra = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : 0;
        c &= extra == 3 ? 0x07 : extra == 2 ? 0x0f : extra == 1 ? 0x1f : 0x7f;
        i++;
        while (extra-- && i < l) c = c << 6 | ((u8)pw[i++] & 0x3f);
        if (c >= 0x10000) { c -= 0x10000; u32 hi = 0xd800 + (c >> 10), lo = 0xdc00 + (c & 0x3ff); o[k++] = (u8)(hi >> 8); o[k++] = (u8)hi; c = lo; }
        o[k++] = (u8)(c >> 8);
        o[k++] = (u8)c;
    }
    o[k++] = 0;
    o[k++] = 0;
    *n = k;
    return o;
}

/* ================================================================ PKCS#12 */
enum { P12_OK = 0, P12_E_PARSE = 1, P12_E_MAC = 2, P12_E_UNSUPPORTED = 3, P12_E_NOMEM = 4 };
typedef struct { u8 *der; u32 n; u8 id[64]; u32 idn; char name[128]; } p12_item_t;
typedef struct { p12_item_t certs[32], keys[8]; int ncerts, nkeys; } p12_out_t;
static void p12_free(p12_out_t *o) {
    for (int i = 0; i < o->ncerts; i++) free(o->certs[i].der);
    for (int i = 0; i < o->nkeys; i++) { if (o->keys[i].der) memset(o->keys[i].der, 0, o->keys[i].n); free(o->keys[i].der); }
    o->ncerts = o->nkeys = 0;
}
static int p12_cert_matches_key(const p12_out_t *o, int c, int k) {
    return o->keys[k].idn && o->certs[c].idn == o->keys[k].idn && !memcmp(o->certs[c].id, o->keys[k].id, o->keys[k].idn);
}

static const u8 OID_DATA[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01};
static const u8 OID_ENCDATA[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x06};
static const u8 OID_KEYBAG[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x0a, 0x01, 0x01};
static const u8 OID_SHROUDED[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x0a, 0x01, 0x02};
static const u8 OID_CERTBAG[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x0a, 0x01, 0x03};
static const u8 OID_SAFEBAG[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x0a, 0x01, 0x06};
static const u8 OID_X509CERT[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x16, 0x01};
static const u8 OID_FNAME[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x14};
static const u8 OID_LKID[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x15};
static const u8 OID_PBES2[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x05, 0x0d};
static const u8 OID_PBKDF2[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x05, 0x0c};
static const u8 OID_PBE_PREFIX[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x01};   /* + 1..6 */
static const u8 OID_HMAC_PREFIX[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02};            /* + 7..11 */
static const u8 OID_AES_PREFIX[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01};        /* + 2, 22, 42 */
static const u8 OID_DES3CBC[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x03, 0x07};
static const u8 OID_SHA1[] = {0x2b, 0x0e, 0x03, 0x02, 0x1a};
static const u8 OID_SHA2_PREFIX[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02};       /* + 1 256, 2 384, 3 512, 4 224 */

static int p12_hash_of_oid(const u8 *v, usize n) {
    if (oid_is(v, n, OID_SHA1, sizeof OID_SHA1)) return PH_SHA1;
    if (n == sizeof OID_SHA2_PREFIX + 1 && !memcmp(v, OID_SHA2_PREFIX, sizeof OID_SHA2_PREFIX)) {
        u8 x = v[n - 1];
        return x == 1 ? PH_SHA256 : x == 2 ? PH_SHA384 : x == 3 ? PH_SHA512 : x == 4 ? PH_SHA224 : 0;
    }
    return 0;
}
typedef struct { const char *pw; u8 *bmp; usize bmpn; } p12_pw_t;

/* decifra com um AlgorithmIdentifier de PBE (conteúdo da SEQUENCE) */
static int p12_decrypt(const u8 *alg, usize an, const u8 *ct, usize cn, const p12_pw_t *pw, u8 **out, usize *on) {
    be_rd r = {alg, alg + an};
    const u8 *ov, *pv;
    usize ovn, pn;
    if (be_expect(&r, 0x06, &ov, &ovn) || be_expect(&r, 0x30, &pv, &pn)) return P12_E_PARSE;
    be_rd p = {pv, pv + pn};
    u8 key[32], iv[16];
    u32 calg, klen, ivlen = 8, opts = 1;   /* kCCOptionPKCS7Padding */
    if (ovn == sizeof OID_PBE_PREFIX + 1 && !memcmp(ov, OID_PBE_PREFIX, sizeof OID_PBE_PREFIX)) {
        const u8 *sv;
        usize sn;
        u32 iter;
        if (be_expect(&p, 0x04, &sv, &sn) || be_get_uint(&p, &iter)) return P12_E_PARSE;
        switch (ov[ovn - 1]) {
        case 1: calg = 4; klen = 16; ivlen = 0; break;     /* RC4-128 */
        case 2: calg = 4; klen = 5; ivlen = 0; break;      /* RC4-40 */
        case 3: calg = 2; klen = 24; break;                /* 3DES (3 chaves) */
        case 4: calg = 2; klen = 16; break;                /* 3DES (2 chaves: k1 k2 k1) */
        case 5: calg = 5; klen = 16; break;                /* RC2-128 */
        case 6: calg = 5; klen = 5; break;                 /* RC2-40 */
        default: return P12_E_UNSUPPORTED;
        }
        if (calg == 4) opts = 0;
        p12_kdf(PH_SHA1, pw->bmp, pw->bmpn, sv, sn, iter, 1, key, klen);
        if (ivlen) p12_kdf(PH_SHA1, pw->bmp, pw->bmpn, sv, sn, iter, 2, iv, ivlen);
        if (calg == 2 && klen == 16) { memcpy(key + 16, key, 8); klen = 24; }
    } else if (oid_is(ov, ovn, OID_PBES2, sizeof OID_PBES2)) {
        const u8 *kdf, *enc, *kov, *kpv, *sv, *eov, *ivv;
        usize kdfn, encn, kon, kpn, sn, eon, ivn;
        u32 iter, want = 0;
        if (be_expect(&p, 0x30, &kdf, &kdfn) || be_expect(&p, 0x30, &enc, &encn)) return P12_E_PARSE;
        be_rd k = {kdf, kdf + kdfn};
        if (be_expect(&k, 0x06, &kov, &kon) || !oid_is(kov, kon, OID_PBKDF2, sizeof OID_PBKDF2) || be_expect(&k, 0x30, &kpv, &kpn))
            return P12_E_UNSUPPORTED;
        be_rd kp = {kpv, kpv + kpn};
        if (be_expect(&kp, 0x04, &sv, &sn) || be_get_uint(&kp, &iter)) return P12_E_PARSE;
        int prf = PH_SHA1;
        while (kp.p < kp.end) {
            u8 t;
            const u8 *xv;
            usize xn;
            be_rd save = kp;
            if (be_get(&kp, &t, &xv, &xn, 0)) return P12_E_PARSE;
            if (t == 0x02) { kp = save; be_get_uint(&kp, &want); continue; }
            if (t == 0x30) {
                be_rd pr = {xv, xv + xn};
                const u8 *pov;
                usize pon;
                if (be_expect(&pr, 0x06, &pov, &pon)) return P12_E_PARSE;
                if (pon != sizeof OID_HMAC_PREFIX + 1 || memcmp(pov, OID_HMAC_PREFIX, sizeof OID_HMAC_PREFIX)) return P12_E_UNSUPPORTED;
                u8 x = pov[pon - 1];
                prf = x == 7 ? PH_SHA1 : x == 8 ? PH_SHA224 : x == 9 ? PH_SHA256 : x == 10 ? PH_SHA384 : x == 11 ? PH_SHA512 : 0;
                if (!prf) return P12_E_UNSUPPORTED;
            }
        }
        be_rd e = {enc, enc + encn};
        if (be_expect(&e, 0x06, &eov, &eon) || be_expect(&e, 0x04, &ivv, &ivn)) return P12_E_PARSE;
        if (eon == sizeof OID_AES_PREFIX + 1 && !memcmp(eov, OID_AES_PREFIX, sizeof OID_AES_PREFIX)) {
            u8 x = eov[eon - 1];
            calg = 0;
            klen = x == 2 ? 16 : x == 22 ? 24 : x == 42 ? 32 : 0;
            if (!klen) return P12_E_UNSUPPORTED;
            ivlen = 16;
        } else if (oid_is(eov, eon, OID_DES3CBC, sizeof OID_DES3CBC)) { calg = 2; klen = 24; ivlen = 8; }
        else return P12_E_UNSUPPORTED;
        if (want && want != klen) return P12_E_PARSE;
        if (ivn != ivlen) return P12_E_PARSE;
        memcpy(iv, ivv, ivlen);
        const char *s = pw->pw ? pw->pw : "";
        ph_pbkdf2(prf, (const u8 *)s, strlen(s), sv, sn, iter, key, klen);
    } else return P12_E_UNSUPPORTED;
    u8 *o = malloc(cn + 16);
    size_t moved = 0;
    if (!o) return P12_E_NOMEM;
    int rc = CCCrypt(1, calg, opts, key, klen, ivlen ? iv : 0, ct, cn, o, cn + 16, &moved);
    memset(key, 0, sizeof key);
    if (rc) { free(o); return P12_E_MAC; }                 /* padding ruim = senha errada */
    *out = o;
    *on = moved;
    return 0;
}
static void p12_attrs(const u8 *v, usize n, p12_item_t *it) {
    be_rd r = {v, v + n};
    while (r.p < r.end) {
        const u8 *sv, *ov, *setv, *xv;
        usize sn, on, setn, xn;
        u8 t;
        if (be_expect(&r, 0x30, &sv, &sn)) return;
        be_rd s = {sv, sv + sn};
        if (be_expect(&s, 0x06, &ov, &on) || be_expect(&s, 0x31, &setv, &setn)) return;
        be_rd set = {setv, setv + setn};
        if (be_get(&set, &t, &xv, &xn, 0)) continue;
        if (oid_is(ov, on, OID_FNAME, sizeof OID_FNAME)) sc_str_utf8(t, xv, xn, it->name, sizeof it->name);
        else if (oid_is(ov, on, OID_LKID, sizeof OID_LKID) && t == 0x04 && xn <= sizeof it->id) { memcpy(it->id, xv, xn); it->idn = (u32)xn; }
    }
}
static int p12_safe_contents(const u8 *b, usize n, const p12_pw_t *pw, p12_out_t *o, int depth);
static int p12_bag(const u8 *v, usize n, const p12_pw_t *pw, p12_out_t *o, int depth) {
    be_rd r = {v, v + n};
    const u8 *ov, *bv;
    usize on, bn;
    if (be_expect(&r, 0x06, &ov, &on) || be_expect(&r, 0xa0, &bv, &bn)) return P12_E_PARSE;
    p12_item_t it;
    memset(&it, 0, sizeof it);
    if (r.p < r.end) {
        const u8 *av;
        usize an;
        if (!be_expect(&r, 0x31, &av, &an)) p12_attrs(av, an, &it);
    }
    be_rd b = {bv, bv + bn};
    if (oid_is(ov, on, OID_CERTBAG, sizeof OID_CERTBAG)) {
        const u8 *cv, *tv, *wv;
        usize cn, tn, wn;
        if (be_expect(&b, 0x30, &cv, &cn)) return P12_E_PARSE;
        be_rd c = {cv, cv + cn};
        if (be_expect(&c, 0x06, &tv, &tn) || be_expect(&c, 0xa0, &wv, &wn)) return P12_E_PARSE;
        if (!oid_is(tv, tn, OID_X509CERT, sizeof OID_X509CERT)) return 0;    /* SDSI: ignorado */
        be_rd w = {wv, wv + wn};
        u8 *der;
        usize dn;
        if (be_get_octets(&w, 0x04, &der, &dn)) return P12_E_PARSE;
        if (o->ncerts >= 32) { free(der); return 0; }
        it.der = der;
        it.n = (u32)dn;
        o->certs[o->ncerts++] = it;
        return 0;
    }
    if (oid_is(ov, on, OID_KEYBAG, sizeof OID_KEYBAG) || oid_is(ov, on, OID_SHROUDED, sizeof OID_SHROUDED)) {
        u8 tg;
        const u8 *kv;
        usize kn;
        if (be_get(&b, &tg, &kv, &kn, 0) || tg != 0x30) return P12_E_PARSE;
        u8 *der;
        usize dn;
        if (oid_is(ov, on, OID_KEYBAG, sizeof OID_KEYBAG)) {
            sk_buf t = {0};
            dw_tlv(&t, 0x30, kv, kn);
            if (t.err) { free(t.b); return P12_E_NOMEM; }
            der = t.b;
            dn = t.n;
        } else {
            be_rd e = {kv, kv + kn};
            const u8 *av;
            usize an;
            u8 *ct;
            usize cn;
            if (be_expect(&e, 0x30, &av, &an) || be_get_octets(&e, 0x04, &ct, &cn)) return P12_E_PARSE;
            int rc = p12_decrypt(av, an, ct, cn, pw, &der, &dn);
            free(ct);
            if (rc) return rc;
        }
        if (o->nkeys >= 8) { free(der); return 0; }
        it.der = der;
        it.n = (u32)dn;
        o->keys[o->nkeys++] = it;
        return 0;
    }
    if (oid_is(ov, on, OID_SAFEBAG, sizeof OID_SAFEBAG) && depth < 4) {
        const u8 *sv;
        usize sn;
        if (be_expect(&b, 0x30, &sv, &sn)) return P12_E_PARSE;
        return p12_safe_contents(bv, bn, pw, o, depth + 1);
    }
    return 0;                                                /* CRL e segredos: ignorados */
}
static int p12_safe_contents(const u8 *b, usize n, const p12_pw_t *pw, p12_out_t *o, int depth) {
    be_rd r = {b, b + n};
    const u8 *v;
    usize vn;
    if (be_expect(&r, 0x30, &v, &vn)) return P12_E_PARSE;
    be_rd s = {v, v + vn};
    while (s.p < s.end) {
        const u8 *bv;
        usize bn;
        if (be_expect(&s, 0x30, &bv, &bn)) return P12_E_PARSE;
        int rc = p12_bag(bv, bn, pw, o, depth);
        if (rc) return rc;
    }
    return 0;
}
static int sc_is_pkcs12(const u8 *b, u32 n) {
    be_rd r = {b, b + n};
    const u8 *v;
    usize vn;
    u32 ver;
    if (be_expect(&r, 0x30, &v, &vn)) return 0;
    be_rd s = {v, v + vn};
    return !be_get_uint(&s, &ver) && ver == 3;
}
/* conteúdo de um ContentInfo do tipo data: a OCTET STRING dentro de [0] */
static int p12_data_content(const u8 *v, usize n, const u8 *want_oid, usize won, const u8 **rest, usize *rn) {
    be_rd r = {v, v + n};
    const u8 *ov, *cv;
    usize on, cn;
    if (be_expect(&r, 0x06, &ov, &on) || !oid_is(ov, on, want_oid, won) || be_expect(&r, 0xa0, &cv, &cn)) return -1;
    *rest = cv;
    *rn = cn;
    return 0;
}
static int p12_parse(const u8 *b, u32 n, const char *pwtxt, p12_out_t *o) {
    memset(o, 0, sizeof *o);
    be_rd r = {b, b + n};
    const u8 *v, *civ, *macv = 0;
    usize vn, cin, macn = 0;
    u32 ver;
    if (be_expect(&r, 0x30, &v, &vn)) return P12_E_PARSE;
    be_rd pfx = {v, v + vn};
    if (be_get_uint(&pfx, &ver) || ver != 3 || be_expect(&pfx, 0x30, &civ, &cin)) return P12_E_PARSE;
    if (pfx.p < pfx.end && be_expect(&pfx, 0x30, &macv, &macn)) return P12_E_PARSE;
    const u8 *wrap;
    usize wn;
    if (p12_data_content(civ, cin, OID_DATA, sizeof OID_DATA, &wrap, &wn)) return P12_E_UNSUPPORTED;   /* signedData */
    be_rd w = {wrap, wrap + wn};
    u8 *auth;
    usize an;
    if (be_get_octets(&w, 0x04, &auth, &an)) return P12_E_PARSE;
    p12_pw_t pw = {pwtxt, 0, 0};
    pw.bmp = p12_bmp(pwtxt ? pwtxt : "", &pw.bmpn);
    int rc = 0;
    if (macv) {                                              /* MacData: confere a senha antes de decifrar */
        be_rd m = {macv, macv + macn};
        const u8 *dv, *av, *ov, *dg, *sv;
        usize dn, avn, on, dgn, sn;
        u32 iter = 1;
        if (be_expect(&m, 0x30, &dv, &dn) || be_expect(&m, 0x04, &sv, &sn)) { rc = P12_E_PARSE; goto out; }
        if (m.p < m.end) be_get_uint(&m, &iter);
        be_rd d = {dv, dv + dn};
        if (be_expect(&d, 0x30, &av, &avn) || be_expect(&d, 0x04, &dg, &dgn)) { rc = P12_E_PARSE; goto out; }
        be_rd a = {av, av + avn};
        if (be_expect(&a, 0x06, &ov, &on)) { rc = P12_E_PARSE; goto out; }
        int h = p12_hash_of_oid(ov, on);
        if (!h) { rc = P12_E_UNSUPPORTED; goto out; }
        u32 hl = ph_len(h);
        u8 key[64], mac[64];
        int good = 0;
        for (int attempt = 0; attempt < 2 && !good; attempt++) {
            /* 1ª: BMPString da senha (com "" = 00 00); 2ª, sem senha: P vazio, como o OpenSSL com senha NULL */
            if (attempt == 1 && pwtxt && pwtxt[0]) break;
            p12_kdf(h, attempt ? 0 : pw.bmp, attempt ? 0 : pw.bmpn, sv, sn, iter, 3, key, hl);
            ph_hmac(h, key, hl, auth, an, mac);
            if (dgn == hl && !memcmp(mac, dg, hl)) good = 1;
            if (good && attempt) { free(pw.bmp); pw.bmp = 0; pw.bmpn = 0; }
        }
        if (!good) { rc = P12_E_MAC; goto out; }
    }
    {
        be_rd as = {auth, auth + an};
        const u8 *sv;
        usize sn;
        if (be_expect(&as, 0x30, &sv, &sn)) { rc = P12_E_PARSE; goto out; }
        be_rd s = {sv, sv + sn};
        while (s.p < s.end && !rc) {
            const u8 *ci, *ov, *cv;
            usize cn, on, cvn;
            if (be_expect(&s, 0x30, &ci, &cn)) { rc = P12_E_PARSE; break; }
            be_rd c = {ci, ci + cn};
            if (be_expect(&c, 0x06, &ov, &on) || be_expect(&c, 0xa0, &cv, &cvn)) { rc = P12_E_PARSE; break; }
            be_rd body = {cv, cv + cvn};
            if (oid_is(ov, on, OID_DATA, sizeof OID_DATA)) {
                u8 *sc;
                usize scn;
                if (be_get_octets(&body, 0x04, &sc, &scn)) { rc = P12_E_PARSE; break; }
                rc = p12_safe_contents(sc, scn, &pw, o, 0);
                free(sc);
            } else if (oid_is(ov, on, OID_ENCDATA, sizeof OID_ENCDATA)) {
                const u8 *ed, *eci, *ctv, *av;
                usize edn, ecin, ctn, avn;
                u32 ev;
                if (be_expect(&body, 0x30, &ed, &edn)) { rc = P12_E_PARSE; break; }
                be_rd e = {ed, ed + edn};
                if (be_get_uint(&e, &ev) || be_expect(&e, 0x30, &eci, &ecin)) { rc = P12_E_PARSE; break; }
                be_rd x = {eci, eci + ecin};
                if (be_expect(&x, 0x06, &ctv, &ctn) || be_expect(&x, 0x30, &av, &avn)) { rc = P12_E_PARSE; break; }
                u8 *ct;
                usize ctl;
                if (be_get_octets(&x, 0x80, &ct, &ctl)) { rc = P12_E_PARSE; break; }   /* [0] IMPLICIT (0x80 ou 0xa0) */
                u8 *plain;
                usize pn;
                rc = p12_decrypt(av, avn, ct, ctl, &pw, &plain, &pn);
                free(ct);
                if (!rc) { rc = p12_safe_contents(plain, pn, &pw, o, 0); memset(plain, 0, pn); free(plain); }
            }
        }
    }
out:
    if (pw.bmp) { memset(pw.bmp, 0, pw.bmpn); free(pw.bmp); }
    free(auth);
    if (rc) p12_free(o);
    return rc;
}
