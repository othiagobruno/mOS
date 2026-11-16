/* Teste de host do AudioConverter codificando (userland/libsystem/coreaudio.c de verdade): LPCM float32 -> AAC/HE-AAC/
 * Opus pelo callback, cookie lido do conversor (kAudioConverterCompressionMagicCookie), pacotes decodificados por outro
 * AudioConverter com esse cookie, e a saída comparada com a origem alinhando o priming. Rodado por tools/midia_audio_test.sh.
 * uso: audioconverter_enc_test aac|aach|aacp|opus taxa canais bitrate [buffer_pequeno]  → "acenc ok ..." */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int OSStatus;
typedef struct { double mSampleRate; unsigned mFormatID, mFormatFlags, mBytesPerPacket, mFramesPerPacket, mBytesPerFrame, mChannelsPerFrame, mBitsPerChannel, mReserved; } asbd_t;
typedef struct { unsigned mNumberChannels, mDataByteSize; void *mData; } abuf_t;
typedef struct { unsigned mNumberBuffers, pad; abuf_t mBuffers[1]; } ablist_t;
typedef struct { long long mStartOffset; unsigned mVariableFramesInPacket, mDataByteSize; } aspd_t;
typedef OSStatus (*acinput_fn)(void *conv, unsigned *np, ablist_t *data, aspd_t **pd, void *ud);
OSStatus AudioConverterNew(const asbd_t *src, const asbd_t *dst, void **out);
OSStatus AudioConverterSetProperty(void *c, unsigned id_, unsigned sz, const void *d);
OSStatus AudioConverterGetProperty(void *c, unsigned id_, unsigned *sz, void *d);
OSStatus AudioConverterGetPropertyInfo(void *c, unsigned id_, unsigned *sz, unsigned char *w);
OSStatus AudioConverterFillComplexBuffer(void *c, acinput_fn proc, void *ud, unsigned *np, ablist_t *out, aspd_t *pd);
OSStatus AudioConverterDispose(void *c);
#define FCC(a, b, c, d) ((unsigned)(a) << 24 | (unsigned)(b) << 16 | (unsigned)(c) << 8 | (unsigned)(d))
#define NO_MORE FCC('n', 'm', 'd', '!')
typedef struct { const float *pcm; long n, at; int ch; } src_t;
static OSStatus feed_pcm(void *conv, unsigned *np, ablist_t *data, aspd_t **pd, void *ud) {
    (void)conv; (void)pd;
    src_t *s = ud;
    if (s->at >= s->n) { *np = 0; return NO_MORE; }
    unsigned k = *np > 333 ? 333 : *np;   /* pedaços que não casam com o tamanho do pacote */
    if (s->at + k > s->n) k = (unsigned)(s->n - s->at);
    data->mNumberBuffers = 1;
    data->mBuffers[0].mNumberChannels = s->ch;
    data->mBuffers[0].mData = (void *)(s->pcm + s->at * s->ch);
    data->mBuffers[0].mDataByteSize = k * 4 * s->ch;
    s->at += k;
    *np = k;
    return 0;
}
typedef struct { unsigned char *buf; aspd_t *pd; int np, i; } pk_t;
static OSStatus feed_pkt(void *conv, unsigned *np, ablist_t *data, aspd_t **pd, void *ud) {
    (void)conv;
    pk_t *p = ud;
    if (p->i >= p->np) { *np = 0; return NO_MORE; }
    data->mNumberBuffers = 1;
    data->mBuffers[0].mData = p->buf + p->pd[p->i].mStartOffset;
    data->mBuffers[0].mDataByteSize = p->pd[p->i].mDataByteSize;
    static aspd_t one;
    one = p->pd[p->i]; one.mStartOffset = 0;
    if (pd) *pd = &one;
    p->i++;
    *np = 1;
    return 0;
}
int main(int argc, char **argv) {
    const char *k = argv[1];
    unsigned fmt = !strcmp(k, "opus") ? FCC('o', 'p', 'u', 's') : !strcmp(k, "aach") ? FCC('a', 'a', 'c', 'h') : !strcmp(k, "aacp") ? FCC('a', 'a', 'c', 'p') : FCC('a', 'a', 'c', ' ');
    int rate = atoi(argv[2]), ch = atoi(argv[3]), small = argc > 5;
    unsigned br = (unsigned)atoi(argv[4]);
    long n = rate * 3;
    float *pcm = malloc(n * ch * sizeof(float));
    for (long i = 0; i < n; i++) for (int c = 0; c < ch; c++) { double t = (double)i / rate; pcm[i * ch + c] = (float)(0.3 * sin(2 * M_PI * (c ? 2500 - 600 * t : 300 + 500 * t) * t)); }
    asbd_t s = {rate, FCC('l', 'p', 'c', 'm'), 1 | 8, 4u * ch, 1, 4u * ch, (unsigned)ch, 32, 0};
    asbd_t d = {rate, fmt, 0, 0, 0, 0, (unsigned)ch, 0, 0};
    void *e = 0;
    if (AudioConverterNew(&s, &d, &e)) { puts("acenc FALHA New"); return 1; }
    if (AudioConverterSetProperty(e, FCC('b', 'r', 'a', 't'), 4, &br)) { puts("acenc FALHA brat"); return 1; }
    unsigned csz = 0; unsigned char w;
    if (AudioConverterGetPropertyInfo(e, FCC('k', 'm', 'g', 'c'), &csz, &w) || !csz) { puts("acenc FALHA tamanho do cookie"); return 1; }
    unsigned char cookie[256]; unsigned cn = sizeof cookie;
    if (AudioConverterGetProperty(e, FCC('k', 'm', 'g', 'c'), &cn, cookie) || cn != csz) { puts("acenc FALHA cookie"); return 1; }
    asbd_t od; unsigned osz = 40;
    AudioConverterGetProperty(e, FCC('a', 'c', 'o', 'd'), &osz, &od);
    unsigned maxp = 0, msz = 4; AudioConverterGetProperty(e, FCC('x', 'o', 'p', 's'), &msz, &maxp);
    unsigned prim[2] = {0, 0}; unsigned psz = 8; AudioConverterGetProperty(e, FCC('p', 'r', 'i', 'm'), &psz, prim);
    src_t src = {pcm, n, 0, ch};
    unsigned char *all = malloc(4 << 20); aspd_t *apd = malloc(sizeof(aspd_t) * 20000); long used = 0; int npk = 0;
    for (;;) {
        unsigned char buf[16384]; aspd_t pd[64];
        unsigned want = 32;
        ablist_t ol = {1, 0, {{(unsigned)ch, small ? maxp + 10 : sizeof buf, buf}}};
        OSStatus r = AudioConverterFillComplexBuffer(e, feed_pcm, &src, &want, &ol, pd);
        for (unsigned i = 0; i < want; i++) { memcpy(all + used, buf + pd[i].mStartOffset, pd[i].mDataByteSize); apd[npk] = pd[i]; apd[npk].mStartOffset = used; used += pd[i].mDataByteSize; npk++; }
        if (!want) { if (r != (OSStatus)NO_MORE && r) { printf("acenc FALHA Fill %d\n", r); return 1; } break; }
    }
    AudioConverterDispose(e);
    /* decodifica com o cookie */
    asbd_t pc = {od.mSampleRate, FCC('l', 'p', 'c', 'm'), 1 | 8, 4u * ch, 1, 4u * ch, (unsigned)ch, 32, 0};
    void *dc = 0;
    if (AudioConverterNew(&od, &pc, &dc)) { puts("acenc FALHA decodificador"); return 1; }
    AudioConverterSetProperty(dc, FCC('d', 'm', 'g', 'c'), cn, cookie);
    pk_t pk = {all, apd, npk, 0};
    float *out = calloc((n + 96000) * ch, sizeof(float)); long got = 0;
    for (;;) {
        unsigned want = 4096;
        ablist_t ol = {1, 0, {{(unsigned)ch, 4096 * 4 * ch, out + got * ch}}};
        OSStatus r = AudioConverterFillComplexBuffer(dc, feed_pkt, &pk, &want, &ol, 0);
        got += want;
        if (!want || r) break;
    }
    double dec_rate = od.mSampleRate;   /* Opus decodifica a 48 kHz */
    int best = 0; double bc = -2;
    long lim = (long)(n * dec_rate / rate);
    for (int off = 0; off < 6000 && off < got; off++) {
        double dot = 0, ea = 0, eb = 0;
        for (long i = lim / 10; i < lim - lim / 10 && i + off < got; i += 5) { double a = pcm[(long)(i * rate / dec_rate) * ch], b = out[(i + off) * ch]; dot += a * b; ea += a * a; eb += b * b; }
        double c = dot / sqrt(ea * eb + 1e-12);
        if (c > bc) { bc = c; best = off; }
    }
    double kbps = used * 8.0 / 3 / 1000;
    int ok = bc > 0.98 && npk > 10 && (!br || (kbps > br / 1000.0 * 0.7 && kbps < br / 1000.0 * 1.3)) && (int)prim[0] > 0 && od.mFramesPerPacket > 0;
    printf("%s %s %d Hz %d ch%s: %d pacotes de %u quadros, %.0f kbit/s, cookie %u bytes, priming %u (medido %d), correlação %.4f\n",
           ok ? "acenc ok" : "acenc FALHA", k, rate, ch, small ? " (buffer pequeno)" : "", npk, od.mFramesPerPacket, kbps, cn, prim[0], best, bc);
    return !ok;
}
