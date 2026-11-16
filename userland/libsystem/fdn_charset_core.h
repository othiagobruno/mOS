/*
 * mOS libSystem — conjunto de caracteres Unicode como lista ordenada de faixas [lo, hi] (fechadas) de
 * escalares 0…0x10FFFF. Base do NSCharacterSet/CFCharacterSet (fdn_charset.c); testada no host.
 */
#ifndef FDN_CHARSET_CORE_H
#define FDN_CHARSET_CORE_H
#include <stdint.h>

#define CR_MAX 0x10FFFFu
typedef struct { uint32_t lo, hi; } cr_t;
typedef struct { cr_t *r; uint32_t n, cap; } crs_t;

void crs_free(crs_t *s);
void crs_clear(crs_t *s);
int crs_copy(crs_t *d, const crs_t *s);
void crs_add(crs_t *s, uint32_t lo, uint32_t hi);
void crs_remove(crs_t *s, uint32_t lo, uint32_t hi);
int crs_has(const crs_t *s, uint32_t c);
void crs_invert(crs_t *s);
void crs_union(crs_t *s, const crs_t *o);
void crs_intersect(crs_t *s, const crs_t *o);
int crs_superset(const crs_t *s, const crs_t *o);
int crs_equal(const crs_t *a, const crs_t *b);
int crs_has_plane(const crs_t *s, uint32_t plane);
/* bitmap do NSCharacterSet: 8192 bytes do plano 0; depois, para cada plano com membros, 1 byte do plano + 8192 */
uint8_t *crs_bitmap(const crs_t *s, uint32_t *len);
void crs_from_bitmap(crs_t *s, const uint8_t *b, uint32_t len);
/* conjunto pelo padrão de propriedades da ICU ("[[:L:][:M:]]"); 0 se a ICU não estiver disponível */
int crs_from_icu_pattern(crs_t *s, const char *pattern);

#endif
