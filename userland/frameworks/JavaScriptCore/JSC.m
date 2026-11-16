/* mOS — JavaScriptCore.framework, API Objective-C (JSVirtualMachine, JSContext, JSValue, JSManagedValue, JSExport) sobre
 * a API C de jsc_api.c. Gerência de memória manual (como o resto dos frameworks do mOS).
 *
 * Conversões (as do JavaScriptCore): nil ↔ undefined, NSNull ↔ null, NSNumber ↔ number/boolean, NSString ↔ string,
 * NSDate ↔ Date, NSArray ↔ Array, NSDictionary ↔ objeto, bloco → função, JSValue → o próprio valor, outros objetos →
 * um objeto JS que embrulha o objeto ObjC (com os métodos e propriedades dos protocolos JSExport que a classe adota) e
 * volta ao mesmo objeto em toObject.
 * Chamadas para blocos e métodos: inteiros/ponteiros e ponto flutuante vão em sequências de registradores separadas
 * (System V no x86_64, AAPCS64 no arm64), então um protótipo fixo de 6 inteiros + 8 doubles cobre qualquer ordem de
 * argumentos; float vai com os bits no registrador XMM. Structs (CGRect...) por valor e mais de 6 inteiros não
 * cabem nesse caminho e lançam TypeError no JS. */
#import <Foundation/Foundation.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jsc_api.h"

/* o SDK do mOS não declara estas (as classes existem no Foundation do libSystem) */
@interface NSNull : NSObject
+ (NSNull *)null;
@end
@interface NSURL : NSObject
- (NSString *)absoluteString;
@end
@class Protocol;

/* ---------------------------------------------------------------- API C (jsc_api.c) */
JSGlobalContextRef JSGlobalContextCreateInGroup(JSContextGroupRef, JSClassRef);
JSGlobalContextRef JSGlobalContextRetain(JSGlobalContextRef);
void JSGlobalContextRelease(JSGlobalContextRef);
JSContextGroupRef JSContextGroupCreate(void);
JSContextGroupRef JSContextGroupRetain(JSContextGroupRef);
void JSContextGroupRelease(JSContextGroupRef);
JSContextGroupRef JSContextGetGroup(JSContextRef);
JSGlobalContextRef JSContextGetGlobalContext(JSContextRef);
JSObjectRef JSContextGetGlobalObject(JSContextRef);
JSStringRef JSGlobalContextCopyName(JSGlobalContextRef);
void JSGlobalContextSetName(JSGlobalContextRef, JSStringRef);
JSValueRef JSEvaluateScript(JSContextRef, JSStringRef, JSObjectRef, JSStringRef, int, JSValueRef *);
void JSGarbageCollect(JSContextRef);
JSStringRef JSStringCreateWithCharacters(const JSChar *, unsigned long);
JSStringRef JSStringCreateWithUTF8CString(const char *);
void JSStringRelease(JSStringRef);
unsigned long JSStringGetLength(JSStringRef);
const JSChar *JSStringGetCharactersPtr(JSStringRef);
JSType JSValueGetType(JSContextRef, JSValueRef);
bool JSValueIsArray(JSContextRef, JSValueRef);
bool JSValueIsDate(JSContextRef, JSValueRef);
bool JSValueIsObjectOfClass(JSContextRef, JSValueRef, JSClassRef);
bool JSValueIsEqual(JSContextRef, JSValueRef, JSValueRef, JSValueRef *);
bool JSValueIsStrictEqual(JSContextRef, JSValueRef, JSValueRef);
bool JSValueIsInstanceOfConstructor(JSContextRef, JSValueRef, JSObjectRef, JSValueRef *);
JSValueRef JSValueMakeUndefined(JSContextRef);
JSValueRef JSValueMakeNull(JSContextRef);
JSValueRef JSValueMakeBoolean(JSContextRef, bool);
JSValueRef JSValueMakeNumber(JSContextRef, double);
JSValueRef JSValueMakeString(JSContextRef, JSStringRef);
JSValueRef JSValueMakeSymbol(JSContextRef, JSStringRef);
bool JSValueToBoolean(JSContextRef, JSValueRef);
double JSValueToNumber(JSContextRef, JSValueRef, JSValueRef *);
JSStringRef JSValueToStringCopy(JSContextRef, JSValueRef, JSValueRef *);
JSObjectRef JSValueToObject(JSContextRef, JSValueRef, JSValueRef *);
void JSValueProtect(JSContextRef, JSValueRef);
void JSValueUnprotect(JSContextRef, JSValueRef);
JSObjectRef JSObjectMake(JSContextRef, JSClassRef, void *);
void *JSObjectGetPrivate(JSObjectRef);
JSObjectRef JSObjectMakeArray(JSContextRef, unsigned long, const JSValueRef[], JSValueRef *);
JSObjectRef JSObjectMakeDate(JSContextRef, unsigned long, const JSValueRef[], JSValueRef *);
JSObjectRef JSObjectMakeError(JSContextRef, unsigned long, const JSValueRef[], JSValueRef *);
JSObjectRef JSObjectMakeRegExp(JSContextRef, unsigned long, const JSValueRef[], JSValueRef *);
JSValueRef JSObjectGetProperty(JSContextRef, JSObjectRef, JSStringRef, JSValueRef *);
void JSObjectSetProperty(JSContextRef, JSObjectRef, JSStringRef, JSValueRef, JSPropertyAttributes, JSValueRef *);
bool JSObjectHasProperty(JSContextRef, JSObjectRef, JSStringRef);
bool JSObjectDeleteProperty(JSContextRef, JSObjectRef, JSStringRef, JSValueRef *);
JSValueRef JSObjectGetPropertyAtIndex(JSContextRef, JSObjectRef, unsigned, JSValueRef *);
void JSObjectSetPropertyAtIndex(JSContextRef, JSObjectRef, unsigned, JSValueRef, JSValueRef *);
JSValueRef JSObjectGetPropertyForKey(JSContextRef, JSObjectRef, JSValueRef, JSValueRef *);
void JSObjectSetPropertyForKey(JSContextRef, JSObjectRef, JSValueRef, JSValueRef, JSPropertyAttributes, JSValueRef *);
bool JSObjectIsFunction(JSContextRef, JSObjectRef);
JSValueRef JSObjectCallAsFunction(JSContextRef, JSObjectRef, JSObjectRef, unsigned long, const JSValueRef[], JSValueRef *);
JSObjectRef JSObjectCallAsConstructor(JSContextRef, JSObjectRef, unsigned long, const JSValueRef[], JSValueRef *);
JSPropertyNameArrayRef JSObjectCopyPropertyNames(JSContextRef, JSObjectRef);
unsigned long JSPropertyNameArrayGetCount(JSPropertyNameArrayRef);
JSStringRef JSPropertyNameArrayGetNameAtIndex(JSPropertyNameArrayRef, unsigned long);
void JSPropertyNameArrayRelease(JSPropertyNameArrayRef);
void JSPropertyNameAccumulatorAddName(JSPropertyNameAccumulatorRef, JSStringRef);
JSClassRef JSClassCreate(const JSClassDefinition *);
extern const JSClassDefinition kJSClassDefinitionEmpty;
void *jsc_context_objc(JSContextRef);
void jsc_context_set_objc(JSContextRef, void *);

