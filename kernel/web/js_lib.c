/* mOS — motor JavaScript "Brisa": biblioteca padrão (Object, Function, Array, String, Number, Boolean,
 * Symbol, Math, Error, iteradores, Map/Set e funções globais) */
#include "js_int.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define TRYV(x) do { if ((x) == JS_EXC) return JS_EXC; } while (0)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

static jsv orig_array_iter;

static const void *memchr_(const void *s, int c, usize n) {
    const u8 *p = s;
    for (usize i = 0; i < n; i++) if (p[i] == (u8)c) return p + i;
    return NULL;
}

/* ============================================================ utilidades */
static jsstr *this_str(jsv thisv, const char *method) {
    if (jv_isnullish(thisv)) { js_throw_err(E_TYPE, "String.prototype.%s chamado em null ou undefined", method); return NULL; }
    if (jv_iscls(thisv, CL_STRING)) return jv_s(jv_o(thisv)->u.prim);
    return js_tostr(thisv);
}

static jsstr *arg_str(int argc, jsv *argv, int i) {
    return js_tostr(ARG(i));
}

static double arg_int(int argc, jsv *argv, int i, double def) {
    if (i >= argc || argv[i] == JS_UNDEF) return def;
    return js_tointeger(argv[i]);
}

static int clamp_index(double rel, int len) {
    if (rel < 0) { rel += len; if (rel < 0) rel = 0; }
    if (rel > len) rel = len;
    return (int)rel;
}

static jsv call3(jsv fn, jsv thisArg, jsv a, jsv b, jsv c) {
    jsv args[3] = {a, b, c};
    return js_call(fn, thisArg, 3, args);
}

/* vetor "denso" a partir de this (vetores reais diretamente; array-likes copiados) */
static jsobj *as_array(jsv thisv, bool *copied) {
    *copied = false;
    if (jv_iscls(thisv, CL_ARRAY) || jv_iscls(thisv, CL_ARGUMENTS)) return jv_o(thisv);
    jsv o = js_toobj(thisv);
    if (o == JS_EXC) return NULL;
    u32 n = js_lengthof(o);
    if (js_pending()) return NULL;
    if (n > (1u << 24)) { js_throw_err(E_RANGE, "Comprimento grande demais"); return NULL; }
    jsobj *a = js_newarr(0);
    for (u32 i = 0; i < n; i++) {
        jsv v = js_getidx(o, i);
        if (v == JS_EXC) return NULL;
        js_arr_push(a, v);
    }
    *copied = true;
    return a;
}

static jsobj *mut_array(jsv thisv, const char *m) {
    if (jv_iscls(thisv, CL_ARRAY) || jv_iscls(thisv, CL_ARGUMENTS)) {
        jsobj *a = jv_o(thisv);
        if (a->frozen) { js_throw_err(E_TYPE, "Não é possível modificar um vetor congelado (%s)", m); return NULL; }
        return a;
    }
    js_throw_err(E_TYPE, "Array.prototype.%s só é suportado em vetores", m);
    return NULL;
}

static jsv need_fn(jsv f, const char *what) {
    if (js_iscallable(f)) return f;
    char b[60];
    return js_throw_err(E_TYPE, "%s não é uma função (%s)", js_cstr(f, b, sizeof b), what);
}

/* ============================================================ Object */
NATIVE(obj_ctor) {
    UNUSED_ARGS;
    jsv v = ARG(0);
    if (jv_isnullish(v)) {
        if (flags & NF_NEW) return thisv;
        return jv_obj(js_newobj(J.realm->p_object));
    }
    return js_toobj(v);
}

static jsobj *to_obj_arg(jsv v) {
    jsv o = js_toobj(v);
    return o == JS_EXC ? NULL : jv_o(o);
}

NATIVE(obj_keys) {
    UNUSED_ARGS;
    jsobj *o = to_obj_arg(ARG(0));
    if (!o) return JS_EXC;
    jsobj *k = js_keylist(o, true, false);
    int mode = self->u.nat.magic;
    if (mode == 0) return jv_obj(k);
    jsobj *out = js_newarr(0);
    for (u32 i = 0; i < k->alen; i++) {
        jsv v = js_getk(jv_obj(o), jv_s(k->arr[i]));
        TRYV(v);
        if (mode == 1) js_arr_push(out, v);
        else { jsv pair[2] = {k->arr[i], v}; js_arr_push(out, jv_obj(js_arr_from(pair, 2))); }
    }
    return jv_obj(out);
}

NATIVE(obj_getownnames) {
    UNUSED_ARGS;
    jsobj *o = to_obj_arg(ARG(0));
    if (!o) return JS_EXC;
    jsobj *k = js_keylist(o, false, false);
    if (o->cls == CL_ARRAY || o->cls == CL_ARGUMENTS) js_arr_push(k, jv_str(A[A_length]));
    return jv_obj(k);
}

NATIVE(obj_assign) {
    UNUSED_ARGS;
    jsv t = js_toobj(ARG(0));
    TRYV(t);
    for (int i = 1; i < argc; i++) {
        if (jv_isnullish(argv[i])) continue;
        jsv s = js_toobj(argv[i]);
        TRYV(s);
        jsobj *k = js_keylist(jv_o(s), true, false);
        for (u32 j = 0; j < k->alen; j++) {
            jsv v = js_getk(s, jv_s(k->arr[j]));
            TRYV(v);
            if (js_setk(t, jv_s(k->arr[j]), v) < 0) return JS_EXC;
        }
    }
    return t;
}

NATIVE(obj_freeze) {
    UNUSED_ARGS;
    jsv v = ARG(0);
    if (!jv_isobj(v)) return v;
    jsobj *o = jv_o(v);
    int mode = self->u.nat.magic;   /* 0 freeze, 1 seal, 2 preventExtensions */
    o->noext = 1;
    if (mode == 0) o->frozen = 1;
    for (u32 i = 0; i < o->nprops; i++) {
        if (mode <= 1) o->props[i].flags |= PF_NOCONF;
        if (mode == 0 && !(o->props[i].flags & PF_ACCESSOR)) o->props[i].flags |= PF_RO;
    }
    return v;
}

NATIVE(obj_isfrozen) {
    UNUSED_ARGS;
    jsv v = ARG(0);
    int mode = self->u.nat.magic;
    if (!jv_isobj(v)) return mode == 2 ? JS_FALSE : JS_TRUE;
    jsobj *o = jv_o(v);
    if (mode == 2) return jv_bool(!o->noext);
    if (!o->noext) return JS_FALSE;
    if (mode == 0 && (o->cls == CL_ARRAY && o->alen && !o->frozen)) return JS_FALSE;
    for (u32 i = 0; i < o->nprops; i++) {
        if (!(o->props[i].flags & PF_NOCONF)) return JS_FALSE;
        if (mode == 0 && !(o->props[i].flags & (PF_RO | PF_ACCESSOR))) return JS_FALSE;
    }
    return JS_TRUE;
}

NATIVE(obj_create) {
    UNUSED_ARGS;
    jsv p = ARG(0);
    if (!jv_isobj(p) && p != JS_NULL) return js_throw_err(E_TYPE, "O protótipo deve ser um objeto ou null");
    jsobj *o = js_newobj(p == JS_NULL ? NULL : jv_o(p));
    if (argc > 1 && argv[1] != JS_UNDEF) {
        jsv args[2] = {jv_obj(o), argv[1]};
        extern jsv obj_defprops_impl(jsv o, jsv props);
        if (obj_defprops_impl(args[0], args[1]) == JS_EXC) return JS_EXC;
    }
    return jv_obj(o);
}

NATIVE(obj_getproto) {
    UNUSED_ARGS;
    jsv v = ARG(0);
    if (jv_isnullish(v)) return js_throw_err(E_TYPE, "Não é possível converter undefined ou null em objeto");
    jsobj *p = js_getproto(v);
    return p ? jv_obj(p) : JS_NULL;
}

NATIVE(obj_setproto) {
    UNUSED_ARGS;
    jsv v = ARG(0), p = ARG(1);
    if (!jv_isobj(v)) return v;
    if (!jv_isobj(p) && p != JS_NULL) return js_throw_err(E_TYPE, "O protótipo deve ser um objeto ou null");
    jsobj *np = p == JS_NULL ? NULL : jv_o(p);
    for (jsobj *q = np; q; q = q->proto) if (q == jv_o(v)) return js_throw_err(E_TYPE, "Cadeia de protótipos cíclica");
    jv_o(v)->proto = np;
    return v;
}

static int define_from_desc(jsobj *o, jsstr *k, jsv desc) {
    if (!jv_isobj(desc)) { js_throw_err(E_TYPE, "O descritor de propriedade deve ser um objeto"); return -1; }
    jsv d = desc;
    int hv = js_has(d, jv_str(A[A_value])), hg = js_has(d, jv_str(A[A_get])), hs = js_has(d, jv_str(A[A_set]));
    int hw = js_has(d, jv_str(A[A_writable])), he = js_has(d, jv_str(A[A_enumerable])), hc = js_has(d, jv_str(A[A_configurable]));
    if (hv < 0 || hg < 0 || hs < 0 || hw < 0 || he < 0 || hc < 0) return -1;
    jsprop *p = js_ownprop(o, k);
    u32 idx;
    if ((o->cls == CL_ARRAY || o->cls == CL_ARGUMENTS) && js_str_isindex(k, &idx) && !hg && !hs) {
        jsv v = hv ? js_getk(d, A[A_value]) : JS_UNDEF;
        if (v == JS_EXC) return -1;
        js_arr_set(o, idx, v);
        return 0;
    }
    if (o->cls == CL_ARRAY && js_str_eq(k, A[A_length])) {
        if (hv) { jsv v = js_getk(d, A[A_value]); if (v == JS_EXC) return -1; js_arr_setlen(o, js_touint32(v)); }
        return 0;
    }
    if (p && (p->flags & PF_NOCONF) && (p->flags & PF_RO)) { js_throw_err(E_TYPE, "Não é possível redefinir a propriedade '%s'", k->d); return -1; }
    u32 fl = p ? p->flags & ~PF_ACCESSOR : (PF_NOENUM | PF_RO | PF_NOCONF);
    if (he) { jsv e = js_getk(d, A[A_enumerable]); if (e == JS_EXC) return -1; if (js_truthy(e)) fl &= ~PF_NOENUM; else fl |= PF_NOENUM; }
    if (hc) { jsv e = js_getk(d, A[A_configurable]); if (e == JS_EXC) return -1; if (js_truthy(e)) fl &= ~PF_NOCONF; else fl |= PF_NOCONF; }
    if (hg || hs) {
        jsv g = hg ? js_getk(d, A[A_get]) : JS_HOLE, s = hs ? js_getk(d, A[A_set]) : JS_HOLE;
        if (g == JS_EXC || s == JS_EXC) return -1;
        if ((g != JS_HOLE && g != JS_UNDEF && !js_iscallable(g)) || (s != JS_HOLE && s != JS_UNDEF && !js_iscallable(s))) {
            js_throw_err(E_TYPE, "Getter/setter deve ser uma função");
            return -1;
        }
        js_defaccessor(o, k, g, s, fl & ~PF_RO);
        return 0;
    }
    if (hw) { jsv e = js_getk(d, A[A_writable]); if (e == JS_EXC) return -1; if (js_truthy(e)) fl &= ~PF_RO; else fl |= PF_RO; }
    else if (p && (p->flags & PF_ACCESSOR)) fl |= PF_RO;
    jsv v = hv ? js_getk(d, A[A_value]) : (p && !(p->flags & PF_ACCESSOR) ? p->v : JS_UNDEF);
    if (v == JS_EXC) return -1;
    js_defprop(o, k, v, fl);
    return 0;
}

NATIVE(obj_defprop) {
    UNUSED_ARGS;
    jsv o = ARG(0);
    if (!jv_isobj(o)) return js_throw_err(E_TYPE, "Object.defineProperty chamado em algo que não é objeto");
    jsstr *k = js_tokey(ARG(1));
    if (!k) return JS_EXC;
    if (define_from_desc(jv_o(o), k, ARG(2)) < 0) return JS_EXC;
    return o;
}

jsv obj_defprops_impl(jsv o, jsv props) {
    jsv ps = js_toobj(props);
    TRYV(ps);
    jsobj *k = js_keylist(jv_o(ps), true, false);
    for (u32 i = 0; i < k->alen; i++) {
        jsv d = js_getk(ps, jv_s(k->arr[i]));
        TRYV(d);
        if (define_from_desc(jv_o(o), jv_s(k->arr[i]), d) < 0) return JS_EXC;
    }
    return o;
}

NATIVE(obj_defprops) {
    UNUSED_ARGS;
    jsv o = ARG(0);
    if (!jv_isobj(o)) return js_throw_err(E_TYPE, "Object.defineProperties chamado em algo que não é objeto");
    return obj_defprops_impl(o, ARG(1));
}

NATIVE(obj_getdesc) {
    UNUSED_ARGS;
    jsobj *o = to_obj_arg(ARG(0));
    if (!o) return JS_EXC;
    jsstr *k = js_tokey(ARG(1));
    if (!k) return JS_EXC;
    jsobj *d = js_newobj(J.realm->p_object);
    u32 idx;
    if ((o->cls == CL_ARRAY || o->cls == CL_ARGUMENTS) && js_str_isindex(k, &idx)) {
        if (idx >= o->alen || o->arr[idx] == JS_HOLE) return JS_UNDEF;
        js_defprop(d, A[A_value], o->arr[idx], 0);
        js_defprop(d, A[A_writable], jv_bool(!o->frozen), 0);
        js_defprop(d, A[A_enumerable], JS_TRUE, 0);
        js_defprop(d, A[A_configurable], jv_bool(!o->frozen), 0);
        return jv_obj(d);
    }
    if (o->cls == CL_ARRAY && js_str_eq(k, A[A_length])) {
        js_defprop(d, A[A_value], jv_int((int)o->alen), 0);
        js_defprop(d, A[A_writable], jv_bool(!o->frozen), 0);
        js_defprop(d, A[A_enumerable], JS_FALSE, 0);
        js_defprop(d, A[A_configurable], JS_FALSE, 0);
        return jv_obj(d);
    }
    jsprop *p = js_ownprop(o, k);
    if (!p) return JS_UNDEF;
    if (p->flags & PF_ACCESSOR) {
        js_defprop(d, A[A_get], p->v, 0);
        js_defprop(d, A[A_set], p->set, 0);
    } else {
        js_defprop(d, A[A_value], p->v, 0);
        js_defprop(d, A[A_writable], jv_bool(!(p->flags & PF_RO) && !o->frozen), 0);
    }
    js_defprop(d, A[A_enumerable], jv_bool(!(p->flags & PF_NOENUM)), 0);
    js_defprop(d, A[A_configurable], jv_bool(!(p->flags & PF_NOCONF)), 0);
    return jv_obj(d);
}

