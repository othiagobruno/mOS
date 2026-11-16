/*
 * mOS libSystem — NSFileManager completo, sobre o VFS do mOS: stat/lstat com as quatro datas e o dono reais
 * (fsmeta.c), statfs, copyfile (recursivo, com stat e xattr), rename, links simbólicos, chmod/chown/chflags/utimes,
 * e os erros Cocoa com o errno real por baixo (NSUnderlyingError no domínio POSIX), como no macOS.
 *
 * Os métodos entram por cima dos do foundation.c/foundation_extra.c/foundation_more.c em cx_fdn_filemanager_init(),
 * chamada no fim do boot do ObjC (objc.c), quando o NSFileManager já existe.
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
int *__error(void);
int lstat(const char *p, void *st);
int chmod(const char *p, unsigned short m);
int chown(const char *p, unsigned u, unsigned g);
int chflags(const char *p, unsigned f);
int utimes(const char *p, const void *tv);
int symlink(const char *t, const char *p);
long readlink(const char *p, char *b, size_t n);
int rename(const char *a, const char *b);
int link(const char *a, const char *b);
char *strtok(char *s, const char *d);
int rmdir(const char *p);
int chdir(const char *p);
int statfs(const char *p, void *s);
int copyfile(const char *from, const char *to, void *state, u32 flags);
int cx_remove_tree(const char *p);
int cx_set_birthtime(const char *abs, int fd, long long ns);
void *getpwuid(unsigned uid);
void *getgrgid(unsigned g);
unsigned getuid(void);
unsigned getgid(void);
long write(int fd, const void *b, size_t n);
long read(int fd, void *b, size_t n);
int open(const char *p, int f, ...);
int close(int fd);
char *strncpy(char *d, const char *s, size_t n);
char *strncat(char *d, const char *s, size_t n);
char *strrchr(const char *s, int c);
id NSHomeDirectory(void);
id NSTemporaryDirectory(void);

/* dirent do macOS ($INODE64), o mesmo de foundationsys.c */
typedef struct { u64 d_ino, d_seekoff; u16 d_reclen, d_namlen; u8 d_type; char d_name[1024]; } mac_dirent_t;
/* stat do macOS ($INODE64), o mesmo layout de libc.c */
typedef struct {
    u32 dev; u16 mode, nlink; u64 ino; u32 uid, gid, rdev;
    i64 atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns, btime, btime_ns;
    i64 size, blocks; i32 blksize; u32 flags, gen; i32 lspare; i64 qspare[2];
} fst_t;
typedef struct {
    u32 f_bsize; i32 f_iosize; u64 f_blocks, f_bfree, f_bavail, f_files, f_ffree; i32 f_fsid[2]; u32 f_owner;
    u32 f_type, f_flags, f_fssubtype; char f_fstypename[16], f_mntonname[1024], f_mntfromname[1024]; u32 f_flags_ext, f_reserved[7];
} fsf_t;

#define S_IFMT 0170000
#define S_IFDIR 0040000
#define S_IFREG 0100000
#define S_IFLNK 0120000
#define S_IFSOCK 0140000
#define S_IFCHR 0020000
#define S_IFBLK 0060000
#define UF_IMMUTABLE 0x2
#define UF_APPEND 0x4
#define UF_HIDDEN 0x8000

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
#define FK(var, text) static struct { void *isa; u64 flags; const char *s; u64 n; } var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
FK(NSFileOwnerAccountName, "NSFileOwnerAccountName") FK(NSFileGroupOwnerAccountName, "NSFileGroupOwnerAccountName")
FK(NSFileImmutable, "NSFileImmutable") FK(NSFileAppendOnly, "NSFileAppendOnly") FK(NSFileBusy, "NSFileBusy")
FK(NSFileHFSCreatorCode, "NSFileHFSCreatorCode") FK(NSFileHFSTypeCode, "NSFileHFSTypeCode")
FK(NSFileDeviceIdentifier, "NSFileDeviceIdentifier") FK(NSFileTypeSocket, "NSFileTypeSocket")
FK(NSFileTypeCharacterSpecial, "NSFileTypeCharacterSpecial") FK(NSFileTypeBlockSpecial, "NSFileTypeBlockSpecial")
FK(NSFileTypeUnknown, "NSFileTypeUnknown")
extern id NSFileType, NSFileSize, NSFileModificationDate, NSFileCreationDate, NSFileReferenceCount, NSFileSystemNumber,
    NSFileSystemFileNumber, NSFileOwnerAccountID, NSFileGroupOwnerAccountID, NSFileTypeDirectory, NSFileTypeRegular,
    NSFileTypeSymbolicLink, NSFileSystemSize, NSFileSystemFreeSize, NSFileProtectionKey, NSFileProtectionNone;
extern const void *NSFilePosixPermissions, *NSFileExtensionHidden, *NSFileSystemNodes, *NSFileSystemFreeNodes;
extern const void *NSFilePathErrorKey, *NSUnderlyingErrorKey;

