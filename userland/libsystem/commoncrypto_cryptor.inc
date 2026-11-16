/* mOS libSystem — CCCryptor genérico do CommonCrypto (incluído no fim de commoncrypto2.c, que tem o AES).
 *
 * Algoritmos: AES (16/24/32), DES (8), 3DES (24, EDE), RC2 (1–128, bits efetivos = 8·tamanho), RC4 (1–512),
 * Blowfish (8–56). CAST5 devolve kCCUnimplemented. Modos: ECB, CBC (com ou sem PKCS#7), CFB, CFB8, CTR, OFB,
 * RC4, XTS (AES; a segunda chave vem no parâmetro tweak; CCCryptorEncryptDataBlock/DecryptDataBlock por unidade
 * de dados, com roubo de texto cifrado) e GCM (AES; IV de qualquer tamanho, tag de 4 a 16 bytes) pelas
 * APIs CCCryptorGCM* e CCCryptorGCMOneshot*. Também: SHA-224, MD2, MD4 e CCCalibratePBKDF.
 * Teste de host: tools/cctest.c + tools/cctest.py (contra o OpenSSL). */
#include "cc_ciphers.inc"

enum { kCCSuccess = 0, kCCParamError = -4300, kCCBufferTooSmall = -4301, kCCMemoryFailure = -4302,
       kCCAlignmentError = -4303, kCCDecodeError = -4304, kCCUnimplemented = -4305, kCCInvalidKey = -4310 };
enum { kCCEncrypt = 0, kCCDecrypt = 1 };
enum { kCCAlgorithmAES = 0, kCCAlgorithmDES = 1, kCCAlgorithm3DES = 2, kCCAlgorithmCAST = 3, kCCAlgorithmRC4 = 4,
       kCCAlgorithmRC2 = 5, kCCAlgorithmBlowfish = 6 };
enum { kCCOptionPKCS7Padding = 1, kCCOptionECBMode = 2 };
enum { kCCModeECB = 1, kCCModeCBC = 2, kCCModeCFB = 3, kCCModeCTR = 4, kCCModeF8 = 5, kCCModeLRW = 6, kCCModeOFB = 7,
       kCCModeXTS = 8, kCCModeRC4 = 9, kCCModeCFB8 = 10, kCCModeGCM = 11, kCCModeCCM = 12 };

typedef struct {
    int op, mode, pad, alg, bs;
    union { aes_key_t aes; des_ks des; des3_ks des3; rc2_ks rc2; bf_ks bf; rc4_st rc4; } k;
    aes_key_t k2;                 /* XTS: chave do tweak */
    u8_ iv[16];                   /* CBC: bloco anterior; CTR: contador; CFB/OFB: registrador */
    u8_ buf[16];
    int nbuf;
    u8_ ks[16]; int nks;          /* fluxo de chave não usado (CTR, CFB, OFB) */
    /* GCM */
    u8_ H[16], J0[16], ctr[16], X[16], ivbuf[16], gks[16];
    u64_ alen, clen;
    int giv_n, gstate;            /* 0 = juntando IV, 1 = AAD, 2 = dados */
    u8_ *ivacc; size_t ivacc_n;
    u8_ abuf[16]; int an;
    int gks_n;
} cryptor_t;

static void blk_enc(cryptor_t *c, const u8_ *in, u8_ *out) {
    switch (c->alg) {
    case kCCAlgorithmAES: aes_enc(&c->k.aes, in, out); break;
    case kCCAlgorithmDES: des_block(&c->k.des, 0, in, out); break;
    case kCCAlgorithm3DES: des3_block(&c->k.des3, 0, in, out); break;
    case kCCAlgorithmRC2: rc2_block(&c->k.rc2, 0, in, out); break;
    case kCCAlgorithmBlowfish: bf_block(&c->k.bf, 0, in, out); break;
    }
}
static void blk_dec(cryptor_t *c, const u8_ *in, u8_ *out) {
    switch (c->alg) {
    case kCCAlgorithmAES: aes_dec(&c->k.aes, in, out); break;
    case kCCAlgorithmDES: des_block(&c->k.des, 1, in, out); break;
    case kCCAlgorithm3DES: des3_block(&c->k.des3, 1, in, out); break;
    case kCCAlgorithmRC2: rc2_block(&c->k.rc2, 1, in, out); break;
    case kCCAlgorithmBlowfish: bf_block(&c->k.bf, 1, in, out); break;
    }
}
static int setkey(cryptor_t *c, const u8_ *key, size_t klen) {
    switch (c->alg) {
    case kCCAlgorithmAES: c->bs = 16; return aes_setkey(&c->k.aes, key, klen) ? kCCInvalidKey : 0;
    case kCCAlgorithmDES: c->bs = 8; if (klen != 8) return kCCInvalidKey; des_setkey(&c->k.des, key); return 0;
    case kCCAlgorithm3DES:
        c->bs = 8;
        if (klen != 24) return kCCInvalidKey;
        des_setkey(&c->k.des3.k1, key); des_setkey(&c->k.des3.k2, key + 8); des_setkey(&c->k.des3.k3, key + 16);
        return 0;
    case kCCAlgorithmRC2: c->bs = 8; if (!klen || klen > 128) return kCCInvalidKey; rc2_setkey(&c->k.rc2, key, klen, (int)klen * 8); return 0;
    case kCCAlgorithmBlowfish: c->bs = 8; if (klen < 8 || klen > 56) return kCCInvalidKey; return bf_setkey(&c->k.bf, key, klen) ? kCCMemoryFailure : 0;
    case kCCAlgorithmRC4: c->bs = 1; if (!klen || klen > 512) return kCCInvalidKey; rc4_setkey(&c->k.rc4, key, klen); return 0;
    case kCCAlgorithmCAST: return kCCUnimplemented;
    default: return kCCParamError;
    }
}
static int stream_mode(int m) { return m == kCCModeCTR || m == kCCModeCFB || m == kCCModeCFB8 || m == kCCModeOFB || m == kCCModeRC4; }

