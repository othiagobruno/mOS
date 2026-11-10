/*
 * Photos no libSystem. A biblioteca é a pasta Imagens do usuário (a mesma do app Fotos).
 * MOS_PHOTOS_LIBRARY troca essa pasta. Autorização é o TCC kTCCServicePhotos.
 * Sem concessão, ou com a pasta vazia/ausente, o fetch devolve PHFetchResult com count 0.
 * Se o Photos.framework estiver no disco, a classe de lá ganha (cx_mark_stub_class).
 */
#include "foundation.h"
#include "libc.h"
#define PHOTOS_IN_LIBSYSTEM 1
#include "../frameworks/Photos/photos_index.c"

void cx_mark_stub_class(cx_class_t *c);
id NSHomeDirectory(void);

extern cx_class_t PHPhotoLibrary_c __asm__("OBJC_CLASS_$_PHPhotoLibrary");
extern cx_class_t PHPhotoLibrary_m __asm__("OBJC_METACLASS_$_PHPhotoLibrary");

#define PHC(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); static cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
PHC(PHAsset)
PHC(PHFetchOptions)
PHC(PHFetchResult)

static const char *kPhotosService = "kTCCServicePhotos";

typedef struct { id key; photos_asset a; } asset_slot;
typedef struct { id key; id pred; id sorts; int hidden; unsigned limit; } opt_slot;
typedef struct { id key; photos_asset items[PHOTOS_CAP]; int n; } res_slot;

static asset_slot g_assets[PHOTOS_CAP];
static opt_slot g_opts[32];
static res_slot g_res[8];
static id g_lib;

int cx_tcc_state(const char *service);
int cx_tcc_request(const char *service);

static asset_slot *asset_of(id o) {
    int i;
    for (i = 0; i < PHOTOS_CAP; i++) if (g_assets[i].key == o) return &g_assets[i];
    return 0;
}
static asset_slot *asset_make(id o, const photos_asset *a) {
    asset_slot *s = asset_of(o);
    int i;
    if (s) { s->a = *a; return s; }
    for (i = 0; i < PHOTOS_CAP; i++) if (!g_assets[i].key) {
        g_assets[i].key = o;
        g_assets[i].a = *a;
        return &g_assets[i];
    }
    return 0;
}
static opt_slot *opt_of(id o) {
    int i;
    if (!o) return 0;
    for (i = 0; i < 32; i++) if (g_opts[i].key == o) return &g_opts[i];
    return 0;
}
static opt_slot *opt_make(id o) {
    opt_slot *s = opt_of(o);
    int i;
    if (s) return s;
    for (i = 0; i < 32; i++) if (!g_opts[i].key) {
        memset(&g_opts[i], 0, sizeof g_opts[i]);
        g_opts[i].key = o;
        return &g_opts[i];
    }
    return 0;
}
static res_slot *res_of(id o) {
    int i;
    for (i = 0; i < 8; i++) if (g_res[i].key == o) return &g_res[i];
    return 0;
}
static res_slot *res_make(id o) {
    int i;
    for (i = 0; i < 8; i++) if (!g_res[i].key || g_res[i].key == o) {
        memset(&g_res[i], 0, sizeof g_res[i]);
        g_res[i].key = o;
        return &g_res[i];
    }
    memset(&g_res[0], 0, sizeof g_res[0]);
    g_res[0].key = o;
    return &g_res[0];
}

static int auth_now(void) { return photos_auth_from_tcc(cx_tcc_state(kPhotosService)); }

static void library_path(char *out, size_t cap) {
    const char *env = getenv("MOS_PHOTOS_LIBRARY");
    if (env && env[0]) { snprintf(out, cap, "%s", env); return; }
    id h = NSHomeDirectory();
    photos_root_from_home(h ? cx_cstr(h) : 0, out, cap);
}

static id asset_new(const photos_asset *a) {
    id o = cx_alloc_instance(&PHAsset_c);
    if (!o || !asset_make(o, a)) return 0;
    return o;
}
static id result_new(const photos_asset *src, int n) {
    id o = cx_alloc_instance(&PHFetchResult_c);
    res_slot *r = o ? res_make(o) : 0;
    int i;
    if (!r) return o;
    if (n > PHOTOS_CAP) n = PHOTOS_CAP;
    for (i = 0; i < n; i++) {
        r->items[i] = src[i];
        asset_new(&src[i]);
    }
    r->n = n;
    return o;
}
static id result_empty(void) { return result_new(0, 0); }

