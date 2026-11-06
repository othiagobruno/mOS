/*
 * mOS libSystem — ponte C de IMAGEM para o SwiftUI (OpenSwiftUI) do mOS.
 * Declarações em userland/sdk/swift/MOSBridge/MOSBridge_image.h.
 *
 * Image("nome") procura o arquivo no diretório de recursos do bundle e no catálogo de assets
 * (pasta Assets.xcassets/<nome>.imageset copiada para Resources). A imagem decodificada (NSImage
 * do AppKit do mOS) fica num cache, para não reler o disco a cada quadro.
 */
#include "foundation.h"
#include "libc.h"

typedef struct { double x, y; } ipt_t;
typedef struct { double w, h; } isz_t;
typedef struct { ipt_t o; isz_t s; } irc_t;
extern id objc_retain(id);
extern void objc_release(id);
extern void cx_objc_boot(u64 sects, u64 n);
static id icls(const char *n) { cx_objc_boot(0, 0); return (id)cx_class_named(n); }

/* ---- cache (nome + diretório → NSImage); guarda também as falhas */
#define IMG_CACHE 96
typedef struct { char key[640]; id img; double w, h, scale; int used; } img_ent_t;
static img_ent_t img_cache[IMG_CACHE];
static int img_next;

static int has_suffix(const char *s, const char *suf) {
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && strcasecmp(s + a - b, suf) == 0;
}
static int is_image_file(const char *f) {
    return has_suffix(f, ".png") || has_suffix(f, ".jpg") || has_suffix(f, ".jpeg") || has_suffix(f, ".gif") || has_suffix(f, ".bmp");
}
/* escala pelo sufixo @2x/@3x do nome do arquivo */
static double scale_of(const char *f) {
    if (strstr(f, "@3x")) return 3;
    if (strstr(f, "@2x")) return 2;
    return 1;
}
static id load_file(const char *path) {
    if (access(path, 0) != 0) return 0;
    id p = cx_str(path);
    id o = MSG(id, MSG0(id, icls("NSImage"), "alloc"), "initWithContentsOfFile:", p);
    objc_release(p);
    return o;
}
/* arquivo direto no diretório: nome, nome.ext, nome@2x.ext, nome@3x.ext */
static id probe_dir(const char *dir, const char *name, double *scale) {
    static const char *exts[] = {"", ".png", ".jpg", ".jpeg", ".gif", ".bmp"};
    static const char *sufs[] = {"", "@2x", "@3x"};
    char p[600];
    for (u32 s = 0; s < 3; s++)
        for (u32 i = 0; i < sizeof exts / sizeof exts[0]; i++) {
            if (i == 0 && s > 0) continue;
            snprintf(p, sizeof p, "%s/%s%s%s", dir, name, sufs[s], exts[i]);
            if (i == 0 && !is_image_file(p)) continue;    /* sem extensão: só se o nome já tiver uma */
            id o = load_file(p);
            if (o) { *scale = s == 0 ? scale_of(name) : (double)(s + 1); return o; }
        }
    return 0;
}
/* catálogo de assets: <dir>/Assets.xcassets/<nome>.imageset/<arquivo>; prefere o de menor escala */
static id probe_imageset(const char *dir, const char *name, double *scale) {
    static const char *cats[] = {"Assets.xcassets", "Assets", "Images.xcassets"};
    char set[600], best[600];
    for (u32 c = 0; c < sizeof cats / sizeof cats[0]; c++) {
        snprintf(set, sizeof set, "%s/%s/%s.imageset", dir, cats[c], name);
        void *d = opendir(set);
        if (!d) continue;
        double bs = 99;
        best[0] = 0;
        void *e;
        while ((e = readdir(d))) {
            const char *f = (const char *)e + 21;          /* d_name do dirent do Darwin */
            if (!is_image_file(f)) continue;
            double s = scale_of(f);
            if (s < bs) { bs = s; snprintf(best, sizeof best, "%s/%s", set, f); }
        }
        closedir(d);
        if (best[0]) {
            id o = load_file(best);
            if (o) { *scale = bs; return o; }
        }
    }
    return 0;
}
static id lookup(const char *dir, const char *name, double *scale) {
    char sub[600];
    id o = probe_dir(dir, name, scale);
    if (!o) o = probe_imageset(dir, name, scale);
    if (!o) {   /* dir pode ser o bundle (.app): tenta Contents/Resources e Resources */
        snprintf(sub, sizeof sub, "%s/Contents/Resources", dir);
        o = probe_dir(sub, name, scale);
        if (!o) o = probe_imageset(sub, name, scale);
    }
    if (!o) {
        snprintf(sub, sizeof sub, "%s/Resources", dir);
        o = probe_dir(sub, name, scale);
        if (!o) o = probe_imageset(sub, name, scale);
    }
    return o;
}

void *mos_image_lookup(const char *dir, const char *name, double *w, double *h, double *scale) {
    *w = *h = 0; *scale = 1;
    if (!name || !*name) return 0;
    char rdir[512];
    if (dir && *dir) snprintf(rdir, sizeof rdir, "%s", dir);
    else {      /* sem diretório: recursos do bundle principal */
        id res = MSG0(id, MSG0(id, icls("NSBundle"), "mainBundle"), "resourcePath");
        snprintf(rdir, sizeof rdir, "%s", res ? cx_cstr(res) : ".");
    }
    char key[640];
    snprintf(key, sizeof key, "%s|%s", rdir, name);
    for (int i = 0; i < IMG_CACHE; i++)
        if (img_cache[i].used && strcmp(img_cache[i].key, key) == 0) {
            *w = img_cache[i].w; *h = img_cache[i].h; *scale = img_cache[i].scale;
            return img_cache[i].img;
        }
    double sc = 1;
    id img = lookup(rdir, name, &sc);
    img_ent_t *e = &img_cache[img_next];
    img_next = (img_next + 1) % IMG_CACHE;
    if (e->used && e->img) objc_release(e->img);
    memset(e, 0, sizeof *e);
    snprintf(e->key, sizeof e->key, "%s", key);
    e->used = 1;
    e->img = img;
    e->scale = sc > 0 ? sc : 1;
    if (img) { isz_t z = MSG0(isz_t, img, "size"); e->w = z.w; e->h = z.h; }
    *w = e->w; *h = e->h; *scale = e->scale;
    return img;
}

void mos_image_draw(void *img, double x, double y, double w, double h, double alpha) {
    if (!img || w <= 0 || h <= 0 || alpha <= 0) return;
    irc_t r = {{x, y}, {w, h}}, from = {{0, 0}, {0, 0}};
    MSG(void, (id)img, "drawInRect:fromRect:operation:fraction:", r, from, (u64)2 /* sourceOver */, alpha > 1 ? 1.0 : alpha);
}
