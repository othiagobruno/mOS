/*
 * mOS libSystem — ACL do Darwin (sys/acl.h) e membership (membership.h) sobre o kernel.
 *
 * A ACL de um arquivo é a kauth_filesec do xattr protegido com.apple.system.Security, lida e gravada pelo
 * SYS_FSMETA (MOS_FSM_GETACL/SETACL) e avaliada pelo kernel em todo acesso (kernel/lib/vfsperm.c), com herança.
 * Aqui fica a representação em memória (acl_t com até ACL_MAX_ENTRIES entradas), o texto do acl_to_text do macOS
 *   !#acl 1
 *   user:FFFFEEEE-DDDD-CCCC-BBBB-AAAA000001F5:visitante:501:allow,file_inherit:read,write
 * e os GUIDs de usuário/grupo (os mesmos do kernel).
 */
#include "sys.h"
#include "libc.h"

#define SET(e) (cx_errno = (e))
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
int  cx_follow_path(const char *in, char *out, size_t cap);   /* libc.c */
void *getpwuid(unsigned uid);
void *getpwnam(const char *n);
void *getgrgid(unsigned g);
void *getgrnam(const char *n);

#define ACL_MAX_ENTRIES 128
#define ACL_MAGIC 0xAC1A0001u
#define ENT_MAGIC 0xAC1E0001u
#define PSET_MAGIC 0xAC1B0001u
#define ACL_TYPE_EXTENDED 0x100
#define ACL_TYPE_DEFAULT  0x1
#define ACL_UNDEFINED_TAG 0
#define ACL_EXTENDED_ALLOW 1
#define ACL_EXTENDED_DENY 2
#define ACL_FIRST_ENTRY 0
#define ACL_NEXT_ENTRY (-1)
#define ACL_LAST_ENTRY (-2)
#define ACL_FLAG_DEFER_INHERIT (1u << 0)
#define ACL_FLAG_NO_INHERIT    (1u << 17)
#define ACE_FLAGS_MASK 0x1f0u           /* inherited, file/dir/limit/only inherit */

typedef struct { u8 b[16]; } guid_t_;
typedef struct acl_ent { u32 magic; int tag; guid_t_ q; u32 perms, flags; struct acl_s *owner; } acl_ent_t;
typedef struct acl_s { u32 magic; u32 flags; int n, cursor; acl_ent_t e[ACL_MAX_ENTRIES]; } acl_s_t;

static u32 g32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static void s32(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }

