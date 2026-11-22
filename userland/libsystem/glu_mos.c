/*
 * mOS libSystem — GLU (OpenGL.framework): gluPerspective, gluLookAt, gluProject, gluErrorString, quádricas
 * (gluSphere/gluCylinder/gluDisk/gluPartialDisk com estilo, normais, orientação e coordenadas de textura) e o
 * tesselador (gluNewTess...): projeta os contornos no plano do polígono, liga cada furo ao contorno de fora por uma
 * ponte até um vértice visível e corta orelhas, entregando GL_TRIANGLES pelos callbacks (ou só os contornos com
 * GLU_TESS_BOUNDARY_ONLY). Desenha pelo pipeline fixo do gl_ffp.c. gluCheckExtension/gluUnProject: services_corpus.c.
 */
#include "libc.h"

typedef unsigned int GLenum;
typedef double GLdouble;
void glBegin(GLenum m);
void glEnd(void);
void glVertex3d(double x, double y, double z);
void glNormal3d(double x, double y, double z);
void glTexCoord2d(double s, double t);
void glMultMatrixd(const double *m);
void glTranslated(double x, double y, double z);
void glFrustum(double l, double r, double b, double t, double n, double f);
double sin(double);
double cos(double);
double tan(double);
double sqrt(double);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);

#define GL_POINTS 0
#define GL_LINES 1
#define GL_LINE_LOOP 2
#define GL_LINE_STRIP 3
#define GL_TRIANGLES 4
#define GL_TRIANGLE_STRIP 5
#define GL_TRIANGLE_FAN 6
#define PI_ 3.14159265358979323846

/* ================================================================ matrizes */
void gluPerspective(double fovy, double aspect, double zn, double zf) {
    double t = zn * tan(fovy * PI_ / 360.0), r = t * aspect;
    glFrustum(-r, r, -t, t, zn, zf);
}
void gluLookAt(double ex, double ey, double ez, double cx, double cy, double cz, double ux, double uy, double uz) {
    double f[3] = {cx - ex, cy - ey, cz - ez}, u[3] = {ux, uy, uz}, s[3], v[3];
    double n = sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (n == 0) return;
    for (int i = 0; i < 3; i++) f[i] /= n;
    s[0] = f[1] * u[2] - f[2] * u[1]; s[1] = f[2] * u[0] - f[0] * u[2]; s[2] = f[0] * u[1] - f[1] * u[0];
    n = sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    if (n == 0) return;
    for (int i = 0; i < 3; i++) s[i] /= n;
    v[0] = s[1] * f[2] - s[2] * f[1]; v[1] = s[2] * f[0] - s[0] * f[2]; v[2] = s[0] * f[1] - s[1] * f[0];
    double m[16] = {s[0], v[0], -f[0], 0, s[1], v[1], -f[1], 0, s[2], v[2], -f[2], 0, 0, 0, 0, 1};
    glMultMatrixd(m);
    glTranslated(-ex, -ey, -ez);
}
int gluProject(double ox, double oy, double oz, const double *model, const double *proj, const int *vp, double *wx, double *wy, double *wz) {
    if (!model || !proj || !vp) return 0;
    double in[4] = {ox, oy, oz, 1}, e[4], c[4];
    for (int i = 0; i < 4; i++) e[i] = model[i] * in[0] + model[4 + i] * in[1] + model[8 + i] * in[2] + model[12 + i] * in[3];
    for (int i = 0; i < 4; i++) c[i] = proj[i] * e[0] + proj[4 + i] * e[1] + proj[8 + i] * e[2] + proj[12 + i] * e[3];
    if (c[3] == 0) return 0;
    for (int i = 0; i < 3; i++) c[i] /= c[3];
    if (wx) *wx = vp[0] + (c[0] + 1) * vp[2] / 2;
    if (wy) *wy = vp[1] + (c[1] + 1) * vp[3] / 2;
    if (wz) *wz = (c[2] + 1) / 2;
    return 1;
}
const u8 *gluErrorString(GLenum e) {
    switch (e) {
    case 0: return (const u8 *)"no error";
    case 0x500: case 100900: return (const u8 *)"invalid enumerant";
    case 0x501: case 100901: return (const u8 *)"invalid value";
    case 0x502: return (const u8 *)"invalid operation";
    case 0x503: return (const u8 *)"stack overflow";
    case 0x504: return (const u8 *)"stack underflow";
    case 0x505: case 100902: return (const u8 *)"out of memory";
    case 0x506: return (const u8 *)"invalid framebuffer operation";
    case 100903: return (const u8 *)"incompatible gl version";
    case 100151: return (const u8 *)"gluTessBeginPolygon() must precede a gluTessEndPolygon()";
    case 100152: return (const u8 *)"gluTessBeginContour() must precede a gluTessEndContour()";
    case 100153: return (const u8 *)"gluTessEndPolygon() must follow a gluTessBeginPolygon()";
    case 100154: return (const u8 *)"gluTessEndContour() must follow a gluTessBeginContour()";
    case 100155: return (const u8 *)"a coordinate is too large";
    case 100156: return (const u8 *)"need combine callback";
    }
    return 0;
}

