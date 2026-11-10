/* Compila com -I tools/hostinc antes de qualquer stdint do sistema.
 * Não inclui stdio.h: o stddef do hostinc não tem size_t. */
#define MOS_LIBSYS_SYSCALL 1
#include "gpucall.h"
#include "mtlshader.h"
#include <stdarg.h>
int printf(const char *fmt, ...);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);

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

static unsigned ru32(const unsigned char *p) {
    unsigned v;
    memcpy(&v, p, 4);
    return v;
}

void host_compute_test(int *fails) {
    const char *src =
        "kernel void add1(device float *o [[buffer(0)]], uint id [[thread_position_in_grid]]) {\n"
        "  o[id] = 3.0;\n"
        "}\n";
    char err[256];
    msl_lib_t *L = msl_compile(src, strlen(src), 0, err, (int)sizeof err);
    msl_fn_t *fn = L ? msl_find(L, "add1") : 0;
    const u32 *sp = 0;
    u32 sn = 0;
    float out = 0;
    mos_gpu_bind_in_t bind;
    mos_gpu_compute_in_t job;
    unsigned char batch[400];
    unsigned bn = 0, fence = 0, base, i;
    int on = 1, bad = 0;
    msl_ctx_t ctx;
    u32 tg[3] = {0, 0, 0}, th[3] = {1, 1, 1}, grid[3] = {1, 1, 1};
    if (!(L && fn && msl_fn_spirv(fn, &sp, &sn) && sp && sn)) {
        printf("falha  spirv do lote %s\n", err);
        if (fails) (*fails)++;
        if (L) msl_lib_free(L);
        return;
    }
    memset(&bind, 0, sizeof bind);
    bind.bytes = &out;
    bind.length = sizeof out;
    bind.range = sizeof out;
    memset(&job, 0, sizeof job);
    job.shader = sp;
    job.shader_len = sn * 4u;
    job.local_x = job.local_y = job.local_z = 1;
    job.groups_x = job.groups_y = job.groups_z = 1;
    job.binds = &bind;
    job.nbind = 1;
    job.batch_out = batch;
    job.batch_cap = sizeof batch;
    job.batch_n = &bn;
    if (mos_gpu_compute_submit(&job, &fence, &on) != 0 || !fence || on) bad = 1;
    if (bn < 176 || (bn & 3) || ru32(batch) != MOS_GPU_BATCH_VER || ru32(batch + 4) != MOS_GPU_CTX_VIRGL) bad = 1;
    if (ru32(batch + 8) != bn || ru32(batch + 12) != 3 || ru32(batch + 16) != 2 || ru32(batch + 20) != 2) bad = 1;
    if (ru32(batch + 24) || ru32(batch + 28)) bad = 1;
    base = 32 + 8 + 32;
    for (i = 0; i < 2; i++) {
        unsigned off = 32 + 8 + i * 16;
        unsigned cmd = ru32(batch + off), boff = ru32(batch + off + 4), idx = ru32(batch + off + 8);
        unsigned cmd_at = base + (cmd == 0 ? 0 : 32);
        if (cmd > 1 || (boff & 3) || boff < 16 || idx != i) bad = 1;
        if (ru32(batch + cmd_at + boff) != idx) bad = 1;
    }
    if (ru32(batch + base) != MOS_GPU_CMD_BIND || ru32(batch + base + 8) != MOS_GPU_VIRGL_SET_SHADER_BUFFERS) bad = 1;
    if (ru32(batch + base + 12) != 0 || ru32(batch + base + 28) != 4) bad = 1;
    if (ru32(batch + base + 32) != MOS_GPU_CMD_SHADER || ru32(batch + base + 40) != MOS_GPU_VIRGL_BIND_SHADER) bad = 1;
    if (ru32(batch + base + 48) != 1 || ru32(batch + base + 56) != sn * 4u) bad = 1;
    if (ru32(batch + base + 60) != 1 || ru32(batch + base + 64) != 1 || ru32(batch + base + 68) != 1) bad = 1;
    if (ru32(batch + base + 72) != MOS_GPU_CMD_DISPATCH || ru32(batch + base + 80) != MOS_GPU_VIRGL_LAUNCH_GRID) bad = 1;
    if (ru32(batch + base + 88) != 1 || ru32(batch + base + 92) != 1 || ru32(batch + base + 96) != 1) bad = 1;
    if (base + 104 != bn) bad = 1;
    memset(&ctx, 0, sizeof ctx);
    ctx.buf[0] = &out;
    msl_run_group(fn, &ctx, tg, th, grid, 0);
    if (out != 3.f) bad = 1;
    if (bad) {
        printf("falha  lote compute (bytes ou interpretador)\n");
        if (fails) (*fails)++;
    } else printf("ok  lote BIND/SHADER/DISPATCH e interpretador\n");
    msl_lib_free(L);
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
