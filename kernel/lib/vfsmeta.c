/*
 * mOS — metadados do VFS: inode, dono, flags BSD, datas em ns e atributos estendidos (xattr).
 *
 * Vale para todo vnode (disco do sistema, RAM e volumes de /Volumes). O que muda aqui chega ao disco do sistema
 * pelo \mOS\.vfsmeta (fs/persist.c, persist_meta_mark); em /Volumes fica na RAM até o driver gravar (HFS+: fase 5).
 * Semântica do macOS (xnu bsd/vfs/vfs_xattr.c e vfs_syscalls.c):
 *   - setxattr com XATTR_CREATE e o nome já existe: EEXIST; XATTR_REPLACE e não existe: ENOATTR;
 *   - "position" só vale para com.apple.ResourceFork (nos outros: EINVAL se != 0);
 *   - com.apple.FinderInfo tem 32 bytes exatos (senão ERANGE); getxattr com buffer pequeno: ERANGE;
 *   - nome vazio: EINVAL; nome com mais de 127 bytes: ENAMETOOLONG;
 *   - listxattr com buf NULL (cap 0) devolve o tamanho necessário.
 */
#include "vfs.h"
#include "syscall.h"
#include "cred.h"

#define E_PERM 1
#define E_NOMEM 12
#define E_EXIST 17
#define E_INVAL 22
#define E_NOSPC 28
#define E_RANGE 34
#define E_NAMETOOLONG 63
#define E_NOATTR 93

/* fim do vfs_init: daqui em diante uma mudança marca o nó para o .vfsmeta (a carga do disco não conta) */
bool vfs_meta_live;

/* ------------------------------------------------------------------ relógio */
#ifndef MOS_HOST_TEST
void rtc_now(datetime_t *dt);
#endif
static i64 boot_ns;          /* hora Unix (ns) em que timer_ticks() valia 0 */

static i64 rtc_unix(void) {
    datetime_t dt;
    rtc_now(&dt);
    if (dt.year < 1971 || dt.month < 1 || dt.month > 12) return 0;
    int y = dt.year - (dt.month <= 2), era = y / 400, yoe = y - era * 400;
    int doy = (153 * (dt.month + (dt.month > 2 ? -3 : 9)) + 2) / 5 + dt.day - 1;
    i64 days = (i64)era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
    return days * 86400 + dt.hour * 3600 + dt.minute * 60 + dt.second;
}

/* ns desde 1970. O RTC é lido uma vez (e de novo se ainda não respondia); depois vale o contador de ms do boot,
 * então duas chamadas seguidas nunca voltam no tempo. */
i64 vfs_now_ns(void) {
    if (!boot_ns) {
        i64 u = rtc_unix();
        if (u) boot_ns = u * 1000000000ll - (i64)timer_ticks() * 1000000ll;
    }
    static i64 last;
    i64 t = boot_ns + (i64)timer_ticks() * 1000000ll;
    if (t <= last) t = last + 1;       /* estritamente crescente: dois writes no mesmo ms mudam o mtime */
    last = t;
    return t;
}

static i64 boot_time_ns(void) { if (!boot_ns) vfs_now_ns(); return boot_ns; }

i64 vfs_time_ns(const vnode_t *n, int which) {
    i64 t = which == VT_ATIME ? n->atime_ns : which == VT_MTIME ? n->mtime_ns : which == VT_CTIME ? n->ctime_ns : n->btime_ns;
    if (t) return t;
    if (which != VT_MTIME && n->mtime_ns) return n->mtime_ns;
    /* nó antigo (veio do disco sem .vfsmeta): o mtime em ms do boot, ou a hora do boot */
    return boot_time_ns() + (i64)n->mtime * 1000000ll;
}

/* ------------------------------------------------------------------ inode */
static u64 ino_next = 16;    /* 2 = raiz (como no HFS+/APFS); abaixo de 16 reservado */
u64 vfs_ino_next(void) { return ino_next++; }
void vfs_ino_reserve(u64 ino) { if (ino >= ino_next) ino_next = ino + 1; }

void vfs_meta_init(vnode_t *n, vnode_t *parent) {
    (void)parent;
    n->ino = vfs_ino_next();
    n->uid = VFS_DEFAULT_UID;
    n->gid = VFS_DEFAULT_GID;
    i64 t = vfs_now_ns();
    n->atime_ns = n->mtime_ns = n->ctime_ns = n->btime_ns = t;
}

