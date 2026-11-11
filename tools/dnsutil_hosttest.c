/* Teste de host de res_9_mkquery, dns_parse_packet e dns_free_reply (userland/libsystem/resolv.c).
 * gcc -DMOS_RESOLV_HOST -O1 tools/dnsutil_hosttest.c userland/libsystem/resolv.c -o /tmp/dnsutil && /tmp/dnsutil */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
typedef struct { uint16_t xid, flags, qdcount, ancount, nscount, arcount; } dns_header_t;
typedef struct { char *name; uint16_t dnstype, dnsclass; } dns_question_t;
typedef struct { uint16_t priority, weight, port; char *target; } dns_SRV_record_t;
typedef struct { uint32_t string_count; char **strings; } dns_TXT_record_t;
typedef struct { char *name; uint16_t dnstype, dnsclass; uint32_t ttl; void *data; } dns_resource_record_t;
typedef struct { uint32_t status; dns_header_t *header; dns_question_t **question;
                 dns_resource_record_t **answer, **authority, **additional; void *server; } dns_reply_t;
dns_reply_t *dns_parse_packet(const char *buf, uint32_t len);
void dns_free_reply(dns_reply_t *r);
int res_9_mkquery(int op, const char *dname, int cls, int type, const unsigned char *data, int datalen, const unsigned char *newrr, unsigned char *buf, int buflen);
static int fails, oks;
#define CHECK(c, m) do { if (c) oks++; else { printf("FALHOU %s\n", m); fails++; } } while (0)
int main(void) {
    unsigned char q[512];
    int n = res_9_mkquery(0, "_xmpp._tcp.example.com", 1, 33, 0, 0, 0, q, sizeof q);
    CHECK(n == 12 + 24 + 4 && q[5] == 1 && q[n - 3] == 33 && q[n - 1] == 1, "res_9_mkquery monta SRV IN");
    CHECK(res_9_mkquery(1, "x.com", 1, 1, 0, 0, 0, q, sizeof q) == -1, "só QUERY");
    /* resposta: a pergunta + SRV + TXT + A, nomes comprimidos para o offset 12 */
    unsigned char r[512];
    int o = 0;
    memcpy(r, q, (size_t)n);
    r[2] = 0x81; r[3] = 0x80; r[7] = 3;                         /* resposta, 3 respostas */
    o = n;
    unsigned char srv[] = {0xC0, 12, 0, 33, 0, 1, 0, 0, 0x0E, 0x10, 0, 12, 0, 10, 0, 5, 0x14, 0x66,
                           3, 'x', 'm', 'p', 0xC0, 12 + 11};       /* alvo "xmp." + "example.com" */
    memcpy(r + o, srv, sizeof srv); o += sizeof srv;
    unsigned char txt[] = {0xC0, 12, 0, 16, 0, 1, 0, 0, 0, 60, 0, 9, 3, 'a', '=', '1', 4, 'b', '=', '2', '2'};
    memcpy(r + o, txt, sizeof txt); o += sizeof txt;
    unsigned char a[] = {0xC0, 12, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4, 10, 0, 2, 15};
    memcpy(r + o, a, sizeof a); o += sizeof a;
    dns_reply_t *rep = dns_parse_packet((const char *)r, (uint32_t)o);
    CHECK(rep && rep->header->ancount == 3 && rep->question[0] && !strcmp(rep->question[0]->name, "_xmpp._tcp.example.com"), "cabeçalho e pergunta");
    if (rep && rep->answer[0] && rep->answer[1] && rep->answer[2]) {
        dns_SRV_record_t *s = rep->answer[0]->data;
        CHECK(rep->answer[0]->dnstype == 33 && s->priority == 10 && s->weight == 5 && s->port == 5222 && !strcmp(s->target, "xmp.example.com"), "SRV");
        dns_TXT_record_t *t = rep->answer[1]->data;
        CHECK(t->string_count == 2 && !strcmp(t->strings[0], "a=1") && !strcmp(t->strings[1], "b=22"), "TXT");
        unsigned char *ip = rep->answer[2]->data;
        CHECK(rep->answer[2]->ttl == 60 && ip[0] == 10 && ip[3] == 15 && !rep->answer[3], "A e fim da lista");
    } else CHECK(0, "três respostas");
    dns_free_reply(rep);
    CHECK(dns_parse_packet((const char *)r, 20) == 0, "pacote truncado devolve NULL");
    printf("dnsutil: %d/%d ok\n", oks, oks + fails);
    return fails != 0;
}
