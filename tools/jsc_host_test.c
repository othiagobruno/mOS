/* mOS — teste de host da API C do JavaScriptCore.framework: o que o MacDown (JSGlobalContextCreate, JSEvaluateScript,
 * JSStringCreateWithUTF8CString, JSValueCreateJSONString...) e o IINA (typed array NoCopy) usam, e o resto da API. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "jsc_api.h"

extern unsigned long jsc_test_stack_top;
#define D(ret, name, args) ret name args;
D(JSGlobalContextRef, JSGlobalContextCreate, (JSClassRef))
D(JSGlobalContextRef, JSGlobalContextCreateInGroup, (JSContextGroupRef, JSClassRef))
D(void, JSGlobalContextRelease, (JSGlobalContextRef))
D(JSContextGroupRef, JSContextGroupCreate, (void))
D(void, JSContextGroupRelease, (JSContextGroupRef))
D(JSObjectRef, JSContextGetGlobalObject, (JSContextRef))
D(JSValueRef, JSEvaluateScript, (JSContextRef, JSStringRef, JSObjectRef, JSStringRef, int, JSValueRef *))
D(bool, JSCheckScriptSyntax, (JSContextRef, JSStringRef, JSStringRef, int, JSValueRef *))
D(void, JSGarbageCollect, (JSContextRef))
D(JSStringRef, JSStringCreateWithUTF8CString, (const char *))
D(JSStringRef, JSStringCreateWithCharacters, (const JSChar *, unsigned long))
D(void, JSStringRelease, (JSStringRef))
D(unsigned long, JSStringGetLength, (JSStringRef))
D(unsigned long, JSStringGetMaximumUTF8CStringSize, (JSStringRef))
D(unsigned long, JSStringGetUTF8CString, (JSStringRef, char *, unsigned long))
D(bool, JSStringIsEqualToUTF8CString, (JSStringRef, const char *))
D(JSType, JSValueGetType, (JSContextRef, JSValueRef))
D(JSValueRef, JSValueMakeNumber, (JSContextRef, double))
D(JSValueRef, JSValueMakeString, (JSContextRef, JSStringRef))
D(JSValueRef, JSValueMakeBoolean, (JSContextRef, bool))
D(JSValueRef, JSValueMakeUndefined, (JSContextRef))
D(JSValueRef, JSValueMakeFromJSONString, (JSContextRef, JSStringRef))
D(JSStringRef, JSValueCreateJSONString, (JSContextRef, JSValueRef, unsigned, JSValueRef *))
D(double, JSValueToNumber, (JSContextRef, JSValueRef, JSValueRef *))
D(JSStringRef, JSValueToStringCopy, (JSContextRef, JSValueRef, JSValueRef *))
D(bool, JSValueToBoolean, (JSContextRef, JSValueRef))
D(JSObjectRef, JSValueToObject, (JSContextRef, JSValueRef, JSValueRef *))
D(bool, JSValueIsStrictEqual, (JSContextRef, JSValueRef, JSValueRef))
D(bool, JSValueIsObjectOfClass, (JSContextRef, JSValueRef, JSClassRef))
D(bool, JSValueIsArray, (JSContextRef, JSValueRef))
D(void, JSValueProtect, (JSContextRef, JSValueRef))
D(void, JSValueUnprotect, (JSContextRef, JSValueRef))
D(JSValueRef, JSObjectGetProperty, (JSContextRef, JSObjectRef, JSStringRef, JSValueRef *))
D(void, JSObjectSetProperty, (JSContextRef, JSObjectRef, JSStringRef, JSValueRef, JSPropertyAttributes, JSValueRef *))
D(bool, JSObjectDeleteProperty, (JSContextRef, JSObjectRef, JSStringRef, JSValueRef *))
D(JSValueRef, JSObjectGetPropertyAtIndex, (JSContextRef, JSObjectRef, unsigned, JSValueRef *))
D(JSObjectRef, JSObjectMake, (JSContextRef, JSClassRef, void *))
D(void *, JSObjectGetPrivate, (JSObjectRef))
D(JSObjectRef, JSObjectMakeFunctionWithCallback, (JSContextRef, JSStringRef, JSObjectCallAsFunctionCallback))
D(JSObjectRef, JSObjectMakeArray, (JSContextRef, unsigned long, const JSValueRef[], JSValueRef *))
D(JSObjectRef, JSObjectMakeConstructor, (JSContextRef, JSClassRef, JSObjectCallAsConstructorCallback))
D(JSObjectRef, JSObjectMakeFunction, (JSContextRef, JSStringRef, unsigned, const JSStringRef[], JSStringRef, JSStringRef, int, JSValueRef *))
D(JSValueRef, JSObjectCallAsFunction, (JSContextRef, JSObjectRef, JSObjectRef, unsigned long, const JSValueRef[], JSValueRef *))
D(JSObjectRef, JSObjectCallAsConstructor, (JSContextRef, JSObjectRef, unsigned long, const JSValueRef[], JSValueRef *))
D(bool, JSObjectIsFunction, (JSContextRef, JSObjectRef))
D(JSPropertyNameArrayRef, JSObjectCopyPropertyNames, (JSContextRef, JSObjectRef))
D(unsigned long, JSPropertyNameArrayGetCount, (JSPropertyNameArrayRef))
D(JSStringRef, JSPropertyNameArrayGetNameAtIndex, (JSPropertyNameArrayRef, unsigned long))
D(void, JSPropertyNameArrayRelease, (JSPropertyNameArrayRef))
D(void, JSPropertyNameAccumulatorAddName, (JSPropertyNameAccumulatorRef, JSStringRef))
D(JSClassRef, JSClassCreate, (const JSClassDefinition *))
D(void, JSClassRelease, (JSClassRef))
D(JSObjectRef, JSObjectMakeTypedArrayWithBytesNoCopy, (JSContextRef, JSTypedArrayType, void *, unsigned long, JSTypedArrayBytesDeallocator, void *, JSValueRef *))
D(JSTypedArrayType, JSValueGetTypedArrayType, (JSContextRef, JSValueRef, JSValueRef *))
D(void *, JSObjectGetTypedArrayBytesPtr, (JSContextRef, JSObjectRef, JSValueRef *))
D(unsigned long, JSObjectGetTypedArrayLength, (JSContextRef, JSObjectRef, JSValueRef *))
extern const JSClassDefinition kJSClassDefinitionEmpty;

static int ok, fail;
#define CHECK(c, ...) do { if (c) ok++; else { fail++; printf("FALHOU %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static JSGlobalContextRef ctx;
static char *cstr(JSStringRef s) {
    static char b[4096];
    JSStringGetUTF8CString(s, b, sizeof b);
    return b;
}
static JSValueRef ev(const char *src, JSValueRef *exc) {
    JSStringRef s = JSStringCreateWithUTF8CString(src);
    JSValueRef r = JSEvaluateScript(ctx, s, NULL, NULL, 1, exc);
    JSStringRelease(s);
    return r;
}
static const char *evs(const char *src) {
    static char b[4096];
    JSValueRef e = NULL;
    JSValueRef r = ev(src, &e);
    JSStringRef s = JSValueToStringCopy(ctx, r ? r : e, NULL);
    snprintf(b, sizeof b, "%s%s", r ? "" : "EXC:", s ? cstr(s) : "(nulo)");
    if (s) JSStringRelease(s);
    return b;
}
#define EQ(src, want) do { const char *r_ = evs(src); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", src, r_, want); } while (0)

/* callbacks */
static JSValueRef cb_soma(JSContextRef c, JSObjectRef f, JSObjectRef t, unsigned long n, const JSValueRef a[], JSValueRef *exc) {
    (void)f; (void)t;
    double s = 0;
    for (unsigned long i = 0; i < n; i++) s += JSValueToNumber(c, a[i], exc);
    return JSValueMakeNumber(c, s);
}
static JSValueRef cb_falha(JSContextRef c, JSObjectRef f, JSObjectRef t, unsigned long n, const JSValueRef a[], JSValueRef *exc) {
    (void)f; (void)t; (void)n; (void)a;
    JSStringRef m = JSStringCreateWithUTF8CString("falhou de propósito");
    *exc = JSValueMakeString(c, m);
    JSStringRelease(m);
    return NULL;
}
typedef struct { int x; int finalized; } ponto_t;
static int g_final;
static JSValueRef pt_get(JSContextRef c, JSObjectRef o, JSStringRef name, JSValueRef *exc) {
    (void)exc;
    ponto_t *p = JSObjectGetPrivate(o);
    if (JSStringIsEqualToUTF8CString(name, "dobro")) return JSValueMakeNumber(c, p->x * 2);
    return NULL;
}
static JSValueRef pt_x(JSContextRef c, JSObjectRef o, JSStringRef name, JSValueRef *exc) { (void)name; (void)exc; return JSValueMakeNumber(c, ((ponto_t *)JSObjectGetPrivate(o))->x); }
static bool pt_setx(JSContextRef c, JSObjectRef o, JSStringRef name, JSValueRef v, JSValueRef *exc) {
    (void)name;
    ((ponto_t *)JSObjectGetPrivate(o))->x = (int)JSValueToNumber(c, v, exc);
    return true;
}
static JSValueRef pt_inc(JSContextRef c, JSObjectRef f, JSObjectRef t, unsigned long n, const JSValueRef a[], JSValueRef *exc) {
    (void)f; (void)n; (void)a; (void)exc;
    ponto_t *p = JSObjectGetPrivate(t);
    p->x++;
    return JSValueMakeNumber(c, p->x);
}
static void pt_final(JSObjectRef o) { ponto_t *p = JSObjectGetPrivate(o); if (p) { g_final++; free(p); } }
static void pt_names(JSContextRef c, JSObjectRef o, JSPropertyNameAccumulatorRef acc) {
    (void)c; (void)o;
    JSStringRef s = JSStringCreateWithUTF8CString("dobro");
    JSPropertyNameAccumulatorAddName(acc, s);
    JSStringRelease(s);
}
static JSObjectRef pt_ctor(JSContextRef c, JSObjectRef ctor, unsigned long n, const JSValueRef a[], JSValueRef *exc);
static JSClassRef g_pt;
static JSObjectRef pt_ctor(JSContextRef c, JSObjectRef ctor, unsigned long n, const JSValueRef a[], JSValueRef *exc) {
    (void)ctor;
    ponto_t *p = calloc(1, sizeof *p);
    p->x = n ? (int)JSValueToNumber(c, a[0], exc) : 0;
    return JSObjectMake(c, g_pt, p);
}
static int g_dealloc;
static void dealloc(void *bytes, void *ctxp) { (void)bytes; g_dealloc += *(int *)ctxp; }

