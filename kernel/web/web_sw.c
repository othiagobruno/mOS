/* mOS — Service Workers e Cache API do motor web, lado C: registros por escopo (um worker por registro, em página
 * própria), ciclo de vida install → installed → activating → activated (waitUntil, skipWaiting, claim), controle das
 * páginas do escopo, ponte entre página e worker (fetch interceptado, mensagens) e o armazenamento do Cache API por
 * origem. A API em JS fica em web_sw.js (web_sw_js.h). O estado vive enquanto o sistema está ligado. */
#include "js_int.h"
#include "web.h"
#include "web_sw_js.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

enum { S_INSTALLING, S_INSTALLED, S_ACTIVATING, S_ACTIVATED, S_REDUNDANT };
static const char *state_names[] = { "installing", "installed", "activating", "activated", "redundant" };

typedef struct reg {
    int id;
    char origin[256], scope[1024], script[1024];
    js_page_t *page;            /* página do worker */
    doc_t *doc;
    js_page_t *ticker;          /* página que roda o worker (uma da origem) */
    int state;
    bool claimed, skip;
    u64 activated_at;
    struct reg *next;
} reg_t;
static reg_t *regs;
static int next_id = 1, next_fetch = 1;
static bool spawning;   /* a página que nasce é a de um service worker: nunca é controlada */

typedef struct pend { int id; js_page_t *page; reg_t *reg; struct pend *next; } pend_t;
static pend_t *pends;

/* ---------------------------------------------------------------- páginas */
static bool is_worker_page(js_page_t *p) {
    jsrealm *r = js_page_realm(p);
    return !r || js_ownprop(r->global, js_intern("\x01" "worker", -1)) != NULL;
}
static bool page_alive(js_page_t *p) {
    for (js_page_t *q = js_page_iter(NULL); q; q = js_page_iter(q)) if (q == p) return true;
    return false;
}
static reg_t *reg_by_id(int id) { for (reg_t *r = regs; r; r = r->next) if (r->id == id && r->state != S_REDUNDANT) return r; return NULL; }
static reg_t *reg_of_worker(js_page_t *p) { for (reg_t *r = regs; r; r = r->next) if (r->page == p) return r; return NULL; }

/* registro que controla a página p (ou NULL) */
static reg_t *controller_of(js_page_t *p) {
    if (!p || is_worker_page(p)) return NULL;
    const char *url = js_page_url(p), *org = js_page_origin(p);
    reg_t *best = NULL;
    for (reg_t *r = regs; r; r = r->next) {
        if ((r->state != S_ACTIVATED && r->state != S_ACTIVATING) || strcmp(r->origin, org) || strncmp(url, r->scope, strlen(r->scope))) continue;
        if (!r->claimed && js_page_created(p) <= r->activated_at) continue;   /* página aberta antes da ativação */
        if (!best || strlen(r->scope) > strlen(best->scope)) best = r;
    }
    return best;
}

/* ---------------------------------------------------------------- fonte JS */
static jsv sw_host(jsrealm *r);
static jsv bundle(void) {
    jsobj *g = J.realm->global;
    jsstr *k = js_intern("\x01" "swb", -1);
    jsprop *p = js_ownprop(g, k);
    if (p && jv_isobj(p->v)) return p->v;
    /* o fetch preguiçoso sai antes: o bundle guarda o fetch de verdade */
    jsprop *orig = js_ownprop(g, js_intern("\x01" "fetch0", -1));
    if (orig) js_defprop(g, js_intern("fetch", -1), orig->v, PF_NOENUM);
    jsstr *err = NULL;
    jsscript *sc = js_parse(web_sw_src, (int)sizeof web_sw_src - 1, "serviceworker", 1, &err);
    if (!sc) return js_throw_err(E_SYNTAX, "serviceworker: %s", err ? err->d : "erro de sintaxe");
    jsv fn = js_run_script(sc);
    if (fn == JS_EXC) return JS_EXC;
    jsv a[3] = { jv_obj(g), sw_host(J.realm), jv_bool(js_ownprop(g, js_intern("\x01" "worker", -1)) != NULL) };
    jsv b = js_call(fn, JS_UNDEF, 3, a);
    if (b == JS_EXC) return JS_EXC;
    js_defprop(g, k, b, PF_NOENUM);
    static const char *names[] = { "Cache", "CacheStorage", "caches", "ServiceWorker", "ServiceWorkerRegistration", "ServiceWorkerContainer", "Client" };
    for (u32 i = 0; i < ARRAY_LEN(names); i++) {
        jsv v = js_getn(b, names[i]);
        if (v == JS_UNDEF) continue;
        jsstr *n = js_intern(names[i], -1);
        js_delprop(g, n);
        js_defprop(g, n, v, PF_NOENUM);
    }
    return b;
}

