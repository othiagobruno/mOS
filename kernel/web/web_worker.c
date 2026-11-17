/* mOS — Web Workers e BroadcastChannel do motor web. Cada Worker é uma página JS própria (reino separado, documento
 * vazio, sem as APIs de DOM) com o escopo DedicatedWorkerGlobalScope: self, postMessage, onmessage, close,
 * importScripts, name, location; temporizadores, fetch, XHR, crypto e o resto da plataforma continuam. Ela roda de forma
 * cooperativa no tick da página dona; as mensagens vão por clone estruturado nos dois sentidos. BroadcastChannel
 * entrega a todos os canais de mesmo nome e mesma origem, em páginas e workers. */
#include "js_int.h"
#include "web.h"
#include "net.h"
#include "vfs.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

static bool has_proto(jsobj *o, jsobj *p) {
    for (int i = 0; o && p && i < 32; i++, o = o->proto) if (o->proto == p) return true;
    return false;
}

typedef struct wk {
    js_page_t *page;          /* página do worker */
    doc_t *doc;
    js_page_t *owner;         /* página dona */
    jsobj *obj;               /* objeto Worker no reino da dona (vivo pelo registro da dona) */
    bool closing, freed;
    int errs_seen;
    char url[1024];
    struct wk *next;
} wk_t;
static wk_t *workers;

static wk_t *wk_of_page(js_page_t *p) { for (wk_t *w = workers; w; w = w->next) if (w->page == p && !w->freed) return w; return NULL; }
static wk_t *wk_of_obj(jsobj *o) { for (wk_t *w = workers; w; w = w->next) if (w->obj == o && !w->freed) return w; return NULL; }

static jsobj *hidden_arr(jsrealm *r, const char *key) {
    jsstr *k = js_intern(key, -1);
    jsprop *p = js_ownprop(r->global, k);
    if (p && jv_isobj(p->v)) return jv_o(p->v);
    jsobj *a = js_newarr(0);
    js_defprop(r->global, k, jv_obj(a), PF_NOENUM);
    return a;
}

/* ============================================================ eventos em objetos que não são nós */
static void fire_obj(jsobj *target, jsobj *listen, const char *type, jsobj *ev) {
    char on[32];
    snprintf(on, sizeof on, "on%s", type);
    jsv h = js_getn(jv_obj(target), on);
    jsv a = jv_obj(ev);
    if (js_pending()) js_report_exception();
    else if (js_iscallable(h) && js_call(h, jv_obj(target), 1, &a) == JS_EXC) js_report_exception();
    if (!listen) return;
    jsobj *l = js_arr_from(listen->arr, (int)listen->alen);
    for (u32 i = 0; i + 1 < l->alen; i += 2) {
        if (strcmp(jv_s(l->arr[i])->d, type)) continue;
        jsv f = l->arr[i + 1];
        jsv rv = js_iscallable(f) ? js_call(f, jv_obj(target), 1, &a) : js_invoke(f, js_intern("handleEvent", -1), 1, &a);
        if (rv == JS_EXC) js_report_exception();
    }
}

static jsobj *msg_event(const char *type, jsv data, jsobj *target) {
    jsobj *ev = js_newobj(J.realm->p_msgevent ? J.realm->p_msgevent : J.realm->p_object);
    js_defn(ev, "type", jv_str(js_intern(type, -1)), 0);
    js_defn(ev, "data", data, 0);
    js_defn(ev, "origin", jv_str(A[A_empty]), 0);
    js_defn(ev, "lastEventId", jv_str(A[A_empty]), 0);
    js_defn(ev, "source", JS_NULL, 0);
    js_defn(ev, "ports", jv_obj(js_newarr(0)), 0);
    js_defn(ev, "target", jv_obj(target), 0);
    js_defn(ev, "currentTarget", jv_obj(target), 0);
    js_defn(ev, "isTrusted", JS_TRUE, 0);
    return ev;
}

/* addEventListener/removeEventListener em lista de pares guardada num slot */
static jsv listen_op(jsobj *l, int argc, jsv *argv, bool add) {
    jsstr *t = js_tostr(ARG(0));
    if (!t) return JS_EXC;
    jsv f = ARG(1);
    if (!jv_isobj(f)) return JS_UNDEF;
    for (u32 i = 0; i + 1 < l->alen; i += 2) {
        if (!js_str_eq(jv_s(l->arr[i]), t) || l->arr[i + 1] != f) continue;
        if (add) return JS_UNDEF;
        memmove(l->arr + i, l->arr + i + 2, sizeof(jsv) * (l->alen - i - 2));
        js_arr_setlen(l, l->alen - 2);
        return JS_UNDEF;
    }
    if (add) { js_arr_push(l, jv_str(t)); js_arr_push(l, f); }
    return JS_UNDEF;
}

