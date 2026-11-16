/* mOS — rasterizador vetorial do motor web, comum ao SVG (svg.c) e ao Canvas 2D (canvas.c):
 * números no estilo SVG, caminhos achatados (retas, cúbicas, quadráticas, arcos elípticos), dados de caminho SVG,
 * arestas e cobertura com 8 subamostras verticais e cobertura horizontal exata (não-zero e par-ímpar),
 * traço com junções miter/round/bevel, tampas e tracejado. Implementação própria. */
#include "web.h"
#include "gfx.h"
#include "raster.h"

bool rs_num_start(const char *p) { return isdigit(*p) || *p == '.' || ((*p == '-' || *p == '+') && (isdigit(p[1]) || p[1] == '.')); }

/* número no estilo SVG ("1.5e-3", "-.5", "10-5" separa) */
bool rs_parse_f(const char **pp, float *out) {
    const char *p = *pp;
    while (*p && (isspace(*p) || *p == ',')) p++;
    if (!rs_num_start(p)) return false;
    bool neg = false;
    if (*p == '+' || *p == '-') neg = *p++ == '-';
    double v = 0;
    bool dig = false;
    while (isdigit(*p)) { v = v * 10 + (*p++ - '0'); dig = true; }
    if (*p == '.') {
        p++;
        double f = 0.1;
        while (isdigit(*p)) { v += (*p++ - '0') * f; f *= 0.1; dig = true; }
    }
    if (!dig) return false;
    if ((*p == 'e' || *p == 'E') && (isdigit(p[1]) || ((p[1] == '-' || p[1] == '+') && isdigit(p[2])))) {
        p++;
        bool en = false;
        if (*p == '+' || *p == '-') en = *p++ == '-';
        int ex = 0;
        while (isdigit(*p)) { ex = ex * 10 + (*p++ - '0'); if (ex > 60) ex = 60; }
        while (ex--) v = en ? v / 10 : v * 10;
    }
    *out = (float)(neg ? -v : v);
    *pp = p;
    return true;
}

void rs_path_free(path_t *p) { if (p->xy) kfree(p->xy); if (p->start) kfree(p->start); if (p->closed) kfree(p->closed); memset(p, 0, sizeof *p); }

void rs_path_pt(path_t *p, float x, float y) {
    if (p->n == p->cap) {
        int nc = p->cap ? p->cap * 2 : 64;
        float *nx = krealloc(p->xy, sizeof(float) * 2 * nc);
        if (!nx) return;
        p->xy = nx; p->cap = nc;
    }
    p->xy[2 * p->n] = x; p->xy[2 * p->n + 1] = y; p->n++;
    p->cx = x; p->cy = y;
}
void rs_path_move(path_t *p, float x, float y) {
    if (p->nc && p->start[p->nc - 1] == p->n - 1) { p->xy[2 * (p->n - 1)] = x; p->xy[2 * (p->n - 1) + 1] = y; p->cx = x; p->cy = y; p->sx = x; p->sy = y; return; }
    if (p->nc == p->ccap) {
        int nc = p->ccap ? p->ccap * 2 : 8;
        int *ns = krealloc(p->start, sizeof(int) * nc);
        if (!ns) return;
        p->start = ns;
        u8 *ncl = krealloc(p->closed, nc);
        if (!ncl) return;
        p->closed = ncl;
        p->ccap = nc;
    }
    p->start[p->nc] = p->n;
    p->closed[p->nc] = 0;
    p->nc++;
    rs_path_pt(p, x, y);
    p->sx = x; p->sy = y;
}
void rs_path_line(path_t *p, float x, float y) { if (!p->nc) rs_path_move(p, p->cx, p->cy); rs_path_pt(p, x, y); }
void rs_path_close(path_t *p) {
    if (!p->nc) return;
    p->closed[p->nc - 1] = 1;
    p->cx = p->sx; p->cy = p->sy;
}
int rs_cont_len(const path_t *p, int c) { return (c + 1 < p->nc ? p->start[c + 1] : p->n) - p->start[c]; }

