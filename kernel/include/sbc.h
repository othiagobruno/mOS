/* mOS — codificador SBC (Bluetooth A2DP) */
#pragma once
#ifdef MOS_HOST_TEST
#include "host_shim.h"
#else
#include "kernel.h"
#endif

enum { SBC_FREQ_16K, SBC_FREQ_32K, SBC_FREQ_44K1, SBC_FREQ_48K };
enum { SBC_MODE_MONO, SBC_MODE_DUAL, SBC_MODE_STEREO, SBC_MODE_JOINT };
enum { SBC_ALLOC_LOUDNESS, SBC_ALLOC_SNR };

typedef struct {
    int freq, blocks, mode, alloc, bitpool, channels;
    float X[2][80];                  /* histórico do filtro de análise */
} sbc_enc_t;

void sbc_init(sbc_enc_t *e, int freq, int blocks, int mode, int alloc, int bitpool);
int  sbc_frame_len(const sbc_enc_t *e);
int  sbc_encode(sbc_enc_t *e, const i16 *pcm, u8 *out);   /* blocks*8 quadros PCM -> 1 quadro SBC */
