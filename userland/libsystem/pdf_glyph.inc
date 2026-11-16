/* mOS PDF — contornos de glifos das fontes embutidas, como CGPath (unidades da fonte).
 * TrueType (glyf/loca, glifos simples e compostos), CFF (charstrings Type 2: subrotinas locais e globais,
 * flex, máscaras de hint, endchar com acento, CID-keyed com FDArray/FDSelect) e Type 1 (eexec, charstrings,
 * Subrs, OtherSubrs 0–3 de flex, seac). Incluído por cgpdf_draw.c. */

void *CGPathCreateMutable(void);
void CGPathMoveToPoint(void *p, const CGAffineTransform *m, double x, double y);
void CGPathAddLineToPoint(void *p, const CGAffineTransform *m, double x, double y);
void CGPathAddQuadCurveToPoint(void *p, const CGAffineTransform *m, double cx, double cy, double x, double y);
void CGPathAddCurveToPoint(void *p, const CGAffineTransform *m, double a, double b, double c, double d, double x, double y);
void CGPathCloseSubpath(void *p);
void CGPathRelease(void *p);

typedef struct { void *path; int open; double sx, sy; } pen_t;
static void pen_move(pen_t *p, double x, double y) { if (p->open) CGPathCloseSubpath(p->path); CGPathMoveToPoint(p->path, 0, x, y); p->open = 1; p->sx = x; p->sy = y; }
static void pen_line(pen_t *p, double x, double y) { if (!p->open) pen_move(p, x, y); else CGPathAddLineToPoint(p->path, 0, x, y); }
static void pen_quad(pen_t *p, double cx, double cy, double x, double y) { CGPathAddQuadCurveToPoint(p->path, 0, cx, cy, x, y); }
static void pen_cubic(pen_t *p, double a, double b, double c, double d, double x, double y) { if (!p->open) pen_move(p, a, b); CGPathAddCurveToPoint(p->path, 0, a, b, c, d, x, y); }
static void pen_close(pen_t *p) { if (p->open) CGPathCloseSubpath(p->path); p->open = 0; }

static u32 g16(const u8 *p) { return (u32)p[0] << 8 | p[1]; }
static int gs16(const u8 *p) { return (int)(int16_t)g16(p); }
static u32 g32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

