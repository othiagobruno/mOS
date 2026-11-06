/* mOS — teste no host das funções do SMP (kernel/bt/smp_crypto.c) e do interpretador HID
 * (kernel/bt/hid_parse.c) contra exemplos da especificação Bluetooth e descritores reais. */
#include "wifi.h"
#include "../kernel/include/hid.h"

long long g_live_allocs, g_live_bytes;
static int fails;
void smp_c1(const u8 *k, const u8 *r, const u8 *preq, const u8 *pres, u8 iat, const u8 *ia, u8 rat, const u8 *ra, u8 *out);
void smp_s1(const u8 *k, const u8 *r1, const u8 *r2, u8 *out);
void smp_ah(const u8 *irk, const u8 *r, u8 *out);

/* hex "MSB primeiro" (como na especificação) -> bytes little-endian */
static void le_from(const char *s, u8 *o, int n) { for (int i = 0; i < n; i++) { unsigned v; sscanf(s + (n - 1 - i) * 2, "%2x", &v); o[i] = (u8)v; } }
static void check(const char *what, const u8 *le, int n, const char *msb) {
    char h[64];
    for (int i = 0; i < n; i++) sprintf(h + i * 2, "%02x", le[n - 1 - i]);
    if (strcmp(h, msb)) { fails++; printf("FALHOU %s\n  obtido   %s\n  esperado %s\n", what, h, msb); }
    else printf("ok  %s\n", what);
}

