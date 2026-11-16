/* mOS — Blob, File, FileReader e URLs blob: do motor web (File API do W3C). Os bytes moram num ArrayBuffer do
 * coletor; URL.createObjectURL registra o Blob na página e os blob: valem em <img>, fetch e XMLHttpRequest. */
#include "js_int.h"
#include "web.h"
#include "crypto.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

/* extra de um Blob: [ArrayBuffer, tipo, nome (File), lastModified (File)] */
enum { BX_BUF, BX_TYPE, BX_NAME, BX_MOD };

static bool has_proto(jsobj *o, jsobj *p) {
    for (int i = 0; o && i < 32; i++, o = o->proto) if (o->proto == p) return true;
    return false;
}

static jsobj *blob_parts(jsv v) {
    if (!jv_iscls(v, CL_HOST) || !J.realm->p_blob || !has_proto(jv_o(v), J.realm->p_blob)) return NULL;
    jsv x = jv_o(v)->u.host.extra;
    return jv_isobj(x) ? jv_o(x) : NULL;
}

bool js_blob_bytes(jsv v, const u8 **d, u32 *n, const char **type) {
    jsobj *x = blob_parts(v);
    if (!x) return false;
    jsv b = js_arr_get(x, BX_BUF), t = js_arr_get(x, BX_TYPE);
    if (!jv_iscls(b, CL_ARRAYBUFFER)) return false;
    *d = jv_o(b)->u.buf.data;
    *n = jv_o(b)->u.buf.len;
    if (type) *type = jv_isstr(t) ? jv_s(t)->d : "";
    return true;
}

/* tipo MIME de Blob: ASCII imprimível em minúsculas; qualquer outro caractere zera o tipo */
static jsstr *norm_type(jsstr *t) {
    if (!t) return A[A_empty];
    char *b = kmalloc(t->len + 1);
    if (!b) return A[A_empty];
    for (u32 i = 0; i < t->len; i++) {
        u8 c = (u8)t->d[i];
        if (c < 0x20 || c > 0x7E) { kfree(b); return A[A_empty]; }
        b[i] = (char)tolower(c);
    }
    jsstr *r = js_strn(b, (int)t->len);
    kfree(b);
    return r;
}

static jsobj *make_blob(jsobj *proto, jsobj *buf, jsstr *type) {
    jsobj *o = js_newobj_cls(CL_HOST, proto);
    o->u.host.page = js_cur_page();
    jsobj *x = js_newarr(0);
    js_arr_push(x, jv_obj(buf));
    js_arr_push(x, jv_str(type ? type : A[A_empty]));
    o->u.host.extra = jv_obj(x);
    return o;
}

jsobj *js_blob_new(const u8 *d, u32 n, const char *type) {
    jsobj *b = js_abuf_new(n);
    if (!b) return NULL;
    if (n) memcpy(b->u.buf.data, d, n);
    return make_blob(J.realm->p_blob, b, js_strz(type ? type : ""));
}

