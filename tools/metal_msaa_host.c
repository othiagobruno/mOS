/*
 * Teste de host do MSAA do Metal por software (userland/libsystem/metalexec.c): monta texturas, pipeline e passada
 * direto (sem o runtime ObjC do metal.c) e confere cobertura por amostra, resolve da cor e da profundidade,
 * [[sample_id]], [[sample_mask]], alfa para cobertura, centroid e cópia de textura multisample.
 *
 *   F="-g -O1 -fno-sanitize=all -ffreestanding -fno-builtin -w -Iuserland/libsystem/kshim -Iuserland/libsystem/fsinc \
 *      -Ikernel/include -Iuserland/libsystem"
 *   for f in metalexec mtltex mtlshader mtlexec mslspirv; do zig cc $F -c userland/libsystem/$f.c -o /tmp/mm-$f.o; done
 *   zig cc -g -O1 -fno-sanitize=all -w -Iuserland/libsystem/kshim -Iuserland/libsystem/fsinc -Ikernel/include \
 *      -Iuserland/libsystem -c tools/metal_msaa_host.c -o /tmp/mm-t.o
 *   zig cc /tmp/mm-*.o -lm -lpthread -o /tmp/metal_msaa && /tmp/metal_msaa
 */
#include "metalint.h"
#include "mtltex.h"

int printf(const char *, ...);
void *calloc(unsigned long, unsigned long);
void mx_exec_cmd(cmd_t *c);
static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static int near(int a, int b, int t) { return a - b <= t && b - a <= t; }

/* o metalexec.c e o interpretador pedem estes do resto do libSystem */
void mx_signal_event(id ev, u64 v) { (void)ev; (void)v; }
int *__error(void) { static int e; return &e; }
int cx_fdprintf(int fd, const char *fmt, ...) { (void)fd; (void)fmt; return 0; }

static mtex_t *mk_tex(int fmt, int type, int w, int h, int samples) {
    mtex_t *t = calloc(1, sizeof *t);
    t->fmt = fmt; t->type = type; t->w = w; t->h = h; t->d = 1; t->levels = 1; t->arrlen = 1; t->samples = samples;
    mx_tex_layout(t, 0);
    t->alloc = calloc(1, mx_tex_bytes(t));
    t->t.data = t->alloc;
    return t;
}
static msl_lib_t *g_lib;
static const char *SRC =
    "#include <metal_stdlib>\nusing namespace metal;\n"
    "struct VO { float4 pos [[position]]; float4 col; float u; float cu [[centroid_perspective]]; };\n"
    "vertex VO vs(uint vid [[vertex_id]], constant float4 *p [[buffer(0)]], constant float4 &col [[buffer(1)]]) {\n"
    "  VO o; o.pos = float4(p[vid].xy, p[vid].z, 1.0); o.col = col; o.u = p[vid].w; o.cu = p[vid].w; return o; }\n"
    "fragment float4 fs(VO in [[stage_in]]) { return in.col; }\n"
    "fragment float4 fs_id(VO in [[stage_in]], uint s [[sample_id]]) { return float4(float(s) / 3.0, 0.0, 0.0, 1.0); }\n"
    "struct FO { float4 c [[color(0)]]; uint m [[sample_mask]]; };\n"
    "fragment FO fs_mask(VO in [[stage_in]]) { FO o; o.c = float4(1.0); o.m = 1u; return o; }\n"
    "fragment float4 fs_u(VO in [[stage_in]]) { return float4(in.u, 0.0, 0.0, 1.0); }\n"
    "fragment float4 fs_cu(VO in [[stage_in]]) { return float4(in.cu, 0.0, 0.0, 1.0); }\n";

