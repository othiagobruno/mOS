/*
 * mOS — codificador SBC (A2DP, Apêndice B da especificação): 8 sub-bandas, 4..16 blocos, mono,
 * estéreo ou estéreo conjunto, alocação SNR ou loudness. Coeficientes do filtro protótipo: tabela
 * oficial (Proto_8, 80 taps), conferida com o BlueZ. Validado decodificando com o ffmpeg
 * (tools/sbctest.c) e medindo a relação sinal-ruído.
 */
#include "sbc.h"

static const float PROTO8[80] = {
    0.00000000E+00f, 1.56575398E-04f, 3.43256425E-04f, 5.54620202E-04f,
    8.23919506E-04f, 1.13992507E-03f, 1.47640169E-03f, 1.78371725E-03f,
    2.01182542E-03f, 2.10371989E-03f, 1.99454554E-03f, 1.61656283E-03f,
    9.02154502E-04f, -1.78805361E-04f, -1.64973098E-03f, -3.49717454E-03f,
    5.65949473E-03f, 8.02941163E-03f, 1.04584443E-02f, 1.27472335E-02f,
    1.46525263E-02f, 1.59045603E-02f, 1.62208471E-02f, 1.53184106E-02f,
    1.29371806E-02f, 8.85757540E-03f, 2.92408442E-03f, -4.91578024E-03f,
    -1.46404076E-02f, -2.61098752E-02f, -3.90751381E-02f, -5.31873032E-02f,
    6.79989431E-02f, 8.29847578E-02f, 9.75753918E-02f, 1.11196689E-01f,
    1.23264548E-01f, 1.33264415E-01f, 1.40753505E-01f, 1.45389847E-01f,
    1.46955068E-01f, 1.45389847E-01f, 1.40753505E-01f, 1.33264415E-01f,
    1.23264548E-01f, 1.11196689E-01f, 9.75753918E-02f, 8.29847578E-02f,
    -6.79989431E-02f, -5.31873032E-02f, -3.90751381E-02f, -2.61098752E-02f,
    -1.46404076E-02f, -4.91578024E-03f, 2.92408442E-03f, 8.85757540E-03f,
    1.29371806E-02f, 1.53184106E-02f, 1.62208471E-02f, 1.59045603E-02f,
    1.46525263E-02f, 1.27472335E-02f, 1.04584443E-02f, 8.02941163E-03f,
    -5.65949473E-03f, -3.49717454E-03f, -1.64973098E-03f, -1.78805361E-04f,
    9.02154502E-04f, 1.61656283E-03f, 1.99454554E-03f, 2.10371989E-03f,
    2.01182542E-03f, 1.78371725E-03f, 1.47640169E-03f, 1.13992507E-03f,
    8.23919506E-04f, 5.54620202E-04f, 3.43256425E-04f, 1.56575398E-04f,

};

static float M[8][16];                      /* M[k][i] = cos((k + 0.5)(i - 4) pi / 8) */
static bool m_ready;

static double dcos(double x) {               /* cos com redução de faixa (sem libm no kernel) */
    const double pi = 3.14159265358979323846;
    while (x > pi) x -= 2 * pi;
    while (x < -pi) x += 2 * pi;
    double x2 = x * x, t = 1, s = 1;
    for (int n = 1; n < 14; n++) { t *= -x2 / ((2 * n - 1) * (2 * n)); s += t; }
    return s;
}

void sbc_init(sbc_enc_t *e, int freq, int blocks, int mode, int alloc, int bitpool) {
    memset(e, 0, sizeof *e);
    e->freq = freq; e->blocks = blocks; e->mode = mode; e->alloc = alloc; e->bitpool = bitpool;
    e->channels = mode == SBC_MODE_MONO ? 1 : 2;
    if (!m_ready) {
        for (int k = 0; k < 8; k++)
            for (int i = 0; i < 16; i++) M[k][i] = (float)dcos((k + 0.5) * (i - 4) * 3.14159265358979323846 / 8);
        m_ready = true;
    }
}

int sbc_frame_len(const sbc_enc_t *e) {
    int nb = 4 + (4 * 8 * e->channels) / 8;
    int bits = e->mode == SBC_MODE_JOINT ? 8 + e->blocks * e->bitpool : e->mode == SBC_MODE_STEREO ? e->blocks * e->bitpool : e->blocks * e->channels * e->bitpool;
    return nb + (bits + 7) / 8;
}

