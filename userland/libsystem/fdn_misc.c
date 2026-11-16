/*
 * mOS libSystem — funções C da Foundation/CoreFoundation que o corpus pedia e não existiam:
 * NSRectFromString/NSPointFromString/NSSizeFromString (os CG são do UIKit), NSFileTypeForHFSTypeCode, NSHFSTypeCodeFromFileType,
 * NSHFSTypeOfFile, CFStringGetSystemEncoding, CFPreferencesSetMultiple/CopyMultiple e CFURLEnumerator.
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
IMP cx_lookup(cx_class_t *cls, SEL sel);
double strtod(const char *s, char **e);
long getxattr(const char *p, const char *n, void *v, size_t s, u32 pos, int o);
id CFPreferencesCopyValue(id key, id app, id user, id host);
void CFPreferencesSetValue(id key, id value, id app, id user, id host);
id CFPreferencesCopyKeyList(id app, id user, id host);
char *strncpy(char *d, const char *s, size_t n);

static id cls(const char *n) { return (id)cx_class_named(n); }

/* ---- NSRect/NSPoint/NSSize de texto: os números na ordem, o que faltar é 0 (como a Foundation) */
typedef struct { double x, y; } fm_pt_t;
typedef struct { double w, h; } fm_sz_t;
typedef struct { fm_pt_t o; fm_sz_t s; } fm_rc_t;
static int numbers(id s, double *v, int max) {
    const char *p = s ? cx_cstr(s) : "";
    int n = 0;
    while (*p && n < max) {
        if ((*p >= '0' && *p <= '9') || ((*p == '-' || *p == '+' || *p == '.') && ((p[1] >= '0' && p[1] <= '9') || p[1] == '.'))) {
            char *e;
            double d = strtod(p, &e);
            if (e == p) { p++; continue; }
            v[n++] = d;
            p = e;
        } else p++;
    }
    for (int i = n; i < max; i++) v[i] = 0;
    return n;
}
fm_rc_t NSRectFromString(id s) { double v[4]; numbers(s, v, 4); fm_rc_t r = {{v[0], v[1]}, {v[2], v[3]}}; return r; }
fm_pt_t NSPointFromString(id s) { double v[2]; numbers(s, v, 2); fm_pt_t r = {v[0], v[1]}; return r; }
fm_sz_t NSSizeFromString(id s) { double v[2]; numbers(s, v, 2); fm_sz_t r = {v[0], v[1]}; return r; }

/* ---- códigos HFS: "'TEXT'" com as aspas simples, como a Foundation */
id NSFileTypeForHFSTypeCode(u32 code) {
    char b[8] = {'\'', (char)(code >> 24), (char)(code >> 16), (char)(code >> 8), (char)code, '\'', 0, 0};
    return objc_autorelease(cx_str_n(b, 6));
}
u32 NSHFSTypeCodeFromFileType(id t) {
    const char *s = t ? cx_cstr(t) : "";
    if (strlen(s) != 6 || s[0] != '\'' || s[5] != '\'') return 0;
    return (u32)(u8)s[1] << 24 | (u32)(u8)s[2] << 16 | (u32)(u8)s[3] << 8 | (u8)s[4];
}
id NSHFSTypeOfFile(id path) {
    u8 fi[32];
    if (!path) return 0;
    /* com.apple.FinderInfo: os 4 primeiros bytes são o tipo (big-endian) */
    long n = getxattr(cx_cstr(path), "com.apple.FinderInfo", fi, sizeof fi, 0, 0);
    if (access(cx_cstr(path), 0) != 0) return 0;
    u32 code = n >= 4 ? (u32)fi[0] << 24 | (u32)fi[1] << 16 | (u32)fi[2] << 8 | fi[3] : 0;
    if (!code) return objc_autorelease(cx_str("''"));
    return NSFileTypeForHFSTypeCode(code);
}

/* ---- CFString: a codificação "do sistema" do macOS é MacRoman (kCFStringEncodingMacRoman = 0) */
u32 CFStringGetSystemEncoding(void) { return 0; }

