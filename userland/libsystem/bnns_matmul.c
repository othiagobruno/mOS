/*
 * mOS — BNNS: multiplicação de matrizes com broadcast (BNNSFilterCreateLayerBroadcastMatMul, BNNSFilterApplyTwoInput,
 * BNNSFilterDestroy), pelo header público do macOS 11 (bnns_structures.h): C = beta·C + alpha·op(A)·op(B) nos dois
 * últimos índices; os de lote casam de trás para a frente e um tamanho 1 (ou ausente) se repete. Float32.
 * Layouts: N-D LastMajor (e RowMajorMatrix/Vector), size[0] é o índice mais rápido; N-D FirstMajor, size[0] é o mais
 * lento. ColumnMajorMatrix e outros tipos de dado: a criação devolve NULL. Teste: tools/bnns_test.sh. Código original (MIT).
 */
#ifdef BNNS_HOST
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#else
#include "libc.h"
#endif

typedef struct {
    uint32_t flags, layout;
    size_t size[8], stride[8];
    void *data;
    uint32_t data_type;
    void *table_data;
    uint32_t table_data_type;
    float data_scale, data_bias;
} bnns_desc;
typedef struct {
    float alpha, beta;
    _Bool transA, transB, quadratic, a_is_weights, b_is_weights;   /* bool do C: 1 byte cada, como no header */
    bnns_desc iA, iB, o;
} bnns_matmul_params;
#define BNNS_F32 0x10020u
#define MM_MAGIC 0x4D4D4E42u

/* visão de um tensor em índices matemáticos (0 = mais lento): tamanho e passo em elementos */
typedef struct { int n; size_t sz[8]; size_t st[8]; } view;
static int make_view(const bnns_desc *d, view *v) {
    uint32_t L = d->layout;
    int rank = (int)((L >> 16) & 0xF);
    int first_major;
    if (L == 0x10000u || L == 0x20000u) first_major = 0;                 /* Vector, RowMajorMatrix */
    else if ((L & 0xFFFFu) == 0x8000u) first_major = 0;                 /* N-D LastMajor */
    else if ((L & 0xFFFFu) == 0x8001u) first_major = 1;                 /* N-D FirstMajor */
    else return -1;                                                     /* ColumnMajorMatrix, imagem, pesos... */
    if (rank < 1 || rank > 8 || d->data_type != BNNS_F32) return -1;
    size_t st[8];
    int contig = 1;
    for (int k = 0; k < rank; k++) if (d->stride[k]) contig = 0;
    if (contig) {
        if (!first_major) { st[0] = 1; for (int k = 1; k < rank; k++) st[k] = st[k - 1] * (d->size[k - 1] ? d->size[k - 1] : 1); }
        else { st[rank - 1] = 1; for (int k = rank - 2; k >= 0; k--) st[k] = st[k + 1] * (d->size[k + 1] ? d->size[k + 1] : 1); }
    } else for (int k = 0; k < rank; k++) st[k] = d->stride[k];
    v->n = rank;
    for (int m = 0; m < rank; m++) {
        int k = first_major ? m : rank - 1 - m;
        v->sz[m] = d->size[k] ? d->size[k] : 1;
        v->st[m] = st[k];
    }
    if (rank == 1) { v->n = 2; v->sz[1] = v->sz[0]; v->st[1] = v->st[0]; v->sz[0] = 1; v->st[0] = 0; }   /* vetor = linha */
    return 0;
}
typedef struct { uint32_t magic; bnns_matmul_params p; view a, b, c; } mm_t;