/* ---------------------------------------------------------------- entrega entre páginas */
NATIVE(deliver_task) {
    UNUSED_ARGS;
    jsobj *g = J.realm->global;
    jsprop *qp = js_ownprop(g, js_intern("\x01" "swq", -1));
    if (!qp || !jv_isobj(qp->v) || !jv_o(qp->v)->alen) return JS_UNDEF;
    jsobj *q = jv_o(qp->v);
    jsv e = q->arr[0];
    memmove(q->arr, q->arr + 1, sizeof(jsv) * (q->alen - 1));
    js_arr_setlen(q, q->alen - 1);
    if (bundle() == JS_EXC) { js_report_exception(); return JS_UNDEF; }
    jsv a[3] = { js_arr_get(jv_o(e), 0), js_structured_clone(js_arr_get(jv_o(e), 1)), JS_UNDEF };
    if (a[1] == JS_EXC) { js_catch(); a[1] = JS_UNDEF; }
    a[2] = js_structured_clone(js_arr_get(jv_o(e), 2));
    if (a[2] == JS_EXC) { js_catch(); a[2] = JS_UNDEF; }
    jsv fn = js_getn(jv_obj(g), "\x01" "swin");
    if (js_iscallable(fn) && js_call(fn, JS_UNDEF, 3, a) == JS_EXC) js_report_exception();
    return JS_UNDEF;
}
static void deliver(js_page_t *to, const char *kind, jsv a, jsv b) {
    jsrealm *r = to ? js_page_realm(to) : NULL;
    if (!r) return;
    jsstr *k = js_intern("\x01" "swq", -1);
    jsprop *qp = js_ownprop(r->global, k);
    jsobj *q;
    if (qp && jv_isobj(qp->v)) q = jv_o(qp->v);
    else { q = js_newarr(0); js_defprop(r->global, k, jv_obj(q), PF_NOENUM); }
    jsv e[3] = { jv_str(js_intern(kind, -1)), a, b };
    js_arr_push(q, jv_obj(js_arr_from(e, 3)));
    js_page_add_timer(to, jv_obj(js_newnative(deliver_task, "sw", 0, 0)), 0);
}
/* avisa as janelas da origem que já usam a API */
static void sw_notify(reg_t *r, const char *kind) {
    for (js_page_t *p = js_page_iter(NULL); p; p = js_page_iter(p)) {
        if (is_worker_page(p) || strcmp(js_page_origin(p), r->origin)) continue;
        jsrealm *rr = js_page_realm(p);
        if (!rr || !js_ownprop(rr->global, js_intern("\x01" "swb", -1))) continue;
        deliver(p, kind, jv_int(r->id), JS_UNDEF);
    }
}
static void set_state(reg_t *r, int st) {
    r->state = st;
    if (st == S_ACTIVATED) r->activated_at = timer_ticks();
    sw_notify(r, "state");
}

/* ---------------------------------------------------------------- ciclo de vida */
static void kill_reg(reg_t *r) {
    for (pend_t **pp = &pends; *pp;) { if ((*pp)->reg == r) { pend_t *d = *pp; *pp = d->next; if (page_alive(d->page)) deliver(d->page, "resp", jv_int(d->id), JS_NULL); kfree(d); } else pp = &(*pp)->next; }
    js_page_t *pg = r->page;
    doc_t *d = r->doc;
    r->page = NULL;
    r->doc = NULL;
    if (pg) js_page_free(pg);
    if (d) doc_free(d);
}

