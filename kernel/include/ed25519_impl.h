/* mOS — Ed25519 (RFC 8032), implementação compartilhada entre o kernel (só verificação: kernel/net/ed25519.c) e as
 * ferramentas do host (assinatura: tools/mkimg.c). Aritmética de corpo e de grupo no estilo do TweetNaCl (domínio público,
 * Bernstein et al.), reescrita aqui com SHA-512 incremental próprio (a mensagem não é copiada) e sem deslocamentos de
 * números negativos. Inclua este arquivo uma vez por unidade de compilação, depois de definir ed_u8/ed_u32/ed_u64/ed_i64
 * (ou deixar os padrões de <stdint.h> no host).
 *
 *   ed25519_pubkey(pk, seed)                      chave pública a partir da semente de 32 bytes
 *   ed25519_sign(sig, msg, len, seed, pk)          assinatura destacada de 64 bytes (determinística)
 *   ed25519_verify(sig, msg, len, pk) -> 1 válida  recusa S >= L (assinaturas maleáveis) e pontos inválidos
 */
#pragma once

#ifndef ED_TYPES_DEFINED
#include <stdint.h>
#include <stddef.h>
typedef uint8_t ed_u8;
typedef uint32_t ed_u32;
typedef uint64_t ed_u64;
typedef int64_t ed_i64;
typedef size_t ed_size;
#endif

/* ------------------------------------------------------------------ SHA-512 */
typedef struct { ed_u64 h[8]; ed_u8 buf[128]; ed_u64 total; ed_u32 n; } ed_sha512;

static const ed_u64 ED_K512[80] = {
    0x428a2f98d728ae22ull, 0x7137449123ef65cdull, 0xb5c0fbcfec4d3b2full, 0xe9b5dba58189dbbcull, 0x3956c25bf348b538ull,
    0x59f111f1b605d019ull, 0x923f82a4af194f9bull, 0xab1c5ed5da6d8118ull, 0xd807aa98a3030242ull, 0x12835b0145706fbeull,
    0x243185be4ee4b28cull, 0x550c7dc3d5ffb4e2ull, 0x72be5d74f27b896full, 0x80deb1fe3b1696b1ull, 0x9bdc06a725c71235ull,
    0xc19bf174cf692694ull, 0xe49b69c19ef14ad2ull, 0xefbe4786384f25e3ull, 0x0fc19dc68b8cd5b5ull, 0x240ca1cc77ac9c65ull,
    0x2de92c6f592b0275ull, 0x4a7484aa6ea6e483ull, 0x5cb0a9dcbd41fbd4ull, 0x76f988da831153b5ull, 0x983e5152ee66dfabull,
    0xa831c66d2db43210ull, 0xb00327c898fb213full, 0xbf597fc7beef0ee4ull, 0xc6e00bf33da88fc2ull, 0xd5a79147930aa725ull,
    0x06ca6351e003826full, 0x142929670a0e6e70ull, 0x27b70a8546d22ffcull, 0x2e1b21385c26c926ull, 0x4d2c6dfc5ac42aedull,
    0x53380d139d95b3dfull, 0x650a73548baf63deull, 0x766a0abb3c77b2a8ull, 0x81c2c92e47edaee6ull, 0x92722c851482353bull,
    0xa2bfe8a14cf10364ull, 0xa81a664bbc423001ull, 0xc24b8b70d0f89791ull, 0xc76c51a30654be30ull, 0xd192e819d6ef5218ull,
    0xd69906245565a910ull, 0xf40e35855771202aull, 0x106aa07032bbd1b8ull, 0x19a4c116b8d2d0c8ull, 0x1e376c085141ab53ull,
    0x2748774cdf8eeb99ull, 0x34b0bcb5e19b48a8ull, 0x391c0cb3c5c95a63ull, 0x4ed8aa4ae3418acbull, 0x5b9cca4f7763e373ull,
    0x682e6ff3d6b2b8a3ull, 0x748f82ee5defb2fcull, 0x78a5636f43172f60ull, 0x84c87814a1f0ab72ull, 0x8cc702081a6439ecull,
    0x90befffa23631e28ull, 0xa4506cebde82bde9ull, 0xbef9a3f7b2c67915ull, 0xc67178f2e372532bull, 0xca273eceea26619cull,
    0xd186b8c721c0c207ull, 0xeada7dd6cde0eb1eull, 0xf57d4f7fee6ed178ull, 0x06f067aa72176fbaull, 0x0a637dc5a2c898a6ull,
    0x113f9804bef90daeull, 0x1b710b35131c471bull, 0x28db77f523047d84ull, 0x32caab7b40c72493ull, 0x3c9ebe0a15c9bebcull,
    0x431d67c49c100d4cull, 0x4cc5d4becb3e42b6ull, 0x597f299cfc657e2aull, 0x5fcb6fab3ad6faecull, 0x6c44198c4a475817ull};