/* ================================================================ quádricas */
enum { GLU_SMOOTH = 100000, GLU_FLAT = 100001, GLU_NONE = 100002, GLU_POINT = 100010, GLU_LINE = 100011, GLU_FILL = 100012,
       GLU_SILHOUETTE = 100013, GLU_OUTSIDE = 100020, GLU_INSIDE = 100021, GLU_ERROR = 100103 };
typedef struct { GLenum style, normals, orient; int tex; void (*err)(GLenum); } quad_t;
quad_t *gluNewQuadric(void) {
    quad_t *q = calloc(1, sizeof *q);
    if (q) { q->style = GLU_FILL; q->normals = GLU_SMOOTH; q->orient = GLU_OUTSIDE; }
    return q;
}
void gluDeleteQuadric(quad_t *q) { free(q); }
void gluQuadricDrawStyle(quad_t *q, GLenum s) { if (q) q->style = s; }
void gluQuadricNormals(quad_t *q, GLenum n) { if (q) q->normals = n; }
void gluQuadricOrientation(quad_t *q, GLenum o) { if (q) q->orient = o; }
void gluQuadricTexture(quad_t *q, u8 t) { if (q) q->tex = t; }
void gluQuadricCallback(quad_t *q, GLenum which, void (*fn)(GLenum)) { if (q && which == GLU_ERROR) q->err = fn; }
static void qn(const quad_t *q, double x, double y, double z) {
    if (q->normals == GLU_NONE) return;
    double s = q->orient == GLU_INSIDE ? -1 : 1;
    glNormal3d(x * s, y * s, z * s);
}
static void qt(const quad_t *q, double s, double t) { if (q->tex) glTexCoord2d(s, t); }

