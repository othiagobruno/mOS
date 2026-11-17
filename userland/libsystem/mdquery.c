/* mOS libSystem — Spotlight (CoreServices/Metadata): MDQuery e MDItem.
 *
 * Não há índice: MDQueryExecute varre o escopo no VFS (sem entrar em pacotes .app, até 12 níveis e 50 000
 * entradas) e avalia o predicado de mdq_core.c em cada item. Atributos de um item: kMDItemPath, kMDItemFSName,
 * kMDItemDisplayName (o nome do app sem ".app", ou CFBundleDisplayName/CFBundleName), kMDItemCFBundleIdentifier
 * (Info.plist), kMDItemContentType e kMDItemContentTypeTree (Launch Services: o tipo e os pais), kMDItemKind,
 * kMDItemFSSize. Escopos: caminhos (CFString ou NSURL), kMDQueryScopeHome, kMDQueryScopeComputer (as pastas de
 * apps e a pasta do usuário); sem escopo vale o computador.
 * kMDQuerySynchronous (1) junta tudo antes de voltar e não manda notificação, como o macOS. Sem ele, a coleta roda
 * na fila de MDQuerySetDispatchQueue (ou na principal) e termina com kMDQueryDidFinishNotification no centro local.
 * Atualizações ao vivo (kMDQueryWantsUpdates): depois da coleta um FSEventStream (fsevents.c) segue o escopo; cada
 * leva de eventos refaz a coleta e posta kMDQueryDidUpdateNotification com kMDQueryUpdateAdded/Changed/RemovedItems.
 * Atributos novos: datas (kMDItemFSCreationDate, kMDItemFSContentChangeDate, kMDItemContentCreationDate,
 * kMDItemContentModificationDate, kMDItemLastUsedDate, kMDItemDateAdded; nas consultas são números CFAbsoluteTime e
 * aceitam $time.*), kMDItemLogicalSize, kMDItemPhysicalSize, kMDItemFSOwnerUserID/GroupID, kMDItemFSInvisible,
 * kMDItemFSNodeCount, kMDItemVersion (CFBundleShortVersionString), kMDItemWhereFroms (o xattr
 * com.apple.metadata:kMDItemWhereFroms que os navegadores gravam) e kMDItemTextContent (o texto de um arquivo
 * public.text de até 256 KB, lido só quando a consulta pede).
 * Um volume com .metadata_never_index na raiz (mdutil -i off) fica fora da busca, como no macOS. */
#include "foundation.h"
#include "mdq_core.h"
#include "sys.h"

int cx_ls_type_of_file(const char *path, char *out, size_t cap);
int cx_ls_type_info(const char *uti, char *desc, size_t dcap, char *parents, size_t pcap);
int cx_snprintf(char *buf, unsigned long n, const char *fmt, ...);
void cx_object_free(id o);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, unsigned long n);
size_t strlen(const char *s);
char *strrchr(const char *s, int c);
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, unsigned long n);
void *calloc(size_t n, size_t s);
void *malloc(size_t n);
void free(void *p);
char *getenv(const char *k);
typedef void (*dispatch_function_t)(void *);
void dispatch_async_f(void *q, void *ctx, dispatch_function_t fn);
void *dispatch_get_main_queue(void);
void *dispatch_get_global_queue(long id, unsigned long flags);
void *dispatch_retain(void *q);
void dispatch_release(void *q);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
id CFDateCreate(void *alloc, double at);
double CFAbsoluteTimeGetCurrent(void);
/* tamanho do arquivo (SYS_STAT); -1 se não existe */
static long long cx_file_size(const char *path) {
    mos_stat_t st;
    if (SYS2(SYS_STAT, path, &st) < 0) return -1;
    return (long long)st.size;
}

/* constantes (os valores são os nomes, como no macOS) */
#define MDK(sym) static nsstr_t sym##_o = { &NSConstantString_c, 0, (char *)#sym, sizeof(#sym) - 1 }; id sym = (id)&sym##_o;
extern cx_class_t NSConstantString_c __asm__("__CFConstantStringClassReference");
MDK(kMDItemCFBundleIdentifier) MDK(kMDItemDisplayName) MDK(kMDItemPath) MDK(kMDItemFSName) MDK(kMDItemContentType)
MDK(kMDItemContentTypeTree) MDK(kMDItemKind) MDK(kMDItemFSSize)
MDK(kMDItemFSCreationDate) MDK(kMDItemFSContentChangeDate) MDK(kMDItemContentCreationDate) MDK(kMDItemContentModificationDate)
MDK(kMDItemLastUsedDate) MDK(kMDItemDateAdded) MDK(kMDItemLogicalSize) MDK(kMDItemPhysicalSize) MDK(kMDItemFSOwnerUserID)
MDK(kMDItemFSOwnerGroupID) MDK(kMDItemFSInvisible) MDK(kMDItemFSNodeCount) MDK(kMDItemVersion) MDK(kMDItemTextContent)
MDK(kMDQueryDidFinishNotification) MDK(kMDQueryDidUpdateNotification) MDK(kMDQueryProgressNotification)
MDK(kMDQueryScopeComputer) MDK(kMDQueryScopeHome) MDK(kMDQueryScopeNetwork) MDK(kMDQueryScopeAllIndexed)
MDK(kMDQueryScopeComputerIndexed) MDK(kMDQueryScopeNetworkIndexed)
MDK(kMDQueryUpdateAddedItems) MDK(kMDQueryUpdateChangedItems) MDK(kMDQueryUpdateRemovedItems)
/* atributos de metadados de conteúdo (MDItem.h): o mOS não tem importadores, então item_get devolve NULL para eles,
 * como o macOS faz para um arquivo sem esses metadados; as chaves existem para predicados e dicionários */
MDK(kMDItemAlbum) MDK(kMDItemAudioBitRate) MDK(kMDItemAudioChannelCount) MDK(kMDItemAudioSampleRate)
MDK(kMDItemAudioTrackNumber) MDK(kMDItemAuthors) MDK(kMDItemBitsPerSample) MDK(kMDItemCity) MDK(kMDItemCodecs)
MDK(kMDItemColorSpace) MDK(kMDItemComment) MDK(kMDItemComposer) MDK(kMDItemContactKeywords)
MDK(kMDItemContributors) MDK(kMDItemCopyright) MDK(kMDItemCreator) MDK(kMDItemDescription)
MDK(kMDItemDurationSeconds) MDK(kMDItemEmailAddresses) MDK(kMDItemEncodingApplications) MDK(kMDItemFinderComment)
MDK(kMDItemFSHasCustomIcon) MDK(kMDItemFSIsExtensionHidden) MDK(kMDItemFSIsStationery) MDK(kMDItemFSLabel)
MDK(kMDItemGenre) MDK(kMDItemHasAlphaChannel) MDK(kMDItemHeadline) MDK(kMDItemHTMLContent) MDK(kMDItemKeywords)
MDK(kMDItemLanguages) MDK(kMDItemLayerNames) MDK(kMDItemLyricist) MDK(kMDItemMediaTypes) MDK(kMDItemMusicalGenre)
MDK(kMDItemNumberOfPages) MDK(kMDItemOrganizations) MDK(kMDItemPageHeight) MDK(kMDItemPageWidth)
MDK(kMDItemParticipants) MDK(kMDItemPerformers) MDK(kMDItemPixelCount) MDK(kMDItemPixelHeight)
MDK(kMDItemPixelWidth) MDK(kMDItemProfileName) MDK(kMDItemProjects) MDK(kMDItemPublishers) MDK(kMDItemRecipients)
MDK(kMDItemRecordingDate) MDK(kMDItemRecordingYear) MDK(kMDItemResolutionHeightDPI) MDK(kMDItemResolutionWidthDPI)
MDK(kMDItemSecurityMethod) MDK(kMDItemTitle) MDK(kMDItemTotalBitRate) MDK(kMDItemVideoBitRate)

enum { MD_SYNC = 1, MD_UPDATES = 4, MD_MAX_DEPTH = 12, MD_MAX_VISIT = 50000 };

