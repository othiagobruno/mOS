/* mOS — teste de host da leitura do VBIOS (kernel/dev/amdgpu/vbios.c): VFCT e tabelas atomfirmware.
 * Um VBIOS sintético com o mesmo desenho de saídas do PC do Thiago sempre roda. Se a variável MOS_VFCT apontar
 * para uma cópia da tabela VFCT (sudo cat /sys/firmware/acpi/tables/VFCT), confere também a imagem real
 * (ela não entra no repositório: é binário da AMD).
 * Rodar da raiz:
 *   gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/dev/amdgpu -I tools tools/amdgpu_vbios_host.c kernel/dev/amdgpu/vbios.c \
 *       kernel/dev/amdgpu/atom.c -o /tmp/vbios && /tmp/vbios */
#include "amdgpu.h"
#include <stdlib.h>

static int pass, total;
#define CHECK(c, ...) do { total++; if (c) pass++; else { printf("FALHOU %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* o que vbios.c usa do resto do driver */
u32 amd_host_rreg(u32 off) { (void)off; return 0; }
void amd_host_wreg(u32 off, u32 v) { (void)off; (void)v; }
void kprintf(const char *fmt, ...) { (void)fmt; }

static u8 img[0x2000];
static void w8(u32 o, u32 v) { img[o] = (u8)v; }
static void w16(u32 o, u32 v) { w8(o, v); w8(o + 1, v >> 8); }
static void w32(u32 o, u32 v) { w16(o, v); w16(o + 2, v >> 16); }

/* imagem com dce_info v4.2, display_object_info v1.4 (DP, HDMI, DP, DP e um mux) e gpio_pin_lut */
static u32 build_image(void) {
    memset(img, 0, sizeof img);
    w16(0, 0xAA55);
    memcpy(img + 0x30, " 761295520", 10);
    w16(0x48, 0x100);
    memcpy(img + 0x104, "ATOM", 4);
    w16(0x100 + 0x1E, 0x200);
    w16(0x100 + 0x20, 0x300);
    /* dce_info (27) */
    w16(0x300 + 4 + 2 * 27, 0x400);
    w16(0x400, 56); w8(0x402, 4); w8(0x403, 2);
    w32(0x404, 4); w32(0x408, 60000); w16(0x40C, 4800); w16(0x40E, 2400);
    /* firmware_info (4) */
    w16(0x300 + 4 + 2 * 4, 0x480);
    w16(0x480, 72); w8(0x482, 3); w8(0x483, 1); w32(0x484, 0x00120203);
    /* gpio_pin_lut (12): HPD1..6 em 0x5DB5, DDC 0x90..0x95 a cada 4 a partir de 0x5D91 */
    w16(0x300 + 4 + 2 * 12, 0x500);
    static const u8 hsh[6] = {0, 8, 16, 24, 26, 28};
    u32 q = 0x504;
    for (int i = 0; i < 6; i++, q += 8) { w32(q, 0x5DB5); w8(q + 4, hsh[i]); w8(q + 5, hsh[i]); w8(q + 6, 1 + i); }
    for (int i = 0; i < 6; i++, q += 8) { w32(q, 0x5D91 + 4u * i); w8(q + 4, 8); w8(q + 5, 8); w8(q + 6, 0x90 + i); }
    w16(0x500, q - 0x500); w8(0x502, 2); w8(0x503, 1);
    /* display_object_info (22) */
    u32 t = 0x600;
    w16(0x300 + 4 + 2 * 22, t);
    w8(t + 2, 1); w8(t + 3, 4); w16(t + 4, 0x688); w8(t + 6, 5);
    static const u16 obj[5] = {0x3113, 0x340C, 0x3213, 0x3313, 0x7103}, enc[5] = {0x211E, 0x221E, 0x2120, 0x2220, 0};
    u32 rec = 0x58;
    for (int p = 0; p < 5; p++) {
        u32 e = t + 8 + 16u * p;
        w16(e, obj[p]); w16(e + 2, rec); w16(e + 4, enc[p]); w16(e + 12, 0x8u << p);
        u32 r = t + rec;
        if (p < 4) { w8(r, 1); w8(r + 1, 4); w8(r + 2, 0x90 + p); r += 4; w8(r, 2); w8(r + 1, 4); w8(r + 2, 1 + p); r += 4; }
        else { w8(r, 16); w8(r + 1, 10); r += 10; }
        w8(r, 0xFF); w8(r + 1, 0);
        rec = r + 2 - t;
    }
    w16(t, rec);
    return 0x800;
}

/* VFCT com a imagem; dev escolhe o DeviceID gravado */
static u8 *make_vfct(const u8 *im, u32 n, u16 dev, u32 *len) {
    *len = 0x4C + 28 + n + 28;
    u8 *t = calloc(1, *len);
    memcpy(t, "VFCT", 4);
    memcpy(t + 4, len, 4);
    u32 o = 0x4C;
    memcpy(t + 0x34, &o, 4);
    t[o + 4] = 0; t[o + 12] = 0x02; t[o + 13] = 0x10; t[o + 14] = (u8)dev; t[o + 15] = (u8)(dev >> 8);
    memcpy(t + o + 24, &n, 4);
    memcpy(t + o + 28, im, n);
    return t;
}

