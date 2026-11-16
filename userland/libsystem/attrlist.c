/*
 * mOS libSystem — atributos de volume e de objeto do macOS sobre o SYS_FSMETA (kernel/lib/vfsmeta.c, vfsops.c):
 * getattrlist/fgetattrlist/getattrlistat, setattrlist/fsetattrlist/setattrlistat, getattrlistbulk,
 * statfs/fstatfs/getfsstat por volume, renamex_np/renameatx_np (RENAME_SWAP, RENAME_EXCL), clonefile/clonefileat/
 * fclonefileat, exchangedata e fsgetpath.
 *
 * Empacotamento do xnu (vfs_attrlist.c): u32 com o tamanho; com ATTR_CMN_RETURNED_ATTRS, um attribute_set_t; depois
 * os atributos na ordem dos bits (comuns, volume, pasta, arquivo, comuns estendidos), cada um alinhado a 4 bytes;
 * nomes e dados variáveis como attrreference_t {deslocamento a partir da própria referência, tamanho}, no fim.
 * Atributos de pasta num arquivo (e vice-versa) não entram, e saem do RETURNED_ATTRS. Bit desconhecido: EINVAL
 * (com FSOPT_PACK_INVAL_ATTRS, vai zerado e fora do RETURNED_ATTRS). Buffer curto: trunca; o u32 do começo diz
 * quanto foi escrito (com FSOPT_REPORT_FULLSIZE, quanto seria preciso).
 */
#include "sys.h"
#include "libc.h"

#define SET(e) (cx_errno = (e))
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
int cx_follow_path(const char *in, char *out, size_t cap);
const char *cx_at_path(int dirfd, const char *p, char *buf, size_t cap);
int cx_statx(const char *abs, int fd, mos_statx_t *st);
int rename(const char *a, const char *b);
long getxattr(const char *p, const char *n, void *v, size_t s, u32 pos, int o);
long fgetxattr(int fd, const char *n, void *v, size_t s, u32 pos, int o);
int setxattr(const char *p, const char *n, const void *v, size_t s, u32 pos, int o);
int fsetxattr(int fd, const char *n, const void *v, size_t s, u32 pos, int o);
int geteuid(void);
int getegid(void);

typedef struct { unsigned short bitmapcount, reserved; u32 commonattr, volattr, dirattr, fileattr, forkattr; } attrlist_t;
#define FSOPT_NOFOLLOW 0x1
#define FSOPT_REPORT_FULLSIZE 0x4
#define FSOPT_PACK_INVAL_ATTRS 0x8
#define FSOPT_ATTR_CMN_EXTENDED 0x20
#define ATTR_VOL_INFO 0x80000000u
#define ATTR_CMN_RETURNED_ATTRS 0x80000000u
#define ATTR_CMN_ERROR 0x20000000u

/* bits suportados (todos os que o xnu define nestes grupos) */
#define CMN_OK  0xFFFFFFFFu
#define VOL_OK  (0x0003FFFFu | 0x00040000u | 0x00100000u | 0x00200000u | 0x00800000u | 0x10000000u | 0x20000000u | 0x40000000u | ATTR_VOL_INFO)
#define DIR_OK  0x3Fu
#define FILE_OK 0x3FFFu
#define CMNEXT_OK 0x1FFCu

/* ======================================================================== empacotador */
typedef struct {
    u8 fix[2048];   /* cabeçalho + parte fixa */
    size_t nfix;
    u8 *var;        /* dados variáveis */
    size_t nvar, capvar;
    size_t refs[32]; size_t refvar[32]; u32 reflen[32]; int nrefs;
    int err;
} pk_t;
static void pk_put(pk_t *p, const void *d, size_t n) {
    if (p->nfix + n > sizeof p->fix) { p->err = 1; return; }
    memcpy(p->fix + p->nfix, d, n);
    p->nfix += n;
    while (p->nfix & 3) p->fix[p->nfix++] = 0;
}
static void pk_u32(pk_t *p, u32 v) { pk_put(p, &v, 4); }
static void pk_u64(pk_t *p, u64 v) { pk_put(p, &v, 8); }
static void pk_ts(pk_t *p, i64 ns) { i64 t[2] = {ns / 1000000000ll, ns % 1000000000ll}; if (t[1] < 0) { t[0]--; t[1] += 1000000000ll; } pk_put(p, t, 16); }
static void pk_ref(pk_t *p, const void *d, u32 n) {
    if (p->nrefs >= 32) { p->err = 1; return; }
    size_t need = (n + 3u) & ~3u;
    if (p->nvar + need > p->capvar) {
        size_t nc = (p->capvar ? p->capvar * 2 : 512) + need;
        u8 *nb = malloc(nc);
        if (!nb) { p->err = 1; return; }
        if (p->nvar) memcpy(nb, p->var, p->nvar);
        free(p->var);
        p->var = nb;
        p->capvar = nc;
    }
    p->refs[p->nrefs] = p->nfix;
    p->refvar[p->nrefs] = p->nvar;
    p->reflen[p->nrefs] = n;
    p->nrefs++;
    memcpy(p->var + p->nvar, d, n);
    memset(p->var + p->nvar + n, 0, need - n);
    p->nvar += need;
    u32 z[2] = {0, n};
    pk_put(p, z, 8);
}
static void pk_str(pk_t *p, const char *s) { pk_ref(p, s, (u32)strlen(s) + 1); }
/* fecha: corrige as referências, escreve o tamanho e copia para o buffer do usuário */
static int pk_finish(pk_t *p, void *buf, size_t n, unsigned opt, size_t setpos, const u32 set[5]) {
    if (p->err) { free(p->var); SET(12); return -1; }
    for (int i = 0; i < p->nrefs; i++) {
        i32 off = (i32)(p->nfix - p->refs[i] + p->refvar[i]);
        memcpy(p->fix + p->refs[i], &off, 4);
    }
    if (setpos) memcpy(p->fix + setpos, set, 20);
    size_t total = p->nfix + p->nvar;
    size_t give = total < n ? total : n;
    u32 hdr = (u32)((opt & FSOPT_REPORT_FULLSIZE) ? total : give);
    memcpy(p->fix, &hdr, 4);
    if (buf) {
        size_t a = give < p->nfix ? give : p->nfix;
        memcpy(buf, p->fix, a);
        if (give > a) memcpy((u8 *)buf + a, p->var, give - a);
    }
    free(p->var);
    p->var = 0;
    return 0;
}

