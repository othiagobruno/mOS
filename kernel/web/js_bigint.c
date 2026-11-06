/* mOS — motor JavaScript "Brisa": BigInt de precisão arbitrária (sinal + magnitude, limbs de 32 bits) */
#include "js_int.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags
#define BIG_MAX_LIMBS (1u << 16)    /* 2 Mbit */

/* ------------------------------------------------------------ magnitudes */
static int mag_norm(const u32 *a, int n) { while (n > 0 && !a[n - 1]) n--; return n; }

static int mag_cmp(const u32 *a, int an, const u32 *b, int bn) {
    if (an != bn) return an < bn ? -1 : 1;
    for (int i = an - 1; i >= 0; i--) if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}

static jsbig *mk(const u32 *d, int n, bool neg) {
    n = mag_norm(d, n);
    if ((u32)n > BIG_MAX_LIMBS) { js_throw_err(E_RANGE, "BigInt grande demais"); return NULL; }
    jsbig *b = gc_alloc(GT_BIG, sizeof(jsbig) + sizeof(u32) * (usize)n);
    b->n = (u32)n;
    b->neg = n ? neg : 0;
    if (n) memcpy(b->d, d, sizeof(u32) * (usize)n);
    return b;
}

/* out tem max(an,bn)+1 limbs */
static int mag_add(const u32 *a, int an, const u32 *b, int bn, u32 *out) {
    if (an < bn) { const u32 *t = a; a = b; b = t; int tn = an; an = bn; bn = tn; }
    u64 c = 0;
    for (int i = 0; i < an; i++) {
        c += (u64)a[i] + (i < bn ? b[i] : 0);
        out[i] = (u32)c;
        c >>= 32;
    }
    out[an] = (u32)c;
    return an + 1;
}

/* a >= b; out tem an limbs */
static int mag_sub(const u32 *a, int an, const u32 *b, int bn, u32 *out) {
    i64 br = 0;
    for (int i = 0; i < an; i++) {
        i64 v = (i64)a[i] - (i < bn ? b[i] : 0) - br;
        br = v < 0;
        out[i] = (u32)(v + (br ? (1ll << 32) : 0));
    }
    return mag_norm(out, an);
}

static int mag_mul(const u32 *a, int an, const u32 *b, int bn, u32 *out) {
    memset(out, 0, sizeof(u32) * (usize)(an + bn));
    for (int i = 0; i < an; i++) {
        u64 c = 0;
        for (int j = 0; j < bn; j++) {
            c += (u64)a[i] * b[j] + out[i + j];
            out[i + j] = (u32)c;
            c >>= 32;
        }
        out[i + bn] = (u32)c;
    }
    return mag_norm(out, an + bn);
}

/* divide por um limb; devolve o resto */
static u32 mag_divsmall(u32 *a, int n, u32 d) {
    u64 r = 0;
    for (int i = n - 1; i >= 0; i--) {
        u64 cur = r << 32 | a[i];
        a[i] = (u32)(cur / d);
        r = cur % d;
    }
    return (u32)r;
}

/* divisão longa bit a bit: q (an limbs) e r (bn+1 limbs) */
static void mag_divmod(const u32 *a, int an, const u32 *b, int bn, u32 *q, int *qn, u32 *r, int *rn) {
    memset(q, 0, sizeof(u32) * (usize)(an ? an : 1));
    memset(r, 0, sizeof(u32) * (usize)(bn + 1));
    if (bn == 1) {
        memcpy(q, a, sizeof(u32) * (usize)an);
        r[0] = mag_divsmall(q, an, b[0]);
        *qn = mag_norm(q, an);
        *rn = mag_norm(r, 1);
        return;
    }
    int rl = 0;
    for (int i = an * 32 - 1; i >= 0; i--) {
        /* r = r << 1 | bit */
        u32 carry = (a[i / 32] >> (i % 32)) & 1;
        for (int k = 0; k <= bn; k++) { u32 nc = r[k] >> 31; r[k] = r[k] << 1 | carry; carry = nc; }
        rl = mag_norm(r, bn + 1);
        if (mag_cmp(r, rl, b, bn) >= 0) {
            mag_sub(r, rl, b, bn, r);
            q[i / 32] |= 1u << (i % 32);
        }
    }
    *qn = mag_norm(q, an);
    *rn = mag_norm(r, bn + 1);
}

