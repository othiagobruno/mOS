/* mOS — DiskArbitration do libSystem (userland/libsystem/diskarb.c) com CF e volumes falsos no host:
 *   sh tools/diskarb_test.sh */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
typedef uint8_t u8; typedef uint32_t u32; typedef uint64_t u64;
typedef void *id;
/* ---- CF falso: string, número, booleano, URL e dicionário (chave comparada por CFEqual) */
typedef struct { int t; char *s; long long n; id k[64], v[64]; int nk; int rc; } obj_t;
enum { TS = 1, TN, TB, TU, TD };
static obj_t T_ = {TB, 0, 1, {0}, {0}, 0, 1}, F_ = {TB, 0, 0, {0}, {0}, 0, 1};
void *kCFBooleanTrue = &T_, *kCFBooleanFalse = &F_;
const char kCFTypeDictionaryKeyCallBacks = 0, kCFTypeDictionaryValueCallBacks = 0;
char cx_cfconst_isa[1] __asm__("__CFConstantStringClassReference");
static obj_t *mk(int t) { obj_t *o = calloc(1, sizeof *o); o->t = t; o->rc = 1; return o; }
id CFStringCreateWithCString(void *a, const char *s, u32 e) { (void)a; (void)e; obj_t *o = mk(TS); o->s = strdup(s); return o; }
/* as constantes do diskarb.c são {isa, flags, char *s, n}: o 3º campo é o texto */
static const char *str_of(id x) { obj_t *o = x; if (o && o->t == TS) return o->s; if (o && o->t == TU) return o->s; return x ? ((const char **)x)[2] : 0; }
static int is_obj(id x) { obj_t *o = x; return o && o->t >= TS && o->t <= TD && o->rc > 0 && o->rc < 1000; }
const char *cx_cstr(id s) { return str_of(s); }
id CFNumberCreate(void *a, long t, const void *p) { (void)a; (void)t; obj_t *o = mk(TN); o->n = *(const long long *)p; return o; }
u8 CFNumberGetValue(id n, long t, void *out) { (void)t; *(long long *)out = ((obj_t *)n)->n; return 1; }
id CFURLCreateWithFileSystemPath(void *a, id p, long st, u8 d) { (void)a; (void)st; (void)d; obj_t *o = mk(TU); o->s = strdup(str_of(p)); return o; }
u8 CFURLGetFileSystemRepresentation(id u, u8 r, u8 *b, long m) { (void)r; snprintf((char *)b, (size_t)m, "%s", str_of(u)); return 1; }
u8 CFEqual(id a, id b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (is_obj(a) && is_obj(b) && ((obj_t *)a)->t == TN && ((obj_t *)b)->t == TN) return ((obj_t *)a)->n == ((obj_t *)b)->n;
    const char *x = str_of(a), *y = str_of(b);
    return x && y && !strcmp(x, y);
}
id CFDictionaryCreateMutable(void *a, long c, const void *k, const void *v) { (void)a; (void)c; (void)k; (void)v; return mk(TD); }
void CFDictionarySetValue(id d, const void *k, const void *v) {
    obj_t *o = d;
    for (int i = 0; i < o->nk; i++) if (CFEqual(o->k[i], (id)k)) { o->v[i] = (id)v; return; }
    o->k[o->nk] = (id)k; o->v[o->nk++] = (id)v;
}
const void *CFDictionaryGetValue(id d, const void *k) { obj_t *o = d; for (int i = 0; i < o->nk; i++) if (CFEqual(o->k[i], (id)k)) return o->v[i]; return 0; }
long CFDictionaryGetCount(id d) { return ((obj_t *)d)->nk; }
id CFRetain(id o) { if (is_obj(o)) ((obj_t *)o)->rc++; return o; }
void CFRelease(id o) { if (is_obj(o)) ((obj_t *)o)->rc--; }
/* ---- sistema falso */
int cx_errno;
void *dispatch_get_main_queue(void) { return (void *)1; }
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *)) { (void)q; fn(ctx); }   /* entrega na hora */
int pthread_create(unsigned long *t, const void *a, void *(*fn)(void *), void *arg) { (void)t; (void)a; (void)fn; (void)arg; return 11; }
int usleep(unsigned us) { (void)us; return 0; }
int cx_follow_path(const char *in, char *out, size_t cap) { snprintf(out, cap, "%s", in); return 0; }
typedef struct {
    u32 f_bsize; int32_t f_iosize; u64 f_blocks, f_bfree, f_bavail, f_files, f_ffree; int32_t f_fsid[2]; u32 f_owner;
    u32 f_type, f_flags, f_fssubtype; char f_fstypename[16], f_mntonname[1024], f_mntfromname[1024]; u32 f_flags_ext, f_reserved[7];
} sf_t;
static sf_t vols[4];
static int nv;
static void addvol(const char *mnt, const char *from, const char *kind, u32 flags) {
    sf_t *s = &vols[nv++]; memset(s, 0, sizeof *s);
    s->f_bsize = 4096; s->f_blocks = 1000; s->f_flags = flags;
    snprintf(s->f_mntonname, sizeof s->f_mntonname, "%s", mnt);
    snprintf(s->f_mntfromname, sizeof s->f_mntfromname, "%s", from);
    snprintf(s->f_fstypename, sizeof s->f_fstypename, "%s", kind);
}
int getfsstat(void *b, int sz, int fl) { (void)fl; if (!b) return nv; int k = sz / (int)sizeof(sf_t); if (k > nv) k = nv; memcpy(b, vols, sizeof(sf_t) * (size_t)k); return k; }
int statfs(const char *p, void *out) {
    sf_t *best = 0;
    for (int i = 0; i < nv; i++) { size_t l = strlen(vols[i].f_mntonname); if (!strncmp(p, vols[i].f_mntonname, l) && (!best || l > strlen(best->f_mntonname))) best = &vols[i]; }
    if (!best) return -1;
    memcpy(out, best, sizeof *best);
    return 0;
}
static const char *detached;
long host_sys(long n, long a, long b, long c) { (void)b; (void)c; if (n == 55) { detached = (const char *)a; return 0; } return -22; }

