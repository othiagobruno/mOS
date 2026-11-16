/* Demuxer ISO BMFF do mOS (ISO/IEC 14496-12 e QuickTime File Format). Implementação própria.
 * Ver mp4demux.h. Não descriptografa: faixa 'encv'/'enca' sai com encrypted = 1. */
#include "mp4demux.h"
#include <stdlib.h>
#include <string.h>

#define FCC MP4_FOURCC

static uint32_t rb16(const uint8_t *p) { return (uint32_t)p[0] << 8 | p[1]; }
static uint32_t rb32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint64_t rb64(const uint8_t *p) { return (uint64_t)rb32(p) << 32 | rb32(p + 4); }

/* Tabelas cruas de stbl, válidas enquanto o buffer de moov vive. */
typedef struct {
    const uint8_t *stts, *ctts, *stss, *stsc, *stsz, *stz2, *stco, *co64;
    uint32_t stts_n, ctts_n, stss_n, stsc_n, stco_n;
} stbl_raw;

typedef struct {
    mp4_file *f;
    mp4_track *cur;
    stbl_raw raw;
} pctx;

static mp4_track *new_track(mp4_file *f) {
    mp4_track *n = realloc(f->t, (f->ntracks + 1) * sizeof *n);
    if (!n) return 0;
    f->t = n;
    mp4_track *t = &f->t[f->ntracks++];
    memset(t, 0, sizeof *t);
    t->edit_media_time = -1;
    t->enabled = 1;
    return t;
}

static int set_cfg(mp4_track *t, uint32_t type, const uint8_t *p, uint32_t n) {
    if (t->cfg) return 0; /* a primeira entrada de stsd manda */
    t->cfg = malloc(n ? n : 1);
    if (!t->cfg) return -3;
    memcpy(t->cfg, p, n);
    t->cfg_len = n;
    t->cfg_type = type;
    return 0;
}

static int push_sample(mp4_track *t, const mp4_sample *s) {
    if (t->nsamples == t->cap) {
        uint32_t nc = t->cap ? t->cap * 2 : 256;
        if (nc < t->cap) return -3;
        mp4_sample *n = realloc(t->s, (size_t)nc * sizeof *n);
        if (!n) return -3;
        t->s = n;
        t->cap = nc;
    }
    t->s[t->nsamples++] = *s;
    return 0;
}

/* Descritor MPEG-4 (esds): tamanho em até 4 bytes de 7 bits. */
static int desc_len(const uint8_t **p, const uint8_t *e, uint32_t *len) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        if (*p >= e) return -1;
        uint8_t c = *(*p)++;
        v = v << 7 | (c & 0x7F);
        if (!(c & 0x80)) break;
    }
    *len = v;
    return 0;
}

static int parse_esds(mp4_track *t, const uint8_t *p, uint32_t n) {
    if (n < 4) return 0;
    const uint8_t *e = p + n;
    p += 4; /* versão/flags */
    while (p < e) {
        uint8_t tag = *p++;
        uint32_t len;
        if (desc_len(&p, e, &len)) return 0;
        const uint8_t *body = p;
        if (len > (uint32_t)(e - body)) len = (uint32_t)(e - body);
        if (tag == 3) { /* ES_Descriptor */
            if (len < 3) return 0;
            uint8_t fl = body[2];
            const uint8_t *q = body + 3;
            if (fl & 0x80) q += 2;
            if (fl & 0x40) { if (q >= body + len) return 0; q += 1 + *q; }
            if (fl & 0x20) q += 2;
            if (q > body + len) return 0;
            p = q;
            e = body + len;
            continue;
        }
        if (tag == 4) { /* DecoderConfigDescriptor */
            if (len < 13) return 0;
            if (!t->oti) t->oti = body[0];
            p = body + 13;
            e = body + len;
            continue;
        }
        if (tag == 5) return set_cfg(t, FCC('e', 's', 'd', 's'), body, len);
        p = body + len;
    }
    return 0;
}

static int walk(pctx *c, const uint8_t *p, uint64_t n, uint32_t parent);

