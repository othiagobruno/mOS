/*
 * mOS libSystem — File Manager do Carbon (CoreServices/CarbonCore): FSRef, catálogo, forks, iteradores e o
 * Folder Manager (FSFindFolder). APIs obsoletas, mas apps antigos, plug-ins e instaladores ainda as ligam.
 *
 * FSRef (80 bytes opacos): "mOSf", u32 fsid do volume, u64 inode, resto zero. O inode do mOS é único no VFS
 * inteiro e não muda com rename/move, então o FSRef acompanha o arquivo como no HFS+ (o caminho sai do
 * fsgetpath a cada uso). Resource fork = xattr com.apple.ResourceFork; FinderInfo = com.apple.FinderInfo.
 *
 * Layouts do x86_64 (Files.h, #pragma pack(2)): UTCDateTime 8 bytes (u16 alto, u32 baixo, u16 fração; segundos
 * desde 1/1/1904 UTC); FSPermissionInfo 20 bytes (uid, gid, u8, u8 userAccess, u16 mode, ponteiro fileSec);
 * FSCatalogInfo 148 bytes; HFSUniStr255 512 bytes; FSIORefNum int.
 * Nomes: o "/" do Carbon é o ":" do POSIX e vice-versa (como o HFS+ faz).
 */
#include "sys.h"
#include "libc.h"

typedef short s16_t;
typedef signed char s8_t;
typedef i32 OSStatus;
typedef s16_t OSErr;
typedef u8 Boolean;
typedef u16 UniChar;
typedef unsigned long ItemCount;
typedef unsigned long UniCharCount;
typedef unsigned long ByteCount;
typedef int FSIORefNum;
typedef struct { u8 hidden[80]; } FSRef;
typedef struct { u16 length; UniChar unicode[255]; } HFSUniStr255;
#pragma pack(push, 2)
typedef struct { u16 highSeconds; u32 lowSeconds; u16 fraction; } UTCDateTime;
typedef struct { u32 userID, groupID; u8 reserved1, userAccess; u16 mode; void *fileSec; } FSPermissionInfo;
typedef struct {
    u16 nodeFlags;
    s16_t volume;
    u32 parentDirID, nodeID;
    u8 sharingFlags, userPrivileges, reserved1, reserved2;
    UTCDateTime createDate, contentModDate, attributeModDate, accessDate, backupDate;
    FSPermissionInfo permissions;
    u8 finderInfo[16], extFinderInfo[16];
    u64 dataLogicalSize, dataPhysicalSize, rsrcLogicalSize, rsrcPhysicalSize;
    u32 valence, textEncodingHint;
} FSCatalogInfo;
#pragma pack(pop)
_Static_assert(sizeof(UTCDateTime) == 8, "UTCDateTime");
_Static_assert(sizeof(FSPermissionInfo) == 20, "FSPermissionInfo");
_Static_assert(sizeof(FSCatalogInfo) == 148, "FSCatalogInfo");
_Static_assert(sizeof(HFSUniStr255) == 512, "HFSUniStr255");

/* erros (MacErrors.h) */
enum {
    noErr = 0, dskFulErr = -34, nsvErr = -35, ioErr = -36, bdNamErr = -37, eofErr = -39, posErr = -40,
    tmfoErr = -42, fnfErr = -43, wPrErr = -44, fLckdErr = -45, fBsyErr = -47, dupFNErr = -48, paramErr = -50,
    rfNumErr = -51, permErr = -54, wrPermErr = -61, memFullErr = -108, dirNFErr = -120, notAFileErr = -1302,
    diffVolErr = -1303, errFSBadFSRef = -1401, errFSBadForkName = -1402, errFSBadBuffer = -1403,
    errFSBadForkRef = -1404, errFSBadInfoBitmap = -1405, errFSNotAFolder = -1407, errFSForkNotFound = -1409,
    errFSNameTooLong = -1410, errFSMissingName = -1411, errFSBadPosMode = -1412, errFSNoMoreItems = -1417,
    errFSBadItemCount = -1418, errFSRefsDifferent = -1420, errFSForkExists = -1421,
    errFSBadIteratorFlags = -1422, errFSIteratorNotFound = -1423, afpAccessDenied = -5000,
};
/* FSCatalogInfoBitmap */
enum {
    kFSCatInfoTextEncoding = 0x1, kFSCatInfoNodeFlags = 0x2, kFSCatInfoVolume = 0x4, kFSCatInfoParentDirID = 0x8,
    kFSCatInfoNodeID = 0x10, kFSCatInfoCreateDate = 0x20, kFSCatInfoContentMod = 0x40, kFSCatInfoAttrMod = 0x80,
    kFSCatInfoAccessDate = 0x100, kFSCatInfoBackupDate = 0x200, kFSCatInfoPermissions = 0x400,
    kFSCatInfoFinderInfo = 0x800, kFSCatInfoFinderXInfo = 0x1000, kFSCatInfoValence = 0x2000,
    kFSCatInfoDataSizes = 0x4000, kFSCatInfoRsrcSizes = 0x8000, kFSCatInfoSharingFlags = 0x10000,
    kFSCatInfoUserPrivs = 0x20000, kFSCatInfoUserAccess = 0x80000, kFSCatInfoSetOwnership = 0x100000,
};
#define NODE_LOCKED 0x0001
#define NODE_DIR    0x0010
#define NODE_HLINK  0x0100

#define SET(e) (cx_errno = (e))
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
char *getenv(const char *n);
const char *cx_path(const char *in, char *out, size_t cap);
int cx_follow_path(const char *in, char *out, size_t cap);
int cx_statx(const char *abs, int fd, mos_statx_t *st);
long fsgetpath(char *buf, size_t n, void *fsid, u64 objid);
long getxattr(const char *p, const char *n, void *v, size_t s, u32 pos, int o);
int setxattr(const char *p, const char *n, const void *v, size_t s, u32 pos, int o);
int removexattr(const char *p, const char *n, int o);
int chmod(const char *p, unsigned short m);
int chown(const char *p, unsigned u, unsigned g);
int chflags(const char *p, unsigned f);
int exchangedata(const char *a, const char *b, unsigned opt);
int open(const char *p, int flags, ...);
long read(int fd, void *b, size_t n);
long write(int fd, const void *b, size_t n);
long lseek(int fd, long off, int wh);
int ftruncate(int fd, long n);
int fsync(int fd);
int close(int fd);
int mkdir(const char *p, unsigned short m);
int rmdir(const char *p);
int unlink(const char *p);
int rename(const char *a, const char *b);
int geteuid(void);
#ifndef CX_HOST_TEST
char *strrchr(const char *s, int c);
#endif

#define O_RDONLY_ 0
#define O_WRONLY_ 1
#define O_RDWR_   2
#define O_CREAT_  0x200
#define O_EXCL_   0x800
#define XATTR_NOFOLLOW_ 1
#define XATTR_CREATE_   2
#define UF_IMMUTABLE_ 0x2
#define RSRC "com.apple.ResourceFork"
#define FINFO "com.apple.FinderInfo"
#define EPOCH_1904 2082844800LL   /* segundos entre 1/1/1904 e 1/1/1970 */

