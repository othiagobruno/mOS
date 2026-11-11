/* mOS libSystem — CATransform3DInvert (QuartzCore). Sem dependências: o teste de host
 * (tools/agentes_host.c) liga este arquivo sozinho.
 * Inversa 4x4 por Gauss-Jordan com pivô parcial. Sem inversa (determinante ~0), devolve a matriz
 * original, como a documentação da Apple diz. */
typedef struct { double m[16]; } T3;        /* CATransform3D, linha a linha (o mesmo de quartzcore.c) */

static double fabs_(double x) { return x < 0 ? -x : x; }

T3 CATransform3DInvert(T3 t) {
    double a[4][8];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 8; j++) a[i][j] = j < 4 ? t.m[i * 4 + j] : (j - 4 == i ? 1.0 : 0.0);
    for (int c = 0; c < 4; c++) {
        int p = c;
        for (int r = c + 1; r < 4; r++) if (fabs_(a[r][c]) > fabs_(a[p][c])) p = r;
        if (fabs_(a[p][c]) < 1e-12) return t;
        if (p != c) for (int j = 0; j < 8; j++) { double x = a[c][j]; a[c][j] = a[p][j]; a[p][j] = x; }
        double inv = 1.0 / a[c][c];
        for (int j = 0; j < 8; j++) a[c][j] *= inv;
        for (int r = 0; r < 4; r++) {
            if (r == c || a[r][c] == 0) continue;
            double f = a[r][c];
            for (int j = 0; j < 8; j++) a[r][j] -= f * a[c][j];
        }
    }
    T3 out;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) out.m[i * 4 + j] = a[i][j + 4];
    return out;
}
