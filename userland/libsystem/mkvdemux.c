/*
 * mOS — demuxer Matroska/WebM (RFC 9559, EBML RFC 8794). O arquivo inteiro na memória; percorre o Segment (também com
 * tamanho desconhecido, como em WebM ao vivo): Info (TimecodeScale, Duration), Tracks (TrackEntry com CodecID,
 * CodecPrivate, Audio, Video, DefaultDuration, CodecDelay, SeekPreRoll) e Clusters (Timecode, SimpleBlock, BlockGroup
 * com Block, ReferenceBlock e BlockDuration). Blocos com lacing Xiph, EBML ou de tamanho fixo viram um pacote por quadro.
 * Código original do mOS (MIT).
 */
#include "mkvdemux.h"
void *calloc(unsigned long n, unsigned long s);
void *realloc(void *p, unsigned long n);
void *malloc(unsigned long n);
void free(void *p);
void *memset(void *d, int c, unsigned long n);
void *memcpy(void *d, const void *s, unsigned long n);
int strcmp(const char *a, const char *b);

#define ID_EBML 0x1A45DFA3
#define ID_DOCTYPE 0x4282
#define ID_SEGMENT 0x18538067
#define ID_INFO 0x1549A966
#define ID_TCSCALE 0x2AD7B1
#define ID_DURATION 0x4489
#define ID_TRACKS 0x1654AE6B
#define ID_TRACKENTRY 0xAE
#define ID_TRACKNUMBER 0xD7
#define ID_TRACKTYPE 0x83
#define ID_CODECID 0x86
#define ID_CODECPRIV 0x63A2
#define ID_DEFDUR 0x23E383
#define ID_CODECDELAY 0x56AA
#define ID_SEEKPREROLL 0x56BB
#define ID_AUDIO 0xE1
#define ID_SAMPFREQ 0xB5
#define ID_OUTSAMPFREQ 0x78B5
#define ID_CHANNELS 0x9F
#define ID_BITDEPTH 0x6264
#define ID_VIDEO 0xE0
#define ID_PIXW 0xB0
#define ID_PIXH 0xBA
#define ID_CLUSTER 0x1F43B675
#define ID_TIMECODE 0xE7
#define ID_SIMPLEBLOCK 0xA3
#define ID_BLOCKGROUP 0xA0
#define ID_BLOCK 0xA1
#define ID_REFBLOCK 0xFB
#define UNKNOWN_SIZE (~0ull)

/* id com os bits de comprimento (como nas especificações); 0 = erro */
static int rd_id(const uint8_t *b, uint64_t n, uint64_t *at, uint32_t *id) {
    if (*at >= n) return -1;
    uint8_t c = b[*at];
    int len = c & 0x80 ? 1 : c & 0x40 ? 2 : c & 0x20 ? 3 : c & 0x10 ? 4 : 0;
    if (!len || *at + (uint64_t)len > n) return -1;
    uint32_t v = 0;
    for (int i = 0; i < len; i++) v = v << 8 | b[*at + i];
    *at += (uint64_t)len;
    *id = v;
    return 0;
}
/* vint de tamanho: sem o marcador; todos os bits 1 = desconhecido */
static int rd_size(const uint8_t *b, uint64_t n, uint64_t *at, uint64_t *v) {
    if (*at >= n) return -1;
    uint8_t c = b[*at];
    int len = 1;
    while (len <= 8 && !(c & (0x80 >> (len - 1)))) len++;
    if (len > 8 || *at + (uint64_t)len > n) return -1;
    uint64_t x = c & (0xFF >> len), all = x == (0xFFu >> len);
    for (int i = 1; i < len; i++) { x = x << 8 | b[*at + i]; all = all && b[*at + i] == 0xFF; }
    *at += (uint64_t)len;
    *v = all ? UNKNOWN_SIZE : x;
    return 0;
}
static uint64_t uint_be(const uint8_t *p, uint64_t n) { uint64_t v = 0; for (uint64_t i = 0; i < n && i < 8; i++) v = v << 8 | p[i]; return v; }
static double flt_be(const uint8_t *p, uint64_t n) {
    if (n == 4) { union { uint32_t u; float f; } x; x.u = (uint32_t)uint_be(p, 4); return x.f; }
    if (n == 8) { union { uint64_t u; double d; } x; x.u = uint_be(p, 8); return x.d; }
    return 0;
}

