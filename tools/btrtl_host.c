/* mOS — teste de host do carregador de firmware Bluetooth Realtek (kernel/bt/btrtl.c) e do despacho por
 * fabricante (kernel/bt/vendor.c). Um controlador Realtek simulado responde aos comandos HCI; o teste
 * remonta o que chegou pelo 0xFC20 e compara com o patch esperado, montado aqui de forma independente
 * a partir do firmware real (firmware/rtl_bt/). Não sobe o kernel nem usa QEMU.
 *
 *   gcc -O2 -Wall -Wextra -Wno-incompatible-library-redeclaration -I kernel/include tools/btrtl_host.c kernel/bt/btrtl.c kernel/bt/vendor.c \
 *       kernel/bt/btintel.c -o /tmp/btrtl_host && /tmp/btrtl_host firmware
 */
#include "bluetooth.h"
#include "vfs.h"

/* só os cabeçalhos do kernel (os da libc brigam com types.h); estas vêm da libc do host */
typedef struct _IO_FILE FILE;
int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
FILE *fopen(const char *path, const char *mode);
int fseek(FILE *f, long off, int whence);
long ftell(FILE *f);
usize fread(void *p, usize sz, usize n, FILE *f);
int fclose(FILE *f);
void *malloc(usize n);
void free(void *p);
void exit(int code);
#define SEEK_SET 0
#define SEEK_END 2

int btrtl_build_image(const u8 *fw, u64 fw_len, const u8 *cfg, u64 cfg_len, u8 rom_version, int want_project, u8 **out);
u8 btrtl_frag_index(u32 i, u32 nfrag);

/* ---------------------------------------------------------------- ambiente do kernel */
static int quiet;
void kprintf(const char *fmt, ...) {
    if (quiet) return;
    va_list ap;
    va_start(ap, fmt);
    printf("    | ");
    vprintf(fmt, ap);
    va_end(ap);
}
void *kmalloc(usize n) { return malloc(n); }
void kfree(void *p) { free(p); }
static u64 now_ms;
u64 timer_ticks(void) { return now_ms += 1; }
void sleep_ms(u64 ms) { now_ms += ms; }

#define MAXF 4
static vnode_t files[MAXF];
static char fpaths[MAXF][96];
static int nfiles;
static void fs_reset(void) { nfiles = 0; }
static void fs_add(const char *path, u8 *data, usize size) {
    snprintf(fpaths[nfiles], sizeof fpaths[0], "%s", path);
    memset(&files[nfiles], 0, sizeof files[0]);
    files[nfiles].data = data;
    files[nfiles].size = size;
    nfiles++;
}
vnode_t *vfs_lookup(const char *path) {
    for (int i = 0; i < nfiles; i++)
        if (!strcmp(fpaths[i], path)) return &files[i];
    return NULL;
}

/* ---------------------------------------------------------------- controlador simulado */
static struct {
    u16 manuf, lmp_subver, hci_rev;
    u8  hci_ver, rom;
    int fail_frag;                 /* bloco (1..n) que devolve status 1; 0 = nenhum */
    u8  rx[200000];                /* bytes recebidos pelo 0xFC20 */
    u32 rx_len, nfrag;
    u8  idx[2048];
    bool last_seen, order_ok;
    int  n1001, nfc6d, nfc05;
    u16  ops[4096];
    int  nops;
} sim;

static void sim_reset(u16 manuf, u16 subver, u16 rev, u8 ver, u8 rom) {
    memset(&sim, 0, sizeof sim);
    sim.manuf = manuf; sim.lmp_subver = subver; sim.hci_rev = rev; sim.hci_ver = ver; sim.rom = rom;
    sim.order_ok = true;
}

int bt_hci_cmd_resp(u16 op, const void *p, u8 len, u8 *resp, int max, int timeout_ms) {
    (void)timeout_ms;
    u8 r[16];
    int n = 0;
    if (sim.nops < (int)ARRAY_LEN(sim.ops)) sim.ops[sim.nops++] = op;
    if (op == 0x1001) {
        sim.n1001++;
        r[0] = 0; r[1] = sim.hci_ver; r[2] = (u8)sim.hci_rev; r[3] = (u8)(sim.hci_rev >> 8);
        r[4] = sim.hci_ver; r[5] = (u8)sim.manuf; r[6] = (u8)(sim.manuf >> 8);
        r[7] = (u8)sim.lmp_subver; r[8] = (u8)(sim.lmp_subver >> 8);
        n = 9;
    } else if (op == 0xFC6D) {
        sim.nfc6d++;
        r[0] = 0; r[1] = sim.rom; n = 2;
    } else if (op == 0xFC20) {
        const u8 *b = p;
        if (len < 2 || sim.last_seen) sim.order_ok = false;
        u8 idx = b[0];
        sim.idx[sim.nfrag++] = idx;
        memcpy(sim.rx + sim.rx_len, b + 1, len - 1u);
        sim.rx_len += len - 1u;
        if (idx & 0x80) {
            sim.last_seen = true;
            sim.lmp_subver = 0x75B8;               /* patch rodando: o chip passa a informar outra versão */
        }
        r[0] = (sim.fail_frag && (int)sim.nfrag == sim.fail_frag) ? 1 : 0;
        r[1] = idx; n = 2;
    } else if (op == 0xFC05) {
        sim.nfc05++;
        r[0] = 0x01; n = 1;                        /* Intel Read Version recusado: btintel desiste */
    } else {
        r[0] = 0x01; n = 1;                        /* comando desconhecido */
    }
    n = n < max ? n : max;
    if (resp) memcpy(resp, r, n);
    return n;
}
int bt_hci_cmd(u16 op, const void *p, u8 len, int timeout_ms) {
    u8 r[16];
    int n = bt_hci_cmd_resp(op, p, len, r, sizeof r, timeout_ms);
    return n < 1 ? -1 : r[0];
}
bool bt_hci_cmd_async(u16 op, const void *p, u8 len) { (void)op; (void)p; (void)len; return true; }
int bt_wait_event(u8 *buf, int max, int timeout_ms) { (void)buf; (void)max; (void)timeout_ms; return -2; }

