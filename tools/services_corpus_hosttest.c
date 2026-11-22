/*
 * mOS — teste de host de userland/libsystem/services_corpus.c (prefixo mos_). Rodar: tools/services_corpus_hosttest.sh
 * vis/strunvis (ida e volta), wordexp, fmtcheck, fcvt/gcvt, registros TXT do DNS-SD, GLU, __powidf2, OpenCL sem
 * plataforma e o caminho de erro do vmnet.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef __APPLE__
const char *__progname = "teste";
#endif
char *mos_vis(char *, int, int, int);
int mos_strvis(char *, const char *, int);
int mos_strunvis(char *, const char *);
typedef struct { size_t we_wordc; char **we_wordv; size_t we_offs; } wordexp_t;
int mos_wordexp(const char *, wordexp_t *, int);
void mos_wordfree(wordexp_t *);
const char *mos_fmtcheck(const char *, const char *);
char *mos_fcvt(double, int, int *, int *);
char *mos_gcvt(double, int, char *);
typedef struct { unsigned char *buf; unsigned short cap, len; unsigned char own; } txtref_t;
void mos_TXTRecordCreate(void *, unsigned short, void *);
int mos_TXTRecordSetValue(void *, const char *, unsigned char, const void *);
const void *mos_TXTRecordGetValuePtr(unsigned short, const void *, const char *, unsigned char *);
int mos_TXTRecordContainsKey(unsigned short, const void *, const char *);
unsigned short mos_TXTRecordGetCount(unsigned short, const void *);
void mos_TXTRecordDeallocate(void *);
unsigned char mos_gluCheckExtension(const unsigned char *, const unsigned char *);
int mos_gluUnProject(double, double, double, const double *, const double *, const int *, double *, double *, double *);
double mos___powidf2(double, int);
int mos_clGetPlatformIDs(unsigned, void **, unsigned *);
void *mos_vmnet_start_interface(void *, void *, void *);
char *mos_tempnam(const char *, const char *);

static int fails, total;
#define CK(c, m) do { total++; if (!(c)) { fails++; printf("FALHA: %s\n", m); } } while (0)

int main(void) {
    char v[256], u[256];
    const char *src = "a b\tc\n\\x\x01\xe9";
    mos_strvis(v, src, 0x02 | 0x04 | 0x08 | 0x10);      /* VIS_CSTYLE|SP|TAB|NL */
    CK(!strchr(v, ' ') && !strchr(v, '\t') && !strchr(v, '\n'), "strvis escapa espaço, tab e newline");
    CK(mos_strunvis(u, v) == (int)strlen(src) && !strcmp(u, src), "strunvis desfaz o strvis");
    mos_strvis(v, "\x7f", 0);
    CK(!strcmp(v, "\\^?"), "vis DEL em ^?");
    wordexp_t w = {0};
    setenv("MOS_T", "valor", 1);
    CK(mos_wordexp("um 'dois tres' \"q$MOS_T\" ${MOS_T}x a\\ b", &w, 0) == 0 && w.we_wordc == 5, "wordexp conta 5");
    CK(w.we_wordc == 5 && !strcmp(w.we_wordv[1], "dois tres") && !strcmp(w.we_wordv[2], "qvalor") &&
       !strcmp(w.we_wordv[3], "valorx") && !strcmp(w.we_wordv[4], "a b") && !w.we_wordv[5], "wordexp aspas, $VAR, ${VAR} e \\");
    mos_wordfree(&w);
    CK(mos_wordexp("a `ls`", &w, 0) == 3, "wordexp recusa substituição de comando");
    CK(mos_wordexp("a | b", &w, 0) == 6, "wordexp recusa metacaractere");
    CK(!strcmp(mos_fmtcheck("%d itens", "%d x"), "%d itens") && !strcmp(mos_fmtcheck("%s", "%d"), "%d"), "fmtcheck");
    int dec, sign;
    char *f = mos_fcvt(-12.345, 2, &dec, &sign);
    CK(!strcmp(f, "1235") && dec == 2 && sign == 1, "fcvt");
    char g[64];
    CK(!strcmp(mos_gcvt(0.0001234, 3, g), "0.000123"), "gcvt");
    txtref_t t;
    mos_TXTRecordCreate(&t, 0, 0);
    mos_TXTRecordSetValue(&t, "txtvers", 1, "1");
    mos_TXTRecordSetValue(&t, "path", 5, "/apps");
    mos_TXTRecordSetValue(&t, "flag", 0, 0);
    unsigned char vl = 0;
    const char *pv = mos_TXTRecordGetValuePtr(t.len, t.buf, "path", &vl);
    CK(pv && vl == 5 && !memcmp(pv, "/apps", 5), "TXTRecordGetValuePtr");
    CK(mos_TXTRecordContainsKey(t.len, t.buf, "flag") && !mos_TXTRecordContainsKey(t.len, t.buf, "pat") &&
       mos_TXTRecordGetCount(t.len, t.buf) == 3, "TXTRecord chaves e contagem");
    mos_TXTRecordDeallocate(&t);
    CK(mos_gluCheckExtension((const unsigned char *)"GL_EXT_x", (const unsigned char *)"GL_EXT_xy GL_EXT_x GL_ARB_z") &&
       !mos_gluCheckExtension((const unsigned char *)"GL_EXT", (const unsigned char *)"GL_EXT_xy"), "gluCheckExtension palavra inteira");
    double id[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    int vp[4] = {0, 0, 200, 100};
    double ox, oy, oz;
    CK(mos_gluUnProject(150, 25, 0.5, id, id, vp, &ox, &oy, &oz) && fabs(ox - 0.5) < 1e-12 && fabs(oy + 0.5) < 1e-12 && fabs(oz) < 1e-12,
       "gluUnProject com identidade");
    CK(mos___powidf2(2.0, 10) == 1024.0 && mos___powidf2(2.0, -2) == 0.25, "__powidf2");
    unsigned n = 9;
    void *pl;
    CK(mos_clGetPlatformIDs(1, &pl, &n) == -1001 && n == 0, "OpenCL sem plataforma");
    CK(mos_vmnet_start_interface(0, 0, 0) == 0, "vmnet sem interface");
    char *tn = mos_tempnam(0, "mos");
    CK(tn && strstr(tn, "mos"), "tempnam");
    free(tn);
    printf("services-corpus: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
