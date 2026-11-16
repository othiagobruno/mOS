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
