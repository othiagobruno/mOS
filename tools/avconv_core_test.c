/* Núcleo do AVAudioConverter (userland/libsystem/avconv_core.c), que o Claude desktop importa.
 *   zig cc -std=c11 -Wall -Wextra -Werror -Iuserland/libsystem tools/avconv_core_test.c \
 *     userland/libsystem/avconv_core.c -lm -o /tmp/avconv_core_test && /tmp/avconv_core_test
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avconv_core.h"

static int fails;
#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL %s\n", m); fails++; } else printf("ok %s\n", m); } while (0)

/* buffer não intercalado de float32 ou int16 */
typedef struct { avc_buf b; void *pl[8]; } tbuf;
static void tb_init(tbuf *t, uint32_t common, uint32_t ch, int inter, double rate, uint32_t cap) {
    size_t bps = common == AVC_I16 ? 2 : common == AVC_F64 ? 8 : 4;
    memset(t, 0, sizeof *t);
    t->b.fmt.common = common;
    t->b.fmt.channels = ch;
    t->b.fmt.interleaved = inter;
    t->b.fmt.rate = rate;
    t->b.cap = cap;
    t->b.planes = t->pl;
    if (inter) {
        char *d = calloc((size_t)cap * ch, bps);
        for (uint32_t c = 0; c < ch; c++) t->pl[c] = d + c * bps;
    } else {
        for (uint32_t c = 0; c < ch; c++) t->pl[c] = calloc(cap, bps);
    }
}

/* fonte em blocos: seno de 440 Hz a 48 kHz, mono float, entregue em pedaços de 'chunk' quadros */
typedef struct { tbuf src; uint32_t done, total, chunk, calls; double rate; } feeder;
static int feed(void *ctx, uint32_t want, avc_buf *got) {
    feeder *f = (feeder *)ctx;
    uint32_t n = want < f->chunk ? want : f->chunk;
    f->calls++;
    if (f->done >= f->total) return 2;
    if (n > f->total - f->done) n = f->total - f->done;
    for (uint32_t i = 0; i < n; i++) ((float *)f->src.pl[0])[i] = (float)sin(2 * M_PI * 440.0 * (f->done + i) / f->rate) * 0.5f;
    f->src.b.len = n;
    f->done += n;
    *got = f->src.b;
    return 0;
}

static int nodata(void *ctx, uint32_t want, avc_buf *got) { (void)want; (void)got; (*(int *)ctx)++; return 1; }