/* ======================================================================== membership */
static const u8 user_pfx[12] = {0xFF, 0xFF, 0xEE, 0xEE, 0xDD, 0xDD, 0xCC, 0xCC, 0xBB, 0xBB, 0xAA, 0xAA};
static const u8 group_pfx[12] = {0xAB, 0xCD, 0xEF, 0xAB, 0xCD, 0xEF, 0xAB, 0xCD, 0xEF, 0xAB, 0xCD, 0xEF};
int mbr_uid_to_uuid(unsigned uid, u8 *uu) { memcpy(uu, user_pfx, 12); s32(uu + 12, uid); return 0; }
int mbr_gid_to_uuid(unsigned gid, u8 *uu) { memcpy(uu, group_pfx, 12); s32(uu + 12, gid); return 0; }
/* type: ID_TYPE_UID 0, ID_TYPE_GID 1 */
int mbr_uuid_to_id(const u8 *uu, unsigned *id, int *type) {
    if (!memcmp(uu, user_pfx, 12)) { *id = g32(uu + 12); *type = 0; return 0; }
    if (!memcmp(uu, group_pfx, 12)) { *id = g32(uu + 12); *type = 1; return 0; }
    return 2;   /* ENOENT */
}
static const unsigned admin_groups[] = {20, 12, 61, 79, 80, 81, 98, 33, 100, 204, 250, 395, 398, 399, 400, 701};
static int in_groups(unsigned uid, unsigned gid) {
    if (uid == 0 || gid == 12) return 1;
    if (uid == 501) for (unsigned i = 0; i < sizeof admin_groups / sizeof *admin_groups; i++) if (admin_groups[i] == gid) return 1;
    return 0;
}
int mbr_check_membership(const u8 *user, const u8 *group, int *ismember) {
    unsigned u, g; int tu, tg;
    if (mbr_uuid_to_id(user, &u, &tu) || mbr_uuid_to_id(group, &g, &tg) || tu != 0) return 2;
    *ismember = tg == 1 ? in_groups(u, g) : (tg == 0 && u == g);
    return 0;
}
int mbr_check_membership_refresh(const u8 *user, const u8 *group, int *ismember) { return mbr_check_membership(user, group, ismember); }
int mbr_check_membership_by_id(const u8 *user, unsigned gid, int *ismember) {
    u8 g[16]; mbr_gid_to_uuid(gid, g); return mbr_check_membership(user, g, ismember);
}
int mbr_user_name_to_uuid(const char *name, u8 *uu) {
    void *pw = getpwnam(name);
    if (!pw) return 2;
    return mbr_uid_to_uuid(*(unsigned *)((char *)pw + 16), uu);   /* struct passwd: pw_name, pw_passwd, pw_uid */
}
int mbr_group_name_to_uuid(const char *name, u8 *uu) {
    void *gr = getgrnam(name);
    if (!gr) return 2;
    return mbr_gid_to_uuid(*(unsigned *)((char *)gr + 16), uu);   /* struct group: gr_name, gr_passwd, gr_gid */
}
int mbr_uuid_to_string(const u8 *uu, char *s) {
    static const char hx[] = "0123456789ABCDEF";
    int o = 0;
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) s[o++] = '-';
        s[o++] = hx[uu[i] >> 4]; s[o++] = hx[uu[i] & 15];
    }
    s[o] = 0;
    return 0;
}
static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
int mbr_string_to_uuid(const char *s, u8 *uu) {
    int k = 0;
    for (const char *p = s; *p && k < 32; p++) {
        if (*p == '-') continue;
        int v = hexv(*p);
        if (v < 0) return 22;
        if (k & 1) uu[k / 2] |= (u8)v; else uu[k / 2] = (u8)(v << 4);
        k++;
    }
    return k == 32 ? 0 : 22;
}

/* ======================================================================== acl_t */
static acl_s_t *A(void *p) { acl_s_t *a = p; return a && a->magic == ACL_MAGIC ? a : 0; }
static acl_ent_t *E(void *p) { acl_ent_t *e = p; return e && e->magic == ENT_MAGIC ? e : 0; }

