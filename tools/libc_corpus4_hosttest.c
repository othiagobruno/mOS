/*
 * mOS — teste de host de userland/libsystem/libc_corpus4.c (prefixo mos_). Rodar: tools/libc_corpus4_hosttest.sh
 * ndbm com arquivo de verdade (gravar, fechar, reabrir, apagar), open_memstream/fmemopen, hash_*, swscanf, strfmon,
 * inet_net_pton, __udivmodti4, __truncsfhf2, inversas do simd e os complexos contra a libm do host.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <wchar.h>
#include <math.h>
#include <complex.h>

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

typedef struct { void *dptr; size_t dsize; } datum;
void *mos_dbm_open(const char *, int, int);
void mos_dbm_close(void *);
datum mos_dbm_fetch(void *, datum);
int mos_dbm_store(void *, datum, datum, int);
int mos_dbm_delete(void *, datum);
datum mos_dbm_firstkey(void *);
datum mos_dbm_nextkey(void *);
FILE *mos_open_memstream(char **, size_t *);
FILE *mos_fmemopen(void *, size_t, const char *);
void *mos_hash_create(int);
void *mos_hash_search(void *, char *, void *, void (*)(void *));
int mos_swscanf(const wchar_t *, const wchar_t *, ...);
long mos_strfmon(char *, size_t, const char *, ...);
int mos_inet_net_pton(int, const char *, void *, size_t);
unsigned __int128 mos___udivmodti4(unsigned __int128, unsigned __int128, unsigned __int128 *);
unsigned short mos___gnu_f2h_ieee(float);
double _Complex mos_csin(double _Complex), mos_ccos(double _Complex), mos_ctan(double _Complex),
    mos_casinh(double _Complex), mos_cacosh(double _Complex), mos_casin(double _Complex), mos_cacos(double _Complex),
    mos_catan(double _Complex), mos_ctanh(double _Complex), mos_csinh(double _Complex), mos_ccosh(double _Complex);
typedef double d4v __attribute__((ext_vector_type(4)));
typedef struct { d4v c[4]; } simd_double4x4;
simd_double4x4 mos___invert_d4(simd_double4x4);
int mos_wcsspn(const wchar_t *, const wchar_t *);
char *mos_yperr_string(int);
int mos_yp_get_default_domain(char **);

static int fails, total;
#define CK(c, m) do { total++; if (!(c)) { fails++; printf("FALHA: %s\n", m); } } while (0)
static datum D(const char *s) { datum d = {(void *)s, strlen(s)}; return d; }
static int ceq(double _Complex a, double _Complex b) { return cabs(a - b) <= 1e-9 * (1 + cabs(b)); }

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "/tmp";
    char base[512];
    snprintf(base, sizeof base, "%s/mosdbm", dir);
    char path[520];
    snprintf(path, sizeof path, "%s.db", base);
    unlink(path);
    /* ---- ndbm */
    void *d = mos_dbm_open(base, 2 | 0x200, 0644);
    CK(d != 0, "dbm_open cria");
    CK(mos_dbm_store(d, D("um"), D("1"), 0) == 0, "dbm_store insere");
    CK(mos_dbm_store(d, D("dois"), D("22"), 0) == 0, "dbm_store insere 2");
    CK(mos_dbm_store(d, D("um"), D("x"), 0) == 1, "dbm_store DBM_INSERT existente devolve 1");
    CK(mos_dbm_store(d, D("um"), D("111"), 1) == 0, "dbm_store DBM_REPLACE");
    mos_dbm_close(d);
    d = mos_dbm_open(base, 2, 0);
    CK(d != 0, "dbm_open reabre");
    datum v = mos_dbm_fetch(d, D("um"));
    CK(v.dsize == 3 && !memcmp(v.dptr, "111", 3), "dbm_fetch depois de reabrir");
    int n = 0;
    for (datum k = mos_dbm_firstkey(d); k.dptr; k = mos_dbm_nextkey(d)) n++;
    CK(n == 2, "dbm_firstkey/nextkey percorre 2");
    CK(mos_dbm_delete(d, D("dois")) == 0, "dbm_delete");
    CK(mos_dbm_fetch(d, D("dois")).dptr == 0, "dbm_fetch apagado");
    mos_dbm_close(d);
    d = mos_dbm_open(base, 0, 0);
    CK(d && mos_dbm_fetch(d, D("dois")).dptr == 0 && mos_dbm_fetch(d, D("um")).dsize == 3, "apagar persistiu");
    CK(mos_dbm_store(d, D("z"), D("z"), 1) < 0, "dbm_store só leitura falha");
    mos_dbm_close(d);
    unlink(path);
    /* ---- open_memstream */
    char *buf = 0; size_t sz = 0;
    FILE *f = mos_open_memstream(&buf, &sz);
    CK(f != 0, "open_memstream");
    fprintf(f, "ola %d", 42);
    fflush(f);
    CK(sz == 6 && !strcmp(buf, "ola 42"), "open_memstream depois do fflush");
    for (int i = 0; i < 1000; i++) fputc('a' + i % 26, f);
    fclose(f);
    CK(sz == 1006 && buf[1006] == 0 && buf[6] == 'a', "open_memstream cresce e termina com NUL");
    free(buf);
    /* ---- fmemopen */
    char fb[16] = "abc def";
    f = mos_fmemopen(fb, 7, "r");
    char w1[8], w2[8];
    CK(f && fscanf(f, "%7s %7s", w1, w2) == 2 && !strcmp(w1, "abc") && !strcmp(w2, "def"), "fmemopen leitura");
    if (f) fclose(f);
    char ob[8];
    f = mos_fmemopen(ob, sizeof ob, "w");
    fputs("12345", f);
    fflush(f);
    CK(!strcmp(ob, "12345"), "fmemopen escrita com NUL");
    fclose(f);
    /* ---- hash */
    void *t = mos_hash_create(7);
    mos_hash_search(t, "a", "A", 0);
    mos_hash_search(t, "b", "B", 0);
    CK(!strcmp(mos_hash_search(t, "a", 0, 0), "A") && mos_hash_search(t, "c", 0, 0) == 0, "hash_search procura");
    mos_hash_search(t, "a", "AA", 0);
    CK(!strcmp(mos_hash_search(t, "a", 0, 0), "AA"), "hash_search substitui");
    /* ---- wide */
    int a = 0; double x = 0;
    CK(mos_swscanf(L"17 2.5", L"%d %lf", &a, &x) == 2 && a == 17 && x == 2.5, "swscanf");
    CK(mos_wcsspn(L"aabbcx", L"ab") == 4, "wcsspn");
    /* ---- strfmon */
    char m[64];
    CK(mos_strfmon(m, sizeof m, "%n", 1234.5) == 7 && !strcmp(m, "1234.50"), "strfmon %n");
    CK(mos_strfmon(m, sizeof m, "[%10.1i]", 3.14159) > 0 && !strcmp(m, "[       3.1]"), "strfmon largura/precisão");
    /* ---- inet_net_pton */
    unsigned char ip[4] = {0};
    CK(mos_inet_net_pton(2, "10.1.0.0/16", ip, 4) == 16 && ip[0] == 10 && ip[1] == 1, "inet_net_pton /16");
    CK(mos_inet_net_pton(2, "192.168.1.7", ip, 4) == 32 && ip[3] == 7, "inet_net_pton sem barra");
    CK(mos_inet_net_pton(2, "300.1.1.1", ip, 4) == -1, "inet_net_pton inválido");
    /* ---- compiler-rt */
    unsigned __int128 r, q = mos___udivmodti4(((unsigned __int128)1 << 100) + 12345, 1000003, &r);
    unsigned __int128 big = ((unsigned __int128)1 << 100) + 12345;
    CK(q == big / 1000003 && r == big % 1000003, "__udivmodti4");
    CK(mos___gnu_f2h_ieee(1.0f) == 0x3c00 && mos___gnu_f2h_ieee(-2.0f) == 0xc000 && mos___gnu_f2h_ieee(65504.f) == 0x7bff &&
       mos___gnu_f2h_ieee(1e6f) == 0x7c00 && mos___gnu_f2h_ieee(5.96046448e-8f) == 0x0001, "float -> half");
    /* ---- simd */
    simd_double4x4 M = {{{2, 0, 0, 0}, {0, 4, 0, 0}, {0, 0, 1, 3}, {1, 0, 0, 1}}};
    simd_double4x4 Inv = mos___invert_d4(M);
    int ok = 1;
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {
        double s = 0;
        for (int k = 0; k < 4; k++) s += M.c[k][i] * Inv.c[j][k];
        if (fabs(s - (i == j)) > 1e-12) ok = 0;
    }
    CK(ok, "__invert_d4: M * inv(M) = I");
    /* ---- complexos */
    double _Complex z = 0.7 + 0.3 * I;
    CK(ceq(mos_csin(z), csin(z)) && ceq(mos_ccos(z), ccos(z)) && ceq(mos_ctan(z), ctan(z)), "csin/ccos/ctan");
    CK(ceq(mos_csinh(z), csinh(z)) && ceq(mos_ccosh(z), ccosh(z)) && ceq(mos_ctanh(z), ctanh(z)), "csinh/ccosh/ctanh");
    CK(ceq(mos_casin(z), casin(z)) && ceq(mos_cacos(z), cacos(z)) && ceq(mos_catan(z), catan(z)), "casin/cacos/catan");
    CK(ceq(mos_casinh(z), casinh(z)) && ceq(mos_cacosh(z), cacosh(z)), "casinh/cacosh");
    /* ---- NIS */
    char *dom = (char *)1;
    CK(mos_yp_get_default_domain(&dom) == 12 && dom == 0 && !strcmp(mos_yperr_string(12), "Local domain name not set"), "NIS sem domínio");
    printf("libc-corpus4: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
