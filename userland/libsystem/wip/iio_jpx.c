/*
 * mOS — decodificador JPEG 2000 Parte 1 (ISO/IEC 15444-1), escrito do zero, sem CoreFoundation. Serve o JPXDecode
 * do PDF (pdf_image.inc) e o public.jpeg-2000 do ImageIO.
 *  - Contêiner JP2 (ihdr, colr enumerado/ICC, pclr + cmap, cdef) ou codestream puro.
 *  - Codestream: SIZ, COD/COC, QCD/QCC, RGN, POC, PPM/PPT, tiles em várias partes; marcas SOP/EPH.
 *  - Tier-2: as 5 progressões (e as mudanças do POC), precintos, árvores de etiquetas, camadas.
 *  - Tier-1 (EBCOT): codificador aritmético MQ, passes de significância/refinamento/limpeza com corrida, e as
 *    chaves de modo (BYPASS, RESET, TERMALL, VSC, PTERM, SEGSYM).
 *  - Quantização escalar (sem, derivada, expandida), ROI por maxshift, DWT 5/3 reversível e 9/7 irreversível,
 *    transformação de cor RCT/ICT, deslocamento de nível, componentes subamostradas e com sinal, sYCC → RGB.
 * Teste: tools/iio_jpx_host.py (arquivos do openjpeg pelo PIL e por ctypes, comparados amostra a amostra).
 */
#ifdef JPX_HOST
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#else
#include "sys.h"
#include "libc.h"
double floor(double x);
#endif
#include "iio_jpx.h"

typedef unsigned char u8j;
typedef unsigned int u32j;

/* ---------------------------------------------------------------- utilidades */
static int cdiv(long a, long b) { return (int)((a + b - 1) / b); }                 /* a, b ≥ 0 */
static int cdivp(long a, int p) { return (int)((a + (1L << p) - 1) >> p); }
static int fdivp(long a, int p) { return (int)(a >> p); }
static u32j rb16(const u8j *p) { return (u32j)p[0] << 8 | p[1]; }
static u32j rb32(const u8j *p) { return (u32j)p[0] << 24 | (u32j)p[1] << 16 | (u32j)p[2] << 8 | p[3]; }
static int ilog2(int v) { int r = 0; while (v > 1) { v >>= 1; r++; } return r; }

/* ---------------------------------------------------------------- MQ (anexo C; a mesma tabela do JBIG2) */
static const struct { unsigned short qe; u8j nmps, nlps, sw; } mq_tab[47] = {
    {0x5601, 1, 1, 1}, {0x3401, 2, 6, 0}, {0x1801, 3, 9, 0}, {0x0AC1, 4, 12, 0}, {0x0521, 5, 29, 0}, {0x0221, 38, 33, 0},
    {0x5601, 7, 6, 1}, {0x5401, 8, 14, 0}, {0x4801, 9, 14, 0}, {0x3801, 10, 14, 0}, {0x3001, 11, 17, 0}, {0x2401, 12, 18, 0},
    {0x1C01, 13, 20, 0}, {0x1601, 29, 21, 0}, {0x5601, 15, 14, 1}, {0x5401, 16, 14, 0}, {0x5101, 17, 15, 0}, {0x4801, 18, 16, 0},
    {0x3801, 19, 17, 0}, {0x3401, 20, 18, 0}, {0x3001, 21, 19, 0}, {0x2801, 22, 19, 0}, {0x2401, 23, 20, 0}, {0x2201, 24, 21, 0},
    {0x1C01, 25, 22, 0}, {0x1801, 26, 23, 0}, {0x1601, 27, 24, 0}, {0x1401, 28, 25, 0}, {0x1201, 29, 26, 0}, {0x1101, 30, 27, 0},
    {0x0AC1, 31, 28, 0}, {0x09C1, 32, 29, 0}, {0x08A1, 33, 30, 0}, {0x0521, 34, 31, 0}, {0x0441, 35, 32, 0}, {0x02A1, 36, 33, 0},
    {0x0221, 37, 34, 0}, {0x0141, 38, 35, 0}, {0x0111, 39, 36, 0}, {0x0085, 40, 37, 0}, {0x0049, 41, 38, 0}, {0x0025, 42, 39, 0},
    {0x0015, 43, 40, 0}, {0x0009, 44, 41, 0}, {0x0005, 45, 42, 0}, {0x0001, 45, 43, 0}, {0x5601, 46, 46, 0}};
typedef struct { const u8j *d; size_t n, bp; u32j a, c; int ct; u8j st[19], mps[19]; } mqd;
static u8j mq_byte(const mqd *m, size_t i) { return i < m->n ? m->d[i] : 0xFF; }
static void mq_bytein(mqd *m) {
    if (mq_byte(m, m->bp) == 0xFF) {
        if (mq_byte(m, m->bp + 1) > 0x8F) { m->c += 0xFF00; m->ct = 8; }
        else { m->bp++; m->c += (u32j)mq_byte(m, m->bp) << 9; m->ct = 7; }
    } else { m->bp++; m->c += (u32j)mq_byte(m, m->bp) << 8; m->ct = 8; }
}
static void mq_init(mqd *m, const u8j *d, size_t n) {
    m->d = d; m->n = n; m->bp = 0;
    m->c = (u32j)mq_byte(m, 0) << 16;
    mq_bytein(m);
    m->c <<= 7; m->ct -= 7; m->a = 0x8000;
}
static void mq_reset(mqd *m) {
    memset(m->st, 0, sizeof m->st); memset(m->mps, 0, sizeof m->mps);
    m->st[0] = 4; m->st[17] = 3; m->st[18] = 46;
}
static int mq_dec(mqd *m, int cx) {
    int s = m->st[cx], d;
    u32j qe = mq_tab[s].qe;
    m->a -= qe;
    if ((m->c >> 16) < qe) {                       /* intervalo de baixo: troca condicional do LPS */
        if (m->a < qe) { m->a = qe; d = m->mps[cx]; m->st[cx] = mq_tab[s].nmps; }
        else { m->a = qe; d = 1 - m->mps[cx]; if (mq_tab[s].sw) m->mps[cx] = (u8j)(1 - m->mps[cx]); m->st[cx] = mq_tab[s].nlps; }
    } else {
        m->c -= qe << 16;
        if (m->a & 0x8000) return m->mps[cx];
        if (m->a < qe) { d = 1 - m->mps[cx]; if (mq_tab[s].sw) m->mps[cx] = (u8j)(1 - m->mps[cx]); m->st[cx] = mq_tab[s].nlps; }
        else { d = m->mps[cx]; m->st[cx] = mq_tab[s].nmps; }
    }
    do { if (m->ct == 0) mq_bytein(m); m->a <<= 1; m->c <<= 1; m->ct--; } while (!(m->a & 0x8000));
    return d;
}
/* modo bruto (BYPASS): bits em ordem, um 0 enfiado depois de cada 0xFF */
typedef struct { const u8j *d; size_t n, bp; u32j c; int ct; } rawd;
static void raw_init(rawd *r, const u8j *d, size_t n) { r->d = d; r->n = n; r->bp = 0; r->c = 0; r->ct = 0; }
static int raw_bit(rawd *r) {
    if (r->ct == 0) {
        int prev = r->c == 0xFF;
        r->c = r->bp < r->n ? r->d[r->bp] : 0xFF;
        r->bp++;
        r->ct = prev ? 7 : 8;
    }
    r->ct--;
    return (int)(r->c >> r->ct) & 1;
}

/* ---------------------------------------------------------------- árvore de etiquetas (B.10.2) */
typedef struct { int value, low; } ttn;
typedef struct { int w, h, nl; ttn *n; int lw[32], lh[32], off[32]; } ttree;
static int tt_init(ttree *t, int w, int h) {
    memset(t, 0, sizeof *t);
    t->w = w; t->h = h;
    int total = 0, lw = w, lh = h;
    do {
        t->lw[t->nl] = lw; t->lh[t->nl] = lh; t->off[t->nl] = total;
        total += lw * lh; t->nl++;
        if (lw == 1 && lh == 1) break;
        lw = (lw + 1) / 2; lh = (lh + 1) / 2;
    } while (t->nl < 32);
    t->n = malloc((size_t)(total ? total : 1) * sizeof(ttn));
    if (!t->n) return 0;
    for (int i = 0; i < total; i++) { t->n[i].value = 999; t->n[i].low = 0; }
    return 1;
}
typedef struct { const u8j *d; size_t n, bp; u32j buf; int ct; } bior;
static int bio_bit(bior *b) {
    if (b->ct == 0) {
        int stuffed = (b->buf & 0xFF) == 0xFF;
        b->buf = b->bp < b->n ? b->d[b->bp] : 0;
        b->bp++;
        b->ct = stuffed ? 7 : 8;
    }
    b->ct--;
    return (int)(b->buf >> b->ct) & 1;
}
static u32j bio_bits(bior *b, int n) { u32j v = 0; while (n-- > 0) v = v << 1 | (u32j)bio_bit(b); return v; }
static void bio_align(bior *b) { if ((b->buf & 0xFF) == 0xFF) { b->buf = 0; b->bp++; } b->ct = 0; }
static int tt_decode(ttree *t, bior *b, int leaf, int threshold) {
    int x = leaf % t->w, y = leaf / t->w;
    ttn *path[32];
    for (int l = 0; l < t->nl; l++) { path[l] = &t->n[t->off[l] + y * t->lw[l] + x]; x >>= 1; y >>= 1; }
    int low = 0;
    ttn *node = 0;
    for (int l = t->nl - 1; l >= 0; l--) {
        node = path[l];
        if (low > node->low) node->low = low; else low = node->low;
        while (low < threshold && low < node->value) { if (bio_bit(b)) node->value = low; else low++; }
        node->low = low;
    }
    return node && node->value < threshold;
}

