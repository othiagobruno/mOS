#define _GNU_SOURCE
/*
 * mOS — teste de host de userland/libsystem/libc_apps.c. O objeto entra com o prefixo mos_. Os externos vêm da libc
 * do host (setenv/unsetenv/setpgid/ctermid), menos sysctlbyname (que o Linux não tem): aqui um stub devolve uma
 * loadavg conhecida para conferir o cálculo. Rodar: tools/libc_apps_hosttest.sh
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int *__error(void) { static int e; return &e; }

/* stub do sysctl vm.loadavg: struct loadavg {u32 ldavg[3]; long fscale} */
int sysctlbyname(const char *name, void *oldp, size_t *oldlen, void *newp, size_t newlen) {
    (void)newp; (void)newlen;
    if (strcmp(name, "vm.loadavg") == 0 && oldp && oldlen && *oldlen >= 16) {
        struct { unsigned ldavg[3]; long fscale; } *la = oldp;
        la->ldavg[0] = 2000; la->ldavg[1] = 1000; la->ldavg[2] = 500; la->fscale = 1000;  /* 2.0, 1.0, 0.5 */
        return 0;
    }
    return -1;
}

void mos_swab(const void *, void *, long);
int mos_putenv(char *);
int mos_setpgrp(void);
int mos_sethostname(const char *, int);
char *mos_ctermid_r(char *);
const char *mos_hstrerror(int);
int mos_getloadavg(double[], int);
typedef struct { char *p_name; char **p_aliases; int p_proto; } protoent_t;
protoent_t *mos_getprotobyname(const char *);
protoent_t *mos_getprotobynumber(int);
protoent_t *mos_getprotoent(void);
void mos_setprotoent(int);

static int falhas, total;
#define CHECK(c, ...) do { total++; if (!(c)) { falhas++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void) {
    /* swab */
    char out[8];
    mos_swab("ABCDEF", out, 6);
    CHECK(memcmp(out, "BADCFE", 6) == 0, "swab");
    mos_swab("ABCDE", out, 5);                       /* n ímpar: o último byte fica de fora */
    CHECK(out[0] == 'B' && out[1] == 'A' && out[2] == 'D' && out[3] == 'C', "swab ímpar");

    /* putenv */
    char env[] = "MOS_TESTE=abc";
    CHECK(mos_putenv(env) == 0 && getenv("MOS_TESTE") && strcmp(getenv("MOS_TESTE"), "abc") == 0, "putenv define");
    char env2[] = "MOS_TESTE";
    CHECK(mos_putenv(env2) == 0 && getenv("MOS_TESTE") == 0, "putenv sem = remove");

    /* setpgrp / sethostname */
    CHECK(mos_setpgrp() == 0 || mos_setpgrp() == getpgrp(), "setpgrp");
    CHECK(mos_sethostname("x", 1) == 0, "sethostname aceita");

    /* ctermid_r */
    char tbuf[64];
    char *t = mos_ctermid_r(tbuf);
    CHECK(t == tbuf && strstr(tbuf, "tty") != 0, "ctermid_r -> %s", tbuf);
    CHECK(mos_ctermid_r(0) == 0, "ctermid_r(NULL)");

    /* hstrerror */
    CHECK(strcmp(mos_hstrerror(1), "Unknown host") == 0, "hstrerror HOST_NOT_FOUND");
    CHECK(strstr(mos_hstrerror(4), "No address") != 0, "hstrerror NO_DATA");

    /* getloadavg (usa o stub) */
    double la[3] = {-1, -1, -1};
    CHECK(mos_getloadavg(la, 3) == 3 && la[0] == 2.0 && la[1] == 1.0 && la[2] == 0.5, "getloadavg %.2f %.2f %.2f", la[0], la[1], la[2]);
    CHECK(mos_getloadavg(la, 1) == 1 && la[0] == 2.0, "getloadavg nelem=1");

    /* getprotobyname / getprotobynumber */
    protoent_t *pe = mos_getprotobyname("tcp");
    CHECK(pe && pe->p_proto == 6 && strcmp(pe->p_name, "tcp") == 0, "getprotobyname tcp=6");
    pe = mos_getprotobyname("UDP");                  /* pelo alias */
    CHECK(pe && pe->p_proto == 17, "getprotobyname UDP=17");
    pe = mos_getprotobynumber(1);
    CHECK(pe && strcmp(pe->p_name, "icmp") == 0, "getprotobynumber 1=icmp");
    CHECK(mos_getprotobyname("inexistente") == 0, "proto desconhecido = NULL");
    /* getprotoent percorre a tabela */
    mos_setprotoent(1);
    int count = 0;
    while (mos_getprotoent()) if (++count > 100) break;
    CHECK(count >= 8, "getprotoent lista %d protocolos", count);

    printf("libc_apps: %d/%d ok\n", total - falhas, total);
    return falhas != 0;
}
