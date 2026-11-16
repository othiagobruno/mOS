/* mOS — Streams do motor web: carrega a implementação em JS (web_stream.js, embutida em web_stream_js.h) na primeira vez
 * que a página usa ReadableStream, WritableStream, TransformStream e companhia (CompressionStream e DecompressionStream
 * usam o deflate de web_zlib.c), e liga Response.body, Request.body e Blob.stream() a ela. */
#include "js_int.h"
#include "web.h"
#include "web_stream_js.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

static const char *names[] = { "ReadableStream", "ReadableStreamDefaultReader", "ReadableStreamBYOBReader", "ReadableStreamDefaultController",
                               "ReadableByteStreamController", "WritableStream", "WritableStreamDefaultWriter", "WritableStreamDefaultController",
                               "TransformStream", "TransformStreamDefaultController", "CountQueuingStrategy", "ByteLengthQueuingStrategy",
                               "TextEncoderStream", "TextDecoderStream",
                               "CompressionStream", "DecompressionStream" };

/* objeto {tags, fromBytes} do reino atual; avalia a fonte na primeira vez */
static jsv bundle(void) {
    jsobj *g = J.realm->global;
    jsstr *k = js_intern("\x01" "streams", -1);
    jsprop *p = js_ownprop(g, k);
    if (p && jv_isobj(p->v)) return p->v;
    jsstr *err = NULL;
    jsscript *sc = js_parse(web_stream_src, (int)sizeof web_stream_src - 1, "streams", 1, &err);
    if (!sc) return js_throw_err(E_SYNTAX, "streams: %s", err ? err->d : "erro de sintaxe");
    jsv fn = js_run_script(sc);
    if (fn == JS_EXC) return JS_EXC;
    jsv a[2] = { jv_obj(g), js_zlib_natives() };
    jsv b = js_call(fn, JS_UNDEF, 2, a);
    if (b == JS_EXC) return JS_EXC;
    js_defprop(g, k, b, PF_NOENUM);
    /* troca os acessores preguiçosos pelos construtores de verdade */
    jsv tags = js_getn(b, "tags");
    for (u32 i = 0; i < ARRAY_LEN(names); i++) {
        jsstr *n = js_intern(names[i], -1);
        js_delprop(g, n);
        js_defprop(g, n, js_getk(tags, n), PF_NOENUM);
    }
    return b;
}

NATIVE(lazy_get) {
    UNUSED_ARGS;
    jsv b = bundle();
    if (b == JS_EXC) return JS_EXC;
    return js_getn(jv_obj(J.realm->global), names[self->u.nat.magic]);
}

/* atribuir antes de usar: vira propriedade comum, como num navegador */
NATIVE(lazy_set) {
    UNUSED_ARGS;
    jsobj *g = J.realm->global;
    jsstr *n = js_intern(names[self->u.nat.magic], -1);
    js_delprop(g, n);
    js_defprop(g, n, ARG(0), PF_NOENUM);
    return JS_UNDEF;
}

/* stream de bytes sobre um ArrayBuffer; on_read (nativo) devolve false se o corpo já foi usado */
jsv js_stream_from_bytes(jsv abuf, jsv on_read) {
    jsv b = bundle();
    if (b == JS_EXC) return JS_EXC;
    jsv f = js_getn(b, "fromBytes");
    jsv a[2] = { abuf, on_read };
    return js_call(f, JS_UNDEF, 2, a);
}

/* promessa de ArrayBuffer com todos os bytes do stream */
jsv js_stream_collect(jsv stream) {
    jsv b = bundle();
    if (b == JS_EXC) return JS_EXC;
    return js_call(js_getn(b, "collect"), JS_UNDEF, 1, &stream);
}

/* só existe ReadableStream se a fonte já foi carregada */
bool js_is_readable_stream(jsv v) {
    if (!jv_isobj(v)) return false;
    jsprop *p = js_ownprop(J.realm->global, js_intern("\x01" "streams", -1));
    if (!p || !jv_isobj(p->v)) return false;
    jsv r = js_call(js_getn(p->v, "isStream"), JS_UNDEF, 1, &v);
    return r == JS_TRUE;
}

NATIVE(blob_stream) {
    UNUSED_ARGS;
    const u8 *d;
    u32 n;
    if (!js_blob_bytes(thisv, &d, &n, NULL)) return js_throw_err(E_TYPE, "Invocação ilegal: não é um Blob");
    jsobj *buf = js_abuf_new(n);
    if (!buf) return JS_EXC;
    if (n) memcpy(buf->u.buf.data, d, n);
    return js_stream_from_bytes(jv_obj(buf), JS_UNDEF);
}

void js_stream_init(jsrealm *r) {
    jsobj *g = r->global;
    for (u32 i = 0; i < ARRAY_LEN(names); i++)
        js_defaccessor(g, js_intern(names[i], -1), jv_obj(js_newnative(lazy_get, names[i], 0, (int)i)),
                       jv_obj(js_newnative(lazy_set, names[i], 1, (int)i)), PF_NOENUM);
    if (r->p_blob) js_defnative(r->p_blob, "stream", blob_stream, 0, 0);
}