/* Entrada de stsd: tipo já lido; p aponta para o corpo. */
static int sample_entry(pctx *c, uint32_t type, const uint8_t *p, uint64_t n) {
    mp4_track *t = c->cur;
    if (!t) return 0;
    int first = !t->codec;
    uint64_t kids;
    if (t->handler == FCC('v', 'i', 'd', 'e')) {
        if (n < 78) return 0;
        if (first) { t->width = rb16(p + 24); t->height = rb16(p + 26); }
        kids = 78;
    } else if (t->handler == FCC('s', 'o', 'u', 'n')) {
        if (n < 28) return 0;
        uint32_t ver = rb16(p + 8);
        if (first) {
            t->channels = rb16(p + 16);
            t->bits = rb16(p + 18);
            t->sample_rate = rb32(p + 24) >> 16;
        }
        kids = 28;
        if (ver == 1) {
            if (n < 44) return 0;
            if (first) t->qt_bytes_per_frame = rb32(p + 36);
            kids = 44;
        } else if (ver == 2) {
            if (n < 64) return 0;
            if (first) {
                uint64_t bitsr = rb64(p + 32);
                double d;
                memcpy(&d, &bitsr, 8);
                t->sample_rate = (uint32_t)(d + 0.5);
                t->channels = rb32(p + 40);
                t->bits = rb32(p + 48);
                t->qt_bytes_per_frame = rb32(p + 52);
            }
            kids = 64;
        }
    } else {
        kids = 8; /* texto e metadados: 6 reservados + índice de dref */
    }
    if (!first) return 0;
    t->codec = type;
    if (type == FCC('e', 'n', 'c', 'v') || type == FCC('e', 'n', 'c', 'a')) t->encrypted = 1;
    if (kids < n) return walk(c, p + kids, n - kids, type);
    return 0;
}

static int parse_stsd(pctx *c, const uint8_t *p, uint64_t n) {
    if (n < 8) return 0;
    uint32_t cnt = rb32(p + 4);
    uint64_t at = 8;
    for (uint32_t i = 0; i < cnt && at + 8 <= n; i++) {
        uint64_t sz = rb32(p + at);
        uint32_t ty = rb32(p + at + 4);
        if (sz < 8 || at + sz > n) return -2;
        int r = sample_entry(c, ty, p + at + 8, sz - 8);
        if (r) return r;
        at += sz;
    }
    return 0;
}

typedef struct { const uint8_t *tab; uint32_t n, i, left, val; } runs;
static void runs_init(runs *r, const uint8_t *tab, uint32_t n) {
    r->tab = tab; r->n = n; r->i = 0;
    r->left = n ? rb32(tab) : 0;
    r->val = n ? rb32(tab + 4) : 0;
}
static uint32_t runs_next(runs *r) {
    while (!r->left && r->i + 1 < r->n) {
        r->i++;
        r->left = rb32(r->tab + (uint64_t)r->i * 8);
        r->val = rb32(r->tab + (uint64_t)r->i * 8 + 4);
    }
    if (r->left) r->left--;
    return r->val;
}