static id cls(const char *n) { return (id)cx_class_named(n); }
static const char *P(id s) {
    if (!s) return 0;
    if (cx_is_str(s)) return cx_cstr(s);
    id p = MSG0(id, s, "path");   /* NSURL */
    return p ? cx_cstr(p) : 0;
}
static id str(const char *s) { return cx_str(s ? s : ""); }
static id num(i64 v) { return cx_num_i(v); }
static id num_b(int v) { return MSG(id, cls("NSNumber"), "numberWithBool:", (BOOL)(v != 0)); }
static id date_ns(i64 s, i64 ns) { return MSG(id, cls("NSDate"), "dateWithTimeIntervalSince1970:", (double)s + (double)ns / 1e9); }
static id mdict(void) { return MSG0(id, cls("NSMutableDictionary"), "dictionary"); }
static void dset(id d, const void *k, id v) { if (v) MSG(void, d, "setObject:forKey:", v, (id)k); }
static id url_file(const char *p, int dir) { return MSG(id, cls("NSURL"), "fileURLWithPath:isDirectory:", str(p), (BOOL)dir); }
static int is_dir(const char *p) { fst_t s; return p && stat(p, &s) == 0 && (s.mode & S_IFMT) == S_IFDIR; }
static int exists_l(const char *p) { fst_t s; return p && lstat(p, &s) == 0; }

/* ---- erros: NSCocoaErrorDomain com o errno no NSUnderlyingError (domínio POSIX) */
enum { E_READ, E_WRITE };
static id make_err(int code, const char *path, int eno) {
    id info = mdict();
    if (path) dset(info, NSFilePathErrorKey, str(path));
    if (eno) {
        id u = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSPOSIXErrorDomain"), (i64)eno, (id)0);
        dset(info, NSUnderlyingErrorKey, u);
    }
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSCocoaErrorDomain"), (i64)code, info);
}
static int cocoa_code(int eno, int kind) {
    switch (eno) {
    case 2: return kind == E_READ ? 260 : 4;          /* NSFileReadNoSuchFileError / NSFileNoSuchFileError */
    case 1: case 13: return kind == E_READ ? 257 : 513; /* sem permissão */
    case 17: return 516;                               /* NSFileWriteFileExistsError */
    case 28: case 69: return 640;                       /* NSFileWriteOutOfSpaceError */
    case 30: return 642;                                /* NSFileWriteVolumeReadOnlyError */
    case 63: return kind == E_READ ? 258 : 514;         /* nome inválido / longo demais */
    }
    return kind == E_READ ? 256 : 512;                  /* desconhecido */
}
static BOOL fail(id *err, int kind, const char *path) {
    int eno = *__error();
    if (err) *err = make_err(cocoa_code(eno, kind), path, eno);
    return NO;
}
static BOOL fail_code(id *err, int code, const char *path, int eno) { if (err) *err = make_err(code, path, eno); return NO; }

/* o delegate (fileManager:shouldXxxItemAtPath:...) decide antes de cada operação, como no macOS */
static id g_delegate;
static int ask(id self, const char *sel2, const char *path, const char *path2) {
    id d = g_delegate;
    (void)self;
    if (!d) return 1;
    SEL s = sel_registerName(sel2);
    if (!cx_responds(object_getClass(d), s)) return 1;
    if (path2) return ((BOOL (*)(id, SEL, id, id, id))objc_msgSend)(d, s, self, str(path), str(path2));
    return ((BOOL (*)(id, SEL, id, id))objc_msgSend)(d, s, self, str(path));
}
static id fm_delegate(id self, SEL c) { (void)self; (void)c; return g_delegate; }
static void fm_set_delegate(id self, SEL c, id d) { (void)self; (void)c; g_delegate = d; }   /* fraco, como no macOS */

/* ---- existência e acesso */
static BOOL fm_exists(id self, SEL c, id path) { (void)self; (void)c; fst_t s; const char *p = P(path); return p && stat(p, &s) == 0; }
static BOOL fm_exists_dir(id self, SEL c, id path, BOOL *isdir) {
    (void)self; (void)c;
    fst_t s;
    const char *p = P(path);
    if (!p || stat(p, &s) != 0) { if (isdir) *isdir = NO; return NO; }
    if (isdir) *isdir = (s.mode & S_IFMT) == S_IFDIR;
    return YES;
}
static BOOL fm_readable(id self, SEL c, id path) { (void)self; (void)c; const char *p = P(path); return p && access(p, 4) == 0; }
static BOOL fm_writable(id self, SEL c, id path) { (void)self; (void)c; const char *p = P(path); return p && access(p, 2) == 0; }
static BOOL fm_executable(id self, SEL c, id path) { (void)self; (void)c; const char *p = P(path); return p && access(p, 1) == 0; }
static BOOL fm_deletable(id self, SEL c, id path) {
    (void)self; (void)c;
    const char *p = P(path);
    char parent[1024];
    if (!p || !exists_l(p)) return NO;
    strncpy(parent, p, sizeof parent - 1); parent[1023] = 0;
    char *sl = strrchr(parent, '/');
    if (!sl) strcpy(parent, ".");
    else if (sl == parent) parent[1] = 0;
    else *sl = 0;
    return access(parent, 2) == 0;
}

