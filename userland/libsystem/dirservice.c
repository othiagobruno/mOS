/*
 * mOS libSystem — DirectoryService (DirectoryService.framework, API dsXxx) sobre as contas do sistema (SYS_ACCOUNT).
 * Um nó local, /Local/Default, que também responde como /Search; registros de usuário (as contas) e de grupo (staff,
 * admin, everyone), com os atributos padrão (RecordName, RealName, UniqueID, PrimaryGroupID, NFSHomeDirectory,
 * UserShell, GeneratedUID, AppleMetaNodeLocation, RecordType; nos grupos, GroupMembership).
 * dsDoDirNodeAuth confere a senha no kernel (dsAuthNodeNativeCanUseClearText/CannotUseClearText e dsAuthClearText:
 * buffer = [u32 tamanho][nome][u32 tamanho][senha]); a trava de 30 s depois de 5 erros é a do kernel.
 * Os resultados de dsFindDirNodes e dsGetRecordList ficam numa tabela do processo; o buffer do app leva só a marca e o
 * índice (o formato do buffer é privado, como no macOS). Tudo vem numa leva só: o continueData volta sempre 0.
 */
#include "sys.h"
#include "libc.h"

typedef i32 tDirStatus;
typedef u32 tDirReference, tDirNodeReference, tContextData, tAttributeListRef, tAttributeValueListRef, tDirPatternMatch;
typedef struct { u32 fBufferSize, fBufferLength; char fBufferData[1]; } tDataBuffer;
typedef tDataBuffer tDataNode;
typedef struct { u32 fDataNodeCount; tDataNode *fDataListHead; } tDataList;
typedef struct { u32 g, d, n, o, a; } tAccessControlEntry;
typedef struct { u32 fAttributeValueID; tDataNode fAttributeValueData; } tAttributeValueEntry;
typedef struct { u32 fReserved1; tAccessControlEntry fReserved2; u32 fAttributeValueCount, fAttributeDataSize, fAttributeValueMaxSize; tDataNode fAttributeSignature; } tAttributeEntry;
typedef struct { u32 fReserved1; tAccessControlEntry fReserved2; u32 fRecordAttributeCount; tDataNode fRecordNameAndType; } tRecordEntry;
unsigned getuid(void);
void CC_MD5(const void *d, u32 n, u8 *md);
int snprintf(char *b, size_t n, const char *fmt, ...);
char *strstr(const char *h, const char *n);
char *strcat(char *d, const char *s);
char *strncat(char *d, const char *s, size_t n);

enum { eDSNoErr = 0, eDSNodeNotFound = -14008, eDSInvalidIndex = -14061, eDSIndexOutOfRange = -14062, eDSInvalidReference = -14071,
       eDSInvalidDirRef = -14073, eDSInvalidNodeRef = -14074, eDSInvalidAttrListRef = -14076, eDSInvalidAttrValueRef = -14077,
       eDSInvalidBuffFormat = -14079, eDSAuthFailed = -14090, eDSAuthMethodNotSupported = -14091, eDSAuthInBuffFormatError = -14094,
       eDSAuthUnknownUser = -14098, eDSInvalidRecordType = -14130, eDSRecordNotFound = -14136, eDSNullParameter = -14200,
       eDSNullDataBuff = -14201, eDSNullNodeName = -14202, eDSNullRecName = -14204, eDSNullRecType = -14206,
       eDSNullNodeNamePattern = -14222, eDSEmptyParameter = -14230, eDSBufferTooSmall = -14260 };
enum { eDSAnyMatch = 1, eDSExact = 0x2001, eDSStartsWith = 0x2002, eDSEndsWith = 0x2003, eDSContains = 0x2004, eDSiExact = 0x2101,
       eDSiStartsWith = 0x2102, eDSiEndsWith = 0x2103, eDSiContains = 0x2104, eDSLocalNodeNames = 0x2200, eDSSearchNodeName = 0x2201,
       eDSConfigNodeName = 0x2202, eDSLocalHostedNodes = 0x2203, eDSContactsSearchNodeName = 0x2204, eDSNetworkSearchNodeName = 0x2205,
       eDSDefaultNetworkNodes = 0x2206 };
#define REC_USERS "dsRecTypeStandard:Users"
#define REC_GROUPS "dsRecTypeStandard:Groups"
#define A_NAME "dsAttrTypeStandard:RecordName"
#define A_REAL "dsAttrTypeStandard:RealName"
#define A_UID "dsAttrTypeStandard:UniqueID"
#define A_GID "dsAttrTypeStandard:PrimaryGroupID"
#define A_HOME "dsAttrTypeStandard:NFSHomeDirectory"
#define A_SHELL "dsAttrTypeStandard:UserShell"
#define A_GUID "dsAttrTypeStandard:GeneratedUID"
#define A_META "dsAttrTypeStandard:AppleMetaNodeLocation"
#define A_TYPE "dsAttrTypeStandard:RecordType"
#define A_MEMBERS "dsAttrTypeStandard:GroupMembership"
#define A_ALL "dsAttributesAll"
#define A_STDALL "dsAttributesStandardAll"
#define NODE "/Local/Default"

