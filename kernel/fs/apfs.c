/*
 * mOS — driver APFS somente leitura (Apple File System Reference).
 * Contêiner -> checkpoint mais recente -> object map -> volumes -> árvore do sistema de
 * arquivos. Suporta diretórios com nomes "hashed" e normais, extents esparsos, hard links,
 * symlinks, xattrs embutidos/stream e compressão decmpfs. Volumes criptografados são ignorados.
 */
#include "fs.h"

#define NX_MAGIC   0x4253584Eu /* "NXSB" */
#define APSB_MAGIC 0x42535041u /* "APSB" */
#define BT_ROOT  1
#define BT_LEAF  2
#define BT_FIXED 4
enum { T_INODE = 3, T_XATTR = 4, T_EXTENT = 8, T_DREC = 9 };
#define OID_MASK 0x0FFFFFFFFFFFFFFFull

#define NCACHE 24

typedef struct {
    blkdev_t *dev;
    u32 bs;
    u64 xid;
    u64 omap_root;          /* raiz da omap do contêiner (física) */
    struct { u64 addr; u8 *buf; u64 stamp; } cache[NCACHE];
    u64 clock;
    int refs;
} nx_t;

typedef struct {
    nx_t *nx;
    u64 omap_root;          /* omap do volume */
    u64 tree_root;          /* árvore do fs (oid virtual) */
    bool hashed;
    u64 cache_ino;          /* último arquivo decmpfs */
    u8 *cache;
    u64 cache_len;
} vol_t;

typedef struct { u64 logical, len, phys; } fext_t;

typedef struct {
    u64 private_id;
    bool compressed, loaded;
    fext_t *ext;
    u32 n;
} afile_t;

/* ------------------------------------------------------------ blocos */
static u64 fletcher64(const u8 *b, u32 len) {
    u64 s1 = 0, s2 = 0;
    for (u32 i = 8; i < len; i += 4) {
        s1 = (s1 + le32(b + i)) % 0xFFFFFFFFull;
        s2 = (s2 + s1) % 0xFFFFFFFFull;
    }
    u64 c1 = 0xFFFFFFFFull - (s1 + s2) % 0xFFFFFFFFull;
    u64 c2 = 0xFFFFFFFFull - (s1 + c1) % 0xFFFFFFFFull;
    return c2 << 32 | c1;
}

static const u8 *block(nx_t *nx, u64 addr) {
    for (int i = 0; i < NCACHE; i++)
        if (nx->cache[i].buf && nx->cache[i].addr == addr) { nx->cache[i].stamp = ++nx->clock; return nx->cache[i].buf; }
    int v = 0;
    for (int i = 1; i < NCACHE; i++) if (nx->cache[i].stamp < nx->cache[v].stamp) v = i;
    if (!nx->cache[v].buf && !(nx->cache[v].buf = kmalloc(nx->bs))) return NULL;
    nx->cache[v].addr = ~0ull;
    if (!blk_read(nx->dev, addr * nx->bs, nx->cache[v].buf, nx->bs)) return NULL;
    nx->cache[v].addr = addr;
    nx->cache[v].stamp = ++nx->clock;
    return nx->cache[v].buf;
}

/* ------------------------------------------------------------ nós de B-tree */
typedef struct {
    const u8 *n;
    u16 flags, level;
    u32 nkeys;
    const u8 *toc, *keys, *vend;
} node_t;

static bool node_parse(nx_t *nx, const u8 *b, node_t *o) {
    if (!b) return false;
    u16 type = le32(b + 24) & 0xFFFF;
    if (type != 2 && type != 3) return false; /* btree / btree_node */
    o->n = b;
    o->flags = le16(b + 32);
    o->level = le16(b + 34);
    o->nkeys = le32(b + 36);
    u16 toff = le16(b + 40), tlen = le16(b + 42);
    o->toc = b + 56 + toff;
    o->keys = o->toc + tlen;
    o->vend = b + nx->bs - ((o->flags & BT_ROOT) ? 40 : 0);
    return 56u + toff + tlen <= nx->bs && o->nkeys < nx->bs;
}