int main(void) {
    /* Core spec v5.x, Vol 3, Parte H, 2.2.3 (exemplo de c1) */
    u8 k[16] = {0}, r[16], preq[7], pres[7], ia[6], ra[6], out[16];
    le_from("5783d52156ad6f0e6388274ec6702ee0", r, 16);
    le_from("07071000000101", preq, 7);
    le_from("05000800000302", pres, 7);
    le_from("a1a2a3a4a5a6", ia, 6);
    le_from("b1b2b3b4b5b6", ra, 6);
    smp_c1(k, r, preq, pres, 1, ia, 0, ra, out);
    check("SMP c1 (exemplo da especificação)", out, 16, "1e1e3fef878988ead2a74dc5bef13b86");
    /* 2.2.4 (exemplo de s1) */
    u8 r1[16], r2[16];
    le_from("000f0e0d0c0b0a091122334455667788", r1, 16);
    le_from("010203040506070899aabbccddeeff00", r2, 16);
    smp_s1(k, r1, r2, out);
    check("SMP s1 (exemplo da especificação)", out, 16, "9a1fe1f0e8b0f49b5b4216ae796da062");
    /* 2.2.2 (exemplo de ah): IRK ec0234a357c8ad05341010a60a397d9b, r 708194 -> 0dfbaa */
    u8 irk[16], rr[3], h3[3];
    le_from("ec0234a357c8ad05341010a60a397d9b", irk, 16);
    le_from("708194", rr, 3);
    smp_ah(irk, rr, h3);
    check("SMP ah (exemplo da especificação)", h3, 3, "0dfbaa");

    /* ---- LE Secure Connections */
    extern void aes_cmac(const u8 *k, const u8 *m, usize len, u8 *out);
    extern void smp_f4(const u8 *u, const u8 *v, const u8 *x, u8 z, u8 *out);
    extern void smp_f5(const u8 *w, const u8 *n1, const u8 *n2, const u8 *a1, const u8 *a2, u8 *mk, u8 *ltk);
    extern void smp_f6(const u8 *w, const u8 *n1, const u8 *n2, const u8 *r, const u8 *io, const u8 *a1, const u8 *a2, u8 *out);
    extern void p256_public(const u8 *priv, u8 *x, u8 *y);
    extern bool p256_ecdh(const u8 *priv, const u8 *px, const u8 *py, u8 *dh);
    /* AES-CMAC, RFC 4493 (exemplos 1 e 2); tudo em ordem MSB primeiro */
    u8 ck[16], cm[16], mac[16];
    #define MSB(s, o, n) do { for (int _i = 0; _i < (n); _i++) { unsigned _v; sscanf((s) + _i * 2, "%2x", &_v); (o)[_i] = (u8)_v; } } while (0)
    #define CHK_MSB(what, got, n, want) do { char _h[200]; for (int _i = 0; _i < (n); _i++) sprintf(_h + _i * 2, "%02x", (got)[_i]); \
        if (strcmp(_h, want)) { fails++; printf("FALHOU %s\n  obtido   %s\n  esperado %s\n", what, _h, want); } else printf("ok  %s\n", what); } while (0)
    MSB("2b7e151628aed2a6abf7158809cf4f3c", ck, 16);
    aes_cmac(ck, NULL, 0, mac);
    CHK_MSB("AES-CMAC RFC 4493 #1 (vazio)", mac, 16, "bb1d6929e95937287fa37d129b756746");
    MSB("6bc1bee22e409f96e93d7e117393172a", cm, 16);
    aes_cmac(ck, cm, 16, mac);
    CHK_MSB("AES-CMAC RFC 4493 #2 (16 bytes)", mac, 16, "070a16b46b4d4144f79bdd9dd04a287c");
    /* f4, f5, f6: exemplos do Apêndice D da especificação */
    u8 U[32], V[32], X[16], W[32], N1[16], N2[16], A1[7], A2[7], R[16], IO[3], mk[16], ltk[16], o[16];
    MSB("20b003d2f297be2c5e2c83a7e9f9a5b9eff49111acf4fddbcc0301480e359de6", U, 32);
    MSB("55188b3d32f6bb9a900afcfbeed4e72a59cb9ac2f19d7cfb6b4fdd49f47fc5fd", V, 32);
    MSB("d5cb8454d177733effffb2ec712baeab", X, 16);
    smp_f4(U, V, X, 0, o);
    CHK_MSB("SMP f4 (Apêndice D)", o, 16, "f2c916f107a9bd1cf1eda1bea974872d");
    MSB("ec0234a357c8ad05341010a60a397d9b99796b13b4f866f1868d34f373bfa698", W, 32);
    MSB("d5cb8454d177733effffb2ec712baeab", N1, 16);
    MSB("a6e8e7cc25a75f6e216583f7ff3dc4cf", N2, 16);
    MSB("0056123737bfce", A1, 7);
    MSB("00a713702dcfc1", A2, 7);
    smp_f5(W, N1, N2, A1, A2, mk, ltk);
    CHK_MSB("SMP f5 MacKey (Apêndice D)", mk, 16, "2965f176a1084a02fd3f6a20ce636e20");
    CHK_MSB("SMP f5 LTK (Apêndice D)", ltk, 16, "6986791169d7cd23980522b594750a38");
    MSB("12a3343bb453bb5408da42d20c2d0fc8", R, 16);
    MSB("010102", IO, 3);
    smp_f6(mk, N1, N2, R, IO, A1, A2, o);
    CHK_MSB("SMP f6 (Apêndice D)", o, 16, "e3c473989cd0e8c5d26c0b09da958f61");
    /* P-256: 1·G = G; chave pública e DHKey dos exemplos (little-endian como no SMP) */
    u8 one[32] = {1}, gx[32], gy[32];
    p256_public(one, gx, gy);
    check("P-256: 1·G = G (x)", gx, 32, "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296");
    u8 pa[32], pax[32], pay[32], pb[32], pbx[32], pby[32], d1[32], d2[32];
    le_from("3f49f6d4a3c55f3874c9b3e3d2103f504aff607beb40b7995899b8a6cd3c1abd", pa, 32);
    le_from("55188b3d32f6bb9a900afcfbeed4e72a59cb9ac2f19d7cfb6b4fdd49f47fc5fd", pb, 32);
    p256_public(pa, pax, pay);
    check("P-256 chave pública A (x)", pax, 32, "20b003d2f297be2c5e2c83a7e9f9a5b9eff49111acf4fddbcc0301480e359de6");
    check("P-256 chave pública A (y)", pay, 32, "dc809c49652aeb6d63329abf5a52155c766345c28fed3024741c8ed01589d28b");
    p256_public(pb, pbx, pby);
    bool e1 = p256_ecdh(pa, pbx, pby, d1), e2 = p256_ecdh(pb, pax, pay, d2);
    check("P-256 ECDH a·Pb", d1, 32, "ec0234a357c8ad05341010a60a397d9b99796b13b4f866f1868d34f373bfa698");
    if (!e1 || !e2 || memcmp(d1, d2, 32)) { fails++; printf("FALHOU ECDH simétrico\n"); } else printf("ok  ECDH simétrico (a·Pb = b·Pa)\n");
    u8 bad[32];
    memcpy(bad, pby, 32); bad[0] ^= 1;
    if (p256_ecdh(pa, pbx, bad, d1)) { fails++; printf("FALHOU ponto fora da curva aceito\n"); } else printf("ok  ponto fora da curva recusado\n");

    /* HID: descritor típico de mouse com ID de relatório, X/Y de 12 bits e roda (estilo Logitech) */
    static const u8 desc[] = {
        0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x02, 0x09, 0x01, 0xA1, 0x00,
        0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00, 0x25, 0x01, 0x95, 0x10, 0x75, 0x01, 0x81, 0x02,
        0x05, 0x01, 0x16, 0x01, 0xF8, 0x26, 0xFF, 0x07, 0x75, 0x0C, 0x95, 0x02, 0x09, 0x30, 0x09, 0x31, 0x81, 0x06,
        0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x01, 0x09, 0x38, 0x81, 0x06,
        0x05, 0x0C, 0x0A, 0x38, 0x02, 0x95, 0x01, 0x81, 0x06, 0xC0, 0xC0};
    hid_mouse_t m;
    bool ok = hid_parse_mouse(desc, sizeof desc, &m);
    printf("%s  descritor: id %d, botões %d@%d, X %d bits@%d, Y @%d, roda @%d\n", ok ? "ok " : "FALHOU", m.report_id, m.buttons_count,
           m.buttons_bit, m.x_size, m.x_bit, m.y_bit, m.wheel_bit);
    if (!ok || m.report_id != 2 || m.x_size != 12 || m.x_bit != 16 || m.y_bit != 28 || m.wheel_bit != 40) fails++;
    /* relatório: botão esquerdo, X = -3, Y = +5, roda = -1 */
    u8 rep[] = {0x01, 0x00, 0xFD, 0x5F, 0x00, 0xFF, 0x00};   /* bits 16..27 = 0xFFD (-3), 28..39 = 0x005 */
    int dx, dy, dz, btn;
    hid_mouse_decode(&m, rep, sizeof rep, &btn, &dx, &dy, &dz);
    printf("%s  relatório: botões %d dx %d dy %d roda %d\n", btn == 1 && dx == -3 && dy == 5 && dz == -1 ? "ok " : "FALHOU", btn, dx, dy, dz);
    if (!(btn == 1 && dx == -3 && dy == 5 && dz == -1)) fails++;
    printf(fails ? "%d FALHA(S)\n" : "TUDO OK\n", fails);
    return fails != 0;
}
