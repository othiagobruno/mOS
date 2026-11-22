/*
 * mOS libSystem — o pedaço da libarchive (/usr/lib/libarchive.2.dylib) que o corpus usa (docs/lacunas/: 1 a 4 apps):
 *   escrita: tar ustar/pax (nome longo vai num cabeçalho pax "x"), saída em arquivo, fd, memória ou callbacks, filtros
 *            gzip e bzip2 (o arquivo inteiro é comprimido no close);
 *   leitura: tar (ustar, pax e o nome longo do GNU), detectando gzip, bzip2 e xz pelo começo do arquivo; extração para
 *            o disco (arquivos, pastas e links simbólicos); archive_read_disk_entry_from_file pelo stat;
 *   entradas: caminho, tamanho, tipo, modo, dono, datas.
 * zip, 7z e os outros formatos não: archive_read_next_header devolve ARCHIVE_FATAL com a mensagem. Teste:
 * tools/libarchive_mos_test.sh
 */
#include "sys.h"
#include <stdarg.h>

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
char *strdup(const char *s);
char *strrchr(const char *s, int c);
int strcmp(const char *a, const char *b);
int snprintf(char *b, size_t n, const char *f, ...);
int open(const char *p, int flags, ...);
int close(int fd);
long read(int fd, void *b, unsigned long n);
long write(int fd, const void *b, unsigned long n);
int mkdir(const char *p, int mode);
int symlink(const char *t, const char *p);
int lstat(const char *p, void *st);
int chmod(const char *p, int mode);
size_t cz_raw_buffer(unsigned char *dst, size_t dn, const unsigned char *src, size_t sn, int encode);
int BZ2_bzBuffToBuffCompress(char *dst, unsigned *dlen, char *src, unsigned slen, int block, int verb, int work);
int BZ2_bzBuffToBuffDecompress(char *dst, unsigned *dlen, char *src, unsigned slen, int small, int verb);
int lzma_stream_buffer_decode(u64 *memlimit, u32 flags, const void *alloc, const u8 *in, size_t *in_pos, size_t in_size, u8 *out,
                              size_t *out_pos, size_t out_size);

#define ARCHIVE_EOF 1
#define ARCHIVE_OK 0
#define ARCHIVE_WARN (-20)
#define ARCHIVE_FATAL (-30)
#define AE_IFMT 0170000
#define AE_IFREG 0100000
#define AE_IFDIR 0040000
#define AE_IFLNK 0120000
#define O_WR_ 0x601                                 /* O_WRONLY|O_CREAT|O_TRUNC do macOS (o open do libSystem é o do app) */