static OSStatus err_of(int e) {
    switch (e) {
    case 0: return ioErr;
    case 2: return fnfErr;           /* ENOENT */
    case 20: return dirNFErr;        /* ENOTDIR */
    case 17: return dupFNErr;        /* EEXIST */
    case 1: case 13: return afpAccessDenied;   /* EPERM, EACCES */
    case 30: return wPrErr;          /* EROFS */
    case 66: case 16: return fBsyErr;   /* ENOTEMPTY, EBUSY */
    case 28: return dskFulErr;       /* ENOSPC */
    case 63: return errFSNameTooLong;
    case 21: return notAFileErr;     /* EISDIR */
    case 22: return paramErr;
    case 12: return memFullErr;
    case 24: case 23: return tmfoErr;
    case 18: return diffVolErr;      /* EXDEV */
    default: return ioErr;
    }
}

/* ======================================================================== FSRef <-> caminho */
#define REF_MAGIC 0x664f536du   /* "mOSf" */
static void ref_fill(FSRef *r, const mos_statx_t *x) {
    memset(r, 0, sizeof *r);
    u32 m = REF_MAGIC, d = (u32)x->dev;
    memcpy(r->hidden, &m, 4);
    memcpy(r->hidden + 4, &d, 4);
    memcpy(r->hidden + 8, &x->ino, 8);
}
static int ref_ok(const FSRef *r) {
    u32 m;
    if (!r) return 0;
    memcpy(&m, r->hidden, 4);
    return m == REF_MAGIC;
}
static u64 ref_ino(const FSRef *r) { u64 i; memcpy(&i, r->hidden + 8, 8); return i; }
static u32 ref_dev(const FSRef *r) { u32 d; memcpy(&d, r->hidden + 4, 4); return d; }
/* caminho atual do objeto (segue renames); errFSBadFSRef se a referência não é nossa, fnfErr se sumiu */
static OSStatus ref_path(const FSRef *r, char *out, size_t cap) {
    if (!ref_ok(r)) return r ? errFSBadFSRef : paramErr;
    if (fsgetpath(out, cap, 0, ref_ino(r)) <= 0) return fnfErr;
    return noErr;
}
static OSStatus ref_of_path(const char *abs, FSRef *r, mos_statx_t *xo) {
    mos_statx_t x;
    if (cx_statx(abs, -1, &x) < 0) return err_of(cx_errno);
    if (r) ref_fill(r, &x);
    if (xo) *xo = x;
    return noErr;
}

/* nome UTF-16 do Carbon -> UTF-8 POSIX ("/" vira ":"); -1 se inválido */
static int uni_to_utf8(const UniChar *u, unsigned long n, char *out, size_t cap) {
    size_t k = 0;
    for (unsigned long i = 0; i < n; i++) {
        u32 c = u[i];
        if (c >= 0xd800 && c < 0xdc00 && i + 1 < n && u[i + 1] >= 0xdc00 && u[i + 1] < 0xe000) {
            c = 0x10000 + ((c - 0xd800) << 10) + (u[i + 1] - 0xdc00);
            i++;
        } else if (c >= 0xd800 && c < 0xe000) return -1;
        if (c == 0) return -1;
        if (c == '/') c = ':';
        else if (c == ':') c = '/';
        if (k + 4 >= cap) return -1;
        if (c < 0x80) out[k++] = (char)c;
        else if (c < 0x800) { out[k++] = (char)(0xc0 | c >> 6); out[k++] = (char)(0x80 | (c & 0x3f)); }
        else if (c < 0x10000) { out[k++] = (char)(0xe0 | c >> 12); out[k++] = (char)(0x80 | ((c >> 6) & 0x3f)); out[k++] = (char)(0x80 | (c & 0x3f)); }
        else { out[k++] = (char)(0xf0 | c >> 18); out[k++] = (char)(0x80 | ((c >> 12) & 0x3f)); out[k++] = (char)(0x80 | ((c >> 6) & 0x3f)); out[k++] = (char)(0x80 | (c & 0x3f)); }
    }
    out[k] = 0;
    return (int)k;
}
static void utf8_to_hfs(const char *s, HFSUniStr255 *o) {
    const u8 *p = (const u8 *)s;
    u16 n = 0;
    while (*p && n < 255) {
        u32 c = *p++;
        if (c >= 0xf0 && p[0] && p[1] && p[2]) { c = ((c & 7) << 18) | ((u32)(p[0] & 0x3f) << 12) | ((u32)(p[1] & 0x3f) << 6) | (p[2] & 0x3f); p += 3; }
        else if (c >= 0xe0 && p[0] && p[1]) { c = ((c & 0xf) << 12) | ((u32)(p[0] & 0x3f) << 6) | (p[1] & 0x3f); p += 2; }
        else if (c >= 0xc0 && p[0]) { c = ((c & 0x1f) << 6) | (p[0] & 0x3f); p += 1; }
        if (c == ':') c = '/';
        else if (c == '/') c = ':';
        if (c >= 0x10000) {
            if (n + 2 > 255) break;
            c -= 0x10000;
            o->unicode[n++] = (u16)(0xd800 + (c >> 10));
            o->unicode[n++] = (u16)(0xdc00 + (c & 0x3ff));
        } else o->unicode[n++] = (u16)c;
    }
    o->length = n;
}
static const char *base_name(const char *p) {
    const char *s = strrchr(p, '/');
    return s && s[1] ? s + 1 : p;
}
static int join(char *out, size_t cap, const char *dir, const char *name) {
    size_t a = strlen(dir);
    if (strlcpy(out, dir, cap) >= cap) return -1;
    if (!(a && dir[a - 1] == '/') && strlcat(out, "/", cap) >= cap) return -1;
    return strlcat(out, name, cap) >= cap ? -1 : 0;
}
static int parent_of(const char *p, char *out, size_t cap) {
    strlcpy(out, p, cap);
    char *s = strrchr(out, '/');
    if (!s) return -1;
    if (s == out) out[1] = 0;
    else *s = 0;
    return 0;
}

/* ======================================================================== caminho -> FSRef e volta */
#define kFSPathMakeRefDoNotFollowLeafSymlink 0x01
OSStatus FSPathMakeRefWithOptions(const u8 *path, u32 options, FSRef *ref, Boolean *isDirectory) {
    if (!path || !*path || !ref) return paramErr;
    char a[1024], b[1024];
    const char *p = cx_path((const char *)path, a, sizeof a);
    if (!p) return fnfErr;
    if (!(options & kFSPathMakeRefDoNotFollowLeafSymlink)) {
        if (cx_follow_path(p, b, sizeof b) < 0) return fnfErr;
        p = b;
    }
    mos_statx_t x;
    OSStatus s = ref_of_path(p, ref, &x);
    if (s == noErr && isDirectory) *isDirectory = x.kind == 1;
    return s;
}
OSStatus FSPathMakeRef(const u8 *path, FSRef *ref, Boolean *isDirectory) {
    return FSPathMakeRefWithOptions(path, 0, ref, isDirectory);
}
OSStatus FSRefMakePath(const FSRef *ref, u8 *path, u32 maxPathSize) {
    if (!path || !maxPathSize) return paramErr;
    char p[1024];
    OSStatus s = ref_path(ref, p, sizeof p);
    if (s) return s;
    if (strlen(p) + 1 > maxPathSize) return errFSNameTooLong;   /* como o macOS: buffer curto não trunca */
    memcpy(path, p, strlen(p) + 1);
    return noErr;
}
Boolean FSIsFSRefValid(const FSRef *ref) {
    char p[1024];
    return ref_path(ref, p, sizeof p) == noErr;
}
OSErr FSCompareFSRefs(const FSRef *a, const FSRef *b) {
    char p[1024];
    if (ref_path(a, p, sizeof p) || ref_path(b, p, sizeof p)) return errFSBadFSRef;
    if (ref_dev(a) != ref_dev(b)) return diffVolErr;
    return ref_ino(a) == ref_ino(b) ? noErr : errFSRefsDifferent;
}

