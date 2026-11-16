/*
 * mOS libSystem — bookmarks, arquivos de alias e URLs de referência de arquivo.
 *
 * Formato "book" do macOS (o do CFURLCreateBookmarkData desde o 10.6), escrito do zero a partir da descrição pública
 * do formato:
 *   cabeçalho: "book", u32 tamanho total, u32 0x10040000, u32 tamanho do cabeçalho (48), 32 bytes zerados;
 *   dados (deslocamentos relativos ao fim do cabeçalho): u32 com o deslocamento do TOC; itens { u32 tamanho,
 *   u32 tipo, dados alinhados a 4 } (0x0101 texto, 0x0201 dados, 0x0303/0x0304 número 32/64, 0x0400 data em
 *   double big-endian desde 2001, 0x0500/0x0501 falso/verdadeiro, 0x0601 lista de deslocamentos, 0x0901 URL);
 *   TOC: u32 (12 + 12*n), 0xfffffffe, u32 id (1), u32 próximo TOC (0), u32 n, n × { u32 chave, u32 desloc., 0 }.
 * Chaves: 0x1004 componentes do caminho, 0x1005 inodes do caminho, 0x1010 propriedades do arquivo, 0x1040 criação,
 *   0x2002 caminho do volume, 0x2005 URL do volume, 0x2010 nome, 0x2011 UUID, 0x2012 tamanho, 0x2013 criação,
 *   0x2020 propriedades do volume, 0x2030 é a raiz, 0xc001 índice da pasta, 0xc011 usuário, 0xc012 uid,
 *   0xd001 era URL de referência, 0xd010 opções, 0xf080 extensão de escopo de segurança.
 * Resolver: pelo caminho, conferindo o inode; se o arquivo foi movido ou renomeado, pelo inode (fsgetpath) e o
 * bookmark fica "velho" (isStale). Alias do Finder: o próprio bookmark no conteúdo, FinderInfo 'alis'/'MACS' com
 * kIsAlias. URL de referência: "file:///.file/id=<volume>.<inode>/" (cx_path resolve).
 */
#include "sys.h"
#include "libc.h"

#define SET(e) (cx_errno = (e))
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
int cx_follow_path(const char *in, char *out, size_t cap);
int cx_statx(const char *abs, int fd, mos_statx_t *st);
long fsgetpath(char *buf, size_t n, void *fsid, u64 objid);
long getxattr(const char *p, const char *n, void *v, size_t s, u32 pos, int o);
int setxattr(const char *p, const char *n, const void *v, size_t s, u32 pos, int o);
int getuid(void);
const char *getlogin(void);

/* ======================================================================== escrita */
typedef struct { u8 *b; size_t n, cap; int err; } bw_t;
static size_t bw_reserve(bw_t *w, size_t k) {
    if (w->err) return 0;
    if (w->n + k > w->cap) {
        size_t nc = (w->cap ? w->cap * 2 : 1024) + k;
        u8 *nb = realloc(w->b, nc);
        if (!nb) { w->err = 1; return 0; }
        w->b = nb;
        w->cap = nc;
    }
    size_t at = w->n;
    memset(w->b + at, 0, k);
    w->n += k;
    return at;
}
static void p32(u8 *p, u32 v) { memcpy(p, &v, 4); }
static u32 g32(const u8 *p) { u32 v; memcpy(&v, p, 4); return v; }
static u64 g64(const u8 *p) { u64 v; memcpy(&v, p, 8); return v; }
#define HDR 48
/* item: devolve o deslocamento relativo ao começo dos dados */
static u32 item(bw_t *w, u32 type, const void *d, u32 len) {
    size_t at = bw_reserve(w, 8 + ((len + 3u) & ~3u));
    if (w->err) return 0;
    p32(w->b + at, len);
    p32(w->b + at + 4, type);
    if (len) memcpy(w->b + at + 8, d, len);
    return (u32)(at - HDR);
}
static u32 it_str(bw_t *w, const char *s) { return item(w, 0x0101, s, (u32)strlen(s)); }
static u32 it_i64(bw_t *w, i64 v) { return item(w, 0x0304, &v, 8); }
static u32 it_i32(bw_t *w, i32 v) { return item(w, 0x0303, &v, 4); }
static u32 it_bool(bw_t *w, int v) { return item(w, v ? 0x0501 : 0x0500, 0, 0); }
static u32 it_date(bw_t *w, i64 unix_ns) {
    double d = (double)unix_ns / 1e9 - 978307200.0;     /* segundos desde 1/1/2001 */
    u64 bits; memcpy(&bits, &d, 8);
    u8 be[8];
    for (int i = 0; i < 8; i++) be[i] = (u8)(bits >> (56 - 8 * i));
    return item(w, 0x0400, be, 8);
}
static u32 it_list(bw_t *w, const u32 *offs, u32 n) { return item(w, 0x0601, offs, n * 4); }