static void try_activate(reg_t *r) {
    /* outro registro ativo no mesmo escopo espera as páginas dele, salvo skipWaiting */
    for (reg_t *o = regs; o; o = o->next)
        if (o != r && o->state == S_ACTIVATED && !strcmp(o->scope, r->scope) && !strcmp(o->origin, r->origin)) {
            if (!r->skip) return;
            set_state(o, S_REDUNDANT);
            kill_reg(o);
        }
    set_state(r, S_ACTIVATING);
    deliver(r->page, "activate", JS_UNDEF, JS_UNDEF);
}

/* ---------------------------------------------------------------- Cache API */
typedef struct centry { char *url; int status; char *stext; char *hdrs; int nh; u8 *body; u32 blen; char *rurl; struct centry *next; } centry_t;
typedef struct cache { char origin[256], name[256]; centry_t *e; struct cache *next; } cache_t;
static cache_t *caches;

static char *dupz(const char *s) { usize n = strlen(s); char *o = kmalloc(n + 1); if (o) memcpy(o, s, n + 1); return o; }
static cache_t *cache_find(const char *org, const char *name) {
    for (cache_t *c = caches; c; c = c->next) if (!strcmp(c->origin, org) && !strcmp(c->name, name)) return c;
    return NULL;
}
static void entry_free(centry_t *e) { kfree(e->url); kfree(e->stext); kfree(e->hdrs); kfree(e->body); kfree(e->rurl); kfree(e); }
static const char *page_org(void) { js_page_t *p = js_cur_page(); return p ? js_page_origin(p) : "null"; }
static jsstr *arg_str(jsv v) { return js_tostr(v); }

/* ---------------------------------------------------------------- natives H */
enum { H_ORIGIN, H_REGISTER, H_INFO, H_LIST, H_UNREG, H_CONTROLLER, H_FETCH, H_POSTW, H_UPDATE,
       H_CLAIM = 20, H_SKIP, H_DONE, H_RESPOND, H_CLIENTS, H_POSTC, H_SCOPE, H_UNREGSELF,
       H_COPEN = 40, H_CHAS, H_CDEL, H_CKEYS, H_EGET, H_EPUT, H_EDEL, H_EKEYS };

