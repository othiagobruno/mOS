/*
 * mOS — WPA3-Personal: SAE (Simultaneous Authentication of Equals, IEEE 802.11-2020 12.4) no grupo 19 (P-256).
 *
 * Duas formas de achar o elemento da senha (PWE):
 *   - "caçar e bicar" (hunting-and-pecking): HMAC-SHA256 com contador, 40 voltas sempre (tempo constante na senha);
 *   - H2E (hash-to-element): PT = SSWU(u1) + SSWU(u2) a partir de SSID e senha, PWE = val·PT.
 * Depois o commit (escalar = rand + mask, elemento = -mask·PWE), o segredo K = rand·(escalar·PWE + elemento)
 * do outro lado, KCK/PMK pelo KDF-SHA256 e os confirms com HMAC-SHA256.
 * Escrito para o mOS a partir do padrão; conferido com os vetores do anexo J.10 (tools/wifi_stack_host.c).
 */
#include "wifi.h"
#include "p256.h"
/* de kernel/net/crypto.c (o crypto.h não entra aqui: no teste de host ele traz outro shim) */
enum { HASH_SHA256 = 1 };
void hmac(int alg, const u8 *key, usize klen, const void *msg, usize mlen, u8 *out);
void hkdf_extract(int alg, const u8 *salt, usize slen, const u8 *ikm, usize ilen, u8 *prk);
void hkdf_expand(int alg, const u8 *prk, const u8 *info, usize ilen, u8 *out, usize olen);
void crypto_random(void *buf, usize len);
void secure_zero(void *p, usize n);
int  ct_memeq(const void *a, const void *b, usize n);

/* ---- inteiros de 256 bits em 8 palavras little-endian */
typedef struct { u32 v[8]; } n256;
static const n256 ORDER = {{0xFC632551, 0xF3B9CAC2, 0xA7179E84, 0xBCE6FAAD, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF}};

static void from_be(n256 *r, const u8 *b) { for (int i = 0; i < 8; i++) r->v[7 - i] = (u32)b[4 * i] << 24 | (u32)b[4 * i + 1] << 16 | (u32)b[4 * i + 2] << 8 | b[4 * i + 3]; }
static void to_be(u8 *b, const n256 *r) { for (int i = 0; i < 8; i++) { u32 w = r->v[7 - i]; b[4 * i] = (u8)(w >> 24); b[4 * i + 1] = (u8)(w >> 16); b[4 * i + 2] = (u8)(w >> 8); b[4 * i + 3] = (u8)w; } }
static int ncmp(const n256 *a, const n256 *b) { for (int i = 7; i >= 0; i--) if (a->v[i] != b->v[i]) return a->v[i] > b->v[i] ? 1 : -1; return 0; }
static u32 nadd(n256 *r, const n256 *a, const n256 *b) { u64 c = 0; for (int i = 0; i < 8; i++) { c += (u64)a->v[i] + b->v[i]; r->v[i] = (u32)c; c >>= 32; } return (u32)c; }
static u32 nsub(n256 *r, const n256 *a, const n256 *b) { i64 c = 0; for (int i = 0; i < 8; i++) { c += (i64)a->v[i] - b->v[i]; r->v[i] = (u32)c; c >>= 32; } return (u32)(c & 1); }
static void add_mod(n256 *r, const n256 *a, const n256 *b, const n256 *m) { if (nadd(r, a, b) || ncmp(r, m) >= 0) nsub(r, r, m); }
static void reduce_once(n256 *r, const n256 *m) { if (ncmp(r, m) >= 0) nsub(r, r, m); }