void vfs_touch(vnode_t *n, int which) {
    if (!n) return;
    i64 t = vfs_now_ns();
    if (which & VT_ATIME) n->atime_ns = t;
    if (which & VT_MTIME) { n->mtime_ns = t; n->mtime = timer_ticks(); if (!n->mtime) n->mtime = 1; }
    if (which & VT_CTIME) n->ctime_ns = t;
    if (which & VT_BTIME) n->btime_ns = t;
    if (!n->btime_ns) n->btime_ns = n->mtime_ns ? n->mtime_ns : t;
    if (vfs_meta_live && !n->meta) n->meta = true;
    if (vfs_meta_live) persist_meta_mark();
}

void vfs_meta_changed(vnode_t *n) {
    if (!n) return;
    n->ctime_ns = vfs_now_ns();
    if (!n->btime_ns) n->btime_ns = vfs_time_ns(n, VT_MTIME);
    if (!vfs_meta_live) return;
    n->meta = true;
    persist_meta_mark();
}

/* ------------------------------------------------------------------ flags */
bool vfs_immutable(vnode_t *n) { return n && (n->flags & (VFS_UF_IMMUTABLE | VFS_SF_IMMUTABLE)); }
bool vfs_append_only(vnode_t *n) { return n && (n->flags & (VFS_UF_APPEND | VFS_SF_APPEND)); }
bool vfs_no_unlink(vnode_t *n) {
    return n && (n->flags & (VFS_UF_IMMUTABLE | VFS_SF_IMMUTABLE | VFS_UF_APPEND | VFS_SF_APPEND | VFS_SF_NOUNLINK));
}

/* ------------------------------------------------------------------ xattr */
static int check_name(const char *name) {
    if (!name || !name[0]) return -E_INVAL;
    if (strlen(name) > VFS_XATTR_NAME_MAX) return -E_NAMETOOLONG;
    return 0;
}
static bool is_rsrc(const char *name) { return !strcmp(name, "com.apple.ResourceFork"); }
static bool is_finfo(const char *name) { return !strcmp(name, "com.apple.FinderInfo"); }

vxattr_t *vfs_xattr_find(vnode_t *n, const char *name) {
    for (vxattr_t *x = n ? n->xattr : NULL; x; x = x->next) if (!strcmp(x->name, name)) return x;
    return NULL;
}

/* FinderInfo só de zeros conta como ausente (o macOS não lista nem devolve) */
static bool finfo_empty(const vxattr_t *x) {
    for (u32 i = 0; i < x->len; i++) if (x->val[i]) return false;
    return true;
}

i64 vfs_xattr_get(vnode_t *n, const char *name, void *buf, u64 cap, u64 pos) {
    int e = check_name(name);
    if (e) return e;
    if (pos && !is_rsrc(name)) return -E_INVAL;
    vxattr_t *x = vfs_xattr_find(n, name);
    if (!x || (is_finfo(name) && finfo_empty(x))) return -E_NOATTR;
    if (pos > x->len) return -E_INVAL;
    u64 avail = x->len - pos;
    if (!buf) return (i64)avail;                 /* só o tamanho */
    if (cap < avail) {
        if (!is_rsrc(name)) return -E_RANGE;
        avail = cap;                             /* o resource fork se lê em pedaços */
    }
    memcpy(buf, x->val + pos, avail);
    return (i64)avail;
}

static void link_sorted(vnode_t *n, vxattr_t *x) {
    vxattr_t **pp = &n->xattr;
    while (*pp && strcmp((*pp)->name, x->name) < 0) pp = &(*pp)->next;
    x->next = *pp;
    *pp = x;
}

