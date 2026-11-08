/*
 * mOS libSystem — interpretador da árvore de MSL (mtlshader.c) para o Metal por software.
 *
 * Valores numéricos passam por registradores de até 16 componentes (double, ou i64 para inteiros,
 * que são reembrulhados na largura do tipo a cada operação); structs, arrays, ponteiros, texturas e
 * samplers são bytes no layout de memória do Metal. Cada chamada de função tem um quadro (alloca)
 * com as variáveis locais nas posições calculadas pelo compilador.
 *
 * threadgroup_barrier: um kernel que usa barreira roda cada thread do threadgroup numa fibra com
 * pilha própria; na barreira a fibra devolve o controle e o escalonador só passa da barreira quando
 * todas as threads chegaram nela.
 */
#include "mtlint.h"

double sin(double), cos(double), tan(double), asin(double), acos(double), atan(double), atan2(double, double);
double sinh(double), cosh(double), tanh(double), asinh(double), acosh(double), atanh(double);
double exp(double), exp2(double), exp10(double), log(double), log2(double), log10(double), pow(double, double);
double sqrt(double), floor(double), ceil(double), fmod(double, double), round(double), rint(double), trunc(double);
double copysign(double, double);
ty_t *msl_tscal(int k);
ty_t *msl_tvec(int k, int n, int packed);

typedef struct { int n, isint; union { double d[16]; i64 i[16]; }; } reg_t;
typedef struct { u8 *base; msl_ctx_t *ctx; msl_lib_t *lib; int depth; msl_fn_t *fn; } fr_t;
enum { ST_OK = 0, ST_BREAK, ST_CONT, ST_RET };
static msl_ctx_t g_dummy_ctx;

/* ================================================================ half */
u16 msl_f2h(float f) {
    union { float f; u32 u; } v = {f};
    u32 x = v.u, s = (x >> 16) & 0x8000;
    i32 e = (i32)((x >> 23) & 0xff) - 127 + 15;
    u32 m = x & 0x7fffff;
    if (((x >> 23) & 0xff) == 0xff) return (u16)(s | 0x7c00 | (m ? 0x200 : 0));
    if (e >= 31) return (u16)(s | 0x7c00);
    if (e <= 0) {
        if (e < -10) return (u16)s;
        m |= 0x800000;
        u32 sh = (u32)(14 - e);
        u32 h = m >> sh;
        if ((m >> (sh - 1)) & 1) h++;
        return (u16)(s | h);
    }
    u32 h = s | (u32)e << 10 | m >> 13;
    if (m & 0x1000) h++;
    return (u16)h;
}
float msl_h2f(u16 h) {
    u32 s = (u32)(h & 0x8000) << 16, e = (h >> 10) & 0x1f, m = h & 0x3ff, x;
    if (!e) {
        if (!m) x = s;
        else { e = 127 - 15 + 1; while (!(m & 0x400)) { m <<= 1; e--; } m &= 0x3ff; x = s | e << 23 | m << 13; }
    } else if (e == 31) x = s | 0x7f800000 | m << 13;
    else x = s | (e - 15 + 127) << 23 | m << 13;
    union { u32 u; float f; } v = {x};
    return v.f;
}

/* ================================================================ registradores */
static i64 wrapi(int k, i64 v) {
    switch (k) {
    case K_BOOL: return v != 0;
    case K_CHAR: return (i8)v;
    case K_UCHAR: return (u8)v;
    case K_SHORT: return (i16)v;
    case K_USHORT: return (u16)v;
    case K_INT: return (i32)v;
    case K_UINT: return (u32)v;
    default: return v;
    }
}
static i64 d2i(double d) {
    if (d != d) return 0;
    if (d >= 9.2e18) return 0x7fffffffffffffffll;
    if (d <= -9.2e18) return (i64)0x8000000000000000ull;
    return (i64)d;
}
static double rnd(int k, double d) { return k == K_FLOAT || k == K_HALF ? (double)(float)d : d; }
static void conv(reg_t *r, int k) {
    int want = IS_INTK(k);
    if (want && !r->isint) for (int i = 0; i < r->n; i++) r->i[i] = wrapi(k, k == K_BOOL ? r->d[i] != 0 : (k == K_UINT || k == K_ULONG || k == K_USHORT || k == K_UCHAR) && r->d[i] < 0 ? (i64)(u64)d2i(r->d[i]) : d2i(r->d[i]));
    else if (want) for (int i = 0; i < r->n; i++) r->i[i] = wrapi(k, r->i[i]);
    else if (r->isint) for (int i = 0; i < r->n; i++) r->d[i] = (double)r->i[i];
    else for (int i = 0; i < r->n; i++) r->d[i] = rnd(k, r->d[i]);
    r->isint = want;
}
static void bcast(reg_t *r, int n) {
    if (r->n == 1 && n > 1) { for (int i = 1; i < n; i++) r->i[i] = r->i[0]; r->n = n; }
}
static double rd(const reg_t *r, int i) { int j = r->n == 1 ? 0 : i; return r->isint ? (double)r->i[j] : r->d[j]; }
static i64 ri(const reg_t *r, int i) { int j = r->n == 1 ? 0 : i; return r->isint ? r->i[j] : d2i(r->d[j]); }
static int truth(const reg_t *r) { for (int i = 0; i < r->n; i++) if (r->isint ? r->i[i] != 0 : r->d[i] != 0) return 1; return 0; }

static void ld_comp(int k, const u8 *p, reg_t *r, int i) {
    switch (k) {
    case K_BOOL: r->i[i] = *p != 0; break;
    case K_CHAR: r->i[i] = *(const i8 *)p; break;
    case K_UCHAR: r->i[i] = *p; break;
    case K_SHORT: r->i[i] = *(const i16 *)p; break;
    case K_USHORT: r->i[i] = *(const u16 *)p; break;
    case K_INT: r->i[i] = *(const i32 *)p; break;
    case K_UINT: r->i[i] = *(const u32 *)p; break;
    case K_LONG: case K_ULONG: r->i[i] = *(const i64 *)p; break;
    case K_HALF: r->d[i] = msl_h2f(*(const u16 *)p); break;
    default: r->d[i] = *(const float *)p; break;
    }
}
static void st_comp(int k, u8 *p, const reg_t *r, int i) {
    switch (k) {
    case K_BOOL: *p = (u8)(ri(r, i) != 0 || (!r->isint && rd(r, i) != 0)); break;
    case K_CHAR: case K_UCHAR: *p = (u8)ri(r, i); break;
    case K_SHORT: case K_USHORT: *(u16 *)p = (u16)ri(r, i); break;
    case K_INT: case K_UINT: *(u32 *)p = (u32)ri(r, i); break;
    case K_LONG: case K_ULONG: *(i64 *)p = ri(r, i); break;
    case K_HALF: *(u16 *)p = msl_f2h((float)rd(r, i)); break;
    default: *(float *)p = (float)rd(r, i); break;
    }
}
static void load(const ty_t *t, const u8 *p, reg_t *r) {
    int k = ty_scal(t), ss = scal_size(k);
    r->isint = IS_INTK(k);
    if (t->k == K_MAT) {
        int rows = t->e->n;
        r->n = t->n * rows;
        for (int c = 0; c < t->n; c++)
            for (int j = 0; j < rows; j++) ld_comp(k, p + c * t->e->size + j * ss, r, c * rows + j);
        return;
    }
    r->n = t->k == K_VEC ? t->n : 1;
    for (int i = 0; i < r->n; i++) ld_comp(k, p + i * ss, r, i);
}
static void store(const ty_t *t, const reg_t *r, u8 *p) {
    int k = ty_scal(t), ss = scal_size(k);
    if (t->k == K_MAT) {
        int rows = t->e->n;
        if (r->n == 1) {          /* escalar -> diagonal */
            memset(p, 0, t->size);
            for (int c = 0; c < t->n && c < rows; c++) st_comp(k, p + c * t->e->size + c * ss, r, 0);
            return;
        }
        for (int c = 0; c < t->n; c++)
            for (int j = 0; j < rows; j++) st_comp(k, p + c * t->e->size + j * ss, r, c * rows + j);
        return;
    }
    int n = t->k == K_VEC ? t->n : 1;
    for (int i = 0; i < n; i++) st_comp(k, p + i * ss, r, i < r->n ? i : r->n - 1);
}

/* ================================================================ avaliação */
static void ev(node_t *n, fr_t *fr, reg_t *r);
static void evm(node_t *n, fr_t *fr, u8 *dst);
static u8 *lv(node_t *n, fr_t *fr);
static int exec(node_t *s, fr_t *fr);
static void call_fn(msl_fn_t *f, node_t *c, fr_t *fr, u8 *dst);

