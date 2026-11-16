/*
 * mOS libSystem — camada de plataforma da libdispatch (dispatch.c, dispatch_source.c, dispatch_data.c).
 * No mOS: syscalls do kernel (futex, gettid, relógio) e o pthread do libSystem.
 * Com -DMOS_HOST_TEST: Linux (futex, pthread, clock_gettime), para o teste de host tools/dispatch_hosttest.c.
 */
#ifndef MOS_DISPATCH_PLAT_H
#define MOS_DISPATCH_PLAT_H

#ifdef MOS_HOST_TEST
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <linux/futex.h>
#include <sys/syscall.h>
typedef unsigned long long u64;
typedef long long i64;
typedef unsigned u32;
typedef unsigned char u8;
static inline int dp_tid(void) { return (int)syscall(SYS_gettid); }
static inline u64 dp_now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (u64)t.tv_sec * 1000000000ull + (u64)t.tv_nsec;
}
static inline u64 dp_wall_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return (u64)t.tv_sec * 1000000000ull + (u64)t.tv_nsec;
}
/* dorme enquanto *a == v; ms < 0 = sem prazo */
static inline void dp_wait(volatile int *a, int v, long ms) {
    struct timespec t, *tp = 0;
    if (ms >= 0) { t.tv_sec = ms / 1000; t.tv_nsec = (ms % 1000) * 1000000L; tp = &t; }
    syscall(SYS_futex, a, FUTEX_WAIT_PRIVATE, v, tp, 0, 0);
}
static inline void dp_wake(volatile int *a, int n) { syscall(SYS_futex, a, FUTEX_WAKE_PRIVATE, n, 0, 0, 0); }
static inline int dp_thread(void *(*f)(void *), void *arg) {
    pthread_t t;
    int r = pthread_create(&t, 0, f, arg);
    if (!r) pthread_detach(t);
    return r;
}
static inline int dp_ncpu(void) { long n = sysconf(_SC_NPROCESSORS_ONLN); return n > 0 ? (int)n : 1; }
static inline int dp_is_main(void) { return dp_tid() == (int)getpid(); }
static inline void dp_yield(void) { sched_yield(); }
static inline void dp_sleep_ms(long ms) { struct timespec t = {ms / 1000, (ms % 1000) * 1000000L}; nanosleep(&t, 0); }
void *_Block_copy(const void *b);
void _Block_release(const void *b);
static inline void dp_main_wakeup(void) {}
static inline int dp_write_err(const char *s) { return (int)write(2, s, strlen(s)); }
#else
#include "sys.h"
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
size_t strlen(const char *s);
char *strdup(const char *s);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int strcmp(const char *a, const char *b);
__attribute__((noreturn)) void abort(void);
void *memmove(void *d, const void *s, size_t n);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
int cx_futex_wait(volatile int *w, int expected, long ms);   /* extra.c */
int cx_futex_wake(volatile int *w, int n);
long write(int fd, const void *b, size_t n);
extern int cx_main_tid;                                     /* nsurl.c: a thread principal do processo */
static inline int dp_tid(void) { return (int)SYS0(SYS_GETTID); }
u64 mach_absolute_time(void);
static inline u64 dp_now_ns(void) { return mach_absolute_time(); }
typedef struct { long tv_sec, tv_nsec; } dp_timespec_t;
int clock_gettime(int clk, void *ts);
static inline u64 dp_wall_ns(void) { dp_timespec_t t; clock_gettime(0, &t); return (u64)t.tv_sec * 1000000000ull + (u64)t.tv_nsec; }
static inline void dp_wait(volatile int *a, int v, long ms) { cx_futex_wait(a, v, ms); }
static inline void dp_wake(volatile int *a, int n) { cx_futex_wake(a, n); }
static inline int dp_thread(void *(*f)(void *), void *arg) {
    unsigned long t;
    int r = pthread_create(&t, 0, f, arg);
    if (!r) pthread_detach(t);
    return r;
}
static inline int dp_ncpu(void) { long c = SYS0(SYS_CPUS); return c > 0 ? (int)c : 1; }
static inline int dp_is_main(void) { return !cx_main_tid || dp_tid() == cx_main_tid; }
static inline void dp_yield(void) { SYS0(SYS_YIELD); }
static inline void dp_sleep_ms(long ms) { SYS1(SYS_SLEEP, ms); }
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void *CFRunLoopGetMain(void);
void CFRunLoopWakeUp(void *rl);
static inline void dp_main_wakeup(void) { CFRunLoopWakeUp(CFRunLoopGetMain()); }
static inline int dp_write_err(const char *s) { return (int)write(2, s, strlen(s)); }
#endif

/* trava curta (Drepper, "Futexes Are Tricky"): 0 livre, 1 travada, 2 travada com espera */
typedef volatile int dp_lock_t;
static inline void dp_lock(dp_lock_t *w) {
    int c = __sync_val_compare_and_swap(w, 0, 1);
    if (!c) return;
    if (c != 2) c = __sync_lock_test_and_set(w, 2);
    while (c) { dp_wait(w, 2, -1); c = __sync_lock_test_and_set(w, 2); }
}
static inline void dp_unlock(dp_lock_t *w) { if (__sync_fetch_and_sub(w, 1) != 1) { *w = 0; dp_wake(w, 1); } }

/* ABI de Blocks: o invoke recebe o próprio bloco e os argumentos */
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, ...); } dp_block_t;
#define DP_RUN_BLOCK(b) ((void (*)(void *))((dp_block_t *)(b))->invoke)(b)

/* objeto da libdispatch: o isa ObjC (OS_dispatch_*) no 1º word, como no macOS */
#define DP_MAGIC 0x4450u
enum { DK_QUEUE = 1, DK_GROUP, DK_SEMA, DK_SOURCE, DK_ATTR, DK_IO, DK_DATA };
typedef struct dp_obj {
    void *isa;
    unsigned short magic, kind;
    volatile int ref;              /* referências além da de criação; < 0: objeto estático */
    void *ctx;
    void (*fin)(void *);
    struct dp_queue *tq;           /* fila de destino (filas, fontes, io) */
    volatile int susp;             /* contagem de suspensão */
    volatile int inactive;         /* criado inativo: só dispatch_activate libera */
    void (*dispose)(struct dp_obj *);
} dp_obj_t;

struct dp_queue;
void cx_dispatch_retain(void *o);
void cx_dispatch_release(void *o);
int cx_dispatch_is_obj(void *o);
void dp_obj_init(void *o, void *isa, int kind, void (*dispose)(dp_obj_t *));
void *dp_default_target(void);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
u64 cx_dispatch_deadline_ns(u64 t);
/* o timer da libdispatch (dispatch.c): chama cb(arg) em `when` (ns, base dp_now_ns); devolve um id para cancelar */
typedef struct dp_timer dp_timer_t;
dp_timer_t *dp_timer_add(u64 when, void (*cb)(void *), void *arg);
int dp_timer_cancel(dp_timer_t *t);
#endif
