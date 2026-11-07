/* mOS — subsistema de som: mixer por software, sintetizador e sons do sistema
 *
 * - até NVOICES vozes de síntese + uma fila de PCM (audio_play), somadas em
 *   inteiros de 32 bits, com volume mestre (curva perceptual em dB, rampa
 *   suave), limitador suave e saturação para 16 bits;
 * - motor de tabelas de onda em ponto fixo: o caminho por amostra só usa
 *   inteiros (barato até em emulação); envelopes, filtros e modulações são
 *   calculados em float a cada bloco de 32 amostras ("taxa de controle") e
 *   interpolados linearmente dentro do bloco;
 * - timbres: piano (camada fundamental + camada de harmônicos agudos que decai
 *   mais rápido, ruído de martelo, decaimento exponencial), órgão (registros
 *   de drawbar sustentados, tremolo), sintetizador (dois dentes-de-serra
 *   desafinados + quadrada uma oitava abaixo, tabelas limitadas em banda por
 *   oitava, passa-baixas de 2 polos com envelope de corte);
 * - envelope ADSR por voz;
 * - o backend (HDA ou AC'97) chama snd_render() quando precisa de amostras;
 * - a captura (microfone) chega por snd_capture() e fica num anel de ~0,7 s.
 *   audio_capture_read() consome; com o monitor ligado, o mesmo áudio entra
 *   na saída, com a latência do buffer de playback.
 */
#include "kernel.h"

#define RATE     48000
#define NVOICES  16
#define BLOCK    32
#define TBITS    11
#define TLEN     (1 << TBITS)
#define TMASK    (TLEN - 1)
#define NLEVELS  8          /* tabelas limitadas em banda: uma por oitava */

/* backends */
bool hda_probe(char *name, usize n);
void hda_service(void);
void hda_set_mute(bool mute);
bool hda_input_available(void);
const char *hda_input_name(void);
bool ac97_probe(char *name, usize n);
void ac97_service(void);
void ac97_set_mute(bool mute);
bool ac97_input_available(void);
const char *ac97_input_name(void);

enum { BK_NONE, BK_HDA, BK_AC97 };
static int backend;
static char devname[80] = "Nenhum dispositivo";
static int volume = 70;
static i32 gm_target, gm_cur;   /* ganho mestre Q16 (65536 = 0 dB) */

/* timbres (0..2 são os públicos de synth_note) */
enum { W_PIANO, W_ORGAN, W_SYNTH, W_PAD, W_CLICK, W_POP };
enum { ENV_OFF, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };

typedef struct {
    u8    on, wave, stage, late;
    u32   delay, age, hold;          /* em amostras */
    const i16 *ta, *tb;
    u32   ph1, ph2, ph3, inc1, inc2, inc3, base_inc;
    /* controle (por bloco) */
    float env, a_step, d_mul, sus, r_mul;
    float amp, pl, pr;
    float lay, lay_mul;              /* camada aguda do piano */
    float noise, noise_mul;          /* martelo */
    float glide, glide_mul;          /* queda de altura do "pop" */
    float cut, cut_min, cut_mul;     /* filtro do sintetizador */
    float lfo_depth;
    u32   lfo_ph, lfo_inc;
    /* estado em ponto fixo */
    i32   gl, gr, glay, gnoise, fa, lp1, lp2, nlp;
    u32   nrng;
} voice_t;

static voice_t V[NVOICES];

static float fsine[TLEN];
static i16 t_sine[TLEN + 1], t_pf[TLEN + 1], t_pb[TLEN + 1], t_org[TLEN + 1], t_pad[TLEN + 1], t_pop[TLEN + 1];
static i16 t_saw[NLEVELS][TLEN + 1], t_sqr[NLEVELS][TLEN + 1];
static bool synth_tables;

extern void (*audio_tap)(const i16 *stereo, u32 frames);

/* fila de PCM (audio_play): toca em sequência, mixada com as vozes */
#define PCMQ 32
typedef struct { i16 *data; u32 frames, pos; } pcmbuf_t;
static pcmbuf_t pq[PCMQ];
static int pq_head, pq_tail;
static u32 pq_frames;

