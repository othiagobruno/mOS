/*
 * mOS — teste no host da criptografia e do cliente TLS 1.3/1.2 (kernel/net).
 *   zig cc -target x86_64-windows-gnu -O2 -DMOS_HOST_TEST -Ikernel/include -Itools ^
 *          tools/tlstest.c kernel/net/crypto.c kernel/net/tls.c kernel/net/pk.c kernel/net/x509.c ^
 *          -lws2_32 -o build/tlstest.exe
 *   (rodar a partir da raiz do repositório: lê assets/certs/cacert.pem e tools/testdata/)
 *   tlstest            vetores (SHA-2, HMAC, HKDF, X25519, ChaCha20-Poly1305, AES-GCM,
 *                      RFC 8448, PRF do TLS 1.2, ECDH P-256/P-384, RSA PKCS#1/PSS, ECDSA
 *                      P-256/P-384, X.509, nomes, cadeias, ServerHello forjado por um
 *                      servidor local: sentinela de downgrade, versão antiga)
 *   tlstest live [host ...]     + handshake real via Winsock COM validação de certificado
 *                                 (sem hosts: sites 1.3, key_share secp256r1 e badssl.com só-1.2)
 *   tlstest live12 [host ...]   + TLS 1.2 forçado: cada cifra x grupo
 *   tlstest neg                 + servidores que DEVEM ser rejeitados (badssl.com, SNI/IP errado)
 *   tlstest capture host ...    salva a cadeia recebida em tools/testdata/<host>.chain
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "crypto.h"
#include "x509.h"

static int fails, passes;

static int unhex(const char *s, u8 *out) {
    int n = 0, hi = -1;
    for (; *s; s++) {
        int v;
        if (*s >= '0' && *s <= '9') v = *s - '0';
        else if (*s >= 'a' && *s <= 'f') v = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') v = *s - 'A' + 10;
        else continue;
        if (hi < 0) hi = v;
        else { out[n++] = (u8)(hi << 4 | v); hi = -1; }
    }
    return n;
}

static void check(const char *name, const u8 *got, const char *hex) {
    u8 exp[512];
    int n = unhex(hex, exp);
    if (memcmp(got, exp, n) == 0) { passes++; printf("  ok    %s\n", name); return; }
    fails++;
    printf("  FALHA %s\n    obtido:   ", name);
    for (int i = 0; i < n; i++) printf("%02x", got[i]);
    printf("\n    esperado: %s\n", hex);
}

static void check_true(const char *name, int cond) {
    if (cond) { passes++; printf("  ok    %s\n", name); }
    else { fails++; printf("  FALHA %s\n", name); }
}

u64 timer_ticks(void) { return GetTickCount64(); }

/* relógio: UTC do Windows, ou uma data forçada pelos testes offline */
static bool g_fake_clock;
static datetime_t g_fake_now;
void rtc_now(datetime_t *dt) {
    if (g_fake_clock) { *dt = g_fake_now; return; }
    SYSTEMTIME st;
    GetSystemTime(&st);
    dt->year = st.wYear; dt->month = st.wMonth; dt->day = st.wDay;
    dt->hour = st.wHour; dt->minute = st.wMinute; dt->second = st.wSecond; dt->weekday = st.wDayOfWeek;
}

static u8 *read_file(const char *path, u32 *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *b = malloc((size_t)n + 1);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    if (b) { b[n] = 0; *len = (u32)n; }
    return b;
}