/* ---------------------------------------------------------------- TrueType */
typedef struct { const u8 *d; size_t n; const u8 *glyf, *loca, *cmap, *hmtx, *post; size_t glyfn, locan, cmapn, hmtxn, postn; int longloca, upem, nglyphs, nhm; } tt_font;
static const u8 *tt_table(const u8 *d, size_t n, u32 tag, size_t *len) {
    size_t base = 0;
    if (n >= 12 && g32(d) == 0x74746366u) base = g32(d + 12);       /* coleção: a primeira fonte */
    if (base + 12 > n) return 0;
    u32 nt = g16(d + base + 4);
    for (u32 i = 0; i < nt && base + 12 + (size_t)i * 16 + 16 <= n; i++) {
        const u8 *e = d + base + 12 + i * 16;
        u32 off = g32(e + 8), l = g32(e + 12);
        if (g32(e) == tag && (size_t)off + l <= n) { *len = l; return d + off; }
    }
    return 0;
}
static int tt_open(tt_font *t, const u8 *d, size_t n) {
    memset(t, 0, sizeof *t);
    t->d = d; t->n = n;
    size_t hl, ml, hhl;
    const u8 *head = tt_table(d, n, 0x68656164, &hl), *maxp = tt_table(d, n, 0x6D617870, &ml), *hhea = tt_table(d, n, 0x68686561, &hhl);
    t->glyf = tt_table(d, n, 0x676C7966, &t->glyfn);
    t->loca = tt_table(d, n, 0x6C6F6361, &t->locan);
    t->cmap = tt_table(d, n, 0x636D6170, &t->cmapn);
    t->hmtx = tt_table(d, n, 0x686D7478, &t->hmtxn);
    t->post = tt_table(d, n, 0x706F7374, &t->postn);
    t->upem = head && hl >= 20 ? (int)g16(head + 18) : 1000;
    if (t->upem < 16) t->upem = 1000;
    t->longloca = head && hl >= 52 ? gs16(head + 50) : 0;
    t->nglyphs = maxp && ml >= 6 ? (int)g16(maxp + 4) : 0;
    t->nhm = hhea && hhl >= 36 ? (int)g16(hhea + 34) : 0;
    return t->glyf && t->loca ? 0 : -1;
}
static int tt_glyph_range(const tt_font *t, int gid, u32 *a, u32 *b) {
    if (gid < 0) return 0;
    if (t->longloca) { if ((size_t)(gid + 2) * 4 > t->locan) return 0; *a = g32(t->loca + gid * 4); *b = g32(t->loca + gid * 4 + 4); }
    else { if ((size_t)(gid + 2) * 2 > t->locan) return 0; *a = g16(t->loca + gid * 2) * 2; *b = g16(t->loca + gid * 2 + 2) * 2; }
    return *b > *a && *b <= t->glyfn;
}
static void tt_outline(const tt_font *t, int gid, pen_t *pen, const double *m, int depth) {
    u32 a, b;
    if (depth > 8 || !tt_glyph_range(t, gid, &a, &b)) return;
    const u8 *g = t->glyf + a, *end = t->glyf + b;
    if (end - g < 10) return;
    int nc = gs16(g);
    if (nc >= 0) {
        const u8 *ep = g + 10;
        if (ep + nc * 2 + 2 > end) return;
        int npts = nc ? (int)g16(ep + (nc - 1) * 2) + 1 : 0;
        u32 il = g16(ep + nc * 2);
        const u8 *p = ep + nc * 2 + 2 + il;
        if (p > end || npts <= 0 || npts > 65535) return;
        u8 *fl = malloc((size_t)npts);
        double *xs = malloc((size_t)npts * sizeof(double)), *ys = malloc((size_t)npts * sizeof(double));
        if (!fl || !xs || !ys) { free(fl); free(xs); free(ys); return; }
        for (int i = 0; i < npts && p < end;) {
            u8 f = *p++;
            int rep = 1;
            if (f & 8 && p < end) rep += *p++;
            for (int k = 0; k < rep && i < npts; k++) fl[i++] = f;
        }
        int v = 0;
        for (int i = 0; i < npts; i++) {
            u8 f = fl[i];
            if (f & 2) { int d = p < end ? *p++ : 0; v += f & 16 ? d : -d; }
            else if (!(f & 16)) { v += p + 1 < end ? gs16(p) : 0; p += 2; }
            xs[i] = v;
        }
        v = 0;
        for (int i = 0; i < npts; i++) {
            u8 f = fl[i];
            if (f & 4) { int d = p < end ? *p++ : 0; v += f & 32 ? d : -d; }
            else if (!(f & 32)) { v += p + 1 < end ? gs16(p) : 0; p += 2; }
            ys[i] = v;
        }
        for (int i = 0; i < npts; i++) {                     /* matriz do composto */
            double x = xs[i], y = ys[i];
            xs[i] = m[0] * x + m[2] * y + m[4];
            ys[i] = m[1] * x + m[3] * y + m[5];
        }
        int s = 0;
        for (int c = 0; c < nc; c++) {
            int e = (int)g16(ep + c * 2);
            if (e < s || e >= npts) break;
            int cnt = e - s + 1;
            /* ponto inicial: o primeiro na curva, ou o meio entre dois fora */
            int first = s;
            double fx, fy;
            if (fl[s] & 1) { fx = xs[s]; fy = ys[s]; first = s + 1; cnt--; }
            else if (fl[e] & 1) { fx = xs[e]; fy = ys[e]; }
            else { fx = (xs[s] + xs[e]) / 2; fy = (ys[s] + ys[e]) / 2; }
            pen_move(pen, fx, fy);
            double px = fx, py = fy;
            int have_ctl = 0;
            double cx = 0, cy = 0;
            for (int k = 0; k < cnt + 1; k++) {
                int i = k < cnt ? s + ((first - s + k) % (e - s + 1)) : -1;
                double x, y;
                int on;
                if (i < 0) { x = fx; y = fy; on = 1; }
                else { x = xs[i]; y = ys[i]; on = fl[i] & 1; }
                if (on) {
                    if (have_ctl) pen_quad(pen, cx, cy, x, y); else pen_line(pen, x, y);
                    have_ctl = 0;
                } else {
                    if (have_ctl) { double mx = (cx + x) / 2, my = (cy + y) / 2; pen_quad(pen, cx, cy, mx, my); }
                    cx = x; cy = y; have_ctl = 1;
                }
                px = x; py = y;
            }
            (void)px; (void)py;
            pen_close(pen);
            s = e + 1;
        }
        free(fl); free(xs); free(ys);
        return;
    }
    /* composto */
    const u8 *p = g + 10;
    for (int guard = 0; guard < 64 && p + 4 <= end; guard++) {
        u32 fl = g16(p), cg = g16(p + 2);
        p += 4;
        double dx, dy, a2 = 1, b2 = 0, c2 = 0, d2 = 1;
        if (fl & 1) { if (p + 4 > end) break; dx = gs16(p); dy = gs16(p + 2); p += 4; }
        else { if (p + 2 > end) break; dx = (int8_t)p[0]; dy = (int8_t)p[1]; p += 2; }
        if (fl & 8) { a2 = d2 = gs16(p) / 16384.0; p += 2; }
        else if (fl & 0x40) { a2 = gs16(p) / 16384.0; d2 = gs16(p + 2) / 16384.0; p += 4; }
        else if (fl & 0x80) { a2 = gs16(p) / 16384.0; b2 = gs16(p + 2) / 16384.0; c2 = gs16(p + 4) / 16384.0; d2 = gs16(p + 6) / 16384.0; p += 8; }
        if (!(fl & 2)) dx = dy = 0;                          /* pontos casados: não suportado; sem deslocamento */
        double cm[6] = {a2 * m[0] + b2 * m[2], a2 * m[1] + b2 * m[3], c2 * m[0] + d2 * m[2], c2 * m[1] + d2 * m[3],
                        dx * m[0] + dy * m[2] + m[4], dx * m[1] + dy * m[3] + m[5]};
        tt_outline(t, (int)cg, pen, cm, depth + 1);
        if (!(fl & 0x20)) break;
    }
}
/* cmap: plataforma/codificação → glifo (formatos 0, 4, 6, 12) */
static int tt_cmap(const tt_font *t, int plat, int enc, u32 code) {
    if (!t->cmap || t->cmapn < 4) return -1;
    u32 n = g16(t->cmap + 2);
    for (u32 i = 0; i < n && 4 + (size_t)i * 8 + 8 <= t->cmapn; i++) {
        const u8 *e = t->cmap + 4 + i * 8;
        if ((int)g16(e) != plat || (int)g16(e + 2) != enc) continue;
        u32 off = g32(e + 4);
        if (off + 6 > t->cmapn) continue;
        const u8 *s = t->cmap + off, *end = t->cmap + t->cmapn;
        u32 fmt = g16(s);
        if (fmt == 0) return code < 256 && s + 6 + code < end ? s[6 + code] : 0;
        if (fmt == 6) { u32 first = g16(s + 6), cnt = g16(s + 8); return code >= first && code < first + cnt && s + 10 + (code - first) * 2 + 2 <= end ? (int)g16(s + 10 + (code - first) * 2) : 0; }
        if (fmt == 4) {
            u32 seg = g16(s + 6) / 2;
            const u8 *ends = s + 14, *starts = ends + seg * 2 + 2, *deltas = starts + seg * 2, *ranges = deltas + seg * 2;
            if (ranges + seg * 2 > end) return 0;
            for (u32 k = 0; k < seg; k++) {
                u32 ee = g16(ends + k * 2), ss = g16(starts + k * 2);
                if (code > ee) continue;
                if (code < ss) return 0;
                u32 dd = g16(deltas + k * 2), rr = g16(ranges + k * 2);
                if (!rr) return (int)((code + dd) & 0xFFFF);
                const u8 *gp = ranges + k * 2 + rr + (code - ss) * 2;
                if (gp + 2 > end) return 0;
                u32 gv = g16(gp);
                return gv ? (int)((gv + dd) & 0xFFFF) : 0;
            }
            return 0;
        }
        if (fmt == 12) {
            u32 ng = g32(s + 12);
            for (u32 k = 0; k < ng && s + 16 + k * 12 + 12 <= end; k++) {
                const u8 *gg = s + 16 + k * 12;
                if (code >= g32(gg) && code <= g32(gg + 4)) return (int)(g32(gg + 8) + code - g32(gg));
            }
            return 0;
        }
    }
    return -1;
}
/* post formato 2: nome de glifo → gid */
static int tt_post_gid(const tt_font *t, const char *name) {
    if (!t->post || t->postn < 34 || g32(t->post) != 0x00020000) return -1;
    u32 ng = g16(t->post + 32);
    const u8 *idx = t->post + 34, *names = idx + ng * 2, *end = t->post + t->postn;
    if (names > end) return -1;
    /* nomes Macintosh padrão (os 258) não são listados: só os personalizados (índice ≥ 258) */
    const u8 *strs[1024];
    u32 nstr = 0;
    for (const u8 *p = names; p < end && nstr < 1024; p += 1 + *p) strs[nstr++] = p;
    size_t L = strlen(name);
    for (u32 g = 0; g < ng; g++) {
        u32 k = g16(idx + g * 2);
        if (k >= 258 && k - 258 < nstr) {
            const u8 *s = strs[k - 258];
            if (*s == L && !memcmp(s + 1, name, L)) return (int)g;
        }
    }
    return -1;
}
static double tt_advance(const tt_font *t, int gid) {
    if (!t->hmtx || t->nhm <= 0) return 0;
    int i = gid < t->nhm ? gid : t->nhm - 1;
    return (size_t)i * 4 + 2 <= t->hmtxn ? g16(t->hmtx + i * 4) : 0;
}