/* ---- conteúdo de diretório */
static int skip_name(const char *n) { return !strcmp(n, ".") || !strcmp(n, ".."); }
static id list_dir(const char *p, int skip_hidden, id *err) {
    void *d = p ? opendir(p) : 0;
    if (!d) { fail(err, E_READ, p); return 0; }
    id a = MSG0(id, cls("NSMutableArray"), "array");
    for (mac_dirent_t *e; (e = readdir(d));) {
        if (skip_name(e->d_name)) continue;
        if (skip_hidden && e->d_name[0] == '.') continue;
        cx_arr_add(a, str(e->d_name));
    }
    closedir(d);
    if (err) *err = 0;
    return a;
}
static id fm_contents(id self, SEL c, id path, id *err) { (void)self; (void)c; return list_dir(P(path), 0, err); }
static id fm_contents_url(id self, SEL c, id url, id keys, u64 opts, id *err) {
    (void)self; (void)c; (void)keys;
    const char *p = P(url);
    id names = list_dir(p, (opts & 4) != 0 /* NSDirectoryEnumerationSkipsHiddenFiles */, err);
    if (!names) return 0;
    id out = MSG0(id, cls("NSMutableArray"), "array");
    u64 n = MSG0(u64, names, "count");
    for (u64 i = 0; i < n; i++) {
        char full[1024];
        snprintf(full, sizeof full, "%s/%s", p, cx_cstr(MSG(id, names, "objectAtIndex:", i)));
        cx_arr_add(out, url_file(full, is_dir(full)));
    }
    return out;
}
static void subpaths_rec(const char *root, const char *rel, id out, int depth) {
    char full[1024];
    snprintf(full, sizeof full, "%s%s%s", root, rel[0] ? "/" : "", rel);
    void *d = opendir(full);
    if (!d || depth > 64) { if (d) closedir(d); return; }
    for (mac_dirent_t *e; (e = readdir(d));) {
        if (skip_name(e->d_name)) continue;
        char r2[1024], f2[1024];
        snprintf(r2, sizeof r2, "%s%s%s", rel, rel[0] ? "/" : "", e->d_name);
        snprintf(f2, sizeof f2, "%s/%s", root, r2);
        cx_arr_add(out, str(r2));
        fst_t s;
        if (lstat(f2, &s) == 0 && (s.mode & S_IFMT) == S_IFDIR) subpaths_rec(root, r2, out, depth + 1);
    }
    closedir(d);
}
static id fm_subpaths_err(id self, SEL c, id path, id *err) {
    (void)self; (void)c;
    const char *p = P(path);
    if (!p || !is_dir(p)) { if (!p || !exists_l(p)) *__error() = 2; else *__error() = 20; fail(err, E_READ, p); return 0; }
    id out = MSG0(id, cls("NSMutableArray"), "array");
    subpaths_rec(p, "", out, 0);
    if (err) *err = 0;
    return out;
}
static id fm_subpaths(id self, SEL c, id path) { return fm_subpaths_err(self, c, path, 0); }