static int is_lv(node_t *n) {
    return n->op == E_VAR || n->op == E_DEREF || (n->op == E_MEMBER && is_lv(n->a)) ||
           (n->op == E_INDEX && (n->a->t->k == K_PTR || is_lv(n->a)));
}
static u8 *var_addr(var_t *v, fr_t *fr) {
    u8 *p = v->where == 1 ? fr->lib->gmem + v->off : v->where == 2 ? fr->ctx->tgstatic + v->off : fr->base + v->off;
    if (v->ref) p = *(u8 **)p;
    return p;
}
static int stride_of(ty_t *t) { return t->k == K_VEC ? scal_size(t->e->k) : t->e->size; }
static i64 clampi(i64 i, int n) { return n <= 0 ? i : i < 0 ? 0 : i >= n ? n - 1 : i; }
/* endereço de um lvalue, ou de uma cópia temporária (tmp) para rvalues */
static int chain_size(node_t *n) {
    int s = n->t ? n->t->size : 0;
    while ((n->op == E_MEMBER || n->op == E_INDEX || n->op == E_SWZ) && n->a) { n = n->a; if (n->t && n->t->size > s) s = n->t->size; }
    return s < 16 ? 16 : s;
}
static u8 *addr(node_t *n, fr_t *fr, u8 *tmp) {
    switch (n->op) {
    case E_VAR: return var_addr(n->var, fr);
    case E_MEMBER: return addr(n->a, fr, tmp) + n->off;
    case E_DEREF: { u8 *p; evm(n->a, fr, (u8 *)&p); return p; }
    case E_INDEX: {
        reg_t ir;
        ev(n->b, fr, &ir);
        i64 i = ri(&ir, 0);
        ty_t *bt = n->a->t;
        if (bt->k == K_PTR) { u8 *p; evm(n->a, fr, (u8 *)&p); return p + i * (i64)(bt->e->size ? bt->e->size : 1); }
        u8 *b = addr(n->a, fr, tmp);
        int cnt = bt->k == K_ARRAY ? bt->n : bt->k == K_VEC ? bt->n : bt->k == K_MAT ? bt->n : 0;
        return b + clampi(i, cnt) * stride_of(bt);
    }
    default: evm(n, fr, tmp); return tmp;
    }
}
static u8 *lv(node_t *n, fr_t *fr) { return addr(n, fr, 0); }

/* ---- operações binárias */
static void matmul(const reg_t *a, const ty_t *ta, const reg_t *b, const ty_t *tb, reg_t *r) {
    r->isint = 0;
    if (ta->k == K_MAT && tb->k == K_MAT) {
        int ra = ta->e->n, ca = ta->n, cb = tb->n, rb = tb->e->n;
        (void)rb;
        r->n = cb * ra;
        for (int j = 0; j < cb; j++)
            for (int i = 0; i < ra; i++) {
                double s = 0;
                for (int k = 0; k < ca; k++) s += rd(a, k * ra + i) * rd(b, j * tb->e->n + k);
                r->d[j * ra + i] = s;
            }
    } else if (ta->k == K_MAT) {
        int ra = ta->e->n, ca = ta->n;
        r->n = ra;
        for (int i = 0; i < ra; i++) {
            double s = 0;
            for (int c = 0; c < ca; c++) s += rd(a, c * ra + i) * rd(b, c);
            r->d[i] = s;
        }
    } else {
        int rb = tb->e->n, cb = tb->n;
        r->n = cb;
        for (int j = 0; j < cb; j++) {
            double s = 0;
            for (int i = 0; i < rb; i++) s += rd(a, i) * rd(b, j * rb + i);
            r->d[j] = s;
        }
    }
}
static void binop(int op, reg_t *a, const ty_t *ta, reg_t *b, const ty_t *tb, const ty_t *rt, reg_t *r) {
    if (op == '*' && (ta->k == K_MAT || tb->k == K_MAT) && !(IS_SCALAR(ta) || IS_SCALAR(tb))) {
        matmul(a, ta, b, tb, r);
        int k = ty_scal(rt);
        for (int i = 0; i < r->n; i++) r->d[i] = rnd(k, r->d[i]);
        return;
    }
    int n = a->n > b->n ? a->n : b->n;
    int rk = ty_scal(rt);
    int cmp = op == 'e' || op == 'n' || op == '<' || op == '>' || op == 'l' || op == 'g' || op == 'O' || op == 'A';
    int intdom = a->isint && b->isint;
    r->n = n;
    if (cmp) {
        r->isint = 1;
        for (int i = 0; i < n; i++) {
            int v;
            if (op == 'O') v = (intdom ? ri(a, i) != 0 : rd(a, i) != 0) || (b->isint ? ri(b, i) != 0 : rd(b, i) != 0);
            else if (op == 'A') v = (a->isint ? ri(a, i) != 0 : rd(a, i) != 0) && (b->isint ? ri(b, i) != 0 : rd(b, i) != 0);
            else if (intdom) {
                i64 x = ri(a, i), y = ri(b, i);
                v = op == 'e' ? x == y : op == 'n' ? x != y : op == '<' ? x < y : op == '>' ? x > y : op == 'l' ? x <= y : x >= y;
            } else {
                double x = rd(a, i), y = rd(b, i);
                v = op == 'e' ? x == y : op == 'n' ? x != y : op == '<' ? x < y : op == '>' ? x > y : op == 'l' ? x <= y : x >= y;
            }
            r->i[i] = v;
        }
        return;
    }
    if (IS_INTK(rk)) {
        r->isint = 1;
        int uns = IS_UNSK(rk);
        int bits = scal_size(rk) * 8;
        for (int i = 0; i < n; i++) {
            u64 x = (u64)ri(a, i), y = (u64)ri(b, i);
            i64 v;
            switch (op) {
            case '+': v = (i64)(x + y); break;
            case '-': v = (i64)(x - y); break;
            case '*': v = (i64)(x * y); break;
            case '/':
                if (uns) { u64 m = bits == 64 ? ~0ull : (1ull << bits) - 1; v = (y & m) ? (i64)((x & m) / (y & m)) : 0; }
                else v = (i64)y ? (i64)x / (i64)y : 0;
                break;
            case '%':
                if (uns) { u64 m = bits == 64 ? ~0ull : (1ull << bits) - 1; v = (y & m) ? (i64)((x & m) % (y & m)) : 0; }
                else v = (i64)y ? (i64)x % (i64)y : 0;
                break;
            case '&': v = (i64)(x & y); break;
            case '|': v = (i64)(x | y); break;
            case '^': v = (i64)(x ^ y); break;
            case 'L': v = (i64)(x << (y & (bits - 1))); break;
            case 'R': {
                u64 m = bits == 64 ? ~0ull : (1ull << bits) - 1;
                v = uns ? (i64)((x & m) >> (y & (bits - 1))) : (i64)wrapi(rk, (i64)x) >> (y & (bits - 1));
                break;
            }
            default: v = 0;
            }
            r->i[i] = wrapi(rk, v);
        }
        return;
    }
    r->isint = 0;
    for (int i = 0; i < n; i++) {
        double x = rd(a, i), y = rd(b, i), v;
        switch (op) {
        case '+': v = x + y; break;
        case '-': v = x - y; break;
        case '*': v = x * y; break;
        case '/': v = x / y; break;
        default: v = 0;
        }
        r->d[i] = rnd(rk, v);
    }
}

/* ---- funções embutidas */
static double fsign(double x) { return x > 0 ? 1 : x < 0 ? -1 : 0; }
static double fclampd(double x, double a, double b) { return x < a ? a : x > b ? b : x; }
static double un_f(int b, double x) {
    switch (b) {
    case B_SIN: return sin(x);
    case B_COS: return cos(x);
    case B_TAN: return tan(x);
    case B_ASIN: return asin(x);
    case B_ACOS: return acos(x);
    case B_ATAN: return atan(x);
    case B_SINH: return sinh(x);
    case B_COSH: return cosh(x);
    case B_TANH: return tanh(x);
    case B_ASINH: return asinh(x);
    case B_ACOSH: return acosh(x);
    case B_ATANH: return atanh(x);
    case B_EXP: return exp(x);
    case B_EXP2: return exp2(x);
    case B_EXP10: return exp10(x);
    case B_LOG: return log(x);
    case B_LOG2: return log2(x);
    case B_LOG10: return log10(x);
    case B_SQRT: return sqrt(x);
    case B_RSQRT: return 1.0 / sqrt(x);
    case B_FLOOR: return floor(x);
    case B_CEIL: return ceil(x);
    case B_ROUND: return round(x);
    case B_RINT: return rint(x);
    case B_TRUNC: return trunc(x);
    case B_FRACT: { double f = x - floor(x); return f >= 1.0 ? 0.99999994 : f; }
    case B_SATURATE: return fclampd(x, 0, 1);
    case B_SIGN: return fsign(x);
    case B_ABS: return x < 0 ? -x : x;
    case B_SINPI: return sin(x * 3.14159265358979323846);
    case B_COSPI: return cos(x * 3.14159265358979323846);
    case B_TANPI: return tan(x * 3.14159265358979323846);
    }
    return 0;
}
static double dotn(const reg_t *a, const reg_t *b, int n) { double s = 0; for (int i = 0; i < n; i++) s += rd(a, i) * rd(b, i); return s; }
static double det3(const double *m, int rows) {
#define M(c, r) m[(c) * rows + (r)]
    return M(0, 0) * (M(1, 1) * M(2, 2) - M(2, 1) * M(1, 2)) - M(1, 0) * (M(0, 1) * M(2, 2) - M(2, 1) * M(0, 2)) +
           M(2, 0) * (M(0, 1) * M(1, 2) - M(1, 1) * M(0, 2));
#undef M
}
static u32 popc(u64 v) { u32 c = 0; while (v) { v &= v - 1; c++; } return c; }

static void* (*g_yield)(void);
static void tex_call(node_t *n, fr_t *fr, reg_t *r);
static void atomic_op(node_t *n, fr_t *fr, reg_t *r);