/* ---- escrita de bits (MSB primeiro) e CRC-8 (x^8 + x^4 + x^3 + x^2 + 1, inicial 0x0F) */
typedef struct { u8 *p; int bit; } bw_t;
static void put(bw_t *w, u32 v, int n) {
    for (int i = n - 1; i >= 0; i--) {
        if (v >> i & 1) w->p[w->bit >> 3] |= (u8)(0x80 >> (w->bit & 7));
        w->bit++;
    }
}
static u8 crc_bits(u8 crc, u32 v, int n) {
    for (int i = n - 1; i >= 0; i--) {
        int b = (int)(v >> i & 1) ^ (crc >> 7);
        crc = (u8)(crc << 1);
        if (b) crc ^= 0x1D;
    }
    return crc;
}

static void alloc_bits(const sbc_enc_t *e, const int sf[2][8], int bits[2][8]) {
    static const int off8[4][8] = {{-2, 0, 0, 0, 0, 0, 0, 1}, {-3, 0, 0, 0, 0, 0, 1, 2},
                                   {-4, 0, 0, 0, 0, 0, 1, 2}, {-4, 0, 0, 0, 0, 0, 1, 2}};
    int need[2][8];
    for (int ch = 0; ch < e->channels; ch++)
        for (int sb = 0; sb < 8; sb++) {
            if (e->alloc == SBC_ALLOC_SNR) need[ch][sb] = sf[ch][sb];
            else if (!sf[ch][sb]) need[ch][sb] = -5;
            else {
                int loud = sf[ch][sb] - off8[e->freq][sb];
                need[ch][sb] = loud > 0 ? loud / 2 : loud;
            }
        }
    /* estéreo/conjunto: os dois canais dividem o bitpool; mono/dual: um bitpool por canal */
    int groups = (e->mode == SBC_MODE_STEREO || e->mode == SBC_MODE_JOINT) ? 1 : e->channels;
    for (int g = 0; g < groups; g++) {
        int c0 = groups == 1 ? 0 : g, c1 = groups == 1 ? e->channels - 1 : g;
        int maxn = 0;
        for (int ch = c0; ch <= c1; ch++) for (int sb = 0; sb < 8; sb++) if (need[ch][sb] > maxn) maxn = need[ch][sb];
        int bitcount = 0, slicecount = 0, slice = maxn + 1;
        do {
            slice--;
            bitcount += slicecount;
            slicecount = 0;
            for (int ch = c0; ch <= c1; ch++)
                for (int sb = 0; sb < 8; sb++) {
                    if (need[ch][sb] > slice + 1 && need[ch][sb] < slice + 16) slicecount++;
                    else if (need[ch][sb] == slice + 1) slicecount += 2;
                }
        } while (bitcount + slicecount < e->bitpool);
        if (bitcount + slicecount == e->bitpool) { bitcount += slicecount; slice--; }
        for (int ch = c0; ch <= c1; ch++)
            for (int sb = 0; sb < 8; sb++) bits[ch][sb] = need[ch][sb] < slice + 2 ? 0 : MIN(need[ch][sb] - slice, 16);
        int ch = c0, sb = 0;
        while (bitcount < e->bitpool && sb < 8) {
            if (bits[ch][sb] >= 2 && bits[ch][sb] < 16) { bits[ch][sb]++; bitcount++; }
            else if (need[ch][sb] == slice + 1 && e->bitpool > bitcount + 1) { bits[ch][sb] = 2; bitcount += 2; }
            if (ch == c1) { ch = c0; sb++; } else ch++;
        }
        ch = c0; sb = 0;
        while (bitcount < e->bitpool && sb < 8) {
            if (bits[ch][sb] < 16) { bits[ch][sb]++; bitcount++; }
            if (ch == c1) { ch = c0; sb++; } else ch++;
        }
    }
}

