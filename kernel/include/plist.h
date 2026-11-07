/* mOS — property lists (XML e binário "bplist00") como árvore: Info.plist dos apps e as preferências do
 * Launch Services. Só leitura de dicionários, arrays, strings, inteiros e booleanos (reais, datas e dados
 * viram nós sem valor). Tudo é alocado num bloco por árvore: pl_free(raiz) libera tudo. */
#pragma once
#include "kernel.h"

enum { PL_NULL, PL_DICT, PL_ARRAY, PL_STRING, PL_INT, PL_BOOL, PL_REAL, PL_DATA, PL_DATE };

typedef struct pl {
    u8 type;
    const char *key;            /* chave no dicionário pai (NULL fora de dicionários) */
    const char *str;            /* PL_STRING (UTF-8) */
    i64 num;                    /* PL_INT, PL_BOOL (0/1) */
    struct pl *child, *next;    /* filhos de PL_DICT/PL_ARRAY, em ordem */
    int count;
} pl_t;

pl_t       *pl_parse(const u8 *data, usize n);    /* NULL se não for plist */
void        pl_free(pl_t *root);
pl_t       *pl_get(const pl_t *dict, const char *key);
const char *pl_str(const pl_t *dict, const char *key);   /* string do dicionário ou NULL */
pl_t       *pl_at(const pl_t *arr, int i);
/* string isolada ou array de strings: chama fn para cada string */
void        pl_each_str(const pl_t *v, void (*fn)(const char *s, void *ctx), void *ctx);
