/*
 * mOS libSystem — DiskArbitration.framework sobre os volumes reais do VFS (getfsstat/statfs de attrlist.c,
 * SYS_IMGDETACH para desmontar/ejetar).
 *
 * Cada volume montado é um "disco" com nome BSD estável: o do sistema é disk0s1 (inteiro disk0); os de /Volumes
 * são disk<N>s1 (inteiro disk<N>), N a partir de 2 na ordem em que aparecem. DADiskRef e DASessionRef são objetos
 * CF (dicionários), então CFRetain/CFRelease/CFEqual valem. As descrições usam as chaves da Apple.
 * Callbacks: DARegisterDiskAppeared chama uma vez para cada disco existente quando a sessão é agendada (como o
 * diskarbitrationd faz) e depois a cada volume novo; Disappeared quando o volume sai. Um fio olha os volumes a cada
 * segundo; a entrega é na fila do DASessionSetDispatchQueue, ou na fila principal com ScheduleWithRunLoop.
 */
#include "sys.h"
#include "libc.h"

typedef void *id;
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
id CFRetain(id o);
void CFRelease(id o);
id CFStringCreateWithCString(void *alloc, const char *s, u32 enc);
const char *cx_cstr(id s);
id CFDictionaryCreateMutable(void *alloc, long cap, const void *k, const void *v);
void CFDictionarySetValue(id d, const void *k, const void *v);
const void *CFDictionaryGetValue(id d, const void *k);
long CFDictionaryGetCount(id d);
id CFNumberCreate(void *alloc, long type, const void *p);
id CFURLCreateWithFileSystemPath(void *alloc, id path, long style, u8 dir);
u8 CFURLGetFileSystemRepresentation(id url, u8 resolve, u8 *buf, long max);
u8 CFEqual(id a, id b);
extern const char kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks;
extern void *kCFBooleanTrue, *kCFBooleanFalse;
void *dispatch_get_main_queue(void);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int usleep(unsigned us);
int getfsstat(void *buf, int bufsize, int flags);
int statfs(const char *path, void *s);
int cx_follow_path(const char *in, char *out, size_t cap);
#ifndef CX_HOST_TEST
int snprintf(char *b, size_t n, const char *f, ...);
int atoi(const char *s);
char *strrchr(const char *s, int c);
char *strchr(const char *s, int c);
#else
#include <stdio.h>
#include <stdlib.h>
#endif

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } da_cs_t;
#define K(var, text) static da_cs_t var##_cs = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_cs;
K(kDADiskDescriptionVolumeKindKey, "DAVolumeKind")
K(kDADiskDescriptionVolumeMountableKey, "DAVolumeMountable")
K(kDADiskDescriptionVolumeNameKey, "DAVolumeName")
K(kDADiskDescriptionVolumeNetworkKey, "DAVolumeNetwork")
K(kDADiskDescriptionVolumePathKey, "DAVolumePath")
K(kDADiskDescriptionVolumeTypeKey, "DAVolumeType")
K(kDADiskDescriptionVolumeUUIDKey, "DAVolumeUUID")
K(kDADiskDescriptionMediaBlockSizeKey, "DAMediaBlockSize")
K(kDADiskDescriptionMediaBSDMajorKey, "DAMediaBSDMajor")
K(kDADiskDescriptionMediaBSDMinorKey, "DAMediaBSDMinor")
K(kDADiskDescriptionMediaBSDNameKey, "DAMediaBSDName")
K(kDADiskDescriptionMediaBSDUnitKey, "DAMediaBSDUnit")
K(kDADiskDescriptionMediaContentKey, "DAMediaContent")
K(kDADiskDescriptionMediaEjectableKey, "DAMediaEjectable")
K(kDADiskDescriptionMediaEncryptedKey, "DAMediaEncrypted")
K(kDADiskDescriptionMediaKindKey, "DAMediaKind")
K(kDADiskDescriptionMediaLeafKey, "DAMediaLeaf")
K(kDADiskDescriptionMediaNameKey, "DAMediaName")
K(kDADiskDescriptionMediaPathKey, "DAMediaPath")
K(kDADiskDescriptionMediaRemovableKey, "DAMediaRemovable")
K(kDADiskDescriptionMediaSizeKey, "DAMediaSize")
K(kDADiskDescriptionMediaTypeKey, "DAMediaType")
K(kDADiskDescriptionMediaUUIDKey, "DAMediaUUID")
K(kDADiskDescriptionMediaWholeKey, "DAMediaWhole")
K(kDADiskDescriptionMediaWritableKey, "DAMediaWritable")
K(kDADiskDescriptionDeviceGUIDKey, "DADeviceGUID")
K(kDADiskDescriptionDeviceInternalKey, "DADeviceInternal")
K(kDADiskDescriptionDeviceModelKey, "DADeviceModel")
K(kDADiskDescriptionDevicePathKey, "DADevicePath")
K(kDADiskDescriptionDeviceProtocolKey, "DADeviceProtocol")
K(kDADiskDescriptionDeviceRevisionKey, "DADeviceRevision")
K(kDADiskDescriptionDeviceUnitKey, "DADeviceUnit")
K(kDADiskDescriptionDeviceVendorKey, "DADeviceVendor")
K(kDADiskDescriptionBusNameKey, "DABusName")
K(kDADiskDescriptionBusPathKey, "DABusPath")
K(kDA_BSDNAME_KEY, "DA.mOS.BSDName")       /* dentro do objeto disco */
K(kDA_MNT_KEY, "DA.mOS.MountPoint")
K(kDA_KIND_KEY, "DA.mOS.Kind")

