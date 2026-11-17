/*
 * mOS libSystem — catálogos de recursos compilados (Assets.car) no AppKit:
 *   +[NSImage imageNamed:] e -[NSBundle imageForResource:] leem o Assets.car do bundle (imagens em bitmap LZFSE/LZVN/
 *   zlib/sem compressão, dentro de atlas ou sozinhas, e dados PNG/JPEG/PDF); a imagem é do tamanho em pontos da
 *   rendition, na escala 2× quando houver, e na aparência escura quando o sistema está escuro;
 *   +[NSColor colorNamed:] / colorNamed:bundle: devolvem a cor do catálogo (nil se não houver, como no macOS);
 *   o ícone do app sai do catálogo pelo CFBundleIconName (appkit_icons.c chama cx_catalog_image).
 * Leitura em car_core.c (teste userland/tests-appkit/car_host.c), inclusive as comprimidas em deepmap2 (padrão, cru,
 * sem perdas e paleta). O deepmap antigo (compressão 10) segue sem leitura: imageNamed: devolve nil nele.
 * Registro: cx_appkit_catalog_init(), chamado do cx_appkit_input_init (depois do appkit_postico.c e do appkit_icons.c).
 */
#include "foundation.h"
#include "libc.h"
#include "car_core.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
typedef struct { double w, h; } S;
static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
size_t compression_decode_buffer(uint8_t *dst, size_t dn, const uint8_t *src, size_t sn, void *scratch, int algorithm);
void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*release)(void *, const void *, size_t));
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, u32 info, void *prov, const void *dec, _Bool interp, int intent);
void *CGColorSpaceCreateDeviceRGB(void);
void CGColorSpaceRelease(void *s);
void CGDataProviderRelease(void *p);
void CGImageRelease(void *i);

/* ---- descompressão para o car_core */
static long car_dec(int kind, const uint8_t *in, long n, uint8_t *out, long cap) {
    if (kind == 2) {   /* zip: zlib com cabeçalho */
        if (n > 2 && (in[0] & 0x0F) == 8) { in += 2; n -= 2; }
        return (long)compression_decode_buffer(out, (size_t)cap, in, (size_t)n, 0, 0x205);
    }
    if (kind == 3) {   /* LZVN cru (deepmap2): vira um bloco bvxn com o tamanho esperado, que o decodificador LZFSE lê */
        uint8_t *b = malloc((size_t)n + 16);
        if (!b) return 0;
        memcpy(b, "bvxn", 4);
        uint32_t raw = (uint32_t)cap, len = (uint32_t)n;
        memcpy(b + 4, &raw, 4); memcpy(b + 8, &len, 4);
        memcpy(b + 12, in, (size_t)n);
        memcpy(b + 12 + n, "bvx$", 4);
        long got = (long)compression_decode_buffer(out, (size_t)cap, b, (size_t)n + 16, 0, 0x801);
        free(b);
        return got;
    }
    return (long)compression_decode_buffer(out, (size_t)cap, in, (size_t)n, 0, 0x801);   /* LZFSE (e LZVN nos blocos bvxn) */
}

/* ---- um catálogo aberto por bundle */
typedef struct cat { char path[1024]; u8 *data; long n; car_file f; int ok; struct cat *next; } cat_t;
static cat_t *g_cats;
static volatile int g_lock;
static void lock(void) { while (__sync_lock_test_and_set(&g_lock, 1)) { } }
static void unlock(void) { __sync_lock_release(&g_lock); }
static cat_t *catalog_for(id bundle) {
    if (!bundle) bundle = MSG0(id, (id)kls("NSBundle"), "mainBundle");
    id res = bundle ? MSG0(id, bundle, "resourcePath") : 0;
    if (!res) return 0;
    char p[1024];
    snprintf(p, sizeof p, "%s/Assets.car", cx_cstr(res));
    for (cat_t *c = g_cats; c; c = c->next) if (!strcmp(c->path, p)) return c->ok ? c : 0;
    cat_t *c = calloc(1, sizeof *c);
    if (!c) return 0;
    snprintf(c->path, sizeof c->path, "%s", p);
    int fd = open(p, 0);
    if (fd >= 0) {
        long cap = 1 << 20, n = 0;
        u8 *b = malloc((usize)cap);
        for (long r; b && (r = read(fd, b + n, (unsigned long)(cap - n))) > 0;) {
            n += r;
            if (n == cap) { u8 *nb = realloc(b, (usize)cap * 2); if (!nb) { free(b); b = 0; break; } b = nb; cap *= 2; }
        }
        close(fd);
        if (b && car_open(&c->f, b, n)) { c->data = b; c->n = n; c->ok = 1; }
        else free(b);
    }
    c->next = g_cats;
    g_cats = c;
    return c->ok ? c : 0;
}
static int dark_mode(void) {
    extern id NSApp;
    id ap = NSApp && cx_responds(object_getClass(NSApp), sl("effectiveAppearance")) ? MSG0(id, NSApp, "effectiveAppearance") : 0;
    id name = ap ? MSG0(id, ap, "name") : 0;
    return name && cx_is_str(name) && strstr(cx_cstr(name), "Dark") != 0;
}

