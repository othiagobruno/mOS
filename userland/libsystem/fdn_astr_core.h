/*
 * mOS libSystem — trechos de atributos do NSAttributedString: lista de (tamanho em UTF-16, valor). Trechos
 * vizinhos com valores iguais se juntam, e não fica trecho vazio. Testado no host por tools/fdn_astr_test.c.
 */
#ifndef FDN_ASTR_CORE_H
#define FDN_ASTR_CORE_H
#include <stdint.h>
#include <stddef.h>

typedef struct { uint64_t *len; void **val; size_t n, cap; } runs_t;
typedef struct {
    void *(*retain)(void *v);
    void (*release)(void *v);
    int (*equal)(void *a, void *b);
} runs_cb;

void runs_free(runs_t *r, const runs_cb *cb);
uint64_t runs_total(const runs_t *r);
/* valor do trecho que contém idx (< total) e a faixa dele */
void *runs_at(const runs_t *r, uint64_t idx, uint64_t *start, uint64_t *len);
/* troca o valor de [loc, loc+len) */
void runs_set(runs_t *r, const runs_cb *cb, uint64_t loc, uint64_t len, void *val);
/* apaga [loc, loc+len) e põe no lugar newlen unidades com val */
void runs_replace(runs_t *r, const runs_cb *cb, uint64_t loc, uint64_t len, uint64_t newlen, void *val);

#endif
