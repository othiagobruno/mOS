/* mOS PDF — shadings (ISO 32000-1 §8.7.4.5), tipos 1 a 7. Incluído por cgpdf_draw.c.
 * 2 e 3 viram CGShading; 1 (por função) e 4–7 (malhas) passam pelo raster próprio por pixel (sh_rast). */

typedef double CGFloat_;
typedef struct { u32 version; void (*evaluate)(void *info, const double *in, double *out); void (*release)(void *info); } pcgfn_cb;
void *CGFunctionCreate(void *info, size_t ndom, const double *dom, size_t nrange, const double *range, const pcgfn_cb *cb);
void CGFunctionRelease(void *f);
void *CGShadingCreateAxial(void *space, CGPoint s, CGPoint e, void *fn, bool e0, bool e1);
void *CGShadingCreateRadial(void *space, CGPoint s, double r0, CGPoint e, double r1, void *fn, bool e0, bool e1);
void CGShadingRelease(void *s);
void CGContextDrawShading(void *c, void *s);
void CGContextSetShouldAntialias(void *c, int v);
void CGContextSetFillColorWithColor(void *c, void *color);
void CGContextFillPath(void *c);
void CGContextBeginPath(void *c);
void CGContextMoveToPoint(void *c, double x, double y);
void CGContextAddLineToPoint(void *c, double x, double y);
void CGContextClosePath(void *c);
void CGContextAddRect(void *c, CGRect r);
void CGContextClip(void *c);
void CGContextSaveGState(void *c);
void CGContextRestoreGState(void *c);
void CGContextConcatCTM(void *c, CGAffineTransform t);
CGAffineTransform CGContextGetCTM(void *c);
void CFRelease(void *o);

typedef struct { pf_set fn; const pcs *cs; pcs *target; int nout; } sh_ctx;
/* cor do shading (função ou componentes diretos) → componentes do espaço que o CG representa */
static void sh_eval(void *info, const double *in, double *out) {
    sh_ctx *c = info;
    double v[PF_MAXOUT] = {0};
    pfs_eval(&c->fn, in, v);
    double o[PF_MAXOUT] = {0};
    pcs_cgcomps(c->cs, v, o);
    for (int i = 0; i < c->nout; i++) out[i] = o[i];
}
static void *sh_color(const pcs *cs, const pf_set *fn, const double *t, int nt) {
    double v[PF_MAXOUT] = {0};
    if (fn && fn->nf) pfs_eval(fn, t, v);
    else for (int i = 0; i < nt && i < PF_MAXOUT; i++) v[i] = t[i];
    return pcs_color(cs, v, 1);
}

/* leitor de bits dos streams de malha */
typedef struct { const u8 *d; size_t n, bit; } sh_bits;
static double sh_read(sh_bits *b, int nbits, double lo, double hi) {
    u64 v = 0;
    for (int i = 0; i < nbits; i++, b->bit++) v = v << 1 | (b->bit / 8 < b->n ? (u64)(b->d[b->bit / 8] >> (7 - b->bit % 8) & 1) : 0);
    double mx = nbits >= 64 ? 18446744073709551615.0 : (double)((1ull << nbits) - 1);
    return lo + (double)v * (hi - lo) / mx;
}
static void sh_align(sh_bits *b) { b->bit = (b->bit + 7) & ~(size_t)7; }

typedef struct { double x, y, c[PF_MAXOUT]; } sh_vtx;

/* Raster próprio das malhas e do tipo 1: cada pixel do dispositivo recebe a cor interpolada (Gouraud) ou
 * avaliada no ponto; o resultado vira uma imagem desenhada com a CTM identidade. Sem fendas entre peças. */