static void builtin(node_t *n, fr_t *fr, reg_t *r) {
    int b = n->bi;
    reg_t a[4];
    int na = n->nv > 4 ? 4 : n->nv;
    if (b >= B_ATOMIC_LOAD && b <= B_ATOMIC_XOR) { atomic_op(n, fr, r); return; }
    if (b == B_DISCARD) { fr->ctx->discarded = 1; r->n = 1; r->isint = 1; r->i[0] = 0; return; }
    if (b == B_BARRIER) { if (g_yield) g_yield(); r->n = 1; r->isint = 1; r->i[0] = 0; return; }
    for (int i = 0; i < na; i++) {
        if (n->v[i]->t && (IS_NUM(n->v[i]->t) || n->v[i]->t->k == K_ATOMIC)) ev(n->v[i], fr, &a[i]);
        else { a[i].n = 1; a[i].isint = 1; a[i].i[0] = 0; }
    }
    ty_t *rt = n->t;
    int rk = ty_scal(rt), rn = rt->k == K_VEC ? rt->n : rt->k == K_MAT ? rt->n * rt->e->n : 1;
    if (b < B_LAST_GEN) {
        int ints = IS_INTK(rk);
        r->n = rn;
        r->isint = ints;
        for (int i = 0; i < rn; i++) {
            if (ints) {
                i64 x = ri(&a[0], i), y = na > 1 ? ri(&a[1], i) : 0, z = na > 2 ? ri(&a[2], i) : 0, v = 0;
                int uns = IS_UNSK(rk);
                switch (b) {
                case B_ABS: v = x < 0 ? -x : x; break;
                case B_MIN: v = uns ? ((u64)x < (u64)y ? x : y) : (x < y ? x : y); break;
                case B_MAX: v = uns ? ((u64)x > (u64)y ? x : y) : (x > y ? x : y); break;
                case B_CLAMP: v = x < y ? y : x > z ? z : x; break;
                case B_SELECT: v = ri(&a[2], i) ? y : x; break;
                case B_SIGN: v = x > 0 ? 1 : x < 0 ? -1 : 0; break;
                case B_MULHI: v = (i64)(((u64)(u32)x * (u64)(u32)y) >> 32); break;
                case B_ABSDIFF: v = x > y ? x - y : y - x; break;
                case B_HADD: v = (x + y) >> 1; break;
                case B_ROTATE: { int bits = scal_size(rk) * 8; u64 m = bits == 64 ? ~0ull : (1ull << bits) - 1; u64 ux = (u64)x & m; int s = (int)(y & (bits - 1)); v = (i64)(((ux << s) | (ux >> ((bits - s) & (bits - 1)))) & m); if (!s) v = (i64)ux; break; }
                default: v = (i64)un_f(b, (double)x); break;
                }
                r->i[i] = wrapi(rk, v);
                continue;
            }
            double x = rd(&a[0], i), y = na > 1 ? rd(&a[1], i) : 0, z = na > 2 ? rd(&a[2], i) : 0, v;
            switch (b) {
            case B_POW: v = pow(x, y); break;
            case B_ATAN2: v = atan2(x, y); break;
            case B_FMOD: v = fmod(x, y); break;
            case B_MIN: v = x < y ? x : y; break;
            case B_MAX: v = x > y ? x : y; break;
            case B_STEP: v = y < x ? 0 : 1; break;
            case B_COPYSIGN: v = copysign(x, y); break;
            case B_FDIM: v = x > y ? x - y : 0; break;
            case B_CLAMP: v = x < y ? y : x > z ? z : x; break;
            case B_MIX: v = x + (y - x) * z; break;
            case B_SMOOTHSTEP: { double t = y == x ? (z < x ? 0 : 1) : fclampd((z - x) / (y - x), 0, 1); v = t * t * (3 - 2 * t); break; }
            case B_FMA: v = x * y + z; break;
            case B_SELECT: v = (a[2].isint ? ri(&a[2], i) != 0 : rd(&a[2], i) != 0) ? y : x; break;
            default: v = un_f(b, x);
            }
            r->d[i] = rnd(rk, v);
        }
        return;
    }
    int n0 = a[0].n;
    r->isint = 0;
    r->n = 1;
    switch (b) {
    case B_DOT: { int nn = n0 > a[1].n ? n0 : a[1].n; double s = dotn(&a[0], &a[1], nn); if (IS_INTK(rk)) { r->isint = 1; r->i[0] = wrapi(rk, d2i(s)); } else r->d[0] = rnd(rk, s); return; }
    case B_LENGTH: r->d[0] = rnd(rk, sqrt(dotn(&a[0], &a[0], n0))); return;
    case B_LENGTH2: r->d[0] = rnd(rk, dotn(&a[0], &a[0], n0)); return;
    case B_DISTANCE: case B_DISTANCE2: {
        double s = 0;
        for (int i = 0; i < n0; i++) { double d = rd(&a[0], i) - rd(&a[1], i); s += d * d; }
        r->d[0] = rnd(rk, b == B_DISTANCE ? sqrt(s) : s);
        return;
    }
    case B_NORMALIZE: {
        double l = sqrt(dotn(&a[0], &a[0], n0));
        r->n = n0;
        for (int i = 0; i < n0; i++) r->d[i] = rnd(rk, l > 0 ? rd(&a[0], i) / l : 0);
        return;
    }
    case B_CROSS: {
        double x0 = rd(&a[0], 0), y0 = rd(&a[0], 1), z0 = rd(&a[0], 2), x1 = rd(&a[1], 0), y1 = rd(&a[1], 1), z1 = rd(&a[1], 2);
        r->n = 3;
        r->d[0] = rnd(rk, y0 * z1 - z0 * y1);
        r->d[1] = rnd(rk, z0 * x1 - x0 * z1);
        r->d[2] = rnd(rk, x0 * y1 - y0 * x1);
        return;
    }
    case B_REFLECT: {
        double d = dotn(&a[1], &a[0], n0);
        r->n = n0;
        for (int i = 0; i < n0; i++) r->d[i] = rnd(rk, rd(&a[0], i) - 2 * d * rd(&a[1], i));
        return;
    }
    case B_REFRACT: {
        double d = dotn(&a[1], &a[0], n0), eta = rd(&a[2], 0), k = 1 - eta * eta * (1 - d * d);
        r->n = n0;
        for (int i = 0; i < n0; i++) r->d[i] = k < 0 ? 0 : rnd(rk, eta * rd(&a[0], i) - (eta * d + sqrt(k)) * rd(&a[1], i));
        return;
    }
    case B_FACEFORWARD: {
        double d = dotn(&a[2], &a[1], n0);
        r->n = n0;
        for (int i = 0; i < n0; i++) r->d[i] = d < 0 ? rd(&a[0], i) : -rd(&a[0], i);
        return;
    }
    case B_ANY: case B_ALL: {
        int v = b == B_ALL;
        for (int i = 0; i < n0; i++) { int t = a[0].isint ? a[0].i[i] != 0 : a[0].d[i] != 0; if (b == B_ANY) v |= t; else v &= t; }
        r->isint = 1;
        r->i[0] = v;
        return;
    }
    case B_ISNAN: case B_ISINF: case B_ISFINITE: case B_SIGNBIT:
        r->isint = 1;
        r->n = n0;
        for (int i = 0; i < n0; i++) {
            double x = rd(&a[0], i);
            r->i[i] = b == B_ISNAN ? x != x : b == B_ISINF ? (x == __builtin_inf() || x == -__builtin_inf()) :
                      b == B_ISFINITE ? (x == x && x != __builtin_inf() && x != -__builtin_inf()) : (x < 0 || (x == 0 && 1 / x < 0));
        }
        return;
    case B_TRANSPOSE: {
        ty_t *at = n->v[0]->t;
        int ca = at->n, ra = at->e->n;
        r->n = ca * ra;
        for (int c = 0; c < ca; c++) for (int j = 0; j < ra; j++) r->d[j * ca + c] = rd(&a[0], c * ra + j);
        return;
    }
    case B_DETERMINANT: {
        ty_t *at = n->v[0]->t;
        int dim = at->n;
        double m[16];
        for (int i = 0; i < dim * dim; i++) m[i] = rd(&a[0], i);
        double d;
        if (dim == 2) d = m[0] * m[3] - m[2] * m[1];
        else if (dim == 3) d = det3(m, 3);
        else {
            d = 0;
            for (int c = 0; c < 4; c++) {
                double sub[9];
                int k = 0;
                for (int cc = 0; cc < 4; cc++) { if (cc == c) continue; for (int rr = 1; rr < 4; rr++) sub[k++] = m[cc * 4 + rr]; }
                d += (c & 1 ? -1 : 1) * m[c * 4] * det3(sub, 3);
            }
        }
        r->d[0] = d;
        return;
    }
    case B_POPCOUNT: case B_CLZ: case B_CTZ: case B_REVERSE_BITS: {
        int bits = scal_size(rk) * 8;
        u64 msk = bits == 64 ? ~0ull : (1ull << bits) - 1;
        r->isint = 1;
        r->n = n0;
        for (int i = 0; i < n0; i++) {
            u64 x = (u64)ri(&a[0], i) & msk, v = 0;
            if (b == B_POPCOUNT) v = popc(x);
            else if (b == B_CLZ) { v = bits; for (int j = bits - 1; j >= 0; j--) if (x >> j & 1) { v = bits - 1 - j; break; } }
            else if (b == B_CTZ) { v = bits; for (int j = 0; j < bits; j++) if (x >> j & 1) { v = j; break; } }
            else for (int j = 0; j < bits; j++) if (x >> j & 1) v |= 1ull << (bits - 1 - j);
            r->i[i] = wrapi(rk, (i64)v);
        }
        return;
    }
    case B_EXTRACT_BITS: case B_INSERT_BITS: {
        r->isint = 1;
        r->n = n0;
        for (int i = 0; i < n0; i++) {
            u64 x = (u64)ri(&a[0], i);
            if (b == B_EXTRACT_BITS) {
                int off = (int)ri(&a[1], 0), cnt = (int)ri(&a[2], 0);
                u64 m = cnt >= 64 ? ~0ull : (1ull << cnt) - 1;
                i64 v = (i64)((x >> off) & m);
                if (!IS_UNSK(rk) && cnt && cnt < 64 && (v >> (cnt - 1) & 1)) v |= ~(i64)m;
                r->i[i] = wrapi(rk, v);
            } else {
                u64 y = (u64)ri(&a[1], i);
                int off = (int)ri(&a[2], 0), cnt = n->nv > 3 ? (int)ri(&a[3], 0) : 0;
                u64 m = (cnt >= 64 ? ~0ull : (1ull << cnt) - 1) << off;
                r->i[i] = wrapi(rk, (i64)((x & ~m) | ((y << off) & m)));
            }
        }
        return;
    }
    case B_DFDX: case B_DFDY: case B_FWIDTH:
        r->n = n0;
        r->isint = a[0].isint;
        for (int i = 0; i < n0; i++) r->i[i] = 0;
        return;
    case B_SIMD_IDENT: case B_SIMD_SUM: *r = a[0]; return;
    case B_SIMD_BARRIER: r->isint = 1; r->i[0] = 0; return;
    case B_PACK_UNORM4X8: case B_PACK_SNORM4X8: {
        u32 v = 0;
        for (int i = 0; i < 4; i++) {
            double x = rd(&a[0], i);
            i32 c = b == B_PACK_UNORM4X8 ? (i32)(fclampd(x, 0, 1) * 255 + 0.5) : (i32)round(fclampd(x, -1, 1) * 127);
            v |= (u32)(c & 0xff) << (i * 8);
        }
        r->isint = 1;
        r->i[0] = v;
        return;
    }
    case B_PACK_UNORM2X16: case B_PACK_SNORM2X16: {
        u32 v = 0;
        for (int i = 0; i < 2; i++) {
            double x = rd(&a[0], i);
            i32 c = b == B_PACK_UNORM2X16 ? (i32)(fclampd(x, 0, 1) * 65535 + 0.5) : (i32)round(fclampd(x, -1, 1) * 32767);
            v |= (u32)(c & 0xffff) << (i * 16);
        }
        r->isint = 1;
        r->i[0] = v;
        return;
    }
    case B_UNPACK_UNORM4X8: case B_UNPACK_SNORM4X8: {
        u32 x = (u32)ri(&a[0], 0);
        r->n = 4;
        for (int i = 0; i < 4; i++) {
            u8 c = (u8)(x >> (i * 8));
            r->d[i] = b == B_UNPACK_UNORM4X8 ? c / 255.0 : fclampd((i8)c / 127.0, -1, 1);
        }
        return;
    }
    case B_UNPACK_UNORM2X16: case B_UNPACK_SNORM2X16: {
        u32 x = (u32)ri(&a[0], 0);
        r->n = 2;
        for (int i = 0; i < 2; i++) {
            u16 c = (u16)(x >> (i * 16));
            r->d[i] = b == B_UNPACK_UNORM2X16 ? c / 65535.0 : fclampd((i16)c / 32767.0, -1, 1);
        }
        return;
    }
    }
    r->isint = 1;
    r->n = 1;
    r->i[0] = 0;
}

