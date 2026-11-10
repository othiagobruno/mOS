/* mOS — VFS em memória */
#include "vfs.h"
#include "fwlink.h"
#include "account.h"

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
__attribute__((weak)) bool vfs_ext_writable(vnode_t *n) { (void)n; return false; }

static int ensuring;
static void ensure(vnode_t *n) {
    if (n && n->ext && !ensuring) { ensuring++; vfs_ensure(n); ensuring--; }
}

/* volumes externos (.dmg, APFS...) são somente leitura: o ancestral mais próximo com ext decide;
 * volumes FAT/HFS+ de discos graváveis aceitam escrita (vfs_bridge.c leva as mudanças ao disco).
 * Exceção: enquanto a ponte materializa um volume (dentro de ensure) ela pode criar nós. */
static bool read_only(vnode_t *n) {
    if (ensuring) return false;
    for (; n; n = n->parent) if (n->ext) return !vfs_ext_writable(n);
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
    n->mode = isdir ? 0755 : 0644;
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

/* links simbólicos do mOS: arquivo que começa com "#mOS-link:<alvo>" (o libSystem cria com symlink()).
 * O walk segue os links nos componentes do MEIO do caminho, como diretórios ("opt/tree" -> "../Cellar/tree/2.3.2",
 * "portable-ruby/current" -> "4.0.9"); o último componente fica com quem chama (stat segue, lstat não, o execve
 * do kernel segue). Alvo relativo vale a partir da pasta do link. */
#define VFS_LINK_MAGIC "#mOS-link:"
static vnode_t *walk_at(vnode_t *start, const char *path, bool create_dirs, bool last_is_dir, int depth);

static bool vnode_in_framework(vnode_t *n) {
    for (; n; n = n->parent)
        if (n->name[0] && strstr(n->name, ".framework")) return true;
    return false;
}

static vnode_t *follow_dir_link(vnode_t *dirnode, vnode_t *c, int depth) {
    char tgt[512];
    int magic;
    if (c->dir) return c;
    ensure(c);
    if (!c->data || !c->size) return c;
    magic = c->size >= 10 && !memcmp(c->data, VFS_LINK_MAGIC, 10);
    /* #mOS-link: em qualquer pasta. Texto puro ("A", "Versions/Current/Electron Framework") só dentro de
     * .framework: é o symlink que o Windows gravou como arquivo. Fora disso um arquivo pequeno continua arquivo. */
    if (!magic && !vnode_in_framework(c)) return c;
    if (!fwlink_target(c->data, c->size, tgt, sizeof tgt)) return c;
    if (depth >= 8) return NULL;                            /* ELOOP */
    return walk_at(tgt[0] == '/' ? &root : dirnode, tgt, false, false, depth + 1);
}

static vnode_t *walk(const char *path, bool create_dirs, bool last_is_dir) { return walk_at(&root, path, create_dirs, last_is_dir, 0); }

static vnode_t *walk_at(vnode_t *start, const char *path, bool create_dirs, bool last_is_dir, int depth) {
    vnode_t *cur = start;
    char part[256];
    const char *p = path;
    if (*p == '/') cur = &root;
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
        bool more_parts = false;
        for (const char *q = p; *q; q++) if (*q != '/') { more_parts = true; break; }
        if (more_parts && !c->dir) {                        /* componente do meio que é um link: segue */
            c = follow_dir_link(cur, c, depth);
            if (!c || !c->dir) return NULL;
        }
        cur = c;
    }
    return cur;
}

vnode_t *vfs_lookup(const char *path) { vnode_t *n = walk(path, false, false); ensure(n); return n; }
/* pasta pelo caminho, seguindo também um link no último componente ("opt/tree" -> Cellar): rename e afins */
vnode_t *vfs_lookup_dir(const char *path) {
    vnode_t *n = walk(path, false, false);
    if (n && !n->dir && n->parent) n = follow_dir_link(n->parent, n, 0);
    ensure(n);
    return n && n->dir ? n : NULL;
}
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
    /* buffer de kmalloc_pages (arquivo grande lido do disco): o macho.c pode ter mapeado páginas dele nos processos (o
     * __TEXT dos frameworks), então nada se escreve nele no lugar; o conteúdo vai para um buffer novo, e o kfree do velho
     * só devolve as páginas que nenhum processo usa mais */
    bool lent = f->cap && kmalloc_is_pages(f->data);
    if (lent || f->size + len + 1 > f->cap) {
        usize nc = lent ? f->size + len + 256 : MAX(f->cap * 2, f->size + len + 256);
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
    if (!n || n == &root || !n->parent || read_only(n) || vfs_is_volume(n)) return false; /* volume: vfs_eject */
    persist_delete(n);
    unlink_node(n);
    free_tree(n);
    return true;
}

bool vfs_move(vnode_t *n, vnode_t *np, const char *newname) {
    if (!n || !np || !np->dir || n == &root || read_only(n) || read_only(np) || vfs_is_volume(n)) return false;
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

/* Copia um arquivo ou uma pasta (bundles .app saídos de um .dmg). O destino que já existe não é trocado. */
static bool copy_rec(vnode_t *s, char *dst, usize len) {
    if (!s) return false;
    if (vfs_lookup(dst)) return true;
    if (s->dir) {
        if (!vfs_mkdir(dst)) return false;
        for (vnode_t *c = s->child; c; c = c->next) {
            usize n = strlen(c->name);
            if (len + 1 + n + 1 > 1024) return false;
            dst[len] = '/';
            memcpy(dst + len + 1, c->name, n + 1);
            if (!copy_rec(c, dst, len + 1 + n)) return false;
            dst[len] = 0;
        }
        return true;
    }
    ensure(s);
    return vfs_write_file(dst, s->data ? s->data : (const void *)"", s->data ? s->size : 0) != NULL;
}
bool vfs_copy_tree(const char *src, const char *dst) {
    vnode_t *s = vfs_lookup(src);
    char buf[1024];
    if (!s || !dst || !dst[0]) return false;
    snprintf(buf, sizeof buf, "%s", dst);
    return copy_rec(s, buf, strlen(buf));
}

void vfs_init(void) {
    static const char *dirs[] = {
        "/Aplicativos", "/Sistema/Biblioteca", "/Users/Visitante/Mesa", "/Users/Visitante/Documentos",
        "/Users/Visitante/Downloads", "/Users/Visitante/Imagens", "/Users/Visitante/Música", TRASH, "/tmp"};
    for (u32 i = 0; i < ARRAY_LEN(dirs); i++) vfs_mkdir(dirs[i]);
    account_ensure_dirs(HOME);
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