static int add_packet(mkv_file *f, uint64_t off, uint32_t size, int64_t pts, uint32_t track, int key) {
    if (f->np == f->cap) {
        uint32_t nc = f->cap ? f->cap * 2 : 1024;
        mkv_packet *x = realloc(f->p, nc * sizeof *x);
        if (!x) return -3;
        f->p = x; f->cap = nc;
    }
    mkv_packet *p = &f->p[f->np++];
    p->off = off; p->size = size; p->pts_ns = pts; p->track = track; p->key = (uint8_t)key;
    return 0;
}
static int track_of(const mkv_file *f, uint64_t num) { for (uint32_t i = 0; i < f->ntracks; i++) if (f->t[i].number == num) return (int)i; return -1; }

/* Block/SimpleBlock: número da faixa (vint), timecode relativo (s16), flags, lacing */
static int parse_block(mkv_file *f, uint64_t at, uint64_t end, int64_t cluster_tc, int simple, int key_hint) {
    const uint8_t *b = f->b;
    uint64_t tn;
    if (rd_size(b, end, &at, &tn) || at + 3 > end) return -2;
    int ti = track_of(f, tn);
    if (ti < 0) return 0;   /* faixa desconhecida: ignora o bloco */
    int16_t rel = (int16_t)(b[at] << 8 | b[at + 1]);
    uint8_t flags = b[at + 2];
    at += 3;
    int key = simple ? (flags & 0x80) != 0 : key_hint;
    int64_t pts = (cluster_tc + rel) * (int64_t)f->timescale;
    int lacing = (flags >> 1) & 3;
    if (!lacing) return add_packet(f, at, (uint32_t)(end - at), pts, (uint32_t)ti, key);
    if (at >= end) return -2;
    uint32_t nf = (uint32_t)b[at++] + 1, sizes[256];
    uint64_t total = 0;
    if (lacing == 1) {   /* Xiph: cada tamanho é uma soma de bytes até um < 255 */
        for (uint32_t i = 0; i + 1 < nf; i++) {
            uint32_t s = 0;
            for (;;) { if (at >= end) return -2; uint8_t c = b[at++]; s += c; if (c != 255) break; }
            sizes[i] = s; total += s;
        }
    } else if (lacing == 3) {   /* EBML: o primeiro tamanho em vint, os outros em diferenças com sinal */
        uint64_t s0;
        if (rd_size(b, end, &at, &s0)) return -2;
        sizes[0] = (uint32_t)s0; total = s0;
        for (uint32_t i = 1; i + 1 < nf; i++) {
            uint64_t raw, at0 = at;
            if (rd_size(b, end, &at, &raw)) return -2;
            int len = (int)(at - at0);
            int64_t bias = ((int64_t)1 << (7 * len - 1)) - 1;
            int64_t s = (int64_t)sizes[i - 1] + ((int64_t)raw - bias);
            if (s < 0) return -2;
            sizes[i] = (uint32_t)s; total += (uint64_t)s;
        }
    } else {   /* tamanho fixo */
        uint64_t rest = end - at;
        if (rest % nf) return -2;
        for (uint32_t i = 0; i + 1 < nf; i++) { sizes[i] = (uint32_t)(rest / nf); total += sizes[i]; }
    }
    if (at + total > end) return -2;
    sizes[nf - 1] = (uint32_t)(end - at - total);
    uint64_t dd = f->t[ti].default_dur_ns;
    for (uint32_t i = 0; i < nf; i++) {
        int r = add_packet(f, at, sizes[i], pts + (int64_t)(dd * i), (uint32_t)ti, key && i == 0);
        if (r) return r;
        at += sizes[i];
    }
    return 0;
}