/* ============================================================ fonte de um script (data:, blob:, file:, http) */
/* texto do script em sb; false = falha de rede/inexistente */
static bool load_source(const char *url, sbuf_t *sb) {
    if (!strncmp(url, "data:", 5)) {
        char mime[96];
        usize n = 0;
        u8 *d = frame_data_url(url, &n, mime, sizeof mime);
        if (!d) return false;
        sb_putn(sb, (const char *)d, (int)n);
        kfree(d);
        return true;
    }
    if (!strncmp(url, "blob:", 5)) {
        const u8 *d;
        u32 n;
        if (!js_blob_bytes(js_blob_from_url(url), &d, &n, NULL)) return false;
        sb_putn(sb, (const char *)d, (int)n);
        return true;
    }
    if (!strncmp(url, "file:", 5)) {
        const char *q = url + 5;
        if (!strncmp(q, "//", 2)) q += 2;
        char clean[1024], path[1024];
        snprintf(clean, sizeof clean, "%s", q);
        for (char *c = clean; *c; c++) if (*c == '?' || *c == '#') { *c = 0; break; }
        url_decode_path(clean, path, sizeof path);
        vnode_t *v = vfs_lookup(path);
        if (!v || v->dir) return false;
        sb_putn(sb, (const char *)v->data, (int)v->size);
        return true;
    }
    if (!strncmp(url, "http:", 5) || !strncmp(url, "https:", 6)) {
        /* síncrono (importScripts e o script inicial do worker): espera a rede do kernel, até 60 s */
        http_req_t *r = http_request(url, "GET", "", NULL, 0, 0);
        if (!r) return false;
        u64 limit = timer_ticks() + 60000;
        while (!r->state && timer_ticks() < limit) { net_poll(); http_poll(r); }
        bool ok = r->state == 1 && r->status >= 200 && r->status < 300;
        if (ok) sb_putn(sb, (const char *)r->body, (int)r->body_len);
        http_free(r);
        return ok;
    }
    return false;
}

/* ============================================================ escopo do worker */
static void worker_strip(jsobj *g) {
    static const char *gone[] = { "document", "window", "frames", "parent", "top", "frameElement", "alert", "confirm", "prompt",
        "localStorage", "sessionStorage", "history", "screen", "getComputedStyle", "scrollTo", "scroll", "scrollBy", "print",
        "focus", "Node", "Element", "HTMLElement", "HTMLCanvasElement", "HTMLImageElement", "Image", "DOMParser",
        "MutationObserver", "WebKitMutationObserver", "MutationRecord", "ResizeObserver", "IntersectionObserver",
        "innerWidth", "innerHeight", "outerWidth", "outerHeight", "scrollX", "scrollY", "pageXOffset", "pageYOffset",
        "devicePixelRatio", "CanvasRenderingContext2D" };
    for (u32 i = 0; i < ARRAY_LEN(gone); i++) js_delprop(g, js_intern(gone[i], -1));
}

/* postMessage do worker: vai para a fila de saída, que a dona entrega no Worker */
NATIVE(wg_post) {
    UNUSED_ARGS;
    if (argc < 1) return js_throw_err(E_TYPE, "postMessage: é necessário 1 argumento");
    jsv c = js_structured_clone(ARG(0));
    if (c == JS_EXC) return JS_EXC;
    js_arr_push(hidden_arr(J.realm, "\x01" "saida"), c);
    return JS_UNDEF;
}

NATIVE(wg_close) {
    UNUSED_ARGS;
    wk_t *w = wk_of_page(js_cur_page());
    if (w) w->closing = true;
    return JS_UNDEF;
}

NATIVE(wg_import) {
    UNUSED_ARGS;
    js_page_t *p = js_cur_page();
    for (int i = 0; i < argc; i++) {
        jsstr *u = js_tostr(argv[i]);
        if (!u) return JS_EXC;
        char url[2048];
        if (!web_url_parse(u->d, js_page_base(p), url, sizeof url)) return js_throw_dom("SyntaxError", "importScripts: URL inválido");
        sbuf_t sb = { 0 };
        if (!load_source(url, &sb)) { sb_free(&sb); return js_throw_dom("NetworkError", "importScripts: não foi possível carregar o script"); }
        jsstr *err = NULL;
        jsscript *sc = js_parse(sb.p ? sb.p : "", sb.len, url, 1, &err);
        sb_free(&sb);
        if (!sc) return js_throw_err(E_SYNTAX, "%s", err ? err->d : "importScripts: erro de sintaxe");
        if (js_run_script(sc) == JS_EXC) return JS_EXC;
    }
    return JS_UNDEF;
}

