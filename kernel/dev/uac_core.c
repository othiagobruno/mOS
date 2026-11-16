/* mOS — núcleo UAC (ver uac_core.h). Especificações: USB Audio 1.0 (4.3.2, 4.5.2, Formats 2.2.5) e 2.0 (4.7, 4.9.2,
 * 5.2.1 Layout 3 RANGE). Código original do mOS (MIT). */
#include "uac_core.h"

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t le24(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16; }
static uint32_t le32(const uint8_t *p) { return le24(p) | (uint32_t)p[3] << 24; }
#define CS_INTERFACE 0x24

uint16_t uac_parse_ac(const uint8_t *d, uint32_t len, uint8_t *clock_id) {
    uint16_t ver = 0;
    if (clock_id) *clock_id = 0;
    for (uint32_t o = 0; o + 3 <= len;) {
        uint8_t bl = d[o];
        if (bl < 3 || o + bl > len) break;
        if (d[o + 1] == CS_INTERFACE) {
            if (d[o + 2] == 0x01 && bl >= 5) ver = le16(d + o + 3);                       /* HEADER */
            else if (d[o + 2] == 0x0A && bl >= 4 && clock_id && !*clock_id) *clock_id = d[o + 3];   /* CLOCK_SOURCE (UAC2) */
        }
        o += bl;
    }
    return ver;
}

int uac_parse_as(const uint8_t *d, uint32_t len, uint16_t ver, uac_alt_info *a) {
    int pcm = 0, fmt = 0;
    a->nrates = 0; a->continuous = 0;
    for (uint32_t o = 0; o + 3 <= len;) {
        uint8_t bl = d[o];
        if (bl < 3 || o + bl > len) break;
        const uint8_t *p = d + o;
        o += bl;
        if (p[1] != CS_INTERFACE) continue;
        if (p[2] == 0x01) {   /* AS_GENERAL */
            a->terminal = bl > 3 ? p[3] : 0;
            if (ver >= 0x0200) {
                if (bl >= 11) { pcm = p[5] == 1 && (le32(p + 6) & 1); a->channels = p[10]; }   /* FORMAT_TYPE_I, PCM */
            } else if (bl >= 7) pcm = le16(p + 5) == 1;                                      /* wFormatTag PCM */
        } else if (p[2] == 0x02 && bl >= 6 && p[3] == 1) {   /* FORMAT_TYPE I */
            if (ver >= 0x0200) { a->subslot = p[4]; a->bits = p[5]; fmt = 1; }
            else if (bl >= 8) {
                a->channels = p[4]; a->subslot = p[5]; a->bits = p[6];
                uint8_t n = p[7];
                if (n == 0 && bl >= 14) { a->rates[0] = le24(p + 8); a->rates[1] = le24(p + 11); a->nrates = 2; a->continuous = 1; }
                else for (int i = 0; i < n && i < UAC_MAX_RATES && 8 + 3 * i + 3 <= bl; i++) a->rates[a->nrates++] = le24(p + 8 + 3 * i);
                fmt = 1;
            }
        }
    }
    if (!pcm || !fmt || !a->channels || a->subslot < 2 || a->subslot > 4) return -1;
    return 0;
}

int uac_parse_ranges(const uint8_t *d, uint32_t len, uint32_t *rates, int max, int *continuous) {
    if (continuous) *continuous = 0;
    if (len < 2) return 0;
    int n = le16(d), k = 0;
    for (int i = 0; i < n && 2 + 12 * i + 12 <= (int)len && k < max; i++) {
        uint32_t lo = le32(d + 2 + 12 * i), hi = le32(d + 6 + 12 * i), step = le32(d + 10 + 12 * i);
        if (lo == hi) { rates[k++] = lo; continue; }
        if (!step) { if (k + 2 <= max) { rates[k++] = lo; rates[k++] = hi; if (continuous) *continuous = 1; } continue; }
        for (uint32_t r = lo; r <= hi && k < max; r += step) rates[k++] = r;
    }
    return k;
}