/* codifica blocks*8 quadros PCM (16 bits, intercalados se estéreo); devolve o tamanho do quadro SBC */
int sbc_encode(sbc_enc_t *e, const i16 *pcm, u8 *out) {
    static float s[16][2][8];                /* amostras de sub-banda [bloco][canal][sub-banda] */
    int nch = e->channels;
    for (int blk = 0; blk < e->blocks; blk++)
        for (int ch = 0; ch < nch; ch++) {
            float *X = e->X[ch];
            for (int i = 79; i >= 8; i--) X[i] = X[i - 8];
            for (int i = 7; i >= 0; i--) X[i] = pcm[(blk * 8 + (7 - i)) * nch + ch];
            float Y[16];
            for (int i = 0; i < 16; i++) {
                float acc = 0;
                for (int j = 0; j < 5; j++) acc += PROTO8[i + j * 16] * X[i + j * 16];
                Y[i] = acc;
            }
            for (int k = 0; k < 8; k++) {
                float acc = 0;
                for (int i = 0; i < 16; i++) acc += M[k][i] * Y[i];
                s[blk][ch][k] = acc;
            }
        }
    /* fatores de escala: menor sf com |s| < 2^(sf+1) */
    int sf[2][8] = {{0}}, join = 0;
    for (int ch = 0; ch < nch; ch++)
        for (int sb = 0; sb < 8; sb++) {
            float m = 0;
            for (int blk = 0; blk < e->blocks; blk++) { float v = s[blk][ch][sb] < 0 ? -s[blk][ch][sb] : s[blk][ch][sb]; if (v > m) m = v; }
            int x = 0;
            while (x < 15 && m >= (float)(1 << (x + 1))) x++;
            sf[ch][sb] = x;
        }
    if (e->mode == SBC_MODE_JOINT)                         /* M/S onde reduz os fatores de escala */
        for (int sb = 0; sb < 7; sb++) {
            float mm = 0, ms = 0;
            for (int blk = 0; blk < e->blocks; blk++) {
                float a = (s[blk][0][sb] + s[blk][1][sb]) / 2, b = (s[blk][0][sb] - s[blk][1][sb]) / 2;
                if (a < 0) a = -a;
                if (b < 0) b = -b;
                if (a > mm) mm = a;
                if (b > ms) ms = b;
            }
            int sm = 0, ss = 0;
            while (sm < 15 && mm >= (float)(1 << (sm + 1))) sm++;
            while (ss < 15 && ms >= (float)(1 << (ss + 1))) ss++;
            if (sm + ss < sf[0][sb] + sf[1][sb]) {
                join |= 1 << (7 - sb);
                for (int blk = 0; blk < e->blocks; blk++) {
                    float a = s[blk][0][sb], b = s[blk][1][sb];
                    s[blk][0][sb] = (a + b) / 2;
                    s[blk][1][sb] = (a - b) / 2;
                }
                sf[0][sb] = sm; sf[1][sb] = ss;
            }
        }
    int bits[2][8] = {{0}};
    alloc_bits(e, sf, bits);
    int len = sbc_frame_len(e);
    memset(out, 0, len);
    u8 b1 = (u8)(e->freq << 6 | ((e->blocks / 4 - 1) << 4) | e->mode << 2 | e->alloc << 1 | 1);
    out[0] = 0x9C; out[1] = b1; out[2] = (u8)e->bitpool;
    u8 crc = 0x0F;
    crc = crc_bits(crc, b1, 8);
    crc = crc_bits(crc, (u32)e->bitpool, 8);
    bw_t w = {out, 32};
    if (e->mode == SBC_MODE_JOINT) { put(&w, (u32)join, 8); crc = crc_bits(crc, (u32)join, 8); }
    for (int ch = 0; ch < nch; ch++)
        for (int sb = 0; sb < 8; sb++) { put(&w, (u32)sf[ch][sb], 4); crc = crc_bits(crc, (u32)sf[ch][sb], 4); }
    out[3] = crc;
    for (int blk = 0; blk < e->blocks; blk++)
        for (int ch = 0; ch < nch; ch++)
            for (int sb = 0; sb < 8; sb++) {
                int nb = bits[ch][sb];
                if (!nb) continue;
                u32 levels = (1u << nb) - 1;
                float scale = (float)(1 << (sf[ch][sb] + 1));
                int q = (int)((s[blk][ch][sb] / scale + 1.0f) * (float)levels / 2.0f);
                if (q < 0) q = 0;
                if (q > (int)levels) q = (int)levels;
                put(&w, (u32)q, nb);
            }
    return len;
}
