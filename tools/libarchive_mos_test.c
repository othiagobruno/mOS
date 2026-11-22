/*
 * mOS — teste de host de userland/libsystem/libarchive_mos.c (prefixo mos_). Rodar: tools/libarchive_mos_test.sh
 * Escreve um .tar.gz (com nome de 150+ caracteres, pasta e link) que o tar do host lista, e lê de volta um .tar.bz2 e
 * um .tar.gz gerados pelo tar do host, extraindo para uma pasta.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>
#include <bzlib.h>
#include <sys/stat.h>

/* o código do libSystem passa ao open os O_* do macOS (a ABI do app); aqui eles viram os do host */
int open(const char *path, int flags, ...) {
    va_list ap; va_start(ap, flags); int mode = va_arg(ap, int); va_end(ap);
    int h = flags & 3;
    if (flags & 0x200) h |= O_CREAT;
    if (flags & 0x400) h |= O_TRUNC;
    if (flags & 0x800) h |= O_EXCL;
    if (flags & 0x8) h |= O_APPEND;
    return openat(AT_FDCWD, path, h, mode);   /* o open do host está encoberto por este */
}
/* o deflate cru do libSystem (compression_z.c), aqui pela zlib do host */
size_t cz_raw_buffer(unsigned char *dst, size_t dn, const unsigned char *src, size_t sn, int encode) {
    z_stream z; memset(&z, 0, sizeof z);
    if ((encode ? deflateInit2(&z, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) : inflateInit2(&z, -15)) != Z_OK) return 0;
    z.next_in = (unsigned char *)src; z.avail_in = (unsigned)sn; z.next_out = dst; z.avail_out = (unsigned)dn;
    int r = encode ? deflate(&z, Z_FINISH) : inflate(&z, Z_FINISH);
    size_t n = z.total_out;
    if (encode) deflateEnd(&z); else inflateEnd(&z);
    return r == Z_STREAM_END ? n : 0;
}
int lzma_stream_buffer_decode(void *a, unsigned b, const void *c, const void *d, size_t *e, size_t f, void *g, size_t *h, size_t i) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i; return 7;
}
#ifndef __APPLE__
int *__error(void) { extern int *__errno_location(void); return __errno_location(); }
#endif

typedef struct archive archive; typedef struct archive_entry archive_entry;
archive *mos_archive_write_new(void); int mos_archive_write_set_format_pax_restricted(archive *); int mos_archive_write_add_filter_gzip(archive *);
int mos_archive_write_open_filename(archive *, const char *); int mos_archive_write_header(archive *, archive_entry *);
long mos_archive_write_data(archive *, const void *, size_t); int mos_archive_write_close(archive *); int mos_archive_write_free(archive *);
int mos_archive_write_open_memory(archive *, void *, size_t, size_t *);
archive_entry *mos_archive_entry_new(void); void mos_archive_entry_free(archive_entry *); void mos_archive_entry_set_pathname(archive_entry *, const char *);
void mos_archive_entry_set_size(archive_entry *, long long); void mos_archive_entry_set_filetype(archive_entry *, unsigned);
void mos_archive_entry_set_perm(archive_entry *, unsigned); void mos_archive_entry_set_symlink(archive_entry *, const char *);
archive_entry *mos_archive_entry_clear(archive_entry *); const char *mos_archive_entry_pathname(archive_entry *);
long long mos_archive_entry_size(archive_entry *); unsigned mos_archive_entry_filetype(archive_entry *);
archive *mos_archive_read_new(void); int mos_archive_read_support_filter_all(archive *); int mos_archive_read_support_format_all(archive *);
int mos_archive_read_open_filename(archive *, const char *, size_t); int mos_archive_read_open_memory(archive *, const void *, size_t);
int mos_archive_read_next_header(archive *, archive_entry **); long mos_archive_read_data(archive *, void *, size_t);
int mos_archive_read_extract(archive *, archive_entry *, int); int mos_archive_read_free(archive *); const char *mos_archive_error_string(archive *);

