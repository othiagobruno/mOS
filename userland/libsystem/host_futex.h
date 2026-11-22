/*
 * mOS — futex e tid do host para os testes de host (-DMOS_HOST_TEST) da libdispatch e do runtime de blocos.
 * Linux: futex(2) e gettid. macOS (tools/build-mac.sh): os_sync_wait_on_address (macOS 14.4+) e pthread_threadid_np.
 * No mOS de verdade nada disto entra: lá são as syscalls do kernel.
 */
#ifndef MOS_HOST_FUTEX_H
#define MOS_HOST_FUTEX_H
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#if defined(__APPLE__)
#include <os/os_sync_wait_on_address.h>
/* dorme enquanto *a == v; ms < 0 = sem prazo */
static inline void host_futex_wait(volatile int *a, int v, long ms) {
    if (ms < 0) os_sync_wait_on_address((void *)a, (uint64_t)(uint32_t)v, 4, OS_SYNC_WAIT_ON_ADDRESS_NONE);
    else os_sync_wait_on_address_with_timeout((void *)a, (uint64_t)(uint32_t)v, 4, OS_SYNC_WAIT_ON_ADDRESS_NONE,
                                              OS_CLOCK_MACH_ABSOLUTE_TIME, (uint64_t)ms * 1000000ull);
}
static inline void host_futex_wake(volatile int *a, int n) {
    if (n == 1) os_sync_wake_by_address_any((void *)a, 4, OS_SYNC_WAKE_BY_ADDRESS_NONE);
    else os_sync_wake_by_address_all((void *)a, 4, OS_SYNC_WAKE_BY_ADDRESS_NONE);
}
static inline int host_tid(void) { uint64_t t = 0; pthread_threadid_np(0, &t); return (int)t; }
static inline int host_is_main(void) { return pthread_main_np() != 0; }
#else
#include <linux/futex.h>
#include <sys/syscall.h>
static inline void host_futex_wait(volatile int *a, int v, long ms) {
    struct timespec t, *tp = 0;
    if (ms >= 0) { t.tv_sec = ms / 1000; t.tv_nsec = (ms % 1000) * 1000000L; tp = &t; }
    syscall(SYS_futex, a, FUTEX_WAIT_PRIVATE, v, tp, 0, 0);
}
static inline void host_futex_wake(volatile int *a, int n) { syscall(SYS_futex, a, FUTEX_WAKE_PRIVATE, n, 0, 0, 0); }
static inline int host_tid(void) { return (int)syscall(SYS_gettid); }
static inline int host_is_main(void) { return host_tid() == (int)getpid(); }
#endif
#endif
