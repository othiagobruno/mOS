/*
 * mOS — pacote do Instalador (xar + Bom + Payload cpio).
 * O Payload pode vir em gzip ou em pbzx (xz por chunk, ou chunk cru).
 * Scripts (membro Scripts, preinstall/postinstall) não são lidos para executar.
 * FairPlay: caminho com componente SC_Info recusa o pacote inteiro.
 */
#include "pkgextract.h"

#define PKG_MAX_TOC     (1u << 20)
#define PKG_MAX_MEMBER  (32u << 20)
#define PKG_MAX_FILES   128
#define PKG_MAX_XFILES  64
#define PKG_NAME        200

typedef struct {
    int parent;
    int is_dir;
    int enc;                 /* 0 cru, 1 gzip no xar */
    int has_data;
    u64 offset, length, size;
    char name[PKG_NAME];
} xfile_t;

typedef struct {
    char rel[256];
    const u8 *data;
    u64 n;
    int kind;                /* 0 arquivo, 1 diretório */
} out_file_t;


static void set_reason(pkg_out_t *o, const char *s) {
    if (!o->reason[0]) snprintf(o->reason, sizeof o->reason, "%s", s);
}

static const u8 *find_bytes(const u8 *p, const u8 *end, const char *s) {
    usize n = strlen(s);
    if (!n) return p;
    for (; p + n <= end; p++) if (!memcmp(p, s, n)) return p;
    return 0;
}

/* 0 ok, 1 entrada vazia (só "."), -1 proibido */
static int norm_rel(const char *in, char *out, int cap) {
    const char *p = in;
    char tmp[256];
    int o = 0;
    if (!p || !p[0]) return -1;
    if (p[0] == '/') return -1;
    while (*p) {
        int n;
        const char *s;
        while (*p == '/') p++;
        if (!*p) break;
        s = p;
        while (*p && *p != '/') p++;
        n = (int)(p - s);
        if (n == 1 && s[0] == '.') continue;
        if (n == 2 && s[0] == '.' && s[1] == '.') return -1;
        if (n <= 0 || n >= 200) return -1;
        if (o && o + 1 < (int)sizeof tmp) tmp[o++] = '/';
        if (o + n >= (int)sizeof tmp) return -1;
        memcpy(tmp + o, s, (usize)n);
        o += n;
    }
    tmp[o] = 0;
    if (!o) return 1;
    if (o + 1 > cap) return -1;
    memcpy(out, tmp, (usize)o + 1);
    return 0;
}

static int component_is(const char *rel, const char *name) {
    const char *p = rel;
    usize n = strlen(name);
    while (*p) {
        const char *s = p;
        while (*p && *p != '/') p++;
        if ((usize)(p - s) == n && !memcmp(s, name, n)) return 1;
        if (*p == '/') p++;
    }
    return 0;
}

/* ---------------------------------------------------------------- gzip / pbzx / cpio */

static int gzip_deflate(const u8 *p, u64 n, const u8 **def, u64 *dn) {
    u64 i;
    u8 flags;
    if (n < 18 || p[0] != 0x1f || p[1] != 0x8b || p[2] != 8) return -1;
    flags = p[3];
    if (flags & 0xe0) return -1;
    i = 10;
    if (flags & 4) {
        u16 xlen;
        if (i + 2 > n) return -1;
        xlen = (u16)(p[i] | (p[i + 1] << 8));
        i += 2u + xlen;
    }
    if (flags & 8) { while (i < n && p[i]) i++; i++; }
    if (flags & 16) { while (i < n && p[i]) i++; i++; }
    if (flags & 2) i += 2;
    if (i > n || n - i < 8) return -1;
    *def = p + i;
    *dn = n - i;
    return 0;
}

static int gunzip_alloc(const u8 *p, u64 n, u64 hint, u8 **out, u64 *outn) {
    const u8 *def;
    u64 dn, want;
    u8 *b;
    i64 g;
    if (gzip_deflate(p, n, &def, &dn)) return -1;
    want = hint;
    if (n >= 4) {
        u32 isize = le32(p + n - 4);
        if (isize && isize <= PKG_MAX_MEMBER) want = isize;
    }
    if (!want || want > PKG_MAX_MEMBER) return -1;
    b = kmalloc((usize)want + 1);
    if (!b) return -1;
    g = zlib_decompress(def, dn, b, want);
    if (g < 0) { kfree(b); return -1; }
    b[g] = 0;
    *out = b;
    *outn = (u64)g;
    return 0;
}

