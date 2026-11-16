/*
 * mOS — RTL8821CE: núcleo sem hardware (testável no host).
 *   - tabelas condicionais da Realtek (if/elif/else/endif por RFE, interface, pacote e corte do chip)
 *   - sequências de energia carddis -> cardemu -> act e o caminho inverso
 *   - eFuse físico -> mapa lógico e os campos do 8821CE
 *   - validação do firmware rtw88/rtw8821c_fw.bin
 *
 * Os valores das sequências de energia são os da Realtek (rtw8821c.c, GPL-2.0 OR BSD-3-Clause; o mOS
 * usa a BSD-3-Clause, aviso em rtw8821c_table.c). A lógica foi escrita para o mOS.
 * Só as linhas de PCIe e "todas as interfaces" importam aqui; as de USB/SDIO ficam por fidelidade
 * à sequência e são puladas pela máscara de interface.
 */
#include "rtw.h"

/* ============================================================ tabelas condicionais
 * Cada entrada é um par de u32. Se o bit 31 do primeiro estiver ligado, é cabeçalho de condição:
 *   bits 0-7 RFE, 8-11 interface, 12-15 pacote, 24-27 corte, 28-29 ramo (0 if, 1 elif, 2 else, 3 endif),
 *   bit 30 "neg" (o par seguinte que fecha a condição), bit 31 "pos" (abre).
 * O par "pos" guarda a condição; o "neg" a avalia. Depois de um ramo verdadeiro os demais são pulados. */
static bool cond_match(u32 w, const rtw_cond_t *c) {
    u32 rfe = w & 0xFF, intf = (w >> 8) & 0xF, pkg = (w >> 12) & 0xF, cut = (w >> 24) & 0xF;
    u8 my_cut = c->cut ? c->cut : 15, my_pkg = c->pkg ? c->pkg : 15;
    if (cut && cut != my_cut) return false;
    if (pkg && pkg != my_pkg) return false;
    if (intf && intf != c->intf) return false;
    return rfe == c->rfe;
}

u32 rtw_parse_cond_table(const rtw_table_t *t, const rtw_cond_t *c, rtw_cfg_fn cfg, void *ctx) {
    bool matched = true, skipped = false;
    u32 pos = 0, n = 0;
    for (u32 i = 0; i + 1 < t->size; i += 2) {
        u32 a = t->data[i], d = t->data[i + 1];
        if (a & 0x80000000u) {                          /* pos: abre ou fecha um ramo */
            switch ((a >> 28) & 3) {
            case 3: matched = true; skipped = false; break;              /* endif */
            case 2: matched = !skipped; break;                           /* else */
            default: pos = a; break;                                     /* if / elif */
            }
        } else if (a & 0x40000000u) {                   /* neg: avalia o ramo guardado */
            if (!skipped) {
                if (cond_match(pos, c)) { matched = true; skipped = true; }
                else matched = false;
            } else matched = false;
        } else if (matched) {
            cfg(ctx, a, d);
            n++;
        }
    }
    return n;
}

/* ============================================================ sequências de energia do 8821C */
#define ALL 0xFF
#define W RTW_PWR_WRITE
#define P RTW_PWR_POLL
#define D RTW_PWR_DELAY
#define E RTW_PWR_END
#define I_ALL RTW_PWR_INTF_ALL
#define I_PCI RTW_PWR_INTF_PCI
#define I_USB RTW_PWR_INTF_USB
#define I_SDIO RTW_PWR_INTF_SDIO
#define B(n) (1u << (n))

