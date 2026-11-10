#ifndef HOST_ID_H
#define HOST_ID_H
/* Credenciais de proc_spawn_in (kernel/proc/process.c): uid/euid/suid = 501, gid/egid/sgid = 20.
 * pread: offset explícito, a posição do descritor não muda (SYS_PREAD). */
long host_getid(unsigned sysno);
long host_pread_fd(int fd, void *buf, unsigned long n, unsigned long off);
#endif