#define ED_ROR(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
static void ed_sha512_block(ed_sha512 *c, const ed_u8 *p) {
    ed_u64 w[80], a, b, cc, d, e, f, g, h;
    for (int i = 0; i < 16; i++) {
        ed_u64 v = 0;
        for (int k = 0; k < 8; k++) v = v << 8 | p[i * 8 + k];
        w[i] = v;
    }
    for (int i = 16; i < 80; i++) {
        ed_u64 s0 = ED_ROR(w[i - 15], 1) ^ ED_ROR(w[i - 15], 8) ^ (w[i - 15] >> 7);
        ed_u64 s1 = ED_ROR(w[i - 2], 19) ^ ED_ROR(w[i - 2], 61) ^ (w[i - 2] >> 6);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = c->h[0]; b = c->h[1]; cc = c->h[2]; d = c->h[3]; e = c->h[4]; f = c->h[5]; g = c->h[6]; h = c->h[7];
    for (int i = 0; i < 80; i++) {
        ed_u64 t1 = h + (ED_ROR(e, 14) ^ ED_ROR(e, 18) ^ ED_ROR(e, 41)) + ((e & f) ^ (~e & g)) + ED_K512[i] + w[i];
        ed_u64 t2 = (ED_ROR(a, 28) ^ ED_ROR(a, 34) ^ ED_ROR(a, 39)) + ((a & b) ^ (a & cc) ^ (b & cc));
        h = g; g = f; f = e; e = d + t1; d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d; c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}
static void ed_sha512_init(ed_sha512 *c) {
    static const ed_u64 iv[8] = {0x6a09e667f3bcc908ull, 0xbb67ae8584caa73bull, 0x3c6ef372fe94f82bull, 0xa54ff53a5f1d36f1ull,
                                 0x510e527fade682d1ull, 0x9b05688c2b3e6c1full, 0x1f83d9abfb41bd6bull, 0x5be0cd19137e2179ull};
    for (int i = 0; i < 8; i++) c->h[i] = iv[i];
    c->total = 0; c->n = 0;
}
static void ed_sha512_update(ed_sha512 *c, const void *data, ed_size len) {
    const ed_u8 *p = data;
    c->total += len;
    while (len) {
        ed_size k = 128 - c->n < len ? 128 - c->n : len;
        for (ed_size i = 0; i < k; i++) c->buf[c->n + i] = p[i];
        c->n += (ed_u32)k; p += k; len -= k;
        if (c->n == 128) { ed_sha512_block(c, c->buf); c->n = 0; }
    }
}
static void ed_sha512_final(ed_sha512 *c, ed_u8 out[64]) {
    ed_u64 bits = c->total * 8;
    ed_u8 pad = 0x80;
    ed_sha512_update(c, &pad, 1);
    pad = 0;
    while (c->n != 112) ed_sha512_update(c, &pad, 1);
    ed_u8 l[16] = {0};
    for (int i = 0; i < 8; i++) l[8 + i] = (ed_u8)(bits >> (56 - 8 * i));
    ed_sha512_update(c, l, 16);
    for (int i = 0; i < 8; i++)
        for (int k = 0; k < 8; k++) out[i * 8 + k] = (ed_u8)(c->h[i] >> (56 - 8 * k));
}

/* ------------------------------------------------------------------ corpo GF(2^255 - 19), 16 limbs de 16 bits */
typedef ed_i64 ed_gf[16];
static const ed_gf ED_GF0 = {0};
static const ed_gf ED_GF1 = {1};
static const ed_gf ED_D = {0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070,
                           0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203};
static const ed_gf ED_D2 = {0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283, 0x149a, 0x00e0,
                            0xd130, 0xeef3, 0x80f2, 0x198e, 0xfce7, 0x56df, 0xd9dc, 0x2406};
static const ed_gf ED_X = {0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525, 0xc760, 0x692c,
                           0xdc5c, 0xfdd6, 0xe231, 0xc0a4, 0x53fe, 0xcd6e, 0x36d3, 0x2169};
static const ed_gf ED_Y = {0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666,
                           0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666};
static const ed_gf ED_I = {0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43,
                           0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83};

static void ed_set(ed_gf r, const ed_gf a) { for (int i = 0; i < 16; i++) r[i] = a[i]; }
static void ed_car(ed_gf o) {
    for (int i = 0; i < 16; i++) {
        o[i] += 65536;
        ed_i64 c = o[i] >> 16;               /* deslocamento aritmético (o[i] pode ser negativo) */
        if (i < 15) o[i + 1] += c - 1;
        else o[0] += 38 * (c - 1);
        o[i] -= c * 65536;
    }
}
static void ed_sel(ed_gf p, ed_gf q, int b) {
    ed_i64 c = ~((ed_i64)b - 1);
    for (int i = 0; i < 16; i++) { ed_i64 t = c & (p[i] ^ q[i]); p[i] ^= t; q[i] ^= t; }
}
static void ed_pack25519(ed_u8 *o, const ed_gf n) {
    ed_gf m, t;
    ed_set(t, n);
    ed_car(t); ed_car(t); ed_car(t);
    for (int j = 0; j < 2; j++) {
        m[0] = t[0] - 0xffed;
        for (int i = 1; i < 15; i++) { m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1); m[i - 1] &= 0xffff; }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        int b = (int)((m[15] >> 16) & 1);
        m[14] &= 0xffff;
        ed_sel(t, m, 1 - b);
    }
    for (int i = 0; i < 16; i++) { o[2 * i] = (ed_u8)(t[i] & 0xff); o[2 * i + 1] = (ed_u8)(t[i] >> 8); }
}
static int ed_neq(const ed_gf a, const ed_gf b) {
    ed_u8 c[32], d[32];
    ed_pack25519(c, a); ed_pack25519(d, b);
    ed_u32 x = 0;
    for (int i = 0; i < 32; i++) x |= c[i] ^ d[i];
    return x != 0;
}
static ed_u8 ed_par(const ed_gf a) { ed_u8 d[32]; ed_pack25519(d, a); return d[0] & 1; }
static void ed_unpack25519(ed_gf o, const ed_u8 *n) {
    for (int i = 0; i < 16; i++) o[i] = n[2 * i] + ((ed_i64)n[2 * i + 1] << 8);
    o[15] &= 0x7fff;
}
static void ed_A(ed_gf o, const ed_gf a, const ed_gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] + b[i]; }
static void ed_Z(ed_gf o, const ed_gf a, const ed_gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] - b[i]; }
static void ed_M(ed_gf o, const ed_gf a, const ed_gf b) {
    ed_i64 t[31];
    for (int i = 0; i < 31; i++) t[i] = 0;
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 16; j++) t[i + j] += a[i] * b[j];
    for (int i = 0; i < 15; i++) t[i] += 38 * t[i + 16];
    for (int i = 0; i < 16; i++) o[i] = t[i];
    ed_car(o); ed_car(o);
}
static void ed_S(ed_gf o, const ed_gf a) { ed_M(o, a, a); }
static void ed_inv(ed_gf o, const ed_gf in) {
    ed_gf c;
    ed_set(c, in);
    for (int a = 253; a >= 0; a--) { ed_S(c, c); if (a != 2 && a != 4) ed_M(c, c, in); }
    ed_set(o, c);
}
static void ed_pow2523(ed_gf o, const ed_gf in) {
    ed_gf c;
    ed_set(c, in);
    for (int a = 250; a >= 0; a--) { ed_S(c, c); if (a != 1) ed_M(c, c, in); }
    ed_set(o, c);
}

