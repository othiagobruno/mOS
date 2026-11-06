/*
 * mOS — funções criptográficas do SMP (Bluetooth Core, Vol 3, Parte H, 2.2):
 *   e(k, x)  = AES-128 (bytes "MSB primeiro", como na especificação)
 *   c1       = confirmação do pareamento legado
 *   s1       = STK do pareamento legado
 *   ah       = hash de endereço aleatório resolvível
 * Os valores nos pacotes SMP vêm em little-endian; aqui tudo é convertido para a ordem da
 * especificação. Testado contra os exemplos da especificação em tools/bttest.c.
 */
#ifdef MOS_HOST_TEST
#include "host_shim.h"
#else
#include "kernel.h"
#endif
#include "wifi.h"   /* AES-128 próprio (wpa_aes_*) */

void smp_e(const u8 key[16], const u8 in[16], u8 out[16]) {
    wpa_aes_t c;
    wpa_aes_setkey(&c, key);
    wpa_aes_encrypt(&c, in, out);
}

static void rev(u8 *d, const u8 *s, int n) { for (int i = 0; i < n; i++) d[i] = s[n - 1 - i]; }

/* Todos os argumentos em little-endian (ordem dos pacotes); resultado também em little-endian. */
void smp_c1(const u8 k_le[16], const u8 r_le[16], const u8 preq[7], const u8 pres[7],
            u8 iat, const u8 ia_le[6], u8 rat, const u8 ra_le[6], u8 out_le[16]) {
    u8 k[16], r[16], p1[16], p2[16], t[16];
    rev(k, k_le, 16);
    rev(r, r_le, 16);
    /* p1 = pres || preq || rat' || iat' (MSB primeiro) */
    rev(p1, pres, 7);
    rev(p1 + 7, preq, 7);
    p1[14] = rat;
    p1[15] = iat;
    /* p2 = padding(4) || ia || ra */
    memset(p2, 0, 4);
    rev(p2 + 4, ia_le, 6);
    rev(p2 + 10, ra_le, 6);
    for (int i = 0; i < 16; i++) t[i] = r[i] ^ p1[i];
    smp_e(k, t, t);
    for (int i = 0; i < 16; i++) t[i] ^= p2[i];
    smp_e(k, t, t);
    rev(out_le, t, 16);
}

/* STK = e(k, r1' || r2'), r1' e r2' = 64 bits menos significativos de r1 e r2 */
void smp_s1(const u8 k_le[16], const u8 r1_le[16], const u8 r2_le[16], u8 out_le[16]) {
    u8 k[16], r[16], t[16];
    rev(k, k_le, 16);
    for (int i = 0; i < 8; i++) { r[i] = r1_le[7 - i]; r[8 + i] = r2_le[7 - i]; }
    smp_e(k, r, t);
    rev(out_le, t, 16);
}

/* ================================================================ LE Secure Connections
 * AES-CMAC (RFC 4493) e as funções f4, f5, f6 (Core, Vol 3, Parte H, 2.2.6–2.2.8).
 * Aqui os argumentos já estão na ordem da especificação (MSB primeiro); ble.c converte. */
static void cmac_subkeys(const u8 k[16], u8 k1[16], u8 k2[16]) {
    u8 z[16] = {0}, l[16];
    smp_e(k, z, l);
    for (int i = 0; i < 16; i++) k1[i] = (u8)(l[i] << 1 | (i < 15 ? l[i + 1] >> 7 : 0));
    if (l[0] & 0x80) k1[15] ^= 0x87;
    for (int i = 0; i < 16; i++) k2[i] = (u8)(k1[i] << 1 | (i < 15 ? k1[i + 1] >> 7 : 0));
    if (k1[0] & 0x80) k2[15] ^= 0x87;
}

void aes_cmac(const u8 key[16], const u8 *m, usize len, u8 out[16]) {
    u8 k1[16], k2[16], x[16] = {0}, last[16];
    cmac_subkeys(key, k1, k2);
    usize n = len ? (len + 15) / 16 : 1;
    bool full = len && len % 16 == 0;
    for (usize b = 0; b + 1 < n; b++) {
        for (int i = 0; i < 16; i++) x[i] ^= m[b * 16 + i];
        smp_e(key, x, x);
    }
    usize rem = len - (n - 1) * 16;
    memset(last, 0, 16);
    memcpy(last, m + (n - 1) * 16, rem);
    if (full) for (int i = 0; i < 16; i++) last[i] ^= k1[i];
    else { last[rem] = 0x80; for (int i = 0; i < 16; i++) last[i] ^= k2[i]; }
    for (int i = 0; i < 16; i++) x[i] ^= last[i];
    smp_e(key, x, out);
}

/* f4(U, V, X, Z) = CMAC_X(U || V || Z) — confirmação */
void smp_f4(const u8 u[32], const u8 v[32], const u8 x[16], u8 z, u8 out[16]) {
    u8 m[65];
    memcpy(m, u, 32); memcpy(m + 32, v, 32); m[64] = z;
    aes_cmac(x, m, 65, out);
}

/* f5(W, N1, N2, A1, A2) -> MacKey, LTK; A1/A2 = 7 bytes (tipo de endereço + endereço) */
void smp_f5(const u8 w[32], const u8 n1[16], const u8 n2[16], const u8 a1[7], const u8 a2[7], u8 mackey[16], u8 ltk[16]) {
    static const u8 salt[16] = {0x6C, 0x88, 0x83, 0x91, 0xAA, 0xF5, 0xA5, 0x38, 0x60, 0x37, 0x0B, 0xDB, 0x5A, 0x60, 0x83, 0xBE};
    u8 t[16], m[53];
    aes_cmac(salt, w, 32, t);
    m[0] = 0;
    m[1] = 0x62; m[2] = 0x74; m[3] = 0x6C; m[4] = 0x65;                 /* "btle" */
    memcpy(m + 5, n1, 16); memcpy(m + 21, n2, 16);
    memcpy(m + 37, a1, 7); memcpy(m + 44, a2, 7);
    m[51] = 0x01; m[52] = 0x00;                                         /* tamanho: 256 bits */
    aes_cmac(t, m, 53, mackey);
    m[0] = 1;
    aes_cmac(t, m, 53, ltk);
}

/* f6(W, N1, N2, R, IOcap, A1, A2) = CMAC_W(N1 || N2 || R || IOcap || A1 || A2) — DHKey check */
void smp_f6(const u8 w[16], const u8 n1[16], const u8 n2[16], const u8 r[16], const u8 io[3], const u8 a1[7], const u8 a2[7], u8 out[16]) {
    u8 m[65];
    memcpy(m, n1, 16); memcpy(m + 16, n2, 16); memcpy(m + 32, r, 16);
    memcpy(m + 48, io, 3); memcpy(m + 51, a1, 7); memcpy(m + 58, a2, 7);
    aes_cmac(w, m, 65, out);
}

/* ah(k, r) = e(k, padding || r) mod 2^24 (endereço aleatório resolvível) */
void smp_ah(const u8 irk_le[16], const u8 r_le[3], u8 out_le[3]) {
    u8 k[16], x[16] = {0}, t[16];
    rev(k, irk_le, 16);
    x[13] = r_le[2]; x[14] = r_le[1]; x[15] = r_le[0];
    smp_e(k, x, t);
    out_le[0] = t[15]; out_le[1] = t[14]; out_le[2] = t[13];
}
