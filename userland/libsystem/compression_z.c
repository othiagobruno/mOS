/*
 * Deflate cru (RFC 1951, windowBits -15) com a zlib 1.3.2.
 * COMPRESSION_ZLIB do macOS. O stream e o buffer usam cz_raw_*.
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

/* COMPRESSION_ZLIB no macOS é deflate cru (RFC 1951), windowBits -15. */

static void *cz_alloc(void *q, unsigned n, unsigned m) {
    (void)q;
    return calloc((unsigned long)n, (unsigned long)m);
}
static void cz_free(void *q, void *p) { (void)q; free(p); }

int cz_raw_begin(z_stream *z, int encode) {
    int r;
    memset(z, 0, sizeof *z);
    z->zalloc = cz_alloc;
    z->zfree = cz_free;
    if (encode) r = deflateInit2(z, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    else r = inflateInit2(z, -15);
    return r == Z_OK;
}

void cz_raw_end(z_stream *z, int encode) {
    if (encode) deflateEnd(z);
    else inflateEnd(z);
}

size_t cz_raw_buffer(unsigned char *dst, size_t dn, const unsigned char *src, size_t sn, int encode) {
    z_stream z;
    int r;
    unsigned long n;
    if (!dst || dn > 0xfffffffful || sn > 0xfffffffful) return 0;
    if (sn && !src) return 0;
    if (!cz_raw_begin(&z, encode)) return 0;
    z.next_in = (unsigned char *)src;
    z.avail_in = (unsigned)sn;
    z.next_out = dst;
    z.avail_out = (unsigned)dn;
    r = encode ? deflate(&z, Z_FINISH) : inflate(&z, Z_FINISH);
    n = z.total_out;
    cz_raw_end(&z, encode);
    return r == Z_STREAM_END ? (size_t)n : 0;
}
