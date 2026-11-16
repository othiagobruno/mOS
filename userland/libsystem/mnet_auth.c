/* mOS — mnet: Basic e Digest (com MD5 e SHA-256 próprios, para valer igual no host e no libSystem). */
#include "mnet.h"

/* ---- MD5 (RFC 1321) */
static uint32_t rol(uint32_t x, int c) { return x << c | x >> (32 - c); }
void mnet_md5(const void *data, size_t n, uint8_t out[16]) {
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af,
        0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
        0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97,
        0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const int R[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
                              4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    uint32_t h[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    size_t total = ((n + 8) / 64 + 1) * 64;
    uint8_t *m = calloc(1, total);
    if (!m) { memset(out, 0, 16); return; }
    memcpy(m, data, n);
    m[n] = 0x80;
    uint64_t bits = (uint64_t)n * 8;
    for (int i = 0; i < 8; i++) m[total - 8 + i] = (uint8_t)(bits >> (8 * i));
    for (size_t off = 0; off < total; off += 64) {
        uint32_t w[16];
        for (int i = 0; i < 16; i++) w[i] = (uint32_t)m[off + 4 * i] | (uint32_t)m[off + 4 * i + 1] << 8 | (uint32_t)m[off + 4 * i + 2] << 16 | (uint32_t)m[off + 4 * i + 3] << 24;
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        for (int i = 0; i < 64; i++) {
            uint32_t f; int g;
            if (i < 16) { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
            else { f = c ^ (b | ~d); g = (7 * i) % 16; }
            uint32_t t = d; d = c; c = b;
            b = b + rol(a + f + K[i] + w[g], R[i]);
            a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    }
    free(m);
    for (int i = 0; i < 4; i++) for (int k = 0; k < 4; k++) out[4 * i + k] = (uint8_t)(h[i] >> (8 * k));
}
/* ---- SHA-256 (FIPS 180-4) */
static uint32_t ror(uint32_t x, int n) { return x >> n | x << (32 - n); }
void mnet_sha256(const void *data, size_t n, uint8_t out[32]) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
        0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
        0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
        0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    size_t total = ((n + 8) / 64 + 1) * 64;
    uint8_t *m = calloc(1, total);
    if (!m) { memset(out, 0, 32); return; }
    memcpy(m, data, n);
    m[n] = 0x80;
    uint64_t bits = (uint64_t)n * 8;
    for (int i = 0; i < 8; i++) m[total - 1 - i] = (uint8_t)(bits >> (8 * i));
    for (size_t off = 0; off < total; off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) w[i] = (uint32_t)m[off + 4 * i] << 24 | (uint32_t)m[off + 4 * i + 1] << 16 | (uint32_t)m[off + 4 * i + 2] << 8 | m[off + 4 * i + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25), ch = (e & f) ^ (~e & g), t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22), mj = (a & b) ^ (a & c) ^ (b & c), t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    free(m);
    for (int i = 0; i < 8; i++) { out[4 * i] = (uint8_t)(h[i] >> 24); out[4 * i + 1] = (uint8_t)(h[i] >> 16); out[4 * i + 2] = (uint8_t)(h[i] >> 8); out[4 * i + 3] = (uint8_t)h[i]; }
}

static int param(const char *s, const char *p, char *out, size_t on) {
    size_t pl = strlen(p);
    for (const char *q = s; *q; q++) {
        if ((q == s || q[-1] == ' ' || q[-1] == ',') && mnet_ieqn(q, p, pl) && q[pl] == '=') {
            q += pl + 1;
            size_t k = 0;
            if (*q == '"') { q++; while (*q && *q != '"' && k + 1 < on) { if (*q == '\\' && q[1]) q++; out[k++] = *q++; } }
            else while (*q && *q != ',' && *q != ' ' && k + 1 < on) out[k++] = *q++;
            out[k] = 0;
            return 1;
        }
    }
    return 0;
}
int mnet_auth_param(const char *challenge, const char *name, char *out, size_t n) {
    const char *sp = challenge ? strchr(challenge, ' ') : 0;
    return sp ? param(sp + 1, name, out, n) : 0;
}
const char *mnet_auth_pick(const mnet_hdrs_t *h, const char *field, int *score) {
    const char *best = 0; int sc0 = 0;
    for (int i = 0; i < h->n; i++) {
        if (!mnet_ieq(h->v[i].name, field)) continue;
        const char *v = h->v[i].value;
        int sc = 0;
        if (mnet_ieqn(v, "digest", 6)) { char a[32] = "MD5"; param(v + 6, "algorithm", a, sizeof a); sc = mnet_ieqn(a, "SHA-256", 7) ? 3 : mnet_ieqn(a, "MD5", 3) ? 2 : 0; }
        else if (mnet_ieqn(v, "basic", 5)) sc = 1;
        if (sc > sc0) { sc0 = sc; best = v; }
    }
    if (score) *score = sc0;
    return best;
}
static void hexs(const uint8_t *d, size_t n, char *o) { static const char h[] = "0123456789abcdef"; for (size_t i = 0; i < n; i++) { o[2 * i] = h[d[i] >> 4]; o[2 * i + 1] = h[d[i] & 15]; } o[2 * n] = 0; }
static void hashs(int sha, const char *s, char *out) {
    uint8_t md[32];
    if (sha) { mnet_sha256(s, strlen(s), md); hexs(md, 32, out); }
    else { mnet_md5(s, strlen(s), md); hexs(md, 16, out); }
}
char *mnet_auth_value(const char *ch, const char *user, const char *pass, const char *method, const char *uri, int nc, const uint8_t cn_raw[16]) {
    mnet_buf_t o = {0};
    if (!ch || !user) return 0;
    if (!pass) pass = "";
    if (mnet_ieqn(ch, "basic", 5)) {
        mnet_buf_t up = {0};
        mnet_buf_puts(&up, user); mnet_buf_putc(&up, ':'); mnet_buf_puts(&up, pass);
        char *b = malloc(up.n * 4 / 3 + 8);
        if (b) { mnet_b64(up.p, up.n, b); mnet_buf_puts(&o, "Basic "); mnet_buf_puts(&o, b); free(b); }
        mnet_buf_free(&up);
        return (char *)o.p;
    }
    if (!mnet_ieqn(ch, "digest", 6)) return 0;
    char realm[256] = "", nonce[256] = "", opaque[256] = "", qop[64] = "", alg[32] = "MD5";
    param(ch + 6, "realm", realm, sizeof realm);
    param(ch + 6, "nonce", nonce, sizeof nonce);
    int has_opaque = param(ch + 6, "opaque", opaque, sizeof opaque);
    int has_alg = param(ch + 6, "algorithm", alg, sizeof alg);
    param(ch + 6, "qop", qop, sizeof qop);
    int sha = mnet_ieqn(alg, "SHA-256", 7);
    int sess = mnet_ieqn(alg + (sha ? 7 : 3), "-sess", 5);
    if (!sha && !mnet_ieqn(alg, "MD5", 3)) return 0;
    char cnonce[33];
    uint8_t rnd[16];
    if (cn_raw) memcpy(rnd, cn_raw, 16); else for (int i = 0; i < 16; i++) rnd[i] = (uint8_t)(i * 73 + nc * 31 + (uintptr_t)&rnd);
    hexs(rnd, 16, cnonce);
    char ha1[65], ha2[65], resp[65], ncs[9];
    mnet_buf_t b = {0};
    mnet_buf_puts(&b, user); mnet_buf_putc(&b, ':'); mnet_buf_puts(&b, realm); mnet_buf_putc(&b, ':'); mnet_buf_puts(&b, pass);
    hashs(sha, (char *)b.p, ha1); b.n = 0;
    if (sess) { mnet_buf_puts(&b, ha1); mnet_buf_putc(&b, ':'); mnet_buf_puts(&b, nonce); mnet_buf_putc(&b, ':'); mnet_buf_puts(&b, cnonce); hashs(sha, (char *)b.p, ha1); b.n = 0; }
    mnet_buf_puts(&b, method); mnet_buf_putc(&b, ':'); mnet_buf_puts(&b, uri);
    hashs(sha, (char *)b.p, ha2); b.n = 0;
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++) ncs[i] = hx[(nc >> (28 - 4 * i)) & 15];
    ncs[8] = 0;
    int use_qop = qop[0] && mnet_tok_has(qop, "auth");
    mnet_buf_puts(&b, ha1); mnet_buf_putc(&b, ':'); mnet_buf_puts(&b, nonce); mnet_buf_putc(&b, ':');
    if (use_qop) { mnet_buf_puts(&b, ncs); mnet_buf_putc(&b, ':'); mnet_buf_puts(&b, cnonce); mnet_buf_puts(&b, ":auth:"); }
    mnet_buf_puts(&b, ha2);
    hashs(sha, (char *)b.p, resp);
    mnet_buf_free(&b);
    mnet_buf_puts(&o, "Digest username=\""); mnet_buf_puts(&o, user); mnet_buf_puts(&o, "\", realm=\""); mnet_buf_puts(&o, realm);
    mnet_buf_puts(&o, "\", nonce=\""); mnet_buf_puts(&o, nonce); mnet_buf_puts(&o, "\", uri=\""); mnet_buf_puts(&o, uri);
    if (has_alg) { mnet_buf_puts(&o, "\", algorithm="); mnet_buf_puts(&o, alg); mnet_buf_puts(&o, ", response=\""); }
    else mnet_buf_puts(&o, "\", response=\"");
    mnet_buf_puts(&o, resp); mnet_buf_putc(&o, '"');
    if (use_qop) { mnet_buf_puts(&o, ", qop=auth, nc="); mnet_buf_puts(&o, ncs); mnet_buf_puts(&o, ", cnonce=\""); mnet_buf_puts(&o, cnonce); mnet_buf_putc(&o, '"'); }
    if (has_opaque) { mnet_buf_puts(&o, ", opaque=\""); mnet_buf_puts(&o, opaque); mnet_buf_putc(&o, '"'); }
    return (char *)o.p;
}
