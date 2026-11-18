/*
 * mOS libSystem — Resource Manager do Carbon (CarbonCore/Resources.h). Lê e grava arquivos de recursos no formato do
 * Mac (cabeçalho, dados, mapa com lista de tipos, de referências e de nomes, tudo big-endian), no resource fork
 * (xattr com.apple.ResourceFork, como o filemgr.c) ou no data fork (.rsrc). Cadeia de arquivos abertos como no Mac:
 * o mais novo fica no topo, GetResource busca do arquivo atual para baixo, Get1Resource só no atual. Cada recurso
 * carregado vira um Handle do Memory Manager (cs_legacy.c), o mesmo a cada pedido até ReleaseResource.
 * Escrita: AddResource, ChangedResource, RemoveResource, SetResInfo/SetResAttrs, WriteResource e UpdateResFile;
 * CloseResFile grava o que mudou. Não há arquivo de recursos do sistema: a cadeia só tem o que o app abriu.
 */
#include "sys.h"
#include "libc.h"

typedef short OSErr;
typedef short ResID;
typedef short ResFileRefNum;
typedef u32 ResType;
typedef char **Handle;
typedef u16 UniChar;
typedef struct { u8 hidden[80]; } FSRef;
Handle NewHandle(long size);
void DisposeHandle(Handle h);
long GetHandleSize(Handle h);
void SetHandleSize(Handle h, long size);
i32 FSRefMakePath(const FSRef *ref, u8 *path, u32 max);
long getxattr(const char *p, const char *n, void *v, size_t s, u32 pos, int o);
int setxattr(const char *p, const char *n, const void *v, size_t s, u32 pos, int o);
int fstat(int fd, void *st);
int snprintf(char *b, size_t n, const char *fmt, ...);

enum { noErr = 0, eofErr = -39, fnfErr = -43, wrPermErr = -61, memFullErr = -108, paramErr = -50, permErr = -54,
       resNotFound = -192, resFNotFound = -193, addResFailed = -194, rmvResFailed = -196, resAttrErr = -198,
       mapReadErr = -199, writErr = -20 };
enum { fsCurPerm = 0, fsRdPerm = 1, fsWrPerm = 2, fsRdWrPerm = 3, fsRdWrShPerm = 4 };
enum { resChanged = 2, resPreload = 4, resProtected = 8, resLocked = 16, resPurgeable = 32, resSysHeap = 64 };
enum { mapReadOnly = 128, mapChanged = 32 };
#define RSRC "com.apple.ResourceFork"
#define O_RDONLY_ 0
#define O_WRONLY_ 1
#define O_CREAT_ 0x200
#define O_TRUNC_ 0x400

typedef struct {
    ResType type;
    ResID id;
    u8 attr;
    u8 name[256];                                                                /* Str255; name[0] = 0 sem nome */
    u8 *data;                                                                    /* bytes do arquivo (ou os gravados) */
    u32 len;
    Handle h;                                                                    /* carregado; 0 se não */
    int changed;
} res_t;
typedef struct rfile {
    struct rfile *next;                                                          /* o próximo mais velho da cadeia */
    ResFileRefNum ref;
    char path[1024];
    int rsrc_fork, writable, dirty;
    u16 fattr;
    res_t *res;
    int n, cap;
} rfile_t;
static rfile_t *g_top, *g_cur;
static OSErr g_err;
static int g_load = 1;
static ResFileRefNum g_next_ref = 0x7000;

OSErr ResError(void) { return g_err; }
void SetResLoad(u8 load) { g_load = load != 0; }
static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u16 be16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }
static void put32(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }
static void put16(u8 *p, u16 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }

static res_t *add_slot(rfile_t *f) {
    if (f->n == f->cap) {
        int c = f->cap ? f->cap * 2 : 16;
        res_t *r = realloc(f->res, (size_t)c * sizeof *r);
        if (!r) return 0;
        f->res = r; f->cap = c;
    }
    res_t *r = &f->res[f->n++];
    memset(r, 0, sizeof *r);
    return r;
}
/* mapa do arquivo de recursos; 0 ok, mapReadErr se malformado */
static OSErr parse(rfile_t *f, const u8 *b, size_t n) {
    if (n < 16) return mapReadErr;
    u32 doff = be32(b), moff = be32(b + 4), dlen = be32(b + 8), mlen = be32(b + 12);
    if ((u64)doff + dlen > n || (u64)moff + mlen > n || mlen < 30) return mapReadErr;
    const u8 *m = b + moff;
    f->fattr = be16(m + 22);
    u32 tl = be16(m + 24), nl = be16(m + 26);
    if (tl + 2 > mlen) return mapReadErr;
    const u8 *t = m + tl;
    int ntypes = (int)(u16)(be16(t) + 1);
    if (be16(t) == 0xffff) ntypes = 0;                                           /* mapa vazio */
    for (int i = 0; i < ntypes; i++) {
        if (tl + 2 + (u32)(i + 1) * 8 > mlen) return mapReadErr;
        const u8 *te = t + 2 + i * 8;
        ResType type = be32(te);
        int cnt = be16(te + 4) + 1;
        u32 rl = be16(te + 6);
        for (int k = 0; k < cnt; k++) {
            u32 ro = tl + rl + (u32)k * 12;
            if (ro + 12 > mlen) return mapReadErr;
            const u8 *re = m + ro;
            res_t *r = add_slot(f);
            if (!r) return memFullErr;
            r->type = type;
            r->id = (ResID)be16(re);
            u16 noff = be16(re + 2);
            r->attr = re[4];
            u32 off = (u32)re[5] << 16 | (u32)re[6] << 8 | re[7];
            if (noff != 0xffff) {
                u32 no = nl + noff;
                if (no >= mlen || no + 1 + m[no] > mlen) return mapReadErr;
                memcpy(r->name, m + no, (size_t)m[no] + 1);
            }
            if ((u64)off + 4 > dlen) return mapReadErr;
            u32 len = be32(b + doff + off);
            if ((u64)off + 4 + len > dlen) return mapReadErr;
            r->data = malloc(len ? len : 1);
            if (!r->data) return memFullErr;
            memcpy(r->data, b + doff + off + 4, len);
            r->len = len;
        }
    }
    return noErr;
}
/* serializa (dados a partir de 256, como o Mac) */
static u8 *serialize(rfile_t *f, size_t *outn) {
    ResType types[512];
    int nt = 0;
    for (int i = 0; i < f->n; i++) {
        int k = 0;
        while (k < nt && types[k] != f->res[i].type) k++;
        if (k == nt) { if (nt == 512) return 0; types[nt++] = f->res[i].type; }
    }
    size_t dlen = 0, names = 0;
    for (int i = 0; i < f->n; i++) {
        res_t *r = &f->res[i];
        u32 len = r->h ? (u32)GetHandleSize(r->h) : r->len;
        dlen += 4 + len;
        if (r->name[0]) names += (size_t)r->name[0] + 1;
    }
    size_t tl = 28, tsize = 2 + (size_t)nt * 8 + (size_t)f->n * 12, mlen = tl + tsize + names;
    size_t total = 256 + dlen + mlen;
    if (total > 0xffffffu || tl + tsize > 0xffff) return 0;
    u8 *b = calloc(1, total);
    if (!b) return 0;
    put32(b, 256); put32(b + 4, (u32)(256 + dlen)); put32(b + 8, (u32)dlen); put32(b + 12, (u32)mlen);
    u8 *m = b + 256 + dlen;
    memcpy(m, b, 16);
    put16(m + 22, f->fattr & (u16)~mapChanged);
    put16(m + 24, (u16)tl); put16(m + 26, (u16)(tl + tsize));
    u8 *t = m + tl;
    put16(t, (u16)(nt - 1));
    size_t doff = 0, noff = 0, refl = 2 + (size_t)nt * 8;
    for (int k = 0; k < nt; k++) {
        u8 *te = t + 2 + k * 8;
        int cnt = 0;
        put32(te, types[k]);
        put16(te + 6, (u16)refl);
        for (int i = 0; i < f->n; i++) {
            res_t *r = &f->res[i];
            if (r->type != types[k]) continue;
            u8 *re = t + refl + (size_t)cnt * 12;
            put16(re, (u16)r->id);
            if (r->name[0]) { put16(re + 2, (u16)noff); memcpy(m + tl + tsize + noff, r->name, (size_t)r->name[0] + 1); noff += (size_t)r->name[0] + 1; }
            else put16(re + 2, 0xffff);
            re[4] = r->attr & (u8)~resChanged;
            re[5] = (u8)(doff >> 16); re[6] = (u8)(doff >> 8); re[7] = (u8)doff;
            const u8 *src = r->h ? (const u8 *)*r->h : r->data;
            u32 len = r->h ? (u32)GetHandleSize(r->h) : r->len;
            put32(b + 256 + doff, len);
            if (len) memcpy(b + 256 + doff + 4, src, len);
            doff += 4 + len;
            cnt++;
        }
        put16(te + 4, (u16)(cnt - 1));
        refl += (size_t)cnt * 12;
    }
    *outn = total;
    return b;
}
static OSErr save(rfile_t *f) {
    if (!f->dirty) return noErr;
    if (!f->writable) return wrPermErr;
    size_t n;
    u8 *b = serialize(f, &n);
    if (!b) return memFullErr;
    OSErr e = noErr;
    if (f->rsrc_fork) e = setxattr(f->path, RSRC, b, n, 0, 0) < 0 ? writErr : noErr;
    else {
        int fd = open(f->path, O_WRONLY_ | O_TRUNC_);
        if (fd < 0) e = fnfErr;
        else { if (write(fd, b, n) != (ssize_t)n) e = writErr; close(fd); }
    }
    free(b);
    if (!e) {
        f->dirty = 0;
        for (int i = 0; i < f->n; i++) {                                         /* o gravado vira o conteúdo de referência */
            res_t *r = &f->res[i];
            if (r->changed && r->h) {
                u32 hn = (u32)GetHandleSize(r->h);
                u8 *d = malloc(hn ? hn : 1);
                if (d) { memcpy(d, *r->h, hn); free(r->data); r->data = d; r->len = hn; }
            }
            r->changed = 0;
            r->attr &= (u8)~resChanged;
        }
    }
    return e;
}
static rfile_t *g_detached;
static rfile_t *file_of(ResFileRefNum ref) { for (rfile_t *f = g_top; f; f = f->next) if (f->ref == ref) return f; return 0; }
static void free_file(rfile_t *f) {
    for (int i = 0; i < f->n; i++) { free(f->res[i].data); if (f->res[i].h) DisposeHandle(f->res[i].h); }
    free(f->res);
    free(f);
}
/* lê o fork inteiro; -1 se não existe */
static long read_fork(const char *path, int rsrc, u8 **out) {
    *out = 0;
    if (rsrc) {
        long n = getxattr(path, RSRC, 0, 0, 0, 0);
        if (n < 0) return -1;
        u8 *b = malloc(n ? (size_t)n : 1);
        if (!b) return -1;
        n = n ? getxattr(path, RSRC, b, (size_t)n, 0, 0) : 0;
        if (n < 0) { free(b); return -1; }
        *out = b;
        return n;
    }
    int fd = open(path, O_RDONLY_);
    if (fd < 0) return -1;
    size_t cap = 65536, n = 0;
    u8 *b = malloc(cap);
    ssize_t r;
    while (b && (r = read(fd, b + n, cap - n)) > 0) {
        n += (size_t)r;
        if (n == cap) { u8 *nb = realloc(b, cap * 2); if (!nb) { free(b); b = 0; break; } b = nb; cap *= 2; }
    }
    close(fd);
    if (!b) return -1;
    *out = b;
    return (long)n;
}
static int fork_is_rsrc(unsigned long nlen, const UniChar *name, int *ok) {
    static const char rf[] = "RESOURCE_FORK";
    *ok = 1;
    if (!nlen || !name) return 0;                                                /* nome vazio = data fork */
    if (nlen == 13) { int same = 1; for (int i = 0; i < 13; i++) if (name[i] != (UniChar)rf[i]) same = 0; if (same) return 1; }
    *ok = 0;                                                                     /* forks com nome: só os dois */
    return 0;
}
static OSErr open_res(const FSRef *ref, int rsrc, i8 perm, ResFileRefNum *out) {
    char path[1024];
    *out = -1;
    if (!ref) return paramErr;
    i32 st = FSRefMakePath(ref, (u8 *)path, sizeof path);
    if (st) return (OSErr)st;
    for (rfile_t *f = g_top; f; f = f->next)                                     /* já aberto: mesmo refnum, vira o atual */
        if (f->rsrc_fork == rsrc && !strcmp(f->path, path)) {
            if ((perm == fsWrPerm || perm == fsRdWrPerm) && !f->writable) return permErr;
            g_cur = f; *out = f->ref;
            return noErr;
        }
    u8 *b;
    long n = read_fork(path, rsrc, &b);
    if (n < 0) return rsrc ? eofErr : fnfErr;
    if (n == 0) { free(b); return eofErr; }
    rfile_t *f = calloc(1, sizeof *f);
    if (!f) { free(b); return memFullErr; }
    OSErr e = parse(f, b, (size_t)n);
    free(b);
    if (e) { free_file(f); return e; }
    snprintf(f->path, sizeof f->path, "%s", path);
    f->rsrc_fork = rsrc;
    f->writable = perm != fsRdPerm && !(f->fattr & mapReadOnly);
    if (f->writable) {                                                           /* confere se dá para gravar */
        int fd = open(path, O_WRONLY_);
        if (fd < 0) { if (perm == fsWrPerm || perm == fsRdWrPerm) { free_file(f); return permErr; } f->writable = 0; }
        else close(fd);
    }
    f->ref = g_next_ref++;
    f->next = g_top; g_top = g_cur = f;
    *out = f->ref;
    return noErr;
}
ResFileRefNum FSOpenResFile(const FSRef *ref, i8 perm) {
    ResFileRefNum r;
    g_err = open_res(ref, 1, perm, &r);
    return g_err ? -1 : r;
}
OSErr FSOpenResourceFile(const FSRef *ref, unsigned long nlen, const UniChar *name, i8 perm, ResFileRefNum *out) {
    int ok, rsrc = fork_is_rsrc(nlen, name, &ok);
    ResFileRefNum r = -1;
    g_err = ok ? open_res(ref, rsrc, perm, &r) : paramErr;
    if (out) *out = r;
    return g_err;
}
/* cria um mapa vazio no fork (o arquivo tem de existir) */
static OSErr create_map(const FSRef *ref, int rsrc) {
    char path[1024];
    i32 st = FSRefMakePath(ref, (u8 *)path, sizeof path);
    if (st) return (OSErr)st;
    u8 *old;
    long n = read_fork(path, rsrc, &old);
    free(old);
    if (n > 0) return noErr;                                                     /* já tem recursos: deixa como está */
    rfile_t tmp;
    memset(&tmp, 0, sizeof tmp);
    snprintf(tmp.path, sizeof tmp.path, "%s", path);
    tmp.rsrc_fork = rsrc; tmp.writable = 1; tmp.dirty = 1;
    return save(&tmp);
}
OSErr FSCreateResourceFork(const FSRef *ref, unsigned long nlen, const UniChar *name, u32 flags) {
    (void)flags;
    int ok, rsrc = fork_is_rsrc(nlen, name, &ok);
    g_err = !ref || !ok ? paramErr : create_map(ref, rsrc);
    return g_err;
}
OSErr FSCreateFileUnicode(const FSRef *parent, unsigned long nlen, const UniChar *name, u32 which, const void *info, FSRef *newRef, void *spec);
OSErr FSCreateResourceFile(const FSRef *parent, unsigned long nlen, const UniChar *name, u32 which, const void *info,
                           unsigned long flen, const UniChar *fork, FSRef *newRef, void *spec) {
    FSRef tmp;
    int ok, rsrc = fork_is_rsrc(flen, fork, &ok);
    if (!ok) return g_err = paramErr;
    OSErr e = FSCreateFileUnicode(parent, nlen, name, which, info, newRef ? newRef : &tmp, spec);
    if (!e) e = create_map(newRef ? newRef : &tmp, rsrc);
    return g_err = e;
}
void FSCreateResFile(const FSRef *parent, unsigned long nlen, const UniChar *name, u32 which, const void *info, FSRef *newRef, void *spec) {
    FSCreateResourceFile(parent, nlen, name, which, info, 13, (const UniChar[]){'R', 'E', 'S', 'O', 'U', 'R', 'C', 'E', '_', 'F', 'O', 'R', 'K'}, newRef, spec);
}
void CloseResFile(ResFileRefNum ref) {
    rfile_t *f = file_of(ref);
    if (!f) {                                                                    /* destacado da cadeia (DetachResourceFile) */
        for (rfile_t **pp = &g_detached; *pp; pp = &(*pp)->next)
            if ((*pp)->ref == ref) { f = *pp; *pp = f->next; g_err = save(f); free_file(f); return; }
        g_err = resFNotFound;
        return;
    }
    g_err = save(f);
    for (rfile_t **pp = &g_top; *pp; pp = &(*pp)->next) if (*pp == f) { *pp = f->next; break; }
    if (g_cur == f) g_cur = f->next ? f->next : g_top;
    free_file(f);
}
ResFileRefNum CurResFile(void) { g_err = noErr; return g_cur ? g_cur->ref : 0; }
void UseResFile(ResFileRefNum ref) {
    rfile_t *f = file_of(ref);
    if (!f) { g_err = resFNotFound; return; }
    g_cur = f; g_err = noErr;
}
OSErr GetTopResourceFile(ResFileRefNum *ref) { if (!ref) return paramErr; *ref = g_top ? g_top->ref : -1; return g_top ? noErr : resFNotFound; }
OSErr GetNextResourceFile(ResFileRefNum cur, ResFileRefNum *next) {
    rfile_t *f = file_of(cur);
    if (!next) return paramErr;
    if (!f) return resFNotFound;
    *next = f->next ? f->next->ref : -1;
    return f->next ? noErr : resFNotFound;
}
void UpdateResFile(ResFileRefNum ref) { rfile_t *f = file_of(ref); g_err = f ? save(f) : resFNotFound; }
short GetResFileAttrs(ResFileRefNum ref) { rfile_t *f = file_of(ref); g_err = f ? noErr : resFNotFound; return f ? (short)f->fattr : 0; }
void SetResFileAttrs(ResFileRefNum ref, short a) { rfile_t *f = file_of(ref); if (!f) { g_err = resFNotFound; return; } f->fattr = (u16)a; f->dirty = 1; g_err = noErr; }

