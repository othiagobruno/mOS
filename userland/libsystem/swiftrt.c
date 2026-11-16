/*
 * mOS libSystem — a parte em C do runtime que a biblioteca padrão Swift (Embedded e o caminho sem ObjC) importa:
 * normalização (NFC/NFD), maiúsculas/minúsculas/título, quebra de grafema e de palavra, emoji estendido.
 * As tabelas são as do próprio Swift (userland/third_party/swift-unicode, Apache 2.0); as buscas repetem
 * stdlib/public/stubs/Unicode (os .cpp de lá). A libswiftCore real tem a própria cópia interna e não usa estas.
 */
#ifdef MOS_HOST_TEST
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int32_t i32; typedef uint64_t u64; typedef int64_t i64;
#else
#include "foundation.h"
#include "libc.h"
#endif
#include "../third_party/swift-unicode/NormalizationData.h"
#include "../third_party/swift-unicode/CaseMappingData.h"
#include "../third_party/swift-unicode/GraphemeData.h"
#include "../third_party/swift-unicode/EmojiData.h"
#include "../third_party/swift-unicode/WordData.h"

#define NO_IDX ((long)0x7fffffffffffffffL)

/* ---------------------------------------------------------------- buscas (UnicodeData.cpp) */
static u32 scramble(u32 s) {
    s *= 0xCC9E2D51u;
    s = (s << 15) | (s >> 17);
    s *= 0x1B873593u;
    return s;
}
static u32 mph_hash(u32 scalar, u32 level, u32 seed) {
    u32 h = seed;
    h ^= scramble(scalar);
    h = (h << 13) | (h >> 19);
    h = h * 5 + 0xE6546B64u;
    h ^= scramble(level);
    h = (h << 13) | (h >> 19);
    h = h * 5 + 0xE6546B64u;
    h ^= 8;
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h % level;
}
/* índice em tabela de hash perfeito mínimo (níveis de bit arrays com ranks a cada 512 bits) */
static long mph_idx(u32 scalar, long levels, const __swift_uint64_t *const *keys, const __swift_uint16_t *const *ranks,
                    const __swift_uint16_t *sizes) {
    for (int i = 0; i != levels; i++) {
        const __swift_uint64_t *ba = keys[i];
        u64 idx = mph_hash(scalar, sizes[i], (u32)i);
        u64 word = ba[idx / 64];
        u64 mask = 1ull << (idx % 64);
        if (word & mask) {
            long rank = ranks[i][idx / 512];
            for (u64 j = (idx / 64) & ~7ull; j != idx / 64; j++) rank += __builtin_popcountll(ba[j]);
            if (idx % 64 > 0) rank += __builtin_popcountll(word << (64 - (idx % 64)));
            return rank;
        }
    }
    return 0;
}
/* índice no array de dados de uma propriedade guardada em "quick look" + bit arrays de 272 escalares por bloco */
static long bitarray_idx(u32 scalar, const __swift_uint64_t *ba, const __swift_uint16_t *ranks) {
    const u32 chunk = 0x110000 / 64 / 64;
    u32 base = scalar / chunk, idx = base / 64, bit = base % 64;
    u64 qsize = ba[0];
    if ((u64)idx > qsize - 1) return NO_IDX;
    u64 quick = ba[idx + 1];
    if ((quick & (1ull << bit)) == 0) return NO_IDX;
    u64 crank = ranks[idx];
    if (bit != 0) crank += (u64)__builtin_popcountll(quick << (64 - bit));
    const __swift_uint64_t *cba = ba + 1 + qsize + crank * 5;
    u32 overall = scalar - base * chunk, sbit = overall % 64, sword = overall / 64;
    u64 cw = cba[sword];
    if ((cw & (1ull << sbit)) == 0) return NO_IDX;
    long srank = ranks[qsize + crank * 5 + sword];
    if (sbit != 0) srank += __builtin_popcountll(cw << (64 - sbit));
    return (long)(cba[4] >> 16) + srank;
}