static int sort_code(const char *key, int asc) {
    if (!key) return PHOTOS_SORT_NONE;
    if (!strcmp(key, "creationDate")) return asc ? PHOTOS_SORT_CREATION_ASC : PHOTOS_SORT_CREATION_DESC;
    if (!strcmp(key, "localIdentifier")) return asc ? PHOTOS_SORT_IDENT_ASC : PHOTOS_SORT_IDENT_DESC;
    if (!strcmp(key, "pixelWidth")) return asc ? PHOTOS_SORT_WIDTH_ASC : PHOTOS_SORT_WIDTH_DESC;
    if (!strcmp(key, "pixelHeight")) return asc ? PHOTOS_SORT_HEIGHT_ASC : PHOTOS_SORT_HEIGHT_DESC;
    if (!strcmp(key, "duration")) return asc ? PHOTOS_SORT_DURATION_ASC : PHOTOS_SORT_DURATION_DESC;
    return PHOTOS_SORT_NONE;
}

static id ph_shared(id cls, SEL c) {
    (void)c;
    if (!g_lib) g_lib = cx_alloc_instance((cx_class_t *)cls);
    return g_lib;
}
static long ph_status(id cls, SEL c) {
    (void)cls; (void)c;
    return auth_now();
}
static void call_status_block(void *blk, long st) {
    void (*inv)(void *, long);
    if (!blk) return;
    inv = *(void (**)(void *, long))((unsigned char *)blk + 16);
    if (inv) inv(blk, st);
}
static void ph_request(id cls, SEL c, void *blk) {
    (void)cls; (void)c;
    cx_tcc_request(kPhotosService);
    call_status_block(blk, auth_now());
}
static void ph_request_level(id cls, SEL c, long level, void *blk) {
    (void)level;
    ph_request(cls, c, blk);
}

static id opt_init(id self, SEL c) {
    (void)c;
    opt_make(self);
    return self;
}
static void opt_set_pred(id self, SEL c, id p) { (void)c; opt_slot *s = opt_make(self); if (s) s->pred = p; }
static id opt_pred(id self, SEL c) { (void)c; opt_slot *s = opt_of(self); return s ? s->pred : 0; }
static void opt_set_sort(id self, SEL c, id a) { (void)c; opt_slot *s = opt_make(self); if (s) s->sorts = a; }
static id opt_sort(id self, SEL c) { (void)c; opt_slot *s = opt_of(self); return s ? s->sorts : 0; }
static void opt_set_limit(id self, SEL c, unsigned long n) { (void)c; opt_slot *s = opt_make(self); if (s) s->limit = (unsigned)n; }
static unsigned long opt_limit(id self, SEL c) { (void)c; opt_slot *s = opt_of(self); return s ? s->limit : 0; }
static void opt_set_hidden(id self, SEL c, BOOL v) { (void)c; opt_slot *s = opt_make(self); if (s) s->hidden = v ? 1 : 0; }
static BOOL opt_hidden(id self, SEL c) { (void)c; opt_slot *s = opt_of(self); return s && s->hidden ? YES : NO; }

