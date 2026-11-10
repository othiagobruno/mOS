/* Teste de host: round-trip e um vetor da referência (zlib / lz4 / lzfse). */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef int compression_algorithm;
typedef struct {
    uint8_t *dst_ptr;
    size_t dst_size;
    const uint8_t *src_ptr;
    size_t src_size;
    void *state;
} compression_stream;

size_t compression_encode_buffer(uint8_t *, size_t, const uint8_t *, size_t, void *, compression_algorithm);
size_t compression_decode_buffer(uint8_t *, size_t, const uint8_t *, size_t, void *, compression_algorithm);
int compression_stream_init(compression_stream *, int, int);
int compression_stream_process(compression_stream *, int);
int compression_stream_destroy(compression_stream *);

#define ZLIB 0x205
#define LZ4 0x100
#define LZ4_RAW 0x101
#define LZFSE 0x801
#define LZMA 0x306

static const char plain[] = "mOS libcompression raw deflate vector v1\n";

/* zlib.compressobj(level=6, wbits=-15) do CPython, o mesmo RFC 1951 do macOS. */
static const uint8_t zlib_ref[] = {
    203,245,15,86,200,201,76,74,206,207,45,40,74,45,46,206,204,207,83,40,74,44,87,72,73,77,203,73,44,73,85,40,75,77,46,201,47,82,40,51,228,2,0
};
/* frame do `lz4` 1.10.0. */
static const uint8_t lz4_ref[] = {
    4,34,77,24,100,64,167,41,0,0,128,109,79,83,32,108,105,98,99,111,109,112,114,101,115,115,105,111,110,32,114,97,119,32,100,101,102,108,97,116,101,32,118,101,99,116,111,114,32,118,49,10,0,0,0,0,141,147,7,138
};
/* `lzfse -encode` da referência 1.0. */
static const uint8_t lzfse_ref[] = {
    98,118,120,45,41,0,0,0,109,79,83,32,108,105,98,99,111,109,112,114,101,115,115,105,111,110,32,114,97,119,32,100,101,102,108,97,116,101,32,118,101,99,116,111,114,32,118,49,10,98,118,120,36
};

static int fail;

static void expect_dec(const char *name, int algo, const uint8_t *ref, size_t rn) {
    uint8_t out[128];
    size_t n = compression_decode_buffer(out, sizeof out, ref, rn, NULL, algo);
    if (n != sizeof plain - 1 || memcmp(out, plain, n) != 0) {
        fprintf(stderr, "vetor %s falhou n=%zu\n", name, n);
        fail = 1;
    }
}

static void roundtrip(const char *name, int algo) {
    uint8_t comp[256], back[256];
    const char *msg = "abcdefghijklmnopqrstuvwxyz 0123456789 mOS round trip payload.";
    size_t sn = strlen(msg);
    size_t n = compression_encode_buffer(comp, sizeof comp, (const uint8_t *)msg, sn, NULL, algo);
    size_t m = n ? compression_decode_buffer(back, sizeof back, comp, n, NULL, algo) : 0;
    if (!n || m != sn || memcmp(back, msg, sn) != 0) {
        fprintf(stderr, "round-trip %s falhou enc=%zu dec=%zu\n", name, n, m);
        fail = 1;
    }
}

static void stream_rt(void) {
    compression_stream e, d;
    uint8_t comp[256], back[256];
    const char *msg = "stream-zlib-duas-partes";
    size_t sn = strlen(msg), half = 10, got = 0;
    memset(&e, 0, sizeof e);
    e.dst_ptr = comp; e.dst_size = sizeof comp;
    if (compression_stream_init(&e, 0, ZLIB) != 0) { fail = 1; return; }
    e.src_ptr = (const uint8_t *)msg; e.src_size = half;
    if (compression_stream_process(&e, 0) != 0) fail = 1;
    e.src_ptr = (const uint8_t *)msg + half; e.src_size = sn - half;
    if (compression_stream_process(&e, 1) != 1) fail = 1;
    got = (size_t)(e.dst_ptr - comp);
    compression_stream_destroy(&e);
    memset(&d, 0, sizeof d);
    d.dst_ptr = back; d.dst_size = sizeof back;
    d.src_ptr = comp; d.src_size = got;
    if (compression_stream_init(&d, 1, ZLIB) != 0) fail = 1;
    if (compression_stream_process(&d, 1) != 1) fail = 1;
    if ((size_t)(d.dst_ptr - back) != sn || memcmp(back, msg, sn) != 0) {
        fprintf(stderr, "stream zlib falhou\n");
        fail = 1;
    }
    compression_stream_destroy(&d);
}

int main(void) {
    uint8_t z[64];
    size_t n;
    expect_dec("zlib", ZLIB, zlib_ref, sizeof zlib_ref);
    expect_dec("lz4", LZ4, lz4_ref, sizeof lz4_ref);
    expect_dec("lzfse", LZFSE, lzfse_ref, sizeof lzfse_ref);
    n = compression_encode_buffer(z, sizeof z, (const uint8_t *)plain, sizeof plain - 1, NULL, ZLIB);
    if (n != sizeof zlib_ref || memcmp(z, zlib_ref, n) != 0) {
        fprintf(stderr, "encode zlib nao bate com a referencia n=%zu\n", n);
        fail = 1;
    }
    roundtrip("zlib", ZLIB);
    roundtrip("lz4", LZ4);
    roundtrip("lz4_raw", LZ4_RAW);
    roundtrip("lzfse", LZFSE);
    if (compression_encode_buffer(z, sizeof z, (const uint8_t *)plain, sizeof plain - 1, NULL, LZMA) != 0) {
        fprintf(stderr, "LZMA devia ser 0\n");
        fail = 1;
    }
    stream_rt();
    if (fail) return 1;
    printf("compression host ok\n");
    return 0;
}