void *acl_init(int n) {
    if (n < 0 || n > ACL_MAX_ENTRIES) { SET(22); return 0; }
    acl_s_t *a = calloc(1, sizeof *a);
    if (!a) { SET(12); return 0; }
    a->magic = ACL_MAGIC;
    return a;
}
void *acl_dup(void *p) {
    acl_s_t *a = A(p);
    if (!a) { SET(22); return 0; }
    acl_s_t *b = malloc(sizeof *b);
    if (!b) { SET(12); return 0; }
    *b = *a;
    for (int i = 0; i < b->n; i++) b->e[i].owner = b;
    return b;
}
int acl_free(void *p) {
    if (!p) return 0;
    acl_s_t *a = A(p);
    if (a) { a->magic = 0; free(a); return 0; }
    free(p);   /* texto do acl_to_text ou qualificador do acl_get_qualifier */
    return 0;
}
int acl_valid(void *p) {
    acl_s_t *a = A(p);
    if (!a || a->n < 0 || a->n > ACL_MAX_ENTRIES) { SET(22); return -1; }
    for (int i = 0; i < a->n; i++) {
        unsigned id; int t;
        if (a->e[i].tag != ACL_EXTENDED_ALLOW && a->e[i].tag != ACL_EXTENDED_DENY) { SET(22); return -1; }
        if (mbr_uuid_to_id(a->e[i].q.b, &id, &t)) { SET(22); return -1; }
    }
    return 0;
}
int acl_create_entry_np(void **acl, void **ent, int idx) {
    acl_s_t *a = acl ? A(*acl) : 0;
    if (!a) { SET(22); return -1; }
    if (a->n >= ACL_MAX_ENTRIES) { SET(12); return -1; }
    if (idx == ACL_LAST_ENTRY || idx > a->n || idx < 0) idx = a->n;
    for (int i = a->n; i > idx; i--) a->e[i] = a->e[i - 1];
    memset(&a->e[idx], 0, sizeof a->e[idx]);
    a->e[idx].magic = ENT_MAGIC;
    a->e[idx].owner = a;
    a->n++;
    if (ent) *ent = &a->e[idx];
    return 0;
}
int acl_create_entry(void **acl, void **ent) { return acl_create_entry_np(acl, ent, ACL_LAST_ENTRY); }
int acl_delete_entry(void *acl, void *ent) {
    acl_s_t *a = A(acl);
    acl_ent_t *e = E(ent);
    if (!a || !e || e < a->e || e >= a->e + a->n) { SET(22); return -1; }
    int i = (int)(e - a->e);
    for (; i < a->n - 1; i++) a->e[i] = a->e[i + 1];
    a->n--;
    memset(&a->e[a->n], 0, sizeof a->e[a->n]);
    return 0;
}
int acl_get_entry(void *acl, int id, void **ent) {
    acl_s_t *a = A(acl);
    if (ent) *ent = 0;
    if (!a) { SET(22); return -1; }
    if (id == ACL_FIRST_ENTRY) a->cursor = 0;
    else if (id == ACL_LAST_ENTRY) a->cursor = a->n - 1;
    else if (id != ACL_NEXT_ENTRY) { if (id < 0 || id >= a->n) { SET(22); return -1; } a->cursor = id; }
    if (a->cursor < 0 || a->cursor >= a->n) { SET(22); return -1; }
    if (ent) *ent = &a->e[a->cursor];
    a->cursor++;
    return 0;
}
int acl_copy_entry(void *dst, void *src) {
    acl_ent_t *d = E(dst), *s = E(src);
    if (!d || !s) { SET(22); return -1; }
    acl_s_t *o = d->owner;
    *d = *s;
    d->owner = o;
    return 0;
}
int acl_get_tag_type(void *ent, int *tag) { acl_ent_t *e = E(ent); if (!e || !tag) { SET(22); return -1; } *tag = e->tag; return 0; }
int acl_set_tag_type(void *ent, int tag) {
    acl_ent_t *e = E(ent);
    if (!e || (tag != ACL_EXTENDED_ALLOW && tag != ACL_EXTENDED_DENY && tag != ACL_UNDEFINED_TAG)) { SET(22); return -1; }
    e->tag = tag;
    return 0;
}
void *acl_get_qualifier(void *ent) {
    acl_ent_t *e = E(ent);
    if (!e) { SET(22); return 0; }
    u8 *g = malloc(16);
    if (!g) { SET(12); return 0; }
    memcpy(g, e->q.b, 16);
    return g;
}
int acl_set_qualifier(void *ent, const void *q) {
    acl_ent_t *e = E(ent);
    if (!e || !q) { SET(22); return -1; }
    memcpy(e->q.b, q, 16);
    return 0;
}
/* permset e flagset: apontam para a própria entrada (como no Darwin, onde são o campo da entrada) */
int acl_get_permset(void *ent, void **ps) { acl_ent_t *e = E(ent); if (!e || !ps) { SET(22); return -1; } *ps = &e->perms; return 0; }
int acl_set_permset(void *ent, void *ps) { acl_ent_t *e = E(ent); if (!e || !ps) { SET(22); return -1; } e->perms = *(u32 *)ps; return 0; }
int acl_add_perm(void *ps, unsigned perm) { if (!ps) { SET(22); return -1; } *(u32 *)ps |= perm; return 0; }
int acl_delete_perm(void *ps, unsigned perm) { if (!ps) { SET(22); return -1; } *(u32 *)ps &= ~perm; return 0; }
int acl_clear_perms(void *ps) { if (!ps) { SET(22); return -1; } *(u32 *)ps = 0; return 0; }
int acl_get_perm_np(void *ps, unsigned perm) { if (!ps) { SET(22); return -1; } return (*(u32 *)ps & perm) != 0; }
int acl_get_permset_mask_np(void *ent, u64 *m) { acl_ent_t *e = E(ent); if (!e || !m) { SET(22); return -1; } *m = e->perms; return 0; }
int acl_set_permset_mask_np(void *ent, u64 m) { acl_ent_t *e = E(ent); if (!e) { SET(22); return -1; } e->perms = (u32)m; return 0; }
int acl_maximal_permset_mask_np(u64 *m) { if (!m) { SET(22); return -1; } *m = 0x3ffe; return 0; }
/* flagset de uma ACL (DEFER/NO_INHERIT) ou de uma entrada (herança) */
int acl_get_flagset_np(void *obj, void **fs) {
    if (!fs) { SET(22); return -1; }
    acl_s_t *a = A(obj);
    if (a) { *fs = &a->flags; return 0; }
    acl_ent_t *e = E(obj);
    if (e) { *fs = &e->flags; return 0; }
    SET(22); return -1;
}
int acl_set_flagset_np(void *obj, void *fs) {
    if (!fs) { SET(22); return -1; }
    acl_s_t *a = A(obj);
    if (a) { a->flags = *(u32 *)fs; return 0; }
    acl_ent_t *e = E(obj);
    if (e) { e->flags = *(u32 *)fs; return 0; }
    SET(22); return -1;
}
int acl_add_flag_np(void *fs, unsigned f) { if (!fs) { SET(22); return -1; } *(u32 *)fs |= f; return 0; }
int acl_delete_flag_np(void *fs, unsigned f) { if (!fs) { SET(22); return -1; } *(u32 *)fs &= ~f; return 0; }
int acl_clear_flags_np(void *fs) { if (!fs) { SET(22); return -1; } *(u32 *)fs = 0; return 0; }
int acl_get_flag_np(void *fs, unsigned f) { if (!fs) { SET(22); return -1; } return (*(u32 *)fs & f) != 0; }