#define BMK_PATH 0x1004
#define BMK_CNIDS 0x1005
#define BMK_FILEPROPS 0x1010
#define BMK_CRTIME 0x1040
#define BMK_VOLPATH 0x2002
#define BMK_VOLURL 0x2005
#define BMK_VOLNAME 0x2010
#define BMK_VOLUUID 0x2011
#define BMK_VOLSIZE 0x2012
#define BMK_VOLCRTIME 0x2013
#define BMK_VOLPROPS 0x2020
#define BMK_VOLROOT 0x2030
#define BMK_FOLDERIDX 0xc001
#define BMK_USER 0xc011
#define BMK_UID 0xc012
#define BMK_WASREF 0xd001
#define BMK_OPTIONS 0xd010
#define BMK_SECURITY 0xf080

/* propriedades do arquivo (kCFURLResourceIs*): os bits do campo 0x1010 */
#define FP_REGULAR 0x1
#define FP_DIR 0x2
#define FP_LINK 0x4
#define FP_VOLUME 0x8
#define FP_PACKAGE 0x10
#define FP_HIDDEN 0x80
#define FP_APP 0x200
#define FP_ALIAS 0x8000

static int ends_with(const char *s, const char *e) { size_t a = strlen(s), b = strlen(e); return a >= b && !strcmp(s + a - b, e); }

int cx_alias_is(const char *abs) {
    u8 fi[32];
    if (getxattr(abs, "com.apple.FinderInfo", fi, 32, 0, 0x1) != 32) return 0;
    return !memcmp(fi, "alis", 4) && (fi[8] & 0x80);    /* kIsAlias = 0x8000 nos finderFlags (big-endian) */
}

