/* mOS — teste da File Manager do Carbon (userland/libsystem/filemgr.c) sobre o VFS real do kernel:
 * sh tools/filemgr_test.sh */
#include <stdio.h>
#include <stdlib.h>
#include "sys.h"
int cx_errno;
long kfsm(int op, mos_fsmeta_t *rq);
long kchmod(const char *p, unsigned m);
extern const char *k_fd_path[64];
typedef struct vnode vnode_t;
vnode_t *vfs_write_file(const char *path, const void *data, size_t n);
vnode_t *vfs_mkdir(const char *path);
vnode_t *vfs_lookup(const char *path);
vnode_t *vfs_create(const char *path);
int vfs_count(vnode_t *dir);
_Bool vfs_delete(vnode_t *n);
_Bool vfs_move(vnode_t *n, vnode_t *newparent, const char *newname);
long vfs_read_all(const char *p, void *buf, long cap);
void vfs_init(void);
long host_sys(long n, long a, long b, long c) {
    if (n == SYS_FSMETA) return kfsm((int)a, (mos_fsmeta_t *)b);
    if (n == SYS_CHMOD) return kchmod((const char *)a, (unsigned)b);
    (void)c;
    return -78;
}
size_t strlcpy(char *d, const char *s, size_t n) { size_t k = strlen(s); if (n) { size_t m = k < n - 1 ? k : n - 1; memcpy(d, s, m); d[m] = 0; } return k; }
size_t strlcat(char *d, const char *s, size_t n) { size_t a = strlen(d); return a + strlcpy(d + a, s, n > a ? n - a : 0); }
const char *cx_path(const char *in, char *out, size_t cap) { strlcpy(out, in, cap); return out; }
int cx_follow_path(const char *in, char *out, size_t cap) { strlcpy(out, in, cap); return 0; }
const char *cx_at_path(int dirfd, const char *p, char *buf, size_t cap) { (void)dirfd; strlcpy(buf, p, cap); return buf; }
int geteuid(void) { return 501; }
int getuid(void) { return 501; }
int getegid(void) { return 20; }
const char *getlogin(void) { return "visitante"; }
char *getenv(const char *n) { return !strcmp(n, "HOME") ? "/Users/Visitante" : 0; }
int chmod(const char *p, unsigned short m) { long r = kchmod(p, m); if (r < 0) { cx_errno = (int)-r; return -1; } return 0; }