/* ======================================================================== o objeto */
typedef struct {
    const char *path;       /* absoluto (ou 0 com fd) */
    int fd;
    mos_statx_t x;
    mos_statfs_t vol;
    int have_vol;
    char name[256];
} obj_t;

static int load_obj(obj_t *o, const char *abs, int fd) {
    memset(o, 0, sizeof *o);
    o->path = abs;
    o->fd = fd;
    if (cx_statx(abs, fd, &o->x) < 0) return -1;
    if (abs) {
        const char *s = abs + strlen(abs);
        while (s > abs && s[-1] == '/') s--;
        const char *b = s;
        while (b > abs && b[-1] != '/') b--;
        size_t k = (size_t)(s - b);
        if (k >= sizeof o->name) k = sizeof o->name - 1;
        memcpy(o->name, b, k);
        o->name[k] = 0;
    }
    return 0;
}
static mos_statfs_t *obj_vol(obj_t *o) {
    if (!o->have_vol) {
        mos_fsmeta_t rq = {0};
        rq.fd = o->path ? -1 : o->fd;
        rq.path = (unsigned long long)o->path;
        rq.buf = (unsigned long long)&o->vol;
        rq.size = sizeof o->vol;
        if (SYS2(SYS_FSMETA, MOS_FSM_STATFS, &rq) < 0) {
            strlcpy(o->vol.fstype, "msdos", sizeof o->vol.fstype);
            strlcpy(o->vol.mnton, "/", sizeof o->vol.mnton);
            o->vol.fsid = 0x1000002u;
        }
        o->have_vol = 1;
    }
    return &o->vol;
}
static int is_root(obj_t *o) { mos_statfs_t *v = obj_vol(o); return o->path && !strcmp(o->path, v->mnton); }
static u32 objtag(obj_t *o) {
    const char *t = obj_vol(o)->fstype;
    if (!strcmp(t, "msdos")) return 4;   /* VT_MSDOSFS */
    if (!strcmp(t, "hfs")) return 16;    /* VT_HFS */
    return 24;                           /* VT_OTHER (apfs, tmpfs) */
}
static u32 objtype(const mos_statx_t *x) { return x->link ? 5 : x->kind == 1 ? 2 : 1; }   /* VLNK, VDIR, VREG */
static u32 st_mode_of(const mos_statx_t *x) { return (x->link ? 0120000u : x->kind == 1 ? 0040000u : 0100000u) | (x->mode & 07777u); }
static long obj_xattr(obj_t *o, const char *n, void *b, size_t s) {
    return o->path ? getxattr(o->path, n, b, s, 0, 0x1) : fgetxattr(o->fd, n, b, s, 0, 0);
}
static void finfo(obj_t *o, u8 out[32]) {
    memset(out, 0, 32);
    if (obj_xattr(o, "com.apple.FinderInfo", out, 32) != 32) memset(out, 0, 32);
    if (o->x.flags & 0x8000u) out[8] |= 0x40;   /* UF_HIDDEN -> kIsInvisible */
}
static u32 dir_entries(obj_t *o) {
    if (o->x.kind != 1 || !o->path) return 0;
    mos_fsm_dirent_t de;
    u32 n = 0;
    for (u64 i = 0;; i++) {
        mos_fsmeta_t rq = {0};
        rq.fd = -1; rq.path = (unsigned long long)o->path; rq.pos = i;
        rq.buf = (unsigned long long)&de; rq.size = sizeof de;
        if (SYS2(SYS_FSMETA, MOS_FSM_READDIR, &rq) != 1) break;
        n++;
    }
    return n;
}
static u32 user_access(obj_t *o) {
    u32 r = 0;
    for (int b = 4; b; b >>= 1) {
        mos_fsmeta_t rq = {0};
        rq.fd = o->path ? -1 : o->fd; rq.path = (unsigned long long)o->path; rq.a = (unsigned)b; rq.opts = 1;
        if (SYS2(SYS_FSMETA, MOS_FSM_ACCESS, &rq) == 0) r |= (u32)b;
    }
    return r;
}

/* vol_capabilities_attr_t: formatos e interfaces que o mOS tem de verdade */
#define CAP_FORMAT (0x1u | 0x2u | 0x200u | 0x400u | 0x800u | 0x2000u | 0x4000u | 0x20000u)
#define CAP_INTERFACES (0x2u | 0x8u | 0x10u | 0x20u | 0x400u | 0x800u | 0x4000u | 0x10000u | 0x40000u | 0x80000u)

