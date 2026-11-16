/*
 * mOS libSystem — NSBundle e CFBundle completos (CFBundle é o próprio NSBundle).
 *
 * Por cima do NSBundle de foundation.c (caminho, Info.plist, mainBundle), este arquivo dá:
 *  - um bundle por caminho (bundleWithPath: devolve sempre o mesmo objeto), bundleForClass: pela imagem que
 *    contém a classe (dladdr), bundleWithIdentifier:, allBundles e allFrameworks (imagens carregadas);
 *  - as pastas do bundle (recursos, executável, Frameworks, SharedFrameworks, SharedSupport, PlugIns);
 *  - recursos com localização, na ordem do macOS: a pasta de recursos, depois <idioma>.lproj nos idiomas
 *    preferidos, Base.lproj e o idioma de desenvolvimento;
 *  - localizações (*.lproj), preferredLocalizations casando com AppleLanguages/NSLocale.preferredLanguages;
 *  - localizedStringForKey:value:table: lendo .strings (texto OpenStep em UTF-8/UTF-16, ou plist XML/binário),
 *    com cache por tabela e idioma, e localizedInfoDictionary (InfoPlist.strings);
 *  - load/unload/isLoaded/preflight (dlopen do executável), principalClass, classNamed:, executableArchitectures;
 *  - toda a API C de CFBundle que faltava.
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
IMP cx_lookup(cx_class_t *cls, SEL sel);
void *dlopen(const char *p, int mode);
void *dlsym(void *h, const char *n);
int dlclose(void *h);
const char *dlerror(void);
typedef struct { const char *dli_fname; void *dli_fbase; const char *dli_sname; void *dli_saddr; } dlinfo_t;
int dladdr(const void *addr, dlinfo_t *info);
u32 _dyld_image_count(void);
const char *_dyld_get_image_name(u32 i);
id cx_plist_parse(const u8 *b, u64 n);
char *strncpy(char *d, const char *s, size_t n);
char *strrchr(const char *s, int c);
char *strncat(char *d, const char *s, size_t n);
long read(int fd, void *b, size_t n);
int open(const char *p, int f, ...);
int close(int fd);
cx_class_t *objc_getClass(const char *n);

static id cls(const char *n) { return (id)cx_class_named(n); }
static id str(const char *s) { return cx_str(s ? s : ""); }
static const char *cs(id s) { return s ? cx_cstr(s) : ""; }
static int exists(const char *p) { return p && access(p, 0) == 0; }
static int ends(const char *p, const char *s) { size_t a = strlen(p), b = strlen(s); return a >= b && !strcmp(p + a - b, s); }
static id url_of(const char *p, int dir) { return p ? MSG(id, cls("NSURL"), "fileURLWithPath:isDirectory:", str(p), (BOOL)dir) : 0; }
static int is_dir(const char *p) { void *d = p ? opendir(p) : 0; if (!d) return 0; closedir(d); return 1; }
static id arr(void) { return MSG0(id, cls("NSMutableArray"), "array"); }
static id ret(id o) { return o ? objc_retain(o) : 0; }

/* ---- estado por bundle (os campos do foundation.c ficam como estão; o resto vive aqui) */
typedef struct bstate {
    struct bstate *next;
    id bundle;
    void *handle;
    int loaded;
    id strings;              /* NSMutableDictionary "tabela|idioma" -> NSDictionary */
    id loc_info;
} bstate_t;
static bstate_t *g_states;
static volatile int g_lock;
static void lk(void) { while (__atomic_exchange_n(&g_lock, 1, __ATOMIC_ACQUIRE)) { } }
static void ulk(void) { __atomic_store_n(&g_lock, 0, __ATOMIC_RELEASE); }
static bstate_t *state(id b) {
    lk();
    bstate_t *s = g_states;
    for (; s; s = s->next) if (s->bundle == b) break;
    if (!s && (s = calloc(1, sizeof *s))) { s->bundle = b; s->next = g_states; g_states = s; }
    ulk();
    return s;
}

static const char *root_of(id b, char *out, int cap) {
    id p = MSG0(id, b, "bundlePath");
    strncpy(out, cs(p), (size_t)cap - 1);
    out[cap - 1] = 0;
    return out;
}
static void contents_dir(id b, char *out, int cap) {
    char r[1024];
    root_of(b, r, sizeof r);
    char c[1100];
    snprintf(c, sizeof c, "%s/Contents", r);
    if (is_dir(c) && (ends(r, ".app") || ends(r, ".bundle") || ends(r, ".plugin") || ends(r, ".appex") || ends(r, ".xpc") || ends(r, ".kext")))
        snprintf(out, (size_t)cap, "%s", c);
    else if (ends(r, ".framework")) {
        snprintf(out, (size_t)cap, "%s/Versions/Current", r);
        if (!is_dir(out)) snprintf(out, (size_t)cap, "%s", r);
    } else snprintf(out, (size_t)cap, "%s", is_dir(c) ? c : r);
}

