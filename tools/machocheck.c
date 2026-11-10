/*
 * mOS - machocheck: mede o que falta para um .app do macOS rodar no mOS.
 *
 *   machocheck [--disco DIR]... <libSystem.mos> <Pasta.app> [relatorio.txt]
 *
 * Varre o bundle inteiro (executavel + frameworks/dylibs embutidos), le os imports de cada
 * imagem Mach-O x86_64 (chained fixups ou dyld_info) e os exports (export trie), e resolve
 * cada import como o mOS resolveria: primeiro nas imagens do proprio bundle, depois no
 * libSystem.mos (mesma regra de nomes do kernel/compat/macho.c: tira o "_" e a variante "$...").
 * --disco aponta a libc++, as libswift*, a libunwind, a libz e o CryptoKit do build ou da
 * imagem (um diretorio vasculhado, ou o arquivo em si). Os exports entram na resolucao, como
 * o carregador le /usr/lib/libc++.1.dylib, /usr/lib/swift/libswift*.dylib, /usr/lib/libz.1.dylib
 * e /System/Library/Frameworks/CryptoKit.framework/Versions/A/CryptoKit do disco, e como o
 * libunwind entra no libSystem quando os objetos de build/libunwind-x86 sao ligados.
 * A libz nao e reimplementada aqui: vale a dylib do build-zlib.ps1. O CryptoKit vale o
 * framework compilado de userland/frameworks/CryptoKit, nao a trie de tools/ckhost/mkexports.py.
 * Imprime um resumo por biblioteca e grava os ausentes separados em sistema, libc++, Swift
 * e unwind.
 * Compilar: zig cc -O2 -target x86_64-windows-gnu tools/machocheck.c -o build/machocheck.exe
 *           gcc -O2 -o machocheck tools/machocheck.c
 *           clang -O2 -o machocheck tools/machocheck.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64; typedef int64_t i64; typedef int32_t i32;
static u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u64 rd64(const u8 *p) { return (u64)rd32(p) | (u64)rd32(p + 4) << 32; }
static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u64 be64(const u8 *p) { return (u64)be32(p) << 32 | be32(p + 4); }
static u64 uleb(const u8 **p, const u8 *e) { u64 v = 0; int s = 0; while (*p < e) { u8 b = *(*p)++; if (s < 64) v |= (u64)(b & 0x7F) << s; s += 7; if (!(b & 0x80)) break; } return v; }
static i64 sleb(const u8 **p, const u8 *e) { i64 v = 0; int s = 0; u8 b = 0; while (*p < e) { b = *(*p)++; if (s < 64) v |= (i64)(b & 0x7F) << s; s += 7; if (!(b & 0x80)) break; } if (s < 64 && (b & 0x40)) v |= -((i64)1 << s); return v; }

/* ---------------------------------------------------------------- tabela de strings (hash) */
typedef struct { char **k; u32 n, cap; } set_t;
static u32 hstr(const char *s) { u32 h = 2166136261u; while (*s) h = (h ^ (u8)*s++) * 16777619u; return h; }
static int set_has(set_t *s, const char *k) {
    if (!s->cap) return 0;
    for (u32 i = hstr(k) & (s->cap - 1);; i = (i + 1) & (s->cap - 1)) { if (!s->k[i]) return 0; if (!strcmp(s->k[i], k)) return 1; }
}
static void set_add(set_t *s, const char *k) {
    if ((s->n + 1) * 2 > s->cap) {
        u32 nc = s->cap ? s->cap * 2 : 1024; char **nk = calloc(nc, sizeof(char *));
        for (u32 i = 0; i < s->cap; i++) if (s->k[i]) { u32 j = hstr(s->k[i]) & (nc - 1); while (nk[j]) j = (j + 1) & (nc - 1); nk[j] = s->k[i]; }
        free(s->k); s->k = nk; s->cap = nc;
    }
    u32 i = hstr(k) & (s->cap - 1);
    while (s->k[i]) { if (!strcmp(s->k[i], k)) return; i = (i + 1) & (s->cap - 1); }
    s->k[i] = strdup(k); s->n++;
}

/* ---------------------------------------------------------------- imagens */
typedef struct { char *name; int lib; int weak; } imp_t;   /* lib: ordinal (1..n), 0 self, <0 especial */
typedef struct {
    char path[1024];
    u32 ftype;
    char *install;            /* LC_ID_DYLIB */
    char *dylibs[512]; int dweak[512]; int nd;
    imp_t *imps; u32 nimp, capimp;
    int has_tlv_init, has_main, chained;
    u64 size;
} img_t;
static img_t *imgs; static int nimgs;
static set_t bundle_exports, libsys;

static void add_imp(img_t *im, const char *name, int lib, int weak) {
    if (im->nimp == im->capimp) { im->capimp = im->capimp ? im->capimp * 2 : 256; im->imps = realloc(im->imps, im->capimp * sizeof(imp_t)); }
    im->imps[im->nimp++] = (imp_t){strdup(name), lib, weak};
}

static void walk_trie(const u8 *base, const u8 *end, const u8 *node, char *buf, int len, int depth) {
    if (node >= end || depth > 512) return;
    const u8 *p = node;
    u64 tsz = uleb(&p, end);
    if (tsz) { buf[len] = 0; set_add(&bundle_exports, buf); }
    p += tsz;
    if (p >= end) return;
    u8 nch = *p++;
    for (u8 i = 0; i < nch && p < end; i++) {
        int l = len;
        while (p < end && *p && l < 4000) buf[l++] = (char)*p++;
        p++;
        u64 off = uleb(&p, end);
        walk_trie(base, end, base + off, buf, l, depth + 1);
    }
}

static void bind_names(img_t *im, const u8 *p, const u8 *end, int lazy) {
    int ord = 0, weak = 0; const char *sym = "";
    while (p < end) {
        u8 b = *p++, op = b & 0xF0, imm = b & 0x0F;
        switch (op) {
        case 0x00: if (!lazy) return; break;
        case 0x10: ord = imm; break;
        case 0x20: ord = (int)uleb(&p, end); break;
        case 0x30: ord = imm ? (int)(signed char)(0xF0 | imm) : 0; break;
        case 0x40: sym = (const char *)p; weak = imm & 1; while (p < end && *p) p++; p++; break;
        case 0x50: break;
        case 0x60: sleb(&p, end); break;
        case 0x70: uleb(&p, end); break;
        case 0x80: uleb(&p, end); break;
        case 0x90: case 0xB0: add_imp(im, sym, ord, weak); break;
        case 0xA0: uleb(&p, end); add_imp(im, sym, ord, weak); break;
        case 0xC0: uleb(&p, end); uleb(&p, end); add_imp(im, sym, ord, weak); break;
        default: return;
        }
    }
}

