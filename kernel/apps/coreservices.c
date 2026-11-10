/*
 * mOS — apps de /System/Library/CoreServices que abrem documentos sem aparecer no Launchpad:
 *  - Utilitário de Compactação (com.apple.archiveutility): descompacta .zip ao lado do arquivo, como no macOS
 *    (um item no topo vai direto para a pasta; vários itens vão para uma pasta com o nome do .zip).
 *    Links simbólicos do zip (frameworks de apps) viram links do mOS ("#mOS-link:alvo").
 *  - DiskImageMounter (com.apple.DiskImageMounter): monta .dmg/.img/.iso em /Volumes.
 *    Um .pkg/.mpkg flat (xar) tem o Payload extraído para /Aplicativos. Scripts não rodam.
 * Os dois são registrados no Launch Services (kernel/compat/launchsvc.c) como quem abre esses tipos; um app de
 * terceiros que declare o tipo com LSHandlerRank Owner, ou que o usuário escolha, passa na frente.
 */
#include "kit.h"
#include "web.h"
#include "fs.h"
#include "launchsvc.h"
#include "vfs.h"
#include "pkgextract.h"

void finder_open_path(const char *path);

static u16 zle16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u32 zle32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

typedef struct { const u8 *z; usize n; usize cd; u32 count; } zipdir_t;