static void pack_vol(pk_t *p, obj_t *o, u32 va) {
    mos_statfs_t *v = obj_vol(o);
    u64 bs = v->bsize ? v->bsize : 4096;
    if (va & 0x1) pk_u32(p, objtag(o));                                   /* FSTYPE */
    if (va & 0x2) pk_u32(p, !strcmp(v->fstype, "hfs") ? 0x482B : !strcmp(v->fstype, "msdos") ? 0x4D53 : 0x4144);   /* SIGNATURE */
    if (va & 0x4) pk_u64(p, v->total);                                    /* SIZE */
    if (va & 0x8) pk_u64(p, v->avail);                                    /* SPACEFREE */
    if (va & 0x10) pk_u64(p, v->avail);                                   /* SPACEAVAIL */
    if (va & 0x20) pk_u64(p, bs);                                         /* MINALLOCATION */
    if (va & 0x40) pk_u64(p, bs);                                         /* ALLOCATIONCLUMP */
    if (va & 0x80) pk_u32(p, (u32)bs);                                    /* IOBLOCKSIZE */
    if (va & 0x100) pk_u32(p, (u32)(v->files - v->ffree));                /* OBJCOUNT */
    if (va & 0x200) pk_u32(p, (u32)(v->files - v->ffree));                /* FILECOUNT */
    if (va & 0x400) pk_u32(p, 0);                                         /* DIRCOUNT */
    if (va & 0x800) pk_u32(p, (u32)v->files);                             /* MAXOBJCOUNT */
    if (va & 0x1000) pk_str(p, v->mnton);                                 /* MOUNTPOINT */
    if (va & 0x2000) pk_str(p, v->volname);                               /* NAME */
    if (va & 0x4000) pk_u32(p, v->flags);                                 /* MOUNTFLAGS */
    if (va & 0x8000) pk_str(p, v->mntfrom);                               /* MOUNTEDDEVICE */
    if (va & 0x10000) pk_u64(p, 0x1);                                     /* ENCODINGSUSED: MacRoman */
    if (va & 0x20000) {                                                   /* CAPABILITIES */
        u32 c[8] = {CAP_FORMAT | (v->case_sensitive ? 0x100u : 0), CAP_INTERFACES, 0, 0, 0x03FFFFFFu, 0x001FFFFFu, 0, 0};
        pk_put(p, c, 32);
    }
    if (va & 0x40000) {                                                   /* UUID: do fsid, estável */
        u8 u[16] = {0x6d, 0x4f, 0x53, 0x56, 0x4f, 0x4c, 0x40, 0x00, 0x80, 0, 0, 0, 0, 0, 0, 0};
        memcpy(u + 12, &v->fsid, 4);
        pk_put(p, u, 16);
    }
    if (va & 0x100000) pk_str(p, v->fstype);                              /* FSTYPENAME */
    if (va & 0x200000) pk_u32(p, 0);                                      /* FSSUBTYPE */
    if (va & 0x800000) pk_u64(p, v->total - v->avail);                    /* SPACEUSED */
    if (va & 0x10000000) pk_u64(p, 0);                                    /* QUOTA_SIZE */
    if (va & 0x20000000) pk_u64(p, 0);                                    /* RESERVED_SIZE */
    if (va & 0x40000000) {                                                /* ATTRIBUTES: os grupos que este código responde */
        u32 a[10] = {CMN_OK, VOL_OK, DIR_OK, FILE_OK, CMNEXT_OK, CMN_OK, VOL_OK, DIR_OK, FILE_OK, CMNEXT_OK};
        pk_put(p, a, 40);
    }
}

