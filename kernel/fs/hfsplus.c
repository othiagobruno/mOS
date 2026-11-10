/*
 * mOS — driver HFS+/HFSX (TN1150): leitura aqui, escrita em hfsplus_wr.c (só em discos graváveis).
 * Suporta: catálogo e extents overflow, wrapper HFS, hard links de arquivo, symlinks e
 * arquivos com compressão transparente (decmpfs: zlib, LZVN, LZFSE).
 */
#include "hfsplus_int.h"

/* ------------------------------------------------------------ forks */
bool hfs_fork_read(hfs_t *h, const fork_t *f, u64 off, void *buf, u64 len) {
    u8 *out = buf;
    u64 pos = 0;
    for (u32 i = 0; i < f->n && len; i++) {
        u64 bytes = (u64)f->ext[i].count * h->bs;
        if (off < pos + bytes) {
            u64 in = off - pos, n = MIN(len, bytes - in);
            if (!blk_read(h->dev, (u64)f->ext[i].start * h->bs + in, out, n)) return false;
            out += n; off += n; len -= n;
        }
        pos += bytes;
    }
    if (len) memset(out, 0, len); /* além do último extent: zeros */
    return true;
}

static bool bt_search(hfs_t *h, int tree, const void *target, u32 *leaf, int *rec);

typedef struct { u8 fork_type; u32 file_id, start; } extkey_t;

static int ext_cmp(const u8 *k, const void *t) {
    const extkey_t *e = t;
    u32 id = be32(k + 4), st = be32(k + 8);
    u8 ft = k[2];
    if (id != e->file_id) return id < e->file_id ? -1 : 1;
    if (ft != e->fork_type) return ft < e->fork_type ? -1 : 1;
    if (st != e->start) return st < e->start ? -1 : 1;
    return 0;
}

static void ecache_save(hfs_t *h, u32 file_id, u8 fork_type, const fork_t *f) {
    if (h->wr) return; /* volume gravável: extents mudam; não guardar cópia velha */
    int slot = 0;
    for (int i = 0; i < 8; i++) if (!h->ecache[i].used) { slot = i; break; }
    if (h->ecache[slot].used) kfree(h->ecache[slot].f.ext);
    ext_t *c = f->n ? kmalloc(sizeof(ext_t) * f->n) : NULL;
    if (f->n && !c) return;
    if (c) memcpy(c, f->ext, sizeof(ext_t) * f->n);
    h->ecache[slot].used = true;
    h->ecache[slot].id = file_id;
    h->ecache[slot].ft = fork_type;
    h->ecache[slot].f.size = f->size;
    h->ecache[slot].f.n = f->n;
    h->ecache[slot].f.ext = c;
}

bool hfs_fork_load(hfs_t *h, const u8 *fd, u32 file_id, u8 fork_type, fork_t *f) {
    if (!h->wr) {
        for (int i = 0; i < 8; i++) if (h->ecache[i].used && h->ecache[i].id == file_id && h->ecache[i].ft == fork_type) {
            f->size = h->ecache[i].f.size;
            f->n = h->ecache[i].f.n;
            f->ext = f->n ? kmalloc(sizeof(ext_t) * f->n) : NULL;
            if (f->n && !f->ext) return false;
            if (f->n) memcpy(f->ext, h->ecache[i].f.ext, sizeof(ext_t) * f->n);
            return true;
        }
    }
    f->size = be64(fd);
    u32 total = be32(fd + 12), have = 0;
    f->n = 0;
    f->ext = kmalloc(sizeof(ext_t) * 8);
    if (!f->ext) return false;
    for (int i = 0; i < 8; i++) {
        u32 s = be32(fd + 16 + i * 8), c = be32(fd + 20 + i * 8);
        if (!c) break;
        f->ext[f->n++] = (ext_t){s, c};
        have += c;
    }
    /* extents além dos 8 primeiros ficam na árvore de extents overflow */
    u32 cap = 8;
    while (have < total && h->ext_root && file_id != 3) {
        extkey_t key = {fork_type, file_id, have};
        u32 leaf;
        int rec;
        if (!bt_search(h, 1, &key, &leaf, &rec) || rec < 0) return false;
        const u8 *n = h->node;
        u32 off = be16(n + h->ext_ns - 2 * (rec + 1));
        if (ext_cmp(n + off, &key) != 0) return false;
        const u8 *d = n + off + 2 + be16(n + off);
        u32 added = 0;
        for (int i = 0; i < 8; i++) {
            u32 s = be32(d + i * 8), c = be32(d + 4 + i * 8);
            if (!c) break;
            if (f->n == cap) {
                ext_t *p = krealloc(f->ext, sizeof(ext_t) * cap * 2);
                if (!p) return false;
                f->ext = p;
                cap *= 2;
            }
            f->ext[f->n++] = (ext_t){s, c};
            have += c;
            added += c;
        }
        if (!added) return false;
    }
    ecache_save(h, file_id, fork_type, f);
    return true;
}