static void atomic_op(node_t *n, fr_t *fr, reg_t *r) {
    u8 *p = 0;
    evm(n->v[0], fr, (u8 *)&p);
    ty_t *et = n->v[0]->t->e;
    int k = et->k == K_ATOMIC ? et->e->k : ty_scal(et);
    reg_t cur, v = {0}, out;
    cur.n = 1;
    cur.isint = IS_INTK(k);
    if (!p) { r->n = 1; r->isint = 1; r->i[0] = 0; return; }
    ld_comp(k, p, &cur, 0);
    int b = n->bi;
    if (b != B_ATOMIC_LOAD && n->nv > 1) {
        if (b == B_ATOMIC_CAS) { if (n->nv > 2) ev(n->v[2], fr, &v); }
        else ev(n->v[1], fr, &v);
        conv(&v, k);
    }
    out = cur;
    out.n = 1;
    switch (b) {
    case B_ATOMIC_LOAD: break;
    case B_ATOMIC_STORE: st_comp(k, p, &v, 0); break;
    case B_ATOMIC_XCHG: st_comp(k, p, &v, 0); break;
    case B_ATOMIC_CAS: {
        u8 *ep = 0;
        evm(n->v[1], fr, (u8 *)&ep);
        reg_t e = {0};
        e.n = 1;
        e.isint = cur.isint;
        if (ep) ld_comp(k, ep, &e, 0);
        int eq = cur.isint ? cur.i[0] == e.i[0] : cur.d[0] == e.d[0];
        if (eq) st_comp(k, p, &v, 0);
        else if (ep) st_comp(k, ep, &cur, 0);
        r->n = 1;
        r->isint = 1;
        r->i[0] = eq;
        return;
    }
    default: {
        reg_t res = {0};
        res.n = 1;
        res.isint = cur.isint;
        if (cur.isint) {
            i64 x = cur.i[0], y = v.i[0], z;
            int uns = IS_UNSK(k);
            switch (b) {
            case B_ATOMIC_ADD: z = x + y; break;
            case B_ATOMIC_SUB: z = x - y; break;
            case B_ATOMIC_MIN: z = uns ? ((u64)x < (u64)y ? x : y) : (x < y ? x : y); break;
            case B_ATOMIC_MAX: z = uns ? ((u64)x > (u64)y ? x : y) : (x > y ? x : y); break;
            case B_ATOMIC_AND: z = x & y; break;
            case B_ATOMIC_OR: z = x | y; break;
            default: z = x ^ y; break;
            }
            res.i[0] = wrapi(k, z);
        } else {
            double x = cur.d[0], y = v.d[0];
            res.d[0] = b == B_ATOMIC_ADD ? x + y : b == B_ATOMIC_SUB ? x - y : b == B_ATOMIC_MIN ? (x < y ? x : y) : (x > y ? x : y);
        }
        st_comp(k, p, &res, 0);
    }
    }
    *r = out;
}

