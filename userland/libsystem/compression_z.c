/*
 * compression_stream_* e crc32 com a zlib 1.3.2 (a mesma do build-zlib.ps1),
 * ligada dentro do libSystem. COMPRESSION_ZLIB (0x205) comprime e descomprime.
 * Os outros algoritmos devolvem erro. Não é o codec de vídeo.
 */
#ifdef CZ_HOST
#include <stdlib.h>
#include <string.h>
#else
#include "sys.h"
#include "libc.h"
#endif
#define Z_SOLO 1
#include "../third_party/zlib/adler32.c"
#include "../third_party/zlib/crc32.c"
#include "../third_party/zlib/deflate.c"
#include "../third_party/zlib/trees.c"
#include "../third_party/zlib/inftrees.c"
#include "../third_party/zlib/inffast.c"
#include "../third_party/zlib/inflate.c"
#include "../third_party/zlib/zutil.c"

enum { CZ_ZLIB = 0x205, CZ_ENCODE = 0, CZ_FINAL = 1 };
enum { CZ_OK = 0, CZ_ERR = -1, CZ_END = 1 };

typedef struct {
    unsigned char *dst_ptr;
    unsigned long dst_size;
    const unsigned char *src_ptr;
    unsigned long src_size;
    void *state;
} cz_stream;

typedef struct {
    z_stream z;
    int encode;
} cz_state;

static void *cz_alloc(void *q, unsigned n, unsigned m) {
    (void)q;
    return calloc((unsigned long)n, (unsigned long)m);
}
static void cz_free(void *q, void *p) { (void)q; free(p); }

int compression_stream_init(cz_stream *s, int op, int algo) {
    cz_state *st;
    int r;
    if (!s || algo != CZ_ZLIB) return CZ_ERR;
    st = (cz_state *)malloc(sizeof *st);
    if (!st) return CZ_ERR;
    memset(st, 0, sizeof *st);
    st->z.zalloc = cz_alloc;
    st->z.zfree = cz_free;
    st->encode = (op == CZ_ENCODE);
    if (st->encode) r = deflateInit(&st->z, 6);
    else r = inflateInit(&st->z);
    if (r != Z_OK) { free(st); return CZ_ERR; }
    s->state = st;
    return CZ_OK;
}

int compression_stream_process(cz_stream *s, int flags) {
    cz_state *st;
    int r;
    if (!s || !s->state) return CZ_ERR;
    st = (cz_state *)s->state;
    st->z.next_in = (unsigned char *)s->src_ptr;
    st->z.avail_in = (unsigned)s->src_size;
    st->z.next_out = s->dst_ptr;
    st->z.avail_out = (unsigned)s->dst_size;
    if (st->encode) r = deflate(&st->z, (flags & CZ_FINAL) ? Z_FINISH : Z_NO_FLUSH);
    else r = inflate(&st->z, (flags & CZ_FINAL) ? Z_FINISH : Z_NO_FLUSH);
    s->src_ptr = st->z.next_in;
    s->src_size = st->z.avail_in;
    s->dst_ptr = st->z.next_out;
    s->dst_size = st->z.avail_out;
    if (r == Z_STREAM_END) return CZ_END;
    if (r == Z_OK || r == Z_BUF_ERROR) return CZ_OK;
    return CZ_ERR;
}

int compression_stream_destroy(cz_stream *s) {
    cz_state *st;
    if (!s || !s->state) return CZ_OK;
    st = (cz_state *)s->state;
    if (st->encode) deflateEnd(&st->z);
    else inflateEnd(&st->z);
    free(st);
    s->state = 0;
    return CZ_OK;
}
