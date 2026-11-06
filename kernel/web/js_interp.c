/* mOS — motor JavaScript "Brisa": interpretador que percorre a árvore sintática
 * eval() avalia expressões (devolve JS_EXC em exceção, JS_SHORT em curto-circuito de ?.);
 * exec() executa instruções (devolve CMP_* ou -1 em exceção). */
#include "js_int.h"

#define NOINL __attribute__((noinline))

static jsscript *cur_script;
static jsstr *pending_label;

static jsv eval(jsnode *n, jsenv *env);
static int exec(jsnode *n, jsenv *env);
static int exec_list(jsnode **list, int n, jsenv *env);
static void init_fields(jsobj *F, jsobj *obj);
static int field_init_err;

void js_ictx_save(jsictx *c) {
    c->script = cur_script; c->plabel = pending_label; c->label = J.label; c->srcname = J.srcname;
    c->calldepth = J.calldepth; c->line = J.line;
}

void js_ictx_restore(jsictx *c) {
    cur_script = c->script; pending_label = c->plabel; J.label = c->label; J.srcname = c->srcname;
    J.calldepth = c->calldepth; J.line = c->line;
}

/* ============================================================ ambientes */
jsenv *js_newenv(jsenv *parent, int cap) {
    jsenv *e = gc_alloc(GT_ENV, sizeof(jsenv));
    e->parent = parent;
    e->thisv = JS_UNDEF;
    e->newtarget = JS_UNDEF;
    if (cap > 0) {
        e->names = kmalloc(sizeof(jsstr *) * (usize)cap);
        e->vals = kmalloc(sizeof(jsv) * (usize)cap);
        e->vflags = kmalloc((usize)cap);
        if (!e->names || !e->vals || !e->vflags) panic("JS: memória esgotada (ambiente)");
        e->cap = (u32)cap;
    }
    return e;
}

static int env_find(jsenv *e, jsstr *name) {
    for (u32 i = 0; i < e->n; i++) {
        jsstr *k = e->names[i];
        if (k == name || (k->hash == name->hash && js_str_eq(k, name))) return (int)i;
    }
    return -1;
}

void js_env_decl(jsenv *e, jsstr *name, jsv v, u8 flags) {
    int i = env_find(e, name);
    if (i >= 0) { e->vals[i] = v; e->vflags[i] = flags; return; }
    if (e->n == e->cap) {
        u32 nc = e->cap ? e->cap * 2 : 4;
        jsstr **nn = krealloc(e->names, sizeof(jsstr *) * nc);
        jsv *nv = krealloc(e->vals, sizeof(jsv) * nc);
        u8 *nf = krealloc(e->vflags, nc);
        if (!nn || !nv || !nf) panic("JS: memória esgotada (ambiente)");
        e->names = nn; e->vals = nv; e->vflags = nf;
        e->cap = nc;
    }
    e->names[e->n] = name;
    e->vals[e->n] = v;
    e->vflags[e->n] = flags;
    e->n++;
}

static jsv get_var(jsenv *env, jsstr *name) {
    for (jsenv *e = env; e; e = e->parent) {
        int i = env_find(e, name);
        if (i >= 0) {
            jsv v = e->vals[i];
            if (v == JS_HOLE) return js_throw_err(E_REF, "Não é possível acessar '%s' antes da inicialização", name->d);
            if (e->vflags[i] & VF_REF) { jsobj *r = jv_o(v); return js_getk(r->arr[0], jv_s(r->arr[1])); }
            return v;
        }
    }
    jsobj *g = J.realm->global;
    jsobj *owner;
    jsprop *p = js_findprop(g, name, &owner);
    if (!p) return js_throw_err(E_REF, "%s não está definido", name->d);
    if (p->flags & PF_ACCESSOR) return js_iscallable(p->v) ? js_call(p->v, jv_obj(g), 0, NULL) : JS_UNDEF;
    return p->v;
}

jsv js_env_get(jsenv *e, jsstr *name) {
    int i = env_find(e, name);
    if (i < 0) return js_throw_err(E_REF, "%s não está definido", name->d);
    jsv v = e->vals[i];
    if (v == JS_HOLE) return js_throw_err(E_REF, "Não é possível acessar '%s' antes da inicialização", name->d);
    if (e->vflags[i] & VF_REF) { jsobj *r = jv_o(v); return js_getk(r->arr[0], jv_s(r->arr[1])); }
    return v;
}

jsscript *js_cur_script(void) { return cur_script; }

static bool has_var(jsenv *env, jsstr *name) {
    for (jsenv *e = env; e; e = e->parent) if (env_find(e, name) >= 0) return true;
    return js_findprop(J.realm->global, name, NULL) != NULL;
}

static int set_var(jsenv *env, jsstr *name, jsv v) {
    for (jsenv *e = env; e; e = e->parent) {
        int i = env_find(e, name);
        if (i >= 0) {
            if (e->vflags[i] & VF_REF) { js_throw_err(E_TYPE, "Não é possível atribuir à importação '%s'", name->d); return -1; }
            if (e->vals[i] == JS_HOLE) { js_throw_err(E_REF, "Não é possível acessar '%s' antes da inicialização", name->d); return -1; }
            if (e->vflags[i] & VF_CONST) { js_throw_err(E_TYPE, "Atribuição a variável constante '%s'", name->d); return -1; }
            e->vals[i] = v;
            return 0;
        }
    }
    return js_setk(jv_obj(J.realm->global), name, v);
}

/* inicializa um binding léxico (let/const/class) já declarado em env */
static void init_lex(jsenv *env, jsstr *name, jsv v) {
    for (jsenv *e = env; e; e = e->parent) {
        int i = env_find(e, name);
        if (i >= 0) { e->vals[i] = v; return; }
    }
    js_env_decl(env, name, v, VF_LEX);
}

static jsenv *this_env(jsenv *env) {
    for (jsenv *e = env; e; e = e->parent) if (e->has_this) return e;
    return NULL;
}

static jsv get_this(jsenv *env) {
    jsenv *e = this_env(env);
    if (!e) return jv_obj(J.realm->global);
    if (e->thisv == JS_HOLE) return js_throw_err(E_REF, "É preciso chamar super() antes de acessar 'this' no construtor derivado");
    return e->thisv;
}

/* ============================================================ funções */
jsobj *js_make_closure(jsfunc *fi, jsenv *env, jsscript *sc) {
    jsobj *f = js_newobj_cls(CL_FUNCTION, J.realm->p_function);
    f->u.f.fi = fi;
    f->u.f.env = env;
    f->u.f.script = sc;
    f->is_arrow = fi->is_arrow;
    f->is_method = fi->is_method;
    js_defprop(f, A[A_length], jv_int(fi->length), PF_NOENUM | PF_RO);
    js_defprop(f, A[A_name], jv_str(fi->name ? fi->name : A[A_empty]), PF_NOENUM | PF_RO);
    if (fi->is_gen) {
        js_defprop(f, A[A_prototype], jv_obj(js_newobj(fi->is_async ? J.realm->p_asyncgen : J.realm->p_gen)), PF_NOENUM);
    } else if (!fi->is_arrow && !fi->is_method && !fi->is_async) {
        jsobj *proto = js_newobj(J.realm->p_object);
        js_defprop(proto, A[A_constructor], jv_obj(f), PF_NOENUM);
        js_defprop(f, A[A_prototype], jv_obj(proto), PF_NOENUM);
    }
    return f;
}

static void declare_block(jsblock *b, jsenv *e) {
    for (int i = 0; i < b->n; i++) js_env_decl(e, b->names[i], JS_HOLE, (u8)(VF_LEX | (b->isconst[i] ? VF_CONST : 0)));
    for (int i = 0; i < b->nfuncs; i++) {
        jsnode *fd = b->funcs[i];
        jsobj *f = js_make_closure(fd->a->x, e, cur_script);
        js_env_decl(e, fd->str, jv_obj(f), VF_LEX);
    }
}

enum { BIND_ASSIGN, BIND_VAR, BIND_INIT, BIND_DECL };
static int bind_pattern(jsnode *t, jsv v, jsenv *env, int mode);

static jsobj *make_arguments(int argc, jsv *argv, jsobj *callee) {
    jsobj *a = js_newobj_cls(CL_ARGUMENTS, J.realm->p_object);
    if (argc > 0) {
        js_arr_reserve(a, (u32)argc);
        memcpy(a->arr, argv, sizeof(jsv) * (usize)argc);
        a->alen = (u32)argc;
    }
    js_defprop(a, A[A_callee], jv_obj(callee), PF_NOENUM);
    return a;
}

static jsv construct_this(jsv newtarget) {
    jsv pr = js_getk(newtarget, A[A_prototype]);
    if (pr == JS_EXC) return JS_EXC;
    return jv_obj(js_newobj(jv_isobj(pr) ? jv_o(pr) : J.realm->p_object));
}

