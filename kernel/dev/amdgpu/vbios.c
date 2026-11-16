/* mOS — VBIOS da GPU AMD: imagem pela tabela VFCT do ACPI (o GOP a deixa para o sistema), interpretador ATOM
 * ligado ao MMIO, e as tabelas de dados do formato atomfirmware (GFX9/DCN): relógios do display (dce_info),
 * caminhos de exibição (display_object_info v1.4: conector, encoder UNIPHY, linha I2C e pino de HPD) e a
 * tabela de GPIO que traduz os pinos em instâncias de DDC e HPD. Ver amdgpu.h. As funções de leitura são
 * lógica pura; tools/amdgpu_vbios_host.c as testa com um VBIOS sintético e, se houver, com o do Thiago.
 *
 * Referência dos formatos: atomfirmware.h (MIT). Pino -> instância: a tabela gpio_pin_lut dá o registrador
 * DC_GPIO_*_A e o bit de cada pino; DC_GPIO_HPD_A tem HPD1..6 nos bits 0, 8, 16, 24, 26 e 28, e
 * DC_GPIO_DDC1_A..DDC6_A ficam a cada 4 dwords a partir de 0x5D91. */
#include "amdgpu.h"
#include "regs.h"
#include "atom.h"

#define DC_GPIO_DDC1_A   0x5D91u
#define DC_GPIO_HPD_A    0x5DB5u
#define DATA_GPIO_LUT    12

/* ------------------------------------------------------------ VFCT */
int amd_vfct_find(const u8 *t, u32 len, u16 vendor, u16 device, const u8 **img, u32 *img_len) {
    if (!t || len < 0x4C || memcmp(t, "VFCT", 4)) return -1;
    u32 tl = amd_le32(t + 4);
    if (tl < len) len = tl;
    u32 off = amd_le32(t + 0x34);
    while (off + 28 <= len) {
        u32 n = amd_le32(t + off + 24);
        u16 v = amd_le16(t + off + 12), d = amd_le16(t + off + 14);
        if (!n) break;
        if (n > len - off - 28) return -2;                       /* imagem cortada */
        if (v == vendor && d == device) { *img = t + off + 28; *img_len = n; return 0; }
        off += 28 + n;
    }
    return -3;
}

/* ------------------------------------------------------------ tabelas de dados */
static int hpd_from_shift(u32 shift) {
    static const u8 sh[AMD_NUM_HPD] = {0, 8, 16, 24, 26, 28};
    for (int i = 0; i < AMD_NUM_HPD; i++) if (sh[i] == shift) return i;
    return -1;
}

/* pino da GPIO (gpio_id) -> registrador e bit, pela gpio_pin_lut */
static bool gpio_lookup(amd_atom_t *at, u8 id, u32 *reg, u32 *shift) {
    u16 size, start;
    if (!amd_atom_data_header(at, DATA_GPIO_LUT, &size, NULL, NULL, &start)) return false;
    for (u32 q = start + 4u; q + 8 <= (u32)start + size; q += 8)
        if (amd_atom_u8(at, q + 6) == id) { *reg = amd_atom_u32(at, q); *shift = amd_atom_u8(at, q + 4); return true; }
    return false;
}

