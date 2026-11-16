/*
 * mOS — teste de host do libproc do libSystem (userland/libsystem/procinfo.c) contra um kernel falso de SYS_DARWIN.
 *
 *   gcc -O1 -Wall -DMOS_LIBSYS_SYSCALL -Iuserland/libsystem -c userland/libsystem/procinfo.c -o /tmp/procinfo_ls.o
 *   gcc -O1 -Wall tools/procinfo_host.c /tmp/procinfo_ls.o -o /tmp/procinfo_host && /tmp/procinfo_host
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#define MOS_SSIZE_T_DEFINED
#include "../kernel/include/syscall.h"

int proc_pidinfo(int pid, int flavor, uint64_t arg, void *buf, int size);
int proc_pidfdinfo(int pid, int fd, int flavor, void *buf, int size);
int proc_pid_rusage(int pid, int flavor, void *buf);
int proc_listpids(uint32_t type, uint32_t info, void *buf, int bytes);
int proc_listallpids(void *buf, int bytes);
int proc_listchildpids(int ppid, void *buf, int bytes);
int proc_pidpath(int pid, void *buf, uint32_t size);
int proc_name(int pid, void *buf, uint32_t size);
int proc_terminate(int pid, int *sig);
int proc_track_dirty(int pid, uint32_t flags);
int proc_set_dirty(int pid, int dirty);
int proc_get_dirty(int pid, uint32_t *flags);
size_t os_proc_available_memory(void);

static int err;
int *__error(void) { return &err; }
int getpid(void) { return 42; }
static int killed_pid, killed_sig;
int kill(int pid, int sig) { killed_pid = pid; killed_sig = sig; return 0; }
unsigned long long mach_absolute_time(void) { return 0; }
int _NSGetExecutablePath(char *b, uint32_t *n) { strcpy(b, "/Aplicativos/Teste.app/Contents/MacOS/Teste"); (void)n; return 0; }
int sysctl(int *m, unsigned n, void *o, size_t *l, void *nw, size_t nl) {   /* kinfo_proc com e_tdev do pid 77 */
    (void)n; (void)nw; (void)nl;
    memset(o, 0, 648);
    int dev = m[3] == 77 ? (16 << 24) | 3 : -1;
    memcpy((char *)o + 572, &dev, 4);
    *l = 648;
    return 0;
}
typedef struct { uint32_t dev; uint16_t mode, nlink; uint64_t ino; uint32_t uid, gid, rdev; int64_t t[8]; int64_t size, blocks; int32_t blksize; uint32_t flags, gen; int32_t lsp; int64_t q[2]; } mstat_t;
int stat(const char *p, void *st) {
    mstat_t *s = st;
    memset(s, 0, sizeof *s);
    s->dev = 7; s->ino = 1234; s->uid = 501; s->gid = 20;
    if (!strcmp(p, "/") || !strcmp(p, "/Users/Visitante")) { s->mode = 0x4000 | 0755; return 0; }
    if (!strcmp(p, "/Users/Visitante/a.txt")) { s->mode = 0x8000 | 0644; s->size = 99; return 0; }
    return -1;
}

typedef struct { int pid, ppid, pgid, sid, uid; const char *name; } fp_t;
static const fp_t fp[] = {{1, 0, 1, 1, 0, "init"}, {42, 1, 42, 42, 501, "Teste"}, {77, 42, 42, 42, 501, "Ajudante"}, {90, 1, 90, 90, 0, "launchd"}};
#define NFP 4
long cx_syscall(long nr, long a, long b, long c, long d, long e) {
    (void)e;
    if (nr == SYS_GETEUID) return 501;
    if (nr == SYS_VMCTL && a == MOS_VM_FOOTPRINT) return 5 << 20;
    if (nr != SYS_DARWIN) return -38;
    switch (a) {
    case MOS_DW_SYSINFO: { mos_dw_sysinfo_t *s = (void *)b; memset(s, 0, sizeof *s); s->mem_total = 14ull << 30; s->mem_free = 6ull << 30; s->boot_unix_us = 1000000000ull * 1000000; return 0; }
    case MOS_DW_PIDS: { int *o = (int *)b; for (int i = 0; i < NFP && i < c; i++) o[i] = fp[i].pid; return NFP; }
    case MOS_DW_PROC:
        for (int i = 0; i < NFP; i++) if (fp[i].pid == b) {
            mos_dw_proc_t *p = (void *)c;
            memset(p, 0, sizeof *p);
            p->pid = fp[i].pid; p->ppid = fp[i].ppid; p->pgid = fp[i].pgid; p->sid = fp[i].sid;
            p->uid = p->euid = p->suid = fp[i].uid; p->gid = p->egid = p->sgid = 20;
            p->state = MOS_DW_ST_RUN; p->nthreads = 2; p->cpu_ms = 1500; p->mem_bytes = 3 << 20; p->vsize = 64 << 20;
            p->start_unix_us = 1000000000ull * 1000000 + 7000000; p->flags = fp[i].pid == 42 ? 2 : 0;
            strcpy(p->name, fp[i].name);
            sprintf(p->path, "/bin/%s", fp[i].name);
            return 0;
        }
        return -MOS_ESRCH;
    case MOS_DW_THREADS: {
        if (b != 42 && b != 77) return -MOS_ESRCH;
        mos_dw_thread_t *t = (void *)c;
        for (int i = 0; i < 2 && i < d; i++) { memset(&t[i], 0, sizeof t[i]); t[i].tid = 100 + i; t[i].cpu_ms = 700; t[i].state = i ? MOS_DW_ST_SLEEP : MOS_DW_ST_RUN; t[i].qos = 2; strcpy(t[i].name, i ? "trabalho" : "principal"); }
        return 2;
    }
    case MOS_DW_FDS: {
        if (b == 1) return -MOS_EPERM;
        if (b != 42) return -MOS_ESRCH;
        mos_dw_fd_t *f = (void *)c;
        if (!c) return 3;
        static const int kinds[3] = {MOS_DW_FD_PTY, MOS_DW_FD_FILE, MOS_DW_FD_PIPE};
        for (int i = 0; i < 3 && i < d; i++) {
            memset(&f[i], 0, sizeof f[i]);
            f[i].fd = i == 2 ? 5 : i; f[i].kind = kinds[i]; f[i].flags = i == 1 ? 2 | 0x400 : 0; f[i].off = i == 1 ? 33 : 0;
            f[i].cloexec = i == 1;
            strcpy(f[i].path, i == 1 ? "/Users/Visitante/a.txt" : i == 0 ? "/dev/ttys003" : "");
        }
        return 3;
    }
    case MOS_DW_CWD: strcpy((char *)c, "/Users/Visitante"); return 16;
    }
    return -22;
}

