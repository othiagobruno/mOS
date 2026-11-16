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
/* ---- codificador (FDK libAACenc). aot: 2 AAC-LC, 5 HE-AAC (SBR), 29 HE-AACv2 (SBR+PS, só estéreo); bitrate em bits/s
 * (0 = o padrão do FDK para o modo). adts 1 = pacotes com cabeçalho ADTS. asc recebe o AudioSpecificConfig (cookie);
 * frame = quadros de entrada por pacote (1024 LC, 2048 HE); delay = atraso do codificador em quadros (priming). */
void *cx_aacenc_open(int rate, int channels, int bitrate, int aot, int adts, uint8_t *asc, int *asc_n, int *frame, int *delay);
/* frames quadros PCM s16 intercalados (frames <= 0: esvazia o fim). Devolve bytes de um pacote (0 = ainda nenhum), < 0 erro */
int cx_aacenc_encode(void *e, const int16_t *pcm, int frames, uint8_t *out, int cap);
void cx_aacenc_close(void *e);
#ifdef __cplusplus
}
#endif
#endif
