/*
 * mOS — gravação de MP4/MOV/M4A (ISO/IEC 14496-12 e 14496-14; QuickTime File Format para o MOV).
 * ftyp + mdat (tamanho de 64 bits, as amostras na ordem em que chegam) + moov no fechamento. Com faststart o moov vai
 * para antes do mdat (os offsets dos chunks são deslocados e o arquivo é regravado). Faixas: H.264 ('avc1' + avcC),
 * AAC ('mp4a' + esds, priming na edit list), Opus ('Opus' + dOps) e PCM 16 bits ('sowt', só MOV, como a AVFoundation).
 * Amostras seguidas da mesma faixa formam um chunk; stco vira co64 quando algum offset passa de 4 GiB.
 * Código original do mOS (MIT).
 */
#include "mp4mux.h"

typedef unsigned long size_t_;
void *malloc(unsigned long n);
void *calloc(unsigned long n, unsigned long s);
void *realloc(void *p, unsigned long n);
void free(void *p);
void *memcpy(void *d, const void *s, unsigned long n);
void *memset(void *d, int c, unsigned long n);
unsigned long strlen(const char *s);
void *fopen(const char *p, const char *m);
int fclose(void *f);
int fseek(void *f, long off, int whence);
long ftell(void *f);
unsigned long fwrite(const void *p, unsigned long z, unsigned long n, void *f);
unsigned long fread(void *p, unsigned long z, unsigned long n, void *f);
int rename(const char *a, const char *b);
int remove(const char *p);
int snprintf(char *s, unsigned long n, const char *fmt, ...);

typedef struct { uint32_t size, dur; int32_t cto; uint8_t sync; } wsample_t;
typedef struct { uint64_t off; uint32_t count; } wchunk_t;
typedef struct {
    int kind;                   /* 1 h264, 2 aac, 3 opus, 4 lpcm */
    uint32_t timescale;
    int width, height, rate, channels, bits;
    uint8_t *cfg; uint32_t cfg_n;
    uint32_t bitrate, priming, preskip, in_rate;
    wsample_t *s; uint32_t ns, cs;
    wchunk_t *c; uint32_t nc, cc;
    int64_t first_dts;
    int has_cto;
    int64_t delay, skip, limit;   /* edit list da sessão (mp4w_set_edit) */
    int32_t mtx[9]; int has_mtx;
    uint64_t pcm_frames;          /* LPCM: cada quadro de áudio é uma amostra de tamanho fixo (stsz constante) */
} wtrack_t;
struct mp4w {
    void *f;
    char path[1024];
    int kind, faststart, failed;
    uint64_t mdat_start, pos;   /* início do cabeçalho do mdat; próxima posição de escrita */
    int last_track;
    wtrack_t t[8];
    int nt;
};

/* ---- buffer de caixas */
typedef struct { uint8_t *b; size_t_ n, cap; int err; } buf_t;
static void put(buf_t *b, const void *d, size_t_ n) {
    if (b->err) return;
    if (b->n + n > b->cap) {
        size_t_ nc = (b->n + n) * 2 + 256;
        uint8_t *x = realloc(b->b, nc);
        if (!x) { b->err = 1; return; }
        b->b = x; b->cap = nc;
    }
    memcpy(b->b + b->n, d, n);
    b->n += n;
}
static void w8(buf_t *b, unsigned v) { uint8_t x = (uint8_t)v; put(b, &x, 1); }
static void w16(buf_t *b, unsigned v) { uint8_t x[2] = {(uint8_t)(v >> 8), (uint8_t)v}; put(b, x, 2); }
static void w32(buf_t *b, uint32_t v) { uint8_t x[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v}; put(b, x, 4); }
static void w64(buf_t *b, uint64_t v) { w32(b, (uint32_t)(v >> 32)); w32(b, (uint32_t)v); }
static void zeros(buf_t *b, int n) { for (int i = 0; i < n; i++) w8(b, 0); }
static void fourcc(buf_t *b, const char *c) { put(b, c, 4); }
static size_t_ box(buf_t *b, const char *type) { size_t_ at = b->n; w32(b, 0); fourcc(b, type); return at; }
static size_t_ fullbox(buf_t *b, const char *type, int ver, uint32_t flags) { size_t_ at = box(b, type); w32(b, (uint32_t)ver << 24 | (flags & 0xffffff)); return at; }
static void end(buf_t *b, size_t_ at) {
    if (b->err) return;
    uint32_t n = (uint32_t)(b->n - at);
    b->b[at] = (uint8_t)(n >> 24); b->b[at + 1] = (uint8_t)(n >> 16); b->b[at + 2] = (uint8_t)(n >> 8); b->b[at + 3] = (uint8_t)n;
}
static void matrix(buf_t *b) { w32(b, 0x10000); w32(b, 0); w32(b, 0); w32(b, 0); w32(b, 0x10000); w32(b, 0); w32(b, 0); w32(b, 0); w32(b, 0x40000000); }