/* ------------------------------------------------------------------ grupo (coordenadas estendidas X, Y, Z, T) */
static void ed_add(ed_gf p[4], ed_gf q[4]) {
    ed_gf a, b, c, d, t, e, f, g, h;
    ed_Z(a, p[1], p[0]); ed_Z(t, q[1], q[0]); ed_M(a, a, t);
    ed_A(b, p[0], p[1]); ed_A(t, q[0], q[1]); ed_M(b, b, t);
    ed_M(c, p[3], q[3]); ed_M(c, c, ED_D2);
    ed_M(d, p[2], q[2]); ed_A(d, d, d);
    ed_Z(e, b, a); ed_Z(f, d, c); ed_A(g, d, c); ed_A(h, b, a);
    ed_M(p[0], e, f); ed_M(p[1], h, g); ed_M(p[2], g, f); ed_M(p[3], e, h);
}
static void ed_cswap(ed_gf p[4], ed_gf q[4], ed_u8 b) { for (int i = 0; i < 4; i++) ed_sel(p[i], q[i], b); }
static void ed_pack(ed_u8 *r, ed_gf p[4]) {
    ed_gf tx, ty, zi;
    ed_inv(zi, p[2]);
    ed_M(tx, p[0], zi);
    ed_M(ty, p[1], zi);
    ed_pack25519(r, ty);
    r[31] ^= (ed_u8)(ed_par(tx) << 7);
}
static void ed_scalarmult(ed_gf p[4], ed_gf q[4], const ed_u8 *s) {
    ed_set(p[0], ED_GF0); ed_set(p[1], ED_GF1); ed_set(p[2], ED_GF1); ed_set(p[3], ED_GF0);
    for (int i = 255; i >= 0; --i) {
        ed_u8 b = (s[i / 8] >> (i & 7)) & 1;
        ed_cswap(p, q, b);
        ed_add(q, p);
        ed_add(p, p);
        ed_cswap(p, q, b);
    }
}
static void ed_scalarbase(ed_gf p[4], const ed_u8 *s) {
    ed_gf q[4];
    ed_set(q[0], ED_X); ed_set(q[1], ED_Y); ed_set(q[2], ED_GF1); ed_M(q[3], ED_X, ED_Y);
    ed_scalarmult(p, q, s);
}

