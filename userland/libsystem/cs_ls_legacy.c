/*
 * mOS libSystem — LaunchServices e File Manager antigos (FSRef) que os apps do corpus ainda ligam:
 *  - LSGetApplicationForInfo/ForItem, LSFindApplicationForInfo, LSCopyApplicationForMIMEType, LSCopyItemAttribute/
 *    SetItemAttribute, LSCopyItemInfoForRef/URL, LSCopyDisplayName/KindString*, LSGetExtensionInfo, LSOpenFSRef,
 *    LSOpenFromRefSpec, LSOpenApplication, LSOpenURLsWithRole, e o resto do LSSharedFileList;
 *  - aliases (AliasHandle = registro "mOSa" com um bookmark de bookmark.c; arquivos de alias do Finder pelo mesmo
 *    formato de cx_alias_*), FSResolveAliasFile*, FSCopyObjectSync, FSGetVolumeInfo/Parms, FSEjectVolumeSync,
 *    FSGetHFSUniStrFromString, PBMakeFSRefUnicodeSync, FNNotify*, CSBackup*, CSDiskSpace*, ReadLocation e as
 *    conversões de data UCConvert*.
 * Tudo por cima das APIs públicas já existentes (FSPathMakeRef/FSRefMakePath de filemgr.c, openurl.c, bookmark.c).
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"

typedef int OSStatus;
typedef short OSErr;
typedef unsigned OSType;
typedef unsigned long ItemCount;
typedef unsigned long UniCharCount;
typedef struct { u8 hidden[80]; } FSRef;
typedef struct { u16 length; u16 unicode[255]; } HFSUniStr255;
typedef char **Handle;
id objc_retain(id o);
void objc_release(id o);
void CFRelease(id o);
id CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(id a, id v);
extern const char kCFTypeArrayCallBacks[];
OSStatus FSPathMakeRef(const u8 *path, FSRef *ref, u8 *isDir);
OSStatus FSRefMakePath(const FSRef *ref, u8 *path, u32 max);
id UTTypeCreatePreferredIdentifierForTag(id cls_, id tag, id conf);
id UTTypeCopyDescription(id uti);
id UTTypeCopyPreferredTagWithClass(id uti, id cls_);
id LSCopyDefaultApplicationURLForContentType(id type, u32 roles, id *err);
id LSCopyDefaultApplicationURLForURL(id url, u32 roles, id *err);
id LSCopyApplicationURLsForBundleIdentifier(id ident, id *err);
int cx_ls_open_urls(id app, id urls);
int cx_ls_type_of_file(const char *path, char *out, size_t cap);
int cx_ls_type_for_tag(const char *cls_, const char *tag, const char *conf, char *out, size_t cap);
int cx_alias_is(const char *abs);
long cx_alias_read(const char *path, u8 **out);
long cx_book_create(const char *path, unsigned opts, int was_ref, u8 **out);
int cx_book_resolve(const u8 *d, long n, unsigned opts, char *out, size_t cap, int *stale);
Handle NewHandle(long size);
long GetHandleSize(Handle h);
void SetHandleSize(Handle h, long size);
int copyfile(const char *from, const char *to, void *state, u32 flags);
long getxattr(const char *p, const char *n, void *v, size_t s, u32 pos, int o);
int setxattr(const char *p, const char *n, const void *v, size_t s, u32 pos, int o);
int removexattr(const char *p, const char *n, int o);
int stat(const char *p, void *st);
int lstat(const char *p, void *st);
char *realpath(const char *p, char *out);
long readlink(const char *p, char *b, size_t n);
int utimes(const char *p, const void *tv);
int statfs(const char *p, void *s);
char *strrchr(const char *s, int c);

#define paramErr (-50)
#define fnfErr (-43)
#define dupFNErr (-48)
#define nsvErr (-35)
#define kLSApplicationNotFoundErr (-10814)
#define kLSAttributeNotFoundErr (-10817)
#define kLSAttributeNotSettableErr (-10818)
#define kLSDataUnavailableErr (-10813)

static int ref_path(const FSRef *r, char *out, size_t cap) { return r && !FSRefMakePath(r, (u8 *)out, (u32)cap) ? 0 : -1; }
static id file_url(const char *p) { return p ? MSG(id, (id)cx_class_named("NSURL"), "fileURLWithPath:", cx_str(p)) : 0; }
static const char *url_path(id url, char *buf, size_t cap) {
    if (!url) return 0;
    id p = MSG0(id, url, "path");
    const char *s = p ? cx_cstr(p) : 0;
    if (!s) return 0;
    snprintf(buf, cap, "%s", s);
    return buf;
}
static void give_app(id url, FSRef *ref, id *out) {
    char p[1024];
    if (ref && url_path(url, p, sizeof p)) FSPathMakeRef((const u8 *)p, ref, 0);
    if (out) *out = url ? objc_retain(url) : 0;
}

/* ================================================================ quem abre o quê */
OSStatus LSGetApplicationForInfo(OSType type, OSType creator, id ext, u32 roles, FSRef *ref, id *url) {
    (void)creator;
    if (url) *url = 0;
    id uti = 0;
    if (ext && cx_is_str(ext)) uti = UTTypeCreatePreferredIdentifierForTag(cx_str("public.filename-extension"), ext, 0);
    if (!uti && type) {
        char t[5] = {(char)(type >> 24), (char)(type >> 16), (char)(type >> 8), (char)type, 0};
        uti = UTTypeCreatePreferredIdentifierForTag(cx_str("com.apple.ostype"), cx_str(t), 0);
    }
    if (!uti) return kLSApplicationNotFoundErr;
    id app = LSCopyDefaultApplicationURLForContentType(uti, roles ? roles : 0xffffffffu, 0);
    objc_release(uti);
    if (!app) return kLSApplicationNotFoundErr;
    give_app(app, ref, url);
    objc_release(app);
    return 0;
}
OSStatus LSGetApplicationForItem(const FSRef *item, u32 roles, FSRef *ref, id *url) {
    char p[1024];
    if (url) *url = 0;
    if (ref_path(item, p, sizeof p)) return paramErr;
    id app = LSCopyDefaultApplicationURLForURL(file_url(p), roles ? roles : 0xffffffffu, 0);
    if (!app) return kLSApplicationNotFoundErr;
    give_app(app, ref, url);
    objc_release(app);
    return 0;
}
OSStatus LSCopyApplicationForMIMEType(id mime, u32 roles, id *url) {
    if (!mime || !url) return paramErr;
    *url = 0;
    id uti = UTTypeCreatePreferredIdentifierForTag(cx_str("public.mime-type"), mime, 0);
    if (!uti) return kLSApplicationNotFoundErr;
    *url = LSCopyDefaultApplicationURLForContentType(uti, roles ? roles : 0xffffffffu, 0);
    objc_release(uti);
    return *url ? 0 : kLSApplicationNotFoundErr;
}
/* pelo bundle id primeiro; depois pelo nome em /Applications, /System/Applications e ~/Applications */
OSStatus LSFindApplicationForInfo(OSType creator, id bundleID, id name, FSRef *ref, id *url) {
    (void)creator;
    if (url) *url = 0;
    if (bundleID) {
        id all = LSCopyApplicationURLsForBundleIdentifier(bundleID, 0);
        id first = all && MSG0(u64, all, "count") ? MSG(id, all, "objectAtIndex:", (u64)0) : 0;
        if (first) { give_app(first, ref, url); objc_release(all); return 0; }
        if (all) objc_release(all);
    }
    const char *n = name && cx_is_str(name) ? cx_cstr(name) : 0;
    if (n) {
        const char *home = getenv("HOME");
        char dirs[3][512];
        snprintf(dirs[0], 512, "/Applications");
        snprintf(dirs[1], 512, "/System/Applications");
        snprintf(dirs[2], 512, "%s/Applications", home ? home : "");
        for (int i = 0; i < 3; i++) {
            char p[1024], st[256];
            size_t l = strlen(n);
            snprintf(p, sizeof p, "%s/%s%s", dirs[i], n, l > 4 && !strcmp(n + l - 4, ".app") ? "" : ".app");
            if (!stat(p, st)) { give_app(file_url(p), ref, url); return 0; }
        }
    }
    return kLSApplicationNotFoundErr;
}

