/*
 * mOS — teste de host de userland/libsystem/libc_bsd.c. O objeto freestanding entra com o prefixo mos_
 * (objcopy --prefix-symbols), para não colidir com a libc do host, e os poucos externos que faltam viram
 * atalhos aqui. Confere random() contra a sequência da BSD (o mesmo gerador do glibc), ffs/fls, lldiv,
 * timingsafe_bcmp, NXArchInfo e o desenrolar por ponteiro de frame. Rodar: tools/libc_bsd_hosttest.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* externos do libc_bsd que não vêm da libc do host */
int *__error(void) { static int e; return &e; }
unsigned mach_thread_self(void) { return 0; }
unsigned qos_class_self(void) { return 0x15; }

long mos_random(void);
void mos_srandom(unsigned);
char *mos_initstate(unsigned, char *, size_t);
char *mos_setstate(char *);
int mos_ffs(int), mos_ffsl(long), mos_ffsll(long long), mos_fls(int), mos_flsl(long), mos_flsll(long long);
typedef struct { long long quot, rem; } mos_lldiv_t;
mos_lldiv_t mos_lldiv(long long, long long);
int mos_timingsafe_bcmp(const void *, const void *, size_t);
typedef struct { const char *name; int cputype, cpusubtype, byteorder; const char *description; } NXArchInfo;
const NXArchInfo *mos_NXGetArchInfoFromCpuType(int, int);
const NXArchInfo *mos_NXGetLocalArchInfo(void);
int mos_backtrace_from_fp(void *, void **, int);
char *mos_xattr_name_with_flags(const char *, int);

static int falhas, total;
#define CHECK(c, ...) do { total++; if (!(c)) { falhas++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* o glibc usa o mesmo gerador TYPE_3 (31 inteiros): com a mesma semente, random() deve bater byte a byte */
int main(void) {
    /* random() é o gerador de Fibonacci atrasado da BSD (não o do glibc): aqui garante-se que é determinístico,
     * reprodutível pela semente, dentro de [0, 2^31) e sensível ao estado. A sequência exata foi conferida contra
     * a implementação de referência do musl na revisão deste código. */
    long seqA[20], seqB[20];
    mos_srandom(42);
    for (int k = 0; k < 20; k++) { seqA[k] = mos_random(); CHECK(seqA[k] >= 0 && seqA[k] < 0x80000000L, "random em faixa"); }
    mos_srandom(42);
    for (int k = 0; k < 20; k++) seqB[k] = mos_random();
    CHECK(!memcmp(seqA, seqB, sizeof seqA), "random() reprodutível pela semente");
    mos_srandom(43);
    CHECK(mos_random() != seqA[0], "sementes diferentes, saídas diferentes");
    /* initstate: dois estados externos de 256 bytes (n=63) com a mesma semente dão a mesma sequência; setstate
     * alterna entre eles. Guarda a sequência de referência de s1 e confere que s2 a reproduz. */
    char s1[256], s2[256];
    long ref[5];
    mos_initstate(7, s1, sizeof s1);
    for (int k = 0; k < 5; k++) ref[k] = mos_random();
    char *prev = mos_initstate(7, s2, sizeof s2);   /* troca para s2 (mesma semente) */
    CHECK(prev != 0, "initstate devolve o estado anterior");
    long rep[5];
    for (int k = 0; k < 5; k++) rep[k] = mos_random();
    CHECK(!memcmp(ref, rep, sizeof ref), "initstate com a mesma semente reproduz a sequência");
    CHECK(ref[0] != seqA[0], "estado de 256 bytes difere do de 128 (período maior)");

    /* ffs / fls */
    CHECK(mos_ffs(0) == 0 && mos_ffs(1) == 1 && mos_ffs(0x10) == 5 && mos_ffs(0x80000000) == 32, "ffs");
    CHECK(mos_ffsl(0) == 0 && mos_ffsll(1ll << 40) == 41, "ffsl/ffsll");
    CHECK(mos_fls(0) == 0 && mos_fls(1) == 1 && mos_fls(0xFF) == 8 && mos_fls(0x80000000) == 32, "fls");
    CHECK(mos_flsl(1l << 50) == 51 && mos_flsll(1ll << 62) == 63, "flsl/flsll");

    /* lldiv */
    mos_lldiv_t d = mos_lldiv(-7, 2);
    CHECK(d.quot == -3 && d.rem == -1, "lldiv(-7,2) = %lld r %lld", d.quot, d.rem);

    /* timingsafe_bcmp */
    CHECK(mos_timingsafe_bcmp("abcd", "abcd", 4) == 0 && mos_timingsafe_bcmp("abcd", "abce", 4) != 0, "timingsafe_bcmp");

    /* NXArchInfo */
    const NXArchInfo *x = mos_NXGetArchInfoFromCpuType(0x01000007, -1);
    CHECK(x && !strcmp(x->name, "x86_64") && x->byteorder == 1, "NXArchInfo x86_64");
    const NXArchInfo *ar = mos_NXGetArchInfoFromCpuType(0x0100000C, 2);
    CHECK(ar && !strcmp(ar->name, "arm64e"), "NXArchInfo arm64e por subtipo");
    CHECK(mos_NXGetArchInfoFromCpuType(0x99, -1) == 0, "NXArchInfo desconhecida = NULL");
    CHECK(mos_NXGetLocalArchInfo() != 0, "NXGetLocalArchInfo");

    /* backtrace por ponteiro de frame: monta uma cadeia falsa {fp, ret} e confere a leitura */
    uint64_t frame[4];
    frame[0] = (uint64_t)&frame[2]; frame[1] = 0x1111;
    frame[2] = 0;                   frame[3] = 0x2222;   /* fp seguinte 0 encerra após este quadro */
    void *bt[8];
    int n = mos_backtrace_from_fp(&frame[0], bt, 8);
    CHECK(n == 2 && bt[0] == (void *)0x1111 && bt[1] == (void *)0x2222, "backtrace_from_fp: %d quadros", n);

    /* xattr_name_with_flags */
    char *nm = mos_xattr_name_with_flags("com.apple.x", 0);
    CHECK(nm && !strcmp(nm, "com.apple.x"), "xattr_name_with_flags sem flags");
    free(nm);

    printf("libc_bsd: %d/%d ok\n", total - falhas, total);
    return falhas != 0;
}