static int fw(mp4w_t *m, const void *d, size_t_ n) {
    if (m->failed) return -1;
    if (n && fwrite(d, 1, n, m->f) != n) { m->failed = 1; return -1; }
    m->pos += n;
    return 0;
}

mp4w_t *mp4w_open(const char *path, int kind, int faststart) {
    if (!path || strlen(path) >= 1000) return 0;
    void *f = fopen(path, "w+b");   /* o faststart relê o mdat */
    if (!f) return 0;
    mp4w_t *m = calloc(1, sizeof *m);
    if (!m) { fclose(f); return 0; }
    m->f = f;
    snprintf(m->path, sizeof m->path, "%s", path);
    m->kind = kind;
    m->faststart = faststart;
    m->last_track = -1;
    buf_t b = {0};
    size_t_ at = box(&b, "ftyp");
    if (kind == MP4W_MOV) { fourcc(&b, "qt  "); w32(&b, 0x20050300); fourcc(&b, "qt  "); }
    else if (kind == MP4W_M4A) { fourcc(&b, "M4A "); w32(&b, 0); fourcc(&b, "M4A "); fourcc(&b, "mp42"); fourcc(&b, "isom"); }
    else { fourcc(&b, "isom"); w32(&b, 0x200); fourcc(&b, "isom"); fourcc(&b, "iso2"); fourcc(&b, "avc1"); fourcc(&b, "mp41"); }
    end(&b, at);
    m->mdat_start = b.n;
    w32(&b, 1); fourcc(&b, "mdat"); w64(&b, 0);   /* largesize, corrigido no fechamento */
    if (b.err || fw(m, b.b, b.n)) { free(b.b); fclose(f); free(m); return 0; }
    free(b.b);
    return m;
}

static wtrack_t *newtrack(mp4w_t *m, int kind) {
    if (!m || m->failed || m->nt >= 8) return 0;
    wtrack_t *t = &m->t[m->nt];   /* pode chegar depois das amostras das outras (o avcC do codificador só sai no 1º quadro) */
    memset(t, 0, sizeof *t);
    t->kind = kind;
    t->first_dts = -1;
    t->limit = -1;
    return t;
}
static int setcfg(wtrack_t *t, const uint8_t *d, uint32_t n) {
    if (!n) return 0;
    t->cfg = malloc(n);
    if (!t->cfg) return -1;
    memcpy(t->cfg, d, n);
    t->cfg_n = n;
    return 0;
}
int mp4w_add_h264(mp4w_t *m, int width, int height, const uint8_t *avcc, uint32_t avcc_n, uint32_t timescale) {
    wtrack_t *t = newtrack(m, 1);
    if (!t || width <= 0 || height <= 0 || !avcc || avcc_n < 7 || avcc[0] != 1 || setcfg(t, avcc, avcc_n)) return -1;
    t->width = width; t->height = height; t->timescale = timescale ? timescale : 90000;
    return m->nt++;
}
int mp4w_add_aac(mp4w_t *m, int rate, int channels, const uint8_t *asc, uint32_t asc_n, uint32_t bitrate, uint32_t priming) {
    wtrack_t *t = newtrack(m, 2);
    if (!t || rate <= 0 || channels < 1 || !asc || asc_n < 2 || asc_n > 64 || setcfg(t, asc, asc_n)) return -1;
    t->rate = rate; t->channels = channels; t->timescale = (uint32_t)rate; t->bitrate = bitrate; t->priming = priming;
    return m->nt++;
}
int mp4w_add_opus(mp4w_t *m, int channels, uint32_t preskip, uint32_t in_rate) {
    wtrack_t *t = newtrack(m, 3);
    if (!t || channels < 1 || channels > 2 || m->kind == MP4W_MOV) return -1;
    t->rate = 48000; t->channels = channels; t->timescale = 48000; t->preskip = preskip; t->in_rate = in_rate;
    t->priming = preskip;
    return m->nt++;
}
int mp4w_add_lpcm(mp4w_t *m, int rate, int channels, int bits) {
    if (!m || m->kind != MP4W_MOV || bits != 16) return -1;   /* MP4/M4A não aceitam LPCM (como na AVFoundation) */
    wtrack_t *t = newtrack(m, 4);
    if (!t || rate <= 0 || channels < 1) return -1;
    t->rate = rate; t->channels = channels; t->bits = bits; t->timescale = (uint32_t)rate;
    return m->nt++;
}

