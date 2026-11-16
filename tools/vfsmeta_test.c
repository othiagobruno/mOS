/*
 * mOS — teste de host dos metadados do VFS (kernel/lib/vfs.c + kernel/lib/vfsmeta.c, código real do kernel):
 * inode estável, xattr com a semântica do macOS, flags BSD, chown, datas em ns, SYS_FSMETA (vfs_fsmeta) e a ida e
 * volta do \mOS\.vfsmeta (persistência no disco do sistema).
 *
 *   gcc -O1 -g -DMOS_HOST_TEST -Itools/vfsmeta_host -Ikernel/include -Itools tools/vfsmeta_test.c \
 *       kernel/lib/vfs.c kernel/lib/vfsmeta.c kernel/lib/vfsperm.c kernel/lib/vfsevents.c kernel/proc/cred.c -o /tmp/vfsmeta_test && /tmp/vfsmeta_test
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


/* kauth_filesec big-endian com n ACEs (guid, flags, direitos) */
static u64 mkfsec(u8 *out, int n, const u32 *kind_id, const u32 *flags, const u32 *rights) {
    memset(out, 0, 44 + 24 * n);
    out[0] = 0x01; out[1] = 0x2c; out[2] = 0xc1; out[3] = 0x6d;
    out[36] = 0; out[37] = 0; out[38] = 0; out[39] = (u8)n;
    for (int i = 0; i < n; i++) {
        u8 *a = out + 44 + 24 * i;
        if (kind_id[2 * i] == 1) vfs_guid_user(a, kind_id[2 * i + 1]); else vfs_guid_group(a, kind_id[2 * i + 1]);
        a[16] = (u8)(flags[i] >> 24); a[17] = (u8)(flags[i] >> 16); a[18] = (u8)(flags[i] >> 8); a[19] = (u8)flags[i];
        a[20] = (u8)(rights[i] >> 24); a[21] = (u8)(rights[i] >> 16); a[22] = (u8)(rights[i] >> 8); a[23] = (u8)rights[i];
    }
    return 44 + 24 * (u64)n;
}

