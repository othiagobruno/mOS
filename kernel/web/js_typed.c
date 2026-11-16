/* mOS — motor JavaScript "Brisa": ArrayBuffer, typed arrays (Int8 ... BigUint64), DataView, TextEncoder e TextDecoder.
 * Os elementos ficam em little-endian (x86_64 e arm64); o núcleo (js_core.c) desvia os índices inteiros para
 * js_typed_get/js_typed_set. Os métodos só de leitura vêm do Array.prototype (genéricos), os que criam ou mudam
 * vetores são próprios e devolvem o mesmo tipo. */
#include "js_int.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags
#define TRYV(x) do { if ((x) == JS_EXC) return JS_EXC; } while (0)

#define ABUF_MAX (1u << 30)

static const char *ta_names[TA_COUNT] = {"Int8Array", "Uint8Array", "Uint8ClampedArray", "Int16Array", "Uint16Array",
                                         "Int32Array", "Uint32Array", "Float32Array", "Float64Array", "BigInt64Array",
                                         "BigUint64Array"};
static const u8 ta_size[TA_COUNT] = {1, 1, 1, 2, 2, 4, 4, 4, 8, 8, 8};

int js_typed_elsize(int kind) { return kind >= 0 && kind < TA_COUNT ? ta_size[kind] : 1; }
static bool is_big(int kind) { return kind == TA_BIGINT64 || kind == TA_BIGUINT64; }

/* ---------------------------------------------------------------- ArrayBuffer */
jsobj *js_abuf_new(u32 len) {
    jsobj *b = js_newobj_cls(CL_ARRAYBUFFER, J.realm->p_abuf);
    b->u.buf.data = kcalloc(len ? len : 1, 1);
    if (!b->u.buf.data) { js_throw_err(E_RANGE, "Sem memória para um ArrayBuffer de %u bytes", len); return NULL; }
    b->u.buf.len = len;
    return b;
}

jsobj *js_abuf_external(void *bytes, u32 len, void (*dealloc)(void *, void *), void *ctx) {
    jsobj *b = js_newobj_cls(CL_ARRAYBUFFER, J.realm->p_abuf);
    b->u.buf.data = bytes;
    b->u.buf.len = len;
    b->u.buf.dealloc = dealloc;
    b->u.buf.dctx = ctx;
    if (!dealloc) b->u.buf.detached = false;
    return b;
}

static u8 *buf_bytes(jsobj *b) { return b && b->cls == CL_ARRAYBUFFER && !b->u.buf.detached ? b->u.buf.data : NULL; }
static u32 buf_len(jsobj *b) { return b && b->cls == CL_ARRAYBUFFER && !b->u.buf.detached ? b->u.buf.len : 0; }

/* ---------------------------------------------------------------- typed arrays: elementos */
jsobj *js_typed_new(int kind, jsobj *buffer, u32 off, u32 len) {
    jsobj *t = js_newobj_cls(CL_TYPED, J.realm->p_ta[kind]);
    t->u.ta.buffer = buffer;
    t->u.ta.off = off;
    t->u.ta.len = len;
    t->u.ta.kind = (u8)kind;
    return t;
}

u32 js_typed_length(jsobj *t) {
    u32 bl = buf_len(t->u.ta.buffer);
    u32 es = ta_size[t->u.ta.kind];
    if (t->u.ta.off > bl) return 0;
    if (t->u.ta.tracking) return (bl - t->u.ta.off) / es;
    if ((u64)t->u.ta.off + (u64)t->u.ta.len * es > bl) return 0;   /* buffer encolheu/destacado */
    return t->u.ta.len;
}

static u8 *el_ptr(jsobj *t, u32 i) {
    if (i >= js_typed_length(t)) return NULL;
    return buf_bytes(t->u.ta.buffer) + t->u.ta.off + (usize)i * ta_size[t->u.ta.kind];
}

static jsv read_el(int kind, const u8 *p) {
    switch (kind) {
    case TA_INT8: return jv_num((i8)p[0]);
    case TA_UINT8: case TA_UINT8C: return jv_num(p[0]);
    case TA_INT16: { i16 v; memcpy(&v, p, 2); return jv_num(v); }
    case TA_UINT16: { u16 v; memcpy(&v, p, 2); return jv_num(v); }
    case TA_INT32: { i32 v; memcpy(&v, p, 4); return jv_num(v); }
    case TA_UINT32: { u32 v; memcpy(&v, p, 4); return jv_num(v); }
    case TA_FLOAT32: { float v; memcpy(&v, p, 4); double d = v; return d != d ? (jsv)JS_NAN_BITS : jv_num(d); }
    case TA_FLOAT64: { double v; memcpy(&v, p, 8); return v != v ? (jsv)JS_NAN_BITS : jv_num(v); }
    case TA_BIGINT64: { i64 v; memcpy(&v, p, 8); return v < 0 ? js_big_from_u64((u64)0 - (u64)v, true) : js_big_from_u64((u64)v, false); }
    case TA_BIGUINT64: { u64 v; memcpy(&v, p, 8); return js_big_from_u64(v, false); }
    }
    return JS_UNDEF;
}

