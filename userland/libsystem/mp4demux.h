/* Demuxer ISO BMFF (MP4, MOV, M4A, M4V, 3GP e MP4 fragmentado) do mOS.
 * Lê só o índice (moov/moof) para a memória; as amostras vêm sob demanda pelo callback de leitura.
 * Serve o AVPlayer, o AVAssetReader, o AudioFile e o cx_audiodec. Teste de host: tools/mp4demux_test.c. */
#ifndef MOS_MP4DEMUX_H
#define MOS_MP4DEMUX_H
#include <stddef.h>
#include <stdint.h>

#define MP4_FOURCC(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))

typedef struct {
    uint64_t off;      /* posição no arquivo */
    uint32_t size;
    uint32_t dur;      /* em timescale da faixa */
    int64_t dts;       /* tempo de decodificação, timescale da faixa */
    int32_t cto;       /* composição - decodificação (ctts) */
    uint8_t sync;      /* quadro-chave (stss, ou todos quando não há stss) */
    uint8_t desc;      /* índice da entrada de stsd (0 = primeira) */
} mp4_sample;

typedef struct {
    uint32_t id;
    uint32_t handler;          /* 'soun', 'vide', 'text', 'sbtl', 'subt', 'clcp', 'tmcd', 'meta' */
    uint32_t codec;            /* 'avc1', 'hvc1', 'mp4a', 'Opus', 'alac', 'vp09', 'av01', 'lpcm', ... */
    uint32_t timescale;
    uint64_t duration;         /* mdhd, timescale da faixa */
    uint32_t width, height;    /* amostra visual (pixels) */
    double disp_w, disp_h;     /* tkhd (16.16), tamanho de apresentação */
    int32_t matrix[9];         /* tkhd, 16.16 / 2.30 */
    uint32_t channels, sample_rate, bits;
    uint32_t qt_bytes_per_frame;   /* QuickTime som v1: bytes por quadro (PCM) */
    uint8_t oti;               /* objectTypeIndication do esds (0x40 AAC, 0x6B MP3, ...) */
    uint8_t encrypted;         /* 'encv'/'enca': o codec veio de sinf/frma; não há descriptografia */
    uint8_t enabled;           /* tkhd flags & 1 */
    char lang[4];              /* mdhd ISO-639-2 */
    uint8_t *cfg;              /* corpo de avcC, hvcC, dOps, alac, vpcC, av1C, dfLa; o ASC no caso de esds (cópia própria) */
    uint32_t cfg_len;
    uint32_t cfg_type;         /* fourcc da caixa de cfg; 'esds' quando cfg é o DecoderSpecificInfo */
    int64_t edit_media_time;   /* primeira entrada de elst que não é vazia; -1 sem elst */
    uint64_t edit_empty;       /* soma das entradas vazias, em timescale do filme */
    uint32_t nsamples, cap;
    mp4_sample *s;
    /* padrões de trex/tfhd para fragmentos */
    uint32_t trex_desc, trex_dur, trex_size, trex_flags;
    int64_t next_dts;
} mp4_track;

typedef int (*mp4_read_fn)(void *ctx, uint64_t off, void *dst, uint32_t n); /* 0 ok */

typedef struct {
    mp4_read_fn read;
    void *ctx;
    uint64_t size;
    uint32_t brand;            /* ftyp major_brand ('isom', 'qt  ', 'M4A ', ...) */
    uint32_t timescale;        /* mvhd */
    uint64_t duration;         /* mvhd, timescale do filme */
    int fragmented;
    uint32_t ntracks;
    mp4_track *t;
    const char *err;
    const uint8_t *mem_b;      /* mp4_open_mem: o arquivo inteiro na memória */
} mp4_file;

/* 0 ok; -1 não é ISO BMFF; -2 corrompido; -3 sem memória. Em erro, mp4_close já foi feito. */
int mp4_open(mp4_file *f, mp4_read_fn read, void *ctx, uint64_t size);
int mp4_open_mem(mp4_file *f, const void *bytes, uint64_t n);
void mp4_close(mp4_file *f);
int mp4_read_sample(mp4_file *f, const mp4_track *t, uint32_t i, void *dst); /* dst com s[i].size bytes */
/* Primeira faixa com esse handler ('soun', 'vide') que tem amostras; -1 sem nenhuma. */
int mp4_find_track(const mp4_file *f, uint32_t handler);
/* Amostra de decodificação que cobre o tempo (segundos, eixo de apresentação), e o quadro-chave anterior. */
uint32_t mp4_sample_at(const mp4_track *t, double sec, uint32_t *keyframe);
double mp4_track_seconds(const mp4_track *t);
/* Tempo de apresentação (s) da amostra i, já com a edit list. */
double mp4_pts(const mp4_track *t, uint32_t i);
#endif
