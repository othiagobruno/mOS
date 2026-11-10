#include "avconv_core.h"
#include <stdlib.h>
#include <string.h>

float avc_get(const avc_buf *b, uint32_t ch, uint32_t frame) {
    uint64_t i = (uint64_t)frame * (b->fmt.interleaved ? b->fmt.channels : 1);
    switch (b->fmt.common) {
    case AVC_F32: return ((const float *)b->planes[ch])[i];
    case AVC_F64: return (float)((const double *)b->planes[ch])[i];
    case AVC_I16: return (float)((const int16_t *)b->planes[ch])[i] / 32768.0f;
    case AVC_I32: return (float)((double)((const int32_t *)b->planes[ch])[i] / 2147483648.0);
    default: return 0;
    }
}

void avc_put(avc_buf *b, uint32_t ch, uint32_t frame, float v) {
    uint64_t i = (uint64_t)frame * (b->fmt.interleaved ? b->fmt.channels : 1);
    if (v > 1) v = 1;
    if (v < -1) v = -1;
    switch (b->fmt.common) {
    case AVC_F32: ((float *)b->planes[ch])[i] = v; break;
    case AVC_F64: ((double *)b->planes[ch])[i] = v; break;
    case AVC_I16: { float s = v * 32767.0f; ((int16_t *)b->planes[ch])[i] = (int16_t)(s < 0 ? s - 0.5f : s + 0.5f); break; }
    case AVC_I32: { double s = (double)v * 2147483647.0; ((int32_t *)b->planes[ch])[i] = (int32_t)(s < 0 ? s - 0.5 : s + 0.5); break; }
    default: break;
    }
}

/* Sem mapa: mesmos canais copiam; saída mono é a média; entrada mono vai a todos; o resto copia os primeiros. */
float avc_mix(const avc_route *r, const float *frame, uint32_t ic, uint32_t oc, uint32_t och) {
    if (r && r->nmap >= 0) {
        if ((long)oc >= r->nmap) return 0;
        long src = r->map[oc];
        return src >= 0 && (uint32_t)src < ic ? frame[src] : 0;
    }
    if (ic == och) return frame[oc];
    if (och == 1) { float s = 0; for (uint32_t c = 0; c < ic; c++) s += frame[c]; return s / (float)ic; }
    if (ic == 1) return frame[0];
    return oc < ic ? frame[oc] : 0;
}

int avc_direct(const avc_route *r, const avc_buf *in, avc_buf *out) {
    uint32_t ic = in->fmt.channels, och = out->fmt.channels;
    uint32_t n = in->len < out->cap ? in->len : out->cap;
    float fr[32];
    if (in->fmt.rate != out->fmt.rate) return -1;
    if (ic > 32) ic = 32;
    for (uint32_t f = 0; f < n; f++) {
        for (uint32_t c = 0; c < ic; c++) fr[c] = avc_get(in, c, f);
        for (uint32_t oc = 0; oc < och; oc++) avc_put(out, oc, f, avc_mix(r, fr, ic, oc, och));
    }
    out->len = n;
    return 0;
}

static int stage_add(avc_state *s, uint32_t ic, const avc_buf *b) {
    uint32_t bc = b->fmt.channels;
    if (s->n + b->len > s->cap) {
        uint32_t nc = s->cap ? s->cap : 4096;
        while (nc < s->n + b->len) nc *= 2;
        float *ns = realloc(s->stage, (size_t)nc * ic * sizeof(float));
        if (!ns) return 0;
        s->stage = ns;
        s->cap = nc;
    }
    for (uint32_t f = 0; f < b->len; f++)
        for (uint32_t c = 0; c < ic; c++) s->stage[(uint64_t)(s->n + f) * ic + c] = c < bc ? avc_get(b, c, f) : 0;
    s->n += b->len;
    return 1;
}

int avc_convert(avc_state *s, const avc_route *r, const avc_fmt *in, avc_buf *out, avc_pull_fn pull, void *ctx) {
    uint32_t ic = in->channels > 32 ? 32 : in->channels, och = out->fmt.channels;
    double ratio = in->rate / out->fmt.rate;
    uint32_t made = 0;
    int dry = 0;
    float fr[32];
    while (made < out->cap) {
        uint32_t i0 = (uint32_t)s->pos;
        if (i0 + 1 >= s->n && !s->eos && !dry) {
            avc_buf got;
            uint32_t want = (uint32_t)((out->cap - made) * ratio) + 2;
            memset(&got, 0, sizeof got);
            int st = pull(ctx, want, &got);
            if (st == 0 && got.len) {
                if (!stage_add(s, ic, &got)) return 3;
                continue;
            }
            if (st == 2) s->eos = 1;
            else dry = 1;
            continue;
        }
        if (i0 >= s->n) break;
        if (i0 + 1 >= s->n && !s->eos) break;           /* falta o vizinho da interpolação: espera mais entrada */
        if (s->eos && s->pos > (double)(s->n - 1) + 1e-6) break;   /* fim: nada de quadro além da última amostra */
        double frac = s->pos - i0;
        const float *a = s->stage + (uint64_t)i0 * ic;
        const float *z = i0 + 1 < s->n ? a + ic : a;
        for (uint32_t c = 0; c < ic; c++) fr[c] = a[c] + (float)((z[c] - a[c]) * frac);
        for (uint32_t oc = 0; oc < och; oc++) avc_put(out, oc, made, avc_mix(r, fr, ic, oc, och));
        made++;
        s->pos += ratio;
    }
    uint32_t used = (uint32_t)s->pos;
    if (used > s->n) used = s->n;
    if (used) {
        memmove(s->stage, s->stage + (uint64_t)used * ic, (size_t)(s->n - used) * ic * sizeof(float));
        s->n -= used;
        s->pos -= used;
    }
    out->len = made;
    if (made) return 0;
    return s->eos ? 2 : 1;
}

void avc_reset(avc_state *s) { s->n = 0; s->pos = 0; s->eos = 0; }
void avc_free(avc_state *s) { free(s->stage); s->stage = 0; s->n = s->cap = 0; s->pos = 0; }