static int has_rate(const uac_alt_info *a, uint32_t r) {
    if (a->continuous) return a->nrates >= 2 && a->rates[0] <= r && r <= a->rates[1];
    for (int i = 0; i < a->nrates; i++) if (a->rates[i] == r) return 1;
    return 0;
}
uint32_t uac_pick_rate(const uac_alt_info *a) {
    if (!a->nrates) return 48000;
    if (has_rate(a, 48000)) return 48000;
    uint32_t best = a->rates[0];
    for (int i = 1; i < a->nrates; i++) {
        uint32_t r = a->rates[i];
        long long db = (long long)best - 48000, dr = (long long)r - 48000;
        if ((dr < 0 ? -dr : dr) < (db < 0 ? -db : db)) best = r;
    }
    if (a->continuous) best = 48000 < a->rates[0] ? a->rates[0] : a->rates[1];
    return best;
}
int uac_score(const uac_alt_info *a, uint32_t rate) {
    int s = 0;
    if (rate == 48000) s += 1000; else if (rate == 44100) s += 800; else s += (int)(rate / 100);
    if (a->bits == 16 || a->subslot == 2) s += 100; else s += 50;
    if (a->channels <= 2) s += 20 + a->channels; else s += 5;
    return s;
}

uint32_t uac_pack(const int16_t *s, uint32_t frames, int ch, int ss, uint8_t *d) {
    uint8_t *o = d;
    for (uint32_t i = 0; i < frames; i++) {
        for (int c = 0; c < ch; c++) {
            int32_t v = ch == 1 ? ((int32_t)s[i * 2] + s[i * 2 + 1]) / 2 : c < 2 ? s[i * 2 + c] : 0;
            for (int b = 0; b < ss - 2; b++) *o++ = 0;   /* bits de baixo em zero (24/32 bits) */
            *o++ = (uint8_t)v; *o++ = (uint8_t)((uint32_t)v >> 8);
        }
    }
    return (uint32_t)(o - d);
}
uint32_t uac_frames_for(uint32_t rate, uint32_t interval_us, uint64_t *acc) {
    *acc += (uint64_t)rate * interval_us;
    uint32_t n = (uint32_t)(*acc / 1000000u);
    *acc -= (uint64_t)n * 1000000u;
    return n;
}
void uac_rs_init(uac_rs *r, uint32_t rate) { r->in_rate = rate ? rate : 48000; r->pos = 0; r->last_l = r->last_r = 0; r->have_last = 0; }
static int16_t sample(const uint8_t *p, int subslot) {
    if (subslot == 2) return (int16_t)(p[0] | p[1] << 8);
    if (subslot == 3) return (int16_t)(p[1] | p[2] << 8);   /* os 16 bits de cima */
    return (int16_t)(p[2] | p[3] << 8);
}
/* pos: posição na entrada em 1/65536 de quadro; cada saída anda in_rate/48000 */
uint32_t uac_convert(uac_rs *r, const uint8_t *src, uint32_t bytes, int ch, int ss, int16_t *out, uint32_t cap) {
    if (ch < 1 || ss < 2 || ss > 4) return 0;
    uint32_t fb = (uint32_t)(ch * ss), nin = bytes / fb, n = 0;
    if (!nin) return 0;
    uint64_t step = ((uint64_t)r->in_rate << 16) / 48000;
    /* entrada estendida: o último quadro do pacote anterior vem antes deste (índice -1) */
    while (n < cap) {
        uint64_t ip = r->pos >> 16;   /* índice relativo ao início deste pacote, contando o anterior como -1 => +1 */
        if (ip + 1 > nin) break;      /* precisa de ip e ip+1 em [-1, nin-1] */
        uint32_t t = (uint32_t)(r->pos & 0xFFFF);
        int16_t l0, r0, l1, r1;
        if (ip == 0) {
            if (!r->have_last) { const uint8_t *q = src; l0 = sample(q, ss); r0 = ch > 1 ? sample(q + ss, ss) : l0; }
            else { l0 = r->last_l; r0 = r->last_r; }
        } else { const uint8_t *q = src + (ip - 1) * fb; l0 = sample(q, ss); r0 = ch > 1 ? sample(q + ss, ss) : l0; }
        const uint8_t *q1 = src + ip * fb;
        l1 = sample(q1, ss); r1 = ch > 1 ? sample(q1 + ss, ss) : l1;
        out[n * 2] = (int16_t)(l0 + (((int32_t)l1 - l0) * (int32_t)t >> 16));
        out[n * 2 + 1] = (int16_t)(r0 + (((int32_t)r1 - r0) * (int32_t)t >> 16));
        n++;
        r->pos += step;
    }
    const uint8_t *lq = src + (nin - 1) * fb;
    r->last_l = sample(lq, ss); r->last_r = ch > 1 ? sample(lq + ss, ss) : r->last_l; r->have_last = 1;
    r->pos -= (uint64_t)nin << 16;
    return n;
}
