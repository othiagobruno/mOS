/*
 * mOS — driver Realtek RTL8821CE (PCIe 10ec:c821): tipos comuns.
 *
 * Camadas (docs/rede-hardware.md):
 *   rtw_core.c   tabelas condicionais de MAC/BB/AGC/RF, sequências de energia, eFuse, firmware
 *   rtw8821c_table.c  dados da Realtek (BSD-3-Clause)
 *
 * O acesso aos registradores passa por rtw_io_t para o mesmo código rodar no kernel (MMIO do BAR2)
 * e no teste de host (tools/rtw88_host.c), que simula o chip.
 */
#pragma once
#ifdef MOS_HOST_TEST
#include "host_shim.h"
typedef struct pci_dev pci_dev_t;          /* o teste de host não usa o PCI */
u64 timer_ticks(void);
#else
#include "kernel.h"
#endif

/* ---------------- acesso a registradores */
typedef struct rtw_io {
    void *ctx;
    u8   (*r8)(struct rtw_io *io, u32 a);
    u16  (*r16)(struct rtw_io *io, u32 a);
    u32  (*r32)(struct rtw_io *io, u32 a);
    void (*w8)(struct rtw_io *io, u32 a, u8 v);
    void (*w16)(struct rtw_io *io, u32 a, u16 v);
    void (*w32)(struct rtw_io *io, u32 a, u32 v);
    void (*udelay)(struct rtw_io *io, u32 us);
} rtw_io_t;

static inline u32 rtw_ffs(u32 m) { return m ? (u32)__builtin_ctz(m) : 0; }
static inline u32 rtw_r32_mask(rtw_io_t *io, u32 a, u32 m) { return (io->r32(io, a) & m) >> rtw_ffs(m); }
static inline void rtw_w32_mask(rtw_io_t *io, u32 a, u32 m, u32 v) {
    if (m == 0xFFFFFFFFu) { io->w32(io, a, v); return; }
    u32 s = rtw_ffs(m);
    io->w32(io, a, (io->r32(io, a) & ~m) | ((v << s) & m));
}
static inline void rtw_w8_set(rtw_io_t *io, u32 a, u8 b) { io->w8(io, a, (u8)(io->r8(io, a) | b)); }
static inline void rtw_w8_clr(rtw_io_t *io, u32 a, u8 b) { io->w8(io, a, (u8)(io->r8(io, a) & ~b)); }
static inline void rtw_w32_set(rtw_io_t *io, u32 a, u32 b) { io->w32(io, a, io->r32(io, a) | b); }
static inline void rtw_w32_clr(rtw_io_t *io, u32 a, u32 b) { io->w32(io, a, io->r32(io, a) & ~b); }

/* ---------------- tabelas da Realtek */
typedef struct { const u32 *data; u32 size; } rtw_table_t;                       /* pares (endereço, valor) */
typedef struct { u32 band, rf_path, tx_num, addr, bitmask, data; } rtw_pg_t;     /* potência por taxa */
typedef struct { u8 regd, band, bw, rs, ch; i8 lmt; } rtw_lmt_t;                  /* limite regulatório */
typedef struct { const rtw_pg_t *data; u32 size; } rtw_pg_table_t;
typedef struct { const rtw_lmt_t *data; u32 size; } rtw_lmt_table_t;

extern const rtw_table_t rtw8821c_mac, rtw8821c_agc, rtw8821c_agc_btg_type2, rtw8821c_bb, rtw8821c_rf_a;
extern const rtw_pg_table_t rtw8821c_bb_pg_type0;
extern const rtw_lmt_table_t rtw8821c_txpwr_lmt_type0;

/* condição do chip usada pelas tabelas (campos do cabeçalho "if" das tabelas) */
#define RTW_INTF_PCIE 1
typedef struct { u8 rfe, intf, pkg, cut; } rtw_cond_t;

/* percorre uma tabela condicional e chama cfg(addr, data) para os pares que valem para este chip;
 * devolve quantos pares aplicou */
typedef void (*rtw_cfg_fn)(void *ctx, u32 addr, u32 data);
u32 rtw_parse_cond_table(const rtw_table_t *t, const rtw_cond_t *c, rtw_cfg_fn cfg, void *ctx);

