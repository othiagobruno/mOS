/*
 * Decodificadores de áudio do mOS e o contrato de permissão com a frente de TCC.
 *
 * Funciona (host, comparado com o ffmpeg):
 *   - MP3: minimp3 (CC0), userland/third_party/minimp3/minimp3.h
 *   - ALAC de 16 bits em CAF e em M4A/MP4: decodificador de referência da Apple
 *     (Apache-2.0), userland/third_party/alac/, casca em alac_dec.c
 *   - AAC (LC, HE v1/v2, LD/ELD, xHE) em ADTS, M4A/MP4/MOV e CAF: FDK AAC 2.0.3, casca aac_glue.cpp
 *   - Opus em Ogg, MP4 ('Opus' + dOps) e CAF: libopus 1.5.2 (BSD-3), casca opus_glue.c
 *   - MP3 dentro de MP4 (esds 0x6B/0x69) pelo minimp3
 * O contêiner MP4/MOV é o demuxer próprio (userland/libsystem/mp4demux.c).
 * Mais de 2 canais: o FDK faz o downmix; no Opus a mistura é feita aqui (ITU-R BS.775).
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

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "../third_party/minimp3/minimp3.h"
#include "../third_party/alac/alac_dec.c"
#include "opus_glue.h"
#include "aac_glue.h"
#include "../libsystem/mp4demux.h"

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

/* Mais de 2 canais vira estéreo (ordem Vorbis/Opus: L C R | L R | L C R Ls Rs LFE...). */
static short *downmix_stereo(short *pcm, int ch, int frames) {
    if (ch <= 2) return pcm;
    short *o = malloc((size_t)frames * 2 * sizeof *o);
    if (!o) return 0;
    for (int i = 0; i < frames; i++) {
        const short *s = pcm + (size_t)i * ch;
        double l = 0, r = 0;
        if (ch == 3) { l = s[0] + 0.707 * s[1]; r = s[2] + 0.707 * s[1]; }
        else if (ch == 4) { l = s[0] + 0.707 * s[2]; r = s[1] + 0.707 * s[3]; }
        else { /* 5.0/5.1/6.1/7.1: L C R Ls Rs [LFE] ... */
            l = s[0] + 0.707 * s[1] + 0.707 * s[3];
            r = s[2] + 0.707 * s[1] + 0.707 * s[4];
            for (int c = 6; c + 1 < ch; c += 2) { l += 0.5 * s[c]; r += 0.5 * s[c + 1]; }
        }
        l *= 0.5; r *= 0.5;
        o[i * 2] = (short)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
        o[i * 2 + 1] = (short)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
    }
    free(pcm);
    return o;
}

static int finish(cx_audiodec_out *o, int kind, short *pcm, int ch, int rate, int frames) {
    o->kind = kind;
    if (!pcm || frames < 1 || ch < 1) { free(pcm); o->status = -1; o->limit = "nenhum quadro decodificável"; return 1; }
    if (ch > 2) {
        pcm = downmix_stereo(pcm, ch, frames);
        ch = 2;
        if (!pcm) { o->status = -1; return 1; }
    }
    o->status = 0; o->limit = 0;
    o->pcm = pcm; o->channels = ch; o->rate = rate; o->frames = frames;
    return 1;
}

/* Buffer de PCM que cresce por pacote. */
typedef struct { short *p; size_t n, cap; int ch; } pcmbuf;
static int pcm_room(pcmbuf *b, size_t frames) {
    if (b->n + frames <= b->cap) return 0;
    size_t nc = b->cap ? b->cap * 2 : 48000;
    while (nc < b->n + frames) nc *= 2;
    short *np = realloc(b->p, nc * (size_t)b->ch * sizeof *np);
    if (!np) return -1;
    b->p = np; b->cap = nc;
    return 0;
}

/* Pacotes de um contêiner (MP4 ou CAF). skip/keep em quadros de saída; keep < 0 = tudo. */
typedef struct {
    const uint8_t *(*get)(void *ctx, int i, int *n);
    void *ctx;
    int count;
} pktsrc;

