/* Casca C do mOS sobre o decodificador FDK AAC. Ver aac_glue.cpp. */
#ifndef MOS_AAC_GLUE_H
#define MOS_AAC_GLUE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { CX_AAC_RAW = 0, CX_AAC_ADTS = 1, CX_AAC_LATM = 2 };
/* asc = AudioSpecificConfig (só RAW). max_channels > 0 faz o downmix do FDK (2 = estéreo). */
void *cx_aac_open(const uint8_t *asc, int asc_n, int transport, int max_channels);
void cx_aac_close(void *c);
int cx_aac_decode(void *c, const uint8_t *pkt, int n, int16_t *pcm, int max_samples, int *channels, int *rate);
int cx_aac_decode_adts(const uint8_t *b, uint64_t n, int max_channels, int16_t **out, int *channels, int *rate, int *frames);
#ifdef __cplusplus
}
#endif
#endif
