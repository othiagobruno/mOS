/*
 * mOS — teste de host de userland/libsystem/resolv_ns.c (prefixo mos_). Rodar: tools/resolv_ns_test.sh
 * Uma resposta DNS montada à mão (pergunta, SRV e TXT na resposta, nome comprimido) percorrida com
 * ns_initparse/ns_parserr como os clientes de SRV fazem.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* o resolv_ns.c usa o dn_expand do libSystem; no host vem da libresolv */
#ifdef __APPLE__
int res_9_dn_expand(const unsigned char *, const unsigned char *, const unsigned char *, char *, int);
int dn_expand(const unsigned char *m, const unsigned char *e, const unsigned char *s, char *d, int n) { return res_9_dn_expand(m, e, s, d, n); }
#else
int __dn_expand(const unsigned char *, const unsigned char *, const unsigned char *, char *, int);
int dn_expand(const unsigned char *m, const unsigned char *e, const unsigned char *s, char *d, int n) { return __dn_expand(m, e, s, d, n); }
#endif
int res_init(void) { return 0; }
int res_query(const char *a, int b, int c, unsigned char *d, int e) { (void)a; (void)b; (void)c; (void)d; (void)e; return -1; }
int res_search(const char *a, int b, int c, unsigned char *d, int e) { (void)a; (void)b; (void)c; (void)d; (void)e; return -1; }

typedef struct { const uint8_t *_msg, *_eom; uint16_t _id, _flags, _counts[4]; const uint8_t *_sections[4]; int _sect, _rrnum; const uint8_t *_ptr; } ns_msg;
typedef struct { char name[1025]; uint16_t type, rr_class; uint32_t ttl; uint16_t rdlength; const uint8_t *rdata; } ns_rr;
int mos_res_9_ns_initparse(const uint8_t *, int, ns_msg *);
int mos_res_9_ns_parserr(ns_msg *, int, int, ns_rr *);
int mos_res_9_ns_msg_getflag(ns_msg, int);
unsigned mos_res_9_ns_get16(const uint8_t *);

static int fails, total;
#define CK(c, m) do { total++; if (!(c)) { fails++; printf("FALHA: %s\n", m); } } while (0)

int main(void) {
    uint8_t m[512];
    int n = 0;
    #define B(...) do { uint8_t t_[] = {__VA_ARGS__}; memcpy(m + n, t_, sizeof t_); n += sizeof t_; } while (0)
    B(0x12, 0x34, 0x81, 0x80, 0, 1, 0, 2, 0, 0, 0, 0);                  /* id, QR|RD|RA, 1 pergunta, 2 respostas */
    int qname = n;
    B(5, '_', 'x', 'm', 'p', 'p', 4, '_', 't', 'c', 'p', 7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0, 0, 33, 0, 1);
    B(0xc0, (uint8_t)qname, 0, 33, 0, 1, 0, 0, 0x0e, 0x10, 0, 18);    /* SRV, TTL 3600, rdata 18 */
    B(0, 10, 0, 5, 0x14, 0x66, 3, 'x', 'm', 'p', 0xc0, (uint8_t)(qname + 11));     /* prio 10 peso 5 porta 5222 xmp.example.com */
    /* o alvo comprimido aponta para "example.com" dentro da pergunta; o rdlength conta 2 + 2 + 2 + 4 + 2 = 12 */
    m[n - 12 - 2] = 0; m[n - 12 - 1] = 12;
    B(0xc0, (uint8_t)qname, 0, 16, 0, 1, 0, 0, 0, 60, 0, 6, 5, 'o', 'l', 'a', '!', '!');      /* TXT */
    ns_msg h;
    CK(mos_res_9_ns_initparse(m, n, &h) == 0, "ns_initparse");
    CK(h._counts[0] == 1 && h._counts[1] == 2 && h._id == 0x1234, "contagens e id");
    CK(mos_res_9_ns_msg_getflag(h, 0) == 1 && mos_res_9_ns_msg_getflag(h, 9) == 0, "ns_msg_getflag QR e RCODE");
    ns_rr rr;
    CK(mos_res_9_ns_parserr(&h, 0, 0, &rr) == 0 && !strcmp(rr.name, "_xmpp._tcp.example.com") && rr.type == 33, "pergunta");
    CK(mos_res_9_ns_parserr(&h, 1, 0, &rr) == 0 && rr.type == 33 && rr.ttl == 3600 && rr.rdlength == 12, "SRV");
    CK(mos_res_9_ns_get16(rr.rdata) == 10 && mos_res_9_ns_get16(rr.rdata + 4) == 5222, "SRV prioridade e porta");
    CK(mos_res_9_ns_parserr(&h, 1, 1, &rr) == 0 && rr.type == 16 && rr.rdlength == 6 && rr.rdata[0] == 5 && !memcmp(rr.rdata + 1, "ola!!", 5), "TXT");
    CK(mos_res_9_ns_parserr(&h, 1, 0, &rr) == 0 && rr.type == 33, "volta para o primeiro da seção");
    CK(mos_res_9_ns_parserr(&h, 1, 2, &rr) == -1, "fora da seção");
    CK(mos_res_9_ns_initparse(m, n - 1, &h) == -1, "mensagem truncada");
    printf("resolv-ns: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
