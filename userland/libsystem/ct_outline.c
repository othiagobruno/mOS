/*
 * mOS Core Text — contornos vetoriais dos glifos das fontes do sistema e das registradas. O kernel entrega só o
 * raster; aqui as tabelas da face (GUI_FONT/GF_TABLE) são lidas uma vez e os contornos saem dos mesmos leitores
 * do PDF (pdf_glyph.inc): TrueType (glyf/loca, compostos) e CFF/OpenType (charstrings Type 2, CID-keyed).
 * Usado por CTFontCreatePathForGlyph, pelo texto num CGPDFContext e pela fonte de reserva do desenho de PDF.
 */
#include "foundation.h"
#include "libc.h"
#include <stdbool.h>
#include "../../kernel/include/compat_gui.h"

typedef double CGFloat;
typedef struct { CGFloat x, y; } CGPoint;
typedef struct { CGFloat width, height; } CGSize;
typedef struct { CGPoint origin; CGSize size; } CGRect;
typedef struct { CGFloat a, b, c, d, tx, ty; } CGAffineTransform;
double floor(double), fabs(double), sqrt(double), pow(double, double);
static int p_hexv(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
#ifdef CT_HOST          /* teste no host: tools/ct_outline_host.c serve as tabelas */
long ct_host_sys5(long a, long b, long c, long d, void *e, long f);
#undef SYS5
#define SYS5(a, b, c, d, e, f) ct_host_sys5((long)(a), (long)(b), (long)(c), (long)(d), (void *)(e), (long)(f))
#endif
#include "pdf_enc.inc"
#include "pdf_glyph.inc"

void *CGPathCreateCopyByTransformingPath(const void *p, const CGAffineTransform *m);

/* uma face: as tabelas num sfnt montado na memória (TrueType) ou a tabela CFF */
typedef struct {
    int state;               /* 0 não tentado, 1 TrueType, 2 CFF, -1 sem contornos */
    u8 *sfnt;
    size_t n;
    tt_font tt;
    cff_font cff;
    double em[6];            /* unidades da fonte → 1 em */
} ct_face;
static ct_face faces[96];

static u8 *ct_table(int face, u32 tag, size_t *len) {
    long n = SYS5(GUI_FONT, GF_TABLE, face, tag, 0, 0);
    if (n <= 0 || n > (16 << 20)) return 0;
    u8 *b = malloc((size_t)n);
    if (!b) return 0;
    long g = SYS5(GUI_FONT, GF_TABLE, face, tag, b, n);
    if (g <= 0) { free(b); return 0; }
    *len = (size_t)(g < n ? g : n);
    return b;
}
static void be32(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }
static void be16(u8 *p, u32 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }

static ct_face *ct_face_get(int face) {
    if (face < 0 || face >= (int)(sizeof faces / sizeof faces[0])) return 0;
    ct_face *f = &faces[face];
    if (f->state) return f->state > 0 ? f : 0;
    f->state = -1;
    size_t cl;
    u8 *cff = ct_table(face, 0x43464620u, &cl);                 /* 'CFF ' */
    if (cff) {
        if (cff_open(&f->cff, cff, cl) == 0) {
            f->sfnt = cff; f->n = cl; f->state = 2;
            memcpy(f->em, f->cff.fm, sizeof f->em);
            return f;
        }
        free(cff);
    }
    /* TrueType: monta um sfnt com as tabelas que o leitor usa */
    static const u32 tags[] = {0x676C7966u, 0x68656164u, 0x68686561u, 0x686D7478u, 0x6C6F6361u, 0x6D617870u};   /* ordenadas */
    u8 *tb[6] = {0};
    size_t tl[6] = {0}, total = 12 + 16 * 6;
    for (int i = 0; i < 6; i++) {
        tb[i] = ct_table(face, tags[i], &tl[i]);
        total += (tl[i] + 3) & ~(size_t)3;
    }
    if (tb[0] && tb[1] && tb[4]) {
        u8 *s = calloc(1, total);
        if (s) {
            be32(s, 0x00010000u); be16(s + 4, 6);
            size_t off = 12 + 16 * 6;
            for (int i = 0; i < 6; i++) {
                u8 *e = s + 12 + 16 * i;
                be32(e, tags[i]); be32(e + 8, (u32)off); be32(e + 12, (u32)tl[i]);
                if (tb[i]) memcpy(s + off, tb[i], tl[i]);
                off += (tl[i] + 3) & ~(size_t)3;
            }
            if (tt_open(&f->tt, s, total) == 0) {
                f->sfnt = s; f->n = total; f->state = 1;
                double u = 1.0 / (f->tt.upem > 0 ? f->tt.upem : 1000);
                double em[6] = {u, 0, 0, u, 0, 0};
                memcpy(f->em, em, sizeof em);
            } else free(s);
        }
    }
    for (int i = 0; i < 6; i++) free(tb[i]);
    return f->state > 0 ? f : 0;
}

/* contorno do glifo em unidades de 1 em; NULL se a face não tem contornos. Vazio (espaço) = caminho vazio. */
static void *ct_em_path(int face, int gid) {
    ct_face *f = ct_face_get(face);
    if (!f) return 0;
    pen_t pen = {CGPathCreateMutable(), 0, 0, 0};
    if (!pen.path) return 0;
    if (f->state == 1) { double id[6] = {1, 0, 0, 1, 0, 0}; tt_outline(&f->tt, gid, &pen, id, 0); }
    else cff_outline(&f->cff, gid, &pen);
    pen_close(&pen);
    CGAffineTransform m = {f->em[0], f->em[1], f->em[2], f->em[3], f->em[4], f->em[5]};
    void *r = CGPathCreateCopyByTransformingPath(pen.path, &m);
    CGPathRelease(pen.path);
    return r;
}
/* contorno no tamanho dado (pontos), depois a matriz do chamador (NULL = identidade) */
void *cx_font_glyph_path(int face, int gid, double size, const CGAffineTransform *matrix) {
    void *p = ct_em_path(face, gid);
    if (!p) return 0;
    CGAffineTransform m = {size, 0, 0, size, 0, 0};
    if (matrix) {
        const CGAffineTransform *t = matrix;
        CGAffineTransform r = {m.a * t->a + m.b * t->c, m.a * t->b + m.b * t->d, m.c * t->a + m.d * t->c, m.c * t->b + m.d * t->d,
                               m.tx * t->a + m.ty * t->c + t->tx, m.tx * t->b + m.ty * t->d + t->ty};
        m = r;
    }
    void *q = CGPathCreateCopyByTransformingPath(p, &m);
    CGPathRelease(p);
    return q;
}
/* avanço do glifo em milésimos de em (hmtx pelo kernel) */
double cx_font_glyph_advance(int face, int gid) {
    long r = SYS5(GUI_FONT, GF_ADVANCE, face, gid, 0, 0);
    if (r < 0) return 0;
    int units = (int)(r & 0xFFFF), upem = (int)((r >> 16) & 0xFFFF);
    return upem ? units * 1000.0 / upem : 0;
}
/* Unicode de um glifo: a cmap (3/10, 3/1 ou 0/x) invertida segmento a segmento, uma vez por face */
typedef struct { u32 *rev; int n; int done; } ct_rev;
static ct_rev revs[96];
u32 cx_font_glyph_unicode(int face, int gid) {
    if (face < 0 || face >= 96 || gid < 0) return 0;
    ct_rev *r = &revs[face];
    if (!r->done) {
        r->done = 1;
        size_t cl, ml;
        u8 *cm = ct_table(face, 0x636D6170u, &cl), *mx = ct_table(face, 0x6D617870u, &ml);
        int ng = mx && ml >= 6 ? (int)g16(mx + 4) : 0;
        free(mx);
        if (cm && ng > 0 && cl >= 4 && (r->rev = calloc((size_t)ng, 4))) {
            r->n = ng;
            /* subtabela preferida: 3/10, depois 3/1, depois Unicode (0/x) */
            const u8 *best = 0;
            int rank = 0;
            u32 nt = g16(cm + 2);
            for (u32 i = 0; i < nt && 4 + (size_t)i * 8 + 8 <= cl; i++) {
                const u8 *e = cm + 4 + i * 8;
                int pl = (int)g16(e), en = (int)g16(e + 2), k = pl == 3 && en == 10 ? 3 : pl == 3 && en == 1 ? 2 : pl == 0 ? 1 : 0;
                u32 off = g32(e + 4);
                if (k > rank && off + 6 <= cl) { rank = k; best = cm + off; }
            }
            const u8 *end = cm + cl;
#define SETREV(g, c) do { u32 g_ = (g), c_ = (c); if (g_ > 0 && g_ < (u32)ng && (!r->rev[g_] || c_ < r->rev[g_])) r->rev[g_] = c_; } while (0)
            u32 fmt = best ? g16(best) : 99;
            if (fmt == 0) for (u32 c = 0; c < 256 && best + 6 + c < end; c++) SETREV(best[6 + c], c);
            else if (fmt == 6) {
                u32 first = g16(best + 6), cnt = g16(best + 8);
                for (u32 c = 0; c < cnt && best + 10 + c * 2 + 2 <= end; c++) SETREV(g16(best + 10 + c * 2), first + c);
            } else if (fmt == 4) {
                u32 seg = g16(best + 6) / 2;
                const u8 *ends = best + 14, *starts = ends + seg * 2 + 2, *deltas = starts + seg * 2, *ranges = deltas + seg * 2;
                for (u32 k = 0; ranges + seg * 2 <= end && k < seg; k++) {
                    u32 ee = g16(ends + k * 2), ss = g16(starts + k * 2), dd = g16(deltas + k * 2), rr = g16(ranges + k * 2);
                    for (u32 c = ss; c <= ee && c != 0xFFFF; c++) {
                        u32 g;
                        if (!rr) g = (c + dd) & 0xFFFF;
                        else {
                            const u8 *gp = ranges + k * 2 + rr + (c - ss) * 2;
                            if (gp + 2 > end) break;
                            g = g16(gp);
                            if (g) g = (g + dd) & 0xFFFF;
                        }
                        SETREV(g, c);
                    }
                }
            } else if (fmt == 12) {
                u32 n12 = g32(best + 12);
                for (u32 k = 0; k < n12 && best + 16 + k * 12 + 12 <= end; k++) {
                    const u8 *gg = best + 16 + k * 12;
                    u32 c0 = g32(gg), c1 = g32(gg + 4), g0 = g32(gg + 8);
                    for (u32 c = c0; c <= c1 && c <= 0x10FFFF && g0 + (c - c0) < (u32)ng; c++) SETREV(g0 + (c - c0), c);
                }
            }
#undef SETREV
        }
        free(cm);
    }
    return r->rev && gid < r->n ? r->rev[gid] : 0;
}
/* nome do glifo pela tabela 'post' formato 2 (nomes próprios da fonte; os 258 padrão do Mac ficam de fora) */
int cx_font_glyph_post_name(int face, int gid, char *out, int cap) {
    size_t n;
    u8 *t = ct_table(face, 0x706F7374u, &n);
    int ok = 0;
    if (t && n >= 34 && g32(t) == 0x00020000u) {
        u32 ng = g16(t + 32);
        if (gid >= 0 && (u32)gid < ng && 34 + ng * 2 <= n) {
            u32 idx = g16(t + 34 + gid * 2);
            if (idx >= 258) {
                const u8 *p = t + 34 + ng * 2, *end = t + n;
                for (u32 k = 258; p < end; k++) {
                    u32 len = *p;
                    if (p + 1 + len > end) break;
                    if (k == idx) {
                        int m = (int)len < cap - 1 ? (int)len : cap - 1;
                        memcpy(out, p + 1, (size_t)m);
                        out[m] = 0;
                        ok = m > 0;
                        break;
                    }
                    p += 1 + len;
                }
            }
        }
    }
    free(t);
    return ok;
}
/* a face tem contornos? (1 TrueType, 2 CFF, 0 não) */
int cx_font_outline_kind(int face) { ct_face *f = ct_face_get(face); return f ? f->state : 0; }
#ifdef CT_HOST
int ct_host_cmap(const u8 *file, size_t n, u32 cp) {
    tt_font t;
    tt_open(&t, file, n);
    return tt_cmap(&t, 3, 1, cp);
}
#endif
