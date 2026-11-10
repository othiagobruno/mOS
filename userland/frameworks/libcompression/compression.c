/*
 * mOS — /usr/lib/libcompression.dylib (Mach-O): a API compression_* do macOS, primeira versão. Sem codecs ainda
 * (LZFSE/LZ4/ZLIB/LZMA): as funções devolvem 0 bytes / COMPRESSION_STATUS_ERROR, o que o chamador trata como
 * falha. O Sparkle a liga, mas só a usa para descompactar atualizações.
 * tools/build-frameworks-objc.ps1
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef int compression_algorithm;
typedef struct { uint8_t *dst_ptr; size_t dst_size; const uint8_t *src_ptr; size_t src_size; void *state; } compression_stream;
#define COMPRESSION_STATUS_ERROR (-1)

static void warn(void) {
    static int w;
    if (!w++) fprintf(stderr, "libcompression[mOS]: sem codecs ainda (compression_* falham)\n");
}

size_t compression_encode_scratch_buffer_size(compression_algorithm a) { (void)a; return 0; }
size_t compression_decode_scratch_buffer_size(compression_algorithm a) { (void)a; return 0; }
size_t compression_encode_buffer(uint8_t *dst, size_t dn, const uint8_t *src, size_t sn, void *scratch, compression_algorithm a) {
    (void)dst; (void)dn; (void)src; (void)sn; (void)scratch; (void)a; warn(); return 0;
}
size_t compression_decode_buffer(uint8_t *dst, size_t dn, const uint8_t *src, size_t sn, void *scratch, compression_algorithm a) {
    (void)dst; (void)dn; (void)src; (void)sn; (void)scratch; (void)a; warn(); return 0;
}
/* zlib 1.3.2, o mesmo código do libSystem (compression_z.c). */
#define CZ_HOST 1
#include "../../libsystem/compression_z.c"
