/*
 * mOS — libSystem: XPC (libxpc). O modelo de objetos inteiro e as conexões.
 *
 * Por quê (tools/machocheck.c, docs/compat-apps.md): VS Code, Cursor, Claude, Slack e Grok Bot importam ~50 funções
 * xpc_* (o Chromium e o Crashpad montam dicionários XPC e abrem conexões com serviços Mach) e o mOS não tinha
 * nenhuma; o checklist de compatibilidade (Mach IPC + XPC) também pede.
 *
 * O que é real:
 *  - objetos: null, bool, int64, uint64, double, date, data, string, uuid, array, dictionary, error; criar, ler,
 *    copiar (xpc_copy), comparar (xpc_equal), hash, descrição, apply com blocos, retain/release. São objetos ObjC
 *    da classe OS_xpc_object (como no macOS), então objc_retain/objc_release/ARC e o Swift funcionam neles.
 *  - os singletons XPC_BOOL_TRUE/FALSE e os erros XPC_ERROR_CONNECTION_INVALID/INTERRUPTED/TERMINATION_IMMINENT
 *    (dicionários com XPC_ERROR_KEY_DESCRIPTION), estáticos e imortais.
 *  - conexões: o mOS não tem um launchd com serviços XPC registrados, então uma conexão com serviço Mach se
 *    comporta como num Mac em que o serviço não existe: depois de resume, o handler de eventos recebe
 *    XPC_ERROR_CONNECTION_INVALID; send_message_with_reply entrega o mesmo erro ao bloco de resposta;
 *    send_message_with_reply_sync o devolve; barreiras rodam; cancel entrega INVALID uma vez. Quem trata a
 *    falha (todo cliente XPC precisa) segue em frente. Conexões anônimas/de ouvinte idem.
 *  - xpc_main não volta (o mOS não lança serviços XPC: o processo sai); xpc_pipe_* (API privada do launchd)
 *    devolve EINVAL; xpc_dictionary_copy_mach_send devolve MACH_PORT_NULL.
 * A entrega usa dispatch_async_f na fila da conexão (a libdispatch do mOS ainda roda na hora, na thread de quem
 * chama: dispatch.c).
 */
#include "foundation.h"
#include "libc.h"

typedef unsigned char Boolean;
char *strdup(const char *s);
int strncmp(const char *a, const char *b, size_t n);
void *realloc(void *p, size_t n);
void *calloc(size_t a, size_t b);
cx_class_t *objc_allocateClassPair(cx_class_t *super, const char *name, u64 extra);
void objc_registerClassPair(cx_class_t *c);
BOOL class_addIvar(cx_class_t *c, const char *name, u64 size, u8 align, const char *types);
BOOL class_addMethod(cx_class_t *c, SEL s, IMP imp, const char *types);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
id class_createInstance(cx_class_t *c, u64 extra);
id objc_retain(id o);
void objc_release(id o);
void _Block_release(const void *b);
void *dispatch_get_global_queue(long priority, unsigned long flags);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
int vsnprintf(char *b, size_t n, const char *fmt, va_list ap);
int gettimeofday(mac_tv_t *tv, void *tz);

typedef struct { void *isa; int flags, reserved; void *invoke; } blk_t;
#define CALL_BLOCK1(b, a) ((void (*)(void *, void *))((blk_t *)(b))->invoke)((b), (a))

/* ---------------------------------------------------------------- tipos (XPC_TYPE_* = &_xpc_type_*) */
struct _xpc_type_s { const char *name; };
typedef const struct _xpc_type_s *xpc_type_t;
#define T(n) const struct _xpc_type_s _xpc_type_##n = { #n };
T(connection) T(endpoint) T(null) T(bool) T(int64) T(uint64) T(double) T(date) T(data) T(string) T(uuid) T(fd)
T(shmem) T(array) T(dictionary) T(error) T(activity) T(mach_send) T(mach_recv) T(rich_error) T(session)
#undef T

typedef struct xo xo_t;
typedef struct {
    char *name;
    void *queue;
    void *handler;                       /* bloco de eventos (_Block_copy) */
    void *context;
    void (*finalizer)(void *);
    int resumed, invalid_sent, canceled;
} xconn_t;
struct xo {
    cx_class_t *isa;
    xpc_type_t type;
    union {
        i64 i;
        u64 u;
        double d;
        Boolean b;
        struct { u8 *p; size_t n; } data;
        struct { char *s; size_t n; } str;
        u8 uuid[16];
        struct { xo_t **v; size_t n, cap; } arr;
        struct { char **k; xo_t **v; size_t n, cap; } dict;
        xconn_t *conn;
        int fd;
    } u;
};
typedef xo_t *xpc_object_t;
typedef xo_t *xpc_connection_t;

/* ---------------------------------------------------------------- a classe e os singletons */
static cx_class_t *xcls;
static IMP nsobject_dealloc;
static void xo_free_contents(xo_t *o);
static id xo_description(id self, SEL c);