/* ---------------------------------------------------------------- tabelas */
static inline i32 lk(const i16 *t, u32 ph) {
    u32 i = ph >> (32 - TBITS);
    i32 a = t[i], b = t[i + 1];
    return a + (((b - a) * (i32)((ph >> (32 - TBITS - 14)) & 0x3FFF)) >> 14);
}

/* soma de harmônicos inteiros -> tabela i16; devolve o pico (para normalizar camadas juntas) */
static float table_sum(float *acc, const int *h, const float *a, int n) {
    float peak = 0;
    for (int i = 0; i < TLEN; i++) {
        float x = 0;
        for (int k = 0; k < n; k++) x += a[k] * fsine[(h[k] * i) & TMASK];
        acc[i] = x;
        if (x > peak) peak = x;
        if (-x > peak) peak = -x;
    }
    return peak;
}
static void table_store(i16 *t, const float *acc, float scale) {
    for (int i = 0; i < TLEN; i++) t[i] = (i16)CLAMP((i32)(acc[i] * scale), -32767, 32767);
    t[TLEN] = t[0];
}
static void make_table(i16 *t, const int *h, const float *a, int n, float *tmp) {
    float pk = table_sum(tmp, h, a, n);
    table_store(t, tmp, pk > 0 ? 30000.0f / pk : 0);
}

static void build_tables(void) {
    static float tmp[TLEN], tmp2[TLEN];
    for (int i = 0; i < TLEN; i++) fsine[i] = fsin(2.0f * PI_F * i / TLEN);
    static const int h1[1] = {1};
    static const float a1[1] = {1};
    make_table(t_sine, h1, a1, 1, tmp);
    /* piano: camada fundamental e camada de harmônicos 2..8, normalizadas juntas */
    static const int hf[2] = {1, 2};
    static const float af[2] = {1.0f, 0.28f};
    static const int hb[7] = {2, 3, 4, 5, 6, 7, 8};
    static const float ab[7] = {0.34f, 0.32f, 0.2f, 0.13f, 0.08f, 0.05f, 0.03f};
    float p1 = table_sum(tmp, hf, af, 2), p2 = table_sum(tmp2, hb, ab, 7);
    float sc = 30000.0f / (p1 + p2);
    table_store(t_pf, tmp, sc);
    table_store(t_pb, tmp2, sc);
    /* órgão: tocado uma oitava abaixo; harmônicos = 16' 8' 5 1/3' 4' 2 2/3' 2' 1 3/5' 1' */
    static const int ho[8] = {1, 2, 3, 4, 6, 8, 10, 16};
    static const float ao[8] = {0.55f, 1.0f, 0.5f, 0.55f, 0.3f, 0.35f, 0.12f, 0.2f};
    make_table(t_org, ho, ao, 8, tmp);
    static const int hp[3] = {1, 2, 3};
    static const float ap[3] = {1.0f, 0.2f, 0.06f};
    make_table(t_pad, hp, ap, 3, tmp);
    static const float apo[3] = {1.0f, 0.22f, 0.06f};
    make_table(t_pop, hp, apo, 3, tmp);
}

/* dente-de-serra e quadrada limitados em banda, uma tabela por oitava (gerados no 1º uso) */
static void build_synth_tables(void) {
    static float tmp[TLEN];
    for (int lv = 0; lv < NLEVELS; lv++) {
        float top = 27.5f * (float)(1 << lv) * 2.0f;   /* maior frequência tocada com esta tabela */
        int nh = (int)(18000.0f / top);
        nh = CLAMP(nh, 1, 48);
        for (int sq = 0; sq < 2; sq++) {
            float peak = 0;
            for (int i = 0; i < TLEN; i++) {
                float x = 0;
                for (int h = 1; h <= nh; h++) {
                    if (sq && !(h & 1)) continue;
                    x += fsine[(h * i) & TMASK] / (float)h;
                }
                tmp[i] = x;
                if (x > peak) peak = x;
                if (-x > peak) peak = -x;
            }
            table_store(sq ? t_sqr[lv] : t_saw[lv], tmp, 30000.0f / peak);
        }
    }
    synth_tables = true;
}