/* ======================================================================== catálogo */
static UTCDateTime utc(i64 ns) {
    UTCDateTime t;
    i64 s = ns / 1000000000LL + EPOCH_1904;
    i64 frac = ns % 1000000000LL;
    if (s < 0) s = 0;
    t.highSeconds = (u16)((u64)s >> 32);
    t.lowSeconds = (u32)s;
    t.fraction = (u16)((u64)frac * 65536ull / 1000000000ull);
    return t;
}
static i64 utc_ns(const UTCDateTime *t) {
    i64 s = (i64)(((u64)t->highSeconds << 32) | t->lowSeconds) - EPOCH_1904;
    return s * 1000000000LL + (i64)((u64)t->fraction * 1000000000ull / 65536ull);
}
static u32 eff_access(const char *p) {
    u32 r = 0;
    for (u32 b = 4; b; b >>= 1) {
        mos_fsmeta_t rq = {0};
        rq.fd = -1; rq.path = (unsigned long long)p; rq.a = b; rq.opts = 1;
        if (SYS2(SYS_FSMETA, MOS_FSM_ACCESS, &rq) == 0) r |= b;
    }
    return r;
}
static u32 dir_count(const char *p) {
    mos_fsm_dirent_t de;
    u32 n = 0;
    for (u64 i = 0;; i++) {
        mos_fsmeta_t rq = {0};
        rq.fd = -1; rq.path = (unsigned long long)p; rq.pos = i;
        rq.buf = (unsigned long long)&de; rq.size = sizeof de;
        if (SYS2(SYS_FSMETA, MOS_FSM_READDIR, &rq) != 1) break;
        n++;
    }
    return n;
}
static int vol_of(const char *p, mos_statfs_t *sf) {
    mos_fsmeta_t rq = {0};
    rq.fd = -1; rq.path = (unsigned long long)p; rq.buf = (unsigned long long)sf; rq.size = sizeof *sf;
    return SYS2(SYS_FSMETA, MOS_FSM_STATFS, &rq) < 0 ? -1 : 0;
}
/* FSVolumeRefNum: o volume de boot é -100, os outros -101, -102... pela ordem do fsid */
static s16_t vol_refnum(u32 fsid) {
    static u32 root_fsid = 0xffffffffu;
    if (root_fsid == 0xffffffffu) {
        mos_statx_t r;
        root_fsid = cx_statx("/", -1, &r) == 0 ? (u32)r.dev : 0;
    }
    return (s16_t)(-100 - (i32)((fsid - root_fsid) & 0x3fff));
}
static void fill_info(const char *p, const mos_statx_t *x, u32 which, FSCatalogInfo *ci) {
    memset(ci, 0, sizeof *ci);
    int dir = x->kind == 1;
    if (which & kFSCatInfoNodeFlags) {
        ci->nodeFlags = (u16)((dir ? NODE_DIR : 0) | ((x->flags & (UF_IMMUTABLE_ | 0x20000)) ? NODE_LOCKED : 0) |
                              (!dir && x->nlink > 1 ? NODE_HLINK : 0));
    }
    if (which & kFSCatInfoVolume) ci->volume = vol_refnum((u32)x->dev);
    if (which & kFSCatInfoParentDirID) ci->parentDirID = strcmp(p, "/") ? (x->parent_ino ? x->parent_ino : 2) : 1;
    if (which & kFSCatInfoNodeID) ci->nodeID = strcmp(p, "/") ? (u32)x->ino : 2;
    if (which & kFSCatInfoCreateDate) ci->createDate = utc(x->btime_ns);
    if (which & kFSCatInfoContentMod) ci->contentModDate = utc(x->mtime_ns);
    if (which & kFSCatInfoAttrMod) ci->attributeModDate = utc(x->ctime_ns);
    if (which & kFSCatInfoAccessDate) ci->accessDate = utc(x->atime_ns);
    u32 acc = (which & (kFSCatInfoPermissions | kFSCatInfoUserAccess | kFSCatInfoUserPrivs)) ? eff_access(p) : 0;
    if (which & (kFSCatInfoPermissions | kFSCatInfoUserAccess)) {
        ci->permissions.userID = x->uid;
        ci->permissions.groupID = x->gid;
        ci->permissions.mode = (u16)((x->mode & 07777) | (dir ? 0040000 : x->link ? 0120000 : 0100000));
        ci->permissions.userAccess = (u8)acc;
    }
    if (which & (kFSCatInfoFinderInfo | kFSCatInfoFinderXInfo)) {
        u8 fi[32] = {0};
        getxattr(p, FINFO, fi, 32, 0, XATTR_NOFOLLOW_);
        if (which & kFSCatInfoFinderInfo) memcpy(ci->finderInfo, fi, 16);
        if (which & kFSCatInfoFinderXInfo) memcpy(ci->extFinderInfo, fi + 16, 16);
    }
    if (which & kFSCatInfoValence) ci->valence = dir ? dir_count(p) : 0;
    if ((which & kFSCatInfoDataSizes) && !dir) {
        ci->dataLogicalSize = x->size;
        ci->dataPhysicalSize = x->blocks ? x->blocks * 512 : (x->size + 4095) & ~4095ull;
    }
    if ((which & kFSCatInfoRsrcSizes) && !dir) {
        long n = getxattr(p, RSRC, 0, 0, 0, XATTR_NOFOLLOW_);
        if (n > 0) { ci->rsrcLogicalSize = (u64)n; ci->rsrcPhysicalSize = ((u64)n + 4095) & ~4095ull; }
    }
    if (which & kFSCatInfoUserPrivs) {
        /* bits do AFP: 0x02 não vê pastas, 0x04 não vê arquivos, 0x08 não pode mudar, 0x80 não é o dono */
        u8 up = 0;
        if (dir && !(acc & 4)) up |= 0x06;
        if (!(acc & 2)) up |= 0x08;
        if (x->uid != (unsigned)geteuid()) up |= 0x80;
        ci->userPrivileges = up;
    }
}
static void name_of(const char *p, HFSUniStr255 *out) {
    mos_statfs_t sf;
    if (vol_of(p, &sf) == 0 && !strcmp(sf.mnton, p) && sf.volname[0]) utf8_to_hfs(sf.volname, out);   /* raiz: nome do volume */
    else utf8_to_hfs(base_name(p), out);
}
OSErr FSGetCatalogInfo(const FSRef *ref, u32 whichInfo, FSCatalogInfo *catalogInfo, HFSUniStr255 *outName,
                       void *fsSpec, FSRef *parentRef) {
    char p[1024];
    OSStatus s = ref_path(ref, p, sizeof p);
    if (s) return (OSErr)s;
    mos_statx_t x;
    if (cx_statx(p, -1, &x) < 0) return (OSErr)err_of(cx_errno);
    if (catalogInfo) fill_info(p, &x, whichInfo, catalogInfo);
    if (outName) name_of(p, outName);
    if (fsSpec) memset(fsSpec, 0, 70);   /* FSSpec não existe no x86_64 moderno: devolvido zerado */
    if (parentRef) {
        char par[1024];
        memset(parentRef, 0, sizeof *parentRef);
        if (strcmp(p, "/") && parent_of(p, par, sizeof par) == 0) ref_of_path(par, parentRef, 0);   /* raiz: sem pai */
    }
    return noErr;
}
static OSStatus set_info(const char *p, u32 which, const FSCatalogInfo *ci) {
    mos_statx_t x;
    if (cx_statx(p, -1, &x) < 0) return err_of(cx_errno);
    /* trava primeiro saindo (destrava antes de mexer no resto) e por último entrando */
    int lock = (which & kFSCatInfoNodeFlags) ? (ci->nodeFlags & NODE_LOCKED) != 0 : -1;
    if (lock == 0 && (x.flags & UF_IMMUTABLE_) && chflags(p, x.flags & ~(unsigned)UF_IMMUTABLE_) < 0) return err_of(cx_errno);
    long long t[4] = {0};
    u32 mask = 0;
    if (which & kFSCatInfoAccessDate) { t[0] = utc_ns(&ci->accessDate); mask |= MOS_FSM_ATIME; }
    if (which & kFSCatInfoContentMod) { t[1] = utc_ns(&ci->contentModDate); mask |= MOS_FSM_MTIME; }
    if (which & kFSCatInfoCreateDate) { t[3] = utc_ns(&ci->createDate); mask |= MOS_FSM_BTIME; }
    if (mask) {
        mos_fsmeta_t rq = {0};
        rq.fd = -1; rq.path = (unsigned long long)p; rq.a = mask;
        memcpy(rq.t, t, sizeof t);
        long r = SYS2(SYS_FSMETA, MOS_FSM_SETTIMES, &rq);
        if (r < 0) return err_of((int)-r);
    }
    if (which & (kFSCatInfoFinderInfo | kFSCatInfoFinderXInfo)) {
        u8 fi[32] = {0};
        getxattr(p, FINFO, fi, 32, 0, XATTR_NOFOLLOW_);
        if (which & kFSCatInfoFinderInfo) memcpy(fi, ci->finderInfo, 16);
        if (which & kFSCatInfoFinderXInfo) memcpy(fi + 16, ci->extFinderInfo, 16);
        static const u8 zero[32];
        if (!memcmp(fi, zero, 32)) removexattr(p, FINFO, XATTR_NOFOLLOW_);
        else if (setxattr(p, FINFO, fi, 32, 0, XATTR_NOFOLLOW_) < 0) return err_of(cx_errno);
    }
    if (which & kFSCatInfoPermissions) {
        if ((which & kFSCatInfoSetOwnership) || ci->permissions.userID != x.uid || ci->permissions.groupID != x.gid) {
            if (chown(p, ci->permissions.userID, ci->permissions.groupID) < 0) return err_of(cx_errno);
        }
        if (chmod(p, (unsigned short)(ci->permissions.mode & 07777)) < 0) return err_of(cx_errno);
    }
    if (lock == 1 && !(x.flags & UF_IMMUTABLE_)) {
        if (cx_statx(p, -1, &x) < 0 || chflags(p, x.flags | UF_IMMUTABLE_) < 0) return err_of(cx_errno);
    }
    return noErr;
}
OSErr FSSetCatalogInfo(const FSRef *ref, u32 whichInfo, const FSCatalogInfo *catalogInfo) {
    if (!catalogInfo) return paramErr;
    /* só estes campos são graváveis; os outros fazem o macOS devolver errFSBadInfoBitmap */
    const u32 ok = kFSCatInfoNodeFlags | kFSCatInfoCreateDate | kFSCatInfoContentMod | kFSCatInfoAttrMod |
                   kFSCatInfoAccessDate | kFSCatInfoBackupDate | kFSCatInfoPermissions | kFSCatInfoFinderInfo |
                   kFSCatInfoFinderXInfo | kFSCatInfoTextEncoding | kFSCatInfoSetOwnership;
    if (whichInfo & ~ok) return errFSBadInfoBitmap;
    char p[1024];
    OSStatus s = ref_path(ref, p, sizeof p);
    return (OSErr)(s ? s : set_info(p, whichInfo, catalogInfo));
}

