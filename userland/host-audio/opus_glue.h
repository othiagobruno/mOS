/* Casca do mOS sobre o libopus. Ver opus_glue.c. */
#ifndef MOS_OPUS_GLUE_H
#define MOS_OPUS_GLUE_H
#include <stdint.h>

typedef struct {
    int channels, preskip, gain_q8, family, streams, coupled;
    uint32_t in_rate;
    uint8_t map[255];
} cx_opus_head;

int cx_opus_head_ogg(const uint8_t *p, int n, cx_opus_head *h);  /* pacote OpusHead */
int cx_opus_head_dops(const uint8_t *p, int n, cx_opus_head *h); /* corpo da caixa dOps */
void *cx_opus_open(const cx_opus_head *h);                        /* decodificador a 48 kHz */
int cx_opus_decode(void *d, const uint8_t *pkt, int n, int16_t *pcm, int max_frames); /* quadros ou < 0 */
void cx_opus_close(void *d);
int cx_opus_packet_frames(const uint8_t *pkt, int n);
/* Arquivo Ogg Opus inteiro. 0 ok (*out com free), < 0 erro. Sempre 48 kHz. */
int cx_opus_decode_ogg(const uint8_t *b, uint64_t n, int16_t **out, int *channels, int *frames);
#endif
