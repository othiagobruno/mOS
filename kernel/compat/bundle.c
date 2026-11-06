/*
 * mOS — bundles .app do macOS: lê o Info.plist (XML ou binário "bplist00"), acha o executável
 * (Contents/MacOS/<CFBundleExecutable>) e produz o relatório de compatibilidade.
 */
#include "vfs.h"
#include "compat.h"

/* ------------------------------------------------------------ plist XML */
static bool xml_value(const char *x, usize n, const char *key, char *out, usize cap) {
    char pat[160];
    snprintf(pat, sizeof pat, "<key>%s</key>", key);
    usize pl = strlen(pat);
    for (usize i = 0; i + pl <= n; i++) {
        if (memcmp(x + i, pat, pl)) continue;
        usize j = i + pl;
        while (j < n && (x[j] == ' ' || x[j] == '\t' || x[j] == '\r' || x[j] == '\n')) j++;
        if (j + 8 > n || memcmp(x + j, "<string>", 8)) return false;
        j += 8;
        usize o = 0;
        while (j < n && x[j] != '<' && o + 1 < cap) out[o++] = x[j++];
        out[o] = 0;
        return true;
    }
    return false;
}

/* ------------------------------------------------------------ plist binário */
typedef struct { const u8 *b; usize n; u8 offsz, refsz; u64 nobj, top, tbl; } bpl_t;

static u64 bpl_uint(const u8 *p, u8 sz) { u64 v = 0; for (u8 i = 0; i < sz; i++) v = v << 8 | p[i]; return v; }

static bool bpl_obj(bpl_t *bp, u64 idx, const u8 **out) {
    if (idx >= bp->nobj || bp->tbl + (idx + 1) * bp->offsz > bp->n) return false;
    u64 off = bpl_uint(bp->b + bp->tbl + idx * bp->offsz, bp->offsz);
    if (off >= bp->n) return false;
    *out = bp->b + off;
    return true;
}

/* comprimento de objeto (marcador baixo 0xF => inteiro a seguir) */
static bool bpl_len(bpl_t *bp, const u8 **p, u64 *len) {
    u8 m = **p & 0x0F;
    (*p)++;
    if (m != 0x0F) { *len = m; return true; }
    u8 im = **p;
    if ((im & 0xF0) != 0x10) return false;
    u8 sz = (u8)(1u << (im & 0x0F));
    (*p)++;
    if (*p + sz > bp->b + bp->n) return false;
    *len = bpl_uint(*p, sz);
    *p += sz;
    return true;
}

static bool bpl_string(bpl_t *bp, u64 idx, char *out, usize cap) {
    const u8 *p;
    if (!bpl_obj(bp, idx, &p)) return false;
    u8 type = *p >> 4;
    u64 len;
    if ((type != 5 && type != 6) || !bpl_len(bp, &p, &len)) return false;
    usize o = 0;
    if (type == 5) {
        for (u64 i = 0; i < len && o + 1 < cap && p + i < bp->b + bp->n; i++) out[o++] = (char)p[i];
    } else {
        for (u64 i = 0; i < len && p + i * 2 + 1 < bp->b + bp->n; i++) {
            u32 c = (u32)p[i * 2] << 8 | p[i * 2 + 1];
            if (c < 0x80 && o + 1 < cap) out[o++] = (char)c;
            else if (c < 0x800 && o + 2 < cap) { out[o++] = (char)(0xC0 | c >> 6); out[o++] = (char)(0x80 | (c & 0x3F)); }
            else if (o + 3 < cap) { out[o++] = (char)(0xE0 | c >> 12); out[o++] = (char)(0x80 | ((c >> 6) & 0x3F)); out[o++] = (char)(0x80 | (c & 0x3F)); }
        }
    }
    out[o] = 0;
    return true;
}

static bool bpl_value(const u8 *b, usize n, const char *key, char *out, usize cap) {
    if (n < 40 || memcmp(b, "bplist00", 8)) return false;
    const u8 *t = b + n - 32;
    bpl_t bp = {b, n, t[6], t[7], bpl_uint(t + 8, 8), bpl_uint(t + 16, 8), bpl_uint(t + 24, 8)};
    if (!bp.offsz || !bp.refsz || bp.offsz > 8 || bp.refsz > 8) return false;
    const u8 *p;
    if (!bpl_obj(&bp, bp.top, &p) || (*p >> 4) != 0xD) return false;
    u64 cnt;
    if (!bpl_len(&bp, &p, &cnt) || p + cnt * 2 * bp.refsz > b + n) return false;
    for (u64 i = 0; i < cnt; i++) {
        char k[160];
        u64 kref = bpl_uint(p + i * bp.refsz, bp.refsz), vref = bpl_uint(p + (cnt + i) * bp.refsz, bp.refsz);
        if (bpl_string(&bp, kref, k, sizeof k) && !strcmp(k, key)) return bpl_string(&bp, vref, out, cap);
    }
    return false;
}