/* ---- criar */
static void apply_attrs(const char *p, id attrs);
static BOOL mkdir_p(const char *p, int inter, id *err) {
    if (!inter) {
        if (mkdir(p, 0755) == 0) return YES;
        return fail(err, E_WRITE, p);
    }
    char b[1024];
    strncpy(b, p, sizeof b - 1); b[1023] = 0;
    for (char *q = b + 1; *q; q++) {
        if (*q != '/') continue;
        *q = 0;
        if (!is_dir(b) && mkdir(b, 0755) != 0 && !is_dir(b)) return fail(err, E_WRITE, b);
        *q = '/';
    }
    if (is_dir(b)) return YES;
    if (mkdir(b, 0755) != 0 && !is_dir(b)) return fail(err, E_WRITE, b);
    return YES;
}
static BOOL fm_mkdir(id self, SEL c, id path, BOOL inter, id attrs, id *err) {
    (void)self; (void)c;
    const char *p = P(path);
    if (!p) return fail_code(err, 4, 0, 2);
    if (exists_l(p) && !inter) return fail_code(err, 516, p, 17);
    if (exists_l(p) && !is_dir(p)) return fail_code(err, 516, p, 17);
    if (!mkdir_p(p, inter, err)) return NO;
    apply_attrs(p, attrs);
    if (err) *err = 0;
    return YES;
}
static BOOL fm_create_file(id self, SEL c, id path, id data, id attrs) {
    (void)self; (void)c;
    const char *p = P(path);
    if (!p) return NO;
    int fd = open(p, 0x0601, 0644);   /* O_WRONLY|O_CREAT|O_TRUNC */
    if (fd < 0) return NO;
    u64 n = data ? MSG0(u64, data, "length") : 0;
    const char *b = data ? MSG0(const char *, data, "bytes") : 0;
    for (u64 o = 0; o < n;) { long w = write(fd, b + o, (size_t)(n - o)); if (w <= 0) { close(fd); return NO; } o += (u64)w; }
    close(fd);
    apply_attrs(p, attrs);
    return YES;
}
static id fm_contents_at(id self, SEL c, id path) {
    (void)self; (void)c;
    const char *p = P(path);
    if (!p || is_dir(p)) return 0;
    return MSG(id, cls("NSData"), "dataWithContentsOfFile:", str(p));
}
static BOOL fm_equal_at(id self, SEL c, id a, id b) {
    const char *pa = P(a), *pb = P(b);
    if (!pa || !pb) return NO;
    if (is_dir(pa) || is_dir(pb)) {
        if (!(is_dir(pa) && is_dir(pb))) return NO;
        id la = list_dir(pa, 0, 0), lb = list_dir(pb, 0, 0);
        la = MSG(id, la, "sortedArrayUsingSelector:", sel_registerName("compare:"));
        lb = MSG(id, lb, "sortedArrayUsingSelector:", sel_registerName("compare:"));
        if (!MSG(BOOL, la, "isEqualToArray:", lb)) return NO;
        u64 n = MSG0(u64, la, "count");
        for (u64 i = 0; i < n; i++) {
            char x[1024], y[1024];
            const char *name = cx_cstr(MSG(id, la, "objectAtIndex:", i));
            snprintf(x, sizeof x, "%s/%s", pa, name);
            snprintf(y, sizeof y, "%s/%s", pb, name);
            if (!fm_equal_at(self, c, str(x), str(y))) return NO;
        }
        return YES;
    }
    id da = fm_contents_at(self, c, a), db = fm_contents_at(self, c, b);
    return da && db && MSG(BOOL, da, "isEqualToData:", db);
}

/* ---- remover, copiar, mover, ligar */
static BOOL fm_remove(id self, SEL c, id path, id *err) {
    (void)c;
    const char *p = P(path);
    if (!p || !exists_l(p)) return fail_code(err, 4, p, 2);
    if (!ask(self, "fileManager:shouldRemoveItemAtPath:", p, 0)) { if (err) *err = 0; return YES; }
    if (cx_remove_tree(p) != 0) return fail(err, E_WRITE, p);
    if (err) *err = 0;
    return YES;
}
static BOOL copy_item(id self, const char *a, const char *b, id *err) {
    if (!a || !exists_l(a)) return fail_code(err, 260, a, 2);
    if (!b) return fail_code(err, 4, 0, 2);
    if (exists_l(b)) return fail_code(err, 516, b, 17);
    if (!ask(self, "fileManager:shouldCopyItemAtPath:toPath:", a, b)) { if (err) *err = 0; return YES; }
    fst_t s;
    if (lstat(a, &s) == 0 && (s.mode & S_IFMT) == S_IFLNK) {
        /* link simbólico: copia o link, não o destino (como o macOS) */
        char t[1024];
        long n = readlink(a, t, sizeof t - 1);
        if (n < 0) return fail(err, E_READ, a);
        t[n] = 0;
        if (symlink(t, b) != 0) return fail(err, E_WRITE, b);
    } else if (copyfile(a, b, 0, (1u << 15) | (1u << 3) | (1u << 1) | (1u << 2)) != 0) {
        /* COPYFILE_RECURSIVE | DATA | STAT | XATTR */
        return fail(err, E_WRITE, b);
    }
    if (err) *err = 0;
    return YES;
}
static BOOL fm_copy(id self, SEL c, id a, id b, id *err) { (void)c; return copy_item(self, P(a), P(b), err); }
static BOOL move_item(id self, const char *a, const char *b, id *err) {
    if (!a || !exists_l(a)) return fail_code(err, 4, a, 2);
    if (!b) return fail_code(err, 4, 0, 2);
    if (exists_l(b)) return fail_code(err, 516, b, 17);
    if (!ask(self, "fileManager:shouldMoveItemAtPath:toPath:", a, b)) { if (err) *err = 0; return YES; }
    if (rename(a, b) == 0) { if (err) *err = 0; return YES; }
    if (*__error() != 18 /* EXDEV */) return fail(err, E_WRITE, b);
    /* outro volume: copia e apaga a origem */
    if (copyfile(a, b, 0, (1u << 15) | (1u << 3) | (1u << 1) | (1u << 2)) != 0) return fail(err, E_WRITE, b);
    if (cx_remove_tree(a) != 0) return fail(err, E_WRITE, a);
    if (err) *err = 0;
    return YES;
}
static BOOL fm_move(id self, SEL c, id a, id b, id *err) { (void)c; return move_item(self, P(a), P(b), err); }
static BOOL fm_link(id self, SEL c, id a, id b, id *err) {
    (void)c;
    const char *pa = P(a), *pb = P(b);
    if (!pa || !exists_l(pa)) return fail_code(err, 260, pa, 2);
    if (pb && exists_l(pb)) return fail_code(err, 516, pb, 17);
    if (!ask(self, "fileManager:shouldLinkItemAtPath:toPath:", pa, pb)) { if (err) *err = 0; return YES; }
    /* diretório: link físico não existe; o macOS também recusa */
    if (is_dir(pa)) return fail_code(err, 512, pb, 1);
    if (link(pa, pb) != 0) return fail(err, E_WRITE, pb);
    if (err) *err = 0;
    return YES;
}
static BOOL fm_symlink(id self, SEL c, id path, id dest, id *err) {
    (void)self; (void)c;
    const char *p = P(path), *d = P(dest);
    if (!p || !d) return fail_code(err, 4, p, 2);
    if (exists_l(p)) return fail_code(err, 516, p, 17);
    if (symlink(d, p) != 0) return fail(err, E_WRITE, p);
    if (err) *err = 0;
    return YES;
}
static BOOL fm_symlink_url(id self, SEL c, id url, id dest, id *err) {
    /* o destino de URL relativa fica relativo (relativePath), como no macOS */
    id rel = dest ? MSG0(id, dest, "relativePath") : 0;
    return fm_symlink(self, c, url, rel ? rel : dest, err);
}
static id fm_readlink(id self, SEL c, id path, id *err) {
    (void)self; (void)c;
    char t[1024];
    const char *p = P(path);
    long n = p ? readlink(p, t, sizeof t - 1) : -1;
    if (n < 0) { fail(err, E_READ, p); return 0; }
    t[n] = 0;
    if (err) *err = 0;
    return str(t);
}

