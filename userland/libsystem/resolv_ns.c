/*
 * mOS libSystem — o leitor de mensagens DNS do libresolv (nameser.h, prefixo res_9_ do macOS) e a API dns.h
 * (dns_open/dns_search): docs/lacunas/leonardo.md, 10 apps (clientes de SRV/TXT/MX do Electron, do Go e do Python).
 *   ns_initparse/ns_parserr/ns_skiprr: percorrem as quatro seções; ns_name_uncompress/ns_name_skip pelo dn_expand
 *   do resolv.c; res_9_init/res_9_query pelas versões sem prefixo; dns_search pelo res_search.
 * Layout de ns_msg e ns_rr igual ao nameser.h do macOS (BIND 8). Teste: tools/resolv_ns_test.sh
 */
#include "sys.h"

void *calloc(size_t n, size_t s);
void free(void *p);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int dn_expand(const u8 *msg, const u8 *eom, const u8 *src, char *dst, int dstsiz);
int res_init(void);
int res_query(const char *name, int cls, int type, u8 *ans, int anslen);
int res_search(const char *name, int cls, int type, u8 *ans, int anslen);

enum { ns_s_qd = 0, ns_s_an = 1, ns_s_ns = 2, ns_s_ar = 3, ns_s_max = 4 };
typedef struct {
    const u8 *_msg, *_eom;
    u16 _id, _flags, _counts[ns_s_max];
    const u8 *_sections[ns_s_max];
    int _sect;
    int _rrnum;
    const u8 *_msg_ptr;
} ns_msg;
#define NS_MAXDNAME 1025
typedef struct {
    char name[NS_MAXDNAME];
    u16 type, rr_class;
    u32 ttl;
    u16 rdlength;
    const u8 *rdata;
} ns_rr;
_Static_assert(sizeof(ns_msg) == 80 && __builtin_offsetof(ns_rr, type) == 1026 && __builtin_offsetof(ns_rr, ttl) == 1032 &&
               __builtin_offsetof(ns_rr, rdata) == 1040, "layout do nameser.h do macOS");

static u16 g16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }
static u32 g32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
u32 res_9_ns_get16(const u8 *p) { return g16(p); }
u32 res_9_ns_get32(const u8 *p) { return g32(p); }
void res_9_ns_put16(u32 v, u8 *p) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
void res_9_ns_put32(u32 v, u8 *p) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }

