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

/* DCN 1.0 — motor de I2C (DDC1..6) */
#define AMD_DCN_BASE1                 0x00C0u
#define DCN1_REG(off)                 AMD_SEG(AMD_DCN_BASE1, off)
#define R_DC_I2C_CONTROL              DCN_REG(0x1E98)
#define R_DC_I2C_ARBITRATION          DCN_REG(0x1E99)
#define R_DC_I2C_SW_STATUS            DCN_REG(0x1E9B)
#define R_DC_I2C_DDC_HW_STATUS(n)     DCN_REG(0x1E9C + (n))     /* n = 0..5 (DDC1..6) */
#define R_DC_I2C_DDC_SPEED(n)         DCN_REG(0x1EA2 + 2 * (n))
#define R_DC_I2C_DDC_SETUP(n)         DCN_REG(0x1EA3 + 2 * (n))
#define R_DC_I2C_TRANSACTION(t)       DCN_REG(0x1EAE + (t))     /* t = 0..3 */
#define R_DC_I2C_DATA                 DCN_REG(0x1EB2)
#define R_MICROSECOND_TIME_BASE_DIV   DCN1_REG(0x007B)
#define   I2C_GO                      0x1u
#define   I2C_SOFT_RESET              0x2u
#define   I2C_SEND_RESET              0x4u
#define   I2C_SW_STATUS_RESET         0x8u
#define   I2C_DDC_SELECT_SHIFT        8
#define   I2C_DDC_SELECT_MASK         0x700u
#define   I2C_TRANS_COUNT_SHIFT       20
#define   I2C_TRANS_COUNT_MASK        0x300000u
#define   I2C_ARB_STATUS_SHIFT        2
#define   I2C_ARB_STATUS_MASK         0xCu
#define   I2C_ARB_NO_QUEUED_SW_GO     0x10u
#define   I2C_ARB_SW_USE_REQ          0x100000u
#define   I2C_ARB_SW_DONE             0x200000u
#define   I2C_ST_IDLE                 0u
#define   I2C_ST_USED_BY_SW           1u
#define   I2C_ST_USED_BY_HW           2u
#define   I2C_SW_STATUS_MASK          0x3u
#define   I2C_SW_DONE                 0x4u
#define   I2C_SW_ABORTED              0x10u
#define   I2C_SW_TIMEOUT              0x20u
#define   I2C_SW_NACK                 0x100u
#define   I2C_SETUP_DATA_DRIVE_EN     0x1u
#define   I2C_SETUP_DATA_DRIVE_SEL    0x2u
#define   I2C_SETUP_ENABLE            0x40u
#define   I2C_SETUP_CLK_DRIVE_EN      0x80u
#define   I2C_SETUP_DELAYS_MASK       0x00FFFF00u
#define   I2C_SETUP_TIME_LIMIT_SHIFT  24
#define   I2C_SPEED_THRESHOLD_MASK    0x3u
#define   I2C_SPEED_SS_TIMING_SHIFT   8
#define   I2C_SPEED_SS_TIMING_MASK    0x300u
#define   I2C_SPEED_PRESCALE_SHIFT    16
#define   I2C_TR_RW                   0x1u          /* 1 = leitura */
#define   I2C_TR_STOP_ON_NACK         0x100u
#define   I2C_TR_START                0x1000u
#define   I2C_TR_STOP                 0x2000u
#define   I2C_TR_COUNT_SHIFT          16
#define   I2C_TR_COUNT_MASK           0x03FF0000u
#define   I2C_DATA_RW                 0x1u          /* 1 = ler o buffer */
#define   I2C_DATA_SHIFT              8
#define   I2C_DATA_MASK               0xFF00u
#define   I2C_INDEX_SHIFT             16
#define   I2C_INDEX_WRITE             0x80000000u
#define   USEC_BASE_DIV_MASK          0x7Fu
#define   XTAL_REF_DIV_SHIFT          8
#define   XTAL_REF_DIV_MASK           0x7F00u
#define AMD_I2C_BUFFER   144u
#define AMD_NUM_DDC      6

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
/* contextos 1..15 (VMIDs dos processos): CNTL a cada dword, endereços a cada 2 dwords a partir do contexto 0 */
#define R_VM_CTX_CNTL(n)              (R_VM_CONTEXT0_CNTL + 4u * (n))
#define R_VM_CTX_PT_BASE_LO(n)        (R_VM_CTX0_PT_BASE_LO + 8u * (n))
#define R_VM_CTX_PT_BASE_HI(n)        (R_VM_CTX0_PT_BASE_HI + 8u * (n))
#define R_VM_CTX_PT_START_LO(n)       (R_VM_CTX0_PT_START_LO + 8u * (n))
#define R_VM_CTX_PT_START_HI(n)       (R_VM_CTX0_PT_START_HI + 8u * (n))
#define R_VM_CTX_PT_END_LO(n)         (R_VM_CTX0_PT_END_LO + 8u * (n))
#define R_VM_CTX_PT_END_HI(n)         (R_VM_CTX0_PT_END_HI + 8u * (n))
#define   CTX_DEPTH_4LEVEL            (3u << 1)      /* PDB2 -> PDB1 -> PDB0 -> PTB, blocos de 512 entradas */
#define   CTX_FAULT_DEFAULTS          0x00555400u    /* range, dummy, PDE0, valid, read, write, exec: página de rascunho */
#define   CTX_FAULT_INTERRUPTS        0x002AAA00u    /* e cada uma delas avisa pelo IH */
#define R_VM_L2_PROT_FAULT_CNTL2      MMHUB_REG(0x0688)
#define R_VM_L2_PROT_FAULT_STATUS     MMHUB_REG(0x068B)
#define   PROT_FAULT_CLEAR_STATUS     0x1u
#define   AMD_VM_MAX_PFN              (1ull << 36)   /* 48 bits de VA */
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
#define   INV_REQ_ALL_LEVELS          0x7C0000u      /* L2 PTEs, PDE0/1/2 e L1 */
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
#define AMD_PTE_MTYPE_MASK (3ull << 57)                /* 0 = NC: o padrão do amdgpu para APU */
#define AMD_PDE_VALID     (1ull << 0)
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

