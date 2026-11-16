/* CoreGraphics do mOS — layouts de pixel (incluído por coregraphics.c depois de cgobj_t e cgspace_t).
 *
 * CGImage: qualquer combinação que o CGImageCreate do macOS aceita vira ARGB 8 bits por dentro:
 * 1/2/4/8/16 bits por componente, 32 bits float (kCGBitmapFloatComponents), 5 bits empacotado (16 bpp),
 * cinza, RGB, CMYK e indexado, alfa primeiro/último/pré-multiplicado/pulado/só alfa, ordem 16/32 little ou big,
 * e o array decode.
 *
 * CGBitmapContext: os formatos que o CGBitmapContextCreate do macOS aceita, lidos e gravados aqui:
 * só alfa 8 bits, cinza 8 (sem alfa), cinza+alfa 8, RGB 8 bpc em 32 bpp (as 8 combinações de alfa/ordem),
 * RGB 5 bpc em 16 bpp, RGBA 16 bpc, RGBA float 32 bpc e CMYK 8 bpc. */

enum { PF_NONE = 0, PF_A8, PF_G8, PF_GA8, PF_RGB32, PF_RGB16_555, PF_RGBA64, PF_RGBAF, PF_CMYK32, PF_G16 };
enum { AI_NONE = 0, AI_PLAST = 1, AI_PFIRST = 2, AI_LAST = 3, AI_FIRST = 4, AI_SKIPLAST = 5, AI_SKIPFIRST = 6, AI_ONLY = 7 };
#define BI_FLOAT 0x100u
#define BI_ORDER(i) ((i) & 0x7000u)
#define BO_16L 0x1000u
#define BO_32L 0x2000u
#define BO_16B 0x3000u
#define BO_32B 0x4000u

static int ai_has_alpha(u32 ai) { return ai >= 1 && ai <= 4; }
static int ai_first(u32 ai) { return ai == AI_PFIRST || ai == AI_FIRST || ai == AI_SKIPFIRST; }
static int ai_present(u32 ai) { return ai != AI_NONE; }        /* um canal ocupado (alfa ou pulado) */
static int ai_premul(u32 ai) { return ai == AI_PLAST || ai == AI_PFIRST; }

static u32 pm_of(u32 a, u32 r, u32 g, u32 b) {
    if (a >= 255) return 0xFF000000u | r << 16 | g << 8 | b;
    return a << 24 | (r * a + 127) / 255 << 16 | (g * a + 127) / 255 << 8 | (b * a + 127) / 255;
}
static u32 un_pm(u32 p, u32 *r, u32 *g, u32 *b) {
    u32 a = p >> 24;
    *r = (p >> 16) & 255; *g = (p >> 8) & 255; *b = p & 255;
    if (a && a < 255) {
        *r = (*r * 255 + a / 2) / a; *g = (*g * 255 + a / 2) / a; *b = (*b * 255 + a / 2) / a;
        if (*r > 255) *r = 255;
        if (*g > 255) *g = 255;
        if (*b > 255) *b = 255;
    }
    return a;
}

/* ---------------------------------------------------------------- contexto */
/* posição em bytes de A, R, G, B dentro do pixel de 32 bpp; -1 = sem alfa */
static void rgb32_pos(u32 info, int *pa, int *pr, int *pg, int *pb) {
    u32 ai = info & 31;
    int first = ai_first(ai);
    int p[4];                                    /* ordem lógica (big-endian) */
    if (first) { p[0] = 0; p[1] = 1; p[2] = 2; p[3] = 3; }      /* A R G B */
    else { p[1] = 0; p[2] = 1; p[3] = 2; p[0] = 3; }            /* R G B A */
    if (BI_ORDER(info) == BO_32L) for (int i = 0; i < 4; i++) p[i] = 3 - p[i];
    *pa = p[0]; *pr = p[1]; *pg = p[2]; *pb = p[3];
}

/* formato de um contexto, ou PF_NONE se o macOS também recusaria */
static int ctx_format(size_t bpc, u32 info, int model, size_t *bpp) {
    u32 ai = info & 31;
    int fl = (info & BI_FLOAT) != 0;
    if (ai == AI_ONLY) { if (bpc != 8) return PF_NONE; *bpp = 1; return PF_A8; }
    if (model == 0) {                                     /* cinza */
        if (bpc == 8 && ai == AI_NONE) { *bpp = 1; return PF_G8; }
        if (bpc == 16 && ai == AI_NONE && !fl) { *bpp = 2; return PF_G16; }
        if (bpc == 8 && (ai == AI_PLAST || ai == AI_PFIRST || ai == AI_SKIPLAST || ai == AI_SKIPFIRST)) { *bpp = 2; return PF_GA8; }
        return PF_NONE;
    }
    if (model == 2) {                                     /* CMYK sem alfa */
        if (bpc == 8 && ai == AI_NONE) { *bpp = 4; return PF_CMYK32; }
        return PF_NONE;
    }
    if (ai == AI_NONE || ai == AI_LAST || ai == AI_FIRST) return PF_NONE;   /* RGB precisa de alfa pré-multiplicado ou pulado */
    if (bpc == 8) { *bpp = 4; return PF_RGB32; }
    if (bpc == 5 && (ai == AI_SKIPFIRST)) { *bpp = 2; return PF_RGB16_555; }
    if (bpc == 16 && !fl) { *bpp = 8; return PF_RGBA64; }
    if (bpc == 32 && fl) { *bpp = 16; return PF_RGBAF; }
    return PF_NONE;
}

