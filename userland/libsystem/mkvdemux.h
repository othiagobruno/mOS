/* mOS — demuxer Matroska/WebM (EBML). Ver mkvdemux.c. */
#pragma once
#include <stdint.h>
typedef struct {
    uint64_t off;              /* posição do quadro no arquivo */
    uint32_t size;
    int64_t pts_ns;            /* tempo de apresentação em ns (Cluster + bloco, × TimecodeScale) */
    uint32_t track;            /* índice em mkv_file.t */
    uint8_t key;
} mkv_packet;
typedef struct {
    uint64_t number;           /* TrackNumber */
    int type;                  /* 1 vídeo, 2 áudio, 17 legenda */
    char codec[32];            /* CodecID: A_OPUS, A_VORBIS, A_AAC, A_MPEG/L3, A_FLAC, V_VP9, V_AV1, V_MPEG4/ISO/AVC ... */
    uint8_t *priv; uint32_t priv_n;   /* CodecPrivate */
    double rate;               /* SamplingFrequency (OutputSamplingFrequency quando há, HE-AAC) */
    uint32_t channels, bits, width, height;
    uint64_t default_dur_ns, codec_delay_ns, seek_preroll_ns;
} mkv_track;
typedef struct {
    const uint8_t *b; uint64_t n;   /* o arquivo inteiro na memória */
    uint64_t timescale;        /* ns por unidade de Timecode (padrão 1.000.000) */
    double duration_ns;
    int webm;                  /* DocType "webm" */
    mkv_track t[16]; uint32_t ntracks;
    mkv_packet *p; uint32_t np, cap;
    const char *err;
} mkv_file;
/* 0 ok; -1 não é EBML/Matroska; -2 corrompido; -3 sem memória. Os pacotes ficam na ordem do arquivo. */
int mkv_open_mem(mkv_file *f, const void *bytes, uint64_t n);
void mkv_close(mkv_file *f);
/* primeira faixa do tipo (1 vídeo, 2 áudio); -1 sem */
int mkv_find_track(const mkv_file *f, int type);