static const p256_fe *PP(void) { return p256_prime(); }
static void fe_from(p256_fe *f, const n256 *n) { memcpy(f->v, n->v, 32); }
static void fe_to(n256 *n, const p256_fe *f) { memcpy(n->v, f->v, 32); }
static void fe_set(p256_fe *f, u32 w) { memset(f, 0, sizeof *f); f->v[0] = w; }
static void fe_neg(p256_fe *r, const p256_fe *a) { p256_fe z; fe_set(&z, 0); p256_fe_sub(r, &z, a); }
static void fe_sel(p256_fe *r, const p256_fe *a, const p256_fe *b, u32 pick_a) {   /* r = pick_a ? a : b */
    u32 m = (u32)0 - (pick_a & 1);
    for (int i = 0; i < 8; i++) r->v[i] = (a->v[i] & m) | (b->v[i] & ~m);
}

/* expoentes (p-1)/2 e (p+1)/4 */
static void exp_legendre(u32 e[8]) { n256 p, one = {{1}}; fe_to(&p, PP()); nsub(&p, &p, &one); for (int i = 0; i < 8; i++) e[i] = p.v[i] >> 1 | (i < 7 ? p.v[i + 1] << 31 : 0); }
static void exp_sqrt(u32 e[8]) { n256 p, one = {{1}}; fe_to(&p, PP()); nadd(&p, &p, &one); for (int i = 0; i < 8; i++) e[i] = p.v[i] >> 2 | (i < 7 ? p.v[i + 1] << 30 : 0); }

/* y² = x³ - 3x + b */
static void rhs(p256_fe *r, const p256_fe *x) {
    p256_fe t, x3;
    p256_fe_mul(&x3, x, x); p256_fe_mul(&x3, &x3, x);
    p256_fe_add(&t, x, x); p256_fe_add(&t, &t, x);
    p256_fe_sub(r, &x3, &t); p256_fe_add(r, r, p256_b());
}
static u32 is_qr(const p256_fe *a) {                       /* 1 se a for resíduo quadrático (ou zero) */
    u32 e[8]; exp_legendre(e);
    p256_fe l, one; fe_set(&one, 1);
    p256_fe_pow(&l, a, e);
    return (u32)(p256_fe_cmp(&l, &one) == 0) | (u32)p256_fe_is_zero(a);
}
static void fe_sqrt(p256_fe *r, const p256_fe *a) { u32 e[8]; exp_sqrt(e); p256_fe_pow(r, a, e); }

/* KDF-SHA256 do 802.11 (12.7.1.6.2): HMAC(K, i || rótulo || contexto || bits) */
static void kdf256(const u8 *key, usize klen, const char *label, const u8 *ctx, usize clen, u8 *out, usize bits) {
    usize ll = strlen(label), n = (bits + 255) / 256;
    u8 *msg = kmalloc(2 + ll + clen + 2);
    if (!msg) return;
    for (usize i = 1; i <= n; i++) {
        msg[0] = (u8)i; msg[1] = (u8)(i >> 8);
        memcpy(msg + 2, label, ll);
        memcpy(msg + 2 + ll, ctx, clen);
        msg[2 + ll + clen] = (u8)bits; msg[2 + ll + clen + 1] = (u8)(bits >> 8);
        u8 h[32];
        hmac(HASH_SHA256, key, klen, msg, 2 + ll + clen + 2, h);
        usize take = bits / 8 - (i - 1) * 32;
        memcpy(out + (i - 1) * 32, h, take > 32 ? 32 : take);
    }
    kfree(msg);
}
void wpa_kdf_sha256(const u8 *key, usize klen, const char *label, const u8 *ctx, usize clen, u8 *out, usize bits) { kdf256(key, klen, label, ctx, clen, out, bits); }

static void macs_maxmin(u8 out[12], const u8 a[6], const u8 b[6]) {
    if (memcmp(a, b, 6) > 0) { memcpy(out, a, 6); memcpy(out + 6, b, 6); }
    else { memcpy(out, b, 6); memcpy(out + 6, a, 6); }
}

