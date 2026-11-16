/* AAC no mOS: decodificador do Fraunhofer FDK AAC 2.0.3 (userland/third_party/fdk-aac, licença em NOTICE)
 * atrás de uma interface C. AAC-LC, HE-AAC v1 (SBR), HE-AAC v2 (PS), AAC-LD/ELD e xHE-AAC (USAC).
 * Entrada: AudioSpecificConfig (MP4/CAF) ou fluxo ADTS/LATM. Saída: PCM 16 bits intercalado.
 * Compila como C++ freestanding (sem exceções, RTTI nem libc++). */
#include "aacdecoder_lib.h"
#include "aacenc_lib.h"
#include "aac_glue.h"

extern "C" {
void *malloc(unsigned long n);
void *realloc(void *p, unsigned long n);
void free(void *p);
void *memmove(void *d, const void *s, unsigned long n);
void *memset(void *d, int c, unsigned long n);

struct cx_aac {
    HANDLE_AACDECODER h;
    int adts;
};

void *cx_aac_open(const uint8_t *asc, int asc_n, int transport, int max_channels) {
    TRANSPORT_TYPE tt = transport == CX_AAC_ADTS ? TT_MP4_ADTS : transport == CX_AAC_LATM ? TT_MP4_LATM_MCP1 : TT_MP4_RAW;
    HANDLE_AACDECODER h = aacDecoder_Open(tt, 1);
    if (!h) return 0;
    if (tt == TT_MP4_RAW) {
        UCHAR *cfg[1] = {(UCHAR *)asc};
        UINT len[1] = {(UINT)asc_n};
        if (!asc || asc_n < 2 || aacDecoder_ConfigRaw(h, cfg, len) != AAC_DEC_OK) { aacDecoder_Close(h); return 0; }
    }
    if (max_channels > 0) aacDecoder_SetParam(h, AAC_PCM_MAX_OUTPUT_CHANNELS, max_channels);
    /* o limitador do FDK atrasa a saída 15 ms (lookahead); o CoreAudio não tem esse atraso */
    aacDecoder_SetParam(h, AAC_PCM_LIMITER_ENABLE, 0);
    /* ocultação 2 (interpolação de energia) segura um quadro inteiro; a 1 (ruído) não atrasa */
    aacDecoder_SetParam(h, AAC_CONCEAL_METHOD, 1);
    cx_aac *c = (cx_aac *)malloc(sizeof(cx_aac));
    if (!c) { aacDecoder_Close(h); return 0; }
    c->h = h;
    c->adts = tt != TT_MP4_RAW;
    return c;
}

void cx_aac_close(void *p) {
    cx_aac *c = (cx_aac *)p;
    if (!c) return;
    aacDecoder_Close(c->h);
    free(c);
}

/* Um pacote cru (MP4/CAF). Devolve quadros por canal, 0 quando o decodificador ainda não tem saída, < 0 erro. */
int cx_aac_decode(void *p, const uint8_t *pkt, int n, int16_t *pcm, int max_samples, int *channels, int *rate) {
    cx_aac *c = (cx_aac *)p;
    if (!c || !pkt || n < 1) return -1;
    UCHAR *buf[1] = {(UCHAR *)pkt};
    UINT size[1] = {(UINT)n}, valid = (UINT)n;
    if (aacDecoder_Fill(c->h, buf, size, &valid) != AAC_DEC_OK) return -1;
    AAC_DECODER_ERROR e = aacDecoder_DecodeFrame(c->h, pcm, max_samples, 0);
    if (e == AAC_DEC_NOT_ENOUGH_BITS) return 0;
    if (e != AAC_DEC_OK) return -2;
    CStreamInfo *si = aacDecoder_GetStreamInfo(c->h);
    if (!si || si->numChannels < 1) return -2;
    if (channels) *channels = si->numChannels;
    if (rate) *rate = si->sampleRate;
    return si->frameSize;
}

/* Fluxo ADTS inteiro. 0 ok; *out com free. */
int cx_aac_decode_adts(const uint8_t *b, uint64_t n, int max_channels, int16_t **out, int *channels, int *rate, int *frames) {
    *out = 0;
    *channels = *rate = *frames = 0;
    cx_aac *c = (cx_aac *)cx_aac_open(0, 0, CX_AAC_ADTS, max_channels);
    if (!c) return -3;
    unsigned long cap = 1 << 16, len = 0; /* amostras (todas as faixas) */
    int16_t *pcm = (int16_t *)malloc(cap * sizeof(int16_t));
    int16_t *tmp = (int16_t *)malloc(8 * 2048 * 2 * sizeof(int16_t));
    if (!pcm || !tmp) { free(pcm); free(tmp); cx_aac_close(c); return -3; }
    uint64_t at = 0;
    int ch = 0, sr = 0, rc = 0;
    while (!rc) {
        if (at < n) {
            UINT chunk = n - at > 65536 ? 65536 : (UINT)(n - at);
            UCHAR *buf[1] = {(UCHAR *)(b + at)};
            UINT size[1] = {chunk}, valid = chunk;
            if (aacDecoder_Fill(c->h, buf, size, &valid) != AAC_DEC_OK) break;
            at += chunk - valid;
        }
        for (;;) {
            AAC_DECODER_ERROR e = aacDecoder_DecodeFrame(c->h, tmp, 8 * 2048 * 2, 0);
            if (e == AAC_DEC_NOT_ENOUGH_BITS) break;
            if (e != AAC_DEC_OK) {
                if (IS_OUTPUT_VALID(e)) continue;
                if (e == AAC_DEC_TRANSPORT_SYNC_ERROR || IS_DECODE_ERROR(e)) continue;
                break;
            }
            CStreamInfo *si = aacDecoder_GetStreamInfo(c->h);
            if (!si || si->numChannels < 1) continue;
            if (!ch) { ch = si->numChannels; sr = si->sampleRate; }
            if (si->numChannels != ch) continue; /* troca de configuração no meio: fica o primeiro leiaute */
            unsigned long add = (unsigned long)si->frameSize * (unsigned long)ch;
            if (len + add > cap) {
                unsigned long nc = cap * 2 + add;
                int16_t *np = (int16_t *)realloc(pcm, nc * sizeof(int16_t));
                if (!np) { rc = -3; break; }
                pcm = np;
                cap = nc;
            }
            memmove(pcm + len, tmp, add * sizeof(int16_t));
            len += add;
        }
        if (at >= n) {
            /* esvazia o que sobrou no buffer interno */
            AAC_DECODER_ERROR e = aacDecoder_DecodeFrame(c->h, tmp, 8 * 2048 * 2, 0);
            if (e == AAC_DEC_OK) {
                CStreamInfo *si = aacDecoder_GetStreamInfo(c->h);
                if (si && si->numChannels == ch) {
                    unsigned long add = (unsigned long)si->frameSize * (unsigned long)ch;
                    if (len + add <= cap) { memmove(pcm + len, tmp, add * sizeof(int16_t)); len += add; }
                    continue;
                }
            }
            break;
        }
    }
    free(tmp);
    cx_aac_close(c);
    if (rc || !ch) { free(pcm); return rc ? rc : -2; }
    *out = pcm;
    *channels = ch;
    *rate = sr;
    *frames = (int)(len / (unsigned long)ch);
    return 0;
}

/* ---- codificador */
struct cx_aacenc { HANDLE_AACENCODER h; int ch; };
void *cx_aacenc_open(int rate, int channels, int bitrate, int aot, int adts, uint8_t *asc, int *asc_n, int *frame, int *delay) {
    if (channels < 1 || channels > 8 || (aot == 29 && channels != 2)) return 0;
    HANDLE_AACENCODER h = 0;
    if (aacEncOpen(&h, 0, (UINT)channels) != AACENC_OK) return 0;
    CHANNEL_MODE mode = channels == 1 ? MODE_1 : channels == 2 ? MODE_2 : channels == 3 ? MODE_1_2 : channels == 4 ? MODE_1_2_1 :
                        channels == 5 ? MODE_1_2_2 : channels == 6 ? MODE_1_2_2_1 : channels == 7 ? MODE_1_2_2_2_1 : MODE_7_1_BACK;
    if (aacEncoder_SetParam(h, AACENC_AOT, (UINT)(aot == 5 || aot == 29 ? aot : 2)) != AACENC_OK ||
        aacEncoder_SetParam(h, AACENC_SAMPLERATE, (UINT)rate) != AACENC_OK ||
        aacEncoder_SetParam(h, AACENC_CHANNELMODE, mode) != AACENC_OK ||
        aacEncoder_SetParam(h, AACENC_CHANNELORDER, 1) != AACENC_OK ||   /* ordem WAV (L R C LFE ...) */
        aacEncoder_SetParam(h, AACENC_TRANSMUX, adts ? TT_MP4_ADTS : TT_MP4_RAW) != AACENC_OK ||
        aacEncoder_SetParam(h, AACENC_AFTERBURNER, 1) != AACENC_OK ||
        (bitrate > 0 && aacEncoder_SetParam(h, AACENC_BITRATE, (UINT)bitrate) != AACENC_OK) ||
        aacEncEncode(h, NULL, NULL, NULL, NULL) != AACENC_OK) {
        aacEncClose(&h);
        return 0;
    }
    AACENC_InfoStruct info;
    if (aacEncInfo(h, &info) != AACENC_OK) { aacEncClose(&h); return 0; }
    if (asc && asc_n) { int n = (int)info.confSize; if (n > *asc_n) n = *asc_n; memmove(asc, info.confBuf, (unsigned long)n); *asc_n = n; }
    if (frame) *frame = (int)info.frameLength;
    if (delay) *delay = (int)info.nDelay;
    cx_aacenc *e = (cx_aacenc *)malloc(sizeof(cx_aacenc));
    if (!e) { aacEncClose(&h); return 0; }
    e->h = h;
    e->ch = channels;
    return e;
}
int cx_aacenc_encode(void *p, const int16_t *pcm, int frames, uint8_t *out, int cap) {
    cx_aacenc *e = (cx_aacenc *)p;
    if (!e || !out || cap <= 0) return -1;
    void *ib = (void *)pcm, *ob = out;
    INT iid = IN_AUDIO_DATA, oid = OUT_BITSTREAM_DATA, isz = (INT)(frames > 0 ? frames * e->ch * 2 : 0), osz = cap, iel = 2, oel = 1;
    AACENC_BufDesc in, o;
    in.numBufs = 1; in.bufs = &ib; in.bufferIdentifiers = &iid; in.bufSizes = &isz; in.bufElSizes = &iel;
    o.numBufs = 1; o.bufs = &ob; o.bufferIdentifiers = &oid; o.bufSizes = &osz; o.bufElSizes = &oel;
    AACENC_InArgs ia;
    AACENC_OutArgs oa;
    memset(&ia, 0, sizeof ia);
    memset(&oa, 0, sizeof oa);
    ia.numInSamples = frames > 0 ? frames * e->ch : -1;
    AACENC_ERROR r = aacEncEncode(e->h, &in, &o, &ia, &oa);   /* numInSamples -1: esvazia o atraso do fim */
    if (r == AACENC_ENCODE_EOF) return 0;
    if (r != AACENC_OK) return -2;
    return oa.numOutBytes;
}
void cx_aacenc_close(void *p) {
    cx_aacenc *e = (cx_aacenc *)p;
    if (!e) return;
    aacEncClose(&e->h);
    free(e);
}
}