/* empacota um objeto; set = o que foi devolvido (RETURNED_ATTRS) */
static void pack_obj(pk_t *p, obj_t *o, const attrlist_t *al, unsigned opt, u32 set[5], int bulk) {
    mos_statx_t *x = &o->x;
    u32 ca = al->commonattr & ~ATTR_CMN_RETURNED_ATTRS;
    if (!bulk) ca &= ~ATTR_CMN_ERROR;
    set[0] = ca | (al->commonattr & ATTR_CMN_RETURNED_ATTRS);
    if (ca & 0x1) pk_str(p, is_root(o) && !bulk ? obj_vol(o)->volname : o->name);   /* NAME */
    if (ca & 0x2) pk_u32(p, (u32)x->dev);                                 /* DEVID */
    if (ca & 0x4) { u32 f[2] = {(u32)x->dev, objtag(o)}; pk_put(p, f, 8); }   /* FSID */
    if (ca & 0x8) pk_u32(p, objtype(x));                                  /* OBJTYPE */
    if (ca & 0x10) pk_u32(p, objtag(o));                                  /* OBJTAG */
    if (ca & 0x20) { u32 f[2] = {(u32)x->ino, 0}; pk_put(p, f, 8); }      /* OBJID */
    if (ca & 0x40) { u32 f[2] = {(u32)x->ino, 0}; pk_put(p, f, 8); }      /* OBJPERMANENTID */
    if (ca & 0x80) { u32 f[2] = {(u32)x->parent, 0}; pk_put(p, f, 8); }   /* PAROBJID */
    if (ca & 0x100) pk_u32(p, 0);                                         /* SCRIPT: kTextEncodingMacRoman */
    if (ca & 0x200) pk_ts(p, x->btime_ns);                                /* CRTIME */
    if (ca & 0x400) pk_ts(p, x->mtime_ns);                                /* MODTIME */
    if (ca & 0x800) pk_ts(p, x->ctime_ns);                                /* CHGTIME */
    if (ca & 0x1000) pk_ts(p, x->atime_ns);                               /* ACCTIME */
    if (ca & 0x2000) pk_ts(p, 0);                                         /* BKUPTIME */
    if (ca & 0x4000) { u8 fi[32]; finfo(o, fi); pk_put(p, fi, 32); }      /* FNDRINFO */
    if (ca & 0x8000) pk_u32(p, x->uid);                                   /* OWNERID */
    if (ca & 0x10000) pk_u32(p, x->gid);                                  /* GRPID */
    if (ca & 0x20000) pk_u32(p, st_mode_of(x));                           /* ACCESSMASK */
    if (ca & 0x40000) pk_u32(p, x->flags);                                /* FLAGS */
    if (ca & 0x80000) pk_u32(p, 0);                                       /* GEN_COUNT */
    if (ca & 0x100000) pk_u32(p, 0);                                      /* DOCUMENT_ID */
    if (ca & 0x200000) pk_u32(p, user_access(o));                         /* USERACCESS */
    if (ca & 0x400000) {                                                  /* EXTENDED_SECURITY: kauth_filesec */
        u8 fs[44 + 24 * 32];
        mos_fsmeta_t rq = {0};
        rq.fd = o->path ? -1 : o->fd; rq.path = (unsigned long long)o->path;
        rq.buf = (unsigned long long)fs; rq.size = sizeof fs;
        long n = SYS2(SYS_FSMETA, MOS_FSM_GETACL, &rq);
        if (n > 0) pk_ref(p, fs, (u32)n);
        else { set[0] &= ~0x400000u; u32 z[2] = {0, 0}; pk_put(p, z, 8); }
    }
    if (ca & 0x800000) { u8 g[16]; memcpy(g, "\xFF\xFF\xEE\xEE\xDD\xDD\xCC\xCC\xBB\xBB\xAA\xAA", 12); g[12] = (u8)(x->uid >> 24); g[13] = (u8)(x->uid >> 16); g[14] = (u8)(x->uid >> 8); g[15] = (u8)x->uid; pk_put(p, g, 16); }   /* UUID */
    if (ca & 0x1000000) { u8 g[16]; memcpy(g, "\xAB\xCD\xEF\xAB\xCD\xEF\xAB\xCD\xEF\xAB\xCD\xEF", 12); g[12] = (u8)(x->gid >> 24); g[13] = (u8)(x->gid >> 16); g[14] = (u8)(x->gid >> 8); g[15] = (u8)x->gid; pk_put(p, g, 16); }   /* GRPUUID */
    if (ca & 0x2000000) pk_u64(p, x->ino);                                /* FILEID */
    if (ca & 0x4000000) pk_u64(p, x->parent);                             /* PARENTID */
    if (ca & 0x8000000) pk_str(p, o->path ? o->path : "");                /* FULLPATH */
    if (ca & 0x10000000) pk_ts(p, x->btime_ns);                           /* ADDEDTIME: o mOS não separa; a criação */
    if (ca & ATTR_CMN_ERROR) pk_u32(p, 0);                                /* ERROR (bulk) */
    if (ca & 0x40000000) pk_u32(p, 0);                                    /* DATA_PROTECT_FLAGS */
    if (al->volattr) { pack_vol(p, o, al->volattr & ~ATTR_VOL_INFO); set[1] = al->volattr; }
    u64 alloc = (x->size + 4095) & ~4095ull;
    if (x->kind == 1) {
        u32 da = al->dirattr;
        set[2] = da;
        if (da & 0x1) pk_u32(p, x->nlink);                                /* LINKCOUNT */
        if (da & 0x2) pk_u32(p, dir_entries(o));                          /* ENTRYCOUNT */
        if (da & 0x4) pk_u32(p, is_root(o) ? 1 : 0);                      /* MOUNTSTATUS: DIR_MNTSTATUS_MNTPOINT */
        if (da & 0x8) pk_u64(p, 0);                                       /* ALLOCSIZE */
        if (da & 0x10) pk_u32(p, 4096);                                   /* IOBLOCKSIZE */
        if (da & 0x20) pk_u64(p, 0);                                      /* DATALENGTH */
    } else {
        u32 fa = al->fileattr;
        set[3] = fa;
        long rsrc = obj_xattr(o, "com.apple.ResourceFork", 0, 0);
        if (rsrc < 0) rsrc = 0;
        if (fa & 0x1) pk_u32(p, x->nlink);                                /* LINKCOUNT */
        if (fa & 0x2) pk_u64(p, x->size + (u64)rsrc);                     /* TOTALSIZE */
        if (fa & 0x4) pk_u64(p, alloc + (((u64)rsrc + 4095) & ~4095ull)); /* ALLOCSIZE */
        if (fa & 0x8) pk_u32(p, 4096);                                    /* IOBLOCKSIZE */
        if (fa & 0x10) pk_u32(p, 4096);                                   /* CLUMPSIZE */
        if (fa & 0x20) pk_u32(p, 0);                                      /* DEVTYPE */
        if (fa & 0x40) pk_u32(p, 0);                                      /* FILETYPE */
        if (fa & 0x80) pk_u32(p, rsrc ? 2 : 1);                           /* FORKCOUNT */
        if (fa & 0x100) { u32 z[2] = {0, 0}; pk_put(p, z, 8); set[3] &= ~0x100u; }   /* FORKLIST: obsoleto */
        if (fa & 0x200) pk_u64(p, x->size);                               /* DATALENGTH */
        if (fa & 0x400) pk_u64(p, alloc);                                 /* DATAALLOCSIZE */
        if (fa & 0x800) { u8 z[64] = {0}; pk_put(p, z, 64); set[3] &= ~0x800u; }      /* DATAEXTENTS: obsoleto */
        if (fa & 0x1000) pk_u64(p, (u64)rsrc);                            /* RSRCLENGTH */
        if (fa & 0x2000) pk_u64(p, ((u64)rsrc + 4095) & ~4095ull);        /* RSRCALLOCSIZE */
    }
    if (opt & FSOPT_ATTR_CMN_EXTENDED) {
        u32 ea = al->forkattr;
        set[4] = ea;
        if (ea & 0x4) pk_str(p, o->path ? o->path : "");                  /* RELPATH */
        if (ea & 0x8) pk_u64(p, x->kind == 1 ? 0 : alloc);                /* PRIVATESIZE */
        if (ea & 0x10) pk_u64(p, x->ino);                                 /* LINKID */
        if (ea & 0x20) pk_str(p, o->path ? o->path : "");                 /* NOFIRMLINKPATH */
        if (ea & 0x40) pk_u32(p, (u32)x->dev);                            /* REALDEVID */
        if (ea & 0x80) { u32 f[2] = {(u32)x->dev, objtag(o)}; pk_put(p, f, 8); }   /* REALFSID */
        if (ea & 0x100) pk_u64(p, x->ino);                                /* CLONEID */
        if (ea & 0x200) pk_u64(p, 0);                                     /* EXT_FLAGS */
        if (ea & 0x400) pk_u64(p, 0);                                     /* RECURSIVE_GENCOUNT */
        if (ea & 0x800) pk_u64(p, 0);                                     /* ATTRIBUTION_TAG */
        if (ea & 0x1000) pk_u32(p, 1);                                    /* CLONE_REFCNT */
    }
}