static jsv jv_n_ok(jsv v) { return v; }

/* ToBigInt para os vetores de 64 bits */
static bool to_big64(jsv v, u64 *out) {
    jsv p = js_toprim(v, 1);
    if (p == JS_EXC) return false;
    if (jv_isbig(p)) { *out = js_big_to_u64(jv_b(p)); return true; }
    if (p == JS_TRUE || p == JS_FALSE) { *out = p == JS_TRUE; return true; }
    if (jv_isstr(p)) {
        jsv b = js_big_from_str_value(jv_s(p));
        if (b == JS_EXC) return false;
        if (!jv_isbig(b)) { js_throw_err(E_SYNTAX, "Não é possível converter \"%s\" em BigInt", jv_s(p)->d); return false; }
        *out = js_big_to_u64(jv_b(b));
        return true;
    }
    js_throw_err(E_TYPE, "Não é possível converter %s em BigInt", jv_isnum(p) ? "um número" : "esse valor");
    return false;
}

static u8 clamp_u8(double d) {
    if (!(d > 0)) return 0;          /* NaN e negativos */
    if (d >= 255) return 255;
    double f = m_floor(d);
    double r = d - f;
    u32 i = (u32)f;
    if (r > 0.5 || (r == 0.5 && (i & 1))) i++;
    return (u8)i;
}

/* escreve v convertido (a conversão pode lançar) */
static int write_el(int kind, u8 *p, jsv v) {
    if (is_big(kind)) {
        u64 m;
        if (!to_big64(v, &m)) return -1;
        if (p) memcpy(p, &m, 8);
        return 0;
    }
    double d = js_tonum(v);
    if (js_pending()) return -1;
    if (!p) return 0;
    switch (kind) {
    case TA_INT8: case TA_UINT8: { u8 b = (u8)js_touint32(jv_num(d)); p[0] = b; break; }
    case TA_UINT8C: p[0] = clamp_u8(d); break;
    case TA_INT16: case TA_UINT16: { u16 x = (u16)js_touint32(jv_num(d)); memcpy(p, &x, 2); break; }
    case TA_INT32: case TA_UINT32: { u32 x = js_touint32(jv_num(d)); memcpy(p, &x, 4); break; }
    case TA_FLOAT32: { float f = (float)d; memcpy(p, &f, 4); break; }
    case TA_FLOAT64: memcpy(p, &d, 8); break;
    }
    return 0;
}

jsv js_typed_get(jsobj *t, u32 i) {
    u8 *p = el_ptr(t, i);
    return p ? read_el(t->u.ta.kind, p) : JS_UNDEF;
}

int js_typed_set(jsobj *t, u32 i, jsv v) {
    /* a conversão acontece antes de saber se o índice vale (ela pode ter efeitos), como na especificação */
    if (is_big(t->u.ta.kind)) {
        u64 m;
        if (!to_big64(v, &m)) return -1;
        u8 *p = el_ptr(t, i);
        if (p) memcpy(p, &m, 8);
        return 0;
    }
    double d = js_tonum(v);
    if (js_pending()) return -1;
    u8 *p = el_ptr(t, i);
    if (p) write_el(t->u.ta.kind, p, jv_num(d));
    return 0;
}

static jsobj *this_ta(jsv thisv, const char *m) {
    if (!jv_iscls(thisv, CL_TYPED)) { js_throw_err(E_TYPE, "%s: o objeto não é um TypedArray", m); return NULL; }
    return jv_o(thisv);
}

static double rel_index(jsv v, double len, double def) {
    if (v == JS_UNDEF) return def;
    double r = js_tointeger(v);
    if (r < 0) { r += len; if (r < 0) r = 0; }
    else if (r > len) r = len;
    return r;
}

/* cria um typed array do mesmo tipo de t (ou de kind) com n elementos zerados */
static jsobj *alloc_ta(int kind, u32 n) {
    if ((u64)n * ta_size[kind] > ABUF_MAX) { js_throw_err(E_RANGE, "Comprimento de TypedArray grande demais"); return NULL; }
    jsobj *b = js_abuf_new(n * ta_size[kind]);
    if (!b) return NULL;
    return js_typed_new(kind, b, 0, n);
}

/* preenche um vetor novo a partir de um iterável ou array-like */
static jsobj *ta_from_values(int kind, jsv src, jsv mapfn, jsv thisarg) {
    jsobj *vals = NULL;
    jsv itf = jv_isobj(src) ? js_getk(src, A[A_sym_iterator]) : JS_UNDEF;
    if (itf == JS_EXC) return NULL;
    if (js_iscallable(itf) && !jv_iscls(src, CL_TYPED)) {
        vals = js_newarr(0);
        jsv it = js_iter_open(src);
        if (it == JS_EXC) return NULL;
        for (;;) {
            jsv e;
            int r = js_iter_step(it, &e);
            if (r < 0) return NULL;
            if (!r) break;
            js_arr_push(vals, e);
        }
    }
    u32 n;
    if (vals) n = vals->alen;
    else {
        jsv o = js_toobj(src);
        if (o == JS_EXC) return NULL;
        src = o;
        n = js_lengthof(o);
        if (js_pending()) return NULL;
    }
    jsobj *t = alloc_ta(kind, n);
    if (!t) return NULL;
    for (u32 i = 0; i < n; i++) {
        jsv v = vals ? vals->arr[i] : js_getidx(src, i);
        if (v == JS_EXC) return NULL;
        if (js_iscallable(mapfn)) {
            jsv a[2] = {v, jv_num(i)};
            v = js_call(mapfn, thisarg, 2, a);
            if (v == JS_EXC) return NULL;
        }
        if (js_typed_set(t, i, v) < 0) return NULL;
    }
    return t;
}