static void check_thiago(const amd_vbios_info_t *v, const char *what) {
    CHECK(v->nconn == 4, "%s: 4 saídas (%u)", what, v->nconn);
    CHECK(v->conn[0].type == AMD_CONN_DP && v->conn[1].type == AMD_CONN_HDMI && v->conn[2].type == AMD_CONN_DP && v->conn[3].type == AMD_CONN_DP, "%s: DP, HDMI, DP, DP", what);
    CHECK(v->conn[1].ddc == 1 && v->conn[1].hpd == 1 && v->conn[1].transmitter == 0 && v->conn[1].link == 1, "%s: HDMI no DDC2/HPD2, UNIPHY0 link B (ddc %d hpd %d tx %d link %d)",
          what, v->conn[1].ddc, v->conn[1].hpd, v->conn[1].transmitter, v->conn[1].link);
    CHECK(v->conn[0].ddc == 0 && v->conn[0].hpd == 0 && v->conn[2].transmitter == 1 && v->conn[2].link == 0 && v->conn[3].transmitter == 1 &&
          v->conn[3].link == 1 && v->conn[3].ddc == 3 && v->conn[3].hpd == 3, "%s: os três DP", what);
    CHECK(v->dce_refclk_khz == 48000 && v->i2c_refclk_khz == 24000 && v->dispclk_khz == 600000, "%s: relógios %u %u %u", what, v->dce_refclk_khz, v->i2c_refclk_khz, v->dispclk_khz);
}

int main(void) {
    u32 n = build_image(), len = 0;
    u8 *vf = make_vfct(img, n, 0x15D8, &len);
    const u8 *im = NULL;
    u32 il = 0;
    CHECK(!amd_vfct_find(vf, len, 0x1002, 0x15D8, &im, &il) && im == vf + 0x4C + 28 && il == n, "VFCT acha a imagem");
    CHECK(amd_vfct_find(vf, len, 0x1002, 0x1636, &im, &il) == -3, "outro dispositivo não casa");
    CHECK(amd_vfct_find(vf, 0x40, 0x1002, 0x15D8, &im, &il) == -1, "tabela curta");
    u32 big = 0x7FFFFFF0u;
    memcpy(vf + 0x4C + 24, &big, 4);
    CHECK(amd_vfct_find(vf, len, 0x1002, 0x15D8, &im, &il) == -2, "imagem cortada");
    memcpy(vf + 0x4C + 24, &n, 4);

    static amdgpu_t a;
    memset(&a, 0, sizeof a);
    CHECK(amdgpu_vbios_init(&a, vf, len) && a.vbios_ok, "VBIOS sintético");
    check_thiago(&a.vbios, "sintético");
    CHECK(a.vbios.fw_revision == 0x00120203 && a.vbios.conn[4].type == 0, "revisão do firmware e o mux fica de fora");
    CHECK(a.vbios.mem_channels == 0, "sem integratedsysteminfo no sintético: canais ficam 0 (o driver usa 2)");
    /* registro sem terminador e lut sem o pino: sem DDC/HPD, sem estourar */
    w8(0x600 + 0x58 + 8, 0x07);
    w8(0x504 + 6, 0x77);
    amd_atom_t at; amd_vbios_info_t v;
    memset(&at, 0, sizeof at);
    CHECK(!amd_atom_init(&at, img, n) && !amd_vbios_parse(&at, &v) && v.conn[0].hpd == -1 && v.conn[0].ddc == 0, "pino que não está na tabela de GPIO");
    w16(0x300 + 4 + 2 * 22, 0);
    CHECK(amd_vbios_parse(&at, &v) == -1, "sem display_object_info");
    free(vf);

    const char *path = getenv("MOS_VFCT");
    if (path) {
        FILE *f = fopen(path, "rb");
        static u8 real[1 << 17];
        u32 rn = f ? (u32)fread(real, 1, sizeof real, f) : 0;
        if (f) fclose(f);
        static amdgpu_t r;
        memset(&r, 0, sizeof r);
        CHECK(rn > 0x4C && amdgpu_vbios_init(&r, real, rn), "VBIOS real (%u bytes)", rn);
        check_thiago(&r.vbios, "real");
        CHECK(r.vbios.mem_type == 26 && r.vbios.mem_channels == 2, "integratedsysteminfo real: DDR4 em 2 canais (%u %u)", r.vbios.mem_type, r.vbios.mem_channels);
        u8 fr, cr;
        CHECK(amd_atom_cmd_header(&r.atom, ATOM_CMD_SET_PIXEL_CLOCK, &fr, &cr) && fr == 1 && cr == 7 &&
              amd_atom_cmd_header(&r.atom, ATOM_CMD_DIG1_TRANSMITTER, &fr, &cr) && cr == 6 &&
              amd_atom_cmd_header(&r.atom, ATOM_CMD_DIGX_ENCODER_CONTROL, &fr, &cr) && cr == 5, "tabelas de comando do VBIOS real");
        CHECK(r.atom.bad_reads == 0, "nenhuma leitura fora da imagem");
    } else printf("(sem MOS_VFCT: só o VBIOS sintético)\n");

    printf("amdgpu-vbios-host: %d/%d ok\n", pass, total);
    return pass == total ? 0 : 1;
}