static void ctr_inc_n(u8_ *c, int n) { for (int i = n - 1; i >= 0; i--) if (++c[i]) break; }
static void do_block(cryptor_t *c, const u8_ *in, u8_ *out) {
    int bs = c->bs;
    u8_ t[16];
    if (c->mode == kCCModeECB) { if (c->op == kCCEncrypt) blk_enc(c, in, out); else blk_dec(c, in, out); return; }
    if (c->op == kCCEncrypt) {
        for (int i = 0; i < bs; i++) t[i] = in[i] ^ c->iv[i];
        blk_enc(c, t, out);
        memcpy(c->iv, out, (size_t)bs);
    } else {
        u8_ prev[16];
        memcpy(prev, in, (size_t)bs);
        blk_dec(c, in, t);
        for (int i = 0; i < bs; i++) out[i] = t[i] ^ c->iv[i];
        memcpy(c->iv, prev, (size_t)bs);
    }
}
static u8_ stream_byte(cryptor_t *c, u8_ in) {
    int bs = c->bs;
    switch (c->mode) {
    case kCCModeCTR:
        if (!c->nks) { blk_enc(c, c->iv, c->ks); ctr_inc_n(c->iv, bs); c->nks = bs; }
        return in ^ c->ks[bs - c->nks--];
    case kCCModeOFB:
        if (!c->nks) { blk_enc(c, c->iv, c->iv); memcpy(c->ks, c->iv, (size_t)bs); c->nks = bs; }
        return in ^ c->ks[bs - c->nks--];
    case kCCModeCFB: {
        if (!c->nks) { blk_enc(c, c->iv, c->ks); c->nks = bs; }
        int pos = bs - c->nks--;
        u8_ o = in ^ c->ks[pos];
        c->iv[pos] = c->op == kCCEncrypt ? o : in;       /* o registrador recebe o texto cifrado */
        return o;
    }
    case kCCModeCFB8: {
        u8_ k[16];
        blk_enc(c, c->iv, k);
        u8_ o = in ^ k[0];
        memmove(c->iv, c->iv + 1, (size_t)bs - 1);
        c->iv[bs - 1] = c->op == kCCEncrypt ? o : in;
        return o;
    }
    default: {                                            /* RC4 */
        u8_ o;
        rc4_xor(&c->k.rc4, &in, &o, 1);
        return o;
    }
    }
}