int amd_vbios_parse(amd_atom_t *at, amd_vbios_info_t *v) {
    memset(v, 0, sizeof *v);
    u16 size, start;
    u8 frev, crev;
    if (amd_atom_data_header(at, ATOM_DATA_DCE_INFO, &size, &frev, &crev, &start) && frev == 4 && size >= 20) {
        v->dispclk_khz = amd_atom_u32(at, start + 8u) * 10;
        v->dce_refclk_khz = amd_atom_u16(at, start + 12u) * 10u;
        v->i2c_refclk_khz = amd_atom_u16(at, start + 14u) * 10u;
    }
    if (amd_atom_data_header(at, ATOM_DATA_FIRMWARE_INFO, &size, &frev, &crev, &start) && frev == 3)
        v->fw_revision = amd_atom_u32(at, start + 4u);
    if (!amd_atom_data_header(at, ATOM_DATA_DISPLAY_OBJECT_INFO, &size, &frev, &crev, &start)) return -1;
    if (frev != 1 || crev < 4) return -2;                       /* só o formato v1.4/1.5 do atomfirmware */
    u32 n = amd_atom_u8(at, start + 6u);
    for (u32 p = 0; p < n && v->nconn < AMD_MAX_CONN; p++) {
        u32 q = start + 8u + p * 16u;
        if (q + 16 > (u32)start + size) break;
        u16 obj = amd_atom_u16(at, q), recoff = amd_atom_u16(at, q + 2), enc = amd_atom_u16(at, q + 4);
        if (((obj >> 12) & 7) != 3) continue;                     /* só conectores (o resto é roteador/mux) */
        amd_conn_t *c = &v->conn[v->nconn];
        memset(c, 0, sizeof *c);
        c->object = obj;
        c->encoder = enc;
        c->device_tag = amd_atom_u16(at, q + 12);
        u8 id = obj & 0xFF;
        c->type = id == 0x0C || id == 0x0D ? AMD_CONN_HDMI : id == 0x13 ? AMD_CONN_DP : id == 0x14 ? AMD_CONN_EDP :
                  (id >= 0x02 && id <= 0x04) ? AMD_CONN_DVI : AMD_CONN_OTHER;
        /* UNIPHY: id 0x1E/0x20/0x21 = transmissor 0/1/2; o enum (bits 8..10) escolhe o link A (1) ou B (2) */
        u8 eid = enc & 0xFF;
        c->transmitter = eid == 0x1E ? 0 : eid == 0x20 ? 1 : eid == 0x21 ? 2 : -1;
        c->link = ((enc >> 8) & 7) == 2 ? 1 : 0;
        c->ddc = -1; c->hpd = -1;
        for (u32 r = start + (u32)recoff, guard = 0; r + 2 <= (u32)start + size && guard < 32; guard++) {
            u8 type = amd_atom_u8(at, r), len = amd_atom_u8(at, r + 1);
            if (type == 0xFF || len < 2) break;
            u32 reg, sh;
            if (type == 1 && len >= 4 && gpio_lookup(at, amd_atom_u8(at, r + 2), &reg, &sh) &&
                reg >= DC_GPIO_DDC1_A && reg <= DC_GPIO_DDC1_A + 4u * (AMD_NUM_DDC - 1) && !((reg - DC_GPIO_DDC1_A) & 3))
                c->ddc = (int)((reg - DC_GPIO_DDC1_A) / 4);
            if (type == 2 && len >= 4 && gpio_lookup(at, amd_atom_u8(at, r + 2), &reg, &sh) && reg == DC_GPIO_HPD_A)
                c->hpd = hpd_from_shift(sh);
            r += len;
        }
        v->nconn++;
    }
    return 0;
}

/* ------------------------------------------------------------ hardware */
#ifndef MOS_HOST_TEST
static u32 card_rd(void *c, u32 reg) { amdgpu_t *a = c; return (u64)reg * 4 < AMD_MMIO_SIZE ? mmio_r32(a->mmio, reg * 4) : 0; }
static void card_wr(void *c, u32 reg, u32 v) { amdgpu_t *a = c; if ((u64)reg * 4 < AMD_MMIO_SIZE) mmio_w32(a->mmio, reg * 4, v); }
static void card_delay(void *c, u32 us) { (void)c; if (us >= 1000) sleep_ms(us / 1000); for (u32 i = 0; i < us % 1000; i++) io_wait(); }
#else
static u32 card_rd(void *c, u32 reg) { (void)c; return (u64)reg * 4 < AMD_MMIO_SIZE ? amd_host_rreg(reg * 4) : 0; }
static void card_wr(void *c, u32 reg, u32 v) { (void)c; if ((u64)reg * 4 < AMD_MMIO_SIZE) amd_host_wreg(reg * 4, v); }
static void card_delay(void *c, u32 us) { (void)c; (void)us; }
#endif

bool amdgpu_vbios_init(amdgpu_t *a, const u8 *vfct, u32 len) {
    const u8 *img = NULL;
    u32 n = 0;
    if (amd_vfct_find(vfct, len, 0x1002, a->pci ? a->pci->device : 0x15D8, &img, &n)) { kprintf("amdgpu: sem VBIOS na VFCT\n"); return false; }
    memset(&a->atom, 0, sizeof a->atom);
    a->atom.card = a;
    a->atom.reg_read = card_rd; a->atom.reg_write = card_wr; a->atom.delay_us = card_delay;
    int e = amd_atom_init(&a->atom, img, n);
    if (e) { kprintf("amdgpu: VBIOS não é ATOM (%d)\n", e); return false; }
    e = amd_vbios_parse(&a->atom, &a->vbios);
    if (e) { kprintf("amdgpu: VBIOS sem caminhos de exibição (%d)\n", e); return false; }
    a->vbios_ok = true;
    kprintf("amdgpu: VBIOS de %u KiB, %u saídas, refclk %u kHz, dispclk %u kHz\n", n / 1024, a->vbios.nconn,
            a->vbios.dce_refclk_khz, a->vbios.dispclk_khz);
    for (u32 i = 0; i < a->vbios.nconn; i++) {
        const amd_conn_t *c = &a->vbios.conn[i];
        static const char *nm[] = {"?", "DP", "HDMI", "eDP", "DVI"};
        kprintf("amdgpu:   %s: UNIPHY%d link %c, DDC%d, HPD%d\n", nm[c->type], c->transmitter, 'A' + c->link, c->ddc + 1, c->hpd + 1);
    }
    return true;
}

#ifndef MOS_HOST_TEST
bool amdgpu_vbios_load(amdgpu_t *a) {
    const u8 *t = acpi_find_table("VFCT", 0);
    if (!t) { kprintf("amdgpu: ACPI sem VFCT\n"); return false; }
    return amdgpu_vbios_init(a, t, amd_le32(t + 4));
}
#endif
