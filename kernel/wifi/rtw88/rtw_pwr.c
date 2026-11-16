/*
 * mOS — RTL8821CE: potência de transmissão por taxa.
 *
 * índice final = base do eFuse (grupo de canal, banda, largura) + min(deslocamento por taxa, limite regulatório)
 * O deslocamento por taxa vem da tabela bb_pg (BCD, em relação à taxa de referência de cada seção) e o limite da
 * tabela txpwr_lmt (por regulador, banda, largura, seção e canal). Mesma regra do driver da Realtek.
 */
#include "rtw.h"

static const u8 rs_cck[] = {0x00, 0x01, 0x02, 0x03};
static const u8 rs_ofdm[] = {0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B};
static const u8 rs_ht1[] = {0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13};
static const u8 rs_ht2[] = {0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B};
static const u8 rs_vht1[] = {0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};
static const u8 rs_vht2[] = {0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F};
const u8 *const rtw_rate_sec[RTW_RS_NUM] = {rs_cck, rs_ofdm, rs_ht1, rs_ht2, rs_vht1, rs_vht2};
const u8 rtw_rate_sec_n[RTW_RS_NUM] = {4, 8, 8, 8, 10, 10};

static const u8 ch5g[RTW_CH5G] = {36, 38, 40, 42, 44, 46, 48, 52, 54, 56, 58, 60, 62, 64, 100, 102, 104, 106, 108, 110, 112,
                                  116, 118, 120, 122, 124, 126, 128, 132, 134, 136, 138, 140, 142, 144, 149, 151, 153, 155,
                                  157, 159, 161, 165, 167, 169, 171, 173, 175, 177};

int rtw_ch_idx(int band, int ch) {
    if (band == 0) return ch >= 1 && ch <= 14 ? ch - 1 : -1;
    for (int i = 0; i < RTW_CH5G; i++) if (ch5g[i] == ch) return i;
    return -1;
}

int rtw_rate_section(u8 r) {
    if (r <= 0x03) return RTW_RS_CCK;
    if (r <= 0x0B) return RTW_RS_OFDM;
    if (r <= 0x13) return RTW_RS_HT1;
    if (r <= 0x1B) return RTW_RS_HT2;
    if (r >= 0x2C && r <= 0x35) return RTW_RS_VHT1;
    if (r >= 0x36 && r <= 0x3F) return RTW_RS_VHT2;
    return -1;
}

static u8 bcd(u8 v) { return (u8)((v >> 4) * 10 + (v & 0xF)); }

/* endereço da tabela bb_pg -> taxas que o valor de 32 bits descreve (um byte BCD por taxa) */
static int pg_rates(u32 addr, u32 mask, u32 val, u8 *rate, u8 *pw) {
    static const struct { u16 a; u8 first; u8 n; } m[] = {
        {0xC20, 0x00, 4}, {0xC24, 0x04, 4}, {0xC28, 0x08, 4}, {0xC2C, 0x0C, 4}, {0xC30, 0x10, 4}, {0xC34, 0x14, 4},
        {0xC38, 0x18, 4}, {0xC3C, 0x2C, 4}, {0xC40, 0x30, 4}, {0xC44, 0x34, 4}, {0xC48, 0x38, 4}, {0xC4C, 0x3C, 4},
    };
    (void)mask;
    u32 a = addr & 0xFFF;
    if (a >= 0xE00) a -= 0x200;                  /* E20.. espelha C20.. (caminho B) */
    for (u32 i = 0; i < ARRAY_LEN(m); i++)
        if (m[i].a == a) {
            /* C44: VHT1SS MCS8/9 e VHT2SS MCS0/1 */
            for (int k = 0; k < m[i].n; k++) { rate[k] = (u8)(m[i].first + k); pw[k] = bcd((u8)(val >> (8 * k))); }
            return m[i].n;
        }
    return 0;
}

static i8 clamp63(int v) { return (i8)(v > 63 ? 63 : v < -63 ? -63 : v); }