static bool zip_open(const u8 *z, usize n, zipdir_t *zd) {
    if (n < 22) return false;
    usize lim = n > 65557 ? n - 65557 : 0;
    for (usize i = n - 22;; i--) {
        if (zle32(z + i) == 0x06054b50) {
            zd->z = z; zd->n = n; zd->cd = zle32(z + i + 16); zd->count = zle16(z + i + 10);
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
    if (*p + 46 > zd->n || zle32(z + *p) != 0x02014b50) return false;
    e->method = zle16(z + *p + 10);
    e->csize = zle32(z + *p + 20);
    e->usize_ = zle32(z + *p + 24);
    e->nl = zle16(z + *p + 28);
    int el = zle16(z + *p + 30), cl = zle16(z + *p + 32);
    e->mode = zle32(z + *p + 38) >> 16;
    u32 lho = zle32(z + *p + 42);
    e->name = (const char *)z + *p + 46;
    *p += 46 + (usize)e->nl + el + cl;
    if ((usize)lho + 30 > zd->n) return false;
    usize data = lho + 30 + zle16(z + lho + 26) + zle16(z + lho + 28);
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

static void zip_expand(const char *path) {
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

static int ends_pkg(const char *name) {
    usize n = name ? strlen(name) : 0;
    return n > 4 && (!strcasecmp(name + n - 4, ".pkg") || (n > 5 && !strcasecmp(name + n - 5, ".mpkg")));
}

/* Cada componente do meio precisa ser um diretório de verdade. vfs_mkdir segue link no meio do caminho. */
static vnode_t *pkg_real_dir(const char *path) {
    vnode_t *cur = vfs_lookup("/");
    const char *p;
    if (!cur || !path || path[0] != '/') return NULL;
    p = path + 1;
    if (!*p) return cur;
    while (*p) {
        char part[256];
        int n = 0;
        vnode_t *c;
        while (*p && *p != '/' && n < 255) part[n++] = *p++;
        part[n] = 0;
        while (*p == '/') p++;
        if (!n) continue;
        c = vfs_child(cur, part);
        if (!c || !c->dir) return NULL;
        cur = c;
    }
    return cur;
}

static int pkg_rel_ok(const char *rel) {
    const char *p;
    if (!rel || !rel[0] || rel[0] == '/') return 0;
    p = rel;
    while (*p) {
        const char *s = p;
        int n;
        while (*p && *p != '/') p++;
        n = (int)(p - s);
        if (n <= 0 || (n == 1 && s[0] == '.') || (n == 2 && s[0] == '.' && s[1] == '.')) return 0;
        if (*p == '/') p++;
    }
    return 1;
}

static int pkg_vfs_write(void *ctx, const char *rel, const void *data, u64 n, int kind) {
    const char *root = ctx;
    char cur[1024], next[1024];
    const char *p = rel;
    vnode_t *parent;
    if (!pkg_rel_ok(rel) || !root) return -1;
    if (!pkg_real_dir(root) && !vfs_mkdir(root)) return -1;
    parent = pkg_real_dir(root);
    if (!parent) return -1;
    snprintf(cur, sizeof cur, "%s", root);
    while (*p) {
        char part[256];
        int ln = 0, last;
        vnode_t *c;
        while (*p && *p != '/' && ln < 255) part[ln++] = *p++;
        part[ln] = 0;
        while (*p == '/') p++;
        last = !*p;
        if (!ln || snprintf(next, sizeof next, "%s/%s", cur, part) >= (int)sizeof next) return -1;
        c = vfs_child(parent, part);
        if (!last) {
            if (!c) {
                if (!vfs_mkdir(next) || !(c = pkg_real_dir(next))) return -1;
            }
            if (!c->dir) return -1;
            parent = c;
            snprintf(cur, sizeof cur, "%s", next);
            continue;
        }
        if (kind == 1) {
            if (c && !c->dir) return -1;
            if (!c && !vfs_mkdir(next)) return -1;
            return pkg_real_dir(next) ? 0 : -1;
        }
        if (c && c->dir) return -1;
        if (n > 512ull * 1024 * 1024) return -1;
        return vfs_write_file(next, data ? data : "", (usize)n) ? 0 : -1;
    }
    return -1;
}

static void install_pkg_bytes(const u8 *data, u64 n) {
    pkg_out_t o;
    int r;
    char msg[240];
    memset(&o, 0, sizeof o);
    o.write = pkg_vfs_write;
    o.ctx = (void *)"/Aplicativos";
    r = pkg_extract(data, n, &o);
    if (r != 0) {
        snprintf(msg, sizeof msg, "%s", o.reason[0] ? o.reason : "pacote malformado");
        notify("Pacote recusado", msg);
        kprintf("pkg: recusado (%s)\n", msg);
        return;
    }
    kprintf("pkg: %d arquivo(s) em /Aplicativos; %d script(s) não executado(s)\n", o.files, o.scripts_skipped);
    notify("Pacote instalado", "Os apps foram para Aplicativos. Scripts de pré e pós-instalação não foram executados.");
    ls_rescan();
}

static void install_pkg_tree(vnode_t *dir) {
    int any = 0;
    vnode_t *pack;
    char sub[512];
    if (!dir) return;
    for (vnode_t *c = dir->child; c; c = c->next) {
        if (c->dir || !c->data || !ends_pkg(c->name)) continue;
        install_pkg_bytes(c->data, c->size);
        any = 1;
    }
    pack = vfs_child(dir, "Contents");
    pack = pack ? vfs_child(pack, "Packages") : NULL;
    if (pack && pack->dir) {
        for (vnode_t *c = pack->child; c; c = c->next) {
            if (c->dir || !c->data || !ends_pkg(c->name)) continue;
            install_pkg_bytes(c->data, c->size);
            any = 1;
        }
    }
    if (!any) {
        snprintf(sub, sizeof sub, "%s", dir->name);
        notify("Pacote recusado", "O .mpkg não tem um xar com Payload. Nada foi copiado.");
        kprintf("pkg: %s sem xar\n", sub);
    }
}

static void install_apps_in(const char *dir) {
    vnode_t *d = vfs_lookup(dir);
    char path[512];
    if (!d || !d->dir) return;
    for (vnode_t *c = d->child; c; c = c->next) {
        usize n = strlen(c->name);
        if (!c->dir || n < 5 || strcasecmp(c->name + n - 4, ".app")) continue;
        snprintf(path, sizeof path, "%s/%s", dir, c->name);
        ls_install_app(path);
    }
}

void diskimage_open_file(const char *path) {
    vnode_t *n = vfs_lookup(path);
    if (!n) return;
    char msg[300];
    if (ends_pkg(n->name)) {
        if (n->dir) install_pkg_tree(n);
        else if (n->data) install_pkg_bytes(n->data, n->size);
        else notify("Pacote recusado", "Arquivo vazio. Nada foi copiado.");
        return;
    }
    char before[8][64];
    int nb = 0;
    vnode_t *vols = vfs_lookup("/Volumes");
    if (vols) for (vnode_t *v = vols->child; v && nb < 8; v = v->next) snprintf(before[nb++], 64, "%s", v->name);
    if (vfs_mount_file(n) > 0) {
        snprintf(msg, sizeof msg, "\"%s\" montado", n->name);
        notify(msg, "Disponível em Volumes. Apps novos do volume vão para Aplicativos, menos os da App Store.");
        vols = vfs_lookup("/Volumes");
        if (vols) for (vnode_t *v = vols->child; v; v = v->next) {
            int known = 0;
            char p[400], sub[480];
            for (int i = 0; i < nb; i++) if (!strcmp(before[i], v->name)) known = 1;
            if (known || !v->dir) continue;
            snprintf(p, sizeof p, "/Volumes/%s", v->name);
            install_apps_in(p);
            snprintf(sub, sizeof sub, "%s/Applications", p);
            install_apps_in(sub);
            snprintf(sub, sizeof sub, "%s/Aplicativos", p);
            install_apps_in(sub);
        }
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

/* ================================================================ Comprimir (menu de contexto do Arquivos)
 * Como o Finder: "Comprimir “X”" cria "X.zip" ao lado (ou "X 2.zip"); vários itens viram "Arquivo.zip". Deflate com
 * códigos de Huffman fixos e LZ77 (janela de 32 KiB); o que não encolhe vai armazenado. Links simbólicos do mOS
 * entram como links no zip (modo 0120777), como o ditto faz. */
static u32 crc_tab[256];
static u32 crc32z(const u8 *p, usize n) {
    if (!crc_tab[1]) for (u32 i = 0; i < 256; i++) { u32 c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; crc_tab[i] = c; }
    u32 c = 0xFFFFFFFFu;
    for (usize i = 0; i < n; i++) c = crc_tab[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

typedef struct { u8 *o; usize n, cap; u32 bits; int nb; bool full; } bw_t;
static void bw_put(bw_t *w, u32 v, int n) {          /* n bits de v, do menos significativo */
    w->bits |= v << w->nb;
    w->nb += n;
    while (w->nb >= 8) {
        if (w->n < w->cap) w->o[w->n++] = (u8)w->bits; else w->full = true;
        w->bits >>= 8;
        w->nb -= 8;
    }
}
static void bw_huff(bw_t *w, u32 code, int len) {     /* código de Huffman: do bit mais significativo */
    u32 r = 0;
    for (int i = 0; i < len; i++) r |= ((code >> i) & 1) << (len - 1 - i);
    bw_put(w, r, len);
}
static void put_lit(bw_t *w, u32 v) {
    if (v < 144) bw_huff(w, 0x30 + v, 8);
    else if (v < 256) bw_huff(w, 0x190 + (v - 144), 9);
    else if (v < 280) bw_huff(w, v - 256, 7);
    else bw_huff(w, 0xC0 + (v - 280), 8);
}
static const u16 len_base[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const u8 len_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const u16 dist_base[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const u8 dist_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static u32 hash3(const u8 *p) { return ((u32)p[0] << 10 ^ (u32)p[1] << 5 ^ p[2]) & 0x7FFF; }

/* deflate em out (cap); devolve o tamanho, ou 0 se não coube */
static usize deflate_fixed(const u8 *in, usize n, u8 *out, usize cap) {
    bw_t w = {out, 0, cap, 0, 0, false};
    bw_put(&w, 1, 1);           /* último bloco */
    bw_put(&w, 1, 2);           /* Huffman fixo */
    i32 *head = kmalloc(0x8000 * sizeof(i32));
    i32 *prev = kmalloc((n ? n : 1) * sizeof(i32));
    if (!head || !prev) { kfree(head); kfree(prev); return 0; }
    for (int i = 0; i < 0x8000; i++) head[i] = -1;
    usize i = 0;
    while (i < n && !w.full) {
        int best = 0, bdist = 0;
        if (i + 3 <= n) {
            u32 h = hash3(in + i);
            i32 cand = head[h];
            for (int tries = 0; cand >= 0 && i - (usize)cand <= 32768 && tries < 32; tries++, cand = prev[cand]) {
                usize lim = MIN((usize)258, n - i), l = 0;
                while (l < lim && in[cand + l] == in[i + l]) l++;
                if ((int)l > best) { best = (int)l; bdist = (int)(i - (usize)cand); if (l == lim) break; }
            }
            prev[i] = head[h];
            head[h] = (i32)i;
        }
        if (best >= 3) {
            int c = 0;
            while (c < 28 && len_base[c + 1] <= best) c++;
            put_lit(&w, 257 + (u32)c);
            if (len_extra[c]) bw_put(&w, (u32)(best - len_base[c]), len_extra[c]);
            int d = 0;
            while (d < 29 && dist_base[d + 1] <= bdist) d++;
            bw_huff(&w, (u32)d, 5);
            if (dist_extra[d]) bw_put(&w, (u32)(bdist - dist_base[d]), dist_extra[d]);
            for (usize k = 1; k < (usize)best && i + k + 3 <= n; k++) {   /* as posições copiadas entram no dicionário */
                u32 h = hash3(in + i + k);
                prev[i + k] = head[h];
                head[h] = (i32)(i + k);
            }
            i += (usize)best;
        } else {
            put_lit(&w, in[i]);
            i++;
        }
    }
    put_lit(&w, 256);
    if (w.nb) bw_put(&w, 0, 8 - w.nb);
    kfree(head);
    kfree(prev);
    return w.full ? 0 : w.n;
}

typedef struct { u8 *p; usize n, cap; } zbuf_t;
static bool zb_put(zbuf_t *b, const void *d, usize n) {
    if (b->n + n > b->cap) {
        usize nc = b->cap ? b->cap * 2 : 65536;
        while (nc < b->n + n) nc *= 2;
        u8 *q = krealloc(b->p, nc);
        if (!q) return false;
        b->p = q;
        b->cap = nc;
    }
    if (n) memcpy(b->p + b->n, d, n);
    b->n += n;
    return true;
}
static void w16(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static void w32(u8 *p, u32 v) { w16(p, v); w16(p + 2, v >> 16); }

typedef struct { zbuf_t data, cd; u32 count; u16 dtime, ddate; bool fail; } zw_t;

static void zw_add(zw_t *z, const char *name, const u8 *d, usize n, u32 mode, bool dir) {
    usize nl = strlen(name);
    u32 crc = dir ? 0 : crc32z(d, n);
    u8 *cbuf = NULL;
    usize cl = n;
    u16 method = 0;
    if (!dir && n > 64) {
        cbuf = kmalloc(n);
        usize got = cbuf ? deflate_fixed(d, n, cbuf, n - 1) : 0;
        if (got && got < n) { cl = got; method = 8; } else { kfree(cbuf); cbuf = NULL; }
    }
    u32 off = (u32)z->data.n;
    u8 lh[30];
    memset(lh, 0, sizeof lh);
    w32(lh, 0x04034b50); w16(lh + 4, 20); w16(lh + 6, 0x0800); w16(lh + 8, method);   /* 0x0800: nomes em UTF-8 */
    w16(lh + 10, z->dtime); w16(lh + 12, z->ddate); w32(lh + 14, crc); w32(lh + 18, (u32)cl); w32(lh + 22, (u32)n); w16(lh + 26, (u32)nl);
    bool ok = zb_put(&z->data, lh, 30) && zb_put(&z->data, name, nl) && (dir || zb_put(&z->data, method ? cbuf : d, cl));
    u8 ch[46];
    memset(ch, 0, sizeof ch);
    w32(ch, 0x02014b50); w16(ch + 4, 0x0314); w16(ch + 6, 20); w16(ch + 8, 0x0800); w16(ch + 10, method);   /* 0x0314: Unix */
    w16(ch + 12, z->dtime); w16(ch + 14, z->ddate); w32(ch + 16, crc); w32(ch + 20, (u32)cl); w32(ch + 24, (u32)n); w16(ch + 28, (u32)nl);
    w32(ch + 38, mode << 16 | (dir ? 0x10 : 0)); w32(ch + 42, off);
    ok = ok && zb_put(&z->cd, ch, 46) && zb_put(&z->cd, name, nl);
    kfree(cbuf);
    if (!ok) z->fail = true;
    z->count++;
}

static void zw_tree(zw_t *z, vnode_t *n, const char *rel) {
    if (!n || z->fail || z->count > 60000) return;
    vfs_ensure(n);
    char name[1024];
    if (n->dir) {
        snprintf(name, sizeof name, "%s/", rel);
        zw_add(z, name, NULL, 0, 040755, true);
        for (vnode_t *c = n->child; c; c = c->next) {
            if (!strcmp(c->name, ".DS_Store")) continue;
            snprintf(name, sizeof name, "%s/%s", rel, c->name);
            zw_tree(z, c, name);
        }
        return;
    }
    static const char lk[] = "#mOS-link:";
    usize ll = sizeof lk - 1;
    if (n->data && n->size > ll && !memcmp(n->data, lk, ll)) zw_add(z, rel, n->data + ll, n->size - ll, 0120777, false);
    else zw_add(z, rel, n->data ? n->data : (const u8 *)"", n->data ? n->size : 0, 0100644, false);
}

/* paths: itens da mesma pasta. true e o caminho do .zip criado em out */
bool archive_compress(const char *const *paths, int npaths, char *out, usize cap) {
    if (npaths <= 0) return false;
    char dir[400];
    snprintf(dir, sizeof dir, "%s", paths[0]);
    char *sl = strrchr(dir, '/');
    if (sl && sl != dir) *sl = 0; else snprintf(dir, sizeof dir, "/");
    if (vfs_is_read_only(vfs_lookup(dir))) snprintf(dir, sizeof dir, "%s", home_sub("/Downloads"));
    zw_t z;
    memset(&z, 0, sizeof z);
    datetime_t dt;
    rtc_now(&dt);
    z.dtime = (u16)(dt.hour << 11 | dt.minute << 5 | dt.second / 2);
    z.ddate = (u16)((dt.year > 1980 ? dt.year - 1980 : 0) << 9 | dt.month << 5 | dt.day);
    for (int i = 0; i < npaths; i++) {
        const char *b = strrchr(paths[i], '/');
        zw_tree(&z, vfs_lookup(paths[i]), b ? b + 1 : paths[i]);
    }
    if (z.fail || !z.count) {
        kfree(z.data.p); kfree(z.cd.p);
        notify("Não foi possível comprimir", "Falta memória ou o item não existe.");
        return false;
    }
    u8 eocd[22];
    memset(eocd, 0, sizeof eocd);
    u32 cnt = z.count > 0xFFFF ? 0xFFFF : z.count;
    w32(eocd, 0x06054b50); w16(eocd + 8, cnt); w16(eocd + 10, cnt); w32(eocd + 12, (u32)z.cd.n); w32(eocd + 16, (u32)z.data.n);
    bool ok = zb_put(&z.data, z.cd.p, z.cd.n) && zb_put(&z.data, eocd, 22);
    char name[260];
    if (npaths == 1) { const char *b = strrchr(paths[0], '/'); snprintf(name, sizeof name, "%s.zip", b ? b + 1 : paths[0]); }
    else snprintf(name, sizeof name, "Arquivo.zip");
    unique_path(dir, name, out, cap);
    if (ok) ok = vfs_write_file(out, z.data.p, z.data.n) != NULL;
    kprintf("compactacao: %d item(ns) -> %s (%llu bytes)%s\n", npaths, out, (u64)z.data.n, ok ? "" : " FALHOU");
    kfree(z.data.p);
    kfree(z.cd.p);
    return ok;
}

/* ================================================================ .tar, .gz, .tgz, .bz2, .xz (como o Utilitário de
 * Compactação do macOS): o compressor é tirado e, se dentro houver um tar, ele é expandido na mesma operação
 * ("X.tar.gz" vira a pasta "X"). Um .gz/.bz2/.xz sem tar vira o arquivo sem a extensão ("notas.txt.gz" → "notas.txt").
 * .7z e .rar não são do sistema: no macOS também precisam de um app (Keka, The Unarchiver). */
static u64 oct(const char *s, int n) {
    u64 v = 0;
    for (int i = 0; i < n && s[i]; i++) if (s[i] >= '0' && s[i] <= '7') v = v * 8 + (u64)(s[i] - '0');
    return v;
}
static bool is_tar(const u8 *d, usize n) { return n >= 512 && !memcmp(d + 257, "ustar", 5); }

/* descompressão de tamanho desconhecido: tenta buffers maiores até caber (limite de 1 GiB) */
static u8 *grow_decompress(i64 (*fn)(const u8 *, u64, u8 *, u64), const u8 *src, usize n, usize *out) {
    for (u64 cap = MAX((u64)n * 6, (u64)1 << 20); cap <= (1ull << 30); cap *= 4) {
        u8 *b = kmalloc(cap);
        if (!b) return NULL;
        i64 r = fn(src, n, b, cap);
        if (r >= 0 && (u64)r < cap) { *out = (usize)r; return b; }
        kfree(b);
        if (r < 0 && cap > (u64)n * 200) return NULL;   /* erro de verdade, não falta de espaço */
    }
    return NULL;
}
static i64 bz_fn(const u8 *s, u64 n, u8 *d, u64 c) { return bzip2_decompress(s, n, d, c); }
static i64 xz_fn(const u8 *s, u64 n, u8 *d, u64 c) { return xz_decompress(s, n, d, c); }

/* gzip (RFC 1952): cabeçalho, deflate, CRC e tamanho no fim; vários membros concatenados também valem */
static u8 *gunzip(const u8 *d, usize n, usize *out, char *orig, usize ocap) {
    if (n < 18 || d[0] != 0x1f || d[1] != 0x8b || d[2] != 8) return NULL;
    u8 flg = d[3];
    usize p = 10;
    if (flg & 4) { if (p + 2 > n) return NULL; p += 2 + (usize)(d[p] | d[p + 1] << 8); }
    if (flg & 8) {               /* FNAME: o nome original */
        usize s = p;
        while (p < n && d[p]) p++;
        if (orig && ocap) snprintf(orig, ocap, "%.*s", (int)MIN(p - s, ocap - 1), (const char *)d + s);
        p++;
    }
    if (flg & 16) { while (p < n && d[p]) p++; p++; }
    if (flg & 2) p += 2;
    if (p >= n) return NULL;
    u32 isize = (u32)d[n - 4] | (u32)d[n - 3] << 8 | (u32)d[n - 2] << 16 | (u32)d[n - 1] << 24;
    usize cap = isize ? isize : n * 8;
    for (int tries = 0; tries < 6; tries++, cap *= 4) {       /* ISIZE é módulo 2^32 (e de um membro só) */
        u8 *b = kmalloc(cap + 1);
        if (!b) return NULL;
        i64 r = zlib_decompress(d + p, n - p - 8, b, cap);
        if (r >= 0 && ((u64)r < cap || (u64)r == isize)) { *out = (usize)r; return b; }
        kfree(b);
    }
    return NULL;
}

/* nome de uma entrada do tar: prefixo do ustar + nome, ou o do pax/GNU que veio antes */
static void tar_name(const u8 *h, const char *longname, char *out, usize cap) {
    if (longname && longname[0]) { snprintf(out, cap, "%s", longname); return; }
    char nm[101], pf[156];
    memcpy(nm, h, 100); nm[100] = 0;
    memcpy(pf, h + 345, 155); pf[155] = 0;
    if (pf[0] && !memcmp(h + 257, "ustar", 5)) snprintf(out, cap, "%s/%s", pf, nm);
    else snprintf(out, cap, "%s", nm);
}
/* "27 path=nome/do/arquivo\n" de um cabeçalho pax */
static void pax_get(const u8 *d, usize n, const char *key, char *out, usize cap) {
    usize kl = strlen(key);
    for (usize p = 0; p < n;) {
        usize len = 0, q = p;
        while (q < n && d[q] >= '0' && d[q] <= '9') len = len * 10 + (usize)(d[q++] - '0');
        if (!len || q >= n || d[q] != ' ') break;
        q++;
        if (q + kl + 1 <= p + len && !memcmp(d + q, key, kl) && d[q + kl] == '=') {
            usize vs = q + kl + 1, ve = p + len - 1;
            snprintf(out, cap, "%.*s", (int)MIN(ve - vs, cap - 1), (const char *)d + vs);
        }
        p += len;
    }
}

typedef struct { const char *name; const u8 *data; usize size; char type; char link[300]; } tent_t;
/* chama fn para cada entrada; devolve o número de entradas */
static int tar_walk(const u8 *d, usize n, void (*fn)(tent_t *e, void *ctx), void *ctx) {
    char longname[1024] = "", longlink[300] = "";
    int count = 0;
    for (usize p = 0; p + 512 <= n;) {
        const u8 *h = d + p;
        bool zero = true;
        for (int i = 0; i < 512 && zero; i++) if (h[i]) zero = false;
        if (zero) break;
        u64 sz = oct((const char *)h + 124, 12);
        char tf = (char)h[156];
        const u8 *data = h + 512;
        if (p + 512 + sz > n) break;
        usize next = p + 512 + (usize)((sz + 511) & ~511ull);
        if (tf == 'x') { pax_get(data, (usize)sz, "path", longname, sizeof longname); pax_get(data, (usize)sz, "linkpath", longlink, sizeof longlink); p = next; continue; }
        if (tf == 'g') { p = next; continue; }
        if (tf == 'L') { snprintf(longname, sizeof longname, "%.*s", (int)MIN(sz, sizeof longname - 1), (const char *)data); p = next; continue; }
        if (tf == 'K') { snprintf(longlink, sizeof longlink, "%.*s", (int)MIN(sz, sizeof longlink - 1), (const char *)data); p = next; continue; }
        char name[1100];
        tar_name(h, longname, name, sizeof name);
        tent_t e = {name, data, (usize)sz, tf ? tf : '0', ""};
        if (longlink[0]) snprintf(e.link, sizeof e.link, "%s", longlink);
        else { memcpy(e.link, h + 157, 100); e.link[100] = 0; }
        if (!(name[0] == '.' && name[1] == '/' && !name[2]) && strcmp(name, ".")) { fn(&e, ctx); count++; }
        longname[0] = longlink[0] = 0;
        p = next;
    }
    return count;
}

typedef struct { char top[256]; bool single; bool any; char dest[400]; char strip[256]; int files, bad; } tctx_t;
static const char *skip_dot(const char *s) { while (s[0] == '.' && s[1] == '/') s += 2; return s; }
static void tar_scan(tent_t *e, void *c) {
    tctx_t *t = c;
    const char *nm = skip_dot(e->name);
    if (!nm[0] || !strncmp(nm, "._", 2) || strstr(nm, "/._")) return;
    usize tl = 0;
    while (nm[tl] && nm[tl] != '/') tl++;
    if (!t->any) { snprintf(t->top, sizeof t->top, "%.*s", (int)tl, nm); t->any = true; return; }
    if (strlen(t->top) != tl || memcmp(t->top, nm, tl)) t->single = false;
}
static void tar_out(tent_t *e, void *c) {
    tctx_t *t = c;
    const char *nm = skip_dot(e->name);
    if (!nm[0] || !strncmp(nm, "._", 2) || strstr(nm, "/._") || strstr(nm, "../")) return;
    usize sl = strlen(t->strip);
    char rel[1100];
    if (sl && !strncmp(nm, t->strip, sl)) snprintf(rel, sizeof rel, "%s", nm + sl); else snprintf(rel, sizeof rel, "/%s", nm);
    char full[1500];
    snprintf(full, sizeof full, "%s%s", t->dest, rel);
    usize fl = strlen(full);
    while (fl > 1 && full[fl - 1] == '/') full[--fl] = 0;
    if (e->type == '5') { vfs_mkdir(full); return; }
    char parent[1500];
    snprintf(parent, sizeof parent, "%s", full);
    char *ps = strrchr(parent, '/');
    if (ps && ps != parent) { *ps = 0; vfs_mkdir(parent); }
    if (e->type == '2') {
        char lk[400];
        int n = snprintf(lk, sizeof lk, "#mOS-link:%s", e->link);
        vfs_write_file(full, lk, (usize)n);
    } else if (e->type == '0' || e->type == '7') {
        if (!vfs_write_file(full, e->data, e->size)) t->bad++;
    } else return;                                  /* dispositivos, hard links: fora */
    t->files++;
}

static void expand_tar(const u8 *d, usize n, const char *dir, const char *stem) {
    tctx_t t;
    memset(&t, 0, sizeof t);
    t.single = true;
    tar_walk(d, n, tar_scan, &t);
    if (!t.any) { notify("O arquivo está vazio", stem); return; }
    if (t.single) {
        unique_path(dir, t.top, t.dest, sizeof t.dest);
        snprintf(t.strip, sizeof t.strip, "%s", t.top);
    } else {
        unique_path(dir, stem, t.dest, sizeof t.dest);
        vfs_mkdir(t.dest);
    }
    tar_walk(d, n, tar_out, &t);
    kprintf("compactacao: tar -> %s (%d itens, %d com erro)\n", t.dest, t.files, t.bad);
}

/* nome sem a(s) extensão(ões) de compactação: "a.tar.gz" → "a", "a.tgz" → "a", "notas.txt.gz" → "notas.txt" */
static void strip_ext(const char *name, char *out, usize cap) {
    snprintf(out, cap, "%s", name);
    static const char *const exts[] = {".tar.gz", ".tar.bz2", ".tar.xz", ".tgz", ".tbz", ".tbz2", ".txz", ".tar", ".gz", ".gzip", ".bz2", ".xz", ".zip"};
    usize l = strlen(out);
    for (u32 i = 0; i < ARRAY_LEN(exts); i++) {
        usize el = strlen(exts[i]);
        if (l > el && !strcasecmp(out + l - el, exts[i])) { out[l - el] = 0; return; }
    }
}

void archive_open_file(const char *path) {
    vnode_t *f = vfs_lookup(path);
    if (!f || f->dir) return;
    vfs_ensure(f);
    if (!f->data || f->size < 4) { notify("Não foi possível expandir", f->name); return; }
    const u8 *d = f->data;
    usize n = f->size;
    if (d[0] == 'P' && d[1] == 'K') { zip_expand(path); return; }
    char dir[300], stem[256], gzname[256] = "";
    snprintf(dir, sizeof dir, "%s", path);
    char *sl = strrchr(dir, '/');
    if (sl && sl != dir) *sl = 0; else snprintf(dir, sizeof dir, "/");
    if (vfs_is_read_only(vfs_lookup(dir))) snprintf(dir, sizeof dir, "%s", home_sub("/Downloads"));
    strip_ext(f->name, stem, sizeof stem);
    u8 *buf = NULL;
    usize bl = 0;
    if (d[0] == 0x1f && d[1] == 0x8b) buf = gunzip(d, n, &bl, gzname, sizeof gzname);
    else if (d[0] == 'B' && d[1] == 'Z' && d[2] == 'h') buf = grow_decompress(bz_fn, d, n, &bl);
    else if (n > 6 && !memcmp(d, "\xFD" "7zXZ\0", 6)) buf = grow_decompress(xz_fn, d, n, &bl);
    else if (is_tar(d, n)) { expand_tar(d, n, dir, stem); return; }
    else { notify("Não foi possível expandir", "O Utilitário de Compactação não reconhece este formato."); return; }
    if (!buf) { notify("Não foi possível expandir", f->name); return; }
    if (is_tar(buf, bl)) expand_tar(buf, bl, dir, stem);
    else {
        /* só um arquivo compactado: sai com o nome de dentro do .gz, ou sem a extensão */
        char out[512];
        unique_path(dir, gzname[0] && !strchr(gzname, '/') ? gzname : stem, out, sizeof out);
        vfs_write_file(out, buf, bl);
        kprintf("compactacao: %s -> %s (%llu bytes)\n", path, out, (u64)bl);
    }
    kfree(buf);
}
