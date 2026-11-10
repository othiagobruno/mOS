/* config.h do liblzma no ELF freestanding.
 * O config.h gerado pelo xz fica em third_party/xz e serve a dylib.
 * Aqui só se desligam threads, clock e sysconf: o kshim não tem pthread. */
#ifndef MOS_XZ_CONFIG_H
#define MOS_XZ_CONFIG_H
#include_next <config.h>
#undef MYTHREAD_POSIX
#undef MYTHREAD_ENABLED
#undef HAVE_CLOCK_GETTIME
#undef HAVE_CLOCK_MONOTONIC
#undef HAVE_PTHREAD_CONDATTR_SETCLOCK
#undef HAVE_PTHREAD_PRIO_INHERIT
#undef TUKLIB_CPUCORES_SCHED_GETAFFINITY
#undef TUKLIB_PHYSMEM_SYSCONF
#endif