/* ------------------------------------------------------------ conversões */
jsbig *js_big_from_double(double d) {
    bool neg = d < 0;
    if (neg) d = -d;
    if (d < 18446744073709551616.0) {
        u64 v = (u64)d;
        u32 t[2] = {(u32)v, (u32)(v >> 32)};
        return mk(t, 2, neg);
    }
    union { double d; u64 u; } x;
    x.d = d;
    int e = (int)((x.u >> 52) & 0x7FF) - 1075;
    u64 m = (x.u & ((1ull << 52) - 1)) | (1ull << 52);
    int words = e / 32 + 3;
    u32 *t = kcalloc((usize)words, sizeof(u32));
    if (!t) return NULL;
    int w = e / 32, s = e % 32;
    u64 lo = m << s, hi = s ? m >> (64 - s) : 0;
    u32 parts[3] = {(u32)lo, (u32)(lo >> 32), (u32)hi};
    for (int i = 0; i < 3 && w + i < words; i++) t[w + i] = parts[i];
    jsbig *b = mk(t, words, neg);
    kfree(t);
    return b;
}

double js_big_to_double(jsbig *b) {
    double r = 0;
    for (int i = (int)b->n - 1; i >= 0; i--) r = r * 4294967296.0 + b->d[i];
    return b->neg ? -r : r;
}

static int digit_val(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if ((c | 32) >= 'a' && (c | 32) <= 'z') return (c | 32) - 'a' + 10;
    return 99;
}

jsbig *js_big_parse(const char *s, int len, int radix, bool *ok) {
    *ok = false;
    int i = 0;
    bool neg = false;
    if (radix == 0) {
        radix = 10;
        if (len > 2 && s[0] == '0' && ((s[1] | 32) == 'x' || (s[1] | 32) == 'o' || (s[1] | 32) == 'b')) {
            radix = (s[1] | 32) == 'x' ? 16 : (s[1] | 32) == 'o' ? 8 : 2;
            i = 2;
        } else if (len > 0 && (s[0] == '-' || s[0] == '+')) { neg = s[0] == '-'; i = 1; }
    }
    if (i >= len) return NULL;
    int cap = len / 4 + 2;
    u32 *t = kcalloc((usize)cap, sizeof(u32));
    if (!t) return NULL;
    int n = 0;
    for (; i < len; i++) {
        if (s[i] == '_') continue;
        int dv = digit_val((u8)s[i]);
        if (dv >= radix) { kfree(t); return NULL; }
        u64 c = (u64)dv;
        for (int k = 0; k < n; k++) { c += (u64)t[k] * (u32)radix; t[k] = (u32)c; c >>= 32; }
        if (c && n < cap) t[n++] = (u32)c;
    }
    jsbig *b = mk(t, n, neg);
    kfree(t);
    *ok = b != NULL;
    return b;
}

jsv js_big_from_str_value(jsstr *s) {
    int a = 0, e = (int)s->len;
    while (a < e && isspace((u8)s->d[a])) a++;
    while (e > a && isspace((u8)s->d[e - 1])) e--;
    if (a == e) return jv_big(mk(NULL, 0, false));
    for (int i = a; i < e; i++) if (s->d[i] == '_') return JS_UNDEF;
    bool ok;
    jsbig *b = js_big_parse(s->d + a, e - a, 0, &ok);
    return ok ? jv_big(b) : (js_pending() ? JS_EXC : JS_UNDEF);
}

jsstr *js_big_tostr(jsbig *b, int radix) {
    if (!b->n) return js_strz("0");
    u32 *t = kmalloc(sizeof(u32) * b->n);
    char *out = kmalloc((usize)b->n * 33 + 2);
    if (!t || !out) { kfree(t); kfree(out); return js_strz("?"); }
    memcpy(t, b->d, sizeof(u32) * b->n);
    int n = (int)b->n, k = 0;
    u32 chunk = (u32)radix;
    int per = 1;
    while ((u64)chunk * (u32)radix <= 0xFFFFFFFFu) { chunk *= (u32)radix; per++; }
    while (n > 0) {
        u32 r = mag_divsmall(t, n, chunk);
        n = mag_norm(t, n);
        for (int j = 0; j < per; j++) {
            out[k++] = "0123456789abcdefghijklmnopqrstuvwxyz"[r % (u32)radix];
            r /= (u32)radix;
            if (!n && !r) break;
        }
    }
    while (k > 1 && out[k - 1] == '0') k--;
    if (b->neg) out[k++] = '-';
    for (int i = 0; i < k / 2; i++) { char c = out[i]; out[i] = out[k - 1 - i]; out[k - 1 - i] = c; }
    jsstr *s = js_strn(out, k);
    kfree(t);
    kfree(out);
    return s;
}

/* ------------------------------------------------------------ comparação */
int js_big_cmp(jsbig *a, jsbig *b) {
    if (a->neg != b->neg) return a->neg ? -1 : 1;
    int c = mag_cmp(a->d, (int)a->n, b->d, (int)b->n);
    return a->neg ? -c : c;
}