/* new XxxArray(length | typedArray | arrayBuffer[, byteOffset[, length]] | iterável/array-like) */
NATIVE(ta_ctor) {
    UNUSED_ARGS;
    int kind = self->u.nat.magic;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor %s exige 'new'", ta_names[kind]);
    jsobj *proto = jv_isobj(thisv) && jv_o(thisv)->proto ? jv_o(thisv)->proto : J.realm->p_ta[kind];
    jsv a0 = ARG(0);
    jsobj *t = NULL;
    u32 es = ta_size[kind];
    if (!jv_isobj(a0)) {
        double n = a0 == JS_UNDEF ? 0 : js_tointeger(a0);
        if (js_pending()) return JS_EXC;
        if (n < 0 || n * es > ABUF_MAX) return js_throw_err(E_RANGE, "Comprimento de %s inválido", ta_names[kind]);
        t = alloc_ta(kind, (u32)n);
    } else if (jv_iscls(a0, CL_ARRAYBUFFER)) {
        jsobj *b = jv_o(a0);
        if (b->u.buf.detached) return js_throw_err(E_TYPE, "O ArrayBuffer foi destacado");
        double off = js_tointeger(ARG(1));
        if (js_pending()) return JS_EXC;
        if (off < 0 || (u64)off % es) return js_throw_err(E_RANGE, "O deslocamento de %s deve ser múltiplo de %u", ta_names[kind], es);
        u32 bl = b->u.buf.len;
        if (off > bl) return js_throw_err(E_RANGE, "Deslocamento além do fim do ArrayBuffer");
        t = js_typed_new(kind, b, (u32)off, 0);
        if (ARG(2) == JS_UNDEF) {
            if ((bl - (u32)off) % es) return js_throw_err(E_RANGE, "O tamanho do ArrayBuffer menos o deslocamento deve ser múltiplo de %u", es);
            t->u.ta.len = (bl - (u32)off) / es;
        } else {
            double n = js_tointeger(ARG(2));
            if (js_pending()) return JS_EXC;
            if (n < 0 || off + n * es > bl) return js_throw_err(E_RANGE, "Comprimento além do fim do ArrayBuffer");
            t->u.ta.len = (u32)n;
        }
    } else if (jv_iscls(a0, CL_TYPED)) {
        jsobj *s = jv_o(a0);
        u32 n = js_typed_length(s);
        if (is_big(kind) != is_big(s->u.ta.kind)) return js_throw_err(E_TYPE, "Não dá para misturar BigInt e Number em typed arrays");
        t = alloc_ta(kind, n);
        if (!t) return JS_EXC;
        for (u32 i = 0; i < n; i++) if (js_typed_set(t, i, js_typed_get(s, i)) < 0) return JS_EXC;
    } else t = ta_from_values(kind, a0, JS_UNDEF, JS_UNDEF);
    if (!t) return JS_EXC;
    t->proto = proto;
    return jv_obj(t);
}

NATIVE(ta_static_from) {
    UNUSED_ARGS;
    int kind = self->u.nat.magic;
    jsv mapfn = ARG(1);
    if (mapfn != JS_UNDEF && !js_iscallable(mapfn)) return js_throw_err(E_TYPE, "%s.from: o segundo argumento não é uma função", ta_names[kind]);
    jsobj *t = ta_from_values(kind, ARG(0), mapfn, ARG(2));
    return t ? jv_obj(t) : JS_EXC;
}

NATIVE(ta_static_of) {
    UNUSED_ARGS;
    int kind = self->u.nat.magic;
    jsobj *t = alloc_ta(kind, (u32)argc);
    if (!t) return JS_EXC;
    for (int i = 0; i < argc; i++) if (js_typed_set(t, (u32)i, argv[i]) < 0) return JS_EXC;
    return jv_obj(t);
}

/* getters: buffer, byteLength, byteOffset, length, [Symbol.toStringTag] */
NATIVE(ta_getter) {
    UNUSED_ARGS;
    if (self->u.nat.magic == 4) return jv_iscls(thisv, CL_TYPED) ? jv_str(js_intern(ta_names[jv_o(thisv)->u.ta.kind], -1)) : JS_UNDEF;
    jsobj *t = this_ta(thisv, "TypedArray");
    if (!t) return JS_EXC;
    u32 n = js_typed_length(t);
    switch (self->u.nat.magic) {
    case 0: return jv_obj(t->u.ta.buffer);
    case 1: return jv_num((double)n * ta_size[t->u.ta.kind]);
    case 2: return jv_num(n ? t->u.ta.off : 0);
    default: return jv_num(n);
    }
}