/* cria um bookmark de abs (que precisa existir). opts: as NSURLBookmarkCreationOptions. */
long cx_book_create(const char *path, unsigned opts, int was_ref, u8 **out) {
    char abs[1024];
    mos_statx_t x;
    if (cx_follow_path(path, abs, sizeof abs) < 0) return -1;
    if (cx_statx(abs, -1, &x) < 0) return -1;
    bw_t w = {0};
    bw_reserve(&w, HDR + 4);                             /* cabeçalho + deslocamento do TOC */
    /* componentes e inodes do caminho */
    u32 comp[128], cnid[128], nc = 0;
    char part[1024];
    strlcpy(part, abs, sizeof part);
    {
        const char *s = abs;
        char pre[1024]; size_t pl = 0;
        while (*s && nc < 128) {
            while (*s == '/') s++;
            if (!*s) break;
            const char *e = s;
            while (*e && *e != '/') e++;
            char nm[256]; size_t k = (size_t)(e - s) < sizeof nm - 1 ? (size_t)(e - s) : sizeof nm - 1;
            memcpy(nm, s, k); nm[k] = 0;
            if (pl + 1 + k + 1 < sizeof pre) { pre[pl++] = '/'; memcpy(pre + pl, nm, k); pl += k; pre[pl] = 0; }
            mos_statx_t cx;
            u64 ino = cx_statx(pre, -1, &cx) == 0 ? cx.ino : 0;
            comp[nc] = it_str(&w, nm);
            cnid[nc] = it_i64(&w, (i64)ino);
            nc++;
            s = e;
        }
    }
    u32 o_path = it_list(&w, comp, nc), o_cnid = it_list(&w, cnid, nc);
    u64 fp[3] = {0, 0xFFFF, 0};
    fp[0] = x.link ? FP_LINK : x.kind == 1 ? FP_DIR : FP_REGULAR;
    if (x.kind == 1 && (ends_with(abs, ".app") || ends_with(abs, ".bundle") || ends_with(abs, ".framework"))) fp[0] |= FP_PACKAGE;
    if (x.kind == 1 && ends_with(abs, ".app")) fp[0] |= FP_APP;
    if (x.flags & 0x8000u) fp[0] |= FP_HIDDEN;
    if (x.kind != 1 && cx_alias_is(abs)) fp[0] |= FP_ALIAS;
    if (!strcmp(abs, "/")) fp[0] |= FP_VOLUME;
    u32 o_fprops = item(&w, 0x0201, fp, 24);
    u32 o_crtime = it_date(&w, x.btime_ns);
    /* volume */
    mos_statfs_t v;
    memset(&v, 0, sizeof v);
    {
        mos_fsmeta_t rq = {0};
        rq.fd = -1; rq.path = (unsigned long long)abs; rq.buf = (unsigned long long)&v; rq.size = sizeof v;
        if (SYS2(SYS_FSMETA, MOS_FSM_STATFS, &rq) < 0) { strlcpy(v.mnton, "/", sizeof v.mnton); strlcpy(v.volname, "mOS", sizeof v.volname); v.fsid = 0x1000002u; }
    }
    char uuid[40], vurl[300];
    {
        static const char hx[] = "0123456789ABCDEF";
        u8 u[16] = {0x6d, 0x4f, 0x53, 0x56, 0x4f, 0x4c, 0x40, 0x00, 0x80, 0, 0, 0, 0, 0, 0, 0};
        memcpy(u + 12, &v.fsid, 4);
        int o = 0;
        for (int i = 0; i < 16; i++) { if (i == 4 || i == 6 || i == 8 || i == 10) uuid[o++] = '-'; uuid[o++] = hx[u[i] >> 4]; uuid[o++] = hx[u[i] & 15]; }
        uuid[o] = 0;
        strlcpy(vurl, "file://", sizeof vurl);
        strlcat(vurl, v.mnton, sizeof vurl);
        if (vurl[strlen(vurl) - 1] != '/') strlcat(vurl, "/", sizeof vurl);
    }
    u32 o_volpath = it_str(&w, v.mnton), o_volurl = item(&w, 0x0901, vurl, (u32)strlen(vurl));
    u32 o_volname = it_str(&w, v.volname), o_voluuid = it_str(&w, uuid), o_volsize = it_i64(&w, (i64)v.total);
    u32 o_volcr = it_date(&w, 0);
    u64 vp[3] = {0x81 | (strcmp(v.mnton, "/") ? 0 : 0x2), 0xFF, 0};   /* local, interno (raiz) */
    u32 o_volprops = item(&w, 0x0201, vp, 24);
    u32 o_volroot = it_bool(&w, !strcmp(v.mnton, "/"));
    u32 o_folder = it_i32(&w, nc >= 2 ? (i32)nc - 2 : 0);
    const char *user = getlogin();
    u32 o_user = it_str(&w, user ? user : "visitante"), o_uid = it_i32(&w, getuid());
    u32 o_wasref = it_bool(&w, was_ref), o_opts = it_i32(&w, (i32)opts);
    u32 o_sec = 0;
    if (opts & (1u << 11)) {                             /* NSURLBookmarkCreationWithSecurityScope */
        char ext[1200];
        strlcpy(ext, (opts & (1u << 12)) ? "com.apple.app-sandbox.read;" : "com.apple.app-sandbox.read-write;", sizeof ext);
        strlcat(ext, abs, sizeof ext);
        o_sec = item(&w, 0x0201, ext, (u32)strlen(ext) + 1);
    }
    struct { u32 k, o; } toc[24] = {
        {BMK_PATH, o_path}, {BMK_CNIDS, o_cnid}, {BMK_FILEPROPS, o_fprops}, {BMK_CRTIME, o_crtime},
        {BMK_VOLPATH, o_volpath}, {BMK_VOLURL, o_volurl}, {BMK_VOLNAME, o_volname}, {BMK_VOLUUID, o_voluuid},
        {BMK_VOLSIZE, o_volsize}, {BMK_VOLCRTIME, o_volcr}, {BMK_VOLPROPS, o_volprops}, {BMK_VOLROOT, o_volroot},
        {BMK_FOLDERIDX, o_folder}, {BMK_USER, o_user}, {BMK_UID, o_uid}, {BMK_WASREF, o_wasref}, {BMK_OPTIONS, o_opts},
    };
    u32 nt = 17;
    if (o_sec) { toc[nt].k = BMK_SECURITY; toc[nt].o = o_sec; nt++; }
    size_t t = bw_reserve(&w, 20 + 12 * nt);
    if (w.err) { free(w.b); SET(12); return -1; }
    p32(w.b + t, 12 + 12 * nt);
    p32(w.b + t + 4, 0xfffffffeu);
    p32(w.b + t + 8, 1);
    p32(w.b + t + 12, 0);
    p32(w.b + t + 16, nt);
    for (u32 i = 0; i < nt; i++) { p32(w.b + t + 20 + 12 * i, toc[i].k); p32(w.b + t + 24 + 12 * i, toc[i].o); }
    memcpy(w.b, "book", 4);
    p32(w.b + 4, (u32)w.n);
    p32(w.b + 8, 0x10040000u);
    p32(w.b + 12, HDR);
    p32(w.b + HDR, (u32)(t - HDR));
    *out = w.b;
    return (long)w.n;
}

