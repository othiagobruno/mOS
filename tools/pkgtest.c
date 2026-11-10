/* Teste de host do extrator de .pkg. O pacote é gerado aqui (xar, Bom, cpio, gzip e pbzx).
 * gcc -std=c11 -Wall -Wextra -O1 -DMOS_HOST_TEST -D_GNU_SOURCE -D_msize=malloc_usable_size \
 *   -Ikernel/include -Ikernel/apps tools/pkgtest.c kernel/apps/pkgextract.c kernel/fs/inflate.c kernel/fs/xz.c \
 *   -o /tmp/pkgtest && /tmp/pkgtest
 */
#include "pkgextract.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

long long g_live_allocs, g_live_bytes;

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

typedef struct { u8 *p; u64 n, cap; } buf_t;

static void bgrow(buf_t *b, u64 need) {
    if (b->n + need <= b->cap) return;
    u64 c = b->cap ? b->cap : 256;
    while (b->n + need > c) c *= 2;
    b->p = realloc(b->p, c);
    b->cap = c;
}
static void bput(buf_t *b, const void *s, u64 n) { bgrow(b, n); memcpy(b->p + b->n, s, n); b->n += n; }
static void b8(buf_t *b, u8 v) { bput(b, &v, 1); }
static void b16(buf_t *b, u16 v) { u8 d[2] = { (u8)(v >> 8), (u8)v }; bput(b, d, 2); }
static void b32(buf_t *b, u32 v) { u8 d[4] = { (u8)(v >> 24), (u8)(v >> 16), (u8)(v >> 8), (u8)v }; bput(b, d, 4); }
static void b64(buf_t *b, u64 v) { b32(b, (u32)(v >> 32)); b32(b, (u32)v); }