int js_big_cmp_num(jsbig *a, double d) {
    if (d != d) return 2;
    if (!m_isfinite(d)) return d > 0 ? -1 : 1;
    double fl = m_floor(d);
    jsbig *f = js_big_from_double(fl);
    if (!f) return 2;
    int c = js_big_cmp(a, f);
    if (c) return c;
    return d > fl ? -1 : 0;
}

/* ------------------------------------------------------------ aritmética */
static jsv add_signed(jsbig *a, jsbig *b, bool bneg) {
    int n = (int)MAX(a->n, b->n) + 1;
    u32 *t = kmalloc(sizeof(u32) * (usize)n);
    if (!t) return js_throw_err(E_RANGE, "Memória insuficiente para BigInt");
    jsbig *r;
    if (a->neg == bneg) { int m = mag_add(a->d, (int)a->n, b->d, (int)b->n, t); r = mk(t, m, a->neg); }
    else {
        int c = mag_cmp(a->d, (int)a->n, b->d, (int)b->n);
        if (c >= 0) { int m = mag_sub(a->d, (int)a->n, b->d, (int)b->n, t); r = mk(t, m, a->neg); }
        else { int m = mag_sub(b->d, (int)b->n, a->d, (int)a->n, t); r = mk(t, m, bneg); }
    }
    kfree(t);
    return jv_big(r);
}

static jsv mul(jsbig *a, jsbig *b) {
    if (!a->n || !b->n) return jv_big(mk(NULL, 0, false));
    if (a->n + b->n > BIG_MAX_LIMBS) return js_throw_err(E_RANGE, "BigInt grande demais");
    u32 *t = kmalloc(sizeof(u32) * (a->n + b->n));
    if (!t) return js_throw_err(E_RANGE, "Memória insuficiente para BigInt");
    int m = mag_mul(a->d, (int)a->n, b->d, (int)b->n, t);
    jsbig *r = mk(t, m, a->neg != b->neg);
    kfree(t);
    return jv_big(r);
}

static jsv divmod(jsbig *a, jsbig *b, bool want_rem) {
    if (!b->n) return js_throw_err(E_RANGE, "Divisão de BigInt por zero");
    if (mag_cmp(a->d, (int)a->n, b->d, (int)b->n) < 0) return want_rem ? jv_big(a) : jv_big(mk(NULL, 0, false));
    u32 *q = kmalloc(sizeof(u32) * (a->n + 1)), *r = kmalloc(sizeof(u32) * (b->n + 2));
    if (!q || !r) { kfree(q); kfree(r); return js_throw_err(E_RANGE, "Memória insuficiente para BigInt"); }
    int qn, rn;
    mag_divmod(a->d, (int)a->n, b->d, (int)b->n, q, &qn, r, &rn);
    jsbig *res = want_rem ? mk(r, rn, a->neg) : mk(q, qn, a->neg != b->neg);
    kfree(q);
    kfree(r);
    return jv_big(res);
}

static jsv bpow(jsbig *a, jsbig *e) {
    if (e->neg) return js_throw_err(E_RANGE, "O expoente de BigInt deve ser não negativo");
    if (e->n > 1) {
        if (a->n == 0 || (a->n == 1 && a->d[0] == 1)) return jv_big(a->neg && (e->d[0] & 1) ? a : mk(a->d, (int)a->n, false));
        return js_throw_err(E_RANGE, "BigInt grande demais");
    }
    u32 ex = e->n ? e->d[0] : 0;
    u32 one = 1;
    jsv result = jv_big(mk(&one, 1, false)), base = jv_big(a);
    while (ex) {
        if (ex & 1) { result = mul(jv_b(result), jv_b(base)); if (result == JS_EXC) return JS_EXC; }
        ex >>= 1;
        if (ex) { base = mul(jv_b(base), jv_b(base)); if (base == JS_EXC) return JS_EXC; }
    }
    return result;
}

static jsv shift(jsbig *a, jsbig *s, bool left) {
    if (s->neg) { left = !left; }
    if (s->n > 1 || (s->n && s->d[0] > BIG_MAX_LIMBS * 32)) {
        if (!left) return jv_big(mk(a->neg ? (u32[]){1} : NULL, a->neg ? 1 : 0, a->neg));
        return js_throw_err(E_RANGE, "BigInt grande demais");
    }
    u32 sh = s->n ? s->d[0] : 0;
    u32 two = 2;
    jsv p = bpow(mk(&two, 1, false), mk(&sh, 1, false));
    if (p == JS_EXC) return JS_EXC;
    if (left) return mul(a, jv_b(p));
    /* deslocamento à direita arredonda para baixo (−∞) */
    jsv q = divmod(a, jv_b(p), false);
    if (q == JS_EXC || !a->neg) return q;
    jsv r = divmod(a, jv_b(p), true);
    if (r == JS_EXC) return JS_EXC;
    if (jv_b(r)->n) return js_big_add_small(jv_b(q), -1);
    return q;
}