NATIVE(obj_fromentries) {
    UNUSED_ARGS;
    jsobj *o = js_newobj(J.realm->p_object);
    jsv it = js_iter_open(ARG(0));
    TRYV(it);
    for (;;) {
        jsv e;
        int r = js_iter_step(it, &e);
        if (r < 0) return JS_EXC;
        if (!r) break;
        jsv k = js_getidx(e, 0);
        TRYV(k);
        jsv v = js_getidx(e, 1);
        TRYV(v);
        jsstr *ks = js_tokey(k);
        if (!ks) return JS_EXC;
        js_defprop(o, ks, v, 0);
    }
    return jv_obj(o);
}

NATIVE(obj_is) { UNUSED_ARGS; return jv_bool(js_samevalue(ARG(0), ARG(1), false)); }

static bool own_has(jsobj *o, jsstr *k) {
    u32 idx;
    if ((o->cls == CL_ARRAY || o->cls == CL_ARGUMENTS)) {
        if (js_str_isindex(k, &idx)) return idx < o->alen && o->arr[idx] != JS_HOLE;
        if (js_str_eq(k, A[A_length])) return true;
    }
    if (o->cls == CL_STRING) {
        jsstr *s = jv_s(o->u.prim);
        if (js_str_isindex(k, &idx)) return idx < s->ulen;
        if (js_str_eq(k, A[A_length])) return true;
    }
    if (o->cls >= CL_NODE && o->cls <= CL_HOST) {
        bool found = false;
        js_dom_get(o, k, &found);
        if (found) return true;
    }
    return js_ownprop(o, k) != NULL;
}

NATIVE(obj_hasown) {
    UNUSED_ARGS;
    jsobj *o = to_obj_arg(ARG(0));
    if (!o) return JS_EXC;
    jsstr *k = js_tokey(ARG(1));
    if (!k) return JS_EXC;
    return jv_bool(own_has(o, k));
}

NATIVE(objp_hasown) {
    UNUSED_ARGS;
    jsstr *k = js_tokey(ARG(0));
    if (!k) return JS_EXC;
    jsobj *o = to_obj_arg(thisv);
    if (!o) return JS_EXC;
    return jv_bool(own_has(o, k));
}

NATIVE(objp_isproto) {
    UNUSED_ARGS;
    jsv v = ARG(0);
    if (!jv_isobj(v) || !jv_isobj(thisv)) return JS_FALSE;
    for (jsobj *p = jv_o(v)->proto; p; p = p->proto) if (p == jv_o(thisv)) return JS_TRUE;
    return JS_FALSE;
}

NATIVE(objp_propenum) {
    UNUSED_ARGS;
    jsstr *k = js_tokey(ARG(0));
    if (!k) return JS_EXC;
    jsobj *o = to_obj_arg(thisv);
    if (!o) return JS_EXC;
    jsprop *p = js_ownprop(o, k);
    if (p) return jv_bool(!(p->flags & PF_NOENUM));
    return jv_bool(own_has(o, k) && !js_str_eq(k, A[A_length]));
}

NATIVE(objp_tostring) {
    UNUSED_ARGS;
    if (thisv == JS_UNDEF) return jv_str(js_strz("[object Undefined]"));
    if (thisv == JS_NULL) return jv_str(js_strz("[object Null]"));
    jsv o = js_toobj(thisv);
    TRYV(o);
    jsv tag = js_getk(o, A[A_sym_toStringTag]);
    TRYV(tag);
    if (jv_isstr(tag)) return jv_str(js_str_fmt("[object %s]", jv_s(tag)->d));
    const char *t = "Object";
    switch (jv_o(o)->cls) {
    case CL_ARRAY: t = "Array"; break;
    case CL_FUNCTION: case CL_NATIVE: case CL_BOUND: t = "Function"; break;
    case CL_ERROR: t = "Error"; break;
    case CL_DATE: t = "Date"; break;
    case CL_REGEXP: t = "RegExp"; break;
    case CL_STRING: t = "String"; break;
    case CL_NUMBER: t = "Number"; break;
    case CL_BOOLEAN: t = "Boolean"; break;
    case CL_ARGUMENTS: t = "Arguments"; break;
    }
    return jv_str(js_str_fmt("[object %s]", t));
}

NATIVE(objp_tolocale) { UNUSED_ARGS; return js_invoke(thisv, A[A_toString], 0, NULL); }
NATIVE(objp_valueof) { UNUSED_ARGS; return js_toobj(thisv); }

NATIVE(objp_proto_get) {
    UNUSED_ARGS;
    jsobj *p = js_getproto(thisv);
    return p ? jv_obj(p) : JS_NULL;
}

NATIVE(objp_proto_set) {
    UNUSED_ARGS;
    jsv p = ARG(0);
    if (!jv_isobj(thisv)) return JS_UNDEF;
    if (!jv_isobj(p) && p != JS_NULL) return JS_UNDEF;
    jsobj *np = p == JS_NULL ? NULL : jv_o(p);
    for (jsobj *q = np; q; q = q->proto) if (q == jv_o(thisv)) return js_throw_err(E_TYPE, "Cadeia de protótipos cíclica");
    jv_o(thisv)->proto = np;
    return JS_UNDEF;
}

/* ============================================================ Function */
NATIVE(fn_proto_call) { UNUSED_ARGS; return JS_UNDEF; }

NATIVE(fn_ctor) {
    UNUSED_ARGS;
    return js_new_function(argc, argv);
}

NATIVE(fnp_call) {
    UNUSED_ARGS;
    if (!js_iscallable(thisv)) return js_throw_err(E_TYPE, "Function.prototype.call chamado em algo que não é função");
    return js_call(thisv, ARG(0), argc > 1 ? argc - 1 : 0, argv + 1);
}

NATIVE(fnp_apply) {
    UNUSED_ARGS;
    if (!js_iscallable(thisv)) return js_throw_err(E_TYPE, "Function.prototype.apply chamado em algo que não é função");
    jsv arr = ARG(1);
    if (jv_isnullish(arr)) return js_call(thisv, ARG(0), 0, NULL);
    if (!jv_isobj(arr)) return js_throw_err(E_TYPE, "O segundo argumento de apply deve ser um vetor");
    bool copied;
    jsobj *a = as_array(arr, &copied);
    if (!a) return JS_EXC;
    if (a->alen > 65536) return js_throw_err(E_RANGE, "Argumentos demais");
    jsobj *copy = js_arr_from(a->arr, (int)a->alen);
    for (u32 i = 0; i < copy->alen; i++) if (copy->arr[i] == JS_HOLE) copy->arr[i] = JS_UNDEF;
    return js_call(thisv, ARG(0), (int)copy->alen, copy->arr);
}

NATIVE(fnp_bind) {
    UNUSED_ARGS;
    if (!js_iscallable(thisv)) return js_throw_err(E_TYPE, "Bind deve ser chamado em uma função");
    jsobj *b = js_newobj_cls(CL_BOUND, jv_o(thisv)->proto);
    b->u.bound.target = thisv;
    b->u.bound.bthis = ARG(0);
    b->u.bound.args = argc > 1 ? js_arr_from(argv + 1, argc - 1) : NULL;
    jsv nm = js_getk(thisv, A[A_name]);
    TRYV(nm);
    jsv ln = js_getk(thisv, A[A_length]);
    TRYV(ln);
    double l = jv_isnum(ln) ? jv_n(ln) - (argc > 1 ? argc - 1 : 0) : 0;
    js_defprop(b, A[A_length], jv_num(l > 0 ? l : 0), PF_NOENUM | PF_RO);
    js_defprop(b, A[A_name], jv_str(js_str_fmt("bound %s", jv_isstr(nm) ? jv_s(nm)->d : "")), PF_NOENUM | PF_RO);
    return jv_obj(b);
}

NATIVE(fnp_tostring) {
    UNUSED_ARGS;
    if (!js_iscallable(thisv)) return js_throw_err(E_TYPE, "Function.prototype.toString chamado em algo que não é função");
    jsv nm = js_getk(thisv, A[A_name]);
    TRYV(nm);
    const char *n = jv_isstr(nm) ? jv_s(nm)->d : "";
    jsobj *f = jv_o(thisv);
    if (f->cls == CL_FUNCTION && f->is_class) return jv_str(js_str_fmt("class %s { }", n));
    if (f->cls == CL_FUNCTION) return jv_str(js_str_fmt("function %s() { [código] }", n));
    return jv_str(js_str_fmt("function %s() { [código nativo] }", n));
}

NATIVE(fnp_hasinstance) {
    UNUSED_ARGS;
    int r = js_instanceof(ARG(0), thisv);
    return r < 0 ? JS_EXC : jv_bool(r);
}

/* ============================================================ Array */
NATIVE(arr_ctor) {
    UNUSED_ARGS;
    jsobj *a = (flags & NF_NEW) ? jv_o(thisv) : js_newarr(0);
    a->cls = CL_ARRAY;
    if (argc == 1 && jv_isnum(argv[0])) {
        double d = jv_n(argv[0]);
        u32 n = (u32)d;
        if ((double)n != d || d < 0) return js_throw_err(E_RANGE, "Comprimento de vetor inválido");
        if (n > (1u << 26)) return js_throw_err(E_RANGE, "Comprimento de vetor grande demais");
        js_arr_setlen(a, n);
    } else for (int i = 0; i < argc; i++) js_arr_push(a, argv[i]);
    return jv_obj(a);
}

NATIVE(arr_isarray) { UNUSED_ARGS; return jv_bool(js_isarray(ARG(0))); }

NATIVE(arr_of) { UNUSED_ARGS; return jv_obj(js_arr_from(argv, argc)); }

NATIVE(arr_from) {
    UNUSED_ARGS;
    jsv src = ARG(0), mapfn = ARG(1), thisArg = ARG(2);
    if (mapfn != JS_UNDEF && !js_iscallable(mapfn)) return js_throw_err(E_TYPE, "Array.from: a função de mapeamento não é chamável");
    jsobj *out = js_newarr(0);
    if (jv_isnullish(src)) return js_throw_err(E_TYPE, "Array.from: %s não é iterável", src == JS_NULL ? "null" : "undefined");
    jsv itf = jv_isobj(src) || jv_isstr(src) ? js_getk(src, A[A_sym_iterator]) : JS_UNDEF;
    TRYV(itf);
    if (js_iscallable(itf)) {
        jsv it = js_iter_open(src);
        TRYV(it);
        for (u32 i = 0;; i++) {
            jsv v;
            int r = js_iter_step(it, &v);
            if (r < 0) return JS_EXC;
            if (!r) break;
            if (mapfn != JS_UNDEF) { v = call3(mapfn, thisArg, v, jv_num(i), JS_UNDEF); TRYV(v); }
            js_arr_push(out, v);
        }
        return jv_obj(out);
    }
    jsv o = js_toobj(src);
    TRYV(o);
    u32 n = js_lengthof(o);
    if (js_pending()) return JS_EXC;
    if (n > (1u << 24)) return js_throw_err(E_RANGE, "Comprimento grande demais");
    for (u32 i = 0; i < n; i++) {
        jsv v = js_getidx(o, i);
        TRYV(v);
        if (mapfn != JS_UNDEF) { v = call3(mapfn, thisArg, v, jv_num(i), JS_UNDEF); TRYV(v); }
        js_arr_push(out, v);
    }
    return jv_obj(out);
}

NATIVE(arrp_push) {
    UNUSED_ARGS;
    jsobj *a = mut_array(thisv, "push");
    if (!a) return JS_EXC;
    if (a->noext) return js_throw_err(E_TYPE, "Não é possível adicionar elementos: o vetor não é extensível");
    for (int i = 0; i < argc; i++) js_arr_push(a, argv[i]);
    return jv_int((int)a->alen);
}

NATIVE(arrp_pop) {
    UNUSED_ARGS;
    jsobj *a = mut_array(thisv, "pop");
    if (!a) return JS_EXC;
    if (!a->alen) return JS_UNDEF;
    jsv v = a->arr[--a->alen];
    return v == JS_HOLE ? JS_UNDEF : v;
}

NATIVE(arrp_shift) {
    UNUSED_ARGS;
    jsobj *a = mut_array(thisv, "shift");
    if (!a) return JS_EXC;
    if (!a->alen) return JS_UNDEF;
    jsv v = a->arr[0];
    memmove(a->arr, a->arr + 1, sizeof(jsv) * (a->alen - 1));
    a->alen--;
    return v == JS_HOLE ? JS_UNDEF : v;
}

NATIVE(arrp_unshift) {
    UNUSED_ARGS;
    jsobj *a = mut_array(thisv, "unshift");
    if (!a) return JS_EXC;
    if (argc) {
        js_arr_reserve(a, a->alen + (u32)argc);
        memmove(a->arr + argc, a->arr, sizeof(jsv) * a->alen);
        memcpy(a->arr, argv, sizeof(jsv) * (usize)argc);
        a->alen += (u32)argc;
    }
    return jv_int((int)a->alen);
}

NATIVE(arrp_slice) {
    UNUSED_ARGS;
    bool cp;
    jsobj *a = as_array(thisv, &cp);
    if (!a) return JS_EXC;
    int len = (int)a->alen;
    int s = clamp_index(arg_int(argc, argv, 0, 0), len);
    int e = clamp_index(arg_int(argc, argv, 1, len), len);
    if (js_pending()) return JS_EXC;
    jsobj *out = js_newarr(0);
    for (int i = s; i < e; i++) js_arr_push(out, a->arr[i]);
    return jv_obj(out);
}