/* ---------------------------------------------------------------- estruturas */
enum { CB_BYPASS = 1, CB_RESET = 2, CB_TERMALL = 4, CB_VSC = 8, CB_PTERM = 16, CB_SEGSYM = 32 };
typedef struct { int nl, xcb, ycb, cbsty, rev, csty, ppx[33], ppy[33]; } jcod;
typedef struct { int style, guard, n, ex[97], mu[97]; } jqcd;
typedef struct { int len, npasses, maxpasses, raw; } jseg;
typedef struct {
    int x0, y0, x1, y1;
    int included, numbps, lblock, npasses;
    u8j *data; int len, cap;
    jseg *seg; int nseg, segcap;
} jcblk;
typedef struct { int cw, ch, cbx0, cby0; jcblk *cb; ttree incl, zbp; } jpband;
typedef struct { jpband b[3]; } jprec;
typedef struct { int type, x0, y0, x1, y1, Mb, roishift; float step; } jband;
typedef struct {
    int x0, y0, x1, y1, ppx, ppy, pw, ph, nb, xcb, ycb;
    jband band[3];
    jprec *prec;
    int *nextl;
} jres;
typedef struct {
    int x0, y0, x1, y1;
    jcod cod; jqcd qcd; int roishift;
    jres res[33];
    int *ic; float *fc;
} jtcomp;
typedef struct { int rs, cs, lye, re, ce, prog; } jpoc;
typedef struct {
    int csiz, xsiz, ysiz, xo, yo, xt, yt, xto, yto;
    int prec[JPX_MAXC], sgnd[JPX_MAXC], dx[JPX_MAXC], dy[JPX_MAXC];
    /* COD/QCD principais e por componente (COC/QCC) */
    int prog, layers, mct, sop, eph;
    jcod cod[JPX_MAXC]; int cod_from_coc[JPX_MAXC];
    jqcd qcd[JPX_MAXC]; int qcd_from_qcc[JPX_MAXC];
    int roi[JPX_MAXC];
    jpoc poc[64]; int npoc;
    u8j *ppm; size_t ppm_n, ppm_cap;  /* cabeçalhos de pacote do PPM, por parte de tile, na ordem */
    size_t *ppm_off; size_t *ppm_len; int nppm;
} jmain;
typedef struct {
    u8j *data; size_t n, cap;      /* corpo do tile (partes concatenadas) */
    u8j *ppt; size_t ppt_n, ppt_cap;
    int has_cod, prog, layers, mct, sop, eph;
    jcod cod[JPX_MAXC]; int cod_set[JPX_MAXC], cod_from_coc[JPX_MAXC];
    jqcd qcd[JPX_MAXC]; int qcd_set[JPX_MAXC], qcd_from_qcc[JPX_MAXC];
    int roi[JPX_MAXC], roi_set[JPX_MAXC];
    jpoc poc[64]; int npoc;
    int parts;                     /* partes vistas (para o PPM) */
    int init;                      /* estado do tile já copiado do principal */
    int ppm_first; int ppm_parts;
} jtile;

static int append(u8j **buf, size_t *n, size_t *cap, const u8j *src, size_t len) {
    if (*n + len > *cap) {
        size_t c = *cap ? *cap * 2 : 4096;
        while (c < *n + len) c *= 2;
        u8j *nb = realloc(*buf, c);
        if (!nb) return 0;
        *buf = nb; *cap = c;
    }
    memcpy(*buf + *n, src, len);
    *n += len;
    return 1;
}

/* ---------------------------------------------------------------- marcadores */
static int read_spcod(const u8j *p, size_t l, jcod *c, int scod) {
    if (l < 5) return 0;
    c->nl = p[0]; c->xcb = (p[1] & 15) + 2; c->ycb = (p[2] & 15) + 2; c->cbsty = p[3]; c->rev = p[4] == 1;
    c->csty = scod;
    if (c->nl > 32 || c->xcb > 10 || c->ycb > 10 || c->xcb + c->ycb > 12) return 0;
    for (int r = 0; r <= c->nl; r++) {
        if (scod & 1) { if (l < 5 + (size_t)r + 1) return 0; c->ppx[r] = p[5 + r] & 15; c->ppy[r] = p[5 + r] >> 4; }
        else { c->ppx[r] = 15; c->ppy[r] = 15; }
    }
    return 1;
}
static int read_qcd(const u8j *p, size_t l, jqcd *q) {
    if (l < 1) return 0;
    q->style = p[0] & 31; q->guard = p[0] >> 5;
    q->n = 0;
    if (q->style == 0) { for (size_t i = 1; i < l && q->n < 97; i++) { q->ex[q->n] = p[i] >> 3; q->mu[q->n] = 0; q->n++; } }
    else { for (size_t i = 1; i + 1 < l && q->n < 97; i += 2) { u32j v = rb16(p + i); q->ex[q->n] = (int)(v >> 11); q->mu[q->n] = (int)(v & 0x7FF); q->n++; } }
    return q->n > 0;
}
static int read_poc(const u8j *p, size_t l, int csiz, jpoc *out, int *n) {
    int cb = csiz < 257 ? 1 : 2;
    size_t e = (size_t)(5 + 2 * cb);
    for (size_t i = 0; i + e <= l && *n < 64; i += e) {
        jpoc *o = &out[(*n)++];
        o->rs = p[i]; o->cs = cb == 1 ? p[i + 1] : (int)rb16(p + i + 1);
        o->lye = (int)rb16(p + i + 1 + cb); o->re = p[i + 3 + cb];
        o->ce = cb == 1 ? p[i + 4 + cb] : (int)rb16(p + i + 4 + cb);
        if (o->ce == 0) o->ce = cb == 1 ? 256 : 16384;
        o->prog = p[i + 4 + 2 * cb];
    }
    return 1;
}

