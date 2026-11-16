/* mOS — vídeo de MP4/MOV: ver avvideo.h.
 * Decodifica na ordem do arquivo e recebe os quadros na ordem de exibição (cx_h264 em modo de exibição, carimbo = índice
 * da amostra). Guarda o quadro atual (o último com pts <= t) e uma fila dos seguintes já decodificados (o flush do fim
 * entrega vários de uma vez). Tempo para trás, ou
 * além do próximo quadro-chave, recomeça no quadro-chave anterior ao tempo pedido com um decodificador novo. */
#include "avvideo.h"
#include "mp4demux.h"

typedef unsigned long size_t_;
void *malloc(unsigned long n);
void *calloc(unsigned long n, unsigned long s);
void *realloc(void *p, unsigned long n);
void free(void *p);
void *memcpy(void *d, const void *s, unsigned long n);
void *memset(void *d, int c, unsigned long n);
void *fopen(const char *p, const char *m);
int fclose(void *f);
int fseek(void *f, long off, int whence);
long ftell(void *f);
unsigned long fread(void *p, unsigned long z, unsigned long n, void *f);

typedef struct { int full_range, matrix, primaries, transfer; } cx_h264_color;
typedef void (*cx_h264_emit)(void *ctx, const uint8_t *y, const uint8_t *u, const uint8_t *v, int sy, int suv, int w, int h,
                             uint64_t tag, const cx_h264_color *c);
void *cx_h264_open(int decode_order);
int cx_h264_decode(void *d, const uint8_t *annexb, int n, uint64_t tag, cx_h264_emit emit, void *ctx);
int cx_h264_flush(void *d, cx_h264_emit emit, void *ctx);
void cx_h264_close(void *d);

typedef struct { uint8_t *buf; int w, h, valid; double pts; cx_h264_color col; } slot_t;
struct avv {
    void *f;
    mp4_file m;
    mp4_track *t;
    uint8_t *ps;                /* SPS e PPS do avcC em Annex B */
    int ps_n, nal;
    void *dec;
    uint32_t next;              /* próxima amostra a entregar ao decodificador */
    int flushed;
    slot_t cur;                 /* quadro atual */
    slot_t q[17];               /* seguintes, em ordem de exibição (o DPB do H.264 tem no máximo 16) */
    int qn;
    double floor_pts;           /* quadros com pts menor (antes do quadro-chave de um seek) são descartados */
    uint8_t *sample, *annexb;
    uint32_t sample_cap, annexb_cap;
    int w, h;
};

static int rd(void *ctx, uint64_t off, void *dst, uint32_t n) {
    void *f = ctx;
    if (fseek(f, (long)off, 0)) return -1;
    return fread(dst, 1, n, f) == n ? 0 : -1;
}

/* avcC: versão, perfil, compat, nível, 111111 + (tamanho da NAL - 1), 111 + nº de SPS, [tam SPS], nº de PPS, [tam PPS] */
static int parse_avcc(avv_t *v, const uint8_t *a, uint32_t n) {
    if (n < 7) return -1;
    v->nal = (a[4] & 3) + 1;
    uint32_t p = 5, cap = n * 2 + 16;
    v->ps = malloc(cap);
    if (!v->ps) return -1;
    int nsps = a[p++] & 31;
    for (int pass = 0; pass < 2; pass++) {
        int cnt = pass ? (p < n ? a[p++] : 0) : nsps;
        for (int i = 0; i < cnt; i++) {
            if (p + 2 > n) return -1;
            uint32_t len = (uint32_t)a[p] << 8 | a[p + 1];
            p += 2;
            if (p + len > n) return -1;
            uint8_t *d = v->ps + v->ps_n;
            d[0] = 0; d[1] = 0; d[2] = 0; d[3] = 1;
            memcpy(d + 4, a + p, len);
            v->ps_n += (int)len + 4;
            p += len;
        }
    }
    return v->ps_n > 0 ? 0 : -1;
}

