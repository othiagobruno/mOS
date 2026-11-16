/* Teste de host do fdn_astr_core.c (trechos de atributos do NSAttributedString), com inteiros como valores. */
#include <stdio.h>
#include <stdint.h>
#include "fdn_astr_core.h"
static int ok, bad, live;
static void eq(const char *w, long a, long b) { if (a == b) ok++; else { bad++; printf("FALHOU %s: %ld (queria %ld)\n", w, a, b); } }
static void *ret(void *v) { live++; return v; }
static void rel(void *v) { (void)v; live--; }
static int equ(void *a, void *b) { return (intptr_t)a == (intptr_t)b; }
static const runs_cb CB = { ret, rel, equ };
#define V(x) ((void *)(intptr_t)(x))
static long at(runs_t *r, uint64_t i, uint64_t *s, uint64_t *l) { return (long)(intptr_t)runs_at(r, i, s, l); }
int main(void) {
    runs_t r = { 0 };
    uint64_t s, l;
    runs_replace(&r, &CB, 0, 0, 10, V(1));               /* "0123456789" com 1 */
    eq("total", (long)runs_total(&r), 10); eq("um trecho", (long)r.n, 1);
    runs_set(&r, &CB, 3, 4, V(2));                        /* 111 2222 111 */
    eq("três trechos", (long)r.n, 3);
    eq("valor 5", at(&r, 5, &s, &l), 2); eq("faixa 5", (long)s * 100 + (long)l, 304);
    eq("valor 8", at(&r, 8, &s, &l), 1); eq("faixa 8", (long)s * 100 + (long)l, 703);
    runs_set(&r, &CB, 3, 4, V(1));                        /* volta: junta tudo */
    eq("juntou", (long)r.n, 1); eq("tam", (long)r.len[0], 10);
    runs_set(&r, &CB, 0, 2, V(3));                        /* 33 11111111 */
    runs_replace(&r, &CB, 1, 3, 1, V(3));                 /* apaga 1..3, põe 1 com 3: 33 111111 */
    eq("total depois", (long)runs_total(&r), 8); eq("trechos", (long)r.n, 2); eq("início", (long)r.len[0], 2);
    runs_replace(&r, &CB, 2, 0, 3, V(4));                 /* insere 444 no meio */
    eq("insere", at(&r, 3, &s, &l), 4); eq("insere faixa", (long)s * 100 + (long)l, 203);
    runs_replace(&r, &CB, 0, runs_total(&r), 0, 0);       /* apaga tudo */
    eq("vazio", (long)r.n, 0); eq("total 0", (long)runs_total(&r), 0);
    eq("sem vazamento", live, 0);
    runs_replace(&r, &CB, 0, 0, 4, V(7));
    runs_set(&r, &CB, 0, 4, V(8)); runs_set(&r, &CB, 1, 2, V(9)); runs_set(&r, &CB, 3, 1, V(9));
    eq("8 9 9 9 -> 2 trechos", (long)r.n, 2);
    runs_free(&r, &CB);
    eq("livre", live, 0);
    printf("fdn_astr: %d/%d\n", ok, ok + bad);
    return bad != 0;
}
