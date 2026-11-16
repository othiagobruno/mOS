/* Booleanas de CGPath (cg_pathbool.c) no host: para cada caso, A e B rasterizados com a regra deles e a operação
 * feita pixel a pixel nas máscaras tem de bater com o raster do caminho resultante (fora dos pixels de borda).
 *   F="-g -O1 -fno-sanitize=all -ffreestanding -fno-builtin -w -Iuserland/libsystem/kshim -Iuserland/libsystem/fsinc -Ikernel/include -Iuserland/libsystem"
 *   for f in coregraphics cgpdf pdf_core cg_consumer cg_pathbool; do zig cc $F -c userland/libsystem/$f.c -o /tmp/mariana-pb/$f.o; done
 *   zig cc $F -DCT_HOST -c userland/libsystem/ct_outline.c -o /tmp/mariana-pb/ct_outline.o
 *   zig cc -std=gnu11 -O1 -w -D_DEFAULT_SOURCE -Iuserland/libsystem -Itools tools/cg_pathbool_host.c /tmp/mariana-pb/*.o \
 *     userland/libsystem/cg_raster.c userland/libsystem/cgimage_crop.c -DCS_HOST userland/libsystem/colorsync.c -lm -lz -o /tmp/mariana-pb/pbhost
 *   /tmp/mariana-pb/pbhost */