static void xo_dealloc(id self, SEL c) {
    xo_free_contents((xo_t *)self);
    if (nsobject_dealloc) ((void (*)(id, SEL))nsobject_dealloc)(self, c);
}
static cx_class_t *xclass(void) {
    if (xcls) return xcls;
    cx_class_t *ns = cx_class_named("NSObject");
    nsobject_dealloc = ns ? class_getMethodImplementation(ns, sel_registerName("dealloc")) : 0;
    cx_class_t *k = objc_allocateClassPair(ns, "OS_xpc_object", 0);
    if (!k) k = cx_class_named("OS_xpc_object");
    class_addIvar(k, "_xpc", sizeof(xo_t) - 8, 3, "?");
    class_addMethod(k, sel_registerName("dealloc"), (IMP)xo_dealloc, "v@:");
    class_addMethod(k, sel_registerName("description"), (IMP)xo_description, "@@:");
    class_addMethod(k, sel_registerName("debugDescription"), (IMP)xo_description, "@@:");
    objc_registerClassPair(k);
    xcls = k;
    return k;
}
static xo_t *xnew(xpc_type_t t) {
    xo_t *o = (xo_t *)class_createInstance(xclass(), 0);
    if (!o) return 0;
    memset((u8 *)o + 8, 0, sizeof *o - 8);
    o->type = t;
    return o;
}

/* estáticos (sem cabeçalho do runtime: objc_retain/release não mexem neles, são imortais). O isa é posto na
 * primeira chamada a qualquer função xpc_ (fix_statics). */
xo_t _xpc_bool_true = { 0, &_xpc_type_bool, { .b = 1 } };
xo_t _xpc_bool_false = { 0, &_xpc_type_bool, { .b = 0 } };
static xo_t x_null = { 0, &_xpc_type_null, { 0 } };
const char *const _xpc_error_key_description = "XPCErrorDescription";
const char *const _xpc_event_key_name = "XPCEventName";
#define ERRSTR(var, text) static xo_t var = { 0, &_xpc_type_string, { .str = { (char *)text, sizeof text - 1 } } }
ERRSTR(es_invalid, "Connection invalid");
ERRSTR(es_interrupted, "Connection interrupted");
ERRSTR(es_imminent, "Process will terminate imminently");
static char *ek[] = { (char *)"XPCErrorDescription" };
static xo_t *ev_invalid[] = { &es_invalid }, *ev_interrupted[] = { &es_interrupted }, *ev_imminent[] = { &es_imminent };
#define ERRDICT(var, vals) xo_t var = { 0, &_xpc_type_error, { .dict = { ek, vals, 1, 1 } } }
ERRDICT(_xpc_error_connection_invalid, ev_invalid);
ERRDICT(_xpc_error_connection_interrupted, ev_interrupted);
ERRDICT(_xpc_error_termination_imminent, ev_imminent);
#define XPC_ERROR_CONNECTION_INVALID (&_xpc_error_connection_invalid)

static int statics_ok;
static void fix_statics(void) {
    if (statics_ok) return;
    cx_class_t *k = xclass();
    xo_t *all[] = { &_xpc_bool_true, &_xpc_bool_false, &x_null, &es_invalid, &es_interrupted, &es_imminent,
                    &_xpc_error_connection_invalid, &_xpc_error_connection_interrupted, &_xpc_error_termination_imminent };
    for (size_t i = 0; i < sizeof all / sizeof all[0]; i++) all[i]->isa = k;
    statics_ok = 1;
}
static int is_static(const xo_t *o) {
    return o == &_xpc_bool_true || o == &_xpc_bool_false || o == &x_null || o == &es_invalid || o == &es_interrupted ||
           o == &es_imminent || o == &_xpc_error_connection_invalid || o == &_xpc_error_connection_interrupted ||
           o == &_xpc_error_termination_imminent;
}

static void xo_free_contents(xo_t *o) {
    if (!o || is_static(o)) return;
    if (o->type == &_xpc_type_data) free(o->u.data.p);
    else if (o->type == &_xpc_type_string) free(o->u.str.s);
    else if (o->type == &_xpc_type_array) {
        for (size_t i = 0; i < o->u.arr.n; i++) objc_release((id)o->u.arr.v[i]);
        free(o->u.arr.v);
    } else if (o->type == &_xpc_type_dictionary || o->type == &_xpc_type_error) {
        for (size_t i = 0; i < o->u.dict.n; i++) { free(o->u.dict.k[i]); objc_release((id)o->u.dict.v[i]); }
        free(o->u.dict.k); free(o->u.dict.v);
    } else if (o->type == &_xpc_type_connection && o->u.conn) {
        xconn_t *c = o->u.conn;
        if (c->finalizer && c->context) c->finalizer(c->context);
        if (c->handler) _Block_release(c->handler);
        free(c->name);
        free(c);
    }
    memset(&o->u, 0, sizeof o->u);
}

