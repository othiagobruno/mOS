/*
 * mOS libSystem — IPC POSIX nomeado e chamadas BSD que faltavam, com o contrato do macOS (o mesmo que o Darling
 * entrega repassando à glibc ou devolvendo ENOTSUP):
 *
 *   shm_open / shm_unlink      objetos de memória compartilhada por nome, entre processos;
 *   sem_open / sem_close / sem_unlink / sem_post / sem_wait / sem_trywait
 *                              semáforos nomeados entre processos;
 *   sem_init / sem_destroy / sem_getvalue / sem_timedwait
 *                              sem semáforo anônimo no macOS: -1 com ENOSYS;
 *   abort_with_reason / abort_with_payload
 *                              motivo no stderr e SIGABRT;
 *   getattrlistbulk, exchangedata, fsgetpath, getdirentriesattr, searchfs
 *                              -1 com ENOTSUP (os volumes do mOS não têm essas operações, nem o Darling).
 *
 * O nome ("/x" ou "x") vira um arquivo em /tmp/.posix-ipc (shm.x, sem.x); '/' e '%' no meio do nome são
 * escapados. Nome com mais de 31 bytes (PSHMNAMLEN/PSEMNAMLEN) é ENAMETOOLONG. O semáforo é um registro de
 * 8 bytes (marca + valor) no começo do arquivo, mapeado MAP_SHARED: os processos veem a mesma página.
 * Quem espera dorme no futex do valor: o kernel acorda entre processos numa página MAP_SHARED (PR #236).
 * Teste: tools/test-posixipc.sh (userland/tests-posixipc).
 */
#include "sys.h"
#include "libc.h"

#define E_NOENT        2
#define E_NOMEM        12
#define E_EXIST        17
#define E_INVAL        22
#define E_MFILE        24
#define E_AGAIN        35
#define E_NOTSUP       45
#define E_NAMETOOLONG  63
#define E_NOSYS        78
#define O_RDONLY_  0
#define O_WRONLY_  1
#define O_RDWR_    2
#define O_ACCMODE_ 3
#define O_CREAT_   0x200
#define O_TRUNC_   0x400
#define O_EXCL_    0x800
#define PROT_RW    3
#define MAP_SHARED_ 1
#define NAME_MAX_POSIX_IPC 31
#define SEM_VALUE_MAX_ 32767
#define IPC_DIR "/tmp/.posix-ipc"

int open(const char *path, int flags, ...);
int close(int fd);
ssize_t read(int fd, void *b, size_t n);
ssize_t write(int fd, const void *b, size_t n);
int unlink(const char *path);
int mkdir(const char *path, int mode);
int ftruncate(int fd, long len);
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int munmap(void *a, size_t len);
int cx_futex_wait(volatile int *w, int expected, long ms);
int cx_futex_wake(volatile int *w, int n);
void abort(void);
int snprintf(char *b, size_t n, const char *fmt, ...);

static int fail(int e) { cx_errno = e; return -1; }

/* nome POSIX -> caminho do arquivo. 0 = ok; senão o errno. */
static int ipc_path(const char *kind, const char *name, char *out, size_t cap) {
    if (!name) return E_INVAL;
    if (name[0] == '/') name++;
    size_t len = strlen(name);
    if (len == 0) return E_INVAL;
    if (len > NAME_MAX_POSIX_IPC) return E_NAMETOOLONG;
    mkdir("/tmp", 0777);
    mkdir(IPC_DIR, 0777);
    size_t k = (size_t)snprintf(out, cap, IPC_DIR "/%s.", kind);
    for (const char *s = name; *s && k + 4 < cap; s++) {
        if (*s == '/' || *s == '%') { snprintf(out + k, cap - k, "%%%02X", (unsigned char)*s); k += 3; }
        else out[k++] = *s;
    }
    out[k] = 0;
    return 0;
}