static void parse_macho(const char *path, const u8 *f, u64 flen) {
    const u8 *m = NULL; u64 ml = 0;
    u32 mg = flen >= 8 ? be32(f) : 0;
    if (mg == 0xCAFEBABE || mg == 0xCAFEBABF) {
        u32 n = be32(f + 4); int b64 = mg == 0xCAFEBABF;
        for (u32 i = 0; i < n && i < 32; i++) {
            const u8 *a = f + 8 + i * (b64 ? 32 : 20);
            if ((u64)(a - f) + 20 > flen) break;
            u64 off = b64 ? be64(a + 8) : be32(a + 8), sz = b64 ? be64(a + 16) : be32(a + 12);
            if (be32(a) == 0x01000007 && off + sz <= flen) { m = f + off; ml = sz; }
        }
    } else if (flen >= 32 && rd32(f) == 0xFEEDFACF) { m = f; ml = flen; }
    if (!m || ml < 32 || rd32(m) != 0xFEEDFACF || rd32(m + 4) != 0x01000007) return;
    img_t *im = &imgs[nimgs++];
    memset(im, 0, sizeof *im);
    snprintf(im->path, sizeof im->path, "%s", path);
    im->ftype = rd32(m + 12); im->size = ml;
    u32 ncmds = rd32(m + 16);
    const u8 *c = m + 32, *cend = m + 32 + rd32(m + 20);
    const u8 *trie = NULL; u32 triesz = 0;
    const u8 *dinfo = NULL, *chained = NULL;
    for (u32 i = 0; i < ncmds && c + 8 <= cend; i++) {
        u32 cmd = rd32(c), csz = rd32(c + 4);
        if (csz < 8) break;
        switch (cmd) {
        case 0x19: { u32 ns = rd32(c + 64); for (u32 k = 0; k < ns && 72 + (k + 1) * 80 <= csz; k++) if ((rd32(c + 72 + k * 80 + 64) & 0xFF) == 0x15) im->has_tlv_init = 1; break; }
        case 0x0D: im->install = strdup((const char *)c + rd32(c + 8)); break;
        case 0x0C: case 0x80000018: case 0x8000001F: case 0x20: case 0x80000023:
            if (im->nd < 512) { im->dweak[im->nd] = cmd == 0x80000018; im->dylibs[im->nd++] = strdup((const char *)c + rd32(c + 8)); } break;
        case 0x22: case 0x80000022: dinfo = c; break;
        case 0x80000034: chained = c; break;
        case 0x80000033: if (rd32(c + 8) + (u64)rd32(c + 12) <= ml) { trie = m + rd32(c + 8); triesz = rd32(c + 12); } break;
        case 0x80000028: im->has_main = 1; break;
        }
        c += csz;
    }
    if (dinfo) {
        u32 eo = rd32(dinfo + 40), es = rd32(dinfo + 44);
        if (es && eo + (u64)es <= ml) { trie = m + eo; triesz = es; }
        for (int part = 1; part <= 3; part++) {
            u32 o = rd32(dinfo + 8 + part * 8), s = rd32(dinfo + 12 + part * 8);
            if (part == 2) continue;                  /* weak binds: so sobrescrevem, nao exigem */
            if (s && o + (u64)s <= ml) bind_names(im, m + o, m + o + s, part == 3);
        }
    }
    if (chained) {
        im->chained = 1;
        u32 off = rd32(chained + 8), sz = rd32(chained + 12);
        if (off + (u64)sz <= ml && sz >= 28) {
            const u8 *h = m + off, *he = h + sz;
            u32 io = rd32(h + 8), so = rd32(h + 12), cnt = rd32(h + 16), fmt = rd32(h + 20);
            u32 isz = fmt == 1 ? 4 : fmt == 2 ? 8 : 16;
            for (u32 i = 0; i < cnt; i++) {
                const u8 *e = h + io + (u64)i * isz;
                if (e + isz > he) break;
                int lib, weak; u32 no;
                if (fmt == 3) { lib = (int16_t)rd16(e); weak = rd16(e + 2) & 1; no = rd32(e + 4); }
                else { u32 v = rd32(e); lib = (int)(v & 0xFF); if (lib >= 0xF0) lib = (int)(int8_t)lib; weak = (v >> 8) & 1; no = v >> 9; }
                const char *nm = (const char *)h + so + no;
                if ((const u8 *)nm < he) add_imp(im, nm, lib, weak);
            }
        }
    }
    if (trie) { static char buf[4096]; walk_trie(trie, trie + triesz, trie, buf, 0, 0); }
}

static void scan(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char p[1024]; snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(p, &st)) continue;
        if (S_ISDIR(st.st_mode)) { scan(p); continue; }
        if (st.st_size < 32) continue;
        FILE *fp = fopen(p, "rb"); if (!fp) continue;
        u8 hdr[4]; if (fread(hdr, 1, 4, fp) != 4) { fclose(fp); continue; }
        u32 be = be32(hdr), le = rd32(hdr);
        if (be != 0xCAFEBABE && be != 0xCAFEBABF && le != 0xFEEDFACF) { fclose(fp); continue; }
        u8 *buf = malloc((size_t)st.st_size);
        fseek(fp, 0, SEEK_SET);
        if (buf && fread(buf, 1, (size_t)st.st_size, fp) == (size_t)st.st_size) {
            if (nimgs < 4096) parse_macho(p, buf, (u64)st.st_size);
        }
        free(buf); fclose(fp);
    }
    closedir(d);
}

static int load_libsystem(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *e = malloc((size_t)n); if (fread(e, 1, (size_t)n, f) != (size_t)n) { fclose(f); return 0; } fclose(f);
    if (memcmp(e, "\x7F" "ELF", 4)) return 0;
    u64 shoff = rd64(e + 40); u16 shes = rd16(e + 58), shn = rd16(e + 60);
    for (u16 i = 0; i < shn; i++) {
        const u8 *sh = e + shoff + (u64)i * shes;
        if (rd32(sh + 4) != 2) continue;
        u64 off = rd64(sh + 24), size = rd64(sh + 32), es = rd64(sh + 56);
        u64 stroff = rd64(e + shoff + (u64)rd32(sh + 40) * shes + 24);
        for (u64 k = 0; k < size / es; k++) {
            const u8 *s = e + off + k * es;
            if ((s[4] >> 4) != 1 || !rd16(s + 6) || (s[4] & 15) > 2) continue;
            set_add(&libsys, (const char *)e + stroff + rd32(s));
        }
    }
    return libsys.n > 0;
}