/* comprimento do nome comprimido em src (sem segui-lo), -1 se passar do fim */
int res_9_ns_name_skip(const u8 **ptr, const u8 *eom) {
    const u8 *p = *ptr;
    while (p < eom) {
        u8 n = *p++;
        if (!n) { *ptr = p; return 0; }
        if ((n & 0xc0) == 0xc0) { if (p >= eom) break; p++; *ptr = p; return 0; }
        if (n & 0xc0) break;                       /* rótulo estendido: não suportado */
        p += n;
    }
    cx_errno = 40;                                 /* EMSGSIZE */
    return -1;
}
int res_9_dn_skipname(const u8 *src, const u8 *eom) {
    const u8 *p = src;
    return res_9_ns_name_skip(&p, eom) < 0 ? -1 : (int)(p - src);
}
static int skiprr(const u8 *p, const u8 *eom, int sect, int count) {
    const u8 *s = p;
    for (int i = 0; i < count; i++) {
        if (res_9_ns_name_skip(&p, eom) < 0) return -1;
        p += sect == ns_s_qd ? 4 : 10;
        if (p > eom) { cx_errno = 40; return -1; }
        if (sect != ns_s_qd) {
            u16 rd = g16(p - 2);
            p += rd;
            if (p > eom) { cx_errno = 40; return -1; }
        }
    }
    return (int)(p - s);
}
int res_9_ns_skiprr(const u8 *p, const u8 *eom, int sect, int count) { return skiprr(p, eom, sect, count); }
int res_9_ns_initparse(const u8 *msg, int len, ns_msg *h) {
    if (!msg || !h || len < 12) { cx_errno = 40; return -1; }
    const u8 *eom = msg + len, *p = msg + 12;
    memset(h, 0, sizeof *h);
    h->_msg = msg;
    h->_eom = eom;
    h->_id = g16(msg);
    h->_flags = g16(msg + 2);
    for (int i = 0; i < ns_s_max; i++) h->_counts[i] = g16(msg + 4 + 2 * i);
    for (int i = 0; i < ns_s_max; i++) {
        if (!h->_counts[i]) { h->_sections[i] = 0; continue; }
        h->_sections[i] = p;
        int b = skiprr(p, eom, i, h->_counts[i]);
        if (b < 0) return -1;
        p += b;
    }
    if (p != eom) { cx_errno = 40; return -1; }
    h->_sect = ns_s_max;
    h->_rrnum = -1;
    h->_msg_ptr = 0;
    return 0;
}
/* registro rrnum da seção: segue do anterior quando é o próximo, senão relê a seção desde o começo */
int res_9_ns_parserr(ns_msg *h, int sect, int rrnum, ns_rr *rr) {
    if (!h || !rr || sect < 0 || sect >= ns_s_max) { cx_errno = 19; return -1; }          /* ENODEV */
    if (sect != h->_sect) { h->_sect = sect; h->_rrnum = 0; h->_msg_ptr = h->_sections[sect]; }
    if (rrnum == -1) rrnum = h->_rrnum;
    if (rrnum < 0 || rrnum >= h->_counts[sect]) { cx_errno = 19; return -1; }
    if (rrnum < h->_rrnum) { h->_rrnum = 0; h->_msg_ptr = h->_sections[sect]; }
    if (rrnum > h->_rrnum) {
        int b = skiprr(h->_msg_ptr, h->_eom, sect, rrnum - h->_rrnum);
        if (b < 0) return -1;
        h->_msg_ptr += b;
        h->_rrnum = rrnum;
    }
    const u8 *p = h->_msg_ptr;
    int k = dn_expand(h->_msg, h->_eom, p, rr->name, NS_MAXDNAME);
    if (k < 0) { cx_errno = 40; return -1; }
    p += k;
    if (p + 4 > h->_eom) { cx_errno = 40; return -1; }
    rr->type = g16(p); rr->rr_class = g16(p + 2);
    p += 4;
    if (sect == ns_s_qd) { rr->ttl = 0; rr->rdlength = 0; rr->rdata = 0; }
    else {
        if (p + 6 > h->_eom) { cx_errno = 40; return -1; }
        rr->ttl = g32(p); rr->rdlength = g16(p + 4);
        p += 6;
        if (p + rr->rdlength > h->_eom) { cx_errno = 40; return -1; }
        rr->rdata = p;
        p += rr->rdlength;
    }
    h->_rrnum++;
    if (h->_rrnum > h->_counts[sect]) { h->_sect = sect + 1; h->_rrnum = 0; }
    h->_msg_ptr = p;
    return 0;
}
int res_9_ns_msg_getflag(ns_msg h, int flag) {
    static const struct { u16 mask, shift; } f[] = {{0x8000, 15}, {0x7800, 11}, {0x0400, 10}, {0x0200, 9}, {0x0100, 8},
                                                    {0x0080, 7}, {0x0040, 6}, {0x0020, 5}, {0x0010, 4}, {0x000f, 0}};
    if (flag < 0 || flag >= (int)(sizeof f / sizeof *f)) return 0;
    return (h._flags & f[flag].mask) >> f[flag].shift;
}
int res_9_ns_name_uncompress(const u8 *msg, const u8 *eom, const u8 *src, char *dst, size_t dstsiz) {
    return dn_expand(msg, eom, src, dst, (int)dstsiz);
}
/* as que não têm prefixo no libSystem.B também ficam (alguns apps ligam libresolv sem o resolv.h da Apple) */
int ns_initparse(const u8 *msg, int len, ns_msg *h) { return res_9_ns_initparse(msg, len, h); }
int ns_parserr(ns_msg *h, int sect, int rrnum, ns_rr *rr) { return res_9_ns_parserr(h, sect, rrnum, rr); }
int ns_skiprr(const u8 *p, const u8 *eom, int sect, int count) { return skiprr(p, eom, sect, count); }
int ns_name_uncompress(const u8 *msg, const u8 *eom, const u8 *src, char *dst, size_t dstsiz) { return dn_expand(msg, eom, src, dst, (int)dstsiz); }
u32 ns_get16(const u8 *p) { return g16(p); }
u32 ns_get32(const u8 *p) { return g32(p); }
int res_9_init(void) { return res_init(); }
int res_9_query(const char *name, int cls, int type, u8 *ans, int anslen) { return res_query(name, cls, type, ans, anslen); }

/* ---- dns.h: um "handle" por chamador, consultas pelo resolvedor do sistema */
typedef struct { int magic; } dns_handle_s;
#define DNS_MAGIC 0x444e5331
void *dns_open(const char *name) {
    (void)name;                                    /* NULL = configuração padrão; um arquivo de resolver não é lido */
    dns_handle_s *h = calloc(1, sizeof *h);
    if (h) h->magic = DNS_MAGIC;
    return h;
}
void dns_free(void *h) { dns_handle_s *x = h; if (x && x->magic == DNS_MAGIC) { x->magic = 0; free(x); } }
int32_t dns_search(void *h, const char *name, u16 cls, u16 type, char *buf, u32 len, void *from, u32 *fromlen) {
    (void)h;
    if (fromlen) *fromlen = 0;
    (void)from;
    int n = res_search(name, cls, type, (u8 *)buf, (int)len);
    return n < 0 ? -1 : n;
}
int32_t dns_query(void *h, const char *name, u16 cls, u16 type, char *buf, u32 len, void *from, u32 *fromlen) {
    (void)h;
    if (fromlen) *fromlen = 0;
    (void)from;
    int n = res_query(name, cls, type, (u8 *)buf, (int)len);
    return n < 0 ? -1 : n;
}
u32 dns_search_list_count(void *h) { (void)h; return 0; }
char *dns_search_list_domain(void *h, u32 i) { (void)h; (void)i; return 0; }   /* o mOS não configura domínio de busca */
void dns_set_buffer_size(void *h, u32 len) { (void)h; (void)len; }
u32 dns_get_buffer_size(void *h) { (void)h; return 8192; }