/* ======================================================================== filesec <-> acl_t */
static u8 *to_fsec(acl_s_t *a, u64 *len) {
    u64 n = 44 + 24 * (u64)a->n;
    u8 *d = calloc(1, n);
    if (!d) return 0;
    s32(d, 0x012cc16du);
    s32(d + 36, (u32)a->n);
    s32(d + 40, a->flags);
    for (int i = 0; i < a->n; i++) {
        u8 *e = d + 44 + 24 * i;
        memcpy(e, a->e[i].q.b, 16);
        s32(e + 16, (u32)a->e[i].tag | (a->e[i].flags & ACE_FLAGS_MASK));
        s32(e + 20, a->e[i].perms);
    }
    *len = n;
    return d;
}
static void *from_fsec(const u8 *d, u64 n) {
    acl_s_t *a = acl_init(0);
    if (!a) return 0;
    if (n < 44) return a;
    u32 cnt = g32(d + 36);
    if (cnt == 0xffffffffu) return a;
    a->flags = g32(d + 40);
    for (u32 i = 0; i < cnt && i < ACL_MAX_ENTRIES && 44 + 24 * (u64)(i + 1) <= n; i++) {
        const u8 *e = d + 44 + 24 * i;
        acl_ent_t *x = &a->e[a->n++];
        x->magic = ENT_MAGIC;
        x->owner = a;
        memcpy(x->q.b, e, 16);
        u32 fl = g32(e + 16);
        x->tag = (int)(fl & 0xf);
        x->flags = fl & ACE_FLAGS_MASK;
        x->perms = g32(e + 20);
    }
    return a;
}

static void *get_acl(int fd, const char *abs) {
    mos_fsmeta_t rq = {0};
    rq.fd = abs ? -1 : fd;
    rq.path = (unsigned long long)abs;
    long need = SYS2(SYS_FSMETA, MOS_FSM_GETACL, &rq);
    if (need < 0) { SET((int)(need == -MOS_ENOATTR ? 2 : -need)); return 0; }   /* sem ACL: ENOENT */
    u8 *b = malloc((size_t)need);
    if (!b) { SET(12); return 0; }
    rq.buf = (unsigned long long)b;
    rq.size = (u64)need;
    long r = SYS2(SYS_FSMETA, MOS_FSM_GETACL, &rq);
    void *a = r < 0 ? 0 : from_fsec(b, (u64)r);
    if (r < 0) SET((int)-r);
    free(b);
    return a;
}
static int set_acl(int fd, const char *abs, void *p) {
    acl_s_t *a = A(p);
    if (!a) { SET(22); return -1; }
    u64 len = 0;
    u8 *d = to_fsec(a, &len);
    if (!d) { SET(12); return -1; }
    mos_fsmeta_t rq = {0};
    rq.fd = abs ? -1 : fd;
    rq.path = (unsigned long long)abs;
    rq.buf = (unsigned long long)d;
    rq.size = a->n ? len : 0;      /* ACL vazia = tirar a ACL */
    long r = SYS2(SYS_FSMETA, MOS_FSM_SETACL, &rq);
    free(d);
    if (r < 0) { SET((int)-r); return -1; }
    return 0;
}

