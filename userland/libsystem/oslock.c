/*
 * mOS libSystem — os_unfair_lock, os_unfair_recursive_lock e os_sync_wait_on_address (libsystem_platform/os/os_sync),
 * sobre o futex do kernel (SYS_FUTEX_WAIT/WAKE). docs/darwin-bsd-mach.md, fase F7.
 *
 * os_unfair_lock (u32): 0 = livre; senão o tid do dono, com o bit 31 ligado quando há quem espere. Sem disputa
 * travar e destravar não entram no kernel; com disputa, quem espera dorme no futex da palavra (antes girava com
 * SYS_YIELD). Destravar sem ser o dono ou travar de novo pela mesma thread aborta, como o "BUG IN CLIENT OF
 * LIBPLATFORM" do macOS: o PartitionAlloc e o Swift contam com isso para achar erros de trava.
 */
#include "sys.h"

int cx_futex_wait(volatile int *w, int expected, long ms);
int cx_futex_wake(volatile int *w, int n);
int cx_fdprintf(int fd, const char *fmt, ...);
void abort(void);

#define WAITERS 0x80000000u
static u32 me(void) { return (u32)SYS0(SYS_GETTID) & 0x7fffffffu; }
__attribute__((noreturn)) static void client_bug(const char *what) {
    cx_fdprintf(2, "BUG IN CLIENT OF LIBPLATFORM: %s\n", what);
    abort();
    for (;;) {}
}

static void ul_lock(volatile u32 *w, u32 self) {
    u32 c = __sync_val_compare_and_swap(w, 0u, self);
    if (!c) return;
    if ((c & ~WAITERS) == self) client_bug("Trying to recursively lock an os_unfair_lock");
    for (;;) {
        c = *w;
        if (!c) { if (__sync_bool_compare_and_swap(w, 0u, self | WAITERS)) return; continue; }   /* pode haver outros: fica marcado */
        if (!(c & WAITERS) && !__sync_bool_compare_and_swap(w, c, c | WAITERS)) continue;
        cx_futex_wait((volatile int *)w, (int)(c | WAITERS), -1);
    }
}
static void ul_unlock(volatile u32 *w, u32 self) {
    u32 c = __atomic_exchange_n(w, 0u, __ATOMIC_SEQ_CST);   /* release: as escritas protegidas não passam daqui */
    if ((c & ~WAITERS) != self) {
        if (!c) client_bug("Unlock of an os_unfair_lock not locked");
        client_bug("Unlock of an os_unfair_lock not owned by current thread");
    }
    if (c & WAITERS) cx_futex_wake((volatile int *)w, 1);
}
void os_unfair_lock_lock(void *l) { ul_lock(l, me()); }
void os_unfair_lock_lock_with_options(void *l, u32 opts) { (void)opts; ul_lock(l, me()); }
void os_unfair_lock_unlock(void *l) { ul_unlock(l, me()); }
int os_unfair_lock_trylock(void *l) { return __sync_bool_compare_and_swap((volatile u32 *)l, 0u, me()); }
void os_unfair_lock_assert_owner(const void *l) {
    if ((*(const volatile u32 *)l & ~WAITERS) != me()) client_bug("Assertion failed: Lock unexpectedly not owned by current thread");
}
void os_unfair_lock_assert_not_owner(const void *l) {
    if ((*(const volatile u32 *)l & ~WAITERS) == me()) client_bug("Assertion failed: Lock unexpectedly owned by current thread");
}
/* variantes "_no_tsd"/de dados do libplatform que alguns binários importam */
void os_unfair_lock_lock_no_tsd(void *l, u32 opts, u32 port) { (void)opts; (void)port; ul_lock(l, me()); }
void os_unfair_lock_unlock_no_tsd(void *l, u32 port) { (void)port; ul_unlock(l, me()); }