int vfs_xattr_set(vnode_t *n, const char *name, const void *val, u64 len, u64 pos, int opts) {
    int e = check_name(name);
    if (e) return e;
    if (!n) return -E_INVAL;
    if ((opts & VFS_XATTR_CREATE) && (opts & VFS_XATTR_REPLACE)) return -E_INVAL;
    if (pos && !is_rsrc(name)) return -E_INVAL;
    if (is_finfo(name) && len != 32) return -E_RANGE;
    if (len > VFS_XATTR_VAL_MAX || pos > VFS_XATTR_VAL_MAX - len) return -E_NOSPC;
    vxattr_t *x = vfs_xattr_find(n, name);
    bool exists = x && !(is_finfo(name) && finfo_empty(x));
    if ((opts & VFS_XATTR_CREATE) && exists) return -E_EXIST;
    if ((opts & VFS_XATTR_REPLACE) && !exists) return -E_NOATTR;
    u64 newlen = pos ? MAX(pos + len, x ? (u64)x->len : 0) : len;   /* com position o resto do fork fica */
    u64 old = x ? x->len : 0;
    if (n->xattr_bytes - old + newlen > VFS_XATTR_NODE_MAX) return -E_NOSPC;
    u8 *nv = kmalloc(newlen ? newlen : 1);
    if (!nv) return -E_NOMEM;
    if (pos) {
        memset(nv, 0, newlen);
        if (x) memcpy(nv, x->val, x->len);
        memcpy(nv + pos, val, len);
    } else if (len) memcpy(nv, val, len);
    if (!x) {
        x = kcalloc(1, sizeof *x);
        if (!x) { kfree(nv); return -E_NOMEM; }
        strcpy(x->name, name);
        link_sorted(n, x);
    } else kfree(x->val);
    x->val = nv;
    x->len = (u32)newlen;
    n->xattr_bytes = n->xattr_bytes - old + newlen;
    if (is_finfo(name)) {                         /* kIsInvisible (bit 14 do Finder flags, byte 8) = UF_HIDDEN */
        bool inv = nv[8] & 0x40;
        if (inv) n->flags |= VFS_UF_HIDDEN; else n->flags &= ~VFS_UF_HIDDEN;
    }
    vfs_meta_changed(n);
    return 0;
}

i64 vfs_xattr_list(vnode_t *n, char *buf, u64 cap) {
    u64 need = 0;
    for (vxattr_t *x = n ? n->xattr : NULL; x; x = x->next) {
        if (is_finfo(x->name) && finfo_empty(x)) continue;
        if (!strcmp(x->name, VFS_XATTR_SECURITY)) continue;          /* a ACL não é listada (xnu) */
        u64 k = strlen(x->name) + 1;
        if (buf && cap) {
            if (need + k > cap) return -E_RANGE;
            memcpy(buf + need, x->name, k);
        }
        need += k;
    }
    return (i64)need;
}

int vfs_xattr_remove(vnode_t *n, const char *name) {
    int e = check_name(name);
    if (e) return e;
    if (!n) return -E_INVAL;
    for (vxattr_t **pp = &n->xattr; *pp; pp = &(*pp)->next) {
        vxattr_t *x = *pp;
        if (strcmp(x->name, name)) continue;
        bool empty = is_finfo(name) && finfo_empty(x);
        *pp = x->next;
        n->xattr_bytes -= x->len;
        kfree(x->val);
        kfree(x);
        if (is_finfo(name)) n->flags &= ~VFS_UF_HIDDEN;
        if (empty) return -E_NOATTR;
        vfs_meta_changed(n);
        return 0;
    }
    return -E_NOATTR;
}

void vfs_xattr_free(vnode_t *n) {
    for (vxattr_t *x = n->xattr, *nx; x; x = nx) { nx = x->next; kfree(x->val); kfree(x); }
    n->xattr = NULL;
    n->xattr_bytes = 0;
}

bool vfs_xattr_copy(vnode_t *dst, const vnode_t *src) {
    bool ok = true;
    for (vxattr_t *x = src->xattr; x; x = x->next)
        if (vfs_xattr_set(dst, x->name, x->val, x->len, 0, 0) < 0) ok = false;
    return ok;
}

/* ------------------------------------------------------------------ SYS_FSMETA (núcleo; process.c copia de/para o usuário) */
static bool is_link(const vnode_t *v) {
    return !v->dir && v->data && v->size >= 10 && !memcmp(v->data, "#mOS-link:", 10);
}

static u64 dev_of(vnode_t *v) {
    for (vnode_t *p = v; p; p = p->parent)
        if (vfs_is_volume(p)) {
            u32 h = 2166136261u;
            for (const char *s = p->name; *s; s++) h = (h ^ (u8)*s) * 16777619u;
            return 0x1000010u + (h & 0xffffu);
        }
    return 0x1000002u;   /* o volume do sistema (o mesmo st_dev de antes) */
}

