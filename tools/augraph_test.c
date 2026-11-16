/* Teste de host do AUGraph e do AUMultiChannelMixer (userland/libsystem/coreaudio.c compilado de verdade).
 * O render da unidade de saída é chamado como o motor do mOS chama (o callback em kAudioUnitProperty_SetRenderCallback),
 * sem hardware. Rodado por tools/midia_audio_test.sh. Saída: "augraph ok ..." por caso. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int OSStatus;
typedef struct {
    double mSampleRate;
    unsigned mFormatID, mFormatFlags, mBytesPerPacket, mFramesPerPacket, mBytesPerFrame, mChannelsPerFrame, mBitsPerChannel, mReserved;
} asbd_t;
typedef struct { unsigned mNumberChannels, mDataByteSize; void *mData; } abuf_t;
typedef struct { unsigned mNumberBuffers, pad; abuf_t mBuffers[2]; } ablist_t;
typedef struct { double t, h; unsigned char rest[64]; } ats_t;
typedef struct { unsigned type, sub, manu, flags, mask; } acdesc_t;
typedef OSStatus (*rcb)(void *ref, unsigned *flags, const ats_t *ts, unsigned bus, unsigned n, ablist_t *l);
typedef struct { rcb fn; void *ref; } rcs_t;
#define FCC(a, b, c, d) ((unsigned)(a) << 24 | (unsigned)(b) << 16 | (unsigned)(c) << 8 | (unsigned)(d))
void *AudioComponentFindNext(void *after, const acdesc_t *d);
OSStatus AudioComponentInstanceNew(void *c, void **out);
OSStatus AudioComponentInstanceDispose(void *u);
OSStatus AudioUnitInitialize(void *u);
OSStatus AudioUnitSetProperty(void *u, unsigned id, unsigned sc, unsigned el, const void *d, unsigned sz);
OSStatus AudioUnitGetProperty(void *u, unsigned id, unsigned sc, unsigned el, void *d, unsigned *sz);
OSStatus AudioUnitSetParameter(void *u, unsigned id, unsigned sc, unsigned el, float v, unsigned off);
OSStatus AudioUnitRender(void *u, unsigned *flags, const ats_t *ts, unsigned bus, unsigned n, ablist_t *l);
OSStatus NewAUGraph(void **g);
OSStatus DisposeAUGraph(void *g);
OSStatus AUGraphAddNode(void *g, const acdesc_t *d, int *n);
OSStatus AUGraphOpen(void *g);
OSStatus AUGraphNodeInfo(void *g, int n, acdesc_t *d, void **u);
OSStatus AUGraphConnectNodeInput(void *g, int s, unsigned so, int d, unsigned di);
OSStatus AUGraphSetNodeInputCallback(void *g, int d, unsigned di, const rcs_t *cb);
OSStatus AUGraphDisconnectNodeInput(void *g, int d, unsigned di);
OSStatus AUGraphInitialize(void *g);
OSStatus AUGraphUpdate(void *g, unsigned char *up);
OSStatus AUGraphGetNodeCount(void *g, unsigned *n);
OSStatus MusicDeviceMIDIEvent(void *u, unsigned st, unsigned d1, unsigned d2, unsigned off);

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FALHA "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

/* fonte constante: valor em ref, formato float32 intercalado com os canais do primeiro buffer */
static OSStatus konst(void *ref, unsigned *flags, const ats_t *ts, unsigned bus, unsigned n, ablist_t *l) {
    (void)flags; (void)ts; (void)bus;
    float v = *(float *)ref;
    for (unsigned b = 0; b < l->mNumberBuffers; b++) {
        float *p = l->mBuffers[b].mData;
        for (unsigned i = 0; i < n * l->mBuffers[b].mNumberChannels; i++) p[i] = v;
    }
    return 0;
}
/* seno contínuo mono de 440 Hz na taxa do barramento (24 kHz): conta os quadros pedidos */
typedef struct { double rate, phase; long pulled; } sine_t;
static OSStatus sine(void *ref, unsigned *flags, const ats_t *ts, unsigned bus, unsigned n, ablist_t *l) {
    (void)flags; (void)ts; (void)bus;
    sine_t *s = ref;
    float *p = l->mBuffers[0].mData;
    for (unsigned i = 0; i < n; i++) { p[i] = (float)(0.5 * sin(s->phase)); s->phase += 2 * M_PI * 440 / s->rate; }
    s->pulled += n;
    return 0;
}
static asbd_t fmt(double rate, unsigned ch, int noninterleaved) {
    asbd_t f = {0};
    f.mSampleRate = rate;
    f.mFormatID = FCC('l', 'p', 'c', 'm');
    f.mFormatFlags = 1 | 8 | (noninterleaved ? 32 : 0);
    f.mChannelsPerFrame = ch;
    f.mBitsPerChannel = 32;
    f.mFramesPerPacket = 1;
    f.mBytesPerFrame = f.mBytesPerPacket = noninterleaved ? 4 : 4 * ch;
    return f;
}
static float L[4096], R[4096];
static ablist_t outlist(unsigned n) {
    ablist_t l = {2, 0, {{1, n * 4, L}, {1, n * 4, R}}};
    return l;
}