/* ---------------------------------------------------------------- montagem do tile-componente */
static void free_tcomp(jtcomp *tc) {
    for (int r = 0; r <= tc->cod.nl && r < 33; r++) {
        jres *rs = &tc->res[r];
        if (!rs->prec) continue;
        for (int p = 0; p < rs->pw * rs->ph; p++)
            for (int b = 0; b < rs->nb; b++) {
                jpband *pb = &rs->prec[p].b[b];
                for (int k = 0; k < pb->cw * pb->ch; k++) { free(pb->cb[k].data); free(pb->cb[k].seg); }
                free(pb->cb); free(pb->incl.n); free(pb->zbp.n);
            }
        free(rs->prec); free(rs->nextl);
        rs->prec = 0;
    }
    free(tc->ic); free(tc->fc);
    tc->ic = 0; tc->fc = 0;
}
static int build_tcomp(jtcomp *tc, int prec, int layers) {
    jcod *c = &tc->cod;
    int nl = c->nl;
    long area = (long)(tc->x1 - tc->x0) * (tc->y1 - tc->y0);
    if (area < 0 || area > (1L << 28)) return 0;
    if (c->rev) tc->ic = calloc((size_t)(area ? area : 1), sizeof(int));
    else tc->fc = calloc((size_t)(area ? area : 1), sizeof(float));
    if (!tc->ic && !tc->fc) return 0;
    for (int r = 0; r <= nl; r++) {
        jres *rs = &tc->res[r];
        int lv = nl - r;
        rs->x0 = cdivp(tc->x0, lv); rs->y0 = cdivp(tc->y0, lv); rs->x1 = cdivp(tc->x1, lv); rs->y1 = cdivp(tc->y1, lv);
        rs->ppx = c->ppx[r]; rs->ppy = c->ppy[r];
        if (r > 0 && (rs->ppx == 0 || rs->ppy == 0)) return 0;
        rs->pw = rs->x1 > rs->x0 ? cdivp(rs->x1, rs->ppx) - fdivp(rs->x0, rs->ppx) : 0;
        rs->ph = rs->y1 > rs->y0 ? cdivp(rs->y1, rs->ppy) - fdivp(rs->y0, rs->ppy) : 0;
        if ((long)rs->pw * rs->ph > (1L << 20)) return 0;
        rs->xcb = c->xcb < rs->ppx - (r > 0) ? c->xcb : rs->ppx - (r > 0);
        rs->ycb = c->ycb < rs->ppy - (r > 0) ? c->ycb : rs->ppy - (r > 0);
        rs->nb = r == 0 ? 1 : 3;
        for (int b = 0; b < rs->nb; b++) {
            jband *bd = &rs->band[b];
            bd->type = r == 0 ? 0 : b + 1;               /* 0 LL, 1 HL, 2 LH, 3 HH */
            int xob = bd->type == 1 || bd->type == 3, yob = bd->type == 2 || bd->type == 3;
            int nb = r == 0 ? nl : nl - r + 1;
            if (r == 0) { bd->x0 = rs->x0; bd->y0 = rs->y0; bd->x1 = rs->x1; bd->y1 = rs->y1; }
            else {
                long ox = (long)xob << (nb - 1), oy = (long)yob << (nb - 1);
                bd->x0 = tc->x0 - ox <= 0 ? 0 : cdivp(tc->x0 - ox, nb); bd->x1 = tc->x1 - ox <= 0 ? 0 : cdivp(tc->x1 - ox, nb);
                bd->y0 = tc->y0 - oy <= 0 ? 0 : cdivp(tc->y0 - oy, nb); bd->y1 = tc->y1 - oy <= 0 ? 0 : cdivp(tc->y1 - oy, nb);
            }
            /* quantização da sub-banda */
            int qi = r == 0 ? 0 : 3 * (r - 1) + b + 1;
            int ex, mu;
            jqcd *q = &tc->qcd;
            if (q->style == 1) { ex = q->ex[0] - (r == 0 ? 0 : r - 1); mu = q->mu[0]; }
            else { int k = qi < q->n ? qi : q->n - 1; ex = q->ex[k]; mu = q->mu[k]; }
            bd->Mb = q->guard + ex - 1;
            bd->roishift = tc->roishift;
            int gain = bd->type == 0 ? 0 : bd->type == 3 ? 2 : 1;
            bd->step = c->rev ? 1.0f : (float)((1.0 + mu / 2048.0) * ldexp(1.0, prec + gain - ex));
        }
        rs->prec = calloc((size_t)(rs->pw * rs->ph ? rs->pw * rs->ph : 1), sizeof(jprec));
        rs->nextl = calloc((size_t)(rs->pw * rs->ph ? rs->pw * rs->ph : 1), sizeof(int));
        if (!rs->prec || !rs->nextl) return 0;
        int px0 = fdivp(rs->x0, rs->ppx), py0 = fdivp(rs->y0, rs->ppy);
        for (int pj = 0; pj < rs->ph; pj++)
            for (int pi = 0; pi < rs->pw; pi++) {
                jprec *pr = &rs->prec[pj * rs->pw + pi];
                long rx0 = (long)(px0 + pi) << rs->ppx, ry0 = (long)(py0 + pj) << rs->ppy;
                long rx1 = rx0 + (1L << rs->ppx), ry1 = ry0 + (1L << rs->ppy);
                for (int b = 0; b < rs->nb; b++) {
                    jband *bd = &rs->band[b];
                    jpband *pb = &pr->b[b];
                    long bx0 = r ? rx0 >> 1 : rx0, by0 = r ? ry0 >> 1 : ry0, bx1 = r ? rx1 >> 1 : rx1, by1 = r ? ry1 >> 1 : ry1;
                    if (bx0 < bd->x0) bx0 = bd->x0;
                    if (by0 < bd->y0) by0 = bd->y0;
                    if (bx1 > bd->x1) bx1 = bd->x1;
                    if (by1 > bd->y1) by1 = bd->y1;
                    if (bx1 <= bx0 || by1 <= by0) { pb->cw = pb->ch = 0; tt_init(&pb->incl, 1, 1); tt_init(&pb->zbp, 1, 1); continue; }
                    pb->cbx0 = fdivp(bx0, rs->xcb); pb->cby0 = fdivp(by0, rs->ycb);
                    pb->cw = cdivp(bx1, rs->xcb) - pb->cbx0;
                    pb->ch = cdivp(by1, rs->ycb) - pb->cby0;
                    pb->cb = calloc((size_t)(pb->cw * pb->ch), sizeof(jcblk));
                    if (!pb->cb || !tt_init(&pb->incl, pb->cw, pb->ch) || !tt_init(&pb->zbp, pb->cw, pb->ch)) return 0;
                    for (int j = 0; j < pb->ch; j++)
                        for (int i = 0; i < pb->cw; i++) {
                            jcblk *cb = &pb->cb[j * pb->cw + i];
                            long cx0 = (long)(pb->cbx0 + i) << rs->xcb, cy0 = (long)(pb->cby0 + j) << rs->ycb;
                            long cx1 = cx0 + (1L << rs->xcb), cy1 = cy0 + (1L << rs->ycb);
                            cb->x0 = (int)(cx0 < bx0 ? bx0 : cx0); cb->y0 = (int)(cy0 < by0 ? by0 : cy0);
                            cb->x1 = (int)(cx1 > bx1 ? bx1 : cx1); cb->y1 = (int)(cy1 > by1 ? by1 : cy1);
                            cb->lblock = 3;
                        }
                }
            }
    }
    (void)layers;
    return 1;
}

/* ---------------------------------------------------------------- tier-2: um pacote */
static int seg_max(int cbsty, int i, int *raw) {
    *raw = 0;
    if (cbsty & CB_TERMALL) { *raw = (cbsty & CB_BYPASS) && i >= 10 && (i - 10) % 3 != 2; return 1; }
    if (cbsty & CB_BYPASS) {
        if (i < 10) return 10 - i;
        int k = (i - 10) % 3;
        if (k == 2) return 1;
        *raw = 1;
        return 2 - k;
    }
    return 1 << 20;
}
typedef struct { jcblk *cb; int len; } jpend;
typedef struct { jpend *v; int n, cap; } jpends;
static int decode_packet(jtcomp *tc, int r, int p, int l, bior *hdr, const u8j *body, size_t bn, size_t *bpos, int sop, int eph, int sephdr, jpends *pq) {
    jres *rs = &tc->res[r];
    jprec *pr = &rs->prec[p];
    if (sop && *bpos + 6 <= bn && body[*bpos] == 0xFF && body[*bpos + 1] == 0x91) *bpos += 6;
    if (!sephdr) { hdr->d = body; hdr->n = bn; hdr->bp = *bpos; hdr->ct = 0; hdr->buf = 0; }
    int present = bio_bit(hdr);
    pq->n = 0;
    if (present) {
        for (int b = 0; b < rs->nb; b++) {
            jpband *pb = &pr->b[b];
            jband *bd = &rs->band[b];
            for (int k = 0; k < pb->cw * pb->ch; k++) {
                jcblk *cb = &pb->cb[k];
                int inc;
                if (!cb->included) inc = tt_decode(&pb->incl, hdr, k, l + 1);
                else inc = bio_bit(hdr);
                if (!inc) continue;
                if (!cb->included) {
                    int i = 1;
                    while (!tt_decode(&pb->zbp, hdr, k, i)) { if (++i > 74) return 0; }
                    cb->numbps = bd->Mb + 1 - i;
                    cb->included = 1;
                }
                int n;
                if (!bio_bit(hdr)) n = 1;
                else if (!bio_bit(hdr)) n = 2;
                else {
                    int v = (int)bio_bits(hdr, 2);
                    if (v < 3) n = 3 + v;
                    else { v = (int)bio_bits(hdr, 5); n = v < 31 ? 6 + v : 37 + (int)bio_bits(hdr, 7); }
                }
                while (bio_bit(hdr)) cb->lblock++;
                int cbsty = tc->cod.cbsty;
                while (n > 0) {
                    jseg *sg = cb->nseg ? &cb->seg[cb->nseg - 1] : 0;
                    if (!sg || sg->npasses >= sg->maxpasses) {
                        if (cb->nseg == cb->segcap) {
                            int nc = cb->segcap ? cb->segcap * 2 : 4;
                            jseg *ns = realloc(cb->seg, (size_t)nc * sizeof *ns);
                            if (!ns) return 0;
                            cb->seg = ns; cb->segcap = nc;
                        }
                        sg = &cb->seg[cb->nseg++];
                        sg->len = 0; sg->npasses = 0;
                        sg->maxpasses = seg_max(cbsty, cb->npasses, &sg->raw);
                    }
                    int take = sg->maxpasses - sg->npasses;
                    if (take > n) take = n;
                    int len = (int)bio_bits(hdr, cb->lblock + ilog2(take));
                    sg->npasses += take; cb->npasses += take; n -= take;
                    if (pq->n == pq->cap) {
                        int nc = pq->cap ? pq->cap * 2 : 64;
                        jpend *nv = realloc(pq->v, (size_t)nc * sizeof *nv);
                        if (!nv) return 0;
                        pq->v = nv; pq->cap = nc;
                    }
                    pq->v[pq->n].cb = cb; pq->v[pq->n].len = len; pq->n++;
                    sg->len += len;
                }
            }
        }
    }
    bio_align(hdr);
    if (eph && hdr->bp + 2 <= hdr->n && hdr->d[hdr->bp] == 0xFF && hdr->d[hdr->bp + 1] == 0x92) hdr->bp += 2;
    if (!sephdr) *bpos = hdr->bp;
    for (int i = 0; i < pq->n; i++) {
        jpend *pend = pq->v;
        size_t len = (size_t)pend[i].len;
        if (*bpos + len > bn) len = *bpos < bn ? bn - *bpos : 0;
        jcblk *cb = pend[i].cb;
        if (len) {
            size_t n = (size_t)cb->len, cap = (size_t)cb->cap;
            if (!append(&cb->data, &n, &cap, body + *bpos, len)) return 0;
            cb->len = (int)n; cb->cap = (int)cap;
        }
        if ((size_t)pend[i].len > len) {
            /* dados cortados: o segmento fica com o que veio */
            jseg *last = &cb->seg[cb->nseg - 1];
            last->len -= pend[i].len - (int)len;
        }
        *bpos += len;
    }
    return 1;
}