/* ---------------------------------------------------------------- genéricos */
xpc_object_t xpc_retain(xpc_object_t o) { if (o) objc_retain((id)o); return o; }
void xpc_release(xpc_object_t o) { if (o) objc_release((id)o); }
xpc_type_t xpc_get_type(xpc_object_t o) { fix_statics(); return o ? o->type : 0; }
const char *xpc_type_get_name(xpc_type_t t) { return t ? t->name : "<null>"; }

xpc_object_t xpc_null_create(void) { fix_statics(); return &x_null; }
xpc_object_t xpc_bool_create(Boolean v) { fix_statics(); return v ? &_xpc_bool_true : &_xpc_bool_false; }
Boolean xpc_bool_get_value(xpc_object_t o) { return o && o->type == &_xpc_type_bool ? o->u.b : 0; }
#define NUM(name, field, ctype, tname)                                                                       \
    xpc_object_t xpc_##name##_create(ctype v) { fix_statics(); xo_t *o = xnew(&_xpc_type_##tname); if (o) o->u.field = v; return o; } \
    ctype xpc_##name##_get_value(xpc_object_t o) { return o && o->type == &_xpc_type_##tname ? o->u.field : 0; }
NUM(int64, i, i64, int64)
NUM(uint64, u, u64, uint64)
NUM(double, d, double, double)
NUM(date, i, i64, date)
#undef NUM
xpc_object_t xpc_date_create_from_current(void) {
    mac_tv_t tv;
    gettimeofday(&tv, 0);
    return xpc_date_create((i64)tv.sec * 1000000000LL + (i64)tv.usec * 1000);
}
xpc_object_t xpc_data_create(const void *p, size_t n) {
    fix_statics();
    xo_t *o = xnew(&_xpc_type_data);
    if (!o) return 0;
    o->u.data.p = malloc(n ? n : 1);
    if (n && p) memcpy(o->u.data.p, p, n);
    o->u.data.n = n;
    return o;
}
xpc_object_t xpc_data_create_with_dispatch_data(void *dd) { (void)dd; return xpc_data_create(0, 0); }
size_t xpc_data_get_length(xpc_object_t o) { return o && o->type == &_xpc_type_data ? o->u.data.n : 0; }
const void *xpc_data_get_bytes_ptr(xpc_object_t o) { return o && o->type == &_xpc_type_data ? o->u.data.p : 0; }
size_t xpc_data_get_bytes(xpc_object_t o, void *buf, size_t off, size_t len) {
    if (!o || o->type != &_xpc_type_data || off >= o->u.data.n) return 0;
    size_t k = o->u.data.n - off < len ? o->u.data.n - off : len;
    memcpy(buf, o->u.data.p + off, k);
    return k;
}
static xo_t *str_new(const char *s, size_t n) {
    fix_statics();
    xo_t *o = xnew(&_xpc_type_string);
    if (!o) return 0;
    o->u.str.s = malloc(n + 1);
    memcpy(o->u.str.s, s, n);
    o->u.str.s[n] = 0;
    o->u.str.n = n;
    return o;
}
xpc_object_t xpc_string_create(const char *s) { return s ? str_new(s, strlen(s)) : 0; }
xpc_object_t xpc_string_create_with_format(const char *fmt, ...) {
    char *out = 0;
    va_list ap;
    va_start(ap, fmt);
    int n = vasprintf(&out, fmt, ap);
    va_end(ap);
    xo_t *o = n >= 0 && out ? str_new(out, (size_t)n) : 0;
    free(out);
    return o;
}
size_t xpc_string_get_length(xpc_object_t o) { return o && o->type == &_xpc_type_string ? o->u.str.n : 0; }
const char *xpc_string_get_string_ptr(xpc_object_t o) { return o && o->type == &_xpc_type_string ? o->u.str.s : 0; }
xpc_object_t xpc_uuid_create(const u8 *uuid) {
    fix_statics();
    xo_t *o = xnew(&_xpc_type_uuid);
    if (o && uuid) memcpy(o->u.uuid, uuid, 16);
    return o;
}
const u8 *xpc_uuid_get_bytes(xpc_object_t o) { return o && o->type == &_xpc_type_uuid ? o->u.uuid : 0; }
int dup(int fd);
xpc_object_t xpc_fd_create(int fd) {
    fix_statics();
    int d = dup(fd);
    if (d < 0) return 0;
    xo_t *o = xnew(&_xpc_type_fd);
    if (o) o->u.fd = d;
    return o;
}
int xpc_fd_dup(xpc_object_t o) { return o && o->type == &_xpc_type_fd ? dup(o->u.fd) : -1; }

