/* mOS — APIs de plataforma do motor web: structuredClone (algoritmo de clone estruturado do HTML, com ciclos, Map,
 * Set, Date, RegExp, ArrayBuffer e views, Blob/File, ImageData e erros), MessageChannel/MessagePort e
 * crypto.getRandomValues/randomUUID. Implementação própria. */
#include "js_int.h"
#include "web.h"
#include "crypto.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

static bool has_proto(jsobj *o, jsobj *p) {
    for (int i = 0; o && p && i < 32; i++, o = o->proto) if (o->proto == p) return true;
    return false;
}

/* ============================================================ clone estruturado */
/* memória de objetos já clonados em vetores JS (o coletor os vê pela pilha) */
typedef struct { jsobj *src, *dst; } memo_t;

static jsv clone_rec(jsv v, memo_t *m, int depth);

static jsv clone_err(const char *what) {
    char b[160];
    snprintf(b, sizeof b, "%s não pode ser clonado", what);
    return js_throw_dom("DataCloneError", b);
}

static jsv memo_get(memo_t *m, jsobj *o) {
    for (u32 i = 0; i < m->src->alen; i++) if (m->src->arr[i] == jv_obj(o)) return m->dst->arr[i];
    return JS_UNDEF;
}
static void memo_put(memo_t *m, jsobj *o, jsv c) { js_arr_push(m->src, jv_obj(o)); js_arr_push(m->dst, c); }

static jsobj *clone_buffer(memo_t *m, jsobj *b) {
    jsv seen = memo_get(m, b);
    if (seen != JS_UNDEF) return jv_o(seen);
    if (b->u.buf.detached) { clone_err("ArrayBuffer destacado"); return NULL; }
    jsobj *n = js_abuf_new(b->u.buf.len);
    if (!n) return NULL;
    if (b->u.buf.len) memcpy(n->u.buf.data, b->u.buf.data, b->u.buf.len);
    memo_put(m, b, jv_obj(n));
    return n;
}

/* propriedades próprias enumeráveis de string, na ordem */
static bool clone_props(jsobj *src, jsobj *dst, memo_t *m, int depth, bool skip_index) {
    jsobj *keys = js_keylist(src, true, false);
    for (u32 i = 0; keys && i < keys->alen; i++) {
        jsv k = keys->arr[i];
        if (!jv_isstr(k) || jv_s(k)->sym) continue;
        u32 idx;
        if (skip_index && js_str_isindex(jv_s(k), &idx)) continue;
        jsv v = js_getk(jv_obj(src), jv_s(k));
        if (js_pending()) return false;
        jsv c = clone_rec(v, m, depth + 1);
        if (c == JS_EXC) return false;
        js_defprop(dst, jv_s(k), c, 0);
    }
    return true;
}

