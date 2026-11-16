/* mOS PDF — fontes (ISO 32000-1 §9.6–9.10): Type1/MMType1, TrueType, Type0 (CIDFontType0/2), Type3.
 * Programa embutido (FontFile/2/3) lido pelos leitores de pdf_glyph.inc; sem programa, a fonte do sistema
 * pelo CoreText (contorno por Unicode). Larguras, CMaps embutidos (codespace, cid*, usecmap Identity) e
 * ToUnicode. Incluído por cgpdf_draw.c. */

void *CTFontCreateWithName(void *name, double size, const void *matrix);
u8 CTFontGetGlyphsForCharacters(void *f, const u16 *chars, u16 *glyphs, long n);
void *CTFontCreatePathForGlyph(void *f, u16 g, const CGAffineTransform *m);
double CTFontGetAdvancesForGlyphs(void *f, int o, const u16 *g, CGSize *adv, long n);
void *CFStringCreateWithCString(void *alloc, const char *s, u32 enc);

typedef struct { u32 lo, hi, v; int nb; } cm_range;
typedef struct { u32 code; int nb; u16 uni[8]; int nu; } tu_entry;
typedef struct { cm_range *r; int n, cap; cm_range cs[16]; int ncs; int identity; } pcmap;
typedef struct pfont pfont;
struct pfont {
    pdf_obj *key;
    int kind;                       /* 1 simples, 2 Type0, 3 Type3 */
    int prog;                       /* 0 sistema, 1 TrueType, 2 CFF, 3 Type1 */
    u8 *data; size_t n;
    tt_font tt; cff_font cff; t1_font *t1;
    double fm[6];
    double width[256]; u8 has_width[256]; double missing;
    int gid[256];
    char *names[256];
    int symbolic;
    pcmap enc;                      /* Type0: código → CID */
    u16 *cid2gid; size_t ncid2gid; int cid2gid_ident;
    cm_range *w; int nw; double dw;
    int vertical; double dw2[2];
    tu_entry *tu; int ntu, tucap;
    pdf_obj *charprocs, *t3res;
    void *ctfont;
    void *cache[256]; void **ccache; u32 nccache;     /* contornos por código (simples) ou gid (CID) */
};

