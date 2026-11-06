/*
 * mOS libSystem — leitor do formato NIBArchive (os .nib compilados pelo ibtool/Xcode, inclusive os de
 * dentro de .storyboardc). C puro, sem dependências: usado pelo AppKit e por tools/nibdump.c.
 *
 * Layout: "NIBArchive" u32 1, u32 10, depois 4 pares (contagem, deslocamento) para objetos, chaves,
 * valores e nomes de classe. Inteiros variáveis de 7 bits com o bit alto marcando o ÚLTIMO byte.
 */
#pragma once

enum { NV_I8, NV_I16, NV_I32, NV_I64, NV_TRUE, NV_FALSE, NV_FLOAT, NV_DOUBLE, NV_DATA, NV_NIL, NV_OBJ };

typedef struct { unsigned key; int type; long long i; double d; const unsigned char *data; unsigned len; unsigned obj; } nib_val_t;
typedef struct { unsigned cls, first, count; } nib_obj_t;
typedef struct {
    const unsigned char *buf;
    unsigned long size;
    unsigned nobj, nkey, nval, ncls;
    nib_obj_t *obj;
    const char **key;
    nib_val_t *val;
    const char **cls;
} nib_t;

/* devolve 0 se não for um NIBArchive válido; aloca com as funções passadas */
int nib_parse(nib_t *n, const unsigned char *buf, unsigned long size, void *(*alloc)(unsigned long));
const char *nib_class(const nib_t *n, unsigned o);
/* primeiro valor do objeto com a chave dada (ou 0) */
const nib_val_t *nib_get(const nib_t *n, unsigned o, const char *key);
/* número inteiro/real/booleano de um valor */
double nib_num(const nib_val_t *v);