/* ---- busca */
static Handle load(res_t *r) {
    if (r->h) return r->h;
    if (!g_load) {                                                               /* SetResLoad(false): handle vazio */
        r->h = NewHandle(0);
        return r->h;
    }
    r->h = NewHandle((long)r->len);
    if (!r->h) { g_err = memFullErr; return 0; }
    if (r->len) memcpy(*r->h, r->data, r->len);
    return r->h;
}
static res_t *find_in(rfile_t *f, ResType t, int byid, ResID id, const u8 *name) {
    for (int i = 0; i < f->n; i++) {
        res_t *r = &f->res[i];
        if (r->type != t) continue;
        if (byid ? r->id == id : (name && r->name[0] == name[0] && !memcmp(r->name + 1, name + 1, name[0]))) return r;
    }
    return 0;
}
static Handle get(ResType t, int byid, ResID id, const u8 *name, int one) {
    g_err = noErr;
    for (rfile_t *f = g_cur; f; f = one ? 0 : f->next) {
        res_t *r = find_in(f, t, byid, id, name);
        if (r) return load(r);
    }
    g_err = resNotFound;
    return 0;
}
Handle GetResource(ResType t, ResID id) { return get(t, 1, id, 0, 0); }
Handle Get1Resource(ResType t, ResID id) { return get(t, 1, id, 0, 1); }
Handle GetNamedResource(ResType t, const u8 *name) { return get(t, 0, 0, name, 0); }
Handle Get1NamedResource(ResType t, const u8 *name) { return get(t, 0, 0, name, 1); }
static int count_of(rfile_t *f, ResType t) { int n = 0; for (int i = 0; i < f->n; i++) n += f->res[i].type == t; return n; }
short Count1Resources(ResType t) { g_err = noErr; return g_cur ? (short)count_of(g_cur, t) : 0; }
short CountResources(ResType t) { g_err = noErr; int n = 0; for (rfile_t *f = g_cur; f; f = f->next) n += count_of(f, t); return (short)n; }
static Handle ind(ResType t, short index, int one) {
    g_err = noErr;
    int k = 0;
    for (rfile_t *f = g_cur; f && index > 0; f = one ? 0 : f->next)
        for (int i = 0; i < f->n; i++) if (f->res[i].type == t && ++k == index) return load(&f->res[i]);
    g_err = resNotFound;
    return 0;
}
Handle Get1IndResource(ResType t, short index) { return ind(t, index, 1); }
Handle GetIndResource(ResType t, short index) { return ind(t, index, 0); }
/* tipos distintos do arquivo (ou da cadeia), na ordem em que aparecem */
static int types_of(int one, ResType *out, int max) {
    int n = 0;
    for (rfile_t *f = g_cur; f; f = one ? 0 : f->next)
        for (int i = 0; i < f->n; i++) {
            int k = 0;
            while (k < n && out[k] != f->res[i].type) k++;
            if (k == n && n < max) out[n++] = f->res[i].type;
        }
    return n;
}
short Count1Types(void) { ResType t[1024]; g_err = noErr; return (short)types_of(1, t, 1024); }
short CountTypes(void) { ResType t[1024]; g_err = noErr; return (short)types_of(0, t, 1024); }
static void ind_type(ResType *out, short index, int one) {
    ResType t[1024];
    int n = types_of(one, t, 1024);
    g_err = noErr;
    if (!out) { g_err = paramErr; return; }
    if (index < 1 || index > n) { *out = 0; g_err = resNotFound; return; }
    *out = t[index - 1];
}
void Get1IndType(ResType *t, short index) { ind_type(t, index, 1); }
void GetIndType(ResType *t, short index) { ind_type(t, index, 0); }