/* ---- shm_open / shm_unlink */
int shm_open(const char *name, int oflag, ...) {
    unsigned mode = 0600;
    if (oflag & O_CREAT_) { va_list ap; va_start(ap, oflag); mode = va_arg(ap, unsigned); va_end(ap); }
    char path[160];
    int e = ipc_path("shm", name, path, sizeof path);
    if (e) return fail(e);
    if ((oflag & O_ACCMODE_) == O_WRONLY_) return fail(E_INVAL);        /* o macOS só aceita O_RDONLY ou O_RDWR */
    int fd = open(path, oflag & (O_ACCMODE_ | O_CREAT_ | O_EXCL_ | O_TRUNC_), mode & 0777);
    return fd;                                                         /* errno do open: ENOENT, EEXIST... */
}
int shm_unlink(const char *name) {
    char path[160];
    int e = ipc_path("shm", name, path, sizeof path);
    if (e) return fail(e);
    return unlink(path);
}

/* ---- semáforos nomeados */
#define SEM_MAGIC 0x6D53454Du      /* "MSEM" */
typedef struct { volatile u32 magic; volatile int value; } psem_t;
#define MAX_OPEN_SEMS 256
static struct { char path[160]; psem_t *p; int refs; } opened[MAX_OPEN_SEMS];
static volatile int open_lk;
static void ol_lock(void) { while (__sync_lock_test_and_set(&open_lk, 1)) SYS0(SYS_YIELD); }
static void ol_unlock(void) { __sync_lock_release(&open_lk); }
static int sem_slot(const void *s) {
    for (int i = 0; i < MAX_OPEN_SEMS; i++) if (opened[i].p && (const void *)opened[i].p == s) return i;
    return -1;
}