/* ---- texturas */
static void tex_call(node_t *n, fr_t *fr, reg_t *r) {
    msl_tex_t *t = 0;
    evm(n->a, fr, (u8 *)&t);
    ty_t *tt = n->a->t;
    int m = n->sub;
    r->n = 1;
    r->isint = 1;
    r->i[0] = 0;
    int lvl = 0;
    double lod = 0;
    if (n->d && n->d->nv) {
        reg_t o;
        ev(n->d->v[0], fr, &o);
        if (n->d->bi == B_LEVEL || n->d->bi == B_BIAS) lod = rd(&o, 0);
    }
    int array = tt->tex == MSL_TEX_2D_ARRAY || tt->tex == MSL_TEX_1D_ARRAY || tt->tex == MSL_TEX_CUBE_ARRAY || tt->tex == MSL_TEX_2D_MS_ARRAY;
    if (!t) {
        if (m == T_SAMPLE || m == T_READ || m == T_GATHER) { r->n = tt->depth && m != T_GATHER ? 1 : 4; for (int i = 0; i < r->n; i++) r->i[i] = 0; r->isint = IS_INTK(ty_scal(n->t)); if (!r->isint) for (int i = 0; i < r->n; i++) r->d[i] = 0; }
        return;
    }
    switch (m) {
    case T_GET_W: case T_GET_H: case T_GET_D: case T_GET_MIPS: case T_GET_ARR: case T_GET_SAMPLES: {
        if (n->nv) { reg_t l; ev(n->v[0], fr, &l); lvl = (int)ri(&l, 0); }
        i64 v;
        if (m == T_GET_W) v = t->w >> lvl;
        else if (m == T_GET_H) v = t->h >> lvl;
        else if (m == T_GET_D) v = t->d >> lvl;
        else if (m == T_GET_MIPS) v = t->levels;
        else if (m == T_GET_SAMPLES) v = 1;
        else v = t->type == MSL_TEX_CUBE_ARRAY ? t->slices / 6 : t->slices;
        r->i[0] = v < 1 ? 1 : v;
        return;
    }
    case T_FENCE: return;
    case T_SAMPLE: case T_SAMPLE_CMP: case T_GATHER: {
        msl_samp_t *s = 0;
        if (n->nv > 0) evm(n->v[0], fr, (u8 *)&s);
        static msl_samp_t defs = {0, 0, 0, {0, 0, 0}, 1, 0, 0, 0, 1000};
        if (!s) s = &defs;
        reg_t c;
        ev(n->v[1], fr, &c);
        double cd[4] = {rd(&c, 0), c.n > 1 ? rd(&c, 1) : 0, c.n > 2 ? rd(&c, 2) : 0, 0};
        int slice = 0, ai = 2;
        if (array && n->nv > 2) { reg_t sr; ev(n->v[2], fr, &sr); slice = (int)ri(&sr, 0); ai = 3; }
        double out[4];
        if (m == T_GATHER) {
            int w = t->w, h = t->h;
            double x = cd[0] * (s->normalized ? w : 1) - 0.5, y = cd[1] * (s->normalized ? h : 1) - 0.5;
            int x0 = (int)floor(x), y0 = (int)floor(y), comp = 0;
            if (n->nv > ai) { reg_t cr; ev(n->v[n->nv - 1], fr, &cr); if (n->v[n->nv - 1]->t && IS_SCALAR(n->v[n->nv - 1]->t)) comp = (int)ri(&cr, 0) & 3; }
            int px[4] = {x0, x0 + 1, x0 + 1, x0}, py[4] = {y0 + 1, y0 + 1, y0, y0};
            r->n = 4;
            r->isint = IS_INTK(ty_scal(n->t));
            for (int i = 0; i < 4; i++) {
                int xx = px[i] < 0 ? 0 : px[i] >= w ? w - 1 : px[i], yy = py[i] < 0 ? 0 : py[i] >= h ? h - 1 : py[i];
                msl_tex_load(t, xx, yy, 0, slice, 0, out);
                if (r->isint) r->i[i] = d2i(out[comp]); else r->d[i] = out[comp];
            }
            return;
        }
        int nc = tt->tex == MSL_TEX_1D || tt->tex == MSL_TEX_1D_ARRAY ? 1 : (tt->tex == MSL_TEX_3D || tt->tex == MSL_TEX_CUBE || tt->tex == MSL_TEX_CUBE_ARRAY) ? 3 : 2;
        msl_tex_sample(t, s, cd, nc, slice, lod, out);
        if (m == T_SAMPLE_CMP) {
            reg_t ref;
            ev(n->v[ai < n->nv ? ai : n->nv - 1], fr, &ref);
            double rv = rd(&ref, 0), dv = out[0];
            int cf = s->compare, pass;
            switch (cf) {
            case 0: pass = 0; break;
            case 1: pass = rv < dv; break;
            case 2: pass = rv == dv; break;
            case 3: pass = rv <= dv; break;
            case 4: pass = rv > dv; break;
            case 5: pass = rv != dv; break;
            case 6: pass = rv >= dv; break;
            default: pass = 1;
            }
            r->isint = 0;
            r->d[0] = pass;
            return;
        }
        r->isint = IS_INTK(ty_scal(n->t));
        r->n = tt->depth ? 1 : 4;
        for (int i = 0; i < r->n; i++) { if (r->isint) r->i[i] = d2i(out[i]); else r->d[i] = rnd(ty_scal(n->t), out[i]); }
        return;
    }
    case T_READ: case T_WRITE: {
        int ci = m == T_WRITE ? 1 : 0;
        reg_t val;
        if (m == T_WRITE) ev(n->v[0], fr, &val);
        reg_t c;
        ev(n->v[ci], fr, &c);
        int x = (int)ri(&c, 0), y = c.n > 1 ? (int)ri(&c, 1) : 0, z = c.n > 2 ? (int)ri(&c, 2) : 0, slice = 0;
        int nx = ci + 1;
        if ((array || tt->tex == MSL_TEX_CUBE) && n->nv > nx) { reg_t sr; ev(n->v[nx], fr, &sr); slice = (int)ri(&sr, 0); nx++; }
        if (n->nv > nx && tt->tex != MSL_TEX_2D_MS) { reg_t lr; ev(n->v[nx], fr, &lr); lvl = (int)ri(&lr, 0); }
        if (lvl < 0 || lvl >= t->levels) lvl = 0;
        int w = t->w >> lvl, h = t->h >> lvl, d = t->d >> lvl;
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        if (d < 1) d = 1;
        if (tt->tex == MSL_TEX_BUFFER) { y = x / (w ? w : 1); x = x % (w ? w : 1); }
        if (x < 0 || y < 0 || z < 0 || x >= w || y >= h || z >= d || slice < 0 || slice >= (t->slices ? t->slices : 1)) {
            if (m == T_READ) { r->n = tt->depth ? 1 : 4; r->isint = IS_INTK(ty_scal(n->t)); for (int i = 0; i < r->n; i++) r->i[i] = 0; if (!r->isint) for (int i = 0; i < r->n; i++) r->d[i] = 0; }
            return;
        }
        double px[4];
        if (m == T_WRITE) {
            bcast(&val, 4);
            for (int i = 0; i < 4; i++) px[i] = i < val.n ? rd(&val, i) : (i == 3 ? 1 : 0);
            msl_tex_store(t, x, y, z, slice, lvl, px);
            return;
        }
        msl_tex_load(t, x, y, z, slice, lvl, px);
        r->isint = IS_INTK(ty_scal(n->t));
        r->n = tt->depth ? 1 : 4;
        for (int i = 0; i < r->n; i++) { if (r->isint) r->i[i] = d2i(px[i]); else r->d[i] = rnd(ty_scal(n->t), px[i]); }
        return;
    }
    }
}

/* ---- construtores: junta os componentes dos argumentos */
static void gather(node_t **v, int nv, ty_t *t, fr_t *fr, reg_t *r) {
    int k = ty_scal(t), want = ty_ncomp(t);
    r->isint = IS_INTK(k);
    r->n = 0;
    if (nv == 0) { r->n = want; for (int i = 0; i < want; i++) r->i[i] = 0; if (!r->isint) for (int i = 0; i < want; i++) r->d[i] = 0; return; }
    if (nv == 1 && v[0]->t && IS_NUM(v[0]->t)) {
        reg_t a;
        ev(v[0], fr, &a);
        conv(&a, k);
        if (a.n == 1) {
            if (t->k == K_MAT) {     /* diagonal */
                int rows = t->e->n;
                r->n = want;
                for (int i = 0; i < want; i++) { if (r->isint) r->i[i] = 0; else r->d[i] = 0; }
                for (int c = 0; c < t->n && c < rows; c++) r->i[c * rows + c] = a.i[0];
                return;
            }
            bcast(&a, want);
            *r = a;
            r->n = want;
            return;
        }
        if (t->k == K_MAT && v[0]->t->k == K_MAT) {   /* float3x3(float4x4): canto superior esquerdo */
            int rows = t->e->n, srows = v[0]->t->e->n;
            r->n = want;
            for (int c = 0; c < t->n; c++)
                for (int j = 0; j < rows; j++) {
                    int in = c < v[0]->t->n && j < srows;
                    if (r->isint) r->i[c * rows + j] = in ? a.i[c * srows + j] : (c == j);
                    else r->d[c * rows + j] = in ? a.d[c * srows + j] : (c == j);
                }
            return;
        }
    }
    for (int i = 0; i < nv && r->n < 16; i++) {
        reg_t a;
        if (!v[i]->t || !IS_NUM(v[i]->t)) continue;
        ev(v[i], fr, &a);
        conv(&a, k);
        for (int j = 0; j < a.n && r->n < 16; j++) r->i[r->n++] = a.i[j];
    }
    while (r->n < want) { r->i[r->n] = 0; if (!r->isint) r->d[r->n] = 0; r->n++; }
    r->n = want;
}
/* inicializa um agregado (struct/array) a partir de uma lista */
static void init_agg(node_t **v, int nv, ty_t *t, fr_t *fr, u8 *dst) {
    memset(dst, 0, t->size);
    if (t->k == K_ARRAY) {
        for (int i = 0; i < nv && i < t->n; i++) {
            node_t *c = v[i];
            if (c->op == E_INIT && (!c->t || c->t == t->e || c->t->k == t->e->k)) {
                if (IS_NUM(t->e)) { reg_t r; gather(c->v, c->nv, t->e, fr, &r); store(t->e, &r, dst + i * t->e->size); }
                else init_agg(c->v, c->nv, t->e, fr, dst + i * t->e->size);
            } else if (IS_NUM(t->e)) { reg_t r; ev(c, fr, &r); store(t->e, &r, dst + i * t->e->size); }
            else evm(c, fr, dst + i * t->e->size);
        }
        return;
    }
    if (t->k == K_STRUCT) {
        for (int i = 0; i < nv && i < t->nf; i++) {
            fld_t *f = &t->f[i];
            node_t *c = v[i];
            if (c->op == E_INIT && !IS_NUM(f->t)) init_agg(c->v, c->nv, f->t, fr, dst + f->off);
            else if (c->op == E_INIT) { reg_t r; gather(c->v, c->nv, f->t, fr, &r); store(f->t, &r, dst + f->off); }
            else if (IS_NUM(f->t)) { reg_t r; ev(c, fr, &r); store(f->t, &r, dst + f->off); }
            else evm(c, fr, dst + f->off);
        }
        return;
    }
    if (IS_NUM(t)) { reg_t r; gather(v, nv, t, fr, &r); store(t, &r, dst); return; }
    if (nv) evm(v[0], fr, dst);
}

static void incdec(node_t *n, fr_t *fr, reg_t *r) {
    node_t *l = n->a;
    int inc = n->op == E_PREINC || n->op == E_POSTINC, post = n->op == E_POSTINC || n->op == E_POSTDEC;
    if (l->t->k == K_PTR) {
        u8 **pp = (u8 **)lv(l, fr);
        u8 *old = *pp;
        *pp += (inc ? 1 : -1) * (i64)(l->t->e->size ? l->t->e->size : 1);
        r->n = 1;
        r->isint = 1;
        r->i[0] = (i64)(post ? old : *pp);
        return;
    }
    reg_t cur, one = {0}, res;
    one.n = 1;
    one.isint = 1;
    one.i[0] = 1;
    ty_t *t = l->t->k == K_ATOMIC ? l->t->e : l->t;
    if (l->op == E_SWZ) {
        u8 tmpb[64];
        u8 *bp = lv(l->a, fr);
        (void)tmpb;
        int ss = scal_size(ty_scal(l->a->t));
        cur.n = 1;
        cur.isint = IS_INTK(ty_scal(t));
        ld_comp(ty_scal(t), bp + l->swz[0] * ss, &cur, 0);
        binop(inc ? '+' : '-', &cur, t, &one, msl_tscal(K_INT), t, &res);
        st_comp(ty_scal(t), bp + l->swz[0] * ss, &res, 0);
        *r = post ? cur : res;
        return;
    }
    u8 *p = lv(l, fr);
    load(t, p, &cur);
    binop(inc ? '+' : '-', &cur, t, &one, msl_tscal(K_INT), t, &res);
    store(t, &res, p);
    *r = post ? cur : res;
}

