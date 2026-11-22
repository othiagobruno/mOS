/* mOS — teste de host do tesselador e das matrizes de userland/libsystem/glu_mos.c (prefixo mos_). tools/glu_mos_test.sh */
#include <math.h>
#include <stdio.h>
#include <string.h>

void *mos_gluNewTess(void);
void mos_gluDeleteTess(void *);
void mos_gluTessCallback(void *, unsigned, void (*)(void));
void mos_gluTessProperty(void *, unsigned, double);
void mos_gluTessBeginPolygon(void *, void *);
void mos_gluTessBeginContour(void *);
void mos_gluTessVertex(void *, const double *, void *);
void mos_gluTessEndContour(void *);
void mos_gluTessEndPolygon(void *);
int mos_gluProject(double, double, double, const double *, const double *, const int *, double *, double *, double *);
const unsigned char *mos_gluErrorString(unsigned);
/* o GL de mentira que as quádricas chamam */
static int nverts;
void glBegin(unsigned m) { (void)m; }
void glEnd(void) {}
void glVertex3d(double x, double y, double z) { (void)x; (void)y; (void)z; nverts++; }
void glNormal3d(double x, double y, double z) { (void)x; (void)y; (void)z; }
void glTexCoord2d(double s, double t) { (void)s; (void)t; }
void glMultMatrixd(const double *m) { (void)m; }
void glTranslated(double x, double y, double z) { (void)x; (void)y; (void)z; }
void glFrustum(double l, double r, double b, double t, double n, double f) { (void)l; (void)r; (void)b; (void)t; (void)n; (void)f; }
void *mos_gluNewQuadric(void);
void mos_gluSphere(void *, double, int, int);

static int fails, total;
#define CK(c, m) do { total++; if (!(c)) { fails++; printf("FALHA: %s\n", m); } } while (0)
static unsigned mode; static int ntri, nv, polydata_ok; static double area; static double *tri[3];
static void cb_begin_data(unsigned m, void *pd) { mode = m; if (pd == (void *)0x1234) polydata_ok = 1; }
static void cb_vertex(void *d) {
    tri[nv % 3] = d; nv++;
    if (nv % 3 == 0) { ntri++; area += ((tri[1][0] - tri[0][0]) * (tri[2][1] - tri[0][1]) - (tri[1][1] - tri[0][1]) * (tri[2][0] - tri[0][0])) / 2; }
}
static void cb_end(void) {}

static void poly(void *t, double (*c)[3], int n) { mos_gluTessBeginContour(t); for (int i = 0; i < n; i++) mos_gluTessVertex(t, c[i], c[i]); mos_gluTessEndContour(t); }

int main(void) {
    void *t = mos_gluNewTess();
    mos_gluTessCallback(t, 100106, (void (*)(void))cb_begin_data);
    mos_gluTessCallback(t, 100101, (void (*)(void))cb_vertex);
    mos_gluTessCallback(t, 100102, (void (*)(void))cb_end);
    /* "L" côncavo (área 3) */
    double L[6][3] = {{0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {1, 1, 0}, {1, 2, 0}, {0, 2, 0}};
    mos_gluTessBeginPolygon(t, (void *)0x1234); poly(t, L, 6); mos_gluTessEndPolygon(t);
    CK(mode == 4 && polydata_ok, "BEGIN_DATA com GL_TRIANGLES e o polygon_data");
    CK(ntri == 4 && fabs(area - 3) < 1e-9, "L côncavo: 4 triângulos, área 3, todos anti-horários");
    /* quadrado 4x4 com furo 2x2 (área 12), furo dado no mesmo sentido */
    ntri = nv = 0; area = 0;
    double O[4][3] = {{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {0, 4, 0}}, H[4][3] = {{1, 1, 0}, {3, 1, 0}, {3, 3, 0}, {1, 3, 0}};
    mos_gluTessBeginPolygon(t, 0); poly(t, O, 4); poly(t, H, 4); mos_gluTessEndPolygon(t);
    CK(ntri == 8 && fabs(area - 12) < 1e-9, "quadrado com furo: 8 triângulos, área 12");
    /* polígono no plano x = 5 em sentido horário visto de +x */
    ntri = nv = 0; area = 0;
    double X[4][3] = {{5, 0, 0}, {5, 0, 1}, {5, 1, 1}, {5, 1, 0}};
    mos_gluTessBeginPolygon(t, 0); poly(t, X, 4); mos_gluTessEndPolygon(t);
    CK(ntri == 2, "quadrado no plano x: 2 triângulos");
    /* só a borda */
    ntri = nv = 0;
    mos_gluTessProperty(t, 100141, 1);
    mos_gluTessBeginPolygon(t, 0); poly(t, L, 6); mos_gluTessEndPolygon(t);
    CK(mode == 2 && nv == 6, "BOUNDARY_ONLY: GL_LINE_LOOP com os 6 vértices");
    mos_gluDeleteTess(t);

    double id[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    int vp[4] = {0, 0, 100, 50};
    double wx, wy, wz;
    CK(mos_gluProject(0.5, -0.5, 0, id, id, vp, &wx, &wy, &wz) && fabs(wx - 75) < 1e-9 && fabs(wy - 12.5) < 1e-9 && fabs(wz - 0.5) < 1e-9, "gluProject");
    CK(!strcmp((const char *)mos_gluErrorString(0x501), "invalid value"), "gluErrorString");
    nverts = 0;
    mos_gluSphere(mos_gluNewQuadric(), 1, 8, 4);
    CK(nverts == 4 * 9 * 2, "gluSphere: 4 faixas de 9 pares");
    printf("glu_mos: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