/* ---------------------------------------------------------------- utilidades */
static inline u32 hz_inc(float hz) {
    if (hz <= 0 || hz >= RATE / 2) return 0;
    return (u32)(hz * (4294967296.0f / RATE));
}
/* multiplicador por bloco para cair a 1/e em tau segundos */
static inline float blk_mul(float tau) { return tau <= 0 ? 0 : fexp(-(float)BLOCK / (tau * RATE)); }
static float vol_curve(int pct) {
    if (pct <= 0) return 0;
    float x = pct / 100.0f;
    return fpow(10.0f, -36.0f * (1.0f - x) / 20.0f); /* 100% = 0 dB, 50% = -18 dB, 1% ~ -36 dB */
}
static int level_for(float f) {
    int lv = 0;
    float b = 27.5f * 2.0f;
    while (lv < NLEVELS - 1 && f >= b) { b *= 2.0f; lv++; }
    return lv;
}

/* ---------------------------------------------------------------- vozes */
static voice_t *voice_alloc(void) {
    voice_t *best = NULL;
    float best_score = 1e30f;
    for (int i = 0; i < NVOICES; i++) {
        voice_t *v = &V[i];
        if (!v->on) { best = v; break; }
        /* rouba a mais silenciosa; vozes soltas valem menos */
        float s = v->env * v->amp * (v->stage == ENV_RELEASE ? 0.25f : 1.0f) + (v->delay ? 10.0f : 0);
        if (s < best_score) { best_score = s; best = v; }
    }
    memset(best, 0, sizeof *best);
    return best;
}

static void set_adsr(voice_t *v, float a, float d_tau, float s, float r_tau) {
    v->a_step = a > 0 ? (float)BLOCK / (a * RATE) : 1.0f;
    v->d_mul = blk_mul(d_tau);
    v->sus = s;
    v->r_mul = blk_mul(r_tau);
    v->stage = ENV_ATTACK;
    v->env = 0;
}

static void set_pan(voice_t *v, float p) { /* -1 esquerda .. 1 direita, lei de potência */
    p = CLAMP(p, -1.0f, 1.0f);
    v->pl = fsqrt(0.5f * (1.0f - p));
    v->pr = fsqrt(0.5f * (1.0f + p));
}