void *sem_open(const char *name, int oflag, ...) {
    unsigned mode = 0600, value = 0;
    if (oflag & O_CREAT_) {
        va_list ap; va_start(ap, oflag);
        mode = va_arg(ap, unsigned);
        value = va_arg(ap, unsigned);
        va_end(ap);
        if (value > SEM_VALUE_MAX_) { cx_errno = E_INVAL; return (void *)-1; }
    }
    char path[160];
    int e = ipc_path("sem", name, path, sizeof path);
    if (e) { cx_errno = e; return (void *)-1; }

    ol_lock();
    int created = 0, fd = -1;
    if (oflag & O_CREAT_) {
        fd = open(path, O_RDWR_ | O_CREAT_ | O_EXCL_, mode & 0777);
        if (fd >= 0) created = 1;
        else if (cx_errno != E_EXIST || (oflag & O_EXCL_)) { ol_unlock(); return (void *)-1; }
    }
    if (!created) {
        for (int i = 0; i < MAX_OPEN_SEMS; i++)
            if (opened[i].p && !strcmp(opened[i].path, path)) { opened[i].refs++; psem_t *p = opened[i].p; ol_unlock(); return p; }
        fd = open(path, O_RDWR_, 0);
        if (fd < 0) { ol_unlock(); return (void *)-1; }
    }
    int slot = -1;
    for (int i = 0; i < MAX_OPEN_SEMS && slot < 0; i++) if (!opened[i].p) slot = i;
    if (slot < 0) { close(fd); if (created) unlink(path); ol_unlock(); cx_errno = E_MFILE; return (void *)-1; }
    if (created) {
        /* a marca entra por último, já no mapa: quem abre espera por ela. O arquivo é fechado e reaberto antes
         * do mmap porque o volume pode só gravar o conteúdo no close. */
        psem_t init = {0, (int)value};
        ssize_t w = write(fd, &init, sizeof init);
        close(fd);
        fd = w == (ssize_t)sizeof init ? open(path, O_RDWR_, 0) : -1;
        if (fd < 0) { unlink(path); ol_unlock(); return (void *)-1; }
    }
    psem_t *p = mmap(0, 4096, PROT_RW, MAP_SHARED_, fd, 0);
    close(fd);
    if (p == (void *)-1 || !p) { if (created) unlink(path); ol_unlock(); cx_errno = E_NOMEM; return (void *)-1; }
    if (created) __atomic_store_n(&p->magic, SEM_MAGIC, __ATOMIC_RELEASE);
    else for (int i = 0; i < 1000 && __atomic_load_n(&p->magic, __ATOMIC_ACQUIRE) != SEM_MAGIC; i++) SYS1(SYS_SLEEP, 1);
    strlcpy(opened[slot].path, path, sizeof opened[slot].path);
    opened[slot].p = p;
    opened[slot].refs = 1;
    ol_unlock();
    return p;
}
int sem_close(void *s) {
    ol_lock();
    int i = sem_slot(s);
    if (i < 0) { ol_unlock(); return fail(E_INVAL); }
    if (--opened[i].refs == 0) { munmap(opened[i].p, 4096); opened[i].p = 0; opened[i].path[0] = 0; }
    ol_unlock();
    return 0;
}
/* o nome some na hora; quem já abriu continua usando o semáforo até o sem_close */
int sem_unlink(const char *name) {
    char path[160];
    int e = ipc_path("sem", name, path, sizeof path);
    if (e) return fail(e);
    if (unlink(path) < 0) return -1;
    ol_lock();
    for (int i = 0; i < MAX_OPEN_SEMS; i++) if (opened[i].p && !strcmp(opened[i].path, path)) opened[i].path[0] = 0;
    ol_unlock();
    return 0;
}
static psem_t *sem_valid(void *s) {
    ol_lock();
    int i = sem_slot(s);
    ol_unlock();
    return i < 0 ? 0 : (psem_t *)s;
}
int sem_post(void *s) {
    psem_t *p = sem_valid(s);
    if (!p) return fail(E_INVAL);
    __atomic_add_fetch(&p->value, 1, __ATOMIC_RELEASE);
    cx_futex_wake(&p->value, 1);
    return 0;
}
static int sem_take(psem_t *p) {
    int v = __atomic_load_n(&p->value, __ATOMIC_ACQUIRE);
    while (v > 0)
        if (__atomic_compare_exchange_n(&p->value, &v, v - 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) return 1;
    return 0;
}
int sem_trywait(void *s) {
    psem_t *p = sem_valid(s);
    if (!p) return fail(E_INVAL);
    return sem_take(p) ? 0 : fail(E_AGAIN);
}
int sem_wait(void *s) {
    psem_t *p = sem_valid(s);
    if (!p) return fail(E_INVAL);
    while (!sem_take(p)) cx_futex_wait(&p->value, 0, -1);
    return 0;
}
/* o macOS não tem semáforo POSIX anônimo */
int sem_init(void *s, int pshared, unsigned value) { (void)s; (void)pshared; (void)value; return fail(E_NOSYS); }
int sem_destroy(void *s) { (void)s; return fail(E_NOSYS); }
int sem_getvalue(void *s, int *v) { (void)s; (void)v; return fail(E_NOSYS); }
int sem_timedwait(void *s, const void *ts) { (void)s; (void)ts; return fail(E_NOSYS); }

/* ---- abort com motivo (os_crash, Swift, libdispatch): o motivo vai para o stderr e o processo leva SIGABRT */
__attribute__((noreturn)) void abort_with_payload(u32 ns, u64 code, void *payload, u32 size, const char *reason, u64 flags) {
    (void)payload; (void)size; (void)flags;
    char b[600];
    int n = snprintf(b, sizeof b, "abort_with_payload: reason: %s; namespace: %u; code: 0x%llx\n",
                     reason ? reason : "(nenhum)", ns, (unsigned long long)code);
    if (n > 0) write(2, b, (size_t)(n < (int)sizeof b ? n : (int)sizeof b - 1));
    abort();
}
__attribute__((noreturn)) void abort_with_reason(u32 ns, u64 code, const char *reason, u64 flags) {
    abort_with_payload(ns, code, 0, 0, reason, flags);
}

/* ---- operações de volume que o FAT/APFS só-leitura do mOS não têm */
int getdirentriesattr(int fd, void *al, void *buf, size_t n, unsigned *count, unsigned *base, unsigned *newstate, unsigned opt) {
    (void)fd; (void)al; (void)buf; (void)n; (void)count; (void)base; (void)newstate; (void)opt;
    return fail(E_NOTSUP);
}
int searchfs(const char *p, void *sb, unsigned long *nmatch, unsigned script, unsigned opt, void *state) {
    (void)p; (void)sb; (void)nmatch; (void)script; (void)opt; (void)state;
    return fail(E_NOTSUP);
}
