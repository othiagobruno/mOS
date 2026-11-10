/* Teste de host do formato OSSerialize. Não sobe o mOS.
 *   cc -O2 -DMOS_OSSER_HOST -Iuserland/libsystem tools/test-iocf.c userland/libsystem/osser.c -o /tmp/test-iocf
 */
#include <stdio.h>
int mos_osser_selftest(void);
int main(void) {
    int f = mos_osser_selftest();
    printf("%s\n", f ? "FALHOU" : "ok osser");
    return f;
}