static void trim(pcmbuf *b, long long skip, long long keep) {
    if (skip < 0) skip = 0;
    if ((size_t)skip > b->n) skip = (long long)b->n;
    size_t left = b->n - (size_t)skip;
    if (keep >= 0 && (size_t)keep < left) left = (size_t)keep;
    memmove(b->p, b->p + (size_t)skip * b->ch, left * (size_t)b->ch * sizeof *b->p);
    b->n = left;
}

/* skip/keep em unidades de ts (timescale do contêiner); ts 0 = já em quadros de saída. */
static int decode_aac_packets(const uint8_t *asc, int asc_n, pktsrc *src, long long skip, long long keep, unsigned ts, cx_audiodec_out *o) {
    void *d = cx_aac_open(asc, asc_n, CX_AAC_RAW, 2);
    if (!d) { o->kind = 3; o->status = -1; o->limit = "AudioSpecificConfig AAC inválido"; return 1; }
    pcmbuf b = {0, 0, 0, 2};
    short *tmp = malloc(2 * 2048 * 2 * sizeof *tmp);
    int rate = 0, ch = 0;
    for (int i = 0; tmp && i < src->count; i++) {
        int n = 0;
        const uint8_t *pk = src->get(src->ctx, i, &n);
        if (!pk) break;
        int c = 0, r = 0;
        int got = cx_aac_decode(d, pk, n, tmp, 2 * 2048 * 2, &c, &r);
        if (got <= 0) continue;
        if (!ch) { ch = c; rate = r; b.ch = ch; }
        if (c != ch) continue;
        if (pcm_room(&b, (size_t)got)) break;
        memcpy(b.p + b.n * (size_t)ch, tmp, (size_t)got * (size_t)ch * sizeof *tmp);
        b.n += (size_t)got;
    }
    free(tmp);
    cx_aac_close(d);
    /* priming e duração vêm na taxa do contêiner; o SBR dobra a taxa de saída */
    if (ts && rate && (unsigned)rate != ts) {
        skip = skip * rate / ts;
        if (keep >= 0) keep = keep * rate / ts;
    }
    trim(&b, skip, keep);
    return finish(o, 3, b.p, ch, rate, (int)b.n);
}

static int decode_opus_packets(const cx_opus_head *h, pktsrc *src, long long skip, long long keep, cx_audiodec_out *o) {
    void *d = cx_opus_open(h);
    if (!d) { o->kind = 4; o->status = -1; o->limit = "cabeçalho Opus inválido"; return 1; }
    pcmbuf b = {0, 0, 0, h->channels};
    for (int i = 0; i < src->count; i++) {
        int n = 0;
        const uint8_t *pk = src->get(src->ctx, i, &n);
        if (!pk) break;
        if (pcm_room(&b, 5760)) break;
        int got = cx_opus_decode(d, pk, n, b.p + b.n * (size_t)b.ch, 5760);
        if (got > 0) b.n += (size_t)got;
    }
    cx_opus_close(d);
    trim(&b, skip, keep);
    return finish(o, 4, b.p, h->channels, 48000, (int)b.n);
}

static int decode_mp3_packets(pktsrc *src, long long skip, long long keep, unsigned ts, cx_audiodec_out *o) {
    mp3dec_t dec;
    mp3dec_init(&dec);
    pcmbuf b = {0, 0, 0, 0};
    int rate = 0;
    short tmp[MINIMP3_MAX_SAMPLES_PER_FRAME];
    for (int i = 0; i < src->count; i++) {
        int n = 0;
        const uint8_t *pk = src->get(src->ctx, i, &n);
        if (!pk) break;
        mp3dec_frame_info_t info;
        int got = mp3dec_decode_frame(&dec, pk, n, tmp, &info);
        if (got <= 0) continue;
        if (!b.ch) { b.ch = info.channels; rate = info.hz; }
        if (info.channels != b.ch || pcm_room(&b, (size_t)got)) continue;
        memcpy(b.p + b.n * (size_t)b.ch, tmp, (size_t)got * (size_t)b.ch * sizeof *tmp);
        b.n += (size_t)got;
    }
    if (ts && rate && (unsigned)rate != ts) { skip = skip * rate / ts; if (keep >= 0) keep = keep * rate / ts; }
    trim(&b, skip, keep);
    return finish(o, 1, b.p, b.ch, rate, (int)b.n);
}