/* ---- GHASH / GCM (SP 800-38D) */
static void gf_mul(u8_ *x, const u8_ *h) {
    u8_ z[16] = {0}, v[16];
    memcpy(v, h, 16);
    for (int i = 0; i < 128; i++) {
        if (x[i / 8] & (0x80 >> (i % 8))) for (int j = 0; j < 16; j++) z[j] ^= v[j];
        int lsb = v[15] & 1;
        for (int j = 15; j > 0; j--) v[j] = (u8_)(v[j] >> 1 | v[j - 1] << 7);
        v[0] >>= 1;
        if (lsb) v[0] ^= 0xe1;
    }
    memcpy(x, z, 16);
}
static void ghash_blk(u8_ *X, const u8_ *H, const u8_ *b, size_t n) {
    for (size_t i = 0; i < n; i++) X[i] ^= b[i];
    gf_mul(X, H);
}
static void put_be64(u8_ *p, u64_ v) { for (int i = 7; i >= 0; i--) { p[i] = (u8_)v; v >>= 8; } }
static int gcm_begin_data(cryptor_t *c) {
    if (c->gstate == 0) {                                 /* fecha o IV: J0 */
        if (c->ivacc_n == 12) { memcpy(c->J0, c->ivacc, 12); c->J0[12] = c->J0[13] = c->J0[14] = 0; c->J0[15] = 1; }
        else if (c->ivacc_n) {
            u8_ X[16] = {0}, L[16] = {0};
            for (size_t i = 0; i < c->ivacc_n; i += 16) ghash_blk(X, c->H, c->ivacc + i, c->ivacc_n - i < 16 ? c->ivacc_n - i : 16);
            put_be64(L + 8, (u64_)c->ivacc_n * 8);
            ghash_blk(X, c->H, L, 16);
            memcpy(c->J0, X, 16);
        } else return kCCParamError;
        memcpy(c->ctr, c->J0, 16);
        ctr_inc_n(c->ctr + 12, 4);
        c->gstate = 1;
    }
    return kCCSuccess;
}
static void gcm_flush_aad(cryptor_t *c) {
    if (c->an) { ghash_blk(c->X, c->H, c->abuf, (size_t)c->an); c->an = 0; }
}
static int gcm_aad(cryptor_t *c, const u8_ *a, size_t n) {
    int r = gcm_begin_data(c);
    if (r) return r;
    if (c->gstate != 1) return kCCParamError;
    for (size_t i = 0; i < n; i++) {
        c->abuf[c->an++] = a[i];
        if (c->an == 16) gcm_flush_aad(c);
    }
    c->alen += n;
    return kCCSuccess;
}
static int gcm_crypt(cryptor_t *c, const u8_ *in, u8_ *out, size_t n) {
    int r = gcm_begin_data(c);
    if (r) return r;
    if (c->gstate == 1) { gcm_flush_aad(c); c->gstate = 2; }
    for (size_t i = 0; i < n; i++) {
        if (!c->gks_n) { aes_enc(&c->k.aes, c->ctr, c->gks); ctr_inc_n(c->ctr + 12, 4); c->gks_n = 16; }
        u8_ ci = c->op == kCCEncrypt ? (u8_)(in[i] ^ c->gks[16 - c->gks_n]) : in[i];
        out[i] = in[i] ^ c->gks[16 - c->gks_n--];
        c->abuf[c->an++] = ci;
        if (c->an == 16) gcm_flush_aad(c);
    }
    c->clen += n;
    return kCCSuccess;
}
static int gcm_tag(cryptor_t *c, u8_ *tag, size_t tl) {
    int r = gcm_begin_data(c);
    if (r) return r;
    gcm_flush_aad(c);
    u8_ L[16], E[16];
    put_be64(L, c->alen * 8);
    put_be64(L + 8, c->clen * 8);
    ghash_blk(c->X, c->H, L, 16);
    aes_enc(&c->k.aes, c->J0, E);
    for (size_t i = 0; i < tl; i++) tag[i] = c->X[i] ^ E[i];
    return kCCSuccess;
}

/* ---- XTS */
static void xts_mul_alpha(u8_ *t) {
    int carry = t[15] >> 7;
    for (int i = 15; i > 0; i--) t[i] = (u8_)(t[i] << 1 | t[i - 1] >> 7);
    t[0] = (u8_)(t[0] << 1);
    if (carry) t[0] ^= 0x87;
}
static void xts_one(cryptor_t *c, int enc, const u8_ *T, const u8_ *in, u8_ *out) {
    u8_ x[16];
    for (int i = 0; i < 16; i++) x[i] = in[i] ^ T[i];
    if (enc) aes_enc(&c->k.aes, x, x); else aes_dec(&c->k.aes, x, x);
    for (int i = 0; i < 16; i++) out[i] = x[i] ^ T[i];
}
static int xts_unit(cryptor_t *c, int enc, const u8_ *tweak, const u8_ *in, size_t n, u8_ *out) {
    if (n < 16) return kCCAlignmentError;
    u8_ T[16], Tn[16];
    aes_enc(&c->k2, tweak, T);
    size_t full = n / 16, rem = n % 16;
    size_t last = rem ? full - 1 : full;
    for (size_t b = 0; b < last; b++) { xts_one(c, enc, T, in + 16 * b, out + 16 * b); xts_mul_alpha(T); }
    if (rem) {
        size_t p = 16 * last;
        u8_ cc[16], pp[16];
        memcpy(Tn, T, 16);
        xts_mul_alpha(Tn);
        if (enc) {
            xts_one(c, 1, T, in + p, cc);                  /* C_{m-1}' */
            memcpy(pp, in + p + 16, rem);
            memcpy(pp + rem, cc + rem, 16 - rem);
            memcpy(out + p + 16, cc, rem);
            xts_one(c, 1, Tn, pp, out + p);
        } else {
            xts_one(c, 0, Tn, in + p, pp);
            memcpy(cc, in + p + 16, rem);
            memcpy(cc + rem, pp + rem, 16 - rem);
            memcpy(out + p + 16, pp, rem);
            xts_one(c, 0, T, cc, out + p);
        }
    }
    return kCCSuccess;
}