static const rtw_pwr_cmd_t carddis_to_cardemu[] = {
    {0x0086, ALL, I_SDIO, 1, W, B(0), 0},
    {0x0086, ALL, I_SDIO, 1, P, B(1), B(1)},
    {0x004A, ALL, I_USB, 0, W, B(0), 0},
    {0x0005, ALL, I_ALL, 0, W, B(3) | B(4) | B(7), 0},
    {0x0300, ALL, I_PCI, 0, W, 0xFF, 0},
    {0x0301, ALL, I_PCI, 0, W, 0xFF, 0},
    {0xFFFF, ALL, I_ALL, 0, E, 0, 0},
};
static const rtw_pwr_cmd_t cardemu_to_act[] = {
    {0x0020, ALL, I_USB | I_SDIO, 0, W, B(0), B(0)},
    {0x0001, ALL, I_USB | I_SDIO, 0, D, 1, RTW_PWR_DELAY_MS},
    {0x0000, ALL, I_USB | I_SDIO, 0, W, B(5), 0},
    {0x0005, ALL, I_ALL, 0, W, B(4) | B(3) | B(2), 0},
    {0x0075, ALL, I_PCI, 0, W, B(0), B(0)},
    {0x0006, ALL, I_ALL, 0, P, B(1), B(1)},
    {0x0075, ALL, I_PCI, 0, W, B(0), 0},
    {0x0006, ALL, I_ALL, 0, W, B(0), B(0)},
    {0x0005, ALL, I_ALL, 0, W, B(7), 0},
    {0x0005, ALL, I_ALL, 0, W, B(4) | B(3), 0},
    {0x10C3, ALL, I_USB, 0, W, B(0), B(0)},
    {0x0005, ALL, I_ALL, 0, W, B(0), B(0)},
    {0x0005, ALL, I_ALL, 0, P, B(0), 0},
    {0x0020, ALL, I_ALL, 0, W, B(3), B(3)},
    {0x0074, ALL, I_PCI, 0, W, B(5), B(5)},
    {0x0022, ALL, I_PCI, 0, W, B(1), 0},
    {0x0062, ALL, I_PCI, 0, W, B(7) | B(6) | B(5), B(7) | B(6) | B(5)},
    {0x0061, ALL, I_PCI, 0, W, B(7) | B(6) | B(5), 0},
    {0x007C, ALL, I_ALL, 0, W, B(1), 0},
    {0xFFFF, ALL, I_ALL, 0, E, 0, 0},
};
static const rtw_pwr_cmd_t act_to_cardemu[] = {
    {0x0093, ALL, I_ALL, 0, W, B(3), 0},
    {0x001F, ALL, I_ALL, 0, W, 0xFF, 0},
    {0x0049, ALL, I_ALL, 0, W, B(1), 0},
    {0x0006, ALL, I_ALL, 0, W, B(0), B(0)},
    {0x0002, ALL, I_ALL, 0, W, B(1), 0},
    {0x10C3, ALL, I_USB, 0, W, B(0), 0},
    {0x0005, ALL, I_ALL, 0, W, B(1), B(1)},
    {0x0005, ALL, I_ALL, 0, P, B(1), 0},
    {0x0020, ALL, I_ALL, 0, W, B(3), 0},
    {0x0000, ALL, I_USB | I_SDIO, 0, W, B(5), B(5)},
    {0xFFFF, ALL, I_ALL, 0, E, 0, 0},
};
static const rtw_pwr_cmd_t cardemu_to_carddis[] = {
    {0x0007, ALL, I_USB | I_SDIO, 0, W, 0xFF, 0x20},
    {0x0067, ALL, I_ALL, 0, W, B(5), 0},
    {0x0005, ALL, I_PCI, 0, W, B(2), B(2)},
    {0x004A, ALL, I_USB, 0, W, B(0), 0},
    {0x0067, ALL, I_SDIO, 0, W, B(5), 0},
    {0x0067, ALL, I_SDIO, 0, W, B(4), 0},
    {0x004F, ALL, I_SDIO, 0, W, B(0), 0},
    {0x0067, ALL, I_SDIO, 0, W, B(1), 0},
    {0x0046, ALL, I_SDIO, 0, W, B(6), B(6)},
    {0x0067, ALL, I_SDIO, 0, W, B(2), 0},
    {0x0046, ALL, I_SDIO, 0, W, B(7), B(7)},
    {0x0062, ALL, I_SDIO, 0, W, B(4), B(4)},
    {0x0081, ALL, I_ALL, 0, W, B(7) | B(6), 0},
    {0x0005, ALL, I_USB | I_SDIO, 0, W, B(3) | B(4), B(3)},
    {0x0086, ALL, I_SDIO, 1, W, B(0), B(0)},
    {0x0086, ALL, I_SDIO, 1, P, B(1), 0},
    {0x0090, ALL, I_USB | I_PCI, 0, W, B(1), 0},
    {0x0044, ALL, I_SDIO, 1, W, 0xFF, 0},
    {0x0040, ALL, I_SDIO, 1, W, 0xFF, 0x90},
    {0x0041, ALL, I_SDIO, 1, W, 0xFF, 0x00},
    {0x0042, ALL, I_SDIO, 1, W, 0xFF, 0x04},
    {0xFFFF, ALL, I_ALL, 0, E, 0, 0},
};
const rtw_pwr_cmd_t *const rtw8821c_pwr_on[] = {carddis_to_cardemu, cardemu_to_act, NULL};
const rtw_pwr_cmd_t *const rtw8821c_pwr_off[] = {act_to_cardemu, cardemu_to_carddis, NULL};

