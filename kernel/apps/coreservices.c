/*
 * mOS — apps de /System/Library/CoreServices que abrem documentos sem aparecer no Launchpad:
 *  - Utilitário de Compactação (com.apple.archiveutility): descompacta .zip ao lado do arquivo, como no macOS
 *    (um item no topo vai direto para a pasta; vários itens vão para uma pasta com o nome do .zip).
 *    Links simbólicos do zip (frameworks de apps) viram links do mOS ("#mOS-link:alvo").
 *  - DiskImageMounter (com.apple.DiskImageMounter): monta .dmg/.img/.iso em /Volumes.
 * Os dois são registrados no Launch Services (kernel/compat/launchsvc.c) como quem abre esses tipos; um app de
 * terceiros que declare o tipo com LSHandlerRank Owner, ou que o usuário escolha, passa na frente.
 */
#include "kit.h"
#include "web.h"

void finder_open_path(const char *path);

static u16 le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u32 le32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

typedef struct { const u8 *z; usize n; usize cd; u32 count; } zipdir_t;

static bool zip_open(const u8 *z, usize n, zipdir_t *zd) {
    if (n < 22) return false;
    usize lim = n > 65557 ? n - 65557 : 0;
    for (usize i = n - 22;; i--) {
        if (le32(z + i) == 0x06054b50) {
            zd->z = z; zd->n = n; zd->cd = le32(z + i + 16); zd->count = le16(z + i + 10);
            return zd->cd < n;
        }
        if (i == lim || i == 0) break;
    }
    return false;
}

/* entrada idx: nome (sem NUL) e dados; false no fim ou se estiver corrompida */
typedef struct { const char *name; int nl; const u8 *data; u32 csize, usize_; int method; u32 mode; } zent_t;
static bool zip_next(zipdir_t *zd, usize *p, zent_t *e) {
    const u8 *z = zd->z;
    if (*p + 46 > zd->n || le32(z + *p) != 0x02014b50) return false;
    e->method = le16(z + *p + 10);
    e->csize = le32(z + *p + 20);
    e->usize_ = le32(z + *p + 24);
    e->nl = le16(z + *p + 28);
    int el = le16(z + *p + 30), cl = le16(z + *p + 32);
    e->mode = le32(z + *p + 38) >> 16;
    u32 lho = le32(z + *p + 42);
    e->name = (const char *)z + *p + 46;
    *p += 46 + (usize)e->nl + el + cl;
    if ((usize)lho + 30 > zd->n) return false;
    usize data = lho + 30 + le16(z + lho + 26) + le16(z + lho + 28);
    if (data + e->csize > zd->n) return false;
    e->data = z + data;
    return true;
}

static bool skip_entry(const zent_t *e) {
    if (e->nl >= 9 && !memcmp(e->name, "__MACOSX/", 9)) return true;
    const char *b = e->name;
    for (int i = 0; i < e->nl; i++) if (e->name[i] == '/' && i + 1 < e->nl) b = e->name + i + 1;
    return b[0] == '.' && b[1] == '_';   /* "._arquivo": forks de recursos do Finder */
}

/* nome livre na pasta: "X", "X 2", "X 3"... (com a extensão no fim, como o macOS) */
static void unique_path(const char *dir, const char *name, char *out, usize cap) {
    snprintf(out, cap, "%s/%s", dir, name);
    if (!vfs_lookup(out)) return;
    char stem[200], ext[40] = "";
    snprintf(stem, sizeof stem, "%s", name);
    char *dot = strrchr(stem, '.');
    if (dot && dot != stem) { snprintf(ext, sizeof ext, "%s", dot); *dot = 0; }
    for (int k = 2; k < 1000; k++) {
        snprintf(out, cap, "%s/%s %d%s", dir, stem, k, ext);
        if (!vfs_lookup(out)) return;
    }
}

