/* mOS — observadores do DOM no motor web: MutationObserver (childList, attributes, characterData, subtree,
 * oldValue, attributeFilter, takeRecords, entrega em microtarefa), ResizeObserver (content/border box, entregue
 * depois do layout) e IntersectionObserver (raiz = viewport ou elemento, rootMargin, thresholds). Implementação
 * própria a partir das especificações DOM, Resize Observer e Intersection Observer. */
#include "js_int.h"
#include "web.h"
#include "raster.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

static bool has_proto(jsobj *o, jsobj *p) {
    for (int i = 0; o && p && i < 32; i++, o = o->proto) if (o->proto == p) return true;
    return false;
}
static node_t *node_of(jsv v) { return jv_iscls(v, CL_NODE) ? jv_o(v)->u.host.node : NULL; }

/* registro de observadores da página (no global, para o coletor) */
static jsobj *registry(const char *key) {
    jsobj *g = J.realm->global;
    jsstr *k = js_intern(key, -1);
    jsprop *p = js_ownprop(g, k);
    if (p && jv_isobj(p->v)) return jv_o(p->v);
    jsobj *a = js_newarr(0);
    js_defprop(g, k, jv_obj(a), PF_NOENUM);
    return a;
}

/* ============================================================ MutationObserver */
/* extra: [callback, registros, alvos: [nó, opções]..., agendado] */
enum { M_CB, M_RECORDS, M_TARGETS, M_TRANSIENT, M_NSLOTS };   /* transitórios: [nó, opções]... até a próxima entrega */
enum { O_CHILD = 1, O_ATTR = 2, O_DATA = 4, O_SUBTREE = 8, O_ATTROLD = 16, O_DATAOLD = 32 };

static int mo_live;               /* alvos observados em todas as páginas: 0 = ganchos sem custo */
static int mo_batch;              /* operações compostas (innerHTML, textContent) juntam registros */
static u32 mo_batch_id, mo_next_batch;
static bool mo_scheduled;

void js_mo_batch(int delta) {
    if (delta > 0 && mo_batch++ == 0) mo_batch_id = ++mo_next_batch;
    else if (delta < 0 && mo_batch > 0 && --mo_batch == 0) mo_batch_id = 0;
}

static jsobj *mo_x(jsv v) {
    if (!jv_iscls(v, CL_HOST) || !has_proto(jv_o(v), J.realm->p_mutobs) || !jv_isobj(jv_o(v)->u.host.extra)) return NULL;
    return jv_o(jv_o(v)->u.host.extra);
}

NATIVE(mo_deliver) {
    UNUSED_ARGS;
    mo_scheduled = false;
    jsobj *reg = registry("\x01" "mutacoes");
    jsobj *list = js_arr_from(reg->arr, (int)reg->alen);
    for (u32 i = 0; i < list->alen; i++) {
        jsobj *x = mo_x(list->arr[i]);
        if (!x) continue;
        js_arr_setlen(jv_o(js_arr_get(x, M_TRANSIENT)), 0);   /* registros transitórios valem até a entrega */
        jsobj *recs = jv_o(js_arr_get(x, M_RECORDS));
        if (!recs->alen) continue;
        js_arr_set(x, M_RECORDS, jv_obj(js_newarr(0)));
        jsv a[2] = { jv_obj(recs), list->arr[i] };
        if (js_call(js_arr_get(x, M_CB), list->arr[i], 2, a) == JS_EXC) js_report_exception();
    }
    return JS_UNDEF;
}

static void mo_schedule(void) {
    if (mo_scheduled) return;
    mo_scheduled = true;
    js_enqueue_job(jv_obj(js_newnative(mo_deliver, "entregar", 0, 0)), JS_HOLE, JS_UNDEF, JOB_CALL);
}

static jsobj *mo_record(const char *type, jsv target) {
    jsobj *r = js_newobj(J.realm->p_mutrec);
    js_defn(r, "type", jv_str(js_intern(type, -1)), 0);
    js_defn(r, "target", target, 0);
    js_defn(r, "addedNodes", jv_obj(js_newarr(0)), 0);
    js_defn(r, "removedNodes", jv_obj(js_newarr(0)), 0);
    js_defn(r, "previousSibling", JS_NULL, 0);
    js_defn(r, "nextSibling", JS_NULL, 0);
    js_defn(r, "attributeName", JS_NULL, 0);
    js_defn(r, "attributeNamespace", JS_NULL, 0);
    js_defn(r, "oldValue", JS_NULL, 0);
    js_defn(r, "\x01" "lote", jv_int((int)mo_batch_id), PF_NOENUM);
    return r;
}