/* ---------------------------------------------------------------- runtime ObjC e blocos */
typedef struct objc_method_description_ { SEL name; const char *types; } jsc_mdesc;
void *object_getClass(id o);
const char *class_getName(void *c);
void *class_getSuperclass(void *c);
Protocol **class_copyProtocolList(void *c, unsigned *n);
Protocol **protocol_copyProtocolList(Protocol *p, unsigned *n);
BOOL protocol_conformsToProtocol(Protocol *a, Protocol *b);
jsc_mdesc *protocol_copyMethodDescriptionList(Protocol *p, BOOL required, BOOL instance, unsigned *n);
Protocol *objc_getProtocol(const char *name);
const char *sel_getName(SEL s);
SEL sel_registerName(const char *n);
id objc_msgSend(id, SEL, ...);
void *_Block_copy(const void *b);
void _Block_release(const void *b);

/* layout público dos blocos (ABI do clang) e das listas de propriedades dos protocolos (ABI do runtime da Apple) */
struct jsc_block_desc { unsigned long reserved, size; void *rest[]; };
struct jsc_block { void *isa; int flags, reserved; void *invoke; struct jsc_block_desc *desc; };
#define JSC_BLOCK_HAS_COPY_DISPOSE (1 << 25)
#define JSC_BLOCK_HAS_SIGNATURE (1 << 30)
typedef struct { void *isa; const char *name; void *protocols; const void *inst, *cls, *opt_inst, *opt_cls; const struct { unsigned entsize, count; struct { const char *name, *attrs; } list[]; } *props; } jsc_proto_layout;

static const char *block_signature(id blk) {
    struct jsc_block *b = (struct jsc_block *)blk;
    if (!b || !(b->flags & JSC_BLOCK_HAS_SIGNATURE)) return NULL;
    int i = (b->flags & JSC_BLOCK_HAS_COPY_DISPOSE) ? 2 : 0;
    return (const char *)b->desc->rest[i];
}
static BOOL is_block(id o) {
    if (!o) return NO;
    for (Class c = (Class)object_getClass(o); c; c = (Class)class_getSuperclass(c)) {
        const char *n = class_getName(c);
        if (n && (!strcmp(n, "NSBlock") || !strncmp(n, "__NS", 4) && strstr(n, "Block"))) return YES;
    }
    return NO;
}

@class JSContext, JSValue;
@protocol JSExport
@end

/* ---------------------------------------------------------------- tipos de argumento */
/* avança um tipo de uma assinatura ObjC ("v24@0:8@16"), devolve o começo do próximo */
static const char *skip_type(const char *t, char *code, char *clsname, int cap) {
    while (*t == 'r' || *t == 'n' || *t == 'N' || *t == 'o' || *t == 'O' || *t == 'R' || *t == 'V' || *t == 'A') t++;
    *code = *t;
    if (clsname) clsname[0] = 0;
    if (*t == '@') {
        t++;
        if (*t == '?') t++;                       /* bloco */
        else if (*t == '"') {
            const char *e = t + 1;
            while (*e && *e != '"') e++;
            if (clsname) { int n = (int)(e - t - 1); if (n >= cap) n = cap - 1; memcpy(clsname, t + 1, (unsigned long)n); clsname[n] = 0; }
            t = *e ? e + 1 : e;
        }
    } else if (*t == '{' || *t == '(' || *t == '[') {
        char open = *t, close = open == '{' ? '}' : open == '(' ? ')' : ']';
        int dep = 0;
        do { if (*t == open) dep++; else if (*t == close) dep--; t++; } while (*t && dep);
    } else if (*t == '^') { t++; char c2; t = skip_type(t, &c2, NULL, 0); }
    else if (*t) t++;
    while (*t >= '0' && *t <= '9') t++;           /* deslocamento */
    return t;
}

/* ---------------------------------------------------------------- estado de chamada (currentContext etc.) */
typedef struct jsc_frame { JSContext *ctx; JSValue *callee, *thisv; NSArray *args; struct jsc_frame *prev; } jsc_frame;
static __thread jsc_frame *g_frame;

@interface JSVirtualMachine : NSObject { @public JSContextGroupRef _group; NSMutableArray *_managed; }
@end
@interface JSContext : NSObject {
    @public
    JSGlobalContextRef _ctx;
    JSVirtualMachine *_vm;
    JSValue *_exception;
    id _handler;
}
+ (JSContext *)contextWithJSGlobalContextRef:(JSGlobalContextRef)c;
- (JSValue *)_wrap:(JSValueRef)v;
- (void)_raise:(JSValueRef)exc;
@end
@interface JSValue : NSObject { @public JSValueRef _v; JSContext *_c; }
+ (JSValue *)valueWithJSValueRef:(JSValueRef)v inContext:(JSContext *)c;
- (id)toObject;
@end
@interface JSManagedValue : NSObject { JSValue *_value; }
@end

static JSValueRef to_js(JSContext *c, id o, int depth);
static id to_objc(JSContextRef ctx, JSValueRef v, int depth);

static JSStringRef jsstr(NSString *s) {
    unsigned long n = [s length];
    JSChar *u = malloc(sizeof(JSChar) * (n ? n : 1));
    [s getCharacters:u range:NSMakeRange(0, n)];
    JSStringRef r = JSStringCreateWithCharacters(u, n);
    free(u);
    return r;
}
static NSString *nsstr(JSStringRef s) {
    if (!s) return nil;
    return [NSString stringWithCharacters:JSStringGetCharactersPtr(s) length:JSStringGetLength(s)];
}

/* ---------------------------------------------------------------- objetos ObjC no JS (embrulho) */
typedef struct { id obj; } wrapdata_t;
typedef struct { SEL sel; char *types; id block; BOOL is_block; id target; } calldata_t;

static JSClassRef g_wrap_class, g_call_class;