static void rs_cubic_rec(path_t *p, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, int depth) {
    float dx = x3 - x0, dy = y3 - y0;
    float d1 = fabs_((x1 - x3) * dy - (y1 - y3) * dx), d2 = fabs_((x2 - x3) * dy - (y2 - y3) * dx);
    float dd = (d1 + d2) * (d1 + d2), ll = dx * dx + dy * dy;
    if (depth > 14 || dd <= p->tol * p->tol * (ll > 1e-12f ? ll : 1e-12f) * 4) {
        if (ll < 1e-12f && (fabs_(x1 - x0) + fabs_(y1 - y0) + fabs_(x2 - x0) + fabs_(y2 - y0)) > p->tol && depth <= 14) { /* laço fechado: subdivide */ }
        else { rs_path_pt(p, x3, y3); return; }
    }
    float x01 = (x0 + x1) * .5f, y01 = (y0 + y1) * .5f, x12 = (x1 + x2) * .5f, y12 = (y1 + y2) * .5f, x23 = (x2 + x3) * .5f, y23 = (y2 + y3) * .5f;
    float xa = (x01 + x12) * .5f, ya = (y01 + y12) * .5f, xb = (x12 + x23) * .5f, yb = (y12 + y23) * .5f;
    float xm = (xa + xb) * .5f, ym = (ya + yb) * .5f;
    rs_cubic_rec(p, x0, y0, x01, y01, xa, ya, xm, ym, depth + 1);
    rs_cubic_rec(p, xm, ym, xb, yb, x23, y23, x3, y3, depth + 1);
}
void rs_path_cubic(path_t *p, float x1, float y1, float x2, float y2, float x3, float y3) {
    if (!p->nc) rs_path_move(p, p->cx, p->cy);
    rs_cubic_rec(p, p->cx, p->cy, x1, y1, x2, y2, x3, y3, 0);
}
void rs_path_quad(path_t *p, float x1, float y1, float x2, float y2) {
    float x0 = p->cx, y0 = p->cy;
    rs_path_cubic(p, x0 + 2.0f / 3 * (x1 - x0), y0 + 2.0f / 3 * (y1 - y0), x2 + 2.0f / 3 * (x1 - x2), y2 + 2.0f / 3 * (y1 - y2), x2, y2);
}

/* arco elíptico (SVG F.6.5) em cúbicas de até 90° */
void rs_path_arc(path_t *p, float rx, float ry, float phi_deg, int large, int sweep, float x, float y) {
    float x0 = p->cx, y0 = p->cy;
    if (x0 == x && y0 == y) return;
    rx = fabs_(rx); ry = fabs_(ry);
    if (rx < 1e-6f || ry < 1e-6f) { rs_path_line(p, x, y); return; }
    float phi = phi_deg * PI_F / 180.0f, cp = fcos(phi), sp = fsin(phi);
    float dx2 = (x0 - x) / 2, dy2 = (y0 - y) / 2;
    float x1p = cp * dx2 + sp * dy2, y1p = -sp * dx2 + cp * dy2;
    float lam = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lam > 1) { float s = sqrtf_(lam); rx *= s; ry *= s; }
    float num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
    float den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
    float co = den > 0 && num > 0 ? sqrtf_(num / den) : 0;
    if (large == sweep) co = -co;
    float cxp = co * rx * y1p / ry, cyp = -co * ry * x1p / rx;
    float cx = cp * cxp - sp * cyp + (x0 + x) / 2, cy = sp * cxp + cp * cyp + (y0 + y) / 2;
    float ux = (x1p - cxp) / rx, uy = (y1p - cyp) / ry, vx = (-x1p - cxp) / rx, vy = (-y1p - cyp) / ry;
    float t1 = atan2_(uy, ux);
    float dt = atan2_(ux * vy - uy * vx, ux * vx + uy * vy);
    if (!sweep && dt > 0) dt -= 2 * PI_F;
    else if (sweep && dt < 0) dt += 2 * PI_F;
    int segs = (int)(fabs_(dt) / (PI_F / 2) + 0.999f);
    if (segs < 1) segs = 1;
    float step = dt / segs, k = 4.0f / 3.0f * fsin(step / 4) / fcos(step / 4);
    for (int i = 0; i < segs; i++) {
        float a0 = t1 + step * i, a1 = a0 + step;
        float c0 = fcos(a0), s0 = fsin(a0), c1 = fcos(a1), s1 = fsin(a1);
        float ex0 = c0 - k * s0, ey0 = s0 + k * c0, ex1 = c1 + k * s1, ey1 = s1 - k * c1;
        float p1x = cx + rx * ex0 * cp - ry * ey0 * sp, p1y = cy + rx * ex0 * sp + ry * ey0 * cp;
        float p2x = cx + rx * ex1 * cp - ry * ey1 * sp, p2y = cy + rx * ex1 * sp + ry * ey1 * cp;
        float p3x = i == segs - 1 ? x : cx + rx * c1 * cp - ry * s1 * sp, p3y = i == segs - 1 ? y : cy + rx * c1 * sp + ry * s1 * cp;
        rs_path_cubic(p, p1x, p1y, p2x, p2y, p3x, p3y);
    }
}