int main(void) {
    avc_route none = {0, -1};

    /* int16 intercalado estéreo -> float32 não intercalado estéreo, mesma taxa */
    {
        tbuf in, out;
        tb_init(&in, AVC_I16, 2, 1, 44100, 4);
        tb_init(&out, AVC_F32, 2, 0, 44100, 8);
        int16_t *d = (int16_t *)in.pl[0];
        d[0] = 16384; d[1] = -16384; d[2] = 32767; d[3] = -32768; d[4] = 0; d[5] = 8192; d[6] = 1; d[7] = -1;
        in.b.len = 4;
        CHECK(avc_direct(&none, &in.b, &out.b) == 0 && out.b.len == 4, "direto: 4 quadros");
        float *l = (float *)out.pl[0], *r = (float *)out.pl[1];
        CHECK(fabsf(l[0] - 0.5f) < 1e-4f && fabsf(r[0] + 0.5f) < 1e-4f, "int16 -> float: ±0,5");
        CHECK(fabsf(r[1] + 1.0f) < 1e-4f && fabsf(r[2] - 0.25f) < 1e-4f, "int16 -> float: -1 e 0,25");
    }
    /* estéreo -> mono é a média; mono -> estéreo duplica; channelMap troca L/R */
    {
        tbuf in, out;
        tb_init(&in, AVC_F32, 2, 0, 48000, 2);
        tb_init(&out, AVC_F32, 1, 0, 48000, 2);
        ((float *)in.pl[0])[0] = 0.8f; ((float *)in.pl[1])[0] = 0.2f;
        in.b.len = 1;
        avc_direct(&none, &in.b, &out.b);
        CHECK(fabsf(((float *)out.pl[0])[0] - 0.5f) < 1e-6f, "estéreo -> mono: média");
        tbuf mono, st;
        tb_init(&mono, AVC_F32, 1, 0, 48000, 2);
        tb_init(&st, AVC_I16, 2, 1, 48000, 2);
        ((float *)mono.pl[0])[0] = 0.5f;
        mono.b.len = 1;
        avc_direct(&none, &mono.b, &st.b);
        CHECK(((int16_t *)st.pl[0])[0] == 16384 && ((int16_t *)st.pl[1])[0] == 16384, "mono -> estéreo int16: duplica");
        long swap[2] = {1, 0};
        avc_route r = {swap, 2};
        tbuf o2;
        tb_init(&o2, AVC_F32, 2, 0, 48000, 2);
        avc_direct(&r, &in.b, &o2.b);
        CHECK(fabsf(((float *)o2.pl[0])[0] - 0.2f) < 1e-6f && fabsf(((float *)o2.pl[1])[0] - 0.8f) < 1e-6f, "channelMap {1,0}: troca L/R");
    }
    /* taxa diferente no modo direto é recusada */
    {
        tbuf in, out;
        tb_init(&in, AVC_F32, 1, 0, 48000, 4);
        tb_init(&out, AVC_F32, 1, 0, 16000, 4);
        in.b.len = 4;
        CHECK(avc_direct(&none, &in.b, &out.b) == -1, "direto recusa troca de taxa");
    }
    /* 48 kHz -> 16 kHz em blocos picados: quantidade de quadros e o seno preservado */
    {
        feeder f;
        avc_state s;
        avc_fmt in = {AVC_F32, 1, 0, 48000};
        tbuf out;
        memset(&s, 0, sizeof s);
        memset(&f, 0, sizeof f);
        tb_init(&f.src, AVC_F32, 1, 0, 48000, 333);
        f.total = 48000;
        f.rate = 48000;
        f.chunk = 333;                  /* não divide 3: força o estado entre chamadas */
        tb_init(&out, AVC_F32, 1, 0, 16000, 1000);
        uint32_t total = 0;
        double err = 0;
        int st = 0, rounds = 0;
        while ((st = avc_convert(&s, &none, &in, &out.b, feed, &f)) == 0 && rounds < 100) {
            for (uint32_t i = 0; i < out.b.len; i++) {
                double want = sin(2 * M_PI * 440.0 * (total + i) / 16000.0) * 0.5;
                double e = fabs(((float *)out.pl[0])[i] - want);
                if (e > err) err = e;
            }
            total += out.b.len;
            rounds++;
        }
        printf("  48k->16k: %u quadros, erro máx %.4f, %u pedidos\n", total, err, f.calls);
        CHECK(st == 2, "48k->16k termina com EndOfStream");
        CHECK(total >= 15999 && total <= 16001, "48k->16k: 1 s vira 16000 quadros");
        CHECK(err < 0.01, "48k->16k: seno preservado (interpolação linear)");
        avc_free(&s);
    }
    /* 44,1 kHz -> 48 kHz: razão fracionária, a interpolação linear fica perto do seno exato */
    {
        feeder f;
        avc_state s;
        avc_fmt in = {AVC_F32, 1, 0, 44100};
        tbuf out;
        memset(&s, 0, sizeof s);
        memset(&f, 0, sizeof f);
        tb_init(&f.src, AVC_F32, 1, 0, 44100, 500);
        f.total = 44100;
        f.rate = 44100;
        f.chunk = 500;
        tb_init(&out, AVC_I16, 2, 1, 48000, 512);
        uint32_t total = 0;
        double err = 0;
        while (avc_convert(&s, &none, &in, &out.b, feed, &f) == 0) {
            for (uint32_t i = 0; i < out.b.len; i++) {
                double want = sin(2 * M_PI * 440.0 * (total + i) / 48000.0) * 0.5;
                double e = fabs(((int16_t *)out.pl[0])[2 * i] / 32767.0 - want);
                if (e > err) err = e;
            }
            total += out.b.len;
        }
        printf("  44,1k->48k estéreo int16: %u quadros, erro máx %.5f\n", total, err);
        CHECK(total >= 47999 && total <= 48001, "44,1k->48k: 1 s vira 48000 quadros");
        CHECK(err < 0.002, "44,1k->48k: erro de interpolação < 0,002");
        avc_free(&s);
    }
    /* fonte sem dados agora (NoDataNow): InputRanDry e nada escrito */
    {
        avc_state s;
        avc_fmt in = {AVC_F32, 1, 0, 16000};
        tbuf out;
        int calls = 0;
        memset(&s, 0, sizeof s);
        tb_init(&out, AVC_F32, 1, 0, 48000, 64);
        int st = avc_convert(&s, &none, &in, &out.b, nodata, &calls);
        CHECK(st == 1 && out.b.len == 0 && calls == 1, "NoDataNow -> InputRanDry, um pedido só");
        avc_free(&s);
    }
    if (fails) { fprintf(stderr, "avconv: %d falhas\n", fails); return 1; }
    printf("avconv: ok (0 falhas)\n");
    return 0;
}
