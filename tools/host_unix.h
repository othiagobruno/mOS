/* AF_UNIX do harness (o mesmo contrato de kernel/net/unix.c e docs/af-unix.md). */
#pragma once

typedef struct host_unix_hooks {
    int (*alloc_fd)(int slot, int nb, int cloexec); /* devolve fd ou -errno */
    int (*fd_slot)(int fd);                         /* slot do endpoint, ou -1 */
    void (*fd_set_slot)(int fd, int slot);
    int (*dup_fd)(int fd);                          /* SCM_RIGHTS: novo fd do mesmo objeto */
    int (*parent_ok)(const char *path);
    int (*create_node)(const char *path);
    int (*unlink_node)(const char *path);
    /* trava grande do processo, a mesma que cobre as outras syscalls. O recv bloqueante
     * solta ela enquanto dorme: no kernel a thread sai do escalonador e outra roda
     * (FUTEX_WAKE inclusive). Sem isto o recv segura a trava e o wake não entra. */
    void (*drop_proc)(void);
    void (*take_proc)(void);
} host_unix_hooks;

void host_unix_set_hooks(const host_unix_hooks *h);
long host_unix_op(unsigned long long op, unsigned long long a, unsigned long long b, unsigned long long c);
int host_unix_owns(int fd);
void host_unix_close(int fd);
void host_unix_hold(int fd);
long host_unix_read(int fd, void *buf, unsigned long long len);
long host_unix_write(int fd, const void *buf, unsigned long long len);
void host_unix_fstat(int fd, unsigned long long *size, int *is_dir, long long *mtime);
void host_unix_set_nb(int fd, int nb);
int host_unix_nb(int fd);