void *acl_get_file(const char *p, int t) {
    char b[1024];
    if (t == ACL_TYPE_DEFAULT) { SET(2); return 0; }
    if (cx_follow_path(p, b, sizeof b) < 0) return 0;
    return get_acl(-1, b);
}
void *acl_get_link_np(const char *p, int t) {
    char b[1024];
    if (t == ACL_TYPE_DEFAULT) { SET(2); return 0; }
    cx_path(p, b, sizeof b);
    return get_acl(-1, b);
}
void *acl_get_fd(int fd) { return get_acl(fd, 0); }
void *acl_get_fd_np(int fd, int t) { if (t == ACL_TYPE_DEFAULT) { SET(2); return 0; } return get_acl(fd, 0); }
int acl_set_file(const char *p, int t, void *a) {
    char b[1024];
    if (t == ACL_TYPE_DEFAULT) { SET(45); return -1; }
    if (cx_follow_path(p, b, sizeof b) < 0) return -1;
    return set_acl(-1, b, a);
}
int acl_set_link_np(const char *p, int t, void *a) {
    char b[1024];
    if (t == ACL_TYPE_DEFAULT) { SET(45); return -1; }
    cx_path(p, b, sizeof b);
    return set_acl(-1, b, a);
}
int acl_set_fd(int fd, void *a) { return set_acl(fd, 0, a); }
int acl_set_fd_np(int fd, void *a, int t) { if (t == ACL_TYPE_DEFAULT) { SET(45); return -1; } return set_acl(fd, 0, a); }
int acl_delete_def_file(const char *p) { (void)p; SET(45); return -1; }   /* o Darwin não tem ACL default */
int acl_valid_fd_np(int fd, int t, void *a) { (void)fd; (void)t; return acl_valid(a); }
int acl_valid_file_np(const char *p, int t, void *a) { (void)p; (void)t; return acl_valid(a); }
int acl_valid_link_np(const char *p, int t, void *a) { (void)p; (void)t; return acl_valid(a); }

/* representação externa: a própria filesec */
long acl_size(void *p) { acl_s_t *a = A(p); if (!a) { SET(22); return -1; } return 44 + 24 * (long)a->n; }
long acl_copy_ext(void *buf, void *p, long size) {
    acl_s_t *a = A(p);
    if (!a || !buf) { SET(22); return -1; }
    u64 len = 0;
    u8 *d = to_fsec(a, &len);
    if (!d) { SET(12); return -1; }
    if ((long)len > size) { free(d); SET(34); return -1; }
    memcpy(buf, d, len);
    free(d);
    return (long)len;
}
long acl_copy_ext_native(void *buf, void *p, long size) { return acl_copy_ext(buf, p, size); }
void *acl_copy_int(const void *buf) {
    const u8 *d = buf;
    if (!d || g32(d) != 0x012cc16du) { SET(22); return 0; }
    u32 cnt = g32(d + 36);
    return from_fsec(d, 44 + (cnt == 0xffffffffu ? 0 : 24 * (u64)cnt));
}
void *acl_copy_int_native(const void *buf) { return acl_copy_int(buf); }