jsv js_call_function(jsobj *f, jsv thisv, int argc, jsv *argv, jsv newtarget) {
    jsfunc *fi = f->u.f.fi;
    if (fi && (fi->is_gen || fi->is_async)) {
        if (!fi->is_arrow && jv_isnullish(thisv) && !fi->is_method) thisv = jv_obj(J.realm->global);
        if (fi->is_gen && fi->is_async) return js_asyncgen_create(f, thisv, argc, argv);
        return fi->is_gen ? js_gen_create(f, thisv, argc, argv) : js_async_call(f, thisv, argc, argv);
    }
    return js_call_body(f, thisv, argc, argv, newtarget);
}

jsv js_call_body(jsobj *f, jsv thisv, int argc, jsv *argv, jsv newtarget) {
    jsfunc *fi = f->u.f.fi;
    if (!js_check_stack()) return JS_EXC;
    if (!js_tick_budget()) return JS_EXC;
    jsscript *saved_script = cur_script;
    int saved_line = J.line;
    cur_script = f->u.f.script;
    J.calldepth++;
    jsv result = JS_UNDEF;
    jsenv *env = js_newenv(f->u.f.env, fi ? fi->nparams + fi->nvars + 2 : 2);
    env->is_func = 1;
    env->fnobj = f;
    if (!fi || !fi->is_arrow) {
        env->has_this = 1;
        env->newtarget = newtarget;
        if (newtarget != JS_UNDEF) {
            if (f->is_derived) env->thisv = JS_HOLE;
            else {
                jsv t = construct_this(newtarget);
                if (t == JS_EXC) { result = JS_EXC; goto done; }
                env->thisv = t;
                if (f->is_class) {
                    init_fields(f, jv_o(t));
                    if (field_init_err) { field_init_err = 0; result = JS_EXC; goto done; }
                }
            }
        } else {
            if (jv_isnullish(thisv) && !f->is_class && !(fi && fi->is_method)) thisv = jv_obj(J.realm->global);
            env->thisv = thisv;
        }
    }
    if (!fi) {
        /* construtor padrão de classe */
        if (f->is_derived) {
            jsobj *parent = f->proto;
            jsv r = js_construct(jv_obj(parent), argc, argv, newtarget);
            if (r == JS_EXC) { result = JS_EXC; goto done; }
            env->thisv = r;
            init_fields(f, jv_o(r));
            if (field_init_err) { field_init_err = 0; result = JS_EXC; goto done; }
        }
        result = JS_UNDEF;
        goto done;
    }
    /* parâmetros */
    if (fi->simple_params) {
        for (int i = 0; i < fi->nparams; i++) js_env_decl(env, fi->params[i]->str, i < argc ? argv[i] : JS_UNDEF, 0);
    } else {
        for (int i = 0; i < fi->nparams; i++) {
            jsnode *p = fi->params[i];
            if (p->type == N_SPREAD) {
                jsobj *rest = js_arr_from(argv + MIN(i, argc), argc > i ? argc - i : 0);
                if (bind_pattern(p->a, jv_obj(rest), env, BIND_DECL) < 0) { result = JS_EXC; goto done; }
                break;
            }
            if (bind_pattern(p, i < argc ? argv[i] : JS_UNDEF, env, BIND_DECL) < 0) { result = JS_EXC; goto done; }
        }
    }
    if (fi->uses_args && env_find(env, A[A_arguments]) < 0) js_env_decl(env, A[A_arguments], jv_obj(make_arguments(argc, argv, f)), 0);
    for (int i = 0; i < fi->nvars; i++) if (env_find(env, fi->vars[i]) < 0) js_env_decl(env, fi->vars[i], JS_UNDEF, 0);
    for (int i = 0; i < fi->nfuncs; i++) {
        jsnode *fd = fi->funcs[i];
        js_env_decl(env, fd->str, jv_obj(js_make_closure(fd->a->x, env, cur_script)), 0);
    }
    if (fi->expr_body) {
        result = eval(fi->body, env);
        if (result == JS_SHORT) result = JS_UNDEF;
    } else {
        jsnode *body = fi->body;
        if (body->x) declare_block(body->x, env);
        int r = exec_list(body->list, body->n, env);
        if (r < 0) result = JS_EXC;
        else if (r == CMP_RETURN) { result = J.retval; J.retval = JS_UNDEF; }
        else result = JS_UNDEF;
    }
done:
    J.calldepth--;
    cur_script = saved_script;
    if (result != JS_EXC) J.line = saved_line;
    if (newtarget != JS_UNDEF && result != JS_EXC) {
        if (jv_isobj(result)) return result;
        if (env->thisv == JS_HOLE) return js_throw_err(E_REF, "O construtor derivado deve chamar super() antes de terminar");
        return env->thisv;
    }
    return result;
}

/* ============================================================ desestruturação */
static jsv eval_key(jsnode *p, jsenv *env, bool computed) {
    if (!computed) return jv_str(p->str);
    jsv k = eval(p, env);
    if (k == JS_EXC) return JS_EXC;
    if (jv_isstr(k) || jv_isnum(k)) return k;
    jsstr *s = js_tokey(k);
    return s ? jv_str(s) : JS_EXC;
}

static int bind_name(jsstr *name, jsv v, jsenv *env, int mode) {
    switch (mode) {
    case BIND_DECL: js_env_decl(env, name, v, 0); return 0;
    case BIND_INIT: init_lex(env, name, v); return 0;
    default: return set_var(env, name, v);
    }
}

static int assign_member(jsnode *t, jsv v, jsenv *env);

static int bind_pattern(jsnode *t, jsv v, jsenv *env, int mode) {
    switch (t->type) {
    case N_IDENT: return bind_name(t->str, v, env, mode);
    case N_MEMBER: return assign_member(t, v, env);
    case N_ASSIGN:
        if (v == JS_UNDEF) {
            v = eval(t->b, env);
            if (v == JS_EXC) return -1;
        }
        return bind_pattern(t->a, v, env, mode);
    case N_ARRAY: {
        if (jv_isnullish(v)) { js_throw_err(E_TYPE, "%s não é iterável", v == JS_NULL ? "null" : "undefined"); return -1; }
        if (jv_iscls(v, CL_ARRAY) || jv_iscls(v, CL_ARGUMENTS)) {
            jsobj *a = jv_o(v);
            u32 k = 0;
            for (int i = 0; i < t->n; i++) {
                jsnode *e = t->list[i];
                if (e->type == N_ELISION) { k++; continue; }
                if (e->type == N_SPREAD) {
                    jsobj *rest = js_newarr(0);
                    for (; k < a->alen; k++) js_arr_push(rest, js_arr_get(a, k));
                    if (bind_pattern(e->a, jv_obj(rest), env, mode) < 0) return -1;
                    break;
                }
                jsv x = k < a->alen ? js_arr_get(a, k) : JS_UNDEF;
                k++;
                if (bind_pattern(e, x, env, mode) < 0) return -1;
            }
            return 0;
        }
        jsv it = js_iter_open(v);
        if (it == JS_EXC) return -1;
        bool done = false;
        for (int i = 0; i < t->n; i++) {
            jsnode *e = t->list[i];
            jsv x = JS_UNDEF;
            if (e->type == N_SPREAD) {
                jsobj *rest = js_newarr(0);
                while (!done) {
                    int r = js_iter_step(it, &x);
                    if (r < 0) return -1;
                    if (r == 0) { done = true; break; }
                    js_arr_push(rest, x);
                }
                if (bind_pattern(e->a, jv_obj(rest), env, mode) < 0) return -1;
                break;
            }
            if (!done) {
                int r = js_iter_step(it, &x);
                if (r < 0) return -1;
                if (r == 0) { done = true; x = JS_UNDEF; }
            }
            if (e->type == N_ELISION) continue;
            if (bind_pattern(e, x, env, mode) < 0) return -1;
        }
        return 0;
    }
    case N_OBJECT: {
        if (jv_isnullish(v)) { js_throw_err(E_TYPE, "Não é possível desestruturar '%s'", v == JS_NULL ? "null" : "undefined"); return -1; }
        jsobj *used = NULL;
        for (int i = 0; i < t->n; i++) {
            jsnode *p = t->list[i];
            if (p->flags & NF_SPREADPROP) {
                jsv src = js_toobj(v);
                if (src == JS_EXC) return -1;
                jsobj *rest = js_newobj(J.realm->p_object);
                jsobj *keys = js_keylist(jv_o(src), true, false);
                for (u32 k = 0; k < keys->alen; k++) {
                    jsstr *ks = jv_s(keys->arr[k]);
                    bool skip = false;
                    for (u32 u = 0; used && u < used->alen; u++) if (js_str_eq(jv_s(used->arr[u]), ks)) skip = true;
                    if (skip) continue;
                    jsv val = js_getk(src, ks);
                    if (val == JS_EXC) return -1;
                    js_defprop(rest, ks, val, 0);
                }
                if (bind_pattern(p->b, jv_obj(rest), env, mode) < 0) return -1;
                continue;
            }
            jsv key = eval_key(p->a, env, (p->flags & NF_COMPUTED) != 0);
            if (key == JS_EXC) return -1;
            if (!used) used = js_newarr(0);
            jsstr *ks = js_tokey(key);
            if (!ks) return -1;
            js_arr_push(used, jv_str(ks));
            jsv val = js_getk(v, ks);
            if (val == JS_EXC) return -1;
            if (bind_pattern(p->b, val, env, mode) < 0) return -1;
        }
        return 0;
    }
    }
    js_throw_err(E_SYNTAX, "Alvo de atribuição inválido");
    return -1;
}

