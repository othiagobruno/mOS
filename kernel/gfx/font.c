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
/* fonte de fallback (CJK e outros alfabetos que a Inter não cobre), carregada do disco em font_load_fallback */
static struct font fallback;

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
static u32 find_table_len(struct font *f, const char *tag, u32 *len) {
    u16 n = be16(f->data + 4);
    for (u16 i = 0; i < n; i++) {
        const u8 *r = f->data + 12 + i * 16;
        if (!memcmp(r, tag, 4)) { if (len) *len = be32(r + 12); return be32(r + 8); }
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
/* kern = false nas fontes dos apps: o GPOS não passa por font_sane, então o leitor nem olha para ele
 * (o kerning delas é do HarfBuzz do app, que lê as tabelas por GF_TABLE) */
static void font_load_into(struct font *f, int id, const u8 *d, const u8 *e, bool kern) {
    /* (f vem do chamador) */
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
    if (!kern) { f->gpos = 0; f->kern_keys = NULL; f->kern_vals = NULL; f->kern_n = 0; return; }
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

static void font_load(int id, const u8 *d, const u8 *e) { font_load_into(&fonts[id], id, d, e, true); }

void font_init(void) {
    font_load(FONT_REGULAR, font_regular_ttf, font_regular_ttf_end);
    font_load(FONT_MEDIUM, font_medium_ttf, font_medium_ttf_end);
    font_load(FONT_SEMIBOLD, font_semibold_ttf, font_semibold_ttf_end);
    font_load(FONT_BOLD, font_bold_ttf, font_bold_ttf_end);
    kprintf("Fontes: Inter (%u glifos, %u pares de kerning)\n", fonts[0].nglyphs, fonts[0].kern_n);
}

font_t *font_get(int w) { return &fonts[CLAMP(w, 0, FONT_COUNT - 1)]; }

/* TrueType com glyf/loca/cmap (não CFF): confere as tabelas antes de usar dados vindos do disco */
static bool font_valid(const u8 *d, u32 n) {
    if (!d || n < 12) return false;
    u32 tag = (u32)d[0] << 24 | d[1] << 16 | d[2] << 8 | d[3];
    if (tag != 0x00010000 && tag != 0x74727565) return false;           /* 1.0 ou 'true' */
    u16 nt = be16(d + 4);
    if (12 + (u32)nt * 16 > n) return false;
    const char *need[] = {"head", "maxp", "hhea", "hmtx", "loca", "glyf", "cmap"};
    for (int k = 0; k < 7; k++) {
        bool ok = false;
        for (u16 i = 0; i < nt; i++) {
            const u8 *r = d + 12 + i * 16;
            u32 off = be32(r + 8), len = be32(r + 12);
            if (!memcmp(r, need[k], 4)) { ok = off < n && len <= n - off && len >= 4; break; }
        }
        if (!ok) return false;
    }
    return true;
}
bool font_set_fallback(const u8 *d, u32 n) {
    if (!font_valid(d, n)) return false;
    /* cópia própria: arquivos de volumes externos podem sair da memória (LRU do vfs_bridge) */
    u8 *copy = kmalloc(n);
    if (!copy) return false;
    memcpy(copy, d, n);
    font_load_into(&fallback, FONT_COUNT, copy, copy + n, true);   /* id próprio: o cache de glifos não mistura com a Inter */
    return fallback.data != NULL;
}

/* ------------------------------------------------------------ fontes dos apps
 * Fontes web que o Chromium entrega ao CoreText (CTFontManagerCreateFontDescriptorFromData), registradas
 * por GF_REGISTER. Os bytes vêm de um processo e o leitor acima confia nas tabelas, então font_sane
 * confere antes tudo o que ele vai ler: diretório, head/maxp/hhea/hmtx, cada entrada da loca, cada
 * subtabela 4 e 12 da cmap e cada glifo. Fonte que não passa é recusada inteira.
 * Ficam até o desligamento: o cache de glifos guarda o id, e reaproveitar a vaga misturaria glifos de
 * fontes diferentes. O mesmo arquivo registrado de novo (outro processo, outra aba) devolve o mesmo id. */
#define FONT_USER_BYTES (32u << 20)
static struct font userfonts[FONT_USER_MAX];
static u32 user_sum[FONT_USER_MAX];
static u32 nuser, user_bytes;

static bool span_ok(u32 n, u32 off, u32 len) { return off <= n && len <= n - off; }
/* um glifo (p, len) inteiro dentro dos seus bytes, do jeito que glyph_outline e raster_into o lêem */
static bool glyph_sane(const u8 *p, u32 len) {
    if (len < 10) return false;                          /* raster_into lê a caixa (p+2..p+9) */
    i16 nc = bes16(p);
    if (nc == 0) return true;                            /* glyph_outline ignora */
    if (nc < 0) {                                        /* composto: registros até o fim da cadeia */
        u32 q = 10;
        u16 flags;
        do {
            if (q + 4 > len) return false;
            flags = be16(p + q);
            q += 4 + ((flags & 1) ? 4 : 2);
            if (flags & 8) q += 2;
            else if (flags & 0x40) q += 4;
            else if (flags & 0x80) q += 8;
            if (q > len) return false;
        } while (flags & 0x20);
        return true;                                     /* índices dos componentes: glyph_offset confere */
    }
    u32 q = 10 + (u32)nc * 2;
    if (q + 2 > len) return false;
    u32 last = 0;
    for (int c = 0; c < nc; c++) {                       /* fins de contorno estritamente crescentes */
        u32 e = be16(p + 10 + c * 2);
        if (c && e <= last) return false;
        last = e;
    }
    u32 npts = last + 1;
    q += 2 + be16(p + q);                                /* instruções */
    if (q > len) return false;
    u32 xb = 0, yb = 0;
    for (u32 i = 0; i < npts;) {                         /* bandeiras (com repetição), somando os bytes de x e y */
        if (q >= len) return false;
        u8 f = p[q++];
        u32 rep = 1;
        if (f & 8) { if (q >= len) return false; rep += p[q++]; }
        if (rep > npts - i) rep = npts - i;
        xb += rep * ((f & 2) ? 1u : (f & 16) ? 0u : 2u);
        yb += rep * ((f & 4) ? 1u : (f & 32) ? 0u : 2u);
        i += rep;
    }
    return (u64)q + xb + yb <= len;
}
/* subtabelas 4 e 12 da cmap: o que glyph_index pode ler para qualquer codepoint */
static bool cmap_sane(const u8 *c, u32 cl) {
    if (cl < 4) return false;
    u16 n = be16(c + 2);
    if (4 + (u32)n * 8 > cl) return false;
    for (u16 i = 0; i < n; i++) {
        u32 off = be32(c + 4 + i * 8 + 4);
        if (off > cl || cl - off < 4) return false;
        u32 room = cl - off;
        const u8 *t = c + off;
        u16 fmt = be16(t);
        if (fmt == 12) {
            if (room < 16 || be32(t + 12) > (room - 16) / 12) return false;
        } else if (fmt == 4) {
            if (room < 14) return false;
            u32 segx2 = be16(t + 6);
            if ((segx2 & 1) || 14 + segx2 * 4 + 2 > room) return false;
            u32 at_ranges = 14 + segx2 * 3 + 2;
            for (u32 s = 0; s < segx2 / 2; s++) {
                u32 end = be16(t + 14 + s * 2), start = be16(t + 14 + segx2 + 2 + s * 2);
                u32 ro = be16(t + at_ranges + s * 2);
                if (!ro || end < start) continue;
                if (at_ranges + s * 2 + ro + (end - start) * 2 + 2 > room) return false;
            }
        }
    }
    return true;
}
static bool font_sane(const u8 *d, u32 n) {
    if (!font_valid(d, n)) return false;
    u16 nt = be16(d + 4);
    for (u16 i = 0; i < nt; i++) {                       /* font_table copia qualquer tabela */
        const u8 *r = d + 12 + i * 16;
        if (!span_ok(n, be32(r + 8), be32(r + 12))) return false;
    }
    struct font t;
    memset(&t, 0, sizeof t);
    t.data = d; t.size = n;
    u32 hl = 0, ml = 0, hhl = 0, hml = 0, ll = 0, gl = 0, cl = 0;
    u32 head = find_table_len(&t, "head", &hl), maxp = find_table_len(&t, "maxp", &ml);
    u32 hhea = find_table_len(&t, "hhea", &hhl);
    find_table_len(&t, "hmtx", &hml);                   /* só o tamanho: nh * 4 cabe nele */
    u32 loca = find_table_len(&t, "loca", &ll), glyf = find_table_len(&t, "glyf", &gl);
    u32 cmap = find_table_len(&t, "cmap", &cl);
    if (hl < 54 || ml < 6 || hhl < 36) return false;
    u32 upem = be16(d + head + 18), ng = be16(d + maxp + 4), nh = be16(d + hhea + 34);
    i16 lf = bes16(d + head + 50);
    if (upem < 16 || upem > 16384 || (lf != 0 && lf != 1) || !ng || !nh) return false;
    if (nh * 4 > hml) return false;
    if ((ng + 1) * (lf ? 4u : 2u) > ll) return false;
    if (!cmap_sane(d + cmap, cl)) return false;
    u32 prev = 0;
    for (u32 g = 0; g <= ng; g++) {                      /* loca crescente e dentro da glyf; cada glifo são */
        u32 o = lf ? be32(d + loca + g * 4) : be16(d + loca + g * 2) * 2u;
        if (o > gl || (g && o < prev)) return false;
        if (g && o > prev && !glyph_sane(d + glyf + prev, o - prev)) return false;
        prev = o;
    }
    return true;
}
/* d veio de kmalloc; *kept diz se a fonte ficou com o buffer (aí quem chamou não libera) */
int font_register(u8 *d, u32 n, bool *kept) {
    *kept = false;
    if (!d || n < 12) return -1;
    u32 sum = 2166136261u;
    for (u32 i = 0; i < n; i++) { sum ^= d[i]; sum *= 16777619u; }
    for (u32 i = 0; i < nuser; i++)
        if (user_sum[i] == sum && userfonts[i].size == n && !memcmp(userfonts[i].data, d, n)) return FONT_USER_BASE + (int)i;
    if (nuser >= FONT_USER_MAX || user_bytes + n > FONT_USER_BYTES || !font_sane(d, n)) return -1;
    u32 i = nuser;
    font_load_into(&userfonts[i], FONT_USER_BASE + (int)i, d, d + n, false);
    user_sum[i] = sum;
    user_bytes += n;
    nuser = i + 1;                                       /* publica só depois de pronta */
    *kept = true;
    kprintf("Fontes: fonte de app registrada (id %d, %u glifos, %u bytes)\n", FONT_USER_BASE + (int)i, userfonts[i].nglyphs, n);
    return FONT_USER_BASE + (int)i;
}
bool font_has_fallback(void) { return fallback.data != NULL; }
/* procura a fonte nos lugares de costume; chamado depois que o sistema de arquivos está montado */
#include "vfs.h"
bool font_load_fallback(void) {
    static const char *paths[] = {
        "/System/Library/Fonts/mOS-CJK.ttf", "/Sistema/Fontes/mOS-CJK.ttf",
        "/System/Library/Fonts/DroidSansFallbackFull.ttf", "/System/Library/Fonts/DroidSansFallback.ttf",
        "/Library/Fonts/mOS-CJK.ttf",
    };
    if (fallback.data) return true;
    for (u32 i = 0; i < sizeof paths / sizeof *paths; i++) {
        vnode_t *v = vfs_lookup(paths[i]);
        if (!v || v->dir || !vfs_ensure(v) || !v->data) continue;
        if (font_set_fallback(v->data, (u32)v->size)) {
            kprintf("Fontes: fallback %s (%u glifos)\n", paths[i], fallback.nglyphs);
            return true;
        }
    }
    return false;
}

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
    if (!nc) return;                                     /* sem contornos: não há fins de contorno a ler */
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
static spinlock_t font_lock;

static void raster_into(struct font *f, float size, u16 gi, glyph_t *g) {
    if (!gamma_ready) {
        for (int i = 0; i < 256; i++) gamma_lut[i] = (u8)(fpow(i / 255.0f, 0.78f) * 255 + 0.5f);
        gamma_ready = true;
    }
    u32 len;
    u32 off = glyph_offset(f, gi, &len);
    if (!len) return;
    float scale = size / f->upem;
    const u8 *p = f->data + off;
    float xmin = bes16(p + 2) * scale, ymin = bes16(p + 4) * scale, xmax = bes16(p + 6) * scale, ymax = bes16(p + 8) * scale;
    int x0 = (int)ffloor(xmin) - 1, y0 = (int)ffloor(-ymax) - 1;
    int w = (int)fceil(xmax) - x0 + 2, hh = (int)fceil(-ymin) - y0 + 2;
    if (w <= 0 || hh <= 0 || w >= 1024 || hh >= 1024) return;
    lines_t l = {0};
    xform_t id = {1, 0, 0, 1, 0, 0};
    glyph_outline(f, gi, id, scale, (float)-x0, (float)-y0, &l, 0);
    float *acc = kcalloc((usize)w * hh + 4, sizeof(float));
    if (!acc) { kfree(l.pts); return; }
    for (int i = 0; i < l.n; i += 4) raster_line(acc, w, hh, l.pts[i], l.pts[i + 1], l.pts[i + 2], l.pts[i + 3]);
    u8 *bmp = kmalloc((usize)w * hh);
    if (!bmp) { kfree(acc); kfree(l.pts); return; }
    g->bmp = bmp;
    raster_finish(acc, w * hh, g->bmp);
    for (int i = 0; i < w * hh; i++) g->bmp[i] = gamma_lut[g->bmp[i]];
    kfree(acc);
    kfree(l.pts);
    g->w = (i16)w; g->h = (i16)hh; g->left = (i16)x0; g->top = (i16)y0;
}

/* chave 0x80000000 | glifo distingue o cache por índice do cache por codepoint */
static glyph_t *glyph_cached(struct font *f, float size, u32 key, u16 gi) {
    u16 sz = (u16)(size * 4);
    u32 h = (key * 2654435761u ^ sz * 40503u ^ f->id * 97u) % GCACHE;
    u64 fl = spin_lock_irqsave(&font_lock);
    for (glyph_t *g = gcache[h]; g; g = g->next)
        if (g->key_cp == key && g->key_size == sz && g->key_font == f->id) {
            spin_unlock_irqrestore(&font_lock, fl);
            return g;
        }
    spin_unlock_irqrestore(&font_lock, fl);
    glyph_t *g = kcalloc(1, sizeof(glyph_t));
    if (!g) return 0;
    g->key_cp = key; g->key_size = sz; g->key_font = (u16)f->id;
    raster_into(f, size, gi, g);
    fl = spin_lock_irqsave(&font_lock);
    for (glyph_t *e = gcache[h]; e; e = e->next)
        if (e->key_cp == key && e->key_size == sz && e->key_font == f->id) {
            spin_unlock_irqrestore(&font_lock, fl);
            kfree(g->bmp);
            kfree(g);
            return e;
        }
    g->next = gcache[h];
    gcache[h] = g;
    spin_unlock_irqrestore(&font_lock, fl);
    return g;
}

static glyph_t *glyph_get(struct font *f, float size, u32 cp) {
    return glyph_cached(f, size, cp, glyph_index(f, cp));
}

bool font_has_glyph(font_t *f, u32 cp) { return glyph_index(f, cp) != 0 || (fallback.data && cp >= 0x80 && glyph_index(&fallback, cp) != 0); }

static struct font *face(int weight) {
    if (weight == FONT_FALLBACK_WEIGHT && fallback.data) return &fallback;
    /* fonte de app; id que não foi registrado cai na regular (nunca numa face sem dados) */
    if (weight >= FONT_USER_BASE) return weight < FONT_USER_BASE + (int)nuser ? &userfonts[weight - FONT_USER_BASE] : &fonts[0];
    if (weight < 0) weight = 0;
    if (weight >= FONT_COUNT) weight = FONT_COUNT - 1;
    return &fonts[weight];
}
int font_table(int weight, u32 tag, void *dst, u32 cap) {
    struct font *f = face(weight);
    if (!f->data) return 0;
    char t[4] = {(char)(tag >> 24), (char)(tag >> 16), (char)(tag >> 8), (char)tag};
    u32 len = 0, off = find_table_len(f, t, &len);
    if (!off || !len || off + len > f->size) return 0;
    if (dst && cap) {
        u32 n = len < cap ? len : cap;
        memcpy(dst, f->data + off, n);
    }
    return (int)len;
}
u16 font_glyph_id(int weight, u32 cp) { return glyph_index(face(weight), cp); }
void font_face_metrics(int weight, i32 *upem, i32 *asc, i32 *desc, i32 *nglyphs) {
    struct font *f = face(weight);
    /* upem 0 = a face ainda não foi carregada. Quem chama distingue isso de uma métrica real. */
    if (!f->data) {
        if (upem) *upem = 0;
        if (asc) *asc = 0;
        if (desc) *desc = 0;
        if (nglyphs) *nglyphs = 0;
        return;
    }
    if (upem) *upem = f->upem ? f->upem : 2048;
    if (asc) *asc = f->ascender;
    if (desc) *desc = f->descender;
    if (nglyphs) *nglyphs = f->nglyphs;
}
i32 font_advance_units(int weight, u16 glyph) {
    struct font *f = face(weight);
    return advance_units(f, glyph);
}
int font_raster_glyph(int weight, u16 glyph, float size, u8 *dst, u32 cap, i16 *w, i16 *h, i16 *left, i16 *top) {
    struct font *f = face(weight);
    if (size < 1) size = 1;
    if (size > 512) size = 512;
    if (!f->data) return 0;
    glyph_t *g = glyph_cached(f, size, 0x80000000u | glyph, glyph);
    if (!g) return -1;
    if (w) *w = g->w;
    if (h) *h = g->h;
    if (left) *left = g->left;
    if (top) *top = g->top;
    int n = (int)g->w * g->h;
    if (n < 0) n = 0;
    if (dst && g->bmp && cap && n) memcpy(dst, g->bmp, n < (int)cap ? (u32)n : cap);
    return n;
}

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

/* glifo que falta na Inter: vem da fonte de fallback (CJK, hangul, kana...) */
static struct font *fallback_for(struct font *f, u32 cp, u16 *g) {
    if (*g || cp < 0x80 || !fallback.data) return f;
    u16 fg = glyph_index(&fallback, cp);
    if (!fg) return f;
    *g = fg;
    return &fallback;
}

int text_width_n(font_t *f, float size, const char *s, int bytes) {
    float x = 0;
    const char *e = bytes < 0 ? NULL : s + bytes;
    u16 prev = 0;
    while (*s && (!e || s < e)) {
        u32 cp = utf8_decode(&s);
        if (cp == '\t') cp = ' ';
        u16 g = glyph_index(f, cp);
        struct font *ff = fallback_for(f, cp, &g);
        if (ff != f) { x += advance_units(ff, g) * size / ff->upem; prev = 0; continue; }
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
        struct font *ff = fallback_for(f, cp, &gi);
        if (ff != f) {
            glyph_t *g = glyph_cached(ff, size, cp, gi);
            if (g && g->bmp) gfx_mask(s, (int)(pen + 0.5f) + g->left, baseline + g->top, g->bmp, g->w, g->h, g->w, color);
            pen += advance_units(ff, gi) * size / ff->upem;
            prev = 0;
            continue;
        }
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
