/*
 * mOS libSystem — algoritmos de texto do NSString em UTF-16, sobre a ICU (testados no host por
 * tools/fdn_text_test.c). Índices e tamanhos são em unidades UTF-16, como no NSString.
 */
#ifndef FDN_TEXT_CORE_H
#define FDN_TEXT_CORE_H
#include "fdn_icu.h"

/* NSStringCompareOptions */
enum { TX_CASE = 1, TX_LITERAL = 2, TX_BACKWARDS = 4, TX_ANCHORED = 8, TX_NUMERIC = 64, TX_DIACRITIC = 128,
       TX_WIDTH = 256, TX_FORCED = 512, TX_REGEX = 1024 };

/* busca needle em hay[0..hn); devolve 1 e a faixa achada. Com TX_REGEX, needle é o padrão (uregex). */
int tx_search(const UChar *hay, int32_t hn, const UChar *needle, int32_t nn, unsigned opts, int32_t *loc, int32_t *len);
/* -1/0/1. locale NULL = ordem por ponto de código com as opções; com locale = collation da ICU */
int tx_compare(const UChar *a, int32_t an, const UChar *b, int32_t bn, unsigned opts, const char *locale);
/* caixa: 0 maiúsculas, 1 minúsculas, 2 título (palavras); malloc, *on = tamanho */
UChar *tx_case(int kind, const UChar *s, int32_t n, const char *locale, int32_t *on);
/* dobra (stringByFoldingWithOptions:): caixa, acentos, largura */
UChar *tx_fold(const UChar *s, int32_t n, unsigned opts, int32_t *on);
/* sequência composta (grafema) que contém idx */
void tx_composed(const UChar *s, int32_t n, int32_t idx, int32_t *st, int32_t *len);
/* início, fim e fim do conteúdo da linha (ou parágrafo) que contém [loc, loc+len) */
void tx_line(const UChar *s, int32_t n, int32_t loc, int32_t len, int para, int32_t *start, int32_t *end, int32_t *cend);
/* fronteiras da ICU: kind 1 palavra, 3 frase; chama fn(início, fim) para cada segmento (palavras sem espaço) */
int tx_segments(const UChar *s, int32_t n, int kind, const char *locale, int (*fn)(void *, int32_t, int32_t), void *ctx);
/* transliteração da ICU (NSStringTransform* são ids da ICU) */
UChar *tx_transform(const UChar *s, int32_t n, const char *id, int reverse, int32_t *on);

#endif