/* ---- referências: uma tabela de objetos com tipo */
enum { R_DIR = 1, R_NODE, R_ATTRLIST, R_VALLIST, R_RESULT };
typedef struct { int kind; void *p; } ref_t;
static ref_t g_refs[1024];
static u32 ref_new(int kind, void *p) {
    for (u32 i = 1; i < 1024; i++) if (!g_refs[i].kind) { g_refs[i].kind = kind; g_refs[i].p = p; return i; }
    return 0;
}
static void *ref_get(u32 r, int kind) { return r && r < 1024 && g_refs[r].kind == kind ? (g_refs[r].p ? g_refs[r].p : (void *)1) : 0; }
static void ref_del(u32 r) { if (r && r < 1024) { g_refs[r].kind = 0; g_refs[r].p = 0; } }

/* ---- registros */
typedef struct { char type[64]; char *vals[8]; int n; } attr_t;
typedef struct { char name[64], rtype[32]; attr_t a[12]; int na; } rec_t;
typedef struct { int is_nodes; char nodes[4][64]; int nnodes; rec_t *recs; int nrecs; } result_t;
static void add_val(rec_t *r, const char *type, const char *v) {
    attr_t *a = 0;
    for (int i = 0; i < r->na; i++) if (!strcmp(r->a[i].type, type)) a = &r->a[i];
    if (!a) { if (r->na == 12) return; a = &r->a[r->na++]; snprintf(a->type, sizeof a->type, "%s", type); }
    if (a->n < 8) { size_t l = strlen(v) + 1; a->vals[a->n] = malloc(l); if (a->vals[a->n]) memcpy(a->vals[a->n++], v, l); }
}
static void guid_of(const char *kind, const char *name, char *out) {             /* UUID v3 estável do nome */
    char buf[128];
    u8 h[16];
    int n = snprintf(buf, sizeof buf, "mOS:%s:%s", kind, name);
    CC_MD5(buf, (u32)n, h);
    h[6] = (u8)((h[6] & 0x0f) | 0x30); h[8] = (u8)((h[8] & 0x3f) | 0x80);
    snprintf(out, 40, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X", h[0], h[1], h[2], h[3], h[4], h[5], h[6],
             h[7], h[8], h[9], h[10], h[11], h[12], h[13], h[14], h[15]);
}
static int load_all(rec_t **out) {
    long cnt = SYS1(SYS_ACCOUNT, MOS_ACCT_COUNT);
    if (cnt < 0) cnt = 0;
    rec_t *r = calloc((size_t)cnt + 3, sizeof *r);
    if (!r) return 0;
    mos_account_t cur;
    int has_cur = SYS3(SYS_ACCOUNT, MOS_ACCT_CURRENT, 0, &cur) >= 0;
    int n = 0;
    char admins[512] = "", all[512] = "", num[16], g[40];
    for (long i = 0; i < cnt; i++) {
        mos_account_t a;
        if (SYS3(SYS_ACCOUNT, MOS_ACCT_GET, i, &a) < 0) continue;
        rec_t *u = &r[n++];
        snprintf(u->name, sizeof u->name, "%s", a.name);
        snprintf(u->rtype, sizeof u->rtype, REC_USERS);
        add_val(u, A_NAME, a.name);
        if (a.full[0] && strcmp(a.full, a.name)) add_val(u, A_NAME, a.full);
        add_val(u, A_REAL, a.full[0] ? a.full : a.name);
        snprintf(num, sizeof num, "%u", getuid());                               /* toda conta roda com o mesmo uid */
        add_val(u, A_UID, num);
        add_val(u, A_GID, "20");
        char home[300];
        const char *env = getenv("HOME");
        if (has_cur && !strcmp(cur.name, a.name) && env) snprintf(home, sizeof home, "%s", env);
        else snprintf(home, sizeof home, "/Users/%s", a.name);
        add_val(u, A_HOME, home);
        const char *sh = getenv("SHELL");
        add_val(u, A_SHELL, sh ? sh : "/bin/msh");
        guid_of("user", a.name, g);
        add_val(u, A_GUID, g);
        add_val(u, A_META, NODE);
        add_val(u, A_TYPE, REC_USERS);
        size_t l = strlen(all);
        snprintf(all + l, sizeof all - l, "%s%s", l ? "," : "", a.name);
        if (a.admin) { l = strlen(admins); snprintf(admins + l, sizeof admins - l, "%s%s", l ? "," : "", a.name); }
    }
    static const struct { const char *n, *full, *gid; int which; } grp[] = {{"staff", "Staff", "20", 0}, {"admin", "Administradores", "80", 1}, {"everyone", "Todos", "12", 0}};
    for (int k = 0; k < 3; k++) {
        rec_t *gr = &r[n++];
        snprintf(gr->name, sizeof gr->name, "%s", grp[k].n);
        snprintf(gr->rtype, sizeof gr->rtype, REC_GROUPS);
        add_val(gr, A_NAME, grp[k].n);
        add_val(gr, A_REAL, grp[k].full);
        add_val(gr, A_GID, grp[k].gid);
        guid_of("group", grp[k].n, g);
        add_val(gr, A_GUID, g);
        add_val(gr, A_META, NODE);
        add_val(gr, A_TYPE, REC_GROUPS);
        char list[512];
        snprintf(list, sizeof list, "%s", grp[k].which ? admins : all);
        for (char *s = list, *e; *s; s = e) {
            e = strchr(s, ',');
            if (e) *e++ = 0; else e = s + strlen(s);
            add_val(gr, A_MEMBERS, s);
        }
    }
    *out = r;
    return n;
}
static void free_recs(rec_t *r, int n) {
    for (int i = 0; i < n; i++) for (int k = 0; k < r[i].na; k++) for (int v = 0; v < r[i].a[k].n; v++) free(r[i].a[k].vals[v]);
    free(r);
}
static void free_result(result_t *res) { if (res) { free_recs(res->recs, res->nrecs); free(res); } }

