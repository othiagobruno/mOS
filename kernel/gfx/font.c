/* mOS — leitor e rasterizador TrueType (glyf/loca/cmap/hmtx/kern GPOS par-a-par) */
#include "gfx.h"

extern void raster_line(float *acc, int w, int h, float x0, float y0, float x1, float y1);
extern void raster_finish(const float *acc, int n, u8 *out);

extern const u8 font_regular_ttf[], font_regular_ttf_end[];
extern const u8 font_medium_ttf[], font_medium_ttf_end[];
extern const u8 font_semibold_ttf[], font_semibold_ttf_end[];
extern const u8 font_bold_ttf[], font_bold_ttf_end[];

struct font {
    const u8 *data;
    u32 size;
    u32 glyf, loca, hmtx, cmap_sub, gpos;
    u16 upem, nglyphs, nhmetrics;
    i16 loca_fmt, ascender, descender, linegap;
    u8 cmap_fmt;
    int id;
    /* kerning: pares (g1<<16|g2) → ajuste, ordenados */
    u32 *kern_keys;
    i16 *kern_vals;
    u32 kern_n;
};

static struct font fonts[FONT_COUNT];

static inline u16 be16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }
static inline i16 bes16(const u8 *p) { return (i16)be16(p); }
static inline u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

static u32 find_table(struct font *f, const char *tag) {
    u16 n = be16(f->data + 4);
    for (u16 i = 0; i < n; i++) {
        const u8 *r = f->data + 12 + i * 16;
        if (!memcmp(r, tag, 4)) return be32(r + 8);
    }
    return 0;
}

/* ------------------------------------------------------------ cmap */
static u16 glyph_index(struct font *f, u32 cp) {
    const u8 *t = f->data + f->cmap_sub;
    if (f->cmap_fmt == 12) {
        u32 n = be32(t + 12);
        u32 lo = 0, hi = n;
        while (lo < hi) {
            u32 mid = (lo + hi) / 2;
            const u8 *g = t + 16 + mid * 12;
            u32 s = be32(g), e = be32(g + 4);
            if (cp < s) hi = mid;
            else if (cp > e) lo = mid + 1;
            else return (u16)(be32(g + 8) + cp - s);
        }
        return 0;
    }
    if (f->cmap_fmt == 4) {
        if (cp > 0xFFFF) return 0;
        u16 segx2 = be16(t + 6);
        const u8 *ends = t + 14, *starts = ends + segx2 + 2, *deltas = starts + segx2, *ranges = deltas + segx2;
        for (u16 i = 0; i < segx2 / 2; i++) {
            if (be16(ends + i * 2) < cp) continue;
            u16 s = be16(starts + i * 2);
            if (s > cp) return 0;
            u16 ro = be16(ranges + i * 2);
            if (!ro) return (u16)(cp + bes16(deltas + i * 2));
            const u8 *gp = ranges + i * 2 + ro + (cp - s) * 2;
            u16 g = be16(gp);
            return g ? (u16)(g + bes16(deltas + i * 2)) : 0;
        }
    }
    return 0;
}

/* ------------------------------------------------------------ GPOS (kerning por pares) */
static int coverage_index(const u8 *cov, u16 g) {
    u16 fmt = be16(cov);
    if (fmt == 1) {
        u16 n = be16(cov + 2);
        int lo = 0, hi = n - 1;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            u16 v = be16(cov + 4 + mid * 2);
            if (v == g) return mid;
            if (v < g) lo = mid + 1; else hi = mid - 1;
        }
    } else if (fmt == 2) {
        u16 n = be16(cov + 2);
        for (u16 i = 0; i < n; i++) {
            const u8 *r = cov + 4 + i * 6;
            if (g >= be16(r) && g <= be16(r + 2)) return be16(r + 4) + (g - be16(r));
        }
    }
    return -1;
}

