/*
 * mOS — teste do XPC do libSystem (userland/libsystem/xpc.c). Compila igual no macOS (clang -fblocks main.c).
 * No mOS: .\tools\build-xpctest.ps1 e rode no harness:
 *   $env:MACHOTEST_PATH = '/XpcTest.app/Contents/MacOS/XpcTest'
 *   .\build\machotest.exe build\libSystem.mos build\xpctest build\xpctest\XpcTest.app\Contents\MacOS\XpcTest
 * Imprime "xpctest: N/N ok". Os casos de conexão conferem o que um Mac faz quando o serviço não existe.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef void *xpc_object_t, *xpc_connection_t;
typedef const struct _xpc_type_s *xpc_type_t;
typedef void *dispatch_queue_t;
extern const struct _xpc_type_s _xpc_type_dictionary, _xpc_type_array, _xpc_type_string, _xpc_type_int64,
    _xpc_type_error, _xpc_type_bool, _xpc_type_connection, _xpc_type_null;
extern struct _xpc_dictionary_s _xpc_error_connection_invalid;
extern struct _xpc_bool_s _xpc_bool_true;
extern const char *const _xpc_error_key_description;

xpc_object_t xpc_dictionary_create(const char *const *, const xpc_object_t *, size_t);
void xpc_dictionary_set_string(xpc_object_t, const char *, const char *);
void xpc_dictionary_set_int64(xpc_object_t, const char *, int64_t);
void xpc_dictionary_set_uint64(xpc_object_t, const char *, uint64_t);
void xpc_dictionary_set_bool(xpc_object_t, const char *, bool);
void xpc_dictionary_set_double(xpc_object_t, const char *, double);
void xpc_dictionary_set_data(xpc_object_t, const char *, const void *, size_t);
void xpc_dictionary_set_value(xpc_object_t, const char *, xpc_object_t);
const char *xpc_dictionary_get_string(xpc_object_t, const char *);
int64_t xpc_dictionary_get_int64(xpc_object_t, const char *);
uint64_t xpc_dictionary_get_uint64(xpc_object_t, const char *);
bool xpc_dictionary_get_bool(xpc_object_t, const char *);
double xpc_dictionary_get_double(xpc_object_t, const char *);
const void *xpc_dictionary_get_data(xpc_object_t, const char *, size_t *);
xpc_object_t xpc_dictionary_get_value(xpc_object_t, const char *);
size_t xpc_dictionary_get_count(xpc_object_t);
bool xpc_dictionary_apply(xpc_object_t, bool (^)(const char *, xpc_object_t));
xpc_object_t xpc_array_create(const xpc_object_t *, size_t);
void xpc_array_append_value(xpc_object_t, xpc_object_t);
size_t xpc_array_get_count(xpc_object_t);
xpc_object_t xpc_array_get_value(xpc_object_t, size_t);
xpc_object_t xpc_string_create(const char *);
const char *xpc_string_get_string_ptr(xpc_object_t);
size_t xpc_string_get_length(xpc_object_t);
xpc_object_t xpc_int64_create(int64_t);
int64_t xpc_int64_get_value(xpc_object_t);
xpc_object_t xpc_bool_create(bool);
xpc_object_t xpc_null_create(void);
xpc_object_t xpc_uuid_create(const unsigned char *);
const unsigned char *xpc_uuid_get_bytes(xpc_object_t);
xpc_object_t xpc_date_create_from_current(void);
int64_t xpc_date_get_value(xpc_object_t);
xpc_object_t xpc_copy(xpc_object_t);
bool xpc_equal(xpc_object_t, xpc_object_t);
size_t xpc_hash(xpc_object_t);
char *xpc_copy_description(xpc_object_t);
xpc_type_t xpc_get_type(xpc_object_t);
const char *xpc_type_get_name(xpc_type_t);
xpc_object_t xpc_retain(xpc_object_t);
void xpc_release(xpc_object_t);
xpc_connection_t xpc_connection_create_mach_service(const char *, dispatch_queue_t, uint64_t);
void xpc_connection_set_event_handler(xpc_connection_t, void (^)(xpc_object_t));
void xpc_connection_resume(xpc_connection_t);
void xpc_connection_cancel(xpc_connection_t);
void xpc_connection_send_message(xpc_connection_t, xpc_object_t);
void xpc_connection_send_barrier(xpc_connection_t, void (^)(void));
void xpc_connection_send_message_with_reply(xpc_connection_t, xpc_object_t, dispatch_queue_t, void (^)(xpc_object_t));
xpc_object_t xpc_connection_send_message_with_reply_sync(xpc_connection_t, xpc_object_t);
const char *xpc_connection_get_name(xpc_connection_t);
xpc_object_t xpc_dictionary_create_reply(xpc_object_t);
dispatch_queue_t dispatch_get_global_queue(long, unsigned long);
void dispatch_sync(dispatch_queue_t, void (^)(void));
void usleep(unsigned);
int fork(void);
int waitpid(int pid, int *st, int opt);
void _exit(int code);
int getpid(void);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
unsigned bootstrap_port;
int bootstrap_check_in(unsigned bp, const char *name, unsigned *port);
int mach_port_allocate(unsigned task, int right, unsigned *name);
int mach_port_insert_right(unsigned task, unsigned name, unsigned poly, int type);
int mach_port_type(unsigned task, unsigned name, unsigned *ptype);
int mach_msg(void *hdr, int opt, unsigned send_sz, unsigned rcv_sz, unsigned rcv, unsigned timeout, unsigned notify);
void xpc_dictionary_set_mach_send(xpc_object_t, const char *, unsigned);
unsigned xpc_dictionary_copy_mach_send(xpc_object_t, const char *);
void *xpc_pipe_create(const char *, uint64_t);
int xpc_pipe_routine(void *, xpc_object_t, xpc_object_t *);
int xpc_pipe_receive(unsigned, xpc_object_t *);
int xpc_pipe_routine_reply(xpc_object_t);
void xpc_set_event_stream_handler(const char *, void *, void *);
void xpc_event_stream_post(const char *, xpc_object_t);
int cx_xpc_peer_ready(void);
int cx_xpc_peer_call(int, const void *, int, void *, int, int *);
int cx_xpc_peer_send(int, const void *, int);
int cx_xpc_peer_listen(void (*fn)(int, const void *, int, void *), void *);

typedef struct objc_object *id;
typedef struct objc_selector *SEL;
id objc_msgSend(id, SEL, ...);
id objc_getClass(const char *);
void *objc_allocateClassPair(void *, const char *, unsigned long);
int class_addMethod(void *, SEL, void *, const char *);
void objc_registerClassPair(void *);
SEL sel_registerName(const char *);
id cx_str(const char *);
const char *cx_cstr(id);

static int ok, total;
static volatile int g_ev_remote;
static volatile int g_port_got;
static unsigned g_child_port;
static volatile int g_peer_from;
static id g_export;
static volatile int g_ns_got;
static char g_ns_text[80];

static int child_status(int pid) {
    int st = 0;
    if (waitpid(pid, &st, 0) != pid) return -1;
    if ((st & 0x7f) != 0) return -1;
    return (st >> 8) & 0xff;
}
static void *port_thr(void *arg) {
    unsigned char buf[64];
    unsigned sz = 64, id = 0;
    (void)arg;
    memset(buf, 0, sizeof buf);
    memcpy(buf + 4, &sz, 4);
    memcpy(buf + 12, &g_child_port, 4);
    int kr = mach_msg(buf, 2 | 0x100, 0, 64, g_child_port, 3000, 0);
    memcpy(&id, buf + 20, 4);
    if (kr == 0 && id == 42) g_port_got = 1;
    return 0;
}
static void on_peer(int from, const void *bytes, int n, void *ctx) {
    (void)ctx;
    g_peer_from = from;
    if (n == 4 && bytes && !memcmp(bytes, "ping", 4)) cx_xpc_peer_send(from, "pong", 4);
}
static void echo_imp(id self, SEL cmd, id str, void *reply) {
    char buf[80];
    typedef struct { void *isa; int flags, reserved; void *invoke; } blk_t;
    (void)self;
    (void)cmd;
    snprintf(buf, sizeof buf, "ok:%s", str ? cx_cstr(str) : "");
    if (reply) ((void (*)(void *, id))((blk_t *)reply)->invoke)(reply, cx_str(buf));
}
static signed char accept_imp(id self, SEL cmd, id listener, id conn) {
    (void)self;
    (void)cmd;
    (void)listener;
    objc_msgSend(conn, sel_registerName("setExportedObject:"), g_export);
    objc_msgSend(conn, sel_registerName("resume"));
    return 1;
}
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
    fflush(stdout);
}

int main(void) {
    /* objetos */
    xpc_object_t d = xpc_dictionary_create(NULL, NULL, 0);
    check(d && xpc_get_type(d) == &_xpc_type_dictionary, "xpc_dictionary_create = XPC_TYPE_DICTIONARY");
    xpc_dictionary_set_string(d, "nome", "mOS");
    xpc_dictionary_set_int64(d, "n", -42);
    xpc_dictionary_set_uint64(d, "u", 1ULL << 40);
    xpc_dictionary_set_bool(d, "b", true);
    xpc_dictionary_set_double(d, "x", 2.5);
    xpc_dictionary_set_data(d, "bytes", "\x01\x02\x03", 3);
    size_t dl = 0;
    const unsigned char *db = xpc_dictionary_get_data(d, "bytes", &dl);
    check(!strcmp(xpc_dictionary_get_string(d, "nome"), "mOS") && xpc_dictionary_get_int64(d, "n") == -42 &&
          xpc_dictionary_get_uint64(d, "u") == 1ULL << 40 && xpc_dictionary_get_bool(d, "b") &&
          xpc_dictionary_get_double(d, "x") == 2.5 && dl == 3 && db[2] == 3, "set/get de string, int64, uint64, bool, double, data");
    check(xpc_dictionary_get_count(d) == 6 && !xpc_dictionary_get_value(d, "falta"), "contagem e chave ausente = NULL");
    xpc_dictionary_set_int64(d, "n", 7);
    check(xpc_dictionary_get_int64(d, "n") == 7 && xpc_dictionary_get_count(d) == 6, "sobrescrever uma chave");
    xpc_dictionary_set_value(d, "n", NULL);
    check(xpc_dictionary_get_count(d) == 5 && !xpc_dictionary_get_value(d, "n"), "valor NULL apaga a chave");
    __block int seen = 0;
    bool all = xpc_dictionary_apply(d, ^bool(const char *k, xpc_object_t v) { (void)k; (void)v; seen++; return true; });
    check(all && seen == 5, "xpc_dictionary_apply visita todas as chaves");
    seen = 0;
    all = xpc_dictionary_apply(d, ^bool(const char *k, xpc_object_t v) { (void)k; (void)v; return ++seen < 2; });
    check(!all && seen == 2, "xpc_dictionary_apply para quando o bloco devolve false");

    xpc_object_t a = xpc_array_create(NULL, 0);
    xpc_object_t s = xpc_string_create("olá");
    xpc_array_append_value(a, s);
    xpc_array_append_value(a, xpc_bool_create(true));
    xpc_object_t i = xpc_int64_create(5);
    xpc_array_append_value(a, i);
    xpc_release(i);
    check(xpc_array_get_count(a) == 3 && xpc_get_type(xpc_array_get_value(a, 0)) == &_xpc_type_string &&
          xpc_string_get_length(s) == 4 && xpc_array_get_value(a, 1) == (xpc_object_t)&_xpc_bool_true &&
          xpc_int64_get_value(xpc_array_get_value(a, 2)) == 5, "array: append, get, XPC_BOOL_TRUE singleton");
    xpc_dictionary_set_value(d, "lista", a);
    xpc_release(a);
    xpc_release(s);
    check(xpc_array_get_count(xpc_dictionary_get_value(d, "lista")) == 3, "array aninhado sobrevive ao release do criador");

    unsigned char u[16] = {0x12, 0x34};
    xpc_object_t uu = xpc_uuid_create(u);
    check(xpc_uuid_get_bytes(uu)[1] == 0x34, "uuid");
    xpc_object_t dt = xpc_date_create_from_current();
    check(xpc_date_get_value(dt) > 1600000000LL * 1000000000LL, "date: nanossegundos desde 1970");
    xpc_release(uu);
    xpc_release(dt);
    check(xpc_get_type(xpc_null_create()) == &_xpc_type_null && !strcmp(xpc_type_get_name(&_xpc_type_dictionary), "dictionary"),
          "null e xpc_type_get_name");

    xpc_object_t c = xpc_copy(d);
    check(c != d && xpc_equal(c, d) && xpc_hash(c) == xpc_hash(d), "xpc_copy profunda: igual e mesmo hash");
    xpc_dictionary_set_string(c, "nome", "outro");
    check(!xpc_equal(c, d) && !strcmp(xpc_dictionary_get_string(d, "nome"), "mOS"), "mudar a cópia não muda o original");
    char *desc = xpc_copy_description(d);
    check(desc && strstr(desc, "<dictionary: { count = 6") && strstr(desc, "\"nome\" => <string: \"mOS\">"), "xpc_copy_description");
    free(desc);
    xpc_release(c);

    /* erros */
    xpc_object_t err = &_xpc_error_connection_invalid;
    check(xpc_get_type(err) == &_xpc_type_error && !strcmp(xpc_dictionary_get_string(err, _xpc_error_key_description), "Connection invalid"),
          "XPC_ERROR_CONNECTION_INVALID com XPC_ERROR_KEY_DESCRIPTION");
    xpc_retain(err);
    xpc_release(err);
    xpc_release(err);
    check(xpc_get_type(err) == &_xpc_type_error, "singletons de erro são imortais");

    /* conexão com um serviço que não existe */
    xpc_connection_t conn = xpc_connection_create_mach_service("com.mos.teste.inexistente", dispatch_get_global_queue(0, 0), 0);
    check(conn && xpc_get_type(conn) == &_xpc_type_connection && !strcmp(xpc_connection_get_name(conn), "com.mos.teste.inexistente"),
          "xpc_connection_create_mach_service");
    __block int events = 0;
    __block xpc_object_t last = NULL;
    xpc_connection_set_event_handler(conn, ^(xpc_object_t ev) { events++; last = ev; });
    xpc_connection_resume(conn);
    for (int k = 0; k < 100 && !events; k++) usleep(1000);
    check(events == 1 && last == err, "resume: o handler recebe XPC_ERROR_CONNECTION_INVALID");
    xpc_connection_send_message(conn, d);
    __block xpc_object_t reply = NULL;
    xpc_connection_send_message_with_reply(conn, d, NULL, ^(xpc_object_t r) { reply = r; });
    for (int k = 0; k < 100 && !reply; k++) usleep(1000);
    check(reply == err, "send_message_with_reply: a resposta é XPC_ERROR_CONNECTION_INVALID");
    check(xpc_connection_send_message_with_reply_sync(conn, d) == err, "send_message_with_reply_sync = XPC_ERROR_CONNECTION_INVALID");
    __block int barrier = 0;
    xpc_connection_send_barrier(conn, ^{ barrier = 1; });
    for (int k = 0; k < 100 && !barrier; k++) usleep(1000);
    check(barrier == 1, "send_barrier roda o bloco");
    xpc_connection_cancel(conn);
    usleep(5000);
    check(events == 1, "cancel depois de inválida não repete o evento");
    check(xpc_dictionary_create_reply(d) == NULL, "create_reply de mensagem local = NULL");
    xpc_release(conn);
    xpc_release(d);

    /* porta Mach no dicionário, no mesmo processo: o nome volta igual */
    {
        unsigned port = 0;
        xpc_object_t md = xpc_dictionary_create(NULL, NULL, 0);
        check(mach_port_allocate(0x103, 1, &port) == 0 && port != 0, "mach_port_allocate de recepção");
        xpc_dictionary_set_mach_send(md, "port", port);
        xpc_object_t cp = xpc_copy(md);
        check(xpc_dictionary_copy_mach_send(md, "port") == port && xpc_equal(cp, md) &&
              xpc_dictionary_copy_mach_send(cp, "port") == port, "set/copy/equal de mach_send no mesmo processo");
        xpc_release(cp);
        xpc_release(md);
    }

    /* stream: o bloco fica guardado e roda neste processo */
    {
        char name[64];
        __block int local = 0;
        snprintf(name, sizeof name, "com.mos.xpctest.ev.local.%d", getpid());
        xpc_set_event_stream_handler(name, NULL, ^(xpc_object_t ev) {
            const char *k = xpc_dictionary_get_string(ev, "k");
            if (k && !strcmp(k, "local")) local = 1;
        });
        xpc_object_t ev = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_string(ev, "k", "local");
        xpc_event_stream_post(name, ev);
        xpc_release(ev);
        check(local == 1, "xpc_set_event_stream_handler guarda e chama o bloco");
    }

    /* o filho publica no stream que o pai registrou */
    {
        char name[64];
        snprintf(name, sizeof name, "com.mos.xpctest.ev.%d", getpid());
        g_ev_remote = 0;
        xpc_set_event_stream_handler(name, NULL, ^(xpc_object_t ev) {
            const char *k = xpc_dictionary_get_string(ev, "k");
            if (k && !strcmp(k, "filho")) g_ev_remote = 1;
        });
        int pid = fork();
        if (pid == 0) {
            xpc_object_t ev = xpc_dictionary_create(NULL, NULL, 0);
            xpc_dictionary_set_string(ev, "k", "filho");
            xpc_event_stream_post(name, ev);
            xpc_release(ev);
            _exit(0);
        }
        int saw = 0;
        for (int i = 0; pid > 0 && i < 3000 && !g_ev_remote; i++) usleep(1000);
        saw = g_ev_remote;
        int st = pid > 0 ? child_status(pid) : -1;
        check(pid > 0 && st == 0 && saw, "xpc_event_stream_post de outro processo chama o bloco");
    }

    /* pipe entre pai e filho, com um direito de envio de verdade no dicionário.
     * O filho é o servidor: se ele morrer, xpc_pipe_routine estoura o prazo em vez de ficar preso. */
    {
        char name[64];
        unsigned long tid = 0;
        void *pipe;
        xpc_object_t req, rep = NULL;
        int rc, port_ok = 0;
        snprintf(name, sizeof name, "com.mos.xpctest.pipe.%d", getpid());
        int pid = fork();
        if (pid == 0) {
            unsigned svc = 0;
            xpc_object_t msg = NULL;
            int right = 0;
            if (bootstrap_check_in(bootstrap_port, name, &svc) != 0 || xpc_pipe_receive(svc, &msg) != 0 || !msg) _exit(2);
            unsigned p = xpc_dictionary_copy_mach_send(msg, "port");
            unsigned ty = 0;
            const char *k = xpc_dictionary_get_string(msg, "k");
            if (k && !strcmp(k, "ping") && p && mach_port_type(0x103, p, &ty) == 0 && (ty & 0x10000u)) {
                unsigned char mh[32];
                unsigned sz = 24, id = 42;
                memset(mh, 0, sizeof mh);
                mh[0] = 19;
                memcpy(mh + 4, &sz, 4);
                memcpy(mh + 8, &p, 4);
                memcpy(mh + 20, &id, 4);
                if (mach_msg(mh, 1, 24, 0, 0, 0, 0) == 0) right = 1;
            }
            xpc_object_t reply = xpc_dictionary_create_reply(msg);
            if (reply) {
                xpc_dictionary_set_string(reply, "k", "pong");
                xpc_pipe_routine_reply(reply);
                xpc_release(reply);
            }
            xpc_release(msg);
            _exit(right ? 0 : 3);
        }
        g_port_got = 0;
        port_ok = pid > 0 && mach_port_allocate(0x103, 1, &g_child_port) == 0 &&
                  mach_port_insert_right(0x103, g_child_port, g_child_port, 20) == 0 &&
                  pthread_create(&tid, 0, port_thr, 0) == 0;
        if (port_ok) usleep(20000);
        pipe = NULL;
        for (int i = 0; pid > 0 && i < 2000 && !pipe; i++) {
            pipe = xpc_pipe_create(name, 0);
            if (!pipe) usleep(1000);
        }
        req = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_string(req, "k", "ping");
        if (port_ok) xpc_dictionary_set_mach_send(req, "port", g_child_port);
        rc = pipe ? xpc_pipe_routine(pipe, req, &rep) : 22;
        for (int i = 0; i < 1000 && !g_port_got; i++) usleep(1000);
        xpc_release(req);
        {
            const char *k = rep ? xpc_dictionary_get_string(rep, "k") : NULL;
            int st = pid > 0 ? child_status(pid) : -1;
            check(pid > 0 && port_ok && rc == 0 && g_port_got && k && !strcmp(k, "pong") && st == 0,
                  "xpc_pipe leva um direito Mach COPY_SEND entre processos");
        }
        if (rep) xpc_release(rep);
    }

    /* bytes por pid: o formato que axtransport.h chama de call/send/listen */
    {
        g_peer_from = 0;
        int ready = cx_xpc_peer_listen(on_peer, 0);
        int parent = getpid();
        int pid = ready == 0 ? fork() : -1;
        if (pid == 0) {
            unsigned char rep[8];
            int rn = 0;
            int rc = cx_xpc_peer_call(parent, "ping", 4, rep, (int)sizeof rep, &rn);
            _exit(rc == 0 && rn == 4 && !memcmp(rep, "pong", 4) ? 0 : 4);
        }
        int st = pid > 0 ? child_status(pid) : -1;
        check(ready == 0 && cx_xpc_peer_ready() && pid > 0 && st == 0 && g_peer_from == pid,
              "cx_xpc_peer_call/send/listen entre dois processos");
    }

    /* NSXPCConnection: o filho chama echo:reply: no objeto exportado do pai */
    {
        void *echo_cls = objc_allocateClassPair(objc_getClass("NSObject"), "MosEcho", 0);
        void *del_cls = objc_allocateClassPair(objc_getClass("NSObject"), "MosAxDel", 0);
        id listener_cls = objc_getClass("NSXPCListener");
        char svc[64];
        snprintf(svc, sizeof svc, "com.mos.xpctest.nsxpc.%d", getpid());
        if (echo_cls && del_cls && listener_cls) {
            class_addMethod(echo_cls, sel_registerName("echo:reply:"), (void *)echo_imp, "v@:@@");
            class_addMethod(del_cls, sel_registerName("listener:shouldAcceptNewConnection:"), (void *)accept_imp, "c@:@@");
            objc_registerClassPair(echo_cls);
            objc_registerClassPair(del_cls);
            g_export = objc_msgSend(objc_msgSend((id)echo_cls, sel_registerName("alloc")), sel_registerName("init"));
            id del = objc_msgSend(objc_msgSend((id)del_cls, sel_registerName("alloc")), sel_registerName("init"));
            id listener = objc_msgSend(objc_msgSend(listener_cls, sel_registerName("alloc")),
                                       sel_registerName("initWithMachServiceName:"), cx_str(svc));
            objc_msgSend(listener, sel_registerName("setDelegate:"), del);
            objc_msgSend(listener, sel_registerName("resume"));
            int pid = fork();
            if (pid == 0) {
                id ccls = objc_getClass("NSXPCConnection");
                id cnx = objc_msgSend(objc_msgSend(ccls, sel_registerName("alloc")),
                                      sel_registerName("initWithMachServiceName:options:"), cx_str(svc), (unsigned long long)0);
                id proxy;
                objc_msgSend(cnx, sel_registerName("resume"));
                proxy = objc_msgSend(cnx, sel_registerName("remoteObjectProxy"));
                g_ns_got = 0;
                g_ns_text[0] = 0;
                ((void (*)(id, SEL, id, void (^)(id)))objc_msgSend)(proxy, sel_registerName("echo:reply:"), cx_str("ping"), ^(id r) {
                    snprintf(g_ns_text, sizeof g_ns_text, "%s", r ? cx_cstr(r) : "");
                    g_ns_got = 1;
                });
                for (int i = 0; i < 5000 && !g_ns_got; i++) usleep(1000);
                _exit(g_ns_got && !strcmp(g_ns_text, "ok:ping") ? 0 : 5);
            }
            int st = pid > 0 ? child_status(pid) : -1;
            check(pid > 0 && st == 0, "NSXPCConnection: proxy remoto echo:reply: entre dois processos");
        } else check(0, "NSXPCConnection: proxy remoto echo:reply: entre dois processos");
    }

    printf("xpctest: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