/* observadores interessados numa mutação de 'type' em n; chama f para cada um */
typedef void (*mo_fn)(jsobj *obs_x, int opts, jsv filter, void *ctx);
static void mo_each(node_t *n, int need, const char *attr, mo_fn f, void *ctx) {
    jsobj *reg = registry("\x01" "mutacoes");
    for (u32 i = 0; i < reg->alen; i++) {
        jsobj *x = mo_x(reg->arr[i]);
        if (!x) continue;
        int best = 0;
        jsv filter = JS_UNDEF;
        for (int list = 0; list < 2; list++) {
        jsobj *t = jv_o(js_arr_get(x, list ? M_TRANSIENT : M_TARGETS));
        for (u32 k = 0; k + 1 < t->alen; k += 2) {
            node_t *tn = node_of(t->arr[k]);
            int opts = js_toint32(js_getn(t->arr[k + 1], "\x01" "o"));
            bool on_path = false;
            for (node_t *a = n; a; a = a->parent) if (a == tn) { on_path = a == n || (opts & O_SUBTREE); break; }
            if (!on_path || !(opts & need)) continue;
            jsv flt = js_getn(t->arr[k + 1], "attributeFilter");
            if (attr && jv_isobj(flt)) {
                bool hit = false;
                u32 fn = js_lengthof(flt);
                for (u32 q = 0; q < fn; q++) { jsv e = js_getidx(flt, q); if (jv_isstr(e) && !strcmp(jv_s(e)->d, attr)) hit = true; }
                if (!hit) continue;
            }
            best |= opts;
            filter = flt;
        }
        }
        if (best) f(x, best, filter, ctx);
    }
}

typedef struct { node_t *par, *added, *removed, *prev, *next; } child_ctx;
static void child_cb(jsobj *x, int opts, jsv filter, void *c0) {
    (void)opts; (void)filter;
    child_ctx *c = c0;
    js_page_t *p = js_cur_page();
    jsobj *recs = jv_o(js_arr_get(x, M_RECORDS));
    jsv tv = js_dom_wrapv(p, c->par);
    /* lote: junta no registro anterior do mesmo alvo */
    if (mo_batch && recs->alen) {
        jsv last = recs->arr[recs->alen - 1];
        if (js_getn(last, "target") == tv && js_toint32(js_getn(last, "\x01" "lote")) == (int)mo_batch_id &&
            !strcmp(jv_s(js_getn(last, "type"))->d, "childList")) {
            if (c->added) js_arr_push(jv_o(js_getn(last, "addedNodes")), js_dom_wrapv(p, c->added));
            if (c->removed) js_arr_push(jv_o(js_getn(last, "removedNodes")), js_dom_wrapv(p, c->removed));
            if (c->added) js_defn(jv_o(last), "nextSibling", c->next ? js_dom_wrapv(p, c->next) : JS_NULL, 0);
            return;
        }
    }
    jsobj *r = mo_record("childList", tv);
    if (c->added) js_arr_push(jv_o(js_getn(jv_obj(r), "addedNodes")), js_dom_wrapv(p, c->added));
    if (c->removed) js_arr_push(jv_o(js_getn(jv_obj(r), "removedNodes")), js_dom_wrapv(p, c->removed));
    js_defn(r, "previousSibling", c->prev ? js_dom_wrapv(p, c->prev) : JS_NULL, 0);
    js_defn(r, "nextSibling", c->next ? js_dom_wrapv(p, c->next) : JS_NULL, 0);
    js_arr_push(recs, jv_obj(r));
    mo_schedule();
}

/* nó tirado de uma subárvore observada com subtree: ganha um registro transitório (mutações nele ainda chegam até a
 * próxima entrega) */