static u16 class_of(const u8 *cd, u16 g) {
    u16 fmt = be16(cd);
    if (fmt == 1) {
        u16 start = be16(cd + 2), n = be16(cd + 4);
        if (g >= start && g < start + n) return be16(cd + 6 + (g - start) * 2);
    } else if (fmt == 2) {
        u16 n = be16(cd + 2);
        int lo = 0, hi = n - 1;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            const u8 *r = cd + 4 + mid * 6;
            if (g < be16(r)) hi = mid - 1;
            else if (g > be16(r + 2)) lo = mid + 1;
            else return be16(r + 4);
        }
    }
    return 0;
}

static int vr_size(u16 fmt) { int n = 0; for (int i = 0; i < 8; i++) if (fmt & (1 << i)) n += 2; return n; }

/* ajuste de kerning em unidades da fonte, lendo os subtables PairPos direto do GPOS */
static int gpos_kern(struct font *f, u16 g1, u16 g2) {
    if (!f->gpos) return 0;
    const u8 *gp = f->data + f->gpos;
    const u8 *ll = gp + be16(gp + 8);
    u16 nl = be16(ll);
    for (u16 li = 0; li < nl; li++) {
        const u8 *lk = ll + be16(ll + 2 + li * 2);
        u16 type = be16(lk), nsub = be16(lk + 4);
        for (u16 si = 0; si < nsub; si++) {
            const u8 *st = lk + be16(lk + 6 + si * 2);
            u16 t = type;
            if (t == 9) { t = be16(st + 2); st = st + be32(st + 4); }
            if (t != 2) continue;
            u16 fmt = be16(st), vf1 = be16(st + 4), vf2 = be16(st + 6);
            int ci = coverage_index(st + be16(st + 2), g1);
            if (ci < 0) continue;
            int s1 = vr_size(vf1), s2 = vr_size(vf2);
            if (fmt == 1) {
                const u8 *ps = st + be16(st + 10 + ci * 2);
                u16 n = be16(ps);
                int rec = 2 + s1 + s2;
                int lo = 0, hi = n - 1;
                while (lo <= hi) {
                    int mid = (lo + hi) / 2;
                    const u8 *r = ps + 2 + mid * rec;
                    u16 sg = be16(r);
                    if (sg == g2) return (vf1 & 4) ? bes16(r + 2 + ((vf1 & 1) ? 2 : 0) + ((vf1 & 2) ? 2 : 0)) : 0;
                    if (sg < g2) lo = mid + 1; else hi = mid - 1;
                }
            } else if (fmt == 2) {
                u16 c1 = class_of(st + be16(st + 8), g1), c2 = class_of(st + be16(st + 10), g2);
                u16 n2 = be16(st + 14);
                const u8 *r = st + 16 + (c1 * n2 + c2) * (s1 + s2);
                if (vf1 & 4) {
                    int v = bes16(r + ((vf1 & 1) ? 2 : 0) + ((vf1 & 2) ? 2 : 0));
                    if (v) return v;
                }
            }
        }
    }
    return 0;
}

static int kern_cache_lookup(struct font *f, u16 a, u16 b);