/* ---------------------------------------------------------------- entradas */
typedef struct archive_entry {
    char *path, *link, *uname, *gname;
    i64 size;
    u32 mode;                                       /* tipo | permissões */
    i64 uid, gid;
    i64 mtime, atime, ctime;
    int has_mtime;
} archive_entry;
static void set_str(char **d, const char *s) { char *n = s ? strdup(s) : 0; free(*d); *d = n; }
archive_entry *archive_entry_new(void) { archive_entry *e = calloc(1, sizeof *e); if (e) e->mode = AE_IFREG | 0644; return e; }
archive_entry *archive_entry_new2(void *a) { (void)a; return archive_entry_new(); }
archive_entry *archive_entry_clear(archive_entry *e) {
    if (!e) return 0;
    free(e->path); free(e->link); free(e->uname); free(e->gname);
    memset(e, 0, sizeof *e);
    e->mode = AE_IFREG | 0644;
    return e;
}
void archive_entry_free(archive_entry *e) { if (e) { archive_entry_clear(e); free(e); } }
archive_entry *archive_entry_clone(archive_entry *e) {
    archive_entry *c = archive_entry_new();
    if (!c || !e) return c;
    *c = *e;
    c->path = e->path ? strdup(e->path) : 0; c->link = e->link ? strdup(e->link) : 0;
    c->uname = e->uname ? strdup(e->uname) : 0; c->gname = e->gname ? strdup(e->gname) : 0;
    return c;
}
void archive_entry_set_pathname(archive_entry *e, const char *p) { if (e) set_str(&e->path, p); }
void archive_entry_copy_pathname(archive_entry *e, const char *p) { archive_entry_set_pathname(e, p); }
const char *archive_entry_pathname(archive_entry *e) { return e ? e->path : 0; }
void archive_entry_set_symlink(archive_entry *e, const char *p) { if (e) set_str(&e->link, p); }
void archive_entry_copy_symlink(archive_entry *e, const char *p) { archive_entry_set_symlink(e, p); }
const char *archive_entry_symlink(archive_entry *e) { return e ? e->link : 0; }
void archive_entry_set_uname(archive_entry *e, const char *p) { if (e) set_str(&e->uname, p); }
void archive_entry_set_gname(archive_entry *e, const char *p) { if (e) set_str(&e->gname, p); }
const char *archive_entry_uname(archive_entry *e) { return e ? e->uname : 0; }
const char *archive_entry_gname(archive_entry *e) { return e ? e->gname : 0; }
void archive_entry_set_size(archive_entry *e, i64 s) { if (e) e->size = s; }
i64 archive_entry_size(archive_entry *e) { return e ? e->size : 0; }
void archive_entry_set_filetype(archive_entry *e, u32 t) { if (e) e->mode = (e->mode & ~AE_IFMT) | (t & AE_IFMT); }
u32 archive_entry_filetype(archive_entry *e) { return e ? e->mode & AE_IFMT : 0; }
void archive_entry_set_perm(archive_entry *e, u32 p) { if (e) e->mode = (e->mode & AE_IFMT) | (p & 07777); }
u32 archive_entry_perm(archive_entry *e) { return e ? e->mode & 07777 : 0; }
void archive_entry_set_mode(archive_entry *e, u32 m) { if (e) e->mode = m; }
u32 archive_entry_mode(archive_entry *e) { return e ? e->mode : 0; }
void archive_entry_set_uid(archive_entry *e, i64 v) { if (e) e->uid = v; }
void archive_entry_set_gid(archive_entry *e, i64 v) { if (e) e->gid = v; }
i64 archive_entry_uid(archive_entry *e) { return e ? e->uid : 0; }
i64 archive_entry_gid(archive_entry *e) { return e ? e->gid : 0; }
void archive_entry_set_mtime(archive_entry *e, i64 t, long ns) { (void)ns; if (e) { e->mtime = t; e->has_mtime = 1; } }
void archive_entry_set_atime(archive_entry *e, i64 t, long ns) { (void)ns; if (e) e->atime = t; }
void archive_entry_set_ctime(archive_entry *e, i64 t, long ns) { (void)ns; if (e) e->ctime = t; }
void archive_entry_unset_mtime(archive_entry *e) { if (e) { e->mtime = 0; e->has_mtime = 0; } }
void archive_entry_unset_atime(archive_entry *e) { if (e) e->atime = 0; }
void archive_entry_unset_ctime(archive_entry *e) { if (e) e->ctime = 0; }
i64 archive_entry_mtime(archive_entry *e) { return e ? e->mtime : 0; }

/* ---------------------------------------------------------------- objeto archive */
typedef long (*write_cb_t)(void *a, void *ud, const void *b, size_t n);
typedef int (*oc_cb_t)(void *a, void *ud);
typedef struct archive {
    int reading, filter;                            /* filter: 0 nenhum, 1 gzip, 2 bzip2 */
    int fd, own_fd;
    void *ud; write_cb_t wcb; oc_cb_t ccb;
    void *mbuf; size_t msize, *mused;               /* archive_write_open_memory */
    u8 *out; size_t outn, outcap;                   /* saída acumulada (filtro) */
    i64 left, pad;                                  /* bytes da entrada atual */
    char err[256]; int errn;
    /* leitura */
    u8 *in; size_t inn, ipos;
    archive_entry *cur;
    int ext_opts;
} archive;
static void a_err(archive *a, int e, const char *m) { if (!a) return; a->errn = e; snprintf(a->err, sizeof a->err, "%s", m); }
const char *archive_error_string(archive *a) { return a && a->err[0] ? a->err : 0; }
int archive_errno(archive *a) { return a ? a->errn : 0; }
void archive_clear_error(archive *a) { if (a) { a->err[0] = 0; a->errn = 0; } }
void archive_set_error(archive *a, int e, const char *fmt, ...) {
    if (!a) return;
    va_list ap; va_start(ap, fmt);
    extern int vsnprintf(char *, size_t, const char *, va_list);
    vsnprintf(a->err, sizeof a->err, fmt, ap);
    va_end(ap);
    a->errn = e;
}
int archive_version_number(void) { return 3007002; }
const char *archive_version_string(void) { return "libarchive 3.7.2 (mOS)"; }
const char *archive_format_name(archive *a) { (void)a; return "POSIX ustar"; }
int archive_format(archive *a) { (void)a; return 0x30001; }                    /* ARCHIVE_FORMAT_TAR_USTAR */