static void voice_start(int wave, float f, int ms, float vol, int delay_ms, float pan) {
    if ((!backend && !audio_tap) || f <= 20.0f || f > 16000.0f || vol <= 0) return;
    if (wave == W_SYNTH && !synth_tables) build_synth_tables();
    u64 fl = irq_save();
    voice_t *v = voice_alloc();
    v->wave = (u8)wave;
    v->delay = (u32)(delay_ms > 0 ? delay_ms : 0) * (RATE / 1000);
    v->hold = (u32)(ms > 0 ? ms : 1) * (RATE / 1000);
    v->amp = vol;
    v->nrng = rand32() | 1;
    v->ph1 = rand32(); v->ph2 = rand32(); v->ph3 = rand32();
    set_pan(v, pan);
    switch (wave) {
    case W_PIANO: {
        v->ta = t_pf; v->tb = t_pb;
        v->inc1 = hz_inc(f);
        float tau = 1.4f * fpow(262.0f / f, 0.45f);       /* graves soam por mais tempo */
        tau = CLAMP(tau, 0.35f, 3.5f);
        set_adsr(v, 0.002f, tau, 0.0f, 0.12f);
        v->lay = 1.0f;
        v->lay_mul = blk_mul(tau * 0.22f);               /* brilho some antes do corpo */
        v->noise = 0.35f;
        v->noise_mul = blk_mul(0.004f);
        v->amp *= 1.2f;
        break;
    }
    case W_ORGAN:
        v->ta = t_org;
        v->inc1 = hz_inc(f * 0.5f);
        set_adsr(v, 0.008f, 0.3f, 0.9f, 0.06f);
        v->lfo_inc = hz_inc(6.2f) * BLOCK;
        v->lfo_depth = 0.08f;
        v->amp *= 0.9f;
        break;
    case W_SYNTH: {
        int lv = level_for(f), lv2 = level_for(f * 0.5f);
        v->ta = t_saw[lv]; v->tb = t_sqr[lv2];
        v->inc1 = hz_inc(f * 1.0035f);
        v->inc2 = hz_inc(f * 0.9965f);
        v->inc3 = hz_inc(f * 0.5f);
        v->cut = MIN(f * 9.0f, 14000.0f);
        v->cut_min = MIN(f * 2.2f, 9000.0f);
        v->cut_mul = blk_mul(0.28f);
        set_adsr(v, 0.006f, 0.35f, 0.65f, 0.15f);
        v->amp *= 1.0f;
        break;
    }
    case W_PAD:
        v->ta = t_pad;
        v->inc1 = hz_inc(f);
        v->inc2 = hz_inc(f * 1.0028f);                   /* desafinação leve: coro lento */
        set_adsr(v, 0.32f, 1.5f, 0.75f, 0.30f);
        break;
    case W_CLICK:
        v->ta = t_sine;
        v->inc1 = hz_inc(f);
        set_adsr(v, 0.0003f, 0.0025f, 0.0f, 0.003f);
        break;
    case W_POP:
        v->ta = t_pop;
        v->base_inc = v->inc1 = hz_inc(f);
        set_adsr(v, 0.0015f, 0.16f, 0.0f, 0.05f);
        v->glide = 0.22f;                                 /* começa ~22% acima e cai rápido */
        v->glide_mul = blk_mul(0.012f);
        break;
    }
    v->on = 1;
    v->late = audio_tap ? 0 : 1;   /* com derivação (fone Bluetooth) tudo passa pelo snd_render */
    irq_restore(fl);
}

/* avança o envelope um bloco; devolve false quando a voz terminou */
static bool env_block(voice_t *v) {
    switch (v->stage) {
    case ENV_ATTACK:
        v->env += v->a_step;
        if (v->env >= 1.0f) { v->env = 1.0f; v->stage = ENV_DECAY; }
        break;
    case ENV_DECAY:
        v->env = v->sus + (v->env - v->sus) * v->d_mul;
        if (v->env - v->sus < 0.0005f) {
            v->env = v->sus;
            v->stage = ENV_SUSTAIN;
            if (v->sus <= 0) return false;
        }
        break;
    case ENV_SUSTAIN:
        break;
    case ENV_RELEASE:
        v->env *= v->r_mul;
        if (v->env < 0.0008f) { v->env = 0; return false; }
        break;
    }
    return true;
}

