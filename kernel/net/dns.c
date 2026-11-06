/* mOS — cliente DNS assíncrono (registros A, cache com TTL, seguimento de CNAME) */
#include "netpriv.h"

#define DNS_CACHE     32
#define DNS_NAME_MAX  254
#define DNS_RETRY_MS  1500
#define DNS_TRIES     4
#define DNS_NEG_MS    10000      /* cache negativo */
#define DNS_MAX_CNAME 6

enum { DE_FREE, DE_PENDING, DE_OK, DE_FAIL };

typedef struct {
    int  state;
    char name[DNS_NAME_MAX + 1];    /* nome pedido pelo usuário (chave) */
    char qname[DNS_NAME_MAX + 1];   /* nome consultado agora (após CNAMEs) */
    u32  ip;
    u16  id;
    u8   tries, cnames;
    u64  sent, expires, used;
} dns_entry_t;

static dns_entry_t cache[DNS_CACHE];
static u16 dns_port;

static u32 dns_server(void) {
    if (g_net.dns) return g_net.dns;
    return g_net.gw;
}

static bool name_eq(const char *a, const char *b) { return !strcasecmp(a, b); }

/* codifica "www.exemplo.com" em rótulos */
static int encode_name(u8 *o, const char *name) {
    int n = 0;
    const char *p = name;
    while (*p) {
        const char *dot = strchr(p, '.');
        int l = dot ? (int)(dot - p) : (int)strlen(p);
        if (l == 0) { if (!dot) break; p = dot + 1; continue; }
        if (l > 63 || n + l + 2 > DNS_NAME_MAX + 1) return -1;
        o[n++] = (u8)l;
        memcpy(o + n, p, l);
        n += l;
        if (!dot) break;
        p = dot + 1;
    }
    o[n++] = 0;
    return n;
}

static void send_query(dns_entry_t *e) {
    u32 srv = dns_server();
    e->sent = timer_ticks();
    if (!srv || !g_net.configured) { e->tries = 0; return; }   /* reenviado por dns_tick */
    e->tries++;
    u8 q[300];
    e->id = (u16)rand32();
    wr16(q, e->id);
    wr16(q + 2, 0x0100);          /* RD */
    wr16(q + 4, 1); wr16(q + 6, 0); wr16(q + 8, 0); wr16(q + 10, 0);
    int n = encode_name(q + 12, e->qname);
    if (n < 0) { e->state = DE_FAIL; e->expires = timer_ticks() + DNS_NEG_MS; return; }
    wr16(q + 12 + n, 1);          /* tipo A */
    wr16(q + 14 + n, 1);          /* classe IN */
    udp_send(srv, dns_port, DNS_PORT, q, (u16)(16 + n));
}

/* lê um nome (com compressão) em out; devolve o deslocamento após o nome no registro, ou -1 */
static int read_name(const u8 *m, u16 len, u16 off, char *out, int outmax) {
    int o = 0, jumps = 0;
    int end = -1;
    u16 p = off;
    for (;;) {
        if (p >= len) return -1;
        u8 l = m[p];
        if ((l & 0xC0) == 0xC0) {
            if (p + 1 >= len || ++jumps > 16) return -1;
            if (end < 0) end = p + 2;
            p = (u16)(((l & 0x3F) << 8) | m[p + 1]);
            continue;
        }
        if (l & 0xC0) return -1;
        p++;
        if (l == 0) break;
        if (p + l > len) return -1;
        if (o && o < outmax - 1) out[o++] = '.';
        for (int i = 0; i < l; i++) if (o < outmax - 1) out[o++] = (char)m[p + i];
        p += l;
    }
    out[o] = 0;
    return end >= 0 ? end : p;
}