/* ---------------------------------------------------------------- array */
static void arr_push(xo_t *a, xo_t *v) {
    if (a->u.arr.n == a->u.arr.cap) {
        size_t cap = a->u.arr.cap ? a->u.arr.cap * 2 : 8;
        a->u.arr.v = realloc(a->u.arr.v, cap * sizeof(xo_t *));
        a->u.arr.cap = cap;
    }
    a->u.arr.v[a->u.arr.n++] = (xo_t *)objc_retain((id)v);
}
xpc_object_t xpc_array_create(xpc_object_t const *objs, size_t n) {
    fix_statics();
    xo_t *a = xnew(&_xpc_type_array);
    for (size_t i = 0; a && objs && i < n; i++) if (objs[i]) arr_push(a, objs[i]);
    return a;
}
xpc_object_t xpc_array_create_empty(void) { return xpc_array_create(0, 0); }
#define IS_ARR(a) ((a) && (a)->type == &_xpc_type_array)
size_t xpc_array_get_count(xpc_object_t a) { return IS_ARR(a) ? a->u.arr.n : 0; }
xpc_object_t xpc_array_get_value(xpc_object_t a, size_t i) { return IS_ARR(a) && i < a->u.arr.n ? a->u.arr.v[i] : 0; }
void xpc_array_append_value(xpc_object_t a, xpc_object_t v) { if (IS_ARR(a) && v) arr_push(a, v); }
#define XPC_ARRAY_APPEND ((size_t)-1)
void xpc_array_set_value(xpc_object_t a, size_t i, xpc_object_t v) {
    if (!IS_ARR(a) || !v) return;
    if (i == XPC_ARRAY_APPEND || i == a->u.arr.n) { arr_push(a, v); return; }
    if (i >= a->u.arr.n) return;
    objc_retain((id)v);
    objc_release((id)a->u.arr.v[i]);
    a->u.arr.v[i] = v;
}
Boolean xpc_array_apply(xpc_object_t a, void *block) {
    if (!IS_ARR(a) || !block) return 1;
    for (size_t i = 0; i < a->u.arr.n; i++)
        if (!((Boolean (*)(void *, size_t, xo_t *))((blk_t *)block)->invoke)(block, i, a->u.arr.v[i])) return 0;
    return 1;
}
#define ARR_GET(name, ctype, getter)                                                                          \
    ctype xpc_array_get_##name(xpc_object_t a, size_t i) { return getter(xpc_array_get_value(a, i)); }
#define ARR_SET(name, ctype, maker)                                                                           \
    void xpc_array_set_##name(xpc_object_t a, size_t i, ctype v) { xo_t *o = maker(v); xpc_array_set_value(a, i, o); xpc_release(o); }
ARR_GET(bool, Boolean, xpc_bool_get_value) ARR_SET(bool, Boolean, xpc_bool_create)
ARR_GET(int64, i64, xpc_int64_get_value) ARR_SET(int64, i64, xpc_int64_create)
ARR_GET(uint64, u64, xpc_uint64_get_value) ARR_SET(uint64, u64, xpc_uint64_create)
ARR_GET(double, double, xpc_double_get_value) ARR_SET(double, double, xpc_double_create)
ARR_GET(date, i64, xpc_date_get_value) ARR_SET(date, i64, xpc_date_create)
ARR_GET(string, const char *, xpc_string_get_string_ptr) ARR_SET(string, const char *, xpc_string_create)
ARR_GET(uuid, const u8 *, xpc_uuid_get_bytes) ARR_SET(uuid, const u8 *, xpc_uuid_create)
const void *xpc_array_get_data(xpc_object_t a, size_t i, size_t *len) {
    xo_t *o = xpc_array_get_value(a, i);
    if (len) *len = xpc_data_get_length(o);
    return xpc_data_get_bytes_ptr(o);
}
void xpc_array_set_data(xpc_object_t a, size_t i, const void *p, size_t n) { xo_t *o = xpc_data_create(p, n); xpc_array_set_value(a, i, o); xpc_release(o); }
xpc_object_t xpc_array_get_array(xpc_object_t a, size_t i) { xo_t *o = xpc_array_get_value(a, i); return IS_ARR(o) ? o : 0; }
xpc_object_t xpc_array_get_dictionary(xpc_object_t a, size_t i) { xo_t *o = xpc_array_get_value(a, i); return o && o->type == &_xpc_type_dictionary ? o : 0; }