/* junta as partes (strings em UTF-8, ArrayBuffer, views e Blobs) */
static jsobj *join_parts(jsv parts) {
    sbuf_t sb = { 0 };
    if (parts != JS_UNDEF) {
        if (!jv_isobj(parts)) { js_throw_err(E_TYPE, "Blob: as partes devem ser uma sequência"); return NULL; }
        u32 n = js_lengthof(parts);
        if (js_pending()) return NULL;
        for (u32 i = 0; i < n; i++) {
            jsv v = js_getidx(parts, i);
            if (js_pending()) { sb_free(&sb); return NULL; }
            const u8 *d = NULL;
            u32 len = 0;
            if (jv_iscls(v, CL_ARRAYBUFFER)) { d = jv_o(v)->u.buf.data; len = jv_o(v)->u.buf.detached ? 0 : jv_o(v)->u.buf.len; }
            else if (jv_iscls(v, CL_TYPED) || jv_iscls(v, CL_DATAVIEW)) {
                jsobj *t = jv_o(v), *b = t->u.ta.buffer;
                if (b && !b->u.buf.detached) {
                    d = b->u.buf.data + t->u.ta.off;
                    len = t->cls == CL_DATAVIEW ? t->u.ta.len : js_typed_length(t) * (u32)js_typed_elsize(t->u.ta.kind);
                }
            } else if (js_blob_bytes(v, &d, &len, NULL)) { /* bytes do Blob */ }
            else {
                jsstr *s = js_tostr(v);
                if (!s) { sb_free(&sb); return NULL; }
                /* USVString: surrogates soltos (3 bytes WTF-8 ED A0..BF xx) viram U+FFFD */
                for (u32 k = 0; k < s->len; k++) {
                    u8 c = (u8)s->d[k];
                    if (c == 0xED && k + 2 < s->len && (u8)s->d[k + 1] >= 0xA0) { sb_puts(&sb, "\xEF\xBF\xBD"); k += 2; }
                    else sb_putc(&sb, (char)c);
                }
                continue;
            }
            if (len) sb_putn(&sb, (const char *)d, (int)len);
        }
    }
    jsobj *b = js_abuf_new((u32)sb.len);
    if (b && sb.len) memcpy(b->u.buf.data, sb.p, (usize)sb.len);
    sb_free(&sb);
    return b;
}

NATIVE(blob_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor Blob exige 'new'");
    jsobj *buf = join_parts(ARG(0));
    if (!buf) return JS_EXC;
    jsstr *type = A[A_empty];
    if (jv_isobj(ARG(1))) {
        jsv t = js_getn(ARG(1), "type");
        if (js_pending()) return JS_EXC;
        if (t != JS_UNDEF) { jsstr *ts = js_tostr(t); if (!ts) return JS_EXC; type = norm_type(ts); }
    }
    jsobj *proto = jv_isobj(thisv) && jv_o(thisv)->proto ? jv_o(thisv)->proto : J.realm->p_blob;   /* subclasses */
    return jv_obj(make_blob(proto, buf, type));
}

NATIVE(file_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor File exige 'new'");
    if (argc < 2) return js_throw_err(E_TYPE, "File: são necessários 2 argumentos");
    jsobj *buf = join_parts(ARG(0));
    if (!buf) return JS_EXC;
    jsstr *name = js_tostr(ARG(1));
    if (!name) return JS_EXC;
    jsstr *type = A[A_empty];
    double mod = js_date_now();
    if (jv_isobj(ARG(2))) {
        jsv t = js_getn(ARG(2), "type");
        if (js_pending()) return JS_EXC;
        if (t != JS_UNDEF) { jsstr *ts = js_tostr(t); if (!ts) return JS_EXC; type = norm_type(ts); }
        jsv m = js_getn(ARG(2), "lastModified");
        if (js_pending()) return JS_EXC;
        if (m != JS_UNDEF) { mod = js_tointeger(m); if (js_pending()) return JS_EXC; }
    }
    jsobj *proto = jv_isobj(thisv) && jv_o(thisv)->proto ? jv_o(thisv)->proto : J.realm->p_file;
    jsobj *o = make_blob(proto, buf, type);
    jsobj *x = jv_o(o->u.host.extra);
    js_arr_push(x, jv_str(name));
    js_arr_push(x, jv_num(mod));
    return jv_obj(o);
}

/* getters: 0 size 1 type 2 name 3 lastModified 4 webkitRelativePath */
NATIVE(blob_get) {
    UNUSED_ARGS;
    jsobj *x = blob_parts(thisv);
    if (!x) return js_throw_err(E_TYPE, "Invocação ilegal: não é um Blob");
    switch (self->u.nat.magic) {
    case 0: { jsv b = js_arr_get(x, BX_BUF); return jv_int(jv_iscls(b, CL_ARRAYBUFFER) ? (int)jv_o(b)->u.buf.len : 0); }
    case 1: return js_arr_get(x, BX_TYPE);
    case 2: return x->alen > BX_NAME ? js_arr_get(x, BX_NAME) : JS_UNDEF;
    case 3: return x->alen > BX_MOD ? js_arr_get(x, BX_MOD) : JS_UNDEF;
    case 4: return jv_str(A[A_empty]);
    }
    return JS_UNDEF;
}

