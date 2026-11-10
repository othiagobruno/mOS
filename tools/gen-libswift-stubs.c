/*
 * Gera libswift*.dylib Mach-O x86_64 de stub. Cada símbolo exportado é um ret
 * (0xC3) e o binário contém a cadeia "STUB mOS libswift". Não é o runtime.
 *
 *   gcc -O2 -o /tmp/gen-libswift-stubs tools/gen-libswift-stubs.c
 *   /tmp/gen-libswift-stubs <dir> tools/libswift-stub-exports.txt [--fixture <arq>]
 *
 * Sem --fixture, só as dylibs. Com --fixture, também um executável que importa
 * cada símbolo da lista (para o machocheck).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_DY 32
#define MAX_SYM 512
#define MAX_NAME 160
#define MAX_NODES 8192
#define MAX_KIDS 16384

static void w32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void uleb4(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v & 0x7f) | 0x80);
    p[1] = (uint8_t)(((v >> 7) & 0x7f) | 0x80);
    p[2] = (uint8_t)(((v >> 14) & 0x7f) | 0x80);
    p[3] = (uint8_t)((v >> 21) & 0x7f);
}

typedef struct { int ch; int child; } kid_t;
static int kid_head[MAX_NODES], kid_n[MAX_NODES], nnodes, nkids, term[MAX_NODES];
static kid_t kids[MAX_KIDS];
static uint32_t node_off[MAX_NODES], node_sz[MAX_NODES];

static void trie_reset(void) {
    nnodes = 1; nkids = 0;
    memset(kid_head, 0xff, sizeof kid_head);
    memset(kid_n, 0, sizeof kid_n);
    memset(term, 0, sizeof term);
}

static int ensure_child(int node, int ch) {
    int i, k;
    for (i = 0; i < kid_n[node]; i++) {
        k = kid_head[node] < 0 ? -1 : kid_head[node] + i;
        if (k >= 0 && kids[k].ch == ch) return kids[k].child;
    }
    if (nnodes >= MAX_NODES || nkids >= MAX_KIDS) return -1;
    if (kid_n[node] == 0) kid_head[node] = nkids;
    else if (kid_head[node] + kid_n[node] != nkids) {
        int base = nkids;
        for (i = 0; i < kid_n[node]; i++) kids[base + i] = kids[kid_head[node] + i];
        kid_head[node] = base;
        nkids += kid_n[node];
    }
    kids[nkids].ch = ch;
    kids[nkids].child = nnodes;
    nkids++;
    kid_n[node]++;
    term[nnodes] = 0;
    kid_head[nnodes] = -1;
    kid_n[nnodes] = 0;
    return nnodes++;
}

static int trie_insert(const char *s) {
    int node = 0;
    for (; *s; s++) {
        node = ensure_child(node, (unsigned char)*s);
        if (node < 0) return -1;
    }
    term[node] = 1;
    return 0;
}

static int cmp_kid(const void *a, const void *b) {
    return ((const kid_t *)a)->ch - ((const kid_t *)b)->ch;
}

static uint32_t size_node(int node) {
    uint32_t n = 1 + (term[node] ? 8 : 0) + 1; /* uleb(tsz) + flags/addr + nchild */
    n += (uint32_t)kid_n[node] * (2 + 4);      /* char, NUL, uleb4 */
    return n;
}

static void assign(int node, uint32_t *cursor) {
    int i;
    node_sz[node] = size_node(node);
    node_off[node] = *cursor;
    *cursor += node_sz[node];
    for (i = 0; i < kid_n[node]; i++) assign(kids[kid_head[node] + i].child, cursor);
}

static void write_node(uint8_t *buf, int node, uint32_t stub_off) {
    uint8_t *p = buf + node_off[node];
    int i, nk = kid_n[node];
    kid_t tmp[1024];
    if (nk > 1024) nk = 1024;
    for (i = 0; i < nk; i++) tmp[i] = kids[kid_head[node] + i];
    qsort(tmp, (size_t)nk, sizeof tmp[0], cmp_kid);
    if (term[node]) {
        *p++ = 8;
        uleb4(p, 0); p += 4;
        uleb4(p, stub_off); p += 4;
    } else *p++ = 0;
    *p++ = (uint8_t)nk;
    for (i = 0; i < nk; i++) {
        *p++ = (uint8_t)tmp[i].ch;
        *p++ = 0;
        uleb4(p, node_off[tmp[i].child]);
        p += 4;
    }
}

