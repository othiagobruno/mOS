/* AudioToolbox/AudioFileStream: análise incremental de áudio que chega em pedaços (rede, demuxer próprio do app).
 * Formatos: MP3 (MPEG-1/2/2.5, camadas I-III, ID3v2, quadro Xing/Info), AAC em ADTS (pacotes sem o cabeçalho e o
 * cookie esds montado do cabeçalho, como no macOS), WAV, AIFF/AIFC, CAF (LPCM e comprimido com 'pakt') e MP4/M4A
 * com moov antes do mdat. Com moov depois do mdat o fluxo não é "otimizado" e a análise devolve 'optm', como no macOS.
 * Teste de host: tools/audiofilestream_test.c (pedaços de tamanho aleatório contra o ffmpeg). */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mp4demux.h"

typedef int OSStatus;
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;
typedef struct {
    double mSampleRate;
    u32 mFormatID, mFormatFlags, mBytesPerPacket, mFramesPerPacket, mBytesPerFrame, mChannelsPerFrame, mBitsPerChannel, mReserved;
} asbd_t;
typedef struct { i64 mStartOffset; u32 mVariableFramesInPacket, mDataByteSize; } aspd_t;
typedef struct { asbd_t mASBD; u32 mChannelLayoutTag; } afmtlist_t;
typedef void (*afs_listener)(void *client, void *stream, u32 prop, u32 *flags);
typedef void (*afs_packets)(void *client, u32 nbytes, u32 npackets, const void *data, aspd_t *pd);

#define F4(a, b, c, d) ((u32)(a) << 24 | (u32)(b) << 16 | (u32)(c) << 8 | (u32)(d))
#define ERR_UNSUPPORTED_FILE F4('t', 'y', 'p', '?')
#define ERR_UNSUPPORTED_FORMAT F4('f', 'm', 't', '?')
#define ERR_UNSUPPORTED_PROP F4('p', 't', 'y', '?')
#define ERR_BAD_SIZE F4('!', 's', 'i', 'z')
#define ERR_NOT_OPTIMIZED F4('o', 'p', 't', 'm')
#define ERR_INVALID_PACKET F4('p', 'c', 'k', '?')
#define ERR_INVALID_FILE F4('d', 't', 'a', '?')
#define ERR_VALUE_UNKNOWN F4('u', 'n', 'k', '?')
#define ERR_DATA_NOT_READY F4('d', 't', 'n', 'r')
#define ERR_DISCONTINUITY F4('d', 's', 'c', 'n')
#define ERR_PARAM (-50)
#define FMT_LPCM F4('l', 'p', 'c', 'm')
#define FL_FLOAT 1u
#define FL_BIG 2u
#define FL_SIGNED 4u
#define FL_PACKED 8u

enum { K_NONE, K_MP3, K_ADTS, K_WAV, K_AIFF, K_CAF, K_MP4 };

typedef struct {
    u32 magic;
    void *client;
    afs_listener pl;
    afs_packets pp;
    u32 hint;
    u8 *buf;                     /* bytes ainda não consumidos; buf[0] fica em base no arquivo */
    u64 n, cap, base;
    int kind, ready, failed;
    u32 file_type;
    asbd_t fmt;
    u8 *cookie;
    u32 cookie_n;
    i64 data_off;                /* início dos dados de áudio no arquivo */
    u64 data_bytes;              /* 0 = desconhecido */
    u64 packet_count;            /* 0 = desconhecido */
    u32 max_packet;
    u32 bit_rate;
    u64 at;                      /* próximo byte a analisar (posição no arquivo) */
    u64 data_end;                /* fim dos dados (0 = até o fim do fluxo) */
    int skip_first_mp3;          /* o primeiro quadro MP3 é Xing/Info e não é áudio */
    /* CAF/MP4: tabela de pacotes */
    u64 *poff; u32 *psz; u64 np, pnext;
    i64 priming, remainder;
    mp4_file mf;
    int have_mf;
} afs_t;
#define AFS_MAGIC 0x41465354u

static afs_t *afs_of(void *s) { afs_t *a = s; return a && a->magic == AFS_MAGIC ? a : 0; }
static u32 rb16(const u8 *p) { return (u32)p[0] << 8 | p[1]; }
static u32 rb32(const u8 *p) { return rb16(p) << 16 | rb16(p + 2); }
static u64 rb64(const u8 *p) { return (u64)rb32(p) << 32 | rb32(p + 4); }
static u32 rl16(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8; }
static u32 rl32(const u8 *p) { return rl16(p) | rl16(p + 2) << 16; }

static void notify(afs_t *a, u32 prop) {
    u32 flags = 0;
    if (a->pl) a->pl(a->client, a, prop, &flags);
}

