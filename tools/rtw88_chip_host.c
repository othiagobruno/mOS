/* mOS — teste de host do driver RTL8821CE inteiro (kernel/wifi/rtw88/rtw8821c.c) contra um chip simulado.
 *
 *   gcc -O1 -Wall -Wextra -DMOS_HOST_TEST -I tools -I kernel/include -I kernel/wifi/rtw88 tools/rtw88_chip_host.c \
 *       kernel/wifi/rtw88/rtw8821c.c kernel/wifi/rtw88/rtw_core.c kernel/wifi/rtw88/rtw_pwr.c \
 *       kernel/wifi/rtw88/rtw8821c_table.c -o /tmp/rtw88_chip && /tmp/rtw88_chip firmware/rtw88/rtw8821c_fw.bin
 *
 * O chip simulado faz o que o driver espera do hardware: sequência de energia, bloco de coexistência indireto,
 * DMA da fila de beacon para o buffer de TX, DDMA para a IMEM/DMEM/EMEM da CPU do Wi-Fi, "firmware pronto",
 * LLT, eFuse, RF pela interface serial, caixas de H2C, anéis de TX e RX. O teste confere o que chegou à
 * memória da CPU (o firmware inteiro, byte a byte), a potência gravada para o canal 42/80 MHz (a tabela do
 * Linux neste PC), os comandos H2C da associação e um quadro de ida e volta pelos anéis. */
#include "rtw.h"
#include "wifi.h"

long long g_live_allocs, g_live_bytes;
uint32_t rand32(void) { return 0x12345678; }
static u64 ticks;
u64 timer_ticks(void) { return ticks; }

int g_fail;
#define CHECK(c, ...) do { if (c) printf("ok   "); else { printf("FALHA "); g_fail++; } printf(__VA_ARGS__); printf("\n"); } while (0)

void rtw8821c_set_fw(const rtw_fw_t *fw);
const rtw_efuse_t *rtw8821c_efuse(void);
int rtw8821c_state(int *band, int *primary, int *center, int *bw);
bool rtw8821c_ps_broken(void);

/* ---------------- memória de DMA: endereços "físicos" de 32 bits */
typedef struct { u8 *p; u32 pa, n; } dmab_t;
static dmab_t dm[1024];
static int ndm;
static u32 next_pa = 0x10000000;
void *rtw_host_dma(usize n, u64 *pa) {
    u8 *p = calloc(1, n);
    dm[ndm].p = p; dm[ndm].pa = next_pa; dm[ndm].n = (u32)n; ndm++;
    *pa = next_pa;
    next_pa += (u32)((n + 4095) & ~4095ull);
    return p;
}
static u8 *pa2va(u32 pa) {
    for (int i = 0; i < ndm; i++) if (pa >= dm[i].pa && pa < dm[i].pa + dm[i].n) return dm[i].p + (pa - dm[i].pa);
    return NULL;
}

/* ---------------- o chip */
static u8 R[0x10000];
static u32 lte[0x100];
static u8 txff[65536];                 /* FIFO de TX do chip */
static u8 *cpumem_dmem, *cpumem_imem, *cpumem_emem;
static u32 dmem_base, imem_base, emem_base;
static u32 rf[256];
static u8 efuse[512];
static u32 hw_tx_rp[8];                /* índice de leitura do hardware por fila (0x3A0..) */
static u32 rx_hw_wp;
static struct { u8 b[8]; } h2c_log[64];
static int nh2c;
static u8 h2c_pkt_log[16][32];
static int nh2cpkt;
static u8 mgmt_out[2400]; static int mgmt_len; static u8 mgmt_desc[48];
static int lps_stuck;                  /* 1: o firmware não restaura o TCR ao sair do LPS (o defeito do Linux) */

static u32 rd32raw(u32 a) { return (u32)R[a] | (u32)R[a + 1] << 8 | (u32)R[a + 2] << 16 | (u32)R[a + 3] << 24; }
static void wr32raw(u32 a, u32 v) { R[a] = (u8)v; R[a + 1] = (u8)(v >> 8); R[a + 2] = (u8)(v >> 16); R[a + 3] = (u8)(v >> 24); }