/* ---------------------------------------------------------------- tier-1 (anexo D) */
enum { F_SIG = 1, F_NEG = 2, F_VIS = 4, F_REF = 8 };
typedef struct { int w, h, stride; unsigned short *f; int *v; int band, vsc; } t1s;
static int sig_ctx(t1s *t, int x, int y) {
    unsigned short *f = &t->f[(y + 1) * t->stride + x + 1];
    int s = t->stride;
    int below = !(t->vsc && (y & 3) == 3);
    int h = (f[-1] & F_SIG) + (f[1] & F_SIG);
    int v = (f[-s] & F_SIG) + (below ? (f[s] & F_SIG) : 0);
    int d = (f[-s - 1] & F_SIG) + (f[-s + 1] & F_SIG) + (below ? (f[s - 1] & F_SIG) + (f[s + 1] & F_SIG) : 0);
    if (t->band == 3) {
        int hv = h + v;
        if (d >= 3) return 8;
        if (d == 2) return hv >= 1 ? 7 : 6;
        if (d == 1) return hv >= 2 ? 5 : hv == 1 ? 4 : 3;
        return hv >= 2 ? 2 : hv;
    }
    if (t->band == 1) { int tmp = h; h = v; v = tmp; }
    if (h == 2) return 8;
    if (h == 1) return v >= 1 ? 7 : d >= 1 ? 6 : 5;
    if (v == 2) return 4;
    if (v == 1) return 3;
    return d >= 2 ? 2 : d;
}
static int sign_ctx(t1s *t, int x, int y, int *xr) {
    unsigned short *f = &t->f[(y + 1) * t->stride + x + 1];
    int s = t->stride;
    int below = !(t->vsc && (y & 3) == 3);
#define CONTRIB(q) (((q) & F_SIG) ? (((q) & F_NEG) ? -1 : 1) : 0)
    int h = CONTRIB(f[-1]) + CONTRIB(f[1]);
    int v = CONTRIB(f[-s]) + (below ? CONTRIB(f[s]) : 0);
#undef CONTRIB
    if (h > 1) h = 1; if (h < -1) h = -1;
    if (v > 1) v = 1; if (v < -1) v = -1;
    static const int ctx[3][3] = {{13, 12, 11}, {10, 9, 10}, {11, 12, 13}};
    static const int xb[3][3] = {{1, 1, 1}, {1, 0, 0}, {0, 0, 0}};
    *xr = xb[h + 1][v + 1];
    return ctx[h + 1][v + 1];
}
static int mag_ctx(t1s *t, int x, int y) {
    unsigned short *f = &t->f[(y + 1) * t->stride + x + 1];
    if (*f & F_REF) return 16;
    int s = t->stride;
    int below = !(t->vsc && (y & 3) == 3);
    int any = (f[-1] | f[1] | f[-s] | f[-s - 1] | f[-s + 1] | (below ? (f[s] | f[s - 1] | f[s + 1]) : 0)) & F_SIG;
    return any ? 15 : 14;
}
static void t1_sigdec(t1s *t, mqd *m, rawd *rw, int x, int y, int one) {
    unsigned short *f = &t->f[(y + 1) * t->stride + x + 1];
    int bit;
    if (rw) bit = raw_bit(rw);
    else bit = mq_dec(m, sig_ctx(t, x, y));
    if (bit) {
        int neg;
        if (rw) neg = raw_bit(rw);
        else { int xr, cx = sign_ctx(t, x, y, &xr); neg = mq_dec(m, cx) ^ xr; }
        t->v[y * t->w + x] = one | (one >> 1);
        *f |= F_SIG | (neg ? F_NEG : 0);
    }
    *f |= F_VIS;
}
static void t1_spp(t1s *t, mqd *m, rawd *rw, int one) {
    for (int y0 = 0; y0 < t->h; y0 += 4)
        for (int x = 0; x < t->w; x++)
            for (int y = y0; y < y0 + 4 && y < t->h; y++) {
                unsigned short *f = &t->f[(y + 1) * t->stride + x + 1];
                if (*f & F_SIG) continue;
                if (!sig_ctx(t, x, y)) continue;
                t1_sigdec(t, m, rw, x, y, one);
            }
}
static void t1_mrp(t1s *t, mqd *m, rawd *rw, int one) {
    int half = one >> 1;
    for (int y0 = 0; y0 < t->h; y0 += 4)
        for (int x = 0; x < t->w; x++)
            for (int y = y0; y < y0 + 4 && y < t->h; y++) {
                unsigned short *f = &t->f[(y + 1) * t->stride + x + 1];
                if ((*f & (F_SIG | F_VIS)) != F_SIG) continue;
                int bit = rw ? raw_bit(rw) : mq_dec(m, mag_ctx(t, x, y));
                int *v = &t->v[y * t->w + x];
                *v += bit ? half : -half;
                *f |= F_REF;
            }
}
static void t1_cup(t1s *t, mqd *m, int one, int segsym) {
    for (int y0 = 0; y0 < t->h; y0 += 4)
        for (int x = 0; x < t->w; x++) {
            int y = y0;
            /* corrida: 4 linhas, nenhuma vista nem significativa e todos os contextos zero */
            if (y0 + 4 <= t->h) {
                int ok = 1;
                for (int k = 0; k < 4 && ok; k++) {
                    unsigned short *f = &t->f[(y0 + k + 1) * t->stride + x + 1];
                    if ((*f & (F_SIG | F_VIS)) || sig_ctx(t, x, y0 + k)) ok = 0;
                }
                if (ok) {
                    if (!mq_dec(m, 17)) continue;
                    int pos = mq_dec(m, 18) << 1;
                    pos |= mq_dec(m, 18);
                    y = y0 + pos;
                    /* este fica significativo sem decodificar a significância */
                    unsigned short *f = &t->f[(y + 1) * t->stride + x + 1];
                    int xr, cx = sign_ctx(t, x, y, &xr);
                    int neg = mq_dec(m, cx) ^ xr;
                    t->v[y * t->w + x] = one | (one >> 1);
                    *f |= F_SIG | (neg ? F_NEG : 0);
                    y++;
                }
            }
            for (; y < y0 + 4 && y < t->h; y++) {
                unsigned short *f = &t->f[(y + 1) * t->stride + x + 1];
                if (*f & (F_SIG | F_VIS)) continue;
                t1_sigdec(t, m, 0, x, y, one);
            }
        }
    /* limpa as marcas de "visto" */
    for (int y = 0; y < t->h; y++) for (int x = 0; x < t->w; x++) t->f[(y + 1) * t->stride + x + 1] &= (unsigned short)~F_VIS;
    if (segsym) { for (int i = 0; i < 4; i++) (void)mq_dec(m, 18); }
}
static void decode_cblk(jtcomp *tc, jres *rs, jband *bd, jcblk *cb) {
    int w = cb->x1 - cb->x0, h = cb->y1 - cb->y0;
    if (w <= 0 || h <= 0 || cb->numbps <= 0 || !cb->nseg) return;
    t1s t;
    t.w = w; t.h = h; t.stride = w + 2; t.band = bd->type; t.vsc = (tc->cod.cbsty & CB_VSC) != 0;
    t.f = calloc((size_t)(w + 2) * (size_t)(h + 2), sizeof *t.f);
    t.v = calloc((size_t)w * (size_t)h, sizeof *t.v);
    if (!t.f || !t.v) { free(t.f); free(t.v); return; }
    mqd m;
    mq_reset(&m);
    int cbsty = tc->cod.cbsty;
    int planes = cb->numbps + bd->roishift;
    if (planes > 30) planes = 30;
    int pass = 0, off = 0;
    for (int s = 0; s < cb->nseg; s++) {
        jseg *sg = &cb->seg[s];
        if (off + sg->len > cb->len) break;
        const u8j *d = cb->data + off;
        rawd rw;
        if (sg->raw) raw_init(&rw, d, (size_t)sg->len); else mq_init(&m, d, (size_t)sg->len);
        for (int k = 0; k < sg->npasses; k++, pass++) {
            int type = pass == 0 ? 2 : (pass - 1) % 3;   /* 0 SPP, 1 MRP, 2 CUP */
            int p = planes - 1 - (pass + 2) / 3;
            if (p < 0) break;
            int one = 1 << (p + 1);
            if (type == 0) t1_spp(&t, sg->raw ? 0 : &m, sg->raw ? &rw : 0, one);
            else if (type == 1) t1_mrp(&t, sg->raw ? 0 : &m, sg->raw ? &rw : 0, one);
            else t1_cup(&t, &m, one, (cbsty & CB_SEGSYM) != 0);
            if (cbsty & CB_RESET) mq_reset(&m);
        }
        off += sg->len;
    }
    /* coeficientes no lugar da sub-banda no tile-componente */
    int tw = tc->x1 - tc->x0;
    int ox = cb->x0 - bd->x0, oy = cb->y0 - bd->y0;
    if (bd->type == 1 || bd->type == 3) ox += tc->res[(int)(rs - tc->res) - 1].x1 - tc->res[(int)(rs - tc->res) - 1].x0;
    if (bd->type == 2 || bd->type == 3) oy += tc->res[(int)(rs - tc->res) - 1].y1 - tc->res[(int)(rs - tc->res) - 1].y0;
    long rthresh = bd->roishift ? 1L << (bd->roishift + 1) : 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            long v = t.v[y * w + x];
            if (rthresh && v >= rthresh) v >>= bd->roishift;
            int neg = t.f[(y + 1) * t.stride + x + 1] & F_NEG;
            size_t at = (size_t)(oy + y) * (size_t)tw + (size_t)(ox + x);
            if (tc->ic) { int q = (int)(v / 2); tc->ic[at] = neg ? -q : q; }
            else { float q = (float)v * bd->step * 0.5f; tc->fc[at] = neg ? -q : q; }
        }
    free(t.f); free(t.v);
}