static void add_transient(node_t *par, node_t *removed) {
    js_page_t *p = js_cur_page();
    jsobj *reg = registry("\x01" "mutacoes");
    for (u32 i = 0; i < reg->alen; i++) {
        jsobj *x = mo_x(reg->arr[i]);
        if (!x) continue;
        jsobj *t = jv_o(js_arr_get(x, M_TARGETS)), *tr = jv_o(js_arr_get(x, M_TRANSIENT));
        for (u32 k = 0; k + 1 < t->alen; k += 2) {
            node_t *tn = node_of(t->arr[k]);
            int opts = js_toint32(js_getn(t->arr[k + 1], "\x01" "o"));
            if (!(opts & O_SUBTREE)) continue;
            bool on_path = false;
            for (node_t *a = par; a && !on_path; a = a->parent) on_path = a == tn;
            if (!on_path) continue;
            js_arr_push(tr, js_dom_wrapv(p, removed));
            js_arr_push(tr, t->arr[k + 1]);
        }
    }
}

void js_mo_child(node_t *par, node_t *added, node_t *removed, node_t *prev, node_t *next) {
    if (!mo_live || !par || !js_cur_page()) return;
    child_ctx c = { par, added, removed, prev, next };
    mo_each(par, O_CHILD, NULL, child_cb, &c);
    if (removed) add_transient(par, removed);
}

typedef struct { node_t *n; const char *name; const char *old; bool data; } attr_ctx;
static void attr_cb(jsobj *x, int opts, jsv filter, void *c0) {
    (void)filter;
    attr_ctx *c = c0;
    js_page_t *p = js_cur_page();
    jsobj *r = mo_record(c->data ? "characterData" : "attributes", js_dom_wrapv(p, c->n));
    if (!c->data) js_defn(r, "attributeName", jv_str(js_strz(c->name)), 0);
    bool want_old = c->data ? (opts & O_DATAOLD) : (opts & O_ATTROLD);
    if (want_old && c->old) js_defn(r, "oldValue", jv_str(js_strz(c->old)), 0);
    js_arr_push(jv_o(js_arr_get(x, M_RECORDS)), jv_obj(r));
    mo_schedule();
}

void js_mo_attr(node_t *n, const char *name, const char *old) {
    if (!mo_live || !js_cur_page()) return;
    attr_ctx c = { n, name, old, false };
    mo_each(n, O_ATTR, name, attr_cb, &c);
}

void js_mo_data(node_t *n, const char *old) {
    if (!mo_live || !js_cur_page()) return;
    attr_ctx c = { n, NULL, old, true };
    mo_each(n, O_DATA, NULL, attr_cb, &c);
}

NATIVE(mo_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor MutationObserver exige 'new'");
    if (!js_iscallable(ARG(0))) return js_throw_err(E_TYPE, "MutationObserver: callback esperado");
    jsobj *o = js_newobj_cls(CL_HOST, J.realm->p_mutobs);
    o->u.host.page = js_cur_page();
    jsv s[M_NSLOTS] = { ARG(0), jv_obj(js_newarr(0)), jv_obj(js_newarr(0)), jv_obj(js_newarr(0)) };
    o->u.host.extra = jv_obj(js_arr_from(s, M_NSLOTS));
    return jv_obj(o);
}

static bool opt(jsv o, const char *k, int *present) {
    jsv v = js_getn(o, k);
    *present = v != JS_UNDEF;
    return js_truthy(v);
}