/* nome JS de um seletor: "fazer:com:" → "fazerCom" */
static NSString *js_name(const char *sel) {
    NSMutableString *m = [NSMutableString string];
    BOOL up = NO;
    for (const char *p = sel; *p; p++) {
        if (*p == ':') { up = YES; continue; }
        if (up && *p >= 'a' && *p <= 'z') [m appendFormat:@"%c", *p - 32];
        else [m appendFormat:@"%c", *p];
        up = NO;
    }
    return m;
}

static BOOL is_export_proto(Protocol *p, int depth) {
    if (!p || depth > 16) return NO;
    Protocol *jx = objc_getProtocol("JSExport");
    if (!jx) return NO;
    if (p == jx) return NO;
    unsigned n = 0;
    Protocol **l = protocol_copyProtocolList(p, &n);
    BOOL r = NO;
    for (unsigned i = 0; i < n && !r; i++) r = l[i] == jx || is_export_proto(l[i], depth + 1);
    free(l);
    return r;
}

typedef struct { const char *name; SEL get, set; char *types; BOOL is_prop; } member_t;

/* procura o membro exportado com esse nome JS nos protocolos JSExport da classe (e das superclasses) */
static BOOL find_member(id obj, NSString *jsname, member_t *out) {
    const char *want = [jsname UTF8String];
    for (Class c = (Class)object_getClass(obj); c; c = (Class)class_getSuperclass(c)) {
        unsigned np = 0;
        Protocol **pl = class_copyProtocolList(c, &np);
        for (unsigned i = 0; i < np; i++) {
            if (!is_export_proto(pl[i], 0)) continue;
            /* propriedades declaradas */
            const jsc_proto_layout *pp = (const jsc_proto_layout *)pl[i];
            if (pp->props)
                for (unsigned k = 0; k < pp->props->count; k++)
                    if (!strcmp(pp->props->list[k].name, want)) {
                        out->is_prop = YES;
                        out->name = want;
                        out->get = sel_registerName(want);
                        const char *a = pp->props->list[k].attrs, *g = strstr(a, ",G"), *s = strstr(a, ",S");
                        if (g) { char b[128]; int n = 0; g += 2; while (*g && *g != ',' && n < 127) b[n++] = *g++; b[n] = 0; out->get = sel_registerName(b); }
                        BOOL ro = strstr(a, ",R") != NULL;
                        if (s) { char b[128]; int n = 0; s += 2; while (*s && *s != ',' && n < 127) b[n++] = *s++; b[n] = 0; out->set = sel_registerName(b); }
                        else if (!ro) { char b[140]; snprintf(b, sizeof b, "set%c%s:", want[0] >= 'a' && want[0] <= 'z' ? want[0] - 32 : want[0], want + 1); out->set = sel_registerName(b); }
                        /* "T<tipo>,..." → assinaturas do getter e do setter, para o mesmo caminho de chamada */
                        char ty[96] = "@";
                        if (a[0] == 'T') { int n = 0; const char *q = a + 1; while (*q && *q != ',' && n < 95) ty[n++] = *q++; ty[n] = 0; }
                        char g2[128], s2[128];
                        snprintf(g2, sizeof g2, "%s16@0:8", ty);
                        snprintf(s2, sizeof s2, "v24@0:8%s16", ty);
                        out->types = malloc(strlen(g2) + strlen(s2) + 2);
                        memcpy(out->types, g2, strlen(g2) + 1);
                        memcpy(out->types + strlen(g2) + 1, s2, strlen(s2) + 1);
                        free(pl);
                        return YES;
                    }
            /* métodos (obrigatórios e opcionais), com JSExportAs */
            for (int req = 1; req >= 0; req--) {
                unsigned nm = 0;
                jsc_mdesc *ml = protocol_copyMethodDescriptionList(pl[i], (BOOL)req, YES, &nm);
                for (unsigned k = 0; k < nm; k++) {
                    const char *sn = sel_getName(ml[k].name);
                    const char *as = strstr(sn, "__JS_EXPORT_AS__");
                    NSString *nm2;
                    SEL real = ml[k].name;
                    if (as) {
                        char rb[256];
                        int n = (int)(as - sn);
                        if (n >= 255) continue;
                        memcpy(rb, sn, (unsigned long)n); rb[n] = 0;
                        real = sel_registerName(rb);
                        NSString *rn = [NSString stringWithUTF8String:as + 16];
                        nm2 = [rn hasSuffix:@":"] ? [rn substringToIndex:[rn length] - 1] : rn;
                    } else nm2 = js_name(sn);
                    if ([nm2 isEqualToString:jsname]) {
                        out->is_prop = NO;
                        out->get = real;
                        out->set = NULL;
                        /* tipos do método real na lista (o do JSExportAs é o do apelido) */
                        const char *ty = ml[k].types;
                        for (unsigned q = 0; as && q < nm; q++) if (ml[q].name == real) ty = ml[q].types;
                        unsigned long tl = strlen(ty);
                        out->types = malloc(tl + 1);
                        memcpy(out->types, ty, tl + 1);
                        free(ml);
                        free(pl);
                        return YES;
                    }
                }
                free(ml);
            }
        }
        free(pl);
    }
    return NO;
}

/* chamada genérica: 6 inteiros + 8 FP (bits) por registrador; ret_kind 0 inteiro, 1 double, 2 float */
typedef unsigned long long u64_t;
typedef u64_t (*call_i)(u64_t, u64_t, u64_t, u64_t, u64_t, u64_t, double, double, double, double, double, double, double, double);
typedef double (*call_d)(u64_t, u64_t, u64_t, u64_t, u64_t, u64_t, double, double, double, double, double, double, double, double);
typedef float (*call_f)(u64_t, u64_t, u64_t, u64_t, u64_t, u64_t, double, double, double, double, double, double, double, double);