/* filtros prontos: dicionários reais (antes eram nil) */
const void *kDADiskDescriptionMatchMediaUnformatted, *kDADiskDescriptionMatchMediaWhole, *kDADiskDescriptionMatchVolumeMountable,
    *kDADiskDescriptionMatchVolumeUnrecognized, *kDADiskDescriptionWatchVolumeName, *kDADiskDescriptionWatchVolumePath;
static void da_consts(void) {
    static int done;
    if (done) return;
    done = 1;
    id a = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(a, kDADiskDescriptionVolumeMountableKey, kCFBooleanTrue);
    kDADiskDescriptionMatchVolumeMountable = a;
    id b = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(b, kDADiskDescriptionMediaWholeKey, kCFBooleanTrue);
    kDADiskDescriptionMatchMediaWhole = b;
    id c = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(c, kDADiskDescriptionVolumeMountableKey, kCFBooleanFalse);
    kDADiskDescriptionMatchVolumeUnrecognized = c;
    kDADiskDescriptionMatchMediaUnformatted = c;
    kDADiskDescriptionWatchVolumeName = 0;
    kDADiskDescriptionWatchVolumePath = 0;
}

/* ======================================================================== volumes */
typedef struct {
    u32 f_bsize; i32 f_iosize; u64 f_blocks, f_bfree, f_bavail, f_files, f_ffree; i32 f_fsid[2]; u32 f_owner;
    u32 f_type, f_flags, f_fssubtype; char f_fstypename[16], f_mntonname[1024], f_mntfromname[1024]; u32 f_flags_ext, f_reserved[7];
} da_sf_t;
typedef struct { char bsd[16], mnt[1024], from[1024], kind[16]; u64 size; u32 flags; int unit; } da_vol_t;
#define DA_MAX 32
static char unit_of[DA_MAX][1024];          /* ponto de montagem -> unidade (estável enquanto montado) */
static int unit_used[DA_MAX];