/* ---------------------------------------------------------------- CFF */
typedef struct { const u8 *d; size_t n; u32 count; int offsz; size_t offs, data; } cff_index;
static size_t cff_off(const u8 *p, int sz) { size_t v = 0; for (int i = 0; i < sz; i++) v = v << 8 | p[i]; return v; }
static size_t cff_index_at(const u8 *d, size_t n, size_t pos, cff_index *ix) {
    memset(ix, 0, sizeof *ix);
    ix->d = d; ix->n = n;
    if (pos + 2 > n) return n;
    ix->count = g16(d + pos);
    if (!ix->count) return pos + 2;
    if (pos + 3 > n) return n;
    ix->offsz = d[pos + 2];
    if (ix->offsz < 1 || ix->offsz > 4) { ix->count = 0; return n; }
    ix->offs = pos + 3;
    ix->data = ix->offs + (size_t)(ix->count + 1) * ix->offsz - 1;
    if (ix->offs + (size_t)(ix->count + 1) * ix->offsz > n) { ix->count = 0; return n; }
    return ix->data + cff_off(d + ix->offs + (size_t)ix->count * ix->offsz, ix->offsz);
}
static const u8 *cff_item(const cff_index *ix, u32 i, size_t *len) {
    if (i >= ix->count) return 0;
    size_t a = cff_off(ix->d + ix->offs + (size_t)i * ix->offsz, ix->offsz), b = cff_off(ix->d + ix->offs + (size_t)(i + 1) * ix->offsz, ix->offsz);
    if (b < a || ix->data + b > ix->n) return 0;
    *len = b - a;
    return ix->d + ix->data + a;
}
/* DICT: operador → operandos (até 48 por operador) */
typedef struct { int op; double v[48]; int n; } cff_entry;
static int cff_dict(const u8 *p, size_t n, cff_entry *out, int max) {
    double st[48];
    int sp = 0, ne = 0;
    for (size_t i = 0; i < n;) {
        int b0 = p[i];
        if (b0 <= 21) {
            int op = b0;
            i++;
            if (b0 == 12 && i < n) op = 1200 + p[i++];
            if (ne < max) { out[ne].op = op; out[ne].n = sp; memcpy(out[ne].v, st, sizeof(double) * (size_t)sp); ne++; }
            sp = 0;
            continue;
        }
        double v;
        if (b0 == 28 && i + 2 < n) { v = (int16_t)(p[i + 1] << 8 | p[i + 2]); i += 3; }
        else if (b0 == 29 && i + 4 < n) { v = (int32_t)g32(p + i + 1); i += 5; }
        else if (b0 == 30) {                                 /* real em BCD */
            char buf[64];
            int k = 0, done = 0;
            i++;
            while (i < n && !done) {
                for (int h = 0; h < 2 && !done; h++) {
                    int nib = h ? p[i] & 15 : p[i] >> 4;
                    if (nib <= 9) { if (k < 60) buf[k++] = (char)('0' + nib); }
                    else if (nib == 0xA) { if (k < 60) buf[k++] = '.'; }
                    else if (nib == 0xB) { if (k < 59) { buf[k++] = 'E'; } }
                    else if (nib == 0xC) { if (k < 58) { buf[k++] = 'E'; buf[k++] = '-'; } }
                    else if (nib == 0xE) { if (k < 60) buf[k++] = '-'; }
                    else if (nib == 0xF) done = 1;
                }
                i++;
            }
            buf[k] = 0;
            /* conversão manual (sem strtod no freestanding) */
            double m = 0, f = 0.1, e = 0;
            int neg = 0, dot = 0, ex = 0, eneg = 0;
            for (int j = 0; buf[j]; j++) {
                char c = buf[j];
                if (c == '-' && !ex && j == 0) neg = 1;
                else if (c == '-' && ex) eneg = 1;
                else if (c == '.') dot = 1;
                else if (c == 'E') ex = 1;
                else if (c >= '0' && c <= '9') { if (ex) e = e * 10 + (c - '0'); else if (dot) { m += (c - '0') * f; f /= 10; } else m = m * 10 + (c - '0'); }
            }
            v = m * pow(10, eneg ? -e : e);
            if (neg) v = -v;
        } else if (b0 >= 32 && b0 <= 246) { v = b0 - 139; i++; }
        else if (b0 >= 247 && b0 <= 250 && i + 1 < n) { v = (b0 - 247) * 256 + p[i + 1] + 108; i += 2; }
        else if (b0 >= 251 && b0 <= 254 && i + 1 < n) { v = -(b0 - 251) * 256 - p[i + 1] - 108; i += 2; }
        else { i++; continue; }
        if (sp < 48) st[sp++] = v;
    }
    return ne;
}
static const cff_entry *cff_find(const cff_entry *e, int n, int op) { for (int i = 0; i < n; i++) if (e[i].op == op) return &e[i]; return 0; }
typedef struct { cff_index lsubrs; int has; double nominal, dflt; } cff_priv;
typedef struct {
    const u8 *d; size_t n;
    cff_index chars, gsubrs, strings, fdarray;
    cff_priv priv, fdpriv[256];
    int nfd, cid, charset_fmt;
    size_t charset, fdselect;
    double fm[6];
    int nglyphs;
} cff_font;
static void cff_load_priv(cff_font *f, const cff_entry *pe, cff_priv *pr) {
    if (!pe || pe->n < 2) return;
    size_t sz = (size_t)pe->v[0], off = (size_t)pe->v[1];
    if (off + sz > f->n) return;
    cff_entry e[64];
    int ne = cff_dict(f->d + off, sz, e, 64);
    const cff_entry *s = cff_find(e, ne, 19), *nm = cff_find(e, ne, 21), *df = cff_find(e, ne, 20);
    if (s && s->n) { cff_index_at(f->d, f->n, off + (size_t)s->v[0], &pr->lsubrs); }
    pr->nominal = nm && nm->n ? nm->v[0] : 0;
    pr->dflt = df && df->n ? df->v[0] : 0;
    pr->has = 1;
}
static int cff_open(cff_font *f, const u8 *d, size_t n) {
    memset(f, 0, sizeof *f);
    if (n < 4 || d[0] != 1) return -1;
    f->d = d; f->n = n;
    cff_index names, tops;
    size_t p = cff_index_at(d, n, d[2], &names);
    p = cff_index_at(d, n, p, &tops);
    p = cff_index_at(d, n, p, &f->strings);
    cff_index_at(d, n, p, &f->gsubrs);
    size_t tl;
    const u8 *top = cff_item(&tops, 0, &tl);
    if (!top) return -1;
    cff_entry e[64];
    int ne = cff_dict(top, tl, e, 64);
    const cff_entry *cs = cff_find(e, ne, 17), *fm = cff_find(e, ne, 1207), *ros = cff_find(e, ne, 1230), *ch = cff_find(e, ne, 15);
    if (!cs || !cs->n) return -1;
    cff_index_at(d, n, (size_t)cs->v[0], &f->chars);
    f->nglyphs = (int)f->chars.count;
    double dm[6] = {0.001, 0, 0, 0.001, 0, 0};
    if (fm && fm->n >= 6) memcpy(dm, fm->v, sizeof dm);
    memcpy(f->fm, dm, sizeof dm);
    f->charset = ch && ch->n ? (size_t)ch->v[0] : 0;
    cff_load_priv(f, cff_find(e, ne, 18), &f->priv);
    if (ros) {
        f->cid = 1;
        const cff_entry *fda = cff_find(e, ne, 1236), *fds = cff_find(e, ne, 1237);
        if (fda && fda->n) {
            cff_index_at(d, n, (size_t)fda->v[0], &f->fdarray);
            f->nfd = (int)(f->fdarray.count < 256 ? f->fdarray.count : 256);
            for (int i = 0; i < f->nfd; i++) {
                size_t fl;
                const u8 *fd = cff_item(&f->fdarray, (u32)i, &fl);
                if (!fd) continue;
                cff_entry fe[32];
                int nf = cff_dict(fd, fl, fe, 32);
                cff_load_priv(f, cff_find(fe, nf, 18), &f->fdpriv[i]);
            }
        }
        f->fdselect = fds && fds->n ? (size_t)fds->v[0] : 0;
    }
    return 0;
}
static int cff_fd_of(const cff_font *f, int gid) {
    if (!f->fdselect || f->fdselect >= f->n) return 0;
    const u8 *s = f->d + f->fdselect;
    if (s[0] == 0) return f->fdselect + 1 + (size_t)gid < f->n ? s[1 + gid] : 0;
    if (s[0] == 3 && f->fdselect + 3 <= f->n) {
        u32 nr = g16(s + 1);
        for (u32 i = 0; i < nr && f->fdselect + 3 + (size_t)i * 3 + 5 <= f->n; i++) {
            u32 first = g16(s + 3 + i * 3), next = g16(s + 3 + i * 3 + 3);
            if ((u32)gid >= first && (u32)gid < next) return s[3 + i * 3 + 2];
        }
    }
    return 0;
}
/* charset: gid → SID (fonte normal) ou CID (CID-keyed); a busca inversa devolve o gid */
static int cff_charset_lookup(const cff_font *f, u32 want, int by_gid, u32 *out) {
    if (!f->charset) { if (by_gid) { *out = want; return 1; } *out = want; return want < (u32)f->nglyphs; }   /* ISOAdobe: identidade */
    if (f->charset < 3) { *out = want; return 1; }
    const u8 *s = f->d + f->charset, *end = f->d + f->n;
    int fmt = s[0];
    u32 gid = 1;
    const u8 *p = s + 1;
    if (by_gid && want == 0) { *out = 0; return 1; }
    while (gid < (u32)f->nglyphs && p < end) {
        if (fmt == 0) {
            if (p + 2 > end) break;
            u32 sid = g16(p);
            p += 2;
            if (by_gid ? gid == want : sid == want) { *out = by_gid ? sid : gid; return 1; }
            gid++;
        } else {
            if (p + (fmt == 1 ? 3 : 4) > end) break;
            u32 first = g16(p), left = fmt == 1 ? p[2] : g16(p + 2);
            p += fmt == 1 ? 3 : 4;
            if (by_gid ? (want >= gid && want <= gid + left) : (want >= first && want <= first + left)) {
                *out = by_gid ? first + (want - gid) : gid + (want - first);
                return 1;
            }
            gid += left + 1;
        }
    }
    return 0;
}
static const char *const cff_std_strings[] = {
    ".notdef", "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand", "quoteright", "parenleft", "parenright",
    "asterisk", "plus", "comma", "hyphen", "period", "slash", "zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
    "nine", "colon", "semicolon", "less", "equal", "greater", "question", "at", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K",
    "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "bracketleft", "backslash", "bracketright",
    "asciicircum", "underscore", "quoteleft", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r",
    "s", "t", "u", "v", "w", "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde", "exclamdown", "cent", "sterling",
    "fraction", "yen", "florin", "section", "currency", "quotesingle", "quotedblleft", "guillemotleft", "guilsinglleft",
    "guilsinglright", "fi", "fl", "endash", "dagger", "daggerdbl", "periodcentered", "paragraph", "bullet", "quotesinglbase",
    "quotedblbase", "quotedblright", "guillemotright", "ellipsis", "perthousand", "questiondown", "grave", "acute", "circumflex",
    "tilde", "macron", "breve", "dotaccent", "dieresis", "ring", "cedilla", "hungarumlaut", "ogonek", "caron", "emdash", "AE",
    "ordfeminine", "Lslash", "Oslash", "OE", "ordmasculine", "ae", "dotlessi", "lslash", "oslash", "oe", "germandbls", "onesuperior",
    "logicalnot", "mu", "trademark", "Eth", "onehalf", "plusminus", "Thorn", "onequarter", "divide", "brokenbar", "degree", "thorn",
    "threequarters", "twosuperior", "registered", "minus", "eth", "multiply", "threesuperior", "copyright", "Aacute", "Acircumflex",
    "Adieresis", "Agrave", "Aring", "Atilde", "Ccedilla", "Eacute", "Ecircumflex", "Edieresis", "Egrave", "Iacute", "Icircumflex",
    "Idieresis", "Igrave", "Ntilde", "Oacute", "Ocircumflex", "Odieresis", "Ograve", "Otilde", "Scaron", "Uacute", "Ucircumflex",
    "Udieresis", "Ugrave", "Yacute", "Ydieresis", "Zcaron", "aacute", "acircumflex", "adieresis", "agrave", "aring", "atilde",
    "ccedilla", "eacute", "ecircumflex", "edieresis", "egrave", "iacute", "icircumflex", "idieresis", "igrave", "ntilde", "oacute",
    "ocircumflex", "odieresis", "ograve", "otilde", "scaron", "uacute", "ucircumflex", "udieresis", "ugrave", "yacute", "ydieresis",
    "zcaron"};