static int check_list(const attrlist_t *al, unsigned opt, int bulk) {
    if (!al) { SET(14); return -1; }
    if (al->bitmapcount != 5) { SET(22); return -1; }
    int pack_inval = opt & FSOPT_PACK_INVAL_ATTRS;
    u32 fork_ok = (opt & FSOPT_ATTR_CMN_EXTENDED) ? CMNEXT_OK : 0;
    if (!pack_inval && ((al->commonattr & ~CMN_OK) || (al->volattr & ~VOL_OK) || (al->dirattr & ~DIR_OK) ||
                        (al->fileattr & ~FILE_OK) || (al->forkattr & ~fork_ok))) { SET(22); return -1; }
    (void)0;   /* ATTR_VOL_INFO: a Apple pede, mas há quem não ponha; aceito sem ele */
    if (bulk && (al->volattr || !(al->commonattr & ATTR_CMN_RETURNED_ATTRS))) { SET(22); return -1; }
    return 0;
}

static int getattr_obj(obj_t *o, const attrlist_t *al, void *buf, size_t n, unsigned opt) {
    pk_t *p = calloc(1, sizeof *p);
    if (!p) { SET(12); return -1; }
    u32 set[5] = {0};
    size_t setpos = 0;
    p->nfix = 4;
    if (al->commonattr & ATTR_CMN_RETURNED_ATTRS) { setpos = p->nfix; u8 z[20] = {0}; pk_put(p, z, 20); }
    attrlist_t a = *al;
    if (opt & FSOPT_PACK_INVAL_ATTRS) { a.commonattr &= CMN_OK; a.volattr &= VOL_OK; a.dirattr &= DIR_OK; a.fileattr &= FILE_OK; a.forkattr &= CMNEXT_OK; }
    pack_obj(p, o, &a, opt, set, 0);
    int r = pk_finish(p, buf, n, opt, setpos, set);
    free(p);
    return r;
}

int cx_getattrlist(const char *path, void *al, void *buf, size_t n, unsigned opt) {
    char abs[1024];
    obj_t o;
    if (!path) { SET(14); return -1; }
    if (check_list(al, opt, 0) < 0) return -1;
    if (opt & FSOPT_NOFOLLOW) cx_path(path, abs, sizeof abs);
    else if (cx_follow_path(path, abs, sizeof abs) < 0) return -1;
    if (load_obj(&o, abs, -1) < 0) return -1;
    return getattr_obj(&o, al, buf, n, opt);
}
int getattrlist(const char *p, void *al, void *buf, size_t n, unsigned opt) { return cx_getattrlist(p, al, buf, n, opt); }
int fgetattrlist(int fd, void *al, void *buf, size_t n, unsigned opt) {
    char p[1024];
    obj_t o;
    if (check_list(al, opt, 0) < 0) return -1;
    long r = SYS3(SYS_FDPATH, fd, p, sizeof p);
    if (r < 0) { SET((int)-r); return -1; }
    if (load_obj(&o, p, -1) < 0) return -1;    /* pelo caminho do fd: nome e pasta-mãe ficam certos */
    return getattr_obj(&o, al, buf, n, opt);
}
int getattrlistat(int dirfd, const char *p, void *al, void *buf, size_t n, unsigned long opt) {
    char tp[1024];
    if (!p) { SET(14); return -1; }
    const char *full = cx_at_path(dirfd, p, tp, sizeof tp);
    return full ? cx_getattrlist(full, al, buf, n, (unsigned)opt) : -1;
}

/* getattrlistbulk: as entradas da pasta do fd a partir do offset do fd (que esta função avança, como o xnu);
 * lseek(fd, 0, SEEK_SET) recomeça */