/* ---- buffers e listas */
tDataBuffer *dsDataBufferAllocate(tDirReference d, u32 size) {
    (void)d;
    tDataBuffer *b = calloc(1, sizeof(tDataBuffer) + size);
    if (b) b->fBufferSize = size;
    return b;
}
/* o buffer leva "mOSd" + o ref do resultado; o resultado anterior do mesmo buffer é solto */
static result_t *buf_result(tDataBuffer *b) {
    if (!b || b->fBufferLength < 8 || memcmp(b->fBufferData, "mOSd", 4)) return 0;
    u32 r;
    memcpy(&r, b->fBufferData + 4, 4);
    void *p = ref_get(r, R_RESULT);
    return p == (void *)1 ? 0 : p;
}
static void buf_clear(tDataBuffer *b) {
    if (!b || b->fBufferLength < 8 || memcmp(b->fBufferData, "mOSd", 4)) return;
    u32 r;
    memcpy(&r, b->fBufferData + 4, 4);
    result_t *res = ref_get(r, R_RESULT);
    if (res && res != (void *)1) free_result(res);
    ref_del(r);
    b->fBufferLength = 0;
}
static tDirStatus buf_set(tDataBuffer *b, result_t *res) {
    if (b->fBufferSize < 8) { free_result(res); return eDSBufferTooSmall; }
    buf_clear(b);
    u32 r = ref_new(R_RESULT, res);
    if (!r) { free_result(res); return eDSBufferTooSmall; }
    memcpy(b->fBufferData, "mOSd", 4);
    memcpy(b->fBufferData + 4, &r, 4);
    b->fBufferLength = 8;
    return eDSNoErr;
}
tDirStatus dsDataBufferDeAllocate(tDirReference d, tDataBuffer *b) { (void)d; if (!b) return eDSNullDataBuff; buf_clear(b); free(b); return eDSNoErr; }
tDataNode *dsDataNodeAllocateBlock(tDirReference d, u32 size, u32 len, const void *data) {
    tDataNode *n = dsDataBufferAllocate(d, size < len ? len : size);
    if (n && data && len) { memcpy(n->fBufferData, data, len); n->fBufferLength = len; }
    return n;
}
tDataNode *dsDataNodeAllocateString(tDirReference d, const char *s) {
    if (!s) return 0;
    u32 l = (u32)strlen(s);
    tDataNode *n = dsDataNodeAllocateBlock(d, l + 1, l, s);
    if (n) n->fBufferData[l] = 0;
    return n;
}
tDirStatus dsDataNodeDeAllocate(tDirReference d, tDataNode *n) { (void)d; if (!n) return eDSNullParameter; free(n); return eDSNoErr; }
tDirStatus dsDataNodeSetLength(tDataNode *n, u32 len) { if (!n) return eDSNullParameter; if (len > n->fBufferSize) return eDSBufferTooSmall; n->fBufferLength = len; return eDSNoErr; }
u32 dsDataNodeGetLength(tDataNode *n) { return n ? n->fBufferLength : 0; }
u32 dsDataNodeGetSize(tDataNode *n) { return n ? n->fBufferSize : 0; }
/* a lista: fDataListHead aponta para um bloco privado com o vetor de nós */
typedef struct { u32 cap; tDataNode *nodes[]; } listblk_t;
static listblk_t *lb(const tDataList *l) { return l ? (listblk_t *)l->fDataListHead : 0; }
tDataList *dsDataListAllocate(tDirReference d) { (void)d; return calloc(1, sizeof(tDataList)); }
static tDirStatus list_append(tDataList *l, const char *s, size_t n) {
    listblk_t *b = lb(l);
    if (!b || l->fDataNodeCount == b->cap) {
        u32 cap = b ? b->cap * 2 : 8;
        listblk_t *nb = realloc(b, sizeof *nb + cap * sizeof(tDataNode *));
        if (!nb) return eDSBufferTooSmall;
        nb->cap = cap;
        l->fDataListHead = (tDataNode *)nb;
        b = nb;
    }
    tDataNode *node = dsDataNodeAllocateBlock(0, (u32)n + 1, (u32)n, s);
    if (!node) return eDSBufferTooSmall;
    node->fBufferData[n] = 0;
    b->nodes[l->fDataNodeCount++] = node;
    return eDSNoErr;
}
tDirStatus dsDataListDeallocate(tDirReference d, tDataList *l) {
    (void)d;
    if (!l) return eDSNullParameter;
    listblk_t *b = lb(l);
    for (u32 i = 0; b && i < l->fDataNodeCount; i++) free(b->nodes[i]);
    free(b);
    l->fDataListHead = 0; l->fDataNodeCount = 0;
    return eDSNoErr;
}
tDirStatus dsDataListDeAllocate(tDirReference d, tDataList *l, u8 unused) { (void)unused; return dsDataListDeallocate(d, l); }
u32 dsDataListGetNodeCount(const tDataList *l) { return l ? l->fDataNodeCount : 0; }
tDirStatus dsAppendStringToListAlloc(tDirReference d, tDataList *l, const char *s) { (void)d; if (!l || !s) return eDSNullParameter; return list_append(l, s, strlen(s)); }
tDataList *dsBuildListFromStrings(tDirReference d, const char *first, ...) {
    tDataList *l = dsDataListAllocate(d);
    if (!l) return 0;
    va_list ap;
    va_start(ap, first);
    for (const char *s = first; s; s = va_arg(ap, const char *)) list_append(l, s, strlen(s));
    va_end(ap);
    return l;
}
tDirStatus dsBuildListFromPathAlloc(tDirReference d, tDataList *l, const char *path, const char *sep) {
    (void)d;
    if (!l || !path || !sep || !*sep) return eDSNullParameter;
    size_t sl = strlen(sep);
    const char *p = path;
    while (*p) {
        while (!strncmp(p, sep, sl)) p += sl;
        if (!*p) break;
        const char *e = strstr(p, sep);
        size_t n = e ? (size_t)(e - p) : strlen(p);
        list_append(l, p, n);
        p += n;
    }
    return eDSNoErr;
}
tDataList *dsBuildFromPath(tDirReference d, const char *path, const char *sep) {
    tDataList *l = dsDataListAllocate(d);
    if (l && dsBuildListFromPathAlloc(d, l, path, sep)) { dsDataListDeallocate(d, l); free(l); return 0; }
    return l;
}
char *dsGetPathFromList(tDirReference d, const tDataList *l, const char *sep) {
    (void)d;
    if (!l || !sep) return 0;
    listblk_t *b = lb(l);
    size_t tot = 1;
    for (u32 i = 0; b && i < l->fDataNodeCount; i++) tot += strlen(sep) + b->nodes[i]->fBufferLength;
    char *out = malloc(tot);
    if (!out) return 0;
    out[0] = 0;
    for (u32 i = 0; b && i < l->fDataNodeCount; i++) { strcat(out, sep); strncat(out, b->nodes[i]->fBufferData, b->nodes[i]->fBufferLength); }
    return out;
}
tDirStatus dsDataListGetNodeAlloc(tDirReference d, const tDataList *l, u32 index, tDataNode **out) {
    (void)d;
    if (!l || !out) return eDSNullParameter;
    listblk_t *b = lb(l);
    if (!b || index < 1 || index > l->fDataNodeCount) return eDSIndexOutOfRange;
    tDataNode *s = b->nodes[index - 1];
    *out = dsDataNodeAllocateBlock(d, s->fBufferLength + 1, s->fBufferLength, s->fBufferData);
    if (*out) (*out)->fBufferData[s->fBufferLength] = 0;
    return *out ? eDSNoErr : eDSBufferTooSmall;
}
char **dsAllocStringsFromList(tDirReference d, const tDataList *l) {
    (void)d;
    if (!l) return 0;
    listblk_t *b = lb(l);
    char **v = calloc(l->fDataNodeCount + 1, sizeof *v);
    for (u32 i = 0; v && b && i < l->fDataNodeCount; i++) {
        v[i] = malloc(b->nodes[i]->fBufferLength + 1);
        if (v[i]) { memcpy(v[i], b->nodes[i]->fBufferData, b->nodes[i]->fBufferLength); v[i][b->nodes[i]->fBufferLength] = 0; }
    }
    return v;
}
static const char *list_str(const tDataList *l, u32 i) { listblk_t *b = lb(l); return b && i < l->fDataNodeCount ? b->nodes[i]->fBufferData : 0; }

