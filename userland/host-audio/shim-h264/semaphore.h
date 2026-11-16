#pragma once
#ifdef __cplusplus
extern "C" {
#endif
typedef int sem_t;
#define SEM_FAILED ((sem_t *)-1)
struct timespec;
int sem_init(sem_t *s, int pshared, unsigned value);
int sem_destroy(sem_t *s);
int sem_wait(sem_t *s);
int sem_trywait(sem_t *s);
int sem_post(sem_t *s);
int sem_timedwait(sem_t *s, const struct timespec *ts);
sem_t *sem_open(const char *name, int oflag, ...);
int sem_close(sem_t *s);
int sem_unlink(const char *name);
#ifdef __cplusplus
}
#endif
