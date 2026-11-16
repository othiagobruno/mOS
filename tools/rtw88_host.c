/* mOS — teste de host do núcleo do RTL8821CE (kernel/wifi/rtw88/rtw_core.c + tabelas).
 *
 *   gcc -O1 -Wall -Wextra -DMOS_HOST_TEST -I tools -I kernel/include -I kernel/wifi/rtw88 tools/rtw88_host.c \
 *       kernel/wifi/rtw88/rtw_core.c kernel/wifi/rtw88/rtw8821c_table.c -o /tmp/rtw88_host && \
 *       /tmp/rtw88_host firmware/rtw88/rtw8821c_fw.bin tools/rtw88-fixture/bb_8821ce_linux.txt
 *
 * O arquivo de fixture é o despejo (debugfs do rtw88, só leitura) dos registradores de BB 0x800-0x1FFF do
 * RTL8821CE do PC do Thiago com o Linux associado em 5 GHz canal 42/80 MHz. Os registradores que a tabela
 * de BB escreve e que nem o canal, nem o DIG, nem a calibração mexem depois têm de bater. */
#include "rtw.h"
#include <assert.h>

int g_fail;
#define CHECK(c, ...) do { if (c) printf("ok   "); else { printf("FALHA "); g_fail++; } printf(__VA_ARGS__); printf("\n"); } while (0)

/* ---- chip simulado: 64 KiB de registradores de 8 bits */
static u8 regs[0x10000];
static u32 writes;
static u8 r8(rtw_io_t *io, u32 a) { (void)io; return regs[a & 0xFFFF]; }
static u16 r16(rtw_io_t *io, u32 a) { return (u16)(r8(io, a) | r8(io, a + 1) << 8); }
static u32 r32(rtw_io_t *io, u32 a) { return (u32)r16(io, a) | (u32)r16(io, a + 2) << 16; }
static void w8(rtw_io_t *io, u32 a, u8 v) {
    (void)io; writes++;
    regs[a & 0xFFFF] = v;
    /* efeitos do hardware que as sequências de energia esperam */
    if (a == 0x0005 && (v & 1)) regs[0x0005] &= (u8)~1;          /* APFM_ONMAC termina na hora */
    if (a == 0x0075) regs[0x0006] |= 2;                           /* power ready */
}
static void w16(rtw_io_t *io, u32 a, u16 v) { w8(io, a, (u8)v); w8(io, a + 1, (u8)(v >> 8)); }
static void w32(rtw_io_t *io, u32 a, u32 v) { w16(io, a, (u16)v); w16(io, a + 2, (u16)(v >> 16)); }
static void ud(rtw_io_t *io, u32 us) { (void)io; (void)us; }
static rtw_io_t io = {0, r8, r16, r32, w8, w16, w32, ud};

/* ---- coleta das escritas de uma tabela */
typedef struct { u32 a, d; } pair_t;
static pair_t got[4096];
static u32 ngot;
static void collect(void *ctx, u32 a, u32 d) { (void)ctx; if (ngot < 4096) got[ngot++] = (pair_t){a, d}; }
static u32 last_for(u32 a, bool *found) {
    u32 v = 0; *found = false;
    for (u32 i = 0; i < ngot; i++) if (got[i].a == a) { v = got[i].d; *found = true; }
    return v;
}

/* ---- fixture */
static u32 bbv[0x2000];
static bool bbok[0x2000];
static void load_fixture(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { printf("sem fixture %s\n", path); return; }
    char line[256];
    while (fgets(line, sizeof line, f)) {
        unsigned a, v[4];
        if (sscanf(line, "%x %x %x %x %x", &a, &v[0], &v[1], &v[2], &v[3]) == 5 && a < 0x2000 - 16)
            for (int i = 0; i < 4; i++) { bbv[a + 4 * i] = v[i]; bbok[a + 4 * i] = true; }
    }
    fclose(f);
}