/* ---- criação */
int CCCryptorCreateWithMode(u32_ op, u32_ mode, u32_ alg, u32_ padding, const void *iv, const void *key, size_t klen,
                            const void *tweak, size_t tlen, int rounds, u32_ options, void **ref) {
    (void)rounds; (void)options;
    if (!ref) return kCCParamError;
    *ref = 0;
    if (op > 1 || !key) return kCCParamError;
    if (mode == kCCModeF8 || mode == kCCModeLRW || mode == kCCModeCCM) return kCCUnimplemented;
    if (mode < kCCModeECB || mode > kCCModeGCM) return kCCParamError;
    if ((alg == kCCAlgorithmRC4) != (mode == kCCModeRC4)) return kCCParamError;
    if ((mode == kCCModeXTS || mode == kCCModeGCM) && alg != kCCAlgorithmAES) return kCCParamError;
    cryptor_t *c = calloc(1, sizeof *c);
    if (!c) return kCCMemoryFailure;
    c->op = (int)op; c->mode = (int)mode; c->alg = (int)alg; c->pad = padding == 1 && (mode == kCCModeECB || mode == kCCModeCBC);
    int r = setkey(c, key, klen);
    if (!r && mode == kCCModeXTS) r = !tweak || tlen != klen || aes_setkey(&c->k2, tweak, tlen) ? kCCParamError : 0;
    if (r) { memset(c, 0, sizeof *c); free(c); return r; }
    if (iv && mode != kCCModeGCM && mode != kCCModeRC4) memcpy(c->iv, iv, (size_t)c->bs);
    if (mode == kCCModeGCM) { u8_ z[16] = {0}; aes_enc(&c->k.aes, z, c->H); }
    *ref = c;
    return kCCSuccess;
}
int CCCryptorCreate(u32_ op, u32_ alg, u32_ options, const void *key, size_t klen, const void *iv, void **ref) {
    u32_ mode = alg == kCCAlgorithmRC4 ? kCCModeRC4 : (options & kCCOptionECBMode) ? kCCModeECB : kCCModeCBC;
    return CCCryptorCreateWithMode(op, mode, alg, (options & kCCOptionPKCS7Padding) ? 1 : 0, iv, key, klen, 0, 0, 0, 0, ref);
}
int CCCryptorCreateFromData(u32_ op, u32_ alg, u32_ options, const void *key, size_t klen, const void *iv,
                            const void *data, size_t dlen, void **ref, size_t *used) {
    (void)data; (void)dlen;
    if (used) *used = sizeof(cryptor_t);
    return CCCryptorCreate(op, alg, options, key, klen, iv, ref);
}
int CCCryptorRelease(void *ref) {
    cryptor_t *c = ref;
    if (c) { free(c->ivacc); memset(c, 0, sizeof *c); free(c); }
    return kCCSuccess;
}
int CCCryptorReset(void *ref, const void *iv) {
    cryptor_t *c = ref;
    if (!c) return kCCParamError;
    if (c->mode == kCCModeGCM) {
        free(c->ivacc); c->ivacc = 0; c->ivacc_n = 0;
        memset(c->X, 0, 16); c->alen = c->clen = 0; c->gstate = 0; c->an = 0; c->gks_n = 0;
        return kCCSuccess;
    }
    if (iv) memcpy(c->iv, iv, (size_t)c->bs); else memset(c->iv, 0, 16);
    c->nbuf = 0; c->nks = 0;
    return kCCSuccess;
}
size_t CCCryptorGetOutputLength(void *ref, size_t inlen, int final) {
    cryptor_t *c = ref;
    if (!c) return 0;
    if (stream_mode(c->mode) || c->mode == kCCModeGCM || c->mode == kCCModeXTS) return inlen;
    size_t bs = (size_t)c->bs, t = inlen + (size_t)c->nbuf;
    if (final && c->pad && c->op == kCCEncrypt) return (t / bs + 1) * bs;
    return final ? (t + bs - 1) / bs * bs : t / bs * bs;
}
int CCCryptorUpdate(void *ref, const void *in, size_t inlen, void *out, size_t avail, size_t *moved) {
    cryptor_t *c = ref;
    const u8_ *p = in;
    u8_ *o = out;
    size_t w = 0;
    if (moved) *moved = 0;
    if (!c || (inlen && (!in || !out))) return kCCParamError;
    if (c->mode == kCCModeGCM) {
        if (avail < inlen) return kCCBufferTooSmall;
        int r = gcm_crypt(c, p, o, inlen);
        if (!r && moved) *moved = inlen;
        return r;
    }
    if (c->mode == kCCModeXTS) return kCCParamError;   /* XTS: CCCryptorEncryptDataBlock */
    if (stream_mode(c->mode)) {
        if (avail < inlen) return kCCBufferTooSmall;
        for (size_t i = 0; i < inlen; i++) o[i] = stream_byte(c, p[i]);
        if (moved) *moved = inlen;
        return kCCSuccess;
    }
    int bs = c->bs;
    /* na decifragem com padding o último bloco completo fica guardado para o Final tirar o PKCS#7 */
    int hold = c->op == kCCDecrypt && c->pad;
    size_t need = CCCryptorGetOutputLength(c, inlen, 0);
    if (hold && need >= (size_t)bs && (inlen + (size_t)c->nbuf) % (size_t)bs == 0) need -= (size_t)bs;
    if (avail < need) return kCCBufferTooSmall;
    for (size_t i = 0; i < inlen; i++) {
        if (c->nbuf == bs) {
            do_block(c, c->buf, o + w);
            w += (size_t)bs;
            c->nbuf = 0;
        }
        c->buf[c->nbuf++] = p[i];
    }
    if (c->nbuf == bs && !hold) {
        do_block(c, c->buf, o + w);
        w += (size_t)bs;
        c->nbuf = 0;
    }
    if (moved) *moved = w;
    return kCCSuccess;
}
int CCCryptorFinal(void *ref, void *out, size_t avail, size_t *moved) {
    cryptor_t *c = ref;
    u8_ *o = out;
    if (moved) *moved = 0;
    if (!c) return kCCParamError;
    if (stream_mode(c->mode) || c->mode == kCCModeGCM || c->mode == kCCModeXTS) return kCCSuccess;
    int bs = c->bs;
    if (c->op == kCCEncrypt) {
        if (!c->pad) return c->nbuf ? kCCAlignmentError : kCCSuccess;
        if (avail < (size_t)bs) return kCCBufferTooSmall;
        u8_ v = (u8_)(bs - c->nbuf);
        while (c->nbuf < bs) c->buf[c->nbuf++] = v;
        do_block(c, c->buf, o);
        c->nbuf = 0;
        if (moved) *moved = (size_t)bs;
        return kCCSuccess;
    }
    if (!c->pad) return c->nbuf ? kCCAlignmentError : kCCSuccess;
    if (c->nbuf != bs) return c->nbuf ? kCCAlignmentError : kCCDecodeError;
    u8_ t[16];
    do_block(c, c->buf, t);
    c->nbuf = 0;
    u8_ v = t[bs - 1];
    if (!v || v > bs) return kCCDecodeError;
    for (int i = bs - v; i < bs; i++) if (t[i] != v) return kCCDecodeError;
    size_t n = (size_t)(bs - v);
    if (avail < n) return kCCBufferTooSmall;
    memcpy(o, t, n);
    if (moved) *moved = n;
    return kCCSuccess;
}
int CCCrypt(u32_ op, u32_ alg, u32_ options, const void *key, size_t klen, const void *iv, const void *in, size_t inlen,
            void *out, size_t avail, size_t *moved) {
    void *ref = 0;
    size_t a = 0, b = 0;
    if (moved) *moved = 0;
    int r = CCCryptorCreate(op, alg, options, key, klen, iv, &ref);
    if (r) return r;
    size_t need = CCCryptorGetOutputLength(ref, inlen, 1);
    if (avail < need && op != kCCDecrypt) { CCCryptorRelease(ref); if (moved) *moved = need; return kCCBufferTooSmall; }
    r = CCCryptorUpdate(ref, in, inlen, out, avail, &a);
    if (!r) r = CCCryptorFinal(ref, (u8_ *)out + a, avail - a, &b);
    CCCryptorRelease(ref);
    if (moved) *moved = r ? 0 : a + b;
    return r;
}
int CCCryptorEncryptDataBlock(void *ref, const void *iv, const void *in, size_t n, void *out) {
    cryptor_t *c = ref;
    if (!c || !in || !out) return kCCParamError;
    if (c->mode == kCCModeXTS) { if (!iv) return kCCParamError; return xts_unit(c, 1, iv, in, n, out); }
    if (iv) CCCryptorReset(c, iv);
    size_t m = 0;
    return CCCryptorUpdate(c, in, n, out, n, &m);
}
int CCCryptorDecryptDataBlock(void *ref, const void *iv, const void *in, size_t n, void *out) {
    cryptor_t *c = ref;
    if (!c || !in || !out) return kCCParamError;
    if (c->mode == kCCModeXTS) { if (!iv) return kCCParamError; return xts_unit(c, 0, iv, in, n, out); }
    if (iv) CCCryptorReset(c, iv);
    size_t m = 0;
    return CCCryptorUpdate(c, in, n, out, n, &m);
}

