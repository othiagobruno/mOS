/* mOS — VFS em memória */
#include "vfs.h"

extern const char *account_home(void) __attribute__((weak));
const char *home_sub(const char *sub) {
    static char bufs[8][512];
    static int k;
    char *b = bufs[k++ & 7];
    const char *h = account_home ? account_home() : NULL;
    snprintf(b, 512, "%s%s", h && h[0] ? h : HOME, sub ? sub : "");
    return b;
}

static vnode_t root = {.name = "", .dir = true};

/* implementações padrão; a ponte de FS externos as substitui com definições fortes */
__attribute__((weak)) bool vfs_ensure(vnode_t *n) { (void)n; return true; }
__attribute__((weak)) void dfs_mount_all(void) {}
__attribute__((weak)) bool vfs_is_volume(vnode_t *n) { (void)n; return false; }
__attribute__((weak)) bool vfs_eject(vnode_t *vol) { (void)vol; return false; }
__attribute__((weak)) int vfs_mount_file(vnode_t *f) { (void)f; return 0; }

static int ensuring;
static void ensure(vnode_t *n) {
    if (n && n->ext && !ensuring) { ensuring++; vfs_ensure(n); ensuring--; }
}

/* volumes externos (.dmg etc.) são somente leitura: o nó ou algum ancestral tem ext.
 * Exceção: enquanto a ponte materializa um volume (dentro de ensure) ela pode criar nós. */
static bool read_only(vnode_t *n) {
    if (ensuring) return false;
    for (; n; n = n->parent) if (n->ext) return true;
    return false;
}
bool vfs_is_read_only(vnode_t *n) { return read_only(n); }

vnode_t *vfs_root(void) { return &root; }

vnode_t *vfs_child(vnode_t *dir, const char *name) {
    if (!dir || !dir->dir) return NULL;
    ensure(dir);
    for (vnode_t *c = dir->child; c; c = c->next) if (!strcmp(c->name, name)) return c;
    return NULL;
}

static vnode_t *add_child(vnode_t *dir, const char *name, bool isdir) {
    vnode_t *n = kcalloc(1, sizeof(vnode_t));
    strncpy(n->name, name, sizeof(n->name) - 1);
    n->dir = isdir;
    n->parent = dir;
    n->mtime = timer_ticks();
    /* inserção ordenada (diretórios e arquivos misturados, ordem alfabética sem caixa) */
    vnode_t **pp = &dir->child;
    while (*pp && strcasecmp((*pp)->name, name) < 0) pp = &(*pp)->next;
    n->next = *pp;
    *pp = n;
    persist_mark(n);
    return n;
}

static vnode_t *walk(const char *path, bool create_dirs, bool last_is_dir) {
    vnode_t *cur = &root;
    char part[256];
    const char *p = path;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        int n = 0;
        while (*p && *p != '/' && n < 255) part[n++] = *p++;
        part[n] = 0;
        while (*p && *p != '/') p++;
        if (!strcmp(part, ".")) continue;
        if (!strcmp(part, "..")) { if (cur->parent) cur = cur->parent; continue; }
        vnode_t *c = vfs_child(cur, part);
        if (!c) {
            if (!create_dirs || read_only(cur)) return NULL;
            bool more = false;
            for (const char *q = p; *q; q++) if (*q != '/') { more = true; break; }
            c = add_child(cur, part, more || last_is_dir);
        }
        cur = c;
    }
    return cur;
}

vnode_t *vfs_lookup(const char *path) { vnode_t *n = walk(path, false, false); ensure(n); return n; }
vnode_t *vfs_mkdir(const char *path) { return walk(path, true, true); }

vnode_t *vfs_create(const char *path) {
    vnode_t *n = vfs_lookup(path);
    if (n && (n->dir || read_only(n))) return NULL;
    if (!n) n = walk(path, true, false);
    if (n) { n->size = 0; n->mtime = timer_ticks(); persist_mark(n); }
    return n;
}

bool vfs_append(vnode_t *f, const void *data, usize len) {
    if (!f || f->dir || read_only(f)) return false;
    if (f->size + len + 1 > f->cap) {
        usize nc = MAX(f->cap * 2, f->size + len + 256);
        u8 *nd = kmalloc(nc);
        if (!nd) return false;
        if (f->data) memcpy(nd, f->data, f->size);
        if (f->cap) kfree(f->data); /* cap 0 = dados do pendrive, não liberar */
        f->data = nd;
        f->cap = nc;
    }
    memcpy(f->data + f->size, data, len);
    f->size += len;
    f->data[f->size] = 0;
    f->mtime = timer_ticks();
    persist_mark(f);
    return true;
}

vnode_t *vfs_write_file(const char *path, const void *data, usize n) {
    vnode_t *f = vfs_create(path);
    if (f) vfs_append(f, data, n);
    return f;
}

static void free_tree(vnode_t *n) {
    for (vnode_t *c = n->child, *nx; c; c = nx) { nx = c->next; free_tree(c); }
    if (n->cap) kfree(n->data);
    kfree(n);
}

static void unlink_node(vnode_t *n) {
    vnode_t **pp = &n->parent->child;
    while (*pp && *pp != n) pp = &(*pp)->next;
    if (*pp) *pp = n->next;
    n->next = NULL;
}

bool vfs_delete(vnode_t *n) {
    if (!n || n == &root || !n->parent || read_only(n)) return false;
    persist_delete(n);
    unlink_node(n);
    free_tree(n);
    return true;
}