long lseek(int fd, long off, int whence);
int getattrlistbulk(int dirfd, void *alv, void *buf, size_t n, u64 opt) {
    const attrlist_t *al = alv;
    char dir[1024];
    if (check_list(al, (unsigned)opt, 1) < 0) return -1;
    if (!buf) { SET(14); return -1; }
    long r = SYS3(SYS_FDPATH, dirfd, dir, sizeof dir);
    if (r < 0) { SET(9); return -1; }
    long cur = lseek(dirfd, 0, 1);
    if (cur < 0) cur = 0;
    struct { u64 pos; } st = {(u64)cur}, *bc = &st;
    size_t used = 0;
    int count = 0;
    mos_fsm_dirent_t de;
    pk_t *p = calloc(1, sizeof *p);
    if (!p) { SET(12); return -1; }
    for (;;) {
        mos_fsmeta_t rq = {0};
        rq.fd = -1; rq.path = (unsigned long long)dir; rq.pos = bc->pos;
        rq.buf = (unsigned long long)&de; rq.size = sizeof de;
        long k = SYS2(SYS_FSMETA, MOS_FSM_READDIR, &rq);
        if (k < 0) { if (!count) { free(p); SET((int)-k); return -1; } break; }
        if (k == 0) break;
        char full[1300];
        strlcpy(full, dir, sizeof full);
        if (full[strlen(full) - 1] != '/') strlcat(full, "/", sizeof full);
        strlcat(full, de.name, sizeof full);
        obj_t o;
        u32 set[5] = {0};
        memset(p, 0, sizeof *p);
        p->nfix = 4;
        size_t setpos = p->nfix;
        { u8 z[20] = {0}; pk_put(p, z, 20); }
        if (load_obj(&o, full, -1) < 0) {
            if (!(al->commonattr & ATTR_CMN_ERROR)) { bc->pos++; continue; }
            set[0] = ATTR_CMN_RETURNED_ATTRS | ATTR_CMN_ERROR;
            pk_u32(p, (u32)cx_errno);
        } else pack_obj(p, &o, al, (unsigned)opt, set, 1);
        if (p->err) break;
        size_t len = (p->nfix + p->nvar + 7) & ~7ull;     /* entradas alinhadas a 8 */
        if (used + len > n) { free(p->var); p->var = 0; break; }
        u8 *ent = (u8 *)buf + used;
        memset(ent, 0, len);
        pk_finish(p, ent, len, FSOPT_REPORT_FULLSIZE, setpos, set);
        memcpy(ent, &(u32){(u32)len}, 4);
        used += len;
        count++;
        bc->pos++;
    }
    free(p);
    lseek(dirfd, (long)bc->pos, 0);
    return count;
}

/* ======================================================================== setattrlist */
static int setattr_obj(const char *abs, int fd, const attrlist_t *a, const void *buf, size_t n) {
    if (!a || !buf) { SET(14); return -1; }
    if (a->bitmapcount != 5) { SET(22); return -1; }
    const u32 cmn_set = 0x100u | 0x200u | 0x400u | 0x800u | 0x1000u | 0x2000u | 0x4000u | 0x8000u | 0x10000u | 0x20000u | 0x40000u |
                        0x400000u | 0x800000u | 0x1000000u | 0x10000000u;
    if (a->volattr & ~(ATTR_VOL_INFO | 0x2000u)) { SET(22); return -1; }
    if (a->dirattr || a->fileattr || a->forkattr || (a->commonattr & ~cmn_set)) { SET(22); return -1; }
    mos_statx_t x;
    if (cx_statx(abs, fd, &x) < 0) return -1;
    const u8 *p = buf;
    size_t off = 0;
    mos_fsmeta_t tm = {0};
    tm.fd = abs ? -1 : fd; tm.path = (unsigned long long)abs;
    u32 uid = 0xffffffffu, gid = 0xffffffffu;
    int have_mode = 0, have_flags = 0, have_finfo = 0;
    u32 mode = 0, flags = 0;
    u8 fi[32];
    const u8 *acl = 0; u32 acl_len = 0;
    for (u32 bit = 1; bit; bit <<= 1) {
        if (!(a->commonattr & bit)) continue;
        size_t sz = (bit >= 0x200u && bit <= 0x2000u) || bit == 0x10000000u ? 16 : bit == 0x4000u ? 32 :
                    bit == 0x400000u ? 8 : (bit == 0x800000u || bit == 0x1000000u) ? 16 : 4;
        if (off + sz > n) { SET(22); return -1; }
        const u8 *v = p + off;
        i64 ts[2];
        switch (bit) {
        case 0x200u: memcpy(ts, v, 16); tm.a |= MOS_FSM_BTIME; tm.t[3] = ts[0] * 1000000000ll + ts[1]; break;
        case 0x400u: memcpy(ts, v, 16); tm.a |= MOS_FSM_MTIME; tm.t[1] = ts[0] * 1000000000ll + ts[1]; break;
        case 0x800u: break;   /* CHGTIME: o xnu ignora (só o sistema de arquivos muda) */
        case 0x1000u: memcpy(ts, v, 16); tm.a |= MOS_FSM_ATIME; tm.t[0] = ts[0] * 1000000000ll + ts[1]; break;
        case 0x4000u: memcpy(fi, v, 32); have_finfo = 1; break;
        case 0x8000u: memcpy(&uid, v, 4); break;
        case 0x10000u: memcpy(&gid, v, 4); break;
        case 0x20000u: memcpy(&mode, v, 4); have_mode = 1; break;
        case 0x40000u: memcpy(&flags, v, 4); have_flags = 1; break;
        case 0x400000u: { i32 ro; u32 rl; memcpy(&ro, v, 4); memcpy(&rl, v + 4, 4);
                          if (ro < 0 || off + (size_t)ro + rl > n) { SET(22); return -1; }
                          acl = v + ro; acl_len = rl; break; }
        case 0x800000u: case 0x1000000u: {                    /* UUID/GRPUUID: dono ou grupo pelo GUID */
            if (!memcmp(v, "\xFF\xFF\xEE\xEE\xDD\xDD\xCC\xCC\xBB\xBB\xAA\xAA", 12) && bit == 0x800000u)
                uid = (u32)v[12] << 24 | (u32)v[13] << 16 | (u32)v[14] << 8 | v[15];
            else if (!memcmp(v, "\xAB\xCD\xEF\xAB\xCD\xEF\xAB\xCD\xEF\xAB\xCD\xEF", 12) && bit == 0x1000000u)
                gid = (u32)v[12] << 24 | (u32)v[13] << 16 | (u32)v[14] << 8 | v[15];
            else { SET(22); return -1; }
            break;
        }
        default: break;                                       /* SCRIPT, BKUPTIME, ADDEDTIME: aceitos e sem efeito */
        }
        off += sz;
    }
    if (a->volattr & 0x2000u) { SET(30); return -1; }             /* renomear volume: EROFS (não há rótulo gravável) */
    long r;
    if (uid != 0xffffffffu || gid != 0xffffffffu) {
        mos_fsmeta_t rq = tm; rq.a = uid; rq.b = gid;
        if ((r = SYS2(SYS_FSMETA, MOS_FSM_CHOWN, &rq)) < 0) { SET((int)-r); return -1; }
    }
    if (have_mode) {
        if (abs) r = SYS2(SYS_CHMOD, abs, mode & 07777);
        else r = SYS2(SYS_FCHMOD, fd, mode & 07777);
        if (r < 0) { SET((int)-r); return -1; }
    }
    if (have_finfo) {
        r = abs ? setxattr(abs, "com.apple.FinderInfo", fi, 32, 0, 0x1) : fsetxattr(fd, "com.apple.FinderInfo", fi, 32, 0, 0);
        if (r < 0) return -1;
    }
    if (acl) {
        mos_fsmeta_t rq = tm; rq.buf = (unsigned long long)acl; rq.size = acl_len;
        if ((r = SYS2(SYS_FSMETA, MOS_FSM_SETACL, &rq)) < 0) { SET((int)-r); return -1; }
    }
    if (tm.a) { if ((r = SYS2(SYS_FSMETA, MOS_FSM_SETTIMES, &tm)) < 0) { SET((int)-r); return -1; } }
    if (have_flags) {                                         /* por último: uchg depois de gravar o resto */
        mos_fsmeta_t rq = tm; rq.a = flags;
        if ((r = SYS2(SYS_FSMETA, MOS_FSM_CHFLAGS, &rq)) < 0) { SET((int)-r); return -1; }
    }
    return 0;
}
int setattrlist(const char *p, void *al, void *buf, size_t n, unsigned opts) {
    char abs[1024];
    if (!p) { SET(14); return -1; }
    if (opts & FSOPT_NOFOLLOW) cx_path(p, abs, sizeof abs);
    else if (cx_follow_path(p, abs, sizeof abs) < 0) return -1;
    return setattr_obj(abs, -1, al, buf, n);
}
int fsetattrlist(int fd, void *al, void *buf, size_t n, unsigned opts) { (void)opts; return setattr_obj(0, fd, al, buf, n); }
int setattrlistat(int dirfd, const char *p, void *al, void *buf, size_t n, unsigned opts) {
    char tp[1024];
    if (!p) { SET(14); return -1; }
    const char *full = cx_at_path(dirfd, p, tp, sizeof tp);
    return full ? setattrlist(full, al, buf, n, opts) : -1;
}

