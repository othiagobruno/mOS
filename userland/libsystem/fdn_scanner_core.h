/*
 * mOS libSystem — núcleo do NSScanner em UTF-16 (testado no host por tools/fdn_scanner_test.c).
 * Posições em unidades UTF-16, como o scanLocation do macOS. Cada leitura pula antes os caracteres de
 * "skip" (charactersToBeSkipped; NULL = não pula nada) e, se falhar, deixa a posição onde estava.
 */
#ifndef FDN_SCANNER_CORE_H
#define FDN_SCANNER_CORE_H
#include "fdn_icu.h"

/* pertence ao conjunto? (escalar Unicode inteiro: os pares substitutos já vêm juntos) */
typedef int (*scn_member_fn)(void *ctx, uint32_t c);

typedef struct {
    const UChar *s;
    int32_t n, pos;
    scn_member_fn skip;     /* NULL: charactersToBeSkipped = nil */
    void *skip_ctx;
    int case_sensitive;     /* NO por padrão, como no macOS */
    UChar dec_sep;          /* separador decimal do locale ('.' sem locale) */
} scn_t;

/* posição depois de pular os caracteres de skip, sem mexer em pos */
int32_t scn_skipped(const scn_t *sc);
int scn_at_end(const scn_t *sc);
/* faixa achada em start e len (UTF-16) e pos avançada; 0 = falhou e pos não mudou */
int scn_string(scn_t *sc, const UChar *str, int32_t n, int32_t *start, int32_t *len);
int scn_upto_string(scn_t *sc, const UChar *str, int32_t n, int32_t *start, int32_t *len);
int scn_chars(scn_t *sc, scn_member_fn set, void *ctx, int32_t *start, int32_t *len);
int scn_upto_chars(scn_t *sc, scn_member_fn set, void *ctx, int32_t *start, int32_t *len);
/* inteiros decimais: estouro satura em min/max e consome os dígitos todos, como no macOS */
int scn_i64(scn_t *sc, long long lo, long long hi, long long *out);
int scn_u64(scn_t *sc, unsigned long long *out);
/* hexadecimal com "0x"/"0X" opcional; estouro satura em max */
int scn_hex(scn_t *sc, unsigned long long max, unsigned long long *out);
int scn_double(scn_t *sc, double *out);
/* ponto flutuante hexadecimal do C99 ("0x1.8p3"); também aceita o decimal */
int scn_hex_double(scn_t *sc, double *out);
/* texto ASCII do número a partir de pos (depois do skip) para o parser do NSDecimal; devolve o tamanho */
int32_t scn_ascii_prefix(const scn_t *sc, char *buf, int32_t cap, int32_t *from);

#endif
