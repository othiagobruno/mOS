/* mOS — JavaScriptCore.framework, API C: o motor "Brisa" do mOS (kernel/web/js_*.c, o mesmo do WKWebView) rodando no
 * processo do app. Este arquivo liga o motor ao userland (memória, relógio, os ganchos que no kernel o DOM
 * fornece) e implementa a API C pública: grupos e contextos, avaliação de script, JSValue, JSString, JSObject,
 * JSClass com callbacks, typed arrays e proteção de valores.
 *
 * Representação: JSValueRef/JSObjectRef = o valor do motor (64 bits, NaN-boxing) XOR JSC_XOR, para que o número
 * +0.0 não vire NULL (que a API usa como "nada"); o coletor considera o XOR ao varrer a pilha. JSStringRef é uma
 * string UTF-16 com contagem de referências, fora do coletor. Um contexto global é um reino do motor; o grupo
 * é só a contagem. Todo o motor fica atrás de uma trava recursiva (como a de um JSVirtualMachine).
 * A pilha que o coletor varre vai do ponto atual até o topo da thread, então valores guardados em variáveis
 * locais do app continuam vivos, como no JavaScriptCore. */
#include "kernel/web/js_int.h"
#include "jsc_api.h"

#define JSC_XOR 0x7FF8000000000001ull

/* ---------------------------------------------------------------- libc do userland (sem os cabeçalhos do sistema) */
void *malloc(unsigned long n);
void *calloc(unsigned long a, unsigned long b);
void *realloc(void *p, unsigned long n);
void free(void *p);
int write(int fd, const void *b, unsigned long n);
void abort(void);
void *pthread_self(void);
int sched_yield(void);
int gettimeofday(void *tv, void *tz);
#ifdef __APPLE__
void *pthread_get_stackaddr_np(void *t);
unsigned long pthread_get_stacksize_np(void *t);
unsigned long long mach_absolute_time(void);
#endif

void *kmalloc(usize n) { return malloc(n ? n : 1); }
void *kcalloc(usize n, usize sz) { return calloc(n ? n : 1, sz ? sz : 1); }
void *krealloc(void *p, usize n) { return realloc(p, n ? n : 1); }
void kfree(void *p) { free(p); }