static jsv clone_rec(jsv v, memo_t *m, int depth) {
    if (!jv_isobj(v)) {
        if (jv_issym(v)) return clone_err("Symbol");
        return v;   /* primitivos (inclusive BigInt e strings) são imutáveis */
    }
    if (depth > 2000) return js_throw_err(E_RANGE, "structuredClone: aninhamento profundo demais");
    jsobj *o = jv_o(v);
    jsv seen = memo_get(m, o);
    if (seen != JS_UNDEF) return seen;
    jsrealm *r = J.realm;
    switch (o->cls) {
    case CL_BOOLEAN: case CL_NUMBER: case CL_STRING: case CL_BIGINT: {
        jsv c = js_toobj(o->u.prim);
        if (c == JS_EXC) return JS_EXC;
        memo_put(m, o, c);
        return c;
    }
    case CL_DATE: {
        jsobj *d = js_newobj_cls(CL_DATE, r->p_date);
        d->u.date = o->u.date;
        memo_put(m, o, jv_obj(d));
        return jv_obj(d);
    }
    case CL_REGEXP: {
        jsv a[2] = { jv_str(o->u.re.src), jv_str(o->u.re.flags) };
        jsv c = js_construct(jv_obj(r->c_regexp), 2, a, jv_obj(r->c_regexp));
        if (c == JS_EXC) return JS_EXC;
        memo_put(m, o, c);
        return c;
    }
    case CL_ARRAYBUFFER: { jsobj *n = clone_buffer(m, o); return n ? jv_obj(n) : JS_EXC; }
    case CL_TYPED: case CL_DATAVIEW: {
        jsobj *nb = clone_buffer(m, o->u.ta.buffer);
        if (!nb) return JS_EXC;
        jsobj *c;
        if (o->cls == CL_TYPED) c = js_typed_new(o->u.ta.kind, nb, o->u.ta.off, o->u.ta.len);
        else {
            c = js_newobj_cls(CL_DATAVIEW, r->p_dview);
            c->u.ta.buffer = nb; c->u.ta.off = o->u.ta.off; c->u.ta.len = o->u.ta.len;
        }
        c->u.ta.tracking = o->u.ta.tracking;
        memo_put(m, o, jv_obj(c));
        return jv_obj(c);
    }
    case CL_MAP: case CL_SET: {
        bool map = o->cls == CL_MAP;
        jsv ctor = js_getn(jv_obj(r->global), map ? "Map" : "Set");
        jsv c = js_construct(ctor, 0, NULL, ctor);
        if (c == JS_EXC) return JS_EXC;
        memo_put(m, o, c);
        /* entradas atuais por Array.from (ordem de inserção) */
        jsv from = js_getn(jv_obj(r->c_array), "from");
        jsv items = js_call(from, jv_obj(r->c_array), 1, &v);
        if (items == JS_EXC) return JS_EXC;
        u32 n = js_lengthof(items);
        for (u32 i = 0; i < n; i++) {
            jsv e = js_getidx(items, i);
            if (map) {
                jsv k = clone_rec(js_getidx(e, 0), m, depth + 1);
                if (k == JS_EXC) return JS_EXC;
                jsv val = clone_rec(js_getidx(e, 1), m, depth + 1);
                if (val == JS_EXC) return JS_EXC;
                jsv a[2] = { k, val };
                if (js_invoke(c, js_intern("set", -1), 2, a) == JS_EXC) return JS_EXC;
            } else {
                jsv k = clone_rec(e, m, depth + 1);
                if (k == JS_EXC) return JS_EXC;
                if (js_invoke(c, js_intern("add", -1), 1, &k) == JS_EXC) return JS_EXC;
            }
        }
        return c;
    }
    case CL_ERROR: {
        jsv name = js_getn(v, "name"), msg = js_getn(v, "message"), stack = js_getn(v, "stack");
        if (js_pending()) return JS_EXC;
        int type = E_ERROR;
        if (jv_isstr(name)) {
            static const char *nm[] = { "Error", "TypeError", "RangeError", "SyntaxError", "ReferenceError", "EvalError", "URIError" };
            for (int i = 0; i < E_COUNT && i < 7; i++) if (!strcmp(jv_s(name)->d, nm[i])) type = i;
        }
        jsstr *ms = jv_isstr(msg) ? jv_s(msg) : NULL;
        jsobj *e = js_make_err(type, ms ? ms : A[A_empty]);
        if (!ms) js_delprop(e, js_intern("message", -1));
        if (jv_isstr(stack)) js_defn(e, "stack", stack, PF_NOENUM);
        jsv cause = js_getn(v, "cause");
        if (js_pending()) return JS_EXC;
        memo_put(m, o, jv_obj(e));
        if (js_ownprop(o, js_intern("cause", -1))) {
            jsv cc = clone_rec(cause, m, depth + 1);
            if (cc == JS_EXC) return JS_EXC;
            js_defn(e, "cause", cc, PF_NOENUM);
        }
        return jv_obj(e);
    }
    case CL_ARRAY: {
        /* comprimento e só as propriedades próprias: buracos continuam buracos */
        jsobj *a = js_newarr(0);
        memo_put(m, o, jv_obj(a));
        if (js_setn(jv_obj(a), "length", jv_num((double)js_lengthof(v))) < 0) return JS_EXC;
        if (!clone_props(o, a, m, depth, false)) return JS_EXC;
        return jv_obj(a);
    }
    case CL_OBJECT: case CL_ARGUMENTS: {
        /* ImageData é um objeto comum com protótipo próprio */
        if (r->p_imgdata && has_proto(o, r->p_imgdata)) {
            jsv d = js_getn(v, "data"), w = js_getn(v, "width");
            jsv dc = clone_rec(d, m, depth + 1);
            if (dc == JS_EXC) return JS_EXC;
            jsv ctor = js_getn(jv_obj(r->global), "ImageData");
            jsv a[2] = { dc, w };
            jsv c = js_construct(ctor, 2, a, ctor);
            if (c != JS_EXC) memo_put(m, o, c);
            return c;
        }
        /* objetos de classes do usuário viram objetos comuns (só as propriedades próprias), como no navegador */
        jsobj *c = js_newobj(r->p_object);
        memo_put(m, o, jv_obj(c));
        if (!clone_props(o, c, m, depth, false)) return JS_EXC;
        return jv_obj(c);
    }
    case CL_HOST: {
        const u8 *d;
        u32 n;
        const char *type;
        if (js_blob_bytes(v, &d, &n, &type)) {
            /* Blob e File: os bytes são imutáveis, então o clone os copia uma vez */
            jsobj *b = js_blob_new(d, n, type);
            if (!b) return JS_EXC;
            jsv nm = js_getn(v, "name"), lm = js_getn(v, "lastModified");
            if (js_pending()) return JS_EXC;
            if (jv_isstr(nm) && r->p_file && has_proto(o, r->p_file)) {
                b->proto = r->p_file;
                jsobj *x = jv_o(b->u.host.extra);
                js_arr_push(x, nm);
                js_arr_push(x, lm);
            }
            memo_put(m, o, jv_obj(b));
            return jv_obj(b);
        }
        if (r->p_cryptokey && has_proto(o, r->p_cryptokey)) {
            jsobj *c = js_newobj_cls(CL_HOST, r->p_cryptokey);
            c->u.host.page = o->u.host.page;
            c->u.host.extra = o->u.host.extra;   /* o material da chave é imutável */
            memo_put(m, o, jv_obj(c));
            return jv_obj(c);
        }
        break;
    }
    default: break;
    }
    if (js_iscallable(v)) return clone_err("Uma função");
    if (o->cls == CL_NODE) return clone_err("Um nó do DOM");
    if (o->cls == CL_PROMISE) return clone_err("Uma Promise");
    if (o->cls == CL_SYMBOL) return clone_err("Symbol");
    return clone_err("Este objeto");
}