/* ---------------------------------------------------------------- MDItem */
typedef struct {
    char *path;
    int have_info, have_type, have_stat, have_from, have_text;
    char name[256], display[256], bundle[256], version[64], type[128], kind[128];
    char tree[8][128];
    int ntree;
    /* stat: datas em CFAbsoluteTime */
    double birth, mtime, atime;
    long long size, blocks;
    unsigned uid, gid, flags, is_dir;
    long nodes;
    char from[4][512];
    int nfrom;
    char *text;
    char num[12][40];        /* textos dos valores numéricos que item_get devolve */
} mditem_t;

static cx_class_t MDItem_c, MDItem_m, MDQuery_c, MDQuery_m;
static int md_ready;
static mditem_t *item_of(id o) { return o && object_getClass(o) == &MDItem_c ? *(mditem_t **)((u8 *)o + 8) : 0; }

static void item_dealloc(id self, SEL c) {
    mditem_t *m = item_of(self);
    (void)c;
    if (m) { free(m->path); free(m->text); free(m); }
    *(void **)((u8 *)self + 8) = 0;
    cx_object_free(self);
}

typedef struct mdquery_s mdquery_t;
static mdquery_t *query_of(id o);
static void query_dealloc(id self, SEL c);
static void md_boot(void) {
    cx_class_t *ns;
    if (md_ready) return;
    ns = cx_class_named("NSObject");
    if (!ns) return;
    cx_define_class(&MDItem_c, &MDItem_m, "__MDItem", ns, 8);
    cx_add_method(&MDItem_c, "dealloc", (void *)item_dealloc);
    cx_define_class(&MDQuery_c, &MDQuery_m, "__MDQuery", ns, 8);
    cx_add_method(&MDQuery_c, "dealloc", (void *)query_dealloc);
    md_ready = 1;
}

static void copy_s(char *d, size_t cap, const char *s) {
    size_t i = 0;
    for (; s && s[i] && i + 1 < cap; i++) d[i] = s[i];
    d[i] = 0;
}
static int ends(const char *s, const char *suf) { size_t a = strlen(s), b = strlen(suf); return a >= b && !strcmp(s + a - b, suf); }

static void item_info(mditem_t *m) {
    if (m->have_info) return;
    m->have_info = 1;
    const char *sl = strrchr(m->path, '/');
    copy_s(m->name, sizeof m->name, sl ? sl + 1 : m->path);
    copy_s(m->display, sizeof m->display, m->name);
    if (ends(m->name, ".app")) {
        m->display[strlen(m->display) - 4] = 0;
        char plist[1100];
        cx_snprintf(plist, sizeof plist, "%s/Contents/Info.plist", m->path);
        id d = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithContentsOfFile:", cx_str(plist));
        if (d) {
            id b = MSG(id, d, "objectForKey:", cx_str("CFBundleIdentifier"));
            id dn = MSG(id, d, "objectForKey:", cx_str("CFBundleDisplayName"));
            if (!dn || !cx_is_str(dn)) dn = MSG(id, d, "objectForKey:", cx_str("CFBundleName"));
            if (b && cx_is_str(b)) copy_s(m->bundle, sizeof m->bundle, cx_cstr(b));
            id ver = MSG(id, d, "objectForKey:", cx_str("CFBundleShortVersionString"));
            if (ver && cx_is_str(ver)) copy_s(m->version, sizeof m->version, cx_cstr(ver));
            if (dn && cx_is_str(dn) && cx_cstr(dn)[0]) copy_s(m->display, sizeof m->display, cx_cstr(dn));
        }
    }
}
static void item_type(mditem_t *m) {
    if (m->have_type) return;
    m->have_type = 1;
    if (!cx_ls_type_of_file(m->path, m->type, sizeof m->type) || !m->type[0]) {
        void *d = opendir(m->path);
        if (d) { closedir(d); copy_s(m->type, sizeof m->type, ends(m->path, ".app") ? "com.apple.application-bundle" : "public.folder"); }
        else copy_s(m->type, sizeof m->type, "public.data");
    }
    /* a árvore: o tipo e os pais, subindo (até 8) */
    copy_s(m->tree[0], sizeof m->tree[0], m->type);
    m->ntree = 1;
    for (int i = 0; i < m->ntree && m->ntree < 8; i++) {
        char desc[128], parents[512];
        if (!cx_ls_type_info(m->tree[i], desc, sizeof desc, parents, sizeof parents)) continue;
        if (i == 0) copy_s(m->kind, sizeof m->kind, desc);
        char *p = parents;
        while (*p && m->ntree < 8) {
            char *e = p;
            while (*e && *e != ' ') e++;
            char save = *e;
            *e = 0;
            int dup = 0;
            for (int k = 0; k < m->ntree; k++) if (!strcmp(m->tree[k], p)) dup = 1;
            if (*p && !dup) copy_s(m->tree[m->ntree++], sizeof m->tree[0], p);
            *e = save;
            p = *e ? e + 1 : e;
        }
    }
}