/* ---- CFPreferences em lote */
void CFPreferencesSetMultiple(id set, id remove, id app, id user, id host) {
    id keys = set ? MSG0(id, set, "allKeys") : 0;
    u64 n = keys ? MSG0(u64, keys, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        CFPreferencesSetValue(k, MSG(id, set, "objectForKey:", k), app, user, host);
    }
    u64 r = remove ? MSG0(u64, remove, "count") : 0;
    for (u64 i = 0; i < r; i++) CFPreferencesSetValue(MSG(id, remove, "objectAtIndex:", i), 0, app, user, host);
}
id CFPreferencesCopyMultiple(id keys, id app, id user, id host) {
    id d = MSG0(id, MSG0(id, cls("NSMutableDictionary"), "alloc"), "init");
    id list = keys ? keys : CFPreferencesCopyKeyList(app, user, host);
    u64 n = list ? MSG0(u64, list, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, list, "objectAtIndex:", i);
        id v = CFPreferencesCopyValue(k, app, user, host);
        if (v) { MSG(void, d, "setObject:forKey:", v, k); objc_release(v); }
    }
    if (!keys && list) objc_release(list);
    return d;
}

/* ---- CFURLEnumerator: diretório (recursivo ou não, pré/pós-ordem) e volumes montados */
enum { EO_RECURSIVE = 1, EO_SKIP_INVISIBLES = 2, EO_FILE_REFS = 4, EO_SKIP_PACKAGES = 8, EO_DIRS_PRE = 16,
       EO_DIRS_POST = 32, EO_RELATIVE = 64 };
