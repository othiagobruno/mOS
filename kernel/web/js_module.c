/* mOS — motor JavaScript "Brisa": módulos ES (import/export, import() dinâmico, <script type="module">)
 *
 * Cada módulo é um registro (objeto CL_MODULE) guardado no mapa do reino por URL absoluta: é baixado
 * pela mesma fila do fetch (http/https/file), analisado, ligado e avaliado uma única vez.
 * Ligação: cada módulo tem um ambiente próprio (pai = escopo léxico global) e um objeto namespace
 * cujas propriedades são acessores — as importações apontam para [namespace, nome], então os
 * bindings são vivos (mudanças no módulo exportador aparecem em quem importou). */
#include "js_int.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

enum { M_FETCHING, M_PARSED, M_LINKED, M_EVALUATING, M_DONE, M_ERROR };

typedef struct jsmodule {
    char url[1024];
    int state;
    jsscript *script;
    jsenv *env;
    jsobj *ns;
    jsv error;
    jsobj *deps;        /* vetor de registros, paralelo a script->mreq */
    u32 visit;
} jsmodule;

static u32 visit_gen;

static jsmodule *M(jsobj *o) { return o->u.mod; }

void js_module_mark(jsobj *o) {
    jsmodule *m = M(o);
    if (!m) return;
    gc_mark_ptr(m->script);
    gc_mark_ptr(m->env);
    gc_mark_ptr(m->ns);
    gc_mark_val(m->error);
    gc_mark_ptr(m->deps);
}

void js_module_free(jsobj *o) { kfree(o->u.mod); o->u.mod = NULL; }

static jsobj *registry(void) {
    jsrealm *r = J.realm;
    if (!r->modules) r->modules = js_newobj(NULL);
    if (!r->modjobs) r->modjobs = js_newarr(0);
    return r->modules;
}

static jsobj *new_module(const char *url) {
    jsobj *o = js_newobj_cls(CL_MODULE, NULL);
    jsmodule *m = kcalloc(1, sizeof(jsmodule));
    if (!m) panic("JS: memória esgotada (módulo)");
    strncpy(m->url, url, sizeof m->url - 1);
    m->state = M_FETCHING;
    m->error = JS_UNDEF;
    o->u.mod = m;
    js_defprop(registry(), js_strz(url), jv_obj(o), 0);
    return o;
}

static jsobj *get_module(const char *url) {
    jsprop *p = js_ownprop(registry(), js_strz(url));
    if (p) return jv_o(p->v);
    jsobj *o = new_module(url);
    js_net_fetch_module(J.realm->page, url, o);
    return o;
}

static void check_jobs(void);

static void fail(jsmodule *m, jsv err) {
    m->state = M_ERROR;
    m->error = err;
}

static void set_source(jsobj *o, const char *src, int len) {
    jsmodule *m = M(o);
    jsstr *err = NULL;
    jsscript *s = js_parse_ex(src, len, m->url, 1, true, &err);
    if (!s) {
        jsv e = jv_obj(js_make_err(E_SYNTAX, js_str_fmt("%s (módulo %s)", err ? err->d : "erro de sintaxe", m->url)));
        fail(m, e);
        return;
    }
    m->script = s;
    m->state = M_PARSED;
    m->deps = js_newarr(0);
    for (int i = 0; i < s->nmreq; i++) {
        char url[1024];
        url_resolve(m->url, s->mreq[i]->d, url, sizeof url);
        js_arr_push(m->deps, jv_obj(get_module(url)));
    }
}

void js_module_fetched(jsobj *mod, const char *data, int len, int status, const char *err) {
    jsmodule *m = M(mod);
    if (!m || m->state != M_FETCHING) return;
    if (err || status >= 400 || !data) {
        fail(m, jv_obj(js_make_err(E_TYPE, js_str_fmt("Falha ao carregar o módulo %s (%s)", m->url,
                                                         err ? err : status == 404 ? "não encontrado" : "erro HTTP"))));
    } else set_source(mod, data, len);
    check_jobs();
}