/* stat do macOS ($INODE64, o mac_stat_t de libc.c) */
typedef struct {
    u32 dev; u16 mode, nlink; u64 ino; u32 uid, gid, rdev;
    i64 atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns, btime, btime_ns;
    i64 size, blocks; i32 blksize; u32 flags, gen; i32 lspare; i64 qspare[2];
} md_stat_t;
int stat(const char *p, void *st);
long getxattr(const char *p, const char *n, void *v, size_t s, u32 pos, int o);
int open(const char *p, int flags, ...);
long read(int fd, void *b, size_t n);
int close(int fd);
#define ABS(s, ns) ((double)(s) + (double)(ns) / 1e9 - 978307200.0)
static void item_stat(mditem_t *m) {
    if (m->have_stat) return;
    m->have_stat = 1;
    md_stat_t st;
    if (stat(m->path, &st) != 0) return;
    m->birth = ABS(st.btime ? st.btime : st.mtime, st.btime ? st.btime_ns : st.mtime_ns);
    m->mtime = ABS(st.mtime, st.mtime_ns);
    m->atime = ABS(st.atime, st.atime_ns);
    m->size = st.size;
    m->blocks = st.blocks;
    m->uid = st.uid;
    m->gid = st.gid;
    m->flags = st.flags;
    m->is_dir = (st.mode & 0170000) == 0040000;
    m->nodes = -1;
    if (m->is_dir) {
        void *d = opendir(m->path);
        if (d) {
            long n = 0;
            u8 *e;
            while ((e = readdir(d))) { const char *nm = (const char *)e + 21; if (strcmp(nm, ".") && strcmp(nm, "..")) n++; }
            closedir(d);
            m->nodes = n;
        }
    }
}
/* kMDItemWhereFroms: plist (binário ou XML) com a lista de URLs, gravada pelo navegador no download */
static void item_from(mditem_t *m) {
    if (m->have_from) return;
    m->have_from = 1;
    u8 buf[4096];
    long n = getxattr(m->path, "com.apple.metadata:kMDItemWhereFroms", buf, sizeof buf, 0, 0);
    if (n <= 0) return;
    id data = MSG(id, (id)cx_class_named("NSData"), "dataWithBytes:length:", (const void *)buf, (u64)n);
    id list = data ? MSG(id, (id)cx_class_named("NSPropertyListSerialization"), "propertyListWithData:options:format:error:",
                         data, (u64)0, (void *)0, (void *)0) : 0;
    if (!list || !MSG(BOOL, list, "isKindOfClass:", (id)cx_class_named("NSArray"))) return;
    u64 c = MSG0(u64, list, "count");
    for (u64 i = 0; i < c && m->nfrom < 4; i++) {
        id v = MSG(id, list, "objectAtIndex:", i);
        if (v && cx_is_str(v)) copy_s(m->from[m->nfrom++], sizeof m->from[0], cx_cstr(v));
    }
}
static int conforms(mditem_t *m, const char *uti);
/* kMDItemTextContent: o conteúdo de arquivos de texto (até 256 KB) */
static void item_text(mditem_t *m) {
    if (m->have_text) return;
    m->have_text = 1;
    if (!conforms(m, "public.text")) return;
    int fd = open(m->path, 0);
    if (fd < 0) return;
    enum { MAXT = 256 * 1024 };
    char *b = malloc(MAXT + 1);
    long n = b ? read(fd, b, MAXT) : -1;
    close(fd);
    if (n < 0) { free(b); return; }
    for (long i = 0; i < n; i++) if (!b[i]) b[i] = ' ';
    b[n] = 0;
    m->text = b;
}
static const char *fmt_num(mditem_t *m, int slot, double v, int integer) {
    if (integer) cx_snprintf(m->num[slot], sizeof m->num[0], "%lld", (long long)v);
    else cx_snprintf(m->num[slot], sizeof m->num[0], "%.3f", v);
    return m->num[slot];
}
static const char *item_get(void *ctx, const char *attr, int i) {
    mditem_t *m = ctx;
    if (!strcmp(attr, "kMDItemContentTypeTree")) { item_type(m); return i < m->ntree ? m->tree[i] : 0; }
    if (!strcmp(attr, "kMDItemWhereFroms")) { item_from(m); return i < m->nfrom ? m->from[i] : 0; }
    if (i) return 0;
    if (!strcmp(attr, "kMDItemPath")) return m->path;
    item_info(m);
    if (!strcmp(attr, "kMDItemFSName")) return m->name;
    if (!strcmp(attr, "kMDItemDisplayName")) return m->display;
    if (!strcmp(attr, "kMDItemCFBundleIdentifier")) return m->bundle[0] ? m->bundle : 0;
    if (!strcmp(attr, "kMDItemContentType")) { item_type(m); return m->type; }
    if (!strcmp(attr, "kMDItemKind")) { item_type(m); return m->kind[0] ? m->kind : 0; }
    if (!strcmp(attr, "kMDItemVersion")) return m->version[0] ? m->version : 0;
    if (!strcmp(attr, "kMDItemWhereFroms")) { item_from(m); return 0; }
    if (!strcmp(attr, "kMDItemTextContent")) { item_text(m); return m->text; }
    item_stat(m);
    if (!strcmp(attr, "kMDItemFSSize") || !strcmp(attr, "kMDItemLogicalSize")) return fmt_num(m, 0, (double)m->size, 1);
    if (!strcmp(attr, "kMDItemPhysicalSize")) return fmt_num(m, 1, (double)m->blocks * 512, 1);
    if (!strcmp(attr, "kMDItemFSCreationDate") || !strcmp(attr, "kMDItemContentCreationDate") || !strcmp(attr, "kMDItemDateAdded"))
        return fmt_num(m, 2, m->birth, 0);
    if (!strcmp(attr, "kMDItemFSContentChangeDate") || !strcmp(attr, "kMDItemContentModificationDate")) return fmt_num(m, 3, m->mtime, 0);
    if (!strcmp(attr, "kMDItemLastUsedDate")) return fmt_num(m, 4, m->atime, 0);
    if (!strcmp(attr, "kMDItemFSOwnerUserID")) return fmt_num(m, 5, m->uid, 1);
    if (!strcmp(attr, "kMDItemFSOwnerGroupID")) return fmt_num(m, 6, m->gid, 1);
    if (!strcmp(attr, "kMDItemFSInvisible")) return fmt_num(m, 7, m->name[0] == '.' || (m->flags & 0x8000), 1);   /* UF_HIDDEN */
    if (!strcmp(attr, "kMDItemFSNodeCount")) return m->nodes >= 0 ? fmt_num(m, 8, (double)m->nodes, 1) : 0;
    return 0;
}
/* conformidade pela árvore de tipos */
static int conforms(mditem_t *m, const char *uti) {
    item_type(m);
    for (int i = 0; i < m->ntree; i++) if (!strcmp(m->tree[i], uti)) return 1;
    return 0;
}

static id item_new(const char *path) {
    md_boot();
    mditem_t *m = calloc(1, sizeof *m);
    if (!m) return 0;
    size_t n = strlen(path);
    m->path = malloc(n + 1);
    if (!m->path) { free(m); return 0; }
    memcpy(m->path, path, n + 1);
    id o = cx_alloc_instance(&MDItem_c);
    if (!o) { free(m->path); free(m); return 0; }
    *(mditem_t **)((u8 *)o + 8) = m;
    return o;
}

id MDItemCreate(void *alloc, id path) {
    (void)alloc;
    const char *p = path && cx_is_str(path) ? cx_cstr(path) : 0;
    if (!p || p[0] != '/') return 0;
    void *d = opendir(p);
    if (d) closedir(d);
    else if (cx_file_size(p) < 0) return 0;          /* o macOS devolve NULL para caminho inexistente */
    return item_new(p);
}
id MDItemCreateWithURL(void *alloc, id url) { return url ? MDItemCreate(alloc, MSG0(id, url, "path")) : 0; }

id MDItemCopyAttribute(id item, id name) {
    mditem_t *m = item_of(item);
    const char *a = name && cx_is_str(name) ? cx_cstr(name) : 0;
    if (!m || !a) return 0;
    if (!strcmp(a, "kMDItemContentTypeTree")) {
        item_type(m);
        id arr = cx_arr_new(0);
        for (int i = 0; i < m->ntree; i++) cx_arr_add(arr, cx_str(m->tree[i]));
        return objc_retain(arr);
    }
    if (!strcmp(a, "kMDItemWhereFroms")) {
        item_from(m);
        if (!m->nfrom) return 0;
        id arr = cx_arr_new(0);
        for (int i = 0; i < m->nfrom; i++) cx_arr_add(arr, cx_str(m->from[i]));
        return objc_retain(arr);
    }
    static const char *dates[] = {"kMDItemFSCreationDate", "kMDItemContentCreationDate", "kMDItemDateAdded",
                                  "kMDItemFSContentChangeDate", "kMDItemContentModificationDate", "kMDItemLastUsedDate"};
    static const char *nums[] = {"kMDItemFSSize", "kMDItemLogicalSize", "kMDItemPhysicalSize", "kMDItemFSOwnerUserID",
                                 "kMDItemFSOwnerGroupID", "kMDItemFSInvisible", "kMDItemFSNodeCount"};
    for (unsigned k = 0; k < sizeof dates / sizeof dates[0]; k++)
        if (!strcmp(a, dates[k])) {
            item_stat(m);
            if (!m->have_stat || (!m->mtime && !m->birth)) return 0;
            double t = k < 3 ? m->birth : k < 5 ? m->mtime : m->atime;
            return CFDateCreate(0, t);
        }
    for (unsigned k = 0; k < sizeof nums / sizeof nums[0]; k++)
        if (!strcmp(a, nums[k])) {
            const char *v = item_get(m, a, 0);
            if (!v) return 0;
            if (k == 5) return objc_retain(MSG(id, (id)cx_class_named("NSNumber"), "numberWithBool:", (BOOL)(v[0] == '1')));
            long long x = 0;
            for (const char *p = v; *p >= '0' && *p <= '9'; p++) x = x * 10 + (*p - '0');
            return cx_num_i(x);                         /* +1 */
        }
    const char *v = item_get(m, a, 0);
    return v ? objc_retain(cx_str(v)) : 0;
}
id MDItemCopyAttributes(id item, id names) {
    if (!item_of(item) || !names) return 0;
    id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    u64 n = MSG0(u64, names, "count");
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, names, "objectAtIndex:", i);
        id v = MDItemCopyAttribute(item, k);
        if (v) { MSG(void, d, "setObject:forKey:", v, k); objc_release(v); }
    }
    return objc_retain(d);
}
id MDItemCopyAttributeNames(id item) {
    static const char *all[] = {"kMDItemPath", "kMDItemFSName", "kMDItemDisplayName", "kMDItemContentType",
                                "kMDItemContentTypeTree", "kMDItemKind", "kMDItemFSSize", "kMDItemLogicalSize",
                                "kMDItemPhysicalSize", "kMDItemFSCreationDate", "kMDItemContentCreationDate",
                                "kMDItemDateAdded", "kMDItemFSContentChangeDate", "kMDItemContentModificationDate",
                                "kMDItemLastUsedDate", "kMDItemFSOwnerUserID", "kMDItemFSOwnerGroupID", "kMDItemFSInvisible",
                                "kMDItemFSNodeCount", "kMDItemCFBundleIdentifier", "kMDItemVersion", "kMDItemWhereFroms"};
    mditem_t *m = item_of(item);
    if (!m) return 0;
    item_info(m);
    item_stat(m);
    item_from(m);
    id arr = cx_arr_new(0);
    for (unsigned i = 0; i < sizeof all / sizeof all[0]; i++) {
        const char *a = all[i];
        if (!strcmp(a, "kMDItemCFBundleIdentifier") && !m->bundle[0]) continue;
        if (!strcmp(a, "kMDItemVersion") && !m->version[0]) continue;
        if (!strcmp(a, "kMDItemWhereFroms") && !m->nfrom) continue;
        if (!strcmp(a, "kMDItemFSNodeCount") && m->nodes < 0) continue;
        cx_arr_add(arr, cx_str(a));
    }
    return objc_retain(arr);
}

