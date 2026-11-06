/* mOS — teste no host da criptografia do WPA2 (kernel/wifi/wpa.c) contra vetores padrão. */
#include "wifi.h"

long long g_live_allocs, g_live_bytes;
static int fails;

static void hexs(const u8 *b, int n, char *o) { for (int i = 0; i < n; i++) sprintf(o + i * 2, "%02x", b[i]); }
static void unhex(const char *s, u8 *o) { for (int i = 0; s[i * 2]; i++) { unsigned v; sscanf(s + i * 2, "%2x", &v); o[i] = (u8)v; } }
static void check(const char *what, const u8 *got, int n, const char *want) {
    char h[512];
    hexs(got, n, h);
    if (strcmp(h, want)) { fails++; printf("FALHOU %s\n  obtido   %s\n  esperado %s\n", what, h, want); }
    else printf("ok  %s\n", what);
}

int main(void) {
    u8 out[64];
    /* SHA-1 (RFC 3174) */
    wpa_sha1("abc", 3, out);
    check("SHA-1 \"abc\"", out, 20, "a9993e364706816aba3e25717850c26c9cd0d89d");
    wpa_sha1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, out);
    check("SHA-1 2 blocos", out, 20, "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    /* HMAC-SHA1 (RFC 2202, casos 1 e 2) */
    u8 k20[20];
    memset(k20, 0x0b, 20);
    wpa_hmac_sha1(k20, 20, "Hi There", 8, out);
    check("HMAC-SHA1 RFC 2202 #1", out, 20, "b617318655057264e28bc0b6fb378c8ef146be00");
    wpa_hmac_sha1((const u8 *)"Jefe", 4, "what do ya want for nothing?", 28, out);
    check("HMAC-SHA1 RFC 2202 #2", out, 20, "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");
    /* PBKDF2 do 802.11i (H.4.1): senha "password", SSID "IEEE" */
    wpa_psk("password", (const u8 *)"IEEE", 4, out);
    check("PSK 802.11i \"password\"/\"IEEE\"", out, 32, "f42c6fc52df0ebef9ebb4b90b38a5f902e83fe1b135a70e23aed762e9710a12e");
    wpa_psk("ThisIsAPassword", (const u8 *)"ThisIsASSID", 11, out);
    check("PSK 802.11i \"ThisIsAPassword\"", out, 32, "0dc0d6eb90555ed6419756b9a15ec3e3209b63df707dd508d14581f8982721af");
    /* PRF-SHA1 (802.11i H.3, caso 1): chave 0x0b*20, "prefix", "Hi There", 64 bytes */
    wpa_prf(k20, 20, "prefix", (const u8 *)"Hi There", 8, out, 64);
    check("PRF-512 802.11i H.3 #1", out, 64,
          "bcd4c650b30b9684951829e0d75f9d54b862175ed9f00606e17d8da35402ffee75df78c3d31e0f889f012120c0862beb67753e7439ae242edb8373698356cf5a");
    /* AES Key Wrap (RFC 3394 4.1) */
    u8 kek[16], key[16], wrapped[24];
    unhex("000102030405060708090a0b0c0d0e0f", kek);
    unhex("00112233445566778899aabbccddeeff", key);
    wpa_key_wrap(kek, key, 16, wrapped);
    check("AES Key Wrap RFC 3394", wrapped, 24, "1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5");
    u8 back[16];
    bool ok = wpa_key_unwrap(kek, wrapped, 24, back);
    check("AES Key Unwrap RFC 3394", back, 16, "00112233445566778899aabbccddeeff");
    wrapped[3] ^= 1;
    if (wpa_key_unwrap(kek, wrapped, 24, back) || !ok) { fails++; printf("FALHOU unwrap deveria rejeitar dado adulterado\n"); }
    else printf("ok  unwrap rejeita dado adulterado\n");
    /* AES-128 (FIPS-197 C.1) */
    wpa_aes_t a;
    u8 pt[16], ct[16];
    unhex("000102030405060708090a0b0c0d0e0f", key);
    unhex("00112233445566778899aabbccddeeff", pt);
    wpa_aes_setkey(&a, key);
    wpa_aes_encrypt(&a, pt, ct);
    check("AES-128 FIPS-197", ct, 16, "69c4e0d86a7b0430d8cdb78070b4c55a");
    wpa_aes_decrypt(&a, ct, pt);
    check("AES-128 decifra", pt, 16, "00112233445566778899aabbccddeeff");
    /* AES-CCM (RFC 3610, Packet Vector #1: M=8, L=2) */
    u8 ck[16], nonce[13], aad[8], msg[23], mic[8];
    unhex("c0c1c2c3c4c5c6c7c8c9cacbcccdcecf", ck);
    unhex("00000003020100a0a1a2a3a4a5", nonce);
    unhex("0001020304050607", aad);
    unhex("08090a0b0c0d0e0f101112131415161718191a1b1c1d1e", msg);
    wpa_ccm_encrypt(ck, nonce, aad, 8, msg, 23, mic);
    check("AES-CCM RFC 3610 #1 (cifra)", msg, 23, "588c979a61c663d2f066d0c2c0f989806d5f6b61dac384");
    check("AES-CCM RFC 3610 #1 (MIC)", mic, 8, "17e8d12cfdf926e0");
    ok = wpa_ccm_decrypt(ck, nonce, aad, 8, msg, 23, mic);
    check("AES-CCM RFC 3610 #1 (decifra)", msg, 23, "08090a0b0c0d0e0f101112131415161718191a1b1c1d1e");
    msg[0] ^= 1;
    if (!ok || wpa_ccm_decrypt(ck, nonce, aad, 8, msg, 23, mic)) { fails++; printf("FALHOU CCM deveria rejeitar dado adulterado\n"); }
    else printf("ok  CCM rejeita dado adulterado\n");
    printf(fails ? "%d FALHA(S)\n" : "TUDO OK\n", fails);
    return fails != 0;
}