/* ---------------------------------------------------------------- bibliotecas que o carregador le do disco */
static set_t disk_cxx, disk_swift, disk_unwind, disk_z, disk_ck, swift_lidas;
static u32 cxx_files, swift_files, uw_files, z_files, ck_files;
static u32 cxx_syms, swift_syms, uw_syms, z_syms, ck_syms;
static int disco_faltou;
static dev_t seen_dev[8192];
static ino_t seen_ino[8192];
static int nseen;

static int sufixo(const char *s, const char *suf) {
    size_t n = strlen(s), m = strlen(suf);
    return n >= m && !memcmp(s + n - m, suf, m);
}

/* 1 = libc++, 2 = Swift, 3 = libunwind, 4 = libz, 5 = CryptoKit, 0 = outro */
static int kind_de(const char *path) {
    const char *b = strrchr(path, '/');
    b = b ? b + 1 : path;
    if (!strcmp(b, "libc++.1.dylib") || !strcmp(b, "libc++abi.dylib")) return 1;
    if (!strncmp(b, "libswift", 8) && sufixo(b, ".dylib")) return 2;
    if ((sufixo(b, ".o") || sufixo(b, ".a") || sufixo(b, ".dylib")) &&
        (strstr(b, "libunwind") || !strncmp(b, "Unwind", 6))) return 3;
    /* /usr/lib/libz.1.dylib da imagem (tools/build-zlib.ps1). Nao e simbolo do libSystem. */
    if (!strcmp(b, "libz.1.dylib") || !strcmp(b, "libz.dylib")) return 4;
    /* binario do framework, install name .../CryptoKit.framework/Versions/A/CryptoKit */
    if (!strcmp(b, "CryptoKit")) return 5;
    return 0;
}

static u8 *ler_arquivo(const char *path, u64 *out_n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n <= 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    u8 *b = malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *out_n = (u64)n;
    return b;
}

static int fatia_x64(const u8 *f, u64 flen, const u8 **m, u64 *ml) {
    *m = NULL; *ml = 0;
    if (flen < 8) return 0;
    u32 mg = be32(f);
    if (mg == 0xCAFEBABE || mg == 0xCAFEBABF) {
        u32 nar = be32(f + 4);
        int b64 = mg == 0xCAFEBABF;
        for (u32 i = 0; i < nar && i < 64; i++) {
            const u8 *a = f + 8 + (u64)i * (b64 ? 32 : 20);
            if ((u64)(a - f) + (b64 ? 32 : 20) > flen) break;
            u64 off = b64 ? be64(a + 8) : be32(a + 8);
            u64 sz = b64 ? be64(a + 16) : be32(a + 12);
            if (be32(a) == 0x01000007 && sz >= 32 && off + sz <= flen) { *m = f + off; *ml = sz; return 1; }
        }
        return 0;
    }
    if (flen >= 32 && rd32(f) == 0xFEEDFACF && rd32(f + 4) == 0x01000007) { *m = f; *ml = flen; return 1; }
    return 0;
}

static void trie_para(set_t *dest, u32 *cnt, const u8 *base, const u8 *end, const u8 *node, char *buf, int len, int depth) {
    if (node < base || node >= end || depth > 512) return;
    const u8 *p = node;
    u64 tsz = uleb(&p, end);
    if (tsz && len > 0) {
        buf[len] = 0;
        if (!set_has(dest, buf)) { set_add(dest, buf); (*cnt)++; }
    }
    if ((u64)(end - p) < tsz) return;
    p += tsz;
    if (p >= end) return;
    u8 nch = *p++;
    for (u8 i = 0; i < nch && p < end; i++) {
        int l = len;
        while (p < end && *p && l < 4000) buf[l++] = (char)*p++;
        if (p < end) p++;
        u64 off = uleb(&p, end);
        if (off < (u64)(end - base)) trie_para(dest, cnt, base, end, base + off, buf, l, depth + 1);
    }
}

static int nome_terminado(const u8 *s, const u8 *end) {
    while (s < end) { if (!*s) return 1; s++; }
    return 0;
}

/* symtab Mach-O quando a imagem nao tem export trie (objeto .o, ou dylib antigo) */
static void nlist_para(set_t *dest, u32 *cnt, const u8 *m, u64 ml, u32 symoff, u32 nsyms, u32 stroff, u32 strsize) {
    if (!nsyms || !strsize) return;
    if ((u64)symoff + (u64)nsyms * 16 > ml || (u64)stroff + strsize > ml) return;
    const u8 *str = m + stroff;
    for (u32 i = 0; i < nsyms; i++) {
        const u8 *s = m + symoff + (u64)i * 16;
        u32 strx = rd32(s);
        u8 type = s[4];
        if (strx >= strsize || !nome_terminado(str + strx, str + strsize)) continue;
        if (type & 0xE0) continue;                 /* N_STAB */
        if (type & 0x10) continue;                 /* N_PEXT: nao sai na tabela de exportacao */
        if (!(type & 0x01)) continue;              /* N_EXT */
        u8 kind = type & 0x0E;
        if (kind != 0x0E && kind != 0x02) continue; /* N_SECT ou N_ABS */
        const char *nm = (const char *)str + strx;
        if (!nm[0]) continue;
        if (!set_has(dest, nm)) { set_add(dest, nm); (*cnt)++; }
    }
}

/* O ELF do libunwind (zig -target *-apple-*-elf) pode gravar o nome C ou o nome Mach-O.
 * Guarda as duas grafias, e tambem com um "_" a mais, para casar o import do app. */
static void poe_formas(set_t *dest, u32 *cnt, const char *nm) {
    if (!nm || !nm[0] || nm[0] == '.') return;
    if (!set_has(dest, nm)) { set_add(dest, nm); (*cnt)++; }
    if (nm[0] == '_' && nm[1] && !set_has(dest, nm + 1)) set_add(dest, nm + 1);
    if (strlen(nm) < 1100) {
        char buf[1200];
        buf[0] = '_';
        memcpy(buf + 1, nm, strlen(nm) + 1);
        if (!set_has(dest, buf)) set_add(dest, buf);
    }
}