/* ---- imagens */
static uint8_t *g_scratch;
static void release_px(void *info, const void *data, size_t size) { (void)info; (void)size; free((void *)data); }
static id image_from_rendition(cat_t *c, const car_rendition *r, const uint16_t *key, int nkey) {
    if (r->kind == CAR_DATA && r->data && r->data_len > 0) {
        id d = MSG(id, (id)kls("NSData"), "dataWithBytes:length:", (const void *)r->data, (u64)r->data_len);
        id img = MSG(id, MSG0(id, (id)kls("NSImage"), "alloc"), "initWithData:", d);
        return img ? objc_autorelease(img) : 0;
    }
    if (r->kind != CAR_BITMAP || r->width <= 0 || r->height <= 0 || r->width > 8192 || r->height > 8192) return 0;
    usize npx = (usize)r->width * (usize)r->height;
    u32 *px = malloc(npx * 4);
    if (!px) return 0;
    lock();
    if (!g_scratch) g_scratch = malloc(16u << 20);
    int ok = 0;
    if (g_scratch) { car_set_scratch(g_scratch, 16l << 20); ok = car_bitmap_argb(&c->f, key, nkey, car_dec, px, r->width, r->height); }
    unlock();
    if (!ok) { free(px); return 0; }
    /* ARGB direto em u32 little-endian: AlphaFirst (não pré-multiplicado) + ByteOrder32Little */
    void *space = CGColorSpaceCreateDeviceRGB();
    void *prov = CGDataProviderCreateWithData(0, px, npx * 4, release_px);
    void *cg = prov ? CGImageCreate((size_t)r->width, (size_t)r->height, 8, 32, (size_t)r->width * 4, space, 4u | (2u << 12), prov, 0, 1, 0) : 0;
    if (prov) CGDataProviderRelease(prov);
    CGColorSpaceRelease(space);
    if (!cg) return 0;
    double scale = r->scale > 0 ? r->scale / 100.0 : 1;
    S size = {r->width / scale, r->height / scale};
    id img = ((id (*)(id, SEL, void *, S))objc_msgSend)(MSG0(id, (id)kls("NSImage"), "alloc"), sl("initWithCGImage:size:"), cg, size);
    CGImageRelease(cg);
    return img ? objc_autorelease(img) : 0;
}
/* imagem do catálogo do bundle (nil sem catálogo, sem o nome ou com compressão não lida) */
id cx_catalog_image(id bundle, id name) {
    if (!name || !cx_is_str(name)) return 0;
    cat_t *c = catalog_for(bundle);
    if (!c) return 0;
    car_rendition r;
    uint16_t key[32];
    int nk = 0;
    lock();
    int found = car_find(&c->f, cx_cstr(name), 200, dark_mode(), &r, key, &nk);
    unlock();
    if (!found || r.kind == CAR_COLOR) return 0;
    return image_from_rendition(c, &r, key, nk);
}
/* ---- cores */
static id catalog_color(id bundle, id name) {
    if (!name || !cx_is_str(name)) return 0;
    cat_t *c = catalog_for(bundle);
    if (!c) return 0;
    car_rendition r;
    uint16_t key[32];
    int nk = 0;
    lock();
    int found = car_find(&c->f, cx_cstr(name), 0, dark_mode(), &r, key, &nk);
    unlock();
    if (!found || r.kind != CAR_COLOR || r.ncolor < 1) return 0;
    if (r.ncolor <= 2) return MSG(id, (id)kls("NSColor"), "colorWithWhite:alpha:", r.color[0], r.ncolor == 2 ? r.color[1] : 1.0);
    return MSG(id, (id)kls("NSColor"), "colorWithSRGBRed:green:blue:alpha:", r.color[0], r.color[1], r.color[2], r.ncolor >= 4 ? r.color[3] : 1.0);
}
static id color_named(id cls, SEL c, id name) { (void)cls; (void)c; return catalog_color(0, name); }
static id color_named_bundle(id cls, SEL c, id name, id bundle) { (void)cls; (void)c; return catalog_color(bundle, name); }

void cx_appkit_catalog_init(void) {
    cx_class_t *k = kls("NSColor");
    if (k) {
        cx_add_cmethod(k, "colorNamed:", (void *)color_named);
        cx_add_cmethod(k, "colorNamed:bundle:", (void *)color_named_bundle);
    }
}
