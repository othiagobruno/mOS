/*
 * mOS — teste de host do IPC System V do libSystem (userland/libsystem/sysv_ipc.c), entre processos de verdade (fork)
 * no Linux: arquivos MAP_SHARED em /tmp/.sysv-ipc e futex compartilhado, como no kernel do mOS.
 *
 *   gcc -O1 -Wall -DMOS_LIBSYS_SYSCALL -Dopen=h_open -Iuserland/libsystem \
 *       -c userland/libsystem/sysv_ipc.c -o /tmp/sysv_ls.o
 *   gcc -O1 -Wall tools/sysv_ipc_host.c /tmp/sysv_ls.o -o /tmp/sysv_ipc_host && /tmp/sysv_ipc_host
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <linux/futex.h>
#include <time.h>
#include <errno.h>
#include <sched.h>

int semget(int key, int nsems, int flg);
int semop(int id, void *ops, size_t n);
int semctl(int id, int num, int cmd, ...);
int shmget(int key, size_t size, int flg);
void *shmat(int id, const void *addr, int flg);
int shmdt(const void *addr);
int shmctl(int id, int cmd, void *buf);
int msgget(int key, int flg);
int msgsnd(int id, const void *m, size_t sz, int flg);
ssize_t msgrcv(int id, void *m, size_t sz, long type, int flg);
int msgctl(int id, int cmd, void *buf);
void cx_sysv_exit(void);

/* ---- o que o libSystem pede, no Linux */
static int err;
int *__error(void) { return &err; }
int h_open(const char *p, int fl, ...) {   /* flags O_* do macOS -> Linux */
    va_list ap; va_start(ap, fl); int mode = va_arg(ap, int); va_end(ap);
    int l = fl & 3;
    if (fl & 0x200) l |= O_CREAT;
    if (fl & 0x800) l |= O_EXCL;
    int r = open(p, l, mode);
    if (r < 0) err = errno == EEXIST ? 17 : errno == ENOENT ? 2 : errno;
    return r;
}
long cx_syscall(long n, long a, long b, long c, long d, long e) {
    (void)a; (void)b; (void)c; (void)d; (void)e;
    if (n == 99 || n == 100) return geteuid();   /* SYS_GETUID/GETEUID */
    if (n == 101 || n == 102) return getegid();
    if (n == 4) { sched_yield(); return 0; }       /* SYS_YIELD */
    return -38;
}
int cx_futex_wait(volatile int *w, int expected, long ms) {
    long r = syscall(SYS_futex, w, FUTEX_WAIT, expected, NULL, NULL, 0);
    return r < 0 && errno == EINTR ? -4 : 0;
    (void)ms;
}
int cx_futex_wake(volatile int *w, int n) { return (int)syscall(SYS_futex, w, FUTEX_WAKE, n, NULL, NULL, 0); }

static int fails, oks;
#define CHECK(c, ...) do { if (c) oks++; else { fails++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf(" (errno %d)\n", err); } } while (0)
typedef struct { unsigned short num; short op, flg; } sb_t;