/* ---- serviço e nós */
tDirStatus dsOpenDirService(tDirReference *out) {
    if (!out) return eDSNullParameter;
    *out = ref_new(R_DIR, 0);
    return *out ? eDSNoErr : eDSInvalidDirRef;
}
tDirStatus dsOpenDirServiceLocal(tDirReference *out, const char *path) { (void)path; return dsOpenDirService(out); }
tDirStatus dsCloseDirService(tDirReference d) { if (!ref_get(d, R_DIR)) return eDSInvalidDirRef; ref_del(d); return eDSNoErr; }
tDirStatus dsVerifyDirRefNum(tDirReference d) { return ref_get(d, R_DIR) ? eDSNoErr : eDSInvalidDirRef; }
tDirStatus dsIsDirServiceRunning(void) { return eDSNoErr; }
tDirStatus dsGetDirNodeCount(tDirReference d, u32 *n) { if (!ref_get(d, R_DIR)) return eDSInvalidDirRef; if (!n) return eDSNullParameter; *n = 1; return eDSNoErr; }
tDirStatus dsReleaseContinueData(tDirReference d, tContextData c) { (void)d; (void)c; return eDSNoErr; }
static tDirStatus nodes_result(tDataBuffer *b, const char *const *names, int n, u32 *count) {
    result_t *res = calloc(1, sizeof *res);
    if (!res) return eDSBufferTooSmall;
    res->is_nodes = 1;
    for (int i = 0; i < n && i < 4; i++) snprintf(res->nodes[res->nnodes++], 64, "%s", names[i]);
    if (count) *count = (u32)res->nnodes;
    return buf_set(b, res);
}
tDirStatus dsGetDirNodeList(tDirReference d, tDataBuffer *b, u32 *count, tContextData *cont) {
    if (!ref_get(d, R_DIR)) return eDSInvalidDirRef;
    if (!b) return eDSNullDataBuff;
    if (cont) *cont = 0;
    static const char *const all[] = {NODE, "/Search", "/Search/Contacts"};
    return nodes_result(b, all, 3, count);
}
tDirStatus dsFindDirNodes(tDirReference d, tDataBuffer *b, tDataList *pattern, tDirPatternMatch how, u32 *count, tContextData *cont) {
    if (!ref_get(d, R_DIR)) return eDSInvalidDirRef;
    if (!b) return eDSNullDataBuff;
    if (cont) *cont = 0;
    static const char *const local[] = {NODE}, *const search[] = {"/Search"}, *const contacts[] = {"/Search/Contacts"};
    switch (how) {
    case eDSLocalNodeNames: case eDSLocalHostedNodes: return nodes_result(b, local, 1, count);
    case eDSSearchNodeName: return nodes_result(b, search, 1, count);
    case eDSContactsSearchNodeName: return nodes_result(b, contacts, 1, count);
    case eDSNetworkSearchNodeName: case eDSDefaultNetworkNodes: case eDSConfigNodeName: return nodes_result(b, local, 0, count);
    }
    if (!pattern) return eDSNullNodeNamePattern;
    char *path = dsGetPathFromList(d, pattern, "/");
    int hit = path && (!strcmp(path, NODE) || ((how == eDSStartsWith || how == eDSiStartsWith) && !strncmp(NODE, path, strlen(path))) ||
                       how == eDSAnyMatch);
    free(path);
    return nodes_result(b, local, hit ? 1 : 0, count);
}
tDirStatus dsGetDirNodeName(tDirReference d, tDataBuffer *b, u32 index, tDataList **out) {
    (void)d;
    if (!b || !out) return eDSNullParameter;
    result_t *res = buf_result(b);
    if (!res || !res->is_nodes) return eDSInvalidBuffFormat;
    if (index < 1 || index > (u32)res->nnodes) return eDSIndexOutOfRange;
    *out = dsBuildFromPath(d, res->nodes[index - 1], "/");
    return *out ? eDSNoErr : eDSBufferTooSmall;
}
typedef struct { int search; } node_t;
tDirStatus dsOpenDirNode(tDirReference d, tDataList *name, tDirNodeReference *out) {
    if (!ref_get(d, R_DIR)) return eDSInvalidDirRef;
    if (!name || !out) return eDSNullNodeName;
    char *path = dsGetPathFromList(d, name, "/");
    int ok = path && (!strcmp(path, NODE) || !strcmp(path, "/Local") || !strcmp(path, "/Search") || !strcmp(path, "/Search/Contacts") ||
                      !strcmp(path, "/Search/Authentication"));
    int search = path && !strncmp(path, "/Search", 7);
    free(path);
    if (!ok) return eDSNodeNotFound;
    node_t *n = calloc(1, sizeof *n);
    n->search = search;
    *out = ref_new(R_NODE, n);
    return *out ? eDSNoErr : eDSInvalidNodeRef;
}
tDirStatus dsCloseDirNode(tDirNodeReference r) {
    node_t *n = ref_get(r, R_NODE);
    if (!n) return eDSInvalidNodeRef;
    if (n != (void *)1) free(n);
    ref_del(r);
    return eDSNoErr;
}