/* esfera em torno do eixo z: pilhas de +z (t = 1) a -z (t = 0), fatias anti-horárias vistas de +z */
void gluSphere(quad_t *q, double r, int slices, int stacks) {
    if (!q || slices < 2 || stacks < 1 || r < 0) return;
    int inside = q->orient == GLU_INSIDE;
    if (q->style == GLU_FILL) {
        for (int i = 0; i < stacks; i++) {
            double t0 = PI_ * i / stacks, t1 = PI_ * (i + 1) / stacks;
            glBegin(GL_TRIANGLE_STRIP);
            for (int j = 0; j <= slices; j++) {
                int jj = inside ? slices - j : j;
                double p = 2 * PI_ * jj / slices, cp = cos(p), sp = sin(p);
                double ax = sin(t0) * cp, ay = sin(t0) * sp, az = cos(t0);
                double bx = sin(t1) * cp, by = sin(t1) * sp, bz = cos(t1);
                qn(q, ax, ay, az); qt(q, (double)jj / slices, 1 - (double)i / stacks); glVertex3d(ax * r, ay * r, az * r);
                qn(q, bx, by, bz); qt(q, (double)jj / slices, 1 - (double)(i + 1) / stacks); glVertex3d(bx * r, by * r, bz * r);
            }
            glEnd();
        }
        return;
    }
    /* linhas/pontos: meridianos e paralelos */
    GLenum m = q->style == GLU_POINT ? GL_POINTS : GL_LINE_STRIP;
    for (int i = 1; i < stacks; i++) {
        double t = PI_ * i / stacks;
        glBegin(q->style == GLU_POINT ? GL_POINTS : GL_LINE_LOOP);
        for (int j = 0; j < slices; j++) { double p = 2 * PI_ * j / slices; double x = sin(t) * cos(p), y = sin(t) * sin(p), z = cos(t); qn(q, x, y, z); glVertex3d(x * r, y * r, z * r); }
        glEnd();
    }
    for (int j = 0; j < slices; j++) {
        double p = 2 * PI_ * j / slices;
        glBegin(m);
        for (int i = 0; i <= stacks; i++) { double t = PI_ * i / stacks; double x = sin(t) * cos(p), y = sin(t) * sin(p), z = cos(t); qn(q, x, y, z); glVertex3d(x * r, y * r, z * r); }
        glEnd();
    }
}
/* cilindro (ou cone) ao longo de +z: raio 'base' em z = 0 e 'top' em z = height */
void gluCylinder(quad_t *q, double base, double top, double height, int slices, int stacks) {
    if (!q || slices < 2 || stacks < 1 || base < 0 || top < 0 || height < 0) return;
    double nz = (base - top) / (height ? height : 1);
    double nl = sqrt(1 + nz * nz);
    int inside = q->orient == GLU_INSIDE;
    if (q->style == GLU_FILL) {
        for (int i = 0; i < stacks; i++) {
            double z0 = height * i / stacks, z1 = height * (i + 1) / stacks;
            double r0 = base + (top - base) * i / stacks, r1 = base + (top - base) * (i + 1) / stacks;
            glBegin(GL_TRIANGLE_STRIP);
            for (int j = 0; j <= slices; j++) {
                int jj = inside ? slices - j : j;
                double p = 2 * PI_ * jj / slices, cp = cos(p), sp = sin(p);
                qn(q, cp / nl, sp / nl, nz / nl);
                qt(q, (double)jj / slices, (double)(i + 1) / stacks); glVertex3d(r1 * cp, r1 * sp, z1);
                qt(q, (double)jj / slices, (double)i / stacks); glVertex3d(r0 * cp, r0 * sp, z0);
            }
            glEnd();
        }
        return;
    }
    for (int i = 0; i <= stacks; i++) {
        double z = height * i / stacks, r = base + (top - base) * i / stacks;
        glBegin(q->style == GLU_POINT ? GL_POINTS : GL_LINE_LOOP);
        for (int j = 0; j < slices; j++) { double p = 2 * PI_ * j / slices; qn(q, cos(p) / nl, sin(p) / nl, nz / nl); glVertex3d(r * cos(p), r * sin(p), z); }
        glEnd();
        if (q->style == GLU_SILHOUETTE && i == 0) i = stacks - 1;     /* silhueta: só as bordas */
    }
    if (q->style == GLU_SILHOUETTE) return;
    glBegin(q->style == GLU_POINT ? GL_POINTS : GL_LINES);
    for (int j = 0; j < slices; j++) {
        double p = 2 * PI_ * j / slices;
        qn(q, cos(p) / nl, sin(p) / nl, nz / nl);
        glVertex3d(base * cos(p), base * sin(p), 0);
        glVertex3d(top * cos(p), top * sin(p), height);
    }
    glEnd();
}
/* disco parcial no plano z = 0: ângulos em graus a partir de +y, no sentido horário (como o SGI GLU) */
void gluPartialDisk(quad_t *q, double inner, double outer, int slices, int loops, double start, double sweep) {
    if (!q || slices < 2 || loops < 1 || inner < 0 || outer < inner) return;
    if (sweep > 360) sweep = 360;
    if (sweep < -360) sweep = -360;
    double nzs = q->orient == GLU_INSIDE ? -1 : 1;
    double a0 = start * PI_ / 180, da = sweep * PI_ / 180 / slices;
    if (q->normals != GLU_NONE) glNormal3d(0, 0, nzs);
    if (q->style == GLU_FILL) {
        for (int l = 0; l < loops; l++) {
            double r0 = inner + (outer - inner) * l / loops, r1 = inner + (outer - inner) * (l + 1) / loops;
            glBegin(GL_TRIANGLE_STRIP);
            for (int j = 0; j <= slices; j++) {
                int jj = nzs < 0 ? slices - j : j;
                double a = a0 + da * jj, sa = sin(a), ca = cos(a);
                qt(q, 0.5 + sa * r1 / outer / 2, 0.5 + ca * r1 / outer / 2); glVertex3d(r1 * sa, r1 * ca, 0);
                qt(q, 0.5 + sa * r0 / outer / 2, 0.5 + ca * r0 / outer / 2); glVertex3d(r0 * sa, r0 * ca, 0);
            }
            glEnd();
        }
        return;
    }
    int full = sweep >= 360 || sweep <= -360;
    for (int l = 0; l <= loops; l++) {
        double r = inner + (outer - inner) * l / loops;
        if (r == 0) continue;
        glBegin(q->style == GLU_POINT ? GL_POINTS : full ? GL_LINE_LOOP : GL_LINE_STRIP);
        for (int j = 0; j < slices + !full; j++) { double a = a0 + da * j; glVertex3d(r * sin(a), r * cos(a), 0); }
        glEnd();
        if (q->style == GLU_SILHOUETTE && l == 0) l = loops - 1;
    }
    if (q->style == GLU_SILHOUETTE && full) return;
    glBegin(q->style == GLU_POINT ? GL_POINTS : GL_LINES);
    for (int j = 0; j <= slices; j += q->style == GLU_SILHOUETTE ? slices : 1) {
        double a = a0 + da * j;
        glVertex3d(inner * sin(a), inner * cos(a), 0);
        glVertex3d(outer * sin(a), outer * cos(a), 0);
    }
    glEnd();
}
void gluDisk(quad_t *q, double inner, double outer, int slices, int loops) { gluPartialDisk(q, inner, outer, slices, loops, 0, 360); }