NATIVE(arrp_splice) {
    UNUSED_ARGS;
    jsobj *a = mut_array(thisv, "splice");
    if (!a) return JS_EXC;
    int len = (int)a->alen;
    int s = clamp_index(arg_int(argc, argv, 0, 0), len);
    int dc = argc == 0 ? 0 : argc == 1 ? len - s : (int)CLAMP(arg_int(argc, argv, 1, 0), 0, len - s);
    if (js_pending()) return JS_EXC;
    int ins = argc > 2 ? argc - 2 : 0;
    jsobj *removed = js_arr_from(a->arr + s, dc);
    int nl = len - dc + ins;
    if (ins > dc) js_arr_reserve(a, (u32)nl);
    memmove(a->arr + s + ins, a->arr + s + dc, sizeof(jsv) * (usize)(len - s - dc));
    for (int i = 0; i < ins; i++) a->arr[s + i] = argv[2 + i];
    a->alen = (u32)nl;
    return jv_obj(removed);
}

NATIVE(arrp_concat) {
    UNUSED_ARGS;
    jsobj *out = js_newarr(0);
    jsv self0 = js_toobj(thisv);
    TRYV(self0);
    for (int i = -1; i < argc; i++) {
        jsv v = i < 0 ? self0 : argv[i];
        if (jv_iscls(v, CL_ARRAY)) {
            jsobj *s = jv_o(v);
            js_arr_reserve(out, out->alen + s->alen);
            for (u32 k = 0; k < s->alen; k++) out->arr[out->alen++] = s->arr[k];
        } else js_arr_push(out, v);
    }
    return jv_obj(out);
}

