/*
 * mOS — escrita em volumes HFS+/HFSX (TN1150), reescrita do zero para o mOS (MIT).
 *
 * Modelo: cada operação do VFS (gravar arquivo inteiro, criar pasta, apagar, renomear) é uma
 * transação. Durante a operação, os nós das B-trees alterados ficam num cache em RAM, o bitmap de
 * alocação inteiro fica em RAM e o cabeçalho do volume também; no fim (tx_commit) tudo que mudou é
 * gravado de uma vez:
 *   - volume com journal: os blocos vão primeiro para o journal (uma lista de blocos + dados, com
 *     checksums, no formato do xnu), o cabeçalho do journal avança, os blocos são gravados no lugar e
 *     o journal volta a ficar vazio (checkpoint imediato). Uma queda no meio deixa o journal com a
 *     transação inteira, que o próximo mount (aqui ou no macOS) repete;
 *   - sem journal: grava direto (como o HFS+ sem journal do macOS: uma queda pode pedir fsck).
 * O conteúdo dos arquivos (fork de dados) é gravado direto em blocos recém-alocados antes da
 * transação de metadados, então um arquivo substituído nunca fica pela metade.
 *
 * B-trees: inserção com divisão de nós (split) e criação de nova raiz; remoção que libera nós vazios
 * e encolhe a raiz; chaves de índice sempre iguais à primeira chave do filho. Árvores crescem pelo
 * clump size quando faltam nós (até 8 extents no cabeçalho do volume).
 *
 * Limitações (docs/volumes-graveis.md): hard links não são apagados quando fazem parte de uma
 * cadeia de links; volumes dentro de um wrapper HFS e journal em outro disco ficam só leitura; a
 * ordenação de nomes sem diferenciar maiúsculas cobre latim, grego, cirílico e largura total.
 */
#include "hfsplus_int.h"

#define VA_HWLOCK     0x00000080u
#define VA_UNMOUNTED  0x00000100u
#define VA_INCONSIST  0x00000800u
#define VA_JOURNALED  0x00002000u
#define VA_SWLOCK     0x00008000u

#define JI_IN_FS      0x1u
#define JI_OTHER_DEV  0x2u
#define JI_NEED_INIT  0x4u
#define JNL_MAGIC     0x4a4e4c78u      /* 'JNLx' */
#define JNL_ENDIAN    0x12345678u
#define BLHDR_FIRST   0x0002u

#define REC_FOLDER        1
#define REC_FILE          2
#define REC_FOLDER_THREAD 3
#define REC_FILE_THREAD   4
#define F_THREAD_EXISTS   0x0002
#define F_HAS_FOLDERCOUNT 0x0010
#define F_HAS_LINKCHAIN   0x0020
#define UF_COMPRESSED     0x20

#define CNID_ROOT_PARENT 1
#define CNID_ROOT        2
#define MAX_BITMAP       (64u << 20)

enum { T_CAT, T_EXT, T_ATTR, NTREES };

typedef struct nbuf { int tree; u32 idx; bool dirty; u8 *d; struct nbuf *next; } nbuf_t;

typedef struct {
    fork_t *fork;            /* o mesmo fork_t usado pela leitura (hfsplus.c) */
    u32 vh_off;              /* deslocamento do HFSPlusForkData no cabeçalho do volume */
    u32 file_id;
    u32 ns;                  /* 0 = árvore inexistente */
    bool varidx;             /* chaves de índice de tamanho variável */
    u16 maxkey;
} bt_t;

typedef struct { u64 off; u32 len; u8 *data; } txent_t;

typedef struct hfs_wr {
    hfs_t *h;
    u8 vh[512];
    bool vh_dirty;
    u64 vol_bytes;
    u32 total_blocks;
    fork_t alloc;
    u8 *bm;
    u32 bm_bytes, bm_pages;
    u8 *bm_dirty;            /* 1 byte por página (bloco) do arquivo de alocação */
    bt_t bt[NTREES];
    nbuf_t *nodes;
    int ndirty;
    bool binary;             /* HFSX com comparação binária de nomes */
    bool use_fc;             /* pastas com contagem de subpastas (kHFSHasFolderCountMask) */
    u32 priv_dir;            /* pasta dos hard links (0 = nenhuma) */
    /* journal */
    bool jnl;
    u64 joff, jsize;
    u32 jhdr_size, blhdr_size, jseq;
    bool jle;
    u64 jstart, jend;
    txent_t *tx;
    int ntx, captx;
} hfs_wr_t;

/* ================================================================ utilidades */
static void put16(u8 *p, u16 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
static void put32(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }
static void put64(u8 *p, u64 v) { put32(p, (u32)(v >> 32)); put32(p + 4, (u32)v); }
static void lput32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static void lput64(u8 *p, u64 v) { lput32(p, (u32)v); lput32(p + 4, (u32)(v >> 32)); }
static void lput16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }

/* datas do HFS+: segundos desde 1904-01-01 (UTC) */
static u32 hfs_now(void) {
    datetime_t dt;
#ifdef MOS_HOST_TEST
    rtc_now(&dt);
#else
    rtc_now_utc(&dt);
#endif
    int y = dt.year, m = dt.month, d = dt.day;
    if (y < 1970 || m < 1 || m > 12 || d < 1) return (u32)HFS_EPOCH_DELTA;
    int yy = y - (m <= 2), era = yy / 400, yoe = yy - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    i64 days = (i64)era * 146097 + doe - 719468;
    return (u32)(days * 86400 + dt.hour * 3600 + dt.minute * 60 + dt.second + HFS_EPOCH_DELTA);
}

/* checksum do journal (TN1150): byte a byte, (c << 8) ^ (c + b), complementado no fim */
static u32 jnl_cksum(const u8 *p, u32 len) {
    u32 c = 0;
    for (u32 i = 0; i < len; i++) c = (c << 8) ^ (c + p[i]);
    return ~c;
}

/* ---------------------------------------------------------------- nomes
 * UTF-8 (NFC, do VFS) -> UTF-16 decomposto como o macOS guarda (acentos latinos separados), e ':' do
 * VFS volta a ser '/' no catálogo (o HFS+ guarda '/' onde o POSIX mostra ':'). */
static const struct { u16 pre, base, mark; } DECOMP[] = {
    {0xC0, 'A', 0x300}, {0xC1, 'A', 0x301}, {0xC2, 'A', 0x302}, {0xC3, 'A', 0x303}, {0xC4, 'A', 0x308}, {0xC5, 'A', 0x30A},
    {0xC7, 'C', 0x327}, {0xC8, 'E', 0x300}, {0xC9, 'E', 0x301}, {0xCA, 'E', 0x302}, {0xCB, 'E', 0x308},
    {0xCC, 'I', 0x300}, {0xCD, 'I', 0x301}, {0xCE, 'I', 0x302}, {0xCF, 'I', 0x308}, {0xD1, 'N', 0x303},
    {0xD2, 'O', 0x300}, {0xD3, 'O', 0x301}, {0xD4, 'O', 0x302}, {0xD5, 'O', 0x303}, {0xD6, 'O', 0x308},
    {0xD9, 'U', 0x300}, {0xDA, 'U', 0x301}, {0xDB, 'U', 0x302}, {0xDC, 'U', 0x308}, {0xDD, 'Y', 0x301},
    {0xE0, 'a', 0x300}, {0xE1, 'a', 0x301}, {0xE2, 'a', 0x302}, {0xE3, 'a', 0x303}, {0xE4, 'a', 0x308}, {0xE5, 'a', 0x30A},
    {0xE7, 'c', 0x327}, {0xE8, 'e', 0x300}, {0xE9, 'e', 0x301}, {0xEA, 'e', 0x302}, {0xEB, 'e', 0x308},
    {0xEC, 'i', 0x300}, {0xED, 'i', 0x301}, {0xEE, 'i', 0x302}, {0xEF, 'i', 0x308}, {0xF1, 'n', 0x303},
    {0xF2, 'o', 0x300}, {0xF3, 'o', 0x301}, {0xF4, 'o', 0x302}, {0xF5, 'o', 0x303}, {0xF6, 'o', 0x308},
    {0xF9, 'u', 0x300}, {0xFA, 'u', 0x301}, {0xFB, 'u', 0x302}, {0xFC, 'u', 0x308}, {0xFD, 'y', 0x301}, {0xFF, 'y', 0x308},
};

/* devolve o nº de unidades UTF-16, ou -1 se não couber em 255 / for inválido */
static int name16(const char *s, u16 *out) {
    int n = 0;
    const u8 *p = (const u8 *)s;
    while (*p) {
        u32 c = *p++;
        if (c >= 0xF0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            c = (c & 7) << 18 | (u32)(p[0] & 63) << 12 | (u32)(p[1] & 63) << 6 | (p[2] & 63); p += 3;
        } else if (c >= 0xE0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80) {
            c = (c & 15) << 12 | (u32)(p[0] & 63) << 6 | (p[1] & 63); p += 2;
        } else if (c >= 0xC0 && (p[0] & 0xC0) == 0x80) {
            c = (c & 31) << 6 | (p[0] & 63); p += 1;
        } else if (c >= 0x80) return -1;
        if (c == ':') c = '/';
        u32 mark = 0;
        for (u32 i = 0; i < ARRAY_LEN(DECOMP); i++) if (DECOMP[i].pre == c) { c = DECOMP[i].base; mark = DECOMP[i].mark; break; }
        if (c >= 0x10000) {
            if (n + 2 > 255) return -1;
            c -= 0x10000;
            out[n++] = (u16)(0xD800 | (c >> 10));
            out[n++] = (u16)(0xDC00 | (c & 0x3FF));
        } else {
            if (n + 1 > 255) return -1;
            out[n++] = (u16)c;
        }
        if (mark) { if (n + 1 > 255) return -1; out[n++] = (u16)mark; }
    }
    return n ? n : -1;
}

/* dobra de caixa para comparar nomes como o HFS+ sem diferenciar maiúsculas: 0 = caractere ignorado;
 * NUL ordena depois de tudo (é assim que a pasta privada "\0\0\0\0HFS+ Private Data" fica no fim) */
static u16 fold(u16 c) {
    if (c == 0) return 0xFFFF;
    if (c < 0x80) return c >= 'A' && c <= 'Z' ? (u16)(c + 32) : c;
    if ((c >= 0x200C && c <= 0x200F) || (c >= 0x202A && c <= 0x202E) || (c >= 0x206A && c <= 0x206F) || c == 0xFEFF) return 0;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return (u16)(c + 32);
    if ((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177)) return (u16)(c | 1);
    if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) return (u16)((c & 1) ? c + 1 : c);
    if (c == 0x178) return 0xFF;
    if (c >= 0x391 && c <= 0x3A9 && c != 0x3A2) return (u16)(c + 32);
    if (c >= 0x410 && c <= 0x42F) return (u16)(c + 32);
    if (c >= 0x400 && c <= 0x40F) return (u16)(c + 80);
    if (c >= 0xFF21 && c <= 0xFF3A) return (u16)(c + 32);
    return c;
}

/* a, b: UTF-16 big-endian com al/bl unidades */
static int name_cmp(const hfs_wr_t *w, const u8 *a, u32 al, const u8 *b, u32 bl) {
    if (w->binary) {
        for (u32 i = 0; i < al && i < bl; i++) {
            u16 x = be16(a + i * 2), y = be16(b + i * 2);
            if (x != y) return x < y ? -1 : 1;
        }
        return al == bl ? 0 : al < bl ? -1 : 1;
    }
    u32 i = 0, j = 0;
    for (;;) {
        u16 x = 0, y = 0;
        while (i < al && !(x = fold(be16(a + i * 2)))) i++;
        while (j < bl && !(y = fold(be16(b + j * 2)))) j++;
        if (i < al) i++;
        if (j < bl) j++;
        if (x != y) return x < y ? -1 : 1;
        if (!x) return 0;                  /* os dois acabaram */
    }
}