static int unit_for(const char *mnt) {
    if (!strcmp(mnt, "/")) return 0;
    for (int u = 2; u < DA_MAX; u++) if (unit_used[u] && !strcmp(unit_of[u], mnt)) return u;
    for (int u = 2; u < DA_MAX; u++) if (!unit_used[u]) { unit_used[u] = 1; strlcpy(unit_of[u], mnt, sizeof unit_of[u]); return u; }
    return DA_MAX - 1;
}
static int scan(da_vol_t *out, int max) {
    int n = getfsstat(0, 0, 0);
    if (n <= 0) return 0;
    if (n > max) n = max;
    da_sf_t *sf = calloc((size_t)n, sizeof *sf);
    if (!sf) return 0;
    n = getfsstat(sf, n * (int)sizeof *sf, 0);
    for (int i = 0; i < n; i++) {
        da_vol_t *v = &out[i];
        memset(v, 0, sizeof *v);
        v->unit = unit_for(sf[i].f_mntonname);
        snprintf(v->bsd, sizeof v->bsd, "disk%ds1", v->unit);
        strlcpy(v->mnt, sf[i].f_mntonname, sizeof v->mnt);
        strlcpy(v->from, sf[i].f_mntfromname, sizeof v->from);
        strlcpy(v->kind, sf[i].f_fstypename, sizeof v->kind);
        v->size = (u64)sf[i].f_blocks * sf[i].f_bsize;
        v->flags = sf[i].f_flags;
    }
    free(sf);
    /* unidades de volumes que saíram ficam livres */
    for (int u = 2; u < DA_MAX; u++) {
        if (!unit_used[u]) continue;
        int still = 0;
        for (int i = 0; i < n; i++) if (out[i].unit == u) still = 1;
        if (!still) unit_used[u] = 0;
    }
    return n;
}

/* ======================================================================== DADisk */
static id cfs(const char *s) { return CFStringCreateWithCString(0, s, 0x08000100u); }
static void set_str(id d, const void *k, const char *v) { id s = cfs(v); if (s) { CFDictionarySetValue(d, k, s); CFRelease(s); } }
static void set_num(id d, const void *k, long long v) { id n = CFNumberCreate(0, 4, &v); if (n) { CFDictionarySetValue(d, k, n); CFRelease(n); } }
static id disk_make(const char *bsd, const char *mnt, const char *kind) {
    id d = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!d) return 0;
    set_str(d, kDA_BSDNAME_KEY, bsd);
    set_str(d, kDA_MNT_KEY, mnt ? mnt : "");
    set_str(d, kDA_KIND_KEY, kind ? kind : "");
    return d;
}
static const char *disk_str(id d, const void *k) { const void *s = d ? CFDictionaryGetValue(d, k) : 0; return s ? cx_cstr((id)s) : 0; }
static da_vol_t *vol_of_disk(id disk, da_vol_t *tmp, int *n) {
    *n = scan(tmp, DA_MAX);
    const char *bsd = disk_str(disk, kDA_BSDNAME_KEY);
    if (!bsd) return 0;
    for (int i = 0; i < *n; i++) {
        if (!strcmp(tmp[i].bsd, bsd)) return &tmp[i];
        char whole[16];
        snprintf(whole, sizeof whole, "disk%d", tmp[i].unit);
        if (!strcmp(whole, bsd)) return &tmp[i];
    }
    return 0;
}

id DASessionCreate(void *alloc) {
    (void)alloc;
    da_consts();
    id s = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (s) set_str(s, kDA_KIND_KEY, "DASession");
    return s;
}
id DAApprovalSessionCreate(void *alloc) { return DASessionCreate(alloc); }
unsigned long DASessionGetTypeID(void) { return 0x4441; }
unsigned long DADiskGetTypeID(void) { return 0x4442; }
unsigned long DADissenterGetTypeID(void) { return 0x4443; }