/* ---- GCM: APIs próprias */
int CCCryptorGCMAddIV(void *ref, const void *iv, size_t n) {
    cryptor_t *c = ref;
    if (!c || c->mode != kCCModeGCM || c->gstate != 0 || (n && !iv)) return kCCParamError;
    u8_ *nb = realloc(c->ivacc, c->ivacc_n + n + 1);
    if (!nb) return kCCMemoryFailure;
    c->ivacc = nb;
    memcpy(c->ivacc + c->ivacc_n, iv, n);
    c->ivacc_n += n;
    return kCCSuccess;
}
int CCCryptorGCMSetIV(void *ref, const void *iv, size_t n) { return CCCryptorGCMAddIV(ref, iv, n); }
int CCCryptorGCMAddAAD(void *ref, const void *a, size_t n) {
    cryptor_t *c = ref;
    if (!c || c->mode != kCCModeGCM || (n && !a)) return kCCParamError;
    return gcm_aad(c, a, n);
}
int CCCryptorGCMaddAAD(void *ref, const void *a, size_t n) { return CCCryptorGCMAddAAD(ref, a, n); }
int CCCryptorGCMEncrypt(void *ref, const void *in, size_t n, void *out) {
    cryptor_t *c = ref;
    if (!c || c->mode != kCCModeGCM || c->op != kCCEncrypt) return kCCParamError;
    return gcm_crypt(c, in, out, n);
}
int CCCryptorGCMDecrypt(void *ref, const void *in, size_t n, void *out) {
    cryptor_t *c = ref;
    if (!c || c->mode != kCCModeGCM || c->op != kCCDecrypt) return kCCParamError;
    return gcm_crypt(c, in, out, n);
}
int CCCryptorGCMFinal(void *ref, void *tag, size_t *tl) {
    cryptor_t *c = ref;
    if (!c || c->mode != kCCModeGCM || !tag || !tl || *tl < 4 || *tl > 16) return kCCParamError;
    return gcm_tag(c, tag, *tl);
}
/* Finalize confere a tag na decifragem (e a calcula na cifragem) */
int CCCryptorGCMFinalize(void *ref, void *tag, size_t tl) {
    cryptor_t *c = ref;
    if (!c || c->mode != kCCModeGCM || !tag || tl < 4 || tl > 16) return kCCParamError;
    u8_ t[16];
    int r = gcm_tag(c, t, tl);
    if (r) return r;
    if (c->op == kCCEncrypt) { memcpy(tag, t, tl); return kCCSuccess; }
    u8_ d = 0;
    for (size_t i = 0; i < tl; i++) d |= t[i] ^ ((const u8_ *)tag)[i];
    return d ? kCCDecodeError : kCCSuccess;
}
int CCCryptorGCMReset(void *ref) { return CCCryptorReset(ref, 0); }
static int gcm_oneshot(u32_ op, u32_ alg, const void *key, size_t kl, const void *iv, size_t ivl, const void *a, size_t al,
                       const void *in, size_t n, void *out, void *tag, size_t tl) {
    void *ref;
    if (alg != kCCAlgorithmAES || !iv || !ivl) return kCCParamError;
    int r = CCCryptorCreateWithMode(op, kCCModeGCM, alg, 0, 0, key, kl, 0, 0, 0, 0, &ref);
    if (r) return r;
    r = CCCryptorGCMAddIV(ref, iv, ivl);
    if (!r && al) r = CCCryptorGCMAddAAD(ref, a, al);
    if (!r && n) r = gcm_crypt(ref, in, out, n);
    if (!r) r = CCCryptorGCMFinalize(ref, tag, tl);
    CCCryptorRelease(ref);
    if (r && op == kCCDecrypt && out) memset(out, 0, n);   /* tag ruim: nada de texto claro */
    return r;
}
int CCCryptorGCMOneshotEncrypt(u32_ alg, const void *key, size_t kl, const void *iv, size_t ivl, const void *a, size_t al,
                               const void *in, size_t n, void *out, void *tag, size_t tl) {
    return gcm_oneshot(kCCEncrypt, alg, key, kl, iv, ivl, a, al, in, n, out, tag, tl);
}
int CCCryptorGCMOneshotDecrypt(u32_ alg, const void *key, size_t kl, const void *iv, size_t ivl, const void *a, size_t al,
                               const void *in, size_t n, void *out, const void *tag, size_t tl) {
    return gcm_oneshot(kCCDecrypt, alg, key, kl, iv, ivl, a, al, in, n, out, (void *)tag, tl);
}
/* API antiga de uma chamada: na cifragem devolve a tag; na decifragem também devolve a tag calculada (o
 * chamador compara), como o CommonCrypto antigo */