static int build_samples(mp4_track *t, const stbl_raw *r) {
    uint32_t count = 0, fixed = 0, field = 32;
    const uint8_t *sizes = 0;
    if (r->stsz) { fixed = rb32(r->stsz + 4); count = rb32(r->stsz + 8); sizes = r->stsz + 12; }
    else if (r->stz2) { field = r->stz2[7]; count = rb32(r->stz2 + 8); sizes = r->stz2 + 12; }
    else return 0;
    if (!count || !r->stsc || !(r->stco || r->co64)) return 0;
    if (field != 4 && field != 8 && field != 16 && field != 32) return -2;
    /* PCM do QuickTime: stsz fixo pequeno e uma "amostra" por quadro. Junta o pedaço numa amostra. */
    int pcm_chunks = t->handler == FCC('s', 'o', 'u', 'n') && fixed && fixed <= 64 && count > 4096;
    uint32_t nchunks = r->stco_n, si = 0;
    runs stts, ctts;
    runs_init(&stts, r->stts, r->stts_n);
    runs_init(&ctts, r->ctts, r->ctts_n);
    int64_t dts = 0;
    for (uint32_t e = 0; e < r->stsc_n && si < count; e++) {
        const uint8_t *q = r->stsc + (uint64_t)e * 12;
        uint32_t first = rb32(q), spc = rb32(q + 4), desc = rb32(q + 8);
        uint32_t last = e + 1 < r->stsc_n ? rb32(q + 12) : nchunks + 1;
        if (!first || last < first || !spc) return -2;
        uint8_t di = (uint8_t)(desc ? desc - 1 : 0);
        for (uint32_t ch = first; ch < last && ch <= nchunks && si < count; ch++) {
            uint64_t off = r->co64 ? rb64(r->co64 + (uint64_t)(ch - 1) * 8) : rb32(r->stco + (uint64_t)(ch - 1) * 4);
            if (pcm_chunks) {
                uint32_t k = spc < count - si ? spc : count - si;
                uint64_t dur = 0;
                for (uint32_t j = 0; j < k; j++) dur += runs_next(&stts);
                mp4_sample s = {off, fixed * k, (uint32_t)dur, dts, 0, 1, di};
                dts += (int64_t)dur;
                si += k;
                int rc = push_sample(t, &s);
                if (rc) return rc;
                continue;
            }
            for (uint32_t j = 0; j < spc && si < count; j++, si++) {
                uint32_t sz;
                if (fixed) sz = fixed;
                else if (field == 32) sz = rb32(sizes + (uint64_t)si * 4);
                else if (field == 16) sz = rb16(sizes + (uint64_t)si * 2);
                else if (field == 8) sz = sizes[si];
                else sz = (si & 1) ? (sizes[si / 2] & 15) : (sizes[si / 2] >> 4);
                uint32_t d = runs_next(&stts);
                int32_t cto = r->ctts_n ? (int32_t)runs_next(&ctts) : 0;
                mp4_sample s = {off, sz, d, dts, cto, (uint8_t)(r->stss ? 0 : 1), di};
                dts += d;
                off += sz;
                int rc = push_sample(t, &s);
                if (rc) return rc;
            }
        }
    }
    if (r->stss) {
        for (uint32_t i = 0; i < r->stss_n; i++) {
            uint32_t k = rb32(r->stss + (uint64_t)i * 4);
            if (k >= 1 && k <= t->nsamples) t->s[k - 1].sync = 1;
        }
    }
    t->next_dts = dts;
    return 0;
}

static void lang(mp4_track *t, uint32_t l) {
    t->lang[0] = (char)(((l >> 10) & 31) + 0x60);
    t->lang[1] = (char)(((l >> 5) & 31) + 0x60);
    t->lang[2] = (char)((l & 31) + 0x60);
    t->lang[3] = 0;
}

#define TABLE(field, nfield, esz, hdr) \
    do { \
        if (bn >= 8) { \
            c->raw.nfield = rb32(b + 4); \
            if ((hdr) + (uint64_t)c->raw.nfield * (esz) > bn) return -2; \
            c->raw.field = b + (hdr); \
        } \
    } while (0)