/* ------------------------------------------------------------ B-trees
 * tree: 0 catálogo, 1 extents, 2 atributos. O nó lido fica em h->node. */
typedef int (*keycmp_t)(const u8 *key, const void *target);

static bool read_node(hfs_t *h, int tree, u32 idx) {
    const fork_t *f = tree == 0 ? &h->cat : tree == 1 ? &h->ext : &h->attr;
    u32 ns = tree == 0 ? h->cat_ns : tree == 1 ? h->ext_ns : h->attr_ns;
    if ((u64)(idx + 1) * ns > f->size || !h->node) return false;
    for (int i = 0; i < 8; i++) {
        if (h->ncache[i].data && h->ncache[i].tree == tree && h->ncache[i].idx == idx && h->ncache[i].len == ns) {
            memcpy(h->node, h->ncache[i].data, ns);
            h->ncache[i].stamp = ++h->nclock;
            return true;
        }
    }
    if (!hfs_fork_read(h, f, (u64)idx * ns, h->node, ns)) return false;
    int slot = 0;
    for (int i = 1; i < 8; i++)
        if (!h->ncache[i].data || h->ncache[i].stamp < h->ncache[slot].stamp) slot = i;
    if (!h->ncache[slot].data || h->ncache[slot].len != ns) {
        kfree(h->ncache[slot].data);
        h->ncache[slot].data = kmalloc(ns);
        h->ncache[slot].len = h->ncache[slot].data ? ns : 0;
    }
    if (h->ncache[slot].data) {
        memcpy(h->ncache[slot].data, h->node, ns);
        h->ncache[slot].tree = tree;
        h->ncache[slot].idx = idx;
        h->ncache[slot].stamp = ++h->nclock;
    }
    return true;
}

static int cat_cmp(const u8 *k, const void *t);
static int attr_cmp(const u8 *k, const void *t);

/* desce até a folha; *rec = último registro com chave <= alvo (-1 se o alvo é menor que todos) */
static bool bt_search(hfs_t *h, int tree, const void *target, u32 *leaf, int *rec) {
    u32 ns = tree == 0 ? h->cat_ns : tree == 1 ? h->ext_ns : h->attr_ns;
    u32 node = tree == 0 ? h->cat_root : tree == 1 ? h->ext_root : h->attr_root;
    keycmp_t cmp = tree == 0 ? cat_cmp : tree == 1 ? ext_cmp : attr_cmp;
    for (int depth = 0; depth < 16; depth++) {
        if (!node || !read_node(h, tree, node)) return false;
        const u8 *n = h->node;
        i8 kind = (i8)n[8];
        u16 nrec = be16(n + 10);
        int best = -1;
        for (int i = 0; i < nrec; i++) {
            u32 off = be16(n + ns - 2 * (i + 1));
            if (off + 2 > ns) return false;
            if (cmp(n + off, target) <= 0) best = i;
            else break;
        }
        if (kind == -1) { *leaf = node; *rec = best; return true; } /* folha */
        if (kind != 0 || !nrec) return false;
        if (best < 0) best = 0;
        u32 off = be16(n + ns - 2 * (best + 1));
        node = be32(n + off + 2 + be16(n + off));
    }
    return false;
}

/* catálogo: alvo = (pai, nome vazio) — só precisamos achar o começo de um diretório */
static int cat_cmp(const u8 *k, const void *t) {
    u32 p = be32(k + 2), want = *(const u32 *)t;
    if (p != want) return p < want ? -1 : 1;
    return be16(k + 6) == 0 ? 0 : 1;
}

typedef struct { u32 file_id; const u16 *name; u32 nlen; } attrkey_t;