static int keep(afs_t *a, const u8 *p, u32 n) {
    if (a->n + n > a->cap) {
        u64 nc = (a->n + n) * 2 + 4096;
        u8 *nb = realloc(a->buf, nc);
        if (!nb) return -1;
        a->buf = nb;
        a->cap = nc;
    }
    memcpy(a->buf + a->n, p, n);
    a->n += n;
    return 0;
}
/* descarta o que está antes de 'off' no arquivo */
static void drop_before(afs_t *a, u64 off) {
    if (off <= a->base) return;
    u64 k = off - a->base;
    if (k > a->n) k = a->n;
    memmove(a->buf, a->buf + k, a->n - k);
    a->n -= k;
    a->base += k;
}
static const u8 *at_file(afs_t *a, u64 off, u64 need) {
    if (off < a->base || off - a->base + need > a->n) return 0;
    return a->buf + (off - a->base);
}

/* ---------------- MP3 ---------------- */
typedef struct { int ok, ver, layer, rate, ch, size, spf, kbps; } mp3h;
static mp3h mp3_header(const u8 *p) {
    mp3h h = {0};
    if (p[0] != 0xFF || (p[1] & 0xE0) != 0xE0) return h;
    int v = (p[1] >> 3) & 3, l = (p[1] >> 1) & 3, br = p[2] >> 4, sr = (p[2] >> 2) & 3, pad = (p[2] >> 1) & 1;
    if (v == 1 || l == 0 || br == 0 || br == 15 || sr == 3) return h;
    static const int rates[4][3] = {{11025, 12000, 8000}, {0, 0, 0}, {22050, 24000, 16000}, {44100, 48000, 32000}};
    static const int kb1[3][16] = {
        {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
        {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}};
    static const int kb2[3][16] = {
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}};
    int layer = 4 - l; /* 1, 2, 3 */
    int mpeg1 = v == 3;
    h.rate = rates[v][sr];
    h.kbps = mpeg1 ? kb1[layer - 1][br] : kb2[layer - 1][br];
    h.layer = layer;
    h.ver = v;
    h.ch = ((p[3] >> 6) == 3) ? 1 : 2;
    if (layer == 1) { h.spf = 384; h.size = (12 * h.kbps * 1000 / h.rate + pad) * 4; }
    else if (layer == 2 || mpeg1) { h.spf = 1152; h.size = 144 * h.kbps * 1000 / h.rate + pad; }
    else { h.spf = 576; h.size = 72 * h.kbps * 1000 / h.rate + pad; }
    h.ok = h.size >= 21;
    return h;
}
static int mp3_is_xing(const u8 *p, const mp3h *h, int n) {
    int side = h->ver == 3 ? (h->ch == 1 ? 17 : 32) : (h->ch == 1 ? 9 : 17);
    int o = 4 + side;
    if (o + 4 <= n && (!memcmp(p + o, "Xing", 4) || !memcmp(p + o, "Info", 4))) return 1;
    if (36 + 4 <= n && !memcmp(p + 36, "VBRI", 4)) return 1;
    return 0;
}

/* ---------------- ADTS ---------------- */
typedef struct { int ok, hdr, len, aot, sfi, ch; } adtsh;
static adtsh adts_header(const u8 *p) {
    adtsh h = {0};
    if (p[0] != 0xFF || (p[1] & 0xF6) != 0xF0) return h;
    h.hdr = (p[1] & 1) ? 7 : 9;
    h.aot = (p[2] >> 6) + 1;
    h.sfi = (p[2] >> 2) & 15;
    h.ch = ((p[2] & 1) << 2) | (p[3] >> 6);
    h.len = ((p[3] & 3) << 11) | (p[4] << 3) | (p[5] >> 5);
    h.ok = h.sfi < 13 && h.len > h.hdr;
    return h;
}
static const int g_sfi[13] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350};
/* ES_Descriptor (tag 3) com o AudioSpecificConfig: o mesmo cookie que o AudioFile entrega para AAC */
static int build_esds(afs_t *a, const u8 *asc, int asc_n, u32 avg_bitrate) {
    u8 *c = malloc(64);
    if (!c) return -1;
    int n = 0;
    c[n++] = 3; c[n++] = (u8)(3 + 2 + 13 + 2 + asc_n + 3); c[n++] = 0; c[n++] = 0; c[n++] = 0;
    c[n++] = 4; c[n++] = (u8)(13 + 2 + asc_n); c[n++] = 0x40; c[n++] = 0x15; c[n++] = 0; c[n++] = 0; c[n++] = 0;
    for (int i = 0; i < 4; i++) c[n++] = 0;                       /* maxBitrate */
    for (int i = 3; i >= 0; i--) c[n++] = (u8)(avg_bitrate >> (8 * i));
    c[n++] = 5; c[n++] = (u8)asc_n;
    memcpy(c + n, asc, (size_t)asc_n); n += asc_n;
    c[n++] = 6; c[n++] = 1; c[n++] = 2;                          /* SLConfigDescriptor */
    free(a->cookie);
    a->cookie = c;
    a->cookie_n = (u32)n;
    return 0;
}