static void entry(const node_t *nd, u32 i, const u8 **k, u32 *kl, const u8 **v, u32 *vl, u32 fk, u32 fv) {
    if (nd->flags & BT_FIXED) {
        u16 ko = le16(nd->toc + i * 4), vo = le16(nd->toc + i * 4 + 2);
        *k = nd->keys + ko; *kl = fk;
        *v = vo == 0xFFFF ? NULL : nd->vend - vo;
        *vl = nd->level ? 8 : fv;
    } else {
        const u8 *t = nd->toc + i * 8;
        *k = nd->keys + le16(t); *kl = le16(t + 2);
        u16 vo = le16(t + 4);
        *v = vo == 0xFFFF ? NULL : nd->vend - vo;
        *vl = le16(t + 6);
    }
}

/* omap: (oid, xid) -> endereço físico */
static u64 omap_lookup(nx_t *nx, u64 root, u64 oid, u64 xid) {
    u64 addr = root;
    for (int depth = 0; depth < 12; depth++) {
        node_t nd;
        if (!node_parse(nx, block(nx, addr), &nd)) return 0;
        i64 best = -1;
        for (u32 i = 0; i < nd.nkeys; i++) {
            const u8 *k, *v;
            u32 kl, vl;
            entry(&nd, i, &k, &kl, &v, &vl, 16, 16);
            u64 ko = le64(k), kx = le64(k + 8);
            if (ko < oid || (ko == oid && kx <= xid)) best = i;
            else break;
        }
        if (best < 0) return 0;
        const u8 *k, *v;
        u32 kl, vl;
        entry(&nd, (u32)best, &k, &kl, &v, &vl, 16, 16);
        if (!v) return 0;
        if (nd.level == 0) {
            if (le64(k) != oid) return 0;
            if (le32(v) & 1) return 0; /* OMAP_VAL_DELETED */
            return le64(v + 8);
        }
        addr = le64(v);
    }
    return 0;
}

/* ------------------------------------------------------------ árvore do fs */
typedef int (*fs_cb)(vol_t *vl, const u8 *k, u32 kl, const u8 *v, u32 vlen, void *ctx); /* 0 continua, 1 para */

static int cmp_hdr(u64 hdr, u64 id, u32 type) {
    u64 kid = hdr & OID_MASK;
    u32 kt = (u32)(hdr >> 60);
    if (kid != id) return kid < id ? -1 : 1;
    if (kt != type) return kt < type ? -1 : 1;
    return 0;
}

/* visita, em ordem, todos os registros (id, type). Retorna 1 se encontrou chave maior (parar). */
static int walk(vol_t *vl, u64 oid, u64 id, u32 type, fs_cb cb, void *ctx, int depth) {
    nx_t *nx = vl->nx;
    if (depth > 12) return -1;
    u64 paddr = omap_lookup(nx, vl->omap_root, oid, nx->xid);
    node_t nd;
    if (!paddr || !node_parse(nx, block(nx, paddr), &nd)) return -1;
    /* copia o nó: chamadas recursivas reutilizam o cache */
    u8 *copy = kmalloc(nx->bs);
    if (!copy) return -1;
    memcpy(copy, nd.n, nx->bs);
    node_parse(nx, copy, &nd);
    int ret = 0;
    if (nd.level == 0) {
        for (u32 i = 0; i < nd.nkeys; i++) {
            const u8 *k, *v;
            u32 kl, vlen;
            entry(&nd, i, &k, &kl, &v, &vlen, 0, 0);
            int c = cmp_hdr(le64(k), id, type);
            if (c < 0) continue;
            if (c > 0) { ret = 1; break; }
            if (v && cb(vl, k, kl, v, vlen, ctx)) { ret = 1; break; }
        }
    } else {
        u32 start = 0;
        for (u32 i = 0; i < nd.nkeys; i++) {
            const u8 *k, *v;
            u32 kl, vlen;
            entry(&nd, i, &k, &kl, &v, &vlen, 0, 0);
            if (cmp_hdr(le64(k), id, type) < 0) start = i;
            else break;
        }
        for (u32 i = start; i < nd.nkeys; i++) {
            const u8 *k, *v;
            u32 kl, vlen;
            entry(&nd, i, &k, &kl, &v, &vlen, 0, 0);
            if (i > start && cmp_hdr(le64(k), id, type) > 0) { ret = 1; break; }
            if (!v) continue;
            int r = walk(vl, le64(v), id, type, cb, ctx, depth + 1);
            if (r) { ret = r; break; }
        }
    }
    kfree(copy);
    return ret;
}