/* E/S POSIX do filemgr.c (compilado com -Dopen=t_open...) sobre o VFS do kernel */
static struct { char path[512]; long off; int used; } tf[16];
#define MAXF (1 << 20)
static unsigned char iobuf[MAXF];
int t_open(const char *p, int flags, ...) {
    vnode_t *v = vfs_lookup(p);
    if (v && (flags & 0x200) && (flags & 0x800)) { cx_errno = 17; return -1; }
    if (!v) {
        if (!(flags & 0x200)) { cx_errno = 2; return -1; }
        if (!vfs_create(p)) { cx_errno = 2; return -1; }
    }
    for (int i = 0; i < 16; i++) if (!tf[i].used) {
        tf[i].used = 1; tf[i].off = 0; strlcpy(tf[i].path, p, sizeof tf[i].path);
        k_fd_path[i + 10] = tf[i].path;
        return i + 10;
    }
    cx_errno = 24;
    return -1;
}
long t_read(int fd, void *b, size_t n) {
    int i = fd - 10;
    long len = vfs_read_all(tf[i].path, iobuf, MAXF);
    long k = len - tf[i].off;
    if (k <= 0) return 0;
    if (k > (long)n) k = (long)n;
    memcpy(b, iobuf + tf[i].off, (size_t)k);
    tf[i].off += k;
    return k;
}
long t_write(int fd, const void *b, size_t n) {
    int i = fd - 10;
    long len = vfs_read_all(tf[i].path, iobuf, MAXF);
    if (len < 0) len = 0;
    if (tf[i].off > len) memset(iobuf + len, 0, (size_t)(tf[i].off - len));
    memcpy(iobuf + tf[i].off, b, n);
    tf[i].off += (long)n;
    if (tf[i].off > len) len = tf[i].off;
    vfs_write_file(tf[i].path, iobuf, (size_t)len);
    return (long)n;
}
long lseek(int fd, long off, int wh) { int i = fd - 10; if (wh == 0) tf[i].off = off; else if (wh == 1) tf[i].off += off; return tf[i].off; }
int ftruncate(int fd, long n) {
    int i = fd - 10;
    long len = vfs_read_all(tf[i].path, iobuf, MAXF);
    if (len < 0) len = 0;
    if (n > len) memset(iobuf + len, 0, (size_t)(n - len));
    vfs_write_file(tf[i].path, iobuf, (size_t)n);
    return 0;
}
int fsync(int fd) { (void)fd; return 0; }
int t_close(int fd) { int i = fd - 10; tf[i].used = 0; k_fd_path[fd] = 0; return 0; }
int mkdir(const char *p, unsigned short m) { (void)m; if (vfs_lookup(p)) { cx_errno = 17; return -1; } return vfs_mkdir(p) ? 0 : (cx_errno = 2, -1); }
int rmdir(const char *p) {
    vnode_t *v = vfs_lookup(p);
    if (!v) { cx_errno = 2; return -1; }
    if (vfs_count(v)) { cx_errno = 66; return -1; }
    return vfs_delete(v) ? 0 : (cx_errno = 1, -1);
}
int unlink(const char *p) { vnode_t *v = vfs_lookup(p); if (!v) { cx_errno = 2; return -1; } return vfs_delete(v) ? 0 : (cx_errno = 1, -1); }
int rename(const char *a, const char *b) {
    vnode_t *v = vfs_lookup(a);
    char d[512];
    strlcpy(d, b, sizeof d);
    char *s = strrchr(d, '/');
    *s = 0;
    vnode_t *par = vfs_lookup(d[0] ? d : "/");
    if (!v || !par) { cx_errno = 2; return -1; }
    return vfs_move(v, par, s + 1) ? 0 : (cx_errno = 1, -1);
}

