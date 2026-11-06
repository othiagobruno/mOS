/*
 * mOS — ponte entre os volumes de imagens de disco (dfs: DMG/HFS+/APFS) e o VFS do sistema.
 *
 * Cada volume montado aparece em /Volumes/<nome> como vnodes comuns com ->ext apontando para
 * o dnode correspondente. Nada é copiado até ser usado: o VFS chama vfs_ensure() quando um
 * diretório é percorrido (cria os filhos) ou um arquivo é aberto (carrega o conteúdo).
 *
 * Memória: o conteúdo carregado entra numa fila LRU com orçamento fixo; ao estourar, os mais
 * antigos voltam a ser "não carregados" (data = NULL) e são relidos no próximo vfs_lookup.
 */
#include "vfs.h"
#include "fs.h"

#define MAX_FILE_LOAD (256ull << 20)
#define CACHE_BUDGET  (32ull << 20)   /* conteúdo de arquivos de volumes mantido em RAM */
#define KEEP_RECENT   4               /* nunca descarta os últimos carregados */
#define NAME_MAX_VFS  (sizeof(((vnode_t *)0)->name) - 1)

static dnode_t *lru_head, *lru_tail;  /* head = mais antigo */
static u64 lru_bytes;
static int lru_count;

static void lru_unlink(dnode_t *d) {
    if (!d->in_lru) return;
    if (d->lru_prev) d->lru_prev->lru_next = d->lru_next; else lru_head = d->lru_next;
    if (d->lru_next) d->lru_next->lru_prev = d->lru_prev; else lru_tail = d->lru_prev;
    d->lru_prev = d->lru_next = NULL;
    d->in_lru = false;
    lru_bytes -= d->size;
    lru_count--;
    d->vn = NULL;
}

static void lru_push(dnode_t *d, vnode_t *v) {
    lru_unlink(d);
    d->vn = v;
    d->in_lru = true;
    d->lru_prev = lru_tail;
    if (lru_tail) lru_tail->lru_next = d; else lru_head = d;
    lru_tail = d;
    lru_bytes += d->size;
    lru_count++;
}

/* devolve um arquivo ao estado "não carregado" se o conteúdo ainda for o que nós carregamos */
static void evict(dnode_t *d) {
    vnode_t *v = d->vn;
    lru_unlink(d);
    if (!v || v->ext != d) return;
    if (v->data && v->cap == d->size + 1 && v->size == d->size) {
        kfree(v->data);
        v->data = NULL;
        v->cap = 0;
    }
}

static void enforce_budget(void) {
    if (lru_bytes <= CACHE_BUDGET) return;
    while (lru_head && lru_bytes > CACHE_BUDGET && lru_count > KEEP_RECENT) evict(lru_head);
    dfs_trim(); /* sob pressão, solta também os caches internos dos drivers */
}

/* nomes do VFS têm tamanho fixo: corta sem quebrar um caractere UTF-8 */
static void fit_name(const char *src, char *dst) {
    usize n = 0, len = strlen(src);
    for (; n < len && n < NAME_MAX_VFS; n++) dst[n] = src[n] == '/' ? ':' : src[n];
    while (n && n < len && ((u8)src[n] & 0xC0) == 0x80) n--;
    dst[n] = 0;
}

static void mirror_dir(vnode_t *dir, dnode_t *d) {
    char *base = kmalloc(1024), *path = kmalloc(1024 + NAME_MAX_VFS + 2);
    char name[NAME_MAX_VFS + 1];
    if (!base || !path) { kfree(base); kfree(path); return; }
    vfs_path(dir, base, 1024);
    for (dnode_t *c = dfs_first_child(d); c; c = c->next) {
        fit_name(c->name, name);
        if (!name[0]) continue;
        snprintf(path, 1024 + NAME_MAX_VFS + 2, "%s/%s", base, name);
        vnode_t *v = c->type == DN_DIR ? vfs_mkdir(path) : vfs_create(path);
        if (!v) continue;
        v->ext = c;
        v->mtime = 0;
        if (c->type != DN_DIR) { v->data = NULL; v->size = c->size; v->cap = 0; }
    }
    d->mirrored = true;
    kfree(base);
    kfree(path);
}