static bool fs_iter(vol_t *vl, u64 id, u32 type, fs_cb cb, void *ctx) {
    return walk(vl, vl->tree_root, id, type, cb, ctx, 0) >= 0;
}

/* ------------------------------------------------------------ inodes e xattrs */
typedef struct { u64 private_id, size, mtime; u32 bsd_flags; u16 mode; bool found; } inode_t;

static int inode_cb(vol_t *vl, const u8 *k, u32 kl, const u8 *v, u32 vlen, void *ctx) {
    inode_t *in = ctx;
    if (vlen < 92) return 1;
    in->private_id = le64(v + 8);
    in->mtime = le64(v + 24);
    in->bsd_flags = le32(v + 68);
    in->mode = le16(v + 80);
    in->size = 0;
    /* campos estendidos: procura o dstream (tipo 8) */
    u16 nx = le16(v + 92);
    const u8 *hdr = v + 96, *data = v + 96 + nx * 4;
    for (u16 i = 0; i < nx && (u32)(data - v) <= vlen; i++) {
        u8 t = hdr[i * 4];
        u16 sz = le16(hdr + i * 4 + 2);
        if (t == 8 && (u32)(data - v) + 8 <= vlen) in->size = le64(data);
        data += (sz + 7) & ~7u;
    }
    in->found = true;
    return 1;
}

static bool get_inode(vol_t *vl, u64 ino, inode_t *in) {
    memset(in, 0, sizeof(*in));
    return fs_iter(vl, ino, T_INODE, inode_cb, in) && in->found;
}

typedef struct { u64 id; fext_t *ext; u32 n, cap; bool err; } ext_ctx_t;

static int ext_cb(vol_t *vl, const u8 *k, u32 kl, const u8 *v, u32 vlen, void *ctx) {
    ext_ctx_t *e = ctx;
    if (kl < 16 || vlen < 16) return 0;
    if (e->n == e->cap) {
        u32 nc = e->cap ? e->cap * 2 : 8;
        fext_t *p = krealloc(e->ext, nc * sizeof(fext_t));
        if (!p) { e->err = true; return 1; }
        e->ext = p;
        e->cap = nc;
    }
    e->ext[e->n++] = (fext_t){le64(k + 8), le64(v) & 0x00FFFFFFFFFFFFFFull, le64(v + 8)};
    return 0;
}

static bool load_extents(vol_t *vl, u64 id, fext_t **ext, u32 *n) {
    ext_ctx_t e = {id, NULL, 0, 0, false};
    if (!fs_iter(vl, id, T_EXTENT, ext_cb, &e) || e.err) { kfree(e.ext); return false; }
    *ext = e.ext;
    *n = e.n;
    return true;
}

static bool ext_read(vol_t *vl, const fext_t *ext, u32 n, u64 off, void *buf, u64 len) {
    u8 *out = buf;
    nx_t *nx = vl->nx;
    while (len) {
        const fext_t *e = NULL;
        for (u32 i = 0; i < n; i++)
            if (off >= ext[i].logical && off < ext[i].logical + ext[i].len) { e = &ext[i]; break; }
        u64 chunk;
        if (!e) { /* buraco: zeros até o próximo extent */
            u64 next = ~0ull;
            for (u32 i = 0; i < n; i++) if (ext[i].logical > off && ext[i].logical < next) next = ext[i].logical;
            chunk = MIN(len, next - off);
            memset(out, 0, chunk);
        } else {
            u64 in = off - e->logical;
            chunk = MIN(len, e->len - in);
            if (!e->phys) memset(out, 0, chunk);
            else if (!blk_read(nx->dev, e->phys * nx->bs + in, out, chunk)) return false;
        }
        out += chunk; off += chunk; len -= chunk;
    }
    return true;
}