static void voice_block(voice_t *v, i32 *acc, int n) {
    int i0 = 0;
    if (v->delay) {
        if (v->delay >= (u32)n) { v->delay -= n; return; }
        i0 = (int)v->delay;
        v->delay = 0;
    }
    int cnt = n - i0;
    if (v->stage != ENV_RELEASE && v->age >= v->hold) v->stage = ENV_RELEASE;
    bool alive = env_block(v);
    float e = alive ? v->env : 0.0f;
    if (v->lfo_depth > 0) {
        v->lfo_ph += v->lfo_inc;
        e *= 1.0f - v->lfo_depth * (0.5f + 0.5f * (float)lk(t_sine, v->lfo_ph) * (1.0f / 32767.0f));
    }
    float g = e * v->amp * 32767.0f;
    i32 tl = (i32)(g * v->pl), tr = (i32)(g * v->pr);
    i32 gl = v->gl, gr = v->gr, dl = (tl - gl) / cnt, dr = (tr - gr) / cnt;
    i32 *o = acc + i0 * 2;
    u32 p1 = v->ph1, p2 = v->ph2, p3 = v->ph3;
    const u32 i1 = v->inc1, i2 = v->inc2, i3 = v->inc3;
    const i16 *ta = v->ta, *tb = v->tb;

    switch (v->wave) {
    case W_PIANO: {
        v->lay *= v->lay_mul;
        v->noise *= v->noise_mul;
        i32 tlay = (i32)(v->lay * 32767.0f), tno = (i32)(v->noise * 32767.0f);
        i32 glay = v->glay, dlay = (tlay - glay) / cnt, gno = v->gnoise, dno = (tno - gno) / cnt;
        i32 nlp = v->nlp;
        u32 r = v->nrng;
        for (int i = 0; i < cnt; i++) {
            i32 s = lk(ta, p1) + ((lk(tb, p1) * glay) >> 15);
            p1 += i1;
            if (gno > 0) {
                r = r * 1664525u + 1013904223u;
                nlp += (((i32)(r >> 16) - 32768) - nlp) >> 2;   /* ruído com passa-baixas */
                s += (nlp * gno) >> 16;
            }
            o[i * 2] += (s * gl) >> 15;
            o[i * 2 + 1] += (s * gr) >> 15;
            gl += dl; gr += dr; glay += dlay; gno += dno;
        }
        v->glay = tlay; v->gnoise = tno; v->nlp = nlp; v->nrng = r;
        break;
    }
    case W_SYNTH: {
        float c = v->cut_min + (v->cut - v->cut_min) * v->cut_mul;
        v->cut = c;
        i32 fa = (i32)((1.0f - fexp(-2.0f * PI_F * c / RATE)) * 16384.0f);
        i32 lp1 = v->lp1, lp2 = v->lp2;
        for (int i = 0; i < cnt; i++) {
            i32 x = ((lk(ta, p1) + lk(ta, p2)) * 12452 + lk(tb, p3) * 9830) >> 15;
            p1 += i1; p2 += i2; p3 += i3;
            lp1 += ((x - lp1) * fa) >> 14;
            lp2 += ((lp1 - lp2) * fa) >> 14;
            o[i * 2] += (lp2 * gl) >> 15;
            o[i * 2 + 1] += (lp2 * gr) >> 15;
            gl += dl; gr += dr;
        }
        v->lp1 = lp1; v->lp2 = lp2;
        break;
    }
    case W_PAD:
        for (int i = 0; i < cnt; i++) {
            i32 s = (lk(ta, p1) + lk(ta, p2)) >> 1;
            p1 += i1; p2 += i2;
            o[i * 2] += (s * gl) >> 15;
            o[i * 2 + 1] += (s * gr) >> 15;
            gl += dl; gr += dr;
        }
        break;
    case W_POP:
        if (v->glide > 0.0001f) {
            v->glide *= v->glide_mul;
            v->inc1 = (u32)((float)v->base_inc * (1.0f + v->glide));
        } else v->inc1 = v->base_inc;
        /* fall through: oscilador simples */
    default:
        for (int i = 0; i < cnt; i++) {
            i32 s = lk(ta, p1);
            p1 += v->inc1;
            o[i * 2] += (s * gl) >> 15;
            o[i * 2 + 1] += (s * gr) >> 15;
            gl += dl; gr += dr;
        }
        break;
    }
    v->ph1 = p1; v->ph2 = p2; v->ph3 = p3;
    v->gl = tl; v->gr = tr;
    v->age += cnt;
    if (!alive) v->on = 0;
}

/* ---------------------------------------------------------------- captura (48 kHz estéreo) */
#define CAP_N    32768
#define CAP_MASK (CAP_N - 1)
static i16 *cap_ring;
static u32 cap_w, cap_r, cap_m;
static int cap_vol = 100;
static bool cap_on, cap_monitor;
static char inname[80] = "Nenhuma entrada";
static spinlock_t cap_lock;

static i16 cap_gain(i16 s, i32 g) {
    i32 v = (i32)(((i64)s * g) >> 16);
    return (i16)CLAMP(v, -32768, 32767);
}