/* ================================================================ chaves */
static int key_cmp(const hfs_wr_t *w, int tree, const u8 *a, const u8 *b) {
    if (tree == T_CAT) {
        u32 pa = be32(a + 2), pb = be32(b + 2);
        if (pa != pb) return pa < pb ? -1 : 1;
        return name_cmp(w, a + 8, be16(a + 6), b + 8, be16(b + 6));
    }
    if (tree == T_EXT) {
        u32 fa = be32(a + 4), fb = be32(b + 4);
        if (fa != fb) return fa < fb ? -1 : 1;
        if (a[2] != b[2]) return a[2] < b[2] ? -1 : 1;
        u32 sa = be32(a + 8), sb = be32(b + 8);
        return sa == sb ? 0 : sa < sb ? -1 : 1;
    }
    u32 fa = be32(a + 4), fb = be32(b + 4);
    if (fa != fb) return fa < fb ? -1 : 1;
    u32 na = be16(a + 12), nb = be16(b + 12);
    for (u32 i = 0; i < na && i < nb; i++) {
        u16 x = be16(a + 14 + i * 2), y = be16(b + 14 + i * 2);
        if (x != y) return x < y ? -1 : 1;
    }
    if (na != nb) return na < nb ? -1 : 1;
    u32 sa = be32(a + 8), sb = be32(b + 8);
    return sa == sb ? 0 : sa < sb ? -1 : 1;
}

static u32 cat_key(u8 *k, u32 parent, const u16 *name, int n) {
    put16(k, (u16)(6 + 2 * n));
    put32(k + 2, parent);
    put16(k + 6, (u16)n);
    for (int i = 0; i < n; i++) put16(k + 8 + i * 2, name[i]);
    return 8 + 2 * (u32)n;
}

static u32 ext_key(u8 *k, u8 fork_type, u32 file_id, u32 start) {
    put16(k, 10);
    k[2] = fork_type;
    k[3] = 0;
    put32(k + 4, file_id);
    put32(k + 8, start);
    return 12;
}

/* ================================================================ nós (cache por operação) */
static u32 ns_of(hfs_wr_t *w, int t) { return w->bt[t].ns; }

static u8 *node_get(hfs_wr_t *w, int t, u32 idx) {
    for (nbuf_t *b = w->nodes; b; b = b->next) if (b->tree == t && b->idx == idx) return b->d;
    u32 ns = ns_of(w, t);
    if (!ns || (u64)(idx + 1) * ns > w->bt[t].fork->size) return NULL;
    nbuf_t *b = kcalloc(1, sizeof *b);
    u8 *d = b ? kmalloc(ns) : NULL;
    if (!d || !hfs_fork_read(w->h, w->bt[t].fork, (u64)idx * ns, d, ns)) { kfree(d); kfree(b); return NULL; }
    b->tree = t;
    b->idx = idx;
    b->d = d;
    b->next = w->nodes;
    w->nodes = b;
    return d;
}

static void node_dirty(hfs_wr_t *w, int t, u32 idx) {
    for (nbuf_t *b = w->nodes; b; b = b->next)
        if (b->tree == t && b->idx == idx) { if (!b->dirty) w->ndirty++; b->dirty = true; return; }
}

static void nodes_drop(hfs_wr_t *w) {
    for (nbuf_t *b = w->nodes, *nx; b; b = nx) { nx = b->next; kfree(b->d); kfree(b); }
    w->nodes = NULL;
    w->ndirty = 0;
}

/* layout de um nó: descritor de 14 bytes, registros, tabela de deslocamentos no fim */
static u16 n_nrec(const u8 *n) { return be16(n + 10); }
static u16 n_off(const u8 *n, u32 ns, int i) { return be16(n + ns - 2 * (i + 1)); }
static const u8 *n_rec(const u8 *n, u32 ns, int i, u32 *len) {
    u16 a = n_off(n, ns, i), b = n_off(n, ns, i + 1);
    if (len) *len = b > a ? (u32)(b - a) : 0;
    return n + a;
}
static u32 key_bytes(const u8 *rec) { u32 k = 2 + be16(rec); return (k + 1) & ~1u; }

/* cabeçalho da B-tree (registro 0 do nó 0) */
static u8 *bt_hdr(hfs_wr_t *w, int t) { u8 *n = node_get(w, t, 0); return n ? n + 14 : NULL; }
static void hdr_dirty(hfs_wr_t *w, int t) { node_dirty(w, t, 0); }

/* nó 0 tem o mapa de nós no registro 2; nós de mapa adicionais (tipo 2) seguem pelo fLink */
typedef struct { int tree; u32 node; u32 off, len; } mapseg_t;
static int map_segments(hfs_wr_t *w, int t, mapseg_t *seg, int max) {
    u32 ns = ns_of(w, t), idx = 0;
    int k = 0;
    for (int guard = 0; guard < 64 && k < max; guard++) {
        u8 *n = node_get(w, t, idx);
        if (!n) break;
        int r = idx == 0 ? 2 : 0;
        if (n_nrec(n) <= r) break;
        u32 len;
        const u8 *p = n_rec(n, ns, r, &len);
        seg[k++] = (mapseg_t){t, idx, (u32)(p - n), len};
        idx = be32(n);
        if (!idx) break;
    }
    return k;
}

static bool bt_grow(hfs_wr_t *w, int t);

static u32 node_alloc(hfs_wr_t *w, int t) {
    u8 *hd = bt_hdr(w, t);
    if (!hd) return 0;
    if (!be32(hd + 26) && (!bt_grow(w, t) || !(hd = bt_hdr(w, t)) || !be32(hd + 26))) return 0;
    u32 total = be32(hd + 22);
    mapseg_t seg[64];
    int ns = map_segments(w, t, seg, 64);
    u32 base = 0;
    for (int s = 0; s < ns; s++) {
        u8 *n = node_get(w, t, seg[s].node);
        for (u32 i = 0; i < seg[s].len * 8 && base + i < total; i++) {
            u8 *byte = n + seg[s].off + i / 8, bit = (u8)(0x80 >> (i % 8));
            if (*byte & bit) continue;
            *byte |= bit;
            node_dirty(w, t, seg[s].node);
            hd = bt_hdr(w, t);
            put32(hd + 26, be32(hd + 26) - 1);
            hdr_dirty(w, t);
            u32 idx = base + i;
            u8 *nn = node_get(w, t, idx);
            if (!nn) return 0;
            memset(nn, 0, ns_of(w, t));
            node_dirty(w, t, idx);
            return idx;
        }
        base += seg[s].len * 8;
    }
    return 0;
}

static void node_free(hfs_wr_t *w, int t, u32 idx) {
    mapseg_t seg[64];
    int ns = map_segments(w, t, seg, 64);
    u32 base = 0;
    for (int s = 0; s < ns; s++) {
        if (idx < base + seg[s].len * 8) {
            u32 i = idx - base;
            u8 *n = node_get(w, t, seg[s].node);
            n[seg[s].off + i / 8] &= (u8)~(0x80 >> (i % 8));
            node_dirty(w, t, seg[s].node);
            u8 *hd = bt_hdr(w, t);
            put32(hd + 26, be32(hd + 26) + 1);
            hdr_dirty(w, t);
            u8 *nn = node_get(w, t, idx);
            if (nn) { memset(nn, 0, ns_of(w, t)); node_dirty(w, t, idx); }
            return;
        }
        base += seg[s].len * 8;
    }
}

/* ================================================================ bitmap de alocação */
static bool bm_get(hfs_wr_t *w, u32 b) { return w->bm[b >> 3] & (0x80 >> (b & 7)); }
static void bm_set(hfs_wr_t *w, u32 b, bool v) {
    u8 bit = (u8)(0x80 >> (b & 7));
    if (v) w->bm[b >> 3] |= bit; else w->bm[b >> 3] &= (u8)~bit;
    w->bm_dirty[(b >> 3) / w->h->bs] = 1;
}

static void blocks_mark(hfs_wr_t *w, u32 start, u32 count, bool used) {
    for (u32 i = 0; i < count; i++) bm_set(w, start + i, used);
    put32(w->vh + 48, be32(w->vh + 48) + (used ? -count : count));
    w->vh_dirty = true;
}

/* maior trecho livre começando exatamente em b (até max) */
static u32 run_at(hfs_wr_t *w, u32 b, u32 max) {
    u32 n = 0;
    while (b + n < w->total_blocks && n < max && !bm_get(w, b + n)) n++;
    return n;
}

/* aloca want blocos: um trecho contíguo se houver; senão vários (first fit a partir da dica).
 * Devolve o nº de extents em *ext (kmalloc) ou -1 se faltar espaço. */
static int blocks_alloc(hfs_wr_t *w, u32 want, ext_t **out, u32 prefer) {
    *out = NULL;
    if (!want) return 0;
    if (be32(w->vh + 48) < want) return -1;
    u32 tb = w->total_blocks, hint = prefer ? prefer : be32(w->vh + 52);
    if (hint >= tb) hint = 0;
    /* 1ª passada: trecho inteiro */
    for (u32 pass = 0, b = hint; pass < 2; pass++, b = 0) {
        u32 end = pass ? hint : tb;
        while (b < end) {
            if (w->bm[b >> 3] == 0xFF && !(b & 7)) { b += 8; continue; }
            if (bm_get(w, b)) { b++; continue; }
            u32 r = run_at(w, b, want);
            if (r >= want) {
                ext_t *e = kmalloc(sizeof *e);
                if (!e) return -1;
                *e = (ext_t){b, want};
                blocks_mark(w, b, want, true);
                put32(w->vh + 52, b + want < tb ? b + want : 0);
                *out = e;
                return 1;
            }
            b += r;
        }
    }
    /* 2ª: junta trechos menores */
    int cap = 8, n = 0;
    ext_t *e = kmalloc(sizeof *e * cap);
    if (!e) return -1;
    u32 left = want;
    for (u32 b = 0; b < tb && left; ) {
        if (bm_get(w, b)) { b++; continue; }
        u32 r = run_at(w, b, left);
        if (n == cap) { ext_t *p = krealloc(e, sizeof *e * cap * 2); if (!p) break; e = p; cap *= 2; }
        e[n++] = (ext_t){b, r};
        blocks_mark(w, b, r, true);
        left -= r;
        b += r;
    }
    if (left) { for (int i = 0; i < n; i++) blocks_mark(w, e[i].start, e[i].count, false); kfree(e); return -1; }
    *out = e;
    return n;
}

/* ================================================================ transação */
static void tx_clear(hfs_wr_t *w) {
    for (int i = 0; i < w->ntx; i++) kfree(w->tx[i].data);
    w->ntx = 0;
}

