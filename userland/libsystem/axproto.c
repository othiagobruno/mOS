/* Codifica e decodifica ax_msg em bytes little-endian. Sem alocação. */
#include "axproto.h"

void *memcpy(void *d, const void *s, unsigned long n);
void *memset(void *d, int c, unsigned long n);

static void put_u16(unsigned char *p, unsigned v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void put_u32(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
static void put_i32(unsigned char *p, int v) { put_u32(p, (unsigned)v); }
static unsigned get_u16(const unsigned char *p) { return (unsigned)p[0] | (unsigned)p[1] << 8; }
static unsigned get_u32(const unsigned char *p) {
    return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}
static int get_i32(const unsigned char *p) { return (int)get_u32(p); }

static int put_name(unsigned char *p, int cap, const char *s) {
    int n = 0;
    if (!s) s = "";
    while (s[n] && n < AX_NAME_MAX - 1) n++;
    if (cap < 2 + n) return -1;
    put_u16(p, (unsigned)n);
    memcpy(p + 2, s, (unsigned long)n);
    return 2 + n;
}

int ax_msg_encode(const ax_msg *m, void *dst, int cap) {
    unsigned char *p = dst;
    int o = 0, k, nk;
    if (!m || !dst || cap < 32) return -1;
    put_u32(p + o, AX_MAGIC); o += 4;
    put_u16(p + o, (unsigned)m->op); o += 2;
    put_u16(p + o, (unsigned)m->pay); o += 2;
    if (o + 24 > cap) return -1;
    put_i32(p + o, m->pid); o += 4;
    put_i32(p + o, m->kind); o += 4;
    put_i32(p + o, m->id); o += 4;
    put_i32(p + o, m->status); o += 4;
    put_i32(p + o, m->loc); o += 4;
    put_i32(p + o, m->len); o += 4;
    k = put_name(p + o, cap - o, m->name);
    if (k < 0) return -1;
    o += k;
    if (m->pay == AX_PAY_STR) {
        int n = 0;
        while (m->str[n] && n < AX_STR_MAX - 1) n++;
        if (o + 2 + n > cap) return -1;
        put_u16(p + o, (unsigned)n); o += 2;
        memcpy(p + o, m->str, (unsigned long)n); o += n;
    } else if (m->pay == AX_PAY_I32) {
        if (o + 4 > cap) return -1;
        put_i32(p + o, m->i); o += 4;
    } else if (m->pay == AX_PAY_BOX) {
        if (o + 16 > cap) return -1;
        put_i32(p + o, m->x); put_i32(p + o + 4, m->y); put_i32(p + o + 8, m->w); put_i32(p + o + 12, m->h);
        o += 16;
    } else if (m->pay == AX_PAY_RANGE) {
        if (o + 8 > cap) return -1;
        put_i32(p + o, m->px); put_i32(p + o + 4, m->py); o += 8;
    } else if (m->pay == AX_PAY_KIDS) {
        nk = m->nkids;
        if (nk < 0) nk = 0;
        if (nk > AX_KIDS_MAX) nk = AX_KIDS_MAX;
        if (o + 2 + nk * 6 > cap) return -1;
        put_u16(p + o, (unsigned)nk); o += 2;
        for (int i = 0; i < nk; i++) {
            put_u16(p + o, (unsigned)m->kid_kind[i]); o += 2;
            put_i32(p + o, m->kid_id[i]); o += 4;
        }
    }
    return o;
}

int ax_msg_decode(const void *src, int n, ax_msg *m) {
    const unsigned char *p = src;
    int o = 0, namelen, slen, nk;
    if (!src || !m || n < 28) return -1;
    memset(m, 0, sizeof *m);
    if (get_u32(p) != AX_MAGIC) return -1;
    o = 4;
    m->op = (int)get_u16(p + o); o += 2;
    m->pay = (int)get_u16(p + o); o += 2;
    m->pid = get_i32(p + o); o += 4;
    m->kind = get_i32(p + o); o += 4;
    m->id = get_i32(p + o); o += 4;
    m->status = get_i32(p + o); o += 4;
    m->loc = get_i32(p + o); o += 4;
    m->len = get_i32(p + o); o += 4;
    if (o + 2 > n) return -1;
    namelen = (int)get_u16(p + o); o += 2;
    if (namelen < 0 || namelen >= AX_NAME_MAX || o + namelen > n) return -1;
    memcpy(m->name, p + o, (unsigned long)namelen);
    m->name[namelen] = 0;
    o += namelen;
    if (m->pay == AX_PAY_NONE) return 0;
    if (m->pay == AX_PAY_STR) {
        if (o + 2 > n) return -1;
        slen = (int)get_u16(p + o); o += 2;
        if (slen < 0 || slen >= AX_STR_MAX || o + slen > n) return -1;
        memcpy(m->str, p + o, (unsigned long)slen);
        m->str[slen] = 0;
        return 0;
    }
    if (m->pay == AX_PAY_I32) {
        if (o + 4 > n) return -1;
        m->i = get_i32(p + o);
        return 0;
    }
    if (m->pay == AX_PAY_BOX) {
        if (o + 16 > n) return -1;
        m->x = get_i32(p + o); m->y = get_i32(p + o + 4); m->w = get_i32(p + o + 8); m->h = get_i32(p + o + 12);
        return 0;
    }
    if (m->pay == AX_PAY_RANGE) {
        if (o + 8 > n) return -1;
        m->px = get_i32(p + o); m->py = get_i32(p + o + 4);
        return 0;
    }
    if (m->pay == AX_PAY_KIDS) {
        if (o + 2 > n) return -1;
        nk = (int)get_u16(p + o); o += 2;
        if (nk < 0 || nk > AX_KIDS_MAX || o + nk * 6 > n) return -1;
        m->nkids = nk;
        for (int i = 0; i < nk; i++) {
            m->kid_kind[i] = (int)get_u16(p + o); o += 2;
            m->kid_id[i] = get_i32(p + o); o += 4;
        }
        return 0;
    }
    return -1;
}