/* ---- atributos */
static const char *user_name(unsigned uid) { void *pw = getpwuid(uid); return pw ? *(const char **)pw : 0; }
static const char *group_name(unsigned gid) { void *gr = getgrgid(gid); return gr ? *(const char **)gr : 0; }
static id attrs_of(const char *p, int follow, id *err) {
    fst_t s;
    if (!p || (follow ? stat(p, &s) : lstat(p, &s)) != 0) { fail(err, E_READ, p); return 0; }
    id d = mdict();
    id type;
    switch (s.mode & S_IFMT) {
    case S_IFDIR: type = NSFileTypeDirectory; break;
    case S_IFREG: type = NSFileTypeRegular; break;
    case S_IFLNK: type = NSFileTypeSymbolicLink; break;
    case S_IFSOCK: type = (id)NSFileTypeSocket; break;
    case S_IFCHR: type = (id)NSFileTypeCharacterSpecial; break;
    case S_IFBLK: type = (id)NSFileTypeBlockSpecial; break;
    default: type = (id)NSFileTypeUnknown; break;
    }
    dset(d, NSFileType, type);
    dset(d, NSFileSize, MSG(id, cls("NSNumber"), "numberWithUnsignedLongLong:", (u64)s.size));
    dset(d, NSFileModificationDate, date_ns(s.mtime, s.mtime_ns));
    dset(d, NSFileCreationDate, date_ns(s.btime ? s.btime : s.mtime, s.btime ? s.btime_ns : s.mtime_ns));
    dset(d, NSFilePosixPermissions, num(s.mode & 07777));
    dset(d, NSFileReferenceCount, num(s.nlink));
    dset(d, NSFileSystemNumber, num(s.dev));
    dset(d, NSFileSystemFileNumber, MSG(id, cls("NSNumber"), "numberWithUnsignedLongLong:", s.ino));
    dset(d, NSFileOwnerAccountID, num(s.uid));
    dset(d, NSFileGroupOwnerAccountID, num(s.gid));
    const char *un = user_name(s.uid), *gn = group_name(s.gid);
    if (un) dset(d, NSFileOwnerAccountName, str(un));
    if (gn) dset(d, NSFileGroupOwnerAccountName, str(gn));
    dset(d, NSFileExtensionHidden, num_b(0));
    dset(d, NSFileImmutable, num_b((s.flags & UF_IMMUTABLE) != 0));
    dset(d, NSFileAppendOnly, num_b((s.flags & UF_APPEND) != 0));
    dset(d, NSFileBusy, num_b(0));
    dset(d, NSFileHFSCreatorCode, num(0));
    dset(d, NSFileHFSTypeCode, num(0));
    if ((s.mode & S_IFMT) == S_IFCHR || (s.mode & S_IFMT) == S_IFBLK) dset(d, NSFileDeviceIdentifier, num(s.rdev));
    dset(d, NSFileProtectionKey, NSFileProtectionNone);
    if (err) *err = 0;
    return d;
}
static id fm_attrs(id self, SEL c, id path, id *err) { (void)self; (void)c; return attrs_of(P(path), 0, err); }
static id fm_attrs_traverse(id self, SEL c, id path, BOOL follow) { (void)self; (void)c; return attrs_of(P(path), follow, 0); }
static id dget(id d, const void *k) { return d ? MSG(id, d, "objectForKey:", (id)k) : 0; }
static void apply_attrs(const char *p, id a) {
    id v;
    if (!a || !p) return;
    fst_t s;
    if (lstat(p, &s) != 0) return;
    if ((v = dget(a, NSFilePosixPermissions))) chmod(p, (unsigned short)MSG0(i64, v, "longLongValue"));
    id ou = dget(a, NSFileOwnerAccountID), gu = dget(a, NSFileGroupOwnerAccountID);
    if (ou || gu) chown(p, ou ? (unsigned)MSG0(i64, ou, "longLongValue") : (unsigned)-1, gu ? (unsigned)MSG0(i64, gu, "longLongValue") : (unsigned)-1);
    id md = dget(a, NSFileModificationDate);
    if (md) {
        double t = MSG0(double, md, "timeIntervalSince1970");
        struct { long sec; int usec; int pad; } tv[2];
        tv[0].sec = (long)s.atime; tv[0].usec = (int)(s.atime_ns / 1000); tv[0].pad = 0;
        tv[1].sec = (long)t; tv[1].usec = (int)((t - (double)(long)t) * 1e6); tv[1].pad = 0;
        utimes(p, tv);
    }
    id cd = dget(a, NSFileCreationDate);
    if (cd) cx_set_birthtime(p, -1, (long long)(MSG0(double, cd, "timeIntervalSince1970") * 1e9));
    id im = dget(a, NSFileImmutable), ap = dget(a, NSFileAppendOnly), hid = dget(a, NSFileExtensionHidden);
    if (im || ap || hid) {
        unsigned f = s.flags;
        if (im) f = MSG0(BOOL, im, "boolValue") ? (f | UF_IMMUTABLE) : (f & ~UF_IMMUTABLE);
        if (ap) f = MSG0(BOOL, ap, "boolValue") ? (f | UF_APPEND) : (f & ~UF_APPEND);
        chflags(p, f);
    }
}
static BOOL fm_set_attrs(id self, SEL c, id attrs, id path, id *err) {
    (void)self; (void)c;
    const char *p = P(path);
    if (!p || !exists_l(p)) return fail_code(err, 4, p, 2);
    *__error() = 0;
    apply_attrs(p, attrs);
    if (*__error() == 1 || *__error() == 13) return fail(err, E_WRITE, p);
    if (err) *err = 0;
    return YES;
}
static id fm_fs_attrs(id self, SEL c, id path, id *err) {
    (void)self; (void)c;
    fsf_t f;
    const char *p = P(path);
    if (!p || statfs(p, &f) != 0) { fail(err, E_READ, p); return 0; }
    id d = mdict(), N = cls("NSNumber");
    dset(d, NSFileSystemSize, MSG(id, N, "numberWithUnsignedLongLong:", f.f_blocks * (u64)f.f_bsize));
    dset(d, NSFileSystemFreeSize, MSG(id, N, "numberWithUnsignedLongLong:", f.f_bavail * (u64)f.f_bsize));
    dset(d, NSFileSystemNodes, MSG(id, N, "numberWithUnsignedLongLong:", f.f_files));
    dset(d, NSFileSystemFreeNodes, MSG(id, N, "numberWithUnsignedLongLong:", f.f_ffree));
    fst_t s;
    dset(d, NSFileSystemNumber, num(stat(p, &s) == 0 ? s.dev : (i64)f.f_fsid[0]));
    if (err) *err = 0;
    return d;
}

