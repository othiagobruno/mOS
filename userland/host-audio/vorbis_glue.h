/* Vorbis no mOS: stb_vorbis (domínio público/MIT, userland/third_party/stb_vorbis) atrás de uma casca C. Ver vorbis_glue.c. */
#pragma once
#include <stdint.h>
/* Ogg Vorbis inteiro -> PCM 16 bits intercalado (*out com free). 0 ok */
int cx_vorbis_decode_ogg(const uint8_t *b, uint64_t n, int16_t **out, int *channels, int *rate, int *frames);
/* Vorbis de Matroska/WebM: CodecPrivate (3 cabeçalhos em lacing Xiph) e os pacotes na ordem do arquivo */
int cx_vorbis_decode_packets(const uint8_t *priv, uint32_t priv_n, const uint8_t *const *pkt, const uint32_t *len, uint32_t np,
                             int16_t **out, int *channels, int *rate, int *frames);