static const photos_asset *asset_data(id self) {
    asset_slot *s = asset_of(self);
    return s ? &s->a : 0;
}
static id asset_ident(id self, SEL c) {
    const photos_asset *a = asset_data(self);
    (void)c;
    return a ? cx_str(a->ident) : cx_str("");
}
static long asset_media(id self, SEL c) {
    const photos_asset *a = asset_data(self);
    (void)c;
    return a ? a->media : 0;
}
static long asset_w(id self, SEL c) {
    const photos_asset *a = asset_data(self);
    (void)c;
    return a ? a->pixel_w : 0;
}
static long asset_h(id self, SEL c) {
    const photos_asset *a = asset_data(self);
    (void)c;
    return a ? a->pixel_h : 0;
}
static double asset_dur(id self, SEL c) {
    const photos_asset *a = asset_data(self);
    (void)c;
    return a ? a->duration : 0;
}
static BOOL asset_hidden(id self, SEL c) {
    const photos_asset *a = asset_data(self);
    (void)c;
    return a && a->hidden ? YES : NO;
}
static id asset_date(id self, SEL c) {
    const photos_asset *a = asset_data(self);
    (void)c;
    if (!a) return 0;
    return MSG(id, (id)cx_class_named("NSDate"), "dateWithTimeIntervalSince1970:", (double)a->creation_unix);
}
static id asset_vfk(id self, SEL c, id key) {
    const char *k = cx_cstr(key);
    (void)c;
    if (!k) return 0;
    if (!strcmp(k, "localIdentifier")) return asset_ident(self, 0);
    if (!strcmp(k, "mediaType")) return cx_num_i(asset_media(self, 0));
    if (!strcmp(k, "pixelWidth")) return cx_num_i(asset_w(self, 0));
    if (!strcmp(k, "pixelHeight")) return cx_num_i(asset_h(self, 0));
    if (!strcmp(k, "duration")) return cx_num_d(asset_dur(self, 0));
    if (!strcmp(k, "creationDate")) return asset_date(self, 0);
    if (!strcmp(k, "hidden") || !strcmp(k, "isHidden")) return cx_num_i(asset_hidden(self, 0) ? 1 : 0);
    return 0;
}

static unsigned long res_count(id self, SEL c) {
    res_slot *r = res_of(self);
    (void)c;
    return r ? (unsigned long)r->n : 0;
}
static id res_at(id self, SEL c, unsigned long i) {
    res_slot *r = res_of(self);
    (void)c;
    if (!r || i >= (unsigned long)r->n) return 0;
    return asset_new(&r->items[i]);
}
static id res_first(id self, SEL c) { return res_at(self, c, 0); }
static id res_last(id self, SEL c) {
    res_slot *r = res_of(self);
    if (!r || r->n <= 0) return 0;
    return res_at(self, c, (unsigned long)r->n - 1);
}
static void res_enum(id self, SEL c, void *blk) {
    res_slot *r = res_of(self);
    void (*inv)(void *, id, unsigned long, BOOL *);
    BOOL stop = NO;
    int i;
    (void)c;
    if (!r || !blk) return;
    inv = *(void (**)(void *, id, unsigned long, BOOL *))((unsigned char *)blk + 16);
    if (!inv) return;
    for (i = 0; i < r->n && !stop; i++) inv(blk, asset_new(&r->items[i]), (unsigned long)i, &stop);
}

static void fill_sorts(id sorts, photos_query *q) {
    unsigned long n, i;
    q->nsort = 0;
    if (!sorts) return;
    n = MSG0(unsigned long, sorts, "count");
    if (n > 4) n = 4;
    for (i = 0; i < n; i++) {
        id d = MSG(id, sorts, "objectAtIndex:", i);
        id key = d ? MSG0(id, d, "key") : 0;
        int asc = d ? (MSG0(BOOL, d, "ascending") ? 1 : 0) : 1;
        int code = sort_code(key ? cx_cstr(key) : 0, asc);
        if (code) q->sort[q->nsort++] = code;
    }
}

static void assets_reset(void) { memset(g_assets, 0, sizeof g_assets); }

static id ph_fetch(id cls, SEL c, id opts) {
    char path[512];
    photos_lib *lib;
    photos_query hidden_q, sort_q;
    int idx[PHOTOS_CAP], idx2[PHOTOS_CAP];
    photos_asset kept[PHOTOS_CAP], ordered[PHOTOS_CAP];
    int n, nk = 0, n2, i;
    opt_slot *op;
    (void)cls; (void)c;
    if (auth_now() != PHOTOS_AUTH_AUTHORIZED) return result_empty();
    library_path(path, sizeof path);
    lib = photos_open(path);
    if (!lib) return result_empty();
    memset(&hidden_q, 0, sizeof hidden_q);
    hidden_q.media = -1;
    op = opt_of(opts);
    hidden_q.include_hidden = op && op->hidden ? 1 : 0;
    n = photos_apply(lib->items, lib->n, &hidden_q, idx, PHOTOS_CAP);
    for (i = 0; i < n; i++) {
        id tmp = asset_new(&lib->items[idx[i]]);
        if (op && op->pred) {
            BOOL ok = MSG(BOOL, op->pred, "evaluateWithObject:", tmp);
            if (!ok) continue;
        }
        kept[nk++] = lib->items[idx[i]];
    }
    photos_close(lib);
    assets_reset();
    memset(&sort_q, 0, sizeof sort_q);
    sort_q.media = -1;
    sort_q.include_hidden = 1;
    if (op) {
        fill_sorts(op->sorts, &sort_q);
        sort_q.limit = op->limit;
    }
    n2 = photos_apply(kept, nk, &sort_q, idx2, PHOTOS_CAP);
    for (i = 0; i < n2; i++) ordered[i] = kept[idx2[i]];
    return result_new(ordered, n2);
}