#define main cg_context_main
#include "cg_context_test.c"
#undef main
const char kCFTypeArrayCallBacks[64];
void *CGPathCreateMutable(void);
CGRect CGPathGetBoundingBox(const void *p);
void CGPathAddEllipseInRect(void *p, const void *m, CGRect r);
void CGPathMoveToPoint(void *p, const void *m, double x, double y);
void CGPathAddLineToPoint(void *p, const void *m, double x, double y);
void CGPathCloseSubpath(void *p);
void CGPathRelease(void *p);
void *CGPathCreateCopyByUnioningPath(const void *a, const void *b, int eo);
void *CGPathCreateCopyByIntersectingPath(const void *a, const void *b, int eo);
void *CGPathCreateCopyBySubtractingPath(const void *a, const void *b, int eo);
void *CGPathCreateCopyBySymmetricDifferenceOfPath(const void *a, const void *b, int eo);
void *CGPathCreateCopyByNormalizing(const void *p, int eo);
void *CGPathCreateCopyByFlattening(const void *p, double t);
void *CGPathCreateCopyOfLineByIntersectingPath(const void *l, const void *r, int eo);
void *CGPathCreateCopyOfLineBySubtractingPath(const void *l, const void *r, int eo);
int CGPathIntersectsPath(const void *a, const void *b, int eo);
int CGPathIsEmpty(const void *p);
void *CGPathCreateSeparateComponents(const void *p, int eo);
int CGPathContainsPoint(const void *p, const void *m, CGPoint pt, int eo);
int CGPathIsPrimitive(const void *p);
int CGPathIsRectWithTransform(const void *p, CGRect *r, CGAffineTransform *t);
int CGPathIsEllipseWithTransform(const void *p, CGRect *r, CGAffineTransform *t);
int CGPathIsRoundedRectWithTransform(const void *p, CGRect *r, double *cw, double *ch, CGAffineTransform *t);
void *CGPathCreateWithRect(CGRect r, const CGAffineTransform *m);
void *CGPathCreateWithEllipseInRect(CGRect r, const CGAffineTransform *m);
void *CGPathCreateWithRoundedRect(CGRect r, double cw, double ch, const CGAffineTransform *m);
void *CGPathCreateMutableCopy(const void *p);
void *CGPathCreateCopyByTransformingPath(const void *p, const CGAffineTransform *m);
static int req(CGRect a, double x, double y, double w, double h) { return a.origin.x == x && a.origin.y == y && a.size.width == w && a.size.height == h; }
static int teq(CGAffineTransform t, double a, double b, double c, double d, double tx, double ty) {
    return fabs(t.a - a) < 1e-9 && fabs(t.b - b) < 1e-9 && fabs(t.c - c) < 1e-9 && fabs(t.d - d) < 1e-9 && fabs(t.tx - tx) < 1e-9 && fabs(t.ty - ty) < 1e-9;
}
#define W 200
static uint8_t *mask(const void *p, int eo) {
    void *c = CGBitmapContextCreate(0, W, W, 8, W * 4, 0, 1);
    cx_bm_set_fill(c, 0xFF000000u);
    cx_bm_begin(c);
    CGContextAddPath(c, p);
    cx_bm_fill(c, eo);
    uint8_t *m = malloc(W * W), *d = CGBitmapContextGetData(c);
    for (int i = 0; i < W * W; i++) m[i] = d[i * 4 + 3];
    CGContextRelease(c);
    return m;
}
static int fails;
#define CHECK(c, msg) do { int ok_ = (c); printf("%s %s\n", ok_ ? "ok  " : "FAIL", msg); if (!ok_) fails++; } while (0)
/* fração de pixels "certos" (A e B longe da borda) em que o resultado discorda da operação */
static double mismatch(const void *a, const void *b, int eo, const void *res, int op) {
    uint8_t *ma = mask(a, eo), *mb = mask(b, eo), *mr = mask(res, 0);
    long bad = 0, tot = 0;
    for (int i = 0; i < W * W; i++) {
        int ca = ma[i] > 230 ? 1 : ma[i] < 25 ? 0 : -1, cb = mb[i] > 230 ? 1 : mb[i] < 25 ? 0 : -1;
        if (ca < 0 || cb < 0) continue;
        int want = op == 0 ? (ca || cb) : op == 1 ? (ca && cb) : op == 2 ? (ca && !cb) : (ca != cb);
        int got = mr[i] > 127;
        if (mr[i] > 25 && mr[i] < 230) continue;          /* borda do resultado */
        tot++;
        if (got != want) bad++;
    }
    free(ma); free(mb); free(mr);
    return tot ? (double)bad / tot : 1;
}
int main(void) {
    void *circ = CGPathCreateMutable(), *rect = CGPathCreateMutable(), *star = CGPathCreateMutable(), *donut = CGPathCreateMutable(), *far = CGPathCreateMutable();
    CGPathAddEllipseInRect(circ, 0, (CGRect){{20, 20}, {110, 110}});
    CGPathAddRect(rect, NULL, (CGRect){{70, 60}, {110, 90}});
    double cx = 100, cy = 100;
    for (int k = 0; k < 5; k++) {                     /* estrela de 5 pontas auto-intersectante */
        double a = -1.5707963 + k * 4 * 3.14159265 / 5;
        if (!k) CGPathMoveToPoint(star, 0, cx + 80 * cos(a), cy + 80 * sin(a)); else CGPathAddLineToPoint(star, 0, cx + 80 * cos(a), cy + 80 * sin(a));
    }
    CGPathCloseSubpath(star);
    CGPathAddEllipseInRect(donut, 0, (CGRect){{30, 30}, {140, 140}});
    CGPathAddEllipseInRect(donut, 0, (CGRect){{70, 70}, {60, 60}});
    CGPathAddRect(far, NULL, (CGRect){{185, 185}, {10, 10}});
    static const char *opn[4] = {"união", "interseção", "subtração", "diferença simétrica"};
    void *(*fn[4])(const void *, const void *, int) = {CGPathCreateCopyByUnioningPath, CGPathCreateCopyByIntersectingPath, CGPathCreateCopyBySubtractingPath, CGPathCreateCopyBySymmetricDifferenceOfPath};
    for (int op = 0; op < 4; op++) {
        char m[128];
        void *r = fn[op](circ, rect, 0);
        double e = mismatch(circ, rect, 0, r, op);
        snprintf(m, sizeof m, "círculo %s retângulo (discordância %.3f%%)", opn[op], e * 100);
        CHECK(e < 0.003, m);
        CGPathRelease(r);
        r = fn[op](star, donut, 1);
        e = mismatch(star, donut, 1, r, op);
        snprintf(m, sizeof m, "estrela %s rosca, par-ímpar (%.3f%%)", opn[op], e * 100);
        CHECK(e < 0.003, m);
        CGPathRelease(r);
        r = fn[op](star, donut, 0);
        e = mismatch(star, donut, 0, r, op);
        snprintf(m, sizeof m, "estrela %s rosca, não nula (%.3f%%)", opn[op], e * 100);
        CHECK(e < 0.003, m);
        CGPathRelease(r);
    }
    /* quadrados sobrepostos (pedido do SwiftUI): A = 2 quadrados no mesmo sentido que se cruzam (o miolo some no par-ímpar),
     * B = quadrado que corta os dois e tem uma borda coincidente com A; cada operação nas duas regras + pontos de controle */
    void *sqA = CGPathCreateMutable(), *sqB = CGPathCreateMutable();
    CGPathAddRect(sqA, NULL, (CGRect){{20, 20}, {80, 80}});
    CGPathAddRect(sqA, NULL, (CGRect){{60, 60}, {80, 80}});
    CGPathAddRect(sqB, NULL, (CGRect){{100, 30}, {80, 60}});      /* x=100 coincide com a borda direita do 1º quadrado */
    for (int eo = 0; eo < 2; eo++)
        for (int op = 0; op < 4; op++) {
            char m[160];
            void *r = fn[op](sqA, sqB, eo);
            double e = mismatch(sqA, sqB, eo, r, op);
            /* (80,80): miolo de A (dentro só na não nula); (120,70): só B+2º quadrado; (150,40): só B; (40,40): só A */
            CGPoint pts[4] = {{80, 80}, {120, 70}, {150, 40}, {40, 40}};
            int inA[4] = {!eo, 1, 0, 1}, inB[4] = {0, 1, 1, 0}, okp = 1;
            for (int k = 0; k < 4; k++) {
                int want = op == 0 ? (inA[k] || inB[k]) : op == 1 ? (inA[k] && inB[k]) : op == 2 ? (inA[k] && !inB[k]) : (inA[k] != inB[k]);
                if ((CGPathContainsPoint(r, 0, pts[k], 0) != 0) != want) okp = 0;
            }
            snprintf(m, sizeof m, "quadrados sobrepostos %s, %s (%.3f%%, pontos %s)", opn[op], eo ? "par-ímpar" : "não nula", e * 100, okp ? "ok" : "errados");
            CHECK(e < 0.003 && okp, m);
            CGPathRelease(r);
        }
    void *sq1 = CGPathCreateMutable(), *sq2 = CGPathCreateMutable();
    CGPathAddRect(sq1, NULL, (CGRect){{0, 0}, {100, 100}});
    CGPathAddRect(sq2, NULL, (CGRect){{50, 50}, {100, 100}});
    void *u = CGPathCreateCopyByUnioningPath(sq1, sq2, 0), *in = CGPathCreateCopyByIntersectingPath(sq1, sq2, 1);
    CGRect bu = CGPathGetBoundingBox(u), bi = CGPathGetBoundingBox(in);
    CHECK(bu.origin.x == 0 && bu.size.width == 150 && bi.origin.x == 50 && bi.origin.y == 50 && bi.size.width == 50 && bi.size.height == 50, "quadrados: caixa da união 0..150 e da interseção 50..100 exatas");
    /* SPI de forma primitiva (OpenRenderBox) */
    {
        CGAffineTransform rot = {0, 1, -1, 0, 5, 7}, tr = {1, 0, 0, 1, 10, 20}, t;
        CGRect r; double cw = -1, ch = -1;
        void *pr = CGPathCreateWithRect((CGRect){{30, 40}, {-20, 10}}, &rot);
        CHECK(CGPathIsPrimitive(pr) && CGPathIsRectWithTransform(pr, &r, &t) && req(r, 10, 40, 20, 10) && teq(t, 0, 1, -1, 0, 5, 7)
              && !CGPathIsEllipseWithTransform(pr, &r, &t) && CGPathIsRoundedRectWithTransform(pr, &r, &cw, &ch, &t) && cw == 0 && ch == 0,
              "retângulo com rotação: primitivo, retângulo padronizado + transformação; vale como arredondado de raio 0");
        void *pe = CGPathCreateWithEllipseInRect((CGRect){{0, 0}, {40, 20}}, 0);
        CHECK(CGPathIsPrimitive(pe) && CGPathIsEllipseWithTransform(pe, &r, &t) && req(r, 0, 0, 40, 20) && teq(t, 1, 0, 0, 1, 0, 0)
              && !CGPathIsRectWithTransform(pe, &r, &t) && !CGPathIsRoundedRectWithTransform(pe, &r, &cw, &ch, &t), "elipse: só elipse, transformação identidade");
        void *pq = CGPathCreateWithRoundedRect((CGRect){{0, 0}, {100, 30}}, 8, 40, &tr);
        CHECK(CGPathIsRoundedRectWithTransform(pq, &r, &cw, &ch, &t) && req(r, 0, 0, 100, 30) && cw == 8 && ch == 15 && teq(t, 1, 0, 0, 1, 10, 20)
              && !CGPathIsRectWithTransform(pq, &r, &t), "retângulo arredondado: raios (altura limitada à metade) e transformação");
        void *p0 = CGPathCreateWithRoundedRect((CGRect){{0, 0}, {50, 50}}, 0, 0, 0);
        CHECK(CGPathIsRectWithTransform(p0, &r, &t) && req(r, 0, 0, 50, 50), "arredondado de raio 0 vira retângulo");
        void *cp = CGPathCreateCopyByTransformingPath(pq, &tr);
        CHECK(CGPathIsRoundedRectWithTransform(cp, &r, &cw, &ch, &t) && cw == 8 && teq(t, 1, 0, 0, 1, 20, 40), "cópia transformada continua primitiva, com as transformações compostas");
        void *mu = CGPathCreateMutableCopy(pe);
        CGPathAddLineToPoint(mu, 0, 1, 1);
        void *man = CGPathCreateMutable();
        CGPathAddRect(man, NULL, (CGRect){{1, 2}, {3, 4}});
        CHECK(!CGPathIsPrimitive(mu) && !CGPathIsEllipseWithTransform(mu, &r, &t) && !CGPathIsPrimitive(man)
              && CGPathIsRectWithTransform(man, &r, &t) && req(r, 1, 2, 3, 4) && teq(t, 1, 0, 0, 1, 0, 0),
              "caminho alterado perde a marca; retângulo montado à mão não é primitivo, mas IsRectWithTransform reconhece");
    }
    void *empty = CGPathCreateMutable();
    void *nrm = CGPathCreateCopyByNormalizing(star, 1);
    CHECK(mismatch(star, empty, 1, nrm, 0) < 0.003 && !CGPathContainsPoint(nrm, 0, (CGPoint){100, 100}, 0), "normalizar a estrela par-ímpar: o centro fica de fora mesmo na regra não nula");
    CHECK(CGPathIntersectsPath(circ, rect, 0) && !CGPathIntersectsPath(circ, far, 0) && CGPathIntersectsPath(donut, CGPathCreateCopyByNormalizing(circ, 0), 1), "CGPathIntersectsPath");
    void *comps = CGPathCreateSeparateComponents(donut, 1);
    void *two = CGPathCreateMutable();
    CGPathAddRect(two, NULL, (CGRect){{10, 10}, {40, 40}});
    CGPathAddRect(two, NULL, (CGRect){{100, 100}, {40, 40}});
    CGPathAddRect(two, NULL, (CGRect){{110, 110}, {10, 10}});
    void *comps2 = CGPathCreateSeparateComponents(two, 1);
    CHECK(CFArrayGetCount(comps) == 1 && CFArrayGetCount(comps2) == 2, "componentes: rosca = 1 (com o buraco), dois quadrados = 2");
    void *flat = CGPathCreateCopyByFlattening(circ, 0.5);
    CHECK(mismatch(circ, empty, 0, flat, 0) < 0.003, "achatamento do círculo mantém a área");
    void *ln = CGPathCreateMutable();
    CGPathMoveToPoint(ln, 0, 0, 75); CGPathAddLineToPoint(ln, 0, 200, 75);
    void *cut = CGPathCreateCopyOfLineByIntersectingPath(ln, circ, 0);
    CGRect bb = CGPathGetBoundingBox(cut);
    CHECK(bb.origin.x > 19 && bb.origin.x < 22 && bb.origin.x + bb.size.width > 128 && bb.origin.x + bb.size.width < 131, "linha recortada pelo círculo");
    void *rest = CGPathCreateCopyOfLineBySubtractingPath(ln, circ, 0);
    CGRect br = CGPathGetBoundingBox(rest);
    CHECK(br.origin.x == 0 && br.size.width == 200 && !CGPathContainsPoint(rest, 0, (CGPoint){75, 75}, 0) && CGPathIsEmpty(CGPathCreateCopyOfLineByIntersectingPath(rest, circ, 0)), "linha menos o círculo: sobram as duas pontas, nada dentro do círculo");
    printf("%d falha(s)\n", fails);
    return fails ? 1 : 0;
}