typedef struct { const char *name; u32 nlen; u8 *data; u64 len; u64 stream_id, stream_size; bool found; } xattr_ctx_t;

static int xattr_cb(vol_t *vl, const u8 *k, u32 kl, const u8 *v, u32 vlen, void *ctx) {
    xattr_ctx_t *x = ctx;
    u16 nl = le16(k + 8);
    if (kl < 10u + nl || nl != x->nlen + 1 || memcmp(k + 10, x->name, x->nlen)) return 0;
    u16 flags = le16(v), dl = le16(v + 2);
    if (vlen < 4u + dl) return 1;
    if (flags & 2) { /* embutido */
        x->data = kmalloc(dl ? dl : 1);
        if (x->data) { memcpy(x->data, v + 4, dl); x->len = dl; x->found = true; }
    } else if ((flags & 1) && dl >= 16) { /* stream */
        x->stream_id = le64(v + 4);
        x->stream_size = le64(v + 12);
        x->found = true;
    }
    return 1;
}

/* lê um xattr inteiro (até 64 MiB); retorna buffer (kfree) */
static u8 *get_xattr(vol_t *vl, u64 ino, const char *name, u64 *len) {
    xattr_ctx_t x = {name, (u32)strlen(name), NULL, 0, 0, 0, false};
    if (!fs_iter(vl, ino, T_XATTR, xattr_cb, &x) || !x.found) return NULL;
    if (x.data) { *len = x.len; return x.data; }
    if (x.stream_size > (64u << 20)) return NULL;
    fext_t *ext;
    u32 n;
    if (!load_extents(vl, x.stream_id, &ext, &n)) return NULL;
    u8 *b = kmalloc(x.stream_size ? x.stream_size : 1);
    if (b && !ext_read(vl, ext, n, 0, b, x.stream_size)) { kfree(b); b = NULL; }
    kfree(ext);
    if (b) *len = x.stream_size;
    return b;
}

/* ------------------------------------------------------------ readdir/read */
typedef struct { dnode_t *dir; bool err; } dir_ctx_t;

static int drec_cb(vol_t *vl, const u8 *k, u32 kl, const u8 *v, u32 vlen, void *ctx) {
    dir_ctx_t *dc = ctx;
    const u8 *name;
    u32 nl;
    if (vl->hashed) { nl = le32(k + 8) & 0x3FF; name = k + 12; }
    else { nl = le16(k + 8); name = k + 10; }
    if (vlen < 18 || nl == 0 || (u32)(name - k) + nl > kl) return 0;
    char buf[768];
    nl = MIN(nl, (u32)sizeof(buf) - 1);
    memcpy(buf, name, nl);
    buf[nl] = 0;
    utf8_compose(buf);
    u16 dt = le16(v + 16) & 0x0F;
    u8 type = dt == 4 ? DN_DIR : dt == 10 ? DN_LINK : DN_FILE;
    dnode_t *c = dfs_new_child(dc->dir, buf, type, 0);
    if (!c) { dc->err = true; return 1; }
    c->ino = le64(v);
    return 0;
}

static bool apfs_readdir(dmount_t *m, dnode_t *dir) {
    vol_t *vl = m->priv;
    dir_ctx_t dc = {dir, false};
    if (!fs_iter(vl, dir->ino, T_DREC, drec_cb, &dc) || dc.err) return false;
    for (dnode_t *c = dir->child; c; c = c->next) {
        inode_t in;
        if (!get_inode(vl, c->ino, &in)) continue;
        c->mtime = (i64)(in.mtime / 1000000000ull);
        c->mode = in.mode & 07777;
        if (c->type == DN_DIR) continue;
        afile_t *f = kcalloc(1, sizeof(*f));
        if (!f) return false;
        f->private_id = in.private_id;
        f->compressed = (in.bsd_flags & 0x20) != 0;
        c->priv = f;
        c->size = in.size;
        if (f->compressed) {
            u64 xl = 0, sz;
            u8 *x = get_xattr(vl, c->ino, "com.apple.decmpfs", &xl);
            if (x && decmpfs_header(x, xl, NULL, &sz)) c->size = sz;
            kfree(x);
        } else if (c->type == DN_LINK) {
            u64 xl = 0;
            u8 *x = get_xattr(vl, c->ino, "com.apple.fs.symlink", &xl);
            if (x) { c->size = xl && !x[xl - 1] ? xl - 1 : xl; kfree(x); }
        }
    }
    return true;
}

