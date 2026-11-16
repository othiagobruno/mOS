/* mOS — microcódigo da AMD (linux-firmware, licença Redistributable-amdgpu): cabeçalhos e validação.
 *
 * Todo arquivo começa com o cabeçalho comum (32 bytes, crc32 no fim, que o driver não confere); depois vem a parte de cada bloco:
 *   gfx v1.0  (PFP, ME, CE, MEC): + feature, jt_offset, jt_size              (dwords)
 *   rlc v2.x  : + listas de save/restore do RLC (v2.1: listas cntl/gpm/srm)
 *   sdma v1.0 : + feature, change, jt_offset, jt_size
 *   gpu_info v1.0: + versão do bloco; o payload é a tabela de configuração do GC
 * Família Raven: carga direta (o driver escreve o microcódigo nos registradores do bloco); não passa
 * pelo PSP. Lógica pura: o teste de host valida os blobs reais de firmware/amdgpu. */
#pragma once
#include "amdgpu_base.h"

enum {
    UC_KIND_GFX = 1,     /* PFP, ME, CE, MEC */
    UC_KIND_RLC,
    UC_KIND_SDMA,
    UC_KIND_GPU_INFO,
};

typedef struct {
    u32 size_bytes, header_size_bytes;
    u16 hdr_major, hdr_minor, ip_major, ip_minor;
    u32 ucode_version, ucode_size_bytes, ucode_offset;
    u32 feature_version;
    /* gfx e sdma: tabela de saltos (em dwords a partir do início do microcódigo) */
    u32 jt_offset, jt_size;
    const u32 *ucode;    /* aponta para dentro do arquivo; alinhado a 4 */
    u32 ucode_dwords;
    /* rlc v2.x */
    u32 rlc_save_restore_offset, rlc_csd_offset, rlc_scratch_locations;
    u32 rlc_reg_restore_list_size, rlc_reg_list_format_start, rlc_reg_list_format_separate_start;
    u32 rlc_starting_offsets_start;
    const u32 *rlc_reg_list_format; u32 rlc_reg_list_format_dwords;
    const u32 *rlc_reg_list;        u32 rlc_reg_list_dwords;
    /* rlc v2.1: listas de save/restore (cntl, gpm, srm) */
    u32 rlc_direct_reg_list_length;
    const u8 *rlc_srl_cntl; u32 rlc_srl_cntl_bytes;
    const u8 *rlc_srl_gpm;  u32 rlc_srl_gpm_bytes;
    const u8 *rlc_srl_srm;  u32 rlc_srl_srm_bytes;
    /* gpu_info v1.0 */
    u32 gi_num_se, gi_num_cu_per_sh, gi_num_sh_per_se, gi_num_rb_per_se, gi_num_tccs;
    u32 gi_num_gprs, gi_max_gs_threads, gi_gs_table_depth, gi_gsprim_buff_depth;
    u32 gi_parameter_cache_depth, gi_double_offchip_lds_buf, gi_wave_size;
    u32 gi_max_waves_per_simd, gi_max_scratch_slots_per_cu, gi_lds_size;
} amd_ucode_t;

/* 0 = válido; < 0 = motivo (para o log). O arquivo precisa estar alinhado a 4 bytes. */
int         amd_ucode_parse(const u8 *data, usize len, int kind, amd_ucode_t *out);
const char *amd_ucode_error(int err);

/* Variante do chip na família Raven (GC 9.1/9.2.2): decide o prefixo dos arquivos. */
enum { AMD_RAVEN = 1, AMD_PICASSO, AMD_RAVEN2 };
/* device = ID PCI; rev_id = campo STRAP_ATI_REV_ID do NBIO (RCC_DEV0_EPF0_STRAP0[27:24]).
 * Raven2 vem primeiro: é o rev_id >= 8, mesmo com o ID 15D8. */
int         amd_raven_variant(u16 device, u32 rev_id);
const char *amd_raven_fw_prefix(int variant);      /* "raven", "picasso", "raven2" */
/* Nome do arquivo do RLC (sem diretório). Picasso em soquete AM4 (rev PCI 0xC8..0xCF ou 0xD8..0xDF) usa
 * "picasso_rlc_am4.bin"; Raven com SMU >= 0x41E2B usa "raven_kicker_rlc.bin"; o resto, "<prefixo>_rlc.bin". */
void        amd_raven_rlc_name(int variant, u8 pci_rev, u32 smu_version, char *out, usize n);
