/* mOS — teste de host da ACL do libSystem (userland/libsystem/acl.c):
 *   sh tools/acl_test.sh */
#include <stdio.h>
#include <stdlib.h>
#include "sys.h"

int cx_errno;
static u8 stored[4096]; static long stored_len = -1;
long host_syscall(long n, long op, long arg) {
    mos_fsmeta_t *rq = (mos_fsmeta_t *)arg;
    if (n != SYS_FSMETA) return -22;
    if (op == MOS_FSM_GETACL) {
        if (stored_len < 0) return -MOS_ENOATTR;
        if (!rq->buf) return stored_len;
        memcpy((void *)rq->buf, stored, (size_t)stored_len);
        return stored_len;
    }
    if (op == MOS_FSM_SETACL) {
        if (!rq->size) { stored_len = -1; return 0; }
        memcpy(stored, (void *)rq->buf, rq->size); stored_len = (long)rq->size; return 0;
    }
    return -22;
}
const char *cx_path(const char *in, char *out, size_t cap) { snprintf(out, cap, "%s", in); return out; }
int cx_follow_path(const char *in, char *out, size_t cap) { snprintf(out, cap, "%s", in); return 0; }
struct pw { char *name; char *pass; unsigned uid, gid; };
static struct pw me = {"visitante", "*", 501, 20}, staff = {"staff", "*", 20, 0};
void *getpwuid(unsigned u) { return u == 501 ? &me : 0; }
void *getpwnam(const char *n) { return !strcmp(n, "visitante") ? &me : 0; }
void *getgrgid(unsigned g) { return g == 20 ? &staff : 0; }
void *getgrnam(const char *n) { return !strcmp(n, "staff") ? &staff : 0; }

void *acl_init(int); int acl_free(void *); int acl_valid(void *); int acl_create_entry(void **, void **);
int acl_set_tag_type(void *, int); int acl_set_qualifier(void *, const void *); int acl_get_permset(void *, void **);
int acl_add_perm(void *, unsigned); int acl_get_flagset_np(void *, void **); int acl_add_flag_np(void *, unsigned);
char *acl_to_text(void *, long *); void *acl_from_text(const char *); int acl_get_entry(void *, int, void **);
int acl_get_tag_type(void *, int *); void *acl_get_qualifier(void *); int acl_get_perm_np(void *, unsigned);
int acl_set_file(const char *, int, void *); void *acl_get_file(const char *, int); long acl_size(void *);
long acl_copy_ext(void *, void *, long); void *acl_copy_int(const void *); int acl_delete_entry(void *, void *);
int mbr_uid_to_uuid(unsigned, u8 *); int mbr_uuid_to_id(const u8 *, unsigned *, int *);
int mbr_check_membership(const u8 *, const u8 *, int *); int mbr_gid_to_uuid(unsigned, u8 *);
int mbr_uuid_to_string(const u8 *, char *); int mbr_string_to_uuid(const char *, u8 *);

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALHA %d: %s\n", __LINE__, #c); } } while (0)

int main(void) {
    u8 g[16], h[16]; unsigned id; int t, mem;
    char s[40];
    mbr_uid_to_uuid(501, g);
    mbr_uuid_to_string(g, s);
    CHECK(!strcmp(s, "FFFFEEEE-DDDD-CCCC-BBBB-AAAA000001F5"));
    CHECK(mbr_string_to_uuid(s, h) == 0 && !memcmp(g, h, 16));
    CHECK(mbr_uuid_to_id(g, &id, &t) == 0 && id == 501 && t == 0);
    mbr_gid_to_uuid(80, h);
    CHECK(mbr_check_membership(g, h, &mem) == 0 && mem == 1);
    mbr_gid_to_uuid(1234, h);
    CHECK(mbr_check_membership(g, h, &mem) == 0 && mem == 0);

    void *a = acl_init(4), *e, *ps, *fs;
    CHECK(a && acl_create_entry(&a, &e) == 0);
    acl_set_tag_type(e, 1); acl_set_qualifier(e, g);
    acl_get_permset(e, &ps); acl_add_perm(ps, 1u << 1); acl_add_perm(ps, 1u << 2);
    acl_get_flagset_np(e, &fs); acl_add_flag_np(fs, 1u << 5);
    CHECK(acl_create_entry(&a, &e) == 0);
    mbr_gid_to_uuid(20, h);
    acl_set_tag_type(e, 2); acl_set_qualifier(e, h);
    acl_get_permset(e, &ps); acl_add_perm(ps, 1u << 4);
    CHECK(acl_valid(a) == 0);
    long n;
    char *txt = acl_to_text(a, &n);
    CHECK(txt && !strcmp(txt, "!#acl 1\nuser:FFFFEEEE-DDDD-CCCC-BBBB-AAAA000001F5:visitante:501:allow,file_inherit:read,write\n"
                              "group:ABCDEFAB-CDEF-ABCD-EFAB-CDEF00000014:staff:20:deny:delete\n"));
    CHECK(n == (long)strlen(txt));
    void *b = acl_from_text(txt);
    char *txt2 = acl_to_text(b, 0);
    CHECK(txt2 && !strcmp(txt, txt2));
    void *c = acl_from_text("user:visitante:allow:list,search\ngroup:80:deny,directory_inherit:add_subdirectory\n");
    CHECK(c != 0);
    CHECK(acl_get_entry(c, 0, &e) == 0 && acl_get_tag_type(e, &t) == 0 && t == 1);
    acl_get_permset(e, &ps);
    CHECK(acl_get_perm_np(ps, 1u << 1) == 1 && acl_get_perm_np(ps, 1u << 3) == 1 && acl_get_perm_np(ps, 1u << 2) == 0);
    u8 *q = acl_get_qualifier(e);
    CHECK(q && !memcmp(q, g, 16));
    acl_free(q);
    CHECK(acl_get_entry(c, -1, &e) == 0 && acl_get_tag_type(e, &t) == 0 && t == 2);
    CHECK(acl_get_entry(c, -1, &e) == -1);
    CHECK(acl_from_text("user:ninguem:allow:read") == 0);
    CHECK(acl_from_text("user:501:talvez:read") == 0);
    /* gravar/ler pelo kernel (falso) */
    CHECK(acl_get_file("/x", 0x100) == 0 && cx_errno == 2);
    CHECK(acl_set_file("/x", 0x100, a) == 0 && stored_len == 44 + 48);
    void *r = acl_get_file("/x", 0x100);
    char *txt3 = acl_to_text(r, 0);
    CHECK(txt3 && !strcmp(txt3, txt));
    /* externo */
    u8 ext[512];
    CHECK(acl_size(a) == 92 && acl_copy_ext(ext, a, sizeof ext) == 92);
    void *ia = acl_copy_int(ext);
    char *txt4 = acl_to_text(ia, 0);
    CHECK(txt4 && !strcmp(txt4, txt));
    acl_get_entry(a, 0, &e);
    CHECK(acl_delete_entry(a, e) == 0 && acl_size(a) == 68);
    void *empty = acl_init(0);
    CHECK(acl_set_file("/x", 0x100, empty) == 0 && stored_len == -1);   /* vazia tira a ACL */
    free(txt); free(txt2); free(txt3); free(txt4);
    acl_free(a); acl_free(b); acl_free(c); acl_free(r); acl_free(ia); acl_free(empty);
    printf("acl: %d verificações, %d falha(s)\n", checks, fails);
    return fails != 0;
}