/* ---- a API */
id DASessionCreate(void *); id DADiskCreateFromBSDName(void *, id, const char *); id DADiskCreateFromVolumePath(void *, id, id);
const char *DADiskGetBSDName(id); id DADiskCopyDescription(id); id DADiskCopyWholeDisk(id);
void DADiskUnmount(id, unsigned, void (*)(id, id, void *), void *); int DADissenterGetStatus(id);
void DARegisterDiskAppearedCallback(id, id, void *, void *); void DARegisterDiskDisappearedCallback(id, id, void *, void *);
void DASessionSetDispatchQueue(id, void *); void cx_da_poll_once(void);
extern const void *kDADiskDescriptionVolumeNameKey, *kDADiskDescriptionVolumeKindKey, *kDADiskDescriptionMediaBSDNameKey,
    *kDADiskDescriptionMediaWritableKey, *kDADiskDescriptionMediaRemovableKey, *kDADiskDescriptionDeviceProtocolKey,
    *kDADiskDescriptionVolumePathKey, *kDADiskDescriptionMediaWholeKey, *kDADiskDescriptionMatchVolumeMountable;
static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALHA %d: %s\n", __LINE__, #c); } } while (0)
static int appeared, gone;
static char last[64];
static void on_app(id d, void *ctx) { (void)ctx; appeared++; snprintf(last, sizeof last, "%s", DADiskGetBSDName(d)); }
static void on_gone(id d, void *ctx) { (void)ctx; gone++; snprintf(last, sizeof last, "%s", DADiskGetBSDName(d)); }
static int un_status = -1;
static void on_un(id d, id dis, void *ctx) { (void)d; (void)ctx; un_status = dis ? DADissenterGetStatus(dis) : 0; }