/* ---------------------------------------------------------------- utilidades */
static int fails, oks;
static void check(int cond, const char *what) {
    if (cond) { oks++; printf("ok  %s\n", what); }
    else { fails++; printf("FALHOU %s\n", what); }
}
static u8 *read_all(const char *path, usize *len) {
    FILE *f = fopen(path, "rb");
    if (!f) { printf("não abriu %s\n", path); exit(2); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *b = malloc((usize)n);
    if (fread(b, 1, (usize)n, f) != (usize)n) exit(2);
    fclose(f);
    *len = (usize)n;
    return b;
}
static u16 g16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u32 g32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

/* patch esperado, montado direto do formato (independente de btrtl.c) */
static u8 *expected(const u8 *fw, const u8 *cfg, usize cfg_len, u8 rom, u32 *len) {
    u16 n = g16(fw + 12);
    for (u16 i = 0; i < n; i++) {
        if (g16(fw + 14 + 2 * i) != rom + 1) continue;
        u32 pl = g16(fw + 14 + 2 * n + 2 * i), po = g32(fw + 14 + 4 * n + 4 * i);
        u8 *e = malloc(pl + cfg_len);
        memcpy(e, fw + po, pl);
        memcpy(e + pl - 4, fw + 8, 4);
        if (cfg_len) memcpy(e + pl, cfg, cfg_len);
        *len = pl + (u32)cfg_len;
        return e;
    }
    *len = 0;
    return NULL;
}
static bool indices_ok(void) {
    for (u32 i = 0; i < sim.nfrag; i++) {
        u8 want = i > 0x7F ? (u8)((i & 0x7F) + 1) : (u8)i;
        if (i == sim.nfrag - 1) want |= 0x80;
        if (sim.idx[i] != want) return false;
    }
    return sim.nfrag > 0;
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "firmware";
    char p[256];
    usize fw_len, cfg_len;
    snprintf(p, sizeof p, "%s/rtl_bt/rtl8821c_fw.bin", dir);
    u8 *fw = read_all(p, &fw_len);
    snprintf(p, sizeof p, "%s/rtl_bt/rtl8821c_config.bin", dir);
    u8 *cfg = read_all(p, &cfg_len);
    printf("firmware real: rtl8821c_fw.bin %llu bytes, %u patches; config %llu bytes\n", fw_len, g16(fw + 12), cfg_len);

    /* índices dos blocos */
    check(btrtl_frag_index(0, 200) == 0x00 && btrtl_frag_index(0x7F, 200) == 0x7F, "índices 0..0x7F seguem o contador");
    check(btrtl_frag_index(0x80, 200) == 0x01 && btrtl_frag_index(0x81, 200) == 0x02, "depois de 0x7F o índice volta para 1");
    check(btrtl_frag_index(199, 200) == ((u8)((199 & 0x7F) + 1) | 0x80) && btrtl_frag_index(0, 1) == 0x80, "último bloco leva o bit 7");

    for (u8 rom = 0; rom <= 1; rom++) {
        printf("-- RTL8821C na ROM, versão da ROM %u\n", rom);
        fs_reset();
        fs_add("/Sistema/Firmware/rtl_bt/rtl8821c_fw.bin", fw, fw_len);
        fs_add("/Sistema/Firmware/rtl_bt/rtl8821c_config.bin", cfg, cfg_len);
        sim_reset(0x005D, 0x8821, 0x000C, 0x08, rom);
        bool ok = bt_vendor_setup(0x0BDA, 0xC821);
        u32 elen;
        u8 *e = expected(fw, cfg, cfg_len, rom, &elen);
        check(ok, "bt_vendor_setup(0bda:c821) devolve true");
        check(sim.nfc6d == 1, "lê a versão da ROM (0xFC6D)");
        check(e && sim.rx_len == elen && !memcmp(sim.rx, e, elen), "bytes recebidos == patch do chip_id ROM+1 com fw_version + config");
        check(sim.nfrag == (elen + 251) / 252, "número de blocos de 252 bytes");
        check(indices_ok() && sim.order_ok, "índices em ordem, com volta e bit 7 só no último");
        if (rom == 1) check(sim.nfrag > 0x80, "o patch da ROM v1 passa de 128 blocos (exercita a volta do índice)");
        check(sim.n1001 == 2, "relê a versão depois do envio");
        printf("    patch %u bytes em %u blocos\n", elen, sim.nfrag);
        free(e);
    }

    printf("-- patch já carregado (lmp_subver diferente da ROM)\n");
    sim_reset(0x005D, 0x75B8, 0x000C, 0x08, 1);
    check(bt_vendor_setup(0x0BDA, 0xC821) && sim.nfrag == 0 && sim.nfc6d == 0, "não reenvia nada");

    printf("-- outro fabricante com VID 0bda\n");
    sim_reset(0x000A, 0x8821, 0x000C, 0x08, 1);
    check(bt_vendor_setup(0x0BDA, 0x1234) && sim.nfrag == 0 && sim.nfc6d == 0, "fabricante != 93: não mexe");

    printf("-- VID sem relação com a Realtek\n");
    sim_reset(0x005D, 0x8821, 0x000C, 0x08, 1);
    check(bt_vendor_setup(0x0A12, 0x0001) && sim.nops == 0, "nem envia comando");

    printf("-- Intel continua indo para o btintel\n");
    sim_reset(0x0002, 0, 0, 0, 0);
    quiet = 1;
    bool intel = bt_vendor_setup(0x8087, 0x0026);
    quiet = 0;
    check(sim.nfc05 == 1 && sim.n1001 == 0 && !intel, "8087: chama o Intel Read Version (0xFC05), não o caminho Realtek");

    printf("-- firmware ausente\n");
    fs_reset();
    sim_reset(0x005D, 0x8821, 0x000C, 0x08, 1);
    check(!bt_vendor_setup(0x0BDA, 0xC821) && sim.nfrag == 0, "sem o arquivo: falha e não envia nada");

    printf("-- bloco recusado pelo chip\n");
    fs_reset();
    fs_add("/Sistema/Firmware/rtl_bt/rtl8821c_fw.bin", fw, fw_len);
    sim_reset(0x005D, 0x8821, 0x000C, 0x08, 1);
    sim.fail_frag = 3;
    check(!bt_vendor_setup(0x0BDA, 0xC821) && sim.nfrag == 3, "para no bloco 3 e devolve false");

    printf("-- sem config: só o patch\n");
    sim_reset(0x005D, 0x8821, 0x000C, 0x08, 1);
    {
        u32 elen;
        u8 *e = expected(fw, NULL, 0, 1, &elen);
        check(bt_vendor_setup(0x0BDA, 0xC821) && sim.rx_len == elen && !memcmp(sim.rx, e, elen), "envia o patch sem config");
        free(e);
    }

    printf("-- arquivos corrompidos (btrtl_build_image)\n");
    u8 *img;
    u8 *bad = malloc(fw_len);
    memcpy(bad, fw, fw_len);
    bad[0] = 'X';
    check(btrtl_build_image(bad, fw_len, cfg, cfg_len, 1, 10, &img) == -1, "sem \"Realtech\": recusa");
    memcpy(bad, fw, fw_len);
    bad[fw_len - 1] ^= 0xFF;
    check(btrtl_build_image(bad, fw_len, cfg, cfg_len, 1, 10, &img) == -2, "assinatura de extensão errada: recusa");
    check(btrtl_build_image(fw, fw_len, cfg, cfg_len, 1, 13, &img) == -3, "arquivo de outro projeto (8822C): recusa");
    check(btrtl_build_image(fw, fw_len, cfg, cfg_len, 5, 10, &img) == -5, "sem patch para a ROM v5: recusa");
    memcpy(bad, fw, fw_len);
    bad[14 + 2 * 2 + 2] = 0xFF; bad[14 + 2 * 2 + 3] = 0xFF;          /* patch_length[1] = 65535 */
    bad[14 + 4 * 2 + 4 + 3] = 0x7F;                                  /* patch_offset[1] enorme */
    check(btrtl_build_image(bad, fw_len, cfg, cfg_len, 1, 10, &img) == -6, "patch apontando para fora do arquivo: recusa");
    u8 badcfg[10];
    memcpy(badcfg, cfg, sizeof badcfg);
    badcfg[0] = 0;
    int n = btrtl_build_image(fw, fw_len, badcfg, sizeof badcfg, 1, 10, &img);
    u32 elen;
    u8 *e = expected(fw, NULL, 0, 1, &elen);
    check(n == (int)elen && !memcmp(img, e, elen), "config com assinatura errada: ignora o config, mantém o patch");
    free(img); free(e); free(bad);

    printf("%s (%d ok, %d falhas)\n", fails ? "btrtl_host: FALHOU" : "btrtl_host: ok", oks, fails);
    return fails ? 1 : 0;
}