/* ================================================================ informações de item */
/* LSItemInfoRecord {LSItemInfoFlags flags; OSType filetype, creator; CFStringRef extension;} */
typedef struct { u32 flags; OSType type, creator; id ext; } ls_info_t;
enum { kLSItemInfoIsPlainFile = 1, kLSItemInfoIsPackage = 2, kLSItemInfoIsApplication = 4, kLSItemInfoIsContainer = 8,
       kLSItemInfoIsAliasFile = 16, kLSItemInfoIsSymlink = 32, kLSItemInfoIsInvisible = 64, kLSItemInfoIsNativeApp = 128,
       kLSItemInfoIsVolume = 512, kLSItemInfoExtensionIsHidden = 0x100000 };
enum { kLSRequestExtension = 1, kLSRequestTypeCreator = 2, kLSRequestBasicFlagsOnly = 4, kLSRequestAppTypeFlags = 8,
       kLSRequestAllFlags = 16, kLSRequestIconAndKind = 32, kLSRequestExtensionFlagsOnly = 64 };
typedef struct { u8 raw[256]; } st_buf;
static u32 st_mode(const st_buf *s) { u16 m; memcpy(&m, s->raw + 4, 2); return m; }   /* struct stat do macOS: st_mode em 4 */
static const char *ext_of(const char *p) {
    const char *b = strrchr(p, '/'), *d = strrchr(p, '.');
    b = b ? b + 1 : p;
    return d && d > b && d[1] ? d + 1 : 0;
}
static OSStatus item_info(const char *p, u32 which, ls_info_t *o) {
    st_buf s, l;
    memset(o, 0, sizeof *o);
    if (lstat(p, &l)) return fnfErr;
    int link = (st_mode(&l) & 0170000) == 0120000;
    if (stat(p, &s)) s = l;
    int dir = (st_mode(&s) & 0170000) == 0040000;
    const char *e = ext_of(p), *base = strrchr(p, '/');
    base = base ? base + 1 : p;
    if (which & (kLSRequestExtension | kLSRequestExtensionFlagsOnly)) o->ext = e ? objc_retain(cx_str(e)) : 0;
    if (!dir) o->flags |= kLSItemInfoIsPlainFile;
    else {
        o->flags |= kLSItemInfoIsContainer;
        char uti[128] = "";
        cx_ls_type_of_file(p, uti, sizeof uti);
        int app = e && !strcmp(e, "app");
        if (app || !strcmp(uti, "com.apple.package") || !strcmp(uti, "com.apple.bundle")) o->flags |= kLSItemInfoIsPackage;
        if (app) o->flags |= kLSItemInfoIsApplication | kLSItemInfoIsNativeApp;
        if (!strcmp(p, "/") || !strncmp(p, "/Volumes/", 9)) o->flags |= kLSItemInfoIsVolume;
    }
    if (link) o->flags |= kLSItemInfoIsSymlink;
    if (cx_alias_is(p)) o->flags |= kLSItemInfoIsAliasFile;
    if (base[0] == '.') o->flags |= kLSItemInfoIsInvisible;
    u8 fi[32];
    if (getxattr(p, "com.apple.FinderInfo", fi, 32, 0, 0) == 32) {
        if (which & kLSRequestTypeCreator) { o->type = (OSType)fi[0] << 24 | fi[1] << 16 | fi[2] << 8 | fi[3]; o->creator = (OSType)fi[4] << 24 | fi[5] << 16 | fi[6] << 8 | fi[7]; }
        if (fi[8] & 0x40) o->flags |= kLSItemInfoIsInvisible;               /* kIsInvisible = 0x4000 */
        if (fi[9] & 0x10) o->flags |= kLSItemInfoExtensionIsHidden;          /* kExtensionHidden = 0x0010 */
    }
    if (dir && (o->flags & kLSItemInfoIsApplication) && (which & kLSRequestTypeCreator)) o->type = 0x4150504c;   /* 'APPL' */
    return 0;
}
OSStatus LSCopyItemInfoForRef(const FSRef *r, u32 which, ls_info_t *out) {
    char p[1024];
    if (!out) return paramErr;
    if (ref_path(r, p, sizeof p)) return paramErr;
    return item_info(p, which, out);
}
OSStatus LSCopyItemInfoForURL(id url, u32 which, ls_info_t *out) {
    char p[1024];
    if (!out || !url_path(url, p, sizeof p)) return paramErr;
    return item_info(p, which, out);
}
/* nome de exibição: o do Finder (sem .app e sem a extensão escondida) */
static id display_name(const char *p) {
    const char *b = strrchr(p, '/');
    b = b && b[1] ? b + 1 : p;
    char n[512];
    snprintf(n, sizeof n, "%s", b);
    ls_info_t in;
    if (!item_info(p, kLSRequestAllFlags, &in) && ((in.flags & kLSItemInfoIsApplication) || (in.flags & kLSItemInfoExtensionIsHidden))) {
        char *d = strrchr(n, '.');
        if (d && d != n) *d = 0;
    }
    for (char *c = n; *c; c++) if (*c == ':') *c = '/';                      /* o ":" do POSIX é "/" na tela */
    return objc_retain(cx_str(n));
}
OSStatus LSCopyDisplayNameForRef(const FSRef *r, id *name) {
    char p[1024];
    if (!name) return paramErr;
    *name = 0;
    if (ref_path(r, p, sizeof p)) return paramErr;
    *name = display_name(p);
    return 0;
}
OSStatus LSCopyDisplayNameForURL(id url, id *name) {
    char p[1024];
    if (!name || !url_path(url, p, sizeof p)) return paramErr;
    *name = display_name(p);
    return 0;
}
/* tipo ("Documento de Texto"...): a descrição do UTI no LaunchServices */
static OSStatus kind_of_path(const char *p, id *kind) {
    char uti[128];
    if (!kind) return paramErr;
    *kind = 0;
    if (!cx_ls_type_of_file(p, uti, sizeof uti) || !uti[0]) return kLSDataUnavailableErr;
    *kind = UTTypeCopyDescription(cx_str(uti));
    return *kind ? 0 : kLSDataUnavailableErr;
}
OSStatus LSCopyKindStringForRef(const FSRef *r, id *kind) { char p[1024]; return ref_path(r, p, sizeof p) ? paramErr : kind_of_path(p, kind); }
OSStatus LSCopyKindStringForURL(id url, id *kind) { char p[1024]; return url_path(url, p, sizeof p) ? kind_of_path(p, kind) : paramErr; }
OSStatus LSCopyKindStringForTypeInfo(OSType type, OSType creator, id ext, id *kind) {
    (void)creator;
    if (!kind) return paramErr;
    *kind = 0;
    id uti = ext ? UTTypeCreatePreferredIdentifierForTag(cx_str("public.filename-extension"), ext, 0) : 0;
    if (!uti && type) {
        char t[5] = {(char)(type >> 24), (char)(type >> 16), (char)(type >> 8), (char)type, 0};
        uti = UTTypeCreatePreferredIdentifierForTag(cx_str("com.apple.ostype"), cx_str(t), 0);
    }
    if (!uti) return kLSDataUnavailableErr;
    *kind = UTTypeCopyDescription(uti);
    objc_release(uti);
    return *kind ? 0 : kLSDataUnavailableErr;
}
/* início da extensão no nome (UTF-16); sem extensão = kLSInvalidExtensionIndex */
OSStatus LSGetExtensionInfo(UniCharCount n, const u16 *name, UniCharCount *start) {
    if (!name || !start) return paramErr;
    *start = (UniCharCount)-1;
    for (UniCharCount i = n; i > 1; i--) {
        if (name[i - 1] == '/' || name[i - 1] == ':') break;
        if (name[i - 1] == '.') { if (i < n && i - 1 > 0) *start = i; break; }
    }
    return 0;
}
/* atributos (kLSItem*) */
extern id kLSItemDisplayName, kLSItemQuarantineProperties;
static id kstr(const char *s) { return cx_str(s); }
static int attr_is(id a, const char *n) { return a && cx_is_str(a) && !strcmp(cx_cstr(a), n); }
id cx_quarantine_props(const char *path);          /* gatekeeper/quarentena (fdn_url_props.c ou similar) */
OSStatus LSCopyItemAttribute(const FSRef *r, u32 roles, id attr, id *value) {
    (void)roles;
    char p[1024], uti[128];
    if (!value || !attr) return paramErr;
    *value = 0;
    if (ref_path(r, p, sizeof p)) return paramErr;
    ls_info_t in;
    OSStatus st = item_info(p, kLSRequestAllFlags | kLSRequestExtension | kLSRequestTypeCreator, &in);
    if (st) return st;
    if (attr_is(attr, "LSItemDisplayName")) *value = display_name(p);
    else if (attr_is(attr, "LSItemContentType")) *value = cx_ls_type_of_file(p, uti, sizeof uti) ? objc_retain(kstr(uti)) : 0;
    else if (attr_is(attr, "LSItemExtension")) *value = in.ext ? objc_retain(in.ext) : 0;
    else if (attr_is(attr, "LSItemDisplayKind")) kind_of_path(p, value);
    else if (attr_is(attr, "LSItemIsInvisible")) *value = objc_retain(cx_num_i((in.flags & kLSItemInfoIsInvisible) != 0));
    else if (attr_is(attr, "LSItemExtensionIsHidden")) *value = objc_retain(cx_num_i((in.flags & kLSItemInfoExtensionIsHidden) != 0));
    else if (attr_is(attr, "LSItemQuarantineProperties")) {
        id url = file_url(p), props = 0;
        if (url && MSG(BOOL, url, "getResourceValue:forKey:error:", &props, kstr("NSURLQuarantinePropertiesKey"), (id *)0) && props)
            *value = objc_retain(props);
    } else {
        if (in.ext) objc_release(in.ext);
        return kLSAttributeNotFoundErr;
    }
    if (in.ext) objc_release(in.ext);
    return *value ? 0 : kLSDataUnavailableErr;
}
/* só a extensão escondida e a quarentena se mudam (como no macOS); o resto não é gravável */
OSStatus LSSetItemAttribute(const FSRef *r, u32 roles, id attr, id value) {
    (void)roles;
    char p[1024];
    if (ref_path(r, p, sizeof p)) return paramErr;
    if (attr_is(attr, "LSItemExtensionIsHidden")) {
        u8 fi[32] = {0};
        getxattr(p, "com.apple.FinderInfo", fi, 32, 0, 0);
        int on = value && MSG0(BOOL, value, "boolValue");
        fi[9] = on ? (u8)(fi[9] | 0x10) : (u8)(fi[9] & ~0x10);
        return setxattr(p, "com.apple.FinderInfo", fi, 32, 0, 0) ? -36 : 0;
    }
    if (attr_is(attr, "LSItemQuarantineProperties")) {
        id url = file_url(p);
        if (!value) { removexattr(p, "com.apple.quarantine", 0); return 0; }
        return url && MSG(BOOL, url, "setResourceValue:forKey:error:", value, kstr("NSURLQuarantinePropertiesKey"), (id *)0) ? 0 : -36;
    }
    return kLSAttributeNotSettableErr;
}