static int fails, oks;
#define CHECK(c, ...) do { if (c) oks++; else { fails++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static uint32_t r32(const void *b, int o) { uint32_t v; memcpy(&v, (const char *)b + o, 4); return v; }
static uint64_t r64(const void *b, int o) { uint64_t v; memcpy(&v, (const char *)b + o, 8); return v; }

int main(void) {
    unsigned char b[4096];
    /* BSDINFO */
    CHECK(proc_pidinfo(42, 3, 0, b, 136) == 136, "bsdinfo");
    CHECK(r32(b, 12) == 42 && r32(b, 16) == 1 && r32(b, 20) == 501 && r32(b, 24) == 20 && r32(b, 100) == 42, "bsdinfo ids");
    CHECK((r32(b, 0) & 0x10) && (r32(b, 0) & 0x20) && (r32(b, 0) & 0x40) && r32(b, 4) == 2, "bsdinfo flags/status");
    CHECK(!strcmp((char *)b + 48, "Teste") && !strcmp((char *)b + 64, "Teste") && r32(b, 96) == 3, "bsdinfo nome/nfiles");
    CHECK(r64(b, 120) == 1000000007 && r64(b, 128) == 0, "bsdinfo início");
    CHECK(proc_pidinfo(42, 3, 0, b, 100) == 0 && err == 12, "bsdinfo curto");
    CHECK(proc_pidinfo(4242, 3, 0, b, 136) == 0 && err == 3, "pid inexistente");
    /* TASKINFO e TASKALLINFO */
    CHECK(proc_pidinfo(42, 4, 0, b, 96) == 96 && r64(b, 0) == 64 << 20 && r64(b, 8) == 3 << 20 && r64(b, 16) == 1500000000ull, "taskinfo");
    CHECK(r32(b, 84) == 2 && r32(b, 88) == 1, "taskinfo threads");
    CHECK(proc_pidinfo(42, 2, 0, b, 232) == 232 && r32(b, 12) == 42 && r64(b, 136 + 8) == 3 << 20, "taskallinfo");
    /* threads */
    CHECK(proc_pidinfo(42, 6, 0, b, 64) == 16 && r64(b, 0) == 100 && r64(b, 8) == 101, "listthreads");
    CHECK(proc_pidinfo(42, 5, 101, b, 112) == 112 && r64(b, 0) == 700000000ull && r32(b, 24) == 3 && !strcmp((char *)b + 48, "trabalho"), "threadinfo");
    CHECK(proc_pidinfo(42, 5, 555, b, 112) == 0 && err == 3, "threadinfo inexistente");
    /* fds */
    int n = proc_pidinfo(42, 1, 0, b, sizeof b);
    CHECK(n == 24 && r32(b, 0) == 0 && r32(b, 4) == 1 && r32(b, 16) == 5 && r32(b, 20) == 6, "listfds");
    CHECK(proc_pidinfo(42, 1, 0, 0, 0) >= 24, "listfds tamanho");
    CHECK(proc_pidinfo(1, 1, 0, b, sizeof b) == 0 && err == 1, "listfds EPERM");
    CHECK(proc_pidfdinfo(42, 1, 2, b, 1200) == 1200, "fd vnodepath");
    CHECK((r32(b, 0) & 3) == 2 && (r32(b, 0) & 8) && r32(b, 4) == 1 && r64(b, 8) == 33 && !strcmp((char *)b + 24 + 152, "/Users/Visitante/a.txt"), "fd fileinfo/caminho");
    CHECK(r64(b, 24 + 88) == 99 && r32(b, 24 + 136) == 1, "fd vinfo tamanho/VREG");
    CHECK(proc_pidfdinfo(42, 1, 1, b, 176) == 176, "fd vnodeinfo");
    CHECK(proc_pidfdinfo(42, 5, 6, b, 184) == 184 && r32(b, 16) == 6, "fd pipe");
    CHECK(proc_pidfdinfo(42, 5, 2, b, 1200) == 0 && err == 9, "pipe como vnode EBADF");
    CHECK(proc_pidfdinfo(42, 9, 2, b, 1200) == 0 && err == 9, "fd fechado EBADF");
    /* caminhos */
    CHECK(proc_pidinfo(42, 9, 0, b, 2352) == 2352 && !strcmp((char *)b + 152, "/Users/Visitante") && r32(b, 136) == 2 && !strcmp((char *)b + 1176 + 152, "/"), "vnodepathinfo");
    char p[1024];
    CHECK(proc_pidpath(42, p, sizeof p) > 0 && !strcmp(p, "/Aplicativos/Teste.app/Contents/MacOS/Teste"), "pidpath próprio");
    CHECK(proc_pidpath(77, p, sizeof p) > 0 && !strcmp(p, "/bin/Ajudante"), "pidpath outro");
    CHECK(proc_pidpath(77, p, 5) == 0 && err == 12, "pidpath curto");
    CHECK(proc_name(77, p, sizeof p) == 8 && !strcmp(p, "Ajudante"), "proc_name");
    /* outros sabores */
    CHECK(proc_pidinfo(42, 13, 0, b, 64) == 64 && r32(b, 0) == 42 && r32(b, 8) == 42 && !strcmp((char *)b + 16, "Teste"), "shortbsdinfo");
    CHECK(proc_pidinfo(42, 17, 0, b, 56) == 56 && r64(b, 16) != 0 && r64(b, 24) != 0, "uniqidentifier");
    CHECK(proc_pidinfo(42, 18, 0, b, 192) == 192, "bsdinfowithuniqid");
    CHECK(proc_pidinfo(42, 19, 0, b, 8) == 8 && r32(b, 0) == 0x01000007, "archinfo");
    CHECK(proc_pidinfo(42, 12, 0, b, 16) == 16 && r32(b, 0) == 2 && r32(b, 4) == 1, "workqueueinfo");
    CHECK(proc_pidinfo(42, 7, 0, b, 512) == 0 && err == 45, "regioninfo ENOTSUP");
    /* rusage */
    CHECK(proc_pid_rusage(42, 4, b) == 0 && r64(b, 16) == 1500000000ull && r64(b, 64) == 3 << 20 && r64(b, 72) == 5 << 20 && r64(b, 80) == 7000000000ull, "rusage v4");
    CHECK(proc_pid_rusage(42, 7, b) == -1 && err == 22, "rusage sabor inválido");
    /* listas */
    int pids[16];
    CHECK(proc_listallpids(pids, sizeof pids) == 4, "listallpids");
    CHECK(proc_listpids(4, 501, pids, sizeof pids) == 8 && pids[0] == 42 && pids[1] == 77, "listpids uid");
    CHECK(proc_listpids(2, 42, pids, sizeof pids) == 8, "listpids pgrp");
    CHECK(proc_listpids(6, 1, pids, sizeof pids) == 8 && pids[0] == 42 && pids[1] == 90, "listpids ppid");
    CHECK(proc_listpids(3, (16 << 24) | 3, pids, sizeof pids) == 4 && pids[0] == 77, "listpids tty");
    CHECK(proc_listchildpids(42, pids, sizeof pids) == 4 && pids[0] == 77 && proc_listchildpids(42, 0, 0) == 1, "listchildpids");
    /* término e dirty */
    int sig = 0;
    CHECK(proc_terminate(77, &sig) == 0 && sig == 15 && killed_pid == 77, "terminate SIGTERM");
    CHECK(proc_terminate(1, &sig) == -1 && err == 1, "terminate outro uid EPERM");
    uint32_t fl = 9;
    CHECK(proc_track_dirty(42, 3) == 0 && proc_get_dirty(42, &fl) == 0 && fl == 3, "track dirty");
    CHECK(proc_terminate(42, &sig) == 0 && sig == 9, "terminate limpo SIGKILL");
    CHECK(proc_set_dirty(42, 1) == 0 && proc_get_dirty(42, &fl) == 0 && fl == 7, "set dirty");
    CHECK(proc_terminate(42, &sig) == 0 && sig == 15, "terminate sujo SIGTERM");
    CHECK(proc_set_dirty(77, 1) == 1, "dirty de outro EPERM");
    CHECK(os_proc_available_memory() == 6ull << 30, "available_memory");
    printf("procinfo: %d/%d ok\n", oks, oks + fails);
    return fails != 0;
}