static bool tx_add(hfs_wr_t *w, u64 off, const u8 *data, u32 len) {
    u32 J = w->jnl ? w->jhdr_size : 1;
    u64 a = off / J * J, b = (off + len + J - 1) / J * J;
    for (int i = 0; i < w->ntx; i++) {                     /* mesmo bloco duas vezes: fica o novo */
        if (w->tx[i].off == a && w->tx[i].len == b - a) { memcpy(w->tx[i].data + (off - a), data, len); return true; }
        if (a < w->tx[i].off + w->tx[i].len && w->tx[i].off < b) {   /* sobreposição parcial: junta */
            u64 na = MIN(a, w->tx[i].off), nb = MAX(b, w->tx[i].off + w->tx[i].len);
            u8 *nd = kmalloc(nb - na);
            if (!nd || !blk_read(w->h->dev, na, nd, nb - na)) { kfree(nd); return false; }
            memcpy(nd + (w->tx[i].off - na), w->tx[i].data, w->tx[i].len);
            memcpy(nd + (off - na), data, len);
            kfree(w->tx[i].data);
            w->tx[i] = (txent_t){na, (u32)(nb - na), nd};
            return true;
        }
    }
    if (w->ntx == w->captx) {
        int nc = w->captx ? w->captx * 2 : 64;
        txent_t *p = krealloc(w->tx, sizeof *p * nc);
        if (!p) return false;
        w->tx = p;
        w->captx = nc;
    }
    u8 *d = kmalloc(b - a);
    if (!d) return false;
    if ((a != off || b != off + len) && !blk_read(w->h->dev, a, d, b - a)) { kfree(d); return false; }
    memcpy(d + (off - a), data, len);
    w->tx[w->ntx++] = (txent_t){a, (u32)(b - a), d};
    return true;
}

/* registra no tx o trecho [off, off+len) de um fork (nós de B-tree, bitmap) */
static bool tx_add_fork(hfs_wr_t *w, const fork_t *f, u64 off, const u8 *data, u64 len) {
    u64 pos = 0;
    u32 bs = w->h->bs;
    for (u32 i = 0; i < f->n && len; i++) {
        u64 bytes = (u64)f->ext[i].count * bs;
        if (off < pos + bytes) {
            u64 in = off - pos, n = MIN(len, bytes - in);
            if (!tx_add(w, (u64)f->ext[i].start * bs + in, data, (u32)n)) return false;
            data += n; off += n; len -= n;
        }
        pos += bytes;
    }
    return len == 0;
}

/* ---- journal: E/S circular dentro da área [jhdr_size, jsize) */
static bool jio(hfs_wr_t *w, u64 *pos, u8 *buf, u64 len, bool wr) {
    while (len) {
        if (*pos >= w->jsize) *pos = w->jhdr_size;
        u64 n = MIN(len, w->jsize - *pos);
        bool ok = wr ? blk_write(w->h->dev, w->joff + *pos, buf, n) : blk_read(w->h->dev, w->joff + *pos, buf, n);
        if (!ok) return false;
        *pos += n; buf += n; len -= n;
        if (*pos >= w->jsize) *pos = w->jhdr_size;
    }
    return true;
}

static u32 jget32(const hfs_wr_t *w, const u8 *p) { return w->jle ? le32(p) : be32(p); }
static u64 jget64(const hfs_wr_t *w, const u8 *p) { return w->jle ? le64(p) : be64(p); }
static void jput32(const hfs_wr_t *w, u8 *p, u32 v) { if (w->jle) lput32(p, v); else put32(p, v); }
static void jput64(const hfs_wr_t *w, u8 *p, u64 v) { if (w->jle) lput64(p, v); else put64(p, v); }
static void jput16(const hfs_wr_t *w, u8 *p, u16 v) { if (w->jle) lput16(p, v); else put16(p, v); }

static bool jhdr_write(hfs_wr_t *w) {
    u8 *b = kcalloc(1, w->jhdr_size);
    if (!b) return false;
    bool ok = blk_read(w->h->dev, w->joff, b, w->jhdr_size);
    jput32(w, b, JNL_MAGIC);
    jput32(w, b + 4, JNL_ENDIAN);
    jput64(w, b + 8, w->jstart);
    jput64(w, b + 16, w->jend);
    jput64(w, b + 24, w->jsize);
    jput32(w, b + 32, w->blhdr_size);
    jput32(w, b + 36, 0);
    jput32(w, b + 40, w->jhdr_size);
    jput32(w, b + 44, w->jseq);
    jput32(w, b + 36, jnl_cksum(b, 44));
    ok = ok && blk_write(w->h->dev, w->joff, b, w->jhdr_size) && blk_flush(w->h->dev);
    kfree(b);
    return ok;
}

/* grava tx[first..first+cnt) como uma transação do journal e depois no lugar */
static bool jnl_commit_chunk(hfs_wr_t *w, int first, int cnt) {
    u8 *bl = kcalloc(1, w->blhdr_size);
    if (!bl) return false;
    u64 bytes = w->blhdr_size;
    for (int i = 0; i < cnt; i++) bytes += w->tx[first + i].len;
    jput16(w, bl, (u16)(w->blhdr_size / 16 - 1));
    jput16(w, bl + 2, (u16)(cnt + 1));
    jput32(w, bl + 4, (u32)bytes);
    jput32(w, bl + 12, BLHDR_FIRST);
    /* binfo[0]: bnum (sem uso no disco), bsize 0, número de sequência da transação */
    w->jseq++;
    jput32(w, bl + 16 + 12, w->jseq);
    for (int i = 0; i < cnt; i++) {
        u8 *bi = bl + 16 + 16 * (i + 1);
        jput64(w, bi, w->tx[first + i].off / w->jhdr_size);
        jput32(w, bi + 8, w->tx[first + i].len);
    }
    jput32(w, bl + 8, jnl_cksum(bl, 32));
    u64 pos = w->jend;
    bool ok = jio(w, &pos, bl, w->blhdr_size, true);
    for (int i = 0; ok && i < cnt; i++) ok = jio(w, &pos, w->tx[first + i].data, w->tx[first + i].len, true);
    kfree(bl);
    if (!ok || !blk_flush(w->h->dev)) return false;
    w->jend = pos;
    if (!jhdr_write(w)) return false;                      /* a partir daqui a transação vale */
    for (int i = 0; ok && i < cnt; i++) ok = blk_write(w->h->dev, w->tx[first + i].off, w->tx[first + i].data, w->tx[first + i].len);
    if (!ok || !blk_flush(w->h->dev)) return false;
    w->jstart = w->jend;                                   /* checkpoint: journal vazio de novo */
    return jhdr_write(w);
}

static bool tx_flush(hfs_wr_t *w) {
    bool ok = true;
    if (!w->jnl) {
        for (int i = 0; ok && i < w->ntx; i++) ok = blk_write(w->h->dev, w->tx[i].off, w->tx[i].data, w->tx[i].len);
        ok = ok && blk_flush(w->h->dev);
    } else {
        int maxb = (int)(w->blhdr_size / 16) - 1;
        u64 room = w->jsize - w->jhdr_size - w->blhdr_size - w->jhdr_size;
        for (int i = 0; ok && i < w->ntx; ) {
            int cnt = 0;
            u64 bytes = 0;
            while (i + cnt < w->ntx && cnt < maxb && bytes + w->tx[i + cnt].len <= room) bytes += w->tx[i + cnt++].len;
            if (!cnt) { ok = false; break; }                /* um bloco maior que o journal inteiro */
            ok = jnl_commit_chunk(w, i, cnt);
            i += cnt;
        }
    }
    tx_clear(w);
    return ok;
}

/* atualiza as raízes vistas pela leitura (hfsplus.c) a partir dos cabeçalhos das árvores */
static void sync_read_side(hfs_wr_t *w) {
    hfs_t *h = w->h;
    u8 *hd;
    if ((hd = bt_hdr(w, T_CAT))) h->cat_root = be32(hd + 2);
    if ((hd = bt_hdr(w, T_EXT))) h->ext_root = be32(hd + 2);
    if (w->bt[T_ATTR].ns && (hd = bt_hdr(w, T_ATTR))) h->attr_root = be32(hd + 2);
    kfree(h->cache);                                       /* conteúdo decmpfs pode ter mudado */
    h->cache = NULL;
    h->cache_len = 0;
    h->cache_ino = 0;
}

static bool tx_commit(hfs_wr_t *w) {
    bool ok = true;
    sync_read_side(w);
    for (nbuf_t *b = w->nodes; ok && b; b = b->next) {
        if (!b->dirty) continue;
        u32 ns = ns_of(w, b->tree);
        ok = tx_add_fork(w, w->bt[b->tree].fork, (u64)b->idx * ns, b->d, ns);
    }
    u32 bs = w->h->bs;
    for (u32 p = 0; ok && p < w->bm_pages; p++) {
        if (!w->bm_dirty[p]) continue;
        u64 off = (u64)p * bs;
        u8 *page = kcalloc(1, bs);
        if (!page) { ok = false; break; }
        memcpy(page, w->bm + off, MIN((u64)bs, (u64)w->bm_bytes - off));
        ok = tx_add_fork(w, &w->alloc, off, page, bs);
        kfree(page);
    }
    if (ok && w->vh_dirty) {
        put32(w->vh + 20, hfs_now());                      /* modifyDate */
        put32(w->vh + 68, be32(w->vh + 68) + 1);           /* writeCount */
        ok = tx_add(w, 1024, w->vh, 512) && tx_add(w, w->vol_bytes - 1024, w->vh, 512);
    }
    ok = ok && tx_flush(w);
    if (ok) {
        memset(w->bm_dirty, 0, w->bm_pages);
        w->vh_dirty = false;
    } else kprintf("hfs+: falha ao gravar a transação\n");
    tx_clear(w);
    nodes_drop(w);
    return ok;
}

/* desfaz uma operação que falhou no meio: nada ainda foi para o disco além de dados em blocos livres */
static void tx_abort(hfs_wr_t *w) {
    tx_clear(w);
    nodes_drop(w);
    u8 vh[512];
    if (blk_read(w->h->dev, 1024, vh, 512)) memcpy(w->vh, vh, 512);
    w->vh_dirty = false;
    u32 bs = w->h->bs;
    for (u32 p = 0; p < w->bm_pages; p++) {
        if (!w->bm_dirty[p]) continue;
        u64 off = (u64)p * bs;
        u8 *page = kmalloc(bs);
        if (page && hfs_fork_read(w->h, &w->alloc, off, page, bs)) memcpy(w->bm + off, page, MIN((u64)bs, (u64)w->bm_bytes - off));
        kfree(page);
        w->bm_dirty[p] = 0;
    }
}

/* ================================================================ B-tree: busca */
typedef struct { u32 node; int idx; } pent_t;
#define MAXDEPTH 16

/* último registro com chave <= key (-1 se key é menor que todos) */
static int node_find(hfs_wr_t *w, int t, const u8 *n, const u8 *key, bool *exact) {
    u32 ns = ns_of(w, t);
    int lo = 0, hi = n_nrec(n) - 1, best = -1;
    *exact = false;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = key_cmp(w, t, n_rec(n, ns, mid, NULL), key);
        if (c <= 0) { best = mid; lo = mid + 1; if (!c) { *exact = true; break; } }
        else hi = mid - 1;
    }
    return best;
}

/* desce da raiz até a folha; path[d-1] = folha com idx = último <= key */
static int bt_descend(hfs_wr_t *w, int t, const u8 *key, pent_t *path, bool *exact) {
    u8 *hd = bt_hdr(w, t);
    if (!hd) return -1;
    u32 node = be32(hd + 2), ns = ns_of(w, t);
    int d = 0;
    *exact = false;
    while (node && d < MAXDEPTH) {
        u8 *n = node_get(w, t, node);
        if (!n) return -1;
        int i = node_find(w, t, n, key, exact);
        path[d++] = (pent_t){node, i};
        if ((i8)n[8] == -1) return d;                      /* folha */
        if ((i8)n[8] != 0 || !n_nrec(n)) return -1;
        if (i < 0) { i = 0; path[d - 1].idx = 0; }
        const u8 *r = n_rec(n, ns, i, NULL);
        node = be32(r + key_bytes(r));
    }
    return d ? -1 : 0;                                     /* árvore vazia: 0 */
}

