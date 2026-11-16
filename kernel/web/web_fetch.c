/* mOS — Fetch e XMLHttpRequest do motor web (WHATWG Fetch e XHR, implementação própria): Headers com guarda e
 * combinação, Request e Response completos (corpo binário, clone, Response.json/error/redirect), FormData (com
 * <form>), fetch() com http(s), data:, blob: e file:, redirecionamento follow/error/manual, AbortController e
 * AbortSignal (abort, timeout, any), e XMLHttpRequest com responseType, eventos de progresso, upload, timeout,
 * abort e modo síncrono. A rede é o cliente HTTP do kernel (http_request). */
#include "js_int.h"
#include "web.h"
#include "net.h"
#include "vfs.h"
#include "crypto.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

static bool has_proto(jsobj *o, jsobj *p) {
    for (int i = 0; o && p && i < 32; i++, o = o->proto) if (o->proto == p) return true;
    return false;
}
static jsobj *extra_of(jsv v, jsobj *proto) {
    if (!jv_iscls(v, CL_HOST) || !has_proto(jv_o(v), proto) || !jv_isobj(jv_o(v)->u.host.extra)) return NULL;
    return jv_o(jv_o(v)->u.host.extra);
}
static jsobj *host_new(jsobj *proto, int nslots) {
    jsobj *o = js_newobj_cls(CL_HOST, proto);
    o->u.host.page = js_cur_page();
    jsobj *x = js_newarr(0);
    for (int i = 0; i < nslots; i++) js_arr_push(x, JS_UNDEF);
    o->u.host.extra = jv_obj(x);
    return o;
}
#define SLOT(x, i) js_arr_get((x), (i))
#define SET(x, i, v) js_arr_set((x), (i), (v))
static jsv S(const char *s) { return jv_str(js_strz(s)); }
static jsv reject_value(int type, const char *msg) { js_throw_err(type, "%s", msg); return js_catch(); }
static jsv dom_value(const char *name, const char *msg) { js_throw_dom(name, msg); return js_catch(); }

/* ============================================================ Headers */
enum { G_NONE, G_REQUEST, G_IMMUTABLE, G_RESPONSE, G_NOCORS };
enum { H_LIST, H_GUARD };

