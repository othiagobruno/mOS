/*
 * mOS libSystem — o que a biblioteca padrão Swift (Embedded) importa da parte em C do runtime.
 * A stdlib Embedded traz quase tudo em Swift; faltam as tabelas de normalização Unicode
 * (stdlib/public/stubs/Unicode). Aqui elas respondem "já normalizado / sem composição": comparações
 * de String tratam como iguais apenas sequências idênticas, o que basta para texto em NFC
 * (o caso comum, inclusive português digitado ou vindo de arquivos).
 */
#include "foundation.h"
#include "libc.h"

static const u8 empty_table[16];
const u8 *const _swift_stdlib_nfd_decompositions = empty_table;

/* dados de normalização: 0 = classe combinatória 0, já em NFC/NFD */
u16 _swift_stdlib_getNormData(u32 scalar) { (void)scalar; return 0; }
/* sem decomposição */
u32 _swift_stdlib_getDecompositionEntry(u32 scalar) { (void)scalar; return 0; }
/* sem composição canônica */
u32 _swift_stdlib_getComposition(u32 x, u32 y) { (void)x; (void)y; return 0xFFFFFFFFu; }

/* maiúsculas/minúsculas (mapping: 0 = maiúscula, 1 = minúscula, 2 = título): devolve a distância até o
 * escalar mapeado (0 = sem mapeamento). Cobre ASCII, Latin-1 (acentos do português), Latin Extended-A,
 * grego e cirílico básicos. */
static int is_upper(u32 c) {
    return (c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7) || (c >= 0x391 && c <= 0x3A9 && c != 0x3A2) || (c >= 0x410 && c <= 0x42F) || (c >= 0x400 && c <= 0x40F);
}
static int is_lower(u32 c) {
    return (c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7) || (c >= 0x3B1 && c <= 0x3C9 && c != 0x3C2) || (c >= 0x430 && c <= 0x44F) || (c >= 0x450 && c <= 0x45F);
}
i32 _swift_stdlib_getMapping(u32 c, u8 mapping) {
    int up = mapping != 1;
    if (c >= 0x100 && c <= 0x17F && c != 0x130 && c != 0x131 && c != 0x138 && c != 0x149 && c != 0x17F) {   /* Latin Extended-A: pares */
        int odd_upper = (c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E);
        int is_up = odd_upper ? (c & 1) : !(c & 1);
        if (up && !is_up) return -1;
        if (!up && is_up) return 1;
        return 0;
    }
    if (c == 0xFF && up) return 0x178 - 0xFF;
    if (c == 0x178 && !up) return 0xFF - 0x178;
    if (up && is_lower(c)) {
        if (c >= 0x450 && c <= 0x45F) return -80;
        if (c >= 0x430) return -32;
        if (c >= 0x3B1) return -32;
        return -32;
    }
    if (!up && is_upper(c)) {
        if (c >= 0x400 && c <= 0x40F) return 80;
        return 32;
    }
    return 0;
}
/* ß → SS na maiúscula; os demais não têm mapeamento especial */
const u8 *_swift_stdlib_getSpecialMapping(u32 c, u8 mapping, i64 *len) {
    static const u8 ss[] = {'S', 'S'};
    if (c == 0xDF && mapping == 0) { *len = 2; return ss; }
    *len = 0;
    return 0;
}