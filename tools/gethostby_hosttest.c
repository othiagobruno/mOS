#define _GNU_SOURCE
/*
 * mOS — teste de host de userland/libsystem/libc_resolv_compat.c (prefixo mos_).
 * getaddrinfo/freeaddrinfo falsos no layout do macOS (canonname antes de addr; sockaddr do macOS com sin_len);
 * confere gethostbyname/gethostbyname2 (IPv4 e IPv6), af inválido e falha de resolução.
 * Rodar: tools/gethostby_hosttest.sh
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

int h_errno;   /* definido em resolv.c no mOS; aqui para o teste */

typedef struct mos_ai { int flags, family, socktype, protocol; uint32_t addrlen; char *canonname; void *addr; struct mos_ai *next; } mos_ai;

/* sockaddr do macOS: sin_len, sin_family, sin_port, depois o endereço */
typedef struct { uint8_t len, fam; uint16_t port; uint8_t addr[4]; uint8_t zero[8]; } sin4;
typedef struct { uint8_t len, fam; uint16_t port; uint32_t flow; uint8_t addr[16]; uint32_t scope; } sin6;

static sin4 g_s4; static sin6 g_s6; static mos_ai g_ai1, g_ai2;
static int fail_resolve = 0;

int getaddrinfo(const char *host, const char *svc, const mos_ai *hints, mos_ai **res) {
    (void)svc;
    if (fail_resolve) return 1;
    int af = hints ? hints->family : 2;
    if (af == 2) {
        g_s4.len = sizeof g_s4; g_s4.fam = 2; g_s4.addr[0] = 1; g_s4.addr[1] = 2; g_s4.addr[2] = 3; g_s4.addr[3] = 4;
        g_ai1 = (mos_ai){0}; g_ai1.family = 2; g_ai1.addr = &g_s4; g_ai1.canonname = (char *)"host.example";
        *res = &g_ai1; return 0;
    }
    if (af == 30) {
        g_s6.len = sizeof g_s6; g_s6.fam = 30; for (int i = 0; i < 16; i++) g_s6.addr[i] = (uint8_t)(i + 1);
        g_ai2 = (mos_ai){0}; g_ai2.family = 30; g_ai2.addr = &g_s6; g_ai2.canonname = 0;
        *res = &g_ai2; return 0;
    }
    (void)host; return 1;
}
void freeaddrinfo(mos_ai *ai) { (void)ai; }

struct hostent { char *h_name; char **h_aliases; int h_addrtype; int h_length; char **h_addr_list; };
struct hostent *mos_gethostbyname(const char *);
struct hostent *mos_gethostbyname2(const char *, int);

static int fails = 0, total = 0;
#define CK(cond, msg) do { total++; if (!(cond)) { fails++; printf("FALHA: %s\n", msg); } } while (0)

int main(void) {
    /* IPv4 por gethostbyname */
    struct hostent *h = mos_gethostbyname("host.example");
    CK(h != 0, "gethostbyname != NULL");
    if (h) {
        CK(h->h_addrtype == 2 && h->h_length == 4, "IPv4 addrtype/length");
        CK(strcmp(h->h_name, "host.example") == 0, "h_name = canonname");
        CK(h->h_addr_list[0] && (uint8_t)h->h_addr_list[0][0] == 1 && (uint8_t)h->h_addr_list[0][3] == 4, "endereço 1.2.3.4");
        CK(h->h_addr_list[1] == 0, "lista terminada em NULL");
        CK(h->h_aliases && h->h_aliases[0] == 0, "aliases vazios");
    }
    /* IPv6 por gethostbyname2; canonname NULL -> usa o nome dado */
    struct hostent *h6 = mos_gethostbyname2("v6.example", 30);
    CK(h6 != 0, "gethostbyname2 AF_INET6 != NULL");
    if (h6) {
        CK(h6->h_addrtype == 30 && h6->h_length == 16, "IPv6 addrtype/length");
        CK((uint8_t)h6->h_addr_list[0][0] == 1 && (uint8_t)h6->h_addr_list[0][15] == 16, "endereço IPv6 do offset 8");
        CK(strcmp(h6->h_name, "v6.example") == 0, "h_name = nome dado (canonname NULL)");
    }
    /* af inválido */
    CK(mos_gethostbyname2("x", 99) == 0, "af inválido -> NULL");
    /* falha de resolução */
    fail_resolve = 1;
    CK(mos_gethostbyname("nope") == 0 && h_errno == 1 /* HOST_NOT_FOUND */, "falha -> NULL + h_errno");

    printf("%d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