void vfs_statx(vnode_t *v, void *stv) {
    mos_statx_t *st = stv;
    memset(st, 0, sizeof *st);
    st->ino = v->ino ? v->ino : 2;
    st->dev = dev_of(v);
    st->kind = v->dir ? 1 : 0;
    st->size = v->dir ? 0 : v->size;
    st->blocks = (st->size + 511) / 512;
    st->mode = mode_stat(v->mode, v->dir) | (v->mode & 0x10000u);   /* 0x10000: modo gravado (chmod/creat) */
    st->uid = v->uid;
    st->gid = v->gid;
    st->flags = v->flags;
    if (v->dir) {
        st->nlink = 2;
        for (vnode_t *c = v->child; c; c = c->next) if (c->dir) st->nlink++;
    } else st->nlink = 1;
    for (vxattr_t *x = v->xattr; x; x = x->next) if (strcmp(x->name, VFS_XATTR_SECURITY)) st->nxattr++;
    st->atime_ns = vfs_time_ns(v, VT_ATIME);
    st->mtime_ns = vfs_time_ns(v, VT_MTIME);
    st->ctime_ns = vfs_time_ns(v, VT_CTIME);
    st->btime_ns = vfs_time_ns(v, VT_BTIME);
}

static bool hidden_name(const char *n) { return n && !strcmp(n, VFS_XATTR_SECURITY); }

