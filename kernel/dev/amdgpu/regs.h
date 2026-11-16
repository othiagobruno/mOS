/* mOS — registradores da família Raven (Raven, Picasso, Raven2) usados pelo driver AMD.
 *
 * Deslocamentos tirados dos headers asic_reg da AMD (gc_9_1, dcn_1_0, mmhub_1_0, nbio_7_0), que vêm com
 * esta licença:
 *
 *   Copyright (C) 2017 Advanced Micro Devices, Inc.
 *   Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 *   associated documentation files (the "Software"), to deal in the Software without restriction,
 *   including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *   and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 *   subject to the following conditions: The above copyright notice and this permission notice shall be
 *   included in all copies or substantial portions of the Software.
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.
 *
 * Endereço em bytes no BAR5 = (base do segmento do IP + deslocamento) * 4. As bases são as da tabela de
 * IP discovery do Picasso do Thiago (docs/gpu-amd.md), iguais em toda a família Raven. */
#pragma once

#define AMD_SEG(base, off)   (((base) + (off)) * 4u)

/* bases (dwords) */
#define AMD_NBIF_BASE2   0x0D20u
#define AMD_MMHUB_BASE0  0x1A000u
#define AMD_DCN_BASE2    0x34C0u
#define AMD_GC_BASE0     0x2000u
#define AMD_GC_BASE1     0xA000u

#define NBIF_REG(off)    AMD_SEG(AMD_NBIF_BASE2, off)
#define MMHUB_REG(off)   AMD_SEG(AMD_MMHUB_BASE0, off)
#define DCN_REG(off)     AMD_SEG(AMD_DCN_BASE2, off)

/* NBIO 7.0 */
#define R_RCC_DEV0_EPF0_STRAP0        NBIF_REG(0x000F)
#define   STRAP0_ATI_REV_ID_SHIFT     24
#define   STRAP0_ATI_REV_ID_MASK      0x0F000000u

/* MMHUB 1.0: onde a VRAM aparece no espaço de endereços da GPU (unidades de 16 MiB) */
#define R_MC_VM_FB_OFFSET             MMHUB_REG(0x0817)   /* endereço físico da VRAM (carve-out da APU) */
#define R_MC_VM_FB_LOCATION_BASE      MMHUB_REG(0x082C)
#define R_MC_VM_FB_LOCATION_TOP       MMHUB_REG(0x082D)
#define   MC_FB_MASK                  0x00FFFFFFu

/* DCN 1.0 — OTG (gerador de timing), 4 instâncias, passo 0x80 dwords */
#define OTG_STRIDE                    0x80u
#define R_OTG_H_TOTAL(i)              DCN_REG(0x1B2A + (i) * OTG_STRIDE)
#define R_OTG_H_BLANK_START_END(i)    DCN_REG(0x1B2B + (i) * OTG_STRIDE)
#define R_OTG_H_SYNC_A(i)             DCN_REG(0x1B2C + (i) * OTG_STRIDE)
#define R_OTG_H_SYNC_A_CNTL(i)        DCN_REG(0x1B2D + (i) * OTG_STRIDE)
#define R_OTG_V_TOTAL(i)              DCN_REG(0x1B2F + (i) * OTG_STRIDE)
#define R_OTG_V_BLANK_START_END(i)    DCN_REG(0x1B36 + (i) * OTG_STRIDE)
#define R_OTG_V_SYNC_A(i)             DCN_REG(0x1B37 + (i) * OTG_STRIDE)
#define R_OTG_V_SYNC_A_CNTL(i)        DCN_REG(0x1B38 + (i) * OTG_STRIDE)
#define R_OTG_CONTROL(i)              DCN_REG(0x1B41 + (i) * OTG_STRIDE)
#define R_OTG_INTERLACE_CONTROL(i)    DCN_REG(0x1B44 + (i) * OTG_STRIDE)
#define R_OTG_STATUS(i)               DCN_REG(0x1B49 + (i) * OTG_STRIDE)
#define R_OTG_STATUS_POSITION(i)      DCN_REG(0x1B4A + (i) * OTG_STRIDE)
#define R_OTG_STATUS_FRAME_COUNT(i)   DCN_REG(0x1B4C + (i) * OTG_STRIDE)
#define R_OTG_MASTER_UPDATE_LOCK(i)   DCN_REG(0x1B8D + (i) * OTG_STRIDE)
#define   OTG_MASTER_EN               0x1u
#define   OTG_V_BLANK                 0x1u
#define   OTG_VERT_COUNT_MASK         0x7FFFu
#define   OTG_FRAME_COUNT_MASK        0x00FFFFFFu
#define   OTG_TIMING_MASK             0x7FFFu
#define   OTG_SYNC_POL_NEG            0x1u     /* OTG_*_SYNC_A_POL = 1: sincronismo negativo */
#define   OTG_UPDATE_LOCK             0x1u
#define   OTG_UPDATE_LOCK_STATUS      0x100u

