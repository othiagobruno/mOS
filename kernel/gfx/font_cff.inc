/* mOS — fontes OpenType com contornos CFF ('OTTO'), incluído por font.c.
 *
 * As fontes web do Slack e de muitos sites chegam como CFF: em vez de glyf/loca, a tabela 'CFF ' guarda cada
 * glifo como um charstring Type 2 (um pequeno programa de pilha que desenha cúbicas). Aqui ficam o leitor dos
 * INDEX e DICT do CFF 1 e o interpretador de charstrings: subrotinas locais e globais, flex, fontes CID
 * (FDArray + FDSelect formatos 0 e 3) e as operações aritméticas comuns. As cúbicas viram segmentos para o
 * mesmo rasterizador da glyf.
 *
 * Os bytes vêm de um processo (GF_REGISTER): cff_parse confere a estrutura inteira (cabeçalho, cada INDEX,
 * os DICTs, Private, Subrs e FDSelect) e o interpretador confere cada leitura, a pilha (48 operandos), a
 * profundidade das subrotinas (10), o número de operações e o de segmentos. Hints, a largura no charstring (o avanço vem da
 * hmtx) e acentos por endchar (seac) são ignorados. CFF2 (fontes variáveis) não é aceito. */

#define CFF_MAX_FD 16
#define CFF_MAX_SEGS 65536   /* segmentos por glifo: um charstring malicioso não esgota a memória do kernel */

typedef struct { u32 off, count, data; u8 osz; } cff_index_t;   /* data: dados - 1 (os deslocamentos começam em 1) */

typedef struct cff {
    const u8 *p;
    u32 n;
    cff_index_t cs, gsubr, lsubr[CFF_MAX_FD];
    u32 fdsel;            /* deslocamento do FDSelect; 0 = fonte sem CID */
    u8 fdsel_fmt, nfd;
} cff_t;

static u32 cff_uoff(const u8 *p, u8 osz) {
    u32 v = 0;
    for (u8 i = 0; i < osz; i++) v = v << 8 | p[i];
    return v;
}

/* INDEX em `off`: confere os deslocamentos (o primeiro é 1, nunca diminuem, o último cabe na tabela) e
 * devolve onde ele termina; 0 = malformado */
static u32 cff_index(const u8 *p, u32 n, u32 off, cff_index_t *ix) {
    memset(ix, 0, sizeof *ix);
    if ((u64)off + 2 > n) return 0;
    u32 count = be16(p + off);
    ix->off = off;
    ix->count = count;
    if (!count) return off + 2;
    if ((u64)off + 3 > n) return 0;
    u8 osz = p[off + 2];
    if (osz < 1 || osz > 4) return 0;
    u64 offs = (u64)off + 3, end = offs + (u64)(count + 1) * osz;
    if (end > n) return 0;
    u32 prev = 0;
    for (u32 i = 0; i <= count; i++) {
        u32 v = cff_uoff(p + offs + (u64)i * osz, osz);
        if (i == 0 ? v != 1 : v < prev) return 0;
        prev = v;
    }
    if (end - 1 + prev > n) return 0;
    ix->osz = osz;
    ix->data = (u32)(end - 1);
    return ix->data + prev;
}

/* item i de um INDEX já conferido: [*s, *e) */
static bool cff_item(const u8 *p, u32 n, const cff_index_t *ix, u32 i, u32 *s, u32 *e) {
    if (i >= ix->count || !ix->osz) return false;
    const u8 *o = p + ix->off + 3 + (u64)i * ix->osz;
    u32 a = cff_uoff(o, ix->osz), b = cff_uoff(o + ix->osz, ix->osz);
    *s = ix->data + a;
    *e = ix->data + b;
    return a <= b && *e <= n;
}

typedef struct { double charstrings, priv_size, priv_off, subrs, fdarray, fdselect, cstype; bool cid; } cff_dict_t;
static void cff_dict_init(cff_dict_t *d) {
    memset(d, 0, sizeof *d);
    d->charstrings = d->priv_size = d->priv_off = d->subrs = d->fdarray = d->fdselect = -1;
    d->cstype = 2;
}