static int elf_para(set_t *dest, u32 *cnt, const u8 *e, u64 n) {
    if (n < 64 || memcmp(e, "\x7F" "ELF", 4) || e[4] != 2 || e[5] != 1) return 0;
    u64 shoff = rd64(e + 40);
    u16 shes = rd16(e + 58), shn = rd16(e + 60);
    if (!shes || shoff + (u64)shn * shes > n) return 0;
    for (u16 i = 0; i < shn; i++) {
        const u8 *sh = e + shoff + (u64)i * shes;
        u32 typ = rd32(sh + 4);
        if (typ != 2 && typ != 11) continue;       /* SHT_SYMTAB ou SHT_DYNSYM */
        u64 off = rd64(sh + 24), size = rd64(sh + 32), es = rd64(sh + 56);
        u32 link = rd32(sh + 40);
        if (es < 24 || !size || off + size > n || link >= shn) continue;
        const u8 *strsh = e + shoff + (u64)link * shes;
        u64 stroff = rd64(strsh + 24), strsz = rd64(strsh + 32);
        if (stroff + strsz > n) continue;
        for (u64 k = 0; k < size / es; k++) {
            const u8 *s = e + off + k * es;
            u8 info = s[4], bind = info >> 4, ty = info & 15;
            u16 shndx = rd16(s + 6);
            if (bind != 1 && bind != 2) continue;  /* GLOBAL ou WEAK (aliases unw_*) */
            if (!shndx) continue;
            if (ty > 2) continue;                  /* NOTYPE, OBJECT, FUNC */
            u32 noff = rd32(s);
            if ((u64)noff >= strsz) continue;
            const char *nm = (const char *)e + stroff + noff;
            if (!nome_terminado((const u8 *)nm, e + stroff + strsz)) continue;
            poe_formas(dest, cnt, nm);
        }
    }
    return 1;
}

static int macho_exports(set_t *dest, u32 *cnt, const u8 *f, u64 flen) {
    const u8 *m; u64 ml;
    if (!fatia_x64(f, flen, &m, &ml) || ml < 32) return 0;
    u32 ncmds = rd32(m + 16), cszs = rd32(m + 20);
    if (32 + (u64)cszs > ml) return 0;
    const u8 *c = m + 32, *cend = m + 32 + cszs;
    const u8 *trie = NULL; u32 triesz = 0;
    u32 symoff = 0, nsyms = 0, stroff = 0, strsize = 0, tem_sym = 0;
    for (u32 i = 0; i < ncmds && c + 8 <= cend; i++) {
        u32 cmd = rd32(c), csz = rd32(c + 4);
        if (csz < 8 || c + csz > cend) break;
        if (cmd == 0x80000033 && csz >= 16) {          /* LC_DYLD_EXPORTS_TRIE */
            u32 off = rd32(c + 8), sz = rd32(c + 12);
            if (sz && off + (u64)sz <= ml) { trie = m + off; triesz = sz; }
        } else if ((cmd == 0x22 || cmd == 0x80000022) && csz >= 48) {
            u32 off = rd32(c + 40), sz = rd32(c + 44);
            if (sz && off + (u64)sz <= ml) { trie = m + off; triesz = sz; }
        } else if (cmd == 0x02 && csz >= 24) {         /* LC_SYMTAB */
            symoff = rd32(c + 8); nsyms = rd32(c + 12); stroff = rd32(c + 16); strsize = rd32(c + 20);
            tem_sym = 1;
        }
        c += csz;
    }
    if (trie && triesz) {
        char buf[4096];
        trie_para(dest, cnt, trie, trie + triesz, trie, buf, 0, 0);
        return 1;
    }
    if (tem_sym) { nlist_para(dest, cnt, m, ml, symoff, nsyms, stroff, strsize); return 1; }
    return 0;
}

static void ar_membros(set_t *dest, u32 *cnt, const u8 *b, u64 n) {
    if (n < 8 || memcmp(b, "!<arch>\n", 8)) return;
    u64 o = 8;
    while (o + 60 <= n) {
        char sizeb[16];
        memcpy(sizeb, b + o + 48, 10);
        sizeb[10] = 0;
        char nome[20];
        memcpy(nome, b + o, 16);
        nome[16] = 0;
        u64 sz = strtoull(sizeb, NULL, 10);
        o += 60;
        if (!sz || sz > n - o) break;
        u64 skip = !strncmp(nome, "#1/", 3) ? strtoull(nome + 3, NULL, 10) : 0;
        if (skip < sz) {
            const u8 *m = b + o + skip;
            u64 ml = sz - skip;
            if (ml >= 4 && !memcmp(m, "\x7F" "ELF", 4)) elf_para(dest, cnt, m, ml);
            else macho_exports(dest, cnt, m, ml);
        }
        o += sz;
        if (o & 1) o++;
    }
}

static int carrega_exports(const char *path, set_t *dest, u32 *cnt) {
    u64 n = 0;
    u8 *b = ler_arquivo(path, &n);
    if (!b) { fprintf(stderr, "disco: nao consegui ler %s\n", path); return 0; }
    int ok = 0;
    if (n >= 8 && !memcmp(b, "!<arch>\n", 8)) { ar_membros(dest, cnt, b, n); ok = 1; }
    else if (n >= 4 && !memcmp(b, "\x7F" "ELF", 4)) ok = elf_para(dest, cnt, b, n);
    else ok = macho_exports(dest, cnt, b, n);
    free(b);
    if (!ok) fprintf(stderr, "disco: %s nao e Mach-O x86_64, ELF ou arquivo .a\n", path);
    return ok;
}

static int visto_dir(dev_t dev, ino_t ino) {
    for (int i = 0; i < nseen; i++) if (seen_dev[i] == dev && seen_ino[i] == ino) return 1;
    if (nseen < 8192) { seen_dev[nseen] = dev; seen_ino[nseen++] = ino; return 0; }
    return 1;
}