static int attr_cmp(const u8 *k, const void *t) {
    const attrkey_t *a = t;
    u32 id = be32(k + 4);
    if (id != a->file_id) return id < a->file_id ? -1 : 1;
    u32 n = be16(k + 12);
    for (u32 i = 0; i < n && i < a->nlen; i++) {
        u16 c = be16(k + 14 + i * 2);
        if (c != a->name[i]) return c < a->name[i] ? -1 : 1;
    }
    if (n != a->nlen) return n < a->nlen ? -1 : 1;
    u32 sb = be32(k + 8);
    return sb ? 1 : 0;
}

/* lê o atributo estendido "name" do arquivo; retorna buffer (kfree) */
static u8 *get_xattr(hfs_t *h, u32 file_id, const char *name, u64 *len) {
    if (!h->attr_root) return NULL;
    u16 wname[64];
    u32 nl = 0;
    while (name[nl] && nl < 64) { wname[nl] = (u8)name[nl]; nl++; }
    attrkey_t key = {file_id, wname, nl};
    u32 leaf;
    int rec;
    if (!bt_search(h, 2, &key, &leaf, &rec) || rec < 0) return NULL;
    const u8 *n = h->node;
    u32 off = be16(n + h->attr_ns - 2 * (rec + 1));
    if (attr_cmp(n + off, &key) != 0) return NULL;
    const u8 *d = n + off + 2 + be16(n + off);
    u32 type = be32(d);
    if (type == 0x10) {
        u32 sz = be32(d + 12);
        if ((u64)(d - n) + 16 + sz > h->attr_ns) return NULL;
        u8 *b = kmalloc(sz ? sz : 1);
        if (b) { memcpy(b, d + 16, sz); *len = sz; }
        return b;
    }
    if (type == 0x20) {
        fork_t f;
        u8 fd[80];
        memcpy(fd, d + 8, 80);
        if (!hfs_fork_load(h, fd, 0, 0, &f) || f.size > (16u << 20)) { kfree(f.ext); return NULL; }
        u8 *b = kmalloc(f.size ? f.size : 1);
        if (b && !hfs_fork_read(h, &f, 0, b, f.size)) { kfree(b); b = NULL; }
        if (b) *len = f.size;
        kfree(f.ext);
        return b;
    }
    return NULL;
}

/* percorre os registros de catálogo do diretório "parent" */
typedef bool (*cat_cb)(hfs_t *h, const u8 *key, const u8 *data, u32 dlen, void *ctx);

static bool cat_iterate(hfs_t *h, u32 parent, cat_cb cb, void *ctx) {
    u32 leaf;
    int rec;
    if (!bt_search(h, 0, &parent, &leaf, &rec)) return false;
    if (rec < 0) rec = 0;
    u32 ns = h->cat_ns;
    for (int guard = 0; leaf && guard < 1000000; guard++) {
        if (!read_node(h, 0, leaf)) return false;
        u8 *n = h->node;
        u16 nrec = be16(n + 10);
        u32 next = be32(n);
        for (int i = rec; i < nrec; i++) {
            u32 off = be16(n + ns - 2 * (i + 1));
            const u8 *k = n + off;
            u32 p = be32(k + 2);
            if (p < parent) continue;
            if (p > parent) return true;
            u32 klen = be16(k);
            u32 doff = off + 2 + klen;
            u32 dend = i + 1 < nrec ? be16(n + ns - 2 * (i + 2)) : ns;
            if (doff >= ns || dend > ns || dend < doff) return false;
            /* o callback pode ler outros nós: trabalha sobre uma cópia */
            u8 kc[520], dc[520];
            u32 len = MIN(dend - doff, (u32)sizeof(dc));
            memcpy(kc, k, MIN(2 + klen, (u32)sizeof(kc)));
            memcpy(dc, n + doff, len);
            if (!cb(h, kc, dc, len, ctx)) return true;
            if (!read_node(h, 0, leaf)) return false;
        }
        leaf = next;
        rec = 0;
    }
    return true;
}