/* ------------------------------------------------------------ estado do grafo */
/* 0 ainda baixando, 1 pronto, -1 erro (*errv recebe o erro) */
static int graph_status(jsobj *o, jsv *errv) {
    jsmodule *m = M(o);
    if (m->visit == visit_gen) return 1;
    m->visit = visit_gen;
    if (m->state == M_ERROR) { *errv = m->error; return -1; }
    if (m->state == M_FETCHING) return 0;
    int r = 1;
    for (u32 i = 0; m->deps && i < m->deps->alen; i++) {
        int s = graph_status(jv_o(m->deps->arr[i]), errv);
        if (s < 0) return -1;
        if (s == 0) r = 0;
    }
    return r;
}

static jsobj *dep_for(jsmodule *m, jsstr *spec) {
    for (int i = 0; i < m->script->nmreq; i++) if (js_str_eq(m->script->mreq[i], spec)) return jv_o(m->deps->arr[i]);
    return NULL;
}

/* ------------------------------------------------------------ namespaces */
NATIVE(ns_getter) {
    UNUSED_ARGS;
    switch (self->u.nat.magic) {
    case 0: { /* binding local do módulo */
        jsmodule *m = M(jv_o(self->u.nat.data));
        if (!m->env) return js_throw_err(E_REF, "Módulo ainda não ligado");
        return js_env_get(m->env, jv_s(self->u.nat.data2));
    }
    case 1: return js_getk(self->u.nat.data, jv_s(self->u.nat.data2));   /* reexportação */
    default: return self->u.nat.data;                                     /* export * as */
    }
}

static bool has_export(jsobj *o, jsstr *name, int depth);

/* nomes exportados (inclui export * recursivo, sem "default"); vetor de strings */
static void export_names(jsobj *o, jsobj *out, int depth) {
    jsmodule *m = M(o);
    if (!m->script || depth > 64) return;
    jsscript *s = m->script;
    for (int i = 0; i < s->nexports; i++) {
        jsexport *e = &s->exports[i];
        if (e->exp) {
            bool dup = false;
            for (u32 k = 0; k < out->alen; k++) if (js_str_eq(jv_s(out->arr[k]), e->exp)) dup = true;
            if (!dup) js_arr_push(out, jv_str(e->exp));
        }
    }
    for (int i = 0; i < s->nexports; i++) {
        jsexport *e = &s->exports[i];
        if (e->exp || !e->spec) continue;
        jsobj *d = dep_for(m, e->spec);
        if (!d) continue;
        jsobj *sub = js_newarr(0);
        export_names(d, sub, depth + 1);
        for (u32 j = 0; j < sub->alen; j++) {
            jsstr *nm = jv_s(sub->arr[j]);
            if (!strcmp(nm->d, "default")) continue;
            bool dup = false;
            for (u32 k = 0; k < out->alen; k++) if (js_str_eq(jv_s(out->arr[k]), nm)) dup = true;
            if (!dup) js_arr_push(out, jv_str(nm));
        }
    }
}

static bool has_export(jsobj *o, jsstr *name, int depth) {
    jsobj *names = js_newarr(0);
    export_names(o, names, depth);
    for (u32 i = 0; i < names->alen; i++) if (js_str_eq(jv_s(names->arr[i]), name)) return true;
    return false;
}

