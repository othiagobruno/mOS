/*
 * mOS CoreGraphics — operações booleanas de CGPath (macOS 14): união, interseção, subtração, diferença simétrica,
 * normalização, componentes separados, interseção/subtração de linha, achatamento e teste de interseção.
 *
 * Método (classificação de arestas): os dois caminhos são achatados em polígonos (curvas com tolerância de 0,02),
 * todas as arestas são cortadas nas interseções entre si, e cada pedaço é classificado pelos pontos logo à esquerda e
 * à direita do meio dele (dentro de A? dentro de B? com a regra de preenchimento de cada um). Fica o pedaço em que o
 * resultado muda de um lado para o outro, orientado com o interior à esquerda; os pedaços são encadeados em contornos
 * fechados (externos no sentido anti-horário, buracos no horário: servem às duas regras). O resultado sai em segmentos
 * de reta; `CGPathCreateSeparateComponents` e `CGPathCreateCopyByFlattening` preservam/achatam as curvas originais.
 * Teste: tools/cg_pathbool_host.py (raster do resultado = operação nas máscaras de A e B).
 */
#include "foundation.h"
#include "libc.h"
#include <stdbool.h>

typedef double CGFloat;
typedef struct { CGFloat x, y; } CGPoint;
typedef struct { i32 type; CGPoint *points; } CGPathElement;
void CGPathApply(const void *path, void *info, void (*fn)(void *, const CGPathElement *));
void *CGPathCreateMutable(void);
void CGPathMoveToPoint(void *p, const void *m, CGFloat x, CGFloat y);
void CGPathAddLineToPoint(void *p, const void *m, CGFloat x, CGFloat y);
void CGPathAddQuadCurveToPoint(void *p, const void *m, CGFloat cx, CGFloat cy, CGFloat x, CGFloat y);
void CGPathAddCurveToPoint(void *p, const void *m, CGFloat a, CGFloat b, CGFloat c, CGFloat d, CGFloat x, CGFloat y);
void CGPathCloseSubpath(void *p);
id CFArrayCreate(void *alloc, const void **vals, long n, const void *cb);
extern const char kCFTypeArrayCallBacks[];
double sqrt(double), fabs(double), floor(double);
void CGPathRelease(void *p);