/* ---- cache de bundles por caminho */
static id g_by_path;   /* NSMutableDictionary caminho -> bundle */
static IMP g_orig_with_path, g_orig_init_path, g_orig_info_key;
static id b_with_path(id k, SEL c, id path) {
    if (!path) return 0;
    id std = MSG0(id, path, "stringByStandardizingPath");
    lk();
    if (!g_by_path) g_by_path = MSG0(id, MSG0(id, cls("NSMutableDictionary"), "alloc"), "init");
    id b = MSG(id, g_by_path, "objectForKey:", std);
    ulk();
    if (b) return b;
    b = ((id (*)(id, SEL, id))g_orig_with_path)(k, c, std);
    if (!b) return 0;
    lk();
    id again = MSG(id, g_by_path, "objectForKey:", std);
    if (again) b = again;
    else MSG(void, g_by_path, "setObject:forKey:", b, std);
    ulk();
    return b;
}
static id b_with_url(id k, SEL c, id url) { (void)c; return url ? b_with_path(k, sel_registerName("bundleWithPath:"), MSG0(id, url, "path")) : 0; }
static id b_init_url(id self, SEL c, id url) {
    (void)c;
    if (!url) { objc_release(self); return 0; }
    return ((id (*)(id, SEL, id))g_orig_init_path)(self, sel_registerName("initWithPath:"), MSG0(id, url, "path"));
}
/* raiz de bundle de um caminho de executável/imagem: X.app, X.framework, X.bundle... */
static int bundle_root_of_image(const char *img, char *out, int cap) {
    static const char *const suf[] = {".app/", ".framework/", ".bundle/", ".plugin/", ".appex/", ".xpc/", 0};
    const char *best = 0;
    size_t blen = 0;
    for (int i = 0; suf[i]; i++) {
        const char *p = img, *q;
        while ((q = strstr(p, suf[i]))) { if (!best || q + strlen(suf[i]) > best + blen) { best = q; blen = strlen(suf[i]); } p = q + 1; }
    }
    if (!best) return 0;
    snprintf(out, (size_t)cap, "%.*s", (int)(best - img + blen - 1), img);
    return 1;
}
static id b_for_class(id k, SEL c, id klass) {
    dlinfo_t di;
    char r[1024];
    if (klass && dladdr((const void *)klass, &di) && di.dli_fname && bundle_root_of_image(di.dli_fname, r, sizeof r)) {
        id main = MSG0(id, k, "mainBundle");
        char m[1024];
        if (!strcmp(root_of(main, m, sizeof m), r)) return main;
        id b = b_with_path(k, sel_registerName("bundleWithPath:"), str(r));
        if (b) return b;
    }
    (void)c;
    return MSG0(id, k, "mainBundle");
}
static id b_all_frameworks(id k, SEL c) {
    (void)c;
    id out = arr();
    u32 n = _dyld_image_count();
    for (u32 i = 0; i < n; i++) {
        const char *name = _dyld_get_image_name(i);
        char r[1024];
        if (!name || !strstr(name, ".framework/") || !bundle_root_of_image(name, r, sizeof r) || !ends(r, ".framework")) continue;
        id b = b_with_path(k, sel_registerName("bundleWithPath:"), str(r));
        if (b && !MSG(BOOL, out, "containsObject:", b)) cx_arr_add(out, b);
    }
    return out;
}
static id b_all_bundles(id k, SEL c) {
    (void)c;
    id out = arr();
    cx_arr_add(out, MSG0(id, k, "mainBundle"));
    lk();
    id vals = g_by_path ? MSG0(id, g_by_path, "allValues") : 0;
    ulk();
    u64 n = vals ? MSG0(u64, vals, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id b = MSG(id, vals, "objectAtIndex:", i);
        char r[1024];
        root_of(b, r, sizeof r);
        if (ends(r, ".framework")) continue;
        if (!MSG(BOOL, out, "containsObject:", b)) cx_arr_add(out, b);
    }
    return out;
}
static id b_with_identifier(id k, SEL c, id ident) {
    (void)c;
    if (!ident) return 0;
    id main = MSG0(id, k, "mainBundle");
    if (MSG(BOOL, MSG0(id, main, "bundleIdentifier"), "isEqualToString:", ident)) return main;
    id lists[2] = {b_all_frameworks(k, 0), b_all_bundles(k, 0)};
    for (int l = 0; l < 2; l++) {
        u64 n = MSG0(u64, lists[l], "count");
        for (u64 i = 0; i < n; i++) {
            id b = MSG(id, lists[l], "objectAtIndex:", i);
            id bi = MSG0(id, b, "bundleIdentifier");
            if (bi && MSG(BOOL, bi, "isEqualToString:", ident)) return b;
        }
    }
    /* frameworks embutidos ainda não carregados: Contents/Frameworks do app */
    char f[1100];
    contents_dir(main, f, sizeof f);
    strncat(f, "/Frameworks", sizeof f - strlen(f) - 1);
    void *d = opendir(f);
    typedef struct { u64 ino, off; u16 reclen, namlen; u8 type; char name[1024]; } de_t;
    for (de_t *e; d && (e = (de_t *)readdir(d));) {
        if (!ends(e->name, ".framework")) continue;
        char p[1300];
        snprintf(p, sizeof p, "%s/%s", f, e->name);
        id b = b_with_path(k, sel_registerName("bundleWithPath:"), str(p));
        id bi = b ? MSG0(id, b, "bundleIdentifier") : 0;
        if (bi && MSG(BOOL, bi, "isEqualToString:", ident)) { closedir(d); return b; }
    }
    if (d) closedir(d);
    return 0;
}

/* ---- pastas */
static id sub_path(id b, const char *rel) {
    char c[1100], p[1300];
    contents_dir(b, c, sizeof c);
    snprintf(p, sizeof p, "%s/%s", c, rel);
    return str(p);
}
static id b_priv_fw_path(id s, SEL c) { (void)c; return sub_path(s, "Frameworks"); }
static id b_shared_fw_path(id s, SEL c) { (void)c; return sub_path(s, "SharedFrameworks"); }
static id b_shared_support_path(id s, SEL c) { (void)c; return sub_path(s, "SharedSupport"); }
static id b_plugins_path(id s, SEL c) { (void)c; return sub_path(s, "PlugIns"); }
static id b_priv_fw_url(id s, SEL c) { (void)c; return url_of(cs(b_priv_fw_path(s, 0)), 1); }
static id b_shared_fw_url(id s, SEL c) { (void)c; return url_of(cs(b_shared_fw_path(s, 0)), 1); }
static id b_shared_support_url(id s, SEL c) { (void)c; return url_of(cs(b_shared_support_path(s, 0)), 1); }
static id b_plugins_url(id s, SEL c) { (void)c; return url_of(cs(b_plugins_path(s, 0)), 1); }
static id b_resource_url(id s, SEL c) { (void)c; return url_of(cs(MSG0(id, s, "resourcePath")), 1); }
static id b_bundle_url(id s, SEL c) { (void)c; return url_of(cs(MSG0(id, s, "bundlePath")), 1); }
static id b_exec_url(id s, SEL c) { (void)c; id p = MSG0(id, s, "executablePath"); return p ? url_of(cs(p), 0) : 0; }
static id b_receipt_url(id s, SEL c) { (void)c; return url_of(cs(sub_path(s, "_MASReceipt/receipt")), 0); }
static id b_aux_path(id s, SEL c, id name) {
    (void)c;
    char c2[1100], p[1300];
    contents_dir(s, c2, sizeof c2);
    snprintf(p, sizeof p, "%s/MacOS/%s", c2, cs(name));
    return exists(p) ? str(p) : 0;
}
static id b_aux_url(id s, SEL c, id name) { id p = b_aux_path(s, c, name); return p ? url_of(cs(p), 0) : 0; }