static int walk(pctx *c, const uint8_t *p, uint64_t n, uint32_t parent) {
    mp4_file *f = c->f;
    uint64_t at = 0;
    while (at + 8 <= n) {
        uint64_t sz = rb32(p + at);
        uint32_t ty = rb32(p + at + 4);
        uint64_t hdr = 8;
        if (sz == 1) {
            if (at + 16 > n) return -2;
            sz = rb64(p + at + 8);
            hdr = 16;
        } else if (sz == 0) {
            if (ty == 0) break; /* terminador de 4 zeros do QuickTime */
            sz = n - at;
        }
        if (sz < hdr || at + sz > n) return -2;
        const uint8_t *b = p + at + hdr;
        uint64_t bn = sz - hdr;
        int rc = 0;
        mp4_track *t = c->cur;
        switch (ty) {
        case FCC('m', 'o', 'o', 'v'): case FCC('m', 'd', 'i', 'a'): case FCC('m', 'i', 'n', 'f'):
        case FCC('s', 't', 'b', 'l'): case FCC('e', 'd', 't', 's'): case FCC('m', 'v', 'e', 'x'):
        case FCC('w', 'a', 'v', 'e'): case FCC('s', 'i', 'n', 'f'):
            rc = walk(c, b, bn, ty);
            break;
        case FCC('t', 'r', 'a', 'k'): {
            if (!new_track(f)) return -3;
            memset(&c->raw, 0, sizeof c->raw);
            c->cur = &f->t[f->ntracks - 1];
            rc = walk(c, b, bn, ty);
            if (!rc) rc = build_samples(&f->t[f->ntracks - 1], &c->raw);
            c->cur = 0;
            break;
        }
        case FCC('m', 'v', 'h', 'd'):
            if (b[0] == 1) { if (bn >= 32) { f->timescale = rb32(b + 20); f->duration = rb64(b + 24); } }
            else if (bn >= 20) { f->timescale = rb32(b + 12); f->duration = rb32(b + 16); }
            break;
        case FCC('t', 'k', 'h', 'd'): {
            if (!t || bn < 84) break;
            t->enabled = (b[3] & 1) != 0;
            t->id = rb32(b + (b[0] == 1 ? 20 : 12));
            uint64_t m = b[0] == 1 ? 52 : 40; /* matriz depois de duração, reservado, camada, grupo, volume */
            if (m + 44 > bn) break;
            for (int i = 0; i < 9; i++) t->matrix[i] = (int32_t)rb32(b + m + (uint64_t)i * 4);
            t->disp_w = rb32(b + m + 36) / 65536.0;
            t->disp_h = rb32(b + m + 40) / 65536.0;
            break;
        }
        case FCC('m', 'd', 'h', 'd'):
            if (!t) break;
            if (b[0] == 1) { if (bn >= 34) { t->timescale = rb32(b + 20); t->duration = rb64(b + 24); lang(t, rb16(b + 32)); } }
            else if (bn >= 22) { t->timescale = rb32(b + 12); t->duration = rb32(b + 16); lang(t, rb16(b + 20)); }
            break;
        case FCC('h', 'd', 'l', 'r'):
            /* o hdlr de minf (QuickTime 'dhlr') não troca o tipo de mídia */
            if (t && parent == FCC('m', 'd', 'i', 'a') && bn >= 12) t->handler = rb32(b + 8);
            break;
        case FCC('e', 'l', 's', 't'): {
            if (!t || bn < 8) break;
            uint32_t cnt = rb32(b + 4), es = b[0] == 1 ? 20 : 12;
            for (uint32_t i = 0; i < cnt && 8 + (uint64_t)(i + 1) * es <= bn; i++) {
                const uint8_t *q = b + 8 + (uint64_t)i * es;
                uint64_t dur = b[0] == 1 ? rb64(q) : rb32(q);
                int64_t mt = b[0] == 1 ? (int64_t)rb64(q + 8) : (int64_t)(int32_t)rb32(q + 4);
                if (mt == -1) { t->edit_empty += dur; continue; }
                t->edit_media_time = mt;
                break;
            }
            break;
        }
        case FCC('s', 't', 's', 'd'): if (t) rc = parse_stsd(c, b, bn); break;
        case FCC('s', 't', 't', 's'): TABLE(stts, stts_n, 8, 8); break;
        case FCC('c', 't', 't', 's'): TABLE(ctts, ctts_n, 8, 8); break;
        case FCC('s', 't', 's', 's'): TABLE(stss, stss_n, 4, 8); break;
        case FCC('s', 't', 's', 'c'): TABLE(stsc, stsc_n, 12, 8); break;
        case FCC('s', 't', 'c', 'o'): TABLE(stco, stco_n, 4, 8); break;
        case FCC('c', 'o', '6', '4'): TABLE(co64, stco_n, 8, 8); break;
        case FCC('s', 't', 's', 'z'):
            if (bn >= 12) {
                if (!rb32(b + 4) && 12 + (uint64_t)rb32(b + 8) * 4 > bn) return -2;
                c->raw.stsz = b;
            }
            break;
        case FCC('s', 't', 'z', '2'):
            if (bn >= 12) {
                if (12 + ((uint64_t)rb32(b + 8) * b[7] + 7) / 8 > bn) return -2;
                c->raw.stz2 = b;
            }
            break;
        case FCC('e', 's', 'd', 's'): if (t) rc = parse_esds(t, b, (uint32_t)bn); break;
        case FCC('a', 'v', 'c', 'C'): case FCC('h', 'v', 'c', 'C'): case FCC('d', 'O', 'p', 's'):
        case FCC('v', 'p', 'c', 'C'): case FCC('a', 'v', '1', 'C'): case FCC('d', 'f', 'L', 'a'):
        case FCC('d', 'a', 'c', '3'): case FCC('d', 'e', 'c', '3'):
            if (t) rc = set_cfg(t, ty, b, (uint32_t)bn);
            break;
        case FCC('a', 'l', 'a', 'c'):
            /* a entrada 'alac' de stsd tem dentro outra caixa 'alac' (versão/flags + cookie de 24 bytes) */
            if (t && parent != FCC('s', 't', 's', 'd')) rc = set_cfg(t, ty, b, (uint32_t)bn);
            break;
        case FCC('f', 'r', 'm', 'a'):
            if (t && parent == FCC('s', 'i', 'n', 'f') && bn >= 4) t->codec = rb32(b);
            break;
        case FCC('t', 'r', 'e', 'x'):
            if (bn >= 24) {
                uint32_t id = rb32(b + 4);
                for (uint32_t i = 0; i < f->ntracks; i++) if (f->t[i].id == id) {
                    f->t[i].trex_desc = rb32(b + 8);
                    f->t[i].trex_dur = rb32(b + 12);
                    f->t[i].trex_size = rb32(b + 16);
                    f->t[i].trex_flags = rb32(b + 20);
                }
                f->fragmented = 1;
            }
            break;
        default:
            break;
        }
        if (rc) return rc;
        at += sz;
    }
    return 0;
}