jsstr *js_array_join(jsobj *a, jsstr *sep) {
    static int depth;
    if (depth > 64) return A[A_empty];
    sbuf_t sb = {0};
    depth++;
    for (u32 i = 0; i < a->alen; i++) {
        if (i && sep->len) sb_putn(&sb, sep->d, (int)sep->len);
        jsv v = a->arr[i];
        if (v == JS_HOLE || jv_isnullish(v)) continue;
        jsstr *s = js_tostr(v);
        if (!s) { depth--; sb_free(&sb); return NULL; }
        sb_putn(&sb, s->d, (int)s->len);
    }
    depth--;
    jsstr *r = js_strn(sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
    return r;
}

NATIVE(arrp_join) {
    UNUSED_ARGS;
    bool cp;
    jsobj *a = as_array(thisv, &cp);
    if (!a) return JS_EXC;
    jsstr *sep = ARG(0) == JS_UNDEF ? js_intern(",", 1) : js_tostr(ARG(0));
    if (!sep) return JS_EXC;
    jsstr *r = js_array_join(a, sep);
    return r ? jv_str(r) : JS_EXC;
}

NATIVE(arrp_tostring) {
    UNUSED_ARGS;
    if (!jv_isobj(thisv)) return objp_tostring(thisv, argc, argv, self, flags);
    jsv j = js_getk(thisv, A[A_join]);
    TRYV(j);
    if (js_iscallable(j)) return js_call(j, thisv, 0, NULL);
    return objp_tostring(thisv, argc, argv, self, flags);
}

NATIVE(arrp_reverse) {
    UNUSED_ARGS;
    jsobj *a = mut_array(thisv, "reverse");
    if (!a) return JS_EXC;
    for (u32 i = 0, j = a->alen ? a->alen - 1 : 0; i < j; i++, j--) { jsv t = a->arr[i]; a->arr[i] = a->arr[j]; a->arr[j] = t; }
    return thisv;
}

NATIVE(arrp_indexof) {
    UNUSED_ARGS;
    bool cp;
    jsobj *a = as_array(thisv, &cp);
    if (!a) return JS_EXC;
    int mode = self->u.nat.magic;   /* 0 indexOf, 1 lastIndexOf, 2 includes */
    int len = (int)a->alen;
    jsv x = ARG(0);
    if (mode == 1) {
        int from = argc > 1 ? (int)arg_int(argc, argv, 1, len - 1) : len - 1;
        if (from < 0) from += len;
        for (int i = MIN(from, len - 1); i >= 0; i--) if (a->arr[i] != JS_HOLE && js_strict_eq(a->arr[i], x)) return jv_int(i);
        return jv_int(-1);
    }
    int from = clamp_index(arg_int(argc, argv, 1, 0), len);
    for (int i = from; i < len; i++) {
        jsv v = a->arr[i];
        if (mode == 2) { if (js_samevalue(v == JS_HOLE ? JS_UNDEF : v, x, true)) return JS_TRUE; }
        else if (v != JS_HOLE && js_strict_eq(v, x)) return jv_int(i);
    }
    return mode == 2 ? JS_FALSE : jv_int(-1);
}

/* find / findIndex / findLast / findLastIndex / filter / map / forEach / some / every */
NATIVE(arrp_iter) {
    UNUSED_ARGS;
    bool cp;
    jsobj *a = as_array(thisv, &cp);
    if (!a) return JS_EXC;
    jsv fn = need_fn(ARG(0), "callback");
    TRYV(fn);
    jsv targ = ARG(1);
    int mode = self->u.nat.magic;
    jsv src = cp ? jv_obj(a) : thisv;
    jsobj *out = (mode == 4 || mode == 5) ? js_newarr(0) : NULL;
    if (mode == 5) js_arr_setlen(out, a->alen);
    bool rev = mode == 2 || mode == 3;
    u32 len = a->alen;
    for (u32 k = 0; k < len; k++) {
        u32 i = rev ? len - 1 - k : k;
        jsv v = i < a->alen ? a->arr[i] : JS_HOLE;
        if (v == JS_HOLE) { if (mode >= 4) continue; v = JS_UNDEF; }
        jsv r = call3(fn, targ, v, jv_num(i), src);
        TRYV(r);
        switch (mode) {
        case 0: case 2: if (js_truthy(r)) return v; break;
        case 1: case 3: if (js_truthy(r)) return jv_num(i); break;
        case 4: if (js_truthy(r)) js_arr_push(out, v); break;
        case 5: if (i < out->alen) out->arr[i] = r; break;
        case 7: if (js_truthy(r)) return JS_TRUE; break;
        case 8: if (!js_truthy(r)) return JS_FALSE; break;
        }
    }
    switch (mode) {
    case 0: case 2: return JS_UNDEF;
    case 1: case 3: return jv_int(-1);
    case 4: case 5: return jv_obj(out);
    case 7: return JS_FALSE;
    case 8: return JS_TRUE;
    }
    return JS_UNDEF;
}

NATIVE(arrp_reduce) {
    UNUSED_ARGS;
    bool cp;
    jsobj *a = as_array(thisv, &cp);
    if (!a) return JS_EXC;
    jsv fn = need_fn(ARG(0), "reduce");
    TRYV(fn);
    bool right = self->u.nat.magic == 1;
    u32 len = a->alen;
    u32 k = 0;
    jsv acc;
    if (argc >= 2) acc = argv[1];
    else {
        for (; k < len; k++) { u32 i = right ? len - 1 - k : k; if (a->arr[i] != JS_HOLE) break; }
        if (k >= len) return js_throw_err(E_TYPE, "Redução de vetor vazio sem valor inicial");
        acc = a->arr[right ? len - 1 - k : k];
        k++;
    }
    for (; k < len; k++) {
        u32 i = right ? len - 1 - k : k;
        if (i >= a->alen || a->arr[i] == JS_HOLE) continue;
        jsv args[4] = {acc, a->arr[i], jv_num(i), cp ? jv_obj(a) : thisv};
        acc = js_call(fn, JS_UNDEF, 4, args);
        TRYV(acc);
    }
    return acc;
}

static int sort_cmp(jsv cmpfn, jsv x, jsv y, int *err) {
    if (x == JS_UNDEF) return y == JS_UNDEF ? 0 : 1;
    if (y == JS_UNDEF) return -1;
    if (cmpfn != JS_UNDEF) {
        jsv args[2] = {x, y};
        jsv r = js_call(cmpfn, JS_UNDEF, 2, args);
        if (r == JS_EXC) { *err = 1; return 0; }
        double d = js_tonum(r);
        if (js_pending()) { *err = 1; return 0; }
        return d < 0 ? -1 : d > 0 ? 1 : 0;
    }
    if (jv_isnum(x) && jv_isnum(y)) {
        /* comparação por string sem alocar quando possível */
        char a[40], b[40];
        js_numfmt(jv_n(x), a, sizeof a);
        js_numfmt(jv_n(y), b, sizeof b);
        int c = strcmp(a, b);
        return c < 0 ? -1 : c > 0 ? 1 : 0;
    }
    jsstr *sx = js_tostr(x);
    if (!sx) { *err = 1; return 0; }
    jsstr *sy = js_tostr(y);
    if (!sy) { *err = 1; return 0; }
    return js_str_cmp(sx, sy);
}

NATIVE(arrp_sort) {
    UNUSED_ARGS;
    jsobj *a = mut_array(thisv, "sort");
    if (!a) return JS_EXC;
    jsv cmp = ARG(0);
    if (cmp != JS_UNDEF && !js_iscallable(cmp)) return js_throw_err(E_TYPE, "A função de comparação deve ser uma função ou undefined");
    /* copia os valores não-buraco para um vetor temporário e faz merge sort estável */
    u32 n = 0, holes = 0;
    jsobj *tmp = js_newarr(0);
    for (u32 i = 0; i < a->alen; i++) { if (a->arr[i] == JS_HOLE) holes++; else { js_arr_push(tmp, a->arr[i]); n++; } }
    jsobj *buf = js_newarr(n);
    int err = 0;
    for (u32 width = 1; width < n && !err; width *= 2) {
        for (u32 lo = 0; lo < n && !err; lo += 2 * width) {
            u32 mid = MIN(lo + width, n), hi = MIN(lo + 2 * width, n);
            u32 i = lo, j = mid, k = lo;
            while (i < mid && j < hi) {
                int c = sort_cmp(cmp, tmp->arr[j], tmp->arr[i], &err);
                if (err) break;
                if (c < 0) buf->arr[k++] = tmp->arr[j++];
                else buf->arr[k++] = tmp->arr[i++];
            }
            if (err) break;
            while (i < mid) buf->arr[k++] = tmp->arr[i++];
            while (j < hi) buf->arr[k++] = tmp->arr[j++];
        }
        if (err) break;
        jsv *t = tmp->arr; tmp->arr = buf->arr; buf->arr = t;
        u32 tc = tmp->acap; tmp->acap = buf->acap; buf->acap = tc;
    }
    if (err) return JS_EXC;
    js_arr_setlen(a, n + holes);
    for (u32 i = 0; i < n; i++) a->arr[i] = tmp->arr[i];
    for (u32 i = n; i < n + holes; i++) a->arr[i] = JS_HOLE;
    return thisv;
}

NATIVE(arrp_fill) {
    UNUSED_ARGS;
    jsobj *a = mut_array(thisv, "fill");
    if (!a) return JS_EXC;
    int len = (int)a->alen;
    int s = clamp_index(arg_int(argc, argv, 1, 0), len), e = clamp_index(arg_int(argc, argv, 2, len), len);
    for (int i = s; i < e; i++) a->arr[i] = ARG(0);
    return thisv;
}

static void flat_into(jsobj *out, jsobj *a, double depth) {
    for (u32 i = 0; i < a->alen; i++) {
        jsv v = a->arr[i];
        if (v == JS_HOLE) continue;
        if (depth >= 1 && jv_iscls(v, CL_ARRAY)) flat_into(out, jv_o(v), depth - 1);
        else js_arr_push(out, v);
    }
}

NATIVE(arrp_flat) {
    UNUSED_ARGS;
    bool cp;
    jsobj *a = as_array(thisv, &cp);
    if (!a) return JS_EXC;
    double d = arg_int(argc, argv, 0, 1);
    jsobj *out = js_newarr(0);
    flat_into(out, a, d > 100 ? 100 : d);
    return jv_obj(out);
}

NATIVE(arrp_flatmap) {
    UNUSED_ARGS;
    bool cp;
    jsobj *a = as_array(thisv, &cp);
    if (!a) return JS_EXC;
    jsv fn = need_fn(ARG(0), "flatMap");
    TRYV(fn);
    jsobj *out = js_newarr(0);
    for (u32 i = 0; i < a->alen; i++) {
        if (a->arr[i] == JS_HOLE) continue;
        jsv r = call3(fn, ARG(1), a->arr[i], jv_num(i), thisv);
        TRYV(r);
        if (jv_iscls(r, CL_ARRAY)) flat_into(out, jv_o(r), 0);
        else js_arr_push(out, r);
    }
    return jv_obj(out);
}

NATIVE(arrp_at) {
    UNUSED_ARGS;
    bool cp;
    jsobj *a = as_array(thisv, &cp);
    if (!a) return JS_EXC;
    double i = arg_int(argc, argv, 0, 0);
    if (i < 0) i += a->alen;
    if (i < 0 || i >= a->alen) return JS_UNDEF;
    return js_arr_get(a, (u32)i);
}

static jsobj *new_iter(jsv target, int kind) {
    jsobj *it = js_newobj_cls(CL_ITER, J.realm->p_iter);
    it->u.it.target = target;
    it->u.it.kind = kind;
    it->u.it.pos = 0;
    return it;
}

NATIVE(arrp_iterator) {
    UNUSED_ARGS;
    jsv o = js_toobj(thisv);
    TRYV(o);
    return jv_obj(new_iter(o, self->u.nat.magic));
}

bool js_array_iter_pristine(jsv v) {
    jsobj *a = jv_o(v);
    if (a->nprops && js_ownprop(a, A[A_sym_iterator])) return false;
    if (a->proto != J.realm->p_array) return false;
    jsprop *p = js_ownprop(J.realm->p_array, A[A_sym_iterator]);
    return p && p->v == orig_array_iter;
}

/* ============================================================ iteradores */
enum { IT_ARR_VALUES, IT_ARR_KEYS, IT_ARR_ENTRIES, IT_STRING, IT_MAP_ENTRIES, IT_MAP_KEYS, IT_MAP_VALUES, IT_SET_VALUES, IT_SET_ENTRIES };

static int builtin_step(jsobj *it, jsv *out) {
    jsv t = it->u.it.target;
    if (t == JS_UNDEF) return 0;
    switch (it->u.it.kind) {
    case IT_ARR_VALUES: case IT_ARR_KEYS: case IT_ARR_ENTRIES: {
        u32 len;
        jsobj *a = jv_isobj(t) ? jv_o(t) : NULL;
        if (a && (a->cls == CL_ARRAY || a->cls == CL_ARGUMENTS)) len = a->alen;
        else { len = js_lengthof(t); if (js_pending()) return -1; }
        u32 i = it->u.it.pos;
        if (i >= len) { it->u.it.target = JS_UNDEF; return 0; }
        it->u.it.pos++;
        if (it->u.it.kind == IT_ARR_KEYS) { *out = jv_num(i); return 1; }
        jsv v = a && (a->cls == CL_ARRAY || a->cls == CL_ARGUMENTS) ? js_arr_get(a, i) : js_getidx(t, i);
        if (v == JS_EXC) return -1;
        if (it->u.it.kind == IT_ARR_VALUES) { *out = v; return 1; }
        jsv pair[2] = {jv_num(i), v};
        *out = jv_obj(js_arr_from(pair, 2));
        return 1;
    }
    case IT_STRING: {
        jsstr *s = jv_s(t);
        u32 p = it->u.it.pos;
        if (p >= s->len) { it->u.it.target = JS_UNDEF; return 0; }
        int i = (int)p;
        js_utf8_get(s->d, (int)s->len, &i);
        *out = jv_str(js_strn(s->d + p, i - (int)p));
        it->u.it.pos = (u32)i;
        return 1;
    }
    default: {
        jsobj *m = jv_o(t);
        jsobj *keys = m->u.map.keys, *vals = m->u.map.vals;
        while (it->u.it.pos < keys->alen && keys->arr[it->u.it.pos] == JS_HOLE) it->u.it.pos++;
        if (it->u.it.pos >= keys->alen) { it->u.it.target = JS_UNDEF; return 0; }
        u32 i = it->u.it.pos++;
        jsv k = keys->arr[i], v = vals ? vals->arr[i] : k;
        switch (it->u.it.kind) {
        case IT_MAP_KEYS: *out = k; break;
        case IT_MAP_VALUES: case IT_SET_VALUES: *out = v; break;
        default: { jsv pair[2] = {k, v}; *out = jv_obj(js_arr_from(pair, 2)); }
        }
        return 1;
    }
    }
}

NATIVE(iter_next) {
    UNUSED_ARGS;
    if (!jv_iscls(thisv, CL_ITER)) return js_throw_err(E_TYPE, "next() chamado em algo que não é iterador");
    jsv v = JS_UNDEF;
    int r = builtin_step(jv_o(thisv), &v);
    if (r < 0) return JS_EXC;
    jsobj *res = js_newobj(J.realm->p_object);
    js_defprop(res, A[A_value], r ? v : JS_UNDEF, 0);
    js_defprop(res, A[A_done], jv_bool(!r), 0);
    return jv_obj(res);
}

NATIVE(iter_self) { UNUSED_ARGS; return thisv; }

jsv js_iter_open(jsv v) {
    if ((jv_iscls(v, CL_ARRAY) && js_array_iter_pristine(v)) || jv_iscls(v, CL_ARGUMENTS)) return jv_obj(new_iter(v, IT_ARR_VALUES));
    if (jv_isstr(v) && !jv_s(v)->sym) return jv_obj(new_iter(v, IT_STRING));
    if (jv_isnullish(v)) return js_throw_err(E_TYPE, "%s não é iterável", v == JS_NULL ? "null" : "undefined");
    jsv f = js_getk(v, A[A_sym_iterator]);
    TRYV(f);
    if (!js_iscallable(f)) {
        char b[60];
        return js_throw_err(E_TYPE, "%s não é iterável", js_cstr(v, b, sizeof b));
    }
    jsv it = js_call(f, v, 0, NULL);
    TRYV(it);
    if (!jv_isobj(it)) return js_throw_err(E_TYPE, "O resultado de [Symbol.iterator]() não é um objeto");
    return it;
}

int js_iter_step(jsv it, jsv *out) {
    if (jv_iscls(it, CL_GEN) && !jv_o(it)->nprops && jv_o(it)->proto && jv_o(it)->proto->proto == J.realm->p_gen) return js_gen_step(jv_o(it), out);
    if (jv_iscls(it, CL_ITER)) {
        jsobj *io = jv_o(it);
        jsprop *p = io->nprops ? js_ownprop(io, A[A_next]) : NULL;
        if (!p) return builtin_step(io, out);
    }
    jsv r = js_invoke(it, A[A_next], 0, NULL);
    if (r == JS_EXC) return -1;
    if (!jv_isobj(r)) { js_throw_err(E_TYPE, "O resultado de next() não é um objeto"); return -1; }
    jsv d = js_getk(r, A[A_done]);
    if (d == JS_EXC) return -1;
    if (js_truthy(d)) return 0;
    jsv v = js_getk(r, A[A_value]);
    if (v == JS_EXC) return -1;
    *out = v;
    return 1;
}

/* ============================================================ String */
NATIVE(str_ctor) {
    UNUSED_ARGS;
    jsstr *s;
    if (argc == 0) s = A[A_empty];
    else if (!(flags & NF_NEW) && jv_issym(argv[0])) s = js_str_fmt("Symbol(%s)", jv_s(argv[0])->d);
    else s = js_tostr(argv[0]);
    if (!s) return JS_EXC;
    if (flags & NF_NEW) {
        jsobj *o = jv_o(thisv);
        o->cls = CL_STRING;
        o->u.prim = jv_str(s);
        return thisv;
    }
    return jv_str(s);
}

NATIVE(str_fromcharcode) {
    UNUSED_ARGS;
    u16 buf[256];
    u16 *u = argc <= 256 ? buf : kmalloc(sizeof(u16) * (usize)argc);
    if (!u) return JS_EXC;
    for (int i = 0; i < argc; i++) {
        u32 c = js_touint32(argv[i]);
        if (js_pending()) { if (u != buf) kfree(u); return JS_EXC; }
        u[i] = (u16)c;
    }
    jsstr *r = js_str_from_units(u, argc);
    if (u != buf) kfree(u);
    return jv_str(r);
}

NATIVE(str_fromcodepoint) {
    UNUSED_ARGS;
    sbuf_t sb = {0};
    for (int i = 0; i < argc; i++) {
        double d = js_tonum(argv[i]);
        if (js_pending()) { sb_free(&sb); return JS_EXC; }
        if (d < 0 || d > 0x10FFFF || d != m_trunc(d)) { sb_free(&sb); return js_throw_err(E_RANGE, "Code point inválido"); }
        char t[4];
        sb_putn(&sb, t, js_utf8_put(t, (u32)d));
    }
    jsstr *r = js_strn(sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
    return jv_str(r);
}

NATIVE(str_raw) {
    UNUSED_ARGS;
    jsv strs = ARG(0);
    jsv raw = jv_isobj(strs) ? js_getk(strs, A[A_raw]) : JS_UNDEF;
    TRYV(raw);
    if (!jv_isobj(raw)) return jv_str(A[A_empty]);
    u32 n = js_lengthof(raw);
    sbuf_t sb = {0};
    for (u32 i = 0; i < n; i++) {
        jsv p = js_getidx(raw, i);
        jsstr *ps = p == JS_EXC ? NULL : js_tostr(p);
        if (!ps) { sb_free(&sb); return JS_EXC; }
        sb_putn(&sb, ps->d, (int)ps->len);
        if (i + 1 < n && (int)i + 1 < argc) {
            jsstr *a = js_tostr(argv[i + 1]);
            if (!a) { sb_free(&sb); return JS_EXC; }
            sb_putn(&sb, a->d, (int)a->len);
        }
    }
    jsstr *r = js_strn(sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
    return jv_str(r);
}

NATIVE(strp_tostring) {
    UNUSED_ARGS;
    if (jv_isstr(thisv) && !jv_s(thisv)->sym) return thisv;
    if (jv_iscls(thisv, CL_STRING)) return jv_o(thisv)->u.prim;
    return js_throw_err(E_TYPE, "String.prototype.toString exige uma string");
}

NATIVE(strp_charat) {
    UNUSED_ARGS;
    jsstr *s = this_str(thisv, "charAt");
    if (!s) return JS_EXC;
    double i = arg_int(argc, argv, 0, 0);
    if (js_pending()) return JS_EXC;
    int mode = self->u.nat.magic;   /* 0 charAt, 1 charCodeAt, 2 codePointAt, 3 at */
    if (mode == 3 && i < 0) i += s->ulen;
    if (i < 0 || i >= s->ulen) return mode == 0 ? jv_str(A[A_empty]) : mode == 1 ? JS_NAN_BITS : JS_UNDEF;
    if (mode == 0 || mode == 3) return jv_str(js_str_usub(s, (int)i, (int)i + 1));
    u32 u = js_str_unit(s, (int)i);
    if (mode == 2 && u >= 0xD800 && u < 0xDC00) {
        u32 lo = js_str_unit(s, (int)i + 1);
        if (lo >= 0xDC00 && lo < 0xE000) return jv_num(0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00));
    }
    return jv_num(u);
}

static int find_bytes(jsstr *h, jsstr *n, int from_b) {
    if (n->len == 0) return from_b <= (int)h->len ? from_b : (int)h->len;
    for (int i = from_b; i + (int)n->len <= (int)h->len; i++)
        if (h->d[i] == n->d[0] && !memcmp(h->d + i, n->d, n->len)) return i;
    return -1;
}

NATIVE(strp_indexof) {
    UNUSED_ARGS;
    jsstr *s = this_str(thisv, "indexOf");
    if (!s) return JS_EXC;
    int mode = self->u.nat.magic;   /* 0 indexOf, 1 lastIndexOf, 2 includes, 3 startsWith, 4 endsWith */
    if (mode >= 2 && jv_iscls(ARG(0), CL_REGEXP)) return js_throw_err(E_TYPE, "O argumento não pode ser uma expressão regular");
    jsstr *n = arg_str(argc, argv, 0);
    if (!n) return JS_EXC;
    if (mode == 1) {
        double fp = argc > 1 ? js_tonum(argv[1]) : jv_n(JS_NAN_BITS);
        if (js_pending()) return JS_EXC;
        int from = fp != fp ? (int)s->ulen : (int)CLAMP(m_trunc(fp), 0, (double)s->ulen);
        int fb = js_str_boff(s, from);
        for (int i = MIN(fb, (int)s->len - (int)n->len); i >= 0; i--)
            if (!memcmp(s->d + i, n->d, n->len)) return jv_int(js_str_uidx(s, i));
        return jv_int(-1);
    }
    if (mode == 4) {
        int end = (int)(argc > 1 && argv[1] != JS_UNDEF ? CLAMP(js_tointeger(argv[1]), 0, (double)s->ulen) : s->ulen);
        int eb = js_str_boff(s, end);
        if ((int)n->len > eb) return JS_FALSE;
        return jv_bool(!memcmp(s->d + eb - n->len, n->d, n->len));
    }
    int from = (int)CLAMP(arg_int(argc, argv, 1, 0), 0, (double)s->ulen);
    if (js_pending()) return JS_EXC;
    int fb = js_str_boff(s, from);
    if (mode == 3) return jv_bool(fb + n->len <= s->len && !memcmp(s->d + fb, n->d, n->len));
    int r = find_bytes(s, n, fb);
    if (mode == 2) return jv_bool(r >= 0);
    return jv_int(r < 0 ? -1 : js_str_uidx(s, r));
}

NATIVE(strp_slice) {
    UNUSED_ARGS;
    jsstr *s = this_str(thisv, "slice");
    if (!s) return JS_EXC;
    int len = (int)s->ulen;
    int mode = self->u.nat.magic;   /* 0 slice, 1 substring, 2 substr */
    double a = arg_int(argc, argv, 0, 0);
    if (js_pending()) return JS_EXC;
    if (mode == 0) {
        double b = arg_int(argc, argv, 1, len);
        int x = clamp_index(a, len), y = clamp_index(b, len);
        return jv_str(js_str_usub(s, x, y));
    }
    if (mode == 1) {
        double b = arg_int(argc, argv, 1, len);
        int x = (int)CLAMP(a, 0, len), y = (int)CLAMP(b, 0, len);
        if (x > y) { int t = x; x = y; y = t; }
        return jv_str(js_str_usub(s, x, y));
    }
    int x = clamp_index(a, len);
    double l = arg_int(argc, argv, 1, len - x);
    int y = (int)CLAMP(x + l, x, len);
    return jv_str(js_str_usub(s, x, y));
}

static u32 cp_upper(u32 c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c < 128) return c;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 32;
    if (c == 0xFF) return 0x178;
    if (c >= 0x100 && c <= 0x17F && (c & 1) && c != 0x131 && c != 0x138 && c != 0x149 && !(c >= 0x139 && c <= 0x148)) return c - 1;
    if (c >= 0x139 && c <= 0x148 && !(c & 1)) return c - 1;
    if (c >= 0x3B1 && c <= 0x3C9 && c != 0x3C2) return c - 32;
    if (c >= 0x430 && c <= 0x44F) return c - 32;
    if (c >= 0x450 && c <= 0x45F) return c - 80;
    return c;
}

static u32 cp_lower(u32 c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c < 128) return c;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
    if (c == 0x178) return 0xFF;
    if (c >= 0x100 && c <= 0x17F && !(c & 1) && c != 0x130 && c != 0x138 && !(c >= 0x139 && c <= 0x148)) return c + 1;
    if (c >= 0x139 && c <= 0x148 && (c & 1)) return c + 1;
    if (c >= 0x391 && c <= 0x3A9) return c + 32;
    if (c >= 0x410 && c <= 0x42F) return c + 32;
    if (c >= 0x400 && c <= 0x40F) return c + 80;
    return c;
}

NATIVE(strp_case) {
    UNUSED_ARGS;
    jsstr *s = this_str(thisv, "toUpperCase");
    if (!s) return JS_EXC;
    bool up = self->u.nat.magic == 0;
    sbuf_t sb = {0};
    for (int i = 0; i < (int)s->len;) {
        if ((u8)s->d[i] < 128) {
            char c = s->d[i++];
            if (up && c >= 'a' && c <= 'z') c -= 32;
            else if (!up && c >= 'A' && c <= 'Z') c += 32;
            sb_putc(&sb, c);
            continue;
        }
        u32 c = js_utf8_get(s->d, (int)s->len, &i);
        if (up && c == 0xDF) { sb_puts(&sb, "SS"); continue; }
        c = up ? cp_upper(c) : cp_lower(c);
        char t[4];
        sb_putn(&sb, t, js_utf8_put(t, c));
    }
    jsstr *r = js_strn(sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
    return jv_str(r);
}

static bool ws_at(jsstr *s, int i, int *n) {
    u8 c = (u8)s->d[i];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0x0B || c == 0x0C) { *n = 1; return true; }
    if (c >= 0x80) {
        int k = i;
        u32 cp = js_utf8_get(s->d, (int)s->len, &k);
        if (cp == 0xA0 || cp == 0xFEFF || cp == 0x2028 || cp == 0x2029 || cp == 0x3000 || (cp >= 0x2000 && cp <= 0x200A) || cp == 0x202F || cp == 0x205F || cp == 0x1680) {
            *n = k - i;
            return true;
        }
    }
    return false;
}