static u8 c_r8(rtw_io_t *io, u32 a) {
    (void)io;
    a &= 0xFFFF;
    if (a >= 0x1700 && a < 0x1704) { u32 v = rd32raw(0x1700) | (1u << 29); return (u8)(v >> (8 * (a - 0x1700))); }
    if (a >= 0x3B4 && a < 0x3B8) { u32 v = (rx_hw_wp << 16) | (rd32raw(0x3B4) & 0xFFFF); return (u8)(v >> (8 * (a - 0x3B4))); }
    for (int q = 0; q < 6; q++) {
        static const u32 idx[6] = {0x3AC, 0x3A8, 0x3A4, 0x3A0, 0x3B0, 0x3B8};
        if (a >= idx[q] && a < idx[q] + 4) { u32 v = (hw_tx_rp[q] << 16) | (rd32raw(idx[q]) & 0xFFFF); return (u8)(v >> (8 * (a - idx[q]))); }
    }
    return R[a];
}
static u16 c_r16(rtw_io_t *io, u32 a) { return (u16)(c_r8(io, a) | c_r8(io, a + 1) << 8); }
static u32 c_r32(rtw_io_t *io, u32 a) { return (u32)c_r16(io, a) | (u32)c_r16(io, a + 2) << 16; }

static void on_write(u32 a);
static void c_w8(rtw_io_t *io, u32 a, u8 v) { (void)io; a &= 0xFFFF; R[a] = v; on_write(a); }
static void c_w16(rtw_io_t *io, u32 a, u16 v) {
    (void)io; a &= 0xFFFF;
    if (a == 0x0204) {                                  /* FIFOPAGE_CTRL_2: bit 15 escrito = limpa "válido" */
        R[a] = (u8)v; R[a + 1] = (u8)((v >> 8) & 0x7F);
        return;
    }
    R[a] = (u8)v; R[a + 1] = (u8)(v >> 8); on_write(a);
}
static void c_w32(rtw_io_t *io, u32 a, u32 v) { (void)io; a &= 0xFFFF; wr32raw(a, v); on_write(a); }
static void c_ud(rtw_io_t *io, u32 us) { (void)io; ticks += us / 1000; }
static rtw_io_t io = {0, c_r8, c_r16, c_r32, c_w8, c_w16, c_w32, c_ud};

static u8 *cpu_mem(u32 da, u32 *room) {
    if (da >= dmem_base && da < dmem_base + 0x10000) { *room = dmem_base + 0x10000 - da; return cpumem_dmem + (da - dmem_base); }
    if (da >= imem_base && da < imem_base + 0x20000) { *room = imem_base + 0x20000 - da; return cpumem_imem + (da - imem_base); }
    if (emem_base && da >= emem_base && da < emem_base + 0x20000) { *room = emem_base + 0x20000 - da; return cpumem_emem + (da - emem_base); }
    return NULL;
}