/* ================================================================ abrir */
static id urls_of_refs(const FSRef *refs, ItemCount n) {
    id a = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    for (ItemCount i = 0; i < n; i++) { char p[1024]; if (!ref_path(&refs[i], p, sizeof p)) CFArrayAppendValue(a, file_url(p)); }
    return a;
}
OSStatus LSOpenFSRef(const FSRef *r, FSRef *launched) {
    char p[1024];
    if (ref_path(r, p, sizeof p)) return paramErr;
    id a = urls_of_refs(r, 1);
    int st = cx_ls_open_urls(0, a);
    CFRelease(a);
    if (!st && launched) {
        id app = LSCopyDefaultApplicationURLForURL(file_url(p), 0xffffffffu, 0);
        if (app) { give_app(app, launched, 0); objc_release(app); }
        else memcpy(launched, r, sizeof *r);                                 /* era um app: ele mesmo */
    }
    return st;
}
/* LSLaunchFSRefSpec {const FSRef *appRef; ItemCount numDocs; const FSRef *itemRefs; const AEDesc *passThru;
 * LSLaunchFlags flags; void *asyncRefCon} */
typedef struct { const FSRef *app; ItemCount n; const FSRef *items; const void *pass; u32 flags; void *ref; } ls_fsspec_t;
OSStatus LSOpenFromRefSpec(const ls_fsspec_t *s, FSRef *launched) {
    if (!s || (!s->app && !s->n)) return paramErr;
    char p[1024];
    id app = 0;
    if (s->app) { if (ref_path(s->app, p, sizeof p)) return paramErr; app = file_url(p); }
    id items = s->n ? urls_of_refs(s->items, s->n) : 0;
    int st = cx_ls_open_urls(app, items ? items : CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));
    if (items) CFRelease(items);
    if (!st && launched && s->app) memcpy(launched, s->app, sizeof *launched);
    return st;
}
/* LSApplicationParameters {CFIndex version; LSLaunchFlags flags; const FSRef *application; void *asyncLaunchRefCon;
 * CFDictionaryRef environment; CFArrayRef argv; AppleEvent *initialEvent} */
