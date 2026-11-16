/*
 * mOS — teste de host do os_activity e dos bootstrap_* antigos do libSystem (userland/libsystem/osactivity.c).
 *   gcc -O1 -Wall -DMOS_LIBSYS_SYSCALL -Iuserland/libsystem -c userland/libsystem/osactivity.c -o /tmp/osactivity_ls.o
 *   gcc -O1 -Wall -pthread tools/osactivity_host.c /tmp/osactivity_ls.o -o /tmp/osactivity_host && /tmp/osactivity_host
 */
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
extern char _os_activity_none, _os_activity_current;
void *os_activity_create(void *, const char *, void *, unsigned);
void os_activity_scope_enter(void *, uint64_t *);
void os_activity_scope_leave(uint64_t *);
uint64_t os_activity_get_identifier(void *, uint64_t *);
void os_activity_apply_f(void *, void *, void (*)(void *));
int bootstrap_parent(unsigned, unsigned *);
int bootstrap_subset(unsigned, unsigned, unsigned *);
int _dyld_bind_fully_image_containing_address(const void *);
unsigned bootstrap_port = 0x303;
static uint64_t next = 100;
int mach_generate_activity_id(unsigned t, int c, uint64_t *id) { (void)t; (void)c; *id = __sync_fetch_and_add(&next, 1); return 0; }
long cx_syscall(long n, long a, long b, long c, long d, long e) { (void)n; (void)a; (void)b; (void)c; (void)d; (void)e; return 0; }
static int fails, oks;
#define CHECK(c, m) do { if (c) oks++; else { fails++; printf("FALHA %d: %s\n", __LINE__, m); } } while (0)
static void *other(void *a) { return (void *)(uintptr_t)os_activity_get_identifier(&_os_activity_current, 0); }
static uint64_t seen;
static void fn(void *ctx) { seen = os_activity_get_identifier(&_os_activity_current, 0); (void)ctx; }
int main(void) {
    void *a = os_activity_create(0, "abrir", &_os_activity_current, 0);
    uint64_t ida = os_activity_get_identifier(a, 0), st[2], st2[2], par;
    CHECK(ida >= 100 && os_activity_get_identifier(&_os_activity_current, 0) == 0, "criada, nada atual");
    os_activity_scope_enter(a, st);
    CHECK(os_activity_get_identifier(&_os_activity_current, 0) == ida, "enter");
    void *b = os_activity_create(0, "filha", &_os_activity_current, 0);
    CHECK(os_activity_get_identifier(b, &par) != ida && par == ida, "pai herdado");
    void *c = os_activity_create(0, "solta", &_os_activity_current, 1);
    CHECK(os_activity_get_identifier(c, &par) && par == 0, "DETACHED sem pai");
    pthread_t t; void *r;
    pthread_create(&t, 0, other, 0); pthread_join(t, &r);
    CHECK((uintptr_t)r == 0, "outra thread não vê a atividade");
    os_activity_scope_enter(b, st2);
    os_activity_scope_leave(st2);
    CHECK(os_activity_get_identifier(&_os_activity_current, 0) == ida, "leave restaura");
    os_activity_scope_leave(st);
    CHECK(os_activity_get_identifier(&_os_activity_current, 0) == 0, "leave final");
    os_activity_apply_f(b, 0, fn);
    CHECK(seen == os_activity_get_identifier(b, 0) && os_activity_get_identifier(&_os_activity_current, 0) == 0, "apply_f");
    unsigned p = 0, sub = 1;
    CHECK(bootstrap_parent(0x303, &p) == 0 && p == 0x303 && bootstrap_subset(0x303, 0, &sub) == 1100 && sub == 0, "bootstrap antigos");
    CHECK(_dyld_bind_fully_image_containing_address(main) == 1, "bind_fully");
    printf("osactivity: %d/%d ok\n", oks, oks + fails);
    return fails != 0;
}
