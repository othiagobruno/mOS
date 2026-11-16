/* mOS — teste de host do CommonCrypto do libSystem (commoncrypto.c, commoncrypto2.c, commoncrypto_cryptor.inc).
 *
 * Os dois .c são compilados com o mesmo alvo do libSystem (zig cc -target x86_64-freestanding-none) e ligados a
 * este harness com a libc do host; tools/cctest.py monta isso e confere contra o OpenSSL/hashlib.
 *
 *   cctest crypt ALG MODO PAD OP CHAVEHEX IVHEX [TWEAKHEX]    stdin -> stdout (CCCryptorCreateWithMode)
 *   cctest xts OP CHAVEHEX TWEAKKEYHEX UNIDADEHEX            stdin -> stdout (CCCryptorEncryptDataBlock)
 *   cctest gcm enc|dec CHAVEHEX IVHEX AADHEX TAGLEN [TAGHEX] stdin -> stdout (+ tag em hex no stderr)
 *   cctest hash md2|md4|md5|sha1|sha224|sha256|sha384|sha512  stdin -> hex
 *   cctest hmac ALG CHAVEHEX                                  stdin -> hex
 *   cctest pbkdf PRF SENHA SALHEX RODADAS TAMANHO             -> hex
 *   cctest calibrate                                          -> rodadas para 100 ms */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

uint64_t mach_absolute_time(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec; }

int CCCryptorCreateWithMode(uint32_t op, uint32_t mode, uint32_t alg, uint32_t padding, const void *iv, const void *key,
                            size_t klen, const void *tweak, size_t tlen, int rounds, uint32_t options, void **ref);
int CCCryptorUpdate(void *ref, const void *in, size_t inlen, void *out, size_t avail, size_t *moved);
int CCCryptorFinal(void *ref, void *out, size_t avail, size_t *moved);
int CCCryptorRelease(void *ref);
int CCCryptorEncryptDataBlock(void *ref, const void *iv, const void *in, size_t n, void *out);
int CCCryptorDecryptDataBlock(void *ref, const void *iv, const void *in, size_t n, void *out);
int CCCryptorGCMOneshotEncrypt(uint32_t alg, const void *key, size_t kl, const void *iv, size_t ivl, const void *a, size_t al,
                               const void *in, size_t n, void *out, void *tag, size_t tl);
int CCCryptorGCMOneshotDecrypt(uint32_t alg, const void *key, size_t kl, const void *iv, size_t ivl, const void *a, size_t al,
                               const void *in, size_t n, void *out, const void *tag, size_t tl);
unsigned char *CC_MD2(const void *, uint32_t, unsigned char *);
unsigned char *CC_MD4(const void *, uint32_t, unsigned char *);
unsigned char *CC_MD5(const void *, uint32_t, unsigned char *);
unsigned char *CC_SHA1(const void *, uint32_t, unsigned char *);
unsigned char *CC_SHA224(const void *, uint32_t, unsigned char *);
unsigned char *CC_SHA256(const void *, uint32_t, unsigned char *);
unsigned char *CC_SHA384(const void *, uint32_t, unsigned char *);
unsigned char *CC_SHA512(const void *, uint32_t, unsigned char *);
void CCHmac(uint32_t alg, const void *key, size_t klen, const void *d, size_t n, void *mac);
int CCKeyDerivationPBKDF(uint32_t a, const char *pw, size_t pl, const uint8_t *salt, size_t sl, uint32_t prf, uint32_t r,
                         uint8_t *out, size_t ol);
uint32_t CCCalibratePBKDF(uint32_t alg, size_t pwlen, size_t saltlen, uint32_t prf, size_t dklen, uint32_t ms);