/* ------------------------------------------------------------ interrupções (irq.c) */
#define AMD_OSSSYS_BASE0 0x10A0u
#define OSS_REG(off)     AMD_SEG(AMD_OSSSYS_BASE0, off)
#define R_IH_RB_CNTL                  OSS_REG(0x0080)
#define R_IH_RB_BASE                  OSS_REG(0x0081)
#define R_IH_RB_BASE_HI               OSS_REG(0x0082)
#define R_IH_RB_RPTR                  OSS_REG(0x0083)
#define R_IH_RB_WPTR                  OSS_REG(0x0084)
#define R_IH_RB_WPTR_ADDR_HI          OSS_REG(0x0085)
#define R_IH_RB_WPTR_ADDR_LO          OSS_REG(0x0086)
#define R_IH_DOORBELL_RPTR            OSS_REG(0x0087)
#define   IH_RB_ENABLE                0x1u
#define   IH_RB_SIZE_SHIFT            1
#define   IH_RB_SIZE_MASK             0x3Eu
#define   IH_RB_GPU_TS_ENABLE         0x80u
#define   IH_WPTR_WB_ENABLE           0x100u
#define   IH_WPTR_OVERFLOW_ENABLE     0x10000u
#define   IH_ENABLE_INTR              0x20000u
#define   IH_MC_SNOOP                 0x100000u
#define   IH_RPTR_REARM               0x200000u
#define   IH_MC_RO                    0x400000u
#define   IH_MC_VMID_MASK             0x0F000000u
#define   IH_MC_SPACE_MASK            0x70000000u
#define   IH_MC_SPACE_BUS             (1u << 28)     /* endereço de barramento (físico, sem a MMHUB) */
#define   IH_WPTR_OVERFLOW_CLEAR      0x80000000u
#define   IH_WPTR_RB_OVERFLOW         0x1u
#define   IH_WPTR_OFFSET_MASK         0x3FFFCu
#define   IH_DOORBELL_ENABLE          0x10000000u
#define R_NBIO_INTERRUPT_CNTL         NBIF_REG(0x00F1)
#define R_NBIO_INTERRUPT_CNTL2        NBIF_REG(0x00F2)  /* página de leitura falsa >> 8 */
#define   NBIO_IH_DUMMY_RD_OVERRIDE   0x1u
#define   NBIO_IH_REQ_NONSNOOP_EN     0x8u
#define R_DC_HPD_INT_CONTROL(i)       DCN_REG(0x1F15 + (i) * 8u)
#define   DC_HPD_INT_STATUS           0x1u
#define   DC_HPD_INT_ACK              0x1u
#define   DC_HPD_INT_POLARITY         0x100u    /* 1 = interrompe quando o sinal sobe (monitor plugado) */
#define   DC_HPD_INT_EN               0x10000u
#define   SDMA_CNTL_TRAP_ENABLE       0x1u
/* entrada do IH: 8 dwords. dw0 = cliente[7:0] | fonte[15:8] | anel[23:16] | vmid[27:24]; dw4.. = dados */
#define IH_CLIENT_DCE     0x04u
#define IH_CLIENT_SDMA0   0x08u
#define IH_SRC_SDMA_TRAP  224u
#define IH_SRC_DC_HPD     9u          /* contexto (dw4) 0..5 = HPD1..6 */
#define AMD_IH_RING_BYTES (64u * 1024u)