/* ------------------------------------------------------------ readdir/read */
static bool add_entry(hfs_t *h, const u8 *k, const u8 *d, u32 dlen, void *ctx) {
    dnode_t *dir = ctx;
    i16 type = (i16)be16(d);
    if (type != 1 && type != 2) return true;
    u32 nlen = MIN(be16(k + 6), 255u);
    char name[768];
    utf16be_to_utf8(k + 8, nlen, name, sizeof(name));
    if (!name[0]) return true;
    /* diretórios internos (nomes com NUL/CR) e metadados do journal ficam de fora */
    if (strstr(name, "\xE2\x90\x80") || strchr(name, '\r') || !strcmp(name, ".journal") || !strcmp(name, ".journal_info_block")) {
        if (type == 1 && dir->ino == 2 && strstr(name, "HFS+ Private Data")) h->priv_dir = be32(d + 8);
        return true;
    }
    if (type == 1) {
        if (dlen < 88) return true;
        dnode_t *c = dfs_new_child(dir, name, DN_DIR, 0);
        if (!c) return false;
        c->ino = be32(d + 8);
        c->mtime = (i64)be32(d + 16) - HFS_EPOCH_DELTA;
        c->mode = be16(d + 42) & 07777;
        return true;
    }
    if (dlen < 248) return true;
    u32 ftype = be32(d + 48), creator = be32(d + 52);
    hfs_file_t *f = kcalloc(1, sizeof(*f));
    if (!f) return false;
    memcpy(f->data, d + 88, 80);
    memcpy(f->rsrc, d + 168, 80);
    f->file_id = be32(d + 8);
    f->compressed = (d[41] & 0x20) != 0;
    bool link = ftype == FOURCC('s', 'l', 'n', 'k') && creator == FOURCC('r', 'h', 'a', 'p');
    if (ftype == FOURCC('h', 'l', 'n', 'k') && creator == FOURCC('h', 'f', 's', '+')) f->link_ino = be32(d + 44);
    u64 size = be64(d + 88);
    dnode_t *c = dfs_new_child(dir, name, link ? DN_LINK : DN_FILE, size);
    if (!c) { kfree(f); return false; }
    c->ino = f->file_id;
    c->mtime = (i64)be32(d + 16) - HFS_EPOCH_DELTA;
    c->mode = be16(d + 42) & 07777;
    c->priv = f;
    return true;
}

typedef struct { char want[32]; u8 rec[248]; bool found; } find_ctx_t;

static bool find_inode(hfs_t *h, const u8 *k, const u8 *d, u32 dlen, void *ctx) {
    find_ctx_t *fc = ctx;
    if ((i16)be16(d) != 2 || dlen < 248) return true;
    char name[64];
    utf16be_to_utf8(k + 8, MIN(be16(k + 6), 20u), name, sizeof(name));
    if (strcmp(name, fc->want)) return true;
    memcpy(fc->rec, d, 248);
    fc->found = true;
    return false;
}

/* resolve hard links e carrega extents/tamanho real (decmpfs) */
static bool prepare(hfs_t *h, dnode_t *v, hfs_file_t *f) {
    if (f->loaded) return true;
    if (f->link_ino && h->priv_dir) {
        find_ctx_t fc = {0};
        snprintf(fc.want, sizeof(fc.want), "iNode%u", f->link_ino);
        cat_iterate(h, h->priv_dir, find_inode, &fc);
        if (fc.found) {
            memcpy(f->data, fc.rec + 88, 80);
            memcpy(f->rsrc, fc.rec + 168, 80);
            f->file_id = be32(fc.rec + 8);
            f->compressed = (fc.rec[41] & 0x20) != 0;
            v->size = be64(fc.rec + 88);
        }
        f->link_ino = 0;
    }
    if (!f->compressed && !hfs_fork_load(h, f->data, f->file_id, 0, &f->dfork)) return false;
    f->loaded = true;
    return true;
}

typedef struct { hfs_t *h; fork_t f; } rsrc_ctx_t;
static bool rsrc_reader(void *ctx, u64 off, void *buf, u64 len) {
    rsrc_ctx_t *r = ctx;
    return hfs_fork_read(r->h, &r->f, off, buf, len);
}

static bool load_compressed(hfs_t *h, hfs_file_t *f) {
    if (h->cache && h->cache_ino == f->file_id) return true;
    u64 xl = 0;
    u8 *x = get_xattr(h, f->file_id, "com.apple.decmpfs", &xl);
    if (!x) return false;
    rsrc_ctx_t rc = {h, {0}};
    bool ok = hfs_fork_load(h, f->rsrc, f->file_id, 0xFF, &rc.f);
    u64 out = 0;
    u8 *buf = ok ? decmpfs_decode(x, xl, rsrc_reader, &rc, rc.f.size, &out) : NULL;
    kfree(rc.f.ext);
    kfree(x);
    if (!buf) return false;
    kfree(h->cache);
    h->cache = buf;
    h->cache_len = out;
    h->cache_ino = f->file_id;
    return true;
}