/* ======================================================================== criar, apagar, renomear, mover */
static OSStatus child_path(const FSRef *parent, UniCharCount n, const UniChar *name, char *out, size_t cap, int need_dir) {
    char dir[1024], nm[768];
    if (!name && n) return paramErr;
    if (n == 0) return errFSMissingName;
    if (n > 255) return errFSNameTooLong;
    OSStatus s = ref_path(parent, dir, sizeof dir);
    if (s) return s;
    if (need_dir) {
        mos_statx_t x;
        if (cx_statx(dir, -1, &x) < 0) return err_of(cx_errno);
        if (x.kind != 1) return errFSNotAFolder;
    }
    if (uni_to_utf8(name, n, nm, sizeof nm) <= 0) return bdNamErr;
    if (!strcmp(nm, ".") || !strcmp(nm, "..")) return bdNamErr;
    return join(out, cap, dir, nm) ? errFSNameTooLong : noErr;
}
OSErr FSMakeFSRefUnicode(const FSRef *parentRef, UniCharCount nameLength, const UniChar *name, u32 textEncodingHint,
                         FSRef *newRef) {
    (void)textEncodingHint;
    char p[1024];
    OSStatus s = child_path(parentRef, nameLength, name, p, sizeof p, 1);
    if (s) return (OSErr)s;
    return (OSErr)ref_of_path(p, newRef, 0);
}
static OSStatus after_create(const char *p, u32 whichInfo, const FSCatalogInfo *ci, FSRef *newRef) {
    OSStatus s = noErr;
    if (ci && whichInfo) {
        if (whichInfo & ~(u32)(kFSCatInfoNodeFlags | kFSCatInfoCreateDate | kFSCatInfoContentMod | kFSCatInfoAttrMod |
                               kFSCatInfoAccessDate | kFSCatInfoBackupDate | kFSCatInfoPermissions |
                               kFSCatInfoFinderInfo | kFSCatInfoFinderXInfo | kFSCatInfoTextEncoding |
                               kFSCatInfoSetOwnership)) return errFSBadInfoBitmap;
        s = set_info(p, whichInfo, ci);
    }
    if (s == noErr && newRef) s = ref_of_path(p, newRef, 0);
    return s;
}
OSErr FSCreateFileUnicode(const FSRef *parentRef, UniCharCount nameLength, const UniChar *name, u32 whichInfo,
                          const FSCatalogInfo *catalogInfo, FSRef *newRef, void *newSpec) {
    char p[1024];
    OSStatus s = child_path(parentRef, nameLength, name, p, sizeof p, 1);
    if (s) return (OSErr)s;
    int fd = open(p, O_WRONLY_ | O_CREAT_ | O_EXCL_, 0644);
    if (fd < 0) return (OSErr)err_of(cx_errno);
    close(fd);
    if (newSpec) memset(newSpec, 0, 70);
    return (OSErr)after_create(p, whichInfo, catalogInfo, newRef);
}
OSErr FSCreateDirectoryUnicode(const FSRef *parentRef, UniCharCount nameLength, const UniChar *name, u32 whichInfo,
                               const FSCatalogInfo *catalogInfo, FSRef *newRef, void *newSpec, u32 *newDirID) {
    char p[1024];
    OSStatus s = child_path(parentRef, nameLength, name, p, sizeof p, 1);
    if (s) return (OSErr)s;
    if (mkdir(p, 0755) < 0) return (OSErr)err_of(cx_errno);
    if (newSpec) memset(newSpec, 0, 70);
    FSRef r;
    s = after_create(p, whichInfo, catalogInfo, &r);
    if (s) return (OSErr)s;
    if (newRef) *newRef = r;
    if (newDirID) *newDirID = (u32)ref_ino(&r);
    return noErr;
}
OSErr FSDeleteObject(const FSRef *ref) {
    char p[1024];
    OSStatus s = ref_path(ref, p, sizeof p);
    if (s) return (OSErr)s;
    mos_statx_t x;
    if (cx_statx(p, -1, &x) < 0) return (OSErr)err_of(cx_errno);
    if (x.flags & (UF_IMMUTABLE_ | 0x20000)) return fLckdErr;   /* arquivo travado no Finder */
    if (x.kind == 1 && !x.link) return (OSErr)(rmdir(p) < 0 ? err_of(cx_errno) : noErr);
    return (OSErr)(unlink(p) < 0 ? err_of(cx_errno) : noErr);
}
OSErr FSRenameUnicode(const FSRef *ref, UniCharCount nameLength, const UniChar *name, u32 textEncodingHint,
                      FSRef *newRef) {
    (void)textEncodingHint;
    char p[1024], par[1024], np[1024], nm[768];
    OSStatus s = ref_path(ref, p, sizeof p);
    if (s) return (OSErr)s;
    if (!strcmp(p, "/")) return paramErr;
    if (!nameLength) return errFSMissingName;
    if (nameLength > 255) return errFSNameTooLong;
    if (!name || uni_to_utf8(name, nameLength, nm, sizeof nm) <= 0 || !strcmp(nm, ".") || !strcmp(nm, "..")) return bdNamErr;
    parent_of(p, par, sizeof par);
    if (join(np, sizeof np, par, nm)) return errFSNameTooLong;
    if (strcmp(np, p)) {
        mos_statx_t a, b;
        /* outro objeto com o nome: dupFNErr (só a mudança de maiúsculas do mesmo objeto passa) */
        if (cx_statx(np, -1, &b) == 0 && (cx_statx(p, -1, &a) < 0 || a.ino != b.ino)) return dupFNErr;
        if (rename(p, np) < 0) return (OSErr)err_of(cx_errno);
    }
    return (OSErr)(newRef ? ref_of_path(np, newRef, 0) : noErr);
}
OSErr FSMoveObject(const FSRef *ref, const FSRef *destDirectory, FSRef *newRef) {
    char p[1024], d[1024], np[1024];
    OSStatus s = ref_path(ref, p, sizeof p);
    if (s) return (OSErr)s;
    if ((s = ref_path(destDirectory, d, sizeof d))) return (OSErr)s;
    mos_statx_t x, dx;
    if (cx_statx(p, -1, &x) < 0 || cx_statx(d, -1, &dx) < 0) return (OSErr)err_of(cx_errno);
    if (dx.kind != 1) return errFSNotAFolder;
    if (x.dev != dx.dev) return diffVolErr;
    size_t pl = strlen(p);
    if (!strncmp(d, p, pl) && (d[pl] == '/' || !d[pl])) return paramErr;   /* para dentro de si mesma */
    if (join(np, sizeof np, d, base_name(p))) return errFSNameTooLong;
    if (strcmp(np, p)) {
        if (cx_statx(np, -1, &dx) == 0) return dupFNErr;
        if (rename(p, np) < 0) return (OSErr)err_of(cx_errno);
    }
    return (OSErr)(newRef ? ref_of_path(np, newRef, 0) : noErr);
}
OSErr FSExchangeObjects(const FSRef *a, const FSRef *b) {
    char pa[1024], pb[1024];
    OSStatus s = ref_path(a, pa, sizeof pa);
    if (s || (s = ref_path(b, pb, sizeof pb))) return (OSErr)s;
    return (OSErr)(exchangedata(pa, pb, 0) < 0 ? err_of(cx_errno) : noErr);
}