/* ======================================================================== texto */
static const struct { u32 bit; const char *name; } perm_names[] = {
    {1u << 1, "read"}, {1u << 2, "write"}, {1u << 3, "execute"}, {1u << 4, "delete"}, {1u << 5, "append"},
    {1u << 6, "delete_child"}, {1u << 7, "readattr"}, {1u << 8, "writeattr"}, {1u << 9, "readextattr"},
    {1u << 10, "writeextattr"}, {1u << 11, "readsecurity"}, {1u << 12, "writesecurity"}, {1u << 13, "chown"},
};
static const struct { u32 bit; const char *name; } perm_alias[] = {   /* nomes de pasta aceitos na leitura */
    {1u << 1, "list"}, {1u << 2, "add_file"}, {1u << 3, "search"}, {1u << 5, "add_subdirectory"},
};
static const struct { u32 bit; const char *name; } flag_names[] = {
    {1u << 4, "inherited"}, {1u << 5, "file_inherit"}, {1u << 6, "directory_inherit"},
    {1u << 7, "limit_inherit"}, {1u << 8, "only_inherit"},
};

typedef struct { char *b; size_t n, cap; int err; } sb_t;
static void sb_add(sb_t *s, const char *t) {
    size_t k = strlen(t);
    if (s->err) return;
    if (s->n + k + 1 > s->cap) {
        size_t nc = (s->cap ? s->cap * 2 : 256) + k;
        char *nb = malloc(nc);
        if (!nb) { s->err = 1; return; }
        if (s->n) memcpy(nb, s->b, s->n);
        free(s->b);
        s->b = nb;
        s->cap = nc;
    }
    memcpy(s->b + s->n, t, k + 1);
    s->n += k;
}
static void utoa_(unsigned v, char *o) { char t[12]; int k = 0; do t[k++] = (char)('0' + v % 10); while (v /= 10); for (int i = 0; i < k; i++) o[i] = t[k - 1 - i]; o[k] = 0; }

char *acl_to_text(void *p, long *len) {
    acl_s_t *a = A(p);
    if (!a) { SET(22); return 0; }
    sb_t s = {0};
    sb_add(&s, "!#acl 1\n");
    for (int i = 0; i < a->n; i++) {
        acl_ent_t *e = &a->e[i];
        unsigned id = 0; int t = 0;
        char us[40], num[12];
        int known = mbr_uuid_to_id(e->q.b, &id, &t) == 0;
        mbr_uuid_to_string(e->q.b, us);
        sb_add(&s, known && t == 1 ? "group:" : "user:");
        sb_add(&s, us);
        sb_add(&s, ":");
        if (known) {
            void *ent = t == 1 ? getgrgid(id) : getpwuid(id);
            const char *nm = ent ? *(const char **)ent : 0;
            if (nm) sb_add(&s, nm);
            sb_add(&s, ":");
            utoa_(id, num);
            sb_add(&s, num);
        } else sb_add(&s, ":");
        sb_add(&s, e->tag == ACL_EXTENDED_DENY ? ":deny" : ":allow");
        for (unsigned k = 0; k < sizeof flag_names / sizeof *flag_names; k++)
            if (e->flags & flag_names[k].bit) { sb_add(&s, ","); sb_add(&s, flag_names[k].name); }
        sb_add(&s, ":");
        int first = 1;
        for (unsigned k = 0; k < sizeof perm_names / sizeof *perm_names; k++)
            if (e->perms & perm_names[k].bit) { if (!first) sb_add(&s, ","); sb_add(&s, perm_names[k].name); first = 0; }
        sb_add(&s, "\n");
    }
    if (s.err) { free(s.b); SET(12); return 0; }
    if (len) *len = (long)s.n;
    return s.b;
}

static int tok_eq(const char *a, size_t n, const char *b) { return strlen(b) == n && !memcmp(a, b, n); }
static int is_digits(const char *a, size_t n) { if (!n) return 0; for (size_t i = 0; i < n; i++) if (a[i] < '0' || a[i] > '9') return 0; return 1; }