NATIVE(h_native) {
    UNUSED_ARGS;
    js_page_t *p = js_cur_page();
    const char *org = page_org();
    switch (self->u.nat.magic) {
    case H_ORIGIN: return jv_str(js_strz(org));
    case H_REGISTER: {
        jsstr *su = arg_str(ARG(0)), *sc = su ? arg_str(ARG(1)) : NULL;
        if (!sc) return JS_EXC;
        usize ol = strlen(org);
        if (strncmp(su->d, "http", 4) && strncmp(su->d, "file:", 5)) return js_throw_err(E_TYPE, "register: o script precisa ser http(s)");
        if (strncmp(su->d, org, ol) || strncmp(sc->d, org, ol)) return js_throw_dom("SecurityError", "register: script e escopo precisam ser da origem da página");
        /* o escopo não pode passar da pasta do script (sem Service-Worker-Allowed) */
        char dir[1024];
        snprintf(dir, sizeof dir, "%s", su->d);
        char *sl = strrchr(dir, '/');
        if (sl) sl[1] = 0;
        if (strncmp(sc->d, dir, strlen(dir))) return js_throw_dom("SecurityError", "register: o escopo precisa estar dentro da pasta do script");
        for (reg_t *r = regs; r; r = r->next)
            if (r->state != S_REDUNDANT && !strcmp(r->origin, org) && !strcmp(r->scope, sc->d) && !strcmp(r->script, su->d)) return jv_int(r->id);
        reg_t *r = kcalloc(1, sizeof *r);
        if (!r) return js_throw_err(E_RANGE, "sem memória");
        r->id = next_id++;
        snprintf(r->origin, sizeof r->origin, "%s", org);
        snprintf(r->scope, sizeof r->scope, "%s", sc->d);
        snprintf(r->script, sizeof r->script, "%s", su->d);
        spawning = true;
        r->page = web_sw_spawn(su->d, org, &r->doc);
        spawning = false;
        if (!r->page) { kfree(r); return js_throw_err(E_TYPE, "register: o script do service worker não pôde ser carregado"); }
        r->ticker = p;
        r->next = regs;
        regs = r;
        r->state = S_INSTALLING;
        deliver(r->page, "install", JS_UNDEF, JS_UNDEF);
        return jv_int(r->id);
    }
    case H_INFO: {
        reg_t *r = reg_by_id(js_toint32(ARG(0)));
        if (!r) {
            for (reg_t *q = regs; q; q = q->next) if (q->id == js_toint32(ARG(0))) r = q;
            if (!r) return JS_NULL;
        }
        jsobj *o = js_newobj(J.realm->p_object);
        js_defn(o, "scope", jv_str(js_strz(r->scope)), 0);
        js_defn(o, "script", jv_str(js_strz(r->script)), 0);
        js_defn(o, "state", jv_str(js_strz(state_names[r->state])), 0);
        return jv_obj(o);
    }
    case H_LIST: {
        jsobj *a = js_newarr(0);
        for (reg_t *r = regs; r; r = r->next) if (r->state != S_REDUNDANT && !strcmp(r->origin, org)) js_arr_push(a, jv_int(r->id));
        return jv_obj(a);
    }
    case H_UNREG: case H_UNREGSELF: {
        reg_t *r = self->u.nat.magic == H_UNREG ? reg_by_id(js_toint32(ARG(0))) : reg_of_worker(p);
        if (!r || strcmp(r->origin, org)) return JS_FALSE;
        set_state(r, S_REDUNDANT);
        sw_notify(r, "controller");
        if (r->page != p) kill_reg(r);   /* o próprio worker sai no próximo tick (não se libera rodando) */
        return JS_TRUE;
    }
    case H_CONTROLLER: { reg_t *r = controller_of(p); return jv_int(r ? r->id : -1); }
    case H_FETCH: {
        reg_t *r = reg_by_id(js_toint32(ARG(0)));
        pend_t *d = kcalloc(1, sizeof *d);
        if (!d) return js_throw_err(E_RANGE, "sem memória");
        d->id = next_fetch++;
        d->page = p;
        d->reg = r;
        d->next = pends;
        pends = d;
        if (!r || !r->page) deliver(p, "resp", jv_int(d->id), JS_NULL);
        else deliver(r->page, "fetch", jv_int(d->id), ARG(1));
        return jv_int(d->id);
    }
    case H_POSTW: {
        reg_t *r = reg_by_id(js_toint32(ARG(0)));
        if (!r || !r->page) return JS_UNDEF;
        jsobj *src = js_newobj(J.realm->p_object);
        js_defn(src, "id", jv_str(js_strz(js_page_url(p))), 0);
        js_defn(src, "url", jv_str(js_strz(js_page_url(p))), 0);
        deliver(r->page, "message", ARG(1), jv_obj(src));
        return JS_UNDEF;
    }
    case H_UPDATE: return JS_UNDEF;   /* o script não muda sem recarregar o worker: nada a atualizar */
    case H_CLAIM: {
        reg_t *r = reg_of_worker(p);
        if (!r || (r->state != S_ACTIVATED && r->state != S_ACTIVATING)) return js_throw_dom("InvalidStateError", "clients.claim: o worker não está ativo");
        r->claimed = true;
        sw_notify(r, "controller");
        return JS_UNDEF;
    }
    case H_SKIP: { reg_t *r = reg_of_worker(p); if (r) { r->skip = true; if (r->state == S_INSTALLED) try_activate(r); } return JS_UNDEF; }
    case H_DONE: {
        reg_t *r = reg_of_worker(p);
        jsstr *t = arg_str(ARG(0));
        if (!r || !t) return JS_UNDEF;
        bool ok = js_truthy(ARG(1));
        if (!strcmp(t->d, "install")) {
            if (!ok) { set_state(r, S_REDUNDANT); return JS_UNDEF; }
            set_state(r, S_INSTALLED);
            try_activate(r);
        } else if (!strcmp(t->d, "activate")) set_state(r, S_ACTIVATED);
        return JS_UNDEF;
    }
    case H_RESPOND: {
        int id = js_toint32(ARG(0));
        for (pend_t **pp = &pends; *pp; pp = &(*pp)->next) {
            if ((*pp)->id != id) continue;
            pend_t *d = *pp;
            *pp = d->next;
            if (page_alive(d->page)) deliver(d->page, "resp", jv_int(id), ARG(1));
            kfree(d);
            break;
        }
        return JS_UNDEF;
    }
    case H_CLIENTS: {
        reg_t *r = reg_of_worker(p);
        jsobj *a = js_newarr(0);
        if (!r) return jv_obj(a);
        for (js_page_t *q = js_page_iter(NULL); q; q = js_page_iter(q)) {
            if (is_worker_page(q) || strcmp(js_page_origin(q), r->origin) || strncmp(js_page_url(q), r->scope, strlen(r->scope))) continue;
            jsobj *c = js_newobj(J.realm->p_object);
            js_defn(c, "id", jv_str(js_strz(js_page_url(q))), 0);
            js_defn(c, "url", jv_str(js_strz(js_page_url(q))), 0);
            js_defn(c, "controlled", jv_bool(controller_of(q) == r), 0);
            js_arr_push(a, jv_obj(c));
        }
        return jv_obj(a);
    }
    case H_POSTC: {
        reg_t *r = reg_of_worker(p);
        jsstr *id = arg_str(ARG(0));
        if (!r || !id) return JS_EXC;
        for (js_page_t *q = js_page_iter(NULL); q; q = js_page_iter(q))
            if (!is_worker_page(q) && !strcmp(js_page_url(q), id->d) && !strcmp(js_page_origin(q), r->origin)) deliver(q, "message", ARG(1), JS_UNDEF);
        return JS_UNDEF;
    }
    case H_SCOPE: { reg_t *r = reg_of_worker(p); return jv_str(js_strz(r ? r->scope : "")); }
    case H_COPEN: case H_CHAS: case H_CDEL: {
        jsstr *n = arg_str(ARG(0));
        if (!n) return JS_EXC;
        cache_t *c = cache_find(org, n->d);
        if (self->u.nat.magic == H_CHAS) return jv_bool(c != NULL);
        if (self->u.nat.magic == H_CDEL) {
            if (!c) return JS_FALSE;
            for (cache_t **pp = &caches; *pp; pp = &(*pp)->next) if (*pp == c) { *pp = c->next; break; }
            for (centry_t *e = c->e, *nx; e; e = nx) { nx = e->next; entry_free(e); }
            kfree(c);
            return JS_TRUE;
        }
        if (!c) {
            c = kcalloc(1, sizeof *c);
            if (!c) return js_throw_err(E_RANGE, "sem memória");
            snprintf(c->origin, sizeof c->origin, "%s", org);
            snprintf(c->name, sizeof c->name, "%s", n->d);
            cache_t **pp = &caches;   /* ordem de criação (keys() devolve nessa ordem) */
            while (*pp) pp = &(*pp)->next;
            *pp = c;
        }
        return JS_TRUE;
    }
    case H_CKEYS: {
        jsobj *a = js_newarr(0);
        for (cache_t *c = caches; c; c = c->next) if (!strcmp(c->origin, org)) js_arr_push(a, jv_str(js_strz(c->name)));
        return jv_obj(a);
    }
    case H_EKEYS: case H_EGET: case H_EDEL: case H_EPUT: {
        jsstr *n = arg_str(ARG(0));
        if (!n) return JS_EXC;
        cache_t *c = cache_find(org, n->d);
        int m = self->u.nat.magic;
        if (m == H_EKEYS) {
            jsobj *a = js_newarr(0);
            for (centry_t *e = c ? c->e : NULL; e; e = e->next) js_arr_push(a, jv_str(js_strz(e->url)));
            return jv_obj(a);
        }
        jsstr *u = arg_str(ARG(1));
        if (!u) return JS_EXC;
        centry_t **pp = c ? &c->e : NULL, *e = NULL;
        while (pp && *pp && strcmp((*pp)->url, u->d)) pp = &(*pp)->next;
        if (pp) e = *pp;
        if (m == H_EGET) {
            if (!e) return JS_NULL;
            jsobj *o = js_newobj(J.realm->p_object);
            js_defn(o, "url", jv_str(js_strz(e->rurl ? e->rurl : e->url)), 0);
            js_defn(o, "status", jv_int(e->status), 0);
            js_defn(o, "statusText", jv_str(js_strz(e->stext)), 0);
            jsobj *hs = js_newarr(0);
            const char *q = e->hdrs;
            for (int i = 0; i < e->nh; i++) {
                const char *v = q + strlen(q) + 1;
                jsv pr[2] = { jv_str(js_strz(q)), jv_str(js_strz(v)) };
                js_arr_push(hs, jv_obj(js_arr_from(pr, 2)));
                q = v + strlen(v) + 1;
            }
            js_defn(o, "headers", jv_obj(hs), 0);
            jsobj *b = js_abuf_new(e->blen);
            if (!b) return JS_EXC;
            if (e->blen) memcpy(b->u.buf.data, e->body, e->blen);
            js_defn(o, "body", jv_obj(b), 0);
            return jv_obj(o);
        }
        if (m == H_EDEL) {
            if (!e) return JS_FALSE;
            *pp = e->next;
            entry_free(e);
            return JS_TRUE;
        }
        /* put: {url, status, statusText, headers, body} */
        if (!c) {
            c = kcalloc(1, sizeof *c);
            if (!c) return js_throw_err(E_RANGE, "sem memória");
            snprintf(c->origin, sizeof c->origin, "%s", org);
            snprintf(c->name, sizeof c->name, "%s", n->d);
            c->next = caches;
            caches = c;
            pp = &c->e;
        }
        jsv d = ARG(2);
        centry_t *ne = kcalloc(1, sizeof *ne);
        if (!ne) return js_throw_err(E_RANGE, "sem memória");
        ne->url = dupz(u->d);
        jsv ru = js_getn(d, "url");
        jsstr *rus = jv_isstr(ru) && jv_s(ru)->len ? jv_s(ru) : NULL;
        ne->rurl = rus ? dupz(rus->d) : NULL;
        ne->status = js_toint32(js_getn(d, "status"));
        jsstr *st = js_tostr(js_getn(d, "statusText"));
        ne->stext = dupz(st ? st->d : "");
        jsv hv = js_getn(d, "headers");
        sbuf_t hb = { 0 };
        u32 hn = jv_isobj(hv) ? js_lengthof(hv) : 0;
        for (u32 i = 0; i < hn; i++) {
            jsv pr = js_getidx(hv, i);
            jsstr *k = js_tostr(js_getidx(pr, 0)), *v = js_tostr(js_getidx(pr, 1));
            if (!k || !v) { sb_free(&hb); entry_free(ne); return JS_EXC; }
            sb_putn(&hb, k->d, (int)k->len + 1);
            sb_putn(&hb, v->d, (int)v->len + 1);
            ne->nh++;
        }
        ne->hdrs = kmalloc((usize)hb.len + 1);
        if (ne->hdrs) { if (hb.len) memcpy(ne->hdrs, hb.p, (usize)hb.len); ne->hdrs[hb.len] = 0; }
        sb_free(&hb);
        jsv bv = js_getn(d, "body");
        if (jv_iscls(bv, CL_ARRAYBUFFER) && jv_o(bv)->u.buf.len) {
            ne->blen = jv_o(bv)->u.buf.len;
            ne->body = kmalloc(ne->blen);
            if (ne->body) memcpy(ne->body, jv_o(bv)->u.buf.data, ne->blen); else ne->blen = 0;
        }
        if (e) { *pp = e->next; entry_free(e); }
        pp = &c->e;   /* entrada nova vai para o fim (ordem de inserção) */
        while (*pp) pp = &(*pp)->next;
        *pp = ne;
        return JS_UNDEF;
    }
    }
    return JS_UNDEF;
}