/* ---------------- publicação das propriedades ---------------- */
static void publish(afs_t *a) {
    notify(a, F4('f', 'f', 'm', 't'));
    notify(a, F4('d', 'f', 'm', 't'));
    if (a->cookie) notify(a, F4('m', 'g', 'i', 'c'));
    if (a->data_bytes) notify(a, F4('b', 'c', 'n', 't'));
    notify(a, F4('d', 'o', 'f', 'f'));
    a->ready = 1;
    notify(a, F4('r', 'e', 'd', 'y'));
}

static double ext80(const u8 *p) {
    int e = ((p[0] & 0x7F) << 8 | p[1]) - 16383 - 63;
    double v = (double)rb64(p + 2);
    while (e > 0) { v *= 2; e--; }
    while (e < 0) { v /= 2; e++; }
    return (p[0] & 0x80) ? -v : v;
}

static void lpcm(asbd_t *f, double rate, u32 ch, u32 bits, int is_float, int big) {
    memset(f, 0, sizeof *f);
    f->mSampleRate = rate;
    f->mFormatID = FMT_LPCM;
    f->mFormatFlags = (is_float ? FL_FLOAT : (bits > 8 ? FL_SIGNED : 0)) | (big ? FL_BIG : 0) | FL_PACKED;
    f->mChannelsPerFrame = ch;
    f->mBitsPerChannel = bits;
    f->mFramesPerPacket = 1;
    f->mBytesPerFrame = f->mBytesPerPacket = ch * ((bits + 7) / 8);
}