/* ============================================================ PWE: caçar e bicar */
bool sae_pwe_hnp(sae_t *s, const u8 own[6], const u8 peer[6], const char *pw) {
    u8 key[12], pbe[32];
    macs_maxmin(key, own, peer);
    to_be(pbe, (const n256 *)PP());
    usize pl = strlen(pw);
    u8 *msg = kmalloc(pl + 1);
    if (!msg) return false;
    memcpy(msg, pw, pl);
    p256_fe found_x = {{0}}, found_y2 = {{0}};
    u32 found = 0, odd = 0;
    for (int counter = 1; counter <= 40 || (!found && counter <= 255); counter++) {
        msg[pl] = (u8)counter;
        u8 seed[32], val[32];
        hmac(HASH_SHA256, key, 12, msg, pl + 1, seed);
        kdf256(seed, 32, "SAE Hunting and Pecking", pbe, 32, val, 256);
        n256 v; from_be(&v, val);
        u32 in_range = (u32)(ncmp(&v, (const n256 *)PP()) < 0);
        p256_fe x, y2;
        fe_from(&x, &v);
        if (!in_range) fe_set(&x, 0);
        rhs(&y2, &x);
        u32 ok = in_range & is_qr(&y2) & (u32)!p256_fe_is_zero(&y2);
        u32 take = ok & (u32)!found;
        fe_sel(&found_x, &x, &found_x, take);
        fe_sel(&found_y2, &y2, &found_y2, take);
        u32 m = (u32)0 - take;
        odd = (odd & ~m) | ((seed[31] & 1) & m);
        found |= ok;
    }
    secure_zero(msg, pl + 1);
    kfree(msg);
    if (!found) return false;
    p256_fe y, ny;
    fe_sqrt(&y, &found_y2);
    fe_neg(&ny, &y);
    fe_sel(&y, &y, &ny, (u32)((y.v[0] & 1) == odd));
    s->pwe_x = found_x; s->pwe_y = y;
    return p256_on_curve_fe(&s->pwe_x, &s->pwe_y);
}

/* ============================================================ PWE: H2E */
static void mod_p_48(p256_fe *r, const u8 v[48]) {           /* valor de 384 bits mod p */
    n256 hi = {{0}}, lo, R1;                                  /* R1 = 2^256 mod p = 2^256 - p */
    u8 hb[32] = {0};
    memcpy(hb + 16, v, 16);
    from_be(&hi, hb);
    from_be(&lo, v + 16);
    n256 zero = {{0}};
    nsub(&R1, &zero, (const n256 *)PP());
    reduce_once(&lo, (const n256 *)PP());
    p256_fe fh, fr1, fl, t;
    fe_from(&fh, &hi); fe_from(&fr1, &R1); fe_from(&fl, &lo);
    p256_fe_mul(&t, &fh, &fr1);
    p256_fe_add(r, &t, &fl);
}

static void sswu(p256_fe *ox, p256_fe *oy, const p256_fe *u) {
    p256_fe z, a, b = *p256_b(), u2, t1, t2, m, t, x1, gx1, x2, gx2, x, v, y, ny, one, tmp, tmp2;
    fe_set(&tmp, 10); fe_neg(&z, &tmp);                     /* z = -10 */
    fe_set(&tmp, 3); fe_neg(&a, &tmp);                      /* a = -3 */
    fe_set(&one, 1);
    p256_fe_mul(&u2, u, u);
    p256_fe_mul(&t1, &z, &u2);                               /* z·u² */
    p256_fe_mul(&t2, &t1, &t1);                              /* z²·u⁴ */
    p256_fe_add(&m, &t2, &t1);
    u32 l = (u32)p256_fe_is_zero(&m);
    p256_fe_inv(&t, &m);                                     /* m = 0 dá 0 */
    /* x1 = l ? b/(z·a) : (-b/a)·(1 + t) */
    p256_fe za, inv_za, bza, inv_a, nb, nba, opt;
    p256_fe_mul(&za, &z, &a); p256_fe_inv(&inv_za, &za); p256_fe_mul(&bza, &b, &inv_za);
    p256_fe_inv(&inv_a, &a); fe_neg(&nb, &b); p256_fe_mul(&nba, &nb, &inv_a);
    p256_fe_add(&opt, &one, &t); p256_fe_mul(&tmp2, &nba, &opt);
    fe_sel(&x1, &bza, &tmp2, l);
    rhs(&gx1, &x1);
    p256_fe_mul(&x2, &t1, &x1);
    rhs(&gx2, &x2);
    u32 q = is_qr(&gx1);
    fe_sel(&v, &gx1, &gx2, q);
    fe_sel(&x, &x1, &x2, q);
    fe_sqrt(&y, &v);
    fe_neg(&ny, &y);
    fe_sel(&y, &y, &ny, (u32)((u->v[0] & 1) == (y.v[0] & 1)));
    *ox = x; *oy = y;
}