static int pbzx_alloc(const u8 *p, u64 n, u8 **out, u64 *outn) {
    u64 flags, off, total, o;
    u8 *b;
    if (n < 12 || memcmp(p, "pbzx", 4)) return -1;
    flags = be64(p + 4);
    if (!flags || flags > PKG_MAX_MEMBER) return -1;
    total = 0;
    off = 12;
    while (off + 16 <= n) {
        u64 raw = be64(p + off);
        u64 csize = be64(p + off + 8);
        if (!raw || raw > PKG_MAX_MEMBER || csize > n - (off + 16)) return -1;
        if (total + raw > PKG_MAX_MEMBER) return -1;
        total += raw;
        off += 16 + csize;
    }
    if (off != n || !total) return -1;
    b = kmalloc((usize)total + 1);
    if (!b) return -1;
    off = 12;
    o = 0;
    while (off + 16 <= n) {
        u64 raw = be64(p + off);
        u64 csize = be64(p + off + 8);
        const u8 *c;
        off += 16;
        c = p + off;
        off += csize;
        if (csize >= 6 && c[0] == 0xfd && !memcmp(c, "\xfd" "7zXZ", 6)) {
            i64 g = xz_decompress(c, csize, b + o, raw);
            if (g < 0 || (u64)g != raw) { kfree(b); return -1; }
        } else if (csize == raw) {
            memcpy(b + o, c, (usize)raw);
        } else {
            kfree(b);
            return -1;
        }
        o += raw;
    }
    b[o] = 0;
    *out = b;
    *outn = o;
    return 0;
}

static int parse_oct(const char *s, int n, u64 *o) {
    u64 v = 0;
    int i, any = 0;
    for (i = 0; i < n; i++) {
        if (s[i] == ' ' || s[i] == 0) continue;
        if (s[i] < '0' || s[i] > '7') return -1;
        v = (v << 3) | (u64)(s[i] - '0');
        any = 1;
    }
    if (!any) return -1;
    *o = v;
    return 0;
}

static int parse_hex(const char *s, int n, u64 *o) {
    u64 v = 0;
    int i;
    for (i = 0; i < n; i++) {
        char c = s[i];
        u64 d;
        if (c >= '0' && c <= '9') d = (u64)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (u64)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (u64)(c - 'A' + 10);
        else return -1;
        v = (v << 4) | d;
    }
    *o = v;
    return 0;
}

static int bom_has(char paths[][256], int np, const char *rel) {
    int i;
    for (i = 0; i < np; i++) if (!strcmp(paths[i], rel)) return 1;
    return 0;
}

/* Bom mínimo: "BOMStore", vars com "Paths", árvore com folha.
 * pathinfo1 = id u32 + length u32 + nome (length bytes, com NUL). */
static int bom_collect(const u8 *b, u64 n, int block, char paths[][256], int *np, int max, int depth) {
    u32 nblocks, index_off, child, count, i;
    u64 node;
    u16 is_leaf, nent;
    if (depth > 8 || block < 0) return -1;
    if (n < 32 || memcmp(b, "BOMStore", 8)) return -1;
    nblocks = be32(b + 12);
    index_off = be32(b + 16);
    if (!nblocks || nblocks > 4096) return -1;
    if ((u64)index_off + (u64)nblocks * 4 > n) return -1;
    if ((u32)block >= nblocks) return -1;
    node = be32(b + index_off + (u32)block * 4);
    if (!node || node + 8 > n) return -1;
    is_leaf = be16(b + node);
    nent = be16(b + node + 2);
    if (nent > 4096) return -1;
    if (is_leaf) {
        u64 ent = node + 12;
        if (ent + (u64)nent * 8 > n) return -1;
        for (i = 0; i < nent; i++) {
            u32 info = be32(b + ent + (u64)i * 8);
            u64 rec, len;
            char rel[256];
            int nr;
            if (info >= nblocks) return -1;
            rec = be32(b + index_off + info * 4);
            if (!rec || rec + 8 > n) return -1;
            len = be32(b + rec + 4);
            if (!len || len > 240 || rec + 8 + len > n) return -1;
            if (b[rec + 8 + len - 1] != 0) return -1;
            nr = norm_rel((const char *)b + rec + 8, rel, (int)sizeof rel);
            if (nr < 0) return -1;
            if (nr == 1) continue;
            if (*np >= max) return -1;
            memcpy(paths[*np], rel, strlen(rel) + 1);
            (*np)++;
        }
        return 0;
    }
    /* ramo: primeiro filho, depois (chave, filho) * count */
    count = nent;
    if (node + 12 + ((u64)count * 2 + 1) * 4 > n) return -1;
    child = be32(b + node + 12);
    if (bom_collect(b, n, (int)child, paths, np, max, depth + 1)) return -1;
    for (i = 0; i < count; i++) {
        child = be32(b + node + 12 + ((u64)i * 2 + 2) * 4);
        if (bom_collect(b, n, (int)child, paths, np, max, depth + 1)) return -1;
    }
    return 0;
}