static void dns_rx(u32 src, u16 sport, const u8 *m, u16 len) {
    if (len < 12 || sport != DNS_PORT) return;
    u16 id = rd16(m), flags = rd16(m + 2);
    if (!(flags & 0x8000)) return;
    dns_entry_t *e = NULL;
    for (int i = 0; i < DNS_CACHE; i++) if (cache[i].state == DE_PENDING && cache[i].id == id) { e = &cache[i]; break; }
    if (!e) return;
    u16 qd = rd16(m + 4), an = rd16(m + 6);
    u64 now = timer_ticks();
    int rcode = flags & 15;
    if (rcode != 0) {
        /* NXDOMAIN (3) ou erro do servidor: falha definitiva para este nome */
        e->state = DE_FAIL;
        e->expires = now + DNS_NEG_MS;
        return;
    }
    char nm[DNS_NAME_MAX + 1], target[DNS_NAME_MAX + 1];
    int off = 12;
    for (int i = 0; i < qd; i++) {
        off = read_name(m, len, (u16)off, nm, sizeof nm);
        if (off < 0 || off + 4 > len) return;
        off += 4;
    }
    /* percorre as respostas seguindo a cadeia de CNAMEs a partir do nome consultado */
    char cur[DNS_NAME_MAX + 1];
    strcpy(cur, e->qname);
    u32 found = 0, ttl_found = 0;
    bool got_cname = false;
    for (int pass = 0; pass < DNS_MAX_CNAME + 1 && !found; pass++) {
        int o = off;
        bool advanced = false;
        for (int i = 0; i < an; i++) {
            o = read_name(m, len, (u16)o, nm, sizeof nm);
            if (o < 0 || o + 10 > len) break;
            u16 type = rd16(m + o), cls = rd16(m + o + 2);
            u32 ttl = rd32(m + o + 4);
            u16 rdl = rd16(m + o + 8);
            o += 10;
            if (o + rdl > len) break;
            if (cls == 1 && name_eq(nm, cur)) {
                if (type == 1 && rdl == 4) { found = rd32(m + o); ttl_found = ttl; break; }
                if (type == 5) {
                    if (read_name(m, len, (u16)o, target, sizeof target) > 0) {
                        strcpy(cur, target);
                        got_cname = true;
                        advanced = true;
                    }
                }
            }
            o += rdl;
        }
        if (!advanced) break;
    }
    if (found) {
        e->ip = found;
        e->state = DE_OK;
        u32 ttl = CLAMP(ttl_found, 30u, 86400u);
        e->expires = now + (u64)ttl * 1000;
        return;
    }
    if (got_cname && e->cnames < DNS_MAX_CNAME) {
        /* só veio o CNAME: consulta o alvo */
        e->cnames++;
        strcpy(e->qname, cur);
        e->tries = 0;
        send_query(e);
        return;
    }
    e->state = DE_FAIL;
    e->expires = now + DNS_NEG_MS;
}

int dns_resolve(const char *host, u32 *ip) {
    if (!host || !*host) return -1;
    if (str_to_ip(host, ip)) return 1;
    if (!strcasecmp(host, "localhost")) { if (ip) *ip = 0x7F000001; return 1; }
    if (!g_netdev || strlen(host) > DNS_NAME_MAX) return -1;
    u64 now = timer_ticks();
    dns_entry_t *slot = NULL;
    for (int i = 0; i < DNS_CACHE; i++) {
        dns_entry_t *e = &cache[i];
        if (e->state == DE_FREE || !name_eq(e->name, host)) continue;
        if ((e->state == DE_OK || e->state == DE_FAIL) && now >= e->expires) { e->state = DE_FREE; break; }
        e->used = now;
        if (e->state == DE_OK) { if (ip) *ip = e->ip; return 1; }
        if (e->state == DE_FAIL) return -1;
        return 0;
    }
    /* nova consulta: escolhe entrada livre ou a menos usada recentemente (que não esteja pendente) */
    for (int i = 0; i < DNS_CACHE; i++) if (cache[i].state == DE_FREE) { slot = &cache[i]; break; }
    if (!slot) {
        for (int i = 0; i < DNS_CACHE; i++)
            if (cache[i].state != DE_PENDING && (!slot || cache[i].used < slot->used)) slot = &cache[i];
    }
    if (!slot) return 0;   /* todas pendentes: tente de novo depois */
    memset(slot, 0, sizeof *slot);
    strcpy(slot->name, host);
    strcpy(slot->qname, host);
    /* remove ponto final */
    usize l = strlen(slot->qname);
    if (l && slot->qname[l - 1] == '.') slot->qname[l - 1] = 0;
    slot->state = DE_PENDING;
    slot->used = now;
    send_query(slot);
    return slot->state == DE_FAIL ? -1 : 0;
}

void dns_tick(u64 now) {
    for (int i = 0; i < DNS_CACHE; i++) {
        dns_entry_t *e = &cache[i];
        if (e->state != DE_PENDING) continue;
        /* aguardando configuração da rede: não gasta tentativas */
        if (!g_net.configured) { e->sent = now; continue; }
        if (e->tries == 0 || now - e->sent >= DNS_RETRY_MS * (u64)e->tries) {
            if (e->tries >= DNS_TRIES) { e->state = DE_FAIL; e->expires = now + DNS_NEG_MS; continue; }
            send_query(e);
        }
    }
}

void dns_init(void) {
    memset(cache, 0, sizeof cache);
    dns_port = net_ephemeral_port();
    udp_bind(dns_port, dns_rx);
}
