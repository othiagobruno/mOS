/* mOS — dispositivos de bloco em memória/partição e decmpfs (compressão por arquivo) */
#include "fs.h"

static bool mem_read(blkdev_t *d, u64 off, void *buf, u64 len) {
    memcpy(buf, (const u8 *)d->ctx + off, len);
    return true;
}

blkdev_t *blk_from_memory(const u8 *data, u64 size) {
    blkdev_t *d = kcalloc(1, sizeof(*d));
    if (!d) return NULL;
    d->size = size;
    d->read = mem_read;
    d->ctx = (void *)data;
    return d;
}

static bool slice_read(blkdev_t *d, u64 off, void *buf, u64 len) {
    return blk_read(d->parent, d->base + off, buf, len);
}
static bool slice_write(blkdev_t *d, u64 off, const void *buf, u64 len) {
    return blk_write(d->parent, d->base + off, buf, len);
}
static bool slice_flush(blkdev_t *d) { return blk_flush(d->parent); }

blkdev_t *blk_slice(blkdev_t *parent, u64 off, u64 size) {
    if (off > parent->size) return NULL;
    if (size > parent->size - off) size = parent->size - off;
    blkdev_t *d = kcalloc(1, sizeof(*d));
    if (!d) return NULL;
    d->size = size;
    d->read = slice_read;
    if (parent->write) { d->write = slice_write; d->flush = slice_flush; }
    d->base = off;
    d->parent = parent;
    return d;
}

void blk_release(blkdev_t *d) {
    if (!d) return;
    if (!d->parent) pcache_drop(d);
    if (d->release) d->release(d);
    kfree(d);
}

/* ================================================================ decmpfs */
#define DECMPFS_MAGIC 0x636d7066u /* "fpmc" */
#define CHUNK 65536u

bool decmpfs_header(const u8 *x, u64 xlen, u32 *type, u64 *size) {
    if (xlen < 16 || le32(x) != DECMPFS_MAGIC) return false;
    if (type) *type = le32(x + 4);
    if (size) *size = le64(x + 8);
    return true;
}

static i64 chunk_decode(u32 type, const u8 *src, u64 n, u8 *dst, u64 cap) {
    if (!n) return 0;
    switch (type) {
    case 3: case 4:
        if ((src[0] & 0x0F) == 0x0F) { n = MIN(n - 1, cap); memcpy(dst, src + 1, n); return (i64)n; }
        return zlib_decompress(src, n, dst, cap);
    case 7: case 8:
        if (src[0] == 0x06) { n = MIN(n - 1, cap); memcpy(dst, src + 1, n); return (i64)n; }
        return lzvn_decompress(src, n, dst, cap);
    case 11: case 12:
        return lzfse_decompress(src, n, dst, cap);
    case 1: case 9: case 10:
        n = MIN(n, cap); memcpy(dst, src, n); return (i64)n;
    }
    return -1;
}

u8 *decmpfs_decode(const u8 *x, u64 xlen, fork_reader_t rsrc, void *rctx, u64 rsrc_size, u64 *out_size) {
    u32 type;
    u64 size;
    if (!decmpfs_header(x, xlen, &type, &size) || size > (1ull << 31)) return NULL;
    u8 *out = kmalloc(size ? size : 1);
    if (!out) return NULL;
    *out_size = size;
    if (!size) return out;

    if (type == 1 || type == 3 || type == 7 || type == 9 || type == 11) { /* dados no próprio xattr */
        i64 r = chunk_decode(type, x + 16, xlen - 16, out, size);
        if (r != (i64)size) goto fail;
        return out;
    }
    if (type != 4 && type != 8 && type != 10 && type != 12) goto fail;
    if (!rsrc || rsrc_size < 8 || rsrc_size > (1ull << 31)) goto fail;

    u8 *fork = kmalloc(rsrc_size);
    if (!fork) goto fail;
    if (!rsrc(rctx, 0, fork, rsrc_size)) { kfree(fork); goto fail; }
    u64 nblocks = (size + CHUNK - 1) / CHUNK, o = 0;
    bool ok = true;
    if (type == 4) {
        /* resource fork clássico: cabeçalho de 256 bytes, tabela {offset,tamanho} LE */
        u32 doff = be32(fork);
        if ((u64)doff + 8 > rsrc_size) ok = false;
        u32 cnt = ok ? le32(fork + doff + 4) : 0;
        const u8 *tbl = fork + doff + 8, *base = fork + doff + 4;
        if (ok && (cnt < nblocks || (u64)(tbl - fork) + (u64)cnt * 8 > rsrc_size)) ok = false;
        for (u64 i = 0; ok && i < nblocks; i++) {
            u32 bo = le32(tbl + i * 8), bs = le32(tbl + i * 8 + 4);
            if ((u64)(base - fork) + bo + bs > rsrc_size) { ok = false; break; }
            u64 want = MIN(CHUNK, size - o);
            i64 r = chunk_decode(type, base + bo, bs, out + o, want);
            if (r != (i64)want) ok = false;
            o += want;
        }
    } else {
        /* LZVN/LZFSE/cru: tabela de offsets u32 LE no início do fork */
        if ((nblocks + 1) * 4 > rsrc_size) ok = false;
        for (u64 i = 0; ok && i < nblocks; i++) {
            u32 a = le32(fork + i * 4), b = le32(fork + i * 4 + 4);
            if (b < a || b > rsrc_size) { ok = false; break; }
            u64 want = MIN(CHUNK, size - o);
            i64 r = chunk_decode(type, fork + a, b - a, out + o, want);
            if (r != (i64)want) ok = false;
            o += want;
        }
    }
    kfree(fork);
    if (ok) return out;
fail:
    kfree(out);
    return NULL;
}