typedef struct { unsigned char hidden[80]; } FSRef;
typedef struct { unsigned short length; unsigned short unicode[255]; } HFSUniStr255;
#pragma pack(push, 2)
typedef struct { unsigned short hi; unsigned lo; unsigned short frac; } UTCDateTime;
typedef struct { unsigned uid, gid; unsigned char r1, userAccess; unsigned short mode; void *fileSec; } FSPermissionInfo;
typedef struct {
    unsigned short nodeFlags; short volume; unsigned parentDirID, nodeID;
    unsigned char sharingFlags, userPrivileges, r1, r2;
    UTCDateTime createDate, contentModDate, attributeModDate, accessDate, backupDate;
    FSPermissionInfo permissions;
    unsigned char finderInfo[16], extFinderInfo[16];
    unsigned long long dataLogicalSize, dataPhysicalSize, rsrcLogicalSize, rsrcPhysicalSize;
    unsigned valence, textEncodingHint;
} FSCatalogInfo;
#pragma pack(pop)
typedef struct { int initialize; short data[6]; } CatPositionRec;
int FSPathMakeRef(const unsigned char *path, FSRef *ref, unsigned char *isDir);
int FSPathMakeRefWithOptions(const unsigned char *path, unsigned opt, FSRef *ref, unsigned char *isDir);
int FSRefMakePath(const FSRef *ref, unsigned char *path, unsigned max);
unsigned char FSIsFSRefValid(const FSRef *ref);
short FSCompareFSRefs(const FSRef *a, const FSRef *b);
short FSGetCatalogInfo(const FSRef *ref, unsigned which, FSCatalogInfo *ci, HFSUniStr255 *name, void *spec, FSRef *parent);
short FSSetCatalogInfo(const FSRef *ref, unsigned which, const FSCatalogInfo *ci);
short FSMakeFSRefUnicode(const FSRef *parent, unsigned long n, const unsigned short *name, unsigned enc, FSRef *out);
short FSCreateFileUnicode(const FSRef *parent, unsigned long n, const unsigned short *name, unsigned which, const FSCatalogInfo *ci, FSRef *out, void *spec);
short FSCreateDirectoryUnicode(const FSRef *parent, unsigned long n, const unsigned short *name, unsigned which, const FSCatalogInfo *ci, FSRef *out, void *spec, unsigned *dirid);
short FSDeleteObject(const FSRef *ref);
short FSRenameUnicode(const FSRef *ref, unsigned long n, const unsigned short *name, unsigned enc, FSRef *out);
short FSMoveObject(const FSRef *ref, const FSRef *dest, FSRef *out);
short FSExchangeObjects(const FSRef *a, const FSRef *b);
short FSGetDataForkName(HFSUniStr255 *n);
short FSGetResourceForkName(HFSUniStr255 *n);
short FSOpenFork(const FSRef *ref, unsigned long n, const unsigned short *name, signed char perm, int *fr);
short FSReadFork(int fr, unsigned short mode, long long off, unsigned long req, void *buf, unsigned long *got);
short FSWriteFork(int fr, unsigned short mode, long long off, unsigned long req, const void *buf, unsigned long *put);
short FSGetForkPosition(int fr, long long *pos);
short FSSetForkPosition(int fr, unsigned short mode, long long off);
short FSGetForkSize(int fr, long long *size);
short FSSetForkSize(int fr, unsigned short mode, long long off);
short FSCloseFork(int fr);
short FSCreateFork(const FSRef *ref, unsigned long n, const unsigned short *name);
short FSDeleteFork(const FSRef *ref, unsigned long n, const unsigned short *name);
short FSIterateForks(const FSRef *ref, CatPositionRec *it, HFSUniStr255 *name, long long *size, unsigned long long *phys);
short FSOpenIterator(const FSRef *c, unsigned flags, void **it);
short FSCloseIterator(void *it);
short FSGetCatalogInfoBulk(void *it, unsigned long max, unsigned long *actual, unsigned char *changed, unsigned which, FSCatalogInfo *ci, FSRef *refs, void *specs, HFSUniStr255 *names);
short FSFindFolder(short vref, unsigned type, unsigned char create, FSRef *out);
long getxattr(const char *p, const char *n, void *v, size_t s, unsigned pos, int o);
int setxattr(const char *p, const char *n, const void *v, size_t s, unsigned pos, int o);
int cx_statx(const char *abs, int fd, mos_statx_t *st);
int cx_set_birthtime(const char *abs, int fd, long long ns);

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALHA %d: %s\n", __LINE__, #c); } } while (0)
static int path_is(const FSRef *r, const char *want) {
    unsigned char p[1024];
    return FSRefMakePath(r, p, sizeof p) == 0 && !strcmp((char *)p, want);
}
static unsigned long u16s(const char *s, unsigned short *o) { unsigned long n = 0; while (s[n]) { o[n] = (unsigned char)s[n]; n++; } return n; }
static int name_is(const HFSUniStr255 *h, const char *want) {
    if (h->length != strlen(want)) return 0;
    for (unsigned i = 0; i < h->length; i++) if (h->unicode[i] != (unsigned char)want[i]) return 0;
    return 1;
}
#define FOUR(s) (((unsigned)(s)[0] << 24) | ((unsigned)(s)[1] << 16) | ((unsigned)(s)[2] << 8) | (unsigned)(s)[3])

