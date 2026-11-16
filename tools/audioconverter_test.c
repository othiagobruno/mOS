/* Teste de host do AudioConverter decodificando (userland/libsystem/coreaudio.c, compilado de verdade).
 * O app entrega um pacote por vez pelo callback (com AudioStreamPacketDescription), como a Firefox faz no
 * AppleATDecoder, e devolve um erro próprio quando acaba. A saída float32 é comparada com o cx_audiodec (arquivo
 * inteiro, validado contra o ffmpeg em tools/midia_audio_test.sh). Rodado por tools/midia_audio_test.sh.
 * uso: audioconverter_test arquivo.mp4 [taxa de saída] */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mp4demux.h"
typedef int OSStatus;
typedef struct {
    double mSampleRate;
    unsigned mFormatID, mFormatFlags, mBytesPerPacket, mFramesPerPacket, mBytesPerFrame, mChannelsPerFrame, mBitsPerChannel, mReserved;
} asbd_t;
typedef struct { unsigned mNumberChannels, mDataByteSize; void *mData; } abuf_t;
typedef struct { unsigned mNumberBuffers, pad; abuf_t mBuffers[1]; } ablist_t;
typedef struct { long long mStartOffset; unsigned mVariableFramesInPacket, mDataByteSize; } aspd_t;
typedef OSStatus (*acinput_fn)(void *conv, unsigned *npackets, ablist_t *data, aspd_t **pd, void *ud);
OSStatus AudioConverterNew(const asbd_t *src, const asbd_t *dst, void **out);
OSStatus AudioConverterSetProperty(void *c, unsigned id_, unsigned sz, const void *d);
OSStatus AudioConverterFillComplexBuffer(void *c, acinput_fn proc, void *ud, unsigned *npackets, ablist_t *out, aspd_t *pd);
OSStatus AudioConverterDispose(void *c);
typedef struct { int kind, status; const char *limit; int rate, channels, frames; short *pcm; } cx_audiodec_out;
int cx_audiodec(const void *b, unsigned long long n, cx_audiodec_out *o);
#define FCC(a, b, c, d) ((unsigned)(a) << 24 | (unsigned)(b) << 16 | (unsigned)(c) << 8 | (unsigned)(d))
#define NO_MORE FCC('n', 'm', 'd', '!')

typedef struct { mp4_file *f; mp4_track *t; uint32_t next; uint8_t *buf; aspd_t pd; } feed_t;
static OSStatus feed(void *conv, unsigned *np, ablist_t *data, aspd_t **pd, void *ud) {
    (void)conv;
    feed_t *s = ud;
    if (s->next >= s->t->nsamples) { *np = 0; return NO_MORE; }
    uint32_t sz = s->t->s[s->next].size;
    mp4_read_sample(s->f, s->t, s->next, s->buf);
    s->next++;
    data->mNumberBuffers = 1;
    data->mBuffers[0].mData = s->buf;
    data->mBuffers[0].mDataByteSize = sz;
    data->mBuffers[0].mNumberChannels = s->t->channels;
    s->pd.mStartOffset = 0;
    s->pd.mDataByteSize = sz;
    s->pd.mVariableFramesInPacket = 0;
    if (pd) *pd = &s->pd;
    *np = 1;
    return 0;
}