static uint32_t build_trie(uint8_t *buf, uint32_t cap, uint32_t stub_off) {
    uint32_t total = 0;
    int i;
    assign(0, &total);
    if (total > cap) return 0;
    memset(buf, 0, total);
    for (i = 0; i < nnodes; i++) write_node(buf, i, stub_off);
    return total;
}

static uint64_t rd_uleb(const uint8_t **p, const uint8_t *end) {
    uint64_t v = 0; int s = 0;
    while (*p < end) {
        uint8_t b = *(*p)++;
        if (s < 64) v |= (uint64_t)(b & 0x7f) << s;
        s += 7;
        if (!(b & 0x80)) break;
    }
    return v;
}

/* Aresta de um caractere, do jeito que build_trie grava. */
static int trie_has_char(const uint8_t *base, uint32_t n, const char *s) {
    const uint8_t *p = base, *end = base + n;
    while (*s) {
        uint64_t tsz; uint8_t nch; int i; int found = 0;
        if (p >= end) return 0;
        tsz = rd_uleb(&p, end);
        p += tsz;
        if (p >= end) return 0;
        nch = *p++;
        for (i = 0; i < nch; i++) {
            const uint8_t *e = p;
            while (e < end && *e) e++;
            if (e >= end) return 0;
            if (e == p + 1 && p[0] == (uint8_t)*s) {
                p = e + 1;
                {
                    uint64_t off = rd_uleb(&p, end);
                    p = base + off;
                }
                s++;
                found = 1;
                break;
            }
            p = e + 1;
            (void)rd_uleb(&p, end);
        }
        if (!found) return 0;
    }
    if (p >= end) return 0;
    return rd_uleb(&p, end) != 0;
}

typedef struct { char name[64]; char sym[MAX_SYM][MAX_NAME]; int nsym; } dy_t;
static dy_t dys[MAX_DY];
static int ndy;

static int load_list(const char *path) {
    FILE *f = fopen(path, "r");
    char line[MAX_NAME];
    int cur = -1;
    if (!f) return -1;
    ndy = 0;
    while (fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (!line[0] || line[0] == '#') continue;
        if (!strncmp(line, "libswift", 8) && strstr(line, ".dylib")) {
            if (ndy >= MAX_DY) { fclose(f); return -1; }
            snprintf(dys[ndy].name, sizeof dys[ndy].name, "%s", line);
            dys[ndy].nsym = 0;
            cur = ndy++;
            continue;
        }
        if (cur < 0 || dys[cur].nsym >= MAX_SYM || line[0] != '_') { fclose(f); return -1; }
        snprintf(dys[cur].sym[dys[cur].nsym], MAX_NAME, "%s", line);
        dys[cur].nsym++;
    }
    fclose(f);
    return ndy ? 0 : -1;
}

static void swift_path(char *dst, size_t cap, const char *name) {
    const char pre[] = "/usr/lib/swift/";
    size_t i = 0, j = 0;
    while (pre[i] && i + 1 < cap) dst[i] = pre[i], i++;
    while (name[j] && i + 1 < cap) dst[i++] = name[j++];
    dst[i] = 0;
}

