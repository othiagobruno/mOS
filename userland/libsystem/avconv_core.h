/* Núcleo do AVAudioConverter (avfaudio.c): PCM para PCM sem runtime ObjC, para o teste de host
 * (tools/avconv_core_test.c) chamar as mesmas funções que a classe chama. */
#ifndef MOS_AVCONV_CORE_H
#define MOS_AVCONV_CORE_H
#include <stdint.h>

/* formato comum do AVAudioFormat: 1 float32, 2 float64, 3 int16, 4 int32 */
enum { AVC_F32 = 1, AVC_F64 = 2, AVC_I16 = 3, AVC_I32 = 4 };

typedef struct {
    uint32_t common, channels;
    int interleaved;
    double rate;
} avc_fmt;

/* vista de um AVAudioPCMBuffer: planes[c] aponta a primeira amostra do canal c */
typedef struct {
    avc_fmt fmt;
    void **planes;
    uint32_t len, cap;
} avc_buf;

typedef struct {
    float *stage;                /* entrada em float, stage[f * canais + c] */
    uint32_t n, cap;
    double pos;                  /* próximo quadro de saída, fracionário, dentro de stage */
    int eos;
} avc_state;

/* channelMap: map[oc] = canal de entrada (−1 = silêncio); nmap < 0 sem mapa */
typedef struct { const long *map; long nmap; } avc_route;

float avc_get(const avc_buf *b, uint32_t ch, uint32_t frame);
void avc_put(avc_buf *b, uint32_t ch, uint32_t frame, float v);
float avc_mix(const avc_route *r, const float *frame, uint32_t ic, uint32_t oc, uint32_t och);

/* Mesma taxa: copia min(in.len, out.cap) quadros convertendo amostra e canais. -1 se as taxas diferem. */
int avc_direct(const avc_route *r, const avc_buf *in, avc_buf *out);

/* Pede entrada: devolve o status do AVAudioConverterInputStatus (0 HaveData, 1 NoDataNow, 2 EndOfStream)
 * e preenche *got com a vista do buffer (len 0 se não veio nada). */
typedef int (*avc_pull_fn)(void *ctx, uint32_t want, avc_buf *got);

/* Modo com bloco. Volta 0 HaveData, 1 InputRanDry, 2 EndOfStream, 3 erro (sem memória). */
int avc_convert(avc_state *s, const avc_route *r, const avc_fmt *in, avc_buf *out, avc_pull_fn pull, void *ctx);
void avc_reset(avc_state *s);
void avc_free(avc_state *s);

#endif