/* ---------------------------------------------------------------- DWT inversa (anexo F) */
#define PADX 4
static void ext_i(int *b, int n) {                 /* extensão simétrica (periódica) dos dois lados */
    for (int k = 1; k <= PADX; k++) {
        int i = -k, j = n - 1 + k;
        int pi = i, pj = j;
        if (n == 1) { pi = 0; pj = 0; }
        else {
            int per = 2 * (n - 1);
            pi = ((pi % per) + per) % per; if (pi >= n) pi = per - pi;
            pj = ((pj % per) + per) % per; if (pj >= n) pj = per - pj;
        }
        b[PADX + i] = b[PADX + pi]; b[PADX + j] = b[PADX + pj];
    }
}
static void ext_f(float *b, int n) {
    for (int k = 1; k <= PADX; k++) {
        int i = -k, j = n - 1 + k, pi, pj;
        if (n == 1) { pi = 0; pj = 0; }
        else {
            int per = 2 * (n - 1);
            pi = ((i % per) + per) % per; if (pi >= n) pi = per - pi;
            pj = ((j % per) + per) % per; if (pj >= n) pj = per - pj;
        }
        b[PADX + i] = b[PADX + pi]; b[PADX + j] = b[PADX + pj];
    }
}
static int fl4(int a) { return a >= 0 ? a / 4 : -((-a + 3) / 4); }
static int fl2(int a) { return a >= 0 ? a / 2 : -((-a + 1) / 2); }
/* uma linha: entrada com as baixas primeiro e as altas depois; u0 = coordenada absoluta da 1ª amostra (a paridade
 * diz quem é baixa: (u0 + k) par) */
static void idwt53_1d(int *line, int n, int u0, int *buf) {
    int nl = (u0 + n + 1) / 2 - (u0 + 1) / 2;
    int *x = buf + PADX;
    int li = 0, hi = nl;
    for (int k = 0; k < n; k++) x[k] = ((u0 + k) & 1) ? line[hi++] : line[li++];
    if (n == 1) { line[0] = (u0 & 1) ? x[0] / 2 : x[0]; return; }
    ext_i(buf, n);
    for (int k = 0; k < n; k++) if (!((u0 + k) & 1)) x[k] -= fl4(x[k - 1] + x[k + 1] + 2);
    ext_i(buf, n);
    for (int k = 0; k < n; k++) if ((u0 + k) & 1) x[k] += fl2(x[k - 1] + x[k + 1]);
    memcpy(line, x, (size_t)n * sizeof *x);
}
static void idwt97_1d(float *line, int n, int u0, float *buf) {
    int nl = (u0 + n + 1) / 2 - (u0 + 1) / 2;
    float *x = buf + PADX;
    int li = 0, hi = nl;
    for (int k = 0; k < n; k++) x[k] = ((u0 + k) & 1) ? line[hi++] : line[li++];
    if (n == 1) { line[0] = (u0 & 1) ? x[0] / 2 : x[0]; return; }
    const float K = 1.230174104914001f, K2 = 1.0f / 1.230174104914001f;   /* T.800 F.3.8.2: altas × 1/K */
    const float al = -1.586134342059924f, be = -0.052980118572961f, ga = 0.882911075530934f, de = 0.443506852043971f;
    for (int k = 0; k < n; k++) x[k] *= ((u0 + k) & 1) ? K2 : K;
    ext_f(buf, n);
    for (int k = 0; k < n; k++) if (!((u0 + k) & 1)) x[k] -= de * (x[k - 1] + x[k + 1]);
    ext_f(buf, n);
    for (int k = 0; k < n; k++) if ((u0 + k) & 1) x[k] -= ga * (x[k - 1] + x[k + 1]);
    ext_f(buf, n);
    for (int k = 0; k < n; k++) if (!((u0 + k) & 1)) x[k] -= be * (x[k - 1] + x[k + 1]);
    ext_f(buf, n);
    for (int k = 0; k < n; k++) if ((u0 + k) & 1) x[k] -= al * (x[k - 1] + x[k + 1]);
    memcpy(line, x, (size_t)n * sizeof *x);
}
static int idwt(jtcomp *tc) {
    int tw = tc->x1 - tc->x0;
    int maxn = 0;
    for (int r = 0; r <= tc->cod.nl; r++) {
        int a = tc->res[r].x1 - tc->res[r].x0, b = tc->res[r].y1 - tc->res[r].y0;
        if (a > maxn) maxn = a;
        if (b > maxn) maxn = b;
    }
    if (maxn <= 0) return 1;
    void *buf = malloc((size_t)(maxn + 2 * PADX) * sizeof(float) * 2), *line = malloc((size_t)maxn * sizeof(float) * 2);
    if (!buf || !line) { free(buf); free(line); return 0; }
    for (int r = 1; r <= tc->cod.nl; r++) {
        jres *rs = &tc->res[r];
        int rw = rs->x1 - rs->x0, rh = rs->y1 - rs->y0;
        if (rw <= 0 || rh <= 0) continue;
        for (int y = 0; y < rh; y++) {
            if (tc->ic) { int *row = tc->ic + (size_t)y * tw; memcpy(line, row, (size_t)rw * sizeof(int)); idwt53_1d(line, rw, rs->x0, buf); memcpy(row, line, (size_t)rw * sizeof(int)); }
            else { float *row = tc->fc + (size_t)y * tw; memcpy(line, row, (size_t)rw * sizeof(float)); idwt97_1d(line, rw, rs->x0, buf); memcpy(row, line, (size_t)rw * sizeof(float)); }
        }
        for (int x = 0; x < rw; x++) {
            if (tc->ic) {
                int *l = line;
                for (int y = 0; y < rh; y++) l[y] = tc->ic[(size_t)y * tw + x];
                idwt53_1d(l, rh, rs->y0, buf);
                for (int y = 0; y < rh; y++) tc->ic[(size_t)y * tw + x] = l[y];
            } else {
                float *l = line;
                for (int y = 0; y < rh; y++) l[y] = tc->fc[(size_t)y * tw + x];
                idwt97_1d(l, rh, rs->y0, buf);
                for (int y = 0; y < rh; y++) tc->fc[(size_t)y * tw + x] = l[y];
            }
        }
    }
    free(buf); free(line);
    return 1;
}

/* ---------------------------------------------------------------- iterador de pacotes (B.12) */
typedef struct {
    jmain *mh; jtile *tl; jtcomp *tc;
    int tx0, ty0, tx1, ty1;
    const u8j *body; size_t bn, bpos;
    bior hdr; int sephdr;
    int sop, eph;
    jpends pq;
    u8j *join;
} jctx;
static void emit(jctx *k, int l, int r, int c, int p) {
    jtcomp *tc = &k->tc[c];
    if (r > tc->cod.nl) return;
    jres *rs = &tc->res[r];
    if (p < 0 || p >= rs->pw * rs->ph) return;
    if (rs->nextl[p] != l) return;
    rs->nextl[p]++;
    decode_packet(tc, r, p, l, &k->hdr, k->body, k->bn, &k->bpos, k->sop, k->eph, k->sephdr, &k->pq);
}
/* o precinto de (c, r) cujo canto está em (x, y) da grade de referência, ou −1 */
static int prec_at(jctx *k, int c, int r, int x, int y) {
    jtcomp *tc = &k->tc[c];
    if (r > tc->cod.nl) return -1;
    jres *rs = &tc->res[r];
    int lv = tc->cod.nl - r;
    if (rs->x0 == rs->x1 || rs->y0 == rs->y1) return -1;
    long dx = (long)k->mh->dx[c], dy = (long)k->mh->dy[c];
    int rpx = rs->ppx + lv, rpy = rs->ppy + lv;
    if (rpx > 30 || rpy > 30) return -1;
    if (!((y % (dy << rpy) == 0) || (y == k->ty0 && (((long)rs->y0 << lv) % (1L << rpy))))) return -1;
    if (!((x % (dx << rpx) == 0) || (x == k->tx0 && (((long)rs->x0 << lv) % (1L << rpx))))) return -1;
    int pi = fdivp(cdiv(x, dx << lv), rs->ppx) - fdivp(rs->x0, rs->ppx);
    int pj = fdivp(cdiv(y, dy << lv), rs->ppy) - fdivp(rs->y0, rs->ppy);
    if (pi < 0 || pj < 0 || pi >= rs->pw || pj >= rs->ph) return -1;
    return pj * rs->pw + pi;
}
static void progression(jctx *k, int prog, int l1, int r0, int r1, int c0, int c1) {
    int nc = k->mh->csiz;
    if (c1 > nc) c1 = nc;
    int maxr = 0;
    for (int c = c0; c < c1; c++) if (k->tc[c].cod.nl + 1 > maxr) maxr = k->tc[c].cod.nl + 1;
    if (r1 > maxr) r1 = maxr;
    if (prog == 0) {                                         /* LRCP */
        for (int l = 0; l < l1; l++) for (int r = r0; r < r1; r++) for (int c = c0; c < c1; c++) {
            if (r > k->tc[c].cod.nl) continue;
            jres *rs = &k->tc[c].res[r];
            for (int p = 0; p < rs->pw * rs->ph; p++) emit(k, l, r, c, p);
        }
        return;
    }
    if (prog == 1) {                                         /* RLCP */
        for (int r = r0; r < r1; r++) for (int l = 0; l < l1; l++) for (int c = c0; c < c1; c++) {
            if (r > k->tc[c].cod.nl) continue;
            jres *rs = &k->tc[c].res[r];
            for (int p = 0; p < rs->pw * rs->ph; p++) emit(k, l, r, c, p);
        }
        return;
    }
    /* progressões por posição: passos mínimos sobre componentes e resoluções */
    long xs = 1L << 30, ys = 1L << 30;
    for (int c = c0; c < c1; c++)
        for (int r = 0; r <= k->tc[c].cod.nl; r++) {
            int lv = k->tc[c].cod.nl - r;
            long a = (long)k->mh->dx[c] << (k->tc[c].res[r].ppx + lv), b = (long)k->mh->dy[c] << (k->tc[c].res[r].ppy + lv);
            if (a > 0 && a < xs) xs = a;
            if (b > 0 && b < ys) ys = b;
        }
#define FOR_Y for (long y = k->ty0; y < k->ty1; y += ys - (y % ys))
#define FOR_X for (long x = k->tx0; x < k->tx1; x += xs - (x % xs))
    if (prog == 2) {                                         /* RPCL */
        for (int r = r0; r < r1; r++) FOR_Y FOR_X for (int c = c0; c < c1; c++) {
            int p = prec_at(k, c, r, (int)x, (int)y);
            if (p >= 0) for (int l = 0; l < l1; l++) emit(k, l, r, c, p);
        }
    } else if (prog == 3) {                                  /* PCRL */
        FOR_Y FOR_X for (int c = c0; c < c1; c++) for (int r = r0; r < r1; r++) {
            int p = prec_at(k, c, r, (int)x, (int)y);
            if (p >= 0) for (int l = 0; l < l1; l++) emit(k, l, r, c, p);
        }
    } else if (prog == 4) {                                  /* CPRL */
        for (int c = c0; c < c1; c++) FOR_Y FOR_X for (int r = r0; r < r1; r++) {
            int p = prec_at(k, c, r, (int)x, (int)y);
            if (p >= 0) for (int l = 0; l < l1; l++) emit(k, l, r, c, p);
        }
    }
#undef FOR_Y
#undef FOR_X
}