static bool is_token(const char *s, int n) {
    if (!n) return false;
    for (int i = 0; i < n; i++) {
        u8 c = (u8)s[i];
        if (!(isalnum(c) || strchr("!#$%&'*+-.^_`|~", c))) return false;
    }
    return true;
}
static bool http_ws(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

/* nome em minúsculas e valor normalizado; NULL + exceção se inválido */
static bool header_norm(jsv name, jsv value, jsstr **on, jsstr **ov) {
    jsstr *n = js_tostr(name);
    if (!n) return false;
    jsstr *v = js_tostr(value);
    if (!v) return false;
    if (!is_token(n->d, (int)n->len)) { js_throw_err(E_TYPE, "Headers: nome inválido '%s'", n->d); return false; }
    const char *a = v->d, *b = v->d + v->len;
    while (a < b && http_ws(*a)) a++;
    while (b > a && http_ws(b[-1])) b--;
    for (const char *q = a; q < b; q++) if (*q == 0 || *q == '\r' || *q == '\n') { js_throw_err(E_TYPE, "Headers: valor inválido para '%s'", n->d); return false; }
    char *low = kmalloc(n->len + 1);
    if (!low) { js_throw_err(E_RANGE, "sem memória"); return false; }
    for (u32 i = 0; i < n->len; i++) low[i] = (char)tolower((u8)n->d[i]);
    *on = js_strn(low, (int)n->len);
    kfree(low);
    *ov = js_strn(a, (int)(b - a));
    return true;
}

static bool forbidden_request(const char *n) {
    static const char *f[] = { "accept-charset", "accept-encoding", "access-control-request-headers", "access-control-request-method",
                               "connection", "content-length", "cookie", "cookie2", "date", "dnt", "expect", "host", "keep-alive",
                               "origin", "referer", "set-cookie", "te", "trailer", "transfer-encoding", "upgrade", "via" };
    for (u32 i = 0; i < ARRAY_LEN(f); i++) if (!strcmp(n, f[i])) return true;
    return !strncmp(n, "proxy-", 6) || !strncmp(n, "sec-", 4);
}

static bool guard_blocks(int guard, const char *n) {
    if (guard == G_REQUEST || guard == G_NOCORS) return forbidden_request(n);
    if (guard == G_RESPONSE) return !strcmp(n, "set-cookie") || !strcmp(n, "set-cookie2");
    return false;
}

static jsobj *headers_new(int guard) {
    jsobj *o = host_new(J.realm->p_headers, 2);
    jsobj *x = jv_o(o->u.host.extra);
    SET(x, H_LIST, jv_obj(js_newarr(0)));
    SET(x, H_GUARD, jv_int(guard));
    return o;
}
static jsobj *hlist(jsobj *hx) { return jv_o(SLOT(hx, H_LIST)); }

/* 1 ok, 0 ignorado, -1 exceção */
static int headers_append(jsobj *hx, jsv name, jsv value, bool replace) {
    jsstr *n, *v;
    if (!header_norm(name, value, &n, &v)) return -1;
    int guard = js_toint32(SLOT(hx, H_GUARD));
    if (guard == G_IMMUTABLE) { js_throw_err(E_TYPE, "Headers imutáveis"); return -1; }
    if (guard_blocks(guard, n->d)) return 0;
    jsobj *l = hlist(hx);
    if (replace) {
        jsobj *out = js_newarr(0);
        bool done = false;
        for (u32 i = 0; i + 1 < l->alen; i += 2) {
            if (js_str_eq(jv_s(l->arr[i]), n)) { if (!done) { js_arr_push(out, jv_str(n)); js_arr_push(out, jv_str(v)); done = true; } continue; }
            js_arr_push(out, l->arr[i]);
            js_arr_push(out, l->arr[i + 1]);
        }
        if (!done) { js_arr_push(out, jv_str(n)); js_arr_push(out, jv_str(v)); }
        SET(hx, H_LIST, jv_obj(out));
    } else { js_arr_push(l, jv_str(n)); js_arr_push(l, jv_str(v)); }
    return 1;
}

/* valor combinado ("a, b") ou NULL */
static jsstr *headers_get(jsobj *hx, const char *name) {
    jsobj *l = hlist(hx);
    sbuf_t sb = { 0 };
    bool any = false;
    for (u32 i = 0; i + 1 < l->alen; i += 2) {
        if (strcasecmp(jv_s(l->arr[i])->d, name)) continue;
        if (any) sb_puts(&sb, ", ");
        sb_putn(&sb, jv_s(l->arr[i + 1])->d, (int)jv_s(l->arr[i + 1])->len);
        any = true;
    }
    jsstr *r = any ? js_strn(sb.p ? sb.p : "", sb.len) : NULL;
    sb_free(&sb);
    return r;
}

/* preenche com HeadersInit (Headers, sequência de pares ou registro) */
static bool headers_fill(jsobj *hx, jsv init) {
    if (init == JS_UNDEF) return true;
    if (!jv_isobj(init)) { js_throw_err(E_TYPE, "Headers: inicialização inválida"); return false; }
    jsobj *src = extra_of(init, J.realm->p_headers);
    if (src) {
        jsobj *l = hlist(src);
        jsobj *copy = js_arr_from(l->arr, (int)l->alen);
        for (u32 i = 0; i + 1 < copy->alen; i += 2) if (headers_append(hx, copy->arr[i], copy->arr[i + 1], false) < 0) return false;
        return true;
    }
    jsv itf = js_getk(init, A[A_sym_iterator]);
    if (js_pending()) return false;
    if (js_iscallable(itf)) {
        jsv from = js_getn(jv_obj(J.realm->c_array), "from");
        jsv items = js_call(from, jv_obj(J.realm->c_array), 1, &init);
        if (items == JS_EXC) return false;
        u32 n = js_lengthof(items);
        for (u32 i = 0; i < n; i++) {
            jsv pair = js_getidx(items, i);
            jsv pa = js_call(from, jv_obj(J.realm->c_array), 1, &pair);
            if (pa == JS_EXC) return false;
            if (js_lengthof(pa) != 2) { js_throw_err(E_TYPE, "Headers: cada par precisa de 2 itens"); return false; }
            if (headers_append(hx, js_getidx(pa, 0), js_getidx(pa, 1), false) < 0) return false;
        }
        return true;
    }
    jsobj *keys = js_keylist(jv_o(init), true, false);
    for (u32 i = 0; keys && i < keys->alen; i++) {
        jsv v = js_get(init, keys->arr[i]);
        if (js_pending()) return false;
        if (headers_append(hx, keys->arr[i], v, false) < 0) return false;
    }
    return true;
}

/* "Nome: valor\r\n" para o cliente HTTP */
static void headers_wire(jsobj *hx, sbuf_t *o) {
    jsobj *l = hlist(hx);
    for (u32 i = 0; i + 1 < l->alen; i += 2) {
        sb_putn(o, jv_s(l->arr[i])->d, (int)jv_s(l->arr[i])->len);
        sb_puts(o, ": ");
        sb_putn(o, jv_s(l->arr[i + 1])->d, (int)jv_s(l->arr[i + 1])->len);
        sb_puts(o, "\r\n");
    }
}

/* cabeçalhos crus da resposta ("Nome: valor\r\n"); Set-Cookie nunca chega ao script */
static void headers_from_wire(jsobj *hx, const char *raw) {
    jsobj *l = hlist(hx);
    for (const char *p = raw ? raw : ""; *p;) {
        const char *e = strstr(p, "\r\n");
        int ln = e ? (int)(e - p) : (int)strlen(p);
        const char *c = NULL;
        for (int q = 0; q < ln; q++) if (p[q] == ':') { c = p + q; break; }
        if (c && c > p) {
            char *low = kmalloc((usize)(c - p) + 1);
            if (low) {
                for (int i = 0; i < c - p; i++) low[i] = (char)tolower((u8)p[i]);
                low[c - p] = 0;
                bool cookie = !strcmp(low, "set-cookie") || !strcmp(low, "set-cookie2");
                if (cookie) { kfree(low); p += ln + (e ? 2 : 0); continue; }
                js_arr_push(l, jv_str(js_strn(low, (int)(c - p))));
                kfree(low);
                const char *v = c + 1, *ve = p + ln;
                while (v < ve && http_ws(*v)) v++;
                while (ve > v && http_ws(ve[-1])) ve--;
                js_arr_push(l, jv_str(js_strn(v, (int)(ve - v))));
            }
        }
        p += ln + (e ? 2 : 0);
    }
}

/* lista ordenada e combinada para iteração (set-cookie fica separado) */
static jsobj *headers_sorted(jsobj *hx) {
    jsobj *l = hlist(hx), *names = js_newarr(0), *out = js_newarr(0);
    for (u32 i = 0; i + 1 < l->alen; i += 2) {
        bool seen = false;
        for (u32 k = 0; k < names->alen; k++) if (js_str_eq(jv_s(names->arr[k]), jv_s(l->arr[i]))) seen = true;
        if (!seen) js_arr_push(names, l->arr[i]);
    }
    for (u32 i = 1; i < names->alen; i++) {
        jsv t = names->arr[i];
        u32 j = i;
        while (j > 0 && js_str_cmp(jv_s(names->arr[j - 1]), jv_s(t)) > 0) { names->arr[j] = names->arr[j - 1]; j--; }
        names->arr[j] = t;
    }
    for (u32 k = 0; k < names->alen; k++) {
        jsstr *n = jv_s(names->arr[k]);
        if (!strcmp(n->d, "set-cookie")) {
            for (u32 i = 0; i + 1 < l->alen; i += 2)
                if (js_str_eq(jv_s(l->arr[i]), n)) { jsv pr[2] = { jv_str(n), l->arr[i + 1] }; js_arr_push(out, jv_obj(js_arr_from(pr, 2))); }
        } else {
            jsv pr[2] = { jv_str(n), jv_str(headers_get(hx, n->d)) };
            js_arr_push(out, jv_obj(js_arr_from(pr, 2)));
        }
    }
    return out;
}

NATIVE(headers_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor Headers exige 'new'");
    jsobj *o = headers_new(G_NONE);
    if (!headers_fill(jv_o(o->u.host.extra), ARG(0))) return JS_EXC;
    return jv_obj(o);
}

/* magic: 0 append 1 delete 2 get 3 has 4 set 5 getSetCookie 6 forEach 7 entries 8 keys 9 values */
NATIVE(headers_method) {
    UNUSED_ARGS;
    jsobj *hx = extra_of(thisv, J.realm->p_headers);
    if (!hx) return js_throw_err(E_TYPE, "Invocação ilegal: não é um Headers");
    int m = self->u.nat.magic;
    static const int need[] = { 2, 1, 1, 1, 2, 0, 1, 0, 0, 0 };
    if (argc < need[m]) return js_throw_err(E_TYPE, "Headers: faltam argumentos");
    switch (m) {
    case 0: case 4: return headers_append(hx, ARG(0), ARG(1), m == 4) < 0 ? JS_EXC : JS_UNDEF;
    case 1: case 2: case 3: {
        jsstr *n = js_tostr(ARG(0));
        if (!n) return JS_EXC;
        if (!is_token(n->d, (int)n->len)) return js_throw_err(E_TYPE, "Headers: nome inválido '%s'", n->d);
        if (m == 2) { jsstr *v = headers_get(hx, n->d); return v ? jv_str(v) : JS_NULL; }
        if (m == 3) return jv_bool(headers_get(hx, n->d) != NULL);
        int guard = js_toint32(SLOT(hx, H_GUARD));
        if (guard == G_IMMUTABLE) return js_throw_err(E_TYPE, "Headers imutáveis");
        jsobj *l = hlist(hx), *out = js_newarr(0);
        for (u32 i = 0; i + 1 < l->alen; i += 2)
            if (strcasecmp(jv_s(l->arr[i])->d, n->d)) { js_arr_push(out, l->arr[i]); js_arr_push(out, l->arr[i + 1]); }
        if (!guard_blocks(guard, n->d)) SET(hx, H_LIST, jv_obj(out));
        return JS_UNDEF;
    }
    case 5: {
        jsobj *l = hlist(hx), *r = js_newarr(0);
        for (u32 i = 0; i + 1 < l->alen; i += 2) if (!strcmp(jv_s(l->arr[i])->d, "set-cookie")) js_arr_push(r, l->arr[i + 1]);
        return jv_obj(r);
    }
    case 6: {
        if (!js_iscallable(ARG(0))) return js_throw_err(E_TYPE, "Headers.forEach: função esperada");
        jsobj *s = headers_sorted(hx);
        for (u32 i = 0; i < s->alen; i++) {
            jsv a[3] = { js_getidx(s->arr[i], 1), js_getidx(s->arr[i], 0), thisv };
            if (js_call(ARG(0), ARG(1), 3, a) == JS_EXC) return JS_EXC;
        }
        return JS_UNDEF;
    }
    default: {
        jsobj *s = headers_sorted(hx);
        if (m != 7) for (u32 i = 0; i < s->alen; i++) s->arr[i] = js_getidx(s->arr[i], m == 8 ? 0 : 1);
        return js_invoke(jv_obj(s), js_intern("values", -1), 0, NULL);
    }
    }
}

/* ============================================================ FormData */
/* extra: [lista nome, valor (string ou File), ...] */
static jsobj *fd_list(jsv v) { jsobj *x = extra_of(v, J.realm->p_formdata); return x ? jv_o(SLOT(x, 0)) : NULL; }

/* entrada: string ou Blob (vira File com nome) */
static bool fd_value(jsv v, jsv fname, bool has_name, jsv *out) {
    const u8 *d;
    u32 n;
    const char *type;
    if (js_blob_bytes(v, &d, &n, &type)) {
        jsv nm = fname;
        if (!has_name) {
            jsv cur = js_getn(v, "name");
            if (js_pending()) return false;
            nm = jv_isstr(cur) ? cur : S("blob");
        }
        jsstr *ns = js_tostr(nm);
        if (!ns) return false;
        jsv ctor = js_getn(jv_obj(J.realm->global), "File");
        jsv parts = jv_obj(js_arr_from(&v, 1));
        jsobj *opt = js_newobj(J.realm->p_object);
        js_defn(opt, "type", S(type), 0);
        jsv lm = js_getn(v, "lastModified");
        if (js_pending()) return false;
        if (lm != JS_UNDEF) js_defn(opt, "lastModified", lm, 0);
        jsv a[3] = { parts, jv_str(ns), jv_obj(opt) };
        jsv f = js_construct(ctor, 3, a, ctor);
        if (f == JS_EXC) return false;
        *out = f;
        return true;
    }
    jsstr *s = js_tostr(v);
    if (!s) return false;
    *out = jv_str(s);
    return true;
}

NATIVE(fd_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor FormData exige 'new'");
    jsobj *o = host_new(J.realm->p_formdata, 1);
    jsobj *l = js_newarr(0);
    SET(jv_o(o->u.host.extra), 0, jv_obj(l));
    if (ARG(0) != JS_UNDEF) {
        if (!js_form_entries(ARG(0), ARG(1), l)) return js_pending() ? JS_EXC : js_throw_err(E_TYPE, "FormData: o argumento não é um <form>");
    }
    return jv_obj(o);
}

/* magic: 0 append 1 delete 2 get 3 getAll 4 has 5 set 6 forEach 7 entries 8 keys 9 values */
NATIVE(fd_method) {
    UNUSED_ARGS;
    jsobj *l = fd_list(thisv);
    if (!l) return js_throw_err(E_TYPE, "Invocação ilegal: não é um FormData");
    int m = self->u.nat.magic;
    static const int need[] = { 2, 1, 1, 1, 1, 2, 1, 0, 0, 0 };
    if (argc < need[m]) return js_throw_err(E_TYPE, "FormData: faltam argumentos");
    jsstr *k = NULL;
    if (m <= 5) { k = js_tostr(ARG(0)); if (!k) return JS_EXC; }
    switch (m) {
    case 0: case 5: {
        jsv v;
        if (!fd_value(ARG(1), ARG(2), argc > 2, &v)) return JS_EXC;
        if (m == 0) { js_arr_push(l, jv_str(k)); js_arr_push(l, v); return JS_UNDEF; }
        jsobj *out = js_newarr(0);
        bool done = false;
        for (u32 i = 0; i + 1 < l->alen; i += 2) {
            if (js_str_eq(jv_s(l->arr[i]), k)) { if (!done) { js_arr_push(out, jv_str(k)); js_arr_push(out, v); done = true; } continue; }
            js_arr_push(out, l->arr[i]); js_arr_push(out, l->arr[i + 1]);
        }
        if (!done) { js_arr_push(out, jv_str(k)); js_arr_push(out, v); }
        SET(jv_o(jv_o(thisv)->u.host.extra), 0, jv_obj(out));
        return JS_UNDEF;
    }
    case 1: {
        jsobj *out = js_newarr(0);
        for (u32 i = 0; i + 1 < l->alen; i += 2) if (!js_str_eq(jv_s(l->arr[i]), k)) { js_arr_push(out, l->arr[i]); js_arr_push(out, l->arr[i + 1]); }
        SET(jv_o(jv_o(thisv)->u.host.extra), 0, jv_obj(out));
        return JS_UNDEF;
    }
    case 2: for (u32 i = 0; i + 1 < l->alen; i += 2) if (js_str_eq(jv_s(l->arr[i]), k)) return l->arr[i + 1]; return JS_NULL;
    case 3: { jsobj *r = js_newarr(0); for (u32 i = 0; i + 1 < l->alen; i += 2) if (js_str_eq(jv_s(l->arr[i]), k)) js_arr_push(r, l->arr[i + 1]); return jv_obj(r); }
    case 4: for (u32 i = 0; i + 1 < l->alen; i += 2) if (js_str_eq(jv_s(l->arr[i]), k)) return JS_TRUE; return JS_FALSE;
    case 6: {
        if (!js_iscallable(ARG(0))) return js_throw_err(E_TYPE, "FormData.forEach: função esperada");
        jsobj *c = js_arr_from(l->arr, (int)l->alen);
        for (u32 i = 0; i + 1 < c->alen; i += 2) { jsv a[3] = { c->arr[i + 1], c->arr[i], thisv }; if (js_call(ARG(0), ARG(1), 3, a) == JS_EXC) return JS_EXC; }
        return JS_UNDEF;
    }
    default: {
        jsobj *r = js_newarr(0);
        for (u32 i = 0; i + 1 < l->alen; i += 2) {
            if (m == 7) { jsv pr[2] = { l->arr[i], l->arr[i + 1] }; js_arr_push(r, jv_obj(js_arr_from(pr, 2))); }
            else js_arr_push(r, l->arr[m == 8 ? i : i + 1]);
        }
        return js_invoke(jv_obj(r), js_intern("values", -1), 0, NULL);
    }
    }
}

/* multipart/form-data: escapa " e quebras nos nomes, como os navegadores */
static void mp_name(sbuf_t *o, jsstr *s) {
    for (u32 i = 0; i < s->len; i++) {
        char c = s->d[i];
        if (c == '"') sb_puts(o, "%22"); else if (c == '\r') sb_puts(o, "%0D"); else if (c == '\n') sb_puts(o, "%0A"); else sb_putc(o, c);
    }
}
static jsobj *fd_encode(jsobj *l, char *boundary) {
    u8 r[12];
    crypto_random(r, sizeof r);
    int k = snprintf(boundary, 70, "----mOSFormBoundary");
    static const char al[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < 12; i++) boundary[k++] = al[r[i] % 62];
    boundary[k] = 0;
    sbuf_t o = { 0 };
    for (u32 i = 0; i + 1 < l->alen; i += 2) {
        sb_puts(&o, "--"); sb_puts(&o, boundary); sb_puts(&o, "\r\nContent-Disposition: form-data; name=\"");
        mp_name(&o, jv_s(l->arr[i]));
        sb_putc(&o, '"');
        const u8 *d;
        u32 n;
        const char *type;
        if (js_blob_bytes(l->arr[i + 1], &d, &n, &type)) {
            jsv fn = js_getn(l->arr[i + 1], "name");
            if (js_pending()) js_catch();
            sb_puts(&o, "; filename=\"");
            if (jv_isstr(fn)) mp_name(&o, jv_s(fn));
            sb_puts(&o, "\"\r\nContent-Type: ");
            sb_puts(&o, *type ? type : "application/octet-stream");
            sb_puts(&o, "\r\n\r\n");
            if (n) sb_putn(&o, (const char *)d, (int)n);
        } else {
            sb_puts(&o, "\r\n\r\n");
            jsstr *v = jv_s(l->arr[i + 1]);
            /* quebras normalizadas para CRLF */
            for (u32 q = 0; q < v->len; q++) {
                if (v->d[q] == '\r' && q + 1 < v->len && v->d[q + 1] == '\n') { sb_puts(&o, "\r\n"); q++; }
                else if (v->d[q] == '\r' || v->d[q] == '\n') sb_puts(&o, "\r\n");
                else sb_putc(&o, v->d[q]);
            }
        }
        sb_puts(&o, "\r\n");
    }
    sb_puts(&o, "--"); sb_puts(&o, boundary); sb_puts(&o, "--\r\n");
    jsobj *b = js_abuf_new((u32)o.len);
    if (b && o.len) memcpy(b->u.buf.data, o.p, (usize)o.len);
    sb_free(&o);
    return b;
}

/* corpo multipart → FormData; false se malformado */
static bool fd_decode(const u8 *d, u32 n, const char *ctype, jsobj *l) {
    const char *bp = strstr(ctype, "boundary=");
    if (!bp) return false;
    bp += 9;
    char bnd[200];
    int bl = 0;
    if (*bp == '"') { bp++; while (*bp && *bp != '"' && bl < 190) bnd[bl++] = *bp++; }
    else while (*bp && *bp != ';' && *bp != ' ' && bl < 190) bnd[bl++] = *bp++;
    bnd[bl] = 0;
    char delim[210];
    int dl = snprintf(delim, sizeof delim, "--%s", bnd);
    const u8 *p = d, *e = d + n;
    /* acha a primeira delimitação */
    while (p + dl <= e && memcmp(p, delim, (usize)dl)) p++;
    if (p + dl > e) return false;
    for (;;) {
        p += dl;
        if (p + 2 <= e && p[0] == '-' && p[1] == '-') return true;
        if (p + 2 > e || p[0] != '\r' || p[1] != '\n') return false;
        p += 2;
        /* cabeçalhos da parte */
        char name[256] = "", fname[256] = "", type[128] = "";
        bool has_fname = false;
        for (;;) {
            const u8 *le = p;
            while (le + 1 < e && !(le[0] == '\r' && le[1] == '\n')) le++;
            if (le + 1 >= e) return false;
            if (le == p) { p += 2; break; }
            char line[600];
            int ln = (int)MIN((usize)(le - p), sizeof line - 1);
            memcpy(line, p, (usize)ln);
            line[ln] = 0;
            if (!strncasecmp(line, "content-disposition:", 20)) {
                char *q = strstr(line, " name=\"");
                if (!q) q = strstr(line, ";name=\"");
                if (q) { q += 7; int k = 0; while (*q && *q != '"' && k < 255) name[k++] = *q++; name[k] = 0; }
                q = strstr(line, "filename=\"");
                if (q) { has_fname = true; q += 10; int k = 0; while (*q && *q != '"' && k < 255) fname[k++] = *q++; fname[k] = 0; }
            } else if (!strncasecmp(line, "content-type:", 13)) {
                const char *q = line + 13;
                while (*q == ' ') q++;
                snprintf(type, sizeof type, "%s", q);
            }
            p = le + 2;
        }
        /* conteúdo até CRLF + delimitação */
        const u8 *ce = p;
        while (ce + 2 + dl <= e && !(ce[0] == '\r' && ce[1] == '\n' && !memcmp(ce + 2, delim, (usize)dl))) ce++;
        if (ce + 2 + dl > e) return false;
        js_arr_push(l, jv_str(js_strz(name)));
        if (has_fname) {
            jsobj *b = js_blob_new(p, (u32)(ce - p), type);
            jsv parts = jv_obj(b), ctor = js_getn(jv_obj(J.realm->global), "File");
            jsobj *opt = js_newobj(J.realm->p_object);
            js_defn(opt, "type", S(type), 0);
            jsv a[3] = { jv_obj(js_arr_from(&parts, 1)), S(fname), jv_obj(opt) };
            jsv f = js_construct(ctor, 3, a, ctor);
            if (f == JS_EXC) return false;
            js_arr_push(l, f);
        } else {
            bool bad;
            jsstr *s = js_utf8_decode(p, (u32)(ce - p), false, &bad);
            js_arr_push(l, jv_str(s ? s : A[A_empty]));
        }
        p = ce + 2;
    }
}

/* ============================================================ corpos */
/* BodyInit → bytes e tipo padrão. 1 ok, -1 exceção */
static int extract_body(jsv v, jsobj **buf, jsstr **ctype) {
    *ctype = NULL;
    const u8 *d;
    u32 n;
    const char *type;
    if (js_blob_bytes(v, &d, &n, &type)) {
        *buf = js_abuf_new(n);
        if (!*buf) return -1;
        if (n) memcpy((*buf)->u.buf.data, d, n);
        if (*type) *ctype = js_strz(type);
        return 1;
    }
    if (jv_iscls(v, CL_ARRAYBUFFER) || jv_iscls(v, CL_TYPED) || jv_iscls(v, CL_DATAVIEW)) {
        const u8 *src = NULL;
        u32 len = 0;
        jsobj *o = jv_o(v);
        if (o->cls == CL_ARRAYBUFFER) { src = o->u.buf.data; len = o->u.buf.detached ? 0 : o->u.buf.len; }
        else if (o->u.ta.buffer && !o->u.ta.buffer->u.buf.detached) {
            src = o->u.ta.buffer->u.buf.data + o->u.ta.off;
            len = o->cls == CL_DATAVIEW ? o->u.ta.len : js_typed_length(o) * (u32)js_typed_elsize(o->u.ta.kind);
        }
        *buf = js_abuf_new(len);
        if (!*buf) return -1;
        if (len) memcpy((*buf)->u.buf.data, src, len);
        return 1;
    }
    jsstr *usp = js_usp_string(v);
    if (usp) {
        *buf = js_abuf_new(usp->len);
        if (!*buf) return -1;
        if (usp->len) memcpy((*buf)->u.buf.data, usp->d, usp->len);
        *ctype = js_strz("application/x-www-form-urlencoded;charset=UTF-8");
        return 1;
    }
    jsobj *fl = fd_list(v);
    if (fl) {
        char bnd[80];
        *buf = fd_encode(fl, bnd);
        if (!*buf) return -1;
        *ctype = js_str_fmt("multipart/form-data; boundary=%s", bnd);
        return 1;
    }
    jsstr *s = js_tostr(v);
    if (!s) return -1;
    /* USVString em UTF-8 */
    sbuf_t o = { 0 };
    for (u32 k = 0; k < s->len; k++) {
        u8 c = (u8)s->d[k];
        if (c == 0xED && k + 2 < s->len && (u8)s->d[k + 1] >= 0xA0) { sb_puts(&o, "\xEF\xBF\xBD"); k += 2; }
        else sb_putc(&o, (char)c);
    }
    *buf = js_abuf_new((u32)o.len);
    if (*buf && o.len) memcpy((*buf)->u.buf.data, o.p, (usize)o.len);
    sb_free(&o);
    if (!*buf) return -1;
    *ctype = js_strz("text/plain;charset=UTF-8");
    return 1;
}

/* slots comuns de Request/Response */
enum { B_HDR, B_BODY, B_USED };
enum { R_URL = 3, R_METHOD, R_SIGNAL, R_MODE, R_CRED, R_CACHE, R_REDIRECT, R_REFERRER, R_INTEGRITY, R_KEEPALIVE, R_REFPOL, R_DEST, R_NSLOTS };
enum { S_URL = 3, S_STATUS, S_STATUSTEXT, S_TYPE, S_REDIRECTED, S_NSLOTS };

static jsobj *body_owner(jsv v) {
    jsobj *x = extra_of(v, J.realm->p_request);
    if (!x) x = extra_of(v, J.realm->p_response);
    return x;
}

/* magic: 0 text 1 json 2 arrayBuffer 3 blob 4 bytes 5 formData */
NATIVE(body_read) {
    UNUSED_ARGS;
    jsobj *x = body_owner(thisv);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal: não é um Request nem um Response");
    jsobj *pr = js_promise_new();
    if (js_truthy(SLOT(x, B_USED))) { js_promise_reject(pr, reject_value(E_TYPE, "o corpo já foi lido")); return jv_obj(pr); }
    jsv bv = SLOT(x, B_BODY);
    const u8 *d = NULL;
    u32 n = 0;
    if (jv_iscls(bv, CL_ARRAYBUFFER)) { d = jv_o(bv)->u.buf.data; n = jv_o(bv)->u.buf.len; SET(x, B_USED, JS_TRUE); }
    jsstr *ct = headers_get(jv_o(jv_o(SLOT(x, B_HDR))->u.host.extra), "content-type");
    switch (self->u.nat.magic) {
    case 0: case 1: {
        bool bad;
        jsstr *s = js_utf8_decode(d, n, false, &bad);
        if (!s) s = A[A_empty];
        if (self->u.nat.magic == 0) { js_promise_resolve(pr, jv_str(s)); break; }
        jsv v = js_json_parse(s, JS_UNDEF);
        if (v == JS_EXC) js_promise_reject(pr, js_catch()); else js_promise_resolve(pr, v);
        break;
    }
    case 2: case 4: {
        jsobj *b = js_abuf_new(n);
        if (!b) { js_promise_reject(pr, js_catch()); break; }
        if (n) memcpy(b->u.buf.data, d, n);
        js_promise_resolve(pr, self->u.nat.magic == 2 ? jv_obj(b) : jv_obj(js_typed_new(TA_UINT8, b, 0, n)));
        break;
    }
    case 3: {
        char type[200] = "";
        if (ct) { int k = 0; for (u32 i = 0; i < ct->len && k < 199; i++) { u8 c = (u8)ct->d[i]; if (c >= 0x20 && c <= 0x7E) type[k++] = (char)tolower(c); } type[k] = 0; }
        js_promise_resolve(pr, jv_obj(js_blob_new(d, n, type)));
        break;
    }
    case 5: {
        jsobj *o = host_new(J.realm->p_formdata, 1);
        jsobj *l = js_newarr(0);
        SET(jv_o(o->u.host.extra), 0, jv_obj(l));
        const char *c = ct ? ct->d : "";
        bool ok = true;
        if (!strncasecmp(c, "multipart/form-data", 19)) ok = fd_decode(d, n, c, l);
        else if (!strncasecmp(c, "application/x-www-form-urlencoded", 33)) {
            jsv ctor = js_getn(jv_obj(J.realm->global), "URLSearchParams");
            bool bad;
            jsstr *s = js_utf8_decode(d, n, false, &bad);
            jsv a = jv_str(s ? s : A[A_empty]);
            jsv usp = js_construct(ctor, 1, &a, ctor);
            if (usp == JS_EXC) { js_promise_reject(pr, js_catch()); break; }
            jsv ents = js_invoke(usp, js_intern("entries", -1), 0, NULL);
            jsv from = js_getn(jv_obj(J.realm->c_array), "from");
            jsv arr = js_call(from, jv_obj(J.realm->c_array), 1, &ents);
            if (arr == JS_EXC) { js_promise_reject(pr, js_catch()); break; }
            for (u32 i = 0; i < js_lengthof(arr); i++) { jsv pair = js_getidx(arr, i); js_arr_push(l, js_getidx(pair, 0)); js_arr_push(l, js_getidx(pair, 1)); }
        } else ok = false;
        if (!ok) js_promise_reject(pr, reject_value(E_TYPE, "formData: corpo não é multipart/form-data nem urlencoded"));
        else js_promise_resolve(pr, jv_obj(o));
        break;
    }
    }
    return jv_obj(pr);
}

NATIVE(body_used_get) {
    UNUSED_ARGS;
    jsobj *x = body_owner(thisv);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal");
    return jv_bool(js_truthy(SLOT(x, B_USED)));
}

/* ============================================================ Request */
/* DELETE, GET, HEAD, OPTIONS, POST e PUT viram maiúsculas; os outros (PATCH inclusive) ficam como vieram */
static const char *norm_method(jsstr *m, char *buf) {
    static const char *std[] = { "DELETE", "GET", "HEAD", "OPTIONS", "POST", "PUT" };
    for (u32 i = 0; i < ARRAY_LEN(std); i++) if (!strcasecmp(m->d, std[i])) { strcpy(buf, std[i]); return buf; }
    return m->d;
}

static bool pick_enum(jsv init, const char *key, const char **vals, int n, jsv *out) {
    jsv v = js_getn(init, key);
    if (js_pending()) return false;
    if (v == JS_UNDEF) return true;
    jsstr *s = js_tostr(v);
    if (!s) return false;
    for (int i = 0; i < n; i++) if (!strcmp(s->d, vals[i])) { *out = jv_str(s); return true; }
    js_throw_err(E_TYPE, "Request: valor inválido para %s: '%s'", key, s->d);
    return false;
}

static jsobj *make_request(jsv input, jsv init) {
    js_page_t *page = js_cur_page();
    jsobj *src = extra_of(input, J.realm->p_request);
    jsobj *o = host_new(J.realm->p_request, R_NSLOTS);
    jsobj *x = jv_o(o->u.host.extra);
    jsobj *h = headers_new(G_REQUEST);
    SET(x, B_HDR, jv_obj(h));
    SET(x, B_BODY, JS_NULL);
    SET(x, B_USED, JS_FALSE);
    static const char *modes[] = { "cors", "no-cors", "same-origin", "navigate" }, *creds[] = { "omit", "same-origin", "include" },
                      *caches[] = { "default", "no-store", "reload", "no-cache", "force-cache", "only-if-cached" },
                      *redirs[] = { "follow", "error", "manual" },
                      *pols[] = { "", "no-referrer", "no-referrer-when-downgrade", "same-origin", "origin", "strict-origin",
                                  "origin-when-cross-origin", "strict-origin-when-cross-origin", "unsafe-url" };
    if (src) {
        for (int i = R_URL; i < R_NSLOTS; i++) SET(x, i, SLOT(src, i));
        if (!headers_fill(jv_o(h->u.host.extra), SLOT(src, B_HDR))) return NULL;
        if (js_truthy(SLOT(src, B_USED)) && SLOT(src, B_BODY) != JS_NULL) { js_throw_err(E_TYPE, "Request: o corpo da origem já foi lido"); return NULL; }
        SET(x, B_BODY, SLOT(src, B_BODY));
    } else {
        jsstr *u = js_tostr(input);
        if (!u) return NULL;
        char url[2048];
        if (!web_url_parse(u->d, js_page_base(page), url, sizeof url)) { js_throw_err(E_TYPE, "Request: URL inválido '%s'", u->d); return NULL; }
        if (strstr(url, "://") && strchr(strstr(url, "://") + 3, '@') && strchr(strstr(url, "://") + 3, '@') < strchr(strstr(url, "://") + 3, '/')) {
            js_throw_err(E_TYPE, "Request: URL com credenciais não é permitido"); return NULL;
        }
        SET(x, R_URL, S(url));
        SET(x, R_METHOD, S("GET"));
        SET(x, R_MODE, S("cors"));
        SET(x, R_CRED, S("same-origin"));
        SET(x, R_CACHE, S("default"));
        SET(x, R_REDIRECT, S("follow"));
        SET(x, R_REFERRER, S("about:client"));
        SET(x, R_INTEGRITY, S(""));
        SET(x, R_KEEPALIVE, JS_FALSE);
        SET(x, R_REFPOL, S(""));
        SET(x, R_DEST, S(""));
        SET(x, R_SIGNAL, JS_NULL);
    }
    if (jv_isobj(init)) {
        jsv m = js_getn(init, "method");
        if (js_pending()) return NULL;
        if (m != JS_UNDEF) {
            jsstr *ms = js_tostr(m);
            if (!ms) return NULL;
            if (!is_token(ms->d, (int)ms->len)) { js_throw_err(E_TYPE, "Request: método inválido '%s'", ms->d); return NULL; }
            if (!strcasecmp(ms->d, "CONNECT") || !strcasecmp(ms->d, "TRACE") || !strcasecmp(ms->d, "TRACK")) { js_throw_err(E_TYPE, "Request: método proibido '%s'", ms->d); return NULL; }
            char buf[16];
            SET(x, R_METHOD, S(norm_method(ms, buf)));
        }
        jsv v;
        v = SLOT(x, R_MODE); if (!pick_enum(init, "mode", modes, 4, &v)) return NULL;
        if (jv_isstr(v) && !strcmp(jv_s(v)->d, "navigate") && v != SLOT(x, R_MODE)) { js_throw_err(E_TYPE, "Request: modo 'navigate' não é permitido"); return NULL; }
        SET(x, R_MODE, v);
        v = SLOT(x, R_CRED); if (!pick_enum(init, "credentials", creds, 3, &v)) return NULL; SET(x, R_CRED, v);
        v = SLOT(x, R_CACHE); if (!pick_enum(init, "cache", caches, 6, &v)) return NULL; SET(x, R_CACHE, v);
        v = SLOT(x, R_REDIRECT); if (!pick_enum(init, "redirect", redirs, 3, &v)) return NULL; SET(x, R_REDIRECT, v);
        v = SLOT(x, R_REFPOL); if (!pick_enum(init, "referrerPolicy", pols, 9, &v)) return NULL; SET(x, R_REFPOL, v);
        jsv integ = js_getn(init, "integrity"), keep = js_getn(init, "keepalive"), ref = js_getn(init, "referrer"), sig = js_getn(init, "signal");
        if (js_pending()) return NULL;
        if (integ != JS_UNDEF) { jsstr *s = js_tostr(integ); if (!s) return NULL; SET(x, R_INTEGRITY, jv_str(s)); }
        if (keep != JS_UNDEF) SET(x, R_KEEPALIVE, jv_bool(js_truthy(keep)));
        if (ref != JS_UNDEF) {
            jsstr *s = js_tostr(ref);
            if (!s) return NULL;
            if (!*s->d) SET(x, R_REFERRER, S(""));
            else {
                char ru[2048];
                if (!web_url_parse(s->d, js_page_base(page), ru, sizeof ru)) { js_throw_err(E_TYPE, "Request: referrer inválido"); return NULL; }
                SET(x, R_REFERRER, S(ru));
            }
        }
        if (sig != JS_UNDEF) {
            if (sig != JS_NULL && !extra_of(sig, J.realm->p_abortsig)) { js_throw_err(E_TYPE, "Request: signal não é um AbortSignal"); return NULL; }
            SET(x, R_SIGNAL, sig);
        }
        jsv hv = js_getn(init, "headers");
        if (js_pending()) return NULL;
        if (hv != JS_UNDEF) {
            jsobj *nh = headers_new(jv_isstr(SLOT(x, R_MODE)) && !strcmp(jv_s(SLOT(x, R_MODE))->d, "no-cors") ? G_NOCORS : G_REQUEST);
            if (!headers_fill(jv_o(nh->u.host.extra), hv)) return NULL;
            h = nh;
            SET(x, B_HDR, jv_obj(h));
        }
        jsv bv = js_getn(init, "body");
        if (js_pending()) return NULL;
        if (!jv_isnullish(bv)) {
            const char *meth = jv_s(SLOT(x, R_METHOD))->d;
            if (!strcmp(meth, "GET") || !strcmp(meth, "HEAD")) { js_throw_err(E_TYPE, "Request: GET/HEAD não podem ter corpo"); return NULL; }
            jsobj *buf;
            jsstr *ct;
            if (extract_body(bv, &buf, &ct) < 0) return NULL;
            SET(x, B_BODY, jv_obj(buf));
            if (ct && !headers_get(jv_o(h->u.host.extra), "content-type")) headers_append(jv_o(h->u.host.extra), S("Content-Type"), jv_str(ct), false);
        }
    }
    /* sinal próprio que segue o do pedido de origem */
    jsv sig = SLOT(x, R_SIGNAL);
    jsobj *own = js_abort_signal_new();
    if (extra_of(sig, J.realm->p_abortsig)) js_abort_follow(own, sig);
    SET(x, R_SIGNAL, jv_obj(own));
    if (src && SLOT(src, B_BODY) != JS_NULL && (!jv_isobj(init) || SLOT(x, B_BODY) == SLOT(src, B_BODY))) SET(src, B_USED, JS_TRUE);
    return o;
}

NATIVE(request_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor Request exige 'new'");
    if (argc < 1) return js_throw_err(E_TYPE, "Request: é necessário 1 argumento");
    jsobj *o = make_request(ARG(0), ARG(1));
    return o ? jv_obj(o) : JS_EXC;
}

static const char *req_names[] = { NULL, NULL, NULL, "url", "method", "signal", "mode", "credentials", "cache", "redirect", "referrer",
                                   "integrity", "keepalive", "referrerPolicy", "destination" };

NATIVE(request_get) {
    UNUSED_ARGS;
    jsobj *x = extra_of(thisv, J.realm->p_request);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal: não é um Request");
    int m = self->u.nat.magic;
    if (m == B_HDR) return SLOT(x, B_HDR);
    return SLOT(x, m);
}

/* clone: magic 0 Request, 1 Response */
NATIVE(body_clone) {
    UNUSED_ARGS;
    jsobj *proto = self->u.nat.magic ? J.realm->p_response : J.realm->p_request;
    jsobj *x = extra_of(thisv, proto);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal");
    if (js_truthy(SLOT(x, B_USED))) return js_throw_err(E_TYPE, "clone: o corpo já foi lido");
    int ns = self->u.nat.magic ? S_NSLOTS : R_NSLOTS;
    jsobj *o = host_new(proto, ns);
    jsobj *y = jv_o(o->u.host.extra);
    for (int i = 0; i < ns; i++) SET(y, i, SLOT(x, i));
    jsobj *hx = jv_o(jv_o(SLOT(x, B_HDR))->u.host.extra);
    jsobj *h = headers_new(js_toint32(SLOT(hx, H_GUARD)));
    jsobj *hl = hlist(hx);
    SET(jv_o(h->u.host.extra), H_LIST, jv_obj(js_arr_from(hl->arr, (int)hl->alen)));
    SET(y, B_HDR, jv_obj(h));
    if (!self->u.nat.magic) {
        jsobj *own = js_abort_signal_new();
        js_abort_follow(own, SLOT(x, R_SIGNAL));
        SET(y, R_SIGNAL, jv_obj(own));
    }
    return jv_obj(o);
}

/* ============================================================ Response */
static const char *status_text(int s) {
    static const struct { int s; const char *t; } tab[] = {
        { 100, "Continue" }, { 101, "Switching Protocols" }, { 200, "OK" }, { 201, "Created" }, { 202, "Accepted" }, { 203, "Non-Authoritative Information" },
        { 204, "No Content" }, { 205, "Reset Content" }, { 206, "Partial Content" }, { 300, "Multiple Choices" }, { 301, "Moved Permanently" },
        { 302, "Found" }, { 303, "See Other" }, { 304, "Not Modified" }, { 307, "Temporary Redirect" }, { 308, "Permanent Redirect" },
        { 400, "Bad Request" }, { 401, "Unauthorized" }, { 402, "Payment Required" }, { 403, "Forbidden" }, { 404, "Not Found" },
        { 405, "Method Not Allowed" }, { 406, "Not Acceptable" }, { 408, "Request Timeout" }, { 409, "Conflict" }, { 410, "Gone" },
        { 411, "Length Required" }, { 412, "Precondition Failed" }, { 413, "Payload Too Large" }, { 414, "URI Too Long" },
        { 415, "Unsupported Media Type" }, { 416, "Range Not Satisfiable" }, { 417, "Expectation Failed" }, { 418, "I'm a teapot" },
        { 422, "Unprocessable Entity" }, { 425, "Too Early" }, { 426, "Upgrade Required" }, { 428, "Precondition Required" },
        { 429, "Too Many Requests" }, { 431, "Request Header Fields Too Large" }, { 451, "Unavailable For Legal Reasons" },
        { 500, "Internal Server Error" }, { 501, "Not Implemented" }, { 502, "Bad Gateway" }, { 503, "Service Unavailable" },
        { 504, "Gateway Timeout" }, { 505, "HTTP Version Not Supported" } };
    for (u32 i = 0; i < ARRAY_LEN(tab); i++) if (tab[i].s == s) return tab[i].t;
    return "";
}

static jsobj *response_new(int status, const char *stext, const char *type, const char *url, bool redirected, int guard) {
    jsobj *o = host_new(J.realm->p_response, S_NSLOTS);
    jsobj *x = jv_o(o->u.host.extra);
    SET(x, B_HDR, jv_obj(headers_new(guard)));
    SET(x, B_BODY, JS_NULL);
    SET(x, B_USED, JS_FALSE);
    SET(x, S_URL, S(url ? url : ""));
    SET(x, S_STATUS, jv_int(status));
    SET(x, S_STATUSTEXT, S(stext ? stext : ""));
    SET(x, S_TYPE, S(type));
    SET(x, S_REDIRECTED, jv_bool(redirected));
    return o;
}

static bool null_body_status(int s) { return s == 101 || s == 103 || s == 204 || s == 205 || s == 304; }

/* init do Response: status, statusText, headers. false = exceção */
static bool response_init(jsobj *x, jsv init, int *status) {
    *status = 200;
    if (jv_isnullish(init)) return true;
    if (!jv_isobj(init)) { js_throw_err(E_TYPE, "Response: init inválido"); return false; }
    jsv st = js_getn(init, "status"), tx = js_getn(init, "statusText"), hv = js_getn(init, "headers");
    if (js_pending()) return false;
    if (st != JS_UNDEF) {
        double d = js_tonum(st);
        if (js_pending()) return false;
        if (!(d >= 200 && d <= 599) || d != (int)d) { js_throw_err(E_RANGE, "Response: status %g fora de 200..599", d); return false; }
        *status = (int)d;
        SET(x, S_STATUS, jv_int(*status));
    }
    if (tx != JS_UNDEF) {
        jsstr *s = js_tostr(tx);
        if (!s) return false;
        for (u32 i = 0; i < s->len; i++) if (s->d[i] == '\r' || s->d[i] == '\n') { js_throw_err(E_TYPE, "Response: statusText inválido"); return false; }
        SET(x, S_STATUSTEXT, jv_str(s));
    }
    if (hv != JS_UNDEF && !headers_fill(jv_o(jv_o(SLOT(x, B_HDR))->u.host.extra), hv)) return false;
    return true;
}

NATIVE(response_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor Response exige 'new'");
    jsobj *o = response_new(200, "", "default", "", false, G_RESPONSE);
    jsobj *x = jv_o(o->u.host.extra);
    int status;
    if (!response_init(x, ARG(1), &status)) return JS_EXC;
    if (!jv_isnullish(ARG(0))) {
        if (null_body_status(status)) return js_throw_err(E_TYPE, "Response: status %d não pode ter corpo", status);
        jsobj *buf;
        jsstr *ct;
        if (extract_body(ARG(0), &buf, &ct) < 0) return JS_EXC;
        SET(x, B_BODY, jv_obj(buf));
        jsobj *hx = jv_o(jv_o(SLOT(x, B_HDR))->u.host.extra);
        if (ct && !headers_get(hx, "content-type")) headers_append(hx, S("Content-Type"), jv_str(ct), false);
    }
    return jv_obj(o);
}

/* estáticos: magic 0 error 1 redirect 2 json */
NATIVE(response_static) {
    UNUSED_ARGS;
    int m = self->u.nat.magic;
    if (m == 0) {
        jsobj *o = response_new(0, "", "error", "", false, G_IMMUTABLE);
        return jv_obj(o);
    }
    if (m == 1) {
        jsstr *u = js_tostr(ARG(0));
        if (!u) return JS_EXC;
        char url[2048];
        if (!web_url_parse(u->d, js_page_base(js_cur_page()), url, sizeof url)) return js_throw_err(E_TYPE, "Response.redirect: URL inválido");
        int st = 302;
        if (ARG(1) != JS_UNDEF) { st = js_toint32(ARG(1)); if (js_pending()) return JS_EXC; }
        if (st != 301 && st != 302 && st != 303 && st != 307 && st != 308) return js_throw_err(E_RANGE, "Response.redirect: status %d inválido", st);
        jsobj *o = response_new(st, "", "default", "", false, G_IMMUTABLE);
        jsobj *hl = hlist(jv_o(jv_o(SLOT(jv_o(o->u.host.extra), B_HDR))->u.host.extra));
        js_arr_setlen(hl, 0);
        js_arr_push(hl, S("location"));
        js_arr_push(hl, S(url));
        return jv_obj(o);
    }
    jsv json = js_getn(jv_obj(J.realm->global), "JSON");
    jsv txt = js_invoke(json, js_intern("stringify", -1), 1, argv);
    if (txt == JS_EXC) return JS_EXC;
    if (!jv_isstr(txt)) return js_throw_err(E_TYPE, "Response.json: o valor não vira JSON");
    jsobj *o = response_new(200, "", "default", "", false, G_RESPONSE);
    jsobj *x = jv_o(o->u.host.extra);
    int status;
    if (!response_init(x, ARG(1), &status)) return JS_EXC;
    if (null_body_status(status)) return js_throw_err(E_TYPE, "Response.json: status %d não pode ter corpo", status);
    jsobj *buf;
    jsstr *ct;
    if (extract_body(txt, &buf, &ct) < 0) return JS_EXC;
    SET(x, B_BODY, jv_obj(buf));
    jsobj *hx = jv_o(jv_o(SLOT(x, B_HDR))->u.host.extra);
    if (!headers_get(hx, "content-type")) headers_append(hx, S("Content-Type"), S("application/json"), false);
    return jv_obj(o);
}

/* getters: 0 headers 3 url 4 status 5 statusText 6 type 7 redirected 8 ok */
NATIVE(response_get) {
    UNUSED_ARGS;
    jsobj *x = extra_of(thisv, J.realm->p_response);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal: não é um Response");
    int m = self->u.nat.magic;
    if (m == 8) { int s = js_toint32(SLOT(x, S_STATUS)); return jv_bool(s >= 200 && s <= 299); }
    return SLOT(x, m);
}

/* ============================================================ AbortController / AbortSignal */
enum { A_ABORTED, A_REASON, A_LISTEN, A_ALGOS, A_NSLOTS };

jsobj *js_abort_signal_new(void) {
    jsobj *o = host_new(J.realm->p_abortsig, A_NSLOTS);
    jsobj *x = jv_o(o->u.host.extra);
    SET(x, A_ABORTED, JS_FALSE);
    SET(x, A_LISTEN, jv_obj(js_newarr(0)));
    SET(x, A_ALGOS, jv_obj(js_newarr(0)));
    return o;
}

static void fire_on(jsobj *target, jsobj *listen, const char *type, jsobj *ev) {
    char on[32];
    snprintf(on, sizeof on, "on%s", type);
    jsv h = js_getn(jv_obj(target), on);
    if (js_pending()) js_report_exception();
    else if (js_iscallable(h)) { jsv a = jv_obj(ev); if (js_call(h, jv_obj(target), 1, &a) == JS_EXC) js_report_exception(); }
    if (!listen) return;
    /* lista em trincas: tipo, função, once; percorre uma cópia (o ouvinte pode mexer na lista) */
    jsobj *l = js_arr_from(listen->arr, (int)listen->alen);
    for (u32 i = 0; i + 2 < l->alen; i += 3) {
        if (!jv_isstr(l->arr[i]) || strcmp(jv_s(l->arr[i])->d, type)) continue;
        jsv f = l->arr[i + 1], a = jv_obj(ev);
        if (js_truthy(l->arr[i + 2]))
            for (u32 k = 0; k + 2 < listen->alen; k += 3)
                if (listen->arr[k] == l->arr[i] && listen->arr[k + 1] == f) {
                    memmove(listen->arr + k, listen->arr + k + 3, sizeof(jsv) * (listen->alen - k - 3));
                    js_arr_setlen(listen, listen->alen - 3);
                    break;
                }
        jsv rv = js_iscallable(f) ? js_call(f, jv_obj(target), 1, &a) : js_invoke(f, js_intern("handleEvent", -1), 1, &a);
        if (rv == JS_EXC) js_report_exception();
    }
}

static jsobj *make_event(const char *type, jsobj *target, jsobj *proto) {
    jsobj *ev = js_newobj(proto ? proto : J.realm->p_event);
    js_defn(ev, "type", jv_str(js_intern(type, -1)), 0);
    js_defn(ev, "target", jv_obj(target), 0);
    js_defn(ev, "currentTarget", jv_obj(target), 0);
    js_defn(ev, "srcElement", jv_obj(target), 0);
    js_defn(ev, "bubbles", JS_FALSE, 0);
    js_defn(ev, "cancelable", JS_FALSE, 0);
    js_defn(ev, "defaultPrevented", JS_FALSE, 0);
    js_defn(ev, "isTrusted", JS_TRUE, 0);
    js_defn(ev, "timeStamp", jv_num(js_date_now()), 0);
    return ev;
}

void js_abort_signal(jsobj *sig, jsv reason) {
    jsobj *x = jv_o(sig->u.host.extra);
    if (js_truthy(SLOT(x, A_ABORTED))) return;
    if (reason == JS_UNDEF) reason = dom_value("AbortError", "A operação foi abortada.");
    SET(x, A_ABORTED, JS_TRUE);
    SET(x, A_REASON, reason);
    /* algoritmos internos (fetch, sinais dependentes) antes do evento */
    jsobj *al = jv_o(SLOT(x, A_ALGOS));
    jsobj *copy = js_arr_from(al->arr, (int)al->alen);
    js_arr_setlen(al, 0);
    for (u32 i = 0; i < copy->alen; i++) { jsv a = reason; if (js_call(copy->arr[i], jv_obj(sig), 1, &a) == JS_EXC) js_report_exception(); }
    fire_on(sig, jv_o(SLOT(x, A_LISTEN)), "abort", make_event("abort", sig, NULL));
}

static void abort_add_algo(jsobj *sig, jsv fn) { js_arr_push(jv_o(SLOT(jv_o(sig->u.host.extra), A_ALGOS)), fn); }

NATIVE(follow_algo) { UNUSED_ARGS; js_abort_signal(jv_o(self->u.nat.data), ARG(0)); return JS_UNDEF; }

void js_abort_follow(jsobj *own, jsv parent) {
    jsobj *px = extra_of(parent, J.realm->p_abortsig);
    if (!px) return;
    if (js_truthy(SLOT(px, A_ABORTED))) { js_abort_signal(own, SLOT(px, A_REASON)); return; }
    jsobj *f = js_newnative(follow_algo, "seguir", 1, 0);
    f->u.nat.data = jv_obj(own);
    abort_add_algo(jv_o(parent), jv_obj(f));
}

NATIVE(ac_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor AbortController exige 'new'");
    jsobj *o = host_new(J.realm->p_abortctl, 1);
    SET(jv_o(o->u.host.extra), 0, jv_obj(js_abort_signal_new()));
    return jv_obj(o);
}
/* magic 0 signal (getter), 1 abort */
NATIVE(ac_method) {
    UNUSED_ARGS;
    jsobj *x = extra_of(thisv, J.realm->p_abortctl);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal: não é um AbortController");
    if (self->u.nat.magic == 0) return SLOT(x, 0);
    js_abort_signal(jv_o(SLOT(x, 0)), ARG(0));
    return JS_UNDEF;
}

/* magic 0 aborted 1 reason 2 throwIfAborted 3 addEventListener 4 removeEventListener 5 dispatchEvent */
NATIVE(as_method) {
    UNUSED_ARGS;
    jsobj *x = extra_of(thisv, J.realm->p_abortsig);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal: não é um AbortSignal");
    switch (self->u.nat.magic) {
    case 0: return jv_bool(js_truthy(SLOT(x, A_ABORTED)));
    case 1: return SLOT(x, A_REASON);
    case 2: if (js_truthy(SLOT(x, A_ABORTED))) return js_throw(SLOT(x, A_REASON)); return JS_UNDEF;
    case 3: case 4: {
        jsstr *t = js_tostr(ARG(0));
        if (!t) return JS_EXC;
        jsv f = ARG(1);
        if (!jv_isobj(f)) return JS_UNDEF;
        bool once = jv_isobj(ARG(2)) && js_truthy(js_getn(ARG(2), "once"));
        jsobj *l = jv_o(SLOT(x, A_LISTEN));
        for (u32 i = 0; i + 2 < l->alen; i += 3) {
            if (!js_str_eq(jv_s(l->arr[i]), t) || l->arr[i + 1] != f) continue;
            if (self->u.nat.magic == 3) return JS_UNDEF;
            memmove(l->arr + i, l->arr + i + 3, sizeof(jsv) * (l->alen - i - 3));
            js_arr_setlen(l, l->alen - 3);
            return JS_UNDEF;
        }
        if (self->u.nat.magic == 3) { js_arr_push(l, jv_str(t)); js_arr_push(l, f); js_arr_push(l, jv_bool(once)); }
        return JS_UNDEF;
    }
    case 5: {
        jsv ev = ARG(0);
        jsv ty = js_getn(ev, "type");
        if (js_pending()) return JS_EXC;
        jsstr *t = js_tostr(ty);
        if (!t || !jv_isobj(ev)) return JS_EXC;
        js_defn(jv_o(ev), "target", thisv, 0);
        js_defn(jv_o(ev), "currentTarget", thisv, 0);
        fire_on(jv_o(thisv), jv_o(SLOT(x, A_LISTEN)), t->d, jv_o(ev));
        return JS_TRUE;
    }
    }
    return JS_UNDEF;
}

NATIVE(timeout_task) {
    UNUSED_ARGS;
    js_abort_signal(jv_o(self->u.nat.data), dom_value("TimeoutError", "O tempo da operação esgotou."));
    return JS_UNDEF;
}

/* estáticos: magic 0 abort 1 timeout 2 any */
NATIVE(as_static) {
    UNUSED_ARGS;
    jsobj *s = js_abort_signal_new();
    switch (self->u.nat.magic) {
    case 0: js_abort_signal(s, ARG(0)); break;
    case 1: {
        double ms = js_tonum(ARG(0));
        if (js_pending()) return JS_EXC;
        if (!(ms >= 0) || ms > 9007199254740991.0) return js_throw_err(E_TYPE, "AbortSignal.timeout: tempo inválido");
        jsobj *t = js_newnative(timeout_task, "tempo", 0, 0);
        t->u.nat.data = jv_obj(s);
        js_page_add_timer(js_cur_page(), jv_obj(t), ms);
        break;
    }
    case 2: {
        jsv list = ARG(0);
        u32 n = js_lengthof(list);
        if (js_pending()) return JS_EXC;
        for (u32 i = 0; i < n; i++) {
            jsv e = js_getidx(list, i);
            if (!extra_of(e, J.realm->p_abortsig)) return js_throw_err(E_TYPE, "AbortSignal.any: só AbortSignal");
        }
        for (u32 i = 0; i < n; i++) {
            js_abort_follow(s, js_getidx(list, i));
            if (js_truthy(SLOT(jv_o(s->u.host.extra), A_ABORTED))) break;
        }
        break;
    }
    }
    return jv_obj(s);
}

/* ============================================================ rede pendente */
/* pedido em andamento: owner = [kind (0 fetch, 1 xhr), promessa ou xhr, request, sinal ...] mantido vivo no global */
typedef struct wf {
    http_req_t *req;
    js_page_t *page;
    jsobj *owner;
    bool sync_ready;      /* resposta local já pronta (data:, blob:, file:), entregue no próximo tick */
    int status;
    char *body; u32 blen;
    char *headers;
    char url[2048];
    bool redirected;
    struct wf *next;
} wf_t;
static wf_t *wf_list;

static jsobj *keepalive(bool create) {
    jsobj *g = J.realm->global;
    jsstr *k = js_intern("\x01" "redes", -1);
    jsprop *p = js_ownprop(g, k);
    if (p && jv_isobj(p->v)) return jv_o(p->v);
    if (!create) return NULL;
    jsobj *a = js_newarr(0);
    js_defprop(g, k, jv_obj(a), PF_NOENUM);
    return a;
}
static void keep_add(jsobj *o) { js_arr_push(keepalive(true), jv_obj(o)); }
static void keep_del(jsobj *o) {
    jsobj *a = keepalive(false);
    if (!a) return;
    for (u32 i = 0; i < a->alen; i++) if (a->arr[i] == jv_obj(o)) { memmove(a->arr + i, a->arr + i + 1, sizeof(jsv) * (a->alen - i - 1)); js_arr_setlen(a, a->alen - 1); return; }
}

static void wf_unlink(wf_t *w) {
    for (wf_t **pp = &wf_list; *pp; pp = &(*pp)->next) if (*pp == w) { *pp = w->next; break; }
    if (w->req) http_free(w->req);
    kfree(w->body);
    kfree(w->headers);
    kfree(w);
}

bool web_fetch_pending(js_page_t *p) { for (wf_t *w = wf_list; w; w = w->next) if (w->page == p) return true; return false; }
void web_fetch_drop(js_page_t *p) {
    for (wf_t *w = wf_list, *nx; w; w = nx) { nx = w->next; if (w->page == p) wf_unlink(w); }
}

/* lê um URL local (data:, blob:, file:) para o wf. false = erro de rede */
static bool local_load(wf_t *w, const char *url, const char *method) {
    char mime[128] = "";
    if (!strncmp(url, "data:", 5)) {
        usize n = 0;
        u8 *d = frame_data_url(url, &n, mime, sizeof mime);
        if (!d) return false;
        w->body = (char *)d; w->blen = (u32)n;
        /* o tipo leva os parâmetros (charset) do data: */
        const char *c = strchr(url, ','), *b64 = strstr(url, ";base64");
        char full[256];
        int fl = (int)MIN((usize)((b64 && b64 < c ? b64 : c) - (url + 5)), sizeof full - 1);
        memcpy(full, url + 5, (usize)fl);
        full[fl] = 0;
        if (!fl) strcpy(full, "text/plain;charset=US-ASCII");
        usize hl = strlen(full) + 20;
        w->headers = kmalloc(hl);
        if (w->headers) snprintf(w->headers, hl, "Content-Type: %s\r\n", full);
        w->status = 200;
        return true;
    }
    if (!strncmp(url, "blob:", 5)) {
        if (strcmp(method, "GET")) return false;
        const u8 *d;
        u32 n;
        const char *type;
        if (!js_blob_bytes(js_blob_from_url(url), &d, &n, &type)) return false;
        w->body = kmalloc(n + 1);
        if (!w->body) return false;
        memcpy(w->body, d, n);
        w->blen = n;
        usize hl = strlen(type) + 64;
        w->headers = kmalloc(hl);
        if (w->headers) snprintf(w->headers, hl, "Content-Type: %s\r\nContent-Length: %u\r\n", type, n);
        w->status = 200;
        return true;
    }
    if (!strncmp(url, "file:", 5)) {
        const char *q = url + 5;
        if (!strncmp(q, "//", 2)) q += 2;
        char clean[1024], path[1024];
        snprintf(clean, sizeof clean, "%s", q);
        for (char *cut = clean; *cut; cut++) if (*cut == '?' || *cut == '#') { *cut = 0; break; }
        url_decode_path(clean, path, sizeof path);
        vnode_t *v = vfs_lookup(path);
        if (!v || v->dir) return false;
        w->body = kmalloc(v->size + 1);
        if (!w->body) return false;
        memcpy(w->body, v->data, v->size);
        w->blen = (u32)v->size;
        const char *e = strrchr(path, '.');
        static const struct { const char *e, *t; } ty[] = { { ".html", "text/html" }, { ".htm", "text/html" }, { ".css", "text/css" },
            { ".js", "text/javascript" }, { ".mjs", "text/javascript" }, { ".json", "application/json" }, { ".svg", "image/svg+xml" },
            { ".png", "image/png" }, { ".jpg", "image/jpeg" }, { ".jpeg", "image/jpeg" }, { ".gif", "image/gif" }, { ".webp", "image/webp" },
            { ".txt", "text/plain" }, { ".wasm", "application/wasm" }, { ".xml", "application/xml" } };
        const char *t = "application/octet-stream";
        for (u32 i = 0; e && i < ARRAY_LEN(ty); i++) if (!strcasecmp(e, ty[i].e)) t = ty[i].t;
        w->headers = kmalloc(64);
        if (w->headers) snprintf(w->headers, 64, "Content-Type: %s\r\n", t);
        w->status = 200;
        return true;
    }
    return false;
}

static bool same_origin(js_page_t *p, const char *url) {
    const char *o = js_page_origin(p);
    usize n = strlen(o);
    return n && !strncmp(url, o, n) && (url[n] == '/' || url[n] == 0 || url[n] == '?' || url[n] == '#');
}

/* ============================================================ fetch() */
enum { F_KIND, F_TARGET, F_REQ, F_SIG };

NATIVE(fetch_abort_algo) {
    UNUSED_ARGS;
    jsobj *owner = jv_o(self->u.nat.data);
    for (wf_t *w = wf_list; w; w = w->next) if (w->owner == owner) {
        jsobj *pr = jv_o(js_arr_get(owner, F_TARGET));
        js_promise_reject(pr, ARG(0));
        keep_del(owner);
        wf_unlink(w);
        break;
    }
    return JS_UNDEF;
}

static wf_t *wf_start(js_page_t *page, jsobj *owner, const char *url, const char *method, const char *hdrs, const u8 *body, u32 blen, int redirect) {
    wf_t *w = kcalloc(1, sizeof *w);
    if (!w) return NULL;
    w->page = page;
    w->owner = owner;
    snprintf(w->url, sizeof w->url, "%s", url);
    if (!strncmp(url, "http:", 5) || !strncmp(url, "https:", 6)) {
        w->req = http_request(url, method, hdrs, body, blen, redirect == 2 ? 1 : 0);
        if (!w->req) { w->status = -1; w->sync_ready = true; }
    } else {
        w->sync_ready = true;
        if (!local_load(w, url, method)) w->status = -1;
    }
    /* no fim da lista: os pedidos terminam na ordem em que começaram */
    wf_t **tail = &wf_list;
    while (*tail) tail = &(*tail)->next;
    *tail = w;
    keep_add(owner);
    return w;
}

/* resposta de um wf pronto → Response; NULL = erro de rede */
static jsobj *wf_response(wf_t *w, jsobj *reqx, bool *net_err) {
    *net_err = false;
    const char *rurl = jv_s(SLOT(reqx, R_URL))->d, *redir = jv_s(SLOT(reqx, R_REDIRECT))->d;
    int status;
    const char *final = rurl, *hdrs;
    const char *body;
    u32 blen;
    bool redirected = false;
    if (w->req) {
        if (w->req->state != 1) { *net_err = true; return NULL; }
        status = w->req->status;
        final = w->req->location[0] ? w->req->location : rurl;
        redirected = w->req->redirects > 0;
        hdrs = w->req->headers;
        body = (const char *)w->req->body;
        blen = w->req->body_len;
    } else {
        if (w->status < 0) { *net_err = true; return NULL; }
        status = w->status; hdrs = w->headers; body = w->body; blen = w->blen;
    }
    if (redirected && !strcmp(redir, "error")) { *net_err = true; return NULL; }
    if (!strcmp(redir, "manual") && status >= 300 && status < 400 && w->req) {
        jsobj *o = response_new(0, "", "opaqueredirect", rurl, false, G_IMMUTABLE);
        return o;
    }
    bool cors = w->req && !same_origin(w->page, final);
    jsobj *o = response_new(status, status_text(status), cors ? "cors" : "basic", final, redirected, G_IMMUTABLE);
    jsobj *x = jv_o(o->u.host.extra);
    headers_from_wire(jv_o(jv_o(SLOT(x, B_HDR))->u.host.extra), hdrs);
    if (!null_body_status(status) && strcmp(jv_s(SLOT(reqx, R_METHOD))->d, "HEAD")) {
        jsobj *b = js_abuf_new(blen);
        if (b && blen) memcpy(b->u.buf.data, body, blen);
        SET(x, B_BODY, b ? jv_obj(b) : JS_NULL);
    }
    return o;
}

NATIVE(w_fetch) {
    UNUSED_ARGS;
    jsobj *pr = js_promise_new();
    jsobj *req = make_request(ARG(0), ARG(1));
    if (!req) { js_promise_reject(pr, js_catch()); return jv_obj(pr); }
    jsobj *x = jv_o(req->u.host.extra);
    jsobj *sig = jv_o(SLOT(x, R_SIGNAL));
    jsobj *sx = jv_o(sig->u.host.extra);
    if (js_truthy(SLOT(sx, A_ABORTED))) { js_promise_reject(pr, SLOT(sx, A_REASON)); return jv_obj(pr); }
    if (js_truthy(SLOT(x, B_USED)) && SLOT(x, B_BODY) != JS_NULL) { js_promise_reject(pr, reject_value(E_TYPE, "fetch: o corpo do Request já foi lido")); return jv_obj(pr); }
    if (SLOT(x, B_BODY) != JS_NULL) SET(x, B_USED, JS_TRUE);
    jsv ownv[4] = { jv_int(0), jv_obj(pr), jv_obj(req), jv_obj(sig) };
    jsobj *owner = js_arr_from(ownv, 4);
    sbuf_t hw = { 0 };
    headers_wire(jv_o(jv_o(SLOT(x, B_HDR))->u.host.extra), &hw);
    jsv bv = SLOT(x, B_BODY);
    const u8 *bd = jv_iscls(bv, CL_ARRAYBUFFER) ? jv_o(bv)->u.buf.data : NULL;
    u32 bl = jv_iscls(bv, CL_ARRAYBUFFER) ? jv_o(bv)->u.buf.len : 0;
    const char *redir = jv_s(SLOT(x, R_REDIRECT))->d;
    wf_t *w = wf_start(js_cur_page(), owner, jv_s(SLOT(x, R_URL))->d, jv_s(SLOT(x, R_METHOD))->d, hw.p ? hw.p : "", bd, bl,
                       !strcmp(redir, "manual") ? 2 : 0);
    sb_free(&hw);
    if (!w) { js_promise_reject(pr, reject_value(E_RANGE, "sem memória")); return jv_obj(pr); }
    jsobj *algo = js_newnative(fetch_abort_algo, "abortar", 1, 0);
    algo->u.nat.data = jv_obj(owner);
    abort_add_algo(sig, jv_obj(algo));
    return jv_obj(pr);
}

/* ============================================================ XMLHttpRequest */
enum { X_STATE, X_METHOD, X_URL, X_ASYNC, X_REQHDRS, X_STATUS, X_STEXT, X_RESPHDRS, X_RESPBYTES, X_RTYPE, X_TIMEOUT, X_CREDS,
       X_UPLOAD, X_LISTEN, X_SEND, X_RESPURL, X_MIME, X_GEN, X_CACHE, X_ERROR, X_ULISTEN, X_NSLOTS };

static jsobj *this_xhr(jsv v) {
    jsobj *x = extra_of(v, J.realm->p_xhr);
    if (!x) js_throw_err(E_TYPE, "Invocação ilegal: não é um XMLHttpRequest");
    return x;
}

static void xhr_fire(jsobj *xhr, jsobj *x, const char *type, bool progress, u32 loaded, u32 total, bool upload) {
    jsobj *target = upload ? jv_o(SLOT(x, X_UPLOAD)) : xhr;
    jsobj *ev = make_event(type, target, progress ? J.realm->p_progress : NULL);
    if (progress) {
        js_defn(ev, "lengthComputable", jv_bool(total > 0), 0);
        js_defn(ev, "loaded", jv_int((int)loaded), 0);
        js_defn(ev, "total", jv_int((int)total), 0);
    }
    fire_on(target, jv_o(SLOT(x, upload ? X_ULISTEN : X_LISTEN)), type, ev);
}

static void xhr_state(jsobj *xhr, jsobj *x, int st) {
    SET(x, X_STATE, jv_int(st));
    xhr_fire(xhr, x, "readystatechange", false, 0, 0, false);
}

NATIVE(xhr_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor XMLHttpRequest exige 'new'");
    jsobj *o = host_new(J.realm->p_xhr, X_NSLOTS);
    jsobj *x = jv_o(o->u.host.extra);
    SET(x, X_STATE, jv_int(0));
    SET(x, X_REQHDRS, jv_obj(headers_new(G_REQUEST)));
    SET(x, X_STATUS, jv_int(0));
    SET(x, X_STEXT, S(""));
    SET(x, X_RESPHDRS, S(""));
    SET(x, X_RESPBYTES, JS_NULL);
    SET(x, X_RTYPE, S(""));
    SET(x, X_TIMEOUT, jv_int(0));
    SET(x, X_CREDS, JS_FALSE);
    jsobj *up = host_new(J.realm->p_xhrup, 1);
    SET(jv_o(up->u.host.extra), 0, jv_obj(o));
    SET(x, X_UPLOAD, jv_obj(up));
    SET(x, X_LISTEN, jv_obj(js_newarr(0)));
    SET(x, X_ULISTEN, jv_obj(js_newarr(0)));
    SET(x, X_SEND, JS_FALSE);
    SET(x, X_RESPURL, S(""));
    SET(x, X_GEN, jv_int(0));
    SET(x, X_ERROR, JS_FALSE);
    return jv_obj(o);
}

/* resposta completa: preenche e dispara os eventos finais */
static void xhr_finish(jsobj *xhr, jsobj *x, wf_t *w) {
    int status;
    const char *hdrs, *body, *final;
    u32 blen;
    bool err = false;
    if (w->req) {
        err = w->req->state != 1;
        status = w->req->status; hdrs = w->req->headers; body = (const char *)w->req->body; blen = w->req->body_len;
        final = w->req->location[0] ? w->req->location : w->url;
    } else {
        err = w->status < 0;
        status = w->status; hdrs = w->headers; body = w->body; blen = w->blen; final = w->url;
    }
    SET(x, X_SEND, JS_FALSE);
    if (err) {
        SET(x, X_ERROR, JS_TRUE);
        SET(x, X_STATUS, jv_int(0));
        SET(x, X_RESPBYTES, JS_NULL);
        xhr_state(xhr, x, 4);
        xhr_fire(xhr, x, "error", true, 0, 0, false);
        xhr_fire(xhr, x, "loadend", true, 0, 0, false);
        return;
    }
    SET(x, X_STATUS, jv_int(status));
    SET(x, X_STEXT, S(status_text(status)));
    SET(x, X_RESPHDRS, S(hdrs ? hdrs : ""));
    SET(x, X_RESPURL, S(final));
    jsobj *b = js_abuf_new(blen);
    if (b && blen) memcpy(b->u.buf.data, body, blen);
    SET(x, X_RESPBYTES, b ? jv_obj(b) : JS_NULL);
    SET(x, X_CACHE, JS_UNDEF);
    xhr_state(xhr, x, 2);
    xhr_state(xhr, x, 3);
    xhr_fire(xhr, x, "progress", true, blen, blen, false);
    xhr_state(xhr, x, 4);
    xhr_fire(xhr, x, "load", true, blen, blen, false);
    xhr_fire(xhr, x, "loadend", true, blen, blen, false);
}

NATIVE(xhr_timeout_task) {
    UNUSED_ARGS;
    jsobj *xhr = jv_o(self->u.nat.data), *x = jv_o(xhr->u.host.extra);
    if (SLOT(x, X_GEN) != self->u.nat.data2 || !js_truthy(SLOT(x, X_SEND))) return JS_UNDEF;
    for (wf_t *w = wf_list; w; w = w->next) if (js_arr_get(w->owner, F_TARGET) == jv_obj(xhr)) { keep_del(w->owner); wf_unlink(w); break; }
    SET(x, X_SEND, JS_FALSE);
    SET(x, X_ERROR, JS_TRUE);
    xhr_state(xhr, x, 4);
    xhr_fire(xhr, x, "timeout", true, 0, 0, false);
    xhr_fire(xhr, x, "loadend", true, 0, 0, false);
    return JS_UNDEF;
}

static jsv xhr_response(jsobj *x, bool text_only) {
    jsv bytes = SLOT(x, X_RESPBYTES);
    const char *rt = jv_s(SLOT(x, X_RTYPE))->d;
    int st = js_toint32(SLOT(x, X_STATE));
    if (text_only || !*rt || !strcmp(rt, "text")) {
        if (st < 3 || !jv_iscls(bytes, CL_ARRAYBUFFER)) return jv_str(A[A_empty]);
        bool bad;
        jsstr *s = js_utf8_decode(jv_o(bytes)->u.buf.data, jv_o(bytes)->u.buf.len, false, &bad);
        return jv_str(s ? s : A[A_empty]);
    }
    if (st != 4 || !jv_iscls(bytes, CL_ARRAYBUFFER)) return JS_NULL;
    jsv cached = SLOT(x, X_CACHE);
    if (cached != JS_UNDEF) return cached;
    jsv r = JS_NULL;
    const u8 *d = jv_o(bytes)->u.buf.data;
    u32 n = jv_o(bytes)->u.buf.len;
    if (!strcmp(rt, "arraybuffer")) { jsobj *b = js_abuf_new(n); if (b && n) memcpy(b->u.buf.data, d, n); r = jv_obj(b); }
    else if (!strcmp(rt, "blob")) {
        char type[160] = "";
        jsv mime = SLOT(x, X_MIME);
        const char *ct = jv_isstr(mime) ? jv_s(mime)->d : NULL;
        const char *raw = jv_s(SLOT(x, X_RESPHDRS))->d, *c = NULL;
        for (const char *p = raw; !ct && *p;) {
            const char *e = strstr(p, "\r\n");
            if (!strncasecmp(p, "content-type:", 13)) { c = p + 13; while (*c == ' ') c++; int k = 0; while (c[k] && c[k] != '\r' && k < 159) { type[k] = (char)tolower((u8)c[k]); k++; } type[k] = 0; break; }
            p = e ? e + 2 : p + strlen(p);
        }
        if (ct) snprintf(type, sizeof type, "%s", ct);
        r = jv_obj(js_blob_new(d, n, type));
    } else if (!strcmp(rt, "json")) {
        bool bad;
        jsstr *s = js_utf8_decode(d, n, false, &bad);
        r = s ? js_json_parse(s, JS_UNDEF) : JS_NULL;
        if (r == JS_EXC) { js_catch(); r = JS_NULL; }
    } else if (!strcmp(rt, "document")) {
        bool bad;
        jsstr *s = js_utf8_decode(d, n, false, &bad);
        r = s ? js_dom_parse_document(jv_s(SLOT(x, X_RESPURL))->d, s, "text/html") : JS_NULL;
        if (r == JS_EXC) { js_catch(); r = JS_NULL; }
    }
    SET(x, X_CACHE, r);
    return r;
}

/* getters: 0 readyState 1 status 2 statusText 3 response 4 responseText 5 responseURL 6 responseType 7 timeout 8 withCredentials 9 upload 10 responseXML */
NATIVE(xhr_get) {
    UNUSED_ARGS;
    jsobj *x = this_xhr(thisv);
    if (!x) return JS_EXC;
    switch (self->u.nat.magic) {
    case 0: return SLOT(x, X_STATE);
    case 1: return SLOT(x, X_STATUS);
    case 2: return SLOT(x, X_STEXT);
    case 3: return xhr_response(x, false);
    case 4: {
        const char *rt = jv_s(SLOT(x, X_RTYPE))->d;
        if (*rt && strcmp(rt, "text")) return js_throw_dom("InvalidStateError", "responseText só vale com responseType '' ou 'text'");
        return xhr_response(x, true);
    }
    case 5: return SLOT(x, X_RESPURL);
    case 6: return SLOT(x, X_RTYPE);
    case 7: return SLOT(x, X_TIMEOUT);
    case 8: return SLOT(x, X_CREDS);
    case 9: return SLOT(x, X_UPLOAD);
    case 10: {
        const char *rt = jv_s(SLOT(x, X_RTYPE))->d;
        if (*rt && strcmp(rt, "document")) return js_throw_dom("InvalidStateError", "responseXML só vale com responseType '' ou 'document'");
        if (js_toint32(SLOT(x, X_STATE)) != 4) return JS_NULL;
        jsv saved = SLOT(x, X_RTYPE);
        SET(x, X_RTYPE, S("document"));
        jsv r = xhr_response(x, false);
        SET(x, X_RTYPE, saved);
        if (!*jv_s(saved)->d) SET(x, X_CACHE, JS_UNDEF);
        return r;
    }
    }
    return JS_UNDEF;
}

NATIVE(xhr_set) {
    UNUSED_ARGS;
    jsobj *x = this_xhr(thisv);
    if (!x) return JS_EXC;
    int st = js_toint32(SLOT(x, X_STATE));
    switch (self->u.nat.magic) {
    case 6: {
        jsstr *s = js_tostr(ARG(0));
        if (!s) return JS_EXC;
        static const char *ok[] = { "", "arraybuffer", "blob", "document", "json", "text" };
        bool valid = false;
        for (u32 i = 0; i < ARRAY_LEN(ok); i++) if (!strcmp(s->d, ok[i])) valid = true;
        if (!valid) return JS_UNDEF;
        if (st == 3 || st == 4) return js_throw_dom("InvalidStateError", "responseType não pode mudar depois do carregamento");
        if (st >= 1 && !js_truthy(SLOT(x, X_ASYNC))) return js_throw_dom("InvalidAccessError", "responseType não vale em XHR síncrono");
        SET(x, X_RTYPE, jv_str(s));
        return JS_UNDEF;
    }
    case 7: {
        double d = js_tonum(ARG(0));
        if (js_pending()) return JS_EXC;
        if (st >= 1 && !js_truthy(SLOT(x, X_ASYNC))) return js_throw_dom("InvalidAccessError", "timeout não vale em XHR síncrono");
        SET(x, X_TIMEOUT, jv_num(d == d && d > 0 ? (double)(u32)d : 0));
        return JS_UNDEF;
    }
    case 8:
        if (st > 1 || js_truthy(SLOT(x, X_SEND))) return js_throw_dom("InvalidStateError", "withCredentials só antes do send()");
        SET(x, X_CREDS, jv_bool(js_truthy(ARG(0))));
        return JS_UNDEF;
    }
    return JS_UNDEF;
}

/* tira um pedido em andamento do XHR (open/abort) */
static void xhr_cancel(jsobj *xhr) {
    for (wf_t *w = wf_list; w; w = w->next)
        if (js_arr_get(w->owner, F_KIND) == jv_int(1) && js_arr_get(w->owner, F_TARGET) == jv_obj(xhr)) { keep_del(w->owner); wf_unlink(w); return; }
}

/* magic: 0 open 1 setRequestHeader 2 send 3 abort 4 getResponseHeader 5 getAllResponseHeaders 6 overrideMimeType
 * 7 addEventListener 8 removeEventListener 9 dispatchEvent */
NATIVE(xhr_method) {
    UNUSED_ARGS;
    jsobj *x = this_xhr(thisv);
    if (!x) return JS_EXC;
    jsobj *xhr = jv_o(thisv);
    js_page_t *page = js_cur_page();
    int st = js_toint32(SLOT(x, X_STATE));
    switch (self->u.nat.magic) {
    case 0: {
        if (argc < 2) return js_throw_err(E_TYPE, "XMLHttpRequest.open: são necessários 2 argumentos");
        jsstr *m = js_tostr(ARG(0));
        if (!m) return JS_EXC;
        jsstr *u = js_tostr(ARG(1));
        if (!u) return JS_EXC;
        if (!is_token(m->d, (int)m->len)) return js_throw_dom("SyntaxError", "XMLHttpRequest.open: método inválido");
        if (!strcasecmp(m->d, "CONNECT") || !strcasecmp(m->d, "TRACE") || !strcasecmp(m->d, "TRACK")) return js_throw_dom("SecurityError", "XMLHttpRequest.open: método proibido");
        char url[2048];
        if (!web_url_parse(u->d, js_page_base(page), url, sizeof url)) return js_throw_dom("SyntaxError", "XMLHttpRequest.open: URL inválido");
        bool async = argc < 3 || js_truthy(ARG(2));
        if (!async && *jv_s(SLOT(x, X_RTYPE))->d) return js_throw_dom("InvalidAccessError", "XHR síncrono não aceita responseType");
        xhr_cancel(xhr);
        char mb[16];
        SET(x, X_METHOD, S(norm_method(m, mb)));
        SET(x, X_URL, S(url));
        SET(x, X_ASYNC, jv_bool(async));
        SET(x, X_REQHDRS, jv_obj(headers_new(G_REQUEST)));
        SET(x, X_SEND, JS_FALSE);
        SET(x, X_STATUS, jv_int(0));
        SET(x, X_STEXT, S(""));
        SET(x, X_RESPHDRS, S(""));
        SET(x, X_RESPBYTES, JS_NULL);
        SET(x, X_RESPURL, S(""));
        SET(x, X_CACHE, JS_UNDEF);
        SET(x, X_ERROR, JS_FALSE);
        SET(x, X_GEN, jv_int(js_toint32(SLOT(x, X_GEN)) + 1));
        if (st != 1) xhr_state(xhr, x, 1); else SET(x, X_STATE, jv_int(1));
        return JS_UNDEF;
    }
    case 1: {
        if (st != 1 || js_truthy(SLOT(x, X_SEND))) return js_throw_dom("InvalidStateError", "setRequestHeader: chame open() antes e send() depois");
        if (argc < 2) return js_throw_err(E_TYPE, "setRequestHeader: são necessários 2 argumentos");
        jsobj *hx = jv_o(jv_o(SLOT(x, X_REQHDRS))->u.host.extra);
        jsstr *n, *v;
        if (!header_norm(ARG(0), ARG(1), &n, &v)) return js_pending() ? JS_EXC : js_throw_dom("SyntaxError", "setRequestHeader: cabeçalho inválido");
        if (forbidden_request(n->d)) return JS_UNDEF;
        /* o mesmo nome combina com ", " */
        jsstr *cur = headers_get(hx, n->d);
        if (cur) return headers_append(hx, jv_str(n), jv_str(js_str_fmt("%s, %s", cur->d, v->d)), true) < 0 ? JS_EXC : JS_UNDEF;
        return headers_append(hx, jv_str(n), jv_str(v), false) < 0 ? JS_EXC : JS_UNDEF;
    }
    case 2: {
        if (st != 1 || js_truthy(SLOT(x, X_SEND))) return js_throw_dom("InvalidStateError", "send: o pedido não está aberto");
        const char *meth = jv_s(SLOT(x, X_METHOD))->d;
        jsobj *buf = NULL;
        jsstr *ct = NULL;
        jsobj *hx = jv_o(jv_o(SLOT(x, X_REQHDRS))->u.host.extra);
        if (!jv_isnullish(ARG(0)) && strcmp(meth, "GET") && strcmp(meth, "HEAD")) {
            if (extract_body(ARG(0), &buf, &ct) < 0) return JS_EXC;
            if (ct && !headers_get(hx, "content-type")) headers_append(hx, S("Content-Type"), jv_str(ct), false);
        }
        SET(x, X_ERROR, JS_FALSE);
        SET(x, X_SEND, JS_TRUE);
        u32 bl = buf ? buf->u.buf.len : 0;
        bool async = js_truthy(SLOT(x, X_ASYNC));
        if (async) {
            xhr_fire(xhr, x, "loadstart", true, 0, 0, false);
            jsobj *ul = jv_o(SLOT(x, X_ULISTEN));
            bool up_listeners = ul->alen > 0;
            if (buf && up_listeners) {
                xhr_fire(xhr, x, "loadstart", true, 0, bl, true);
                xhr_fire(xhr, x, "progress", true, bl, bl, true);
                xhr_fire(xhr, x, "load", true, bl, bl, true);
                xhr_fire(xhr, x, "loadend", true, bl, bl, true);
            }
            if (!js_truthy(SLOT(x, X_SEND))) return JS_UNDEF;   /* abortado por um ouvinte */
        }
        sbuf_t hw = { 0 };
        headers_wire(hx, &hw);
        jsv ownv[2] = { jv_int(1), thisv };
        jsobj *owner = js_arr_from(ownv, 2);
        wf_t *w = wf_start(page, owner, jv_s(SLOT(x, X_URL))->d, meth, hw.p ? hw.p : "", buf ? buf->u.buf.data : NULL, bl, 0);
        sb_free(&hw);
        if (!w) return js_throw_err(E_RANGE, "sem memória");
        if (!async) {
            /* síncrono: espera aqui (rede do kernel avançando), até o timeout ou 60 s */
            u64 limit = timer_ticks() + 60000;
            while (w->req && !w->req->state && timer_ticks() < limit) { net_poll(); http_poll(w->req); }
            keep_del(owner);
            for (wf_t **pp = &wf_list; *pp; pp = &(*pp)->next) if (*pp == w) { *pp = w->next; break; }
            w->next = NULL;
            bool ok = w->req ? w->req->state == 1 : w->status >= 0;
            if (w->req && !w->req->state) { http_free(w->req); w->req = NULL; w->status = -1; ok = false; }
            xhr_finish(xhr, x, w);
            kfree(w->body); kfree(w->headers); if (w->req) http_free(w->req); kfree(w);
            if (!ok) return js_throw_dom("NetworkError", "XMLHttpRequest síncrono falhou");
            return JS_UNDEF;
        }
        double to = js_tonum(SLOT(x, X_TIMEOUT));
        if (to > 0) {
            jsobj *t = js_newnative(xhr_timeout_task, "timeout", 0, 0);
            t->u.nat.data = thisv;
            t->u.nat.data2 = SLOT(x, X_GEN);
            js_page_add_timer(page, jv_obj(t), to);
        }
        return JS_UNDEF;
    }
    case 3: {
        bool sending = js_truthy(SLOT(x, X_SEND));
        xhr_cancel(xhr);
        SET(x, X_SEND, JS_FALSE);
        SET(x, X_GEN, jv_int(js_toint32(SLOT(x, X_GEN)) + 1));
        if ((st == 1 && sending) || st == 2 || st == 3) {
            SET(x, X_RESPBYTES, JS_NULL);
            SET(x, X_STATUS, jv_int(0));
            xhr_state(xhr, x, 4);
            xhr_fire(xhr, x, "abort", true, 0, 0, false);
            xhr_fire(xhr, x, "loadend", true, 0, 0, false);
        }
        if (js_toint32(SLOT(x, X_STATE)) == 4) SET(x, X_STATE, jv_int(0));
        return JS_UNDEF;
    }
    case 4: {
        jsstr *n = js_tostr(ARG(0));
        if (!n) return JS_EXC;
        if (st < 2 || js_truthy(SLOT(x, X_ERROR))) return JS_NULL;
        if (!strcasecmp(n->d, "set-cookie") || !strcasecmp(n->d, "set-cookie2")) return JS_NULL;
        jsobj *tmp = headers_new(G_NONE);
        headers_from_wire(jv_o(tmp->u.host.extra), jv_s(SLOT(x, X_RESPHDRS))->d);
        jsstr *v = headers_get(jv_o(tmp->u.host.extra), n->d);
        return v ? jv_str(v) : JS_NULL;
    }
    case 5: {
        if (st < 2 || js_truthy(SLOT(x, X_ERROR))) return jv_str(A[A_empty]);
        jsobj *tmp = headers_new(G_NONE);
        headers_from_wire(jv_o(tmp->u.host.extra), jv_s(SLOT(x, X_RESPHDRS))->d);
        jsobj *s = headers_sorted(jv_o(tmp->u.host.extra));
        sbuf_t o = { 0 };
        for (u32 i = 0; i < s->alen; i++) {
            jsstr *k = jv_s(js_getidx(s->arr[i], 0));
            if (!strcmp(k->d, "set-cookie") || !strcmp(k->d, "set-cookie2")) continue;
            sb_putn(&o, k->d, (int)k->len);
            sb_puts(&o, ": ");
            jsstr *v = jv_s(js_getidx(s->arr[i], 1));
            sb_putn(&o, v->d, (int)v->len);
            sb_puts(&o, "\r\n");
        }
        jsstr *r = js_strn(o.p ? o.p : "", o.len);
        sb_free(&o);
        return jv_str(r);
    }
    case 6: {
        if (st == 3 || st == 4) return js_throw_dom("InvalidStateError", "overrideMimeType depois do carregamento");
        jsstr *s = js_tostr(ARG(0));
        if (!s) return JS_EXC;
        SET(x, X_MIME, jv_str(s));
        return JS_UNDEF;
    }
    }
    return JS_UNDEF;
}

/* listeners de XHR e do upload: magic 0 add 1 remove 2 dispatch; data: 0 xhr, 1 upload */
NATIVE(xhr_listen) {
    UNUSED_ARGS;
    bool up = self->u.nat.data == JS_TRUE;
    jsobj *x;
    jsobj *target = jv_isobj(thisv) ? jv_o(thisv) : NULL;
    if (up) {
        jsobj *ux = extra_of(thisv, J.realm->p_xhrup);
        if (!ux) return js_throw_err(E_TYPE, "Invocação ilegal");
        x = jv_o(jv_o(SLOT(ux, 0))->u.host.extra);
    } else if (!(x = this_xhr(thisv))) return JS_EXC;
    jsobj *l = jv_o(SLOT(x, up ? X_ULISTEN : X_LISTEN));
    if (self->u.nat.magic == 2) {
        jsv ev = ARG(0);
        jsv ty = js_getn(ev, "type");
        if (js_pending()) return JS_EXC;
        jsstr *t = js_tostr(ty);
        if (!t || !jv_isobj(ev)) return JS_EXC;
        js_defn(jv_o(ev), "target", thisv, 0);
        js_defn(jv_o(ev), "currentTarget", thisv, 0);
        fire_on(target, l, t->d, jv_o(ev));
        return JS_TRUE;
    }
    jsstr *t = js_tostr(ARG(0));
    if (!t) return JS_EXC;
    jsv f = ARG(1);
    if (!jv_isobj(f)) return JS_UNDEF;
    bool once = jv_isobj(ARG(2)) && js_truthy(js_getn(ARG(2), "once"));
    for (u32 i = 0; i + 2 < l->alen; i += 3) {
        if (!js_str_eq(jv_s(l->arr[i]), t) || l->arr[i + 1] != f) continue;
        if (self->u.nat.magic == 0) return JS_UNDEF;
        memmove(l->arr + i, l->arr + i + 3, sizeof(jsv) * (l->alen - i - 3));
        js_arr_setlen(l, l->alen - 3);
        return JS_UNDEF;
    }
    if (self->u.nat.magic == 0) { js_arr_push(l, jv_str(t)); js_arr_push(l, f); js_arr_push(l, jv_bool(once)); }
    return JS_UNDEF;
}

/* ============================================================ tick */
void web_fetch_pump(js_page_t *p) {
    for (int guard = 0; guard < 64; guard++) {
        wf_t *ready = NULL;
        for (wf_t *w = wf_list; w; w = w->next) {
            if (w->page != p) continue;
            if (w->sync_ready || (w->req && http_poll(w->req) != 0)) { ready = w; break; }
        }
        if (!ready) return;
        /* tira da lista antes de entregar: o script pode abrir pedidos novos */
        for (wf_t **pp = &wf_list; *pp; pp = &(*pp)->next) if (*pp == ready) { *pp = ready->next; break; }
        ready->next = NULL;
        jsobj *owner = ready->owner;
        keep_del(owner);
        if (js_arr_get(owner, F_KIND) == jv_int(0)) {
            jsobj *pr = jv_o(js_arr_get(owner, F_TARGET));
            jsobj *reqx = jv_o(jv_o(js_arr_get(owner, F_REQ))->u.host.extra);
            bool err;
            jsobj *res = wf_response(ready, reqx, &err);
            if (err) js_promise_reject(pr, reject_value(E_TYPE, "Failed to fetch"));
            else js_promise_resolve(pr, jv_obj(res));
        } else {
            jsobj *xhr = jv_o(js_arr_get(owner, F_TARGET));
            xhr_finish(xhr, jv_o(xhr->u.host.extra), ready);
        }
        kfree(ready->body); kfree(ready->headers);
        if (ready->req) http_free(ready->req);
        kfree(ready);
        js_run_jobs();
    }
}

/* ============================================================ registro */
static void iface(jsobj *g, const char *name, jsobj *proto, jsnative ctor, int nargs, jsobj **out) {
    jsobj *c = js_newctor(ctor, name, nargs, proto);
    js_defn(g, name, jv_obj(c), PF_NOENUM);
    js_defprop(proto, A[A_sym_toStringTag], jv_str(js_intern(name, -1)), PF_NOENUM | PF_RO);
    if (out) *out = c;
}
NATIVE(illegal) { UNUSED_ARGS; return js_throw_err(E_TYPE, "Construtor ilegal"); }

void js_fetch_init(jsrealm *r) {
    jsobj *g = r->global, *op = r->p_object;
    /* Headers */
    r->p_headers = js_newobj(op);
    iface(g, "Headers", r->p_headers, headers_ctor, 0, NULL);
    static const char *hm[] = { "append", "delete", "get", "has", "set", "getSetCookie", "forEach", "entries", "keys", "values" };
    static const int ha[] = { 2, 1, 1, 1, 2, 0, 1, 0, 0, 0 };
    for (int i = 0; i < 10; i++) js_defnative(r->p_headers, hm[i], headers_method, ha[i], i);
    js_defprop(r->p_headers, A[A_sym_iterator], js_getn(jv_obj(r->p_headers), "entries"), PF_NOENUM);
    /* FormData */
    r->p_formdata = js_newobj(op);
    iface(g, "FormData", r->p_formdata, fd_ctor, 0, NULL);
    static const char *fm[] = { "append", "delete", "get", "getAll", "has", "set", "forEach", "entries", "keys", "values" };
    static const int fa[] = { 2, 1, 1, 1, 1, 2, 1, 0, 0, 0 };
    for (int i = 0; i < 10; i++) js_defnative(r->p_formdata, fm[i], fd_method, fa[i], i);
    js_defprop(r->p_formdata, A[A_sym_iterator], js_getn(jv_obj(r->p_formdata), "entries"), PF_NOENUM);
    /* corpo comum */
    static const char *bm[] = { "text", "json", "arrayBuffer", "blob", "bytes", "formData" };
    /* Request */
    r->p_request = js_newobj(op);
    iface(g, "Request", r->p_request, request_ctor, 1, NULL);
    for (int i = 0; i < 6; i++) js_defnative(r->p_request, bm[i], body_read, 0, i);
    js_defgetter(r->p_request, "bodyUsed", body_used_get, 0);
    js_defgetter(r->p_request, "headers", request_get, B_HDR);
    for (int i = R_URL; i < R_NSLOTS; i++) js_defgetter(r->p_request, req_names[i], request_get, i);
    js_defnative(r->p_request, "clone", body_clone, 0, 0);
    /* Response */
    r->p_response = js_newobj(op);
    jsobj *rc;
    iface(g, "Response", r->p_response, response_ctor, 0, &rc);
    for (int i = 0; i < 6; i++) js_defnative(r->p_response, bm[i], body_read, 0, i);
    js_defgetter(r->p_response, "bodyUsed", body_used_get, 0);
    js_defgetter(r->p_response, "headers", response_get, B_HDR);
    js_defgetter(r->p_response, "url", response_get, S_URL);
    js_defgetter(r->p_response, "status", response_get, S_STATUS);
    js_defgetter(r->p_response, "statusText", response_get, S_STATUSTEXT);
    js_defgetter(r->p_response, "type", response_get, S_TYPE);
    js_defgetter(r->p_response, "redirected", response_get, S_REDIRECTED);
    js_defgetter(r->p_response, "ok", response_get, 8);
    js_defnative(r->p_response, "clone", body_clone, 0, 1);
    js_defnative(rc, "error", response_static, 0, 0);
    js_defnative(rc, "redirect", response_static, 1, 1);
    js_defnative(rc, "json", response_static, 1, 2);
    /* AbortController / AbortSignal */
    jsobj *et = r->p_node ? r->p_node->proto : op;
    r->p_abortsig = js_newobj(et);
    jsobj *asc;
    iface(g, "AbortSignal", r->p_abortsig, illegal, 0, &asc);
    js_defgetter(r->p_abortsig, "aborted", as_method, 0);
    js_defgetter(r->p_abortsig, "reason", as_method, 1);
    js_defnative(r->p_abortsig, "throwIfAborted", as_method, 0, 2);
    js_defnative(r->p_abortsig, "addEventListener", as_method, 2, 3);
    js_defnative(r->p_abortsig, "removeEventListener", as_method, 2, 4);
    js_defnative(r->p_abortsig, "dispatchEvent", as_method, 1, 5);
    js_defn(r->p_abortsig, "onabort", JS_NULL, 0);
    js_defnative(asc, "abort", as_static, 0, 0);
    js_defnative(asc, "timeout", as_static, 1, 1);
    js_defnative(asc, "any", as_static, 1, 2);
    r->p_abortctl = js_newobj(op);
    iface(g, "AbortController", r->p_abortctl, ac_ctor, 0, NULL);
    js_defgetter(r->p_abortctl, "signal", ac_method, 0);
    js_defnative(r->p_abortctl, "abort", ac_method, 0, 1);
    /* fetch */
    js_defnative(g, "fetch", w_fetch, 1, 0);
    /* XMLHttpRequest e XMLHttpRequestUpload */
    r->p_xhrup = js_newobj(et);
    iface(g, "XMLHttpRequestUpload", r->p_xhrup, illegal, 0, NULL);
    r->p_xhr = js_newobj(et);
    jsobj *xc;
    iface(g, "XMLHttpRequest", r->p_xhr, xhr_ctor, 0, &xc);
    static const char *xm[] = { "open", "setRequestHeader", "send", "abort", "getResponseHeader", "getAllResponseHeaders", "overrideMimeType" };
    static const int xa[] = { 2, 2, 0, 0, 1, 0, 1 };
    for (int i = 0; i < 7; i++) js_defnative(r->p_xhr, xm[i], xhr_method, xa[i], i);
    static const char *ls[] = { "addEventListener", "removeEventListener", "dispatchEvent" };
    for (int i = 0; i < 3; i++) {
        jsobj *f = js_defnative(r->p_xhr, ls[i], xhr_listen, i == 2 ? 1 : 2, i);
        f->u.nat.data = JS_FALSE;
        jsobj *fu = js_defnative(r->p_xhrup, ls[i], xhr_listen, i == 2 ? 1 : 2, i);
        fu->u.nat.data = JS_TRUE;
    }
    static const char *xg[] = { "readyState", "status", "statusText", "response", "responseText", "responseURL", "responseType",
                                "timeout", "withCredentials", "upload", "responseXML" };
    for (int i = 0; i < 11; i++) {
        bool rw = i >= 6 && i <= 8;
        js_defaccessor(r->p_xhr, js_intern(xg[i], -1), jv_obj(js_newnative(xhr_get, xg[i], 0, i)),
                       rw ? jv_obj(js_newnative(xhr_set, xg[i], 1, i)) : JS_UNDEF, 0);
    }
    static const char *xs[] = { "UNSENT", "OPENED", "HEADERS_RECEIVED", "LOADING", "DONE" };
    for (int i = 0; i < 5; i++) { js_defn(xc, xs[i], jv_int(i), PF_RO); js_defn(r->p_xhr, xs[i], jv_int(i), PF_RO); }
    static const char *on[] = { "onreadystatechange", "onloadstart", "onprogress", "onload", "onloadend", "onerror", "onabort", "ontimeout" };
    for (int i = 0; i < 8; i++) { js_defn(r->p_xhr, on[i], JS_NULL, 0); if (i) js_defn(r->p_xhrup, on[i], JS_NULL, 0); }
    /* ProgressEvent construível */
    jsobj *pc = js_newctor(illegal, "ProgressEvent", 1, r->p_progress);
    js_defn(g, "ProgressEvent", jv_obj(pc), PF_NOENUM);
}