/* ---------------------------------------------------------------- MDQuery */
struct mdquery_s {
    mdq_node *pred;
    id query_text;
    id results;            /* NSMutableArray de MDItem */
    char **scopes;
    int nscopes;
    void *queue;
    int running, gathering, stopped, updates_off, visited, wants_updates, pending_update;
    void *stream;            /* FSEventStreamRef das atualizações ao vivo */
    int (*filter)(void *ctx, id item);   /* NSMetadataQuery: o NSPredicate decide (ctx = o estado dele) */
    void (*on_update)(void *ctx, id added, id changed, id removed);
    void *hook_ctx;
    id self_obj;             /* fraco: o __MDQuery dono (para o objeto da notificação) */
    long max_count;          /* MDQuerySetMaxCount: 0 = sem limite */
    id sort_attrs;           /* MDQueryCreate(sortingAttrs) */
    int (*sort_cmp)(const void **, const void **, void *);   /* MDQuerySetSortComparator */
    void *sort_ctx;
    u32 sort_flags[8];       /* MDQuerySetSortOptionFlagsForAttribute: 1 = decrescente */
    id sort_flag_attr[8];
    u64 batch[6];            /* MDQueryBatchingParams: guardados (a coleta do mOS entrega tudo de uma vez) */
};
static mdquery_t *query_of(id o) { return o && object_getClass(o) == &MDQuery_c ? *(mdquery_t **)((u8 *)o + 8) : 0; }
static void free_scopes(mdquery_t *q) {
    for (int i = 0; i < q->nscopes; i++) free(q->scopes[i]);
    free(q->scopes);
    q->scopes = 0;
    q->nscopes = 0;
}
void FSEventStreamStop(void *s);
void FSEventStreamInvalidate(void *s);
void FSEventStreamRelease(void *s);
static void query_dealloc(id self, SEL c) {
    mdquery_t *q = query_of(self);
    (void)c;
    if (q) {
        mdq_free(q->pred);
        if (q->results) objc_release(q->results);
        if (q->sort_attrs) objc_release(q->sort_attrs);
        for (int i = 0; i < 8; i++) if (q->sort_flag_attr[i]) objc_release(q->sort_flag_attr[i]);
        if (q->query_text) objc_release(q->query_text);
        if (q->stream) { FSEventStreamStop(q->stream); FSEventStreamInvalidate(q->stream); FSEventStreamRelease(q->stream); }
        if (q->queue) dispatch_release(q->queue);
        free_scopes(q);
        free(q);
    }
    *(void **)((u8 *)self + 8) = 0;
    cx_object_free(self);
}

id MDQueryCreate(void *alloc, id query, id valueListAttrs, id sortingAttrs) {
    (void)alloc; (void)valueListAttrs;
    char err[96];
    const char *s = query && cx_is_str(query) ? cx_cstr(query) : 0;
    if (!s) return 0;
    /* $time.* conta a partir de agora, no fuso do sistema */
    id tz = MSG0(id, (id)cx_class_named("NSTimeZone"), "localTimeZone");
    mdq_set_clock(CFAbsoluteTimeGetCurrent(), tz ? (long)MSG0(i64, tz, "secondsFromGMT") : 0);
    mdq_node *p = mdq_parse(s, err, sizeof err);
    if (!p) return 0;                                  /* consulta inválida: NULL, como no macOS */
    md_boot();
    mdquery_t *q = calloc(1, sizeof *q);
    if (!q) { mdq_free(p); return 0; }
    q->pred = p;
    q->query_text = objc_retain(query);
    q->results = objc_retain(cx_arr_new(0));
    q->sort_attrs = sortingAttrs ? objc_retain(sortingAttrs) : 0;
    id o = cx_alloc_instance(&MDQuery_c);
    if (!o) { mdq_free(p); objc_release(q->results); objc_release(q->query_text); free(q); return 0; }
    *(mdquery_t **)((u8 *)o + 8) = q;
    return o;
}
id MDQueryCreateSubset(void *alloc, id query, id vl, id sort, id items) { (void)items; return MDQueryCreate(alloc, query, vl, sort); }
id MDQueryCopyQueryString(id query) { mdquery_t *q = query_of(query); return q ? objc_retain(q->query_text) : 0; }

static void add_scope(mdquery_t *q, const char *p) {
    char **n = (char **)malloc(sizeof(char *) * (size_t)(q->nscopes + 1));
    if (!n) return;
    for (int i = 0; i < q->nscopes; i++) n[i] = q->scopes[i];
    size_t l = strlen(p);
    n[q->nscopes] = malloc(l + 1);
    if (!n[q->nscopes]) { free(n); return; }
    memcpy(n[q->nscopes], p, l + 1);
    free(q->scopes);
    q->scopes = n;
    q->nscopes++;
}
static void add_home(mdquery_t *q) { const char *h = getenv("HOME"); add_scope(q, h && h[0] ? h : "/Users"); }
static void add_computer(mdquery_t *q) {
    static const char *apps[] = {"/Aplicativos", "/Applications", "/Sistema/Aplicativos", "/System/Applications"};
    for (unsigned i = 0; i < 4; i++) add_scope(q, apps[i]);
    add_home(q);
}
void MDQuerySetSearchScope(id query, id scopes, u32 options) {
    mdquery_t *q = query_of(query);
    (void)options;
    if (!q) return;
    free_scopes(q);
    u64 n = scopes ? MSG0(u64, scopes, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id s = MSG(id, scopes, "objectAtIndex:", i);
        const char *p = 0;
        if (s && cx_is_str(s)) p = cx_cstr(s);
        else if (s && MSG(BOOL, s, "respondsToSelector:", sel_registerName("path"))) { id ps = MSG0(id, s, "path"); p = ps ? cx_cstr(ps) : 0; }
        if (!p) continue;
        if (!strcmp(p, "kMDQueryScopeComputer") || !strcmp(p, "kMDQueryScopeComputerIndexed") || !strcmp(p, "kMDQueryScopeAllIndexed")) add_computer(q);
        else if (!strcmp(p, "kMDQueryScopeHome")) add_home(q);
        else if (!strcmp(p, "kMDQueryScopeNetwork") || !strcmp(p, "kMDQueryScopeNetworkIndexed")) continue;   /* sem volumes de rede indexados */
        else if (p[0] == '/') add_scope(q, p);
    }
}
void MDQuerySetDispatchQueue(id query, void *queue) {
    mdquery_t *q = query_of(query);
    if (!q) return;
    if (queue) dispatch_retain(queue);
    if (q->queue) dispatch_release(q->queue);
    q->queue = queue;
}