/* ---------------------------------------------------------------- dicionário */
#define IS_DICT(d) ((d) && ((d)->type == &_xpc_type_dictionary || (d)->type == &_xpc_type_error))
static long dict_find(xo_t *d, const char *k) {
    for (size_t i = 0; i < d->u.dict.n; i++) if (!strcmp(d->u.dict.k[i], k)) return (long)i;
    return -1;
}
xpc_object_t xpc_dictionary_create(const char *const *keys, xpc_object_t const *vals, size_t n);
void xpc_dictionary_set_value(xpc_object_t d, const char *k, xpc_object_t v) {
    if (!IS_DICT(d) || !k || is_static(d)) return;
    long i = dict_find(d, k);
    if (!v) {                                         /* valor NULL apaga a chave */
        if (i < 0) return;
        free(d->u.dict.k[i]);
        objc_release((id)d->u.dict.v[i]);
        d->u.dict.n--;
        for (size_t j = (size_t)i; j < d->u.dict.n; j++) { d->u.dict.k[j] = d->u.dict.k[j + 1]; d->u.dict.v[j] = d->u.dict.v[j + 1]; }
        return;
    }
    objc_retain((id)v);
    if (i >= 0) { objc_release((id)d->u.dict.v[i]); d->u.dict.v[i] = v; return; }
    if (d->u.dict.n == d->u.dict.cap) {
        size_t cap = d->u.dict.cap ? d->u.dict.cap * 2 : 8;
        d->u.dict.k = realloc(d->u.dict.k, cap * sizeof(char *));
        d->u.dict.v = realloc(d->u.dict.v, cap * sizeof(xo_t *));
        d->u.dict.cap = cap;
    }
    d->u.dict.k[d->u.dict.n] = strdup(k);
    d->u.dict.v[d->u.dict.n++] = v;
}
xpc_object_t xpc_dictionary_create(const char *const *keys, xpc_object_t const *vals, size_t n) {
    fix_statics();
    xo_t *d = xnew(&_xpc_type_dictionary);
    for (size_t i = 0; d && keys && vals && i < n; i++) xpc_dictionary_set_value(d, keys[i], vals[i]);
    return d;
}
xpc_object_t xpc_dictionary_create_empty(void) { return xpc_dictionary_create(0, 0, 0); }
xpc_object_t xpc_dictionary_get_value(xpc_object_t d, const char *k) {
    fix_statics();
    if (!IS_DICT(d) || !k) return 0;
    long i = dict_find(d, k);
    return i < 0 ? 0 : d->u.dict.v[i];
}
size_t xpc_dictionary_get_count(xpc_object_t d) { return IS_DICT(d) ? d->u.dict.n : 0; }
Boolean xpc_dictionary_apply(xpc_object_t d, void *block) {
    if (!IS_DICT(d) || !block) return 1;
    for (size_t i = 0; i < d->u.dict.n; i++)
        if (!((Boolean (*)(void *, const char *, xo_t *))((blk_t *)block)->invoke)(block, d->u.dict.k[i], d->u.dict.v[i])) return 0;
    return 1;
}
#define DICT_GET(name, ctype, getter)                                                                         \
    ctype xpc_dictionary_get_##name(xpc_object_t d, const char *k) { return getter(xpc_dictionary_get_value(d, k)); }
#define DICT_SET(name, ctype, maker)                                                                          \
    void xpc_dictionary_set_##name(xpc_object_t d, const char *k, ctype v) { xo_t *o = maker(v); xpc_dictionary_set_value(d, k, o); xpc_release(o); }
DICT_GET(bool, Boolean, xpc_bool_get_value) DICT_SET(bool, Boolean, xpc_bool_create)
DICT_GET(int64, i64, xpc_int64_get_value) DICT_SET(int64, i64, xpc_int64_create)
DICT_GET(uint64, u64, xpc_uint64_get_value) DICT_SET(uint64, u64, xpc_uint64_create)
DICT_GET(double, double, xpc_double_get_value) DICT_SET(double, double, xpc_double_create)
DICT_GET(date, i64, xpc_date_get_value) DICT_SET(date, i64, xpc_date_create)
DICT_GET(string, const char *, xpc_string_get_string_ptr) DICT_SET(string, const char *, xpc_string_create)
DICT_GET(uuid, const u8 *, xpc_uuid_get_bytes) DICT_SET(uuid, const u8 *, xpc_uuid_create)
const void *xpc_dictionary_get_data(xpc_object_t d, const char *k, size_t *len) {
    xo_t *o = xpc_dictionary_get_value(d, k);
    if (len) *len = xpc_data_get_length(o);
    return xpc_data_get_bytes_ptr(o);
}
void xpc_dictionary_set_data(xpc_object_t d, const char *k, const void *p, size_t n) { xo_t *o = xpc_data_create(p, n); xpc_dictionary_set_value(d, k, o); xpc_release(o); }
void xpc_dictionary_set_fd(xpc_object_t d, const char *k, int fd) { xo_t *o = xpc_fd_create(fd); if (o) { xpc_dictionary_set_value(d, k, o); xpc_release(o); } }
int xpc_dictionary_dup_fd(xpc_object_t d, const char *k) { return xpc_fd_dup(xpc_dictionary_get_value(d, k)); }
xpc_object_t xpc_dictionary_get_dictionary(xpc_object_t d, const char *k) { xo_t *o = xpc_dictionary_get_value(d, k); return o && o->type == &_xpc_type_dictionary ? o : 0; }
xpc_object_t xpc_dictionary_get_array(xpc_object_t d, const char *k) { xo_t *o = xpc_dictionary_get_value(d, k); return IS_ARR(o) ? o : 0; }
/* direitos Mach por XPC: sem launchd/serviços, nenhuma mensagem traz um */
unsigned xpc_dictionary_copy_mach_send(xpc_object_t d, const char *k) { (void)d; (void)k; return 0; }
void xpc_dictionary_set_mach_send(xpc_object_t d, const char *k, unsigned port) { (void)d; (void)k; (void)port; }
/* só mensagens recebidas de um par têm resposta; aqui nenhuma chega de fora */
xpc_object_t xpc_dictionary_create_reply(xpc_object_t original) { (void)original; return 0; }
xpc_connection_t xpc_dictionary_get_remote_connection(xpc_object_t d) { (void)d; return 0; }