static void carrega_kind(const char *path, int kind) {
    struct stat st;
    if (stat(path, &st)) { fprintf(stderr, "disco: nao consegui ler %s\n", path); return; }
    if (visto_dir(st.st_dev, st.st_ino)) return;
    set_t *dest = &disk_cxx;
    u32 *cnt = &cxx_syms, *files = &cxx_files;
    if (kind == 2) { dest = &disk_swift; cnt = &swift_syms; files = &swift_files; }
    else if (kind == 3) { dest = &disk_unwind; cnt = &uw_syms; files = &uw_files; }
    else if (kind == 4) { dest = &disk_z; cnt = &z_syms; files = &z_files; }
    else if (kind == 5) { dest = &disk_ck; cnt = &ck_syms; files = &ck_files; }
    if (carrega_exports(path, dest, cnt)) {
        (*files)++;
        if (kind == 2) set_add(&swift_lidas, strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
    }
}

static void anda_disco(const char *dir, int depth) {
    if (depth > 16) return;
    DIR *d = opendir(dir);
    if (!d) { fprintf(stderr, "disco: nao abri %s\n", dir); disco_faltou = 1; return; }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (!strcmp(e->d_name, ".git") || !strcmp(e->d_name, "node_modules")) continue;
        char p[4096];
        int wr = snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        if (wr < 0 || wr >= (int)sizeof p) continue;
        struct stat st;
        if (lstat(p, &st)) continue;
        if (S_ISLNK(st.st_mode) && stat(p, &st)) continue;
        if (S_ISDIR(st.st_mode)) {
            if (visto_dir(st.st_dev, st.st_ino)) continue;
            anda_disco(p, depth + 1);
        } else if (S_ISREG(st.st_mode)) {
            int k = kind_de(p);
            if (k) carrega_kind(p, k);
        }
    }
    closedir(d);
}

static void carrega_disco(const char *path) {
    struct stat st;
    if (stat(path, &st)) { fprintf(stderr, "disco: caminho inexistente: %s\n", path); disco_faltou = 1; return; }
    if (S_ISREG(st.st_mode)) {
        int k = kind_de(path);
        if (!k) { fprintf(stderr, "disco: %s nao e libc++, libswift*, libunwind, libz nem CryptoKit\n", path); disco_faltou = 1; return; }
        carrega_kind(path, k);
        return;
    }
    if (!S_ISDIR(st.st_mode)) { fprintf(stderr, "disco: %s ignorado\n", path); disco_faltou = 1; return; }
    if (!visto_dir(st.st_dev, st.st_ino)) anda_disco(path, 0);
}

static int no_disco(const char *mname) {
    return set_has(&disk_cxx, mname) || set_has(&disk_swift, mname) || set_has(&disk_unwind, mname)
        || set_has(&disk_z, mname) || set_has(&disk_ck, mname);
}

/* mesma regra do ls_lookup do kernel */
static int in_libsystem(const char *mname) {
    if (mname[0] == '_') mname++;
    if (set_has(&libsys, mname)) return 1;
    const char *d = strchr(mname, '$');
    if (!d) return 0;
    char t[1024]; size_t n = (size_t)(d - mname); if (n >= sizeof t) return 0;
    memcpy(t, mname, n); t[n] = 0;
    return set_has(&libsys, t);
}

/* Espelho de libsystem_dylib_ok em kernel/compat/macho.c. O carregador aceita estas
 * bibliotecas e as resolve no libSystem.mos; uma lista mais curta faz o relatório
 * dizer "biblioteca inexistente" para um app que o kernel abre (o Codex CLI liga
 * Security, CFNetwork, SystemConfiguration, CoreServices, IOKit e LocalAuthentication). */
static int sys_ok(const char *n) {
    if (!strcmp(n, "(flat/self)")) return 1;
    if (set_has(&swift_lidas, strrchr(n, '/') ? strrchr(n, '/') + 1 : n)) return 1;
    static const char *ok[] = {
        "/usr/lib/libSystem.B.dylib", "/usr/lib/libSystem.dylib", "/usr/lib/libc.dylib",
        "/usr/lib/libm.dylib", "/usr/lib/libdl.dylib", "/usr/lib/libpthread.dylib",
        "/usr/lib/libinfo.dylib", "/usr/lib/libgcc_s.1.dylib", "/usr/lib/libiconv.2.dylib", "/usr/lib/libiconv.dylib",
        "/usr/lib/libresolv.9.dylib", "/usr/lib/libicucore.A.dylib",
        "/usr/lib/libbz2.1.0.dylib", "/usr/lib/libxar.1.dylib",
        "/usr/lib/libc++.1.dylib", "/usr/lib/libc++abi.dylib",
    };
    static const char *fw[] = {
        "/usr/lib/libobjc.A.dylib", "/System/Library/Frameworks/Foundation.framework/",
        "/System/Library/Frameworks/AppKit.framework/", "/System/Library/Frameworks/Cocoa.framework/",
        "/System/Library/Frameworks/CoreFoundation.framework/", "/System/Library/Frameworks/CoreGraphics.framework/",
        "/System/Library/Frameworks/ApplicationServices.framework/", "/System/Library/Frameworks/QuartzCore.framework/",
        "/System/Library/Frameworks/CoreText.framework/",
        "/System/Library/Frameworks/IOSurface.framework/", "/System/Library/Frameworks/CoreVideo.framework/",
        "/System/Library/Frameworks/Metal.framework/", "/System/Library/Frameworks/MetalKit.framework/",
        "/System/Library/Frameworks/UniformTypeIdentifiers.framework/",
        "/System/Library/Frameworks/Security.framework/", "/System/Library/Frameworks/SystemConfiguration.framework/",
        "/System/Library/Frameworks/CFNetwork.framework/", "/System/Library/Frameworks/CoreServices.framework/",
        "/System/Library/Frameworks/IOKit.framework/", "/System/Library/Frameworks/LocalAuthentication.framework/",
        "/System/Library/Frameworks/LDAP.framework/", "/System/Library/Frameworks/Kerberos.framework/",
        "/System/Library/Frameworks/GSS.framework/",
        "/System/Library/Frameworks/AVFAudio.framework/",
        "/System/Library/Frameworks/AVFoundation.framework/",
        "/System/Library/Frameworks/Accelerate.framework/",
        "/System/Library/Frameworks/Accessibility.framework/",
        "/System/Library/Frameworks/AudioToolbox.framework/",
        "/System/Library/Frameworks/AudioUnit.framework/",
        "/System/Library/Frameworks/AuthenticationServices.framework/",
        "/System/Library/Frameworks/Carbon.framework/",
        "/System/Library/Frameworks/CoreAudio.framework/",
        "/System/Library/Frameworks/CoreBluetooth.framework/",
        "/System/Library/Frameworks/CoreHaptics.framework/",
        "/System/Library/Frameworks/CoreImage.framework/",
        "/System/Library/Frameworks/CoreLocation.framework/",
        "/System/Library/Frameworks/CoreMIDI.framework/",
        "/System/Library/Frameworks/CoreML.framework/",
        "/System/Library/Frameworks/CoreMedia.framework/",
        "/System/Library/Frameworks/CoreMediaIO.framework/",
        "/System/Library/Frameworks/CoreWLAN.framework/",
        "/System/Library/Frameworks/CryptoTokenKit.framework/",
        "/System/Library/Frameworks/DiskArbitration.framework/",
        "/System/Library/Frameworks/ForceFeedback.framework/",
        "/System/Library/Frameworks/GameController.framework/",
        "/System/Library/Frameworks/IOBluetooth.framework/",
        "/System/Library/Frameworks/LocalAuthenticationEmbeddedUI.framework/",
        "/System/Library/Frameworks/MediaAccessibility.framework/",
        "/System/Library/Frameworks/MediaPlayer.framework/",
        "/System/Library/Frameworks/Network.framework/",
        "/System/Library/Frameworks/OpenDirectory.framework/",
        "/System/Library/Frameworks/OpenGL.framework/",
        "/System/Library/Frameworks/Quartz.framework/",
        "/System/Library/Frameworks/QuickLookThumbnailing.framework/",
        "/System/Library/Frameworks/QuickLookUI.framework/",
        "/System/Library/Frameworks/StoreKit.framework/",
        "/System/Library/Frameworks/CoreData.framework/",
        "/System/Library/Frameworks/SafariServices.framework/",
        "/System/Library/Frameworks/ScreenCaptureKit.framework/",
        "/System/Library/Frameworks/SecurityInterface.framework/",
        "/System/Library/Frameworks/ServiceManagement.framework/",
        "/System/Library/Frameworks/UserNotifications.framework/",
        "/System/Library/Frameworks/VideoToolbox.framework/",
        "/System/Library/Frameworks/Vision.framework/",
        "/System/Library/PrivateFrameworks/SkyLight.framework/", "/usr/lib/libsandbox.1.dylib",
        "/usr/lib/libbsm.0.dylib", "/usr/lib/libcups.2.dylib", "/usr/lib/libpmenergy.dylib", "/usr/lib/libpmsample.dylib",
    };
    for (size_t i = 0; i < sizeof ok / sizeof *ok; i++) if (!strcmp(n, ok[i])) return 1;
    for (size_t i = 0; i < sizeof fw / sizeof *fw; i++) if (!strncmp(n, fw[i], strlen(fw[i]))) return 1;
    return !strncmp(n, "/usr/lib/system/", 16);
}
static int is_bundled(const char *n) { return n[0] == '@'; }