id DADiskCreateFromBSDName(void *alloc, id session, const char *name) {
    (void)alloc; (void)session;
    if (!name) return 0;
    if (!strncmp(name, "/dev/", 5)) name += 5;
    if (name[0] == 'r' && !strncmp(name + 1, "disk", 4)) name++;          /* /dev/rdiskN */
    da_vol_t t[DA_MAX];
    int n = scan(t, DA_MAX);
    for (int i = 0; i < n; i++) {
        char whole[16];
        snprintf(whole, sizeof whole, "disk%d", t[i].unit);
        if (!strcmp(t[i].bsd, name)) return disk_make(t[i].bsd, t[i].mnt, t[i].kind);
        if (!strcmp(whole, name)) return disk_make(whole, "", "");
    }
    return 0;
}
id DADiskCreateFromVolumePath(void *alloc, id session, id url) {
    (void)alloc; (void)session;
    char p[1024], abs[1024];
    if (!url || !CFURLGetFileSystemRepresentation(url, 1, (u8 *)p, sizeof p)) return 0;
    if (cx_follow_path(p, abs, sizeof abs) < 0) return 0;
    da_sf_t sf;
    if (statfs(abs, &sf) < 0) return 0;
    if (strcmp(abs, sf.f_mntonname)) {                                     /* a Apple pede o ponto de montagem */
        size_t l = strlen(abs);
        if (!(l && abs[l - 1] == '/' && !strncmp(abs, sf.f_mntonname, l - 1))) return 0;
    }
    da_vol_t t[DA_MAX];
    int n = scan(t, DA_MAX);
    for (int i = 0; i < n; i++) if (!strcmp(t[i].mnt, sf.f_mntonname)) return disk_make(t[i].bsd, t[i].mnt, t[i].kind);
    return 0;
}
id DADiskCreateFromIOMedia(void *alloc, id session, unsigned media) { (void)alloc; (void)session; (void)media; return 0; }   /* sem IOKit */
const char *DADiskGetBSDName(id disk) { return disk_str(disk, kDA_BSDNAME_KEY); }
unsigned DADiskCopyIOMedia(id disk) { (void)disk; return 0; }
id DADiskCopyWholeDisk(id disk) {
    const char *b = disk_str(disk, kDA_BSDNAME_KEY);
    if (!b || strncmp(b, "disk", 4)) return 0;
    char w[16];
    int u = atoi(b + 4);
    snprintf(w, sizeof w, "disk%d", u);
    return disk_make(w, "", "");
}
unsigned DADiskGetOptions(id disk) { (void)disk; return 0; }
int DADiskSetOptions(id disk, unsigned o, u8 v) { (void)disk; (void)o; (void)v; return 0; }