NATIVE(illegal) { UNUSED_ARGS; return js_throw_err(E_TYPE, "Construtor ilegal"); }
NATIVE(loc_tostring) { UNUSED_ARGS; return js_getn(thisv, "href"); }

/* kind: 0 dedicado, 1 compartilhado, 2 service worker */
static void worker_scope(js_page_t *p, const char *url, const char *name, int kind) {
    jsrealm *r = J.realm;
    jsobj *g = r->global;
    worker_strip(g);
    bool shared = kind != 0;
    js_defprop(g, js_intern("\x01" "worker", -1), jv_int(kind + 1), PF_NOENUM);
    /* (Dedicated|Shared|Service)WorkerGlobalScope → WorkerGlobalScope → protótipo antigo do global */
    const char *sn = kind == 2 ? "ServiceWorkerGlobalScope" : shared ? "SharedWorkerGlobalScope" : "DedicatedWorkerGlobalScope";
    jsobj *wgs = js_newobj(g->proto), *dwgs = js_newobj(wgs);
    g->proto = dwgs;
    jsobj *c1 = js_newctor(illegal, "WorkerGlobalScope", 0, wgs), *c2 = js_newctor(illegal, sn, 0, dwgs);
    js_defn(g, "WorkerGlobalScope", jv_obj(c1), PF_NOENUM);
    js_defn(g, sn, jv_obj(c2), PF_NOENUM);
    js_defprop(dwgs, A[A_sym_toStringTag], jv_str(js_intern(sn, -1)), PF_NOENUM | PF_RO);
    js_defn(g, "self", jv_obj(g), PF_NOENUM);
    js_defn(g, "name", jv_str(js_strz(name)), 0);
    js_defn(g, "onerror", JS_NULL, 0);
    if (kind == 1) js_defn(g, "onconnect", JS_NULL, 0);
    else {
        js_defn(g, "onmessage", JS_NULL, 0);
        js_defn(g, "onmessageerror", JS_NULL, 0);
        js_defnative(g, "postMessage", wg_post, 1, 0);
    }
    js_defnative(g, "close", wg_close, 0, 0);
    js_defnative(g, "importScripts", wg_import, 1, 0);
    /* WorkerLocation: URL somente leitura */
    jsv ctor = js_getn(jv_obj(g), "URL");
    jsv a = jv_str(js_strz(url));
    jsv u = js_construct(ctor, 1, &a, ctor);
    if (u == JS_EXC) { js_catch(); return; }
    jsobj *loc = js_newobj(r->p_object);
    static const char *keys[] = { "href", "origin", "protocol", "host", "hostname", "port", "pathname", "search", "hash" };
    for (u32 i = 0; i < ARRAY_LEN(keys); i++) js_defn(loc, keys[i], js_getn(u, keys[i]), PF_RO);
    js_defnative(loc, "toString", loc_tostring, 0, 0);
    js_defprop(loc, A[A_sym_toStringTag], jv_str(js_intern("WorkerLocation", -1)), PF_NOENUM | PF_RO);
    js_defn(g, "location", jv_obj(loc), PF_NOENUM | PF_RO);
    (void)p;
}

typedef struct { const char *url, *name; int kind; } wk_init_t;
static void wk_init(js_page_t *p, int phase, void *ctx) {
    wk_init_t *c = ctx;
    if (phase == 0) worker_scope(p, c->url, c->name, c->kind);
}

/* ============================================================ Worker (lado da dona) */
/* extra do objeto Worker: [ouvintes] */
static jsobj *worker_x(jsv v) {
    if (!jv_iscls(v, CL_HOST) || !has_proto(jv_o(v), J.realm->p_worker) || !jv_isobj(jv_o(v)->u.host.extra)) return NULL;
    return jv_o(jv_o(v)->u.host.extra);
}