static bool hfs_readdir(dmount_t *m, dnode_t *dir) {
    hfs_t *h = m->priv;
    if (!cat_iterate(h, (u32)dir->ino, add_entry, dir)) return false;
    /* arquivos comprimidos: o tamanho real está no cabeçalho decmpfs */
    for (dnode_t *c = dir->child; c; c = c->next) {
        hfs_file_t *f = c->priv;
        if (!f || c->type == DN_DIR) continue;
        if (f->link_ino) { prepare(h, c, f); }
        if (f->compressed) {
            u64 xl = 0;
            u8 *x = get_xattr(h, f->file_id, "com.apple.decmpfs", &xl);
            u64 sz;
            if (x && decmpfs_header(x, xl, NULL, &sz)) c->size = sz;
            kfree(x);
        }
    }
    return true;
}

static i64 hfs_read(dmount_t *m, dnode_t *v, u64 off, void *buf, u64 len) {
    hfs_t *h = m->priv;
    hfs_file_t *f = v->priv;
    if (!f || !prepare(h, v, f)) return -1;
    if (f->compressed) {
        if (!load_compressed(h, f)) return -1;
        if (off >= h->cache_len) return 0;
        len = MIN(len, h->cache_len - off);
        memcpy(buf, h->cache + off, len);
        return (i64)len;
    }
    if (off >= f->dfork.size) return 0;
    len = MIN(len, f->dfork.size - off);
    return hfs_fork_read(h, &f->dfork, off, buf, len) ? (i64)len : -1;
}

static void hfs_unmount(dmount_t *m) {
    hfs_t *h = m->priv;
    if (h->wr) hfs_wr_close(h);          /* grava o pendente e marca o volume como desmontado direito */
    kfree(h->cat.ext); kfree(h->ext.ext); kfree(h->attr.ext);
    kfree(h->node); kfree(h->cache);
    for (int i = 0; i < 8; i++) { kfree(h->ncache[i].data); kfree(h->ecache[i].f.ext); }
    if (h->own_dev) blk_release(h->dev); /* o resto pertence à imagem (dfs.c) */
    kfree(h);
}

static void hfs_free_node(dnode_t *n) {
    if (n->type == DN_DIR) return;
    hfs_file_t *f = n->priv;
    kfree(f->dfork.ext);
}

static void hfs_trim(dmount_t *m) {
    hfs_t *h = m->priv;
    kfree(h->cache);
    h->cache = NULL;
    h->cache_len = 0;
    h->cache_ino = 0;
    for (int i = 0; i < 8; i++) {
        kfree(h->ncache[i].data);
        h->ncache[i].data = NULL;
        h->ncache[i].len = 0;
        if (!h->wr) { kfree(h->ecache[i].f.ext); h->ecache[i].f.ext = NULL; h->ecache[i].used = false; }
    }
}

/* Consentimento do usuário (por volume): liga a escrita num volume que foi montado só leitura porque o disco não estava liberado.
 * Com journal pendente NÃO liga (o replay mudaria as estruturas já carregadas): o volume continua só leitura. */
static bool hfs_enable_write(dmount_t *m) {
    hfs_t *h = m->priv;
    if (!h) return false;
    if (h->wr) return true;
    u8 vh[512];
    if (h->own_dev || !blk_writable(h->dev) || !blk_read(h->dev, 1024, vh, sizeof vh)) return false;   /* wrapper HFS fica só leitura */
    if (be32(vh + 4) & 0x2000) {
        bool jro = false;
        if (!hfs_journal_prepare(h->dev, vh, false, &jro) || jro) return false;
    }
    m->writable = hfs_wr_open(h, vh);
    return m->writable;
}

static const dfs_ops_t HFS_OPS = {"HFS+", hfs_readdir, hfs_read, hfs_unmount, hfs_free_node, hfs_trim,
                                  hfs_wr_write, hfs_wr_mkdir, hfs_wr_remove, hfs_wr_rename, hfs_wr_sync, hfs_enable_write};