/* ---- diretórios e nomes */
static id fm_cwd(id self, SEL c) { (void)self; (void)c; char b[1024]; return str(getcwd(b, sizeof b)); }
static BOOL fm_chdir(id self, SEL c, id path) { (void)self; (void)c; const char *p = P(path); return p && chdir(p) == 0; }
static id fm_display_name(id self, SEL c, id path) {
    (void)self; (void)c;
    const char *p = P(path);
    if (!p) return str("");
    const char *sl = strrchr(p, '/');
    const char *n = sl && sl[1] ? sl + 1 : p;
    if (!strcmp(p, "/")) return str("Macintosh HD");
    /* pacotes .app aparecem sem a extensão, como no Finder */
    size_t l = strlen(n);
    if (l > 4 && !strcmp(n + l - 4, ".app")) return cx_str_n(n, l - 4);
    return str(n);
}
static id fm_components_display(id self, SEL c, id path) {
    const char *p = P(path);
    if (!p || !exists_l(p)) return 0;
    id out = MSG0(id, cls("NSMutableArray"), "array");
    char acc[1024] = "";
    cx_arr_add(out, fm_display_name(self, c, str("/")));
    char b[1024];
    strncpy(b, p, sizeof b - 1); b[1023] = 0;
    for (char *tok = strtok(b, "/"); tok; tok = strtok(0, "/")) {
        strncat(acc, "/", sizeof acc - strlen(acc) - 1);
        strncat(acc, tok, sizeof acc - strlen(acc) - 1);
        cx_arr_add(out, fm_display_name(self, c, str(acc)));
    }
    return out;
}
static const char *fm_fsrep(id self, SEL c, id path) { (void)self; (void)c; return path ? cx_cstr(path) : ""; }
static id fm_from_fsrep(id self, SEL c, const char *s, u64 n) { (void)self; (void)c; return s ? cx_str_n(s, n) : 0; }
static id fm_temp_dir(id self, SEL c) { (void)self; (void)c; return url_file(cx_cstr(NSTemporaryDirectory()), 1); }
static id fm_home_url(id self, SEL c) { (void)self; (void)c; return url_file(cx_cstr(NSHomeDirectory()), 1); }
static id fm_home_for(id self, SEL c, id user) {
    (void)self; (void)c;
    const char *me = user_name(getuid());
    if (!user || (me && !strcmp(cx_cstr(user), me))) return url_file(cx_cstr(NSHomeDirectory()), 1);
    char b[512];
    snprintf(b, sizeof b, "/Users/%s", cx_cstr(user));
    return is_dir(b) ? url_file(b, 1) : 0;
}
static id fm_user_name(id self, SEL c) { (void)self; (void)c; const char *u = user_name(getuid()); return str(u ? u : "user"); }

