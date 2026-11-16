/*
 * mOS — formatadores de volume em memória para o hdiutil create (userland/cli-macos/hdiutil.c):
 *   vf_hfs(v, size, label, journaled, case_sensitive)   HFS+ / HFSX (TN1150), com journal opcional
 *   vf_fat32(v, size, label)                            FAT32
 * Volume vazio, como o newfs_hfs/newfs_msdos deixam. Escrito do zero; conferido pelo tools/volimg.c check.
 */
#ifndef VOLFMT_C
#define VOLFMT_C
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void vf_wb16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void vf_wb32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static void vf_wb64(uint8_t *p, uint64_t v) { vf_wb32(p, (uint32_t)(v >> 32)); vf_wb32(p + 4, (uint32_t)v); }
static void vf_wl16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void vf_wl32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static void vf_wl64(uint8_t *p, uint64_t v) { vf_wl32(p, (uint32_t)v); vf_wl32(p + 4, (uint32_t)(v >> 32)); }
static uint16_t vf_rl16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

#define VF_HFS_DELTA 2082844800u
static void vf_mark(uint8_t *bm, uint32_t s, uint32_t c) { for (uint32_t i = 0; i < c; i++) bm[(s + i) / 8] |= (uint8_t)(0x80 >> ((s + i) % 8)); }

static void vf_tree(uint8_t *t, uint32_t ns, uint32_t total, uint32_t maxkey, uint32_t attrs, uint8_t cmp, uint32_t clump, uint32_t used_leaf) {
    memset(t, 0, (uint64_t)ns * total);
    t[8] = 1;
    vf_wb16(t + 10, 3);
    uint8_t *hr = t + 14;
    vf_wb16(hr + 18, (uint16_t)ns);
    vf_wb16(hr + 20, (uint16_t)maxkey);
    vf_wb32(hr + 22, total);
    vf_wb32(hr + 26, total - 1 - (used_leaf ? 1 : 0));
    vf_wb32(hr + 32, clump);
    hr[37] = cmp;
    vf_wb32(hr + 38, attrs);
    vf_wb16(t + ns - 2, 14);
    vf_wb16(t + ns - 4, 14 + 106);
    vf_wb16(t + ns - 6, 14 + 106 + 128);
    vf_wb16(t + ns - 8, (uint16_t)(ns - 8));
    t[14 + 106 + 128] = used_leaf ? 0xC0 : 0x80;          /* nó 0 (e a folha 1) em uso */
}

/* rótulo UTF-8 -> UTF-16 (BMP); ':' vira '/' como no HFS+ */
static int vf_u16(const char *s, uint16_t *o, int max) {
    int n = 0;
    const uint8_t *p = (const uint8_t *)s;
    while (*p && n < max) {
        uint32_t c = *p++;
        if (c >= 0xC0) {
            int x = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
            c &= 0x3F >> x;
            while (x-- && (*p & 0xC0) == 0x80) c = c << 6 | (*p++ & 0x3F);
        }
        if (c > 0xFFFF) c = 0xFFFD;
        o[n++] = (uint16_t)(c == ':' ? '/' : c);
    }
    return n;
}
typedef struct { uint8_t k[520]; uint32_t kl; uint8_t d[248]; uint32_t dl; } vf_rec_t;
static uint32_t vf_ckey(uint8_t *k, uint32_t parent, const uint16_t *nm, int n) {
    memset(k, 0, 520);
    vf_wb32(k + 2, parent);
    vf_wb16(k + 6, (uint16_t)n);
    for (int i = 0; i < n; i++) vf_wb16(k + 8 + 2 * i, nm[i]);
    vf_wb16(k, (uint16_t)(6 + 2 * n));
    return 8 + 2u * (uint32_t)n;
}

/* HFS+: cabeçalho em 1024, bitmap, extents, catálogo, atributos e (com journal) .journal_info_block e .journal
 * como arquivos invisíveis da raiz, igual ao newfs_hfs -J. Retorna 0 ou -1 (volume pequeno demais). */