static u32 rel(jsv v, u32 len, u32 def, bool *exc) {
    if (v == JS_UNDEF) return def;
    double d = js_tointeger(v);
    if (js_pending()) { *exc = true; return 0; }
    if (d < 0) d = d + len < 0 ? 0 : d + len;
    return d > len ? len : (u32)d;
}

NATIVE(blob_slice) {
    UNUSED_ARGS;
    const u8 *d;
    u32 n;
    if (!js_blob_bytes(thisv, &d, &n, NULL)) return js_throw_err(E_TYPE, "Invocação ilegal: não é um Blob");
    bool exc = false;
    u32 a = rel(ARG(0), n, 0, &exc), b = rel(ARG(1), n, n, &exc);
    if (exc) return JS_EXC;
    if (b < a) b = a;
    jsstr *type = A[A_empty];
    if (ARG(2) != JS_UNDEF) { jsstr *t = js_tostr(ARG(2)); if (!t) return JS_EXC; type = norm_type(t); }
    js_blob_bytes(thisv, &d, &n, NULL);   /* ToInteger pode ter rodado código */
    jsobj *buf = js_abuf_new(b - a);
    if (!buf) return JS_EXC;
    if (b > a && b <= n) memcpy(buf->u.buf.data, d + a, b - a);
    return jv_obj(make_blob(J.realm->p_blob, buf, type));
}

/* magic: 0 text 1 arrayBuffer 2 bytes */
NATIVE(blob_read) {
    UNUSED_ARGS;
    const u8 *d;
    u32 n;
    if (!js_blob_bytes(thisv, &d, &n, NULL)) return js_throw_err(E_TYPE, "Invocação ilegal: não é um Blob");
    jsobj *pr = js_promise_new();
    if (self->u.nat.magic == 0) {
        bool bad;
        jsstr *s = js_utf8_decode(d, n, false, &bad);
        js_promise_resolve(pr, s ? jv_str(s) : jv_str(A[A_empty]));
    } else {
        jsobj *b = js_abuf_new(n);
        if (!b) { js_promise_reject(pr, js_catch()); return jv_obj(pr); }
        if (n) memcpy(b->u.buf.data, d, n);
        js_promise_resolve(pr, self->u.nat.magic == 1 ? jv_obj(b) : jv_obj(js_typed_new(TA_UINT8, b, 0, n)));
    }
    return jv_obj(pr);
}

/* ============================================================ FileReader */
/* estado no extra: [readyState, result, error, ouvintes, abortado, geração] */
enum { FR_STATE, FR_RESULT, FR_ERROR, FR_LISTEN, FR_ABORT, FR_GEN };

static jsobj *this_reader(jsv thisv) {
    if (jv_iscls(thisv, CL_HOST) && has_proto(jv_o(thisv), J.realm->p_freader) && jv_isobj(jv_o(thisv)->u.host.extra)) return jv_o(jv_o(thisv)->u.host.extra);
    js_throw_err(E_TYPE, "Invocação ilegal: não é um FileReader");
    return NULL;
}