int main(void) {
    int key = 0x4d4f5300 | (getpid() & 0xff);
    /* ---- semáforos */
    int s = semget(key, 3, 0x200 | 0600);
    CHECK(s > 0, "semget cria");
    CHECK(semget(key, 3, 0x200 | 0x400 | 0600) == -1 && err == 17, "IPC_EXCL EEXIST");
    CHECK(semget(key, 2, 0) == s, "semget acha pela chave");
    CHECK(semget(key, 9, 0) == -1 && err == 22, "nsems maior EINVAL");
    CHECK(semget(key + 0x1000000, 1, 0) == -1 && err == 2, "chave inexistente ENOENT");
    CHECK(semctl(s, 0, 8, (void *)(long)2) == 0 && semctl(s, 0, 5) == 2, "SETVAL/GETVAL");
    sb_t dec = {0, -1, 0x800};
    CHECK(semop(s, &dec, 1) == 0 && semop(s, &dec, 1) == 0 && semctl(s, 0, 5) == 0, "semop -1 duas vezes");
    CHECK(semop(s, &dec, 1) == -1 && err == 35, "IPC_NOWAIT EAGAIN");
    sb_t two[2] = {{1, 1, 0}, {0, -1, 0x800}};
    CHECK(semop(s, two, 2) == -1 && semctl(s, 1, 5) == 0, "tudo ou nada");
    CHECK(semctl(s, 0, 4) == getpid(), "GETPID");
    unsigned short all[3] = {5, 6, 7}, got[3] = {0};
    CHECK(semctl(s, 0, 9, all) == 0 && semctl(s, 0, 6, got) == 0 && got[0] == 5 && got[2] == 7, "SETALL/GETALL");
    unsigned char ds[72];
    CHECK(semctl(s, 0, 2, ds) == 0 && *(unsigned short *)(ds + 28) == 3 && *(int *)(ds + 20) == key, "IPC_STAT");
    semctl(s, 0, 8, (void *)0L);
    semctl(s, 1, 8, (void *)0L);
    pid_t c = fork();
    if (!c) {   /* filho: espera o pai liberar e depois sinaliza o semáforo 2 */
        sb_t w = {0, -1, 0}, p = {2, 10, 0};
        int r = semop(s, &w, 1);
        semop(s, &p, 1);
        _exit(r == 0 ? 0 : 1);
    }
    usleep(100000);
    CHECK(semctl(s, 0, 3) == 1, "GETNCNT com o filho esperando");
    sb_t post = {0, 1, 0};
    semop(s, &post, 1);
    int st = -1;
    waitpid(c, &st, 0);
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0 && semctl(s, 2, 5) == 17, "filho acordou entre processos");
    c = fork();
    if (!c) { sb_t u = {1, 4, 0x1000}; semop(s, &u, 1); int ok = semctl(s, 1, 5) == 4; cx_sysv_exit(); _exit(ok ? 0 : 1); }   /* o _exit do libSystem chama o gancho */   /* SEM_UNDO */
    waitpid(c, &st, 0);
    CHECK(WEXITSTATUS(st) == 0 && semctl(s, 1, 5) == 0, "SEM_UNDO desfeito na saída");
    CHECK(semctl(s, 0, 0) == 0 && semctl(s, 0, 5) == -1, "IPC_RMID");
    CHECK(semget(key, 1, 0) == -1 && err == 2, "chave some com o objeto");

    /* ---- memória compartilhada */
    int m = shmget(key, 10000, 0x200 | 0600);
    CHECK(m > 0, "shmget");
    CHECK(shmget(key, 5 << 20, 0x200) == -1 && err == 22, "acima de shmmax");
    char *a = shmat(m, 0, 0);
    CHECK(a != (void *)-1, "shmat");
    strcpy(a, "antes");
    c = fork();
    if (!c) { char *b = shmat(m, 0, 0); int ok = b != (void *)-1 && !strcmp(b, "antes"); strcpy(b + 9000, "do filho"); shmdt(b); _exit(ok ? 0 : 1); }
    waitpid(c, &st, 0);
    CHECK(WEXITSTATUS(st) == 0 && !strcmp(a + 9000, "do filho"), "shm entre processos");
    unsigned char sd[76];
    CHECK(shmctl(m, 2, sd) == 0 && *(uint64_t *)(sd + 24) == 10000 && *(unsigned short *)(sd + 40) == 1, "shm IPC_STAT");
    CHECK(shmctl(m, 0, 0) == 0 && shmdt(a) == 0, "shm RMID + shmdt");
    CHECK(shmdt(a) == -1 && err == 22, "shmdt repetido EINVAL");

    /* ---- filas de mensagens */
    int q = msgget(0, 0x200 | 0600);   /* IPC_PRIVATE */
    CHECK(q > 0, "msgget privado");
    struct { long t; char d[64]; } msg = {3, "tres"}, in;
    CHECK(msgsnd(q, &msg, 5, 0) == 0, "msgsnd");
    msg.t = 1; strcpy(msg.d, "um");
    CHECK(msgsnd(q, &msg, 3, 0) == 0, "msgsnd 2");
    CHECK(msgrcv(q, &in, 64, -2, 0) == 3 && in.t == 1 && !strcmp(in.d, "um"), "msgrcv tipo <= 2");
    CHECK(msgrcv(q, &in, 2, 0, 0) == -1 && err == 7, "E2BIG");
    CHECK(msgrcv(q, &in, 2, 0, 0x1000) == 2 && in.t == 3, "MSG_NOERROR trunca");
    CHECK(msgrcv(q, &in, 64, 0, 0x800) == -1 && err == 91, "fila vazia ENOMSG");
    c = fork();
    if (!c) { usleep(50000); struct { long t; char d[8]; } r = {7, "filho"}; _exit(msgsnd(q, &r, 6, 0) == 0 ? 0 : 1); }
    CHECK(msgrcv(q, &in, 64, 7, 0) == 6 && !strcmp(in.d, "filho"), "msgrcv espera o filho");
    waitpid(c, &st, 0);
    unsigned char qd[116];
    CHECK(msgctl(q, 2, qd) == 0 && *(uint64_t *)(qd + 40) == 0 && *(uint64_t *)(qd + 48) == 2048, "msg IPC_STAT");
    for (int i = 0; i < 2000; i++) { msg.t = 1; if (msgsnd(q, &msg, 64, 0x800) != 0) break; }
    CHECK(err == 35, "fila cheia EAGAIN");
    CHECK(msgctl(q, 0, 0) == 0 && msgsnd(q, &msg, 1, 0) == -1, "msg RMID");
    printf("sysv-ipc: %d/%d ok\n", oks, oks + fails);
    return fails != 0;
}