/* Cabeçalho: devolve 1 quando o formato está conhecido e os pacotes podem começar, 0 se faltam bytes, <0 erro. */
static int parse_header(afs_t *a) {
    const u8 *b = a->buf;
    u64 n = a->n;
    if (n < 12) return 0;
    if (!memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WAVE", 4)) {
        a->kind = K_WAV; a->file_type = F4('W', 'A', 'V', 'E');
        int have_fmt = 0;
        for (u64 o = 12; o + 8 <= n;) {
            u32 cl = rl32(b + o + 4);
            if (!memcmp(b + o, "fmt ", 4)) {
                if (o + 8 + cl > n) return 0;
                const u8 *c = b + o + 8;
                u32 tag = rl16(c), ch = rl16(c + 2), rate = rl32(c + 4), bits = rl16(c + 14);
                if (tag == 0xFFFE && cl >= 40) tag = rl16(c + 24);
                if (tag != 1 && tag != 3) return -(int)ERR_UNSUPPORTED_FORMAT;
                lpcm(&a->fmt, rate, ch, bits, tag == 3, 0);
                have_fmt = 1;
            } else if (!memcmp(b + o, "data", 4)) {
                if (!have_fmt) return -(int)ERR_INVALID_FILE;
                a->data_off = (i64)(o + 8);
                a->data_bytes = cl && cl != 0xFFFFFFFFu ? cl : 0;
                a->data_end = a->data_bytes ? o + 8 + cl : 0;
                a->at = o + 8;
                return 1;
            }
            o += 8 + cl + (cl & 1);
        }
        return 0;
    }
    if (!memcmp(b, "FORM", 4) && (!memcmp(b + 8, "AIFF", 4) || !memcmp(b + 8, "AIFC", 4))) {
        int aifc = !memcmp(b + 8, "AIFC", 4);
        a->kind = K_AIFF; a->file_type = aifc ? F4('A', 'I', 'F', 'C') : F4('A', 'I', 'F', 'F');
        int have = 0;
        for (u64 o = 12; o + 8 <= n;) {
            u32 cl = rb32(b + o + 4);
            if (!memcmp(b + o, "COMM", 4)) {
                if (o + 8 + cl > n) return 0;
                const u8 *c = b + o + 8;
                u32 ch = rb16(c), bits = rb16(c + 6);
                double rate = ext80(c + 8);
                int fl = 0, big = 1;
                if (aifc && cl >= 22) {
                    u32 comp = rb32(c + 18);
                    if (comp == F4('s', 'o', 'w', 't')) big = 0;
                    else if (comp == F4('f', 'l', '3', '2') || comp == F4('F', 'L', '3', '2')) { fl = 1; bits = 32; }
                    else if (comp == F4('f', 'l', '6', '4') || comp == F4('F', 'L', '6', '4')) { fl = 1; bits = 64; }
                    else if (comp != F4('N', 'O', 'N', 'E')) return -(int)ERR_UNSUPPORTED_FORMAT;
                }
                lpcm(&a->fmt, rate, ch, bits, fl, big);
                have = 1;
            } else if (!memcmp(b + o, "SSND", 4)) {
                if (!have) return -(int)ERR_INVALID_FILE;
                if (o + 16 > n) return 0;
                u32 off = rb32(b + o + 8);
                a->data_off = (i64)(o + 16 + off);
                a->data_bytes = cl >= 8 + off ? cl - 8 - off : 0;
                a->data_end = a->data_bytes ? (u64)a->data_off + a->data_bytes : 0;
                a->at = (u64)a->data_off;
                return 1;
            }
            o += 8 + cl + (cl & 1);
        }
        return 0;
    }
    if (!memcmp(b, "caff", 4)) {
        a->kind = K_CAF; a->file_type = F4('c', 'a', 'f', 'f');
        int have = 0;
        const u8 *pakt = 0; u64 pakt_n = 0;
        for (u64 o = 8; o + 12 <= n;) {
            u64 cl = rb64(b + o + 4);
            const u8 *c = b + o + 12;
            if (!memcmp(b + o, "data", 4)) {
                if (!have) return -(int)ERR_INVALID_FILE;
                a->data_off = (i64)(o + 12 + 4);  /* mEditCount */
                a->data_bytes = cl != ~0ull && cl >= 4 ? cl - 4 : 0;
                a->data_end = a->data_bytes ? (u64)a->data_off + a->data_bytes : 0;
                a->at = (u64)a->data_off;
                if (a->fmt.mFormatID != FMT_LPCM && !a->fmt.mBytesPerPacket) {
                    if (!pakt) return -(int)ERR_INVALID_FILE;   /* VBR sem tabela de pacotes antes dos dados */
                    i64 cnt = (i64)rb64(pakt);
                    if (cnt < 0 || cnt > 100000000) return -(int)ERR_INVALID_FILE;
                    a->poff = malloc((size_t)(cnt ? cnt : 1) * sizeof(u64));
                    a->psz = malloc((size_t)(cnt ? cnt : 1) * sizeof(u32));
                    if (!a->poff || !a->psz) return -(int)ERR_INVALID_FILE;
                    a->priming = (int32_t)rb32(pakt + 16);
                    a->remainder = (int32_t)rb32(pakt + 20);
                    const u8 *p = pakt + 24, *e = pakt + pakt_n;
                    u64 off = (u64)a->data_off;
                    for (i64 i = 0; i < cnt; i++) {
                        u32 v = 0; int k = 0;
                        while (p < e && k < 5) { u8 x = *p++; v = v << 7 | (x & 0x7F); k++; if (!(x & 0x80)) break; }
                        if (!a->fmt.mFramesPerPacket) { while (p < e && (*p++ & 0x80)) {} } /* quadros variáveis por pacote */
                        a->poff[i] = off; a->psz[i] = v; off += v;
                        if (v > a->max_packet) a->max_packet = v;
                    }
                    a->np = (u64)cnt;
                    a->packet_count = (u64)cnt;
                }
                return 1;
            }
            if (cl == ~0ull || o + 12 + cl > n) return 0;
            if (!memcmp(b + o, "desc", 4) && cl >= 32) {
                u64 rr = rb64(c);
                memcpy(&a->fmt.mSampleRate, &rr, 8);
                a->fmt.mFormatID = rb32(c + 8);
                u32 fl = rb32(c + 12);
                a->fmt.mBytesPerPacket = rb32(c + 16);
                a->fmt.mFramesPerPacket = rb32(c + 20);
                a->fmt.mChannelsPerFrame = rb32(c + 24);
                a->fmt.mBitsPerChannel = rb32(c + 28);
                if (a->fmt.mFormatID == FMT_LPCM) {
                    /* no CAF o bit 2 é "little-endian", o inverso do CoreAudio */
                    a->fmt.mFormatFlags = ((fl & 1) ? FL_FLOAT : FL_SIGNED) | ((fl & 2) ? 0 : FL_BIG) | FL_PACKED;
                    a->fmt.mBytesPerFrame = a->fmt.mBytesPerPacket;
                } else a->fmt.mFormatFlags = fl;
                have = 1;
            } else if (!memcmp(b + o, "kuki", 4)) {
                a->cookie = malloc(cl ? cl : 1);
                if (!a->cookie) return -(int)ERR_INVALID_FILE;
                memcpy(a->cookie, c, cl);
                a->cookie_n = (u32)cl;
            } else if (!memcmp(b + o, "pakt", 4)) { pakt = c; pakt_n = cl; }
            o += 12 + cl;
        }
        return 0;
    }
    if (n >= 8 && (!memcmp(b + 4, "ftyp", 4) || !memcmp(b + 4, "moov", 4) || !memcmp(b + 4, "wide", 4) || !memcmp(b + 4, "free", 4))) {
        a->kind = K_MP4; a->file_type = F4('m', 'p', '4', 'f');
        /* espera o moov inteiro; mdat antes dele = arquivo não otimizado para fluxo */
        for (u64 o = 0; o + 8 <= n;) {
            u64 sz = rb32(b + o), hdr = 8;
            if (sz == 1) { if (o + 16 > n) return 0; sz = rb64(b + o + 8); hdr = 16; }
            if (sz < hdr) return -(int)ERR_INVALID_FILE;
            if (!memcmp(b + o + 4, "mdat", 4)) return -(int)ERR_NOT_OPTIMIZED;
            if (!memcmp(b + o + 4, "moov", 4)) {
                if (o + sz > n) return 0;
                if (mp4_open_mem(&a->mf, b, o + sz)) return -(int)ERR_INVALID_FILE;
                a->have_mf = 1;
                int ti = mp4_find_track(&a->mf, MP4_FOURCC('s', 'o', 'u', 'n'));
                if (ti < 0) return -(int)ERR_UNSUPPORTED_FORMAT;
                mp4_track *t = &a->mf.t[ti];
                if (t->encrypted) return -(int)ERR_UNSUPPORTED_FORMAT;
                memset(&a->fmt, 0, sizeof a->fmt);
                a->fmt.mSampleRate = t->sample_rate ? t->sample_rate : t->timescale;
                a->fmt.mChannelsPerFrame = t->channels;
                if (t->codec == MP4_FOURCC('m', 'p', '4', 'a')) {
                    if (t->oti == 0x6B || t->oti == 0x69) { a->fmt.mFormatID = F4('.', 'm', 'p', '3'); a->fmt.mFramesPerPacket = 1152; }
                    else {
                        a->fmt.mFormatID = F4('a', 'a', 'c', ' ');
                        a->fmt.mFramesPerPacket = 1024;
                        if (t->cfg && t->cfg_type == MP4_FOURCC('e', 's', 'd', 's')) {
                            if (t->cfg_len >= 1 && (t->cfg[0] >> 3) == 5) a->fmt.mFormatID = F4('a', 'a', 'c', 'h');
                            build_esds(a, t->cfg, (int)t->cfg_len, 0);
                        }
                    }
                } else if (t->codec == MP4_FOURCC('a', 'l', 'a', 'c')) {
                    a->fmt.mFormatID = F4('a', 'l', 'a', 'c');
                    if (t->cfg && t->cfg_len >= 28) {
                        a->fmt.mFramesPerPacket = rb32(t->cfg + 4);
                        a->cookie = malloc(t->cfg_len - 4);
                        if (a->cookie) { memcpy(a->cookie, t->cfg + 4, t->cfg_len - 4); a->cookie_n = t->cfg_len - 4; }
                    }
                } else if (t->codec == MP4_FOURCC('O', 'p', 'u', 's')) {
                    a->fmt.mFormatID = F4('o', 'p', 'u', 's');
                    a->fmt.mSampleRate = 48000;
                    a->fmt.mFramesPerPacket = 960;
                } else return -(int)ERR_UNSUPPORTED_FORMAT;
                a->np = t->nsamples;
                a->poff = malloc((size_t)(a->np ? a->np : 1) * sizeof(u64));
                a->psz = malloc((size_t)(a->np ? a->np : 1) * sizeof(u32));
                if (!a->poff || !a->psz) return -(int)ERR_INVALID_FILE;
                u64 total = 0;
                for (u64 i = 0; i < a->np; i++) {
                    a->poff[i] = t->s[i].off; a->psz[i] = t->s[i].size; total += t->s[i].size;
                    if (t->s[i].size > a->max_packet) a->max_packet = t->s[i].size;
                }
                a->packet_count = a->np;
                a->data_bytes = total;
                a->data_off = a->np ? (i64)a->poff[0] : (i64)(o + sz);
                a->priming = t->edit_media_time > 0 ? t->edit_media_time : 0;
                a->at = (u64)a->data_off;
                /* o índice aponta para o buffer, que ainda vai crescer: fica só a tabela copiada */
                mp4_close(&a->mf);
                a->have_mf = 0;
                return 1;
            }
            o += sz;
        }
        return 0;
    }
    /* MP3 e ADTS: acha o primeiro quadro (depois de um ID3v2) e confirma pelo quadro seguinte */
    u64 o = 0;
    if (n >= 10 && !memcmp(b, "ID3", 3)) {
        u64 tag = 10 + ((u64)(b[6] & 0x7F) << 21 | (u64)(b[7] & 0x7F) << 14 | (u64)(b[8] & 0x7F) << 7 | (b[9] & 0x7F));
        if (b[5] & 0x10) tag += 10;
        if (n < tag + 8) return 0;
        o = tag;
    }
    for (; o + 10 <= n; o++) {
        adtsh ah = adts_header(b + o);
        if (ah.ok && (a->hint == 0 || a->hint == F4('a', 'd', 't', 's') || a->hint == F4('a', 'a', 'c', ' '))) {
            if (o + (u64)ah.len + 7 > n) return 0;
            adtsh nx = adts_header(b + o + ah.len);
            if (!nx.ok || nx.sfi != ah.sfi) continue;
            a->kind = K_ADTS; a->file_type = F4('a', 'd', 't', 's');
            memset(&a->fmt, 0, sizeof a->fmt);
            a->fmt.mSampleRate = g_sfi[ah.sfi];
            a->fmt.mFormatID = F4('a', 'a', 'c', ' ');
            a->fmt.mFramesPerPacket = 1024;
            a->fmt.mChannelsPerFrame = (u32)ah.ch;
            u8 asc[2] = {(u8)(ah.aot << 3 | ah.sfi >> 1), (u8)((ah.sfi & 1) << 7 | ah.ch << 3)};
            build_esds(a, asc, 2, 0);
            a->data_off = (i64)o;
            a->at = o;
            return 1;
        }
        mp3h mh = mp3_header(b + o);
        if (mh.ok) {
            if (o + (u64)mh.size + 4 > n) return 0;
            mp3h nx = mp3_header(b + o + mh.size);
            if (!nx.ok || nx.rate != mh.rate || nx.layer != mh.layer) continue;
            a->kind = K_MP3;
            a->file_type = mh.layer == 3 ? F4('M', 'P', 'G', '3') : mh.layer == 2 ? F4('M', 'P', 'G', '2') : F4('M', 'P', 'G', '1');
            memset(&a->fmt, 0, sizeof a->fmt);
            a->fmt.mSampleRate = mh.rate;
            a->fmt.mFormatID = mh.layer == 3 ? F4('.', 'm', 'p', '3') : mh.layer == 2 ? F4('.', 'm', 'p', '2') : F4('.', 'm', 'p', '1');
            a->fmt.mFramesPerPacket = (u32)mh.spf;
            a->fmt.mChannelsPerFrame = (u32)mh.ch;
            a->bit_rate = (u32)mh.kbps * 1000;
            a->skip_first_mp3 = mp3_is_xing(b + o, &mh, (int)(n - o));
            a->data_off = (i64)o;
            a->at = o;
            return 1;
        }
        if (o > 65536) return -(int)ERR_UNSUPPORTED_FILE;
    }
    return 0;
}

