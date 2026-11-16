/* mOS — teste do getattrlist/setattrlist/getattrlistbulk/statfs/renamex/clonefile/exchangedata/fsgetpath do
 * libSystem (attrlist.c + fsmeta.c) sobre o VFS real do kernel: sh tools/attrlist_test.sh */
#include <stdio.h>
#include <stdlib.h>
#include "sys.h"
int cx_errno;
long kfsm(int op, mos_fsmeta_t *rq);
long kchmod(const char *p, unsigned m);
extern const char *k_fd_path[64];
extern int k_euid;
typedef struct vnode vnode_t;
vnode_t *vfs_write_file(const char *path, const void *data, size_t n);
vnode_t *vfs_mkdir(const char *path);
void vfs_init(void);
long host_sys(long n, long a, long b, long c) {
    if (n == SYS_FSMETA) return kfsm((int)a, (mos_fsmeta_t *)b);
    if (n == SYS_CHMOD) return kchmod((const char *)a, (unsigned)b);
    if (n == SYS_FDPATH) { if (a < 0 || a >= 64 || !k_fd_path[a]) return -9; strcpy((char *)b, k_fd_path[a]); return (long)strlen(k_fd_path[a]); }
    (void)c;
    return -78;
}
size_t strlcpy(char *d, const char *s, size_t n) { size_t k = strlen(s); if (n) { size_t m = k < n - 1 ? k : n - 1; memcpy(d, s, m); d[m] = 0; } return k; }
size_t strlcat(char *d, const char *s, size_t n) { size_t a = strlen(d); return a + strlcpy(d + a, s, n > a ? n - a : 0); }
const char *cx_path(const char *in, char *out, size_t cap) { strlcpy(out, in, cap); return out; }
int cx_follow_path(const char *in, char *out, size_t cap) { strlcpy(out, in, cap); return 0; }
const char *cx_at_path(int dirfd, const char *p, char *buf, size_t cap) { (void)dirfd; strlcpy(buf, p, cap); return buf; }
static long fdoff[64];
long lseek(int fd, long off, int wh) { if (wh == 0) fdoff[fd] = off; else if (wh == 1) fdoff[fd] += off; return fdoff[fd]; }
int rename(const char *a, const char *b) { (void)a; (void)b; return 0; }
int geteuid(void) { return 501; }
int getegid(void) { return 20; }

int getattrlist(const char *p, void *al, void *buf, size_t n, unsigned opt);
int setattrlist(const char *p, void *al, void *buf, size_t n, unsigned opt);
int getattrlistbulk(int dirfd, void *al, void *buf, size_t n, u64 opt);
int statfs(const char *p, void *s);
int getfsstat(void *buf, int bufsize, int flags);
int renamex_np(const char *a, const char *b, unsigned flags);
int clonefile(const char *a, const char *b, u32 flags);
int exchangedata(const char *a, const char *b, unsigned opt);
long fsgetpath(char *buf, size_t n, void *fsid, u64 objid);
int setxattr(const char *p, const char *n, const void *v, size_t s, u32 pos, int o);
long getxattr(const char *p, const char *n, void *v, size_t s, u32 pos, int o);
int cx_statx(const char *abs, int fd, mos_statx_t *st);
typedef struct { unsigned short bitmapcount, reserved; u32 common, vol, dir, file, fork; } al_t;

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALHA %d: %s\n", __LINE__, #c); } } while (0)
static u32 g32(const u8 *p) { u32 v; memcpy(&v, p, 4); return v; }
static u64 g64(const u8 *p) { u64 v; memcpy(&v, p, 8); return v; }

