/* mOS libSystem — cifras de bloco e de fluxo do CommonCrypto além do AES: DES, 3DES (EDE), RC2 (RFC 2268),
 * RC4 e Blowfish. Sem estado global além das tabelas do Blowfish, que são os dígitos hexadecimais de pi
 * calculados na primeira vez (fórmula de Machin em ponto fixo), em vez de copiadas.
 * Incluído por commoncrypto2.c e tools/cctest.c. Tipos: u8_, u32_, u64_ do includer. */

/* ================================================================ DES */
static const u8_ des_ip[64] = {58, 50, 42, 34, 26, 18, 10, 2, 60, 52, 44, 36, 28, 20, 12, 4, 62, 54, 46, 38, 30, 22, 14, 6,
                               64, 56, 48, 40, 32, 24, 16, 8, 57, 49, 41, 33, 25, 17, 9, 1, 59, 51, 43, 35, 27, 19, 11, 3,
                               61, 53, 45, 37, 29, 21, 13, 5, 63, 55, 47, 39, 31, 23, 15, 7};
static const u8_ des_fp[64] = {40, 8, 48, 16, 56, 24, 64, 32, 39, 7, 47, 15, 55, 23, 63, 31, 38, 6, 46, 14, 54, 22, 62, 30,
                               37, 5, 45, 13, 53, 21, 61, 29, 36, 4, 44, 12, 52, 20, 60, 28, 35, 3, 43, 11, 51, 19, 59, 27,
                               34, 2, 42, 10, 50, 18, 58, 26, 33, 1, 41, 9, 49, 17, 57, 25};
static const u8_ des_e[48] = {32, 1, 2, 3, 4, 5, 4, 5, 6, 7, 8, 9, 8, 9, 10, 11, 12, 13, 12, 13, 14, 15, 16, 17,
                              16, 17, 18, 19, 20, 21, 20, 21, 22, 23, 24, 25, 24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32, 1};
static const u8_ des_p[32] = {16, 7, 20, 21, 29, 12, 28, 17, 1, 15, 23, 26, 5, 18, 31, 10,
                              2, 8, 24, 14, 32, 27, 3, 9, 19, 13, 30, 6, 22, 11, 4, 25};
static const u8_ des_pc1[56] = {57, 49, 41, 33, 25, 17, 9, 1, 58, 50, 42, 34, 26, 18, 10, 2, 59, 51, 43, 35, 27, 19, 11, 3,
                                60, 52, 44, 36, 63, 55, 47, 39, 31, 23, 15, 7, 62, 54, 46, 38, 30, 22, 14, 6,
                                61, 53, 45, 37, 29, 21, 13, 5, 28, 20, 12, 4};
static const u8_ des_pc2[48] = {14, 17, 11, 24, 1, 5, 3, 28, 15, 6, 21, 10, 23, 19, 12, 4, 26, 8, 16, 7, 27, 20, 13, 2,
                                41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48, 44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32};
static const u8_ des_shifts[16] = {1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1};
static const u8_ des_s[8][64] = {
    {14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7, 0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8,
     4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0, 15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13},
    {15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10, 3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5,
     0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15, 13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9},
    {10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8, 13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1,
     13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7, 1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12},
    {7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15, 13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9,
     10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4, 3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14},
    {2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9, 14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6,
     4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14, 11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3},
    {12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11, 10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8,
     9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6, 4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13},
    {4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1, 13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6,
     1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2, 6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12},
    {13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7, 1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2,
     7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8, 2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11}};