NATIVE(worker_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor Worker exige 'new'");
    if (argc < 1) return js_throw_err(E_TYPE, "Worker: é necessário 1 argumento");
    js_page_t *owner = js_cur_page();
    if (!owner) return js_throw_err(E_TYPE, "Worker: sem página");
    jsstr *u = js_tostr(ARG(0));
    if (!u) return JS_EXC;
    char url[1024];
    if (!web_url_parse(u->d, js_page_base(owner), url, sizeof url)) return js_throw_dom("SyntaxError", "Worker: URL inválido");
    /* mesma origem (blob: e data: valem) */
    const char *org = js_page_origin(owner);
    if (strncmp(url, "blob:", 5) && strncmp(url, "data:", 5) && strncmp(url, org, strlen(org)))
        return js_throw_dom("SecurityError", "Worker: o script precisa ser da mesma origem");
    jsstr *name = js_strz("");
    bool module = false;
    if (jv_isobj(ARG(1))) {
        jsv n = js_getn(ARG(1), "name"), t = js_getn(ARG(1), "type");
        if (js_pending()) return JS_EXC;
        if (n != JS_UNDEF) { name = js_tostr(n); if (!name) return JS_EXC; }
        if (t != JS_UNDEF) {
            jsstr *ts = js_tostr(t);
            if (!ts) return JS_EXC;
            if (!strcmp(ts->d, "module")) module = true;
            else if (strcmp(ts->d, "classic")) return js_throw_err(E_TYPE, "Worker: type '%s' inválido", ts->d);
        }
    }
    /* o texto é lido agora, no reino da dona (blob: vive nela) */
    sbuf_t src = { 0 };
    bool ok = load_source(url, &src);
    wk_t *w = kcalloc(1, sizeof *w);
    if (!w) { sb_free(&src); return js_throw_err(E_RANGE, "sem memória"); }
    snprintf(w->url, sizeof w->url, "%s", url);
    w->owner = owner;
    jsobj *o = js_newobj_cls(CL_HOST, jv_isobj(thisv) && jv_o(thisv)->proto ? jv_o(thisv)->proto : J.realm->p_worker);
    o->u.host.page = owner;
    o->u.host.extra = jv_obj(js_arr_from((jsv[]){ jv_obj(js_newarr(0)) }, 1));
    w->obj = o;
    js_arr_push(hidden_arr(J.realm, "\x01" "workers"), jv_obj(o));
    w->next = workers;
    workers = w;
    if (!ok) {
        /* falha ao buscar o script: evento error na dona, no próximo tick */
        sb_free(&src);
        w->closing = true;
        w->errs_seen = -1;
        return jv_obj(o);
    }
    w->doc = doc_parse("", 0, url);
    wk_init_t ic = { url, name->d, 0 };
    w->page = js_page_new2(w->doc, url, 0, 0, wk_init, &ic);
    if (!w->page) { sb_free(&src); w->closing = true; w->errs_seen = -1; return jv_obj(o); }
    js_page_set_origin(w->page, js_page_origin(owner));   /* blob:/data: herdam a origem da dona */
    if (module) js_page_add_module_source(w->page, src.p ? src.p : "", src.len);   /* <script type=module> direto no DOM, sem parser */
    else js_page_eval(w->page, src.p ? src.p : "", src.len, url);
    sb_free(&src);
    return jv_obj(o);
}

/* magic: 0 postMessage 1 terminate 2 addEventListener 3 removeEventListener */
NATIVE(worker_method) {
    UNUSED_ARGS;
    jsobj *x = worker_x(thisv);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal: não é um Worker");
    wk_t *w = wk_of_obj(jv_o(thisv));
    switch (self->u.nat.magic) {
    case 0: {
        if (argc < 1) return js_throw_err(E_TYPE, "postMessage: é necessário 1 argumento");
        jsv c = js_structured_clone(ARG(0));
        if (c == JS_EXC) return JS_EXC;
        if (w && w->page && !w->closing) js_page_post_message(w->page, c, js_page_origin(js_cur_page()));
        return JS_UNDEF;
    }
    case 1: if (w) w->closing = true; return JS_UNDEF;
    case 2: case 3: return listen_op(jv_o(js_arr_get(x, 0)), argc, argv, self->u.nat.magic == 2);
    }
    return JS_UNDEF;
}

static void wk_free(wk_t *w) {
    if (w->freed) return;
    w->freed = true;
    js_page_t *pg = w->page;
    doc_t *d = w->doc;
    w->page = NULL;
    w->doc = NULL;
    if (pg) js_page_free(pg);   /* derruba também os workers aninhados (a dona deles é pg) */
    if (d) doc_free(d);
}

