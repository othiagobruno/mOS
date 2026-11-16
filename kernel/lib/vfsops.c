/*
 * mOS — operações de VFS do macOS que vão além de open/rename/unlink: statfs por volume, renamex_np
 * (RENAME_SWAP/RENAME_EXCL), clonefile (cópia com dados, xattrs, modo, flags e datas), exchangedata e fsgetpath
 * (inode -> caminho). Chamadas pelo SYS_FSMETA (proc/process.c); as regras de permissão são as de vfsperm.c.
 */
#include "vfs.h"
#include "syscall.h"

/* ganchos: volume de /Volumes (fs/vfs_bridge.c) e o disco do sistema (fs/persist.c) */
__attribute__((weak)) bool vfs_volume_info(vnode_t *vol, mos_statfs_t *o) { (void)vol; (void)o; return false; }
__attribute__((weak)) bool persist_space(u64 *total, u64 *avail, char *label, usize cap) { (void)total; (void)avail; (void)label; (void)cap; return false; }

static void copy_str(char *d, usize cap, const char *s) {
    usize n = strlen(s);
    if (n >= cap) n = cap - 1;
    memcpy(d, s, n);
    d[n] = 0;
}

static u64 count_nodes(vnode_t *d) {
    u64 n = 1;
    for (vnode_t *c = d->child; c; c = c->next) n += c->dir ? count_nodes(c) : 1;
    return n;
}

int vfs_statfs(vnode_t *v, void *ov) {
    mos_statfs_t *o = ov;
    memset(o, 0, sizeof *o);
    o->bsize = 4096;
    for (vnode_t *p = v; p; p = p->parent) {
        if (!vfs_is_volume(p)) continue;
        char path[300];
        vfs_path(p, path, sizeof path);
        copy_str(o->mnton, sizeof o->mnton, path);
        copy_str(o->volname, sizeof o->volname, p->name);
        o->flags = 0x1000u | 0x100000u;                         /* MNT_LOCAL | MNT_DONTBROWSE (o Finder mostra pelo /Volumes) */
        if (!vfs_ext_writable(p)) o->flags |= 1;               /* MNT_RDONLY */
        u32 h = 2166136261u;
        for (const char *s = p->name; *s; s++) h = (h ^ (u8)*s) * 16777619u;
        o->fsid = 0x1000010u + (h & 0xffffu);
        if (!vfs_volume_info(p, o)) copy_str(o->fstype, sizeof o->fstype, "hfs");
        return 0;
    }
    /* o volume do sistema: FAT32 no disco (\mOS\) ou só RAM */
    copy_str(o->mnton, sizeof o->mnton, "/");
    o->fsid = 0x1000002u;
    o->flags = 0x1000u | 0x4000u;                              /* MNT_LOCAL | MNT_ROOTFS */
    char label[64] = "";
    u64 total = 0, avail = 0;
    if (persist_space(&total, &avail, label, sizeof label)) {
        copy_str(o->fstype, sizeof o->fstype, "msdos");
        copy_str(o->mntfrom, sizeof o->mntfrom, "/dev/disk0s1");
        copy_str(o->volname, sizeof o->volname, label[0] ? label : "mOS");
        o->total = total;
        o->avail = avail;
    } else {
        copy_str(o->fstype, sizeof o->fstype, "tmpfs");       /* sistema só na RAM (live sem persistência) */
        copy_str(o->mntfrom, sizeof o->mntfrom, "tmpfs");
        copy_str(o->volname, sizeof o->volname, "mOS");
        o->total = (u64)vfs_total_bytes() + (512ull << 20);
        o->avail = 512ull << 20;
    }
    u64 n = count_nodes(vfs_root());
    o->files = n + 1000000;
    o->ffree = 1000000;
    return 0;
}

/* ------------------------------------------------------------------ inode -> caminho */
static vnode_t *find_ino(vnode_t *d, u64 ino, int depth) {
    if (depth > 256) return NULL;
    for (vnode_t *c = d->child; c; c = c->next) {
        if (c->ino == ino) return c;
        if (c->dir && !c->ext) { vnode_t *r = find_ino(c, ino, depth + 1); if (r) return r; }
    }
    return NULL;
}
vnode_t *vfs_by_ino(u64 ino) {
    if (ino == 2 || ino == 1) return vfs_root();
    return find_ino(vfs_root(), ino, 0);
}