/* ---- a partir do Handle */
static res_t *res_of(Handle h, rfile_t **owner) {
    if (!h) return 0;
    for (rfile_t *f = g_top; f; f = f->next)
        for (int i = 0; i < f->n; i++) if (f->res[i].h == h) { if (owner) *owner = f; return &f->res[i]; }
    return 0;
}
void GetResInfo(Handle h, ResID *id, ResType *t, u8 *name) {
    res_t *r = res_of(h, 0);
    if (!r) { g_err = resNotFound; return; }
    if (id) *id = r->id;
    if (t) *t = r->type;
    if (name) memcpy(name, r->name, (size_t)r->name[0] + 1);
    g_err = noErr;
}
void SetResInfo(Handle h, ResID id, const u8 *name) {
    rfile_t *f;
    res_t *r = res_of(h, &f);
    if (!r) { g_err = resNotFound; return; }
    if (r->attr & resProtected) { g_err = resAttrErr; return; }
    r->id = id;
    if (name) memcpy(r->name, name, (size_t)name[0] + 1);
    f->dirty = 1; g_err = noErr;
}
short GetResAttrs(Handle h) { res_t *r = res_of(h, 0); g_err = r ? noErr : resNotFound; return r ? r->attr : 0; }
void SetResAttrs(Handle h, short a) {
    rfile_t *f;
    res_t *r = res_of(h, &f);
    if (!r) { g_err = resNotFound; return; }
    r->attr = (u8)a; f->dirty = 1; g_err = noErr;
}
ResFileRefNum HomeResFile(Handle h) { rfile_t *f = 0; g_err = res_of(h, &f) ? noErr : resNotFound; return f ? f->ref : -1; }
long GetResourceSizeOnDisk(Handle h) {
    res_t *r = res_of(h, 0);
    g_err = r ? noErr : resNotFound;
    return r ? (long)(r->changed ? (u32)GetHandleSize(r->h) : r->len) : -1;
}
long GetMaxResourceSize(Handle h) {
    res_t *r = res_of(h, 0);
    if (!r) { g_err = resNotFound; return -1; }
    g_err = noErr;
    long hs = GetHandleSize(h);
    return hs > (long)r->len ? hs : (long)r->len;
}
void LoadResource(Handle h) {
    res_t *r = res_of(h, 0);
    g_err = r ? noErr : resNotFound;
    if (r && r->h && GetHandleSize(r->h) == 0 && r->len) { SetHandleSize(r->h, (long)r->len); memcpy(*r->h, r->data, r->len); }
}
void ReleaseResource(Handle h) {
    rfile_t *f;
    res_t *r = res_of(h, &f);
    if (!r) { g_err = resNotFound; return; }
    if (r->changed) {                                                            /* ainda não gravado: mantém os bytes */
        u32 n = (u32)GetHandleSize(h);
        u8 *d = malloc(n ? n : 1);
        if (d) { memcpy(d, *h, n); free(r->data); r->data = d; r->len = n; }
    }
    DisposeHandle(h);
    r->h = 0;
    g_err = noErr;
}
void DetachResource(Handle h) {
    res_t *r = res_of(h, 0);
    if (!r) { g_err = resNotFound; return; }
    if (r->changed) { g_err = resAttrErr; return; }
    r->h = 0;                                                                    /* o handle agora é do app */
    g_err = noErr;
}
void ChangedResource(Handle h) {
    rfile_t *f;
    res_t *r = res_of(h, &f);
    if (!r) { g_err = resNotFound; return; }
    if (r->attr & resProtected) { g_err = resAttrErr; return; }
    if (!f->writable) { g_err = wrPermErr; return; }
    r->changed = 1; r->attr |= resChanged; f->dirty = 1; g_err = noErr;
}
void WriteResource(Handle h) {
    rfile_t *f;
    res_t *r = res_of(h, &f);
    if (!r) { g_err = resNotFound; return; }
    g_err = r->changed ? save(f) : noErr;
}
void AddResource(Handle h, ResType t, ResID id, const u8 *name) {
    if (!h || !g_cur || res_of(h, 0)) { g_err = addResFailed; return; }
    if (!g_cur->writable) { g_err = wrPermErr; return; }
    res_t *r = add_slot(g_cur);
    if (!r) { g_err = memFullErr; return; }
    r->type = t; r->id = id; r->h = h; r->changed = 1; r->attr = resChanged;
    if (name) memcpy(r->name, name, (size_t)name[0] + 1);
    g_cur->dirty = 1; g_err = noErr;
}
void RemoveResource(Handle h) {
    rfile_t *f;
    res_t *r = res_of(h, &f);
    if (!r) { g_err = rmvResFailed; return; }
    if ((r->attr & resProtected) || !f->writable) { g_err = rmvResFailed; return; }
    free(r->data);
    int i = (int)(r - f->res);
    memmove(&f->res[i], &f->res[i + 1], (size_t)(f->n - i - 1) * sizeof *r);
    f->n--;
    f->dirty = 1; g_err = noErr;                                                 /* o handle fica com o app */
}
static ResID unique(ResType t, int one) {
    static ResID seed = 128;
    for (int tries = 0; tries < 65536; tries++) {
        ResID id = seed++;
        if (seed < 128) seed = 128;
        int used = 0;
        for (rfile_t *f = g_cur; f && !used; f = one ? 0 : f->next) used = find_in(f, t, 1, id, 0) != 0;
        if (!used) return id;
    }
    return 0;
}
ResID UniqueID(ResType t) { g_err = noErr; return unique(t, 0); }
ResID Unique1ID(ResType t) { g_err = noErr; return unique(t, 1); }

