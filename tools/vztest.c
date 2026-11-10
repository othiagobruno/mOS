/* gcc -std=c11 -Wall -Wextra -O1 tools/vztest.c -o /tmp/vztest && /tmp/vztest
 * Cobre o comportamento que o libSystem e o Virtualization.framework chamam.
 * Não sobe VM. */
#include <stdio.h>
#include <string.h>
#include "../userland/frameworks/Virtualization/vz_policy.h"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static const char *const vz_names[] = {
#define X(N) #N,
    VZ_CLASS_LIST(X)
#undef X
};

static int listed(const char *s) {
    size_t i;
    for (i = 0; i < sizeof vz_names / sizeof *vz_names; i++)
        if (!strcmp(vz_names[i], s)) return 1;
    return 0;
}

int main(void) {
    const char *msg;
    char line[128];
    int nfile = 0;
    FILE *f;

    CHECK(VZ_CLASS_COUNT == 41);
    CHECK((int)(sizeof vz_names / sizeof *vz_names) == VZ_CLASS_COUNT);
    CHECK(vz_policy_supported() == 0);
    msg = 0;
    CHECK(vz_policy_validate(&msg) == 0);
    CHECK(msg && strcmp(msg, "não suportado") == 0);
    msg = "ok";
    vz_policy_start(&msg);
    CHECK(msg && strcmp(msg, "não suportado") == 0);
    CHECK(vz_policy_state() == VZ_STATE_STOPPED);
    CHECK(vz_policy_can() == 0);
    msg = 0;
    CHECK(vz_policy_disk(&msg) == 0 && msg && strcmp(msg, "não suportado") == 0);
    msg = 0;
    CHECK(vz_policy_efi(&msg) == 0 && msg && strcmp(msg, "não suportado") == 0);
    CHECK(vz_policy_mac() == 0);
    CHECK(strcmp(VZ_ERROR_DOMAIN, "VZErrorDomain") == 0);
    CHECK(VZ_ERROR_CODE != 0);

    f = fopen("tools/testdata/vz-imports.txt", "r");
    if (!f) f = fopen("../tools/testdata/vz-imports.txt", "r");
    CHECK(f != 0);
    if (f) {
        while (fgets(line, sizeof line, f)) {
            char *s = line;
            size_t n;
            while (*s == ' ' || *s == '\t') s++;
            n = strlen(s);
            while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = 0;
            if (!n || s[0] == '#') continue;
            if (!strncmp(s, "_OBJC_CLASS_$_", 14)) s += 14;
            CHECK(listed(s));
            nfile++;
        }
        fclose(f);
    }
    CHECK(nfile == VZ_CLASS_COUNT);
    printf("vz: %d checks, %d fails, %d classes\n", checks, fails, VZ_CLASS_COUNT);
    return fails ? 1 : 0;
}