/* ---------------------------------------------------------------- escrita */
archive *archive_write_new(void) { archive *a = calloc(1, sizeof *a); if (a) a->fd = -1; return a; }
int archive_write_set_format_ustar(archive *a) { return a ? ARCHIVE_OK : ARCHIVE_FATAL; }
int archive_write_set_format_pax(archive *a) { return a ? ARCHIVE_OK : ARCHIVE_FATAL; }
int archive_write_set_format_pax_restricted(archive *a) { return a ? ARCHIVE_OK : ARCHIVE_FATAL; }
int archive_write_set_format_gnutar(archive *a) { return a ? ARCHIVE_OK : ARCHIVE_FATAL; }
int archive_write_set_format_by_name(archive *a, const char *n) {
    if (!a || !n) return ARCHIVE_FATAL;
    if (!strcmp(n, "ustar") || !strcmp(n, "pax") || !strcmp(n, "paxr") || !strcmp(n, "gnutar") || !strcmp(n, "tar")) return ARCHIVE_OK;
    a_err(a, 22, "formato não suportado no mOS (só tar)");
    return ARCHIVE_FATAL;
}
int archive_write_set_format_zip(archive *a) { a_err(a, 22, "zip não suportado no mOS"); return ARCHIVE_FATAL; }
int archive_write_add_filter_none(archive *a) { if (a) a->filter = 0; return ARCHIVE_OK; }
int archive_write_add_filter_gzip(archive *a) { if (a) a->filter = 1; return ARCHIVE_OK; }
int archive_write_add_filter_bzip2(archive *a) { if (a) a->filter = 2; return ARCHIVE_OK; }
int archive_write_set_compression_gzip(archive *a) { return archive_write_add_filter_gzip(a); }
int archive_write_set_compression_bzip2(archive *a) { return archive_write_add_filter_bzip2(a); }
int archive_write_set_bytes_per_block(archive *a, int n) { (void)a; (void)n; return ARCHIVE_OK; }
int archive_write_set_bytes_in_last_block(archive *a, int n) { (void)a; (void)n; return ARCHIVE_OK; }
static int emit(archive *a, const void *b, size_t n) {
    if (!n) return ARCHIVE_OK;
    if (a->filter || a->mbuf) {                     /* junta tudo: o filtro comprime no fim; memória copia no fim */
        if (a->outn + n > a->outcap) {
            size_t c = a->outcap ? a->outcap * 2 : 65536;
            while (c < a->outn + n) c *= 2;
            u8 *p = realloc(a->out, c);
            if (!p) { a_err(a, 12, "sem memória"); return ARCHIVE_FATAL; }
            a->out = p; a->outcap = c;
        }
        memcpy(a->out + a->outn, b, n);
        a->outn += n;
        return ARCHIVE_OK;
    }
    if (a->wcb) return a->wcb(a, a->ud, b, n) == (long)n ? ARCHIVE_OK : ARCHIVE_FATAL;
    if (a->fd >= 0) {
        const u8 *p = b;
        while (n) { long k = write(a->fd, p, n); if (k <= 0) { a_err(a, 5, "erro de escrita"); return ARCHIVE_FATAL; } p += k; n -= (size_t)k; }
        return ARCHIVE_OK;
    }
    a_err(a, 22, "archive não aberto");
    return ARCHIVE_FATAL;
}
int archive_write_open_fd(archive *a, int fd) { if (!a || fd < 0) return ARCHIVE_FATAL; a->fd = fd; return ARCHIVE_OK; }
int archive_write_open_filename(archive *a, const char *path) {
    if (!a) return ARCHIVE_FATAL;
    if (!path) return archive_write_open_fd(a, 1);
    int fd = open(path, O_WR_, 0644);
    if (fd < 0) { a_err(a, cx_errno, "não foi possível criar o arquivo"); return ARCHIVE_FATAL; }
    a->fd = fd; a->own_fd = 1;
    return ARCHIVE_OK;
}
int archive_write_open_file(archive *a, const char *p) { return archive_write_open_filename(a, p); }
int archive_write_open_memory(archive *a, void *buf, size_t size, size_t *used) {
    if (!a || !buf) return ARCHIVE_FATAL;
    a->mbuf = buf; a->msize = size; a->mused = used;
    if (used) *used = 0;
    return ARCHIVE_OK;
}
int archive_write_open(archive *a, void *ud, oc_cb_t open_cb, write_cb_t wcb, oc_cb_t ccb) {
    if (!a || !wcb) return ARCHIVE_FATAL;
    a->ud = ud; a->wcb = wcb; a->ccb = ccb;
    return open_cb ? (open_cb(a, ud) ? ARCHIVE_FATAL : ARCHIVE_OK) : ARCHIVE_OK;
}
static void octal(char *f, int w, u64 v) { f[w - 1] = 0; for (int i = w - 2; i >= 0; i--) { f[i] = (char)('0' + (v & 7)); v >>= 3; } }
static int tar_header(archive *a, const char *name, i64 size, u32 mode, char type, const char *link, archive_entry *e) {
    u8 h[512];
    memset(h, 0, 512);
    size_t nl = strlen(name);
    if (nl <= 100) memcpy(h, name, nl);
    else {                                         /* ustar: prefixo de até 155 + nome de até 100 */
        const char *cut = 0;
        for (const char *p = name; *p; p++) if (*p == '/' && (size_t)(p - name) <= 155 && strlen(p + 1) <= 100) { cut = p; break; }
        if (cut) { memcpy(h + 345, name, (size_t)(cut - name)); memcpy(h, cut + 1, strlen(cut + 1)); }
        else memcpy(h, name, 100);
    }
    octal((char *)h + 100, 8, mode & 07777);
    octal((char *)h + 108, 8, e ? (u64)e->uid : 0);
    octal((char *)h + 116, 8, e ? (u64)e->gid : 0);
    octal((char *)h + 124, 12, (u64)size);
    octal((char *)h + 136, 12, e ? (u64)e->mtime : 0);
    h[156] = (u8)type;
    if (link) { size_t ll = strlen(link); memcpy(h + 157, link, ll > 100 ? 100 : ll); }
    memcpy(h + 257, "ustar", 6); h[263] = '0'; h[264] = '0';
    if (e && e->uname) { size_t k = strlen(e->uname); memcpy(h + 265, e->uname, k > 31 ? 31 : k); }
    if (e && e->gname) { size_t k = strlen(e->gname); memcpy(h + 297, e->gname, k > 31 ? 31 : k); }
    memset(h + 148, ' ', 8);
    u32 sum = 0;
    for (int i = 0; i < 512; i++) sum += h[i];
    octal((char *)h + 148, 7, sum);
    h[155] = ' ';
    return emit(a, h, 512);
}
static int pad_to_block(archive *a, i64 n) {
    static const u8 z[512];
    i64 r = n % 512;
    return r ? emit(a, z, (size_t)(512 - r)) : ARCHIVE_OK;
}
int archive_write_finish_entry(archive *a) {
    if (!a) return ARCHIVE_FATAL;
    static const u8 z[512];
    while (a->left > 0) { size_t k = a->left > 512 ? 512 : (size_t)a->left; if (emit(a, z, k)) return ARCHIVE_FATAL; a->left -= (i64)k; }
    int r = a->pad ? pad_to_block(a, a->pad) : ARCHIVE_OK;
    a->pad = 0;
    return r;
}
int archive_write_header(archive *a, archive_entry *e) {
    if (!a || !e || !e->path) { a_err(a, 22, "entrada sem caminho"); return ARCHIVE_FATAL; }
    if (archive_write_finish_entry(a)) return ARCHIVE_FATAL;
    u32 t = e->mode & AE_IFMT;
    char type = t == AE_IFDIR ? '5' : t == AE_IFLNK ? '2' : '0';
    i64 size = type == '0' ? e->size : 0;
    int need_pax = strlen(e->path) > 100 || (e->link && strlen(e->link) > 100) || size > 077777777777LL;
    if (need_pax) {                                /* registro pax: "len path=..." */
        char rec[4200];
        size_t rn = 0;
        const char *keys[2] = {"path", "linkpath"};
        const char *vals[2] = {e->path, e->link};
        for (int i = 0; i < 2; i++) {
            if (!vals[i]) continue;
            size_t body = strlen(keys[i]) + strlen(vals[i]) + 3, len = body + 1;
            while (1) { char d[24]; int k = snprintf(d, sizeof d, "%zu", len); if ((size_t)k + body == len) break; len = (size_t)k + body; }
            if (rn + len >= sizeof rec) break;
            rn += (size_t)snprintf(rec + rn, sizeof rec - rn, "%zu %s=%s\n", len, keys[i], vals[i]);
        }
        if (tar_header(a, "././@PaxHeader", (i64)rn, 0644, 'x', 0, 0) || emit(a, rec, rn) || pad_to_block(a, (i64)rn)) return ARCHIVE_FATAL;
    }
    if (tar_header(a, e->path, size, e->mode, type, e->link, e)) return ARCHIVE_FATAL;
    a->left = size;
    a->pad = size;
    return ARCHIVE_OK;
}
long archive_write_data(archive *a, const void *b, size_t n) {
    if (!a) return ARCHIVE_FATAL;
    if ((i64)n > a->left) n = (size_t)a->left;
    if (emit(a, b, n)) return ARCHIVE_FATAL;
    a->left -= (i64)n;
    return (long)n;
}
static int flush_filtered(archive *a) {
    u8 *data = a->out;
    size_t n = a->outn;
    u8 *comp = 0;
    if (a->filter == 1) {                          /* gzip: cabeçalho + deflate cru + CRC32 + tamanho */
        size_t cap = n + n / 10 + 1024;
        comp = malloc(cap + 18);
        if (!comp) return ARCHIVE_FATAL;
        static const u8 gh[10] = {0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 3};
        memcpy(comp, gh, 10);
        size_t k = cz_raw_buffer(comp + 10, cap, data ? data : (const u8 *)"", n, 1);
        if (!k && n) { free(comp); a_err(a, 5, "gzip falhou"); return ARCHIVE_FATAL; }
        u32 crc = 0xffffffffu;
        for (size_t i = 0; i < n; i++) { crc ^= data[i]; for (int b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1))); }
        crc ^= 0xffffffffu;
        u8 *t = comp + 10 + k;
        for (int i = 0; i < 4; i++) t[i] = (u8)(crc >> (8 * i));
        for (int i = 0; i < 4; i++) t[4 + i] = (u8)((u32)n >> (8 * i));
        data = comp; n = 10 + k + 8;
    } else if (a->filter == 2) {
        unsigned cap = (unsigned)(n + n / 100 + 600);
        comp = malloc(cap);
        if (!comp || BZ2_bzBuffToBuffCompress((char *)comp, &cap, (char *)(data ? data : (u8 *)""), (unsigned)n, 9, 0, 0)) {
            free(comp); a_err(a, 5, "bzip2 falhou"); return ARCHIVE_FATAL;
        }
        data = comp; n = cap;
    }
    int r = ARCHIVE_OK;
    if (a->mbuf) {
        if (n > a->msize) { a_err(a, 28, "buffer de memória pequeno"); r = ARCHIVE_FATAL; }
        else { memcpy(a->mbuf, data, n); if (a->mused) *a->mused = n; }
    } else {
        int f = a->filter;
        u8 *saved = a->out;
        a->filter = 0; a->out = 0;
        r = emit(a, data, n);
        a->filter = f; a->out = saved;
    }
    free(comp);
    return r;
}
int archive_write_close(archive *a) {
    if (!a) return ARCHIVE_FATAL;
    if (a->reading) return ARCHIVE_OK;
    int r = archive_write_finish_entry(a);
    static const u8 z[1024];
    if (!r) r = emit(a, z, 1024);                  /* dois blocos zerados */
    if (!r && (a->filter || a->mbuf)) r = flush_filtered(a);
    free(a->out); a->out = 0; a->outn = a->outcap = 0;
    if (a->own_fd && a->fd >= 0) { close(a->fd); a->fd = -1; a->own_fd = 0; }
    if (a->ccb) { a->ccb(a, a->ud); a->ccb = 0; }
    a->wcb = 0;
    a->mbuf = 0;                                   /* fechado: o archive_write_free não fecha de novo */
    return r;
}
int archive_write_free(archive *a) {
    if (!a) return ARCHIVE_OK;
    if (a->wcb || a->fd >= 0 || a->mbuf) archive_write_close(a);
    free(a->out);
    free(a);
    return ARCHIVE_OK;
}
int archive_write_finish(archive *a) { return archive_write_free(a); }