/* ======================================================================== leitura */
typedef struct { const u8 *d; size_t n; const u8 *data; size_t dn; const u8 *toc; u32 nt; } br_t;
static int br_open(br_t *r, const u8 *d, size_t n) {
    memset(r, 0, sizeof *r);
    if (!d || n < HDR + 4 || memcmp(d, "book", 4)) return -1;
    u32 hl = g32(d + 12);
    if (hl < 16 || hl > n - 4 || g32(d + 4) > n) return -1;
    r->d = d; r->n = n; r->data = d + hl; r->dn = n - hl;
    u32 to = g32(r->data);
    if (to + 20 > r->dn || g32(r->data + to + 4) != 0xfffffffeu) return -1;
    r->nt = g32(r->data + to + 16);
    if ((size_t)to + 20 + 12ull * r->nt > r->dn) return -1;
    r->toc = r->data + to + 20;
    return 0;
}
/* item pelo deslocamento: tipo, ponteiro e tamanho (com o limite) */
static const u8 *br_item(const br_t *r, u32 off, u32 *type, u32 *len) {
    if ((size_t)off + 8 > r->dn) return 0;
    u32 l = g32(r->data + off), t = g32(r->data + off + 4);
    if ((size_t)off + 8 + l > r->dn) return 0;
    *type = t; *len = l;
    return r->data + off + 8;
}
static const u8 *br_key(const br_t *r, u32 key, u32 *type, u32 *len) {
    for (u32 i = 0; i < r->nt; i++) if (g32(r->toc + 12 * i) == key) return br_item(r, g32(r->toc + 12 * i + 4), type, len);
    return 0;
}
static int br_str(const br_t *r, u32 off, char *out, size_t cap) {
    u32 t, l;
    const u8 *p = br_item(r, off, &t, &l);
    if (!p || (t != 0x0101 && t != 0x0901) || l >= cap) return -1;
    memcpy(out, p, l); out[l] = 0;
    return 0;
}
static int br_num(const br_t *r, u32 off, i64 *v) {
    u32 t, l;
    const u8 *p = br_item(r, off, &t, &l);
    if (!p) return -1;
    if (t == 0x0304 && l == 8) { *v = (i64)g64(p); return 0; }
    if (t == 0x0303 && l == 4) { *v = (i32)g32(p); return 0; }
    return -1;
}

typedef struct {
    char path[1024], name[256], volpath[300], volname[256];
    u64 ino, props;
    i64 crtime_ns;
    int has_scope, was_ref;
} cx_book_info_t;

int cx_book_info(const u8 *d, long n, cx_book_info_t *in) {
    br_t r;
    u32 t, l;
    memset(in, 0, sizeof *in);
    if (n <= 0 || br_open(&r, d, (size_t)n) < 0) { SET(22); return -1; }
    const u8 *pl = br_key(&r, BMK_PATH, &t, &l);
    if (!pl || t != 0x0601) { SET(22); return -1; }
    size_t o = 0;
    in->path[0] = 0;
    for (u32 i = 0; i + 4 <= l; i += 4) {
        char c[256];
        if (br_str(&r, g32(pl + i), c, sizeof c) < 0) { SET(22); return -1; }
        size_t k = strlen(c);
        if (o + 1 + k + 1 > sizeof in->path) { SET(63); return -1; }
        in->path[o++] = '/'; memcpy(in->path + o, c, k + 1); o += k;
        strlcpy(in->name, c, sizeof in->name);
    }
    if (!o) strlcpy(in->path, "/", sizeof in->path);
    const u8 *cl = br_key(&r, BMK_CNIDS, &t, &l);
    if (cl && t == 0x0601 && l >= 4) { i64 v; if (br_num(&r, g32(cl + l - 4), &v) == 0) in->ino = (u64)v; }
    const u8 *fp = br_key(&r, BMK_FILEPROPS, &t, &l);
    if (fp && t == 0x0201 && l >= 8) in->props = g64(fp);
    const u8 *ct = br_key(&r, BMK_CRTIME, &t, &l);
    if (ct && t == 0x0400 && l == 8) {
        u64 bits = 0;
        for (int i = 0; i < 8; i++) bits = bits << 8 | ct[i];
        double s; memcpy(&s, &bits, 8);
        in->crtime_ns = (i64)((s + 978307200.0) * 1e9);
    }
    for (u32 i = 0; i < r.nt; i++) {
        u32 k = g32(r.toc + 12 * i), off = g32(r.toc + 12 * i + 4);
        if (k == BMK_VOLPATH) br_str(&r, off, in->volpath, sizeof in->volpath);
        if (k == BMK_VOLNAME) br_str(&r, off, in->volname, sizeof in->volname);
        if (k == BMK_SECURITY) in->has_scope = 1;
        if (k == BMK_WASREF) { u32 tt, ll; if (br_item(&r, off, &tt, &ll) && tt == 0x0501) in->was_ref = 1; }
    }
    return 0;
}

