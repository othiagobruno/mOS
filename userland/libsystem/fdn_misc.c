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

/* ---- memória: página, zona padrão e cópia de páginas */
extern u64 vm_page_size;
void *memmove(void *d, const void *s, size_t n);
int munmap(void *addr, size_t len);
u64 NSPageSize(void) { return vm_page_size; }
u64 NSLogPageSize(void) { u64 p = vm_page_size, l = 0; while (p > 1) { p >>= 1; l++; } return l; }
u64 NSRoundUpToMultipleOfPageSize(u64 n) { u64 p = vm_page_size; return (n + p - 1) & ~(p - 1); }
u64 NSRoundDownToMultipleOfPageSize(u64 n) { return n & ~(vm_page_size - 1); }
void NSCopyMemoryPages(const void *src, void *dst, u64 n) { if (src && dst && n) memmove(dst, src, n); }
/* NSZone: a Foundation moderna ignora zonas; um endereço fixo e não nulo basta (NSZoneName devolve "default") */
static struct { void *reserved[16]; } default_zone;
void *NSDefaultMallocZone(void) { return &default_zone; }
/* acima de todas as NSFoundationVersionNumber10_* publicadas, para os testes de versão passarem */
double NSFoundationVersionNumber = 2048.0;

/* ---- NSDivideRect: corta 'amount' a partir da borda (MinX 0, MinY 1, MaxX 2, MaxY 3) */
void NSDivideRect(fm_rc_t in, fm_rc_t *slice, fm_rc_t *rem, double amount, u64 edge) {
    fm_rc_t s = in, r = in, z = {{0, 0}, {0, 0}};
    if (in.s.w <= 0 || in.s.h <= 0) { if (slice) *slice = z; if (rem) *rem = z; return; }
    if (amount < 0) amount = 0;
    switch (edge) {
    case 0: case 2:
        if (amount > in.s.w) amount = in.s.w;
        s.s.w = amount; r.s.w = in.s.w - amount;
        if (edge == 0) r.o.x = in.o.x + amount; else s.o.x = in.o.x + in.s.w - amount;
        break;
    default:
        if (amount > in.s.h) amount = in.s.h;
        s.s.h = amount; r.s.h = in.s.h - amount;
        if (edge == 1) r.o.y = in.o.y + amount; else s.o.y = in.o.y + in.s.h - amount;
        break;
    }
    if (slice) *slice = s;
    if (rem) *rem = r;
}

/* ---- NSDataDeallocatorFree/Unmap/VM: blocos globais (void (^)(void *bytes, NSUInteger length)) */
extern char _NSConcreteGlobalBlock[];
int vm_deallocate(u32 task, u64 addr, u64 size);
u32 mach_task_self(void);
typedef struct { void *isa; int flags, reserved; void *invoke; const void *desc; } gblk_t;
static const struct { u64 reserved, size; } gblk_desc = {0, sizeof(gblk_t)};
static void dealloc_free(void *b, void *p, u64 n) { (void)b; (void)n; free(p); }
static void dealloc_unmap(void *b, void *p, u64 n) { (void)b; if (p) munmap(p, n); }
static void dealloc_vm(void *b, void *p, u64 n) { (void)b; if (p) vm_deallocate(mach_task_self(), (u64)p, n); }
static gblk_t blk_free = {_NSConcreteGlobalBlock, 1 << 28, 0, (void *)dealloc_free, &gblk_desc};
static gblk_t blk_unmap = {_NSConcreteGlobalBlock, 1 << 28, 0, (void *)dealloc_unmap, &gblk_desc};
static gblk_t blk_vm = {_NSConcreteGlobalBlock, 1 << 28, 0, (void *)dealloc_vm, &gblk_desc};
void *const NSDataDeallocatorFree = &blk_free;
void *const NSDataDeallocatorUnmap = &blk_unmap;
void *const NSDataDeallocatorVM = &blk_vm;

