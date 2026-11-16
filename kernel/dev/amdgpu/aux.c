/* mOS — canal AUX do DisplayPort pelo motor DP_AUX do DCN 1.0 (o mesmo do dce_aux.c do amdgpu, MIT):
 * transações nativas (DPCD) e I2C sobre AUX (EDID e DDC/CI de monitores DP), com novas tentativas em DEFER.
 * Ver amdgpu.h. Testado no host contra um motor AUX simulado com um sink DP (tools/amdgpu_hw_host.c).
 *
 * Uma transação: pede o motor por arbitragem (o DMCU também usa), monta o cabeçalho (comando e endereço de 20
 * bits, tamanho - 1) e os dados de escrita no buffer, dá GO, espera DONE e lê a resposta (o primeiro byte traz o
 * código: ACK, NACK ou DEFER, nativo ou de I2C). No máximo 16 bytes por transação. */
#include "amdgpu.h"
#include "regs.h"
#ifndef MOS_HOST_TEST
#include "syscall.h"
#endif

#ifdef MOS_HOST_TEST
#define RR(a, o)    ((void)(a), amd_host_rreg(o))
#define WR(a, o, v) ((void)(a), amd_host_wreg(o, v))
#define DELAY_US(n) ((void)(n))
#else
#define RR(a, o)    mmio_r32((a)->mmio, (o))
#define WR(a, o, v) mmio_w32((a)->mmio, (o), (v))
#define DELAY_US(n) do { for (u32 _i = 0; _i < (u32)(n); _i++) io_wait(); } while (0)
#endif

#define SPIN_MAX  200000u
#define RETRIES   7

static bool aux_acquire(amdgpu_t *a, u32 i) {
    u32 arb = RR(a, R_AUX_ARB_CONTROL(i));
    if (((arb & AUX_ARB_STATUS_MASK) >> AUX_ARB_STATUS_SHIFT) == AUX_ST_DMCU) return false;
    u32 c = RR(a, R_AUX_CONTROL(i));
    if (!(c & AUX_EN)) {
        WR(a, R_AUX_CONTROL(i), c | AUX_EN | AUX_RESET);
        for (u32 n = 0; n < 1000 && !(RR(a, R_AUX_CONTROL(i)) & AUX_RESET_DONE); n++) DELAY_US(1);
        WR(a, R_AUX_CONTROL(i), (c | AUX_EN) & ~AUX_RESET);
        for (u32 n = 0; n < 1000 && (RR(a, R_AUX_CONTROL(i)) & AUX_RESET_DONE); n++) DELAY_US(1);
    }
    WR(a, R_AUX_ARB_CONTROL(i), RR(a, R_AUX_ARB_CONTROL(i)) | AUX_SW_USE_REQ);
    return ((RR(a, R_AUX_ARB_CONTROL(i)) & AUX_ARB_STATUS_MASK) >> AUX_ARB_STATUS_SHIFT) == AUX_ST_SW;
}

static void aux_release(amdgpu_t *a, u32 i) {
    WR(a, R_AUX_ARB_CONTROL(i), (RR(a, R_AUX_ARB_CONTROL(i)) & ~AUX_SW_USE_REQ) | AUX_SW_DONE_USING);
}

/* Uma transação crua. cmd: AUX_CMD_*. Devolve os bytes lidos (>= 0) e *reply = código da resposta, ou -MOS_*. */
static int aux_once(amdgpu_t *a, u32 i, u8 cmd, u32 addr, u8 *buf, u32 len, u8 *reply) {
    bool wr = !(cmd & AUX_CMD_READ);
    WR(a, R_AUX_INTERRUPT_CONTROL(i), RR(a, R_AUX_INTERRUPT_CONTROL(i)) | AUX_SW_DONE_ACK);
    for (u32 n = 0; n < SPIN_MAX && (RR(a, R_AUX_SW_STATUS(i)) & AUX_SW_DONE); n++) DELAY_US(1);
    u32 bytes = (len ? 4 : 3) + (wr ? len : 0);
    u32 c = RR(a, R_AUX_SW_CONTROL(i)) & ~(AUX_SW_START_DELAY_MASK | AUX_SW_WR_BYTES_MASK | AUX_SW_GO);
    WR(a, R_AUX_SW_CONTROL(i), c | bytes << AUX_SW_WR_BYTES_SHIFT);
    WR(a, R_AUX_SW_DATA(i), AUX_SW_AUTOINC_DISABLE | (u32)(cmd | ((addr >> 16) & 0xF)) << AUX_SW_DATA_SHIFT);
    WR(a, R_AUX_SW_DATA(i), ((addr >> 8) & 0xFF) << AUX_SW_DATA_SHIFT);
    WR(a, R_AUX_SW_DATA(i), (addr & 0xFF) << AUX_SW_DATA_SHIFT);
    if (len) WR(a, R_AUX_SW_DATA(i), (len - 1) << AUX_SW_DATA_SHIFT);
    if (wr) for (u32 k = 0; k < len; k++) WR(a, R_AUX_SW_DATA(i), (u32)buf[k] << AUX_SW_DATA_SHIFT);
    WR(a, R_AUX_SW_CONTROL(i), RR(a, R_AUX_SW_CONTROL(i)) | AUX_SW_GO);
    u32 st = 0, n = 0;
    for (; n < SPIN_MAX; n++) { st = RR(a, R_AUX_SW_STATUS(i)); if (st & AUX_SW_DONE) break; DELAY_US(1); }
    if (n >= SPIN_MAX) return -MOS_ETIMEDOUT;
    if (st & AUX_SW_HPD_DISCON) return -MOS_ENXIO;                 /* nada plugado */
    if (st & (AUX_SW_RX_TIMEOUT | AUX_SW_RX_TIMEOUT_STATE)) return -MOS_ETIMEDOUT;
    if (st & (AUX_SW_RX_INVALID_STOP | AUX_SW_RX_NO_DET | AUX_SW_RX_INVALID_H | AUX_SW_RX_INVALID_L | AUX_SW_RX_OVERFLOW)) return -MOS_EIO;
    u32 got = (st & AUX_SW_REPLY_COUNT_MASK) >> AUX_SW_REPLY_COUNT_SHIFT;
    if (!got) return -MOS_EIO;
    WR(a, R_AUX_SW_DATA(i), AUX_SW_DATA_RW | AUX_SW_AUTOINC_DISABLE);   /* índice 0, leitura */
    *reply = (u8)(((RR(a, R_AUX_SW_DATA(i)) & AUX_SW_DATA_MASK) >> AUX_SW_DATA_SHIFT) >> 4);
    if (*reply) return 0;
    got--;
    if (got > len && !wr) return -MOS_EIO;
    for (u32 k = 0; k < got && !wr; k++) buf[k] = (u8)((RR(a, R_AUX_SW_DATA(i)) & AUX_SW_DATA_MASK) >> AUX_SW_DATA_SHIFT);
    return (int)(wr ? len : got);
}