/* resolve: caminho atual do alvo em out. *stale = 1 se ele mudou de lugar (o app deve criar o bookmark de novo). */
int cx_book_resolve(const u8 *d, long n, unsigned opts, char *out, size_t cap, int *stale) {
    cx_book_info_t in;
    mos_statx_t x;
    (void)opts;
    if (stale) *stale = 0;
    if (cx_book_info(d, n, &in) < 0) return -1;
    if (cx_statx(in.path, -1, &x) == 0 && (!in.ino || x.ino == in.ino)) {
        strlcpy(out, in.path, cap);
        return 0;
    }
    if (in.ino) {                                        /* movido ou renomeado: pelo inode */
        char p[1024];
        if (fsgetpath(p, sizeof p, 0, in.ino) > 0) {
            strlcpy(out, p, cap);
            if (stale) *stale = 1;
            return 0;
        }
    }
    if (cx_statx(in.path, -1, &x) == 0) {               /* outro arquivo no mesmo caminho (substituído) */
        strlcpy(out, in.path, cap);
        if (stale) *stale = 1;
        return 0;
    }
    SET(2);
    return -1;
}

/* ======================================================================== alias do Finder */
int open(const char *p, int flags, ...);
long read(int fd, void *b, size_t n);
long write(int fd, const void *b, size_t n);
int close(int fd);
int cx_alias_write(const u8 *d, long n, const char *path) {
    char abs[1024];
    cx_path(path, abs, sizeof abs);
    int fd = open(abs, 0x0601, 0644);                    /* O_WRONLY|O_CREAT|O_TRUNC */
    if (fd < 0) return -1;
    long w = write(fd, d, (size_t)n);
    close(fd);
    if (w != n) { SET(5); return -1; }
    u8 fi[32] = {'a', 'l', 'i', 's', 'M', 'A', 'C', 'S', 0x80, 0};   /* tipo, criador, kIsAlias */
    return setxattr(abs, "com.apple.FinderInfo", fi, 32, 0, 0x1);
}
long cx_alias_read(const char *path, u8 **out) {
    char abs[1024];
    mos_statx_t x;
    if (cx_follow_path(path, abs, sizeof abs) < 0) return -1;
    if (cx_statx(abs, -1, &x) < 0) return -1;
    if (x.kind == 1 || x.size < HDR || x.size > (16u << 20)) { SET(22); return -1; }
    u8 *b = malloc(x.size);
    if (!b) { SET(12); return -1; }
    int fd = open(abs, 0);
    long r = fd >= 0 ? read(fd, b, x.size) : -1;
    if (fd >= 0) close(fd);
    br_t chk;
    if (r != (long)x.size || br_open(&chk, b, x.size) < 0) { free(b); SET(22); return -1; }
    *out = b;
    return r;
}

/* ======================================================================== URL de referência: /.file/id=<vol>.<inode> */
int cx_file_ref_path(const char *abs, char *out, size_t cap) {
    mos_statx_t x;
    if (cx_statx(abs, -1, &x) < 0) return -1;
    char t[64];
    int k = 0;
    u64 v = x.dev, i = x.ino;
    char a[24], b[24]; int na = 0, nb = 0;
    do a[na++] = (char)('0' + v % 10); while (v /= 10);
    do b[nb++] = (char)('0' + i % 10); while (i /= 10);
    memcpy(t, "/.file/id=", 10); k = 10;
    while (na) t[k++] = a[--na];
    t[k++] = '.';
    while (nb) t[k++] = b[--nb];
    t[k] = 0;
    strlcpy(out, t, cap);
    return 0;
}
/* o contrário: "/.file/id=V.I[/resto]" -> caminho real; 0 se não for referência */
int cx_file_ref_resolve(const char *in, char *out, size_t cap) {
    if (strncmp(in, "/.file/id=", 10)) return 0;
    const char *p = in + 10;
    while (*p >= '0' && *p <= '9') p++;
    if (*p != '.') return -1;
    p++;
    u64 ino = 0;
    while (*p >= '0' && *p <= '9') ino = ino * 10 + (u64)(*p++ - '0');
    char base[1024];
    if (fsgetpath(base, sizeof base, 0, ino) <= 0) return -1;
    strlcpy(out, base, cap);
    if (*p == '/' && p[1]) strlcat(out, p, cap);
    return 1;
}