/* os_unfair_recursive_lock {os_unfair_lock ourl_lock; u32 ourl_count} */
typedef struct { volatile u32 lock; u32 count; } ur_lock_t;
void os_unfair_recursive_lock_lock_with_options(void *l, u32 opts) {
    (void)opts;
    ur_lock_t *r = l;
    u32 self = me();
    if ((r->lock & ~WAITERS) == self) { r->count++; return; }
    ul_lock(&r->lock, self);
    r->count = 0;
}
void os_unfair_recursive_lock_lock(void *l) { os_unfair_recursive_lock_lock_with_options(l, 0); }
int os_unfair_recursive_lock_trylock(void *l) {
    ur_lock_t *r = l;
    u32 self = me();
    if ((r->lock & ~WAITERS) == self) { r->count++; return 1; }
    if (!__sync_bool_compare_and_swap(&r->lock, 0u, self)) return 0;
    r->count = 0;
    return 1;
}
void os_unfair_recursive_lock_unlock(void *l) {
    ur_lock_t *r = l;
    u32 self = me();
    if ((r->lock & ~WAITERS) != self) client_bug("Unlock of an os_unfair_recursive_lock not owned by current thread");
    if (r->count) { r->count--; return; }
    ul_unlock(&r->lock, self);
}
int os_unfair_recursive_lock_tryunlock4objc(void *l) {
    ur_lock_t *r = l;
    if ((r->lock & ~WAITERS) != me()) return 0;
    os_unfair_recursive_lock_unlock(l);
    return 1;
}
void os_unfair_recursive_lock_unlock_forked_child(void *l) { ur_lock_t *r = l; r->lock = 0; r->count = 0; }
void os_unfair_recursive_lock_assert_owner(const void *l) { os_unfair_lock_assert_owner(l); }
void os_unfair_recursive_lock_assert_not_owner(const void *l) { os_unfair_lock_assert_not_owner(l); }
int os_unfair_recursive_lock_owned(const void *l) { return (*(const volatile u32 *)l & ~WAITERS) == me(); }

/* ---- os_sync_wait_on_address (macOS 14.4). size 4 ou 8; o futex olha os 32 bits baixos (little-endian) e o
 * laço confere o valor inteiro. OS_SYNC_WAIT_ON_ADDRESS_SHARED (1) vale também: o futex do kernel acorda entre
 * processos numa página MAP_SHARED. Devolve quantos ainda esperam (estimativa: 0) ou -1 com errno. */
#define OS_CLOCK_MACH_ABSOLUTE_TIME 32
unsigned long long mach_absolute_time(void);
static int bad_args(void *addr, size_t size, u32 flags) {
    if (!addr || (size != 4 && size != 8) || ((u64)addr & (size - 1)) || (flags & ~1u)) { cx_errno = CX_EINVAL; return 1; }
    return 0;
}
static int differs(void *addr, u64 value, size_t size) {
    return size == 4 ? *(volatile u32 *)addr != (u32)value : *(volatile u64 *)addr != value;
}
static int wait_until(void *addr, u64 value, size_t size, u64 deadline_ns) {
    for (;;) {
        if (differs(addr, value, size)) return 0;
        long ms = -1;
        if (deadline_ns) {
            u64 now = mach_absolute_time();
            if (now >= deadline_ns) { cx_errno = 60; return -1; }   /* ETIMEDOUT */
            ms = (long)((deadline_ns - now + 999999) / 1000000);
        }
        int r = cx_futex_wait((volatile int *)addr, (int)(u32)value, ms);
        if (r == -4) { cx_errno = 4; return -1; }                 /* EINTR */
        if (r == -60 && deadline_ns && mach_absolute_time() >= deadline_ns) { cx_errno = 60; return -1; }
    }
}
int os_sync_wait_on_address(void *addr, u64 value, size_t size, u32 flags) {
    if (bad_args(addr, size, flags)) return -1;
    return wait_until(addr, value, size, 0);
}
int os_sync_wait_on_address_with_timeout(void *addr, u64 value, size_t size, u32 flags, u32 clockid, u64 timeout_ns) {
    if (bad_args(addr, size, flags)) return -1;
    if (clockid != OS_CLOCK_MACH_ABSOLUTE_TIME) { cx_errno = CX_EINVAL; return -1; }
    return wait_until(addr, value, size, mach_absolute_time() + (timeout_ns ? timeout_ns : 1));
}
int os_sync_wait_on_address_with_deadline(void *addr, u64 value, size_t size, u32 flags, u32 clockid, u64 deadline) {
    if (bad_args(addr, size, flags)) return -1;
    if (clockid != OS_CLOCK_MACH_ABSOLUTE_TIME) { cx_errno = CX_EINVAL; return -1; }
    return wait_until(addr, value, size, deadline ? deadline : 1);
}
static int wake(void *addr, size_t size, u32 flags, int n) {
    if (bad_args(addr, size, flags)) return -1;
    int r = cx_futex_wake((volatile int *)addr, n);
    if (r <= 0) { cx_errno = CX_ENOENT; return -1; }               /* ninguém esperando: ENOENT, como no macOS */
    return 0;
}
int os_sync_wake_by_address_any(void *addr, size_t size, u32 flags) { return wake(addr, size, flags, 1); }
int os_sync_wake_by_address_all(void *addr, size_t size, u32 flags) { return wake(addr, size, flags, 0x7fffffff); }
