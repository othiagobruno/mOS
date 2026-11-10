#define _POSIX_C_SOURCE 200809L
/* gcc -std=c11 -Wall -Wextra -O1 tools/sktest.c -o /tmp/sktest && /tmp/sktest
 * Teste de host da política do StoreKit. Não chama a App Store e não valida recibo. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "../userland/libsystem/storekit_stub.inc"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void) {
    char err[160];
    CHECK(sk_policy_can_pay() == 0);
    CHECK(sk_policy_product_count() == 0);
    CHECK(sk_policy_purchased() == 0);
    CHECK(sk_policy_fail_state() == 2);
    CHECK(sk_policy_receipt_len() == 0);
    CHECK(sk_policy_fail_state() != 1);
    CHECK(sk_policy_fail_state() != 3);
    err[0] = 0;
    CHECK(sk_verify("recibo", 6, err, sizeof err) == -1);
    CHECK(strcmp(err, "recibo não verificado: o mOS não valida recibo da App Store nem fala com a Apple") == 0);
    printf("%d checks, %d fails\n", checks, fails);
    return fails ? 1 : 0;
}