static bool plist_value(const u8 *b, usize n, const char *key, char *out, usize cap) {
    out[0] = 0;
    if (n >= 8 && !memcmp(b, "bplist00", 8)) return bpl_value(b, n, key, out, cap);
    return xml_value((const char *)b, n, key, out, cap);
}

/* ------------------------------------------------------------ bundles */
static bool is_app(const char *path) {
    usize n = strlen(path);
    while (n && path[n - 1] == '/') n--;
    return n > 4 && !strncasecmp(path + n - 4, ".app", 4);
}

bool macho_bundle_info(const char *app, bundle_info_t *bi) {
    memset(bi, 0, sizeof *bi);
    char p[512];
    snprintf(p, sizeof p, "%s/Contents/Info.plist", app);
    vnode_t *v = vfs_lookup(p);
    if (!v || v->dir || !v->data) return false;
    /* o nome que o macOS mostra (ex.: "Google Chrome"); CFBundleName é o curto ("Chrome") */
    plist_value(v->data, v->size, "CFBundleDisplayName", bi->name, sizeof bi->name);
    if (!bi->name[0]) plist_value(v->data, v->size, "CFBundleName", bi->name, sizeof bi->name);
    plist_value(v->data, v->size, "CFBundleIdentifier", bi->identifier, sizeof bi->identifier);
    plist_value(v->data, v->size, "CFBundleShortVersionString", bi->version, sizeof bi->version);
    plist_value(v->data, v->size, "CFBundleExecutable", bi->executable, sizeof bi->executable);
    plist_value(v->data, v->size, "LSMinimumSystemVersion", bi->min_os, sizeof bi->min_os);
    plist_value(v->data, v->size, "CFBundleIconFile", bi->icon, sizeof bi->icon);
    return true;
}

/* ------------------------------------------------------------ ícone (.icns) */
extern surface_t *image_decode(const u8 *d, usize n);

/* lado em pixels das entradas PNG de um .icns (as outras são formatos antigos ou JPEG 2000) */
static int icns_px(const u8 *t) {
    static const struct { char t[5]; int px; } k[] = {
        {"icp4", 16}, {"icp5", 32}, {"icp6", 64}, {"ic07", 128}, {"ic08", 256}, {"ic09", 512},
        {"ic10", 1024}, {"ic11", 32}, {"ic12", 64}, {"ic13", 256}, {"ic14", 512},
    };
    for (u32 i = 0; i < ARRAY_LEN(k); i++) if (!memcmp(t, k[i].t, 4)) return k[i].px;
    return 0;
}

static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

static struct { char path[300]; int size; surface_t *s; } icon_cache[16];
static int icon_next;

surface_t *macho_bundle_icon(const char *app, int size) {
    if (size < 8 || size > 1024) return NULL;
    for (u32 i = 0; i < ARRAY_LEN(icon_cache); i++)
        if (icon_cache[i].s && icon_cache[i].size == size && !strcmp(icon_cache[i].path, app)) return icon_cache[i].s;
    bundle_info_t bi;
    if (!macho_bundle_info(app, &bi)) return NULL;
    char file[160], p[512];
    snprintf(file, sizeof file, "%s", bi.icon[0] ? bi.icon : "AppIcon");
    usize fl = strlen(file);
    if (fl < 5 || strcasecmp(file + fl - 5, ".icns")) snprintf(file + fl, sizeof file - fl, ".icns");
    snprintf(p, sizeof p, "%s/Contents/Resources/%s", app, file);
    vnode_t *v = vfs_lookup(p);
    if (!v || v->dir || !v->data || v->size < 16 || memcmp(v->data, "icns", 4)) return NULL;
    /* a menor entrada PNG que cubra o tamanho pedido (ou a maior que houver) */
    const u8 *best = NULL;
    u32 best_len = 0;
    int best_px = 0;
    for (usize o = 8; o + 8 <= v->size;) {
        u32 len = be32(v->data + o + 4);
        if (len < 8 || len > v->size - o) break;
        int px = icns_px(v->data + o);
        if (px && len > 16 && !memcmp(v->data + o + 8, "\x89PNG", 4)) {
            bool better = !best || (best_px < size ? px > best_px : (px >= size && px < best_px));
            if (better) { best = v->data + o + 8; best_len = len - 8; best_px = px; }
        }
        o += len;
    }
    if (!best) return NULL;
    surface_t *img = image_decode(best, best_len);
    if (!img) return NULL;
    surface_t *s = surface_new(size, size);
    if (!s) { surface_free(img); return NULL; }
    memset(s->px, 0, (usize)s->stride * size * 4);
    gfx_blit_scaled(s, img, 0, 0, size, size, 255);
    surface_free(img);
    int slot = icon_next++ % (int)ARRAY_LEN(icon_cache);
    if (icon_cache[slot].s) surface_free(icon_cache[slot].s);
    snprintf(icon_cache[slot].path, sizeof icon_cache[slot].path, "%s", app);
    icon_cache[slot].size = size;
    icon_cache[slot].s = s;
    return s;
}