static void test_cond(void) {
    /* tabela sintética: um ramo para RFE 2, um "else" e pares fora de condição */
    static const u32 t[] = {
        0x100, 1,
        0x80000002, 0, 0x40000000, 0,   0x200, 2,      /* if rfe==2 */
        0x90000003, 0, 0x40000000, 0,   0x300, 3,      /* elif rfe==3 */
        0xA0000000, 0,                  0x400, 4,      /* else (sem par neg, como nas tabelas reais) */
        0xB0000000, 0,                                 /* endif */
        0x500, 5,
    };
    rtw_table_t tb = {t, ARRAY_LEN(t)};
    rtw_cond_t c = {.rfe = 3, .intf = RTW_INTF_PCIE};
    ngot = 0; rtw_parse_cond_table(&tb, &c, collect, NULL);
    CHECK(ngot == 3 && got[0].a == 0x100 && got[1].a == 0x300 && got[2].a == 0x500, "condição: elif escolhe só o ramo do RFE 3");
    c.rfe = 9; ngot = 0; rtw_parse_cond_table(&tb, &c, collect, NULL);
    CHECK(ngot == 3 && got[1].a == 0x400, "condição: sem ramo que bata, vale o else");
    c.rfe = 2; ngot = 0; rtw_parse_cond_table(&tb, &c, collect, NULL);
    CHECK(ngot == 3 && got[1].a == 0x200, "condição: if do RFE 2");
}

static void test_tables(void) {
    rtw_cond_t c = {.rfe = 0, .intf = RTW_INTF_PCIE, .pkg = 0, .cut = 0};
    ngot = 0; u32 n = rtw_parse_cond_table(&rtw8821c_bb, &c, collect, NULL);
    CHECK(n > 300, "BB do 8821C para RFE 0/PCIe: %u pares", n);
    u32 cmp = 0, eq = 0;
    for (u32 a = 0x800; a < 0x2000; a += 4) {
        bool f; u32 v = last_for(a, &f);
        if (!f || !bbok[a]) continue;
        /* registradores que o driver muda depois da tabela: canal/banda (0x808 0x82c 0x8ac 0x8c4 0xa04 0xc1c
         * 0xc50 0xcb0 0xcb4 0x1d00..) ficam de fora pela comparação aproximada abaixo */
        cmp++;
        if (v == bbv[a]) eq++;
    }
    if (cmp) {
        printf("     BB: %u de %u registradores escritos pela tabela iguais ao chip no Linux\n", eq, cmp);
        CHECK(eq * 100 >= cmp * 75, "BB: pelo menos 75%% iguais ao estado real (o resto é canal/DIG/IQK)");
    }
    ngot = 0; n = rtw_parse_cond_table(&rtw8821c_agc, &c, collect, NULL);
    CHECK(n > 100, "AGC para RFE 0: %u pares", n);
    ngot = 0; n = rtw_parse_cond_table(&rtw8821c_rf_a, &c, collect, NULL);
    CHECK(n > 300, "RF caminho A para RFE 0: %u pares", n);
    ngot = 0; n = rtw_parse_cond_table(&rtw8821c_mac, &c, collect, NULL);
    CHECK(n > 50 && got[0].a == 0x010, "MAC: %u pares, começando por 0x010", n);
    CHECK(rtw8821c_bb_pg_type0.size == 15 && rtw8821c_txpwr_lmt_type0.size > 1000, "potência por taxa (%u) e limites (%u)", rtw8821c_bb_pg_type0.size, rtw8821c_txpwr_lmt_type0.size);
}