/* ---------------------------------------------------------------- normalização (UnicodeNormalization.cpp) */
u16 _swift_stdlib_getNormData(u32 scalar) {
    if (scalar < 0xC0) return 0;            /* ASCII e Latin-1 inicial: sem propriedades */
    long i = bitarray_idx(scalar, _swift_stdlib_normData, _swift_stdlib_normData_ranks);
    if (i == NO_IDX) return 0;
    return _swift_stdlib_normData_data[_swift_stdlib_normData_data_indices[i]];
}
const u8 *const _swift_stdlib_nfd_decompositions = _swift_stdlib_nfd_decomp;
u32 _swift_stdlib_getDecompositionEntry(u32 scalar) {
    long i = mph_idx(scalar, NFD_DECOMP_LEVEL_COUNT, _swift_stdlib_nfd_decomp_keys, _swift_stdlib_nfd_decomp_ranks,
                     _swift_stdlib_nfd_decomp_sizes);
    return _swift_stdlib_nfd_decomp_indices[i];
}
u32 _swift_stdlib_getComposition(u32 x, u32 y) {
    long ci = mph_idx(y, NFC_COMP_LEVEL_COUNT, _swift_stdlib_nfc_comp_keys, _swift_stdlib_nfc_comp_ranks,
                      _swift_stdlib_nfc_comp_sizes);
    const u32 *arr = _swift_stdlib_nfc_comp_indices[ci];
    u32 realY = (arr[0] << 11) >> 11;
    if (y != realY) return 0xFFFFFFFFu;
    u32 count = arr[0] >> 21, low = 1, high = count - 1;
    while (high >= low) {
        u32 idx = low + (high - low) / 2;
        u32 e = arr[idx];
        u32 lower = (e << 15) >> 15;
        int neg = (int)(e >> 31);
        u32 rc = (e << 1) >> 18;
        if (neg) rc = (u32)-(i32)rc;
        u32 composed = lower + rc;
        if (x == lower) return composed;
        if (x > lower) { low = idx + 1; continue; }
        if (idx == 0) break;
        high = idx - 1;
    }
    return 0xFFFFFFFFu;
}

/* ---------------------------------------------------------------- maiúsculas/minúsculas/título (UnicodeScalarProps.cpp) */
/* mapping: 0 maiúscula, 1 minúscula, 2 título; devolve a distância até o escalar mapeado (0 = sem mapeamento simples) */
i32 _swift_stdlib_getMapping(u32 c, u8 mapping) {
    long i = bitarray_idx(c, _swift_stdlib_mappings, _swift_stdlib_mappings_ranks);
    if (i == NO_IDX) return 0;
    u32 m = _swift_stdlib_mappings_data_indices[i];
    u8 mi;
    switch (mapping) {
    case 0: mi = (u8)(m & 0xFF); break;
    case 1: mi = (u8)((m & 0xFF00) >> 8); break;
    case 2: mi = (u8)((m & 0xFF0000) >> 16); break;
    default: return 0;
    }
    if (mi == 0xFF) return 0;
    return _swift_stdlib_mappings_data[mi];
}
/* mapeamento especial (ß -> SS, ligaduras...): UTF-8 e o comprimento; NULL se não houver */
const u8 *_swift_stdlib_getSpecialMapping(u32 c, u8 mapping, i64 *len) {
    long i = bitarray_idx(c, _swift_stdlib_special_mappings, _swift_stdlib_special_mappings_ranks);
    if (i == NO_IDX) return 0;
    const u8 *up = _swift_stdlib_special_mappings_data + _swift_stdlib_special_mappings_data_indices[i];
    const u8 *lo = up + 1 + *up;
    const u8 *ti = lo + 1 + *lo;
    switch (mapping) {
    case 0: *len = *up; return up + 1;
    case 1: *len = *lo; return lo + 1;
    case 2: *len = *ti; return ti + 1;
    default: return 0;
    }
}

/* ---------------------------------------------------------------- grafemas, emoji, palavras */
u8 _swift_stdlib_getGraphemeBreakProperty(u32 scalar) {
    int index = 1;
    while (index < GRAPHEME_BREAK_DATA_COUNT) {
        u32 e = _swift_stdlib_graphemeBreakProperties[index];
        u32 lower = (e << 11) >> 11, upper = lower + ((e << 3) >> 24);
        u8 v = (u8)(e >> 29);
        if (v == 5) upper = lower + ((e << 2) >> 23);   /* extendedPictographic: um bit a mais no tamanho */
        if (scalar < lower) index = 2 * index;
        else if (scalar <= upper) return v;
        else index = 2 * index + 1;
    }
    return 0xFF;
}
_Bool _swift_stdlib_isInCB_Consonant(u32 scalar) {
    return bitarray_idx(scalar, _swift_stdlib_InCB_Consonant, _swift_stdlib_InCB_Consonant_ranks) != NO_IDX;
}
_Bool _swift_stdlib_isExtendedPictographic(u32 scalar) {
    if (scalar < 0xA9) return 0;
    return bitarray_idx(scalar, _swift_stdlib_emojiData, _swift_stdlib_emojiData_ranks) != NO_IDX;
}
u8 _swift_stdlib_getWordBreakProperty(u32 scalar) {
    int index = 1;
    while (index < WORD_BREAK_DATA_COUNT) {
        u32 e = _swift_stdlib_words[index];
        u32 lower = (e << 11) >> 11, upper = lower + (e >> 21) - 1;
        if (scalar < lower) index = 2 * index;
        else if (scalar <= upper) return _swift_stdlib_words_data[index];
        else index = 2 * index + 1;
    }
    return 0xFF;
}