static int already(mdquery_t *q, const char *path) {
    u64 n = MSG0(u64, q->results, "count");
    for (u64 i = 0; i < n; i++) {
        mditem_t *m = item_of(MSG(id, q->results, "objectAtIndex:", i));
        if (m && !strcmp(m->path, path)) return 1;
    }
    return 0;
}
static void test_path(mdquery_t *q, const char *path) {
    mditem_t probe;
    memset(&probe, 0, sizeof probe);
    probe.path = (char *)path;
    if (!mdq_eval(q->pred, item_get, &probe) || already(q, path)) return;
    if (q->max_count > 0 && (long)MSG0(u64, q->results, "count") >= q->max_count) return;   /* MDQuerySetMaxCount */
    id it = item_new(path);
    if (!it) return;
    if (!q->filter || q->filter(q->hook_ctx, it)) cx_arr_add(q->results, it);
    objc_release(it);
}
static void walk(mdquery_t *q, const char *dir, int depth) {
    if (q->stopped || depth > MD_MAX_DEPTH || q->visited > MD_MAX_VISIT) return;
    void *d = opendir(dir);
    if (!d) return;
    u8 *e;
    while ((e = readdir(d)) && !q->stopped && q->visited <= MD_MAX_VISIT) {
        const char *nm = (const char *)e + 21;           /* o nome no dirent do mOS */
        if (!strcmp(nm, ".") || !strcmp(nm, "..") || nm[0] == '.') continue;
        char full[1100];
        cx_snprintf(full, sizeof full, "%s%s%s", dir, ends(dir, "/") ? "" : "/", nm);
        q->visited++;
        test_path(q, full);
        if (ends(nm, ".app") || ends(nm, ".framework") || ends(nm, ".bundle")) continue;   /* pacote: um item só */
        void *sub = opendir(full);
        if (sub) { closedir(sub); walk(q, full, depth + 1); }
    }
    closedir(d);
}
/* volume com busca desligada (mdutil -i off): o arquivo .metadata_never_index na raiz dele */
static int never_index(const char *path) {
    char root[600], flag[700];
    if (!strncmp(path, "/Volumes/", 9)) {
        const char *e = path + 9;
        while (*e && *e != '/') e++;
        size_t n = (size_t)(e - path) < sizeof root - 1 ? (size_t)(e - path) : sizeof root - 1;
        memcpy(root, path, n);
        root[n] = 0;
    } else copy_s(root, sizeof root, "");
    cx_snprintf(flag, sizeof flag, "%s/.metadata_never_index", root);
    return cx_file_size(flag) >= 0;
}
static void sort_results(mdquery_t *q);
static void gather(mdquery_t *q) {
    if (!q->nscopes) add_computer(q);
    q->visited = 0;
    for (int i = 0; i < q->nscopes && !q->stopped; i++)
        if (!never_index(q->scopes[i])) walk(q, q->scopes[i], 0);
    sort_results(q);
}
/* ordenação pelos atributos de ordenação: o comparador do app ou a ordem natural (texto, número, data), com a opção
 * decrescente de MDQuerySetSortOptionFlagsForAttribute (kMDQueryReverseSortOrderFlag = 1) */
long CFStringCompare(id a, id b, u64 opts);
u8 CFEqual(id a, id b);
static int natural_cmp(id a, id b) {
    if (!a || !b) return a ? -1 : b ? 1 : 0;
    if (cx_is_str(a) && cx_is_str(b)) return (int)CFStringCompare(a, b, 0x220 /* localizado e numérico */);
    if (MSG(BOOL, a, "respondsToSelector:", sel_registerName("compare:"))) return (int)MSG(long, a, "compare:", b);
    return 0;
}
static mdquery_t *g_sortq;
static int sort_one(const void *x, const void *y) {
    mdquery_t *q = g_sortq;
    id a = *(id *)x, b = *(id *)y;
    long n = (long)MSG0(u64, q->sort_attrs, "count");
    if (n > 8) n = 8;
    const void *va[8] = {0}, *vb[8] = {0};
    for (long i = 0; i < n; i++) {
        id k = MSG(id, q->sort_attrs, "objectAtIndex:", (u64)i);
        va[i] = MDItemCopyAttribute(a, k);
        vb[i] = MDItemCopyAttribute(b, k);
    }
    int r = 0;
    if (q->sort_cmp) r = q->sort_cmp(va, vb, q->sort_ctx);
    else
        for (long i = 0; i < n && !r; i++) {
            id k = MSG(id, q->sort_attrs, "objectAtIndex:", (u64)i);
            r = natural_cmp((id)va[i], (id)vb[i]);
            for (int j = 0; j < 8; j++) if (q->sort_flag_attr[j] && CFEqual(q->sort_flag_attr[j], k) && (q->sort_flags[j] & 1)) r = -r;
        }
    for (long i = 0; i < n; i++) { if (va[i]) objc_release((id)va[i]); if (vb[i]) objc_release((id)vb[i]); }
    return r;
}
void qsort(void *b, size_t n, size_t s, int (*cmp)(const void *, const void *));
static void sort_results(mdquery_t *q) {
    if (!q->sort_attrs || !MSG0(u64, q->sort_attrs, "count")) return;
    u64 n = MSG0(u64, q->results, "count");
    if (n < 2) return;
    id *v = malloc(n * sizeof(id));
    if (!v) return;
    for (u64 i = 0; i < n; i++) v[i] = objc_retain(MSG(id, q->results, "objectAtIndex:", i));
    static volatile int lk;
    while (__sync_lock_test_and_set(&lk, 1)) ;
    g_sortq = q;
    qsort(v, n, sizeof(id), sort_one);
    __sync_lock_release(&lk);
    MSG0(void, q->results, "removeAllObjects");
    for (u64 i = 0; i < n; i++) { cx_arr_add(q->results, v[i]); objc_release(v[i]); }
    free(v);
}

/* ---- atualizações ao vivo: o FSEventStream acorda, a coleta é refeita e a diferença vira kMDQueryDidUpdateNotification */
typedef struct { long version; void *info; void *retain; void *release; void *desc; } fse_ctx_t;
void *FSEventStreamCreate(void *alloc, void *cb, fse_ctx_t *ctx, id paths, u64 since, double latency, u32 flags);
void FSEventStreamSetDispatchQueue(void *s, void *q);
u8 FSEventStreamStart(void *s);
static int has_path(id arr, const char *path) {
    u64 n = MSG0(u64, arr, "count");
    for (u64 i = 0; i < n; i++) { mditem_t *m = item_of(MSG(id, arr, "objectAtIndex:", i)); if (m && !strcmp(m->path, path)) return 1; }
    return 0;
}
static void post_update(mdquery_t *q, id added, id changed, id removed) {
    if (q->on_update) { q->on_update(q->hook_ctx, added, changed, removed); return; }
    id info = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    MSG(void, info, "setObject:forKey:", added, kMDQueryUpdateAddedItems);
    MSG(void, info, "setObject:forKey:", changed, kMDQueryUpdateChangedItems);
    MSG(void, info, "setObject:forKey:", removed, kMDQueryUpdateRemovedItems);
    id nc = MSG0(id, (id)cx_class_named("NSNotificationCenter"), "defaultCenter");
    if (nc) MSG(void, nc, "postNotificationName:object:userInfo:", kMDQueryDidUpdateNotification, q->self_obj, info);
}
static void regather(mdquery_t *q, char **paths, u64 n) {
    id old = q->results;
    q->results = objc_retain(cx_arr_new(0));
    gather(q);
    id added = cx_arr_new(0), changed = cx_arr_new(0), removed = cx_arr_new(0);
    u64 nn = MSG0(u64, q->results, "count"), no = MSG0(u64, old, "count");
    for (u64 i = 0; i < nn; i++) {
        id it = MSG(id, q->results, "objectAtIndex:", i);
        mditem_t *m = item_of(it);
        if (!m) continue;
        if (!has_path(old, m->path)) { cx_arr_add(added, it); continue; }
        for (u64 k = 0; k < n; k++) if (paths[k] && !strcmp(paths[k], m->path)) { cx_arr_add(changed, it); break; }
    }
    for (u64 i = 0; i < no; i++) {
        id it = MSG(id, old, "objectAtIndex:", i);
        mditem_t *m = item_of(it);
        if (m && !has_path(q->results, m->path)) cx_arr_add(removed, it);
    }
    objc_release(old);
    if (MSG0(u64, added, "count") || MSG0(u64, changed, "count") || MSG0(u64, removed, "count")) post_update(q, added, changed, removed);
}
static void fse_cb(void *stream, void *info, size_t n, void *paths, const u32 *flags, const u64 *ids) {
    (void)stream; (void)flags; (void)ids;
    mdquery_t *q = info;
    if (!q || q->stopped) return;
    if (q->updates_off) { q->pending_update = 1; return; }   /* MDQueryDisableUpdates: represa até reativar */
    regather(q, (char **)paths, (u64)n);
}
static void start_updates(mdquery_t *q) {
    if (q->stream || !q->nscopes) return;
    id arr = cx_arr_new(0);
    for (int i = 0; i < q->nscopes; i++) cx_arr_add(arr, cx_str(q->scopes[i]));
    fse_ctx_t ctx = {0, q, 0, 0, 0};
    q->stream = FSEventStreamCreate(0, (void *)fse_cb, &ctx, arr, 0xFFFFFFFFFFFFFFFFull /* desde agora */, 0.5,
                                    0x10 /* FileEvents */);
    if (!q->stream) return;
    FSEventStreamSetDispatchQueue(q->stream, q->queue ? q->queue : dispatch_get_main_queue());
    FSEventStreamStart(q->stream);
}
static void post_finish(id query) {
    id nc = MSG0(id, (id)cx_class_named("NSNotificationCenter"), "defaultCenter");
    if (nc) MSG(void, nc, "postNotificationName:object:", kMDQueryDidFinishNotification, query);
}
static void async_gather(void *ctx) {
    id query = ctx;
    mdquery_t *q = query_of(query);
    if (q && !q->stopped) {
        gather(q);
        q->gathering = 0;
        if (!q->stopped) post_finish(query);
        if (!q->stopped && q->wants_updates) start_updates(q);
    }
    objc_release(query);
}