/* DCN 1.0 — HUBP (busca do plano na memória), 4 instâncias, passo 0xC4 dwords */
#define HUBP_STRIDE                   0xC4u
#define R_DCSURF_SURFACE_CONFIG(i)    DCN_REG(0x0559 + (i) * HUBP_STRIDE)
#define R_DCSURF_TILING_CONFIG(i)     DCN_REG(0x055B + (i) * HUBP_STRIDE)
#define R_DCSURF_PRI_VIEWPORT_START(i) DCN_REG(0x055C + (i) * HUBP_STRIDE)
#define R_DCSURF_PRI_VIEWPORT_DIM(i)  DCN_REG(0x055D + (i) * HUBP_STRIDE)
#define R_DCHUBP_CNTL(i)              DCN_REG(0x0566 + (i) * HUBP_STRIDE)
#define R_DCSURF_SURFACE_PITCH(i)     DCN_REG(0x057B + (i) * HUBP_STRIDE)
#define R_DCSURF_PRIMARY_ADDR(i)      DCN_REG(0x057D + (i) * HUBP_STRIDE)
#define R_DCSURF_PRIMARY_ADDR_HI(i)   DCN_REG(0x057E + (i) * HUBP_STRIDE)
#define R_DCSURF_FLIP_CONTROL(i)      DCN_REG(0x058E + (i) * HUBP_STRIDE)
#define R_DCSURF_EARLIEST_INUSE(i)    DCN_REG(0x0597 + (i) * HUBP_STRIDE)
#define R_DCSURF_EARLIEST_INUSE_HI(i) DCN_REG(0x0598 + (i) * HUBP_STRIDE)
#define R_HUBPRET_CONTROL(i)          DCN_REG(0x05E0 + (i) * HUBP_STRIDE)
#define   SURFACE_PIXEL_FORMAT_MASK   0x7Fu
#define   SURFACE_FMT_ARGB8888        8u       /* ARGB e ABGR 8:8:8:8; a ordem sai do crossbar */
#define   TILING_SW_MODE_MASK         0x1Fu    /* 0 = linear */
#define   VIEWPORT_W_MASK             0x3FFFu
#define   VIEWPORT_H_SHIFT            16
#define   PITCH_MASK                  0x3FFFu  /* em pixels, menos 1 */
#define   HUBP_BLANK_EN               0x1u
#define   HUBP_DISABLE                0x4u
#define   FLIP_TYPE_IMMEDIATE         0x2u     /* 0 = troca no vblank */
#define   FLIP_PENDING                0x80000000u
#define   XBAR_CB_B_SHIFT             20
#define   XBAR_CR_R_SHIFT             22
#define   XBAR_MASK                   0x00F00000u

/* DCN 1.0 — HPD (presença de monitor), 6 instâncias, passo 8 dwords */
#define R_DC_HPD_INT_STATUS(i)        DCN_REG(0x1F14 + (i) * 8u)
#define   DC_HPD_SENSE                0x2u

#define AMD_NUM_OTG   4
#define AMD_NUM_HPD   6
#define AMD_MMIO_SIZE 0x80000u        /* 512 KiB do BAR5 */

/* ------------------------------------------------------------ motores (smu.c / gmc.c / sdma.c em engine.c) */
#define AMD_MP1_BASE0    0x16000u
#define AMD_SDMA0_BASE0  0x1260u
#define MP1_REG(off)     AMD_SEG(AMD_MP1_BASE0, off)
#define SDMA_REG(off)    AMD_SEG(AMD_SDMA0_BASE0, off)

/* SMU 10 (MP1): caixa de mensagens */
#define R_MP1_C2PMSG_66               MP1_REG(0x0282)   /* mensagem */
#define R_MP1_C2PMSG_82               MP1_REG(0x0292)   /* argumento / resposta */
#define R_MP1_C2PMSG_90               MP1_REG(0x029A)   /* estado: 0 ocupado, 1 ok, outro = erro */
#define SMU_MSG_TEST                  0x01u
#define SMU_MSG_GET_VERSION           0x02u
#define SMU_MSG_POWER_UP_GFX          0x06u
#define SMU_MSG_DISABLE_GFXOFF        0x08u
#define SMU_MSG_POWER_DOWN_SDMA       0x0Du
#define SMU_MSG_POWER_UP_SDMA         0x0Eu
#define SMU_RESP_OK                   0x01u