/* magic: 0 observe 1 disconnect 2 takeRecords */
NATIVE(mo_method) {
    UNUSED_ARGS;
    jsobj *x = mo_x(thisv);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal: não é um MutationObserver");
    jsobj *t = jv_o(js_arr_get(x, M_TARGETS));
    switch (self->u.nat.magic) {
    case 0: {
        node_t *n = node_of(ARG(0));
        if (!n) return js_throw_err(E_TYPE, "observe: o alvo precisa ser um nó");
        jsv o = jv_isobj(ARG(1)) ? ARG(1) : jv_obj(js_newobj(J.realm->p_object));
        int pc, pa, pd, pao, pdo, pf, ps;
        bool child = opt(o, "childList", &pc), attr = opt(o, "attributes", &pa), data = opt(o, "characterData", &pd);
        bool aold = opt(o, "attributeOldValue", &pao), dold = opt(o, "characterDataOldValue", &pdo), sub = opt(o, "subtree", &ps);
        jsv flt = js_getn(o, "attributeFilter");
        if (js_pending()) return JS_EXC;
        pf = flt != JS_UNDEF;
        if ((aold || pf) && !pa) attr = true;
        if (dold && !pd) data = true;
        if (!child && !attr && !data) return js_throw_err(E_TYPE, "observe: escolha childList, attributes ou characterData");
        if (pa && !attr && (aold || pf)) return js_throw_err(E_TYPE, "observe: attributeOldValue/attributeFilter exigem attributes");
        if (pd && !data && dold) return js_throw_err(E_TYPE, "observe: characterDataOldValue exige characterData");
        jsobj *opts = js_newobj(J.realm->p_object);
        int bits = (child ? O_CHILD : 0) | (attr ? O_ATTR : 0) | (data ? O_DATA : 0) | (sub ? O_SUBTREE : 0) | (aold ? O_ATTROLD : 0) | (dold ? O_DATAOLD : 0);
        js_defn(opts, "\x01" "o", jv_int(bits), 0);
        if (pf) {
            jsv from = js_getn(jv_obj(J.realm->c_array), "from");
            jsv arr = js_call(from, jv_obj(J.realm->c_array), 1, &flt);
            if (arr == JS_EXC) return JS_EXC;
            js_defn(opts, "attributeFilter", arr, 0);
        }
        for (u32 k = 0; k + 1 < t->alen; k += 2) if (t->arr[k] == ARG(0)) { t->arr[k + 1] = jv_obj(opts); return JS_UNDEF; }
        js_arr_push(t, ARG(0));
        js_arr_push(t, jv_obj(opts));
        mo_live++;
        jsobj *reg = registry("\x01" "mutacoes");
        bool in = false;
        for (u32 i = 0; i < reg->alen; i++) if (reg->arr[i] == thisv) in = true;
        if (!in) js_arr_push(reg, thisv);
        return JS_UNDEF;
    }
    case 1:
        mo_live -= (int)(t->alen / 2);
        if (mo_live < 0) mo_live = 0;
        js_arr_set(x, M_TARGETS, jv_obj(js_newarr(0)));
        js_arr_set(x, M_RECORDS, jv_obj(js_newarr(0)));
        return JS_UNDEF;
    case 2: {
        jsv r = js_arr_get(x, M_RECORDS);
        js_arr_set(x, M_RECORDS, jv_obj(js_newarr(0)));
        return r;
    }
    }
    return JS_UNDEF;
}

/* ============================================================ ResizeObserver e IntersectionObserver */
/* extra: [callback, alvos: [nó, estado]..., opções] */
enum { R_CB, R_TARGETS, R_OPTS, R_NSLOTS };

static jsobj *ro_x(jsv v, jsobj *proto) {
    if (!jv_iscls(v, CL_HOST) || !has_proto(jv_o(v), proto) || !jv_isobj(jv_o(v)->u.host.extra)) return NULL;
    return jv_o(jv_o(v)->u.host.extra);
}

static jsobj *rect_obj(float x, float y, float w, float h) {
    jsobj *r = js_newobj(J.realm->p_domrect ? J.realm->p_domrect : J.realm->p_object);
    js_defn(r, "x", jv_num(x), 0); js_defn(r, "y", jv_num(y), 0);
    js_defn(r, "width", jv_num(w), 0); js_defn(r, "height", jv_num(h), 0);
    js_defn(r, "top", jv_num(h < 0 ? y + h : y), 0); js_defn(r, "left", jv_num(w < 0 ? x + w : x), 0);
    js_defn(r, "bottom", jv_num(h < 0 ? y : y + h), 0); js_defn(r, "right", jv_num(w < 0 ? x : x + w), 0);
    return r;
}
static jsobj *size_list(float inl, float blk) {
    jsobj *s = js_newobj(J.realm->p_object);
    js_defn(s, "inlineSize", jv_num(inl), 0);
    js_defn(s, "blockSize", jv_num(blk), 0);
    return js_arr_from((jsv[]){ jv_obj(s) }, 1);
}

static jsv obs_ctor(jsv thisv, int argc, jsv *argv, int flags, jsobj *proto, const char *name, const char *reg) {
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor %s exige 'new'", name);
    if (!js_iscallable(ARG(0))) return js_throw_err(E_TYPE, "%s: callback esperado", name);
    jsobj *o = js_newobj_cls(CL_HOST, proto);
    o->u.host.page = js_cur_page();
    jsv s[R_NSLOTS] = { ARG(0), jv_obj(js_newarr(0)), ARG(1) };
    o->u.host.extra = jv_obj(js_arr_from(s, R_NSLOTS));
    js_arr_push(registry(reg), jv_obj(o));
    return jv_obj(o);
}