/* ---- localizações */
static id b_localizations(id s, SEL c) {
    (void)c;
    id out = arr();
    void *d = opendir(cs(MSG0(id, s, "resourcePath")));
    typedef struct { u64 ino, off; u16 reclen, namlen; u8 type; char name[1024]; } de_t;
    for (de_t *e; d && (e = (de_t *)readdir(d));) {
        if (!ends(e->name, ".lproj")) continue;
        cx_arr_add(out, cx_str_n(e->name, strlen(e->name) - 6));
    }
    if (d) closedir(d);
    return out;
}
static id b_dev_localization(id s, SEL c) {
    (void)c;
    id v = MSG(id, MSG0(id, s, "infoDictionary"), "objectForKey:", cx_str("CFBundleDevelopmentRegion"));
    return v ? v : cx_str("en");
}
/* idioma do .lproj que casa com uma preferência ("pt-BR" -> "pt-BR", "pt_BR", "pt", "Portuguese") */
static id match_one(id locs, const char *pref) {
    char a[64], lang[16];
    u64 n = MSG0(u64, locs, "count");
    snprintf(a, sizeof a, "%s", pref);
    for (char *p = a; *p; p++) if (*p == '_') *p = '-';
    int k = 0;
    while (pref[k] && pref[k] != '-' && pref[k] != '_' && k < 15) { lang[k] = pref[k]; k++; }
    lang[k] = 0;
    static const char *const legacy[][2] = {{"en", "English"}, {"pt", "Portuguese"}, {"es", "Spanish"}, {"fr", "French"},
        {"de", "German"}, {"it", "Italian"}, {"ja", "Japanese"}, {"zh", "Chinese"}, {"ko", "Korean"}, {"nl", "Dutch"}, {"ru", "Russian"}};
    for (int pass = 0; pass < 3; pass++) {
        for (u64 i = 0; i < n; i++) {
            id l = MSG(id, locs, "objectAtIndex:", i);
            char b[64];
            snprintf(b, sizeof b, "%s", cs(l));
            for (char *p = b; *p; p++) if (*p == '_') *p = '-';
            if (pass == 0 && !strcasecmp(a, b)) return l;
            if (pass == 1 && !strcasecmp(lang, b)) return l;
            if (pass == 2) for (size_t j = 0; j < sizeof legacy / sizeof legacy[0]; j++)
                if (!strcmp(lang, legacy[j][0]) && !strcmp(b, legacy[j][1])) return l;
        }
    }
    return 0;
}
static id pref_from_array(id locs, id prefs) {
    id out = arr();
    u64 n = prefs ? MSG0(u64, prefs, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id m = match_one(locs, cs(MSG(id, prefs, "objectAtIndex:", i)));
        if (m) { cx_arr_add(out, m); break; }   /* o macOS devolve o melhor casamento (e variantes do mesmo idioma) */
    }
    if (!MSG0(u64, out, "count") && MSG0(u64, locs, "count")) {
        id en = match_one(locs, "en");
        cx_arr_add(out, en ? en : MSG(id, locs, "objectAtIndex:", (u64)0));
    }
    return out;
}
static id user_prefs(void) { return MSG0(id, cls("NSLocale"), "preferredLanguages"); }
static id b_preferred_localizations(id s, SEL c) {
    (void)c;
    id locs = b_localizations(s, 0);
    id r = pref_from_array(locs, user_prefs());
    if (!MSG0(u64, r, "count")) cx_arr_add(r, b_dev_localization(s, 0));
    return r;
}
static id b_s_pref_from(id k, SEL c, id locs) { (void)k; (void)c; return pref_from_array(locs, user_prefs()); }
static id b_s_pref_from_for(id k, SEL c, id locs, id prefs) { (void)k; (void)c; return pref_from_array(locs, prefs ? prefs : user_prefs()); }