u8 MDQueryExecute(id query, u32 options) {
    mdquery_t *q = query_of(query);
    if (!q || q->running) return 0;
    q->running = 1;
    q->stopped = 0;
    q->self_obj = query;
    q->wants_updates = (options & MD_UPDATES) != 0;
    if (options & MD_SYNC) {
        gather(q);
        if (q->wants_updates) start_updates(q);
        return 1;
    }
    q->gathering = 1;
    dispatch_async_f(q->queue ? q->queue : dispatch_get_main_queue(), objc_retain(query), async_gather);
    return 1;
}
void MDQueryStop(id query) {
    mdquery_t *q = query_of(query);
    if (!q) return;
    q->stopped = 1;
    q->gathering = 0;
    if (q->stream) { FSEventStreamStop(q->stream); FSEventStreamInvalidate(q->stream); FSEventStreamRelease(q->stream); q->stream = 0; }
}
u8 MDQueryIsGatheringComplete(id query) { mdquery_t *q = query_of(query); return q && q->running && !q->gathering; }
void MDQueryDisableUpdates(id query) { mdquery_t *q = query_of(query); if (q) q->updates_off++; }
/* reativar com mudança represada: a próxima leva de eventos refaz a coleta e avisa tudo de uma vez */
void MDQueryEnableUpdates(id query) {
    mdquery_t *q = query_of(query);
    if (!q || q->updates_off <= 0) return;
    if (--q->updates_off == 0 && q->pending_update) { q->pending_update = 0; regather(q, 0, 0); }
}
long MDQueryGetResultCount(id query) { mdquery_t *q = query_of(query); return q ? (long)MSG0(u64, q->results, "count") : 0; }
const void *MDQueryGetResultAtIndex(id query, long idx) {
    mdquery_t *q = query_of(query);
    if (!q || idx < 0 || idx >= MDQueryGetResultCount(query)) return 0;
    return MSG(id, q->results, "objectAtIndex:", (u64)idx);
}
long MDQueryGetIndexOfResult(id query, const void *result) {
    mdquery_t *q = query_of(query);
    long n = MDQueryGetResultCount(query);
    for (long i = 0; q && i < n; i++) if (MSG(id, q->results, "objectAtIndex:", (u64)i) == (id)result) return i;
    return -1;
}
void *MDQueryGetAttributeValueOfResultAtIndex(id query, id name, long idx) {
    id it = (id)MDQueryGetResultAtIndex(query, idx);
    id v = it ? MDItemCopyAttribute(it, name) : 0;
    return v ? objc_autorelease(v) : 0;
}
unsigned long MDQueryGetTypeID(void) { return 0x4D445100u; }
unsigned long MDItemGetTypeID(void) { return 0x4D444900u; }

/* ================================================================ NSMetadataQuery e NSMetadataItem
 * A classe existe em foundation_more.c (fachada); os métodos daqui a substituem. A busca é a do MDQuery acima com a
 * consulta "true" e o NSPredicate do app como filtro (evaluateWithObject: num NSMetadataItem, que responde por
 * valueForKey:). Escopos: pastas (NSString/NSURL), NSMetadataQueryUserHomeScope, …LocalComputerScope e as variantes
 * Indexed; os de iCloud (…Ubiquitous…) e de rede não têm itens. Ordem: sortDescriptors (chave e ascending).
 * Notificações no fio principal: DidStartGathering, DidFinishGathering e, com a consulta viva, DidUpdate com
 * NSMetadataQueryUpdateAdded/Changed/RemovedItemsKey (o FSEventStream do MDQuery). groupedResults e valueLists: o
 * mOS não agrupa; devolvem vazio. */
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
static char k_mditem, k_mqstate;
typedef struct {
    id predicate, scopes, sorts, delegate;
    id mdq;                  /* __MDQuery */
    id results;              /* NSMutableArray de NSMetadataItem */
    int started, gathering, stopped, disabled;
    id self_obj;
} mqs_t;
static cx_class_t MQS_c, MQS_m;
static void mqs_dealloc(id self, SEL c) {
    mqs_t *m = *(mqs_t **)((u8 *)self + 8);
    (void)c;
    if (m) {
        if (m->predicate) objc_release(m->predicate);
        if (m->scopes) objc_release(m->scopes);
        if (m->sorts) objc_release(m->sorts);
        if (m->mdq) { MDQueryStop(m->mdq); objc_release(m->mdq); }
        if (m->results) objc_release(m->results);
        free(m);
    }
    cx_object_free(self);
}
static mqs_t *mqs(id self) {
    id box = objc_getAssociatedObject(self, &k_mqstate);
    if (!box) {
        box = cx_alloc_instance(&MQS_c);
        mqs_t *m = calloc(1, sizeof *m);
        *(mqs_t **)((u8 *)box + 8) = m;
        m->self_obj = self;
        m->results = objc_retain(cx_arr_new(0));
        objc_setAssociatedObject(self, &k_mqstate, box, 1 /* RETAIN_NONATOMIC */);
        objc_release(box);
    }
    return *(mqs_t **)((u8 *)box + 8);
}
/* NSMetadataItem sobre um MDItem */
static id mi_wrap(id mditem) {
    id o = MSG0(id, (id)cx_class_named("NSMetadataItem"), "alloc");
    o = MSG0(id, o, "init");
    if (o) objc_setAssociatedObject(o, &k_mditem, mditem, 1);
    return o;                                          /* +1 */
}
static id mi_value(id self, SEL c, id attr) {
    (void)c;
    id it = objc_getAssociatedObject(self, &k_mditem);
    if (!it || !attr) return 0;
    if (cx_is_str(attr) && !strcmp(cx_cstr(attr), "kMDItemURL")) {
        id p = MDItemCopyAttribute(it, cx_str("kMDItemPath"));
        id u = p ? MSG(id, (id)cx_class_named("NSURL"), "fileURLWithPath:", p) : 0;
        if (p) objc_release(p);
        return u;
    }
    id v = MDItemCopyAttribute(it, attr);
    return v ? objc_autorelease(v) : 0;
}
static id mi_values(id self, SEL c, id attrs) {
    (void)c;
    id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    u64 n = attrs ? MSG0(u64, attrs, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, attrs, "objectAtIndex:", i);
        id v = mi_value(self, 0, k);
        if (v) MSG(void, d, "setObject:forKey:", v, k);
    }
    return d;
}
static id mi_attributes(id self, SEL c) {
    (void)c;
    id it = objc_getAssociatedObject(self, &k_mditem);
    id a = it ? MDItemCopyAttributeNames(it) : 0;
    return a ? objc_autorelease(a) : cx_arr_new(0);
}
static id mi_init_url(id self, SEL c, id url) {
    (void)c;
    id it = url ? MDItemCreateWithURL(0, url) : 0;
    if (!it) { objc_release(self); return 0; }
    objc_setAssociatedObject(self, &k_mditem, it, 1);
    objc_release(it);
    return self;
}