/* converte os argumentos JS conforme a assinatura e chama; ok=NO com exceção já preparada */
static JSValueRef do_call(JSContextRef ctx, void *fn, u64_t first, u64_t second, int nfixed, const char *types, unsigned long argc,
                          const JSValueRef argv[], JSValueRef *exc) {
    JSContext *jc = [JSContext contextWithJSGlobalContextRef:JSContextGetGlobalContext(ctx)];
    u64_t I[6] = {first, second, 0, 0, 0, 0};
    double F[8] = {0};
    int ni = nfixed, nf = 0, ai = 0;
    char rc, cls[64];
    const char *t = skip_type(types, &rc, NULL, 0);      /* retorno */
    for (int k = 0; k < nfixed; k++) { char x; t = skip_type(t, &x, NULL, 0); }   /* self/_cmd ou o bloco */
    NSMutableArray *keep = [NSMutableArray array];
    while (*t) {
        char c;
        t = skip_type(t, &c, cls, sizeof cls);
        if (!c) break;
        JSValueRef a = (unsigned long)ai < argc ? argv[ai] : JSValueMakeUndefined(ctx);
        ai++;
        u64_t iv = 0;
        BOOL isfp = NO;
        double dv = 0;
        switch (c) {
        case '@': {
            id o;
            if (!strcmp(cls, "JSValue")) o = [JSValue valueWithJSValueRef:a inContext:jc];
            else o = to_objc(ctx, a, 0);
            if (o) [keep addObject:o];
            iv = (u64_t)(unsigned long)o;
            break;
        }
        case 'B': iv = JSValueToBoolean(ctx, a); break;
        case 'c': case 'C': case 's': case 'S': case 'i': case 'I': case 'l': case 'L': case 'q': case 'Q': {
            double d = JSValueToNumber(ctx, a, exc);
            iv = (c == 'Q' || c == 'L' || c == 'I' || c == 'S' || c == 'C') ? (u64_t)(d < 0 ? 0 : d) : (u64_t)(long long)d;
            break;
        }
        case 'd': isfp = YES; dv = JSValueToNumber(ctx, a, exc); break;
        case 'f': { isfp = YES; float fl = (float)JSValueToNumber(ctx, a, exc); u64_t bits = 0; memcpy(&bits, &fl, 4); memcpy(&dv, &bits, 8); break; }
        case '*': { NSString *s = to_objc(ctx, a, 0); if (![s isKindOfClass:[NSString class]]) s = [NSString stringWithFormat:@"%@", s ?: @""]; [keep addObject:s]; iv = (u64_t)(unsigned long)[s UTF8String]; break; }
        case ':': { NSString *s = to_objc(ctx, a, 0); iv = (u64_t)(unsigned long)([s isKindOfClass:[NSString class]] ? sel_registerName([s UTF8String]) : NULL); break; }
        case '#': { NSString *s = to_objc(ctx, a, 0); iv = (u64_t)(unsigned long)([s isKindOfClass:[NSString class]] ? NSClassFromString(s) : nil); break; }
        case '^': iv = 0; break;
        default: {
            JSStringRef m = JSStringCreateWithUTF8CString("JavaScriptCore do mOS: tipo de argumento sem conversão (struct por valor)");
            if (exc) { JSValueRef msg = JSValueMakeString(ctx, m); *exc = JSObjectMakeError(ctx, 1, &msg, NULL); }
            JSStringRelease(m);
            return NULL;
        }
        }
        if (isfp) { if (nf >= 8) goto toomany; F[nf++] = dv; }
        else { if (ni >= 6) goto toomany; I[ni++] = iv; }
    }
    if (rc == 'd') {
        double r = ((call_d)fn)(I[0], I[1], I[2], I[3], I[4], I[5], F[0], F[1], F[2], F[3], F[4], F[5], F[6], F[7]);
        return JSValueMakeNumber(ctx, r);
    }
    if (rc == 'f') {
        float r = ((call_f)fn)(I[0], I[1], I[2], I[3], I[4], I[5], F[0], F[1], F[2], F[3], F[4], F[5], F[6], F[7]);
        return JSValueMakeNumber(ctx, r);
    }
    if (rc == '{') {
        JSStringRef m = JSStringCreateWithUTF8CString("JavaScriptCore do mOS: retorno struct por valor não suportado");
        if (exc) { JSValueRef msg = JSValueMakeString(ctx, m); *exc = JSObjectMakeError(ctx, 1, &msg, NULL); }
        JSStringRelease(m);
        return NULL;
    }
    u64_t r = ((call_i)fn)(I[0], I[1], I[2], I[3], I[4], I[5], F[0], F[1], F[2], F[3], F[4], F[5], F[6], F[7]);
    switch (rc) {
    case 'v': return JSValueMakeUndefined(ctx);
    case '@': return to_js(jc, (id)(unsigned long)r, 0);
    case 'B': return JSValueMakeBoolean(ctx, (r & 0xFF) != 0);
    case 'c': return JSValueMakeNumber(ctx, (signed char)r);
    case 'C': return JSValueMakeNumber(ctx, (unsigned char)r);
    case 's': return JSValueMakeNumber(ctx, (short)r);
    case 'S': return JSValueMakeNumber(ctx, (unsigned short)r);
    case 'i': return JSValueMakeNumber(ctx, (int)r);
    case 'I': return JSValueMakeNumber(ctx, (unsigned)r);
    case 'q': case 'l': return JSValueMakeNumber(ctx, (double)(long long)r);
    case 'Q': case 'L': return JSValueMakeNumber(ctx, (double)r);
    case '*': return r ? to_js(jc, [NSString stringWithUTF8String:(const char *)(unsigned long)r], 0) : JSValueMakeNull(ctx);
    default: return JSValueMakeUndefined(ctx);
    }
toomany: {
        JSStringRef m = JSStringCreateWithUTF8CString("JavaScriptCore do mOS: argumentos demais para a chamada nativa");
        if (exc) { JSValueRef msg = JSValueMakeString(ctx, m); *exc = JSObjectMakeError(ctx, 1, &msg, NULL); }
        JSStringRelease(m);
        return NULL;
    }
}

/* função JS que chama um bloco ou um método ObjC */
static JSValueRef call_cb(JSContextRef ctx, JSObjectRef fn, JSObjectRef thisObj, unsigned long argc, const JSValueRef argv[], JSValueRef *exc) {
    calldata_t *cd = JSObjectGetPrivate(fn);
    if (!cd) return JSValueMakeUndefined(ctx);
    JSContext *jc = [JSContext contextWithJSGlobalContextRef:JSContextGetGlobalContext(ctx)];
    NSMutableArray *args = [NSMutableArray array];
    for (unsigned long i = 0; i < argc; i++) [args addObject:[JSValue valueWithJSValueRef:argv[i] inContext:jc]];
    jsc_frame fr = {jc, [JSValue valueWithJSValueRef:fn inContext:jc], thisObj ? [JSValue valueWithJSValueRef:thisObj inContext:jc] : nil, args, g_frame};
    g_frame = &fr;
    JSValueRef r;
    if (cd->is_block) {
        const char *sig = block_signature(cd->block);
        if (!sig) {
            /* sem assinatura (blocos muito antigos): chama com os objetos */
            sig = "@?0@?8";
        }
        r = do_call(ctx, ((struct jsc_block *)cd->block)->invoke, (u64_t)(unsigned long)cd->block, 0, 1, sig, argc, argv, exc);
    } else {
        id target = cd->target;
        if (!target && thisObj) {
            void *p = JSObjectGetPrivate(thisObj);
            if (p && JSValueIsObjectOfClass(ctx, thisObj, g_wrap_class)) target = ((wrapdata_t *)p)->obj;
        }
        if (!target) {
            JSStringRef m = JSStringCreateWithUTF8CString("o método ObjC foi chamado sem o objeto");
            JSValueRef msg = JSValueMakeString(ctx, m);
            if (exc) *exc = JSObjectMakeError(ctx, 1, &msg, NULL);
            JSStringRelease(m);
            r = NULL;
        } else r = do_call(ctx, (void *)objc_msgSend, (u64_t)(unsigned long)target, (u64_t)(unsigned long)cd->sel, 2, cd->types, argc, argv, exc);
    }
    g_frame = fr.prev;
    return r;
}
static void call_finalize(JSObjectRef o) {
    calldata_t *cd = JSObjectGetPrivate(o);
    if (!cd) return;
    if (cd->block) _Block_release(cd->block);
    free(cd->types);
    free(cd);
}