#ifndef CX_HOST_TEST
/* ======================================================================== CFURL e NSURL */
typedef void *id;
typedef void *SEL;
id objc_msgSend(id, SEL, ...);
SEL sel_registerName(const char *);
id objc_retain(id);
void *cx_class_named(const char *);
void cx_add_method(void *cls, const char *sel, void *imp);
void cx_add_cmethod(void *cls, const char *sel, void *imp);
id CFDataCreate(void *alloc, const u8 *b, long n);
long CFDataGetLength(id d);
const u8 *CFDataGetBytePtr(id d);
id CFURLCreateWithFileSystemPath(void *alloc, id path, long style, u8 dir);
u8 CFURLGetFileSystemRepresentation(id url, u8 resolve, u8 *buf, long max);
void CFRelease(id o);
id CFStringCreateWithCString(void *alloc, const char *s, u32 enc);
id CFErrorCreate(void *alloc, id domain, long code, id info);
id CFDictionaryCreateMutable(void *alloc, long cap, const void *k, const void *v);
void CFDictionarySetValue(id d, const void *k, const void *v);
const void *CFDictionaryGetValue(id d, const void *k);
long CFArrayGetCount(id a);
id CFArrayGetValueAtIndex(id a, long i);
id CFNumberCreate(void *alloc, long type, const void *p);
id CFDateCreate(void *alloc, double at);
extern const char kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks;
extern const void *kCFBooleanTrue, *kCFBooleanFalse;
#define MSG0(T, o, s) ((T (*)(id, SEL))objc_msgSend)((id)(o), sel_registerName(s))
#define MSG1(T, o, s, a) ((T (*)(id, SEL, __typeof__(a)))objc_msgSend)((id)(o), sel_registerName(s), (a))

static id cfstr(const char *s) { return CFStringCreateWithCString(0, s, 0x08000100u); }
static const char *cstr(id s) { return s ? MSG0(const char *, s, "UTF8String") : 0; }
static int url_path(id url, char *out, size_t cap) { return url && CFURLGetFileSystemRepresentation(url, 1, (u8 *)out, (long)cap) ? 0 : -1; }
static id url_of(const char *p, int dir) {
    id s = cfstr(p);
    id u = s ? CFURLCreateWithFileSystemPath(0, s, 0, (u8)dir) : 0;
    if (s) CFRelease(s);
    return u;
}
static void set_err(id *err, int code) {
    if (!err) return;
    /* NSCocoaErrorDomain: 4 NSFileNoSuchFileError, 259 NSFileReadCorruptFileError, 256 NSFileReadUnknownError */
    id dom = cfstr("NSCocoaErrorDomain");
    *err = CFErrorCreate(0, dom, code == 2 ? 4 : code == 22 ? 259 : 256, 0);
    if (dom) CFRelease(dom);
}

id CFURLCreateBookmarkData(void *alloc, id url, unsigned long opts, id keys, id rel, id *err) {
    (void)alloc; (void)keys; (void)rel;
    char p[1024];
    u8 *b = 0;
    if (url_path(url, p, sizeof p) < 0) { set_err(err, 22); return 0; }
    long n = cx_book_create(p, (unsigned)opts, 0, &b);
    if (n < 0) { set_err(err, cx_errno); return 0; }
    id d = CFDataCreate(0, b, n);
    free(b);
    return d;
}
id CFURLCreateByResolvingBookmarkData(void *alloc, id data, unsigned long opts, id rel, id keys, u8 *stale, id *err) {
    (void)alloc; (void)rel; (void)keys;
    char p[1024];
    int st = 0;
    if (!data || cx_book_resolve(CFDataGetBytePtr(data), CFDataGetLength(data), (unsigned)opts, p, sizeof p, &st) < 0) {
        set_err(err, data ? cx_errno : 22);
        return 0;
    }
    if (stale) *stale = (u8)st;
    mos_statx_t x;
    int dir = cx_statx(p, -1, &x) == 0 && x.kind == 1;
    return url_of(p, dir);
}
id CFURLCreateBookmarkDataFromFile(void *alloc, id file, id *err) {
    (void)alloc;
    char p[1024];
    u8 *b = 0;
    if (url_path(file, p, sizeof p) < 0) { set_err(err, 22); return 0; }
    long n = cx_alias_read(p, &b);
    if (n < 0) { set_err(err, cx_errno); return 0; }
    id d = CFDataCreate(0, b, n);
    free(b);
    return d;
}
u8 CFURLWriteBookmarkDataToFile(id data, id file, unsigned long opts, id *err) {
    (void)opts;
    char p[1024];
    if (!data || url_path(file, p, sizeof p) < 0) { set_err(err, 22); return 0; }
    if (cx_alias_write(CFDataGetBytePtr(data), CFDataGetLength(data), p) < 0) { set_err(err, cx_errno); return 0; }
    return 1;
}
id CFURLCreateBookmarkDataFromAliasRecord(void *alloc, id alias) { (void)alloc; (void)alias; return 0; }   /* AliasRecord (Carbon) não existe aqui */