static int mq_filter(void *ctx, id mditem) {
    mqs_t *m = ctx;
    if (!m->predicate) return 1;
    id w = mi_wrap(mditem);
    BOOL ok = w ? MSG(BOOL, m->predicate, "evaluateWithObject:", w) : NO;
    if (w) objc_release(w);
    return ok;
}
static int sort_cmp(id a, id b, id sorts) {
    u64 n = sorts ? MSG0(u64, sorts, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id sd = MSG(id, sorts, "objectAtIndex:", i);
        id key = MSG0(id, sd, "key");
        BOOL asc = MSG0(BOOL, sd, "ascending");
        id va = mi_value(a, 0, key), vb = mi_value(b, 0, key);
        long r = 0;
        if (va && vb) r = MSG(long, va, "compare:", vb);
        else if (va) r = 1;
        else if (vb) r = -1;
        if (r) return asc ? (int)r : (int)-r;
    }
    return 0;
}
static void mq_fill(mqs_t *m) {
    id fresh = cx_arr_new(0);
    long n = MDQueryGetResultCount(m->mdq);
    for (long i = 0; i < n; i++) {
        id w = mi_wrap((id)MDQueryGetResultAtIndex(m->mdq, i));
        if (!w) continue;
        /* inserção ordenada (estável) */
        u64 c = MSG0(u64, fresh, "count"), at = c;
        if (m->sorts) for (u64 k = 0; k < c; k++) if (sort_cmp(w, MSG(id, fresh, "objectAtIndex:", k), m->sorts) < 0) { at = k; break; }
        MSG(void, fresh, "insertObject:atIndex:", w, at);
        objc_release(w);
    }
    objc_release(m->results);
    m->results = objc_retain(fresh);
}
static void mq_post(id obj, const char *name, id info) {
    id nc = MSG0(id, (id)cx_class_named("NSNotificationCenter"), "defaultCenter");
    if (nc) MSG(void, nc, "postNotificationName:object:userInfo:", cx_str(name), obj, info);
}
static id wrap_all(id mditems) {
    id out = cx_arr_new(0);
    u64 n = MSG0(u64, mditems, "count");
    for (u64 i = 0; i < n; i++) { id w = mi_wrap(MSG(id, mditems, "objectAtIndex:", i)); if (w) { cx_arr_add(out, w); objc_release(w); } }
    return out;
}
static void mq_on_update(void *ctx, id added, id changed, id removed) {
    mqs_t *m = ctx;
    if (m->stopped) return;
    mq_fill(m);
    id info = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    MSG(void, info, "setObject:forKey:", wrap_all(added), cx_str("kMDQueryUpdateAddedItems"));
    MSG(void, info, "setObject:forKey:", wrap_all(changed), cx_str("kMDQueryUpdateChangedItems"));
    MSG(void, info, "setObject:forKey:", wrap_all(removed), cx_str("kMDQueryUpdateRemovedItems"));
    mq_post(m->self_obj, "NSMetadataQueryDidUpdateNotification", info);
}
static void mq_finish_main(void *ctx) {
    id self = ctx;
    mqs_t *m = mqs(self);
    if (!m->stopped) {
        mq_fill(m);
        m->gathering = 0;
        mq_post(self, "NSMetadataQueryDidFinishGatheringNotification", 0);
        mdquery_t *q = query_of(m->mdq);
        if (q && !m->stopped) { q->queue = dispatch_retain(dispatch_get_main_queue()); start_updates(q); }
    }
    objc_release(self);
}
static void mq_gather_bg(void *ctx) {
    id self = ctx;
    mqs_t *m = mqs(self);
    if (!m->stopped) MDQueryExecute(m->mdq, MD_SYNC);
    dispatch_async_f(dispatch_get_main_queue(), self, mq_finish_main);
}
static void add_ns_scope(mdquery_t *q, id sc) {
    const char *p = 0;
    if (sc && cx_is_str(sc)) p = cx_cstr(sc);
    else if (sc && MSG(BOOL, sc, "respondsToSelector:", sel_registerName("path"))) { id ps = MSG0(id, sc, "path"); p = ps ? cx_cstr(ps) : 0; }
    if (!p) return;
    if (!strcmp(p, "NSMetadataQueryUserHomeScope")) add_home(q);
    else if (!strcmp(p, "NSMetadataQueryLocalComputerScope") || !strcmp(p, "NSMetadataQueryIndexedLocalComputerScope")) add_computer(q);
    else if (p[0] == '/') add_scope(q, p);
    /* …Ubiquitous… (iCloud) e …NetworkScope: sem itens no mOS */
}
static BOOL mq_start(id self, SEL c) {
    (void)c;
    mqs_t *m = mqs(self);
    if (m->started && !m->stopped) return NO;
    id mdq = MDQueryCreate(0, cx_str("true"), 0, 0);
    if (!mdq) return NO;
    mdquery_t *q = query_of(mdq);
    q->filter = mq_filter;
    q->on_update = mq_on_update;
    q->hook_ctx = m;
    q->wants_updates = 1;
    int any = 0;
    u64 n = m->scopes ? MSG0(u64, m->scopes, "count") : 0;
    for (u64 i = 0; i < n; i++) add_ns_scope(q, MSG(id, m->scopes, "objectAtIndex:", i));
    any = q->nscopes > 0;
    if (!any && n) q->stopped = 1;                    /* só escopos sem itens (iCloud): termina vazio */
    if (m->mdq) { MDQueryStop(m->mdq); objc_release(m->mdq); }
    m->mdq = mdq;
    m->started = 1;
    m->stopped = 0;
    m->gathering = 1;
    mq_post(self, "NSMetadataQueryDidStartGatheringNotification", 0);
    dispatch_async_f(dispatch_get_global_queue(0, 0), objc_retain(self), mq_gather_bg);
    return YES;
}
static void mq_stop(id self, SEL c) { (void)c; mqs_t *m = mqs(self); m->stopped = 1; m->gathering = 0; if (m->mdq) MDQueryStop(m->mdq); }
static void mq_set_pred(id self, SEL c, id p) { (void)c; mqs_t *m = mqs(self); if (p) objc_retain(p); if (m->predicate) objc_release(m->predicate); m->predicate = p; }
static id mq_pred(id self, SEL c) { (void)c; return mqs(self)->predicate; }
static void mq_set_scopes(id self, SEL c, id a) { (void)c; mqs_t *m = mqs(self); if (a) objc_retain(a); if (m->scopes) objc_release(m->scopes); m->scopes = a; }
static id mq_scopes(id self, SEL c) { (void)c; mqs_t *m = mqs(self); return m->scopes ? m->scopes : cx_arr_new(0); }
static void mq_set_sorts(id self, SEL c, id a) { (void)c; mqs_t *m = mqs(self); if (a) objc_retain(a); if (m->sorts) objc_release(m->sorts); m->sorts = a; }
static id mq_sorts(id self, SEL c) { (void)c; mqs_t *m = mqs(self); return m->sorts ? m->sorts : cx_arr_new(0); }
static void mq_set_delegate(id self, SEL c, id d) { (void)c; mqs(self)->delegate = d; }   /* fraco, como no Foundation */
static id mq_delegate(id self, SEL c) { (void)c; return mqs(self)->delegate; }
static id mq_results(id self, SEL c) { (void)c; return MSG0(id, mqs(self)->results, "copy"); }
static u64 mq_count(id self, SEL c) { (void)c; return MSG0(u64, mqs(self)->results, "count"); }
static id mq_at(id self, SEL c, u64 i) { (void)c; mqs_t *m = mqs(self); return i < MSG0(u64, m->results, "count") ? MSG(id, m->results, "objectAtIndex:", i) : 0; }
static u64 mq_index(id self, SEL c, id r) { (void)c; return MSG(u64, mqs(self)->results, "indexOfObject:", r); }
static id mq_value_at(id self, SEL c, id attr, u64 i) { (void)c; id it = mq_at(self, 0, i); return it ? mi_value(it, 0, attr) : 0; }
static BOOL mq_started(id self, SEL c) { (void)c; mqs_t *m = mqs(self); return m->started && !m->stopped; }
static BOOL mq_gathering(id self, SEL c) { (void)c; return mqs(self)->gathering; }
static BOOL mq_stopped(id self, SEL c) { (void)c; return mqs(self)->stopped; }
static void mq_disable(id self, SEL c) { (void)c; mqs_t *m = mqs(self); if (m->mdq) MDQueryDisableUpdates(m->mdq); }
static void mq_enable(id self, SEL c) { (void)c; mqs_t *m = mqs(self); if (m->mdq) MDQueryEnableUpdates(m->mdq); }
static void mq_enum(id self, SEL c, void *blk) {
    (void)c;
    mqs_t *m = mqs(self);
    void (*inv)(void *, id, u64, BOOL *) = *(void **)((u8 *)blk + 16);
    u64 n = MSG0(u64, m->results, "count");
    for (u64 i = 0; i < n; i++) { BOOL stop = NO; inv(blk, MSG(id, m->results, "objectAtIndex:", i), i, &stop); if (stop) break; }
}
static id mq_empty_arr(id self, SEL c) { (void)self; (void)c; return cx_arr_new(0); }
static id mq_empty_dict(id self, SEL c) { (void)self; (void)c; return MSG0(id, (id)cx_class_named("NSDictionary"), "dictionary"); }
static void mq_ignore(id self, SEL c, id v) { (void)self; (void)c; (void)v; }
static void mq_ignore_d(id self, SEL c, double v) { (void)self; (void)c; (void)v; }
static double mq_batch(id self, SEL c) { (void)self; (void)c; return 1.0; }