/* dados de um registro exato; *node_out para marcar sujo */
static u8 *bt_lookup(hfs_wr_t *w, int t, const u8 *key, u32 *dlen, u32 *node_out) {
    if (!ns_of(w, t)) return NULL;
    pent_t path[MAXDEPTH];
    bool exact;
    int d = bt_descend(w, t, key, path, &exact);
    if (d <= 0 || !exact) return NULL;
    u8 *n = node_get(w, t, path[d - 1].node);
    u32 len;
    u8 *r = (u8 *)n_rec(n, ns_of(w, t), path[d - 1].idx, &len);
    u32 kb = key_bytes(r);
    if (dlen) *dlen = len > kb ? len - kb : 0;
    if (node_out) *node_out = path[d - 1].node;
    return r + kb;
}

/* percorre as folhas a partir da primeira chave >= key; cb devolve false para parar.
 * cb não pode alterar a árvore. */
typedef bool (*iter_cb)(hfs_wr_t *w, const u8 *key, const u8 *data, u32 dlen, void *ctx);
static bool bt_iter(hfs_wr_t *w, int t, const u8 *key, iter_cb cb, void *ctx) {
    if (!ns_of(w, t)) return true;
    pent_t path[MAXDEPTH];
    bool exact;
    int d = bt_descend(w, t, key, path, &exact);
    if (d < 0) return false;
    if (!d) return true;
    u32 node = path[d - 1].node, ns = ns_of(w, t);
    int i = path[d - 1].idx;
    if (i < 0) i = 0; else if (!exact) i++;
    for (int guard = 0; node && guard < 1000000; guard++) {
        u8 *n = node_get(w, t, node);
        if (!n) return false;
        for (; i < n_nrec(n); i++) {
            u32 len;
            const u8 *r = n_rec(n, ns, i, &len);
            u32 kb = key_bytes(r);
            if (!cb(w, r, r + kb, len > kb ? len - kb : 0, ctx)) return true;
        }
        node = be32(n);
        i = 0;
    }
    return true;
}

/* ================================================================ B-tree: alteração */
typedef struct { u8 *p; u32 len; } rec_t;

/* registro de índice que aponta para child (chave = 1ª chave do filho) */
static u32 index_rec(hfs_wr_t *w, int t, u32 child, u8 *out) {
    u8 *n = node_get(w, t, child);
    if (!n || !n_nrec(n)) return 0;
    const u8 *r = n_rec(n, ns_of(w, t), 0, NULL);
    u32 kb = key_bytes(r);
    if (w->bt[t].varidx) memcpy(out, r, kb);
    else {
        kb = (2u + w->bt[t].maxkey + 1) & ~1u;
        memset(out, 0, kb);
        memcpy(out, r, MIN(kb, 2u + be16(r)));
        put16(out, w->bt[t].maxkey);
    }
    put32(out + kb, child);
    return kb + 4;
}

/* monta um nó a partir de uma lista de registros (em tmp, depois copia) */
static void node_build(u8 *dst, u32 ns, const u8 *desc, rec_t *r, int cnt) {
    memset(dst, 0, ns);
    memcpy(dst, desc, 10);                                 /* fLink, bLink, kind, height */
    put16(dst + 10, (u16)cnt);
    u32 off = 14;
    for (int i = 0; i < cnt; i++) {
        memcpy(dst + off, r[i].p, r[i].len);
        put16(dst + ns - 2 * (i + 1), (u16)off);
        off += r[i].len;
    }
    put16(dst + ns - 2 * (cnt + 1), (u16)off);
}

static bool fits(u32 ns, rec_t *r, int cnt) {
    u32 s = 14 + 2 * (cnt + 1);
    for (int i = 0; i < cnt; i++) s += r[i].len;
    return s <= ns;
}

/* grava a lista de registros no nó idx; se não couber, divide e devolve o novo irmão à direita */
static bool node_store(hfs_wr_t *w, int t, u32 idx, rec_t *r, int cnt, u32 *sib) {
    u32 ns = ns_of(w, t);
    *sib = 0;
    u8 *n = node_get(w, t, idx);
    u8 *tmp = kmalloc(ns);
    if (!n || !tmp) { kfree(tmp); return false; }
    u8 desc[10];
    memcpy(desc, n, 10);
    if (fits(ns, r, cnt)) {
        node_build(tmp, ns, desc, r, cnt);
        memcpy(n, tmp, ns);
        node_dirty(w, t, idx);
        kfree(tmp);
        return true;
    }
    /* ponto de divisão: o mais equilibrado em bytes em que os dois lados cabem */
    u32 total = 0;
    for (int i = 0; i < cnt; i++) total += r[i].len;
    int best = -1;
    u32 bestdiff = ~0u, acc = 0;
    for (int k = 1; k < cnt; k++) {
        acc += r[k - 1].len;
        if (!fits(ns, r, k) || !fits(ns, r + k, cnt - k)) continue;
        u32 diff = acc > total - acc ? acc - (total - acc) : (total - acc) - acc;
        if (diff < bestdiff) { bestdiff = diff; best = k; }
    }
    u8 *tmp2 = best > 0 ? kmalloc(ns) : NULL;
    u32 nidx = tmp2 ? node_alloc(w, t) : 0;
    if (!nidx) { kfree(tmp); kfree(tmp2); return false; }
    n = node_get(w, t, idx);                                /* node_alloc pode ter lido outros nós */
    u32 old_next = be32(n);
    u8 rdesc[10];
    memcpy(rdesc, desc, 10);
    put32(rdesc, old_next);                                /* fLink do novo = antigo fLink */
    put32(rdesc + 4, idx);                                 /* bLink do novo = nó original */
    put32(desc, nidx);
    node_build(tmp, ns, desc, r, best);
    node_build(tmp2, ns, rdesc, r + best, cnt - best);
    memcpy(n, tmp, ns);
    node_dirty(w, t, idx);
    u8 *nn = node_get(w, t, nidx);
    memcpy(nn, tmp2, ns);
    node_dirty(w, t, nidx);
    kfree(tmp);
    kfree(tmp2);
    if (old_next) {
        u8 *o = node_get(w, t, old_next);
        if (!o) return false;
        put32(o + 4, nidx);
        node_dirty(w, t, old_next);
    }
    u8 *hd = bt_hdr(w, t);
    if ((i8)desc[8] == -1 && be32(hd + 14) == idx) { put32(hd + 14, nidx); hdr_dirty(w, t); }
    *sib = nidx;
    return true;
}

/* copia os registros de um nó para uma lista (buffers próprios; liberar com recs_free) */
static int recs_load(hfs_wr_t *w, int t, u32 idx, rec_t *r, int max) {
    u8 *n = node_get(w, t, idx);
    if (!n) return -1;
    int cnt = n_nrec(n);
    if (cnt > max) return -1;
    for (int i = 0; i < cnt; i++) {
        u32 len;
        const u8 *p = n_rec(n, ns_of(w, t), i, &len);
        r[i].p = kmalloc(len ? len : 1);
        if (!r[i].p) { while (i--) kfree(r[i].p); return -1; }
        memcpy(r[i].p, p, len);
        r[i].len = len;
    }
    return cnt;
}
static void recs_free(rec_t *r, int cnt) { for (int i = 0; i < cnt; i++) kfree(r[i].p); }

static void unlink_node(hfs_wr_t *w, int t, u32 idx) {
    u8 *n = node_get(w, t, idx);
    if (!n) return;
    u32 f = be32(n), b = be32(n + 4);
    bool leaf = (i8)n[8] == -1;
    if (f) { u8 *x = node_get(w, t, f); if (x) { put32(x + 4, b); node_dirty(w, t, f); } }
    if (b) { u8 *x = node_get(w, t, b); if (x) { put32(x, f); node_dirty(w, t, b); } }
    if (leaf) {
        u8 *hd = bt_hdr(w, t);
        if (be32(hd + 10) == idx) put32(hd + 10, f);
        if (be32(hd + 14) == idx) put32(hd + 14, b);
        hdr_dirty(w, t);
    }
    node_free(w, t, idx);
}

#define MAXRECS 1024

/* sobe pela árvore aplicando, no pai, o que mudou no filho path[lvl]:
 * firstchg = a 1ª chave do filho mudou; removed = o filho ficou vazio (sai do pai); sib = novo irmão */
static bool propagate(hfs_wr_t *w, int t, pent_t *path, int lvl, bool firstchg, bool removed, u32 sib) {
    rec_t *r = kmalloc(sizeof(rec_t) * (MAXRECS + 2));
    u8 *ib1 = kmalloc(600), *ib2 = kmalloc(600);
    bool ok = r && ib1 && ib2;
    while (ok && lvl > 0 && (firstchg || removed || sib)) {
        u32 child = path[lvl].node, parent = path[lvl - 1].node;
        int pi = path[lvl - 1].idx;
        int cnt = recs_load(w, t, parent, r, MAXRECS);
        if (cnt < 0 || pi < 0 || pi >= cnt) { ok = false; break; }
        u32 kb = key_bytes(r[pi].p);
        if (be32(r[pi].p + kb) != child) { ok = false; break; }   /* caminho inconsistente */
        bool pfirst = false, premoved = false;
        if (removed) {
            kfree(r[pi].p);
            memmove(r + pi, r + pi + 1, sizeof(rec_t) * (cnt - pi - 1));
            cnt--;
            unlink_node(w, t, child);
            pfirst = pi == 0 && cnt > 0;
            premoved = cnt == 0;
            if (sib) { ok = false; break; }                 /* não acontece: vazio não divide */
        } else {
            if (firstchg) {
                u32 l = index_rec(w, t, child, ib1);
                if (!l) { ok = false; break; }
                kfree(r[pi].p);
                r[pi].p = kmalloc(l);
                if (!r[pi].p) { ok = false; break; }
                memcpy(r[pi].p, ib1, l);
                r[pi].len = l;
                pfirst = pi == 0;
            }
            if (sib) {
                u32 l = index_rec(w, t, sib, ib2);
                u8 *p = l ? kmalloc(l) : NULL;
                if (!p) { ok = false; break; }
                memcpy(p, ib2, l);
                memmove(r + pi + 2, r + pi + 1, sizeof(rec_t) * (cnt - pi - 1));
                r[pi + 1] = (rec_t){p, l};
                cnt++;
            }
        }
        u32 psib = 0;
        if (!premoved) ok = node_store(w, t, parent, r, cnt, &psib);
        recs_free(r, cnt);
        firstchg = pfirst;
        removed = premoved;
        sib = psib;
        lvl--;
    }
    u8 *hd = ok ? bt_hdr(w, t) : NULL;
    if (ok && lvl == 0) {
        u32 root = path[0].node;
        if (removed) {                                     /* árvore ficou vazia */
            u8 *n = node_get(w, t, root);
            bool leaf = n && (i8)n[8] == -1;
            node_free(w, t, root);
            hd = bt_hdr(w, t);
            put32(hd + 2, 0);
            put16(hd, 0);
            if (leaf) { put32(hd + 10, 0); put32(hd + 14, 0); }
            hdr_dirty(w, t);
        } else if (sib) {                                  /* raiz dividiu: nova raiz acima */
            u32 nr = node_alloc(w, t);
            u8 *n = nr ? node_get(w, t, root) : NULL;
            u32 l1 = n ? index_rec(w, t, root, ib1) : 0, l2 = l1 ? index_rec(w, t, sib, ib2) : 0;
            if (!l2) ok = false;
            else {
                u8 desc[10] = {0};
                desc[8] = 0;                               /* índice */
                desc[9] = (u8)(n[9] + 1);
                rec_t rr[2] = {{ib1, l1}, {ib2, l2}};
                u8 *nn = node_get(w, t, nr);
                node_build(nn, ns_of(w, t), desc, rr, 2);
                node_dirty(w, t, nr);
                hd = bt_hdr(w, t);
                put32(hd + 2, nr);
                put16(hd, (u16)(be16(hd) + 1));
                hdr_dirty(w, t);
            }
        }
    }
    /* raiz de índice com um só filho: o filho vira a raiz */
    for (int guard = 0; ok && guard < MAXDEPTH; guard++) {
        hd = bt_hdr(w, t);
        u32 root = be32(hd + 2);
        u8 *n = root ? node_get(w, t, root) : NULL;
        if (!n || (i8)n[8] != 0 || n_nrec(n) != 1) break;
        const u8 *rr = n_rec(n, ns_of(w, t), 0, NULL);
        u32 child = be32(rr + key_bytes(rr));
        node_free(w, t, root);
        hd = bt_hdr(w, t);
        put32(hd + 2, child);
        put16(hd, (u16)(be16(hd) - 1));
        hdr_dirty(w, t);
    }
    kfree(r);
    kfree(ib1);
    kfree(ib2);
    return ok;
}