int main(void) {
    vfs_init();
    vfs_write_file("/Users/Visitante/at/f.txt", "abcdef", 6);
    vfs_mkdir("/Users/Visitante/at/d");
    vfs_write_file("/Users/Visitante/at/d/1", "", 0);
    vfs_write_file("/Users/Visitante/at/d/2", "", 0);
    setxattr("/Users/Visitante/at/f.txt", "com.apple.ResourceFork", "RSRC", 4, 0, 0);
    mos_statx_t x;
    CHECK(cx_statx("/Users/Visitante/at/f.txt", -1, &x) == 0);
    /* NAME, OBJTYPE, FILEID, MODTIME, OWNERID, ACCESSMASK + TOTALSIZE, DATALENGTH, RSRCLENGTH, com RETURNED_ATTRS */
    al_t al = {5, 0, 0x80000000u | 0x1 | 0x8 | 0x400 | 0x8000 | 0x20000 | 0x2000000, 0, 0x2 /* dir ENTRYCOUNT */, 0x2 | 0x200 | 0x1000, 0};
    u8 buf[512];
    CHECK(getattrlist("/Users/Visitante/at/f.txt", &al, buf, sizeof buf, 0) == 0);
    u32 len = g32(buf);
    const u8 *p = buf + 4;
    u32 rc = g32(p), rd = g32(p + 8), rf = g32(p + 12);
    CHECK(rc == al.common && rd == 0 && rf == (0x2u | 0x200u | 0x1000u));     /* pasta: não num arquivo */
    p += 20;
    i32 no; u32 nl; memcpy(&no, p, 4); nl = g32(p + 4);
    CHECK(nl == 6 && !strcmp((const char *)p + no, "f.txt"));
    p += 8;
    CHECK(g32(p) == 1); p += 4;                                    /* VREG */
    i64 ts[2]; memcpy(ts, p, 16); p += 16;
    CHECK(ts[0] * 1000000000ll + ts[1] == x.mtime_ns);
    CHECK(g32(p) == 501); p += 4;
    CHECK(g32(p) == 0100644u); p += 4;
    CHECK(g64(p) == x.ino); p += 8;
    CHECK(g64(p) == 10); p += 8;                                   /* TOTALSIZE = dados + resource fork */
    CHECK(g64(p) == 6); p += 8;
    CHECK(g64(p) == 4); p += 8;
    CHECK((u32)(p - buf) + 8 == len);                              /* + o nome (6 -> 8 com o alinhamento) */
    /* pasta: ENTRYCOUNT e LINKCOUNT */
    al_t ad = {5, 0, 0x8, 0, 0x1 | 0x2, 0, 0};
    CHECK(getattrlist("/Users/Visitante/at/d", &ad, buf, sizeof buf, 0) == 0);
    CHECK(g32(buf) == 16 && g32(buf + 4) == 2 && g32(buf + 8) == 2 && g32(buf + 12) == 2);
    /* buffer curto: trunca; FSOPT_REPORT_FULLSIZE diz o tamanho cheio */
    CHECK(getattrlist("/Users/Visitante/at/d", &ad, buf, 8, 0x4) == 0 && g32(buf) == 16);
    CHECK(getattrlist("/Users/Visitante/at/d", &ad, buf, 8, 0) == 0 && g32(buf) == 8);
    /* bit inválido: EINVAL; com FSOPT_PACK_INVAL_ATTRS passa */
    al_t bad = {5, 0, 0, 0, 0x80, 0, 0};
    CHECK(getattrlist("/Users/Visitante/at/d", &bad, buf, sizeof buf, 0) == -1 && cx_errno == 22);
    CHECK(getattrlist("/Users/Visitante/at/d", &bad, buf, sizeof buf, 0x8) == 0);
    al_t five = {4, 0, 0x8, 0, 0, 0, 0};
    CHECK(getattrlist("/Users/Visitante/at/d", &five, buf, sizeof buf, 0) == -1 && cx_errno == 22);
    /* volume: CAPABILITIES, FSTYPENAME, MOUNTPOINT */
    al_t av = {5, 0, 0, 0x80000000u | 0x20000 | 0x1000 | 0x100000, 0, 0, 0};
    CHECK(getattrlist("/", &av, buf, sizeof buf, 0) == 0);
    p = buf + 4;
    i32 o1; memcpy(&o1, p, 4); CHECK(!strcmp((const char *)p + o1, "/")); p += 8;
    u32 fmt = g32(p), itf = g32(p + 4);
    CHECK((fmt & 0x200) && (fmt & 0x2) && !(fmt & 0x100) && (itf & 0x4000) && (itf & 0x40000) && (itf & 0x80000) && (itf & 0x10000));
    p += 32;
    i32 o2; memcpy(&o2, p, 4); CHECK(!strcmp((const char *)p + o2, "tmpfs"));
    /* setattrlist: datas de criação/modificação, modo, flags, FinderInfo */
    struct { i64 cr[2]; i64 mod[2]; u8 fi[32]; u32 mode; u32 flags; } __attribute__((packed)) sa;
    memset(&sa, 0, sizeof sa);
    sa.cr[0] = 1000000000; sa.mod[0] = 1100000000; sa.mod[1] = 5;
    sa.fi[0] = 'T'; sa.fi[1] = 'E'; sa.fi[2] = 'X'; sa.fi[3] = 'T'; sa.fi[8] = 0x40;
    sa.mode = 0600; sa.flags = 0x8000;                             /* UF_HIDDEN */
    al_t as = {5, 0, 0x200 | 0x400 | 0x4000 | 0x20000 | 0x40000, 0, 0, 0, 0};
    CHECK(setattrlist("/Users/Visitante/at/f.txt", &as, &sa, sizeof sa, 0) == 0);
    CHECK(cx_statx("/Users/Visitante/at/f.txt", -1, &x) == 0);
    CHECK(x.btime_ns == 1000000000ll * 1000000000ll && x.mtime_ns == 1100000000ll * 1000000000ll + 5);
    CHECK((x.mode & 07777) == 0600 && (x.flags & 0x8000));
    u8 fi[32];
    CHECK(getxattr("/Users/Visitante/at/f.txt", "com.apple.FinderInfo", fi, 32, 0, 0) == 32 && !memcmp(fi, "TEXT", 4));
    al_t rf2 = {5, 0, 0x4000, 0, 0, 0, 0};
    CHECK(getattrlist("/Users/Visitante/at/f.txt", &rf2, buf, sizeof buf, 0) == 0 && !memcmp(buf + 4, "TEXT", 4));
    /* bulk: as duas entradas de d, com RETURNED_ATTRS e NAME */
    k_fd_path[5] = "/Users/Visitante/at/d";
    al_t ab = {5, 0, 0x80000000u | 0x1 | 0x8, 0, 0, 0, 0};
    u8 big[4096];
    int n = getattrlistbulk(5, &ab, big, sizeof big, 0);
    CHECK(n == 2);
    const u8 *e = big;
    char names[2][8] = {{0}};
    for (int i = 0; i < n && i < 2; i++) {
        u32 l = g32(e);
        i32 off; memcpy(&off, e + 24, 4);
        strncpy(names[i], (const char *)e + 24 + off, 7);
        CHECK(g32(e + 32) == 1 && (l % 8) == 0);
        e += l;
    }
    CHECK(!strcmp(names[0], "1") && !strcmp(names[1], "2"));
    CHECK(getattrlistbulk(5, &ab, big, sizeof big, 0) == 0);      /* fim */
    lseek(5, 0, 0);
    CHECK(getattrlistbulk(5, &ab, big, sizeof big, 0) == 2);      /* rewind */
    al_t nob = {5, 0, 0x1, 0, 0, 0, 0};
    CHECK(getattrlistbulk(5, &nob, big, sizeof big, 0) == -1 && cx_errno == 22);   /* sem RETURNED_ATTRS */
    /* statfs/getfsstat */
    u8 sf[2168];
    CHECK(statfs("/Users/Visitante", sf) == 0 && !strcmp((char *)sf + 72, "tmpfs") && !strcmp((char *)sf + 88, "/"));
    CHECK(getfsstat(0, 0, 0) == 1);
    /* renamex_np */
    vfs_write_file("/Users/Visitante/at/s1", "1", 1);
    vfs_write_file("/Users/Visitante/at/s2", "22", 2);
    CHECK(renamex_np("/Users/Visitante/at/s1", "/Users/Visitante/at/s2", 0x4) == -1 && cx_errno == 17);
    CHECK(renamex_np("/Users/Visitante/at/s1", "/Users/Visitante/at/s2", 0x2) == 0);
    CHECK(cx_statx("/Users/Visitante/at/s1", -1, &x) == 0 && x.size == 2);
    CHECK(renamex_np("/Users/Visitante/at/s1", "/Users/Visitante/at/s2", 0x6) == -1 && cx_errno == 22);
    /* clonefile e exchangedata */
    CHECK(clonefile("/Users/Visitante/at/f.txt", "/Users/Visitante/at/g.txt", 0) == 0);
    CHECK(cx_statx("/Users/Visitante/at/g.txt", -1, &x) == 0 && x.size == 6 && (x.flags & 0x8000) && x.nxattr == 2);
    CHECK(clonefile("/Users/Visitante/at/f.txt", "/Users/Visitante/at/g.txt", 0) == -1 && cx_errno == 17);
    CHECK(exchangedata("/Users/Visitante/at/s1", "/Users/Visitante/at/s2", 0) == 0);
    CHECK(cx_statx("/Users/Visitante/at/s1", -1, &x) == 0 && x.size == 1);
    /* fsgetpath */
    char fp[256];
    CHECK(fsgetpath(fp, sizeof fp, 0, x.ino) == (long)strlen("/Users/Visitante/at/s1") + 1 && !strcmp(fp, "/Users/Visitante/at/s1"));
    CHECK(fsgetpath(fp, 3, 0, x.ino) == -1 && cx_errno == 34);
    printf("attrlist: %d verificações, %d falha(s)\n", checks, fails);
    return fails != 0;
}