/* MMHUB 1.0: aberturas, TLB L1, cache L2, contexto 0 (GART) e invalidação */
#define R_MC_VM_AGP_TOP               MMHUB_REG(0x082E)
#define R_MC_VM_AGP_BOT               MMHUB_REG(0x082F)
#define R_MC_VM_AGP_BASE              MMHUB_REG(0x0830)
#define R_MC_VM_SYS_APERTURE_LOW      MMHUB_REG(0x0831)  /* >> 18 */
#define R_MC_VM_SYS_APERTURE_HIGH     MMHUB_REG(0x0832)
#define R_MC_VM_SYS_APERTURE_DEF_LSB  MMHUB_REG(0x0818)  /* >> 12, físico */
#define R_MC_VM_SYS_APERTURE_DEF_MSB  MMHUB_REG(0x0819)
#define R_MC_VM_MX_L1_TLB_CNTL        MMHUB_REG(0x0833)
#define R_VM_L2_CNTL                  MMHUB_REG(0x0680)
#define R_VM_L2_CNTL2                 MMHUB_REG(0x0681)
#define R_VM_L2_CNTL3                 MMHUB_REG(0x0682)
#define R_VM_L2_CNTL4                 MMHUB_REG(0x0697)
#define R_VM_L2_FAULT_DEF_LO          MMHUB_REG(0x068E)
#define R_VM_L2_FAULT_DEF_HI          MMHUB_REG(0x068F)
#define R_VM_L2_CTX1_IDENT_LOW_LO     MMHUB_REG(0x0691)
#define R_VM_L2_CTX1_IDENT_LOW_HI     MMHUB_REG(0x0692)
#define R_VM_L2_CTX1_IDENT_HIGH_LO    MMHUB_REG(0x0693)
#define R_VM_L2_CTX1_IDENT_HIGH_HI    MMHUB_REG(0x0694)
#define R_VM_L2_CTX_IDENT_PHYS_LO     MMHUB_REG(0x0695)
#define R_VM_L2_CTX_IDENT_PHYS_HI     MMHUB_REG(0x0696)
#define R_VM_CONTEXT0_CNTL            MMHUB_REG(0x06C0)
#define R_VM_CTX0_PT_BASE_LO          MMHUB_REG(0x072B)
#define R_VM_CTX0_PT_BASE_HI          MMHUB_REG(0x072C)
#define R_VM_CTX0_PT_START_LO         MMHUB_REG(0x074B)
#define R_VM_CTX0_PT_START_HI         MMHUB_REG(0x074C)
#define R_VM_CTX0_PT_END_LO           MMHUB_REG(0x076B)
#define R_VM_CTX0_PT_END_HI           MMHUB_REG(0x076C)
#define R_VM_INV_ENG17_SEM            MMHUB_REG(0x06E2)
#define R_VM_INV_ENG17_REQ            MMHUB_REG(0x06F4)
#define R_VM_INV_ENG17_ACK            MMHUB_REG(0x0706)
#define   L1_TLB_ENABLE               0x1u
#define   L1_TLB_SYS_ACCESS_MODE_3    (3u << 3)
#define   L1_TLB_SYS_APERTURE_UNMAPPED 0x20u
#define   L1_TLB_ADV_DRIVER_MODEL     0x40u
#define   L1_TLB_MTYPE_MASK           0x1800u
#define   L1_TLB_MTYPE_UC             (3u << 11)
#define   L1_TLB_ATC_EN               0x2000u
#define   L2_ENABLE_CACHE             0x1u
#define   L2_ENABLE_FRAG              0x2u
#define   L2_PDE0_TAG_GEN_MODE        0x100u
#define   L2_PDE_FAULT_CLASS          0x40000u
#define   L2_CTX1_IDENT_MODE_MASK     0x180000u
#define   L2_CTX1_IDENT_MODE_1        (1u << 19)
#define   L2_IDENT_FRAG_MASK          0x3E00000u
#define   L2_CNTL2_INV_L1_TLBS        0x1u
#define   L2_CNTL2_INV_L2_CACHE       0x2u
#define   L2_CNTL3_DEFAULT            0x80100007u
#define   L2_CNTL3_BANK_MASK          0x3Fu
#define   L2_CNTL3_BIGK_SHIFT         15
#define   L2_CNTL3_BIGK_MASK          0xF8000u
#define   L2_CNTL4_DEFAULT            0x000000C1u
#define   L2_CNTL4_PHYS_MASK          0xC0u
#define   CTX0_ENABLE                 0x1u
#define   CTX0_DEPTH_MASK             0x6u
#define   CTX0_BLOCK_MASK             0x78u
#define   CTX0_RETRY_FAULT            0x80u
#define   INV_REQ_VMID0               0x1u
#define   INV_REQ_L2_PTES             0x40000u
#define   INV_REQ_L2_PDE0             0x80000u
#define   INV_REQ_L2_PDE1             0x100000u
#define   INV_REQ_L2_PDE2             0x200000u
#define   INV_REQ_L1_PTES             0x400000u

