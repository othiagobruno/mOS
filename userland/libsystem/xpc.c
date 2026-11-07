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
 *  - conexões: o ouvinte (XPC_CONNECTION_MACH_SERVICE_LISTENER) faz bootstrap_check_in e o cliente faz
 *    bootstrap_look_up. Se o serviço não existe, o comportamento de sempre se mantém: depois de resume o
 *    handler recebe XPC_ERROR_CONNECTION_INVALID uma vez, send_message_with_reply e o sync devolvem o mesmo
 *    singleton imortal, a barreira roda e o cancel não repete o evento. Se o look_up acha o serviço, os dois
 *    lados trocam dicionários XPC por mach_msg (formato próprio do mOS: os dois extremos são esta libxpc).
 *    Conexão anônima, sem nome, continua inválida.
 *  - xpc_main, com XPC_SERVICE_NAME no ambiente, faz check_in desse nome e fica à espera; sem o nome, sai.
 *    xpc_pipe_* (API privada do launchd) devolve EINVAL; xpc_dictionary_copy_mach_send devolve MACH_PORT_NULL.
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
    void (*service)(void *);             /* xpc_main: função C chamada a cada cliente novo */
    u64 flags;
    u32 recv, send;                      /* porta de recepção e direito de envio do par */
    u32 inbound_reply;                   /* id da resposta que o próximo send_message deve carregar */
    u32 next_id;
    u32 sync_id;
    int resumed, invalid_sent, canceled, live;
    int is_peer;                         /* lado do servidor (criada pelo ouvinte para um cliente) */
    volatile int interrupted;            /* o serviço morreu: o próximo envio reconecta (launchd o reinicia) */
    int peer_pid;
    volatile int sync_done;
    volatile int sync_wait;               /* send_message_with_reply_sync esperando a resposta */
    volatile int lk;
    void *sync_reply;
    struct { u32 id; void *block; void *queue; } pend[4];
    xo_t *early[4];                      /* mensagem que chegou antes do handler ser instalado */
    u32 early_id[4];
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
    } else if (o->type == &_xpc_type_endpoint) free(o->u.str.s);
    else if (o->type == &_xpc_type_connection && o->u.conn) {
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
/* mensagem recebida que pede resposta -> conexão que a enviou (create_reply / get_remote_connection).
 * Mensagem montada aqui não entra na tabela, e create_reply devolve NULL, como no teste. */
static struct { xo_t *dict; xo_t *conn; } xpc_remote[16];
static int xpc_remote_i;
static void xpc_note_remote(xo_t *dict, xo_t *conn) {
    if (!dict || !conn || dict->type != &_xpc_type_dictionary) return;
    for (int i = 0; i < 16; i++)
        if (xpc_remote[i].dict == dict) { xpc_remote[i].conn = conn; return; }
    int i = xpc_remote_i++ & 15;
    xpc_remote[i].dict = dict;
    xpc_remote[i].conn = conn;
}
static void xpc_forget_remote(xo_t *dict) {
    for (int i = 0; i < 16; i++)
        if (xpc_remote[i].dict == dict) xpc_remote[i].dict = 0;
}
static xo_t *xpc_remote_conn(xo_t *dict) {
    if (!dict) return 0;
    for (int i = 0; i < 16; i++)
        if (xpc_remote[i].dict == dict) return xpc_remote[i].conn;
    return 0;
}
xpc_object_t xpc_dictionary_create_reply(xpc_object_t original) {
    if (!xpc_remote_conn(original)) return 0;
    return xpc_dictionary_create(0, 0, 0);
}
xpc_connection_t xpc_dictionary_get_remote_connection(xpc_object_t d) { return xpc_remote_conn(d); }

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
#define XPC_LISTENER 1u                 /* XPC_CONNECTION_MACH_SERVICE_LISTENER */
#define XP_COPY_SEND 19
#define XP_MAKE_SEND 20
#define XP_COMPLEX 0x80000000u
#define XP_HELLO 1
#define XP_WELCOME 2
#define XP_DATA 3
#define XP_MAGIC 0x58504331u
extern unsigned bootstrap_port;
int bootstrap_check_in(unsigned bp, const char *name, unsigned *port);
int bootstrap_look_up(unsigned bp, const char *name, unsigned *port);
int mach_port_allocate(unsigned task, int right, unsigned *name);
int mach_port_deallocate(unsigned task, unsigned name);
int mach_msg(void *msg, int option, unsigned send_size, unsigned rcv_size, unsigned rcv_name, unsigned timeout, unsigned notify);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int sched_yield(void);

static void w32p(u8 *b, u32 o, u32 v) { b[o] = (u8)v; b[o + 1] = (u8)(v >> 8); b[o + 2] = (u8)(v >> 16); b[o + 3] = (u8)(v >> 24); }
static u32 r32p(const u8 *b, u32 o) { return (u32)b[o] | (u32)b[o + 1] << 8 | (u32)b[o + 2] << 16 | (u32)b[o + 3] << 24; }
static int trailer_pid(const u8 *msg) {
    u32 body = (r32p(msg, 4) + 3u) & ~3u;
    return (int)r32p(msg, body + 40);          /* audit_token.val[5], com MACH_RCV_TRAILER_ELEMENTS(3) */
}
#define XPC_ERROR_CONNECTION_INTERRUPTED (&_xpc_error_connection_interrupted)
static void xp_fail_pending(xo_t *c, xo_t *err);
static void xp_watch_dead(unsigned send, unsigned recv);
static int port_alloc(unsigned *out) {
    *out = 0;
    return mach_port_allocate(0x103u, 1, out) == 0 && *out ? 0 : -1;
}

static xo_t *conn_new(const char *name, void *queue, u64 flags) {
    fix_statics();
    xo_t *o = xnew(&_xpc_type_connection);
    if (!o) return 0;
    o->u.conn = calloc(1, sizeof(xconn_t));
    if (!o->u.conn) { objc_release((id)o); return 0; }
    o->u.conn->name = name ? strdup(name) : 0;
    o->u.conn->queue = queue;
    o->u.conn->flags = flags;
    return o;
}
xpc_connection_t xpc_connection_create(const char *name, void *queue) { return conn_new(name, queue, 0); }
xpc_connection_t xpc_connection_create_mach_service(const char *name, void *queue, u64 flags) { return conn_new(name, queue, flags); }
xpc_connection_t xpc_connection_create_from_endpoint(xpc_object_t ep) {
    const char *name = (ep && ep->type == &_xpc_type_endpoint) ? ep->u.str.s : 0;
    return conn_new(name, 0, 0);
}
xpc_object_t xpc_endpoint_create(xpc_connection_t c) {
    fix_statics();
    xo_t *o = xnew(&_xpc_type_endpoint);
    if (o && IS_CONN(c) && c->u.conn->name) {
        o->u.str.s = strdup(c->u.conn->name);
        o->u.str.n = strlen(o->u.str.s);
    }
    return o;
}
static void xp_flush(xo_t *c);
void xpc_connection_set_event_handler(xpc_connection_t c, void *block) {
    if (!IS_CONN(c)) return;
    if (c->u.conn->handler) _Block_release(c->u.conn->handler);
    c->u.conn->handler = block ? _Block_copy(block) : 0;
    if (c->u.conn->handler) xp_flush(c);
}
void xpc_connection_set_target_queue(xpc_connection_t c, void *q) { if (IS_CONN(c)) c->u.conn->queue = q; }
void xpc_connection_set_context(xpc_connection_t c, void *ctx) { if (IS_CONN(c)) c->u.conn->context = ctx; }
void *xpc_connection_get_context(xpc_connection_t c) { return IS_CONN(c) ? c->u.conn->context : 0; }
void xpc_connection_set_finalizer_f(xpc_connection_t c, void (*fn)(void *)) { if (IS_CONN(c)) c->u.conn->finalizer = fn; }
const char *xpc_connection_get_name(xpc_connection_t c) { return IS_CONN(c) ? c->u.conn->name : 0; }
int xpc_connection_get_pid(xpc_connection_t c) { return IS_CONN(c) ? c->u.conn->peer_pid : 0; }
unsigned xpc_connection_get_euid(xpc_connection_t c) { return IS_CONN(c) && c->u.conn->live ? 501u : 0; }
unsigned xpc_connection_get_egid(xpc_connection_t c) { return IS_CONN(c) && c->u.conn->live ? 20u : 0; }
int xpc_connection_get_asid(xpc_connection_t c) { (void)c; return 0; }
void xpc_connection_get_audit_token(xpc_connection_t c, void *tok) {
    if (!tok) return;
    u32 *v = tok;
    memset(v, 0, 32);
    v[0] = v[1] = 501;
    v[2] = v[3] = 20;
    if (IS_CONN(c)) v[5] = (u32)c->u.conn->peer_pid;   /* audit_token_to_pid lê val[5] */
    v[7] = 1;
}
int xpc_connection_set_peer_code_signing_requirement(xpc_connection_t c, const char *req) { (void)c; (void)req; return 0; }

static void *conn_queue(xconn_t *c) { return c->queue ? c->queue : dispatch_get_global_queue(0, 0); }

typedef struct { xo_t *conn; void *block; xo_t *arg; int release_arg; } deliver_t;
static void deliver_fn(void *p) {
    deliver_t *d = p;
    if (d->block) CALL_BLOCK1(d->block, d->arg);
    if (d->arg) xpc_forget_remote(d->arg);
    if (d->release_arg && d->arg) objc_release((id)d->arg);
    if (d->block) _Block_release(d->block);
    objc_release((id)d->conn);
    free(d);
}
static void deliver(xo_t *c, void *block, xo_t *arg, int copy_block, int release_arg, void *queue) {
    if (!block) {
        if (release_arg && arg) objc_release((id)arg);
        return;
    }
    deliver_t *d = malloc(sizeof *d);
    if (!d) {
        if (release_arg && arg) objc_release((id)arg);
        return;
    }
    d->conn = (xo_t *)objc_retain((id)c);
    d->block = copy_block ? _Block_copy(block) : block;
    d->arg = arg;
    d->release_arg = release_arg;
    dispatch_async_f(queue ? queue : conn_queue(c->u.conn), d, deliver_fn);
}
/* serviço ausente, ou a porta morreu: o handler fica sabendo uma vez só, com o singleton imortal */
static void conn_invalidate(xo_t *c) {
    xconn_t *k = c->u.conn;
    k->live = 0;
    if (k->invalid_sent) return;
    k->invalid_sent = 1;
    k->sync_done = 1;
    if (k->handler) deliver(c, k->handler, XPC_ERROR_CONNECTION_INVALID, 1, 0, 0);
}

/* ---- serialização própria (os dois lados são esta libxpc; não é o fio da libxpc da Apple) */
typedef struct { u8 *p; u32 n, cap; int err; } xpbuf_t;
static void xp_put(xpbuf_t *b, const void *s, u32 n) {
    if (b->err) return;
    if (b->n + n > b->cap) {
        u32 c = b->cap ? b->cap * 2 : 64;
        while (c < b->n + n) c *= 2;
        if (c > 32 * 1024) { b->err = 1; return; }
        u8 *np = realloc(b->p, c);
        if (!np) { b->err = 1; return; }
        b->p = np;
        b->cap = c;
    }
    if (n && s) memcpy(b->p + b->n, s, n);
    b->n += n;
}
static void xp_u8(xpbuf_t *b, u8 v) { xp_put(b, &v, 1); }
static void xp_u32(xpbuf_t *b, u32 v) { u8 d[4] = { (u8)v, (u8)(v >> 8), (u8)(v >> 16), (u8)(v >> 24) }; xp_put(b, d, 4); }
static void xp_u64(xpbuf_t *b, u64 v) { xp_u32(b, (u32)v); xp_u32(b, (u32)(v >> 32)); }
static void xp_enc(xpbuf_t *b, xo_t *o, int depth);
static void xp_enc(xpbuf_t *b, xo_t *o, int depth) {
    if (b->err) return;
    if (!o || depth > 16) { xp_u8(b, 1); return; }
    xpc_type_t t = o->type;
    if (t == &_xpc_type_null) xp_u8(b, 1);
    else if (t == &_xpc_type_bool) { xp_u8(b, 2); xp_u8(b, o->u.b ? 1 : 0); }
    else if (t == &_xpc_type_int64) { xp_u8(b, 3); xp_u64(b, (u64)o->u.i); }
    else if (t == &_xpc_type_uint64) { xp_u8(b, 4); xp_u64(b, o->u.u); }
    else if (t == &_xpc_type_double) { u64 bits; memcpy(&bits, &o->u.d, 8); xp_u8(b, 5); xp_u64(b, bits); }
    else if (t == &_xpc_type_date) { xp_u8(b, 6); xp_u64(b, (u64)o->u.i); }
    else if (t == &_xpc_type_string) { xp_u8(b, 7); xp_u32(b, (u32)o->u.str.n); xp_put(b, o->u.str.s, (u32)o->u.str.n); }
    else if (t == &_xpc_type_data) { xp_u8(b, 8); xp_u32(b, (u32)o->u.data.n); xp_put(b, o->u.data.p, (u32)o->u.data.n); }
    else if (t == &_xpc_type_uuid) { xp_u8(b, 9); xp_put(b, o->u.uuid, 16); }
    else if (t == &_xpc_type_array) {
        xp_u8(b, 10);
        xp_u32(b, (u32)o->u.arr.n);
        for (size_t i = 0; i < o->u.arr.n; i++) xp_enc(b, o->u.arr.v[i], depth + 1);
    } else if (t == &_xpc_type_dictionary) {
        xp_u8(b, 11);
        xp_u32(b, (u32)o->u.dict.n);
        for (size_t i = 0; i < o->u.dict.n; i++) {
            u32 kn = (u32)strlen(o->u.dict.k[i]);
            xp_u32(b, kn);
            xp_put(b, o->u.dict.k[i], kn);
            xp_enc(b, o->u.dict.v[i], depth + 1);
        }
    } else if (t == &_xpc_type_error) {
        const char *s = xpc_dictionary_get_string(o, "XPCErrorDescription");
        u32 kn = s ? (u32)strlen(s) : 0;
        xp_u8(b, 12);
        xp_u32(b, kn);
        if (kn) xp_put(b, s, kn);
    } else xp_u8(b, 1);
}
typedef struct { const u8 *p; u32 n, i; int err; } xprd_t;
static u8 xp_r8(xprd_t *r) { if (r->i >= r->n) { r->err = 1; return 0; } return r->p[r->i++]; }
static u32 xp_r32(xprd_t *r) {
    if (r->i + 4 > r->n) { r->err = 1; return 0; }
    u32 v = (u32)r->p[r->i] | (u32)r->p[r->i + 1] << 8 | (u32)r->p[r->i + 2] << 16 | (u32)r->p[r->i + 3] << 24;
    r->i += 4;
    return v;
}
static u64 xp_r64(xprd_t *r) { u64 lo = xp_r32(r); return lo | (u64)xp_r32(r) << 32; }
static char *xp_rstr(xprd_t *r, u32 n) {
    if (r->err || r->i + n > r->n || n > 1024 * 1024) { r->err = 1; return 0; }
    char *s = malloc(n + 1);
    if (!s) { r->err = 1; return 0; }
    if (n) memcpy(s, r->p + r->i, n);
    s[n] = 0;
    r->i += n;
    return s;
}
static xo_t *xp_err_singleton(const char *s) {
    if (!s) return 0;
    if (!strcmp(s, "Connection invalid")) return &_xpc_error_connection_invalid;
    if (!strcmp(s, "Connection interrupted")) return &_xpc_error_connection_interrupted;
    if (!strcmp(s, "Process will terminate imminently")) return &_xpc_error_termination_imminent;
    return 0;
}
static xo_t *xp_dec(xprd_t *r, int depth) {
    if (r->err || depth > 16) { r->err = 1; return 0; }
    switch (xp_r8(r)) {
    case 1: return xpc_null_create();
    case 2: return xpc_bool_create(xp_r8(r));
    case 3: return xpc_int64_create((i64)xp_r64(r));
    case 4: return xpc_uint64_create(xp_r64(r));
    case 5: { u64 bits = xp_r64(r); double d; memcpy(&d, &bits, 8); return r->err ? 0 : xpc_double_create(d); }
    case 6: return xpc_date_create((i64)xp_r64(r));
    case 7: { u32 n = xp_r32(r); char *s = xp_rstr(r, n); xo_t *o = s ? xpc_string_create(s) : 0; free(s); return o; }
    case 8: { u32 n = xp_r32(r); if (r->err || r->i + n > r->n) { r->err = 1; return 0; } xo_t *o = xpc_data_create(r->p + r->i, n); r->i += n; return o; }
    case 9: { if (r->i + 16 > r->n) { r->err = 1; return 0; } xo_t *o = xpc_uuid_create(r->p + r->i); r->i += 16; return o; }
    case 10: {
        u32 n = xp_r32(r);
        if (r->err || n > 100000) { r->err = 1; return 0; }
        xo_t *a = xpc_array_create(0, 0);
        for (u32 i = 0; i < n; i++) {
            xo_t *v = xp_dec(r, depth + 1);
            if (!v) { xpc_release(a); return 0; }
            xpc_array_append_value(a, v);
            xpc_release(v);
        }
        return a;
    }
    case 11: {
        u32 n = xp_r32(r);
        if (r->err || n > 100000) { r->err = 1; return 0; }
        xo_t *d = xpc_dictionary_create(0, 0, 0);
        for (u32 i = 0; i < n; i++) {
            u32 kn = xp_r32(r);
            char *key = xp_rstr(r, kn);
            xo_t *v = xp_dec(r, depth + 1);
            if (!key || !v) { free(key); xpc_release(v); xpc_release(d); return 0; }
            xpc_dictionary_set_value(d, key, v);
            free(key);
            xpc_release(v);
        }
        return d;
    }
    case 12: { u32 n = xp_r32(r); char *s = xp_rstr(r, n); xo_t *e = xp_err_singleton(s); free(s); return e ? e : xpc_null_create(); }
    default: r->err = 1; return 0;
    }
}

static void xp_lock(xconn_t *k) { while (__sync_lock_test_and_set(&k->lk, 1)) sched_yield(); }
static void xp_unlock(xconn_t *k) { __sync_lock_release(&k->lk); }
static int xp_pend_add(xconn_t *k, u32 id, void *block, void *queue) {
    xp_lock(k);
    int ok = 0;
    for (int i = 0; i < 4; i++)
        if (!k->pend[i].id) {
            k->pend[i].id = id;
            k->pend[i].block = _Block_copy(block);
            k->pend[i].queue = queue;
            ok = 1;
            break;
        }
    xp_unlock(k);
    return ok;
}
static void *xp_pend_take(xconn_t *k, u32 id, void **queue) {
    xp_lock(k);
    void *b = 0;
    for (int i = 0; i < 4; i++)
        if (k->pend[i].id == id) {
            b = k->pend[i].block;
            if (queue) *queue = k->pend[i].queue;
            k->pend[i].id = 0;
            k->pend[i].block = 0;
            break;
        }
    xp_unlock(k);
    return b;
}
static int xp_send(xconn_t *k, xo_t *msg, u32 reply_id) {
    if (!k->live || !k->send || k->canceled) return -1;
    xpbuf_t b = { 0, 0, 0, 0 };
    xp_enc(&b, msg, 0);
    if (b.err || !b.p) { free(b.p); return -1; }
    u32 total = 36 + b.n;
    u8 *buf = calloc(1, total);
    if (!buf) { free(b.p); return -1; }
    w32p(buf, 0, XP_COPY_SEND);
    w32p(buf, 4, total);
    w32p(buf, 8, k->send);
    w32p(buf, 20, XP_DATA);
    w32p(buf, 24, XP_MAGIC);
    w32p(buf, 28, reply_id);
    w32p(buf, 32, b.n);
    memcpy(buf + 36, b.p, b.n);
    int kr = mach_msg(buf, 1 /* MACH_SEND_MSG */, total, 0, 0, 0, 0);
    free(buf);
    free(b.p);
    return kr;
}
static int xp_welcome(unsigned remote, unsigned right) {
    u8 msg[64];
    memset(msg, 0, sizeof msg);
    w32p(msg, 0, XP_COMPLEX | XP_COPY_SEND);
    w32p(msg, 4, 40);
    w32p(msg, 8, remote);
    w32p(msg, 20, XP_WELCOME);
    w32p(msg, 24, 1);
    w32p(msg, 28, right);
    msg[38] = XP_MAKE_SEND;
    msg[39] = 0;
    return mach_msg(msg, 1, 40, 0, 0, 0, 0);
}
static void xp_give(xo_t *c, xo_t *obj, u32 rid) {
    xconn_t *k = c->u.conn;
    if (!obj) obj = xpc_null_create();
    if (rid && k->sync_id == rid && k->sync_wait) {
        k->sync_reply = obj;
        __sync_synchronize();
        k->sync_done = 1;
        return;
    }
    void *queue = 0;
    void *block = rid ? xp_pend_take(k, rid, &queue) : 0;
    if (block) { deliver(c, block, obj, 0, 1, queue); return; }
    if (!k->handler) {
        /* o ouvinte ainda está a instalar o bloco do par; guarda e o set_event_handler entrega */
        xp_lock(k);
        int saved = 0;
        for (int i = 0; i < 4; i++)
            if (!k->early[i]) { k->early[i] = obj; k->early_id[i] = rid; saved = 1; break; }
        xp_unlock(k);
        if (!saved && !is_static(obj)) objc_release((id)obj);
        return;
    }
    if (k->handler) {
        if (rid) k->inbound_reply = rid;
        xpc_note_remote(obj, c);
        deliver(c, k->handler, obj, 1, 1, 0);
        k->inbound_reply = 0;
    } else if (!is_static(obj)) objc_release((id)obj);
}
static void xp_flush(xo_t *c) {
    xconn_t *k = c->u.conn;
    xo_t *e[4];
    u32 id[4];
    int n = 0;
    xp_lock(k);
    for (int i = 0; i < 4; i++)
        if (k->early[i]) { e[n] = k->early[i]; id[n] = k->early_id[i]; k->early[i] = 0; n++; }
    xp_unlock(k);
    for (int i = 0; i < n; i++) xp_give(c, e[i], id[i]);
}
static void *xp_recv_main(void *arg) {
    xo_t *c = arg;
    xconn_t *k = c->u.conn;
    u8 *buf = malloc(48 * 1024);
    if (!buf) { objc_release((id)c); return 0; }
    while (!k->canceled) {
        memset(buf, 0, 64);
        w32p(buf, 4, 48 * 1024);
        w32p(buf, 12, k->recv);
        int kr = mach_msg(buf, 2 | (3u << 24), 0, 48 * 1024, k->recv, 0, 0);
        if (kr != 0) {
            if (k->canceled) break;
            if ((unsigned)kr == 0x10004004u) continue;   /* RCV_TOO_LARGE: o kernel já descartou */
            break;
        }
        if (r32p(buf, 20) == 72 /* MACH_NOTIFY_DEAD_NAME */) {
            /* o outro lado morreu. Cliente de um serviço com nome: INTERRUPTED, e o próximo envio reconecta (o launchd
             * guarda a porta do serviço e o inicia de novo). Lado do servidor: o cliente foi embora, INVALID. */
            if (k->is_peer || !k->name) break;
            k->live = 0;
            k->interrupted = 1;
            k->sync_reply = XPC_ERROR_CONNECTION_INTERRUPTED;
            k->sync_done = 1;
            if (k->send) { mach_port_deallocate(0x103u, k->send); k->send = 0; }
            xp_fail_pending(c, XPC_ERROR_CONNECTION_INTERRUPTED);
            if (k->handler) deliver(c, k->handler, XPC_ERROR_CONNECTION_INTERRUPTED, 1, 0, 0);
            break;
        }
        if (r32p(buf, 20) != XP_DATA || r32p(buf, 24) != XP_MAGIC) continue;
        u32 n = r32p(buf, 32);
        if (36u + n > 48u * 1024u) continue;
        int pid = trailer_pid(buf);
        if (pid) k->peer_pid = pid;
        xprd_t rd = { buf + 36, n, 0, 0 };
        xo_t *obj = xp_dec(&rd, 0);
        if (rd.err && obj && !is_static(obj)) { objc_release((id)obj); obj = 0; }
        xp_give(c, obj, r32p(buf, 28));
    }
    free(buf);
    if (!k->interrupted) {
        k->live = 0;
        k->sync_done = 1;
        if (!k->canceled) { xp_fail_pending(c, XPC_ERROR_CONNECTION_INVALID); conn_invalidate(c); }
    }
    objc_release((id)c);
    return 0;
}
static void *xp_listener_main(void *arg) {
    xo_t *c = arg;
    xconn_t *k = c->u.conn;
    u8 buf[512];
    while (!k->canceled) {
        memset(buf, 0, sizeof buf);
        w32p(buf, 4, sizeof buf);
        w32p(buf, 12, k->recv);
        int kr = mach_msg(buf, 2 | (3u << 24), 0, sizeof buf, k->recv, 0, 0);
        if (kr != 0) {
            if (k->canceled) break;
            if ((unsigned)kr == 0x10004004u) continue;
            break;
        }
        if (r32p(buf, 20) != XP_HELLO) continue;
        unsigned client = r32p(buf, 28);
        unsigned mine = 0;
        if (!client || port_alloc(&mine) != 0) continue;
        xo_t *peer = conn_new(0, k->queue, 0);
        if (!peer) { mach_port_deallocate(0x103u, mine); continue; }
        peer->u.conn->send = client;
        peer->u.conn->recv = mine;
        peer->u.conn->live = 1;
        peer->u.conn->resumed = 1;
        peer->u.conn->is_peer = 1;
        peer->u.conn->peer_pid = trailer_pid(buf);
        if (xp_welcome(client, mine) != 0) {
            mach_port_deallocate(0x103u, mine);
            objc_release((id)peer);
            continue;
        }
        xp_watch_dead(client, mine);           /* o cliente morre: aviso de nome morto na porta do par -> INVALID */
        /* a thread já recebe durante o callback: um send_message_with_reply_sync feito ali tem quem leia a resposta.
         * mensagem que chegue antes do set_event_handler fica em early[] e é entregue quando o bloco é instalado. */
        objc_retain((id)peer);
        unsigned long tid = 0;
        int started = pthread_create(&tid, 0, xp_recv_main, peer) == 0;
        if (k->service) k->service(peer);
        else if (k->handler) deliver(c, k->handler, peer, 1, 0, 0);
        if (!started) {
            objc_release((id)peer);
            conn_invalidate(peer);
        }
        objc_release((id)peer);
    }
    objc_release((id)c);
    return 0;
}
static void xp_start(xo_t *c, void *(*fn)(void *)) {
    objc_retain((id)c);
    unsigned long tid = 0;
    if (pthread_create(&tid, 0, fn, c) != 0) {
        objc_release((id)c);
        conn_invalidate(c);
    }
}
static int xp_handshake(unsigned svc, unsigned recv, unsigned *peer) {
    u8 msg[256];
    memset(msg, 0, sizeof msg);
    w32p(msg, 0, XP_COMPLEX | XP_COPY_SEND);
    w32p(msg, 4, 40);
    w32p(msg, 8, svc);
    w32p(msg, 20, XP_HELLO);
    w32p(msg, 24, 1);
    w32p(msg, 28, recv);
    msg[38] = XP_MAKE_SEND;
    msg[39] = 0;
    /* SEND|RCV|RCV_TIMEOUT, 30 s: sem ouvinte do outro lado o resume não fica preso. O prazo cobre o launchd iniciar
     * o daemon sob demanda (um Mach-O leva alguns segundos para carregar no QEMU) */
    int kr = mach_msg(msg, 1 | 2 | 0x100, 40, sizeof msg, recv, 30000, 0);
    if (kr != 0 || r32p(msg, 20) != XP_WELCOME) return -1;
    *peer = r32p(msg, 28);
    return *peer ? 0 : -1;
}

/* serviço XPC dentro do bundle do app (Contents/XPCServices/<x>.xpc, CFBundleIdentifier = nome): o launchd o registra
 * sob demanda com o executável do .xpc; o look_up seguinte acha o nome. 0 = registrado */
int cx_launchd_submit(const char *label, const char *program);
id cx_plist_file(const char *path);
static int xp_bundle_service(const char *name) {
    id b = MSG0(id, (id)cx_class_named("NSBundle"), "mainBundle");
    id bp = b ? MSG0(id, b, "bundlePath") : 0;
    const char *root = bp ? cx_cstr(bp) : 0;
    if (!root || !name) return -1;
    char dir[768], path[1024], exe[1024];
    snprintf(dir, sizeof dir, "%s/Contents/XPCServices", root);
    void *d = opendir(dir);
    if (!d) return -1;
    int found = -1;
    for (void *e; found && (e = readdir(d));) {
        const char *ent = (const char *)e + 21;             /* struct dirent do macOS ($INODE64): d_name no byte 21 */
        size_t n = strlen(ent);
        if (n < 5 || strcmp(ent + n - 4, ".xpc")) continue;
        snprintf(path, sizeof path, "%s/%s/Contents/Info.plist", dir, ent);
        id info = cx_plist_file(path);
        id ident = info ? MSG(id, info, "objectForKey:", cx_str("CFBundleIdentifier")) : 0;
        id execn = info ? MSG(id, info, "objectForKey:", cx_str("CFBundleExecutable")) : 0;
        const char *is = ident ? cx_cstr(ident) : 0;
        int same_dir = n - 4 == strlen(name) && !strncmp(ent, name, n - 4);
        if (!(is && !strcmp(is, name)) && !same_dir) continue;
        if (execn) snprintf(exe, sizeof exe, "%s/%s/Contents/MacOS/%s", dir, ent, cx_cstr(execn));
        else snprintf(exe, sizeof exe, "%s/%s/Contents/MacOS/%.*s", dir, ent, (int)(n - 4), ent);
        if (access(exe, 0) != 0) continue;
        found = cx_launchd_submit(name, exe) == 0 ? 0 : -1;
    }
    closedir(d);
    return found;
}
/* aviso de nome morto no envio `send`, entregue na nossa porta `recv` (msgh_id 72) */
int mach_port_request_notification(unsigned task, unsigned name, int variant, unsigned sync, unsigned notify, int poly, unsigned *prev);
static void xp_watch_dead(unsigned send, unsigned recv) {
    unsigned prev = 0;
    mach_port_request_notification(0x103u, send, 72, 0, recv, 21 /* MAKE_SEND_ONCE */, &prev);
}
static void xp_fail_pending(xo_t *c, xo_t *err) {
    xconn_t *k = c->u.conn;
    for (int i = 0; i < 4; i++) {
        xp_lock(k);
        u32 id = k->pend[i].id;
        xp_unlock(k);
        if (!id) continue;
        void *q = 0;
        void *b = xp_pend_take(k, id, &q);
        if (b) deliver(c, b, err, 0, 0, q);
    }
}
/* cliente: acha o serviço (bootstrap; senão um XPC service do bundle), faz o aperto de mão e inicia a recepção */
static int xp_connect(xo_t *c) {
    xconn_t *k = c->u.conn;
    unsigned svc = 0;
    if (bootstrap_look_up(bootstrap_port, k->name, &svc) != 0 || !svc) {
        svc = 0;
        if (xp_bundle_service(k->name) != 0 || bootstrap_look_up(bootstrap_port, k->name, &svc) != 0 || !svc) return -1;
    }
    if (!k->recv && port_alloc(&k->recv) != 0) { mach_port_deallocate(0x103u, svc); return -1; }
    unsigned peer = 0;
    int r = xp_handshake(svc, k->recv, &peer);
    mach_port_deallocate(0x103u, svc);
    if (r != 0) return -1;
    k->send = peer;
    xp_watch_dead(peer, k->recv);
    k->interrupted = 0;
    k->live = 1;
    xp_start(c, xp_recv_main);
    return 0;
}
/* depois de INTERRUPTED, o próximo envio reconecta (como a libxpc, que reabre a conexão com o serviço) */
static void xp_revive(xo_t *c) {
    xconn_t *k = c->u.conn;
    if (k->interrupted && !k->canceled && !k->live && xp_connect(c) != 0) { k->interrupted = 0; conn_invalidate(c); }
}
void xpc_connection_resume(xpc_connection_t c) {
    if (!IS_CONN(c) || c->u.conn->resumed) return;
    xconn_t *k = c->u.conn;
    k->resumed = 1;
    if (k->canceled || !k->name || !k->name[0]) { conn_invalidate(c); return; }
    if (k->flags & XPC_LISTENER) {
        unsigned port = 0;
        if (bootstrap_check_in(bootstrap_port, k->name, &port) != 0 || !port) { conn_invalidate(c); return; }
        k->recv = port;
        k->live = 1;
        xp_start(c, xp_listener_main);
        return;
    }
    if (xp_connect(c) != 0) conn_invalidate(c);
}
void xpc_connection_activate(xpc_connection_t c) { xpc_connection_resume(c); }
void xpc_connection_suspend(xpc_connection_t c) { (void)c; }
void xpc_connection_cancel(xpc_connection_t c) {
    if (!IS_CONN(c) || c->u.conn->canceled) return;
    xconn_t *k = c->u.conn;
    k->canceled = 1;
    k->live = 0;
    k->sync_done = 1;
    if (k->recv) mach_port_deallocate(0x103u, k->recv);
    conn_invalidate(c);
}
void xpc_connection_send_message(xpc_connection_t c, xpc_object_t msg) {
    if (IS_CONN(c)) xp_revive(c);
    if (!IS_CONN(c) || !c->u.conn->live) return;
    xconn_t *k = c->u.conn;
    u32 rid = k->inbound_reply;
    k->inbound_reply = 0;
    if (xp_send(k, msg, rid) != 0) conn_invalidate(c);
}
void xpc_connection_send_barrier(xpc_connection_t c, void *block) {
    if (!IS_CONN(c) || !block) return;
    deliver(c, block, 0, 1, 0, 0);             /* o bloco da barreira ignora o argumento */
}
void xpc_connection_send_message_with_reply(xpc_connection_t c, xpc_object_t msg, void *queue, void *handler) {
    if (!IS_CONN(c) || !handler) return;
    xconn_t *k = c->u.conn;
    xp_revive(c);
    if (!k->live) { deliver(c, handler, XPC_ERROR_CONNECTION_INVALID, 1, 0, queue); return; }
    u32 id = ++k->next_id;
    if (!id) id = ++k->next_id;
    if (!xp_pend_add(k, id, handler, queue) || xp_send(k, msg, id) != 0) {
        void *q = 0;
        void *b = xp_pend_take(k, id, &q);
        if (b) _Block_release(b);
        conn_invalidate(c);
        deliver(c, handler, XPC_ERROR_CONNECTION_INVALID, 1, 0, queue);
    }
}
xpc_object_t xpc_connection_send_message_with_reply_sync(xpc_connection_t c, xpc_object_t msg) {
    fix_statics();
    if (IS_CONN(c)) xp_revive(c);
    if (!IS_CONN(c) || !c->u.conn->live) return XPC_ERROR_CONNECTION_INVALID;
    xconn_t *k = c->u.conn;
    u32 id = ++k->next_id;
    if (!id) id = ++k->next_id;
    k->sync_reply = 0;
    k->sync_done = 0;
    k->sync_id = id;
    k->sync_wait = 1;
    if (xp_send(k, msg, id) != 0) {
        k->sync_wait = 0;
        conn_invalidate(c);
        return XPC_ERROR_CONNECTION_INVALID;
    }
    while (!k->sync_done && !k->canceled) sched_yield();
    k->sync_wait = 0;
    return k->sync_reply ? k->sync_reply : XPC_ERROR_CONNECTION_INVALID;
}

/* ---------------------------------------------------------------- serviço, eventos, transações, pipes */
void xpc_main(void (*handler)(xpc_connection_t)) {
    /* o launchd põe XPC_SERVICE_NAME (o Label do job, ou o CFBundleIdentifier de um .xpc); sem ele, o identificador do bundle */
    const char *name = getenv("XPC_SERVICE_NAME");
    if (!name || !name[0]) {
        id b = MSG0(id, (id)cx_class_named("NSBundle"), "mainBundle");
        id ident = b ? MSG0(id, b, "bundleIdentifier") : 0;
        name = ident ? cx_cstr(ident) : 0;
    }
    if (!name || !name[0] || !handler) exit(0);
    xo_t *c = xpc_connection_create_mach_service(name, 0, XPC_LISTENER);
    if (!IS_CONN(c)) exit(1);
    c->u.conn->service = (void (*)(void *))handler;
    xpc_connection_resume(c);
    if (!c->u.conn->live) exit(1);
    for (;;) usleep(1000000);
}
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