void rtw_pwr_init(rtw_pwr_t *p, const rtw_pg_table_t *pg, const rtw_lmt_table_t *lmt, int regd) {
    memset(p, 0, sizeof *p);
    for (int b = 0; b < RTW_BW_N; b++)
        for (int s = 0; s < RTW_RS_NUM; s++) {
            for (int c = 0; c < 14; c++) p->lmt2[b][s][c] = 63;
            for (int c = 0; c < RTW_CH5G; c++) p->lmt5[b][s][c] = 63;
        }
    /* 1) potência por taxa (só o caminho A: o 8821C é 1T1R) */
    for (u32 i = 0; i < pg->size; i++) {
        const rtw_pg_t *e = &pg->data[i];
        if (e->rf_path != 0 || e->addr == 0xFE || e->addr == 0xFFE) continue;
        u8 rate[4], pw[4];
        int n = pg_rates(e->addr, e->bitmask, e->data, rate, pw);
        for (int k = 0; k < n; k++) {
            if (e->band == 0) p->byr2[rate[k]] = (i8)pw[k];
            else p->byr5[rate[k]] = (i8)pw[k];
        }
    }
    /* 2) referência de cada seção (última taxa; MCS7 no VHT) e deslocamentos relativos */
    for (int s = 0; s < RTW_RS_NUM; s++) {
        u8 n = rtw_rate_sec_n[s];
        u8 ref = n == 10 ? rtw_rate_sec[s][n - 3] : rtw_rate_sec[s][n - 1];
        p->base2[s] = p->byr2[ref];
        p->base5[s] = p->byr5[ref];
    }
    for (int s = 0; s < RTW_RS_NUM; s++)
        for (int k = 0; k < rtw_rate_sec_n[s]; k++) {
            u8 r = rtw_rate_sec[s][k];
            p->byr2[r] = (i8)(p->byr2[r] - p->base2[s]);
            p->byr5[r] = (i8)(p->byr5[r] - p->base5[s]);
        }
    /* 3) limites do regulador escolhido; sem entrada para ele, o mínimo entre todos (mundial) */
    bool have = false;
    for (u32 i = 0; i < lmt->size; i++) if (lmt->data[i].regd == regd) have = true;
    for (u32 i = 0; i < lmt->size; i++) {
        const rtw_lmt_t *e = &lmt->data[i];
        if (have && e->regd != regd) continue;
        if (e->bw >= RTW_BW_N || e->rs >= RTW_RS_NUM) continue;
        int ci = rtw_ch_idx(e->band, e->ch);
        if (ci < 0) continue;
        i8 v = clamp63(e->lmt);
        i8 *slot = e->band == 0 ? &p->lmt2[e->bw][e->rs][ci] : &p->lmt5[e->bw][e->rs][ci];
        if (v < *slot) *slot = v;
    }
    /* HT e VHT de 5 GHz em 20/40 MHz se completam quando um dos dois falta */
    for (int b = 0; b <= 1; b++)
        for (int c = 0; c < RTW_CH5G; c++)
            for (int k = 0; k < 2; k++) {
                int ht = k ? RTW_RS_HT2 : RTW_RS_HT1, vht = k ? RTW_RS_VHT2 : RTW_RS_VHT1;
                i8 *h = &p->lmt5[b][ht][c], *v = &p->lmt5[b][vht][c];
                if (*h == *v) continue;
                if (*h == 63) *h = *v; else if (*v == 63) *v = *h;
            }
    /* 4) limites em relação à mesma referência da potência por taxa */
    for (int b = 0; b < RTW_BW_N; b++)
        for (int s = 0; s < RTW_RS_NUM; s++) {
            for (int c = 0; c < 14; c++) p->lmt2[b][s][c] = (i8)(p->lmt2[b][s][c] - p->base2[s]);
            for (int c = 0; c < RTW_CH5G; c++) p->lmt5[b][s][c] = (i8)(p->lmt5[b][s][c] - p->base5[s]);
        }
}

/* canal central usado em cada largura, dado o canal central da largura atual (como no driver da Realtek) */
static int cch_for(int bw_now, int center, int primary, int bw) {
    if (bw == 0) return primary;
    if (bw == bw_now) return center;
    if (bw == 1 && bw_now == 2) return primary > center ? center + 4 : center - 4;
    return center;
}