/* número real do DICT (operador 30): dígitos em nibbles até 0xf */
static u32 cff_real(const u8 *p, u32 i, u32 e, double *out) {
    double v = 0, frac = 0.1;
    int expo = 0, esign = 1, part = 0;   /* 0 inteiro, 1 fração, 2 expoente */
    bool neg = false;
    for (; i < e; i++) {
        for (int k = 0; k < 2; k++) {
            u8 nb = k ? (p[i] & 15) : (p[i] >> 4);
            if (nb <= 9) {
                if (part == 0) v = v * 10 + nb;
                else if (part == 1) { v += nb * frac; frac /= 10; }
                else if (expo < 400) expo = expo * 10 + nb;
            } else if (nb == 0xa) part = 1;
            else if (nb == 0xb) { part = 2; esign = 1; }
            else if (nb == 0xc) { part = 2; esign = -1; }
            else if (nb == 0xe) neg = true;
            else if (nb == 0xf) {
                while (expo-- > 0) v = esign > 0 ? v * 10 : v / 10;
                *out = neg ? -v : v;
                return i + 1;
            }
        }
    }
    return 0;
}

/* os operadores de que o leitor precisa (Top DICT, Font DICT do FDArray e Private) */
static bool cff_parse_dict(const u8 *p, u32 s, u32 e, cff_dict_t *d) {
    double a[48];
    int na = 0;
    u32 i = s;
    while (i < e) {
        u8 b = p[i];
        double v;
        if (b <= 21) {
            int op = b;
            i++;
            if (b == 12) { if (i >= e) return false; op = 1200 + p[i++]; }
            switch (op) {
            case 17: if (na >= 1) d->charstrings = a[na - 1]; break;
            case 18: if (na >= 2) { d->priv_size = a[na - 2]; d->priv_off = a[na - 1]; } break;
            case 19: if (na >= 1) d->subrs = a[na - 1]; break;
            case 1206: if (na >= 1) d->cstype = a[na - 1]; break;
            case 1230: d->cid = true; break;
            case 1236: if (na >= 1) d->fdarray = a[na - 1]; break;
            case 1237: if (na >= 1) d->fdselect = a[na - 1]; break;
            default: break;
            }
            na = 0;
            continue;
        }
        if (b == 28) { if ((u64)i + 3 > e) return false; v = (i16)be16(p + i + 1); i += 3; }
        else if (b == 29) { if ((u64)i + 5 > e) return false; v = (i32)be32(p + i + 1); i += 5; }
        else if (b == 30) { if (!(i = cff_real(p, i + 1, e, &v))) return false; }
        else if (b >= 32 && b <= 246) { v = (int)b - 139; i++; }
        else if (b >= 247 && b <= 250) { if ((u64)i + 2 > e) return false; v = ((int)b - 247) * 256 + p[i + 1] + 108; i += 2; }
        else if (b >= 251 && b <= 254) { if ((u64)i + 2 > e) return false; v = -((int)b - 251) * 256 - p[i + 1] - 108; i += 2; }
        else return false;                                   /* 22..27, 31 e 255 são reservados no DICT */
        if (na < 48) a[na++] = v;
    }
    return true;
}

static bool cff_in(double v, u32 n) { return v >= 0 && v < (double)n; }

/* Private DICT de uma face: só as subrotinas locais interessam */
static bool cff_private(const u8 *p, u32 n, const cff_dict_t *d, cff_index_t *subrs) {
    memset(subrs, 0, sizeof *subrs);
    if (d->priv_off < 0 || d->priv_size <= 0) return true;   /* sem Private: sem subrotinas locais */
    if (!cff_in(d->priv_off, n) || d->priv_size > (double)(n - (u32)d->priv_off)) return false;
    u32 s = (u32)d->priv_off, e = s + (u32)d->priv_size;
    cff_dict_t pd;
    cff_dict_init(&pd);
    if (!cff_parse_dict(p, s, e, &pd)) return false;
    if (pd.subrs > 0) {
        double so = (double)s + pd.subrs;
        if (!cff_in(so, n) || !cff_index(p, n, (u32)so, subrs)) return false;
    }
    return true;
}

