/* mOS — vídeo de um arquivo MP4/MOV para o AVFoundation (AVAssetImageGenerator, AVPlayerItemVideoOutput, AVPlayerLayer).
 * Demuxer: mp4demux.c. Decodificador: OpenH264 corrigido pela casca cx_h264_* (host-audio/h264_glue.cpp).
 * Tempos em segundos no eixo de apresentação (edit list aplicada). Só H.264 ('avc1'/'avc3') por enquanto. */
#pragma once
#include <stdint.h>

typedef struct avv avv_t;
typedef struct {
    const uint8_t *y, *u, *v;   /* I420, válidos até a próxima chamada de avv_frame_at/avv_close */
    int sy, suv, w, h;
    double pts;                 /* segundos */
    int full_range, matrix, primaries, transfer;   /* VUI (H.264 E.2.1); 2 = não especificado */
} avv_frame;

/* 0 sem faixa de vídeo decodificável; *err: -11828 formato desconhecido, -11838 codec sem decodificador, -11800 E/S */
avv_t *avv_open_path(const char *path, int *err);
void avv_close(avv_t *v);
/* largura e altura do quadro, duração da faixa (s), quadros por segundo nominais, número de amostras */
void avv_info(const avv_t *v, int *w, int *h, double *duration, double *fps, uint32_t *nsamples);
/* o quadro exibido no tempo t (o último com pts <= t; antes do primeiro, o primeiro). 0 ok, < 0 erro. */
int avv_frame_at(avv_t *v, double t, avv_frame *out);
/* pts do próximo quadro depois do atual (para saber se há quadro novo); < 0 no fim */
double avv_next_pts(avv_t *v);
/* rotação da faixa (tkhd) em graus: 0, 90, 180 ou 270 (o preferredTransform do AVAssetTrack) */
int avv_rotation(const avv_t *v);
/* quadro -> BGRA (alfa 255) com a faixa e a matriz do VUI (sem matriz: HD em BT.709, SD em BT.601, como o macOS);
 * rot gira a imagem (0/90/180/270) e out_w x out_h (0 = tamanho natural já girado) reduz por média de área. dst com
 * out_w*out_h*4 bytes (bpr = out_w*4). Devolve 0 e o tamanho final em *ow, *oh. */
int avv_to_bgra(const avv_frame *f, int rot, int out_w, int out_h, uint8_t **dst, int *ow, int *oh);
/* quadro -> NV12 (Y + CbCr intercalado) na faixa pedida (dst_full: '420f'), convertendo se o fluxo for da outra faixa */
void avv_to_nv12(const avv_frame *f, int dst_full, uint8_t *yp, long ybpr, uint8_t *uvp, long uvbpr);
/* matriz efetiva (código H.273: 1 BT.709, 6 BT.601, 9 BT.2020, 7 SMPTE 240M) */
int avv_matrix(const avv_frame *f);
/* leitura sequencial (AVAssetReader): o próximo quadro decodificado na ordem de exibição; 1 ok, 0 fim, < 0 erro */
int avv_next_frame(avv_t *v, avv_frame *out);
/* amostra comprimida i na ordem do arquivo (AVCC), válida até a próxima chamada; tempos em segundos */
int avv_raw_sample(avv_t *v, uint32_t i, const uint8_t **data, uint32_t *size, double *pts, double *dts, double *dur, int *sync);
/* corpo do avcC (para a CMVideoFormatDescription) */
const uint8_t *avv_avcc(const avv_t *v, uint32_t *n);
