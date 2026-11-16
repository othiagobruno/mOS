/* Teste de host do userland/libsystem/bnns_matmul.c (BroadcastMatMul). */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
typedef struct { uint32_t flags, layout; size_t size[8], stride[8]; void *data; uint32_t data_type; void *table_data; uint32_t table_data_type; float data_scale, data_bias; } desc;
typedef struct { float alpha, beta; bool transA, transB, quadratic, a_is_weights, b_is_weights; desc iA, iB, o; } params;
void *BNNSFilterCreateLayerBroadcastMatMul(const params *, const void *);
int BNNSFilterApplyTwoInput(void *, const void *, const void *, void *);
void BNNSFilterDestroy(void *);
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
static desc nd(int rank, const size_t *math_sizes, int first_major) {   /* tamanhos na ordem matemática (lento -> rápido) */
    desc d; memset(&d, 0, sizeof d);
    d.layout = (uint32_t)(rank << 16) | (first_major ? 0x8001u : 0x8000u);
    for (int m = 0; m < rank; m++) d.size[first_major ? m : rank - 1 - m] = math_sizes[m];
    d.data_type = 0x10020;
    return d;
}
int main(void) {
    /* 2x3 · 3x2 */
    float A[6] = {1, 2, 3, 4, 5, 6}, B[6] = {7, 8, 9, 10, 11, 12}, C[4];
    size_t sa[2] = {2, 3}, sb[2] = {3, 2}, sc[2] = {2, 2};
    params p; memset(&p, 0, sizeof p);
    p.alpha = 1; p.iA = nd(2, sa, 0); p.iB = nd(2, sb, 0); p.o = nd(2, sc, 0);
    void *f = BNNSFilterCreateLayerBroadcastMatMul(&p, 0);
    CHECK(f && BNNSFilterApplyTwoInput(f, A, B, C) == 0);
    CHECK(C[0] == 58 && C[1] == 64 && C[2] == 139 && C[3] == 154);
    BNNSFilterDestroy(f);
    /* mesmo com FirstMajor (mesma memória linha a linha) */
    p.iA = nd(2, sa, 1); p.iB = nd(2, sb, 1); p.o = nd(2, sc, 1);
    f = BNNSFilterCreateLayerBroadcastMatMul(&p, 0);
    memset(C, 0, sizeof C);
    CHECK(f && BNNSFilterApplyTwoInput(f, A, B, C) == 0 && C[0] == 58 && C[3] == 154);
    BNNSFilterDestroy(f);
    /* transB: B guardado 2x3 */
    float Bt[6] = {7, 9, 11, 8, 10, 12};
    size_t sbt[2] = {2, 3};
    p.transB = true; p.iA = nd(2, sa, 0); p.iB = nd(2, sbt, 0); p.o = nd(2, sc, 0); p.alpha = 2;
    f = BNNSFilterCreateLayerBroadcastMatMul(&p, 0);
    CHECK(f && BNNSFilterApplyTwoInput(f, A, Bt, C) == 0 && C[0] == 116 && C[3] == 308);
    BNNSFilterDestroy(f);
    /* lote com broadcast: A 2x(2x3), B (3x2) só uma, C 2x(2x2); beta 1 soma */
    float A2[12] = {1, 2, 3, 4, 5, 6, 1, 0, 0, 0, 1, 0}, C2[8] = {1, 1, 1, 1, 0, 0, 0, 0};
    size_t sa2[3] = {2, 2, 3}, sc2[3] = {2, 2, 2};
    memset(&p, 0, sizeof p);
    p.alpha = 1; p.beta = 1; p.iA = nd(3, sa2, 0); p.iB = nd(2, sb, 0); p.o = nd(3, sc2, 0);
    f = BNNSFilterCreateLayerBroadcastMatMul(&p, 0);
    CHECK(f && BNNSFilterApplyTwoInput(f, A2, B, C2) == 0);
    CHECK(C2[0] == 59 && C2[3] == 155 && C2[4] == 7 && C2[5] == 8 && C2[6] == 9 && C2[7] == 10);
    BNNSFilterDestroy(f);
    /* pesos em B (o ponteiro de entrada é ignorado) e quadrático (A·A) */
    memset(&p, 0, sizeof p);
    p.alpha = 1; p.iA = nd(2, sa, 0); p.iB = nd(2, sb, 0); p.o = nd(2, sc, 0); p.b_is_weights = true; p.iB.data = B;
    f = BNNSFilterCreateLayerBroadcastMatMul(&p, 0);
    CHECK(f && BNNSFilterApplyTwoInput(f, A, 0, C) == 0 && C[1] == 64);
    BNNSFilterDestroy(f);
    float Q[4] = {1, 2, 3, 4}, Qo[4];
    size_t s22[2] = {2, 2};
    memset(&p, 0, sizeof p);
    p.alpha = 1; p.quadratic = true; p.iA = nd(2, s22, 0); p.o = nd(2, s22, 0);
    f = BNNSFilterCreateLayerBroadcastMatMul(&p, 0);
    CHECK(f && BNNSFilterApplyTwoInput(f, Q, 0, Qo) == 0 && Qo[0] == 7 && Qo[1] == 10 && Qo[2] == 15 && Qo[3] == 22);
    BNNSFilterDestroy(f);
    /* recusas: tamanhos que não casam, beta fora de {0,1}, coluna-maior */
    memset(&p, 0, sizeof p);
    p.alpha = 1; p.iA = nd(2, sa, 0); p.iB = nd(2, sa, 0); p.o = nd(2, sc, 0);
    CHECK(!BNNSFilterCreateLayerBroadcastMatMul(&p, 0));
    p.iB = nd(2, sb, 0); p.beta = 0.5f;
    CHECK(!BNNSFilterCreateLayerBroadcastMatMul(&p, 0));
    p.beta = 0; p.iA.layout = 0x20001;
    CHECK(!BNNSFilterCreateLayerBroadcastMatMul(&p, 0));
    printf(fails ? "bnns: %d falhas\n" : "bnns: ok\n", fails);
    return fails != 0;
}
