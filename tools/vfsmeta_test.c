/*
 * mOS — teste de host dos metadados do VFS (kernel/lib/vfs.c + kernel/lib/vfsmeta.c, código real do kernel):
 * inode estável, xattr com a semântica do macOS, flags BSD, chown, datas em ns, SYS_FSMETA (vfs_fsmeta) e a ida e
 * volta do \mOS\.vfsmeta (persistência no disco do sistema).
 *
 *   gcc -O1 -g -DMOS_HOST_TEST -Itools/vfsmeta_host -Ikernel/include -Itools tools/vfsmeta_test.c \
 *       kernel/lib/vfs.c kernel/lib/vfsmeta.c kernel/proc/cred.c -o /tmp/vfsmeta_test && /tmp/vfsmeta_test
 */
#include "vfs_host_kernel.h"
#include "vfs.h"
#include "syscall.h"

long long g_live_allocs, g_live_bytes;
static u64 fake_ms = 1000;
u64 timer_ticks(void) { return fake_ms; }
void rtc_now(datetime_t *dt) { *dt = (datetime_t){2026, 11, 15, 12, 0, 0, 0}; }
uint32_t rand32(void) { return 4; }
static boot_info_t boot0;
boot_info_t *g_boot = &boot0;

/* ganchos que o kernel liga em fs/persist.c e fs/vfs_bridge.c */
static int meta_marks;
bool persist_load(void) { return false; }
void persist_enable(void) {}
void persist_mark(vnode_t *n) { (void)n; }
void persist_delete(vnode_t *n) { (void)n; }
void persist_move(const char *o, vnode_t *n) { (void)o; (void)n; }
bool persist_available(void) { return false; }
void persist_meta_mark(void) { meta_marks++; }
void vfs_init(void);
extern bool vfs_meta_live;

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALHA %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static i64 op(vnode_t *v, int o, mos_fsmeta_t *rq, const char *name, void *buf, int euid) {
    return vfs_fsmeta(v, o, rq, name, buf, euid, 20);
}

static bool track_all(void *ctx, vnode_t *v, char *vp, usize cap) {
    (void)ctx;
    vfs_path(v, vp, cap);
    return strncmp(vp, "/tmp", 4) != 0;
}
static vnode_t *lookup(void *ctx, const char *p) { (void)ctx; return vfs_lookup(p); }