static bool tree_header(hfs_t *h, int tree, fork_t *f, u32 *ns, u32 *root) {
    if (!f->size) { *root = 0; return true; }
    u8 hdr[512];
    if (!hfs_fork_read(h, f, 0, hdr, sizeof(hdr))) return false;
    *root = be32(hdr + 14 + 2);
    *ns = be16(hdr + 14 + 18);
    return *ns >= 512 && *ns <= 32768 && (*ns & (*ns - 1)) == 0;
}

typedef struct { char name[256]; } volname_ctx_t;

int hfsplus_probe(blkdev_t *dev, const char *source, const char *fmt) {
    u8 vh[512];
    if (dev->size < 2048 || !blk_read(dev, 1024, vh, 512)) return 0;
    u16 sig = be16(vh);
    blkdev_t *vol = dev;
    if (sig == 0x4244 && be16(vh + 0x7C) == 0x482B) { /* wrapper HFS com HFS+ embutido */
        u32 alblk = be32(vh + 0x14);
        u64 start = (u64)be16(vh + 0x1C) * 512 + (u64)be16(vh + 0x7E) * alblk;
        u64 len = (u64)be16(vh + 0x80) * alblk;
        vol = blk_slice(dev, start, len);
        if (!vol || !blk_read(vol, 1024, vh, 512)) { blk_release(vol); return 0; }
        sig = be16(vh);
    }
    if (sig != 0x482B && sig != 0x4858) { if (vol != dev) blk_release(vol); return 0; }
    /* journal: com transações pendentes, o replay vem antes de ler qualquer estrutura (pode mudar o
     * próprio cabeçalho). Sem poder repetir (disco só leitura, journal em outro disco), monta só leitura.
     * Volumes dentro de um wrapper HFS ficam só leitura. */
    bool ro = vol != dev || !blk_writable(vol);
    if (be32(vh + 4) & 0x2000) {
        bool jro = false;
        if (!hfs_journal_prepare(vol, vh, !ro, &jro) || !blk_read(vol, 1024, vh, 512)) {
            if (vol != dev) blk_release(vol);
            return 0;
        }
        if (jro) ro = true;
    }
    hfs_t *h = kcalloc(1, sizeof(*h));
    h->dev = vol;
    h->own_dev = vol != dev;
    h->bs = be32(vh + 40);
    bool ok = h->bs >= 512 && (h->bs & (h->bs - 1)) == 0;
    ok = ok && hfs_fork_load(h, vh + 192, 3, 0, &h->ext);
    h->node = kmalloc(32768);
    ok = ok && h->node && tree_header(h, 1, &h->ext, &h->ext_ns, &h->ext_root);
    ok = ok && hfs_fork_load(h, vh + 272, 4, 0, &h->cat);
    ok = ok && tree_header(h, 0, &h->cat, &h->cat_ns, &h->cat_root) && h->cat_root;
    ok = ok && hfs_fork_load(h, vh + 352, 8, 0, &h->attr);
    ok = ok && tree_header(h, 2, &h->attr, &h->attr_ns, &h->attr_root);
    if (!ok) {
        kprintf("hfs+: volume inválido\n");
        kfree(h->ext.ext); kfree(h->cat.ext); kfree(h->attr.ext); kfree(h->node);
        for (int i = 0; i < 8; i++) { kfree(h->ncache[i].data); kfree(h->ecache[i].f.ext); }
        kfree(h);
        if (vol != dev) blk_release(vol);
        return 0;
    }
    /* nome do volume: registro "thread" da pasta raiz (id 2) */
    char name[768] = "Sem Título";
    u32 root = 2, leaf;
    int rec;
    if (bt_search(h, 0, &root, &leaf, &rec) && rec >= 0) {
        const u8 *n = h->node;
        u32 off = be16(n + h->cat_ns - 2 * (rec + 1));
        const u8 *d = n + off + 2 + be16(n + off);
        if (cat_cmp(n + off, &root) == 0 && be16(d) == 3) utf16be_to_utf8(d + 10, MIN(be16(d + 8), 255u), name, sizeof(name));
    }
    /* o catálogo da raiz precisa ser lido antes para achar a pasta de hard links */
    char fs[40];
    snprintf(fs, sizeof(fs), "%s", fmt);
    dmount_t *m = dfs_attach(&HFS_OPS, vol, name, source, fs, h, 2);
    if (!m) { hfs_unmount(&(dmount_t){.priv = h}); return 0; }
    dfs_first_child(m->root);
    if (!ro) m->writable = hfs_wr_open(h, vh);
    (void)sig;
    return 1;
}
