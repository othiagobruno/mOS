/* Selo do Keychain em disco.
 * A chave NÃO é senha nem credencial: é HMAC-SHA256 do nome de login (USER, LOGNAME
 * ou o último componente de HOME), com sal fixo do domínio "mOS-keychain-v2".
 * Quem lê o arquivo sem passar por SecItem* não vê o segredo. Quem conhece o nome
 * de login deste usuário consegue abrir: é de propósito, não há senha de sessão.
 *
 * Arquivo versão 2: "mOS-keychain 2\n" + nonce de 16 bytes + ciphertext + MAC de 32.
 * O miolo cifrado é o texto da versão 1. Quem inclui este arquivo já declarou
 * u8, u32, size_t, ssize_t, malloc, free, memcpy, memcmp, strlen, open, read, close.
 */
void *memset(void *s, int c, size_t n);

#ifndef MAC_O_RDONLY
#define MAC_O_RDONLY 0
#endif

static u32 kc_rotr(u32 x, int n) { return (x >> n) | (x << (32 - n)); }

static void kc_compress(u32 st[8], const u8 blk[64]) {
    static const u32 K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    u32 w[64];
    for (int i = 0; i < 16; i++) {
        const u8 *p = blk + (u32)i * 4;
        w[i] = ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
    }
    for (int i = 16; i < 64; i++) {
        u32 s0 = kc_rotr(w[i - 15], 7) ^ kc_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        u32 s1 = kc_rotr(w[i - 2], 17) ^ kc_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    u32 a = st[0], b = st[1], c = st[2], d = st[3], e = st[4], f = st[5], g = st[6], h = st[7];
    for (int i = 0; i < 64; i++) {
        u32 S1 = kc_rotr(e, 6) ^ kc_rotr(e, 11) ^ kc_rotr(e, 25);
        u32 ch = (e & f) ^ (~e & g);
        u32 t1 = h + S1 + ch + K[i] + w[i];
        u32 S0 = kc_rotr(a, 2) ^ kc_rotr(a, 13) ^ kc_rotr(a, 22);
        u32 mj = (a & b) ^ (a & c) ^ (b & c);
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + S0 + mj;
    }
    st[0] += a; st[1] += b; st[2] += c; st[3] += d; st[4] += e; st[5] += f; st[6] += g; st[7] += h;
}

static void kc_sha256(const u8 *msg, u32 len, u8 out[32]) {
    u32 st[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    u8 block[64];
    u32 off = 0;
    unsigned long long bits = (unsigned long long)len * 8ull;
    while (off + 64 <= len) {
        kc_compress(st, msg + off);
        off += 64;
    }
    u32 rem = len - off;
    memset(block, 0, 64);
    if (rem && msg) memcpy(block, msg + off, rem);
    block[rem] = 0x80;
    if (rem >= 56) {
        kc_compress(st, block);
        memset(block, 0, 64);
    }
    block[56] = (u8)(bits >> 56); block[57] = (u8)(bits >> 48); block[58] = (u8)(bits >> 40); block[59] = (u8)(bits >> 32);
    block[60] = (u8)(bits >> 24); block[61] = (u8)(bits >> 16); block[62] = (u8)(bits >> 8); block[63] = (u8)bits;
    kc_compress(st, block);
    for (int i = 0; i < 8; i++) {
        out[i * 4] = (u8)(st[i] >> 24); out[i * 4 + 1] = (u8)(st[i] >> 16);
        out[i * 4 + 2] = (u8)(st[i] >> 8); out[i * 4 + 3] = (u8)st[i];
    }
    memset(block, 0, 64);
    memset(st, 0, sizeof st);
}

static void kc_hmac(const u8 *key, u32 klen, const u8 *msg, u32 mlen, u8 mac[32]) {
    u8 k[64], ipad[64], opad[64], inner[32];
    memset(k, 0, 64);
    if (klen > 64) kc_sha256(key, klen, k);
    else if (klen) memcpy(k, key, klen);
    for (int i = 0; i < 64; i++) { ipad[i] = (u8)(k[i] ^ 0x36); opad[i] = (u8)(k[i] ^ 0x5c); }
    u32 catn = 64 + (mlen > 32 ? mlen : 32);
    u8 *cat = (u8 *)malloc(catn);
    if (!cat) { memset(mac, 0, 32); memset(k, 0, 64); return; }
    memcpy(cat, ipad, 64);
    if (mlen) memcpy(cat + 64, msg, mlen);
    kc_sha256(cat, 64 + mlen, inner);
    memcpy(cat, opad, 64);
    memcpy(cat + 64, inner, 32);
    kc_sha256(cat, 96, mac);
    free(cat);
    memset(k, 0, 64);
    memset(inner, 0, 32);
}

static void kc_login(char *out, size_t n) {
    const char *u = getenv("USER");
    if (!u || !u[0]) u = getenv("LOGNAME");
    if (!u || !u[0]) {
        const char *h = getenv("HOME");
        if (h && h[0]) {
            const char *s = h;
            for (const char *p = h; *p; p++) if (*p == '/') s = p + 1;
            u = s[0] ? s : h;
        }
    }
    if (!u || !u[0]) u = "visitante";
    size_t i = 0;
    for (; u[i] && i + 1 < n; i++) out[i] = u[i];
    out[i] = 0;
}

/* HKDF enxuto: extract(sal fixo, nome de login) e um expand com info "file". */
static void kc_key_from(const char *login, u8 out[32]) {
    static const u8 salt[] = "mOS-keychain-v2";
    u8 prk[32];
    kc_hmac(salt, (u32)(sizeof salt - 1), (const u8 *)(login ? login : ""), login ? (u32)strlen(login) : 0, prk);
    kc_hmac(prk, 32, (const u8 *)"file", 4, out);
    memset(prk, 0, 32);
}

static void kc_subkeys(const u8 filekey[32], u8 enc[32], u8 mac[32]) {
    kc_hmac(filekey, 32, (const u8 *)"enc", 3, enc);
    kc_hmac(filekey, 32, (const u8 *)"mac", 3, mac);
}

static u32 kc_nonce_seq;
static void kc_nonce(u8 n[16]) {
    int fd = open("/dev/urandom", MAC_O_RDONLY);
    if (fd >= 0) {
        u32 got = 0;
        while (got < 16) {
            ssize_t k = read(fd, n + got, 16 - got);
            if (k <= 0) break;
            got += (u32)k;
        }
        close(fd);
        if (got == 16) return;
    }
    kc_nonce_seq++;
    unsigned long mix = (unsigned long)kc_nonce_seq * 0x9E3779B9ul ^ (unsigned long)(size_t)n;
    for (int i = 0; i < 16; i++) {
        mix = mix * 1664525ul + 1013904223ul;
        n[i] = (u8)(mix >> 16);
    }
}

static void kc_stream(const u8 enc[32], const u8 nonce[16], const u8 *in, u8 *out, u32 n) {
    u32 off = 0, ctr = 0;
    while (off < n) {
        u8 msg[20], block[32];
        memcpy(msg, nonce, 16);
        msg[16] = (u8)(ctr >> 24); msg[17] = (u8)(ctr >> 16); msg[18] = (u8)(ctr >> 8); msg[19] = (u8)ctr;
        kc_hmac(enc, 32, msg, 20, block);
        u32 take = n - off;
        if (take > 32) take = 32;
        for (u32 i = 0; i < take; i++) out[off + i] = (u8)(in[off + i] ^ block[i]);
        off += take;
        ctr++;
        memset(block, 0, 32);
    }
}

static int kc_mac_ok(const u8 *a, const u8 *b) {
    u8 d = 0;
    for (int i = 0; i < 32; i++) d |= (u8)(a[i] ^ b[i]);
    return d == 0;
}

/* 0 = ok. *out é o arquivo inteiro (cabeçalho + nonce + cifra + mac). */
static int kc_seal(const u8 filekey[32], const u8 *plain, u32 n, u8 **out, u32 *outn) {
    if (out) *out = 0;
    if (outn) *outn = 0;
    if (!filekey || !out || !plain) return -1;
    u8 enc[32], mac_key[32], nonce[16], mac[32];
    kc_subkeys(filekey, enc, mac_key);
    kc_nonce(nonce);
    u32 total = 15u + 16u + n + 32u;
    u8 *buf = (u8 *)malloc(total ? total : 1);
    if (!buf) { memset(enc, 0, 32); memset(mac_key, 0, 32); return -1; }
    memcpy(buf, "mOS-keychain 2\n", 15);
    memcpy(buf + 15, nonce, 16);
    kc_stream(enc, nonce, plain, buf + 31, n);
    u8 *mac_in = (u8 *)malloc((size_t)16 + n);
    if (!mac_in) { free(buf); memset(enc, 0, 32); memset(mac_key, 0, 32); return -1; }
    memcpy(mac_in, nonce, 16);
    if (n) memcpy(mac_in + 16, buf + 31, n);
    kc_hmac(mac_key, 32, mac_in, 16 + n, mac);
    free(mac_in);
    memcpy(buf + 31 + n, mac, 32);
    memset(enc, 0, 32);
    memset(mac_key, 0, 32);
    *out = buf;
    if (outn) *outn = total;
    return 0;
}

/* 0 = ok e *plain é o texto da versão 1. -1 = não é v2, MAC ruim ou sem memória. */
static int kc_open(const u8 filekey[32], const u8 *file, u32 n, u8 **plain, u32 *pn) {
    if (plain) *plain = 0;
    if (pn) *pn = 0;
    if (!filekey || !file || n < 15 + 16 + 32 || memcmp(file, "mOS-keychain 2\n", 15)) return -1;
    u32 body = n - 15 - 16 - 32;
    const u8 *nonce = file + 15;
    const u8 *ct = file + 31;
    const u8 *tag = file + 31 + body;
    u8 enc[32], mac_key[32], mac[32];
    kc_subkeys(filekey, enc, mac_key);
    u8 *mac_in = (u8 *)malloc((size_t)16 + body);
    if (!mac_in) { memset(enc, 0, 32); memset(mac_key, 0, 32); return -1; }
    memcpy(mac_in, nonce, 16);
    if (body) memcpy(mac_in + 16, ct, body);
    kc_hmac(mac_key, 32, mac_in, 16 + body, mac);
    free(mac_in);
    int ok = kc_mac_ok(mac, tag);
    memset(mac_key, 0, 32);
    if (!ok) { memset(enc, 0, 32); return -1; }
    u8 *p = (u8 *)malloc(body + 1);
    if (!p) { memset(enc, 0, 32); return -1; }
    kc_stream(enc, nonce, ct, p, body);
    p[body] = 0;
    memset(enc, 0, 32);
    *plain = p;
    if (pn) *pn = body;
    return 0;
}