typedef struct { u64_ k[16]; } des_ks;
/* permutação de `n` bits de saída a partir de uma entrada de `w` bits (bit 1 = mais significativo) */
static u64_ des_perm(u64_ in, int w, const u8_ *tab, int n) {
    u64_ o = 0;
    for (int i = 0; i < n; i++) o = o << 1 | ((in >> (w - tab[i])) & 1);
    return o;
}
static void des_setkey(des_ks *ks, const u8_ key[8]) {
    u64_ k = 0;
    for (int i = 0; i < 8; i++) k = k << 8 | key[i];
    u64_ cd = des_perm(k, 64, des_pc1, 56);
    u32_ c = (u32_)(cd >> 28) & 0xfffffff, d = (u32_)cd & 0xfffffff;
    for (int r = 0; r < 16; r++) {
        for (int s = 0; s < des_shifts[r]; s++) {
            c = ((c << 1) | (c >> 27)) & 0xfffffff;
            d = ((d << 1) | (d >> 27)) & 0xfffffff;
        }
        ks->k[r] = des_perm((u64_)c << 28 | d, 56, des_pc2, 48);
    }
}
static u32_ des_f(u32_ r, u64_ k) {
    u64_ x = des_perm(r, 32, des_e, 48) ^ k;
    u32_ o = 0;
    for (int i = 0; i < 8; i++) {
        u32_ six = (u32_)(x >> (42 - 6 * i)) & 0x3f;
        u32_ row = (six & 0x20) >> 4 | (six & 1), col = (six >> 1) & 0xf;
        o = o << 4 | des_s[i][row * 16 + col];
    }
    return (u32_)des_perm(o, 32, des_p, 32);
}
static void des_block(const des_ks *ks, int dec, const u8_ in[8], u8_ out[8]) {
    u64_ v = 0;
    for (int i = 0; i < 8; i++) v = v << 8 | in[i];
    v = des_perm(v, 64, des_ip, 64);
    u32_ l = (u32_)(v >> 32), r = (u32_)v;
    for (int i = 0; i < 16; i++) {
        u32_ t = r;
        r = l ^ des_f(r, ks->k[dec ? 15 - i : i]);
        l = t;
    }
    v = des_perm((u64_)r << 32 | l, 64, des_fp, 64);
    for (int i = 7; i >= 0; i--) { out[i] = (u8_)v; v >>= 8; }
}
typedef struct { des_ks k1, k2, k3; } des3_ks;
static void des3_block(const des3_ks *k, int dec, const u8_ in[8], u8_ out[8]) {
    u8_ t[8], t2[8];
    if (!dec) { des_block(&k->k1, 0, in, t); des_block(&k->k2, 1, t, t2); des_block(&k->k3, 0, t2, out); }
    else { des_block(&k->k3, 1, in, t); des_block(&k->k2, 0, t, t2); des_block(&k->k1, 1, t2, out); }
}