/* ======================================================================== forks */
static const UniChar rsrc_name[] = {'R', 'E', 'S', 'O', 'U', 'R', 'C', 'E', '_', 'F', 'O', 'R', 'K'};
OSErr FSGetDataForkName(HFSUniStr255 *dataForkName) {
    if (!dataForkName) return paramErr;
    dataForkName->length = 0;
    return noErr;
}
OSErr FSGetResourceForkName(HFSUniStr255 *resourceForkName) {
    if (!resourceForkName) return paramErr;
    resourceForkName->length = 13;
    memcpy(resourceForkName->unicode, rsrc_name, sizeof rsrc_name);
    return noErr;
}
/* 0 dados, 1 recurso, -1 nome desconhecido */
static int fork_kind(UniCharCount n, const UniChar *name) {
    if (n == 0) return 0;
    if (n == 13 && name && !memcmp(name, rsrc_name, sizeof rsrc_name)) return 1;
    return -1;
}
enum { fsAtMark = 0, fsFromStart = 1, fsFromLEOF = 2, fsFromMark = 3 };
enum { fsCurPerm = 0, fsRdPerm = 1, fsWrPerm = 2, fsRdWrPerm = 3, fsRdWrShPerm = 4 };
#define NFORK 64
#define FORK_BASE 3000
typedef struct {
    int used, rsrc, wr, fd, dirty;
    u64 ino;
    i64 pos;
    u8 *buf;            /* resource fork: o xattr inteiro em memória, gravado no flush/close */
    i64 n, cap;
} fork_t;
static fork_t forks[NFORK];
static fork_t *fork_get(FSIORefNum r) {
    int i = r - FORK_BASE;
    return i >= 0 && i < NFORK && forks[i].used ? &forks[i] : 0;
}
static OSStatus rsrc_flush(fork_t *f) {
    if (!f->rsrc || !f->dirty) return noErr;
    char p[1024];
    if (fsgetpath(p, sizeof p, 0, f->ino) <= 0) return fnfErr;
    if (setxattr(p, RSRC, f->buf ? f->buf : (const u8 *)"", (size_t)f->n, 0, XATTR_NOFOLLOW_) < 0) return err_of(cx_errno);
    f->dirty = 0;
    return noErr;
}
static i64 fork_size(fork_t *f) {
    if (f->rsrc) return f->n;
    mos_statx_t x;
    return cx_statx(0, f->fd, &x) < 0 ? -1 : (i64)x.size;
}
static int rsrc_reserve(fork_t *f, i64 n) {
    if (n <= f->cap) return 0;
    i64 nc = f->cap ? f->cap : 4096;
    while (nc < n) nc *= 2;
    u8 *nb = malloc((size_t)nc);
    if (!nb) return -1;
    if (f->buf) { memcpy(nb, f->buf, (size_t)f->n); free(f->buf); }
    f->buf = nb;
    f->cap = nc;
    return 0;
}
OSErr FSOpenFork(const FSRef *ref, UniCharCount forkNameLength, const UniChar *forkName, s8_t permissions,
                 FSIORefNum *forkRefNum) {
    if (!forkRefNum) return paramErr;
    int kind = fork_kind(forkNameLength, forkName);
    if (kind < 0) return errFSBadForkName;
    char p[1024];
    OSStatus s = ref_path(ref, p, sizeof p);
    if (s) return (OSErr)s;
    mos_statx_t x;
    if (cx_statx(p, -1, &x) < 0) return (OSErr)err_of(cx_errno);
    if (x.kind == 1) return notAFileErr;
    int wr = permissions == fsWrPerm || permissions == fsRdWrPerm || permissions == fsRdWrShPerm ||
             (permissions == fsCurPerm && (eff_access(p) & 2));
    if (wr && (x.flags & (UF_IMMUTABLE_ | 0x20000))) {   /* travado: fsCurPerm abre só para leitura */
        if (permissions != fsCurPerm) return fLckdErr;
        wr = 0;
    }
    int i = 0;
    while (i < NFORK && forks[i].used) i++;
    if (i == NFORK) return tmfoErr;
    fork_t *f = &forks[i];
    memset(f, 0, sizeof *f);
    f->rsrc = kind;
    f->wr = wr;
    f->ino = x.ino;
    f->fd = -1;
    if (kind == 0) {
        f->fd = open(p, wr ? O_RDWR_ : O_RDONLY_);
        if (f->fd < 0) {
            if (permissions != fsCurPerm || !wr) return (OSErr)err_of(cx_errno);
            f->wr = 0;
            if ((f->fd = open(p, O_RDONLY_)) < 0) return (OSErr)err_of(cx_errno);
        }
    } else {
        long n = getxattr(p, RSRC, 0, 0, 0, XATTR_NOFOLLOW_);
        if (n < 0 && cx_errno != 93) return (OSErr)err_of(cx_errno);
        if (n < 0 && !wr) return errFSForkNotFound;   /* sem resource fork e só leitura */
        if (n > 0) {
            if (rsrc_reserve(f, n) < 0) return memFullErr;
            f->n = getxattr(p, RSRC, f->buf, (size_t)n, 0, XATTR_NOFOLLOW_);
            if (f->n < 0) { free(f->buf); return (OSErr)err_of(cx_errno); }
        }
    }
    f->used = 1;
    *forkRefNum = FORK_BASE + i;
    return noErr;
}
OSErr FSOpenForkReadOnly(const FSRef *ref, UniCharCount n, const UniChar *name, FSIORefNum *forkRefNum) {
    return FSOpenFork(ref, n, name, fsRdPerm, forkRefNum);
}
static OSStatus seek_to(fork_t *f, u16 mode, i64 off, i64 *out) {
    i64 base;
    switch (mode & 3) {
    case fsAtMark: *out = f->pos; return noErr;
    case fsFromStart: base = 0; break;
    case fsFromLEOF: base = fork_size(f); if (base < 0) return ioErr; break;
    default: base = f->pos; break;
    }
    if (base + off < 0) return posErr;
    *out = base + off;
    return noErr;
}
OSErr FSReadFork(FSIORefNum forkRefNum, u16 positionMode, i64 positionOffset, ByteCount requestCount, void *buffer,
                 ByteCount *actualCount) {
    fork_t *f = fork_get(forkRefNum);
    if (actualCount) *actualCount = 0;
    if (!f) return rfNumErr;
    if (!buffer && requestCount) return paramErr;
    i64 at;
    OSStatus s = seek_to(f, positionMode, positionOffset, &at);
    if (s) return (OSErr)s;
    i64 size = fork_size(f);
    if (size < 0) return ioErr;
    ByteCount got = 0;
    if (at < size) {
        ByteCount want = requestCount;
        if ((i64)want > size - at) want = (ByteCount)(size - at);
        if (f->rsrc) { memcpy(buffer, f->buf + at, want); got = want; }
        else {
            if (lseek(f->fd, at, 0) < 0) return ioErr;
            while (got < want) {
                long k = read(f->fd, (u8 *)buffer + got, want - got);
                if (k < 0) return (OSErr)err_of(cx_errno);
                if (k == 0) break;
                got += (ByteCount)k;
            }
        }
    }
    f->pos = at + (i64)got;
    if (actualCount) *actualCount = got;
    return got < requestCount ? eofErr : noErr;
}
OSErr FSWriteFork(FSIORefNum forkRefNum, u16 positionMode, i64 positionOffset, ByteCount requestCount,
                  const void *buffer, ByteCount *actualCount) {
    fork_t *f = fork_get(forkRefNum);
    if (actualCount) *actualCount = 0;
    if (!f) return rfNumErr;
    if (!f->wr) return wrPermErr;
    if (!buffer && requestCount) return paramErr;
    i64 at;
    OSStatus s = seek_to(f, positionMode, positionOffset, &at);
    if (s) return (OSErr)s;
    ByteCount put = 0;
    if (f->rsrc) {
        if (rsrc_reserve(f, at + (i64)requestCount) < 0) return memFullErr;
        if (at > f->n) memset(f->buf + f->n, 0, (size_t)(at - f->n));
        memcpy(f->buf + at, buffer, requestCount);
        if (at + (i64)requestCount > f->n) f->n = at + (i64)requestCount;
        f->dirty = 1;
        put = requestCount;
    } else {
        if (lseek(f->fd, at, 0) < 0) return ioErr;
        while (put < requestCount) {
            long k = write(f->fd, (const u8 *)buffer + put, requestCount - put);
            if (k <= 0) { f->pos = at + (i64)put; if (actualCount) *actualCount = put; return (OSErr)err_of(cx_errno); }
            put += (ByteCount)k;
        }
    }
    f->pos = at + (i64)put;
    if (actualCount) *actualCount = put;
    return noErr;
}
OSErr FSGetForkPosition(FSIORefNum forkRefNum, i64 *position) {
    fork_t *f = fork_get(forkRefNum);
    if (!f) return rfNumErr;
    if (!position) return paramErr;
    *position = f->pos;
    return noErr;
}
OSErr FSSetForkPosition(FSIORefNum forkRefNum, u16 positionMode, i64 positionOffset) {
    fork_t *f = fork_get(forkRefNum);
    if (!f) return rfNumErr;
    i64 at;
    OSStatus s = seek_to(f, positionMode, positionOffset, &at);
    if (s) return (OSErr)s;
    i64 size = fork_size(f);
    if (at > size) return posErr;   /* a marca não passa do fim do fork */
    f->pos = at;
    return noErr;
}
OSErr FSGetForkSize(FSIORefNum forkRefNum, i64 *forkSize) {
    fork_t *f = fork_get(forkRefNum);
    if (!f) return rfNumErr;
    if (!forkSize) return paramErr;
    i64 n = fork_size(f);
    if (n < 0) return ioErr;
    *forkSize = n;
    return noErr;
}
OSErr FSSetForkSize(FSIORefNum forkRefNum, u16 positionMode, i64 positionOffset) {
    fork_t *f = fork_get(forkRefNum);
    if (!f) return rfNumErr;
    if (!f->wr) return wrPermErr;
    i64 at;
    OSStatus s = seek_to(f, positionMode, positionOffset, &at);
    if (s) return (OSErr)s;
    if (f->rsrc) {
        if (rsrc_reserve(f, at) < 0) return memFullErr;
        if (at > f->n) memset(f->buf + f->n, 0, (size_t)(at - f->n));
        f->n = at;
        f->dirty = 1;
    } else if (ftruncate(f->fd, at) < 0) return (OSErr)err_of(cx_errno);
    if (f->pos > at) f->pos = at;
    return noErr;
}
OSErr FSAllocateFork(FSIORefNum forkRefNum, u16 flags, u16 positionMode, i64 positionOffset, u64 requestCount,
                     u64 *actualCount) {
    (void)flags; (void)positionMode; (void)positionOffset;
    fork_t *f = fork_get(forkRefNum);
    if (!f) return rfNumErr;
    if (!f->wr) return wrPermErr;
    if (actualCount) *actualCount = requestCount;   /* o VFS aloca na gravação: a reserva sempre "cabe" */
    return noErr;
}
OSErr FSFlushFork(FSIORefNum forkRefNum) {
    fork_t *f = fork_get(forkRefNum);
    if (!f) return rfNumErr;
    if (f->rsrc) return (OSErr)rsrc_flush(f);
    if (f->wr) fsync(f->fd);
    return noErr;
}
OSErr FSCloseFork(FSIORefNum forkRefNum) {
    fork_t *f = fork_get(forkRefNum);
    if (!f) return rfNumErr;
    OSStatus s = rsrc_flush(f);
    if (f->fd >= 0) close(f->fd);
    free(f->buf);
    memset(f, 0, sizeof *f);
    return (OSErr)s;
}
OSErr FSCreateFork(const FSRef *ref, UniCharCount forkNameLength, const UniChar *forkName) {
    int kind = fork_kind(forkNameLength, forkName);
    if (kind < 0) return errFSBadForkName;
    char p[1024];
    OSStatus s = ref_path(ref, p, sizeof p);
    if (s) return (OSErr)s;
    if (kind == 0) return errFSForkExists;   /* todo arquivo tem fork de dados */
    if (setxattr(p, RSRC, "", 0, 0, XATTR_NOFOLLOW_ | XATTR_CREATE_) < 0) return (OSErr)(cx_errno == 17 ? errFSForkExists : err_of(cx_errno));
    return noErr;
}
OSErr FSDeleteFork(const FSRef *ref, UniCharCount forkNameLength, const UniChar *forkName) {
    int kind = fork_kind(forkNameLength, forkName);
    if (kind < 0) return errFSBadForkName;
    char p[1024];
    OSStatus s = ref_path(ref, p, sizeof p);
    if (s) return (OSErr)s;
    if (kind == 0) {
        int fd = open(p, O_WRONLY_);
        if (fd < 0) return (OSErr)err_of(cx_errno);
        int r = ftruncate(fd, 0);
        close(fd);
        return (OSErr)(r < 0 ? err_of(cx_errno) : noErr);
    }
    if (removexattr(p, RSRC, XATTR_NOFOLLOW_) < 0) return (OSErr)(cx_errno == 93 ? errFSForkNotFound : err_of(cx_errno));
    return noErr;
}
/* CatPositionRec { SInt32 initialize; SInt16 data[6]; }: initialize 0 começa; data[0] guarda o próximo índice */
typedef struct { i32 initialize; s16_t data[6]; } CatPositionRec;
OSErr FSIterateForks(const FSRef *ref, CatPositionRec *forkIterator, HFSUniStr255 *forkName, i64 *forkSize,
                     u64 *forkPhysicalSize) {
    if (!forkIterator) return paramErr;
    char p[1024];
    OSStatus s = ref_path(ref, p, sizeof p);
    if (s) return (OSErr)s;
    mos_statx_t x;
    if (cx_statx(p, -1, &x) < 0) return (OSErr)err_of(cx_errno);
    if (forkIterator->initialize == 0) { forkIterator->initialize = 1; forkIterator->data[0] = 0; }
    for (;;) {
        int i = forkIterator->data[0]++;
        if (i >= 2 || x.kind == 1) return errFSNoMoreItems;
        i64 n;
        if (i == 0) n = (i64)x.size;
        else { n = getxattr(p, RSRC, 0, 0, 0, XATTR_NOFOLLOW_); if (n < 0) continue; }
        if (forkName) { if (i == 0) FSGetDataForkName(forkName); else FSGetResourceForkName(forkName); }
        if (forkSize) *forkSize = n;
        if (forkPhysicalSize) *forkPhysicalSize = ((u64)n + 4095) & ~4095ull;
        return noErr;
    }
}

