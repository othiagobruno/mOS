/*
 * Decodificadores de áudio do mOS e o contrato de permissão com a frente de TCC.
 *
 * Funciona (host, comparado com o ffmpeg):
 *   - MP3: minimp3 (CC0), userland/third_party/minimp3/minimp3.h
 *   - ALAC de 16 bits em CAF e em M4A/MP4: decodificador de referência da Apple
 *     (Apache-2.0), userland/third_party/alac/, casca em alac_dec.c
 * Limitação (reconhecido, devolve erro, não produz PCM):
 *   - AAC (ADTS e 'mp4a'): o decodificador compatível seria o fdk-aac (BSD). O FAAD2
 *     é GPL e não entra neste repositório MIT. Sem o fdk-aac, o arquivo continua
 *     kAudioFileUnsupportedDataFormatError.
 *   - Opus (Ogg ou CAF 'opus'): o libopus (BSD-3) não foi vendido nesta entrega.
 *     Não há decodificação.
 * Não contorna FairPlay nem qualquer outro DRM.
 *
 * Permissão de microfone/câmera: não implementa TCC. Consulta o símbolo fraco
 *   int cx_tcc_request(const char *service);
 * da frente de libSystem. 1 = concedido, 0 = negado, e o símbolo ausente (ponteiro
 * nulo) vale -1 ("a frente ainda não está ligada"). Serviços:
 *   kTCCServiceMicrophone, kTCCServiceCamera, kTCCServiceScreenCapture.
 * Sem concessão, captura devolve erro de permissão. Não há captura silenciosa.
 */
#include "sys.h"
#include "libc.h"
#include <stdint.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "../third_party/minimp3/minimp3.h"
#include "../third_party/alac/alac_dec.c"

extern int cx_tcc_request(const char *service) __attribute__((weak));

int cx_media_tcc(const char *service) {
    if (!cx_tcc_request) return -1;
    return cx_tcc_request(service);
}

int cx_pcm_s16_resample(const short *src, int nsrc, int src_rate, short *dst, int ndst, int dst_rate, int ch) {
    if (!src || !dst || nsrc < 1 || ndst < 1 || src_rate < 1 || dst_rate < 1) return 0;
    if (ch < 1) ch = 1;
    if (ch > 2) ch = 2;
    if (src_rate == dst_rate) {
        int n = nsrc < ndst ? nsrc : ndst;
        if (ch == 2) { memcpy(dst, src, (size_t)n * 4); return n; }
        for (int i = 0; i < n; i++) dst[i] = (short)(((int)src[i * 2] + src[i * 2 + 1]) / 2);
        return n;
    }
    int written = 0;
    for (int i = 0; i < ndst; i++) {
        double pos = (double)i * (double)src_rate / (double)dst_rate;
        int i0 = (int)pos;
        if (i0 >= nsrc) break;
        int i1 = i0 + 1 < nsrc ? i0 + 1 : i0;
        double frac = pos - (double)i0;
        for (int c = 0; c < ch; c++) {
            int s0 = ch == 1 ? ((int)src[i0 * 2] + src[i0 * 2 + 1]) / 2 : src[i0 * 2 + c];
            int s1 = ch == 1 ? ((int)src[i1 * 2] + src[i1 * 2 + 1]) / 2 : src[i1 * 2 + c];
            int v = (int)((double)s0 + ((double)s1 - (double)s0) * frac);
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            dst[i * ch + c] = (short)v;
        }
        written++;
    }
    return written;
}

typedef struct {
    int kind;                 /* 1 mp3, 2 alac, 3 aac, 4 opus; 0 não é destes */
    int status;               /* 0 decodificado, 1 reconhecido sem decodificador, -1 erro */
    const char *limit;
    int rate, channels, frames;
    short *pcm;
} cx_audiodec_out;

void cx_audiodec_free(cx_audiodec_out *o) {
    if (!o) return;
    free(o->pcm);
    o->pcm = 0;
}

static int be32(const uint8_t *p) { return (int)((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]); }
static uint64_t be64(const uint8_t *p) { return ((uint64_t)be32(p) << 32) | (uint32_t)be32(p + 4); }