/* propriedades guardadas no bookmark (sem acessar o arquivo) */
static id book_props(id data) {
    cx_book_info_t in;
    if (!data || cx_book_info(CFDataGetBytePtr(data), CFDataGetLength(data), &in) < 0) return 0;
    id d = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!d) return 0;
    struct { const char *k; id v; } kv[8];
    int n = 0;
    kv[n].k = "NSURLNameKey"; kv[n++].v = cfstr(in.name);
    kv[n].k = "NSURLPathKey"; kv[n++].v = cfstr(in.path);
    kv[n].k = "_NSURLPathKey"; kv[n++].v = cfstr(in.path);
    kv[n].k = "NSURLVolumeNameKey"; kv[n++].v = cfstr(in.volname);
    kv[n].k = "NSURLVolumeURLKey"; kv[n++].v = url_of(in.volpath[0] ? in.volpath : "/", 1);
    kv[n].k = "NSURLCreationDateKey"; kv[n++].v = CFDateCreate(0, (double)in.crtime_ns / 1e9 - 978307200.0);
    long long ino = (long long)in.ino;
    kv[n].k = "NSURLFileResourceIdentifierKey"; kv[n++].v = CFNumberCreate(0, 4, &ino);
    for (int i = 0; i < n; i++) {
        id k = cfstr(kv[i].k);
        if (k && kv[i].v) CFDictionarySetValue(d, k, kv[i].v);
        if (k) CFRelease(k);
        if (kv[i].v) CFRelease(kv[i].v);
    }
    struct { const char *k; u64 bit; } bk[] = {
        {"NSURLIsDirectoryKey", FP_DIR}, {"NSURLIsRegularFileKey", FP_REGULAR}, {"NSURLIsSymbolicLinkKey", FP_LINK},
        {"NSURLIsVolumeKey", FP_VOLUME}, {"NSURLIsPackageKey", FP_PACKAGE}, {"NSURLIsHiddenKey", FP_HIDDEN},
        {"NSURLIsApplicationKey", FP_APP}, {"NSURLIsAliasFileKey", FP_ALIAS},
    };
    for (unsigned i = 0; i < sizeof bk / sizeof *bk; i++) {
        id k = cfstr(bk[i].k);
        if (k) { CFDictionarySetValue(d, k, (in.props & bk[i].bit) ? kCFBooleanTrue : kCFBooleanFalse); CFRelease(k); }
    }
    return d;
}
id CFURLCreateResourcePropertiesForKeysFromBookmarkData(void *alloc, id keys, id data) {
    (void)alloc;
    id all = book_props(data);
    if (!all || !keys) return all;
    id d = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    for (long i = 0; d && i < CFArrayGetCount(keys); i++) {
        id k = CFArrayGetValueAtIndex(keys, i);
        const void *v = CFDictionaryGetValue(all, k);
        if (v) CFDictionarySetValue(d, k, v);
    }
    CFRelease(all);
    return d;
}
id CFURLCreateResourcePropertyForKeyFromBookmarkData(void *alloc, id key, id data) {
    (void)alloc;
    id all = book_props(data);
    if (!all) return 0;
    const void *v = CFDictionaryGetValue(all, key);
    id r = v ? objc_retain((id)v) : 0;
    CFRelease(all);
    return r;
}
u8 CFURLStartAccessingSecurityScopedResource(id url) { (void)url; return 1; }   /* sem sandbox: o acesso já existe */
void CFURLStopAccessingSecurityScopedResource(id url) { (void)url; }
id CFURLCreateFileReferenceURL(void *alloc, id url, id *err) {
    (void)alloc;
    char p[1024], r[128];
    if (url_path(url, p, sizeof p) < 0 || cx_file_ref_path(p, r, sizeof r) < 0) { set_err(err, 2); return 0; }
    return url_of(r, 0);
}
id CFURLCreateFilePathURL(void *alloc, id url, id *err) {
    (void)alloc;
    char p[1024], r[1024];
    if (url_path(url, p, sizeof p) < 0) { set_err(err, 22); return 0; }
    int k = cx_file_ref_resolve(p, r, sizeof r);
    if (k < 0) { set_err(err, 2); return 0; }
    return k ? url_of(r, 0) : objc_retain(url);
}
u8 CFURLIsFileReferenceURL(id url) { char p[1024]; return url_path(url, p, sizeof p) == 0 && !strncmp(p, "/.file/id=", 10); }