static void sw_pump(js_page_t *owner);
static void sw_drop(js_page_t *owner);
static bool sw_busy(js_page_t *owner);
/* no tick da dona: roda os workers dela e entrega o que eles mandaram */
void web_worker_pump(js_page_t *owner) {
    sw_pump(owner);
    web_sw_pump(owner);
    for (wk_t *w = workers; w; w = w->next) {
        if (w->owner != owner || w->freed) continue;
        jsobj *wo = w->obj;
        jsobj *listen = jv_o(js_arr_get(jv_o(wo->u.host.extra), 0));
        if (w->errs_seen < 0) {
            /* o script não carregou */
            jsobj *ev = msg_event("error", JS_UNDEF, wo);
            js_defn(ev, "message", jv_str(js_strz("o script do Worker não pôde ser carregado")), 0);
            fire_obj(wo, listen, "error", ev);
            w->errs_seen = 0;
            wk_free(w);
            continue;
        }
        if (!w->page) continue;
        if (!w->closing) js_page_tick(w->page);
        /* saída do worker → evento message no Worker (clone no reino da dona) */
        jsrealm *wr = js_page_realm(w->page);
        jsprop *pr = wr ? js_ownprop(wr->global, js_intern("\x01" "saida", -1)) : NULL;
        if (pr && jv_isobj(pr->v) && jv_o(pr->v)->alen) {
            jsobj *q = js_arr_from(jv_o(pr->v)->arr, (int)jv_o(pr->v)->alen);
            js_arr_setlen(jv_o(pr->v), 0);
            for (u32 i = 0; i < q->alen; i++) {
                jsv data = js_structured_clone(q->arr[i]);
                if (data == JS_EXC) { js_catch(); fire_obj(wo, listen, "messageerror", msg_event("messageerror", JS_NULL, wo)); continue; }
                fire_obj(wo, listen, "message", msg_event("message", data, wo));
            }
        }
        /* erro não tratado no worker → evento error no Worker */
        int errs = js_page_errors(w->page);
        if (errs > w->errs_seen) {
            w->errs_seen = errs;
            const char *con = js_page_console(w->page);
            const char *last = con, *q = con;
            while ((q = strchr(q, '\n')) && q[1]) last = ++q;
            jsobj *ev = msg_event("error", JS_UNDEF, wo);
            int ll = 0;
            while (last[ll] && last[ll] != '\n') ll++;
            js_defn(ev, "message", jv_str(js_strn(last, ll)), 0);
            js_defn(ev, "filename", jv_str(js_strz(w->url)), 0);
            js_defn(ev, "lineno", jv_int(0), 0);
            js_defn(ev, "colno", jv_int(0), 0);
            fire_obj(wo, listen, "error", ev);
        }
        if (w->closing) wk_free(w);
    }
}

bool web_worker_busy(js_page_t *owner) {
    for (wk_t *w = workers; w; w = w->next) if (w->owner == owner && !w->freed) return true;
    if (sw_busy(owner)) return true;
    return false;
}

void web_worker_drop(js_page_t *owner) {
    sw_drop(owner);
    web_sw_drop(owner);
    for (wk_t **pp = &workers; *pp;) {
        wk_t *w = *pp;
        if (w->owner == owner) { *pp = w->next; wk_free(w); kfree(w); continue; }
        pp = &w->next;
    }
}

/* página de service worker (web_sw.c): busca o script e roda no escopo ServiceWorkerGlobalScope; NULL = falhou */
js_page_t *web_sw_spawn(const char *url, const char *origin, doc_t **doc) {
    sbuf_t src = { 0 };
    *doc = NULL;
    if (!load_source(url, &src)) { sb_free(&src); return NULL; }
    *doc = doc_parse("", 0, url);
    wk_init_t ic = { url, "", 2 };
    js_page_t *pg = *doc ? js_page_new2(*doc, url, 0, 0, wk_init, &ic) : NULL;
    if (pg) {
        js_page_set_origin(pg, origin);
        js_page_eval(pg, src.p ? src.p : "", src.len, url);
    }
    sb_free(&src);
    if (!pg && *doc) { doc_free(*doc); *doc = NULL; }
    return pg;
}

/* ============================================================ SharedWorker */
/* um worker por (URL, nome, origem), com várias páginas clientes; quem roda o worker é o primeiro cliente vivo */
#define SW_CLIENTS 32
typedef struct sw {
    js_page_t *page;
    doc_t *doc;
    char url[1024], name[256], origin[256];
    struct { js_page_t *p; jsobj *obj; bool err_sent; } cl[SW_CLIENTS];
    int ncl;
    bool failed;
    struct sw *next;
} sw_t;
static sw_t *shared_workers;

static jsobj *sw_x(jsv v) {
    if (!jv_iscls(v, CL_HOST) || !J.realm->p_sharedworker || !has_proto(jv_o(v), J.realm->p_sharedworker) || !jv_isobj(jv_o(v)->u.host.extra)) return NULL;
    return jv_o(jv_o(v)->u.host.extra);
}