/* set(fonte[, deslocamento]) */
NATIVE(ta_set) {
    UNUSED_ARGS;
    jsobj *t = this_ta(thisv, "set");
    if (!t) return JS_EXC;
    double off = js_tointeger(ARG(1));
    if (js_pending()) return JS_EXC;
    if (off < 0) return js_throw_err(E_RANGE, "Deslocamento negativo");
    u32 len = js_typed_length(t);
    jsv src = ARG(0);
    if (jv_iscls(src, CL_TYPED)) {
        jsobj *s = jv_o(src);
        u32 n = js_typed_length(s);
        if (off + n > len) return js_throw_err(E_RANGE, "A fonte não cabe no TypedArray");
        if (is_big(t->u.ta.kind) != is_big(s->u.ta.kind)) return js_throw_err(E_TYPE, "Não dá para misturar BigInt e Number em typed arrays");
        /* mesma memória: copia por um vetor intermediário */
        jsobj *tmp = js_newarr(0);
        for (u32 i = 0; i < n; i++) js_arr_push(tmp, js_typed_get(s, i));
        for (u32 i = 0; i < n; i++) if (js_typed_set(t, (u32)off + i, tmp->arr[i]) < 0) return JS_EXC;
        return JS_UNDEF;
    }
    jsv o = js_toobj(src);
    TRYV(o);
    u32 n = js_lengthof(o);
    if (js_pending()) return JS_EXC;
    if (off + n > len) return js_throw_err(E_RANGE, "A fonte não cabe no TypedArray");
    for (u32 i = 0; i < n; i++) {
        jsv v = js_getidx(o, i);
        TRYV(v);
        if (js_typed_set(t, (u32)off + i, v) < 0) return JS_EXC;
    }
    return JS_UNDEF;
}

/* subarray(início, fim): mesma memória */
NATIVE(ta_subarray) {
    UNUSED_ARGS;
    jsobj *t = this_ta(thisv, "subarray");
    if (!t) return JS_EXC;
    double len = js_typed_length(t);
    double b = rel_index(ARG(0), len, 0), e = rel_index(ARG(1), len, len);
    if (js_pending()) return JS_EXC;
    u32 n = e > b ? (u32)(e - b) : 0;
    jsobj *r = js_typed_new(t->u.ta.kind, t->u.ta.buffer, t->u.ta.off + (u32)b * ta_size[t->u.ta.kind], n);
    r->proto = t->proto;
    return jv_obj(r);
}

/* slice(início, fim): cópia */
NATIVE(ta_slice) {
    UNUSED_ARGS;
    jsobj *t = this_ta(thisv, "slice");
    if (!t) return JS_EXC;
    double len = js_typed_length(t);
    double b = rel_index(ARG(0), len, 0), e = rel_index(ARG(1), len, len);
    if (js_pending()) return JS_EXC;
    u32 n = e > b ? (u32)(e - b) : 0;
    jsobj *r = alloc_ta(t->u.ta.kind, n);
    if (!r) return JS_EXC;
    if (n) memcpy(buf_bytes(r->u.ta.buffer), el_ptr(t, (u32)b), (usize)n * ta_size[t->u.ta.kind]);
    r->proto = t->proto;
    return jv_obj(r);
}

NATIVE(ta_fill) {
    UNUSED_ARGS;
    jsobj *t = this_ta(thisv, "fill");
    if (!t) return JS_EXC;
    double len = js_typed_length(t);
    u8 tmp[8];
    if (write_el(t->u.ta.kind, tmp, ARG(0)) < 0) return JS_EXC;
    double b = rel_index(ARG(1), len, 0), e = rel_index(ARG(2), len, len);
    if (js_pending()) return JS_EXC;
    for (u32 i = (u32)b; i < (u32)e; i++) { u8 *p = el_ptr(t, i); if (p) memcpy(p, tmp, ta_size[t->u.ta.kind]); }
    return thisv;
}

NATIVE(ta_reverse) {
    UNUSED_ARGS;
    jsobj *t = this_ta(thisv, "reverse");
    if (!t) return JS_EXC;
    u32 n = js_typed_length(t), es = ta_size[t->u.ta.kind];
    for (u32 i = 0; i < n / 2; i++) {
        u8 *a = el_ptr(t, i), *b = el_ptr(t, n - 1 - i), x[8];
        memcpy(x, a, es); memcpy(a, b, es); memcpy(b, x, es);
    }
    return thisv;
}

NATIVE(ta_copywithin) {
    UNUSED_ARGS;
    jsobj *t = this_ta(thisv, "copyWithin");
    if (!t) return JS_EXC;
    double len = js_typed_length(t);
    double to = rel_index(ARG(0), len, 0), from = rel_index(ARG(1), len, 0), fin = rel_index(ARG(2), len, len);
    if (js_pending()) return JS_EXC;
    double cnt = fin - from;
    if (len - to < cnt) cnt = len - to;
    if (cnt > 0) memmove(el_ptr(t, (u32)to), el_ptr(t, (u32)from), (usize)cnt * ta_size[t->u.ta.kind]);
    return thisv;
}