static int bom_paths(const u8 *b, u64 n, char paths[][256], int max, int *np) {
    u32 nblocks, index_off, vars_off, vcount, i;
    u64 p;
    int tree = -1;
    *np = 0;
    if (n < 32 || memcmp(b, "BOMStore", 8) || be32(b + 8) != 1) return -1;
    nblocks = be32(b + 12);
    index_off = be32(b + 16);
    vars_off = be32(b + 24);
    if (!nblocks || (u64)index_off + (u64)nblocks * 4 > n || vars_off + 4 > n) return -1;
    vcount = be32(b + vars_off);
    if (vcount > 64) return -1;
    p = vars_off + 4;
    for (i = 0; i < vcount; i++) {
        u32 idx;
        u8 ln;
        if (p + 5 > n) return -1;
        idx = be32(b + p);
        ln = b[p + 4];
        p += 5;
        if (p + ln > n) return -1;
        if (ln == 5 && !memcmp(b + p, "Paths", 5)) tree = (int)idx;
        p += ln;
    }
    if (tree < 0) return -1;
    /* o bloco da árvore começa com 'tree'; o filho está no u32 seguinte ao version */
    if ((u32)tree >= nblocks) return -1;
    {
        u64 off = be32(b + index_off + (u32)tree * 4);
        u32 kid;
        if (!off || off + 21 > n || memcmp(b + off, "tree", 4)) return -1;
        kid = be32(b + off + 8);
        return bom_collect(b, n, (int)kid, paths, np, max, 0);
    }
}

static int cpio_collect(const u8 *p, u64 n, char bom[][256], int nb, out_file_t *files, int *nf, int maxf) {
    u64 i = 0;
    while (i + 6 <= n) {
        int odc = !memcmp(p + i, "070707", 6);
        int neu = !memcmp(p + i, "070701", 6) || !memcmp(p + i, "070702", 6);
        u64 namesz = 0, filesz = 0, mode = 0, name_at, data_at, next;
        char name[256], rel[256];
        int nr, kind, pad;
        if (!odc && !neu) return -1;
        if (odc) {
            if (i + 76 > n) return -1;
            if (parse_oct((const char *)p + i + 18, 6, &mode)) return -1;
            if (parse_oct((const char *)p + i + 59, 6, &namesz)) return -1;
            if (parse_oct((const char *)p + i + 65, 11, &filesz)) return -1;
            name_at = i + 76;
            if (namesz == 0 || namesz > 240 || name_at + namesz > n) return -1;
            data_at = name_at + namesz;
            next = data_at + filesz;
            if (next > n) return -1;
        } else {
            if (i + 110 > n) return -1;
            if (parse_hex((const char *)p + i + 14, 8, &mode)) return -1;
            if (parse_hex((const char *)p + i + 54, 8, &filesz)) return -1;
            if (parse_hex((const char *)p + i + 94, 8, &namesz)) return -1;
            name_at = i + 110;
            if (namesz == 0 || namesz > 240 || name_at + namesz > n) return -1;
            pad = (int)((4 - ((110 + namesz) & 3)) & 3);
            data_at = name_at + namesz + (u64)pad;
            if (data_at + filesz > n) return -1;
            next = data_at + filesz;
            next = (next + 3) & ~(u64)3;
            if (next > n) return -1;
        }
        if (p[name_at + namesz - 1] != 0) return -1;
        memcpy(name, p + name_at, (usize)namesz);
        if (!strcmp(name, "TRAILER!!!")) return 0;
        nr = norm_rel(name, rel, (int)sizeof rel);
        if (nr < 0) return -2;
        if (nr == 1) { i = next; continue; }
        if (component_is(rel, "SC_Info")) return -3;
        kind = ((mode & 0170000) == 0040000) ? 1 : ((mode & 0170000) == 0100000) ? 0 : -1;
        if ((mode & 0170000) == 0120000) {
            /* link: não grava. Alvo absoluto ou com ".." recusa o pacote. */
            char tgt[256];
            if (filesz >= sizeof tgt) return -2;
            memcpy(tgt, p + data_at, (usize)filesz);
            tgt[filesz] = 0;
            if (tgt[0] == '/' || norm_rel(tgt, rel, (int)sizeof rel) < 0) return -2;
            i = next;
            continue;
        }
        if (kind < 0) { i = next; continue; }
        if (!bom_has(bom, nb, rel)) return -4;
        if (*nf >= maxf) return -1;
        files[*nf].kind = kind;
        files[*nf].data = kind == 1 ? 0 : p + data_at;
        files[*nf].n = kind == 1 ? 0 : filesz;
        memcpy(files[*nf].rel, rel, strlen(rel) + 1);
        (*nf)++;
        i = next;
    }
    return -1;
}