void CGDataProviderRelease(void *p);
void CGImageRelease(void *i);
void CGContextDrawImage(void *c, CGRect r, void *img);
CGRect CGContextGetClipBoundingBox(void *c);
typedef struct {
    u32 *px;
    int x0, y0, w, h;
    CGAffineTransform m;                 /* usuário → dispositivo */
    const pcs *cs;
    const pf_set *fn;
    int nc;
    double lo, hi;                       /* faixa do parâmetro t (função de 1 entrada) */
    u32 lut[1024];
    int has_lut;
    u32 ckey[4096], cval[4096];          /* cache de cores para várias componentes */
} sh_rast;
static CGAffineTransform tinv_(CGAffineTransform m) {
    double det = m.a * m.d - m.b * m.c;
    CGAffineTransform z = {1, 0, 0, 1, 0, 0};
    if (det > -1e-14 && det < 1e-14) return z;
    CGAffineTransform r = {m.d / det, -m.b / det, -m.c / det, m.a / det, (m.c * m.ty - m.d * m.tx) / det, (m.b * m.tx - m.a * m.ty) / det};
    return r;
}
static void sh_free_cb(void *info, const void *data, size_t n) { (void)info; (void)n; free((void *)data); }
static u32 sh_argb_slow(const sh_rast *r, const double *in) {
    void *k = sh_color(r->cs, r->fn, in, r->nc);
    if (!k) return 0;
    const double *o = k;                 /* NSColor do mOS: {isa, r, g, b, a} */
    u32 R = (u32)(pf_clamp(o[1], 0, 1) * 255 + .5), G2 = (u32)(pf_clamp(o[2], 0, 1) * 255 + .5), B = (u32)(pf_clamp(o[3], 0, 1) * 255 + .5);
    CFRelease(k);
    return 0xFF000000u | R << 16 | G2 << 8 | B;
}
static u32 sh_argb(sh_rast *r, const double *in) {
    if (r->has_lut) {
        double t = r->hi != r->lo ? (in[0] - r->lo) / (r->hi - r->lo) : 0;
        int i = (int)(pf_clamp(t, 0, 1) * 1023 + .5);
        return r->lut[i];
    }
    u32 key = 0x80000000u;
    for (int i = 0; i < r->nc && i < 4; i++) key = key * 31 + (u32)(pf_clamp(in[i], -4, 4) * 255 + 1024);
    u32 slot = (key ^ key >> 13) & 4095;
    if (r->ckey[slot] == key) return r->cval[slot];
    u32 v = sh_argb_slow(r, in);
    r->ckey[slot] = key; r->cval[slot] = v;
    return v;
}
static int sh_rast_begin(sh_rast *r, void *ctx, const pcs *cs, const pf_set *fn, int nc, double lo, double hi) {
    memset(r, 0, sizeof *r);
    r->m = CGContextGetCTM(ctx);
    r->cs = cs; r->fn = fn; r->nc = nc; r->lo = lo; r->hi = hi;
    CGRect cb = CGContextGetClipBoundingBox(ctx);
    double xs[4] = {cb.origin.x, cb.origin.x + cb.size.width, cb.origin.x, cb.origin.x + cb.size.width};
    double ys[4] = {cb.origin.y, cb.origin.y, cb.origin.y + cb.size.height, cb.origin.y + cb.size.height};
    double x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
    for (int i = 0; i < 4; i++) {
        double X = r->m.a * xs[i] + r->m.c * ys[i] + r->m.tx, Y = r->m.b * xs[i] + r->m.d * ys[i] + r->m.ty;
        if (X < x0) x0 = X;
        if (X > x1) x1 = X;
        if (Y < y0) y0 = Y;
        if (Y > y1) y1 = Y;
    }
    if (!(x1 > x0 && y1 > y0) || x1 - x0 > 8192 || y1 - y0 > 8192) return 0;
    r->x0 = (int)floor(x0); r->y0 = (int)floor(y0);
    r->w = (int)ceil(x1) - r->x0; r->h = (int)ceil(y1) - r->y0;
    if (r->w <= 0 || r->h <= 0) return 0;
    r->px = calloc((size_t)r->w * r->h, 4);
    if (!r->px) return 0;
    if (nc == 1) {
        r->has_lut = 1;
        for (int i = 0; i < 1024; i++) { double t = lo + (hi - lo) * i / 1023.0; r->lut[i] = sh_argb_slow(r, &t); }
    }
    return 1;
}
static u32 *sh_row(sh_rast *r, int y) { return r->px + (size_t)(r->h - 1 - (y - r->y0)) * r->w; }    /* linha 0 = topo */
static void sh_rast_end(sh_rast *r, void *ctx) {
    if (!r->px) return;
    void *prov = CGDataProviderCreateWithData(0, r->px, (size_t)r->w * r->h * 4, sh_free_cb);
    void *img = prov ? CGImageCreate((size_t)r->w, (size_t)r->h, 8, 32, (size_t)r->w * 4, 0, 2 | 0x2000 /* PremultipliedFirst, 32 little */, prov, 0, 0, 0) : 0;
    if (prov) CGDataProviderRelease(prov); else free(r->px);
    r->px = 0;
    if (!img) return;
    CGContextSaveGState(ctx);
    CGContextConcatCTM(ctx, tinv_(r->m));
    CGRect dst = {{(double)r->x0, (double)r->y0}, {(double)r->w, (double)r->h}};
    CGContextDrawImage(ctx, dst, img);
    CGContextRestoreGState(ctx);
    CGImageRelease(img);
}
/* triângulo de Gouraud no dispositivo: centros de pixel dentro (regra de cima/esquerda tolerante) */
static void sh_fill_tri(sh_rast *r, const sh_vtx *a, const sh_vtx *b, const sh_vtx *c) {
    const CGAffineTransform *m = &r->m;
    double ax = m->a * a->x + m->c * a->y + m->tx, ay = m->b * a->x + m->d * a->y + m->ty;
    double bx = m->a * b->x + m->c * b->y + m->tx, by = m->b * b->x + m->d * b->y + m->ty;
    double cx = m->a * c->x + m->c * c->y + m->tx, cy = m->b * c->x + m->d * c->y + m->ty;
    double area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    if (fabs(area) < 1e-12) return;
    double mnx = ax < bx ? (ax < cx ? ax : cx) : (bx < cx ? bx : cx), mxx = ax > bx ? (ax > cx ? ax : cx) : (bx > cx ? bx : cx);
    double mny = ay < by ? (ay < cy ? ay : cy) : (by < cy ? by : cy), mxy = ay > by ? (ay > cy ? ay : cy) : (by > cy ? by : cy);
    int px0 = (int)floor(mnx - 0.5), px1 = (int)ceil(mxx), py0 = (int)floor(mny - 0.5), py1 = (int)ceil(mxy);
    if (px0 < r->x0) px0 = r->x0;
    if (py0 < r->y0) py0 = r->y0;
    if (px1 > r->x0 + r->w - 1) px1 = r->x0 + r->w - 1;
    if (py1 > r->y0 + r->h - 1) py1 = r->y0 + r->h - 1;
    double eps = 1e-9 * fabs(area);
    for (int y = py0; y <= py1; y++) {
        u32 *row = sh_row(r, y);
        double Y = y + 0.5;
        for (int x = px0; x <= px1; x++) {
            double X = x + 0.5;
            double w0 = ((bx - X) * (cy - Y) - (by - Y) * (cx - X)) / area;
            double w1 = ((cx - X) * (ay - Y) - (cy - Y) * (ax - X)) / area;
            double w2 = 1 - w0 - w1;
            if (w0 < -eps || w1 < -eps || w2 < -eps) continue;
            double in[PF_MAXOUT];
            for (int i = 0; i < r->nc; i++) in[i] = w0 * a->c[i] + w1 * b->c[i] + w2 * c->c[i];
            row[x - r->x0] = sh_argb(r, in);
        }
    }
}
/* Coons (16 pontos com os 4 internos implícitos) e tensor: S(u,v) por Bézier bicúbica dos 16 pontos */
static void sh_patch_eval(const double P[4][4][2], double u, double v, double *x, double *y) {
    double bu[4] = {(1 - u) * (1 - u) * (1 - u), 3 * u * (1 - u) * (1 - u), 3 * u * u * (1 - u), u * u * u};
    double bv[4] = {(1 - v) * (1 - v) * (1 - v), 3 * v * (1 - v) * (1 - v), 3 * v * v * (1 - v), v * v * v};
    *x = *y = 0;
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) { *x += bu[i] * bv[j] * P[i][j][0]; *y += bu[i] * bv[j] * P[i][j][1]; }
}
/* patch dividido em N×N quadriláteros (2 triângulos cada), cores bilineares nos cantos (c00, c03, c33, c30) */
static void sh_fill_patch(sh_rast *r, const double P[4][4][2], const double cc[4][PF_MAXOUT]) {
    const CGAffineTransform *m = &r->m;
    double ext = 0;
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {
        double dx = m->a * (P[i][j][0] - P[0][0][0]) + m->c * (P[i][j][1] - P[0][0][1]), dy = m->b * (P[i][j][0] - P[0][0][0]) + m->d * (P[i][j][1] - P[0][0][1]);
        double e = fabs(dx) + fabs(dy);
        if (e > ext) ext = e;
    }
    int N = (int)(ext / 3) + 2;
    if (N > 64) N = 64;
    sh_vtx *g = malloc((size_t)(N + 1) * (N + 1) * sizeof *g);
    if (!g) return;
    for (int a = 0; a <= N; a++)
        for (int b = 0; b <= N; b++) {
            double u = (double)a / N, v = (double)b / N;
            sh_vtx *q = &g[a * (N + 1) + b];
            sh_patch_eval(P, u, v, &q->x, &q->y);
            for (int i = 0; i < r->nc; i++)
                q->c[i] = (1 - u) * (1 - v) * cc[0][i] + (1 - u) * v * cc[1][i] + u * v * cc[2][i] + u * (1 - v) * cc[3][i];
        }
    for (int a = 0; a < N; a++)
        for (int b = 0; b < N; b++) {
            const sh_vtx *p00 = &g[a * (N + 1) + b], *p01 = &g[a * (N + 1) + b + 1], *p10 = &g[(a + 1) * (N + 1) + b], *p11 = &g[(a + 1) * (N + 1) + b + 1];
            sh_fill_tri(r, p00, p10, p11);
            sh_fill_tri(r, p00, p11, p01);
        }
    free(g);
}

