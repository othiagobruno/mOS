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
 * Atualizações ao vivo (kMDQueryWantsUpdates) não chegam: sem o daemon de índice nada observa o disco; o resultado
 * fica o da coleta. */
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
void *dispatch_retain(void *q);
void dispatch_release(void *q);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
/* tamanho do arquivo (SYS_STAT); -1 se não existe */
static long long cx_file_size(const char *path) {
    mos_stat_t st;
    if (SYS2(SYS_STAT, path, &st) < 0) return -1;
    return (long long)st.size;
}

/* constantes (os valores são os nomes, como no macOS) */
#define MDK(sym) static nsstr_t sym##_o = { &NSConstantString_c, 0, (char *)#sym, sizeof(#sym) - 1 }; id sym = (id)&sym##_o;
extern cx_class_t NSConstantString_c;
MDK(kMDItemCFBundleIdentifier) MDK(kMDItemDisplayName) MDK(kMDItemPath) MDK(kMDItemFSName) MDK(kMDItemContentType)
MDK(kMDItemContentTypeTree) MDK(kMDItemKind) MDK(kMDItemFSSize)
MDK(kMDQueryDidFinishNotification) MDK(kMDQueryDidUpdateNotification) MDK(kMDQueryProgressNotification)
MDK(kMDQueryScopeComputer) MDK(kMDQueryScopeHome) MDK(kMDQueryScopeNetwork) MDK(kMDQueryScopeAllIndexed)
MDK(kMDQueryScopeComputerIndexed) MDK(kMDQueryScopeNetworkIndexed)
MDK(kMDQueryUpdateAddedItems) MDK(kMDQueryUpdateChangedItems) MDK(kMDQueryUpdateRemovedItems)

enum { MD_SYNC = 1, MD_UPDATES = 4, MD_MAX_DEPTH = 12, MD_MAX_VISIT = 50000 };

/* ---------------------------------------------------------------- MDItem */
typedef struct {
    char *path;
    int have_info, have_type;
    char name[256], display[256], bundle[256], type[128], kind[128];
    char tree[8][128];
    int ntree;
} mditem_t;

static cx_class_t MDItem_c, MDItem_m, MDQuery_c, MDQuery_m;
static int md_ready;
static mditem_t *item_of(id o) { return o && object_getClass(o) == &MDItem_c ? *(mditem_t **)((u8 *)o + 8) : 0; }

static void item_dealloc(id self, SEL c) {
    mditem_t *m = item_of(self);
    (void)c;
    if (m) { free(m->path); free(m); }
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

static const char *item_get(void *ctx, const char *attr, int i) {
    mditem_t *m = ctx;
    if (!strcmp(attr, "kMDItemContentTypeTree")) { item_type(m); return i < m->ntree ? m->tree[i] : 0; }
    if (i) return 0;
    if (!strcmp(attr, "kMDItemPath")) return m->path;
    item_info(m);
    if (!strcmp(attr, "kMDItemFSName")) return m->name;
    if (!strcmp(attr, "kMDItemDisplayName")) return m->display;
    if (!strcmp(attr, "kMDItemCFBundleIdentifier")) return m->bundle[0] ? m->bundle : 0;
    if (!strcmp(attr, "kMDItemContentType")) { item_type(m); return m->type; }
    if (!strcmp(attr, "kMDItemKind")) { item_type(m); return m->kind[0] ? m->kind : 0; }
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
    if (!strcmp(a, "kMDItemFSSize")) {
        long long sz = cx_file_size(m->path);
        return sz >= 0 ? objc_retain(cx_num_i(sz)) : 0;
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
                                "kMDItemContentTypeTree", "kMDItemKind", "kMDItemFSSize", "kMDItemCFBundleIdentifier"};
    mditem_t *m = item_of(item);
    if (!m) return 0;
    item_info(m);
    id arr = cx_arr_new(0);
    for (unsigned i = 0; i < sizeof all / sizeof all[0]; i++) {
        if (i == 7 && !m->bundle[0]) continue;
        cx_arr_add(arr, cx_str(all[i]));
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
    int running, gathering, stopped, updates_off, visited;
};
static mdquery_t *query_of(id o) { return o && object_getClass(o) == &MDQuery_c ? *(mdquery_t **)((u8 *)o + 8) : 0; }
static void free_scopes(mdquery_t *q) {
    for (int i = 0; i < q->nscopes; i++) free(q->scopes[i]);
    free(q->scopes);
    q->scopes = 0;
    q->nscopes = 0;
}
static void query_dealloc(id self, SEL c) {
    mdquery_t *q = query_of(self);
    (void)c;
    if (q) {
        mdq_free(q->pred);
        if (q->results) objc_release(q->results);
        if (q->query_text) objc_release(q->query_text);
        if (q->queue) dispatch_release(q->queue);
        free_scopes(q);
        free(q);
    }
    *(void **)((u8 *)self + 8) = 0;
    cx_object_free(self);
}

id MDQueryCreate(void *alloc, id query, id valueListAttrs, id sortingAttrs) {
    (void)alloc; (void)valueListAttrs; (void)sortingAttrs;
    char err[96];
    const char *s = query && cx_is_str(query) ? cx_cstr(query) : 0;
    if (!s) return 0;
    mdq_node *p = mdq_parse(s, err, sizeof err);
    if (!p) return 0;                                  /* consulta inválida: NULL, como no macOS */
    md_boot();
    mdquery_t *q = calloc(1, sizeof *q);
    if (!q) { mdq_free(p); return 0; }
    q->pred = p;
    q->query_text = objc_retain(query);
    q->results = objc_retain(cx_arr_new(0));
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
    id it = item_new(path);
    if (it) { cx_arr_add(q->results, it); objc_release(it); }
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
static void gather(mdquery_t *q) {
    if (!q->nscopes) add_computer(q);
    q->visited = 0;
    for (int i = 0; i < q->nscopes && !q->stopped; i++) walk(q, q->scopes[i], 0);
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
    }
    objc_release(query);
}

u8 MDQueryExecute(id query, u32 options) {
    mdquery_t *q = query_of(query);
    if (!q || q->running) return 0;
    q->running = 1;
    q->stopped = 0;
    if (options & MD_SYNC) {
        gather(q);
        return 1;
    }
    q->gathering = 1;
    dispatch_async_f(q->queue ? q->queue : dispatch_get_main_queue(), objc_retain(query), async_gather);
    return 1;
}
void MDQueryStop(id query) { mdquery_t *q = query_of(query); if (q) { q->stopped = 1; q->gathering = 0; } }
u8 MDQueryIsGatheringComplete(id query) { mdquery_t *q = query_of(query); return q && q->running && !q->gathering; }
void MDQueryDisableUpdates(id query) { mdquery_t *q = query_of(query); if (q) q->updates_off++; }
void MDQueryEnableUpdates(id query) { mdquery_t *q = query_of(query); if (q && q->updates_off > 0) q->updates_off--; }
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