static bool bt_insert(hfs_wr_t *w, int t, const u8 *key, u32 klen, const u8 *data, u32 dlen) {
    u32 ns = ns_of(w, t);
    if (!ns) return false;
    u32 kb = (klen + 1) & ~1u, rl = kb + ((dlen + 1) & ~1u);
    if (rl + 14 + 4 > ns / 2) return false;
    u8 *rec = kcalloc(1, rl);
    if (!rec) return false;
    memcpy(rec, key, klen);
    memcpy(rec + kb, data, dlen);
    bool ok = false;
    u8 *hd = bt_hdr(w, t);
    if (!hd) goto out;
    if (!be32(hd + 2)) {                                   /* árvore vazia: a 1ª folha é a raiz */
        u32 idx = node_alloc(w, t);
        if (!idx) goto out;
        u8 desc[10] = {0};
        desc[8] = 0xFF;
        desc[9] = 1;
        rec_t r = {rec, rl};
        node_build(node_get(w, t, idx), ns, desc, &r, 1);
        node_dirty(w, t, idx);
        hd = bt_hdr(w, t);
        put32(hd + 2, idx);
        put16(hd, 1);
        put32(hd + 10, idx);
        put32(hd + 14, idx);
        put32(hd + 6, be32(hd + 6) + 1);
        hdr_dirty(w, t);
        ok = true;
        goto out;
    }
    pent_t path[MAXDEPTH];
    bool exact;
    int d = bt_descend(w, t, rec, path, &exact);
    if (d <= 0 || exact) goto out;                          /* já existe */
    rec_t *r = kmalloc(sizeof(rec_t) * (MAXRECS + 1));
    int cnt = r ? recs_load(w, t, path[d - 1].node, r, MAXRECS) : -1;
    if (cnt < 0) { kfree(r); goto out; }
    int pos = path[d - 1].idx + 1;
    memmove(r + pos + 1, r + pos, sizeof(rec_t) * (cnt - pos));
    r[pos] = (rec_t){kmalloc(rl), rl};
    if (!r[pos].p) { r[pos].len = 0; recs_free(r, cnt + 1); kfree(r); goto out; }
    memcpy(r[pos].p, rec, rl);
    cnt++;
    u32 sib = 0;
    ok = node_store(w, t, path[d - 1].node, r, cnt, &sib);
    recs_free(r, cnt);
    kfree(r);
    ok = ok && propagate(w, t, path, d - 1, pos == 0, false, sib);
    if (ok) { hd = bt_hdr(w, t); put32(hd + 6, be32(hd + 6) + 1); hdr_dirty(w, t); }
out:
    kfree(rec);
    return ok;
}

static bool bt_delete(hfs_wr_t *w, int t, const u8 *key) {
    if (!ns_of(w, t)) return false;
    pent_t path[MAXDEPTH];
    bool exact;
    int d = bt_descend(w, t, key, path, &exact);
    if (d <= 0 || !exact) return false;
    rec_t *r = kmalloc(sizeof(rec_t) * (MAXRECS + 1));
    int cnt = r ? recs_load(w, t, path[d - 1].node, r, MAXRECS) : -1;
    if (cnt < 0) { kfree(r); return false; }
    int pos = path[d - 1].idx;
    kfree(r[pos].p);
    memmove(r + pos, r + pos + 1, sizeof(rec_t) * (cnt - pos - 1));
    cnt--;
    bool ok = true, removed = cnt == 0;
    u32 sib = 0;
    if (!removed) ok = node_store(w, t, path[d - 1].node, r, cnt, &sib);
    recs_free(r, cnt);
    kfree(r);
    ok = ok && propagate(w, t, path, d - 1, pos == 0 && !removed, removed, sib);
    if (ok) { u8 *hd = bt_hdr(w, t); put32(hd + 6, be32(hd + 6) - 1); hdr_dirty(w, t); }
    return ok;
}

/* ---- crescimento de um arquivo de B-tree (catálogo, extents, atributos) */
static bool fork_append(hfs_wr_t *w, fork_t *f, u8 *fd, u32 start, u32 count) {
    if (f->n && f->ext[f->n - 1].start + f->ext[f->n - 1].count == start) f->ext[f->n - 1].count += count;
    else {
        if (f->n >= 8) return false;
        ext_t *p = krealloc(f->ext, sizeof(ext_t) * (f->n + 1));
        if (!p) return false;
        f->ext = p;
        f->ext[f->n++] = (ext_t){start, count};
    }
    u32 bs = w->h->bs;
    f->size += (u64)count * bs;
    put64(fd, f->size);
    put32(fd + 12, be32(fd + 12) + count);
    memset(fd + 16, 0, 64);
    for (u32 i = 0; i < f->n; i++) { put32(fd + 16 + i * 8, f->ext[i].start); put32(fd + 20 + i * 8, f->ext[i].count); }
    w->vh_dirty = true;
    return true;
}

static bool bt_grow(hfs_wr_t *w, int t) {
    bt_t *b = &w->bt[t];
    u8 *hd = bt_hdr(w, t);
    if (!hd) return false;
    u32 bs = w->h->bs, ns = b->ns;
    u32 clump = be32(hd + 32);
    if (clump < ns * 8) clump = ns * 8;
    u32 blocks = (clump + bs - 1) / bs;
    u32 total = be32(hd + 22), add = (u32)((u64)blocks * bs / ns);
    /* o mapa de nós precisa ter bits para os novos nós */
    mapseg_t seg[64];
    int k = map_segments(w, t, seg, 64);
    u64 bits = 0;
    for (int i = 0; i < k; i++) bits += (u64)seg[i].len * 8;
    if (total + add > bits) add = bits > total ? (u32)(bits - total) : 0;
    if (!add) { kprintf("hfs+: B-tree %u sem espaço no mapa de nós\n", b->file_id); return false; }
    blocks = (u32)(((u64)add * ns + bs - 1) / bs);
    fork_t *f = b->fork;
    u32 prefer = f->n ? f->ext[f->n - 1].start + f->ext[f->n - 1].count : 0;
    ext_t *e;
    int ne = -1;
    if (prefer && run_at(w, prefer, blocks) >= blocks) {      /* emenda no último extent */
        e = kmalloc(sizeof *e);
        if (e) { *e = (ext_t){prefer, blocks}; blocks_mark(w, prefer, blocks, true); ne = 1; }
    } else ne = blocks_alloc(w, blocks, &e, 0);
    if (ne != 1) {
        if (ne > 1) { for (int i = 0; i < ne; i++) blocks_mark(w, e[i].start, e[i].count, false); }
        kfree(e);
        kprintf("hfs+: sem espaço contíguo para crescer a B-tree %u\n", b->file_id);
        return false;
    }
    /* zera a área nova (fora do journal: eram blocos livres) */
    u8 *z = kcalloc(1, bs);
    bool ok = z != NULL;
    for (u32 i = 0; ok && i < e->count; i++) ok = blk_write(w->h->dev, (u64)(e->start + i) * bs, z, bs);
    kfree(z);
    ok = ok && fork_append(w, f, w->vh + b->vh_off, e->start, e->count);
    if (!ok) { blocks_mark(w, e->start, e->count, false); kfree(e); return false; }
    kfree(e);
    hd = bt_hdr(w, t);
    put32(hd + 22, total + add);
    put32(hd + 26, be32(hd + 26) + add);
    hdr_dirty(w, t);
    return true;
}

/* ================================================================ forks de arquivos */
/* todos os extents de um fork (os 8 do registro + extents overflow) */
typedef struct { u32 file_id; u8 type; ext_t *e; int n, cap; u32 *starts; int ns, scap; } extcol_t;
static bool ext_collect(hfs_wr_t *w, const u8 *key, const u8 *data, u32 dlen, void *ctx) {
    extcol_t *c = ctx;
    if (be32(key + 4) != c->file_id || key[2] != c->type) return false;
    if (c->ns == c->scap) {
        int nc = c->scap ? c->scap * 2 : 8;
        u32 *p = krealloc(c->starts, sizeof(u32) * nc);
        if (!p) return false;
        c->starts = p;
        c->scap = nc;
    }
    c->starts[c->ns++] = be32(key + 8);
    for (int i = 0; i < 8 && dlen >= 64; i++) {
        u32 s = be32(data + i * 8), n = be32(data + i * 8 + 4);
        if (!n) break;
        if (c->n == c->cap) {
            int nc = c->cap ? c->cap * 2 : 16;
            ext_t *p = krealloc(c->e, sizeof(ext_t) * nc);
            if (!p) return false;
            c->e = p;
            c->cap = nc;
        }
        c->e[c->n++] = (ext_t){s, n};
    }
    return true;
}

/* libera os blocos de um fork e apaga os registros de extents overflow dele */
static bool fork_free(hfs_wr_t *w, const u8 *fd, u32 file_id, u8 type) {
    u32 total = be32(fd + 12), have = 0;
    for (int i = 0; i < 8; i++) {
        u32 s = be32(fd + 16 + i * 8), n = be32(fd + 20 + i * 8);
        if (!n) break;
        if (s + n > w->total_blocks) return false;
        blocks_mark(w, s, n, false);
        have += n;
    }
    if (have >= total) return true;
    extcol_t c = {file_id, type, NULL, 0, 0, NULL, 0, 0};
    u8 k[12];
    ext_key(k, type, file_id, 0);
    bool ok = bt_iter(w, T_EXT, k, ext_collect, &c);
    for (int i = 0; ok && i < c.n; i++) {
        if (c.e[i].start + c.e[i].count > w->total_blocks) { ok = false; break; }
        blocks_mark(w, c.e[i].start, c.e[i].count, false);
    }
    for (int i = 0; ok && i < c.ns; i++) { ext_key(k, type, file_id, c.starts[i]); ok = bt_delete(w, T_EXT, k); }
    kfree(c.e);
    kfree(c.starts);
    return ok;
}