/* ---- CAF ---- */
typedef struct { const uint8_t *data; int data_len; int *off, *len; } cafpk;
static const uint8_t *caf_get(void *ctx, int i, int *n) {
    cafpk *c = ctx;
    if (c->off[i] < 0 || c->off[i] + c->len[i] > c->data_len) return 0;
    *n = c->len[i];
    return c->data + c->off[i];
}

/* ASC dentro do esds (cookie 'kuki' do CAF começa no ES_Descriptor, tag 3). */
static int esds_asc(const uint8_t *p, int n, const uint8_t **asc, int *asc_n) {
    const uint8_t *e = p + n;
    while (p < e) {
        int tag = *p++;
        unsigned len = 0;
        for (int k = 0; k < 4 && p < e; k++) { uint8_t c = *p++; len = len << 7 | (c & 0x7F); if (!(c & 0x80)) break; }
        if (len > (unsigned)(e - p)) len = (unsigned)(e - p);
        if (tag == 3) {
            if (len < 3) return -1;
            uint8_t fl = p[2];
            const uint8_t *q = p + 3;
            if (fl & 0x80) q += 2;
            if (fl & 0x40) q += 1 + *q;
            if (fl & 0x20) q += 2;
            e = p + len;
            p = q;
            continue;
        }
        if (tag == 4) { if (len < 13) return -1; e = p + len; p += 13; continue; }
        if (tag == 5) { *asc = p; *asc_n = (int)len; return 0; }
        p += len;
    }
    return -1;
}