/* ------------------------------------------------------------------ clonefile */
static void copy_meta(vnode_t *d, vnode_t *s, bool own, int euid, int egid) {
    d->mode = s->mode;
    d->flags = s->flags & ~(VFS_SF_SETTABLE & ~VFS_SF_ARCHIVED);   /* SF_* (menos arquivado) não passa ao clone */
    if (own) { d->uid = s->uid; d->gid = s->gid; } else { d->uid = (u32)euid; d->gid = (u32)egid; }
    vfs_xattr_free(d);
    vfs_xattr_copy(d, s);
    d->btime_ns = s->btime_ns ? s->btime_ns : vfs_time_ns(s, VT_BTIME);
    d->mtime_ns = vfs_time_ns(s, VT_MTIME);
    d->atime_ns = vfs_time_ns(s, VT_ATIME);
    vfs_meta_changed(d);
}

static int clone_rec(vnode_t *s, const char *dst, bool own, int euid, int egid, int depth) {
    if (depth > 128) return -MOS_ENAMETOOLONG;
    vnode_t *d;
    if (s->dir) {
        d = vfs_mkdir(dst);
        if (!d) return -MOS_ENOENT;
        char *p = kmalloc(1024);
        if (!p) return -MOS_ENOMEM;
        int r = 0;
        for (vnode_t *c = s->child; c && !r; c = c->next) {
            usize a = strlen(dst), b = strlen(c->name);
            if (a + 1 + b + 1 > 1024) { r = -MOS_ENAMETOOLONG; break; }
            memcpy(p, dst, a); p[a] = '/'; memcpy(p + a + 1, c->name, b + 1);
            r = clone_rec(c, p, own, euid, egid, depth + 1);
        }
        kfree(p);
        if (r) return r;
    } else {
        extern bool vfs_ensure(vnode_t *n);
        if (s->ext) vfs_ensure(s);
        d = vfs_write_file(dst, s->data ? s->data : (const void *)"", s->data ? s->size : 0);
        if (!d) return -MOS_ENOENT;
    }
    copy_meta(d, s, own, euid, egid);
    return 0;
}

int vfs_clone(vnode_t *s, const char *dst, u32 flags, int euid, int egid) {
    if (!s || !dst || !dst[0]) return -MOS_EINVAL;
    if (vfs_lookup(dst)) return -MOS_EEXIST;
    /* CLONE_NOOWNERCOPY (2): o clone é de quem chamou; sem ele o dono só é mantido por root (como no xnu) */
    bool own = !(flags & 2) && (euid == 0 || (u32)euid == s->uid);
    return clone_rec(s, dst, own, euid, egid, 0);
}

/* ------------------------------------------------------------------ exchangedata */
int vfs_exchange(vnode_t *a, vnode_t *b) {
    if (!a || !b || a->dir || b->dir) return -MOS_EINVAL;
    if (a == b) return 0;
    if (vfs_is_read_only(a) || vfs_is_read_only(b)) return -MOS_EROFS;
    if (vfs_immutable(a) || vfs_immutable(b)) return -MOS_EPERM;
    u8 *d = a->data; usize sz = a->size, cap = a->cap;
    a->data = b->data; a->size = b->size; a->cap = b->cap;
    b->data = d; b->size = sz; b->cap = cap;
    void *e = a->ext; a->ext = b->ext; b->ext = e;           /* conteúdo ainda no volume vai junto */
    i64 m = a->mtime_ns; a->mtime_ns = b->mtime_ns; b->mtime_ns = m;
    vfs_meta_changed(a);
    vfs_meta_changed(b);
    vfs_ev(a, FSEV_MODIFIED);
    vfs_ev(b, FSEV_MODIFIED);
    persist_mark(a);
    persist_mark(b);
    return 0;
}

/* ------------------------------------------------------------------ renamex_np: troca dois nomes */
int vfs_swap(vnode_t *a, vnode_t *b) {
    if (!a || !b || a == b) return a == b ? 0 : -MOS_EINVAL;
    for (vnode_t *p = b; p; p = p->parent) if (p == a) return -MOS_EINVAL;   /* um dentro do outro */
    for (vnode_t *p = a; p; p = p->parent) if (p == b) return -MOS_EINVAL;
    vnode_t *pa = a->parent, *pb = b->parent;
    char na[256], nb[256], tmp[256];
    copy_str(na, sizeof na, a->name);
    copy_str(nb, sizeof nb, b->name);
    snprintf(tmp, sizeof tmp, ".mos-swap-%llu", (unsigned long long)a->ino);
    if (!vfs_move(a, pa, tmp)) return -MOS_EINVAL;
    if (!vfs_move(b, pa, na)) { vfs_move(a, pa, na); return -MOS_EINVAL; }
    if (!vfs_move(a, pb, nb)) { vfs_move(b, pb, nb); vfs_move(a, pa, na); return -MOS_EINVAL; }
    return 0;
}