static void mk_pipe(mrpipe_t *p, const char *fname) {
    memset(p, 0, sizeof *p);
    p->vfn = msl_find(g_lib, "vs");
    p->ffn = msl_find(g_lib, fname);
    p->raster = 1;
    p->ca[0].fmt = 70; p->ca[0].wmask = 0xF; p->ca[0].srgb = 1; p->ca[0].sa = 1;
    p->nvout = msl_fn_out(p->vfn, &p->vout, &p->vout_size);
    p->nvin = msl_fn_stagein(p->vfn, &p->vin, &p->vin_size);
    p->pos_field = p->psize_field = -1;
    for (int i = 0; i < p->nvout; i++) if (p->vout[i].attr == A_POSITION) p->pos_field = i;
    for (int i = 0; i < 8; i++) p->fcolor[i] = -1;
    p->fdepth = p->fmask = -1;
    p->nfin = msl_fn_stagein(p->ffn, &p->fin, &p->fin_size);
    p->nfout = msl_fn_out(p->ffn, &p->fout, &p->fout_size);
    /* o mesmo casamento do make_rpipe (metal.c) */
    for (int i = 0; i < p->nfin; i++) {
        const msl_field_t *fi = &p->fin[i];
        p->fmap[i] = -1;
        p->finterp[i] = fi->interp & 0xF;
        if (fi->interp & MSL_INTERP_CENTROID) p->centroid |= 1u << i;
        if (fi->interp & MSL_INTERP_SAMPLE) p->per_sample = 1;
        if (fi->attr == A_POSITION) { p->fmap[i] = -2; continue; }
        for (int j = 0; j < p->nvout; j++) if (!strcmp(fi->name, p->vout[j].name)) { p->fmap[i] = j; break; }
    }
    for (int i = 0; i < p->nfout; i++) {
        if (p->fout[i].attr == A_COLOR && p->fout[i].idx >= 0 && p->fout[i].idx < 8) p->fcolor[p->fout[i].idx] = i;
        if (p->fout[i].attr == A_DEPTH) p->fdepth = i;
        if (p->fout[i].attr == A_SAMPLE_MASK) p->fmask = i;
    }
    p->discards = msl_fn_discards(p->ffn);
    if (msl_fn_has_param(p->ffn, A_SAMPLE_ID)) p->per_sample = 1;
}
/* x0..x1 em pixels (alvo 8x8; NDC = px/4 - 1), z de cada lado; w = valor de u */
static float g_v[6][4];
static void quad(float x0, float x1, float z0, float z1) {
    float a = x0 / 4 - 1, b = x1 / 4 - 1;
    float v[6][4] = {{a, -1, z0, x0 / 8}, {b, -1, z1, x1 / 8}, {b, 1, z1, x1 / 8}, {a, -1, z0, x0 / 8}, {b, 1, z1, x1 / 8}, {a, 1, z0, x0 / 8}};
    memcpy(g_v, v, sizeof v);
}
static float g_col[4];
static void pass(rpass_t *rp, rstate_t *st, int draw) {
    cmd_t c;
    memset(&c, 0, sizeof c);
    c.k = C_RBEGIN; c.rp = rp; mx_exec_cmd(&c);
    if (draw) {
        memset(&c, 0, sizeof c);
        c.k = C_DRAW; c.d.st = st; c.d.prim = 3; c.d.vcount = 6; c.d.icount = 1;
        mx_exec_cmd(&c);
    }
    memset(&c, 0, sizeof c);
    c.k = C_REND; mx_exec_cmd(&c);
}
static int red(mtex_t *t, int x, int y, int slice) { double v[4]; msl_tex_load(&t->t, x, y, 0, slice, 0, v); return (int)(v[0] * 255 + 0.5); }

