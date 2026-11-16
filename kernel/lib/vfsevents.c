/*
 * mOS — journal de eventos do VFS (o que alimenta o FSEvents do libSystem e o EVFILT_VNODE).
 *
 * Cada mudança vira um evento com id crescente, flags kFSEventStreamEventFlag*, inode, pid de quem fez e o
 * caminho. Os ids continuam depois do reboot (o próximo id vai no .vfsmeta), como no fseventsd; os eventos de
 * antes do boot não estão aqui, e quem pede um id antigo recebe KernelDropped|MustScanSubDirs.
 * Anel de VFSEV_CAP eventos; quem fica para trás também recebe KernelDropped. Modificações seguidas do mesmo
 * nó, que ninguém leu ainda, juntam num evento só (como o fseventsd junta pela latência).
 */
#include "vfs.h"
#include "syscall.h"

#define VFSEV_CAP 8192
typedef struct { u64 id, ino; u32 flags, pid; char *path; } vfsev_t;

static vfsev_t *ring;
static u64 next_id = 1;          /* próximo id a dar */
static u64 read_mark;            /* maior id já entregue a alguém (não juntar nele) */
static u64 base_id = 1;          /* 1º id deste boot */

__attribute__((weak)) int vfs_ev_pid(void) { return 0; }
int vfs_ev_mute;

void vfs_ev_set_base(u64 id) {   /* .vfsmeta: os ids seguem do boot anterior */
    if (id > next_id) next_id = base_id = id;
}
u64 vfs_ev_next_id(void) { return next_id; }

static u32 kind_flag(vnode_t *n) {
    if (n->dir) return FSEV_IS_DIR;
    if (n->data && n->size >= 10 && !memcmp(n->data, "#mOS-link:", 10)) return FSEV_IS_SYMLINK;
    return FSEV_IS_FILE;
}

static void push(u64 ino, u32 flags, const char *path) {
    if (!ring) {
        ring = kcalloc(VFSEV_CAP, sizeof *ring);
        if (!ring) return;
    }
    int pid = vfs_ev_pid();
    if (next_id > 1) {
        vfsev_t *last = &ring[(next_id - 1) % VFSEV_CAP];
        /* junta: mesmo nó, mesmo caminho, mesmo processo, só modificação/metadados, ainda não lido */
        u32 mergeable = FSEV_MODIFIED | FSEV_INODE_META | FSEV_XATTR | FSEV_FINDER | FSEV_OWNER;
        if (last->path && last->id > read_mark && last->ino == ino && last->pid == (u32)pid &&
            !(flags & ~(mergeable | FSEV_TYPE_MASK)) && !(last->flags & (FSEV_REMOVED | FSEV_RENAMED)) &&
            !strcmp(last->path, path)) {
            last->flags |= flags;
            return;
        }
    }
    usize pl = strlen(path);
    char *cp = kmalloc(pl + 1);
    if (!cp) return;
    memcpy(cp, path, pl + 1);
    vfsev_t *e = &ring[next_id % VFSEV_CAP];
    kfree(e->path);
    e->path = cp;
    e->id = next_id++;
    e->ino = ino;
    e->flags = flags;
    e->pid = (u32)pid;
}

/* caminhos fora do journal: /tmp/.orfaos (arquivos apagados com fd aberto) e o que o kernel guarda para si */
static bool quiet_path(const char *p) {
    return !strncmp(p, "/tmp/.orfaos", 12) || !strcmp(p, "/Sistema/Biblioteca/Logs/kernel.log");
}

void vfs_ev(vnode_t *n, u32 flags) {
    extern bool vfs_meta_live;
    if (!n || !vfs_meta_live || vfs_ev_mute) return;
    char *p = kmalloc(1024);
    if (!p) return;
    vfs_path(n, p, 1024);
    if (!quiet_path(p)) push(n->ino, flags | kind_flag(n), p);
    kfree(p);
}

/* rename: dois eventos seguidos com ItemRenamed (o caminho velho e o novo), como o fseventsd */
void vfs_ev_rename(vnode_t *n, const char *old_path) {
    extern bool vfs_meta_live;
    if (!n || !vfs_meta_live || vfs_ev_mute || !old_path) return;
    char *p = kmalloc(1024);
    if (!p) return;
    vfs_path(n, p, 1024);
    u32 k = kind_flag(n);
    bool qo = quiet_path(old_path), qn = quiet_path(p);
    /* para /tmp/.orfaos (apagado com fd aberto) é remoção; de lá de volta seria criação */
    if (!qo && qn) push(n->ino, FSEV_REMOVED | k, old_path);
    else if (qo && !qn) push(n->ino, FSEV_CREATED | k, p);
    else if (!qo) { push(n->ino, FSEV_RENAMED | k, old_path); push(n->ino, FSEV_RENAMED | k, p); }
    kfree(p);
}

/* montar/desmontar um volume em /Volumes */
void vfs_ev_mount(const char *path, bool mounted) {
    extern bool vfs_meta_live;
    if (vfs_meta_live) push(0, (mounted ? FSEV_MOUNT : FSEV_UNMOUNT) | FSEV_IS_DIR, path);
}

/* eventos com id > since, em ordem. Devolve quantos copiou para out (até max). *latest = último id dado.
 * Lacuna (since antes do anel ou de outro boot): o 1º é KernelDropped|MustScanSubDirs em "/". */
int vfs_ev_read(u64 since, void *outv, int max, u64 *latest) {
    mos_fsevent_t *out = outv;
    int k = 0;
    u64 last = next_id - 1;
    if (latest) *latest = last;
    if (max <= 0 || !out || since >= last) return 0;
    u64 oldest = next_id > VFSEV_CAP ? next_id - VFSEV_CAP : 1;
    if (oldest < base_id) oldest = base_id;
    u64 from = since + 1;
    if (from < oldest) {
        memset(&out[k], 0, sizeof out[k]);
        out[k].id = oldest - 1;
        out[k].flags = FSEV_KERNEL_DROPPED | FSEV_MUST_SCAN | FSEV_IS_DIR;
        out[k].path[0] = '/';
        k++;
        from = oldest;
    }
    for (u64 id = from; id <= last && k < max; id++) {
        vfsev_t *e = &ring[id % VFSEV_CAP];
        if (!ring || e->id != id || !e->path) continue;
        out[k].id = e->id;
        out[k].ino = e->ino;
        out[k].flags = e->flags;
        out[k].pid = e->pid;
        usize pl = strlen(e->path);
        if (pl >= sizeof out[k].path) pl = sizeof out[k].path - 1;
        memcpy(out[k].path, e->path, pl);
        out[k].path[pl] = 0;
        if (e->id > read_mark) read_mark = e->id;
        k++;
    }
    return k;
}