/* PTE da GART (GFX9) */
#define AMD_PTE_VALID     (1ull << 0)
#define AMD_PTE_SYSTEM    (1ull << 1)
#define AMD_PTE_SNOOPED   (1ull << 2)
#define AMD_PTE_EXEC      (1ull << 4)
#define AMD_PTE_READ      (1ull << 5)
#define AMD_PTE_WRITE     (1ull << 6)
#define AMD_PTE_MTYPE_UC  (3ull << 57)
#define AMD_PTE_ADDR_MASK 0x0000FFFFFFFFF000ull

/* SDMA 4.1 */
#define R_SDMA_UCODE_ADDR             SDMA_REG(0x0000)
#define R_SDMA_UCODE_DATA             SDMA_REG(0x0001)
#define R_SDMA_POWER_CNTL             SDMA_REG(0x001A)
#define R_SDMA_CLK_CTRL               SDMA_REG(0x001B)
#define R_SDMA_CNTL                   SDMA_REG(0x001C)
#define R_SDMA_CHICKEN_BITS           SDMA_REG(0x001D)
#define R_SDMA_GB_ADDR_CONFIG         SDMA_REG(0x001E)
#define R_SDMA_GB_ADDR_CONFIG_READ    SDMA_REG(0x001F)
#define R_SDMA_SEM_WAIT_FAIL_TIMER    SDMA_REG(0x0021)
#define R_SDMA_STATUS_REG             SDMA_REG(0x0025)
#define R_SDMA_F32_CNTL               SDMA_REG(0x002A)
#define R_SDMA_UTCL1_WATERMK          SDMA_REG(0x003D)
#define R_SDMA_UTCL1_PAGE             SDMA_REG(0x0048)
#define R_SDMA_GFX_RB_CNTL            SDMA_REG(0x0080)
#define R_SDMA_GFX_RB_BASE            SDMA_REG(0x0081)
#define R_SDMA_GFX_RB_BASE_HI         SDMA_REG(0x0082)
#define R_SDMA_GFX_RB_RPTR            SDMA_REG(0x0083)
#define R_SDMA_GFX_RB_RPTR_HI         SDMA_REG(0x0084)
#define R_SDMA_GFX_RB_WPTR            SDMA_REG(0x0085)
#define R_SDMA_GFX_RB_WPTR_HI         SDMA_REG(0x0086)
#define R_SDMA_GFX_RB_WPTR_POLL_CNTL  SDMA_REG(0x0087)
#define R_SDMA_GFX_RB_RPTR_ADDR_HI    SDMA_REG(0x0088)
#define R_SDMA_GFX_RB_RPTR_ADDR_LO    SDMA_REG(0x0089)
#define R_SDMA_GFX_IB_CNTL            SDMA_REG(0x008A)
#define R_SDMA_GFX_DOORBELL           SDMA_REG(0x0092)
#define R_SDMA_GFX_MINOR_PTR_UPDATE   SDMA_REG(0x00B5)
#define R_SDMA_RLC0_RB_WPTR_POLL_CNTL SDMA_REG(0x0147)
#define R_SDMA_RLC0_IB_CNTL           SDMA_REG(0x014A)
#define R_SDMA_RLC1_RB_WPTR_POLL_CNTL SDMA_REG(0x01A7)
#define R_SDMA_RLC1_IB_CNTL           SDMA_REG(0x01AA)
#define   SDMA_F32_HALT               0x1u
#define   SDMA_RB_ENABLE              0x1u
#define   SDMA_RB_SIZE_SHIFT          1
#define   SDMA_RB_SIZE_MASK           0x7Eu
#define   SDMA_RPTR_WB_ENABLE         0x1000u
#define   SDMA_IB_ENABLE              0x1u
#define   SDMA_CNTL_UTC_L1            0x2u
#define   SDMA_STATUS_IDLE            0x1u
#define   SDMA_DOORBELL_ENABLE        0x10000000u
#define   SDMA_WPTR_POLL_F32          0x4u
/* pacotes */
#define SDMA_OP_NOP        0u
#define SDMA_OP_COPY       1u
#define SDMA_OP_WRITE      2u
#define SDMA_OP_FENCE      5u
#define SDMA_OP_TRAP       6u
#define SDMA_OP_CONST_FILL 11u
#define SDMA_HDR(op, sub)  ((u32)(op) | (u32)(sub) << 8)
#define SDMA_COPY_MAX      (1u << 22)       /* bytes por pacote COPY_LINEAR / CONST_FILL */
