/* lado kernel do tools/attrlist_test.sh: o SYS_FSMETA do process.c, sem cópia de/para o usuário */
#include "vfs_host_kernel.h"
#include "vfs.h"
#include "syscall.h"
long long g_live_allocs, g_live_bytes;
static u64 fake_ms = 1000;
u64 timer_ticks(void) { return fake_ms; }
void rtc_now(datetime_t *dt) { *dt = (datetime_t){2026, 11, 15, 12, 0, 0, 0}; }
uint32_t rand32(void) { return 4; }
static boot_info_t boot0;
boot_info_t *g_boot = &boot0;
bool persist_load(void) { return false; }
void persist_enable(void) {}
void persist_mark(vnode_t *n) { (void)n; }
void persist_delete(vnode_t *n) { (void)n; }
void persist_move(const char *o, vnode_t *n) { (void)o; (void)n; }
bool persist_available(void) { return false; }
void persist_meta_mark(void) {}
int k_euid = 501, k_egid = 20;
const char *k_fd_path[64];
long kfsm(int op, mos_fsmeta_t *rq) {
    if (op == MOS_FSM_EVENTID) { u64 l = 0; vfs_ev_read(~0ull, 0, 0, &l); return (long)l; }
    if (op == MOS_FSM_FSGETPATH) {
        vnode_t *t = vfs_by_ino(rq->a);
        if (!t) return -2;
        char p[1024]; vfs_path(t, p, sizeof p);
        usize n = strlen(p) + 1;
        if (rq->buf) { if (rq->size < n) return -34; memcpy((void *)rq->buf, p, n); }
        return (long)n;
    }
    const char *path = rq->fd >= 0 ? (rq->fd < 64 ? k_fd_path[rq->fd] : 0) : (const char *)rq->path;
    if (!path) return -9;
    vnode_t *v = vfs_lookup(path);
    if (!v) return -2;
    switch (op) {
    case MOS_FSM_STATFS: return vfs_statfs(v, (void *)rq->buf);
    case MOS_FSM_CLONE: return vfs_clone(v, (const char *)rq->name, (u32)rq->a, k_euid, k_egid);
    case MOS_FSM_EXCHANGE: { vnode_t *d = vfs_lookup((const char *)rq->name); return d ? vfs_exchange(v, d) : -2; }
    case MOS_FSM_RENAMEX: {
        vnode_t *d = vfs_lookup((const char *)rq->name);
        if ((rq->a & 6) == 6) return -22;
        if (rq->a & 4) return d ? -17 : 1;
        if (rq->a & 2) return d ? vfs_swap(v, d) : -2;
        return 1;
    }
    case MOS_FSM_ACCESS: return vfs_fsmeta(v, op, rq, 0, 0, k_euid, k_egid);
    default: return vfs_fsmeta(v, op, rq, (const char *)rq->name, (void *)rq->buf, k_euid, k_egid);
    }
}
long kchmod(const char *p, unsigned m) {
    vnode_t *v = vfs_lookup(p);
    if (!v) return -2;
    v->mode = (m & 07777) | 0x10000u;
    return 0;
}