/* ------------------------------------------------------------ init */
static void font_load(int id, const u8 *d, const u8 *e) {
    struct font *f = &fonts[id];
    f->data = d; f->size = (u32)(e - d); f->id = id;
    u32 head = find_table(f, "head"), maxp = find_table(f, "maxp"), hhea = find_table(f, "hhea");
    f->upem = be16(d + head + 18);
    f->loca_fmt = bes16(d + head + 50);
    f->nglyphs = be16(d + maxp + 4);
    f->ascender = bes16(d + hhea + 4);
    f->descender = bes16(d + hhea + 6);
    f->linegap = bes16(d + hhea + 8);
    f->nhmetrics = be16(d + hhea + 34);
    f->hmtx = find_table(f, "hmtx");
    f->loca = find_table(f, "loca");
    f->glyf = find_table(f, "glyf");
    f->gpos = find_table(f, "GPOS");
    u32 cmap = find_table(f, "cmap");
    u16 n = be16(d + cmap + 2);
    u32 best = 0;
    u8 bestfmt = 0;
    for (u16 i = 0; i < n; i++) {
        const u8 *r = d + cmap + 4 + i * 8;
        u16 pid = be16(r), eid = be16(r + 2);
        u32 off = cmap + be32(r + 4);
        u16 fmt = be16(d + off);
        if (fmt == 12 && ((pid == 3 && eid == 10) || pid == 0)) { best = off; bestfmt = 12; break; }
        if (fmt == 4 && ((pid == 3 && eid == 1) || pid == 0) && bestfmt != 12) { best = off; bestfmt = 4; }
    }
    f->cmap_sub = best;
    f->cmap_fmt = bestfmt;
    /* pré-calcula kerning para pares ASCII comuns (rápido em tempo de desenho) */
    u16 gl[95];
    for (int c = 32; c < 127; c++) gl[c - 32] = glyph_index(f, c);
    f->kern_keys = kmalloc(95 * 95 * 4);
    f->kern_vals = kmalloc(95 * 95 * 2);
    f->kern_n = 0;
    for (int a = 0; a < 95; a++)
        for (int b = 0; b < 95; b++) {
            int k = gpos_kern(f, gl[a], gl[b]);
            if (k) { f->kern_keys[f->kern_n] = (u32)gl[a] << 16 | gl[b]; f->kern_vals[f->kern_n++] = (i16)k; }
        }
    /* ordena para busca binária (insertion sort é suficiente para poucos milhares) */
    for (u32 i = 1; i < f->kern_n; i++) {
        u32 k = f->kern_keys[i]; i16 v = f->kern_vals[i];
        int j = (int)i - 1;
        while (j >= 0 && f->kern_keys[j] > k) { f->kern_keys[j + 1] = f->kern_keys[j]; f->kern_vals[j + 1] = f->kern_vals[j]; j--; }
        f->kern_keys[j + 1] = k; f->kern_vals[j + 1] = v;
    }
}

static int kern_cache_lookup(struct font *f, u16 a, u16 b) {
    u32 key = (u32)a << 16 | b;
    int lo = 0, hi = (int)f->kern_n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (f->kern_keys[mid] == key) return f->kern_vals[mid];
        if (f->kern_keys[mid] < key) lo = mid + 1; else hi = mid - 1;
    }
    return 0;
}

void font_init(void) {
    font_load(FONT_REGULAR, font_regular_ttf, font_regular_ttf_end);
    font_load(FONT_MEDIUM, font_medium_ttf, font_medium_ttf_end);
    font_load(FONT_SEMIBOLD, font_semibold_ttf, font_semibold_ttf_end);
    font_load(FONT_BOLD, font_bold_ttf, font_bold_ttf_end);
    kprintf("Fontes: Inter (%u glifos, %u pares de kerning)\n", fonts[0].nglyphs, fonts[0].kern_n);
}

font_t *font_get(int w) { return &fonts[CLAMP(w, 0, FONT_COUNT - 1)]; }

static int advance_units(struct font *f, u16 g) {
    if (g >= f->nhmetrics) g = f->nhmetrics - 1;
    return be16(f->data + f->hmtx + g * 4);
}

/* ------------------------------------------------------------ contornos */
typedef struct { float *pts; int n, cap; } lines_t; /* pares de pontos (x0,y0,x1,y1) */

static void add_line(lines_t *l, float x0, float y0, float x1, float y1) {
    if (l->n + 4 > l->cap) {
        l->cap = l->cap ? l->cap * 2 : 512;
        l->pts = krealloc(l->pts, l->cap * sizeof(float));
    }
    l->pts[l->n++] = x0; l->pts[l->n++] = y0; l->pts[l->n++] = x1; l->pts[l->n++] = y1;
}