int mp4w_set_edit(mp4w_t *m, int track, int64_t delay, int64_t skip, int64_t limit) {
    if (!m || track < 0 || track >= m->nt || delay < 0 || skip < 0) return -1;
    m->t[track].delay = delay; m->t[track].skip = skip; m->t[track].limit = limit;
    return 0;
}
int mp4w_set_matrix(mp4w_t *m, int track, const int32_t mtx[9]) {
    if (!m || track < 0 || track >= m->nt || !mtx) return -1;
    memcpy(m->t[track].mtx, mtx, sizeof m->t[track].mtx);
    m->t[track].has_mtx = 1;
    return 0;
}

int mp4w_write(mp4w_t *m, int track, const uint8_t *data, uint32_t size, int64_t pts, int64_t dts, uint32_t dur, int sync) {
    if (!m || m->failed || track < 0 || track >= m->nt || !data || !size) return -1;
    wtrack_t *t = &m->t[track];
    uint32_t bpf = (uint32_t)(t->channels * t->bits / 8);
    if (t->kind == 4) {   /* LPCM: o bloco vira size/bpf amostras de um quadro no chunk */
        if (!bpf || size % bpf) return -1;
        if (m->last_track != track || !t->nc) {
            if (t->nc == t->cc) {
                uint32_t nc = t->cc ? t->cc * 2 : 64;
                wchunk_t *x = realloc(t->c, nc * sizeof *x);
                if (!x) { m->failed = 1; return -1; }
                t->c = x; t->cc = nc;
            }
            t->c[t->nc].off = m->pos;
            t->c[t->nc].count = 0;
            t->nc++;
        }
        t->c[t->nc - 1].count += size / bpf;
        t->pcm_frames += size / bpf;
        m->last_track = track;
        return fw(m, data, size);
    }
    if (t->ns == t->cs) {
        uint32_t nc = t->cs ? t->cs * 2 : 256;
        wsample_t *x = realloc(t->s, nc * sizeof *x);
        if (!x) { m->failed = 1; return -1; }
        t->s = x; t->cs = nc;
    }
    if (t->first_dts < 0) t->first_dts = dts;
    int64_t cto = pts - dts;
    if (cto > 0x7fffffff || cto < -0x7fffffff) return -1;
    wsample_t *w = &t->s[t->ns];
    w->size = size; w->dur = dur; w->cto = (int32_t)cto; w->sync = sync ? 1 : 0;
    if (cto) t->has_cto = 1;
    /* chunk novo quando a faixa muda (o anterior continua enquanto as amostras dela vêm seguidas) */
    if (m->last_track != track || !t->nc) {
        if (t->nc == t->cc) {
            uint32_t nc = t->cc ? t->cc * 2 : 64;
            wchunk_t *x = realloc(t->c, nc * sizeof *x);
            if (!x) { m->failed = 1; return -1; }
            t->c = x; t->cc = nc;
        }
        t->c[t->nc].off = m->pos;
        t->c[t->nc].count = 0;
        t->nc++;
    }
    t->c[t->nc - 1].count++;
    m->last_track = track;
    t->ns++;
    return fw(m, data, size);
}