int main(void) {
    char err[512] = {0};
    g_lib = msl_compile(SRC, strlen(SRC), 0, err, sizeof err);
    CHECK(g_lib != 0, "MSL compila: %s", err);
    if (!g_lib) return 1;
    mtex_t *ms = mk_tex(70, MSL_TEX_2D_MS, 8, 8, 4), *one = mk_tex(70, MSL_TEX_2D, 8, 8, 1);
    CHECK(ms->t.samples == 4 && ms->t.slices == 4, "textura 4x: %d amostras, %d fatias", ms->t.samples, ms->t.slices);
    mrpipe_t pp;
    rstate_t st;
    rpass_t rp;
    memset(&st, 0, sizeof st);
    st.pipe = &pp;
    st.vb[0].bytes = g_v;
    st.vb[1].bytes = g_col;
    memset(&rp, 0, sizeof rp);
    rp.c[0].tex = ms; rp.c[0].load = 2; rp.c[0].store = 2; rp.c[0].resolve = one;
    rp.c[0].cc.a = 1;
    /* borda em x = 2.6: 2 das 4 amostras do pixel 2 (x = 0.375, 0.875, 0.125, 0.625) */
    mk_pipe(&pp, "fs");
    g_col[0] = 1; g_col[3] = 1;
    quad(0, 2.6f, 0.5f, 0.5f);
    pass(&rp, &st, 1);
    CHECK(red(one, 1, 4, 0) == 255, "pixel coberto %d", red(one, 1, 4, 0));
    CHECK(near(red(one, 2, 4, 0), 128, 1), "resolve da borda com 2 de 4 amostras: %d", red(one, 2, 4, 0));
    CHECK(red(one, 3, 4, 0) == 0, "pixel fora %d", red(one, 3, 4, 0));
    /* [[sample_id]]: o shader roda por amostra; cada fatia guarda a sua */
    mk_pipe(&pp, "fs_id");
    CHECK(pp.per_sample, "fs com [[sample_id]] roda por amostra");
    quad(0, 8, 0.5f, 0.5f);
    pass(&rp, &st, 1);
    int okid = 1;
    for (int k = 0; k < 4; k++) if (!near(red(ms, 4, 4, k), k * 85, 1)) okid = 0;
    CHECK(okid, "[[sample_id]] por fatia: %d %d %d %d", red(ms, 4, 4, 0), red(ms, 4, 4, 1), red(ms, 4, 4, 2), red(ms, 4, 4, 3));
    CHECK(near(red(one, 4, 4, 0), 128, 1), "resolve da média (0+85+170+255)/4: %d", red(one, 4, 4, 0));
    /* [[sample_mask]] = 1: só a amostra 0 */
    mk_pipe(&pp, "fs_mask");
    pass(&rp, &st, 1);
    CHECK(near(red(one, 4, 4, 0), 64, 1), "[[sample_mask]] 1 de 4: %d", red(one, 4, 4, 0));
    /* alfa -> cobertura: alfa 0.5 cobre 2 amostras */
    mk_pipe(&pp, "fs");
    pp.a2c = 1;
    g_col[0] = 1; g_col[3] = 0.5f;
    pass(&rp, &st, 1);
    CHECK(near(red(one, 4, 4, 0), 128, 1), "alfa 0.5 -> 2 amostras: %d", red(one, 4, 4, 0));
    pp.a2c = 0;
    g_col[3] = 1;
    /* centroid: centro do pixel 2 (2.5) fora da borda 2.4; a amostra 0 (2.375) está dentro */
    for (int cen = 0; cen < 2; cen++) {
        mk_pipe(&pp, cen ? "fs_cu" : "fs_u");
        quad(0, 2.4f, 0.5f, 0.5f);
        pass(&rp, &st, 1);
        int r = red(ms, 2, 4, 0);
        if (cen) CHECK(near(r, 76, 1), "centroid_perspective na amostra coberta: %d", r);
        else CHECK(near(r, 80, 1), "sem centroid: valor do centro %d", r);
    }
    /* profundidade por amostra e resolve com filtro min/max */
    mtex_t *dms = mk_tex(252, MSL_TEX_2D_MS, 8, 8, 4), *done = mk_tex(252, MSL_TEX_2D, 8, 8, 1);
    mdss_t dss;
    memset(&dss, 0, sizeof dss);
    dss.dcmp = 1; dss.dwrite = 1;   /* less */
    st.dss = &dss;
    rp.d.tex = dms; rp.d.load = 2; rp.d.cd = 1; rp.d.store = 2; rp.d.resolve = done;
    mk_pipe(&pp, "fs");
    g_col[0] = 1;
    quad(0, 8, 0, 1);   /* z = x / 8 */
    for (int f = 0; f < 3; f++) {
        rp.d.rfilter = f;
        pass(&rp, &st, 1);
        double v[4];
        msl_tex_load(&done->t, 4, 4, 0, 0, 0, v);
        /* amostras do pixel 4: x = 4.375, 4.875, 4.125, 4.625 */
        double want = f == 0 ? 4.375 / 8 : f == 1 ? 4.125 / 8 : 4.875 / 8;
        CHECK(v[0] > want - 1e-4 && v[0] < want + 1e-4, "resolve da profundidade (filtro %d): %f (esperado %f)", f, v[0], want);
    }
    /* profundidade por amostra: plano z = 0.5 à frente da rampa só em parte do pixel 4 */
    rp.d.rfilter = 0;
    rp.d.load = 1;   /* mantém a rampa e a cor vermelha dela */
    rp.c[0].load = 1;
    g_col[0] = 0; g_col[2] = 1;
    quad(0, 8, 0.53125f, 0.53125f);   /* 4.25 / 8: na frente das amostras 4.375, 4.875, 4.625 */
    pass(&rp, &st, 1);
    rp.c[0].load = 2;
    double bv[4];
    msl_tex_load(&one->t, 4, 4, 0, 0, 0, bv);
    CHECK(near(red(one, 4, 4, 0), 64, 1) && near((int)(bv[2] * 255 + 0.5), 191, 1), "profundidade em cada amostra: %d %d", red(one, 4, 4, 0), (int)(bv[2] * 255 + 0.5));
    st.dss = 0;
    memset(&rp.d, 0, sizeof rp.d);
    /* cópia de textura multisample amostra a amostra */
    mtex_t *ms2 = mk_tex(70, MSL_TEX_2D_MS, 8, 8, 4);
    cmd_t c;
    memset(&c, 0, sizeof c);
    c.k = C_COPY_TT; c.tt.s = ms; c.tt.d = ms2; c.tt.nlevels = 1; c.tt.nslices = 1;
    mx_exec_cmd(&c);
    int okc = 1;
    for (int k = 0; k < 4; k++) if (red(ms2, 4, 4, k) != red(ms, 4, 4, k)) okc = 0;
    CHECK(okc, "cópia multisample preserva cada amostra");
    float sp[2];
    mx_sample_pos(4, 1, sp);
    CHECK(sp[0] == 0.875f && sp[1] == 0.375f, "posição padrão da amostra 1 de 4: %g %g", sp[0], sp[1]);
    printf("metal_msaa: %d/%d ok\n", checks - fails, checks);
    return fails != 0;
}