typedef struct { long version; u32 flags; const FSRef *app; void *ref; id env; id argv; void *ev; } ls_appparams_t;
typedef struct { u32 hi, lo; } psn_t;
OSStatus LSOpenApplication(const ls_appparams_t *ap, psn_t *psn) {
    char p[1024];
    if (!ap || ref_path(ap->app, p, sizeof p)) return paramErr;
    int st = cx_ls_open_urls(file_url(p), CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));
    if (!st && psn) { psn->hi = 0; psn->lo = 0; }                            /* o mOS identifica processos por pid */
    return st;
}
OSStatus LSOpenURLsWithRole(id urls, u32 roles, const void *aekey, const ls_appparams_t *ap, psn_t *psns, long max) {
    (void)roles; (void)aekey;
    char p[1024];
    id app = ap && ap->app && !ref_path(ap->app, p, sizeof p) ? file_url(p) : 0;
    if (!urls && !app) return paramErr;
    int st = cx_ls_open_urls(app, urls ? urls : CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));
    for (long i = 0; !st && psns && i < max; i++) psns[i].hi = psns[i].lo = 0;
    return st;
}

/* ================================================================ LSSharedFileList (resto) */
id LSSharedFileListItemCopyResolvedURL(id item, u32 flags, id *err);
id LSSharedFileListCopySnapshot(id list, u32 *seed);
OSStatus LSSharedFileListItemRemove(id list, id item);
OSStatus LSSharedFileListItemResolve(id item, u32 flags, id *url, FSRef *ref) {
    if (url) *url = 0;
    id u = LSSharedFileListItemCopyResolvedURL(item, flags, 0);
    if (!u) return fnfErr;
    char p[1024];
    if (ref && url_path(u, p, sizeof p)) FSPathMakeRef((const u8 *)p, ref, 0);
    if (url) *url = u; else objc_release(u);
    return 0;
}
id LSSharedFileListItemCopyDisplayName(id item) {
    id u = LSSharedFileListItemCopyResolvedURL(item, 0, 0);
    char p[1024];
    id n = u && url_path(u, p, sizeof p) ? display_name(p) : 0;
    if (u) objc_release(u);
    return n;
}
/* observadores: a lista de itens de login só muda por estas funções no próprio processo; o observador é chamado
 * depois de cada mudança feita aqui (RemoveAllItems) e pelo LSSharedFileListInsertItemURL não (é de vscode_frameworks.c) */
