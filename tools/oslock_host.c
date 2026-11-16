/*
 * mOS — teste de host do os_unfair_lock / os_unfair_recursive_lock / os_sync_wait_on_address do libSystem
 * (userland/libsystem/oslock.c) com threads reais e o futex do Linux.
 *
 *   gcc -O1 -Wall -DMOS_LIBSYS_SYSCALL -Iuserland/libsystem -c userland/libsystem/oslock.c -o /tmp/oslock_ls.o
 *   gcc -O1 -Wall -pthread tools/oslock_host.c /tmp/oslock_ls.o -o /tmp/oslock_host && /tmp/oslock_host
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#define MOS_SSIZE_T_DEFINED
#include "../kernel/include/syscall.h"

void os_unfair_lock_lock(void *l);
void os_unfair_lock_unlock(void *l);
int os_unfair_lock_trylock(void *l);
void os_unfair_recursive_lock_lock(void *l);
void os_unfair_recursive_lock_unlock(void *l);
int os_unfair_recursive_lock_trylock(void *l);
int os_sync_wait_on_address(void *a, uint64_t v, size_t n, uint32_t f);
int os_sync_wait_on_address_with_timeout(void *a, uint64_t v, size_t n, uint32_t f, uint32_t c, uint64_t t);
int os_sync_wake_by_address_any(void *a, size_t n, uint32_t f);
int os_sync_wake_by_address_all(void *a, size_t n, uint32_t f);

static __thread int terr;
int *__error(void) { return &terr; }
long cx_syscall(long n, long a, long b, long c, long d, long e) { (void)a; (void)b; (void)c; (void)d; (void)e; return n == SYS_GETTID ? syscall(SYS_gettid) : -38; }
int cx_fdprintf(int fd, const char *fmt, ...) { (void)fd; (void)fmt; return 0; }
unsigned long long mach_absolute_time(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (unsigned long long)t.tv_sec * 1000000000ull + t.tv_nsec; }
int cx_futex_wait(volatile int *w, int v, long ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000}, *p = ms < 0 ? NULL : &ts;
    long r = syscall(SYS_futex, w, FUTEX_WAIT, v, p, NULL, 0);
    return r == 0 ? 0 : errno == EAGAIN ? -35 : errno == ETIMEDOUT ? -60 : errno == EINTR ? -4 : 0;
}
int cx_futex_wake(volatile int *w, int n) { return (int)syscall(SYS_futex, w, FUTEX_WAKE, n, NULL, NULL, 0); }

static int fails, oks;
#define CHECK(c, ...) do { if (c) oks++; else { fails++; printf("FALHA %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static uint32_t L;
volatile long counter, rcounter;   /* um contador por trava: as duas fases rodam ao mesmo tempo em threads diferentes */
static struct { uint32_t lock, count; } R;
static void *worker(void *x) {
    (void)x;
    for (int i = 0; i < 200000; i++) { os_unfair_lock_lock(&L); counter++; os_unfair_lock_unlock(&L); }
    for (int i = 0; i < 20000; i++) { os_unfair_recursive_lock_lock(&R); os_unfair_recursive_lock_lock(&R); rcounter++; os_unfair_recursive_lock_unlock(&R); os_unfair_recursive_lock_unlock(&R); }
    return 0;
}
static volatile uint32_t word;
static void *waker(void *x) { (void)x; usleep(50000); word = 1; os_sync_wake_by_address_all((void *)&word, 4, 0); return 0; }

int main(void) {
    pthread_t t[4];
    for (int i = 0; i < 4; i++) pthread_create(&t[i], 0, worker, 0);
    for (int i = 0; i < 4; i++) pthread_join(t[i], 0);
    CHECK(counter == 4 * 200000 && rcounter == 4 * 20000 && L == 0 && R.lock == 0, "contadores %ld %ld", counter, rcounter);
    CHECK(os_unfair_lock_trylock(&L) && !os_unfair_lock_trylock(&L), "trylock");
    os_unfair_lock_unlock(&L);
    CHECK(os_unfair_recursive_lock_trylock(&R) && os_unfair_recursive_lock_trylock(&R) && R.count == 1, "recursivo trylock");
    os_unfair_recursive_lock_unlock(&R); os_unfair_recursive_lock_unlock(&R);
    CHECK(R.lock == 0, "recursivo solto");
    pthread_t w;
    pthread_create(&w, 0, waker, 0);
    CHECK(os_sync_wait_on_address((void *)&word, 0, 4, 0) == 0 && word == 1, "os_sync wait/wake");
    pthread_join(w, 0);
    unsigned long long t0 = mach_absolute_time();
    CHECK(os_sync_wait_on_address_with_timeout((void *)&word, 1, 4, 0, 32, 30000000) == -1 && terr == 60 && mach_absolute_time() - t0 >= 29000000, "timeout");
    CHECK(os_sync_wait_on_address((void *)&word, 0, 4, 0) == 0, "valor já diferente");
    CHECK(os_sync_wait_on_address((void *)&word, 0, 3, 0) == -1 && terr == 22, "tamanho inválido");
    CHECK(os_sync_wake_by_address_any((void *)&word, 4, 0) == -1 && terr == 2, "wake sem ninguém ENOENT");
    printf("oslock: %d/%d ok\n", oks, oks + fails);
    return fails != 0;
}
