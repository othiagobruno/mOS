/*
 * mOS libSystem — CFPreferences e o armazenamento do NSUserDefaults.
 *
 * Funcional: domínio do usuário em NSHomeDirectory()/Library/Preferences/<domínio>.plist,
 * domínio de qualquer usuário em /Library/Preferences, .GlobalPreferences no mesmo arquivo
 * que NSGlobalDomain e kCFPreferencesAnyApplication, domínio de registro só em memória,
 * synchronize devolve falha real se a escrita falhar, bplist existente não é sobrescrito.
 *
 * Limitações: não há cfprefsd (cada processo grava o plist; outro processo só aparece se o
 * mtime ou o tamanho mudar — contrato com o VFS). CurrentHost e AnyHost são o mesmo domínio.
 * Sem MDM: CFPreferencesAppValueIsForced é 0 de verdade. Tipo que não é property list é
 * recusado, sem exceção. Container devolvido é cópia: mutar no lugar não grava.
 */
#include "foundation.h"
#include "libc.h"
#include "prefs_core.inc"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
id NSHomeDirectory(void);
id cx_plist_parse(const u8 *b, u64 n);
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");

int rename(const char *a, const char *b);

typedef struct { cx_class_t *isa; u32 kind; u32 pad; double d; i64 i; } pf_num_t;
typedef struct { cx_class_t *isa; double t; } pf_date_t;
typedef struct {
    u32 st_dev; u16 st_mode, st_nlink; u64 st_ino; u32 st_uid, st_gid, st_rdev;
    i64 atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns, btime, btime_ns;
    i64 st_size, st_blocks; i32 st_blksize; u32 st_flags, st_gen; i32 lspare; i64 qspare[2];
} pf_stat_t;

typedef struct pf_ud {
    void *obj;
    char primary[208];
    char reg[48];
    char suites[16][208];
    int nsuites;
    int bound;
    struct pf_ud *next;
} pf_ud;

typedef struct pf_cache {
    char *scope, *name, *key;
    unsigned gen;
    id obj;
    struct pf_cache *next;
} pf_cache;

static pf_db *g_db;
static pf_ud *g_uds;
static pf_cache *g_cache;
static char g_bundle[256];
static int g_bundle_ok;

static const char *prefs_cstr(id o) {
    if (!o) return 0;
    if (cx_is_str(o)) return ((nsstr_t *)o)->s ? ((nsstr_t *)o)->s : "";
    if ((void *)o->isa == (void *)cx_cfconst_isa) {
        typedef struct { void *isa; u64 flags; const char *s; u64 n; } cf_t;
        return ((cf_t *)o)->s;
    }
    return cx_cstr(o);
}

static const char *bundle_id(void) {
    if (g_bundle_ok) return g_bundle;
    id b = MSG0(id, (id)cx_class_named("NSBundle"), "mainBundle");
    id bid = b ? MSG0(id, b, "bundleIdentifier") : 0;
    const char *s = bid ? prefs_cstr(bid) : 0;
    if (!s || !s[0] || !strcmp(s, "(null)") || !strcmp(s, "?")) s = "com.mos.app";
    snprintf(g_bundle, sizeof g_bundle, "%s", s);
    g_bundle_ok = 1;
    return g_bundle;
}

static int canon_domain(const char *s, char *out, size_t cap) {
    if (!s || !s[0]) return 0;
    if (!strcmp(s, "kCFPreferencesAnyApplication") || !strcmp(s, "NSGlobalDomain")) s = ".GlobalPreferences";
    if (!strcmp(s, "kCFPreferencesCurrentApplication")) s = bundle_id();
    if (!pf_domain_ok(s) || strlen(s) + 1 > cap) return 0;
    memcpy(out, s, strlen(s) + 1);
    return 1;
}

static pf_db *db(void) {
    if (!g_db) g_db = pf_db_new();
    return g_db;
}

static const char *home_dir(void) {
    id h = NSHomeDirectory();
    const char *s = h ? prefs_cstr(h) : 0;
    if (!s || !s[0] || !strcmp(s, "(null)")) return "/Users/Visitante";
    return s;
}

static void dom_path(const char *scope, const char *name, char *out, size_t cap) {
    if (scope && !strcmp(scope, "any")) snprintf(out, cap, "/Library/Preferences/%s.plist", name);
    else snprintf(out, cap, "%s/Library/Preferences/%s.plist", home_dir(), name);
}