static int parse_trun(mp4_track *t, const uint8_t *b, uint64_t bn, uint64_t base, uint64_t *data_end,
                      uint32_t desc, uint32_t ddur, uint32_t dsize, uint32_t dflags) {
    uint32_t tf = rb32(b) & 0xFFFFFF, cnt = rb32(b + 4);
    uint64_t o = 8, off = *data_end ? *data_end : base;
    if (tf & 0x1) { if (o + 4 > bn) return -2; off = base + (uint64_t)(int64_t)(int32_t)rb32(b + o); o += 4; }
    uint32_t first_flags = dflags;
    int has_first = 0;
    if (tf & 0x4) { if (o + 4 > bn) return -2; first_flags = rb32(b + o); o += 4; has_first = 1; }
    uint32_t per = ((tf & 0x100) ? 4 : 0) + ((tf & 0x200) ? 4 : 0) + ((tf & 0x400) ? 4 : 0) + ((tf & 0x800) ? 4 : 0);
    if (o + (uint64_t)cnt * per > bn) return -2;
    for (uint32_t i = 0; i < cnt; i++) {
        uint32_t d = ddur, s = dsize, fl = dflags;
        int32_t cto = 0;
        if (tf & 0x100) { d = rb32(b + o); o += 4; }
        if (tf & 0x200) { s = rb32(b + o); o += 4; }
        if (tf & 0x400) { fl = rb32(b + o); o += 4; }
        if (tf & 0x800) { cto = (int32_t)rb32(b + o); o += 4; }
        if (i == 0 && has_first) fl = first_flags;
        mp4_sample sm = {off, s, d, t->next_dts, cto, (uint8_t)!((fl >> 16) & 1), (uint8_t)(desc ? desc - 1 : 0)};
        int rc = push_sample(t, &sm);
        if (rc) return rc;
        t->next_dts += d;
        off += s;
    }
    *data_end = off;
    return 0;
}