/* ---- lixeira, troca segura, volumes, iCloud */
static BOOL fm_trash(id self, SEL c, id url, id *out, id *err) {
    (void)c;
    const char *p = P(url);
    if (!p || !exists_l(p)) return fail_code(err, 4, p, 2);
    char trash[1024], dst[1100];
    snprintf(trash, sizeof trash, "%s/.Trash", cx_cstr(NSHomeDirectory()));
    if (!is_dir(trash) && mkdir(trash, 0700) != 0) return fail(err, E_WRITE, trash);
    const char *sl = strrchr(p, '/');
    const char *name = sl ? sl + 1 : p;
    snprintf(dst, sizeof dst, "%s/%s", trash, name);
    /* nome repetido na lixeira: "nome 2", "nome 3"... como o Finder */
    for (int i = 2; exists_l(dst) && i < 10000; i++) {
        const char *dot = strrchr(name, '.');
        if (dot && dot != name) snprintf(dst, sizeof dst, "%s/%.*s %d%s", trash, (int)(dot - name), name, i, dot);
        else snprintf(dst, sizeof dst, "%s/%s %d", trash, name, i);
    }
    if (!move_item(self, p, dst, err)) return NO;
    if (out) *out = url_file(dst, is_dir(dst));
    return YES;
}
static BOOL fm_replace(id self, SEL c, id orig, id repl, id backup_name, u64 opts, id *result, id *err) {
    (void)c;
    const char *po = P(orig), *pr = P(repl);
    char bak[1200] = "";
    if (!pr || !exists_l(pr)) return fail_code(err, 4, pr, 2);
    if (!po) return fail_code(err, 4, 0, 2);
    if (exists_l(po)) {
        if (backup_name) {
            char dir[1024];
            strncpy(dir, po, sizeof dir - 1); dir[1023] = 0;
            char *sl = strrchr(dir, '/');
            if (sl) *sl = 0;
            snprintf(bak, sizeof bak, "%s/%s", sl ? dir : ".", cx_cstr(backup_name));
            cx_remove_tree(bak);
            if (rename(po, bak) != 0) return fail(err, E_WRITE, bak);
        } else if (cx_remove_tree(po) != 0) return fail(err, E_WRITE, po);
    }
    if (rename(pr, po) != 0) {
        if (bak[0]) rename(bak, po);   /* desfaz */
        return fail(err, E_WRITE, po);
    }
    /* NSFileManagerItemReplacementWithoutDeletingBackupItem = 2 */
    if (bak[0] && !(opts & 2)) cx_remove_tree(bak);
    if (result) *result = url_file(po, is_dir(po));
    if (err) *err = 0;
    (void)self;
    return YES;
}
static id fm_volumes(id self, SEL c, id keys, u64 opts) { (void)self; (void)c; (void)keys; (void)opts; id a = MSG0(id, cls("NSMutableArray"), "array"); cx_arr_add(a, url_file("/", 1)); return a; }
static id fm_volumes0(id self, SEL c) { (void)self; (void)c; id a = MSG0(id, cls("NSMutableArray"), "array"); cx_arr_add(a, str("/")); return a; }
static BOOL fm_relationship(id self, SEL c, i64 *rel, id dir, id item, id *err) {
    (void)self; (void)c;
    const char *d = P(dir), *i = P(item);
    if (!d || !i) return fail_code(err, 4, d, 2);
    size_t n = strlen(d);
    while (n > 1 && d[n - 1] == '/') n--;
    /* NSURLRelationshipContains = 0, Same = 1, Other = 2 */
    if (!strncmp(d, i, n) && (i[n] == 0 || (i[n] == '/' && i[n + 1] == 0))) *rel = 1;
    else if (!strncmp(d, i, n) && i[n] == '/') *rel = 0;
    else *rel = 2;
    if (err) *err = 0;
    return YES;
}
static id nil_m(id self, SEL c) { (void)self; (void)c; return 0; }
static id nil_m1(id self, SEL c, id a) { (void)self; (void)c; (void)a; return 0; }
static BOOL no_m1(id self, SEL c, id a) { (void)self; (void)c; (void)a; return NO; }
static BOOL ubq_set(id self, SEL c, BOOL f, id item, id dest, id *err) {
    (void)self; (void)c; (void)f; (void)item; (void)dest;
    /* sem conta iCloud no mOS: NSFeatureUnsupportedError, como um Mac sem iCloud */
    return fail_code(err, 3328, 0, 0);
}
static BOOL ubq_evict(id self, SEL c, id url, id *err) { (void)self; (void)c; (void)url; return fail_code(err, 3328, 0, 0); }

