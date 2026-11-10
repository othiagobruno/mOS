#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#define KC_HOST 1
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
#include <sys/stat.h>
#include <errno.h>

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

    char base[] = "/tmp/kctest-XXXXXX";
    char *root = mkdtemp(base);
    CHECK(root != 0);
    if (root) {
        char d1[128], d2[128], k1[160], k2[160], kc[160];
        snprintf(d1, sizeof d1, "%s/a/Library/Keychains", root);
        snprintf(d2, sizeof d2, "%s/b/Library/Keychains", root);
        snprintf(k1, sizeof k1, "%s/mos-keychain.key", d1);
        snprintf(k2, sizeof k2, "%s/mos-keychain.key", d2);
        u8 a[32], b[32], again[32];
        CHECK(kc_key_create(k1, a) == 0);
        struct stat st;
        CHECK(lstat(k1, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & 0777) == 0600 && st.st_size == 32);
        CHECK(lstat(d1, &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & 0777) == 0700);
        CHECK(kc_key_read(k1, again) == 0 && memcmp(a, again, 32) == 0);
        CHECK(kc_key_ensure(k1, b) == 0 && memcmp(a, b, 32) == 0);
        CHECK(kc_key_create(k2, b) == 0 && memcmp(a, b, 32) != 0);
        u8 derived[32];
        kc_key_from("visitante", derived);
        CHECK(memcmp(a, derived, 32) != 0);
        memset(derived, 0, 32);

        CHECK(chmod(k1, 0644) == 0);
        u8 refused[32];
        memset(refused, 0x5a, 32);
        CHECK(kc_key_read(k1, refused) != 0);
        CHECK(kc_key_ensure(k1, refused) != 0);
        int z = 1;
        for (int i = 0; i < 32; i++) if (refused[i]) z = 0;
        CHECK(z == 1);
        CHECK(chmod(k1, 0600) == 0);

        char linkp[160];
        snprintf(linkp, sizeof linkp, "%s/link.key", d1);
        CHECK(symlink(k1, linkp) == 0);
        CHECK(kc_key_read(linkp, refused) != 0);

        char shortp[160];
        snprintf(shortp, sizeof shortp, "%s/short.key", d1);
        int sfd = open(shortp, O_WRONLY | O_CREAT | O_EXCL, 0600);
        CHECK(sfd >= 0);
        CHECK(write(sfd, "0123456789abcdef", 16) == 16);
        close(sfd);
        CHECK(chmod(d1, 0700) == 0);
        CHECK(kc_key_read(shortp, refused) != 0);

        const char *plain2 = "mOS-keychain 1\nGEN 7 1 1 73656e6861\n";
        u8 oldk[32], newk[32];
        kc_key_from("visitante", oldk);
        CHECK(kc_key_read(k2, newk) == 0);
        u8 *mig = 0;
        u32 mn = 0;
        CHECK(kc_seal(oldk, (const u8 *)plain2, (u32)strlen(plain2), &mig, &mn) == 0);
        snprintf(kc, sizeof kc, "%s/mos-keychain.txt", d2);
        int kfd = open(kc, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        CHECK(kfd >= 0 && write(kfd, mig, mn) == (ssize_t)mn);
        if (kfd >= 0) close(kfd);
        free(mig);
        CHECK(kc_migrate_file(kc, oldk, newk) == 0);
        u8 *raw = 0;
        u32 rn = 0;
        CHECK(kc_read_all(kc, &raw, &rn) == 0);
        u8 *back2 = 0;
        u32 bn2 = 0;
        CHECK(kc_open(newk, raw, rn, &back2, &bn2) == 0);
        CHECK(back2 && bn2 == (u32)strlen(plain2) && memcmp(back2, plain2, bn2) == 0);
        CHECK(kc_open(oldk, raw, rn, 0, 0) != 0);
        free(back2);
        free(raw);

        u8 *stuck = 0;
        u32 stuckn = 0;
        CHECK(kc_seal(oldk, (const u8 *)plain2, (u32)strlen(plain2), &stuck, &stuckn) == 0);
        char bad[160];
        snprintf(bad, sizeof bad, "%s/mos-keychain.txt", d1);
        kfd = open(bad, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        CHECK(kfd >= 0 && write(kfd, stuck, stuckn) == (ssize_t)stuckn);
        if (kfd >= 0) close(kfd);
        CHECK(chmod(d1, 0500) == 0);
        u8 before[4096];
        CHECK(stuckn < sizeof before);
        memcpy(before, stuck, stuckn);
        CHECK(kc_migrate_file(bad, oldk, newk) != 0);
        CHECK(chmod(d1, 0700) == 0);
        u8 *after = 0;
        u32 an = 0;
        CHECK(kc_read_all(bad, &after, &an) == 0);
        CHECK(an == stuckn && memcmp(after, before, an) == 0);
        u8 *still = 0;
        u32 stilln = 0;
        CHECK(kc_open(oldk, after, an, &still, &stilln) == 0);
        CHECK(still && stilln == (u32)strlen(plain2) && memcmp(still, plain2, stilln) == 0);
        free(still);
        free(after);
        free(stuck);
        u8 e1[32], e2[32];
        int ediff = 0, i;
        CHECK(kc_hwentropy(e1, 32) == 0);
        CHECK(kc_hwentropy(e2, 32) == 0);
        for (i = 0; i < 32; i++) if (e1[i] != e2[i]) ediff = 1;
        CHECK(ediff == 1);
        memset(e1, 0xab, 32);
        kc_entropy_fail = 1;
        CHECK(kc_hwentropy(e1, 32) != 0);
        ediff = 1;
        for (i = 0; i < 32; i++) if (e1[i]) ediff = 0;
        CHECK(ediff == 1);
        {
            char failkey[160];
            snprintf(failkey, sizeof failkey, "%s/noent.key", d1);
            CHECK(kc_key_create(failkey, e2) != 0);
            CHECK(lstat(failkey, &st) != 0);
        }
        kc_entropy_fail = 0;
        memset(e1, 0, 32);
        memset(e2, 0, 32);
        memset(a, 0, 32);
        memset(b, 0, 32);
        memset(again, 0, 32);
        memset(oldk, 0, 32);
        memset(newk, 0, 32);
    }

    if (fails) {
        fprintf(stderr, "kctest: %d falhas em %d checagens\n", fails, checks);
        return 1;
    }
    printf("kctest: %d/%d PASS\n", checks, checks);
    return 0;
}