/* ---------------------------------------------------------------- cópia, igualdade, hash, descrição */
xpc_object_t xpc_copy(xpc_object_t o) {
    fix_statics();
    if (!o) return 0;
    if (is_static(o) || o->type == &_xpc_type_connection || o->type == &_xpc_type_endpoint) return xpc_retain(o);
    if (o->type == &_xpc_type_data) return xpc_data_create(o->u.data.p, o->u.data.n);
    if (o->type == &_xpc_type_string) return str_new(o->u.str.s, o->u.str.n);
    if (o->type == &_xpc_type_fd) return xpc_fd_create(o->u.fd);
    if (o->type == &_xpc_type_array) {
        xo_t *a = xpc_array_create(0, 0);
        for (size_t i = 0; i < o->u.arr.n; i++) { xo_t *c = xpc_copy(o->u.arr.v[i]); arr_push(a, c); xpc_release(c); }
        return a;
    }
    if (IS_DICT(o)) {
        xo_t *d = xpc_dictionary_create(0, 0, 0);
        for (size_t i = 0; i < o->u.dict.n; i++) { xo_t *c = xpc_copy(o->u.dict.v[i]); xpc_dictionary_set_value(d, o->u.dict.k[i], c); xpc_release(c); }
        return d;
    }
    xo_t *n = xnew(o->type);
    if (n) n->u = o->u;
    return n;
}
Boolean xpc_equal(xpc_object_t a, xpc_object_t b) {
    if (a == b) return 1;
    if (!a || !b || a->type != b->type) return 0;
    xpc_type_t t = a->type;
    if (t == &_xpc_type_int64 || t == &_xpc_type_date) return a->u.i == b->u.i;
    if (t == &_xpc_type_uint64) return a->u.u == b->u.u;
    if (t == &_xpc_type_double) return a->u.d == b->u.d;
    if (t == &_xpc_type_bool) return a->u.b == b->u.b;
    if (t == &_xpc_type_null) return 1;
    if (t == &_xpc_type_uuid) return !memcmp(a->u.uuid, b->u.uuid, 16);
    if (t == &_xpc_type_data) return a->u.data.n == b->u.data.n && !memcmp(a->u.data.p, b->u.data.p, a->u.data.n);
    if (t == &_xpc_type_string) return a->u.str.n == b->u.str.n && !memcmp(a->u.str.s, b->u.str.s, a->u.str.n);
    if (t == &_xpc_type_array) {
        if (a->u.arr.n != b->u.arr.n) return 0;
        for (size_t i = 0; i < a->u.arr.n; i++) if (!xpc_equal(a->u.arr.v[i], b->u.arr.v[i])) return 0;
        return 1;
    }
    if (IS_DICT(a)) {
        if (a->u.dict.n != b->u.dict.n) return 0;
        for (size_t i = 0; i < a->u.dict.n; i++) if (!xpc_equal(a->u.dict.v[i], xpc_dictionary_get_value(b, a->u.dict.k[i]))) return 0;
        return 1;
    }
    return 0;
}
static size_t fnv(const void *p, size_t n, size_t h) {
    const u8 *b = p;
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 1099511628211ULL;
    return h;
}
size_t xpc_hash(xpc_object_t o) {
    if (!o) return 0;
    size_t h = fnv(&o->type, sizeof o->type, 1469598103934665603ULL);
    xpc_type_t t = o->type;
    if (t == &_xpc_type_data) return fnv(o->u.data.p, o->u.data.n, h);
    if (t == &_xpc_type_string) return fnv(o->u.str.s, o->u.str.n, h);
    if (t == &_xpc_type_array) { for (size_t i = 0; i < o->u.arr.n; i++) h = h * 31 + xpc_hash(o->u.arr.v[i]); return h; }
    if (IS_DICT(o)) { for (size_t i = 0; i < o->u.dict.n; i++) h += fnv(o->u.dict.k[i], strlen(o->u.dict.k[i]), 0) ^ xpc_hash(o->u.dict.v[i]); return h; }
    if (t == &_xpc_type_connection || t == &_xpc_type_endpoint) return (size_t)o;
    return fnv(&o->u, 16, h);
}