static int parse_track(mkv_file *f, uint64_t at, uint64_t end) {
    if (f->ntracks >= 16) return 0;
    mkv_track *t = &f->t[f->ntracks];
    memset(t, 0, sizeof *t);
    const uint8_t *b = f->b;
    while (at < end) {
        uint32_t id; uint64_t sz;
        if (rd_id(b, end, &at, &id) || rd_size(b, end, &at, &sz) || sz == UNKNOWN_SIZE || at + sz > end) return -2;
        const uint8_t *p = b + at;
        switch (id) {
        case ID_TRACKNUMBER: t->number = uint_be(p, sz); break;
        case ID_TRACKTYPE: t->type = (int)uint_be(p, sz); break;
        case ID_CODECID: { uint64_t k = sz < sizeof t->codec - 1 ? sz : sizeof t->codec - 1; memcpy(t->codec, p, k); t->codec[k] = 0; break; }
        case ID_CODECPRIV: free(t->priv); t->priv = malloc(sz ? sz : 1); if (!t->priv) return -3; memcpy(t->priv, p, sz); t->priv_n = (uint32_t)sz; break;
        case ID_DEFDUR: t->default_dur_ns = uint_be(p, sz); break;
        case ID_CODECDELAY: t->codec_delay_ns = uint_be(p, sz); break;
        case ID_SEEKPREROLL: t->seek_preroll_ns = uint_be(p, sz); break;
        case ID_AUDIO: case ID_VIDEO: {
            uint64_t a = at, e = at + sz;
            double out_rate = 0;
            while (a < e) {
                uint32_t sid; uint64_t ssz;
                if (rd_id(b, e, &a, &sid) || rd_size(b, e, &a, &ssz) || a + ssz > e) return -2;
                const uint8_t *q = b + a;
                if (sid == ID_SAMPFREQ) t->rate = flt_be(q, ssz);
                else if (sid == ID_OUTSAMPFREQ) out_rate = flt_be(q, ssz);
                else if (sid == ID_CHANNELS) t->channels = (uint32_t)uint_be(q, ssz);
                else if (sid == ID_BITDEPTH) t->bits = (uint32_t)uint_be(q, ssz);
                else if (sid == ID_PIXW) t->width = (uint32_t)uint_be(q, ssz);
                else if (sid == ID_PIXH) t->height = (uint32_t)uint_be(q, ssz);
                a += ssz;
            }
            if (out_rate > 0) t->rate = out_rate;
            if (id == ID_AUDIO && !t->channels) t->channels = 1;   /* padrão da especificação */
            if (id == ID_AUDIO && t->rate <= 0) t->rate = 8000;
            break;
        }
        }
        at += sz;
    }
    if (t->number) f->ntracks++;
    else { free(t->priv); t->priv = 0; }
    return 0;
}

/* elementos de nível superior dentro do Segment; um Cluster com tamanho desconhecido termina no próximo de nível 1 */
static int is_level1(uint32_t id) { return id == ID_CLUSTER || id == ID_INFO || id == ID_TRACKS || id == 0x1C53BB6B || id == 0x114D9B74 || id == 0x1254C367 || id == 0x1941A469 || id == 0x1043A770; }
static int parse_cluster(mkv_file *f, uint64_t at, uint64_t end, uint64_t *stop) {
    const uint8_t *b = f->b;
    int64_t tc = 0;
    while (at < end) {
        uint64_t start = at;
        uint32_t id; uint64_t sz;
        if (rd_id(b, end, &at, &id)) break;
        if (is_level1(id)) { at = start; break; }
        if (rd_size(b, end, &at, &sz)) return -2;
        if (sz == UNKNOWN_SIZE || at + sz > end) return -2;
        if (id == ID_TIMECODE) tc = (int64_t)uint_be(b + at, sz);
        else if (id == ID_SIMPLEBLOCK) { int r = parse_block(f, at, at + sz, tc, 1, 0); if (r) return r; }
        else if (id == ID_BLOCKGROUP) {
            uint64_t a = at, e = at + sz, blk = 0, blk_n = 0;
            int has_ref = 0;
            while (a < e) {
                uint32_t sid; uint64_t ssz;
                if (rd_id(b, e, &a, &sid) || rd_size(b, e, &a, &ssz) || a + ssz > e) return -2;
                if (sid == ID_BLOCK) { blk = a; blk_n = ssz; }
                else if (sid == ID_REFBLOCK) has_ref = 1;
                a += ssz;
            }
            if (blk_n) { int r = parse_block(f, blk, blk + blk_n, tc, 0, !has_ref); if (r) return r; }
        }
        at += sz;
    }
    *stop = at;
    return 0;
}

