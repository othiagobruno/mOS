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

static int has_op(const u32 *w, u32 n, u32 op) {
    for (u32 i = 5; i < n;) {
        u32 wc = w[i] >> 16;
        if (!wc || i + wc > n) return 0;
        if ((w[i] & 0xffffu) == op) return 1;
        i += wc;
    }
    return 0;
}

void host_spirv_test(int *fails) {
    const char *src =
        "kernel void k(device float *o [[buffer(0)]], uint id [[thread_position_in_grid]]) {\n"
        "  o[id] = 2.0;\n"
        "}\n";
    char err[256];
    msl_lib_t *L = msl_compile(src, strlen(src), 0, err, (int)sizeof err);
    const u32 *w = 0;
    u32 n = 0;
    msl_fn_t *k = L ? msl_find(L, "k") : 0;
    int ok = L && k && msl_fn_spirv(k, &w, &n) && w && w[0] == 0x07230203u && has_op(w, n, 62);
    if (ok) printf("ok  spirv do kernel\n");
    else { printf("falha  spirv do kernel %s\n", err); if (fails) (*fails)++; }
    if (L) msl_lib_free(L);
}