/* ------------------------------------------------------------------ escalares mod L */
static const ed_i64 ED_L[32] = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10};
static void ed_modL(ed_u8 *r, ed_i64 x[64]) {
    ed_i64 carry;
    int i, j;
    for (i = 63; i >= 32; --i) {
        carry = 0;
        for (j = i - 32; j < i - 12; ++j) {
            x[j] += carry - 16 * x[i] * ED_L[j - (i - 32)];
            carry = (x[j] + 128) >> 8;
            x[j] -= carry * 256;
        }
        x[j] += carry;
        x[i] = 0;
    }
    carry = 0;
    for (j = 0; j < 32; j++) {
        x[j] += carry - (x[31] >> 4) * ED_L[j];
        carry = x[j] >> 8;
        x[j] &= 255;
    }
    for (j = 0; j < 32; j++) x[j] -= carry * ED_L[j];
    for (i = 0; i < 32; i++) {
        x[i + 1] += x[i] >> 8;
        r[i] = (ed_u8)(x[i] & 255);
    }
}
static void ed_reduce(ed_u8 *r) {                 /* r: 64 bytes -> 32 bytes (mod L) */
    ed_i64 x[64];
    for (int i = 0; i < 64; i++) x[i] = r[i];
    for (int i = 0; i < 64; i++) r[i] = 0;
    ed_modL(r, x);
}
/* S < L (little-endian), condição do RFC 8032 para não aceitar assinaturas maleáveis */
static int ed_scalar_ok(const ed_u8 s[32]) {
    for (int i = 31; i >= 0; i--) {
        if (s[i] < (ed_u8)ED_L[i]) return 1;
        if (s[i] > (ed_u8)ED_L[i]) return 0;
    }
    return 0;                                      /* S == L */
}