static void out_str(const char *s) { write(2, s, strlen(s)); }
void kprintf(const char *fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    out_str(b);
}
void panic(const char *fmt, ...) {
    char b[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    out_str("JavaScriptCore: ");
    out_str(b);
    out_str("\n");
    abort();
    for (;;) {}
}

u64 timer_ticks(void) {
    struct { long sec; long usec; } tv;
    gettimeofday(&tv, 0);
    return (u64)tv.sec * 1000 + (u64)tv.usec / 1000;
}
u64 rdtsc(void) {
#ifdef __APPLE__
    return mach_absolute_time();
#else
    struct { long sec; long usec; } tv;
    gettimeofday(&tv, 0);
    return (u64)tv.sec * 1000000 + (u64)tv.usec;
#endif
}
static u32 rs = 0x9E3779B9u;
u32 rand32(void) { if (rs == 0x9E3779B9u) rs ^= (u32)rdtsc(); rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

/* do gfx do kernel (usado pela conversão de charsets do util.c) */
int utf8_encode(u32 cp, char *out) { return js_utf8_put(out, cp); }

/* data/hora local: o JS do Date usa só para o fuso; sem tzdata aqui, UTC */
void rtc_now(datetime_t *dt) {
    u64 s = timer_ticks() / 1000;
    long days = (long)(s / 86400);
    u32 rem = (u32)(s % 86400);
    dt->hour = (int)(rem / 3600); dt->minute = (int)(rem % 3600 / 60); dt->second = (int)(rem % 60);
    dt->weekday = (int)((days + 4) % 7);
    long z = days + 719468, era = z / 146097, doe = z - era * 146097, yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    dt->day = (int)(doy - (153 * mp + 2) / 5 + 1);
    dt->month = (int)(mp < 10 ? mp + 3 : mp - 9);
    dt->year = (int)(y + (dt->month <= 2));
}

/* ---------------------------------------------------------------- trava do motor (recursiva, por thread) */
static volatile int g_lock;
static void *volatile g_owner;
static int g_depth;
static void lock(void) {
    void *me = pthread_self();
    if (g_owner == me) { g_depth++; return; }
    while (__atomic_exchange_n(&g_lock, 1, __ATOMIC_ACQUIRE)) sched_yield();
    g_owner = me;
    g_depth = 1;
}
static void unlock(void) {
    if (--g_depth) return;
    g_owner = 0;
    __atomic_store_n(&g_lock, 0, __ATOMIC_RELEASE);
}

uptr jsc_test_stack_top;   /* teste de host: topo da pilha (no macOS vem do pthread) */
static uptr stack_top(void *frame) {
#ifdef __APPLE__
    (void)frame;
    return (uptr)pthread_get_stackaddr_np(pthread_self());
#else
    return jsc_test_stack_top ? jsc_test_stack_top : (uptr)frame + 256 * 1024;
#endif
}

/* entrada e saída do motor em cada função da API */
#define ENTER() int jsc_frame_; lock(); jsc_enter(&jsc_frame_)
#define LEAVE() jsc_leave()
static jsrealm *g_saved_realm[64];
static int g_nest;
static void jsc_enter(void *frame) {
    if (g_nest < 64) g_saved_realm[g_nest] = J.realm;
    g_nest++;
    js_enter(frame);
    if (J.entry == 1) {
        uptr top = stack_top(frame);
        if (top > (uptr)frame) {
            J.stack_base = top;
#ifdef __APPLE__
            usize sz = pthread_get_stacksize_np(pthread_self());
            J.stack_limit = sz > 96 * 1024 ? (u32)(sz - 64 * 1024 < (1u << 30) ? sz - 64 * 1024 : (1u << 30)) : 32 * 1024;
#else
            J.stack_limit = (u32)(top - (uptr)frame) + 600 * 1024;
#endif
        }
        J.deadline = 0;
    }
}
static void jsc_leave(void) {
    js_leave();
    g_nest--;
    if (g_nest < 64 && g_saved_realm[g_nest]) J.realm = g_saved_realm[g_nest];
    unlock();
}

/* ---------------------------------------------------------------- valores */
static inline JSValueRef R(jsv v) { return (JSValueRef)(uptr)(v ^ JSC_XOR); }
static inline JSObjectRef RO(jsv v) { return (JSObjectRef)(uptr)(v ^ JSC_XOR); }
static inline jsv V(JSValueRef r) { return r ? ((u64)(uptr)r ^ JSC_XOR) : JS_UNDEF; }
static inline jsobj *O(JSObjectRef r) { jsv v = V(r); return r && jv_isobj(v) ? jv_o(v) : NULL; }

struct OpaqueJSContextGroup { int rc; };
struct OpaqueJSContext {
    jsrealm *realm;
    struct OpaqueJSContextGroup *group;
    int rc;
    JSStringRef name;
    struct OpaqueJSContext *next;
    void *objc;                     /* JSContext do ObjC ligado a este contexto (JSC.m) */
};
struct OpaqueJSString { int rc; u32 len; u16 *u; char *utf8; };
struct OpaqueJSClass {
    int rc;
    JSClassDefinition def;
    char *name;
    JSStaticValue *values;
    JSStaticFunction *funcs;
    int nvalues, nfuncs;
    struct OpaqueJSClass *parent;
};
struct OpaqueJSPropertyNameArray { int rc; int n; JSStringRef *names; };
struct OpaqueJSPropertyNameAccumulator { jsobj *out; };

/* dados de um objeto de classe (CL_HOST: u.host.node aponta para cá) */
typedef struct { JSClassRef cls; void *priv; } hostdata_t;

static struct OpaqueJSContext *g_contexts;

static struct OpaqueJSContext *ctx_of_realm(jsrealm *r) {
    for (struct OpaqueJSContext *c = g_contexts; c; c = c->next) if (c->realm == r) return c;
    return g_contexts;
}
static struct OpaqueJSContext *cur_ctx(void) { return ctx_of_realm(J.realm); }
static void use_ctx(JSContextRef c) { if (c && c->realm) J.realm = ((struct OpaqueJSContext *)c)->realm; }

/* exceção pendente → *exception (ou descarta) */
static void take_exc(JSValueRef *exception) {
    if (!js_pending()) return;
    jsv e = js_catch();
    if (exception) *exception = R(e);
}

/* valores protegidos (JSValueProtect) e outros que o app guarda fora da pilha */
typedef struct { jsv v; u32 n; } prot_t;
static prot_t *g_prot;
static u32 g_nprot, g_cprot;
static void protect(jsv v, int d) {
    if (!jv_isobj(v) && !jv_isstr(v) && !jv_isbig(v)) return;
    for (u32 i = 0; i < g_nprot; i++)
        if (g_prot[i].v == v) {
            if (d > 0) g_prot[i].n++;
            else if (--g_prot[i].n == 0) g_prot[i] = g_prot[--g_nprot];
            return;
        }
    if (d < 0) return;
    if (g_nprot == g_cprot) {
        u32 nc = g_cprot ? g_cprot * 2 : 64;
        prot_t *np = krealloc(g_prot, sizeof(prot_t) * nc);
        if (!np) return;
        g_prot = np;
        g_cprot = nc;
    }
    g_prot[g_nprot].v = v;
    g_prot[g_nprot].n = 1;
    g_nprot++;
}

/* ---------------------------------------------------------------- ganchos que no kernel vêm do DOM */
void js_mark_pages(void) {
    for (u32 i = 0; i < g_nprot; i++) gc_mark_val(g_prot[i].v);
}
const char *js_page_base(struct js_page *p) { (void)p; return ""; }
void js_report_exception(void) {
    jsv e = js_catch();
    jsstr *m = js_exc_string(e);
    kprintf("JavaScriptCore: exceção não tratada: %s\n", m ? m->d : "?");
}
void js_console_log(int level, const char *msg) {
    static const char *tags[] = {"log", "info", "warn", "error"};
    if (level < 0) return;
    kprintf("JS %s: %s\n", tags[level > 3 ? 3 : level], msg);
}
/* módulos ES não existem no JavaScriptCore.framework (só scripts): import falha com erro */
void js_net_fetch_module(struct js_page *p, const char *url, jsobj *mod) {
    (void)p; (void)mod;
    js_throw_err(E_TYPE, "import de '%s': o JavaScriptCore não carrega módulos pela rede", url);
}
/* sem DOM: nenhum nó tem atributos */
const char *node_attr(node_t *n, const char *name) { (void)n; (void)name; return NULL; }

static hostdata_t *hdata(jsobj *o) { return o && o->cls == CL_HOST && !o->u.host.page ? (hostdata_t *)o->u.host.node : NULL; }

static JSStringRef str_from_js(jsstr *s);
static jsstr *str_to_js(JSStringRef s);
JSC_EXPORT void JSStringRelease(JSStringRef s);
JSC_EXPORT void JSClassRelease(JSClassRef c);
JSC_EXPORT JSClassRef JSClassRetain(JSClassRef c);

/* propriedade dinâmica de objeto de classe: valores estáticos, funções estáticas e os callbacks, do filho ao pai */
static jsv jsc_fn_tramp(jsv thisv, int argc, jsv *argv, jsobj *self, int flags);
jsv js_dom_get(jsobj *o, jsstr *k, bool *found) {
    *found = false;
    hostdata_t *h = hdata(o);
    if (!h || k->sym) return JS_UNDEF;
    JSStringRef name = NULL;
    jsv out = JS_UNDEF;
    for (JSClassRef c = h->cls; c && !*found; c = c->parent) {
        for (int i = 0; i < c->nvalues; i++)
            if (!strcmp(c->values[i].name, k->d) && c->values[i].getProperty) {
                if (!name) name = str_from_js(k);
                JSValueRef exc = NULL;
                JSValueRef r = c->values[i].getProperty(cur_ctx(), RO(jv_obj(o)), name, &exc);
                if (exc) { js_throw(V(exc)); *found = true; out = JS_EXC; goto done; }
                if (r) { *found = true; out = V(r); goto done; }
            }
        for (int i = 0; i < c->nfuncs; i++)
            if (!strcmp(c->funcs[i].name, k->d) && c->funcs[i].callAsFunction) {
                if (js_ownprop(o, k)) goto done;   /* já materializada (ou trocada pelo script) */
                jsobj *f = js_newnative(jsc_fn_tramp, c->funcs[i].name, 0, 0);
                f->u.nat.data = jv_num((double)(uptr)c->funcs[i].callAsFunction);
                js_defprop(o, k, jv_obj(f), (c->funcs[i].attributes & kJSPropertyAttributeDontEnum ? PF_NOENUM : 0) |
                                            (c->funcs[i].attributes & kJSPropertyAttributeReadOnly ? PF_RO : 0));
                goto done;
            }
        if (c->def.hasProperty) {
            if (!name) name = str_from_js(k);
            if (!c->def.hasProperty(cur_ctx(), RO(jv_obj(o)), name)) continue;
        }
        if (c->def.getProperty) {
            if (!name) name = str_from_js(k);
            JSValueRef exc = NULL;
            JSValueRef r = c->def.getProperty(cur_ctx(), RO(jv_obj(o)), name, &exc);
            if (exc) { js_throw(V(exc)); *found = true; out = JS_EXC; goto done; }
            if (r) { *found = true; out = V(r); goto done; }
        }
    }
done:
    if (name) JSStringRelease(name);
    return out;
}

int js_dom_set(jsobj *o, jsstr *k, jsv v) {
    hostdata_t *h = hdata(o);
    if (!h || k->sym) return 0;
    JSStringRef name = NULL;
    int r = 0;
    for (JSClassRef c = h->cls; c && !r; c = c->parent) {
        for (int i = 0; i < c->nvalues; i++)
            if (!strcmp(c->values[i].name, k->d)) {
                if (c->values[i].attributes & kJSPropertyAttributeReadOnly) { r = 1; goto done; }
                if (!c->values[i].setProperty) continue;
                if (!name) name = str_from_js(k);
                JSValueRef exc = NULL;
                bool ok = c->values[i].setProperty(cur_ctx(), RO(jv_obj(o)), name, R(v), &exc);
                if (exc) { js_throw(V(exc)); r = -1; goto done; }
                if (ok) { r = 1; goto done; }
            }
        if (c->def.setProperty) {
            if (!name) name = str_from_js(k);
            JSValueRef exc = NULL;
            bool ok = c->def.setProperty(cur_ctx(), RO(jv_obj(o)), name, R(v), &exc);
            if (exc) { js_throw(V(exc)); r = -1; goto done; }
            if (ok) { r = 1; goto done; }
        }
    }
done:
    if (name) JSStringRelease(name);
    return r;
}

static int host_delete(jsobj *o, jsstr *k) {
    hostdata_t *h = hdata(o);
    if (!h || k->sym) return 0;
    for (JSClassRef c = h->cls; c; c = c->parent) {
        for (int i = 0; i < c->nvalues; i++)
            if (!strcmp(c->values[i].name, k->d) && (c->values[i].attributes & kJSPropertyAttributeDontDelete)) return 1;
        if (c->def.deleteProperty) {
            JSStringRef name = str_from_js(k);
            JSValueRef exc = NULL;
            bool ok = c->def.deleteProperty(cur_ctx(), RO(jv_obj(o)), name, &exc);
            JSStringRelease(name);
            if (exc) { js_throw(V(exc)); return -1; }
            if (ok) return 1;
        }
    }
    return 0;
}

static void host_keys(jsobj *o, jsobj *out) {
    hostdata_t *h = hdata(o);
    if (!h) return;
    for (JSClassRef c = h->cls; c; c = c->parent) {
        for (int i = 0; i < c->nvalues; i++)
            if (!(c->values[i].attributes & kJSPropertyAttributeDontEnum)) js_arr_push(out, jv_str(js_intern(c->values[i].name, -1)));
        for (int i = 0; i < c->nfuncs; i++)
            if (!(c->funcs[i].attributes & kJSPropertyAttributeDontEnum) && !js_ownprop(o, js_intern(c->funcs[i].name, -1)))
                js_arr_push(out, jv_str(js_intern(c->funcs[i].name, -1)));
        if (c->def.getPropertyNames) {
            struct OpaqueJSPropertyNameAccumulator acc = {out};
            c->def.getPropertyNames(cur_ctx(), RO(jv_obj(o)), &acc);
        }
    }
}

static hostdata_t *any_hdata(jsobj *o);
static void host_finalize(jsobj *o) {
    hostdata_t *h = any_hdata(o);
    if (!h) return;
    if (o->cls == CL_NATIVE) o->u.nat.data2 = JS_UNDEF;
    if (!(o->cls == CL_NATIVE && o->u.nat.magic == 3))   /* o construtor de JSObjectMakeConstructor não é instância */
        for (JSClassRef c = h->cls; c; c = c->parent) if (c->def.finalize) c->def.finalize(RO(jv_obj(o)));
    JSClassRelease(h->cls);
    kfree(h);
    if (o->cls == CL_HOST) o->u.host.node = NULL;
}

static bool g_init;
static void init_once(void) {
    if (g_init) return;
    g_init = true;
    js_ref_xor = JSC_XOR;
    js_host_finalize_hook = host_finalize;
    js_host_delete_hook = host_delete;
    js_host_keys_hook = host_keys;
    js_core_init();
}

/* ---------------------------------------------------------------- strings */
static JSStringRef str_new(u32 len) {
    JSStringRef s = kcalloc(1, sizeof(struct OpaqueJSString));
    if (!s) return NULL;
    s->rc = 1;
    s->len = len;
    s->u = kmalloc(sizeof(u16) * (len ? len : 1));
    if (!s->u) { kfree(s); return NULL; }
    return s;
}
static JSStringRef str_from_js(jsstr *j) {
    JSStringRef s = str_new(j->ulen);
    if (!s) return NULL;
    for (u32 i = 0; i < j->ulen; i++) s->u[i] = (u16)js_str_unit(j, (int)i);
    return s;
}
static jsstr *str_to_js(JSStringRef s) { return s ? js_str_from_units(s->u, (int)s->len) : A[A_empty]; }

JSC_EXPORT JSStringRef JSStringCreateWithCharacters(const JSChar *chars, unsigned long n) {
    JSStringRef s = str_new((u32)n);
    if (s && n) memcpy(s->u, chars, sizeof(u16) * n);
    return s;
}
JSC_EXPORT JSStringRef JSStringCreateWithUTF8CString(const char *c) {
    if (!c) c = "";
    int n = (int)strlen(c);
    JSStringRef s = str_new((u32)n * 2 + 1);
    if (!s) return NULL;
    u32 k = 0;
    for (int i = 0; i < n;) {
        u32 cp = js_utf8_get(c, n, &i);
        if (cp >= 0x10000) { cp -= 0x10000; s->u[k++] = (u16)(0xD800 + (cp >> 10)); s->u[k++] = (u16)(0xDC00 + (cp & 0x3FF)); }
        else s->u[k++] = (u16)cp;
    }
    s->len = k;
    return s;
}
JSC_EXPORT JSStringRef JSStringRetain(JSStringRef s) { if (s) __atomic_add_fetch(&s->rc, 1, __ATOMIC_RELAXED); return s; }
JSC_EXPORT void JSStringRelease(JSStringRef s) {
    if (!s || __atomic_sub_fetch(&s->rc, 1, __ATOMIC_ACQ_REL)) return;
    kfree(s->u);
    kfree(s->utf8);
    kfree(s);
}
JSC_EXPORT unsigned long JSStringGetLength(JSStringRef s) { return s ? s->len : 0; }
JSC_EXPORT const JSChar *JSStringGetCharactersPtr(JSStringRef s) { return s ? s->u : NULL; }
JSC_EXPORT unsigned long JSStringGetMaximumUTF8CStringSize(JSStringRef s) { return s ? (unsigned long)s->len * 3 + 1 : 1; }
JSC_EXPORT unsigned long JSStringGetUTF8CString(JSStringRef s, char *buf, unsigned long cap) {
    if (!buf || !cap) return 0;
    unsigned long k = 0;
    for (u32 i = 0; s && i < s->len; i++) {
        u32 cp = s->u[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s->len && s->u[i + 1] >= 0xDC00 && s->u[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (s->u[i + 1] - 0xDC00);
            i++;
        } else if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;
        char t[4];
        int n = js_utf8_put(t, cp);
        if (k + (unsigned long)n + 1 > cap) break;   /* nunca corta um caractere no meio */
        memcpy(buf + k, t, (usize)n);
        k += (unsigned long)n;
    }
    buf[k] = 0;
    return k + 1;
}
JSC_EXPORT bool JSStringIsEqual(JSStringRef a, JSStringRef b) {
    if (!a || !b) return a == b;
    return a->len == b->len && !memcmp(a->u, b->u, sizeof(u16) * a->len);
}
JSC_EXPORT bool JSStringIsEqualToUTF8CString(JSStringRef a, const char *b) {
    JSStringRef t = JSStringCreateWithUTF8CString(b);
    bool r = JSStringIsEqual(a, t);
    JSStringRelease(t);
    return r;
}

/* ---------------------------------------------------------------- grupos e contextos */
JSC_EXPORT JSContextGroupRef JSContextGroupCreate(void) {
    struct OpaqueJSContextGroup *g = kcalloc(1, sizeof *g);
    if (g) g->rc = 1;
    return g;
}
JSC_EXPORT JSContextGroupRef JSContextGroupRetain(JSContextGroupRef g) {
    if (g) __atomic_add_fetch(&((struct OpaqueJSContextGroup *)g)->rc, 1, __ATOMIC_RELAXED);
    return g;
}
JSC_EXPORT void JSContextGroupRelease(JSContextGroupRef g) {
    if (g && !__atomic_sub_fetch(&((struct OpaqueJSContextGroup *)g)->rc, 1, __ATOMIC_ACQ_REL)) kfree((void *)g);
}

JSC_EXPORT JSClassRef JSClassRetain(JSClassRef c);
JSC_EXPORT void JSClassRelease(JSClassRef c);
static jsobj *make_host(JSClassRef cls, void *priv, jsobj *proto);
static void run_initialize(JSContextRef ctx, JSClassRef cls, jsobj *o);

JSC_EXPORT JSGlobalContextRef JSGlobalContextCreateInGroup(JSContextGroupRef group, JSClassRef globalClass) {
    lock();
    init_once();
    int frame;
    js_enter(&frame);
    jsrealm *r = js_realm_new();
    struct OpaqueJSContext *c = r ? kcalloc(1, sizeof *c) : NULL;
    if (!c) { js_leave(); unlock(); return NULL; }
    c->realm = r;
    c->rc = 1;
    c->group = (struct OpaqueJSContextGroup *)(group ? JSContextGroupRetain(group) : JSContextGroupCreate());
    c->next = g_contexts;
    g_contexts = c;
    jsrealm *saved = J.realm;
    J.realm = r;
    if (globalClass) {
        /* a classe do objeto global: as propriedades dela respondem no global (via um objeto de classe no topo da
         * cadeia de protótipos, porque o objeto global do motor já existe) */
        jsobj *h = make_host(globalClass, NULL, r->global->proto);
        if (h) { r->global->proto = h; run_initialize(c, globalClass, h); }
    }
    J.realm = saved ? saved : r;
    js_leave();
    unlock();
    return c;
}
JSC_EXPORT JSGlobalContextRef JSGlobalContextCreate(JSClassRef globalClass) { return JSGlobalContextCreateInGroup(NULL, globalClass); }
JSC_EXPORT JSGlobalContextRef JSGlobalContextRetain(JSGlobalContextRef c) { if (c) __atomic_add_fetch(&c->rc, 1, __ATOMIC_RELAXED); return c; }
JSC_EXPORT void JSGlobalContextRelease(JSGlobalContextRef c) {
    if (!c) return;
    lock();
    if (--c->rc == 0) {
        for (struct OpaqueJSContext **pp = &g_contexts; *pp; pp = &(*pp)->next) if (*pp == c) { *pp = c->next; break; }
        int frame;
        js_enter(&frame);
        js_realm_free(c->realm);
        js_leave();
        if (J.entry == 0) gc_collect();
        JSContextGroupRelease(c->group);
        if (c->name) JSStringRelease(c->name);
        kfree(c);
    }
    unlock();
}
JSC_EXPORT JSObjectRef JSContextGetGlobalObject(JSContextRef c) { return c ? RO(jv_obj(c->realm->global)) : NULL; }
JSC_EXPORT JSContextGroupRef JSContextGetGroup(JSContextRef c) { return c ? c->group : NULL; }
JSC_EXPORT JSGlobalContextRef JSContextGetGlobalContext(JSContextRef c) { return (JSGlobalContextRef)c; }
JSC_EXPORT JSStringRef JSGlobalContextCopyName(JSGlobalContextRef c) { return c && c->name ? JSStringRetain(c->name) : NULL; }
JSC_EXPORT void JSGlobalContextSetName(JSGlobalContextRef c, JSStringRef name) {
    if (!c) return;
    if (c->name) JSStringRelease(c->name);
    c->name = name ? JSStringRetain(name) : NULL;
}
JSC_EXPORT void *jsc_context_objc(JSContextRef c) { return c ? c->objc : NULL; }
JSC_EXPORT void jsc_context_set_objc(JSContextRef c, void *o) { if (c) ((struct OpaqueJSContext *)c)->objc = o; }

/* ---------------------------------------------------------------- avaliação */
static char *utf8_of(JSStringRef s, int *len) {
    unsigned long cap = JSStringGetMaximumUTF8CStringSize(s);
    char *b = kmalloc(cap);
    if (!b) return NULL;
    unsigned long n = JSStringGetUTF8CString(s, b, cap);
    *len = n ? (int)n - 1 : 0;
    return b;
}

JSC_EXPORT JSValueRef JSEvaluateScript(JSContextRef ctx, JSStringRef script, JSObjectRef thisObject, JSStringRef sourceURL,
                                       int startingLineNumber, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    int len;
    char *src = utf8_of(script, &len);
    char *url = NULL;
    int ul = 0;
    if (sourceURL) url = utf8_of(sourceURL, &ul);
    JSValueRef out = NULL;
    jsstr *err = NULL;
    jsscript *s = src ? js_parse(src, len, url ? url : "JSEvaluateScript", startingLineNumber > 0 ? startingLineNumber : 1, &err) : NULL;
    if (!s) {
        jsobj *e = js_make_err(E_SYNTAX, err ? err : js_strz("erro de sintaxe"));
        if (exception) *exception = R(jv_obj(e));
    } else {
        jsv r;
        if (thisObject && O(thisObject) != J.realm->global) {
            /* this próprio: o programa roda no escopo global (as declarações continuam globais) com esse this */
            jsenv *ge = J.realm->glex;
            jsv saved_this = ge->thisv;
            ge->thisv = V(thisObject);
            r = js_run_script(s);
            ge->thisv = saved_this;
        } else r = js_run_script(s);
        if (r == JS_EXC) take_exc(exception);
        else out = R(r);
        js_run_jobs();
        if (js_pending()) js_catch();
    }
    kfree(src);
    kfree(url);
    LEAVE();
    return out;
}

JSC_EXPORT bool JSCheckScriptSyntax(JSContextRef ctx, JSStringRef script, JSStringRef sourceURL, int startingLineNumber, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    int len;
    char *src = utf8_of(script, &len);
    jsstr *err = NULL;
    (void)sourceURL;
    jsscript *s = src ? js_parse(src, len, "JSCheckScriptSyntax", startingLineNumber > 0 ? startingLineNumber : 1, &err) : NULL;
    if (!s && exception) *exception = R(jv_obj(js_make_err(E_SYNTAX, err ? err : js_strz("erro de sintaxe"))));
    kfree(src);
    LEAVE();
    return s != NULL;
}

JSC_EXPORT void JSGarbageCollect(JSContextRef ctx) {
    (void)ctx;
    ENTER();
    gc_collect();
    LEAVE();
}

/* ---------------------------------------------------------------- JSValue */
JSC_EXPORT JSType JSValueGetType(JSContextRef ctx, JSValueRef r) {
    (void)ctx;
    jsv v = V(r);
    if (v == JS_UNDEF) return kJSTypeUndefined;
    if (v == JS_NULL) return kJSTypeNull;
    if (jv_isbool(v)) return kJSTypeBoolean;
    if (jv_isnum(v)) return kJSTypeNumber;
    if (jv_isbig(v)) return kJSTypeBigInt;
    if (jv_isstr(v)) return jv_s(v)->sym ? kJSTypeSymbol : kJSTypeString;
    return kJSTypeObject;
}
JSC_EXPORT bool JSValueIsUndefined(JSContextRef c, JSValueRef v) { return JSValueGetType(c, v) == kJSTypeUndefined; }
JSC_EXPORT bool JSValueIsNull(JSContextRef c, JSValueRef v) { return JSValueGetType(c, v) == kJSTypeNull; }
JSC_EXPORT bool JSValueIsBoolean(JSContextRef c, JSValueRef v) { return JSValueGetType(c, v) == kJSTypeBoolean; }
JSC_EXPORT bool JSValueIsNumber(JSContextRef c, JSValueRef v) { return JSValueGetType(c, v) == kJSTypeNumber; }
JSC_EXPORT bool JSValueIsString(JSContextRef c, JSValueRef v) { return JSValueGetType(c, v) == kJSTypeString; }
JSC_EXPORT bool JSValueIsSymbol(JSContextRef c, JSValueRef v) { return JSValueGetType(c, v) == kJSTypeSymbol; }
JSC_EXPORT bool JSValueIsBigInt(JSContextRef c, JSValueRef v) { return JSValueGetType(c, v) == kJSTypeBigInt; }
JSC_EXPORT bool JSValueIsObject(JSContextRef c, JSValueRef v) { return JSValueGetType(c, v) == kJSTypeObject; }
JSC_EXPORT bool JSValueIsArray(JSContextRef c, JSValueRef v) { (void)c; return js_isarray(V(v)); }
JSC_EXPORT bool JSValueIsDate(JSContextRef c, JSValueRef v) { (void)c; return jv_iscls(V(v), CL_DATE); }
JSC_EXPORT bool JSValueIsObjectOfClass(JSContextRef c, JSValueRef v, JSClassRef cls) {
    (void)c;
    hostdata_t *h = hdata(jv_isobj(V(v)) ? jv_o(V(v)) : NULL);
    for (JSClassRef k = h ? h->cls : NULL; k; k = k->parent) if (k == cls) return true;
    return false;
}

JSC_EXPORT bool JSValueIsEqual(JSContextRef ctx, JSValueRef a, JSValueRef b, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    int r = js_loose_eq(V(a), V(b));
    if (r < 0) { take_exc(exception); r = 0; }
    LEAVE();
    return r == 1;
}
JSC_EXPORT bool JSValueIsStrictEqual(JSContextRef ctx, JSValueRef a, JSValueRef b) { (void)ctx; return js_strict_eq(V(a), V(b)); }
JSC_EXPORT bool JSValueIsInstanceOfConstructor(JSContextRef ctx, JSValueRef v, JSObjectRef ctor, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    int r = js_instanceof(V(v), V(ctor));
    if (r < 0) { take_exc(exception); r = 0; }
    LEAVE();
    return r == 1;
}

JSC_EXPORT JSValueRef JSValueMakeUndefined(JSContextRef c) { (void)c; return R(JS_UNDEF); }
JSC_EXPORT JSValueRef JSValueMakeNull(JSContextRef c) { (void)c; return R(JS_NULL); }
JSC_EXPORT JSValueRef JSValueMakeBoolean(JSContextRef c, bool b) { (void)c; return R(jv_bool(b)); }
JSC_EXPORT JSValueRef JSValueMakeNumber(JSContextRef c, double d) { (void)c; return R(d != d ? (jsv)JS_NAN_BITS : jv_num(d)); }
JSC_EXPORT JSValueRef JSValueMakeString(JSContextRef ctx, JSStringRef s) {
    ENTER();
    use_ctx(ctx);
    JSValueRef r = R(jv_str(str_to_js(s)));
    LEAVE();
    return r;
}
JSC_EXPORT JSValueRef JSValueMakeSymbol(JSContextRef ctx, JSStringRef desc) {
    ENTER();
    use_ctx(ctx);
    JSValueRef r = R(jv_str(js_new_symbol(desc ? str_to_js(desc) : NULL)));
    LEAVE();
    return r;
}
JSC_EXPORT JSValueRef JSValueMakeFromJSONString(JSContextRef ctx, JSStringRef s) {
    ENTER();
    use_ctx(ctx);
    jsv v = js_json_parse(str_to_js(s), JS_UNDEF);
    JSValueRef r = NULL;
    if (v == JS_EXC) js_catch();
    else r = R(v);
    LEAVE();
    return r;
}
JSC_EXPORT JSStringRef JSValueCreateJSONString(JSContextRef ctx, JSValueRef v, unsigned indent, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    jsv r = js_json_stringify(V(v), JS_UNDEF, indent ? jv_num(indent > 10 ? 10 : indent) : JS_UNDEF);
    JSStringRef out = NULL;
    if (r == JS_EXC) take_exc(exception);
    else if (jv_isstr(r)) out = str_from_js(jv_s(r));
    LEAVE();
    return out;
}
JSC_EXPORT bool JSValueToBoolean(JSContextRef c, JSValueRef v) { (void)c; return js_truthy(V(v)); }
JSC_EXPORT double JSValueToNumber(JSContextRef ctx, JSValueRef v, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    double d = js_tonum(V(v));
    if (js_pending()) { take_exc(exception); d = jv_n(JS_NAN_BITS); }
    LEAVE();
    return d;
}
JSC_EXPORT JSStringRef JSValueToStringCopy(JSContextRef ctx, JSValueRef v, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    jsstr *s = js_tostr(V(v));
    JSStringRef out = NULL;
    if (!s) take_exc(exception);
    else out = str_from_js(s);
    LEAVE();
    return out;
}
JSC_EXPORT JSObjectRef JSValueToObject(JSContextRef ctx, JSValueRef v, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    jsv o = js_toobj(V(v));
    JSObjectRef out = NULL;
    if (o == JS_EXC) take_exc(exception);
    else out = RO(o);
    LEAVE();
    return out;
}
JSC_EXPORT void JSValueProtect(JSContextRef c, JSValueRef v) { (void)c; lock(); protect(V(v), 1); unlock(); }
JSC_EXPORT void JSValueUnprotect(JSContextRef c, JSValueRef v) { (void)c; lock(); protect(V(v), -1); unlock(); }

/* ---------------------------------------------------------------- classes */
JSC_EXPORT const JSClassDefinition kJSClassDefinitionEmpty = {0};

static char *dup_c(const char *s) { if (!s) return NULL; usize n = strlen(s); char *d = kmalloc(n + 1); if (d) memcpy(d, s, n + 1); return d; }

JSC_EXPORT JSClassRef JSClassCreate(const JSClassDefinition *def) {
    if (!def) return NULL;
    JSClassRef c = kcalloc(1, sizeof(struct OpaqueJSClass));
    if (!c) return NULL;
    c->rc = 1;
    c->def = *def;
    c->name = dup_c(def->className ? def->className : "Object");
    c->def.className = c->name;
    if (def->staticValues) {
        int n = 0;
        while (def->staticValues[n].name) n++;
        c->values = kcalloc((usize)n + 1, sizeof(JSStaticValue));
        for (int i = 0; c->values && i < n; i++) { c->values[i] = def->staticValues[i]; c->values[i].name = dup_c(def->staticValues[i].name); }
        c->nvalues = c->values ? n : 0;
    }
    if (def->staticFunctions) {
        int n = 0;
        while (def->staticFunctions[n].name) n++;
        c->funcs = kcalloc((usize)n + 1, sizeof(JSStaticFunction));
        for (int i = 0; c->funcs && i < n; i++) { c->funcs[i] = def->staticFunctions[i]; c->funcs[i].name = dup_c(def->staticFunctions[i].name); }
        c->nfuncs = c->funcs ? n : 0;
    }
    c->parent = def->parentClass ? JSClassRetain(def->parentClass) : NULL;
    return c;
}
JSC_EXPORT JSClassRef JSClassRetain(JSClassRef c) { if (c) __atomic_add_fetch(&c->rc, 1, __ATOMIC_RELAXED); return c; }
JSC_EXPORT void JSClassRelease(JSClassRef c) {
    if (!c || __atomic_sub_fetch(&c->rc, 1, __ATOMIC_ACQ_REL)) return;
    for (int i = 0; i < c->nvalues; i++) kfree((void *)c->values[i].name);
    for (int i = 0; i < c->nfuncs; i++) kfree((void *)c->funcs[i].name);
    kfree(c->values); kfree(c->funcs); kfree(c->name);
    JSClassRelease(c->parent);
    kfree(c);
}

static jsobj *make_host(JSClassRef cls, void *priv, jsobj *proto) {
    hostdata_t *h = kcalloc(1, sizeof *h);
    if (!h) return NULL;
    h->cls = JSClassRetain(cls);
    h->priv = priv;
    jsobj *o = js_newobj_cls(CL_HOST, proto ? proto : J.realm->p_object);
    o->u.host.page = NULL;
    o->u.host.node = (node_t *)h;
    o->u.host.extra = JS_UNDEF;
    return o;
}
static void run_initialize(JSContextRef ctx, JSClassRef cls, jsobj *o) {
    /* do pai para o filho, como no JSC */
    JSClassRef chain[32];
    int n = 0;
    for (JSClassRef c = cls; c && n < 32; c = c->parent) chain[n++] = c;
    for (int i = n - 1; i >= 0; i--) if (chain[i]->def.initialize) chain[i]->def.initialize(ctx, RO(jv_obj(o)));
}

/* chamada de objeto de classe com callAsFunction/callAsConstructor (o objeto é uma função nativa com dados) */
static JSClassRef callable_class(JSClassRef c, bool ctor) {
    for (; c; c = c->parent) if (ctor ? c->def.callAsConstructor != NULL : c->def.callAsFunction != NULL) return c;
    return NULL;
}

static jsv jsc_fn_tramp(jsv thisv, int argc, jsv *argv, jsobj *self, int flags) {
    JSContextRef ctx = cur_ctx();
    JSValueRef stackargs[16];
    JSValueRef *args = argc <= 16 ? stackargs : kmalloc(sizeof(JSValueRef) * (usize)argc);
    if (!args) return js_throw_err(E_RANGE, "sem memória");
    for (int i = 0; i < argc; i++) args[i] = R(argv[i]);
    JSValueRef exc = NULL;
    jsv out = JS_UNDEF;
    int kind = self->u.nat.magic;   /* 0 função com callback, 2 objeto de classe chamável, 3 construtor de classe */
    if (kind == 0) {
        JSObjectCallAsFunctionCallback cb = (JSObjectCallAsFunctionCallback)(uptr)jv_n(self->u.nat.data);
        JSValueRef r = cb(ctx, RO(jv_obj(self)), jv_isobj(thisv) ? RO(thisv) : NULL, (unsigned long)argc, args, &exc);
        out = r ? V(r) : JS_UNDEF;
    } else if (kind == 2 || kind == 3) {
        hostdata_t *h = (hostdata_t *)(uptr)jv_n(self->u.nat.data2);
        JSClassRef cc = h ? callable_class(h->cls, (flags & NF_NEW) != 0) : NULL;
        if ((flags & NF_NEW) && cc) {
            JSObjectRef r = cc->def.callAsConstructor(ctx, RO(jv_obj(self)), (unsigned long)argc, args, &exc);
            out = r ? V(r) : JS_UNDEF;
        } else if ((flags & NF_NEW) && kind == 3) {
            jsobj *o = make_host(h->cls, NULL, NULL);
            if (o) { run_initialize(ctx, h->cls, o); out = jv_obj(o); }
        } else if ((cc = h ? callable_class(h->cls, false) : NULL)) {
            JSValueRef r = cc->def.callAsFunction(ctx, RO(jv_obj(self)), jv_isobj(thisv) ? RO(thisv) : NULL, (unsigned long)argc, args, &exc);
            out = r ? V(r) : JS_UNDEF;
        } else exc = R(jv_obj(js_make_err(E_TYPE, js_strz("o objeto não é uma função"))));
    }
    if (args != stackargs) kfree(args);
    if (exc) return js_throw(V(exc));
    return out;
}

JSC_EXPORT JSObjectRef JSObjectMake(JSContextRef ctx, JSClassRef cls, void *data) {
    ENTER();
    use_ctx(ctx);
    jsobj *o;
    if (!cls) o = js_newobj(J.realm->p_object);
    else if (callable_class(cls, false) || callable_class(cls, true)) {
        /* chamável: função nativa que despacha para o callAsFunction/callAsConstructor da classe */
        hostdata_t *h = kcalloc(1, sizeof *h);
        h->cls = JSClassRetain(cls);
        h->priv = data;
        o = js_newnative(jsc_fn_tramp, cls->name, 0, 2);
        o->is_class = callable_class(cls, true) != NULL;
        o->u.nat.data2 = jv_num((double)(uptr)h);
        js_defn(o, "\x01jsch", jv_num((double)(uptr)h), PF_NOENUM);
        run_initialize(ctx, cls, o);
    } else {
        o = make_host(cls, data, NULL);
        if (o) run_initialize(ctx, cls, o);
    }
    LEAVE();
    return o ? RO(jv_obj(o)) : NULL;
}

static hostdata_t *any_hdata(jsobj *o) {
    hostdata_t *h = hdata(o);
    if (h) return h;
    if (o && o->cls == CL_NATIVE && o->u.nat.fn == jsc_fn_tramp && (o->u.nat.magic == 2 || o->u.nat.magic == 3))
        return (hostdata_t *)(uptr)jv_n(o->u.nat.data2);
    return NULL;
}
JSC_EXPORT void *JSObjectGetPrivate(JSObjectRef r) {
    jsobj *o = O(r);
    hostdata_t *h = any_hdata(o);
    if (!h && o && o->proto && o->proto->cls == CL_HOST) {
        /* objeto global de contexto criado com classe */
        for (struct OpaqueJSContext *c = g_contexts; c; c = c->next) if (c->realm->global == o) { h = hdata(o->proto); break; }
    }
    return h ? h->priv : NULL;
}
JSC_EXPORT bool JSObjectSetPrivate(JSObjectRef r, void *data) {
    jsobj *o = O(r);
    hostdata_t *h = any_hdata(o);
    if (!h && o && o->proto && o->proto->cls == CL_HOST)
        for (struct OpaqueJSContext *c = g_contexts; c; c = c->next) if (c->realm->global == o) { h = hdata(o->proto); break; }
    if (!h) return false;
    h->priv = data;
    return true;
}

JSC_EXPORT JSObjectRef JSObjectMakeFunctionWithCallback(JSContextRef ctx, JSStringRef name, JSObjectCallAsFunctionCallback cb) {
    ENTER();
    use_ctx(ctx);
    char *n = NULL;
    int nl = 0;
    if (name) n = utf8_of(name, &nl);
    jsobj *f = js_newnative(jsc_fn_tramp, n ? n : "anonymous", 0, 0);
    f->u.nat.data = jv_num((double)(uptr)cb);
    kfree(n);
    LEAVE();
    return RO(jv_obj(f));
}
JSC_EXPORT JSObjectRef JSObjectMakeConstructor(JSContextRef ctx, JSClassRef cls, JSObjectCallAsConstructorCallback cb) {
    ENTER();
    use_ctx(ctx);
    hostdata_t *h = kcalloc(1, sizeof *h);
    /* uma classe interna só para levar o callAsConstructor; o objeto criado sem callback é da classe dada */
    JSClassDefinition d = kJSClassDefinitionEmpty;
    d.className = cls ? cls->name : "Object";
    d.parentClass = cls;
    d.callAsConstructor = cb;
    h->cls = JSClassCreate(&d);
    jsobj *f = js_newnative(jsc_fn_tramp, d.className, 0, 3);
    f->is_class = 1;
    f->u.nat.data2 = jv_num((double)(uptr)h);
    js_defn(f, "\x01jsch", jv_num((double)(uptr)h), PF_NOENUM);
    jsobj *proto = js_newobj(J.realm->p_object);
    js_defprop(f, A[A_prototype], jv_obj(proto), PF_NOENUM | PF_NOCONF);
    js_defprop(proto, A[A_constructor], jv_obj(f), PF_NOENUM);
    LEAVE();
    return RO(jv_obj(f));
}

/* new Array/Date/Error/RegExp pelo construtor do reino */
static JSObjectRef make_by_ctor(JSContextRef ctx, const char *name, unsigned long argc, const JSValueRef argv[], JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    jsv c = js_getn(jv_obj(J.realm->global), name);
    jsv args[32];
    int n = argc > 32 ? 32 : (int)argc;
    for (int i = 0; i < n; i++) args[i] = V(argv[i]);
    jsv r = c == JS_EXC ? JS_EXC : js_construct(c, n, args, c);
    JSObjectRef out = NULL;
    if (r == JS_EXC) take_exc(exception);
    else out = RO(r);
    LEAVE();
    return out;
}
JSC_EXPORT JSObjectRef JSObjectMakeArray(JSContextRef ctx, unsigned long argc, const JSValueRef argv[], JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    jsobj *a = js_newarr(0);
    for (unsigned long i = 0; i < argc; i++) js_arr_push(a, V(argv[i]));
    LEAVE();
    (void)exception;
    return RO(jv_obj(a));
}
JSC_EXPORT JSObjectRef JSObjectMakeDate(JSContextRef c, unsigned long n, const JSValueRef a[], JSValueRef *e) { return make_by_ctor(c, "Date", n, a, e); }
JSC_EXPORT JSObjectRef JSObjectMakeError(JSContextRef c, unsigned long n, const JSValueRef a[], JSValueRef *e) { return make_by_ctor(c, "Error", n, a, e); }
JSC_EXPORT JSObjectRef JSObjectMakeRegExp(JSContextRef c, unsigned long n, const JSValueRef a[], JSValueRef *e) { return make_by_ctor(c, "RegExp", n, a, e); }
JSC_EXPORT JSObjectRef JSObjectMakeFunction(JSContextRef ctx, JSStringRef name, unsigned paramCount, const JSStringRef params[], JSStringRef body,
                                            JSStringRef sourceURL, int line, JSValueRef *exception) {
    (void)sourceURL; (void)line;
    ENTER();
    use_ctx(ctx);
    sbuf_t src = {0};
    sb_puts(&src, "(function ");
    if (name) { int l; char *n = utf8_of(name, &l); if (n) { sb_puts(&src, n); kfree(n); } }
    sb_putc(&src, '(');
    for (unsigned i = 0; i < paramCount; i++) { int l; char *p = utf8_of(params[i], &l); if (i) sb_putc(&src, ','); if (p) { sb_puts(&src, p); kfree(p); } }
    sb_puts(&src, ") {\n");
    if (body) { int l; char *b = utf8_of(body, &l); if (b) { sb_puts(&src, b); kfree(b); } }
    sb_puts(&src, "\n})");
    jsstr *err = NULL;
    jsscript *s = js_parse(src.p, src.len, "JSObjectMakeFunction", 1, &err);
    sb_free(&src);
    JSObjectRef out = NULL;
    if (!s) { if (exception) *exception = R(jv_obj(js_make_err(E_SYNTAX, err ? err : js_strz("erro de sintaxe")))); }
    else {
        jsv r = js_run_script(s);
        if (r == JS_EXC) take_exc(exception);
        else out = RO(r);
    }
    LEAVE();
    return out;
}

JSC_EXPORT JSValueRef JSObjectGetPrototype(JSContextRef c, JSObjectRef o) { (void)c; jsobj *x = O(o); return R(x && x->proto ? jv_obj(x->proto) : JS_NULL); }
JSC_EXPORT void JSObjectSetPrototype(JSContextRef c, JSObjectRef o, JSValueRef p) {
    (void)c;
    jsobj *x = O(o);
    jsv pv = V(p);
    if (!x) return;
    if (pv == JS_NULL) x->proto = NULL;
    else if (jv_isobj(pv)) {
        for (jsobj *q = jv_o(pv); q; q = q->proto) if (q == x) return;   /* ciclo */
        x->proto = jv_o(pv);
    }
}

JSC_EXPORT bool JSObjectHasProperty(JSContextRef ctx, JSObjectRef o, JSStringRef name) {
    ENTER();
    use_ctx(ctx);
    int r = js_has(V(o), jv_str(str_to_js(name)));
    if (r < 0) { js_catch(); r = 0; }
    LEAVE();
    return r == 1;
}
JSC_EXPORT JSValueRef JSObjectGetProperty(JSContextRef ctx, JSObjectRef o, JSStringRef name, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    jsv r = js_getk(V(o), str_to_js(name));
    JSValueRef out = NULL;
    if (r == JS_EXC) take_exc(exception);
    else out = R(r);
    LEAVE();
    return out;
}
JSC_EXPORT void JSObjectSetProperty(JSContextRef ctx, JSObjectRef o, JSStringRef name, JSValueRef v, JSPropertyAttributes attr, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    jsobj *x = O(o);
    jsstr *k = str_to_js(name);
    if (x && attr && !js_ownprop(x, k)) {
        js_defprop(x, k, V(v), (attr & kJSPropertyAttributeReadOnly ? PF_RO : 0) | (attr & kJSPropertyAttributeDontEnum ? PF_NOENUM : 0) |
                               (attr & kJSPropertyAttributeDontDelete ? PF_NOCONF : 0));
    } else if (js_setk(V(o), k, V(v)) < 0) take_exc(exception);
    LEAVE();
}
JSC_EXPORT bool JSObjectDeleteProperty(JSContextRef ctx, JSObjectRef o, JSStringRef name, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    int r = js_delete(V(o), jv_str(str_to_js(name)));
    if (r < 0) { take_exc(exception); r = 0; }
    LEAVE();
    return r == 1;
}
JSC_EXPORT JSValueRef JSObjectGetPropertyAtIndex(JSContextRef ctx, JSObjectRef o, unsigned i, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    jsv r = js_getidx(V(o), i);
    JSValueRef out = NULL;
    if (r == JS_EXC) take_exc(exception);
    else out = R(r);
    LEAVE();
    return out;
}
JSC_EXPORT void JSObjectSetPropertyAtIndex(JSContextRef ctx, JSObjectRef o, unsigned i, JSValueRef v, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    if (js_setidx(V(o), i, V(v)) < 0) take_exc(exception);
    LEAVE();
}
/* chave como valor (string, número ou símbolo) */
JSC_EXPORT JSValueRef JSObjectGetPropertyForKey(JSContextRef ctx, JSObjectRef o, JSValueRef key, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    jsv r = js_get(V(o), V(key));
    JSValueRef out = NULL;
    if (r == JS_EXC) take_exc(exception);
    else out = R(r);
    LEAVE();
    return out;
}
JSC_EXPORT void JSObjectSetPropertyForKey(JSContextRef ctx, JSObjectRef o, JSValueRef key, JSValueRef v, JSPropertyAttributes attr, JSValueRef *exception) {
    (void)attr;
    ENTER();
    use_ctx(ctx);
    if (js_set(V(o), V(key), V(v)) < 0) take_exc(exception);
    LEAVE();
}
JSC_EXPORT bool JSObjectHasPropertyForKey(JSContextRef ctx, JSObjectRef o, JSValueRef key, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    int r = js_has(V(o), V(key));
    if (r < 0) { take_exc(exception); r = 0; }
    LEAVE();
    return r == 1;
}
JSC_EXPORT bool JSObjectDeletePropertyForKey(JSContextRef ctx, JSObjectRef o, JSValueRef key, JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    int r = js_delete(V(o), V(key));
    if (r < 0) { take_exc(exception); r = 0; }
    LEAVE();
    return r == 1;
}

JSC_EXPORT bool JSObjectIsFunction(JSContextRef c, JSObjectRef o) { (void)c; return o && js_iscallable(V(o)); }
JSC_EXPORT bool JSObjectIsConstructor(JSContextRef c, JSObjectRef o) { (void)c; return o && js_isctor(V(o)); }
JSC_EXPORT JSValueRef JSObjectCallAsFunction(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, unsigned long argc,
                                             const JSValueRef argv[], JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    jsv sa[16];
    jsv *a = argc <= 16 ? sa : kmalloc(sizeof(jsv) * argc);
    for (unsigned long i = 0; a && i < argc; i++) a[i] = V(argv[i]);
    jsv r = a ? js_call(V(f), thisObject ? V(thisObject) : jv_obj(J.realm->global), (int)argc, a) : JS_EXC;
    if (a != sa) kfree(a);
    JSValueRef out = NULL;
    if (r == JS_EXC) take_exc(exception);
    else out = R(r);
    js_run_jobs();
    if (js_pending()) js_catch();
    LEAVE();
    return out;
}
JSC_EXPORT JSObjectRef JSObjectCallAsConstructor(JSContextRef ctx, JSObjectRef f, unsigned long argc, const JSValueRef argv[], JSValueRef *exception) {
    ENTER();
    use_ctx(ctx);
    jsv sa[16];
    jsv *a = argc <= 16 ? sa : kmalloc(sizeof(jsv) * argc);
    for (unsigned long i = 0; a && i < argc; i++) a[i] = V(argv[i]);
    jsv r = a ? js_construct(V(f), (int)argc, a, V(f)) : JS_EXC;
    if (a != sa) kfree(a);
    JSObjectRef out = NULL;
    if (r == JS_EXC) take_exc(exception);
    else out = RO(r);
    LEAVE();
    return out;
}

JSC_EXPORT JSPropertyNameArrayRef JSObjectCopyPropertyNames(JSContextRef ctx, JSObjectRef o) {
    ENTER();
    use_ctx(ctx);
    jsobj *x = O(o);
    JSPropertyNameArrayRef a = kcalloc(1, sizeof(struct OpaqueJSPropertyNameArray));
    a->rc = 1;
    jsobj *keys = x ? js_keylist(x, true, true) : NULL;
    if (keys && keys->alen) {
        a->names = kcalloc(keys->alen, sizeof(JSStringRef));
        for (u32 i = 0; a->names && i < keys->alen; i++) a->names[a->n++] = str_from_js(jv_s(keys->arr[i]));
    }
    LEAVE();
    return a;
}
JSC_EXPORT JSPropertyNameArrayRef JSPropertyNameArrayRetain(JSPropertyNameArrayRef a) { if (a) a->rc++; return a; }
JSC_EXPORT void JSPropertyNameArrayRelease(JSPropertyNameArrayRef a) {
    if (!a || --a->rc) return;
    for (int i = 0; i < a->n; i++) JSStringRelease(a->names[i]);
    kfree(a->names);
    kfree(a);
}
JSC_EXPORT unsigned long JSPropertyNameArrayGetCount(JSPropertyNameArrayRef a) { return a ? (unsigned long)a->n : 0; }
JSC_EXPORT JSStringRef JSPropertyNameArrayGetNameAtIndex(JSPropertyNameArrayRef a, unsigned long i) { return a && i < (unsigned long)a->n ? a->names[i] : NULL; }
JSC_EXPORT void JSPropertyNameAccumulatorAddName(JSPropertyNameAccumulatorRef acc, JSStringRef name) {
    if (acc && acc->out && name) js_arr_push(acc->out, jv_str(str_to_js(name)));
}

/* ---------------------------------------------------------------- typed arrays */
static int to_kind(JSTypedArrayType t) {
    switch (t) {
    case kJSTypedArrayTypeInt8Array: return TA_INT8;
    case kJSTypedArrayTypeInt16Array: return TA_INT16;
    case kJSTypedArrayTypeInt32Array: return TA_INT32;
    case kJSTypedArrayTypeUint8Array: return TA_UINT8;
    case kJSTypedArrayTypeUint8ClampedArray: return TA_UINT8C;
    case kJSTypedArrayTypeUint16Array: return TA_UINT16;
    case kJSTypedArrayTypeUint32Array: return TA_UINT32;
    case kJSTypedArrayTypeFloat32Array: return TA_FLOAT32;
    case kJSTypedArrayTypeFloat64Array: return TA_FLOAT64;
    case kJSTypedArrayTypeBigInt64Array: return TA_BIGINT64;
    case kJSTypedArrayTypeBigUint64Array: return TA_BIGUINT64;
    default: return -1;
    }
}
static JSTypedArrayType from_kind(int k) {
    static const JSTypedArrayType m[TA_COUNT] = {kJSTypedArrayTypeInt8Array, kJSTypedArrayTypeUint8Array, kJSTypedArrayTypeUint8ClampedArray,
        kJSTypedArrayTypeInt16Array, kJSTypedArrayTypeUint16Array, kJSTypedArrayTypeInt32Array, kJSTypedArrayTypeUint32Array,
        kJSTypedArrayTypeFloat32Array, kJSTypedArrayTypeFloat64Array, kJSTypedArrayTypeBigInt64Array, kJSTypedArrayTypeBigUint64Array};
    return k >= 0 && k < TA_COUNT ? m[k] : kJSTypedArrayTypeNone;
}
static JSValueRef type_err(JSValueRef *exception, const char *m) {
    if (exception) *exception = R(jv_obj(js_make_err(E_TYPE, js_strz(m))));
    return NULL;
}

JSC_EXPORT JSTypedArrayType JSValueGetTypedArrayType(JSContextRef c, JSValueRef v, JSValueRef *e) {
    (void)c; (void)e;
    jsv x = V(v);
    if (jv_iscls(x, CL_TYPED)) return from_kind(jv_o(x)->u.ta.kind);
    if (jv_iscls(x, CL_ARRAYBUFFER)) return kJSTypedArrayTypeArrayBuffer;
    return kJSTypedArrayTypeNone;
}
JSC_EXPORT JSObjectRef JSObjectMakeTypedArray(JSContextRef ctx, JSTypedArrayType t, unsigned long len, JSValueRef *exception) {
    int k = to_kind(t);
    if (k < 0) return (JSObjectRef)type_err(exception, "tipo de typed array inválido");
    ENTER();
    use_ctx(ctx);
    jsobj *b = js_abuf_new((u32)(len * (unsigned long)js_typed_elsize(k)));
    JSObjectRef out = NULL;
    if (!b) take_exc(exception);
    else out = RO(jv_obj(js_typed_new(k, b, 0, (u32)len)));
    LEAVE();
    return out;
}
JSC_EXPORT JSObjectRef JSObjectMakeTypedArrayWithBytesNoCopy(JSContextRef ctx, JSTypedArrayType t, void *bytes, unsigned long byteLength,
                                                             JSTypedArrayBytesDeallocator dealloc, void *dctx, JSValueRef *exception) {
    int k = to_kind(t);
    if (k < 0) return (JSObjectRef)type_err(exception, "tipo de typed array inválido");
    ENTER();
    use_ctx(ctx);
    jsobj *b = js_abuf_external(bytes, (u32)byteLength, dealloc, dctx);
    if (!dealloc) b->u.buf.detached = false;
    u32 n = (u32)(byteLength / (unsigned long)js_typed_elsize(k));
    JSObjectRef out = RO(jv_obj(js_typed_new(k, b, 0, n)));
    LEAVE();
    return out;
}
JSC_EXPORT JSObjectRef JSObjectMakeArrayBufferWithBytesNoCopy(JSContextRef ctx, void *bytes, unsigned long byteLength,
                                                              JSTypedArrayBytesDeallocator dealloc, void *dctx, JSValueRef *exception) {
    (void)exception;
    ENTER();
    use_ctx(ctx);
    JSObjectRef out = RO(jv_obj(js_abuf_external(bytes, (u32)byteLength, dealloc, dctx)));
    LEAVE();
    return out;
}
JSC_EXPORT JSObjectRef JSObjectMakeTypedArrayWithArrayBuffer(JSContextRef ctx, JSTypedArrayType t, JSObjectRef buffer, JSValueRef *exception) {
    jsobj *b = O(buffer);
    int k = to_kind(t);
    if (k < 0 || !b || b->cls != CL_ARRAYBUFFER) return (JSObjectRef)type_err(exception, "precisa de um ArrayBuffer e de um tipo válido");
    ENTER();
    use_ctx(ctx);
    JSObjectRef out = RO(jv_obj(js_typed_new(k, b, 0, b->u.buf.len / (u32)js_typed_elsize(k))));
    LEAVE();
    return out;
}
JSC_EXPORT JSObjectRef JSObjectMakeTypedArrayWithArrayBufferAndOffset(JSContextRef ctx, JSTypedArrayType t, JSObjectRef buffer, unsigned long off,
                                                                      unsigned long len, JSValueRef *exception) {
    jsobj *b = O(buffer);
    int k = to_kind(t);
    if (k < 0 || !b || b->cls != CL_ARRAYBUFFER) return (JSObjectRef)type_err(exception, "precisa de um ArrayBuffer e de um tipo válido");
    if (off % (unsigned long)js_typed_elsize(k) || off + len * (unsigned long)js_typed_elsize(k) > b->u.buf.len) {
        if (exception) *exception = R(jv_obj(js_make_err(E_RANGE, js_strz("deslocamento ou tamanho fora do ArrayBuffer"))));
        return NULL;
    }
    ENTER();
    use_ctx(ctx);
    JSObjectRef out = RO(jv_obj(js_typed_new(k, b, (u32)off, (u32)len)));
    LEAVE();
    return out;
}
static jsobj *ta_of(JSObjectRef o) { jsobj *x = O(o); return x && x->cls == CL_TYPED ? x : NULL; }
JSC_EXPORT void *JSObjectGetTypedArrayBytesPtr(JSContextRef c, JSObjectRef o, JSValueRef *e) {
    (void)c; (void)e;
    jsobj *t = ta_of(o);
    if (!t || !t->u.ta.buffer || t->u.ta.buffer->u.buf.detached) return NULL;
    return t->u.ta.buffer->u.buf.data;   /* o início do buffer: o JSC devolve o do buffer e o app soma o byteOffset */
}
JSC_EXPORT unsigned long JSObjectGetTypedArrayLength(JSContextRef c, JSObjectRef o, JSValueRef *e) { (void)c; (void)e; jsobj *t = ta_of(o); return t ? js_typed_length(t) : 0; }
JSC_EXPORT unsigned long JSObjectGetTypedArrayByteLength(JSContextRef c, JSObjectRef o, JSValueRef *e) {
    (void)c; (void)e;
    jsobj *t = ta_of(o);
    return t ? (unsigned long)js_typed_length(t) * (unsigned long)js_typed_elsize(t->u.ta.kind) : 0;
}
JSC_EXPORT unsigned long JSObjectGetTypedArrayByteOffset(JSContextRef c, JSObjectRef o, JSValueRef *e) { (void)c; (void)e; jsobj *t = ta_of(o); return t ? t->u.ta.off : 0; }
JSC_EXPORT JSObjectRef JSObjectGetTypedArrayBuffer(JSContextRef c, JSObjectRef o, JSValueRef *e) { (void)c; (void)e; jsobj *t = ta_of(o); return t ? RO(jv_obj(t->u.ta.buffer)) : NULL; }
JSC_EXPORT void *JSObjectGetArrayBufferBytesPtr(JSContextRef c, JSObjectRef o, JSValueRef *e) {
    (void)c; (void)e;
    jsobj *b = O(o);
    return b && b->cls == CL_ARRAYBUFFER && !b->u.buf.detached ? b->u.buf.data : NULL;
}
JSC_EXPORT unsigned long JSObjectGetArrayBufferByteLength(JSContextRef c, JSObjectRef o, JSValueRef *e) {
    (void)c; (void)e;
    jsobj *b = O(o);
    return b && b->cls == CL_ARRAYBUFFER && !b->u.buf.detached ? b->u.buf.len : 0;
}