/* ---- busca de registros */
static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int eqn(const char *a, const char *b, size_t n, int ci) {
    for (size_t i = 0; i < n; i++) if (ci ? lower(a[i]) != lower(b[i]) : a[i] != b[i]) return 0;
    return 1;
}
static int str_match(const char *s, const char *p, tDirPatternMatch how) {
    size_t ls = strlen(s), lp = strlen(p);
    int ci = how >= 0x2100 && how < 0x2200;
    switch (how == eDSAnyMatch ? 1 : how & 0x200f) {
    case 1: return 1;
    case 0x2001: return ls == lp && eqn(s, p, ls, ci);
    case 0x2002: return ls >= lp && eqn(s, p, lp, ci);
    case 0x2003: return ls >= lp && eqn(s + ls - lp, p, lp, ci);
    case 0x2004: for (size_t i = 0; i + lp <= ls; i++) if (eqn(s + i, p, lp, ci)) return 1; return 0;
    }
    return 0;
}
static int attr_wanted(const tDataList *types, const char *t) {
    if (!types) return 0;
    for (u32 i = 0; i < types->fDataNodeCount; i++) {
        const char *w = list_str(types, i);
        if (!strcmp(w, A_ALL) || !strcmp(w, A_STDALL) || !strcmp(w, t)) return 1;
    }
    return 0;
}
tDirStatus dsGetRecordList(tDirNodeReference node, tDataBuffer *b, tDataList *names, tDirPatternMatch how, tDataList *types,
                           tDataList *attrs, int info_only, u32 *count, tContextData *cont) {
    if (!ref_get(node, R_NODE)) return eDSInvalidNodeRef;
    if (!b) return eDSNullDataBuff;
    if (!names) return eDSNullRecName;
    if (!types) return eDSNullRecType;
    if (cont && *cont) { if (count) *count = 0; *cont = 0; return eDSNoErr; }   /* tudo já veio na primeira leva */
    u32 max = count ? *count : 0;
    rec_t *all;
    int n = load_all(&all);
    result_t *res = calloc(1, sizeof *res);
    if (!res) { free_recs(all, n); return eDSBufferTooSmall; }
    res->recs = calloc((size_t)n + 1, sizeof(rec_t));
    for (int i = 0; i < n; i++) {
        rec_t *r = &all[i];
        int tok = 0, nok = 0;
        for (u32 k = 0; k < types->fDataNodeCount; k++) {
            const char *t = list_str(types, k);
            tok |= !strcmp(t, r->rtype) || !strcmp(t, "dsRecordsAll") || !strcmp(t, "dsRecordsStandardAll");
        }
        for (u32 k = 0; k < names->fDataNodeCount && !nok; k++) {
            const char *want = list_str(names, k);
            if (!strcmp(want, "dsRecordsAll")) { nok = 1; break; }
            for (int a = 0; a < r->na && !nok; a++)                              /* nome curto ou longo */
                if (!strcmp(r->a[a].type, A_NAME)) for (int v = 0; v < r->a[a].n && !nok; v++) nok = str_match(r->a[a].vals[v], want, how);
        }
        if (!tok || !nok) continue;
        if (max && (u32)res->nrecs >= max) break;
        rec_t *o = &res->recs[res->nrecs++];
        *o = *r;
        o->na = 0;
        for (int a = 0; a < r->na; a++) {                                         /* só os atributos pedidos */
            if (!attr_wanted(attrs, r->a[a].type)) continue;
            attr_t *dst = &o->a[o->na++];
            *dst = r->a[a];
            for (int v = 0; v < dst->n; v++) {                                   /* só informação: a contagem, sem valores */
                if (info_only) { dst->vals[v] = 0; continue; }
                size_t l = strlen(r->a[a].vals[v]) + 1;
                dst->vals[v] = malloc(l);
                if (dst->vals[v]) memcpy(dst->vals[v], r->a[a].vals[v], l);
            }
        }
    }
    free_recs(all, n);
    if (count) *count = (u32)res->nrecs;
    if (cont) *cont = 0;
    if (!res->nrecs) { free_result(res); buf_clear(b); return eDSNoErr; }
    return buf_set(b, res);
}
/* listas de atributos e de valores: guardam o registro (cópia do ponteiro dentro do resultado) */
typedef struct { result_t *res; int rec; int attr; } aref_t;
tDirStatus dsGetRecordEntry(tDirNodeReference node, tDataBuffer *b, u32 index, tAttributeListRef *alr, tRecordEntry **out) {
    if (!ref_get(node, R_NODE)) return eDSInvalidNodeRef;
    if (!b || !alr || !out) return eDSNullParameter;
    result_t *res = buf_result(b);
    if (!res || res->is_nodes) return eDSInvalidBuffFormat;
    if (index < 1 || index > (u32)res->nrecs) return eDSIndexOutOfRange;
    rec_t *r = &res->recs[index - 1];
    u16 nl = (u16)strlen(r->name), tl = (u16)strlen(r->rtype);
    u32 dl = 2u + nl + 2u + tl;
    tRecordEntry *e = calloc(1, sizeof *e + dl);
    if (!e) return eDSBufferTooSmall;
    e->fRecordAttributeCount = (u32)r->na;
    e->fRecordNameAndType.fBufferSize = dl;
    e->fRecordNameAndType.fBufferLength = dl;
    char *p = e->fRecordNameAndType.fBufferData;
    memcpy(p, &nl, 2); memcpy(p + 2, r->name, nl); memcpy(p + 2 + nl, &tl, 2); memcpy(p + 4 + nl, r->rtype, tl);
    aref_t *a = calloc(1, sizeof *a);
    a->res = res; a->rec = (int)index - 1; a->attr = -1;
    *alr = ref_new(R_ATTRLIST, a);
    *out = e;
    return eDSNoErr;
}
tDirStatus dsGetAttributeEntry(tDirNodeReference node, tDataBuffer *b, tAttributeListRef alr, u32 index, tAttributeValueListRef *vlr, tAttributeEntry **out) {
    (void)b;
    if (!ref_get(node, R_NODE)) return eDSInvalidNodeRef;
    aref_t *a = ref_get(alr, R_ATTRLIST);
    if (!a || a == (void *)1) return eDSInvalidAttrListRef;
    if (!vlr || !out) return eDSNullParameter;
    rec_t *r = &a->res->recs[a->rec];
    if (index < 1 || index > (u32)r->na) return eDSIndexOutOfRange;
    attr_t *at = &r->a[index - 1];
    u32 sl = (u32)strlen(at->type), size = 0, maxv = 0;
    for (int v = 0; v < at->n; v++) { u32 l = at->vals[v] ? (u32)strlen(at->vals[v]) : 0; size += l; if (l > maxv) maxv = l; }
    tAttributeEntry *e = calloc(1, sizeof *e + sl + 1);
    if (!e) return eDSBufferTooSmall;
    e->fAttributeValueCount = (u32)at->n;
    e->fAttributeDataSize = size;
    e->fAttributeValueMaxSize = maxv;
    e->fAttributeSignature.fBufferSize = sl + 1;
    e->fAttributeSignature.fBufferLength = sl;
    memcpy(e->fAttributeSignature.fBufferData, at->type, sl + 1);
    aref_t *va = calloc(1, sizeof *va);
    *va = *a; va->attr = (int)index - 1;
    *vlr = ref_new(R_VALLIST, va);
    *out = e;
    return eDSNoErr;
}
tDirStatus dsGetAttributeValue(tDirNodeReference node, tDataBuffer *b, u32 index, tAttributeValueListRef vlr, tAttributeValueEntry **out) {
    (void)b;
    if (!ref_get(node, R_NODE)) return eDSInvalidNodeRef;
    aref_t *a = ref_get(vlr, R_VALLIST);
    if (!a || a == (void *)1) return eDSInvalidAttrValueRef;
    if (!out) return eDSNullParameter;
    attr_t *at = &a->res->recs[a->rec].a[a->attr];
    if (index < 1 || index > (u32)at->n) return eDSIndexOutOfRange;
    const char *v = at->vals[index - 1] ? at->vals[index - 1] : "";
    u32 l = (u32)strlen(v);
    tAttributeValueEntry *e = calloc(1, sizeof *e + l + 1);
    if (!e) return eDSBufferTooSmall;
    e->fAttributeValueID = index;
    e->fAttributeValueData.fBufferSize = l + 1;
    e->fAttributeValueData.fBufferLength = l;
    memcpy(e->fAttributeValueData.fBufferData, v, l + 1);
    *out = e;
    return eDSNoErr;
}
tDirStatus dsCloseAttributeList(tAttributeListRef r) { void *a = ref_get(r, R_ATTRLIST); if (!a) return eDSInvalidAttrListRef; if (a != (void *)1) free(a); ref_del(r); return eDSNoErr; }
tDirStatus dsCloseAttributeValueList(tAttributeValueListRef r) { void *a = ref_get(r, R_VALLIST); if (!a) return eDSInvalidAttrValueRef; if (a != (void *)1) free(a); ref_del(r); return eDSNoErr; }
tDirStatus dsDeallocRecordEntry(tDirReference d, tRecordEntry *e) { (void)d; free(e); return eDSNoErr; }
tDirStatus dsDeallocAttributeEntry(tDirReference d, tAttributeEntry *e) { (void)d; free(e); return eDSNoErr; }
tDirStatus dsDeallocAttributeValueEntry(tDirReference d, tAttributeValueEntry *e) { (void)d; free(e); return eDSNoErr; }