void *BNNSFilterCreateLayerBroadcastMatMul(const bnns_matmul_params *lp, const void *fp) {
    (void)fp;
    if (!lp || (lp->beta != 0.0f && lp->beta != 1.0f)) return 0;
    mm_t *m = calloc(1, sizeof *m);
    if (!m) return 0;
    m->magic = MM_MAGIC; m->p = *lp;
    if (make_view(&lp->iA, &m->a) || make_view(&lp->o, &m->c) || (!lp->quadratic && make_view(&lp->iB, &m->b))) { free(m); return 0; }
    if (lp->quadratic) { m->b = m->a; }
    /* dimensões: A (M x K), B (K x N), C (M x N) depois das transposições */
    size_t Ma = m->a.sz[m->a.n - 2], Ka = m->a.sz[m->a.n - 1];
    if (lp->transA) { size_t t = Ma; Ma = Ka; Ka = t; }
    size_t Kb = m->b.sz[m->b.n - 2], Nb = m->b.sz[m->b.n - 1];
    if (lp->transB) { size_t t = Kb; Kb = Nb; Nb = t; }
    if (Ka != Kb || m->c.sz[m->c.n - 2] != Ma || m->c.sz[m->c.n - 1] != Nb) { free(m); return 0; }
    /* lote: casa de trás para a frente com o da saída */
    for (int i = 3; i <= m->c.n; i++) {
        size_t oc = m->c.sz[m->c.n - i];
        size_t ia = m->a.n - i >= 0 ? m->a.sz[m->a.n - i] : 1, ib = m->b.n - i >= 0 ? m->b.sz[m->b.n - i] : 1;
        if ((ia != 1 && ia != oc) || (ib != 1 && ib != oc)) { free(m); return 0; }
    }
    if (m->a.n > m->c.n || m->b.n > m->c.n) { free(m); return 0; }
    return m;
}
int BNNSFilterApplyTwoInput(void *f, const void *inA, const void *inB, void *out) {
    mm_t *m = f;
    if (!m || m->magic != MM_MAGIC || !out) return -1;
    const float *A = m->p.a_is_weights ? (const float *)m->p.iA.data : (const float *)inA;
    const float *B = m->p.quadratic ? A : m->p.b_is_weights ? (const float *)m->p.iB.data : (const float *)inB;
    float *C = out;
    if (!A || !B) return -1;
    const view *a = &m->a, *b = &m->b, *c = &m->c;
    size_t M = c->sz[c->n - 2], N = c->sz[c->n - 1], K = m->p.transA ? a->sz[a->n - 2] : a->sz[a->n - 1];
    size_t nbatch = 1;
    for (int i = 0; i < c->n - 2; i++) nbatch *= c->sz[i];
    for (size_t bi = 0; bi < nbatch; bi++) {
        /* índice de lote da saída -> deslocamentos de A, B e C */
        size_t rem = bi, oa = 0, ob = 0, oc = 0;
        for (int i = c->n - 3; i >= 0; i--) {
            size_t idx = rem % c->sz[i];
            rem /= c->sz[i];
            int back = c->n - i;   /* posição contada do fim */
            oc += idx * c->st[i];
            if (a->n - back >= 0) { int ai = a->n - back; oa += (a->sz[ai] == 1 ? 0 : idx) * a->st[ai]; }
            if (b->n - back >= 0) { int bj = b->n - back; ob += (b->sz[bj] == 1 ? 0 : idx) * b->st[bj]; }
        }
        size_t ar = a->st[a->n - 2], ac = a->st[a->n - 1], br = b->st[b->n - 2], bc = b->st[b->n - 1], cr = c->st[c->n - 2], cc = c->st[c->n - 1];
        for (size_t i = 0; i < M; i++)
            for (size_t j = 0; j < N; j++) {
                double s = 0;
                for (size_t k = 0; k < K; k++) {
                    float av = m->p.transA ? A[oa + k * ar + i * ac] : A[oa + i * ar + k * ac];
                    float bv = m->p.transB ? B[ob + j * br + k * bc] : B[ob + k * br + j * bc];
                    s += (double)av * bv;
                }
                float *dst = &C[oc + i * cr + j * cc];
                *dst = (float)(m->p.alpha * s + (m->p.beta == 1.0f ? *dst : 0.0f));
            }
    }
    return 0;
}
void BNNSFilterDestroy(void *f) {
    mm_t *m = f;
    if (m && m->magic == MM_MAGIC) { m->magic = 0; free(m); }
}