static const char *shortname(const char *p) {
    static char out[256];
    if (!strcmp(p, "(flat/self)")) { snprintf(out, sizeof out, "(flat/self)"); return out; }
    const char *fw = strstr(p, ".framework");
    if (fw) { const char *s = fw; while (s > p && s[-1] != '/') s--; snprintf(out, sizeof out, "%.*s.framework", (int)(fw - s), s); return out; }
    const char *b = strrchr(p, '/'); snprintf(out, sizeof out, "%s", b ? b + 1 : p); return out;
}

typedef struct { char name[256]; u32 total, missing; int bundled, sysok; } libstat_t;
static libstat_t ls[1024]; static int nls;
static libstat_t *libstat(const char *path) {
    const char *s = shortname(path);
    for (int i = 0; i < nls; i++) if (!strcmp(ls[i].name, s)) return &ls[i];
    libstat_t *l = &ls[nls++]; memset(l, 0, sizeof *l);
    snprintf(l->name, sizeof l->name, "%s", s); l->bundled = is_bundled(path); l->sysok = sys_ok(path);
    return l;
}

/* --calls <imagem Mach-O> <endereÃ§o>: lista as chamadas "call rel32" nos 256 bytes antes do endereÃ§o
 * e o import ligado ao GOT de cada stub (jmp [rip+X]), percorrendo as cadeias de chained fixups. */