NATIVE(strp_trim) {
    UNUSED_ARGS;
    jsstr *s = this_str(thisv, "trim");
    if (!s) return JS_EXC;
    int mode = self->u.nat.magic;   /* 0 ambos, 1 início, 2 fim */
    int a = 0, b = (int)s->len, n;
    if (mode != 2) while (a < b && ws_at(s, a, &n)) a += n;
    if (mode != 1) {
        while (b > a) {
            int p = b - 1;
            while (p > a && ((u8)s->d[p] & 0xC0) == 0x80) p--;
            if (!ws_at(s, p, &n)) break;
            b = p;
        }
    }
    if (a == 0 && b == (int)s->len) return jv_str(s);
    return jv_str(js_strn(s->d + a, b - a));
}

NATIVE(strp_pad) {
    UNUSED_ARGS;
    jsstr *s = this_str(thisv, "padStart");
    if (!s) return JS_EXC;
    double target = arg_int(argc, argv, 0, 0);
    if (js_pending()) return JS_EXC;
    jsstr *fill = ARG(1) == JS_UNDEF ? js_intern(" ", 1) : js_tostr(ARG(1));
    if (!fill) return JS_EXC;
    if (target <= s->ulen || !fill->len) return jv_str(s);
    if (target > (1 << 24)) return js_throw_err(E_RANGE, "Comprimento inválido");
    int need = (int)target - (int)s->ulen;
    sbuf_t pad = {0};
    while (need > 0) {
        if ((int)fill->ulen <= need) { sb_putn(&pad, fill->d, (int)fill->len); need -= (int)fill->ulen; }
        else { int b = js_str_boff(fill, need); sb_putn(&pad, fill->d, b); need = 0; }
    }
    sbuf_t out = {0};
    if (self->u.nat.magic == 0) { sb_putn(&out, pad.p, pad.len); sb_putn(&out, s->d, (int)s->len); }
    else { sb_putn(&out, s->d, (int)s->len); sb_putn(&out, pad.p, pad.len); }
    jsstr *r = js_strn(out.p, out.len);
    sb_free(&pad);
    sb_free(&out);
    return jv_str(r);
}