int CCCryptorGCM(u32_ op, u32_ alg, const void *key, size_t kl, const void *iv, size_t ivl, const void *a, size_t al,
                 const void *in, size_t n, void *out, void *tag, size_t *tl) {
    void *ref;
    if (!tl || *tl < 4 || *tl > 16) return kCCParamError;
    int r = CCCryptorCreateWithMode(op, kCCModeGCM, alg, 0, 0, key, kl, 0, 0, 0, 0, &ref);
    if (r) return r;
    r = CCCryptorGCMAddIV(ref, iv, ivl);
    if (!r && al) r = CCCryptorGCMAddAAD(ref, a, al);
    if (!r && n) r = gcm_crypt(ref, in, out, n);
    if (!r) r = gcm_tag(ref, tag, *tl);
    CCCryptorRelease(ref);
    return r;
}

/* ================================================================ SHA-224, MD4, MD2 */
typedef CC_SHA256_CTX CC_SHA224_CTX;
int CC_SHA224_Init(CC_SHA224_CTX *c) { CC_SHA256_Init(c); memcpy(c->hash, sha224_iv, sizeof sha224_iv); return 1; }
int CC_SHA224_Update(CC_SHA224_CTX *c, const void *d, u32_ n) { return CC_SHA256_Update(c, d, n); }
int CC_SHA224_Final(u8_ *md, CC_SHA224_CTX *c) { u8_ t[32]; CC_SHA256_Final(t, c); memcpy(md, t, 28); return 1; }
u8_ *CC_SHA224(const void *d, u32_ n, u8_ *md) { CC_SHA224_CTX c; CC_SHA224_Init(&c); CC_SHA224_Update(&c, d, n); CC_SHA224_Final(md, &c); return md; }