void rs_parse_path_d(path_t *P, const char *d) {
    const char *p = d;
    char cmd = 0;
    float lcx = 0, lcy = 0;   /* último controle (S/T) */
    char prev = 0;
    while (*p) {
        while (*p && (isspace(*p) || *p == ',')) p++;
        if (!*p) break;
        if (isalpha(*p)) cmd = *p++;
        else if (!cmd) break;
        else if (!rs_num_start(p)) { p++; continue; }
        bool rel = cmd >= 'a';
        char C = (char)toupper(cmd);
        float ox = rel ? P->cx : 0, oy = rel ? P->cy : 0;
        float v[7];
        int need = C == 'M' || C == 'L' || C == 'T' ? 2 : C == 'H' || C == 'V' ? 1 : C == 'C' ? 6 : C == 'S' || C == 'Q' ? 4 : C == 'A' ? 7 : 0;
        if (C == 'Z') { rs_path_close(P); prev = 'Z'; cmd = 0; lcx = P->cx; lcy = P->cy; continue; }
        bool ok = true;
        for (int i = 0; i < need && ok; i++) {
            if (C == 'A' && (i == 3 || i == 4)) {
                while (*p && (isspace(*p) || *p == ',')) p++;
                if (*p == '0' || *p == '1') v[i] = (float)(*p++ - '0'); else ok = false;
            } else ok = rs_parse_f(&p, &v[i]);
        }
        if (!ok) break;
        switch (C) {
        case 'M': rs_path_move(P, ox + v[0], oy + v[1]); cmd = rel ? 'l' : 'L'; break;
        case 'L': rs_path_line(P, ox + v[0], oy + v[1]); break;
        case 'H': rs_path_line(P, ox + v[0], P->cy); break;
        case 'V': rs_path_line(P, P->cx, oy + v[0]); break;
        case 'C': rs_path_cubic(P, ox + v[0], oy + v[1], ox + v[2], oy + v[3], ox + v[4], oy + v[5]); lcx = ox + v[2]; lcy = oy + v[3]; break;
        case 'S': {
            float c1x = P->cx, c1y = P->cy;
            if (prev == 'C' || prev == 'S') { c1x = 2 * P->cx - lcx; c1y = 2 * P->cy - lcy; }
            rs_path_cubic(P, c1x, c1y, ox + v[0], oy + v[1], ox + v[2], oy + v[3]);
            lcx = ox + v[0]; lcy = oy + v[1];
            break;
        }
        case 'Q': rs_path_quad(P, ox + v[0], oy + v[1], ox + v[2], oy + v[3]); lcx = ox + v[0]; lcy = oy + v[1]; break;
        case 'T': {
            float c1x = P->cx, c1y = P->cy;
            if (prev == 'Q' || prev == 'T') { c1x = 2 * P->cx - lcx; c1y = 2 * P->cy - lcy; }
            rs_path_quad(P, c1x, c1y, ox + v[0], oy + v[1]);
            lcx = c1x; lcy = c1y;
            break;
        }
        case 'A': rs_path_arc(P, v[0], v[1], v[2], (int)v[3], (int)v[4], ox + v[5], oy + v[6]); break;
        default: return;
        }
        prev = C;
    }
}