/* confere e prepara a tabela 'CFF ' (p, n) para uma fonte de nglyphs glifos */
static bool cff_parse(const u8 *p, u32 n, u32 nglyphs, cff_t *c) {
    memset(c, 0, sizeof *c);
    if (n < 4 || p[0] != 1) return false;                    /* CFF 1 (o CFF2 tem outro formato) */
    u32 hdr = p[2];
    if (hdr < 4 || hdr >= n) return false;
    cff_index_t names, top, strings;
    u32 o = cff_index(p, n, hdr, &names);
    if (!o) return false;
    o = cff_index(p, n, o, &top);
    if (!o || top.count < 1) return false;
    o = cff_index(p, n, o, &strings);
    if (!o) return false;
    if (!cff_index(p, n, o, &c->gsubr)) return false;
    u32 ts, te;
    if (!cff_item(p, n, &top, 0, &ts, &te)) return false;
    cff_dict_t d;
    cff_dict_init(&d);
    if (!cff_parse_dict(p, ts, te, &d) || d.cstype != 2 || d.charstrings <= 0 || !cff_in(d.charstrings, n)) return false;
    if (!cff_index(p, n, (u32)d.charstrings, &c->cs) || !c->cs.count || c->cs.count < nglyphs) return false;
    if (d.cid) {
        if (!cff_in(d.fdarray, n) || d.fdarray <= 0 || !cff_in(d.fdselect, n) || d.fdselect <= 0) return false;
        cff_index_t fda;
        if (!cff_index(p, n, (u32)d.fdarray, &fda) || !fda.count || fda.count > CFF_MAX_FD) return false;
        for (u32 i = 0; i < fda.count; i++) {
            u32 s, e;
            cff_dict_t fd;
            cff_dict_init(&fd);
            if (!cff_item(p, n, &fda, i, &s, &e) || !cff_parse_dict(p, s, e, &fd) || !cff_private(p, n, &fd, &c->lsubr[i])) return false;
        }
        c->nfd = (u8)fda.count;
        u32 fs = (u32)d.fdselect;
        u8 fmt = p[fs];
        if (fmt == 0) {
            if ((u64)fs + 1 + c->cs.count > n) return false;
            for (u32 g = 0; g < c->cs.count; g++) if (p[fs + 1 + g] >= c->nfd) return false;
        } else if (fmt == 3) {
            if ((u64)fs + 3 > n) return false;
            u32 nr = be16(p + fs + 1);
            if (!nr || (u64)fs + 3 + (u64)nr * 3 + 2 > n || be16(p + fs + 3) != 0) return false;
            u32 prev = 0;
            for (u32 r = 0; r < nr; r++) {
                u32 first = be16(p + fs + 3 + r * 3);
                if ((r && first <= prev) || p[fs + 3 + r * 3 + 2] >= c->nfd) return false;
                prev = first;
            }
            if (be16(p + fs + 3 + nr * 3) <= prev) return false;   /* sentinela depois do último intervalo */
        } else return false;
        c->fdsel = fs;
        c->fdsel_fmt = fmt;
    } else {
        c->nfd = 1;
        if (!cff_private(p, n, &d, &c->lsubr[0])) return false;
    }
    c->p = p;
    c->n = n;
    return true;
}

static u32 cff_fd(const cff_t *c, u32 g) {
    if (!c->fdsel) return 0;
    const u8 *p = c->p + c->fdsel;
    if (c->fdsel_fmt == 0) return g < c->cs.count ? p[1 + g] : 0;
    u32 nr = be16(p + 1), fd = 0;
    for (u32 r = 0; r < nr; r++) {
        if (be16(p + 3 + r * 3) > g) break;
        fd = p[3 + r * 3 + 2];
    }
    return fd < c->nfd ? fd : 0;
}

static int cff_bias(u32 count) { return count < 1240 ? 107 : count < 33900 ? 1131 : 32768; }

/* ------------------------------------------------------------ charstrings Type 2 */
static void add_cubic(lines_t *l, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3) {
    float dd = fabsf_(x0 - 2 * x1 + x2) + fabsf_(y0 - 2 * y1 + y2) + fabsf_(x1 - 2 * x2 + x3) + fabsf_(y1 - 2 * y2 + y3);
    int n = 1 + (int)fsqrt(dd * 3);
    if (n > 24) n = 24;
    float px = x0, py = y0;
    for (int i = 1; i <= n; i++) {
        float t = (float)i / n, mt = 1 - t;
        float a = mt * mt * mt, b = 3 * mt * mt * t, cc = 3 * mt * t * t, d = t * t * t;
        float x = a * x0 + b * x1 + cc * x2 + d * x3, y = a * y0 + b * y1 + cc * y2 + d * y3;
        add_line(l, px, py, x, y);
        px = x;
        py = y;
    }
}

