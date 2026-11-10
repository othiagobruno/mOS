/*
 * mOS — /usr/lib/libcompression.dylib
 * COMPRESSION_ZLIB (0x205) é deflate cru, RFC 1951, zlib 1.3.2 (windowBits -15).
 * COMPRESSION_LZ4 (0x100) é frame LZ4. COMPRESSION_LZ4_RAW (0x101) é um bloco cru.
 * COMPRESSION_LZFSE (0x801) é a referência Apple (BSD-3).
 * COMPRESSION_LZMA, LZBITMAP e BROTLI devolvem 0 (não ligados).
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int compression_algorithm;
typedef struct {
    uint8_t *dst_ptr;
    size_t dst_size;
    const uint8_t *src_ptr;
    size_t src_size;
    void *state;
} compression_stream;

#define COMPRESSION_LZ4       0x100
#define COMPRESSION_LZ4_RAW   0x101
#define COMPRESSION_ZLIB      0x205
#define COMPRESSION_LZMA      0x306
#define COMPRESSION_LZBITMAP  0x702
#define COMPRESSION_LZFSE     0x801
#define COMPRESSION_BROTLI    0xB02

#define COMPRESSION_STATUS_OK    0
#define COMPRESSION_STATUS_END   1
#define COMPRESSION_STATUS_ERROR (-1)
#define COMPRESSION_STREAM_ENCODE 0
#define COMPRESSION_STREAM_FINALIZE 0x0001

#define CZ_HOST 1
#include "../../libsystem/compression_z.c"

#include "lz4.h"
#include "lz4frame.h"
#include "lzfse.h"

static void warn_unsupported(compression_algorithm a) {
    static int w;
    if (!w++) fprintf(stderr, "libcompression[mOS]: algoritmo 0x%x sem codec (LZMA/LZBITMAP/BROTLI)\n", a);
}

static int supported(compression_algorithm a) {
    return a == COMPRESSION_ZLIB || a == COMPRESSION_LZ4 || a == COMPRESSION_LZ4_RAW || a == COMPRESSION_LZFSE;
}

size_t compression_encode_scratch_buffer_size(compression_algorithm a) {
    if (a == COMPRESSION_LZFSE) return lzfse_encode_scratch_size();
    return 0;
}

size_t compression_decode_scratch_buffer_size(compression_algorithm a) {
    if (a == COMPRESSION_LZFSE) return lzfse_decode_scratch_size();
    return 0;
}

static size_t lz4_frame_encode(uint8_t *dst, size_t dn, const uint8_t *src, size_t sn) {
    size_t n;
    if (sn > 0x7fffffff) return 0;
    n = LZ4F_compressFrame(dst, dn, src, sn, NULL);
    return LZ4F_isError(n) ? 0 : n;
}

static size_t lz4_frame_decode(uint8_t *dst, size_t dn, const uint8_t *src, size_t sn) {
    LZ4F_dctx *ctx = NULL;
    size_t off_s = 0, off_d = 0;
    LZ4F_errorCode_t e = LZ4F_createDecompressionContext(&ctx, LZ4F_VERSION);
    if (LZ4F_isError(e) || !ctx) return 0;
    while (off_s < sn && off_d < dn) {
        size_t ssz = sn - off_s, dsz = dn - off_d;
        size_t r = LZ4F_decompress(ctx, dst + off_d, &dsz, src + off_s, &ssz, NULL);
        if (LZ4F_isError(r)) { LZ4F_freeDecompressionContext(ctx); return 0; }
        off_s += ssz;
        off_d += dsz;
        if (r == 0) break;
    }
    LZ4F_freeDecompressionContext(ctx);
    return off_d;
}

static size_t lz4_raw_encode(uint8_t *dst, size_t dn, const uint8_t *src, size_t sn) {
    int n;
    if (sn > 0x7fffffff || dn > 0x7fffffff) return 0;
    n = LZ4_compress_default((const char *)src, (char *)dst, (int)sn, (int)dn);
    return n > 0 ? (size_t)n : 0;
}

static size_t lz4_raw_decode(uint8_t *dst, size_t dn, const uint8_t *src, size_t sn) {
    int n;
    if (sn > 0x7fffffff || dn > 0x7fffffff) return 0;
    n = LZ4_decompress_safe((const char *)src, (char *)dst, (int)sn, (int)dn);
    return n >= 0 ? (size_t)n : 0;
}

size_t compression_encode_buffer(uint8_t *dst, size_t dn, const uint8_t *src, size_t sn,
                                 void *scratch, compression_algorithm a) {
    if (!dst || !dn) return 0;
    if (a == COMPRESSION_ZLIB) return cz_raw_buffer(dst, dn, src, sn, 1);
    if (a == COMPRESSION_LZ4) return lz4_frame_encode(dst, dn, src, sn);
    if (a == COMPRESSION_LZ4_RAW) return lz4_raw_encode(dst, dn, src, sn);
    if (a == COMPRESSION_LZFSE) return lzfse_encode_buffer(dst, dn, src, sn, scratch);
    warn_unsupported(a);
    return 0;
}

size_t compression_decode_buffer(uint8_t *dst, size_t dn, const uint8_t *src, size_t sn,
                                 void *scratch, compression_algorithm a) {
    if (!dst || !dn) return 0;
    if (a == COMPRESSION_ZLIB) return cz_raw_buffer(dst, dn, src, sn, 0);
    if (a == COMPRESSION_LZ4) return lz4_frame_decode(dst, dn, src, sn);
    if (a == COMPRESSION_LZ4_RAW) return lz4_raw_decode(dst, dn, src, sn);
    if (a == COMPRESSION_LZFSE) return lzfse_decode_buffer(dst, dn, src, sn, scratch);
    warn_unsupported(a);
    return 0;
}

typedef struct {
    int algo;
    int encode;
    int zlib;
    int done;
    z_stream z;
    uint8_t *acc;
    size_t n, cap;
} mos_st;

int compression_stream_init(compression_stream *s, int op, int algo) {
    mos_st *st;
    if (!s || !supported(algo)) return COMPRESSION_STATUS_ERROR;
    st = (mos_st *)calloc(1, sizeof *st);
    if (!st) return COMPRESSION_STATUS_ERROR;
    st->algo = algo;
    st->encode = (op == COMPRESSION_STREAM_ENCODE);
    if (algo == COMPRESSION_ZLIB) {
        st->zlib = 1;
        if (!cz_raw_begin(&st->z, st->encode)) { free(st); return COMPRESSION_STATUS_ERROR; }
    }
    s->state = st;
    return COMPRESSION_STATUS_OK;
}

static int bag_add(mos_st *st, const uint8_t *p, size_t n) {
    if (n > (size_t)-1 - st->n) return 0;
    if (st->n + n > st->cap) {
        size_t c = st->cap ? st->cap : 256;
        uint8_t *g;
        while (c < st->n + n) c *= 2;
        g = (uint8_t *)realloc(st->acc, c);
        if (!g) return 0;
        st->acc = g;
        st->cap = c;
    }
    if (n) memcpy(st->acc + st->n, p, n);
    st->n += n;
    return 1;
}

int compression_stream_process(compression_stream *s, int flags) {
    mos_st *st;
    int r;
    size_t n;
    if (!s || !s->state) return COMPRESSION_STATUS_ERROR;
    st = (mos_st *)s->state;
    if (st->done) return COMPRESSION_STATUS_END;
    if (st->zlib) {
        if (s->src_size > 0xffffffffu || s->dst_size > 0xffffffffu) return COMPRESSION_STATUS_ERROR;
        st->z.next_in = (unsigned char *)s->src_ptr;
        st->z.avail_in = (unsigned)s->src_size;
        st->z.next_out = s->dst_ptr;
        st->z.avail_out = (unsigned)s->dst_size;
        r = st->encode ? deflate(&st->z, (flags & COMPRESSION_STREAM_FINALIZE) ? Z_FINISH : Z_NO_FLUSH)
                       : inflate(&st->z, (flags & COMPRESSION_STREAM_FINALIZE) ? Z_FINISH : Z_NO_FLUSH);
        s->src_ptr = st->z.next_in;
        s->src_size = st->z.avail_in;
        s->dst_ptr = st->z.next_out;
        s->dst_size = st->z.avail_out;
        if (r == Z_STREAM_END) { st->done = 1; return COMPRESSION_STATUS_END; }
        if (r == Z_OK || r == Z_BUF_ERROR) return COMPRESSION_STATUS_OK;
        return COMPRESSION_STATUS_ERROR;
    }
    if (s->src_size) {
        if (!bag_add(st, s->src_ptr, s->src_size)) return COMPRESSION_STATUS_ERROR;
        s->src_ptr += s->src_size;
        s->src_size = 0;
    }
    if (!(flags & COMPRESSION_STREAM_FINALIZE)) return COMPRESSION_STATUS_OK;
    if (st->encode)
        n = compression_encode_buffer(s->dst_ptr, s->dst_size, st->acc, st->n, NULL, st->algo);
    else
        n = compression_decode_buffer(s->dst_ptr, s->dst_size, st->acc, st->n, NULL, st->algo);
    if (!n) return COMPRESSION_STATUS_ERROR;
    s->dst_ptr += n;
    s->dst_size -= n;
    st->done = 1;
    return COMPRESSION_STATUS_END;
}

int compression_stream_destroy(compression_stream *s) {
    mos_st *st;
    if (!s || !s->state) return COMPRESSION_STATUS_OK;
    st = (mos_st *)s->state;
    if (st->zlib) cz_raw_end(&st->z, st->encode);
    free(st->acc);
    free(st);
    s->state = NULL;
    return COMPRESSION_STATUS_OK;
}
