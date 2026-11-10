#define _POSIX_C_SOURCE 200809L
/* Teste de host do mapa do MLDictionaryFeatureProvider (coreml_dict.inc).
 * A classe ObjC está em coreml.c. predictionFromFeatures continua stub.
 *
 *   gcc -std=c11 -Wall -Wextra -O1 tools/coremltest.c -o /tmp/coremltest
 *   /tmp/coremltest
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../userland/libsystem/coreml_dict.inc"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void) {
    ml_map m;
    memset(&m, 0, sizeof m);
    void *prev = (void *)1;
    CHECK(ml_put(&m, "", (void *)1, &prev) != 0);
    CHECK(prev == 0);
    CHECK(ml_put(&m, "voz", (void *)(size_t)11, 0) == 0);
    CHECK(ml_put(&m, "n", (void *)(size_t)7, 0) == 0);
    CHECK(ml_get(&m, "voz") == (void *)(size_t)11);
    CHECK(ml_get(&m, "ausente") == 0);
    CHECK(ml_name_at(&m, 0) && strcmp(ml_name_at(&m, 0), "voz") == 0);
    CHECK(ml_name_at(&m, 1) && strcmp(ml_name_at(&m, 1), "n") == 0);
    CHECK(ml_name_at(&m, 2) == 0);
    prev = 0;
    CHECK(ml_put(&m, "voz", (void *)(size_t)99, &prev) == 0);
    CHECK(prev == (void *)(size_t)11);
    CHECK(ml_get(&m, "voz") == (void *)(size_t)99);
    CHECK(m.n == 2);
    ml_free(&m);
    CHECK(m.n == 0 && m.e == 0);
    CHECK(ml_get(&m, "voz") == 0);

    if (fails) {
        fprintf(stderr, "coremltest: %d falhas em %d checagens\n", fails, checks);
        return 1;
    }
    printf("coremltest: %d/%d PASS\n", checks, checks);
    return 0;
}