static void pcmap_add(pcmap *m, u32 lo, u32 hi, u32 v, int nb) {
    if (m->n == m->cap) { m->cap = m->cap ? m->cap * 2 : 64; m->r = realloc(m->r, (size_t)m->cap * sizeof *m->r); if (!m->r) { m->n = m->cap = 0; return; } }
    m->r[m->n++] = (cm_range){lo, hi, v, nb};
}
static void tu_add(pfont *f, u32 code, int nb, const u16 *u, int nu) {
    if (f->ntu == f->tucap) { f->tucap = f->tucap ? f->tucap * 2 : 128; f->tu = realloc(f->tu, (size_t)f->tucap * sizeof *f->tu); if (!f->tu) { f->ntu = f->tucap = 0; return; } }
    tu_entry *e = &f->tu[f->ntu++];
    e->code = code; e->nb = nb; e->nu = nu > 8 ? 8 : nu;
    for (int i = 0; i < e->nu; i++) e->uni[i] = u[i];
}
static u32 str_code(pdf_obj *s, int *nb) {
    u32 v = 0;
    for (u32 i = 0; i < s->n && i < 4; i++) v = v << 8 | s->v.str[i];
    *nb = (int)(s->n < 4 ? s->n : 4);
    return v;
}
/* CMap embutido (Encoding de Type0 ou ToUnicode) */
static void cmap_parse(pdf_doc *pd, pdf_obj *s, pcmap *m, pfont *f, int depth) {
    size_t n;
    const u8 *d = pdf_stream_data(pd, s, &n, 0);
    if (!d || depth > 4) return;
    pdf_lexer *L = pdf_lex_begin(pd, d, n);
    if (!L) return;
    pdf_obj *st[8];
    int sp = 0, mode = 0;   /* 1 codespace, 2 cidrange, 3 cidchar, 4 bfrange, 5 bfchar */
    pdf_obj *o;
    while ((o = pdf_lex_next(L))) {
        if (o->type == PO_OP) {
            const char *w = o->v.name;
            if (!strcmp(w, "begincodespacerange")) mode = 1;
            else if (!strcmp(w, "begincidrange")) mode = 2;
            else if (!strcmp(w, "begincidchar")) mode = 3;
            else if (!strcmp(w, "beginbfrange")) mode = 4;
            else if (!strcmp(w, "beginbfchar")) mode = 5;
            else if (!strncmp(w, "end", 3)) mode = 0;
            else if (!strcmp(w, "usecmap") && sp >= 1 && st[sp - 1]->type == PO_NAME && m) {
                const char *nm = st[sp - 1]->v.name;
                if (!strncmp(nm, "Identity", 8)) m->identity = 1;
            }
            sp = 0;
            continue;
        }
        if (sp < 8) st[sp++] = o;
        if (mode == 1 && sp == 2 && m) {
            int nb;
            if (st[0]->type == PO_STRING && st[1]->type == PO_STRING && m->ncs < 16) { u32 lo = str_code(st[0], &nb), hi = str_code(st[1], &nb); m->cs[m->ncs++] = (cm_range){lo, hi, 0, nb}; }
            sp = 0;
        } else if (mode == 2 && sp == 3 && m) {
            int nb;
            double c;
            if (st[0]->type == PO_STRING && pdf_num(st[2], &c)) { u32 lo = str_code(st[0], &nb), hi = str_code(st[1], &nb); pcmap_add(m, lo, hi, (u32)c, nb); }
            sp = 0;
        } else if (mode == 3 && sp == 2 && m) {
            int nb;
            double c;
            if (st[0]->type == PO_STRING && pdf_num(st[1], &c)) { u32 lo = str_code(st[0], &nb); pcmap_add(m, lo, lo, (u32)c, nb); }
            sp = 0;
        } else if (mode == 4 && sp == 3 && f) {
            int nb;
            if (st[0]->type == PO_STRING && st[1]->type == PO_STRING) {
                u32 lo = str_code(st[0], &nb), hi = str_code(st[1], &nb);
                if (hi - lo > 65536) hi = lo + 65536;
                if (st[2]->type == PO_STRING) {
                    u16 u[8];
                    int nu = 0;
                    for (u32 i = 0; i + 1 < st[2]->n && nu < 8; i += 2) u[nu++] = (u16)(st[2]->v.str[i] << 8 | st[2]->v.str[i + 1]);
                    for (u32 c = lo; c <= hi && nu; c++) { tu_add(f, c, nb, u, nu); u[nu - 1]++; }
                } else if (st[2]->type == PO_ARRAY) {
                    for (u32 c = lo, i = 0; c <= hi && i < st[2]->n; c++, i++) {
                        pdf_obj *x = pdf_resolve(pd, st[2]->v.arr[i]);
                        if (!x || x->type != PO_STRING) continue;
                        u16 u[8];
                        int nu = 0;
                        for (u32 k = 0; k + 1 < x->n && nu < 8; k += 2) u[nu++] = (u16)(x->v.str[k] << 8 | x->v.str[k + 1]);
                        tu_add(f, c, nb, u, nu);
                    }
                }
            }
            sp = 0;
        } else if (mode == 5 && sp == 2 && f) {
            int nb;
            if (st[0]->type == PO_STRING && st[1]->type == PO_STRING) {
                u32 c = str_code(st[0], &nb);
                u16 u[8];
                int nu = 0;
                for (u32 k = 0; k + 1 < st[1]->n && nu < 8; k += 2) u[nu++] = (u16)(st[1]->v.str[k] << 8 | st[1]->v.str[k + 1]);
                if (st[1]->n == 1) { u[0] = st[1]->v.str[0]; nu = 1; }
                tu_add(f, c, nb, u, nu);
            }
            sp = 0;
        }
    }
    pdf_lex_end(L);
}
/* próximo código de uma string (Type0: pelo codespace; simples: 1 byte) */
static int pf_next_code(const pfont *f, const u8 *s, size_t n, size_t *i, u32 *code) {
    if (*i >= n) return 0;
    if (f->kind != 2) { *code = s[(*i)++]; return 1; }
    if (f->enc.ncs) {
        for (int nb = 1; nb <= 4; nb++) {
            if (*i + (size_t)nb > n) break;
            u32 v = 0;
            for (int k = 0; k < nb; k++) v = v << 8 | s[*i + k];
            for (int c = 0; c < f->enc.ncs; c++) {
                const cm_range *r = &f->enc.cs[c];
                if (r->nb != nb) continue;
                /* faixa por byte (o codespace é retangular) */
                int ok = 1;
                for (int k = 0; k < nb && ok; k++) {
                    u32 b = v >> (8 * (nb - 1 - k)) & 255, lo = r->lo >> (8 * (nb - 1 - k)) & 255, hi = r->hi >> (8 * (nb - 1 - k)) & 255;
                    ok = b >= lo && b <= hi;
                }
                if (ok) { *code = v; *i += (size_t)nb; return nb; }
            }
        }
        *code = s[(*i)++];
        return 1;
    }
    if (*i + 1 >= n) { *code = s[(*i)++]; return 1; }
    *code = (u32)s[*i] << 8 | s[*i + 1];
    *i += 2;
    return 2;
}
static u32 pf_cid(const pfont *f, u32 code, int nb) {
    if (f->kind != 2) return code;
    if (f->enc.identity && !f->enc.n) return code;
    for (int i = 0; i < f->enc.n; i++) {
        const cm_range *r = &f->enc.r[i];
        if (r->nb == nb && code >= r->lo && code <= r->hi) return r->v + (code - r->lo);
    }
    return f->enc.identity ? code : 0;
}
/* largura do código em unidades de texto ×1000 (Type3: espaço do glifo pela FontMatrix) */
static double pf_glyph_advance_prog(pfont *f, int gid);
static double pf_width(pfont *f, u32 code, int nb) {
    if (f->kind == 2) {
        u32 cid = pf_cid(f, code, nb);
        for (int i = 0; i < f->nw; i++) if (cid >= f->w[i].lo && cid <= f->w[i].hi) return (double)(int32_t)f->w[i].v / 1000.0;
        return f->dw / 1000.0;
    }
    if (code < 256 && f->has_width[code]) return f->kind == 3 ? f->width[code] * f->fm[0] : f->width[code] / 1000.0;
    if (f->kind == 3) return 0;
    if (code < 256 && f->gid[code] >= 0) { double a = pf_glyph_advance_prog(f, f->gid[code]); if (a > 0) return a; }
    return f->missing / 1000.0;
}