/* ---------------------------------------------------------------- xar */

static int tag_name(const u8 *p, const u8 *end, const char *name) {
    usize n = strlen(name);
    char c;
    if ((usize)(end - p) < n || memcmp(p, name, n)) return 0;
    c = (char)p[n];
    return c == '>' || c == ' ' || c == '/' || c == '\t' || c == '\n' || c == '\r';
}

static const u8 *skip_tag(const u8 *p, const u8 *end) {
    while (p < end && *p != '>') p++;
    return p < end ? p + 1 : end;
}

static int take_text(const u8 *p, const u8 *end, char *out, int cap, const u8 **next) {
    const u8 *s;
    int n, i;
    p = skip_tag(p, end);
    s = p;
    while (p < end && *p != '<') p++;
    n = (int)(p - s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == '\t')) n--;
    i = 0;
    while (i < n && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) { i++; n--; s++; }
    if (n >= cap) n = cap - 1;
    memcpy(out, s, (usize)n);
    out[n] = 0;
    *next = p;
    return 0;
}

static int parse_u64_text(const char *s, u64 *o) {
    u64 v = 0;
    if (!s || !s[0]) return -1;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return -1;
        if (v > (u64)~0 / 10) return -1;
        v = v * 10 + (u64)(*s - '0');
    }
    *o = v;
    return 0;
}

static int xar_parse(const u8 *toc, u64 tn, xfile_t *files, int max, int *nout) {
    const u8 *p = toc, *end = toc + tn;
    int stack[16], sp = 0, in_data = 0, n = 0;
    *nout = 0;
    while (p < end) {
        if (*p != '<') { p++; continue; }
        if (p + 1 < end && p[1] == '/') {
            const u8 *nme = p + 2;
            if (tag_name(nme, end, "file")) { if (sp) sp--; }
            else if (tag_name(nme, end, "data")) in_data = 0;
            p = skip_tag(p, end);
            continue;
        }
        if (p + 1 < end && (p[1] == '?' || p[1] == '!')) { p = skip_tag(p, end); continue; }
        if (tag_name(p + 1, end, "file")) {
            xfile_t *f;
            if (n >= max || sp >= 16) return -1;
            f = &files[n];
            memset(f, 0, sizeof *f);
            f->parent = sp ? stack[sp - 1] : -1;
            stack[sp++] = n;
            n++;
            p = skip_tag(p, end);
            continue;
        }
        if (tag_name(p + 1, end, "data")) { in_data = 1; p = skip_tag(p, end); continue; }
        if (sp && !in_data && tag_name(p + 1, end, "name")) {
            take_text(p, end, files[stack[sp - 1]].name, PKG_NAME, &p);
            continue;
        }
        if (sp && tag_name(p + 1, end, "type")) {
            char t[32];
            take_text(p, end, t, (int)sizeof t, &p);
            if (!strcmp(t, "directory")) files[stack[sp - 1]].is_dir = 1;
            continue;
        }
        if (sp && in_data && (tag_name(p + 1, end, "offset") || tag_name(p + 1, end, "length") || tag_name(p + 1, end, "size"))) {
            char t[32];
            u64 v = 0;
            const char *which = (const char *)p + 1;
            take_text(p, end, t, (int)sizeof t, &p);
            if (parse_u64_text(t, &v)) return -1;
            if (which[0] == 'o') files[stack[sp - 1]].offset = v;
            else if (which[0] == 'l') files[stack[sp - 1]].length = v;
            else files[stack[sp - 1]].size = v;
            files[stack[sp - 1]].has_data = 1;
            continue;
        }
        if (sp && in_data && tag_name(p + 1, end, "encoding")) {
            const u8 *tagend = skip_tag(p, end);
            const u8 *st = find_bytes(p, tagend, "style=\"");
            if (st && (find_bytes(st, tagend, "gzip") || find_bytes(st, tagend, "x-gzip"))) files[stack[sp - 1]].enc = 1;
            p = tagend;
            continue;
        }
        p = skip_tag(p, end);
    }
    *nout = n;
    return n > 0 ? 0 : -1;
}