jsv js_structured_clone(jsv v) {
    memo_t m = { js_newarr(0), js_newarr(0) };
    return clone_rec(v, &m, 0);
}

NATIVE(w_structuredclone) {
    UNUSED_ARGS;
    if (argc < 1) return js_throw_err(E_TYPE, "structuredClone: é necessário 1 argumento");
    if (jv_isobj(ARG(1))) {
        jsv t = js_getn(ARG(1), "transfer");
        if (js_pending()) return JS_EXC;
        if (t != JS_UNDEF && js_lengthof(t)) {
            /* transferir: copia e depois destaca os ArrayBuffer listados */
            jsv c = js_structured_clone(ARG(0));
            if (c == JS_EXC) return JS_EXC;
            u32 n = js_lengthof(t);
            for (u32 i = 0; i < n; i++) {
                jsv b = js_getidx(t, i);
                if (!jv_iscls(b, CL_ARRAYBUFFER)) return js_throw_dom("DataCloneError", "structuredClone: só ArrayBuffer pode ser transferido");
                jsobj *bo = jv_o(b);
                if (bo->u.buf.dealloc) bo->u.buf.dealloc(bo->u.buf.data, bo->u.buf.dctx); else kfree(bo->u.buf.data);
                bo->u.buf.data = NULL;
                bo->u.buf.len = 0;
                bo->u.buf.detached = true;
                bo->u.buf.dealloc = NULL;
            }
            return c;
        }
    }
    return js_structured_clone(ARG(0));
}

/* ============================================================ MessageChannel / MessagePort */
/* porta: extra = [par, aberta?, iniciada?, fila de mensagens, ouvintes] */
enum { MP_PEER, MP_OPEN, MP_STARTED, MP_QUEUE, MP_LISTEN, MP_NSLOTS };