#define REG_SYS_PW_CTRL 0x0004
#define BIT_PFM_WOWL    0x08            /* bit 3 do byte 0x04 */

static bool poll8(rtw_io_t *io, u32 a, u8 mask, u8 want, u32 ms) {
    for (u32 i = 0; i < ms * 20; i++) {
        if ((io->r8(io, a) & mask) == (want & mask)) return true;
        io->udelay(io, 50);
    }
    return false;
}

int rtw_pwr_seq(rtw_io_t *io, const rtw_pwr_cmd_t *const *seq, u8 cut, u32 poll_ms) {
    u8 cut_mask = (u8)(1u << (cut + 1 > 7 ? 7 : cut + 1));     /* corte A (0) = bit 1, B = bit 2 ... */
    for (; *seq; seq++)
        for (const rtw_pwr_cmd_t *c = *seq; c->cmd != RTW_PWR_END; c++) {
            if (!(c->intf_mask & RTW_PWR_INTF_PCI) || !(c->cut_mask & cut_mask) || c->sdio) continue;
            switch (c->cmd) {
            case RTW_PWR_WRITE: io->w8(io, c->off, (u8)((io->r8(io, c->off) & ~c->mask) | (c->value & c->mask))); break;
            case RTW_PWR_POLL:
                if (poll8(io, c->off, c->mask, c->value, poll_ms)) break;
                /* no PCIe, alternar o PFM_WOWL destrava o estado de energia e tenta de novo */
                {
                    u8 v = io->r8(io, REG_SYS_PW_CTRL);
                    io->w8(io, REG_SYS_PW_CTRL, (u8)(v | BIT_PFM_WOWL));
                    io->w8(io, REG_SYS_PW_CTRL, (u8)(v & ~BIT_PFM_WOWL));
                }
                if (!poll8(io, c->off, c->mask, c->value, poll_ms)) return -1;
                break;
            case RTW_PWR_DELAY: io->udelay(io, c->value == RTW_PWR_DELAY_US ? c->off : c->off * 1000u); break;
            default: break;
            }
        }
    return 0;
}

/* ============================================================ eFuse
 * Cabeçalho de 1 byte: bloco[7:4] palavras[3:0]; de 2 bytes quando hdr1[4:0] == 0x0F:
 *   bloco = hdr2[7:4] << 3 | hdr1[7:5], palavras = hdr2[3:0]. Bit de palavra 0 = palavra presente.
 * Cada bloco cobre 8 bytes lógicos (4 palavras de 2 bytes). */