/* ---- moov */
static void esds(buf_t *b, const wtrack_t *t) {
    size_t_ at = fullbox(b, "esds", 0, 0);
    uint32_t dsi = 2 + t->cfg_n, dcd = 13 + 5 + t->cfg_n - 0, es = 3 + 5 + dcd + 5 + 1;
    (void)dsi;
    /* tamanhos em forma expandida de 4 bytes (0x80 0x80 0x80 n) */
    w8(b, 3); w8(b, 0x80); w8(b, 0x80); w8(b, 0x80); w8(b, es);
    w16(b, 0); w8(b, 0);                                     /* ES_ID, flags */
    w8(b, 4); w8(b, 0x80); w8(b, 0x80); w8(b, 0x80); w8(b, dcd);
    w8(b, 0x40); w8(b, 0x15);                               /* AAC, áudio */
    w8(b, 0); w16(b, 0);                                     /* bufferSizeDB */
    w32(b, t->bitrate); w32(b, t->bitrate);
    w8(b, 5); w8(b, 0x80); w8(b, 0x80); w8(b, 0x80); w8(b, t->cfg_n);
    put(b, t->cfg, t->cfg_n);
    w8(b, 6); w8(b, 0x80); w8(b, 0x80); w8(b, 0x80); w8(b, 1); w8(b, 2);
    end(b, at);
}
static void sample_entry(buf_t *b, const mp4w_t *m, const wtrack_t *t) {
    size_t_ at;
    if (t->kind == 1) {
        at = box(b, "avc1");
        zeros(b, 6); w16(b, 1);                               /* data_reference_index */
        zeros(b, 16);
        w16(b, (unsigned)t->width); w16(b, (unsigned)t->height);
        w32(b, 0x00480000); w32(b, 0x00480000); w32(b, 0); w16(b, 1);
        uint8_t name[32] = {0};
        static const char cn[] = "H.264";
        name[0] = sizeof cn - 1; memcpy(name + 1, cn, sizeof cn - 1);
        put(b, name, 32);
        w16(b, 0x18); w16(b, 0xffff);
        size_t_ c = box(b, "avcC"); put(b, t->cfg, t->cfg_n); end(b, c);
        end(b, at);
        return;
    }
    at = box(b, t->kind == 2 ? "mp4a" : t->kind == 3 ? "Opus" : "sowt");
    zeros(b, 6); w16(b, 1);
    zeros(b, 8);                                              /* versão 0 (MOV e MP4) */
    w16(b, (unsigned)t->channels); w16(b, 16); w16(b, 0); w16(b, 0);
    w32(b, (uint32_t)(t->rate > 65535 ? 0 : t->rate) << 16);
    if (t->kind == 2) esds(b, t);
    else if (t->kind == 3) {
        size_t_ d = box(b, "dOps");
        w8(b, 0); w8(b, (unsigned)t->channels); w16(b, t->preskip); w32(b, t->in_rate); w16(b, 0); w8(b, 0);
        end(b, d);
    }
    (void)m;
    end(b, at);
}
static uint64_t media_duration(const wtrack_t *t) { if (t->kind == 4) return t->pcm_frames; uint64_t d = 0; for (uint32_t i = 0; i < t->ns; i++) d += t->s[i].dur; return d; }
/* edit list de uma faixa: [vazio de delay] + (pd a partir de media_time). lead = atraso dos quadros B (vídeo) ou priming
 * (áudio); a sessão (mp4w_set_edit) soma o corte do começo e limita a duração. 1 se a faixa precisa de edts. */
