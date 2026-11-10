/* Registro no Launch Services dos XPC e do plugin de autorização de um .app.
 * O plugin não é carregado e não recebe privilégio. */
#include "kernel.h"
#include "vfs.h"
#include "bundlereg.inc"

static int br_vfs_list(void *ctx, const char *dir, int index, char *name, int cap) {
    vnode_t *d, *c;
    int i = 0, k;
    (void)ctx;
    if (!name || cap < 2) return 0;
    d = vfs_lookup(dir);
    if (!d || !d->dir) return 0;
    for (c = d->child; c; c = c->next, i++) if (i == index) {
        for (k = 0; c->name[k] && k + 1 < cap; k++) name[k] = c->name[k];
        name[k] = 0;
        return c->name[k] ? 0 : 1;
    }
    return 0;
}

static int br_vfs_read(void *ctx, const char *path, char *buf, int cap) {
    vnode_t *v;
    int n;
    (void)ctx;
    if (!buf || cap < 2) return -1;
    memset(buf, 0, (usize)cap);
    v = vfs_lookup(path);
    if (!v || v->dir) return -1;
    n = (int)v->size;
    if (n > cap - 1) n = cap - 1;
    if (n < 0) n = 0;
    if (n && v->data) memcpy(buf, v->data, (usize)n);
    else n = 0;
    buf[n] = 0;
    return n;
}

static int br_vfs_isfile(void *ctx, const char *path) {
    vnode_t *v = vfs_lookup(path);
    (void)ctx;
    return v && !v->dir;
}

void br_vfs_announce(const char *root) {
    br_item_t *it;
    br_fs_t fs;
    char line[480];
    int n, i;
    if (!root || !root[0]) return;
    it = kmalloc(sizeof(*it) * BR_MAX);
    if (!it) return;
    memset(it, 0, sizeof(*it) * BR_MAX);
    fs.list = br_vfs_list;
    fs.read = br_vfs_read;
    fs.isfile = br_vfs_isfile;
    fs.ctx = 0;
    n = br_scan(&fs, root, it, BR_MAX);
    for (i = 0; i < n; i++) if (br_fmt(&it[i], line, (int)sizeof line) > 0) kprintf("ls: %s\n", line);
    kfree(it);
}
