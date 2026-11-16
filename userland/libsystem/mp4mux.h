/* mOS — gravação de MP4/MOV/M4A (ISO BMFF) para o AVAssetWriter e o AVAudioRecorder. Ver mp4mux.c. */
#pragma once
#include <stdint.h>
typedef struct mp4w mp4w_t;
enum { MP4W_MP4 = 0, MP4W_MOV = 1, MP4W_M4A = 2 };
/* abre para escrita (trunca). faststart: o moov vai antes do mdat no fechamento (shouldOptimizeForNetworkUse) */
mp4w_t *mp4w_open(const char *path, int kind, int faststart);
/* faixas; devolvem o índice (0..) ou < 0. timescale: 90000 é bom para vídeo; o de áudio é a taxa de amostragem */
int mp4w_add_h264(mp4w_t *m, int width, int height, const uint8_t *avcc, uint32_t avcc_n, uint32_t timescale);
/* asc = AudioSpecificConfig; priming = quadros de atraso do codificador (vira edit list) */
int mp4w_add_aac(mp4w_t *m, int rate, int channels, const uint8_t *asc, uint32_t asc_n, uint32_t bitrate, uint32_t priming);
int mp4w_add_opus(mp4w_t *m, int channels, uint32_t preskip, uint32_t in_rate);
/* PCM inteiro com sinal, little-endian (MOV 'sowt' / MP4 'ipcm'); bits 16/24/32 */
int mp4w_add_lpcm(mp4w_t *m, int rate, int channels, int bits);
/* uma amostra; tempos no timescale da faixa; sync = quadro-chave (áudio: sempre 1) */
int mp4w_write(mp4w_t *m, int track, const uint8_t *data, uint32_t size, int64_t pts, int64_t dts, uint32_t dur, int sync);
/* grava o moov e fecha; 0 ok */
int mp4w_close(mp4w_t *m);
void mp4w_abort(mp4w_t *m);