typedef void (*lsfl_cb)(id list, void *ctx);
static struct { id list; lsfl_cb cb; void *ctx; } g_obs[16];
void LSSharedFileListAddObserver(id list, void *rl, id mode, lsfl_cb cb, void *ctx) {
    (void)rl; (void)mode;
    for (int i = 0; i < 16; i++) if (!g_obs[i].cb) { g_obs[i].list = list; g_obs[i].cb = cb; g_obs[i].ctx = ctx; return; }
}
void LSSharedFileListRemoveObserver(id list, void *rl, id mode, lsfl_cb cb, void *ctx) {
    (void)rl; (void)mode;
    for (int i = 0; i < 16; i++) if (g_obs[i].list == list && g_obs[i].cb == cb && g_obs[i].ctx == ctx) g_obs[i].cb = 0;
}
OSStatus LSSharedFileListRemoveAllItems(id list) {
    id snap = LSSharedFileListCopySnapshot(list, 0);
    u64 n = snap ? MSG0(u64, snap, "count") : 0;
    OSStatus st = 0;
    for (u64 i = 0; i < n; i++) { OSStatus e = LSSharedFileListItemRemove(list, MSG(id, snap, "objectAtIndex:", i)); if (e) st = e; }
    if (snap) objc_release(snap);
    for (int i = 0; i < 16; i++) if (g_obs[i].cb && g_obs[i].list == list) g_obs[i].cb(list, g_obs[i].ctx);
    return st;
}
/* propriedades da lista: só kLSSharedFileListRecentItemsMaxAmount tem valor (10, o padrão do macOS) */
id LSSharedFileListCopyProperty(id list, id name) {
    (void)list;
    return attr_is(name, "com.apple.LSSharedFileList.RecentItemsMaxAmount") ? objc_retain(cx_num_i(10)) : 0;
}