typedef struct { const u8 *p; u64 n; } mem_ctx_t;
static bool mem_reader(void *ctx, u64 off, void *buf, u64 len) {
    mem_ctx_t *m = ctx;
    if (off > m->n || len > m->n - off) return false;
    memcpy(buf, m->p + off, len);
    return true;
}

typedef struct { vol_t *vl; fext_t *ext; u32 n; } rs_ctx_t;
static bool rsrc_read(void *ctx, u64 off, void *buf, u64 len) {
    rs_ctx_t *r = ctx;
    return ext_read(r->vl, r->ext, r->n, off, buf, len);
}

static i64 apfs_read(dmount_t *m, dnode_t *v, u64 off, void *buf, u64 len) {
    vol_t *vl = m->priv;
    afile_t *f = v->priv;
    if (!f) return -1;
    if (v->type == DN_LINK && !f->compressed) {
        u64 xl = 0;
        u8 *x = get_xattr(vl, v->ino, "com.apple.fs.symlink", &xl);
        if (!x) return -1;
        len = off < xl ? MIN(len, xl - off) : 0;
        memcpy(buf, x + off, len);
        kfree(x);
        return (i64)len;
    }
    if (f->compressed) {
        if (!vl->cache || vl->cache_ino != v->ino) {
            u64 xl = 0, rl = 0, out = 0;
            u8 *x = get_xattr(vl, v->ino, "com.apple.decmpfs", &xl);
            if (!x) return -1;
            /* o resource fork é um xattr; para não carregá-lo inteiro, lemos seus extents */
            xattr_ctx_t xr = {"com.apple.ResourceFork", 22, NULL, 0, 0, 0, false};
            rs_ctx_t rc = {vl, NULL, 0};
            u8 *emb = NULL;
            fork_reader_t rd = NULL;
            if (fs_iter(vl, v->ino, T_XATTR, xattr_cb, &xr) && xr.found) {
                if (xr.data) {
                    emb = xr.data; rl = xr.len;
                } else if (load_extents(vl, xr.stream_id, &rc.ext, &rc.n)) {
                    rl = xr.stream_size;
                    rd = rsrc_read;
                }
            }
            mem_ctx_t mc = {emb, rl};
            u8 *res = decmpfs_decode(x, xl, emb ? mem_reader : rd, emb ? (void *)&mc : (void *)&rc, rl, &out);
            kfree(rc.ext);
            kfree(emb);
            kfree(x);
            if (!res) return -1;
            kfree(vl->cache);
            vl->cache = res;
            vl->cache_len = out;
            vl->cache_ino = v->ino;
        }
        if (off >= vl->cache_len) return 0;
        len = MIN(len, vl->cache_len - off);
        memcpy(buf, vl->cache + off, len);
        return (i64)len;
    }
    if (!f->loaded) {
        if (!load_extents(vl, f->private_id, &f->ext, &f->n)) return -1;
        f->loaded = true;
    }
    return ext_read(vl, f->ext, f->n, off, buf, len) ? (i64)len : -1;
}

static void apfs_unmount(dmount_t *m) {
    vol_t *vl = m->priv;
    nx_t *nx = vl->nx;
    kfree(vl->cache);
    kfree(vl);
    if (--nx->refs == 0) { /* o dispositivo pertence à imagem (dfs.c) */
        for (int i = 0; i < NCACHE; i++) kfree(nx->cache[i].buf);
        kfree(nx);
    }
}

static void apfs_free_node(dnode_t *n) {
    if (n->type == DN_DIR) return;
    afile_t *f = n->priv;
    kfree(f->ext);
}