avv_t *avv_open_path(const char *path, int *err) {
    if (err) *err = -11800;
    void *f = path ? fopen(path, "rb") : 0;
    if (!f) return 0;
    fseek(f, 0, 2);
    long size = ftell(f);
    avv_t *v = calloc(1, sizeof *v);
    if (!v || size <= 0) { if (v) free(v); fclose(f); return 0; }
    v->f = f;
    if (mp4_open(&v->m, rd, f, (uint64_t)size)) { if (err) *err = -11828; fclose(f); free(v); return 0; }
    int ti = mp4_find_track(&v->m, MP4_FOURCC('v', 'i', 'd', 'e'));
    if (ti < 0) { if (err) *err = -11828; avv_close(v); return 0; }
    v->t = &v->m.t[ti];
    uint32_t c = v->t->codec;
    if ((c != MP4_FOURCC('a', 'v', 'c', '1') && c != MP4_FOURCC('a', 'v', 'c', '3')) || v->t->encrypted || !v->t->cfg ||
        parse_avcc(v, v->t->cfg, v->t->cfg_len)) {
        if (err) *err = -11838;
        avv_close(v);
        return 0;
    }
    v->w = (int)v->t->width;
    v->h = (int)v->t->height;
    v->floor_pts = -1e300;
    if (err) *err = 0;
    return v;
}

static void slot_free(slot_t *s) { free(s->buf); memset(s, 0, sizeof *s); }
void avv_close(avv_t *v) {
    if (!v) return;
    if (v->dec) cx_h264_close(v->dec);
    slot_free(&v->cur);
    for (int i = 0; i < 17; i++) slot_free(&v->q[i]);
    free(v->ps);
    free(v->sample);
    free(v->annexb);
    mp4_close(&v->m);
    if (v->f) fclose(v->f);
    free(v);
}

void avv_info(const avv_t *v, int *w, int *h, double *duration, double *fps, uint32_t *nsamples) {
    double dur = v ? mp4_track_seconds(v->t) : 0;
    if (w) *w = v ? v->w : 0;
    if (h) *h = v ? v->h : 0;
    if (duration) *duration = dur;
    if (fps) {   /* nominalFrameRate: a duração típica de amostra (a do meio da faixa) */
        uint32_t sd = v && v->t->nsamples ? v->t->s[v->t->nsamples / 2].dur : 0;
        *fps = sd ? (double)v->t->timescale / sd : 0;
    }
    if (nsamples) *nsamples = v ? v->t->nsamples : 0;
}

static int slot_put(slot_t *s, const uint8_t *y, const uint8_t *u, const uint8_t *v, int sy, int suv, int w, int h, double pts,
                    const cx_h264_color *c) {
    size_t_ need = (size_t_)w * h + 2 * (size_t_)((w + 1) / 2) * ((h + 1) / 2);
    if (!s->buf || s->w != w || s->h != h) {
        uint8_t *b = realloc(s->buf, need);
        if (!b) return -1;
        s->buf = b;
    }
    int cw = (w + 1) / 2, ch = (h + 1) / 2;
    for (int r = 0; r < h; r++) memcpy(s->buf + (size_t_)r * w, y + (size_t_)r * sy, (size_t_)w);
    uint8_t *pu = s->buf + (size_t_)w * h, *pv = pu + (size_t_)cw * ch;
    for (int r = 0; r < ch; r++) { memcpy(pu + (size_t_)r * cw, u + (size_t_)r * suv, (size_t_)cw); memcpy(pv + (size_t_)r * cw, v + (size_t_)r * suv, (size_t_)cw); }
    s->w = w; s->h = h; s->pts = pts; s->col = *c; s->valid = 1;
    return 0;
}
/* os quadros chegam em ordem de exibição: vão para o fim da fila */
static void on_frame(void *ctx, const uint8_t *y, const uint8_t *u, const uint8_t *vv, int sy, int suv, int w, int h, uint64_t tag,
                     const cx_h264_color *c) {
    avv_t *v = ctx;
    if (tag >= v->t->nsamples || v->qn >= 17) return;
    double pts = mp4_pts(v->t, (uint32_t)tag);
    if (pts < v->floor_pts) return;
    if (!slot_put(&v->q[v->qn], y, u, vv, sy, suv, w, h, pts, c)) v->qn++;
}
/* tira o primeiro da fila para cur (troca os buffers, sem copiar) */
static void take(avv_t *v) {
    slot_t t = v->cur;
    v->cur = v->q[0];
    for (int i = 1; i < v->qn; i++) v->q[i - 1] = v->q[i];
    v->q[--v->qn] = t;
    v->q[v->qn].valid = 0;
}