/* ---------------------------------------------------------------- vetores */
static void test_sha(void) {
    printf("SHA-2\n");
    u8 o[64];
    sha256("abc", 3, o);
    check("SHA-256(\"abc\")", o, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const char *m2 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    sha256(m2, strlen(m2), o);
    check("SHA-256(448 bits)", o, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    sha256_ctx c;
    sha256_init(&c);
    u8 a[1000];
    memset(a, 'a', sizeof(a));
    for (u32 left = 1000000, i = 0; left; i++) {     /* pedaços de tamanhos variados */
        u32 k = (i * 37) % 1000 + 1;
        if (k > left) k = left;
        sha256_update(&c, a, k);
        left -= k;
    }
    sha256_final(&c, o);
    check("SHA-256(10^6 x 'a')", o, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    hash_buf(HASH_SHA384, "abc", 3, o);
    check("SHA-384(\"abc\")", o, "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7");
    sha512_ctx c5;
    sha384_init(&c5);
    for (int i = 0; i < 1000; i++) sha512_update(&c5, a, 1000);
    sha512_final(&c5, o);
    check("SHA-384(10^6 x 'a')", o, "9d0e1809716474cb086e834e310a4a1ced149e9c00f248527972cec5704c2a5b07b8b3dc38ecc4ebae97ddd87f3d8985");
}

static void test_hmac_hkdf(void) {
    printf("HMAC (RFC 4231) / HKDF (RFC 5869)\n");
    u8 key[131], o[64];
    memset(key, 0x0b, 20);
    hmac(HASH_SHA256, key, 20, "Hi There", 8, o);
    check("HMAC-SHA256 caso 1", o, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    hmac(HASH_SHA384, key, 20, "Hi There", 8, o);
    check("HMAC-SHA384 caso 1", o, "afd03944d84895626b0825f4ab46907f15f9dadbe4101ec682aa034c7cebc59cfaea9ea9076ede7f4af152e8b2fa9cb6");
    const char *d2 = "what do ya want for nothing?";
    hmac(HASH_SHA256, (const u8 *)"Jefe", 4, d2, strlen(d2), o);
    check("HMAC-SHA256 caso 2", o, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    memset(key, 0xaa, 131);
    const char *d6 = "Test Using Larger Than Block-Size Key - Hash Key First";
    hmac(HASH_SHA256, key, 131, d6, strlen(d6), o);
    check("HMAC-SHA256 caso 6 (chave longa)", o, "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");

    u8 ikm[22], salt[13], info[10], prk[32], okm[42];
    memset(ikm, 0x0b, 22);
    for (int i = 0; i < 13; i++) salt[i] = (u8)i;
    for (int i = 0; i < 10; i++) info[i] = (u8)(0xf0 + i);
    hkdf_extract(HASH_SHA256, salt, 13, ikm, 22, prk);
    check("HKDF-Extract caso 1 (PRK)", prk, "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5");
    hkdf_expand(HASH_SHA256, prk, info, 10, okm, 42);
    check("HKDF-Expand caso 1 (OKM)", okm, "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865");
}

static void test_x25519(void) {
    printf("X25519 (RFC 7748)\n");
    u8 k[32], u[32], o[32];
    unhex("a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4", k);
    unhex("e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c", u);
    x25519(o, k, u);
    check("§5.2 vetor 1", o, "c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552");

    u8 kk[32] = {9}, uu[32] = {9}, r[32];
    for (int i = 1; i <= 1000; i++) {
        x25519(r, kk, uu);
        memcpy(uu, kk, 32);
        memcpy(kk, r, 32);
        if (i == 1) check("§5.2 iterado 1x", kk, "422c8e7a6227d7bca1350b3e2bb7279f7897b87bb6854b783c60e80311ae3079");
    }
    check("§5.2 iterado 1000x", kk, "684cf59ba83309552800ef566f2f4d3c1c3887c49360e3875f2eb94d99532c51");

    u8 ap[32], bp[32], apub[32], bpub[32], s1[32], s2[32];
    unhex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a", ap);
    unhex("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb", bp);
    x25519_base(apub, ap);
    x25519_base(bpub, bp);
    check("§6.1 chave pública de Alice", apub, "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
    check("§6.1 chave pública de Bob", bpub, "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f");
    x25519(s1, ap, bpub);
    x25519(s2, bp, apub);
    check("§6.1 segredo (Alice)", s1, "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
    check("§6.1 segredo (Bob)", s2, "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
}

static void test_chacha(void) {
    printf("ChaCha20-Poly1305 (RFC 8439)\n");
    u8 pk[32], tag[16];
    unhex("85d6be7857556d337f4452fe42d506a80103808afb0db2fd4abff6af4149f51b", pk);
    poly1305_ctx p;
    poly1305_init(&p, pk);
    const char *m = "Cryptographic Forum Research Group";
    poly1305_update(&p, (const u8 *)m, 10);
    poly1305_update(&p, (const u8 *)m + 10, strlen(m) - 10);
    poly1305_final(&p, tag);
    check("§2.5.2 Poly1305", tag, "a8061dc1305136c6c22b8baf0c0127a9");

    u8 key[32], nonce[12], aad[12], ct[128], pt[128];
    for (int i = 0; i < 32; i++) key[i] = (u8)(0x80 + i);
    unhex("070000004041424344454647", nonce);
    unhex("50515253c0c1c2c3c4c5c6c7", aad);
    const char *s = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for "
                    "the future, sunscreen would be it.";
    u32 n = (u32)strlen(s);
    chacha20poly1305_seal(key, nonce, aad, 12, (const u8 *)s, n, ct, tag);
    check("§2.8.2 texto cifrado", ct,
          "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b"
          "1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc"
          "3ff4def08e4b7a9de576d26586cec64b6116");
    check("§2.8.2 tag", tag, "1ae10b594f09e26a7e902ecbd0600691");
    check_true("§2.8.2 abrir (decifra e confere)",
               chacha20poly1305_open(key, nonce, aad, 12, ct, n, pt, tag) == 0 && memcmp(pt, s, n) == 0);
    ct[5] ^= 1;
    check_true("§2.8.2 rejeita texto adulterado", chacha20poly1305_open(key, nonce, aad, 12, ct, n, pt, tag) != 0);
}

static void test_aes(void) {
    printf("AES / AES-GCM (FIPS-197, McGrew-Viega)\n");
    u8 k[32], in[64], out[64], tag[16], iv[12], aad[20];
    aes_ctx a;
    unhex("000102030405060708090a0b0c0d0e0f", k);
    unhex("00112233445566778899aabbccddeeff", in);
    aes_setkey(&a, k, 16);
    aes_encrypt_block(&a, in, out);
    check("FIPS-197 AES-128", out, "69c4e0d86a7b0430d8cdb78070b4c55a");
    unhex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", k);
    aes_setkey(&a, k, 32);
    aes_encrypt_block(&a, in, out);
    check("FIPS-197 AES-256", out, "8ea2b7ca516745bfeafc49904b496089");

    aes_gcm_ctx g;
    memset(k, 0, 32); memset(in, 0, 16); memset(iv, 0, 12);
    aes_gcm_init(&g, k, 16);
    aes_gcm_seal(&g, iv, NULL, 0, in, 16, out, tag);
    check("GCM caso 2 C", out, "0388dace60b6a392f328c2b971b2fe78");
    check("GCM caso 2 T", tag, "ab6e47d42cec13bdf53a67b21257bddf");

    unhex("feffe9928665731c6d6a8f9467308308", k);
    unhex("cafebabefacedbaddecaf888", iv);
    unhex("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525"
          "b16aedf5aa0de657ba637b39", in);
    unhex("feedfacedeadbeeffeedfacedeadbeefabaddad2", aad);
    aes_gcm_init(&g, k, 16);
    aes_gcm_seal(&g, iv, aad, 20, in, 60, out, tag);
    check("GCM caso 4 C", out,
          "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa05"
          "1ba30b396a0aac973d58e091");
    check("GCM caso 4 T", tag, "5bc94fbc3221a5db94fae95ae7121a47");
    u8 back[64];
    check_true("GCM caso 4 abrir", aes_gcm_open(&g, iv, aad, 20, out, 60, back, tag) == 0 && !memcmp(back, in, 60));
    tag[0] ^= 1;
    check_true("GCM caso 4 rejeita tag adulterada", aes_gcm_open(&g, iv, aad, 20, out, 60, back, tag) != 0);

    memset(k, 0, 32); memset(in, 0, 16); memset(iv, 0, 12);
    aes_gcm_init(&g, k, 32);
    aes_gcm_seal(&g, iv, NULL, 0, in, 0, out, tag);
    check("GCM caso 13 (AES-256) T", tag, "530f8afbc74536b9a963b4f1c4cb738b");
    aes_gcm_seal(&g, iv, NULL, 0, in, 16, out, tag);
    check("GCM caso 14 (AES-256) C", out, "cea7403d4d606b6e074ec5d3baf39d18");
    check("GCM caso 14 (AES-256) T", tag, "d0d1c8a799996bf0265b98b5d48ab919");
}

static void test_rfc8448(void) {
    printf("TLS 1.3 escalonamento de chaves (RFC 8448 §3, Simple 1-RTT)\n");
    u8 cpriv[32], spriv[32], pub[32], spub[32], shared[32];
    unhex("49af42ba7f7994852d713ef2784bcbcaa7911de26adc5642cb634540e7ea5005", cpriv);
    unhex("b1580eeadf6dd589b8ef4f2d5652578cc810e9980191ec8d058308cea216a21e", spriv);
    x25519_base(pub, cpriv);
    check("chave pública x25519 do cliente", pub, "99381de560e4bd43d23d8e435a7dbafeb3c06e51c13cae4d5413691e529aaf2c");
    x25519_base(spub, spriv);
    check("chave pública x25519 do servidor", spub, "c9828876112095fe66762bdbf7c672e156d6cc253b833df1dd69b1b04e751f0f");
    x25519(shared, cpriv, spub);
    check("segredo ECDHE", shared, "8bd4054fb55b9d63fdfbacf9f04b9f0d35e6d63f537563efd46272900f89492d");

    u8 th_hs[32], th_ap[32], hs[32], chs[32], shs[32], ms[32], cap[32], sap[32], key[16], iv[12];
    unhex("860c06edc07858ee8e78f0e7428c58edd6b43f2ca3e6e95f02ed063cf0e1cad8", th_hs);
    unhex("9608102a0f1ccc6db6250b7b7e417b1a000eaada3daae4777a7686c9ff83df13", th_ap);
    tls13_derive_handshake(HASH_SHA256, shared, 32, th_hs, hs, chs, shs, ms);
    check("handshake secret", hs, "1dc826e93606aa6fdc0aadc12f741b01046aa6b99f691ed221a9f0ca043fbeac");
    check("c hs traffic", chs, "b3eddb126e067f35a780b3abf45e2d8f3b1a950738f52e9600746a0e27a55a21");
    check("s hs traffic", shs, "b67b7d690cc16c4e75e54213cb2d37b4e9c912bcded9105d42befd59d391ad38");
    check("master secret", ms, "18df06843d13a08bf2a449844c5f8a478001bc4d4c627984d5a41da8d0402919");
    tls13_traffic_keys(HASH_SHA256, shs, 16, key, iv);
    check("chave handshake do servidor", key, "3fce516009c21727d0f2e4e86ee403bc");
    check("IV handshake do servidor", iv, "5d313eb2671276ee13000b30");
    tls13_traffic_keys(HASH_SHA256, chs, 16, key, iv);
    check("chave handshake do cliente", key, "dbfaa693d1762c5b666af5d950258d01");
    check("IV handshake do cliente", iv, "5bd3c71b836e0b76bb73265f");
    tls13_derive_app(HASH_SHA256, ms, th_ap, cap, sap);
    check("c ap traffic", cap, "9e40646ce79a7f9dc05af8889bce6552875afa0b06df0087f792ebb7c17504a5");
    check("s ap traffic", sap, "a11af9f05531f856ad47116b45a950328204b4f44bfb6b3a4b4f1f3fcb631643");
    tls13_traffic_keys(HASH_SHA256, sap, 16, key, iv);
    check("chave aplicação do servidor", key, "9f02283b6c9c07efc26bb9f2ac92e356");
    check("IV aplicação do servidor", iv, "cf782b88dd83549aadf1e984");
    tls13_traffic_keys(HASH_SHA256, cap, 16, key, iv);
    check("chave aplicação do cliente", key, "17422dda596ed5d9acd890e3c63f5051");
    check("IV aplicação do cliente", iv, "5b78923dee08579033e523d9");
}

static void test_tls12_prf(void) {
    printf("TLS 1.2 PRF (P_SHA256 / P_SHA384) e ECDH P-256\n");
    u8 secret[16], seed[16], o[148];
    unhex("9bbe436ba940f017b17652849a71db35", secret);
    unhex("a0ba9f936cda311827a6f796ffd5198c", seed);
    tls12_prf(HASH_SHA256, secret, 16, "test label", seed, 16, o, 100);
    check("PRF-SHA256 (100 bytes)", o,
          "e3f229ba727be17b8d122620557cd453c2aab21d07c3d495329b52d4e61edb5a6b301791e90d35c9c9a46b4e14baf9af"
          "0fa022f7077def17abfd3797c0564bab4fbc91666e9def9b97fce34f796789baa48082d122ee42c5a72e5a5110fff701"
          "87347b66");
    unhex("b80b733d6ceefcdc71566ea48e5567df", secret);
    unhex("cd665cf6a8447dd6ff8b27555edb7465", seed);
    tls12_prf(HASH_SHA384, secret, 16, "test label", seed, 16, o, 148);
    check("PRF-SHA384 (148 bytes)", o,
          "7b0c18e9ced410ed1804f2cfa34a336a1c14dffb4900bb5fd7942107e81c83cde9ca0faa60be9fe34f82b1233c9146a0"
          "e534cb400fed2700884f9dc236f80edd8bfa961144c9e8d792eca722a7b32fc3d416d473ebc2c5fd4abfdad05d918425"
          "9b5bf8cd4d90fa0d31e2dec479e4f1a26066f2eea9a69236a3e52655c9e9aee691c8f3a26854308d5eaa3be85e0990703d73e56f");

    /* RFC 5903 §8.1 (ECDH 256-bit random group) */
    u8 i[32], r[32], gi[65], gr[65], s1[32], s2[32];
    unhex("c88f01f510d9ac3f70a292daa2316de544e9aab8afe84049c62a9c57862d1433", i);
    unhex("04dad0b65394221cf9b051e1feca5787d098dfe637fc90b9ef945d0c37725811805271a0461cdb8252d61f1c456fa3e59ab1f45b33accf5f58389e0577b8990bb3", gi);
    unhex("c6ef9c5d78ae012a011164acb397ce2088685d8f06bf9be0b283ab46476bee53", r);
    unhex("04d12dfb5289c8d4f81208b70270398c342296970a0bccb74c736fc7554494bf6356fbf3ca366cc23e8157854c13c58d6aac23f046ada30f8353e74f33039872ab", gr);
    check_true("RFC 5903 ECDH i·gr", ecdh_p256(s1, i, gr, 65) == 0);
    check("RFC 5903 segredo (i·gr)", s1, "d6840f6b42f6edafd13116e0e12565202fef8e9ece7dce03812464d04b9442de");
    check_true("RFC 5903 ECDH r·gi", ecdh_p256(s2, r, gi, 65) == 0);
    check("RFC 5903 segredo (r·gi)", s2, "d6840f6b42f6edafd13116e0e12565202fef8e9ece7dce03812464d04b9442de");
    u8 a[32], b[32], pa[65], pb[65];
    check_true("keygen P-256 (dois pares)", ecdh_p256_keygen(a, pa) == 0 && ecdh_p256_keygen(b, pb) == 0);
    check_true("acordo P-256 com chaves geradas", ecdh_p256(s1, a, pb, 65) == 0 && ecdh_p256(s2, b, pa, 65) == 0 &&
                                                  !memcmp(s1, s2, 32));
    pb[64] ^= 1;
    check_true("ECDH P-256 rejeita ponto fora da curva", ecdh_p256(s1, a, pb, 65) != 0);
    pb[64] ^= 1;
    pb[0] = 2;
    check_true("ECDH P-256 rejeita ponto comprimido", ecdh_p256(s1, a, pb, 65) != 0);
    u8 a3[48], b3[48], pa3[97], pb3[97], t1[48], t2[48];
    check_true("keygen P-384 (dois pares)", ecdh_keygen(EC_P384, a3, pa3) == 0 && ecdh_keygen(EC_P384, b3, pb3) == 0);
    check_true("acordo P-384 com chaves geradas", ecdh_shared(EC_P384, t1, a3, pb3, 97) == 0 &&
                                                  ecdh_shared(EC_P384, t2, b3, pa3, 97) == 0 && !memcmp(t1, t2, 48));
    pb3[96] ^= 1;
    check_true("ECDH P-384 rejeita ponto fora da curva", ecdh_shared(EC_P384, t1, a3, pb3, 97) != 0);
}

static void test_rng(void) {
    printf("CSPRNG\n");
    u8 a[64], b[64];
    crypto_random(a, sizeof(a));
    crypto_random(b, sizeof(b));
    int zero = 1;
    for (int i = 0; i < 64; i++) if (a[i]) zero = 0;
    check_true("saídas distintas e não nulas", !zero && memcmp(a, b, 64) != 0);
}

/* ------------------------------------------------- chave pública / X.509 */
#include "tlstest_vectors.h"

static const char PK_MSG[] = "mOS teste de assinatura";

static void rsa_case(const char *name, const char *nh, const char *eh, int pss, int halg, const char *sigh) {
    static u8 n[1100], e[8], sig[1100], h[64];
    int nl = unhex(nh, n), el = unhex(eh, e), sl = unhex(sigh, sig);
    hash_buf(halg, PK_MSG, strlen(PK_MSG), h);
    u64 t0 = rdtsc();
    int r = pss ? rsa_verify_pss(n, nl, e, el, halg, h, sig, sl, (int)hash_len(halg))
                : rsa_verify_pkcs1(n, nl, e, el, halg, h, sig, sl);
    u64 cyc = rdtsc() - t0;
    char nm[128];
    snprintf(nm, sizeof(nm), "%s válida (%.2f Mciclos)", name, cyc / 1e6);
    check_true(nm, r == 0);
    sig[sl / 2] ^= 0x10;
    int r2 = pss ? rsa_verify_pss(n, nl, e, el, halg, h, sig, sl, -1) : rsa_verify_pkcs1(n, nl, e, el, halg, h, sig, sl);
    sig[sl / 2] ^= 0x10;
    h[3] ^= 1;
    int r3 = pss ? rsa_verify_pss(n, nl, e, el, halg, h, sig, sl, -1) : rsa_verify_pkcs1(n, nl, e, el, halg, h, sig, sl);
    snprintf(nm, sizeof(nm), "%s rejeita assinatura/mensagem adulterada", name);
    check_true(nm, r2 != 0 && r3 != 0);
}

static void ec_case(const char *name, int curve, const char *pubh, int halg, const char *msg, u32 mlen,
                    const char *rh, const char *sh) {
    static u8 pub[200], r[64], s[64], h[64];
    int pl = unhex(pubh, pub), rl = unhex(rh, r), sl = unhex(sh, s);
    hash_buf(halg, msg, mlen, h);
    u32 hl = hash_len(halg);
    u64 t0 = GetTickCount64(), c0 = rdtsc();
    int ok = ecdsa_verify(curve, pub, pl, h, hl, r, rl, s, sl);
    u64 cyc = rdtsc() - c0;
    (void)t0;
    char nm[128];
    snprintf(nm, sizeof(nm), "%s válida (%.2f Mciclos)", name, cyc / 1e6);
    check_true(nm, ok == 0);
    s[sl - 1] ^= 1;
    int bad1 = ecdsa_verify(curve, pub, pl, h, hl, r, rl, s, sl);
    s[sl - 1] ^= 1;
    h[0] ^= 0x80;
    int bad2 = ecdsa_verify(curve, pub, pl, h, hl, r, rl, s, sl);
    h[0] ^= 0x80;
    pub[pl - 1] ^= 1;                                   /* ponto fora da curva */
    int bad3 = ecdsa_verify(curve, pub, pl, h, hl, r, rl, s, sl);
    pub[pl - 1] ^= 1;
    snprintf(nm, sizeof(nm), "%s rejeita s/hash adulterados e ponto fora da curva", name);
    check_true(nm, bad1 && bad2 && bad3);
}

/* assinatura P1363 (r||s) em hex -> r, s */
static void ec_cng(const char *name, int curve, const char *pub, int halg, const char *sig) {
    u32 half = (u32)strlen(sig) / 2;
    char r[200], s[200];
    memcpy(r, sig, half); r[half] = 0;
    strcpy(s, sig + half);
    ec_case(name, curve, pub, halg, PK_MSG, (u32)strlen(PK_MSG), r, s);
}

static void test_pk(void) {
    printf("SHA-512 (FIPS 180-2)\n");
    u8 o[64];
    hash_buf(HASH_SHA512, "abc", 3, o);
    check("SHA-512(\"abc\")", o, "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
    hmac(HASH_SHA512, (const u8 *)"Jefe", 4, "what do ya want for nothing?", 28, o);
    check("HMAC-SHA512 (RFC 4231 caso 2)", o, "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737");

    printf("RSA PKCS#1 v1.5 / PSS (vetores Windows CNG, 2048/3072/4096/8192 bits)\n");
#define RSA_SET(B) \
    rsa_case("RSA-" #B " PKCS#1 SHA-256", RSA##B##_N, RSA##B##_E, 0, HASH_SHA256, RSA##B##_PKCS1_SHA256); \
    rsa_case("RSA-" #B " PKCS#1 SHA-384", RSA##B##_N, RSA##B##_E, 0, HASH_SHA384, RSA##B##_PKCS1_SHA384); \
    rsa_case("RSA-" #B " PKCS#1 SHA-512", RSA##B##_N, RSA##B##_E, 0, HASH_SHA512, RSA##B##_PKCS1_SHA512); \
    rsa_case("RSA-" #B " PSS SHA-256", RSA##B##_N, RSA##B##_E, 1, HASH_SHA256, RSA##B##_PSS_SHA256); \
    rsa_case("RSA-" #B " PSS SHA-384", RSA##B##_N, RSA##B##_E, 1, HASH_SHA384, RSA##B##_PSS_SHA384); \
    rsa_case("RSA-" #B " PSS SHA-512", RSA##B##_N, RSA##B##_E, 1, HASH_SHA512, RSA##B##_PSS_SHA512);
    RSA_SET(2048)
    RSA_SET(3072)
    RSA_SET(4096)
    RSA_SET(8192)
    {   /* PKCS#1 com hash trocado (assinado SHA-256, verificado como SHA-512) deve falhar */
        static u8 n[600], e[8], sig[600], h[64];
        int nl = unhex(RSA2048_N, n), el = unhex(RSA2048_E, e), sl = unhex(RSA2048_PKCS1_SHA256, sig);
        hash_buf(HASH_SHA512, PK_MSG, strlen(PK_MSG), h);
        check_true("RSA PKCS#1 rejeita DigestInfo de outro hash", rsa_verify_pkcs1(n, nl, e, el, HASH_SHA512, h, sig, sl) != 0);
    }

    printf("ECDSA P-256 / P-384\n");
    check_true("P-256: G na curva e n·G = infinito", ec_selftest(EC_P256) == 0);
    check_true("P-384: G na curva e n·G = infinito", ec_selftest(EC_P384) == 0);
    /* RFC 6979 A.2.5 / A.2.6, mensagem "sample" */
    ec_case("RFC 6979 P-256/SHA-256 \"sample\"", EC_P256,
            "0460fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb67903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299",
            HASH_SHA256, "sample", 6,
            "efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf3716",
            "f7cb1c942d657c41d436c7a1b6e29f65f3e900dbb9aff4064dc4ab2f843acda8");
    ec_case("RFC 6979 P-384/SHA-384 \"sample\"", EC_P384,
            "04ec3a4e415b4e19a4568618029f427fa5da9a8bc4ae92e02e06aae5286b300c64def8f0ea9055866064a254515480bc13"
            "8015d9b72d7d57244ea8ef9ac0c621896708a59367f9dfb9f54ca84b3f1c9db1288b231c3ae0d4fe7344fd2533264720",
            HASH_SHA384, "sample", 6,
            "94edbb92a5ecb8aad4736e56c691916b3f88140666ce9fa73d64c4ea95ad133c81a648152e44acf96e36dd1e80fabe46",
            "99ef4aeb15f178cea1fe40db2603138f130e740a19624526203b6351d0a3a94fa329c145786e679e7b82c71a38628ac8");
    ec_cng("CNG P-256/SHA-256", EC_P256, EC256_PUB, HASH_SHA256, EC256_SIG_SHA256);
    ec_cng("CNG P-256/SHA-384", EC_P256, EC256_PUB, HASH_SHA384, EC256_SIG_SHA384);
    ec_cng("CNG P-256/SHA-512", EC_P256, EC256_PUB, HASH_SHA512, EC256_SIG_SHA512);
    ec_cng("CNG P-384/SHA-256", EC_P384, EC384_PUB, HASH_SHA256, EC384_SIG_SHA256);
    ec_cng("CNG P-384/SHA-384", EC_P384, EC384_PUB, HASH_SHA384, EC384_SIG_SHA384);
    ec_cng("CNG P-384/SHA-512", EC_P384, EC384_PUB, HASH_SHA512, EC384_SIG_SHA512);
}

/* --- X.509: nomes e cadeias capturadas (tools/testdata/<host>.chain) --- */
static void name_case(const x509_cert_t *c, const char *host, int expect) {
    char nm[128];
    snprintf(nm, sizeof(nm), "nome %-22s %s", host, expect ? "aceito" : "recusado");
    check_true(nm, x509_match_host(c, host) == expect);
}

/* .chain = sequência de (u24 comprimento big-endian || DER), folha primeiro */
static int load_chain(const char *path, const u8 **ders, u32 *lens, u32 max, u8 **buf) {
    u32 len = 0;
    u8 *b = read_file(path, &len);
    if (!b) return -1;
    *buf = b;
    u32 n = 0, off = 0;
    while (off + 3 <= len && n < max) {
        u32 l = (u32)b[off] << 16 | (u32)b[off + 1] << 8 | b[off + 2];
        off += 3;
        if (off + l > len) break;
        ders[n] = b + off; lens[n] = l; n++;
        off += l;
    }
    return (int)n;
}

static void chain_case(const char *label, const u8 *const *ders, const u32 *lens, u32 n, const char *host,
                       const datetime_t *now, const char *expect) {
    char err[192], anchor[96], nm[256];
    u64 c0 = rdtsc();
    int e = x509_verify_chain(ders, lens, n, host, now, err, sizeof(err), anchor, sizeof(anchor));
    double mc = (rdtsc() - c0) / 1e6;
    bool ok = expect ? (e != 0 && strstr(err, expect)) : e == 0;
    snprintf(nm, sizeof(nm), "%s: %s (%.1f Mciclos)", label, e ? err : anchor, mc);
    check_true(nm, ok);
}

static void test_chain_file(const char *path, const char *host) {
    const u8 *ders[10];
    u32 lens[10];
    u8 *buf = NULL;
    int n = load_chain(path, ders, lens, 10, &buf);
    if (n <= 0) { printf("  (sem %s — rode 'tlstest capture %s' para gerar)\n", path, host); return; }
    x509_cert_t leaf;
    if (x509_parse(&leaf, ders[0], lens[0]) != X509_OK) { check_true("parse da folha", 0); free(buf); return; }
    char cn[96], icn[96];
    x509_name_cn(leaf.subject, leaf.subject_len, cn, sizeof(cn));
    x509_name_cn(leaf.issuer, leaf.issuer_len, icn, sizeof(icn));
    printf("  %s: %d certs, folha CN=%s emissor=%s, chave %s, válida %llu..%llu\n", path, n, cn, icn,
           leaf.key_type == X509_KEY_RSA ? "RSA" : leaf.curve == EC_P256 ? "P-256" : "P-384",
           (unsigned long long)leaf.not_before, (unsigned long long)leaf.not_after);
    for (int i = 1; i < n; i++) {
        x509_cert_t c;
        char nm[160];
        int e = x509_parse(&c, ders[i], lens[i]);
        x509_name_cn(c.subject, c.subject_len, cn, sizeof(cn));
        snprintf(nm, sizeof(nm), "parse do intermediário %d (%s, CA=%d)", i, e ? "?" : cn, c.ca);
        check_true(nm, e == 0 && c.has_bc && c.ca);
    }
    /* datas: um dia depois do início da validade da folha */
    datetime_t t = {0};
    u64 nb = leaf.not_before / 1000000;              /* AAAAMMDD */
    t.year = (int)(nb / 10000); t.month = (int)(nb / 100 % 100); t.day = (int)(nb % 100) + 1;
    if (t.day > 28) { t.day = 1; t.month++; if (t.month > 12) { t.month = 1; t.year++; } }
    t.hour = 12;
    chain_case("cadeia válida", ders, lens, n, host, &t, NULL);
    chain_case("cadeia sem checar datas", ders, lens, n, host, NULL, NULL);
    datetime_t late = t; late.year += 30;
    chain_case("relógio 30 anos à frente", ders, lens, n, host, &late, "expirado");
    datetime_t early = t; early.year -= 1;
    chain_case("relógio 1 ano atrás", ders, lens, n, host, &early, "ainda não é válido");
    chain_case("nome errado", ders, lens, n, "mos-teste.invalid", &t, "não corresponde a mos-teste.invalid");
    if (n > 1) chain_case("sem intermediários", ders, lens, 1, host, &t, "emissor desconhecido");
    if (n > 2) {   /* ordem embaralhada */
        const u8 *d2[10]; u32 l2[10];
        d2[0] = ders[0]; l2[0] = lens[0];
        for (int i = 1; i < n; i++) { d2[i] = ders[n - i]; l2[i] = lens[n - i]; }
        chain_case("intermediários em ordem invertida", d2, l2, n, host, &t, NULL);
    }
    /* adultera um byte da assinatura da folha (últimos bytes do DER) */
    u8 *cp = malloc(lens[0]);
    memcpy(cp, ders[0], lens[0]);
    cp[lens[0] - 5] ^= 0x01;
    const u8 *d3[10]; u32 l3[10];
    memcpy(d3, ders, sizeof(d3)); memcpy(l3, lens, sizeof(l3));
    d3[0] = cp;
    chain_case("assinatura da folha adulterada", d3, l3, n, host, &t, "assinatura inválida");
    /* adultera o tbsCertificate (um byte do número de série) */
    memcpy(cp, ders[0], lens[0]);
    cp[(leaf.tbs - ders[0]) + 12] ^= 0x01;
    chain_case("tbsCertificate adulterado (nº de série)", d3, l3, n, host, &t, "assinatura inválida");
    free(cp);
    free(buf);
}

static void test_x509(void) {
    printf("X.509: nome do host (SAN, curingas, IPs)\n");
    /* SAN sintético: *.example.com, example.com, f*.bad.com, *.com, 192.0.2.7, 2001:db8::1 */
    static const u8 san[] = {
        0x82, 13, '*', '.', 'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm',
        0x82, 11, 'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm',
        0x82, 10, 'f', '*', '.', 'b', 'a', 'd', '.', 'c', 'o', 'm',
        0x82, 5, '*', '.', 'c', 'o', 'm',
        0x87, 4, 192, 0, 2, 7,
        0x87, 16, 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    };
    x509_cert_t c;
    memset(&c, 0, sizeof(c));
    c.san = san;
    c.san_len = sizeof(san);
    name_case(&c, "www.example.com", 1);
    name_case(&c, "WWW.Example.COM", 1);
    name_case(&c, "example.com", 1);
    name_case(&c, "example.com.", 1);
    name_case(&c, "a.b.example.com", 0);
    name_case(&c, ".example.com", 0);
    name_case(&c, "foo.bad.com", 0);
    name_case(&c, "f.bad.com", 0);
    name_case(&c, "other.com", 0);
    name_case(&c, "example.org", 0);
    name_case(&c, "192.0.2.7", 1);
    name_case(&c, "192.0.2.8", 0);
    name_case(&c, "2001:db8::1", 1);
    name_case(&c, "2001:db8::2", 0);
    printf("X.509: cadeias reais capturadas (tools/testdata)\n");
    test_chain_file("tools/testdata/example.com.chain", "example.com");
    test_chain_file("tools/testdata/www.google.com.chain", "www.google.com");
    test_chain_file("tools/testdata/github.com.chain", "github.com");
    test_chain_file("tools/testdata/wikipedia.org.chain", "wikipedia.org");
}

/* ------------------------------------------------------ TCP via Winsock */
extern void (*tls_test_chain_hook)(const char *host, const u8 *const *ders, const u32 *lens, u32 count);
static const char *g_capture_path;
static void capture_hook(const char *host, const u8 *const *ders, const u32 *lens, u32 count) {
    FILE *f = fopen(g_capture_path, "wb");
    if (!f) return;
    for (u32 i = 0; i < count; i++) {
        u8 h[3] = {(u8)(lens[i] >> 16), (u8)(lens[i] >> 8), (u8)lens[i]};
        fwrite(h, 1, 3, f);
        fwrite(ders[i], 1, lens[i], f);
    }
    fclose(f);
    printf("  cadeia de %s (%u certificados) salva em %s\n", host, count, g_capture_path);
}

static SOCKET socks[8];
static int nsocks;

int tcp_state(int s) { return s >= 0 && s < nsocks && socks[s] != INVALID_SOCKET ? TCP_OPEN : TCP_ERROR; }

int tcp_send(int s, const void *data, int len) {
    int n = send(socks[s], data, len, 0);
    if (n == SOCKET_ERROR) return WSAGetLastError() == WSAEWOULDBLOCK ? 0 : -1;
    return n;
}

int tcp_recv(int s, void *buf, int max) {
    /* lê em pedaços pequenos e aleatórios para exercitar a remontagem de registros */
    int lim = 1 + (int)(rdtsc() % 3000);
    if (lim < max) max = lim;
    int n = recv(socks[s], buf, max, 0);
    if (n == 0) return -1;
    if (n == SOCKET_ERROR) return WSAGetLastError() == WSAEWOULDBLOCK ? 0 : -1;
    return n;
}

static int host_connect(const char *host) {
    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, "443", &hints, &res) != 0) return -1;
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connect(s, res->ai_addr, (int)res->ai_addrlen) != 0) { freeaddrinfo(res); closesocket(s); return -1; }
    freeaddrinfo(res);
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
    for (int i = 0; i < nsocks; i++)
        if (socks[i] == INVALID_SOCKET) { socks[i] = s; return i; }
    if (nsocks == (int)ARRAY_LEN(socks)) { closesocket(s); return -1; }
    socks[nsocks] = s;
    return nsocks++;
}

static const char *suite_name(u16 s) {
    return s == 0x1301 ? "TLS_AES_128_GCM_SHA256" : s == 0x1302 ? "TLS_AES_256_GCM_SHA384" : "TLS_CHACHA20_POLY1305_SHA256";
}

/* teste negativo: conecta em `addr` com nome `sni` e exige falha de handshake cujo
 * texto contenha `expect` */
static const u16 DEF13[4] = {0x1303, 0x1301, 0x1302, 0};
static const u16 DEF12[7] = {0xcca9, 0xcca8, 0xc02b, 0xc02f, 0xc02c, 0xc030, 0};
static const u16 DEFGRP[4] = {0x001d, 0x0017, 0x0018, 0};

/* volta às preferências padrão de tls.c */
static void prefs_default(void) {
    memcpy(tls_cipher_prefs, DEF13, sizeof(DEF13));
    memcpy(tls12_cipher_prefs, DEF12, sizeof(DEF12));
    memcpy(tls_group_prefs, DEFGRP, sizeof(DEFGRP));
    tls_versions = TLS_V12 | TLS_V13;
}

static void live_reject(const char *addr, const char *sni, const char *expect) {
    prefs_default();
    printf("  %-26s nome=%-26s ", addr, sni);
    fflush(stdout);
    int s = host_connect(addr);
    if (s < 0) { printf("FALHA (conexão TCP)\n"); fails++; return; }
    tls_conn_t *c = tls_new(s, sni);
    u64 t0 = GetTickCount64();
    int r;
    while ((r = tls_handshake(c)) == 0) {
        if (GetTickCount64() - t0 > 15000) break;
        Sleep(1);
    }
    const char *err = r < 0 ? tls_error(c) : r == 0 ? "tempo esgotado" : "(handshake ACEITO)";
    bool ok = r < 0 && strstr(err, expect) != NULL;
    printf("%s rejeitado: \"%s\"\n", ok ? "ok   " : "FALHA", err);
    if (ok) passes++; else fails++;
    tls_free(c);
    closesocket(socks[s]);
    socks[s] = INVALID_SOCKET;
}

/* handshake + GET / real. label descreve a configuração; skip_unsup: se o servidor
 * recusar a cifra/grupo forçado (alerta handshake_failure), conta como "não suportado"
 * em vez de falha. */
static void live_run(const char *host, const char *label, bool skip_unsup) {
    printf("  %-22s %-44s ", host, label);
    fflush(stdout);
    int s = host_connect(host);
    if (s < 0) { printf("FALHA (conexão TCP)\n"); fails++; return; }
    tls_conn_t *c = tls_new(s, host);
    u64 t0 = GetTickCount64();
    int r;
    while ((r = tls_handshake(c)) == 0) {
        if (GetTickCount64() - t0 > 15000) break;
        Sleep(1);
    }
    if (r != 1 && skip_unsup && r < 0 && (strstr(tls_error(c), "(40)") || strstr(tls_error(c), "encerrou a conexão durante"))) {
        printf("--    não suportado pelo servidor (%s)\n", tls_error(c));
        tls_free(c); closesocket(socks[s]); socks[s] = INVALID_SOCKET; return;
    }
    if (r != 1) { printf("FALHA handshake: %s\n", r ? tls_error(c) : "tempo esgotado"); fails++; tls_free(c); closesocket(socks[s]); socks[s] = INVALID_SOCKET; return; }
    u64 ths = GetTickCount64() - t0;
    char req[512];
    int rl = snprintf(req, sizeof(req), "GET / HTTP/1.1\r\nHost: %s\r\nUser-Agent: mOS-tlstest\r\nConnection: close\r\n\r\n", host);
    int sent = 0;
    while (sent < rl) {
        int n = tls_send(c, req + sent, rl - sent);
        if (n < 0) break;
        sent += n;
    }
    static char body[1 << 20];
    int total = 0;
    t0 = GetTickCount64();
    for (;;) {
        int n = tls_recv(c, body + total, (int)sizeof(body) - 1 - total);
        if (n < 0) break;
        if (n == 0) {
            if (GetTickCount64() - t0 > 15000) break;
            Sleep(1);
            continue;
        }
        total += n;
        if (total >= (int)sizeof(body) - 1) break;
    }
    body[total] = 0;
    if (getenv("TLSTEST_DUMP")) {
        char *hend = strstr(body, "\r\n\r\n");
        if (hend) fwrite(body, 1, hend - body, stdout);
        printf("\n[tail] %.80s\n", total > 80 ? body + total - 80 : body);
    }
    char *eol = strstr(body, "\r\n");
    if (eol) *eol = 0;
    const char *err = tls_error(c);
    bool ok = total > 0 && !strncmp(body, "HTTP/1.1 ", 9);
    printf("%s handshake %llums, %d bytes, \"%s\"%s%s\n", ok ? "ok   " : "FALHA", (unsigned long long)ths, total, body,
           strcmp(err, "sem erro") ? " erro: " : "", strcmp(err, "sem erro") ? err : "");
    if (ok) passes++; else fails++;
    tls_free(c);
    closesocket(socks[s]);
    socks[s] = INVALID_SOCKET;
}

/* TLS 1.3 com uma cifra forçada (TLS 1.2 continua oferecido como alternativa) */
static void live(const char *host, u16 suite) {
    prefs_default();
    tls_cipher_prefs[0] = suite;
    tls_cipher_prefs[1] = 0;
    live_run(host, suite_name(suite), false);
}

static const char *suite12_name(u16 s) {
    switch (s) {
    case 0xc02b: return "ECDHE_ECDSA_AES_128_GCM_SHA256";
    case 0xc02c: return "ECDHE_ECDSA_AES_256_GCM_SHA384";
    case 0xc02f: return "ECDHE_RSA_AES_128_GCM_SHA256";
    case 0xc030: return "ECDHE_RSA_AES_256_GCM_SHA384";
    case 0xcca8: return "ECDHE_RSA_CHACHA20_POLY1305";
    case 0xcca9: return "ECDHE_ECDSA_CHACHA20_POLY1305";
    default: return "?";
    }
}

/* TLS 1.2 forçado (só 1.2 no ClientHello) com uma cifra e um grupo */
static void live12(const char *host, u16 suite, u16 group, bool skip_unsup) {
    prefs_default();
    tls_versions = TLS_V12;
    tls12_cipher_prefs[0] = suite;
    tls12_cipher_prefs[1] = 0;
    tls_group_prefs[0] = group;
    tls_group_prefs[1] = 0;
    char label[96];
    snprintf(label, sizeof(label), "1.2 %s %s", suite12_name(suite), group == 0x17 ? "P-256" : group == 0x18 ? "P-384" : "x25519");
    live_run(host, label, skip_unsup);
    prefs_default();
}

/* TLS 1.3 com key_share secp256r1 */
static void live13_p256(const char *host) {
    prefs_default();
    tls_group_prefs[0] = 0x0017;
    tls_group_prefs[1] = 0;
    live_run(host, "1.3 padrão, key_share secp256r1", false);
    prefs_default();
}

/* --- servidor falso local: ServerHello forjado (sentinela de downgrade, versão antiga) --- */
static void mock_server_hello(const char *label, u8 versions, u16 ver, const u8 *tail8, const char *expect,
                              bool expect_absent) {
    prefs_default();
    tls_versions = versions;
    printf("  %-60s ", label);
    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(ls, (struct sockaddr *)&a, sizeof(a));
    listen(ls, 1);
    int al = sizeof(a);
    getsockname(ls, (struct sockaddr *)&a, &al);
    SOCKET cs = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(cs, (struct sockaddr *)&a, sizeof(a));
    SOCKET ss = accept(ls, NULL, NULL);
    closesocket(ls);
    u_long nb = 1;
    ioctlsocket(cs, FIONBIO, &nb);
    int idx = -1;
    for (int i = 0; i < nsocks; i++) if (socks[i] == INVALID_SOCKET) { idx = i; break; }
    if (idx < 0) idx = nsocks++;
    socks[idx] = cs;
    tls_conn_t *c = tls_new(idx, "mock.mos.test");
    tls_handshake(c);                                       /* envia o ClientHello */
    DWORD to = 3000;
    setsockopt(ss, SOL_SOCKET, SO_RCVTIMEO, (const char *)&to, sizeof(to));
    char junk[4096];
    recv(ss, junk, sizeof(junk), 0);
    u8 sh[128], *p = sh + 9;
    *p++ = (u8)(ver >> 8); *p++ = (u8)ver;
    for (int i = 0; i < 24; i++) *p++ = (u8)(0x40 + i);
    memcpy(p, tail8, 8); p += 8;
    *p++ = 0;                                               /* session id vazio */
    *p++ = 0xc0; *p++ = 0x2f;                               /* ECDHE_RSA_AES_128_GCM_SHA256 */
    *p++ = 0;
    static const u8 ext[] = {0, 9, 0x00, 0x17, 0, 0, 0xff, 0x01, 0, 1, 0};
    memcpy(p, ext, sizeof(ext)); p += sizeof(ext);
    u32 bl = (u32)(p - sh - 9);
    sh[0] = 22; sh[1] = 3; sh[2] = 3; sh[3] = (u8)((bl + 4) >> 8); sh[4] = (u8)(bl + 4);
    sh[5] = 2; sh[6] = 0; sh[7] = (u8)(bl >> 8); sh[8] = (u8)bl;
    send(ss, (const char *)sh, (int)(p - sh), 0);
    u64 t0 = GetTickCount64();
    int r;
    while ((r = tls_handshake(c)) == 0 && GetTickCount64() - t0 < 1000) Sleep(1);
    closesocket(ss);                                        /* se ainda esperava o Certificate */
    while (r == 0 && (r = tls_handshake(c)) == 0 && GetTickCount64() - t0 < 3000) Sleep(1);
    const char *err = tls_error(c);
    bool has = strstr(err, expect) != NULL;
    bool ok = r < 0 && (expect_absent ? !has : has);
    printf("%s \"%s\"\n", ok ? "ok   " : "FALHA", err);
    if (ok) passes++; else fails++;
    tls_free(c);
    closesocket(cs);
    socks[idx] = INVALID_SOCKET;
    prefs_default();
}

static void test_mock(void) {
    printf("ServerHello forjado (servidor local)\n");
    static const u8 dg12[8] = {'D', 'O', 'W', 'N', 'G', 'R', 'D', 1}, dg11[8] = {'D', 'O', 'W', 'N', 'G', 'R', 'D', 0};
    static const u8 plain[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    mock_server_hello("1.2 com sentinela DOWNGRD\\x01 (1.3 oferecido) -> rejeita", TLS_V12 | TLS_V13, 0x0303, dg12,
                      "ataque de downgrade", false);
    mock_server_hello("1.2 com sentinela DOWNGRD\\x00 (1.3 oferecido) -> rejeita", TLS_V12 | TLS_V13, 0x0303, dg11,
                      "ataque de downgrade", false);
    mock_server_hello("1.2 com sentinela, só 1.2 oferecido -> segue o handshake", TLS_V12, 0x0303, dg12,
                      "downgrade", true);
    mock_server_hello("1.2 sem sentinela -> segue o handshake", TLS_V12 | TLS_V13, 0x0303, plain, "downgrade", true);
    mock_server_hello("ServerHello TLS 1.0 -> versão antiga", TLS_V12 | TLS_V13, 0x0301, plain,
                      "versões antigas de TLS", false);
    mock_server_hello("1.2 recusado quando só 1.3 é oferecido", TLS_V13, 0x0303, plain, "não suporta TLS 1.3", false);
}

/* ------------------------------------------------------------------ OCSP */
static char *g_ca_pem;
static u32 g_ca_len;
extern void (*tls_test_ocsp_hook)(const char *host, const u8 *resp, u32 len);
static char g_ocsp_path[300];
static void capture_ocsp_hook(const char *host, const u8 *resp, u32 len) {
    FILE *f = fopen(g_ocsp_path, "wb");
    if (!f) return;
    fwrite(resp, 1, len, f);
    fclose(f);
    printf("  resposta OCSP grampeada de %s (%u bytes) salva em %s\n", host, len, g_ocsp_path);
}

static const char *ocsp_code(int e) {
    static const char *n[] = {"GOOD", "REVOKED", "BAD_SIG", "STALE", "UNKNOWN", "UNUSABLE"};
    return e >= 0 && e <= 5 ? n[e] : "?";
}

static void ocsp_case(const char *label, const u8 *resp, u32 rlen, const u8 *const *ders, const u32 *lens, u32 n,
                      const datetime_t *now, int expect, const char *substr) {
    char msg[256], nm[400];
    int e = x509_ocsp_check(resp, rlen, ders, lens, n, now, msg, sizeof(msg));
    bool ok = e == expect && (!substr || strstr(msg, substr));
    snprintf(nm, sizeof(nm), "%-34s %-8s \"%s\"", label, ocsp_code(e), msg);
    check_true(nm, ok);
}

static datetime_t mkdate(int y, int m, int d) {
    datetime_t t = {0};
    t.year = y; t.month = m; t.day = d; t.hour = 12;
    return t;
}

/* respostas sintéticas (tools/gen-ocsp-testdata.ps1): CA de teste, thisUpdate 2026-01-10,
 * nextUpdate 2026-01-17, revogado em 2026-01-05 por keyCompromise */
static void test_ocsp_synthetic(void) {
    printf("OCSP: respostas sintéticas (tools/testdata/ocsp, geradas por tools/gen-ocsp-testdata.ps1)\n");
    const u8 *ders[4];
    u32 lens[4];
    u8 *cb = NULL;
    int n = load_chain("tools/testdata/ocsp/mock.chain", ders, lens, 4, &cb);
    if (n != 2) { check_true("tools/testdata/ocsp/mock.chain presente", 0); return; }
    datetime_t ok = mkdate(2026, 1, 12), late = mkdate(2026, 2, 1), early = mkdate(2026, 1, 1),
               much_later = mkdate(2026, 3, 1);
    static const struct { const char *file; const datetime_t *now; int expect; const char *substr; } cases[] = {
        {"good-ca.der", NULL, OCSP_GOOD, "não revogado (mOS Teste OCSP CA"},
        {"revoked-ca.der", NULL, OCSP_REVOKED, "certificado revogado em 2026-01-05 (motivo: chave comprometida"},
        {"unknown-ca.der", NULL, OCSP_UNKNOWN, "status desconhecido"},
        {"good-delegado.der", NULL, OCSP_GOOD, "responder delegado mOS Teste OCSP Responder"},
        {"revoked-delegado.der", NULL, OCSP_REVOKED, "certificado revogado"},
        {"delegado-sem-eku.der", NULL, OCSP_BAD_SIG, "sem id-kp-OCSPSigning"},
        {"assinatura-falsa.der", NULL, OCSP_BAD_SIG, "assinatura inválida"},
        {"outro-serial.der", NULL, OCSP_UNUSABLE, "não se refere a este certificado"},
        {"try-later.der", NULL, OCSP_UNUSABLE, "responseStatus 3"},
    };
    for (u32 i = 0; i < ARRAY_LEN(cases); i++) {
        char path[200];
        u32 len = 0;
        snprintf(path, sizeof(path), "tools/testdata/ocsp/%s", cases[i].file);
        u8 *r = read_file(path, &len);
        if (!r) { check_true(path, 0); continue; }
        const datetime_t *now = cases[i].now ? cases[i].now : &ok;
        ocsp_case(cases[i].file, r, len, ders, lens, (u32)n, now, cases[i].expect, cases[i].substr);
        free(r);
    }
    u32 len = 0, rl = 0;
    u8 *g = read_file("tools/testdata/ocsp/good-ca.der", &len), *rv = read_file("tools/testdata/ocsp/revoked-ca.der", &rl);
    if (g && rv) {
        ocsp_case("good-ca, relógio após nextUpdate", g, len, ders, lens, 2, &late, OCSP_STALE, "expirada (válida até 2026-01-17)");
        ocsp_case("good-ca, relógio antes de thisUpdate", g, len, ders, lens, 2, &early, OCSP_STALE, "data no futuro");
        ocsp_case("revoked-ca, resposta antiga", rv, rl, ders, lens, 2, &much_later, OCSP_REVOKED, "revogado");
        ocsp_case("good-ca sem o emissor na cadeia", g, len, ders, lens, 1, &ok, OCSP_UNUSABLE, "emissor do certificado não encontrado");
        g[len - 3] ^= 1;
        ocsp_case("good-ca com assinatura adulterada", g, len, ders, lens, 2, &ok, OCSP_BAD_SIG, "assinatura inválida");
        g[len - 3] ^= 1;
        u8 *cut = malloc(len);
        memcpy(cut, g, len);
        ocsp_case("good-ca truncada", cut, len - 40, ders, lens, 2, &ok, OCSP_UNUSABLE, "malformada");
        free(cut);
    }
    free(g);
    free(rv);
    free(cb);
}

/* respostas reais capturadas com 'tlstest capture <host>' (tools/testdata/<host>.ocsp) */
static void test_ocsp_real(const char *host, const char *other_chain) {
    char cp[200], op[200];
    snprintf(cp, sizeof(cp), "tools/testdata/%s.chain", host);
    snprintf(op, sizeof(op), "tools/testdata/%s.ocsp", host);
    const u8 *ders[10], *od[10];
    u32 lens[10], ol[10], rlen = 0;
    u8 *cb = NULL, *ob = NULL;
    int n = load_chain(cp, ders, lens, 10, &cb);
    u8 *r = read_file(op, &rlen);
    if (n <= 0 || !r) { printf("  (sem %s/.ocsp — rode 'tlstest capture %s')\n", cp, host); free(cb); free(r); return; }
    char msg[256], label[160];
    int e = x509_ocsp_check(r, rlen, ders, lens, (u32)n, NULL, msg, sizeof(msg));
    snprintf(label, sizeof(label), "%s: sem checar datas", host);
    ocsp_case(label, r, rlen, ders, lens, (u32)n, NULL, OCSP_GOOD, "não revogado");
    /* datas da própria resposta: "...; AAAA-MM-DD..AAAA-MM-DD)" */
    const char *d = e == OCSP_GOOD ? strstr(msg, "..") : NULL;
    if (d && d - msg >= 10) {
        int y1, m1, d1, y2, m2, d2;
        if (sscanf(d - 10, "%d-%d-%d..%d-%d-%d", &y1, &m1, &d1, &y2, &m2, &d2) == 6) {
            datetime_t in = mkdate(y1, m1, d1), out = mkdate(y2, m2, d2);
            in.hour = 23;
            out.day += 2; if (out.day > 28) { out.day = 2; out.month++; if (out.month > 12) { out.month = 1; out.year++; } }
            snprintf(label, sizeof(label), "%s: dentro da validade", host);
            ocsp_case(label, r, rlen, ders, lens, (u32)n, &in, OCSP_GOOD, NULL);
            snprintf(label, sizeof(label), "%s: 2 dias após nextUpdate", host);
            ocsp_case(label, r, rlen, ders, lens, (u32)n, &out, OCSP_STALE, "expirada");
        }
    }
    r[rlen - 2] ^= 0x01;
    snprintf(label, sizeof(label), "%s: assinatura adulterada", host);
    ocsp_case(label, r, rlen, ders, lens, (u32)n, NULL, OCSP_BAD_SIG, NULL);
    r[rlen - 2] ^= 0x01;
    int on = load_chain(other_chain, od, ol, 10, &ob);
    if (on > 0) {
        snprintf(label, sizeof(label), "%s: resposta usada em outro site", host);
        ocsp_case(label, r, rlen, od, ol, (u32)on, NULL, OCSP_UNUSABLE, NULL);
    }
    free(ob);
    free(cb);
    free(r);
}

/* TLS 1.2 com servidor local: ServerHello + Certificate + CertificateStatus forjados
 * (cadeia da CA de teste, que vira temporariamente a única raiz confiável) */
static void mock_tls12_staple(const char *label, const char *ocsp_file, const char *expect) {
    prefs_default();
    printf("  %-60s ", label);
    const u8 *ders[4];
    u32 lens[4], ol = 0, pl = 0;
    u8 *cb = NULL;
    int n = load_chain("tools/testdata/ocsp/mock.chain", ders, lens, 4, &cb);
    u8 *ocsp = read_file(ocsp_file, &ol);
    char *pem = (char *)read_file("tools/testdata/ocsp/ca.pem", &pl);
    if (n != 2 || !ocsp || !pem) { printf("FALHA (dados de teste ausentes)\n"); fails++; free(cb); free(ocsp); free(pem); return; }
    x509_trust_load_pem(pem, pl);
    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(ls, (struct sockaddr *)&a, sizeof(a));
    listen(ls, 1);
    int al = sizeof(a);
    getsockname(ls, (struct sockaddr *)&a, &al);
    SOCKET cs = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(cs, (struct sockaddr *)&a, sizeof(a));
    SOCKET ss = accept(ls, NULL, NULL);
    closesocket(ls);
    u_long nb = 1;
    ioctlsocket(cs, FIONBIO, &nb);
    int idx = -1;
    for (int i = 0; i < nsocks; i++) if (socks[i] == INVALID_SOCKET) { idx = i; break; }
    if (idx < 0) idx = nsocks++;
    socks[idx] = cs;
    tls_conn_t *c = tls_new(idx, "mock.mos.test");
    tls_handshake(c);
    DWORD to = 3000;
    setsockopt(ss, SOL_SOCKET, SO_RCVTIMEO, (const char *)&to, sizeof(to));
    char junk[4096];
    recv(ss, junk, sizeof(junk), 0);
    static u8 rec[16384];
    u8 *p = rec + 5;
    /* ServerHello: 1.2, ECDHE_RSA_AES_128_GCM, extensões EMS, renegotiation_info, status_request */
    u8 *h = p;
    *p++ = 2; p += 3;
    *p++ = 3; *p++ = 3;
    for (int i = 0; i < 32; i++) *p++ = (u8)(0x50 + i);
    *p++ = 0;
    *p++ = 0xc0; *p++ = 0x2f; *p++ = 0;
    static const u8 ext[] = {0, 13, 0x00, 0x17, 0, 0, 0xff, 0x01, 0, 1, 0, 0x00, 0x05, 0, 0};
    memcpy(p, ext, sizeof(ext)); p += sizeof(ext);
    u32 hl = (u32)(p - h - 4);
    h[1] = (u8)(hl >> 16); h[2] = (u8)(hl >> 8); h[3] = (u8)hl;
    /* Certificate */
    h = p;
    u32 list = 3 + lens[0] + 3 + lens[1];
    *p++ = 11; *p++ = (u8)((list + 3) >> 16); *p++ = (u8)((list + 3) >> 8); *p++ = (u8)(list + 3);
    *p++ = (u8)(list >> 16); *p++ = (u8)(list >> 8); *p++ = (u8)list;
    for (int i = 0; i < 2; i++) {
        *p++ = 0; *p++ = (u8)(lens[i] >> 8); *p++ = (u8)lens[i];
        memcpy(p, ders[i], lens[i]); p += lens[i];
    }
    /* CertificateStatus */
    *p++ = 22; *p++ = 0; *p++ = (u8)((ol + 4) >> 8); *p++ = (u8)(ol + 4);
    *p++ = 1; *p++ = 0; *p++ = (u8)(ol >> 8); *p++ = (u8)ol;
    memcpy(p, ocsp, ol); p += ol;
    u32 body = (u32)(p - rec - 5);
    rec[0] = 22; rec[1] = 3; rec[2] = 3; rec[3] = (u8)(body >> 8); rec[4] = (u8)body;
    send(ss, (const char *)rec, (int)(p - rec), 0);
    u64 t0 = GetTickCount64();
    int r;
    while ((r = tls_handshake(c)) == 0 && GetTickCount64() - t0 < 1000) Sleep(1);
    closesocket(ss);                                         /* se o cliente seguiu esperando o ServerKeyExchange */
    while (r == 0 && (r = tls_handshake(c)) == 0 && GetTickCount64() - t0 < 3000) Sleep(1);
    const char *err = r < 0 ? tls_error(c) : "(sem erro)";
    bool ok = r < 0 && strstr(err, expect);
    printf("%s \"%s\"\n", ok ? "ok   " : "FALHA", err);
    if (ok) passes++; else fails++;
    tls_free(c);
    closesocket(cs);
    socks[idx] = INVALID_SOCKET;
    x509_trust_load_pem(g_ca_pem, g_ca_len);                 /* volta à lista da Mozilla */
    free(cb); free(ocsp); free(pem);
}

static void test_ocsp(void) {
    test_ocsp_synthetic();
    printf("OCSP: respostas reais grampeadas (tools/testdata/<host>.ocsp)\n");
    test_ocsp_real("www.digicert.com", "tools/testdata/github.com.chain");
    test_ocsp_real("www.microsoft.com", "tools/testdata/github.com.chain");
    test_ocsp_real("www.amazon.com", "tools/testdata/github.com.chain");
    printf("OCSP: TLS 1.2 com CertificateStatus forjado (servidor local)\n");
    mock_tls12_staple("staple revoked (CA de teste) -> rejeita", "tools/testdata/ocsp/revoked-ca.der",
                      "certificado revogado em 2026-01-05");
    mock_tls12_staple("staple revoked por responder delegado -> rejeita", "tools/testdata/ocsp/revoked-delegado.der",
                      "certificado revogado");
    mock_tls12_staple("staple good mas vencido (jan/2026) -> rejeita", "tools/testdata/ocsp/good-ca.der",
                      "resposta OCSP grampeada expirada");
    mock_tls12_staple("staple com assinatura falsa -> rejeita", "tools/testdata/ocsp/assinatura-falsa.der",
                      "assinatura inválida");
    mock_tls12_staple("staple de outro certificado -> soft-fail", "tools/testdata/ocsp/outro-serial.der",
                      "conexão TCP encerrada");
}

int main(int argc, char **argv) {
    test_sha();
    test_hmac_hkdf();
    test_x25519();
    test_chacha();
    test_aes();
    test_rfc8448();
    test_tls12_prf();
    test_rng();
    const char *capath = getenv("TLSTEST_CA") ? getenv("TLSTEST_CA") : "assets/certs/cacert.pem";
    u32 calen = 0;
    char *ca = (char *)read_file(capath, &calen);
    int nroots = ca ? x509_trust_load_pem(ca, calen) : -1;
    g_ca_pem = ca;
    g_ca_len = calen;
    printf("Lista de confiança %s: %d raízes\n", capath, nroots);
    check_true("bundle da Mozilla carregado (> 100 raízes)", nroots > 100);
    test_pk();
    test_x509();
    WSADATA wd;
    WSAStartup(MAKEWORD(2, 2), &wd);
    test_mock();
    test_ocsp();
    if (argc > 1 && !strcmp(argv[1], "live")) {
        printf("Handshake real (Winsock) com validação de certificados\n");
        static const char *def[] = {"example.com", "www.google.com", "cloudflare.com", "github.com"};
        const char **hosts = argc > 2 ? (const char **)argv + 2 : def;
        int nh = argc > 2 ? argc - 2 : 4;
        static const u16 suites[] = {0x1303, 0x1301, 0x1302};
        for (int i = 0; i < nh; i++)
            for (int k = 0; k < 3; k++) live(hosts[i], suites[k]);
        if (argc == 2) {
            printf("TLS 1.3 com key_share secp256r1\n");
            live13_p256("www.google.com");
            live13_p256("cloudflare.com");
            printf("Servidores só-TLS-1.2 (badssl.com), preferências padrão (1.3 + 1.2 oferecidos)\n");
            static const char *b12[] = {"badssl.com", "sha256.badssl.com", "ecc256.badssl.com", "rsa2048.badssl.com",
                                        "ecc384.badssl.com", "rsa4096.badssl.com"};
            for (u32 i = 0; i < ARRAY_LEN(b12); i++) {
                prefs_default();
                live_run(b12[i], "padrão (1.3 + 1.2)", false);
            }
            /* RSA-8192: o certificado de rsa8192.badssl.com venceu em 2024-03-27; com o relógio
             * em 2024-01-15 o handshake 1.2 inteiro roda (cadeia + ServerKeyExchange RSA-8192) */
            printf("RSA-8192 (rsa8192.badssl.com, relógio forçado em 2024-01-15)\n");
            prefs_default();
            g_fake_clock = true;
            g_fake_now = mkdate(2024, 1, 15);
            live_run("rsa8192.badssl.com", "padrão, relógio 2024-01-15", false);
            g_fake_clock = false;
            live_reject("rsa8192.badssl.com", "rsa8192.badssl.com", "certificado expirado (válido até 2024-03-27)");
            printf("OCSP grampeado (status_request) em sites reais\n");
            static const char *oc[] = {"www.digicert.com", "www.microsoft.com", "www.amazon.com", "www.sectigo.com",
                                       "www.globalsign.com", "revoked.badssl.com"};
            for (u32 i = 0; i < ARRAY_LEN(oc); i++) {
                prefs_default();
                live_run(oc[i], "padrão + status_request", false);
            }
        }
    }
    if (argc > 1 && !strcmp(argv[1], "live12")) {
        /* cada cifra e grupo do TLS 1.2, forçados; servidores que não suportam a
         * combinação respondem handshake_failure e são marcados como "--" */
        printf("TLS 1.2 forçado: todas as cifras x grupos\n");
        static const char *def[] = {"badssl.com", "ecc256.badssl.com", "www.google.com", "cloudflare.com", "github.com"};
        const char **hosts = argc > 2 ? (const char **)argv + 2 : def;
        int nh = argc > 2 ? argc - 2 : (int)ARRAY_LEN(def);
        for (int i = 0; i < nh; i++)
            for (int k = 0; k < 6; k++)
                for (int g = 0; g < 3; g++) live12(hosts[i], DEF12[k], DEFGRP[g], true);
    }
    if (argc > 2 && !strcmp(argv[1], "capture")) {
        printf("Captura de cadeias (tools/testdata/<host>.chain)\n");
        CreateDirectoryA("tools\\testdata", NULL);
        tls_test_chain_hook = capture_hook;
        tls_test_ocsp_hook = capture_ocsp_hook;
        for (int i = 2; i < argc; i++) {
            static char path[300];
            snprintf(path, sizeof(path), "tools/testdata/%s.chain", argv[i]);
            snprintf(g_ocsp_path, sizeof(g_ocsp_path), "tools/testdata/%s.ocsp", argv[i]);
            g_capture_path = path;
            live(argv[i], 0x1303);
        }
        tls_test_chain_hook = NULL;
        tls_test_ocsp_hook = NULL;
    }
    if (argc > 1 && !strcmp(argv[1], "neg")) {
        printf("Testes negativos (devem ser REJEITADOS)\n");
        /* badssl.com só fala TLS 1.2: handshake 1.2 real, rejeitado na validação da cadeia */
        prefs_default();
        live_run("badssl.com", "controle: TLS 1.2 válido", false);
        live_reject("expired.badssl.com", "expired.badssl.com", "certificado expirado");
        live_reject("wrong.host.badssl.com", "wrong.host.badssl.com",
                    "o nome do certificado não corresponde a wrong.host.badssl.com");
        live_reject("self-signed.badssl.com", "self-signed.badssl.com", "certificado não confiável: autoassinado");
        live_reject("untrusted-root.badssl.com", "untrusted-root.badssl.com",
                    "certificado não confiável: emissor desconhecido");
        /* TLS 1.3 de verdade com nome errado (SNI de outro site) */
        live_reject("www.google.com", "example.com", "o nome do certificado não corresponde a example.com");
        live_reject("github.com", "mos-teste.github.io.invalid", "não corresponde a mos-teste.github.io.invalid");
        printf("  (controle positivo: IP literal com iPAddress no SAN)\n");
        live("1.1.1.1", 0x1303);
        /* IP literal: sem SNI, o certificado padrão não tem iPAddress correspondente */
        struct addrinfo hints = {0}, *res;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo("github.com", "443", &hints, &res) == 0) {
            char ip[64];
            inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ip, sizeof(ip));
            freeaddrinfo(res);
            char msg[96];
            snprintf(msg, sizeof(msg), "não corresponde a %s", ip);
            live_reject(ip, ip, msg);
        }
    }
    printf("\n%d ok, %d falhas\n", passes, fails);
    return fails ? 1 : 0;
}