/* ---------------------------------------------------------------- leitura */
archive *archive_read_new(void) { archive *a = calloc(1, sizeof *a); if (a) { a->reading = 1; a->fd = -1; } return a; }
int archive_read_support_format_all(archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_format_tar(archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_format_gnutar(archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_format_zip(archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_format_raw(archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_filter_all(archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_filter_gzip(archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_filter_bzip2(archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_filter_xz(archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_compression_all(archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_compression_xz(archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_compression_gzip(archive *a) { (void)a; return ARCHIVE_OK; }
/* abre: lê tudo e tira a compressão de fora */
static int read_setup(archive *a, u8 *buf, size_t n) {
    u8 *plain = buf;
    size_t pn = n;
    if (n >= 18 && buf[0] == 0x1f && buf[1] == 0x8b && buf[2] == 8) {
        u8 fl = buf[3];
        size_t p = 10;
        if (fl & 4) { if (p + 2 > n) goto bad; p += 2 + (size_t)(buf[p] | buf[p + 1] << 8); }
        if (fl & 8) { while (p < n && buf[p]) p++; p++; }
        if (fl & 16) { while (p < n && buf[p]) p++; p++; }
        if (fl & 2) p += 2;
        if (p >= n - 8) goto bad;
        size_t isz = (size_t)buf[n - 4] | (size_t)buf[n - 3] << 8 | (size_t)buf[n - 2] << 16 | (size_t)buf[n - 1] << 24;
        plain = malloc(isz ? isz : 1);
        if (!plain) goto bad;
        pn = cz_raw_buffer(plain, isz, buf + p, n - p - 8, 0);
        if (pn != isz) { free(plain); goto bad; }
        free(buf);
    } else if (n >= 3 && buf[0] == 'B' && buf[1] == 'Z' && buf[2] == 'h') {
        for (size_t cap = n * 4 + 4096;; cap *= 2) {
            plain = malloc(cap);
            if (!plain) goto bad;
            unsigned out = (unsigned)cap;
            int r = BZ2_bzBuffToBuffDecompress((char *)plain, &out, (char *)buf, (unsigned)n, 0, 0);
            if (r == 0) { pn = out; break; }
            free(plain);
            if (r != -8 || cap > ((size_t)1 << 31)) goto bad;     /* BZ_OUTBUFF_FULL */
        }
        free(buf);
    } else if (n >= 6 && !memcmp(buf, "\xfd" "7zXZ\0", 6)) {
        for (size_t cap = n * 4 + 4096;; cap *= 2) {
            plain = malloc(cap);
            if (!plain) goto bad;
            u64 lim = (u64)1 << 30;
            size_t ip = 0, op = 0;
            int r = lzma_stream_buffer_decode(&lim, 0, 0, buf, &ip, n, plain, &op, cap);
            if (r == 0) { pn = op; break; }
            free(plain);
            if (r != 10 || cap > ((size_t)1 << 31)) goto bad;     /* LZMA_BUF_ERROR */
        }
        free(buf);
    }
    a->in = plain; a->inn = pn; a->ipos = 0;
    return ARCHIVE_OK;
bad:
    free(buf);
    a_err(a, 79, "arquivo comprimido inválido");
    return ARCHIVE_FATAL;
}
int archive_read_open_memory(archive *a, const void *buf, size_t n) {
    if (!a || (!buf && n)) return ARCHIVE_FATAL;
    u8 *c = malloc(n ? n : 1);
    if (!c) return ARCHIVE_FATAL;
    memcpy(c, buf, n);
    return read_setup(a, c, n);
}
int archive_read_open_fd(archive *a, int fd, size_t block) {
    (void)block;
    if (!a || fd < 0) return ARCHIVE_FATAL;
    size_t cap = 65536, n = 0;
    u8 *b = malloc(cap);
    for (;;) {
        if (!b) return ARCHIVE_FATAL;
        if (n == cap) { cap *= 2; u8 *nb = realloc(b, cap); if (!nb) { free(b); return ARCHIVE_FATAL; } b = nb; }
        long k = read(fd, b + n, cap - n);
        if (k < 0) { free(b); a_err(a, cx_errno, "erro de leitura"); return ARCHIVE_FATAL; }
        if (!k) break;
        n += (size_t)k;
    }
    return read_setup(a, b, n);
}
int archive_read_open_filename(archive *a, const char *path, size_t block) {
    if (!a) return ARCHIVE_FATAL;
    int fd = path ? open(path, 0) : 0;
    if (fd < 0) { a_err(a, cx_errno, "não foi possível abrir o arquivo"); return ARCHIVE_FATAL; }
    int r = archive_read_open_fd(a, fd, block);
    if (path) close(fd);
    return r;
}
int archive_read_open_file(archive *a, const char *p, size_t b) { return archive_read_open_filename(a, p, b); }
static u64 oct(const u8 *p, int n) {
    if (p[0] & 0x80) { u64 v = 0; for (int i = 1; i < n; i++) v = v << 8 | p[i]; return v; }   /* base 256 do GNU */
    u64 v = 0;
    for (int i = 0; i < n && p[i]; i++) if (p[i] >= '0' && p[i] <= '7') v = v * 8 + (u64)(p[i] - '0');
    return v;
}
int archive_read_next_header2(archive *a, archive_entry *e) {
    if (!a || !e || !a->in) return ARCHIVE_FATAL;
    if (a->pad > (i64)a->ipos) a->ipos = (size_t)a->pad;   /* pula o que sobrou da entrada anterior (pad = próximo cabeçalho) */
    a->left = 0;
    char *longname = 0, *longlink = 0;
    for (;;) {
        if (a->ipos + 512 > a->inn) { free(longname); free(longlink); return ARCHIVE_EOF; }
        const u8 *h = a->in + a->ipos;
        int zero = 1;
        for (int i = 0; i < 512; i++) if (h[i]) { zero = 0; break; }
        if (zero) { free(longname); free(longlink); return ARCHIVE_EOF; }
        if (memcmp(h + 257, "ustar", 5) && a->ipos == 0) {
            int sum = 0;
            for (int i = 0; i < 512; i++) sum += i >= 148 && i < 156 ? ' ' : h[i];
            if ((u64)sum != oct(h + 148, 8)) { a_err(a, 79, "formato não reconhecido (o mOS só lê tar)"); return ARCHIVE_FATAL; }
        }
        u64 size = oct(h + 124, 12);
        char type = (char)h[156];
        const u8 *data = h + 512;
        if (a->ipos + 512 + size > a->inn) { a_err(a, 79, "tar truncado"); free(longname); free(longlink); return ARCHIVE_FATAL; }
        if (type == 'x' || type == 'L' || type == 'K' || type == 'g') {
            if (type == 'L' || type == 'K') {
                char *s = malloc(size + 1);
                if (s) { memcpy(s, data, size); s[size] = 0; }
                if (type == 'L') { free(longname); longname = s; } else { free(longlink); longlink = s; }
            } else if (type == 'x') {
                for (u64 p = 0; p < size;) {
                    u64 len = 0, q = p;
                    while (q < size && data[q] >= '0' && data[q] <= '9') len = len * 10 + (data[q++] - '0');
                    if (!len || p + len > size) break;
                    const u8 *kv = data + q + 1, *eq = kv;
                    while (eq < data + p + len && *eq != '=') eq++;
                    size_t vl = (size_t)(data + p + len - 1 - (eq + 1));
                    char *v = malloc(vl + 1);
                    if (v) { memcpy(v, eq + 1, vl); v[vl] = 0; }
                    if ((size_t)(eq - kv) == 4 && !memcmp(kv, "path", 4)) { free(longname); longname = v; }
                    else if ((size_t)(eq - kv) == 8 && !memcmp(kv, "linkpath", 8)) { free(longlink); longlink = v; }
                    else free(v);
                    p += len;
                }
            }
            a->ipos += 512 + (size_t)((size + 511) / 512 * 512);
            continue;
        }
        archive_entry_clear(e);
        char name[260];
        if (longname) archive_entry_set_pathname(e, longname);
        else {
            size_t pl = 0, nl = 0;
            while (pl < 155 && h[345 + pl]) pl++;
            while (nl < 100 && h[nl]) nl++;
            if (pl) { memcpy(name, h + 345, pl); name[pl] = '/'; memcpy(name + pl + 1, h, nl); name[pl + 1 + nl] = 0; }
            else { memcpy(name, h, nl); name[nl] = 0; }
            archive_entry_set_pathname(e, name);
        }
        u32 perm = (u32)oct(h + 100, 8) & 07777;
        u32 t = type == '5' ? AE_IFDIR : type == '2' ? AE_IFLNK : AE_IFREG;
        if (t == AE_IFLNK) {
            char ln[101]; size_t ll = 0;
            while (ll < 100 && h[157 + ll]) { ln[ll] = (char)h[157 + ll]; ll++; }
            ln[ll] = 0;
            archive_entry_set_symlink(e, longlink ? longlink : ln);
        }
        e->mode = t | perm;
        e->size = t == AE_IFREG ? (i64)size : 0;
        e->uid = (i64)oct(h + 108, 8); e->gid = (i64)oct(h + 116, 8);
        e->mtime = (i64)oct(h + 136, 12); e->has_mtime = 1;
        if (h[265]) { char u[33] = {0}; memcpy(u, h + 265, 32); archive_entry_set_uname(e, u); }
        if (h[297]) { char g[33] = {0}; memcpy(g, h + 297, 32); archive_entry_set_gname(e, g); }
        free(longname); free(longlink);
        a->ipos += 512;
        a->left = (i64)size;
        a->pad = (i64)(a->ipos + (size_t)((size + 511) / 512 * 512));
        return ARCHIVE_OK;
    }
}
int archive_read_next_header(archive *a, archive_entry **out) {
    if (!a || !out) return ARCHIVE_FATAL;
    if (!a->cur) a->cur = archive_entry_new();
    int r = archive_read_next_header2(a, a->cur);
    *out = r == ARCHIVE_OK ? a->cur : 0;
    return r;
}
long archive_read_data(archive *a, void *buf, size_t n) {
    if (!a || !a->in) return ARCHIVE_FATAL;
    if ((i64)n > a->left) n = (size_t)a->left;
    memcpy(buf, a->in + a->ipos, n);
    a->ipos += n;
    a->left -= (i64)n;
    if (!a->left && a->pad > (i64)a->ipos) a->ipos = (size_t)a->pad;
    return (long)n;
}
int archive_read_data_skip(archive *a) { if (!a) return ARCHIVE_FATAL; return ARCHIVE_OK; }   /* o próximo header pula */
/* extração: cria a pasta, o arquivo ou o link no caminho da entrada */
static int mkparents(const char *p) {
    char b[1024];
    size_t n = strlen(p);
    if (n >= sizeof b) return -1;
    memcpy(b, p, n + 1);
    for (size_t i = 1; i < n; i++) if (b[i] == '/') { b[i] = 0; mkdir(b, 0755); b[i] = '/'; }
    return 0;
}
int archive_read_extract(archive *a, archive_entry *e, int flags) {
    (void)flags;
    if (!a || !e || !e->path) return ARCHIVE_FATAL;
    if (e->path[0] == '/' || (e->path[0] == '.' && e->path[1] == '.' && (e->path[2] == '/' || !e->path[2]))) {
        a_err(a, 1, "caminho fora da pasta de extração recusado");
        return ARCHIVE_FATAL;
    }
    for (const char *p = e->path; *p; p++) if (p[0] == '/' && p[1] == '.' && p[2] == '.' && (p[3] == '/' || !p[3])) {
        a_err(a, 1, "caminho fora da pasta de extração recusado");
        return ARCHIVE_FATAL;
    }
    mkparents(e->path);
    u32 t = e->mode & AE_IFMT;
    if (t == AE_IFDIR) { mkdir(e->path, (int)(e->mode & 07777 ? e->mode & 07777 : 0755)); return ARCHIVE_OK; }
    if (t == AE_IFLNK) return symlink(e->link ? e->link : "", e->path) == 0 ? ARCHIVE_OK : ARCHIVE_WARN;
    int fd = open(e->path, O_WR_, (int)(e->mode & 07777 ? e->mode & 07777 : 0644));
    if (fd < 0) { a_err(a, cx_errno, "não foi possível criar o arquivo"); return ARCHIVE_FATAL; }
    u8 buf[8192];
    long k;
    while ((k = archive_read_data(a, buf, sizeof buf)) > 0)
        if (write(fd, buf, (unsigned long)k) != k) { close(fd); a_err(a, 5, "erro de escrita"); return ARCHIVE_FATAL; }
    close(fd);
    chmod(e->path, (int)(e->mode & 07777 ? e->mode & 07777 : 0644));
    return ARCHIVE_OK;
}
int archive_read_close(archive *a) { if (!a) return ARCHIVE_FATAL; free(a->in); a->in = 0; a->inn = a->ipos = 0; return ARCHIVE_OK; }
int archive_read_free(archive *a) {
    if (!a) return ARCHIVE_OK;
    archive_read_close(a);
    archive_entry_free(a->cur);
    free(a);
    return ARCHIVE_OK;
}
int archive_read_finish(archive *a) { return archive_read_free(a); }

/* ---------------------------------------------------------------- disco */
archive *archive_write_disk_new(void) { archive *a = archive_write_new(); if (a) a->reading = 2; return a; }
int archive_write_disk_set_options(archive *a, int opts) { if (a) a->ext_opts = opts; return ARCHIVE_OK; }
int archive_write_disk_set_standard_lookup(archive *a) { (void)a; return ARCHIVE_OK; }
archive *archive_read_disk_new(void) { return archive_read_new(); }
int archive_read_disk_set_standard_lookup(archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_disk_set_symlink_logical(archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_disk_set_symlink_physical(archive *a) { (void)a; return ARCHIVE_OK; }
/* struct stat ($INODE64) do macOS: modo em +4 (u16), uid +16, gid +20, mtime +48, tamanho +96 */
int archive_read_disk_entry_from_file(archive *a, archive_entry *e, int fd, const void *st) {
    (void)fd;
    u8 local[144];
    if (!e) return ARCHIVE_FATAL;
    if (!st) {
        if (!e->path || lstat(e->path, local)) { a_err(a, cx_errno, "stat falhou"); return ARCHIVE_FATAL; }
        st = local;
    }
    const u8 *s = st;
    u16 mode; memcpy(&mode, s + 4, 2);
    u32 uid, gid; memcpy(&uid, s + 16, 4); memcpy(&gid, s + 20, 4);
    i64 mt, sz; memcpy(&mt, s + 48, 8); memcpy(&sz, s + 96, 8);
    e->mode = mode; e->uid = uid; e->gid = gid; e->mtime = mt; e->has_mtime = 1;
    e->size = (mode & AE_IFMT) == AE_IFREG ? sz : 0;
    return ARCHIVE_OK;
}