/* ============================================================ referências */
static jsv super_base(jsenv *env) {
    jsenv *e = this_env(env);
    jsobj *f = e ? e->fnobj : NULL;
    jsobj *home = f && f->cls == CL_FUNCTION ? f->u.f.home : NULL;
    if (!home) return js_throw_err(E_SYNTAX, "'super' fora de um método");
    return home->proto ? jv_obj(home->proto) : JS_NULL;
}

/* avalia objeto e chave de um N_MEMBER */
static int member_ref(jsnode *n, jsenv *env, jsv *obj, jsv *key) {
    if (n->a->type == N_SUPER) *obj = super_base(env);
    else *obj = eval(n->a, env);
    if (*obj == JS_EXC) return -1;
    if (*obj == JS_SHORT) return 1;
    if ((n->flags & NF_OPTIONAL) && jv_isnullish(*obj)) return 1;
    if (n->flags & NF_COMPUTED) {
        *key = eval(n->b, env);
        if (*key == JS_EXC) return -1;
    } else *key = jv_str(n->b->str);
    return 0;
}

static jsv get_member(jsnode *n, jsv obj, jsv key, jsenv *env) {
    if (n->a->type == N_SUPER) {
        jsv t = get_this(env);
        if (t == JS_EXC) return JS_EXC;
        if (!jv_isobj(obj)) return JS_UNDEF;
        jsstr *k = js_tokey(key);
        if (!k) return JS_EXC;
        return js_get_with_this(obj, k, t);
    }
    if (jv_isnullish(obj)) {
        char kb[80];
        const char *what = n->a->type == N_IDENT ? n->a->str->d : NULL;
        if (what) return js_throw_err(E_TYPE, "Não é possível ler propriedades de %s (lendo '%s' de '%s')", obj == JS_NULL ? "null" : "undefined",
                                      js_cstr(key, kb, sizeof kb), what);
    }
    return js_get(obj, key);
}

static int assign_member(jsnode *t, jsv v, jsenv *env) {
    jsv obj, key;
    int r = member_ref(t, env, &obj, &key);
    if (r < 0) return -1;
    if (r > 0) { js_throw_err(E_SYNTAX, "Atribuição inválida em encadeamento opcional"); return -1; }
    if (t->a->type == N_SUPER) obj = get_this(env);
    return js_set(obj, key, v);
}

static void describe(jsnode *n, char *buf, int cap) {
    if (n->type == N_IDENT) { snprintf(buf, (usize)cap, "%s", n->str->d); return; }
    if (n->type == N_MEMBER && !(n->flags & NF_COMPUTED)) {
        char inner[80];
        describe(n->a, inner, sizeof inner);
        snprintf(buf, (usize)cap, "%s.%s", inner, n->b->str->d);
        return;
    }
    if (n->type == N_THIS) { snprintf(buf, (usize)cap, "this"); return; }
    if (n->type == N_MEMBER) {
        char inner[80];
        describe(n->a, inner, sizeof inner);
        snprintf(buf, (usize)cap, "%s[...]", inner);
        return;
    }
    if (n->type == N_CALL) {
        char inner[80];
        describe(n->a, inner, sizeof inner);
        snprintf(buf, (usize)cap, "%s(...)", inner);
        return;
    }
    snprintf(buf, (usize)cap, "expressão");
}

/* ============================================================ operadores */
static jsv add_values(jsv a, jsv b) {
    if (jv_isnum(a) && jv_isnum(b)) return jv_num(jv_n(a) + jv_n(b));
    if (jv_isstr(a) && jv_isstr(b) && !jv_s(a)->sym && !jv_s(b)->sym) { jsstr *s = js_str_cat(jv_s(a), jv_s(b)); return s ? jv_str(s) : JS_EXC; }
    jsv pa = js_toprim(a, 0);
    if (pa == JS_EXC) return JS_EXC;
    jsv pb = js_toprim(b, 0);
    if (pb == JS_EXC) return JS_EXC;
    if (!jv_isstr(pa) && !jv_isstr(pb) && (jv_isbig(pa) || jv_isbig(pb))) {
        if (jv_isbig(pa) && jv_isbig(pb)) return js_big_binop(OP_ADD, jv_b(pa), jv_b(pb));
        return js_throw_err(E_TYPE, "Não é possível misturar BigInt e outros tipos; use conversões explícitas");
    }
    if (jv_isstr(pa) || jv_isstr(pb)) {
        jsstr *sa = js_tostr(pa);
        if (!sa) return JS_EXC;
        jsstr *sb = js_tostr(pb);
        if (!sb) return JS_EXC;
        jsstr *s = js_str_cat(sa, sb);
        return s ? jv_str(s) : JS_EXC;
    }
    double x = js_tonum(pa);
    if (js_pending()) return JS_EXC;
    double y = js_tonum(pb);
    if (js_pending()) return JS_EXC;
    return jv_num(x + y);
}

static jsv compare(jsv a, jsv b, int op) {
    jsv pa = js_toprim(a, 1);
    if (pa == JS_EXC) return JS_EXC;
    jsv pb = js_toprim(b, 1);
    if (pb == JS_EXC) return JS_EXC;
    if (jv_isstr(pa) && jv_isstr(pb)) {
        int c = js_str_cmp(jv_s(pa), jv_s(pb));
        switch (op) { case OP_LT: return jv_bool(c < 0); case OP_GT: return jv_bool(c > 0); case OP_LE: return jv_bool(c <= 0); default: return jv_bool(c >= 0); }
    }
    if (jv_isbig(pa) || jv_isbig(pb)) {
        /* BigInt x BigInt / Number / String */
        int c;
        if (jv_isstr(pa)) { pa = js_big_from_str_value(jv_s(pa)); if (pa == JS_EXC) return JS_EXC; if (pa == JS_UNDEF) return JS_FALSE; }
        if (jv_isstr(pb)) { pb = js_big_from_str_value(jv_s(pb)); if (pb == JS_EXC) return JS_EXC; if (pb == JS_UNDEF) return JS_FALSE; }
        if (jv_isbig(pa) && jv_isbig(pb)) c = js_big_cmp(jv_b(pa), jv_b(pb));
        else if (jv_isbig(pa)) {
            double y = js_tonum(pb);
            if (js_pending()) return JS_EXC;
            c = js_big_cmp_num(jv_b(pa), y);
        } else {
            double x = js_tonum(pa);
            if (js_pending()) return JS_EXC;
            c = js_big_cmp_num(jv_b(pb), x);
            if (c != 2) c = -c;
        }
        if (c == 2) return JS_FALSE;
        switch (op) { case OP_LT: return jv_bool(c < 0); case OP_GT: return jv_bool(c > 0); case OP_LE: return jv_bool(c <= 0); default: return jv_bool(c >= 0); }
    }
    double x = js_tonum(pa);
    if (js_pending()) return JS_EXC;
    double y = js_tonum(pb);
    if (js_pending()) return JS_EXC;
    switch (op) { case OP_LT: return jv_bool(x < y); case OP_GT: return jv_bool(x > y); case OP_LE: return jv_bool(x <= y); default: return jv_bool(x >= y); }
}

static jsv arith(int op, double x, double y) {
    switch (op) {
    case OP_SUB: return jv_num(x - y);
    case OP_MUL: return jv_num(x * y);
    case OP_DIV: return jv_num(x / y);
    case OP_MOD: return jv_num(m_fmod(x, y));
    case OP_EXP: return jv_num(m_pow(x, y));
    }
    i32 a = js_toint32(jv_num(x)), b = js_toint32(jv_num(y));
    switch (op) {
    case OP_SHL: return jv_num((double)(i32)((u32)a << (b & 31)));
    case OP_SHR: return jv_num((double)(a >> (b & 31)));
    case OP_USHR: return jv_num((double)((u32)a >> (b & 31)));
    case OP_BAND: return jv_num((double)(a & b));
    case OP_BOR: return jv_num((double)(a | b));
    case OP_BXOR: return jv_num((double)(a ^ b));
    }
    return JS_UNDEF;
}