static int calls_mode(const char *path, u64 addr) {
    FILE *f = fopen(path, "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *b = malloc((size_t)n);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) return 1;
    fclose(f);
    const u8 *m = b;
    if (rd32(m) != 0xFEEDFACF) { fprintf(stderr, "use um Mach-O x86_64 fino\n"); return 1; }
    u32 ncmds = rd32(m + 16);
    const u8 *c = m + 32, *chained = 0;
    u64 segaddr[16], segoff[16]; int nseg = 0;
    for (u32 i = 0; i < ncmds; i++) {
        u32 cmd = rd32(c), csz = rd32(c + 4);
        if (cmd == 0x19 && nseg < 16) { segaddr[nseg] = rd64(c + 24); segoff[nseg++] = rd64(c + 40); }
        if (cmd == 0x80000034) chained = c;
        c += csz;
    }
    /* mapa slot -> import (sÃ³ binds) */
    u64 *slots = calloc(1 << 20, 8); u32 *ords = calloc(1 << 20, 4); u32 nslots = 0;
    const u8 *h = m + rd32(chained + 8);
    u32 so = rd32(h + 4), io = rd32(h + 8), sy = rd32(h + 12), fmt = rd32(h + 20);
    const u8 *si = h + so;
    u32 segc = rd32(si);
    for (u32 s = 0; s < segc; s++) {
        u32 o = rd32(si + 4 + s * 4);
        if (!o) continue;
        const u8 *ss = si + o;
        u16 ps = rd16(ss + 4), pc = rd16(ss + 20);
        u64 seg_off = rd64(ss + 8);
        for (u16 p = 0; p < pc; p++) {
            u16 st = rd16(ss + 22 + p * 2);
            if (st == 0xFFFF) continue;
            u64 va = seg_off + (u64)p * ps + st;   /* vmaddr relativo ao cabeÃ§alho (dylib: = offset no arquivo) */
            for (int g = 0; g < 100000; g++) {
                u64 off = va;
                for (int k = 0; k < nseg; k++) if (va >= segaddr[k] && (k + 1 == nseg || va < segaddr[k + 1])) off = va - segaddr[k] + segoff[k];
                u64 v = rd64(m + off);
                if ((v >> 63) && nslots < (1 << 20)) { slots[nslots] = va; ords[nslots++] = (u32)(v & 0xFFFFFF); }
                u64 next = (v >> 51) & 0xFFF;
                if (!next) break;
                va += next * 4;
            }
        }
    }
    (void)fmt;
    for (u64 a = addr - 256; a < addr; a++) {
        if (m[a] != 0xE8) continue;
        i32 rel = (i32)rd32(m + a + 1);
        u64 t = a + 5 + (i64)rel;
        if (t >= (u64)n || m[t] != 0xFF || m[t + 1] != 0x25) continue;      /* stub: jmp [rip+X] */
        u64 got = t + 6 + (i64)(i32)rd32(m + t + 2);
        const char *name = "?";
        for (u32 i = 0; i < nslots; i++)
            if (slots[i] == got) { u32 v = rd32(h + io + ords[i] * 4); name = (const char *)h + sy + (v >> 9); break; }
        printf("%llx: call %llx (stub) -> GOT %llx = %s\n", (unsigned long long)a, (unsigned long long)t, (unsigned long long)got, name);
    }
    return 0;
}

/* --sym <libSystem.mos> <endereÃ§o>: funÃ§Ã£o do libSystem (inclusive static) que contÃ©m o endereÃ§o */
static int sym_mode(const char *path, u64 addr) {
    FILE *f = fopen(path, "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *e = malloc((size_t)n);
    if (fread(e, 1, (size_t)n, f) != (size_t)n) return 1;
    fclose(f);
    u64 bias = rd16(e + 16) == 3 ? 0x400000100000ULL : 0, shoff = rd64(e + 40), best = 0;
    const char *bn = "?";
    u16 shes = rd16(e + 58), shn = rd16(e + 60);
    for (u16 i = 0; i < shn; i++) {
        const u8 *sh = e + shoff + (u64)i * shes;
        if (rd32(sh + 4) != 2) continue;
        u64 off = rd64(sh + 24), size = rd64(sh + 32), es = rd64(sh + 56);
        u64 stroff = rd64(e + shoff + (u64)rd32(sh + 40) * shes + 24);
        for (u64 k = 0; k < size / es; k++) {
            const u8 *s = e + off + k * es;
            if ((s[4] & 15) != 2 || !rd16(s + 6)) continue;
            u64 a = rd64(s + 8) + bias;
            if (a <= addr && a > best) { best = a; bn = (const char *)e + stroff + rd32(s); }
        }
    }
    printf("%s+0x%llx\n", bn, (unsigned long long)(addr - best));
    return 0;
}

/* --refs <imagem> <endereÃ§o>: instruÃ§Ãµes "lea reg, [rip+X]" / "mov reg, [rip+X]" que apontam para o
 * endereÃ§o (ex.: um descritor TLV), com os 24 bytes seguintes (para ver se Ã© leitura ou gravaÃ§Ã£o) */
static int refs_mode(const char *path, u64 target) {
    FILE *f = fopen(path, "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *m = malloc((size_t)n);
    if (fread(m, 1, (size_t)n, f) != (size_t)n) return 1;
    fclose(f);
    int found = 0;
    for (long i = 0; i + 7 < n && found < 20000; i++) {
        if ((m[i] & 0xF8) != 0x48 || (m[i + 1] != 0x8D && m[i + 1] != 0x8B) || (m[i + 2] & 0xC7) != 0x05) continue;
        u64 t = (u64)(i + 7) + (i64)(i32)rd32(m + i + 3);
        if (t != target) continue;
        printf("%lx:", i);
        for (int k = 0; k < 24 && i + 7 + k < n; k++) printf(" %02X", m[i + 7 + k]);
        printf("\n");
        found++;
    }
    printf("%d referÃªncia(s)\n", found);
    return 0;
}

/* origem de um ausente: unwind pelo nome (_Unwind ou unw_), libc++ e Swift pela biblioteca
 * que importou (ou, no bind flat, pelo mangling Itanium __Z ou Swift $). O resto e sistema. */
static set_t missing_nomes;
static const char *classe_de(const char *libpath, const char *sym) {
    const char *s = sym ? sym : "";
    if (s[0] == '_') s++;
    if (!strncmp(s, "_Unwind", 7) || !strncmp(s, "Unwind_", 7) || !strncmp(s, "unw_", 4)) return "unwind";
    if (!strcmp(libpath, "(flat/self)")) {
        const char *z = (sym && sym[0] == '_') ? sym + 1 : s;
        if (z[0] == '_' && z[1] == 'Z') return "libc++";
        if (z[0] == '$') return "Swift";
    }
    const char *b = strrchr(libpath, '/');
    b = b ? b + 1 : libpath;
    if (!strcmp(b, "libc++.1.dylib") || !strcmp(b, "libc++abi.dylib")) return "libc++";
    if (!strncmp(b, "libswift", 8)) return "Swift";
    return "sistema";
}

typedef struct { const char *cat; char lib[256]; char *name; } mrec_t;
static mrec_t *mrecs;
static u32 nmrec, cmrec, n_sis, n_cxx, n_sw, n_uw;

static void anota_ausente(const char *libpath, const char *name) {
    if (set_has(&missing_nomes, name)) return;
    set_add(&missing_nomes, name);
    const char *cat = classe_de(libpath, name);
    if (!strcmp(cat, "libc++")) n_cxx++;
    else if (!strcmp(cat, "Swift")) n_sw++;
    else if (!strcmp(cat, "unwind")) n_uw++;
    else { cat = "sistema"; n_sis++; }
    if (nmrec == cmrec) {
        u32 nc = cmrec ? cmrec * 2 : 256;
        mrec_t *nr = realloc(mrecs, nc * sizeof *nr);
        if (!nr) return;
        mrecs = nr;
        cmrec = nc;
    }
    char *copia = strdup(name);
    if (!copia) return;
    mrec_t *r = &mrecs[nmrec++];
    r->cat = cat;
    snprintf(r->lib, sizeof r->lib, "%s", shortname(libpath));
    r->name = copia;
}

static void grava_cat(FILE *rep, const char *cat, u32 n) {
    fprintf(rep, "# ausentes %s: %u\n", cat, n);
    for (u32 i = 0; i < nmrec; i++) if (mrecs[i].cat == cat || !strcmp(mrecs[i].cat, cat))
        fprintf(rep, "%s\t%s\n", mrecs[i].lib, mrecs[i].name);
}

static int uso(void) {
    fprintf(stderr,
            "uso: machocheck [--disco DIR]... <libSystem.mos> <Pasta.app> [relatorio.txt]\n"
            "     machocheck --calls <imagem> <endereco hex>\n"
            "     machocheck --sym <libSystem.mos> <endereco hex>\n"
            "     machocheck --refs <imagem> <endereco hex>\n"
            "--disco: diretorio (ou arquivo) da libc++, das libswift*, da libunwind,\n"
            "         da libz.1.dylib e do CryptoKit.framework do build ou da imagem.\n"
            "         Ex.: build/libcxx, build/swift/objc/dylib, build/libunwind-x86,\n"
            "         build/zlib-x86_64/libz.1.dylib, esp/mOS\n");
    return 2;
}

int main(int argc, char **argv) {
    if (argc == 4 && !strcmp(argv[1], "--refs")) return refs_mode(argv[2], strtoull(argv[3], 0, 16));
    if (argc == 4 && !strcmp(argv[1], "--sym")) return sym_mode(argv[2], strtoull(argv[3], 0, 16));
    if (argc == 4 && !strcmp(argv[1], "--calls")) return calls_mode(argv[2], strtoull(argv[3], 0, 16));
    const char *discos[32];
    int ndisco = 0;
    const char *pos[4];
    int npos = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--disco")) {
            if (i + 1 >= argc || ndisco == 32) return uso();
            discos[ndisco++] = argv[++i];
            continue;
        }
        if (!strcmp(argv[i], "--refs") || !strcmp(argv[i], "--sym") || !strcmp(argv[i], "--calls")) return uso();
        if (npos == 4) return uso();
        pos[npos++] = argv[i];
    }
    if (npos < 2) return uso();
    const char *libpath = pos[0], *app = pos[1], *reppath = npos > 2 ? pos[2] : NULL;
    if (!load_libsystem(libpath)) { fprintf(stderr, "nao consegui ler a tabela de simbolos de %s\n", libpath); return 1; }
    for (int i = 0; i < ndisco; i++) carrega_disco(discos[i]);
    imgs = calloc(4096, sizeof(img_t));
    scan(app);
    FILE *rep = reppath ? fopen(reppath, "w") : NULL;
    if (reppath && !rep) fprintf(stderr, "nao consegui gravar %s\n", reppath);
    printf("libSystem.mos: %u simbolos exportados\n", libsys.n);
    if (ndisco) {
        printf("Disco:");
        for (int i = 0; i < ndisco; i++) printf(" %s", discos[i]);
        printf("\n");
        printf("  libc++: %u arquivo(s), %u simbolos\n", cxx_files, cxx_syms);
        printf("  Swift: %u arquivo(s), %u simbolos\n", swift_files, swift_syms);
        printf("  unwind: %u arquivo(s), %u simbolos\n", uw_files, uw_syms);
        printf("  libz: %u arquivo(s), %u simbolos\n", z_files, z_syms);
        printf("  CryptoKit: %u arquivo(s), %u simbolos\n", ck_files, ck_syms);
    }
    printf("Imagens Mach-O x86_64 no bundle: %d (exports do bundle: %u)\n\n", nimgs, bundle_exports.n);
    set_t sysdylibs = {0};
    u64 tot = 0, miss = 0;
    int tlv = 0;
    size_t applen = strlen(app);
    for (int i = 0; i < nimgs; i++) {
        img_t *im = &imgs[i];
        const char *rel = im->path;
        if (!strncmp(im->path, app, applen)) rel = im->path + applen;
        u32 mi = 0;
        if (im->has_tlv_init) tlv++;
        for (u32 k = 0; k < im->nimp; k++) {
            imp_t *x = &im->imps[k];
            const char *lib = x->lib > 0 && x->lib <= im->nd ? im->dylibs[x->lib - 1] : "(flat/self)";
            int weak = x->weak || (x->lib > 0 && x->lib <= im->nd && im->dweak[x->lib - 1]);
            libstat_t *l = libstat(lib);
            l->total++; tot++;
            if (set_has(&bundle_exports, x->name) || in_libsystem(x->name) || no_disco(x->name) || weak) continue;
            l->missing++; miss++; mi++;
            anota_ausente(lib, x->name);
        }
        for (int k = 0; k < im->nd; k++) if (!is_bundled(im->dylibs[k])) set_add(&sysdylibs, im->dylibs[k]);
        printf("%-9s %6.1f MiB  imports %6u  ausentes %6u%s%s  %s\n",
               im->ftype == 2 ? "exec" : im->ftype == 6 ? "dylib" : im->ftype == 8 ? "bundle" : "outro",
               im->size / 1048576.0, im->nimp, mi, im->chained ? "  chained" : "", im->has_tlv_init ? "  TLV-init" : "", rel);
    }
    printf("\nBibliotecas do sistema exigidas (%u):\n", sysdylibs.n);
    for (u32 i = 0; i < sysdylibs.cap; i++) if (sysdylibs.k[i]) printf("  [%s] %s\n", sys_ok(sysdylibs.k[i]) ? "ok   " : "falta", sysdylibs.k[i]);
    printf("\nImports por biblioteca de origem (total / sem implementacao):\n");
    for (int i = 0; i < nls; i++) if (!ls[i].bundled)
        printf("  %-40s %7u / %7u%s\n", ls[i].name, ls[i].total, ls[i].missing, ls[i].sysok ? "" : "   (biblioteca inexistente no mOS)");
    printf("\nTotal: %llu imports, %llu sem implementacao (%u simbolos distintos)%s\n",
           (unsigned long long)tot, (unsigned long long)miss, missing_nomes.n, rep ? "; lista completa no relatorio" : "");
    printf("Ausentes por origem (simbolos distintos): sistema %u, libc++ %u, Swift %u, unwind %u\n",
           n_sis, n_cxx, n_sw, n_uw);
    if (tlv) printf("Imagens com inicializadores thread-local (nao suportados): %d\n", tlv);
    if (rep) {
        grava_cat(rep, "sistema", n_sis);
        grava_cat(rep, "libc++", n_cxx);
        grava_cat(rep, "Swift", n_sw);
        grava_cat(rep, "unwind", n_uw);
        fclose(rep);
    }
    return disco_faltou ? 1 : 0;
}