static void assign(node_t *n, fr_t *fr, reg_t *out, u8 *outm) {
    node_t *l = n->a;
    int op = n->sub;
    if (l->op == E_SWZ) {
        u8 *bp = lv(l->a, fr);
        int k = ty_scal(l->a->t), ss = scal_size(k);
        reg_t rv, cur;
        ev(n->b, fr, &rv);
        if (op != '=') {
            cur.n = l->nswz;
            cur.isint = IS_INTK(k);
            for (int i = 0; i < l->nswz; i++) ld_comp(k, bp + l->swz[i] * ss, &cur, i);
            reg_t res;
            binop(op, &cur, l->t, &rv, n->b->t, l->t, &res);
            rv = res;
        }
        conv(&rv, k);
        bcast(&rv, l->nswz);
        for (int i = 0; i < l->nswz; i++) st_comp(k, bp + l->swz[i] * ss, &rv, i);
        if (out) *out = rv;
        return;
    }
    u8 *p = lv(l, fr);
    ty_t *t = l->t->k == K_ATOMIC ? l->t->e : l->t;
    if (IS_NUM(t)) {
        reg_t rv;
        ev(n->b, fr, &rv);
        if (op != '=') {
            reg_t cur, res;
            load(t, p, &cur);
            binop(op, &cur, t, &rv, n->b->t, t, &res);
            rv = res;
        }
        store(t, &rv, p);
        if (out) load(t, p, out);
        return;
    }
    if (t->k == K_PTR && op != '=') {
        reg_t rv;
        ev(n->b, fr, &rv);
        *(u8 **)p += (op == '-' ? -1 : 1) * ri(&rv, 0) * (i64)(t->e->size ? t->e->size : 1);
        return;
    }
    u8 *tmp = __builtin_alloca(t->size + 16);
    evm(n->b, fr, tmp);
    memcpy(p, tmp, t->size);
    if (outm) memcpy(outm, p, t->size);
}

static void ev(node_t *n, fr_t *fr, reg_t *r) {
    switch (n->op) {
    case E_LIT: {
        int k = ty_scal(n->t);
        r->n = 1;
        r->isint = IS_INTK(k);
        if (r->isint) r->i[0] = n->iv; else r->d[0] = rnd(k, n->fv);
        return;
    }
    case E_VAR: case E_MEMBER: case E_INDEX: case E_DEREF: {
        if (n->op == E_INDEX && n->a->t->k == K_VEC && !is_lv(n->a)) {
            reg_t b, i;
            ev(n->a, fr, &b);
            ev(n->b, fr, &i);
            i64 j = clampi(ri(&i, 0), b.n);
            r->n = 1;
            r->isint = b.isint;
            r->i[0] = b.i[j];
            return;
        }
        u8 *tmp = is_lv(n) ? 0 : __builtin_alloca(chain_size(n));
        u8 *p = addr(n, fr, tmp);
        ty_t *t = n->t->k == K_ATOMIC ? n->t->e : n->t;
        if (!p) { r->n = ty_ncomp(t); r->isint = 1; for (int i = 0; i < r->n; i++) r->i[i] = 0; return; }
        load(t, p, r);
        return;
    }
    case E_SWZ: {
        reg_t b;
        ev(n->a, fr, &b);
        r->n = n->nswz;
        r->isint = b.isint;
        for (int i = 0; i < n->nswz; i++) r->i[i] = b.i[b.n == 1 ? 0 : n->swz[i]];
        return;
    }
    case E_CALL: {
        u8 *tmp = __builtin_alloca(n->t->size + 16);
        call_fn(n->fn, n, fr, tmp);
        if (IS_NUM(n->t)) load(n->t, tmp, r);
        else { r->n = 1; r->isint = 1; r->i[0] = 0; }
        return;
    }
    case E_BI: builtin(n, fr, r); return;
    case E_TEX: tex_call(n, fr, r); return;
    case E_CTOR: case E_INIT:
        if (IS_NUM(n->t)) { gather(n->v, n->nv, n->t, fr, r); return; }
        r->n = 1; r->isint = 1; r->i[0] = 0;
        return;
    case E_CAST:
        if (n->t->k == K_PTR || !IS_NUM(n->t)) { u8 *p = 0; evm(n->a, fr, (u8 *)&p); r->n = 1; r->isint = 1; r->i[0] = (i64)p; return; }
        ev(n->a, fr, r);
        conv(r, ty_scal(n->t));
        if (n->t->k == K_VEC) bcast(r, n->t->n);
        return;
    case E_BITCAST: {
        u8 tmp[64] = {0};
        if (n->a->t->size <= 64) evm(n->a, fr, tmp);
        load(n->t, tmp, r);
        return;
    }
    case E_UN: {
        ev(n->a, fr, r);
        int k = ty_scal(n->t);
        if (n->sub == '!') {
            for (int i = 0; i < r->n; i++) r->i[i] = r->isint ? !r->i[i] : !r->d[i];
            r->isint = 1;
        } else if (n->sub == '~') {
            for (int i = 0; i < r->n; i++) r->i[i] = wrapi(k, ~r->i[i]);
        } else if (n->sub == '-') {
            if (r->isint) for (int i = 0; i < r->n; i++) r->i[i] = wrapi(k == K_BOOL ? K_INT : k, -r->i[i]);
            else for (int i = 0; i < r->n; i++) r->d[i] = -r->d[i];
        }
        return;
    }
    case E_BIN: {
        if (n->t->k == K_PTR) { u8 *p = 0; evm(n, fr, (u8 *)&p); r->n = 1; r->isint = 1; r->i[0] = (i64)p; return; }
        reg_t a, b;
        ev(n->a, fr, &a);
        ev(n->b, fr, &b);
        binop(n->sub, &a, n->a->t->k == K_ATOMIC ? n->a->t->e : n->a->t, &b, n->b->t->k == K_ATOMIC ? n->b->t->e : n->b->t, n->t, r);
        return;
    }
    case E_LAND: case E_LOR: {
        reg_t a;
        ev(n->a, fr, &a);
        int t = truth(&a);
        if (n->op == E_LAND ? t : !t) { reg_t b; ev(n->b, fr, &b); t = truth(&b); }
        r->n = 1;
        r->isint = 1;
        r->i[0] = t;
        return;
    }
    case E_COND: {
        reg_t c;
        ev(n->a, fr, &c);
        if (c.n > 1) {         /* condição vetorial: escolhe por componente */
            reg_t a, b;
            ev(n->b, fr, &a);
            ev(n->c, fr, &b);
            int k = ty_scal(n->t);
            conv(&a, k);
            conv(&b, k);
            int nn = c.n;
            bcast(&a, nn);
            bcast(&b, nn);
            r->n = nn;
            r->isint = a.isint;
            for (int i = 0; i < nn; i++) r->i[i] = (c.isint ? c.i[i] : c.d[i] != 0) ? a.i[i] : b.i[i];
            return;
        }
        ev(truth(&c) ? n->b : n->c, fr, r);
        conv(r, ty_scal(n->t));
        return;
    }
    case E_ASSIGN: assign(n, fr, r, 0); return;
    case E_PREINC: case E_PREDEC: case E_POSTINC: case E_POSTDEC: incdec(n, fr, r); return;
    case E_ADDR: { u8 *p = lv(n->a, fr); r->n = 1; r->isint = 1; r->i[0] = (i64)p; return; }
    case E_COMMA: {
        if (n->a->t && n->a->t->size) { u8 *tmp = __builtin_alloca(n->a->t->size + 16); evm(n->a, fr, tmp); }
        else { reg_t d; ev(n->a, fr, &d); }
        ev(n->b, fr, r);
        return;
    }
    }
    r->n = 1;
    r->isint = 1;
    r->i[0] = 0;
}

static void evm(node_t *n, fr_t *fr, u8 *dst) {
    ty_t *t = n->t;
    if (!t) { reg_t d; ev(n, fr, &d); return; }
    if (IS_NUM(t)) { reg_t r; ev(n, fr, &r); store(t, &r, dst); return; }
    switch (n->op) {
    case E_VAR: case E_MEMBER: case E_INDEX: case E_DEREF: {
        u8 *tmp = is_lv(n) ? 0 : __builtin_alloca(chain_size(n));
        u8 *p = addr(n, fr, tmp);
        if (p) memmove(dst, p, t->size); else memset(dst, 0, t->size);
        return;
    }
    case E_CALL: call_fn(n->fn, n, fr, dst); return;
    case E_INIT: case E_CTOR: init_agg(n->v, n->nv, t, fr, dst); return;
    case E_ADDR: *(u8 **)dst = lv(n->a, fr); return;
    case E_CAST: case E_BITCAST:
        if (n->a->t && IS_NUM(n->a->t)) { reg_t r; ev(n->a, fr, &r); *(u64 *)dst = (u64)ri(&r, 0); return; }
        evm(n->a, fr, dst);
        return;
    case E_BIN:
        if (t->k == K_PTR) {
            u8 *p = 0;
            reg_t i;
            evm(n->a, fr, (u8 *)&p);
            ev(n->b, fr, &i);
            *(u8 **)dst = p + (n->sub == '-' ? -1 : 1) * ri(&i, 0) * (i64)(t->e->size ? t->e->size : 1);
            return;
        }
        break;
    case E_COND: { reg_t c; ev(n->a, fr, &c); evm(truth(&c) ? n->b : n->c, fr, dst); return; }
    case E_ASSIGN: assign(n, fr, 0, dst); return;
    case E_COMMA: { u8 *tmp = __builtin_alloca((n->a->t ? n->a->t->size : 0) + 16); evm(n->a, fr, tmp); evm(n->b, fr, dst); return; }
    case E_PREINC: case E_PREDEC: case E_POSTINC: case E_POSTDEC: { reg_t r; incdec(n, fr, &r); *(i64 *)dst = r.i[0]; return; }
    case E_BI: case E_TEX: { reg_t r; ev(n, fr, &r); return; }
    }
    memset(dst, 0, t->size);
}