void snd_capture(const i16 *src, u32 frames) {
    if (!cap_on || !cap_ring || !src || !frames) return;
    i32 g = cap_vol <= 0 ? 0 : (i32)cap_vol * 65536 / 100;
    while (frames) {
        u32 n = MIN(frames, 128);
        i16 tmp[128 * 2];
        for (u32 i = 0; i < n; i++) {
            tmp[i * 2] = cap_gain(src[i * 2], g);
            tmp[i * 2 + 1] = cap_gain(src[i * 2 + 1], g);
        }
        u64 fl = spin_lock_irqsave(&cap_lock);
        for (u32 i = 0; i < n; i++) {
            cap_ring[cap_w * 2] = tmp[i * 2];
            cap_ring[cap_w * 2 + 1] = tmp[i * 2 + 1];
            cap_w = (cap_w + 1) & CAP_MASK;
            if (cap_w == cap_r) cap_r = (cap_r + 1) & CAP_MASK;
        }
        spin_unlock_irqrestore(&cap_lock, fl);
        src += n * 2;
        frames -= n;
    }
}

/* ---------------------------------------------------------------- mixer */
static inline i32 soft_clip(i32 x) {
    /* limitador suave acima de 0,75 FS, nunca passa de 1,0 FS */
    i32 a = x < 0 ? -x : x;
    if (a > 24576) {
        i32 e = a - 24576;
        a = 24576 + (i32)(((i64)e * 8191) / (e + 8192));
        x = x < 0 ? -a : a;
    }
    return CLAMP(x, -32767, 32767);
}

static i32 acc[BLOCK * 2];

static void mix_block(i16 *dst, int n, bool late) {
    memset(acc, 0, sizeof(i32) * 2 * n);
    for (int k = 0; k < NVOICES; k++)
        if (V[k].on && V[k].late == late) voice_block(&V[k], acc, n);
    i32 g0 = gm_cur, g1 = gm_cur;
    if (!late) {
        for (int i = 0; i < n && pq_head != pq_tail; i++) {
            pcmbuf_t *b = &pq[pq_head];
            acc[i * 2] += b->data[b->pos * 2];
            acc[i * 2 + 1] += b->data[b->pos * 2 + 1];
            if (++b->pos >= b->frames) {
                pq_frames -= b->frames;
                kfree(b->data);
                b->data = NULL;
                pq_head = (pq_head + 1) % PCMQ;
            }
        }
        if (cap_monitor && cap_on && cap_ring) {
            i16 mon[BLOCK * 2];
            int got = 0;
            u64 fl = spin_lock_irqsave(&cap_lock);
            u32 avail = (cap_w - cap_m) & CAP_MASK;
            if (avail > CAP_N / 2) cap_m = (cap_w - 960) & CAP_MASK; /* ~20 ms, sem despejar o anel inteiro */
            while (got < n && cap_m != cap_w) {
                mon[got * 2] = cap_ring[cap_m * 2];
                mon[got * 2 + 1] = cap_ring[cap_m * 2 + 1];
                cap_m = (cap_m + 1) & CAP_MASK;
                got++;
            }
            spin_unlock_irqrestore(&cap_lock, fl);
            for (int i = 0; i < got; i++) {
                acc[i * 2] += mon[i * 2];
                acc[i * 2 + 1] += mon[i * 2 + 1];
            }
        }
        g1 = gm_cur + (gm_target - gm_cur) / 8;          /* rampa ~5 ms: sem estalos ao mudar o volume */
        if (g1 == gm_cur) g1 = gm_target;
        gm_cur = g1;
    }
    i32 dg = (g1 - g0) / n, g = g0;
    for (int i = 0; i < n; i++) {
        i32 l = (i32)(((i64)acc[i * 2] * g) >> 16);
        i32 r = (i32)(((i64)acc[i * 2 + 1] * g) >> 16);
        g += dg;
        if (late) {
            l += dst[i * 2]; r += dst[i * 2 + 1];
            dst[i * 2] = (i16)CLAMP(l, -32767, 32767);
            dst[i * 2 + 1] = (i16)CLAMP(r, -32767, 32767);
        } else {
            dst[i * 2] = (i16)soft_clip(l);
            dst[i * 2 + 1] = (i16)soft_clip(r);
        }
    }
}