static int looks_opus(const uint8_t *b, uint64_t n) {
    if (n >= 8 && !memcmp(b, "OggS", 4)) {
        uint64_t m = n < 128 ? n : 128;
        for (uint64_t i = 0; i + 8 <= m; i++) if (!memcmp(b + i, "OpusHead", 8)) return 1;
    }
    if (n >= 12 && !memcmp(b, "caff", 4)) {
        uint64_t m = n < 256 ? n : 256;
        for (uint64_t i = 0; i + 4 <= m; i++) if (!memcmp(b + i, "opus", 4)) return 1;
    }
    return 0;
}
static int looks_adts(const uint8_t *b, uint64_t n) {
    if (n < 7) return 0;
    if (b[0] != 0xFF || (b[1] & 0xF0) != 0xF0) return 0;
    int layer = (b[1] >> 1) & 3;
    return layer == 0;
}
static int looks_mp3(const uint8_t *b, uint64_t n) {
    if (n >= 3 && !memcmp(b, "ID3", 3)) return 1;
    if (n >= 2 && b[0] == 0xFF && (b[1] & 0xE0) == 0xE0 && !looks_adts(b, n)) return 1;
    return 0;
}

static int caf_varint(const uint8_t *p, const uint8_t *end, int *used) {
    int v = 0, n = 0;
    while (p + n < end && n < 8) {
        int c = p[n++];
        v = (v << 7) | (c & 0x7f);
        if (!(c & 0x80)) break;
    }
    *used = n;
    return v;
}

static int decode_alac_packets(const uint8_t *cookie, int cookie_len, const uint8_t *audio, int audio_len,
                               const int *sizes, int nsizes, cx_audiodec_out *o) {
    alac_st st;
    int rc = alac_open(&st, cookie, cookie_len);
    if (rc == -2) { o->kind = 2; o->status = 1; o->limit = "ALAC que não é 16 bits não é decodificado"; return 1; }
    if (rc) { o->kind = 2; o->status = -1; o->limit = "cookie ALAC inválido"; return 1; }
    int cap = st.cfg.frameLength * nsizes + 8;
    if (cap < 1 || cap > (1 << 26)) { alac_close(&st); o->status = -1; return 1; }
    short *pcm = calloc((size_t)cap * (size_t)st.cfg.numChannels, sizeof(short));
    if (!pcm) { alac_close(&st); o->status = -1; return 1; }
    int frames = 0;
    int off = 0;
    for (int i = 0; i < nsizes; i++) {
        if (sizes[i] < 1 || off + sizes[i] > audio_len) { free(pcm); alac_close(&st); o->kind = 2; o->status = -1; return 1; }
        int got = 0;
        if (alac_decode(&st, audio + off, sizes[i], pcm + (size_t)frames * st.cfg.numChannels, cap - frames, &got)) {
            free(pcm); alac_close(&st); o->kind = 2; o->status = -1; o->limit = "quadro ALAC não decodificou"; return 1;
        }
        frames += got;
        off += sizes[i];
    }
    o->kind = 2; o->status = 0; o->limit = 0;
    o->rate = (int)st.cfg.sampleRate; o->channels = (int)st.cfg.numChannels; o->frames = frames; o->pcm = pcm;
    alac_close(&st);
    return 1;
}