static jsv sw_host(jsrealm *r) {
    (void)r;
    static const struct { const char *n; int m; } fs[] = {
        { "origin", H_ORIGIN }, { "register", H_REGISTER }, { "info", H_INFO }, { "list", H_LIST }, { "unregister", H_UNREG },
        { "controller", H_CONTROLLER }, { "fetch", H_FETCH }, { "postToWorker", H_POSTW }, { "update", H_UPDATE },
        { "claim", H_CLAIM }, { "skipWaiting", H_SKIP }, { "done", H_DONE }, { "respond", H_RESPOND }, { "clients", H_CLIENTS },
        { "postToClient", H_POSTC }, { "scope", H_SCOPE }, { "unregisterSelf", H_UNREGSELF },
        { "cOpen", H_COPEN }, { "cHas", H_CHAS }, { "cDelete", H_CDEL }, { "cKeys", H_CKEYS }, { "eGet", H_EGET }, { "ePut", H_EPUT },
        { "eDel", H_EDEL }, { "eKeys", H_EKEYS },
    };
    jsobj *o = js_newobj(J.realm->p_object);
    for (u32 i = 0; i < ARRAY_LEN(fs); i++) js_defnative(o, fs[i].n, h_native, 2, fs[i].m);
    return jv_obj(o);
}