static int decode_caf(const uint8_t *b, uint64_t n, cx_audiodec_out *o) {
    const uint8_t *cookie = 0; int cookie_len = 0;
    const uint8_t *data = 0; int data_len = 0;
    const uint8_t *pakt = 0; int pakt_len = 0;
    uint32_t fmt = 0, fpp = 0, ch = 0;
    double rate = 0;
    for (uint64_t opos = 8; opos + 12 <= n;) {
        uint64_t cl = be64(b + opos + 4);
        const uint8_t *c = b + opos + 12;
        if (cl == ~(uint64_t)0) cl = n - opos - 12; /* data de tamanho aberto */
        if (cl > n - opos - 12) cl = n - opos - 12;
        if (!memcmp(b + opos, "desc", 4) && cl >= 32) {
            uint64_t rb = be64(c);
            memcpy(&rate, &rb, 8);
            fmt = (uint32_t)be32(c + 8);
            fpp = (uint32_t)be32(c + 20);
            ch = (uint32_t)be32(c + 24);
        } else if (!memcmp(b + opos, "kuki", 4)) { cookie = c; cookie_len = (int)cl; }
        else if (!memcmp(b + opos, "data", 4)) { data = c; data_len = (int)cl; }
        else if (!memcmp(b + opos, "pakt", 4)) { pakt = c; pakt_len = (int)cl; }
        opos += 12 + cl;
    }
    int kind = fmt == MP4_FOURCC('a', 'l', 'a', 'c') ? 2 : fmt == MP4_FOURCC('o', 'p', 'u', 's') ? 4 :
               (fmt == MP4_FOURCC('a', 'a', 'c', ' ') || fmt == MP4_FOURCC('a', 'a', 'c', 'h') ||
                fmt == MP4_FOURCC('a', 'a', 'c', 'p') || fmt == MP4_FOURCC('a', 'a', 'c', 'l') ||
                fmt == MP4_FOURCC('a', 'a', 'c', 'e')) ? 3 : 0;
    if (!kind) return 0;
    if (!data || data_len < 4 || !pakt || pakt_len < 24) { o->kind = kind; o->status = -1; o->limit = "CAF sem dados ou sem tabela de pacotes"; return 1; }
    int64_t np = (int64_t)be64(pakt);
    int64_t valid = (int64_t)be64(pakt + 8);
    int priming = be32(pakt + 16);
    if (np < 1 || np > 10000000) { o->kind = kind; o->status = -1; return 1; }
    int *sizes = malloc((size_t)np * sizeof(int)), *offs = malloc((size_t)np * sizeof(int));
    if (!sizes || !offs) { free(sizes); free(offs); o->status = -1; return 1; }
    const uint8_t *sp = pakt + 24, *se = pakt + pakt_len;
    int off = 0;
    for (int64_t i = 0; i < np; i++) {
        int used = 0;
        sizes[i] = caf_varint(sp, se, &used);
        if (!fpp && used) { int u2 = 0; caf_varint(sp + used, se, &u2); used += u2; } /* quadros por pacote variáveis */
        sp += used;
        offs[i] = off;
        off += sizes[i];
    }
    int r = 1;
    if (kind == 2) {
        r = decode_alac_packets(cookie, cookie_len, data + 4, data_len - 4, sizes, (int)np, o);
    } else {
        cafpk cp = {data + 4, data_len - 4, offs, sizes};
        pktsrc src = {caf_get, &cp, (int)np};
        if (kind == 3) {
            const uint8_t *asc = 0; int asc_n = 0;
            if (!cookie || esds_asc(cookie, cookie_len, &asc, &asc_n)) { o->kind = 3; o->status = -1; o->limit = "CAF AAC sem cookie esds"; }
            else r = decode_aac_packets(asc, asc_n, &src, priming, valid > 0 ? valid : -1, rate > 0 ? (unsigned)(rate + 0.5) : 0, o);
        } else {
            cx_opus_head h;
            if (!cookie || cx_opus_head_ogg(cookie, cookie_len, &h)) {
                /* sem OpusHead no cookie: mono/estéreo simples */
                memset(&h, 0, sizeof h);
                h.channels = (int)ch; h.streams = 1; h.coupled = ch == 2; h.map[1] = 1;
            }
            long long sk = priming ? priming : h.preskip;
            r = decode_opus_packets(&h, &src, sk, valid > 0 ? valid : -1, o);
        }
    }
    (void)rate;
    free(sizes); free(offs);
    return r;
}

/* ---- MP4 / M4A / MOV ---- */
typedef struct { mp4_file *f; mp4_track *t; uint8_t *buf; uint32_t cap; } mp4pk;
static const uint8_t *mp4_get(void *ctx, int i, int *n) {
    mp4pk *m = ctx;
    uint32_t sz = m->t->s[i].size;
    if (sz > m->cap) {
        uint8_t *nb = realloc(m->buf, sz);
        if (!nb) return 0;
        m->buf = nb; m->cap = sz;
    }
    if (mp4_read_sample(m->f, m->t, (uint32_t)i, m->buf)) return 0;
    *n = (int)sz;
    return m->buf;
}

