/*
 * mOS — carregador de firmware dos controladores Bluetooth Realtek USB (ex.: 0bda:c821, o Bluetooth do
 * combo RTL8821CE). O controlador liga com o código da ROM, que já responde a HCI, mas sem o patch o
 * rádio LE/BR-EDR não funciona direito. O patch (formato "epatch" v1, arquivo "Realtech") é enviado
 * pelo comando de fabricante 0xFC20 em blocos de até 252 bytes; depois vem o HCI Reset normal
 * (bluetooth.c), que coloca o código novo para rodar.
 *
 * Sequência:
 *   1. HCI Read Local Version (0x1001): fabricante 93 (Realtek), lmp_subver e hci_rev identificam o
 *      chip ainda na ROM (8821C: 0x8821 / 0x000c). Outro lmp_subver = patch já carregado: nada a fazer.
 *   2. Versão da ROM (0xFC6D): escolhe, dentro do arquivo, o patch com chip_id == ROM + 1.
 *   3. Arquivo rtl_bt/<chip>_fw.bin:
 *        "Realtech" | fw_version (le32) | n (le16) | chip_id[n] (le16) | len[n] (le16) | off[n] (le32) | ...
 *      e no fim a assinatura de extensão 51 04 fd 77, precedida (de trás para frente) por registros
 *      [dados][tamanho][opcode]; o opcode 0 com tamanho 1 é o ID do projeto (8821C = 10), que tem que
 *      bater com o chip, para não gravar o patch de outro chip.
 *   4. Os 4 últimos bytes do patch escolhido viram o fw_version; o rtl_bt/<chip>_config.bin (assinatura
 *      0x8723ab55 | tamanho le16 | entradas) vai colado no fim.
 *   5. 0xFC20 com [índice][dados]: o índice conta a partir de 0, depois de 0x7F volta para 1, e o
 *      último bloco leva o bit 7.
 */
#include "bluetooth.h"
#include "vfs.h"

#define FW_DIR   "/Sistema/Firmware"
#define FRAG_LEN 252
#define RTL_MANUF 0x005D

typedef struct {
    u16 lmp_subver, hci_rev;
    u8  hci_ver;
    u8  project_id;                /* ID gravado na extensão do arquivo */
    const char *fw, *cfg;          /* sem FW_DIR */
    const char *name;
} rtl_ic_t;

/* chips na ROM (antes do patch). Só os que têm arquivo na imagem; os outros caem em "desconhecido". */
static const rtl_ic_t ics[] = {
    {0x8821, 0x000c, 0x08, 10, "rtl_bt/rtl8821c_fw.bin", "rtl_bt/rtl8821c_config.bin", "RTL8821C"},
};

typedef struct {
    u16 lmp_subver, hci_rev, manuf;
    u8  hci_ver;
} rtl_ver_t;

static u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

static bool read_local_version(rtl_ver_t *v, int timeout_ms) {
    u8 r[16];
    int n = bt_hci_cmd_resp(0x1001, NULL, 0, r, sizeof r, timeout_ms);
    if (n < 9 || r[0]) return false;
    v->hci_ver = r[1];
    v->hci_rev = rd16(r + 2);
    v->manuf = rd16(r + 5);
    v->lmp_subver = rd16(r + 7);
    return true;
}

static const u8 *load_file(const char *name, u64 *len) {
    char path[96];
    snprintf(path, sizeof path, FW_DIR "/%s", name);
    vnode_t *v = vfs_lookup(path);
    if (!v || v->dir || !v->data || !v->size) return NULL;
    *len = v->size;
    return v->data;
}

/* ID do projeto gravado na extensão do fim do arquivo; -1 se o arquivo não tiver extensão válida */
static int project_id(const u8 *fw, u64 len) {
    static const u8 sig[4] = {0x51, 0x04, 0xFD, 0x77};
    if (len < 14 + 4 + 3 || memcmp(fw + len - 4, sig, 4)) return -1;
    const u8 *p = fw + len - 4, *min = fw + 14 + 3;
    while (p >= min) {
        u8 op = *--p, l = *--p, data = *--p;
        if (op == 0xFF) break;
        if (!l) return -1;
        if (op == 0 && l == 1) return data;
        if ((u64)(p - fw) < l) return -1;
        p -= l;
    }
    return -1;
}

/* Monta patch + config em *out (alocado aqui). Devolve o tamanho ou <0 com motivo no log.
 * Exposto (não static) para o teste de host. */