static JSValueRef wrap_get(JSContextRef ctx, JSObjectRef o, JSStringRef name, JSValueRef *exc) {
    wrapdata_t *w = JSObjectGetPrivate(o);
    if (!w || !w->obj) return NULL;
    member_t m = {0};
    if (!find_member(w->obj, nsstr(name), &m)) return NULL;
    if (m.is_prop) {
        JSValueRef r = do_call(ctx, (void *)objc_msgSend, (u64_t)(unsigned long)w->obj, (u64_t)(unsigned long)m.get, 2, m.types, 0, NULL, exc);
        free(m.types);
        return r;
    }
    calldata_t *cd = calloc(1, sizeof *cd);
    cd->sel = m.get;
    cd->types = m.types;
    cd->target = nil;
    JSObjectRef f = JSObjectMake(ctx, g_call_class, cd);
    /* fica guardada no objeto: a mesma função a cada leitura */
    JSObjectSetProperty(ctx, o, name, f, kJSPropertyAttributeDontEnum, exc);
    return f;
}
static bool wrap_set(JSContextRef ctx, JSObjectRef o, JSStringRef name, JSValueRef v, JSValueRef *exc) {
    (void)exc;
    wrapdata_t *w = JSObjectGetPrivate(o);
    if (!w || !w->obj) return false;
    member_t m = {0};
    if (!find_member(w->obj, nsstr(name), &m) || !m.is_prop) return false;
    if (m.set) do_call(ctx, (void *)objc_msgSend, (u64_t)(unsigned long)w->obj, (u64_t)(unsigned long)m.set, 2,
                       m.types + strlen(m.types) + 1, 1, &v, exc);   /* sem setter (readonly): ignora, como o JSC */
    free(m.types);
    return true;
}
static void wrap_finalize(JSObjectRef o) {
    wrapdata_t *w = JSObjectGetPrivate(o);
    if (!w) return;
    [w->obj release];
    free(w);
}

static void init_classes(void) {
    if (g_wrap_class) return;
    JSClassDefinition d = kJSClassDefinitionEmpty;
    d.className = "ObjCObject";
    d.getProperty = wrap_get;
    d.setProperty = wrap_set;
    d.finalize = wrap_finalize;
    g_wrap_class = JSClassCreate(&d);
    JSClassDefinition f = kJSClassDefinitionEmpty;
    f.className = "ObjCFunction";
    f.callAsFunction = call_cb;
    f.finalize = call_finalize;
    g_call_class = JSClassCreate(&f);
}

/* ---------------------------------------------------------------- conversões */
@interface NSNumber (JSCTipo)
- (const char *)objCType;
@end
/* @YES/@NO (numberWithBool:) viram booleanos; o resto, números */
static BOOL is_bool_number(id n) {
    if (n == (id)[NSNumber numberWithBool:YES] || n == (id)[NSNumber numberWithBool:NO]) return YES;
    const char *t = [(NSNumber *)n objCType];
    return t && t[0] == 'B';
}

static JSValueRef to_js(JSContext *c, id o, int depth) {
    JSContextRef ctx = c->_ctx;
    init_classes();
    if (!o) return JSValueMakeUndefined(ctx);
    if (depth > 64) return JSValueMakeNull(ctx);
    if ([o isKindOfClass:[JSValue class]]) return ((JSValue *)o)->_v;
    if (o == [NSNull null]) return JSValueMakeNull(ctx);
    if ([o isKindOfClass:[NSString class]]) { JSStringRef s = jsstr(o); JSValueRef r = JSValueMakeString(ctx, s); JSStringRelease(s); return r; }
    if ([o isKindOfClass:[NSNumber class]]) {
        if (is_bool_number(o)) return JSValueMakeBoolean(ctx, [o boolValue]);
        return JSValueMakeNumber(ctx, [o doubleValue]);
    }
    if ([o isKindOfClass:[NSDate class]]) {
        JSValueRef ms = JSValueMakeNumber(ctx, [(NSDate *)o timeIntervalSince1970] * 1000.0);
        return JSObjectMakeDate(ctx, 1, &ms, NULL);
    }
    if ([o isKindOfClass:[NSArray class]]) {
        NSArray *a = o;
        JSObjectRef arr = JSObjectMakeArray(ctx, 0, NULL, NULL);
        for (NSUInteger i = 0; i < [a count]; i++) JSObjectSetPropertyAtIndex(ctx, arr, (unsigned)i, to_js(c, [a objectAtIndex:i], depth + 1), NULL);
        return arr;
    }
    if ([o isKindOfClass:[NSDictionary class]]) {
        NSDictionary *d = o;
        JSObjectRef obj = JSObjectMake(ctx, NULL, NULL);
        for (id k in d) {
            JSStringRef ks = jsstr([k isKindOfClass:[NSString class]] ? k : [k description]);
            JSObjectSetProperty(ctx, obj, ks, to_js(c, [d objectForKey:k], depth + 1), 0, NULL);
            JSStringRelease(ks);
        }
        return obj;
    }
    if (is_block(o)) {
        calldata_t *cd = calloc(1, sizeof *cd);
        cd->is_block = YES;
        cd->block = _Block_copy(o);
        return JSObjectMake(ctx, g_call_class, cd);
    }
    wrapdata_t *w = calloc(1, sizeof *w);
    w->obj = [o retain];
    return JSObjectMake(ctx, g_wrap_class, w);
}