/* nome → gid pelo charset e pelas strings (padrão até 228 como na tabela; depois, a String INDEX) */
static int cff_name_gid(const cff_font *f, const char *name) {
    u32 sid = 0xFFFFFFFFu;
    for (u32 i = 0; i < sizeof cff_std_strings / sizeof cff_std_strings[0]; i++) if (!strcmp(cff_std_strings[i], name)) { sid = i; break; }
    if (sid == 0xFFFFFFFFu) {
        size_t L = strlen(name);
        for (u32 i = 0; i < f->strings.count; i++) {
            size_t sl;
            const u8 *s = cff_item(&f->strings, i, &sl);
            if (s && sl == L && !memcmp(s, name, L)) { sid = 391 + i; break; }
        }
    }
    if (sid == 0xFFFFFFFFu) return -1;
    u32 gid;
    return cff_charset_lookup(f, sid, 0, &gid) ? (int)gid : -1;
}
static int cff_cid_gid(const cff_font *f, u32 cid) {
    if (!f->cid) return (int)cid;
    u32 gid;
    return cff_charset_lookup(f, cid, 0, &gid) ? (int)gid : (cid == 0 ? 0 : -1);
}
static double cff_bias(u32 n) { return n < 1240 ? 107 : n < 33900 ? 1131 : 32768; }
typedef struct { const cff_font *f; const cff_priv *pr; pen_t *pen; double x, y, st[48]; int sp, nstems, width_done, done; double width; int depth; } t2_state;
static void t2_run(t2_state *s, const u8 *c, size_t n);
static void t2_run(t2_state *s, const u8 *c, size_t n) {
    if (s->depth > 10) return;
    s->depth++;
    for (size_t i = 0; i < n && !s->done;) {
        int b = c[i];
        if (b >= 32 || b == 28) {
            double v;
            if (b == 28) { if (i + 2 >= n) break; v = (int16_t)(c[i + 1] << 8 | c[i + 2]); i += 3; }
            else if (b <= 246) { v = b - 139; i++; }
            else if (b <= 250) { if (i + 1 >= n) break; v = (b - 247) * 256 + c[i + 1] + 108; i += 2; }
            else if (b <= 254) { if (i + 1 >= n) break; v = -(b - 251) * 256 - c[i + 1] - 108; i += 2; }
            else { if (i + 4 >= n) break; v = (int32_t)g32(c + i + 1) / 65536.0; i += 5; }
            if (s->sp < 48) s->st[s->sp++] = v;
            continue;
        }
        i++;
        double *a = s->st;
        int k = s->sp;
#define W(odd) do { if (!s->width_done) { if ((k & 1) == (odd)) { a++; k--; } s->width_done = 1; } } while (0)
        switch (b) {
        case 1: case 3: case 18: case 23: W(1); s->nstems += k / 2; s->sp = 0; break;   /* hstem vstem hstemhm vstemhm */
        case 19: case 20:                                   /* hintmask cntrmask */
            W(1); s->nstems += k / 2;
            i += (size_t)(s->nstems + 7) / 8;
            s->sp = 0;
            break;
        case 21: W(1); s->x += a[k - 2]; s->y += a[k - 1]; pen_move(s->pen, s->x, s->y); s->sp = 0; break;   /* rmoveto */
        case 22: if (!s->width_done) { if (k > 1) { a++; k--; } s->width_done = 1; } s->x += a[k - 1]; pen_move(s->pen, s->x, s->y); s->sp = 0; break;
        case 4: if (!s->width_done) { if (k > 1) { a++; k--; } s->width_done = 1; } s->y += a[k - 1]; pen_move(s->pen, s->x, s->y); s->sp = 0; break;
        case 5: for (int j = 0; j + 1 < k; j += 2) { s->x += a[j]; s->y += a[j + 1]; pen_line(s->pen, s->x, s->y); } s->sp = 0; break;
        case 6: case 7:                                     /* hlineto vlineto alternados */
            for (int j = 0; j < k; j++) {
                if ((j + (b == 7)) % 2 == 0) s->x += a[j]; else s->y += a[j];
                pen_line(s->pen, s->x, s->y);
            }
            s->sp = 0;
            break;
        case 8: case 24:                                    /* rrcurveto, rcurveline */
            {
                int j = 0, lim = b == 24 ? k - 2 : k;
                for (; j + 5 < lim + (b == 8 ? 0 : 0) && j + 6 <= lim; j += 6) {
                    double x1 = s->x + a[j], y1 = s->y + a[j + 1], x2 = x1 + a[j + 2], y2 = y1 + a[j + 3];
                    s->x = x2 + a[j + 4]; s->y = y2 + a[j + 5];
                    pen_cubic(s->pen, x1, y1, x2, y2, s->x, s->y);
                }
                if (b == 24 && j + 2 <= k) { s->x += a[j]; s->y += a[j + 1]; pen_line(s->pen, s->x, s->y); }
            }
            s->sp = 0;
            break;
        case 25:                                            /* rlinecurve */
            {
                int j = 0;
                for (; j + 2 <= k - 6; j += 2) { s->x += a[j]; s->y += a[j + 1]; pen_line(s->pen, s->x, s->y); }
                if (j + 6 <= k) {
                    double x1 = s->x + a[j], y1 = s->y + a[j + 1], x2 = x1 + a[j + 2], y2 = y1 + a[j + 3];
                    s->x = x2 + a[j + 4]; s->y = y2 + a[j + 5];
                    pen_cubic(s->pen, x1, y1, x2, y2, s->x, s->y);
                }
            }
            s->sp = 0;
            break;
        case 26: case 27:                                   /* vvcurveto hhcurveto */
            {
                int j = 0;
                double d0 = 0;
                if (k & 1) { d0 = a[0]; j = 1; }
                for (; j + 4 <= k; j += 4) {
                    double x1, y1;
                    if (b == 26) { x1 = s->x + d0; y1 = s->y + a[j]; } else { x1 = s->x + a[j]; y1 = s->y + d0; }
                    double x2 = x1 + a[j + 1], y2 = y1 + a[j + 2];
                    if (b == 26) { s->x = x2; s->y = y2 + a[j + 3]; } else { s->x = x2 + a[j + 3]; s->y = y2; }
                    pen_cubic(s->pen, x1, y1, x2, y2, s->x, s->y);
                    d0 = 0;
                }
            }
            s->sp = 0;
            break;
        case 30: case 31:                                   /* vhcurveto hvcurveto */
            {
                int horiz = b == 31;
                for (int j = 0; j + 4 <= k; j += 4) {
                    int last = j + 4 >= k - 1;
                    double x1, y1, x2, y2;
                    if (horiz) {
                        x1 = s->x + a[j]; y1 = s->y; x2 = x1 + a[j + 1]; y2 = y1 + a[j + 2];
                        s->y = y2 + a[j + 3]; s->x = x2 + (last && j + 5 == k ? a[j + 4] : 0);
                    } else {
                        x1 = s->x; y1 = s->y + a[j]; x2 = x1 + a[j + 1]; y2 = y1 + a[j + 2];
                        s->x = x2 + a[j + 3]; s->y = y2 + (last && j + 5 == k ? a[j + 4] : 0);
                    }
                    pen_cubic(s->pen, x1, y1, x2, y2, s->x, s->y);
                    horiz = !horiz;
                }
            }
            s->sp = 0;
            break;
        case 10: case 29: {                                 /* callsubr callgsubr */
            if (!k) break;
            const cff_index *ix = b == 10 ? &s->pr->lsubrs : &s->f->gsubrs;
            int idx = (int)a[k - 1] + (int)cff_bias(ix->count);
            s->sp--;
            size_t sl;
            const u8 *sub = cff_item(ix, (u32)idx, &sl);
            if (sub) t2_run(s, sub, sl);
            break;
        }
        case 11: s->depth--; return;                        /* return */
        case 14:                                            /* endchar */
            if (!s->width_done && (k == 1 || k == 5)) { a++; k--; s->width_done = 1; }
            pen_close(s->pen);
            s->done = 1;
            break;
        case 12: {
            if (i >= n) break;
            int e = c[i++];
            double dx[6], dy[6];
            switch (e) {
            case 35: if (k >= 13) { for (int q = 0; q < 6; q++) { dx[q] = a[2 * q]; dy[q] = a[2 * q + 1]; } goto flex; } break;
            case 34: if (k >= 7) { dx[0] = a[0]; dy[0] = 0; dx[1] = a[1]; dy[1] = a[2]; dx[2] = a[3]; dy[2] = 0; dx[3] = a[4]; dy[3] = 0; dx[4] = a[5]; dy[4] = -a[2]; dx[5] = a[6]; dy[5] = 0; goto flex; } break;
            case 36: if (k >= 9) { dx[0] = a[0]; dy[0] = a[1]; dx[1] = a[2]; dy[1] = a[3]; dx[2] = a[4]; dy[2] = 0; dx[3] = a[5]; dy[3] = 0; dx[4] = a[6]; dy[4] = a[7]; dx[5] = a[8]; dy[5] = -(a[1] + a[3] + a[7]); goto flex; } break;
            case 37: if (k >= 11) {
                double sx = 0, sy = 0;
                for (int q = 0; q < 5; q++) { dx[q] = a[2 * q]; dy[q] = a[2 * q + 1]; sx += dx[q]; sy += dy[q]; }
                if (fabs(sx) > fabs(sy)) { dx[5] = a[10]; dy[5] = -sy; } else { dx[5] = -sx; dy[5] = a[10]; }
                goto flex;
            } break;
            default: break;
            }
            s->sp = 0;
            break;
        flex: {
                double x1 = s->x + dx[0], y1 = s->y + dy[0], x2 = x1 + dx[1], y2 = y1 + dy[1], x3 = x2 + dx[2], y3 = y2 + dy[2];
                pen_cubic(s->pen, x1, y1, x2, y2, x3, y3);
                double x4 = x3 + dx[3], y4 = y3 + dy[3], x5 = x4 + dx[4], y5 = y4 + dy[4], x6 = x5 + dx[5], y6 = y5 + dy[5];
                pen_cubic(s->pen, x4, y4, x5, y5, x6, y6);
                s->x = x6; s->y = y6;
            }
            s->sp = 0;
            break;
        }
        default: s->sp = 0; break;
        }
#undef W
    }
    s->depth--;
}
static void cff_outline(const cff_font *f, int gid, pen_t *pen) {
    size_t n;
    const u8 *c = cff_item(&f->chars, (u32)gid, &n);
    if (!c) return;
    t2_state s;
    memset(&s, 0, sizeof s);
    s.f = f;
    s.pr = f->cid && f->nfd ? &f->fdpriv[cff_fd_of(f, gid) % (f->nfd ? f->nfd : 1)] : &f->priv;
    s.pen = pen;
    t2_run(&s, c, n);
    pen_close(pen);
}

