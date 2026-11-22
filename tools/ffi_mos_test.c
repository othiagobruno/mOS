/*
 * mOS — teste de host de userland/libsystem/ffi_mos.c + ffi_arm64.S (prefixo mos_). Rodar: tools/ffi_mos_test.sh.
 * No macOS arm64 o host tem o mesmo PCS da Apple que o libSystem arm64 do mOS: as chamadas vão para funções de
 * verdade compiladas pelo clang (inteiros estreitos, HFA, struct pequena e grande, retorno indireto, pilha,
 * variádica) e os closures são chamados pelo C, inclusive como comparador do qsort da libc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

typedef struct ffi_type { size_t size; unsigned short alignment, type; struct ffi_type **elements; } ffi_type;
typedef struct { int abi; unsigned nargs; ffi_type **arg_types; ffi_type *rtype; unsigned bytes; unsigned flags; unsigned extra[4]; } ffi_cif;
extern ffi_type mos_ffi_type_sint32, mos_ffi_type_sint8, mos_ffi_type_uint8, mos_ffi_type_double, mos_ffi_type_float, mos_ffi_type_pointer,
    mos_ffi_type_sint64, mos_ffi_type_void, mos_ffi_type_uint16, mos_ffi_type_sint16;
int mos_ffi_prep_cif(ffi_cif *, int, unsigned, ffi_type *, ffi_type **);
int mos_ffi_prep_cif_var(ffi_cif *, int, unsigned, unsigned, ffi_type *, ffi_type **);
void mos_ffi_call(ffi_cif *, void (*)(void), void *, void **);
void *mos_ffi_closure_alloc(size_t, void **);
int mos_ffi_prep_closure_loc(void *, ffi_cif *, void (*)(ffi_cif *, void *, void **, void *), void *, void *);
void mos_ffi_closure_free(void *);
#define ABI 1

static int fails, total;
#define CK(c, m) do { total++; if (!(c)) { fails++; printf("FALHA: %s\n", m); } } while (0)

static int add3(int a, int b, int c) { return a + b * 10 + c * 100; }
static double mix(int8_t a, double b, float c, uint16_t d) { return a + b + c + d; }
static int8_t neg8(void) { return -5; }
typedef struct { double x, y; } P2;
static P2 p2scale(P2 p, double k) { P2 r = {p.x * k, p.y * k}; return r; }
typedef struct { float a, b, c, d; } F4;
static float f4sum(F4 v) { return v.a + v.b + v.c + v.d; }
typedef struct { char c; int i; } CI;
static CI ci_bump(CI v) { v.c++; v.i *= 2; return v; }
typedef struct { long a, b, c; } BIG;
static BIG big_mk(BIG v, long k) { BIG r = {v.a + k, v.b + k, v.c + k}; return r; }
static long many(long a, long b, long c, long d, long e, long f, long g, long h, long i, int8_t j, double x1, double x2, double x3,
                 double x4, double x5, double x6, double x7, double x8, double x9, float x10) {
    return a + b + c + d + e + f + g + h + i * 2 + j * 3 + (long)(x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8 + x9 * 10 + x10 * 100);
}

static void cb_add(ffi_cif *cif, void *ret, void **args, void *ud) { (void)cif; *(int64_t *)ret = *(int *)args[0] + *(int *)args[1] + *(int *)ud; }
static void cb_dbl(ffi_cif *cif, void *ret, void **args, void *ud) { (void)cif; (void)ud; *(double *)ret = *(double *)args[0] * *(float *)args[1]; }
static void cb_cmp(ffi_cif *cif, void *ret, void **args, void *ud) {
    (void)cif; (void)ud;
    int a = **(int **)args[0], b = **(int **)args[1];
    *(int64_t *)ret = (a > b) - (a < b);
}
static void cb_p2(ffi_cif *cif, void *ret, void **args, void *ud) { (void)cif; (void)ud; P2 p = *(P2 *)args[0]; P2 r = {p.y, p.x}; *(P2 *)ret = r; }

int main(void) {
    ffi_cif cif;
    ffi_type *t3[3] = {&mos_ffi_type_sint32, &mos_ffi_type_sint32, &mos_ffi_type_sint32};
    int a = 1, b = 2, c = 3;
    void *av[20] = {&a, &b, &c};
    int64_t ri = 0;
    CK(mos_ffi_prep_cif(&cif, ABI, 3, &mos_ffi_type_sint32, t3) == 0, "prep_cif");
    mos_ffi_call(&cif, (void (*)(void))add3, &ri, av);
    CK(ri == 321, "int(int,int,int)");
    ffi_type *tm[4] = {&mos_ffi_type_sint8, &mos_ffi_type_double, &mos_ffi_type_float, &mos_ffi_type_uint16};
    int8_t i8 = -3; double d = 1.5; float f = 0.25f; uint16_t u16 = 40000;
    void *am[4] = {&i8, &d, &f, &u16};
    double rd = 0;
    mos_ffi_prep_cif(&cif, ABI, 4, &mos_ffi_type_double, tm);
    mos_ffi_call(&cif, (void (*)(void))mix, &rd, am);
    CK(rd == -3 + 1.5 + 0.25 + 40000, "double(int8, double, float, uint16)");
    mos_ffi_prep_cif(&cif, ABI, 0, &mos_ffi_type_sint8, 0);
    ri = 0;
    mos_ffi_call(&cif, (void (*)(void))neg8, &ri, 0);
    CK(ri == -5, "retorno int8 estendido com sinal");
    ffi_type *p2el[3] = {&mos_ffi_type_double, &mos_ffi_type_double, 0};
    ffi_type p2t = {0, 0, 13, p2el};
    ffi_type *tp[2] = {&p2t, &mos_ffi_type_double};
    P2 p = {1, 2}, rp = {0, 0};
    double k = 3;
    void *ap[2] = {&p, &k};
    CK(mos_ffi_prep_cif(&cif, ABI, 2, &p2t, tp) == 0 && p2t.size == 16 && p2t.alignment == 8, "struct {double,double} tamanho");
    mos_ffi_call(&cif, (void (*)(void))p2scale, &rp, ap);
    CK(rp.x == 3 && rp.y == 6, "HFA de 2 doubles: argumento e retorno");
    ffi_type *f4el[5] = {&mos_ffi_type_float, &mos_ffi_type_float, &mos_ffi_type_float, &mos_ffi_type_float, 0};
    ffi_type f4t = {0, 0, 13, f4el};
    ffi_type *tf4[1] = {&f4t};
    F4 v4 = {1, 2, 3, 4.5f};
    void *af4[1] = {&v4};
    float rf = 0;
    mos_ffi_prep_cif(&cif, ABI, 1, &mos_ffi_type_float, tf4);
    mos_ffi_call(&cif, (void (*)(void))f4sum, &rf, af4);
    CK(rf == 10.5f, "HFA de 4 floats");
    ffi_type *ciel[3] = {&mos_ffi_type_sint8, &mos_ffi_type_sint32, 0};
    ffi_type cit = {0, 0, 13, ciel};
    ffi_type *tci[1] = {&cit};
    CI vci = {'a', 21}, rci = {0, 0};
    void *aci[1] = {&vci};
    mos_ffi_prep_cif(&cif, ABI, 1, &cit, tci);
    mos_ffi_call(&cif, (void (*)(void))ci_bump, &rci, aci);
    CI exp_ci = ci_bump(vci);
    CK(rci.c == exp_ci.c && rci.i == exp_ci.i, "struct {char,int} em x");
    ffi_type *bel[4] = {&mos_ffi_type_sint64, &mos_ffi_type_sint64, &mos_ffi_type_sint64, 0};
    ffi_type bt = {0, 0, 13, bel};
    ffi_type *tb[2] = {&bt, &mos_ffi_type_sint64};
    BIG vb = {10, 20, 30}, rb = {0, 0, 0};
    long kb = 5;
    void *ab[2] = {&vb, &kb};
    mos_ffi_prep_cif(&cif, ABI, 2, &bt, tb);
    mos_ffi_call(&cif, (void (*)(void))big_mk, &rb, ab);
    CK(rb.a == 15 && rb.b == 25 && rb.c == 35 && vb.a == 10, "struct de 24 bytes: por cópia e retorno em x8");
    ffi_type *tmany[20];
    long L[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    int8_t j8 = -1;
    double X[9] = {1, 1, 1, 1, 1, 1, 1, 1, 2};
    float x10 = 2;
    void *amany[20];
    for (int i = 0; i < 9; i++) { tmany[i] = &mos_ffi_type_sint64; amany[i] = &L[i]; }
    tmany[9] = &mos_ffi_type_sint8; amany[9] = &j8;
    for (int i = 0; i < 9; i++) { tmany[10 + i] = &mos_ffi_type_double; amany[10 + i] = &X[i]; }
    tmany[19] = &mos_ffi_type_float; amany[19] = &x10;
    long rl = 0;
    mos_ffi_prep_cif(&cif, ABI, 20, &mos_ffi_type_sint64, tmany);
    mos_ffi_call(&cif, (void (*)(void))many, &rl, amany);
    CK(rl == many(1, 2, 3, 4, 5, 6, 7, 8, 9, -1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2), "20 argumentos: x/v cheios e o resto na pilha");
    /* variádica: snprintf(buf, n, fmt, int, double, char *) */
    char buf[64];
    size_t n = sizeof buf;
    const char *fmt = "%d|%.2f|%s", *str = "oi";
    char *pb = buf;
    int vi = 42;
    double vd = 3.14159;
    ffi_type *tv[6] = {&mos_ffi_type_pointer, &mos_ffi_type_sint64, &mos_ffi_type_pointer, &mos_ffi_type_sint32, &mos_ffi_type_double, &mos_ffi_type_pointer};
    void *avv[6] = {&pb, &n, &fmt, &vi, &vd, &str};
    ri = 0;
    CK(mos_ffi_prep_cif_var(&cif, ABI, 3, 6, &mos_ffi_type_sint32, tv) == 0, "prep_cif_var");
    mos_ffi_call(&cif, (void (*)(void))snprintf, &ri, avv);
    CK(ri == 10 && !strcmp(buf, "42|3.14|oi"), "variádica (snprintf)");
    /* closures */
    void *code = 0;
    void *cl = mos_ffi_closure_alloc(64, &code);
    int bias = 1000;
    ffi_type *tcl[2] = {&mos_ffi_type_sint32, &mos_ffi_type_sint32};
    ffi_cif ccif;
    mos_ffi_prep_cif(&ccif, ABI, 2, &mos_ffi_type_sint32, tcl);
    CK(cl && code && mos_ffi_prep_closure_loc(cl, &ccif, cb_add, &bias, code) == 0, "closure alloc/prep");
    int (*fa)(int, int) = (int (*)(int, int))code;
    CK(fa(3, 4) == 1007, "closure int(int,int)");
    void *code2 = 0, *cl2 = mos_ffi_closure_alloc(64, &code2);
    ffi_type *tcd[2] = {&mos_ffi_type_double, &mos_ffi_type_float};
    ffi_cif dcif;
    mos_ffi_prep_cif(&dcif, ABI, 2, &mos_ffi_type_double, tcd);
    mos_ffi_prep_closure_loc(cl2, &dcif, cb_dbl, 0, code2);
    double (*fd)(double, float) = (double (*)(double, float))code2;
    CK(fd(2.5, 4.0f) == 10.0, "closure double(double,float)");
    void *code3 = 0, *cl3 = mos_ffi_closure_alloc(64, &code3);
    ffi_type *tq[2] = {&mos_ffi_type_pointer, &mos_ffi_type_pointer};
    ffi_cif qcif;
    mos_ffi_prep_cif(&qcif, ABI, 2, &mos_ffi_type_sint32, tq);
    mos_ffi_prep_closure_loc(cl3, &qcif, cb_cmp, 0, code3);
    int arr[6] = {5, 1, 4, 2, 6, 3};
    qsort(arr, 6, sizeof(int), (int (*)(const void *, const void *))code3);
    CK(arr[0] == 1 && arr[1] == 2 && arr[5] == 6, "closure como comparador do qsort");
    void *code4 = 0, *cl4 = mos_ffi_closure_alloc(64, &code4);
    ffi_type *tp1[1] = {&p2t};
    ffi_cif pcif;
    mos_ffi_prep_cif(&pcif, ABI, 1, &p2t, tp1);
    mos_ffi_prep_closure_loc(cl4, &pcif, cb_p2, 0, code4);
    P2 (*fp)(P2) = (P2 (*)(P2))code4;
    P2 sw = fp((P2){7, 8});
    CK(sw.x == 8 && sw.y == 7, "closure com HFA de argumento e retorno");
    mos_ffi_closure_free(cl); mos_ffi_closure_free(cl2); mos_ffi_closure_free(cl3); mos_ffi_closure_free(cl4);
    printf("ffi-mos: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