/* tarefa no worker: evento connect com a porta nova */
NATIVE(sw_connect_task) {
    UNUSED_ARGS;
    jsobj *g = J.realm->global;
    jsv port = self->u.nat.data;
    jsv ctor = js_getn(jv_obj(g), "Event");
    jsv t = jv_str(js_intern("connect", -1));
    jsv ev = js_iscallable(ctor) ? js_construct(ctor, 1, &t, ctor) : JS_EXC;
    if (ev == JS_EXC) { js_catch(); return JS_UNDEF; }
    js_defprop(jv_o(ev), js_intern("data", -1), jv_str(A[A_empty]), 0);
    js_defprop(jv_o(ev), js_intern("origin", -1), jv_str(A[A_empty]), 0);
    js_defprop(jv_o(ev), js_intern("source", -1), port, 0);
    js_defprop(jv_o(ev), js_intern("ports", -1), jv_obj(js_arr_from(&port, 1)), 0);
    if (js_invoke(jv_obj(g), js_intern("dispatchEvent", -1), 1, &ev) == JS_EXC) js_report_exception();
    return JS_UNDEF;
}

NATIVE(sw_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor SharedWorker exige 'new'");
    if (argc < 1) return js_throw_err(E_TYPE, "SharedWorker: é necessário 1 argumento");
    js_page_t *owner = js_cur_page();
    if (!owner) return js_throw_err(E_TYPE, "SharedWorker: sem página");
    jsstr *u = js_tostr(ARG(0));
    if (!u) return JS_EXC;
    char url[1024];
    if (!web_url_parse(u->d, js_page_base(owner), url, sizeof url)) return js_throw_dom("SyntaxError", "SharedWorker: URL inválido");
    const char *org = js_page_origin(owner);
    if (strncmp(url, "blob:", 5) && strncmp(url, "data:", 5) && strncmp(url, org, strlen(org)))
        return js_throw_dom("SecurityError", "SharedWorker: o script precisa ser da mesma origem");
    jsstr *name = js_strz("");
    jsv opt = ARG(1);
    if (jv_isobj(opt)) { jsv n = js_getn(opt, "name"); if (js_pending()) return JS_EXC; if (n != JS_UNDEF) opt = n; else opt = JS_UNDEF; }
    if (opt != JS_UNDEF) { name = js_tostr(opt); if (!name) return JS_EXC; }
    sw_t *s = NULL;
    for (sw_t *q = shared_workers; q && !s; q = q->next)
        if (!strcmp(q->url, url) && !strcmp(q->name, name->d) && !strcmp(q->origin, org) && q->ncl < SW_CLIENTS) s = q;
    if (!s) {
        s = kcalloc(1, sizeof *s);
        if (!s) return js_throw_err(E_RANGE, "sem memória");
        snprintf(s->url, sizeof s->url, "%s", url);
        snprintf(s->name, sizeof s->name, "%s", name->d);
        snprintf(s->origin, sizeof s->origin, "%s", org);
        sbuf_t src = { 0 };
        if (load_source(url, &src)) {
            s->doc = doc_parse("", 0, url);
            wk_init_t ic = { url, name->d, 1 };
            s->page = js_page_new2(s->doc, url, 0, 0, wk_init, &ic);
            if (s->page) {
                js_page_set_origin(s->page, org);
                js_page_eval(s->page, src.p ? src.p : "", src.len, url);
            }
        }
        sb_free(&src);
        if (!s->page) s->failed = true;
        s->next = shared_workers;
        shared_workers = s;
    }
    jsobj *o = js_newobj_cls(CL_HOST, jv_isobj(thisv) && jv_o(thisv)->proto ? jv_o(thisv)->proto : J.realm->p_sharedworker);
    o->u.host.page = owner;
    o->u.host.extra = jv_obj(js_arr_from((jsv[]){ jv_obj(js_newarr(0)) }, 1));
    js_arr_push(hidden_arr(J.realm, "\x01" "sharedworkers"), jv_obj(o));
    int ci = s->ncl++;
    s->cl[ci].p = owner;
    s->cl[ci].obj = o;
    s->cl[ci].err_sent = false;
    if (s->page) {
        jsobj *wport;
        jsobj *port = js_port_pair(js_page_realm(s->page), &wport);
        js_defn(o, "port", jv_obj(port), PF_RO);
        jsobj *t = js_newnative(sw_connect_task, "connect", 0, 0);
        t->u.nat.data = jv_obj(wport);
        js_page_add_timer(s->page, jv_obj(t), 0);
    } else {
        jsobj *wport;
        js_defn(o, "port", jv_obj(js_port_pair(J.realm, &wport)), PF_RO);   /* porta sem ninguém do outro lado */
    }
    return jv_obj(o);
}