/* ---------------------------------------------------------------- polígonos */
typedef struct { CGPoint *p; int n, cap; int closed; } ring;
typedef struct { ring *r; int n, cap; } poly;
static void ring_add(ring *r, CGPoint q) {
    if (r->n && fabs(r->p[r->n - 1].x - q.x) < 1e-12 && fabs(r->p[r->n - 1].y - q.y) < 1e-12) return;
    if (r->n == r->cap) { int c = r->cap ? r->cap * 2 : 16; CGPoint *np = realloc(r->p, (size_t)c * sizeof *np); if (!np) return; r->p = np; r->cap = c; }
    r->p[r->n++] = q;
}
static ring *poly_new_ring(poly *P) {
    if (P->n == P->cap) { int c = P->cap ? P->cap * 2 : 8; ring *nr = realloc(P->r, (size_t)c * sizeof *nr); if (!nr) return 0; P->r = nr; P->cap = c; }
    ring *r = &P->r[P->n++];
    memset(r, 0, sizeof *r);
    return r;
}
static void poly_free(poly *P) { for (int i = 0; i < P->n; i++) free(P->r[i].p); free(P->r); P->r = 0; P->n = P->cap = 0; }
typedef struct { poly *P; ring *cur; CGPoint start, last; double tol; } flat_ctx;
static void flat_cubic(flat_ctx *c, CGPoint p0, CGPoint p1, CGPoint p2, CGPoint p3, int depth) {
    double d1 = fabs((p1.x - p3.x) * (p0.y - p3.y) - (p1.y - p3.y) * (p0.x - p3.x)), d2 = fabs((p2.x - p3.x) * (p0.y - p3.y) - (p2.y - p3.y) * (p0.x - p3.x));
    double l = sqrt((p3.x - p0.x) * (p3.x - p0.x) + (p3.y - p0.y) * (p3.y - p0.y));
    if (depth > 16 || (d1 + d2) <= c->tol * (l > 1e-9 ? l : 1e-9) * 4) { ring_add(c->cur, p3); return; }
    CGPoint a = {(p0.x + p1.x) / 2, (p0.y + p1.y) / 2}, b = {(p1.x + p2.x) / 2, (p1.y + p2.y) / 2}, cc = {(p2.x + p3.x) / 2, (p2.y + p3.y) / 2};
    CGPoint ab = {(a.x + b.x) / 2, (a.y + b.y) / 2}, bc = {(b.x + cc.x) / 2, (b.y + cc.y) / 2}, m = {(ab.x + bc.x) / 2, (ab.y + bc.y) / 2};
    flat_cubic(c, p0, a, ab, m, depth + 1);
    flat_cubic(c, m, bc, cc, p3, depth + 1);
}
static void flat_el(void *info, const CGPathElement *e) {
    flat_ctx *c = info;
    switch (e->type) {
    case 0: c->cur = poly_new_ring(c->P); if (c->cur) ring_add(c->cur, e->points[0]); c->start = c->last = e->points[0]; break;
    case 1: if (!c->cur) { c->cur = poly_new_ring(c->P); ring_add(c->cur, c->last); } ring_add(c->cur, e->points[0]); c->last = e->points[0]; break;
    case 2: {
        if (!c->cur) { c->cur = poly_new_ring(c->P); ring_add(c->cur, c->last); }
        CGPoint p0 = c->last, q = e->points[0], p3 = e->points[1];
        CGPoint p1 = {p0.x + 2.0 / 3 * (q.x - p0.x), p0.y + 2.0 / 3 * (q.y - p0.y)}, p2 = {p3.x + 2.0 / 3 * (q.x - p3.x), p3.y + 2.0 / 3 * (q.y - p3.y)};
        flat_cubic(c, p0, p1, p2, p3, 0);
        c->last = p3;
        break;
    }
    case 3:
        if (!c->cur) { c->cur = poly_new_ring(c->P); ring_add(c->cur, c->last); }
        flat_cubic(c, c->last, e->points[0], e->points[1], e->points[2], 0);
        c->last = e->points[2];
        break;
    case 4: if (c->cur) c->cur->closed = 1; c->last = c->start; c->cur = 0; break;
    }
}
static void flatten(const void *path, double tol, poly *P) {
    memset(P, 0, sizeof *P);
    flat_ctx c = {P, 0, {0, 0}, {0, 0}, tol};
    if (path) CGPathApply(path, &c, flat_el);
}
/* dentro do polígono (todas as voltas fechadas implicitamente): regra par-ímpar ou não nula */
static int inside(const poly *P, CGPoint q, int eo) {
    int wind = 0, cross = 0;
    for (int k = 0; k < P->n; k++) {
        const ring *r = &P->r[k];
        if (r->n < 3) continue;
        for (int i = 0; i < r->n; i++) {
            CGPoint a = r->p[i], b = r->p[(i + 1) % r->n];
            if ((a.y <= q.y) != (b.y <= q.y)) {
                double x = a.x + (q.y - a.y) * (b.x - a.x) / (b.y - a.y);
                if (x > q.x) { cross++; wind += b.y > a.y ? 1 : -1; }
            }
        }
    }
    return eo ? (cross & 1) : wind != 0;
}