bool macho_bundle_exec(const char *app, char *out, usize cap) {
    bundle_info_t bi;
    char exe[160];
    if (macho_bundle_info(app, &bi) && bi.executable[0]) snprintf(exe, sizeof exe, "%s", bi.executable);
    else {
        const char *b = strrchr(app, '/');
        snprintf(exe, sizeof exe, "%s", b ? b + 1 : app);
        char *dot = strrchr(exe, '.');
        if (dot) *dot = 0;
    }
    snprintf(out, cap, "%s/Contents/MacOS/%s", app, exe);
    vnode_t *v = vfs_lookup(out);
    return v && !v->dir;
}

bool macho_report(const char *path, char *out, usize cap) {
    char exe[512];
    usize o = 0;
    const char *target = path;
    if (is_app(path)) {
        bundle_info_t bi;
        if (macho_bundle_info(path, &bi))
            o += (usize)snprintf(out + o, cap - o, "App: %s %s (%s)\nmacOS mínimo pedido: %s\n",
                                 bi.name[0] ? bi.name : "?", bi.version, bi.identifier[0] ? bi.identifier : "sem identificador",
                                 bi.min_os[0] ? bi.min_os : "-");
        if (!macho_bundle_exec(path, exe, sizeof exe)) {
            snprintf(out + o, cap - o, "Executável do bundle não encontrado (%s)\n", exe);
            return false;
        }
        target = exe;
    }
    vnode_t *v = vfs_lookup(target);
    if (!v || v->dir || !v->data || !v->size) { snprintf(out + o, cap - o, "%s: arquivo não encontrado\n", target); return false; }
    return macho_report_image(v->data, v->size, target, out + o, cap - o);
}

bool macho_is_macho(const u8 *d, usize n) {
    if (!d || n < 8) return false;
    u32 le = (u32)d[0] | (u32)d[1] << 8 | (u32)d[2] << 16 | (u32)d[3] << 24;
    return le == 0xFEEDFACFu || le == 0xBEBAFECAu || le == 0xBFBAFECAu;
}

int macho_spawn(const char *path, const char *args, const char *cwd, char *err, usize errlen) {
    char exe[512];
    const char *target = path;
    err[0] = 0;
    if (is_app(path)) {
        if (!macho_bundle_exec(path, exe, sizeof exe)) { snprintf(err, errlen, "o bundle não tem executável (%s)", exe); return -1; }
        target = exe;
    }
    vnode_t *v = vfs_lookup(target);
    if (!v || v->dir || !v->data) { snprintf(err, errlen, "%s: arquivo não encontrado", target); return -1; }
    if (!macho_is_macho(v->data, v->size)) { snprintf(err, errlen, "%s não é um executável do macOS", target); return -1; }
    /* checa antes para dar um motivo legível (o proc_spawn só registra no log) */
    static char rep[4096];
    if (!macho_report_image(v->data, v->size, target, rep, sizeof rep)) {
        const char *ver = strstr(rep, "Veredito: ");
        snprintf(err, errlen, "%s", ver ? ver + 10 : "executável incompatível");
        usize n = strlen(err);
        if (n && err[n - 1] == '\n') err[n - 1] = 0;
        return -1;
    }
    macho_set_cwd(cwd);
    int pid = proc_spawn(target, args);
    if (pid < 0) snprintf(err, errlen, "o carregador recusou %s (veja o log)", target);
    return pid;
}