/* ======================================================================== iteradores */
#define kFSIterateFlat 0
#define kFSIterateSubtree 1
#define kFSIterateReserved 0xfffffffeu
typedef struct it_dir { struct it_dir *up; char *path; u64 pos; } it_dir_t;
typedef struct { u32 magic, flags; it_dir_t *top; } fsiter_t;
#define IT_MAGIC 0x69745346u
static it_dir_t *it_push(it_dir_t *up, const char *path) {
    it_dir_t *d = calloc(1, sizeof *d);
    if (!d) return 0;
    size_t n = strlen(path) + 1;
    d->path = malloc(n);
    if (!d->path) { free(d); return 0; }
    memcpy(d->path, path, n);
    d->up = up;
    return d;
}
static void it_pop(fsiter_t *it) {
    it_dir_t *d = it->top;
    it->top = d->up;
    free(d->path);
    free(d);
}
OSErr FSOpenIterator(const FSRef *container, u32 iteratorFlags, void **iterator) {
    if (!iterator) return paramErr;
    *iterator = 0;
    if (iteratorFlags & kFSIterateReserved) return errFSBadIteratorFlags;
    char p[1024];
    OSStatus s = ref_path(container, p, sizeof p);
    if (s) return (OSErr)s;
    mos_statx_t x;
    if (cx_statx(p, -1, &x) < 0) return (OSErr)err_of(cx_errno);
    if (x.kind != 1) return errFSNotAFolder;
    fsiter_t *it = calloc(1, sizeof *it);
    if (!it) return memFullErr;
    it->magic = IT_MAGIC;
    it->flags = iteratorFlags;
    if (!(it->top = it_push(0, p))) { free(it); return memFullErr; }
    *iterator = it;
    return noErr;
}
OSErr FSCloseIterator(void *iterator) {
    fsiter_t *it = iterator;
    if (!it || it->magic != IT_MAGIC) return errFSIteratorNotFound;
    while (it->top) it_pop(it);
    it->magic = 0;
    free(it);
    return noErr;
}
/* próximo objeto (caminho absoluto); 0 no fim */
static int it_next(fsiter_t *it, char *out, size_t cap, mos_statx_t *x) {
    while (it->top) {
        it_dir_t *d = it->top;
        mos_fsm_dirent_t de;
        mos_fsmeta_t rq = {0};
        rq.fd = -1; rq.path = (unsigned long long)d->path; rq.pos = d->pos;
        rq.buf = (unsigned long long)&de; rq.size = sizeof de;
        if (SYS2(SYS_FSMETA, MOS_FSM_READDIR, &rq) != 1) { it_pop(it); continue; }
        d->pos++;
        if (join(out, cap, d->path, de.name) || cx_statx(out, -1, x) < 0) continue;
        if ((it->flags & kFSIterateSubtree) && x->kind == 1 && !x->link) {
            it_dir_t *c = it_push(it->top, out);
            if (c) it->top = c;
        }
        return 1;
    }
    return 0;
}
OSErr FSGetCatalogInfoBulk(void *iterator, ItemCount maximumObjects, ItemCount *actualObjects,
                           Boolean *containerChanged, u32 whichInfo, FSCatalogInfo *catalogInfos, FSRef *refs,
                           void *specs, HFSUniStr255 *names) {
    fsiter_t *it = iterator;
    if (actualObjects) *actualObjects = 0;
    if (containerChanged) *containerChanged = 0;
    if (!it || it->magic != IT_MAGIC) return errFSIteratorNotFound;
    if (!maximumObjects || !actualObjects) return errFSBadItemCount;
    ItemCount k = 0;
    char p[1024];
    mos_statx_t x;
    while (k < maximumObjects && it_next(it, p, sizeof p, &x)) {
        if (catalogInfos) fill_info(p, &x, whichInfo, &catalogInfos[k]);
        if (refs) ref_fill(&refs[k], &x);
        if (specs) memset((u8 *)specs + k * 70, 0, 70);
        if (names) utf8_to_hfs(base_name(p), &names[k]);
        k++;
    }
    *actualObjects = k;
    return k < maximumObjects ? errFSNoMoreItems : noErr;
}

