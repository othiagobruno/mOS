/* Teste de host do lowering AIR. Gera bitcode com o clang desta máquina, baixa para o
 * interpretador e compara com o mesmo cálculo compilado do fonte MSL.
 * Não é teste de VM e não usa hardware.
 *
 *   clang -std=c11 -O1 -I userland/libsystem -o /tmp/airlower_test tools/airlower_test.c \
 *        userland/libsystem/mtlshader.c userland/libsystem/mtlexec.c userland/libsystem/mtlair.c -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include "mtlshader.h"

msl_lib_t *mtlair_lower_binds(msl_lib_t *into, const void *data, u64 len, const char *fn,
                              const int *attrs, const int *idxs, int nargs, char *err, int errn);

int *__error(void) { static int e; return &e; }
int cx_fdprintf(int fd, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vfprintf(fd == 2 ? stderr : stdout, fmt, ap);
    va_end(ap);
    return n;
}
void msl_tex_load(const msl_tex_t *t, int x, int y, int z, int slice, int lvl, double out[4]) {
    (void)t; (void)x; (void)y; (void)z; (void)slice; (void)lvl;
    out[0] = out[1] = out[2] = out[3] = 0;
}
void msl_tex_store(const msl_tex_t *t, int x, int y, int z, int slice, int lvl, const double in[4]) {
    (void)t; (void)x; (void)y; (void)z; (void)slice; (void)lvl; (void)in;
}
int msl_fmt_isint(int fmt) { (void)fmt; return 0; }
void msl_tex_sample(const msl_tex_t *t, const msl_samp_t *s, const double *coord, int ncoord, int slice, double lod, double out[4]) {
    (void)t; (void)s; (void)coord; (void)ncoord; (void)slice; (void)lod;
    out[0] = out[1] = out[2] = out[3] = 0;
}

static int g_fail;

static void expect_f(const char *what, float got, float exp) {
    float d = got - exp;
    if (d < 0) d = -d;
    if (d > 0.0001f) { printf("FALHA %s: obtido %g esperado %g\n", what, got, exp); g_fail++; }
    else printf("ok %s = %g\n", what, got);
}
static void expect_i(const char *what, int got, int exp) {
    if (got != exp) { printf("FALHA %s: obtido %d esperado %d\n", what, got, exp); g_fail++; }
    else printf("ok %s = %d\n", what, got);
}

static int write_file(const char *path, const char *s) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fputs(s, f);
    fclose(f);
    return 1;
}
static u8 *loadf(const char *path, u64 *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *d = malloc((size_t)sz + 1);
    if (!d || fread(d, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(d); return 0; }
    fclose(f);
    *n = (u64)sz;
    return d;
}
static int sh(const char *cmd) {
    int rc = system(cmd);
    if (rc != 0) printf("FALHA comando: %s (rc %d)\n", cmd, rc);
    return rc == 0;
}

static msl_fn_t *lower_named(const char *bcpath, const char *fn, const int *attrs, const int *idxs, int nargs, msl_lib_t **out) {
    u64 n = 0;
    u8 *bc = loadf(bcpath, &n);
    char err[512];
    err[0] = 0;
    if (!bc) { printf("FALHA não abriu %s\n", bcpath); g_fail++; return 0; }
    msl_lib_t *L = mtlair_lower_binds(0, bc, n, fn, attrs, idxs, nargs, err, sizeof err);
    free(bc);
    *out = L;
    if (!L) { printf("FALHA AIR %s: %s\n", fn ? fn : bcpath, err); g_fail++; return 0; }
    if (!fn) return 0;
    msl_fn_t *f = msl_find(L, fn);
    if (!f) { printf("FALHA AIR sem %s (%s)\n", fn, err); g_fail++; }
    else printf("baixou %s estágio %d\n", msl_fn_name(f), msl_fn_stage(f));
    return f;
}
static msl_fn_t *compile_msl(const char *src, const char *name, msl_lib_t **out) {
    char err[512];
    msl_lib_t *L = msl_compile(src, strlen(src), 0, err, sizeof err);
    *out = L;
    if (!L) { printf("FALHA MSL %s: %s\n", name, err); g_fail++; return 0; }
    msl_fn_t *f = msl_find(L, name);
    if (!f) { printf("FALHA MSL sem %s\n", name); g_fail++; }
    return f;
}

static void run_bufs(msl_fn_t *air, msl_fn_t *src, void **bufs, int nbuf, u32 tid, int nbytes, const char *tag, int as_int) {
    msl_ctx_t ca, cs;
    memset(&ca, 0, sizeof ca);
    memset(&cs, 0, sizeof cs);
    u8 *oa = calloc(1, (size_t)nbytes);
    u8 *os = calloc(1, (size_t)nbytes);
    memcpy(oa, bufs[0], (size_t)nbytes);
    memcpy(os, bufs[0], (size_t)nbytes);
    ca.buf[0] = oa;
    cs.buf[0] = os;
    for (int i = 1; i < nbuf; i++) { ca.buf[i] = bufs[i]; cs.buf[i] = bufs[i]; }
    ca.tpig[0] = cs.tpig[0] = tid;
    msl_run(air, &ca, 0);
    msl_run(src, &cs, 0);
    if (as_int) {
        int *ga = (int *)oa, *gs = (int *)os;
        int n = nbytes / 4;
        for (int i = 0; i < n; i++) {
            char b[80];
            snprintf(b, sizeof b, "%s[%d]", tag, i);
            expect_i(b, ga[i], gs[i]);
        }
    } else {
        float *ga = (float *)oa, *gs = (float *)os;
        int n = nbytes / 4;
        for (int i = 0; i < n; i++) {
            char b[80];
            snprintf(b, sizeof b, "%s[%d]", tag, i);
            expect_f(b, ga[i], gs[i]);
        }
    }
    free(oa);
    free(os);
}

int main(void) {
    if (system("mkdir -p /tmp/airtry/src") != 0) return 1;
    /* --- k: ramo, fmuladd, inteiro sem sinal, em bitcode -O0 do clang --- */
    write_file("/tmp/airtry/src/k.c",
        "void k(float *out, float *a, unsigned tid) {\n"
        "  float x = a[0] * 2.0f + 1.0f;\n"
        "  if (tid > 0) x = x + out[tid - 1];\n"
        "  out[tid] = x;\n"
        "}\n");
    write_file("/tmp/airtry/src/loopk.c",
        "void loopk(float *out, float *a, unsigned n) {\n"
        "  float s = 0;\n"
        "  for (unsigned i = 0; i < n; i++) s = s + a[i];\n"
        "  out[0] = s;\n"
        "}\n");
    write_file("/tmp/airtry/src/mulk.c",
        "void mulk(float *out, float *a) { out[0] = a[0] * 2.0f + 1.0f; }\n");
    write_file("/tmp/airtry/src/pair.c",
        "void okk(float *out, float *a) { out[0] = a[0] + 1.0f; }\n"
        "double badd(double *out, double *a) { *out = *a + 1.0; return *out; }\n");
    write_file("/tmp/airtry/src/ops.ll",
        "define void @shk(ptr %out, ptr %a) {\n"
        "  %v = load <4 x float>, ptr %a, align 4\n"
        "  %s = shufflevector <4 x float> %v, <4 x float> poison, <4 x i32> <i32 3, i32 2, i32 1, i32 0>\n"
        "  %e = extractelement <4 x float> %s, i64 0\n"
        "  %i = insertelement <4 x float> %s, float %e, i64 1\n"
        "  store <4 x float> %i, ptr %out, align 4\n"
        "  ret void\n"
        "}\n"
        "%S = type { float, i32 }\n"
        "define void @stk(ptr %out, ptr %p) {\n"
        "  %s = load %S, ptr %p, align 4\n"
        "  %x = extractvalue %S %s, 0\n"
        "  %iv = extractvalue %S %s, 1\n"
        "  %i2 = add i32 %iv, 1\n"
        "  %s2 = insertvalue %S %s, i32 %i2, 1\n"
        "  %y = extractvalue %S %s2, 1\n"
        "  %yf = sitofp i32 %y to float\n"
        "  %sum = fadd float %x, %yf\n"
        "  store float %sum, ptr %out, align 4\n"
        "  ret void\n"
        "}\n"
        "define void @selk(ptr %out, ptr %a, ptr %b, ptr %c) {\n"
        "  %va = load <4 x i32>, ptr %a, align 4\n"
        "  %vb = load <4 x i32>, ptr %b, align 4\n"
        "  %vc = load <4 x i32>, ptr %c, align 4\n"
        "  %m = icmp slt <4 x i32> %vc, zeroinitializer\n"
        "  %s = select <4 x i1> %m, <4 x i32> %va, <4 x i32> %vb\n"
        "  store <4 x i32> %s, ptr %out, align 4\n"
        "  ret void\n"
        "}\n");
    if (!sh("clang -std=c11 -O0 -emit-llvm -c /tmp/airtry/src/k.c -o /tmp/airtry/src/k.bc") ||
        !sh("clang -std=c11 -O1 -fno-vectorize -fno-unroll-loops -emit-llvm -c /tmp/airtry/src/loopk.c -o /tmp/airtry/src/loopk.bc") ||
        !sh("clang -std=c11 -O0 -emit-llvm -c /tmp/airtry/src/mulk.c -o /tmp/airtry/src/mulk.bc") ||
        !sh("clang -std=c11 -O0 -emit-llvm -c /tmp/airtry/src/pair.c -o /tmp/airtry/src/pair.bc") ||
        !sh("clang -x ir -emit-llvm -c /tmp/airtry/src/ops.ll -o /tmp/airtry/src/ops.bc")) {
        g_fail++;
        printf("FALHOU (clang não gerou bitcode, %d falhas)\n", g_fail);
        return 1;
    }

    int ab3[3] = { A_BUFFER, A_BUFFER, A_TPIG };
    int ix3[3] = { 0, 1, 0 };
    msl_lib_t *Lk = 0, *Lks = 0;
    msl_fn_t *fk = lower_named("/tmp/airtry/src/k.bc", "k", ab3, ix3, 3, &Lk);
    msl_fn_t *fks = compile_msl(
        "kernel void k(device float *out [[buffer(0)]], device float *a [[buffer(1)]], uint tid [[thread_position_in_grid]]) {\n"
        "  float x = a[0] * 2.0 + 1.0;\n"
        "  if (tid > 0) x = x + out[tid - 1];\n"
        "  out[tid] = x;\n"
        "}\n", "k", &Lks);
    if (fk && fks) {
        float a[4] = { 3, 0, 0, 0 };
        float out0[4] = { 0, 0, 0, 0 };
        void *b0[2] = { out0, a };
        run_bufs(fk, fks, b0, 2, 0, 16, "k tid0", 0);
        run_bufs(fk, fks, b0, 2, 2, 16, "k tid2", 0);
        float out1[4] = { 4, 0, 0, 0 };
        float a1[4] = { 1.5f, 0, 0, 0 };
        void *b1[2] = { out1, a1 };
        run_bufs(fk, fks, b1, 2, 1, 16, "k tid1", 0);
    }

    msl_lib_t *Ll = 0, *Lls = 0;
    msl_fn_t *fl = lower_named("/tmp/airtry/src/loopk.bc", "loopk", ab3, ix3, 3, &Ll);
    msl_fn_t *fls = compile_msl(
        "kernel void loopk(device float *out [[buffer(0)]], device float *a [[buffer(1)]], uint n [[thread_position_in_grid]]) {\n"
        "  float s = 0;\n"
        "  for (uint i = 0; i < n; i = i + 1) s = s + a[i];\n"
        "  out[0] = s;\n"
        "}\n", "loopk", &Lls);
    if (fl && fls) {
        float a[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        float out[4] = { 0, 0, 0, 0 };
        void *b[2] = { out, a };
        run_bufs(fl, fls, b, 2, 4, 16, "loopk n4", 0);
        run_bufs(fl, fls, b, 2, 0, 16, "loopk n0", 0);
    }

    /* sem atributos explícitos: ponteiros caem em buffer 0, 1 */
    msl_lib_t *Lm = 0, *Lms = 0;
    msl_fn_t *fm = lower_named("/tmp/airtry/src/mulk.bc", 0, 0, 0, 0, &Lm);
    if (Lm) {
        fm = msl_find(Lm, "mulk");
        if (!fm) { printf("FALHA fallback sem mulk\n"); g_fail++; }
        else printf("baixou mulk por fallback estágio %d\n", msl_fn_stage(fm));
    }
    msl_fn_t *fms = compile_msl(
        "kernel void mulk(device float *out [[buffer(0)]], device float *a [[buffer(1)]]) {\n"
        "  out[0] = a[0] * 2.0 + 1.0;\n"
        "}\n", "mulk", &Lms);
    if (fm && fms) {
        float a[4] = { 3, 0, 0, 0 };
        float out[4] = { 0, 0, 0, 0 };
        void *b[2] = { out, a };
        run_bufs(fm, fms, b, 2, 0, 16, "mulk", 0);
    }

    msl_lib_t *Lp = 0;
    lower_named("/tmp/airtry/src/pair.bc", 0, 0, 0, 0, &Lp);
    if (Lp) {
        msl_fn_t *ok = msl_find(Lp, "okk");
        msl_fn_t *bad = msl_find(Lp, "badd");
        expect_i("okk presente", ok ? 1 : 0, 1);
        expect_i("badd omitido (double)", bad ? 1 : 0, 0);
        if (ok) {
            float a[1] = { 4 };
            float out[1] = { 0 };
            msl_ctx_t c;
            memset(&c, 0, sizeof c);
            c.buf[0] = out;
            c.buf[1] = a;
            msl_run(ok, &c, 0);
            expect_f("okk", out[0], 5);
        }
    }

    const char *ops_msl =
        "kernel void shk(device float *out [[buffer(0)]], device float *a [[buffer(1)]]) {\n"
        "  out[0] = a[3]; out[1] = a[3]; out[2] = a[1]; out[3] = a[0];\n"
        "}\n"
        "struct S { float x; int i; };\n"
        "kernel void stk(device float *out [[buffer(0)]], device S *p [[buffer(1)]]) {\n"
        "  out[0] = p->x + float(p->i + 1);\n"
        "}\n"
        "kernel void selk(device int *out [[buffer(0)]], device int *a [[buffer(1)]], device int *b [[buffer(2)]], device int *c [[buffer(3)]]) {\n"
        "  for (int i = 0; i < 4; i = i + 1) { if (c[i] < 0) out[i] = a[i]; else out[i] = b[i]; }\n"
        "}\n";
    msl_lib_t *Lo = 0, *Los = 0;
    lower_named("/tmp/airtry/src/ops.bc", 0, 0, 0, 0, &Lo);
    msl_fn_t *fos = 0;
    {
        char err[512];
        Los = msl_compile(ops_msl, strlen(ops_msl), 0, err, sizeof err);
        if (!Los) { printf("FALHA MSL ops: %s\n", err); g_fail++; }
    }
    if (Lo && Los) {
        msl_fn_t *sh = msl_find(Lo, "shk");
        msl_fn_t *st = msl_find(Lo, "stk");
        msl_fn_t *se = msl_find(Lo, "selk");
        fos = msl_find(Los, "shk");
        expect_i("shk presente", sh && fos ? 1 : 0, 1);
        if (sh && fos) {
            float a[4] = { 1, 2, 3, 4 };
            float out[4] = { 0, 0, 0, 0 };
            void *b[2] = { out, a };
            run_bufs(sh, fos, b, 2, 0, 16, "shk", 0);
        }
        msl_fn_t *sts = msl_find(Los, "stk");
        expect_i("stk presente", st && sts ? 1 : 0, 1);
        if (st && sts) {
            struct { float x; int i; } p = { 1.5f, 4 };
            float out[4] = { 0, 0, 0, 0 };
            void *b[2] = { out, &p };
            run_bufs(st, sts, b, 2, 0, 16, "stk", 0);
        }
        msl_fn_t *ses = msl_find(Los, "selk");
        expect_i("selk presente", se && ses ? 1 : 0, 1);
        if (se && ses) {
            int a[4] = { 10, 20, 30, 40 };
            int b[4] = { 1, 2, 3, 4 };
            int c[4] = { -1, 0, -5, 2 };
            int out[4] = { 0, 0, 0, 0 };
            void *bufs[4] = { out, a, b, c };
            run_bufs(se, ses, bufs, 4, 0, 16, "selk", 1);
        }
    }

    if (Lk) msl_lib_free(Lk);
    if (Lks) msl_lib_free(Lks);
    if (Ll) msl_lib_free(Ll);
    if (Lls) msl_lib_free(Lls);
    if (Lm) msl_lib_free(Lm);
    if (Lms) msl_lib_free(Lms);
    if (Lp) msl_lib_free(Lp);
    if (Lo) msl_lib_free(Lo);
    if (Los) msl_lib_free(Los);
    printf("%s (%d falhas)\n", g_fail ? "FALHOU" : "PASSOU", g_fail);
    return g_fail ? 1 : 0;
}