/* ======================================================================== statfs ($INODE64, 2168 bytes) */
typedef struct {
    u32 f_bsize; i32 f_iosize; u64 f_blocks, f_bfree, f_bavail, f_files, f_ffree; i32 f_fsid[2]; u32 f_owner;
    u32 f_type, f_flags, f_fssubtype; char f_fstypename[16], f_mntonname[1024], f_mntfromname[1024]; u32 f_flags_ext, f_reserved[7];
} sf_t;
static int fill_statfs(const char *abs, int fd, sf_t *s) {
    mos_statfs_t v;
    mos_fsmeta_t rq = {0};
    rq.fd = abs ? -1 : fd; rq.path = (unsigned long long)abs;
    rq.buf = (unsigned long long)&v; rq.size = sizeof v;
    long r = SYS2(SYS_FSMETA, MOS_FSM_STATFS, &rq);
    if (r < 0) { SET((int)-r); return -1; }
    memset(s, 0, sizeof *s);
    s->f_bsize = v.bsize ? v.bsize : 4096;
    s->f_iosize = 1 << 20;
    s->f_blocks = v.total / s->f_bsize;
    s->f_bfree = s->f_bavail = v.avail / s->f_bsize;
    s->f_files = v.files;
    s->f_ffree = v.ffree;
    s->f_fsid[0] = (i32)v.fsid;
    s->f_fsid[1] = !strcmp(v.fstype, "msdos") ? 0x1c : !strcmp(v.fstype, "hfs") ? 0x11 : 0x1a;
    s->f_owner = 0;
    s->f_type = (u32)s->f_fsid[1];
    s->f_flags = v.flags;
    strlcpy(s->f_fstypename, v.fstype, sizeof s->f_fstypename);
    strlcpy(s->f_mntonname, v.mnton, sizeof s->f_mntonname);
    strlcpy(s->f_mntfromname, v.mntfrom, sizeof s->f_mntfromname);
    return 0;
}
int statfs(const char *path, sf_t *s) {
    char abs[1024];
    if (cx_follow_path(path, abs, sizeof abs) < 0) return -1;
    return fill_statfs(abs, -1, s);
}
int fstatfs(int fd, sf_t *s) {
    char p[1024];
    if (SYS3(SYS_FDPATH, fd, p, sizeof p) < 0) return fill_statfs("/", -1, s);   /* pipe, terminal: o volume raiz */
    return fill_statfs(p, -1, s);
}
/* getfsstat: o volume do sistema e cada /Volumes/<nome> */
int getfsstat(void *buf, int bufsize, int flags) {
    (void)flags;
    char paths[33][300];
    int n = 0;
    strlcpy(paths[n++], "/", sizeof paths[0]);
    mos_fsm_dirent_t de;
    for (u64 i = 0; n < 33; i++) {
        mos_fsmeta_t rq = {0};
        rq.fd = -1; rq.path = (unsigned long long)"/Volumes"; rq.pos = i;
        rq.buf = (unsigned long long)&de; rq.size = sizeof de;
        if (SYS2(SYS_FSMETA, MOS_FSM_READDIR, &rq) != 1) break;
        char p[300];
        strlcpy(p, "/Volumes/", sizeof p);
        strlcat(p, de.name, sizeof p);
        mos_statfs_t v;
        mos_fsmeta_t q = {0};
        q.fd = -1; q.path = (unsigned long long)p; q.buf = (unsigned long long)&v; q.size = sizeof v;
        if (SYS2(SYS_FSMETA, MOS_FSM_STATFS, &q) == 0 && !strcmp(v.mnton, p)) strlcpy(paths[n++], p, sizeof paths[0]);
    }
    if (!buf) return n;
    int k = 0;
    for (int i = 0; i < n && (k + 1) * (int)sizeof(sf_t) <= bufsize; i++)
        if (fill_statfs(paths[i], -1, (sf_t *)buf + k) == 0) k++;
    return k;
}
int getmntinfo(sf_t **mntbufp, int flags) {
    static sf_t *tab;
    int n = getfsstat(0, 0, flags);
    free(tab);
    tab = calloc((size_t)(n > 0 ? n : 1), sizeof *tab);
    if (!tab) { SET(12); return 0; }
    n = getfsstat(tab, n * (int)sizeof *tab, flags);
    *mntbufp = tab;
    return n;
}