int main(void) {
    addvol("/", "/dev/disk0s1", "msdos", 0x1000 | 0x4000);
    addvol("/Volumes/App", "/Users/Visitante/Downloads/App.dmg", "hfs", 1);
    id s = DASessionCreate(0);
    CHECK(s != 0);
    id d0 = DADiskCreateFromBSDName(0, s, "disk0s1");
    CHECK(d0 && !strcmp(DADiskGetBSDName(d0), "disk0s1"));
    id url = CFURLCreateWithFileSystemPath(0, CFStringCreateWithCString(0, "/Volumes/App", 0), 0, 1);
    id d1 = DADiskCreateFromVolumePath(0, s, url);
    CHECK(d1 && !strcmp(DADiskGetBSDName(d1), "disk2s1"));
    id sub = CFURLCreateWithFileSystemPath(0, CFStringCreateWithCString(0, "/Volumes/App/Pasta", 0), 0, 1);
    CHECK(DADiskCreateFromVolumePath(0, s, sub) == 0);                     /* precisa ser o ponto de montagem */
    id ds = DADiskCopyDescription(d1);
    CHECK(ds && !strcmp(cx_cstr((id)CFDictionaryGetValue(ds, kDADiskDescriptionVolumeNameKey)), "App"));
    CHECK(!strcmp(cx_cstr((id)CFDictionaryGetValue(ds, kDADiskDescriptionVolumeKindKey)), "hfs"));
    CHECK(!strcmp(cx_cstr((id)CFDictionaryGetValue(ds, kDADiskDescriptionDeviceProtocolKey)), "Disk Image"));
    CHECK(CFDictionaryGetValue(ds, kDADiskDescriptionMediaWritableKey) == kCFBooleanFalse);   /* montado só leitura */
    CHECK(CFDictionaryGetValue(ds, kDADiskDescriptionMediaRemovableKey) == kCFBooleanTrue);
    CHECK(!strcmp(cx_cstr((id)CFDictionaryGetValue(ds, kDADiskDescriptionVolumePathKey)), "/Volumes/App"));
    id ds0 = DADiskCopyDescription(d0);
    CHECK(!strcmp(cx_cstr((id)CFDictionaryGetValue(ds0, kDADiskDescriptionDeviceProtocolKey)), "SATA"));
    CHECK(CFDictionaryGetValue(ds0, kDADiskDescriptionMediaRemovableKey) == kCFBooleanFalse);
    id w = DADiskCopyWholeDisk(d1);
    CHECK(w && !strcmp(DADiskGetBSDName(w), "disk2"));
    id dw = DADiskCopyDescription(w);
    CHECK(CFDictionaryGetValue(dw, kDADiskDescriptionMediaWholeKey) == kCFBooleanTrue && !CFDictionaryGetValue(dw, kDADiskDescriptionVolumePathKey));
    CHECK(DADiskCreateFromBSDName(0, s, "/dev/rdisk2s1") != 0);
    /* desmontar: o do sistema não; o da imagem sim, pelo SYS_IMGDETACH */
    DADiskUnmount(d0, 0, on_un, 0);
    CHECK(un_status == (int)0xF8DA0007);
    DADiskUnmount(d1, 0, on_un, 0);
    CHECK(un_status == 0 && detached && !strcmp(detached, "/Volumes/App"));
    /* callbacks: os existentes chegam ao agendar; um novo e um que sai */
    DARegisterDiskAppearedCallback(s, (id)kDADiskDescriptionMatchVolumeMountable, (void *)on_app, 0);
    DARegisterDiskDisappearedCallback(s, 0, (void *)on_gone, 0);
    DASessionSetDispatchQueue(s, (void *)2);
    cx_da_poll_once();
    CHECK(appeared == 2);
    addvol("/Volumes/Pen", "/dev/disk3s1", "msdos", 0);
    cx_da_poll_once();
    CHECK(appeared == 3 && !strcmp(last, "disk3s1"));
    nv = 2;                                                              /* o pendrive saiu */
    cx_da_poll_once();
    CHECK(gone == 1 && !strcmp(last, "disk3s1"));
    cx_da_poll_once();
    CHECK(appeared == 3 && gone == 1);
    printf("diskarb: %d verificações, %d falha(s)\n", checks, fails);
    return fails != 0;
}