void rs_edges_add(edges_t *E, float x0, float y0, float x1, float y1) {
    if (y0 == y1) return;
    int dir = 1;
    if (y0 > y1) { float t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; dir = -1; }
    if (E->n == E->cap) {
        int nc = E->cap ? E->cap * 2 : 128;
        edge_t *ne = krealloc(E->e, sizeof(edge_t) * nc);
        if (!ne) return;
        E->e = ne; E->cap = nc;
    }
    edge_t *e = &E->e[E->n++];
    e->x0 = x0; e->y0 = y0; e->x1 = x1; e->y1 = y1; e->dir = dir;
    e->dxdy = (x1 - x0) / (y1 - y0);
    if (E->n == 1) { E->minx = MIN(x0, x1); E->maxx = MAX(x0, x1); E->miny = y0; E->maxy = y1; }
    else { E->minx = MIN(E->minx, MIN(x0, x1)); E->maxx = MAX(E->maxx, MAX(x0, x1)); E->miny = MIN(E->miny, y0); E->maxy = MAX(E->maxy, y1); }
}

/* todos os contornos (fechados implicitamente para preencher), transformados por m */
void rs_edges_from_path(edges_t *E, const path_t *p, const mat_t *m) {
    for (int c = 0; c < p->nc; c++) {
        int s = p->start[c], n = rs_cont_len(p, c);
        if (n < 2) continue;
        float px, py, fx, fy;
        mapp(m, p->xy[2 * s], p->xy[2 * s + 1], &fx, &fy);
        px = fx; py = fy;
        for (int i = 1; i < n; i++) {
            float x, y;
            mapp(m, p->xy[2 * (s + i)], p->xy[2 * (s + i) + 1], &x, &y);
            rs_edges_add(E, px, py, x, y);
            px = x; py = y;
        }
        rs_edges_add(E, px, py, fx, fy);
    }
}

static int rs_edge_cmp_y(const edge_t *a, const edge_t *b) { return a->y0 < b->y0 ? -1 : a->y0 > b->y0; }
static void rs_edges_sort(edges_t *E) {
    /* ordenação por inserção em blocos + mescla simples: n costuma ser pequeno; usa shell sort */
    int n = E->n;
    for (int gap = n / 2; gap > 0; gap /= 2)
        for (int i = gap; i < n; i++) {
            edge_t t = E->e[i];
            int j = i;
            while (j >= gap && rs_edge_cmp_y(&E->e[j - gap], &t) > 0) { E->e[j] = E->e[j - gap]; j -= gap; }
            E->e[j] = t;
        }
}

