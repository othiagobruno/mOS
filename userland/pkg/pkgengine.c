/*
 * mOS — motor de pacotes do Instalador (.pkg): xar, Distribution, PackageInfo, Bom, Payload, Scripts e recibos.
 * Incluído por userland/cli-macos/installer.c e pkgutil.c (Mach-O do mOS) e pelo teste de host (tools/installer_test.sh).
 * Escrito do zero a partir dos formatos públicos: xar (cabeçalho "xar!", TOC XML em zlib, heap com checksums),
 * cpio odc/newc, pbzx (blocos xz), Bom ("BOMStore"), e o layout de recibos do macOS em /var/db/receipts.
 * Os descompressores são os do kernel (kernel/fs/inflate.c, xz.c, bzip2.c), compilados aqui com MOS_HOST_TEST.
 */
#ifndef PKGENGINE_C
#define PKGENGINE_C
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "fs.h"   /* userland/pkg/fs.h (vem antes de kernel/include no -I) */
#include "../../kernel/fs/inflate.c"
#include "../../kernel/fs/xz.c"
#include "../../kernel/fs/bzip2.c"


extern char **environ;
static int pk_verbose;
static char pk_err[512];
#include <stdarg.h>
static int pk_fail(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(pk_err, sizeof pk_err, fmt, ap);
    va_end(ap);
    return -1;
}
#define PKERR(...) pk_fail(__VA_ARGS__)

/* ======================================================================== utilidades */
static u16 rb16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }
static u32 rb32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u64 rb64(const u8 *p) { return (u64)rb32(p) << 32 | rb32(p + 4); }
static u8 *read_file(const char *path, u64 *n) {
    int fd = open(path, O_RDONLY);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) < 0) { if (fd >= 0) close(fd); return 0; }
    u8 *b = malloc((size_t)st.st_size + 1);
    u64 got = 0;
    while (b && got < (u64)st.st_size) {
        ssize_t r = read(fd, b + got, (size_t)((u64)st.st_size - got));
        if (r <= 0) { free(b); b = 0; break; }
        got += (u64)r;
    }
    close(fd);
    if (b) { b[got] = 0; *n = got; }
    return b;
}
static int write_all(const char *path, const void *d, u64 n, unsigned mode) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode ? mode : 0644);
    if (fd < 0) return -1;
    const u8 *p = d;
    while (n) {
        ssize_t w = write(fd, p, (size_t)(n > (1u << 30) ? (1u << 30) : n));
        if (w <= 0) { close(fd); return -1; }
        p += w; n -= (u64)w;
    }
    return close(fd);
}
static int mkdirs(const char *path, unsigned mode) {
    char t[2048];
    snprintf(t, sizeof t, "%s", path);
    for (char *p = t + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        if (mkdir(t, 0755) < 0 && errno != EEXIST) return -1;
        *p = '/';
    }
    if (mkdir(t, mode ? mode : 0755) < 0 && errno != EEXIST) return -1;
    return 0;
}
static void path_join(char *out, size_t cap, const char *a, const char *b) {
    size_t n = strlen(a);
    while (*b == '/') b++;
    while (b[0] == '.' && b[1] == '/') b += 2;
    if (!strcmp(b, ".")) b = "";
    snprintf(out, cap, "%s%s%s", a, n && a[n - 1] == '/' ? "" : (*b ? "/" : ""), b);
}
/* caminho relativo de pacote: sem ".." (path traversal) */
static int rel_ok(const char *r) {
    if (!r) return 0;
    for (const char *p = r; *p;) {
        while (*p == '/') p++;
        const char *s = p;
        while (*p && *p != '/') p++;
        if (p - s == 2 && s[0] == '.' && s[1] == '.') return 0;
    }
    return 1;
}

/* ======================================================================== hashes (checksums do xar) */
typedef struct { u32 h[5]; u64 len; u8 buf[64]; u32 n; } sha1_t;
#define ROL(x, k) (((x) << (k)) | ((x) >> (32 - (k))))
static void sha1_blk(sha1_t *s, const u8 *b) {
    u32 w[80], a = s->h[0], bb = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4];
    for (int i = 0; i < 16; i++) w[i] = rb32(b + 4 * i);
    for (int i = 16; i < 80; i++) w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    for (int i = 0; i < 80; i++) {
        u32 f, k;
        if (i < 20) { f = (bb & c) | (~bb & d); k = 0x5A827999; }
        else if (i < 40) { f = bb ^ c ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (bb & c) | (bb & d) | (c & d); k = 0x8F1BBCDC; }
        else { f = bb ^ c ^ d; k = 0xCA62C1D6; }
        u32 t = ROL(a, 5) + f + e + k + w[i];
        e = d; d = c; c = ROL(bb, 30); bb = a; a = t;
    }
    s->h[0] += a; s->h[1] += bb; s->h[2] += c; s->h[3] += d; s->h[4] += e;
}
static void sha1(const u8 *d, u64 n, u8 out[20]) {
    sha1_t s = {{0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0}, 0, {0}, 0};
    u64 i = 0;
    for (; i + 64 <= n; i += 64) sha1_blk(&s, d + i);
    u8 last[128] = {0};
    u64 r = n - i;
    memcpy(last, d + i, (size_t)r);
    last[r] = 0x80;
    u64 tot = r + 1 + 8 <= 64 ? 64 : 128;
    u64 bits = n * 8;
    for (int k = 0; k < 8; k++) last[tot - 1 - k] = (u8)(bits >> (8 * k));
    sha1_blk(&s, last);
    if (tot == 128) sha1_blk(&s, last + 64);
    for (int k = 0; k < 5; k++) { out[4 * k] = (u8)(s.h[k] >> 24); out[4 * k + 1] = (u8)(s.h[k] >> 16); out[4 * k + 2] = (u8)(s.h[k] >> 8); out[4 * k + 3] = (u8)s.h[k]; }
}
static const u32 K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROR(x, k) (((x) >> (k)) | ((x) << (32 - (k))))
static void sha256_blk(u32 h[8], const u8 *b) {
    u32 w[64];
    for (int i = 0; i < 16; i++) w[i] = rb32(b + 4 * i);
    for (int i = 16; i < 64; i++) {
        u32 s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    u32 a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
        u32 t1 = hh + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
        u32 t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & bb) ^ (a & c) ^ (bb & c));
        hh = g; g = f; f = e; e = d + t1; d = c; c = bb; bb = a; a = t1 + t2;
    }
    h[0] += a; h[1] += bb; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}