id DADiskCopyDescription(id disk) {
    da_vol_t t[DA_MAX];
    int n;
    da_vol_t *v = vol_of_disk(disk, t, &n);
    const char *bsd = disk_str(disk, kDA_BSDNAME_KEY);
    if (!bsd) return 0;
    id d = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!d) return 0;
    int whole = !strchr(bsd + 4, 's');
    int sys = v && v->unit == 0;
    int image = v && v->from[0] == '/' && strncmp(v->from, "/dev/", 5);
    set_str(d, kDADiskDescriptionMediaBSDNameKey, bsd);
    set_num(d, kDADiskDescriptionMediaBSDMajorKey, 1);
    set_num(d, kDADiskDescriptionMediaBSDMinorKey, v ? v->unit * 8 + (whole ? 0 : 1) : 0);
    set_num(d, kDADiskDescriptionMediaBSDUnitKey, v ? v->unit : atoi(bsd + 4));
    set_num(d, kDADiskDescriptionMediaBlockSizeKey, 512);
    set_num(d, kDADiskDescriptionMediaSizeKey, v ? (long long)v->size : 0);
    set_str(d, kDADiskDescriptionMediaNameKey, image ? "Disk Image" : sys ? "Disco do sistema" : "Volume");
    set_str(d, kDADiskDescriptionMediaContentKey, whole ? "GUID_partition_scheme" : (v && !strcmp(v->kind, "msdos")) ? "Microsoft Basic Data" : "Apple_HFS");
    set_str(d, kDADiskDescriptionMediaKindKey, "IOMedia");
    set_str(d, kDADiskDescriptionMediaPathKey, bsd);
    CFDictionarySetValue(d, kDADiskDescriptionMediaWholeKey, whole ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(d, kDADiskDescriptionMediaLeafKey, whole ? kCFBooleanFalse : kCFBooleanTrue);
    CFDictionarySetValue(d, kDADiskDescriptionMediaWritableKey, v && !(v->flags & 1) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(d, kDADiskDescriptionMediaRemovableKey, sys ? kCFBooleanFalse : kCFBooleanTrue);
    CFDictionarySetValue(d, kDADiskDescriptionMediaEjectableKey, sys ? kCFBooleanFalse : kCFBooleanTrue);
    CFDictionarySetValue(d, kDADiskDescriptionMediaEncryptedKey, kCFBooleanFalse);
    set_str(d, kDADiskDescriptionDeviceProtocolKey, image ? "Disk Image" : sys ? "SATA" : "USB");
    CFDictionarySetValue(d, kDADiskDescriptionDeviceInternalKey, sys ? kCFBooleanTrue : kCFBooleanFalse);
    set_str(d, kDADiskDescriptionDeviceModelKey, image ? "Disk Image" : "mOS");
    set_num(d, kDADiskDescriptionDeviceUnitKey, v ? v->unit : 0);
    if (v && !whole) {
        id path = cfs(v->mnt);
        id url = path ? CFURLCreateWithFileSystemPath(0, path, 0, 1) : 0;
        if (url) { CFDictionarySetValue(d, kDADiskDescriptionVolumePathKey, url); CFRelease(url); }
        if (path) CFRelease(path);
        const char *nm = strrchr(v->mnt, '/');
        set_str(d, kDADiskDescriptionVolumeNameKey, sys ? "mOS" : nm && nm[1] ? nm + 1 : v->mnt);
        set_str(d, kDADiskDescriptionVolumeKindKey, v->kind);
        set_str(d, kDADiskDescriptionVolumeTypeKey, !strcmp(v->kind, "hfs") ? "Mac OS Extended" : !strcmp(v->kind, "msdos") ? "MS-DOS (FAT32)" :
                                                   !strcmp(v->kind, "apfs") ? "APFS" : v->kind);
        CFDictionarySetValue(d, kDADiskDescriptionVolumeMountableKey, kCFBooleanTrue);
        CFDictionarySetValue(d, kDADiskDescriptionVolumeNetworkKey, kCFBooleanFalse);
    }
    return d;
}

/* ======================================================================== dissenter */
#define kDAReturnSuccess 0
#define kDAReturnError 0xF8DA0001
#define kDAReturnBusy 0xF8DA0002
#define kDAReturnBadArgument 0xF8DA0003
#define kDAReturnNotPermitted 0xF8DA0007
#define kDAReturnUnsupported 0xF8DA0009
id DADissenterCreate(void *alloc, int status, id str) {
    (void)alloc;
    id d = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!d) return 0;
    set_num(d, kDA_KIND_KEY, (long long)(unsigned)status);
    if (str) CFDictionarySetValue(d, kDA_MNT_KEY, str);
    return d;
}
int DADissenterGetStatus(id d) {
    const void *n = d ? CFDictionaryGetValue(d, kDA_KIND_KEY) : 0;
    long long v = 0;
    extern u8 CFNumberGetValue(id n, long type, void *out);
    if (n) CFNumberGetValue((id)n, 4, &v);
    return (int)v;
}
id DADissenterGetStatusString(id d) { return d ? (id)CFDictionaryGetValue(d, kDA_MNT_KEY) : 0; }

/* ======================================================================== montar, desmontar, ejetar */
typedef void (*da_done_cb)(id disk, id dissenter, void *ctx);
static void finish(da_done_cb cb, id disk, int status, const char *why, void *ctx) {
    if (!cb) return;
    id dis = 0;
    if (status) { id s = why ? cfs(why) : 0; dis = DADissenterCreate(0, status, s); if (s) CFRelease(s); }
    cb(disk, dis, ctx);
    if (dis) CFRelease(dis);
}
void DADiskUnmount(id disk, unsigned opts, da_done_cb cb, void *ctx) {
    (void)opts;
    da_vol_t t[DA_MAX];
    int n;
    da_vol_t *v = vol_of_disk(disk, t, &n);
    if (!v) { finish(cb, disk, (int)kDAReturnBadArgument, "volume não encontrado", ctx); return; }
    if (v->unit == 0) { finish(cb, disk, (int)kDAReturnNotPermitted, "o volume do sistema não pode ser desmontado", ctx); return; }
    long r = SYS1(SYS_IMGDETACH, v->mnt);
    finish(cb, disk, r < 0 ? (int)kDAReturnBusy : 0, r < 0 ? "o volume está em uso" : 0, ctx);
}
void DADiskEject(id disk, unsigned opts, da_done_cb cb, void *ctx) { DADiskUnmount(disk, opts, cb, ctx); }
void DADiskMount(id disk, id path, unsigned opts, da_done_cb cb, void *ctx) {
    (void)path; (void)opts;
    da_vol_t t[DA_MAX];
    int n;
    finish(cb, disk, vol_of_disk(disk, t, &n) ? 0 : (int)kDAReturnBadArgument, "o volume não está montado no mOS", ctx);   /* montado = já está */
}
void DADiskMountWithArguments(id disk, id path, unsigned opts, da_done_cb cb, void *ctx, id *args) { (void)args; DADiskMount(disk, path, opts, cb, ctx); }
void DADiskRename(id disk, id name, unsigned opts, da_done_cb cb, void *ctx) { (void)name; (void)opts; finish(cb, disk, (int)kDAReturnUnsupported, "renomear volume não é suportado", ctx); }
void DADiskClaim(id disk, unsigned opts, void *rel, void *relctx, da_done_cb cb, void *ctx) { (void)opts; (void)rel; (void)relctx; finish(cb, disk, 0, 0, ctx); }
u8 DADiskIsClaimed(id disk) { (void)disk; return 0; }
void DADiskUnclaim(id disk) { (void)disk; }