/* ---- autenticação */
tDirStatus dsDoDirNodeAuth(tDirNodeReference node, tDataNode *method, int auth_only, tDataBuffer *in, tDataBuffer *out, tContextData *cont) {
    (void)auth_only;
    if (cont) *cont = 0;
    if (out) out->fBufferLength = 0;
    if (!ref_get(node, R_NODE)) return eDSInvalidNodeRef;
    if (!method || !in) return eDSNullParameter;
    char m[96];
    u32 ml = method->fBufferLength < sizeof m - 1 ? method->fBufferLength : (u32)sizeof m - 1;
    memcpy(m, method->fBufferData, ml);
    m[ml] = 0;
    if (strcmp(m, "dsAuthMethodStandard:dsAuthNodeNativeCanUseClearText") && strcmp(m, "dsAuthMethodStandard:dsAuthNodeNativeCannotUseClearText") &&
        strcmp(m, "dsAuthMethodStandard:dsAuthClearText"))
        return eDSAuthMethodNotSupported;
    u32 len = in->fBufferLength, nl, pl;
    const char *p = in->fBufferData;
    if (len < 4) return eDSAuthInBuffFormatError;
    memcpy(&nl, p, 4);
    if (nl > 255 || 4 + nl + 4 > len) return eDSAuthInBuffFormatError;
    memcpy(&pl, p + 4 + nl, 4);
    if (pl > 1024 || 8 + nl + pl > len) return eDSAuthInBuffFormatError;
    char user[256], pw[1025];
    memcpy(user, p + 4, nl); user[nl] = 0;
    memcpy(pw, p + 8 + nl, pl); pw[pl] = 0;
    /* o nome pode ser o longo: acha o curto */
    char shortn[64] = "";
    long cnt = SYS1(SYS_ACCOUNT, MOS_ACCT_COUNT);
    for (long i = 0; i < cnt && !shortn[0]; i++) {
        mos_account_t a;
        if (SYS3(SYS_ACCOUNT, MOS_ACCT_GET, i, &a) < 0) continue;
        if (!strcmp(a.name, user) || !strcmp(a.full, user)) snprintf(shortn, sizeof shortn, "%s", a.name);
    }
    long r = shortn[0] ? SYS3(SYS_ACCOUNT, MOS_ACCT_VERIFY, shortn, pw) : -1;
    for (u32 i = 0; i < sizeof pw; i++) ((volatile char *)pw)[i] = 0;
    if (!shortn[0]) return eDSAuthUnknownUser;
    return r == 1 ? eDSNoErr : eDSAuthFailed;
}
tDirStatus dsDoDirNodeAuthOnRecordType(tDirNodeReference node, tDataNode *method, int auth_only, tDataBuffer *in, tDataBuffer *out,
                                       tContextData *cont, tDataNode *rtype) {
    if (rtype && strncmp(rtype->fBufferData, REC_USERS, rtype->fBufferLength)) return eDSInvalidRecordType;
    return dsDoDirNodeAuth(node, method, auth_only, in, out, cont);
}
