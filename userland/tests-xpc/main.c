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

static int ok, total;
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

    printf("xpctest: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