/* map/filter devolvem o mesmo tipo */
NATIVE(ta_mapfilter) {
    UNUSED_ARGS;
    jsobj *t = this_ta(thisv, self->u.nat.magic ? "filter" : "map");
    if (!t) return JS_EXC;
    jsv fn = ARG(0);
    if (!js_iscallable(fn)) return js_throw_err(E_TYPE, "O callback não é uma função");
    u32 n = js_typed_length(t);
    if (!self->u.nat.magic) {
        jsobj *r = alloc_ta(t->u.ta.kind, n);
        if (!r) return JS_EXC;
        for (u32 i = 0; i < n; i++) {
            jsv a[3] = {js_typed_get(t, i), jv_num(i), thisv};
            jsv v = js_call(fn, ARG(1), 3, a);
            TRYV(v);
            if (js_typed_set(r, i, v) < 0) return JS_EXC;
        }
        return jv_obj(r);
    }
    jsobj *keep = js_newarr(0);
    for (u32 i = 0; i < n; i++) {
        jsv e = js_typed_get(t, i);
        jsv a[3] = {e, jv_num(i), thisv};
        jsv v = js_call(fn, ARG(1), 3, a);
        TRYV(v);
        if (js_truthy(v)) js_arr_push(keep, e);
    }
    jsobj *r = alloc_ta(t->u.ta.kind, keep->alen);
    if (!r) return JS_EXC;
    for (u32 i = 0; i < keep->alen; i++) js_typed_set(r, i, keep->arr[i]);
    return jv_obj(r);
}

/* sort: numérico por padrão (NaN no fim), ou com a função de comparação */
static jsv g_cmp;
static bool g_cmp_err;
static int cmp_vals(jsv a, jsv b) {
    if (g_cmp_err) return 0;
    if (js_iscallable(g_cmp)) {
        jsv args[2] = {a, b};
        jsv r = js_call(g_cmp, JS_UNDEF, 2, args);
        if (r == JS_EXC) { g_cmp_err = true; return 0; }
        double d = js_tonum(r);
        return d < 0 ? -1 : d > 0 ? 1 : 0;
    }
    if (jv_isbig(a)) return js_big_cmp(jv_b(a), jv_b(b));
    double x = jv_n(a), y = jv_n(b);
    if (x != x) return y != y ? 0 : 1;
    if (y != y) return -1;
    if (x < y) return -1;
    if (x > y) return 1;
    if (x == 0 && y == 0) { bool nx = 1 / x < 0, ny = 1 / y < 0; return nx == ny ? 0 : nx ? -1 : 1; }
    return 0;
}
NATIVE(ta_sort) {
    UNUSED_ARGS;
    jsobj *t = this_ta(thisv, "sort");
    if (!t) return JS_EXC;
    if (ARG(0) != JS_UNDEF && !js_iscallable(ARG(0))) return js_throw_err(E_TYPE, "A comparação não é uma função");
    u32 n = js_typed_length(t);
    jsobj *v = js_newarr(0);
    for (u32 i = 0; i < n; i++) js_arr_push(v, js_typed_get(t, i));
    jsv saved = g_cmp;
    g_cmp = ARG(0);
    g_cmp_err = false;
    /* ordenação estável por inserção binária + mescla: n pequeno na prática; usa mescla de baixo para cima */
    jsv *a = v->arr, *tmp = kmalloc(sizeof(jsv) * (n ? n : 1));
    if (!tmp) { g_cmp = saved; return js_throw_err(E_RANGE, "Sem memória"); }
    for (u32 w = 1; w < n && !g_cmp_err; w *= 2)
        for (u32 lo = 0; lo < n && !g_cmp_err; lo += 2 * w) {
            u32 mid = lo + w < n ? lo + w : n, hi = lo + 2 * w < n ? lo + 2 * w : n, i = lo, j = mid, k = lo;
            while (i < mid && j < hi) tmp[k++] = cmp_vals(a[j], a[i]) < 0 ? a[j++] : a[i++];
            while (i < mid) tmp[k++] = a[i++];
            while (j < hi) tmp[k++] = a[j++];
            for (u32 q = lo; q < hi; q++) a[q] = tmp[q];
        }
    kfree(tmp);
    g_cmp = saved;
    if (g_cmp_err) return JS_EXC;
    for (u32 i = 0; i < n; i++) js_typed_set(t, i, a[i]);
    return thisv;
}

NATIVE(ta_iter) {
    UNUSED_ARGS;
    if (!this_ta(thisv, "values")) return JS_EXC;
    return jv_obj(js_new_arr_iter(thisv, self->u.nat.magic));
}