typedef struct {
    const cff_t *c;
    const cff_index_t *lsubr;
    float st[48];
    int sp, nstems, ops;
    bool open;
    float x, y, sx, sy;          /* ponto atual e início do contorno, em unidades da fonte */
    float scale, ox, oy;
    float tr[32];                /* vetor transiente (put/get) */
    lines_t *out;
} cs_t;

static void cs_xy(const cs_t *s, float x, float y, float *X, float *Y) { *X = x * s->scale + s->ox; *Y = s->oy - y * s->scale; }
static void cs_begin(cs_t *s) { if (!s->open) { s->sx = s->x; s->sy = s->y; s->open = true; } }
static void cs_line(cs_t *s, float x, float y) {
    cs_begin(s);
    float x0, y0, x1, y1;
    cs_xy(s, s->x, s->y, &x0, &y0);
    cs_xy(s, x, y, &x1, &y1);
    add_line(s->out, x0, y0, x1, y1);
    s->x = x;
    s->y = y;
}
static void cs_rline(cs_t *s, float dx, float dy) { cs_line(s, s->x + dx, s->y + dy); }
static void cs_rcurve(cs_t *s, float dx1, float dy1, float dx2, float dy2, float dx3, float dy3) {
    cs_begin(s);
    float x1 = s->x + dx1, y1 = s->y + dy1, x2 = x1 + dx2, y2 = y1 + dy2, x3 = x2 + dx3, y3 = y2 + dy3;
    float a0, b0, a1, b1, a2, b2, a3, b3;
    cs_xy(s, s->x, s->y, &a0, &b0);
    cs_xy(s, x1, y1, &a1, &b1);
    cs_xy(s, x2, y2, &a2, &b2);
    cs_xy(s, x3, y3, &a3, &b3);
    add_cubic(s->out, a0, b0, a1, b1, a2, b2, a3, b3);
    s->x = x3;
    s->y = y3;
}
/* fecha o contorno sem mexer no ponto atual: como no Type 1, o moveto seguinte é relativo ao último ponto
 * desenhado, não ao início do contorno (o pingo do "i" ia para o lugar errado) */
static void cs_close(cs_t *s) {
    if (s->open && (s->x != s->sx || s->y != s->sy)) {
        float x = s->x, y = s->y;
        cs_line(s, s->sx, s->sy);
        s->x = x;
        s->y = y;
    }
    s->open = false;
}
static void cs_move(cs_t *s, float dx, float dy) {
    cs_close(s);
    s->x += dx;
    s->y += dy;
}