static void add_quad(lines_t *l, float x0, float y0, float cx, float cy, float x1, float y1) {
    float dd = fabsf_(x0 - 2 * cx + x1) + fabsf_(y0 - 2 * cy + y1);
    int n = 1 + (int)fsqrt(dd * 4);
    if (n > 16) n = 16;
    float px = x0, py = y0;
    for (int i = 1; i <= n; i++) {
        float t = (float)i / n, mt = 1 - t;
        float x = mt * mt * x0 + 2 * mt * t * cx + t * t * x1;
        float y = mt * mt * y0 + 2 * mt * t * cy + t * t * y1;
        add_line(l, px, py, x, y);
        px = x; py = y;
    }
}

static u32 glyph_offset(struct font *f, u16 g, u32 *len) {
    if (g >= f->nglyphs) { *len = 0; return 0; }
    u32 a, b;
    if (f->loca_fmt == 0) { a = be16(f->data + f->loca + g * 2) * 2u; b = be16(f->data + f->loca + g * 2 + 2) * 2u; }
    else { a = be32(f->data + f->loca + g * 4); b = be32(f->data + f->loca + g * 4 + 4); }
    *len = b - a;
    return f->glyf + a;
}

/* transforma: x' = a*x + c*y + e, y' = b*x + d*y + f  (unidades da fonte) */
typedef struct { float a, b, c, d, e, f; } xform_t;

static void glyph_outline(struct font *fn, u16 g, xform_t m, float scale, float ox, float oy, lines_t *out, int depth) {
    u32 len;
    u32 off = glyph_offset(fn, g, &len);
    if (!len || depth > 4) return;
    const u8 *p = fn->data + off;
    i16 nc = bes16(p);
    if (nc < 0) {
        const u8 *q = p + 10;
        u16 flags;
        do {
            flags = be16(q); u16 gi = be16(q + 2); q += 4;
            float dx, dy;
            if (flags & 1) { dx = bes16(q); dy = bes16(q + 2); q += 4; }
            else { dx = (i8)q[0]; dy = (i8)q[1]; q += 2; }
            xform_t c = {1, 0, 0, 1, 0, 0};
            if (flags & 8) { c.a = c.d = bes16(q) / 16384.0f; q += 2; }
            else if (flags & 0x40) { c.a = bes16(q) / 16384.0f; c.d = bes16(q + 2) / 16384.0f; q += 4; }
            else if (flags & 0x80) { c.a = bes16(q) / 16384.0f; c.b = bes16(q + 2) / 16384.0f; c.c = bes16(q + 4) / 16384.0f; c.d = bes16(q + 6) / 16384.0f; q += 8; }
            if (!(flags & 2)) { dx = dy = 0; }
            c.e = dx; c.f = dy;
            xform_t r;
            r.a = m.a * c.a + m.c * c.b; r.b = m.b * c.a + m.d * c.b;
            r.c = m.a * c.c + m.c * c.d; r.d = m.b * c.c + m.d * c.d;
            r.e = m.a * c.e + m.c * c.f + m.e; r.f = m.b * c.e + m.d * c.f + m.f;
            glyph_outline(fn, gi, r, scale, ox, oy, out, depth + 1);
        } while (flags & 0x20);
        return;
    }
    const u8 *ends = p + 10;
    int npts = be16(ends + (nc - 1) * 2) + 1;
    u16 ilen = be16(ends + nc * 2);
    const u8 *fl = ends + nc * 2 + 2 + ilen;
    u8 *flags = kmalloc(npts);
    float *xs = kmalloc(npts * sizeof(float)), *ys = kmalloc(npts * sizeof(float));
    for (int i = 0; i < npts;) {
        u8 f = *fl++;
        flags[i++] = f;
        if (f & 8) { u8 r = *fl++; while (r-- && i < npts) flags[i++] = f; }
    }
    i32 v = 0;
    for (int i = 0; i < npts; i++) {
        u8 f = flags[i];
        if (f & 2) { v += (f & 16) ? *fl : -(i32)*fl; fl++; }
        else if (!(f & 16)) { v += bes16(fl); fl += 2; }
        xs[i] = (float)v;
    }
    v = 0;
    for (int i = 0; i < npts; i++) {
        u8 f = flags[i];
        if (f & 4) { v += (f & 32) ? *fl : -(i32)*fl; fl++; }
        else if (!(f & 32)) { v += bes16(fl); fl += 2; }
        ys[i] = (float)v;
    }
    for (int i = 0; i < npts; i++) {
        float x = xs[i], y = ys[i];
        xs[i] = (m.a * x + m.c * y + m.e) * scale + ox;
        ys[i] = oy - (m.b * x + m.d * y + m.f) * scale;
    }
    int start = 0;
    for (int c = 0; c < nc; c++) {
        int end = be16(ends + c * 2);
        int n = end - start + 1;
        if (n < 2) { start = end + 1; continue; }
        /* ponto inicial: um on-curve, ou o meio entre dois off */
        int first = -1;
        for (int k = 0; k < n; k++) if (flags[start + k] & 1) { first = k; break; }
        float sx, sy;
        if (first < 0) { sx = (xs[start] + xs[start + 1]) / 2; sy = (ys[start] + ys[start + 1]) / 2; first = 0; }
        else { sx = xs[start + first]; sy = ys[start + first]; }
        float px = sx, py = sy, cx = 0, cy = 0;
        bool has_ctrl = false;
        for (int k = 1; k <= n; k++) {
            int idx = start + (first + k) % n;
            float x = xs[idx], y = ys[idx];
            if (flags[idx] & 1) {
                if (has_ctrl) add_quad(out, px, py, cx, cy, x, y); else add_line(out, px, py, x, y);
                px = x; py = y; has_ctrl = false;
            } else {
                if (has_ctrl) {
                    float mx = (cx + x) / 2, my = (cy + y) / 2;
                    add_quad(out, px, py, cx, cy, mx, my);
                    px = mx; py = my;
                }
                cx = x; cy = y; has_ctrl = true;
            }
        }
        if (has_ctrl) add_quad(out, px, py, cx, cy, sx, sy);
        else if (px != sx || py != sy) add_line(out, px, py, sx, sy);
        start = end + 1;
    }
    kfree(flags); kfree(xs); kfree(ys);
}

