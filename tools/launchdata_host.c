/*
 * mOS — teste de host da API cliente do launchd do libSystem (userland/libsystem/launchdata.c).
 *   gcc -O1 -Wall -DMOS_LIBSYS_SYSCALL -Iuserland/libsystem -c userland/libsystem/launchdata.c -o /tmp/launchdata_ls.o
 *   gcc -O1 -Wall tools/launchdata_host.c /tmp/launchdata_ls.o -o /tmp/launchdata_host && /tmp/launchdata_host
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
typedef void *ld;
ld launch_data_alloc(int); void launch_data_free(ld); int launch_data_get_type(ld);
int launch_data_dict_insert(ld, ld, const char *); ld launch_data_dict_lookup(ld, const char *);
int launch_data_dict_remove(ld, const char *); size_t launch_data_dict_get_count(ld);
void launch_data_dict_iterate(ld, void (*)(ld, const char *, void *), void *);
int launch_data_array_set_index(ld, ld, size_t); ld launch_data_array_get_index(ld, size_t); size_t launch_data_array_get_count(ld);
ld launch_data_new_string(const char *); ld launch_data_new_integer(long long); ld launch_data_new_bool(int); ld launch_data_new_real(double);
ld launch_data_new_opaque(const void *, size_t); ld launch_data_new_fd(int);
const char *launch_data_get_string(ld); long long launch_data_get_integer(ld); int launch_data_get_bool(ld); double launch_data_get_real(ld);
size_t launch_data_get_opaque_size(ld); void *launch_data_get_opaque(ld); unsigned launch_data_get_machport(ld); int launch_data_get_errno(ld);
ld launch_data_copy(ld); ld launch_msg(ld); int launch_activate_socket(const char *, int **, size_t *);
void *vproc_transaction_begin(void *); void vproc_transaction_end(void *, void *); size_t vproc_transaction_count(void);
void *vproc_swap_integer(void *, int, const long long *, long long *);

static int err; int *__error(void) { return &err; }
long cx_syscall(long n, long a, long b, long c, long d, long e) { (void)a; (void)b; (void)c; (void)d; (void)e; return n == 2 ? 4242 : -38; }
unsigned bootstrap_port = 0x303;
static int checkins;
int bootstrap_check_in(unsigned bp, const char *name, unsigned *port) { (void)bp; checkins++; *port = !strcmp(name, "com.ex.a") ? 0x1107 : 0x1203; return 0; }
static int fails, oks;
#define CHECK(c, m) do { if (c) oks++; else { fails++; printf("FALHA %d: %s\n", __LINE__, m); } } while (0)
static int seen;
static void it(ld v, const char *k, void *ctx) { (void)v; (void)k; (*(int *)ctx)++; seen++; }

int main(void) {
    ld d = launch_data_alloc(1);
    CHECK(launch_data_dict_insert(d, launch_data_new_string("x"), "a") && launch_data_dict_insert(d, launch_data_new_integer(7), "b"), "insert");
    CHECK(launch_data_dict_insert(d, launch_data_new_integer(9), "b") && launch_data_dict_get_count(d) == 2 && launch_data_get_integer(launch_data_dict_lookup(d, "b")) == 9, "troca valor");
    int n = 0; launch_data_dict_iterate(d, it, &n);
    CHECK(n == 2, "iterate");
    ld a = launch_data_alloc(2);
    launch_data_array_set_index(a, launch_data_new_bool(1), 0);
    launch_data_array_set_index(a, launch_data_new_real(2.5), 2);
    CHECK(launch_data_array_get_count(a) == 3 && launch_data_array_get_index(a, 1) == 0 && launch_data_get_real(launch_data_array_get_index(a, 2)) == 2.5, "array com buraco");
    launch_data_dict_insert(d, a, "arr");
    launch_data_dict_insert(d, launch_data_new_opaque("abc", 3), "op");
    ld cp = launch_data_copy(d);
    CHECK(cp != d && !strcmp(launch_data_get_string(launch_data_dict_lookup(cp, "a")), "x") && launch_data_get_opaque_size(launch_data_dict_lookup(cp, "op")) == 3 && launch_data_get_bool(launch_data_array_get_index(launch_data_dict_lookup(cp, "arr"), 0)) == 1, "copy profunda");
    CHECK(launch_data_dict_remove(cp, "a") && !launch_data_dict_lookup(cp, "a") && launch_data_dict_get_count(cp) == 3, "remove");
    launch_data_free(cp); launch_data_free(d);
    /* fora do launchd */
    unsetenv("XPC_SERVICE_NAME");
    ld r = launch_msg(launch_data_new_string("CheckIn"));
    CHECK(launch_data_get_type(r) == 9 && launch_data_get_errno(r) == 3, "CheckIn fora do launchd ESRCH");
    setenv("XPC_SERVICE_NAME", "com.ex.job", 1);
    setenv("MOS_LAUNCHD_MACHSERVICES", "com.ex.a:com.ex.b", 1);
    r = launch_msg(launch_data_new_string("CheckIn"));
    ld ms = launch_data_dict_lookup(r, "MachServices");
    CHECK(!strcmp(launch_data_get_string(launch_data_dict_lookup(r, "Label")), "com.ex.job") && launch_data_dict_get_count(ms) == 2 && launch_data_get_machport(launch_data_dict_lookup(ms, "com.ex.a")) == 0x1107, "CheckIn com portas");
    r = launch_msg(launch_data_new_string("CheckIn"));
    CHECK(checkins == 2 && launch_data_get_machport(launch_data_dict_lookup(launch_data_dict_lookup(r, "MachServices"), "com.ex.b")) == 0x1203, "segundo CheckIn reusa as portas");
    ld q = launch_data_alloc(1);
    launch_data_dict_insert(q, launch_data_new_string("com.ex.job"), "GetJob");
    r = launch_msg(q);
    CHECK(launch_data_get_integer(launch_data_dict_lookup(r, "PID")) == 4242, "GetJob do próprio job");
    CHECK(launch_data_get_errno(launch_msg(launch_data_new_string("GetJobs"))) == 1, "GetJobs EPERM");
    int *fds; size_t cnt;
    CHECK(launch_activate_socket("Listeners", &fds, &cnt) == 2 && cnt == 0, "activate_socket sem Sockets");
    void *t = vproc_transaction_begin(0);
    CHECK(vproc_transaction_count() == 1, "transação");
    vproc_transaction_end(0, t);
    long long v = 0;
    CHECK(vproc_transaction_count() == 0 && vproc_swap_integer(0, 14, 0, &v) == 0 && v == 1, "IS_MANAGED");
    printf("launchdata: %d/%d ok\n", oks, oks + fails);
    return fails != 0;
}
