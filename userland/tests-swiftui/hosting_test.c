/* Prova do hosting SwiftUI no host (sem QEMU e sem o binário do WhatsApp).
 * Compila a mesma cena que o libSystem exporta: layout, pixels, clique, rolagem,
 * indicador e edição do TextEditor (caret, seleção, backspace, UTF-8).
 *   clang -std=c11 -Wall -Wextra -Werror -I userland/sdk/swift/MOSBridge \
 *     userland/tests-swiftui/hosting_test.c -o /tmp/hosting_test && /tmp/hosting_test
 */
#include "mos_sui_geom.h"
#include <stdio.h>

int main(void) {
    char err[8192];
    int fails = mos_sui_selftest_run(err, (int)sizeof err);
    fputs(err, stdout);
    if (fails) {
        fprintf(stderr, "RESULT FAIL %d\n", fails);
        return 1;
    }
    printf("RESULT PASS\n");
    return 0;
}