static void apfs_trim(dmount_t *m) {
    vol_t *vl = m->priv;
    kfree(vl->cache);
    vl->cache = NULL;
    vl->cache_len = 0;
    vl->cache_ino = 0;
    nx_t *nx = vl->nx;
    for (int i = 0; i < NCACHE; i++) { kfree(nx->cache[i].buf); nx->cache[i].buf = NULL; nx->cache[i].addr = ~0ull; }
}

static const dfs_ops_t APFS_OPS = {"APFS", apfs_readdir, apfs_read, apfs_unmount, apfs_free_node, apfs_trim};

int apfs_probe(blkdev_t *dev, const char *source, const char *fmt) {
    u8 sb0[4096];
    if (dev->size < 8192 || !blk_read(dev, 0, sb0, 4096) || le32(sb0 + 32) != NX_MAGIC) return 0;
    u32 bs = le32(sb0 + 36);
    if (bs < 4096 || bs > 65536 || (bs & (bs - 1))) return 0;
    nx_t *nx = kcalloc(1, sizeof(*nx));
    if (!nx) return 0;
    nx->dev = dev;
    nx->bs = bs;
    u8 *sb = kmalloc(bs), *tmp = kmalloc(bs);
    if (!sb || !tmp || !blk_read(dev, 0, sb, bs)) goto fail;

    /* checkpoint mais recente válido na área de descritores */
    u32 dblocks = le32(sb + 104);
    u64 dbase = le64(sb + 112);
    if (!(dblocks & 0x80000000u)) {
        for (u32 i = 0; i < dblocks && i < 4096; i++) {
            if (!blk_read(dev, (dbase + i) * bs, tmp, bs)) break;
            if (le32(tmp + 32) != NX_MAGIC || (le32(tmp + 24) & 0xFFFF) != 1) continue;
            if (fletcher64(tmp, bs) != le64(tmp)) continue;
            if (le64(tmp + 16) > le64(sb + 16)) memcpy(sb, tmp, bs);
        }
    }
    nx->xid = le64(sb + 16);
    const u8 *om = block(nx, le64(sb + 160));
    if (!om) goto fail;
    nx->omap_root = le64(om + 48);

    int mounted = 0;
    u32 maxfs = MIN(le32(sb + 180), 100u);
    for (u32 i = 0; i < maxfs; i++) {
        u64 oid = le64(sb + 184 + i * 8);
        if (!oid) continue;
        u64 pa = omap_lookup(nx, nx->omap_root, oid, nx->xid);
        const u8 *vb = pa ? block(nx, pa) : NULL;
        if (!vb || le32(vb + 32) != APSB_MAGIC) continue;
        memcpy(tmp, vb, bs);
        char name[257];
        memcpy(name, tmp + 704, 256);
        name[256] = 0;
        if (!(le64(tmp + 264) & 1)) { kprintf("apfs: volume \"%s\" criptografado, ignorado\n", name); continue; }
        u64 incompat = le64(tmp + 56);
        const u8 *vom = block(nx, le64(tmp + 128));
        if (!vom) continue;
        vol_t *vl = kcalloc(1, sizeof(*vl));
        if (!vl) continue;
        vl->nx = nx;
        vl->omap_root = le64(vom + 48);
        vl->tree_root = le64(tmp + 136);
        vl->hashed = (incompat & (1 | 8)) != 0; /* case-insensitive ou normalization-insensitive */
        nx->refs++;
        dmount_t *m = dfs_attach(&APFS_OPS, dev, name, source, fmt, vl, 2);
        if (!m) { nx->refs--; kfree(vl); continue; }
        mounted++;
    }
    kfree(sb);
    kfree(tmp);
    if (!mounted) { for (int i = 0; i < NCACHE; i++) kfree(nx->cache[i].buf); kfree(nx); }
    return mounted;
fail:
    kfree(sb);
    kfree(tmp);
    for (int i = 0; i < NCACHE; i++) kfree(nx->cache[i].buf);
    kfree(nx);
    return 0;
}