NATIVE(strp_repeat) {
    UNUSED_ARGS;
    jsstr *s = this_str(thisv, "repeat");
    if (!s) return JS_EXC;
    double n = arg_int(argc, argv, 0, 0);
    if (js_pending()) return JS_EXC;
    if (n < 0 || !m_isfinite(n)) return js_throw_err(E_RANGE, "Contagem inválida: %d", (int)n);
    if (n * s->len > (32 << 20)) return js_throw_err(E_RANGE, "String grande demais");
    sbuf_t sb = {0};
    for (int i = 0; i < (int)n; i++) sb_putn(&sb, s->d, (int)s->len);
    jsstr *r = js_strn(sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
    return jv_str(r);
}

NATIVE(strp_concat) {
    UNUSED_ARGS;
    jsstr *s = this_str(thisv, "concat");
    if (!s) return JS_EXC;
    for (int i = 0; i < argc; i++) {
        jsstr *a = js_tostr(argv[i]);
        if (!a) return JS_EXC;
        s = js_str_cat(s, a);
        if (!s) return JS_EXC;
    }
    return jv_str(s);
}

NATIVE(strp_split) {
    UNUSED_ARGS;
    jsstr *s = this_str(thisv, "split");
    if (!s) return JS_EXC;
    jsv sepv = ARG(0);
    u32 limit = ARG(1) == JS_UNDEF ? 0xFFFFFFFF : js_touint32(ARG(1));
    if (jv_iscls(sepv, CL_REGEXP)) return js_regexp_split(jv_o(sepv), s, limit);
    jsobj *out = js_newarr(0);
    if (!limit) return jv_obj(out);
    if (sepv == JS_UNDEF) { js_arr_push(out, jv_str(s)); return jv_obj(out); }
    jsstr *sep = js_tostr(sepv);
    if (!sep) return JS_EXC;
    if (!sep->len) {
        for (int i = 0; i < (int)s->ulen && out->alen < limit; i++) js_arr_push(out, jv_str(js_str_usub(s, i, i + 1)));
        return jv_obj(out);
    }
    int p = 0;
    for (;;) {
        int q = find_bytes(s, sep, p);
        if (q < 0) break;
        js_arr_push(out, jv_str(js_strn(s->d + p, q - p)));
        if (out->alen >= limit) return jv_obj(out);
        p = q + (int)sep->len;
    }
    js_arr_push(out, jv_str(js_strn(s->d + p, (int)s->len - p)));
    return jv_obj(out);
}

/* expande $&, $1.., $<nome>, $$, $`, $' */
jsstr *js_expand_replacement(jsstr *rep, jsstr *s, int mstart, int mend, int *caps, int ncaps, jsobj *groups) {
    if (!memchr_(rep->d, '$', rep->len)) return rep;
    sbuf_t sb = {0};
    for (u32 i = 0; i < rep->len; i++) {
        char c = rep->d[i];
        if (c != '$' || i + 1 >= rep->len) { sb_putc(&sb, c); continue; }
        char n = rep->d[i + 1];
        if (n == '$') { sb_putc(&sb, '$'); i++; }
        else if (n == '&') { sb_putn(&sb, s->d + mstart, mend - mstart); i++; }
        else if (n == '`') { sb_putn(&sb, s->d, mstart); i++; }
        else if (n == '\'') { sb_putn(&sb, s->d + mend, (int)s->len - mend); i++; }
        else if (n >= '0' && n <= '9') {
            int g = n - '0', used = 1;
            if (i + 2 < rep->len && rep->d[i + 2] >= '0' && rep->d[i + 2] <= '9' && g * 10 + (rep->d[i + 2] - '0') < ncaps) { g = g * 10 + (rep->d[i + 2] - '0'); used = 2; }
            if (g >= 1 && g < ncaps) {
                if (caps && caps[g * 2] >= 0) sb_putn(&sb, s->d + caps[g * 2], caps[g * 2 + 1] - caps[g * 2]);
                i += (u32)used;
            } else sb_putc(&sb, c);
        } else if (n == '<' && groups) {
            const char *e = memchr_(rep->d + i + 2, '>', rep->len - i - 2);
            if (!e) { sb_putc(&sb, c); continue; }
            jsstr *name = js_strn(rep->d + i + 2, (int)(e - rep->d - i - 2));
            jsv v = js_getk(jv_obj(groups), name);
            if (v != JS_EXC && v != JS_UNDEF) { jsstr *vs = js_tostr(v); if (vs) sb_putn(&sb, vs->d, (int)vs->len); }
            i = (u32)(e - rep->d);
        } else sb_putc(&sb, c);
    }
    jsstr *r = js_strn(sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
    return r;
}

NATIVE(strp_replace) {
    UNUSED_ARGS;
    jsstr *s = this_str(thisv, "replace");
    if (!s) return JS_EXC;
    bool all = self->u.nat.magic == 1;
    jsv pat = ARG(0), rep = ARG(1);
    if (jv_iscls(pat, CL_REGEXP)) {
        if (all && !(rx_flags(jv_o(pat)->u.re.prog) & RXF_G)) return js_throw_err(E_TYPE, "replaceAll exige uma expressão regular com a flag g");
        return js_regexp_replace(jv_o(pat), s, rep);
    }
    jsstr *p = js_tostr(pat);
    if (!p) return JS_EXC;
    jsstr *reps = NULL;
    if (!js_iscallable(rep)) { reps = js_tostr(rep); if (!reps) return JS_EXC; }
    sbuf_t sb = {0};
    int pos = 0;
    for (;;) {
        int q = find_bytes(s, p, pos);
        if (q < 0 || (q == (int)s->len && p->len == 0 && pos > 0 && all && q < pos)) break;
        sb_putn(&sb, s->d + pos, q - pos);
        if (reps) {
            jsstr *x = js_expand_replacement(reps, s, q, q + (int)p->len, NULL, 0, NULL);
            sb_putn(&sb, x->d, (int)x->len);
        } else {
            jsv args[3] = {jv_str(p), jv_int(js_str_uidx(s, q)), jv_str(s)};
            jsv r = js_call(rep, JS_UNDEF, 3, args);
            if (r == JS_EXC) { sb_free(&sb); return JS_EXC; }
            jsstr *rs = js_tostr(r);
            if (!rs) { sb_free(&sb); return JS_EXC; }
            sb_putn(&sb, rs->d, (int)rs->len);
        }
        pos = q + (int)p->len;
        if (!all) break;
        if (p->len == 0) {
            if (pos >= (int)s->len) { pos = (int)s->len; break; }
            int k = pos;
            js_utf8_get(s->d, (int)s->len, &k);
            sb_putn(&sb, s->d + pos, k - pos);
            pos = k;
            if (pos >= (int)s->len) {
                if (reps) { jsstr *x = js_expand_replacement(reps, s, pos, pos, NULL, 0, NULL); sb_putn(&sb, x->d, (int)x->len); }
                break;
            }
        }
    }
    if (pos < (int)s->len) sb_putn(&sb, s->d + pos, (int)s->len - pos);
    jsstr *r = js_strn(sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
    return jv_str(r);
}

NATIVE(strp_match) {
    UNUSED_ARGS;
    jsstr *s = this_str(thisv, "match");
    if (!s) return JS_EXC;
    int mode = self->u.nat.magic;   /* 0 match, 1 matchAll, 2 search */
    jsv pat = ARG(0);
    jsobj *re;
    if (jv_iscls(pat, CL_REGEXP)) re = jv_o(pat);
    else {
        jsstr *src = pat == JS_UNDEF ? A[A_empty] : js_tostr(pat);
        if (!src) return JS_EXC;
        jsv r = js_regexp_new(src, mode == 1 ? js_intern("g", 1) : A[A_empty]);
        TRYV(r);
        re = jv_o(r);
    }
    if (mode == 0) return js_regexp_match(re, s);
    if (mode == 2) return js_regexp_search(re, s);
    if (!(rx_flags(re->u.re.prog) & RXF_G)) return js_throw_err(E_TYPE, "matchAll exige uma expressão regular com a flag g");
    return js_regexp_matchall(re, s);
}

NATIVE(strp_localecompare) {
    UNUSED_ARGS;
    jsstr *s = this_str(thisv, "localeCompare");
    if (!s) return JS_EXC;
    jsstr *o = arg_str(argc, argv, 0);
    if (!o) return JS_EXC;
    /* compara ignorando acentos primeiro (aproximação de colação) */
    int i = 0, j = 0;
    while (i < (int)s->len && j < (int)o->len) {
        u32 a = js_utf8_get(s->d, (int)s->len, &i), b = js_utf8_get(o->d, (int)o->len, &j);
        extern u32 js_base_letter(u32 c);
        u32 x = js_base_letter(cp_lower(a)), y = js_base_letter(cp_lower(b));
        if (x != y) return jv_int(x < y ? -1 : 1);
    }
    if (i < (int)s->len) return jv_int(1);
    if (j < (int)o->len) return jv_int(-1);
    int c = js_str_cmp(s, o);
    return jv_int(c);
}

u32 js_base_letter(u32 c) {
    static const char map[] = "aaaaaaaceeeeiiiidnooooo/ouuuuyty";   /* U+00E0..U+00FF */
    if (c >= 0xE0 && c <= 0xFF && c != 0xF7) return (u32)map[c - 0xE0];
    return c;
}

NATIVE(strp_iterator) {
    UNUSED_ARGS;
    jsstr *s = this_str(thisv, "[Symbol.iterator]");
    if (!s) return JS_EXC;
    return jv_obj(new_iter(jv_str(s), IT_STRING));
}

NATIVE(strp_normalize) { UNUSED_ARGS; jsstr *s = this_str(thisv, "normalize"); return s ? jv_str(s) : JS_EXC; }

/* ============================================================ Number / Boolean / Symbol */
static double this_num(jsv thisv, bool *ok) {
    *ok = true;
    if (jv_isnum(thisv)) return jv_n(thisv);
    if (jv_iscls(thisv, CL_NUMBER)) return jv_n(jv_o(thisv)->u.prim);
    *ok = false;
    js_throw_err(E_TYPE, "Number.prototype chamado em algo que não é número");
    return 0;
}

NATIVE(num_ctor) {
    UNUSED_ARGS;
    jsv a0 = ARG(0);
    if (jv_isobj(a0)) { a0 = js_toprim(a0, 1); TRYV(a0); }
    double d = argc ? (jv_isbig(a0) ? js_big_to_double(jv_b(a0)) : js_tonum(a0)) : 0;
    if (js_pending()) return JS_EXC;
    if (flags & NF_NEW) {
        jsobj *o = jv_o(thisv);
        o->cls = CL_NUMBER;
        o->u.prim = jv_num(d);
        return thisv;
    }
    return jv_num(d);
}

NATIVE(num_is) {
    UNUSED_ARGS;
    jsv v = ARG(0);
    if (!jv_isnum(v)) return JS_FALSE;
    double d = jv_n(v);
    switch (self->u.nat.magic) {
    case 0: return jv_bool(m_isfinite(d) && m_trunc(d) == d);                 /* isInteger */
    case 1: return jv_bool(m_isfinite(d));                                     /* isFinite */
    case 2: return jv_bool(d != d);                                            /* isNaN */
    default: return jv_bool(m_isfinite(d) && m_trunc(d) == d && m_abs(d) <= 9007199254740991.0); /* isSafeInteger */
    }
}

NATIVE(nump_tostring) {
    UNUSED_ARGS;
    bool ok;
    double d = this_num(thisv, &ok);
    if (!ok) return JS_EXC;
    int radix = (int)arg_int(argc, argv, 0, 10);
    if (radix < 2 || radix > 36) return js_throw_err(E_RANGE, "toString() radix deve estar entre 2 e 36");
    char b[1200];
    js_num_radix(d, radix, b, sizeof b);
    return jv_str(js_strz(b));
}

NATIVE(nump_fixed) {
    UNUSED_ARGS;
    bool ok;
    double d = this_num(thisv, &ok);
    if (!ok) return JS_EXC;
    int mode = self->u.nat.magic;  /* 0 fixed, 1 precision, 2 exponential */
    char b[300];
    if (mode == 1 && ARG(0) == JS_UNDEF) { js_numfmt(d, b, sizeof b); return jv_str(js_strz(b)); }
    double f = arg_int(argc, argv, 0, mode == 2 ? -1 : 0);
    if (js_pending()) return JS_EXC;
    if (mode == 0) {
        if (f < 0 || f > 100) return js_throw_err(E_RANGE, "toFixed() aceita de 0 a 100 casas");
        js_num_fixed(d, (int)f, b, sizeof b);
    } else if (mode == 1) {
        if (f < 1 || f > 100) return js_throw_err(E_RANGE, "toPrecision() aceita de 1 a 100 dígitos");
        js_num_prec(d, (int)f, b, sizeof b);
    } else {
        if (ARG(0) != JS_UNDEF && (f < 0 || f > 100)) return js_throw_err(E_RANGE, "toExponential() aceita de 0 a 100 casas");
        js_num_expo(d, ARG(0) == JS_UNDEF ? -1 : (int)f, b, sizeof b);
    }
    return jv_str(js_strz(b));
}

/* toLocaleString no formato pt-BR (1.234.567,89) */
NATIVE(nump_locale) {
    UNUSED_ARGS;
    bool ok;
    double d = this_num(thisv, &ok);
    if (!ok) return JS_EXC;
    if (d != d || !m_isfinite(d)) { char b[32]; js_numfmt(d, b, sizeof b); return jv_str(js_strz(b)); }
    int minf = 0, maxf = 3;
    jsv opts = ARG(1);
    if (jv_isobj(opts)) {
        jsv a = js_getn(opts, "minimumFractionDigits"), b2 = js_getn(opts, "maximumFractionDigits");
        if (a == JS_EXC || b2 == JS_EXC) return JS_EXC;
        if (jv_isnum(a)) minf = (int)CLAMP(jv_n(a), 0, 20);
        if (jv_isnum(b2)) maxf = (int)CLAMP(jv_n(b2), 0, 20);
        if (maxf < minf) maxf = minf;
    }
    char b[300];
    js_num_fixed(d, maxf, b, sizeof b);
    char *dot = strchr(b, '.');
    if (dot) {
        int l = (int)strlen(b);
        while (l > 0 && b[l - 1] == '0' && (int)(&b[l - 1] - dot) > minf) b[--l] = 0;
        if (b[l - 1] == '.') b[--l] = 0;
    }
    char out[340];
    int k = 0;
    const char *p = b;
    if (*p == '-') out[k++] = *p++;
    const char *ip_end = strchr(p, '.');
    int ilen = ip_end ? (int)(ip_end - p) : (int)strlen(p);
    for (int i = 0; i < ilen; i++) {
        out[k++] = p[i];
        int rem = ilen - 1 - i;
        if (rem > 0 && rem % 3 == 0) out[k++] = '.';
    }
    if (ip_end) { out[k++] = ','; for (const char *q = ip_end + 1; *q; q++) out[k++] = *q; }
    out[k] = 0;
    return jv_str(js_strz(out));
}

NATIVE(nump_valueof) {
    UNUSED_ARGS;
    bool ok;
    double d = this_num(thisv, &ok);
    return ok ? jv_num(d) : JS_EXC;
}

NATIVE(bool_ctor) {
    UNUSED_ARGS;
    bool b = js_truthy(ARG(0));
    if (flags & NF_NEW) {
        jsobj *o = jv_o(thisv);
        o->cls = CL_BOOLEAN;
        o->u.prim = jv_bool(b);
        return thisv;
    }
    return jv_bool(b);
}

NATIVE(boolp_tostring) {
    UNUSED_ARGS;
    jsv v = thisv;
    if (jv_iscls(v, CL_BOOLEAN)) v = jv_o(v)->u.prim;
    if (!jv_isbool(v)) return js_throw_err(E_TYPE, "Boolean.prototype chamado em algo que não é booleano");
    if (self->u.nat.magic) return v;
    return jv_str(v == JS_TRUE ? A[A_true] : A[A_false]);
}

NATIVE(sym_ctor) {
    UNUSED_ARGS;
    if (flags & NF_NEW) return js_throw_err(E_TYPE, "Symbol não é um construtor");
    jsstr *d = ARG(0) == JS_UNDEF ? NULL : js_tostr(ARG(0));
    if (ARG(0) != JS_UNDEF && !d) return JS_EXC;
    return jv_str(js_new_symbol(d));
}

static jsobj *sym_registry;

NATIVE(sym_for) {
    UNUSED_ARGS;
    jsstr *k = js_tostr(ARG(0));
    if (!k) return JS_EXC;
    jsprop *p = js_ownprop(sym_registry, k);
    if (p) return p->v;
    jsv s = jv_str(js_new_symbol(k));
    js_defprop(sym_registry, k, s, 0);
    return s;
}

NATIVE(symp_tostring) {
    UNUSED_ARGS;
    jsv v = jv_iscls(thisv, CL_SYMBOL) ? jv_o(thisv)->u.prim : thisv;
    if (!jv_issym(v)) return js_throw_err(E_TYPE, "Symbol.prototype.toString chamado em algo que não é Symbol");
    if (self->u.nat.magic == 1) return jv_str(js_strn(jv_s(v)->d, (int)jv_s(v)->len));
    return jv_str(js_str_fmt("Symbol(%s)", jv_s(v)->d));
}

/* ============================================================ Math */
static u64 rng_state;

static double rnd(void) {
    if (!rng_state) rng_state = ((u64)rand32() << 32 | rand32()) ^ rdtsc() ^ 0x9E3779B97F4A7C15ull;
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return (double)(rng_state >> 11) * (1.0 / 9007199254740992.0);
}

NATIVE(math_fn1) {
    UNUSED_ARGS;
    double x = js_tonum(ARG(0));
    if (js_pending()) return JS_EXC;
    double r;
    switch (self->u.nat.magic) {
    case 0: r = m_abs(x); break;
    case 1: r = m_floor(x); break;
    case 2: r = m_ceil(x); break;
    case 3: r = m_round(x); break;
    case 4: r = m_trunc(x); break;
    case 5: r = x != x ? x : x > 0 ? 1 : x < 0 ? -1 : x; break;
    case 6: r = m_sqrt(x); break;
    case 7: r = x < 0 ? -m_pow(-x, 1.0 / 3) : m_pow(x, 1.0 / 3); { double rr = m_round(r); if (rr * rr * rr == x) r = rr; } break;
    case 8: r = m_exp(x); break;
    case 9: r = m_log(x); break;
    case 10: r = m_log2(x); break;
    case 11: r = m_log10(x); break;
    case 12: r = m_abs(x) < 1e-5 ? x - x * x / 2 + x * x * x / 3 : m_log(1 + x); break;
    case 13: r = m_abs(x) < 1e-5 ? x + x * x / 2 + x * x * x / 6 : m_exp(x) - 1; break;
    case 14: r = m_sin(x); break;
    case 15: r = m_cos(x); break;
    case 16: r = m_tan(x); break;
    case 17: r = m_asin(x); break;
    case 18: r = m_acos(x); break;
    case 19: r = m_atan(x); break;
    case 20: r = (m_exp(x) - m_exp(-x)) / 2; break;
    case 21: r = (m_exp(x) + m_exp(-x)) / 2; break;
    case 22: { if (x > 20) r = 1; else if (x < -20) r = -1; else { double e = m_exp(2 * x); r = (e - 1) / (e + 1); } break; }
    case 23: r = (double)(float)x; break;
    case 24: { u32 v = js_touint32(jv_num(x)); int n = 0; if (!v) n = 32; else while (!(v & 0x80000000u)) { v <<= 1; n++; } r = n; break; }
    case 25: r = m_log(x + m_sqrt(x * x + 1)); break;
    case 26: r = m_log(x + m_sqrt(x * x - 1)); break;
    case 27: r = 0.5 * m_log((1 + x) / (1 - x)); break;
    default: r = jv_n(JS_NAN_BITS);
    }
    return jv_num(r);
}

NATIVE(math_minmax) {
    UNUSED_ARGS;
    bool mx = self->u.nat.magic == 1;
    double r = mx ? -JS_INF : JS_INF;
    bool nan = false;
    for (int i = 0; i < argc; i++) {
        double x = js_tonum(argv[i]);
        if (js_pending()) return JS_EXC;
        if (x != x) nan = true;
        if (mx ? (x > r || (x == 0 && r == 0 && 1 / r < 0)) : (x < r || (x == 0 && r == 0 && 1 / x < 0))) r = x;
    }
    return nan ? JS_NAN_BITS : jv_num(r);
}

NATIVE(math_fn2) {
    UNUSED_ARGS;
    double x = js_tonum(ARG(0));
    if (js_pending()) return JS_EXC;
    double y = js_tonum(ARG(1));
    if (js_pending()) return JS_EXC;
    switch (self->u.nat.magic) {
    case 0: return jv_num(m_pow(x, y));
    case 1: return jv_num(m_atan2(x, y));
    case 2: return jv_num((double)(i32)((u32)js_toint32(jv_num(x)) * (u32)js_toint32(jv_num(y))));
    }
    return JS_UNDEF;
}

NATIVE(math_hypot) {
    UNUSED_ARGS;
    double s = 0;
    for (int i = 0; i < argc; i++) {
        double x = js_tonum(argv[i]);
        if (js_pending()) return JS_EXC;
        if (!m_isfinite(x) && x == x) return jv_num(JS_INF);
        s += x * x;
    }
    return jv_num(m_sqrt(s));
}

NATIVE(math_random) { UNUSED_ARGS; return jv_num(rnd()); }

/* ============================================================ Error */
static const char *err_names[E_COUNT] = {"Error", "TypeError", "RangeError", "SyntaxError", "ReferenceError", "EvalError", "URIError"};

NATIVE(err_ctor) {
    UNUSED_ARGS;
    int type = self->u.nat.magic;
    jsobj *e;
    if (flags & NF_NEW) { e = jv_o(thisv); e->cls = CL_ERROR; }
    else e = js_newobj_cls(CL_ERROR, J.realm->p_err[type]);
    if (ARG(0) != JS_UNDEF) {
        jsstr *m = js_tostr(ARG(0));
        if (!m) return JS_EXC;
        js_defprop(e, A[A_message], jv_str(m), PF_NOENUM);
    }
    if (jv_isobj(ARG(1))) {
        int h = js_has(ARG(1), jv_str(A[A_cause]));
        if (h < 0) return JS_EXC;
        if (h) { jsv c = js_getk(ARG(1), A[A_cause]); TRYV(c); js_defprop(e, A[A_cause], c, PF_NOENUM); }
    }
    js_defprop(e, A[A_lineNumber], jv_int(J.line), PF_NOENUM);
    js_defprop(e, A[A_stack], jv_str(js_str_fmt("%s: %s\n    em %s:%d", err_names[type], ARG(0) == JS_UNDEF ? "" : js_tostr(ARG(0))->d,
                                                 J.srcname ? J.srcname->d : "script", J.line)), PF_NOENUM);
    return jv_obj(e);
}

NATIVE(errp_tostring) {
    UNUSED_ARGS;
    if (!jv_isobj(thisv)) return js_throw_err(E_TYPE, "Error.prototype.toString chamado em algo que não é objeto");
    jsv n = js_getk(thisv, A[A_name]);
    TRYV(n);
    jsv m = js_getk(thisv, A[A_message]);
    TRYV(m);
    jsstr *ns = n == JS_UNDEF ? js_intern("Error", 5) : js_tostr(n);
    if (!ns) return JS_EXC;
    jsstr *ms = m == JS_UNDEF ? A[A_empty] : js_tostr(m);
    if (!ms) return JS_EXC;
    if (!ns->len) return jv_str(ms);
    if (!ms->len) return jv_str(ns);
    return jv_str(js_str_fmt("%s: %s", ns->d, ms->d));
}

/* ============================================================ Map / Set */
static int map_find(jsobj *m, jsv k) {
    jsobj *keys = m->u.map.keys;
    if (jv_isnum(k) && jv_n(k) == 0) k = jv_num(0);
    for (u32 i = 0; i < keys->alen; i++) {
        jsv x = keys->arr[i];
        if (x == JS_HOLE) continue;
        if (js_samevalue(x, k, true)) return (int)i;
    }
    return -1;
}

static jsobj *this_map(jsv thisv, int cls, const char *what) {
    if (!jv_iscls(thisv, cls)) { js_throw_err(E_TYPE, "%s chamado em objeto incompatível", what); return NULL; }
    return jv_o(thisv);
}

static void map_compact(jsobj *m) {
    jsobj *k = m->u.map.keys, *v = m->u.map.vals;
    if (k->alen < 32 || m->u.map.live * 2 > k->alen) return;
    u32 j = 0;
    for (u32 i = 0; i < k->alen; i++) {
        if (k->arr[i] == JS_HOLE) continue;
        k->arr[j] = k->arr[i];
        if (v) v->arr[j] = v->arr[i];
        j++;
    }
    k->alen = j;
    if (v) v->alen = j;
}

static void map_put(jsobj *m, jsv k, jsv v) {
    if (jv_isnum(k) && jv_n(k) == 0) k = jv_num(0);
    int i = map_find(m, k);
    if (i >= 0) { if (m->u.map.vals) m->u.map.vals->arr[i] = v; return; }
    js_arr_push(m->u.map.keys, k);
    if (m->u.map.vals) js_arr_push(m->u.map.vals, v);
    m->u.map.live++;
}

NATIVE(map_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor %s exige 'new'", self->u.nat.magic ? "Set" : "Map");
    bool isset = self->u.nat.magic == 1;
    jsobj *m = jv_o(thisv);
    m->cls = isset ? CL_SET : CL_MAP;
    m->u.map.keys = js_newarr(0);
    m->u.map.vals = isset ? NULL : js_newarr(0);
    m->u.map.live = 0;
    jsv src = ARG(0);
    if (jv_isnullish(src)) return thisv;
    jsv it = js_iter_open(src);
    TRYV(it);
    for (;;) {
        jsv e;
        int r = js_iter_step(it, &e);
        if (r < 0) return JS_EXC;
        if (!r) break;
        if (isset) map_put(m, e, JS_UNDEF);
        else {
            if (!jv_isobj(e)) return js_throw_err(E_TYPE, "Entrada de Map deve ser um par [chave, valor]");
            jsv k = js_getidx(e, 0);
            TRYV(k);
            jsv v = js_getidx(e, 1);
            TRYV(v);
            map_put(m, k, v);
        }
    }
    return thisv;
}

NATIVE(mapp_op) {
    UNUSED_ARGS;
    int op = self->u.nat.magic & 15;
    int cls = (self->u.nat.magic & 16) ? CL_SET : CL_MAP;
    jsobj *m = this_map(thisv, cls, cls == CL_SET ? "Set.prototype" : "Map.prototype");
    if (!m) return JS_EXC;
    switch (op) {
    case 0: { int i = map_find(m, ARG(0)); return i >= 0 ? m->u.map.vals->arr[i] : JS_UNDEF; }       /* get */
    case 1: map_put(m, ARG(0), ARG(1)); return thisv;                                                /* set / add */
    case 2: return jv_bool(map_find(m, ARG(0)) >= 0);                                                /* has */
    case 3: {                                                                                        /* delete */
        int i = map_find(m, ARG(0));
        if (i < 0) return JS_FALSE;
        m->u.map.keys->arr[i] = JS_HOLE;
        if (m->u.map.vals) m->u.map.vals->arr[i] = JS_UNDEF;
        m->u.map.live--;
        map_compact(m);
        return JS_TRUE;
    }
    case 4: m->u.map.keys->alen = 0; if (m->u.map.vals) m->u.map.vals->alen = 0; m->u.map.live = 0; return JS_UNDEF; /* clear */
    case 5: return jv_int((int)m->u.map.live);                                                       /* size */
    case 6: {                                                                                        /* forEach */
        jsv fn = need_fn(ARG(0), "forEach");
        TRYV(fn);
        for (u32 i = 0; i < m->u.map.keys->alen; i++) {
            jsv k = m->u.map.keys->arr[i];
            if (k == JS_HOLE) continue;
            jsv v = m->u.map.vals ? m->u.map.vals->arr[i] : k;
            jsv r = call3(fn, ARG(1), v, k, thisv);
            TRYV(r);
        }
        return JS_UNDEF;
    }
    case 7: return jv_obj(new_iter(thisv, cls == CL_SET ? IT_SET_VALUES : IT_MAP_ENTRIES));         /* entries / iterator */
    case 8: return jv_obj(new_iter(thisv, cls == CL_SET ? IT_SET_VALUES : IT_MAP_KEYS));            /* keys */
    case 9: return jv_obj(new_iter(thisv, cls == CL_SET ? IT_SET_VALUES : IT_MAP_VALUES));          /* values */
    case 10: return jv_obj(new_iter(thisv, IT_SET_ENTRIES));                                         /* Set entries */
    }
    return JS_UNDEF;
}

/* ============================================================ funções globais */
NATIVE(g_parseint) {
    UNUSED_ARGS;
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    int radix = (int)js_tointeger(ARG(1));
    if (js_pending()) return JS_EXC;
    int i = 0, n = (int)s->len, k;
    while (i < n && ws_at(s, i, &k)) i += k;
    bool neg = false;
    if (i < n && (s->d[i] == '+' || s->d[i] == '-')) { neg = s->d[i] == '-'; i++; }
    if (radix == 0) radix = 10;
    if (radix < 2 || radix > 36) return JS_NAN_BITS;
    if ((radix == 16 || ARG(1) == JS_UNDEF || (int)js_tointeger(ARG(1)) == 0) && i + 1 < n && s->d[i] == '0' && (s->d[i + 1] | 32) == 'x') { radix = 16; i += 2; }
    double v = 0;
    int start = i;
    if (radix == 10) {
        while (i < n && isdigit((u8)s->d[i])) i++;
        if (i == start) return JS_NAN_BITS;
        int used;
        v = js_strtod(s->d + start, i - start, &used);
        return jv_num(neg ? -v : v);
    }
    for (; i < n; i++) {
        int c = s->d[i], d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if ((c | 32) >= 'a' && (c | 32) <= 'z') d = (c | 32) - 'a' + 10;
        else break;
        if (d >= radix) break;
        v = v * radix + d;
    }
    if (i == start) return JS_NAN_BITS;
    return jv_num(neg ? -v : v);
}

NATIVE(g_parsefloat) {
    UNUSED_ARGS;
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    int i = 0, n = (int)s->len, k;
    while (i < n && ws_at(s, i, &k)) i += k;
    bool neg = false;
    if (i < n && (s->d[i] == '+' || s->d[i] == '-')) { neg = s->d[i] == '-'; i++; }
    if (n - i >= 8 && !strncmp(s->d + i, "Infinity", 8)) return jv_num(neg ? -JS_INF : JS_INF);
    /* js_strtod aceita '_' como separador; corta antes */
    int e = i;
    while (e < n && s->d[e] != '_') e++;
    int used;
    double v = js_strtod(s->d + i, e - i, &used);
    if (!used) return JS_NAN_BITS;
    return jv_num(neg ? -v : v);
}

NATIVE(g_isnan) {
    UNUSED_ARGS;
    double d = js_tonum(ARG(0));
    if (js_pending()) return JS_EXC;
    return jv_bool(self->u.nat.magic ? m_isfinite(d) : d != d);
}

static const char hexdig[] = "0123456789ABCDEF";

NATIVE(g_encode) {
    UNUSED_ARGS;
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    bool comp = self->u.nat.magic == 0;
    const char *keep = comp ? "-_.!~*'()" : "-_.!~*'();/?:@&=+$,#";
    sbuf_t sb = {0};
    for (u32 i = 0; i < s->len; i++) {
        u8 c = (u8)s->d[i];
        if (isalnum(c) || (c < 128 && strchr(keep, c) && c)) sb_putc(&sb, (char)c);
        else { sb_putc(&sb, '%'); sb_putc(&sb, hexdig[c >> 4]); sb_putc(&sb, hexdig[c & 15]); }
    }
    jsstr *r = js_strn(sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
    return jv_str(r);
}

static int hexv(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if ((c | 32) >= 'a' && (c | 32) <= 'f') return (c | 32) - 'a' + 10;
    return -1;
}

NATIVE(g_decode) {
    UNUSED_ARGS;
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    bool comp = self->u.nat.magic == 0;
    sbuf_t sb = {0};
    for (u32 i = 0; i < s->len; i++) {
        char c = s->d[i];
        if (c == '%') {
            if (i + 2 >= s->len || hexv(s->d[i + 1]) < 0 || hexv(s->d[i + 2]) < 0) { sb_free(&sb); return js_throw_err(E_URI, "URI malformada"); }
            int v = hexv(s->d[i + 1]) * 16 + hexv(s->d[i + 2]);
            if (!comp && v < 128 && strchr(";/?:@&=+$,#", v) && v) { sb_putn(&sb, s->d + i, 3); i += 2; continue; }
            sb_putc(&sb, (char)v);
            i += 2;
        } else sb_putc(&sb, c);
    }
    if (!utf8_valid((const u8 *)(sb.p ? sb.p : ""), (usize)sb.len)) { sb_free(&sb); return js_throw_err(E_URI, "URI malformada"); }
    jsstr *r = js_strn(sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
    return jv_str(r);
}

NATIVE(g_eval) {
    UNUSED_ARGS;
    jsv c = ARG(0);
    if (!jv_isstr(c)) return c;
    return js_eval_string(jv_s(c), "eval");
}

/* raízes estáticas */
void js_lib1_mark(void) { gc_mark_ptr(sym_registry); gc_mark_val(orig_array_iter); }

/* ============================================================ montagem */
static jsobj *mkproto(jsobj *proto) { return js_newobj(proto); }

void js_lib2_init(jsrealm *r);

void js_lib_init(jsrealm *r) {
    jsobj *op = js_newobj(NULL);
    r->p_object = op;
    jsobj *fp = js_newobj_cls(CL_NATIVE, op);
    fp->u.nat.fn = fn_proto_call;
    fp->u.nat.data = fp->u.nat.data2 = JS_UNDEF;
    r->p_function = fp;
    jsobj *g = js_newobj(op);
    r->global = g;
    r->glex = js_newenv(NULL, 8);
    r->glex->has_this = 1;
    r->glex->thisv = jv_obj(g);
    r->p_array = js_newobj_cls(CL_ARRAY, op);
    r->p_string = js_newobj_cls(CL_STRING, op);
    r->p_string->u.prim = jv_str(A[A_empty]);
    r->p_number = js_newobj_cls(CL_NUMBER, op);
    r->p_number->u.prim = jv_num(0);
    r->p_boolean = js_newobj_cls(CL_BOOLEAN, op);
    r->p_boolean->u.prim = JS_FALSE;
    r->p_symbol = mkproto(op);
    r->p_iter = mkproto(op);
    r->p_map = mkproto(op);
    r->p_set = mkproto(op);
    r->jobs = js_newarr(0);
    if (!sym_registry) sym_registry = js_newobj(NULL);
    js_defn(fp, "length", jv_int(0), PF_NOENUM | PF_RO);
    js_defn(fp, "name", jv_str(A[A_empty]), PF_NOENUM | PF_RO);

    /* Object */
    jsobj *c = js_newctor(obj_ctor, "Object", 1, op);
    r->c_object = c;
    js_defn(g, "Object", jv_obj(c), PF_NOENUM);
    js_defnative(c, "keys", obj_keys, 1, 0);
    js_defnative(c, "values", obj_keys, 1, 1);
    js_defnative(c, "entries", obj_keys, 1, 2);
    js_defnative(c, "assign", obj_assign, 2, 0);
    js_defnative(c, "freeze", obj_freeze, 1, 0);
    js_defnative(c, "seal", obj_freeze, 1, 1);
    js_defnative(c, "preventExtensions", obj_freeze, 1, 2);
    js_defnative(c, "isFrozen", obj_isfrozen, 1, 0);
    js_defnative(c, "isSealed", obj_isfrozen, 1, 1);
    js_defnative(c, "isExtensible", obj_isfrozen, 1, 2);
    js_defnative(c, "create", obj_create, 2, 0);
    js_defnative(c, "getPrototypeOf", obj_getproto, 1, 0);
    js_defnative(c, "setPrototypeOf", obj_setproto, 2, 0);
    js_defnative(c, "defineProperty", obj_defprop, 3, 0);
    js_defnative(c, "defineProperties", obj_defprops, 2, 0);
    js_defnative(c, "getOwnPropertyNames", obj_getownnames, 1, 0);
    js_defnative(c, "getOwnPropertyDescriptor", obj_getdesc, 2, 0);
    js_defnative(c, "fromEntries", obj_fromentries, 1, 0);
    js_defnative(c, "is", obj_is, 2, 0);
    js_defnative(c, "hasOwn", obj_hasown, 2, 0);
    js_defnative(op, "hasOwnProperty", objp_hasown, 1, 0);
    js_defnative(op, "isPrototypeOf", objp_isproto, 1, 0);
    js_defnative(op, "propertyIsEnumerable", objp_propenum, 1, 0);
    js_defnative(op, "toString", objp_tostring, 0, 0);
    js_defnative(op, "toLocaleString", objp_tolocale, 0, 0);
    js_defnative(op, "valueOf", objp_valueof, 0, 0);
    js_defaccessor(op, js_intern("__proto__", -1), jv_obj(js_newnative(objp_proto_get, "get __proto__", 0, 0)),
                   jv_obj(js_newnative(objp_proto_set, "set __proto__", 1, 0)), PF_NOENUM);

    /* Function */
    c = js_newctor(fn_ctor, "Function", 1, fp);
    r->c_function = c;
    js_defn(g, "Function", jv_obj(c), PF_NOENUM);
    js_defnative(fp, "call", fnp_call, 1, 0);
    js_defnative(fp, "apply", fnp_apply, 2, 0);
    js_defnative(fp, "bind", fnp_bind, 1, 0);
    js_defnative(fp, "toString", fnp_tostring, 0, 0);
    js_defprop(fp, A[A_sym_hasInstance], jv_obj(js_newnative(fnp_hasinstance, "[Symbol.hasInstance]", 1, 0)), PF_NOENUM);

    /* Array */
    jsobj *ap = r->p_array;
    c = js_newctor(arr_ctor, "Array", 1, ap);
    r->c_array = c;
    js_defn(g, "Array", jv_obj(c), PF_NOENUM);
    js_defnative(c, "isArray", arr_isarray, 1, 0);
    js_defnative(c, "from", arr_from, 1, 0);
    js_defnative(c, "of", arr_of, 0, 0);
    js_defnative(ap, "push", arrp_push, 1, 0);
    js_defnative(ap, "pop", arrp_pop, 0, 0);
    js_defnative(ap, "shift", arrp_shift, 0, 0);
    js_defnative(ap, "unshift", arrp_unshift, 1, 0);
    js_defnative(ap, "slice", arrp_slice, 2, 0);
    js_defnative(ap, "splice", arrp_splice, 2, 0);
    js_defnative(ap, "concat", arrp_concat, 1, 0);
    js_defnative(ap, "join", arrp_join, 1, 0);
    js_defnative(ap, "toString", arrp_tostring, 0, 0);
    js_defnative(ap, "toLocaleString", arrp_tostring, 0, 0);
    js_defnative(ap, "reverse", arrp_reverse, 0, 0);
    js_defnative(ap, "indexOf", arrp_indexof, 1, 0);
    js_defnative(ap, "lastIndexOf", arrp_indexof, 1, 1);
    js_defnative(ap, "includes", arrp_indexof, 1, 2);
    js_defnative(ap, "find", arrp_iter, 1, 0);
    js_defnative(ap, "findIndex", arrp_iter, 1, 1);
    js_defnative(ap, "findLast", arrp_iter, 1, 2);
    js_defnative(ap, "findLastIndex", arrp_iter, 1, 3);
    js_defnative(ap, "filter", arrp_iter, 1, 4);
    js_defnative(ap, "map", arrp_iter, 1, 5);
    js_defnative(ap, "forEach", arrp_iter, 1, 6);
    js_defnative(ap, "some", arrp_iter, 1, 7);
    js_defnative(ap, "every", arrp_iter, 1, 8);
    js_defnative(ap, "reduce", arrp_reduce, 1, 0);
    js_defnative(ap, "reduceRight", arrp_reduce, 1, 1);
    js_defnative(ap, "sort", arrp_sort, 1, 0);
    js_defnative(ap, "fill", arrp_fill, 1, 0);
    js_defnative(ap, "flat", arrp_flat, 0, 0);
    js_defnative(ap, "flatMap", arrp_flatmap, 1, 0);
    js_defnative(ap, "at", arrp_at, 1, 0);
    js_defnative(ap, "keys", arrp_iterator, 0, IT_ARR_KEYS);
    js_defnative(ap, "entries", arrp_iterator, 0, IT_ARR_ENTRIES);
    jsobj *values = js_defnative(ap, "values", arrp_iterator, 0, IT_ARR_VALUES);
    js_defprop(ap, A[A_sym_iterator], jv_obj(values), PF_NOENUM);
    orig_array_iter = jv_obj(values);

    /* iteradores */
    js_defnative(r->p_iter, "next", iter_next, 0, 0);
    js_defprop(r->p_iter, A[A_sym_iterator], jv_obj(js_newnative(iter_self, "[Symbol.iterator]", 0, 0)), PF_NOENUM);
    js_gen_init(r);

    /* String */
    jsobj *sp = r->p_string;
    c = js_newctor(str_ctor, "String", 1, sp);
    r->c_string = c;
    js_defn(g, "String", jv_obj(c), PF_NOENUM);
    js_defnative(c, "fromCharCode", str_fromcharcode, 1, 0);
    js_defnative(c, "fromCodePoint", str_fromcodepoint, 1, 0);
    js_defnative(c, "raw", str_raw, 1, 0);
    js_defnative(sp, "toString", strp_tostring, 0, 0);
    js_defnative(sp, "valueOf", strp_tostring, 0, 0);
    js_defnative(sp, "charAt", strp_charat, 1, 0);
    js_defnative(sp, "charCodeAt", strp_charat, 1, 1);
    js_defnative(sp, "codePointAt", strp_charat, 1, 2);
    js_defnative(sp, "at", strp_charat, 1, 3);
    js_defnative(sp, "indexOf", strp_indexof, 1, 0);
    js_defnative(sp, "lastIndexOf", strp_indexof, 1, 1);
    js_defnative(sp, "includes", strp_indexof, 1, 2);
    js_defnative(sp, "startsWith", strp_indexof, 1, 3);
    js_defnative(sp, "endsWith", strp_indexof, 1, 4);
    js_defnative(sp, "slice", strp_slice, 2, 0);
    js_defnative(sp, "substring", strp_slice, 2, 1);
    js_defnative(sp, "substr", strp_slice, 2, 2);
    js_defnative(sp, "toUpperCase", strp_case, 0, 0);
    js_defnative(sp, "toLowerCase", strp_case, 0, 1);
    js_defnative(sp, "toLocaleUpperCase", strp_case, 0, 0);
    js_defnative(sp, "toLocaleLowerCase", strp_case, 0, 1);
    js_defnative(sp, "trim", strp_trim, 0, 0);
    js_defnative(sp, "trimStart", strp_trim, 0, 1);
    js_defnative(sp, "trimEnd", strp_trim, 0, 2);
    js_defnative(sp, "trimLeft", strp_trim, 0, 1);
    js_defnative(sp, "trimRight", strp_trim, 0, 2);
    js_defnative(sp, "padStart", strp_pad, 2, 0);
    js_defnative(sp, "padEnd", strp_pad, 2, 1);
    js_defnative(sp, "repeat", strp_repeat, 1, 0);
    js_defnative(sp, "concat", strp_concat, 1, 0);
    js_defnative(sp, "split", strp_split, 2, 0);
    js_defnative(sp, "replace", strp_replace, 2, 0);
    js_defnative(sp, "replaceAll", strp_replace, 2, 1);
    js_defnative(sp, "match", strp_match, 1, 0);
    js_defnative(sp, "matchAll", strp_match, 1, 1);
    js_defnative(sp, "search", strp_match, 1, 2);
    js_defnative(sp, "localeCompare", strp_localecompare, 1, 0);
    js_defnative(sp, "normalize", strp_normalize, 0, 0);
    js_defprop(sp, A[A_sym_iterator], jv_obj(js_newnative(strp_iterator, "[Symbol.iterator]", 0, 0)), PF_NOENUM);

    /* Number */
    jsobj *np = r->p_number;
    c = js_newctor(num_ctor, "Number", 1, np);
    r->c_number = c;
    js_defn(g, "Number", jv_obj(c), PF_NOENUM);
    js_defnative(c, "isInteger", num_is, 1, 0);
    js_defnative(c, "isFinite", num_is, 1, 1);
    js_defnative(c, "isNaN", num_is, 1, 2);
    js_defnative(c, "isSafeInteger", num_is, 1, 3);
    js_defn(c, "MAX_SAFE_INTEGER", jv_num(9007199254740991.0), PF_NOENUM | PF_RO);
    js_defn(c, "MIN_SAFE_INTEGER", jv_num(-9007199254740991.0), PF_NOENUM | PF_RO);
    js_defn(c, "EPSILON", jv_num(2.220446049250313e-16), PF_NOENUM | PF_RO);
    js_defn(c, "MAX_VALUE", jv_num(1.7976931348623157e308), PF_NOENUM | PF_RO);
    js_defn(c, "MIN_VALUE", jv_num(5e-324), PF_NOENUM | PF_RO);
    js_defn(c, "POSITIVE_INFINITY", jv_num(JS_INF), PF_NOENUM | PF_RO);
    js_defn(c, "NEGATIVE_INFINITY", jv_num(-JS_INF), PF_NOENUM | PF_RO);
    js_defn(c, "NaN", JS_NAN_BITS, PF_NOENUM | PF_RO);
    js_defnative(np, "toString", nump_tostring, 1, 0);
    js_defnative(np, "toFixed", nump_fixed, 1, 0);
    js_defnative(np, "toPrecision", nump_fixed, 1, 1);
    js_defnative(np, "toExponential", nump_fixed, 1, 2);
    js_defnative(np, "toLocaleString", nump_locale, 0, 0);
    js_defnative(np, "valueOf", nump_valueof, 0, 0);

    /* Boolean */
    c = js_newctor(bool_ctor, "Boolean", 1, r->p_boolean);
    js_defn(g, "Boolean", jv_obj(c), PF_NOENUM);
    js_defnative(r->p_boolean, "toString", boolp_tostring, 0, 0);
    js_defnative(r->p_boolean, "valueOf", boolp_tostring, 0, 1);

    /* Symbol */
    c = js_newctor(sym_ctor, "Symbol", 0, r->p_symbol);
    r->c_symbol = c;
    js_defn(g, "Symbol", jv_obj(c), PF_NOENUM);
    js_defnative(c, "for", sym_for, 1, 0);
    js_defn(c, "iterator", jv_str(A[A_sym_iterator]), PF_NOENUM | PF_RO);
    js_defn(c, "hasInstance", jv_str(A[A_sym_hasInstance]), PF_NOENUM | PF_RO);
    js_defn(c, "toPrimitive", jv_str(A[A_sym_toPrimitive]), PF_NOENUM | PF_RO);
    js_defn(c, "toStringTag", jv_str(A[A_sym_toStringTag]), PF_NOENUM | PF_RO);
    js_defn(c, "asyncIterator", jv_str(A[A_sym_asyncIterator]), PF_NOENUM | PF_RO);
    js_defnative(r->p_symbol, "toString", symp_tostring, 0, 0);
    js_defgetter(r->p_symbol, "description", symp_tostring, 1);

    /* Math */
    jsobj *m = js_newobj(op);
    js_defn(g, "Math", jv_obj(m), PF_NOENUM);
    static const char *f1[] = {"abs", "floor", "ceil", "round", "trunc", "sign", "sqrt", "cbrt", "exp", "log", "log2", "log10",
                               "log1p", "expm1", "sin", "cos", "tan", "asin", "acos", "atan", "sinh", "cosh", "tanh", "fround",
                               "clz32", "asinh", "acosh", "atanh"};
    for (u32 i = 0; i < ARRAY_LEN(f1); i++) js_defnative(m, f1[i], math_fn1, 1, (int)i);
    js_defnative(m, "min", math_minmax, 2, 0);
    js_defnative(m, "max", math_minmax, 2, 1);
    js_defnative(m, "pow", math_fn2, 2, 0);
    js_defnative(m, "atan2", math_fn2, 2, 1);
    js_defnative(m, "imul", math_fn2, 2, 2);
    js_defnative(m, "hypot", math_hypot, 2, 0);
    js_defnative(m, "random", math_random, 0, 0);
    js_defn(m, "PI", jv_num(3.141592653589793), PF_NOENUM | PF_RO);
    js_defn(m, "E", jv_num(2.718281828459045), PF_NOENUM | PF_RO);
    js_defn(m, "LN2", jv_num(0.6931471805599453), PF_NOENUM | PF_RO);
    js_defn(m, "LN10", jv_num(2.302585092994046), PF_NOENUM | PF_RO);
    js_defn(m, "LOG2E", jv_num(1.4426950408889634), PF_NOENUM | PF_RO);
    js_defn(m, "LOG10E", jv_num(0.4342944819032518), PF_NOENUM | PF_RO);
    js_defn(m, "SQRT2", jv_num(1.4142135623730951), PF_NOENUM | PF_RO);
    js_defn(m, "SQRT1_2", jv_num(0.7071067811865476), PF_NOENUM | PF_RO);
    js_defprop(m, A[A_sym_toStringTag], jv_str(js_intern("Math", -1)), PF_NOENUM | PF_RO);

    /* Error */
    for (int i = 0; i < E_COUNT; i++) {
        jsobj *ep = js_newobj(i == 0 ? op : r->p_err[0]);
        r->p_err[i] = ep;
        jsobj *ec = js_newctor(err_ctor, err_names[i], 1, ep);
        ec->u.nat.magic = i;
        if (i) ec->proto = r->c_err[0];
        r->c_err[i] = ec;
        js_defn(ep, "name", jv_str(js_intern(err_names[i], -1)), PF_NOENUM);
        js_defn(ep, "message", jv_str(A[A_empty]), PF_NOENUM);
        js_defn(g, err_names[i], jv_obj(ec), PF_NOENUM);
    }
    r->p_error = r->p_err[0];
    js_defnative(r->p_err[0], "toString", errp_tostring, 0, 0);

    /* Map / Set */
    c = js_newctor(map_ctor, "Map", 0, r->p_map);
    c->u.nat.magic = 0;
    js_defn(g, "Map", jv_obj(c), PF_NOENUM);
    js_defn(g, "WeakMap", jv_obj(c), PF_NOENUM);
    jsobj *mp = r->p_map;
    js_defnative(mp, "get", mapp_op, 1, 0);
    js_defnative(mp, "set", mapp_op, 2, 1);
    js_defnative(mp, "has", mapp_op, 1, 2);
    js_defnative(mp, "delete", mapp_op, 1, 3);
    js_defnative(mp, "clear", mapp_op, 0, 4);
    js_defgetter(mp, "size", mapp_op, 5);
    js_defnative(mp, "forEach", mapp_op, 1, 6);
    jsobj *ment = js_defnative(mp, "entries", mapp_op, 0, 7);
    js_defnative(mp, "keys", mapp_op, 0, 8);
    js_defnative(mp, "values", mapp_op, 0, 9);
    js_defprop(mp, A[A_sym_iterator], jv_obj(ment), PF_NOENUM);
    c = js_newctor(map_ctor, "Set", 0, r->p_set);
    c->u.nat.magic = 1;
    js_defn(g, "Set", jv_obj(c), PF_NOENUM);
    js_defn(g, "WeakSet", jv_obj(c), PF_NOENUM);
    jsobj *setp = r->p_set;
    js_defnative(setp, "add", mapp_op, 1, 16 | 1);
    js_defnative(setp, "has", mapp_op, 1, 16 | 2);
    js_defnative(setp, "delete", mapp_op, 1, 16 | 3);
    js_defnative(setp, "clear", mapp_op, 0, 16 | 4);
    js_defgetter(setp, "size", mapp_op, 16 | 5);
    js_defnative(setp, "forEach", mapp_op, 1, 16 | 6);
    js_defnative(setp, "entries", mapp_op, 0, 16 | 10);
    js_defnative(setp, "keys", mapp_op, 0, 16 | 8);
    jsobj *sval = js_defnative(setp, "values", mapp_op, 0, 16 | 9);
    js_defprop(setp, A[A_sym_iterator], jv_obj(sval), PF_NOENUM);

    /* globais */
    js_defn(g, "NaN", JS_NAN_BITS, PF_NOENUM | PF_RO | PF_NOCONF);
    js_defn(g, "Infinity", jv_num(JS_INF), PF_NOENUM | PF_RO | PF_NOCONF);
    js_defn(g, "undefined", JS_UNDEF, PF_NOENUM | PF_RO | PF_NOCONF);
    js_defn(g, "globalThis", jv_obj(g), PF_NOENUM);
    jsobj *pi = js_defnative(g, "parseInt", g_parseint, 2, 0);
    jsobj *pf = js_defnative(g, "parseFloat", g_parsefloat, 1, 0);
    js_defn(r->c_number, "parseInt", jv_obj(pi), PF_NOENUM);
    js_defn(r->c_number, "parseFloat", jv_obj(pf), PF_NOENUM);
    js_defnative(g, "isNaN", g_isnan, 1, 0);
    js_defnative(g, "isFinite", g_isnan, 1, 1);
    js_defnative(g, "encodeURIComponent", g_encode, 1, 0);
    js_defnative(g, "encodeURI", g_encode, 1, 1);
    js_defnative(g, "decodeURIComponent", g_decode, 1, 0);
    js_defnative(g, "decodeURI", g_decode, 1, 1);
    js_defnative(g, "eval", g_eval, 1, 0);

    js_lib2_init(r);
    js_bigint_init(r);
}
