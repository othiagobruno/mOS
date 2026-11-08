/* suíte diferencial C: laços que o -O3 vetoriza (NEON no arm64), inteiros e FP elemento a elemento */
#include "rt.h"
#define N 4099
static int32_t ia[N], ib[N], ic[N]; static int16_t sa[N], sb[N]; static int8_t ba[N]; static uint8_t ua[N], ub[N];
static float fa[N], fb[N], fc[N]; static double da[N], db[N]; static int64_t la[N]; static uint16_t us[N];

static void fill(void) {
    for (int i = 0; i < N; i++) {
        uint64_t r = rnd64();
        ia[i] = (int32_t)r; ib[i] = (int32_t)(r >> 32) | 1; sa[i] = (int16_t)(r >> 7); sb[i] = (int16_t)(r >> 23);
        ba[i] = (int8_t)(r >> 40); ua[i] = (uint8_t)(r >> 48); ub[i] = (uint8_t)(r >> 56);
        fa[i] = (float)(int32_t)(r >> 40) / 1024.0f; fb[i] = (float)(int16_t)r / 77.0f;
        da[i] = (double)(int64_t)r / 3e9; db[i] = (double)(int32_t)(r >> 16) * 1e-3;
        la[i] = (int64_t)r; us[i] = (uint16_t)(r >> 3);
    }
}

static void run(void) {
    put_s("c_vec\n");
    fill();
    hsh h; h_init(&h);
    int64_t s = 0; for (int i = 0; i < N; i++) s += ia[i];
    h_u(&h, (uint64_t)s);
    int32_t mx = ia[0], mn = ia[0];
    for (int i = 1; i < N; i++) { mx = ia[i] > mx ? ia[i] : mx; mn = ia[i] < mn ? ia[i] : mn; }
    h_u(&h, (uint64_t)(uint32_t)mx); h_u(&h, (uint64_t)(uint32_t)mn);
    for (int i = 0; i < N; i++) ic[i] = (int32_t)((uint32_t)ia[i] * 3u + (uint32_t)ib[i]);
    for (int i = 0; i < N; i++) h_u(&h, (uint64_t)(uint32_t)ic[i]);
    int64_t dot = 0; for (int i = 0; i < N; i++) dot += (int32_t)sa[i] * (int32_t)sb[i];
    h_u(&h, (uint64_t)dot);
    uint32_t sad = 0; for (int i = 0; i < N; i++) sad += (uint32_t)(ua[i] > ub[i] ? ua[i] - ub[i] : ub[i] - ua[i]);
    h_u(&h, sad);
    for (int i = 0; i < N; i++) { int v = ua[i] + ub[i]; ua[i] = (uint8_t)(v > 255 ? 255 : v); }
    for (int i = 0; i < N; i++) h_u(&h, ua[i]);
    for (int i = 0; i < N; i++) { int32_t v = ia[i] >> 12; sa[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }
    for (int i = 0; i < N; i++) h_u(&h, (uint64_t)(uint16_t)sa[i]);
    for (int i = 0; i < N; i++) ic[i] = ba[i] * 7;
    for (int i = 0; i < N; i++) h_u(&h, (uint64_t)(uint32_t)ic[i]);
    for (int i = 0; i < N; i++) ic[i] = (int32_t)((uint32_t)ia[i] >> (ib[i] & 31)) ^ (int32_t)((uint32_t)ib[i] << (ia[i] & 31));
    for (int i = 0; i < N; i++) h_u(&h, (uint64_t)(uint32_t)ic[i]);
    for (int i = 0; i < N; i++) ic[i] = (int32_t)(((int64_t)ia[i] * ib[i]) >> 32);
    for (int i = 0; i < N; i++) h_u(&h, (uint64_t)(uint32_t)ic[i]);
    for (int i = 0; i < N; i++) ic[i] = __builtin_popcount((uint32_t)ia[i]);
    for (int i = 0; i < N; i++) h_u(&h, (uint64_t)ic[i]);
    for (int i = 0; i < N; i++) us[i] = (uint16_t)((us[i] * 3) >> 1);
    for (int i = 0; i < N; i++) h_u(&h, us[i]);
    uint64_t ls = 0; for (int i = 0; i < N; i++) ls ^= (uint64_t)la[i] * 3;
    h_u(&h, ls);
    kv_x("inteiros", h.h);

    h_init(&h);
    for (int i = 0; i < N; i++) fc[i] = fa[i] * 2.5f + fb[i];
    for (int i = 0; i < N; i++) h_f(&h, fc[i]);
    for (int i = 0; i < N; i++) fc[i] = fa[i] / (fb[i] + 3.0f);
    for (int i = 0; i < N; i++) h_f(&h, fc[i]);
    for (int i = 0; i < N; i++) fc[i] = __builtin_sqrtf(fa[i] < 0 ? -fa[i] : fa[i]);
    for (int i = 0; i < N; i++) h_f(&h, fc[i]);
    for (int i = 0; i < N; i++) fc[i] = fa[i] > fb[i] ? fa[i] : fb[i];
    for (int i = 0; i < N; i++) h_f(&h, fc[i]);
    for (int i = 0; i < N; i++) da[i] = da[i] * db[i] - 1.0;
    for (int i = 0; i < N; i++) h_d(&h, da[i]);
    for (int i = 0; i < N; i++) ic[i] = (int32_t)fa[i];
    for (int i = 0; i < N; i++) h_u(&h, (uint64_t)(uint32_t)ic[i]);
    for (int i = 0; i < N; i++) fc[i] = (float)ia[i];
    for (int i = 0; i < N; i++) h_f(&h, fc[i]);
    for (int i = 0; i < N; i++) db[i] = (double)fa[i];
    for (int i = 0; i < N; i++) h_d(&h, db[i]);
    for (int i = 0; i < N; i++) fc[i] = __builtin_floorf(fb[i]) + __builtin_truncf(fa[i]);
    for (int i = 0; i < N; i++) h_f(&h, fc[i]);
    float fs = 0; for (int i = 0; i < N; i++) fs += fa[i];      /* ordem sequencial: sem reassociação */
    h_f(&h, fs);
    kv_x("ponto flutuante", h.h);

    h_init(&h);
    for (int i = 0; i + 1 < N; i += 2) { int32_t t = ia[i]; ia[i] = (int32_t)((uint32_t)ia[i + 1] + 1u); ia[i + 1] = (int32_t)((uint32_t)t - 1u); }
    for (int i = 0; i < N / 2; i++) { int32_t t = ib[i]; ib[i] = ib[N - 1 - i]; ib[N - 1 - i] = t; }
    for (int i = 1; i < N; i++) ia[i] = (int32_t)((uint32_t)ia[i] + (uint32_t)ia[i - 1]);
    for (int i = 0; i < N; i++) { h_u(&h, (uint64_t)(uint32_t)ia[i]); h_u(&h, (uint64_t)(uint32_t)ib[i]); }
    static int32_t A[24][24], B[24][24], C[24][24];
    for (int i = 0; i < 24; i++) for (int j = 0; j < 24; j++) { A[i][j] = (int32_t)(rnd64() % 100) - 50; B[i][j] = (int32_t)(rnd64() % 100) - 50; }
    for (int i = 0; i < 24; i++) for (int j = 0; j < 24; j++) { int32_t t = 0; for (int k = 0; k < 24; k++) t += A[i][k] * B[k][j]; C[i][j] = t; }
    for (int i = 0; i < 24; i++) for (int j = 0; j < 24; j++) h_u(&h, (uint64_t)(uint32_t)C[i][j]);
    kv_x("permutacoes/matriz", h.h);
}
