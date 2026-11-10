/* Teste de host do demangler e do índice de requisito. Não é QEMU. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

const void *mos_demangle_type(const char *s, size_t n, const void *const *args, size_t *used);

int main(void) {
    size_t used = 0;
    const void *t = mos_demangle_type("yt", 2, 0, &used);
    if (!t || used != 2) return 1;
    t = mos_demangle_type("s5UInt8V", 8, 0, &used);
    if (!t || used != 8) return 2;
    t = mos_demangle_type("s5ErrorP", 8, 0, &used);
    if (!t || used != 8) return 3;
    const char arg = 1;
    const void *args[] = { &arg };
    t = mos_demangle_type("x", 1, args, &used);
    if (t != &arg || used != 1) return 4;
    t = mos_demangle_type("Sqs5UInt8V", 10, 0, &used);
    if (!t || used != 10) return 5;
    if (*(const uint64_t *)t != 0x202) return 6;
    t = mos_demangle_type("$ss5UInt8V", 10, 0, &used);
    if (!t || used != 10) return 7;
    puts("ok");
    return 0;
}