/* Transação com novas tentativas (DEFER nativo ou de I2C: o sink pede para repetir). */
int amdgpu_aux_xfer(amdgpu_t *a, u32 inst, u8 cmd, u32 addr, u8 *buf, u32 len) {
    if (!a || !a->ready || inst >= AMD_NUM_AUX || len > 16) return -MOS_EINVAL;
    if (!aux_acquire(a, inst)) { aux_release(a, inst); return -MOS_EBUSY; }
    int r = -MOS_EIO;
    for (int t = 0; t < RETRIES; t++) {
        u8 reply = 0;
        r = aux_once(a, inst, cmd, addr, buf, len, &reply);
        if (r < 0 && r != -MOS_ETIMEDOUT) break;
        if (r >= 0) {
            if (reply == AUX_REPLY_ACK) break;
            if (reply == AUX_REPLY_NACK || reply == AUX_REPLY_I2C_NACK) { r = -MOS_ENXIO; break; }
            r = -MOS_EAGAIN;                                        /* DEFER: espera e repete */
        }
        a->aux_retries++;
        DELAY_US(400);
    }
    aux_release(a, inst);
    a->aux_ops++;
    if (r < 0) a->aux_errors++;
    return r;
}

/* DPCD: leitura e escrita em blocos de até 16 bytes */
int amdgpu_dpcd_read(amdgpu_t *a, u32 inst, u32 addr, u8 *buf, u32 len) {
    for (u32 o = 0; o < len;) {
        u32 n = len - o > 16 ? 16 : len - o;
        int r = amdgpu_aux_xfer(a, inst, AUX_CMD_NATIVE_READ, addr + o, buf + o, n);
        if (r < 0) return r;
        if (!r) return -MOS_EIO;
        o += (u32)r;
    }
    return (int)len;
}

int amdgpu_dpcd_write(amdgpu_t *a, u32 inst, u32 addr, const u8 *buf, u32 len) {
    u8 tmp[16];
    for (u32 o = 0; o < len;) {
        u32 n = len - o > 16 ? 16 : len - o;
        memcpy(tmp, buf + o, n);
        int r = amdgpu_aux_xfer(a, inst, AUX_CMD_NATIVE_WRITE, addr + o, tmp, n);
        if (r < 0) return r;
        o += n;
    }
    return (int)len;
}

/* EDID por I2C sobre AUX: segmento (0x30), deslocamento (0x50) e leituras de 16 bytes com MOT; o fim (sem MOT,
 * tamanho 0) solta o barramento. */
static int edid_block_aux(amdgpu_t *a, u32 inst, u32 block, u8 *out) {
    u8 seg = (u8)(block / 2), off = (u8)((block & 1) * 128);
    int r;
    if (seg && (r = amdgpu_aux_xfer(a, inst, AUX_CMD_I2C_WRITE | AUX_CMD_MOT, 0x30, &seg, 1)) < 0) return r;
    if ((r = amdgpu_aux_xfer(a, inst, AUX_CMD_I2C_WRITE | AUX_CMD_MOT, 0x50, &off, 1)) < 0) return r;
    for (u32 o = 0; o < 128;) {
        r = amdgpu_aux_xfer(a, inst, AUX_CMD_I2C_READ | AUX_CMD_MOT, 0x50, out + o, 16);
        if (r < 0) return r;
        if (!r) return -MOS_EIO;
        o += (u32)r;
    }
    amdgpu_aux_xfer(a, inst, AUX_CMD_I2C_READ, 0x50, NULL, 0);
    return 0;
}

int amdgpu_aux_read_edid(amdgpu_t *a, u32 inst, u8 *buf, u32 max) {
    static const u8 hdr[8] = {0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0};
    if (max < 128) return -MOS_EINVAL;
    int e = edid_block_aux(a, inst, 0, buf);
    if (e) return e;
    u8 s = 0;
    for (int k = 0; k < 128; k++) s = (u8)(s + buf[k]);
    if (memcmp(buf, hdr, 8) || s) return -MOS_EIO;
    u32 total = 128;
    for (u32 b = 1; b <= buf[126] && total + 128 <= max; b++) {
        if (edid_block_aux(a, inst, b, buf + total)) break;
        s = 0;
        for (int k = 0; k < 128; k++) s = (u8)(s + buf[total + k]);
        if (s) break;
        total += 128;
    }
    return (int)total;
}
