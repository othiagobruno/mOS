/* Teste de host dos codificadores de áudio (cx_aacenc_*: FDK libAACenc; cx_opusenc_*: libopus): codifica um chirp
 * estéreo, decodifica pelos decodificadores do mOS (cx_aac_*, cx_opus_*) e compara com a origem alinhando o atraso.
 * Também grava o AAC em ADTS para o script decodificar com o ffmpeg. Rodado por tools/midia_audio_test.sh.
 * uso: audioenc_test aac|he|hev2|opus taxa canais bitrate saida  → "audioenc ok ..." */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aac_glue.h"
#include "opus_glue.h"
int main(int argc, char **argv) {
    const char *kind = argv[1];
    int rate = atoi(argv[2]), ch = atoi(argv[3]), br = atoi(argv[4]);
    int n = rate * 3;
    short *src = malloc((size_t)n * ch * 2);
    for (int i = 0; i < n; i++) for (int c = 0; c < ch; c++) {
        double t = (double)i / rate;
        src[i * ch + c] = (short)(9000 * sin(2 * M_PI * (c ? 3000 - 700 * t : 300 + 600 * t) * t));
    }
    short *dec = calloc((size_t)(n + 48000) * ch * 2, 1);
    long got = 0;
    long bytes = 0;
    int delay = 0;
    FILE *f = fopen(argv[5], "wb");
    uint8_t pkt[8192];
    if (!strcmp(kind, "opus")) {
        int frame, pre;
        void *e = cx_opusenc_open(rate, ch, br, 0, &frame, &pre);
        uint8_t head[19];
        cx_opusenc_head(ch, pre, rate, head);
        cx_opus_head h;
        if (!e || cx_opus_head_ogg(head, 19, &h)) { puts("audioenc FALHA abrir opus"); return 1; }
        void *d = cx_opus_open(&h);
        short *pad = calloc((size_t)frame * ch, 2);
        for (int i = 0; i < n + frame; i += frame) {
            int k = i + frame <= n ? frame : (i < n ? n - i : 0);
            memset(pad, 0, (size_t)frame * ch * 2);
            if (k) memcpy(pad, src + (long)i * ch, (size_t)k * ch * 2);
            int b = cx_opusenc_encode(e, pad, frame, pkt, sizeof pkt);
            if (b < 0) { puts("audioenc FALHA opus"); return 1; }
            bytes += b;
            int fr = cx_opus_decode(d, pkt, b, dec + got * ch, 5760);   /* sempre 48 kHz */
            if (fr > 0) got += fr;
        }
        delay = pre;
        if (rate != 48000) { printf("audioenc: opus %d Hz decodifica a 48 kHz, teste só a 48 kHz\n", rate); return 1; }
    } else {
        int aot = !strcmp(kind, "he") ? 5 : !strcmp(kind, "hev2") ? 29 : 2;
        uint8_t asc[64]; int asc_n = sizeof asc, frame, dl;
        void *e = cx_aacenc_open(rate, ch, br, aot, 0, asc, &asc_n, &frame, &dl);
        void *ea = cx_aacenc_open(rate, ch, br, aot, 1, 0, 0, 0, 0);   /* o mesmo em ADTS, para o ffmpeg */
        if (!e || !ea) { puts("audioenc FALHA abrir aac"); return 1; }
        void *d = cx_aac_open(asc, asc_n, CX_AAC_RAW, 0);
        if (!d) { puts("audioenc FALHA ASC"); return 1; }
        for (int i = 0; ; i += frame) {
            int k = i < n ? (i + frame <= n ? frame : n - i) : 0;
            int b = cx_aacenc_encode(e, k ? src + (long)i * ch : 0, k, pkt, sizeof pkt);
            uint8_t pa[8192];
            int ba = cx_aacenc_encode(ea, k ? src + (long)i * ch : 0, k, pa, sizeof pa);
            if (ba > 0) fwrite(pa, 1, ba, f);
            if (b < 0) { puts("audioenc FALHA aac"); return 1; }
            if (b > 0) {
                bytes += b;
                int c2 = 0, r2 = 0;
                int fr = cx_aac_decode(d, pkt, b, dec + got * ch, 8192 * ch, &c2, &r2);
                if (fr > 0) got += fr;
            } else if (!k) break;
            if (i > n + 20 * frame) break;
        }
        delay = dl;
    }
    fclose(f);
    /* alinhamento: o melhor deslocamento perto do atraso declarado */
    int best = -1; double bc = -2;
    for (int off = delay - 3000 > 0 ? delay - 3000 : 0; off < delay + 3000 && off < got; off++) {
        double dot = 0, ea = 0, eb = 0;
        for (long i = rate / 10; i < n - rate / 10 && i + off < got; i += 7) { double a = src[i * ch], b = dec[(i + off) * ch]; dot += a * b; ea += a * a; eb += b * b; }
        double c = dot / sqrt(ea * eb + 1e-9);
        if (c > bc) { bc = c; best = off; }
    }
    double se = 0, ne = 0;
    for (long i = rate / 10; i < n - rate / 10 && i + best < got; i++) for (int c = 0; c < ch; c++) { double a = src[i * ch + c], b = dec[(i + best) * ch + c]; se += a * a; ne += (a - b) * (a - b); }
    double snr = 10 * log10(se / (ne + 1e-9)), kbps = bytes * 8.0 / 3 / 1000;
    int ok = bc > 0.99 && snr > 12 && (br <= 0 || (kbps > br / 1000.0 * 0.7 && kbps < br / 1000.0 * 1.3));
    printf("%s %s %d Hz %d ch: %.0f kbit/s, atraso %d (declarado %d), correlação %.4f, SNR %.1f dB\n", ok ? "audioenc ok" : "audioenc FALHA", kind, rate, ch, kbps, best, delay, bc, snr);
    return !ok;
}