i64 vfs_fsmeta(vnode_t *v, int op, const void *rqv, const char *name, void *kbuf, int euid, int egid) {
    const mos_fsmeta_t *rq = rqv;
    bool root = euid == 0;
    bool owner = root || (u32)euid == v->uid;
    bool ro = vfs_is_read_only(v);
    switch (op) {
    case MOS_FSM_STATX:
        if (rq->size < sizeof(mos_statx_t)) return -MOS_EINVAL;
        vfs_statx(v, kbuf);
        return 0;
    case MOS_FSM_GETXATTR: {
        int e = vfs_authorize(v, VFS_KR_READ_EXTATTR, euid, egid);
        if (e) return e;
        if (hidden_name(name)) return -MOS_ENOATTR;    /* a ACL não aparece como xattr (como no xnu) */
        return vfs_xattr_get(v, name, kbuf, rq->size, rq->pos);
    }
    case MOS_FSM_LISTXATTR: {
        int e = vfs_authorize(v, VFS_KR_READ_EXTATTR, euid, egid);
        return e ? e : vfs_xattr_list(v, kbuf, kbuf ? rq->size : 0);
    }
    case MOS_FSM_ACCESS:                               /* access(2): rq->a = R 4 | W 2 | X 1 (0 = existe) */
        if ((rq->a & 2) && ro) return -MOS_EROFS;
        if ((rq->a & 2) && vfs_immutable(v)) return -MOS_EPERM;
        return vfs_access(v, (int)(rq->a & 7), euid, egid);
    case MOS_FSM_GETACL: {                             /* filesec inteiro (buf 0 = tamanho) */
        int e = vfs_authorize(v, VFS_KR_READ_SECURITY, euid, egid);
        if (e) return e;
        vxattr_t *x = vfs_xattr_find(v, VFS_XATTR_SECURITY);
        if (!x) return -MOS_ENOATTR;
        if (!kbuf) return x->len;
        if (rq->size < x->len) return -MOS_ERANGE;
        memcpy(kbuf, x->val, x->len);
        return x->len;
    }
    case MOS_FSM_SETACL: {                             /* size 0 = tira a ACL */
        if (ro) return -MOS_EROFS;
        if (vfs_immutable(v)) return -MOS_EPERM;
        int e = vfs_authorize(v, VFS_KR_WRITE_SECURITY, euid, egid);
        if (e) return e;
        if (!rq->size) {
            int r = vfs_xattr_remove(v, VFS_XATTR_SECURITY);
            return r == -MOS_ENOATTR ? 0 : r;
        }
        if (!vfs_filesec_valid(kbuf, rq->size)) return -MOS_EINVAL;
        return vfs_xattr_set(v, VFS_XATTR_SECURITY, kbuf, rq->size, 0, 0);
    }
    case MOS_FSM_SETXATTR:
    case MOS_FSM_REMOVEXATTR: {
        if (ro) return -MOS_EROFS;
        if (vfs_immutable(v)) return -MOS_EPERM;
        int e = check_name(name);
        if (e) return e;
        if (hidden_name(name)) return op == MOS_FSM_SETXATTR ? -MOS_EPERM : -MOS_ENOATTR;   /* ACL: só por SETACL */
        e = vfs_authorize(v, VFS_KR_WRITE_EXTATTR, euid, egid);
        if (e) return e;
        /* com.apple.system.*: só o kernel (ACL em com.apple.system.Security, fase 2) */
        if (!strncmp(name, "com.apple.system.", 17) && !root) return -MOS_EPERM;
        return op == MOS_FSM_SETXATTR ? vfs_xattr_set(v, name, kbuf, rq->size, rq->pos, (int)rq->opts)
                                      : vfs_xattr_remove(v, name);
    }
    case MOS_FSM_CHFLAGS: {
        u32 nf = (u32)rq->a;
        if (ro) return -MOS_EROFS;
        if (!owner) return -MOS_EPERM;
        /* sem root, os SF_* ficam como estão (o xnu devolve EPERM se o pedido os muda) */
        if (!root && ((nf ^ v->flags) & VFS_SF_SETTABLE)) return -MOS_EPERM;
        if (!root && (v->flags & (VFS_SF_IMMUTABLE | VFS_SF_APPEND))) return -MOS_EPERM;
        nf = (nf & ~VFS_UF_COMPRESSED)   /* compressão é do fs, não do usuário */ | (v->flags & VFS_UF_COMPRESSED);
        v->flags = nf;
        vfs_meta_changed(v);
        return 0;
    }
    case MOS_FSM_CHOWN: {
        u32 nu = (u32)rq->a, ng = (u32)rq->b;
        if (ro) return -MOS_EROFS;
        if (vfs_immutable(v)) return -MOS_EPERM;
        if (nu == 0xffffffffu) nu = v->uid;
        if (ng == 0xffffffffu) ng = v->gid;
        if (!root) {
            if (!owner || nu != v->uid) return -MOS_EPERM;
            if (ng != v->gid && !vfs_in_group(ng, euid, egid)) return -MOS_EPERM;
        }
        bool changed = nu != v->uid || ng != v->gid;
        v->uid = nu;
        v->gid = ng;
        if (changed && !root) v->mode &= ~06000u;   /* chown sem root tira setuid/setgid */
        vfs_meta_changed(v);
        return 0;
    }
    case MOS_FSM_SETTIMES: {
        u32 mask = (u32)rq->a, now = (u32)rq->b;
        if (ro) return -MOS_EROFS;
        if (vfs_immutable(v)) return -MOS_EPERM;
        /* datas explícitas: só o dono (ou root, ou ACL com WRITE_ATTRIBUTES); "agora" nas duas (utimes(NULL)):
         * basta poder escrever, como no xnu */
        bool only_now = (mask & ~(u32)(VT_ATIME | VT_MTIME)) == 0 && (now & mask) == mask;
        if (only_now) { if (!owner && vfs_access(v, 2, euid, egid)) return -MOS_EACCES; }
        else if (!owner && vfs_authorize(v, VFS_KR_WRITE_ATTRIBUTES, euid, egid)) return -MOS_EPERM;
        i64 t = vfs_now_ns();
        i64 *slot[4] = {&v->atime_ns, &v->mtime_ns, &v->ctime_ns, &v->btime_ns};
        for (int i = 0; i < 4; i++) {
            u32 bit = 1u << i;
            if (!(mask & bit)) continue;
            *slot[i] = (now & bit) ? t : rq->t[i];
        }
        if (mask & VT_MTIME) {
            v->mtime = timer_ticks();
            if (!v->mtime) v->mtime = 1;
            /* criação depois da modificação não existe no HFS+/APFS: o setattrlist do macOS ajusta igual */
            if (v->btime_ns > v->mtime_ns && !(mask & VT_BTIME)) v->btime_ns = v->mtime_ns;
        }
        i64 keep_c = v->ctime_ns;
        vfs_meta_changed(v);
        if (mask & VT_CTIME) v->ctime_ns = keep_c;    /* pedido explícito de ctime vale (root/restauração) */
        return 0;
    }
    case MOS_FSM_READDIR: {
        mos_fsm_dirent_t *d = kbuf;
        if (!v->dir) return -MOS_ENOTDIR;
        if (rq->pos == 0) { int e = vfs_access(v, 4, euid, egid); if (e) return e; }
        if (rq->size < sizeof *d) return -MOS_EINVAL;
        vnode_t *e = v->child;
        for (u64 i = 0; e && i < rq->pos; i++) e = e->next;
        if (!e) return 0;
        memset(d, 0, sizeof *d);
        d->ino = e->ino ? e->ino : 2;
        d->type = e->dir ? 4 : is_link(e) ? 10 : 8;
        d->namelen = (unsigned)strlen(e->name);
        memcpy(d->name, e->name, d->namelen);
        return 1;
    }
    }
    return -MOS_EINVAL;
}

