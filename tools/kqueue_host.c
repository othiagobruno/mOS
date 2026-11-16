/*
 * mOS — teste de host do kqueue/kevent do libSystem (userland/libsystem/kqueue.c) contra um kernel falso:
 * USER, TIMER, SIGNAL, PROC (EXIT/EXITSTATUS/FORK/EXEC), VNODE (journal do VFS), FS, MEMORYSTATUS/VM, READ de arquivo,
 * EV_DISPATCH, EV_UDATA_SPECIFIC, EV_RECEIPT e os erros de registro.
 *
 *   gcc -O1 -Wall -DMOS_LIBSYS_SYSCALL -Iuserland/libsystem -c userland/libsystem/kqueue.c -o /tmp/kqueue_ls.o
 *   gcc -O1 -Wall tools/kqueue_host.c /tmp/kqueue_ls.o -o /tmp/kqueue_host && /tmp/kqueue_host
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#define MOS_SSIZE_T_DEFINED
#include "../kernel/include/syscall.h"

typedef struct { uint64_t ident; int16_t filter; uint16_t flags; uint32_t fflags; int64_t data; uint64_t udata; uint64_t ext[2]; } kev_t;
int kqueue(void);
int kevent64(int fd, const kev_t *ch, int nch, kev_t *ev, int nev, unsigned flags, const void *ts);
void cx_kq_signal_hit(int s);
int cx_kqueue_peek(int fd);

int cx_errno_v; int *__error(void) { return &cx_errno_v; }
int cx_ca_diag(void) { return 0; }
int cx_fdprintf(int fd, const char *f, ...) { (void)fd; (void)f; return 0; }
int cx_mach_pending(unsigned p) { (void)p; return 0; }
int mach_msg(void *m, int o, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e) { (void)m; (void)o; (void)a; (void)b; (void)c; (void)d; (void)e; return 0; }
long lseek(int fd, long off, int wh) { (void)off; (void)wh; return fd == 5 ? 30 : 0; }
int cx_sig_from_mos(int s) { return s == 10 ? 30 : s; }
static int watch_calls[32];
void cx_sig_watch(int s, int on) { watch_calls[s] += on ? 1 : -1; }

/* estado do kernel falso */
static uint64_t fake_ms = 1000;
static int child_alive = 1, child_status = 3 << 8, children = 0;
static char child_path[64] = "/bin/filho";
static uint64_t ev_last;
static mos_fsevent_t journal[16];
static unsigned pressure = 1;
static void fs_push(uint64_t ino, unsigned fl, const char *path) {
    mos_fsevent_t *e = &journal[ev_last % 16];
    memset(e, 0, sizeof *e);
    e->id = ++ev_last; e->ino = ino; e->flags = fl;
    strcpy(e->path, path);
}
long cx_syscall(long n, long a, long b, long c, long d, long e) {
    (void)d; (void)e;
    switch (n) {
    case SYS_TIME: return (long)fake_ms;
    case SYS_SLEEP: fake_ms += (uint64_t)a; return 0;
    case SYS_YIELD: return 0;
    case SYS_UNIXTIME: return 1700000000;
    case SYS_SIGKILL: return a == 77 && !child_alive ? -3 : 0;
    case SYS_FSTAT: {
        if (a == 99) return -9;
        mos_stat_t *st = (void *)b;
        memset(st, 0, sizeof *st);
        st->size = 100;
        return 0;
    }
    case SYS_FDPATH: strcpy((char *)b, a == 6 ? "/Users/v/pasta" : "/Users/v/a.txt"); return 14;
    case SYS_VMCTL: { mos_vmstats_t *v = (void *)b; memset(v, 0, sizeof *v); v->pressure_level = pressure; return 0; }
    case SYS_FSMETA: {
        mos_fsmeta_t *m = (void *)b;
        if (a == MOS_FSM_EVENTID) return (long)ev_last;
        if (a == MOS_FSM_STATX) { mos_statx_t *x = (void *)m->buf; memset(x, 0, sizeof *x); x->ino = m->fd == 6 ? 600 : 500; return 0; }
        if (a == MOS_FSM_EVENTS) {
            long k = 0;
            mos_fsevent_t *o = (void *)m->buf;
            for (uint64_t id = m->pos + 1; id <= ev_last && (size_t)(k + 1) * sizeof *o <= m->size; id++) o[k++] = journal[(id - 1) % 16];
            return k;
        }
        return -22;
    }
    case SYS_DARWIN:
        switch (a) {
        case MOS_DW_PIDS: { int *o = (int *)b; int k = 0; o[k++] = 77; for (int i = 0; i < children; i++) o[k++] = 200 + i; return k; (void)c; }
        case MOS_DW_PROC: {
            mos_dw_proc_t *p = (void *)c;
            memset(p, 0, sizeof *p);
            if (b == 77 && child_alive) { p->pid = 77; p->ppid = 1; strcpy(p->path, child_path); p->start_unix_us = 5; return 0; }
            if (b >= 200 && b < 200 + children) { p->pid = (int)b; p->ppid = 77; return 0; }
            return -MOS_ESRCH;
        }
        case MOS_DW_EXITSTATUS: return b == 77 && !child_alive ? child_status : -MOS_ESRCH;
        }
        return -22;
    }
    return -38;
}