/* ================================================================ RC2 (RFC 2268) */
static const u8_ rc2_pitable[256] = {
    0xd9, 0x78, 0xf9, 0xc4, 0x19, 0xdd, 0xb5, 0xed, 0x28, 0xe9, 0xfd, 0x79, 0x4a, 0xa0, 0xd8, 0x9d,
    0xc6, 0x7e, 0x37, 0x83, 0x2b, 0x76, 0x53, 0x8e, 0x62, 0x4c, 0x64, 0x88, 0x44, 0x8b, 0xfb, 0xa2,
    0x17, 0x9a, 0x59, 0xf5, 0x87, 0xb3, 0x4f, 0x13, 0x61, 0x45, 0x6d, 0x8d, 0x09, 0x81, 0x7d, 0x32,
    0xbd, 0x8f, 0x40, 0xeb, 0x86, 0xb7, 0x7b, 0x0b, 0xf0, 0x95, 0x21, 0x22, 0x5c, 0x6b, 0x4e, 0x82,
    0x54, 0xd6, 0x65, 0x93, 0xce, 0x60, 0xb2, 0x1c, 0x73, 0x56, 0xc0, 0x14, 0xa7, 0x8c, 0xf1, 0xdc,
    0x12, 0x75, 0xca, 0x1f, 0x3b, 0xbe, 0xe4, 0xd1, 0x42, 0x3d, 0xd4, 0x30, 0xa3, 0x3c, 0xb6, 0x26,
    0x6f, 0xbf, 0x0e, 0xda, 0x46, 0x69, 0x07, 0x57, 0x27, 0xf2, 0x1d, 0x9b, 0xbc, 0x94, 0x43, 0x03,
    0xf8, 0x11, 0xc7, 0xf6, 0x90, 0xef, 0x3e, 0xe7, 0x06, 0xc3, 0xd5, 0x2f, 0xc8, 0x66, 0x1e, 0xd7,
    0x08, 0xe8, 0xea, 0xde, 0x80, 0x52, 0xee, 0xf7, 0x84, 0xaa, 0x72, 0xac, 0x35, 0x4d, 0x6a, 0x2a,
    0x96, 0x1a, 0xd2, 0x71, 0x5a, 0x15, 0x49, 0x74, 0x4b, 0x9f, 0xd0, 0x5e, 0x04, 0x18, 0xa4, 0xec,
    0xc2, 0xe0, 0x41, 0x6e, 0x0f, 0x51, 0xcb, 0xcc, 0x24, 0x91, 0xaf, 0x50, 0xa1, 0xf4, 0x70, 0x39,
    0x99, 0x7c, 0x3a, 0x85, 0x23, 0xb8, 0xb4, 0x7a, 0xfc, 0x02, 0x36, 0x5b, 0x25, 0x55, 0x97, 0x31,
    0x2d, 0x5d, 0xfa, 0x98, 0xe3, 0x8a, 0x92, 0xae, 0x05, 0xdf, 0x29, 0x10, 0x67, 0x6c, 0xba, 0xc9,
    0xd3, 0x00, 0xe6, 0xcf, 0xe1, 0x9e, 0xa8, 0x2c, 0x63, 0x16, 0x01, 0x3f, 0x58, 0xe2, 0x89, 0xa9,
    0x0d, 0x38, 0x34, 0x1b, 0xab, 0x33, 0xff, 0xb0, 0xbb, 0x48, 0x0c, 0x5f, 0xb9, 0xb1, 0xcd, 0x2e,
    0xc5, 0xf3, 0xdb, 0x47, 0xe5, 0xa5, 0x9c, 0x77, 0x0a, 0xa6, 0x20, 0x68, 0xfe, 0x7f, 0xc1, 0xad};
typedef struct { u32_ k[64]; } rc2_ks;   /* 64 palavras de 16 bits */
/* effbits = bits efetivos (o CommonCrypto usa o tamanho da chave × 8; o PKCS#12 pede 40 ou 128) */
static void rc2_setkey(rc2_ks *ks, const u8_ *key, size_t len, int effbits) {
    u8_ L[128];
    for (size_t i = 0; i < len && i < 128; i++) L[i] = key[i];
    for (size_t i = len; i < 128; i++) L[i] = rc2_pitable[(u8_)(L[i - 1] + L[i - len])];
    int T8 = (effbits + 7) / 8;
    u8_ TM = (u8_)(0xff >> (8 * T8 - effbits));
    L[128 - T8] = rc2_pitable[L[128 - T8] & TM];
    for (int i = 127 - T8; i >= 0; i--) L[i] = rc2_pitable[L[i + 1] ^ L[i + T8]];
    for (int i = 0; i < 64; i++) ks->k[i] = (u32_)L[2 * i] | (u32_)L[2 * i + 1] << 8;
}
static u32_ rol16(u32_ x, int n) { x &= 0xffff; return ((x << n) | (x >> (16 - n))) & 0xffff; }
static u32_ ror16(u32_ x, int n) { x &= 0xffff; return ((x >> n) | (x << (16 - n))) & 0xffff; }
static void rc2_block(const rc2_ks *ks, int dec, const u8_ in[8], u8_ out[8]) {
    u32_ R[4];
    static const int s[4] = {1, 2, 3, 5};
    for (int i = 0; i < 4; i++) R[i] = (u32_)in[2 * i] | (u32_)in[2 * i + 1] << 8;
    if (!dec) {
        int j = 0;
        for (int r = 0; r < 16; r++) {
            for (int i = 0; i < 4; i++) {
                R[i] = (R[i] + ks->k[j++] + (R[(i + 3) & 3] & R[(i + 2) & 3]) + (~R[(i + 3) & 3] & R[(i + 1) & 3])) & 0xffff;
                R[i] = rol16(R[i], s[i]);
            }
            if (r == 4 || r == 10)
                for (int i = 0; i < 4; i++) R[i] = (R[i] + ks->k[R[(i + 3) & 3] & 63]) & 0xffff;
        }
    } else {
        int j = 63;
        for (int r = 15; r >= 0; r--) {
            for (int i = 3; i >= 0; i--) {
                R[i] = ror16(R[i], s[i]);
                R[i] = (R[i] - ks->k[j--] - (R[(i + 3) & 3] & R[(i + 2) & 3]) - (~R[(i + 3) & 3] & R[(i + 1) & 3])) & 0xffff;
            }
            if (r == 5 || r == 11)
                for (int i = 3; i >= 0; i--) R[i] = (R[i] - ks->k[R[(i + 3) & 3] & 63]) & 0xffff;
        }
    }
    for (int i = 0; i < 4; i++) { out[2 * i] = (u8_)R[i]; out[2 * i + 1] = (u8_)(R[i] >> 8); }
}