bool rtw_efuse_logical(const u8 *phys, u8 *log) {
    memset(log, 0xFF, RTW_EFUSE_LOG);
    u32 lim = RTW_EFUSE_PHYS - RTW_EFUSE_PROT;
    for (u32 p = 0; p + 1 < lim;) {
        u8 h1 = phys[p], h2 = phys[p + 1], blk, we;
        if (h1 == 0xFF || ((h1 & 0x1F) == 0x0F && h2 == 0xFF)) break;
        if ((h1 & 0x1F) == 0x0F) { blk = (u8)(((h2 & 0xF0) >> 1) | ((h1 >> 5) & 7)); we = h2 & 0xF; p += 2; }
        else { blk = (u8)((h1 & 0xF0) >> 4); we = h1 & 0xF; p += 1; }
        for (int i = 0; i < 4; i++) {
            if (we & (1u << i)) continue;
            u32 l = ((u32)blk << 3) + ((u32)i << 1);
            if (p + 1 > lim || l + 1 >= RTW_EFUSE_LOG) return false;
            log[l] = phys[p]; log[l + 1] = phys[p + 1];
            p += 2;
        }
    }
    return true;
}

void rtw8821c_efuse_parse(const u8 *m, rtw_efuse_t *e) {
    memset(e, 0, sizeof *e);
    memcpy(e->txpwr, m + 0x10, sizeof e->txpwr);
    e->channel_plan = m[0xB8] == 0xFF ? 0x7F : m[0xB8];
    e->xtal = m[0xB9] == 0xFF ? 0 : m[0xB9];
    e->thermal = m[0xBA];
    e->pa_type = m[0xBC] == 0xFF ? 0 : m[0xBC];
    e->lna_2g = m[0xBD] == 0xFF ? 0 : m[0xBD];
    e->lna_5g = m[0xBF] == 0xFF ? 0 : m[0xBF];
    e->rf_board = m[0xC1] == 0xFF ? 0 : m[0xC1];
    e->bt_setting = m[0xC3];
    e->swing_2g = m[0xC6] == 0xFF ? 0 : m[0xC6];
    e->swing_5g = m[0xC7] == 0xFF ? 0 : m[0xC7];
    e->rfe = m[0xCA] & 0x1F;
    e->pkg = (m[0xCA] & 0x20) ? 1 : 0;
    e->country[0] = (char)m[0xCB]; e->country[1] = (char)m[0xCC]; e->country[2] = 0;
    e->regd = e->rf_board & 7;
    e->share_ant = m[0xC3] != 0xFF && (m[0xC3] & 1);
    e->btcoex = (e->rf_board & 0xE0) == 0x20;
    switch (e->rfe) { case 2: case 4: case 7: case 0xA: case 0xC: case 0xF: e->rfe_btg = true; break; default: break; }
    /* RFE 2 e 4: a potência de 2,4 GHz do caminho A vem da tabela do caminho B */
    if (e->rfe == 2 || e->rfe == 4) memcpy(e->txpwr, m + 0x10 + 42, 18);
    memcpy(e->mac, m + 0xD0, 6);
}

/* ============================================================ firmware */
static u32 le32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u16 le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }

bool rtw_fw_parse(const u8 *d, u32 size, rtw_fw_t *fw, char *err, int errlen) {
    if (size < RTW_FW_HDR) { snprintf(err, (usize)errlen, "arquivo curto (%u bytes)", size); return false; }
    memset(fw, 0, sizeof *fw);
    fw->version = le16(d + 4); fw->sub = d[6]; fw->idx = d[7];
    fw->h2c_ver = le16(d + 0x1C);
    fw->dmem_addr = le32(d + 0x20) & ~0x80000000u;
    fw->dmem_size = le32(d + 0x24) + 8;
    fw->imem_size = le32(d + 0x30) + 8;
    fw->emem_size = (d[0x18] & 0x10) ? le32(d + 0x34) + 8 : 0;
    fw->emem_addr = le32(d + 0x38) & ~0x80000000u;
    fw->imem_addr = le32(d + 0x3C) & ~0x80000000u;
    u64 total = (u64)RTW_FW_HDR + fw->dmem_size + fw->imem_size + fw->emem_size;
    if (total != size) { snprintf(err, (usize)errlen, "tamanho %u não bate com o cabeçalho (%llu)", size, (unsigned long long)total); return false; }
    fw->dmem = d + RTW_FW_HDR;
    fw->imem = fw->dmem + fw->dmem_size;
    fw->emem = fw->emem_size ? fw->imem + fw->imem_size : NULL;
    return true;
}
