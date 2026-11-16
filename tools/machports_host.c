/*
 * mOS — teste de host de mach_port_names/space_info/get_srights/get_set_status/kobject do libSystem
 * (userland/libsystem/machports.c) contra um espaço de portas falso (MACH_OP_NAMES/TYPE/STATUS).
 *   gcc -O1 -Wall -DMOS_LIBSYS_SYSCALL -Iuserland/libsystem -c userland/libsystem/machports.c -o /tmp/machports_ls.o
 *   gcc -O1 -Wall tools/machports_host.c /tmp/machports_ls.o -o /tmp/machports_host && /tmp/machports_host
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define MOS_SSIZE_T_DEFINED
#include "../kernel/include/syscall.h"
typedef unsigned u32;
int mach_port_names(u32, u32 **, u32 *, u32 **, u32 *);
int mach_port_space_info(u32, u32 *, u32 **, u32 *, void **, u32 *);
int mach_port_space_basic_info(u32, u32 *);
int mach_port_get_srights(u32, u32, u32 *);
int mach_port_get_set_status(u32, u32, u32 **, u32 *);
int mach_port_kobject(u32, u32, u32 *, uint64_t *);
int mach_port_dnrequest_info(u32, u32, u32 *, u32 *);
static int err; int *__error(void) { return &err; }
int mach_vm_allocate(u32 t, uint64_t *a, uint64_t n, int f) { (void)t; (void)f; *a = (uint64_t)calloc(1, n); return *a ? 0 : 6; }
int mach_vm_deallocate(u32 t, uint64_t a, uint64_t n) { (void)t; (void)n; free((void *)a); return 0; }
int mach_port_allocate(u32 t, int r, u32 *n) { (void)t; (void)r; *n = 0x9999; return 0; }
int mach_port_allocate_name(u32 t, int r, u32 n) { (void)t; (void)r; (void)n; return 0; }
static const mos_mach_name_t sp[] = {{0x303, 0x10000, 1, 0}, {0x1107, 0x20000, 0, 0}, {0x1203, 0x30000, 2, 0}, {0x1307, 0x80000, 0, 0}, {0x1403, 0x100000, 0, 1}};
long cx_syscall(long n, long op, long ap, long c, long d, long e) {
    (void)c; (void)d; (void)e;
    uint64_t *a = (uint64_t *)ap;
    if (n == SYS_GETPID) return 42;
    if (n == SYS_DARWIN && op == MOS_DW_THREADS) { mos_dw_thread_t *t = (void *)c; memset(t, 0, sizeof *t); t->tid = 5; return 1; }
    if (n != SYS_MACH_PORT) return -38;
    if (op == MACH_OP_NAMES) { if (a[0]) for (uint64_t i = 0; i < 5 && i < a[1]; i++) ((mos_mach_name_t *)a[0])[i] = sp[i]; a[2] = 5; return 0; }
    for (int i = 0; i < 5; i++) if (sp[i].name == a[0]) {
        if (op == MACH_OP_TYPE) { a[1] = sp[i].type; return 0; }
        if (op == MACH_OP_STATUS) { a[4] = sp[i].name == 0x1107 ? 3 : 1; a[7] = sp[i].type & 0x20000 ? 0x1307 : 0; return 0; }
    }
    return 15;
}
static int fails, oks;
#define CHECK(c, m) do { if (c) oks++; else { fails++; printf("FALHA %d: %s\n", __LINE__, m); } } while (0)
int main(void) {
    u32 *nm, *ty, nn, nt;
    CHECK(mach_port_names(0x103, &nm, &nn, &ty, &nt) == 0 && nn == 5 && nt == 5 && nm[1] == 0x1107 && ty[3] == 0x80000, "names");
    CHECK(mach_port_names(0x4321, &nm, &nn, &ty, &nt) == 4, "outra task INVALID_ARGUMENT");
    u32 info[6], *tab, tc, trc; void *tr;
    CHECK(mach_port_space_info(0x103, info, &tab, &tc, &tr, &trc) == 0 && tc == 5 && info[1] == 5 && tab[2 * 7] == 0x1203 && tab[2 * 7 + 3] == 2 && trc == 0, "space_info");
    u32 bi[6];
    CHECK(mach_port_space_basic_info(0x103, bi) == 0 && bi[3] == 5, "space_basic_info");
    u32 sr = 0;
    CHECK(mach_port_get_srights(0x103, 0x1107, &sr) == 0 && sr == 3, "srights");
    CHECK(mach_port_get_srights(0x103, 0x303, &sr) == 17, "srights sem receive INVALID_RIGHT");
    CHECK(mach_port_get_srights(0x103, 0x7777, &sr) == 15, "nome inexistente INVALID_NAME");
    u32 *mem, mc;
    CHECK(mach_port_get_set_status(0x103, 0x1307, &mem, &mc) == 0 && mc == 2 && mem[0] == 0x1107 && mem[1] == 0x1203, "set_status");
    CHECK(mach_port_get_set_status(0x103, 0x1107, &mem, &mc) == 17, "set_status de porta comum");
    u32 ko; uint64_t addr;
    CHECK(mach_port_kobject(0x103, 0x103, &ko, &addr) == 0 && ko == 2, "kobject task");
    CHECK(mach_port_kobject(0x103, 0x1005, &ko, &addr) == 0 && ko == 1, "kobject thread");
    CHECK(mach_port_kobject(0x103, 0x303, &ko, &addr) == 0 && ko == 0, "kobject porta comum");
    CHECK(mach_port_kobject(0x103, 0x1203, &ko, &addr) == 0 && ko == 0, "porta comum na faixa 0x1000 não é thread");
    CHECK(mach_port_kobject(0x103, 0x1009, &ko, &addr) == 15, "thread inexistente INVALID_NAME");
    u32 t1, t2;
    CHECK(mach_port_dnrequest_info(0x103, 0x1107, &t1, &t2) == 0, "dnrequest_info");
    printf("machports: %d/%d ok\n", oks, oks + fails);
    return fails != 0;
}
