#pragma once
#include "sys/types.h"
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
/* tamanhos do Darwin (o libSystem do mOS implementa a ABI do macOS) */
typedef struct _opaque_pthread_t *pthread_t;
typedef struct { long sig; char opaque[56]; } pthread_attr_t;
typedef struct { long sig; char opaque[56]; } pthread_mutex_t;
typedef struct { long sig; char opaque[8]; } pthread_mutexattr_t;
typedef struct { long sig; char opaque[40]; } pthread_cond_t;
typedef struct { long sig; char opaque[8]; } pthread_condattr_t;
#define PTHREAD_CREATE_JOINABLE 1
#define PTHREAD_SCOPE_SYSTEM 1   /* valores do macOS */
#define SCHED_OTHER 1
#define SCHED_RR 2
#define SCHED_FIFO 4
struct sched_param { int sched_priority; char opaque[4]; };
int pthread_attr_setscope(pthread_attr_t *a, int scope);
int pthread_attr_setschedpolicy(pthread_attr_t *a, int policy);
int pthread_attr_setschedparam(pthread_attr_t *a, const struct sched_param *p);
#define PTHREAD_MUTEX_RECURSIVE 2
int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg);
int pthread_join(pthread_t t, void **r);
int pthread_detach(pthread_t t);
pthread_t pthread_self(void);
int pthread_attr_init(pthread_attr_t *a);
int pthread_attr_destroy(pthread_attr_t *a);
int pthread_attr_setdetachstate(pthread_attr_t *a, int s);
int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a);
int pthread_mutex_destroy(pthread_mutex_t *m);
int pthread_mutex_lock(pthread_mutex_t *m);
int pthread_mutex_trylock(pthread_mutex_t *m);
int pthread_mutex_unlock(pthread_mutex_t *m);
int pthread_mutexattr_init(pthread_mutexattr_t *a);
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int t);
int pthread_mutexattr_destroy(pthread_mutexattr_t *a);
int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a);
int pthread_cond_destroy(pthread_cond_t *c);
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *ts);
int pthread_cond_signal(pthread_cond_t *c);
int pthread_cond_broadcast(pthread_cond_t *c);
#ifdef __cplusplus
}
#endif