/* moof: traf/tfhd/tfdt/trun. moof_off é a posição do moof no arquivo. */
static int parse_moof(mp4_file *f, const uint8_t *p, uint64_t n, uint64_t moof_off) {
    uint64_t at = 0;
    while (at + 8 <= n) {
        uint64_t sz = rb32(p + at);
        uint32_t ty = rb32(p + at + 4);
        if (sz < 8 || at + sz > n) return -2;
        if (ty == FCC('t', 'r', 'a', 'f')) {
            const uint8_t *q = p + at + 8;
            uint64_t qn = sz - 8;
            mp4_track *t = 0;
            uint32_t desc = 0, ddur = 0, dsize = 0, dflags = 0;
            uint64_t base = moof_off, data_end = 0;
            for (int pass = 0; pass < 2; pass++) {
                uint64_t k = 0;
                while (k + 8 <= qn) {
                    uint64_t bs = rb32(q + k);
                    uint32_t bt = rb32(q + k + 4);
                    if (bs < 8 || k + bs > qn) return -2;
                    const uint8_t *b = q + k + 8;
                    uint64_t bn = bs - 8;
                    if (pass == 0 && bt == FCC('t', 'f', 'h', 'd') && bn >= 8) {
                        uint32_t flags = rb32(b) & 0xFFFFFF, id = rb32(b + 4);
                        for (uint32_t i = 0; i < f->ntracks; i++) if (f->t[i].id == id) t = &f->t[i];
                        if (!t) break;
                        desc = t->trex_desc; ddur = t->trex_dur; dsize = t->trex_size; dflags = t->trex_flags;
                        uint64_t o = 8;
                        if (flags & 0x1) { if (o + 8 > bn) return -2; base = rb64(b + o); o += 8; }
                        if (flags & 0x2) { if (o + 4 > bn) return -2; desc = rb32(b + o); o += 4; }
                        if (flags & 0x8) { if (o + 4 > bn) return -2; ddur = rb32(b + o); o += 4; }
                        if (flags & 0x10) { if (o + 4 > bn) return -2; dsize = rb32(b + o); o += 4; }
                        if (flags & 0x20) { if (o + 4 > bn) return -2; dflags = rb32(b + o); o += 4; }
                    } else if (pass == 0 && bt == FCC('t', 'f', 'd', 't') && bn >= 8 && t) {
                        t->next_dts = b[0] == 1 && bn >= 12 ? (int64_t)rb64(b + 4) : (int64_t)rb32(b + 4);
                    } else if (pass == 1 && bt == FCC('t', 'r', 'u', 'n') && bn >= 8) {
                        int rc = parse_trun(t, b, bn, base, &data_end, desc, ddur, dsize, dflags);
                        if (rc) return rc;
                    }
                    k += bs;
                }
                if (!t) break;
            }
        }
        at += sz;
    }
    return 0;
}

void mp4_close(mp4_file *f) {
    if (!f) return;
    for (uint32_t i = 0; i < f->ntracks; i++) { free(f->t[i].s); free(f->t[i].cfg); }
    free(f->t);
    f->t = 0;
    f->ntracks = 0;
}

#define MOOV_MAX (256u << 20)

static int open_core(mp4_file *f, mp4_read_fn read, void *ctx, uint64_t size) {
    f->read = read;
    f->ctx = ctx;
    f->size = size;
    uint8_t h[16];
    if (size < 8 || read(ctx, 0, h, 8)) return -1;
    uint32_t t0 = rb32(h + 4);
    if (t0 != FCC('f', 't', 'y', 'p') && t0 != FCC('m', 'o', 'o', 'v') && t0 != FCC('m', 'd', 'a', 't') &&
        t0 != FCC('w', 'i', 'd', 'e') && t0 != FCC('f', 'r', 'e', 'e') && t0 != FCC('s', 'k', 'i', 'p') &&
        t0 != FCC('s', 't', 'y', 'p') && t0 != FCC('p', 'n', 'o', 't'))
        return -1;
    int saw_moov = 0;
    uint64_t at = 0;
    while (at + 8 <= size) {
        if (read(ctx, at, h, size - at >= 16 ? 16 : 8)) break;
        uint64_t sz = rb32(h);
        uint32_t ty = rb32(h + 4);
        uint64_t hdr = 8;
        if (sz == 1) { if (at + 16 > size) break; sz = rb64(h + 8); hdr = 16; }
        else if (sz == 0) sz = size - at;
        if (sz < hdr) { f->err = "caixa com tamanho inválido"; mp4_close(f); return -2; }
        if (sz > size - at) sz = size - at; /* arquivo truncado: lê o que há */
        if (ty == FCC('f', 't', 'y', 'p') && sz >= 16) {
            f->brand = rb32(h + 8);
        } else if ((ty == FCC('m', 'o', 'o', 'v') && !saw_moov) || (ty == FCC('m', 'o', 'o', 'f') && saw_moov)) {
            uint64_t bn = sz - hdr;
            if (bn > MOOV_MAX) { f->err = "índice grande demais"; mp4_close(f); return -3; }
            uint8_t *buf = malloc(bn ? bn : 1);
            if (!buf) { mp4_close(f); return -3; }
            if (read(ctx, at + hdr, buf, (uint32_t)bn)) { free(buf); f->err = "leitura falhou"; mp4_close(f); return -2; }
            int rc;
            if (ty == FCC('m', 'o', 'o', 'v')) {
                pctx c;
                memset(&c, 0, sizeof c);
                c.f = f;
                rc = walk(&c, buf, bn, FCC('m', 'o', 'o', 'v'));
                saw_moov = 1;
            } else {
                rc = parse_moof(f, buf, bn, at);
                f->fragmented = 1;
            }
            free(buf);
            if (rc) { f->err = rc == -3 ? "sem memória" : "índice corrompido"; mp4_close(f); return rc; }
        }
        at += sz;
    }
    if (!saw_moov) { f->err = "sem moov"; mp4_close(f); return -2; }
    return 0;
}