static id to_objc(JSContextRef ctx, JSValueRef v, int depth) {
    init_classes();
    switch (JSValueGetType(ctx, v)) {
    case kJSTypeUndefined: return nil;
    case kJSTypeNull: return [NSNull null];
    case kJSTypeBoolean: return [NSNumber numberWithBool:JSValueToBoolean(ctx, v)];
    case kJSTypeNumber: return [NSNumber numberWithDouble:JSValueToNumber(ctx, v, NULL)];
    case kJSTypeString: case kJSTypeSymbol: case kJSTypeBigInt: {
        JSStringRef s = JSValueToStringCopy(ctx, v, NULL);
        NSString *r = nsstr(s);
        if (s) JSStringRelease(s);
        return r;
    }
    default: break;
    }
    JSObjectRef o = (JSObjectRef)v;
    if (JSValueIsObjectOfClass(ctx, v, g_wrap_class)) { wrapdata_t *w = JSObjectGetPrivate(o); return w ? w->obj : nil; }
    if (JSValueIsObjectOfClass(ctx, v, g_call_class)) { calldata_t *cd = JSObjectGetPrivate(o); if (cd && cd->is_block) return cd->block; }
    if (depth > 64) return [NSNull null];
    if (JSValueIsDate(ctx, v)) return [NSDate dateWithTimeIntervalSince1970:JSValueToNumber(ctx, v, NULL) / 1000.0];
    if (JSValueIsArray(ctx, v)) {
        JSStringRef ls = JSStringCreateWithUTF8CString("length");
        unsigned n = (unsigned)JSValueToNumber(ctx, JSObjectGetProperty(ctx, o, ls, NULL), NULL);
        JSStringRelease(ls);
        NSMutableArray *a = [NSMutableArray arrayWithCapacity:n];
        for (unsigned i = 0; i < n; i++) { id e = to_objc(ctx, JSObjectGetPropertyAtIndex(ctx, o, i, NULL), depth + 1); [a addObject:e ?: [NSNull null]]; }
        return a;
    }
    NSMutableDictionary *d = [NSMutableDictionary dictionary];
    JSPropertyNameArrayRef names = JSObjectCopyPropertyNames(ctx, o);
    for (unsigned long i = 0; i < JSPropertyNameArrayGetCount(names); i++) {
        JSStringRef k = JSPropertyNameArrayGetNameAtIndex(names, i);
        id e = to_objc(ctx, JSObjectGetProperty(ctx, o, k, NULL), depth + 1);
        if (e) [d setObject:e forKey:nsstr(k)];
    }
    JSPropertyNameArrayRelease(names);
    return d;
}

/* ================================================================ JSVirtualMachine */
@implementation JSVirtualMachine
- (instancetype)init {
    if ((self = [super init])) { _group = JSContextGroupCreate(); _managed = [[NSMutableArray alloc] init]; }
    return self;
}
- (void)dealloc { [_managed release]; JSContextGroupRelease(_group); [super dealloc]; }
/* referências gerenciadas: o valor fica vivo enquanto o dono o mantém registrado */
- (void)addManagedReference:(id)object withOwner:(id)owner {
    if (object) [_managed addObject:[NSArray arrayWithObjects:object, owner ?: [NSNull null], nil]];
}
- (void)removeManagedReference:(id)object withOwner:(id)owner {
    for (NSUInteger i = 0; i < [_managed count]; i++) {
        NSArray *p = [_managed objectAtIndex:i];
        if ([p objectAtIndex:0] == object && [p objectAtIndex:1] == (owner ?: [NSNull null])) { [_managed removeObjectAtIndex:i]; return; }
    }
}
- (void)performOpportunisticGarbageCollection {}
+ (void)setShouldPerformOpportunisticGarbageCollection:(BOOL)b { (void)b; }
@end

/* ================================================================ JSContext */
@implementation JSContext
- (instancetype)init { return [self initWithVirtualMachine:[[[JSVirtualMachine alloc] init] autorelease]]; }
- (instancetype)initWithVirtualMachine:(JSVirtualMachine *)vm {
    if ((self = [super init])) {
        _vm = [vm retain];
        _ctx = JSGlobalContextCreateInGroup(vm->_group, NULL);
        jsc_context_set_objc(_ctx, self);
        init_classes();
    }
    return self;
}
- (instancetype)_initWithRef:(JSGlobalContextRef)c {
    if ((self = [super init])) {
        _ctx = JSGlobalContextRetain(c);
        _vm = [[JSVirtualMachine alloc] init];
        jsc_context_set_objc(_ctx, self);
    }
    return self;
}
+ (JSContext *)contextWithJSGlobalContextRef:(JSGlobalContextRef)c {
    if (!c) return nil;
    JSContext *o = jsc_context_objc(c);
    if (o) return o;
    return [[[JSContext alloc] _initWithRef:c] autorelease];
}
- (void)dealloc {
    jsc_context_set_objc(_ctx, NULL);
    [_exception release];
    [_handler release];
    JSGlobalContextRelease(_ctx);
    [_vm release];
    [super dealloc];
}
- (JSGlobalContextRef)JSGlobalContextRef { return _ctx; }
- (JSVirtualMachine *)virtualMachine { return _vm; }
- (JSValue *)_wrap:(JSValueRef)v { return v ? [JSValue valueWithJSValueRef:v inContext:self] : nil; }
- (JSValue *)globalObject { return [self _wrap:JSContextGetGlobalObject(_ctx)]; }
- (JSValue *)exception { return _exception; }
- (void)setException:(JSValue *)e { [e retain]; [_exception release]; _exception = e; }
- (void (^)(JSContext *, JSValue *))exceptionHandler { return _handler; }
- (void)setExceptionHandler:(void (^)(JSContext *, JSValue *))h { id c = [h copy]; [_handler release]; _handler = c; }
- (void)_raise:(JSValueRef)exc {
    JSValue *e = [self _wrap:exc];
    if (_handler) ((void (^)(JSContext *, JSValue *))_handler)(self, e);
    else [self setException:e];
}
- (NSString *)name { JSStringRef s = JSGlobalContextCopyName(_ctx); NSString *r = nsstr(s); if (s) JSStringRelease(s); return r; }
- (void)setName:(NSString *)n { JSStringRef s = n ? jsstr(n) : NULL; JSGlobalContextSetName(_ctx, s); if (s) JSStringRelease(s); }
- (JSValue *)evaluateScript:(NSString *)script { return [self evaluateScript:script withSourceURL:nil]; }
- (JSValue *)evaluateScript:(NSString *)script withSourceURL:(NSURL *)url {
    JSStringRef s = jsstr(script ?: @"");
    JSStringRef u = url ? jsstr([url absoluteString]) : NULL;
    JSValueRef exc = NULL;
    JSValueRef r = JSEvaluateScript(_ctx, s, NULL, u, 1, &exc);
    JSStringRelease(s);
    if (u) JSStringRelease(u);
    if (exc) { [self _raise:exc]; return [self _wrap:JSValueMakeUndefined(_ctx)]; }
    return [self _wrap:r];
}
- (JSValue *)objectForKeyedSubscript:(id)key { return [[self globalObject] objectForKeyedSubscript:key]; }
- (void)setObject:(id)obj forKeyedSubscript:(id)key { [[self globalObject] setObject:obj forKeyedSubscript:key]; }
+ (JSContext *)currentContext { return g_frame ? g_frame->ctx : nil; }
+ (JSValue *)currentCallee { return g_frame ? g_frame->callee : nil; }
+ (JSValue *)currentThis { return g_frame ? g_frame->thisv : nil; }
+ (NSArray *)currentArguments { return g_frame ? g_frame->args : nil; }
@end