/* 1 = endchar, 0 = return ou fim dos dados, -1 = charstring inválido */
static int cs_run(cs_t *s, u32 pos, u32 end, int depth) {
    const u8 *p = s->c->p;
    if (depth > 10) return -1;
    while (pos < end) {
        if (++s->ops > 200000 || s->out->n > CFF_MAX_SEGS * 4) return -1;
        u8 b = p[pos];
        if (b >= 32 || b == 28) {                            /* operando */
            float v;
            if (b == 28) { if ((u64)pos + 3 > end) return -1; v = (i16)be16(p + pos + 1); pos += 3; }
            else if (b <= 246) { v = (float)((int)b - 139); pos++; }
            else if (b <= 250) { if ((u64)pos + 2 > end) return -1; v = (float)(((int)b - 247) * 256 + p[pos + 1] + 108); pos += 2; }
            else if (b <= 254) { if ((u64)pos + 2 > end) return -1; v = (float)(-((int)b - 251) * 256 - p[pos + 1] - 108); pos += 2; }
            else { if ((u64)pos + 5 > end) return -1; v = (float)(i32)be32(p + pos + 1) / 65536.0f; pos += 5; }
            if (s->sp >= 48) return -1;
            s->st[s->sp++] = v;
            continue;
        }
        pos++;
        int op = b;
        if (b == 12) { if (pos >= end) return -1; op = 1200 + p[pos++]; }
        float *a = s->st;
        int n = s->sp, i;
        switch (op) {
        case 1: case 3: case 18: case 23:                    /* hstem vstem hstemhm vstemhm (largura se ímpar) */
            s->nstems += n / 2;
            s->sp = 0;
            break;
        case 19: case 20: {                                  /* hintmask cntrmask: vstem implícito + bytes da máscara */
            s->nstems += n / 2;
            s->sp = 0;
            u32 mb = (u32)(s->nstems + 7) / 8;
            if ((u64)pos + mb > end) return -1;
            pos += mb;
            break;
        }
        case 21: if (n >= 2) cs_move(s, a[n - 2], a[n - 1]); s->sp = 0; break;   /* rmoveto */
        case 22: if (n >= 1) cs_move(s, a[n - 1], 0); s->sp = 0; break;          /* hmoveto */
        case 4: if (n >= 1) cs_move(s, 0, a[n - 1]); s->sp = 0; break;           /* vmoveto */
        case 5: for (i = 0; i + 1 < n; i += 2) cs_rline(s, a[i], a[i + 1]); s->sp = 0; break;   /* rlineto */
        case 6: case 7: {                                    /* hlineto vlineto: alternando */
            bool h = op == 6;
            for (i = 0; i < n; i++, h = !h) cs_rline(s, h ? a[i] : 0, h ? 0 : a[i]);
            s->sp = 0;
            break;
        }
        case 8: for (i = 0; i + 5 < n; i += 6) cs_rcurve(s, a[i], a[i + 1], a[i + 2], a[i + 3], a[i + 4], a[i + 5]); s->sp = 0; break;
        case 24:                                             /* rcurveline */
            for (i = 0; i + 5 < n - 2; i += 6) cs_rcurve(s, a[i], a[i + 1], a[i + 2], a[i + 3], a[i + 4], a[i + 5]);
            if (i + 1 < n) cs_rline(s, a[i], a[i + 1]);
            s->sp = 0;
            break;
        case 25:                                             /* rlinecurve */
            for (i = 0; i + 1 < n - 6; i += 2) cs_rline(s, a[i], a[i + 1]);
            if (i + 5 < n) cs_rcurve(s, a[i], a[i + 1], a[i + 2], a[i + 3], a[i + 4], a[i + 5]);
            s->sp = 0;
            break;
        case 26: {                                           /* vvcurveto: dx1? {dya dxb dyb dyc}+ */
            float d1 = 0;
            i = 0;
            if (n & 1) d1 = a[i++];
            for (; i + 3 < n; i += 4, d1 = 0) cs_rcurve(s, d1, a[i], a[i + 1], a[i + 2], 0, a[i + 3]);
            s->sp = 0;
            break;
        }
        case 27: {                                           /* hhcurveto: dy1? {dxa dxb dyb dxc}+ */
            float d1 = 0;
            i = 0;
            if (n & 1) d1 = a[i++];
            for (; i + 3 < n; i += 4, d1 = 0) cs_rcurve(s, a[i], d1, a[i + 1], a[i + 2], a[i + 3], 0);
            s->sp = 0;
            break;
        }
        case 30: case 31: {                                  /* vhcurveto hvcurveto: alternando, último com 5 */
            bool h = op == 31;
            for (i = 0; i + 3 < n; h = !h) {
                float f = (n - i == 5) ? a[i + 4] : 0;
                if (h) cs_rcurve(s, a[i], 0, a[i + 1], a[i + 2], f, a[i + 3]);
                else cs_rcurve(s, 0, a[i], a[i + 1], a[i + 2], a[i + 3], f);
                i += (n - i == 5) ? 5 : 4;
            }
            s->sp = 0;
            break;
        }
        case 10: case 29: {                                  /* callsubr callgsubr */
            if (!n) return -1;
            const cff_index_t *ix = op == 10 ? s->lsubr : &s->c->gsubr;
            i32 k = (i32)a[n - 1] + cff_bias(ix->count);
            s->sp--;
            u32 ss, se;
            if (k < 0 || !cff_item(p, s->c->n, ix, (u32)k, &ss, &se)) return -1;
            int r = cs_run(s, ss, se, depth + 1);
            if (r) return r;
            break;
        }
        case 11: return 0;                                   /* return */
        case 14: cs_close(s); return 1;                      /* endchar (seac ignorado) */
        case 1235:                                           /* flex */
            if (n >= 12) { cs_rcurve(s, a[0], a[1], a[2], a[3], a[4], a[5]); cs_rcurve(s, a[6], a[7], a[8], a[9], a[10], a[11]); }
            s->sp = 0;
            break;
        case 1234:                                           /* hflex */
            if (n >= 7) { cs_rcurve(s, a[0], 0, a[1], a[2], a[3], 0); cs_rcurve(s, a[4], 0, a[5], -a[2], a[6], 0); }
            s->sp = 0;
            break;
        case 1236:                                           /* hflex1 */
            if (n >= 9) {
                cs_rcurve(s, a[0], a[1], a[2], a[3], a[4], 0);
                cs_rcurve(s, a[5], 0, a[6], a[7], a[8], -(a[1] + a[3] + a[7]));
            }
            s->sp = 0;
            break;
        case 1237:                                           /* flex1 */
            if (n >= 11) {
                float dx = a[0] + a[2] + a[4] + a[6] + a[8], dy = a[1] + a[3] + a[5] + a[7] + a[9];
                cs_rcurve(s, a[0], a[1], a[2], a[3], a[4], a[5]);
                if (fabsf_(dx) > fabsf_(dy)) cs_rcurve(s, a[6], a[7], a[8], a[9], a[10], -dy);
                else cs_rcurve(s, a[6], a[7], a[8], a[9], -dx, a[10]);
            }
            s->sp = 0;
            break;
        /* aritmética (rara nas fontes, mas faz parte do Type 2) */
        case 1209: if (n >= 1) a[n - 1] = fabsf_(a[n - 1]); break;                               /* abs */
        case 1210: if (n >= 2) { a[n - 2] += a[n - 1]; s->sp--; } break;                         /* add */
        case 1211: if (n >= 2) { a[n - 2] -= a[n - 1]; s->sp--; } break;                         /* sub */
        case 1212: if (n >= 2) { a[n - 2] = a[n - 1] != 0 ? a[n - 2] / a[n - 1] : 0; s->sp--; } break;   /* div */
        case 1214: if (n >= 1) a[n - 1] = -a[n - 1]; break;                                      /* neg */
        case 1218: if (n >= 1) s->sp--; break;                                                   /* drop */
        case 1220: if (n >= 2) { i32 k = (i32)a[n - 1]; if (k >= 0 && k < 32) s->tr[k] = a[n - 2]; s->sp -= 2; } break;   /* put */
        case 1221: if (n >= 1) { i32 k = (i32)a[n - 1]; a[n - 1] = (k >= 0 && k < 32) ? s->tr[k] : 0; } break;           /* get */
        case 1224: if (n >= 2) { a[n - 2] *= a[n - 1]; s->sp--; } break;                         /* mul */
        case 1226: if (n >= 1) a[n - 1] = a[n - 1] > 0 ? fsqrt(a[n - 1]) : 0; break;             /* sqrt */
        case 1227: if (n >= 1 && n < 48) { a[n] = a[n - 1]; s->sp++; } break;                    /* dup */
        case 1228: if (n >= 2) { float t = a[n - 1]; a[n - 1] = a[n - 2]; a[n - 2] = t; } break; /* exch */
        case 1229: if (n >= 1) { i32 k = (i32)a[n - 1]; a[n - 1] = (k >= 0 && k < n - 1) ? a[n - 2 - k] : (n >= 2 ? a[n - 2] : 0); } break;   /* index */
        default: s->sp = 0; break;                           /* and/or/not/eq/ifelse/random/roll e reservados */
        }
    }
    return 0;
}

/* contorno do glifo g em coordenadas de tela: x' = x*scale + ox, y' = oy - y*scale */
static bool cff_outline(const cff_t *c, u32 g, float scale, float ox, float oy, lines_t *out) {
    u32 s, e;
    if (!c->p || !cff_item(c->p, c->n, &c->cs, g, &s, &e) || s == e) return false;
    cs_t st;
    memset(&st, 0, sizeof st);
    st.c = c;
    st.lsubr = &c->lsubr[cff_fd(c, g)];
    st.scale = scale;
    st.ox = ox;
    st.oy = oy;
    st.out = out;
    int r = cs_run(&st, s, e, 0);
    cs_close(&st);
    return r >= 0;
}

/* para font.c: a face guarda o leitor pronto; font_sane só confere */
static struct cff *cff_load(const u8 *p, u32 n, u32 nglyphs) {
    cff_t *c = kmalloc(sizeof *c);
    if (c && !cff_parse(p, n, nglyphs, c)) { kfree(c); c = NULL; }
    return c;
}
static bool cff_check(const u8 *p, u32 n, u32 nglyphs) {
    cff_t c;
    return cff_parse(p, n, nglyphs, &c);
}