static int load_member(const u8 *xar, u64 xn, u64 heap, const xfile_t *f, u8 **out, u64 *outn) {
    const u8 *p;
    u8 *b;
    if (!f->has_data || f->length > PKG_MAX_MEMBER) return -1;
    if (heap > xn || f->offset > xn - heap || f->length > xn - heap - f->offset) return -1;
    p = xar + heap + f->offset;
    if (f->enc == 1 || (f->length >= 2 && p[0] == 0x1f && p[1] == 0x8b)) {
        u64 hint = f->enc == 1 && f->size ? f->size : 0;
        return gunzip_alloc(p, f->length, hint, out, outn);
    }
    b = kmalloc((usize)f->length + 1);
    if (!b) return -1;
    memcpy(b, p, (usize)f->length);
    b[f->length] = 0;
    *out = b;
    *outn = f->length;
    return 0;
}

static const xfile_t *sibling(const xfile_t *files, int n, int parent, const char *name) {
    int i;
    for (i = 0; i < n; i++)
        if (files[i].parent == parent && !files[i].is_dir && !strcmp(files[i].name, name)) return &files[i];
    return 0;
}

static int payload_to_cpio(const u8 *raw, u64 n, u8 **keep, u64 *keepn) {
    u8 *mid = 0;
    u64 mn = n;
    const u8 *p = raw;
    if (n >= 2 && p[0] == 0x1f && p[1] == 0x8b) {
        if (gunzip_alloc(p, n, 0, &mid, &mn)) return -1;
        p = mid;
    }
    if (mn >= 4 && !memcmp(p, "pbzx", 4)) {
        u8 *uz = 0;
        u64 un = 0;
        int r = pbzx_alloc(p, mn, &uz, &un);
        kfree(mid);
        if (r) return -1;
        *keep = uz;
        *keepn = un;
        return 0;
    }
    if (mid) { *keep = mid; *keepn = mn; return 0; }
    return 1; /* o chamador já tem o buffer */
}

static int add_component(const u8 *xar, u64 xn, u64 heap, const xfile_t *files, int nf, int parent,
                         out_file_t *outf, int *nout, u8 **bufs, int *nbuf) {
    const xfile_t *payload = sibling(files, nf, parent, "Payload");
    const xfile_t *bomf = sibling(files, nf, parent, "Bom");
    u8 *pb = 0, *bb = 0, *cpio = 0;
    u64 pn = 0, bn = 0, cn = 0;
    char paths[PKG_MAX_FILES][256];
    int np = 0, own = 0, rc;
    if (!payload || !bomf) return -1;
    if (load_member(xar, xn, heap, bomf, &bb, &bn)) return -1;
    if (bom_paths(bb, bn, paths, PKG_MAX_FILES, &np)) { kfree(bb); return -5; }
    kfree(bb);
    if (load_member(xar, xn, heap, payload, &pb, &pn)) return -1;
    own = payload_to_cpio(pb, pn, &cpio, &cn);
    if (own < 0) { kfree(pb); return -1; }
    if (own == 1) { cpio = pb; cn = pn; pb = 0; }
    else kfree(pb);
    rc = cpio_collect(cpio, cn, paths, np, outf, nout, PKG_MAX_FILES);
    if (rc) { kfree(cpio); return rc; }
    if (*nbuf >= 8) { kfree(cpio); return -1; }
    bufs[(*nbuf)++] = cpio;
    return 0;
}