int mp4_open(mp4_file *f, mp4_read_fn read, void *ctx, uint64_t size) {
    memset(f, 0, sizeof *f);
    return open_core(f, read, ctx, size);
}

static int mem_read(void *ctx, uint64_t off, void *dst, uint32_t n) {
    mp4_file *f = ctx;
    if (off > f->size || n > f->size - off) return -1;
    memcpy(dst, f->mem_b + off, n);
    return 0;
}

int mp4_open_mem(mp4_file *f, const void *bytes, uint64_t n) {
    memset(f, 0, sizeof *f);
    f->mem_b = bytes;
    return open_core(f, mem_read, f, n);
}

int mp4_read_sample(mp4_file *f, const mp4_track *t, uint32_t i, void *dst) {
    if (!f || !t || i >= t->nsamples) return -1;
    const mp4_sample *s = &t->s[i];
    if (s->off > f->size || s->size > f->size - s->off) return -2;
    return f->read(f->ctx, s->off, dst, s->size) ? -2 : 0;
}

int mp4_find_track(const mp4_file *f, uint32_t handler) {
    for (uint32_t i = 0; i < f->ntracks; i++)
        if (f->t[i].handler == handler && f->t[i].nsamples) return (int)i;
    return -1;
}

static int64_t edit0(const mp4_track *t) { return t->edit_media_time > 0 ? t->edit_media_time : 0; }

double mp4_pts(const mp4_track *t, uint32_t i) {
    if (!t || i >= t->nsamples || !t->timescale) return 0;
    return (double)(t->s[i].dts + t->s[i].cto - edit0(t)) / (double)t->timescale;
}

double mp4_track_seconds(const mp4_track *t) {
    if (!t || !t->timescale) return 0;
    if (!t->nsamples) return (double)t->duration / t->timescale;
    const mp4_sample *l = &t->s[t->nsamples - 1];
    int64_t end = l->dts + l->dur - edit0(t);
    return end > 0 ? (double)end / t->timescale : 0;
}

uint32_t mp4_sample_at(const mp4_track *t, double sec, uint32_t *keyframe) {
    if (!t || !t->nsamples) { if (keyframe) *keyframe = 0; return 0; }
    int64_t want = (int64_t)(sec * t->timescale) + edit0(t);
    uint32_t lo = 0, hi = t->nsamples - 1;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo + 1) / 2;
        if (t->s[mid].dts <= want) lo = mid; else hi = mid - 1;
    }
    /* com B-frames a apresentação não segue a decodificação: procura em volta */
    uint32_t best = lo;
    int64_t bestpts = -0x7fffffffffffffffLL - 1;
    uint32_t from = lo > 32 ? lo - 32 : 0, to = lo + 32 < t->nsamples ? lo + 32 : t->nsamples - 1;
    for (uint32_t i = from; i <= to; i++) {
        int64_t p = t->s[i].dts + t->s[i].cto;
        if (p <= want && p > bestpts) { bestpts = p; best = i; }
    }
    if (keyframe) {
        uint32_t k = best;
        while (k > 0 && !t->s[k].sync) k--;
        *keyframe = k;
    }
    return best;
}
