/*
 * mOS — VFS: árvore de vnodes em memória. Os arquivos do pendrive (\mOS\) viram vnodes em RAM;
 * imagens .dmg/.img encontradas são montadas automaticamente em /Volumes/<nome do volume>.
 * Diretórios de volumes montados só são lidos do disco na primeira vez que alguém os abre.
 */
#include "fs.h"

static dnode_t *s_root, *s_volumes;
static dmount_t *s_mounts;

dnode_t *dfs_root(void) { return s_root; }
dmount_t *dfs_mounts(void) { return s_mounts; }

static dnode_t *new_node(const char *name, u8 type) {
    dnode_t *n = kcalloc(1, sizeof(*n));
    if (!n) return NULL;
    n->name = strdup(name);
    n->type = type;
    n->mode = type == DN_DIR ? 0755 : 0644;
    return n;
}

dnode_t *dfs_new_child(dnode_t *dir, const char *name, u8 type, u64 size) {
    dnode_t *n = new_node(name, type);
    if (!n) return NULL;
    n->size = size;
    n->parent = dir;
    n->mnt = dir->mnt;
    n->hidden = name[0] == '.';
    /* mantém os filhos em ordem alfabética (sem diferenciar maiúsculas), como o Finder */
    dnode_t **pp = &dir->child;
    while (*pp && strcasecmp((*pp)->name, name) < 0) pp = &(*pp)->next;
    n->next = *pp;
    *pp = n;
    return n;
}

static dnode_t *child_named(dnode_t *dir, const char *name, usize len, bool icase) {
    for (dnode_t *c = dfs_first_child(dir); c; c = c->next)
        if (strlen(c->name) == len && (icase ? !strncasecmp(c->name, name, len) : !strncmp(c->name, name, len))) return c;
    return NULL;
}

dnode_t *dfs_lookup(const char *path) {
    dnode_t *n = s_root;
    while (n && *path) {
        while (*path == '/') path++;
        if (!*path) break;
        const char *e = path;
        while (*e && *e != '/') e++;
        usize len = (usize)(e - path);
        if (len == 2 && path[0] == '.' && path[1] == '.') n = n->parent ? n->parent : n;
        else if (!(len == 1 && path[0] == '.')) {
            if (n->type != DN_DIR) return NULL;
            dnode_t *c = child_named(n, path, len, false);
            n = c ? c : child_named(n, path, len, true);
        }
        path = e;
    }
    return n;
}

dnode_t *dfs_first_child(dnode_t *dir) {
    if (!dir || dir->type != DN_DIR) return NULL;
    if (!dir->listed) {
        dir->listed = true;
        if (dir->mnt && dir->mnt->ops->readdir && !dir->mnt->ops->readdir(dir->mnt, dir))
            kprintf("dfs: erro ao ler o diretório %s\n", dir->name);
    }
    return dir->child;
}

i64 dfs_read(dnode_t *f, u64 off, void *buf, u64 len) {
    if (!f || f->type == DN_DIR) return -1;
    if (off >= f->size) return 0;
    len = MIN(len, f->size - off);
    if (f->mem) { memcpy(buf, f->mem + off, len); return (i64)len; }
    if (f->mnt && f->mnt->ops->read) return f->mnt->ops->read(f->mnt, f, off, buf, len);
    return -1;
}

u8 *dfs_read_all(dnode_t *f, u64 max, u64 *out_len) {
    if (!f || f->type == DN_DIR || f->size > max) return NULL;
    u8 *b = kmalloc(f->size + 1);
    if (!b) return NULL;
    i64 r = dfs_read(f, 0, b, f->size);
    if (r < 0) { kfree(b); return NULL; }
    b[r] = 0;
    if (out_len) *out_len = (u64)r;
    return b;
}