/* ---------------------------------------------------------------- tick e saída */
void web_sw_pump(js_page_t *owner) {
    if (!owner || is_worker_page(owner)) return;
    for (reg_t *r = regs; r; r = r->next) {
        if (!r->page) continue;
        if (r->state == S_REDUNDANT) { kill_reg(r); continue; }
        if (strcmp(js_page_origin(owner), r->origin)) continue;
        if (!r->ticker) r->ticker = owner;
        if (r->ticker == owner) js_page_tick(r->page);
    }
}

void web_sw_drop(js_page_t *owner) {
    for (reg_t *r = regs; r; r = r->next) if (r->ticker == owner) r->ticker = NULL;
    for (pend_t **pp = &pends; *pp;) { if ((*pp)->page == owner) { pend_t *d = *pp; *pp = d->next; kfree(d); } else pp = &(*pp)->next; }
}

/* ---------------------------------------------------------------- registro no reino */
static const char *lazy_names[] = { "caches", "Cache", "CacheStorage", "ServiceWorker", "ServiceWorkerRegistration", "ServiceWorkerContainer" };
NATIVE(lazy_get) {
    UNUSED_ARGS;
    jsv b = bundle();
    if (b == JS_EXC) return JS_EXC;
    return js_getn(jv_obj(J.realm->global), lazy_names[self->u.nat.magic]);
}
NATIVE(lazy_set) {
    UNUSED_ARGS;
    jsobj *g = J.realm->global;
    jsstr *n = js_intern(lazy_names[self->u.nat.magic], -1);
    js_delprop(g, n);
    js_defprop(g, n, ARG(0), PF_NOENUM);
    return JS_UNDEF;
}
NATIVE(nav_sw) {
    UNUSED_ARGS;
    jsv b = bundle();
    if (b == JS_EXC) return JS_EXC;
    jsv f = js_getn(b, "container");
    return js_iscallable(f) ? js_call(f, JS_UNDEF, 0, NULL) : JS_UNDEF;
}
/* página controlada: o primeiro fetch carrega a API (que passa a interceptar) */
NATIVE(lazy_fetch) {
    UNUSED_ARGS;
    if (bundle() == JS_EXC) return JS_EXC;
    return js_call(js_getn(jv_obj(J.realm->global), "fetch"), thisv, argc, argv);
}