static jsobj *port_x(jsv v) {
    if (!jv_iscls(v, CL_HOST) || !has_proto(jv_o(v), J.realm->p_msgport) || !jv_isobj(jv_o(v)->u.host.extra)) return NULL;
    return jv_o(jv_o(v)->u.host.extra);
}

static jsobj *port_new(void) {
    jsobj *o = js_newobj_cls(CL_HOST, J.realm->p_msgport);
    o->u.host.page = js_cur_page();
    jsv s[MP_NSLOTS] = { JS_NULL, JS_TRUE, JS_FALSE, jv_obj(js_newarr(0)), jv_obj(js_newarr(0)) };
    o->u.host.extra = jv_obj(js_arr_from(s, MP_NSLOTS));
    return o;
}

static void port_fire(jsobj *port, jsobj *x, jsv data) {
    jsobj *ev = js_newobj(J.realm->p_msgevent ? J.realm->p_msgevent : J.realm->p_object);
    js_defn(ev, "type", jv_str(js_intern("message", -1)), 0);
    js_defn(ev, "data", data, 0);
    js_defn(ev, "origin", jv_str(A[A_empty]), 0);
    js_defn(ev, "lastEventId", jv_str(A[A_empty]), 0);
    js_defn(ev, "source", JS_NULL, 0);
    js_defn(ev, "ports", jv_obj(js_newarr(0)), 0);
    js_defn(ev, "target", jv_obj(port), 0);
    js_defn(ev, "currentTarget", jv_obj(port), 0);
    js_defn(ev, "bubbles", JS_FALSE, 0);
    js_defn(ev, "isTrusted", JS_TRUE, 0);
    jsv h = js_getn(jv_obj(port), "onmessage");
    jsv a = jv_obj(ev);
    if (js_pending()) js_report_exception();
    else if (js_iscallable(h) && js_call(h, jv_obj(port), 1, &a) == JS_EXC) js_report_exception();
    jsobj *l = js_arr_from(jv_o(js_arr_get(x, MP_LISTEN))->arr, (int)jv_o(js_arr_get(x, MP_LISTEN))->alen);
    for (u32 i = 0; i + 1 < l->alen; i += 2) {
        if (strcmp(jv_s(l->arr[i])->d, "message")) continue;
        jsv f = l->arr[i + 1];
        jsv rv = js_iscallable(f) ? js_call(f, jv_obj(port), 1, &a) : js_invoke(f, js_intern("handleEvent", -1), 1, &a);
        if (rv == JS_EXC) js_report_exception();
    }
}

/* tarefa: entrega a fila de uma porta iniciada */
NATIVE(port_task) {
    UNUSED_ARGS;
    jsobj *port = jv_o(self->u.nat.data), *x = jv_o(port->u.host.extra);
    if (!js_truthy(js_arr_get(x, MP_STARTED)) || !js_truthy(js_arr_get(x, MP_OPEN))) return JS_UNDEF;
    jsobj *q = jv_o(js_arr_get(x, MP_QUEUE));
    if (!q->alen) return JS_UNDEF;
    jsv data = q->arr[0];
    memmove(q->arr, q->arr + 1, sizeof(jsv) * (q->alen - 1));
    js_arr_setlen(q, q->alen - 1);
    data = js_structured_clone(data);   /* clone no reino de quem recebe */
    if (data == JS_EXC) { js_catch(); return JS_UNDEF; }
    port_fire(port, x, data);
    return JS_UNDEF;
}

/* a entrega roda na página dona da porta (pode ser outro reino: worker, SharedWorker) */
static void port_schedule(jsobj *port) {
    jsobj *t = js_newnative(port_task, "mensagem", 0, 0);
    t->u.nat.data = jv_obj(port);
    js_page_add_timer(port->u.host.page ? port->u.host.page : js_cur_page(), jv_obj(t), 0);
}

/* par de portas ligadas: uma no reino atual (devolvida) e outra no reino other (*other_port) */
jsobj *js_port_pair(jsrealm *other, jsobj **other_port) {
    jsobj *a = port_new();
    jsrealm *saved = J.realm;
    J.realm = other;
    jsobj *b = port_new();
    J.realm = saved;
    js_arr_set(jv_o(a->u.host.extra), MP_PEER, jv_obj(b));
    js_arr_set(jv_o(b->u.host.extra), MP_PEER, jv_obj(a));
    *other_port = b;
    return a;
}