/* ---- recursos */
static int try_file(const char *dir, const char *sub, const char *name, const char *ext, char *out, int cap) {
    if (sub && *sub) snprintf(out, (size_t)cap, "%s/%s/%s%s%s", dir, sub, name, ext && *ext ? "." : "", ext ? ext : "");
    else snprintf(out, (size_t)cap, "%s/%s%s%s", dir, name, ext && *ext ? "." : "", ext ? ext : "");
    return exists(out);
}
/* pastas de busca na ordem do macOS: recursos, <loc>.lproj (preferida ou a pedida), Base.lproj, desenvolvimento */
static int search_dirs(id b, const char *forced_loc, char dirs[][1200], int max) {
    int n = 0;
    char res[1100];
    snprintf(res, sizeof res, "%s", cs(MSG0(id, b, "resourcePath")));
    snprintf(dirs[n++], 1200, "%s", res);
    if (forced_loc) { snprintf(dirs[n++], 1200, "%s/%s.lproj", res, forced_loc); return n; }
    id pref = b_preferred_localizations(b, 0);
    u64 np = MSG0(u64, pref, "count");
    for (u64 i = 0; i < np && n < max - 2; i++) snprintf(dirs[n++], 1200, "%s/%s.lproj", res, cs(MSG(id, pref, "objectAtIndex:", i)));
    snprintf(dirs[n++], 1200, "%s/Base.lproj", res);
    if (n < max) snprintf(dirs[n++], 1200, "%s/%s.lproj", res, cs(b_dev_localization(b, 0)));
    return n;
}
static id find_resource(id b, id name, id ext, id sub, const char *loc) {
    char dirs[12][1200], out[1500];
    char nm[512], ex[64];
    snprintf(nm, sizeof nm, "%s", cs(name));
    snprintf(ex, sizeof ex, "%s", cs(ext));
    /* "foo.png" com tipo nil: o nome já traz a extensão e try_file usa como está */
    int nd = search_dirs(b, loc, dirs, 12);
    for (int i = 0; i < nd; i++) if (try_file(dirs[i], sub ? cs(sub) : 0, nm, ex, out, sizeof out)) return str(out);
    return 0;
}
static id b_path_res(id s, SEL c, id name, id ext) { (void)c; return name ? find_resource(s, name, ext, 0, 0) : 0; }
static id b_path_res_dir(id s, SEL c, id name, id ext, id sub) { (void)c; return name ? find_resource(s, name, ext, sub, 0) : 0; }
static id b_path_res_dir_loc(id s, SEL c, id name, id ext, id sub, id loc) { (void)c; return name ? find_resource(s, name, ext, sub, loc ? cs(loc) : 0) : 0; }
static id b_url_res(id s, SEL c, id name, id ext) { (void)c; id p = name ? find_resource(s, name, ext, 0, 0) : 0; return p ? url_of(cs(p), is_dir(cs(p))) : 0; }
static id b_url_res_sub(id s, SEL c, id name, id ext, id sub) { (void)c; id p = name ? find_resource(s, name, ext, sub, 0) : 0; return p ? url_of(cs(p), is_dir(cs(p))) : 0; }
static id b_url_res_sub_loc(id s, SEL c, id name, id ext, id sub, id loc) { (void)c; id p = name ? find_resource(s, name, ext, sub, loc ? cs(loc) : 0) : 0; return p ? url_of(cs(p), is_dir(cs(p))) : 0; }
static void list_type(const char *dir, const char *sub, const char *ext, id out, id seen) {
    char d[1300];
    if (sub && *sub) snprintf(d, sizeof d, "%s/%s", dir, sub); else snprintf(d, sizeof d, "%s", dir);
    void *h = opendir(d);
    typedef struct { u64 ino, off; u16 reclen, namlen; u8 type; char name[1024]; } de_t;
    for (de_t *e; h && (e = (de_t *)readdir(h));) {
        if (!strcmp(e->name, ".") || !strcmp(e->name, "..")) continue;
        if (ext && *ext) { const char *dot = strrchr(e->name, '.'); if (!dot || strcmp(dot + 1, ext)) continue; }
        if (MSG(BOOL, seen, "containsObject:", cx_str(e->name))) continue;   /* o mais específico ganha */
        MSG(void, seen, "addObject:", cx_str(e->name));
        char p[1600];
        snprintf(p, sizeof p, "%s/%s", d, e->name);
        cx_arr_add(out, str(p));
    }
    if (h) closedir(h);
}
static id paths_of_type(id b, id ext, id sub, const char *loc) {
    char dirs[12][1200];
    int nd = search_dirs(b, loc, dirs, 12);
    id out = arr(), seen = MSG0(id, cls("NSMutableSet"), "set");
    for (int i = 0; i < nd; i++) list_type(dirs[i], sub ? cs(sub) : 0, ext ? cs(ext) : 0, out, seen);
    return out;
}
static id b_paths_type(id s, SEL c, id ext, id sub) { (void)c; return paths_of_type(s, ext, sub, 0); }
static id b_paths_type_loc(id s, SEL c, id ext, id sub, id loc) { (void)c; return paths_of_type(s, ext, sub, loc ? cs(loc) : 0); }
static id to_urls(id paths) {
    id out = arr();
    u64 n = MSG0(u64, paths, "count");
    for (u64 i = 0; i < n; i++) { const char *p = cs(MSG(id, paths, "objectAtIndex:", i)); cx_arr_add(out, url_of(p, is_dir(p))); }
    return out;
}
static id b_urls_ext(id s, SEL c, id ext, id sub) { (void)c; return to_urls(paths_of_type(s, ext, sub, 0)); }
static id b_urls_ext_loc(id s, SEL c, id ext, id sub, id loc) { (void)c; return to_urls(paths_of_type(s, ext, sub, loc ? cs(loc) : 0)); }
static id b_s_path_res(id k, SEL c, id name, id ext, id dir) {
    (void)k; (void)c;
    char out[1500];
    return name && dir && try_file(cs(dir), 0, cs(name), cs(ext), out, sizeof out) ? str(out) : 0;
}
static id b_s_paths_type(id k, SEL c, id ext, id dir) {
    (void)k; (void)c;
    id out = arr(), seen = MSG0(id, cls("NSMutableSet"), "set");
    if (dir) list_type(cs(dir), 0, ext ? cs(ext) : 0, out, seen);
    return out;
}
static id b_s_url_res(id k, SEL c, id name, id ext, id sub, id burl) {
    id b = burl ? b_with_url(k, c, burl) : 0;
    return b ? b_url_res_sub(b, 0, name, ext, sub) : 0;
}
static id b_s_urls_ext(id k, SEL c, id ext, id sub, id burl) {
    id b = burl ? b_with_url(k, c, burl) : 0;
    return b ? b_urls_ext(b, 0, ext, sub) : 0;
}