/* arredondamento ao inteiro mais próximo, empate para o par (o lrintf do modo padrão, como o openjpeg) */
static long rint_even(float f) {
    double r = floor((double)f), d = (double)f - r;
    long v = (long)r;
    if (d > 0.5 || (d == 0.5 && (v & 1))) v++;
    return v;
}
/* ---------------------------------------------------------------- tile inteiro */
static int decode_tile(jmain *mh, jtile *tl, int tno, int **planes, int cw[], int ch[], int cx0[], int cy0[]) {
    int ntx = cdiv(mh->xsiz - mh->xto, mh->xt);
    int p = tno % ntx, q = tno / ntx;
    int tx0 = mh->xto + p * mh->xt, ty0 = mh->yto + q * mh->yt;
    int tx1 = tx0 + mh->xt, ty1 = ty0 + mh->yt;
    if (tx0 < mh->xo) tx0 = mh->xo;
    if (ty0 < mh->yo) ty0 = mh->yo;
    if (tx1 > mh->xsiz) tx1 = mh->xsiz;
    if (ty1 > mh->ysiz) ty1 = mh->ysiz;
    int nc = mh->csiz;
    jtcomp *tc = calloc((size_t)nc, sizeof *tc);
    if (!tc) return 0;
    int prog = tl->prog, layers = tl->layers, mct = tl->mct;
    int ok = 1;
    for (int c = 0; c < nc && ok; c++) {
        jtcomp *t = &tc[c];
        t->x0 = cdiv(tx0, mh->dx[c]); t->y0 = cdiv(ty0, mh->dy[c]); t->x1 = cdiv(tx1, mh->dx[c]); t->y1 = cdiv(ty1, mh->dy[c]);
        /* o tile já tem a precedência resolvida: COC do tile > COD do tile > COC principal > COD principal */
        t->cod = tl->cod[c];
        t->qcd = tl->qcd[c];
        t->roishift = tl->roi[c];
        ok = build_tcomp(t, mh->prec[c], layers);
    }
    if (ok) {
        jctx k;
        memset(&k, 0, sizeof k);
        k.mh = mh; k.tl = tl; k.tc = tc;
        k.tx0 = tx0; k.ty0 = ty0; k.tx1 = tx1; k.ty1 = ty1;
        k.body = tl->data; k.bn = tl->n;
        k.sop = tl->sop;
        k.eph = tl->eph;
        if (tl->ppt) { k.sephdr = 1; k.hdr.d = tl->ppt; k.hdr.n = tl->ppt_n; }
        else if (mh->ppm && tl->ppm_parts) {
            /* os cabeçalhos das partes deste tile, juntos */
            u8j *join = 0; size_t jn = 0, jcap = 0;
            for (int i = 0; i < tl->ppm_parts; i++) {
                int at = tl->ppm_first + i;
                if (at < mh->nppm) append(&join, &jn, &jcap, mh->ppm + mh->ppm_off[at], mh->ppm_len[at]);
            }
            k.sephdr = 1; k.hdr.d = join; k.hdr.n = jn;
            k.join = join;
        }
        int npoc = tl->npoc ? tl->npoc : mh->npoc;
        jpoc *poc = tl->npoc ? tl->poc : mh->poc;
        if (npoc) {
            for (int i = 0; i < npoc; i++)
                progression(&k, poc[i].prog, poc[i].lye < layers ? poc[i].lye : layers, poc[i].rs, poc[i].re, poc[i].cs, poc[i].ce);
        } else progression(&k, prog, layers, 0, 33, 0, nc);
        /* tier-1 e DWT */
        for (int c = 0; c < nc; c++) {
            jtcomp *t = &tc[c];
            for (int r = 0; r <= t->cod.nl; r++) {
                jres *rs = &t->res[r];
                for (int pp = 0; pp < rs->pw * rs->ph; pp++)
                    for (int b = 0; b < rs->nb; b++) {
                        jpband *pb = &rs->prec[pp].b[b];
                        for (int i = 0; i < pb->cw * pb->ch; i++) decode_cblk(t, rs, &rs->band[b], &pb->cb[i]);
                    }
            }
            idwt(t);
        }
        free(k.pq.v);
        free(k.join);
        /* transformação de cor */
        if (mct && nc >= 3 && tc[0].x1 - tc[0].x0 == tc[1].x1 - tc[1].x0 && tc[0].x1 - tc[0].x0 == tc[2].x1 - tc[2].x0 &&
            tc[0].y1 - tc[0].y0 == tc[1].y1 - tc[1].y0 && tc[0].y1 - tc[0].y0 == tc[2].y1 - tc[2].y0) {
            size_t a = (size_t)(tc[0].x1 - tc[0].x0) * (size_t)(tc[0].y1 - tc[0].y0);
            if (tc[0].ic && tc[1].ic && tc[2].ic)
                for (size_t i = 0; i < a; i++) {
                    int y = tc[0].ic[i], u = tc[1].ic[i], v = tc[2].ic[i];
                    int g = y - (int)floor((u + v) / 4.0), r = v + g, b = u + g;
                    tc[0].ic[i] = r; tc[1].ic[i] = g; tc[2].ic[i] = b;
                }
            else if (tc[0].fc && tc[1].fc && tc[2].fc)
                for (size_t i = 0; i < a; i++) {
                    float y = tc[0].fc[i], u = tc[1].fc[i], v = tc[2].fc[i];
                    tc[0].fc[i] = y + 1.402f * v; tc[1].fc[i] = y - 0.34413f * u - 0.71414f * v; tc[2].fc[i] = y + 1.772f * u;
                }
        }
        /* deslocamento de nível e recorte no plano da componente */
        for (int c = 0; c < nc; c++) {
            jtcomp *t = &tc[c];
            int w = t->x1 - t->x0, h = t->y1 - t->y0;
            int pr = mh->prec[c];
            long shift = mh->sgnd[c] ? 0 : 1L << (pr - 1);
            long lo = mh->sgnd[c] ? -(1L << (pr - 1)) : 0, hi = mh->sgnd[c] ? (1L << (pr - 1)) - 1 : (1L << pr) - 1;
            for (int y = 0; y < h; y++) {
                int py = t->y0 + y - cy0[c];
                if (py < 0 || py >= ch[c]) continue;
                for (int x = 0; x < w; x++) {
                    int px = t->x0 + x - cx0[c];
                    if (px < 0 || px >= cw[c]) continue;
                    long v;
                    if (t->ic) v = t->ic[(size_t)y * w + x] + shift;
                    else v = rint_even(t->fc[(size_t)y * w + x]) + shift;
                    if (v < lo) v = lo;
                    if (v > hi) v = hi;
                    planes[c][(size_t)py * cw[c] + px] = (int)v;
                }
            }
        }
    }
    for (int c = 0; c < nc; c++) free_tcomp(&tc[c]);
    free(tc);
    return ok;
}