/* ======================================================================== Folder Manager */
#define kOnSystemDisk -32768
#define kOnAppropriateDisk -32767
#define kSystemDomain -32766
#define kLocalDomain -32765
#define kNetworkDomain -32764
#define kUserDomain -32763
#define FOUR(a, b, c, d) (((u32)(u8)(a) << 24) | ((u32)(u8)(b) << 16) | ((u32)(u8)(c) << 8) | (u32)(u8)(d))
/* pasta por tipo: 'u' só no usuário, 'l' local (/Library), 's' sistema (/System/Library) */
typedef struct { u32 type; const char *user, *local, *system; } folder_t;
static const folder_t folders[] = {
    {FOUR('p', 'r', 'e', 'f'), "~/Library/Preferences", "/Library/Preferences", "/System/Library/Preferences"},
    {FOUR('a', 's', 'u', 'p'), "~/Library/Application Support", "/Library/Application Support", "/System/Library/Application Support"},
    {FOUR('c', 'a', 'c', 'h'), "~/Library/Caches", "/Library/Caches", "/System/Library/Caches"},
    {FOUR('d', 'l', 'i', 'b'), "~/Library", "/Library", "/System/Library"},
    {FOUR('f', 'o', 'n', 't'), "~/Library/Fonts", "/Library/Fonts", "/System/Library/Fonts"},
    {FOUR('l', 'o', 'g', 's'), "~/Library/Logs", "/Library/Logs", "/System/Library/Logs"},
    {FOUR('f', 'r', 'm', 'k'), "~/Library/Frameworks", "/Library/Frameworks", "/System/Library/Frameworks"},
    {FOUR('a', 'p', 'p', 's'), "~/Applications", "/Applications", "/System/Applications"},
    {FOUR('s', 'c', 'r', '\xc4'), "~/Library/Scripts", "/Library/Scripts", "/System/Library/Scripts"},
    {FOUR('c', 'o', 'm', 'p'), "~/Library/Components", "/Library/Components", "/System/Library/Components"},
    {FOUR('i', 'n', 't', 'e'), "~/Library/Internet Plug-Ins", "/Library/Internet Plug-Ins", "/System/Library/Internet Plug-Ins"},
    {FOUR('p', 'p', 'a', 'n'), "~/Library/PreferencePanes", "/Library/PreferencePanes", "/System/Library/PreferencePanes"},
    {FOUR('a', 'u', 'd', 'p'), "~/Library/Audio/Plug-Ins", "/Library/Audio/Plug-Ins", "/System/Library/Audio/Plug-Ins"},
    {FOUR('d', 'e', 's', 'k'), "~/Desktop", 0, 0},
    {FOUR('d', 'o', 'c', 's'), "~/Documents", 0, 0},
    {FOUR('d', 'o', 'w', 'n'), "~/Downloads", 0, 0},
    {FOUR('m', 'd', 'o', 'c'), "~/Movies", 0, 0},
    {FOUR('\xb5', 'd', 'o', 'c'), "~/Music", 0, 0},
    {FOUR('p', 'd', 'o', 'c'), "~/Pictures", 0, 0},
    {FOUR('p', 'u', 'b', 'b'), "~/Public", 0, 0},
    {FOUR('c', 'u', 's', 'r'), "~", 0, 0},
    {FOUR('t', 'r', 's', 'h'), "~/.Trash", 0, 0},
    {FOUR('t', 'e', 'm', 'p'), "/private/tmp", "/private/tmp", "/private/tmp"},
    {FOUR('u', 's', 'r', 's'), 0, "/Users", "/Users"},
    {FOUR('m', 'a', 'c', 's'), 0, "/System", "/System"},
    {FOUR('c', 's', 'e', 'r'), 0, "/System/Library/CoreServices", "/System/Library/CoreServices"},
    {FOUR('s', 'd', 'a', 't'), 0, "/", "/"},
    {FOUR('a', 'p', 'p', 'r'), 0, "/Applications", "/Applications"},
    {FOUR('u', 't', 'i', 'l'), 0, "/Applications/Utilities", "/System/Applications/Utilities"},
};
static int mkdirs(const char *p) {
    char b[1024];
    strlcpy(b, p, sizeof b);
    for (char *s = b + 1; *s; s++) if (*s == '/') { *s = 0; mkdir(b, 0755); *s = '/'; }
    return mkdir(b, 0700) < 0 && cx_errno != 17 ? -1 : 0;
}
OSErr FSFindFolder(s16_t vRefNum, u32 folderType, Boolean createFolder, FSRef *foundRef) {
    if (!foundRef) return paramErr;
    const folder_t *f = 0;
    for (unsigned i = 0; i < sizeof folders / sizeof *folders; i++) if (folders[i].type == folderType) f = &folders[i];
    if (!f) return fnfErr;
    const char *t;
    switch (vRefNum) {
    case kUserDomain: t = f->user; break;
    case kLocalDomain: t = f->local; break;
    case kSystemDomain: t = f->system; break;
    case kNetworkDomain: return fnfErr;   /* sem domínio de rede */
    default: t = f->user ? f->user : f->local; break;   /* kOnAppropriateDisk, kOnSystemDisk e volumes: o do usuário */
    }
    if (!t) return fnfErr;
    char p[1024];
    if (t[0] == '~') {
        const char *home = getenv("HOME");
        if (!home || !*home) home = "/Users/Visitante";
        strlcpy(p, home, sizeof p);
        strlcat(p, t + 1, sizeof p);
    } else strlcpy(p, t, sizeof p);
    mos_statx_t x;
    if (cx_statx(p, -1, &x) < 0) {
        if (!createFolder) return fnfErr;
        if (mkdirs(p) < 0) return (OSErr)err_of(cx_errno);
    }
    return (OSErr)ref_of_path(p, foundRef, 0);
}
OSErr FSFindFolderExtended(s16_t vRefNum, u32 folderType, Boolean createFolder, u32 flags, void *data, FSRef *foundRef) {
    (void)flags; (void)data;
    return FSFindFolder(vRefNum, folderType, createFolder, foundRef);
}

#ifndef CX_HOST_TEST
/* ======================================================================== CFURL */
typedef void *id;
u8 CFURLGetFileSystemRepresentation(id url, u8 resolve, u8 *buf, long max);
id CFURLCreateFromFileSystemRepresentation(void *alloc, const u8 *buf, long n, u8 dir);
Boolean CFURLGetFSRef(id url, FSRef *fsRef) {
    u8 p[1024];
    if (!url || !fsRef || !CFURLGetFileSystemRepresentation(url, 1, p, sizeof p)) return 0;
    return FSPathMakeRef(p, fsRef, 0) == noErr;
}
id CFURLCreateFromFSRef(void *alloc, const FSRef *fsRef) {
    char p[1024];
    if (ref_path(fsRef, p, sizeof p)) return 0;
    mos_statx_t x;
    if (cx_statx(p, -1, &x) < 0) return 0;
    return CFURLCreateFromFileSystemRepresentation(alloc, (const u8 *)p, (long)strlen(p), x.kind == 1);
}
#endif