int vf_hfs(uint8_t *v, uint64_t size, const char *label, int journaled, int hfsx) {
    const uint32_t bs = 4096, ns = 8192;
    uint64_t tb64 = size / bs;
    if (tb64 < 256 || tb64 > 0xFFFFFFFFull) return -1;
    uint32_t tb = (uint32_t)tb64;
    memset(v, 0, size);
    /* árvores pelo tamanho do volume (como o newfs: ~0,4% para o catálogo, mínimo de 32 nós) */
    uint64_t cat_b = size / 256, ext_b = size / 1024;
    uint32_t cat_nodes = (uint32_t)(cat_b / ns), ext_nodes = (uint32_t)(ext_b / ns), attr_nodes;
    if (cat_nodes < 32) cat_nodes = 32;
    if (cat_nodes > 8192) cat_nodes = 8192;
    if (ext_nodes < 8) ext_nodes = 8;
    if (ext_nodes > 2048) ext_nodes = 2048;
    attr_nodes = cat_nodes;
    uint32_t next = 1;                                    /* bloco 0: boot + cabeçalho */
    uint32_t bm_blocks = (tb / 8 + bs - 1) / bs;
    uint32_t alloc_s = next; next += bm_blocks;
    uint32_t ext_s = next; next += ext_nodes * (ns / bs);
    uint32_t cat_s = next; next += cat_nodes * (ns / bs);
    uint32_t attr_s = next; next += attr_nodes * (ns / bs);
    uint32_t jib_s = 0, j_s = 0, j_blocks = 0;
    if (journaled) {
        uint64_t jsz = size / 128;                        /* journal ~0,8%, de 512 KiB a 64 MiB */
        if (jsz < (512u << 10)) jsz = 512u << 10;
        if (jsz > (64u << 20)) jsz = 64u << 20;
        j_blocks = (uint32_t)(jsz / bs);
        jib_s = next++;
        j_s = next; next += j_blocks;
    }
    if (next + 8 >= tb) return -1;
    uint8_t *bm = v + (uint64_t)alloc_s * bs;
    vf_mark(bm, 0, next);
    vf_mark(bm, tb - 1, 1);                               /* cabeçalho alternativo no penúltimo setor */
    vf_tree(v + (uint64_t)ext_s * bs, ns, ext_nodes, 10, 2, 0, ext_nodes * ns, 0);
    vf_tree(v + (uint64_t)attr_s * bs, ns, attr_nodes, 266, 6, 0, attr_nodes * ns, 0);
    uint8_t *ct = v + (uint64_t)cat_s * bs;
    vf_tree(ct, ns, cat_nodes, 516, 6, hfsx ? 0xBC : 0xCF, cat_nodes * ns, 1);
    uint32_t now = (uint32_t)time(NULL) + VF_HFS_DELTA;
    uint16_t vn[255];
    int vnl = vf_u16(label && label[0] ? label : "Sem título", vn, 255);
    vf_rec_t *r = calloc(8, sizeof *r);
    if (!r) return -1;
    int nr = 0;
    /* raiz (pai 1, rótulo) e thread (2, "") */
    {
        vf_rec_t *x = &r[nr++];
        x->kl = vf_ckey(x->k, 1, vn, vnl);
        vf_wb16(x->d, 1);
        vf_wb32(x->d + 8, 2);
        for (int i = 12; i < 28; i += 4) vf_wb32(x->d + i, now);
        vf_wb32(x->d + 32, 99); vf_wb32(x->d + 36, 99); vf_wb16(x->d + 42, 040755);
        x->dl = 88;
        vf_rec_t *t = &r[nr++];
        t->kl = vf_ckey(t->k, 2, NULL, 0);
        vf_wb16(t->d, 3);
        vf_wb32(t->d + 4, 1);
        vf_wb16(t->d + 8, (uint16_t)vnl);
        for (int i = 0; i < vnl; i++) vf_wb16(t->d + 10 + 2 * i, vn[i]);
        t->dl = 10 + 2u * (uint32_t)vnl;
    }
    uint32_t id = 16;
    int nfiles = 0;
    if (journaled) {
        struct { const char *n; uint32_t s, c; uint64_t len; } jf[2] = {
            {".journal", j_s, j_blocks, (uint64_t)j_blocks * bs}, {".journal_info_block", jib_s, 1, bs}};
        for (int f = 0; f < 2; f++) {
            uint16_t nm[32];
            int n = vf_u16(jf[f].n, nm, 32);
            vf_rec_t *x = &r[nr++];
            x->kl = vf_ckey(x->k, 2, nm, n);
            vf_wb16(x->d, 2);
            vf_wb16(x->d + 2, 2);                         /* kHFSThreadExistsMask */
            vf_wb32(x->d + 8, id + (uint32_t)f);
            for (int i = 12; i < 28; i += 4) vf_wb32(x->d + i, now);
            vf_wb16(x->d + 42, 0100644);
            x->d[41] = 0x02;                              /* UF_IMMUTABLE */
            vf_wb16(x->d + 56, 0x4000 | 0x0400);          /* kIsInvisible | kNameLocked (finderFlags) */
            vf_wb32(x->d + 48, 0x6A726E6C);               /* 'jrnl' */
            vf_wb32(x->d + 52, 0x6866732B);               /* 'hfs+' */
            vf_wb64(x->d + 88, jf[f].len);
            vf_wb32(x->d + 100, jf[f].c);
            vf_wb32(x->d + 104, jf[f].s);
            vf_wb32(x->d + 108, jf[f].c);
            x->dl = 248;
            vf_rec_t *t = &r[nr++];
            t->kl = vf_ckey(t->k, id + (uint32_t)f, NULL, 0);
            vf_wb16(t->d, 4);
            vf_wb32(t->d + 4, 2);
            vf_wb16(t->d + 8, (uint16_t)n);
            for (int i = 0; i < n; i++) vf_wb16(t->d + 10 + 2 * i, nm[i]);
            t->dl = 10 + 2u * (uint32_t)n;
            nfiles++;
        }
        id += 2;
    }
    vf_wb32(r[0].d + 4, (uint32_t)nfiles);                /* valência da raiz */
    /* ordem do catálogo: pai, depois nome (".journal" < ".journal_info_block"); threads (nome vazio) primeiro */
    {
        int order[8], no = 0;
        order[no++] = 0;                                  /* (1, rótulo) */
        order[no++] = 1;                                  /* (2, "") */
        if (journaled) { order[no++] = 2; order[no++] = 4; order[no++] = 3; order[no++] = 5; }
        uint8_t *leaf = ct + ns;
        leaf[8] = 0xFF;
        leaf[9] = 1;
        vf_wb16(leaf + 10, (uint16_t)nr);
        uint32_t off = 14;
        for (int i = 0; i < no; i++) {
            vf_rec_t *x = &r[order[i]];
            vf_wb16(leaf + ns - 2 * (i + 1), (uint16_t)off);
            memcpy(leaf + off, x->k, x->kl); off += x->kl;
            memcpy(leaf + off, x->d, x->dl); off += x->dl;
        }
        vf_wb16(leaf + ns - 2 * (no + 1), (uint16_t)off);
        uint8_t *hr = ct + 14;
        vf_wb16(hr, 1);                                   /* treeDepth */
        vf_wb32(hr + 2, 1);                               /* rootNode */
        vf_wb32(hr + 6, (uint32_t)nr);                    /* leafRecords */
        vf_wb32(hr + 10, 1);                              /* firstLeaf */
        vf_wb32(hr + 14, 1);                              /* lastLeaf */
    }
    free(r);
    if (journaled) {
        uint8_t *jib = v + (uint64_t)jib_s * bs;
        vf_wb32(jib, 1);                                  /* kJIJournalInFSMask */
        vf_wb64(jib + 36, (uint64_t)j_s * bs);
        vf_wb64(jib + 44, (uint64_t)j_blocks * bs);
        uint8_t *jh = v + (uint64_t)j_s * bs;
        vf_wl32(jh, 0x4a4e4c78); vf_wl32(jh + 4, 0x12345678);
        vf_wl64(jh + 8, 512); vf_wl64(jh + 16, 512); vf_wl64(jh + 24, (uint64_t)j_blocks * bs);
        vf_wl32(jh + 32, 4096); vf_wl32(jh + 40, 512); vf_wl32(jh + 44, 1);
        uint32_t ck = 0;                                  /* checksum do cabeçalho do journal (campo zerado) */
        for (int i = 0; i < 44; i++) ck = (ck << 8) ^ (ck + jh[i]);
        vf_wl32(jh + 36, ~ck);
    }
    uint8_t *vh = v + 1024;
    vf_wb16(vh, hfsx ? 0x4858 : 0x482B);
    vf_wb16(vh + 2, hfsx ? 5 : 4);
    vf_wb32(vh + 4, 0x100 | (journaled ? 0x2000 : 0));    /* desmontado direito, journal */
    memcpy(vh + 8, journaled ? "HFSJ" : "10.0", 4);
    vf_wb32(vh + 12, jib_s);
    vf_wb32(vh + 16, now - (uint32_t)0); vf_wb32(vh + 20, now); vf_wb32(vh + 28, now);
    vf_wb32(vh + 32, (uint32_t)nfiles);
    vf_wb32(vh + 36, 0);
    vf_wb32(vh + 40, bs);
    vf_wb32(vh + 44, tb);
    vf_wb32(vh + 48, tb - next - 1);
    vf_wb32(vh + 52, next);
    vf_wb32(vh + 56, 65536);
    vf_wb32(vh + 60, 65536);
    vf_wb32(vh + 64, id);
    vf_wb64(vh + 72, 1);
    struct { uint32_t off, s, c; } F[4] = {{112, alloc_s, bm_blocks}, {192, ext_s, ext_nodes}, {272, cat_s, cat_nodes}, {352, attr_s, attr_nodes}};
    for (int i = 0; i < 4; i++) {
        uint32_t blocks = i ? F[i].c * (ns / bs) : F[i].c;
        vf_wb64(vh + F[i].off, (uint64_t)blocks * bs);
        vf_wb32(vh + F[i].off + 8, i ? F[i].c * ns : bs);
        vf_wb32(vh + F[i].off + 12, blocks);
        vf_wb32(vh + F[i].off + 16, F[i].s);
        vf_wb32(vh + F[i].off + 20, blocks);
    }
    memcpy(v + (uint64_t)tb * bs - 1024, vh, 512);
    return 0;
}