void cx_fdn_filemanager_init(void) {
    cx_class_t *fm = cx_class_named("NSFileManager");
    if (!fm) return;
#define A(sel, f) cx_add_method(fm, sel, (void *)(f))
    A("delegate", fm_delegate); A("setDelegate:", fm_set_delegate);
    A("fileExistsAtPath:", fm_exists); A("fileExistsAtPath:isDirectory:", fm_exists_dir);
    A("isReadableFileAtPath:", fm_readable); A("isWritableFileAtPath:", fm_writable);
    A("isExecutableFileAtPath:", fm_executable); A("isDeletableFileAtPath:", fm_deletable);
    A("contentsOfDirectoryAtPath:error:", fm_contents);
    A("contentsOfDirectoryAtURL:includingPropertiesForKeys:options:error:", fm_contents_url);
    A("subpathsOfDirectoryAtPath:error:", fm_subpaths_err); A("subpathsAtPath:", fm_subpaths);
    A("createDirectoryAtPath:withIntermediateDirectories:attributes:error:", fm_mkdir);
    A("createDirectoryAtURL:withIntermediateDirectories:attributes:error:", fm_mkdir);
    A("createFileAtPath:contents:attributes:", fm_create_file);
    A("contentsAtPath:", fm_contents_at); A("contentsEqualAtPath:andPath:", fm_equal_at);
    A("removeItemAtPath:error:", fm_remove); A("removeItemAtURL:error:", fm_remove);
    A("copyItemAtPath:toPath:error:", fm_copy); A("copyItemAtURL:toURL:error:", fm_copy);
    A("moveItemAtPath:toPath:error:", fm_move); A("moveItemAtURL:toURL:error:", fm_move);
    A("linkItemAtPath:toPath:error:", fm_link); A("linkItemAtURL:toURL:error:", fm_link);
    A("createSymbolicLinkAtPath:withDestinationPath:error:", fm_symlink);
    A("createSymbolicLinkAtURL:withDestinationURL:error:", fm_symlink_url);
    A("destinationOfSymbolicLinkAtPath:error:", fm_readlink);
    A("attributesOfItemAtPath:error:", fm_attrs); A("fileAttributesAtPath:traverseLink:", fm_attrs_traverse);
    A("setAttributes:ofItemAtPath:error:", fm_set_attrs);
    A("attributesOfFileSystemForPath:error:", fm_fs_attrs);
    A("currentDirectoryPath", fm_cwd); A("changeCurrentDirectoryPath:", fm_chdir);
    A("displayNameAtPath:", fm_display_name); A("componentsToDisplayForPath:", fm_components_display);
    A("fileSystemRepresentationWithPath:", fm_fsrep); A("stringWithFileSystemRepresentation:length:", fm_from_fsrep);
    A("temporaryDirectory", fm_temp_dir); A("homeDirectoryForCurrentUser", fm_home_url);
    A("homeDirectoryForUser:", fm_home_for); A("_userName", fm_user_name);
    A("trashItemAtURL:resultingItemURL:error:", fm_trash);
    A("replaceItemAtURL:withItemAtURL:backupItemName:options:resultingItemURL:error:", fm_replace);
    A("mountedVolumeURLsIncludingResourceValuesForKeys:options:", fm_volumes); A("mountedLocalVolumePaths", fm_volumes0);
    A("getRelationship:ofDirectoryAtURL:toItemAtURL:error:", fm_relationship);
    A("ubiquityIdentityToken", nil_m); A("URLForUbiquityContainerIdentifier:", nil_m1);
    A("isUbiquitousItemAtURL:", no_m1); A("setUbiquitous:itemAtURL:destinationURL:error:", ubq_set);
    A("evictUbiquitousItemAtURL:error:", ubq_evict); A("startDownloadingUbiquitousItemAtURL:error:", ubq_evict);
#undef A
}