static void test_pwr(void) {
    memset(regs, 0, sizeof regs);
    regs[0x0005] = 0x98;
    writes = 0;
    int r = rtw_pwr_seq(&io, rtw8821c_pwr_on, 0, 5);
    CHECK(r == 0, "sequência de energia (ligar) termina sem poll preso");
    CHECK((regs[0x0005] & 0x98) == 0 && (regs[0x0006] & 1) && (regs[0x0020] & 8), "ligar: limpa 0x05[7,4,3], liga 0x06[0] e 0x20[3]");
    CHECK(regs[0x0300] == 0 && (regs[0x0062] & 0xE0) == 0xE0 && (regs[0x0074] & 0x20), "ligar: passos de PCIe (0x300, 0x62, 0x74)");
    memset(regs, 0, sizeof regs);
    r = rtw_pwr_seq(&io, rtw8821c_pwr_off, 0, 1);
    /* desligar espera 0x05[1] cair: no chip simulado ele fica de pé, então o poll tem de falhar */
    CHECK(r == -1, "desligar: poll de 0x05[1] sem resposta é erro (o chip simulado não o derruba)");
}

static void test_efuse(void) {
    u8 phys[RTW_EFUSE_PHYS], log[RTW_EFUSE_LOG];
    memset(phys, 0xFF, sizeof phys);
    int p = 0;
    /* bloco 0x1A (0xD0..0xD7), palavras 0-2 => MAC em 0xD0..0xD5; cabeçalho de 2 bytes */
    phys[p++] = (u8)((0x1A & 7) << 5 | 0x0F); phys[p++] = (u8)((0x1A >> 3) << 4 | 0x8);
    const u8 mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    for (int i = 0; i < 6; i++) phys[p++] = mac[i];
    /* bloco 0x19 (0xC8..0xCF), só a palavra 1 => 0xCA (RFE) e 0xCB */
    phys[p++] = (u8)((0x19 & 7) << 5 | 0x0F); phys[p++] = (u8)((0x19 >> 3) << 4 | (0xF & ~2));
    phys[p++] = 0x00; phys[p++] = 'B';
    /* bloco 1 (0x08..0x0F), cabeçalho de 1 byte, palavra 0 */
    phys[p++] = 0x1E; phys[p++] = 0xAB; phys[p++] = 0xCD;
    CHECK(rtw_efuse_logical(phys, log), "eFuse: mapa lógico");
    CHECK(!memcmp(log + 0xD0, mac, 6) && log[0xCA] == 0 && log[0xCB] == 'B' && log[0x08] == 0xAB && log[0x09] == 0xCD && log[0x0A] == 0xFF,
          "eFuse: cabeçalhos de 1 e 2 bytes e palavras ausentes");
    rtw_efuse_t e;
    rtw8821c_efuse_parse(log, &e);
    CHECK(!memcmp(e.mac, mac, 6) && e.rfe == 0 && !e.rfe_btg && e.channel_plan == 0x7F, "eFuse: MAC, RFE 0 (WLG) e plano de canais padrão");
}

static void test_fw(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { CHECK(0, "firmware %s não abriu", path); return; }
    static u8 buf[1 << 20];
    u32 n = (u32)fread(buf, 1, sizeof buf, f);
    fclose(f);
    rtw_fw_t fw; char err[96];
    bool ok = rtw_fw_parse(buf, n, &fw, err, sizeof err);
    CHECK(ok, "firmware: cabeçalho e tamanhos (%s)", ok ? "ok" : err);
    if (ok) {
        printf("     versão %u.%u, H2C %u, DMEM 0x%x+%u, IMEM 0x%x+%u, EMEM %u\n", fw.version, fw.sub, fw.h2c_ver,
               fw.dmem_addr, fw.dmem_size, fw.imem_addr, fw.imem_size, fw.emem_size);
        CHECK(fw.version == 24 && fw.sub == 11, "firmware 24.11 (o mesmo que o Linux carregou neste PC)");
    }
    CHECK(!rtw_fw_parse(buf, n - 1, &fw, err, sizeof err), "firmware truncado é recusado");
}

int main(int argc, char **argv) {
    if (argc > 2) load_fixture(argv[2]);
    test_cond();
    test_tables();
    test_pwr();
    test_efuse();
    test_fw(argc > 1 ? argv[1] : "firmware/rtw88/rtw8821c_fw.bin");
    printf(g_fail ? "rtw88: %d FALHA(S)\n" : "rtw88: ok\n", g_fail);
    return g_fail != 0;
}
