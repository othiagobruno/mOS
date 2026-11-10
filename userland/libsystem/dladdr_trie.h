/* Export trie do Mach-O, a mesma que o dladdr (swiftsupport.c) percorre uma vez por imagem.
 * Sem cabeçalhos de libc: o includer já tem memcpy/strncmp/memcmp. */
#pragma once

typedef void (*dlt_add_fn)(void *ctx, unsigned long long addr, const char *name, int nl);

static inline unsigned long long dlt_uleb(const unsigned char **p, const unsigned char *end) {
    unsigned long long v = 0;
    int s = 0;
    while (*p < end && s <= 56) {
        unsigned char b = *(*p)++;
        v |= (unsigned long long)(b & 0x7f) << s;
        if (!(b & 0x80)) break;
        s += 7;
    }
    return v;
}

/* Ponteiro para os bytes da trie dentro da imagem, ou 0. *slide = hdr - vmaddr do __TEXT. */
static inline const unsigned char *dlt_image(const void *hdr, unsigned *tsz, unsigned long long *slide) {
    const unsigned *mh = (const unsigned *)hdr;
    const unsigned char *p, *hend;
    unsigned ncmds, i, ns = 0;
    unsigned long long text_vm = 0, trie_off = 0, trie_sz = 0;
    unsigned long long seg[8][4];
    if (!mh || mh[0] != 0xFEEDFACFu) return 0;
    p = (const unsigned char *)hdr + 32;
    ncmds = mh[4];
    hend = (const unsigned char *)hdr + 32768;
    for (i = 0; i < ncmds && i < 80; i++) {
        unsigned cmd, sz;
        if (p + 8 > hend) break;
        cmd = *(const unsigned *)p;
        sz = *(const unsigned *)(p + 4);
        if (sz < 8) break;
        if (cmd == 0x19 && ns < 8 && sz >= 72) {
            seg[ns][0] = *(const unsigned long long *)(p + 24);
            seg[ns][1] = *(const unsigned long long *)(p + 32);
            seg[ns][2] = *(const unsigned long long *)(p + 40);
            seg[ns][3] = *(const unsigned long long *)(p + 48);
            if (!strncmp((const char *)p + 8, "__TEXT", 6)) text_vm = seg[ns][0];
            ns++;
        }
        if (cmd == 0x80000033u && sz >= 16) {
            trie_off = *(const unsigned *)(p + 8);
            trie_sz = *(const unsigned *)(p + 12);
        }
        if ((cmd == 0x22 || cmd == 0x80000022u) && !trie_off && sz >= 48) {
            trie_off = *(const unsigned *)(p + 40);
            trie_sz = *(const unsigned *)(p + 44);
        }
        p += sz;
    }
    if (!text_vm || !trie_off || !trie_sz) return 0;
    *slide = (unsigned long long)hdr - text_vm;
    for (i = 0; i < ns; i++) {
        if (trie_off < seg[i][2] || trie_off + trie_sz > seg[i][2] + seg[i][3]) continue;
        *tsz = (unsigned)trie_sz;
        return (const unsigned char *)hdr + (seg[i][0] - text_vm) + (trie_off - seg[i][2]);
    }
    return 0;
}

static inline void dlt_collect(const unsigned char *base, const unsigned char *end, const unsigned char *p,
                               char *name, int nl, int depth, unsigned long long hdr, unsigned long long slide,
                               unsigned long long text_vm, dlt_add_fn add, void *ctx) {
    unsigned long long tsz, flags, raw, addr;
    unsigned char nc;
    unsigned i;
    if (depth > 64 || p < base || p >= end || nl > 180) return;
    tsz = dlt_uleb(&p, end);
    if (tsz && p + tsz <= end && nl > 0) {
        const unsigned char *q = p;
        flags = dlt_uleb(&q, end);
        if (!(flags & 0x18)) {
            raw = dlt_uleb(&q, end);
            addr = (text_vm && raw >= text_vm) ? raw + slide : hdr + raw;
            if (raw && addr >= hdr && add) add(ctx, addr, name, nl);
        }
        p += tsz;
    }
    if (p >= end) return;
    nc = *p++;
    for (i = 0; i < nc && p < end; i++) {
        char child[192];
        int n2 = nl;
        unsigned long long off;
        memcpy(child, name, (unsigned)n2);
        while (p < end && *p && n2 < 180) child[n2++] = (char)*p++;
        while (p < end && *p) p++;
        if (p < end) p++;
        off = dlt_uleb(&p, end);
        if (off && base + off < end)
            dlt_collect(base, end, base + off, child, n2, depth + 1, hdr, slide, text_vm, add, ctx);
    }
}