/* Entrega os pacotes completos que já estão no buffer, num único callback por chamada de ParseBytes. */
static int emit(afs_t *a, int final) {
    aspd_t *pd = 0;
    u32 npd = 0, cap = 0;
    u64 first = 0, last = 0;
    int have = 0;
    /* uma cópia contígua dos pacotes sem cabeçalho ADTS; nos outros formatos os pacotes já são contíguos */
    u8 *out = 0;
    u64 out_n = 0, out_cap = 0;
#define ADD(off, sz) do { \
        if (npd == cap) { u32 nc = cap ? cap * 2 : 64; aspd_t *np2 = realloc(pd, nc * sizeof *np2); if (!np2) goto oom; pd = np2; cap = nc; } \
        pd[npd].mStartOffset = (i64)(off); pd[npd].mDataByteSize = (u32)(sz); pd[npd].mVariableFramesInPacket = 0; npd++; } while (0)
    if (a->kind == K_WAV || a->kind == K_AIFF || (a->kind == K_CAF && (a->fmt.mFormatID == FMT_LPCM || a->fmt.mBytesPerPacket))) {
        u32 bpp = a->fmt.mBytesPerPacket;
        if (!bpp) return 0;
        u64 end = a->base + a->n;
        if (a->data_end && end > a->data_end) end = a->data_end;
        u64 k = end > a->at ? (end - a->at) / bpp : 0;
        if (k) {
            const u8 *p = at_file(a, a->at, k * bpp);
            if (p && a->pp) a->pp(a->client, (u32)(k * bpp), (u32)k, p, 0);
            a->at += k * bpp;
        }
        drop_before(a, a->at);
        return 0;
    }
    if (a->kind == K_CAF || a->kind == K_MP4) {
        /* um callback por trecho contíguo de pacotes completos */
        while (a->pnext < a->np) {
            u64 off = a->poff[a->pnext];
            u32 sz = a->psz[a->pnext];
            if (!at_file(a, off, sz)) break;
            if (npd && off != last) break;
            if (!npd) first = off;
            ADD(off - first, sz);
            last = off + sz;
            a->pnext++;
        }
        if (npd && a->pp) a->pp(a->client, (u32)(last - first), npd, at_file(a, first, last - first), pd);
        u64 end = a->base + a->n, keep_from = a->pnext < a->np ? a->poff[a->pnext] : end;
        drop_before(a, keep_from < end ? keep_from : end);
        free(pd);
        return npd && a->pnext < a->np && at_file(a, a->poff[a->pnext], a->psz[a->pnext]) ? 1 : 0;
    }
    /* MP3 / ADTS */
    for (;;) {
        const u8 *p = at_file(a, a->at, 10);
        if (!p) break;
        if (a->kind == K_ADTS) {
            adtsh h = adts_header(p);
            if (!h.ok) { a->at++; continue; }               /* ressincroniza */
            const u8 *f = at_file(a, a->at, (u64)h.len);
            if (!f) break;
            u32 pay = (u32)(h.len - h.hdr);
            if (out_n + pay > out_cap) { u64 nc = (out_n + pay) * 2 + 4096; u8 *nb = realloc(out, nc); if (!nb) goto oom; out = nb; out_cap = nc; }
            memcpy(out + out_n, f + h.hdr, pay);
            ADD(out_n, pay);
            out_n += pay;
            if (pay > a->max_packet) a->max_packet = pay;
            a->at += (u64)h.len;
        } else {
            mp3h h = mp3_header(p);
            if (!h.ok) { a->at++; continue; }
            if (!at_file(a, a->at, (u64)h.size)) break;
            if (a->skip_first_mp3) { a->skip_first_mp3 = 0; a->at += (u64)h.size; continue; }
            if (!have) { first = a->at; have = 1; }
            if (npd && a->at != last) {
                /* lixo entre quadros: entrega o que já é contíguo e recomeça */
                break;
            }
            ADD(a->at - first, h.size);
            last = a->at + (u64)h.size;
            if ((u32)h.size > a->max_packet) a->max_packet = (u32)h.size;
            a->at += (u64)h.size;
        }
    }
    if (npd && a->pp) {
        if (a->kind == K_ADTS) a->pp(a->client, (u32)out_n, npd, out, pd);
        else a->pp(a->client, (u32)(last - first), npd, at_file(a, first, last - first), pd);
    }
    (void)final;
    drop_before(a, a->at);
    free(pd);
    free(out);
    return 0;
