/*
 * mOS — carregador de firmware dos controladores Bluetooth Intel (USB 8087:xxxx, ex.: 8087:0026 das
 * placas AX201/AX203/AX211). O controlador liga em modo bootloader e só vira um controlador HCI
 * normal depois que o firmware assinado (ibt-*.sfi) é enviado pelo "secure send" (0xFC09) e o chip
 * é reiniciado (0xFC01). Em seguida vêm as configurações DDC (ibt-*.ddc).
 *
 * Sequência (como no btintel do Linux, geração com versão em TLV):
 *   1. Intel Read Version (0xFC05, 0xFF) -> TLVs: CNVi/CNVr top, tipo de imagem, tipo de assinatura
 *   2. Se já estiver no firmware operacional (tipo 0x03): só carrega o DDC
 *   3. Nome: ibt-<cnvi>-<cnvr>.sfi, com <x> = swab16((TYPE << 4) | STEP)
 *   4. Secure send: cabeçalho CSS (0x00), chave pública (0x03), assinatura (0x02), e os comandos do
 *      corpo agrupados em fragmentos de tamanho múltiplo de 4 (0x01); guarda o endereço de boot (0xFC0E)
 *   5. Espera o evento 0xFF/0x06 (download ok), envia Intel Reset (0xFC01) com o endereço de boot e
 *      espera 0xFF/0x02 (boot ok)
 *   6. DDC: cada entrada [len][id(2)][dados] vira um comando 0xFC8B
 */
#include "bluetooth.h"
#include "vfs.h"

#define FW_DIR "/Sistema/Firmware"

/* fornecidos por dev/bluetooth.c; as versões fracas abaixo só valem enquanto ele não os tiver */
__attribute__((weak)) int bt_hci_cmd_resp(u16 opcode, const void *p, u8 len, u8 *resp, int max, int timeout_ms) {
    (void)opcode; (void)p; (void)len; (void)resp; (void)max; (void)timeout_ms;
    return -1;
}
__attribute__((weak)) bool bt_hci_cmd_async(u16 opcode, const void *p, u8 len) { (void)opcode; (void)p; (void)len; return false; }
__attribute__((weak)) int bt_wait_event(u8 *buf, int max, int timeout_ms) { (void)buf; (void)max; (void)timeout_ms; return -1; }

typedef struct {
    u32 cnvi_top, cnvr_top, cnvi_bt;
    u8  img_type, sbe_type, hw_variant;
    bool has_sbe;
    u32 build_num;
} ver_t;

