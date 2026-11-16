/* mOS — I2C das linhas DDC pelo motor de I2C do DCN 1.0 (o mesmo do dce_i2c_hw.c do amdgpu, MIT): EDID ao
 * vivo (blocos de 128 bytes, segmentos E-DDC) e DDC/CI (MCCS) para ler e ajustar o brilho do monitor externo.
 * Ver amdgpu.h. Testado no host contra um motor de I2C simulado com um monitor em cada linha
 * (tools/amdgpu_hw_host.c).
 *
 * O motor é dividido com o firmware do display (DMCU): o driver pede o uso por arbitragem, programa até 4
 * transações (endereço + dados no buffer de 144 bytes), dá GO, espera DONE/NACK/TIMEOUT, lê a resposta do
 * mesmo buffer e devolve o motor. */
#include "amdgpu.h"
#include "regs.h"
#ifndef MOS_HOST_TEST
#include "syscall.h"
#endif

#ifdef MOS_HOST_TEST
#define RR(a, o)    ((void)(a), amd_host_rreg(o))
#define WR(a, o, v) ((void)(a), amd_host_wreg(o, v))
#define DELAY_MS(n) sleep_ms(n)
#else
#define RR(a, o)    mmio_r32((a)->mmio, (o))
#define WR(a, o, v) mmio_w32((a)->mmio, (o), (v))
#define DELAY_MS(n) sleep_ms(n)
#endif

#define I2C_KHZ        100u
#define SPIN_MAX       200000u

static bool arb_acquire(amdgpu_t *a) {
    u32 st = (RR(a, R_DC_I2C_ARBITRATION) & I2C_ARB_STATUS_MASK) >> I2C_ARB_STATUS_SHIFT;
    if (st == I2C_ST_USED_BY_SW) return true;
    if (st == I2C_ST_USED_BY_HW) return false;
    WR(a, R_DC_I2C_ARBITRATION, RR(a, R_DC_I2C_ARBITRATION) | I2C_ARB_SW_USE_REQ);
    st = (RR(a, R_DC_I2C_ARBITRATION) & I2C_ARB_STATUS_MASK) >> I2C_ARB_STATUS_SHIFT;
    return st == I2C_ST_USED_BY_SW;
}

static void set_speed(amdgpu_t *a, u32 ddc) {
    u32 d = RR(a, R_MICROSECOND_TIME_BASE_DIV);
    u32 base = d & USEC_BASE_DIV_MASK, xtal = (d & XTAL_REF_DIV_MASK) >> XTAL_REF_DIV_SHIFT;
    if (!xtal) xtal = 2;
    u32 ref = base ? base * 1000 : 48000;           /* kHz; sem o divisor programado, o cristal de 48 MHz da Raven */
    u32 prescale = ref / xtal / I2C_KHZ;
    u32 v = RR(a, R_DC_I2C_DDC_SPEED(ddc)) & ~(0xFFFF0000u | I2C_SPEED_THRESHOLD_MASK | I2C_SPEED_SS_TIMING_MASK);
    WR(a, R_DC_I2C_DDC_SPEED(ddc), v | prescale << I2C_SPEED_PRESCALE_SHIFT | 2u | 2u << I2C_SPEED_SS_TIMING_SHIFT);
}

static bool engine_setup(amdgpu_t *a, u32 ddc) {
    WR(a, R_DC_I2C_CONTROL, RR(a, R_DC_I2C_CONTROL) & ~I2C_SOFT_RESET);
    if (!arb_acquire(a)) return false;
    set_speed(a, ddc);
    u32 c = RR(a, R_DC_I2C_CONTROL) & ~(I2C_GO | I2C_SEND_RESET | I2C_TRANS_COUNT_MASK | I2C_DDC_SELECT_MASK);
    WR(a, R_DC_I2C_CONTROL, c | I2C_SW_STATUS_RESET | ddc << I2C_DDC_SELECT_SHIFT);
    u32 su = RR(a, R_DC_I2C_DDC_SETUP(ddc)) & ~(0xFF000000u | I2C_SETUP_ENABLE);
    WR(a, R_DC_I2C_DDC_SETUP(ddc), su | 255u << I2C_SETUP_TIME_LIMIT_SHIFT | I2C_SETUP_ENABLE);
    WR(a, R_DC_I2C_ARBITRATION, RR(a, R_DC_I2C_ARBITRATION) & ~I2C_ARB_NO_QUEUED_SW_GO);
    return true;
}