int pkg_extract(const u8 *data, u64 n, pkg_out_t *out) {
    xfile_t *xf = 0;
    out_file_t *files = 0;
    u8 *toc = 0, *bufs[8];
    u64 heap, tlen, tunc;
    int nx = 0, nf = 0, nbuf = 0, i, parents[8], npick = 0, scripts = 0;
    u32 cksum;
    if (!out) return -1;
    out->files = 0;
    out->scripts_skipped = 0;
    out->reason[0] = 0;
    if (!data || n < 28 || be32(data) != 0x78617221u) { set_reason(out, "não é um xar"); return -1; }
    if (be16(data + 4) < 28 || be16(data + 6) != 1) { set_reason(out, "cabeçalho xar recusado"); return -1; }
    tlen = be64(data + 8);
    tunc = be64(data + 16);
    cksum = be32(data + 24);
    (void)cksum;
    if (!tlen || !tunc || tlen > PKG_MAX_TOC || tunc > PKG_MAX_TOC) { set_reason(out, "toc xar grande ou vazia demais"); return -1; }
    if ((u64)be16(data + 4) + tlen > n) { set_reason(out, "xar truncado"); return -1; }
    heap = (u64)be16(data + 4) + tlen;
    toc = kmalloc((usize)tunc + 1);
    xf = kmalloc(sizeof(xfile_t) * PKG_MAX_XFILES);
    files = kmalloc(sizeof(out_file_t) * PKG_MAX_FILES);
    if (!toc || !xf || !files) { set_reason(out, "sem memória"); goto fail; }
    if (zlib_decompress(data + be16(data + 4), tlen, toc, tunc) < 0) { set_reason(out, "toc zlib inválida"); goto fail; }
    toc[tunc] = 0;
    if (xar_parse(toc, tunc, xf, PKG_MAX_XFILES, &nx)) { set_reason(out, "toc xar malformada"); goto fail; }
    for (i = 0; i < nx; i++) if (!xf[i].is_dir && !strcmp(xf[i].name, "Scripts")) scripts++;
    for (i = 0; i < nx; i++) {
        int parent, j, seen;
        if (xf[i].is_dir || strcmp(xf[i].name, "Payload")) continue;
        parent = xf[i].parent;
        if (!sibling(xf, nx, parent, "Bom")) { set_reason(out, "Payload sem Bom"); goto fail; }
        seen = 0;
        for (j = 0; j < npick; j++) if (parents[j] == parent) seen = 1;
        if (seen) continue;
        if (npick >= 8) { set_reason(out, "muitos componentes"); goto fail; }
        parents[npick++] = parent;
    }
    if (!npick) { set_reason(out, "sem Payload"); goto fail; }
    for (i = 0; i < npick; i++) {
        int rc = add_component(data, n, heap, xf, nx, parents[i], files, &nf, bufs, &nbuf);
        if (rc == -2) { set_reason(out, "caminho fora do destino"); goto fail; }
        if (rc == -3) { set_reason(out, "SC_Info (FairPlay); o mOS não extrai nem contorna"); goto fail; }
        if (rc == -4) { set_reason(out, "arquivo do Payload ausente no Bom"); goto fail; }
        if (rc == -5) { set_reason(out, "Bom malformado"); goto fail; }
        if (rc) { set_reason(out, "Payload malformado"); goto fail; }
    }
    if (!nf) { set_reason(out, "payload vazio"); goto fail; }
    for (i = 0; i < nf; i++) {
        if (files[i].kind == 1 && out->write && out->write(out->ctx, files[i].rel, 0, 0, 1)) {
            set_reason(out, "não gravou o diretório no destino");
            goto wrote;
        }
    }
    for (i = 0; i < nf; i++) {
        if (files[i].kind == 0 && out->write && out->write(out->ctx, files[i].rel, files[i].data, files[i].n, 0)) {
            set_reason(out, "não gravou arquivo no destino");
            goto wrote;
        }
        if (files[i].kind == 0) out->files++;
    }
    out->scripts_skipped = scripts;
    for (i = 0; i < nbuf; i++) kfree(bufs[i]);
    kfree(toc);
    kfree(xf);
    kfree(files);
    return 0;
wrote:
    for (i = 0; i < nbuf; i++) kfree(bufs[i]);
    kfree(toc);
    kfree(xf);
    kfree(files);
    return -1;
fail:
    for (i = 0; i < nbuf; i++) kfree(bufs[i]);
    kfree(toc);
    kfree(xf);
    kfree(files);
    return -1;
}