#define SS 8
/* cobre a caixa [cx0,cx1)×[cy0,cy1) do canvas; devolve a máscara (a.a == NULL se vazia) */
#define SS 8
/* cobre a caixa [cx0,cx1)×[cy0,cy1) do canvas; devolve a máscara (a.a == NULL se vazia) */
mask_t rs_rasterize(edges_t *E, bool evenodd, int cx0, int cy0, int cx1, int cy1) {
    mask_t M = { 0 };
    if (!E->n) return M;
    int x0 = (int)ffloor(E->minx), y0 = (int)ffloor(E->miny), x1 = (int)fceil(E->maxx) + 1, y1 = (int)fceil(E->maxy) + 1;
    if (x0 < cx0) x0 = cx0;
    if (y0 < cy0) y0 = cy0;
    if (x1 > cx1) x1 = cx1;
    if (y1 > cy1) y1 = cy1;
    if (x1 <= x0 || y1 <= y0) return M;
    int w = x1 - x0, h = y1 - y0;
    M.a = kcalloc((usize)w * h, 1);
    float *acc = kcalloc(w + 2, sizeof(float));
    int *act = kmalloc(sizeof(int) * E->n);
    struct { float x; int dir; } *xs = kmalloc(sizeof(*xs) * (E->n + 1));
    if (!M.a || !acc || !act || !xs) { if (M.a) kfree(M.a); M.a = NULL; if (acc) kfree(acc); if (act) kfree(act); if (xs) kfree(xs); return M; }
    M.x = x0; M.y = y0; M.w = w; M.h = h;
    rs_edges_sort(E);
    int next = 0, nact = 0;
    const float wsub = 1.0f / SS;
    for (int row = y0; row < y1; row++) {
        memset(acc, 0, sizeof(float) * (w + 2));
        bool any = false;
        for (int s = 0; s < SS; s++) {
            float sy = row + (s + 0.5f) * wsub;
            while (next < E->n && E->e[next].y0 <= sy) act[nact++] = next++;
            int nx = 0;
            for (int i = 0; i < nact;) {
                edge_t *e = &E->e[act[i]];
                if (e->y1 <= sy) { act[i] = act[--nact]; continue; }
                if (e->y0 <= sy) { xs[nx].x = e->x0 + (sy - e->y0) * e->dxdy; xs[nx].dir = e->dir; nx++; }
                i++;
            }
            if (nx < 2) continue;
            for (int i = 1; i < nx; i++) {
                float tx = xs[i].x; int td = xs[i].dir; int j = i;
                while (j > 0 && xs[j - 1].x > tx) { xs[j] = xs[j - 1]; j--; }
                xs[j].x = tx; xs[j].dir = td;
            }
            int wind = 0;
            for (int i = 0; i < nx - 1; i++) {
                wind += xs[i].dir;
                bool in = evenodd ? (wind & 1) != 0 : wind != 0;
                if (!in) continue;
                float a = xs[i].x - x0, b = xs[i + 1].x - x0;
                if (b <= 0 || a >= w) continue;
                if (a < 0) a = 0;
                if (b > w) b = (float)w;
                if (b <= a) continue;
                any = true;
                int ia = (int)a, ib = (int)b;
                if (ia == ib) acc[ia] += (b - a) * wsub;
                else {
                    acc[ia] += (ia + 1 - a) * wsub;
                    for (int k = ia + 1; k < ib; k++) acc[k] += wsub;
                    if (ib < w) acc[ib] += (b - ib) * wsub;
                }
            }
        }
        if (!any) continue;
        u8 *dst = M.a + (usize)(row - y0) * w;
        for (int x = 0; x < w; x++) { float v = acc[x] * 255.0f + 0.5f; dst[x] = v >= 255 ? 255 : v <= 0 ? 0 : (u8)v; }
    }
    kfree(acc); kfree(act); kfree(xs);
    return M;
}

void rs_poly_add(path_t *o, const float *xy, int n) {
    if (n < 3) return;
    float area = 0;
    for (int i = 0; i < n; i++) { int j = (i + 1) % n; area += xy[2 * i] * xy[2 * j + 1] - xy[2 * j] * xy[2 * i + 1]; }
    if (fabs_(area) < 1e-12f) return;
    rs_path_move(o, xy[0], xy[1]);
    if (area > 0) for (int i = 1; i < n; i++) rs_path_pt(o, xy[2 * i], xy[2 * i + 1]);
    else { o->xy[2 * (o->n - 1)] = xy[2 * (n - 1)]; o->xy[2 * (o->n - 1) + 1] = xy[2 * (n - 1) + 1]; for (int i = n - 2; i >= 0; i--) rs_path_pt(o, xy[2 * i], xy[2 * i + 1]); }
    rs_path_close(o);
}

void rs_circle_poly(path_t *o, float cx, float cy, float r, float scale) {
    int n = (int)(2 * PI_F * r * scale / 2.0f);
    if (n < 8) n = 8;
    if (n > 128) n = 128;
    float xy[256];
    for (int i = 0; i < n; i++) { float a = 2 * PI_F * i / n; xy[2 * i] = cx + r * fcos(a); xy[2 * i + 1] = cy + r * fsin(a); }
    rs_poly_add(o, xy, n);
}