static void engine_release(amdgpu_t *a, u32 ddc) {
    bool safe = (RR(a, R_DC_I2C_SW_STATUS) & I2C_SW_STATUS_MASK) == I2C_ST_USED_BY_SW;
    WR(a, R_DC_I2C_CONTROL, RR(a, R_DC_I2C_CONTROL) | I2C_SW_STATUS_RESET | (safe ? I2C_SOFT_RESET : 0));
    WR(a, R_DC_I2C_DDC_SETUP(ddc), RR(a, R_DC_I2C_DDC_SETUP(ddc)) & ~I2C_SETUP_ENABLE);
    WR(a, R_DC_I2C_ARBITRATION, RR(a, R_DC_I2C_ARBITRATION) | I2C_ARB_SW_DONE);
}

/* Uma operação de até 3 transações: escritas (cada uma num endereço) e uma leitura no fim. 0 ou -MOS_*:
 * -MOS_ENXIO sem resposta (NACK), -MOS_ETIMEDOUT, -MOS_EBUSY (motor com o firmware), -MOS_EINVAL. */
typedef struct { u8 addr; bool read; const u8 *wr; u8 *rd; u32 len; } i2c_tr_t;

static int i2c_run(amdgpu_t *a, u32 ddc, const i2c_tr_t *t, u32 n) {
    u32 used = 0;
    for (u32 i = 0; i < n; i++) used += 1 + t[i].len;
    if (!n || n > 4 || used > AMD_I2C_BUFFER || ddc >= AMD_NUM_DDC) return -MOS_EINVAL;
    if (!engine_setup(a, ddc)) return -MOS_EBUSY;
    u32 wpos = 0;
    for (u32 i = 0; i < n; i++) {
        bool last = i == n - 1;
        u32 tr = I2C_TR_STOP_ON_NACK | I2C_TR_START | (t[i].read ? I2C_TR_RW : 0) | t[i].len << I2C_TR_COUNT_SHIFT | (last ? I2C_TR_STOP : 0);
        WR(a, R_DC_I2C_TRANSACTION(i), tr);
        u32 abyte = (u32)t[i].addr << 1 | (t[i].read ? 1 : 0);
        WR(a, R_DC_I2C_DATA, abyte << I2C_DATA_SHIFT | (i == 0 ? I2C_INDEX_WRITE : 0));
        wpos++;
        if (!t[i].read) for (u32 k = 0; k < t[i].len; k++) { WR(a, R_DC_I2C_DATA, (u32)t[i].wr[k] << I2C_DATA_SHIFT); wpos++; }
    }
    WR(a, R_DC_I2C_DDC_SETUP(ddc), RR(a, R_DC_I2C_DDC_SETUP(ddc)) &
       ~(I2C_SETUP_DATA_DRIVE_EN | I2C_SETUP_CLK_DRIVE_EN | I2C_SETUP_DATA_DRIVE_SEL | I2C_SETUP_DELAYS_MASK));
    u32 c = RR(a, R_DC_I2C_CONTROL) & ~(I2C_SOFT_RESET | I2C_SW_STATUS_RESET | I2C_SEND_RESET | I2C_GO | I2C_TRANS_COUNT_MASK);
    WR(a, R_DC_I2C_CONTROL, c | (n - 1) << I2C_TRANS_COUNT_SHIFT);
    WR(a, R_DC_I2C_CONTROL, RR(a, R_DC_I2C_CONTROL) | I2C_GO);
    /* 100 kHz: ~90 µs por byte; o prazo cobre o buffer inteiro com folga */
    u32 st = 0, spins = 0;
    for (; spins < SPIN_MAX; spins++) {
        st = RR(a, R_DC_I2C_SW_STATUS);
        if ((st & I2C_SW_STATUS_MASK) != I2C_ST_USED_BY_SW) break;
    }
    int e = 0;
    if (spins >= SPIN_MAX || (st & I2C_SW_TIMEOUT)) e = -MOS_ETIMEDOUT;
    else if (st & I2C_SW_NACK) e = -MOS_ENXIO;
    else if (st & I2C_SW_ABORTED) e = -MOS_EIO;
    else if (!(st & I2C_SW_DONE)) e = -MOS_EIO;
    if (!e) {
        /* resposta: o que foi lido fica no buffer logo depois do que foi escrito */
        for (u32 i = 0; i < n; i++) {
            if (!t[i].read) continue;
            WR(a, R_DC_I2C_DATA, I2C_DATA_RW | wpos << I2C_INDEX_SHIFT | I2C_INDEX_WRITE);
            for (u32 k = 0; k < t[i].len; k++) t[i].rd[k] = (u8)((RR(a, R_DC_I2C_DATA) & I2C_DATA_MASK) >> I2C_DATA_SHIFT);
            wpos += t[i].len;
        }
    }
    engine_release(a, ddc);
    a->i2c_ops++;
    if (e) a->i2c_errors++;
    return e;
}