oom:
    free(pd);
    free(out);
    return -1;
#undef ADD
}

OSStatus AudioFileStreamOpen(void *client, afs_listener pl, afs_packets pp, u32 hint, void **out) {
    if (!out || !pp) return ERR_PARAM;
    afs_t *a = calloc(1, sizeof *a);
    if (!a) return -108;
    a->magic = AFS_MAGIC;
    a->client = client;
    a->pl = pl;
    a->pp = pp;
    a->hint = hint;
    *out = a;
    return 0;
}

OSStatus AudioFileStreamParseBytes(void *s, u32 nbytes, const void *data, u32 flags) {
    afs_t *a = afs_of(s);
    if (!a || (nbytes && !data)) return ERR_PARAM;
    if (a->failed) return (OSStatus)a->failed;
    if (flags & 1) {   /* kAudioFileStreamParseFlag_Discontinuity: os bytes novos não continuam os anteriores */
        a->n = 0;
        a->base = a->at;
    }
    if (nbytes && keep(a, data, nbytes)) return -108;
    if (!a->ready) {
        int r = parse_header(a);
        if (r < 0) { a->failed = -r; return (OSStatus)a->failed; }
        if (r == 0) return 0;
        publish(a);
    }
    for (int k = 0; k < 1 << 20; k++) {
        int r = emit(a, 0);
        if (r < 0) return -108;
        if (r == 0) break;
    }
    return 0;
}

