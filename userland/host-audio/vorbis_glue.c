/* Vorbis no mOS: casca sobre o stb_vorbis 1.24 (domínio público/MIT, userland/third_party/stb_vorbis).
 * - Ogg Vorbis: o arquivo inteiro pelo stb_vorbis_decode_memory (o granulepos da última página corta o fim).
 * - Matroska/WebM (A_VORBIS): os três cabeçalhos vêm no CodecPrivate em lacing Xiph e os pacotes de áudio nos blocos;
 *   monta um fluxo Ogg em memória (uma página por pacote, CRC do Ogg, continuação para pacotes grandes).
 * Teste de host: tools/vorbis_test.c contra o ffmpeg. */
/* libm que o stb_vorbis usa e o math.h do shim não declara (o libSystem define todas) */
double cos(double), sin(double), exp(double), log(double), pow(double, double), ldexp(double, int), floor(double), fabs(double);
#define alloca __builtin_alloca   /* freestanding: o stb só define alloca no Linux/MinGW */
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "../third_party/stb_vorbis/stb_vorbis.c"
#include "vorbis_glue.h"

__attribute__((visibility("default"))) int cx_vorbis_decode_ogg(const uint8_t *b, uint64_t n, int16_t **out, int *channels, int *rate, int *frames) {
    if (!b || n < 58 || n > 0x7fffffff) return -1;
    short *pcm = 0;
    int ch = 0, r = 0;
    int fr = stb_vorbis_decode_memory(b, (int)n, &ch, &r, &pcm);
    if (fr < 0 || !pcm) return -1;
    *out = pcm; *channels = ch; *rate = r; *frames = fr;
    return 0;
}

/* ---- Ogg em memória */
typedef struct { uint8_t *b; size_t n, cap; uint32_t seq; int err; } ogw_t;
static uint32_t ogg_crc_tab[256];
static void ogg_crc_init(void) {
    if (ogg_crc_tab[1]) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t r = i << 24;
        for (int k = 0; k < 8; k++) r = r & 0x80000000u ? r << 1 ^ 0x04C11DB7u : r << 1;
        ogg_crc_tab[i] = r;
    }
}
static int ogw_need(ogw_t *w, size_t add) {
    if (w->n + add <= w->cap) return 0;
    size_t nc = (w->n + add) * 2;
    uint8_t *x = realloc(w->b, nc);
    if (!x) { w->err = 1; return -1; }
    w->b = x; w->cap = nc;
    return 0;
}
/* um pacote, em quantas páginas forem precisas (255 segmentos por página) */
static void ogw_packet(ogw_t *w, const uint8_t *p, uint32_t len, uint64_t granule, int bos, int eos) {
    uint32_t done = 0;
    int first = 1;
    do {
        uint32_t left = len - done, nseg = 0, take = 0;
        uint8_t seg[255];
        int finished = 0;
        while (nseg < 255) {
            uint32_t s = left - take >= 255 ? 255 : left - take;
            seg[nseg++] = (uint8_t)s;
            take += s;
            if (s < 255) { finished = 1; break; }
        }
        if (ogw_need(w, 27 + nseg + take)) return;
        uint8_t *h = w->b + w->n;
        memcpy(h, "OggS", 4);
        h[4] = 0;
        h[5] = (uint8_t)((first ? 0 : 1) | (bos && first ? 2 : 0) | (eos && finished ? 4 : 0));
        uint64_t g = finished ? granule : ~0ull;   /* -1: nenhum pacote termina nesta página */
        for (int i = 0; i < 8; i++) h[6 + i] = (uint8_t)(g >> (8 * i));
        h[14] = 0x6d; h[15] = 0x6f; h[16] = 0x73; h[17] = 0x01;   /* serial */
        for (int i = 0; i < 4; i++) h[18 + i] = (uint8_t)(w->seq >> (8 * i));
        memset(h + 22, 0, 4);
        h[26] = (uint8_t)nseg;
        memcpy(h + 27, seg, nseg);
        memcpy(h + 27 + nseg, p + done, take);
        uint32_t crc = 0;
        for (uint32_t i = 0; i < 27 + nseg + take; i++) crc = crc << 8 ^ ogg_crc_tab[(crc >> 24) ^ h[i]];
        for (int i = 0; i < 4; i++) h[22 + i] = (uint8_t)(crc >> (8 * i));
        w->n += 27 + nseg + take;
        w->seq++;
        done += take;
        first = 0;
        if (finished) break;
    } while (1);
}

__attribute__((visibility("default"))) int cx_vorbis_decode_packets(const uint8_t *priv, uint32_t priv_n, const uint8_t *const *pkt, const uint32_t *len, uint32_t np,
                             int16_t **out, int *channels, int *rate, int *frames) {
    if (!priv || priv_n < 3 || priv[0] != 2) return -1;
    uint32_t at = 1, hs[3], total = 0;
    for (int i = 0; i < 2; i++) {   /* Xiph: somas de bytes até um < 255 */
        uint32_t s = 0;
        for (;;) { if (at >= priv_n) return -1; uint8_t c = priv[at++]; s += c; if (c != 255) break; }
        hs[i] = s; total += s;
    }
    if (at + total > priv_n) return -1;
    hs[2] = priv_n - at - total;
    const uint8_t *h0 = priv + at;
    if (hs[0] < 30 || h0[0] != 1 || memcmp(h0 + 1, "vorbis", 6)) return -1;
    uint32_t bs1 = 1u << (h0[28] >> 4);   /* blocksize_1: o granulepos estimado nunca fica abaixo do real */
    ogg_crc_init();
    ogw_t w = {0};
    ogw_packet(&w, h0, hs[0], 0, 1, 0);
    ogw_packet(&w, h0 + hs[0], hs[1], 0, 0, 0);
    ogw_packet(&w, h0 + hs[0] + hs[1], hs[2], 0, 0, np == 0);
    uint64_t gran = 0;
    for (uint32_t i = 0; i < np && !w.err; i++) { gran += bs1 / 2; ogw_packet(&w, pkt[i], len[i], gran, 0, i + 1 == np); }
    if (w.err) { free(w.b); return -1; }
    int rc = cx_vorbis_decode_ogg(w.b, w.n, out, channels, rate, frames);
    free(w.b);
    return rc;
}