bool vfs_move(vnode_t *n, vnode_t *np, const char *newname) {
    if (!n || !np || !np->dir || n == &root || read_only(n) || read_only(np)) return false;
    for (vnode_t *p = np; p; p = p->parent) if (p == n) return false;
    const char *nm = newname ? newname : n->name;
    char name[256];
    strncpy(name, nm, 255);
    name[255] = 0;
    int k = 2;
    while (vfs_child(np, name) && vfs_child(np, name) != n) snprintf(name, sizeof name, "%s %d", nm, k++);
    char *old = kmalloc(1024);
    if (old) vfs_path(n, old, 1024);
    unlink_node(n);
    strcpy(n->name, name);
    n->parent = np;
    vnode_t **pp = &np->child;
    while (*pp && strcasecmp((*pp)->name, name) < 0) pp = &(*pp)->next;
    n->next = *pp;
    *pp = n;
    if (old) { persist_move(old, n); kfree(old); }
    return true;
}

void vfs_path(vnode_t *n, char *buf, usize cap) {
    if (!n || n == &root) { strncpy(buf, "/", cap); return; }
    char tmp[1024];
    tmp[0] = 0;
    for (vnode_t *p = n; p && p != &root; p = p->parent) {
        char t2[1024];
        snprintf(t2, sizeof t2, "/%s%s", p->name, tmp);
        strcpy(tmp, t2);
    }
    strncpy(buf, tmp, cap);
    buf[cap - 1] = 0;
}

int vfs_count(vnode_t *d) { int n = 0; for (vnode_t *c = d ? d->child : NULL; c; c = c->next) n++; return n; }

static char search_buf[8][128];
static int search_rec(vnode_t *d, const char *q, const char **out, int n, int max) {
    for (vnode_t *c = d->child; c && n < max; c = c->next) {
        if (c->name[0] == '.') continue;
        char a[256], b[256];
        int i;
        for (i = 0; c->name[i] && i < 255; i++) a[i] = (char)tolower(c->name[i]);
        a[i] = 0;
        for (i = 0; q[i] && i < 255; i++) b[i] = (char)tolower(q[i]);
        b[i] = 0;
        if (strstr(a, b)) { vfs_path(c, search_buf[n], 128); out[n] = search_buf[n]; n++; }
        if (c->dir) n = search_rec(c, q, out, n, max);
    }
    return n;
}
int vfs_search(const char *q, const char **out, int max) { return q[0] ? search_rec(&root, q, out, 0, MIN(max, 8)) : 0; }

static usize total_rec(vnode_t *d) { usize s = d->size; for (vnode_t *c = d->child; c; c = c->next) s += total_rec(c); return s; }
usize vfs_total_bytes(void) { return total_rec(&root); }

const char *vfs_ext(const char *name) { const char *d = strrchr(name, '.'); return d && d != name ? d + 1 : ""; }

const char *vfs_kind(vnode_t *n) {
    if (n->dir) {
        const char *e = vfs_ext(n->name);
        if (!strcasecmp(e, "app") && vfs_child(n, "Contents")) return "Aplicativo do macOS";
        return "Pasta";
    }
    extern bool macho_is_macho(const u8 *data, usize len) __attribute__((weak));
    if (macho_is_macho && n->data && n->size >= 4 && macho_is_macho(n->data, n->size)) return "Executável do macOS";
    const char *e = vfs_ext(n->name);
    if (!strcasecmp(e, "txt") || !strcasecmp(e, "md")) return "Documento de texto";
    if (!strcasecmp(e, "html") || !strcasecmp(e, "htm")) return "Página HTML";
    if (!strcasecmp(e, "bmp") || !strcasecmp(e, "ppm")) return "Imagem";
    if (!strcasecmp(e, "wav")) return "Áudio WAV";
    if (!strcasecmp(e, "efi")) return "Executável UEFI";
    if (!strcasecmp(e, "dmg") || !strcasecmp(e, "img") || !strcasecmp(e, "iso")) return "Imagem de disco";
    if (!strcasecmp(e, "app")) return "Aplicativo";
    if (!strcasecmp(e, "sh")) return "Script de shell";
    return "Documento";
}

void vfs_init(void) {
    static const char *dirs[] = {
        "/Aplicativos", "/Sistema/Biblioteca", "/Users/Visitante/Mesa", "/Users/Visitante/Documentos",
        "/Users/Visitante/Downloads", "/Users/Visitante/Imagens", "/Users/Visitante/Música", TRASH, "/tmp"};
    for (u32 i = 0; i < ARRAY_LEN(dirs); i++) vfs_mkdir(dirs[i]);
    int nf = 0;
    /* com disco do sistema (AHCI + FAT32), a árvore vem direto de \mOS\ no disco */
    if (!persist_load()) for (u32 i = 0; i < g_boot->file_count; i++) {
        boot_file_t *bf = &g_boot->files[i];
        if (bf->size == ~0ULL) { vfs_mkdir(bf->path); continue; }
        vnode_t *f = walk(bf->path, true, false);
        if (!f) continue;
        f->data = bf->data; f->size = bf->size; f->cap = 0; f->mtime = 0;
        nf++;
    }
    vfs_write_file("/Sistema/Biblioteca/versao.txt", MOS_NAME " " MOS_VERSION "\n", strlen(MOS_NAME " " MOS_VERSION "\n"));
    if (persist_available()) kprintf("VFS: arquivos carregados do disco do sistema (gravação ativa)\n");
    else kprintf("VFS: %d arquivos carregados do pendrive (somente RAM)\n", nf);
    persist_enable();
}