static int track_edit(const wtrack_t *t, int64_t *media_time, uint64_t *pd) {
    uint64_t md = media_duration(t);
    int64_t lead = (int64_t)t->priming;
    if (t->kind == 1 && t->has_cto) {   /* menor tempo de composição (em geral o do 1º quadro-chave) */
        int64_t d = 0;
        lead = INT64_MAX;
        for (uint32_t k = 0; k < t->ns; k++) { if (d + t->s[k].cto < lead) lead = d + t->s[k].cto; d += t->s[k].dur; }
    }
    if (lead < 0) lead = 0;
    /* atraso de quadros B: a apresentação dura a mídia inteira (só desloca); priming de áudio: some do começo */
    uint64_t nat = t->kind == 1 ? md : md > (uint64_t)lead ? md - (uint64_t)lead : 0;
    uint64_t p = nat > (uint64_t)t->skip ? nat - (uint64_t)t->skip : 0;
    if (t->limit >= 0 && p > (uint64_t)t->limit) p = (uint64_t)t->limit;
    *media_time = lead + t->skip;
    *pd = p;
    return *media_time > 0 || t->delay > 0 || p != nat;
}
static uint64_t to_movie(uint64_t v, uint32_t ts, uint32_t mts) { return ts ? (v * mts + ts / 2) / ts : 0; }
static int write_moov(mp4w_t *m, buf_t *b, int64_t shift, int co64) {
    uint32_t mts = 1000;
    uint64_t movie_dur = 0;
    for (int i = 0; i < m->nt; i++) {
        const wtrack_t *t = &m->t[i];
        int64_t mt; uint64_t pd;
        track_edit(t, &mt, &pd);
        uint64_t d = to_movie((uint64_t)t->delay, t->timescale, mts) + to_movie(pd, t->timescale, mts);
        if (d > movie_dur) movie_dur = d;
    }
    size_t_ moov = box(b, "moov");
    size_t_ at = fullbox(b, "mvhd", 1, 0);
    w64(b, 0); w64(b, 0); w32(b, mts); w64(b, movie_dur);
    w32(b, 0x10000); w16(b, 0x100); zeros(b, 10); matrix(b); zeros(b, 24); w32(b, (uint32_t)m->nt + 1);
    end(b, at);
    for (int i = 0; i < m->nt; i++) {
        const wtrack_t *t = &m->t[i];
        uint64_t md = media_duration(t);
        int64_t mt; uint64_t pd;
        int need_edts = track_edit(t, &mt, &pd);
        uint64_t dly = to_movie((uint64_t)t->delay, t->timescale, mts), pdm = to_movie(pd, t->timescale, mts);
        size_t_ trak = box(b, "trak");
        at = fullbox(b, "tkhd", 1, 3);                        /* habilitada, no filme */
        w64(b, 0); w64(b, 0); w32(b, (uint32_t)i + 1); w32(b, 0); w64(b, dly + pdm);
        zeros(b, 8); w16(b, 0); w16(b, t->kind == 1 ? 0 : 1); w16(b, t->kind == 1 ? 0 : 0x100); w16(b, 0);
        if (t->has_mtx) for (int k = 0; k < 9; k++) w32(b, (uint32_t)t->mtx[k]);
        else matrix(b);
        w32(b, t->kind == 1 ? (uint32_t)t->width << 16 : 0); w32(b, t->kind == 1 ? (uint32_t)t->height << 16 : 0);
        end(b, at);
        if (need_edts) {   /* edit list: vazio do começo da sessão; depois do priming / do atraso dos quadros B / do corte */
            size_t_ ed = box(b, "edts");
            at = fullbox(b, "elst", 1, 0);
            w32(b, dly ? 2 : 1);
            if (dly) { w64(b, dly); w64(b, (uint64_t)-1); w16(b, 1); w16(b, 0); }
            w64(b, pdm); w64(b, (uint64_t)mt); w16(b, 1); w16(b, 0);
            end(b, at); end(b, ed);
        }
        size_t_ mdia = box(b, "mdia");
        at = fullbox(b, "mdhd", 1, 0);
        w64(b, 0); w64(b, 0); w32(b, t->timescale); w64(b, md); w16(b, 0x55c4); w16(b, 0);   /* idioma "und" */
        end(b, at);
        at = fullbox(b, "hdlr", 0, 0);
        w32(b, m->kind == MP4W_MOV ? 0x6d686c72 /* mhlr */ : 0); fourcc(b, t->kind == 1 ? "vide" : "soun"); zeros(b, 12);
        const char *hn = t->kind == 1 ? "VideoHandler" : "SoundHandler";
        if (m->kind == MP4W_MOV) w8(b, (unsigned)strlen(hn));
        put(b, hn, strlen(hn) + (m->kind == MP4W_MOV ? 0 : 1));
        end(b, at);
        size_t_ minf = box(b, "minf");
        if (t->kind == 1) { at = fullbox(b, "vmhd", 0, 1); zeros(b, 8); end(b, at); }
        else { at = fullbox(b, "smhd", 0, 0); zeros(b, 4); end(b, at); }
        size_t_ dinf = box(b, "dinf");
        size_t_ dref = fullbox(b, "dref", 0, 0); w32(b, 1); at = fullbox(b, "url ", 0, 1); end(b, at); end(b, dref);
        end(b, dinf);
        size_t_ stbl = box(b, "stbl");
        at = fullbox(b, "stsd", 0, 0); w32(b, 1); sample_entry(b, m, t); end(b, at);
        /* stts: durações em sequência */
        at = fullbox(b, "stts", 0, 0);
        size_t_ cnt = b->n; w32(b, 0);
        uint32_t ne = 0;
        if (t->kind == 4 && t->pcm_frames) { w32(b, (uint32_t)t->pcm_frames); w32(b, 1); ne = 1; }
        for (uint32_t k = 0; k < t->ns;) {
            uint32_t r = 1;
            while (k + r < t->ns && t->s[k + r].dur == t->s[k].dur) r++;
            w32(b, r); w32(b, t->s[k].dur); ne++; k += r;
        }
        if (!b->err) { b->b[cnt] = (uint8_t)(ne >> 24); b->b[cnt + 1] = (uint8_t)(ne >> 16); b->b[cnt + 2] = (uint8_t)(ne >> 8); b->b[cnt + 3] = (uint8_t)ne; }
        end(b, at);
        if (t->has_cto) {
            int neg = 0;
            for (uint32_t k = 0; k < t->ns; k++) if (t->s[k].cto < 0) neg = 1;
            at = fullbox(b, "ctts", neg ? 1 : 0, 0);
            cnt = b->n; w32(b, 0); ne = 0;
            for (uint32_t k = 0; k < t->ns;) {
                uint32_t r = 1;
                while (k + r < t->ns && t->s[k + r].cto == t->s[k].cto) r++;
                w32(b, r); w32(b, (uint32_t)t->s[k].cto); ne++; k += r;
            }
            if (!b->err) { b->b[cnt] = (uint8_t)(ne >> 24); b->b[cnt + 1] = (uint8_t)(ne >> 16); b->b[cnt + 2] = (uint8_t)(ne >> 8); b->b[cnt + 3] = (uint8_t)ne; }
            end(b, at);
        }
        if (t->kind == 1) {   /* stss: quadros-chave (sem ele, todos seriam chave) */
            uint32_t nk = 0;
            for (uint32_t k = 0; k < t->ns; k++) nk += t->s[k].sync;
            if (nk < t->ns) {
                at = fullbox(b, "stss", 0, 0); w32(b, nk);
                for (uint32_t k = 0; k < t->ns; k++) if (t->s[k].sync) w32(b, k + 1);
                end(b, at);
            }
        }
        at = fullbox(b, "stsc", 0, 0);
        cnt = b->n; w32(b, 0); ne = 0;
        for (uint32_t k = 0; k < t->nc; k++)
            if (!k || t->c[k].count != t->c[k - 1].count) { w32(b, k + 1); w32(b, t->c[k].count); w32(b, 1); ne++; }
        if (!b->err) { b->b[cnt] = (uint8_t)(ne >> 24); b->b[cnt + 1] = (uint8_t)(ne >> 16); b->b[cnt + 2] = (uint8_t)(ne >> 8); b->b[cnt + 3] = (uint8_t)ne; }
        end(b, at);
        at = fullbox(b, "stsz", 0, 0);
        if (t->kind == 4) { w32(b, (uint32_t)(t->channels * t->bits / 8)); w32(b, (uint32_t)t->pcm_frames); }
        else { w32(b, 0); w32(b, t->ns); }
        for (uint32_t k = 0; k < t->ns; k++) w32(b, t->s[k].size);
        end(b, at);
        at = fullbox(b, co64 ? "co64" : "stco", 0, 0);
        w32(b, t->nc);
        for (uint32_t k = 0; k < t->nc; k++) { uint64_t o = t->c[k].off + (uint64_t)shift; if (co64) w64(b, o); else w32(b, (uint32_t)o); }
        end(b, at);
        end(b, stbl); end(b, minf); end(b, mdia); end(b, trak);
    }
    end(b, moov);
    return b->err ? -1 : 0;
}