static int fails, oks;
#define CHECK(c, ...) do { if (c) oks++; else { fails++; printf("FALHA %d: ", __LINE__); printf(__VA_ARGS__); printf(" (errno %d)\n", cx_errno_v); } } while (0)
static kev_t K(uint64_t id, int16_t f, uint16_t fl, uint32_t ff, int64_t data, uint64_t ud) { kev_t k; memset(&k, 0, sizeof k); k.ident = id; k.filter = f; k.flags = fl; k.fflags = ff; k.data = data; k.udata = ud; return k; }
static const struct { long s, ns; } zero = {0, 0}, ms20 = {0, 20000000};

int main(void) {
    int kq = kqueue();
    kev_t ev[8], c;
    CHECK(kq >= 0x4000, "kqueue");
    /* erros de registro com EV_RECEIPT */
    c = K(1, -11, 0x41, 0, 0, 0);
    CHECK(kevent64(kq, &c, 1, ev, 8, 0, 0) == 1 && (ev[0].flags & 0x4000) && ev[0].data == 22, "filtro desconhecido EINVAL");
    c = K(99, -1, 0x41, 0, 0, 0);
    CHECK(kevent64(kq, &c, 1, ev, 8, 0, 0) == 1 && ev[0].data == 9, "fd inválido EBADF");
    c = K(4242, -5, 0x41, 0x80000000u, 0, 0);
    CHECK(kevent64(kq, &c, 1, ev, 8, 0, 0) == 1 && ev[0].data == 3, "pid inexistente ESRCH");
    /* USER com EV_DISPATCH: entrega uma vez e desliga até EV_ENABLE */
    c = K(7, -10, 0x1 | 0x20 | 0x80, 0, 0, 11);
    kevent64(kq, &c, 1, 0, 0, 0, 0);
    c = K(7, -10, 0, 0x01000000u, 0, 0);
    kevent64(kq, &c, 1, 0, 0, 0, 0);
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1 && ev[0].ident == 7 && ev[0].udata == 11, "USER");
    c = K(7, -10, 0, 0x01000000u, 0, 0);
    kevent64(kq, &c, 1, 0, 0, 0, 0);
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 0, "EV_DISPATCH desligou");
    c = K(7, -10, 0x4, 0x01000000u, 0, 0);
    kevent64(kq, &c, 1, 0, 0, 0, 0);
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1, "EV_ENABLE religa");
    /* EV_UDATA_SPECIFIC: dois knotes do mesmo ident com udata diferentes */
    c = K(8, -10, 0x1 | 0x100, 0, 0, 1); kevent64(kq, &c, 1, 0, 0, 0, 0);
    c = K(8, -10, 0x1 | 0x100, 0, 0, 2); kevent64(kq, &c, 1, 0, 0, 0, 0);
    c = K(8, -10, 0x100, 0x01000000u, 0, 2); kevent64(kq, &c, 1, 0, 0, 0, 0);
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1 && ev[0].udata == 2, "UDATA_SPECIFIC");
    c = K(8, -10, 0x2 | 0x100, 0, 0, 1); CHECK(kevent64(kq, &c, 1, 0, 0, 0, 0) == 0, "delete por udata");
    c = K(8, -10, 0x2 | 0x100, 0, 0, 2); kevent64(kq, &c, 1, 0, 0, 0, 0);
    c = K(7, -10, 0x2, 0, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    /* TIMER: 10 ms periódico; esperando 35 ms conta os disparos perdidos */
    c = K(1, -7, 0x1, 0, 10, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &ms20) == 1 && ev[0].data >= 1, "TIMER");
    fake_ms += 35;
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1 && ev[0].data >= 3, "TIMER disparos perdidos %lld", (long long)ev[0].data);
    c = K(1, -7, 0x2, 0, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    /* SIGNAL: conta e liga o tratador interno */
    c = K(30, -6, 0x1, 0, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    CHECK(watch_calls[30] == 1, "cx_sig_watch ligado");
    cx_kq_signal_hit(30); cx_kq_signal_hit(30);
    CHECK(cx_kqueue_peek(kq) == 1, "peek vê o sinal");
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1 && ev[0].data == 2 && ev[0].filter == -6, "SIGNAL data=2");
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 0, "SIGNAL limpo");
    c = K(30, -6, 0x2, 0, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    CHECK(watch_calls[30] == 0, "cx_sig_watch desligado");
    /* PROC: FORK, EXEC, EXIT + EXITSTATUS */
    c = K(77, -5, 0x1, 0x80000000u | 0x04000000u | 0x40000000u | 0x20000000u, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 0, "PROC quieto");
    children = 1;
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1 && ev[0].fflags == 0x40000000u, "NOTE_FORK");
    strcpy(child_path, "/bin/outro");
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1 && ev[0].fflags == 0x20000000u, "NOTE_EXEC");
    child_alive = 0;
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1 && (ev[0].fflags & 0x80000000u) && (ev[0].fflags & 0x04000000u) && ev[0].data == (3 << 8) && (ev[0].flags & 0x8000), "NOTE_EXIT+EXITSTATUS");
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 0, "EXIT uma vez");
    /* VNODE: arquivo (ino 500) e pasta (ino 600) */
    c = K(5, -4, 0x1 | 0x20, 0x1 | 0x2 | 0x4 | 0x8 | 0x20, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    c = K(6, -4, 0x1 | 0x20, 0x2 | 0x10, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    fs_push(999, FSEV_MODIFIED, "/outro");
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 0, "evento de outro inode não acorda");
    fs_push(500, FSEV_MODIFIED | FSEV_IS_FILE, "/Users/v/a.txt");
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1 && ev[0].ident == 5 && (ev[0].fflags & 0x2) && (ev[0].fflags & 0x4), "NOTE_WRITE|EXTEND");
    fs_push(500, FSEV_INODE_META, "/Users/v/a.txt");
    fs_push(500, FSEV_RENAMED, "/Users/v/b.txt");
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1 && ev[0].fflags == (0x8 | 0x20), "NOTE_ATTRIB|RENAME agregados");
    fs_push(0, FSEV_CREATED | FSEV_IS_DIR, "/Users/v/pasta/sub");
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1 && ev[0].ident == 6 && ev[0].fflags == (0x2 | 0x10), "pasta: NOTE_WRITE|LINK");
    fs_push(0, FSEV_CREATED, "/Users/v/pasta/sub/neto");
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 0, "neto não conta");
    fs_push(500, FSEV_REMOVED, "/Users/v/b.txt");
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1 && ev[0].fflags == 0x1 && (ev[0].flags & 0x8000), "NOTE_DELETE + EOF");
    c = K(5, -4, 0x2, 0, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    c = K(6, -4, 0x2, 0, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    /* FS */
    c = K(0, -9, 0x1 | 0x20, 0, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    fs_push(0, FSEV_MOUNT, "/Volumes/X");
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1 && ev[0].fflags == 0x8, "EVFILT_FS VQ_MOUNT");
    c = K(0, -9, 0x2, 0, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    /* MEMORYSTATUS e VM */
    c = K(0, -14, 0x1 | 0x20, 0x1 | 0x2 | 0x4, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    c = K(0, -12, 0x1 | 0x20, 0x80000000u, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 0, "pressão normal");
    pressure = 2;
    int n = kevent64(kq, 0, 0, ev, 8, 0, &zero);
    CHECK(n == 2, "aviso acorda os dois (%d)", n);
    pressure = 4;
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1 && ev[0].filter == -14 && ev[0].fflags == 4, "crítico só no MEMORYSTATUS");
    pressure = 1;
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1 && ev[0].fflags == 1, "volta ao normal");
    c = K(0, -14, 0x2, 0, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    c = K(0, -12, 0x2, 0, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    /* READ de arquivo comum: o que falta a partir da posição (100 - 30) */
    c = K(5, -1, 0x1, 0, 0, 0); kevent64(kq, &c, 1, 0, 0, 0, 0);
    CHECK(kevent64(kq, 0, 0, ev, 8, 0, &zero) == 1 && ev[0].data == 70, "READ data=%lld", (long long)ev[0].data);
    printf("kqueue: %d/%d ok\n", oks, oks + fails);
    return fails != 0;
}