OSStatus AudioFileStreamSeek(void *s, i64 packet, i64 *byte_off, u32 *io_flags) {
    afs_t *a = afs_of(s);
    if (!a || !byte_off || packet < 0) return ERR_PARAM;
    if (!a->ready) return ERR_DATA_NOT_READY;
    u32 fl = 0;
    i64 rel;
    if (a->poff && (u64)packet < a->np) {
        rel = (i64)a->poff[packet] - a->data_off;
        a->pnext = (u64)packet;
    } else if (a->fmt.mBytesPerPacket) {
        rel = packet * (i64)a->fmt.mBytesPerPacket;
    } else {
        /* VBR sem tabela (MP3/ADTS): estimativa pela taxa média */
        double bpp = a->bit_rate && a->fmt.mSampleRate ? (double)a->bit_rate / 8 * a->fmt.mFramesPerPacket / a->fmt.mSampleRate : 0;
        if (!bpp) return ERR_VALUE_UNKNOWN;
        rel = (i64)(packet * bpp);
        fl |= 1;   /* kAudioFileStreamSeekFlag_OffsetIsEstimated */
    }
    *byte_off = rel;   /* o app passa a entregar bytes a partir de data_offset + rel */
    a->at = (u64)(a->data_off + rel);
    a->base = a->at;
    a->n = 0;
    a->skip_first_mp3 = 0;
    if (io_flags) *io_flags = fl;
    return 0;
}

static u32 prop_size(afs_t *a, u32 id) {
    switch (id) {
    case F4('r', 'e', 'd', 'y'): case F4('f', 'f', 'm', 't'): case F4('p', 's', 'z', 'm'): case F4('p', 'k', 'u', 'b'):
    case F4('b', 'r', 'a', 't'): case F4('m', 'p', 'r', 'a'): return 4;
    case F4('d', 'f', 'm', 't'): return sizeof(asbd_t);
    case F4('f', 'l', 's', 't'): return sizeof(afmtlist_t);
    case F4('m', 'g', 'i', 'c'): return a->cookie_n;
    case F4('b', 'c', 'n', 't'): case F4('p', 'c', 'n', 't'): case F4('d', 'o', 'f', 'f'): return 8;
    case F4('a', 'b', 'p', 'p'): return 8;
    case F4('p', 'n', 'f', 'o'): return 24;
    }
    return 0;
}

OSStatus AudioFileStreamGetPropertyInfo(void *s, u32 id, u32 *size, u8 *writable) {
    afs_t *a = afs_of(s);
    if (!a) return ERR_PARAM;
    u32 n = prop_size(a, id);
    if (!n && id != F4('m', 'g', 'i', 'c')) return ERR_UNSUPPORTED_PROP;
    if (id == F4('m', 'g', 'i', 'c') && !a->cookie) return ERR_UNSUPPORTED_PROP;
    if (size) *size = n;
    if (writable) *writable = 0;
    return 0;
}