/* ================================================================ comandos */
#define LOOP_MAX (1 << 26)
static int exec(node_t *s, fr_t *fr) {
    if (!s) return ST_OK;
    switch (s->op) {
    case S_NOP: return ST_OK;
    case S_EXPR: {
        node_t *e = s->a;
        if (e->t && e->t->size > 0 && !IS_NUM(e->t)) { u8 *tmp = __builtin_alloca(e->t->size + 16); evm(e, fr, tmp); }
        else { reg_t r; ev(e, fr, &r); }
        return fr->ctx->discarded ? ST_RET : ST_OK;
    }
    case S_DECL: {
        var_t *v = s->var;
        u8 *slot = fr->base + v->off;
        if (v->ref) { *(u8 **)slot = s->a ? lv(s->a, fr) : 0; return ST_OK; }
        if (!s->a) { memset(slot, 0, v->t->size); return ST_OK; }
        if (IS_NUM(v->t)) { reg_t r; ev(s->a, fr, &r); store(v->t, &r, slot); }
        else if (s->a->op == E_INIT) init_agg(s->a->v, s->a->nv, v->t, fr, slot);
        else { u8 *tmp = __builtin_alloca(v->t->size + 16); evm(s->a, fr, tmp); memcpy(slot, tmp, v->t->size); }
        return ST_OK;
    }
    case S_BLOCK:
        for (int i = 0; i < s->nv; i++) {
            int r = exec(s->v[i], fr);
            if (r) return r;
            if (fr->ctx->discarded) return ST_RET;
        }
        return ST_OK;
    case S_IF: {
        reg_t c;
        ev(s->a, fr, &c);
        return exec(truth(&c) ? s->b : s->c, fr);
    }
    case S_FOR: {
        int r = exec(s->a, fr);
        if (r) return r;
        for (int it = 0; it < LOOP_MAX; it++) {
            if (s->b) { reg_t c; ev(s->b, fr, &c); if (!truth(&c)) break; }
            r = exec(s->d, fr);
            if (r == ST_BREAK) break;
            if (r == ST_RET) return r;
            if (s->c) { if (s->c->t && s->c->t->size && !IS_NUM(s->c->t)) { u8 *tmp = __builtin_alloca(s->c->t->size + 16); evm(s->c, fr, tmp); } else { reg_t d; ev(s->c, fr, &d); } }
        }
        return ST_OK;
    }
    case S_WHILE:
        for (int it = 0; it < LOOP_MAX; it++) {
            reg_t c;
            ev(s->a, fr, &c);
            if (!truth(&c)) break;
            int r = exec(s->b, fr);
            if (r == ST_BREAK) break;
            if (r == ST_RET) return r;
        }
        return ST_OK;
    case S_DO:
        for (int it = 0; it < LOOP_MAX; it++) {
            int r = exec(s->b, fr);
            if (r == ST_BREAK) break;
            if (r == ST_RET) return r;
            reg_t c;
            ev(s->a, fr, &c);
            if (!truth(&c)) break;
        }
        return ST_OK;
    case S_SWITCH: {
        reg_t c;
        ev(s->a, fr, &c);
        i64 v = ri(&c, 0);
        int start = s->sub;
        for (int i = 0; i < s->off; i++) if (s->cases[i * 2] == v) { start = (int)s->cases[i * 2 + 1]; break; }
        if (start < 0) return ST_OK;
        for (int i = start; i < s->nv; i++) {
            int r = exec(s->v[i], fr);
            if (r == ST_BREAK) return ST_OK;
            if (r) return r;
        }
        return ST_OK;
    }
    case S_RET:
        if (s->a) {
            ty_t *rt = fr->fn && fr->fn->ret->k != K_VOID ? fr->fn->ret : 0;
            u8 *slot = rt ? fr->base + fr->fn->retoff : 0;
            if (rt && IS_NUM(rt)) { reg_t r; ev(s->a, fr, &r); store(rt, &r, slot); }
            else if (rt && s->a->op == E_INIT) init_agg(s->a->v, s->a->nv, rt, fr, slot);
            else if (rt) { u8 *tmp = __builtin_alloca(rt->size + 16); evm(s->a, fr, tmp); memcpy(slot, tmp, rt->size); }
            else { reg_t d; ev(s->a, fr, &d); }
        }
        return ST_RET;
    case S_BREAK: return ST_BREAK;
    case S_CONT: return ST_CONT;
    }
    return ST_OK;
}

/* ================================================================ chamadas */
static void call_fn(msl_fn_t *f, node_t *c, fr_t *fr, u8 *dst) {
    if (!f || !f->defined || fr->depth > 48) {
        if (f && !f->defined) cx_fdprintf(2, "Metal: função '%s' declarada sem corpo\n", f->name);
        if (c->t && c->t->size) memset(dst, 0, c->t->size);
        return;
    }
    u8 *base = __builtin_alloca(f->frame + 16);
    memset(base, 0, f->frame + 16);
    for (int i = 0; i < f->np && i < c->nv; i++) {
        var_t *p = f->params[i];
        node_t *a = c->v[i];
        u8 *slot = base + p->off;
        if (p->ref) {
            if (is_lv(a)) *(u8 **)slot = lv(a, fr);
            else {
                u8 *tmp = __builtin_alloca(p->t->size + 16);
                if (IS_NUM(p->t)) { reg_t r; ev(a, fr, &r); store(p->t, &r, tmp); }
                else evm(a, fr, tmp);
                *(u8 **)slot = tmp;
            }
        } else if (IS_NUM(p->t)) { reg_t r; ev(a, fr, &r); store(p->t, &r, slot); }
        else if (a->op == E_INIT) init_agg(a->v, a->nv, p->t, fr, slot);
        else evm(a, fr, slot);
    }
    fr_t nf = {base, fr->ctx, f->lib, fr->depth + 1, f};
    exec(f->body, &nf);
    if (f->ret->k != K_VOID && dst) memcpy(dst, base + f->retoff, f->ret->size);
}

/* parâmetros da função de entrada a partir do contexto */
static void put_u32s(var_t *p, u8 *slot, const u32 *v, int n) {
    reg_t r;
    r.isint = 1;
    r.n = n;
    for (int i = 0; i < n; i++) r.i[i] = v[i];
    ty_t *t = p->t;
    if (IS_NUM(t)) store(t, &r, slot);
}
void msl_run(msl_fn_t *f, msl_ctx_t *c, void *out) {
    u8 *base = __builtin_alloca(f->frame + 16);
    memset(base, 0, f->frame + 16);
    for (int i = 0; i < f->np; i++) {
        var_t *p = f->params[i];
        u8 *slot = base + p->off;
        ty_t *t = p->t;
        int ix = p->idx;
        switch (p->attr) {
        case A_BUFFER:
            if (ix < 0 || ix > 30) break;
            if (p->ref || t->k == K_PTR) *(void **)slot = c->buf[ix];
            else if (c->buf[ix]) memcpy(slot, c->buf[ix], t->size);
            break;
        case A_TEXTURE:
            if (t->k == K_ARRAY) { for (int j = 0; j < t->n && ix + j < 128; j++) ((void **)slot)[j] = c->tex[ix + j]; }
            else if (ix >= 0 && ix < 128) *(void **)slot = c->tex[ix];
            break;
        case A_SAMPLER:
            if (t->k == K_ARRAY) { for (int j = 0; j < t->n && ix + j < 16; j++) ((void **)slot)[j] = c->samp[ix + j]; }
            else if (ix >= 0 && ix < 16) *(void **)slot = c->samp[ix];
            break;
        case A_STAGE_IN:
            if (!c->stage_in) break;
            if (p->ref) *(void **)slot = c->stage_in;
            else memcpy(slot, c->stage_in, t->size);
            break;
        case A_THREADGROUP: if (ix >= 0 && ix < 31) *(void **)slot = c->tgbuf[ix]; break;
        case A_VID: put_u32s(p, slot, &c->vid, 1); break;
        case A_IID: put_u32s(p, slot, &c->iid, 1); break;
        case A_BASE_VERTEX: put_u32s(p, slot, &c->base_vertex, 1); break;
        case A_BASE_INSTANCE: put_u32s(p, slot, &c->base_instance, 1); break;
        case A_TPIG: put_u32s(p, slot, c->tpig, ty_ncomp(t)); break;
        case A_TPITG: put_u32s(p, slot, c->tpitg, ty_ncomp(t)); break;
        case A_TGPIG: put_u32s(p, slot, c->tgpig, ty_ncomp(t)); break;
        case A_TPTG: put_u32s(p, slot, c->tptg, ty_ncomp(t)); break;
        case A_TPG: put_u32s(p, slot, c->tpg, ty_ncomp(t)); break;
        case A_TGPG: put_u32s(p, slot, c->tgpg, ty_ncomp(t)); break;
        case A_TIITG: put_u32s(p, slot, &c->tiitg, 1); break;
        case A_SIMD_GROUP: put_u32s(p, slot, &c->tiitg, 1); break;
        case A_SIMD_SIZE: { u32 one = 1; put_u32s(p, slot, &one, 1); break; }
        case A_SAMPLE_ID: put_u32s(p, slot, &c->sample_id, 1); break;
        case A_PRIMITIVE_ID: put_u32s(p, slot, &c->primitive_id, 1); break;
        case A_SAMPLE_MASK: { u32 m = 1; put_u32s(p, slot, &m, 1); break; }
        case A_FRONT_FACING: if (IS_NUM(t)) { reg_t r = {1, 1}; r.i[0] = c->front_facing; store(t, &r, slot); } break;
        case A_POSITION: if (IS_NUM(t)) { reg_t r = {4, 0}; for (int j = 0; j < 4; j++) r.d[j] = c->fragpos[j]; store(t, &r, slot); } break;
        case A_POINT_COORD: if (IS_NUM(t)) { reg_t r = {2, 0}; r.d[0] = c->point_coord[0]; r.d[1] = c->point_coord[1]; store(t, &r, slot); } break;
        }
    }
    fr_t fr = {base, c, f->lib, 0, f};
    exec(f->body, &fr);
    if (out && f->ret->k != K_VOID) memcpy(out, base + f->retoff, f->ret->size);
}