int main(int argc, char **argv) {
    FILE *fp = fopen(argv[1], "rb"); fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    uint8_t *b = malloc(n); if (fread(b, 1, n, fp) != (size_t)n) return 2; fclose(fp);
    double out_rate = argc > 2 ? atof(argv[2]) : 0;
    int esds_cookie = argc > 3;
    cx_audiodec_out ref;
    if (!cx_audiodec(b, n, &ref) || ref.status) { puts("FALHA ref"); return 1; }
    mp4_file m;
    if (mp4_open_mem(&m, b, n)) { puts("FALHA mp4"); return 1; }
    mp4_track *t = &m.t[mp4_find_track(&m, MP4_FOURCC('s', 'o', 'u', 'n'))];
    asbd_t src = {0}, dst = {0};
    src.mSampleRate = t->sample_rate;
    src.mChannelsPerFrame = t->channels;
    src.mFramesPerPacket = 1024;
    if (t->codec == MP4_FOURCC('O', 'p', 'u', 's')) { src.mFormatID = FCC('o', 'p', 'u', 's'); src.mSampleRate = 48000; src.mFramesPerPacket = 960; }
    else if (t->oti == 0x6B || t->oti == 0x69) { src.mFormatID = FCC('.', 'm', 'p', '3'); src.mFramesPerPacket = 1152; }
    else if (t->codec == MP4_FOURCC('a', 'l', 'a', 'c')) { src.mFormatID = FCC('a', 'l', 'a', 'c'); src.mFramesPerPacket = 4096; }
    else src.mFormatID = FCC('a', 'a', 'c', ' ');
    int ch = ref.channels;
    dst.mSampleRate = out_rate > 0 ? out_rate : ref.rate;
    dst.mFormatID = FCC('l', 'p', 'c', 'm');
    dst.mFormatFlags = 1 | 8; /* float, packed */
    dst.mChannelsPerFrame = ch;
    dst.mBitsPerChannel = 32;
    dst.mFramesPerPacket = 1;
    dst.mBytesPerFrame = dst.mBytesPerPacket = 4 * ch;
    void *conv = 0;
    if (AudioConverterNew(&src, &dst, &conv)) { puts("FALHA AudioConverterNew"); return 1; }
    if (src.mFormatID == FCC('a', 'a', 'c', ' ')) {
        if (esds_cookie) {
            /* o mesmo cookie que o AudioFile/AudioFileStream entregam: ES_Descriptor com o ASC dentro */
            uint8_t ck[64], *p = ck;
            *p++ = 3; *p++ = (uint8_t)(3 + 2 + 13 + 2 + t->cfg_len); *p++ = 0; *p++ = 1; *p++ = 0;
            *p++ = 4; *p++ = (uint8_t)(13 + 2 + t->cfg_len); *p++ = 0x40; *p++ = 0x15; memset(p, 0, 11); p += 11;
            *p++ = 5; *p++ = (uint8_t)t->cfg_len; memcpy(p, t->cfg, t->cfg_len); p += t->cfg_len;
            if (AudioConverterSetProperty(conv, FCC('d', 'm', 'g', 'c'), (unsigned)(p - ck), ck)) { puts("FALHA cookie"); return 1; }
        } else if (AudioConverterSetProperty(conv, FCC('d', 'm', 'g', 'c'), t->cfg_len, t->cfg)) { puts("FALHA cookie"); return 1; }
    }
    /* ALAC: o cookie é o conteúdo da caixa 'alac' (versão/flags + ALACSpecificConfig), como o AudioFile entrega */
    if (src.mFormatID == FCC('a', 'l', 'a', 'c') && AudioConverterSetProperty(conv, FCC('d', 'm', 'g', 'c'), t->cfg_len, t->cfg)) { puts("FALHA cookie"); return 1; }
    feed_t fd = {&m, t, 0, malloc(1 << 20), {0}};
    size_t cap = (size_t)(ref.frames * (dst.mSampleRate / ref.rate)) + (1 << 16), len = 0;
    float *pcm = malloc(cap * ch * 4);
    OSStatus last = 0;
    for (;;) {
        unsigned want = 1000;
        uint8_t mem[64] = {0};
        ablist_t *ol = (ablist_t *)mem;
        ol->mNumberBuffers = 1;
        ol->mBuffers[0].mNumberChannels = ch;
        ol->mBuffers[0].mData = pcm + len * ch;
        ol->mBuffers[0].mDataByteSize = want * 4 * ch;
        last = AudioConverterFillComplexBuffer(conv, feed, &fd, &want, ol, 0);
        len += want;
        if (last || !want) break;
    }
    AudioConverterDispose(conv);
    if (last != (OSStatus)NO_MORE) { printf("FALHA o erro do callback não voltou (%d)\n", last); return 1; }
    /* priming: o AudioConverter, como no macOS, entrega o fluxo inteiro; o cx_audiodec corta pela edit list */
    long skip = t->edit_media_time > 0 ? (long)(t->edit_media_time * dst.mSampleRate / t->timescale + 0.5) : 0;
    if (src.mFormatID == FCC('o', 'p', 'u', 's') && t->edit_media_time < 0) skip = 312;
    if (out_rate <= 0) {
        long diff = 0, mx = 0;
        for (long i = 0; i < (long)ref.frames * ch; i++) {
            long g = lrintf(pcm[skip * ch + i] * 32768.0f), d = labs(g - ref.pcm[i]);
            if (d > 1) diff++;
            if (d > mx) mx = d;
        }
        if ((long)len - skip < ref.frames) { printf("FALHA curto %ld < %d\n", (long)len - skip, ref.frames); return 1; }
        printf("%s %s %.0f Hz %d ch %ld quadros, %ld amostras fora de ±1 (máx %ld)\n", diff ? "FALHA" : "ac ok",
               esds_cookie ? "esds" : "asc", dst.mSampleRate, ch, (long)len, diff, mx);
        return diff != 0;
    }
    /* reamostrado: duração na proporção e correlação com a referência reamostrada por interpolação */
    /* posição exata do priming na taxa de saída (arredondar desalinha a fase em até meia amostra) */
    double skipx = t->edit_media_time > 0 ? t->edit_media_time * dst.mSampleRate / t->timescale : 0;
    double ratio = dst.mSampleRate / ref.rate, dot = 0, eg = 0, er = 0;
    long expect = (long)((len > 0 ? (double)ref.frames : 0) * ratio);
    for (long i = (long)(skip) + 2000; i < (long)len - 2000 && (double)(i - skip) / ratio < ref.frames - 2; i += 3) {
        double p = ((double)i - skipx) / ratio;
        long k = (long)p;
        double fr = p - k, r = ref.pcm[k * ch] * (1 - fr) + ref.pcm[(k + 1) * ch] * fr, g = pcm[i * ch] * 32768.0;
        dot += g * r; eg += g * g; er += r * r;
    }
    double c = dot / sqrt(eg * er);
    int ok = c > 0.999 && labs((long)len - skip - expect) < 2048;
    printf("%s reamostrado %.0f Hz %ld quadros (esperado ~%ld) corr %.5f\n", ok ? "ac ok" : "FALHA", dst.mSampleRate, (long)len - skip, expect, c);
    return !ok;
}