/* ======================================================================== renamex_np, clonefile, exchangedata, fsgetpath */
#define RENAME_SWAP 0x2
#define RENAME_EXCL 0x4
#define RENAME_NOFOLLOW_ANY 0x10
static long fsm2(int op, const char *a, const char *b, u64 flags) {
    char pa[1024], pb[1024];
    cx_path(a, pa, sizeof pa);
    cx_path(b, pb, sizeof pb);
    mos_fsmeta_t rq = {0};
    rq.fd = -1; rq.path = (unsigned long long)pa; rq.name = (unsigned long long)pb; rq.a = flags;
    long r = SYS2(SYS_FSMETA, op, &rq);
    if (r < 0) { SET((int)-r); return -1; }
    return r;
}
int renamex_np(const char *a, const char *b, unsigned flags) {
    if (!a || !b) { SET(14); return -1; }
    long r = fsm2(MOS_FSM_RENAMEX, a, b, flags & (RENAME_SWAP | RENAME_EXCL));
    if (r < 0) return -1;
    return r == 1 ? rename(a, b) : 0;                      /* 1 = rename comum (sem SWAP; EXCL já conferido) */
}
int renameatx_np(int fa, const char *a, int fb, const char *b, unsigned flags) {
    char ta[1024], tb[1024];
    const char *qa = cx_at_path(fa, a, ta, sizeof ta), *qb = qa ? cx_at_path(fb, b, tb, sizeof tb) : 0;
    return qb ? renamex_np(qa, qb, flags) : -1;
}
#define CLONE_NOFOLLOW 0x1
#define CLONE_NOOWNERCOPY 0x2
int clonefile(const char *src, const char *dst, u32 flags) {
    char s[1024];
    if (!src || !dst) { SET(14); return -1; }
    if (flags & CLONE_NOFOLLOW) cx_path(src, s, sizeof s);
    else if (cx_follow_path(src, s, sizeof s) < 0) return -1;
    return fsm2(MOS_FSM_CLONE, s, dst, flags & CLONE_NOOWNERCOPY) < 0 ? -1 : 0;
}
int clonefileat(int sfd, const char *src, int dfd, const char *dst, u32 flags) {
    char ts[1024], td[1024];
    const char *qs = cx_at_path(sfd, src, ts, sizeof ts), *qd = qs ? cx_at_path(dfd, dst, td, sizeof td) : 0;
    return qd ? clonefile(qs, qd, flags) : -1;
}
int fclonefileat(int src, int dirfd, const char *dst, u32 flags) {
    char sp[1024], td[1024];
    if (SYS3(SYS_FDPATH, src, sp, sizeof sp) < 0) { SET(9); return -1; }
    const char *qd = cx_at_path(dirfd, dst, td, sizeof td);
    return qd ? fsm2(MOS_FSM_CLONE, sp, qd, flags & CLONE_NOOWNERCOPY) < 0 ? -1 : 0 : -1;
}
int exchangedata(const char *a, const char *b, unsigned opt) {
    char pa[1024], pb[1024];
    if (!a || !b) { SET(14); return -1; }
    if (opt & FSOPT_NOFOLLOW) { cx_path(a, pa, sizeof pa); cx_path(b, pb, sizeof pb); }
    else if (cx_follow_path(a, pa, sizeof pa) < 0 || cx_follow_path(b, pb, sizeof pb) < 0) return -1;
    return fsm2(MOS_FSM_EXCHANGE, pa, pb, 0) < 0 ? -1 : 0;
}
/* fsgetpath: o caminho de um inode (o fsid não importa: os inodes do mOS são únicos no VFS inteiro) */
long fsgetpath(char *buf, size_t n, void *fsid, u64 objid) {
    (void)fsid;
    mos_fsmeta_t rq = {0};
    rq.a = objid;
    rq.buf = (unsigned long long)buf;
    rq.size = n;
    long r = SYS2(SYS_FSMETA, MOS_FSM_FSGETPATH, &rq);
    if (r < 0) { SET((int)-r); return -1; }
    return r;
}