void cx_mdquery_init(void) {
    static int done;
    if (done++) return;
    md_boot();
    cx_define_class(&MQS_c, &MQS_m, "__MOSMetadataQueryState", cx_class_named("NSObject"), 8);
    cx_add_method(&MQS_c, "dealloc", (void *)mqs_dealloc);
    cx_class_t *mi = cx_class_named("NSMetadataItem"), *mq = cx_class_named("NSMetadataQuery");
    if (mi) {
        cx_add_method(mi, "valueForAttribute:", (void *)mi_value);
        cx_add_method(mi, "valueForKey:", (void *)mi_value);   /* o NSPredicate lê por KVC */
        cx_add_method(mi, "valuesForAttributes:", (void *)mi_values);
        cx_add_method(mi, "attributes", (void *)mi_attributes);
        cx_add_method(mi, "initWithURL:", (void *)mi_init_url);
    }
    if (!mq) return;
    cx_add_method(mq, "startQuery", (void *)mq_start);
    cx_add_method(mq, "stopQuery", (void *)mq_stop);
    cx_add_method(mq, "setPredicate:", (void *)mq_set_pred);
    cx_add_method(mq, "predicate", (void *)mq_pred);
    cx_add_method(mq, "setSearchScopes:", (void *)mq_set_scopes);
    cx_add_method(mq, "searchScopes", (void *)mq_scopes);
    cx_add_method(mq, "setSortDescriptors:", (void *)mq_set_sorts);
    cx_add_method(mq, "sortDescriptors", (void *)mq_sorts);
    cx_add_method(mq, "setDelegate:", (void *)mq_set_delegate);
    cx_add_method(mq, "delegate", (void *)mq_delegate);
    cx_add_method(mq, "results", (void *)mq_results);
    cx_add_method(mq, "resultCount", (void *)mq_count);
    cx_add_method(mq, "resultAtIndex:", (void *)mq_at);
    cx_add_method(mq, "indexOfResult:", (void *)mq_index);
    cx_add_method(mq, "valueOfAttribute:forResultAtIndex:", (void *)mq_value_at);
    cx_add_method(mq, "isStarted", (void *)mq_started);
    cx_add_method(mq, "isGathering", (void *)mq_gathering);
    cx_add_method(mq, "isStopped", (void *)mq_stopped);
    cx_add_method(mq, "disableUpdates", (void *)mq_disable);
    cx_add_method(mq, "enableUpdates", (void *)mq_enable);
    cx_add_method(mq, "enumerateResultsUsingBlock:", (void *)mq_enum);
    cx_add_method(mq, "groupedResults", (void *)mq_empty_arr);
    cx_add_method(mq, "valueLists", (void *)mq_empty_dict);
    cx_add_method(mq, "setValueListAttributes:", (void *)mq_ignore);
    cx_add_method(mq, "setGroupingAttributes:", (void *)mq_ignore);
    cx_add_method(mq, "setOperationQueue:", (void *)mq_ignore);
    cx_add_method(mq, "setNotificationBatchingInterval:", (void *)mq_ignore_d);
    cx_add_method(mq, "notificationBatchingInterval", (void *)mq_batch);
}

/* ---- limite, ordenação e lotes */
void MDQuerySetMaxCount(id query, long n) { mdquery_t *q = query_of(query); if (q) q->max_count = n > 0 ? n : 0; }
void MDQuerySetSortComparator(id query, void *cmp, void *ctx) {
    mdquery_t *q = query_of(query);
    if (q) { q->sort_cmp = (int (*)(const void **, const void **, void *))cmp; q->sort_ctx = ctx; }
}
u8 MDQuerySetSortOptionFlagsForAttribute(id query, id attr, u32 flags) {
    mdquery_t *q = query_of(query);
    if (!q || !attr || !q->sort_attrs || !MSG(BOOL, q->sort_attrs, "containsObject:", attr)) return 0;
    for (int i = 0; i < 8; i++)
        if (!q->sort_flag_attr[i] || CFEqual(q->sort_flag_attr[i], attr)) {
            if (!q->sort_flag_attr[i]) q->sort_flag_attr[i] = objc_retain(attr);
            q->sort_flags[i] = flags;
            return 1;
        }
    return 0;
}
u32 MDQueryGetSortOptionFlagsForAttribute(id query, id attr) {
    mdquery_t *q = query_of(query);
    for (int i = 0; q && attr && i < 8; i++) if (q->sort_flag_attr[i] && CFEqual(q->sort_flag_attr[i], attr)) return q->sort_flags[i];
    return 0;
}
/* MDQueryBatchingParams {first_max_num, first_max_ms, progress_max_num, progress_max_ms, update_max_num, update_max_ms} */
void MDQuerySetBatchingParameters(id query, const u64 *p) { mdquery_t *q = query_of(query); if (q && p) memcpy(q->batch, p, sizeof q->batch); }
void MDQueryGetBatchingParameters(id query, u64 *out) {
    mdquery_t *q = query_of(query);
    static const u64 def[6] = {1000, 1000, 5000, 1000, 100, 2000};   /* os padrões do macOS */
    if (!out) return;
    memcpy(out, q && (q->batch[0] || q->batch[1]) ? q->batch : def, sizeof def);
}