static jsv binop(int op, jsv a, jsv b) {
    if (jv_isnum(a) && jv_isnum(b)) {
        double x = jv_n(a), y = jv_n(b);
        switch (op) {
        case OP_ADD: return jv_num(x + y);
        case OP_LT: return jv_bool(x < y);
        case OP_GT: return jv_bool(x > y);
        case OP_LE: return jv_bool(x <= y);
        case OP_GE: return jv_bool(x >= y);
        case OP_EQ: case OP_SEQ: return jv_bool(x == y);
        case OP_NE: case OP_SNE: return jv_bool(x != y);
        case OP_INSTANCEOF: case OP_IN: break;
        default: return arith(op, x, y);
        }
    }
    switch (op) {
    case OP_ADD: return add_values(a, b);
    case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD: case OP_EXP: case OP_SHL: case OP_SHR: case OP_USHR:
    case OP_BAND: case OP_BOR: case OP_BXOR: {
        if (jv_isbig(a) || jv_isbig(b) || jv_isobj(a) || jv_isobj(b)) {
            a = js_toprim(a, 1);
            if (a == JS_EXC) return JS_EXC;
            b = js_toprim(b, 1);
            if (b == JS_EXC) return JS_EXC;
            if (jv_isbig(a) && jv_isbig(b)) return js_big_binop(op, jv_b(a), jv_b(b));
            if (jv_isbig(a) || jv_isbig(b)) return js_throw_err(E_TYPE, "Não é possível misturar BigInt e outros tipos; use conversões explícitas");
        }
        double x = js_tonum(a);
        if (js_pending()) return JS_EXC;
        double y = js_tonum(b);
        if (js_pending()) return JS_EXC;
        return arith(op, x, y);
    }
    case OP_EQ: case OP_NE: {
        int r = js_loose_eq(a, b);
        if (r < 0) return JS_EXC;
        return jv_bool(op == OP_EQ ? r : !r);
    }
    case OP_SEQ: return jv_bool(js_strict_eq(a, b));
    case OP_SNE: return jv_bool(!js_strict_eq(a, b));
    case OP_LT: case OP_GT: case OP_LE: case OP_GE: return compare(a, b, op);
    case OP_INSTANCEOF: {
        int r = js_instanceof(a, b);
        return r < 0 ? JS_EXC : jv_bool(r);
    }
    case OP_IN: {
        int r = js_has(b, a);
        return r < 0 ? JS_EXC : jv_bool(r);
    }
    }
    return JS_UNDEF;
}

/* ============================================================ expressões */
static NOINL jsv eval_args(jsnode *n, jsenv *env, jsv *buf, int cap, int *argc, jsobj **heap) {
    bool spread = false;
    for (int i = 0; i < n->n; i++) if (n->list[i]->type == N_SPREAD) { spread = true; break; }
    *heap = NULL;
    if (!spread && n->n <= cap) {
        for (int i = 0; i < n->n; i++) {
            jsv v = eval(n->list[i], env);
            if (v == JS_EXC) return JS_EXC;
            if (v == JS_SHORT) v = JS_UNDEF;
            buf[i] = v;
        }
        *argc = n->n;
        return JS_UNDEF;
    }
    jsobj *a = js_newarr(0);
    *heap = a;
    for (int i = 0; i < n->n; i++) {
        jsnode *e = n->list[i];
        if (e->type == N_SPREAD) {
            jsv s = eval(e->a, env);
            if (s == JS_EXC) return JS_EXC;
            if (jv_iscls(s, CL_ARRAY) || jv_iscls(s, CL_ARGUMENTS)) {
                jsobj *sa = jv_o(s);
                for (u32 k = 0; k < sa->alen; k++) js_arr_push(a, js_arr_get(sa, k));
            } else {
                jsv it = js_iter_open(s);
                if (it == JS_EXC) return JS_EXC;
                for (;;) {
                    jsv x;
                    int r = js_iter_step(it, &x);
                    if (r < 0) return JS_EXC;
                    if (!r) break;
                    js_arr_push(a, x);
                }
            }
        } else {
            jsv v = eval(e, env);
            if (v == JS_EXC) return JS_EXC;
            js_arr_push(a, v);
        }
        if (a->alen > 65536) return js_throw_err(E_RANGE, "Argumentos demais");
    }
    *argc = (int)a->alen;
    return JS_UNDEF;
}

static NOINL jsv eval_call(jsnode *n, jsenv *env) {
    jsnode *c = n->a;
    jsv f, thisv = JS_UNDEF;
    jsv buf[16];
    int argc = 0;
    jsobj *volatile heap = NULL;
    jsobj *hp;
    (void)heap;
    if (c->type == N_SUPER) {
        jsenv *te = this_env(env);
        jsobj *F = te ? te->fnobj : NULL;
        if (!F || !F->is_derived) return js_throw_err(E_SYNTAX, "'super()' só é válido em construtores de classes derivadas");
        if (eval_args(n, env, buf, 16, &argc, &hp) == JS_EXC) return JS_EXC;
        heap = hp;
        jsv parent = F->proto ? jv_obj(F->proto) : JS_NULL;
        jsv r = js_construct(parent, argc, hp ? hp->arr : buf, te->newtarget);
        heap = NULL;
        if (r == JS_EXC) return JS_EXC;
        if (te->thisv != JS_HOLE) return js_throw_err(E_REF, "super() chamado mais de uma vez");
        te->thisv = r;
        init_fields(F, jv_o(r));
        if (field_init_err) { field_init_err = 0; return JS_EXC; }
        return JS_UNDEF;
    }
    if (c->type == N_MEMBER) {
        jsv obj, key;
        int r = member_ref(c, env, &obj, &key);
        if (r < 0) return JS_EXC;
        if (r > 0) return JS_SHORT;
        J.line = n->line;
        f = get_member(c, obj, key, env);
        if (f == JS_EXC) return JS_EXC;
        thisv = c->a->type == N_SUPER ? get_this(env) : obj;
        if (thisv == JS_EXC) return JS_EXC;
    } else {
        f = eval(c, env);
        if (f == JS_EXC || f == JS_SHORT) return f;
    }
    if ((n->flags & NF_OPTIONAL) && jv_isnullish(f)) return JS_SHORT;
    if (eval_args(n, env, buf, 16, &argc, &hp) == JS_EXC) return JS_EXC;
    heap = hp;
    J.line = n->line;
    if (!js_iscallable(f)) {
        char d[120];
        describe(c, d, sizeof d);
        return js_throw_err(E_TYPE, "%s não é uma função", d);
    }
    jsv r = js_call(f, thisv, argc, hp ? hp->arr : buf);
    heap = NULL;
    return r;
}

static NOINL jsv eval_new(jsnode *n, jsenv *env) {
    jsv f = eval(n->a, env);
    if (f == JS_EXC) return JS_EXC;
    jsv buf[16];
    int argc = 0;
    jsobj *hp;
    jsobj *volatile heap;
    if (eval_args(n, env, buf, 16, &argc, &hp) == JS_EXC) return JS_EXC;
    heap = hp;
    J.line = n->line;
    if (!js_isctor(f)) {
        char d[120];
        describe(n->a, d, sizeof d);
        return js_throw_err(E_TYPE, "%s não é um construtor", d);
    }
    jsv r = js_construct(f, argc, hp ? hp->arr : buf, f);
    heap = NULL;
    (void)heap;
    return r;
}

static jsobj *closure_expr(jsnode *n, jsenv *env) {
    jsfunc *fi = n->x;
    if (fi->name && !fi->is_arrow && !fi->is_method) {
        jsenv *fe = js_newenv(env, 1);
        jsobj *f = js_make_closure(fi, fe, cur_script);
        js_env_decl(fe, fi->name, jv_obj(f), VF_CONST);
        return f;
    }
    return js_make_closure(fi, env, cur_script);
}

static NOINL jsv eval_object(jsnode *n, jsenv *env) {
    jsobj *o = js_newobj(J.realm->p_object);
    for (int i = 0; i < n->n; i++) {
        jsnode *p = n->list[i];
        if (p->flags & NF_SPREADPROP) {
            jsv src = eval(p->b, env);
            if (src == JS_EXC) return JS_EXC;
            if (jv_isnullish(src)) continue;
            if (jv_isstr(src)) src = js_toobj(src);
            if (!jv_isobj(src)) continue;
            jsobj *keys = js_keylist(jv_o(src), true, false);
            for (u32 k = 0; k < keys->alen; k++) {
                jsv val = js_getk(src, jv_s(keys->arr[k]));
                if (val == JS_EXC) return JS_EXC;
                js_defprop(o, jv_s(keys->arr[k]), val, 0);
            }
            continue;
        }
        jsv key = eval_key(p->a, env, (p->flags & NF_COMPUTED) != 0);
        if (key == JS_EXC) return JS_EXC;
        jsstr *ks = js_tokey(key);
        if (!ks) return JS_EXC;
        if (p->flags & (NF_GETTER | NF_SETTER)) {
            jsobj *f = js_make_closure(p->b->x, env, cur_script);
            f->u.f.home = o;
            if (p->flags & NF_GETTER) js_defaccessor(o, ks, jv_obj(f), JS_HOLE, 0);
            else js_defaccessor(o, ks, JS_HOLE, jv_obj(f), 0);
            continue;
        }
        jsv v;
        if (p->flags & NF_METHOD) {
            jsobj *f = js_make_closure(p->b->x, env, cur_script);
            f->u.f.home = o;
            v = jv_obj(f);
        } else {
            v = eval(p->b, env);
            if (v == JS_EXC) return JS_EXC;
            if (v == JS_SHORT) v = JS_UNDEF;
            if (!(p->flags & (NF_COMPUTED | NF_SHORTHAND)) && ks->len == 9 && !strcmp(ks->d, "__proto__")) {
                if (jv_isobj(v)) o->proto = jv_o(v);
                else if (v == JS_NULL) o->proto = NULL;
                continue;
            }
        }
        js_defprop(o, ks, v, 0);
    }
    return jv_obj(o);
}