int main(void) {
    int top;
    jsc_test_stack_top = (unsigned long)&top + 64;
    ctx = JSGlobalContextCreate(NULL);
    CHECK(ctx != NULL, "contexto");
    /* ---- o que o MacDown faz: avalia, lê propriedade do global, JSON */
    JSValueRef e = NULL;
    JSValueRef r = ev("var cfg = {a: [1, 2, {b: 'ç'}], n: 0}; 6 * 7", &e);
    CHECK(r && !e && JSValueGetType(ctx, r) == kJSTypeNumber && JSValueToNumber(ctx, r, NULL) == 42, "6*7");
    JSValueRef zero = ev("0", NULL);
    CHECK(zero != NULL && JSValueToNumber(ctx, zero, NULL) == 0, "o número 0 não é NULL");
    JSStringRef k = JSStringCreateWithUTF8CString("cfg");
    JSValueRef cfg = JSObjectGetProperty(ctx, JSContextGetGlobalObject(ctx), k, NULL);
    JSStringRelease(k);
    JSStringRef js = JSValueCreateJSONString(ctx, cfg, 0, NULL);
    CHECK(js && !strcmp(cstr(js), "{\"a\":[1,2,{\"b\":\"ç\"}],\"n\":0}"), "JSON: %s", js ? cstr(js) : "");
    CHECK(JSStringGetLength(js) == 27 && JSStringGetMaximumUTF8CStringSize(js) >= 28, "comprimento UTF-16: %lu", JSStringGetLength(js));
    JSStringRelease(js);
    JSStringRef j2 = JSStringCreateWithUTF8CString("{\"x\":[true,null,1.5]}");
    JSValueRef pv = JSValueMakeFromJSONString(ctx, j2);
    JSStringRelease(j2);
    CHECK(pv && JSValueGetType(ctx, pv) == kJSTypeObject, "JSON → valor");
    /* exceções e sintaxe */
    r = ev("throw new TypeError('ops')", &e);
    CHECK(!r && e, "exceção");
    JSStringRef es = e ? JSValueToStringCopy(ctx, e, NULL) : NULL;
    CHECK(es && !strcmp(cstr(es), "TypeError: ops"), "mensagem: %s", es ? cstr(es) : "");
    if (es) JSStringRelease(es);
    e = NULL;
    r = ev("var = ;", &e);
    CHECK(!r && e && JSValueGetType(ctx, e) == kJSTypeObject, "erro de sintaxe como exceção");
    JSStringRef bad = JSStringCreateWithUTF8CString("function (");
    CHECK(!JSCheckScriptSyntax(ctx, bad, NULL, 1, NULL), "JSCheckScriptSyntax");
    JSStringRelease(bad);
    /* strings: UTF-16 e emoji */
    JSStringRef em = JSStringCreateWithUTF8CString("a😀b");
    CHECK(JSStringGetLength(em) == 4, "emoji conta 2 unidades");
    JSValueRef emv = JSValueMakeString(ctx, em);
    JSStringRelease(em);
    JSStringRef back = JSValueToStringCopy(ctx, emv, NULL);
    CHECK(!strcmp(cstr(back), "a😀b"), "ida e volta UTF-8");
    JSStringRelease(back);
    /* ---- funções com callback */
    JSStringRef nm = JSStringCreateWithUTF8CString("soma");
    JSObjectRef fs = JSObjectMakeFunctionWithCallback(ctx, nm, cb_soma);
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), nm, fs, 0, NULL);
    JSStringRelease(nm);
    EQ("soma(1, 2, 3.5)", "6.5");
    EQ("[1,2,3].map(function(x){ return soma(x, 10) }).join()", "11,12,13");
    nm = JSStringCreateWithUTF8CString("falha");
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), nm, JSObjectMakeFunctionWithCallback(ctx, nm, cb_falha), 0, NULL);
    JSStringRelease(nm);
    EQ("try { falha(); 'não' } catch (x) { 'pegou ' + x }", "pegou falhou de propósito");
    JSValueRef args[2] = {JSValueMakeNumber(ctx, 4), JSValueMakeNumber(ctx, 5)};
    JSValueRef sr = JSObjectCallAsFunction(ctx, fs, NULL, 2, args, NULL);
    CHECK(JSValueToNumber(ctx, sr, NULL) == 9, "JSObjectCallAsFunction");
    JSStringRef pn[2] = {JSStringCreateWithUTF8CString("a"), JSStringCreateWithUTF8CString("b")};
    JSStringRef body = JSStringCreateWithUTF8CString("return a * b");
    JSObjectRef mul = JSObjectMakeFunction(ctx, NULL, 2, pn, body, NULL, 1, NULL);
    CHECK(mul && JSObjectIsFunction(ctx, mul) && JSValueToNumber(ctx, JSObjectCallAsFunction(ctx, mul, NULL, 2, args, NULL), NULL) == 20, "JSObjectMakeFunction");
    JSStringRelease(pn[0]); JSStringRelease(pn[1]); JSStringRelease(body);
    /* ---- classes */
    static JSStaticValue vals[] = {{"x", pt_x, pt_setx, 0}, {"fixo", pt_x, NULL, kJSPropertyAttributeReadOnly}, {0, 0, 0, 0}};
    static JSStaticFunction fns[] = {{"inc", pt_inc, 0}, {0, 0, 0}};
    JSClassDefinition d = kJSClassDefinitionEmpty;
    d.className = "Ponto";
    d.staticValues = vals;
    d.staticFunctions = fns;
    d.getProperty = pt_get;
    d.finalize = pt_final;
    d.getPropertyNames = pt_names;
    g_pt = JSClassCreate(&d);
    nm = JSStringCreateWithUTF8CString("Ponto");
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), nm, JSObjectMakeConstructor(ctx, g_pt, pt_ctor), 0, NULL);
    JSStringRelease(nm);
    EQ("var p = new Ponto(5); p.x + ',' + p.dobro + ',' + p.inc() + ',' + p.x", "5,10,6,6");
    EQ("p.x = 20; p.fixo = 1; p.x + ',' + p.fixo + ',' + p.dobro", "20,20,40");
    EQ("Object.keys(p).sort().join()", "dobro,fixo,inc,x");
    JSValueRef pobj = ev("p", NULL);
    CHECK(JSValueIsObjectOfClass(ctx, pobj, g_pt), "JSValueIsObjectOfClass");
    ponto_t *pp = JSObjectGetPrivate((JSObjectRef)pobj);
    CHECK(pp && pp->x == 20, "privado do objeto");
    /* finalize quando o coletor libera */
    ev("p = null; for (var i = 0; i < 50; i++) new Ponto(i);", NULL);
    JSGarbageCollect(ctx);
    CHECK(g_final >= 40, "finalize chamado: %d", g_final);
    /* ---- valores protegidos sobrevivem ao coletor fora da pilha */
    JSValueRef *heap = malloc(sizeof(JSValueRef));
    *heap = ev("({guardado: 'sim', lista: new Array(1000).fill(7)})", NULL);
    JSValueProtect(ctx, *heap);
    for (int i = 0; i < 5; i++) { ev("var lixo = []; for (var j = 0; j < 20000; j++) lixo.push({j: j});", NULL); JSGarbageCollect(ctx); }
    k = JSStringCreateWithUTF8CString("guardado");
    JSValueRef gv = JSObjectGetProperty(ctx, (JSObjectRef)*heap, k, NULL);
    JSStringRelease(k);
    JSStringRef gs = JSValueToStringCopy(ctx, gv, NULL);
    CHECK(!strcmp(cstr(gs), "sim"), "valor protegido sobreviveu");
    JSStringRelease(gs);
    JSValueUnprotect(ctx, *heap);
    free(heap);
    /* ---- objetos, vetores, nomes */
    JSValueRef el[3] = {JSValueMakeNumber(ctx, 1), JSValueMakeBoolean(ctx, true), JSValueMakeUndefined(ctx)};
    JSObjectRef arr = JSObjectMakeArray(ctx, 3, el, NULL);
    CHECK(JSValueIsArray(ctx, arr) && JSValueToBoolean(ctx, JSObjectGetPropertyAtIndex(ctx, arr, 1, NULL)), "vetor");
    JSObjectRef obj = JSObjectMake(ctx, NULL, NULL);
    JSStringRef ka = JSStringCreateWithUTF8CString("a"), kb = JSStringCreateWithUTF8CString("b");
    JSObjectSetProperty(ctx, obj, ka, JSValueMakeNumber(ctx, 1), 0, NULL);
    JSObjectSetProperty(ctx, obj, kb, JSValueMakeNumber(ctx, 2), kJSPropertyAttributeDontEnum, NULL);
    JSPropertyNameArrayRef names = JSObjectCopyPropertyNames(ctx, obj);
    CHECK(JSPropertyNameArrayGetCount(names) == 1 && JSStringIsEqualToUTF8CString(JSPropertyNameArrayGetNameAtIndex(names, 0), "a"), "nomes enumeráveis");
    JSPropertyNameArrayRelease(names);
    CHECK(JSObjectDeleteProperty(ctx, obj, ka, NULL) && JSValueGetType(ctx, JSObjectGetProperty(ctx, obj, ka, NULL)) == kJSTypeUndefined, "apagar");
    JSStringRelease(ka); JSStringRelease(kb);
    /* ---- typed array sem cópia (IINA) */
    static unsigned char bytes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    int incr = 1;
    JSObjectRef ta = JSObjectMakeTypedArrayWithBytesNoCopy(ctx, kJSTypedArrayTypeUint8Array, bytes, 8, dealloc, &incr, NULL);
    CHECK(JSValueGetTypedArrayType(ctx, ta, NULL) == kJSTypedArrayTypeUint8Array && JSObjectGetTypedArrayLength(ctx, ta, NULL) == 8 &&
          JSObjectGetTypedArrayBytesPtr(ctx, ta, NULL) == bytes, "typed array NoCopy");
    nm = JSStringCreateWithUTF8CString("buf");
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), nm, ta, 0, NULL);
    JSStringRelease(nm);
    EQ("buf[3] = 200; Array.from(buf.subarray(2, 5)).join()", "3,200,5");
    CHECK(bytes[3] == 200, "o JS escreveu na memória do app");
    ev("buf = null", NULL);
    JSGarbageCollect(ctx);
    CHECK(g_dealloc == 1, "desalocador chamado: %d", g_dealloc);
    /* ---- contextos independentes no mesmo grupo */
    JSContextGroupRef grp = JSContextGroupCreate();
    JSGlobalContextRef c2 = JSGlobalContextCreateInGroup(grp, NULL);
    JSStringRef s2 = JSStringCreateWithUTF8CString("typeof soma");
    JSValueRef t2 = JSEvaluateScript(c2, s2, NULL, NULL, 1, NULL);
    JSStringRelease(s2);
    JSStringRef ts2 = JSValueToStringCopy(c2, t2, NULL);
    CHECK(!strcmp(cstr(ts2), "undefined"), "contexto novo não vê o global do outro: %s", cstr(ts2));
    JSStringRelease(ts2);
    JSGlobalContextRelease(c2);
    JSContextGroupRelease(grp);
    /* ---- this próprio no JSEvaluateScript, Promise e microtarefas */
    JSObjectRef me = (JSObjectRef)ev("({nome: 'eu'})", NULL);
    JSStringRef tsrc = JSStringCreateWithUTF8CString("this.nome");
    JSValueRef tv = JSEvaluateScript(ctx, tsrc, me, NULL, 1, NULL);
    JSStringRelease(tsrc);
    JSStringRef tvs = JSValueToStringCopy(ctx, tv, NULL);
    CHECK(!strcmp(cstr(tvs), "eu"), "thisObject: %s", cstr(tvs));
    JSStringRelease(tvs);
    ev("var feito = 0; Promise.resolve(3).then(function(v){ feito = v; });", NULL);
    EQ("feito", "3");
    JSClassRelease(g_pt);
    JSGlobalContextRelease(ctx);
    printf("jsc_host: %d/%d ok\n", ok, ok + fail);
    return fail ? 1 : 0;
}
