#define _POSIX_C_SOURCE 200809L
/* gcc -std=c11 -Wall -Wextra -O1 tools/tcctest.c -o /tmp/tcctest && /tmp/tcctest */
#define TCC_HOST 1
#include "../userland/libsystem/tcc_core.inc"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void) {
    char dir[] = "/tmp/mos-tcc-test-XXXXXX";
    if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
    char path[512];
    snprintf(path, sizeof path, "%s/decisions.txt", dir);
    const char *b = "com.grok.bot";
    const char *sc = "kTCCServiceScreenCapture";
    const char *ax = "kTCCServiceAccessibility";
    CHECK(tcc_preflight(path, b, sc) == 0);
    CHECK(tcc_state_at(path, b, sc) == TCC_MISSING);
    CHECK(tcc_request(path, b, sc) == 0);
    CHECK(tcc_state_at(path, b, sc) == TCC_PENDING);
    CHECK(tcc_preflight(path, b, sc) == 0);
    CHECK(tcc_request(path, b, sc) == 0);
    CHECK(tcc_state_at(path, b, sc) == TCC_PENDING);
    CHECK(tcc_decide(path, b, sc, 1) == 0);
    CHECK(tcc_preflight(path, b, sc) == 1);
    CHECK(tcc_request(path, b, sc) == 1);
    CHECK(tcc_decide(path, b, ax, 0) == 0);
    CHECK(tcc_preflight(path, b, ax) == 0);
    CHECK(tcc_request(path, b, ax) == 0);
    CHECK(tcc_state_at(path, b, ax) == TCC_DENIED);
    CHECK(tcc_request(path, "../x", sc) == 0);
    CHECK(tcc_decide(path, "a/b", sc, 1) == -1);
    /* outro "processo": o arquivo guarda a concessão da tela */
    CHECK(tcc_preflight(path, b, sc) == 1);
    CHECK(tcc_preflight(path, "com.outro", sc) == 0);
    char cmd[640];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
    if (system(cmd) != 0) fprintf(stderr, "aviso: não apaguei %s\n", dir);
    if (fails) { fprintf(stderr, "tcctest: %d falhas em %d\n", fails, checks); return 1; }
    printf("tcctest: %d/%d PASS\n", checks, checks);
    return 0;
}
