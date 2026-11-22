/*
 * mOS libSystem — Identity Services (CSIdentity*, OSServices) sobre as contas do sistema (SYS_ACCOUNT,
 * kernel/lib/account.c): consulta de usuários e grupos, nome POSIX, nome completo, ID, pertencer a grupo e conferir
 * senha (PBKDF2 no kernel, com trava depois de 5 erros). No mOS toda conta roda com o mesmo uid (VFS_DEFAULT_UID, 501),
 * então é esse o ID POSIX de qualquer usuário; os grupos são staff (20, todos), admin (80, contas de administrador) e
 * everyone (12). Uma autoridade só (local = padrão); não há diretório de rede.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
id CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(id a, id v);
long CFArrayGetCount(id a);
id CFArrayGetValueAtIndex(id a, long i);
id CFRetain(id o);
extern const char kCFTypeArrayCallBacks[];
unsigned getuid(void);

enum { kCSIdentityClassUser = 1, kCSIdentityClassGroup = 2 };
enum { kCSIdentityQueryStringEquals = 1, kCSIdentityQueryStringBeginsWith = 2 };

#define ST(T, o) (*(T **)((u8 *)(o) + 8))
static cx_class_t Auth_c, Auth_m, Query_c, Query_m, Ident_c, Ident_m;
typedef struct { int cls; char name[64]; char full[96]; unsigned id; int admin; } ident_t;
typedef struct { int kind; int cls; char name[64]; int method; unsigned posix; id results; } query_t;   /* kind: 0 todos, 1 nome, 2 atual, 3 ID */
static id g_auth;
static void boot(void) {
    static int done;
    if (done) return;
    done = 1;
    cx_class_t *ns = cx_class_named("NSObject");
    cx_define_class(&Auth_c, &Auth_m, "__CSIdentityAuthority", ns, 8);
    cx_define_class(&Query_c, &Query_m, "__CSIdentityQuery", ns, 8);
    cx_define_class(&Ident_c, &Ident_m, "__CSIdentity", ns, 8);
}
id CSGetLocalIdentityAuthority(void) { boot(); if (!g_auth) g_auth = objc_retain(cx_alloc_instance(&Auth_c)); return g_auth; }
id CSGetDefaultIdentityAuthority(void) { return CSGetLocalIdentityAuthority(); }
id CSGetManagedIdentityAuthority(void) { return CSGetLocalIdentityAuthority(); }
id CSIdentityAuthorityCopyLocalizedName(id a) { (void)a; return objc_retain(cx_str("Contas deste Mac")); }