typedef CC_MD5_CTX CC_MD4_CTX;            /* mesmo layout (A, B, C, D, Nl, Nh, data[16], num) */
static void md4_block(CC_MD4_CTX *c, const u8_ *p) {
    u32_ x[16];
    for (int i = 0; i < 16; i++) x[i] = (u32_)p[4 * i] | (u32_)p[4 * i + 1] << 8 | (u32_)p[4 * i + 2] << 16 | (u32_)p[4 * i + 3] << 24;
    u32_ a = c->A, b = c->B, cc = c->C, d = c->D;
#define F4(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define G4(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define H4(x, y, z) ((x) ^ (y) ^ (z))
    static const int r1[4] = {3, 7, 11, 19}, r2[4] = {3, 5, 9, 13}, r3[4] = {3, 9, 11, 15};
    static const int o2[16] = {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};
    static const int o3[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};
    for (int i = 0; i < 16; i++) {
        u32_ t = a + F4(b, cc, d) + x[i];
        a = d; d = cc; cc = b; b = rol32(t, r1[i & 3]);
    }
    for (int i = 0; i < 16; i++) {
        u32_ t = a + G4(b, cc, d) + x[o2[i]] + 0x5a827999u;
        a = d; d = cc; cc = b; b = rol32(t, r2[i & 3]);
    }
    for (int i = 0; i < 16; i++) {
        u32_ t = a + H4(b, cc, d) + x[o3[i]] + 0x6ed9eba1u;
        a = d; d = cc; cc = b; b = rol32(t, r3[i & 3]);
    }
#undef F4
#undef G4
#undef H4
    c->A += a; c->B += b; c->C += cc; c->D += d;
}
int CC_MD4_Init(CC_MD4_CTX *c) {
    memset(c, 0, sizeof *c);
    c->A = 0x67452301; c->B = 0xefcdab89; c->C = 0x98badcfe; c->D = 0x10325476;
    return 1;
}
int CC_MD4_Update(CC_MD4_CTX *c, const void *data, u32_ len) {
    const u8_ *p = data;
    u8_ *buf = (u8_ *)c->data;
    u64_ total = ((u64_)c->Nh << 32 | c->Nl) + len;
    c->Nl = (u32_)total; c->Nh = (u32_)(total >> 32);
    while (len--) {
        buf[c->num++] = *p++;
        if (c->num == 64) { md4_block(c, buf); c->num = 0; }
    }
    return 1;
}
int CC_MD4_Final(u8_ *md, CC_MD4_CTX *c) {
    u64_ bits = ((u64_)c->Nh << 32 | c->Nl) * 8;
    u8_ pad = 0x80, z = 0, l[8];
    CC_MD4_Update(c, &pad, 1);
    while (c->num != 56) CC_MD4_Update(c, &z, 1);
    for (int i = 0; i < 8; i++) l[i] = (u8_)(bits >> (8 * i));
    CC_MD4_Update(c, l, 8);
    u32_ h[4] = {c->A, c->B, c->C, c->D};
    for (int i = 0; i < 16; i++) md[i] = (u8_)(h[i / 4] >> (8 * (i % 4)));
    return 1;
}
u8_ *CC_MD4(const void *d, u32_ n, u8_ *md) { CC_MD4_CTX c; CC_MD4_Init(&c); CC_MD4_Update(&c, d, n); CC_MD4_Final(md, &c); return md; }

