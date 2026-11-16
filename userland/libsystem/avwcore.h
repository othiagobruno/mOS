/* mOS — núcleo do AVAssetWriter (MP4/MOV/M4A sobre o mp4mux): entradas, sessão (startSession/endSession), durações das
 * amostras pelo dts seguinte, faixas criadas na primeira amostra (o avcC do codificador só existe depois do 1º quadro) e a
 * edit list da sessão. Sem Objective-C: a casca do AVAssetWriter (avfaudio.c) codifica (VTCompressionSession,
 * AudioConverter) e entrega pacotes prontos aqui. Teste de host: tools/avwcore_test.c (tools/avwriter_test.sh). */
#pragma once
#include <stdint.h>

typedef struct { int64_t v; int32_t ts; } avw_time;   /* CMTime: valor/escala; ts <= 0 = inválido */
typedef struct avw avw_t;
enum { AVW_VIDEO = 1, AVW_AUDIO = 2 };
enum { AVW_ERR_STATE = -1, AVW_ERR_FORMAT = -2, AVW_ERR_ORDER = -3, AVW_ERR_IO = -4, AVW_ERR_SESSION = -5 };

/* container: MP4W_MP4, MP4W_MOV ou MP4W_M4A (mp4mux.h); faststart = shouldOptimizeForNetworkUse */
avw_t *avw_open(const char *path, int container, int faststart);
/* devolve o índice da entrada (0..7) ou < 0. media_timescale 0 = automático (vídeo: a escala do 1º pts se <= 100000,
 * senão 90000; áudio: a taxa). matrix: tkhd (9 valores do QuickTime) ou NULL */
int avw_add_input(avw_t *w, int media, uint32_t media_timescale, const int32_t *matrix);
/* formato da entrada; pode vir depois de outras entradas já terem amostras, mas antes da 1ª amostra desta */
int avw_set_h264(avw_t *w, int in, int width, int height, const uint8_t *avcc, uint32_t n);
int avw_set_aac(avw_t *w, int in, int rate, int channels, const uint8_t *asc, uint32_t n, uint32_t bitrate, uint32_t priming);
int avw_set_opus(avw_t *w, int in, int channels, uint32_t preskip, uint32_t in_rate);
int avw_set_lpcm(avw_t *w, int in, int rate, int channels, int bits);
int avw_has_format(const avw_t *w, int in);
/* startSessionAtSourceTime: obrigatório antes da 1ª amostra; endSessionAtSourceTime: opcional */
int avw_start_session(avw_t *w, avw_time t);
int avw_end_session(avw_t *w, avw_time t);
/* um pacote. Vídeo: pts/dts (dts inválido = pts) e dur (pode ser inválido; a duração sai do dts seguinte), sync =
 * quadro-chave. Áudio: pts e frames (quadros de áudio do pacote; LPCM: bytes / (canais * bytes por amostra)) */
int avw_append(avw_t *w, int in, const uint8_t *data, uint32_t size, avw_time pts, avw_time dts, avw_time dur,
               uint32_t frames, int sync);
int avw_mark_finished(avw_t *w, int in);
/* fecha o arquivo (moov, edit lists da sessão); 0 ok. Libera w em qualquer caso. */
int avw_finish(avw_t *w);
/* apaga o arquivo parcial e libera w */
void avw_cancel(avw_t *w);