/* ---------------------------------------------------------------- codestream */
static int parse_marker_common(jmain *mh, int csiz, u32j mk, const u8j *p, size_t l, jcod *cod, int *codset, int *from_coc,
                               jqcd *qcd, int *qset, int *from_qcc, int *roi, int *roiset, int *prog, int *layers, int *mct,
                               int *sop, int *eph, int *has_cod, jpoc *poc, int *npoc) {
    int cb = csiz < 257 ? 1 : 2;
    (void)mh;
    if (mk == 0xFF52) {                                           /* COD */
        if (l < 5) return 0;
        int scod = p[0];
        *prog = p[1]; *layers = (int)rb16(p + 2); *mct = p[4];
        *sop = (scod & 2) != 0; *eph = (scod & 4) != 0;
        jcod c;
        if (!read_spcod(p + 5, l - 5, &c, scod)) return 0;
        for (int i = 0; i < csiz; i++) if (!from_coc[i]) { cod[i] = c; if (codset) codset[i] = 1; }
        if (has_cod) *has_cod = 1;
    } else if (mk == 0xFF53) {                                    /* COC */
        if (l < (size_t)(cb + 1)) return 0;
        int ci = cb == 1 ? p[0] : (int)rb16(p);
        if (ci >= csiz) return 1;
        jcod c;
        if (!read_spcod(p + cb + 1, l - (size_t)cb - 1, &c, p[cb])) return 0;
        cod[ci] = c; from_coc[ci] = 1;
        if (codset) codset[ci] = 1;
    } else if (mk == 0xFF5C) {                                    /* QCD */
        jqcd q;
        if (!read_qcd(p, l, &q)) return 0;
        for (int i = 0; i < csiz; i++) if (!from_qcc[i]) { qcd[i] = q; if (qset) qset[i] = 1; }
    } else if (mk == 0xFF5D) {                                    /* QCC */
        if (l < (size_t)cb + 1) return 0;
        int ci = cb == 1 ? p[0] : (int)rb16(p);
        if (ci >= csiz) return 1;
        jqcd q;
        if (!read_qcd(p + cb, l - (size_t)cb, &q)) return 0;
        qcd[ci] = q; from_qcc[ci] = 1;
        if (qset) qset[ci] = 1;
    } else if (mk == 0xFF5E) {                                    /* RGN */
        if (l < (size_t)cb + 2) return 0;
        int ci = cb == 1 ? p[0] : (int)rb16(p);
        if (ci < csiz) { roi[ci] = p[cb + 1]; if (roiset) roiset[ci] = 1; }
    } else if (mk == 0xFF5F) {                                    /* POC */
        read_poc(p, l, csiz, poc, npoc);
    }
    return 1;
}
static int decode_codestream(const u8j *d, size_t n, jpx_image *out) {
    if (n < 4 || rb16(d) != 0xFF4F) return -1;
    jmain *mh = calloc(1, sizeof *mh);
    if (!mh) return -1;
    size_t pos = 2;
    int rc = -1, siz = 0;
    jtile *tiles = 0;
    int ntiles = 0, *planes[JPX_MAXC] = {0}, cw[JPX_MAXC], ch[JPX_MAXC], cx0[JPX_MAXC], cy0[JPX_MAXC];
    mh->layers = 1;
    int cur = -1, tparts_seen = 0;
    while (pos + 4 <= n) {
        u32j mk = rb16(d + pos);
        if (mk == 0xFFD9) break;
        if (mk < 0xFF00) break;
        size_t l = rb16(d + pos + 2);
        if (l < 2 || pos + 2 + l > n) break;
        const u8j *p = d + pos + 4;
        size_t pl = l - 2;
        if (mk == 0xFF51) {                                        /* SIZ */
            if (pl < 36) goto out;
            mh->xsiz = (int)rb32(p + 2); mh->ysiz = (int)rb32(p + 6); mh->xo = (int)rb32(p + 10); mh->yo = (int)rb32(p + 14);
            mh->xt = (int)rb32(p + 18); mh->yt = (int)rb32(p + 22); mh->xto = (int)rb32(p + 26); mh->yto = (int)rb32(p + 30);
            mh->csiz = (int)rb16(p + 34);
            if (mh->csiz < 1 || mh->csiz > JPX_MAXC || pl < 36 + 3 * (size_t)mh->csiz) goto out;
            if (mh->xsiz <= mh->xo || mh->ysiz <= mh->yo || mh->xt <= 0 || mh->yt <= 0 || mh->xsiz - mh->xo > 65535 || mh->ysiz - mh->yo > 65535) goto out;
            for (int c = 0; c < mh->csiz; c++) {
                mh->sgnd[c] = p[36 + 3 * c] >> 7; mh->prec[c] = (p[36 + 3 * c] & 127) + 1;
                mh->dx[c] = p[37 + 3 * c]; mh->dy[c] = p[38 + 3 * c];
                if (mh->prec[c] > 30 || !mh->dx[c] || !mh->dy[c]) goto out;
            }
            ntiles = cdiv(mh->xsiz - mh->xto, mh->xt) * cdiv(mh->ysiz - mh->yto, mh->yt);
            if (ntiles <= 0 || ntiles > 65535) goto out;
            tiles = calloc((size_t)ntiles, sizeof *tiles);
            if (!tiles) goto out;
            siz = 1;
        } else if (!siz) {
            goto out;
        } else if (mk == 0xFF60) {                                 /* PPM: Zppm, depois (Nppm, dados)... */
            for (size_t i = 1; i + 4 <= pl;) {
                size_t nl = rb32(p + i);
                i += 4;
                if (i + nl > pl) nl = pl - i;
                size_t *no = realloc(mh->ppm_off, (size_t)(mh->nppm + 1) * sizeof *no), *nn = realloc(mh->ppm_len, (size_t)(mh->nppm + 1) * sizeof *nn);
                if (!no || !nn) goto out;
                mh->ppm_off = no; mh->ppm_len = nn;
                mh->ppm_off[mh->nppm] = mh->ppm_n; mh->ppm_len[mh->nppm] = nl; mh->nppm++;
                if (!append(&mh->ppm, &mh->ppm_n, &mh->ppm_cap, p + i, nl)) goto out;
                i += nl;
            }
        } else if (mk == 0xFF90) {                                 /* SOT */
            if (pl < 8) goto out;
            int it = (int)rb16(p);
            size_t psot = rb32(p + 2);
            if (it >= ntiles) goto out;
            cur = it;
            jtile *tl = &tiles[cur];
            if (!tl->init) {
                /* o tile começa com o estado principal (COD com os COC principais, QCD com os QCC, RGN) */
                for (int c = 0; c < mh->csiz; c++) { tl->cod[c] = mh->cod[c]; tl->qcd[c] = mh->qcd[c]; tl->roi[c] = mh->roi[c]; }
                tl->prog = mh->prog; tl->layers = mh->layers; tl->mct = mh->mct; tl->sop = mh->sop; tl->eph = mh->eph;
                tl->init = 1;
            }
            if (!tl->parts) tl->ppm_first = tparts_seen;
            tl->parts++; tl->ppm_parts++;
            tparts_seen++;
            /* cabeçalho da parte do tile até o SOD */
            size_t q = pos + 2 + l;
            size_t end = psot ? pos + psot : n;
            if (end > n) end = n;
            while (q + 4 <= end) {
                u32j m2 = rb16(d + q);
                if (m2 == 0xFF93) { q += 2; break; }
                size_t l2 = rb16(d + q + 2);
                if (l2 < 2 || q + 2 + l2 > end) { q = end; break; }
                const u8j *p2 = d + q + 4;
                size_t pl2 = l2 - 2;
                if (m2 == 0xFF61) { if (pl2 > 1) append(&tl->ppt, &tl->ppt_n, &tl->ppt_cap, p2 + 1, pl2 - 1); }
                else {
                    parse_marker_common(mh, mh->csiz, m2, p2, pl2, tl->cod, tl->cod_set, tl->cod_from_coc, tl->qcd, tl->qcd_set,
                                        tl->qcd_from_qcc, tl->roi, tl->roi_set, &tl->prog, &tl->layers, &tl->mct, &tl->sop, &tl->eph,
                                        &tl->has_cod, tl->poc, &tl->npoc);
                }
                q += 2 + l2;
            }
            if (q < end) append(&tl->data, &tl->n, &tl->cap, d + q, end - q);
            pos = end;
            continue;
        } else {
            parse_marker_common(mh, mh->csiz, mk, p, pl, mh->cod, 0, mh->cod_from_coc, mh->qcd, 0, mh->qcd_from_qcc, mh->roi, 0,
                                &mh->prog, &mh->layers, &mh->mct, &mh->sop, &mh->eph, 0, mh->poc, &mh->npoc);
        }
        pos += 2 + l;
    }
    if (!siz) goto out;
    for (int c = 0; c < mh->csiz; c++) {
        cx0[c] = cdiv(mh->xo, mh->dx[c]); cy0[c] = cdiv(mh->yo, mh->dy[c]);
        cw[c] = cdiv(mh->xsiz, mh->dx[c]) - cx0[c]; ch[c] = cdiv(mh->ysiz, mh->dy[c]) - cy0[c];
        planes[c] = calloc((size_t)cw[c] * (size_t)ch[c], sizeof(int));
        if (!planes[c]) goto out;
    }
    for (int t = 0; t < ntiles; t++)
        if (tiles[t].parts) decode_tile(mh, &tiles[t], t, planes, cw, ch, cx0, cy0);
    /* saída na grade de referência (componentes subamostradas esticadas) */
    out->w = mh->xsiz - mh->xo; out->h = mh->ysiz - mh->yo; out->nc = mh->csiz;
    for (int c = 0; c < mh->csiz; c++) {
        out->prec[c] = mh->prec[c]; out->sgnd[c] = mh->sgnd[c];
        if (mh->dx[c] == 1 && mh->dy[c] == 1) { out->plane[c] = planes[c]; planes[c] = 0; continue; }
        int *pl = malloc((size_t)out->w * (size_t)out->h * sizeof(int));
        if (!pl) goto out;
        for (int y = 0; y < out->h; y++) {
            int sy = (mh->yo + y) / mh->dy[c] - cy0[c];
            if (sy < 0) sy = 0; if (sy >= ch[c]) sy = ch[c] - 1;
            for (int x = 0; x < out->w; x++) {
                int sx = (mh->xo + x) / mh->dx[c] - cx0[c];
                if (sx < 0) sx = 0; if (sx >= cw[c]) sx = cw[c] - 1;
                pl[(size_t)y * out->w + x] = planes[c][(size_t)sy * cw[c] + sx];
            }
        }
        out->plane[c] = pl;
    }
    rc = 0;
out:
    for (int c = 0; c < JPX_MAXC; c++) free(planes[c]);
    for (int t = 0; tiles && t < ntiles; t++) { free(tiles[t].data); free(tiles[t].ppt); }
    free(tiles);
    free(mh->ppm); free(mh->ppm_off); free(mh->ppm_len);
    free(mh);
    return rc;
}

