/*
 * mOS libSystem — funções de libc que apps grandes do corpus (firefox, gimp, blender, krita, libreoffice, obs...)
 * importam e o libSystem ainda não exportava (docs/lacunas/leonardo.md): byte swap, variáveis de ambiente,
 * grupo de processo, nome de host, terminal, erros de resolução de nome, carga do sistema e a base de protocolos.
 */
#include "sys.h"

size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
char *strcpy(char *d, const char *s);
void *memcpy(void *d, const void *s, size_t n);
int setenv(const char *name, const char *val, int overwrite);
int unsetenv(const char *name);
int setpgid(int pid, int pgid);
int sysctlbyname(const char *name, void *oldp, size_t *oldlen, void *newp, size_t newlen);

/* ================================================================ swab (troca bytes adjacentes) */
void swab(const void *src, void *dst, long n) {
    const u8 *s = src;
    u8 *d = dst;
    for (long i = 0; i + 1 < n; i += 2) { d[i] = s[i + 1]; d[i + 1] = s[i]; }
}

/* ================================================================ ambiente: putenv */
/* putenv("NOME=VALOR") define a variável; "NOME" sozinho a remove. A Apple/BSD guardam o próprio ponteiro, mas
 * setenv (que copia) basta para os apps do corpus e não deixa ponteiro pendurado. */
int putenv(char *s) {
    if (!s) { cx_errno = CX_EINVAL; return -1; }
    const char *eq = s;
    while (*eq && *eq != '=') eq++;
    if (!*eq) return unsetenv(s);
    char name[256];
    size_t n = (size_t)(eq - s);
    if (n >= sizeof name) n = sizeof name - 1;
    memcpy(name, s, n);
    name[n] = 0;
    return setenv(name, eq + 1, 1);
}

/* ================================================================ grupo de processo */
int setpgrp(void) { return setpgid(0, 0); }         /* System V: setpgrp() == setpgid(0,0) */

/* ================================================================ nome de host */
/* o mOS tem um host só; sethostname aceita e não guarda (não há como mudar o nome do kernel por aqui) */
int sethostname(const char *name, int len) { (void)name; (void)len; return 0; }

/* ================================================================ terminal: ctermid_r */
char *ctermid(char *buf);
char *ctermid_r(char *buf) { return buf ? ctermid(buf) : 0; }

/* ================================================================ erros de resolução de nome (h_errno) */
const char *hstrerror(int err) {
    switch (err) {
    case 0: return "Resolver Error 0 (no error)";
    case 1: return "Unknown host";                  /* HOST_NOT_FOUND */
    case 2: return "Host name lookup failure";       /* TRY_AGAIN */
    case 3: return "Unknown server error";           /* NO_RECOVERY */
    case 4: return "No address associated with name";/* NO_DATA */
    }
    return "Unknown resolver error";
}

/* ================================================================ carga do sistema */
/* lê o sysctl vm.loadavg do kernel (struct loadavg {u32 ldavg[3]; long fscale}): carga real, não inventada */
int getloadavg(double avg[], int nelem) {
    struct { unsigned ldavg[3]; long fscale; } la;
    size_t len = sizeof la;
    if (nelem <= 0) return 0;
    if (sysctlbyname("vm.loadavg", &la, &len, 0, 0) != 0 || la.fscale == 0) return -1;
    int n = nelem < 3 ? nelem : 3;
    for (int i = 0; i < n; i++) avg[i] = (double)la.ldavg[i] / (double)la.fscale;
    return n;
}

/* ================================================================ base de protocolos (/etc/protocols)
 * Tabela estática dos protocolos que os apps consultam (não há arquivo /etc/protocols no mOS). getprotobyname e
 * getprotobynumber devolvem uma struct protoent com um alias, como a libc. */
typedef struct { char *p_name; char **p_aliases; int p_proto; } protoent_t;
static const struct { const char *name; const char *alias; int proto; } proto_tab[] = {
    {"ip", "IP", 0}, {"icmp", "ICMP", 1}, {"igmp", "IGMP", 2}, {"tcp", "TCP", 6}, {"udp", "UDP", 17},
    {"ipv6", "IPV6", 41}, {"esp", "ESP", 50}, {"ah", "AH", 51}, {"icmpv6", "ICMPV6", 58},
    {"ipv6-icmp", "IPv6-ICMP", 58}, {"raw", "RAW", 255}, {0, 0, 0},
};
static protoent_t proto_cur;
static char *proto_alias_list[2];
static char proto_name_buf[32], proto_alias_buf[32];
static int proto_idx;
static protoent_t *proto_fill(int i) {
    if (i < 0 || !proto_tab[i].name) return 0;
    strcpy(proto_name_buf, proto_tab[i].name);
    strcpy(proto_alias_buf, proto_tab[i].alias);
    proto_alias_list[0] = proto_alias_buf;
    proto_alias_list[1] = 0;
    proto_cur.p_name = proto_name_buf;
    proto_cur.p_aliases = proto_alias_list;
    proto_cur.p_proto = proto_tab[i].proto;
    return &proto_cur;
}
void *getprotobyname(const char *name) {
    for (int i = 0; proto_tab[i].name; i++) if (!strcmp(name, proto_tab[i].name) || !strcmp(name, proto_tab[i].alias)) return proto_fill(i);
    return 0;
}
void *getprotobynumber(int proto) {
    for (int i = 0; proto_tab[i].name; i++) if (proto_tab[i].proto == proto) return proto_fill(i);
    return 0;
}
void setprotoent(int stayopen) { (void)stayopen; proto_idx = 0; }
void endprotoent(void) { proto_idx = 0; }
void *getprotoent(void) { return proto_tab[proto_idx].name ? proto_fill(proto_idx++) : 0; }