/* derivação do mixer (kernel/bt/: fones A2DP). Recebe o mesmo PCM estéreo de 48 kHz da saída interna. */
void (*audio_tap)(const i16 *stereo, u32 frames);
bool audio_route_bluetooth;   /* true: a saída interna fica em silêncio enquanto o fone toca */

/* renderiza 'frames' quadros novos (sobrescreve dst; estéreo intercalado) */
void snd_render(i16 *dst, u32 frames) {
    while (frames) {
        int n = frames > BLOCK ? BLOCK : (int)frames;
        mix_block(dst, n, false);
        if (audio_tap) {
            audio_tap(dst, (u32)n);
            if (audio_route_bluetooth) memset(dst, 0, sizeof(i16) * 2 * n);
        }
        dst += n * 2;
        frames -= n;
    }
}

/* Vozes "atrasadas": notas disparadas depois que o backend já escreveu
 * centenas de ms à frente. Em vez de esperar esse buffer tocar, o backend
 * chama snd_render_late() sobre a parte ainda não tocada (a partir de ~10 ms
 * à frente do DMA), somando só essas vozes com saturação. Assim temos buffer
 * grande (robusto a travadas do laço principal) e latência baixa. */
bool snd_late_pending(void) {
    for (int k = 0; k < NVOICES; k++) if (V[k].on && V[k].late) return true;
    return false;
}
void snd_render_late(i16 *dst, u32 frames) {
    while (frames) {
        int n = frames > BLOCK ? BLOCK : (int)frames;
        mix_block(dst, n, true);
        dst += n * 2;
        frames -= n;
    }
}
void snd_late_done(void) {
    for (int k = 0; k < NVOICES; k++) V[k].late = 0;
}

/* ---------------------------------------------------------------- API pública */
bool audio_init(void) {
    gm_target = gm_cur = (i32)(vol_curve(volume) * 65536.0f);
    cap_ring = kmalloc(CAP_N * 4);
    if (cap_ring) memset(cap_ring, 0, CAP_N * 4);
    if (hda_probe(devname, sizeof devname)) backend = BK_HDA;
    else if (ac97_probe(devname, sizeof devname)) backend = BK_AC97;
    build_tables();   /* também sem placa: o fone Bluetooth (audio_tap) usa o mesmo mixer */
    cap_on = false;
    strcpy(inname, "Nenhuma entrada");
    if (cap_ring && backend == BK_HDA && hda_input_available()) {
        cap_on = true;
        snprintf(inname, sizeof inname, "%s", hda_input_name());
    } else if (cap_ring && backend == BK_AC97 && ac97_input_available()) {
        cap_on = true;
        snprintf(inname, sizeof inname, "%s", ac97_input_name());
    }
    if (!backend) {
        strcpy(devname, "Nenhum dispositivo");
        kprintf("Áudio: nenhum dispositivo de saída encontrado\n");
        return false;
    }
    kprintf("Áudio: %s, volume %d%%\n", devname, volume);
    if (cap_on) kprintf("Áudio: entrada %s, ganho %d%%\n", inname, cap_vol);
    else kprintf("Áudio: nenhuma entrada\n");
    return true;
}

bool audio_available(void) { return backend != BK_NONE || audio_tap; }
const char *audio_device_name(void) { return devname; }

bool audio_input_available(void) { return cap_on; }
const char *audio_input_name(void) { return inname; }
void audio_set_input_volume(int pct) { cap_vol = CLAMP(pct, 0, 100); }
int audio_get_input_volume(void) { return cap_vol; }
void audio_set_input_monitor(bool on) {
    u64 fl = spin_lock_irqsave(&cap_lock);
    cap_monitor = on && cap_on;
    if (cap_monitor) cap_m = cap_w;                      /* não reproduz o que já estava no anel */
    spin_unlock_irqrestore(&cap_lock, fl);
}
bool audio_get_input_monitor(void) { return cap_monitor; }