/* ---- .strings: texto OpenStep ("chave" = "valor";) em UTF-8/UTF-16, ou plist XML/binário */
static int utf16_to_utf8(const u8 *b, u64 n, int be, char **out) {
    u64 cap = n * 2 + 4, o = 0;
    char *r = malloc(cap);
    if (!r) return 0;
    for (u64 i = 0; i + 1 < n; i += 2) {
        u32 c = be ? (u32)(b[i] << 8 | b[i + 1]) : (u32)(b[i] | b[i + 1] << 8);
        if (c >= 0xD800 && c <= 0xDBFF && i + 3 < n) {
            u32 d = be ? (u32)(b[i + 2] << 8 | b[i + 3]) : (u32)(b[i + 2] | b[i + 3] << 8);
            if (d >= 0xDC00 && d <= 0xDFFF) { c = 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00); i += 2; }
        }
        if (c < 0x80) r[o++] = (char)c;
        else if (c < 0x800) { r[o++] = (char)(0xC0 | c >> 6); r[o++] = (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { r[o++] = (char)(0xE0 | c >> 12); r[o++] = (char)(0x80 | (c >> 6 & 63)); r[o++] = (char)(0x80 | (c & 63)); }
        else { r[o++] = (char)(0xF0 | c >> 18); r[o++] = (char)(0x80 | (c >> 12 & 63)); r[o++] = (char)(0x80 | (c >> 6 & 63)); r[o++] = (char)(0x80 | (c & 63)); }
    }
    r[o] = 0;
    *out = r;
    return (int)o;
}
static void put_utf8(char **o, u32 c) {
    char *r = *o;
    if (c < 0x80) *r++ = (char)c;
    else if (c < 0x800) { *r++ = (char)(0xC0 | c >> 6); *r++ = (char)(0x80 | (c & 63)); }
    else if (c < 0x10000) { *r++ = (char)(0xE0 | c >> 12); *r++ = (char)(0x80 | (c >> 6 & 63)); *r++ = (char)(0x80 | (c & 63)); }
    else { *r++ = (char)(0xF0 | c >> 18); *r++ = (char)(0x80 | (c >> 12 & 63)); *r++ = (char)(0x80 | (c >> 6 & 63)); *r++ = (char)(0x80 | (c & 63)); }
    *o = r;
}
static const char *skip_ws(const char *p, const char *e) {
    for (;;) {
        while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (p + 1 < e && p[0] == '/' && p[1] == '/') { while (p < e && *p != '\n') p++; continue; }
        if (p + 1 < e && p[0] == '/' && p[1] == '*') { const char *q = strstr(p + 2, "*/"); p = q && q < e ? q + 2 : e; continue; }
        return p;
    }
}
/* string entre aspas (com \n \t \" \\ \U1234 ሴ e octal) ou palavra sem aspas */
static id read_token(const char **pp, const char *e) {
    const char *p = *pp;
    if (p >= e) return 0;
    if (*p != '"') {
        const char *s = p;
        while (p < e && (((*p | 32) >= 'a' && (*p | 32) <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '.' || *p == '$' || *p == '/' || *p == ':' || *p == '-')) p++;
        if (p == s) return 0;
        *pp = p;
        return cx_str_n(s, (u64)(p - s));
    }
    p++;
    char *buf = malloc((size_t)(e - p) * 4 + 4), *o = buf;
    if (!buf) return 0;
    while (p < e && *p != '"') {
        if (*p != '\\' || p + 1 >= e) { *o++ = *p++; continue; }
        p++;
        char ch = *p++;
        switch (ch) {
        case 'n': *o++ = '\n'; break;
        case 't': *o++ = '\t'; break;
        case 'r': *o++ = '\r'; break;
        case 'a': *o++ = 7; break;
        case 'b': *o++ = 8; break;
        case 'f': *o++ = 12; break;
        case 'v': *o++ = 11; break;
        case 'U': case 'u': {
            u32 v = 0;
            for (int i = 0; i < 4 && p < e; i++, p++) {
                char h = *p;
                v = v * 16 + (u32)(h >= '0' && h <= '9' ? h - '0' : (h | 32) - 'a' + 10);
            }
            if (v >= 0xD800 && v <= 0xDBFF && p + 6 <= e && p[0] == '\\' && (p[1] == 'U' || p[1] == 'u')) {
                u32 w = 0;
                for (int i = 0; i < 4; i++) { char h = p[2 + i]; w = w * 16 + (u32)(h >= '0' && h <= '9' ? h - '0' : (h | 32) - 'a' + 10); }
                if (w >= 0xDC00 && w <= 0xDFFF) { v = 0x10000 + ((v - 0xD800) << 10) + (w - 0xDC00); p += 6; }
            }
            put_utf8(&o, v);
            break;
        }
        default:
            if (ch >= '0' && ch <= '7') {
                u32 v = (u32)(ch - '0');
                for (int i = 0; i < 2 && p < e && *p >= '0' && *p <= '7'; i++, p++) v = v * 8 + (u32)(*p - '0');
                put_utf8(&o, v);
            } else *o++ = ch;
        }
    }
    if (p < e) p++;
    *pp = p;
    id r = cx_str_n(buf, (u64)(o - buf));
    free(buf);
    return r;
}
id fdn_parse_strings(const u8 *b, u64 n) {
    char *conv = 0;
    const char *t;
    u64 len;
    if (n >= 8 && !memcmp(b, "bplist00", 8)) return cx_plist_parse(b, n);
    if (n >= 2 && b[0] == 0xFF && b[1] == 0xFE) { len = (u64)utf16_to_utf8(b + 2, n - 2, 0, &conv); t = conv; }
    else if (n >= 2 && b[0] == 0xFE && b[1] == 0xFF) { len = (u64)utf16_to_utf8(b + 2, n - 2, 1, &conv); t = conv; }
    else if (n >= 2 && b[1] == 0 && b[0] != 0) { len = (u64)utf16_to_utf8(b, n, 0, &conv); t = conv; }   /* UTF-16LE sem BOM */
    else if (n >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) { t = (const char *)b + 3; len = n - 3; }
    else { t = (const char *)b; len = n; }
    if (!t) return 0;
    const char *p = skip_ws(t, t + len), *e = t + len;
    if (p < e && *p == '<') { id r = cx_plist_parse((const u8 *)t, len); free(conv); return r; }
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    if (p < e && *p == '{') p++;   /* dicionário OpenStep com chaves */
    for (;;) {
        p = skip_ws(p, e);
        if (p >= e || *p == '}') break;
        id k = read_token(&p, e);
        if (!k) break;
        p = skip_ws(p, e);
        id v = k;
        if (p < e && *p == '=') { p++; p = skip_ws(p, e); v = read_token(&p, e); if (!v) break; p = skip_ws(p, e); }
        if (p < e && *p == ';') p++;
        MSG(void, d, "setObject:forKey:", v, k);
    }
    free(conv);
    return d;
}
static id load_file(const char *path) {
    int fd = open(path, 0);
    if (fd < 0) return 0;
    u64 cap = 1 << 16, n = 0;
    u8 *b = malloc(cap);
    for (long r; b && (r = read(fd, b + n, cap - n)) > 0;) {
        n += (u64)r;
        if (n == cap) { u8 *nb = realloc(b, cap * 2); if (!nb) break; b = nb; cap *= 2; }
    }
    close(fd);
    id d = b ? fdn_parse_strings(b, n) : 0;
    free(b);
    return d;
}
/* tabela de strings de um bundle num idioma (cache) */
static id table_for(id bundle, const char *table, const char *loc) {
    bstate_t *s = state(bundle);
    char key[300];
    snprintf(key, sizeof key, "%s|%s", table, loc ? loc : "");
    lk();
    if (!s->strings) s->strings = MSG0(id, MSG0(id, cls("NSMutableDictionary"), "alloc"), "init");
    id t = MSG(id, s->strings, "objectForKey:", cx_str(key));
    ulk();
    if (t) return t == (id)MSG0(id, cls("NSNull"), "null") ? 0 : t;
    char path[1500];
    snprintf(path, sizeof path, "%s/%s%s%s.strings", cs(MSG0(id, bundle, "resourcePath")), loc ? loc : "", loc ? ".lproj/" : "", table);
    t = load_file(path);
    lk();
    MSG(void, s->strings, "setObject:forKey:", t ? t : MSG0(id, cls("NSNull"), "null"), cx_str(key));
    ulk();
    return t;
}
static id b_localized_string(id s, SEL c, id key, id value, id table) {
    (void)c;
    const char *tb = table && MSG0(u64, table, "length") ? cs(table) : "Localizable";
    if (!key) return value ? value : cx_str("");
    id pref = b_preferred_localizations(s, 0);
    u64 n = MSG0(u64, pref, "count");
    id found = 0;
    for (u64 i = 0; i < n && !found; i++) {
        id t = table_for(s, tb, cs(MSG(id, pref, "objectAtIndex:", i)));
        if (t) found = MSG(id, t, "objectForKey:", key);
    }
    if (!found) { id t = table_for(s, tb, "Base"); if (t) found = MSG(id, t, "objectForKey:", key); }
    if (!found) { id t = table_for(s, tb, cs(b_dev_localization(s, 0))); if (t) found = MSG(id, t, "objectForKey:", key); }
    if (!found) { id t = table_for(s, tb, 0); if (t) found = MSG(id, t, "objectForKey:", key); }
    if (found && cx_is_str(found)) return found;
    /* NSShowNonLocalizedStrings mostra a chave em maiúsculas; sem ela, o valor dado ou a própria chave */
    if (value && MSG0(u64, value, "length")) return value;
    return key;
}
static id b_localized_info(id s, SEL c) {
    (void)c;
    bstate_t *st = state(s);
    if (st->loc_info) return st->loc_info;
    id info = MSG0(id, MSG0(id, s, "infoDictionary"), "mutableCopy");
    id pref = b_preferred_localizations(s, 0);
    id t = MSG0(u64, pref, "count") ? table_for(s, "InfoPlist", cs(MSG(id, pref, "objectAtIndex:", (u64)0))) : 0;
    if (t) MSG(void, info, "addEntriesFromDictionary:", t);
    st->loc_info = ret(info);
    return st->loc_info;
}
static id b_object_for_info_key(id s, SEL c, id key) {
    (void)c;
    id v = MSG(id, b_localized_info(s, 0), "objectForKey:", key);
    /* sem valor localizado: o caminho antigo (Info.plist e, nos helpers do Electron, o do app de fora) */
    if (!v && g_orig_info_key) v = ((id (*)(id, SEL, id))g_orig_info_key)(s, sel_registerName("objectForInfoDictionaryKey:"), key);
    return v;
}

/* ---- código: carregar, classes, arquiteturas */
static BOOL b_load_err(id s, SEL c, id *err) {
    (void)c;
    bstate_t *st = state(s);
    if (st->loaded) return YES;
    id main = MSG0(id, cls("NSBundle"), "mainBundle");
    if (s == main) { st->loaded = 1; return YES; }
    id exe = MSG0(id, s, "executablePath");
    if (!exe) {
        if (err) *err = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSCocoaErrorDomain"), (i64)4, (id)0);
        return NO;
    }
    st->handle = dlopen(cs(exe), 1 /* RTLD_LAZY */);
    if (!st->handle) {
        if (err) {
            id info = MSG0(id, cls("NSMutableDictionary"), "dictionary");
            const char *m = dlerror();
            if (m) MSG(void, info, "setObject:forKey:", cx_str(m), cx_str("NSDebugDescription"));
            *err = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSCocoaErrorDomain"), (i64)3587 /* NSExecutableLoadError */, info);
        }
        return NO;
    }
    st->loaded = 1;
    if (err) *err = 0;
    id nc = MSG0(id, cls("NSNotificationCenter"), "defaultCenter");
    MSG(void, nc, "postNotificationName:object:userInfo:", cx_str("NSBundleDidLoadNotification"), s, (id)0);
    return YES;
}
static BOOL b_load(id s, SEL c) { (void)c; return b_load_err(s, 0, 0); }
static BOOL b_is_loaded(id s, SEL c) {
    (void)c;
    bstate_t *st = state(s);
    if (st->loaded) return YES;
    /* já carregado por outro caminho (dependência do app): o executável está na lista do dyld */
    id exe = MSG0(id, s, "executablePath");
    if (!exe) return NO;
    u32 n = _dyld_image_count();
    for (u32 i = 0; i < n; i++) { const char *nm = _dyld_get_image_name(i); if (nm && !strcmp(nm, cs(exe))) { st->loaded = 1; return YES; } }
    return NO;
}
static BOOL b_unload(id s, SEL c) {
    (void)c;
    bstate_t *st = state(s);
    if (!st->loaded || !st->handle) return NO;
    dlclose(st->handle);
    st->handle = 0;
    st->loaded = 0;
    return YES;
}
static BOOL b_preflight(id s, SEL c, id *err) {
    (void)c;
    id exe = MSG0(id, s, "executablePath");
    if (exe && exists(cs(exe))) { if (err) *err = 0; return YES; }
    if (err) *err = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSCocoaErrorDomain"), (i64)4, (id)0);
    return NO;
}
static id b_class_named(id s, SEL c, id name) {
    (void)c;
    if (!name) return 0;
    b_load(s, 0);
    return (id)objc_getClass(cs(name));
}
static id b_principal(id s, SEL c) {
    (void)c;
    id name = MSG(id, MSG0(id, s, "infoDictionary"), "objectForKey:", cx_str("NSPrincipalClass"));
    b_load(s, 0);
    return name ? (id)objc_getClass(cs(name)) : 0;
}
/* CPU types do Mach-O: x86_64 0x01000007, arm64 0x0100000C, i386 7, ppc 18 */
static id archs_of_file(const char *p) {
    int fd = p ? open(p, 0) : -1;
    u8 h[4096];
    long n = fd >= 0 ? read(fd, h, sizeof h) : 0;
    if (fd >= 0) close(fd);
    id out = arr();
    if (n < 8) return out;
    u32 magic = (u32)h[0] << 24 | (u32)h[1] << 16 | (u32)h[2] << 8 | h[3];
    if (magic == 0xCAFEBABE || magic == 0xCAFEBABF) {   /* fat, big-endian */
        u32 cnt = (u32)h[4] << 24 | (u32)h[5] << 16 | (u32)h[6] << 8 | h[7];
        int step = magic == 0xCAFEBABF ? 32 : 20;
        for (u32 i = 0; i < cnt && 8 + (long)(i + 1) * step <= n; i++) {
            const u8 *e = h + 8 + i * (u32)step;
            u32 cpu = (u32)e[0] << 24 | (u32)e[1] << 16 | (u32)e[2] << 8 | e[3];
            cx_arr_add(out, cx_num_i((i64)cpu));
        }
    } else if (magic == 0xCFFAEDFE || magic == 0xCEFAEDFE) {   /* thin little-endian */
        u32 cpu = (u32)h[4] | (u32)h[5] << 8 | (u32)h[6] << 16 | (u32)h[7] << 24;
        cx_arr_add(out, cx_num_i((i64)cpu));
    }
    return out;
}
static id b_archs(id s, SEL c) { (void)c; return archs_of_file(cs(MSG0(id, s, "executablePath"))); }
static id b_desc(id s, SEL c) {
    (void)c;
    char b[1300];
    snprintf(b, sizeof b, "NSBundle <%s> (%s)", cs(MSG0(id, s, "bundlePath")), b_is_loaded(s, 0) ? "loaded" : "not yet loaded");
    return str(b);
}

/* ======================================================================== CFBundle */

typedef struct { long location, length; } cf_range_t;
static id B(void) { return cls("NSBundle"); }
id CFBundleGetBundleWithIdentifier(id ident) { return b_with_identifier(B(), 0, ident); }
id CFBundleGetAllBundles(void) { return b_all_bundles(B(), 0); }
id CFBundleCreateBundlesFromDirectory(void *alloc, id dirURL, id type) {
    (void)alloc;
    id out = MSG0(id, MSG0(id, cls("NSMutableArray"), "alloc"), "init");
    const char *d = cs(MSG0(id, dirURL, "path"));
    void *h = opendir(d);
    typedef struct { u64 ino, off; u16 reclen, namlen; u8 type; char name[1024]; } de_t;
    for (de_t *e; h && (e = (de_t *)readdir(h));) {
        if (e->name[0] == '.') continue;
        if (type && MSG0(u64, type, "length")) { const char *dot = strrchr(e->name, '.'); if (!dot || strcmp(dot + 1, cs(type))) continue; }
        char p[1300];
        snprintf(p, sizeof p, "%s/%s", d, e->name);
        if (!is_dir(p)) continue;
        id b = b_with_path(B(), sel_registerName("bundleWithPath:"), str(p));
        if (b) cx_arr_add(out, b);
    }
    if (h) closedir(h);
    return out;
}
id CFBundleGetLocalInfoDictionary(id b) { return b ? b_localized_info(b, 0) : 0; }
id CFBundleGetDevelopmentRegion(id b) { return b ? b_dev_localization(b, 0) : 0; }
id CFBundleCopySupportFilesDirectoryURL(id b) { char c[1100]; if (!b) return 0; contents_dir(b, c, sizeof c); return ret(url_of(c, 1)); }
id CFBundleCopyPrivateFrameworksURL(id b) { return b ? ret(b_priv_fw_url(b, 0)) : 0; }
id CFBundleCopySharedFrameworksURL(id b) { return b ? ret(b_shared_fw_url(b, 0)) : 0; }
id CFBundleCopySharedSupportURL(id b) { return b ? ret(b_shared_support_url(b, 0)) : 0; }
id CFBundleCopyBuiltInPlugInsURL(id b) { return b ? ret(b_plugins_url(b, 0)) : 0; }
id CFBundleCopyAuxiliaryExecutableURL(id b, id name) { return b ? ret(b_aux_url(b, 0, name)) : 0; }
unsigned char CFBundleGetPackageInfoInDirectory(id url, u32 *type, u32 *creator) {
    char p[1300];
    if (type) *type = 0x3F3F3F3F;
    if (creator) *creator = 0x3F3F3F3F;
    if (!url) return 0;
    const char *d = cs(MSG0(id, url, "path"));
    snprintf(p, sizeof p, "%s/Contents/PkgInfo", d);
    int fd = open(p, 0);
    u8 b[8];
    if (fd >= 0) {
        long n = read(fd, b, 8);
        close(fd);
        if (n == 8) {
            if (type) *type = (u32)b[0] << 24 | (u32)b[1] << 16 | (u32)b[2] << 8 | b[3];
            if (creator) *creator = (u32)b[4] << 24 | (u32)b[5] << 16 | (u32)b[6] << 8 | b[7];
            return 1;
        }
    }
    /* sem PkgInfo: o Info.plist (CFBundlePackageType/CFBundleSignature) */
    snprintf(p, sizeof p, "%s/Contents/Info.plist", d);
    extern id cx_plist_file(const char *path);
    id info = cx_plist_file(p);
    if (!info) return 0;
    id t = MSG(id, info, "objectForKey:", cx_str("CFBundlePackageType")), cr = MSG(id, info, "objectForKey:", cx_str("CFBundleSignature"));
    const char *ts = cs(t), *crs = cs(cr);
    if (type && strlen(ts) == 4) *type = (u32)(u8)ts[0] << 24 | (u32)(u8)ts[1] << 16 | (u32)(u8)ts[2] << 8 | (u8)ts[3];
    if (creator && strlen(crs) == 4) *creator = (u32)(u8)crs[0] << 24 | (u32)(u8)crs[1] << 16 | (u32)(u8)crs[2] << 8 | (u8)crs[3];
    return t != 0;
}
id CFBundleCopyResourceURLsOfType(id b, id type, id sub) { return b ? ret(b_urls_ext(b, 0, type, sub)) : 0; }
id CFBundleCopyResourceURLForLocalization(id b, id name, id type, id sub, id loc) { return b ? ret(b_url_res_sub_loc(b, 0, name, type, sub, loc)) : 0; }
id CFBundleCopyResourceURLsOfTypeForLocalization(id b, id type, id sub, id loc) { return b ? ret(b_urls_ext_loc(b, 0, type, sub, loc)) : 0; }
id CFBundleCopyResourceURLInDirectory(id dirURL, id name, id type, id sub) {
    char out[1500], d[1200];
    if (!dirURL || !name) return 0;
    snprintf(d, sizeof d, "%s", cs(MSG0(id, dirURL, "path")));
    return try_file(d, sub ? cs(sub) : 0, cs(name), cs(type), out, sizeof out) ? ret(url_of(out, is_dir(out))) : 0;
}
id CFBundleCopyResourceURLsOfTypeInDirectory(id dirURL, id type, id sub) {
    if (!dirURL) return 0;
    id out = arr(), seen = MSG0(id, cls("NSMutableSet"), "set");
    list_type(cs(MSG0(id, dirURL, "path")), sub ? cs(sub) : 0, type ? cs(type) : 0, out, seen);
    return ret(to_urls(out));
}
id CFBundleCopyLocalizedString(id b, id key, id value, id table) { return b ? ret(b_localized_string(b, 0, key, value, table)) : ret(value ? value : key); }
id CFBundleCopyBundleLocalizations(id b) { return b ? ret(b_localizations(b, 0)) : 0; }
id CFBundleCopyPreferredLocalizationsFromArray(id locs) { return ret(pref_from_array(locs, user_prefs())); }
id CFBundleCopyLocalizationsForPreferences(id locs, id prefs) { return ret(pref_from_array(locs, prefs ? prefs : user_prefs())); }
id CFBundleCopyLocalizationsForURL(id url) {
    id b = url ? b_with_url(B(), 0, url) : 0;
    return b ? ret(b_localizations(b, 0)) : 0;
}
id CFBundleCopyExecutableArchitectures(id b) { return b ? ret(b_archs(b, 0)) : 0; }
id CFBundleCopyExecutableArchitecturesForURL(id url) {
    id b = url ? b_with_url(B(), 0, url) : 0;
    return b ? ret(b_archs(b, 0)) : 0;
}
unsigned char CFBundlePreflightExecutable(id b, id *err) { return b && b_preflight(b, 0, err); }
unsigned char CFBundleLoadExecutableAndReturnError(id b, id *err) { return b && b_load_err(b, 0, err); }
unsigned char CFBundleLoadExecutable(id b) { return b && b_load(b, 0); }
unsigned char CFBundleIsExecutableLoaded(id b) { return b && b_is_loaded(b, 0); }
void CFBundleUnloadExecutable(id b) { if (b) b_unload(b, 0); }
void *CFBundleGetDataPointerForName(id b, id name) {
    if (!b || !name) return 0;
    b_load(b, 0);
    bstate_t *st = state(b);
    return dlsym(st->handle ? st->handle : (void *)-2, cs(name));
}
void CFBundleGetFunctionPointersForNames(id b, id names, void **out) {
    u64 n = names ? MSG0(u64, names, "count") : 0;
    for (u64 i = 0; i < n; i++) out[i] = CFBundleGetDataPointerForName(b, MSG(id, names, "objectAtIndex:", i));
}
void CFBundleGetDataPointersForNames(id b, id names, void **out) { CFBundleGetFunctionPointersForNames(b, names, out); }
id CFBundleGetPlugIn(id b) { return b; }
int CFBundleOpenBundleResourceMap(id b) { (void)b; return -1; }   /* Resource Manager não existe no macOS de 64 bits */
void CFBundleCloseBundleResourceMap(id b, int ref) { (void)b; (void)ref; }

/* ---- registro: por cima do NSBundle já definido */
void cx_fdn_bundle_init(void) {
    cx_class_t *k = cx_class_named("NSBundle");
    if (!k) return;
    g_orig_with_path = cx_lookup(object_getClass((id)k), sel_registerName("bundleWithPath:"));
    g_orig_init_path = cx_lookup(k, sel_registerName("initWithPath:"));
    g_orig_info_key = cx_lookup(k, sel_registerName("objectForInfoDictionaryKey:"));
#define CMB(s, f) cx_add_cmethod(k, s, (void *)(f))
#define IMB(s, f) cx_add_method(k, s, (void *)(f))
    CMB("bundleWithPath:", b_with_path); CMB("bundleWithURL:", b_with_url); CMB("bundleForClass:", b_for_class);
    CMB("bundleWithIdentifier:", b_with_identifier); CMB("allBundles", b_all_bundles); CMB("allFrameworks", b_all_frameworks);
    CMB("preferredLocalizationsFromArray:", b_s_pref_from); CMB("preferredLocalizationsFromArray:forPreferences:", b_s_pref_from_for);
    CMB("pathForResource:ofType:inDirectory:", b_s_path_res); CMB("pathsForResourcesOfType:inDirectory:", b_s_paths_type);
    CMB("URLForResource:withExtension:subdirectory:inBundleWithURL:", b_s_url_res);
    CMB("URLsForResourcesWithExtension:subdirectory:inBundleWithURL:", b_s_urls_ext);
    IMB("initWithURL:", b_init_url);
    IMB("bundleURL", b_bundle_url); IMB("resourceURL", b_resource_url); IMB("executableURL", b_exec_url);
    IMB("privateFrameworksPath", b_priv_fw_path); IMB("privateFrameworksURL", b_priv_fw_url);
    IMB("sharedFrameworksPath", b_shared_fw_path); IMB("sharedFrameworksURL", b_shared_fw_url);
    IMB("sharedSupportPath", b_shared_support_path); IMB("sharedSupportURL", b_shared_support_url);
    IMB("builtInPlugInsPath", b_plugins_path); IMB("builtInPlugInsURL", b_plugins_url);
    IMB("appStoreReceiptURL", b_receipt_url);
    IMB("pathForAuxiliaryExecutable:", b_aux_path); IMB("URLForAuxiliaryExecutable:", b_aux_url);
    IMB("localizations", b_localizations); IMB("developmentLocalization", b_dev_localization);
    IMB("preferredLocalizations", b_preferred_localizations);
    IMB("pathForResource:ofType:", b_path_res); IMB("pathForResource:ofType:inDirectory:", b_path_res_dir);
    IMB("pathForResource:ofType:inDirectory:forLocalization:", b_path_res_dir_loc);
    IMB("URLForResource:withExtension:", b_url_res); IMB("URLForResource:withExtension:subdirectory:", b_url_res_sub);
    IMB("URLForResource:withExtension:subdirectory:localization:", b_url_res_sub_loc);
    IMB("pathsForResourcesOfType:inDirectory:", b_paths_type); IMB("pathsForResourcesOfType:inDirectory:forLocalization:", b_paths_type_loc);
    IMB("URLsForResourcesWithExtension:subdirectory:", b_urls_ext);
    IMB("URLsForResourcesWithExtension:subdirectory:localization:", b_urls_ext_loc);
    IMB("localizedStringForKey:value:table:", b_localized_string);
    IMB("localizedInfoDictionary", b_localized_info); IMB("objectForInfoDictionaryKey:", b_object_for_info_key);
    IMB("load", b_load); IMB("loadAndReturnError:", b_load_err); IMB("isLoaded", b_is_loaded); IMB("unload", b_unload);
    IMB("preflightAndReturnError:", b_preflight); IMB("classNamed:", b_class_named); IMB("principalClass", b_principal);
    IMB("executableArchitectures", b_archs); IMB("description", b_desc);
#undef CMB
#undef IMB
}