bool sae_derive_pt(u8 pt[64], const u8 *ssid, usize ssid_len, const char *pw, const char *ident) {
    usize pl = strlen(pw), il = ident ? strlen(ident) : 0;
    u8 *ikm = kmalloc(pl + il + 1);
    if (!ikm) return false;
    memcpy(ikm, pw, pl);
    if (il) memcpy(ikm + pl, ident, il);
    u8 prk[32], v1[48], v2[48];
    hkdf_extract(HASH_SHA256, ssid, ssid_len, ikm, pl + il, prk);
    secure_zero(ikm, pl + il); kfree(ikm);
    static const char L1[] = "SAE Hash to Element u1 P1", L2[] = "SAE Hash to Element u2 P2";
    hkdf_expand(HASH_SHA256, prk, (const u8 *)L1, sizeof L1 - 1, v1, 48);
    hkdf_expand(HASH_SHA256, prk, (const u8 *)L2, sizeof L2 - 1, v2, 48);
    p256_fe u1, u2, p1x, p1y, p2x, p2y, x, y;
    mod_p_48(&u1, v1); mod_p_48(&u2, v2);
    sswu(&p1x, &p1y, &u1);
    sswu(&p2x, &p2y, &u2);
    if (!p256_pt_add(&x, &y, &p1x, &p1y, &p2x, &p2y)) return false;
    n256 nx, ny;
    fe_to(&nx, &x); fe_to(&ny, &y);
    to_be(pt, &nx); to_be(pt + 32, &ny);
    secure_zero(prk, sizeof prk);
    return true;
}

bool sae_pwe_h2e(sae_t *s, const u8 pt[64], const u8 own[6], const u8 peer[6]) {
    u8 mm[12], zeros[32] = {0}, h[32];
    macs_maxmin(mm, own, peer);
    hkdf_extract(HASH_SHA256, zeros, 32, mm, 12, h);
    n256 val, rm1 = ORDER, one = {{1}};
    nsub(&rm1, &rm1, &one);
    from_be(&val, h);
    reduce_once(&val, &rm1);                                 /* val mod (r-1): uma subtração basta */
    nadd(&val, &val, &one);
    n256 px, py;
    from_be(&px, pt); from_be(&py, pt + 32);
    p256_fe fx, fy;
    fe_from(&fx, &px); fe_from(&fy, &py);
    if (!p256_on_curve_fe(&fx, &fy)) return false;
    return p256_pt_mul(&s->pwe_x, &s->pwe_y, val.v, &fx, &fy);
}

/* ============================================================ commit e confirm */
static void rand_mod(n256 *r) {                              /* [2, r-1] */
    for (;;) {
        u8 b[32];
        crypto_random(b, 32);
        from_be(r, b);
        n256 two = {{2}};
        if (ncmp(r, &ORDER) < 0 && ncmp(r, &two) >= 0) return;
    }
}