/* ======================================================================== callbacks de disco */
typedef void (*da_disk_cb)(id disk, void *ctx);
typedef struct { id session, match; da_disk_cb cb; void *ctx; int kind; } da_reg_t;   /* kind 0 apareceu, 1 sumiu, 2 mudou */
#define DA_REGS 32
static da_reg_t regs[DA_REGS];
static int nregs;
static void *sess_queue[DA_REGS];
static id sess_of_queue[DA_REGS];
static da_vol_t known[DA_MAX];
static int nknown, watching, primed;
static volatile int da_lock;
static void lk(void) { while (__sync_lock_test_and_set(&da_lock, 1)) usleep(100); }
static void ulk(void) { __sync_lock_release(&da_lock); }

static int matches(id match, id disk) {
    if (!match) return 1;
    id desc = DADiskCopyDescription(disk);
    if (!desc) return 0;
    static const void **keys[] = {&kDADiskDescriptionVolumeMountableKey, &kDADiskDescriptionMediaWholeKey, &kDADiskDescriptionMediaRemovableKey,
                                  &kDADiskDescriptionVolumeKindKey, &kDADiskDescriptionMediaBSDNameKey};
    int ok = 1;
    for (unsigned i = 0; ok && i < sizeof keys / sizeof *keys; i++) {
        const void *want = CFDictionaryGetValue(match, *keys[i]);
        if (!want) continue;
        const void *have = CFDictionaryGetValue(desc, *keys[i]);
        if (!have) { ok = (want == kCFBooleanFalse); continue; }
        ok = CFEqual((id)want, (id)have);
    }
    CFRelease(desc);
    return ok;
}
typedef struct { da_disk_cb cb; id disk; void *ctx; } da_call_t;
static void call_now(void *p) { da_call_t *c = p; c->cb(c->disk, c->ctx); CFRelease(c->disk); free(c); }
static void deliver(da_reg_t *r, id disk) {
    void *q = 0;
    for (int i = 0; i < DA_REGS; i++) if (sess_of_queue[i] == r->session) q = sess_queue[i];
    da_call_t *c = malloc(sizeof *c);
    if (!c) return;
    c->cb = r->cb; c->disk = CFRetain(disk); c->ctx = r->ctx;
    dispatch_async_f(q ? q : dispatch_get_main_queue(), c, call_now);
}
static void notify(int kind, const da_vol_t *v) {
    id disk = disk_make(v->bsd, v->mnt, v->kind);
    if (!disk) return;
    for (int i = 0; i < nregs; i++) if (regs[i].kind == kind && matches(regs[i].match, disk)) deliver(&regs[i], disk);
    CFRelease(disk);
}
void cx_da_poll_once(void) {                          /* uma volta do vigia (o teste de host chama direto) */
    da_vol_t now[DA_MAX];
    int n = scan(now, DA_MAX);
    lk();
    for (int i = 0; i < n; i++) {
        int seen = 0;
        for (int k = 0; k < nknown; k++) if (!strcmp(known[k].mnt, now[i].mnt)) seen = 1;
        if (!seen) notify(0, &now[i]);
    }
    for (int k = 0; k < nknown; k++) {
        int still = 0;
        for (int i = 0; i < n; i++) if (!strcmp(known[k].mnt, now[i].mnt)) still = 1;
        if (!still) notify(1, &known[k]);
    }
    memcpy(known, now, sizeof(da_vol_t) * (size_t)n);
    nknown = n;
    ulk();
}
static void *watch(void *a) {
    (void)a;
    for (;;) { cx_da_poll_once(); usleep(1000000); }
    return 0;
}
static void start(void) {
    lk();
    if (!primed) { nknown = 0; primed = 1; }          /* a 1ª volta do fio anuncia os que já existem */
    ulk();
    if (!watching) { unsigned long t; watching = 1; pthread_create(&t, 0, watch, 0); }
}
static void reg(id s, id match, void *cb, void *ctx, int kind) {
    lk();
    if (nregs < DA_REGS) { regs[nregs++] = (da_reg_t){s, match ? CFRetain(match) : 0, (da_disk_cb)cb, ctx, kind}; }
    /* registro depois do agendamento: os existentes também chegam (como no macOS) */
    if (kind == 0 && primed) for (int i = 0; i < nknown; i++) {
        id disk = disk_make(known[i].bsd, known[i].mnt, known[i].kind);
        if (disk && matches(match, disk)) deliver(&regs[nregs - 1], disk);
        if (disk) CFRelease(disk);
    }
    ulk();
}
void DARegisterDiskAppearedCallback(id s, id match, void *cb, void *ctx) { reg(s, match, cb, ctx, 0); }
void DARegisterDiskDisappearedCallback(id s, id match, void *cb, void *ctx) { reg(s, match, cb, ctx, 1); }
void DARegisterDiskDescriptionChangedCallback(id s, id match, id watchk, void *cb, void *ctx) { (void)watchk; reg(s, match, cb, ctx, 2); }
void DARegisterDiskEjectApprovalCallback(id s, id match, void *cb, void *ctx) { (void)s; (void)match; (void)cb; (void)ctx; }
void DARegisterDiskMountApprovalCallback(id s, id match, void *cb, void *ctx) { (void)s; (void)match; (void)cb; (void)ctx; }
void DARegisterDiskUnmountApprovalCallback(id s, id match, void *cb, void *ctx) { (void)s; (void)match; (void)cb; (void)ctx; }
void DARegisterDiskPeekCallback(id s, id match, long order, void *cb, void *ctx) { (void)s; (void)match; (void)order; (void)cb; (void)ctx; }
void DAUnregisterCallback(id s, void *cb, void *ctx) {
    (void)s;
    lk();
    for (int i = 0; i < nregs; i++)
        if ((void *)regs[i].cb == cb && regs[i].ctx == ctx) { if (regs[i].match) CFRelease(regs[i].match); regs[i] = regs[--nregs]; i--; }
    ulk();
}
void DAUnregisterApprovalCallback(id s, void *cb, void *ctx) { DAUnregisterCallback(s, cb, ctx); }
void DASessionScheduleWithRunLoop(id s, void *rl, id mode) { (void)s; (void)rl; (void)mode; start(); }
void DASessionUnscheduleFromRunLoop(id s, void *rl, id mode) { (void)s; (void)rl; (void)mode; }
void DAApprovalSessionScheduleWithRunLoop(id s, void *rl, id mode) { DASessionScheduleWithRunLoop(s, rl, mode); }
void DAApprovalSessionUnscheduleFromRunLoop(id s, void *rl, id mode) { (void)s; (void)rl; (void)mode; }
void DASessionSetDispatchQueue(id s, void *q) {
    lk();
    int k = -1;
    for (int i = 0; i < DA_REGS; i++) if (sess_of_queue[i] == s || (!sess_of_queue[i] && k < 0)) { k = i; if (sess_of_queue[i] == s) break; }
    if (k >= 0) { sess_of_queue[k] = q ? s : 0; sess_queue[k] = q; }
    ulk();
    if (q) start();
}