static void fr_fire(jsobj *r, const char *type, u32 loaded, u32 total) {
    jsobj *x = jv_o(r->u.host.extra);
    jsobj *ev = js_newobj(J.realm->p_progress ? J.realm->p_progress : J.realm->p_object);
    js_defn(ev, "type", jv_str(js_intern(type, -1)), 0);
    js_defn(ev, "target", jv_obj(r), 0);
    js_defn(ev, "currentTarget", jv_obj(r), 0);
    js_defn(ev, "lengthComputable", JS_TRUE, 0);
    js_defn(ev, "loaded", jv_int((int)loaded), 0);
    js_defn(ev, "total", jv_int((int)total), 0);
    js_defn(ev, "bubbles", JS_FALSE, 0);
    js_defn(ev, "cancelable", JS_FALSE, 0);
    js_defn(ev, "timeStamp", jv_num(js_date_now()), 0);
    char on[32];
    snprintf(on, sizeof on, "on%s", type);
    jsv h = js_getn(jv_obj(r), on);
    if (js_pending()) js_report_exception();
    else if (js_iscallable(h)) {
        jsv a = jv_obj(ev);
        if (js_call(h, jv_obj(r), 1, &a) == JS_EXC) js_report_exception();
    }
    jsv ls = js_arr_get(x, FR_LISTEN);
    if (!jv_isobj(ls)) return;
    jsobj *l = js_arr_from(jv_o(ls)->arr, (int)jv_o(ls)->alen);   /* cópia: o ouvinte pode mexer na lista */
    for (u32 i = 0; i + 1 < l->alen; i += 2) {
        if (!jv_isstr(l->arr[i]) || strcmp(jv_s(l->arr[i])->d, type)) continue;
        jsv f = l->arr[i + 1], a = jv_obj(ev);
        jsv rv = js_iscallable(f) ? js_call(f, jv_obj(r), 1, &a) : js_invoke(f, js_intern("handleEvent", -1), 1, &a);
        if (rv == JS_EXC) js_report_exception();
    }
}