static int emit_dylib(const char *dir, const dy_t *dy) {
    uint8_t trie[65536];
    uint8_t file[131072];
    char path[1024], idname[128], stubnote[] = "STUB mOS libswift";
    uint32_t stub_off, note_off, trie_off, trie_sz, text_sz, file_sz;
    uint32_t ncmds, sizeofcmds, c;
    const uint32_t id_cmd = 24 + 64; /* offset 24 + nome folgado */
    FILE *fp;
    int i;
    trie_reset();
    if (dy->nsym == 0) {
        if (trie_insert("_mos_swift_stub")) return -1;
    }
    for (i = 0; i < dy->nsym; i++) if (trie_insert(dy->sym[i])) return -1;
    stub_off = 0x200;
    trie_sz = build_trie(trie, sizeof trie, stub_off);
    if (!trie_sz) return -1;
    for (i = 0; i < dy->nsym; i++) if (!trie_has_char(trie, trie_sz, dy->sym[i])) {
        fprintf(stderr, "trie sem %s em %s\n", dy->sym[i], dy->name);
        return -1;
    }
    memset(file, 0, sizeof file);
    note_off = stub_off + 16;
    text_sz = 0x400;
    trie_off = 0x400;
    file_sz = trie_off + trie_sz;
    if (file_sz > sizeof file) return -1;
    file[stub_off] = 0xC3;
    memcpy(file + note_off, stubnote, sizeof stubnote);
    memcpy(file + trie_off, trie, trie_sz);

    ncmds = 3; /* TEXT, LINKEDIT, ID */
    sizeofcmds = (72 + 80 * 2) + 72 + id_cmd;
    w32(file + 0, 0xFEEDFACF);
    w32(file + 4, 0x01000007);
    w32(file + 8, 3);
    w32(file + 12, 6); /* MH_DYLIB */
    w32(file + 16, ncmds);
    w32(file + 20, sizeofcmds);
    w32(file + 24, 0x1 | 0x4); /* NOUNDEFS | DYLDLINK */
    c = 32;
    w32(file + c, 0x19);
    w32(file + c + 4, 72 + 160);
    memcpy(file + c + 8, "__TEXT", 6);
    /* vmaddr 0, vmsize text_sz, fileoff 0, filesize text_sz, prot r-x */
    w32(file + c + 32, text_sz); /* vmsize low */
    w32(file + c + 48, text_sz);
    w32(file + c + 56, 5); /* maxprot */
    w32(file + c + 60, 5);
    w32(file + c + 64, 2); /* nsects */
    /* __text */
    memcpy(file + c + 72, "__text", 6);
    memcpy(file + c + 72 + 16, "__TEXT", 6);
    w32(file + c + 72 + 32, stub_off);
    w32(file + c + 72 + 40, 1);
    w32(file + c + 72 + 48, stub_off);
    w32(file + c + 72 + 64, 0x80000400); /* SOME|PURE instructions */
    /* __cstring */
    memcpy(file + c + 72 + 80, "__cstring", 9);
    memcpy(file + c + 72 + 80 + 16, "__TEXT", 6);
    w32(file + c + 72 + 80 + 32, note_off);
    w32(file + c + 72 + 80 + 40, (uint32_t)sizeof stubnote);
    w32(file + c + 72 + 80 + 48, note_off);
    w32(file + c + 72 + 80 + 64, 2); /* S_CSTRING_LITERALS */
    c += 72 + 160;
    w32(file + c, 0x19);
    w32(file + c + 4, 72);
    memcpy(file + c + 8, "__LINKEDIT", 10);
    w32(file + c + 24, text_sz); /* vmaddr */
    w32(file + c + 32, (trie_sz + 0xFFF) & ~0xFFFu);
    w32(file + c + 40, trie_off);
    w32(file + c + 48, trie_sz);
    w32(file + c + 56, 1);
    w32(file + c + 60, 1);
    c += 72;
    w32(file + c, 0x0D); /* LC_ID_DYLIB */
    w32(file + c + 4, id_cmd);
    w32(file + c + 8, 24);
    w32(file + c + 16, 0x00010000);
    w32(file + c + 20, 0x00010000);
    swift_path(idname, sizeof idname, dy->name);
    memcpy(file + c + 24, idname, strlen(idname) + 1);
    c += id_cmd;
    w32(file + c, 0x80000033);
    w32(file + c + 4, 16);
    w32(file + c + 8, trie_off);
    w32(file + c + 12, trie_sz);
    c += 16;
    ncmds = 4;
    sizeofcmds = c - 32;
    w32(file + 16, ncmds);
    w32(file + 20, sizeofcmds);

    snprintf(path, sizeof path, "%s/%s", dir, dy->name);
    fp = fopen(path, "wb");
    if (!fp) { perror(path); return -1; }
    if (fwrite(file, 1, file_sz, fp) != file_sz) { fclose(fp); return -1; }
    fclose(fp);
    printf("stub %s (%u bytes, %d símbolos)\n", dy->name, file_sz, dy->nsym);
    return 0;
}