static void sha256(const u8 *d, u64 n, u8 out[32]) {
    u32 h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    u64 i = 0;
    for (; i + 64 <= n; i += 64) sha256_blk(h, d + i);
    u8 last[128] = {0};
    u64 r = n - i;
    memcpy(last, d + i, (size_t)r);
    last[r] = 0x80;
    u64 tot = r + 1 + 8 <= 64 ? 64 : 128, bits = n * 8;
    for (int k = 0; k < 8; k++) last[tot - 1 - k] = (u8)(bits >> (8 * k));
    sha256_blk(h, last);
    if (tot == 128) sha256_blk(h, last + 64);
    for (int k = 0; k < 8; k++) { out[4 * k] = (u8)(h[k] >> 24); out[4 * k + 1] = (u8)(h[k] >> 16); out[4 * k + 2] = (u8)(h[k] >> 8); out[4 * k + 3] = (u8)h[k]; }
}
static void md5(const u8 *msg, u64 n, u8 out[16]) {
    static const u32 K[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,
        0x6b901122,0xfd987193,0xa679438e,0x49b40821,0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,
        0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,
        0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
    static const u8 S[64] = {7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
                             4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
    u32 h[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    u64 tot = ((n + 8) / 64 + 1) * 64;
    u8 *m = calloc(1, (size_t)tot);
    if (!m) { memset(out, 0, 16); return; }
    memcpy(m, msg, (size_t)n);
    m[n] = 0x80;
    u64 bits = n * 8;
    for (int k = 0; k < 8; k++) m[tot - 8 + k] = (u8)(bits >> (8 * k));
    for (u64 o = 0; o < tot; o += 64) {
        u32 w[16];
        for (int i = 0; i < 16; i++) w[i] = (u32)m[o + 4 * i] | (u32)m[o + 4 * i + 1] << 8 | (u32)m[o + 4 * i + 2] << 16 | (u32)m[o + 4 * i + 3] << 24;
        u32 a = h[0], b = h[1], c = h[2], d = h[3];
        for (int i = 0; i < 64; i++) {
            u32 f; int g;
            if (i < 16) { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
            else { f = c ^ (b | ~d); g = (7 * i) % 16; }
            u32 t = d; d = c; c = b;
            b = b + ROL(a + f + K[i] + w[g], S[i]);
            a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    }
    free(m);
    for (int k = 0; k < 4; k++) for (int j = 0; j < 4; j++) out[4 * k + j] = (u8)(h[k] >> (8 * j));
}
static int hex_eq(const char *hex, const u8 *d, int n) {
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < n; i++) {
        char a = hex[2 * i], b = hex[2 * i + 1];
        if (a >= 'A' && a <= 'F') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'F') b = (char)(b - 'A' + 'a');
        if (a != hx[d[i] >> 4] || b != hx[d[i] & 15]) return 0;
    }
    return hex[2 * n] == 0 || hex[2 * n] == ' ' || hex[2 * n] == '\n';
}
/* confere um checksum do xar: style sha1/sha256/md5 (sha512 e none: aceito sem conferir) */
static int check_sum(const char *style, const char *hex, const u8 *d, u64 n) {
    u8 h[32];
    if (!style || !hex || !hex[0] || !strcmp(style, "none")) return 1;
    if (!strcmp(style, "sha1")) { sha1(d, n, h); return hex_eq(hex, h, 20); }
    if (!strcmp(style, "sha256")) { sha256(d, n, h); return hex_eq(hex, h, 32); }
    if (!strcmp(style, "md5")) { md5(d, n, h); return hex_eq(hex, h, 16); }
    return 1;
}

/* ======================================================================== XML (TOC, Distribution, PackageInfo, plist) */
typedef struct xnode {
    char *name, *text;
    char *attr[32];        /* pares nome, valor */
    int nattr;
    struct xnode *child, *next, *parent;
} xnode;
static void xfree(xnode *n) {
    while (n) {
        xnode *nx = n->next;
        xfree(n->child);
        free(n->name); free(n->text);
        for (int i = 0; i < n->nattr * 2; i++) free(n->attr[i]);
        free(n);
        n = nx;
    }
}
static char *xdecode(const char *s, size_t n) {
    char *o = malloc(n + 1);
    size_t k = 0;
    if (!o) return 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '&') {
            const char *e = memchr(s + i, ';', n - i);
            if (e && e - (s + i) < 12) {
                size_t l = (size_t)(e - (s + i));
                if (l == 3 && !strncmp(s + i, "&lt", 3)) o[k++] = '<';
                else if (l == 3 && !strncmp(s + i, "&gt", 3)) o[k++] = '>';
                else if (l == 4 && !strncmp(s + i, "&amp", 4)) o[k++] = '&';
                else if (l == 5 && !strncmp(s + i, "&quot", 5)) o[k++] = '"';
                else if (l == 5 && !strncmp(s + i, "&apos", 5)) o[k++] = '\'';
                else if (s[i + 1] == '#') {
                    unsigned long c = s[i + 2] == 'x' ? strtoul(s + i + 3, 0, 16) : strtoul(s + i + 2, 0, 10);
                    if (c < 0x80) o[k++] = (char)c;
                    else if (c < 0x800) { o[k++] = (char)(0xC0 | c >> 6); o[k++] = (char)(0x80 | (c & 63)); }
                    else { o[k++] = (char)(0xE0 | c >> 12); o[k++] = (char)(0x80 | ((c >> 6) & 63)); o[k++] = (char)(0x80 | (c & 63)); }
                } else { memcpy(o + k, s + i, l + 1); k += l + 1; }
                i += l;
                continue;
            }
        }
        o[k++] = s[i];
    }
    o[k] = 0;
    return o;
}
static xnode *xparse(const char *s, size_t n) {
    xnode *root = calloc(1, sizeof *root), *cur = root;
    if (!root) return 0;
    root->name = strdup("#root");
    size_t i = 0;
    while (i < n) {
        if (s[i] != '<') {
            size_t st = i;
            while (i < n && s[i] != '<') i++;
            int blank = 1;
            for (size_t k = st; k < i; k++) if (s[k] != ' ' && s[k] != '\n' && s[k] != '\r' && s[k] != '\t') blank = 0;
            if (!blank && !cur->child) {
                char *t = xdecode(s + st, i - st);
                if (cur->text) { size_t a = strlen(cur->text), b = strlen(t); char *u = realloc(cur->text, a + b + 1); if (u) { memcpy(u + a, t, b + 1); cur->text = u; } free(t); }
                else cur->text = t;
            }
            continue;
        }
        if (i + 4 <= n && !strncmp(s + i, "<!--", 4)) { const char *e = strstr(s + i, "-->"); i = e ? (size_t)(e - s) + 3 : n; continue; }
        if (i + 9 <= n && !strncmp(s + i, "<![CDATA[", 9)) {
            const char *e = strstr(s + i + 9, "]]>");
            size_t l = e ? (size_t)(e - (s + i + 9)) : n - i - 9;
            free(cur->text);
            cur->text = malloc(l + 1);
            if (cur->text) { memcpy(cur->text, s + i + 9, l); cur->text[l] = 0; }
            i = e ? (size_t)(e - s) + 3 : n;
            continue;
        }
        if (s[i + 1] == '?' || s[i + 1] == '!') { while (i < n && s[i] != '>') i++; i++; continue; }
        if (s[i + 1] == '/') {
            while (i < n && s[i] != '>') i++;
            i++;
            if (cur->parent) cur = cur->parent;
            continue;
        }
        i++;
        size_t st = i;
        while (i < n && s[i] != ' ' && s[i] != '>' && s[i] != '/' && s[i] != '\n' && s[i] != '\t' && s[i] != '\r') i++;
        xnode *x = calloc(1, sizeof *x);
        if (!x) break;
        x->name = strndup(s + st, i - st);
        x->parent = cur;
        xnode **pp = &cur->child;
        while (*pp) pp = &(*pp)->next;
        *pp = x;
        for (;;) {
            while (i < n && (s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == '\r')) i++;
            if (i >= n || s[i] == '>' || s[i] == '/') break;
            size_t a = i;
            while (i < n && s[i] != '=' && s[i] != ' ' && s[i] != '>') i++;
            char *an = strndup(s + a, i - a);
            while (i < n && s[i] != '"' && s[i] != '\'' && s[i] != '>') i++;
            char *av = strdup("");
            if (i < n && (s[i] == '"' || s[i] == '\'')) {
                char q = s[i++];
                size_t b = i;
                while (i < n && s[i] != q) i++;
                free(av);
                av = xdecode(s + b, i - b);
                i++;
            }
            if (x->nattr < 16) { x->attr[2 * x->nattr] = an; x->attr[2 * x->nattr + 1] = av; x->nattr++; }
            else { free(an); free(av); }
        }
        if (i < n && s[i] == '/') { while (i < n && s[i] != '>') i++; i++; continue; }
        i++;
        cur = x;
    }
    return root;
}
static const char *xattr_of(const xnode *n, const char *a) {
    for (int i = 0; n && i < n->nattr; i++) if (!strcmp(n->attr[2 * i], a)) return n->attr[2 * i + 1];
    return 0;
}
static xnode *xchild(const xnode *n, const char *name) {
    for (xnode *c = n ? n->child : 0; c; c = c->next) if (!strcmp(c->name, name)) return c;
    return 0;
}
static xnode *xfind(xnode *n, const char *name) {   /* em profundidade */
    for (xnode *c = n ? n->child : 0; c; c = c->next) {
        if (!strcmp(c->name, name)) return c;
        xnode *r = xfind(c, name);
        if (r) return r;
    }
    return 0;
}
static const char *xtext(const xnode *n) { return n && n->text ? n->text : ""; }

/* ======================================================================== xar */
typedef struct xent {
    char path[1024];
    int dir;
    u64 off, len, size;
    char enc[64], asum_style[16], asum[132], esum_style[16], esum[132];
    unsigned mode;
} xent_t;
typedef struct {
    u8 *data; u64 n;
    u64 heap;
    xent_t *e; int ne;
    xnode *toc;
    int signed_;
    int ncerts;
} xar_t;
static void xar_close(xar_t *x) { free(x->data); free(x->e); xfree(x->toc); memset(x, 0, sizeof *x); }
static void xar_walk(xar_t *x, xnode *f, const char *prefix) {
    for (; f; f = f->next) {
        if (strcmp(f->name, "file")) continue;
        xnode *nm = xchild(f, "name"), *ty = xchild(f, "type"), *da = xchild(f, "data");
        char p[1024];
        if (!nm) continue;
        if (prefix[0]) snprintf(p, sizeof p, "%s/%s", prefix, xtext(nm)); else snprintf(p, sizeof p, "%s", xtext(nm));
        xent_t *n = realloc(x->e, sizeof *n * (size_t)(x->ne + 1));
        if (!n) return;
        x->e = n;
        xent_t *e = &x->e[x->ne++];
        memset(e, 0, sizeof *e);
        snprintf(e->path, sizeof e->path, "%s", p);
        e->dir = ty && !strcmp(xtext(ty), "directory");
        xnode *mo = xchild(f, "mode");
        e->mode = mo ? (unsigned)strtoul(xtext(mo), 0, 8) : 0;
        if (da) {
            e->off = strtoull(xtext(xchild(da, "offset")), 0, 10);
            e->len = strtoull(xtext(xchild(da, "length")), 0, 10);
            e->size = strtoull(xtext(xchild(da, "size")), 0, 10);
            xnode *en = xchild(da, "encoding");
            snprintf(e->enc, sizeof e->enc, "%s", en && xattr_of(en, "style") ? xattr_of(en, "style") : "application/octet-stream");
            xnode *as = xchild(da, "archived-checksum"), *es = xchild(da, "extracted-checksum");
            if (as) { snprintf(e->asum_style, sizeof e->asum_style, "%s", xattr_of(as, "style") ? xattr_of(as, "style") : ""); snprintf(e->asum, sizeof e->asum, "%s", xtext(as)); }
            if (es) { snprintf(e->esum_style, sizeof e->esum_style, "%s", xattr_of(es, "style") ? xattr_of(es, "style") : ""); snprintf(e->esum, sizeof e->esum, "%s", xtext(es)); }
        }
        xar_walk(x, f->child, p);
    }
}
static int xar_open(xar_t *x, const char *path) {
    memset(x, 0, sizeof *x);
    x->data = read_file(path, &x->n);
    if (!x->data) return PKERR("não foi possível ler %s (%s)", path, strerror(errno));
    if (x->n < 28 || memcmp(x->data, "xar!", 4)) { xar_close(x); return PKERR("%s não é um pacote xar", path); }
    u16 hs = rb16(x->data + 4);
    u64 tc = rb64(x->data + 8), tu = rb64(x->data + 16);
    u32 alg = rb32(x->data + 24);
    if (hs < 28 || hs + tc > x->n || tu > (64u << 20)) { xar_close(x); return PKERR("cabeçalho xar inválido"); }
    u8 *toc = malloc((size_t)tu + 1);
    if (!toc || zlib_decompress(x->data + hs, tc, toc, tu) != (i64)tu) { free(toc); xar_close(x); return PKERR("TOC do xar corrompido"); }
    toc[tu] = 0;
    x->heap = hs + tc;
    x->toc = xparse((char *)toc, (size_t)tu);
    xnode *t = xfind(x->toc, "toc");
    /* checksum do TOC: no começo do heap, guardado em <checksum style><offset><size> */
    xnode *ck = t ? xchild(t, "checksum") : 0;
    if (ck && alg != 0) {
        u64 co = strtoull(xtext(xchild(ck, "offset")), 0, 10), cs = strtoull(xtext(xchild(ck, "size")), 0, 10);
        const char *st = xattr_of(ck, "style");
        u8 h[32];
        int ok = 1;
        if (x->heap + co + cs > x->n) ok = 0;
        else if (st && !strcmp(st, "sha1") && cs == 20) { sha1(x->data + hs, tc, h); ok = !memcmp(h, x->data + x->heap + co, 20); }
        else if (st && !strcmp(st, "sha256") && cs == 32) { sha256(x->data + hs, tc, h); ok = !memcmp(h, x->data + x->heap + co, 32); }
        else if (st && !strcmp(st, "md5") && cs == 16) { md5(x->data + hs, tc, h); ok = !memcmp(h, x->data + x->heap + co, 16); }
        if (!ok) { free(toc); xar_close(x); return PKERR("checksum do TOC não confere: pacote corrompido"); }
    }
    xnode *sg = t ? xchild(t, "signature") : 0;
    if (sg) {
        x->signed_ = 1;
        xnode *ki = xchild(sg, "KeyInfo"), *xd = ki ? xchild(ki, "X509Data") : 0;
        for (xnode *c = xd ? xd->child : 0; c; c = c->next) if (!strcmp(c->name, "X509Certificate")) x->ncerts++;
    }
    free(toc);
    if (t) xar_walk(x, t->child, "");
    return 0;
}
static xent_t *xar_find(xar_t *x, const char *path) {
    for (int i = 0; i < x->ne; i++) if (!strcmp(x->e[i].path, path)) return &x->e[i];
    return 0;
}
/* dados de um membro, já decodificados (gzip/zlib, bzip2, xz, cru); confere os checksums */
static u8 *xar_read(xar_t *x, xent_t *e, u64 *outn) {
    if (x->heap + e->off + e->len > x->n) { PKERR("membro %s fora do arquivo", e->path); return 0; }
    const u8 *src = x->data + x->heap + e->off;
    if (e->asum_style[0] && !check_sum(e->asum_style, e->asum, src, e->len)) { PKERR("checksum de %s não confere", e->path); return 0; }
    u8 *out = malloc((size_t)e->size + 1);
    if (!out) { PKERR("sem memória para %s (%llu bytes)", e->path, (unsigned long long)e->size); return 0; }
    i64 r;
    if (strstr(e->enc, "gzip") || strstr(e->enc, "zlib")) r = zlib_decompress(src, e->len, out, e->size);
    else if (strstr(e->enc, "bzip2")) r = bzip2_decompress(src, e->len, out, e->size);
    else if (strstr(e->enc, "xz") || strstr(e->enc, "lzma")) r = xz_decompress(src, e->len, out, e->size);
    else { r = (i64)(e->len < e->size ? e->len : e->size); memcpy(out, src, (size_t)r); }
    if (r != (i64)e->size) { free(out); PKERR("não foi possível descomprimir %s (%s)", e->path, e->enc); return 0; }
    if (e->esum_style[0] && !check_sum(e->esum_style, e->esum, out, e->size)) { free(out); PKERR("checksum extraído de %s não confere", e->path); return 0; }
    out[e->size] = 0;
    *outn = e->size;
    return out;
}

/* ======================================================================== Payload / Scripts: gzip, pbzx, bzip2, xz -> cpio */
static u8 *unwrap(const u8 *p, u64 n, u64 *outn) {
    if (n >= 18 && p[0] == 0x1f && p[1] == 0x8b) {               /* gzip */
        u32 isz = (u32)p[n - 4] | (u32)p[n - 3] << 8 | (u32)p[n - 2] << 16 | (u32)p[n - 1] << 24;
        u64 o = 10;
        u8 fl = p[3];
        if (fl & 4) o += 2 + (u64)(p[o] | p[o + 1] << 8);
        if (fl & 8) { while (o < n && p[o]) o++; o++; }
        if (fl & 16) { while (o < n && p[o]) o++; o++; }
        if (fl & 2) o += 2;
        /* ISIZE é o tamanho mod 2^32: para payloads > 4 GiB, aumenta até o inflate caber */
        u64 cap = isz;
        for (int tries = 0; tries < 8; tries++) {
            u8 *out = malloc((size_t)cap + 1);
            if (!out) return 0;
            i64 r = zlib_decompress(p + o, n - o - 8, out, cap);
            if (r >= 0 && (u32)r == isz) { *outn = (u64)r; return out; }
            free(out);
            cap += 1ull << 32;
        }
        return 0;
    }
    if (n >= 12 && !memcmp(p, "pbzx", 4)) {                       /* pbzx: blocos {u64 tamanho, u64 comprimido, dados} */
        u64 o = 12, tot = 0, cap = 1 << 20;
        u8 *out = malloc((size_t)cap);
        while (out && o + 16 <= n) {
            u64 ulen = rb64(p + o), clen = rb64(p + o + 8);
            o += 16;
            if (o + clen > n) { free(out); return 0; }
            if (tot + ulen > cap) {
                while (tot + ulen > cap) cap *= 2;
                u8 *nb = realloc(out, (size_t)cap);
                if (!nb) { free(out); return 0; }
                out = nb;
            }
            if (clen == ulen) memcpy(out + tot, p + o, (size_t)clen);   /* bloco cru */
            else if (xz_decompress(p + o, clen, out + tot, ulen) != (i64)ulen) { free(out); return 0; }
            tot += ulen;
            o += clen;
        }
        *outn = tot;
        return out;
    }
    if (n >= 4 && p[0] == 'B' && p[1] == 'Z' && p[2] == 'h') {    /* bzip2: tamanho desconhecido, cresce */
        for (u64 cap = n * 8 + (1 << 20); cap < (8ull << 30); cap *= 2) {
            u8 *out = malloc((size_t)cap);
            if (!out) return 0;
            i64 r = bzip2_decompress(p, n, out, cap);
            if (r >= 0 && (u64)r < cap) { *outn = (u64)r; return out; }
            free(out);
            if (r < 0 && r != -2) return 0;
        }
        return 0;
    }
    if (n >= 6 && !memcmp(p, "\xFD" "7zXZ", 6)) {
        for (u64 cap = n * 8 + (1 << 20); cap < (8ull << 30); cap *= 2) {
            u8 *out = malloc((size_t)cap);
            if (!out) return 0;
            i64 r = xz_decompress(p, n, out, cap);
            if (r >= 0 && (u64)r < cap) { *outn = (u64)r; return out; }
            free(out);
        }
        return 0;
    }
    u8 *out = malloc((size_t)n + 1);                               /* cpio cru */
    if (out) { memcpy(out, p, (size_t)n); *outn = n; }
    return out;
}

typedef struct { const char *name; unsigned mode, uid, gid; long mtime; u64 ino, nlink; const u8 *data; u64 size; } cpio_ent_t;
typedef int (*cpio_cb)(void *ctx, const cpio_ent_t *e);
static u64 oct(const u8 *p, int n) { u64 v = 0; for (int i = 0; i < n; i++) v = v * 8 + (u64)(p[i] - '0'); return v; }
static u64 hex(const u8 *p, int n) { u64 v = 0; for (int i = 0; i < n; i++) { int c = p[i]; v = v * 16 + (u64)(c <= '9' ? c - '0' : (c | 32) - 'a' + 10); } return v; }
/* cpio odc (070707) e newc/crc (070701/070702) */
static int cpio_each(const u8 *p, u64 n, cpio_cb cb, void *ctx) {
    u64 o = 0;
    while (o + 76 <= n) {
        cpio_ent_t e;
        memset(&e, 0, sizeof e);
        u64 namesz, hdr;
        if (!memcmp(p + o, "070707", 6)) {
            e.ino = oct(p + o + 12, 6); e.mode = (unsigned)oct(p + o + 18, 6); e.uid = (unsigned)oct(p + o + 24, 6); e.gid = (unsigned)oct(p + o + 30, 6);
            e.nlink = oct(p + o + 36, 6); e.mtime = (long)oct(p + o + 48, 11); namesz = oct(p + o + 59, 6); e.size = oct(p + o + 65, 11);
            hdr = 76;
            if (o + hdr + namesz > n) return PKERR("cpio truncado");
            e.name = (const char *)p + o + hdr;
            o += hdr + namesz;
        } else if (!memcmp(p + o, "070701", 6) || !memcmp(p + o, "070702", 6)) {
            if (o + 110 > n) return PKERR("cpio truncado");
            e.ino = hex(p + o + 6, 8); e.mode = (unsigned)hex(p + o + 14, 8); e.uid = (unsigned)hex(p + o + 22, 8); e.gid = (unsigned)hex(p + o + 30, 8);
            e.nlink = hex(p + o + 38, 8); e.mtime = (long)hex(p + o + 46, 8); e.size = hex(p + o + 54, 8); namesz = hex(p + o + 94, 8);
            hdr = 110;
            if (o + hdr + namesz > n) return PKERR("cpio truncado");
            e.name = (const char *)p + o + hdr;
            o = (o + hdr + namesz + 3) & ~3ull;
        } else return PKERR("formato de cpio desconhecido no deslocamento %llu", (unsigned long long)o);
        if (!strcmp(e.name, "TRAILER!!!")) return 0;
        if (o + e.size > n) return PKERR("cpio truncado em %s", e.name);
        e.data = p + o;
        o += e.size;
        if (!memcmp(p + o - e.size - (hdr == 76 ? 0 : 0), "", 0)) {}
        if (hdr == 110) o = (o + 3) & ~3ull;
        int r = cb(ctx, &e);
        if (r) return r;
    }
    return 0;
}

/* ======================================================================== Bom (lista de caminhos do pacote) */
typedef struct { char **v; int n, cap; } strv_t;
static void strv_add(strv_t *s, const char *p) {
    if (s->n == s->cap) { int nc = s->cap ? s->cap * 2 : 64; char **nv = realloc(s->v, sizeof *nv * (size_t)nc); if (!nv) return; s->v = nv; s->cap = nc; }
    s->v[s->n++] = strdup(p);
}
static void strv_free(strv_t *s) { for (int i = 0; i < s->n; i++) free(s->v[i]); free(s->v); memset(s, 0, sizeof *s); }
/* BOMStore: cabeçalho, índice de blocos {endereço, tamanho}, variáveis ("Paths" -> árvore BOMTree). Cada nó
 * folha da árvore Paths traz pares {u32 bloco do valor (BOMPathInfo1), u32 bloco da chave (u32 pai, nome)}. */
static int bom_block(const u8 *b, u64 n, u32 idx, const u8 **p, u32 *len) {
    u32 io = rb32(b + 16), il = rb32(b + 20);
    (void)il;
    if ((u64)io + 4 + 8ull * (idx + 1) > n) return -1;
    u32 cnt = rb32(b + io);
    if (idx >= cnt) return -1;
    u32 a = rb32(b + io + 4 + 8 * idx), l = rb32(b + io + 8 + 8 * idx);
    if ((u64)a + l > n) return -1;
    *p = b + a; *len = l;
    return 0;
}
typedef struct { u32 id, parent; char name[256]; } bomnode_t;
static int bom_paths(const u8 *b, u64 n, strv_t *out) {
    if (n < 32 || memcmp(b, "BOMStore", 8)) return PKERR("Bom inválido");
    u32 vo = rb32(b + 24);
    if ((u64)vo + 4 > n) return PKERR("Bom inválido");
    u32 nv = rb32(b + vo);
    u64 o = vo + 4;
    u32 tree = 0xffffffffu;
    for (u32 i = 0; i < nv && o + 5 <= n; i++) {
        u32 ix = rb32(b + o);
        u8 ln = b[o + 4];
        if (o + 5 + ln > n) break;
        if (ln == 5 && !memcmp(b + o + 5, "Paths", 5)) tree = ix;
        o += 5 + ln;
    }
    const u8 *t; u32 tl;
    if (tree == 0xffffffffu || bom_block(b, n, tree, &t, &tl) < 0 || tl < 21 || memcmp(t, "tree", 4)) return PKERR("Bom sem árvore Paths");
    u32 child = rb32(t + 8);
    bomnode_t *nodes = 0;
    int nn = 0, cap = 0;
    /* desce pelos nós de índice até a primeira folha, depois anda pelas folhas (forward) */
    const u8 *pg; u32 pl;
    int guard = 0;
    while (bom_block(b, n, child, &pg, &pl) == 0 && pl >= 12 && !rb16(pg) && guard++ < 64) {
        if (rb16(pg + 2) == 0) break;
        child = rb32(pg + 12);                                    /* 1º filho (valor do 1º par) */
    }
    for (int pages = 0; pages < 1000000; pages++) {
        if (bom_block(b, n, child, &pg, &pl) < 0 || pl < 12) break;
        u16 cnt = rb16(pg + 2);
        u32 fwd = rb32(pg + 4);
        for (u16 i = 0; i < cnt && 12 + 8u * (i + 1) <= pl; i++) {
            u32 vi = rb32(pg + 12 + 8 * i), ki = rb32(pg + 16 + 8 * i);
            const u8 *vp, *kp; u32 vl, kl;
            if (bom_block(b, n, vi, &vp, &vl) < 0 || bom_block(b, n, ki, &kp, &kl) < 0 || vl < 4 || kl < 5) continue;
            if (nn == cap) { cap = cap ? cap * 2 : 256; bomnode_t *x = realloc(nodes, sizeof *x * (size_t)cap); if (!x) break; nodes = x; }
            bomnode_t *d = &nodes[nn++];
            d->id = rb32(vp);
            d->parent = rb32(kp);
            u32 k = kl - 4 < 255 ? kl - 4 : 255;
            memcpy(d->name, kp + 4, k); d->name[k] = 0;
            d->name[strnlen(d->name, k)] = 0;
        }
        if (!fwd) break;
        child = fwd;
    }
    for (int i = 0; i < nn; i++) {
        char p[2048] = "", tmp[2048];
        u32 cur = nodes[i].id;
        int depth = 0;
        for (int k = i; k >= 0 && depth < 128; depth++) {
            snprintf(tmp, sizeof tmp, "%s%s%s", nodes[k].name, p[0] ? "/" : "", p);
            memcpy(p, tmp, sizeof p);
            u32 par = nodes[k].parent;
            k = -1;
            if (!par) break;
            for (int j = 0; j < nn; j++) if (nodes[j].id == par) { k = j; break; }
        }
        (void)cur;
        strv_add(out, p);
    }
    free(nodes);
    return 0;
}

/* ======================================================================== recibos (/var/db/receipts) */
static void plist_str(FILE *f, const char *k, const char *v) {
    fprintf(f, "\t<key>%s</key>\n\t<string>", k);
    for (const char *s = v ? v : ""; *s; s++) {
        if (*s == '&') fputs("&amp;", f); else if (*s == '<') fputs("&lt;", f); else if (*s == '>') fputs("&gt;", f); else fputc(*s, f);
    }
    fputs("</string>\n", f);
}
typedef struct {
    char id[256], version[128], location[1024], file[512], process[64];
    long date;
} receipt_t;
static int receipt_write(const char *vol, const receipt_t *r, const u8 *bom, u64 bomn) {
    char dir[1024], p[1300];
    path_join(dir, sizeof dir, vol, "var/db/receipts");
    if (mkdirs(dir, 0755) < 0) return PKERR("não foi possível criar %s", dir);
    snprintf(p, sizeof p, "%s/%s.plist", dir, r->id);
    FILE *f = fopen(p, "w");
    if (!f) return PKERR("não foi possível gravar %s", p);
    char date[32];
    time_t t = (time_t)r->date;
    strftime(date, sizeof date, "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
          "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">\n<dict>\n", f);
    fprintf(f, "\t<key>InstallDate</key>\n\t<date>%s</date>\n", date);
    plist_str(f, "InstallPrefixPath", r->location[0] == '/' ? r->location + 1 : r->location);
    plist_str(f, "InstallProcessName", r->process);
    plist_str(f, "PackageFileName", r->file);
    plist_str(f, "PackageIdentifier", r->id);
    plist_str(f, "PackageVersion", r->version);
    fputs("</dict>\n</plist>\n", f);
    fclose(f);
    if (bom && bomn) {
        snprintf(p, sizeof p, "%s/%s.bom", dir, r->id);
        if (write_all(p, bom, bomn, 0644) < 0) return PKERR("não foi possível gravar %s", p);
    }
    return 0;
}
static int receipt_read(const char *vol, const char *id, receipt_t *r) {
    char p[1300], dir[1024];
    u64 n;
    path_join(dir, sizeof dir, vol, "var/db/receipts");
    snprintf(p, sizeof p, "%s/%s.plist", dir, id);
    u8 *d = read_file(p, &n);
    if (!d) return -1;
    memset(r, 0, sizeof *r);
    xnode *x = xparse((char *)d, n), *dict = xfind(x, "dict");
    for (xnode *k = dict ? dict->child : 0; k; k = k->next) {
        if (strcmp(k->name, "key") || !k->next) continue;
        const char *key = xtext(k), *val = xtext(k->next);
        if (!strcmp(key, "PackageIdentifier")) snprintf(r->id, sizeof r->id, "%s", val);
        else if (!strcmp(key, "PackageVersion")) snprintf(r->version, sizeof r->version, "%s", val);
        else if (!strcmp(key, "InstallPrefixPath")) snprintf(r->location, sizeof r->location, "%s", val);
        else if (!strcmp(key, "PackageFileName")) snprintf(r->file, sizeof r->file, "%s", val);
        else if (!strcmp(key, "InstallProcessName")) snprintf(r->process, sizeof r->process, "%s", val);
        else if (!strcmp(key, "InstallDate")) {
            struct tm tm; memset(&tm, 0, sizeof tm);
            if (sscanf(val, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
                tm.tm_year -= 1900; tm.tm_mon -= 1;
                r->date = (long)timegm(&tm);
            }
        }
        k = k->next;
    }
    xfree(x);
    free(d);
    return r->id[0] ? 0 : -1;
}
static int receipts_list(const char *vol, strv_t *ids) {
    char dir[1024];
    path_join(dir, sizeof dir, vol, "var/db/receipts");
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t l = strlen(e->d_name);
        if (l > 6 && !strcmp(e->d_name + l - 6, ".plist")) { char id[256]; snprintf(id, sizeof id, "%.*s", (int)(l - 6), e->d_name); strv_add(ids, id); }
    }
    closedir(d);
    for (int i = 1; i < ids->n; i++) for (int j = i; j > 0 && strcmp(ids->v[j - 1], ids->v[j]) > 0; j--) { char *t = ids->v[j]; ids->v[j] = ids->v[j - 1]; ids->v[j - 1] = t; }
    return ids->n;
}

/* ======================================================================== o pacote: componentes e escolhas */
typedef struct {
    char id[256], version[128], location[1024], dir[512];   /* dir: membro no xar ("" = pacote de componente); bundle: pasta */
    char pre[256], post[256];                                /* nomes dos scripts (relativos ao Scripts) */
    char auth[32];
    int selected;
    u64 kbytes, nfiles;
    xnode *bundles;
} comp_t;
typedef struct {
    xar_t x;
    char path[1024], name[512], title[256];
    comp_t c[64];
    int nc;
    int product;                                             /* tem Distribution */
    int bundle;                                              /* formato antigo: pasta .pkg/.mpkg (Contents/Archive.pax.gz) */
    xnode *dist;
    u8 *dist_raw;
} pkg_t;
static void pkg_close(pkg_t *p) {
    for (int i = 0; i < p->nc; i++) xfree(p->c[i].bundles);
    xfree(p->dist);
    free(p->dist_raw);
    xar_close(&p->x);
}
static int comp_info(pkg_t *p, comp_t *c) {
    char m[600];
    snprintf(m, sizeof m, "%s%sPackageInfo", c->dir, c->dir[0] ? "/" : "");
    xent_t *e = xar_find(&p->x, m);
    if (!e) return PKERR("%s sem PackageInfo", c->dir[0] ? c->dir : p->name);
    u64 n;
    u8 *d = xar_read(&p->x, e, &n);
    if (!d) return -1;
    xnode *x = xparse((char *)d, n), *pi = xfind(x, "pkg-info");
    free(d);
    if (!pi) { xfree(x); return PKERR("PackageInfo inválido em %s", c->dir); }
    if (xattr_of(pi, "identifier")) snprintf(c->id, sizeof c->id, "%s", xattr_of(pi, "identifier"));
    if (xattr_of(pi, "version")) snprintf(c->version, sizeof c->version, "%s", xattr_of(pi, "version"));
    if (xattr_of(pi, "install-location")) snprintf(c->location, sizeof c->location, "%s", xattr_of(pi, "install-location"));
    else snprintf(c->location, sizeof c->location, "/");
    if (xattr_of(pi, "auth")) snprintf(c->auth, sizeof c->auth, "%s", xattr_of(pi, "auth"));
    xnode *pl = xchild(pi, "payload");
    if (pl) { c->kbytes = strtoull(xattr_of(pl, "installKBytes") ? xattr_of(pl, "installKBytes") : "0", 0, 10); c->nfiles = strtoull(xattr_of(pl, "numberOfFiles") ? xattr_of(pl, "numberOfFiles") : "0", 0, 10); }
    xnode *sc = xchild(pi, "scripts");
    if (sc) {
        xnode *a = xchild(sc, "preinstall"), *b = xchild(sc, "postinstall");
        if (a && xattr_of(a, "file")) snprintf(c->pre, sizeof c->pre, "%s", xattr_of(a, "file"));
        if (b && xattr_of(b, "file")) snprintf(c->post, sizeof c->post, "%s", xattr_of(b, "file"));
    }
    xnode *bv = xchild(pi, "bundle-version");
    if (bv) { c->bundles = bv; /* fica com o x */ }
    if (bv) {                                                  /* desliga bv do resto para liberar o x */
        xnode **pp = &pi->child;
        while (*pp && *pp != bv) pp = &(*pp)->next;
        if (*pp) *pp = bv->next;
        bv->next = 0;
    }
    xfree(x);
    if (!c->id[0]) return PKERR("PackageInfo sem identifier em %s", c->dir);
    return 0;
}
/* valor de uma chave de um Info.plist (texto) */
static int plist_get(xnode *x, const char *key, char *out, size_t cap) {
    xnode *dict = xfind(x, "dict");
    for (xnode *k = dict ? dict->child : 0; k; k = k->next) {
        if (strcmp(k->name, "key") || !k->next) continue;
        if (!strcmp(xtext(k), key)) { snprintf(out, cap, "%s", xtext(k->next)); return 1; }
        k = k->next;
    }
    return 0;
}
/* pacote antigo em pasta: Contents/Info.plist, Contents/Archive.pax.gz, Contents/Archive.bom, Contents/Resources/<script> */
static int bundle_comp(pkg_t *p, const char *dir) {
    char ip[1300];
    u64 n;
    snprintf(ip, sizeof ip, "%s/Contents/Info.plist", dir);
    u8 *d = read_file(ip, &n);
    if (!d) return PKERR("%s sem Contents/Info.plist", dir);
    xnode *x = xparse((char *)d, n);
    free(d);
    if (p->nc >= 64) { xfree(x); return 0; }
    comp_t *c = &p->c[p->nc++];
    memset(c, 0, sizeof *c);
    snprintf(c->dir, sizeof c->dir, "%s", dir);
    c->selected = 1;
    if (!plist_get(x, "CFBundleIdentifier", c->id, sizeof c->id)) {
        const char *b = strrchr(dir, '/');
        snprintf(c->id, sizeof c->id, "%s", b ? b + 1 : dir);
    }
    if (!plist_get(x, "CFBundleShortVersionString", c->version, sizeof c->version)) plist_get(x, "CFBundleVersion", c->version, sizeof c->version);
    if (!plist_get(x, "IFPkgFlagDefaultLocation", c->location, sizeof c->location)) snprintf(c->location, sizeof c->location, "/");
    snprintf(c->pre, sizeof c->pre, "preinstall");
    snprintf(c->post, sizeof c->post, "postinstall");
    xfree(x);
    return 0;
}
static int pkg_open_bundle(pkg_t *p, const char *path) {
    char sub[1300];
    snprintf(sub, sizeof sub, "%s/Contents/Packages", path);
    p->bundle = 1;
    DIR *d = opendir(sub);
    if (d) {                                                 /* .mpkg: os pacotes de Contents/Packages */
        struct dirent *e;
        strv_t names = {0};
        while ((e = readdir(d))) { size_t l = strlen(e->d_name); if (l > 4 && (!strcmp(e->d_name + l - 4, ".pkg") || !strcmp(e->d_name + l - 5, ".mpkg"))) strv_add(&names, e->d_name); }
        closedir(d);
        for (int i = 1; i < names.n; i++) for (int j = i; j > 0 && strcmp(names.v[j - 1], names.v[j]) > 0; j--) { char *t = names.v[j]; names.v[j] = names.v[j - 1]; names.v[j - 1] = t; }
        for (int i = 0; i < names.n; i++) {
            char cp[1600];
            snprintf(cp, sizeof cp, "%s/%s", sub, names.v[i]);
            if (bundle_comp(p, cp) < 0) { strv_free(&names); return -1; }
        }
        strv_free(&names);
        if (!p->nc) return PKERR("%s não tem pacotes em Contents/Packages", path);
        return 0;
    }
    return bundle_comp(p, path);
}
static int pkg_open(pkg_t *p, const char *path) {
    memset(p, 0, sizeof *p);
    snprintf(p->path, sizeof p->path, "%s", path);
    const char *b = strrchr(path, '/');
    snprintf(p->name, sizeof p->name, "%s", b ? b + 1 : path);
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        if (pkg_open_bundle(p, path) < 0) { pkg_close(p); return -1; }
        snprintf(p->title, sizeof p->title, "%s", p->name);
        return 0;
    }
    if (xar_open(&p->x, path) < 0) return -1;
    xent_t *dist = xar_find(&p->x, "Distribution");
    if (dist) {
        u64 n;
        p->dist_raw = xar_read(&p->x, dist, &n);
        if (!p->dist_raw) { pkg_close(p); return -1; }
        p->dist = xparse((char *)p->dist_raw, n);
        p->product = 1;
        xnode *t = xfind(p->dist, "title");
        if (t) snprintf(p->title, sizeof p->title, "%s", xtext(t));
        /* pkg-ref com conteúdo "#nome.pkg": o membro do xar; os atributos dizem id e versão */
        xnode *gs = xfind(p->dist, "installer-gui-script");
        if (!gs) gs = xfind(p->dist, "installer-script");
        for (xnode *r = gs ? gs->child : 0; r && p->nc < 64; r = r->next) {
            if (strcmp(r->name, "pkg-ref") || !r->text || r->text[0] != '#') continue;
            const char *mem = r->text + 1;
            int dup = 0;
            for (int i = 0; i < p->nc; i++) if (!strcmp(p->c[i].dir, mem)) dup = 1;
            if (dup) continue;
            comp_t *c = &p->c[p->nc++];
            snprintf(c->dir, sizeof c->dir, "%s", mem);
            c->selected = 1;
            if (comp_info(p, c) < 0) { pkg_close(p); return -1; }
        }
        if (!p->nc) { pkg_close(p); return PKERR("a Distribution não aponta para nenhum pacote"); }
    } else {
        comp_t *c = &p->c[p->nc++];
        c->selected = 1;
        if (comp_info(p, c) < 0) { pkg_close(p); return -1; }
    }
    if (!p->title[0]) snprintf(p->title, sizeof p->title, "%s", p->name);
    return 0;
}
/* escolhas: as <choice> da Distribution com seus pkg-ref; start_selected="false" desliga */
static void pkg_apply_dist_choices(pkg_t *p) {
    if (!p->dist) return;
    xnode *gs = xfind(p->dist, "installer-gui-script");
    if (!gs) gs = xfind(p->dist, "installer-script");
    for (xnode *ch = gs ? gs->child : 0; ch; ch = ch->next) {
        if (strcmp(ch->name, "choice")) continue;
        const char *ss = xattr_of(ch, "start_selected"), *se = xattr_of(ch, "selected");
        int off = (ss && !strcmp(ss, "false")) || (se && !strcmp(se, "false"));
        if (!off) continue;
        for (xnode *r = ch->child; r; r = r->next) {
            if (strcmp(r->name, "pkg-ref")) continue;
            const char *id = xattr_of(r, "id");
            for (int i = 0; id && i < p->nc; i++) if (!strcmp(p->c[i].id, id)) p->c[i].selected = 0;
        }
    }
}

/* ======================================================================== instalação */
typedef struct {
    const char *root;           /* destino da carga (volume + install-location) */
    strv_t *paths;
    int files, dirs, links;
    int as_root;
} inst_t;
static int put_entry(void *ctx, const cpio_ent_t *e) {
    inst_t *in = ctx;
    const char *nm = e->name;
    while (nm[0] == '.' && nm[1] == '/') nm += 2;
    if (!strcmp(nm, ".") || !nm[0]) return 0;
    if (!rel_ok(nm)) return PKERR("caminho proibido no Payload: %s", e->name);
    char dst[2048];
    path_join(dst, sizeof dst, in->root, nm);
    unsigned type = e->mode & 0170000u, perm = e->mode & 07777u;
    char parent[2048];
    snprintf(parent, sizeof parent, "%s", dst);
    char *sl = strrchr(parent, '/');
    if (sl && sl != parent) { *sl = 0; mkdirs(parent, 0755); }
    if (type == 0040000u) {
        if (mkdir(dst, perm ? perm : 0755) < 0 && errno != EEXIST) return PKERR("não foi possível criar %s (%s)", dst, strerror(errno));
        chmod(dst, perm ? perm : 0755);
        in->dirs++;
    } else if (type == 0120000u) {
        char tgt[1024];
        u64 k = e->size < sizeof tgt - 1 ? e->size : sizeof tgt - 1;
        memcpy(tgt, e->data, (size_t)k); tgt[k] = 0;
        unlink(dst);
        if (symlink(tgt, dst) < 0) return PKERR("não foi possível criar o link %s (%s)", dst, strerror(errno));
        in->links++;
    } else if (type == 0100000u || type == 0) {
        struct stat st;
        if (lstat(dst, &st) == 0 && S_ISDIR(st.st_mode)) return PKERR("%s já existe como pasta", dst);
        unlink(dst);
        if (write_all(dst, e->data, e->size, perm ? perm : 0644) < 0) return PKERR("não foi possível gravar %s (%s)", dst, strerror(errno));
        chmod(dst, perm ? perm : 0644);
        in->files++;
    } else return 0;                                           /* dispositivos e fifos: o Installer da Apple também pula */
    if (in->as_root) { if (lchown(dst, e->uid, e->gid) < 0) {} }
    if (type != 0120000u) { struct timeval tv[2] = {{e->mtime, 0}, {e->mtime, 0}}; utimes(dst, tv); }
    if (in->paths) strv_add(in->paths, nm);
    return 0;
}
static int extract_cpio_member(pkg_t *p, const char *member, const char *root, inst_t *in) {
    u64 n, cn;
    u8 *raw;
    if (p->bundle) {                                           /* a pasta: o arquivo pedido já é o caminho */
        raw = read_file(member, &n);
        if (!raw) return 1;
    } else {
        xent_t *e = xar_find(&p->x, member);
        if (!e) return 1;                                      /* não existe (pacote só de scripts, por exemplo) */
        raw = xar_read(&p->x, e, &n);
        if (!raw) return -1;
    }
    u8 *cp = unwrap(raw, n, &cn);
    free(raw);
    if (!cp) return PKERR("não foi possível descomprimir %s", member);
    in->root = root;
    int r = cpio_each(cp, cn, put_entry, in);
    free(cp);
    return r;
}
/* roda um script do pacote com o ambiente do Installer da Apple */
static int run_script(const char *script, const char *pkgpath, const char *dst, const char *vol, const char *tmp, const char *name) {
    struct stat st;
    if (stat(script, &st) < 0) return 0;
    chmod(script, st.st_mode | 0111);
    char *argv[] = {(char *)script, (char *)pkgpath, (char *)dst, (char *)vol, (char *)vol, 0};
    char e[16][1200];
    int k = 0;
    snprintf(e[k++], sizeof e[0], "PACKAGE_PATH=%s", pkgpath);
    snprintf(e[k++], sizeof e[0], "DSTROOT=%s", dst);
    snprintf(e[k++], sizeof e[0], "DSTVOLUME=%s", vol);
    snprintf(e[k++], sizeof e[0], "INSTALLER_TEMP=%s", tmp);
    snprintf(e[k++], sizeof e[0], "TMPDIR=%s", tmp);
    snprintf(e[k++], sizeof e[0], "SCRIPT_NAME=%s", name);
    snprintf(e[k++], sizeof e[0], "COMMAND_LINE_INSTALL=1");
    snprintf(e[k++], sizeof e[0], "INSTALL_PKG_SESSION_ID=mos.%d", (int)getpid());
    snprintf(e[k++], sizeof e[0], "PATH=/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/opt/homebrew/bin");
    const char *h = getenv("HOME"), *u = getenv("USER");
    snprintf(e[k++], sizeof e[0], "HOME=%s", h ? h : "/Users/Visitante");
    snprintf(e[k++], sizeof e[0], "USER=%s", u ? u : "visitante");
    snprintf(e[k++], sizeof e[0], "LOGNAME=%s", u ? u : "visitante");
    char *envp[17];
    for (int i = 0; i < k; i++) envp[i] = e[i];
    envp[k] = 0;
    char cwd[1024];
    if (!getcwd(cwd, sizeof cwd)) cwd[0] = 0;
    char sdir[1024];
    snprintf(sdir, sizeof sdir, "%s", script);
    char *sl = strrchr(sdir, '/');
    if (sl) *sl = 0;
    if (chdir(sdir) < 0) {}
    pid_t pid;
    int r = posix_spawn(&pid, script, 0, 0, argv, envp);
    if (cwd[0] && chdir(cwd) < 0) {}
    if (r) return PKERR("não foi possível rodar %s (%s)", name, strerror(r));
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    if (!WIFEXITED(status) || WEXITSTATUS(status)) return PKERR("o script %s falhou (código %d)", name, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    return 0;
}

typedef struct { int nfiles, ndirs, nlinks, scripts; } inst_result_t;
/* instala os componentes escolhidos em vol ("/" ou um volume) */
static int pkg_install(pkg_t *p, const char *vol, inst_result_t *res, void (*log)(const char *msg)) {
    char tmp[1024];
    memset(res, 0, sizeof *res);
    snprintf(tmp, sizeof tmp, "%s/PKInstallSandbox.%d", getenv("TMPDIR") && getenv("TMPDIR")[0] ? getenv("TMPDIR") : "/tmp", (int)getpid());
    if (mkdirs(tmp, 0700) < 0) return PKERR("não foi possível criar %s", tmp);
    int as_root = geteuid() == 0;
    for (int i = 0; i < p->nc; i++) {
        comp_t *c = &p->c[i];
        if (!c->selected) continue;
        char dst[2048], m[700], sdir[1300], msg[1600];
        path_join(dst, sizeof dst, vol, c->location);
        if (mkdirs(dst, 0755) < 0) return PKERR("não foi possível criar %s", dst);
        snprintf(msg, sizeof msg, "Instalando %s %s em %s", c->id, c->version, dst);
        if (log) log(msg);
        /* scripts */
        snprintf(sdir, sizeof sdir, "%s/Scripts/%s", tmp, c->id);
        snprintf(m, sizeof m, "%s%sScripts", c->dir, c->dir[0] ? "/" : "");
        inst_t si = {0};
        int has_scripts;
        if (p->bundle) {                                       /* scripts soltos em Contents/Resources (pre/postflight também) */
            snprintf(sdir, sizeof sdir, "%s/Contents/Resources", c->dir);
            struct stat rs;
            has_scripts = stat(sdir, &rs) == 0;
        } else has_scripts = extract_cpio_member(p, m, sdir, &si) == 0;
        if (has_scripts && mkdirs(sdir, 0755) < 0) {}
        char pkgpath[1300];
        if (p->product && c->dir[0]) snprintf(pkgpath, sizeof pkgpath, "%s#%s", p->path, c->dir); else snprintf(pkgpath, sizeof pkgpath, "%s", p->path);
        if (has_scripts) {
            char sc[1600];
            path_join(sc, sizeof sc, sdir, c->pre[0] ? c->pre : "preinstall");
            if (pk_verbose && log) { snprintf(msg, sizeof msg, "preinstall de %s", c->id); log(msg); }
            if (p->bundle) {                                   /* formato antigo: preflight antes do preinstall */
                char pf[1600];
                path_join(pf, sizeof pf, sdir, "preflight");
                if (run_script(pf, c->dir, dst, vol, tmp, "preflight") < 0) return -1;
            }
            if (run_script(sc, p->bundle ? c->dir : pkgpath, dst, vol, tmp, "preinstall") < 0) return -1;
            struct stat st;
            if (stat(sc, &st) == 0) res->scripts++;
        }
        /* carga */
        if (p->bundle) snprintf(m, sizeof m, "%s/Contents/Archive.pax.gz", c->dir);
        else snprintf(m, sizeof m, "%s%sPayload", c->dir, c->dir[0] ? "/" : "");
        inst_t in = {0};
        in.as_root = as_root;
        int r = extract_cpio_member(p, m, dst, &in);
        if (r < 0) return -1;
        res->nfiles += in.files; res->ndirs += in.dirs; res->nlinks += in.links;
        if (has_scripts) {
            char sc[1600];
            path_join(sc, sizeof sc, sdir, c->post[0] ? c->post : "postinstall");
            if (pk_verbose && log) { snprintf(msg, sizeof msg, "postinstall de %s", c->id); log(msg); }
            if (run_script(sc, p->bundle ? c->dir : pkgpath, dst, vol, tmp, "postinstall") < 0) return -1;
            if (p->bundle) {                                   /* e postflight no fim */
                char pf[1600];
                path_join(pf, sizeof pf, sdir, "postflight");
                if (run_script(pf, c->dir, dst, vol, tmp, "postflight") < 0) return -1;
            }
            struct stat st;
            if (stat(sc, &st) == 0) res->scripts++;
        }
        /* recibo */
        receipt_t rc;
        memset(&rc, 0, sizeof rc);
        snprintf(rc.id, sizeof rc.id, "%s", c->id);
        snprintf(rc.version, sizeof rc.version, "%s", c->version);
        snprintf(rc.location, sizeof rc.location, "%s", c->location);
        snprintf(rc.file, sizeof rc.file, "%s", p->name);
        snprintf(rc.process, sizeof rc.process, "installer");
        rc.date = (long)time(0);
        u64 bn = 0;
        u8 *bom = 0;
        if (p->bundle) { snprintf(m, sizeof m, "%s/Contents/Archive.bom", c->dir); bom = read_file(m, &bn); }
        else {
            snprintf(m, sizeof m, "%s%sBom", c->dir, c->dir[0] ? "/" : "");
            xent_t *be = xar_find(&p->x, m);
            bom = be ? xar_read(&p->x, be, &bn) : 0;
        }
        int w = receipt_write(vol, &rc, bom, bn);
        free(bom);
        if (w < 0) return -1;
    }
    return 0;
}
#endif