/* tarefa da leitura: magic do nativo guarda o modo; data = leitor; data2 = [blob, geração, codificação] */
NATIVE(fr_task) {
    UNUSED_ARGS;
    jsobj *r = jv_o(self->u.nat.data), *x = jv_o(r->u.host.extra);
    jsobj *args = jv_o(self->u.nat.data2);
    if (js_arr_get(x, FR_GEN) != js_arr_get(args, 1) || js_truthy(js_arr_get(x, FR_ABORT))) return JS_UNDEF;
    const u8 *d = NULL;
    u32 n = 0;
    const char *type = "";
    js_blob_bytes(js_arr_get(args, 0), &d, &n, &type);
    fr_fire(r, "loadstart", 0, n);
    if (js_arr_get(x, FR_GEN) != js_arr_get(args, 1) || js_truthy(js_arr_get(x, FR_ABORT))) return JS_UNDEF;
    js_blob_bytes(js_arr_get(args, 0), &d, &n, &type);
    jsv res = JS_NULL;
    switch (self->u.nat.magic) {
    case 0: {   /* arrayBuffer */
        jsobj *b = js_abuf_new(n);
        if (b && n) memcpy(b->u.buf.data, d, n);
        res = jv_obj(b);
        break;
    }
    case 1: {   /* texto: UTF-8 por padrão, ou a codificação pedida */
        jsv enc = js_arr_get(args, 2);
        const char *label = jv_isstr(enc) ? jv_s(enc)->d : "";
        bool utf8 = !*label || !strcasecmp(label, "utf-8") || !strcasecmp(label, "utf8");
        if (n >= 2 && ((d[0] == 0xFF && d[1] == 0xFE) || (d[0] == 0xFE && d[1] == 0xFF))) utf8 = false;
        if (utf8 || (n >= 3 && d[0] == 0xEF && d[1] == 0xBB && d[2] == 0xBF)) {
            bool bad;
            jsstr *s = js_utf8_decode(d, n, false, &bad);
            res = jv_str(s ? s : A[A_empty]);
        } else {
            usize ul = 0;
            char *u = charset_to_utf8(d, n, label, &ul);
            res = jv_str(u ? js_strn(u, (int)ul) : A[A_empty]);
            kfree(u);
        }
        break;
    }
    case 2: {   /* data: URL em base64 */
        static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        sbuf_t sb = { 0 };
        sb_puts(&sb, "data:");
        sb_puts(&sb, *type ? type : "application/octet-stream");
        sb_puts(&sb, ";base64,");
        for (u32 i = 0; i < n; i += 3) {
            u32 v = (u32)d[i] << 16 | (i + 1 < n ? (u32)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
            char q[4] = { b64[v >> 18], b64[(v >> 12) & 63], i + 1 < n ? b64[(v >> 6) & 63] : '=', i + 2 < n ? b64[v & 63] : '=' };
            sb_putn(&sb, q, 4);
        }
        res = jv_str(js_strn(sb.p, sb.len));
        sb_free(&sb);
        break;
    }
    case 3: {   /* binary string: um byte por unidade */
        u16 *u = kmalloc(sizeof(u16) * (n ? n : 1));
        if (u) { for (u32 i = 0; i < n; i++) u[i] = d[i]; res = jv_str(js_str_from_units(u, (int)n)); kfree(u); }
        break;
    }
    }
    js_arr_set(x, FR_STATE, jv_int(2));
    js_arr_set(x, FR_RESULT, res);
    fr_fire(r, "progress", n, n);
    fr_fire(r, "load", n, n);
    fr_fire(r, "loadend", n, n);
    return JS_UNDEF;
}

/* magic: 0 readAsArrayBuffer 1 readAsText 2 readAsDataURL 3 readAsBinaryString */
NATIVE(fr_read) {
    UNUSED_ARGS;
    jsobj *x = this_reader(thisv);
    if (!x) return JS_EXC;
    if (argc < 1) return js_throw_err(E_TYPE, "FileReader: é necessário um Blob");
    const u8 *d;
    u32 n;
    if (!js_blob_bytes(ARG(0), &d, &n, NULL)) return js_throw_err(E_TYPE, "FileReader: o argumento não é um Blob");
    if (js_toint32(js_arr_get(x, FR_STATE)) == 1) return js_throw_dom("InvalidStateError", "FileReader: já está lendo");
    jsv enc = JS_UNDEF;
    if (self->u.nat.magic == 1 && ARG(1) != JS_UNDEF) { jsstr *e = js_tostr(ARG(1)); if (!e) return JS_EXC; enc = jv_str(e); }
    double gen = js_tonum(js_arr_get(x, FR_GEN)) + 1;
    js_arr_set(x, FR_GEN, jv_num(gen));
    js_arr_set(x, FR_STATE, jv_int(1));
    js_arr_set(x, FR_RESULT, JS_NULL);
    js_arr_set(x, FR_ERROR, JS_NULL);
    js_arr_set(x, FR_ABORT, JS_FALSE);
    jsobj *task = js_newnative(fr_task, "leitura", 0, self->u.nat.magic);
    task->u.nat.data = thisv;
    jsv a3[3] = { ARG(0), jv_num(gen), enc };
    task->u.nat.data2 = jv_obj(js_arr_from(a3, 3));
    js_page_add_timer(js_cur_page(), jv_obj(task), 0);
    return JS_UNDEF;
}

NATIVE(fr_abort) {
    UNUSED_ARGS;
    jsobj *x = this_reader(thisv);
    if (!x) return JS_EXC;
    if (js_toint32(js_arr_get(x, FR_STATE)) != 1) return JS_UNDEF;
    js_arr_set(x, FR_STATE, jv_int(2));
    js_arr_set(x, FR_RESULT, JS_NULL);
    js_arr_set(x, FR_ABORT, JS_TRUE);
    fr_fire(jv_o(thisv), "abort", 0, 0);
    fr_fire(jv_o(thisv), "loadend", 0, 0);
    return JS_UNDEF;
}

/* getters: 0 readyState 1 result 2 error */
NATIVE(fr_get) {
    UNUSED_ARGS;
    jsobj *x = this_reader(thisv);
    if (!x) return JS_EXC;
    return js_arr_get(x, self->u.nat.magic == 0 ? FR_STATE : self->u.nat.magic == 1 ? FR_RESULT : FR_ERROR);
}

/* magic 0 add, 1 remove */
NATIVE(fr_listen) {
    UNUSED_ARGS;
    jsobj *x = this_reader(thisv);
    if (!x) return JS_EXC;
    jsstr *t = js_tostr(ARG(0));
    if (!t) return JS_EXC;
    jsv f = ARG(1);
    if (!jv_isobj(f)) return JS_UNDEF;
    jsv ls = js_arr_get(x, FR_LISTEN);
    if (!jv_isobj(ls)) { ls = jv_obj(js_newarr(0)); js_arr_set(x, FR_LISTEN, ls); }
    jsobj *l = jv_o(ls);
    for (u32 i = 0; i + 1 < l->alen; i += 2) {
        if (!jv_isstr(l->arr[i]) || strcmp(jv_s(l->arr[i])->d, t->d) || l->arr[i + 1] != f) continue;
        if (self->u.nat.magic == 0) return JS_UNDEF;
        memmove(l->arr + i, l->arr + i + 2, sizeof(jsv) * (l->alen - i - 2));
        js_arr_setlen(l, l->alen - 2);
        return JS_UNDEF;
    }
    if (self->u.nat.magic == 0) { js_arr_push(l, jv_str(t)); js_arr_push(l, f); }
    return JS_UNDEF;
}

NATIVE(fr_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor FileReader exige 'new'");
    jsobj *o = js_newobj_cls(CL_HOST, jv_isobj(thisv) && jv_o(thisv)->proto ? jv_o(thisv)->proto : J.realm->p_freader);
    o->u.host.page = js_cur_page();
    jsv init[6] = { jv_int(0), JS_NULL, JS_NULL, JS_UNDEF, JS_FALSE, jv_int(0) };
    o->u.host.extra = jv_obj(js_arr_from(init, 6));
    return jv_obj(o);
}

/* ============================================================ URLs blob: */
static jsobj *registry(bool create) {
    jsobj *g = J.realm->global;
    jsstr *k = js_intern("\x01" "blobs", -1);
    jsprop *p = js_ownprop(g, k);
    if (p && jv_isobj(p->v)) return jv_o(p->v);
    if (!create) return NULL;
    jsobj *r = js_newobj(NULL);
    js_defprop(g, k, jv_obj(r), PF_NOENUM);
    return r;
}

NATIVE(url_create_object) {
    UNUSED_ARGS;
    const u8 *d;
    u32 n;
    if (!js_blob_bytes(ARG(0), &d, &n, NULL)) return js_throw_err(E_TYPE, "URL.createObjectURL: Blob esperado");
    u8 r[16];
    crypto_random(r, sizeof r);
    r[6] = (r[6] & 0x0F) | 0x40;
    r[8] = (r[8] & 0x3F) | 0x80;
    const char *origin = js_page_origin(js_cur_page());
    char url[400];
    snprintf(url, sizeof url, "blob:%s/%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             origin && *origin ? origin : "null", r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10], r[11], r[12], r[13], r[14], r[15]);
    jsobj *reg = registry(true);
    js_defprop(reg, js_strz(url), ARG(0), 0);
    return jv_str(js_strz(url));
}

NATIVE(url_revoke_object) {
    UNUSED_ARGS;
    jsstr *u = js_tostr(ARG(0));
    if (!u) return JS_EXC;
    jsobj *reg = registry(false);
    if (reg) js_delprop(reg, u);
    return JS_UNDEF;
}

/* Blob de um URL blob: (sem o fragmento) na página atual */
jsv js_blob_from_url(const char *url) {
    if (strncmp(url, "blob:", 5)) return JS_UNDEF;
    const char *h = strchr(url, '#');
    jsstr *k = js_strn(url, h ? (int)(h - url) : (int)strlen(url));
    /* o registro de URLs blob: é da origem: vale para a página e os workers dela */
    jsstr *key = js_intern("\x01" "blobs", -1);
    for (int pass = 0; pass < 2; pass++)
        for (jsrealm *r = pass ? J.realms : J.realm; r; r = pass ? r->next : NULL) {
            jsprop *rp = js_ownprop(r->global, key);
            if (rp && jv_isobj(rp->v)) { jsprop *p = js_ownprop(jv_o(rp->v), k); if (p) return p->v; }
        }
    return JS_UNDEF;
}

/* ============================================================ registro */
void js_blob_init(jsrealm *r) {
    jsobj *g = r->global, *op = r->p_object;
    r->p_blob = js_newobj(op);
    jsobj *bc = js_newctor(blob_ctor, "Blob", 0, r->p_blob);
    js_defn(g, "Blob", jv_obj(bc), PF_NOENUM);
    js_defprop(r->p_blob, A[A_sym_toStringTag], jv_str(js_intern("Blob", -1)), PF_NOENUM | PF_RO);
    js_defgetter(r->p_blob, "size", blob_get, 0);
    js_defgetter(r->p_blob, "type", blob_get, 1);
    js_defnative(r->p_blob, "slice", blob_slice, 0, 0);
    js_defnative(r->p_blob, "text", blob_read, 0, 0);
    js_defnative(r->p_blob, "arrayBuffer", blob_read, 0, 1);
    js_defnative(r->p_blob, "bytes", blob_read, 0, 2);
    r->p_file = js_newobj(r->p_blob);
    jsobj *fc = js_newctor(file_ctor, "File", 2, r->p_file);
    fc->proto = bc;
    js_defn(g, "File", jv_obj(fc), PF_NOENUM);
    js_defprop(r->p_file, A[A_sym_toStringTag], jv_str(js_intern("File", -1)), PF_NOENUM | PF_RO);
    js_defgetter(r->p_file, "name", blob_get, 2);
    js_defgetter(r->p_file, "lastModified", blob_get, 3);
    js_defgetter(r->p_file, "webkitRelativePath", blob_get, 4);
    /* ProgressEvent (para os eventos do FileReader) */
    r->p_progress = js_newobj(r->p_event ? r->p_event : op);
    /* FileReader */
    r->p_freader = js_newobj(op);
    jsobj *frc = js_newctor(fr_ctor, "FileReader", 0, r->p_freader);
    js_defn(g, "FileReader", jv_obj(frc), PF_NOENUM);
    js_defprop(r->p_freader, A[A_sym_toStringTag], jv_str(js_intern("FileReader", -1)), PF_NOENUM | PF_RO);
    static const char *reads[] = { "readAsArrayBuffer", "readAsText", "readAsDataURL", "readAsBinaryString" };
    for (int i = 0; i < 4; i++) js_defnative(r->p_freader, reads[i], fr_read, 1, i);
    js_defnative(r->p_freader, "abort", fr_abort, 0, 0);
    js_defnative(r->p_freader, "addEventListener", fr_listen, 2, 0);
    js_defnative(r->p_freader, "removeEventListener", fr_listen, 2, 1);
    js_defgetter(r->p_freader, "readyState", fr_get, 0);
    js_defgetter(r->p_freader, "result", fr_get, 1);
    js_defgetter(r->p_freader, "error", fr_get, 2);
    static const char *st[] = { "EMPTY", "LOADING", "DONE" };
    for (int i = 0; i < 3; i++) { js_defn(frc, st[i], jv_int(i), PF_RO); js_defn(r->p_freader, st[i], jv_int(i), PF_RO); }
    static const char *on[] = { "onloadstart", "onprogress", "onload", "onabort", "onerror", "onloadend" };
    for (int i = 0; i < 6; i++) js_defn(r->p_freader, on[i], JS_NULL, 0);
}

/* métodos estáticos de URL (o construtor URL fica em web_url.c) */
void js_blob_url_statics(jsobj *url_ctor) {
    js_defnative(url_ctor, "createObjectURL", url_create_object, 1, 0);
    js_defnative(url_ctor, "revokeObjectURL", url_revoke_object, 1, 0);
}