static void on_write(u32 a) {
    /* sequência de energia */
    if (a == 0x0005 && (R[5] & 1)) { R[5] &= (u8)~1; if (R[0x100] == 0xEA) R[0x100] = 0; }
    if (a == 0x0005 && (R[5] & 2)) { R[5] &= (u8)~2; R[0x100] = 0xEA; }
    if (a == 0x0075 || a == 0x0074) R[6] |= 2;
    /* coex indireto */
    if (a == 0x1700 || a == 0x1702 || a == 0x1703) {
        u32 c = rd32raw(0x1700);
        if ((c >> 28) == 0xC) lte[c & 0xFF] = rd32raw(0x1704);
        else if ((c >> 28) == 0x8) wr32raw(0x1708, lte[c & 0xFF]);
    }
    /* beacon: DMA do pacote para a FIFO de TX na página pedida */
    if (a == 0x383 && (R[0x383] & 0x10)) {
        u32 bdpa = rd32raw(0x308);
        u8 *bd = pa2va(bdpa);
        u32 pa = (u32)bd[4] | (u32)bd[5] << 8 | (u32)bd[6] << 16 | (u32)bd[7] << 24;
        u16 dlen = (u16)(bd[8] | bd[9] << 8);
        u8 *src = pa2va(pa);
        u32 pg = (R[0x204] | R[0x205] << 8) & 0xFFF;
        memcpy(txff + pg * 128, src, 48 + dlen);
        R[0x205] |= 0x80;                                /* válido */
        R[0x383] &= (u8)~0x10;
    }
    /* DDMA da FIFO de TX para a memória da CPU */
    if (a == 0x1208 && (rd32raw(0x1208) & 0x80000000u)) {
        u32 c = rd32raw(0x1208), sa = rd32raw(0x1200), da = rd32raw(0x1204), n = c & 0x3FFFF;
        u32 room = 0;
        u8 *dst = cpu_mem(da, &room);
        if (dst && n <= room && sa >= 0x18780000u) memcpy(dst, txff + (sa - 0x18780000u), n);
        wr32raw(0x1208, c & ~0x80000000u & ~(1u << 27));
    }
    /* firmware: DW_RDY com a CPU ligada -> INIT_RDY */
    if ((a == 0x0080 || a == 0x0081) && (R[0x81] & 0x40)) R[0x81] |= 0x80;
    if (a == 0x0003 && (R[3] & 4) && (R[0x81] & 0x40)) R[0x81] |= 0x80;
    /* LLT */
    if (a == 0x0208) R[0x208] &= (u8)~1;
    /* eFuse */
    if (a == 0x0030 || a == 0x0033) {
        u32 c = rd32raw(0x30);
        if (!(c & 0x80000000u)) { u32 ad = (c >> 8) & 0x3FF; wr32raw(0x30, (c & ~0xFFu) | efuse[ad & 511] | 0x80000000u); }
    }
    /* RF pela SIPI */
    if (a == 0x0C90) {
        u32 v = rd32raw(0xC90), ad = (v >> 20) & 0xFF;
        rf[ad] = v & 0xFFFFF;
        wr32raw(0x2800 + ad * 4, rf[ad]);
    }
    /* caixas de H2C: o "firmware" lê na hora */
    if (a >= 0x01D0 && a < 0x01E0 && ((a - 0x1D0) & 3) == 0) {
        int box = (int)(a - 0x1D0) / 4;
        if (nh2c < 64) { for (int i = 0; i < 4; i++) h2c_log[nh2c].b[i] = R[a + i]; for (int i = 0; i < 4; i++) h2c_log[nh2c].b[4 + i] = R[0x1F0 + 4 * box + i]; nh2c++; }
        u8 id = R[a];
        if (id == 0x20) {                                 /* SET_PWR_MODE */
            if (R[a + 1]) R[0x604] |= 0x80;               /* LPS: o firmware liga PWRMGT_HWDATA_EN */
            else if (!lps_stuck) R[0x604] &= (u8)~0x80;
        }
    }
    /* anéis de TX: o host avançou o índice -> o hardware "transmite" na hora */
    static const u32 idx[7] = {0x3AC, 0x3A8, 0x3A4, 0x3A0, 0x3B0, 0x3B8, 0x132C};
    static const u32 desa[7] = {0x330, 0x328, 0x320, 0x318, 0x310, 0x340, 0x1320};
    for (int q = 0; q < 7; q++)
        if (a == idx[q]) {
            u32 host = (R[a] | R[a + 1] << 8) & 0xFFF;
            u32 num = q == 6 ? (R[0x1328] | R[0x1329] << 8) : (R[0x380 + (q == 4 ? 0 : q == 0 ? 0xA : q == 1 ? 8 : q == 2 ? 6 : q == 3 ? 4 : 0xC)] | 0);
            u32 prev = (host + num - 1) % (num ? num : 1);
            u8 *bd = pa2va(rd32raw(desa[q])) + 16 * prev;
            u32 pa = (u32)bd[4] | (u32)bd[5] << 8 | (u32)bd[6] << 16 | (u32)bd[7] << 24;
            u16 dlen = (u16)(bd[8] | bd[9] << 8);
            u8 *buf = pa2va(pa);
            if (q == 6 && nh2cpkt < 16) {
                memcpy(h2c_pkt_log[nh2cpkt++], buf + 48, 32);
                if (buf[48 + 2] == 0x0E) { rf[8] = 0xABCDE; wr32raw(0x2800 + 8 * 4, 0xABCDE); }   /* IQK pronto */
            }
            if (q == 4) { memcpy(mgmt_desc, buf, 48); memcpy(mgmt_out, buf + 48, dlen); mgmt_len = dlen; }
            if (q < 6) hw_tx_rp[q] = host;
        }
}

