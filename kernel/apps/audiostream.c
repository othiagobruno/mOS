/* mOS — decodificação progressiva de MP3 para os apps do sistema (Música, QuickTime Player).
 *
 * Saída igual à do kit_load_wav: estéreo, 48 kHz, i16 intercalado (a mesma reamostragem pela amostra anterior).
 * O decodificador é o minimp3 (CC0; cópia em minimp3_kernel.h). A faixa é decodificada aos poucos
 * (kit_stream_step, com orçamento de tempo), para a mesa não parar enquanto uma música de vários minutos vira PCM:
 * quem toca lê kit_stream_pcm/kit_stream_frames, que crescem até kit_stream_done.
 *
 * Só depende de kmalloc/krealloc/kfree, memcpy e do relógio; tools/audiostream_host.c compila este arquivo no host
 * e confere contra o ffmpeg. */
#ifndef AS_HOST                    /* AS_HOST: tools/audiostream_host.c define tipos e kmalloc antes */
#include "kernel.h"
#endif
#include "audiostream.h"
#define MINIMP3_IMPLEMENTATION
#include "minimp3_kernel.h"

struct kit_stream {
    u8 *data;
    u64 len, pos;
    mp3dec_t dec;
    i16 *pcm;               /* saída: estéreo 48 kHz */
    u32 frames, cap;
    u64 src_frames;         /* amostras da fonte já decodificadas (antes do recorte) */
    u64 skip, valid;        /* gapless: descarta as `skip` primeiras e entrega `valid` (0 = até o fim) */
    u32 rate;               /* taxa da fonte (0 até o primeiro quadro) */
    bool done;
};