/* ---------------- sequências de energia (offset, máscaras de corte e interface, comando) */
enum { RTW_PWR_READ, RTW_PWR_WRITE, RTW_PWR_POLL, RTW_PWR_DELAY, RTW_PWR_END };
#define RTW_PWR_INTF_SDIO 0x01
#define RTW_PWR_INTF_USB  0x02
#define RTW_PWR_INTF_PCI  0x04
#define RTW_PWR_INTF_ALL  0x0F
#define RTW_PWR_DELAY_US  0
#define RTW_PWR_DELAY_MS  1
typedef struct { u16 off; u8 cut_mask, intf_mask, sdio, cmd, mask, value; } rtw_pwr_cmd_t;
extern const rtw_pwr_cmd_t *const rtw8821c_pwr_on[], *const rtw8821c_pwr_off[];
/* executa as sequências na ordem; 0 = ok, -1 = algum POLL não chegou ao valor (polls_ms cada) */
int rtw_pwr_seq(rtw_io_t *io, const rtw_pwr_cmd_t *const *seq, u8 cut, u32 poll_ms);

/* ---------------- eFuse: mapa físico (512 B, 96 protegidos) -> mapa lógico (512 B) */
#define RTW_EFUSE_PHYS 512
#define RTW_EFUSE_PROT 96
#define RTW_EFUSE_LOG  512
bool rtw_efuse_logical(const u8 *phys, u8 *log);

/* campos do mapa lógico do 8821CE que o driver usa */
typedef struct {
    u8 mac[6];
    u8 rfe, xtal, thermal, pa_type, lna_2g, lna_5g, channel_plan, rf_board, bt_setting;
    u8 swing_2g, swing_5g, regd;
    char country[3];
    bool rfe_btg, share_ant, btcoex;
    u8 pkg;
    u8 txpwr[42];          /* tabela de potência do caminho A (struct rtw_txpwr_idx da Realtek, 0x10..0x39) */
} rtw_efuse_t;
void rtw8821c_efuse_parse(const u8 *log, rtw_efuse_t *e);

/* ---------------- firmware rtw88/rtw8821c_fw.bin */
typedef struct {
    u16 version; u8 sub, idx;
    u16 h2c_ver;
    u32 dmem_addr, dmem_size, imem_addr, imem_size, emem_addr, emem_size;   /* tamanhos com os 8 B de checksum */
    const u8 *dmem, *imem, *emem;
} rtw_fw_t;
#define RTW_FW_HDR 64
bool rtw_fw_parse(const u8 *data, u32 size, rtw_fw_t *fw, char *err, int errlen);

/* ---------------- potência de transmissão (rtw_pwr.c) */
enum { RTW_RS_CCK, RTW_RS_OFDM, RTW_RS_HT1, RTW_RS_HT2, RTW_RS_VHT1, RTW_RS_VHT2, RTW_RS_NUM };
#define RTW_CH5G 49
#define RTW_BW_N 3                       /* 20, 40, 80 MHz */
enum { RTW_REGD_FCC = 0, RTW_REGD_MKK = 1, RTW_REGD_ETSI = 2 };
typedef struct {
    i8 byr2[0x40], byr5[0x40];           /* deslocamento por taxa, relativo à referência da seção */
    i8 base2[RTW_RS_NUM], base5[RTW_RS_NUM];
    i8 lmt2[RTW_BW_N][RTW_RS_NUM][14];   /* limite relativo à mesma referência */
    i8 lmt5[RTW_BW_N][RTW_RS_NUM][RTW_CH5G];
} rtw_pwr_t;
extern const u8 *const rtw_rate_sec[RTW_RS_NUM];
extern const u8 rtw_rate_sec_n[RTW_RS_NUM];
int  rtw_ch_idx(int band, int ch);                  /* band 0 = 2,4 GHz, 1 = 5 GHz */
int  rtw_rate_section(u8 rate);
int  rtw_ch_group(int ch, u8 rate);
void rtw_pwr_init(rtw_pwr_t *p, const rtw_pg_table_t *pg, const rtw_lmt_table_t *lmt, int regd);
i8   rtw_pwr_limit(const rtw_pwr_t *p, int band, int bw, u8 rate, int center, int primary);
u8   rtw_pwr_base(const u8 *efuse_txpwr, int band, int bw, u8 rate, int ch);
u8   rtw_pwr_index(const rtw_pwr_t *p, const u8 *efuse_txpwr, int band, int bw, u8 rate, int center, int primary);

/* ---------------- driver (rtw8821c.c) */
bool rtw8821c_attach(rtw_io_t *io, pci_dev_t *pci);
