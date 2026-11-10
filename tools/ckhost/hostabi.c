/* Símbolos C que o CryptoKit.swift chama, para o teste de host. As primitivas são as mesmas
 * do libSystem: kernel/net/crypto.c (AES-GCM, ChaCha20-Poly1305, CSPRNG). */
#define MOS_HOST_TEST 1
#include "crypto.c"

int mos_ck_aes_gcm_seal(const u8 *key, size_t klen, const u8 nonce[12], const u8 *aad, size_t alen, const u8 *in, size_t n, u8 *out, u8 tag[16]) {
    if (klen != 16 && klen != 24 && klen != 32) return -2;
    aes_gcm_ctx c;
    aes_gcm_init(&c, key, (u32)klen);
    aes_gcm_seal(&c, nonce, aad, alen, in, n, out, tag);
    secure_zero(&c, sizeof c);
    return 0;
}
int mos_ck_aes_gcm_open(const u8 *key, size_t klen, const u8 nonce[12], const u8 *aad, size_t alen, const u8 *in, size_t n, const u8 tag[16], u8 *out) {
    if (klen != 16 && klen != 24 && klen != 32) return -2;
    aes_gcm_ctx c;
    aes_gcm_init(&c, key, (u32)klen);
    int r = aes_gcm_open(&c, nonce, aad, alen, in, n, out, tag);
    secure_zero(&c, sizeof c);
    return r == 0 ? 0 : -1;
}
int mos_ck_chachapoly_seal(const u8 key[32], const u8 nonce[12], const u8 *aad, size_t alen, const u8 *in, size_t n, u8 *out, u8 tag[16]) {
    chacha20poly1305_seal(key, nonce, aad, alen, in, n, out, tag);
    return 0;
}
int mos_ck_chachapoly_open(const u8 key[32], const u8 nonce[12], const u8 *aad, size_t alen, const u8 *in, size_t n, const u8 tag[16], u8 *out) {
    return chacha20poly1305_open(key, nonce, aad, alen, in, n, out, tag) == 0 ? 0 : -1;
}
void mos_ck_random(void *buf, size_t n) { crypto_random(buf, n); }
