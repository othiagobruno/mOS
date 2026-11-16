/* mOS — teste de host do compilador de kernels Metal -> GCN/GFX9 (userland/libsystem/amdcc.c).
 * Compila kernels MSL com o compilador do libSystem, roda o código gerado num emulador de wave de 64 lanes (só as
 * instruções que o amdcc emite, decodificadas pelos campos da ISA Vega) e compara a memória com o resultado
 * esperado. Também confere que o que não é suportado volta com o motivo (o kernel fica no interpretador).
 *   L=userland/libsystem
 *   gcc -std=gnu11 -O1 -w -fno-builtin -I tools/hostinc -I $L tools/amdcc_host.c $L/amdcc.c $L/amdisa.c $L/mtlshader.c \
 *       $L/mtlexec.c $L/mslspirv.c $L/mtltex.c -lm -lpthread -o /tmp/amdcc && /tmp/amdcc */
#include "amdcc.h"
#include "mtlint.h"
#include "libc.h"

int printf(const char *, ...);
double sqrt(double), floor(double), ceil(double), trunc(double), rint(double), exp2(double), log2(double);
int *__error(void) { static int e; return &e; }
int cx_fdprintf(int fd, const char *fmt, ...) { (void)fd; (void)fmt; return 0; }
static int pass, total;
#define CHECK(c, ...) do { total++; if (c) pass++; else { printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ================================================================ memória da GPU emulada */
typedef unsigned long long u64x;
typedef struct { u64x va; unsigned char *p; u64x size; } region_t;
static region_t rg[16];
static int nrg;
static u64x next_va = 0x100000000ull;
static u64x gpu_alloc(void *host, u64x size) { rg[nrg] = (region_t){next_va, host, size}; next_va += (size + 0xFFFF) & ~0xFFFFull; return rg[nrg++].va; }
static int mem_fault;
static unsigned char *mem(u64x va, unsigned n) {
    for (int i = 0; i < nrg; i++) if (va >= rg[i].va && va + n <= rg[i].va + rg[i].size) return rg[i].p + (va - rg[i].va);
    mem_fault = 1;
    static unsigned char dummy[64];
    return dummy;
}