/* ---------------------------------------------------------------- ArrayBuffer: construtor e métodos */
NATIVE(abuf_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor ArrayBuffer exige 'new'");
    double n = js_tointeger(ARG(0));
    if (js_pending()) return JS_EXC;
    if (n < 0 || n > ABUF_MAX) return js_throw_err(E_RANGE, "Comprimento de ArrayBuffer inválido");
    jsobj *b = js_abuf_new((u32)n);
    if (!b) return JS_EXC;
    if (jv_isobj(thisv) && jv_o(thisv)->proto) b->proto = jv_o(thisv)->proto;
    return jv_obj(b);
}
NATIVE(abuf_isview) { UNUSED_ARGS; return jv_bool(jv_iscls(ARG(0), CL_TYPED) || jv_iscls(ARG(0), CL_DATAVIEW)); }
NATIVE(abuf_bytelength) {
    UNUSED_ARGS;
    if (!jv_iscls(thisv, CL_ARRAYBUFFER)) return js_throw_err(E_TYPE, "byteLength: não é um ArrayBuffer");
    return jv_num(buf_len(jv_o(thisv)));
}
NATIVE(abuf_slice) {
    UNUSED_ARGS;
    if (!jv_iscls(thisv, CL_ARRAYBUFFER)) return js_throw_err(E_TYPE, "slice: não é um ArrayBuffer");
    jsobj *b = jv_o(thisv);
    double len = buf_len(b);
    double s = rel_index(ARG(0), len, 0), e = rel_index(ARG(1), len, len);
    if (js_pending()) return JS_EXC;
    u32 n = e > s ? (u32)(e - s) : 0;
    jsobj *r = js_abuf_new(n);
    if (!r) return JS_EXC;
    if (n) memcpy(r->u.buf.data, b->u.buf.data + (u32)s, n);
    return jv_obj(r);
}

/* ---------------------------------------------------------------- DataView */
NATIVE(dv_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor DataView exige 'new'");
    if (!jv_iscls(ARG(0), CL_ARRAYBUFFER)) return js_throw_err(E_TYPE, "DataView precisa de um ArrayBuffer");
    jsobj *b = jv_o(ARG(0));
    double off = js_tointeger(ARG(1));
    if (js_pending()) return JS_EXC;
    u32 bl = buf_len(b);
    if (off < 0 || off > bl) return js_throw_err(E_RANGE, "Deslocamento do DataView fora do ArrayBuffer");
    double n = ARG(2) == JS_UNDEF ? bl - off : js_tointeger(ARG(2));
    if (js_pending()) return JS_EXC;
    if (n < 0 || off + n > bl) return js_throw_err(E_RANGE, "Comprimento do DataView fora do ArrayBuffer");
    jsobj *d = js_newobj_cls(CL_DATAVIEW, jv_isobj(thisv) && jv_o(thisv)->proto ? jv_o(thisv)->proto : J.realm->p_dview);
    d->u.ta.buffer = b;
    d->u.ta.off = (u32)off;
    d->u.ta.len = (u32)n;
    return jv_obj(d);
}
NATIVE(dv_getter) {
    UNUSED_ARGS;
    if (!jv_iscls(thisv, CL_DATAVIEW)) return js_throw_err(E_TYPE, "não é um DataView");
    jsobj *d = jv_o(thisv);
    switch (self->u.nat.magic) {
    case 0: return jv_obj(d->u.ta.buffer);
    case 1: return jv_num(d->u.ta.len);
    default: return jv_num(d->u.ta.off);
    }
}
/* getX(off, little) / setX(off, valor, little): magic = tipo | 16 para set */
NATIVE(dv_access) {
    UNUSED_ARGS;
    if (!jv_iscls(thisv, CL_DATAVIEW)) return js_throw_err(E_TYPE, "não é um DataView");
    jsobj *d = jv_o(thisv);
    int kind = self->u.nat.magic & 15;
    bool set = self->u.nat.magic & 16;
    double off = js_tointeger(ARG(0));
    if (js_pending()) return JS_EXC;
    jsv val = set ? ARG(1) : JS_UNDEF;
    bool little = js_truthy(set ? ARG(2) : ARG(1));
    u32 es = ta_size[kind];
    u8 tmp[8];
    if (set && write_el(kind, tmp, val) < 0) return JS_EXC;
    u8 *base = buf_bytes(d->u.ta.buffer);
    if (!base) return js_throw_err(E_TYPE, "O ArrayBuffer foi destacado");
    if (off < 0 || off + es > d->u.ta.len) return js_throw_err(E_RANGE, "Posição fora do DataView");
    u8 *p = base + d->u.ta.off + (u32)off;
    if (set) {
        for (u32 i = 0; i < es; i++) p[i] = little ? tmp[i] : tmp[es - 1 - i];
        return JS_UNDEF;
    }
    for (u32 i = 0; i < es; i++) tmp[i] = little ? p[i] : p[es - 1 - i];
    return read_el(kind, tmp);
}

