/* mOS — núcleo do AVAssetExportSession: cópia das amostras (passthrough) de um MP4/MOV/M4A para MP4/MOV/M4A pelo
 * avwcore.c, com o timeRange. Sem Objective-C; a casca está em avexport_objc.c. Teste: tools/avexport_test.c. */
#pragma once
#include <stdint.h>

typedef struct { volatile int cancel; volatile float progress; } avx_ctl;
enum { AVX_OK = 0, AVX_ERR_OPEN = -1, AVX_ERR_CODEC = -2, AVX_ERR_WRITE = -3, AVX_ERR_CANCEL = -4, AVX_ERR_EMPTY = -5 };
enum { AVX_VIDEO = 1, AVX_AUDIO = 2 };

typedef struct {
    int has_video, has_audio;
    uint32_t vcodec, acodec;     /* fourcc da 1ª faixa de cada tipo ('avc1', 'mp4a', ...) */
    int width, height;
    double duration;             /* segundos (mvhd) */
    int video_copyable, audio_copyable, audio_is_aac;
} avx_info;
int avx_probe(const char *in, avx_info *out);
/* copia [start, start + dur) (dur < 0: até o fim) das faixas de want (AVX_VIDEO | AVX_AUDIO). container: MP4W_*.
 * Vídeo começa no quadro-chave anterior a start (cortado pela edit list); AAC/Opus levam um pacote de preroll.
 * 0 ok; em erro o arquivo de saída não fica e *why explica. */
int avx_passthrough(const char *in, const char *out, int container, int faststart, double start, double dur, int want,
                    avx_ctl *ctl, const char **why);
