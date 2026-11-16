/* Opus no mOS: libopus 1.5.2 (BSD-3, userland/third_party/opus) atrás de uma casca própria.
 * - cabeçalho: OpusHead (Ogg, RFC 7845, little-endian) e dOps (MP4, big-endian)
 * - decodificação multistream a 48 kHz, PCM 16 bits intercalado
 * - Ogg Opus inteiro: pre-skip no começo e corte no fim pelo granulepos da última página
 * Teste de host: tools/midia_audio_test.sh (compara com o ffmpeg). */
#include "opus_glue.h"
#include "../third_party/opus/include/opus_multistream.h"
#include <stdlib.h>
#include <string.h>

static uint32_t rl16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t rl32(const uint8_t *p) { return rl16(p) | rl16(p + 2) << 16; }
static uint64_t rl64(const uint8_t *p) { return (uint64_t)rl32(p) | (uint64_t)rl32(p + 4) << 32; }
static uint32_t rb16(const uint8_t *p) { return (uint32_t)p[0] << 8 | p[1]; }
static uint32_t rb32(const uint8_t *p) { return rb16(p) << 16 | rb16(p + 2); }

static int head_mapping(cx_opus_head *h, const uint8_t *m, int n) {
    if (h->family == 0) {
        if (h->channels < 1 || h->channels > 2) return -1;
        h->streams = 1;
        h->coupled = h->channels == 2;
        h->map[0] = 0;
        h->map[1] = 1;
        return 0;
    }
    if (n < 2 + h->channels) return -1;
    h->streams = m[0];
    h->coupled = m[1];
    if (h->streams < 1 || h->coupled > h->streams || h->streams + h->coupled > 255) return -1;
    memcpy(h->map, m + 2, (size_t)h->channels);
    return 0;
}

int cx_opus_head_ogg(const uint8_t *p, int n, cx_opus_head *h) {
    memset(h, 0, sizeof *h);
    if (n < 19 || memcmp(p, "OpusHead", 8) || (p[8] & 0xF0)) return -1;
    h->channels = p[9];
    h->preskip = (int)rl16(p + 10);
    h->in_rate = rl32(p + 12);
    h->gain_q8 = (int16_t)rl16(p + 16);
    h->family = p[18];
    if (h->channels < 1) return -1;
    return head_mapping(h, p + 19, n - 19);
}

int cx_opus_head_dops(const uint8_t *p, int n, cx_opus_head *h) {
    memset(h, 0, sizeof *h);
    if (n < 11 || p[0] != 0) return -1;
    h->channels = p[1];
    h->preskip = (int)rb16(p + 2);
    h->in_rate = rb32(p + 4);
    h->gain_q8 = (int16_t)rb16(p + 8);
    h->family = p[10];
    if (h->channels < 1) return -1;
    return head_mapping(h, p + 11, n - 11);
}

void *cx_opus_open(const cx_opus_head *h) {
    int err = 0;
    OpusMSDecoder *d = opus_multistream_decoder_create(48000, h->channels, h->streams, h->coupled, h->map, &err);
    if (!d || err != OPUS_OK) return 0;
    if (h->gain_q8) opus_multistream_decoder_ctl(d, OPUS_SET_GAIN(h->gain_q8));
    return d;
}

int cx_opus_decode(void *d, const uint8_t *pkt, int n, int16_t *pcm, int max_frames) {
    if (!d) return -1;
    return opus_multistream_decode((OpusMSDecoder *)d, pkt, n, pcm, max_frames, 0);
}

void cx_opus_close(void *d) { if (d) opus_multistream_decoder_destroy((OpusMSDecoder *)d); }

int cx_opus_packet_frames(const uint8_t *pkt, int n) {
    return opus_packet_get_nb_samples(pkt, n, 48000);
}

/* ---- Ogg (RFC 3533): só o primeiro fluxo lógico que é Opus ---- */
typedef struct {
    const uint8_t *b;
    uint64_t n, at;
    uint32_t serial;
    int have_serial;
    uint8_t *pkt;
    int pkt_n, pkt_cap;
    int64_t last_granule;
} ogg_rd;

/* Junta segmentos até um pacote completo. 1 = pacote em r->pkt, 0 = fim, -1 = erro. */
static int ogg_next(ogg_rd *r, const uint8_t **seg_tab, int *nseg, int *seg_i, const uint8_t **data) {
    r->pkt_n = 0;
    for (;;) {
        if (*seg_tab && *seg_i < *nseg) {
            int len = (*seg_tab)[*seg_i];
            (*seg_i)++;
            if (r->pkt_n + len > r->pkt_cap) {
                int nc = (r->pkt_n + len) * 2 + 256;
                uint8_t *np = realloc(r->pkt, (size_t)nc);
                if (!np) return -1;
                r->pkt = np;
                r->pkt_cap = nc;
            }
            memcpy(r->pkt + r->pkt_n, *data, (size_t)len);
            r->pkt_n += len;
            *data += len;
            if (len < 255) return 1;
            continue;
        }
        /* próxima página do fluxo */
        for (;;) {
            if (r->at + 27 > r->n) return r->pkt_n ? 1 : 0;
            const uint8_t *pg = r->b + r->at;
            if (memcmp(pg, "OggS", 4)) {
                /* ressincroniza */
                r->at++;
                continue;
            }
            int ns = pg[26];
            if (r->at + 27 + (uint64_t)ns > r->n) return r->pkt_n ? 1 : 0;
            uint64_t body = 0;
            for (int i = 0; i < ns; i++) body += pg[27 + i];
            if (r->at + 27 + ns + body > r->n) return r->pkt_n ? 1 : 0;
            uint32_t serial = rl32(pg + 14);
            r->at += 27 + ns + body;
            if (r->have_serial && serial != r->serial) continue;
            if (!r->have_serial) {
                if (ns < 1 || pg[27] < 8 || memcmp(pg + 27 + ns, "OpusHead", 8)) continue;
                r->serial = serial;
                r->have_serial = 1;
            }
            int64_t g = (int64_t)rl64(pg + 6);
            if (g != -1) r->last_granule = g;
            *seg_tab = pg + 27;
            *nseg = ns;
            *seg_i = 0;
            *data = pg + 27 + ns;
            break;
        }
    }
}