/* NSURL: os métodos que o Foundation do macOS tem para bookmarks e alias */
static id nsu_bookmark(id self, SEL c, unsigned long opts, id keys, id rel, id *err) { (void)c; id d = CFURLCreateBookmarkData(0, self, opts, keys, rel, err); return d ? MSG0(id, d, "autorelease") : 0; }
static id nsu_resolve(id cls, SEL c, id data, unsigned long opts, id rel, u8 *stale, id *err) {
    (void)cls; (void)c;
    id u = CFURLCreateByResolvingBookmarkData(0, data, opts, rel, 0, stale, err);
    return u ? MSG0(id, u, "autorelease") : 0;
}
static id nsu_init_resolve(id self, SEL c, id data, unsigned long opts, id rel, u8 *stale, id *err) {
    (void)self; (void)c;
    return CFURLCreateByResolvingBookmarkData(0, data, opts, rel, 0, stale, err);
}
static id nsu_book_from_file(id cls, SEL c, id url, id *err) { (void)cls; (void)c; id d = CFURLCreateBookmarkDataFromFile(0, url, err); return d ? MSG0(id, d, "autorelease") : 0; }
static u8 nsu_write_book(id cls, SEL c, id data, id url, unsigned long opts, id *err) { (void)cls; (void)c; return CFURLWriteBookmarkDataToFile(data, url, opts, err); }
static id nsu_resolve_alias(id cls, SEL c, id url, unsigned long opts, id *err) {
    (void)cls; (void)c;
    char p[1024];
    if (url_path(url, p, sizeof p) < 0) { set_err(err, 22); return 0; }
    if (!cx_alias_is(p)) return url;                     /* não é alias: o próprio URL */
    id d = CFURLCreateBookmarkDataFromFile(0, url, err);
    if (!d) return 0;
    id r = CFURLCreateByResolvingBookmarkData(0, d, opts, 0, 0, 0, err);
    CFRelease(d);
    return r ? MSG0(id, r, "autorelease") : 0;
}
static id nsu_props_from_book(id cls, SEL c, id keys, id data) { (void)cls; (void)c; id d = CFURLCreateResourcePropertiesForKeysFromBookmarkData(0, keys, data); return d ? MSG0(id, d, "autorelease") : 0; }
static u8 nsu_start(id self, SEL c) { (void)self; (void)c; return 1; }
static void nsu_stop(id self, SEL c) { (void)self; (void)c; }
static id nsu_file_ref(id self, SEL c) { (void)c; id u = CFURLCreateFileReferenceURL(0, self, 0); return u ? MSG0(id, u, "autorelease") : 0; }
static id nsu_file_path(id self, SEL c) { (void)c; id u = CFURLCreateFilePathURL(0, self, 0); return u ? MSG0(id, u, "autorelease") : 0; }
static u8 nsu_is_ref(id self, SEL c) { (void)c; return CFURLIsFileReferenceURL(self); }

void cx_bookmark_init(void) {
    void *u = cx_class_named("NSURL");
    if (!u) return;
    cx_add_method(u, "bookmarkDataWithOptions:includingResourceValuesForKeys:relativeToURL:error:", (void *)nsu_bookmark);
    cx_add_cmethod(u, "URLByResolvingBookmarkData:options:relativeToURL:bookmarkDataIsStale:error:", (void *)nsu_resolve);
    cx_add_method(u, "initByResolvingBookmarkData:options:relativeToURL:bookmarkDataIsStale:error:", (void *)nsu_init_resolve);
    cx_add_cmethod(u, "bookmarkDataWithContentsOfURL:error:", (void *)nsu_book_from_file);
    cx_add_cmethod(u, "writeBookmarkData:toURL:options:error:", (void *)nsu_write_book);
    cx_add_cmethod(u, "URLByResolvingAliasFileAtURL:options:error:", (void *)nsu_resolve_alias);
    cx_add_cmethod(u, "resourceValuesForKeys:fromBookmarkData:", (void *)nsu_props_from_book);
    cx_add_method(u, "startAccessingSecurityScopedResource", (void *)nsu_start);
    cx_add_method(u, "stopAccessingSecurityScopedResource", (void *)nsu_stop);
    cx_add_method(u, "fileReferenceURL", (void *)nsu_file_ref);
    cx_add_method(u, "filePathURL", (void *)nsu_file_path);
    cx_add_method(u, "isFileReferenceURL", (void *)nsu_is_ref);
}
#endif