static void mkdir_parent(const char *path) {
    char b[1024];
    snprintf(b, sizeof b, "%s", path);
    char *slash = strrchr(b, '/');
    if (!slash || slash == b) return;
    *slash = 0;
    for (char *p = b + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(b, 0755); *p = '/'; }
    mkdir(b, 0755);
}

static int fs_stamp(void *ctx, const char *path, unsigned long long *stamp) {
    (void)ctx;
    pf_stat_t st;
    memset(&st, 0, sizeof st);
    if (stat(path, &st) != 0) return -1;
    if (stamp) *stamp = ((unsigned long long)st.mtime << 20) ^ (unsigned long long)st.st_size;
    return 0;
}

static int fs_read(void *ctx, const char *path, char **buf, size_t *len, unsigned long long *stamp) {
    (void)ctx;
    FILE_ *f = fopen(path, "rb");
    if (!f) return -1;
    size_t cap = 256, n = 0;
    char *b = (char *)malloc(cap);
    if (!b) { fclose(f); return -1; }
    for (;;) {
        if (n + 1 >= cap) {
            cap *= 2;
            char *nb = (char *)realloc(b, cap);
            if (!nb) { free(b); fclose(f); return -1; }
            b = nb;
        }
        size_t r = fread(b + n, 1, cap - n - 1, f);
        n += r;
        if (r == 0) break;
    }
    fclose(f);
    b[n] = 0;
    *buf = b;
    *len = n;
    if (stamp) fs_stamp(0, path, stamp);
    return 0;
}

