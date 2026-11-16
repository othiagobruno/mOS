/* mOS — interpretador ATOM do VBIOS da AMD (atom.c). Lógica pura com callbacks de registrador. */
#pragma once
#include "amdgpu_base.h"

typedef struct amd_atom {
    const u8 *bios;
    u32       size;
    u32       rom_header, cmd_table, data_table;
    u16       iio[128];                 /* métodos de registrador indireto (SET_ATI_PORT) */
    /* acesso ao hardware: reg em dwords (índice do VBIOS); pll/mc/delay podem ser NULL */
    void     *card;
    u32     (*reg_read)(void *card, u32 reg);
    void    (*reg_write)(void *card, u32 reg, u32 val);
    u32     (*pll_read)(void *card, u32 reg);
    void    (*pll_write)(void *card, u32 reg, u32 val);
    u32     (*mc_read)(void *card, u32 reg);
    void    (*mc_write)(void *card, u32 reg, u32 val);
    void    (*delay_us)(void *card, u32 us);
    /* estado da execução */
    u32       data_block, reg_block, fb_base, io_mode, io_attr, shift, divmul[2];
    bool      cs_equal, cs_above;
    u32       scratch[5120];           /* janela FB (scratch do ATOM): 20 KiB, o padrão do amdgpu sem vram_usagebyfirmware */
    int       depth;
    u64       steps;
    u64       errors, bad_reads;
    u32       last_error;               /* código do último erro (atom.c) */
} amd_atom_t;

/* 0 = imagem ATOM válida (0xAA55, " 761295520", "ATOM"); < 0 = não é um VBIOS ATOM */
int  amd_atom_init(amd_atom_t *a, const u8 *bios, u32 size);
/* executa a tabela de comando index com os parâmetros (params_size em dwords). 0 ou < 0 */
int  amd_atom_execute(amd_atom_t *a, int index, u32 *params, int params_size);
bool amd_atom_data_header(amd_atom_t *a, int index, u16 *size, u8 *frev, u8 *crev, u16 *start);
bool amd_atom_cmd_header(amd_atom_t *a, int index, u8 *frev, u8 *crev);
u8   amd_atom_u8(amd_atom_t *a, u32 off);
u16  amd_atom_u16(amd_atom_t *a, u32 off);
u32  amd_atom_u32(amd_atom_t *a, u32 off);

/* índices nas listas mestras v2.1 (atomfirmware.h: o formato dos VBIOS de GFX9/DCN, Raven incluída) */
#define ATOM_DATA_FIRMWARE_INFO        4
#define ATOM_DATA_SMU_INFO             8
#define ATOM_DATA_DISPLAY_OBJECT_INFO  22
#define ATOM_DATA_DCE_INFO             27
#define ATOM_DATA_INTEGRATED_SYS_INFO  30
#define ATOM_CMD_ASIC_INIT             0
#define ATOM_CMD_DIGX_ENCODER_CONTROL  4
#define ATOM_CMD_SET_PIXEL_CLOCK       12
#define ATOM_CMD_ENABLE_DISP_PG        13
#define ATOM_CMD_SET_DCE_CLOCK         46
#define ATOM_CMD_DIG1_TRANSMITTER      76