int cx_opus_decode_ogg(const uint8_t *b, uint64_t n, int16_t **out, int *channels, int *frames) {
    *out = 0;
    *channels = 0;
    *frames = 0;
    ogg_rd r;
    memset(&r, 0, sizeof r);
    r.b = b;
    r.n = n;
    r.last_granule = -1;
    const uint8_t *seg = 0, *data = 0;
    int nseg = 0, si = 0;
    if (ogg_next(&r, &seg, &nseg, &si, &data) != 1) { free(r.pkt); return -1; }
    cx_opus_head h;
    if (cx_opus_head_ogg(r.pkt, r.pkt_n, &h)) { free(r.pkt); return -2; }
    if (ogg_next(&r, &seg, &nseg, &si, &data) != 1 || r.pkt_n < 8 || memcmp(r.pkt, "OpusTags", 8)) { free(r.pkt); return -2; }
    void *d = cx_opus_open(&h);
    if (!d) { free(r.pkt); return -3; }
    int ch = h.channels;
    size_t cap = 48000 * 4, len = 0; /* em quadros */
    int16_t *pcm = malloc(cap * (size_t)ch * sizeof *pcm);
    int rc = 0;
    if (!pcm) rc = -3;
    while (!rc) {
        int k = ogg_next(&r, &seg, &nseg, &si, &data);
        if (k < 0) { rc = -3; break; }
        if (k == 0) break;
        if (len + 5760 > cap) {
            size_t nc = cap * 2;
            int16_t *np = realloc(pcm, nc * (size_t)ch * sizeof *np);
            if (!np) { rc = -3; break; }
            pcm = np;
            cap = nc;
        }
        int got = cx_opus_decode(d, r.pkt, r.pkt_n, pcm + len * (size_t)ch, 5760);
        if (got < 0) continue; /* pacote corrompido: o resto segue */
        len += (size_t)got;
    }
    cx_opus_close(d);
    free(r.pkt);
    if (rc) { free(pcm); return rc; }
    /* granulepos final conta quadros a 48 kHz desde o início, pre-skip incluído */
    size_t total = len;
    if (r.last_granule >= 0 && (uint64_t)r.last_granule < total) total = (size_t)r.last_granule;
    size_t skip = (size_t)h.preskip;
    if (skip > total) skip = total;
    size_t keep = total - skip;
    memmove(pcm, pcm + skip * (size_t)ch, keep * (size_t)ch * sizeof *pcm);
    *out = pcm;
    *channels = ch;
    *frames = (int)keep;
    return 0;
}

/* ---- codificador */
#include "../third_party/opus/include/opus.h"
void *cx_opusenc_open(int rate, int channels, int bitrate, int voip, int *frame, int *preskip) {
    int err = 0;
    if (channels < 1 || channels > 2) return 0;
    OpusEncoder *e = opus_encoder_create(rate, channels, voip ? OPUS_APPLICATION_VOIP : OPUS_APPLICATION_AUDIO, &err);
    if (!e || err != OPUS_OK) return 0;
    if (bitrate > 0) opus_encoder_ctl(e, OPUS_SET_BITRATE(bitrate));
    opus_int32 la = 0;
    opus_encoder_ctl(e, OPUS_GET_LOOKAHEAD(&la));
    if (frame) *frame = rate / 50;
    if (preskip) *preskip = (int)((long long)la * 48000 / rate);
    return e;
}
int cx_opusenc_encode(void *e, const int16_t *pcm, int frames, uint8_t *out, int cap) {
    return opus_encode((OpusEncoder *)e, pcm, frames, out, cap);
}
void cx_opusenc_close(void *e) { if (e) opus_encoder_destroy((OpusEncoder *)e); }
int cx_opusenc_head(int channels, int preskip, int in_rate, uint8_t o[19]) {
    memcpy(o, "OpusHead", 8);
    o[8] = 1;
    o[9] = (uint8_t)channels;
    o[10] = (uint8_t)preskip; o[11] = (uint8_t)(preskip >> 8);
    o[12] = (uint8_t)in_rate; o[13] = (uint8_t)(in_rate >> 8); o[14] = (uint8_t)(in_rate >> 16); o[15] = (uint8_t)(in_rate >> 24);
    o[16] = 0; o[17] = 0;   /* ganho */
    o[18] = 0;              /* família de mapeamento 0 (mono/estéreo) */
    return 19;
}