/* opções do IntersectionObserver: root, rootMargin (px ou %), threshold */
static bool io_parse(jsobj *o, jsv init) {
    jsv root = JS_NULL, thr = jv_int(0);
    char margin[64] = "0px 0px 0px 0px";
    if (jv_isobj(init)) {
        jsv r = js_getn(init, "root"), m = js_getn(init, "rootMargin"), t = js_getn(init, "threshold");
        if (js_pending()) return false;
        if (!jv_isnullish(r)) { if (!node_of(r)) { js_throw_err(E_TYPE, "IntersectionObserver: root precisa ser um elemento ou documento"); return false; } root = r; }
        if (m != JS_UNDEF) { jsstr *s = js_tostr(m); if (!s) return false; snprintf(margin, sizeof margin, "%s", s->d); }
        if (t != JS_UNDEF) thr = t;
    }
    /* margens: 1 a 4 valores em px ou % */
    float mv[4] = { 0, 0, 0, 0 };
    bool pct[4] = { false, false, false, false };
    int nm = 0;
    const char *p = margin;
    while (*p && nm < 4) {
        while (*p == ' ') p++;
        if (!*p) break;
        float v;
        if (!rs_parse_f(&p, &v)) { js_throw_dom("SyntaxError", "rootMargin inválido"); return false; }
        if (*p == '%') { pct[nm] = true; p++; }
        else if (!strncmp(p, "px", 2)) p += 2;
        else if (!(v == 0 && (*p == ' ' || !*p))) { js_throw_dom("SyntaxError", "rootMargin precisa de px ou %"); return false; }
        mv[nm++] = v;
    }
    if (!nm) nm = 1;
    static const int map[4][4] = { { 0, 0, 0, 0 }, { 0, 1, 0, 1 }, { 0, 1, 2, 1 }, { 0, 1, 2, 3 } };
    jsobj *ms = js_newarr(0);
    for (int i = 0; i < 4; i++) { int s = map[nm - 1][i]; js_arr_push(ms, jv_num(mv[s])); js_arr_push(ms, jv_bool(pct[s])); }
    jsobj *th = js_newarr(0);
    if (js_isarray(thr)) {
        u32 n = js_lengthof(thr);
        for (u32 i = 0; i < n; i++) js_arr_push(th, js_getidx(thr, i));
        if (!n) js_arr_push(th, jv_int(0));
    } else js_arr_push(th, thr);
    for (u32 i = 0; i < th->alen; i++) {
        double d = js_tonum(th->arr[i]);
        if (js_pending()) return false;
        if (!(d >= 0 && d <= 1)) { js_throw_err(E_RANGE, "threshold fora de [0, 1]"); return false; }
        th->arr[i] = jv_num(d);
    }
    /* ordenados */
    for (u32 i = 1; i < th->alen; i++) { jsv v = th->arr[i]; u32 j = i; while (j > 0 && jv_n(th->arr[j - 1]) > jv_n(v)) { th->arr[j] = th->arr[j - 1]; j--; } th->arr[j] = v; }
    jsobj *opts = js_newobj(J.realm->p_object);
    js_defn(opts, "root", root, 0);
    js_defn(opts, "margin", jv_obj(ms), 0);
    js_defn(opts, "thresholds", jv_obj(th), 0);
    char canon[96] = "";
    for (int i = 0; i < 4; i++) {
        char one[24];
        int s = map[nm - 1][i];
        snprintf(one, sizeof one, "%s%g%s", i ? " " : "", (double)mv[s], pct[s] ? "%" : "px");
        usize cl = strlen(canon);
        snprintf(canon + cl, sizeof canon - cl, "%s", one);
    }
    js_defn(opts, "rootMargin", jv_str(js_strz(canon)), 0);
    js_arr_set(jv_o(o->u.host.extra), R_OPTS, jv_obj(opts));
    return true;
}

NATIVE(ro_ctor) { UNUSED_ARGS; return obs_ctor(thisv, argc, argv, flags, J.realm->p_resizeobs, "ResizeObserver", "\x01" "tamanhos"); }
NATIVE(io_ctor) {
    UNUSED_ARGS;
    jsv o = obs_ctor(thisv, argc, argv, flags, J.realm->p_interobs, "IntersectionObserver", "\x01" "intersecoes");
    if (o == JS_EXC) return JS_EXC;
    if (!io_parse(jv_o(o), ARG(1))) return JS_EXC;
    return o;
}

