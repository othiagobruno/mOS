/*
 * mOS libSystem — gethostbyname/gethostbyname2 (legado <netdb.h>) sobre getaddrinfo, para os apps do corpus
 * (firefox, floorp, gimp, libreoffice...). Devolvem um struct hostent estático (não reentrante, como no macOS).
 * struct addrinfo do macOS: ai_canonname vem ANTES de ai_addr (igual ao cf_ai de cfhost.c). AF_INET=2, AF_INET6=30.
 * Teste de host: tools/gethostby_hosttest.sh
 */
#include "sys.h"

void *memcpy(void *d, const void *s, size_t n);
char *strncpy(char *d, const char *s, size_t n);
extern int h_errno;   /* resolv.c */

/* códigos de h_errno (<netdb.h>) */
#define HOST_NOT_FOUND 1
#define NO_RECOVERY    3
#define NO_DATA        4

/* addrinfo do macOS (mesma ordem do cf_ai de cfhost.c) */
typedef struct mos_ai {
    int flags, family, socktype, protocol;
    u32 addrlen;
    char *canonname;
    void *addr;            /* struct sockaddr * */
    struct mos_ai *next;
} mos_ai;
extern int getaddrinfo(const char *host, const char *svc, const mos_ai *hints, mos_ai **res);
extern void freeaddrinfo(mos_ai *ai);

/* struct hostent (<netdb.h>) — layout POSIX padrão */
struct hostent {
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

#define MAXADDR 16
static struct hostent he;
static char he_name[256];
static char he_addr[MAXADDR][16];   /* cabe IPv4 (4) e IPv6 (16) */
static char *he_addr_list[MAXADDR + 1];
static char *he_aliases[1];         /* sem apelidos: lista vazia terminada em NULL */

struct hostent *gethostbyname2(const char *name, int af) {
    if (!name || (af != 2 && af != 30)) { h_errno = NO_RECOVERY; return 0; }
    int len = af == 2 ? 4 : 16;         /* tamanho do endereço */
    int off = af == 2 ? 4 : 8;          /* sin_addr no offset 4; sin6_addr no 8 (sockaddr do macOS) */
    mos_ai hints;
    for (unsigned i = 0; i < sizeof hints; i++) ((char *)&hints)[i] = 0;
    hints.family = af; hints.socktype = 1 /* SOCK_STREAM */;
    mos_ai *res = 0;
    if (getaddrinfo(name, 0, &hints, &res) != 0 || !res) { h_errno = HOST_NOT_FOUND; return 0; }
    int n = 0;
    for (mos_ai *a = res; a && n < MAXADDR; a = a->next) {
        if (a->family != af || !a->addr) continue;
        memcpy(he_addr[n], (char *)a->addr + off, (size_t)len);
        he_addr_list[n] = he_addr[n];
        n++;
    }
    he_addr_list[n] = 0;
    const char *nm = res->canonname ? res->canonname : name;
    strncpy(he_name, nm, sizeof he_name - 1); he_name[sizeof he_name - 1] = 0;
    freeaddrinfo(res);
    if (n == 0) { h_errno = NO_DATA; return 0; }
    he_aliases[0] = 0;
    he.h_name = he_name;
    he.h_aliases = he_aliases;
    he.h_addrtype = af;
    he.h_length = len;
    he.h_addr_list = he_addr_list;
    h_errno = 0;
    return &he;
}
struct hostent *gethostbyname(const char *name) { return gethostbyname2(name, 2 /* AF_INET */); }
