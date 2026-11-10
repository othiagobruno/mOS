/* Teste de host da camada de GPU e do SPIR-V.
 *   cc -O2 -std=c11 -DMOS_LIBSYS_SYSCALL -DMOS_GPU_HOST_TEST \
 *      -I userland/libsystem -I kernel/include \
 *      userland/libsystem/mtlshader.c userland/libsystem/mslspirv.c \
 *      userland/libsystem/gpucall.c tools/host-metal-gpu.c -o build/host-metal-gpu
 */
#include "mtlshader.h"
#include <stdio.h>
#include <string.h>

int cx_fdprintf(int fd, const char *fmt, ...) { (void)fd; (void)fmt; return 0; }
void msl_tex_load(const msl_tex_t *t, int x, int y, int z, int slice, int lvl, double out[4]) {
    (void)t; (void)x; (void)y; (void)z; (void)slice; (void)lvl;
    out[0] = out[1] = out[2] = out[3] = 0;
}
void msl_tex_store(const msl_tex_t *t, int x, int y, int z, int slice, int lvl, const double in[4]) {
    (void)t; (void)x; (void)y; (void)z; (void)slice; (void)lvl; (void)in;
}
void msl_tex_sample(const msl_tex_t *t, const msl_samp_t *s, const double *coord, int ncoord, int slice, double lod, double out[4]) {
    (void)t; (void)s; (void)coord; (void)ncoord; (void)slice; (void)lod;
    out[0] = out[1] = out[2] = out[3] = 0;
}

long cx_syscall(long n, long a, long b, long c, long d, long e) {
    (void)n; (void)a; (void)b; (void)c; (void)d; (void)e;
    return -38;
}
int mos_gpu_host_check(void);

static int fails;
static void check(int cond, const char *msg) {
    if (cond) { printf("ok  %s\n", msg); return; }
    printf("falha  %s\n", msg);
    fails++;
}

static int spirv_has(const u32 *w, u32 n, u32 op) {
    for (u32 i = 5; i < n;) {
        u32 wc = w[i] >> 16;
        u32 c = w[i] & 0xffffu;
        if (!wc || i + wc > n) return 0;
        if (c == op) return 1;
        i += wc;
    }
    return 0;
}

static void test_spirv(void) {
    const char *src =
        "kernel void k(device float *o [[buffer(0)]], uint id [[thread_position_in_grid]]) {\n"
        "  o[id] = 2.0;\n"
        "}\n"
        "kernel void tex(texture2d<float> t [[texture(0)]]) {}\n";
    char err[512];
    msl_lib_t *L = msl_compile(src, strlen(src), 0, err, (int)sizeof err);
    check(L != 0, err[0] ? err : "compila msl");
    if (!L) return;
    msl_fn_t *k = msl_find(L, "k");
    const u32 *w = 0;
    u32 n = 0;
    int got = k && msl_fn_spirv(k, &w, &n);
    if (!got) printf("nota k: %s\n", k ? msl_fn_spirv_note(k) : "sem fn");
    check(got == 1, "k tem spirv");
    check(w && n > 8 && w[0] == 0x07230203u, "magic");
    check(w && spirv_has(w, n, 15) && spirv_has(w, n, 62), "entry e store");
    check(k && strstr(msl_fn_spirv_note(k), "LocalSize") != 0, "nota de localsize");
    msl_fn_t *t = msl_find(L, "tex");
    check(t && msl_fn_spirv(t, 0, 0) == 0, "textura não vira módulo");
    check(t && msl_fn_spirv_note(t)[0], "nota da textura");
    msl_lib_free(L);
}

int main(void) {
    int g = mos_gpu_host_check();
    check(g == 0, "backend de cpu da abi");
    if (g) printf("gpu check código %d\n", g);
    test_spirv();
    if (fails) { printf("%d falhas\n", fails); return 1; }
    printf("host-metal-gpu ok\n");
    return 0;
}