static u32 crc32(const u8 *p, u64 n) {
    u32 c = 0xffffffffu;
    u64 i;
    for (i = 0; i < n; i++) {
        int k;
        c ^= p[i];
        for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return c ^ 0xffffffffu;
}
static u32 adler32(const u8 *p, u64 n) {
    u32 a = 1, b = 0;
    u64 i;
    for (i = 0; i < n; i++) { a = (a + p[i]) % 65521; b = (b + a) % 65521; }
    return (b << 16) | a;
}

static u64 deflate_store(const u8 *in, u64 n, u8 *out, u64 cap) {
    u64 i = 0, o = 0;
    do {
        u64 chunk = n - i;
        int last;
        u16 nlen;
        if (chunk > 65535) chunk = 65535;
        last = i + chunk >= n;
        if (o + 5 + chunk > cap) return 0;
        out[o++] = (u8)(last ? 1 : 0);
        out[o++] = (u8)(chunk & 255);
        out[o++] = (u8)((chunk >> 8) & 255);
        nlen = (u16)(chunk ^ 0xffffu);
        out[o++] = (u8)(nlen & 255);
        out[o++] = (u8)((nlen >> 8) & 255);
        if (chunk) memcpy(out + o, in + i, chunk);
        o += chunk;
        i += chunk;
        if (last) break;
    } while (i < n);
    return o;
}

static buf_t zlib_wrap(const u8 *in, u64 n) {
    buf_t b = {0};
    u8 tmp[1 << 16];
    u64 dn;
    u32 a;
    u8 ad[4];
    if (n > 60000) { fprintf(stderr, "zlib_wrap: grande demais\n"); return b; }
    dn = deflate_store(in, n, tmp, sizeof tmp);
    b8(&b, 0x78);
    b8(&b, 0x9c);
    bput(&b, tmp, dn);
    a = adler32(in, n);
    ad[0] = (u8)(a >> 24); ad[1] = (u8)(a >> 16); ad[2] = (u8)(a >> 8); ad[3] = (u8)a;
    bput(&b, ad, 4);
    (void)dn;
    return b;
}

static buf_t gzip_wrap(const u8 *in, u64 n) {
    buf_t b = {0};
    u8 tmp[1 << 16];
    u64 dn = deflate_store(in, n, tmp, sizeof tmp);
    u8 hdr[10] = {0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 0xff};
    u32 c = crc32(in, n);
    u8 tail[8];
    bput(&b, hdr, 10);
    bput(&b, tmp, dn);
    tail[0] = (u8)c; tail[1] = (u8)(c >> 8); tail[2] = (u8)(c >> 16); tail[3] = (u8)(c >> 24);
    tail[4] = (u8)n; tail[5] = (u8)(n >> 8); tail[6] = (u8)(n >> 16); tail[7] = (u8)(n >> 24);
    bput(&b, tail, 8);
    return b;
}

static buf_t xz_store(const u8 *in, u64 n) {
    buf_t b = {0};
    u8 flags[2] = {0, 0};
    u32 c;
    u8 z[12];
    u64 start;
    if (n > 65535) return b;
    memcpy(z, "\xfd" "7zXZ", 6);
    z[6] = 0;
    z[7] = 0;
    c = crc32(flags, 2);
    z[8] = (u8)c; z[9] = (u8)(c >> 8); z[10] = (u8)(c >> 16); z[11] = (u8)(c >> 24);
    bput(&b, z, 12);
    start = b.n;
    b8(&b, 2);
    b8(&b, 0);
    b8(&b, 0x21);
    b8(&b, 1);
    b8(&b, 0);
    b8(&b, 0);
    b8(&b, 0);
    b8(&b, 0);
    b32(&b, 0);
    b8(&b, 1);
    b16(&b, (u16)(n - 1));
    bput(&b, in, n);
    b8(&b, 0);
    while ((b.n - start) & 3) b8(&b, 0);
    b8(&b, 0);
    b8(&b, 1);
    b8(&b, 1);
    b8(&b, 1);
    b32(&b, 0);
    b32(&b, 0);
    b32(&b, 0);
    b16(&b, 0);
    b8(&b, 'Y');
    b8(&b, 'Z');
    return b;
}

static buf_t pbzx_wrap(const u8 *in, u64 n) {
    buf_t xz = xz_store(in, n);
    buf_t b = {0};
    bput(&b, "pbzx", 4);
    b64(&b, 0x1000000ull);
    b64(&b, n);
    b64(&b, xz.n);
    bput(&b, xz.p, xz.n);
    free(xz.p);
    return b;
}

static void put_oct(char *d, int width, u64 v) {
    int i;
    for (i = width - 1; i >= 0; i--) { d[i] = (char)('0' + (v & 7)); v >>= 3; }
}

typedef struct { const char *name; const u8 *data; u64 n; int mode; } cent_t;

static buf_t cpio_odc(const cent_t *e, int n) {
    buf_t b = {0};
    int i;
    for (i = 0; i < n; i++) {
        char h[76];
        u64 namesz = strlen(e[i].name) + 1;
        memset(h, '0', 76);
        memcpy(h, "070707", 6);
        put_oct(h + 6, 6, 1);
        put_oct(h + 12, 6, (u64)(i + 1));
        put_oct(h + 18, 6, (u64)e[i].mode);
        put_oct(h + 24, 6, 0);
        put_oct(h + 30, 6, 0);
        put_oct(h + 36, 6, 1);
        put_oct(h + 42, 6, 0);
        put_oct(h + 48, 11, 0);
        put_oct(h + 59, 6, namesz);
        put_oct(h + 65, 11, e[i].n);
        bput(&b, h, 76);
        bput(&b, e[i].name, namesz);
        if (e[i].n) bput(&b, e[i].data, e[i].n);
    }
    {
        char h[76];
        const char *tr = "TRAILER!!!";
        memset(h, '0', 76);
        memcpy(h, "070707", 6);
        put_oct(h + 18, 6, 0100644);
        put_oct(h + 59, 6, 11);
        put_oct(h + 65, 11, 0);
        bput(&b, h, 76);
        bput(&b, tr, 11);
    }
    return b;
}

static buf_t bom_make(const char **paths, const int *kinds, int n) {
    buf_t b = {0};
    int nblocks = 3 + n * 2;
    u32 *off = calloc((usize)nblocks, 4);
    u32 index_off, vars_off, vars_len, i, blk;
    u64 blocks_at;
    for (i = 0; i < 32; i++) b8(&b, 0);
    index_off = (u32)b.n;
    for (i = 0; i < (u32)nblocks; i++) b32(&b, 0);
    vars_off = (u32)b.n;
    b32(&b, 1);
    b32(&b, 1);
    b8(&b, 5);
    bput(&b, "Paths", 5);
    vars_len = (u32)b.n - vars_off;
    while (b.n & 3) b8(&b, 0);
    blocks_at = b.n;
    off[1] = (u32)b.n;
    b32(&b, 0x74726565u);
    b32(&b, 1);
    b32(&b, 2);
    b32(&b, 4096);
    b32(&b, (u32)n);
    b8(&b, 0);
    while (b.n & 3) b8(&b, 0);
    off[2] = (u32)b.n;
    b16(&b, 1);
    b16(&b, (u16)n);
    b32(&b, 0);
    b32(&b, 0);
    for (i = 0; i < (u32)n; i++) {
        b32(&b, 3 + i * 2);
        b32(&b, 4 + i * 2);
    }
    blk = 3;
    for (i = 0; i < (u32)n; i++) {
        u32 ln = (u32)strlen(paths[i]) + 1;
        u8 info[32];
        off[blk++] = (u32)b.n;
        b32(&b, i + 1);
        b32(&b, ln);
        bput(&b, paths[i], ln);
        off[blk++] = (u32)b.n;
        memset(info, 0, sizeof info);
        info[0] = (u8)(kinds[i] == 1 ? 2 : 1);
        info[4] = 1;
        info[5] = (u8)(kinds[i] == 1 ? 0755 : 0644);
        bput(&b, info, 31);
    }
    memcpy(b.p, "BOMStore", 8);
    b.p[8] = 0; b.p[9] = 0; b.p[10] = 0; b.p[11] = 1;
    b.p[12] = (u8)((nblocks >> 24) & 255);
    b.p[13] = (u8)((nblocks >> 16) & 255);
    b.p[14] = (u8)((nblocks >> 8) & 255);
    b.p[15] = (u8)(nblocks & 255);
    {
        u32 fields[4] = {index_off, (u32)nblocks * 4, vars_off, vars_len};
        int f;
        for (f = 0; f < 4; f++) {
            u32 v = fields[f];
            b.p[16 + f * 4] = (u8)(v >> 24);
            b.p[17 + f * 4] = (u8)(v >> 16);
            b.p[18 + f * 4] = (u8)(v >> 8);
            b.p[19 + f * 4] = (u8)v;
        }
    }
    for (i = 0; i < (u32)nblocks; i++) {
        u32 v = off[i];
        u8 *d = b.p + index_off + i * 4;
        d[0] = (u8)(v >> 24); d[1] = (u8)(v >> 16); d[2] = (u8)(v >> 8); d[3] = (u8)v;
    }
    free(off);
    (void)blocks_at;
    return b;
}

typedef struct { const char *name; buf_t body; int dir; int parent; } xmem_t;

static buf_t xar_make(xmem_t *m, int n) {
    buf_t xml = {0};
    buf_t z, out = {0};
    u64 off = 0;
    int i;
    char line[256];
    bput(&xml, "<?xml version=\"1.0\"?>\n<xar><toc>\n", 32);
    for (i = 0; i < n; i++) {
        snprintf(line, sizeof line, "<file id=\"%d\">\n<name>%s</name>\n<type>%s</type>\n", i + 1, m[i].name, m[i].dir ? "directory" : "file");
        bput(&xml, line, strlen(line));
        if (!m[i].dir) {
            snprintf(line, sizeof line,
                     "<data><offset>%llu</offset><length>%llu</length><size>%llu</size><encoding style=\"application/octet-stream\"/></data>\n",
                     (unsigned long long)off, (unsigned long long)m[i].body.n, (unsigned long long)m[i].body.n);
            bput(&xml, line, strlen(line));
            off += m[i].body.n;
        }
        /* fecha os filhos antes do pai: o gerador lista o pai, depois os filhos, e marca parent.
         * Aqui cada item fecha a própria tag; aninhamento real é feito com dir e a ordem. */
        if (!m[i].dir) bput(&xml, "</file>\n", 8);
    }
    /* diretórios fecham depois dos filhos: segunda passagem simples — o parser usa a ordem das tags.
     * O teste de mpkg emite o XML aninhado por xar_make_nested. */
    for (i = n - 1; i >= 0; i--) if (m[i].dir) bput(&xml, "</file>\n", 8);
    bput(&xml, "</toc></xar>\n", 13);
    z = zlib_wrap(xml.p, xml.n);
    b32(&out, 0x78617221u);
    b16(&out, 28);
    b16(&out, 1);
    b64(&out, z.n);
    b64(&out, xml.n);
    b32(&out, 0);
    bput(&out, z.p, z.n);
    for (i = 0; i < n; i++) if (!m[i].dir) bput(&out, m[i].body.p, m[i].body.n);
    free(xml.p);
    free(z.p);
    return out;
}

/* XML com um diretório e os arquivos dentro, para o .mpkg. */
static buf_t xar_nested(const char *dirname, xmem_t *inner, int n, xmem_t *top, int nt) {
    buf_t xml = {0};
    buf_t z, out = {0};
    u64 off = 0;
    int i, id = 1;
    char line[320];
    bput(&xml, "<?xml version=\"1.0\"?>\n<xar><toc>\n", 32);
    for (i = 0; i < nt; i++) {
        snprintf(line, sizeof line, "<file id=\"%d\"><name>%s</name><type>file</type><data><offset>%llu</offset><length>%llu</length><size>%llu</size><encoding style=\"application/octet-stream\"/></data></file>\n",
                 id++, top[i].name, (unsigned long long)off, (unsigned long long)top[i].body.n, (unsigned long long)top[i].body.n);
        bput(&xml, line, strlen(line));
        off += top[i].body.n;
    }
    snprintf(line, sizeof line, "<file id=\"%d\"><name>%s</name><type>directory</type>\n", id++, dirname);
    bput(&xml, line, strlen(line));
    for (i = 0; i < n; i++) {
        snprintf(line, sizeof line, "<file id=\"%d\"><name>%s</name><type>file</type><data><offset>%llu</offset><length>%llu</length><size>%llu</size><encoding style=\"application/octet-stream\"/></data></file>\n",
                 id++, inner[i].name, (unsigned long long)off, (unsigned long long)inner[i].body.n, (unsigned long long)inner[i].body.n);
        bput(&xml, line, strlen(line));
        off += inner[i].body.n;
    }
    bput(&xml, "</file></toc></xar>\n", 20);
    z = zlib_wrap(xml.p, xml.n);
    b32(&out, 0x78617221u);
    b16(&out, 28);
    b16(&out, 1);
    b64(&out, z.n);
    b64(&out, xml.n);
    b32(&out, 0);
    bput(&out, z.p, z.n);
    for (i = 0; i < nt; i++) bput(&out, top[i].body.p, top[i].body.n);
    for (i = 0; i < n; i++) bput(&out, inner[i].body.p, inner[i].body.n);
    free(xml.p);
    free(z.p);
    return out;
}

typedef struct { char root[512]; int writes; } sink_t;

static int host_write(void *ctx, const char *rel, const void *data, u64 n, int kind) {
    sink_t *s = ctx;
    char path[1024], parent[1024];
    char *slash, *rp, *rootp;
    FILE *f;
    char *p;
    if (!rel || strstr(rel, "..") || rel[0] == '/') { s->writes = -1000; return -1; }
    snprintf(path, sizeof path, "%s/%s", s->root, rel);
    snprintf(parent, sizeof parent, "%s", path);
    slash = strrchr(parent, '/');
    if (!slash) return -1;
    *slash = 0;
    for (p = parent + 1; *p; p++) if (*p == '/') {
        char *w = (char *)p;
        *w = 0;
        mkdir(parent, 0755);
        *w = '/';
    }
    mkdir(parent, 0755);
    rootp = realpath(s->root, 0);
    rp = realpath(parent, 0);
    if (!rootp || !rp || strncmp(rp, rootp, strlen(rootp)) || (rp[strlen(rootp)] != 0 && rp[strlen(rootp)] != '/')) {
        free(rootp);
        free(rp);
        s->writes = -1000;
        return -1;
    }
    free(rootp);
    free(rp);
    if (kind == 1) { mkdir(path, 0755); s->writes++; return 0; }
    f = fopen(path, "wb");
    if (!f || (n && fwrite(data, 1, n, f) != n)) { if (f) fclose(f); return -1; }
    fclose(f);
    s->writes++;
    return 0;
}

static int read_all(const char *path, char *buf, int cap) {
    FILE *f = fopen(path, "rb");
    int n;
    if (!f) return -1;
    n = (int)fread(buf, 1, (usize)cap - 1, f);
    fclose(f);
    buf[n] = 0;
    return n;
}

static void prep(char *base, sink_t *s) {
    char cmd[640];
    strcpy(base, "/tmp/mos-pkg-XXXXXX");
    if (!mkdtemp(base)) { perror("mkdtemp"); exit(1); }
    snprintf(s->root, sizeof s->root, "%s/Aplicativos", base);
    mkdir(s->root, 0755);
    snprintf(cmd, sizeof cmd, "%s/fora", base);
    mkdir(cmd, 0755);
    snprintf(cmd, sizeof cmd, "%s/fora/sentinel", base);
    {
        FILE *f = fopen(cmd, "wb");
        fputs("SENTINELA", f);
        fclose(f);
    }
    s->writes = 0;
}

static int sentinel_ok(const char *base) {
    char path[640], buf[32];
    snprintf(path, sizeof path, "%s/fora/sentinel", base);
    if (read_all(path, buf, (int)sizeof buf) < 0) return 0;
    return !strcmp(buf, "SENTINELA");
}

static int fora_only_sentinel(const char *base) {
    char path[640];
    snprintf(path, sizeof path, "%s/fora", base);
    /* um único arquivo: sentinel. Conferido pelo nome. */
    {
        char ent[256];
        int n = 0;
        FILE *p;
        char cmd[700];
        snprintf(cmd, sizeof cmd, "ls -1 '%s'", path);
        p = popen(cmd, "r");
        if (!p) return 0;
        while (fgets(ent, sizeof ent, p)) n++;
        pclose(p);
        return n == 1;
    }
}

static buf_t payload_files(int evil, int fairplay) {
    const char *plist = "<?xml version=\"1.0\"?><plist><string>com.mos.pkgdemo</string></plist>";
    const char *bin = "ola-pkg";
    cent_t e[6];
    int n = 0;
    e[n++] = (cent_t){"./Demo.app", 0, 0, 040755};
    e[n++] = (cent_t){"./Demo.app/Contents", 0, 0, 040755};
    e[n++] = (cent_t){"./Demo.app/Contents/Info.plist", (const u8 *)plist, strlen(plist), 0100644};
    e[n++] = (cent_t){"./Demo.app/Contents/MacOS", 0, 0, 040755};
    e[n++] = (cent_t){"./Demo.app/Contents/MacOS/Demo", (const u8 *)bin, strlen(bin), 0100755};
    if (evil) e[n++] = (cent_t){"./Demo.app/../../fora/escapou", (const u8 *)"NAO", 3, 0100644};
    if (fairplay) e[n++] = (cent_t){"./Demo.app/Contents/SC_Info/sinf", (const u8 *)"fp", 2, 0100644};
    return cpio_odc(e, n);
}

static buf_t bom_for(int evil, int fairplay) {
    const char *paths[8];
    int kinds[8];
    int n = 0;
    paths[n] = "Demo.app"; kinds[n++] = 1;
    paths[n] = "Demo.app/Contents"; kinds[n++] = 1;
    paths[n] = "Demo.app/Contents/Info.plist"; kinds[n++] = 0;
    paths[n] = "Demo.app/Contents/MacOS"; kinds[n++] = 1;
    paths[n] = "Demo.app/Contents/MacOS/Demo"; kinds[n++] = 0;
    if (evil) { paths[n] = "Demo.app/../../fora/escapou"; kinds[n++] = 0; }
    if (fairplay) { paths[n] = "Demo.app/Contents/SC_Info/sinf"; kinds[n++] = 0; }
    return bom_make(paths, kinds, n);
}

static buf_t scripts_cpio(void) {
    const char *body = "#!/bin/sh\ntouch NAO_EXECUTAR\n";
    cent_t e = {"preinstall", (const u8 *)body, strlen(body), 0100755};
    return cpio_odc(&e, 1);
}

static buf_t make_flat(const u8 *payload, u64 pn, int with_scripts) {
    xmem_t m[4];
    int n = 0;
    buf_t bom = bom_for(0, 0);
    buf_t scripts = {0};
    const char *info = "<pkg-info install-location=\"/Applications\"></pkg-info>";
    m[n].name = "Bom"; m[n].dir = 0; m[n].body = bom; n++;
    m[n].name = "Payload"; m[n].dir = 0; m[n].body.p = (u8 *)payload; m[n].body.n = pn; m[n].body.cap = pn; n++;
    m[n].name = "PackageInfo"; m[n].dir = 0; m[n].body.p = (u8 *)info; m[n].body.n = strlen(info); m[n].body.cap = m[n].body.n; n++;
    if (with_scripts) {
        scripts = scripts_cpio();
        m[n].name = "Scripts"; m[n].dir = 0; m[n].body = scripts; n++;
    }
    {
        buf_t x = xar_make(m, n);
        free(bom.p);
        free(scripts.p);
        return x;
    }
}

static void expect_ok(const u8 *pkg, u64 n, int scripts, const char *tag) {
    char base[64];
    sink_t s;
    pkg_out_t o;
    char got[64];
    char path[700];
    int r;
    prep(base, &s);
    memset(&o, 0, sizeof o);
    o.write = host_write;
    o.ctx = &s;
    r = pkg_extract(pkg, n, &o);
    printf("pkg %s: r=%d arquivos=%d scripts_ignorados=%d writes=%d motivo=%s\n", tag, r, o.files, o.scripts_skipped, s.writes, o.reason);
    CHECK(r == 0);
    CHECK(o.scripts_skipped == scripts);
    CHECK(s.writes > 0);
    snprintf(path, sizeof path, "%s/Demo.app/Contents/MacOS/Demo", s.root);
    CHECK(read_all(path, got, (int)sizeof got) > 0);
    CHECK(strcmp(got, "ola-pkg") == 0);
    snprintf(path, sizeof path, "%s/Demo.app/Contents/Info.plist", s.root);
    CHECK(read_all(path, got, (int)sizeof got) > 0);
    CHECK(strstr(got, "com.mos.pkgdemo") != 0);
    CHECK(sentinel_ok(base));
    CHECK(fora_only_sentinel(base));
    {
        char cmd[700];
        snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
        if (system(cmd) != 0) fprintf(stderr, "aviso: não apaguei %s\n", base);
    }
}

static void expect_refuse(const u8 *pkg, u64 n, const char *piece, const char *tag) {
    char base[64];
    sink_t s;
    pkg_out_t o;
    int r;
    prep(base, &s);
    memset(&o, 0, sizeof o);
    o.write = host_write;
    o.ctx = &s;
    r = pkg_extract(pkg, n, &o);
    printf("pkg %s: r=%d writes=%d motivo=%s\n", tag, r, s.writes, o.reason);
    CHECK(r != 0);
    CHECK(s.writes == 0);
    CHECK(!piece || (o.reason[0] && strstr(o.reason, piece)));
    CHECK(sentinel_ok(base));
    CHECK(fora_only_sentinel(base));
    {
        char cmd[700];
        char demo[700];
        snprintf(demo, sizeof demo, "%s/Demo.app", s.root);
        CHECK(access(demo, 0) != 0);
        snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
        if (system(cmd) != 0) fprintf(stderr, "aviso: não apaguei %s\n", base);
    }
}

int main(void) {
    buf_t cpio, gz, pb, pkg, xzround;
    const u8 sample[] = "ola-pkg";
    u8 back[16];
    i64 g;
    cpio = payload_files(0, 0);
    xzround = xz_store(sample, sizeof sample - 1);
    g = xz_decompress(xzround.p, xzround.n, back, sizeof back);
    printf("xz cru: %lld bytes\n", (long long)g);
    CHECK(g == (i64)(sizeof sample - 1) && !memcmp(back, sample, sizeof sample - 1));
    gz = gzip_wrap(cpio.p, cpio.n);
    pb = pbzx_wrap(cpio.p, cpio.n);
    pkg = make_flat(gz.p, gz.n, 1);
    expect_ok(pkg.p, pkg.n, 1, "gzip+scripts");
    free(pkg.p);
    pkg = make_flat(pb.p, pb.n, 0);
    expect_ok(pkg.p, pkg.n, 0, "pbzx");
    free(pkg.p);
    {
        xmem_t inner[3], top[1];
        buf_t bom = bom_for(0, 0);
        buf_t nest;
        const char *dist = "<installer-script minSpecVersion=\"1\"></installer-script>";
        const char *info = "<pkg-info install-location=\"/Applications\"></pkg-info>";
        inner[0].name = "Bom"; inner[0].body = bom;
        inner[1].name = "Payload"; inner[1].body = gz;
        inner[2].name = "PackageInfo"; inner[2].body.p = (u8 *)info; inner[2].body.n = strlen(info);
        top[0].name = "Distribution"; top[0].body.p = (u8 *)dist; top[0].body.n = strlen(dist);
        nest = xar_nested("Demo.pkg", inner, 3, top, 1);
        expect_ok(nest.p, nest.n, 0, "mpkg");
        free(nest.p);
        free(bom.p);
    }
    {
        buf_t evil_cpio = payload_files(1, 0);
        buf_t evil_gz = gzip_wrap(evil_cpio.p, evil_cpio.n);
        xmem_t m[2];
        buf_t bom = bom_for(0, 0);
        buf_t x;
        m[0].name = "Bom"; m[0].dir = 0; m[0].body = bom;
        m[1].name = "Payload"; m[1].dir = 0; m[1].body = evil_gz;
        x = xar_make(m, 2);
        expect_refuse(x.p, x.n, "fora do destino", "traversal");
        free(x.p);
        free(bom.p);
        free(evil_gz.p);
        free(evil_cpio.p);
    }
    {
        buf_t fp = payload_files(0, 1);
        buf_t fpg = gzip_wrap(fp.p, fp.n);
        xmem_t m[2];
        buf_t bom = bom_for(0, 1);
        buf_t x;
        m[0].name = "Bom"; m[0].dir = 0; m[0].body = bom;
        m[1].name = "Payload"; m[1].dir = 0; m[1].body = fpg;
        x = xar_make(m, 2);
        expect_refuse(x.p, x.n, "FairPlay", "sc_info");
        free(x.p);
        free(bom.p);
        free(fpg.p);
        free(fp.p);
    }
    {
        u8 bad[] = "nao-e-xar";
        expect_refuse(bad, sizeof bad - 1, "xar", "magic");
    }
    {
        xmem_t m[2];
        buf_t bom = bom_for(0, 0);
        buf_t x;
        memcpy(bom.p, "BOMXXXXX", 8);
        m[0].name = "Bom"; m[0].dir = 0; m[0].body = bom;
        m[1].name = "Payload"; m[1].dir = 0; m[1].body = gz;
        x = xar_make(m, 2);
        expect_refuse(x.p, x.n, "Bom", "bom-ruim");
        free(x.p);
        free(bom.p);
    }
    {
        u8 trunc[28];
        memset(trunc, 0, sizeof trunc);
        trunc[0] = 'x'; trunc[1] = 'a'; trunc[2] = 'r'; trunc[3] = '!';
        trunc[5] = 28;
        trunc[7] = 1;
        trunc[15] = 100; /* toc length */
        trunc[23] = 100;
        expect_refuse(trunc, sizeof trunc, "truncado", "truncado");
    }
    free(cpio.p);
    free(gz.p);
    free(pb.p);
    free(xzround.p);
    if (fails) { fprintf(stderr, "pkgtest: %d falhas em %d\n", fails, checks); return 1; }
    printf("pkgtest: %d/%d PASS\n", checks, checks);
    printf("alocações vivas: %lld\n", g_live_allocs);
    return g_live_allocs ? 1 : 0;
}