/* ---- cadeia: tirar e recolocar arquivos sem fechá-los (não há mapa do sistema nem do app: "abaixo" = no fim) */
enum { kRsrcChainBelowSystemMap = 0, kRsrcChainBelowApplicationMap = 1, kRsrcChainAboveApplicationMap = 2, kRsrcChainAboveAllMaps = 4 };
OSErr DetachResourceFile(ResFileRefNum ref) {
    rfile_t *f = file_of(ref);
    if (!f) return g_err = resFNotFound;
    for (rfile_t **pp = &g_top; *pp; pp = &(*pp)->next) if (*pp == f) { *pp = f->next; break; }
    if (g_cur == f) g_cur = f->next ? f->next : g_top;
    f->next = g_detached; g_detached = f;
    return g_err = noErr;
}
OSErr InsertResourceFile(ResFileRefNum ref, i16 where) {
    rfile_t *f = 0;
    for (rfile_t **pp = &g_detached; *pp; pp = &(*pp)->next) if ((*pp)->ref == ref) { f = *pp; *pp = f->next; break; }
    if (!f) return g_err = file_of(ref) ? noErr : resFNotFound;                 /* já está na cadeia */
    if (where == kRsrcChainAboveAllMaps || where == kRsrcChainAboveApplicationMap || !g_top) { f->next = g_top; g_top = f; }
    else { rfile_t *t = g_top; while (t->next) t = t->next; t->next = f; f->next = 0; }
    if (!g_cur) g_cur = f;
    return g_err = noErr;
}
/* tamanho no disco de um recurso não carregado (corta ou completa com zeros) */
void SetResourceSize(Handle h, long size) {
    rfile_t *f;
    res_t *r = res_of(h, &f);
    if (!r || size < 0) { g_err = r ? paramErr : resNotFound; return; }
    if (r->attr & resProtected) { g_err = resAttrErr; return; }
    u8 *d = realloc(r->data, size ? (size_t)size : 1);
    if (!d) { g_err = memFullErr; return; }
    if ((u32)size > r->len) memset(d + r->len, 0, (size_t)size - r->len);
    r->data = d; r->len = (u32)size;
    f->dirty = 1; g_err = noErr;
}
/* o Memory Manager do mOS não esvazia handles (não há purge): nada a ajustar */
void SetResPurge(u8 install) { (void)install; g_err = noErr; }
/* GetNextFOND: o próximo recurso 'FOND' da cadeia depois deste (a família seguinte); NULL no fim */
Handle GetNextFOND(Handle fond) {
    const ResType FOND = 0x464f4e44u;
    short n = CountResources(FOND);
    for (short i = 1; i <= n; i++)
        if (GetIndResource(FOND, i) == fond) return i < n ? GetIndResource(FOND, (short)(i + 1)) : 0;
    g_err = resNotFound;
    return 0;
}