/* grava dados em blocos novos e monta o HFSPlusForkData (fd) + registros overflow */
static bool fork_create(hfs_wr_t *w, u32 file_id, u8 type, const u8 *data, u64 len, u8 *fd) {
    u32 bs = w->h->bs;
    memset(fd, 0, 80);
    u64 nb = (len + bs - 1) / bs;
    if (nb > 0xFFFFFFFFull) return false;
    ext_t *e = NULL;
    int ne = blocks_alloc(w, (u32)nb, &e, 0);
    if (ne < 0) { kprintf("hfs+: sem espaço para %llu bytes\n", len); return false; }
    /* conteúdo: blocos inteiros direto no disco; o último completado com zeros */
    bool ok = true;
    u64 done = 0;
    u8 *pad = NULL;
    for (int i = 0; ok && i < ne; i++) {
        u64 ebytes = (u64)e[i].count * bs, n = MIN(ebytes, len - done);
        u64 whole = n / bs * bs;
        if (whole) ok = blk_write(w->h->dev, (u64)e[i].start * bs, data + done, whole);
        if (ok && n > whole) {
            if (!pad) pad = kmalloc(bs);
            ok = pad != NULL;
            if (ok) {
                memset(pad, 0, bs);
                memcpy(pad, data + done + whole, n - whole);
                ok = blk_write(w->h->dev, (u64)e[i].start * bs + whole, pad, bs);
            }
        }
        done += n;
    }
    kfree(pad);
    put64(fd, len);
    put32(fd + 12, (u32)nb);
    for (int i = 0; i < ne && i < 8; i++) { put32(fd + 16 + i * 8, e[i].start); put32(fd + 20 + i * 8, e[i].count); }
    /* extents além dos 8 primeiros: registros de 8 na árvore de extents overflow */
    u32 fblk = 0;
    for (int i = 0; i < 8 && i < ne; i++) fblk += e[i].count;
    for (int i = 8; ok && i < ne; i += 8) {
        u8 k[12], d[64] = {0};
        ext_key(k, type, file_id, fblk);
        for (int j = 0; j < 8 && i + j < ne; j++) { put32(d + j * 8, e[i + j].start); put32(d + j * 8 + 4, e[i + j].count); fblk += e[i + j].count; }
        ok = bt_insert(w, T_EXT, k, 12, d, 64);
    }
    if (!ok) for (int i = 0; i < ne; i++) blocks_mark(w, e[i].start, e[i].count, false);
    kfree(e);
    return ok;
}

/* ================================================================ atributos estendidos */
typedef struct { u32 file_id; u8 (*keys)[300]; int n, cap; } attrcol_t;
static bool attr_collect(hfs_wr_t *w, const u8 *key, const u8 *data, u32 dlen, void *ctx) {
    attrcol_t *c = ctx;
    if (be32(key + 4) != c->file_id) return false;
    u32 kl = 2 + be16(key);
    if (kl > 300) return true;
    if (c->n == c->cap) {
        int nc = c->cap ? c->cap * 2 : 8;
        u8 (*p)[300] = krealloc(c->keys, 300 * nc);
        if (!p) return false;
        c->keys = p;
        c->cap = nc;
    }
    memset(c->keys[c->n], 0, 300);
    memcpy(c->keys[c->n++], key, kl);
    (void)dlen; (void)data;
    return true;
}

/* apaga atributos de file_id (todos, ou só o de nome "only"); libera os forks dos grandes */
static bool attrs_delete(hfs_wr_t *w, u32 file_id, const char *only) {
    if (!w->bt[T_ATTR].ns || !w->h->attr_root) return true;
    u8 k[300] = {0};
    put16(k, 12);
    put32(k + 4, file_id);
    attrcol_t c = {file_id, NULL, 0, 0};
    bool ok = bt_iter(w, T_ATTR, k, attr_collect, &c);
    for (int i = 0; ok && i < c.n; i++) {
        u8 *key = c.keys[i];
        if (only) {
            u32 nl = be16(key + 12), ol = (u32)strlen(only);
            bool same = nl == ol;
            for (u32 j = 0; same && j < nl; j++) same = be16(key + 14 + j * 2) == (u8)only[j];
            if (!same) continue;
        }
        u32 dlen;
        u8 *d = bt_lookup(w, T_ATTR, key, &dlen, NULL);
        if (d && dlen >= 88 && be32(d) == 0x20) {          /* atributo em fork próprio */
            u8 fd[80];
            memcpy(fd, d + 8, 80);
            for (int j = 0; j < 8; j++) {
                u32 s = be32(fd + 16 + j * 8), n = be32(fd + 20 + j * 8);
                if (n && s + n <= w->total_blocks) blocks_mark(w, s, n, false);
            }
        } else if (d && dlen >= 72 && be32(d) == 0x30) {   /* extents adicionais de um atributo */
            for (int j = 0; j < 8; j++) {
                u32 s = be32(d + 8 + j * 8), n = be32(d + 12 + j * 8);
                if (n && s + n <= w->total_blocks) blocks_mark(w, s, n, false);
            }
        }
        ok = bt_delete(w, T_ATTR, key);
    }
    kfree(c.keys);
    return ok;
}

/* ================================================================ catálogo */
typedef struct { u8 d[520]; u32 len; } catrec_t;

static bool cat_get(hfs_wr_t *w, u32 parent, const u16 *name, int n, catrec_t *out) {
    u8 k[520];
    cat_key(k, parent, name, n);
    u32 dlen;
    u8 *d = bt_lookup(w, T_CAT, k, &dlen, NULL);
    if (!d || dlen > sizeof out->d) return false;
    memcpy(out->d, d, dlen);
    out->len = dlen;
    return true;
}

/* registro de pasta/arquivo pelo CNID (via thread) */
static bool cat_by_id(hfs_wr_t *w, u32 cnid, u8 *key_out, u32 *klen, catrec_t *rec) {
    catrec_t th;
    if (!cat_get(w, cnid, NULL, 0, &th) || th.len < 10) return false;
    u16 type = be16(th.d);
    if (type != REC_FOLDER_THREAD && type != REC_FILE_THREAD) return false;
    u32 n = MIN(be16(th.d + 8), 255u);
    u16 nm[255];
    for (u32 i = 0; i < n; i++) nm[i] = be16(th.d + 10 + i * 2);
    u32 kl = cat_key(key_out, be32(th.d + 4), nm, (int)n);
    if (klen) *klen = kl;
    return !rec || cat_get(w, be32(th.d + 4), nm, (int)n, rec);
}

/* atualiza valência (+dv), contagem de pastas (+df) e datas de uma pasta */
static bool folder_adjust(hfs_wr_t *w, u32 cnid, int dv, int df) {
    u8 k[520];
    u32 kl, dlen, node;
    if (!cat_by_id(w, cnid, k, &kl, NULL)) return false;
    u8 *d = bt_lookup(w, T_CAT, k, &dlen, &node);
    if (!d || dlen < 88 || be16(d) != REC_FOLDER) return false;
    put32(d + 4, (u32)((i32)be32(d + 4) + dv));
    if (df && (be16(d + 2) & F_HAS_FOLDERCOUNT)) put32(d + 84, (u32)((i32)be32(d + 84) + df));
    u32 now = hfs_now();
    put32(d + 16, now);                                    /* contentModDate */
    put32(d + 20, now);                                    /* attributeModDate */
    node_dirty(w, T_CAT, node);
    return true;
}

static void bsd_info(u8 *p, u16 mode) {
    put32(p, 99);                                          /* _unknown: o macOS mostra como do usuário atual */
    put32(p + 4, 99);
    p[8] = 0;
    p[9] = 0;
    put16(p + 10, mode);
    put32(p + 12, 0);
}

static bool thread_insert(hfs_wr_t *w, u32 cnid, u16 type, u32 parent, const u16 *name, int n) {
    u8 k[8], d[10 + 2 * 255];
    cat_key(k, cnid, NULL, 0);
    put16(d, type);
    put16(d + 2, 0);
    put32(d + 4, parent);
    put16(d + 8, (u16)n);
    for (int i = 0; i < n; i++) put16(d + 10 + i * 2, name[i]);
    return bt_insert(w, T_CAT, k, 8, d, 10 + 2 * (u32)n);
}

static u32 new_cnid(hfs_wr_t *w) {
    u32 id = be32(w->vh + 64);
    if (id < 16 || id == 0xFFFFFFFFu) return 0;
    put32(w->vh + 64, id + 1);
    w->vh_dirty = true;
    return id;
}

static void vh_count(hfs_wr_t *w, int files, int folders) {
    put32(w->vh + 32, (u32)((i32)be32(w->vh + 32) + files));
    put32(w->vh + 36, (u32)((i32)be32(w->vh + 36) + folders));
    w->vh_dirty = true;
}

/* caminho -> CNID da pasta pai e nome final (UTF-16) */
static bool resolve_parent(hfs_wr_t *w, const char *path, u32 *parent, u16 *leaf, int *ln) {
    u32 cur = CNID_ROOT;
    const char *p = path;
    char part[768];
    for (;;) {
        while (*p == '/') p++;
        if (!*p) return false;
        int n = 0;
        while (*p && *p != '/') { if (n < (int)sizeof part - 1) part[n++] = *p; p++; }
        part[n] = 0;
        while (*p == '/') p++;
        u16 nm[255];
        int k = name16(part, nm);
        if (k < 0) return false;
        if (!*p) { *parent = cur; memcpy(leaf, nm, sizeof(u16) * k); *ln = k; return true; }
        catrec_t r;
        if (!cat_get(w, cur, nm, k, &r) || be16(r.d) != REC_FOLDER || r.len < 88) return false;
        cur = be32(r.d + 8);
    }
}

/* pasta privada dos hard links ("\0\0\0\0HFS+ Private Data" na raiz) */
static u32 find_priv_dir(hfs_wr_t *w) {
    static const char tail[] = "HFS+ Private Data";
    u16 nm[4 + sizeof tail];
    int n = 0;
    for (int i = 0; i < 4; i++) nm[n++] = 0;
    for (const char *s = tail; *s; s++) nm[n++] = (u8)*s;
    catrec_t r;
    if (!cat_get(w, CNID_ROOT, nm, n, &r) || be16(r.d) != REC_FOLDER) return 0;
    return be32(r.d + 8);
}

/* ================================================================ operações */
static hfs_wr_t *wr_of(dmount_t *m) { hfs_t *h = m->priv; return h ? h->wr : NULL; }

/* hard link (arquivo 'hlnk'/'hfs+'): a chave do inode na pasta privada */
static bool hardlink_inode_key(hfs_wr_t *w, const u8 *rec, u8 *k) {
    if (be32(rec + 48) != FOURCC('h', 'l', 'n', 'k') || be32(rec + 52) != FOURCC('h', 'f', 's', '+')) return false;
    if (!w->priv_dir) w->priv_dir = find_priv_dir(w);
    if (!w->priv_dir) return false;
    char nm[32];
    snprintf(nm, sizeof nm, "iNode%u", be32(rec + 44));
    u16 n16[32];
    int n = 0;
    for (const char *s = nm; *s; s++) n16[n++] = (u8)*s;
    cat_key(k, w->priv_dir, n16, n);
    return true;
}

