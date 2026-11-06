/*
 * mOS — criptografia do WPA2-PSK (IEEE 802.11i):
 *   SHA-1, HMAC-SHA1, PBKDF2-SHA1 (PSK a partir da senha), PRF-SHA1 (PTK), MIC do EAPOL-Key,
 *   AES-128 (cifrar e decifrar), AES Key Wrap/Unwrap (RFC 3394, para a GTK) e AES-CCM (CCMP).
 * Verificado contra vetores padrão em tools/wifitest.c (RFC 3174, RFC 2202, IEEE 802.11i H.4,
 * RFC 3394, RFC 3610).
 */
#include "wifi.h"

/* ============================================================ SHA-1 */
typedef struct { u32 h[5]; u8 buf[64]; u64 total; u32 n; } sha1_ctx;

static u32 rol(u32 x, int n) { return x << n | x >> (32 - n); }

static void sha1_block(u32 *h, const u8 *p) {
    u32 w[80];
    for (int i = 0; i < 16; i++) w[i] = (u32)p[i * 4] << 24 | (u32)p[i * 4 + 1] << 16 | (u32)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    u32 a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
        u32 f, k;
        if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6; }
        u32 t = rol(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

static void sha1_init(sha1_ctx *c) {
    static const u32 iv[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    memcpy(c->h, iv, sizeof iv);
    c->total = 0;
    c->n = 0;
}

static void sha1_update(sha1_ctx *c, const void *data, usize len) {
    const u8 *p = data;
    c->total += len;
    while (len) {
        u32 k = (u32)MIN((usize)(64 - c->n), len);
        memcpy(c->buf + c->n, p, k);
        c->n += k; p += k; len -= k;
        if (c->n == 64) { sha1_block(c->h, c->buf); c->n = 0; }
    }
}

static void sha1_final(sha1_ctx *c, u8 out[20]) {
    u64 bits = c->total * 8;
    u8 pad = 0x80;
    sha1_update(c, &pad, 1);
    u8 z = 0;
    while (c->n != 56) sha1_update(c, &z, 1);
    u8 lb[8];
    for (int i = 0; i < 8; i++) lb[i] = (u8)(bits >> (56 - i * 8));
    sha1_update(c, lb, 8);
    for (int i = 0; i < 5; i++) { out[i * 4] = (u8)(c->h[i] >> 24); out[i * 4 + 1] = (u8)(c->h[i] >> 16); out[i * 4 + 2] = (u8)(c->h[i] >> 8); out[i * 4 + 3] = (u8)c->h[i]; }
}

void wpa_sha1(const void *data, usize len, u8 out[20]) {
    sha1_ctx c;
    sha1_init(&c);
    sha1_update(&c, data, len);
    sha1_final(&c, out);
}

/* HMAC-SHA1 sobre vários pedaços (n partes) */
static void hmac_sha1_v(const u8 *key, usize klen, int n, const void *const *parts, const usize *lens, u8 out[20]) {
    u8 k[64] = {0}, ipad[64], opad[64], ih[20];
    if (klen > 64) wpa_sha1(key, klen, k); else memcpy(k, key, klen);
    for (int i = 0; i < 64; i++) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5C; }
    sha1_ctx c;
    sha1_init(&c);
    sha1_update(&c, ipad, 64);
    for (int i = 0; i < n; i++) sha1_update(&c, parts[i], lens[i]);
    sha1_final(&c, ih);
    sha1_init(&c);
    sha1_update(&c, opad, 64);
    sha1_update(&c, ih, 20);
    sha1_final(&c, out);
}

void wpa_hmac_sha1(const u8 *key, usize klen, const void *msg, usize mlen, u8 out[20]) {
    const void *p[1] = {msg};
    usize l[1] = {mlen};
    hmac_sha1_v(key, klen, 1, p, l, out);
}

/* PSK = PBKDF2-HMAC-SHA1(senha, ssid, 4096, 32) */
void wpa_psk(const char *pass, const u8 *ssid, usize ssid_len, u8 psk[32]) {
    usize plen = strlen(pass);
    for (u32 blk = 1; blk <= 2; blk++) {
        u8 cnt[4] = {(u8)(blk >> 24), (u8)(blk >> 16), (u8)(blk >> 8), (u8)blk};
        const void *p[2] = {ssid, cnt};
        usize l[2] = {ssid_len, 4};
        u8 u[20], t[20];
        hmac_sha1_v((const u8 *)pass, plen, 2, p, l, u);
        memcpy(t, u, 20);
        for (int i = 1; i < 4096; i++) {
            wpa_hmac_sha1((const u8 *)pass, plen, u, 20, u);
            for (int j = 0; j < 20; j++) t[j] ^= u[j];
        }
        memcpy(psk + (blk - 1) * 20, t, blk == 1 ? 20 : 12);
    }
}

/* PRF-SHA1 (802.11i 8.5.1.1): saída de len bytes */
void wpa_prf(const u8 *key, usize klen, const char *label, const u8 *data, usize dlen, u8 *out, usize len) {
    u8 z = 0, ctr, h[20];
    for (usize done = 0, i = 0; done < len; i++) {
        ctr = (u8)i;
        const void *p[4] = {label, &z, data, &ctr};
        usize l[4] = {strlen(label), 1, dlen, 1};
        hmac_sha1_v(key, klen, 4, p, l, h);
        usize k = MIN((usize)20, len - done);
        memcpy(out + done, h, k);
        done += k;
    }
}

/* PTK = PRF-384(PMK, "Pairwise key expansion", min(AA,SA) || max(AA,SA) || min(ANonce,SNonce) || max(...)) */
void wpa_ptk(const u8 pmk[32], const u8 aa[6], const u8 sa[6], const u8 anonce[32], const u8 snonce[32], u8 ptk[48]) {
    u8 d[76];
    bool a_first = memcmp(aa, sa, 6) < 0;
    memcpy(d, a_first ? aa : sa, 6);
    memcpy(d + 6, a_first ? sa : aa, 6);
    bool an_first = memcmp(anonce, snonce, 32) < 0;
    memcpy(d + 12, an_first ? anonce : snonce, 32);
    memcpy(d + 44, an_first ? snonce : anonce, 32);
    wpa_prf(pmk, 32, "Pairwise key expansion", d, sizeof d, ptk, 48);
}

/* MIC do EAPOL-Key (descritor v2: HMAC-SHA1, 16 primeiros bytes) */
void wpa_eapol_mic(const u8 kck[16], const u8 *frame, usize len, u8 mic[16]) {
    u8 h[20];
    wpa_hmac_sha1(kck, 16, frame, len, h);
    memcpy(mic, h, 16);
}

/* ============================================================ AES-128 */
static const u8 SBOX[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16};
static u8 INV[256];

static u8 xt(u8 x) { return (u8)(x << 1 ^ (x & 0x80 ? 0x1B : 0)); }
static u8 mul(u8 a, u8 b) { u8 r = 0; while (b) { if (b & 1) r ^= a; a = xt(a); b >>= 1; } return r; }

void wpa_aes_setkey(wpa_aes_t *c, const u8 key[16]) {
    if (!INV[0x63 ^ 0] && SBOX[0] == 0x63) for (int i = 0; i < 256; i++) INV[SBOX[i]] = (u8)i;
    memcpy(c->rk, key, 16);
    u8 rcon = 1;
    for (int i = 16; i < 176; i += 4) {
        u8 t[4];
        memcpy(t, c->rk + i - 4, 4);
        if (i % 16 == 0) {
            u8 x = t[0];
            t[0] = SBOX[t[1]] ^ rcon; t[1] = SBOX[t[2]]; t[2] = SBOX[t[3]]; t[3] = SBOX[x];
            rcon = xt(rcon);
        }
        for (int j = 0; j < 4; j++) c->rk[i + j] = c->rk[i - 16 + j] ^ t[j];
    }
}

void wpa_aes_encrypt(const wpa_aes_t *c, const u8 in[16], u8 out[16]) {
    u8 s[16];
    for (int i = 0; i < 16; i++) s[i] = in[i] ^ c->rk[i];
    for (int r = 1; r <= 10; r++) {
        u8 t[16];
        for (int i = 0; i < 16; i++) t[i] = SBOX[s[(i + 4 * (i % 4)) % 16]];     /* SubBytes + ShiftRows */
        if (r < 10)
            for (int col = 0; col < 4; col++) {
                u8 *a = t + col * 4, b0 = a[0], b1 = a[1], b2 = a[2], b3 = a[3];
                a[0] = xt(b0) ^ xt(b1) ^ b1 ^ b2 ^ b3;
                a[1] = b0 ^ xt(b1) ^ xt(b2) ^ b2 ^ b3;
                a[2] = b0 ^ b1 ^ xt(b2) ^ xt(b3) ^ b3;
                a[3] = xt(b0) ^ b0 ^ b1 ^ b2 ^ xt(b3);
            }
        for (int i = 0; i < 16; i++) s[i] = t[i] ^ c->rk[r * 16 + i];
    }
    memcpy(out, s, 16);
}

void wpa_aes_decrypt(const wpa_aes_t *c, const u8 in[16], u8 out[16]) {
    if (!INV[0x63]) for (int i = 0; i < 256; i++) INV[SBOX[i]] = (u8)i;
    u8 s[16];
    for (int i = 0; i < 16; i++) s[i] = in[i] ^ c->rk[160 + i];
    for (int r = 9; r >= 0; r--) {
        u8 t[16];
        for (int i = 0; i < 16; i++) t[(i + 4 * (i % 4)) % 16] = INV[s[i]];     /* InvShiftRows + InvSubBytes */
        for (int i = 0; i < 16; i++) t[i] ^= c->rk[r * 16 + i];
        if (r > 0)
            for (int col = 0; col < 4; col++) {
                u8 *a = t + col * 4, b0 = a[0], b1 = a[1], b2 = a[2], b3 = a[3];
                a[0] = mul(b0, 14) ^ mul(b1, 11) ^ mul(b2, 13) ^ mul(b3, 9);
                a[1] = mul(b0, 9) ^ mul(b1, 14) ^ mul(b2, 11) ^ mul(b3, 13);
                a[2] = mul(b0, 13) ^ mul(b1, 9) ^ mul(b2, 14) ^ mul(b3, 11);
                a[3] = mul(b0, 11) ^ mul(b1, 13) ^ mul(b2, 9) ^ mul(b3, 14);
            }
        memcpy(s, t, 16);
    }
    memcpy(out, s, 16);
}

/* ============================================================ AES Key Wrap (RFC 3394) */
bool wpa_key_unwrap(const u8 kek[16], const u8 *in, usize len, u8 *out) {
    if (len < 24 || len % 8) return false;
    usize n = len / 8 - 1;
    wpa_aes_t c;
    wpa_aes_setkey(&c, kek);
    u8 a[8], b[16];
    memcpy(a, in, 8);
    memcpy(out, in + 8, n * 8);
    for (int j = 5; j >= 0; j--)
        for (usize i = n; i >= 1; i--) {
            u64 t = (u64)n * (u64)j + i;
            memcpy(b, a, 8);
            for (int k = 0; k < 8; k++) b[7 - k] ^= (u8)(t >> (k * 8));
            memcpy(b + 8, out + (i - 1) * 8, 8);
            wpa_aes_decrypt(&c, b, b);
            memcpy(a, b, 8);
            memcpy(out + (i - 1) * 8, b + 8, 8);
        }
    for (int i = 0; i < 8; i++) if (a[i] != 0xA6) return false;
    return true;
}

bool wpa_key_wrap(const u8 kek[16], const u8 *in, usize len, u8 *out) {
    if (len < 16 || len % 8) return false;
    usize n = len / 8;
    wpa_aes_t c;
    wpa_aes_setkey(&c, kek);
    u8 a[8], b[16];
    memset(a, 0xA6, 8);
    memcpy(out + 8, in, len);
    for (int j = 0; j <= 5; j++)
        for (usize i = 1; i <= n; i++) {
            memcpy(b, a, 8);
            memcpy(b + 8, out + i * 8, 8);
            wpa_aes_encrypt(&c, b, b);
            u64 t = (u64)n * (u64)j + i;
            memcpy(a, b, 8);
            for (int k = 0; k < 8; k++) a[7 - k] ^= (u8)(t >> (k * 8));
            memcpy(out + i * 8, b + 8, 8);
        }
    memcpy(out, a, 8);
    return true;
}

/* ============================================================ AES-CCM (M = 8, L = 2), como no CCMP */
static void ccm_mac(const wpa_aes_t *c, const u8 nonce[13], const u8 *aad, usize alen, const u8 *msg, usize mlen, u8 tag[8]) {
    u8 x[16], blk[16];
    blk[0] = 0x40 | ((8 - 2) / 2) << 3 | (2 - 1);  /* Adata, M=8, L=2 */
    memcpy(blk + 1, nonce, 13);
    blk[14] = (u8)(mlen >> 8); blk[15] = (u8)mlen;
    wpa_aes_encrypt(c, blk, x);
    /* AAD com o tamanho em 2 bytes na frente */
    usize pos = 0;
    memset(blk, 0, 16);
    blk[0] = (u8)(alen >> 8); blk[1] = (u8)alen;
    usize k = MIN(alen, (usize)14);
    memcpy(blk + 2, aad, k);
    pos = k;
    for (int i = 0; i < 16; i++) x[i] ^= blk[i];
    wpa_aes_encrypt(c, x, x);
    while (pos < alen) {
        memset(blk, 0, 16);
        k = MIN(alen - pos, (usize)16);
        memcpy(blk, aad + pos, k);
        pos += k;
        for (int i = 0; i < 16; i++) x[i] ^= blk[i];
        wpa_aes_encrypt(c, x, x);
    }
    for (pos = 0; pos < mlen; pos += 16) {
        memset(blk, 0, 16);
        k = MIN(mlen - pos, (usize)16);
        memcpy(blk, msg + pos, k);
        for (int i = 0; i < 16; i++) x[i] ^= blk[i];
        wpa_aes_encrypt(c, x, x);
    }
    memcpy(tag, x, 8);
}

static void ccm_ctr(const wpa_aes_t *c, const u8 nonce[13], u16 ctr, u8 out[16]) {
    u8 a[16];
    a[0] = 2 - 1;
    memcpy(a + 1, nonce, 13);
    a[14] = (u8)(ctr >> 8); a[15] = (u8)ctr;
    wpa_aes_encrypt(c, a, out);
}

/* cifra data[len] no lugar e escreve o MIC de 8 bytes em mic */
void wpa_ccm_encrypt(const u8 key[16], const u8 nonce[13], const u8 *aad, usize alen, u8 *data, usize len, u8 mic[8]) {
    wpa_aes_t c;
    wpa_aes_setkey(&c, key);
    u8 tag[8], s[16];
    ccm_mac(&c, nonce, aad, alen, data, len, tag);
    for (usize pos = 0, ctr = 1; pos < len; pos += 16, ctr++) {
        ccm_ctr(&c, nonce, (u16)ctr, s);
        for (usize i = 0; i < 16 && pos + i < len; i++) data[pos + i] ^= s[i];
    }
    ccm_ctr(&c, nonce, 0, s);
    for (int i = 0; i < 8; i++) mic[i] = tag[i] ^ s[i];
}

/* decifra no lugar; false se o MIC não conferir (o conteúdo então não deve ser usado) */
bool wpa_ccm_decrypt(const u8 key[16], const u8 nonce[13], const u8 *aad, usize alen, u8 *data, usize len, const u8 mic[8]) {
    wpa_aes_t c;
    wpa_aes_setkey(&c, key);
    u8 s[16], tag[8];
    for (usize pos = 0, ctr = 1; pos < len; pos += 16, ctr++) {
        ccm_ctr(&c, nonce, (u16)ctr, s);
        for (usize i = 0; i < 16 && pos + i < len; i++) data[pos + i] ^= s[i];
    }
    ccm_mac(&c, nonce, aad, alen, data, len, tag);
    ccm_ctr(&c, nonce, 0, s);
    u8 diff = 0;
    for (int i = 0; i < 8; i++) diff |= (u8)(tag[i] ^ s[i] ^ mic[i]);
    return diff == 0;
}