/* ------------------------------------------------------------ cache de glifos */
typedef struct glyph {
    u32 key_cp;
    u16 key_size, key_font;
    u8 *bmp;
    i16 w, h, left, top;
    struct glyph *next;
} glyph_t;

#define GCACHE 4096
static glyph_t *gcache[GCACHE];
static u8 gamma_lut[256];
static bool gamma_ready;

static glyph_t *glyph_get(struct font *f, float size, u32 cp) {
    u16 sz = (u16)(size * 4);
    u32 h = (cp * 2654435761u ^ sz * 40503u ^ f->id * 97u) % GCACHE;
    for (glyph_t *g = gcache[h]; g; g = g->next)
        if (g->key_cp == cp && g->key_size == sz && g->key_font == f->id) return g;
    if (!gamma_ready) {
        for (int i = 0; i < 256; i++) gamma_lut[i] = (u8)(fpow(i / 255.0f, 0.78f) * 255 + 0.5f);
        gamma_ready = true;
    }
    glyph_t *g = kcalloc(1, sizeof(glyph_t));
    g->key_cp = cp; g->key_size = sz; g->key_font = (u16)f->id;
    u16 gi = glyph_index(f, cp);
    u32 len;
    u32 off = glyph_offset(f, gi, &len);
    if (len) {
        float scale = size / f->upem;
        const u8 *p = f->data + off;
        float xmin = bes16(p + 2) * scale, ymin = bes16(p + 4) * scale, xmax = bes16(p + 6) * scale, ymax = bes16(p + 8) * scale;
        int x0 = (int)ffloor(xmin) - 1, y0 = (int)ffloor(-ymax) - 1;
        int w = (int)fceil(xmax) - x0 + 2, hh = (int)fceil(-ymin) - y0 + 2;
        if (w > 0 && hh > 0 && w < 1024 && hh < 1024) {
            lines_t l = {0};
            xform_t id = {1, 0, 0, 1, 0, 0};
            glyph_outline(f, gi, id, scale, (float)-x0, (float)-y0, &l, 0);
            float *acc = kcalloc((usize)w * hh + 4, sizeof(float));
            for (int i = 0; i < l.n; i += 4) raster_line(acc, w, hh, l.pts[i], l.pts[i + 1], l.pts[i + 2], l.pts[i + 3]);
            g->bmp = kmalloc((usize)w * hh);
            raster_finish(acc, w * hh, g->bmp);
            for (int i = 0; i < w * hh; i++) g->bmp[i] = gamma_lut[g->bmp[i]];
            kfree(acc);
            kfree(l.pts);
            g->w = (i16)w; g->h = (i16)hh; g->left = (i16)x0; g->top = (i16)y0;
        }
    }
    g->next = gcache[h];
    gcache[h] = g;
    return g;
}