/* ================================================================ fibras (threadgroup_barrier) */
#if defined(__aarch64__)
/* arm64: quadro de 160 bytes com os preservados (x19..x30, d8..d15); a fibra nova começa em msl_fiber_start (x30) com
 * x19 = fiber_t */
__asm__(".text\n"
        ".globl msl_ctx_switch\n.p2align 2\n"
        "msl_ctx_switch:\n"
        "  sub sp, sp, #160\n"
        "  stp x19, x20, [sp]\n  stp x21, x22, [sp, #16]\n  stp x23, x24, [sp, #32]\n  stp x25, x26, [sp, #48]\n"
        "  stp x27, x28, [sp, #64]\n  stp x29, x30, [sp, #80]\n  stp d8, d9, [sp, #96]\n  stp d10, d11, [sp, #112]\n"
        "  stp d12, d13, [sp, #128]\n  stp d14, d15, [sp, #144]\n"
        "  mov x9, sp\n  str x9, [x0]\n  mov sp, x1\n"
        "  ldp x19, x20, [sp]\n  ldp x21, x22, [sp, #16]\n  ldp x23, x24, [sp, #32]\n  ldp x25, x26, [sp, #48]\n"
        "  ldp x27, x28, [sp, #64]\n  ldp x29, x30, [sp, #80]\n  ldp d8, d9, [sp, #96]\n  ldp d10, d11, [sp, #112]\n"
        "  ldp d12, d13, [sp, #128]\n  ldp d14, d15, [sp, #144]\n"
        "  add sp, sp, #160\n"
        "  ret\n"
        ".globl msl_fiber_start\n.p2align 2\n"
        "msl_fiber_start:\n"
        "  mov x0, x19\n"
        "  bl msl_fiber_main\n"
        "  brk #1\n");
#else
__asm__(".text\n"
        ".globl msl_ctx_switch\n"
        "msl_ctx_switch:\n"
        "  pushq %rbp\n  pushq %rbx\n  pushq %r12\n  pushq %r13\n  pushq %r14\n  pushq %r15\n"
        "  movq %rsp, (%rdi)\n"
        "  movq %rsi, %rsp\n"
        "  popq %r15\n  popq %r14\n  popq %r13\n  popq %r12\n  popq %rbx\n  popq %rbp\n"
        "  ret\n"
        ".globl msl_fiber_start\n"
        "msl_fiber_start:\n"
        "  movq %rbx, %rdi\n"
        "  andq $-16, %rsp\n"
        "  call msl_fiber_main\n"
        "  ud2\n");
#endif
void msl_ctx_switch(void **save, void *to);
void msl_fiber_start(void);
#define FIBER_STACK (96 * 1024)
#define MAX_FIBERS 1024
typedef struct fiber {
    void *sp;
    u8 *stack;
    int done;
    msl_fn_t *fn;
    msl_ctx_t ctx;
} fiber_t;
static fiber_t *g_fib;
static void *g_sched_sp;
static u8 *g_stacks[MAX_FIBERS];
static void *fiber_yield(void) {
    fiber_t *f = g_fib;
    if (f) msl_ctx_switch(&f->sp, g_sched_sp);
    return 0;
}
void msl_fiber_main(fiber_t *f) {
    msl_run(f->fn, &f->ctx, 0);
    f->done = 1;
    msl_ctx_switch(&f->sp, g_sched_sp);
    for (;;) {}
}
static void fiber_init(fiber_t *f, u8 *stack) {
    u64 *sp = (u64 *)((u64)(stack + FIBER_STACK) & ~15ull);
#if defined(__aarch64__)
    sp -= 20;                          /* quadro do msl_ctx_switch: x19..x30, d8..d15 */
    for (int i = 0; i < 20; i++) sp[i] = 0;
    sp[0] = (u64)f;                    /* x19 */
    sp[11] = (u64)msl_fiber_start;     /* x30 */
#else
    *--sp = 0;                         /* alinhamento */
    *--sp = (u64)msl_fiber_start;      /* ret */
    *--sp = 0;                         /* rbp */
    *--sp = (u64)f;                    /* rbx */
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;   /* r12..r15 */
#endif
    f->sp = sp;
    f->done = 0;
}

void msl_run_group(msl_fn_t *f, msl_ctx_t *tmpl, const u32 tg[3], const u32 tptg[3], const u32 grid[3], int exact) {
    u32 n = tptg[0] * tptg[1] * tptg[2];
    if (!n) return;
    u8 *tgs = f->tgstatic ? calloc(1, f->tgstatic) : 0;
    msl_ctx_t c = *tmpl;
    c.tgstatic = tgs;
    for (int i = 0; i < 3; i++) { c.tgpig[i] = tg[i]; c.tptg[i] = tptg[i]; }
    int use_fibers = f->barrier && n > 1;
    if (!use_fibers) {
        for (u32 z = 0; z < tptg[2]; z++)
            for (u32 y = 0; y < tptg[1]; y++)
                for (u32 x = 0; x < tptg[0]; x++) {
                    c.tpitg[0] = x; c.tpitg[1] = y; c.tpitg[2] = z;
                    c.tpig[0] = tg[0] * tptg[0] + x; c.tpig[1] = tg[1] * tptg[1] + y; c.tpig[2] = tg[2] * tptg[2] + z;
                    if (exact && (c.tpig[0] >= grid[0] || c.tpig[1] >= grid[1] || c.tpig[2] >= grid[2])) continue;
                    c.tiitg = (z * tptg[1] + y) * tptg[0] + x;
                    c.discarded = 0;
                    msl_run(f, &c, 0);
                }
        free(tgs);
        return;
    }
    if (n > MAX_FIBERS) n = MAX_FIBERS;
    fiber_t *fs = calloc(n, sizeof *fs);
    u32 live = 0;
    for (u32 t = 0; t < n; t++) {
        u32 x = t % tptg[0], y = (t / tptg[0]) % tptg[1], z = t / (tptg[0] * tptg[1]);
        fiber_t *fb = &fs[live];
        fb->ctx = c;
        fb->ctx.tpitg[0] = x; fb->ctx.tpitg[1] = y; fb->ctx.tpitg[2] = z;
        fb->ctx.tpig[0] = tg[0] * tptg[0] + x; fb->ctx.tpig[1] = tg[1] * tptg[1] + y; fb->ctx.tpig[2] = tg[2] * tptg[2] + z;
        if (exact && (fb->ctx.tpig[0] >= grid[0] || fb->ctx.tpig[1] >= grid[1] || fb->ctx.tpig[2] >= grid[2])) continue;
        fb->ctx.tiitg = t;
        fb->fn = f;
        if (!g_stacks[live]) g_stacks[live] = malloc(FIBER_STACK);
        if (!g_stacks[live]) break;
        fiber_init(fb, g_stacks[live]);
        live++;
    }
    void *(*prev)(void) = g_yield;
    g_yield = fiber_yield;
    for (int alive = 1; alive;) {
        alive = 0;
        for (u32 t = 0; t < live; t++) {
            if (fs[t].done) continue;
            g_fib = &fs[t];
            msl_ctx_switch(&g_sched_sp, fs[t].sp);
            g_fib = 0;
            if (!fs[t].done) alive = 1;
        }
    }
    g_yield = prev;
    free(fs);
    free(tgs);
}

/* ================================================================ constantes / globais */
void msl_global_init(msl_lib_t *L, node_t *init, ty_t *t, void *dst) {
    fr_t fr = {0, &g_dummy_ctx, L, 0};
    if (IS_NUM(t)) { reg_t r; ev(init, &fr, &r); store(t, &r, dst); }
    else if (init->op == E_INIT) init_agg(init->v, init->nv, t, &fr, dst);
    else evm(init, &fr, dst);
}
static int is_const(node_t *n) {
    if (!n) return 1;
    switch (n->op) {
    case E_LIT: return 1;
    case E_VAR: return n->var->where == 1 && IS_NUM(n->t);
    case E_UN: case E_CAST: return is_const(n->a);
    case E_BIN: case E_LAND: case E_LOR: return is_const(n->a) && is_const(n->b);
    case E_COND: return is_const(n->a) && is_const(n->b) && is_const(n->c);
    case E_CTOR: case E_BI: for (int i = 0; i < n->nv; i++) if (!is_const(n->v[i])) return 0; return n->op == E_CTOR || (n->bi > 0 && n->bi < B_LAST_GEN);
    case E_SWZ: case E_MEMBER: return is_const(n->a);
    case E_INDEX: return is_const(n->a) && is_const(n->b);
    }
    return 0;
}
i64 msl_const_eval(msl_lib_t *L, node_t *n, int *ok) {
    if (!n->t || !IS_NUM(n->t) || !is_const(n)) { *ok = 0; return 0; }
    fr_t fr = {0, &g_dummy_ctx, L, 0};
    reg_t r;
    ev(n, &fr, &r);
    return ri(&r, 0);
}