/* magic: 0 observe 1 unobserve 2 disconnect 3 takeRecords; data = proto */
NATIVE(ro_method) {
    UNUSED_ARGS;
    bool io = self->u.nat.data == JS_TRUE;
    jsobj *x = ro_x(thisv, io ? J.realm->p_interobs : J.realm->p_resizeobs);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal: não é um %s", io ? "IntersectionObserver" : "ResizeObserver");
    jsobj *t = jv_o(js_arr_get(x, R_TARGETS));
    switch (self->u.nat.magic) {
    case 0: {
        node_t *n = node_of(ARG(0));
        if (!n || n->type != N_ELEM) return js_throw_err(E_TYPE, "observe: o alvo precisa ser um elemento");
        for (u32 k = 0; k + 1 < t->alen; k += 2) if (t->arr[k] == ARG(0)) return JS_UNDEF;
        jsobj *st = js_newobj(J.realm->p_object);
        js_defn(st, "box", jv_str(js_strz("content-box")), 0);
        if (!io && jv_isobj(ARG(1))) {
            jsv b = js_getn(ARG(1), "box");
            if (js_pending()) return JS_EXC;
            if (b != JS_UNDEF) {
                jsstr *bs = js_tostr(b);
                if (!bs) return JS_EXC;
                if (strcmp(bs->d, "content-box") && strcmp(bs->d, "border-box") && strcmp(bs->d, "device-pixel-content-box")) return js_throw_err(E_TYPE, "box inválido: %s", bs->d);
                js_defn(st, "box", jv_str(bs), 0);
            }
        }
        js_defn(st, "w", jv_num(-1), 0);   /* nunca entregue: o primeiro tick sempre avisa */
        js_defn(st, "h", jv_num(-1), 0);
        js_defn(st, "idx", jv_int(-1), 0);
        js_arr_push(t, ARG(0));
        js_arr_push(t, jv_obj(st));
        return JS_UNDEF;
    }
    case 1: {
        jsobj *out = js_newarr(0);
        for (u32 k = 0; k + 1 < t->alen; k += 2) if (t->arr[k] != ARG(0)) { js_arr_push(out, t->arr[k]); js_arr_push(out, t->arr[k + 1]); }
        js_arr_set(x, R_TARGETS, jv_obj(out));
        return JS_UNDEF;
    }
    case 2: js_arr_set(x, R_TARGETS, jv_obj(js_newarr(0))); return JS_UNDEF;
    case 3: return jv_obj(js_newarr(0));
    }
    return JS_UNDEF;
}

NATIVE(io_get) {
    UNUSED_ARGS;
    jsobj *x = ro_x(thisv, J.realm->p_interobs);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal: não é um IntersectionObserver");
    jsv o = js_arr_get(x, R_OPTS);
    static const char *k[] = { "root", "rootMargin", "thresholds" };
    jsv v = js_getn(o, k[self->u.nat.magic]);
    if (self->u.nat.magic == 2) return jv_obj(js_arr_from(jv_o(v)->arr, (int)jv_o(v)->alen));
    return v;
}