static int decode_m4a(const uint8_t *b, uint64_t n, cx_audiodec_out *o) {
    mp4_file f;
    if (mp4_open_mem(&f, b, n)) return 0;
    int ti = mp4_find_track(&f, MP4_FOURCC('s', 'o', 'u', 'n'));
    if (ti < 0) { mp4_close(&f); return 0; }
    mp4_track *t = &f.t[ti];
    int r = 0;
    if (t->encrypted) {
        o->kind = t->codec == MP4_FOURCC('m', 'p', '4', 'a') ? 3 : 0;
        o->status = 1; o->limit = "faixa protegida (DRM); o mOS não descriptografa";
        mp4_close(&f);
        return 1;
    }
    mp4pk mp = {&f, t, 0, 0};
    pktsrc src = {mp4_get, &mp, (int)t->nsamples};
    /* edit list: começo (priming) e duração da apresentação, em timescale da faixa */
    long long skip = t->edit_media_time > 0 ? t->edit_media_time : 0;
    long long keep = -1;
    if (t->edit_media_time >= 0) {
        long long total = 0;
        for (uint32_t i = 0; i < t->nsamples; i++) total += t->s[i].dur;
        if (t->duration && (long long)t->duration < total) total = (long long)t->duration;
        keep = total - skip;
    }
    if (t->codec == MP4_FOURCC('a', 'l', 'a', 'c') && t->cfg && t->cfg_len >= 28) {
        int *sizes = malloc(t->nsamples * sizeof(int));
        uint8_t *all = 0;
        uint64_t tot = 0;
        for (uint32_t i = 0; sizes && i < t->nsamples; i++) { sizes[i] = (int)t->s[i].size; tot += t->s[i].size; }
        all = sizes && tot < (1u << 30) ? malloc(tot ? tot : 1) : 0;
        uint64_t at = 0;
        for (uint32_t i = 0; all && i < t->nsamples; i++) { if (mp4_read_sample(&f, t, i, all + at)) { free(all); all = 0; break; } at += t->s[i].size; }
        if (all) r = decode_alac_packets(t->cfg + 4, (int)t->cfg_len - 4, all, (int)tot, sizes, (int)t->nsamples, o);
        else { o->kind = 2; o->status = -1; r = 1; }
        free(all); free(sizes);
    } else if (t->codec == MP4_FOURCC('m', 'p', '4', 'a') && (t->oti == 0x6B || t->oti == 0x69)) {
        r = decode_mp3_packets(&src, skip, keep, t->timescale, o);
    } else if (t->codec == MP4_FOURCC('m', 'p', '4', 'a') || t->codec == MP4_FOURCC('a', 'a', 'c', ' ')) {
        if (!t->cfg || t->cfg_type != MP4_FOURCC('e', 's', 'd', 's')) { o->kind = 3; o->status = -1; o->limit = "AAC sem AudioSpecificConfig"; r = 1; }
        else {
            r = decode_aac_packets(t->cfg, (int)t->cfg_len, &src, skip, keep, t->timescale, o);
        }
    } else if (t->codec == MP4_FOURCC('O', 'p', 'u', 's')) {
        cx_opus_head h;
        if (!t->cfg || t->cfg_type != MP4_FOURCC('d', 'O', 'p', 's') || cx_opus_head_dops(t->cfg, (int)t->cfg_len, &h)) {
            o->kind = 4; o->status = -1; o->limit = "Opus sem dOps"; r = 1;
        } else {
            /* no MP4 o pre-skip vem pela edit list (ou pelo dOps quando não há elst) */
            unsigned ts = t->timescale ? t->timescale : 48000;
            long long sk = t->edit_media_time >= 0 ? skip * 48000 / ts : h.preskip;
            r = decode_opus_packets(&h, &src, sk, keep >= 0 ? keep * 48000 / ts : -1, o);
        }
    }
    free(mp.buf);
    mp4_close(&f);
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
    if (n >= 8 && !memcmp(b, "OggS", 4) && looks_opus(b, n)) {
        short *pcm = 0; int ch = 0, frames = 0;
        if (cx_opus_decode_ogg(b, n, &pcm, &ch, &frames)) { o->kind = 4; o->status = -1; o->limit = "Ogg Opus inválido"; return 1; }
        return finish(o, 4, pcm, ch, 48000, frames);
    }
    if (looks_adts(b, n)) {
        short *pcm = 0; int ch = 0, rate = 0, frames = 0;
        if (cx_aac_decode_adts(b, n, 2, &pcm, &ch, &rate, &frames)) { o->kind = 3; o->status = -1; o->limit = "ADTS sem quadro AAC decodificável"; return 1; }
        return finish(o, 3, pcm, ch, rate, frames);
    }
    if (n >= 8 && !memcmp(b, "caff", 4)) return decode_caf(b, n, o);
    if (n >= 12 && (!memcmp(b + 4, "ftyp", 4) || !memcmp(b + 4, "moov", 4) || !memcmp(b + 4, "wide", 4))) return decode_m4a(b, n, o);
    if (looks_mp3(b, n)) return decode_mp3(b, n, o);
    return 0;
}