/* uma linha "tag:[uuid:][nome:][id:]allow|deny[,flags]:perms" (o acl_to_text, ou só "user:nome:allow:read") */
static int parse_line(acl_s_t *a, const char *l, size_t n) {
    const char *tk[8]; size_t tl[8]; int nt = 0;
    const char *s = l;
    for (size_t i = 0; i <= n && nt < 8; i++) {
        if (i == n || l[i] == ':') { tk[nt] = s; tl[nt] = (size_t)(l + i - s); nt++; s = l + i + 1; }
    }
    if (nt < 3) return -1;
    int group;
    if (tok_eq(tk[0], tl[0], "user")) group = 0;
    else if (tok_eq(tk[0], tl[0], "group")) group = 1;
    else return -1;
    int ki = -1;
    for (int i = 1; i < nt; i++)
        if ((tl[i] >= 5 && !memcmp(tk[i], "allow", 5)) || (tl[i] >= 4 && !memcmp(tk[i], "deny", 4))) { ki = i; }
    if (ki < 2 || ki != nt - 2) return -1;
    u8 q[16];
    int have = 0;
    for (int i = 1; i < ki && !have; i++) {           /* o 1º qualificador que resolve: uuid, id, nome */
        char tmp[128];
        if (!tl[i] || tl[i] >= sizeof tmp) continue;
        memcpy(tmp, tk[i], tl[i]); tmp[tl[i]] = 0;
        if (tl[i] == 36 && mbr_string_to_uuid(tmp, q) == 0) { have = 1; break; }
    }
    for (int i = 1; i < ki && !have; i++)
        if (is_digits(tk[i], tl[i])) {
            unsigned v = 0;
            for (size_t k = 0; k < tl[i]; k++) v = v * 10 + (unsigned)(tk[i][k] - '0');
            group ? mbr_gid_to_uuid(v, q) : mbr_uid_to_uuid(v, q);
            have = 1;
        }
    for (int i = 1; i < ki && !have; i++) {
        char tmp[128];
        if (!tl[i] || tl[i] >= sizeof tmp) continue;
        memcpy(tmp, tk[i], tl[i]); tmp[tl[i]] = 0;
        if ((group ? mbr_group_name_to_uuid(tmp, q) : mbr_user_name_to_uuid(tmp, q)) == 0) have = 1;
    }
    if (!have) return -1;
    void *ent;
    void *ap = a;
    if (acl_create_entry(&ap, &ent) < 0) return -1;
    acl_ent_t *e = ent;
    memcpy(e->q.b, q, 16);
    /* allow|deny[,flag...] */
    const char *k = tk[ki]; size_t kl = tl[ki], o = 0;
    while (o <= kl) {
        size_t st = o;
        while (o < kl && k[o] != ',') o++;
        const char *w = k + st; size_t wl = o - st;
        if (tok_eq(w, wl, "allow")) e->tag = ACL_EXTENDED_ALLOW;
        else if (tok_eq(w, wl, "deny")) e->tag = ACL_EXTENDED_DENY;
        else {
            int ok = 0;
            for (unsigned f = 0; f < sizeof flag_names / sizeof *flag_names; f++)
                if (tok_eq(w, wl, flag_names[f].name)) { e->flags |= flag_names[f].bit; ok = 1; }
            if (!ok) return -1;
        }
        o++;
    }
    const char *pm = tk[ki + 1]; size_t pl = tl[ki + 1];
    o = 0;
    while (o < pl) {
        size_t st = o;
        while (o < pl && pm[o] != ',') o++;
        const char *w = pm + st; size_t wl = o - st;
        int ok = 0;
        for (unsigned f = 0; f < sizeof perm_names / sizeof *perm_names; f++) if (tok_eq(w, wl, perm_names[f].name)) { e->perms |= perm_names[f].bit; ok = 1; }
        for (unsigned f = 0; f < sizeof perm_alias / sizeof *perm_alias; f++) if (tok_eq(w, wl, perm_alias[f].name)) { e->perms |= perm_alias[f].bit; ok = 1; }
        if (!ok && wl) return -1;
        o++;
    }
    return 0;
}

void *acl_from_text(const char *t) {
    if (!t) { SET(22); return 0; }
    acl_s_t *a = acl_init(0);
    if (!a) return 0;
    const char *p = t;
    while (*p) {
        const char *e = p;
        while (*e && *e != '\n') e++;
        size_t n = (size_t)(e - p);
        while (n && (p[n - 1] == ' ' || p[n - 1] == '\r')) n--;
        if (n && !(n >= 2 && p[0] == '!' && p[1] == '#') && p[0] != '#') {
            if (parse_line(a, p, n) < 0) { acl_free(a); SET(22); return 0; }
        }
        p = *e ? e + 1 : e;
    }
    return a;
}