NATIVE(sw_method) {
    UNUSED_ARGS;
    jsobj *x = sw_x(thisv);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal: não é um SharedWorker");
    return listen_op(jv_o(js_arr_get(x, 0)), argc, argv, self->u.nat.magic == 0);
}

static void sw_free(sw_t *s) {
    js_page_t *pg = s->page;
    doc_t *d = s->doc;
    s->page = NULL;
    s->doc = NULL;
    if (pg) js_page_free(pg);
    if (d) doc_free(d);
}

/* no tick de cada página: o primeiro cliente vivo roda o worker; erro de carga vai para os SharedWorker dela */
static void sw_pump(js_page_t *owner) {
    for (sw_t *s = shared_workers; s; s = s->next) {
        for (int i = 0; i < s->ncl; i++) {
            if (s->cl[i].p != owner) continue;
            if (s->failed && !s->cl[i].err_sent) {
                s->cl[i].err_sent = true;
                fire_obj(s->cl[i].obj, jv_o(js_arr_get(jv_o(s->cl[i].obj->u.host.extra), 0)), "error", msg_event("error", JS_UNDEF, s->cl[i].obj));
            }
        }
        if (s->page && s->ncl && s->cl[0].p == owner) js_page_tick(s->page);
    }
}

static bool sw_busy(js_page_t *owner) {
    for (sw_t *s = shared_workers; s; s = s->next) if (s->page && s->ncl && s->cl[0].p == owner && js_page_busy(s->page)) return true;
    return false;
}

static void sw_drop(js_page_t *owner) {
    for (sw_t **pp = &shared_workers; *pp;) {
        sw_t *s = *pp;
        int k = 0;
        for (int i = 0; i < s->ncl; i++) if (s->cl[i].p != owner) s->cl[k++] = s->cl[i];
        s->ncl = k;
        if (!s->ncl) { *pp = s->next; sw_free(s); kfree(s); continue; }
        pp = &s->next;
    }
}

/* ============================================================ BroadcastChannel */
/* extra: [nome, ouvintes, aberto] ; registro global em C (nome + origem + reino) */
typedef struct bc { jsobj *obj; jsrealm *realm; char name[256], origin[256]; struct bc *next; } bc_t;
static bc_t *channels;

static jsobj *bc_x(jsv v) {
    if (!jv_iscls(v, CL_HOST) || !has_proto(jv_o(v), J.realm->p_bchannel) || !jv_isobj(jv_o(v)->u.host.extra)) return NULL;
    return jv_o(jv_o(v)->u.host.extra);
}

NATIVE(bc_task) {
    UNUSED_ARGS;
    jsobj *o = jv_o(self->u.nat.data);
    jsobj *x = jv_o(o->u.host.extra);
    if (!js_truthy(js_arr_get(x, 2))) return JS_UNDEF;
    jsv data = js_structured_clone(self->u.nat.data2);
    if (data == JS_EXC) { js_catch(); return JS_UNDEF; }
    jsobj *ev = msg_event("message", data, o);
    js_defn(ev, "origin", jv_str(js_strz(js_page_origin(js_cur_page()))), 0);
    fire_obj(o, jv_o(js_arr_get(x, 1)), "message", ev);
    return JS_UNDEF;
}

NATIVE(bc_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor BroadcastChannel exige 'new'");
    if (argc < 1) return js_throw_err(E_TYPE, "BroadcastChannel: é necessário 1 argumento");
    jsstr *n = js_tostr(ARG(0));
    if (!n) return JS_EXC;
    jsobj *o = js_newobj_cls(CL_HOST, J.realm->p_bchannel);
    o->u.host.page = js_cur_page();
    jsv s[3] = { jv_str(n), jv_obj(js_newarr(0)), JS_TRUE };
    o->u.host.extra = jv_obj(js_arr_from(s, 3));
    js_arr_push(hidden_arr(J.realm, "\x01" "canais"), jv_obj(o));
    bc_t *c = kcalloc(1, sizeof *c);
    if (c) {
        c->obj = o; c->realm = J.realm;
        snprintf(c->name, sizeof c->name, "%s", n->d);
        snprintf(c->origin, sizeof c->origin, "%s", js_page_origin(js_cur_page()));
        c->next = channels;
        channels = c;
    }
    return jv_obj(o);
}