jsv js_big_add_small(jsbig *a, int k) {
    u32 v = (u32)(k < 0 ? -k : k);
    jsbig *b = mk(&v, 1, k < 0);
    return add_signed(a, b, b->neg);
}

jsv js_big_neg(jsbig *a) { return jv_big(mk(a->d, (int)a->n, !a->neg)); }

jsv js_big_binop(int op, jsbig *a, jsbig *b) {
    switch (op) {
    case OP_ADD: return add_signed(a, b, b->neg);
    case OP_SUB: return add_signed(a, b, !b->neg && b->n);
    case OP_MUL: return mul(a, b);
    case OP_DIV: return divmod(a, b, false);
    case OP_MOD: return divmod(a, b, true);
    case OP_EXP: return bpow(a, b);
    case OP_SHL: return shift(a, b, true);
    case OP_SHR: return shift(a, b, false);
    case OP_USHR: return js_throw_err(E_TYPE, "BigInts não têm deslocamento sem sinal (>>>)");
    }
    return js_throw_err(E_TYPE, "Operador bit a bit não suportado para BigInt");
}

/* ------------------------------------------------------------ biblioteca */
NATIVE(big_ctor) {
    UNUSED_ARGS;
    if (flags & NF_NEW) return js_throw_err(E_TYPE, "BigInt não é um construtor");
    jsv v = js_toprim(ARG(0), 1);
    if (v == JS_EXC) return JS_EXC;
    if (jv_isbig(v)) return v;
    if (jv_isnum(v)) {
        double d = jv_n(v);
        if (!m_isfinite(d) || m_trunc(d) != d) {
            char b[40];
            js_numfmt(d, b, sizeof b);
            return js_throw_err(E_RANGE, "O número %s não pode ser convertido em BigInt porque não é um inteiro", b);
        }
        return jv_big(js_big_from_double(d));
    }
    if (jv_isbool(v)) { u32 one = 1; return jv_big(mk(&one, v == JS_TRUE, false)); }
    if (jv_isstr(v) && !jv_s(v)->sym) {
        jsv r = js_big_from_str_value(jv_s(v));
        if (r == JS_UNDEF) return js_throw_err(E_SYNTAX, "Não é possível converter '%s' em BigInt", jv_s(v)->d);
        return r;
    }
    return js_throw_err(E_TYPE, "Não é possível converter %s em BigInt", v == JS_UNDEF ? "undefined" : v == JS_NULL ? "null" : "Symbol");
}

static jsbig *this_big(jsv thisv) {
    if (jv_isbig(thisv)) return jv_b(thisv);
    if (jv_iscls(thisv, CL_BIGINT)) return jv_b(jv_o(thisv)->u.prim);
    js_throw_err(E_TYPE, "BigInt.prototype chamado em algo que não é BigInt");
    return NULL;
}

NATIVE(bigp_tostring) {
    UNUSED_ARGS;
    jsbig *b = this_big(thisv);
    if (!b) return JS_EXC;
    if (self->u.nat.magic == 1) return jv_big(b);
    int radix = 10;
    if (self->u.nat.magic == 0 && ARG(0) != JS_UNDEF) {
        double r = js_tointeger(ARG(0));
        if (js_pending()) return JS_EXC;
        if (r < 2 || r > 36) return js_throw_err(E_RANGE, "toString() radix deve estar entre 2 e 36");
        radix = (int)r;
    }
    return jv_str(js_big_tostr(b, radix));
}

void js_bigint_init(jsrealm *r) {
    jsobj *bp = js_newobj(r->p_object);
    r->p_bigint = bp;
    jsobj *c = js_newctor(big_ctor, "BigInt", 1, bp);
    js_defn(r->global, "BigInt", jv_obj(c), PF_NOENUM);
    js_defnative(bp, "toString", bigp_tostring, 0, 0);
    js_defnative(bp, "valueOf", bigp_tostring, 0, 1);
    js_defnative(bp, "toLocaleString", bigp_tostring, 0, 2);
    js_defprop(bp, A[A_sym_toStringTag], jv_str(js_intern("BigInt", -1)), PF_NOENUM | PF_RO);
}