/* ================================================================ JSValue */
@implementation JSValue
+ (JSValue *)valueWithJSValueRef:(JSValueRef)v inContext:(JSContext *)c {
    JSValue *o = [[[JSValue alloc] init] autorelease];
    o->_v = v ? v : JSValueMakeUndefined(c->_ctx);
    o->_c = [c retain];
    JSValueProtect(c->_ctx, o->_v);
    return o;
}
- (void)dealloc { JSValueUnprotect(_c->_ctx, _v); [_c release]; [super dealloc]; }
- (JSValueRef)JSValueRef { return _v; }
- (JSContext *)context { return _c; }
+ (JSValue *)valueWithObject:(id)o inContext:(JSContext *)c { return [self valueWithJSValueRef:to_js(c, o, 0) inContext:c]; }
+ (JSValue *)valueWithBool:(BOOL)b inContext:(JSContext *)c { return [self valueWithJSValueRef:JSValueMakeBoolean(c->_ctx, b) inContext:c]; }
+ (JSValue *)valueWithDouble:(double)d inContext:(JSContext *)c { return [self valueWithJSValueRef:JSValueMakeNumber(c->_ctx, d) inContext:c]; }
+ (JSValue *)valueWithInt32:(int)i inContext:(JSContext *)c { return [self valueWithDouble:i inContext:c]; }
+ (JSValue *)valueWithUInt32:(unsigned)i inContext:(JSContext *)c { return [self valueWithDouble:i inContext:c]; }
+ (JSValue *)valueWithNewObjectInContext:(JSContext *)c { return [self valueWithJSValueRef:JSObjectMake(c->_ctx, NULL, NULL) inContext:c]; }
+ (JSValue *)valueWithNewArrayInContext:(JSContext *)c { return [self valueWithJSValueRef:JSObjectMakeArray(c->_ctx, 0, NULL, NULL) inContext:c]; }
+ (JSValue *)valueWithNullInContext:(JSContext *)c { return [self valueWithJSValueRef:JSValueMakeNull(c->_ctx) inContext:c]; }
+ (JSValue *)valueWithUndefinedInContext:(JSContext *)c { return [self valueWithJSValueRef:JSValueMakeUndefined(c->_ctx) inContext:c]; }
+ (JSValue *)valueWithNewRegularExpressionFromPattern:(NSString *)p flags:(NSString *)f inContext:(JSContext *)c {
    JSValueRef a[2] = {to_js(c, p ?: @"", 0), to_js(c, f ?: @"", 0)};
    return [self valueWithJSValueRef:JSObjectMakeRegExp(c->_ctx, 2, a, NULL) inContext:c];
}
+ (JSValue *)valueWithNewErrorFromMessage:(NSString *)m inContext:(JSContext *)c {
    JSValueRef a = to_js(c, m ?: @"", 0);
    return [self valueWithJSValueRef:JSObjectMakeError(c->_ctx, 1, &a, NULL) inContext:c];
}
+ (JSValue *)valueWithNewSymbolFromDescription:(NSString *)d inContext:(JSContext *)c {
    JSStringRef s = jsstr(d ?: @"");
    JSValue *r = [self valueWithJSValueRef:JSValueMakeSymbol(c->_ctx, s) inContext:c];
    JSStringRelease(s);
    return r;
}
- (id)toObject { return to_objc(_c->_ctx, _v, 0); }
- (id)toObjectOfClass:(Class)k { id o = [self toObject]; return [o isKindOfClass:k] ? o : nil; }
- (BOOL)toBool { return JSValueToBoolean(_c->_ctx, _v); }
- (double)toDouble { JSValueRef e = NULL; double d = JSValueToNumber(_c->_ctx, _v, &e); if (e) [_c _raise:e]; return d; }
- (int32_t)toInt32 { double d = [self toDouble]; return d != d || d > 1e300 || d < -1e300 ? 0 : (int32_t)(long long)d; }
- (uint32_t)toUInt32 { return (uint32_t)[self toInt32]; }
- (NSNumber *)toNumber { if ([self isBoolean]) return [NSNumber numberWithBool:[self toBool]]; return [NSNumber numberWithDouble:[self toDouble]]; }
- (NSString *)toString {
    JSValueRef e = NULL;
    JSStringRef s = JSValueToStringCopy(_c->_ctx, _v, &e);
    if (e) { [_c _raise:e]; return nil; }
    NSString *r = nsstr(s);
    JSStringRelease(s);
    return r;
}
- (NSDate *)toDate { return [NSDate dateWithTimeIntervalSince1970:[self toDouble] / 1000.0]; }
- (NSArray *)toArray { id o = [self toObject]; return [o isKindOfClass:[NSArray class]] ? o : nil; }
- (NSDictionary *)toDictionary { id o = [self toObject]; return [o isKindOfClass:[NSDictionary class]] ? o : nil; }
- (BOOL)isUndefined { return JSValueGetType(_c->_ctx, _v) == kJSTypeUndefined; }
- (BOOL)isNull { return JSValueGetType(_c->_ctx, _v) == kJSTypeNull; }
- (BOOL)isBoolean { return JSValueGetType(_c->_ctx, _v) == kJSTypeBoolean; }
- (BOOL)isNumber { return JSValueGetType(_c->_ctx, _v) == kJSTypeNumber; }
- (BOOL)isString { return JSValueGetType(_c->_ctx, _v) == kJSTypeString; }
- (BOOL)isObject { return JSValueGetType(_c->_ctx, _v) == kJSTypeObject; }
- (BOOL)isSymbol { return JSValueGetType(_c->_ctx, _v) == kJSTypeSymbol; }
- (BOOL)isBigInt { return JSValueGetType(_c->_ctx, _v) == kJSTypeBigInt; }
- (BOOL)isArray { return JSValueIsArray(_c->_ctx, _v); }
- (BOOL)isDate { return JSValueIsDate(_c->_ctx, _v); }
- (BOOL)isEqualToObject:(id)o { return JSValueIsStrictEqual(_c->_ctx, _v, to_js(_c, o, 0)); }
- (BOOL)isEqualWithTypeCoercionToObject:(id)o { return JSValueIsEqual(_c->_ctx, _v, to_js(_c, o, 0), NULL); }
- (BOOL)isInstanceOf:(id)o {
    JSValueRef c = to_js(_c, o, 0);
    if (JSValueGetType(_c->_ctx, c) != kJSTypeObject) return NO;
    return JSValueIsInstanceOfConstructor(_c->_ctx, _v, (JSObjectRef)c, NULL);
}
static void args_to_js(JSContext *c, NSArray *a, JSValueRef *out, unsigned long n) { for (unsigned long i = 0; i < n; i++) out[i] = to_js(c, [a objectAtIndex:i], 0); }
- (JSValue *)callWithArguments:(NSArray *)args {
    unsigned long n = [args count];
    JSValueRef *a = malloc(sizeof(JSValueRef) * (n ? n : 1));
    args_to_js(_c, args, a, n);
    JSValueRef e = NULL;
    JSValueRef r = JSValueGetType(_c->_ctx, _v) == kJSTypeObject ? JSObjectCallAsFunction(_c->_ctx, (JSObjectRef)_v, NULL, n, a, &e) : NULL;
    free(a);
    if (e) { [_c _raise:e]; return [JSValue valueWithUndefinedInContext:_c]; }
    return r ? [JSValue valueWithJSValueRef:r inContext:_c] : [JSValue valueWithUndefinedInContext:_c];
}
- (JSValue *)constructWithArguments:(NSArray *)args {
    unsigned long n = [args count];
    JSValueRef *a = malloc(sizeof(JSValueRef) * (n ? n : 1));
    args_to_js(_c, args, a, n);
    JSValueRef e = NULL;
    JSObjectRef r = JSValueGetType(_c->_ctx, _v) == kJSTypeObject ? JSObjectCallAsConstructor(_c->_ctx, (JSObjectRef)_v, n, a, &e) : NULL;
    free(a);
    if (e) { [_c _raise:e]; return [JSValue valueWithUndefinedInContext:_c]; }
    return r ? [JSValue valueWithJSValueRef:r inContext:_c] : [JSValue valueWithUndefinedInContext:_c];
}
- (JSValue *)invokeMethod:(NSString *)method withArguments:(NSArray *)args {
    JSValue *f = [self valueForProperty:method];
    unsigned long n = [args count];
    JSValueRef *a = malloc(sizeof(JSValueRef) * (n ? n : 1));
    args_to_js(_c, args, a, n);
    JSValueRef e = NULL;
    JSValueRef r = [f isObject] ? JSObjectCallAsFunction(_c->_ctx, (JSObjectRef)f->_v, (JSObjectRef)_v, n, a, &e) : NULL;
    free(a);
    if (e) { [_c _raise:e]; return [JSValue valueWithUndefinedInContext:_c]; }
    return r ? [JSValue valueWithJSValueRef:r inContext:_c] : [JSValue valueWithUndefinedInContext:_c];
}
- (JSValue *)valueForProperty:(id)key {
    if (![self isObject]) return [JSValue valueWithUndefinedInContext:_c];
    JSValueRef e = NULL;
    JSValueRef r = JSObjectGetPropertyForKey(_c->_ctx, (JSObjectRef)_v, to_js(_c, key, 0), &e);
    if (e) { [_c _raise:e]; return [JSValue valueWithUndefinedInContext:_c]; }
    return [JSValue valueWithJSValueRef:r inContext:_c];
}
- (void)setValue:(id)v forProperty:(id)key {
    if (![self isObject]) return;
    JSValueRef e = NULL;
    JSObjectSetPropertyForKey(_c->_ctx, (JSObjectRef)_v, to_js(_c, key, 0), to_js(_c, v, 0), 0, &e);
    if (e) [_c _raise:e];
}
- (BOOL)deleteProperty:(id)key {
    if (![self isObject]) return NO;
    JSStringRef k = jsstr([key isKindOfClass:[NSString class]] ? key : [key description]);
    BOOL r = JSObjectDeleteProperty(_c->_ctx, (JSObjectRef)_v, k, NULL);
    JSStringRelease(k);
    return r;
}
- (BOOL)hasProperty:(id)key {
    if (![self isObject]) return NO;
    JSStringRef k = jsstr([key isKindOfClass:[NSString class]] ? key : [key description]);
    BOOL r = JSObjectHasProperty(_c->_ctx, (JSObjectRef)_v, k);
    JSStringRelease(k);
    return r;
}
- (void)defineProperty:(id)key descriptor:(id)desc {
    JSValue *op = [[_c globalObject] valueForProperty:@"Object"];
    [op invokeMethod:@"defineProperty" withArguments:[NSArray arrayWithObjects:self, key, desc ?: [NSDictionary dictionary], nil]];
}
- (JSValue *)valueAtIndex:(NSUInteger)i { return [self valueForProperty:[NSNumber numberWithUnsignedInteger:i]]; }
- (void)setValue:(id)v atIndex:(NSUInteger)i { [self setValue:v forProperty:[NSNumber numberWithUnsignedInteger:i]]; }
- (JSValue *)objectForKeyedSubscript:(id)key { return [self valueForProperty:key]; }
- (JSValue *)objectAtIndexedSubscript:(NSUInteger)i { return [self valueAtIndex:i]; }
- (void)setObject:(id)o forKeyedSubscript:(id)key { [self setValue:o forProperty:key]; }
- (void)setObject:(id)o atIndexedSubscript:(NSUInteger)i { [self setValue:o atIndex:i]; }
- (NSString *)description { return [self toString] ?: @"undefined"; }
@end

/* ================================================================ JSManagedValue */
@implementation JSManagedValue
+ (JSManagedValue *)managedValueWithValue:(JSValue *)v { return [[[self alloc] initWithValue:v] autorelease]; }
+ (JSManagedValue *)managedValueWithValue:(JSValue *)v andOwner:(id)owner {
    JSManagedValue *m = [self managedValueWithValue:v];
    [[[v context] virtualMachine] addManagedReference:m withOwner:owner];
    return m;
}
- (instancetype)initWithValue:(JSValue *)v { if ((self = [super init])) _value = [v retain]; return self; }
- (void)dealloc { [_value release]; [super dealloc]; }
- (JSValue *)value { return _value; }
@end