u32 audio_capture_read(i16 *dst, u32 frames) {
    if (!cap_on || !cap_ring || !dst || !frames) return 0;
    u64 fl = spin_lock_irqsave(&cap_lock);
    u32 n = 0;
    while (n < frames && cap_r != cap_w) {
        dst[n * 2] = cap_ring[cap_r * 2];
        dst[n * 2 + 1] = cap_ring[cap_r * 2 + 1];
        cap_r = (cap_r + 1) & CAP_MASK;
        n++;
    }
    spin_unlock_irqrestore(&cap_lock, fl);
    return n;
}

void audio_poll(void) {
    if (backend == BK_HDA) hda_service();
    else if (backend == BK_AC97) ac97_service();
    else if (audio_tap) {
        /* sem placa de som: o relógio do mixer passa a ser o timer, só para alimentar o fone */
        static u64 last;
        static i16 buf[BLOCK * 2];
        u64 now = timer_ticks();
        if (!last || now - last > 200) last = now;
        u32 due = (u32)(now - last) * (RATE / 1000);
        last = now;
        while (due) { u32 n = MIN(due, (u32)BLOCK); snd_render(buf, n); due -= n; }
    }
}

void audio_set_volume(int pct) {
    pct = CLAMP(pct, 0, 100);
    bool was_muted = volume == 0;
    volume = pct;
    gm_target = (i32)(vol_curve(pct) * 65536.0f);
    if ((pct == 0) != was_muted) {
        if (backend == BK_HDA) hda_set_mute(pct == 0);
        else if (backend == BK_AC97) ac97_set_mute(pct == 0);
    }
}
int audio_get_volume(void) { return volume; }

void audio_play(const i16 *samples, u32 frames) {
    if ((!backend && !audio_tap) || !samples || !frames) return;
    if (pq_frames + frames > RATE * 20) return;          /* no máximo ~20 s na fila */
    int next = (pq_tail + 1) % PCMQ;
    if (next == pq_head) return;
    i16 *copy = kmalloc((usize)frames * 4);
    if (!copy) return;
    memcpy(copy, samples, (usize)frames * 4);
    u64 fl = irq_save();
    pq[pq_tail].data = copy;
    pq[pq_tail].frames = frames;
    pq[pq_tail].pos = 0;
    pq_tail = next;
    pq_frames += frames;
    irq_restore(fl);
}

void synth_note(float freq, int ms, int wave, float vol) {
    if (wave < 0 || wave > 2) wave = 0;
    /* pan suave pela altura: graves à esquerda, agudos à direita */
    float pan = 0.35f * (fpow(freq / 440.0f, 0.5f) - 1.0f);
    voice_start(wave, freq, ms, CLAMP(vol, 0.0f, 1.0f), 0, pan);
}

/* acorde de abertura original: Fá maior com nona (Fá2, Dó4, Fá4, Sol4, Lá4, Dó5),
 * entradas escalonadas e ataque lento (pad suave), ~1,6 s no total */
void sound_startup_chime(void) {
    static const struct { float f; int at; float vol, pan; } n[] = {
        {87.31f, 0, 0.62f, 0.0f},      /* Fá2 */
        {261.63f, 30, 0.44f, -0.35f},  /* Dó4 */
        {349.23f, 70, 0.40f, 0.25f},   /* Fá4 */
        {392.00f, 120, 0.31f, -0.15f}, /* Sol4 (nona) */
        {440.00f, 170, 0.34f, 0.4f},   /* Lá4 */
        {523.25f, 230, 0.22f, -0.4f},  /* Dó5 */
    };
    for (int i = 0; i < (int)ARRAY_LEN(n); i++)
        voice_start(W_PAD, n[i].f, 1150 - n[i].at, n[i].vol, n[i].at, n[i].pan);
}

/* tique curtíssimo e baixo */
void sound_click(void) {
    voice_start(W_CLICK, 2300.0f, 6, 0.08f, 0, 0.0f);
}

/* "pop" de duas notas: Sol5 seguido de Dó6 */
void sound_alert(void) {
    voice_start(W_POP, 783.99f, 180, 0.32f, 0, -0.1f);
    voice_start(W_POP, 1046.50f, 260, 0.30f, 90, 0.1f);
}
