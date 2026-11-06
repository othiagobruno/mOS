/*
 * mOS — leitura dos arquivos de firmware Intel Wi-Fi (iwlwifi-*.ucode, formato TLV).
 *   [0 (4 bytes)] [magic "IWL\n" = 0x0A4C5749] [descrição (64)] [versão] [build] [8 ignorados]
 *   seguido de TLVs {tipo u32, tamanho u32, dados alinhados a 4}
 * Separa as seções de execução (RT/INIT), o IML (carregador de imagem das placas AX210/"So"),
 * a versão e as capacidades, para o driver (wifi/iwlwifi.c) montar o "context info".
 */
#include "iwl.h"

static u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

bool iwl_fw_parse(const u8 *d, u64 len, iwl_fw_t *fw, char *err, usize errlen) {
    memset(fw, 0, sizeof *fw);
    if (len < 88 || rd32(d) != 0 || rd32(d + 4) != 0x0A4C5749) { snprintf(err, errlen, "não é um firmware Intel TLV"); return false; }
    memcpy(fw->human, d + 8, 64);
    fw->human[63] = 0;
    fw->ver = rd32(d + 72);
    fw->build = rd32(d + 76);
    u64 off = 88;
    while (off + 8 <= len) {
        u32 type = rd32(d + off), tl = rd32(d + off + 4);
        const u8 *v = d + off + 8;
        if (off + 8 + tl > len) { snprintf(err, errlen, "TLV %u truncado", type); return false; }
        switch (type) {
        case IWL_TLV_SEC_RT: case IWL_TLV_SEC_INIT: {
            if (tl < 4) break;
            iwl_sec_t *s = type == IWL_TLV_SEC_RT ? &fw->rt[fw->n_rt] : &fw->init[fw->n_init];
            int *n = type == IWL_TLV_SEC_RT ? &fw->n_rt : &fw->n_init;
            if (*n >= IWL_MAX_SEC) break;
            s->offset = rd32(v);
            s->data = v + 4;
            s->len = tl - 4;
            (*n)++;
            break;
        }
        case IWL_TLV_IML: fw->iml = v; fw->iml_len = tl; break;
        case IWL_TLV_NUM_OF_CPU: if (tl >= 4) fw->num_cpu = rd32(v); break;
        case IWL_TLV_PAGING: if (tl >= 4) fw->paging_size = rd32(v); break;
        case IWL_TLV_API_CHANGES: if (tl >= 8 && rd32(v) < 4) fw->api[rd32(v)] = rd32(v + 4); break;
        case IWL_TLV_CAPA: if (tl >= 8 && rd32(v) < 4) fw->capa[rd32(v)] = rd32(v + 4); break;
        case IWL_TLV_FW_VERSION: if (tl >= 12) { fw->major = rd32(v); fw->minor = rd32(v + 4); } break;
        case IWL_TLV_N_SCAN_CH: if (tl >= 4) fw->n_scan_channels = rd32(v); break;
        default: break;
        }
        fw->ntlv++;
        off += 8 + ((tl + 3) & ~3u);
    }
    if (!fw->n_rt) { snprintf(err, errlen, "firmware sem seções de execução"); return false; }
    for (int i = 0; i < fw->n_rt; i++) fw->rt_bytes += fw->rt[i].len;
    return true;
}