int btrtl_build_image(const u8 *fw, u64 fw_len, const u8 *cfg, u64 cfg_len, u8 rom_version, int want_project, u8 **out) {
    *out = NULL;
    if (fw_len < 14 || memcmp(fw, "Realtech", 8)) { kprintf("btrtl: arquivo de firmware sem o cabeçalho \"Realtech\"\n"); return -1; }
    int pid = project_id(fw, fw_len);
    if (pid < 0) { kprintf("btrtl: assinatura de extensão ausente ou corrompida\n"); return -2; }
    if (pid != want_project) { kprintf("btrtl: firmware do projeto %d, o chip é do projeto %d\n", pid, want_project); return -3; }
    u32 fw_version = rd32(fw + 8);
    u16 n = rd16(fw + 12);
    if (!n || 14 + (u64)n * 8 > fw_len) { kprintf("btrtl: tabela de patches inválida (%u)\n", n); return -4; }
    const u8 *ids = fw + 14, *lens = ids + 2 * n, *offs = lens + 2 * n;
    u32 plen = 0, poff = 0;
    bool found = false;
    for (u16 i = 0; i < n; i++)
        if (rd16(ids + 2 * i) == (u16)(rom_version + 1)) {
            plen = rd16(lens + 2 * i);
            poff = rd32(offs + 4 * i);
            found = true;
            break;
        }
    if (!found) { kprintf("btrtl: nenhum patch para a ROM versão %u\n", rom_version); return -5; }
    if (plen < 4 || (u64)poff + plen > fw_len - 4) { kprintf("btrtl: patch fora do arquivo (off %u, %u bytes)\n", poff, plen); return -6; }
    if (cfg) {
        if (cfg_len < 6 || rd32(cfg) != 0x8723AB55 || 6u + rd16(cfg + 4) != cfg_len) {
            kprintf("btrtl: config inválido (%llu bytes); seguindo sem ele\n", cfg_len);
            cfg = NULL;
            cfg_len = 0;
        }
    } else cfg_len = 0;
    u64 total = plen + cfg_len;
    u8 *img = kmalloc(total);
    if (!img) return -7;
    memcpy(img, fw + poff, plen);
    img[plen - 4] = (u8)fw_version;
    img[plen - 3] = (u8)(fw_version >> 8);
    img[plen - 2] = (u8)(fw_version >> 16);
    img[plen - 1] = (u8)(fw_version >> 24);
    if (cfg_len) memcpy(img + plen, cfg, cfg_len);
    *out = img;
    return (int)total;
}

/* índice do bloco i (0..nfrag-1) no comando 0xFC20 */
u8 btrtl_frag_index(u32 i, u32 nfrag) {
    u8 idx = i > 0x7F ? (u8)((i & 0x7F) + 1) : (u8)i;
    if (i == nfrag - 1) idx |= 0x80;
    return idx;
}

static bool download(const u8 *img, u32 len) {
    u8 pkt[1 + FRAG_LEN], r[8];
    u32 nfrag = (len + FRAG_LEN - 1) / FRAG_LEN;
    for (u32 i = 0; i < nfrag; i++) {
        u32 k = MIN((u32)FRAG_LEN, len - i * FRAG_LEN);
        pkt[0] = btrtl_frag_index(i, nfrag);
        memcpy(pkt + 1, img + i * FRAG_LEN, k);
        int n = bt_hci_cmd_resp(0xFC20, pkt, (u8)(k + 1), r, sizeof r, 2000);
        if (n < 1 || r[0]) {
            kprintf("btrtl: bloco %u/%u recusado (%d, status %d)\n", i + 1, nfrag, n, n > 0 ? r[0] : -1);
            return false;
        }
    }
    return true;
}

static const rtl_ic_t *find_ic(const rtl_ver_t *v) {
    for (u32 i = 0; i < ARRAY_LEN(ics); i++)
        if (ics[i].lmp_subver == v->lmp_subver && ics[i].hci_rev == v->hci_rev && ics[i].hci_ver == v->hci_ver) return &ics[i];
    return NULL;
}

/* fabricantes que vendem módulos Realtek com o próprio VID (além do 0bda) */
static bool realtek_vid(u16 vid) {
    switch (vid) {
    case 0x0BDA: case 0x13D3: case 0x0489: case 0x04CA: case 0x04C5: case 0x0CB8: case 0x0CB5:
    case 0x2FF8: case 0x2357: case 0x2550: case 0x7392: case 0x2B89: case 0x35F5:
        return true;
    }
    return false;
}

/* true = pode seguir (sem nada a fazer, ou patch enviado). false = chip Realtek que ficou sem patch. */
bool btrtl_setup(u16 vid, u16 pid) {
    if (!realtek_vid(vid)) return true;
    rtl_ver_t v;
    if (!read_local_version(&v, 1000)) { kprintf("btrtl: %04x:%04x não respondeu ao Read Local Version\n", vid, pid); return true; }
    if (v.manuf != RTL_MANUF) return true;                       /* outro fabricante com o mesmo VID */
    const rtl_ic_t *ic = find_ic(&v);
    if (!ic) {
        kprintf("btrtl: lmp_subver 0x%04x hci_rev 0x%04x: patch já carregado ou chip sem firmware no mOS\n", v.lmp_subver, v.hci_rev);
        return true;
    }
    u8 r[8];
    int n = bt_hci_cmd_resp(0xFC6D, NULL, 0, r, sizeof r, 1000);
    if (n < 2 || r[0]) { kprintf("btrtl: %s: leitura da versão da ROM falhou (%d)\n", ic->name, n); return false; }
    u8 rom = r[1];
    u64 fw_len = 0, cfg_len = 0;
    const u8 *fw = load_file(ic->fw, &fw_len);
    if (!fw) { kprintf("btrtl: firmware " FW_DIR "/%s não encontrado\n", ic->fw); return false; }
    const u8 *cfg = load_file(ic->cfg, &cfg_len);
    if (!cfg) kprintf("btrtl: " FW_DIR "/%s ausente (seguindo sem config)\n", ic->cfg);
    u8 *img;
    int len = btrtl_build_image(fw, fw_len, cfg, cfg_len, rom, ic->project_id, &img);
    if (len < 0) return false;
    kprintf("btrtl: %s ROM v%u, enviando patch de %d bytes\n", ic->name, rom, len);
    u64 t0 = timer_ticks();
    bool ok = download(img, (u32)len);
    kfree(img);
    if (!ok) return false;
    rtl_ver_t v2;
    if (read_local_version(&v2, 1000))
        kprintf("btrtl: %s com patch em %llu ms (lmp_subver 0x%04x)\n", ic->name, timer_ticks() - t0, v2.lmp_subver);
    else
        kprintf("btrtl: %s: patch enviado em %llu ms\n", ic->name, timer_ticks() - t0);
    return true;
}