static int fails, total;
#define CK(c, m) do { total++; if (!(c)) { fails++; printf("FALHA: %s\n", m); } } while (0)

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "/tmp";
    char tgz[600], cmd[2048], longname[200];
    snprintf(tgz, sizeof tgz, "%s/mos.tar.gz", dir);
    memset(longname, 0, sizeof longname);
    strcpy(longname, "pasta/");
    for (int i = 0; i < 15; i++) strcat(longname, "nomecomprido");
    strcat(longname, ".txt");                           /* > 150 caracteres: precisa do cabeçalho pax */
    archive *a = mos_archive_write_new();
    mos_archive_write_set_format_pax_restricted(a);
    mos_archive_write_add_filter_gzip(a);
    CK(mos_archive_write_open_filename(a, tgz) == 0, "write_open_filename");
    archive_entry *e = mos_archive_entry_new();
    mos_archive_entry_set_pathname(e, "pasta"); mos_archive_entry_set_filetype(e, 0040000); mos_archive_entry_set_perm(e, 0755);
    CK(mos_archive_write_header(a, e) == 0, "header da pasta");
    mos_archive_entry_clear(e);
    const char *txt = "olá, tar do mOS\n";
    mos_archive_entry_set_pathname(e, "pasta/a.txt"); mos_archive_entry_set_size(e, (long long)strlen(txt)); mos_archive_entry_set_perm(e, 0644);
    mos_archive_write_header(a, e);
    CK(mos_archive_write_data(a, txt, strlen(txt)) == (long)strlen(txt), "write_data");
    mos_archive_entry_clear(e);
    mos_archive_entry_set_pathname(e, longname); mos_archive_entry_set_size(e, 3); mos_archive_entry_set_perm(e, 0600);
    mos_archive_write_header(a, e);
    mos_archive_write_data(a, "xyz", 3);
    mos_archive_entry_clear(e);
    mos_archive_entry_set_pathname(e, "pasta/link"); mos_archive_entry_set_filetype(e, 0120000); mos_archive_entry_set_symlink(e, "a.txt");
    mos_archive_write_header(a, e);
    CK(mos_archive_write_close(a) == 0, "write_close (gzip)");
    mos_archive_write_free(a);
    snprintf(cmd, sizeof cmd, "tar -tzf '%s' > '%s/list.txt' 2>&1", tgz, dir);
    CK(system(cmd) == 0, "o tar do host lê o .tar.gz do mOS");
    char lp[600]; snprintf(lp, sizeof lp, "%s/list.txt", dir);
    FILE *lf = fopen(lp, "r"); char line[400]; int saw_long = 0, saw_link = 0;
    while (lf && fgets(line, sizeof line, lf)) { if (strstr(line, "nomecomprido.txt")) saw_long = 1; if (strstr(line, "pasta/link")) saw_link = 1; }
    if (lf) fclose(lf);
    CK(saw_long && saw_link, "nome longo (pax) e link na listagem do host");
    /* em memória, sem filtro, e relido */
    static unsigned char mem[1 << 16];
    size_t used = 0;
    a = mos_archive_write_new();
    mos_archive_write_open_memory(a, mem, sizeof mem, &used);
    mos_archive_entry_clear(e);
    mos_archive_entry_set_pathname(e, "m.bin"); mos_archive_entry_set_size(e, 4);
    mos_archive_write_header(a, e); mos_archive_write_data(a, "\x01\x02\x03\x04", 4);
    mos_archive_write_close(a); mos_archive_write_free(a);
    if (used != 512 * 4) printf("  usado: %zu\n", used);
    CK(used == 512 * 4, "tar em memória: cabeçalho + dados + 2 blocos zerados");
    archive *r = mos_archive_read_new();
    mos_archive_read_support_filter_all(r); mos_archive_read_support_format_all(r);
    archive_entry *re;
    unsigned char got[8];
    CK(mos_archive_read_open_memory(r, mem, used) == 0 && mos_archive_read_next_header(r, &re) == 0 && !strcmp(mos_archive_entry_pathname(re), "m.bin") &&
       mos_archive_read_data(r, got, 8) == 4 && got[3] == 4 && mos_archive_read_next_header(r, &re) == 1, "ler o tar da memória");
    mos_archive_read_free(r);
    /* .tar.bz2 do host: listar, ler e extrair */
    snprintf(cmd, sizeof cmd, "cd '%s' && rm -rf src out && mkdir -p src/sub out && printf 'conteudo' > src/sub/f.txt && printf 'x%%.0s' $(seq 1 3000) > src/grande.bin && "
             "COPYFILE_DISABLE=1 tar -cjf host.tar.bz2 -C src . && COPYFILE_DISABLE=1 tar -czf host.tar.gz -C src .", dir);
    CK(system(cmd) == 0, "tar do host cria os arquivos");
    const char *names[2] = {"host.tar.bz2", "host.tar.gz"};
    for (int k = 0; k < 2; k++) {
        char p[700]; snprintf(p, sizeof p, "%s/%s", dir, names[k]);
        r = mos_archive_read_new();
        int ok = mos_archive_read_open_filename(r, p, 10240) == 0, files = 0, big = 0;
        char cwd[1024]; getcwd(cwd, sizeof cwd);
        char out[700]; snprintf(out, sizeof out, "%s/out", dir);
        chdir(out);
        while (ok && mos_archive_read_next_header(r, &re) == 0) {
            if (mos_archive_entry_filetype(re) == 0100000) { files++; if (mos_archive_entry_size(re) == 3000) big = 1; }
            if (mos_archive_read_extract(r, re, 0)) ok = 0;
        }
        chdir(cwd);
        mos_archive_read_free(r);
        if (!(ok && files == 2 && big)) printf("  %s: ok=%d arquivos=%d grande=%d\n", names[k], ok, files, big);
        CK(ok && files == 2 && big, names[k]);
    }
    char fp[700]; snprintf(fp, sizeof fp, "%s/out/sub/f.txt", dir);
    FILE *ff = fopen(fp, "r"); char buf[32] = {0};
    if (ff) { fread(buf, 1, sizeof buf - 1, ff); fclose(ff); }
    CK(!strcmp(buf, "conteudo"), "extração gravou o conteúdo");
    mos_archive_entry_free(e);
    printf("libarchive-mos: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