/* ---------------------------------------------------------------- Type 1 */
typedef struct { u8 *subrs[1024]; size_t subrn[1024]; int nsubrs; char *names[4096]; u8 *cs[4096]; size_t csn[4096]; int ncs; int lenIV; double fm[6]; char *enc[256]; } t1_font;
static void t1_decrypt(u8 *d, size_t n, u16 r) {
    for (size_t i = 0; i < n; i++) { u8 c = d[i]; d[i] = (u8)(c ^ (r >> 8)); r = (u16)((c + r) * 52845 + 22719); }
}
static int t1_is_hex(const u8 *d, size_t n) {
    for (size_t i = 0; i < n && i < 4; i++) if (p_hexv(d[i]) < 0) return 0;
    return 1;
}
static void t1_free(t1_font *t) {
    for (int i = 0; i < t->nsubrs; i++) free(t->subrs[i]);
    for (int i = 0; i < t->ncs; i++) { free(t->names[i]); free(t->cs[i]); }
    for (int i = 0; i < 256; i++) free(t->enc[i]);
}
static size_t t1_skipws(const u8 *d, size_t n, size_t p) { while (p < n && (d[p] == ' ' || d[p] == '\t' || d[p] == '\r' || d[p] == '\n')) p++; return p; }
static long t1_int(const u8 *d, size_t n, size_t *p) {
    *p = t1_skipws(d, n, *p);
    long v = 0;
    int neg = 0;
    if (*p < n && d[*p] == '-') { neg = 1; (*p)++; }
    while (*p < n && d[*p] >= '0' && d[*p] <= '9') v = v * 10 + (d[(*p)++] - '0');
    return neg ? -v : v;
}
static int t1_open(t1_font *t, const u8 *d, size_t n, size_t len1) {
    memset(t, 0, sizeof *t);
    t->lenIV = 4;
    t->fm[0] = t->fm[3] = 0.001;
    /* PFB: tira os cabeçalhos de segmento */
    u8 *buf = 0;
    if (n > 6 && d[0] == 0x80) {
        buf = malloc(n);
        size_t k = 0, p = 0, l1 = 0;
        int seg = 0;
        while (buf && p + 6 <= n && d[p] == 0x80 && d[p + 1] != 3) {
            size_t sl = (size_t)d[p + 2] | (size_t)d[p + 3] << 8 | (size_t)d[p + 4] << 16 | (size_t)d[p + 5] << 24;
            p += 6;
            if (p + sl > n) sl = n - p;
            memcpy(buf + k, d + p, sl);
            k += sl;
            p += sl;
            if (seg++ == 0) l1 = k;
        }
        d = buf; n = k; len1 = l1;
    }
    /* cabeçalho em texto: /FontMatrix e /Encoding */
    size_t clear = len1 && len1 < n ? len1 : n;
    for (size_t p = 0; p + 11 < clear; p++) {
        if (!memcmp(d + p, "/FontMatrix", 11)) {
            size_t q = p + 11;
            while (q < clear && d[q] != '[' && d[q] != '{') q++;
            q++;
            for (int k = 0; k < 6; k++) {
                q = t1_skipws(d, clear, q);
                double v = 0, f = 0.1, sg = 1;
                int dot = 0;
                if (q < clear && d[q] == '-') { sg = -1; q++; }
                while (q < clear && ((d[q] >= '0' && d[q] <= '9') || d[q] == '.')) { if (d[q] == '.') dot = 1; else if (dot) { v += (d[q] - '0') * f; f /= 10; } else v = v * 10 + (d[q] - '0'); q++; }
                t->fm[k] = sg * v;
            }
        }
        if (p + 4 < clear && !memcmp(d + p, "dup ", 4)) {
            size_t q = p + 4;
            long code = t1_int(d, clear, &q);
            q = t1_skipws(d, clear, q);
            if (q < clear && d[q] == '/' && code >= 0 && code < 256) {
                size_t s = ++q;
                while (q < clear && d[q] > ' ' && d[q] != '/' && d[q] != '[' && d[q] != '{') q++;
                free(t->enc[code]);
                t->enc[code] = malloc(q - s + 1);
                if (t->enc[code]) { memcpy(t->enc[code], d + s, q - s); t->enc[code][q - s] = 0; }
            }
        }
    }
    /* parte cifrada */
    size_t es = len1 && len1 < n ? len1 : 0;
    if (!es) { for (size_t p = 0; p + 9 < n; p++) if (!memcmp(d + p, "eexec", 5)) { es = p + 5; break; } }
    while (es < n && (d[es] == '\r' || d[es] == '\n' || d[es] == ' ' || d[es] == '\t')) es++;
    if (!es || es >= n) { free(buf); return -1; }
    size_t en = n - es;
    u8 *e = malloc(en + 1);
    if (!e) { free(buf); return -1; }
    if (t1_is_hex(d + es, en)) {
        size_t k = 0;
        int hi = -1;
        for (size_t i = 0; i < en; i++) { int v = p_hexv(d[es + i]); if (v < 0) continue; if (hi < 0) hi = v; else { e[k++] = (u8)(hi << 4 | v); hi = -1; } }
        en = k;
    } else memcpy(e, d + es, en);
    t1_decrypt(e, en, 55665);
    for (size_t p = 4; p + 7 < en; p++) {
        if (!memcmp(e + p, "/lenIV", 6)) { size_t q = p + 6; t->lenIV = (int)t1_int(e, en, &q); }
        if (!memcmp(e + p, "/Subrs", 6)) {
            size_t q = p + 6;
            long cnt = t1_int(e, en, &q);
            for (long i = 0; i < cnt && q < en; i++) {
                while (q + 3 < en && memcmp(e + q, "dup", 3)) q++;
                q += 3;
                long idx = t1_int(e, en, &q), len = t1_int(e, en, &q);
                q = t1_skipws(e, en, q);
                while (q < en && e[q] != ' ') q++;                /* RD ou -| */
                q++;
                if (len < 0 || q + (size_t)len > en) break;
                if (idx >= 0 && idx < 1024) {
                    u8 *s = malloc((size_t)len);
                    if (s) { memcpy(s, e + q, (size_t)len); t1_decrypt(s, (size_t)len, 4330); t->subrs[idx] = s; t->subrn[idx] = (size_t)len; if (idx >= t->nsubrs) t->nsubrs = (int)idx + 1; }
                }
                q += (size_t)len;
            }
            p = q > p ? q - 1 : p;
        }
        if (!memcmp(e + p, "/CharStrings", 12)) {
            size_t q = p + 12;
            t1_int(e, en, &q);
            while (q < en && t->ncs < 4096) {
                while (q < en && e[q] != '/' && memcmp(e + q, "end", 3)) q++;
                if (q >= en || !memcmp(e + q, "end", 3)) break;
                size_t ns = ++q;
                while (q < en && e[q] > ' ') q++;
                size_t nl = q - ns;
                long len = t1_int(e, en, &q);
                q = t1_skipws(e, en, q);
                while (q < en && e[q] != ' ') q++;
                q++;
                if (len < 0 || q + (size_t)len > en) break;
                u8 *s = malloc((size_t)len);
                char *nm = malloc(nl + 1);
                if (!s || !nm) { free(s); free(nm); break; }
                memcpy(s, e + q, (size_t)len);
                t1_decrypt(s, (size_t)len, 4330);
                memcpy(nm, e + ns, nl);
                nm[nl] = 0;
                t->names[t->ncs] = nm; t->cs[t->ncs] = s; t->csn[t->ncs] = (size_t)len; t->ncs++;
                q += (size_t)len;
            }
            break;
        }
    }
    free(e);
    free(buf);
    return t->ncs ? 0 : -1;
}
static int t1_find(const t1_font *t, const char *name) { for (int i = 0; i < t->ncs; i++) if (!strcmp(t->names[i], name)) return i; return -1; }
typedef struct { const t1_font *t; pen_t *pen; double x, y, sbx, st[64], ps[16]; int sp, nps, flex, nflex; double fx[8], fy[8]; int done, depth; double width; } t1_state;
static void t1_run_glyph(const t1_font *t, int idx, pen_t *pen, double ox, double oy, int depth);
static void t1_run(t1_state *s, const u8 *c, size_t n) {
    if (s->depth > 10) return;
    s->depth++;
    for (size_t i = (size_t)s->t->lenIV < n ? (size_t)(s->t->lenIV >= 0 ? s->t->lenIV : 0) : n; i < n && !s->done;) {
        int b = c[i];
        if (b >= 32) {
            double v;
            if (b <= 246) { v = b - 139; i++; }
            else if (b <= 250) { if (i + 1 >= n) break; v = (b - 247) * 256 + c[i + 1] + 108; i += 2; }
            else if (b <= 254) { if (i + 1 >= n) break; v = -(b - 251) * 256 - c[i + 1] - 108; i += 2; }
            else { if (i + 4 >= n) break; v = (int32_t)g32(c + i + 1); i += 5; }
            if (s->sp < 64) s->st[s->sp++] = v;
            continue;
        }
        i++;
        double *a = s->st;
        int k = s->sp;
        switch (b) {
        case 13: if (k >= 2) { s->sbx = a[0]; s->x = a[0]; s->y = 0; s->width = a[1]; } s->sp = 0; break;   /* hsbw */
        case 9: pen_close(s->pen); s->sp = 0; break;                                                         /* closepath */
        case 21: if (k >= 2) { s->x += a[0]; s->y += a[1]; } if (s->flex) { if (s->nflex < 8) { s->fx[s->nflex] = s->x; s->fy[s->nflex] = s->y; s->nflex++; } } else pen_move(s->pen, s->x, s->y); s->sp = 0; break;
        case 22: if (k >= 1) s->x += a[0]; if (s->flex) { if (s->nflex < 8) { s->fx[s->nflex] = s->x; s->fy[s->nflex] = s->y; s->nflex++; } } else pen_move(s->pen, s->x, s->y); s->sp = 0; break;
        case 4: if (k >= 1) s->y += a[0]; if (s->flex) { if (s->nflex < 8) { s->fx[s->nflex] = s->x; s->fy[s->nflex] = s->y; s->nflex++; } } else pen_move(s->pen, s->x, s->y); s->sp = 0; break;
        case 5: if (k >= 2) { s->x += a[0]; s->y += a[1]; pen_line(s->pen, s->x, s->y); } s->sp = 0; break;
        case 6: if (k >= 1) { s->x += a[0]; pen_line(s->pen, s->x, s->y); } s->sp = 0; break;
        case 7: if (k >= 1) { s->y += a[0]; pen_line(s->pen, s->x, s->y); } s->sp = 0; break;
        case 8: if (k >= 6) { double x1 = s->x + a[0], y1 = s->y + a[1], x2 = x1 + a[2], y2 = y1 + a[3]; s->x = x2 + a[4]; s->y = y2 + a[5]; pen_cubic(s->pen, x1, y1, x2, y2, s->x, s->y); } s->sp = 0; break;
        case 30: if (k >= 4) { double x1 = s->x, y1 = s->y + a[0], x2 = x1 + a[1], y2 = y1 + a[2]; s->x = x2 + a[3]; s->y = y2; pen_cubic(s->pen, x1, y1, x2, y2, s->x, s->y); } s->sp = 0; break;
        case 31: if (k >= 4) { double x1 = s->x + a[0], y1 = s->y, x2 = x1 + a[1], y2 = y1 + a[2]; s->x = x2; s->y = y2 + a[3]; pen_cubic(s->pen, x1, y1, x2, y2, s->x, s->y); } s->sp = 0; break;
        case 1: case 3: s->sp = 0; break;                     /* hstem vstem */
        case 10:                                              /* callsubr */
            if (k >= 1) {
                int idx = (int)a[k - 1];
                s->sp--;
                if (idx >= 0 && idx < s->t->nsubrs && s->t->subrs[idx]) t1_run(s, s->t->subrs[idx], s->t->subrn[idx]);
            }
            break;
        case 11: s->depth--; return;
        case 14: pen_close(s->pen); s->done = 1; break;       /* endchar */
        case 12: {
            if (i >= n) break;
            int e = c[i++];
            if (e == 7) { if (k >= 4) { s->sbx = a[0]; s->x = a[0]; s->y = a[1]; s->width = a[2]; } s->sp = 0; }        /* sbw */
            else if (e == 12) { if (k >= 2) { a[k - 2] = a[k - 1] != 0 ? a[k - 2] / a[k - 1] : 0; s->sp--; } }        /* div */
            else if (e == 16) {                               /* callothersubr */
                if (k < 2) { s->sp = 0; break; }
                int oth = (int)a[k - 1], na = (int)a[k - 2];
                s->sp -= 2;
                if (na < 0 || na > s->sp) na = s->sp;
                s->nps = 0;
                for (int q = 0; q < na; q++) s->ps[s->nps++ < 16 ? s->nps - 1 : 15] = s->st[s->sp - 1 - q];
                s->sp -= na;
                if (oth == 1) { s->flex = 1; s->nflex = 0; }
                else if (oth == 0 && s->flex) {
                    /* fim do flex: 7 pontos (o primeiro é a referência) → 2 curvas */
                    s->flex = 0;
                    if (s->nflex >= 7) {
                        pen_cubic(s->pen, s->fx[1], s->fy[1], s->fx[2], s->fy[2], s->fx[3], s->fy[3]);
                        pen_cubic(s->pen, s->fx[4], s->fy[4], s->fx[5], s->fy[5], s->fx[6], s->fy[6]);
                        s->x = s->fx[6]; s->y = s->fy[6];
                    }
                    /* os dois valores devolvidos pelo pop são o ponto final */
                    s->nps = 2; s->ps[0] = s->y; s->ps[1] = s->x;
                } else if (oth == 3) { s->nps = 1; s->ps[0] = 3; }          /* troca de hints: devolve o número do subr */
            } else if (e == 17) {                             /* pop */
                if (s->nps > 0 && s->sp < 64) s->st[s->sp++] = s->ps[--s->nps];
            } else if (e == 33) { if (k >= 2) { s->x = a[0]; s->y = a[1]; } s->sp = 0; }   /* setcurrentpoint */
            else if (e == 6) {                                /* seac: base + acento (StandardEncoding) */
                if (k >= 5) {
                    double adx = a[1], ady = a[2];
                    int bc = (int)a[3], ac = (int)a[4];
                    const char *bn = bc >= 0 && bc < 256 ? enc_std[bc] : 0, *an = ac >= 0 && ac < 256 ? enc_std[ac] : 0;
                    int bi = bn ? t1_find(s->t, bn) : -1, ai = an ? t1_find(s->t, an) : -1;
                    double sb = s->sbx;
                    if (bi >= 0) t1_run_glyph(s->t, bi, s->pen, 0, 0, s->depth + 1);
                    if (ai >= 0) t1_run_glyph(s->t, ai, s->pen, sb + adx - a[0], ady, s->depth + 1);   /* origem do acento: sb da base + adx − asb */
                }
                s->done = 1;
            } else s->sp = 0;                                 /* dotsection, vstem3, hstem3 */
            break;
        }
        default: s->sp = 0; break;
        }
    }
    s->depth--;
}
static void t1_run_glyph(const t1_font *t, int idx, pen_t *pen, double ox, double oy, int depth) {
    if (idx < 0 || idx >= t->ncs || depth > 4) return;
    /* desenha num pen próprio deslocado por (ox, oy) */
    t1_state s;
    memset(&s, 0, sizeof s);
    s.t = t;
    s.depth = depth;
    pen_t sub = {CGPathCreateMutable(), 0, 0, 0};
    s.pen = &sub;
    t1_run(&s, t->cs[idx], t->csn[idx]);
    pen_close(&sub);
    CGAffineTransform m = {1, 0, 0, 1, ox, oy};
    void CGPathAddPath(void *dst, const CGAffineTransform *m, const void *src);
    CGPathAddPath(pen->path, &m, sub.path);
    CGPathRelease(sub.path);
}
static double t1_outline(const t1_font *t, int idx, pen_t *pen) {
    t1_state s;
    memset(&s, 0, sizeof s);
    s.t = t;
    s.pen = pen;
    if (idx < 0 || idx >= t->ncs) return 0;
    t1_run(&s, t->cs[idx], t->csn[idx]);
    pen_close(pen);
    return s.width;
}