/* depois do layout, no tick: tamanhos que mudaram e interseções que cruzaram um limiar */
void web_obs_tick(js_page_t *p) {
    jsobj *rr = registry("\x01" "tamanhos"), *ir = registry("\x01" "intersecoes");
    if (!rr->alen && !ir->alen) return;
    jsobj *rlist = js_arr_from(rr->arr, (int)rr->alen);
    for (u32 i = 0; i < rlist->alen; i++) {
        jsobj *x = ro_x(rlist->arr[i], J.realm->p_resizeobs);
        if (!x) continue;
        jsobj *t = jv_o(js_arr_get(x, R_TARGETS)), *entries = js_newarr(0);
        for (u32 k = 0; k + 1 < t->alen; k += 2) {
            node_t *n = node_of(t->arr[k]);
            float g[10];
            if (!js_node_geom(p, n, g)) { for (int z = 0; z < 10; z++) g[z] = 0; }
            jsv st = t->arr[k + 1];
            bool border = !strcmp(jv_s(js_getn(st, "box"))->d, "border-box");
            float w = border ? g[2] : g[6], h = border ? g[3] : g[7];
            if (jv_n(js_getn(st, "w")) == w && jv_n(js_getn(st, "h")) == h) continue;
            js_defn(jv_o(st), "w", jv_num(w), 0);
            js_defn(jv_o(st), "h", jv_num(h), 0);
            jsobj *e = js_newobj(J.realm->p_object);
            js_defn(e, "target", t->arr[k], 0);
            js_defn(e, "contentRect", jv_obj(rect_obj(g[8], g[9], g[6], g[7])), 0);   /* origem = padding */
            js_defn(e, "contentBoxSize", jv_obj(size_list(g[6], g[7])), 0);
            js_defn(e, "borderBoxSize", jv_obj(size_list(g[2], g[3])), 0);
            js_defn(e, "devicePixelContentBoxSize", jv_obj(size_list(g[6], g[7])), 0);
            js_arr_push(entries, jv_obj(e));
        }
        if (!entries->alen) continue;
        jsv a[2] = { jv_obj(entries), rlist->arr[i] };
        if (js_call(js_arr_get(x, R_CB), rlist->arr[i], 2, a) == JS_EXC) js_report_exception();
    }
    jsobj *ilist = js_arr_from(ir->arr, (int)ir->alen);
    int vx, vy, vw, vh;
    js_page_viewport_rect(p, &vx, &vy, &vw, &vh);
    for (u32 i = 0; i < ilist->alen; i++) {
        jsobj *x = ro_x(ilist->arr[i], J.realm->p_interobs);
        if (!x) continue;
        jsv opts = js_arr_get(x, R_OPTS);
        jsv root = js_getn(opts, "root");
        jsobj *ms = jv_o(js_getn(opts, "margin")), *th = jv_o(js_getn(opts, "thresholds"));
        /* raiz em coordenadas do documento */
        float rx = (float)vx, ry = (float)vy, rw = (float)vw, rh = (float)vh;
        node_t *rn = node_of(root);
        if (rn && rn->type == N_ELEM) { float g[10]; if (js_node_geom(p, rn, g)) { rx = g[4]; ry = g[5]; rw = g[6]; rh = g[7]; } else rw = rh = 0; }
        float mm[4];
        for (int s = 0; s < 4; s++) { float v = (float)jv_n(ms->arr[2 * s]); mm[s] = js_truthy(ms->arr[2 * s + 1]) ? v * (s % 2 ? rw : rh) / 100 : v; }
        float R0x = rx - mm[3], R0y = ry - mm[0], R1x = rx + rw + mm[1], R1y = ry + rh + mm[2];
        jsobj *t = jv_o(js_arr_get(x, R_TARGETS)), *entries = js_newarr(0);
        for (u32 k = 0; k + 1 < t->alen; k += 2) {
            node_t *n = node_of(t->arr[k]);
            float g[10];
            bool laid = js_node_geom(p, n, g);
            if (rn && rn->type == N_ELEM) { bool inside = false; for (node_t *a = n->parent; a; a = a->parent) if (a == rn) inside = true; if (!inside) laid = false; }
            float tx = g[0], ty = g[1], tw = laid ? g[2] : 0, th2 = laid ? g[3] : 0;
            float ix0 = tx, iy0 = ty, ix1 = tx + tw, iy1 = ty + th2;
            /* ancestrais com overflow diferente de visible recortam na caixa de padding, até a raiz */
            bool clipped_out = false;
            for (node_t *a = n->parent; laid && a && a != rn && a->type == N_ELEM; a = a->parent) {
                if (!a->st || !a->st->clip_overflow) continue;
                float cg[10];
                if (!js_node_geom(p, a, cg)) continue;
                /* a caixa de conteúdo tem o tamanho declarado (o layout cresce com o que transborda) */
                float cw = a->st->width.u == U_PX ? a->st->width.v : cg[6], ch = a->st->height.u == U_PX ? a->st->height.v : cg[7];
                float cx0 = cg[4] - cg[8], cy0 = cg[5] - cg[9], cx1 = cg[4] + cw + cg[8], cy1 = cg[5] + ch + cg[9];
                ix0 = MAX(ix0, cx0); iy0 = MAX(iy0, cy0); ix1 = MIN(ix1, cx1); iy1 = MIN(iy1, cy1);
                if (ix1 < ix0 || iy1 < iy0) { clipped_out = true; break; }
            }
            ix0 = MAX(ix0, R0x); iy0 = MAX(iy0, R0y); ix1 = MIN(ix1, R1x); iy1 = MIN(iy1, R1y);
            bool isect = laid && !clipped_out && ix1 >= ix0 && iy1 >= iy0;
            float area = tw * th2, iarea = isect ? (ix1 - ix0) * (iy1 - iy0) : 0;
            float ratio = isect ? (area > 0 ? iarea / area : 1) : 0;
            /* índice do limiar: o maior limiar ≤ razão (−1 = fora) */
            int idx = -1;
            for (u32 q = 0; q < th->alen; q++) if (isect && ratio >= (float)jv_n(th->arr[q])) idx = (int)q;
            if (isect && idx < 0) idx = 0;
            jsv st = t->arr[k + 1];
            int prev = js_toint32(js_getn(st, "idx"));
            bool prev_in = js_truthy(js_getn(st, "in"));
            bool first = !js_ownprop(jv_o(st), js_intern("in", -1));
            if (!first && prev == idx && prev_in == isect) continue;
            js_defn(jv_o(st), "idx", jv_int(idx), 0);
            js_defn(jv_o(st), "in", jv_bool(isect), 0);
            jsobj *e = js_newobj(J.realm->p_object);
            js_defn(e, "target", t->arr[k], 0);
            js_defn(e, "time", jv_num(js_date_now()), 0);
            /* retângulos relativos à viewport */
            js_defn(e, "boundingClientRect", jv_obj(rect_obj(tx - vx, ty - vy, tw, th2)), 0);
            js_defn(e, "intersectionRect", jv_obj(isect ? rect_obj(ix0 - vx, iy0 - vy, ix1 - ix0, iy1 - iy0) : rect_obj(0, 0, 0, 0)), 0);
            js_defn(e, "rootBounds", jv_obj(rect_obj(R0x - vx, R0y - vy, R1x - R0x, R1y - R0y)), 0);
            js_defn(e, "isIntersecting", jv_bool(isect), 0);
            js_defn(e, "intersectionRatio", jv_num(ratio), 0);
            js_defn(e, "isVisible", JS_FALSE, 0);
            js_arr_push(entries, jv_obj(e));
        }
        if (!entries->alen) continue;
        jsv a[2] = { jv_obj(entries), ilist->arr[i] };
        if (js_call(js_arr_get(x, R_CB), ilist->arr[i], 2, a) == JS_EXC) js_report_exception();
    }
}

