/* Vetores conhecidos das primitivas que o CryptoKit.framework chama (kernel/net/crypto.c e o
 * MD5/SHA-256 do CommonCrypto). Não é o boot do mOS: compila no host.
 *   cc -O2 -DMOS_HOST_TEST -Itools/ckhost -Ikernel/include -Ikernel/net tools/ckvec.c tools/ckhost/hostabi.c -o build/ckvec
 */
#define MOS_HOST_TEST 1
#include "crypto.h"

#include <stdio.h>
#include <string.h>

int mos_ck_aes_gcm_seal(const u8 *key, size_t klen, const u8 nonce[12], const u8 *aad, size_t alen, const u8 *in, size_t n, u8 *out, u8 tag[16]);
int mos_ck_aes_gcm_open(const u8 *key, size_t klen, const u8 nonce[12], const u8 *aad, size_t alen, const u8 *in, size_t n, const u8 tag[16], u8 *out);
int mos_ck_chachapoly_seal(const u8 key[32], const u8 nonce[12], const u8 *aad, size_t alen, const u8 *in, size_t n, u8 *out, u8 tag[16]);
int mos_ck_chachapoly_open(const u8 key[32], const u8 nonce[12], const u8 *aad, size_t alen, const u8 *in, size_t n, const u8 tag[16], u8 *out);

static int fails;

static int hexeq(const u8 *b, size_t n, const char *hex) {
    for (size_t i = 0; i < n; i++) {
        int hi = hex[2 * i], lo = hex[2 * i + 1];
        int v = ((hi <= '9' ? hi - '0' : hi - 'a' + 10) << 4) | (lo <= '9' ? lo - '0' : lo - 'a' + 10);
        if (b[i] != (u8)v) return 0;
    }
    return 1;
}
static void unhex(u8 *b, const char *hex) {
    for (size_t i = 0; hex[2 * i]; i++) {
        int hi = hex[2 * i], lo = hex[2 * i + 1];
        b[i] = (u8)(((hi <= '9' ? hi - '0' : hi - 'a' + 10) << 4) | (lo <= '9' ? lo - '0' : lo - 'a' + 10));
    }
}
static void check(const char *name, int ok) {
    printf("%-28s %s\n", name, ok ? "ok" : "FALHOU");
    if (!ok) fails++;
}

static void test_sha(void) {
    u8 o[32], mac[32], okm[42];
    sha256("abc", 3, o);
    check("SHA-256(\"abc\")", hexeq(o, 32, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    sha256("", 0, o);
    check("SHA-256(\"\")", hexeq(o, 32, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    /* RFC 4231 caso 1 */
    u8 k[20];
    memset(k, 0x0b, 20);
    hmac(HASH_SHA256, k, 20, "Hi There", 8, mac);
    check("HMAC-SHA256 RFC 4231", hexeq(mac, 32, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"));
    /* RFC 5869 A.1 */
    u8 ikm[22], salt[13], info[10], prk[32];
    memset(ikm, 0x0b, 22);
    unhex(salt, "000102030405060708090a0b0c");
    unhex(info, "f0f1f2f3f4f5f6f7f8f9");
    hkdf_extract(HASH_SHA256, salt, 13, ikm, 22, prk);
    check("HKDF-SHA256 PRK RFC 5869", hexeq(prk, 32, "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5"));
    hkdf_expand(HASH_SHA256, prk, info, 10, okm, 42);
    check("HKDF-SHA256 OKM RFC 5869", hexeq(okm, 42, "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865"));
}

static void test_gcm(void) {
    u8 z[32] = {0}, tag[16], back[8];
    /* NIST SP 800-38D, chave e IV zero, texto vazio */
    mos_ck_aes_gcm_seal(z, 16, z, 0, 0, 0, 0, 0, tag);
    check("AES-128-GCM tag NIST", hexeq(tag, 16, "58e2fccefa7e3061367f1d57a4e7455a"));
    check("AES-128-GCM open", mos_ck_aes_gcm_open(z, 16, z, 0, 0, 0, 0, tag, back) == 0);
    tag[0] ^= 1;
    check("AES-128-GCM recusa tag", mos_ck_aes_gcm_open(z, 16, z, 0, 0, 0, 0, tag, back) != 0);
    mos_ck_aes_gcm_seal(z, 24, z, 0, 0, 0, 0, 0, tag);
    check("AES-192-GCM tag NIST", hexeq(tag, 16, "cd33b28ac773f74ba00ed1f312572435"));
    mos_ck_aes_gcm_seal(z, 32, z, 0, 0, 0, 0, 0, tag);
    check("AES-256-GCM tag NIST", hexeq(tag, 16, "530f8afbc74536b9a963b4f1c4cb738b"));
    check("AES chave de 15 bytes", mos_ck_aes_gcm_seal(z, 15, z, 0, 0, 0, 0, 0, tag) != 0);
}

static void test_chacha(void) {
    /* RFC 8439 §2.8.2 */
    u8 key[32], nonce[12], aad[12], pt[114], ct[114], tag[16], back[114];
    unhex(key, "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    unhex(nonce, "070000004041424344454647");
    unhex(aad, "50515253c0c1c2c3c4c5c6c7");
    /* "Ladies and Gentlemen ... sunscreen would be it." — RFC 8439 §2.8.2, 114 bytes. */
    unhex(pt, "4c616469657320616e642047656e746c656d656e206f662074686520636c617373206f66202739393a204966204920636f756c64206f6666657220796f75206f6e6c79206f6e652074697020666f7220746865206675747572652c2073756e73637265656e20776f756c642062652069742e");
    /* RFC 8439 §2.3.2: bloco com contador 1 */
    u8 zkey[32], znonce[12], blk[64];
    for (int i = 0; i < 32; i++) zkey[i] = (u8)i;
    memset(znonce, 0, 12); znonce[3] = 0x09; znonce[7] = 0x4a;
    chacha20_block(zkey, 1, znonce, blk);
    check("ChaCha20 bloco RFC 8439", hexeq(blk, 64, "10f1e7e4d13b5915500fdd1fa32071c4c7d1f4c733c068030422aa9ac3d46c4ed2826446079faa0914c2d705d98b02a2b5129cd1de164eb9cbd083e8a2503c4e"));
    mos_ck_chachapoly_seal(key, nonce, aad, 12, pt, 114, ct, tag);
    /* RFC 8439 §2.8.2, texto e tag publicados (keystream termina em e4 ab e6 22 15 38). */
    check("ChaChaPoly RFC 8439 texto", hexeq(ct, 114, "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d26586cec64b6116"));
    check("ChaChaPoly RFC 8439 tag", hexeq(tag, 16, "1ae10b594f09e26a7e902ecbd0600691"));
    int ok = mos_ck_chachapoly_open(key, nonce, aad, 12, ct, 114, tag, back) == 0 && memcmp(back, pt, 114) == 0;
    ct[0] ^= 1;
    ok = ok && mos_ck_chachapoly_open(key, nonce, aad, 12, ct, 114, tag, back) != 0;
    check("ChaChaPoly abre e recusa", ok);
}

int main(void) {
    test_sha();
    test_gcm();
    test_chacha();
    printf("%s\n", fails ? "FALHAS" : "TUDO OK");
    return fails ? 1 : 0;
}
