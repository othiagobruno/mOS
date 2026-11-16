/* mOS — contas do TD isócrono do xHCI (rev. 1.2, 4.11.2.3 e 4.14.1), sem MMIO: testado por tools/usb_iso_host.c
 *
 * Um TD isócrono = um intervalo de serviço (ESIT): até (burst+1)·(mult+1) pacotes de até MaxPacketSize.
 * TDPC = ⌈len / mps⌉ (no mínimo 1, também para TD de 0 bytes). Em SuperSpeed: TBC = ⌈TDPC / (burst+1)⌉ − 1 e
 * TLBPC = pacotes do último burst − 1; abaixo de SuperSpeed (ou xHCI < 1.0) TBC = 0 e TLBPC = TDPC − 1. */
#pragma once
#ifdef MOS_HOST_TEST
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
#else
#include "types.h"
#endif

static inline void usb_iso_td_fields(u32 len, u16 mps, u8 burst, bool superspeed, bool xhci10, u32 *tbc, u32 *tlbpc) {
    u32 tdpc = mps ? (len + mps - 1) / mps : 1;
    if (!tdpc) tdpc = 1;
    if (!superspeed || !xhci10) { *tbc = 0; *tlbpc = tdpc - 1; return; }
    u32 per = (u32)burst + 1;
    *tbc = (tdpc + per - 1) / per - 1;
    u32 res = tdpc % per;
    *tlbpc = res ? res - 1 : burst;
}

/* Max ESIT Payload (bytes por intervalo de serviço): SS pelo wBytesPerInterval do companion; HS/FS por mps·(burst+1) */
static inline u32 usb_iso_esit(u16 mps, u8 burst, u8 mult, u16 ss_bytes_per_interval, bool superspeed) {
    if (superspeed) return ss_bytes_per_interval ? ss_bytes_per_interval : (u32)mps * (burst + 1u) * (mult + 1u);
    return (u32)mps * (burst + 1u);
}

/* bits do Isoch TRB (6.4.1.3): SIA 31, TBC 8:7, TLBPC 19:16, IOC 5, ISP 2 */
static inline u32 usb_iso_trb_d3(u32 tbc, u32 tlbpc, bool ioc, u32 type_iso) {
    return (1u << 31) | ((tlbpc & 0xF) << 16) | (type_iso << 10) | ((tbc & 3) << 7) | (ioc ? (1u << 5) : 0) | (1u << 2);
}