/* ---- CF: depuração, hash e coleções */
long write(int fd, const void *b, size_t n);
void CFShow(id obj) {
    const char *t = "(null)";
    id d = 0;
    if (obj) {
        d = MSG(BOOL, obj, "isKindOfClass:", cls("NSString")) ? obj : MSG0(id, obj, "description");
        t = d ? cx_cstr(d) : "";
    }
    write(2, t, strlen(t));
    write(2, "\n", 1);
}
/* FNV-1a de 64 bits (o CF só promete um hash estável no processo) */
u64 CFHashBytes(const u8 *b, long n) { u64 h = 1469598103934665603ULL; for (long i = 0; b && i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; } return h; }
u64 CFStringHashNSString(id s) { return s ? MSG0(u64, s, "hash") : 0; }
u64 CFStringHashCString(const u8 *b, long n) {
    id s = cx_str_n((const char *)b, (u64)(n < 0 ? 0 : n));
    u64 h = MSG0(u64, s, "hash");
    objc_release(s);
    return h;
}
void CFArrayExchangeValuesAtIndices(id a, long i, long j) {
    if (!a || i == j) return;
    id x = objc_retain(MSG(id, a, "objectAtIndex:", (u64)i)), y = objc_retain(MSG(id, a, "objectAtIndex:", (u64)j));
    MSG(void, a, "replaceObjectAtIndex:withObject:", (u64)i, y);
    MSG(void, a, "replaceObjectAtIndex:withObject:", (u64)j, x);
    objc_release(x); objc_release(y);
}
typedef struct { long location, length; } cf_range_t;
void CFArrayReplaceValues(id a, cf_range_t r, const id *vals, long n) {
    if (!a) return;
    for (long k = 0; k < r.length; k++) MSG(void, a, "removeObjectAtIndex:", (u64)r.location);
    for (long k = 0; k < n; k++) MSG(void, a, "insertObject:atIndex:", vals[k], (u64)(r.location + k));
}
unsigned char CFDictionaryContainsValue(id d, id v) {
    if (!d) return 0;
    id vals = MSG0(id, d, "allValues");
    u64 n = vals ? MSG0(u64, vals, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id x = MSG(id, vals, "objectAtIndex:", i);
        if (x == v || (x && v && MSG(BOOL, x, "isEqual:", v))) return 1;
    }
    return 0;
}
double CFRunLoopTimerGetInterval(void *t);
unsigned char CFRunLoopTimerDoesRepeat(void *t) { return t && CFRunLoopTimerGetInterval(t) > 0; }

/* ---- CFString: valores numéricos, comparação com locale, sequências compostas, buffers externos */
long strtol(const char *s, char **e, int b);
int CFStringGetIntValue(id s) {
    long v = s ? strtol(cx_cstr(s), 0, 10) : 0;
    return v > 2147483647L ? 2147483647 : v < -2147483647L - 1 ? (int)(-2147483647L - 1) : (int)v;
}
double CFStringGetDoubleValue(id s) { return s ? strtod(cx_cstr(s), 0) : 0; }
id CFStringCreateMutableWithExternalCharactersNoCopy(void *alloc, const u16 *chars, long n, long cap, void *ext) {
    (void)alloc; (void)cap; (void)ext;
    id m = MSG0(id, cls("NSMutableString"), "alloc");
    return chars && n > 0 ? MSG(id, m, "initWithCharacters:length:", chars, (u64)n) : MSG0(id, m, "init");
}
void CFStringSetExternalCharactersNoCopy(id s, const u16 *chars, long n, long cap) {
    (void)cap;
    if (!s) return;
    id t = chars && n > 0 ? MSG(id, MSG0(id, cls("NSString"), "alloc"), "initWithCharacters:length:", chars, (u64)n) : cx_str("");
    MSG(void, s, "setString:", t);
    objc_release(t);
}

/* ---- CFURL: caminho relativo a uma base e FSRef */
id CFURLCreateFromFileSystemRepresentation(void *alloc, const u8 *buf, long n, u8 dir);
id CFURLCreateFromFileSystemRepresentationRelativeToBase(void *alloc, const u8 *buf, long n, u8 dir, id base) {
    if (!buf) return 0;
    if (!base || (n > 0 && buf[0] == '/')) return CFURLCreateFromFileSystemRepresentation(alloc, buf, n, dir);
    char p[2048];
    long l = n < (long)sizeof p - 1 ? n : (long)sizeof p - 1;
    memcpy(p, buf, (size_t)l); p[l] = 0;
    char full[4096];
    const char *bp = cx_cstr(MSG0(id, base, "path"));
    snprintf(full, sizeof full, "%s%s%s", bp, *bp && bp[strlen(bp) - 1] == '/' ? "" : "/", p);
    return CFURLCreateFromFileSystemRepresentation(alloc, (const u8 *)full, (long)strlen(full), dir);
}