static u16 rd16e(const u8 *p, int le) { return le ? (u16)(p[0] | p[1] << 8) : (u16)(p[0] << 8 | p[1]); }
static void wr16e(u8 *p, u16 v, int le) { if (le) { p[0] = (u8)v; p[1] = (u8)(v >> 8); } else { p[0] = (u8)(v >> 8); p[1] = (u8)v; } }
static float rdf(const u8 *p, int le) {
    u32 v = le ? (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24 : (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3];
    float f;
    memcpy(&f, &v, 4);
    return f;
}
static void wrf(u8 *p, float f, int le) {
    u32 v;
    memcpy(&v, &f, 4);
    if (le) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
    else { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }
}
static int f2b(float f) { return f <= 0 ? 0 : f >= 1 ? 255 : (int)(f * 255 + 0.5f); }

/* componente lógica k (0 = primeira na ordem big-endian) de pixels com 4 componentes */
static int comp4_index(u32 info, int which /* 0 A, 1 R, 2 G, 3 B */) {
    int first = ai_first(info & 31);
    static const int fo[4] = {0, 1, 2, 3}, lo[4] = {3, 0, 1, 2};
    return first ? fo[which] : lo[which];
}

/* pixel do contexto → ARGB pré-multiplicado (alfa 255 quando pulado) */
static u32 ctx_get(const cgobj_t *o, const u8 *p) {
    u32 ai = o->info & 31;
    int le16 = BI_ORDER(o->info) == BO_16L || BI_ORDER(o->info) == BO_32L;
    switch (o->fmt) {
    case PF_A8: return (u32)p[0] << 24;
    case PF_G8: return 0xFF000000u | (u32)p[0] * 0x010101u;
    case PF_G16: { u32 v = rd16e(p, le16) >> 8; return 0xFF000000u | v * 0x010101u; }
    case PF_GA8: {
        int first = ai_first(ai);
        u32 g = first ? p[1] : p[0], a = first ? p[0] : p[1];
        if (!ai_has_alpha(ai)) a = 255;
        if (a < 255 && !ai_premul(ai)) g = g * a / 255;
        if (g > a) g = a;
        return a << 24 | g * 0x010101u;
    }
    case PF_RGB16_555: {
        u32 v = rd16e(p, le16);
        u32 r = (v >> 10) & 31, g = (v >> 5) & 31, b = v & 31;
        return 0xFF000000u | (r * 255 / 31) << 16 | (g * 255 / 31) << 8 | (b * 255 / 31);
    }
    case PF_RGBA64: {
        u32 c[4];
        for (int i = 0; i < 4; i++) c[i] = rd16e(p + comp4_index(o->info, i) * 2, le16) >> 8;
        if (!ai_has_alpha(ai)) c[0] = 255;
        return c[0] << 24 | (c[1] > c[0] ? c[0] : c[1]) << 16 | (c[2] > c[0] ? c[0] : c[2]) << 8 | (c[3] > c[0] ? c[0] : c[3]);
    }
    case PF_RGBAF: {
        u32 c[4];
        for (int i = 0; i < 4; i++) c[i] = (u32)f2b(rdf(p + comp4_index(o->info, i) * 4, BI_ORDER(o->info) != BO_32B && BI_ORDER(o->info) != BO_16B));
        if (!ai_has_alpha(ai)) c[0] = 255;
        return c[0] << 24 | (c[1] > c[0] ? c[0] : c[1]) << 16 | (c[2] > c[0] ? c[0] : c[2]) << 8 | (c[3] > c[0] ? c[0] : c[3]);
    }
    case PF_CMYK32: {
        u32 k = p[3];
        return 0xFF000000u | ((255 - p[0]) * (255 - k) / 255) << 16 | ((255 - p[1]) * (255 - k) / 255) << 8 | ((255 - p[2]) * (255 - k) / 255);
    }
    default: {
        int pa, pr, pg, pb;
        rgb32_pos(o->info, &pa, &pr, &pg, &pb);
        u32 a = ai_has_alpha(ai) ? p[pa] : 255, r = p[pr], g = p[pg], b = p[pb];
        if (a < 255 && !ai_premul(ai)) return pm_of(a, r, g, b);
        return a << 24 | (r > a ? a : r) << 16 | (g > a ? a : g) << 8 | (b > a ? a : b);
    }
    }
}

/* ARGB pré-multiplicado → pixel do contexto */
static void ctx_put(const cgobj_t *o, u8 *p, u32 pm) {
    u32 ai = o->info & 31;
    int le16 = BI_ORDER(o->info) == BO_16L || BI_ORDER(o->info) == BO_32L;
    u32 a = pm >> 24, r, g, b;
    switch (o->fmt) {
    case PF_A8: p[0] = (u8)a; return;
    case PF_G8:
    case PF_G16: {
        un_pm(pm, &r, &g, &b);
        u32 y = (r * 77 + g * 150 + b * 29 + 128) >> 8;
        if (o->fmt == PF_G8) p[0] = (u8)y; else wr16e(p, (u16)(y * 257), le16);
        return;
    }
    case PF_GA8: {
        u32 y = ((((pm >> 16) & 255) * 77 + ((pm >> 8) & 255) * 150 + (pm & 255) * 29 + 128) >> 8);   /* já pré-multiplicado */
        if (!ai_has_alpha(ai)) { un_pm(pm, &r, &g, &b); y = (r * 77 + g * 150 + b * 29 + 128) >> 8; a = 255; }
        if (ai_first(ai)) { p[0] = (u8)a; p[1] = (u8)y; } else { p[0] = (u8)y; p[1] = (u8)a; }
        return;
    }
    case PF_RGB16_555: {
        un_pm(pm, &r, &g, &b);
        wr16e(p, (u16)((r * 31 / 255) << 10 | (g * 31 / 255) << 5 | (b * 31 / 255)), le16);
        return;
    }
    case PF_RGBA64: {
        u32 c[4] = {ai_has_alpha(ai) ? a : 0xFF, (pm >> 16) & 255, (pm >> 8) & 255, pm & 255};
        if (!ai_has_alpha(ai)) un_pm(pm, &c[1], &c[2], &c[3]);
        for (int i = 0; i < 4; i++) wr16e(p + comp4_index(o->info, i) * 2, (u16)(c[i] * 257), le16);
        return;
    }
    case PF_RGBAF: {
        u32 c[4] = {ai_has_alpha(ai) ? a : 0xFF, (pm >> 16) & 255, (pm >> 8) & 255, pm & 255};
        if (!ai_has_alpha(ai)) un_pm(pm, &c[1], &c[2], &c[3]);
        int le = BI_ORDER(o->info) != BO_32B && BI_ORDER(o->info) != BO_16B;
        for (int i = 0; i < 4; i++) wrf(p + comp4_index(o->info, i) * 4, c[i] / 255.0f, le);
        return;
    }
    case PF_CMYK32: {
        un_pm(pm, &r, &g, &b);
        u32 c = 255 - r, m = 255 - g, y = 255 - b, k = c < m ? (c < y ? c : y) : (m < y ? m : y);
        if (k == 255) { p[0] = p[1] = p[2] = 0; p[3] = 255; return; }
        p[0] = (u8)((c - k) * 255 / (255 - k)); p[1] = (u8)((m - k) * 255 / (255 - k)); p[2] = (u8)((y - k) * 255 / (255 - k)); p[3] = (u8)k;
        return;
    }
    default: {
        int pa, pr, pg, pb;
        rgb32_pos(o->info, &pa, &pr, &pg, &pb);
        if (ai_has_alpha(ai)) {
            if (!ai_premul(ai)) { un_pm(pm, &r, &g, &b); }
            else { r = (pm >> 16) & 255; g = (pm >> 8) & 255; b = pm & 255; }
            p[pa] = (u8)a;
        } else {
            un_pm(pm, &r, &g, &b);
            p[pa] = 0xFF;
        }
        p[pr] = (u8)r; p[pg] = (u8)g; p[pb] = (u8)b;
        return;
    }
    }
}

/* ---------------------------------------------------------------- CGImage: amostras arbitrárias */
typedef struct {
    size_t bpc, bpp, ncolor, ncomp;  /* ncomp = cores + canal extra (alfa ou pulado) */
    u32 info;
    int model;                       /* 0 cinza, 1 RGB, 2 CMYK, 5 indexado, -2 máscara de imagem */
    const double *decode;
    const u8 *table; int ntable; int tcomp;   /* indexado: tabela de cores (tcomp componentes por entrada) */
    int table_model;
} unpack_t;

/* amostra k (posição física) do pixel x da linha, normalizada a 0..1 */
static double sample_at(const unpack_t *u, const u8 *row, size_t x, size_t k) {
    size_t bit = x * u->bpp + k * u->bpc;
    u32 ord = BI_ORDER(u->info);
    if (u->bpc == 8) return row[bit >> 3] / 255.0;
    if (u->bpc == 16) {
        const u8 *p = row + (bit >> 3);
        int le = ord == BO_16L || ord == BO_32L;
        return rd16e(p, le) / 65535.0;
    }
    if (u->bpc == 32) {
        const u8 *p = row + (bit >> 3);
        int le = ord == BO_32L || ord == BO_16L;
        if (u->info & BI_FLOAT) { float f = rdf(p, le); return f; }
        u32 v = le ? (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24 : (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3];
        return v / 4294967295.0;
    }
    if (u->bpc == 5) {                               /* 16 bpp empacotado: x RRRRR GGGGG BBBBB (ou com o alfa/pulado no fim) */
        const u8 *p = row + x * 2;
        int le = ord == BO_16L || ord == BO_32L;
        u32 v = rd16e(p, le);
        int first = ai_first(u->info & 31);
        int sh = first ? 10 - (int)(k - 1) * 5 : 11 - (int)k * 5;
        if (first && k == 0) return 1;
        return ((v >> sh) & 31) / 31.0;
    }
    /* 1, 2 e 4 bits, MSB primeiro */
    u32 v = (row[bit >> 3] >> (8 - u->bpc - (bit & 7))) & ((1u << u->bpc) - 1);
    return (double)v / (double)((1u << u->bpc) - 1);
}

/* uma linha de qualquer layout → ARGB 8 bits; pm=1: sai pré-multiplicado se a fonte for; 0: sai reto */
static void unpack_row(const unpack_t *u, const u8 *row, size_t w, u32 *out, int *src_premul) {
    u32 ai = u->info & 31;
    int has_a = ai_has_alpha(ai), first = ai_first(ai), extra = ai_present(ai) && ai != AI_ONLY;
    *src_premul = ai_premul(ai);
    for (size_t x = 0; x < w; x++) {
        double c[4] = {0, 0, 0, 0}, a = 1;
        if (u->model == -2) {                         /* máscara de imagem: 0 pinta, 1 mascara (decode inverte) */
            double v = sample_at(u, row, x, 0);
            if (u->decode) v = u->decode[0] + v * (u->decode[1] - u->decode[0]);
            u32 A = (u32)((1 - (v < 0 ? 0 : v > 1 ? 1 : v)) * 255 + 0.5);
            out[x] = A << 24;
            continue;
        }
        if (ai == AI_ONLY) {
            double v = sample_at(u, row, x, 0);
            out[x] = (u32)(v * 255 + 0.5) << 24;
            continue;
        }
        size_t base = extra && first ? 1 : 0;
        for (size_t k = 0; k < u->ncolor && k < 4; k++) {
            double v = sample_at(u, row, x, base + k);
            if (u->decode) v = u->decode[2 * k] + v * (u->decode[2 * k + 1] - u->decode[2 * k]);
            c[k] = v;
        }
        if (has_a) a = sample_at(u, row, x, first ? 0 : u->ncolor);
        if (u->decode && has_a) a = u->decode[2 * u->ncolor] + a * (u->decode[2 * u->ncolor + 1] - u->decode[2 * u->ncolor]);
        double r, g, b;
        int model = u->model;
        double raw[4] = {c[0], c[1], c[2], c[3]};
        if (model == 5 && u->table) {                 /* indexado: o valor cru (sem normalizar) é o índice */
            int idx = (int)(c[0] * ((1u << (u->bpc > 8 ? 8 : u->bpc)) - 1) + 0.5);
            if (u->decode) idx = (int)(c[0] + 0.5);
            if (idx < 0) idx = 0;
            if (idx >= u->ntable) idx = u->ntable - 1;
            for (int k = 0; k < u->tcomp && k < 4; k++) raw[k] = u->table[idx * u->tcomp + k] / 255.0;
            model = u->table_model;
        }
        if (model == 0) r = g = b = raw[0];
        else if (model == 2) {
            r = (1 - raw[0]) * (1 - raw[3]); g = (1 - raw[1]) * (1 - raw[3]); b = (1 - raw[2]) * (1 - raw[3]);
        } else { r = raw[0]; g = raw[1]; b = raw[2]; }
        double cl[4] = {a, r, g, b};
        u32 q[4];
        for (int i = 0; i < 4; i++) q[i] = (u32)((cl[i] < 0 ? 0 : cl[i] > 1 ? 1 : cl[i]) * 255 + 0.5);
        if (*src_premul) { for (int i = 1; i < 4; i++) if (q[i] > q[0]) q[i] = q[0]; }
        out[x] = q[0] << 24 | q[1] << 16 | q[2] << 8 | q[3];
    }
}