OSStatus AudioFileStreamGetProperty(void *s, u32 id, u32 *size, void *out) {
    afs_t *a = afs_of(s);
    if (!a || !size || !out) return ERR_PARAM;
    u32 need = prop_size(a, id);
    if (id == F4('r', 'e', 'd', 'y')) { if (*size < 4) return ERR_BAD_SIZE; *(u32 *)out = (u32)a->ready; *size = 4; return 0; }
    if (!a->ready) return ERR_DATA_NOT_READY;
    if (!need) return ERR_UNSUPPORTED_PROP;
    if (*size < need) return ERR_BAD_SIZE;
    switch (id) {
    case F4('f', 'f', 'm', 't'): *(u32 *)out = a->file_type; break;
    case F4('d', 'f', 'm', 't'): *(asbd_t *)out = a->fmt; break;
    case F4('f', 'l', 's', 't'): { afmtlist_t *l = out; l->mASBD = a->fmt; l->mChannelLayoutTag = 0; break; }
    case F4('m', 'g', 'i', 'c'): memcpy(out, a->cookie, a->cookie_n); break;
    case F4('b', 'c', 'n', 't'): if (!a->data_bytes) return ERR_VALUE_UNKNOWN; *(u64 *)out = a->data_bytes; break;
    case F4('p', 'c', 'n', 't'):
        if (a->packet_count) *(u64 *)out = a->packet_count;
        else if (a->fmt.mBytesPerPacket && a->data_bytes) *(u64 *)out = a->data_bytes / a->fmt.mBytesPerPacket;
        else return ERR_VALUE_UNKNOWN;
        break;
    case F4('d', 'o', 'f', 'f'): *(i64 *)out = a->data_off; break;
    case F4('p', 's', 'z', 'm'): case F4('p', 'k', 'u', 'b'):
        *(u32 *)out = a->fmt.mBytesPerPacket ? a->fmt.mBytesPerPacket : a->max_packet ? a->max_packet :
                      a->kind == K_MP3 ? 1441 : a->kind == K_ADTS ? 6144 / 8 * (a->fmt.mChannelsPerFrame ? a->fmt.mChannelsPerFrame : 2) : 0;
        if (!*(u32 *)out) return ERR_VALUE_UNKNOWN;
        break;
    case F4('b', 'r', 'a', 't'):
        if (a->bit_rate) *(u32 *)out = a->bit_rate;
        else if (a->fmt.mBytesPerPacket) *(u32 *)out = (u32)(a->fmt.mBytesPerPacket * 8 * a->fmt.mSampleRate / (a->fmt.mFramesPerPacket ? a->fmt.mFramesPerPacket : 1));
        else return ERR_VALUE_UNKNOWN;
        break;
    case F4('m', 'p', 'r', 'a'): *(u32 *)out = 0; break;
    case F4('a', 'b', 'p', 'p'): {
        double v = a->fmt.mBytesPerPacket ? a->fmt.mBytesPerPacket :
                   a->packet_count && a->data_bytes ? (double)a->data_bytes / (double)a->packet_count :
                   a->bit_rate ? (double)a->bit_rate / 8 * a->fmt.mFramesPerPacket / a->fmt.mSampleRate : 0;
        if (!v) return ERR_VALUE_UNKNOWN;
        *(double *)out = v;
        break;
    }
    case F4('p', 'n', 'f', 'o'): {
        /* AudioFilePacketTableInfo: mNumberValidFrames (i64), mPrimingFrames (i32), mRemainderFrames (i32) */
        if (!a->packet_count || !a->fmt.mFramesPerPacket) return ERR_VALUE_UNKNOWN;
        i64 valid = (i64)a->packet_count * a->fmt.mFramesPerPacket - a->priming - a->remainder;
        memset(out, 0, 24);
        memcpy(out, &valid, 8);
        int32_t pr = (int32_t)a->priming, rm = (int32_t)a->remainder;
        memcpy((u8 *)out + 8, &pr, 4);
        memcpy((u8 *)out + 12, &rm, 4);
        need = 16;
        break;
    }
    default: return ERR_UNSUPPORTED_PROP;
    }
    *size = need;
    return 0;
}

OSStatus AudioFileStreamSetProperty(void *s, u32 id, u32 size, const void *data) {
    (void)size; (void)data;
    (void)id;
    return afs_of(s) ? (OSStatus)ERR_UNSUPPORTED_PROP : ERR_PARAM;   /* nenhuma propriedade do fluxo é gravável no macOS */
}

OSStatus AudioFileStreamClose(void *s) {
    afs_t *a = afs_of(s);
    if (!a) return ERR_PARAM;
    if (a->have_mf) mp4_close(&a->mf);
    free(a->buf); free(a->cookie); free(a->poff); free(a->psz);
    a->magic = 0;
    free(a);
    return 0;
}
