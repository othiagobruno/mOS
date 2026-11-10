#define _POSIX_C_SOURCE 200809L
/* Teste de host do selo do Keychain (userland/libsystem/kcseal.inc).
 * Não sobe a VM.
 *
 *   gcc -std=c11 -Wall -Wextra -O1 tools/kctest.c -o /tmp/kctest
 *   /tmp/kctest
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

typedef unsigned char u8;
typedef unsigned int u32;

#include "../userland/libsystem/kcseal.inc"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int hexeq(const u8 *p, const char *hex) {
    for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) {
        unsigned v;
        if (sscanf(hex + i, "%2x", &v) != 1) return 0;
        if (p[i / 2] != (u8)v) return 0;
    }
    return 1;
}

static int contains(const u8 *p, u32 n, const char *s) {
    size_t m = strlen(s);
    if (m > n) return 0;
    for (u32 i = 0; i + m <= n; i++)
        if (memcmp(p + i, s, m) == 0) return 1;
    return 0;
}

int main(void) {
    u8 dig[32];
    kc_sha256((const u8 *)"abc", 3, dig);
    CHECK(hexeq(dig, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    kc_sha256((const u8 *)"", 0, dig);
    CHECK(hexeq(dig, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));

    u8 key[20];
    memset(key, 0x0b, 20);
    kc_hmac(key, 20, (const u8 *)"Hi There", 8, dig);
    CHECK(hexeq(dig, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"));

    const char *plain = "mOS-keychain 1\nGEN 1 1 1 736f62726576697665\n";
    u8 filekey[32], other[32];
    kc_key_from("visitante", filekey);
    kc_key_from("outro", other);
    u8 *sealed = 0;
    u32 sn = 0;
    CHECK(kc_seal(filekey, (const u8 *)plain, (u32)strlen(plain), &sealed, &sn) == 0);
    CHECK(sealed && sn > 15 && memcmp(sealed, "mOS-keychain 2\n", 15) == 0);
    CHECK(!contains(sealed, sn, "sobrevive"));
    CHECK(!contains(sealed, sn, "mOS-keychain 1"));

    u8 *back = 0;
    u32 bn = 0;
    CHECK(kc_open(filekey, sealed, sn, &back, &bn) == 0);
    CHECK(back && bn == (u32)strlen(plain) && memcmp(back, plain, bn) == 0);
    CHECK(kc_open(other, sealed, sn, 0, 0) != 0);

    u8 badmac[32];
    memcpy(badmac, sealed + sn - 32, 32);
    sealed[sn - 1] ^= 0x01;
    u8 *rejected = 0;
    CHECK(kc_open(filekey, sealed, sn, &rejected, 0) != 0);
    CHECK(rejected == 0);
    memcpy(sealed + sn - 32, badmac, 32);
    u8 *again = 0;
    u32 again_n = 0;
    CHECK(kc_open(filekey, sealed, sn, &again, &again_n) == 0);
    CHECK(again && again_n == (u32)strlen(plain) && memcmp(again, plain, again_n) == 0);
    free(again);

    char login[96];
    setenv("USER", "visitante", 1);
    kc_login(login, sizeof login);
    CHECK(strcmp(login, "visitante") == 0);

    free(sealed);
    free(back);
    memset(filekey, 0, sizeof filekey);
    memset(other, 0, sizeof other);

    if (fails) {
        fprintf(stderr, "kctest: %d falhas em %d checagens\n", fails, checks);
        return 1;
    }
    printf("kctest: %d/%d PASS\n", checks, checks);
    return 0;
}