static size_t unhex(const char *h, uint8_t *o) {
    size_t n = strlen(h) / 2;
    for (size_t i = 0; i < n; i++) sscanf(h + 2 * i, "%2hhx", &o[i]);
    return n;
}
static void phex(FILE *f, const uint8_t *b, size_t n) { for (size_t i = 0; i < n; i++) fprintf(f, "%02x", b[i]); fputc('\n', f); }
static uint8_t *slurp(size_t *n) {
    size_t cap = 1 << 16, len = 0;
    uint8_t *b = malloc(cap);
    size_t k;
    while ((k = fread(b + len, 1, cap - len, stdin)) > 0) { len += k; if (len == cap) b = realloc(b, cap *= 2); }
    *n = len;
    return b;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    const char *c = argv[1];
    uint8_t key[256], iv[64], tw[64], tag[16];
    size_t n;
    if (!strcmp(c, "crypt") && argc >= 8) {
        uint32_t alg = (uint32_t)atoi(argv[2]), mode = (uint32_t)atoi(argv[3]), pad = (uint32_t)atoi(argv[4]), op = (uint32_t)atoi(argv[5]);
        size_t kl = unhex(argv[6], key), il = unhex(argv[7], iv), tl = argc > 8 ? unhex(argv[8], tw) : 0;
        void *ref;
        int r = CCCryptorCreateWithMode(op, mode, alg, pad, il ? iv : 0, key, kl, tl ? tw : 0, tl, 0, 0, &ref);
        if (r) { fprintf(stderr, "create %d\n", r); return 2; }
        uint8_t *in = slurp(&n), *out = malloc(n + 64);
        size_t a = 0, b = 0, done = 0;
        /* em pedaços irregulares, para exercitar o buffer interno */
        for (size_t off = 0; off < n;) {
            size_t step = (off * 7 + 3) % 29 + 1;
            if (step > n - off) step = n - off;
            r = CCCryptorUpdate(ref, in + off, step, out + done, n + 64 - done, &a);
            if (r) { fprintf(stderr, "update %d\n", r); return 3; }
            done += a;
            off += step;
        }
        r = CCCryptorFinal(ref, out + done, n + 64 - done, &b);
        if (r) { fprintf(stderr, "final %d\n", r); return 4; }
        fwrite(out, 1, done + b, stdout);
        CCCryptorRelease(ref);
        return 0;
    }
    if (!strcmp(c, "xts") && argc == 6) {
        uint32_t op = (uint32_t)atoi(argv[2]);
        size_t kl = unhex(argv[3], key), tl = unhex(argv[4], tw);
        unhex(argv[5], iv);
        void *ref;
        if (CCCryptorCreateWithMode(op, 8, 0, 0, 0, key, kl, tw, tl, 0, 0, &ref)) return 2;
        uint8_t *in = slurp(&n), *out = malloc(n);
        int r = op ? CCCryptorDecryptDataBlock(ref, iv, in, n, out) : CCCryptorEncryptDataBlock(ref, iv, in, n, out);
        if (r) return 3;
        fwrite(out, 1, n, stdout);
        return 0;
    }
    if (!strcmp(c, "gcm") && argc >= 7) {
        int dec = !strcmp(argv[2], "dec");
        uint8_t aad[256];
        size_t kl = unhex(argv[3], key), il = unhex(argv[4], iv), al = unhex(argv[5], aad), tl = (size_t)atoi(argv[6]);
        uint8_t *in = slurp(&n), *out = malloc(n + 1);
        int r;
        if (dec) { unhex(argv[7], tag); r = CCCryptorGCMOneshotDecrypt(0, key, kl, iv, il, aad, al, in, n, out, tag, tl); }
        else r = CCCryptorGCMOneshotEncrypt(0, key, kl, iv, il, aad, al, in, n, out, tag, tl);
        if (r) { fprintf(stderr, "gcm %d\n", r); return 3; }
        fwrite(out, 1, n, stdout);
        if (!dec) phex(stderr, tag, tl);
        return 0;
    }
    if (!strcmp(c, "hash") && argc == 3) {
        uint8_t *in = slurp(&n), md[64];
        const char *h = argv[2];
        size_t l = 0;
        if (!strcmp(h, "md2")) { CC_MD2(in, (uint32_t)n, md); l = 16; }
        else if (!strcmp(h, "md4")) { CC_MD4(in, (uint32_t)n, md); l = 16; }
        else if (!strcmp(h, "md5")) { CC_MD5(in, (uint32_t)n, md); l = 16; }
        else if (!strcmp(h, "sha1")) { CC_SHA1(in, (uint32_t)n, md); l = 20; }
        else if (!strcmp(h, "sha224")) { CC_SHA224(in, (uint32_t)n, md); l = 28; }
        else if (!strcmp(h, "sha256")) { CC_SHA256(in, (uint32_t)n, md); l = 32; }
        else if (!strcmp(h, "sha384")) { CC_SHA384(in, (uint32_t)n, md); l = 48; }
        else if (!strcmp(h, "sha512")) { CC_SHA512(in, (uint32_t)n, md); l = 64; }
        phex(stdout, md, l);
        return 0;
    }
    if (!strcmp(c, "hmac") && argc == 4) {
        static const int lens[] = {20, 16, 32, 48, 64, 28};
        uint32_t alg = (uint32_t)atoi(argv[2]);
        size_t kl = unhex(argv[3], key);
        uint8_t *in = slurp(&n), mac[64];
        CCHmac(alg, key, kl, in, n, mac);
        phex(stdout, mac, (size_t)lens[alg]);
        return 0;
    }
    if (!strcmp(c, "pbkdf") && argc == 7) {
        uint8_t salt[128], out[256];
        size_t sl = unhex(argv[4], salt), ol = (size_t)atoi(argv[6]);
        if (CCKeyDerivationPBKDF(2, argv[3], strlen(argv[3]), salt, sl, (uint32_t)atoi(argv[2]), (uint32_t)atoi(argv[5]), out, ol)) return 2;
        phex(stdout, out, ol);
        return 0;
    }
    if (!strcmp(c, "calibrate")) { printf("%u\n", CCCalibratePBKDF(2, 16, 16, 3, 32, 100)); return 0; }
    return 1;
}