/* ================================================================ emulador de uma wave */
typedef struct { unsigned s[128]; unsigned v[256][64]; u64x exec, vcc; int scc; } wave_t;
static int emu_err;
static float uf(unsigned x) { float f; memcpy(&f, &x, 4); return f; }
static unsigned fu(float f) { unsigned x; memcpy(&x, &f, 4); return x; }
static unsigned sreg(wave_t *w, unsigned r) {
    if (r < 102) return w->s[r];
    if (r == 106) return (unsigned)w->vcc;
    if (r == 107) return (unsigned)(w->vcc >> 32);
    if (r == 126) return (unsigned)w->exec;
    if (r == 127) return (unsigned)(w->exec >> 32);
    if (r >= 128 && r <= 192) return r - 128;
    if (r >= 193 && r <= 208) return (unsigned)-(int)(r - 192);
    static const float fc[8] = {0.5f, -0.5f, 1, -1, 2, -2, 4, -4};
    if (r >= 240 && r <= 247) return fu(fc[r - 240]);
    emu_err = 1;
    return 0;
}
static u64x sreg64(wave_t *w, unsigned r) {
    if (r == 106) return w->vcc;
    if (r == 126) return w->exec;
    if (r == 128) return 0;
    if (r < 102) return (u64x)w->s[r] | (u64x)w->s[r + 1] << 32;
    emu_err = 1;
    return 0;
}
static void swrite64(wave_t *w, unsigned r, u64x v) {
    if (r == 106) w->vcc = v;
    else if (r == 126) w->exec = v;
    else if (r < 102) { w->s[r] = (unsigned)v; w->s[r + 1] = (unsigned)(v >> 32); }
    else emu_err = 1;
}
static unsigned src(wave_t *w, unsigned r, int lane, unsigned lit) { return r >= 256 ? w->v[r - 256][lane] : r == 255 ? lit : sreg(w, r); }
static int cmpf(unsigned op, float a, float b) {
    switch (op & 0xF) { case 1: return a < b; case 2: return a == b; case 3: return a <= b; case 4: return a > b; case 6: return a >= b; case 0xD: return !(a == b); }
    emu_err = 1; return 0;
}
static int cmpi(unsigned op, int a, int b) { switch (op & 7) { case 1: return a < b; case 2: return a == b; case 3: return a <= b; case 4: return a > b; case 5: return a != b; case 6: return a >= b; } emu_err = 1; return 0; }
static int cmpu(unsigned op, unsigned a, unsigned b) { switch (op & 7) { case 1: return a < b; case 2: return a == b; case 3: return a <= b; case 4: return a > b; case 5: return a != b; case 6: return a >= b; } emu_err = 1; return 0; }
static unsigned valu1(unsigned op, unsigned x) {
    float f = uf(x);
    switch (op) {
    case 1: return x;
    case 5: return fu((float)(int)x);
    case 6: return fu((float)x);
    case 7: return f <= 0 ? 0 : (unsigned)f;
    case 8: return (unsigned)(int)f;
    case 27: return fu(f - (float)floor(f));
    case 28: return fu((float)trunc(f));
    case 29: return fu((float)ceil(f));
    case 30: return fu((float)rint(f));
    case 31: return fu((float)floor(f));
    case 32: return fu((float)exp2(f));
    case 33: return fu((float)log2(f));
    case 34: return fu(1.0f / f);
    case 36: return fu((float)(1.0 / sqrt(f)));
    case 39: return fu((float)sqrt(f));
    case 43: return ~x;
    }
    emu_err = 1;
    return 0;
}
/* VOP2 numa lane; carry: VCC de entrada/saída do add_co/addc */
static unsigned valu2(wave_t *w, unsigned op, unsigned a, unsigned b, int lane, u64x *vcc_out) {
    float fa = uf(a), fb = uf(b);
    switch (op) {
    case 0: return (w->vcc >> lane & 1) ? b : a;
    case 1: return fu(fa + fb);
    case 2: return fu(fa - fb);
    case 3: return fu(fb - fa);
    case 5: return fu(fa * fb);
    case 10: return fu(fa < fb ? fa : fb);
    case 11: return fu(fa > fb ? fa : fb);
    case 12: return (int)a < (int)b ? a : b;
    case 13: return (int)a > (int)b ? a : b;
    case 14: return a < b ? a : b;
    case 15: return a > b ? a : b;
    case 16: return b >> (a & 31);
    case 17: return (unsigned)((int)b >> (a & 31));
    case 18: return b << (a & 31);
    case 19: return a & b;
    case 20: return a | b;
    case 21: return a ^ b;
    case 25: { u64x s = (u64x)a + b; if (s >> 32) *vcc_out |= 1ull << lane; return (unsigned)s; }
    case 28: { u64x s = (u64x)a + b + (w->vcc >> lane & 1); if (s >> 32) *vcc_out |= 1ull << lane; return (unsigned)s; }
    case 52: return a + b;
    case 53: return a - b;
    case 54: return b - a;
    }
    emu_err = 1;
    return 0;
}
/* roda a wave; devolve as instruções executadas */
static long run_wave(wave_t *w, const unsigned *code, unsigned n) {
    long steps = 0;
    for (unsigned pc = 0; pc < n && steps < 2000000; steps++) {
        unsigned i = code[pc];
        if (i >> 23 == 0x17F) {                                  /* SOPP */
            unsigned op = i >> 16 & 0x7F;
            int simm = (short)(i & 0xFFFF);
            if (op == 1) return steps;
            if (op == 2) { pc = pc + 1 + simm; continue; }
            if (op == 8) { pc = w->exec ? pc + 1 : pc + 1 + simm; continue; }
            if (op == 12 || op == 10 || op == 0) { pc++; continue; }
            emu_err = 1; return steps;
        }
        if (i >> 23 == 0x17D) {                                  /* SOP1 */
            unsigned op = i >> 8 & 0xFF, d = i >> 16 & 0x7F, s0 = i & 0xFF, lit = s0 == 255 ? code[pc + 1] : 0;
            if (op == 0) { unsigned v = s0 == 255 ? lit : sreg(w, s0); if (d < 102) w->s[d] = v; else emu_err = 1; }
            else if (op == 1) swrite64(w, d, sreg64(w, s0));
            else if (op == 32) { u64x old = w->exec; w->exec &= sreg64(w, s0); swrite64(w, d, old); }
            else emu_err = 1;
            pc += s0 == 255 ? 2 : 1;
            continue;
        }
        if (i >> 26 == 0x30) {                                   /* SMEM */
            unsigned op = i >> 18 & 0xFF, sd = i >> 6 & 0x7F, sb = (i & 0x3F) << 1, off = code[pc + 1] & 0x1FFFFF;
            u64x a = ((u64x)w->s[sb] | (u64x)w->s[sb + 1] << 32) + off;
            unsigned nd = 1u << op;
            memcpy(&w->s[sd], mem(a, 4 * nd), 4 * nd);
            pc += 2;
            continue;
        }
        if (i >> 30 == 2) {                                      /* SOP2 */
            unsigned op = i >> 23 & 0x7F, d = i >> 16 & 0x7F, s1 = i >> 8 & 0xFF, s0 = i & 0xFF;
            unsigned lit = (s0 == 255 || s1 == 255) ? code[pc + 1] : 0;
            if (op == 13 || op == 15 || op == 19) {
                u64x a = sreg64(w, s0), b = sreg64(w, s1);
                swrite64(w, d, op == 13 ? a & b : op == 15 ? a | b : a & ~b);
            } else {
                unsigned a = s0 == 255 ? lit : sreg(w, s0), b = s1 == 255 ? lit : sreg(w, s1), r = 0;
                if (op == 0) r = a + b; else if (op == 28) r = a << (b & 31); else if (op == 36) r = a * b; else emu_err = 1;
                w->s[d] = r;
            }
            pc += lit || s0 == 255 || s1 == 255 ? 2 : 1;
            continue;
        }
        if (i >> 26 == 0x37) {                                   /* GLOBAL */
            unsigned op = i >> 18 & 0x7F, d1 = code[pc + 1];
            int off = (int)(i << 19) >> 19;
            unsigned vdst = d1 >> 24, saddr = d1 >> 16 & 0x7F, vdata = d1 >> 8 & 0xFF, vaddr = d1 & 0xFF;
            if (saddr != 0x7F) emu_err = 1;
            for (int l = 0; l < 64; l++) {
                if (!(w->exec >> l & 1)) continue;
                u64x a = ((u64x)w->v[vaddr][l] | (u64x)w->v[vaddr + 1][l] << 32) + (u64x)(long long)off;
                if (op >= 20 && op <= 23) { unsigned nd = op - 19; for (unsigned k = 0; k < nd; k++) memcpy(&w->v[vdst + k][l], mem(a + 4 * k, 4), 4); }
                else if (op >= 28 && op <= 31) { unsigned nd = op - 27; for (unsigned k = 0; k < nd; k++) memcpy(mem(a + 4 * k, 4), &w->v[vdata + k][l], 4); }
                else emu_err = 1;
            }
            pc += 2;
            continue;
        }
        if (i >> 26 == 0x34) {                                   /* VOP3 (a e b) */
            unsigned op = i >> 16 & 0x3FF, d = i & 0xFF, sdst = i >> 8 & 0x7F, d1 = code[pc + 1];
            unsigned s0 = d1 & 0x1FF, s1 = d1 >> 9 & 0x1FF, s2 = d1 >> 18 & 0x1FF;
            for (int l = 0; l < 64; l++) {
                if (!(w->exec >> l & 1)) continue;
                unsigned a = src(w, s0, l, 0), b = src(w, s1, l, 0);
                if (op == 0x100) { u64x m = s2 >= 256 ? 0 : sreg64(w, s2); w->v[d][l] = (m >> l & 1) ? b : a; }
                else if (op == 0x1CB) w->v[d][l] = fu(uf(a) * uf(b) + uf(src(w, s2, l, 0)));
                else if (op == 0x285) w->v[d][l] = a * b;
                else if (op == 0x1E8 || op == 0x1E9) {
                    u64x c = s2 >= 256 ? ((u64x)w->v[s2 - 256][l] | (u64x)w->v[s2 - 255][l] << 32) : sreg64(w, s2);
                    u64x p = op == 0x1E8 ? (u64x)a * b : (u64x)((long long)(int)a * (long long)(int)b);
                    u64x r = p + c;
                    w->v[d][l] = (unsigned)r; w->v[d + 1][l] = (unsigned)(r >> 32);
                    (void)sdst;
                } else emu_err = 1;
            }
            pc += 2;
            continue;
        }
        if (i >> 25 == 0x3F) {                                   /* VOP1 */
            unsigned op = i >> 9 & 0xFF, d = i >> 17 & 0xFF, s0 = i & 0x1FF, lit = s0 == 255 ? code[pc + 1] : 0;
            for (int l = 0; l < 64; l++) if (w->exec >> l & 1) w->v[d][l] = valu1(op, src(w, s0, l, lit));
            pc += s0 == 255 ? 2 : 1;
            continue;
        }
        if (i >> 25 == 0x3E) {                                   /* VOPC */
            unsigned op = i >> 17 & 0xFF, v1 = i >> 9 & 0xFF, s0 = i & 0x1FF, lit = s0 == 255 ? code[pc + 1] : 0;
            u64x m = 0;
            for (int l = 0; l < 64; l++) {
                if (!(w->exec >> l & 1)) continue;
                unsigned a = src(w, s0, l, lit), b = w->v[v1][l];
                int r = op >= 0x40 && op < 0x50 ? cmpf(op, uf(a), uf(b)) : op >= 0xC0 && op < 0xC8 ? cmpi(op, (int)a, (int)b) : op >= 0xC8 && op < 0xD0 ? cmpu(op, a, b) : (emu_err = 1, 0);
                if (r) m |= 1ull << l;
            }
            w->vcc = m;
            pc += s0 == 255 ? 2 : 1;
            continue;
        }
        if (!(i >> 31)) {                                        /* VOP2 */
            unsigned op = i >> 25 & 0x3F, d = i >> 17 & 0xFF, v1 = i >> 9 & 0xFF, s0 = i & 0x1FF, lit = s0 == 255 ? code[pc + 1] : 0;
            u64x vo = 0;
            int carry = op == 25 || op == 28;
            for (int l = 0; l < 64; l++) if (w->exec >> l & 1) w->v[d][l] = valu2(w, op, src(w, s0, l, lit), w->v[v1][l], l, &vo);
            if (carry) w->vcc = vo;
            pc += s0 == 255 ? 2 : 1;
            continue;
        }
        printf("instrução desconhecida %08x em %u\n", i, pc);
        emu_err = 1;
        return steps;
    }
    return steps;
}
/* despacho: grade gx (1D) com grupos de lx (<= 64) */
static int dispatch(const amdcc_kernel_t *k, amdcc_args_t *args, unsigned gx, unsigned lx) {
    static amdcc_args_t table __attribute__((aligned(256)));
    table = *args;
    table.local[0] = lx; table.local[1] = 1; table.local[2] = 1;
    table.grid[0] = gx; table.grid[1] = 1; table.grid[2] = 1;
    u64x tva = gpu_alloc(&table, sizeof table);
    emu_err = 0;
    mem_fault = 0;
    for (unsigned g = 0; g * lx < gx; g++) {
        static wave_t w;
        memset(&w, 0, sizeof w);
        w.s[0] = (unsigned)tva; w.s[1] = (unsigned)(tva >> 32);
        w.s[2] = g; w.s[3] = 0; w.s[4] = 0;
        for (unsigned l = 0; l < 64; l++) { w.v[0][l] = l; w.v[1][l] = 0; w.v[2][l] = 0; }
        w.exec = lx >= 64 ? ~0ull : (1ull << lx) - 1;
        run_wave(&w, k->code, k->ncode);
        if (emu_err || mem_fault) return 0;
    }
    nrg--;   /* a tabela */
    return 1;
}
static msl_fn_t *kernel(const char *src, const char *name) {
    char err[256] = {0};
    msl_lib_t *L = msl_compile(src, strlen(src), 0, err, sizeof err);
    if (!L) { printf("MSL: %s\n", err); return 0; }
    return msl_find(L, name);
}

