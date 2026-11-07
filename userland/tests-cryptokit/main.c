/* teste do núcleo C do CryptoKit (cryptokit.c), do IME (appkit_ime.c) e do Vision (vision.c), no harness */
#include <stdio.h>
int mos_ck_selftest(void);
int cx_ime_selftest(char *out, unsigned long cap);
int cx_vision_selftest(char *out, unsigned long cap);
int cx_vision_selftest2(char *out, unsigned long cap);
int cx_vision_selftest3(char *out, unsigned long cap);
int main(void) {
    static const char *names[] = {"SHA-256", "SHA-512", "MD5", "SHA-1", "HMAC-SHA256", "HKDF", "AES-GCM", "ChaCha20-Poly1305",
                                  "X25519", "Ed25519 chave", "Ed25519 assinatura", "Ed25519 verificação", "P-256", "P-384", "P-521", "AES-KW", "AES-192-GCM"};
    int f = mos_ck_selftest(), bad = 0;
    for (int i = 0; i < 17; i++) { printf("%-22s %s\n", names[i], (f >> i) & 1 ? "FALHOU" : "ok"); bad += (f >> i) & 1; }
    char ime[512];
    cx_ime_selftest(ime, sizeof ime);
    printf("IME: %s\n", ime);
    char vis[512];
    int vf = cx_vision_selftest(vis, sizeof vis);
    printf("Vision: %s (%s)\n", vis, vf ? "FALHOU" : "ok");
    bad += vf;
    int vf2 = cx_vision_selftest2(vis, sizeof vis);
    printf("Vision 2: %s (%s)\n", vis, vf2 ? "FALHOU" : "ok");
    bad += vf2;
    int vf3 = cx_vision_selftest3(vis, sizeof vis);
    printf("Vision 3: %s (%s)\n", vis, vf3 ? "FALHOU" : "ok");
    bad += vf3;
    printf("%s\n", bad ? "FALHAS" : "TUDO OK");
    return bad;
}