/* ================================================================ tesselador */
enum { TESS_BEGIN = 100100, TESS_VERTEX, TESS_END, TESS_ERROR, TESS_EDGE_FLAG, TESS_COMBINE,
       TESS_BEGIN_DATA, TESS_VERTEX_DATA, TESS_END_DATA, TESS_ERROR_DATA, TESS_EDGE_FLAG_DATA, TESS_COMBINE_DATA };
enum { TESS_WINDING_RULE = 100140, TESS_BOUNDARY_ONLY = 100141, TESS_TOLERANCE = 100142 };
enum { ERR_MISSING_BEGIN_POLYGON = 100151, ERR_MISSING_BEGIN_CONTOUR, ERR_MISSING_END_POLYGON, ERR_MISSING_END_CONTOUR };
typedef struct { double c[3]; void *data; double u, v; } tv_t;
typedef struct { int first, n; } contour_t;
typedef struct {
    void *cb[12];
    void *pdata;
    double normal[3];
    double winding, tol;
    int boundary, in_poly, in_contour;
    tv_t *v; int nv, capv;
    contour_t *ct; int nct, capct;
} tess_t;
tess_t *gluNewTess(void) {
    tess_t *t = calloc(1, sizeof *t);
    if (t) t->winding = 100130;                  /* GLU_TESS_WINDING_ODD */
    return t;
}
void gluDeleteTess(tess_t *t) { if (t) { free(t->v); free(t->ct); free(t); } }
void gluTessCallback(tess_t *t, GLenum which, void (*fn)(void)) { if (t && which >= TESS_BEGIN && which <= TESS_COMBINE_DATA) t->cb[which - TESS_BEGIN] = (void *)fn; }
void gluTessProperty(tess_t *t, GLenum which, double v) {
    if (!t) return;
    if (which == TESS_WINDING_RULE) t->winding = v;
    else if (which == TESS_BOUNDARY_ONLY) t->boundary = v != 0;
    else if (which == TESS_TOLERANCE) t->tol = v;
}
void gluGetTessProperty(tess_t *t, GLenum which, double *v) {
    if (!t || !v) return;
    *v = which == TESS_WINDING_RULE ? t->winding : which == TESS_BOUNDARY_ONLY ? t->boundary : which == TESS_TOLERANCE ? t->tol : 0;
}
void gluTessNormal(tess_t *t, double x, double y, double z) { if (t) { t->normal[0] = x; t->normal[1] = y; t->normal[2] = z; } }
static void t_err(tess_t *t, GLenum e) {
    if (t->cb[TESS_ERROR_DATA - TESS_BEGIN]) ((void (*)(GLenum, void *))t->cb[TESS_ERROR_DATA - TESS_BEGIN])(e, t->pdata);
    else if (t->cb[TESS_ERROR - TESS_BEGIN]) ((void (*)(GLenum))t->cb[TESS_ERROR - TESS_BEGIN])(e);
}
static void t_begin(tess_t *t, GLenum m) {
    if (t->cb[TESS_BEGIN_DATA - TESS_BEGIN]) ((void (*)(GLenum, void *))t->cb[TESS_BEGIN_DATA - TESS_BEGIN])(m, t->pdata);
    else if (t->cb[TESS_BEGIN - TESS_BEGIN]) ((void (*)(GLenum))t->cb[TESS_BEGIN - TESS_BEGIN])(m);
}
static void t_vertex(tess_t *t, void *d) {
    if (t->cb[TESS_VERTEX_DATA - TESS_BEGIN]) ((void (*)(void *, void *))t->cb[TESS_VERTEX_DATA - TESS_BEGIN])(d, t->pdata);
    else if (t->cb[TESS_VERTEX - TESS_BEGIN]) ((void (*)(void *))t->cb[TESS_VERTEX - TESS_BEGIN])(d);
}
static void t_end(tess_t *t) {
    if (t->cb[TESS_END_DATA - TESS_BEGIN]) ((void (*)(void *))t->cb[TESS_END_DATA - TESS_BEGIN])(t->pdata);
    else if (t->cb[TESS_END - TESS_BEGIN]) ((void (*)(void))t->cb[TESS_END - TESS_BEGIN])();
}
static void t_edge(tess_t *t, u8 f) {
    if (t->cb[TESS_EDGE_FLAG_DATA - TESS_BEGIN]) ((void (*)(u8, void *))t->cb[TESS_EDGE_FLAG_DATA - TESS_BEGIN])(f, t->pdata);
    else if (t->cb[TESS_EDGE_FLAG - TESS_BEGIN]) ((void (*)(u8))t->cb[TESS_EDGE_FLAG - TESS_BEGIN])(f);
}
void gluTessBeginPolygon(tess_t *t, void *data) {
    if (!t) return;
    if (t->in_poly) { t_err(t, ERR_MISSING_END_POLYGON); return; }
    t->in_poly = 1; t->pdata = data; t->nv = 0; t->nct = 0;
}
void gluTessBeginContour(tess_t *t) {
    if (!t) return;
    if (!t->in_poly) { t_err(t, ERR_MISSING_BEGIN_POLYGON); return; }
    if (t->in_contour) { t_err(t, ERR_MISSING_END_CONTOUR); return; }
    if (t->nct == t->capct) { int nc = t->capct ? t->capct * 2 : 8; contour_t *n = realloc(t->ct, sizeof *n * (size_t)nc); if (!n) return; t->ct = n; t->capct = nc; }
    t->ct[t->nct].first = t->nv; t->ct[t->nct].n = 0;
    t->nct++;
    t->in_contour = 1;
}
void gluTessVertex(tess_t *t, const double *c, void *data) {
    if (!t || !c) return;
    if (!t->in_contour) { t_err(t, ERR_MISSING_BEGIN_CONTOUR); return; }
    contour_t *ct = &t->ct[t->nct - 1];
    if (ct->n) {                                 /* vértice repetido em seguida não conta */
        tv_t *p = &t->v[t->nv - 1];
        if (p->c[0] == c[0] && p->c[1] == c[1] && p->c[2] == c[2]) return;
    }
    if (t->nv == t->capv) { int nc = t->capv ? t->capv * 2 : 64; tv_t *n = realloc(t->v, sizeof *n * (size_t)nc); if (!n) return; t->v = n; t->capv = nc; }
    tv_t *v = &t->v[t->nv++];
    v->c[0] = c[0]; v->c[1] = c[1]; v->c[2] = c[2]; v->data = data;
    ct->n++;
}
void gluTessEndContour(tess_t *t) {
    if (!t) return;
    if (!t->in_contour) { t_err(t, ERR_MISSING_BEGIN_CONTOUR); return; }
    t->in_contour = 0;
}
static double area2(const tv_t *v, const int *idx, int n) {
    double a = 0;
    for (int i = 0; i < n; i++) { const tv_t *p = &v[idx[i]], *q = &v[idx[(i + 1) % n]]; a += p->u * q->v - q->u * p->v; }
    return a;
}
static double cross3(const tv_t *a, const tv_t *b, const tv_t *c) { return (b->u - a->u) * (c->v - a->v) - (b->v - a->v) * (c->u - a->u); }
static int seg_cross(const tv_t *a, const tv_t *b, const tv_t *c, const tv_t *d) {
    double d1 = cross3(c, d, a), d2 = cross3(c, d, b), d3 = cross3(a, b, c), d4 = cross3(a, b, d);
    return ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0));
}
static int in_tri(const tv_t *p, const tv_t *a, const tv_t *b, const tv_t *c) {
    return cross3(a, b, p) >= 0 && cross3(b, c, p) >= 0 && cross3(c, a, p) >= 0;
}
void gluTessEndPolygon(tess_t *t) {
    if (!t) return;
    if (t->in_contour) { t_err(t, ERR_MISSING_END_CONTOUR); t->in_contour = 0; }
    if (!t->in_poly) { t_err(t, ERR_MISSING_BEGIN_POLYGON); return; }
    t->in_poly = 0;
    if (!t->nv) return;
    /* normal: a dada ou a de Newell; projeta descartando o maior componente */
    double n[3] = {t->normal[0], t->normal[1], t->normal[2]};
    if (n[0] == 0 && n[1] == 0 && n[2] == 0)
        for (int k = 0; k < t->nct; k++)
            for (int i = 0; i < t->ct[k].n; i++) {
                const double *a = t->v[t->ct[k].first + i].c, *b = t->v[t->ct[k].first + (i + 1) % t->ct[k].n].c;
                n[0] += (a[1] - b[1]) * (a[2] + b[2]); n[1] += (a[2] - b[2]) * (a[0] + b[0]); n[2] += (a[0] - b[0]) * (a[1] + b[1]);
            }
    double ax = n[0] < 0 ? -n[0] : n[0], ay = n[1] < 0 ? -n[1] : n[1], az = n[2] < 0 ? -n[2] : n[2];
    int drop = az >= ax && az >= ay ? 2 : ax >= ay ? 0 : 1;
    int flip = n[drop] < 0;
    for (int i = 0; i < t->nv; i++) {
        double *c = t->v[i].c;
        if (drop == 2) { t->v[i].u = c[0]; t->v[i].v = c[1]; }
        else if (drop == 0) { t->v[i].u = c[1]; t->v[i].v = c[2]; }
        else { t->v[i].u = c[2]; t->v[i].v = c[0]; }
        if (flip) t->v[i].u = -t->v[i].u;      /* mantém a orientação vista pela normal */
    }
    if (t->boundary) {
        for (int k = 0; k < t->nct; k++) {
            if (t->ct[k].n < 2) continue;
            t_begin(t, GL_LINE_LOOP);
            for (int i = 0; i < t->ct[k].n; i++) t_vertex(t, t->v[t->ct[k].first + i].data);
            t_end(t);
        }
        return;
    }
    /* o contorno de maior área é o de fora (anti-horário); os outros são furos (horários) ligados por pontes */
    int outer = -1;
    double best = 0;
    int *idx = malloc(sizeof(int) * (size_t)(t->nv * 3 + 8));
    if (!idx) return;
    for (int k = 0; k < t->nct; k++) {
        if (t->ct[k].n < 3) continue;
        for (int i = 0; i < t->ct[k].n; i++) idx[i] = t->ct[k].first + i;
        double a = area2(t->v, idx, t->ct[k].n);
        if (a < 0) a = -a;
        if (a > best) { best = a; outer = k; }
    }
    if (outer < 0) { free(idx); return; }
    int m = 0;
    for (int i = 0; i < t->ct[outer].n; i++) idx[m++] = t->ct[outer].first + i;
    if (area2(t->v, idx, m) < 0) for (int i = 0; i < m / 2; i++) { int x = idx[i]; idx[i] = idx[m - 1 - i]; idx[m - 1 - i] = x; }
    for (int k = 0; k < t->nct; k++) {
        if (k == outer || t->ct[k].n < 3) continue;
        int hn = t->ct[k].n, *h = malloc(sizeof(int) * (size_t)hn);
        if (!h) continue;
        for (int i = 0; i < hn; i++) h[i] = t->ct[k].first + i;
        if (area2(t->v, h, hn) > 0) for (int i = 0; i < hn / 2; i++) { int x = h[i]; h[i] = h[hn - 1 - i]; h[hn - 1 - i] = x; }
        /* vértice do furo mais à direita; ponte para o vértice do anel mais próximo sem cruzar arestas */
        int hi = 0;
        for (int i = 1; i < hn; i++) if (t->v[h[i]].u > t->v[h[hi]].u) hi = i;
        const tv_t *hp = &t->v[h[hi]];
        int bi = -1;
        double bd = 0;
        for (int i = 0; i < m; i++) {
            const tv_t *o = &t->v[idx[i]];
            double d = (o->u - hp->u) * (o->u - hp->u) + (o->v - hp->v) * (o->v - hp->v);
            if (bi >= 0 && d >= bd) continue;
            int ok = 1;
            for (int j = 0; j < m && ok; j++) if (seg_cross(hp, o, &t->v[idx[j]], &t->v[idx[(j + 1) % m]])) ok = 0;
            for (int j = 0; j < hn && ok; j++) if (seg_cross(hp, o, &t->v[h[j]], &t->v[h[(j + 1) % hn]])) ok = 0;
            if (ok) { bi = i; bd = d; }
        }
        if (bi < 0) { free(h); continue; }
        /* anel: ... idx[bi], h[hi], h[hi+1], ..., h[hi], idx[bi], ... */
        int ins = hn + 2;
        memmove(idx + bi + 1 + ins, idx + bi + 1, sizeof(int) * (size_t)(m - bi - 1));
        for (int i = 0; i <= hn; i++) idx[bi + 1 + i] = h[(hi + i) % hn];
        idx[bi + 1 + hn + 1] = idx[bi];
        m += ins;
        free(h);
    }
    /* corte de orelhas */
    int edge_cb = t->cb[TESS_EDGE_FLAG - TESS_BEGIN] || t->cb[TESS_EDGE_FLAG_DATA - TESS_BEGIN];
    t_begin(t, GL_TRIANGLES);
    if (edge_cb) t_edge(t, 1);
    int guard = 0;
    while (m > 3 && guard < m * m + 16) {
        int cut = 0;
        for (int i = 0; i < m; i++) {
            int ia = idx[(i + m - 1) % m], ib = idx[i], ic = idx[(i + 1) % m];
            const tv_t *a = &t->v[ia], *b = &t->v[ib], *c = &t->v[ic];
            if (cross3(a, b, c) <= 0) continue;           /* reflexo ou degenerado */
            int ear = 1;
            for (int j = 0; j < m && ear; j++) {
                int p = idx[j];
                if (p == ia || p == ib || p == ic) continue;
                const tv_t *pv = &t->v[p];
                if ((pv->u == a->u && pv->v == a->v) || (pv->u == b->u && pv->v == b->v) || (pv->u == c->u && pv->v == c->v)) continue;
                if (in_tri(pv, a, b, c)) ear = 0;
            }
            if (!ear) continue;
            t_vertex(t, a->data); t_vertex(t, b->data); t_vertex(t, c->data);
            memmove(idx + i, idx + i + 1, sizeof(int) * (size_t)(m - i - 1));
            m--;
            cut = 1;
            break;
        }
        if (!cut) {                                      /* sem orelha (colinear/degenerado): tira um vértice */
            memmove(idx, idx + 1, sizeof(int) * (size_t)(m - 1));
            m--;
        }
        guard++;
    }
    if (m == 3 && cross3(&t->v[idx[0]], &t->v[idx[1]], &t->v[idx[2]]) > 0) {
        t_vertex(t, t->v[idx[0]].data); t_vertex(t, t->v[idx[1]].data); t_vertex(t, t->v[idx[2]].data);
    }
    t_end(t);
    free(idx);
}
/* API antiga (GLU 1.1) */
void gluBeginPolygon(tess_t *t) { gluTessBeginPolygon(t, 0); gluTessBeginContour(t); }
void gluNextContour(tess_t *t, GLenum type) { (void)type; gluTessEndContour(t); gluTessBeginContour(t); }
void gluEndPolygon(tess_t *t) { gluTessEndContour(t); gluTessEndPolygon(t); }
