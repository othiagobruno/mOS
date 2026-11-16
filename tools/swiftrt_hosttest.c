/* Teste de host das tabelas Unicode do libSystem (userland/libsystem/swiftrt.c). cc -DMOS_HOST_TEST ... */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
uint16_t _swift_stdlib_getNormData(uint32_t);
uint32_t _swift_stdlib_getDecompositionEntry(uint32_t);
uint32_t _swift_stdlib_getComposition(uint32_t, uint32_t);
extern const uint8_t *const _swift_stdlib_nfd_decompositions;
int32_t _swift_stdlib_getMapping(uint32_t, uint8_t);
const uint8_t *_swift_stdlib_getSpecialMapping(uint32_t, uint8_t, int64_t *);
uint8_t _swift_stdlib_getGraphemeBreakProperty(uint32_t);
_Bool _swift_stdlib_isExtendedPictographic(uint32_t);
uint8_t _swift_stdlib_getWordBreakProperty(uint32_t);
static int fails, total;
#define CHECK(c, ...) do { total++; if (c) printf("  ok    "); else { fails++; printf("  FALHA "); } printf(__VA_ARGS__); printf("\n"); } while (0)
int main(void) {
    printf("swiftrt_hosttest: Unicode do libSystem\n");
    CHECK(_swift_stdlib_getComposition('e', 0x301) == 0xE9, "e + acento agudo compõe em é (0x%X)", _swift_stdlib_getComposition('e', 0x301));
    CHECK(_swift_stdlib_getComposition('c', 0x327) == 0xE7, "c + cedilha compõe em ç");
    CHECK(_swift_stdlib_getComposition('x', 0x301) == 0xFFFFFFFFu, "x + acento não compõe");
    uint16_t nd = _swift_stdlib_getNormData(0x301);
    CHECK((nd >> 3) == 230 || nd != 0, "U+0301 tem dados de normalização (0x%X)", nd);
    CHECK(_swift_stdlib_getNormData('a') == 0, "ASCII sem dados");
    uint32_t de = _swift_stdlib_getDecompositionEntry(0xE9);
    /* como o UnicodeData.swift: escalar nos 18 bits baixos (conferência), índice nos 14 altos; o 1º byte é o tamanho em UTF-8 */
    const uint8_t *d = _swift_stdlib_nfd_decompositions + (de >> 18);
    printf("        é: entrada 0x%X, bytes %02X %02X %02X %02X\n", de, d[0], d[1], d[2], d[3]);
    CHECK(((de << 14) >> 14) == 0xE9 && d[0] == 3 && d[1] == 'e' && d[2] == 0xCC && d[3] == 0x81, "é decompõe em e + U+0301 (UTF-8)");
    CHECK(_swift_stdlib_getMapping('a', 0) == -32 && _swift_stdlib_getMapping('A', 1) == 32, "a/A maiúscula e minúscula");
    CHECK(_swift_stdlib_getMapping(0xE7, 0) == -32, "ç -> Ç");
    CHECK(_swift_stdlib_getMapping(0x3B1, 0) == -32, "α -> Α");
    CHECK(_swift_stdlib_getMapping(0x0561, 0) == -48, "armênio ա -> Ա (fora do que o C antigo cobria)");
    int64_t n = 0;
    const uint8_t *sp = _swift_stdlib_getSpecialMapping(0xDF, 0, &n);
    CHECK(sp && n == 2 && sp[0] == 'S' && sp[1] == 'S', "ß maiúscula é SS");
    CHECK(_swift_stdlib_getGraphemeBreakProperty(0x0D) != 0xFF, "CR tem propriedade de quebra de grafema");
    CHECK(_swift_stdlib_getGraphemeBreakProperty('a') == 0xFF, "'a' é .any");
    CHECK(_swift_stdlib_isExtendedPictographic(0x1F600) && !_swift_stdlib_isExtendedPictographic('a'), "😀 é pictográfico estendido");
    CHECK(_swift_stdlib_getWordBreakProperty('a') != 0xFF, "'a' tem propriedade de quebra de palavra (ALetter)");
    printf("swiftrt_hosttest: %d de %d ok\n", total - fails, total);
    return fails;
}