/* injeta um quadro recebido no próximo descritor de RX */
static void rx_inject(const u8 *f, int len, u8 rate, int pwdb, bool c2h) {
    u32 desa = rd32raw(0x338);
    u32 num = R[0x382] | R[0x383 - 1 + 1] << 0;
    num = 64;
    u8 *bd = pa2va(desa) + 8 * rx_hw_wp;
    u32 pa = (u32)bd[4] | (u32)bd[5] << 8 | (u32)bd[6] << 16 | (u32)bd[7] << 24;
    static u16 tag;
    bd[2] = (u8)tag; bd[3] = (u8)(tag >> 8); tag = (u16)((tag + 1) % 8192);
    u8 *b = pa2va(pa);
    memset(b, 0, 64);
    u32 plen = (u32)len + (c2h ? 0 : 4);
    u32 w0 = plen | (c2h ? 0 : (1u << 16) | (1u << 26));    /* drv_info 8 B + phystatus */
    wr32raw(0, 0);
    b[0] = (u8)w0; b[1] = (u8)(w0 >> 8); b[2] = (u8)(w0 >> 16); b[3] = (u8)(w0 >> 24);
    if (c2h) b[11] = 0x10;                                   /* w2 bit 28 */
    b[12] = rate;
    u8 *ps = b + 24;
    if (!c2h) { ps[0] = 1; ps[1] = (u8)(pwdb + 110); memcpy(b + 24 + 8, f, len); memset(b + 24 + 8 + len, 0xEE, 4); }
    else memcpy(b + 24, f, len);
    rx_hw_wp = (rx_hw_wp + 1) % num;
}

/* ---------------- a pilha (o mínimo): guarda o rádio */
static wifi_radio_t *radio;
void wifi_register_radio(wifi_radio_t *r) { radio = r; }
static int lost_calls, pm_regs;
void wifi_radio_lost(void) { lost_calls++; }
typedef struct { const char *name; u32 flags; int (*suspend)(int); void (*resume)(int); const char *(*can_s3)(void); } pm_ops_t;
void pm_register(const pm_ops_t *o) { if (o && (o->flags & 2)) pm_regs++; }
void rtw8821c_pm(int phase);

static u8 *load(const char *path, u32 *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    static u8 buf[1 << 20];
    *n = (u32)fread(buf, 1, sizeof buf, f);
    fclose(f);
    return buf;
}

static void make_efuse(void) {
    u8 log[512];
    memset(log, 0xFF, sizeof log);
    log[0] = 0x29; log[1] = 0x81;                            /* rtl_id */
    /* potência do caminho A: 2,4 GHz CCK/BW40 = 30, 5 GHz BW40 = 35, OFDM +2, VHT 80 MHz -2 */
    for (int i = 0; i < 6; i++) log[0x10 + i] = 30;
    for (int i = 0; i < 5; i++) log[0x16 + i] = 30;
    log[0x10 + 11] = 0x02; log[0x10 + 12] = 0x00;
    for (int i = 0; i < 14; i++) log[0x10 + 18 + i] = 35;
    log[0x10 + 18 + 14] = 0x02;                              /* ofdm +2, bw20 +0 */
    log[0x10 + 18 + 15] = 0x00;
    log[0x10 + 18 + 20] = 0xE0;                              /* vht 1ss bw80 -2 */
    log[0xB9] = 0x20;                                        /* cristal */
    log[0xC1] = 0x20;                                        /* btcoex */
    log[0xC3] = 0x00;                                        /* antena separada */
    log[0xCA] = 0x00;                                        /* RFE 0 */
    log[0xCB] = 'B'; log[0xCC] = 'R';
    static const u8 mac[6] = {0x0C, 0xCD, 0xD0, 0x11, 0x22, 0x33};
    memcpy(log + 0xD0, mac, 6);
    /* grava em cabeçalhos de 2 bytes, bloco por bloco (8 bytes cada), só os blocos usados */
    int p = 0;
    for (int blk = 0; blk < 64; blk++) {
        bool used = false;
        for (int i = 0; i < 8; i++) if (log[blk * 8 + i] != 0xFF) used = true;
        if (!used) continue;
        efuse[p++] = (u8)((blk & 7) << 5 | 0x0F);
        efuse[p++] = (u8)((blk >> 3) << 4 | 0x0);
        memcpy(efuse + p, log + blk * 8, 8); p += 8;
    }
    memset(efuse + p, 0xFF, 512 - p);
}