/* contorno de uma polilinha (pontos sem repetição) */
void rs_stroke_poly(path_t *o, const float *pt, int n, bool closed, const rs_stroke_t *st, float hw, float scale) {
    if (n == 1 || (n == 2 && pt[0] == pt[2] && pt[1] == pt[3])) {
        if (closed) return;
        if (st->cap == CAP_ROUND) rs_circle_poly(o, pt[0], pt[1], hw, scale);
        else if (st->cap == CAP_SQUARE) { float q[8] = { pt[0] - hw, pt[1] - hw, pt[0] + hw, pt[1] - hw, pt[0] + hw, pt[1] + hw, pt[0] - hw, pt[1] + hw }; rs_poly_add(o, q, 4); }
        return;
    }
    int segs = closed ? n : n - 1;
    for (int i = 0; i < segs; i++) {
        const float *a = pt + 2 * i, *b = pt + 2 * ((i + 1) % n);
        float dx = b[0] - a[0], dy = b[1] - a[1], l = sqrtf_(dx * dx + dy * dy);
        if (l < 1e-9f) continue;
        float nx = -dy / l * hw, ny = dx / l * hw;
        float ex = 0, ey = 0, sx = 0, sy = 0;
        if (!closed && st->cap == CAP_SQUARE) {
            if (i == 0) { sx = -dx / l * hw; sy = -dy / l * hw; }
            if (i == segs - 1) { ex = dx / l * hw; ey = dy / l * hw; }
        }
        float q[8] = { a[0] + sx + nx, a[1] + sy + ny, b[0] + ex + nx, b[1] + ey + ny, b[0] + ex - nx, b[1] + ey - ny, a[0] + sx - nx, a[1] + sy - ny };
        rs_poly_add(o, q, 4);
    }
    /* junções */
    int first = closed ? 0 : 1, last = closed ? n : n - 1;
    for (int i = first; i < last; i++) {
        const float *pv = pt + 2 * ((i - 1 + n) % n), *v = pt + 2 * i, *nxp = pt + 2 * ((i + 1) % n);
        float d1x = v[0] - pv[0], d1y = v[1] - pv[1], d2x = nxp[0] - v[0], d2y = nxp[1] - v[1];
        float l1 = sqrtf_(d1x * d1x + d1y * d1y), l2 = sqrtf_(d2x * d2x + d2y * d2y);
        if (l1 < 1e-9f || l2 < 1e-9f) continue;
        d1x /= l1; d1y /= l1; d2x /= l2; d2y /= l2;
        float cr = d1x * d2y - d1y * d2x, dt = d1x * d2x + d1y * d2y;
        if (fabs_(cr) < 1e-6f && dt > 0) continue;
        if (st->join == JOIN_ROUND) { rs_circle_poly(o, v[0], v[1], hw, scale); continue; }
        /* lado de fora da curva */
        float sgn = cr > 0 ? -1.0f : 1.0f;
        float o1x = -d1y * hw * sgn, o1y = d1x * hw * sgn, o2x = -d2y * hw * sgn, o2y = d2x * hw * sgn;
        float tri[8] = { v[0], v[1], v[0] + o1x, v[1] + o1y, 0, 0, v[0] + o2x, v[1] + o2y };
        if (st->join == JOIN_MITER) {
            float cos_half = sqrtf_((1 + dt) / 2);
            float ratio = cos_half > 1e-6f ? 1.0f / cos_half : 1e9f;
            if (ratio <= st->miter) {
                float mx = o1x + o2x, my = o1y + o2y, ml = sqrtf_(mx * mx + my * my);
                if (ml > 1e-9f) {
                    tri[4] = v[0] + mx / ml * hw * ratio; tri[5] = v[1] + my / ml * hw * ratio;
                    rs_poly_add(o, tri, 4);
                    continue;
                }
            }
        }
        float bev[6] = { v[0], v[1], v[0] + o1x, v[1] + o1y, v[0] + o2x, v[1] + o2y };
        rs_poly_add(o, bev, 3);
    }
    if (!closed && st->cap == CAP_ROUND) {
        rs_circle_poly(o, pt[0], pt[1], hw, scale);
        rs_circle_poly(o, pt[2 * (n - 1)], pt[2 * (n - 1) + 1], hw, scale);
    }
}