static int decode_caf_alac(const uint8_t *b, uint64_t n, cx_audiodec_out *o) {
    const uint8_t *cookie = 0; int cookie_len = 0;
    const uint8_t *data = 0; int data_len = 0;
    const uint8_t *pakt = 0; int pakt_len = 0;
    int is_alac = 0, is_opus = 0, is_aac = 0;
    for (uint64_t opos = 8; opos + 12 <= n;) {
        uint64_t cl = be64(b + opos + 4);
        const uint8_t *c = b + opos + 12;
        if (!memcmp(b + opos, "desc", 4) && cl >= 12) {
            if (!memcmp(c + 8, "alac", 4)) is_alac = 1;
            else if (!memcmp(c + 8, "opus", 4)) is_opus = 1;
            else if (!memcmp(c + 8, "aac ", 4) || !memcmp(c + 8, "mp4a", 4)) is_aac = 1;
        } else if (!memcmp(b + opos, "kuki", 4)) { cookie = c; cookie_len = (int)cl; }
        else if (!memcmp(b + opos, "data", 4)) { data = c; data_len = (int)cl; }
        else if (!memcmp(b + opos, "pakt", 4)) { pakt = c; pakt_len = (int)cl; }
        if (cl == ~(uint64_t)0) break;
        if (opos + 12 + cl < opos) break;
        opos += 12 + cl;
    }
    if (is_opus) { o->kind = 4; o->status = 1; o->limit = "Opus reconhecido; libopus não está neste build"; return 1; }
    if (is_aac) { o->kind = 3; o->status = 1; o->limit = "AAC reconhecido; fdk-aac não está neste build"; return 1; }
    if (!is_alac) return 0;
    if (!cookie || !data || !pakt || data_len < 4 || pakt_len < 24) { o->kind = 2; o->status = -1; return 1; }
    int64_t np = (int64_t)be64(pakt);
    if (np < 1 || np > 100000) { o->kind = 2; o->status = -1; return 1; }
    int *sizes = malloc((size_t)np * sizeof(int));
    if (!sizes) { o->status = -1; return 1; }
    const uint8_t *sp = pakt + 24, *se = pakt + pakt_len;
    for (int64_t i = 0; i < np; i++) {
        int used = 0;
        sizes[i] = caf_varint(sp, se, &used);
        sp += used;
    }
    int r = decode_alac_packets(cookie, cookie_len, data + 4, data_len - 4, sizes, (int)np, o);
    free(sizes);
    return r;
}

static int find_cookie(const uint8_t *b, uint64_t n, const uint8_t **cookie, int *cookie_len) {
    const uint8_t mark[8] = {0, 0, 0, 0x24, 'a', 'l', 'a', 'c'};
    for (uint64_t i = 0; i + 36 <= n; i++) {
        if (!memcmp(b + i, mark, 8)) { *cookie = b + i; *cookie_len = 0x24; return 1; }
    }
    return 0;
}
static int atom_is_box(const uint8_t *t) {
    return !memcmp(t, "moov", 4) || !memcmp(t, "trak", 4) || !memcmp(t, "mdia", 4) ||
           !memcmp(t, "minf", 4) || !memcmp(t, "stbl", 4) || !memcmp(t, "edts", 4) ||
           !memcmp(t, "udta", 4) || !memcmp(t, "dinf", 4);
}
/* stsz fica dentro de moov/trak/mdia/minf/stbl, não no nível de ftyp. */
static void walk_mp4(const uint8_t *b, uint64_t begin, uint64_t end, const uint8_t **mdat, int *mdat_len,
                      int **sizes, int *ns, int *sizes_heap, int *saw_mp4a) {
    for (uint64_t i = begin; i + 8 <= end;) {
        uint32_t ln = (uint32_t)be32(b + i);
        uint64_t atom = ln;
        uint64_t hdr = 8;
        if (ln == 1 && i + 16 <= end) { atom = be32(b + i + 8); atom = (atom << 32) | (uint32_t)be32(b + i + 12); hdr = 16; }
        else if (ln == 0) atom = end - i;
        if (atom < hdr || i + atom > end) return;
        const uint8_t *t = b + i + 4;
        if (!memcmp(t, "mdat", 4)) { *mdat = b + i + hdr; *mdat_len = (int)(atom - hdr); }
        if (!memcmp(t, "mp4a", 4)) *saw_mp4a = 1;
        if (!memcmp(t, "stsz", 4) && atom >= 20) {
            int sample_size = be32(b + i + 12);
            int count = be32(b + i + 16);
            if (count > 0 && count < 100000 && sample_size == 0 && 20 + (uint64_t)count * 4 <= atom) {
                if (count > 64) {
                    int *heap = malloc((size_t)count * sizeof(int));
                    if (!heap) return;
                    if (*sizes_heap) free(*sizes);
                    *sizes = heap; *sizes_heap = 1;
                }
                for (int k = 0; k < count; k++) (*sizes)[k] = be32(b + i + 20 + (uint32_t)k * 4);
                *ns = count;
            }
        }
        if (atom_is_box(t)) walk_mp4(b, i + hdr, i + atom, mdat, mdat_len, sizes, ns, sizes_heap, saw_mp4a);
        /* stsd: 8 de cabeçalho do átomo + 4 de versão + 4 de contagem, depois as entradas. */
        if (!memcmp(t, "stsd", 4) && atom >= 16) walk_mp4(b, i + 16, i + atom, mdat, mdat_len, sizes, ns, sizes_heap, saw_mp4a);
        i += atom;
    }
}
static int decode_m4a(const uint8_t *b, uint64_t n, cx_audiodec_out *o) {
    int saw_alac = 0, saw_mp4a = 0;
    const uint8_t *cookie = 0; int cookie_len = 0;
    const uint8_t *mdat = 0; int mdat_len = 0;
    int ns = 0;
    int stack_sizes[64];
    int *sizes = stack_sizes;
    int sizes_heap = 0;
    walk_mp4(b, 0, n, &mdat, &mdat_len, &sizes, &ns, &sizes_heap, &saw_mp4a);
    if (find_cookie(b, n, &cookie, &cookie_len)) saw_alac = 1;
    int r = 0;
    if (saw_alac && cookie && mdat && ns > 0) r = decode_alac_packets(cookie, cookie_len, mdat, mdat_len, sizes, ns, o);
    else if (saw_mp4a && !saw_alac) { o->kind = 3; o->status = 1; o->limit = "AAC em M4A reconhecido; fdk-aac não está neste build"; r = 1; }
    if (sizes_heap) free(sizes);
    return r;
}