static int ed_unpackneg(ed_gf r[4], const ed_u8 p[32]) {
    ed_gf t, chk, num, den, den2, den4, den6;
    ed_set(r[2], ED_GF1);
    ed_unpack25519(r[1], p);
    ed_S(num, r[1]);
    ed_M(den, num, ED_D);
    ed_Z(num, num, r[2]);
    ed_A(den, r[2], den);
    ed_S(den2, den); ed_S(den4, den2); ed_M(den6, den4, den2);
    ed_M(t, den6, num); ed_M(t, t, den);
    ed_pow2523(t, t);
    ed_M(t, t, num); ed_M(t, t, den); ed_M(t, t, den); ed_M(r[0], t, den);
    ed_S(chk, r[0]); ed_M(chk, chk, den);
    if (ed_neq(chk, num)) ed_M(r[0], r[0], ED_I);
    ed_S(chk, r[0]); ed_M(chk, chk, den);
    if (ed_neq(chk, num)) return -1;
    if (ed_par(r[0]) == (p[31] >> 7)) ed_Z(r[0], ED_GF0, r[0]);
    ed_M(r[3], r[0], r[1]);
    return 0;
}

/* ------------------------------------------------------------------ API */
static void ed_expand(const ed_u8 seed[32], ed_u8 d[64]) {
    ed_sha512 c;
    ed_sha512_init(&c); ed_sha512_update(&c, seed, 32); ed_sha512_final(&c, d);
    d[0] &= 248; d[31] &= 127; d[31] |= 64;
}

static void ed25519_pubkey(ed_u8 pk[32], const ed_u8 seed[32]) {
    ed_u8 d[64];
    ed_gf p[4];
    ed_expand(seed, d);
    ed_scalarbase(p, d);
    ed_pack(pk, p);
}

static void ed25519_sign(ed_u8 sig[64], const void *msg, ed_size len, const ed_u8 seed[32], const ed_u8 pk[32]) {
    ed_u8 d[64], r[64], h[64];
    ed_gf p[4];
    ed_sha512 c;
    ed_expand(seed, d);
    ed_sha512_init(&c); ed_sha512_update(&c, d + 32, 32); ed_sha512_update(&c, msg, len); ed_sha512_final(&c, r);
    ed_reduce(r);
    ed_scalarbase(p, r);
    ed_pack(sig, p);                                         /* R */
    ed_sha512_init(&c); ed_sha512_update(&c, sig, 32); ed_sha512_update(&c, pk, 32); ed_sha512_update(&c, msg, len);
    ed_sha512_final(&c, h);
    ed_reduce(h);
    ed_i64 x[64];
    for (int i = 0; i < 64; i++) x[i] = 0;
    for (int i = 0; i < 32; i++) x[i] = r[i];
    for (int i = 0; i < 32; i++)
        for (int j = 0; j < 32; j++) x[i + j] += (ed_i64)h[i] * d[j];
    ed_modL(sig + 32, x);                                    /* S */
}

static int ed25519_verify(const ed_u8 sig[64], const void *msg, ed_size len, const ed_u8 pk[32]) {
    ed_u8 h[64], t[32];
    ed_gf p[4], q[4];
    ed_sha512 c;
    if (!ed_scalar_ok(sig + 32)) return 0;
    if (ed_unpackneg(q, pk)) return 0;
    ed_sha512_init(&c); ed_sha512_update(&c, sig, 32); ed_sha512_update(&c, pk, 32); ed_sha512_update(&c, msg, len);
    ed_sha512_final(&c, h);
    ed_reduce(h);
    ed_scalarmult(p, q, h);                                  /* h·(−A) */
    ed_scalarbase(q, sig + 32);                              /* S·B */
    ed_add(p, q);
    ed_pack(t, p);
    ed_u32 diff = 0;
    for (int i = 0; i < 32; i++) diff |= t[i] ^ sig[i];
    return diff == 0;
}
