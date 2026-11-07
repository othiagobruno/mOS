/*
 * mOS — estruturas internas do driver HFS+/HFSX, compartilhadas entre a leitura (hfsplus.c) e a
 * escrita (hfsplus_wr.c). Referência do formato: Apple TN1150 ("HFS Plus Volume Format").
 */
#pragma once
#include "fs.h"

#define HFS_EPOCH_DELTA 2082844800ll          /* segundos entre 1904-01-01 e 1970-01-01 */
#define FOURCC(a, b, c, d) ((u32)(a) << 24 | (u32)(b) << 16 | (u32)(c) << 8 | (u32)(d))

typedef struct { u32 start, count; } ext_t;
typedef struct { u64 size; u32 n; ext_t *ext; } fork_t;

struct hfs_wr;

typedef struct {
    blkdev_t *dev;
    u32 bs;
    fork_t cat, ext, attr;
    u32 cat_ns, ext_ns, attr_ns;
    u32 cat_root, ext_root, attr_root;
    u32 priv_dir;            /* "\0\0\0\0HFS+ Private Data" (hard links) */
    u8 *node;                /* buffer de nó */
    bool own_dev;            /* dev é um slice criado aqui (wrapper HFS) */
    u64 cache_ino;           /* último arquivo decmpfs descomprimido */
    u8 *cache;
    u64 cache_len;
    struct hfs_wr *wr;       /* estado de escrita (hfsplus_wr.c); NULL = somente leitura */
} hfs_t;

typedef struct {
    u8 data[80], rsrc[80];   /* HFSPlusForkData crus */
    u32 file_id, link_ino;
    bool compressed;
    fork_t dfork;            /* extents completos (carregados na 1ª leitura) */
    bool loaded;
} hfs_file_t;

bool hfs_fork_read(hfs_t *h, const fork_t *f, u64 off, void *buf, u64 len);
bool hfs_fork_load(hfs_t *h, const u8 *fd, u32 file_id, u8 fork_type, fork_t *f);

/* escrita (hfsplus_wr.c) */
bool hfs_journal_prepare(blkdev_t *dev, const u8 *vh, bool writable, bool *ro_out); /* replay; ro_out = montar só leitura */
bool hfs_wr_open(hfs_t *h, const u8 *vh);     /* liga a escrita (disco gravável e volume em bom estado) */
void hfs_wr_close(hfs_t *h);                  /* grava o pendente e marca "desmontado corretamente" */
bool hfs_wr_write(dmount_t *m, const char *path, const void *data, u64 len);
bool hfs_wr_mkdir(dmount_t *m, const char *path);
bool hfs_wr_remove(dmount_t *m, const char *path);
bool hfs_wr_rename(dmount_t *m, const char *from, const char *to);
bool hfs_wr_sync(dmount_t *m);