/* ================================================================ RC4 */
typedef struct { u8_ S[256]; u8_ i, j; } rc4_st;
static void rc4_setkey(rc4_st *s, const u8_ *k, size_t n) {
    for (int i = 0; i < 256; i++) s->S[i] = (u8_)i;
    u8_ j = 0;
    for (int i = 0; i < 256; i++) {
        j = (u8_)(j + s->S[i] + k[i % n]);
        u8_ t = s->S[i]; s->S[i] = s->S[j]; s->S[j] = t;
    }
    s->i = s->j = 0;
}
static void rc4_xor(rc4_st *s, const u8_ *in, u8_ *out, size_t n) {
    for (size_t k = 0; k < n; k++) {
        s->i = (u8_)(s->i + 1);
        s->j = (u8_)(s->j + s->S[s->i]);
        u8_ t = s->S[s->i]; s->S[s->i] = s->S[s->j]; s->S[s->j] = t;
        out[k] = in[k] ^ s->S[(u8_)(s->S[s->i] + s->S[s->j])];
    }
}

/* ================================================================ Blowfish */
/* 18 + 4·256 palavras = os dígitos hexadecimais da parte fracionária de pi (Schneier). Calculados com
 * pi = 16·atan(1/5) − 4·atan(1/239) em ponto fixo de 34 000 bits. */