bool font_has_glyph(font_t *f, u32 cp) { return glyph_index(f, cp) != 0; }

/* ------------------------------------------------------------ UTF-8 */
u32 utf8_decode(const char **pp) {
    const u8 *p = (const u8 *)*pp;
    u32 c = *p++;
    if (c >= 0xF0 && (p[0] & 0xC0) == 0x80) { c = ((c & 7) << 18) | ((p[0] & 63) << 12) | ((p[1] & 63) << 6) | (p[2] & 63); p += 3; }
    else if (c >= 0xE0 && (p[0] & 0xC0) == 0x80) { c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63); p += 2; }
    else if (c >= 0xC0 && (p[0] & 0xC0) == 0x80) { c = ((c & 31) << 6) | (p[0] & 63); p += 1; }
    *pp = (const char *)p;
    return c;
}
int utf8_encode(u32 cp, char *o) {
    if (cp < 0x80) { o[0] = (char)cp; return 1; }
    if (cp < 0x800) { o[0] = (char)(0xC0 | cp >> 6); o[1] = (char)(0x80 | (cp & 63)); return 2; }
    if (cp < 0x10000) { o[0] = (char)(0xE0 | cp >> 12); o[1] = (char)(0x80 | ((cp >> 6) & 63)); o[2] = (char)(0x80 | (cp & 63)); return 3; }
    o[0] = (char)(0xF0 | cp >> 18); o[1] = (char)(0x80 | ((cp >> 12) & 63)); o[2] = (char)(0x80 | ((cp >> 6) & 63)); o[3] = (char)(0x80 | (cp & 63));
    return 4;
}
int utf8_prev(const char *s, int i) { if (i <= 0) return 0; i--; while (i > 0 && ((u8)s[i] & 0xC0) == 0x80) i--; return i; }
int utf8_next(const char *s, int i) { if (!s[i]) return i; i++; while (s[i] && ((u8)s[i] & 0xC0) == 0x80) i++; return i; }

/* ------------------------------------------------------------ medição e desenho */
static float kern_px(struct font *f, float size, u16 a, u16 b) {
    if (!a || !b) return 0;
    int k = kern_cache_lookup(f, a, b);
    return k ? k * size / f->upem : 0;
}

int text_width_n(font_t *f, float size, const char *s, int bytes) {
    float x = 0;
    const char *e = bytes < 0 ? NULL : s + bytes;
    u16 prev = 0;
    while (*s && (!e || s < e)) {
        u32 cp = utf8_decode(&s);
        if (cp == '\t') cp = ' ';
        u16 g = glyph_index(f, cp);
        x += kern_px(f, size, prev, g);
        x += advance_units(f, g) * size / f->upem;
        prev = g;
    }
    return (int)(x + 0.5f);
}
int text_width(font_t *f, float size, const char *s) { return text_width_n(f, size, s, -1); }