/* ------------------------------------------------------------------ .vfsmeta (formato em fs/persist.c) */
u32 vfs_crc32(const void *d, u64 n) {
    static u32 tab[256];
    if (!tab[1]) for (u32 i = 0; i < 256; i++) {
        u32 c = i;
        for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        tab[i] = c;
    }
    u32 c = 0xFFFFFFFFu;
    const u8 *p = d;
    for (u64 i = 0; i < n; i++) c = tab[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

typedef struct { u8 *b; u64 n, cap; bool err; } wbuf_t;
static void wput(wbuf_t *w, const void *src, u64 n) {
    if (w->err) return;
    if (w->n + n > w->cap) {
        u64 nc = MAX(w->cap * 2, w->n + n + 4096);
        u8 *nb = kmalloc(nc);
        if (!nb) { w->err = true; return; }
        if (w->n) memcpy(nb, w->b, w->n);
        kfree(w->b);
        w->b = nb;
        w->cap = nc;
    }
    memcpy(w->b + w->n, src, n);
    w->n += n;
}
static void w8(wbuf_t *w, u8 v) { wput(w, &v, 1); }
static void w16(wbuf_t *w, u16 v) { u8 b[2] = {(u8)v, (u8)(v >> 8)}; wput(w, b, 2); }
static void w32(wbuf_t *w, u32 v) { u8 b[4]; for (int i = 0; i < 4; i++) b[i] = (u8)(v >> (8 * i)); wput(w, b, 4); }
static void w64(wbuf_t *w, u64 v) { u8 b[8]; for (int i = 0; i < 8; i++) b[i] = (u8)(v >> (8 * i)); wput(w, b, 8); }
static u16 r16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u32 r32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u64 r64(const u8 *p) { return (u64)r32(p) | (u64)r32(p + 4) << 32; }

#define VMETA_MAGIC "MOSVMETA"
#define VMETA_HDR 24

u8 *vmeta_build(vnode_t *root, vmeta_track_fn track, void *ctx, u64 *len) {
    wbuf_t w = {0};
    char *vp = kmalloc(1024);
    int cap = 256, sp = 0;
    vnode_t **st = kmalloc(sizeof(vnode_t *) * (usize)cap);
    if (!vp || !st) { kfree(vp); kfree(st); return NULL; }
    wput(&w, VMETA_MAGIC, 8);
    w32(&w, 1);
    w32(&w, 0);                       /* registros (preenchido no fim) */
    w64(&w, ino_next);
    u32 count = 0;
    st[sp++] = root;
    while (sp && !w.err) {            /* iterativo: as árvores de node_modules passam de 64 níveis */
        vnode_t *v = st[--sp];
        if (v != root && v->meta && track(ctx, v, vp, 1024)) {
            usize pl = strlen(vp);
            u16 nx = 0;
            for (vxattr_t *x = v->xattr; x; x = x->next) nx++;
            w16(&w, (u16)pl); wput(&w, vp, pl);
            w8(&w, v->dir);
            w32(&w, v->mode); w32(&w, v->uid); w32(&w, v->gid); w32(&w, v->flags);
            w64(&w, v->ino);
            w64(&w, (u64)v->atime_ns); w64(&w, (u64)v->mtime_ns); w64(&w, (u64)v->ctime_ns); w64(&w, (u64)v->btime_ns);
            w16(&w, nx);
            for (vxattr_t *x = v->xattr; x; x = x->next) {
                u8 nl = (u8)strlen(x->name);
                w8(&w, nl); wput(&w, x->name, nl);
                w32(&w, x->len); wput(&w, x->val, x->len);
            }
            count++;
        }
        if (!v->dir || v->ext) continue;   /* volumes externos não entram */
        for (vnode_t *c = v->child; c; c = c->next) {
            if (sp == cap) {
                vnode_t **ns = kmalloc(sizeof(vnode_t *) * (usize)cap * 2);
                if (!ns) { w.err = true; break; }
                memcpy(ns, st, sizeof(vnode_t *) * (usize)sp);
                kfree(st);
                st = ns;
                cap *= 2;
            }
            st[sp++] = c;
        }
    }
    kfree(st);
    kfree(vp);
    if (w.err) { kfree(w.b); return NULL; }
    w.b[12] = (u8)count; w.b[13] = (u8)(count >> 8); w.b[14] = (u8)(count >> 16); w.b[15] = (u8)(count >> 24);
    w32(&w, vfs_crc32(w.b, w.n));
    if (w.err) { kfree(w.b); return NULL; }
    *len = w.n;
    return w.b;
}

bool vmeta_check(const u8 *d, u64 n, u64 *next_ino) {
    if (!d || n < VMETA_HDR + 4 || memcmp(d, VMETA_MAGIC, 8) || r32(d + 8) != 1) return false;
    if (vfs_crc32(d, n - 4) != r32(d + n - 4)) return false;
    if (next_ino) *next_ino = r64(d + 16);
    return true;
}

int vmeta_apply(const u8 *d, u64 n, vmeta_lookup_fn lookup, void *ctx) {
    u64 next = 0;
    if (!vmeta_check(d, n, &next)) return -1;
    vfs_ino_reserve(next ? next - 1 : 0);
    u32 count = r32(d + 12);
    u64 o = VMETA_HDR, end = n - 4;
    char path[1024];
    int ok = 0;
    for (u32 i = 0; i < count; i++) {
        if (o + 2 > end) break;
        u16 pl = r16(d + o); o += 2;
        if (pl >= sizeof path || o + pl + 1 + 16 + 8 + 32 + 2 > end) break;
        memcpy(path, d + o, pl); path[pl] = 0; o += pl;
        u8 isdir = d[o++];
        u32 mode = r32(d + o), uid = r32(d + o + 4), gid = r32(d + o + 8), flags = r32(d + o + 12); o += 16;
        u64 ino = r64(d + o); o += 8;
        i64 t[4];
        for (int k = 0; k < 4; k++) { t[k] = (i64)r64(d + o); o += 8; }
        u16 nx = r16(d + o); o += 2;
        vnode_t *v = lookup(ctx, path);
        bool use = v && (bool)v->dir == (bool)isdir;
        if (use) {
            vfs_xattr_free(v);
            v->mode = mode; v->uid = uid; v->gid = gid; v->flags = flags;
            if (ino) { v->ino = ino; vfs_ino_reserve(ino); }
        }
        bool bad = false;
        for (u16 k = 0; k < nx; k++) {
            if (o + 1 > end) { bad = true; break; }
            u8 nl = d[o++];
            if (nl == 0 || nl > VFS_XATTR_NAME_MAX || o + nl + 4 > end) { bad = true; break; }
            char nm[VFS_XATTR_NAME_MAX + 1];
            memcpy(nm, d + o, nl); nm[nl] = 0; o += nl;
            u32 vl = r32(d + o); o += 4;
            if (o + vl > end) { bad = true; break; }
            if (use) {
                vxattr_t *x = kcalloc(1, sizeof *x);
                u8 *val = kmalloc(vl ? vl : 1);
                if (x && val) {
                    strcpy(x->name, nm);
                    memcpy(val, d + o, vl);
                    x->val = val; x->len = vl;
                    link_sorted(v, x);
                    v->xattr_bytes += vl;
                } else { kfree(x); kfree(val); }
            }
            o += vl;
        }
        if (bad) break;
        if (use) {
            v->atime_ns = t[0]; v->mtime_ns = t[1]; v->ctime_ns = t[2]; v->btime_ns = t[3];
            v->meta = true;
            ok++;
        }
    }
    return ok;
}
