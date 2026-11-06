/*
 * mOS libSystem — funções de math específicas da Apple que o compilador gera sozinho para alvos Darwin:
 * com sin(x) e cos(x) do mesmo x, o clang/swiftc (-O) chamam __sincos_stret, que devolve os dois de uma vez
 * (struct {sin, cos} em xmm0/xmm1; a versão float devolve os dois floats empacotados em xmm0).
 */
double sin(double), cos(double);

typedef struct { double s, c; } sincos_d;
typedef struct { float s, c; } sincos_f;

sincos_d __sincos_stret(double x) { sincos_d r = {sin(x), cos(x)}; return r; }
sincos_f __sincosf_stret(float x) { sincos_f r = {(float)sin(x), (float)cos(x)}; return r; }
/* __sincospi_stret: sin(πx) e cos(πx) */
sincos_d __sincospi_stret(double x) { const double pi = 3.14159265358979323846; sincos_d r = {sin(pi * x), cos(pi * x)}; return r; }
sincos_f __sincospif_stret(float x) { const double pi = 3.14159265358979323846; sincos_f r = {(float)sin(pi * x), (float)cos(pi * x)}; return r; }