/* ------------------------------------------------------------ GFX 9.1 (gfx.c) */
/* Offsets do gc_9_0_offset.h (o mesmo conjunto que o amdgpu usa para a família Raven). Segmento 0 = 0x2000,
 * segmento 1 = 0xA000; os valores abaixo já são o endereço absoluto em dwords. */
#define GC_REG(dw)       ((u32)(dw) * 4u)
/* GFXHUB 1.0: mesmos registradores da MMHUB, em outro lugar (bloco VM +0x1C0, bloco MC +0x154 sobre a base do GC) */
#define GFXHUB_VM(r)     ((u32)(r) - (AMD_MMHUB_BASE0 - AMD_GC_BASE0 - 0x1C0u) * 4u)
#define GFXHUB_MC(r)     ((u32)(r) - (AMD_MMHUB_BASE0 - AMD_GC_BASE0 - 0x154u) * 4u)
#define R_CB_HW_CONTROL                      GC_REG(0x02680)
#define R_CPC_UTCL1_CNTL                     GC_REG(0x0303D)
#define R_CPF_UTCL1_CNTL                     GC_REG(0x0303E)
#define R_CPG_UTCL1_CNTL                     GC_REG(0x0303C)
#define R_DB_DEBUG2                          GC_REG(0x0260D)
#define R_GB_GPU_ID                          GC_REG(0x02640)
#define R_IA_UTCL1_CNTL                      GC_REG(0x02246)
#define R_PA_SC_BINNER_EVENT_CNTL_3          GC_REG(0x022CF)
#define R_PA_SC_ENHANCE                      GC_REG(0x022FC)
#define R_PA_SC_LINE_STIPPLE_STATE           GC_REG(0x0C281)
#define R_RLC_GPM_UTCL1_CNTL_0               GC_REG(0x0ECB2)
#define R_RLC_GPM_UTCL1_CNTL_1               GC_REG(0x0ECB3)
#define R_RLC_GPM_UTCL1_CNTL_2               GC_REG(0x0ECB4)
#define R_RLC_PREWALKER_UTCL1_CNTL           GC_REG(0x0ECCD)
#define R_RLC_SPM_UTCL1_CNTL                 GC_REG(0x0ECB5)
#define R_TA_CNTL_AUX                        GC_REG(0x02542)
#define R_TCP_CHAN_STEER_HI                  GC_REG(0x02B04)
#define R_TCP_CHAN_STEER_LO                  GC_REG(0x02B03)
#define R_VGT_CACHE_INVALIDATION             GC_REG(0x02231)
#define R_VGT_GS_MAX_WAVE_ID                 GC_REG(0x02269)
#define R_WD_UTCL1_CNTL                      GC_REG(0x02243)
#define R_CP_MEC1_F32_INT_DIS                GC_REG(0x030BD)
#define R_CP_MEC2_F32_INT_DIS                GC_REG(0x030BE)
#define R_CP_DEBUG                           GC_REG(0x0307F)
#define R_CB_HW_CONTROL_3                    GC_REG(0x02683)
#define R_GB_ADDR_CONFIG                     GC_REG(0x0263E)
#define R_GB_ADDR_CONFIG_READ                GC_REG(0x02642)
#define R_PA_SC_ENHANCE_1                    GC_REG(0x022FD)
#define R_PA_SC_MODE_CNTL_1                  GC_REG(0x0A293)
#define R_RMI_UTCL1_CNTL2                    GC_REG(0x0278C)
#define R_TD_CNTL                            GC_REG(0x02525)
#define R_CB_DCC_CONFIG                      GC_REG(0x02688)
#define R_CB_HW_CONTROL_2                    GC_REG(0x02682)
#define R_CP_SD_CNTL                         GC_REG(0x031B7)
#define R_GRBM_CAM_INDEX                     GC_REG(0x0FA04)
#define R_GRBM_CAM_DATA                      GC_REG(0x0FA05)
#define R_GRBM_CNTL                          GC_REG(0x02000)
#define R_GRBM_GFX_CNTL                      GC_REG(0x02022)
#define R_GRBM_GFX_INDEX                     GC_REG(0x0C200)
#define R_GRBM_STATUS                        GC_REG(0x02004)
#define R_GRBM_SOFT_RESET                    GC_REG(0x02008)
#define R_SH_MEM_CONFIG                      GC_REG(0x0230D)
#define R_SH_MEM_BASES                       GC_REG(0x0230A)
#define R_GDS_VMID0_BASE                     GC_REG(0x03300)
#define R_GDS_VMID0_SIZE                     GC_REG(0x03301)
#define R_GDS_GWS_VMID0                      GC_REG(0x03320)
#define R_GDS_OA_VMID0                       GC_REG(0x03330)
#define R_CC_GC_SHADER_ARRAY_CONFIG          GC_REG(0x0226F)
#define R_GC_USER_SHADER_ARRAY_CONFIG        GC_REG(0x02270)
#define R_RLC_CNTL                           GC_REG(0x0EC00)
#define R_CP_INT_CNTL_RING0                  GC_REG(0x0306A)
#define R_RLC_SERDES_CU_MASTER_BUSY          GC_REG(0x0EC61)
#define R_RLC_SERDES_NONCU_MASTER_BUSY       GC_REG(0x0EC62)
#define R_RLC_CGCG_CGLS_CTRL                 GC_REG(0x0EC49)
#define R_RLC_CSIB_ADDR_HI                   GC_REG(0x0ECA3)
#define R_RLC_CSIB_ADDR_LO                   GC_REG(0x0ECA2)
#define R_RLC_CSIB_LENGTH                    GC_REG(0x0ECA4)
#define R_RLC_SRM_CNTL                       GC_REG(0x0EC80)
#define R_RLC_SRM_ARAM_ADDR                  GC_REG(0x0EC83)
#define R_RLC_SRM_ARAM_DATA                  GC_REG(0x0EC84)
#define R_RLC_GPM_SCRATCH_ADDR               GC_REG(0x0EC6C)
#define R_RLC_GPM_SCRATCH_DATA               GC_REG(0x0EC6D)
#define R_RLC_SRM_INDEX_CNTL_ADDR_0          GC_REG(0x0EC8B)
#define R_RLC_SRM_INDEX_CNTL_DATA_0          GC_REG(0x0EC93)
#define R_RLC_GPM_UCODE_ADDR                 GC_REG(0x0F83C)
#define R_RLC_GPM_UCODE_DATA                 GC_REG(0x0F83D)
#define R_RLC_LB_THR_CONFIG_1                GC_REG(0x0ECBF)
#define R_RLC_LB_THR_CONFIG_2                GC_REG(0x0ECB8)
#define R_RLC_LB_THR_CONFIG_3                GC_REG(0x0ECB9)
#define R_RLC_LB_THR_CONFIG_4                GC_REG(0x0ECBA)
#define R_RLC_LB_CNTR_INIT                   GC_REG(0x0EC1B)
#define R_RLC_LB_CNTR_MAX                    GC_REG(0x0EC12)
#define R_RLC_LB_INIT_CU_MASK                GC_REG(0x0EC4F)
#define R_RLC_LB_PARAMS                      GC_REG(0x0EC51)
#define R_RLC_GPM_GENERAL_7                  GC_REG(0x0EC6A)
#define R_RLC_LB_CNTL                        GC_REG(0x0EC19)
#define R_RLC_LB_ALWAYS_ACTIVE_CU_MASK       GC_REG(0x0EC50)
#define R_RLC_PG_ALWAYS_ON_CU_MASK           GC_REG(0x0EC53)
#define R_RLC_SPM_MC_CNTL                    GC_REG(0x0EC71)
#define R_CP_ME_CNTL                         GC_REG(0x021B6)
#define R_CP_PFP_UCODE_ADDR                  GC_REG(0x0F814)
#define R_CP_PFP_UCODE_DATA                  GC_REG(0x0F815)
#define R_CP_CE_UCODE_ADDR                   GC_REG(0x0F818)
#define R_CP_CE_UCODE_DATA                   GC_REG(0x0F819)
#define R_CP_ME_RAM_WADDR                    GC_REG(0x0F816)
#define R_CP_ME_RAM_DATA                     GC_REG(0x0F817)
#define R_CP_MAX_CONTEXT                     GC_REG(0x030AE)
#define R_CP_DEVICE_ID                       GC_REG(0x0304B)
#define R_CP_RB_WPTR_DELAY                   GC_REG(0x021C1)
#define R_CP_RB_VMID                         GC_REG(0x03051)
#define R_CP_RB0_CNTL                        GC_REG(0x03041)
#define R_CP_RB0_WPTR                        GC_REG(0x03054)
#define R_CP_RB0_WPTR_HI                     GC_REG(0x03055)
#define R_CP_RB0_RPTR                        GC_REG(0x021C0)
#define R_CP_RB0_RPTR_ADDR                   GC_REG(0x03043)
#define R_CP_RB0_RPTR_ADDR_HI                GC_REG(0x03044)
#define R_CP_RB_WPTR_POLL_ADDR_LO            GC_REG(0x03046)
#define R_CP_RB_WPTR_POLL_ADDR_HI            GC_REG(0x03047)
#define R_CP_RB0_BASE                        GC_REG(0x03040)
#define R_CP_RB0_BASE_HI                     GC_REG(0x030B1)
#define R_CP_RB_DOORBELL_CONTROL             GC_REG(0x03059)
#define R_CP_RB_DOORBELL_RANGE_LOWER         GC_REG(0x0305A)
#define R_CP_RB_DOORBELL_RANGE_UPPER         GC_REG(0x0305B)
#define R_VGT_INDEX_TYPE                     GC_REG(0x0C243)
#define R_SCRATCH_REG0                       GC_REG(0x0C040)
#define R_CP_MEC_CNTL                        GC_REG(0x0208D)
#define R_CP_MEC_ME1_UCODE_ADDR              GC_REG(0x0F81A)
#define R_CP_MEC_ME1_UCODE_DATA              GC_REG(0x0F81B)
#define R_CP_CPC_IC_BASE_LO                  GC_REG(0x030B9)
#define R_CP_CPC_IC_BASE_HI                  GC_REG(0x030BA)
#define R_CP_CPC_IC_BASE_CNTL                GC_REG(0x030BB)
#define R_CP_HQD_ACTIVE                      GC_REG(0x03247)
#define R_CP_HQD_VMID                        GC_REG(0x03248)
#define R_CP_HQD_PERSISTENT_STATE            GC_REG(0x03249)
#define R_CP_HQD_PQ_BASE                     GC_REG(0x0324D)
#define R_CP_HQD_PQ_BASE_HI                  GC_REG(0x0324E)
#define R_CP_HQD_PQ_RPTR                     GC_REG(0x0324F)
#define R_CP_HQD_PQ_RPTR_REPORT_ADDR         GC_REG(0x03250)
#define R_CP_HQD_PQ_RPTR_REPORT_ADDR_HI      GC_REG(0x03251)
#define R_CP_HQD_PQ_WPTR_POLL_ADDR           GC_REG(0x03252)
#define R_CP_HQD_PQ_WPTR_POLL_ADDR_HI        GC_REG(0x03253)
#define R_CP_HQD_PQ_DOORBELL_CONTROL         GC_REG(0x03254)
#define R_CP_HQD_PQ_CONTROL                  GC_REG(0x03256)
#define R_CP_HQD_IB_CONTROL                  GC_REG(0x0325A)
#define R_CP_HQD_DEQUEUE_REQUEST             GC_REG(0x0325D)
#define R_CP_HQD_EOP_BASE_ADDR               GC_REG(0x0326A)
#define R_CP_HQD_EOP_BASE_ADDR_HI            GC_REG(0x0326B)
#define R_CP_HQD_EOP_CONTROL                 GC_REG(0x0326C)
#define R_CP_HQD_QUANTUM                     GC_REG(0x0324C)
#define R_CP_HQD_PQ_WPTR_LO                  GC_REG(0x0327B)
#define R_CP_HQD_PQ_WPTR_HI                  GC_REG(0x0327C)
#define R_CP_MQD_BASE_ADDR                   GC_REG(0x03245)
#define R_CP_MQD_BASE_ADDR_HI                GC_REG(0x03246)
#define R_CP_MQD_CONTROL                     GC_REG(0x03267)
#define R_CP_PQ_WPTR_POLL_CNTL               GC_REG(0x03083)
#define R_CP_MEC_DOORBELL_RANGE_LOWER        GC_REG(0x0305C)
#define R_CP_MEC_DOORBELL_RANGE_UPPER        GC_REG(0x0305D)
#define R_CP_PQ_STATUS                       GC_REG(0x030B8)
#define R_CP_ME1_PIPE0_INT_CNTL              GC_REG(0x03085)
#define   GRBM_READ_TIMEOUT_MASK      0xFFu
#define   GRBM_GUI_ACTIVE             0x80000000u
#define   GRBM_SOFT_RESET_RLC         0x4u
#define   GRBM_GFX_CNTL_VMID_SHIFT    4
#define   GRBM_IDX_SH_SHIFT           8
#define   GRBM_IDX_SE_SHIFT           16
#define   GRBM_IDX_BROADCAST_ALL      0xE0000000u   /* SE, SH e instância em broadcast */
#define   GRBM_IDX_INSTANCE_BCAST     0x40000000u
#define   SH_MEM_ALIGN_UNALIGNED      (3u << 3)
#define   SH_MEM_RETRY_DISABLE        (1u << 12)
#define   SH_MEM_PRIVATE_BASE         0x1000u       /* abertura privada em 0x1000'0000'0000'0000 (>> 48) */
#define   SH_MEM_SHARED_BASE          0x2000u       /* abertura compartilhada (LDS) em 0x2000'... */
#define   INACTIVE_CUS_SHIFT          16
#define   RLC_ENABLE_F32              0x1u
#define   CP_INT_CMP_BUSY             0x00040000u
#define   CP_INT_CNTX_BUSY            0x00080000u
#define   CP_INT_CNTX_EMPTY           0x00100000u
#define   CP_INT_GFX_IDLE             0x00200000u
#define   CP_INT_PRIV_INSTR           0x00400000u
#define   CP_INT_PRIV_REG             0x00800000u
#define   CP_INT_TIME_STAMP           0x04000000u
#define   CP_INT_GUI_IDLE_ALL         (CP_INT_CMP_BUSY | CP_INT_CNTX_BUSY | CP_INT_CNTX_EMPTY | CP_INT_GFX_IDLE)
#define   SERDES_NONCU_BUSY           0x000DFFFFu    /* SE, GC, TC0 e TC1 */
#define   SRM_ENABLE                  0x1u
#define   SRM_AUTO_INCR               0x2u
#define   RLC_UCODE_START             0x2000u
#define   RLC_LB_ENABLE               0x1u
#define   RLC_SPM_VMID_MASK           0xFu
#define   CP_ME_HALT_ALL              0x15000000u    /* CE, PFP e ME parados */
#define   CP_ME_RESET_ALL             0x003F0000u    /* pipes 0 e 1 de CE, PFP e ME */
#define   CP_ME_ICACHE_INV            0x00000150u
#define   CP_RB_BUFSZ_SHIFT           0
#define   CP_RB_BLKSZ_SHIFT           8
#define   CP_RB_DOORBELL_EN           0x40000000u
#define   CP_RB_DOORBELL_UPPER_MASK   0x0FFFFFFCu
#define AMD_GFX_RING_BYTES  (64u * 1024u)
#define AMD_GFX_MAX_HW_CTX  8u
/* PM4 tipo 3 (soc15d.h) */
#define PM4_TYPE3(op, n)    (0xC0000000u | ((u32)(op) & 0xFFu) << 8 | ((u32)(n) & 0x3FFFu) << 16)
#define PM4_NOP_1DW         PM4_TYPE3(PM4_NOP, 0x3FFF)    /* NOP de um dword só (preenchimento do anel) */
#define PM4_NOP                 0x10u
#define PM4_SET_BASE            0x11u
#define PM4_CLEAR_STATE         0x12u
#define PM4_CONTEXT_CONTROL     0x28u
#define PM4_WRITE_DATA          0x37u
#define PM4_WAIT_REG_MEM        0x3Cu
#define PM4_INDIRECT_BUFFER     0x3Fu
#define PM4_EVENT_WRITE         0x46u
#define PM4_RELEASE_MEM         0x49u
#define PM4_PREAMBLE_CNTL       0x4Au
#define PM4_SET_CONTEXT_REG     0x69u
#define PM4_SET_UCONFIG_REG     0x79u
#define   PM4_CONTEXT_REG_START       0xA000u
#define   PM4_UCONFIG_REG_START       0xC000u
#define   PM4_UCONFIG_INDEX_TYPE      (2u << 28)
#define   PM4_PREAMBLE_BEGIN_CLEAR    (2u << 28)
#define   PM4_PREAMBLE_END_CLEAR      (3u << 28)
#define   PM4_CE_PARTITION_BASE       3u
#define   PM4_WD_DST_MEM              (5u << 8)       /* WRITE_DATA: memória (pela VM) */
#define   PM4_WR_CONFIRM              (1u << 20)
#define   PM4_IB_VALID                (1u << 23)
#define   PM4_EOP_TC_WB               (1u << 15)
#define   PM4_EOP_TCL1                (1u << 16)
#define   PM4_EOP_TC                  (1u << 17)
#define   PM4_EOP_TC_MD               (1u << 21)
#define   PM4_EV_CACHE_FLUSH_INV_TS   0x14u
#define   PM4_EVENT_INDEX(x)          ((u32)(x) << 8)
#define   PM4_DATA_SEL_32             (1u << 29)
#define   PM4_INT_SEL_WAIT            (2u << 24)      /* interrompe depois que o dado foi escrito */
/* IH: fim de pipe (fence) e faltas do CP */
/* compute: MEC1, pipe 0, fila 0, programada direto nos registradores da HQD (como o KFD faz no hqd_load) */
#define   GRBM_GFX_CNTL_MEID_SHIFT    2
#define   CP_MEC_RESET_ALL            0x503F0010u    /* ME1/ME2 parados, pipes em reset, icache invalidado */
#define   CP_HQD_EOP_SIZE_MASK        0x3Fu
#define   CP_HQD_PQ_QUEUE_SIZE_MASK   0x3Fu
#define   CP_HQD_PQ_RPTR_BLOCK_SHIFT  8
#define   CP_HQD_PQ_RPTR_BLOCK_MASK   0x3F00u
#define   CP_HQD_PQ_UNORD_DISPATCH    0x10000000u
#define   CP_HQD_PQ_ROQ_IB_FLIP       0x20000000u
#define   CP_HQD_PQ_PRIV_STATE        0x40000000u
#define   CP_HQD_PQ_KMD_QUEUE         0x80000000u
#define   CP_HQD_DB_OFFSET_SHIFT      2
#define   CP_HQD_DB_OFFSET_MASK       0x0FFFFFFCu
#define   CP_HQD_DB_SOURCE            0x10000000u
#define   CP_HQD_DB_EN                0x40000000u
#define   CP_HQD_DB_HIT               0x80000000u
#define   CP_HQD_PRELOAD_MASK         0x0003FF00u
#define   CP_HQD_MIN_IB_AVAIL_MASK    0x00300000u
#define   CP_HQD_QUANTUM_MASK         0x00003F11u
#define   CP_PQ_STATUS_DOORBELL_EN    0x2u
#define   CP_PQ_WPTR_POLL_EN          0x80000000u
#define AMD_MEC_HPD_BYTES   4096u                    /* buffer de EOP por fila */
#define AMD_COMP_RING_BYTES (64u * 1024u)
#define AMD_DB_KIQ          0x00u                    /* índices de doorbell de 64 bits (layout da Vega10) */
#define AMD_DB_MEC_RING0    0x03u
#define AMD_DB_USERQ_END    0x8Au
/* MQD (struct v9_mqd, 512 dwords): índices em dwords */
#define MQD_DW                       512u
#define MQD_header                     0
#define MQD_pipelinestat_enable       11
#define MQD_thread_mgmt_se0           23
#define MQD_thread_mgmt_se1           24
#define MQD_thread_mgmt_se2           26
#define MQD_thread_mgmt_se3           27
#define MQD_misc_reserved             32
#define MQD_thread_mgmt_se4           39
#define MQD_dyn_cu_mask_lo           126
#define MQD_dyn_cu_mask_hi           127
#define MQD_mqd_base_lo              128
#define MQD_mqd_base_hi              129
#define MQD_hqd_active               130
#define MQD_hqd_vmid                 131
#define MQD_persistent_state         132
#define MQD_quantum                  135
#define MQD_pq_base_lo               136
#define MQD_pq_base_hi               137
#define MQD_pq_rptr                  138
#define MQD_rptr_report_lo           139
#define MQD_rptr_report_hi           140
#define MQD_wptr_poll_lo             141
#define MQD_wptr_poll_hi             142
#define MQD_pq_doorbell_control      143
#define MQD_pq_control               145
#define MQD_ib_control               149
#define MQD_dequeue_request          152
#define MQD_mqd_control              162
#define MQD_eop_base_lo              165
#define MQD_eop_base_hi              166
#define MQD_eop_control              167
#define MQD_pq_wptr_lo               182
#define MQD_pq_wptr_hi               183
#define MQD_DYN_CU_MASK_BYTES        2056u           /* deslocamento da máscara dinâmica de CUs na alocação */
#define R_RCC_DOORBELL_APER_EN        NBIF_REG(0x00C0)
#define   DOORBELL_APER_EN            0x1u
#define IH_CLIENT_VMC            0x12u          /* falha de página na MMHUB */
#define IH_CLIENT_UTCL2          0x1Bu          /* falha de página na GFXHUB */
#define IH_SRC_VM_FAULT          0u
#define IH_CLIENT_GRBM_CP        0x14u
#define IH_SRC_CP_EOP            181u
#define IH_SRC_CP_PRIV_REG       184u
#define IH_SRC_CP_PRIV_INSTR     185u
#define IH_SRC_CP_BAD_OPCODE     183u
#define IH_SRC_CP_CTX_EMPTY      187u
#define IH_SRC_CP_CTX_BUSY       188u
#define IH_SRC_GRBM_GUI_IDLE     233u
#define   CP_INT_OPCODE_ERROR         0x01000000u