NATIVE(illegal) { UNUSED_ARGS; return js_throw_err(E_TYPE, "Construtor ilegal"); }

void js_obs_init(jsrealm *r) {
    jsobj *g = r->global, *op = r->p_object;
    r->p_mutrec = js_newobj(op);
    js_defn(g, "MutationRecord", jv_obj(js_newctor(illegal, "MutationRecord", 0, r->p_mutrec)), PF_NOENUM);
    r->p_mutobs = js_newobj(op);
    js_defn(g, "MutationObserver", jv_obj(js_newctor(mo_ctor, "MutationObserver", 1, r->p_mutobs)), PF_NOENUM);
    js_defn(g, "WebKitMutationObserver", js_getn(jv_obj(g), "MutationObserver"), PF_NOENUM);
    js_defnative(r->p_mutobs, "observe", mo_method, 1, 0);
    js_defnative(r->p_mutobs, "disconnect", mo_method, 0, 1);
    js_defnative(r->p_mutobs, "takeRecords", mo_method, 0, 2);
    r->p_domrect = js_newobj(op);
    js_defn(g, "DOMRectReadOnly", jv_obj(js_newctor(illegal, "DOMRectReadOnly", 0, r->p_domrect)), PF_NOENUM);
    r->p_resizeobs = js_newobj(op);
    js_defn(g, "ResizeObserver", jv_obj(js_newctor(ro_ctor, "ResizeObserver", 1, r->p_resizeobs)), PF_NOENUM);
    r->p_interobs = js_newobj(op);
    js_defn(g, "IntersectionObserver", jv_obj(js_newctor(io_ctor, "IntersectionObserver", 1, r->p_interobs)), PF_NOENUM);
    static const char *mn[] = { "observe", "unobserve", "disconnect", "takeRecords" };
    for (int i = 0; i < 4; i++) {
        jsobj *f = js_defnative(r->p_resizeobs, mn[i], ro_method, i < 2 ? 1 : 0, i);
        f->u.nat.data = JS_FALSE;
        jsobj *fi = js_defnative(r->p_interobs, mn[i], ro_method, i < 2 ? 1 : 0, i);
        fi->u.nat.data = JS_TRUE;
    }
    static const char *ig[] = { "root", "rootMargin", "thresholds" };
    for (int i = 0; i < 3; i++) js_defgetter(r->p_interobs, ig[i], io_get, i);
}