void archive_open_file(const char *path) {
    vnode_t *f = vfs_lookup(path);
    if (!f || f->dir || !f->data) return;
    zipdir_t zd;
    char base[200];
    snprintf(base, sizeof base, "%s", f->name);
    if (!zip_open(f->data, f->size, &zd)) { notify("Não foi possível expandir", base); return; }
    char dir[300];
    snprintf(dir, sizeof dir, "%s", path);
    char *sl = strrchr(dir, '/');
    if (sl && sl != dir) *sl = 0; else snprintf(dir, sizeof dir, "/");
    if (vfs_is_read_only(vfs_lookup(dir))) snprintf(dir, sizeof dir, "%s", home_sub("/Downloads"));
    /* um item só no topo? */
    char top[256] = "";
    bool single = true;
    usize p = zd.cd;
    zent_t e;
    for (u32 i = 0; i < zd.count && zip_next(&zd, &p, &e); i++) {
        if (skip_entry(&e)) continue;
        int tl = 0;
        while (tl < e.nl && e.name[tl] != '/') tl++;
        if (!top[0]) { snprintf(top, sizeof top, "%.*s", tl, e.name); continue; }
        if ((int)strlen(top) != tl || memcmp(top, e.name, (usize)tl)) { single = false; break; }
    }
    if (!top[0]) { notify("O arquivo está vazio", base); return; }
    char *dot = strrchr(base, '.');
    if (dot && dot != base) *dot = 0;
    /* destino: o item do topo com nome livre, ou uma pasta nova com o nome do .zip */
    char dest[400], strip[256];
    if (single) {
        unique_path(dir, top, dest, sizeof dest);
        snprintf(strip, sizeof strip, "%s", top);
    } else {
        unique_path(dir, base, dest, sizeof dest);
        vfs_mkdir(dest);
        strip[0] = 0;
    }
    int files = 0, bad = 0;
    p = zd.cd;
    for (u32 i = 0; i < zd.count && zip_next(&zd, &p, &e); i++) {
        if (skip_entry(&e) || e.nl <= 0) continue;
        char rel[512];
        int sl0 = (int)strlen(strip);
        if (sl0 && e.nl >= sl0 && !memcmp(e.name, strip, (usize)sl0)) snprintf(rel, sizeof rel, "%.*s", e.nl - sl0, e.name + sl0);
        else snprintf(rel, sizeof rel, "/%.*s", e.nl, e.name);
        if (strstr(rel, "/../")) { bad++; continue; }
        /* rel: o resto depois do item do topo ("" ou "/..."), ou o caminho todo com "/" na frente */
        char full[1024];
        snprintf(full, sizeof full, "%s%s", dest, rel);
        usize fl = strlen(full);
        if (fl && full[fl - 1] == '/') { full[fl - 1] = 0; vfs_mkdir(full); continue; }
        char parent[1024];
        snprintf(parent, sizeof parent, "%s", full);
        char *ps = strrchr(parent, '/');
        if (ps && ps != parent) { *ps = 0; vfs_mkdir(parent); }
        if (e.usize_ > 512u * 1024 * 1024) { bad++; continue; }
        u8 *buf = kmalloc(e.usize_ + 1);
        if (!buf) { bad++; continue; }
        usize got = 0;
        bool ok = true;
        if (e.method == 0) { memcpy(buf, e.data, MIN(e.csize, e.usize_)); got = MIN(e.csize, e.usize_); }
        else if (e.method == 8) ok = raw_inflate(e.data, e.csize, buf, e.usize_, &got);
        else ok = false;
        if (ok && (e.mode & 0170000) == 0120000) {
            /* link simbólico: o conteúdo é o alvo */
            char lk[600];
            int n = snprintf(lk, sizeof lk, "#mOS-link:%.*s", (int)MIN(got, 500), (const char *)buf);
            vfs_write_file(full, lk, (usize)n);
        } else if (ok) vfs_write_file(full, buf, got);
        else bad++;
        kfree(buf);
        files++;
    }
    kprintf("compactacao: %s -> %s (%d itens, %d com erro)\n", path, dest, files, bad);
    if (bad) {
        char msg[200];
        snprintf(msg, sizeof msg, "%d %s não puderam ser expandidos (formato não suportado).", bad, bad == 1 ? "item" : "itens");
        notify("Utilitário de Compactação", msg);
    }
}

void diskimage_open_file(const char *path) {
    vnode_t *n = vfs_lookup(path);
    if (!n) return;
    char msg[300];
    if (vfs_mount_file(n) > 0) {
        snprintf(msg, sizeof msg, "\"%s\" montado", n->name);
        notify(msg, "Disponível em Volumes, na barra lateral do Arquivos.");
        vnode_t *vols = vfs_lookup("/Volumes");
        if (vols && vols->child) finder_open_path("/Volumes");
    } else {
        notify("Não foi possível abrir a imagem", n->name);
    }
}

static void zip_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(150, 152, 160), RGB(96, 98, 108));
    gfx_round_rect(s, x + 20 * u, y + 12 * u, 24 * u, 40 * u, 3 * u, RGB(236, 236, 240));
    for (int i = 0; i < 6; i++) gfx_fill(s, (int)(x + (i & 1 ? 32 : 28) * u), (int)(y + (14 + i * 5) * u), (int)(4 * u), (int)(3 * u), RGB(90, 92, 100));
    gfx_round_rect(s, x + 27 * u, y + 44 * u, 10 * u, 6 * u, 2 * u, RGB(90, 92, 100));
}
static void disk_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    gfx_round_rect_grad(s, x + 8 * u, y + 14 * u, 48 * u, 36 * u, 6 * u, RGB(230, 230, 236), RGB(170, 172, 180));
    gfx_circle(s, x + 32 * u, y + 32 * u, 10 * u, RGB(120, 122, 130));
    gfx_circle(s, x + 32 * u, y + 32 * u, 3 * u, RGB(230, 230, 236));
}
static void noop_launch(app_t *a) { a->running = false; }
app_t app_compactacao = {.name = "Utilitário de Compactação", .exec = "compactacao", .draw_icon = zip_icon, .launch = noop_launch};
app_t app_dimounter = {.name = "DiskImageMounter", .exec = "dimounter", .draw_icon = disk_icon, .launch = noop_launch};