static u32_ bf_init_words[18 + 1024];
static volatile int bf_ready;
#define BF_LIMBS 1072                         /* 34 304 bits: 33 344 bits pedidos + folga de arredondamento */
static void fx_div_small(u32_ *a, u32_ d) {   /* a /= d (a[0] = limb mais significativo) */
    u64_ r = 0;
    for (int i = 0; i < BF_LIMBS; i++) { u64_ c = r << 32 | a[i]; a[i] = (u32_)(c / d); r = c % d; }
}
static void fx_add(u32_ *a, const u32_ *b) { u64_ c = 0; for (int i = BF_LIMBS - 1; i >= 0; i--) { c += (u64_)a[i] + b[i]; a[i] = (u32_)c; c >>= 32; } }
static void fx_sub(u32_ *a, const u32_ *b) { i64 br = 0; for (int i = BF_LIMBS - 1; i >= 0; i--) { i64 t = (i64)a[i] - b[i] - br; br = t < 0; a[i] = (u32_)t; } }
static int fx_zero(const u32_ *a) { for (int i = 0; i < BF_LIMBS; i++) if (a[i]) return 0; return 1; }
/* acc += m · atan(1/x), com o ponto entre a[0] (parte inteira) e a[1] */
static void fx_atan_inv(u32_ *acc, u32_ x, u32_ m, u32_ *t, u32_ *term) {
    memset(t, 0, BF_LIMBS * 4);
    t[0] = m;
    fx_div_small(t, x);                         /* t = m/x */
    u32_ x2 = x * x;
    for (u32_ k = 0;; k++) {
        memcpy(term, t, BF_LIMBS * 4);
        fx_div_small(term, 2 * k + 1);
        if (fx_zero(term)) break;
        if (k & 1) fx_sub(acc, term); else fx_add(acc, term);
        fx_div_small(t, x2);
    }
}
static void bf_tables_init(void) {
    if (bf_ready) return;
    u32_ *acc = calloc(BF_LIMBS, 4), *t = malloc(BF_LIMBS * 4), *term = malloc(BF_LIMBS * 4);
    if (!acc || !t || !term) { free(acc); free(t); free(term); return; }
    fx_atan_inv(acc, 5, 16, t, term);
    u32_ *neg = calloc(BF_LIMBS, 4);
    if (neg) {
        fx_atan_inv(neg, 239, 4, t, term);
        fx_sub(acc, neg);
        free(neg);
        for (int i = 0; i < 18 + 1024; i++) bf_init_words[i] = acc[1 + i];   /* acc[0] = 3 */
        __sync_synchronize();
        bf_ready = 1;
    }
    free(acc); free(t); free(term);
}
typedef struct { u32_ P[18]; u32_ S[4][256]; } bf_ks;
static u32_ bf_F(const bf_ks *k, u32_ x) {
    return ((k->S[0][x >> 24] + k->S[1][(x >> 16) & 0xff]) ^ k->S[2][(x >> 8) & 0xff]) + k->S[3][x & 0xff];
}
static void bf_encrypt_words(const bf_ks *k, u32_ *L, u32_ *R) {
    u32_ l = *L, r = *R;
    for (int i = 0; i < 16; i += 2) { l ^= k->P[i]; r ^= bf_F(k, l); r ^= k->P[i + 1]; l ^= bf_F(k, r); }
    l ^= k->P[16]; r ^= k->P[17];
    *L = r; *R = l;
}
static void bf_decrypt_words(const bf_ks *k, u32_ *L, u32_ *R) {
    u32_ l = *L, r = *R;
    for (int i = 16; i > 0; i -= 2) { l ^= k->P[i + 1]; r ^= bf_F(k, l); r ^= k->P[i]; l ^= bf_F(k, r); }
    l ^= k->P[1]; r ^= k->P[0];
    *L = r; *R = l;
}
static int bf_setkey(bf_ks *k, const u8_ *key, size_t n) {
    bf_tables_init();
    if (!bf_ready || !n || n > 56) return -1;
    memcpy(k->P, bf_init_words, sizeof k->P);
    memcpy(k->S, bf_init_words + 18, sizeof k->S);
    for (int i = 0, j = 0; i < 18; i++) {
        u32_ d = 0;
        for (int b = 0; b < 4; b++) { d = d << 8 | key[j]; j = (int)((size_t)(j + 1) % n); }
        k->P[i] ^= d;
    }
    u32_ l = 0, r = 0;
    for (int i = 0; i < 18; i += 2) { bf_encrypt_words(k, &l, &r); k->P[i] = l; k->P[i + 1] = r; }
    for (int s = 0; s < 4; s++)
        for (int i = 0; i < 256; i += 2) { bf_encrypt_words(k, &l, &r); k->S[s][i] = l; k->S[s][i + 1] = r; }
    return 0;
}
static void bf_block(const bf_ks *k, int dec, const u8_ in[8], u8_ out[8]) {
    u32_ l = (u32_)in[0] << 24 | (u32_)in[1] << 16 | (u32_)in[2] << 8 | in[3];
    u32_ r = (u32_)in[4] << 24 | (u32_)in[5] << 16 | (u32_)in[6] << 8 | in[7];
    if (dec) bf_decrypt_words(k, &l, &r); else bf_encrypt_words(k, &l, &r);
    out[0] = (u8_)(l >> 24); out[1] = (u8_)(l >> 16); out[2] = (u8_)(l >> 8); out[3] = (u8_)l;
    out[4] = (u8_)(r >> 24); out[5] = (u8_)(r >> 16); out[6] = (u8_)(r >> 8); out[7] = (u8_)r;
}