typedef struct { char *p; size_t n, cap; } sb_t;
static void sb_put(sb_t *s, const char *t, size_t n) {
    if (s->n + n + 1 > s->cap) {
        size_t cap = (s->n + n + 1) * 2;
        s->p = realloc(s->p, cap);
        s->cap = cap;
    }
    memcpy(s->p + s->n, t, n);
    s->n += n;
    s->p[s->n] = 0;
}
static void sb_str(sb_t *s, const char *t) { sb_put(s, t, strlen(t)); }
static void sb_fmt(sb_t *s, const char *fmt, ...) {
    char b[96];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (n > 0) sb_put(s, b, (size_t)n < sizeof b ? (size_t)n : sizeof b - 1);
}
static void describe(sb_t *s, xo_t *o, int depth) {
    if (!o) { sb_str(s, "<null>"); return; }
    xpc_type_t t = o->type;
    if (t == &_xpc_type_null) sb_str(s, "<null>");
    else if (t == &_xpc_type_bool) sb_str(s, o->u.b ? "<bool: true>" : "<bool: false>");
    else if (t == &_xpc_type_int64) sb_fmt(s, "<int64: %lld>", (long long)o->u.i);
    else if (t == &_xpc_type_uint64) sb_fmt(s, "<uint64: %llu>", (unsigned long long)o->u.u);
    else if (t == &_xpc_type_double) sb_fmt(s, "<double: %g>", o->u.d);
    else if (t == &_xpc_type_date) sb_fmt(s, "<date: %lld>", (long long)o->u.i);
    else if (t == &_xpc_type_data) sb_fmt(s, "<data: { length = %zu bytes }>", o->u.data.n);
    else if (t == &_xpc_type_string) { sb_str(s, "<string: \""); sb_put(s, o->u.str.s, o->u.str.n); sb_str(s, "\">"); }
    else if (t == &_xpc_type_uuid) {
        sb_str(s, "<uuid: ");
        for (int i = 0; i < 16; i++) { sb_fmt(s, "%02X", o->u.uuid[i]); if (i == 3 || i == 5 || i == 7 || i == 9) sb_str(s, "-"); }
        sb_str(s, ">");
    } else if (t == &_xpc_type_array || IS_DICT(o)) {
        int arr = t == &_xpc_type_array;
        size_t n = arr ? o->u.arr.n : o->u.dict.n;
        sb_fmt(s, "<%s: { count = %zu, contents =\n", t == &_xpc_type_error ? "error" : arr ? "array" : "dictionary", n);
        for (size_t i = 0; i < n; i++) {
            for (int d = 0; d <= depth; d++) sb_str(s, "\t");
            if (arr) sb_fmt(s, "%zu: ", i);
            else { sb_str(s, "\""); sb_str(s, o->u.dict.k[i]); sb_str(s, "\" => "); }
            describe(s, arr ? o->u.arr.v[i] : o->u.dict.v[i], depth + 1);
            sb_str(s, "\n");
        }
        for (int d = 0; d < depth; d++) sb_str(s, "\t");
        sb_str(s, "}>");
    } else if (t == &_xpc_type_connection)
        sb_fmt(s, "<connection: { name = %s }>", o->u.conn && o->u.conn->name ? o->u.conn->name : "(anonymous)");
    else sb_fmt(s, "<%s>", t ? t->name : "?");
}
char *xpc_copy_description(xpc_object_t o) {
    sb_t s = { 0, 0, 0 };
    describe(&s, o, 0);
    return s.p ? s.p : strdup("");
}
static id xo_description(id self, SEL c) {
    (void)c;
    char *d = xpc_copy_description((xo_t *)self);
    id r = cx_str(d);
    free(d);
    return r;
}

/* ---------------------------------------------------------------- conexões */
#define IS_CONN(c) ((c) && (c)->type == &_xpc_type_connection && (c)->u.conn)
static xo_t *conn_new(const char *name, void *queue) {
    fix_statics();
    xo_t *o = xnew(&_xpc_type_connection);
    if (!o) return 0;
    o->u.conn = calloc(1, sizeof(xconn_t));
    if (!o->u.conn) { objc_release((id)o); return 0; }
    o->u.conn->name = name ? strdup(name) : 0;
    o->u.conn->queue = queue;
    return o;
}
xpc_connection_t xpc_connection_create(const char *name, void *queue) { return conn_new(name, queue); }
xpc_connection_t xpc_connection_create_mach_service(const char *name, void *queue, u64 flags) { (void)flags; return conn_new(name, queue); }
xpc_connection_t xpc_connection_create_from_endpoint(xpc_object_t ep) { (void)ep; return conn_new(0, 0); }
xpc_object_t xpc_endpoint_create(xpc_connection_t c) { (void)c; fix_statics(); return xnew(&_xpc_type_endpoint); }
void xpc_connection_set_event_handler(xpc_connection_t c, void *block) {
    if (!IS_CONN(c)) return;
    if (c->u.conn->handler) _Block_release(c->u.conn->handler);
    c->u.conn->handler = block ? _Block_copy(block) : 0;
}
void xpc_connection_set_target_queue(xpc_connection_t c, void *q) { if (IS_CONN(c)) c->u.conn->queue = q; }
void xpc_connection_set_context(xpc_connection_t c, void *ctx) { if (IS_CONN(c)) c->u.conn->context = ctx; }
void *xpc_connection_get_context(xpc_connection_t c) { return IS_CONN(c) ? c->u.conn->context : 0; }
void xpc_connection_set_finalizer_f(xpc_connection_t c, void (*fn)(void *)) { if (IS_CONN(c)) c->u.conn->finalizer = fn; }
const char *xpc_connection_get_name(xpc_connection_t c) { return IS_CONN(c) ? c->u.conn->name : 0; }
int xpc_connection_get_pid(xpc_connection_t c) { (void)c; return 0; }
unsigned xpc_connection_get_euid(xpc_connection_t c) { (void)c; return 0; }
unsigned xpc_connection_get_egid(xpc_connection_t c) { (void)c; return 0; }
int xpc_connection_get_asid(xpc_connection_t c) { (void)c; return 0; }
void xpc_connection_get_audit_token(xpc_connection_t c, void *tok) { (void)c; if (tok) memset(tok, 0, 32); }
int xpc_connection_set_peer_code_signing_requirement(xpc_connection_t c, const char *req) { (void)c; (void)req; return 0; }