static NOINL jsv eval_array(jsnode *n, jsenv *env) {
    jsobj *a = js_newarr(0);
    js_arr_reserve(a, (u32)n->n);
    for (int i = 0; i < n->n; i++) {
        jsnode *e = n->list[i];
        if (e->type == N_ELISION) { js_arr_push(a, JS_HOLE); continue; }
        if (e->type == N_SPREAD) {
            jsv s = eval(e->a, env);
            if (s == JS_EXC) return JS_EXC;
            if (jv_iscls(s, CL_ARRAY)) {
                jsobj *sa = jv_o(s);
                for (u32 k = 0; k < sa->alen; k++) js_arr_push(a, js_arr_get(sa, k));
                continue;
            }
            jsv it = js_iter_open(s);
            if (it == JS_EXC) return JS_EXC;
            for (;;) {
                jsv x;
                int r = js_iter_step(it, &x);
                if (r < 0) return JS_EXC;
                if (!r) break;
                js_arr_push(a, x);
            }
            continue;
        }
        jsv v = eval(e, env);
        if (v == JS_EXC) return JS_EXC;
        if (v == JS_SHORT) v = JS_UNDEF;
        js_arr_push(a, v);
    }
    return jv_obj(a);
}

static NOINL jsv eval_template(jsnode *n, jsenv *env) {
    sbuf_t sb = {0};
    for (int i = 0; i < n->n; i++) {
        jsnode *e = n->list[i];
        if (i % 2 == 0) { sb_putn(&sb, e->str->d, (int)e->str->len); continue; }
        jsv v = eval(e, env);
        if (v == JS_EXC) { sb_free(&sb); return JS_EXC; }
        if (v == JS_SHORT) v = JS_UNDEF;
        jsstr *s = js_tostr(v);
        if (!s) { sb_free(&sb); return JS_EXC; }
        sb_putn(&sb, s->d, (int)s->len);
    }
    jsstr *r = js_strn(sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
    return jv_str(r);
}

static NOINL jsv eval_tagged(jsnode *n, jsenv *env) {
    jsnode *t = n->b;
    jsv f, thisv = JS_UNDEF;
    if (n->a->type == N_MEMBER) {
        jsv obj, key;
        int r = member_ref(n->a, env, &obj, &key);
        if (r < 0) return JS_EXC;
        f = get_member(n->a, obj, key, env);
        thisv = obj;
    } else f = eval(n->a, env);
    if (f == JS_EXC) return JS_EXC;
    jsobj *strs = js_newarr(0), *raw = js_newarr(0);
    jsobj *args = js_newarr(0);
    js_arr_push(args, jv_obj(strs));
    for (int i = 0; i < t->n; i++) {
        jsnode *e = t->list[i];
        if (i % 2 == 0) { js_arr_push(strs, jv_str(e->str)); js_arr_push(raw, jv_str(e->str)); continue; }
        jsv v = eval(e, env);
        if (v == JS_EXC) return JS_EXC;
        js_arr_push(args, v);
    }
    js_defprop(strs, A[A_raw], jv_obj(raw), PF_NOENUM);
    return js_call(f, thisv, (int)args->alen, args->arr);
}

static NOINL jsv eval_class(jsnode *n, jsenv *env) {
    jsv parent = JS_UNDEF;
    jsobj *proto_parent = J.realm->p_object, *ctor_parent = J.realm->p_function;
    if (n->a) {
        parent = eval(n->a, env);
        if (parent == JS_EXC) return JS_EXC;
        if (parent == JS_NULL) { proto_parent = NULL; }
        else {
            if (!js_isctor(parent)) return js_throw_err(E_TYPE, "A classe estendida não é um construtor nem null");
            jsv pp = js_getk(parent, A[A_prototype]);
            if (pp == JS_EXC) return JS_EXC;
            if (pp == JS_NULL) proto_parent = NULL;
            else if (jv_isobj(pp)) proto_parent = jv_o(pp);
            else return js_throw_err(E_TYPE, "O prototype da classe estendida não é objeto");
            ctor_parent = jv_o(parent);
        }
    }
    jsenv *cenv = js_newenv(env, 1);
    if (n->str) js_env_decl(cenv, n->str, JS_HOLE, VF_CONST | VF_LEX);
    jsobj *proto = js_newobj(proto_parent);
    jsobj *F;
    if (n->c) {
        F = js_make_closure(n->c->x, cenv, cur_script);
        F->is_method = 0;
    } else {
        F = js_newobj_cls(CL_FUNCTION, J.realm->p_function);
        F->u.f.env = cenv;
        F->u.f.script = cur_script;
        js_defprop(F, A[A_length], jv_int(0), PF_NOENUM | PF_RO);
        js_defprop(F, A[A_name], jv_str(n->str ? n->str : A[A_empty]), PF_NOENUM | PF_RO);
    }
    F->is_class = 1;
    F->is_derived = n->a != NULL && parent != JS_NULL;
    F->proto = ctor_parent;
    F->u.f.home = proto;
    F->u.f.cls = n;
    F->u.f.cenv = cenv;
    js_defprop(F, A[A_prototype], jv_obj(proto), PF_NOENUM | PF_RO);
    js_defprop(proto, A[A_constructor], jv_obj(F), PF_NOENUM);
    for (int i = 0; i < n->n; i++) {
        jsnode *m = n->list[i];
        bool st = (m->flags & NF_STATIC) != 0;
        jsobj *target = st ? F : proto;
        if (m->flags & NF_FIELD) {
            if (!st) continue;
            jsv key = eval_key(m->a, cenv, (m->flags & NF_COMPUTED) != 0);
            if (key == JS_EXC) return JS_EXC;
            jsstr *ks = js_tokey(key);
            if (!ks) return JS_EXC;
            jsv v = JS_UNDEF;
            if (m->b) {
                jsenv *fe = js_newenv(cenv, 0);
                fe->has_this = 1;
                fe->is_func = 1;
                fe->thisv = jv_obj(F);
                fe->fnobj = F;
                jsfunc *ff = m->b->x;
                v = eval(ff->body, fe);
                if (v == JS_EXC) return JS_EXC;
            }
            js_defprop(F, ks, v, 0);
            continue;
        }
        jsv key = eval_key(m->a, cenv, (m->flags & NF_COMPUTED) != 0);
        if (key == JS_EXC) return JS_EXC;
        jsstr *ks = js_tokey(key);
        if (!ks) return JS_EXC;
        jsobj *f = js_make_closure(m->b->x, cenv, cur_script);
        f->u.f.home = target;
        if (m->flags & NF_GETTER) js_defaccessor(target, ks, jv_obj(f), JS_HOLE, PF_NOENUM);
        else if (m->flags & NF_SETTER) js_defaccessor(target, ks, JS_HOLE, jv_obj(f), PF_NOENUM);
        else js_defprop(target, ks, jv_obj(f), PF_NOENUM);
    }
    if (n->str) init_lex(cenv, n->str, jv_obj(F));
    return jv_obj(F);
}

static void init_fields(jsobj *F, jsobj *obj) {
    jsnode *c = F->u.f.cls;
    if (!c) return;
    for (int i = 0; i < c->n; i++) {
        jsnode *m = c->list[i];
        if (!(m->flags & NF_FIELD) || (m->flags & NF_STATIC)) continue;
        jsenv *fe = js_newenv(F->u.f.cenv, 0);
        fe->has_this = 1;
        fe->is_func = 1;
        fe->thisv = jv_obj(obj);
        fe->fnobj = F;
        jsv key = eval_key(m->a, fe, (m->flags & NF_COMPUTED) != 0);
        if (key == JS_EXC) { field_init_err = 1; return; }
        jsstr *ks = js_tokey(key);
        if (!ks) { field_init_err = 1; return; }
        jsv v = JS_UNDEF;
        if (m->b) {
            jsfunc *ff = m->b->x;
            v = eval(ff->body, fe);
            if (v == JS_EXC) { field_init_err = 1; return; }
        }
        js_defprop(obj, ks, v, 0);
    }
}

static NOINL jsv eval_assign(jsnode *n, jsenv *env) {
    jsnode *t = n->a;
    int op = n->op;
    if (op == OP_ASSIGN) {
        if (t->type == N_IDENT) {
            jsv v = eval(n->b, env);
            if (v == JS_EXC) return JS_EXC;
            if (v == JS_SHORT) v = JS_UNDEF;
            if (set_var(env, t->str, v) < 0) return JS_EXC;
            return v;
        }
        if (t->type == N_MEMBER) {
            jsv obj, key;
            int r = member_ref(t, env, &obj, &key);
            if (r < 0) return JS_EXC;
            if (r > 0) return js_throw_err(E_SYNTAX, "Atribuição inválida em encadeamento opcional");
            jsv v = eval(n->b, env);
            if (v == JS_EXC) return JS_EXC;
            if (v == JS_SHORT) v = JS_UNDEF;
            if (t->a->type == N_SUPER) obj = get_this(env);
            J.line = n->line;
            if (js_set(obj, key, v) < 0) return JS_EXC;
            return v;
        }
        jsv v = eval(n->b, env);
        if (v == JS_EXC) return JS_EXC;
        if (bind_pattern(t, v, env, BIND_ASSIGN) < 0) return JS_EXC;
        return v;
    }
    /* compostos */
    jsv obj = JS_UNDEF, key = JS_UNDEF, old;
    if (t->type == N_IDENT) old = get_var(env, t->str);
    else {
        int r = member_ref(t, env, &obj, &key);
        if (r < 0) return JS_EXC;
        if (r > 0) return js_throw_err(E_SYNTAX, "Atribuição inválida em encadeamento opcional");
        old = get_member(t, obj, key, env);
    }
    if (old == JS_EXC) return JS_EXC;
    jsv v;
    if (op == OP_AND || op == OP_OR || op == OP_NULLISH) {
        bool assign = op == OP_AND ? js_truthy(old) : op == OP_OR ? !js_truthy(old) : jv_isnullish(old);
        if (!assign) return old;
        v = eval(n->b, env);
        if (v == JS_EXC) return JS_EXC;
    } else {
        jsv rhs = eval(n->b, env);
        if (rhs == JS_EXC) return JS_EXC;
        v = binop(op, old, rhs);
        if (v == JS_EXC) return JS_EXC;
    }
    if (t->type == N_IDENT) { if (set_var(env, t->str, v) < 0) return JS_EXC; }
    else {
        if (t->a->type == N_SUPER) obj = get_this(env);
        if (js_set(obj, key, v) < 0) return JS_EXC;
    }
    return v;
}

static NOINL jsv eval_update(jsnode *n, jsenv *env) {
    jsnode *t = n->a;
    jsv obj = JS_UNDEF, key = JS_UNDEF, old;
    if (t->type == N_IDENT) old = get_var(env, t->str);
    else {
        int r = member_ref(t, env, &obj, &key);
        if (r < 0) return JS_EXC;
        if (r > 0) return js_throw_err(E_SYNTAX, "Operando inválido para ++/--");
        old = get_member(t, obj, key, env);
    }
    if (old == JS_EXC) return JS_EXC;
    if (jv_isobj(old)) { old = js_toprim(old, 1); if (old == JS_EXC) return JS_EXC; }
    if (jv_isbig(old)) {
        jsv bv = js_big_add_small(jv_b(old), n->op == OP_INC ? 1 : -1);
        if (bv == JS_EXC) return JS_EXC;
        if (t->type == N_IDENT) { if (set_var(env, t->str, bv) < 0) return JS_EXC; }
        else if (js_set(obj, key, bv) < 0) return JS_EXC;
        return (n->flags & NF_PREFIX) ? bv : old;
    }
    double x = jv_isnum(old) ? jv_n(old) : js_tonum(old);
    if (js_pending()) return JS_EXC;
    jsv nv = jv_num(n->op == OP_INC ? x + 1 : x - 1);
    if (t->type == N_IDENT) { if (set_var(env, t->str, nv) < 0) return JS_EXC; }
    else if (js_set(obj, key, nv) < 0) return JS_EXC;
    return (n->flags & NF_PREFIX) ? nv : jv_num(x);
}

static NOINL jsv eval_unary(jsnode *n, jsenv *env) {
    switch (n->op) {
    case OP_TYPEOF:
        if (n->a->type == N_IDENT && !n->a->op && !has_var(env, n->a->str)) return jv_str(A[A_undefined]);
        {
            jsv v = eval(n->a, env);
            if (v == JS_EXC) return JS_EXC;
            if (v == JS_SHORT) v = JS_UNDEF;
            return jv_str(js_typeof(v));
        }
    case OP_DELETE: {
        jsnode *t = n->a;
        if (t->type == N_OPTCHAIN) t = t->a;
        if (t->type == N_MEMBER) {
            jsv obj, key;
            int r = member_ref(t, env, &obj, &key);
            if (r < 0) return JS_EXC;
            if (r > 0) return JS_TRUE;
            int d = js_delete(obj, key);
            return d < 0 ? JS_EXC : jv_bool(d);
        }
        if (t->type == N_IDENT) return JS_FALSE;
        jsv v = eval(t, env);
        return v == JS_EXC ? JS_EXC : JS_TRUE;
    }
    }
    jsv v = eval(n->a, env);
    if (v == JS_EXC) return JS_EXC;
    if (v == JS_SHORT) v = JS_UNDEF;
    switch (n->op) {
    case OP_NOT: return jv_bool(!js_truthy(v));
    case OP_NEG: {
        if (jv_isobj(v)) { v = js_toprim(v, 1); if (v == JS_EXC) return JS_EXC; }
        if (jv_isbig(v)) return js_big_neg(jv_b(v));
        double x = jv_isnum(v) ? jv_n(v) : js_tonum(v);
        if (js_pending()) return JS_EXC;
        return jv_num(-x);
    }
    case OP_PLUS: return js_tonumv(v);
    case OP_BNOT: {
        if (jv_isobj(v)) { v = js_toprim(v, 1); if (v == JS_EXC) return JS_EXC; }
        if (jv_isbig(v)) { jsv t1 = js_big_add_small(jv_b(v), 1); return t1 == JS_EXC ? JS_EXC : js_big_neg(jv_b(t1)); }
        i32 x = js_toint32(v);
        if (js_pending()) return JS_EXC;
        return jv_num((double)~x);
    }
    case OP_VOID: return JS_UNDEF;
    }
    return JS_UNDEF;
}

static NOINL jsv eval_regex(jsnode *n) {
    void **pair = n->x;
    if (!pair) return js_throw_err(E_SYNTAX, "Expressão regular inválida");
    extern jsv js_regexp_from_prog(rxprog *p, jsstr *src, jsstr *flags);
    return js_regexp_from_prog(pair[0], n->str, pair[1]);
}

static jsv eval(jsnode *n, jsenv *env) {
    switch (n->type) {
    case N_NUM: return jv_num(n->num);
    case N_BIGINT: {
        bool ok;
        jsbig *b = js_big_parse(n->str->d, (int)n->str->len, 0, &ok);
        return ok ? jv_big(b) : js_throw_err(E_SYNTAX, "BigInt inválido");
    }
    case N_STR: return jv_str(n->str);
    case N_IDENT:
        if (n->op) return n->op == 1 ? JS_NULL : n->op == 2 ? JS_TRUE : JS_FALSE;
        if (n->str == A[A_undefined]) {
            /* atalho comum: undefined global não sombreado */
            for (jsenv *e = env; e; e = e->parent) if (env_find(e, n->str) >= 0) return get_var(env, n->str);
            return JS_UNDEF;
        }
        J.line = n->line;
        return get_var(env, n->str);
    case N_THIS: return get_this(env);
    case N_TEMPLATE: return eval_template(n, env);
    case N_TAGGED: return eval_tagged(n, env);
    case N_REGEX: return eval_regex(n);
    case N_ARRAY: return eval_array(n, env);
    case N_OBJECT: return eval_object(n, env);
    case N_FUNC: return jv_obj(closure_expr(n, env));
    case N_CLASS: return eval_class(n, env);
    case N_UNARY: return eval_unary(n, env);
    case N_UPDATE: return eval_update(n, env);
    case N_BINARY: {
        jsv a = eval(n->a, env);
        if (a == JS_EXC) return JS_EXC;
        if (a == JS_SHORT) a = JS_UNDEF;
        jsv b = eval(n->b, env);
        if (b == JS_EXC) return JS_EXC;
        if (b == JS_SHORT) b = JS_UNDEF;
        if (n->op == OP_ADD && jv_isnum(a) && jv_isnum(b)) return jv_num(jv_n(a) + jv_n(b));
        J.line = n->line;
        return binop(n->op, a, b);
    }
    case N_LOGICAL: {
        jsv a = eval(n->a, env);
        if (a == JS_EXC) return JS_EXC;
        if (a == JS_SHORT) a = JS_UNDEF;
        if (n->op == OP_AND) { if (!js_truthy(a)) return a; }
        else if (n->op == OP_OR) { if (js_truthy(a)) return a; }
        else if (!jv_isnullish(a)) return a;
        jsv b = eval(n->b, env);
        return b == JS_SHORT ? JS_UNDEF : b;
    }
    case N_ASSIGN: return eval_assign(n, env);
    case N_COND: {
        jsv c = eval(n->a, env);
        if (c == JS_EXC) return JS_EXC;
        jsv r = eval(js_truthy(c == JS_SHORT ? JS_UNDEF : c) ? n->b : n->c, env);
        return r == JS_SHORT ? JS_UNDEF : r;
    }
    case N_CALL: return eval_call(n, env);
    case N_NEW: return eval_new(n, env);
    case N_MEMBER: {
        jsv obj, key;
        int r = member_ref(n, env, &obj, &key);
        if (r < 0) return JS_EXC;
        if (r > 0) return JS_SHORT;
        J.line = n->line;
        return get_member(n, obj, key, env);
    }
    case N_OPTCHAIN: {
        jsv v = eval(n->a, env);
        return v == JS_SHORT ? JS_UNDEF : v;
    }
    case N_SEQ: {
        jsv v = JS_UNDEF;
        for (int i = 0; i < n->n; i++) {
            v = eval(n->list[i], env);
            if (v == JS_EXC) return JS_EXC;
        }
        return v == JS_SHORT ? JS_UNDEF : v;
    }
    case N_NEWTARGET: {
        jsenv *e = this_env(env);
        return e ? e->newtarget : JS_UNDEF;
    }
    case N_IMPORTCALL: {
        jsv v = eval(n->a, env);
        if (v == JS_EXC) return JS_EXC;
        return js_dynamic_import(v);
    }
    case N_IMPORTMETA: return js_import_meta();
    case N_YIELD: {
        jsv v = JS_UNDEF;
        if (n->a) { v = eval(n->a, env); if (v == JS_EXC) return JS_EXC; if (v == JS_SHORT) v = JS_UNDEF; }
        return n->op ? js_yield_star(v) : js_yield(v);
    }
    case N_AWAIT: {
        jsv v = eval(n->a, env);
        if (v == JS_EXC) return JS_EXC;
        if (v == JS_SHORT) v = JS_UNDEF;
        return js_await(v);
    }
    case N_SUPER: return js_throw_err(E_SYNTAX, "Uso inválido de 'super'");
    case N_EMPTY: return JS_UNDEF;
    }
    return js_throw_err(E_SYNTAX, "Expressão não suportada (tipo %d)", n->type);
}

/* ============================================================ instruções */
static bool label_match(jsstr *mine) {
    if (!J.label) return true;
    return mine && js_str_eq(J.label, mine);
}

/* resultado de uma iteração de laço: 0 continua, 1 sai do laço (normal), 2 propaga r */
static int loop_ctl(int r, jsstr *mylabel) {
    if (r == CMP_BREAK) { if (label_match(mylabel)) { J.label = NULL; return 1; } return 2; }
    if (r == CMP_CONTINUE) { if (label_match(mylabel)) { J.label = NULL; return 0; } return 2; }
    if (r == CMP_NORMAL) return 0;
    return 2;
}

static jsenv *copy_env(jsenv *e) {
    jsenv *n = js_newenv(e->parent, (int)e->n);
    for (u32 i = 0; i < e->n; i++) { n->names[i] = e->names[i]; n->vals[i] = e->vals[i]; n->vflags[i] = e->vflags[i]; }
    n->n = e->n;
    return n;
}

static NOINL int exec_for(jsnode *n, jsenv *env) {
    jsstr *mylabel = pending_label;
    pending_label = NULL;
    jsenv *le = env;
    if (n->x) { le = js_newenv(env, 2); declare_block(n->x, le); }
    if (n->a) {
        if (n->a->type == N_VAR) { int r = exec(n->a, le); if (r < 0) return -1; }
        else { jsv v = eval(n->a, le); if (v == JS_EXC) return -1; }
    }
    bool per = (n->flags & NF_PERITER) && n->x;
    if (per) le = copy_env(le);
    for (;;) {
        if (!js_tick_budget()) return -1;
        if (n->b) {
            jsv c = eval(n->b, le);
            if (c == JS_EXC) return -1;
            if (!js_truthy(c)) break;
        }
        int r = exec(n->d, le);
        if (r < 0) return -1;
        int k = loop_ctl(r, mylabel);
        if (k == 1) break;
        if (k == 2) return r;
        if (per) le = copy_env(le);
        if (n->c) { jsv v = eval(n->c, le); if (v == JS_EXC) return -1; }
    }
    return CMP_NORMAL;
}

/* atribui o valor da iteração ao lado esquerdo de for-in/for-of; devolve o ambiente do corpo */
static jsenv *iter_bind(jsnode *n, jsenv *env, jsv v, int *err) {
    jsnode *left = n->a;
    *err = 0;
    if (left->type == N_VAR) {
        jsnode *target = left->list[0]->a;
        if (left->op == VK_VAR) { if (bind_pattern(target, v, env, BIND_VAR) < 0) *err = 1; return env; }
        jsenv *ie = js_newenv(env, 2);
        if (n->x) declare_block(n->x, ie);
        if (bind_pattern(target, v, ie, BIND_INIT) < 0) *err = 1;
        return ie;
    }
    if (bind_pattern(left, v, env, BIND_ASSIGN) < 0) *err = 1;
    return env;
}

static NOINL int exec_forin(jsnode *n, jsenv *env) {
    jsstr *mylabel = pending_label;
    pending_label = NULL;
    jsv o = eval(n->b, env);
    if (o == JS_EXC) return -1;
    if (jv_isnullish(o)) return CMP_NORMAL;
    jsv obj = js_toobj(o);
    if (obj == JS_EXC) return -1;
    jsobj *keys = js_keylist(jv_o(obj), true, true);
    jsobj *volatile hold = keys;
    for (u32 i = 0; i < keys->alen; i++) {
        if (!js_tick_budget()) return -1;
        jsv k = keys->arr[i];
        int has = js_has(obj, k);
        if (has < 0) return -1;
        if (!has) continue;
        int err;
        jsenv *be = iter_bind(n, env, k, &err);
        if (err) return -1;
        int r = exec(n->d, be);
        if (r < 0) return -1;
        int c = loop_ctl(r, mylabel);
        if (c == 1) break;
        if (c == 2) return r;
    }
    (void)hold;
    return CMP_NORMAL;
}

static NOINL int exec_forof(jsnode *n, jsenv *env) {
    jsstr *mylabel = pending_label;
    pending_label = NULL;
    jsv o = eval(n->b, env);
    if (o == JS_EXC) return -1;
    if (n->flags & NF_AWAIT) {
        /* for await: iterador assíncrono, ou síncrono com cada valor esperado */
        if (jv_isnullish(o)) { js_throw_err(E_TYPE, "%s não é iterável", o == JS_NULL ? "null" : "undefined"); return -1; }
        jsv itf = js_getk(o, A[A_sym_asyncIterator]);
        if (itf == JS_EXC) return -1;
        bool asy = js_iscallable(itf);
        jsv it = asy ? js_call(itf, o, 0, NULL) : js_iter_open(o);
        if (it == JS_EXC) return -1;
        for (;;) {
            if (!js_tick_budget()) return -1;
            jsv x;
            if (asy) {
                jsv r = js_invoke(it, A[A_next], 0, NULL);
                if (r == JS_EXC) return -1;
                r = js_await(r);
                if (r == JS_EXC) return -1;
                if (!jv_isobj(r)) { js_throw_err(E_TYPE, "O resultado de next() não é um objeto"); return -1; }
                jsv d = js_getk(r, A[A_done]);
                if (d == JS_EXC) return -1;
                if (js_truthy(d)) break;
                x = js_getk(r, A[A_value]);
                if (x == JS_EXC) return -1;
            } else {
                int s = js_iter_step(it, &x);
                if (s < 0) return -1;
                if (!s) break;
                x = js_await(x);
                if (x == JS_EXC) return -1;
            }
            int err;
            jsenv *be = iter_bind(n, env, x, &err);
            if (err) return -1;
            int r = exec(n->d, be);
            if (r < 0) return -1;
            int c = loop_ctl(r, mylabel);
            if (c == 1) break;
            if (c == 2) return r;
        }
        return CMP_NORMAL;
    }
    extern bool js_array_iter_pristine(jsv v);
    if ((jv_iscls(o, CL_ARRAY) && js_array_iter_pristine(o)) || jv_iscls(o, CL_ARGUMENTS)) {
        jsobj *a = jv_o(o);
        for (u32 i = 0; i < a->alen; i++) {
            if (!js_tick_budget()) return -1;
            int err;
            jsenv *be = iter_bind(n, env, js_arr_get(a, i), &err);
            if (err) return -1;
            int r = exec(n->d, be);
            if (r < 0) return -1;
            int c = loop_ctl(r, mylabel);
            if (c == 1) break;
            if (c == 2) return r;
        }
        return CMP_NORMAL;
    }
    jsv it = js_iter_open(o);
    if (it == JS_EXC) return -1;
    for (;;) {
        if (!js_tick_budget()) return -1;
        jsv x;
        int s = js_iter_step(it, &x);
        if (s < 0) return -1;
        if (!s) break;
        int err;
        jsenv *be = iter_bind(n, env, x, &err);
        if (err) return -1;
        int r = exec(n->d, be);
        if (r < 0) return -1;
        int c = loop_ctl(r, mylabel);
        if (c == 1) break;
        if (c == 2) return r;
    }
    return CMP_NORMAL;
}

static NOINL int exec_while(jsnode *n, jsenv *env, bool dowhile) {
    jsstr *mylabel = pending_label;
    pending_label = NULL;
    for (bool first = true;; first = false) {
        if (!js_tick_budget()) return -1;
        if (!(dowhile && first)) {
            jsv c = eval(n->a, env);
            if (c == JS_EXC) return -1;
            if (!js_truthy(c)) break;
        }
        int r = exec(n->d, env);
        if (r < 0) return -1;
        int k = loop_ctl(r, mylabel);
        if (k == 1) break;
        if (k == 2) return r;
    }
    return CMP_NORMAL;
}

static NOINL int exec_try(jsnode *n, jsenv *env) {
    int r = exec(n->a, env);
    if (r < 0 && n->c && !J.interrupted && !js_is_genreturn(J.exc)) {
        jsv e = js_catch();
        jsenv *ce = js_newenv(env, 1);
        if (n->b) {
            if (bind_pattern(n->b, e, ce, BIND_DECL) < 0) r = -1;
            else r = exec(n->c, ce);
        } else r = exec(n->c, ce);
    }
    if (n->d && !J.interrupted) {
        jsv exc = JS_UNDEF, rv = J.retval;
        jsstr *lab = J.label;
        bool had = r < 0;
        if (had) exc = js_catch();
        int r2 = exec(n->d, env);
        if (r2 != CMP_NORMAL) return r2;
        J.retval = rv;
        J.label = lab;
        if (had) { js_throw(exc); return -1; }
    }
    return r;
}

static NOINL int exec_switch(jsnode *n, jsenv *env) {
    jsstr *mylabel = pending_label;
    pending_label = NULL;
    jsv d = eval(n->a, env);
    if (d == JS_EXC) return -1;
    jsenv *se = env;
    if (n->x) { se = js_newenv(env, 2); declare_block(n->x, se); }
    int start = -1, def = -1;
    for (int i = 0; i < n->n; i++) {
        jsnode *c = n->list[i];
        if (!c->a) { def = i; continue; }
        jsv v = eval(c->a, se);
        if (v == JS_EXC) return -1;
        if (js_strict_eq(d, v)) { start = i; break; }
    }
    if (start < 0) start = def;
    if (start < 0) return CMP_NORMAL;
    for (int i = start; i < n->n; i++) {
        jsnode *c = n->list[i];
        int r = exec_list(c->list, c->n, se);
        if (r < 0) return -1;
        if (r == CMP_BREAK && (!J.label || (mylabel && js_str_eq(J.label, mylabel)))) { J.label = NULL; return CMP_NORMAL; }
        if (r != CMP_NORMAL) return r;
    }
    return CMP_NORMAL;
}

static int exec_list(jsnode **list, int n, jsenv *env) {
    for (int i = 0; i < n; i++) {
        int r = exec(list[i], env);
        if (r != CMP_NORMAL) return r;
    }
    return CMP_NORMAL;
}

static int exec(jsnode *n, jsenv *env) {
    J.line = n->line;
    switch (n->type) {
    case N_EXPR: {
        jsv v = eval(n->a, env);
        if (v == JS_EXC) return -1;
        J.completion = v == JS_SHORT ? JS_UNDEF : v;
        return CMP_NORMAL;
    }
    case N_VAR:
        for (int i = 0; i < n->n; i++) {
            jsnode *d = n->list[i];
            if (!d->b) {
                if (n->op != VK_VAR && bind_pattern(d->a, JS_UNDEF, env, BIND_INIT) < 0) return -1;
                continue;
            }
            jsv v = eval(d->b, env);
            if (v == JS_EXC) return -1;
            if (v == JS_SHORT) v = JS_UNDEF;
            if (bind_pattern(d->a, v, env, n->op == VK_VAR ? BIND_VAR : BIND_INIT) < 0) return -1;
        }
        return CMP_NORMAL;
    case N_FUNCDECL: case N_EMPTY: return CMP_NORMAL;
    case N_EXPORTDEF: {
        jsv v = n->a->type == N_CLASS ? eval_class(n->a, env) : eval(n->a, env);
        if (v == JS_EXC) return -1;
        if (v == JS_SHORT) v = JS_UNDEF;
        if (jv_isobj(v) && (n->a->type == N_FUNC || n->a->type == N_CLASS)) {
            jsprop *np = js_ownprop(jv_o(v), A[A_name]);
            if (np && jv_isstr(np->v) && !jv_s(np->v)->len) np->v = jv_str(js_intern("default", 7));
        }
        init_lex(env, js_intern("*default*", 9), v);
        return CMP_NORMAL;
    }
    case N_CLASSDECL: {
        jsv c = eval_class(n->a, env);
        if (c == JS_EXC) return -1;
        if (n->str) init_lex(env, n->str, c);
        return CMP_NORMAL;
    }
    case N_RETURN:
        if (n->a) {
            jsv v = eval(n->a, env);
            if (v == JS_EXC) return -1;
            J.retval = v == JS_SHORT ? JS_UNDEF : v;
        } else J.retval = JS_UNDEF;
        return CMP_RETURN;
    case N_IF: {
        jsv c = eval(n->a, env);
        if (c == JS_EXC) return -1;
        if (js_truthy(c == JS_SHORT ? JS_UNDEF : c)) return exec(n->b, env);
        if (n->c) return exec(n->c, env);
        return CMP_NORMAL;
    }
    case N_BLOCK: {
        pending_label = NULL;
        jsenv *be = env;
        if (n->x) { be = js_newenv(env, ((jsblock *)n->x)->n + 1); declare_block(n->x, be); }
        return exec_list(n->list, n->n, be);
    }
    case N_FOR: return exec_for(n, env);
    case N_FORIN: return exec_forin(n, env);
    case N_FOROF: return exec_forof(n, env);
    case N_WHILE: return exec_while(n, env, false);
    case N_DOWHILE: return exec_while(n, env, true);
    case N_BREAK: J.label = n->str; return CMP_BREAK;
    case N_CONTINUE: J.label = n->str; return CMP_CONTINUE;
    case N_THROW: {
        jsv v = eval(n->a, env);
        if (v == JS_EXC) return -1;
        js_throw(v);
        return -1;
    }
    case N_TRY: return exec_try(n, env);
    case N_SWITCH: return exec_switch(n, env);
    case N_LABELED: {
        int t = n->a->type;
        pending_label = (t == N_FOR || t == N_FORIN || t == N_FOROF || t == N_WHILE || t == N_DOWHILE || t == N_SWITCH) ? n->str : NULL;
        int r = exec(n->a, env);
        pending_label = NULL;
        if (r == CMP_BREAK && J.label && js_str_eq(J.label, n->str)) { J.label = NULL; return CMP_NORMAL; }
        return r;
    }
    }
    jsv v = eval(n, env);
    return v == JS_EXC ? -1 : CMP_NORMAL;
}

/* ============================================================ scripts */
jsv js_run_script(jsscript *s) {
    jsrealm *r = J.realm;
    jsscript *saved = cur_script;
    jsstr *saved_name = J.srcname;
    cur_script = s;
    J.srcname = s->name;
    jsenv *env = r->glex;
    jsobj *g = r->global;
    jsfunc *top = s->top;
    for (int i = 0; i < top->nvars; i++)
        if (!js_findprop(g, top->vars[i], NULL)) js_defprop(g, top->vars[i], JS_UNDEF, 0);
    for (int i = 0; i < top->nfuncs; i++) {
        jsnode *fd = top->funcs[i];
        jsobj *f = js_make_closure(fd->a->x, env, s);
        js_defprop(g, fd->str, jv_obj(f), 0);
    }
    if (s->body->x) declare_block(s->body->x, env);
    J.completion = JS_UNDEF;
    int rc = exec_list(s->body->list, s->body->n, env);
    cur_script = saved;
    J.srcname = saved_name;
    if (rc < 0) return JS_EXC;
    jsv c = J.completion;
    J.completion = JS_UNDEF;
    return c;
}

jsv js_run_module(jsscript *s, jsenv *menv) {
    jsscript *saved = cur_script;
    jsstr *saved_name = J.srcname;
    cur_script = s;
    J.srcname = s->name;
    jsfunc *top = s->top;
    for (int i = 0; i < top->nvars; i++) if (env_find(menv, top->vars[i]) < 0) js_env_decl(menv, top->vars[i], JS_UNDEF, 0);
    for (int i = 0; i < top->nfuncs; i++) {
        jsnode *fd = top->funcs[i];
        js_env_decl(menv, fd->str, jv_obj(js_make_closure(fd->a->x, menv, s)), 0);
    }
    if (s->body->x) declare_block(s->body->x, menv);
    int rc = exec_list(s->body->list, s->body->n, menv);
    cur_script = saved;
    J.srcname = saved_name;
    return rc < 0 ? JS_EXC : JS_UNDEF;
}

jsv js_eval_string(jsstr *code, const char *name) {
    jsstr *err = NULL;
    jsscript *s = js_parse(code->d, (int)code->len, name ? name : "eval", 1, &err);
    if (!s) return js_throw_err(E_SYNTAX, "%s", err ? err->d : "erro de sintaxe");
    return js_run_script(s);
}

jsv js_new_function(int argc, jsv *argv) {
    sbuf_t sb = {0};
    sb_puts(&sb, "(function anonymous(");
    for (int i = 0; i < argc - 1; i++) {
        jsstr *p = js_tostr(argv[i]);
        if (!p) { sb_free(&sb); return JS_EXC; }
        if (i) sb_putc(&sb, ',');
        sb_putn(&sb, p->d, (int)p->len);
    }
    sb_puts(&sb, "\n) {\n");
    if (argc > 0) {
        jsstr *b = js_tostr(argv[argc - 1]);
        if (!b) { sb_free(&sb); return JS_EXC; }
        sb_putn(&sb, b->d, (int)b->len);
    }
    sb_puts(&sb, "\n})");
    jsstr *err = NULL;
    jsscript *s = js_parse(sb.p, sb.len, "Function", 1, &err);
    sb_free(&sb);
    if (!s) return js_throw_err(E_SYNTAX, "%s", err ? err->d : "erro de sintaxe");
    return js_run_script(s);
}