/* FAT32 (newfs_msdos -F 32): 32 setores reservados, duas FATs, raiz no cluster 2, FSInfo e cópia do boot no 6 */
int vf_fat32(uint8_t *v, uint64_t size, const char *label) {
    uint32_t bps = 512, tot = (uint32_t)(size / bps), res = 32, nf = 2;
    if (size < (33u << 20) || size / bps > 0xFFFFFFFFull) return -1;
    uint32_t spc = size <= (260u << 20) ? 1 : size <= (8ull << 30) ? 8 : size <= (16ull << 30) ? 16 : size <= (32ull << 30) ? 32 : 64;
    uint32_t fatsz = 1, nclus = 0;
    for (int it = 0; it < 32; it++) {
        nclus = (tot - res - nf * fatsz) / spc;
        uint32_t f = (uint32_t)(((uint64_t)(nclus + 2) * 4 + bps - 1) / bps);
        if (f == fatsz) break;
        fatsz = f;
    }
    if (nclus < 65525) return -1;                         /* FAT32 pede pelo menos 65525 clusters */
    memset(v, 0, (uint64_t)(res + nf * fatsz + spc) * bps);
    v[0] = 0xEB; v[1] = 0x58; v[2] = 0x90;
    memcpy(v + 3, "BSD  4.4", 8);
    vf_wl16(v + 11, (uint16_t)bps); v[13] = (uint8_t)spc; vf_wl16(v + 14, (uint16_t)res); v[16] = (uint8_t)nf;
    v[21] = 0xF8; vf_wl16(v + 24, 32); vf_wl16(v + 26, 16);
    vf_wl32(v + 32, tot);
    vf_wl32(v + 36, fatsz); vf_wl32(v + 44, 2); vf_wl16(v + 48, 1); vf_wl16(v + 50, 6);
    char lab[11];
    memset(lab, ' ', 11);
    for (int i = 0; label && label[i] && i < 11; i++) { char c = label[i]; lab[i] = c >= 'a' && c <= 'z' ? (char)(c - 32) : c; }
    uint32_t vid = (uint32_t)time(NULL) * 2654435761u;
    v[64] = 0x80; v[66] = 0x29; vf_wl32(v + 67, vid); memcpy(v + 71, lab, 11); memcpy(v + 82, "FAT32   ", 8);
    v[510] = 0x55; v[511] = 0xAA;
    uint8_t *fi = v + bps;
    vf_wl32(fi, 0x41615252); vf_wl32(fi + 484, 0x61417272); vf_wl32(fi + 488, nclus - 1); vf_wl32(fi + 492, 3);
    fi[510] = 0x55; fi[511] = 0xAA;
    memcpy(v + 6 * bps, v, bps);
    memcpy(v + 7 * bps, fi, bps);
    for (uint32_t f = 0; f < nf; f++) {
        uint8_t *fat = v + (uint64_t)(res + f * fatsz) * bps;
        vf_wl32(fat, 0x0FFFFFF8); vf_wl32(fat + 4, 0x0FFFFFFF); vf_wl32(fat + 8, 0x0FFFFFFF);
    }
    uint8_t *root = v + (uint64_t)(res + nf * fatsz) * bps;
    memcpy(root, lab, 11);
    root[11] = 0x08;                                      /* rótulo do volume */
    (void)vf_rl16;
    return 0;
}
#endif
