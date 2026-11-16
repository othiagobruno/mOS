/* mOS — teste de host da matemática do QuartzCore (userland/libsystem/qcmap.h): o mapa de cada layer para a tela
 * (homografia com perspectiva, composição 4x4 como no Core Animation) e as instâncias do CAReplicatorLayer.
 *   gcc -O1 -I userland/libsystem tools/qcmap_host.c -lm -o /tmp/qcmap && /tmp/qcmap */
#include <stdio.h>
#include <math.h>
#include "qcmap.h"
static int pass, total;
#define CHECK(c, ...) do { total++; if (c) pass++; else { printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static int near(double a, double b) { return a - b < 1e-6 && b - a < 1e-6; }
static void pt(map_t m, double x, double y, double *sx, double *sy) { qc_map_point(m, x, y, sx, sy); }

int main(void) {
    map_t id = {0, 1, 0, 1, 0, 0, 0, 0, 0};
    double I4[16], x, y;
    qc_t3_id(I4);
    /* filho: posição (100, 50) no âncora central de bounds 20x10 */
    pt(qc_child(id, I4, 100, 50, -10, -5, 1, 0), 0, 0, &x, &y);
    CHECK(near(x, 90) && near(y, 45), "filho: origem dos bounds vai para posição - âncora: %g %g", x, y);
    /* geometria invertida: y para baixo dentro dos bounds */
    pt(qc_child(id, I4, 0, 0, -10, 5, -1, 0), 0, 0, &x, &y);
    CHECK(near(x, -10) && near(y, 5), "geometria invertida: %g %g", x, y);
    /* transform nunca inicializado (zeros) vale identidade */
    double Z[16] = {0};
    pt(qc_child(id, Z, 3, 4, 0, 0, 1, 0), 1, 1, &x, &y);
    CHECK(near(x, 4) && near(y, 5), "transform zerado = identidade: %g %g", x, y);

    /* replicador: rotação de 90 graus por instância em volta do centro (50, 50) do replicador */
    double rot[16] = {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, pw[16], post[16];
    qc_t3_id(pw);
    double want[5][2] = {{60, 50}, {50, 60}, {40, 50}, {50, 40}, {60, 50}};
    int okr = 1;
    for (int i = 0; i < 5; i++) {
        qc_t3_about(post, pw, 50, 50);
        pt(qc_child(id, I4, 0, 0, 0, 0, 1, post), 60, 50, &x, &y);
        if (!near(x, want[i][0]) || !near(y, want[i][1])) { okr = 0; printf("  instância %d: %g %g\n", i, x, y); }
        qc_t3_mul(pw, pw, rot);
    }
    CHECK(okr, "instâncias giram 90 graus cada em volta do centro");
    /* translação acumulada num pai com escala 2 e deslocamento */
    double tr[16];
    qc_t3_id(tr); tr[12] = 20;
    qc_t3_id(pw);
    for (int i = 0; i < 3; i++) qc_t3_mul(pw, pw, tr);
    qc_t3_about(post, pw, 7, 9);
    map_t par = {10, 2, 5, 2, 0, 0, 0, 0, 0};
    pt(qc_child(par, I4, 0, 0, 0, 0, 1, post), 0, 0, &x, &y);
    CHECK(near(x, 10 + 2 * 60) && near(y, 5), "instância 3 com passo de 20: %g %g", x, y);
    CHECK(near(qc_replica_chan(1, -0.25, 2), 0.5) && near(qc_replica_chan(1, -0.25, 5), 0) && near(qc_replica_chan(0.2, 0.5, 3), 1),
          "instanceColor + i * deslocamento, preso em 0..1");

    /* perspectiva: sublayerTransform m34 = -1/500 no pai e rotateY(60 graus) no filho = a conta 4x4 do Core Animation */
    double S[16];
    qc_t3_id(S); S[11] = -1.0 / 500;
    double c = cos(M_PI / 3), sn = sin(M_PI / 3);
    double RY[16] = {c, 0, -sn, 0, 0, 1, 0, 0, sn, 0, c, 0, 0, 0, 0, 1};
    double Sc[16];
    qc_t3_about(Sc, S, 0, 0);
    map_t ch = qc_child(id, RY, 0, 0, 0, 0, 1, Sc);
    double M[16];
    qc_t3_mul(M, RY, S);
    int okp = 1;
    double pts[3][2] = {{100, 0}, {-100, 0}, {50, 30}};
    for (int i = 0; i < 3; i++) {
        double X = pts[i][0] * M[0] + pts[i][1] * M[4] + M[12], Y = pts[i][0] * M[1] + pts[i][1] * M[5] + M[13], W = pts[i][0] * M[3] + pts[i][1] * M[7] + M[15];
        pt(ch, pts[i][0], pts[i][1], &x, &y);
        if (!near(x, X / W) || !near(y, Y / W)) { okp = 0; printf("  (%g,%g) -> %g %g, esperado %g %g\n", pts[i][0], pts[i][1], x, y, X / W, Y / W); }
    }
    CHECK(okp, "perspectiva igual à conta 4x4");
    double xa, xb, ya, yb;
    pt(ch, 100, 0, &xa, &ya);
    pt(ch, -100, 0, &xb, &yb);
    CHECK(fabs(fabs(xa) - fabs(xb)) > 1 && !qc_is_affine(ch), "a borda que se afasta encolhe (%g vs %g)", xa, xb);
    /* inversa: da tela de volta ao plano do layer */
    double h[9], hi[9];
    qc_to_mat(ch, h);
    CHECK(qc_mat_inv(hi, h), "homografia inversível");
    pt(ch, 37, -12, &x, &y);
    double lw = hi[6] * x + hi[7] * y + hi[8], lx = (hi[0] * x + hi[1] * y + hi[2]) / lw, ly = (hi[3] * x + hi[4] * y + hi[5]) / lw;
    CHECK(near(lx, 37) && near(ly, -12), "inversa volta ao ponto do layer: %g %g", lx, ly);
    /* atrás do observador */
    double far[16];
    qc_t3_id(far); far[3] = -0.02;
    CHECK(!qc_map_point(qc_apply(id, far), 100, 0, &x, &y), "ponto com W <= 0 não é desenhado");
    printf("qcmap: %d/%d ok\n", pass, total);
    return pass != total;
}