static u32 be32_(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

/* tamanho de uma etiqueta ID3v2 no começo (0 = não há) */
static u64 id3_skip(const u8 *d, u64 n) {
    if (n < 10 || d[0] != 'I' || d[1] != 'D' || d[2] != '3') return 0;
    u64 sz = (u64)(d[6] & 0x7F) << 21 | (u64)(d[7] & 0x7F) << 14 | (u64)(d[8] & 0x7F) << 7 | (d[9] & 0x7F);
    sz += 10 + ((d[5] & 0x10) ? 10 : 0);       /* rodapé */
    return sz <= n ? sz : n;
}

/* O que o começo do arquivo diz. Com cabeçalho Xing/Info o primeiro quadro não é áudio: o áudio começa em `audio`,
 * e o LAME (ou o "Lavc" do ffmpeg, mesmo formato) traz atraso do codificador e enchimento do fim. Como no ffmpeg,
 * descarta-se atraso + 529 (o atraso do decodificador) e o fim vale total - atraso - enchimento. */
typedef struct { u64 audio; u32 hz, bitrate; u64 skip, valid; bool ok; } mp3_head_t;
static mp3_head_t head_of(const u8 *d, u64 n) {
    mp3_head_t h = {0};
    u64 off = id3_skip(d, n);
    if (off + 4 > n) return h;
    mp3dec_t dec;
    mp3dec_frame_info_t info;
    mp3dec_init(&dec);
    mp3dec_decode_frame(&dec, d + off, (int)MIN(n - off, (u64)65536), NULL, &info);
    if (!info.frame_bytes || !info.hz || info.layer != 3) return h;
    h.ok = true;
    h.hz = (u32)info.hz;
    h.bitrate = (u32)info.bitrate_kbps;
    u64 end = off + (u64)info.frame_bytes;                     /* fim do primeiro quadro */
    u64 fo = off;                                              /* começo dele: o sincronismo antes do fim */
    while (fo + 4 < end && !(d[fo] == 0xFF && (d[fo + 1] & 0xE0) == 0xE0)) fo++;
    h.audio = off;
    bool mpeg1 = info.hz >= 32000;
    u32 spf = mpeg1 ? 1152 : 576;
    u32 side = mpeg1 ? (info.channels == 1 ? 17 : 32) : (info.channels == 1 ? 9 : 17);
    u64 x = fo + 4 + side;
    if (x + 8 > n || (memcmp(d + x, "Xing", 4) && memcmp(d + x, "Info", 4))) return h;
    h.audio = end;                                             /* o quadro Info é silêncio de metadado */
    u32 fl = be32_(d + x + 4);
    u64 q = x + 8, frames = 0;
    if (fl & 1) { if (q + 4 > n) return h; frames = be32_(d + q); q += 4; }
    if (fl & 2) q += 4;
    if (fl & 4) q += 100;
    if (fl & 8) q += 4;
    u64 delay = 0, pad = 0;
    if (q + 24 <= n && (!memcmp(d + q, "LAME", 4) || !memcmp(d + q, "Lavc", 4) || !memcmp(d + q, "Lavf", 4))) {
        const u8 *g = d + q + 21;
        delay = (u64)g[0] << 4 | g[1] >> 4;
        pad = (u64)(g[1] & 15) << 8 | g[2];
    }
    h.skip = delay + 529;
    if (frames) {
        u64 total = frames * spf;
        h.valid = total > delay + pad ? total - delay - pad : 0;
    }
    return h;
}

bool kit_mp3_probe(const u8 *d, u64 n) { return head_of(d, n).ok; }

/* duração em quadros de 48 kHz: pelo Xing/Info (VBR e CBR com o cabeçalho), senão pela taxa de bits */
u32 kit_mp3_estimate_frames(const u8 *d, u64 n) {
    mp3_head_t h = head_of(d, n);
    if (!h.ok) return 0;
    u64 src;
    if (h.valid) src = h.valid;
    else if (h.bitrate) src = (n - h.audio) * 8 * (u64)h.hz / ((u64)h.bitrate * 1000);
    else return 0;
    return (u32)(src * 48000 / h.hz);
}

kit_stream_t *kit_stream_open_mem(const u8 *data, u64 len) {
    if (!data || !len) return NULL;
    mp3_head_t h = head_of(data, len);
    if (!h.ok) return NULL;
    kit_stream_t *s = kcalloc(1, sizeof *s);
    if (!s) return NULL;
    s->data = kmalloc(len);
    u32 est = kit_mp3_estimate_frames(data, len);
    s->cap = est ? est + est / 16 + 4800 : 48000 * 60;
    s->pcm = kmalloc((usize)s->cap * 4);
    if (!s->data || !s->pcm) { kfree(s->data); kfree(s->pcm); kfree(s); return NULL; }
    memcpy(s->data, data, len);
    s->len = len;
    s->pos = h.audio;
    s->skip = h.skip;
    s->valid = h.valid;
    mp3dec_init(&s->dec);
    return s;
}

static bool grow(kit_stream_t *s, u32 need) {
    if (need <= s->cap) return true;
    u32 nc = MAX(need, s->cap + s->cap / 2);
    i16 *p = krealloc(s->pcm, (usize)nc * 4);
    if (!p) return false;
    s->pcm = p;
    s->cap = nc;
    return true;
}

/* um quadro MP3; false = acabou (ou erro) */
static bool decode_one(kit_stream_t *s) {
    static i16 tmp[MINIMP3_MAX_SAMPLES_PER_FRAME];
    for (;;) {
        if (s->pos >= s->len) return false;
        mp3dec_frame_info_t info;
        u64 win = MIN(s->len - s->pos, (u64)16384);
        int n = mp3dec_decode_frame(&s->dec, s->data + s->pos, (int)win, tmp, &info);
        if (!info.frame_bytes) return false;                  /* nada mais que pareça MP3 */
        s->pos += (u64)info.frame_bytes;
        if (n <= 0) continue;                                 /* lixo pulado ou quadro só de reservatório */
        if (!s->rate) s->rate = (u32)info.hz;
        int ch = info.channels;
        u64 ra = s->src_frames, rb = ra + (u64)n;              /* amostras cruas deste quadro */
        s->src_frames = rb;
        /* recorte gapless: índices efetivos e = cru - skip, em [0, valid) */
        u64 lo = MAX(ra, s->skip), hi = rb;
        if (s->valid) hi = MIN(hi, s->skip + s->valid);
        if (hi <= lo) { if (s->valid && ra >= s->skip + s->valid) return false; continue; }
        u64 a = lo - s->skip, b = hi - s->skip;
        /* saídas i com fonte j = i*rate/48000 em [a, b): mesma regra do kit_load_wav */
        u64 i0 = (a * 48000 + s->rate - 1) / s->rate;
        u64 i1 = (b * 48000 + s->rate - 1) / s->rate;
        if (i1 > i0 && !grow(s, (u32)i1)) return false;
        for (u64 i = i0; i < i1; i++) {
            u64 j = i * s->rate / 48000 + s->skip - ra;        /* posição dentro de tmp */
            s->pcm[i * 2] = tmp[j * ch];
            s->pcm[i * 2 + 1] = tmp[j * ch + (ch == 2 ? 1 : 0)];
        }
        if (i1 > s->frames) s->frames = (u32)i1;
        return true;
    }
}

bool kit_stream_step(kit_stream_t *s, u32 budget_us) {
    if (!s || s->done) return true;
    u64 t0 = hpet_ns(), m0 = timer_ticks();
    do {
        if (!decode_one(s)) { s->done = true; break; }
    } while (t0 ? (hpet_ns() - t0) < (u64)budget_us * 1000 : (timer_ticks() - m0) * 1000 < budget_us);
    return s->done;
}

i16 *kit_stream_pcm(kit_stream_t *s) { return s ? s->pcm : NULL; }
u32 kit_stream_frames(kit_stream_t *s) { return s ? s->frames : 0; }
bool kit_stream_done(kit_stream_t *s) { return !s || s->done; }
void kit_stream_close(kit_stream_t *s) { if (s) { kfree(s->data); kfree(s->pcm); kfree(s); } }
