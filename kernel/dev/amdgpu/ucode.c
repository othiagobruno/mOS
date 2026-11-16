/* mOS — cabeçalhos do microcódigo da AMD (kernel/dev/amdgpu/ucode.h). Lógica pura. */
#include "ucode.h"

enum {
    UC_E_SHORT = -1, UC_E_ALIGN = -2, UC_E_SIZE = -3, UC_E_HDR = -4, UC_E_VERSION = -5,
    UC_E_PAYLOAD = -6, UC_E_JT = -7, UC_E_RLC_LIST = -8, UC_E_KIND = -9,
};

const char *amd_ucode_error(int err) {
    switch (err) {
    case 0: return "ok";
    case UC_E_SHORT: return "arquivo menor que o cabeçalho";
    case UC_E_ALIGN: return "arquivo fora de alinhamento de 4 bytes";
    case UC_E_SIZE: return "tamanho declarado maior que o arquivo";
    case UC_E_HDR: return "tamanho do cabeçalho inválido";
    case UC_E_VERSION: return "versão de cabeçalho não suportada";
    case UC_E_PAYLOAD: return "microcódigo fora do arquivo";
    case UC_E_JT: return "tabela de saltos fora do microcódigo";
    case UC_E_RLC_LIST: return "lista de save/restore do RLC fora do arquivo";
    case UC_E_KIND: return "tipo de bloco desconhecido";
    default: return "erro desconhecido";
    }
}

static bool in_file(u32 off, u32 size, u32 total) { return off <= total && size <= total - off; }