static void test_perms(void) {
    vnode_t *f = vfs_write_file("/Users/Visitante/p.txt", "x", 1);
    f->mode = 0x10000u | 0600;
    CHECK(vfs_access(f, 4, 501, 20) == 0 && vfs_access(f, 2, 501, 20) == 0);
    CHECK(vfs_access(f, 4, 502, 20) == -13);                     /* EACCES */
    CHECK(vfs_access(f, 4 | 2, 0, 0) == 0);                       /* root */
    CHECK(vfs_access(f, 1, 0, 0) == -13);                         /* root não executa sem bit x */
    f->mode = 0x10000u | 0640;
    CHECK(vfs_access(f, 4, 502, 20) == 0 && vfs_access(f, 2, 502, 20) == -13);
    CHECK(vfs_access(f, 4, 502, 99) == -13);                      /* fora do grupo staff */
    /* ACL: nega escrita ao dono, permite leitura a 502 fora do grupo */
    u8 fs[44 + 24 * 4];
    u32 ki[] = {1, 501, 1, 502}, fl[] = {VFS_ACE_DENY, VFS_ACE_PERMIT}, rt[] = {VFS_KR_WRITE_DATA, VFS_KR_READ_DATA};
    u64 n = mkfsec(fs, 2, ki, fl, rt);
    mos_fsmeta_t rq = {0};
    rq.size = n;
    CHECK(vfs_fsmeta(f, MOS_FSM_SETACL, &rq, NULL, fs, 502, 20) == -1);   /* EPERM: não é o dono */
    CHECK(vfs_fsmeta(f, MOS_FSM_SETACL, &rq, NULL, fs, 501, 20) == 0);
    CHECK(vfs_access(f, 2, 501, 20) == -13 && vfs_access(f, 4, 501, 20) == 0);
    CHECK(vfs_access(f, 4, 502, 99) == 0);
    /* escondida do listxattr/getxattr; GETACL devolve o filesec */
    rq = (mos_fsmeta_t){0};
    CHECK(vfs_fsmeta(f, MOS_FSM_LISTXATTR, &rq, NULL, NULL, 501, 20) == 0);
    char b[512];
    rq.size = sizeof b;
    CHECK(vfs_fsmeta(f, MOS_FSM_GETXATTR, &rq, VFS_XATTR_SECURITY, b, 501, 20) == -93);
    CHECK(vfs_fsmeta(f, MOS_FSM_GETACL, &rq, NULL, b, 501, 20) == (i64)n && !memcmp(b, fs, n));
    rq.size = 3;
    CHECK(vfs_fsmeta(f, MOS_FSM_SETACL, &rq, NULL, fs, 501, 20) == -22);   /* filesec inválido */
    rq.size = 1;
    CHECK(vfs_fsmeta(f, MOS_FSM_SETXATTR, &rq, VFS_XATTR_SECURITY, "x", 501, 20) == -1);
    rq.size = 0;
    CHECK(vfs_fsmeta(f, MOS_FSM_SETACL, &rq, NULL, NULL, 501, 20) == 0);    /* tira */
    CHECK(vfs_access(f, 2, 501, 20) == 0);
    /* ACL dá WRITE_SECURITY a 502: pode trocar a ACL e o modo */
    u32 ki2[] = {1, 502}, fl2[] = {VFS_ACE_PERMIT}, rt2[] = {VFS_KR_WRITE_SECURITY | VFS_KR_WRITE_ATTRIBUTES};
    n = mkfsec(fs, 1, ki2, fl2, rt2);
    rq.size = n;
    CHECK(vfs_fsmeta(f, MOS_FSM_SETACL, &rq, NULL, fs, 501, 20) == 0);
    CHECK(vfs_authorize(f, VFS_KR_WRITE_SECURITY, 502, 99) == 0 && vfs_authorize(f, VFS_KR_WRITE_SECURITY, 503, 99) == -1);
    /* utimes(NULL) por quem escreve; datas explícitas só dono/WRITE_ATTRIBUTES */
    f->mode = 0x10000u | 0666;
    rq = (mos_fsmeta_t){0};
    rq.a = VT_ATIME | VT_MTIME; rq.b = VT_ATIME | VT_MTIME;
    CHECK(vfs_fsmeta(f, MOS_FSM_SETTIMES, &rq, NULL, NULL, 503, 99) == 0);
    rq.b = 0;
    CHECK(vfs_fsmeta(f, MOS_FSM_SETTIMES, &rq, NULL, NULL, 503, 99) == -1);
    CHECK(vfs_fsmeta(f, MOS_FSM_SETTIMES, &rq, NULL, NULL, 502, 99) == 0);  /* pela ACL */
    /* ACCESS */
    rq = (mos_fsmeta_t){0};
    f->mode = 0x10000u | 0400;
    rq.a = 2;
    CHECK(vfs_fsmeta(f, MOS_FSM_ACCESS, &rq, NULL, NULL, 501, 20) == -13);
    rq.a = 4;
    CHECK(vfs_fsmeta(f, MOS_FSM_ACCESS, &rq, NULL, NULL, 501, 20) == 0);

    /* sticky em /tmp: só o dono apaga */
    vnode_t *tmp = vfs_lookup("/tmp");
    CHECK(tmp && (tmp->mode & 01777) == 01777 && tmp->uid == 0);
    vnode_t *t = vfs_write_file("/tmp/de502", "y", 1);
    t->uid = 502;
    CHECK(vfs_may_delete(t, 501, 20) == -1);                    /* EPERM */
    CHECK(vfs_may_delete(t, 502, 20) == 0 && vfs_may_delete(t, 0, 0) == 0);
    CHECK(vfs_may_create(tmp, false, 503, 99) == 0);
    /* pasta 0755 de outro: não cria nem apaga dentro */
    vnode_t *d = vfs_mkdir("/Users/Visitante/alheia");
    d->uid = 502; d->mode = 0x10000u | 0755;
    vnode_t *g = vfs_write_file("/Users/Visitante/alheia/g", "", 0);
    CHECK(vfs_may_create(d, false, 501, 20) == -13 && vfs_may_delete(g, 501, 20) == -13);
    /* ACL DELETE no arquivo vale mesmo sem escrever na pasta */
    u32 ki3[] = {1, 501}, fl3[] = {VFS_ACE_PERMIT}, rt3[] = {VFS_KR_DELETE};
    n = mkfsec(fs, 1, ki3, fl3, rt3);
    vfs_xattr_set(g, VFS_XATTR_SECURITY, fs, n, 0, 0);
    CHECK(vfs_may_delete(g, 501, 20) == 0);
    /* ACL DELETE_CHILD na pasta */
    u32 rt4[] = {VFS_KR_DELETE_CHILD | VFS_KR_EXECUTE | VFS_KR_WRITE_DATA};
    n = mkfsec(fs, 1, ki3, fl3, rt4);
    vfs_xattr_set(d, VFS_XATTR_SECURITY, fs, n, 0, 0);
    CHECK(vfs_may_create(d, false, 501, 20) == 0);
    vfs_xattr_remove(d, VFS_XATTR_SECURITY);

    /* herança: arquivo pega FILE_INHERIT; subpasta guarda as duas e marca ONLY_INHERIT na que só vale p/ arquivo */
    u32 ki5[] = {1, 503, 2, 80}, fl5[] = {VFS_ACE_PERMIT | VFS_ACE_FILE_INHERIT, VFS_ACE_PERMIT | VFS_ACE_DIRECTORY_INHERIT};
    u32 rt5[] = {VFS_KR_READ_DATA, VFS_KR_EXECUTE};
    n = mkfsec(fs, 2, ki5, fl5, rt5);
    vfs_xattr_set(d, VFS_XATTR_SECURITY, fs, n, 0, 0);
    vnode_t *h = vfs_write_file("/Users/Visitante/alheia/herdado", "z", 1);
    vxattr_t *hx = vfs_xattr_find(h, VFS_XATTR_SECURITY);
    CHECK(hx && hx->len == 44 + 24);
    h->mode = 0x10000u | 0600;
    CHECK(vfs_access(h, 4, 503, 99) == 0);
    CHECK(hx && (hx->val[44 + 19] & VFS_ACE_INHERITED));
    vnode_t *sd = vfs_mkdir("/Users/Visitante/alheia/sub");
    vxattr_t *sx = vfs_xattr_find(sd, VFS_XATTR_SECURITY);
    CHECK(sx && sx->len == 44 + 48);
    sd->mode = 0x10000u | 0700;
    CHECK(vfs_access(sd, 4, 503, 99) == -13);                  /* a ACE de arquivo virou só-herança na pasta */
    vnode_t *neto = vfs_write_file("/Users/Visitante/alheia/sub/neto", "n", 1);
    neto->mode = 0x10000u | 0600;
    CHECK(vfs_access(neto, 4, 503, 99) == 0);                  /* e continua descendo */
}