/* entrega amostras até sair um quadro novo (ou o fim); 1 saiu quadro, 0 fim do fluxo, < 0 erro */
static int pump(avv_t *v) {
    while (!v->qn) {
        if (v->next >= v->t->nsamples) {
            if (v->flushed) return 0;
            cx_h264_flush(v->dec, on_frame, v);
            v->flushed = 1;
            return v->qn ? 1 : 0;
        }
        uint32_t i = v->next++;
        uint32_t sz = v->t->s[i].size;
        if (sz > v->sample_cap) { uint8_t *b = realloc(v->sample, sz); if (!b) return -1; v->sample = b; v->sample_cap = sz; }
        if (mp4_read_sample(&v->m, v->t, i, v->sample)) return -1;
        uint32_t need = sz * 2 + (uint32_t)v->ps_n + 16;
        if (need > v->annexb_cap) { uint8_t *b = realloc(v->annexb, need); if (!b) return -1; v->annexb = b; v->annexb_cap = need; }
        int n = 0;
        if (v->t->s[i].sync) { memcpy(v->annexb, v->ps, (size_t_)v->ps_n); n = v->ps_n; }   /* SPS/PPS antes de cada quadro-chave */
        for (uint32_t off = 0; off + (uint32_t)v->nal <= sz;) {
            uint32_t len = 0;
            for (int k = 0; k < v->nal; k++) len = len << 8 | v->sample[off + k];
            off += (uint32_t)v->nal;
            if (!len || off + len > sz) break;
            v->annexb[n++] = 0; v->annexb[n++] = 0; v->annexb[n++] = 0; v->annexb[n++] = 1;
            memcpy(v->annexb + n, v->sample + off, len);
            n += (int)len;
            off += len;
        }
        if (n) cx_h264_decode(v->dec, v->annexb, n, i, on_frame, v);   /* amostra com erro: o quadro não sai, segue */
    }
    return 1;
}

static int restart(avv_t *v, uint32_t key) {
    if (v->dec) cx_h264_close(v->dec);
    v->dec = cx_h264_open(0);
    if (!v->dec) return -1;
    v->next = key;
    v->flushed = 0;
    v->cur.valid = 0;
    v->qn = 0;
    v->floor_pts = mp4_pts(v->t, key);
    return 0;
}

int avv_frame_at(avv_t *v, double t, avv_frame *out) {
    if (!v || !out) return -1;
    double first = mp4_pts(v->t, 0);
    if (t < first) t = first;
    uint32_t key = 0;
    mp4_sample_at(v->t, t, &key);
    /* recomeça: nada decodificado, tempo antes do quadro atual, ou o quadro-chave do tempo pedido está à frente do que já
     * foi entregue ao decodificador (pular é mais barato do que decodificar até lá) */
    int have = v->dec && (v->cur.valid || v->qn);
    double here = v->cur.valid ? v->cur.pts : v->qn ? v->q[0].pts : 1e300;
    if (!have || t < here || (key > v->next && mp4_pts(v->t, key) > here)) {
        if (restart(v, key)) return -1;
    }
    for (;;) {
        if (!v->qn) {
            int r = pump(v);
            if (r < 0) return r;
            if (r == 0) break;   /* fim do fluxo: o atual fica */
        }
        if (v->q[0].pts <= t) { take(v); continue; }
        break;
    }
    slot_t *s = v->cur.valid ? &v->cur : v->qn ? &v->q[0] : 0;   /* antes do primeiro quadro: o primeiro */
    if (!s || !s->valid) return -1;
    int cw = (s->w + 1) / 2, ch = (s->h + 1) / 2;
    out->y = s->buf; out->u = s->buf + (size_t_)s->w * s->h; out->v = out->u + (size_t_)cw * ch;
    out->sy = s->w; out->suv = cw; out->w = s->w; out->h = s->h; out->pts = s->pts;
    out->full_range = s->col.full_range; out->matrix = s->col.matrix; out->primaries = s->col.primaries; out->transfer = s->col.transfer;
    return 0;
}

double avv_next_pts(avv_t *v) {
    if (!v || !v->dec || !v->cur.valid) return -1;
    if (!v->qn && pump(v) <= 0) return -1;
    return v->q[0].pts;
}

int avv_rotation(const avv_t *v) {
    if (!v) return 0;
    const int32_t *m = v->t->matrix;   /* a b u / c d v / x y w (16.16) */
    if (m[0] == 0 && m[1] > 0 && m[3] < 0 && m[4] == 0) return 90;
    if (m[0] < 0 && m[1] == 0 && m[3] == 0 && m[4] < 0) return 180;
    if (m[0] == 0 && m[1] < 0 && m[3] > 0 && m[4] == 0) return 270;
    return 0;
}