/* MD2 (RFC 1319): CC_MD2_CTX = { int num; u8 data[16]; u32 cksm[16]; u32 state[16]; } como no macOS */
typedef struct { int num; u8_ data[16]; u32_ cksm[16]; u32_ state[16]; } CC_MD2_CTX;
static const u8_ md2_S[256] = {
    41, 46, 67, 201, 162, 216, 124, 1, 61, 54, 84, 161, 236, 240, 6, 19, 98, 167, 5, 243, 192, 199, 115, 140, 152, 147, 43, 217,
    188, 76, 130, 202, 30, 155, 87, 60, 253, 212, 224, 22, 103, 66, 111, 24, 138, 23, 229, 18, 190, 78, 196, 214, 218, 158, 222,
    73, 160, 251, 245, 142, 187, 47, 238, 122, 169, 104, 121, 145, 21, 178, 7, 63, 148, 194, 16, 137, 11, 34, 95, 33, 128, 127,
    93, 154, 90, 144, 50, 39, 53, 62, 204, 231, 191, 247, 151, 3, 255, 25, 48, 179, 72, 165, 181, 209, 215, 94, 146, 42, 172, 86,
    170, 198, 79, 184, 56, 210, 150, 164, 125, 182, 118, 252, 107, 226, 156, 116, 4, 241, 69, 157, 112, 89, 100, 113, 135, 32,
    134, 91, 207, 101, 230, 45, 168, 2, 27, 96, 37, 173, 174, 176, 185, 246, 28, 70, 97, 105, 52, 64, 126, 15, 85, 71, 163, 35,
    221, 81, 175, 58, 195, 92, 249, 206, 186, 197, 234, 38, 44, 83, 13, 110, 133, 40, 132, 9, 211, 223, 205, 244, 65, 129, 77,
    82, 106, 220, 55, 200, 108, 193, 171, 250, 36, 225, 123, 8, 12, 189, 177, 74, 120, 136, 149, 139, 227, 99, 232, 109, 233,
    203, 213, 254, 59, 0, 29, 57, 242, 239, 183, 14, 102, 88, 208, 228, 166, 119, 114, 248, 235, 117, 75, 10, 49, 68, 80, 180,
    143, 237, 31, 26, 219, 153, 141, 51, 159, 17, 131, 20};
static void md2_block(CC_MD2_CTX *c, const u8_ *b) {
    u8_ x[48];
    for (int i = 0; i < 16; i++) { x[i] = (u8_)c->state[i]; x[16 + i] = b[i]; x[32 + i] = (u8_)(c->state[i] ^ b[i]); }
    u8_ t = 0;
    for (int j = 0; j < 18; j++) {
        for (int k = 0; k < 48; k++) t = x[k] ^= md2_S[t];
        t = (u8_)(t + j);
    }
    for (int i = 0; i < 16; i++) c->state[i] = x[i];
    u8_ L = (u8_)c->cksm[15];
    for (int i = 0; i < 16; i++) { c->cksm[i] ^= md2_S[b[i] ^ L]; c->cksm[i] &= 0xff; L = (u8_)c->cksm[i]; }
}
int CC_MD2_Init(CC_MD2_CTX *c) { memset(c, 0, sizeof *c); return 1; }
int CC_MD2_Update(CC_MD2_CTX *c, const void *data, u32_ len) {
    const u8_ *p = data;
    while (len--) {
        c->data[c->num++] = *p++;
        if (c->num == 16) { md2_block(c, c->data); c->num = 0; }
    }
    return 1;
}
int CC_MD2_Final(u8_ *md, CC_MD2_CTX *c) {
    u8_ v = (u8_)(16 - c->num), pad[16];
    for (int i = 0; i < v; i++) pad[i] = v;
    CC_MD2_Update(c, pad, v);
    u8_ ck[16];
    for (int i = 0; i < 16; i++) ck[i] = (u8_)c->cksm[i];
    md2_block(c, ck);
    for (int i = 0; i < 16; i++) md[i] = (u8_)c->state[i];
    return 1;
}
u8_ *CC_MD2(const void *d, u32_ n, u8_ *md) { CC_MD2_CTX c; CC_MD2_Init(&c); CC_MD2_Update(&c, d, n); CC_MD2_Final(md, &c); return md; }

/* ================================================================ CCCalibratePBKDF: rodadas para `ms` milissegundos */
u64_ mach_absolute_time(void);
u32_ CCCalibratePBKDF(u32_ alg, size_t pwlen, size_t saltlen, u32_ prf, size_t dklen, u32_ ms) {
    if (alg != kCCPBKDF2 || !dklen || !ms) return (u32_)-1;
    char pw[128];
    u8_ salt[128], out[128];
    memset(pw, 'p', sizeof pw);
    memset(salt, 's', sizeof salt);
    if (pwlen > sizeof pw) pwlen = sizeof pw;
    if (saltlen > sizeof salt) saltlen = sizeof salt;
    if (dklen > sizeof out) dklen = sizeof out;
    /* o relógio do mOS anda de milissegundo em milissegundo: dobra a amostra até ela durar ≥ 20 ms */
    u32_ probe = 1000;
    u64_ dt;
    for (;;) {
        u64_ t0 = mach_absolute_time();                   /* nanossegundos (timebase 1/1) */
        CCKeyDerivationPBKDF(alg, pw, pwlen, salt, saltlen, prf, probe, out, dklen);
        dt = mach_absolute_time() - t0;
        if (dt >= 20000000ull || probe >= (1u << 26)) break;
        probe *= 2;
    }
    if (!dt) dt = 1;
    u64_ rounds = (u64_)probe * ms * 1000000ull / dt;
    if (rounds < 10000) rounds = 10000;                   /* mínimo do CommonCrypto */
    if (rounds > 0xffffffffull) rounds = 0xffffffffull;
    return (u32_)rounds;
}