int mkv_open_mem(mkv_file *f, const void *bytes, uint64_t n) {
    memset(f, 0, sizeof *f);
    f->b = bytes; f->n = n;
    f->timescale = 1000000;
    const uint8_t *b = bytes;
    uint64_t at = 0, sz;
    uint32_t id;
    if (rd_id(b, n, &at, &id) || id != ID_EBML || rd_size(b, n, &at, &sz) || sz == UNKNOWN_SIZE || at + sz > n) { f->err = "não é EBML"; return -1; }
    for (uint64_t a = at, e = at + sz; a < e;) {
        uint32_t hid; uint64_t hsz;
        if (rd_id(b, e, &a, &hid) || rd_size(b, e, &a, &hsz) || a + hsz > e) return -1;
        if (hid == ID_DOCTYPE) f->webm = hsz == 4 && b[a] == 'w' && b[a + 1] == 'e' && b[a + 2] == 'b' && b[a + 3] == 'm';
        a += hsz;
    }
    at += sz;
    if (rd_id(b, n, &at, &id) || id != ID_SEGMENT || rd_size(b, n, &at, &sz)) { f->err = "sem Segment"; return -1; }
    uint64_t end = sz == UNKNOWN_SIZE || at + sz > n ? n : at + sz;
    int rc = 0;
    while (at < end && !rc) {
        if (rd_id(b, end, &at, &id) || rd_size(b, end, &at, &sz)) break;   /* fim truncado: fica o que já foi lido */
        if (id == ID_CLUSTER) {
            uint64_t ce = sz == UNKNOWN_SIZE || at + sz > end ? end : at + sz, stop;
            rc = parse_cluster(f, at, ce, &stop);
            at = sz == UNKNOWN_SIZE ? stop : ce;
            continue;
        }
        if (sz == UNKNOWN_SIZE || at + sz > end) break;
        if (id == ID_INFO) {
            for (uint64_t a = at, e = at + sz; a < e;) {
                uint32_t sid; uint64_t ssz;
                if (rd_id(b, e, &a, &sid) || rd_size(b, e, &a, &ssz) || a + ssz > e) { rc = -2; break; }
                if (sid == ID_TCSCALE) f->timescale = uint_be(b + a, ssz);
                else if (sid == ID_DURATION) f->duration_ns = flt_be(b + a, ssz);
                a += ssz;
            }
            if (!f->timescale) f->timescale = 1000000;
            f->duration_ns *= (double)f->timescale;
        } else if (id == ID_TRACKS) {
            for (uint64_t a = at, e = at + sz; a < e && !rc;) {
                uint32_t sid; uint64_t ssz;
                if (rd_id(b, e, &a, &sid) || rd_size(b, e, &a, &ssz) || a + ssz > e) { rc = -2; break; }
                if (sid == ID_TRACKENTRY) rc = parse_track(f, a, a + ssz);
                a += ssz;
            }
        }
        at += sz;
    }
    if (rc) { f->err = rc == -3 ? "sem memória" : "corrompido"; mkv_close(f); return rc; }
    if (!f->ntracks) { f->err = "sem faixas"; mkv_close(f); return -2; }
    return 0;
}
void mkv_close(mkv_file *f) {
    for (uint32_t i = 0; i < f->ntracks; i++) free(f->t[i].priv);
    free(f->p);
    f->p = 0; f->np = f->cap = 0; f->ntracks = 0;
}
int mkv_find_track(const mkv_file *f, int type) { for (uint32_t i = 0; i < f->ntracks; i++) if (f->t[i].type == type) return (int)i; return -1; }
