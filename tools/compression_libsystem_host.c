/* Ida e volta pelos símbolos que o ELF do libSystem exporta (sem o '_' do Mach-O). */
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
#define LZFSE 0x801

static int roundtrip(const char *name, int algo) {
    const char *msg = "libSystem compression round trip";
    uint8_t comp[512], back[128];
    size_t sn = strlen(msg);
    size_t n = compression_encode_buffer(comp, sizeof comp, (const uint8_t *)msg, sn, NULL, algo);
    size_t m = n ? compression_decode_buffer(back, sizeof back, comp, n, NULL, algo) : 0;
    compression_stream e, d;
    uint8_t sc[512], sb[128];
    size_t got;
    if (!n || m != sn || memcmp(back, msg, sn) != 0) {
        fprintf(stderr, "buffer %s falhou enc=%zu dec=%zu\n", name, n, m);
        return 1;
    }
    memset(&e, 0, sizeof e);
    e.dst_ptr = sc; e.dst_size = sizeof sc;
    e.src_ptr = (const uint8_t *)msg; e.src_size = sn;
    if (compression_stream_init(&e, 0, algo) != 0 || compression_stream_process(&e, 1) != 1) {
        fprintf(stderr, "stream encode %s\n", name);
        return 1;
    }
    got = (size_t)(e.dst_ptr - sc);
    compression_stream_destroy(&e);
    memset(&d, 0, sizeof d);
    d.dst_ptr = sb; d.dst_size = sizeof sb;
    d.src_ptr = sc; d.src_size = got;
    if (compression_stream_init(&d, 1, algo) != 0 || compression_stream_process(&d, 1) != 1) {
        fprintf(stderr, "stream decode %s\n", name);
        return 1;
    }
    if ((size_t)(d.dst_ptr - sb) != sn || memcmp(sb, msg, sn) != 0) {
        fprintf(stderr, "stream %s conteudo\n", name);
        return 1;
    }
    compression_stream_destroy(&d);
    return 0;
}

int main(void) {
    if (roundtrip("zlib", ZLIB) || roundtrip("lz4", LZ4) || roundtrip("lzfse", LZFSE)) return 1;
    printf("libSystem compression ok\n");
    return 0;
}