static int evs(u64 since, mos_fsevent_t *ev, int max) { return vfs_ev_read(since, ev, max, NULL); }
static void test_events(void) {
    static mos_fsevent_t ev[64];
    u64 last = 0;
    vfs_ev_read(~0ull, NULL, 0, &last);
    u64 base = last;
    vnode_t *f = vfs_write_file("/Users/Visitante/ev.txt", "a", 1);
    vfs_append(f, "b", 1);
    int n = evs(base, ev, 64);
    CHECK(n == 1);                                             /* criar + escrever juntam num só */
    CHECK(n >= 1 && !strcmp(ev[0].path, "/Users/Visitante/ev.txt") && (ev[0].flags & FSEV_CREATED) &&
          (ev[0].flags & FSEV_MODIFIED) && (ev[0].flags & FSEV_IS_FILE) && ev[0].ino == f->ino);
    u64 seen = ev[0].id;
    vfs_append(f, "c", 1);                                     /* já lido: não junta mais */
    n = evs(seen, ev, 64);
    CHECK(n == 1 && ev[0].flags == (FSEV_MODIFIED | FSEV_IS_FILE) && ev[0].id == seen + 1);
    seen = ev[0].id;
    vnode_t *docs = vfs_lookup("/Users/Visitante/Documentos");
    CHECK(vfs_move(f, docs, "ev2.txt"));
    n = evs(seen, ev, 64);
    CHECK(n == 2 && (ev[0].flags & FSEV_RENAMED) && (ev[1].flags & FSEV_RENAMED));
    CHECK(n == 2 && !strcmp(ev[0].path, "/Users/Visitante/ev.txt") && !strcmp(ev[1].path, "/Users/Visitante/Documentos/ev2.txt"));
    CHECK(n == 2 && ev[0].ino == ev[1].ino);
    seen = ev[n - 1].id;
    vfs_xattr_set(f, "user.x", "1", 1, 0, 0);
    mos_fsmeta_t rq = {0};
    rq.a = 0xffffffffu; rq.b = 80;
    vfs_fsmeta(f, MOS_FSM_CHOWN, &rq, NULL, NULL, 501, 20);
    n = evs(seen, ev, 64);
    CHECK(n == 1 && (ev[0].flags & FSEV_XATTR) && (ev[0].flags & FSEV_OWNER) && (ev[0].flags & FSEV_INODE_META));
    seen = ev[n - 1].id;
    u8 fi[32] = {0}; fi[8] = 0x40;
    vfs_xattr_set(f, "com.apple.FinderInfo", fi, 32, 0, 0);
    n = evs(seen, ev, 64);
    CHECK(n == 1 && (ev[0].flags & FSEV_FINDER));
    seen = ev[n - 1].id;
    /* apagar uma árvore: um evento por item */
    vfs_mkdir("/Users/Visitante/arv/a/b");
    vfs_write_file("/Users/Visitante/arv/a/b/c.txt", "c", 1);
    n = evs(seen, ev, 64);
    seen = ev[n - 1].id;
    vfs_delete(vfs_lookup("/Users/Visitante/arv"));
    n = evs(seen, ev, 64);
    int rem = 0;
    for (int i = 0; i < n; i++) if (ev[i].flags & FSEV_REMOVED) rem++;
    CHECK(n == 4 && rem == 4);
    seen = ev[n - 1].id;
    /* a pasta-mãe que ganhou um filho não vira "modificada" */
    vfs_write_file("/Users/Visitante/Documentos/novo2", "", 0);
    n = evs(seen, ev, 64);
    CHECK(n == 1 && (ev[0].flags & FSEV_CREATED));
    seen = ev[n - 1].id;
    /* mudo (espelho de volume): nada */
    vfs_ev_mute++;
    vfs_write_file("/Users/Visitante/mudo", "", 0);
    vfs_ev_mute--;
    CHECK(evs(seen, ev, 64) == 0);
    /* lacuna: id de antes do boot */
    vfs_ev_read(~0ull, NULL, 0, &last);
    vfs_ev_set_base(last + 1000);
    vfs_write_file("/Users/Visitante/depois", "", 0);
    n = evs(5, ev, 64);
    CHECK(n == 2 && (ev[0].flags & FSEV_KERNEL_DROPPED) && (ev[0].flags & FSEV_MUST_SCAN) && ev[1].id == last + 1000);
    /* o .vfsmeta leva o próximo id */
    u64 len = 0;
    u8 *img = vmeta_build(vfs_root(), track_all, NULL, &len);
    CHECK(img && vmeta_check(img, len, NULL) && img[8] == 2);
    u64 nid = vfs_ev_next_id();
    kfree(img);
    CHECK(nid == last + 1001);
}

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
    vfs_ev_mute++;                                           /* o anel de eventos guarda caminhos: fora da conta */
    long long before = g_live_allocs;
    vnode_t *z = vfs_write_file("/Users/Visitante/z", "z", 1);
    vfs_xattr_set(z, "k1", "v", 1, 0, 0);
    vfs_xattr_set(z, "k2", "v", 1, 0, 0);
    vfs_delete(z);
    CHECK(g_live_allocs == before);
    vfs_ev_mute--;
    CHECK(meta_marks > 0);

    test_perms();
    test_events();
    printf("vfsmeta: %d verificações, %d falha(s)\n", checks, fails);
    return fails != 0;
}