/* ================================================================ aliases */
/* AliasHandle: "mOSa", u32 tamanho, bookmark (bookmark.c). GetAliasSizeFromPtr lê o tamanho do cabeçalho. */
static Handle alias_make(const char *target) {
    u8 *bk = 0;
    long n = cx_book_create(target, 0, 0, &bk);
    if (n <= 0) return 0;
    Handle h = NewHandle(8 + n);
    if (!h) { free(bk); return 0; }
    u32 len = (u32)n;
    memcpy(*h, "mOSa", 4); memcpy(*h + 4, &len, 4); memcpy(*h + 8, bk, (size_t)n);
    free(bk);
    return h;
}
static int alias_ok(Handle h) { return h && *h && GetHandleSize(h) >= 8 && !memcmp(*h, "mOSa", 4); }
OSErr FSNewAlias(const FSRef *from, const FSRef *target, Handle *alias) {
    (void)from;
    char p[1024];
    if (!alias || ref_path(target, p, sizeof p)) return paramErr;
    *alias = alias_make(p);
    return *alias ? 0 : fnfErr;
}
OSErr FSNewAliasMinimal(const FSRef *target, Handle *alias) { return FSNewAlias(0, target, alias); }
OSErr FSResolveAliasWithMountFlags(const FSRef *from, Handle alias, FSRef *target, u8 *changed, unsigned long flags) {
    (void)from; (void)flags;
    if (!alias_ok(alias) || !target) return paramErr;
    u32 len;
    memcpy(&len, *alias + 4, 4);
    char p[1024];
    int stale = 0;
    if (cx_book_resolve((const u8 *)*alias + 8, len, 0, p, sizeof p, &stale) < 0) return fnfErr;
    if (changed) *changed = stale != 0;
    return (OSErr)FSPathMakeRef((const u8 *)p, target, 0);
}
OSErr FSResolveAlias(const FSRef *from, Handle alias, FSRef *target, u8 *changed) { return FSResolveAliasWithMountFlags(from, alias, target, changed, 0); }
OSErr FSFollowFinderAlias(FSRef *from, Handle alias, u8 logon, FSRef *target, u8 *changed) { (void)logon; return FSResolveAlias(from, alias, target, changed); }
OSErr FSUpdateAlias(const FSRef *from, const FSRef *target, Handle alias, u8 *changed) {
    (void)from;
    char p[1024];
    if (!alias_ok(alias) || ref_path(target, p, sizeof p)) return paramErr;
    Handle n = alias_make(p);
    if (!n) return fnfErr;
    long sz = GetHandleSize(n);
    if (changed) *changed = sz != GetHandleSize(alias) || memcmp(*n, *alias, (size_t)sz);
    SetHandleSize(alias, sz);
    memcpy(*alias, *n, (size_t)sz);
    extern void DisposeHandle(Handle);
    DisposeHandle(n);
    return 0;
}
u16 GetAliasSizeFromPtr(const void *a) { u32 len; if (!a || memcmp(a, "mOSa", 4)) return 0; memcpy(&len, (const u8 *)a + 4, 4); return (u16)(8 + len > 0xffff ? 0xffff : 8 + len); }
/* FSAliasInfo e nomes: preenche o caminho (CFString) e o nome do alvo */
OSStatus FSCopyAliasInfo(Handle alias, HFSUniStr255 *name, HFSUniStr255 *vol, id *path, u32 *which, void *info) {
    if (!alias_ok(alias)) return paramErr;
    if (which) *which = 0;
    if (info) memset(info, 0, 64);
    u32 len;
    memcpy(&len, *alias + 4, 4);
    char p[1024];
    int stale;
    if (cx_book_resolve((const u8 *)*alias + 8, len, 0x100 /* sem montar */, p, sizeof p, &stale) < 0) return fnfErr;
    if (path) *path = objc_retain(cx_str(p));
    extern OSStatus FSGetHFSUniStrFromString(id s, HFSUniStr255 *o);
    const char *b = strrchr(p, '/');
    if (name) FSGetHFSUniStrFromString(cx_str(b && b[1] ? b + 1 : p), name);
    if (vol) FSGetHFSUniStrFromString(cx_str(!strncmp(p, "/Volumes/", 9) ? p + 9 : "mOS"), vol);
    return 0;
}
/* arquivo de alias do Finder ou link simbólico: aponta para outro item */
OSErr FSIsAliasFile(const FSRef *r, u8 *isAlias, u8 *isFolder) {
    char p[1024];
    st_buf s, l;
    if (ref_path(r, p, sizeof p)) return paramErr;
    if (lstat(p, &l)) return fnfErr;
    int link = (st_mode(&l) & 0170000) == 0120000, al = cx_alias_is(p);
    if (isAlias) *isAlias = (u8)(link || al);
    if (isFolder) *isFolder = !stat(p, &s) && (st_mode(&s) & 0170000) == 0040000 && !al;
    return 0;
}
OSErr FSResolveAliasFileWithMountFlags(FSRef *r, u8 chains, u8 *isFolder, u8 *wasAliased, unsigned long flags) {
    (void)flags;
    char p[1024], q[1024];
    if (ref_path(r, p, sizeof p)) return paramErr;
    if (wasAliased) *wasAliased = 0;
    for (int hop = 0; hop < (chains ? 10 : 1); hop++) {
        st_buf l;
        if (lstat(p, &l)) return fnfErr;
        if ((st_mode(&l) & 0170000) == 0120000) {
            if (!realpath(p, q)) return fnfErr;
        } else if (cx_alias_is(p)) {
            u8 *d = 0;
            long n = cx_alias_read(p, &d);
            int stale;
            if (n <= 0 || cx_book_resolve(d, n, 0, q, sizeof q, &stale) < 0) { free(d); return -35 /* nsvErr: alvo sumiu */; }
            free(d);
        } else break;
        memcpy(p, q, sizeof p);
        if (wasAliased) *wasAliased = 1;
    }
    st_buf s;
    if (stat(p, &s)) return fnfErr;
    if (isFolder) *isFolder = (st_mode(&s) & 0170000) == 0040000;
    return (OSErr)FSPathMakeRef((const u8 *)p, r, 0);
}
OSErr FSResolveAliasFile(FSRef *r, u8 chains, u8 *isFolder, u8 *wasAliased) { return FSResolveAliasFileWithMountFlags(r, chains, isFolder, wasAliased, 0); }