static void fill_namespace(jsobj *o) {
    jsmodule *m = M(o);
    jsscript *s = m->script;
    jsobj *ns = m->ns;
    for (int i = 0; i < s->nexports; i++) {
        jsexport *e = &s->exports[i];
        if (!e->exp) continue;
        jsobj *g = js_newnative(ns_getter, e->exp->d, 0, 0);
        if (e->local) { g->u.nat.data = jv_obj(o); g->u.nat.data2 = jv_str(e->local); }
        else {
            jsobj *d = dep_for(m, e->spec);
            if (!d) continue;
            if (e->imp && !strcmp(e->imp->d, "*")) { g->u.nat.magic = 2; g->u.nat.data = jv_obj(M(d)->ns); }
            else { g->u.nat.magic = 1; g->u.nat.data = jv_obj(M(d)->ns); g->u.nat.data2 = jv_str(e->imp); }
        }
        js_defaccessor(ns, e->exp, jv_obj(g), JS_UNDEF, 0);
    }
    /* export * from: encaminha os nomes ainda não definidos */
    for (int i = 0; i < s->nexports; i++) {
        jsexport *e = &s->exports[i];
        if (e->exp || !e->spec) continue;
        jsobj *d = dep_for(m, e->spec);
        if (!d) continue;
        jsobj *names = js_newarr(0);
        export_names(d, names, 0);
        for (u32 j = 0; j < names->alen; j++) {
            jsstr *nm = jv_s(names->arr[j]);
            if (!strcmp(nm->d, "default") || js_ownprop(ns, nm)) continue;
            jsobj *g = js_newnative(ns_getter, nm->d, 0, 1);
            g->u.nat.data = jv_obj(M(d)->ns);
            g->u.nat.data2 = jv_str(nm);
            js_defaccessor(ns, nm, jv_obj(g), JS_UNDEF, 0);
        }
    }
    js_defprop(ns, A[A_sym_toStringTag], jv_str(js_intern("Module", -1)), PF_NOENUM | PF_RO);
    ns->noext = 1;
}

/* coleta o grafo em pós-ordem e cria ambiente + namespace dos módulos ainda não ligados */
static void collect(jsobj *o, jsobj *list) {
    jsmodule *m = M(o);
    if (m->visit == visit_gen) return;
    m->visit = visit_gen;
    for (u32 i = 0; m->deps && i < m->deps->alen; i++) collect(jv_o(m->deps->arr[i]), list);
    if (m->state == M_PARSED) {
        m->env = js_newenv(J.realm->glex, 8);
        m->ns = js_newobj(NULL);
        js_arr_push(list, jv_obj(o));
    }
}

static bool link_graph(jsobj *root, jsv *errv) {
    jsobj *list = js_newarr(0);
    visit_gen++;
    collect(root, list);
    for (u32 i = 0; i < list->alen; i++) fill_namespace(jv_o(list->arr[i]));
    for (u32 i = 0; i < list->alen; i++) {
        jsobj *o = jv_o(list->arr[i]);
        jsmodule *m = M(o);
        jsscript *s = m->script;
        for (int k = 0; k < s->nimports; k++) {
            jsimport *im = &s->imports[k];
            jsobj *d = dep_for(m, im->spec);
            if (!d) continue;
            if (!im->imp) { js_env_decl(m->env, im->local, jv_obj(M(d)->ns), VF_CONST | VF_LEX); continue; }
            if (!has_export(d, im->imp, 0)) {
                *errv = jv_obj(js_make_err(E_SYNTAX, js_str_fmt("O módulo %s não exporta '%s' (importado por %s)", M(d)->url, im->imp->d, m->url)));
                for (u32 j = 0; j < list->alen; j++) fail(M(jv_o(list->arr[j])), *errv);
                return false;
            }
            jsv ref[2] = {jv_obj(M(d)->ns), jv_str(im->imp)};
            js_env_decl(m->env, im->local, jv_obj(js_arr_from(ref, 2)), VF_CONST | VF_LEX | VF_REF);
        }
        m->state = M_LINKED;
    }
    return true;
}

static bool evaluate(jsobj *o, jsv *errv) {
    jsmodule *m = M(o);
    if (m->state == M_ERROR) { *errv = m->error; return false; }
    if (m->state == M_DONE || m->state == M_EVALUATING) return true;
    m->state = M_EVALUATING;
    for (u32 i = 0; m->deps && i < m->deps->alen; i++) {
        if (!evaluate(jv_o(m->deps->arr[i]), errv)) { fail(m, *errv); return false; }
    }
    J.interrupted = false;
    js_set_budget(5000);
    jsv r = js_run_module(m->script, m->env);
    if (r == JS_EXC) {
        jsv e = js_catch();
        /* informa no console como um erro de script */
        jsstr *saved = J.srcname;
        J.srcname = m->script->name;
        js_throw(e);
        js_report_exception();
        J.srcname = saved;
        fail(m, e);
        *errv = e;
        return false;
    }
    m->state = M_DONE;
    return true;
}

