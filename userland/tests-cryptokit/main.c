/* teste do núcleo C do CryptoKit (cryptokit.c), do IME (appkit_ime.c) e do Vision (vision.c), no harness */
#include <stdio.h>
#include <string.h>
int cx_bzip2_selftest(void);
void *BZ2_bzReadOpen(int *e, FILE *f, int v, int s, void *u, int n);
int BZ2_bzRead(int *e, void *h, void *buf, int len);
void BZ2_bzReadClose(int *e, void *h);
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
    /* bzip2: vetor curto e um arquivo de 400 KB em 4 blocos (-1), lido em pedaços pelo BZ2_bzRead */
    int bz1 = cx_bzip2_selftest(), bz2 = 1;
    FILE *fz = fopen("/mix1.bz2", "rb"), *fo = fopen("/mix.bin", "rb");
    if (fz && fo) {
        int e = 0;
        void *h = BZ2_bzReadOpen(&e, fz, 0, 0, 0, 0);
        static unsigned char a[4096], b[4096];
        long tot = 0, same = 1;
        for (;;) {
            int n = BZ2_bzRead(&e, h, a, sizeof a);
            if (n <= 0) break;
            if (fread(b, 1, (size_t)n, fo) != (size_t)n || memcmp(a, b, (size_t)n)) same = 0;
            tot += n;
            if (e == 4) break;
        }
        BZ2_bzReadClose(&e, h);
        bz2 = !(same && tot == 400000);
        printf("bzip2: arquivo %ld bytes, %s\n", tot, same ? "igual ao original" : "DIFERENTE");
    }
    printf("bzip2: vetor %s, arquivo %s\n", bz1 ? "FALHOU" : "ok", bz2 ? "FALHOU" : "ok");
    bad += (bz1 != 0) + bz2;
    printf("%s\n", bad ? "FALHAS" : "TUDO OK");
    return bad;
}