enum { ER_SUCCESS = 1, ER_END = 2, ER_ERROR = 3, ER_POST = 4 };
typedef struct { u64 ino, off; u16 reclen, namlen; u8 type; char name[1024]; } de_t;
typedef struct frame { void *dir; char path[1024]; struct frame *up; } frame_t;
typedef struct {
    cx_class_t *isa;
    frame_t *top;
    unsigned long opts;
    int level, volumes, vol_done, skip_next;
    char last[1024];
    int last_dir;
    char root[1024];
    id keys;
} en_t;
static cx_class_t en_c, en_m;
static int is_dir_path(const char *p) { void *d = opendir(p); if (!d) return 0; closedir(d); return 1; }
static int is_package(const char *p) {
    static const char *const suf[] = {".app", ".bundle", ".framework", ".plugin", ".kext", ".pkg", ".rtfd", ".xpc", ".appex", 0};
    size_t l = strlen(p);
    for (int i = 0; suf[i]; i++) { size_t s = strlen(suf[i]); if (l > s && !strcmp(p + l - s, suf[i])) return 1; }
    return 0;
}
static void push(en_t *e, const char *path) {
    frame_t *f = calloc(1, sizeof *f);
    if (!f) return;
    f->dir = opendir(path);
    strncpy(f->path, path, sizeof f->path - 1);
    f->up = e->top;
    e->top = f;
}
static void pop(en_t *e) {
    frame_t *f = e->top;
    if (!f) return;
    if (f->dir) closedir(f->dir);
    e->top = f->up;
    free(f);
}
id CFURLEnumeratorCreateForDirectoryURL(void *alloc, id url, unsigned long opts, id keys) {
    (void)alloc;
    if (!url) return 0;
    en_t *e = (en_t *)cx_alloc_instance(&en_c);
    e->opts = opts;
    e->keys = keys ? objc_retain(keys) : 0;
    strncpy(e->root, cx_cstr(MSG0(id, url, "path")), sizeof e->root - 1);
    push(e, e->root);
    return (id)e;
}
id CFURLEnumeratorCreateForMountedVolumes(void *alloc, unsigned long opts, id keys) {
    (void)alloc;
    en_t *e = (en_t *)cx_alloc_instance(&en_c);
    e->opts = opts;
    e->volumes = 1;
    e->keys = keys ? objc_retain(keys) : 0;
    return (id)e;
}
static id make_url(en_t *e, const char *p, int dir) {
    if (e->opts & EO_RELATIVE) {
        size_t rl = strlen(e->root);
        const char *rel = !strncmp(p, e->root, rl) ? p + rl + (p[rl] == '/') : p;
        id base = MSG(id, cls("NSURL"), "fileURLWithPath:isDirectory:", cx_str(e->root), (BOOL)1);
        return MSG(id, cls("NSURL"), "fileURLWithPath:isDirectory:relativeToURL:", cx_str(rel), (BOOL)dir, base);
    }
    return MSG(id, cls("NSURL"), "fileURLWithPath:isDirectory:", cx_str(p), (BOOL)dir);
}
long CFURLEnumeratorGetNextURL(id en, id *url, id *err) {
    en_t *e = (en_t *)en;
    if (url) *url = 0;
    if (err) *err = 0;
    if (!e) return ER_END;
    if (e->volumes) {
        if (e->vol_done) return ER_END;
        e->vol_done = 1;
        if (url) *url = MSG(id, cls("NSURL"), "fileURLWithPath:isDirectory:", cx_str("/"), (BOOL)1);
        return ER_SUCCESS;
    }
    /* o último devolvido era diretório e o pedido é recursivo: desce, salvo skipDescendents */
    if (e->last_dir && (e->opts & EO_RECURSIVE) && !e->skip_next && !((e->opts & EO_SKIP_PACKAGES) && is_package(e->last))) {
        push(e, e->last);
        e->level++;
    }
    e->last_dir = 0;
    e->skip_next = 0;
    for (;;) {
        frame_t *f = e->top;
        if (!f) return ER_END;
        if (!f->dir) {
            int had = f->up != 0;
            char p[1024];
            strncpy(p, f->path, sizeof p - 1); p[1023] = 0;
            pop(e);
            if (!had) {
                if (err) *err = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSCocoaErrorDomain"), (i64)260, (id)0);
                return ER_ERROR;
            }
            e->level--;
            continue;
        }
        de_t *d = (de_t *)readdir(f->dir);
        if (!d) {
            int had = f->up != 0;
            char p[1024];
            strncpy(p, f->path, sizeof p - 1); p[1023] = 0;
            pop(e);
            if (!had) return ER_END;
            e->level--;
            if (e->opts & EO_DIRS_POST) { if (url) *url = make_url(e, p, 1); return ER_POST; }
            continue;
        }
        if (!strcmp(d->name, ".") || !strcmp(d->name, "..")) continue;
        if ((e->opts & EO_SKIP_INVISIBLES) && d->name[0] == '.') continue;
        char p[2100];
        snprintf(p, sizeof p, "%s/%s", f->path, d->name);
        int dir = is_dir_path(p);
        strncpy(e->last, p, sizeof e->last - 1);
        e->last_dir = dir;
        if (url) *url = make_url(e, p, dir);
        return ER_SUCCESS;
    }
}
void CFURLEnumeratorSkipDescendents(id en) { if (en) ((en_t *)en)->skip_next = 1; }
long CFURLEnumeratorGetDescendentLevel(id en) { return en ? ((en_t *)en)->level + 1 : 0; }
unsigned char CFURLEnumeratorGetSourceDidChange(id en) { (void)en; return 0; }
u64 CFURLEnumeratorGetTypeID(void) { return 0x52; }
static void en_dealloc(id s, SEL c) {
    (void)c;
    en_t *e = (en_t *)s;
    while (e->top) pop(e);
    if (e->keys) objc_release(e->keys);
    IMP sup = cx_lookup(cx_class_named("NSObject"), sel_registerName("dealloc"));
    if (sup) sup(s, sel_registerName("dealloc"));
}
void cx_fdn_misc_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    if (!ob || cx_class_named("__NSCFURLEnumerator") == &en_c) return;
    cx_define_class(&en_c, &en_m, "__NSCFURLEnumerator", ob, (u32)(sizeof(en_t) - 8));
    cx_add_method(&en_c, "dealloc", (void *)en_dealloc);
}