static id ident_new(int cls, const char *name, const char *full, unsigned pid, int admin) {
    id o = cx_alloc_instance(&Ident_c);
    ident_t *t = calloc(1, sizeof *t);
    t->cls = cls; t->id = pid; t->admin = admin;
    snprintf(t->name, sizeof t->name, "%s", name);
    snprintf(t->full, sizeof t->full, "%s", full);
    ST(ident_t, o) = t;
    return o;
}
static ident_t *ident_of(id o) { return o && object_getClass(o) == &Ident_c ? ST(ident_t, o) : 0; }
/* todas as identidades do sistema: as contas e os 3 grupos */
static int all_identities(id out, int cls) {
    int n = 0;
    if (cls == kCSIdentityClassUser || !cls) {
        long cnt = SYS1(SYS_ACCOUNT, MOS_ACCT_COUNT);
        for (long i = 0; i < cnt; i++) {
            mos_account_t a;
            if (SYS3(SYS_ACCOUNT, MOS_ACCT_GET, i, &a) < 0) continue;
            id o = ident_new(kCSIdentityClassUser, a.name, a.full[0] ? a.full : a.name, getuid(), (int)a.admin);
            CFArrayAppendValue(out, o); objc_release(o); n++;
        }
    }
    if (cls == kCSIdentityClassGroup || !cls) {
        static const struct { const char *n, *f; unsigned id; } g[] = {{"staff", "Staff", 20}, {"admin", "Administradores", 80}, {"everyone", "Todos", 12}};
        for (int i = 0; i < 3; i++) { id o = ident_new(kCSIdentityClassGroup, g[i].n, g[i].f, g[i].id, 0); CFArrayAppendValue(out, o); objc_release(o); n++; }
    }
    return n;
}
static id query_new(int kind, int cls, const char *name, int method, unsigned posix) {
    boot();
    id o = cx_alloc_instance(&Query_c);
    query_t *q = calloc(1, sizeof *q);
    q->kind = kind; q->cls = cls; q->method = method; q->posix = posix;
    if (name) snprintf(q->name, sizeof q->name, "%s", name);
    ST(query_t, o) = q;
    return o;
}
id CSIdentityQueryCreate(void *alloc, int cls, id authority) { (void)alloc; (void)authority; return query_new(0, cls, 0, 0, 0); }
id CSIdentityQueryCreateForName(void *alloc, id name, int method, int cls, id authority) {
    (void)alloc; (void)authority;
    return query_new(1, cls, name && cx_is_str(name) ? cx_cstr(name) : "", method, 0);
}
id CSIdentityQueryCreateForCurrentUser(void *alloc) { (void)alloc; return query_new(2, kCSIdentityClassUser, 0, 0, 0); }
id CSIdentityQueryCreateForPosixID(void *alloc, unsigned pid, int cls, id authority) { (void)alloc; (void)authority; return query_new(3, cls, 0, 0, pid); }
static int prefix_ci(const char *s, const char *p) { for (; *p; s++, p++) { char a = *s, b = *p; if (a >= 'A' && a <= 'Z') a += 32; if (b >= 'A' && b <= 'Z') b += 32; if (a != b) return 0; } return 1; }
static int eq_ci(const char *a, const char *b) { return prefix_ci(a, b) && strlen(a) == strlen(b); }
u8 CSIdentityQueryExecute(id query, u32 flags, id *error) {
    (void)flags;
    if (error) *error = 0;
    query_t *q = query && object_getClass(query) == &Query_c ? ST(query_t, query) : 0;
    if (!q) return 0;
    if (q->results) objc_release(q->results);
    q->results = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    id all = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    all_identities(all, q->kind == 2 ? kCSIdentityClassUser : q->cls);
    mos_account_t cur;
    int has_cur = q->kind == 2 && SYS3(SYS_ACCOUNT, MOS_ACCT_CURRENT, 0, &cur) >= 0;
    for (long i = 0; i < CFArrayGetCount(all); i++) {
        id o = CFArrayGetValueAtIndex(all, i);
        ident_t *t = ident_of(o);
        int take = 0;
        switch (q->kind) {
        case 0: take = 1; break;
        case 1: take = q->method == kCSIdentityQueryStringBeginsWith ? (prefix_ci(t->name, q->name) || prefix_ci(t->full, q->name))
                                                                     : (eq_ci(t->name, q->name) || eq_ci(t->full, q->name)); break;
        case 2: take = has_cur && !strcmp(t->name, cur.name); break;
        case 3: take = t->id == q->posix; break;
        }
        if (take) CFArrayAppendValue(q->results, o);
    }
    objc_release(all);
    return 1;
}
id CSIdentityQueryCopyResults(id query) {
    query_t *q = query && object_getClass(query) == &Query_c ? ST(query_t, query) : 0;
    return q && q->results ? CFRetain(q->results) : CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
}
void CSIdentityQueryStop(id query) { (void)query; }
/* "Get": o objeto continua dono (como na Apple) */
id CSIdentityGetPosixName(id ident) { ident_t *t = ident_of(ident); return t ? objc_autorelease(objc_retain(cx_str(t->name))) : 0; }
id CSIdentityGetFullName(id ident) { ident_t *t = ident_of(ident); return t ? objc_autorelease(objc_retain(cx_str(t->full))) : 0; }
unsigned CSIdentityGetPosixID(id ident) { ident_t *t = ident_of(ident); return t ? t->id : (unsigned)-1; }
int CSIdentityGetClass(id ident) { ident_t *t = ident_of(ident); return t ? t->cls : 0; }
id CSIdentityGetAuthority(id ident) { (void)ident; return CSGetLocalIdentityAuthority(); }
u8 CSIdentityIsEnabled(id ident) { return ident_of(ident) != 0; }
/* UUID estável por conta: versão 5 (nome) sobre a classe e o nome POSIX; o mOS não guarda GeneratedUID */
id CFUUIDCreateFromString(void *alloc, id s);
int snprintf(char *b, size_t n, const char *f, ...);
id CSIdentityGetUUID(id ident) {
    ident_t *t = ident_of(ident);
    if (!t) return 0;
    u8 b[16];
    u32 h = 2166136261u;
    for (int r = 0; r < 16; r++) {
        for (const char *p = t->name; *p; p++) h = (h ^ (u8)*p) * 16777619u;
        h = (h ^ (u32)(r + 31 * t->cls)) * 16777619u;
        b[r] = (u8)(h >> 8);
    }
    b[6] = (u8)((b[6] & 0x0f) | 0x50);
    b[8] = (u8)((b[8] & 0x3f) | 0x80);
    char s[40];
    snprintf(s, sizeof s, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X", b[0], b[1], b[2], b[3], b[4], b[5], b[6],
             b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
    id u = CFUUIDCreateFromString(0, cx_str(s));
    return u ? objc_autorelease(u) : 0;                /* "Get": o original não dá +1 */
}
u8 CSIdentityIsHidden(id ident) { (void)ident; return 0; }
u8 CSIdentityIsMemberOfGroup(id ident, id group) {
    ident_t *u = ident_of(ident), *g = ident_of(group);
    if (!u || !g || g->cls != kCSIdentityClassGroup) return 0;
    if (!strcmp(g->name, "staff") || !strcmp(g->name, "everyone")) return u->cls == kCSIdentityClassUser;
    if (!strcmp(g->name, "admin")) return u->cls == kCSIdentityClassUser && u->admin;
    return 0;
}
/* senha conferida pelo kernel; trava de 30 s depois de 5 erros = falso */
u8 CSIdentityAuthenticateUsingPassword(id user, id password) {
    ident_t *u = ident_of(user);
    if (!u || u->cls != kCSIdentityClassUser) return 0;
    const char *pw = password && cx_is_str(password) ? cx_cstr(password) : "";
    return SYS3(SYS_ACCOUNT, MOS_ACCT_VERIFY, u->name, pw) == 1;
}