/* bloco de 128 bytes do EDID (segmento = bloco / 2, pelo ponteiro 0x30 do E-DDC) */
static int edid_block(amdgpu_t *a, u32 ddc, u32 block, u8 *out) {
    u8 seg = (u8)(block / 2), off = (u8)((block & 1) * 128);
    i2c_tr_t t[3];
    u32 n = 0;
    if (seg) t[n++] = (i2c_tr_t){0x30, false, &seg, NULL, 1};
    t[n++] = (i2c_tr_t){0x50, false, &off, NULL, 1};
    t[n++] = (i2c_tr_t){0x50, true, NULL, out, 128};
    return i2c_run(a, ddc, t, n);
}

static bool block_ok(const u8 *b) { u8 s = 0; for (int i = 0; i < 128; i++) s = (u8)(s + b[i]); return s == 0; }

/* EDID completo da linha ddc (0..5): até max bytes, em blocos de 128. Devolve o tamanho ou -MOS_*. */
int amdgpu_ddc_read_edid(amdgpu_t *a, u32 ddc, u8 *buf, u32 max) {
    static const u8 hdr[8] = {0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0};
    if (!a || !a->ready || max < 128) return -MOS_EINVAL;
    int e = -MOS_EIO;
    for (int tries = 0; tries < 3; tries++) {         /* monitores lentos às vezes devolvem lixo na primeira */
        e = edid_block(a, ddc, 0, buf);
        if (e == -MOS_ENXIO) return e;                 /* ninguém na linha */
        if (!e && !memcmp(buf, hdr, 8) && block_ok(buf)) break;
        if (!e) e = -MOS_EIO;
    }
    if (e) return e;
    u32 ext = buf[126], total = 128;
    for (u32 b = 1; b <= ext && total + 128 <= max; b++) {
        int r = edid_block(a, ddc, b, buf + total);
        if (r || !block_ok(buf + total)) break;        /* fica com os blocos bons */
        total += 128;
    }
    return (int)total;
}

/* ------------------------------------------------------------ DDC/CI (VESA MCCS) no endereço 0x37 */
static u8 ddcci_chk(u8 first, const u8 *p, u32 n) { u8 c = first; for (u32 i = 0; i < n; i++) c ^= p[i]; return c; }

int amdgpu_ddcci_get_vcp(amdgpu_t *a, u32 ddc, u8 vcp, u16 *cur, u16 *max) {
    if (!a || !a->ready) return -MOS_EINVAL;
    u8 req[5] = {0x51, 0x82, 0x01, vcp, 0};
    req[4] = ddcci_chk(0x6E, req, 4);
    i2c_tr_t w = {0x37, false, req, NULL, 5};
    int e = i2c_run(a, ddc, &w, 1);
    if (e) return e;
    DELAY_MS(40);                                      /* o monitor precisa de 40 ms antes da resposta (MCCS) */
    u8 r[11];
    i2c_tr_t rd = {0x37, true, NULL, r, 11};
    e = i2c_run(a, ddc, &rd, 1);
    if (e) return e;
    if (r[0] != 0x6E || (r[1] & 0x7F) != 8 || r[2] != 0x02 || r[4] != vcp || ddcci_chk(0x50, r, 10) != r[10]) return -MOS_EIO;
    if (r[3] != 0) return -MOS_ENOENT;                 /* o monitor não tem esse controle */
    if (max) *max = (u16)(r[6] << 8 | r[7]);
    if (cur) *cur = (u16)(r[8] << 8 | r[9]);
    return 0;
}

int amdgpu_ddcci_set_vcp(amdgpu_t *a, u32 ddc, u8 vcp, u16 val) {
    if (!a || !a->ready) return -MOS_EINVAL;
    u8 req[7] = {0x51, 0x84, 0x03, vcp, (u8)(val >> 8), (u8)val, 0};
    req[6] = ddcci_chk(0x6E, req, 6);
    i2c_tr_t w = {0x37, false, req, NULL, 7};
    int e = i2c_run(a, ddc, &w, 1);
    if (!e) DELAY_MS(50);                              /* intervalo mínimo entre comandos */
    return e;
}

/* Varre as 6 linhas e guarda quais têm monitor (EDID válido). Devolve a máscara (bit i = DDC i+1). */
u32 amdgpu_ddc_probe(amdgpu_t *a) {
    if (!a || !a->ready) return 0;
    u32 mask = 0;
    for (u32 d = 0; d < AMD_NUM_DDC; d++) {
        int n = amdgpu_ddc_read_edid(a, d, a->ddc_edid[d], sizeof a->ddc_edid[d]);
        a->ddc_edid_len[d] = n > 0 ? (u32)n : 0;
        if (n > 0) mask |= 1u << d;
    }
    a->ddc_mask = mask;
    return mask;
}