static int decode_mp3(const uint8_t *b, uint64_t n, cx_audiodec_out *o) {
    if (n > 0x7fffffff) { o->kind = 1; o->status = -1; return 1; }
    mp3dec_t dec;
    mp3dec_init(&dec);
    int cap = (int)(n / 2 + 1152 * 8);
    if (cap < 1152 * 2) cap = 1152 * 2;
    if (cap > (1 << 26)) cap = 1 << 26;
    short *pcm = malloc((size_t)cap * sizeof(short));
    if (!pcm) { o->status = -1; return 1; }
    int frames = 0, rate = 0, ch = 0;
    int pos = 0;
    while (pos < (int)n) {
        mp3dec_frame_info_t info;
        short tmp[MINIMP3_MAX_SAMPLES_PER_FRAME];
        int samples = mp3dec_decode_frame(&dec, b + pos, (int)n - pos, tmp, &info);
        if (info.frame_bytes <= 0) break;
        pos += info.frame_bytes;
        if (samples <= 0) continue;
        if (!rate) { rate = info.hz; ch = info.channels; }
        int add = samples * (ch ? ch : 1);
        if ((frames + samples) * (ch ? ch : 1) > cap) break;
        memcpy(pcm + (size_t)frames * (size_t)(ch ? ch : 1), tmp, (size_t)add * sizeof(short));
        frames += samples;
    }
    if (!frames || !rate) { free(pcm); o->kind = 1; o->status = -1; o->limit = "MP3 sem quadro decodificável"; return 1; }
    o->kind = 1; o->status = 0; o->limit = 0; o->rate = rate; o->channels = ch; o->frames = frames; o->pcm = pcm;
    return 1;
}

int cx_audiodec(const void *bytes, unsigned long long n, cx_audiodec_out *o) {
    if (!o) return 0;
    memset(o, 0, sizeof *o);
    if (!bytes || n < 4) return 0;
    const uint8_t *b = bytes;
    if (looks_opus(b, n)) { o->kind = 4; o->status = 1; o->limit = "Opus reconhecido; libopus não está neste build"; return 1; }
    if (looks_adts(b, n)) { o->kind = 3; o->status = 1; o->limit = "AAC ADTS reconhecido; fdk-aac não está neste build"; return 1; }
    if (n >= 8 && !memcmp(b, "caff", 4)) return decode_caf_alac(b, n, o);
    if (n >= 12 && !memcmp(b + 4, "ftyp", 4)) return decode_m4a(b, n, o);
    if (looks_mp3(b, n)) return decode_mp3(b, n, o);
    return 0;
}