static int emit_fixture(const char *path) {
    uint8_t file[65536];
    char names[64 * 1024];
    uint32_t imports[512];
    int nimp = 0, d, s, nord = 0;
    int ord_of[MAX_DY];
    uint32_t sym_base, blob, c, text_sz, blob_sz;
    FILE *fp;
    memset(ord_of, 0, sizeof ord_of);
    memset(names, 0, sizeof names);
    sym_base = 0;
    for (d = 0; d < ndy; d++) if (dys[d].nsym) ord_of[d] = ++nord;
    for (d = 0; d < ndy; d++) for (s = 0; s < dys[d].nsym; s++) {
        size_t L = strlen(dys[d].sym[s]);
        if (nimp >= 512 || sym_base + L + 1 > sizeof names) return -1;
        memcpy(names + sym_base, dys[d].sym[s], L + 1);
        imports[nimp++] = (sym_base << 9) | (uint32_t)ord_of[d];
        sym_base += (uint32_t)L + 1;
    }
    memset(file, 0, sizeof file);
    /* Comandos primeiro. O blob dos imports fica depois deles: se os dois
     * se sobrepõem, o machocheck vê o LC_DYLD_CHAINED_FIXUPS e zero imports. */
    c = 32;
    w32(file + c, 0x19);
    w32(file + c + 4, 72);
    memcpy(file + c + 8, "__TEXT", 6);
    w32(file + c + 56, 5);
    w32(file + c + 60, 5);
    c += 72;
    for (d = 0; d < ndy; d++) if (dys[d].nsym) {
        char idn[128];
        uint32_t cmdsz, nl;
        swift_path(idn, sizeof idn, dys[d].name);
        nl = (uint32_t)strlen(idn) + 1;
        cmdsz = (24 + nl + 7u) & ~7u;
        if (c + cmdsz + 16 > sizeof file) return -1;
        w32(file + c, 0x0C);
        w32(file + c + 4, cmdsz);
        w32(file + c + 8, 24);
        w32(file + c + 16, 0x00010000);
        w32(file + c + 20, 0x00010000);
        memcpy(file + c + 24, idn, nl);
        c += cmdsz;
    }
    blob_sz = 32 + (uint32_t)nimp * 4 + sym_base;
    blob = (c + 16u + 15u) & ~15u;
    text_sz = (blob + blob_sz + 0xFFFu) & ~0xFFFu;
    if (text_sz > sizeof file || blob + blob_sz < blob) return -1;
    w32(file + 32 + 32, text_sz);
    w32(file + 32 + 48, text_sz);
    w32(file + blob + 0, 0);
    w32(file + blob + 4, 32);
    w32(file + blob + 8, 32);
    w32(file + blob + 12, 32 + (uint32_t)nimp * 4);
    w32(file + blob + 16, (uint32_t)nimp);
    w32(file + blob + 20, 1); /* DYLD_CHAINED_IMPORT */
    memcpy(file + blob + 32, imports, (size_t)nimp * 4);
    memcpy(file + blob + 32 + (uint32_t)nimp * 4, names, sym_base);
    {
        uint32_t got = (uint32_t)file[blob + 16] | ((uint32_t)file[blob + 17] << 8) |
                       ((uint32_t)file[blob + 18] << 16) | ((uint32_t)file[blob + 19] << 24);
        if (got != (uint32_t)nimp) return -1;
    }
    w32(file + c, 0x80000034);
    w32(file + c + 4, 16);
    w32(file + c + 8, blob);
    w32(file + c + 12, blob_sz);
    c += 16;
    w32(file + 0, 0xFEEDFACF);
    w32(file + 4, 0x01000007);
    w32(file + 8, 3);
    w32(file + 12, 2); /* MH_EXECUTE */
    w32(file + 16, 2 + (uint32_t)nord);
    w32(file + 20, c - 32);
    fp = fopen(path, "wb");
    if (!fp) { perror(path); return -1; }
    if (fwrite(file, 1, text_sz, fp) != text_sz) { fclose(fp); return -1; }
    fclose(fp);
    printf("fixture %s (%d imports)\n", path, nimp);
    return nimp > 0 ? 0 : -1;
}

int main(int argc, char **argv) {
    int i;
    const char *fixture = NULL;
    if (argc != 3 && argc != 5) {
        fprintf(stderr, "uso: gen-libswift-stubs <dir> <exports.txt> [--fixture <arq>]\n");
        return 2;
    }
    if (argc == 5) {
        if (strcmp(argv[3], "--fixture")) return 2;
        fixture = argv[4];
    }
    if (load_list(argv[2])) { fprintf(stderr, "lista inválida: %s\n", argv[2]); return 1; }
    for (i = 0; i < ndy; i++) if (emit_dylib(argv[1], &dys[i])) return 1;
    if (fixture && emit_fixture(fixture)) return 1;
    return 0;
}