static void freeall(mp4w_t *m) {
    for (int i = 0; i < m->nt; i++) { free(m->t[i].s); free(m->t[i].c); free(m->t[i].cfg); }
    free(m);
}
int mp4w_close(mp4w_t *m) {
    if (!m) return -1;
    int rc = m->failed ? -1 : 0;
    uint64_t end_pos = m->pos, mdat_size = end_pos - m->mdat_start;
    int co64 = end_pos > 0xF0000000ull;
    if (!rc) {   /* tamanho do mdat (largesize) */
        uint8_t ls[8];
        for (int k = 0; k < 8; k++) ls[k] = (uint8_t)(mdat_size >> (56 - 8 * k));
        if (fseek(m->f, (long)(m->mdat_start + 8), 0) || fwrite(ls, 1, 8, m->f) != 8) rc = -1;
    }
    buf_t b = {0};
    if (!rc && !m->faststart) {
        if (write_moov(m, &b, 0, co64) || fseek(m->f, (long)end_pos, 0) || fwrite(b.b, 1, b.n, m->f) != b.n) rc = -1;
        if (fclose(m->f)) rc = -1;
    } else if (!rc) {
        /* faststart: ftyp + moov + mdat; os offsets dos chunks andam o tamanho do moov (calculado duas vezes: o tamanho
         * não depende dos valores, só de stco/co64) */
        if (write_moov(m, &b, 0, co64)) rc = -1;
        int64_t shift = (int64_t)b.n;
        b.n = 0;
        if (!rc && write_moov(m, &b, shift, co64)) rc = -1;
        char tmp[1100];
        snprintf(tmp, sizeof tmp, "%s.mosfast", m->path);
        void *o = rc ? 0 : fopen(tmp, "wb");
        if (!o) rc = -1;
        if (!rc) {
            uint8_t *cp = malloc(1 << 20);
            if (!cp) rc = -1;
            if (!rc && (fseek(m->f, 0, 0) || fread(cp, 1, m->mdat_start, m->f) != m->mdat_start || fwrite(cp, 1, m->mdat_start, o) != m->mdat_start)) rc = -1;   /* ftyp */
            if (!rc && fwrite(b.b, 1, b.n, o) != b.n) rc = -1;
            uint64_t left = mdat_size;
            if (!rc && fseek(m->f, (long)m->mdat_start, 0)) rc = -1;
            while (!rc && left) {
                unsigned long k = left > (1 << 20) ? (1 << 20) : (unsigned long)left;
                if (fread(cp, 1, k, m->f) != k || fwrite(cp, 1, k, o) != k) rc = -1;
                left -= k;
            }
            free(cp);
            if (fclose(o)) rc = -1;
        }
        fclose(m->f);
        if (!rc && rename(tmp, m->path)) rc = -1;
        if (rc) remove(tmp);
    } else fclose(m->f);
    free(b.b);
    freeall(m);
    return rc;
}
void mp4w_abort(mp4w_t *m) {
    if (!m) return;
    fclose(m->f);
    remove(m->path);
    freeall(m);
}