int text_ascent(font_t *f, float size) { return (int)(f->ascender * size / f->upem + 0.5f); }
int text_descent(font_t *f, float size) { return (int)(-f->descender * size / f->upem + 0.5f); }

int text_draw_n(surface_t *s, font_t *f, float size, int x, int baseline, u32 color, const char *t, int bytes) {
    float pen = (float)x;
    const char *e = bytes < 0 ? NULL : t + bytes;
    u16 prev = 0;
    while (*t && (!e || t < e)) {
        u32 cp = utf8_decode(&t);
        if (cp == '\t') cp = ' ';
        if (cp == '\n') continue;
        u16 gi = glyph_index(f, cp);
        pen += kern_px(f, size, prev, gi);
        prev = gi;
        if (cp != ' ') {
            glyph_t *g = glyph_get(f, size, cp);
            if (g->bmp) gfx_mask(s, (int)(pen + 0.5f) + g->left, baseline + g->top, g->bmp, g->w, g->h, g->w, color);
        }
        pen += advance_units(f, gi) * size / f->upem;
    }
    return (int)(pen - x + 0.5f);
}

int text_draw(surface_t *s, font_t *f, float size, int x, int baseline, u32 color, const char *t) {
    return text_draw_n(s, f, size, x, baseline, color, t, -1);
}

void text_draw_center(surface_t *s, font_t *f, float size, rect_t r, u32 color, const char *t) {
    int w = text_width(f, size, t);
    int asc = text_ascent(f, size), desc = text_descent(f, size);
    int base = r.y + (r.h - (asc + desc)) / 2 + asc;
    text_draw(s, f, size, r.x + (r.w - w) / 2, base, color, t);
}

int text_draw_ellipsis(surface_t *s, font_t *f, float size, int x, int baseline, int maxw, u32 color, const char *t) {
    int w = text_width(f, size, t);
    if (w <= maxw) return text_draw(s, f, size, x, baseline, color, t);
    int ew = text_width(f, size, "…");
    int n = 0, len = (int)strlen(t);
    while (n < len) {
        int nn = utf8_next(t, n);
        if (text_width_n(f, size, t, nn) + ew > maxw) break;
        n = nn;
    }
    int dx = text_draw_n(s, f, size, x, baseline, color, t, n);
    return dx + text_draw(s, f, size, x + dx, baseline, color, "…");
}

int text_index_at(font_t *f, float size, const char *t, int px) {
    int i = 0, len = (int)strlen(t);
    while (i < len) {
        int n = utf8_next(t, i);
        int a = text_width_n(f, size, t, i), b = text_width_n(f, size, t, n);
        if (px < (a + b) / 2) return i;
        i = n;
    }
    return len;
}

/* quebra de linha por palavras; devolve a altura usada */
int text_wrap(surface_t *s, font_t *f, float size, rect_t r, int lh, u32 color, const char *t, bool draw) {
    int y = r.y + text_ascent(f, size);
    const char *line = t;
    while (*line) {
        const char *p = line, *brk = NULL;
        while (*p && *p != '\n') {
            const char *q = p;
            utf8_decode(&q);
            if (text_width_n(f, size, line, (int)(q - line)) > r.w) break;
            if (*p == ' ') brk = p;
            p = q;
        }
        const char *end = p;
        if (*p && *p != '\n' && brk) end = brk;
        if (end == line && *p && *p != '\n') { utf8_decode(&p); end = p; }
        if (draw && s) text_draw_n(s, f, size, r.x, y, color, line, (int)(end - line));
        y += lh;
        line = end;
        if (*line == ' ' || *line == '\n') line++;
    }
    return y - r.y - text_ascent(f, size);
}