int main(void) {
    acdesc_t dm = {FCC('a', 'u', 'm', 'x'), FCC('m', 'c', 'm', 'x'), FCC('a', 'p', 'p', 'l'), 0, 0};
    acdesc_t dout = {FCC('a', 'u', 'o', 'u'), FCC('d', 'e', 'f', ' '), FCC('a', 'p', 'p', 'l'), 0, 0};
    ats_t ts = {0};
    void *mx = 0;
    CHECK(AudioComponentInstanceNew(AudioComponentFindNext(0, &dm), &mx) == 0 && mx, "mixer não abriu");
    unsigned v = 0, sz = 4;
    CHECK(AudioUnitGetProperty(mx, 11, 1, 0, &v, &sz) == 0 && v == 8, "ElementCount padrão %u", v);
    v = 3;
    CHECK(AudioUnitSetProperty(mx, 11, 1, 0, &v, 4) == 0, "ElementCount 3");
    asbd_t out = fmt(48000, 2, 1), mono = fmt(48000, 1, 0), st = fmt(48000, 2, 0), lo = fmt(24000, 1, 0);
    CHECK(AudioUnitSetProperty(mx, 8, 2, 0, &out, 40) == 0, "formato de saída");
    CHECK(AudioUnitSetProperty(mx, 8, 1, 0, &mono, 40) == 0 && AudioUnitSetProperty(mx, 8, 1, 1, &st, 40) == 0, "formatos de entrada");
    CHECK(AudioUnitSetProperty(mx, 8, 1, 5, &st, 40) == -10877, "barramento inexistente devolve InvalidElement");
    float k0 = 0.25f, k1 = 0.5f;
    rcs_t c0 = {konst, &k0}, c1 = {konst, &k1};
    CHECK(AudioUnitSetProperty(mx, 23, 1, 0, &c0, 16) == 0 && AudioUnitSetProperty(mx, 23, 1, 1, &c1, 16) == 0, "callbacks");
    AudioUnitSetParameter(mx, 0, 1, 1, 0.5f, 0);   /* volume do barramento 1 */
    AudioUnitSetParameter(mx, 2, 1, 0, -1.0f, 0);  /* barramento 0 todo à esquerda */
    ablist_t ol = outlist(512);
    unsigned fl = 0;
    CHECK(AudioUnitRender(mx, &fl, &ts, 0, 512, &ol) == -10867, "render sem inicializar devolve Uninitialized");
    AudioUnitInitialize(mx);
    CHECK(AudioUnitRender(mx, &fl, &ts, 0, 512, &ol) == 0, "render");
    CHECK(fabsf(L[100] - 0.5f) < 1e-6f && fabsf(R[100] - 0.25f) < 1e-6f, "mistura L %f R %f (esperado 0.5 / 0.25)", L[100], R[100]);
    printf("augraph ok mixer volume/pan: L %.3f R %.3f\n", L[100], R[100]);
    AudioUnitSetParameter(mx, 1, 1, 1, 0, 0);       /* desliga o barramento 1 */
    AudioUnitSetParameter(mx, 0, 2, 0, 2.0f, 0);    /* volume geral 2 */
    ol = outlist(512);
    AudioUnitRender(mx, &fl, &ts, 0, 512, &ol);
    CHECK(fabsf(L[7] - 0.5f) < 1e-6f && fabsf(R[7]) < 1e-6f, "enable/volume geral L %f R %f", L[7], R[7]);
    printf("augraph ok mixer enable + volume geral: L %.3f R %.3f\n", L[7], R[7]);
    AudioComponentInstanceDispose(mx);

    /* grafo: seno mono de 24 kHz no barramento 0 do mixer (48 kHz) -> saída padrão */
    void *g = 0;
    int nout = 0, nmix = 0, nbad = 0;
    CHECK(NewAUGraph(&g) == 0, "NewAUGraph");
    AUGraphAddNode(g, &dout, &nout);
    AUGraphAddNode(g, &dm, &nmix);
    CHECK(AUGraphConnectNodeInput(g, nmix, 0, nout, 0) == 0, "ConnectNodeInput");
    CHECK(AUGraphOpen(g) == 0, "AUGraphOpen");
    void *umix = 0, *uout = 0;
    AUGraphNodeInfo(g, nmix, 0, &umix);
    AUGraphNodeInfo(g, nout, 0, &uout);
    CHECK(umix && uout, "NodeInfo sem unidade");
    CHECK(AudioUnitSetProperty(umix, 8, 1, 0, &lo, 40) == 0 && AudioUnitSetProperty(umix, 8, 2, 0, &out, 40) == 0, "formatos do mixer");
    sine_t sn = {24000, 0, 0};
    rcs_t cs = {sine, &sn};
    CHECK(AUGraphSetNodeInputCallback(g, nmix, 0, &cs) == 0, "SetNodeInputCallback");
    CHECK(AUGraphInitialize(g) == 0, "AUGraphInitialize");
    asbd_t got;
    sz = 40;
    AudioUnitGetProperty(uout, 8, 1, 0, &got, &sz);
    CHECK(got.mSampleRate == 48000 && got.mChannelsPerFrame == 2 && (got.mFormatFlags & 32), "a conexão não levou o formato do mixer à saída");
    rcs_t rend;
    sz = 16;
    AudioUnitGetProperty(uout, 23, 1, 0, &rend, &sz);
    CHECK(rend.fn != 0, "a saída não ficou ligada ao mixer");
    static float acc[48000];
    long nacc = 0;
    for (int blk = 0; blk < 93 && rend.fn; blk++) {   /* 93 x 512 quadros ≈ 1 s */
        ol = outlist(512);
        fl = 0;
        if (rend.fn(rend.ref, &fl, &ts, 0, 512, &ol)) { CHECK(0, "render do grafo"); break; }
        for (int i = 0; i < 512 && nacc < 48000; i++) acc[nacc++] = L[i];
    }
    /* compara com o seno ideal a 48 kHz: a interpolação linear 24 -> 48 kHz é exata nos pontos pares */
    double dot = 0, ea = 0, eb = 0;
    for (long i = 200; i < nacc; i++) {
        double ref = 0.5 * sin(2 * M_PI * 440 * (double)i / 48000);
        dot += acc[i] * ref; ea += acc[i] * acc[i]; eb += ref * ref;
    }
    double corr = dot / sqrt(ea * eb);
    CHECK(corr > 0.9999, "seno reamostrado corr %.6f", corr);
    CHECK(labs(sn.pulled - nacc / 2) < 8, "quadros pedidos à fonte %ld (esperado ~%ld)", sn.pulled, nacc / 2);
    printf("augraph ok grafo mixer->saída, 24->48 kHz: corr %.6f, %ld quadros pedidos para %ld\n", corr, sn.pulled, nacc);
    /* com o grafo inicializado, a desconexão só vale depois do AUGraphUpdate */
    AUGraphDisconnectNodeInput(g, nout, 0);
    sz = 16;
    AudioUnitGetProperty(uout, 23, 1, 0, &rend, &sz);
    CHECK(rend.fn != 0, "desconexão aplicada antes do AUGraphUpdate");
    unsigned char up = 0;
    AUGraphUpdate(g, &up);
    sz = 16;
    AudioUnitGetProperty(uout, 23, 1, 0, &rend, &sz);
    CHECK(rend.fn == 0 && up, "AUGraphUpdate não desligou");
    printf("augraph ok AUGraphUpdate aplica a desconexão pendente\n");
    CHECK(MusicDeviceMIDIEvent(umix, 0x90, 60, 100, 0) == (OSStatus)0x80000002, "MusicDeviceMIDIEvent num mixer");
    acdesc_t dls = {FCC('a', 'u', 'm', 'u'), FCC('d', 'l', 's', ' '), FCC('a', 'p', 'p', 'l'), 0, 0};
    CHECK(AUGraphAddNode(g, &dls, &nbad) == -2005, "nó de componente inexistente num grafo aberto");
    unsigned cnt = 0;
    AUGraphGetNodeCount(g, &cnt);
    CHECK(cnt == 2, "contagem de nós %u", cnt);
    CHECK(DisposeAUGraph(g) == 0, "DisposeAUGraph");
    void *g2 = 0;
    NewAUGraph(&g2);
    AUGraphAddNode(g2, &dls, &nbad);
    CHECK(AUGraphOpen(g2) == -2005, "AUGraphOpen com componente inexistente");
    DisposeAUGraph(g2);
    printf("augraph ok erros: sintetizador ausente -2005, MusicDeviceMIDIEvent badComponentSelector\n");
    printf("%s\n", fails ? "augraph FALHA" : "augraph: tudo ok");
    return fails != 0;
}