/* magic: 0 postMessage 1 close 2 addEventListener 3 removeEventListener 4 name */
NATIVE(bc_method) {
    UNUSED_ARGS;
    jsobj *x = bc_x(thisv);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal: não é um BroadcastChannel");
    switch (self->u.nat.magic) {
    case 0: {
        if (!js_truthy(js_arr_get(x, 2))) return js_throw_dom("InvalidStateError", "BroadcastChannel fechado");
        if (argc < 1) return js_throw_err(E_TYPE, "postMessage: é necessário 1 argumento");
        jsv c = js_structured_clone(ARG(0));
        if (c == JS_EXC) return JS_EXC;
        const char *org = js_page_origin(js_cur_page());
        jsstr *nm = jv_s(js_arr_get(x, 0));
        for (bc_t *b = channels; b; b = b->next) {
            if (b->obj == jv_o(thisv) || strcmp(b->name, nm->d) || strcmp(b->origin, org)) continue;
            jsobj *bx = jv_o(b->obj->u.host.extra);
            if (!js_truthy(js_arr_get(bx, 2))) continue;
            js_page_t *dst = b->realm->page;
            if (!dst) continue;
            jsobj *t = js_newnative(bc_task, "canal", 0, 0);
            t->u.nat.data = jv_obj(b->obj);
            t->u.nat.data2 = c;
            js_page_add_timer(dst, jv_obj(t), 0);
        }
        return JS_UNDEF;
    }
    case 1: {
        js_arr_set(x, 2, JS_FALSE);
        for (bc_t **pp = &channels; *pp; pp = &(*pp)->next) if ((*pp)->obj == jv_o(thisv)) { bc_t *d = *pp; *pp = d->next; kfree(d); break; }
        return JS_UNDEF;
    }
    case 2: case 3: return listen_op(jv_o(js_arr_get(x, 1)), argc, argv, self->u.nat.magic == 2);
    case 4: return js_arr_get(x, 0);
    }
    return JS_UNDEF;
}

/* reino que some: os canais dele saem do registro */
void web_bc_realm_gone(jsrealm *r) {
    for (bc_t **pp = &channels; *pp;) { if ((*pp)->realm == r) { bc_t *d = *pp; *pp = d->next; kfree(d); continue; } pp = &(*pp)->next; }
}

/* ============================================================ registro */
void js_worker_init(jsrealm *r) {
    jsobj *g = r->global, *op = r->p_object;
    jsobj *et = r->p_node ? r->p_node->proto : op;
    r->p_worker = js_newobj(et);
    js_defn(g, "Worker", jv_obj(js_newctor(worker_ctor, "Worker", 1, r->p_worker)), PF_NOENUM);
    js_defprop(r->p_worker, A[A_sym_toStringTag], jv_str(js_intern("Worker", -1)), PF_NOENUM | PF_RO);
    static const char *wm[] = { "postMessage", "terminate", "addEventListener", "removeEventListener" };
    for (int i = 0; i < 4; i++) js_defnative(r->p_worker, wm[i], worker_method, i == 1 ? 0 : i == 0 ? 1 : 2, i);
    js_defn(r->p_worker, "onmessage", JS_NULL, 0);
    js_defn(r->p_worker, "onmessageerror", JS_NULL, 0);
    js_defn(r->p_worker, "onerror", JS_NULL, 0);
    r->p_sharedworker = js_newobj(et);
    js_defn(g, "SharedWorker", jv_obj(js_newctor(sw_ctor, "SharedWorker", 1, r->p_sharedworker)), PF_NOENUM);
    js_defprop(r->p_sharedworker, A[A_sym_toStringTag], jv_str(js_intern("SharedWorker", -1)), PF_NOENUM | PF_RO);
    js_defnative(r->p_sharedworker, "addEventListener", sw_method, 2, 0);
    js_defnative(r->p_sharedworker, "removeEventListener", sw_method, 2, 1);
    js_defn(r->p_sharedworker, "onerror", JS_NULL, 0);
    r->p_bchannel = js_newobj(et);
    js_defn(g, "BroadcastChannel", jv_obj(js_newctor(bc_ctor, "BroadcastChannel", 1, r->p_bchannel)), PF_NOENUM);
    js_defprop(r->p_bchannel, A[A_sym_toStringTag], jv_str(js_intern("BroadcastChannel", -1)), PF_NOENUM | PF_RO);
    static const char *bm[] = { "postMessage", "close", "addEventListener", "removeEventListener" };
    for (int i = 0; i < 4; i++) js_defnative(r->p_bchannel, bm[i], bc_method, i == 1 ? 0 : i == 0 ? 1 : 2, i);
    js_defgetter(r->p_bchannel, "name", bc_method, 4);
    js_defn(r->p_bchannel, "onmessage", JS_NULL, 0);
    js_defn(r->p_bchannel, "onmessageerror", JS_NULL, 0);
}