/* ================================================================ cópia, nomes, volumes */
#define COPYFILE_ALL 0xf
#define COPYFILE_RECURSIVE (1u << 15)
/* kFSFileOperationOverwrite = 1: sem ele, alvo que já existe = dupFNErr */
OSStatus FSCopyObjectSync(const FSRef *src, const FSRef *dstDir, id name, FSRef *target, u32 opts) {
    char s[1024], d[1024], t[1200];
    if (ref_path(src, s, sizeof s) || ref_path(dstDir, d, sizeof d)) return paramErr;
    const char *b = strrchr(s, '/');
    const char *nm = name && cx_is_str(name) ? cx_cstr(name) : b ? b + 1 : s;
    snprintf(t, sizeof t, "%s/%s", d, nm);
    st_buf st;
    if (!lstat(t, &st)) {
        if (!(opts & 1)) return dupFNErr;
        extern int cx_remove_tree(const char *p);
        cx_remove_tree(t);
    }
    if (copyfile(s, t, 0, COPYFILE_ALL | COPYFILE_RECURSIVE)) return -36;
    return target ? FSPathMakeRef((const u8 *)t, target, 0) : 0;
}
OSStatus FSGetHFSUniStrFromString(id s, HFSUniStr255 *o) {
    if (!s || !o) return paramErr;
    u64 n = MSG0(u64, s, "length");
    if (n > 255) return -37;                                                 /* bdNamErr */
    MSG(void, s, "getCharacters:range:", o->unicode, (u64)0, n);
    o->length = (u16)n;
    return 0;
}
/* FSRefParam (Files.h, x86_64): só os campos que PBMakeFSRefUnicodeSync usa */
typedef struct { void *qLink; short qType, ioTrap; void *ioCmdAddr, *ioCompletion; OSErr ioResult; const u8 *ioNamePtr; short ioVRefNum;
                 short reserved1; u8 reserved2, reserved3; const FSRef *ref; u32 whichInfo; void *catInfo; UniCharCount nameLength;
                 const u16 *name; long ioDirID; void *spec; FSRef *parentRef; FSRef *newRef; u32 textEncodingHint; void *outName; } fsrefparam_t;
OSStatus FSMakeFSRefUnicode(const FSRef *parent, UniCharCount n, const u16 *name, u32 hint, FSRef *out);
OSErr PBMakeFSRefUnicodeSync(fsrefparam_t *pb) {
    if (!pb) return paramErr;
    pb->ioResult = (OSErr)FSMakeFSRefUnicode(pb->ref, pb->nameLength, pb->name, pb->textEncodingHint, pb->newRef);
    return pb->ioResult;
}
/* volumes: FSVolumeRefNum -1 = o de inicialização; o índice começa em 1 e percorre os pontos de montagem */
typedef struct { u8 b[2168]; } statfs_t;                                    /* struct statfs do macOS (64 bits) */
static int vol_path(short vref, unsigned long idx, char *out, size_t cap) {
    if (idx <= 1 && (vref == -1 || vref == 0 || idx == 1)) { snprintf(out, cap, "/"); return 0; }
    /* outros volumes: /Volumes/<n> na ordem do diretório */
    extern void *opendir(const char *);
    extern void *readdir(void *);
    extern int closedir(void *);
    void *d = opendir("/Volumes");
    unsigned long k = 1;
    if (!d) return -1;
    void *e;
    while ((e = readdir(d))) {
        const char *n = (const char *)e + 21;                                /* d_name do struct dirent do macOS */
        if (n[0] == '.') continue;
        if (++k == idx) { snprintf(out, cap, "/Volumes/%s", n); closedir(d); return 0; }
    }
    closedir(d);
    return -1;
}
#pragma pack(push, 2)
typedef struct { u16 hi; u32 lo; u16 frac; } utcdt_t;
typedef struct { utcdt_t create, modify, backup, checked; u32 files, folders; u64 total, free_; u32 bsize, tblocks, fblocks, next,
                 rsrc, data, nextCat; u8 finder[32]; u16 flags, fsid, sig, drive; short driverRef; } volinfo_t;
#pragma pack(pop)
OSErr FSGetVolumeInfo(short vref, unsigned long idx, short *actual, u32 which, volinfo_t *info, HFSUniStr255 *name, FSRef *root) {
    (void)which;
    char p[512];
    if (vol_path(vref, idx, p, sizeof p)) return nsvErr;
    statfs_t s;
    if (statfs(p, &s)) return nsvErr;
    u32 bsize; u64 blocks, bfree;
    memcpy(&bsize, s.b, 4); memcpy(&blocks, s.b + 8, 8); memcpy(&bfree, s.b + 16, 8);
    if (actual) *actual = (short)(idx ? -(short)idx : vref);
    if (info) {
        memset(info, 0, sizeof *info);
        info->bsize = bsize;
        info->total = blocks * bsize; info->free_ = bfree * bsize;
        info->tblocks = (u32)(blocks > 0xffffffffull ? 0xffffffffu : blocks);
        info->fblocks = (u32)(bfree > 0xffffffffull ? 0xffffffffu : bfree);
        info->sig = 0x482b;                                                  /* 'H+' */
    }
    if (name) {
        const char *n = !strcmp(p, "/") ? "mOS" : p + 9;
        FSGetHFSUniStrFromString(cx_str(n), name);
    }
    if (root) FSPathMakeRef((const u8 *)p, root, 0);
    return 0;
}
/* GetVolParmsInfoBuffer: versão 4, atributos de um volume HFS+ local (bHasExtFSVol, bHasFileIDs, bHasCatSearch...) */
typedef struct { short version; long attrib; void *localHand; long server; long volumeGrade; short foreignPrivID; long extendedAttributes;
                 void *deviceID; u32 maxNameLen; } volparms_t;
OSStatus FSGetVolumeParms(short vref, volparms_t *b, unsigned long size) {
    char p[512];
    if (!b || size < 14) return paramErr;
    if (vol_path(vref, 0, p, sizeof p)) return nsvErr;
    memset(b, 0, size);
    b->version = 4;
    b->attrib = (1L << 16) | (1L << 17) | (1L << 15) | (1L << 25);
    if (size >= sizeof *b) b->maxNameLen = 255;
    return 0;
}
OSStatus FSEjectVolumeSync(short vref, u32 flags, int *dissenter) {
    (void)flags;
    char p[512];
    if (dissenter) *dissenter = 0;
    if (vref == -1 || vol_path(vref, (unsigned long)(-vref), p, sizeof p) || !strcmp(p, "/")) return -47;   /* fBsyErr */
    if (SYS1(SYS_IMGDETACH, p) < 0) return -47;                            /* o mesmo caminho do DADiskEject */
    return 0;
}

