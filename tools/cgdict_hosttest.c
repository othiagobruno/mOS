/*
 * Host (Linux): ida e volta, chaves, CFNumber inteiro/float e erros de CG*DictionaryRepresentation.
 *   gcc -std=c11 -Wall -Wextra -O2 -o /tmp/cgdict_host \
 *       tools/cgdict_hosttest.c userland/libsystem/cgdict.c
 *   /tmp/cgdict_host
 * O CF daqui só existe neste harness. No libSystem as mesmas funções chamam o CoreFoundation de cfcollections.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef double CGFloat;
typedef struct { CGFloat x, y; } CGPoint;
typedef struct { CGFloat width, height; } CGSize;
typedef struct { CGPoint origin; CGSize size; } CGRect;
typedef struct obj *cgid;

enum { T_NUM = 1, T_STR = 2, T_DICT = 3, T_OTHER = 4 };
enum { K_SINT64 = 4, K_FLOAT32 = 5, K_FLOAT64 = 6, K_CGFLOAT = 16 };

struct ent { cgid k, v; };
struct obj {
    int type;
    int is_float;
    long long ival;
    double dval;
    char *str;
    struct ent *e;
    int n, cap;
};

const char kCFTypeDictionaryKeyCallBacks;
const char kCFTypeDictionaryValueCallBacks;
static int falhas;

static void check(int cond, const char *msg) {
    if (!cond) { fprintf(stderr, "FALHA: %s\n", msg); falhas++; }
    else printf("ok  %s\n", msg);
}

cgid cx_str(const char *s) {
    struct obj *o = calloc(1, sizeof *o);
    const char *src = s ? s : "";
    o->type = T_STR;
    o->str = malloc(strlen(src) + 1);
    memcpy(o->str, src, strlen(src) + 1);
    return o;
}
static int is_str(cgid o, const char *s) { return o && o->type == T_STR && o->str && strcmp(o->str, s) == 0; }

cgid CFNumberCreate(void *alloc, long type, const void *p) {
    (void)alloc;
    if (!p) return 0;
    struct obj *o = calloc(1, sizeof *o);
    o->type = T_NUM;
    if (type == K_FLOAT32) { o->is_float = 1; o->dval = *(const float *)p; }
    else if (type == K_FLOAT64 || type == K_CGFLOAT) { o->is_float = 1; o->dval = *(const double *)p; }
    else { o->is_float = 0; o->ival = *(const long long *)p; o->dval = (double)o->ival; }
    return o;
}
uint64_t CFGetTypeID(cgid o) {
    if (!o) return 0;
    if (o->type == T_NUM) return 22;
    if (o->type == T_STR) return 7;
    if (o->type == T_DICT) return 18;
    return 1;
}
uint64_t CFNumberGetTypeID(void) { return 22; }
uint8_t CFNumberGetValue(cgid n, long type, void *out) {
    (void)type;
    if (!n || n->type != T_NUM || !out) return 0;
    *(double *)out = n->is_float ? n->dval : (double)n->ival;
    return 1;
}
void CFRelease(cgid o) { (void)o; }

cgid CFDictionaryCreate(void *alloc, const cgid *keys, const cgid *vals, long n, const void *kcb, const void *vcb) {
    (void)alloc; (void)kcb; (void)vcb;
    struct obj *d = calloc(1, sizeof *d);
    d->type = T_DICT;
    for (long i = 0; i < n; i++) {
        if (!keys[i] || !vals[i]) continue;
        if (d->n == d->cap) {
            d->cap = d->cap ? d->cap * 2 : 4;
            d->e = realloc(d->e, (size_t)d->cap * sizeof *d->e);
        }
        d->e[d->n].k = keys[i];
        d->e[d->n].v = vals[i];
        d->n++;
    }
    return d;
}
cgid CFDictionaryGetValue(cgid d, cgid key) {
    if (!d || d->type != T_DICT || !key || key->type != T_STR) return 0;
    for (int i = 0; i < d->n; i++) {
        if (d->e[i].k && d->e[i].k->type == T_STR && strcmp(d->e[i].k->str, key->str) == 0) return d->e[i].v;
    }
    return 0;
}

cgid CGPointCreateDictionaryRepresentation(CGPoint point);
cgid CGSizeCreateDictionaryRepresentation(CGSize size);
cgid CGRectCreateDictionaryRepresentation(CGRect rect);
bool CGPointMakeWithDictionaryRepresentation(cgid dict, CGPoint *point);
bool CGSizeMakeWithDictionaryRepresentation(cgid dict, CGSize *size);
bool CGRectMakeWithDictionaryRepresentation(cgid dict, CGRect *rect);

static cgid num_i(long long v) { return CFNumberCreate(0, K_SINT64, &v); }
static cgid num_f(float v) { return CFNumberCreate(0, K_FLOAT32, &v); }
static cgid dict_put(cgid *ks, cgid *vs, int n) {
    return CFDictionaryCreate(0, ks, vs, n, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

int main(void) {
    CGPoint p = {1.5, -2.25};
    cgid dp = CGPointCreateDictionaryRepresentation(p);
    check(dp != 0, "CGPointCreate devolve dicionário");
    CGPoint back = {9, 9};
    check(CGPointMakeWithDictionaryRepresentation(dp, &back) && back.x == 1.5 && back.y == -2.25, "CGPoint ida e volta");
    check(is_str(dp->e[0].k, "X") && is_str(dp->e[1].k, "Y"), "chaves X e Y");
    check(dp->e[0].v->type == T_NUM && dp->e[0].v->is_float && dp->e[0].v->dval == 1.5, "X é CFNumber double");
    check(dp->e[1].v->dval == -2.25, "Y é CFNumber double");

    CGSize s = {3.5, 4};
    cgid ds = CGSizeCreateDictionaryRepresentation(s);
    CGSize sb = {8, 8};
    check(CGSizeMakeWithDictionaryRepresentation(ds, &sb) && sb.width == 3.5 && sb.height == 4, "CGSize ida e volta");
    check(is_str(ds->e[0].k, "Width") && is_str(ds->e[1].k, "Height"), "chaves Width e Height");

    CGRect r = {{10, 20}, {30.5, 40.5}};
    cgid dr = CGRectCreateDictionaryRepresentation(r);
    CGRect rb;
    memset(&rb, 0x5a, sizeof rb);
    check(CGRectMakeWithDictionaryRepresentation(dr, &rb) && rb.origin.x == 10 && rb.origin.y == 20
          && rb.size.width == 30.5 && rb.size.height == 40.5, "CGRect ida e volta");
    check(dr->n == 4, "CGRect tem quatro pares");

    cgid ik[2] = { cx_str("X"), cx_str("Y") };
    cgid iv[2] = { num_i(7), num_i(-3) };
    CGPoint ip = {1, 1};
    check(CGPointMakeWithDictionaryRepresentation(dict_put(ik, iv, 2), &ip) && ip.x == 7 && ip.y == -3,
          "CFNumber inteiro");
    float fx = 1.25f, fy = 2.5f;
    cgid fv[2] = { num_f(fx), num_f(fy) };
    check(CGPointMakeWithDictionaryRepresentation(dict_put(ik, fv, 2), &ip) && ip.x == 1.25 && ip.y == 2.5,
          "CFNumber float");

    CGPoint junk;
    memset(&junk, 0x5a, sizeof junk);
    check(!CGPointMakeWithDictionaryRepresentation(0, &junk) && junk.x == 0 && junk.y == 0, "dict NULL zera a saída");
    cgid onlyx[1] = { cx_str("X") };
    cgid onlyv[1] = { num_i(1) };
    memset(&junk, 0x5a, sizeof junk);
    check(!CGPointMakeWithDictionaryRepresentation(dict_put(onlyx, onlyv, 1), &junk) && junk.x == 0 && junk.y == 0,
          "falta chave zera a saída");
    cgid badk[2] = { cx_str("X"), cx_str("Y") };
    cgid badv[2] = { cx_str("nope"), num_i(1) };
    memset(&junk, 0x5a, sizeof junk);
    check(!CGPointMakeWithDictionaryRepresentation(dict_put(badk, badv, 2), &junk) && junk.x == 0 && junk.y == 0,
          "valor que não é CFNumber zera a saída");

    CGSize zs;
    memset(&zs, 0x5a, sizeof zs);
    check(!CGSizeMakeWithDictionaryRepresentation(0, &zs) && zs.width == 0 && zs.height == 0, "CGSize dict NULL");
    CGRect zr;
    memset(&zr, 0x5a, sizeof zr);
    cgid rk[3] = { cx_str("X"), cx_str("Y"), cx_str("Width") };
    cgid rv[3] = { num_i(1), num_i(2), num_i(3) };
    check(!CGRectMakeWithDictionaryRepresentation(dict_put(rk, rv, 3), &zr) && zr.origin.x == 0 && zr.size.height == 0,
          "CGRect sem Height");
    struct obj outro = { .type = T_OTHER };
    cgid okv[2] = { num_i(1), &outro };
    memset(&junk, 0x5a, sizeof junk);
    cgid yok[2] = { cx_str("X"), cx_str("Y") };
    check(!CGPointMakeWithDictionaryRepresentation(dict_put(yok, okv, 2), &junk) && junk.y == 0, "tipo estranho não é CFNumber");

    check(CGRectMakeWithDictionaryRepresentation(dr, 0) == true, "ponteiro de saída NULL com dict válido");
    check(CGPointMakeWithDictionaryRepresentation(0, 0) == false, "dict NULL e saída NULL");

    if (falhas) { fprintf(stderr, "cgdict_host: %d falhas\n", falhas); return 1; }
    printf("cgdict_host: ok\n");
    return 0;
}