bool hfs_wr_write(dmount_t *m, const char *path, const void *data, u64 len) {
    hfs_wr_t *w = wr_of(m);
    if (!w) return false;
    u32 parent;
    u16 leaf[255];
    int ln;
    if (!resolve_parent(w, path, &parent, leaf, &ln)) { tx_abort(w); return false; }
    u8 k[520];
    u32 kl = cat_key(k, parent, leaf, ln), dlen, node;
    u8 *d = bt_lookup(w, T_CAT, k, &dlen, &node);
    bool ok;
    if (d) {
        if (be16(d) != REC_FILE || dlen < 248) { tx_abort(w); return false; }
        u8 ik[520];
        if (hardlink_inode_key(w, d, ik)) {                /* grava no inode do hard link */
            memcpy(k, ik, sizeof ik);
            d = bt_lookup(w, T_CAT, k, &dlen, &node);
            if (!d || be16(d) != REC_FILE || dlen < 248) { tx_abort(w); return false; }
        }
        u32 id = be32(d + 8);
        u8 old[80], rsrc[80], nfd[80];
        memcpy(old, d + 88, 80);
        memcpy(rsrc, d + 168, 80);
        bool compressed = d[41] & UF_COMPRESSED;
        /* blocos novos antes de soltar os antigos: o arquivo velho sobrevive a uma falha */
        ok = fork_create(w, id, 0, data, len, nfd);
        if (ok && compressed) {                            /* deixa de ser comprimido (decmpfs) */
            ok = attrs_delete(w, id, "com.apple.decmpfs") && fork_free(w, rsrc, id, 0xFF);
            memset(rsrc, 0, 80);
        }
        ok = ok && fork_free(w, old, id, 0);
        d = ok ? bt_lookup(w, T_CAT, k, &dlen, &node) : NULL;
        if (d) {
            memcpy(d + 88, nfd, 80);
            memcpy(d + 168, rsrc, 80);
            if (compressed) d[41] &= (u8)~UF_COMPRESSED;
            u32 now = hfs_now();
            put32(d + 16, now);
            put32(d + 20, now);
            node_dirty(w, T_CAT, node);
        } else ok = false;
    } else {
        u32 id = new_cnid(w);
        u8 rec[248] = {0};
        ok = id && fork_create(w, id, 0, data, len, rec + 88);
        if (ok) {
            u32 now = hfs_now();
            put16(rec, REC_FILE);
            put16(rec + 2, F_THREAD_EXISTS);
            put32(rec + 8, id);
            put32(rec + 12, now); put32(rec + 16, now); put32(rec + 20, now); put32(rec + 24, now);
            bsd_info(rec + 32, 0100644);
            ok = bt_insert(w, T_CAT, k, kl, rec, 248) && thread_insert(w, id, REC_FILE_THREAD, parent, leaf, ln) &&
                 folder_adjust(w, parent, +1, 0);
            vh_count(w, +1, 0);
        }
    }
    if (!ok) { tx_abort(w); return false; }
    return tx_commit(w);
}

bool hfs_wr_mkdir(dmount_t *m, const char *path) {
    hfs_wr_t *w = wr_of(m);
    if (!w) return false;
    u32 parent;
    u16 leaf[255];
    int ln;
    if (!resolve_parent(w, path, &parent, leaf, &ln)) { tx_abort(w); return false; }
    catrec_t ex;
    if (cat_get(w, parent, leaf, ln, &ex)) { nodes_drop(w); return be16(ex.d) == REC_FOLDER; }
    u8 k[520];
    u32 kl = cat_key(k, parent, leaf, ln);
    u32 id = new_cnid(w);
    u8 rec[88] = {0};
    u32 now = hfs_now();
    put16(rec, REC_FOLDER);
    put16(rec + 2, w->use_fc ? F_HAS_FOLDERCOUNT : 0);
    put32(rec + 8, id);
    put32(rec + 12, now); put32(rec + 16, now); put32(rec + 20, now); put32(rec + 24, now);
    bsd_info(rec + 32, 040755);
    bool ok = id && bt_insert(w, T_CAT, k, kl, rec, 88) && thread_insert(w, id, REC_FOLDER_THREAD, parent, leaf, ln) &&
              folder_adjust(w, parent, +1, +1);
    vh_count(w, 0, +1);
    if (!ok) { tx_abort(w); return false; }
    return tx_commit(w);
}

/* ---- remoção */
typedef struct { u32 parent; u16 (*names)[256]; int n, cap; } kids_t;
static bool kids_collect(hfs_wr_t *w, const u8 *key, const u8 *data, u32 dlen, void *ctx) {
    kids_t *c = ctx;
    if (be32(key + 2) != c->parent) return false;
    u32 nl = be16(key + 6);
    if (!nl || nl > 255) return true;                      /* o próprio thread da pasta */
    if (c->n == c->cap) {
        int nc = c->cap ? c->cap * 2 : 16;
        u16 (*p)[256] = krealloc(c->names, sizeof(u16) * 256 * nc);
        if (!p) return false;
        c->names = p;
        c->cap = nc;
    }
    c->names[c->n][0] = (u16)nl;
    for (u32 i = 0; i < nl; i++) c->names[c->n][1 + i] = be16(key + 8 + i * 2);
    c->n++;
    (void)data; (void)dlen;
    return true;
}

static bool remove_entry(hfs_wr_t *w, u32 parent, const u16 *name, int n, int depth);

static bool remove_children(hfs_wr_t *w, u32 cnid, int depth) {
    if (depth > 64) return false;
    kids_t c = {cnid, NULL, 0, 0};
    u8 k[8];
    cat_key(k, cnid, NULL, 0);
    bool ok = bt_iter(w, T_CAT, k, kids_collect, &c);
    for (int i = 0; ok && i < c.n; i++) {
        ok = remove_entry(w, cnid, c.names[i] + 1, c.names[i][0], depth + 1);
        /* pastas grandes: grava aos pedaços (cada filho removido já deixa tudo consistente) */
        if (ok && w->ndirty > 192) ok = tx_commit(w);
    }
    kfree(c.names);
    return ok;
}

static bool remove_entry(hfs_wr_t *w, u32 parent, const u16 *name, int n, int depth) {
    catrec_t r;
    if (!cat_get(w, parent, name, n, &r)) return false;
    u8 k[520];
    cat_key(k, parent, name, n);
    u16 type = be16(r.d);
    if (type == REC_FOLDER && r.len >= 88) {
        u32 id = be32(r.d + 8);
        if (!remove_children(w, id, depth)) return false;
        u8 tk[8];
        cat_key(tk, id, NULL, 0);
        bool ok = attrs_delete(w, id, NULL) && bt_delete(w, T_CAT, k) && bt_delete(w, T_CAT, tk) && folder_adjust(w, parent, -1, -1);
        vh_count(w, 0, -1);
        return ok;
    }
    if (type != REC_FILE || r.len < 248) return false;
    u32 id = be32(r.d + 8);
    u8 tk[8];
    cat_key(tk, id, NULL, 0);
    u8 ik[520];
    if (hardlink_inode_key(w, r.d, ik)) {
        /* hard link: tira o link; o inode some quando o último link sai (sem cadeia de links) */
        u32 dlen, node;
        u8 *inode = bt_lookup(w, T_CAT, ik, &dlen, &node);
        if (!inode || dlen < 248 || (be16(inode + 2) & F_HAS_LINKCHAIN) || (be16(r.d + 2) & F_HAS_LINKCHAIN)) {
            kprintf("hfs+: hard link com cadeia de links: remoção não suportada\n");
            return false;
        }
        u32 links = be32(inode + 44);
        bool ok = attrs_delete(w, id, NULL) && bt_delete(w, T_CAT, k) && bt_delete(w, T_CAT, tk) && folder_adjust(w, parent, -1, 0);
        vh_count(w, -1, 0);
        if (!ok) return false;
        if (links > 1) {
            inode = bt_lookup(w, T_CAT, ik, &dlen, &node);
            if (!inode) return false;
            put32(inode + 44, links - 1);
            node_dirty(w, T_CAT, node);
            return true;
        }
        u16 nm[255];
        int nn = MIN(be16(ik + 6), 255);
        for (int i = 0; i < nn; i++) nm[i] = be16(ik + 8 + i * 2);
        return remove_entry(w, w->priv_dir, nm, nn, depth + 1);
    }
    bool ok = fork_free(w, r.d + 88, id, 0) && fork_free(w, r.d + 168, id, 0xFF) && attrs_delete(w, id, NULL) &&
              bt_delete(w, T_CAT, k) && bt_delete(w, T_CAT, tk) && folder_adjust(w, parent, -1, 0);
    vh_count(w, -1, 0);
    return ok;
}

bool hfs_wr_remove(dmount_t *m, const char *path) {
    hfs_wr_t *w = wr_of(m);
    if (!w) return false;
    u32 parent;
    u16 leaf[255];
    int ln;
    if (!resolve_parent(w, path, &parent, leaf, &ln) || !remove_entry(w, parent, leaf, ln, 0)) { tx_abort(w); return false; }
    return tx_commit(w);
}

bool hfs_wr_rename(dmount_t *m, const char *from, const char *to) {
    hfs_wr_t *w = wr_of(m);
    if (!w) return false;
    u32 p1, p2;
    u16 n1[255], n2[255];
    int l1, l2;
    catrec_t r, ex;
    if (!resolve_parent(w, from, &p1, n1, &l1) || !resolve_parent(w, to, &p2, n2, &l2) || !cat_get(w, p1, n1, l1, &r)) {
        tx_abort(w);
        return false;
    }
    u16 type = be16(r.d);
    if ((type != REC_FOLDER && type != REC_FILE) || r.len < 88) { tx_abort(w); return false; }
    u32 id = be32(r.d + 8);
    /* destino ocupado por outro item (só a caixa diferente = o mesmo item) */
    if (cat_get(w, p2, n2, l2, &ex) && be32(ex.d + 8) != id) { tx_abort(w); return false; }
    /* não deixa mover uma pasta para dentro dela mesma */
    if (type == REC_FOLDER) {
        u32 cur = p2;
        for (int guard = 0; cur > CNID_ROOT && guard < 256; guard++) {
            if (cur == id) { tx_abort(w); return false; }
            catrec_t th;
            if (!cat_get(w, cur, NULL, 0, &th)) break;
            cur = be32(th.d + 4);
        }
    }
    u8 k1[520], k2[520], tk[8];
    cat_key(k1, p1, n1, l1);
    u32 kl2 = cat_key(k2, p2, n2, l2);
    cat_key(tk, id, NULL, 0);
    put32(r.d + 20, hfs_now());                            /* attributeModDate */
    bool ok = bt_delete(w, T_CAT, k1) && bt_insert(w, T_CAT, k2, kl2, r.d, r.len) && bt_delete(w, T_CAT, tk) &&
              thread_insert(w, id, type == REC_FOLDER ? REC_FOLDER_THREAD : REC_FILE_THREAD, p2, n2, l2);
    int df = type == REC_FOLDER ? 1 : 0;
    if (ok && p1 != p2) ok = folder_adjust(w, p1, -1, -df) && folder_adjust(w, p2, +1, +df);
    else if (ok) ok = folder_adjust(w, p1, 0, 0);
    if (!ok) { tx_abort(w); return false; }
    return tx_commit(w);
}

bool hfs_wr_sync(dmount_t *m) {
    hfs_wr_t *w = wr_of(m);
    if (!w) return true;
    bool ok = true;
    if (w->ndirty || w->vh_dirty) ok = tx_commit(w);
    return blk_flush(w->h->dev) && ok;
}

/* ================================================================ journal: replay e abertura */
typedef struct { u64 joff, jsize; u32 flags; } jib_t;

static bool read_jib(blkdev_t *dev, const u8 *vh, jib_t *j) {
    u32 bs = be32(vh + 40), blk = be32(vh + 12);
    u8 b[512];
    if (!blk || !blk_read(dev, (u64)blk * bs, b, sizeof b)) return false;
    j->flags = be32(b);
    j->joff = be64(b + 36);
    j->jsize = be64(b + 44);
    return true;
}