/* ================================================================ notificações, backup, espaço */
/* FNNotify: avisa quem observa a pasta (Finder, NSFilePresenter, FSEvents) tocando a data dela */
OSStatus FNNotifyByPath(const u8 *path, u32 msg, u32 flags) { (void)msg; (void)flags; return path && !utimes((const char *)path, 0) ? 0 : fnfErr; }
OSStatus FNNotify(const FSRef *r, u32 msg, u32 flags) { char p[1024]; return ref_path(r, p, sizeof p) ? paramErr : FNNotifyByPath((const u8 *)p, msg, flags); }
OSStatus FNNotifyAll(u32 msg, u32 flags) { return FNNotifyByPath((const u8 *)"/", msg, flags); }
/* exclusão do Time Machine: o mesmo xattr que o tmutil usa */
#define BK_XATTR "com.apple.metadata:com_apple_backup_excludeItem"
OSStatus CSBackupSetItemExcluded(id url, u8 exclude, u8 byPath) {
    (void)byPath;
    char p[1024];
    if (!url_path(url, p, sizeof p)) return paramErr;
    if (!exclude) { removexattr(p, BK_XATTR, 0); return 0; }                 /* já não estar marcado não é erro */
    static const u8 v[] = "\x62\x70\x6c\x69\x73\x74\x30\x30\x5f\x10\x11\x63\x6f\x6d\x2e\x61\x70\x70\x6c\x65\x2e\x62\x61\x63\x6b\x75\x70\x64\x08\x00\x00\x00\x00\x00\x00\x01\x01\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x1c";   /* bplist da string "com.apple.backupd", como o tmutil grava */
    return setxattr(p, BK_XATTR, v, sizeof v - 1, 0, 0) ? -36 : 0;
}
u8 CSBackupIsItemExcluded(id url, u8 *byPath) {
    char p[1024];
    if (byPath) *byPath = 0;
    if (!url_path(url, p, sizeof p)) return 0;
    return getxattr(p, BK_XATTR, 0, 0, 0, 0) > 0;
}
/* espaço purgável: o mOS não guarda cache purgável do sistema, então a estimativa é 0 e a recuperação termina já */
OSStatus CSDiskSpaceGetRecoveryEstimate(id volume, u64 *bytes) { (void)volume; if (bytes) *bytes = 0; return volume ? 0 : paramErr; }
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, u8, u64, id); } ds_blk_t;
OSStatus CSDiskSpaceStartRecovery(id volume, u64 want, u32 opts, void *op, void *queue, void *block) {
    (void)want; (void)opts; (void)queue;
    if (!volume || !block) return paramErr;
    if (op) memset(op, 0, 16);
    ((ds_blk_t *)block)->invoke(block, 0, 0, 0);                            /* nada a liberar: termina com 0 bytes */
    return 0;
}
void CSDiskSpaceCancelRecovery(void *op) { (void)op; }

/* ================================================================ lugar e datas */
/* MachineLocation {Fixed latitude, longitude; union {u8 dlsDelta; long gmtDelta}} — o fuso vem do sistema */
typedef struct { int lat, lon; int gmtDelta; } machloc_t;
void ReadLocation(machloc_t *loc) {
    if (!loc) return;
    loc->lat = loc->lon = 0;
    id tz = MSG0(id, (id)cx_class_named("NSTimeZone"), "localTimeZone");
    long secs = tz ? (long)MSG0(long, tz, "secondsFromGMT") : 0;
    int dst = tz && MSG0(BOOL, tz, "isDaylightSavingTime");
    loc->gmtDelta = (int)(secs & 0x00ffffff) | (dst ? (int)0x80000000 : 0);
}
/* segundos desde 1/1/1904 (Mac) <-> CFAbsoluteTime (desde 1/1/2001): diferença de 3061152000 s */
#define MAC_EPOCH_DELTA 3061152000.0
OSStatus UCConvertCFAbsoluteTimeToSeconds(double t, u32 *secs) {
    if (!secs) return paramErr;
    double s = t + MAC_EPOCH_DELTA;
    if (s < 0 || s > 4294967295.0) return -8740;                             /* kUTCOverflowErr */
    *secs = (u32)s;
    return 0;
}
OSStatus UCConvertSecondsToCFAbsoluteTime(u32 secs, double *t) { if (!t) return paramErr; *t = (double)secs - MAC_EPOCH_DELTA; return 0; }
OSStatus UCConvertCFAbsoluteTimeToLongDateTime(double t, long long *ldt) { if (!ldt) return paramErr; *ldt = (long long)(t + MAC_EPOCH_DELTA); return 0; }
/* UTCDateTime {u16 highSeconds; u32 lowSeconds; u16 fraction} (fração em 1/65536 s) */
OSStatus UCConvertCFAbsoluteTimeToUTCDateTime(double t, utcdt_t *u) {
    if (!u) return paramErr;
    double s = t + MAC_EPOCH_DELTA;
    if (s < 0) return -8740;
    unsigned long long whole = (unsigned long long)s;
    u->hi = (u16)(whole >> 32); u->lo = (u32)whole;
    u->frac = (u16)((s - (double)whole) * 65536.0);
    return 0;
}