int amd_ucode_parse(const u8 *d, usize len, int kind, amd_ucode_t *u) {
    memset(u, 0, sizeof *u);
    if (len < 32) return UC_E_SHORT;
    if ((usize)d & 3) return UC_E_ALIGN;
    u->size_bytes = amd_le32(d);
    u->header_size_bytes = amd_le32(d + 4);
    u->hdr_major = amd_le16(d + 8);  u->hdr_minor = amd_le16(d + 10);
    u->ip_major = amd_le16(d + 12);  u->ip_minor = amd_le16(d + 14);
    u->ucode_version = amd_le32(d + 16);
    u->ucode_size_bytes = amd_le32(d + 20);
    u->ucode_offset = amd_le32(d + 24);
    if (u->size_bytes > len) return UC_E_SIZE;
    if (u->header_size_bytes < 32 || u->header_size_bytes > u->size_bytes) return UC_E_HDR;
    u32 total = u->size_bytes;
    if (!in_file(u->ucode_offset, u->ucode_size_bytes, total) || (u->ucode_offset & 3) ||
        (kind != UC_KIND_GPU_INFO && ((u->ucode_size_bytes & 3) || !u->ucode_size_bytes)))
        return UC_E_PAYLOAD;
    u->ucode = (const u32 *)(d + u->ucode_offset);
    u->ucode_dwords = u->ucode_size_bytes / 4;
    const u8 *h = d + 32;                       /* fim do cabeçalho comum (crc32 em 28) */
    u32 hs = u->header_size_bytes;

    switch (kind) {
    case UC_KIND_GFX:
        if (u->hdr_major != 1 || hs < 44) return UC_E_VERSION;
        u->feature_version = amd_le32(h);
        u->jt_offset = amd_le32(h + 4);
        u->jt_size = amd_le32(h + 8);
        if (u->jt_size && (u->jt_offset > u->ucode_dwords || u->jt_size > u->ucode_dwords - u->jt_offset)) return UC_E_JT;
        return 0;
    case UC_KIND_SDMA:
        if (u->hdr_major != 1 || hs < 48) return UC_E_VERSION;
        u->feature_version = amd_le32(h);
        u->jt_offset = amd_le32(h + 8);
        u->jt_size = amd_le32(h + 12);
        if (u->jt_size && (u->jt_offset > u->ucode_dwords || u->jt_size > u->ucode_dwords - u->jt_offset)) return UC_E_JT;
        return 0;
    case UC_KIND_RLC: {
        if (u->hdr_major != 2 || hs < 104) return UC_E_VERSION;
        u->feature_version = amd_le32(h);
        u->jt_offset = amd_le32(h + 4);
        u->jt_size = amd_le32(h + 8);
        u->rlc_save_restore_offset = amd_le32(h + 12);
        u->rlc_csd_offset = amd_le32(h + 16);
        u->rlc_scratch_locations = amd_le32(h + 20);
        u->rlc_reg_restore_list_size = amd_le32(h + 24);
        u->rlc_reg_list_format_start = amd_le32(h + 28);
        u->rlc_reg_list_format_separate_start = amd_le32(h + 32);
        u->rlc_starting_offsets_start = amd_le32(h + 36);
        u32 fmt_sz = amd_le32(h + 40), fmt_off = amd_le32(h + 44);
        u32 lst_sz = amd_le32(h + 48), lst_off = amd_le32(h + 52);
        if (!in_file(fmt_off, fmt_sz, total) || !in_file(lst_off, lst_sz, total) || (fmt_off | lst_off | fmt_sz | lst_sz) & 3)
            return UC_E_RLC_LIST;
        u->rlc_reg_list_format = (const u32 *)(d + fmt_off); u->rlc_reg_list_format_dwords = fmt_sz / 4;
        u->rlc_reg_list = (const u32 *)(d + lst_off);        u->rlc_reg_list_dwords = lst_sz / 4;
        if (u->hdr_minor >= 1) {
            /* o raven2_rlc.bin declara v2.1 com header_size 104, mas os campos estão lá antes do payload */
            if (hs < 156 && u->ucode_offset < 156) return UC_E_VERSION;
            const u8 *x = h + 72;               /* depois dos campos "separate" da v2.0 */
            u->rlc_direct_reg_list_length = amd_le32(x);
            u32 c_sz = amd_le32(x + 12), c_off = amd_le32(x + 16);
            u32 g_sz = amd_le32(x + 28), g_off = amd_le32(x + 32);
            u32 s_sz = amd_le32(x + 44), s_off = amd_le32(x + 48);
            if (!in_file(c_off, c_sz, total) || !in_file(g_off, g_sz, total) || !in_file(s_off, s_sz, total))
                return UC_E_RLC_LIST;
            u->rlc_srl_cntl = d + c_off; u->rlc_srl_cntl_bytes = c_sz;
            u->rlc_srl_gpm = d + g_off;  u->rlc_srl_gpm_bytes = g_sz;
            u->rlc_srl_srm = d + s_off;  u->rlc_srl_srm_bytes = s_sz;
        }
        return 0;
    }
    case UC_KIND_GPU_INFO: {
        if (u->hdr_major != 1 || hs < 36) return UC_E_VERSION;
        if (u->ucode_size_bytes < 60) return UC_E_PAYLOAD;
        const u8 *g = d + u->ucode_offset;
        u32 *dst[] = {&u->gi_num_se, &u->gi_num_cu_per_sh, &u->gi_num_sh_per_se, &u->gi_num_rb_per_se,
                      &u->gi_num_tccs, &u->gi_num_gprs, &u->gi_max_gs_threads, &u->gi_gs_table_depth,
                      &u->gi_gsprim_buff_depth, &u->gi_parameter_cache_depth, &u->gi_double_offchip_lds_buf,
                      &u->gi_wave_size, &u->gi_max_waves_per_simd, &u->gi_max_scratch_slots_per_cu, &u->gi_lds_size};
        for (usize i = 0; i < ARRAY_LEN(dst); i++) *dst[i] = amd_le32(g + 4 * i);
        return 0;
    }
    default: return UC_E_KIND;
    }
}

int amd_raven_variant(u16 device, u32 rev_id) {
    if (rev_id >= 8) return AMD_RAVEN2;
    if (device == 0x15D8) return AMD_PICASSO;
    return AMD_RAVEN;
}

const char *amd_raven_fw_prefix(int variant) {
    switch (variant) {
    case AMD_RAVEN2: return "raven2";
    case AMD_PICASSO: return "picasso";
    default: return "raven";
    }
}

void amd_raven_rlc_name(int variant, u8 pci_rev, u32 smu_version, char *out, usize n) {
    bool am4 = (pci_rev >= 0xC8 && pci_rev <= 0xCF) || (pci_rev >= 0xD8 && pci_rev <= 0xDF);
    if (variant == AMD_PICASSO && am4) snprintf(out, n, "picasso_rlc_am4.bin");
    else if (variant == AMD_RAVEN && smu_version >= 0x41E2B) snprintf(out, n, "raven_kicker_rlc.bin");
    else snprintf(out, n, "%s_rlc.bin", amd_raven_fw_prefix(variant));
}