int main(void) {
    vfs_init();
    vfs_write_file("/Users/Visitante/fm/a.txt", "abcdef", 6);
    vfs_write_file("/Users/Visitante/fm/sub/1", "x", 1);
    vfs_write_file("/Users/Visitante/fm/sub/2", "yy", 2);
    vfs_write_file("/Users/Visitante/fm/com:dois", "", 0);
    FSRef r, d, r2, root;
    unsigned char isdir = 9, buf[1024];
    unsigned short u[64];
    HFSUniStr255 nm;
    FSCatalogInfo ci;
    mos_statx_t x, dx;

    /* ---- caminho -> FSRef -> caminho (o teste cfurl_fm_basic do Darling: ida e volta) */
    CHECK(FSPathMakeRef((const unsigned char *)"/Users/Visitante/fm/a.txt", &r, &isdir) == 0 && isdir == 0);
    CHECK(path_is(&r, "/Users/Visitante/fm/a.txt"));
    CHECK(FSPathMakeRef((const unsigned char *)"/Users", &d, &isdir) == 0 && isdir == 1 && path_is(&d, "/Users"));
    CHECK(FSPathMakeRef((const unsigned char *)"/", &root, &isdir) == 0 && isdir == 1 && path_is(&root, "/"));
    CHECK(FSPathMakeRefWithOptions((const unsigned char *)"/Users/Visitante/fm", 1, &d, 0) == 0);
    CHECK(FSPathMakeRef((const unsigned char *)"/Users/Visitante/fm/nao", &r2, 0) == -43);
    CHECK(FSPathMakeRef((const unsigned char *)"", &r2, 0) == -50);
    CHECK(FSRefMakePath(&r, buf, 5) == -1410);
    FSRef bad = {{0}};
    CHECK(FSRefMakePath(&bad, buf, sizeof buf) == -1401 && !FSIsFSRefValid(&bad) && FSIsFSRefValid(&r));
    CHECK(FSPathMakeRef((const unsigned char *)"/Users/Visitante/fm/a.txt", &r2, 0) == 0 && FSCompareFSRefs(&r, &r2) == 0);
    CHECK(FSCompareFSRefs(&r, &d) == -1420 && FSCompareFSRefs(&r, &bad) == -1401);

    /* ---- catálogo */
    cx_set_birthtime("/Users/Visitante/fm/a.txt", -1, 1700000000LL * 1000000000LL + 500000000LL);
    CHECK(cx_statx("/Users/Visitante/fm/a.txt", -1, &x) == 0 && cx_statx("/Users/Visitante/fm", -1, &dx) == 0);
    FSRef par;
    CHECK(FSGetCatalogInfo(&r, 0xffffffffu & ~0x100000u, &ci, &nm, 0, &par) == 0);
    CHECK(!(ci.nodeFlags & 0x10) && ci.nodeID == (unsigned)x.ino && ci.parentDirID == (unsigned)dx.ino);
    CHECK(ci.dataLogicalSize == 6 && ci.dataPhysicalSize >= 6 && ci.rsrcLogicalSize == 0);
    CHECK(name_is(&nm, "a.txt") && path_is(&par, "/Users/Visitante/fm"));
    CHECK(ci.createDate.hi == 0 && ci.createDate.lo == (unsigned)(1700000000LL + 2082844800LL) && ci.createDate.frac == 32768);
    CHECK((ci.permissions.mode & 0170000) == 0100000 && ci.permissions.uid == x.uid && ci.permissions.userAccess & 4);
    CHECK(ci.volume == -100);
    CHECK(FSGetCatalogInfo(&d, 0x2000 | 0x2, &ci, &nm, 0, 0) == 0 && (ci.nodeFlags & 0x10) && ci.valence == 3 && name_is(&nm, "fm"));
    CHECK(FSGetCatalogInfo(&root, 0x18, &ci, 0, 0, &par) == 0 && ci.nodeID == 2 && ci.parentDirID == 1 && !FSIsFSRefValid(&par));
    FSRef colon;
    CHECK(FSPathMakeRef((const unsigned char *)"/Users/Visitante/fm/com:dois", &colon, 0) == 0);
    CHECK(FSGetCatalogInfo(&colon, 0, 0, &nm, 0, 0) == 0 && name_is(&nm, "com/dois"));   /* ":" do POSIX = "/" do Carbon */

    /* ---- FSSetCatalogInfo: FinderInfo, datas, permissões, trava */
    memset(&ci, 0, sizeof ci);
    memcpy(ci.finderInfo, "TEXTttxt", 8);
    ci.contentModDate.lo = (unsigned)(1600000000LL + 2082844800LL);
    ci.permissions.uid = x.uid; ci.permissions.gid = x.gid; ci.permissions.mode = 0600;
    CHECK(FSSetCatalogInfo(&r, 0x800 | 0x40 | 0x400, &ci) == 0);
    unsigned char fi[32] = {0};
    CHECK(getxattr("/Users/Visitante/fm/a.txt", "com.apple.FinderInfo", fi, 32, 0, 0) == 32 && !memcmp(fi, "TEXTttxt", 8));
    CHECK(cx_statx("/Users/Visitante/fm/a.txt", -1, &x) == 0 && x.mtime_ns == 1600000000LL * 1000000000LL && (x.mode & 07777) == 0600);
    CHECK(FSGetCatalogInfo(&r, 0x800, &ci, 0, 0, 0) == 0 && !memcmp(ci.finderInfo, "TEXTttxt", 8));
    CHECK(FSSetCatalogInfo(&r, 0x4000, &ci) == -1405);   /* tamanho não é gravável */
    ci.nodeFlags = 1;
    CHECK(FSSetCatalogInfo(&r, 0x2, &ci) == 0 && cx_statx("/Users/Visitante/fm/a.txt", -1, &x) == 0 && (x.flags & 2));
    CHECK(FSGetCatalogInfo(&r, 0x2, &ci, 0, 0, 0) == 0 && (ci.nodeFlags & 1));
    CHECK(FSDeleteObject(&r) == -45);
    ci.nodeFlags = 0;
    CHECK(FSSetCatalogInfo(&r, 0x2, &ci) == 0 && cx_statx("/Users/Visitante/fm/a.txt", -1, &x) == 0 && !(x.flags & 2));

    /* ---- criar, achar, renomear, mover: o FSRef acompanha o objeto */
    FSRef nf, nd, f2;
    unsigned long n = u16s("b/c", u);
    CHECK(FSCreateFileUnicode(&d, n, u, 0, 0, &nf, 0) == 0 && path_is(&nf, "/Users/Visitante/fm/b:c"));
    CHECK(FSCreateFileUnicode(&d, n, u, 0, 0, 0, 0) == -48);
    CHECK(FSCreateFileUnicode(&r, n, u, 0, 0, 0, 0) == -1407);   /* pai não é pasta */
    CHECK(FSCreateFileUnicode(&d, 0, u, 0, 0, 0, 0) == -1411);
    n = u16s("..", u);
    CHECK(FSCreateFileUnicode(&d, n, u, 0, 0, 0, 0) == -37);
    unsigned dirid = 0;
    n = u16s("pasta", u);
    CHECK(FSCreateDirectoryUnicode(&d, n, u, 0, 0, &nd, 0, &dirid) == 0 && path_is(&nd, "/Users/Visitante/fm/pasta"));
    CHECK(cx_statx("/Users/Visitante/fm/pasta", -1, &x) == 0 && dirid == (unsigned)x.ino && x.kind == 1);
    CHECK(FSMakeFSRefUnicode(&d, n, u, 0, &f2) == 0 && FSCompareFSRefs(&f2, &nd) == 0);
    n = u16s("nada", u);
    CHECK(FSMakeFSRefUnicode(&d, n, u, 0, &f2) == -43);
    memset(&ci, 0, sizeof ci);
    memcpy(ci.finderInfo, "APPLaaaa", 8);
    n = u16s("comfi", u);
    CHECK(FSCreateFileUnicode(&d, n, u, 0x800, &ci, &f2, 0) == 0 && getxattr("/Users/Visitante/fm/comfi", "com.apple.FinderInfo", fi, 32, 0, 0) == 32 && !memcmp(fi, "APPL", 4));
    n = u16s("novo.txt", u);
    CHECK(FSRenameUnicode(&nf, n, u, 0, &f2) == 0 && path_is(&f2, "/Users/Visitante/fm/novo.txt") && path_is(&nf, "/Users/Visitante/fm/novo.txt"));
    n = u16s("pasta", u);
    CHECK(FSRenameUnicode(&nf, n, u, 0, 0) == -48);
    CHECK(FSMoveObject(&nf, &nd, 0) == 0 && path_is(&nf, "/Users/Visitante/fm/pasta/novo.txt"));
    CHECK(FSMoveObject(&nd, &nd, 0) == -50 && FSMoveObject(&nf, &r, 0) == -1407);

    /* ---- forks */
    HFSUniStr255 df, rf;
    CHECK(FSGetDataForkName(&df) == 0 && df.length == 0 && FSGetResourceForkName(&rf) == 0 && name_is(&rf, "RESOURCE_FORK"));
    int fr = 0;
    unsigned long got = 0;
    long long pos = 0;
    char rb[64];
    CHECK(FSOpenFork(&nf, 0, 0, 3, &fr) == 0);
    CHECK(FSWriteFork(fr, 1, 0, 5, "hello", &got) == 0 && got == 5);
    CHECK(FSWriteFork(fr, 0, 0, 3, " mu", &got) == 0 && FSGetForkPosition(fr, &pos) == 0 && pos == 8);
    CHECK(FSGetForkSize(fr, &pos) == 0 && pos == 8);
    memset(rb, 0, sizeof rb);
    CHECK(FSReadFork(fr, 1, 0, 8, rb, &got) == 0 && got == 8 && !memcmp(rb, "hello mu", 8));
    CHECK(FSReadFork(fr, 1, 6, 10, rb, &got) == -39 && got == 2 && !memcmp(rb, "mu", 2));
    CHECK(FSReadFork(fr, 2, -3, 3, rb, &got) == 0 && !memcmp(rb, " mu", 3));
    CHECK(FSSetForkPosition(fr, 1, 100) == -40 && FSSetForkPosition(fr, 1, -1) == -40);
    CHECK(FSSetForkSize(fr, 1, 2) == 0 && FSGetForkSize(fr, &pos) == 0 && pos == 2 && FSGetForkPosition(fr, &pos) == 0 && pos == 2);
    CHECK(FSCloseFork(fr) == 0 && FSCloseFork(fr) == -51);
    CHECK(vfs_read_all("/Users/Visitante/fm/pasta/novo.txt", rb, sizeof rb) == 2 && !memcmp(rb, "he", 2));
    CHECK(FSOpenFork(&nf, 0, 0, 1, &fr) == 0 && FSWriteFork(fr, 1, 0, 1, "z", &got) == -61 && FSCloseFork(fr) == 0);
    CHECK(FSOpenFork(&nf, rf.length, rf.unicode, 1, &fr) == -1409);   /* sem resource fork */
    unsigned short wrong[3] = {'x', 'y', 'z'};
    CHECK(FSOpenFork(&nf, 3, wrong, 1, &fr) == -1402 && FSOpenFork(&nd, 0, 0, 1, &fr) == -1302);
    CHECK(FSCreateFork(&nf, rf.length, rf.unicode) == 0 && FSCreateFork(&nf, rf.length, rf.unicode) == -1421);
    CHECK(FSOpenFork(&nf, rf.length, rf.unicode, 3, &fr) == 0 && FSWriteFork(fr, 1, 0, 4, "RSRC", &got) == 0);
    CHECK(FSWriteFork(fr, 1, 10, 2, "!!", &got) == 0 && FSGetForkSize(fr, &pos) == 0 && pos == 12 && FSCloseFork(fr) == 0);
    CHECK(getxattr("/Users/Visitante/fm/pasta/novo.txt", "com.apple.ResourceFork", rb, sizeof rb, 0, 0) == 12 && !memcmp(rb, "RSRC\0\0\0\0\0\0!!", 12));
    CHECK(FSGetCatalogInfo(&nf, 0x8000 | 0x4000, &ci, 0, 0, 0) == 0 && ci.rsrcLogicalSize == 12 && ci.dataLogicalSize == 2);
    CatPositionRec cp = {0};
    HFSUniStr255 fk;
    long long fsz;
    CHECK(FSIterateForks(&nf, &cp, &fk, &fsz, 0) == 0 && fk.length == 0 && fsz == 2);
    CHECK(FSIterateForks(&nf, &cp, &fk, &fsz, 0) == 0 && name_is(&fk, "RESOURCE_FORK") && fsz == 12);
    CHECK(FSIterateForks(&nf, &cp, &fk, &fsz, 0) == -1417);
    CHECK(FSDeleteFork(&nf, rf.length, rf.unicode) == 0 && FSDeleteFork(&nf, rf.length, rf.unicode) == -1409);
    CHECK(getxattr("/Users/Visitante/fm/pasta/novo.txt", "com.apple.ResourceFork", 0, 0, 0, 0) < 0);

    /* ---- iteradores */
    void *it = 0;
    FSRef refs[16];
    HFSUniStr255 names[16];
    FSCatalogInfo cis[16];
    unsigned long act = 0;
    CHECK(FSOpenIterator(&d, 0, &it) == 0);
    CHECK(FSGetCatalogInfoBulk(it, 2, &act, 0, 0x4002, cis, refs, 0, names) == 0 && act == 2);
    unsigned long tot = act;
    CHECK(FSGetCatalogInfoBulk(it, 16, &act, 0, 0x4002, cis, refs, 0, names) == -1417);
    tot += act;
    CHECK(tot == 5);   /* a.txt, sub, com:dois, pasta, comfi (o b:c virou pasta/novo.txt) */
    CHECK(FSCloseIterator(it) == 0);
    CHECK(FSOpenIterator(&d, 1, &it) == 0);
    tot = 0;
    int achou = 0;
    for (;;) {
        short s = FSGetCatalogInfoBulk(it, 3, &act, 0, 0, 0, refs, 0, names);
        for (unsigned long i = 0; i < act; i++) if (name_is(&names[i], "novo.txt") && path_is(&refs[i], "/Users/Visitante/fm/pasta/novo.txt")) achou = 1;
        tot += act;
        if (s) { CHECK(s == -1417); break; }
    }
    CHECK(tot == 8 && achou);   /* + sub/1, sub/2, pasta/novo.txt */
    CHECK(FSCloseIterator(it) == 0 && FSOpenIterator(&r, 0, &it) == -1407 && FSOpenIterator(&d, 4, &it) == -1422);

    /* ---- trocar e apagar */
    vfs_write_file("/Users/Visitante/fm/x1", "um", 2);
    vfs_write_file("/Users/Visitante/fm/x2", "dois", 4);
    FSRef a1, a2;
    FSPathMakeRef((const unsigned char *)"/Users/Visitante/fm/x1", &a1, 0);
    FSPathMakeRef((const unsigned char *)"/Users/Visitante/fm/x2", &a2, 0);
    CHECK(FSExchangeObjects(&a1, &a2) == 0 && vfs_read_all("/Users/Visitante/fm/x1", rb, sizeof rb) == 4 && !memcmp(rb, "dois", 4));
    CHECK(FSDeleteObject(&nd) == -47);   /* pasta com novo.txt */
    CHECK(FSDeleteObject(&nf) == 0 && FSRefMakePath(&nf, buf, sizeof buf) == -43 && !FSIsFSRefValid(&nf));
    CHECK(FSDeleteObject(&nd) == 0 && !vfs_lookup("/Users/Visitante/fm/pasta"));

    /* ---- Folder Manager */
    FSRef ff;
    CHECK(FSFindFolder(-32763, FOUR("pref"), 1, &ff) == 0 && path_is(&ff, "/Users/Visitante/Library/Preferences"));
    CHECK(FSFindFolder(-32763, FOUR("docs"), 0, &ff) == -43);
    CHECK(FSFindFolder(-32763, FOUR("docs"), 1, &ff) == 0 && path_is(&ff, "/Users/Visitante/Documents"));
    CHECK(FSFindFolder(-32767, FOUR("cusr"), 0, &ff) == 0 && path_is(&ff, "/Users/Visitante"));
    CHECK(FSFindFolder(-32765, FOUR("asup"), 1, &ff) == 0 && path_is(&ff, "/Library/Application Support"));
    CHECK(FSFindFolder(-32764, FOUR("pref"), 1, &ff) == -43 && FSFindFolder(-32763, FOUR("zzzz"), 1, &ff) == -43);
    CHECK(FSFindFolder(-32763, FOUR("desk"), 1, 0) == -50);

    printf("filemgr: %d verificações, %d falha(s)\n", checks, fails);
    return fails != 0;
}