static int fs_write(void *ctx, const char *path, const char *data, size_t n) {
    (void)ctx;
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    mkdir_parent(path);
    FILE_ *f = fopen(tmp, "wb");
    if (!f) return -1;
    size_t w = fwrite(data, 1, n, f);
    fclose(f);
    if (w != n) { unlink(tmp); return -1; }
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

static const pf_fs g_fs = { 0, fs_read, fs_write, fs_stamp };

static void load_dom(const char *scope, const char *name) {
    if (!scope || !name || !strcmp(scope, "reg")) return;
    char path[512];
    dom_path(scope, name, path, sizeof path);
    pf_load(db(), scope, name, path, &g_fs, 0);
}

static void release_tree(id o) {
    if (!o) return;
    cx_class_t *ac = cx_class_named("NSArray");
    if (ac && cx_is_subclass(o->isa, ac)) {
        nsarr_t *a = (nsarr_t *)o;
        for (u64 i = 0; i < a->n; i++) release_tree(a->items[i]);
        free(a->items);
        a->items = 0;
        a->n = 0;
        objc_release(o);
        return;
    }
    cx_class_t *dc = cx_class_named("NSDictionary");
    if (dc && cx_is_subclass(o->isa, dc)) {
        nsdict_t *d = (nsdict_t *)o;
        for (u64 i = 0; i < d->n; i++) { release_tree(d->keys[i]); release_tree(d->vals[i]); }
        free(d->keys);
        free(d->vals);
        d->keys = 0;
        d->vals = 0;
        d->n = 0;
        objc_release(o);
        return;
    }
    objc_release(o);
}

static id id_from_pf(const pf_val *v, int depth) {
    if (!v || depth > 16) return 0;
    switch (v->type) {
    case PF_BOOL: return cx_num_i(v->b ? 1 : 0);
    case PF_INT: return cx_num_i(v->i);
    case PF_REAL: return cx_num_d(v->d);
    case PF_STR: return cx_str(v->str ? v->str : "");
    case PF_DATA:
        return MSG(id, (id)cx_class_named("NSData"), "dataWithBytes:length:", (const void *)(v->str ? v->str : ""), (u64)v->n);
    case PF_DATE: {
        cx_class_t *c = cx_class_named("NSDate");
        id d = c ? cx_alloc_instance(c) : 0;
        if (d) ((pf_date_t *)d)->t = v->d;
        return d;
    }
    case PF_ARRAY: {
        id a = cx_arr_new(cx_class_named("NSMutableArray"));
        if (!a) return 0;
        for (pf_ent *e = v->ent; e; e = e->next) {
            id o = id_from_pf(e->v, depth + 1);
            if (!o) { release_tree(a); return 0; }
            cx_arr_add(a, o);
            objc_release(o);
        }
        return a;
    }
    case PF_DICT: {
        cx_class_t *c = cx_class_named("NSMutableDictionary");
        id d = c ? cx_alloc_instance(c) : 0;
        if (!d) return 0;
        for (pf_ent *e = v->ent; e; e = e->next) {
            id k = e->key ? cx_str(e->key) : 0;
            id o = id_from_pf(e->v, depth + 1);
            if (!k || !o) {
                if (k) objc_release(k);
                if (o) release_tree(o);
                release_tree(d);
                return 0;
            }
            MSG(void, d, "setObject:forKey:", o, k);
            objc_release(k);
            objc_release(o);
        }
        return d;
    }
    default:
        return 0;
    }
}

static pf_val *pf_from_id(id o, int depth) {
    if (!o || depth > 16) return 0;
    cx_class_t *nullc = cx_class_named("NSNull");
    if (nullc && o->isa == nullc) return 0;
    if (cx_is_str(o) || (void *)o->isa == (void *)cx_cfconst_isa) return pf_str(prefs_cstr(o));
    cx_class_t *nc = cx_class_named("NSNumber");
    if (nc && cx_is_subclass(o->isa, nc)) {
        pf_num_t *n = (pf_num_t *)o;
        if (n->kind == 2) return pf_bool(n->i != 0);
        if (n->kind == 1) return pf_real(n->d);
        return pf_int(n->i);
    }
    cx_class_t *dt = cx_class_named("NSDate");
    if (dt && cx_is_subclass(o->isa, dt)) return pf_date(((pf_date_t *)o)->t);
    cx_class_t *data = cx_class_named("NSData");
    if (data && cx_is_subclass(o->isa, data)) {
        const void *b = MSG0(const void *, o, "bytes");
        u64 n = MSG0(u64, o, "length");
        return pf_data(b, (size_t)n);
    }
    cx_class_t *ac = cx_class_named("NSArray");
    if (ac && cx_is_subclass(o->isa, ac)) {
        pf_val *a = pf_array();
        nsarr_t *ar = (nsarr_t *)o;
        for (u64 i = 0; a && i < ar->n; i++) {
            pf_val *c = pf_from_id(ar->items[i], depth + 1);
            if (!c) { pf_free(a); return 0; }
            pf_array_add(a, c);
        }
        return a;
    }
    cx_class_t *dc = cx_class_named("NSDictionary");
    if (dc && cx_is_subclass(o->isa, dc)) {
        pf_val *d = pf_dict();
        nsdict_t *di = (nsdict_t *)o;
        for (u64 i = 0; d && i < di->n; i++) {
            const char *k = prefs_cstr(di->keys[i]);
            pf_val *c = pf_from_id(di->vals[i], depth + 1);
            if (!k || !c || !cx_is_str(di->keys[i])) { pf_free(c); pf_free(d); return 0; }
            pf_dict_put(d, k, c);
        }
        return d;
    }
    return 0;
}

static id cache_get(const char *scope, const char *name, const char *key, const pf_val *v) {
    unsigned g = pf_gen(db(), scope, name);
    for (pf_cache *e = g_cache; e; e = e->next) {
        if (strcmp(e->scope, scope) || strcmp(e->name, name) || strcmp(e->key, key)) continue;
        if (e->gen == g && e->obj) return e->obj;
        release_tree(e->obj);
        e->obj = id_from_pf(v, 0);
        e->gen = g;
        return e->obj;
    }
    id o = id_from_pf(v, 0);
    if (!o) return 0;
    pf_cache *e = (pf_cache *)calloc(1, sizeof *e);
    if (!e) return o;
    e->scope = strdup(scope);
    e->name = strdup(name);
    e->key = strdup(key);
    if (!e->scope || !e->name || !e->key) {
        free(e->scope); free(e->name); free(e->key); free(e);
        return o;
    }
    e->gen = g;
    e->obj = o;
    e->next = g_cache;
    g_cache = e;
    return o;
}

static pf_ud *ud_get(void *obj, int create) {
    for (pf_ud *u = g_uds; u; u = u->next) if (u->obj == obj) return u;
    if (!create) return 0;
    pf_ud *u = (pf_ud *)calloc(1, sizeof *u);
    if (!u) return 0;
    u->obj = obj;
    snprintf(u->reg, sizeof u->reg, "r%llx", (unsigned long long)(size_t)obj);
    u->next = g_uds;
    g_uds = u;
    return u;
}

static void seed_access(pf_ud *u) {
    if (strcmp(u->primary, "com.apple.universalaccess") != 0) return;
    if (pf_get_mem(db(), "reg", u->reg, "mouseDriverCursorSize")) return;
    pf_put(db(), "reg", u->reg, "mouseDriverCursorSize", pf_real(1.0));
    pf_put(db(), "reg", u->reg, "reduceMotion", pf_int(0));
    pf_put(db(), "reg", u->reg, "reduceTransparency", pf_int(0));
    pf_put(db(), "reg", u->reg, "increaseContrast", pf_int(0));
    pf_put(db(), "reg", u->reg, "differentiateWithoutColor", pf_int(0));
}

void cx_prefs_bind(void *ud, const char *suite) {
    pf_ud *u = ud_get(ud, 1);
    if (!u || u->bound) return;
    const char *name = (suite && suite[0]) ? suite : bundle_id();
    if (!canon_domain(name, u->primary, sizeof u->primary))
        snprintf(u->primary, sizeof u->primary, "%s", "com.mos.app");
    u->bound = 1;
    seed_access(u);
}

static pf_ud *ensure(void *ud) {
    pf_ud *u = ud_get(ud, 1);
    if (u && !u->bound) cx_prefs_bind(ud, 0);
    return u;
}

static int search_of(pf_ud *u, const char **sc, const char **nm, int cap) {
    int n = 0;
    if (n < cap) { sc[n] = "user"; nm[n] = u->primary; n++; }
    for (int i = 0; i < u->nsuites && n < cap; i++) { sc[n] = "user"; nm[n] = u->suites[i]; n++; }
    if (n < cap) { sc[n] = "user"; nm[n] = ".GlobalPreferences"; n++; }
    if (n < cap) { sc[n] = "any"; nm[n] = u->primary; n++; }
    if (n < cap) { sc[n] = "reg"; nm[n] = u->reg; n++; }
    return n;
}

static void load_search(pf_ud *u) {
    load_dom("user", u->primary);
    for (int i = 0; i < u->nsuites; i++) load_dom("user", u->suites[i]);
    load_dom("user", ".GlobalPreferences");
    load_dom("any", u->primary);
}

id cx_prefs_object(void *ud, id key) {
    pf_ud *u = ensure(ud);
    const char *k = key ? prefs_cstr(key) : 0;
    if (!u || !k) return 0;
    load_search(u);
    const char *sc[24], *nm[24];
    int n = search_of(u, sc, nm, 24);
    int which = -1;
    pf_val *v = pf_search(db(), sc, nm, n, k, &which);
    if (!v || which < 0) return 0;
    return cache_get(sc[which], nm[which], k, v);
}

int cx_prefs_set(void *ud, id key, id value) {
    pf_ud *u = ensure(ud);
    const char *k = key ? prefs_cstr(key) : 0;
    if (!u || !k) return -1;
    load_dom("user", u->primary);
    if (!value) return pf_put(db(), "user", u->primary, k, 0);
    cx_class_t *nullc = cx_class_named("NSNull");
    if (nullc && value->isa == nullc) return pf_put(db(), "user", u->primary, k, 0);
    pf_val *v = pf_from_id(value, 0);
    if (!v) return -1;
    return pf_put(db(), "user", u->primary, k, v);
}

void cx_prefs_register(void *ud, id dict) {
    pf_ud *u = ensure(ud);
    if (!u || !dict) return;
    cx_class_t *dc = cx_class_named("NSDictionary");
    if (!dc || !cx_is_subclass(dict->isa, dc)) return;
    nsdict_t *di = (nsdict_t *)dict;
    for (u64 i = 0; i < di->n; i++) {
        const char *k = prefs_cstr(di->keys[i]);
        pf_val *v = pf_from_id(di->vals[i], 0);
        if (!k || !v) { pf_free(v); continue; }
        pf_put(db(), "reg", u->reg, k, v);
    }
}

static id dict_from_dom(const char *scope, const char *name) {
    pf_dom *d = 0;
    for (pf_dom *p = db()->doms; p; p = p->next)
        if (p->scope && p->name && !strcmp(p->scope, scope) && !strcmp(p->name, name)) { d = p; break; }
    if (!d || !d->root) {
        cx_class_t *c = cx_class_named("NSMutableDictionary");
        return c ? cx_alloc_instance(c) : 0;
    }
    return id_from_pf(d->root, 0);
}

id cx_prefs_representation(void *ud) {
    pf_ud *u = ensure(ud);
    cx_class_t *c = cx_class_named("NSMutableDictionary");
    id out = c ? cx_alloc_instance(c) : 0;
    if (!u || !out) return objc_autorelease(out);
    load_search(u);
    const char *sc[24], *nm[24];
    int n = search_of(u, sc, nm, 24);
    /* precedência: o primeiro domínio que tem a chave fica; os seguintes não cobrem */
    for (int i = n - 1; i >= 0; i--) {
        pf_dom *d = 0;
        for (pf_dom *p = db()->doms; p; p = p->next)
            if (p->scope && p->name && !strcmp(p->scope, sc[i]) && !strcmp(p->name, nm[i])) d = p;
        if (!d || !d->root) continue;
        for (pf_ent *e = d->root->ent; e; e = e->next) {
            if (!e->key) continue;
            id k = cx_str(e->key);
            id o = id_from_pf(e->v, 0);
            if (k && o) MSG(void, out, "setObject:forKey:", o, k);
            if (k) objc_release(k);
            if (o) objc_release(o);
        }
    }
    return objc_autorelease(out);
}

id cx_prefs_persistent(const char *name) {
    char dom[208];
    if (!name || !name[0]) name = bundle_id();
    if (!canon_domain(name, dom, sizeof dom)) return objc_autorelease(cx_alloc_instance(cx_class_named("NSMutableDictionary")));
    load_dom("user", dom);
    return objc_autorelease(dict_from_dom("user", dom));
}

static int save_dom(const char *scope, const char *name) {
    char path[512];
    dom_path(scope, name, path, sizeof path);
    int r = pf_save(db(), scope, name, path, &g_fs);
    return r == 0;
}

int cx_prefs_synchronize(void *ud) {
    pf_ud *u = ensure(ud);
    if (!u) return 0;
    return save_dom("user", u->primary);
}

void cx_prefs_remove_persistent(const char *name) {
    char dom[208];
    if (!name || !name[0]) name = bundle_id();
    if (!canon_domain(name, dom, sizeof dom)) return;
    load_dom("user", dom);
    pf_clear(db(), "user", dom);
    save_dom("user", dom);
}

int cx_prefs_set_persistent(const char *name, id dict) {
    char dom[208];
    if (!name || !name[0]) name = bundle_id();
    if (!canon_domain(name, dom, sizeof dom)) return 0;
    pf_val *d = dict ? pf_from_id(dict, 0) : pf_dict();
    if (!d || d->type != PF_DICT) { pf_free(d); return 0; }
    if (pf_replace(db(), "user", dom, d) < 0) return 0;
    return save_dom("user", dom);
}

void cx_prefs_add_suite(void *ud, const char *name) {
    pf_ud *u = ensure(ud);
    char dom[208];
    if (!u || !canon_domain(name, dom, sizeof dom)) return;
    if (!strcmp(dom, u->primary)) return;
    for (int i = 0; i < u->nsuites; i++) if (!strcmp(u->suites[i], dom)) return;
    if (u->nsuites >= 16) return;
    memcpy(u->suites[u->nsuites], dom, strlen(dom) + 1);
    u->nsuites++;
}

void cx_prefs_remove_suite(void *ud, const char *name) {
    pf_ud *u = ud_get(ud, 0);
    char dom[208];
    if (!u || !canon_domain(name, dom, sizeof dom)) return;
    for (int i = 0; i < u->nsuites; i++) if (!strcmp(u->suites[i], dom)) {
        memmove(u->suites[i], u->suites[i + 1], (size_t)(u->nsuites - i - 1) * sizeof u->suites[0]);
        u->nsuites--;
        return;
    }
}

static int domain_from_app(id app, char *out, size_t cap) {
    const char *s = app ? prefs_cstr(app) : 0;
    if (!s || !s[0] || !strcmp(s, "(null)") || !strcmp(s, "?")) s = bundle_id();
    return canon_domain(s, out, cap);
}

static const char *scope_of(id user) {
    const char *s = user ? prefs_cstr(user) : 0;
    if (s && !strcmp(s, "kCFPreferencesAnyUser")) return "any";
    return "user";
}

static id copy_cached(const char *scope, const char *name, const char *key) {
    load_dom(scope, name);
    pf_val *v = pf_get_mem(db(), scope, name, key);
    if (!v) return 0;
    id o = cache_get(scope, name, key, v);
    return o ? objc_retain(o) : 0;
}

id CFPreferencesCopyAppValue(id key, id app) {
    char dom[208];
    const char *k = key ? prefs_cstr(key) : 0;
    if (!k || !domain_from_app(app, dom, sizeof dom)) return 0;
    id o = copy_cached("user", dom, k);
    if (o) return o;
    o = copy_cached("user", ".GlobalPreferences", k);
    if (o) return o;
    return copy_cached("any", dom, k);
}

unsigned char CFPreferencesAppSynchronize(id app) {
    char dom[208];
    if (!domain_from_app(app, dom, sizeof dom)) return 0;
    load_dom("user", dom);
    return save_dom("user", dom) ? 1 : 0;
}

id CFPreferencesCopyValue(id key, id app, id user, id host) {
    (void)host;
    char dom[208];
    const char *k = key ? prefs_cstr(key) : 0;
    if (!k || !domain_from_app(app, dom, sizeof dom)) return 0;
    return copy_cached(scope_of(user), dom, k);
}

void CFPreferencesSetValue(id key, id value, id app, id user, id host) {
    (void)host;
    char dom[208];
    const char *k = key ? prefs_cstr(key) : 0;
    if (!k || !domain_from_app(app, dom, sizeof dom)) return;
    const char *sc = scope_of(user);
    load_dom(sc, dom);
    if (!value) { pf_put(db(), sc, dom, k, 0); return; }
    pf_val *v = pf_from_id(value, 0);
    if (!v) return;
    pf_put(db(), sc, dom, k, v);
}

unsigned char CFPreferencesSynchronize(id app, id user, id host) {
    (void)host;
    char dom[208];
    if (!domain_from_app(app, dom, sizeof dom)) return 0;
    const char *sc = scope_of(user);
    load_dom(sc, dom);
    return save_dom(sc, dom) ? 1 : 0;
}

void CFPreferencesSetAppValue(id key, id value, id app) {
    CFPreferencesSetValue(key, value, app, 0, 0);
}

long CFPreferencesGetAppIntegerValue(id key, id app, unsigned char *valid) {
    id v = CFPreferencesCopyAppValue(key, app);
    if (!v) { if (valid) *valid = 0; return 0; }
    if (valid) *valid = 1;
    long n = (long)MSG0(i64, v, "integerValue");
    objc_release(v);
    return n;
}

/* Sem perfil gerenciado (MDM). 0 é a resposta verdadeira, não um sucesso fingido. */
unsigned char CFPreferencesAppValueIsForced(id key, id app) {
    (void)key; (void)app;
    return 0;
}

id CFPreferencesCopyKeyList(id app, id user, id host) {
    (void)host;
    char dom[208];
    if (!domain_from_app(app, dom, sizeof dom)) return 0;
    const char *sc = scope_of(user);
    load_dom(sc, dom);
    id a = cx_arr_new(cx_class_named("NSMutableArray"));
    pf_dom *d = 0;
    for (pf_dom *p = db()->doms; p; p = p->next)
        if (p->scope && p->name && !strcmp(p->scope, sc) && !strcmp(p->name, dom)) d = p;
    if (d && d->root) for (pf_ent *e = d->root->ent; e; e = e->next) {
        if (!e->key) continue;
        id s = cx_str(e->key);
        if (s) { cx_arr_add(a, s); objc_release(s); }
    }
    return a;
}

id CFPropertyListCreateWithData(void *alloc, id data, unsigned long opt, long *fmt, id *err) {
    (void)alloc; (void)opt;
    if (err) *err = 0;
    if (!data) return 0;
    const u8 *b = MSG0(const u8 *, data, "bytes");
    u64 n = MSG0(u64, data, "length");
    if (fmt) *fmt = (n >= 8 && b && !memcmp(b, "bplist00", 8)) ? 200 : 100;
    return cx_plist_parse(b, n);
}
