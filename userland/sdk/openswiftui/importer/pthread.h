/*
 * mOS SDK (importador) — pthread.h mínimo para os cabeçalhos C que o Swift importa (a libc do macOS que vem com o
 * zig não funciona no modo de módulos do importador). Tipos com o layout do macOS x86_64; funções do libSystem.
 */
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct _opaque_pthread_t *pthread_t;
typedef struct { long __sig; char __opaque[56]; } pthread_mutex_t;
typedef struct { long __sig; char __opaque[8]; } pthread_mutexattr_t;
typedef struct { long __sig; char __opaque[40]; } pthread_cond_t;
typedef struct { long __sig; char __opaque[8]; } pthread_condattr_t;
typedef struct { long __sig; char __opaque[192]; } pthread_rwlock_t;
typedef struct { long __sig; char __opaque[56]; } pthread_attr_t;
typedef struct { long __sig; char __opaque[8]; } pthread_once_t;
typedef unsigned long pthread_key_t;

pthread_t pthread_self(void);
int pthread_equal(pthread_t a, pthread_t b);
int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a);
int pthread_mutex_lock(pthread_mutex_t *m);
int pthread_mutex_trylock(pthread_mutex_t *m);
int pthread_mutex_unlock(pthread_mutex_t *m);
int pthread_mutex_destroy(pthread_mutex_t *m);
int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a);
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
int pthread_cond_signal(pthread_cond_t *c);
int pthread_cond_broadcast(pthread_cond_t *c);
int pthread_cond_destroy(pthread_cond_t *c);
int pthread_key_create(pthread_key_t *k, void (*dtor)(void *));
void *pthread_getspecific(pthread_key_t k);
int pthread_setspecific(pthread_key_t k, const void *v);
int pthread_main_np(void);
#ifdef __cplusplus
}
#endif