/* ---------------------------------------------------------------- JP2 */
int jpx_sniff(const unsigned char *d, size_t n) {
    if (n >= 12 && rb32(d) == 12 && !memcmp(d + 4, "jP  \r\n\x87\n", 8)) return 1;
    if (n >= 4 && d[0] == 0xFF && d[1] == 0x4F && d[2] == 0xFF && d[3] == 0x51) return 2;
    return 0;
}
typedef struct { const u8j *pclr; size_t pclr_n; const u8j *cmap; size_t cmap_n; const u8j *cdef; size_t cdef_n; } jp2x;
static void jp2_boxes(const u8j *d, size_t n, jpx_image *out, const u8j **jp2c, size_t *jp2c_n, jp2x *x, int depth) {
    size_t pos = 0;
    while (pos + 8 <= n) {
        unsigned long long len = rb32(d + pos);
        u32j type = rb32(d + pos + 4);
        size_t hdr = 8;
        if (len == 1) { if (pos + 16 > n) return; len = (unsigned long long)rb32(d + pos + 8) << 32 | rb32(d + pos + 12); hdr = 16; }
        else if (len == 0) len = n - pos;
        if (len < hdr || pos + len > n) len = n - pos;
        const u8j *b = d + pos + hdr;
        size_t bl = (size_t)len - hdr;
        if (type == 0x6A703268u && depth < 2) jp2_boxes(b, bl, out, jp2c, jp2c_n, x, depth + 1);        /* jp2h */
        else if (type == 0x6A703263u && !*jp2c) { *jp2c = b; *jp2c_n = bl; }                         /* jp2c */
        else if (type == 0x636F6C72u && bl >= 3 && !out->cs && !out->icc) {                         /* colr (o 1º vale) */
            if (b[0] == 1 && bl >= 7) out->cs = (int)rb32(b + 3);
            else if ((b[0] == 2 || b[0] == 3) && bl > 3) {
                out->icc = malloc(bl - 3);
                if (out->icc) { memcpy(out->icc, b + 3, bl - 3); out->icc_n = bl - 3; }
            }
        } else if (type == 0x70636C72u) { x->pclr = b; x->pclr_n = bl; }                          /* pclr */
        else if (type == 0x636D6170u) { x->cmap = b; x->cmap_n = bl; }                            /* cmap */
        else if (type == 0x63646566u) { x->cdef = b; x->cdef_n = bl; }                            /* cdef */
        pos += (size_t)len;
    }
}
void jpx_free(jpx_image *img) {
    if (!img) return;
    for (int c = 0; c < JPX_MAXC; c++) { free(img->plane[c]); img->plane[c] = 0; }
    free(img->icc);
    img->icc = 0;
}
int jpx_decode(const unsigned char *d, size_t n, jpx_image *out) {
    memset(out, 0, sizeof *out);
    out->alpha = -1;
    int kind = jpx_sniff(d, n);
    const u8j *cs = d;
    size_t csn = n;
    jp2x x;
    memset(&x, 0, sizeof x);
    if (kind == 1 || (n >= 8 && rb32(d + 4) == 0x6A502020u)) {
        cs = 0; csn = 0;
        jp2_boxes(d, n, out, &cs, &csn, &x, 0);
        if (!cs) return -1;
    } else if (kind != 2) {
        /* às vezes o PDF traz o codestream com lixo antes: procura SOC SIZ */
        size_t i = 0;
        while (i + 4 <= n && !(d[i] == 0xFF && d[i + 1] == 0x4F && d[i + 2] == 0xFF && d[i + 3] == 0x51)) i++;
        if (i + 4 > n) return -1;
        cs = d + i; csn = n - i;
    }
    if (decode_codestream(cs, csn, out)) { jpx_free(out); return -1; }
    size_t px = (size_t)out->w * (size_t)out->h;
    /* paleta: cmap diz de que componente e de que coluna da paleta sai cada canal */
    if (x.pclr && x.pclr_n >= 3) {
        int ne = (int)rb16(x.pclr), npc = x.pclr[2];
        if (npc > 0 && npc <= JPX_MAXC && x.pclr_n >= 3 + (size_t)npc) {
            int bits[JPX_MAXC], bytes[JPX_MAXC];
            size_t row = 0;
            for (int j = 0; j < npc; j++) { bits[j] = (x.pclr[3 + j] & 127) + 1; bytes[j] = (bits[j] + 7) / 8; row += (size_t)bytes[j]; }
            const u8j *tab = x.pclr + 3 + npc;
            if (x.pclr_n >= 3 + (size_t)npc + row * (size_t)ne) {
                int nch = x.cmap ? (int)(x.cmap_n / 4) : npc;
                if (nch > JPX_MAXC) nch = JPX_MAXC;
                int *np[JPX_MAXC] = {0}, nprec[JPX_MAXC];
                int okp = 1;
                for (int k = 0; k < nch && okp; k++) {
                    int cmp = x.cmap ? (int)rb16(x.cmap + 4 * k) : 0, mtyp = x.cmap ? x.cmap[4 * k + 2] : 1, pcol = x.cmap ? x.cmap[4 * k + 3] : k;
                    if (cmp >= out->nc) { okp = 0; break; }
                    np[k] = malloc(px * sizeof(int));
                    if (!np[k]) { okp = 0; break; }
                    if (mtyp == 1 && pcol < npc) {
                        size_t coff = 0;
                        for (int j = 0; j < pcol; j++) coff += (size_t)bytes[j];
                        nprec[k] = bits[pcol];
                        for (size_t i = 0; i < px; i++) {
                            int idx = out->plane[cmp][i];
                            if (idx < 0) idx = 0;
                            if (idx >= ne) idx = ne - 1;
                            const u8j *e = tab + (size_t)idx * row + coff;
                            int v = 0;
                            for (int b = 0; b < bytes[pcol]; b++) v = v << 8 | e[b];
                            np[k][i] = v;
                        }
                    } else { memcpy(np[k], out->plane[cmp], px * sizeof(int)); nprec[k] = out->prec[cmp]; }
                }
                if (okp) {
                    for (int c = 0; c < JPX_MAXC; c++) { free(out->plane[c]); out->plane[c] = 0; }
                    for (int k = 0; k < nch; k++) { out->plane[k] = np[k]; out->prec[k] = nprec[k]; out->sgnd[k] = 0; }
                    out->nc = nch;
                } else for (int k = 0; k < nch; k++) free(np[k]);
            }
        }
    }
    /* cdef: canal de opacidade e reordenação das cores */
    if (x.cdef && x.cdef_n >= 2) {
        int nd = (int)rb16(x.cdef);
        int *ord[JPX_MAXC] = {0}, oprec[JPX_MAXC], osg[JPX_MAXC], used[JPX_MAXC] = {0};
        int ncol = 0;
        for (int i = 0; i < nd && 2 + 6 * (size_t)(i + 1) <= x.cdef_n; i++) {
            int cn = (int)rb16(x.cdef + 2 + 6 * i), typ = (int)rb16(x.cdef + 4 + 6 * i), asoc = (int)rb16(x.cdef + 6 + 6 * i);
            if (cn >= out->nc) continue;
            if (typ == 0 && asoc >= 1 && asoc <= JPX_MAXC && asoc - 1 < out->nc) { ord[asoc - 1] = out->plane[cn]; oprec[asoc - 1] = out->prec[cn]; osg[asoc - 1] = out->sgnd[cn]; used[cn] = 1; if (asoc > ncol) ncol = asoc; }
            else if (typ == 1 || typ == 2) { out->alpha = cn; out->premul = typ == 2; }
        }
        if (ncol > 0) {
            int all = 1;
            for (int k = 0; k < ncol; k++) if (!ord[k]) all = 0;
            if (all) {
                int *alpha = out->alpha >= 0 ? out->plane[out->alpha] : 0;
                int ap = out->alpha >= 0 ? out->prec[out->alpha] : 0;
                int *rest[JPX_MAXC];
                int nr = 0;
                for (int c = 0; c < out->nc; c++) if (!used[c] && c != out->alpha) rest[nr++] = out->plane[c];
                for (int k = 0; k < ncol; k++) { out->plane[k] = ord[k]; out->prec[k] = oprec[k]; out->sgnd[k] = osg[k]; }
                int at = ncol;
                if (alpha) { out->plane[at] = alpha; out->prec[at] = ap; out->sgnd[at] = 0; out->alpha = at++; }
                for (int i = 0; i < nr && at < JPX_MAXC; i++) out->plane[at++] = rest[i];
                out->nc = at;
            }
        }
    }
    /* sYCC → sRGB (componentes 0..2 com 8+ bits) */
    if (out->cs == 18 && out->nc >= 3) {
        int pr = out->prec[0];
        double half = (double)(1 << (pr - 1)), maxv = (double)((1 << pr) - 1);
        for (size_t i = 0; i < px; i++) {
            double y = out->plane[0][i], cb = out->plane[1][i] - half, cr = out->plane[2][i] - half;
            double r = y + 1.402 * cr, g = y - 0.344136 * cb - 0.714136 * cr, b = y + 1.772 * cb;
            out->plane[0][i] = (int)(r < 0 ? 0 : r > maxv ? maxv : r + 0.5);
            out->plane[1][i] = (int)(g < 0 ? 0 : g > maxv ? maxv : g + 0.5);
            out->plane[2][i] = (int)(b < 0 ? 0 : b > maxv ? maxv : b + 0.5);
        }
        out->cs = 16;
    }
    return 0;
}