/* tracejado: corta a polilinha e chama stroke_poly em cada traço */
void rs_stroke_contour(path_t *o, const float *pt, int n, bool closed, const rs_stroke_t *st, float hw, float scale, float dscale) {
    if (!st->ndash) { rs_stroke_poly(o, pt, n, closed, st, hw, scale); return; }
    float dash[32];
    int nd = st->ndash;
    for (int i = 0; i < nd; i++) dash[i] = st->dash[i] * dscale;
    if (nd & 1) { for (int i = 0; i < nd; i++) dash[nd + i] = dash[i]; nd *= 2; }
    float total = 0;
    for (int i = 0; i < nd; i++) total += dash[i];
    if (total <= 0) { rs_stroke_poly(o, pt, n, closed, st, hw, scale); return; }
    float off = st->dashoff * dscale;
    off -= ffloor(off / total) * total;
    int di = 0;
    while (off >= dash[di]) { off -= dash[di]; di = (di + 1) % nd; }
    float rem = dash[di] - off;
    bool on = !(di & 1);
    float *buf = kmalloc(sizeof(float) * 2 * (n + 2) * 2 + 64);
    if (!buf) return;
    int bn = 0;
    int segs = closed ? n : n - 1;
    if (on) { buf[0] = pt[0]; buf[1] = pt[1]; bn = 1; }
    for (int i = 0; i < segs; i++) {
        float ax = pt[2 * i], ay = pt[2 * i + 1];
        const float *b = pt + 2 * ((i + 1) % n);
        float dx = b[0] - ax, dy = b[1] - ay, l = sqrtf_(dx * dx + dy * dy), pos = 0;
        while (l - pos > rem) {
            pos += rem;
            float x = ax + dx * pos / l, y = ay + dy * pos / l;
            if (on) { buf[2 * bn] = x; buf[2 * bn + 1] = y; bn++; rs_stroke_poly(o, buf, bn, false, st, hw, scale); bn = 0; }
            else { buf[0] = x; buf[1] = y; bn = 1; }
            on = !on;
            di = (di + 1) % nd;
            rem = dash[di];
        }
        rem -= l - pos;
        if (on) {
            if (bn >= n + 2) { rs_stroke_poly(o, buf, bn, false, st, hw, scale); buf[0] = buf[2 * (bn - 1)]; buf[1] = buf[2 * (bn - 1) + 1]; bn = 1; }
            buf[2 * bn] = b[0]; buf[2 * bn + 1] = b[1]; bn++;
        }
    }
    if (on && bn > 1) rs_stroke_poly(o, buf, bn, false, st, hw, scale);
    kfree(buf);
}

/* contorno do traço de todos os contornos de p (espaço de p), já com tracejado, tampas e junções */
void rs_stroke_path(path_t *o, const path_t *p, const rs_stroke_t *st, float hw, float scale, float dscale) {
    float *tmp = kmalloc(sizeof(float) * 2 * (p->n + 1));
    if (!tmp) return;
    if (!o->tol) o->tol = p->tol;
    for (int c = 0; c < p->nc; c++) {
        int s = p->start[c], n = rs_cont_len(p, c), k = 0;
        for (int i = 0; i < n; i++) {
            float x = p->xy[2 * (s + i)], y = p->xy[2 * (s + i) + 1];
            if (k && fabs_(x - tmp[2 * (k - 1)]) < 1e-6f && fabs_(y - tmp[2 * (k - 1) + 1]) < 1e-6f) continue;
            tmp[2 * k] = x; tmp[2 * k + 1] = y; k++;
        }
        bool closed = p->closed[c];
        if (closed && k > 1 && fabs_(tmp[0] - tmp[2 * (k - 1)]) < 1e-6f && fabs_(tmp[1] - tmp[2 * (k - 1) + 1]) < 1e-6f) k--;
        if (k) rs_stroke_contour(o, tmp, k, closed && k > 2, st, hw, scale, dscale);
    }
    kfree(tmp);
}