static void port_start(jsobj *port, jsobj *x) {
    if (js_truthy(js_arr_get(x, MP_STARTED))) return;
    js_arr_set(x, MP_STARTED, JS_TRUE);
    jsobj *q = jv_o(js_arr_get(x, MP_QUEUE));
    for (u32 i = 0; i < q->alen; i++) port_schedule(port);
}

/* magic: 0 postMessage 1 start 2 close 3 addEventListener 4 removeEventListener */
NATIVE(port_method) {
    UNUSED_ARGS;
    jsobj *x = port_x(thisv);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal: não é um MessagePort");
    switch (self->u.nat.magic) {
    case 0: {
        if (argc < 1) return js_throw_err(E_TYPE, "postMessage: é necessário 1 argumento");
        jsv c = js_structured_clone(ARG(0));
        if (c == JS_EXC) return JS_EXC;
        jsv peer = js_arr_get(x, MP_PEER);
        if (!js_truthy(js_arr_get(x, MP_OPEN)) || !jv_isobj(peer)) return JS_UNDEF;
        jsobj *px = jv_o(jv_o(peer)->u.host.extra);
        if (!js_truthy(js_arr_get(px, MP_OPEN))) return JS_UNDEF;
        js_arr_push(jv_o(js_arr_get(px, MP_QUEUE)), c);
        if (js_truthy(js_arr_get(px, MP_STARTED))) port_schedule(jv_o(peer));
        return JS_UNDEF;
    }
    case 1: port_start(jv_o(thisv), x); return JS_UNDEF;
    case 2: {
        js_arr_set(x, MP_OPEN, JS_FALSE);
        jsv peer = js_arr_get(x, MP_PEER);
        js_arr_set(x, MP_PEER, JS_NULL);
        if (jv_isobj(peer)) js_arr_set(jv_o(jv_o(peer)->u.host.extra), MP_PEER, JS_NULL);
        return JS_UNDEF;
    }
    case 3: case 4: {
        jsstr *t = js_tostr(ARG(0));
        if (!t) return JS_EXC;
        jsv f = ARG(1);
        if (!jv_isobj(f)) return JS_UNDEF;
        jsobj *l = jv_o(js_arr_get(x, MP_LISTEN));
        for (u32 i = 0; i + 1 < l->alen; i += 2) {
            if (!js_str_eq(jv_s(l->arr[i]), t) || l->arr[i + 1] != f) continue;
            if (self->u.nat.magic == 3) return JS_UNDEF;
            memmove(l->arr + i, l->arr + i + 2, sizeof(jsv) * (l->alen - i - 2));
            js_arr_setlen(l, l->alen - 2);
            return JS_UNDEF;
        }
        if (self->u.nat.magic == 3) { js_arr_push(l, jv_str(t)); js_arr_push(l, f); }
        return JS_UNDEF;
    }
    }
    return JS_UNDEF;
}

/* onmessage: guardar o handler também inicia a porta */
NATIVE(port_onmessage) {
    UNUSED_ARGS;
    jsobj *x = port_x(thisv);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal: não é um MessagePort");
    jsstr *k = js_intern("\x01" "onmessage", -1);
    if (self->u.nat.magic == 0) { jsprop *p = js_ownprop(jv_o(thisv), k); return p ? p->v : JS_NULL; }
    js_defprop(jv_o(thisv), k, js_iscallable(ARG(0)) || jv_isobj(ARG(0)) ? ARG(0) : JS_NULL, PF_NOENUM);
    if (js_iscallable(ARG(0))) port_start(jv_o(thisv), x);
    return JS_UNDEF;
}

NATIVE(channel_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor MessageChannel exige 'new'");
    jsobj *a = port_new(), *b = port_new();
    js_arr_set(jv_o(a->u.host.extra), MP_PEER, jv_obj(b));
    js_arr_set(jv_o(b->u.host.extra), MP_PEER, jv_obj(a));
    jsobj *c = js_newobj(J.realm->p_msgchannel);
    js_defn(c, "port1", jv_obj(a), PF_RO);
    js_defn(c, "port2", jv_obj(b), PF_RO);
    return jv_obj(c);
}