static int clamp8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
int avv_to_bgra(const avv_frame *f, int rot, int out_w, int out_h, uint8_t **dst, int *ow, int *oh) {
    if (!f || !dst) return -1;
    int w = f->w, h = f->h;
    int m = f->matrix;
    if (!(m == 1 || m == 5 || m == 6 || m == 7 || m == 9 || m == 10)) m = h >= 720 ? 1 : 6;
    double kr = 0.299, kb = 0.114;
    if (m == 1) { kr = 0.2126; kb = 0.0722; } else if (m == 9 || m == 10) { kr = 0.2627; kb = 0.0593; } else if (m == 7) { kr = 0.212; kb = 0.087; }
    double ys = f->full_range ? 1.0 : 255.0 / 219.0, cs = f->full_range ? 1.0 : 255.0 / 224.0;
    int yo = f->full_range ? 0 : 16;
    int iy = (int)(ys * 65536 + 0.5), crv = (int)(2 * (1 - kr) * cs * 65536 + 0.5), cbu = (int)(2 * (1 - kb) * cs * 65536 + 0.5);
    int cgu = (int)(2 * (1 - kb) * kb / (1 - kr - kb) * cs * 65536 + 0.5), cgv = (int)(2 * (1 - kr) * kr / (1 - kr - kb) * cs * 65536 + 0.5);
    uint8_t *full = malloc((size_t_)w * h * 4);
    if (!full) return -1;
    for (int r = 0; r < h; r++) {
        const uint8_t *py = f->y + (size_t_)r * f->sy, *pu = f->u + (size_t_)(r / 2) * f->suv, *pv = f->v + (size_t_)(r / 2) * f->suv;
        uint8_t *d = full + (size_t_)r * w * 4;
        for (int x = 0; x < w; x++) {
            int Y = (py[x] - yo) * iy + 32768, cb = pu[x / 2] - 128, cr = pv[x / 2] - 128;
            d[4 * x] = (uint8_t)clamp8((Y + cbu * cb) >> 16);
            d[4 * x + 1] = (uint8_t)clamp8((Y - cgu * cb - cgv * cr) >> 16);
            d[4 * x + 2] = (uint8_t)clamp8((Y + crv * cr) >> 16);
            d[4 * x + 3] = 255;
        }
    }
    /* rotação (no sentido horário, como o preferredTransform de vídeo gravado em retrato) */
    int rw = rot == 90 || rot == 270 ? h : w, rh = rot == 90 || rot == 270 ? w : h;
    uint8_t *rotd = full;
    if (rot == 90 || rot == 180 || rot == 270) {
        rotd = malloc((size_t_)rw * rh * 4);
        if (!rotd) { free(full); return -1; }
        for (int y = 0; y < rh; y++) for (int x = 0; x < rw; x++) {
            int sx, sy2;
            if (rot == 90) { sx = y; sy2 = h - 1 - x; }
            else if (rot == 180) { sx = w - 1 - x; sy2 = h - 1 - y; }
            else { sx = w - 1 - y; sy2 = x; }
            memcpy(rotd + ((size_t_)y * rw + x) * 4, full + ((size_t_)sy2 * w + sx) * 4, 4);
        }
        free(full);
    }
    if (out_w <= 0 || out_h <= 0 || (out_w >= rw && out_h >= rh)) { *dst = rotd; *ow = rw; *oh = rh; return 0; }
    /* redução por média de área (cada pixel de saída é a média do retângulo de origem que ele cobre) */
    uint8_t *sc = malloc((size_t_)out_w * out_h * 4);
    if (!sc) { free(rotd); return -1; }
    for (int y = 0; y < out_h; y++) {
        int y0 = (int)((long long)y * rh / out_h), y1 = (int)((long long)(y + 1) * rh / out_h);
        if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < out_w; x++) {
            int x0 = (int)((long long)x * rw / out_w), x1 = (int)((long long)(x + 1) * rw / out_w);
            if (x1 <= x0) x1 = x0 + 1;
            unsigned acc[4] = {0, 0, 0, 0}, n = 0;
            for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) {
                const uint8_t *p = rotd + ((size_t_)yy * rw + xx) * 4;
                acc[0] += p[0]; acc[1] += p[1]; acc[2] += p[2]; acc[3] += p[3]; n++;
            }
            uint8_t *d = sc + ((size_t_)y * out_w + x) * 4;
            for (int k = 0; k < 4; k++) d[k] = (uint8_t)((acc[k] + n / 2) / n);
        }
    }
    free(rotd);
    *dst = sc; *ow = out_w; *oh = out_h;
    return 0;
}