void dfs_path(dnode_t *n, char *buf, usize len) {
    if (!n->parent) { snprintf(buf, len, "/"); return; }
    char tmp[1024];
    tmp[0] = 0;
    for (dnode_t *p = n; p && p->parent; p = p->parent) {
        char t2[1024];
        snprintf(t2, sizeof(t2), "/%s%s", p->name, tmp);
        strncpy(tmp, t2, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;
    }
    snprintf(buf, len, "%s", tmp);
}

/* ------------------------------------------------------------ montagem */
static dimage_t *s_cur_img; /* imagem sendo sondada (dfs_attach associa os volumes a ela) */

dmount_t *dfs_attach(const dfs_ops_t *ops, blkdev_t *dev, const char *volname, const char *source,
                     const char *fmt, void *priv, u64 root_ino) {
    dmount_t *m = kcalloc(1, sizeof(*m));
    if (!m) return NULL;
    m->ops = ops;
    m->dev = dev;
    m->priv = priv;
    m->img = s_cur_img;
    if (m->img) m->img->mounts++;
    strncpy(m->volname, volname && *volname ? volname : "Sem Título", sizeof(m->volname) - 1);
    strncpy(m->source, source ? source : "", sizeof(m->source) - 1);
    strncpy(m->format, fmt ? fmt : "", sizeof(m->format) - 1);
    /* "/" não pode aparecer em nome de diretório */
    char name[sizeof(m->volname)];
    strcpy(name, m->volname);
    for (char *c = name; *c; c++) if (*c == '/') *c = ':';
    char uniq[sizeof(name) + 12];
    strcpy(uniq, name);
    for (int i = 1; child_named(s_volumes, uniq, strlen(uniq), false); i++) snprintf(uniq, sizeof(uniq), "%s %d", name, i);
    dnode_t *r = dfs_new_child(s_volumes, uniq, DN_DIR, 0);
    if (!r) { if (m->img) m->img->mounts--; kfree(m); return NULL; }
    r->mnt = m;
    r->ino = root_ino;
    m->root = r;
    m->next = s_mounts;
    s_mounts = m;
    kprintf("dfs: montado \"%s\" (%s, %s) em /Volumes/%s\n", m->volname, ops->name, m->format, uniq);
    return m;
}

static void free_tree(dmount_t *m, dnode_t *n) {
    dnode_t *c = n->child;
    while (c) { dnode_t *nx = c->next; free_tree(m, c); c = nx; }
    if (n->priv && m->ops->free_node) m->ops->free_node(n);
    kfree(n->name);
    kfree(n->priv);
    kfree(n);
}

static void image_release(dimage_t *img) {
    for (int i = 0; i < img->nparts; i++) blk_release(img->parts[i]);
    blk_release(img->disk);
    kfree(img->owned);
    kfree(img);
}

void dfs_unmount(dmount_t *m) {
    for (dnode_t **pp = &s_volumes->child; *pp; pp = &(*pp)->next)
        if (*pp == m->root) { *pp = m->root->next; break; }
    for (dmount_t **pp = &s_mounts; *pp; pp = &(*pp)->next)
        if (*pp == m) { *pp = m->next; break; }
    kprintf("dfs: ejetado \"%s\"\n", m->volname);
    free_tree(m, m->root);
    if (m->ops->unmount) m->ops->unmount(m);
    if (m->img && --m->img->mounts == 0) image_release(m->img);
    kfree(m);
}

void dfs_trim(void) {
    for (dmount_t *m = s_mounts; m; m = m->next)
        if (m->ops->trim) m->ops->trim(m);
}
/* procura sistemas de arquivos conhecidos em um disco: volume direto ou tabela de partições */
static int probe_volume(blkdev_t *d, const char *src, const char *fmt) {
    int n = apfs_probe(d, src, fmt);
    if (!n) n = hfsplus_probe(d, src, fmt);
    if (!n) n = fat_probe(d, src, fmt);
    return n;
}

/* sonda uma partição; se montou algo, a imagem passa a ser dona do slice */
static int probe_part(blkdev_t *p, const char *src, const char *fmt) {
    if (!p) return 0;
    int k = probe_volume(p, src, fmt);
    if (k && s_cur_img && s_cur_img->nparts < (int)ARRAY_LEN(s_cur_img->parts)) s_cur_img->parts[s_cur_img->nparts++] = p;
    else if (!k) blk_release(p);
    return k;
}

static int probe_disk(blkdev_t *disk, const char *src, const char *fmt) {
    int n = probe_volume(disk, src, fmt);
    if (n) return n;
    u8 *s = kmalloc(4096);
    if (!s) return 0;
    if (!blk_read(disk, 0, s, MIN(4096, disk->size))) { kfree(s); return 0; }
    if (!memcmp(s + 512, "EFI PART", 8)) {                       /* GPT */
        u64 lba = le64(s + 512 + 72);
        u32 cnt = le32(s + 512 + 80), esz = le32(s + 512 + 84);
        u8 *e = kmalloc(esz ? esz : 128);
        for (u32 i = 0; e && i < MIN(cnt, 128u) && esz >= 56 && esz <= 4096; i++) {
            if (!blk_read(disk, lba * 512 + (u64)i * esz, e, esz)) break;
            static const u8 zero[16];
            if (!memcmp(e, zero, 16)) continue;
            u64 first = le64(e + 32), last = le64(e + 40);
            if (last < first) continue;
            blkdev_t *p = blk_slice(disk, first * 512, (last - first + 1) * 512);
            n += probe_part(p, src, fmt);
        }
        kfree(e);
    } else if (be16(s + 512) == 0x504D) {                         /* Apple Partition Map */
        u32 bsz = be16(s) == 0x4552 && be16(s + 2) ? be16(s + 2) : 512;
        u32 cnt = be32(s + 512 + 4);
        u8 *e = kmalloc(512);
        for (u32 i = 1; e && i <= MIN(cnt, 64u); i++) {
            if (!blk_read(disk, (u64)i * bsz, e, 512) || be16(e) != 0x504D) break;
            const char *type = (const char *)e + 48;
            if (!strncmp(type, "Apple_partition_map", 19) || !strncmp(type, "Apple_Free", 10) ||
                !strncmp(type, "Apple_Driver", 12)) continue;
            u64 start = (u64)be32(e + 8) * bsz, len = (u64)be32(e + 12) * bsz;
            blkdev_t *p = blk_slice(disk, start, len);
            n += probe_part(p, src, fmt);
        }
        kfree(e);
    } else if (s[510] == 0x55 && s[511] == 0xAA) {               /* MBR */
        for (int i = 0; i < 4; i++) {
            const u8 *e = s + 446 + i * 16;
            if (!e[4]) continue;
            blkdev_t *p = blk_slice(disk, (u64)le32(e + 8) * 512, (u64)le32(e + 12) * 512);
            n += probe_part(p, src, fmt);
        }
    }
    kfree(s);
    return n;
}

static int mount_image(const u8 *data, u64 size, const char *path, u8 *owned) {
    char err[96] = "";
    if (!s_root) dfs_init();
    blkdev_t *disk = dmg_open(data, size, err, sizeof(err));
    if (!disk) { kprintf("dfs: %s: %s\n", path, err); kfree(owned); return 0; }
    dimage_t *img = kcalloc(1, sizeof(*img));
    if (!img) { blk_release(disk); kfree(owned); return 0; }
    img->disk = disk;
    img->owned = owned;
    s_cur_img = img;
    int n = probe_disk(disk, path, dmg_format_name(disk));
    s_cur_img = NULL;
    if (!n) { kprintf("dfs: %s: nenhum volume HFS+ ou APFS encontrado\n", path); image_release(img); }
    return n;
}

dmount_t *dfs_find_source(const char *path) {
    for (dmount_t *m = s_mounts; m; m = m->next)
        if (!strcmp(m->source, path)) return m;
    return NULL;
}

/* disco físico (AHCI...): sonda através de uma fatia do disco inteiro, para que image_release
 * libere só a fatia e nunca o blkdev do driver */
int dfs_mount_disk(blkdev_t *disk, const char *name) {
    if (!s_root) dfs_init();
    blkdev_t *whole = blk_slice(disk, 0, disk->size);
    dimage_t *img = whole ? kcalloc(1, sizeof(*img)) : NULL;
    if (!img) { blk_release(whole); return 0; }
    img->disk = whole;
    s_cur_img = img;
    int n = probe_disk(whole, name, "Disco");
    s_cur_img = NULL;
    if (!n) { kprintf("dfs: %s: nenhum volume HFS+ ou APFS encontrado\n", name); image_release(img); }
    return n;
}

int dfs_mount_memory(const u8 *data, u64 size, const char *path) { return mount_image(data, size, path, NULL); }
int dfs_mount_buffer(u8 *data, u64 size, const char *path) { return mount_image(data, size, path, data); }

int dfs_mount_image(dnode_t *file) {
    char path[1024];
    dfs_path(file, path, sizeof(path));
    if (file->mem) return mount_image(file->mem, file->size, path, NULL);
    /* imagem dentro de outro volume: carrega para a RAM; liberada quando o último volume sair */
    u64 size = 0;
    u8 *data = dfs_read_all(file, 512ull << 20, &size);
    if (!data) { kprintf("dfs: não foi possível ler %s\n", path); return 0; }
    return mount_image(data, size, path, data);
}
void dfs_dump(dnode_t *n, int max_depth) {
    static int depth;
    char ind[40];
    int k = MIN(depth * 2, 38);
    memset(ind, ' ', k);
    ind[k] = 0;
    if (n->type == DN_DIR) kprintf("%s%s/\n", ind, n->name);
    else kprintf("%s%s  (%llu bytes)%s\n", ind, n->name, n->size, n->type == DN_LINK ? " -> link" : "");
    if (n->type != DN_DIR || depth >= max_depth) return;
    depth++;
    int shown = 0;
    for (dnode_t *c = dfs_first_child(n); c; c = c->next) {
        if (c->hidden) continue;
        if (++shown > 24) { kprintf("%s  ...\n", ind); break; }
        dfs_dump(c, max_depth);
    }
    depth--;
}

/* ------------------------------------------------------------ texto */
void utf16be_to_utf8(const u8 *src, u32 n, char *dst, usize cap) {
    usize o = 0;
    for (u32 i = 0; i < n && o + 4 < cap; i++) {
        u32 c = be16(src + i * 2);
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < n) {
            u32 lo = be16(src + (i + 1) * 2);
            if (lo >= 0xDC00 && lo < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); i++; }
        }
        if (c == 0) c = 0x2400; /* nomes internos do HFS+ começam com NULs */
        if (c < 0x80) dst[o++] = (char)c;
        else if (c < 0x800) { dst[o++] = (char)(0xC0 | c >> 6); dst[o++] = (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { dst[o++] = (char)(0xE0 | c >> 12); dst[o++] = (char)(0x80 | ((c >> 6) & 0x3F)); dst[o++] = (char)(0x80 | (c & 0x3F)); }
        else { dst[o++] = (char)(0xF0 | c >> 18); dst[o++] = (char)(0x80 | ((c >> 12) & 0x3F)); dst[o++] = (char)(0x80 | ((c >> 6) & 0x3F)); dst[o++] = (char)(0x80 | (c & 0x3F)); }
    }
    dst[o] = 0;
    utf8_compose(dst);
}

/* o macOS guarda nomes decompostos (NFD): "a" + U+0301. Recompõe os acentos latinos comuns. */
static u32 compose(u32 base, u32 mark) {
    static const struct { u16 mark; const char *from; const u8 *to; } T[] = {
        {0x300, "AEIOUaeiou", (const u8 *)"\xC0\xC8\xCC\xD2\xD9\xE0\xE8\xEC\xF2\xF9"},
        {0x301, "AEIOUYaeiouy", (const u8 *)"\xC1\xC9\xCD\xD3\xDA\xDD\xE1\xE9\xED\xF3\xFA\xFD"},
        {0x302, "AEIOUaeiou", (const u8 *)"\xC2\xCA\xCE\xD4\xDB\xE2\xEA\xEE\xF4\xFB"},
        {0x303, "ANOano", (const u8 *)"\xC3\xD1\xD5\xE3\xF1\xF5"},
        {0x308, "AEIOUaeiouy", (const u8 *)"\xC4\xCB\xCF\xD6\xDC\xE4\xEB\xEF\xF6\xFC\xFF"},
        {0x30A, "Aa", (const u8 *)"\xC5\xE5"},
        {0x327, "Cc", (const u8 *)"\xC7\xE7"},
    };
    if (base >= 0x80) return 0;
    for (u32 i = 0; i < ARRAY_LEN(T); i++)
        if (T[i].mark == mark)
            for (int j = 0; T[i].from[j]; j++)
                if ((u32)(u8)T[i].from[j] == base) return T[i].to[j];
    return 0;
}

void utf8_compose(char *s) {
    u8 *r = (u8 *)s, *w = (u8 *)s;
    while (*r) {
        if (r[0] < 0x80 && (r[1] == 0xCC || r[1] == 0xCD)) {
            u32 mark = ((u32)(r[1] & 0x1F) << 6) | (r[2] & 0x3F);
            u32 c = r[2] ? compose(r[0], mark) : 0;
            if (c) {
                *w++ = (u8)(0xC0 | c >> 6);
                *w++ = (u8)(0x80 | (c & 0x3F));
                r += 3;
                continue;
            }
        }
        *w++ = *r++;
    }
    *w = 0;
}

/* ------------------------------------------------------------ inicialização */
static bool has_ext(const char *name, const char *ext) {
    usize n = strlen(name), e = strlen(ext);
    return n > e && !strcasecmp(name + n - e, ext);
}

void dfs_init(void) {
    if (s_root) return;
    s_root = new_node("", DN_DIR);
    s_root->listed = true;
    s_volumes = dfs_new_child(s_root, "Volumes", DN_DIR, 0);
    s_volumes->listed = true;
}