/* pinta o shading no espaço de usuário atual (o chamador já recortou e pôs a matriz) */
static void sh_paint(void *ctx, pdf_doc *pd, pdf_obj *sh, pdf_obj *res) {
    sh = pdf_resolve(pd, sh);
    double type = 0;
    if (!sh || !pdf_num(pdf_dict_get(pd, sh, "ShadingType"), &type)) return;
    pcs *cs = pcs_resolve(pd, pdf_dict_get_raw(sh, "ColorSpace"), res, 0);
    if (!cs) return;
    CGContextSaveGState(ctx);
    double bb[4];
    if (pf_nums(pd, pdf_dict_get(pd, sh, "BBox"), bb, 4) == 4) {
        CGRect r = {{bb[0] < bb[2] ? bb[0] : bb[2], bb[1] < bb[3] ? bb[1] : bb[3]}, {fabs(bb[2] - bb[0]), fabs(bb[3] - bb[1])}};
        CGContextBeginPath(ctx);
        CGContextAddRect(ctx, r);
        CGContextClip(ctx);
    }
    pf_set fn = {{0}, 0};
    pfs_load(pd, pdf_dict_get_raw(sh, "Function"), &fn);
    int t = (int)type;
    if (t == 2 || t == 3) {
        double co[6] = {0}, dom[2] = {0, 1};
        pf_nums(pd, pdf_dict_get(pd, sh, "Coords"), co, 6);
        pf_nums(pd, pdf_dict_get(pd, sh, "Domain"), dom, 2);
        pdf_obj *ex = pdf_dict_get(pd, sh, "Extend");
        int e0 = 0, e1 = 0;
        pdf_obj *x0 = pdf_array_get(pd, ex, 0), *x1 = pdf_array_get(pd, ex, 1);
        if (x0 && x0->type == PO_BOOL) e0 = x0->v.b;
        if (x1 && x1->type == PO_BOOL) e1 = x1->v.b;
        double o[PF_MAXOUT] = {0}, in = dom[0];
        pcs *target = pcs_cgcomps(cs, (double[PF_MAXOUT]){0}, o);
        (void)in;
        if (target && target->cg && fn.nf) {
            sh_ctx sc = {fn, cs, target, target->n};
            pcgfn_cb cb = {0, sh_eval, 0};
            double range[2 * PF_MAXOUT];
            for (int i = 0; i < target->n; i++) { range[2 * i] = target->lab && i ? -128 : 0; range[2 * i + 1] = target->lab ? (i ? 127 : 100) : 1; }
            void *f = CGFunctionCreate(&sc, 1, dom, (size_t)target->n, range, &cb);
            CGPoint p0 = {co[0], co[1]}, p1 = {t == 2 ? co[2] : co[3], t == 2 ? co[3] : co[4]};
            void *s = t == 2 ? CGShadingCreateAxial(target->cg, p0, p1, f, e0, e1) : CGShadingCreateRadial(target->cg, p0, co[2], p1, co[5], f, e0, e1);
            if (s) { CGContextDrawShading(ctx, s); CGShadingRelease(s); }
            if (f) CGFunctionRelease(f);
        }
    } else if (t == 1) {
        double dom[4] = {0, 1, 0, 1}, mt[6] = {1, 0, 0, 1, 0, 0};
        pf_nums(pd, pdf_dict_get(pd, sh, "Domain"), dom, 4);
        pf_nums(pd, pdf_dict_get(pd, sh, "Matrix"), mt, 6);
        CGAffineTransform M = {mt[0], mt[1], mt[2], mt[3], mt[4], mt[5]};
        CGContextConcatCTM(ctx, M);
        sh_rast R;
        if (sh_rast_begin(&R, ctx, cs, &fn, 2, 0, 0)) {
            CGAffineTransform inv = tinv_(R.m);
            for (int y = 0; y < R.h; y++) {
                u32 *row = sh_row(&R, R.y0 + y);
                for (int x = 0; x < R.w; x++) {
                    double X = R.x0 + x + 0.5, Y = R.y0 + y + 0.5;
                    double xy[2] = {inv.a * X + inv.c * Y + inv.tx, inv.b * X + inv.d * Y + inv.ty};
                    if (xy[0] < dom[0] || xy[0] > dom[1] || xy[1] < dom[2] || xy[1] > dom[3]) continue;
                    row[x] = sh_argb(&R, xy);
                }
            }
            sh_rast_end(&R, ctx);
        }
    } else if (t >= 4 && t <= 7 && sh->type == PO_STREAM) {
        double bpcoord = 8, bpcomp = 8, bpflag = 8, vpr = 2, dec[4 + 2 * PF_MAXOUT];
        pdf_num(pdf_dict_get(pd, sh, "BitsPerCoordinate"), &bpcoord);
        pdf_num(pdf_dict_get(pd, sh, "BitsPerComponent"), &bpcomp);
        pdf_num(pdf_dict_get(pd, sh, "BitsPerFlag"), &bpflag);
        pdf_num(pdf_dict_get(pd, sh, "VerticesPerRow"), &vpr);
        int nd = pf_nums(pd, pdf_dict_get(pd, sh, "Decode"), dec, 4 + 2 * PF_MAXOUT);
        int nc = fn.nf ? 1 : cs->n;
        if (nd < 4 + 2 * nc) goto done;
        size_t len;
        const u8 *d = pdf_stream_data(pd, sh, &len, 0);
        if (!d) goto done;
        sh_bits b = {d, len, 0};
        sh_rast R;
        if (!sh_rast_begin(&R, ctx, cs, &fn, nc, dec[4], dec[5])) goto done;
#define RD_V(v) do { (v).x = sh_read(&b, (int)bpcoord, dec[0], dec[1]); (v).y = sh_read(&b, (int)bpcoord, dec[2], dec[3]); \
                     for (int q = 0; q < nc; q++) (v).c[q] = sh_read(&b, (int)bpcomp, dec[4 + 2 * q], dec[5 + 2 * q]); } while (0)
        if (t == 4) {
            /* flag 0: vértice de um triângulo novo (3 seguidos); 1: (b, c, novo); 2: (a, c, novo) */
            sh_vtx tri[3], pend[3];
            int np = 0, have = 0;
            while (b.bit / 8 < len) {
                int flag = (int)sh_read(&b, (int)bpflag, 0, (double)((1u << (int)bpflag) - 1));
                sh_vtx nv;
                RD_V(nv);
                sh_align(&b);
                if (b.bit / 8 > len) break;
                if (flag == 0 || !have) {
                    pend[np++] = nv;
                    if (np == 3) { memcpy(tri, pend, sizeof tri); sh_fill_tri(&R, &tri[0], &tri[1], &tri[2]); have = 1; np = 0; }
                    continue;
                }
                if (flag == 1) { tri[0] = tri[1]; tri[1] = tri[2]; }
                else tri[1] = tri[2];
                tri[2] = nv;
                sh_fill_tri(&R, &tri[0], &tri[1], &tri[2]);
            }
        } else if (t == 5) {
            int W = (int)vpr;
            if (W < 2) goto done;
            sh_vtx *prev = malloc((size_t)W * sizeof *prev), *row = malloc((size_t)W * sizeof *row);
            int first = 1;
            while (prev && row && b.bit / 8 < len) {
                for (int i = 0; i < W; i++) RD_V(row[i]);
                if (b.bit / 8 > len) break;
                if (!first)
                    for (int i = 0; i + 1 < W; i++) {
                        sh_fill_tri(&R, &prev[i], &prev[i + 1], &row[i]);
                        sh_fill_tri(&R, &prev[i + 1], &row[i + 1], &row[i]);
                    }
                sh_vtx *tmp = prev; prev = row; row = tmp;
                first = 0;
            }
            free(prev);
            free(row);
        } else {
            /* 6 (Coons, 12 pontos) e 7 (tensor, 16): pontos na ordem da norma; arestas compartilhadas pela flag */
            double P[4][4][2], C[4][PF_MAXOUT];
            int havep = 0;
            static const int ord12[12][2] = {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 3}, {2, 3}, {3, 3}, {3, 2}, {3, 1}, {3, 0}, {2, 0}, {1, 0}};
            static const int ord16[16][2] = {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 3}, {2, 3}, {3, 3}, {3, 2}, {3, 1}, {3, 0}, {2, 0}, {1, 0},
                                             {1, 1}, {1, 2}, {2, 2}, {2, 1}};
            while (b.bit / 8 < len) {
                int flag = (int)sh_read(&b, (int)bpflag, 0, (double)((1u << (int)bpflag) - 1));
                int np = t == 6 ? 12 : 16, nn = flag ? np - 4 : np, ncol = flag ? 2 : 4;
                double pts[16][2], cols[4][PF_MAXOUT];
                for (int i = 0; i < nn; i++) { pts[i][0] = sh_read(&b, (int)bpcoord, dec[0], dec[1]); pts[i][1] = sh_read(&b, (int)bpcoord, dec[2], dec[3]); }
                for (int i = 0; i < ncol; i++) for (int q = 0; q < nc; q++) cols[i][q] = sh_read(&b, (int)bpcomp, dec[4 + 2 * q], dec[5 + 2 * q]);
                sh_align(&b);
                if (b.bit / 8 > len) break;
                double NP[4][4][2], NC[4][PF_MAXOUT];
                int off = 0;
                if (flag && havep) {
                    /* aresta compartilhada do patch anterior: flag 1 = lado D (03..33), 2 = lado C (33..30), 3 = lado B (30..00) */
                    int ia[4][2];
                    if (flag == 1) { int t1[4][2] = {{0, 3}, {1, 3}, {2, 3}, {3, 3}}; memcpy(ia, t1, sizeof ia); }
                    else if (flag == 2) { int t1[4][2] = {{3, 3}, {3, 2}, {3, 1}, {3, 0}}; memcpy(ia, t1, sizeof ia); }
                    else { int t1[4][2] = {{3, 0}, {2, 0}, {1, 0}, {0, 0}}; memcpy(ia, t1, sizeof ia); }
                    for (int i = 0; i < 4; i++) { NP[ord12[i][0]][ord12[i][1]][0] = P[ia[i][0]][ia[i][1]][0]; NP[ord12[i][0]][ord12[i][1]][1] = P[ia[i][0]][ia[i][1]][1]; }
                    int ci = flag == 1 ? 1 : flag == 2 ? 2 : 3, cj = flag == 1 ? 2 : flag == 2 ? 3 : 0;
                    for (int q = 0; q < nc; q++) { NC[0][q] = C[ci][q]; NC[1][q] = C[cj][q]; }
                    off = 4;
                    for (int i = 0; i < nn; i++) { NP[ord16[off + i][0]][ord16[off + i][1]][0] = pts[i][0]; NP[ord16[off + i][0]][ord16[off + i][1]][1] = pts[i][1]; }
                    for (int q = 0; q < nc; q++) { NC[2][q] = cols[0][q]; NC[3][q] = cols[1][q]; }
                } else {
                    if (flag) continue;
                    for (int i = 0; i < nn; i++) { NP[ord16[i][0]][ord16[i][1]][0] = pts[i][0]; NP[ord16[i][0]][ord16[i][1]][1] = pts[i][1]; }
                    for (int i = 0; i < 4; i++) for (int q = 0; q < nc; q++) NC[i][q] = cols[i][q];
                }
                if (t == 6) {
                    /* pontos internos de Coons (fórmula da norma, eq. 8.7.4.5.7) */
                    for (int k = 0; k < 2; k++) {
                        double p00 = NP[0][0][k], p03 = NP[0][3][k], p30 = NP[3][0][k], p33 = NP[3][3][k];
                        NP[1][1][k] = (-4 * p00 + 6 * (NP[0][1][k] + NP[1][0][k]) - 2 * (NP[0][3][k] + NP[3][0][k]) + 3 * (NP[3][1][k] + NP[1][3][k]) - p33) / 9;
                        NP[1][2][k] = (-4 * p03 + 6 * (NP[0][2][k] + NP[1][3][k]) - 2 * (NP[0][0][k] + NP[3][3][k]) + 3 * (NP[3][2][k] + NP[1][0][k]) - p30) / 9;
                        NP[2][1][k] = (-4 * p30 + 6 * (NP[3][1][k] + NP[2][0][k]) - 2 * (NP[3][3][k] + NP[0][0][k]) + 3 * (NP[0][1][k] + NP[2][3][k]) - p03) / 9;
                        NP[2][2][k] = (-4 * p33 + 6 * (NP[3][2][k] + NP[2][3][k]) - 2 * (NP[3][0][k] + NP[0][3][k]) + 3 * (NP[0][2][k] + NP[2][0][k]) - p00) / 9;
                    }
                }
                memcpy(P, NP, sizeof P);
                memcpy(C, NC, sizeof C);
                havep = 1;
                /* cores dos cantos: c00 (u0,v0), c03 (u0,v1), c33, c30 → ordem esperada por sh_fill_patch */
                sh_fill_patch(&R, P, (const double (*)[PF_MAXOUT])C);
            }
        }
#undef RD_V
        sh_rast_end(&R, ctx);
    }
done:
    CGContextRestoreGState(ctx);
    pfs_free(&fn);
    pcs_free(cs);
}