static int font_flag(pdf_doc *pd, pdf_obj *desc) { double v = 0; pdf_num(pdf_dict_get(pd, desc, "Flags"), &v); return (int)v; }
static void pf_base_encoding(pfont *f, const char *name) {
    const char *const *tab = 0;
    if (name && !strcmp(name, "WinAnsiEncoding")) tab = enc_win;
    else if (name && !strcmp(name, "MacRomanEncoding")) tab = enc_mac;
    else if (name && !strcmp(name, "StandardEncoding")) tab = enc_std;
    if (!tab) return;
    for (int i = 0; i < 256; i++) { free(f->names[i]); f->names[i] = tab[i] ? strdup(tab[i]) : 0; }
}
/* nome efetivo de cada código: codificação base (ou a do programa) + Differences */
static void pf_encoding(pdf_doc *pd, pfont *f, pdf_obj *fd, int has_prog_enc) {
    pdf_obj *e = pdf_dict_get(pd, fd, "Encoding");
    const char *base = 0;
    if (e && e->type == PO_NAME) base = e->v.name;
    else if (e && e->type == PO_DICT) base = pdf_name(pdf_dict_get(pd, e, "BaseEncoding"));
    if (base) pf_base_encoding(f, base);
    else if (!has_prog_enc && !f->symbolic) pf_base_encoding(f, f->prog == 1 ? "WinAnsiEncoding" : "StandardEncoding");
    pdf_obj *diff = e && e->type == PO_DICT ? pdf_dict_get(pd, e, "Differences") : 0;
    if (diff && diff->type == PO_ARRAY) {
        int code = 0;
        for (u32 i = 0; i < diff->n; i++) {
            pdf_obj *x = pdf_resolve(pd, diff->v.arr[i]);
            if (!x) continue;
            if (x->type == PO_INT) code = (int)x->v.i;
            else if (x->type == PO_NAME && code >= 0 && code < 256) { free(f->names[code]); f->names[code] = strdup(x->v.name); code++; }
        }
    }
}
static char *strip_subset(const char *n) {
    if (!n) return strdup("Helvetica");
    if (strlen(n) > 7 && n[6] == '+') return strdup(n + 7);
    return strdup(n);
}
/* fonte do sistema para uma BaseFont não embutida: Inter do mOS (o peso/estilo pelo nome) */
static void *pf_system_font(const char *base) {
    const char *nm = "Inter";
    if (base && (strstr(base, "Bold") || strstr(base, "Black") || strstr(base, "Heavy"))) nm = "Inter-Bold";
    void *s = CFStringCreateWithCString(0, nm, 0x08000100);
    void *f = CTFontCreateWithName(s, 1000, 0);
    CFRelease(s);
    return f;
}
static pfont *font_load(pdf_doc *pd, pdf_obj *fd) {
    fd = pdf_resolve(pd, fd);
    if (!fd || fd->type != PO_DICT) return 0;
    pfont *f = calloc(1, sizeof *f);
    if (!f) return 0;
    f->key = fd;
    for (int i = 0; i < 256; i++) f->gid[i] = -1;
    f->fm[0] = f->fm[3] = 0.001;
    const char *sub = pdf_name(pdf_dict_get(pd, fd, "Subtype"));
    pdf_obj *desc = pdf_dict_get(pd, fd, "FontDescriptor"), *cidf = 0;
    char *base = strip_subset(pdf_name(pdf_dict_get(pd, fd, "BaseFont")));
    f->kind = sub && !strcmp(sub, "Type0") ? 2 : sub && !strcmp(sub, "Type3") ? 3 : 1;
    if (f->kind == 2) {
        pdf_obj *df = pdf_dict_get(pd, fd, "DescendantFonts");
        cidf = pdf_array_get(pd, df, 0);
        desc = pdf_dict_get(pd, cidf, "FontDescriptor");
        pdf_obj *enc = pdf_dict_get(pd, fd, "Encoding");
        const char *en = pdf_name(enc);
        if (en) { f->enc.identity = 1; f->vertical = strstr(en, "-V") != 0; }
        else if (enc && enc->type == PO_STREAM) {
            cmap_parse(pd, enc, &f->enc, 0, 0);
            pdf_obj *wm = pdf_dict_get(pd, enc, "WMode");
            double v;
            if (pdf_num(wm, &v)) f->vertical = v == 1;
        }
        f->dw = 1000;
        pdf_num(pdf_dict_get(pd, cidf, "DW"), &f->dw);
        f->dw2[0] = 880; f->dw2[1] = -1000;
        pf_nums(pd, pdf_dict_get(pd, cidf, "DW2"), f->dw2, 2);
        pdf_obj *w = pdf_dict_get(pd, cidf, "W");
        for (u32 i = 0; w && w->type == PO_ARRAY && i < w->n;) {
            double c1;
            if (!pdf_num(pdf_array_get(pd, w, i), &c1)) { i++; continue; }
            pdf_obj *nx = pdf_array_get(pd, w, i + 1);
            if (nx && nx->type == PO_ARRAY) {
                for (u32 k = 0; k < nx->n; k++) {
                    double wv;
                    if (!pdf_num(pdf_array_get(pd, nx, k), &wv)) continue;
                    if (f->nw % 64 == 0) f->w = realloc(f->w, (size_t)(f->nw + 64) * sizeof *f->w);
                    if (f->w) f->w[f->nw++] = (cm_range){(u32)c1 + k, (u32)c1 + k, (u32)(int32_t)wv, 0};
                }
                i += 2;
            } else {
                double c2, wv;
                if (!pdf_num(nx, &c2) || !pdf_num(pdf_array_get(pd, w, i + 2), &wv)) break;
                if (f->nw % 64 == 0) f->w = realloc(f->w, (size_t)(f->nw + 64) * sizeof *f->w);
                if (f->w) f->w[f->nw++] = (cm_range){(u32)c1, (u32)c2, (u32)(int32_t)wv, 0};
                i += 3;
            }
        }
        pdf_obj *c2g = pdf_dict_get(pd, cidf, "CIDToGIDMap");
        if (c2g && c2g->type == PO_STREAM) {
            size_t n;
            const u8 *d = pdf_stream_data(pd, c2g, &n, 0);
            if (d) { f->ncid2gid = n / 2; f->cid2gid = malloc(f->ncid2gid * 2 + 2); if (f->cid2gid) for (size_t i = 0; i < f->ncid2gid; i++) f->cid2gid[i] = (u16)(d[2 * i] << 8 | d[2 * i + 1]); }
        } else f->cid2gid_ident = 1;
    }
    if (f->kind == 3) {
        double m[6] = {0.001, 0, 0, 0.001, 0, 0};
        pf_nums(pd, pdf_dict_get(pd, fd, "FontMatrix"), m, 6);
        memcpy(f->fm, m, sizeof m);
        f->charprocs = pdf_dict_get(pd, fd, "CharProcs");
        f->t3res = pdf_dict_get(pd, fd, "Resources");
    }
    f->symbolic = (font_flag(pd, desc) & 4) != 0;
    /* programa embutido */
    pdf_obj *ff = 0;
    int which = 0;
    if ((ff = pdf_dict_get(pd, desc, "FontFile2"))) which = 1;
    else if ((ff = pdf_dict_get(pd, desc, "FontFile3"))) which = 2;
    else if ((ff = pdf_dict_get(pd, desc, "FontFile"))) which = 3;
    if (ff && ff->type == PO_STREAM && f->kind != 3) {
        size_t n;
        const u8 *d = pdf_stream_data(pd, ff, &n, 0);
        if (d && n) {
            f->data = malloc(n);
            if (f->data) { memcpy(f->data, d, n); f->n = n; }
        }
        const char *st = pdf_name(pdf_dict_get(pd, ff, "Subtype"));
        if (f->data && which == 2 && st && !strcmp(st, "OpenType")) which = n >= 4 && !memcmp(f->data, "OTTO", 4) ? 4 : 1;
        if (f->data && which == 1 && tt_open(&f->tt, f->data, f->n) == 0) { f->prog = 1; f->fm[0] = f->fm[3] = 1.0 / f->tt.upem; }
        else if (f->data && (which == 2 || which == 4 || (which == 1 && f->n >= 4 && f->data[0] == 1))) {
            const u8 *cd = f->data;
            size_t cn = f->n, tl;
            if (which == 4 || (f->n >= 4 && !memcmp(f->data, "OTTO", 4))) { cd = tt_table(f->data, f->n, 0x43464620, &tl); cn = cd ? tl : 0; }
            if (cd && cff_open(&f->cff, cd, cn) == 0) { f->prog = 2; memcpy(f->fm, f->cff.fm, sizeof f->fm); }
        } else if (f->data && which == 3) {
            double l1 = 0;
            pdf_num(pdf_dict_get(pd, ff, "Length1"), &l1);
            f->t1 = calloc(1, sizeof *f->t1);
            if (f->t1 && t1_open(f->t1, f->data, f->n, (size_t)l1) == 0) { f->prog = 3; memcpy(f->fm, f->t1->fm, sizeof f->fm); }
            else { if (f->t1) t1_free(f->t1); free(f->t1); f->t1 = 0; }
        }
    }
    if (!f->prog && f->kind != 3) { f->ctfont = pf_system_font(base); f->fm[0] = f->fm[3] = 0.001; }
    /* simples: larguras, codificação e código → glifo */
    if (f->kind != 2) {
        double first = 0;
        pdf_num(pdf_dict_get(pd, fd, "FirstChar"), &first);
        pdf_obj *wa = pdf_dict_get(pd, fd, "Widths");
        for (u32 i = 0; wa && wa->type == PO_ARRAY && i < wa->n; i++) {
            int c = (int)first + (int)i;
            double v;
            if (c >= 0 && c < 256 && pdf_num(pdf_array_get(pd, wa, i), &v)) { f->width[c] = v; f->has_width[c] = 1; }
        }
        pdf_num(pdf_dict_get(pd, desc, "MissingWidth"), &f->missing);
        int prog_enc = 0;
        if (f->prog == 3) for (int i = 0; i < 256; i++) if (f->t1->enc[i]) { f->names[i] = strdup(f->t1->enc[i]); prog_enc = 1; }
        if (f->prog == 2 && !f->cff.cid) prog_enc = 1;          /* o CFF tem codificação própria; Differences por cima */
        if (f->kind == 3) prog_enc = 0;
        pf_encoding(pd, f, fd, prog_enc && !pdf_dict_get(pd, fd, "Encoding"));
        if (f->kind == 3 || f->prog == 1 || f->prog == 0) { if (!pdf_dict_get(pd, fd, "Encoding") && f->kind != 3 && !f->symbolic) pf_encoding(pd, f, fd, 0); }
        for (int c = 0; c < 256; c++) {
            const char *nm = f->names[c];
            int g = -1;
            if (f->prog == 1) {
                if (f->symbolic || !nm) {
                    static const u32 offs[4] = {0, 0xF000, 0xF100, 0xF200};
                    for (int k = 0; k < 4 && g <= 0; k++) g = tt_cmap(&f->tt, 3, 0, (u32)c + offs[k]);
                    if (g <= 0) g = tt_cmap(&f->tt, 1, 0, (u32)c);
                }
                if (g <= 0 && nm) {
                    u32 u = glyph_unicode(nm);
                    if (u) g = tt_cmap(&f->tt, 3, 1, u);
                    if (g <= 0 && u) g = tt_cmap(&f->tt, 0, 3, u);
                    if (g <= 0) g = tt_post_gid(&f->tt, nm);
                    if (g <= 0) {
                        for (int k = 0; k < 256 && g <= 0; k++) if (enc_mac[k] && !strcmp(enc_mac[k], nm)) g = tt_cmap(&f->tt, 1, 0, (u32)k);
                    }
                }
                if (g <= 0) g = tt_cmap(&f->tt, 3, 1, (u32)c);
            } else if (f->prog == 2) {
                if (nm) g = cff_name_gid(&f->cff, nm);
                if (g <= 0 && !nm) {
                    /* codificação interna do CFF (padrão): pelo StandardEncoding */
                    if (enc_std[c]) g = cff_name_gid(&f->cff, enc_std[c]);
                }
            } else if (f->prog == 3) {
                if (nm) g = t1_find(f->t1, nm);
            } else if (f->prog == 0 && f->ctfont) {
                u32 u = nm ? glyph_unicode(nm) : (u32)c;
                if (!u && !nm) u = (u32)c;
                u16 ch = (u16)u, gg = 0;
                if (u && CTFontGetGlyphsForCharacters(f->ctfont, &ch, &gg, 1)) g = gg;
            }
            f->gid[c] = g;
        }
    }
    /* ToUnicode */
    pdf_obj *tu = pdf_dict_get(pd, fd, "ToUnicode");
    if (tu && tu->type == PO_STREAM) cmap_parse(pd, tu, 0, f, 0);
    free(base);
    return f;
}
static void font_free(pfont *f) {
    if (!f) return;
    for (int i = 0; i < 256; i++) { free(f->names[i]); if (f->cache[i]) CGPathRelease(f->cache[i]); }
    for (u32 i = 0; i < f->nccache; i++) if (f->ccache[i]) CGPathRelease(f->ccache[i]);
    free(f->ccache);
    if (f->t1) { t1_free(f->t1); free(f->t1); }
    if (f->ctfont) CFRelease(f->ctfont);
    free(f->data); free(f->enc.r); free(f->cid2gid); free(f->w); free(f->tu);
    free(f);
}
static int pf_gid_of(const pfont *f, u32 code, int nb) {
    if (f->kind != 2) return code < 256 ? f->gid[code] : -1;
    u32 cid = pf_cid(f, code, nb);
    if (f->prog == 2) return cff_cid_gid(&f->cff, cid);
    if (f->prog == 1) {
        if (f->cid2gid) return cid < f->ncid2gid ? f->cid2gid[cid] : 0;
        return (int)cid;
    }
    if (f->prog == 0 && f->ctfont) {
        /* CID sem programa: só dá para desenhar via ToUnicode */
        for (int i = 0; i < f->ntu; i++) if (f->tu[i].code == code && f->tu[i].nu) {
            u16 g = 0, ch = f->tu[i].uni[0];
            if (CTFontGetGlyphsForCharacters(f->ctfont, &ch, &g, 1)) return g;
        }
        return -1;
    }
    return (int)cid;
}
static double pf_glyph_advance_prog(pfont *f, int gid) {
    if (gid < 0) return 0;
    if (f->prog == 1) return tt_advance(&f->tt, gid) * f->fm[0];
    if (f->prog == 0 && f->ctfont) { u16 g = (u16)gid; CGSize a = {0, 0}; CTFontGetAdvancesForGlyphs(f->ctfont, 0, &g, &a, 1); return a.width / 1000.0; }
    if (f->prog == 3 && gid < f->t1->ncs) {
        pen_t p = {CGPathCreateMutable(), 0, 0, 0};
        double w = t1_outline(f->t1, gid, &p);
        CGPathRelease(p.path);
        return w * f->fm[0];
    }
    return 0;
}
/* contorno do glifo em unidades da fonte (cache por código/gid) */
static void *pf_glyph_path(pfont *f, u32 code, int nb) {
    int gid = pf_gid_of(f, code, nb);
    if (gid < 0 && f->kind != 2) return 0;
    void **slot;
    if (f->kind != 2 && code < 256) slot = &f->cache[code];
    else {
        u32 key = gid < 0 ? 0 : (u32)gid;
        if (key >= 65536) return 0;
        if (key >= f->nccache) {
            u32 nc = key + 256;
            void **n2 = realloc(f->ccache, nc * sizeof(void *));
            if (!n2) return 0;
            memset(n2 + f->nccache, 0, (nc - f->nccache) * sizeof(void *));
            f->ccache = n2;
            f->nccache = nc;
        }
        slot = &f->ccache[key];
    }
    if (*slot) return *slot;
    pen_t pen = {CGPathCreateMutable(), 0, 0, 0};
    if (!pen.path) return 0;
    if (f->prog == 1) { double id[6] = {1, 0, 0, 1, 0, 0}; tt_outline(&f->tt, gid, &pen, id, 0); }
    else if (f->prog == 2) cff_outline(&f->cff, gid, &pen);
    else if (f->prog == 3) t1_outline(f->t1, gid, &pen);
    else if (f->ctfont && gid >= 0) {
        void *p = CTFontCreatePathForGlyph(f->ctfont, (u16)gid, 0);
        if (p) { CGPathRelease(pen.path); *slot = p; return p; }
    }
    pen_close(&pen);
    *slot = pen.path;
    return pen.path;
}
/* Unicode de um código (ToUnicode, senão o nome do glifo) */
static int pf_unicode(const pfont *f, u32 code, int nb, u16 *out, int max) {
    for (int i = 0; i < f->ntu; i++)
        if (f->tu[i].code == code && (f->tu[i].nb == nb || f->kind != 2)) {
            int k = f->tu[i].nu < max ? f->tu[i].nu : max;
            for (int j = 0; j < k; j++) out[j] = f->tu[i].uni[j];
            return k;
        }
    if (f->kind != 2 && code < 256 && f->names[code]) {
        u32 u = glyph_unicode(f->names[code]);
        if (u && max) { out[0] = (u16)u; return 1; }
    }
    if (f->kind != 2 && max && code >= 32 && code < 127) { out[0] = (u16)code; return 1; }
    return 0;
}