static void *conn_queue(xconn_t *c) { return c->queue ? c->queue : dispatch_get_global_queue(0, 0); }

typedef struct { xo_t *conn; void *block; xo_t *arg; } deliver_t;
static void deliver_fn(void *p) {
    deliver_t *d = p;
    if (d->block) CALL_BLOCK1(d->block, d->arg);
    if (d->block) _Block_release(d->block);
    objc_release((id)d->conn);
    free(d);
}
static void deliver(xo_t *c, void *block, xo_t *arg, int copy_block) {
    if (!block) return;
    deliver_t *d = malloc(sizeof *d);
    if (!d) return;
    d->conn = (xo_t *)objc_retain((id)c);
    d->block = copy_block ? _Block_copy(block) : block;
    d->arg = arg;
    dispatch_async_f(conn_queue(c->u.conn), d, deliver_fn);
}
/* o serviço não existe no mOS: o handler fica sabendo uma vez só */
static void conn_invalidate(xo_t *c) {
    xconn_t *k = c->u.conn;
    if (k->invalid_sent) return;
    k->invalid_sent = 1;
    if (k->handler) deliver(c, k->handler, XPC_ERROR_CONNECTION_INVALID, 1);
}
void xpc_connection_resume(xpc_connection_t c) {
    if (!IS_CONN(c) || c->u.conn->resumed) return;
    c->u.conn->resumed = 1;
    conn_invalidate(c);
}
void xpc_connection_activate(xpc_connection_t c) { xpc_connection_resume(c); }
void xpc_connection_suspend(xpc_connection_t c) { (void)c; }
void xpc_connection_cancel(xpc_connection_t c) {
    if (!IS_CONN(c) || c->u.conn->canceled) return;
    c->u.conn->canceled = 1;
    conn_invalidate(c);
}
void xpc_connection_send_message(xpc_connection_t c, xpc_object_t msg) { (void)c; (void)msg; }   /* sem par: descartada */
void xpc_connection_send_barrier(xpc_connection_t c, void *block) {
    if (!IS_CONN(c) || !block) return;
    deliver_t *d = malloc(sizeof *d);
    if (!d) return;
    d->conn = (xo_t *)objc_retain((id)c);
    d->block = _Block_copy(block);
    d->arg = 0;
    dispatch_async_f(conn_queue(c->u.conn), d, deliver_fn);   /* o bloco da barreira não tem argumento: ignora */
}
void xpc_connection_send_message_with_reply(xpc_connection_t c, xpc_object_t msg, void *queue, void *handler) {
    (void)msg;
    if (!IS_CONN(c) || !handler) return;
    deliver_t *d = malloc(sizeof *d);
    if (!d) return;
    d->conn = (xo_t *)objc_retain((id)c);
    d->block = _Block_copy(handler);
    d->arg = XPC_ERROR_CONNECTION_INVALID;
    dispatch_async_f(queue ? queue : conn_queue(c->u.conn), d, deliver_fn);
}
xpc_object_t xpc_connection_send_message_with_reply_sync(xpc_connection_t c, xpc_object_t msg) {
    (void)c; (void)msg;
    fix_statics();
    return XPC_ERROR_CONNECTION_INVALID;              /* imortal: o release do chamador não faz nada */
}

/* ---------------------------------------------------------------- serviço, eventos, transações, pipes */
void xpc_main(void (*handler)(xpc_connection_t)) { (void)handler; exit(0); }
void xpc_transaction_begin(void) {}
void xpc_transaction_end(void) {}
void xpc_set_event_stream_handler(const char *stream, void *queue, void *handler) { (void)stream; (void)queue; (void)handler; }
const char *xpc_strerror(int e) { (void)e; return "XPC error"; }
#define CX_EINVAL 22
int xpc_pipe_receive(unsigned port, xpc_object_t *msg) { (void)port; if (msg) *msg = 0; return CX_EINVAL; }
int xpc_pipe_routine_reply(xpc_object_t reply) { (void)reply; return CX_EINVAL; }
int xpc_pipe_routine(void *pipe, xpc_object_t msg, xpc_object_t *reply) { (void)pipe; (void)msg; if (reply) *reply = 0; return CX_EINVAL; }
void *xpc_pipe_create(const char *name, u64 flags) { (void)name; (void)flags; return 0; }
void *xpc_pipe_create_from_port(unsigned port, u64 flags) { (void)port; (void)flags; return 0; }
