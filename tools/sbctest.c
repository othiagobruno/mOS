/* mOS — teste do codificador SBC (kernel/bt/sbc.c): gera 2 s de tons (440 Hz à esquerda,
 * 1 kHz à direita), codifica em cada configuração e grava .sbc + .raw para decodificar com o
 * ffmpeg (decodificador independente). A comparação é feita por tools/sbctest.ps1. */
#include "sbc.h"
#include <math.h>
long long g_live_allocs, g_live_bytes;

static void run(const char *name, int freq, int hz, int mode, int alloc, int bitpool) {
    sbc_enc_t e;
    sbc_init(&e, freq, 16, mode, alloc, bitpool);
    int n = hz * 2, ch = e.channels;
    i16 *pcm = malloc(sizeof(i16) * n * ch);
    for (int i = 0; i < n; i++) {
        pcm[i * ch] = (i16)(12000 * sin(2 * M_PI * 440 * i / hz));
        if (ch == 2) pcm[i * ch + 1] = (i16)(9000 * sin(2 * M_PI * 1000 * i / hz) + 3000 * sin(2 * M_PI * 6000 * i / hz));
    }
    char p[128];
    snprintf(p, sizeof p, "build-ahci/sbc-%s.sbc", name);
    FILE *f = fopen(p, "wb");
    u8 frame[1024];
    int frames = 0;
    for (int i = 0; i + 128 <= n; i += 128, frames++) fwrite(frame, 1, sbc_encode(&e, pcm + i * ch, frame), f);
    fclose(f);
    snprintf(p, sizeof p, "build-ahci/sbc-%s.raw", name);
    f = fopen(p, "wb");
    fwrite(pcm, sizeof(i16), (usize)n * ch, f);
    fclose(f);
    printf("%s: %d quadros de %d bytes (%d kbit/s)\n", name, frames, sbc_frame_len(&e), sbc_frame_len(&e) * 8 * hz / 128 / 1000);
    free(pcm);
}

int main(void) {
    run("joint48-snr53", SBC_FREQ_48K, 48000, SBC_MODE_JOINT, SBC_ALLOC_SNR, 53);
    run("stereo44-loud51", SBC_FREQ_44K1, 44100, SBC_MODE_STEREO, SBC_ALLOC_LOUDNESS, 51);
    run("mono48-loud32", SBC_FREQ_48K, 48000, SBC_MODE_MONO, SBC_ALLOC_LOUDNESS, 32);
    return 0;
}