NATIVE(illegal) { UNUSED_ARGS; return js_throw_err(E_TYPE, "Construtor ilegal"); }

/* ============================================================ crypto.getRandomValues / randomUUID */
NATIVE(crypto_grv) {
    UNUSED_ARGS;
    jsv a = ARG(0);
    if (!jv_iscls(a, CL_TYPED)) return js_throw_dom("TypeMismatchError", "getRandomValues: é preciso um typed array inteiro");
    jsobj *t = jv_o(a);
    if (t->u.ta.kind == TA_FLOAT32 || t->u.ta.kind == TA_FLOAT64) return js_throw_dom("TypeMismatchError", "getRandomValues: typed array de ponto flutuante");
    u32 n = js_typed_length(t) * (u32)js_typed_elsize(t->u.ta.kind);
    if (n > 65536) return js_throw_dom("QuotaExceededError", "getRandomValues: mais de 65536 bytes");
    if (n && t->u.ta.buffer && !t->u.ta.buffer->u.buf.detached) crypto_random(t->u.ta.buffer->u.buf.data + t->u.ta.off, n);
    return a;
}

NATIVE(crypto_uuid) {
    UNUSED_ARGS;
    u8 r[16];
    crypto_random(r, sizeof r);
    r[6] = (r[6] & 0x0F) | 0x40;
    r[8] = (r[8] & 0x3F) | 0x80;
    char s[40];
    snprintf(s, sizeof s, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10], r[11], r[12], r[13], r[14], r[15]);
    return jv_str(js_strz(s));
}

/* ============================================================ registro */
void js_plat_init(jsrealm *r) {
    jsobj *g = r->global, *op = r->p_object;
    js_defnative(g, "structuredClone", w_structuredclone, 1, 0);
    /* MessageEvent */
    r->p_msgevent = js_newobj(r->p_event ? r->p_event : op);
    js_defn(g, "MessageEvent", jv_obj(js_newctor(illegal, "MessageEvent", 1, r->p_msgevent)), PF_NOENUM);
    /* MessagePort / MessageChannel */
    jsobj *et = r->p_node ? r->p_node->proto : op;
    r->p_msgport = js_newobj(et);
    js_defn(g, "MessagePort", jv_obj(js_newctor(illegal, "MessagePort", 0, r->p_msgport)), PF_NOENUM);
    js_defprop(r->p_msgport, A[A_sym_toStringTag], jv_str(js_intern("MessagePort", -1)), PF_NOENUM | PF_RO);
    static const char *pm[] = { "postMessage", "start", "close", "addEventListener", "removeEventListener" };
    static const int pa[] = { 1, 0, 0, 2, 2 };
    for (int i = 0; i < 5; i++) js_defnative(r->p_msgport, pm[i], port_method, pa[i], i);
    js_defaccessor(r->p_msgport, js_intern("onmessage", -1), jv_obj(js_newnative(port_onmessage, "onmessage", 0, 0)),
                   jv_obj(js_newnative(port_onmessage, "onmessage", 1, 1)), 0);
    js_defn(r->p_msgport, "onmessageerror", JS_NULL, 0);
    r->p_msgchannel = js_newobj(op);
    js_defn(g, "MessageChannel", jv_obj(js_newctor(channel_ctor, "MessageChannel", 0, r->p_msgchannel)), PF_NOENUM);
    js_defprop(r->p_msgchannel, A[A_sym_toStringTag], jv_str(js_intern("MessageChannel", -1)), PF_NOENUM | PF_RO);
    /* crypto */
    jsobj *cp = js_newobj(op);
    js_defnative(cp, "getRandomValues", crypto_grv, 1, 0);
    js_defnative(cp, "randomUUID", crypto_uuid, 0, 0);
    js_defprop(cp, A[A_sym_toStringTag], jv_str(js_intern("Crypto", -1)), PF_NOENUM | PF_RO);
    jsobj *crypto = js_newobj(cp);
    js_defn(crypto, "subtle", jv_obj(js_subtle_init(r)), PF_RO);
    js_defn(g, "crypto", jv_obj(crypto), PF_NOENUM);
    js_defn(g, "Crypto", jv_obj(js_newctor(illegal, "Crypto", 0, cp)), PF_NOENUM);
}