int main(void) {
    vfs_init();
    CHECK(vfs_meta_live);
    vnode_t *a = vfs_write_file("/Users/Visitante/Documentos/a.txt", "ola", 3);
    CHECK(a && a->ino >= 16 && a->uid == 501 && a->gid == 20);
    CHECK(a->meta);
    i64 t0 = a->mtime_ns;
    CHECK(t0 > 1700000000ll * 1000000000ll);           /* 2026, não 1970 */

    /* ---- xattr */
    mos_fsmeta_t rq = {0};
    char buf[256];
    rq.size = 5;
    CHECK(vfs_xattr_set(a, "user.teste", "valor", 5, 0, 0) == 0);
    CHECK(vfs_xattr_get(a, "user.teste", NULL, 0, 0) == 5);
    CHECK(vfs_xattr_get(a, "user.teste", buf, 4, 0) == -34);          /* ERANGE */
    CHECK(vfs_xattr_get(a, "user.teste", buf, 5, 0) == 5 && !memcmp(buf, "valor", 5));
    CHECK(vfs_xattr_get(a, "nao.existe", buf, 5, 0) == -93);          /* ENOATTR */
    CHECK(vfs_xattr_set(a, "user.teste", "x", 1, 0, VFS_XATTR_CREATE) == -17);   /* EEXIST */
    CHECK(vfs_xattr_set(a, "novo", "x", 1, 0, VFS_XATTR_REPLACE) == -93);
    CHECK(vfs_xattr_set(a, "user.teste", "abc", 3, 0, VFS_XATTR_REPLACE) == 0);
    CHECK(vfs_xattr_get(a, "user.teste", buf, 100, 0) == 3);
    CHECK(vfs_xattr_set(a, "", "x", 1, 0, 0) == -22);
    char longo[200]; memset(longo, 'n', 199); longo[199] = 0;
    CHECK(vfs_xattr_set(a, longo, "x", 1, 0, 0) == -63);              /* ENAMETOOLONG */
    CHECK(vfs_xattr_set(a, "user.teste", "x", 1, 3, 0) == -22);       /* position só no resource fork */
    CHECK(vfs_xattr_set(a, "com.apple.FinderInfo", "curto", 5, 0, 0) == -34);
    u8 fi[32] = {0};
    CHECK(vfs_xattr_set(a, "com.apple.FinderInfo", fi, 32, 0, 0) == 0);
    CHECK(vfs_xattr_get(a, "com.apple.FinderInfo", buf, 32, 0) == -93);   /* só zeros = ausente */
    fi[8] = 0x40;                                                     /* kIsInvisible */
    CHECK(vfs_xattr_set(a, "com.apple.FinderInfo", fi, 32, 0, 0) == 0);
    CHECK(a->flags & VFS_UF_HIDDEN);
    /* resource fork com position: escrita em pedaços */
    CHECK(vfs_xattr_set(a, "com.apple.ResourceFork", "AAAA", 4, 0, 0) == 0);
    CHECK(vfs_xattr_set(a, "com.apple.ResourceFork", "BB", 2, 6, 0) == 0);
    CHECK(vfs_xattr_get(a, "com.apple.ResourceFork", buf, 100, 0) == 8 && !memcmp(buf, "AAAA\0\0BB", 8));
    CHECK(vfs_xattr_get(a, "com.apple.ResourceFork", buf, 3, 5) == 3 && !memcmp(buf, "\0BB", 3));
    /* list: ordenada, com o tamanho pedido por cap 0 */
    i64 need = vfs_xattr_list(a, NULL, 0);
    const char want[] = "com.apple.FinderInfo\0com.apple.ResourceFork\0user.teste\0";
    CHECK(need == (i64)sizeof want - 1);
    CHECK(vfs_xattr_list(a, buf, sizeof buf) == need && !memcmp(buf, want, (usize)need));
    CHECK(vfs_xattr_list(a, buf, 10) == -34);
    CHECK(vfs_xattr_remove(a, "com.apple.ResourceFork") == 0);
    CHECK(vfs_xattr_remove(a, "com.apple.ResourceFork") == -93);
    /* quarentena (formato do LaunchServices) */
    const char *q = "0083;6560f000;Safari;";
    CHECK(vfs_xattr_set(a, "com.apple.quarantine", q, strlen(q), 0, 0) == 0);

    /* ---- SYS_FSMETA pelo núcleo: permissões */
    rq = (mos_fsmeta_t){0};
    CHECK(op(a, MOS_FSM_SETXATTR, &rq, "com.apple.system.Security", "x", 501) == -1);  /* EPERM sem root */
    rq.a = VFS_SF_IMMUTABLE;
    CHECK(op(a, MOS_FSM_CHFLAGS, &rq, NULL, NULL, 501) == -1);                       /* SF_* sem root */
    rq.a = VFS_UF_IMMUTABLE | VFS_UF_HIDDEN;
    CHECK(op(a, MOS_FSM_CHFLAGS, &rq, NULL, NULL, 502) == -1);                       /* não é o dono */
    CHECK(op(a, MOS_FSM_CHFLAGS, &rq, NULL, NULL, 501) == 0 && vfs_immutable(a));
    rq.size = 1;
    CHECK(op(a, MOS_FSM_SETXATTR, &rq, "user.y", "y", 501) == -1);                   /* imutável */
    CHECK(vfs_no_unlink(a));
    rq.a = 0;
    CHECK(op(a, MOS_FSM_CHFLAGS, &rq, NULL, NULL, 501) == 0 && !vfs_immutable(a));
    rq.a = VFS_SF_ARCHIVED;
    CHECK(op(a, MOS_FSM_CHFLAGS, &rq, NULL, NULL, 0) == 0 && a->flags == VFS_SF_ARCHIVED);
    rq.a = 0;
    CHECK(op(a, MOS_FSM_CHFLAGS, &rq, NULL, NULL, 501) == -1);                       /* tirar SF_ também é EPERM */
    CHECK(op(a, MOS_FSM_CHFLAGS, &rq, NULL, NULL, 0) == 0);
    /* chown */
    rq.a = 0; rq.b = 0xffffffffu;
    CHECK(op(a, MOS_FSM_CHOWN, &rq, NULL, NULL, 501) == -1);                         /* dar o arquivo a outro */
    rq.a = 0xffffffffu; rq.b = 80;
    CHECK(op(a, MOS_FSM_CHOWN, &rq, NULL, NULL, 501) == 0 && a->gid == 80);          /* grupo admin */
    rq.b = 1234;
    CHECK(op(a, MOS_FSM_CHOWN, &rq, NULL, NULL, 501) == -1);
    a->mode = 0x10000u | 04755;
    rq.a = 0; rq.b = 0;
    CHECK(op(a, MOS_FSM_CHOWN, &rq, NULL, NULL, 0) == 0 && a->uid == 0 && a->gid == 0);
    CHECK((a->mode & 04000) == 04000);                                               /* root mantém setuid */
    rq.a = 501; rq.b = 20;
    CHECK(op(a, MOS_FSM_CHOWN, &rq, NULL, NULL, 0) == 0);
    a->mode = 0x10000u | 0644;

    /* ---- datas */
    fake_ms += 50;
    rq = (mos_fsmeta_t){0};
    rq.a = VT_ATIME | VT_MTIME; rq.b = 0;
    rq.t[0] = 1000000000ll * 1000000000ll; rq.t[1] = 1100000000ll * 1000000000ll + 123;
    i64 c_before = a->ctime_ns;
    CHECK(op(a, MOS_FSM_SETTIMES, &rq, NULL, NULL, 501) == 0);
    CHECK(a->atime_ns == rq.t[0] && a->mtime_ns == rq.t[1]);
    CHECK(a->ctime_ns > c_before);
    CHECK(a->btime_ns <= a->mtime_ns);                       /* criação não fica depois da modificação */
    CHECK(op(a, MOS_FSM_SETTIMES, &rq, NULL, NULL, 777) == -1);
    rq.a = VT_BTIME; rq.t[3] = 900000000ll * 1000000000ll;
    CHECK(op(a, MOS_FSM_SETTIMES, &rq, NULL, NULL, 501) == 0 && a->btime_ns == rq.t[3]);
    i64 m1 = a->mtime_ns;
    vfs_append(a, "!", 1);
    CHECK(a->mtime_ns > m1);
    i64 m2 = a->mtime_ns;
    vfs_append(a, "!", 1);                                   /* mesmo ms: ainda cresce */
    CHECK(a->mtime_ns > m2);

    /* ---- statx */
    mos_statx_t st;
    rq.size = sizeof st;
    CHECK(op(a, MOS_FSM_STATX, &rq, NULL, &st, 501) == 0);
    CHECK(st.ino == a->ino && st.size == 5 && st.uid == 501 && st.gid == 20 && st.nlink == 1);
    CHECK(st.mode == (0644 | 0x10000u) && st.nxattr == 3 && st.dev == 0x1000002u);
    vnode_t *docs = vfs_lookup("/Users/Visitante/Documentos");
    vfs_mkdir("/Users/Visitante/Documentos/sub1");
    vfs_mkdir("/Users/Visitante/Documentos/sub2");
    CHECK(op(docs, MOS_FSM_STATX, &rq, NULL, &st, 501) == 0 && st.kind == 1 && st.nlink == 4);

    /* ---- rename mantém o inode; pasta-mãe muda mtime */
    u64 ino = a->ino;
    vnode_t *sub1 = vfs_lookup("/Users/Visitante/Documentos/sub1");
    i64 sm = sub1->mtime_ns;
    fake_ms += 10;
    CHECK(vfs_move(a, sub1, "b.txt"));
    CHECK(vfs_lookup("/Users/Visitante/Documentos/sub1/b.txt") == a && a->ino == ino);
    CHECK(sub1->mtime_ns > sm);

    /* ---- readdir com inode e tipo */
    vfs_write_file("/Users/Visitante/Documentos/sub1/link", "#mOS-link:b.txt", 15);
    mos_fsm_dirent_t de;
    rq = (mos_fsmeta_t){0};
    rq.size = sizeof de;
    int seen_link = 0, seen_file = 0;
    for (u64 i = 0;; i++) {
        rq.pos = i;
        i64 r = op(sub1, MOS_FSM_READDIR, &rq, NULL, &de, 501);
        if (r != 1) { CHECK(r == 0); break; }
        if (!strcmp(de.name, "b.txt")) { seen_file = 1; CHECK(de.ino == ino && de.type == 8); }
        if (!strcmp(de.name, "link")) { seen_link = 1; CHECK(de.type == 10); }
    }
    CHECK(seen_file && seen_link);

    /* ---- .vfsmeta: grava, apaga a árvore, recria como o boot faria e restaura */
    vfs_mkdir("/tmp/naoentra");
    u64 len = 0;
    u8 *img = vmeta_build(vfs_root(), track_all, NULL, &len);
    CHECK(img && len > 24);
    u64 next = 0;
    CHECK(vmeta_check(img, len, &next) && next > ino);
    u8 *bad = kmalloc(len);
    memcpy(bad, img, len);
    bad[len / 2] ^= 1;
    CHECK(!vmeta_check(bad, len, NULL));                     /* CRC pega o byte trocado */
    kfree(bad);
    i64 at = a->atime_ns, mt = a->mtime_ns, bt = a->btime_ns;
    u32 fl = a->flags;
    vfs_delete(vfs_lookup("/Users/Visitante/Documentos/sub1/b.txt"));
    vfs_meta_live = false;                                   /* "boot": a carga não marca metadados */
    vnode_t *b = vfs_write_file("/Users/Visitante/Documentos/sub1/b.txt", "ola!!", 5);
    CHECK(b && !b->meta && b->xattr == NULL && b->ino != ino);
    int n = vmeta_apply(img, len, lookup, NULL);
    vfs_meta_live = true;
    CHECK(n > 0);
    CHECK(b->ino == ino && b->uid == 501 && b->flags == fl);
    CHECK(b->atime_ns == at && b->mtime_ns == mt && b->btime_ns == bt);
    CHECK(vfs_xattr_get(b, "com.apple.quarantine", buf, sizeof buf, 0) == (i64)strlen(q) && !memcmp(buf, q, strlen(q)));
    CHECK(vfs_xattr_get(b, "user.teste", buf, sizeof buf, 0) == 3 && !memcmp(buf, "abc", 3));
    vnode_t *c = vfs_write_file("/Users/Visitante/novo.txt", "", 0);
    CHECK(c->ino >= next);                                   /* inodes novos não colidem com os gravados */
    kfree(img);

    /* ---- apagar solta os xattrs (sem vazamento) */
    long long before = g_live_allocs;
    vnode_t *z = vfs_write_file("/Users/Visitante/z", "z", 1);
    vfs_xattr_set(z, "k1", "v", 1, 0, 0);
    vfs_xattr_set(z, "k2", "v", 1, 0, 0);
    vfs_delete(z);
    CHECK(g_live_allocs == before);
    CHECK(meta_marks > 0);

    printf("vfsmeta: %d verificações, %d falha(s)\n", checks, fails);
    return fails != 0;
}
