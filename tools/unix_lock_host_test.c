/* O recv bloqueante não pode segurar a trava das outras syscalls.
 * Uma thread dorme no read; a outra toma a trava e escreve. Se o read
 * segurou a trava, este teste não termina. */
#include "host_unix.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int slots[8];
static int slot_of[8];
static pthread_mutex_t big = PTHREAD_MUTEX_INITIALIZER;
static int fds[2];
static int got;

static int alloc_fd(int slot, int nb, int clo) {
    (void)nb; (void)clo;
    for (int i = 0; i < 8; i++) if (!slots[i]) { slots[i] = 1; slot_of[i] = slot; return i; }
    return -24;
}
static int fd_slot(int fd) { return slots[fd] ? slot_of[fd] : -1; }
static void set_slot(int fd, int slot) { slot_of[fd] = slot; }
static void drop_big(void) { pthread_mutex_unlock(&big); }
static void take_big(void) { pthread_mutex_lock(&big); }

static void *reader(void *arg) {
    (void)arg;
    char buf[8];
    pthread_mutex_lock(&big);
    long n = host_unix_read(fds[0], buf, 4);
    pthread_mutex_unlock(&big);
    got = n == 4 && memcmp(buf, "ping", 4) == 0;
    return NULL;
}

int main(void) {
    host_unix_hooks hk = { alloc_fd, fd_slot, set_slot, NULL, NULL, NULL, NULL, drop_big, take_big };
    host_unix_set_hooks(&hk);
    if (host_unix_op(9, 1, 0, (unsigned long long)fds) != 0) return 2;
    pthread_t th;
    pthread_create(&th, NULL, reader, NULL);
    usleep(200000);
    pthread_mutex_lock(&big);
    long w = host_unix_write(fds[1], "ping", 4);
    pthread_mutex_unlock(&big);
    pthread_join(th, NULL);
    if (w != 4 || !got) { fprintf(stderr, "falhou write=%ld got=%d\n", w, got); return 1; }
    printf("unix-lock ok\n");
    return 0;
}