/* lê o cabeçalho do journal para w (endianness, posições); false se inválido */
static bool jhdr_read(hfs_wr_t *w) {
    u8 b[512];
    if (!blk_read(w->h->dev, w->joff, b, sizeof b)) return false;
    if (le32(b) == JNL_MAGIC && le32(b + 4) == JNL_ENDIAN) w->jle = true;
    else if (be32(b) == JNL_MAGIC && be32(b + 4) == JNL_ENDIAN) w->jle = false;
    else return false;
    u32 ck = jget32(w, b + 36);
    jput32(w, b + 36, 0);
    if (jnl_cksum(b, 44) != ck) return false;
    w->jstart = jget64(w, b + 8);
    w->jend = jget64(w, b + 16);
    w->jsize = jget64(w, b + 24);
    w->blhdr_size = jget32(w, b + 32);
    w->jhdr_size = jget32(w, b + 40);
    w->jseq = jget32(w, b + 44);
    return w->jhdr_size >= 512 && w->jhdr_size <= 65536 && !(w->jhdr_size & (w->jhdr_size - 1)) &&
           w->blhdr_size >= w->jhdr_size && w->blhdr_size % w->jhdr_size == 0 && w->blhdr_size <= (1u << 20) &&
           w->jsize > w->blhdr_size * 2 && w->jstart >= w->jhdr_size && w->jstart <= w->jsize &&
           w->jend >= w->jhdr_size && w->jend <= w->jsize;
}

/* repete as transações pendentes no disco; devolve o nº de listas de blocos aplicadas ou -1 */
static int jnl_replay(hfs_wr_t *w) {
    u8 *bl = kmalloc(w->blhdr_size);
    if (!bl) return -1;
    int applied = 0;
    u64 pos = w->jstart;
    if (pos >= w->jsize) pos = w->jhdr_size;
    u64 end = w->jend >= w->jsize ? w->jhdr_size : w->jend;
    for (int guard = 0; pos != end && guard < 100000; guard++) {
        u64 hpos = pos;
        if (!jio(w, &pos, bl, w->blhdr_size, false)) { applied = -1; break; }
        u32 ck = jget32(w, bl + 8);
        jput32(w, bl + 8, 0);
        if (jnl_cksum(bl, 32) != ck) { kprintf("hfs+: journal: lista de blocos com checksum ruim em %llu\n", hpos); applied = -1; break; }
        u32 maxb = w->jle ? le16(bl) : be16(bl), num = w->jle ? le16(bl + 2) : be16(bl + 2);
        u32 used = jget32(w, bl + 4);
        if (!num || num > maxb || (u64)(num) * 16 > w->blhdr_size || used < w->blhdr_size || used > w->jsize) { applied = -1; break; }
        for (u32 i = 1; i < num; i++) {
            const u8 *bi = bl + 16 * i;
            u64 bnum = jget64(w, bi);
            u32 bsize = jget32(w, bi + 8);
            if (bsize > (16u << 20)) { applied = -1; break; }
            u8 *data = kmalloc(bsize ? bsize : 1);
            if (!data || !jio(w, &pos, data, bsize, false)) { kfree(data); applied = -1; break; }
            if (bnum != ~0ull) {                           /* -1 = bloco "morto" */
                u64 off = bnum * w->jhdr_size;
                if (off + bsize > w->h->dev->size || !blk_write(w->h->dev, off, data, bsize)) { kfree(data); applied = -1; break; }
            }
            kfree(data);
        }
        if (applied < 0) break;
        /* a próxima lista começa depois de bytes_used (pode haver folga) */
        pos = hpos;
        u64 skip = used;
        while (skip) {
            u64 n = MIN(skip, w->jsize - pos);
            pos += n; skip -= n;
            if (pos >= w->jsize) pos = w->jhdr_size;
        }
        applied++;
    }
    kfree(bl);
    if (applied < 0) return -1;
    if (!blk_flush(w->h->dev)) return -1;
    w->jstart = w->jend;
    return jhdr_write(w) ? applied : -1;
}

bool hfs_journal_prepare(blkdev_t *dev, const u8 *vh, bool writable, bool *ro_out) {
    *ro_out = false;
    jib_t j;
    if (!read_jib(dev, vh, &j)) { kprintf("hfs+: journal: bloco de informações ilegível; montado só leitura\n"); *ro_out = true; return true; }
    if ((j.flags & JI_OTHER_DEV) || !(j.flags & JI_IN_FS)) {
        kprintf("hfs+: journal em outro dispositivo: montado só leitura\n");
        *ro_out = true;
        return true;
    }
    if (j.flags & JI_NEED_INIT) return true;               /* journal novo: hfs_wr_open inicializa */
    hfs_t h = {0};
    hfs_wr_t w = {0};
    h.dev = dev;
    w.h = &h;
    w.joff = j.joff;
    w.jsize = j.jsize;
    if (j.joff + j.jsize > dev->size || !jhdr_read(&w)) {
        kprintf("hfs+: journal com cabeçalho inválido: montado só leitura\n");
        *ro_out = true;
        return true;
    }
    if (be32(vh + 8) != FOURCC('H', 'F', 'S', 'J')) return true;   /* montado por quem não usa journal: vale o disco */
    if (w.jstart == w.jend) return true;                   /* limpo */
    if (!writable) {
        kprintf("hfs+: journal com transações pendentes num disco só leitura: montado só leitura sem replay\n");
        *ro_out = true;
        return true;
    }
    int n = jnl_replay(&w);
    if (n < 0) {
        kprintf("hfs+: falha ao repetir o journal: montado só leitura\n");
        *ro_out = true;
        return true;
    }
    kprintf("hfs+: journal: %d lista(s) de blocos repetida(s)\n", n);
    return true;
}

static void wr_free(hfs_wr_t *w) {
    if (!w) return;
    nodes_drop(w);
    tx_clear(w);
    kfree(w->tx);
    kfree(w->bm);
    kfree(w->bm_dirty);
    kfree(w->alloc.ext);
    kfree(w);
}

static bool bt_setup(hfs_wr_t *w, int t, fork_t *f, u32 vh_off, u32 file_id) {
    bt_t *b = &w->bt[t];
    b->fork = f;
    b->vh_off = vh_off;
    b->file_id = file_id;
    if (!f->size) return t == T_ATTR;                      /* sem árvore de atributos: tudo bem */
    u8 hdr[512];
    if (!hfs_fork_read(w->h, f, 0, hdr, sizeof hdr)) return false;
    const u8 *hd = hdr + 14;
    b->ns = be16(hd + 18);
    b->maxkey = be16(hd + 20);
    u32 attrs = be32(hd + 38);
    b->varidx = (attrs & 0x4) != 0;
    if (!(attrs & 0x2)) return false;                      /* sem "big keys": não é HFS+ */
    if (t == T_CAT && hd[37] == 0xBC) w->binary = true;    /* HFSX com comparação binária */
    return b->ns >= 512 && b->ns <= 32768 && !(b->ns & (b->ns - 1)) && (u64)be32(hd + 22) * b->ns <= f->size;
}

bool hfs_wr_open(hfs_t *h, const u8 *vh) {
    u32 attr = be32(vh + 4);
    u16 ver = be16(vh + 2);
    const char *why = NULL;
    if (attr & (VA_HWLOCK | VA_SWLOCK)) why = "travado";
    else if (!(attr & VA_JOURNALED) && (!(attr & VA_UNMOUNTED) || (attr & VA_INCONSIST)))
        why = "não foi desmontado corretamente (rode fsck_hfs)";
    else if (ver != 4 && ver != 5) why = "versão desconhecida";
    if (why) { kprintf("hfs+: volume %s: montado só leitura\n", why); return false; }
    hfs_wr_t *w = kcalloc(1, sizeof *w);
    if (!w) return false;
    w->h = h;
    memcpy(w->vh, vh, 512);
    w->total_blocks = be32(vh + 44);
    w->vol_bytes = (u64)w->total_blocks * h->bs;
    bool ok = w->vol_bytes <= h->dev->size && w->vol_bytes >= 4096;
    ok = ok && hfs_fork_load(h, vh + 112, 6, 0, &w->alloc);
    w->bm_bytes = (w->total_blocks + 7) / 8;
    ok = ok && w->bm_bytes <= MAX_BITMAP && w->alloc.size >= w->bm_bytes;
    w->bm_pages = (w->bm_bytes + h->bs - 1) / h->bs;
    w->bm = ok ? kmalloc((u64)w->bm_pages * h->bs) : NULL;
    w->bm_dirty = ok ? kcalloc(1, w->bm_pages) : NULL;
    ok = ok && w->bm && w->bm_dirty && hfs_fork_read(h, &w->alloc, 0, w->bm, (u64)w->bm_pages * h->bs);
    ok = ok && bt_setup(w, T_CAT, &h->cat, 272, 4) && bt_setup(w, T_EXT, &h->ext, 192, 3) && bt_setup(w, T_ATTR, &h->attr, 352, 8);
    if (ok) {                                              /* contagem de subpastas: segue o que a raiz usa */
        catrec_t root;
        u8 k[520];
        if (cat_by_id(w, CNID_ROOT, k, NULL, &root) && be16(root.d) == REC_FOLDER) w->use_fc = (be16(root.d + 2) & F_HAS_FOLDERCOUNT) != 0;
        else ok = false;
    }
    if (ok && (attr & VA_JOURNALED)) {
        jib_t j;
        ok = read_jib(h->dev, vh, &j) && (j.flags & JI_IN_FS) && !(j.flags & JI_OTHER_DEV);
        w->joff = j.joff;
        w->jsize = j.jsize;
        ok = ok && j.joff + j.jsize <= h->dev->size;
        if (ok && ((j.flags & JI_NEED_INIT) || !jhdr_read(w) || be32(vh + 8) != FOURCC('H', 'F', 'S', 'J'))) {
            /* journal novo, inválido ou deixado por quem não usa journal: recomeça vazio */
            w->jle = true;
            w->jhdr_size = 512;
            w->blhdr_size = 4096;
            w->jsize = j.jsize;
            w->jstart = w->jend = w->jhdr_size;
            ok = w->jsize > 4 * w->blhdr_size && jhdr_write(w);
            if (ok && (j.flags & JI_NEED_INIT)) {          /* tira o "precisa inicializar" */
                u32 bs = h->bs, blk = be32(vh + 12);
                u8 b[512];
                ok = blk_read(h->dev, (u64)blk * bs, b, 512);
                put32(b, j.flags & ~JI_NEED_INIT);
                ok = ok && blk_write(h->dev, (u64)blk * bs, b, 512) && blk_flush(h->dev);
            }
        } else if (ok && w->jstart != w->jend) ok = false; /* hfs_journal_prepare já devia ter repetido */
        w->jnl = ok;
    }
    nodes_drop(w);
    if (!ok) { kprintf("hfs+: estruturas inesperadas: montado só leitura\n"); wr_free(w); return false; }
    /* montado para escrita: "não desmontado" até hfs_wr_close; quem usa journal se identifica como HFSJ */
    put32(w->vh + 4, (attr & ~VA_UNMOUNTED) & ~VA_INCONSIST);
    put32(w->vh + 8, w->jnl ? FOURCC('H', 'F', 'S', 'J') : FOURCC('1', '0', '.', '0'));
    w->vh_dirty = true;
    h->wr = w;
    if (!tx_commit(w)) { h->wr = NULL; wr_free(w); return false; }
    kprintf("hfs+: gravável (%s, %u blocos de %u bytes, %u livres%s)\n", ver == 5 ? "HFSX" : "HFS+", w->total_blocks, h->bs,
            be32(w->vh + 48), w->jnl ? ", com journal" : "");
    return true;
}

void hfs_wr_close(hfs_t *h) {
    hfs_wr_t *w = h->wr;
    if (!w) return;
    if (w->ndirty) tx_commit(w);
    put32(w->vh + 4, be32(w->vh + 4) | VA_UNMOUNTED);
    w->vh_dirty = true;
    if (!tx_commit(w)) kprintf("hfs+: falha ao marcar o volume como desmontado\n");
    h->wr = NULL;
    wr_free(w);
}