void js_sw_init(jsrealm *r) {
    jsobj *g = r->global;
    for (u32 i = 0; i < ARRAY_LEN(lazy_names); i++) {
        if (r->page && is_worker_page(r->page) && i >= 3) break;   /* worker: só caches */
        js_defaccessor(g, js_intern(lazy_names[i], -1), jv_obj(js_newnative(lazy_get, lazy_names[i], 0, (int)i)),
                       jv_obj(js_newnative(lazy_set, lazy_names[i], 1, (int)i)), PF_NOENUM);
    }
    jsv nav = js_getn(jv_obj(g), "navigator");
    if (jv_isobj(nav)) js_defaccessor(jv_o(nav), js_intern("serviceWorker", -1), jv_obj(js_newnative(nav_sw, "serviceWorker", 0, 0)), JS_UNDEF, PF_NOENUM);
    /* a página nasce controlada por um worker ativo do escopo: fetch passa por ele desde o primeiro script */
    if (r->page && !spawning && controller_of(r->page)) {
        jsv f = js_getn(jv_obj(g), "fetch");
        if (js_iscallable(f)) {
            js_defprop(g, js_intern("\x01" "fetch0", -1), f, PF_NOENUM);
            js_defprop(g, js_intern("fetch", -1), jv_obj(js_newnative(lazy_fetch, "fetch", 1, 0)), PF_NOENUM);
        }
    }
}