static void ph_def(cx_class_t *c, cx_class_t *m, const char *name) {
    cx_define_class(c, m, name, cx_class_named("NSObject"), 0);
    cx_mark_stub_class(c);
}

void cx_photos_init(void) {
    ph_def(&PHAsset_c, &PHAsset_m, "PHAsset");
    ph_def(&PHFetchOptions_c, &PHFetchOptions_m, "PHFetchOptions");
    ph_def(&PHFetchResult_c, &PHFetchResult_m, "PHFetchResult");
    cx_mark_stub_class(&PHPhotoLibrary_c);
    cx_add_cmethod(&PHPhotoLibrary_c, "sharedPhotoLibrary", (void *)ph_shared);
    cx_add_cmethod(&PHPhotoLibrary_c, "authorizationStatus", (void *)ph_status);
    cx_add_cmethod(&PHPhotoLibrary_c, "authorizationStatusForAccessLevel:", (void *)ph_status);
    cx_add_cmethod(&PHPhotoLibrary_c, "requestAuthorization:", (void *)ph_request);
    cx_add_cmethod(&PHPhotoLibrary_c, "requestAuthorizationForAccessLevel:handler:", (void *)ph_request_level);
    cx_add_method(&PHFetchOptions_c, "init", (void *)opt_init);
    cx_add_method(&PHFetchOptions_c, "setPredicate:", (void *)opt_set_pred);
    cx_add_method(&PHFetchOptions_c, "predicate", (void *)opt_pred);
    cx_add_method(&PHFetchOptions_c, "setSortDescriptors:", (void *)opt_set_sort);
    cx_add_method(&PHFetchOptions_c, "sortDescriptors", (void *)opt_sort);
    cx_add_method(&PHFetchOptions_c, "setFetchLimit:", (void *)opt_set_limit);
    cx_add_method(&PHFetchOptions_c, "fetchLimit", (void *)opt_limit);
    cx_add_method(&PHFetchOptions_c, "setIncludeHiddenAssets:", (void *)opt_set_hidden);
    cx_add_method(&PHFetchOptions_c, "includeHiddenAssets", (void *)opt_hidden);
    cx_add_method(&PHAsset_c, "localIdentifier", (void *)asset_ident);
    cx_add_method(&PHAsset_c, "mediaType", (void *)asset_media);
    cx_add_method(&PHAsset_c, "pixelWidth", (void *)asset_w);
    cx_add_method(&PHAsset_c, "pixelHeight", (void *)asset_h);
    cx_add_method(&PHAsset_c, "duration", (void *)asset_dur);
    cx_add_method(&PHAsset_c, "creationDate", (void *)asset_date);
    cx_add_method(&PHAsset_c, "isHidden", (void *)asset_hidden);
    cx_add_method(&PHAsset_c, "valueForKey:", (void *)asset_vfk);
    cx_add_cmethod(&PHAsset_c, "fetchAssetsWithOptions:", (void *)ph_fetch);
    cx_add_method(&PHFetchResult_c, "count", (void *)res_count);
    cx_add_method(&PHFetchResult_c, "objectAtIndex:", (void *)res_at);
    cx_add_method(&PHFetchResult_c, "objectAtIndexedSubscript:", (void *)res_at);
    cx_add_method(&PHFetchResult_c, "firstObject", (void *)res_first);
    cx_add_method(&PHFetchResult_c, "lastObject", (void *)res_last);
    cx_add_method(&PHFetchResult_c, "enumerateObjectsUsingBlock:", (void *)res_enum);
}