/* ---------------------------------------------------------------- TextEncoder / TextDecoder (UTF-8) */
NATIVE(te_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor TextEncoder exige 'new'");
    js_defn(jv_o(thisv), "encoding", jv_str(js_intern("utf-8", -1)), PF_RO);
    return thisv;
}
NATIVE(te_encode) {
    UNUSED_ARGS;
    jsstr *s = ARG(0) == JS_UNDEF ? A[A_empty] : js_tostr(ARG(0));
    if (!s) return JS_EXC;
    /* as strings são WTF-8: surrogates soltos viram U+FFFD, como pede o UTF-8 de verdade */
    jsobj *t = alloc_ta(TA_UINT8, 0);
    if (!t) return JS_EXC;
    u8 *out = kmalloc(s->len * 3 + 1);
    if (!out) return js_throw_err(E_RANGE, "Sem memória");
    int n = 0;
    for (int i = 0; i < (int)s->len;) {
        u32 cp = js_utf8_get(s->d, (int)s->len, &i);
        if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;
        n += js_utf8_put((char *)out + n, cp);
    }
    jsobj *b = js_abuf_new((u32)n);
    if (!b) { kfree(out); return JS_EXC; }
    memcpy(b->u.buf.data, out, (usize)n);
    kfree(out);
    t->u.ta.buffer = b;
    t->u.ta.len = (u32)n;
    return jv_obj(t);
}
NATIVE(td_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor TextDecoder exige 'new'");
    jsstr *lab = ARG(0) == JS_UNDEF ? NULL : js_tostr(ARG(0));
    if (ARG(0) != JS_UNDEF && !lab) return JS_EXC;
    if (lab && strcasecmp(lab->d, "utf-8") && strcasecmp(lab->d, "utf8") && strcasecmp(lab->d, "unicode-1-1-utf-8"))
        return js_throw_err(E_RANGE, "TextDecoder: codificação '%s' não suportada", lab->d);
    bool fatal = jv_isobj(ARG(1)) && js_truthy(js_getn(ARG(1), "fatal"));
    js_defn(jv_o(thisv), "encoding", jv_str(js_intern("utf-8", -1)), PF_RO);
    js_defn(jv_o(thisv), "fatal", jv_bool(fatal), PF_RO);
    return thisv;
}
NATIVE(td_decode) {
    UNUSED_ARGS;
    jsv src = ARG(0);
    const u8 *p = NULL;
    u32 n = 0;
    if (src == JS_UNDEF) return jv_str(A[A_empty]);
    if (jv_iscls(src, CL_ARRAYBUFFER)) { p = buf_bytes(jv_o(src)); n = buf_len(jv_o(src)); }
    else if (jv_iscls(src, CL_TYPED)) { jsobj *t = jv_o(src); n = js_typed_length(t) * ta_size[t->u.ta.kind]; p = n ? el_ptr(t, 0) : NULL; }
    else if (jv_iscls(src, CL_DATAVIEW)) { jsobj *d = jv_o(src); p = buf_bytes(d->u.ta.buffer); if (p) p += d->u.ta.off; n = d->u.ta.len; }
    else return js_throw_err(E_TYPE, "TextDecoder.decode precisa de um ArrayBuffer ou de uma view");
    bool fatal = jv_isobj(thisv) && js_truthy(js_getn(thisv, "fatal"));
    /* BOM no começo é removido */
    if (n >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) { p += 3; n -= 3; }
    sbuf_t sb = {0};
    for (u32 i = 0; i < n;) {
        u8 c = p[i];
        u32 cp;
        int need = 0;
        u8 lo = 0x80, hi = 0xBF;
        bool bad = false;
        if (c < 0x80) { cp = c; i++; }
        else {
            if (c >= 0xC2 && c <= 0xDF) { need = 1; cp = c & 0x1F; }
            else if (c >= 0xE0 && c <= 0xEF) { need = 2; cp = c & 0x0F; if (c == 0xE0) lo = 0xA0; if (c == 0xED) hi = 0x9F; }
            else if (c >= 0xF0 && c <= 0xF4) { need = 3; cp = c & 0x07; if (c == 0xF0) lo = 0x90; if (c == 0xF4) hi = 0x8F; }
            else { bad = true; cp = 0xFFFD; }
            i++;
            for (int k = 0; k < need && !bad; k++) {
                /* byte de continuação inválido: U+FFFD e ele não é consumido (começa a próxima sequência) */
                if (i >= n || p[i] < lo || p[i] > hi) { bad = true; cp = 0xFFFD; break; }
                cp = cp << 6 | (p[i] & 0x3F);
                i++;
                lo = 0x80; hi = 0xBF;
            }
        }
        if (bad && fatal) { sb_free(&sb); return js_throw_err(E_TYPE, "TextDecoder: UTF-8 inválido"); }
        char tmp[4];
        sb_putn(&sb, tmp, js_utf8_put(tmp, cp));
    }
    jsstr *r = js_strn(sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
    return jv_str(r);
}

/* ---------------------------------------------------------------- registro */
static const char *ro_methods[] = {"join", "indexOf", "lastIndexOf", "includes", "forEach", "every", "some", "find",
                                   "findIndex", "findLast", "findLastIndex", "reduce", "reduceRight", "at", "toLocaleString", NULL};

void js_typed_init(jsrealm *r) {
    jsobj *g = r->global, *op = r->p_object, *ap = r->p_array;
    (void)jv_n_ok;
    /* ArrayBuffer */
    jsobj *bp = js_newobj(op);
    r->p_abuf = bp;
    jsobj *bc = js_newctor(abuf_ctor, "ArrayBuffer", 1, bp);
    js_defn(g, "ArrayBuffer", jv_obj(bc), PF_NOENUM);
    js_defnative(bc, "isView", abuf_isview, 1, 0);
    js_defgetter(bp, "byteLength", abuf_bytelength, 0);
    js_defnative(bp, "slice", abuf_slice, 2, 0);
    js_defprop(bp, A[A_sym_toStringTag], jv_str(js_intern("ArrayBuffer", -1)), PF_NOENUM | PF_RO);
    /* %TypedArray%.prototype */
    jsobj *tp = js_newobj(op);
    r->p_typed = tp;
    js_defgetter(tp, "buffer", ta_getter, 0);
    js_defgetter(tp, "byteLength", ta_getter, 1);
    js_defgetter(tp, "byteOffset", ta_getter, 2);
    js_defgetter(tp, "length", ta_getter, 3);
    jsobj *tag = js_newnative(ta_getter, "get [Symbol.toStringTag]", 0, 4);
    js_defaccessor(tp, A[A_sym_toStringTag], jv_obj(tag), JS_UNDEF, PF_NOENUM);
    js_defnative(tp, "set", ta_set, 1, 0);
    js_defnative(tp, "subarray", ta_subarray, 2, 0);
    js_defnative(tp, "slice", ta_slice, 2, 0);
    js_defnative(tp, "fill", ta_fill, 1, 0);
    js_defnative(tp, "reverse", ta_reverse, 0, 0);
    js_defnative(tp, "copyWithin", ta_copywithin, 2, 0);
    js_defnative(tp, "map", ta_mapfilter, 1, 0);
    js_defnative(tp, "filter", ta_mapfilter, 1, 1);
    js_defnative(tp, "sort", ta_sort, 1, 0);
    js_defnative(tp, "values", ta_iter, 0, 0);
    js_defnative(tp, "keys", ta_iter, 0, 1);
    js_defnative(tp, "entries", ta_iter, 0, 2);
    jsprop *vp = js_ownprop(tp, js_intern("values", -1));
    if (vp) js_defprop(tp, A[A_sym_iterator], vp->v, PF_NOENUM);
    for (int i = 0; ro_methods[i]; i++) {
        jsprop *p = js_ownprop(ap, js_intern(ro_methods[i], -1));
        if (p) js_defprop(tp, js_intern(ro_methods[i], -1), p->v, PF_NOENUM);
    }
    jsprop *ts = js_ownprop(ap, A[A_toString]);
    if (ts) js_defprop(tp, A[A_toString], ts->v, PF_NOENUM);
    /* os 11 construtores */
    for (int k = 0; k < TA_COUNT; k++) {
        jsobj *p = js_newobj(tp);
        r->p_ta[k] = p;
        jsobj *c = js_newctor(ta_ctor, ta_names[k], 3, p);
        c->u.nat.magic = k;
        js_defn(g, ta_names[k], jv_obj(c), PF_NOENUM);
        js_defn(c, "BYTES_PER_ELEMENT", jv_num(ta_size[k]), PF_RO | PF_NOENUM);
        js_defn(p, "BYTES_PER_ELEMENT", jv_num(ta_size[k]), PF_RO | PF_NOENUM);
        js_defnative(c, "from", ta_static_from, 1, k);
        js_defnative(c, "of", ta_static_of, 0, k);
    }
    /* DataView */
    jsobj *dp = js_newobj(op);
    r->p_dview = dp;
    jsobj *dc = js_newctor(dv_ctor, "DataView", 1, dp);
    js_defn(g, "DataView", jv_obj(dc), PF_NOENUM);
    js_defgetter(dp, "buffer", dv_getter, 0);
    js_defgetter(dp, "byteLength", dv_getter, 1);
    js_defgetter(dp, "byteOffset", dv_getter, 2);
    js_defprop(dp, A[A_sym_toStringTag], jv_str(js_intern("DataView", -1)), PF_NOENUM | PF_RO);
    static const char *dvn[] = {"Int8", "Uint8", NULL, "Int16", "Uint16", "Int32", "Uint32", "Float32", "Float64", "BigInt64", "BigUint64"};
    for (int k = 0; k < TA_COUNT; k++) {
        if (!dvn[k]) continue;
        char nm[24];
        snprintf(nm, sizeof nm, "get%s", dvn[k]);
        js_defnative(dp, nm, dv_access, 1, k);
        snprintf(nm, sizeof nm, "set%s", dvn[k]);
        js_defnative(dp, nm, dv_access, 2, k | 16);
    }
    /* TextEncoder / TextDecoder */
    jsobj *tep = js_newobj(op);
    jsobj *tec = js_newctor(te_ctor, "TextEncoder", 0, tep);
    js_defn(g, "TextEncoder", jv_obj(tec), PF_NOENUM);
    js_defnative(tep, "encode", te_encode, 1, 0);
    jsobj *tdp = js_newobj(op);
    jsobj *tdc = js_newctor(td_ctor, "TextDecoder", 0, tdp);
    js_defn(g, "TextDecoder", jv_obj(tdc), PF_NOENUM);
    js_defnative(tdp, "decode", td_decode, 1, 0);
}