bool sae_commit(sae_t *s, const u8 *rand_be, const u8 *mask_be) {
    n256 rnd, msk, sc;
    for (int tries = 0; tries < 8; tries++) {
        if (rand_be) from_be(&rnd, rand_be); else rand_mod(&rnd);
        if (mask_be) from_be(&msk, mask_be); else rand_mod(&msk);
        add_mod(&sc, &rnd, &msk, &ORDER);
        n256 one = {{1}};
        if (ncmp(&sc, &one) > 0) break;
        if (rand_be) return false;
    }
    memcpy(s->rand, rnd.v, 32);
    to_be(s->scalar, &sc);
    p256_fe ex, ey, ny;
    if (!p256_pt_mul(&ex, &ey, msk.v, &s->pwe_x, &s->pwe_y)) return false;
    fe_neg(&ny, &ey);                                         /* elemento = -(mask·PWE) */
    n256 a, b;
    fe_to(&a, &ex); fe_to(&b, &ny);
    to_be(s->elem, &a); to_be(s->elem + 32, &b);
    secure_zero(&msk, sizeof msk);
    s->send_confirm = 0;
    return true;
}

/* commit do outro lado: confere e deriva KCK, PMK e PMKID. false = commit inválido ou refletido */
bool sae_process_commit(sae_t *s, const u8 *peer_scalar, const u8 *peer_elem) {
    n256 ps, one = {{1}};
    from_be(&ps, peer_scalar);
    if (ncmp(&ps, &one) <= 0 || ncmp(&ps, &ORDER) >= 0) return false;
    if (!memcmp(peer_scalar, s->scalar, 32) && !memcmp(peer_elem, s->elem, 64)) return false;   /* reflexão */
    n256 ex, ey;
    from_be(&ex, peer_elem); from_be(&ey, peer_elem + 32);
    p256_fe fx, fy;
    fe_from(&fx, &ex); fe_from(&fy, &ey);
    if (!p256_on_curve_fe(&fx, &fy)) return false;
    memcpy(s->peer_scalar, peer_scalar, 32);
    memcpy(s->peer_elem, peer_elem, 64);
    /* K = rand·(peer_scalar·PWE + peer_elem) */
    p256_fe sx, sy, tx, ty, kx, ky;
    if (!p256_pt_mul(&sx, &sy, ps.v, &s->pwe_x, &s->pwe_y)) return false;
    if (!p256_pt_add(&tx, &ty, &sx, &sy, &fx, &fy)) return false;
    if (!p256_pt_mul(&kx, &ky, s->rand, &tx, &ty)) return false;
    u8 k[32], zeros[32] = {0}, keyseed[32], ctx[32], out[64];
    n256 nk; fe_to(&nk, &kx); to_be(k, &nk);
    hmac(HASH_SHA256, zeros, 32, k, 32, keyseed);
    n256 own, sum;
    from_be(&own, s->scalar);
    add_mod(&sum, &own, &ps, &ORDER);
    to_be(ctx, &sum);
    kdf256(keyseed, 32, "SAE KCK and PMK", ctx, 32, out, 512);
    memcpy(s->kck, out, 32);
    memcpy(s->pmk, out + 32, 32);
    memcpy(s->pmkid, ctx, 16);
    secure_zero(k, 32); secure_zero(keyseed, 32); secure_zero(out, 64);
    return true;
}

static void confirm_mac(const sae_t *s, u16 sc, bool ours, u8 out[32]) {
    u8 m[2 + 32 + 64 + 32 + 64];
    m[0] = (u8)sc; m[1] = (u8)(sc >> 8);
    const u8 *a_s = ours ? s->scalar : s->peer_scalar, *a_e = ours ? s->elem : s->peer_elem;
    const u8 *b_s = ours ? s->peer_scalar : s->scalar, *b_e = ours ? s->peer_elem : s->elem;
    memcpy(m + 2, a_s, 32); memcpy(m + 34, a_e, 64); memcpy(m + 98, b_s, 32); memcpy(m + 130, b_e, 64);
    hmac(HASH_SHA256, s->kck, 32, m, sizeof m, out);
}

u16 sae_confirm(sae_t *s, u8 out[32]) {
    s->send_confirm++;
    confirm_mac(s, s->send_confirm, true, out);
    return s->send_confirm;
}

bool sae_check_confirm(const sae_t *s, u16 peer_sc, const u8 conf[32]) {
    u8 want[32];
    confirm_mac(s, peer_sc, false, want);
    return ct_memeq(want, conf, 32);
}
