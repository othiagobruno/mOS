/* mOS — Intel Wi-Fi (iwlwifi): formato do firmware e registradores básicos */
#pragma once
#ifdef MOS_HOST_TEST
#include "host_shim.h"
#else
#include "kernel.h"
#endif

/* tipos de TLV do arquivo .ucode (iwl-fw-file.h) */
enum {
    IWL_TLV_SEC_RT = 19, IWL_TLV_SEC_INIT = 20, IWL_TLV_SEC_WOWLAN = 21, IWL_TLV_NUM_OF_CPU = 27,
    IWL_TLV_API_CHANGES = 29, IWL_TLV_CAPA = 30, IWL_TLV_PAGING = 32, IWL_TLV_N_SCAN_CH = 31,
    IWL_TLV_FW_VERSION = 36, IWL_TLV_IML = 52,
};

#define IWL_MAX_SEC 64
typedef struct { u32 offset; const u8 *data; u32 len; } iwl_sec_t;

typedef struct {
    char human[64];
    u32 ver, build, major, minor;
    iwl_sec_t rt[IWL_MAX_SEC], init[IWL_MAX_SEC];
    int n_rt, n_init, ntlv;
    const u8 *iml;
    u32 iml_len, num_cpu, paging_size, n_scan_channels;
    u32 api[4], capa[4];
    u64 rt_bytes;
} iwl_fw_t;

bool iwl_fw_parse(const u8 *d, u64 len, iwl_fw_t *fw, char *err, usize errlen);

/* registradores CSR (deslocamentos no BAR0) */
#define CSR_HW_IF_CONFIG_REG 0x000
#define CSR_INT              0x008
#define CSR_INT_MASK         0x00C
#define CSR_FH_INT_STATUS    0x010
#define CSR_RESET            0x020
#define CSR_GP_CNTRL         0x024
#define CSR_HW_REV           0x028
#define CSR_HW_RF_ID         0x09C
#define CSR_GIO_CHICKEN_BITS 0x100

#define CSR_HW_IF_CONFIG_REG_NIC_READY   (1u << 22)
#define CSR_HW_IF_CONFIG_REG_PREPARE     (1u << 27)
#define CSR_RESET_REG_FLAG_SW_RESET      (1u << 7)
#define CSR_GP_CNTRL_MAC_CLOCK_READY     (1u << 0)
#define CSR_GP_CNTRL_INIT_DONE           (1u << 2)