/* ---------------------------------------------------------------- arestas cortadas nas interseções */
typedef struct { CGPoint a, b; int src; double *t; int nt, capt; } edge;
typedef struct { edge *e; int n, cap; } edges;
static void edges_from(edges *E, const poly *P, int src, int closed_regions) {
    for (int k = 0; k < P->n; k++) {
        const ring *r = &P->r[k];
        int m = r->n;
        if (m < 2) continue;
        int last = closed_regions || r->closed ? m : m - 1;
        for (int i = 0; i < last; i++) {
            CGPoint a = r->p[i], b = r->p[(i + 1) % m];
            if (fabs(a.x - b.x) < 1e-12 && fabs(a.y - b.y) < 1e-12) continue;
            if (E->n == E->cap) { int c = E->cap ? E->cap * 2 : 64; edge *ne = realloc(E->e, (size_t)c * sizeof *ne); if (!ne) return; E->e = ne; E->cap = c; }
            edge *e = &E->e[E->n++];
            memset(e, 0, sizeof *e);
            e->a = a; e->b = b; e->src = src;
        }
    }
}
static void add_t(edge *e, double t) {
    if (t <= 1e-9 || t >= 1 - 1e-9) return;
    if (e->nt == e->capt) { int c = e->capt ? e->capt * 2 : 4; double *nt = realloc(e->t, (size_t)c * sizeof *nt); if (!nt) return; e->t = nt; e->capt = c; }
    e->t[e->nt++] = t;
}
static void intersect(edge *p, edge *q) {
    double ax = p->a.x, ay = p->a.y, bx = p->b.x - ax, by = p->b.y - ay, cx = q->a.x, cy = q->a.y, dx = q->b.x - cx, dy = q->b.y - cy;
    double den = bx * dy - by * dx;
    double scale = fabs(bx) + fabs(by) + fabs(dx) + fabs(dy);
    if (fabs(den) <= 1e-12 * scale * scale) {
        /* paralelas: se colineares, cada ponta de uma corta a outra */
        double cr = (cx - ax) * by - (cy - ay) * bx;
        if (fabs(cr) > 1e-9 * scale * scale) return;
        double bb = bx * bx + by * by, dd = dx * dx + dy * dy;
        if (bb > 0) { add_t(p, ((cx - ax) * bx + (cy - ay) * by) / bb); add_t(p, ((q->b.x - ax) * bx + (q->b.y - ay) * by) / bb); }
        if (dd > 0) { add_t(q, ((ax - cx) * dx + (ay - cy) * dy) / dd); add_t(q, ((p->b.x - cx) * dx + (p->b.y - cy) * dy) / dd); }
        return;
    }
    double t = ((cx - ax) * dy - (cy - ay) * dx) / den, u = ((cx - ax) * by - (cy - ay) * bx) / den;
    if (t < -1e-9 || t > 1 + 1e-9 || u < -1e-9 || u > 1 + 1e-9) return;
    add_t(p, t);
    add_t(q, u);
}
static int cmp_minx(const void *a, const void *b) {
    const edge *x = a, *y = b;
    double ax = x->a.x < x->b.x ? x->a.x : x->b.x, bx = y->a.x < y->b.x ? y->a.x : y->b.x;
    return ax < bx ? -1 : ax > bx;
}
static void split_all(edges *E) {
    qsort(E->e, (size_t)E->n, sizeof *E->e, cmp_minx);
    for (int i = 0; i < E->n; i++) {
        edge *p = &E->e[i];
        double pmaxx = p->a.x > p->b.x ? p->a.x : p->b.x, pminy = p->a.y < p->b.y ? p->a.y : p->b.y, pmaxy = p->a.y > p->b.y ? p->a.y : p->b.y;
        for (int j = i + 1; j < E->n; j++) {
            edge *q = &E->e[j];
            double qminx = q->a.x < q->b.x ? q->a.x : q->b.x;
            if (qminx > pmaxx + 1e-9) break;
            double qminy = q->a.y < q->b.y ? q->a.y : q->b.y, qmaxy = q->a.y > q->b.y ? q->a.y : q->b.y;
            if (qminy > pmaxy + 1e-9 || qmaxy < pminy - 1e-9) continue;
            intersect(p, q);
        }
    }
}
static int cmp_d(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

/* ---------------------------------------------------------------- pedaços, classificação e encadeamento */
typedef struct { CGPoint a, b; int used; } piece;
typedef struct { piece *p; int n, cap; } pieces;
static void piece_add(pieces *S, CGPoint a, CGPoint b) {
    if (S->n == S->cap) { int c = S->cap ? S->cap * 2 : 64; piece *np = realloc(S->p, (size_t)c * sizeof *np); if (!np) return; S->p = np; S->cap = c; }
    S->p[S->n].a = a; S->p[S->n].b = b; S->p[S->n].used = 0;
    S->n++;
}
enum { OP_UNION, OP_INTERSECT, OP_SUBTRACT, OP_XOR };
static int op_in(int op, int a, int b) {
    switch (op) {
    case OP_UNION: return a || b;
    case OP_INTERSECT: return a && b;
    case OP_SUBTRACT: return a && !b;
    default: return a != b;
    }
}
static double bbox_size(const poly *A, const poly *B) {
    double x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
    const poly *L[2] = {A, B};
    for (int k = 0; k < 2; k++)
        for (int i = 0; L[k] && i < L[k]->n; i++)
            for (int j = 0; j < L[k]->r[i].n; j++) {
                CGPoint q = L[k]->r[i].p[j];
                if (q.x < x0) x0 = q.x;
                if (q.y < y0) y0 = q.y;
                if (q.x > x1) x1 = q.x;
                if (q.y > y1) y1 = q.y;
            }
    double s = (x1 - x0) + (y1 - y0);
    return s > 0 ? s : 1;
}
static int same(CGPoint a, CGPoint b, double eps) { return fabs(a.x - b.x) <= eps && fabs(a.y - b.y) <= eps; }
/* pedaços orientados → contornos fechados no caminho de saída */
static void *chain(pieces *S, double eps, int close) {
    void *out = CGPathCreateMutable();
    for (int i = 0; i < S->n; i++) {
        if (S->p[i].used) continue;
        S->p[i].used = 1;
        CGPoint start = S->p[i].a, cur = S->p[i].b;
        CGPathMoveToPoint(out, 0, start.x, start.y);
        CGPathAddLineToPoint(out, 0, cur.x, cur.y);
        for (int guard = 0; guard < S->n; guard++) {
            if (close && same(cur, start, eps)) break;
            int nx = -1;
            for (int j = 0; j < S->n; j++) if (!S->p[j].used && same(S->p[j].a, cur, eps)) { nx = j; break; }
            if (nx < 0 && !close) for (int j = 0; j < S->n; j++) if (!S->p[j].used && same(S->p[j].b, cur, eps)) { nx = j; CGPoint t = S->p[j].a; S->p[j].a = S->p[j].b; S->p[j].b = t; break; }
            if (nx < 0) break;
            S->p[nx].used = 1;
            cur = S->p[nx].b;
            CGPathAddLineToPoint(out, 0, cur.x, cur.y);
        }
        if (close) CGPathCloseSubpath(out);
    }
    return out;
}
/* A op B; line: A é uma linha (só os pedaços de A, mantidos pelo lado de B) */
static void *boolean(const void *a, const void *b, int eo, int op, int line) {
    poly A, B;
    flatten(a, 0.02, &A);
    flatten(b, 0.02, &B);
    edges E = {0};
    edges_from(&E, &A, 0, !line);
    edges_from(&E, &B, 1, 1);
    split_all(&E);
    double size = bbox_size(&A, &B), eps = size * 1e-7, off = size * 1e-6;
    pieces S = {0};
    for (int i = 0; i < E.n; i++) {
        edge *e = &E.e[i];
        if (line && e->src != 0) continue;
        qsort(e->t, (size_t)e->nt, sizeof *e->t, cmp_d);
        double prev = 0;
        for (int k = 0; k <= e->nt; k++) {
            double t = k < e->nt ? e->t[k] : 1;
            if (t - prev < 1e-12) continue;
            CGPoint p0 = {e->a.x + (e->b.x - e->a.x) * prev, e->a.y + (e->b.y - e->a.y) * prev};
            CGPoint p1 = {e->a.x + (e->b.x - e->a.x) * t, e->a.y + (e->b.y - e->a.y) * t};
            prev = t;
            CGPoint m = {(p0.x + p1.x) / 2, (p0.y + p1.y) / 2};
            if (line) {
                int in_b = inside(&B, m, eo);
                if ((op == OP_INTERSECT) == in_b) piece_add(&S, p0, p1);
                continue;
            }
            double dx = p1.x - p0.x, dy = p1.y - p0.y, l = sqrt(dx * dx + dy * dy);
            if (l <= 0) continue;
            CGPoint left = {m.x - dy / l * off, m.y + dx / l * off}, right = {m.x + dy / l * off, m.y - dx / l * off};
            int rl = op_in(op, inside(&A, left, eo), inside(&B, left, eo)), rr = op_in(op, inside(&A, right, eo), inside(&B, right, eo));
            if (rl == rr) continue;
            /* a mesma aresta vinda dos dois caminhos entra uma vez */
            int dup = 0;
            for (int j = 0; j < S.n && !dup; j++)
                if ((same(S.p[j].a, p0, eps) && same(S.p[j].b, p1, eps)) || (same(S.p[j].a, p1, eps) && same(S.p[j].b, p0, eps))) dup = 1;
            if (dup) continue;
            if (rl) piece_add(&S, p0, p1); else piece_add(&S, p1, p0);
        }
    }
    void *out = chain(&S, eps * 10, !line);
    for (int i = 0; i < E.n; i++) free(E.e[i].t);
    free(E.e);
    free(S.p);
    poly_free(&A);
    poly_free(&B);
    return out;
}
void *CGPathCreateCopyByUnioningPath(const void *a, const void *b, bool eo) { return boolean(a, b, eo, OP_UNION, 0); }
void *CGPathCreateCopyByIntersectingPath(const void *a, const void *b, bool eo) { return boolean(a, b, eo, OP_INTERSECT, 0); }
void *CGPathCreateCopyBySubtractingPath(const void *a, const void *b, bool eo) { return boolean(a, b, eo, OP_SUBTRACT, 0); }
void *CGPathCreateCopyBySymmetricDifferenceOfPath(const void *a, const void *b, bool eo) { return boolean(a, b, eo, OP_XOR, 0); }
void *CGPathCreateCopyOfLineByIntersectingPath(const void *line, const void *region, bool eo) { return boolean(line, region, eo, OP_INTERSECT, 1); }
void *CGPathCreateCopyOfLineBySubtractingPath(const void *line, const void *region, bool eo) { return boolean(line, region, eo, OP_SUBTRACT, 1); }
/* normalizar: a borda da região preenchida, sem sobreposição nem auto-interseção */
void *CGPathCreateCopyByNormalizing(const void *path, bool eo) {
    void *empty = CGPathCreateMutable();
    void *r = boolean(path, empty, eo, OP_UNION, 0);
    CGPathRelease(empty);
    return r;
}
/* algum ponto em comum: arestas que se cruzam, ou um contido no outro */
bool CGPathIntersectsPath(const void *a, const void *b, bool eo) {
    poly A, B;
    flatten(a, 0.05, &A);
    flatten(b, 0.05, &B);
    edges E = {0};
    edges_from(&E, &A, 0, 1);
    edges_from(&E, &B, 1, 1);
    int hit = 0;
    for (int i = 0; i < E.n && !hit; i++)
        for (int j = 0; j < E.n && !hit; j++) {
            if (E.e[i].src != 0 || E.e[j].src != 1) continue;
            edge p = E.e[i], q = E.e[j];
            p.t = q.t = 0; p.nt = q.nt = p.capt = q.capt = 0;
            double ax = p.a.x, ay = p.a.y, bx = p.b.x - ax, by = p.b.y - ay, cx = q.a.x, cy = q.a.y, dx = q.b.x - cx, dy = q.b.y - cy;
            double den = bx * dy - by * dx;
            if (fabs(den) < 1e-15) continue;
            double t = ((cx - ax) * dy - (cy - ay) * dx) / den, u = ((cx - ax) * by - (cy - ay) * bx) / den;
            if (t >= 0 && t <= 1 && u >= 0 && u <= 1) hit = 1;
        }
    if (!hit && A.n && A.r[0].n && inside(&B, A.r[0].p[0], eo)) hit = 1;
    if (!hit && B.n && B.r[0].n && inside(&A, B.r[0].p[0], eo)) hit = 1;
    free(E.e);
    poly_free(&A);
    poly_free(&B);
    return hit;
}
/* achatado: curvas viram retas dentro do limite (0 = 0,5) */
void *CGPathCreateCopyByFlattening(const void *path, double threshold) {
    poly P;
    flatten(path, threshold > 0 ? threshold : 0.5, &P);
    void *out = CGPathCreateMutable();
    for (int i = 0; i < P.n; i++) {
        ring *r = &P.r[i];
        if (!r->n) continue;
        CGPathMoveToPoint(out, 0, r->p[0].x, r->p[0].y);
        for (int k = 1; k < r->n; k++) CGPathAddLineToPoint(out, 0, r->p[k].x, r->p[k].y);
        if (r->closed) CGPathCloseSubpath(out);
    }
    poly_free(&P);
    return out;
}
/* componentes: cada contorno externo com os buracos dele, com as curvas originais */
typedef struct { CGPathElement *e; int n, cap; CGPoint *pts; } elist;
typedef struct { elist *sub; int n, cap; CGPoint last; } subs;
static void sub_el(void *info, const CGPathElement *e) {
    subs *S = info;
    if (e->type == 0 || !S->n) {
        if (S->n == S->cap) { int c = S->cap ? S->cap * 2 : 8; elist *ns = realloc(S->sub, (size_t)c * sizeof *ns); if (!ns) return; S->sub = ns; S->cap = c; }
        memset(&S->sub[S->n++], 0, sizeof(elist));
        if (e->type != 0) {                               /* caminho sem move inicial */
            elist *l = &S->sub[S->n - 1];
            CGPoint *p = malloc(sizeof *p);
            if (!p) return;
            p[0] = S->last;
            l->e = malloc(sizeof *l->e);
            if (!l->e) { free(p); return; }
            l->e[0].type = 0; l->e[0].points = p; l->n = l->cap = 1;
        }
    }
    elist *l = &S->sub[S->n - 1];
    if (l->n == l->cap) { int c = l->cap ? l->cap * 2 : 8; CGPathElement *ne = realloc(l->e, (size_t)c * sizeof *ne); if (!ne) return; l->e = ne; l->cap = c; }
    int np = e->type == 0 || e->type == 1 ? 1 : e->type == 2 ? 2 : e->type == 3 ? 3 : 0;
    CGPoint *pts = np ? malloc((size_t)np * sizeof *pts) : 0;
    if (np && !pts) return;
    for (int i = 0; i < np; i++) pts[i] = e->points[i];
    l->e[l->n].type = e->type;
    l->e[l->n].points = pts;
    l->n++;
    if (np) S->last = pts[np - 1];
}
static void elist_emit(void *out, const elist *l) {
    for (int i = 0; i < l->n; i++) {
        const CGPathElement *e = &l->e[i];
        switch (e->type) {
        case 0: CGPathMoveToPoint(out, 0, e->points[0].x, e->points[0].y); break;
        case 1: CGPathAddLineToPoint(out, 0, e->points[0].x, e->points[0].y); break;
        case 2: CGPathAddQuadCurveToPoint(out, 0, e->points[0].x, e->points[0].y, e->points[1].x, e->points[1].y); break;
        case 3: CGPathAddCurveToPoint(out, 0, e->points[0].x, e->points[0].y, e->points[1].x, e->points[1].y, e->points[2].x, e->points[2].y); break;
        case 4: CGPathCloseSubpath(out); break;
        }
    }
}
static double ring_area(const ring *r) { double a = 0; for (int i = 0; i < r->n; i++) { CGPoint p = r->p[i], q = r->p[(i + 1) % r->n]; a += p.x * q.y - q.x * p.y; } return a / 2; }
id CGPathCreateSeparateComponents(const void *path, bool eo) {
    subs S = {0};
    if (path) CGPathApply(path, &S, sub_el);
    int n = S.n;
    poly *fl = calloc((size_t)(n ? n : 1), sizeof *fl);
    int *depth = calloc((size_t)(n ? n : 1), sizeof *depth), *owner = calloc((size_t)(n ? n : 1), sizeof *owner);
    if (!fl || !depth || !owner) { free(fl); free(depth); free(owner); return 0; }
    for (int i = 0; i < n; i++) {
        void *tmp = CGPathCreateMutable();
        elist_emit(tmp, &S.sub[i]);
        flatten(tmp, 0.05, &fl[i]);
        CGPathRelease(tmp);
    }
    /* profundidade: quantos outros contornos contêm um ponto deste (o meio da primeira aresta, para não pegar canto) */
    for (int i = 0; i < n; i++) {
        owner[i] = -1;
        if (!fl[i].n || fl[i].r[0].n < 2) continue;
        CGPoint q = {(fl[i].r[0].p[0].x + fl[i].r[0].p[1].x) / 2, (fl[i].r[0].p[0].y + fl[i].r[0].p[1].y) / 2};
        double best = 1e300;
        for (int j = 0; j < n; j++) {
            if (j == i || !fl[j].n || !inside(&fl[j], q, eo)) continue;
            depth[i]++;
            double a = fabs(ring_area(&fl[j].r[0]));
            if (a < best) { best = a; owner[i] = j; }
        }
    }
    void **parts = calloc((size_t)(n ? n : 1), sizeof *parts);
    int np = 0;
    for (int i = 0; parts && i < n; i++) {
        if (depth[i] & 1) continue;                       /* buraco: entra com o dono */
        void *out = CGPathCreateMutable();
        elist_emit(out, &S.sub[i]);
        for (int j = 0; j < n; j++) if ((depth[j] & 1) && owner[j] == i) elist_emit(out, &S.sub[j]);
        parts[np++] = out;
    }
    id arr = CFArrayCreate(0, (const void **)parts, np, kCFTypeArrayCallBacks);
    for (int i = 0; i < np; i++) CGPathRelease(parts[i]);
    free(parts);
    for (int i = 0; i < n; i++) { poly_free(&fl[i]); for (int k = 0; k < S.sub[i].n; k++) free(S.sub[i].e[k].points); free(S.sub[i].e); }
    free(S.sub);
    free(fl); free(depth); free(owner);
    return arr;
}
