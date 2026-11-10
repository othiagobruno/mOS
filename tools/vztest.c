/* gcc -std=c11 -Wall -Wextra -O1 tools/vztest.c -o /tmp/vztest && /tmp/vztest
 * A política cabe num header. Não sobe VM. */
#include <stdio.h>
#include <string.h>
#include "../userland/frameworks/Virtualization/vz_policy.h"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void) {
    CHECK(vz_is_supported() == 0);
    CHECK(strcmp(VZ_UNSUPPORTED_MSG, "não suportado") == 0);
    CHECK(VZ_ERROR_CODE != 0);
    printf("vz: %d checks, %d fails\n", checks, fails);
    return fails ? 1 : 0;
}