int main(void) {
    amdcc_kernel_t k;
    /* 1. guarda com return, load, aritmética float, conversão, referência constant */
    msl_fn_t *f = kernel("#include <metal_stdlib>\nusing namespace metal;\n"
                         "kernel void k1(device float *out [[buffer(0)]], device const float *in [[buffer(1)]], constant uint &n [[buffer(2)]],\n"
                         "               uint gid [[thread_position_in_grid]]) {\n"
                         "  if (gid >= n) return;\n  float x = in[gid];\n  out[gid] = x * 2.0 + float(gid);\n}\n", "k1");
    CHECK(f && amdcc_compile(f, &k), "k1 compila: %s", k.why);
    static float in[200], out[200];
    unsigned n = 150;
    for (int i = 0; i < 200; i++) { in[i] = (float)i * 0.5f; out[i] = -1; }
    amdcc_args_t a;
    memset(&a, 0, sizeof a);
    a.buf[0] = gpu_alloc(out, sizeof out); a.buf[1] = gpu_alloc(in, sizeof in); a.buf[2] = gpu_alloc(&n, 4);
    CHECK(dispatch(&k, &a, 200, 64), "k1 roda no emulador");
    int ok = 1;
    for (int i = 0; i < 200; i++) { float want = i < 150 ? in[i] * 2 + i : -1; if (out[i] != want) { ok = 0; printf("k1[%d] = %g (esperado %g)\n", i, out[i], want); break; } }
    CHECK(ok, "k1: out[gid] = in[gid] * 2 + gid só para gid < n");
    CHECK(k.used_buf == 7, "buffers usados %x", k.used_buf);
    amdcc_free(&k);
    nrg = 0;

    /* 2. laço divergente (número de voltas por lane), if/else, operações inteiras */
    f = kernel("#include <metal_stdlib>\nusing namespace metal;\n"
               "kernel void k2(device uint *o [[buffer(0)]], uint gid [[thread_position_in_grid]]) {\n"
               "  uint s = 0;\n  for (uint i = 0; i <= gid; i++) { s += i; }\n"
               "  uint t;\n  if ((gid & 1u) != 0u) t = s * 3u; else { t = s ^ 0xFFu; t = t << 2; }\n"
               "  int m = int(gid) - 40;\n  if (m < 0) m = -m;\n"
               "  o[gid * 2u] = t;\n  o[gid * 2u + 1u] = uint(m >> 1);\n}\n", "k2");
    CHECK(f && amdcc_compile(f, &k), "k2 compila: %s", k.why);
    static unsigned o2[256];
    memset(o2, 0, sizeof o2);
    memset(&a, 0, sizeof a);
    a.buf[0] = gpu_alloc(o2, sizeof o2);
    CHECK(dispatch(&k, &a, 100, 50), "k2 roda (grupos de 50)");
    ok = 1;
    for (unsigned g = 0; g < 100; g++) {
        unsigned s = g * (g + 1) / 2, t = (g & 1) ? s * 3 : ((s ^ 0xFF) << 2);
        int m = (int)g - 40; if (m < 0) m = -m;
        if (o2[2 * g] != t || o2[2 * g + 1] != (unsigned)(m >> 1)) { ok = 0; printf("k2[%u] = %u %u (esperado %u %u)\n", g, o2[2 * g], o2[2 * g + 1], t, m >> 1); break; }
    }
    CHECK(ok, "k2: laço divergente, if/else e inteiros");
    amdcc_free(&k);
    nrg = 0;

    /* 3. vetores, struct, embutidas, while, ?: */
    f = kernel("#include <metal_stdlib>\nusing namespace metal;\n"
               "struct P { float4 pos; float w; uint id; };\n"
               "kernel void k3(device P *p [[buffer(0)]], device float4 *r [[buffer(1)]], uint gid [[thread_position_in_grid]]) {\n"
               "  float4 v = p[gid].pos;\n  float w = p[gid].w;\n"
               "  float4 c = clamp(v * w, float4(0.0), float4(10.0));\n"
               "  float d = dot(v.xyz, float3(1.0, 2.0, 3.0));\n"
               "  uint k = p[gid].id;\n  uint steps = 0;\n  while (k > 1u) { k = (k & 1u) != 0u ? k * 3u + 1u : k >> 1; steps++; }\n"
               "  r[gid] = float4(c.x, sqrt(max(d, 0.0)), select(1.0, 2.0, w > 1.5), float(steps));\n}\n", "k3");
    CHECK(f && amdcc_compile(f, &k), "k3 compila: %s", k.why);
    typedef struct { float pos[4]; float w; unsigned id; float pad[2]; } P;
    static P pp[64];
    static float rr[64][4];
    for (int i = 0; i < 64; i++) { for (int c = 0; c < 4; c++) pp[i].pos[c] = (float)(i % 7) - 2 + c; pp[i].w = (float)(i % 3); pp[i].id = (unsigned)i + 1; }
    memset(&a, 0, sizeof a);
    a.buf[0] = gpu_alloc(pp, sizeof pp); a.buf[1] = gpu_alloc(rr, sizeof rr);
    CHECK(dispatch(&k, &a, 64, 64), "k3 roda");
    ok = 1;
    for (int i = 0; i < 64; i++) {
        float cx = pp[i].pos[0] * pp[i].w; cx = cx < 0 ? 0 : cx > 10 ? 10 : cx;
        float d = pp[i].pos[0] + 2 * pp[i].pos[1] + 3 * pp[i].pos[2];
        unsigned kk = pp[i].id, st = 0;
        while (kk > 1) { kk = (kk & 1) ? kk * 3 + 1 : kk >> 1; st++; }
        float want[4] = {cx, (float)sqrt(d > 0 ? d : 0), pp[i].w > 1.5f ? 2.0f : 1.0f, (float)st};
        for (int c = 0; c < 4; c++) if (rr[i][c] < want[c] - 1e-4f || rr[i][c] > want[c] + 1e-4f) { ok = 0; printf("k3[%d][%d] = %g (esperado %g)\n", i, c, rr[i][c], want[c]); }
        if (!ok) break;
    }
    CHECK(ok, "k3: float4, struct, clamp/dot/sqrt/select, while divergente");
    amdcc_free(&k);
    nrg = 0;

    /* 4. o que não compila volta com o motivo */
    f = kernel("#include <metal_stdlib>\nusing namespace metal;\n"
               "kernel void k4(texture2d<float, access::write> t [[texture(0)]], uint2 gid [[thread_position_in_grid]]) { t.write(float4(1.0), gid); }\n", "k4");
    CHECK(f && !amdcc_compile(f, &k) && k.why[0], "textura: fica no interpretador (%s)", k.why);
    f = kernel("#include <metal_stdlib>\nusing namespace metal;\n"
               "kernel void k5(device int *o [[buffer(0)]], uint gid [[thread_position_in_grid]]) { o[gid] = int(gid) / 3; }\n", "k5");
    CHECK(f && !amdcc_compile(f, &k), "divisão inteira: fica no interpretador (%s)", k.why);
    printf("amdcc: %d/%d ok\n", pass, total);
    return pass != total;
}