static u32 rd32le(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u16 swab16(u16 v) { return (u16)(v << 8 | v >> 8); }
static u16 cnvx_name(u32 top) { return swab16((u16)(((top & 0x0FFF) << 4) | ((top & 0x0F000000) >> 24))); }

static bool read_version(ver_t *v) {
    u8 r[260], p = 0xFF;
    int n = bt_hci_cmd_resp(0xFC05, &p, 1, r, sizeof r, 2000);
    if (n < 2 || r[0]) { kprintf("btintel: Read Version falhou (n=%d, status %d)\n", n, n > 0 ? r[0] : -1); return false; }
    memset(v, 0, sizeof *v);
    for (int i = 1; i + 2 <= n && i + 2 + r[i + 1] <= n; i += 2 + r[i + 1]) {
        const u8 *val = r + i + 2;
        u8 t = r[i], l = r[i + 1];
        if (t == 0x10 && l >= 4) v->cnvi_top = rd32le(val);
        if (t == 0x11 && l >= 4) v->cnvr_top = rd32le(val);
        if (t == 0x12 && l >= 4) v->cnvi_bt = rd32le(val);
        if (t == 0x1C && l >= 1) v->img_type = val[0];
        if (t == 0x1F && l >= 2) v->build_num = (u32)val[0] | (u32)val[1] << 8;
        if (t == 0x2F && l >= 1) { v->sbe_type = val[0]; v->has_sbe = true; }
    }
    v->hw_variant = (u8)((v->cnvi_bt >> 16) & 0x3F);
    kprintf("btintel: CNVi 0x%08x CNVr 0x%08x, variante 0x%02x, imagem %s, assinatura %s\n", v->cnvi_top, v->cnvr_top,
            v->hw_variant, v->img_type == 1 ? "bootloader" : v->img_type == 3 ? "operacional" : "?",
            !v->has_sbe ? "?" : v->sbe_type ? "ECDSA" : "RSA");
    return true;
}

static bool secure_send(u8 type, const u8 *data, u32 len) {
    u8 pkt[253];
    while (len) {
        u32 k = MIN(len, 252u);
        pkt[0] = type;
        memcpy(pkt + 1, data, k);
        int st = bt_hci_cmd(0xFC09, pkt, (u8)(k + 1), 3000);
        if (st) { kprintf("btintel: secure send (tipo %u) falhou: %d\n", type, st); return false; }
        data += k; len -= k;
    }
    return true;
}

/* espera um evento de fabricante 0xFF com o subtipo dado; devolve o status (ou -1) */
static int wait_vendor(u8 subtype, int timeout_ms) {
    u8 ev[260];
    u64 end = timer_ticks() + (u64)timeout_ms;
    while (timer_ticks() < end) {
        int n = bt_wait_event(ev, sizeof ev, (int)(end - timer_ticks()));
        if (n < 3) continue;
        if (ev[0] == 0xFF && ev[2] == subtype) return n > 3 ? ev[3] : 0;
    }
    return -1;
}

static u8 *load_file(const char *name, u64 *len) {
    char path[96];
    snprintf(path, sizeof path, FW_DIR "/%s", name);
    vnode_t *v = vfs_lookup(path);
    if (!v || v->dir || !v->data || !v->size) return NULL;
    *len = v->size;
    return v->data;
}

static bool download(const u8 *fw, u64 len, bool ecdsa, u32 *boot_addr) {
    u64 body;
    if (ecdsa) {
        if (len < 964) return false;
        if (!secure_send(0x00, fw + 644, 128) || !secure_send(0x03, fw + 644 + 128, 96) ||
            !secure_send(0x02, fw + 644 + 224, 96)) return false;
        body = 964;
    } else {
        if (len < 644) return false;
        if (!secure_send(0x00, fw, 128) || !secure_send(0x03, fw + 128, 256) || !secure_send(0x02, fw + 388, 256)) return false;
        body = 644;
    }
    /* corpo: comandos HCI encadeados; envia em fragmentos cujo tamanho é múltiplo de 4 */
    const u8 *frag = fw + body;
    u32 flen = 0;
    *boot_addr = 0;
    for (u64 off = body; off + 3 <= len;) {
        u16 op = (u16)(fw[off] | fw[off + 1] << 8);
        u8 plen = fw[off + 2];
        if (off + 3 + plen > len) return false;
        if (op == 0xFC0E && plen >= 4) *boot_addr = rd32le(fw + off + 3);
        flen += 3 + plen;
        off += 3 + plen;
        if (flen % 4 == 0) {
            if (!secure_send(0x01, frag, flen)) return false;
            frag = fw + off;
            flen = 0;
        }
    }
    return flen == 0 || secure_send(0x01, frag, flen);
}

static bool load_ddc(const char *name) {
    u64 len = 0;
    const u8 *d = load_file(name, &len);
    if (!d) { kprintf("btintel: %s ausente (seguindo sem DDC)\n", name); return true; }
    for (u64 off = 0; off < len;) {
        u8 l = d[off];
        if (!l || off + 1 + l > len) break;
        int st = bt_hci_cmd(0xFC8B, d + off, (u8)(l + 1), 2000);
        if (st) { kprintf("btintel: DDC falhou (%d)\n", st); return false; }
        off += 1 + l;
    }
    kprintf("btintel: DDC %s aplicado\n", name);
    return true;
}

bool btintel_setup(u16 vid, u16 pid) {
    (void)pid;
    if (vid != 0x8087) return true;                    /* só controladores Intel */
    ver_t v;
    if (!read_version(&v)) return false;
    char sfi[48], ddc[48];
    snprintf(sfi, sizeof sfi, "ibt-%04x-%04x.sfi", cnvx_name(v.cnvi_top), cnvx_name(v.cnvr_top));
    snprintf(ddc, sizeof ddc, "ibt-%04x-%04x.ddc", cnvx_name(v.cnvi_top), cnvx_name(v.cnvr_top));
    if (v.img_type == 0x03) {
        kprintf("btintel: firmware já carregado (build %u)\n", v.build_num);
        return load_ddc(ddc);
    }
    if (v.img_type != 0x01) { kprintf("btintel: tipo de imagem inesperado 0x%02x\n", v.img_type); return false; }
    u64 len = 0;
    const u8 *fw = load_file(sfi, &len);
    if (!fw) { kprintf("btintel: firmware " FW_DIR "/%s não encontrado\n", sfi); return false; }
    kprintf("btintel: enviando %s (%llu KiB)\n", sfi, len >> 10);
    u64 t0 = timer_ticks();
    u32 boot = 0;
    bool ecdsa = v.has_sbe && v.sbe_type == 0x01;
    if (!download(fw, len, ecdsa, &boot)) { kprintf("btintel: falha no envio do firmware\n"); return false; }
    int st = wait_vendor(0x06, 5000);
    if (st != 0) { kprintf("btintel: o chip não confirmou o download (%d)\n", st); return false; }
    kprintf("btintel: firmware aceito em %llu ms, reiniciando (boot 0x%08x)\n", timer_ticks() - t0, boot);
    u8 reset[8] = {0x00, 0x01, 0x00, 0x01, (u8)boot, (u8)(boot >> 8), (u8)(boot >> 16), (u8)(boot >> 24)};
    bt_hci_cmd_async(0xFC01, reset, sizeof reset);
    if (wait_vendor(0x02, 5000) < 0) { kprintf("btintel: o chip não voltou depois do reset\n"); return false; }
    kprintf("btintel: firmware operacional em %llu ms\n", timer_ticks() - t0);
    ver_t v2;
    if (read_version(&v2) && v2.img_type != 0x03) kprintf("btintel: aviso: imagem ainda não é a operacional\n");
    return load_ddc(ddc);
}