int main(int argc, char **argv) {
    u32 n = 0;
    u8 *fwb = load(argc > 1 ? argv[1] : "firmware/rtw88/rtw8821c_fw.bin", &n);
    if (!fwb) { printf("sem firmware\n"); return 1; }
    rtw_fw_t fw; char err[96];
    if (!rtw_fw_parse(fwb, n, &fw, err, sizeof err)) { printf("firmware: %s\n", err); return 1; }
    rtw8821c_set_fw(&fw);
    dmem_base = fw.dmem_addr; imem_base = fw.imem_addr; emem_base = fw.emem_addr;
    cpumem_dmem = calloc(1, 0x10000); cpumem_imem = calloc(1, 0x20000); cpumem_emem = calloc(1, 0x20000);
    make_efuse();
    R[0x100] = 0xEA;                                         /* chip desligado */
    wr32raw(0xF0, 0x0000A000 | (1u << 23));                  /* SYS_CFG1: corte B? (12-15 = 0xA -> K), MP */
    wr32raw(0xF0, 0x00001000);                               /* corte B */

    bool ok = rtw8821c_attach(&io, NULL);
    CHECK(ok && radio, "o driver sobe: energia, firmware, eFuse, MAC, PHY, coex e registra o rádio");
    if (!ok) return 1;
    CHECK(!memcmp(cpumem_dmem, fw.dmem, fw.dmem_size), "DMEM da CPU do Wi-Fi = firmware (%u bytes)", fw.dmem_size);
    CHECK(!memcmp(cpumem_imem, fw.imem, fw.imem_size), "IMEM da CPU do Wi-Fi = firmware (%u bytes)", fw.imem_size);
    CHECK(!fw.emem_size || !memcmp(cpumem_emem, fw.emem, fw.emem_size), "EMEM da CPU do Wi-Fi = firmware (%u bytes)", fw.emem_size);
    const rtw_efuse_t *e = rtw8821c_efuse();
    CHECK(!memcmp(radio->mac, "\x0C\xCD\xD0\x11\x22\x33", 6) && e->rfe == 0 && !e->share_ant, "MAC e RFE lidos do eFuse");
    CHECK(R[0x610] == 0x0C && R[0x615] == 0x33, "MAC gravado na porta 0");
    CHECK(R[0x100] == 0xFF, "CR: DMA e MAC de TX/RX ligados");
    CHECK((rd32raw(0x608) & 0xF000000E) == 0xF000000E, "RCR: FCS/MIC/ICV/PHY status e broadcast/multicast/unicast aceitos");
    CHECK(nh2cpkt >= 2 && h2c_pkt_log[0][2] == 0x0D && h2c_pkt_log[1][2] == 0x11, "H2C de informação geral e de PHY pelo anel de H2C");
    CHECK(h2c_pkt_log[1][8] == 0 && h2c_pkt_log[1][9] == 1, "PHYDM: RFE 0, 1T1R");

    /* canal 42, 80 MHz (a rede do Thiago) */
    ok = radio->set_chan(radio, 36, 42, 2);
    int band, pr, ce, bw;
    rtw8821c_state(&band, &pr, &ce, &bw);
    CHECK(ok && band == 1 && pr == 36 && ce == 42 && bw == 2, "troca para 5 GHz canal 42/80 MHz");
    u32 r18 = rf[0x18];
    CHECK((r18 & 0xFF) == 42 && (r18 & ((1u << 16) | (1u << 8))) == ((1u << 16) | (1u << 8)) && (r18 & (3u << 10)) == (1u << 10), "RF 0x18: canal 42, banda 5 GHz, 80 MHz");
    CHECK((rd32raw(0xCB8) & ((1u << 23) | (1u << 20))) == ((1u << 23) | (1u << 20)), "chave de RF em WLA (5 GHz)");
    u32 o0 = rd32raw(0x1D04), o1 = rd32raw(0x1D08), m0 = rd32raw(0x1D0C), m1 = rd32raw(0x1D10), v0 = rd32raw(0x1D2C), v2 = rd32raw(0x1D34);
    printf("     TXAGC: OFDM %08x %08x, HT %08x %08x, VHT %08x .. %08x\n", o0, o1, m0, m1, v0, v2);
    CHECK(o0 == 0x2A2A2A2A && o1 == 0x2527292A && m0 == 0x1F1F1F1F && m1 == 0x1F1F1F1F && v0 == 0x1C1C1C1C && (v2 & 0xFFFF) == 0x1C1C,
          "potência gravada = tabela do Linux (OFDM 42/41/39/37, MCS 31, VHT 28)");
    CHECK((rd32raw(0x8AC) & 3) == 2, "BB em 80 MHz (ADCCLK)");
    CHECK((R[0x0668] & 0x80) == 0 && (R[0x0669] & 1), "MAC em 80 MHz (RFMOD)");

    /* TX de gestão: probe request */
    u8 probe[40] = {0x40, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(probe + 10, radio->mac, 6); memset(probe + 16, 0xFF, 6);
    probe[24] = 0; probe[25] = 0;
    ok = radio->tx(radio, probe, 26);
    u32 w1 = (u32)mgmt_desc[4] | (u32)mgmt_desc[5] << 8 | (u32)mgmt_desc[6] << 16;
    CHECK(ok && mgmt_len == 26 && !memcmp(mgmt_out, probe, 26), "TX: probe request sai pela fila de gestão inteiro");
    CHECK(((w1 >> 8) & 0x1F) == 18 && (mgmt_desc[16] & 0x7F) == 0x04 && (mgmt_desc[13] & 1), "TX: QSEL de gestão, 6 Mb/s fixo em 5 GHz");
    u16 ck = 0;
    for (int i = 0; i < 16; i++) ck ^= (u16)(mgmt_desc[2 * i] | mgmt_desc[2 * i + 1] << 8);
    CHECK(ck == 0, "TX: checksum do descritor confere");

    /* RX: beacon */
    u8 bcn[60] = {0x80, 0};
    memset(bcn + 4, 0xFF, 6); memcpy(bcn + 10, "\x02\xAA\xBB\xCC\xDD\xEE", 6); memcpy(bcn + 16, "\x02\xAA\xBB\xCC\xDD\xEE", 6);
    rx_inject(bcn, 60, 0x04, -47, false);
    u8 f[2400]; int rssi = 0;
    int got = radio->rx(radio, f, sizeof f, &rssi);
    CHECK(got == 60 && !memcmp(f, bcn, 60) && rssi == -47, "RX: beacon chega sem FCS, RSSI -47 dBm da PHY");
    u8 c2h_ra[8] = {0x0C, 0x01, 0x35, 0, 0, 0, 0};
    rx_inject(c2h_ra, 7, 0, 0, true);
    got = radio->rx(radio, f, sizeof f, &rssi);
    CHECK(got == 0, "RX: C2H do firmware é consumido pelo driver, não vai para a pilha");

    /* associação: VHT 80 MHz, MCS 0-9 */
    wifi_bss_t b = {0};
    memcpy(b.bssid, "\x02\xAA\xBB\xCC\xDD\xEE", 6);
    b.aid = 3; b.primary = 36; b.center = 42; b.bw = 2; b.ht = b.vht = true; b.sgi80 = true;
    b.ht_mcs = 0xFF; b.vht_mcs_map = 0xFFFE; b.legacy = 0xFF0;
    int before = nh2c;
    radio->bss(radio, &b);
    bool media = false, ra = false;
    for (int i = before; i < nh2c; i++) {
        if (h2c_log[i].b[0] == 0x01 && h2c_log[i].b[1] == 1) media = true;
        if (h2c_log[i].b[0] == 0x40) {
            u32 w = (u32)h2c_log[i].b[0] | (u32)h2c_log[i].b[1] << 8 | (u32)h2c_log[i].b[2] << 16 | (u32)h2c_log[i].b[3] << 24;
            u32 mask = (u32)h2c_log[i].b[4] | (u32)h2c_log[i].b[5] << 8 | (u32)h2c_log[i].b[6] << 16 | (u32)h2c_log[i].b[7] << 24;
            ra = ((w >> 16) & 0x1F) == 10 && ((w >> 24) & 3) == 2 && (w & (1u << 23)) && ((w >> 28) & 3) == 1 && mask == (0xFF0u | 0x3FFu << 12);
        }
    }
    CHECK(media, "associação: H2C de estado da mídia (conectado)");
    CHECK(ra, "associação: RA com ARFR1 AC 1SS, 80 MHz, SGI, VHT, máscara OFDM + MCS0-9");
    CHECK(!memcmp(R + 0x618, b.bssid, 6) && (R[0x6A8] | R[0x6A9] << 8) == 3 && ((rd32raw(0x100) >> 16) & 3) == 2, "BSSID, AID e tipo de rede gravados");

    /* dados unicast: o firmware escolhe a taxa */
    u8 data[64] = {0x88, 0x01};
    memcpy(data + 4, b.bssid, 6); memcpy(data + 10, radio->mac, 6); memcpy(data + 16, b.bssid, 6);
    ok = radio->tx(radio, data, 64);
    CHECK(ok, "TX: dados QoS pela fila BE");

    /* economia de energia: entra e sai */
    CHECK(radio->powersave(radio, true) && (R[0x604] & 0x80), "LPS: o firmware entra (PWRMGT_HWDATA_EN)");
    CHECK(radio->powersave(radio, false) && !(R[0x604] & 0x80), "LPS: sai e o TCR volta");
    lps_stuck = 1;
    radio->powersave(radio, true);
    bool left = radio->powersave(radio, false);
    CHECK(!left && rtw8821c_ps_broken() && !(R[0x604] & 0x80), "LPS: se o firmware travar (como no Linux deste PC), o driver limpa o TCR e desliga o LPS");

    radio->bss(radio, NULL);
    CHECK(((rd32raw(0x100) >> 16) & 3) == 0, "desassociação volta o tipo de rede para sem enlace");

    /* repouso S3: desliga, a energia some, volta e pede reconexão à pilha */
    CHECK(pm_regs == 1, "registro no pm.c com PM_RESTORES_NET");
    radio->set_chan(radio, 36, 42, 2);
    memset(cpumem_dmem, 0, 0x10000); memset(cpumem_imem, 0, 0x20000); memset(cpumem_emem, 0, 0x20000);
    rtw8821c_pm(0);
    CHECK(R[0x100] == 0xEA, "suspend: MAC desligado pela sequência de energia");
    CHECK(!radio->tx(radio, probe, 26), "nada sai com o chip desligado");
    memset(R + 0x1D00, 0, 0x100);                            /* potência perdida com a energia */
    rtw8821c_pm(1);
    rtw8821c_state(&band, &pr, &ce, &bw);
    CHECK(!memcmp(cpumem_imem, fw.imem, fw.imem_size) && band == 1 && ce == 42 && bw == 2 && rd32raw(0x1D04) == 0x2A2A2A2A,
          "resume: firmware de novo, canal 42/80 MHz e potência regravados");
    CHECK(lost_calls == 1, "resume pede à pilha para reconectar");

    printf(g_fail ? "rtw88-chip: %d FALHA(S)\n" : "rtw88-chip: ok\n", g_fail);
    return g_fail != 0;
}