/* ------------------------------------------------------------ jobs: [raiz, promessa|null, estado] */
static void check_jobs(void) {
    jsobj *jobs = J.realm->modjobs;
    if (!jobs) return;
    for (u32 i = 0; i < jobs->alen; i++) {
        jsobj *job = jv_o(jobs->arr[i]);
        if (jv_n(job->arr[2]) != 0) continue;
        jsobj *root = jv_o(job->arr[0]);
        jsv err = JS_UNDEF;
        visit_gen++;
        int st = graph_status(root, &err);
        if (st == 0) continue;
        job->arr[2] = jv_int(2);   /* em andamento: chamadas reentrantes não o processam de novo */
        bool ok = st > 0 && link_graph(root, &err) && evaluate(root, &err);
        job->arr[2] = jv_int(ok ? 1 : -1);
        if (jv_isobj(job->arr[1])) {
            jsobj *pr = jv_o(job->arr[1]);
            if (ok) js_promise_resolve(pr, jv_obj(M(root)->ns));
            else js_promise_reject(pr, err);
        } else if (!ok && st < 0) {
            /* <script type="module"> cujo grafo falhou ao carregar */
            jsstr *s = js_exc_string(err);
            char b[600];
            snprintf(b, sizeof b, "Uncaught %s", s ? s->d : "erro de módulo");
            js_console_log(3, b);
        }
        jobs = J.realm->modjobs;
    }
    /* remove os concluídos sem promessa pendente de leitura (mantém os de script até serem consultados) */
}

static jsv add_job(jsobj *root, jsobj *promise) {
    registry();
    jsv v[3] = {jv_obj(root), promise ? jv_obj(promise) : JS_NULL, jv_int(0)};
    jsobj *job = js_arr_from(v, 3);
    js_arr_push(J.realm->modjobs, jv_obj(job));
    check_jobs();
    return jv_obj(job);
}

static void base_url(char *out, int cap) {
    jsscript *s = js_cur_script();
    if (s && s->is_module && s->name) { strncpy(out, s->name->d, (usize)cap - 1); out[cap - 1] = 0; return; }
    const char *b = js_page_base(J.realm->page);
    strncpy(out, b ? b : "", (usize)cap - 1);
    out[cap - 1] = 0;
}

jsv js_dynamic_import(jsv spec) {
    jsobj *p = js_promise_new();
    jsstr *s = js_tostr(spec);
    if (!s) { js_promise_reject(p, js_catch()); return jv_obj(p); }
    if (!J.realm->page) { js_promise_reject(p, jv_obj(js_make_err(E_TYPE, js_strz("import() exige uma página")))); return jv_obj(p); }
    char base[1024], url[1024];
    base_url(base, sizeof base);
    url_resolve(base, s->d, url, sizeof url);
    add_job(get_module(url), p);
    return jv_obj(p);
}

jsv js_module_start_script(struct js_page *p, const char *url, const char *inline_src, int len) {
    (void)p;
    jsobj *root;
    if (inline_src) {
        static int counter;
        char u[1100];
        snprintf(u, sizeof u, "%s", url);
        jsprop *ex = js_ownprop(registry(), js_strz(u));
        if (ex) snprintf(u, sizeof u, "%s#modulo-inline-%d", url, ++counter);
        root = new_module(u);
        /* imports relativos a partir da URL da página */
        strncpy(M(root)->url, url, sizeof M(root)->url - 1);
        set_source(root, inline_src, len);
    } else root = get_module(url);
    return add_job(root, NULL);
}

int js_module_job_state(jsv job) {
    if (!jv_isobj(job)) return 1;
    int s = (int)jv_n(jv_o(job)->arr[2]);
    return s == 2 ? 0 : s;
}

jsv js_import_meta(void) {
    jsscript *s = js_cur_script();
    if (!s || !s->is_module) return js_throw_err(E_SYNTAX, "import.meta só é válido em módulos");
    jsobj *o = js_newobj(J.realm->p_object);
    js_defn(o, "url", jv_str(s->name), 0);
    return jv_obj(o);
}