i8 rtw_pwr_limit(const rtw_pwr_t *p, int band, int bw, u8 rate, int center, int primary) {
    int s = rtw_rate_section(rate);
    if (s < 0) return 63;
    int bw_now = bw;                                 /* largura do canal; a da taxa pode ser menor */
    if (s == RTW_RS_CCK || s == RTW_RS_OFDM) bw = 0;
    if ((s == RTW_RS_HT1 || s == RTW_RS_HT2) && bw > 1) bw = 1;
    i8 m = 63;
    for (int b = 0; b <= bw; b++) {
        int ch = cch_for(bw_now, center, primary, b);
        int ci = rtw_ch_idx(band, ch);
        if (ci < 0) continue;
        i8 v = band == 0 ? p->lmt2[b][s][ci] : p->lmt5[b][s][ci];
        if (v < m) m = v;
    }
    return m;
}

int rtw_ch_group(int ch, u8 rate) {
    static const u8 top2[] = {2, 5, 8, 11, 13};
    static const u8 top5[] = {42, 50, 58, 64, 106, 114, 122, 130, 138, 144, 155, 161, 171};
    if (ch == 14) return rate <= 0x03 ? 5 : 4;
    if (ch < 14) { for (int i = 0; i < 5; i++) if (ch <= top2[i]) return i; return 4; }
    for (int i = 0; i < 13; i++) if (ch <= top5[i]) return i;
    return 13;
}

static i8 nib_lo(u8 v) { return (i8)((v & 8) ? (int)(v & 0xF) - 16 : (int)(v & 0xF)); }
static i8 nib_hi(u8 v) { return nib_lo((u8)(v >> 4)); }

/* base do eFuse: struct rtw_txpwr_idx da Realtek (2,4 GHz: 6 CCK + 5 BW40 + difs; 5 GHz: 14 BW40 + difs) */
u8 rtw_pwr_base(const u8 *ef, int band, int bw, u8 rate, int ch) {
    int g = rtw_ch_group(ch, rate);
    bool mcs = (rate >= 0x0C && rate <= 0x2B) || rate >= 0x2C;
    bool ss2 = (rate >= 0x14 && rate <= 0x2B) || rate >= 0x36;
    int tx;
    if (band == 0) {
        tx = rate <= 0x03 ? ef[g] : ef[6 + g];
        u8 d1 = ef[11];                                      /* ht_1s_diff: ofdm (lo) bw20 (hi) */
        if (rate >= 0x04 && rate <= 0x0B) tx += nib_lo(d1);
        if (mcs) {
            if (bw == 0) { tx += nib_hi(d1); if (ss2) tx += nib_lo(ef[12]); }
            else if (ss2) tx += nib_hi(ef[12]);
        }
    } else {
        const u8 *f = ef + 18;
        tx = f[g];
        u8 d1 = f[14];
        if (!mcs) tx += nib_lo(d1);
        else if (bw == 0) { tx += nib_hi(d1); if (ss2) tx += nib_lo(f[15]); }
        else if (bw == 1) { if (ss2) tx += nib_hi(f[15]); }
        else {
            tx = (f[g] + f[g + 1]) / 2;
            tx += nib_hi(f[20]);                             /* vht_1s_diff.bw80 */
            if (ss2) tx += nib_hi(f[21]);
        }
    }
    return (u8)(tx < 0 ? 0 : tx);
}

u8 rtw_pwr_index(const rtw_pwr_t *p, const u8 *ef, int band, int bw, u8 rate, int center, int primary) {
    int ch = bw == 0 ? primary : center;
    int base = rtw_pwr_base(ef, band, bw, rate, ch);
    i8 off = band == 0 ? p->byr2[rate] : p->byr5[rate];
    i8 lim = rtw_pwr_limit(p, band, bw, rate, center, primary);
    int v = base + (off < lim ? off : lim);
    return (u8)(v < 0 ? 0 : v > 0x3F ? 0x3F : v);
}