bool vfs_ensure(vnode_t *n) {
    if (!n || !n->ext) return true;
    dnode_t *d = n->ext;
    if (n->dir) {
        if (!d->mirrored) mirror_dir(n, d);
        return true;
    }
    if (n->data) {
        if (d->in_lru && d->vn == n) lru_push(d, n); /* uso recente */
        return true;
    }
    u64 len = 0;
    u8 *b = d->size <= MAX_FILE_LOAD ? dfs_read_all(d, MAX_FILE_LOAD, &len) : NULL;
    if (!b) {
        kprintf("dfs: não foi possível ler %s\n", d->name);
        n->size = 0; /* nunca deixa size > 0 com data == NULL para os apps */
        return false;
    }
    n->data = b;
    n->size = len;
    n->cap = len + 1;
    d->size = len;
    lru_push(d, n);
    enforce_budget();
    return true;
}

/* ------------------------------------------------------------ ejetar */
bool vfs_is_volume(vnode_t *n) {
    if (!n || !n->ext) return false;
    dnode_t *d = n->ext;
    return d->mnt && d->mnt->root == d;
}

bool vfs_eject(vnode_t *vol) {
    if (!vfs_is_volume(vol)) return false;
    dmount_t *m = ((dnode_t *)vol->ext)->mnt;
    /* tira da LRU tudo que é deste volume; os buffers são liberados pelo vfs_delete */
    for (dnode_t *d = lru_head, *nx; d; d = nx) {
        nx = d->lru_next;
        if (d->mnt == m) lru_unlink(d);
    }
    vol->ext = NULL; /* vira uma pasta comum só para poder ser removida */
    if (!vfs_delete(vol)) { vol->ext = m->root; return false; }
    dfs_unmount(m);
    return true;
}

/* ------------------------------------------------------------ montagem */
static bool is_image(const char *name);

/* monta uma imagem aberta pelo usuário (ex.: duplo clique num .dmg); retorna nº de volumes */
int vfs_mount_file(vnode_t *f) {
    if (!f || f->dir || !is_image(f->name)) return 0;
    char path[1024];
    vfs_path(f, path, sizeof(path));
    if (dfs_find_source(path)) return 0; /* já montada */
    dmount_t *before = dfs_mounts();
    int n;
    if (f->ext) {
        u64 len = 0;                                        /* .dmg dentro de outro volume */
        u8 *data = dfs_read_all(f->ext, 512ull << 20, &len);
        n = data ? dfs_mount_buffer(data, len, path) : 0;
    } else if (f->cap == 0 && f->data) {
        n = dfs_mount_memory(f->data, f->size, path);       /* memória do pendrive: fixa */
    } else {
        /* arquivo criado/alterado nesta sessão: pode ser realocado, então usa uma cópia */
        u8 *copy = f->size ? kmalloc(f->size) : NULL;
        if (!copy) return 0;
        memcpy(copy, f->data, f->size);
        n = dfs_mount_buffer(copy, f->size, path);
    }
    if (n) dfs_publish_new(before);
    return n;
}

static bool is_image(const char *name) {
    usize n = strlen(name);
    return n > 4 && (!strcasecmp(name + n - 4, ".dmg") || !strcasecmp(name + n - 4, ".img"));
}

/* publica em /Volumes do VFS os volumes montados depois de efore (ex.: dfs_mount_disk);
 * chame depois de vfs_init */
void dfs_publish_new(dmount_t *before) {
    char name[NAME_MAX_VFS + 1], vpath[NAME_MAX_VFS + 16];
    for (dmount_t *m = dfs_mounts(); m && m != before; m = m->next) {
        fit_name(m->root->name, name);
        snprintf(vpath, sizeof(vpath), "/Volumes/%s", name);
        vnode_t *v = vfs_mkdir(vpath);
        if (v) v->ext = m->root;
    }
}

static void scan(vnode_t *dir, int depth) {
    if (depth > 8) return;
    for (vnode_t *c = dir->child; c; c = c->next) {
        if (c->ext) continue; /* não entra nos volumes já montados */
        if (c->dir) { scan(c, depth + 1); continue; }
        if (!is_image(c->name) || !c->data || !c->size) continue;
        char path[1024];
        vfs_path(c, path, sizeof(path));
        dmount_t *before = dfs_mounts();
        if (dfs_mount_memory(c->data, c->size, path)) dfs_publish_new(before);
    }
}

void dfs_mount_all(void) {
    dfs_init();
    vfs_mkdir("/Volumes");
    scan(vfs_root(), 0);
    int n = 0;
    for (dmount_t *m = dfs_mounts(); m; m = m->next) n++;
    kprintf("dfs: %d volume(s) de imagens de disco em /Volumes\n", n);
}
