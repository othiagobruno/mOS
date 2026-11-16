/* Gerado por scripts/gen_dcn.py (workspace) de asic_reg/dcn/dcn_1_0_offset.h e dcn_1_0_sh_mask.h (AMD, MIT).
 * Não editar à mão. D_*: deslocamento em bytes no BAR5; F_*: "shift, máscara" de um campo. */
#pragma once

#define D_AZALIA_F0_CODEC_FUNCTION_PARAMETER_POWER_STATES      (0x038CDu * 4u)
#define D_AZALIA_F0_CODEC_FUNCTION_PARAMETER_STREAM_FORMATS    (0x038CCu * 4u)
#define D_AZALIA_F0_CODEC_FUNCTION_PARAMETER_SUPPORTED_SIZE_RATES (0x038CBu * 4u)
#define D_AZF0ENDPOINT_AZALIA_F0_CODEC_ENDPOINT_DATA(i)      ((0x03847u + (u32)(i) * 0x6u) * 4u)
#define D_AZF0ENDPOINT_AZALIA_F0_CODEC_ENDPOINT_INDEX(i)     ((0x03846u + (u32)(i) * 0x6u) * 4u)
#define D_CM_CM_BNS_VALUES_B(i)                              ((0x04180u + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_BNS_VALUES_G(i)                              ((0x0417Fu + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_BNS_VALUES_R(i)                              ((0x0417Eu + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_CMOUT_CONTROL(i)                             ((0x041F0u + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_COMB_C11_C12(i)                              ((0x04159u + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_CONTROL(i)                                   ((0x04152u + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_DGAM_CONTROL(i)                              ((0x04181u + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_GAMUT_REMAP_CONTROL(i)                       ((0x04170u + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_HDR_MULT_COEF(i)                             ((0x041EBu + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_ICSC_CONTROL(i)                              ((0x04169u + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_IGAM_CONTROL(i)                              ((0x0415Fu + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_OCSC_C11_C12(i)                              ((0x04178u + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_OCSC_C33_C34(i)                              ((0x0417Du + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_OCSC_CONTROL(i)                              ((0x04177u + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_RGAM_CONTROL(i)                              ((0x041ADu + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_TEST_DEBUG_DATA(i)                           ((0x041F6u + (u32)(i) * 0x11Bu) * 4u)
#define D_CM_CM_TEST_DEBUG_INDEX(i)                          ((0x041F5u + (u32)(i) * 0x11Bu) * 4u)
#define D_CNVC_CFG_CNVC_SURFACE_PIXEL_FORMAT(i)              ((0x04107u + (u32)(i) * 0x11Bu) * 4u)
#define D_CNVC_CFG_FORMAT_CONTROL(i)                         ((0x04108u + (u32)(i) * 0x11Bu) * 4u)
#define D_CURSOR_CURSOR_CONTROL(i)                           ((0x03AACu + (u32)(i) * 0xC4u) * 4u)
#define D_D1VGA_CONTROL                                        (0x000CCu * 4u)
#define D_D2VGA_CONTROL                                        (0x000CEu * 4u)
#define D_D3VGA_CONTROL                                        (0x000F8u * 4u)
#define D_D4VGA_CONTROL                                        (0x000F9u * 4u)
#define D_DCCG_AUDIO_DTO0_MODULE                               (0x0016Du * 4u)
#define D_DCCG_AUDIO_DTO0_PHASE                                (0x0016Cu * 4u)
#define D_DCCG_AUDIO_DTO1_MODULE                               (0x0016Fu * 4u)
#define D_DCCG_AUDIO_DTO1_PHASE                                (0x0016Eu * 4u)
#define D_DCCG_AUDIO_DTO_SOURCE                                (0x0016Bu * 4u)
#define D_DCFCLK_CNTL                                          (0x039F0u * 4u)
#define D_DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_A       (0x039CDu * 4u)
#define D_DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_B       (0x039D2u * 4u)
#define D_DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_C       (0x039D7u * 4u)
#define D_DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_D       (0x039DCu * 4u)
#define D_DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_A              (0x039CBu * 4u)
#define D_DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_B              (0x039D0u * 4u)
#define D_DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_C              (0x039D5u * 4u)
#define D_DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_D              (0x039DAu * 4u)
#define D_DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_A               (0x039CCu * 4u)
#define D_DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_B               (0x039D1u * 4u)
#define D_DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_C               (0x039D6u * 4u)
#define D_DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_D               (0x039DBu * 4u)
#define D_DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_A                (0x039C9u * 4u)
#define D_DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_B                (0x039CEu * 4u)
#define D_DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_C                (0x039D3u * 4u)
#define D_DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_D                (0x039D8u * 4u)
#define D_DCHUBBUB_ARB_DF_REQ_OUTSTAND                         (0x039C5u * 4u)
#define D_DCHUBBUB_ARB_DRAM_STATE_CNTL                         (0x039C8u * 4u)
#define D_DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_A            (0x039CAu * 4u)
#define D_DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_B            (0x039CFu * 4u)
#define D_DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_C            (0x039D4u * 4u)
#define D_DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_D            (0x039D9u * 4u)
#define D_DCHUBBUB_ARB_SAT_LEVEL                               (0x039C6u * 4u)
#define D_DCHUBBUB_SDPIF_FB_BASE                               (0x03953u * 4u)
#define D_DCHUBBUB_SDPIF_FB_OFFSET                             (0x03955u * 4u)
#define D_DC_IP_REQUEST_CNTL                                   (0x0356Du * 4u)
#define D_DIG_AFMT_60958_0(i)                                ((0x05560u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_60958_1(i)                                ((0x05561u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_60958_2(i)                                ((0x05567u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_AUDIO_PACKET_CONTROL(i)                   ((0x0556Au + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_AUDIO_PACKET_CONTROL2(i)                  ((0x0553Cu + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_AUDIO_SRC_CONTROL(i)                      ((0x0556Du + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_CNTL(i)                                   ((0x055A6u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_GENERIC_0(i)                              ((0x0554Du + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_GENERIC_1(i)                              ((0x0554Eu + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_GENERIC_2(i)                              ((0x0554Fu + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_GENERIC_3(i)                              ((0x05550u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_GENERIC_4(i)                              ((0x05551u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_GENERIC_5(i)                              ((0x05552u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_GENERIC_6(i)                              ((0x05553u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_GENERIC_7(i)                              ((0x05554u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_GENERIC_HDR(i)                            ((0x0554Cu + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_INFOFRAME_CONTROL0(i)                     ((0x0556Cu + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_VBI_PACKET_CONTROL(i)                     ((0x0556Bu + (u32)(i) * 0x100u) * 4u)
#define D_DIG_AFMT_VBI_PACKET_CONTROL1(i)                    ((0x055A7u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_DIG_BE_CNTL(i)                                 ((0x0556Fu + (u32)(i) * 0x100u) * 4u)
#define D_DIG_DIG_BE_EN_CNTL(i)                              ((0x05570u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_DIG_FE_CNTL(i)                                 ((0x05528u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_ACR_32_0(i)                               ((0x05556u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_ACR_32_1(i)                               ((0x05557u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_ACR_44_0(i)                               ((0x05558u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_ACR_44_1(i)                               ((0x05559u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_ACR_48_0(i)                               ((0x0555Au + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_ACR_48_1(i)                               ((0x0555Bu + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_ACR_PACKET_CONTROL(i)                     ((0x05534u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_AUDIO_PACKET_CONTROL(i)                   ((0x05533u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_CONTROL(i)                                ((0x05531u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_DB_CONTROL(i)                             ((0x05548u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_GC(i)                                     ((0x0553Bu + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_GENERIC_PACKET_CONTROL0(i)                ((0x05538u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_GENERIC_PACKET_CONTROL1(i)                ((0x05555u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_GENERIC_PACKET_CONTROL2(i)                ((0x05546u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_GENERIC_PACKET_CONTROL3(i)                ((0x05547u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_INFOFRAME_CONTROL0(i)                     ((0x05536u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_INFOFRAME_CONTROL1(i)                     ((0x05537u + (u32)(i) * 0x100u) * 4u)
#define D_DIG_HDMI_VBI_PACKET_CONTROL(i)                     ((0x05535u + (u32)(i) * 0x100u) * 4u)
#define D_DOMAIN0_PG_CONFIG                                    (0x0354Au * 4u)
#define D_DOMAIN0_PG_STATUS                                    (0x0354Bu * 4u)
#define D_DOMAIN1_PG_CONFIG                                    (0x0354Cu * 4u)
#define D_DOMAIN1_PG_STATUS                                    (0x0354Du * 4u)
#define D_DOMAIN2_PG_CONFIG                                    (0x0354Eu * 4u)
#define D_DOMAIN2_PG_STATUS                                    (0x0354Fu * 4u)
#define D_DOMAIN3_PG_CONFIG                                    (0x03550u * 4u)
#define D_DOMAIN3_PG_STATUS                                    (0x03551u * 4u)
#define D_DOMAIN4_PG_CONFIG                                    (0x03552u * 4u)
#define D_DOMAIN4_PG_STATUS                                    (0x03553u * 4u)
#define D_DOMAIN5_PG_CONFIG                                    (0x03554u * 4u)
#define D_DOMAIN5_PG_STATUS                                    (0x03555u * 4u)
#define D_DOMAIN6_PG_CONFIG                                    (0x03556u * 4u)
#define D_DOMAIN6_PG_STATUS                                    (0x03557u * 4u)
#define D_DOMAIN7_PG_CONFIG                                    (0x03558u * 4u)
#define D_DOMAIN7_PG_STATUS                                    (0x03559u * 4u)
#define D_DPP_TOP_DPP_CONTROL(i)                             ((0x040FDu + (u32)(i) * 0x11Bu) * 4u)
#define D_DP_DP_CONFIG(i)                                    ((0x055CBu + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_DPHY_CNTL(i)                                 ((0x055D7u + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_DPHY_PRBS_CNTL(i)                            ((0x055DDu + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_DPHY_SCRAM_CNTL(i)                           ((0x055DEu + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_DPHY_TRAINING_PATTERN_SEL(i)                 ((0x055D8u + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_LINK_CNTL(i)                                 ((0x055C8u + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_LINK_FRAMING_CNTL(i)                         ((0x055D3u + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_MSA_COLORIMETRY(i)                           ((0x055CAu + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_MSA_MISC(i)                                  ((0x055CEu + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_MSA_TIMING_PARAM1(i)                         ((0x0560Cu + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_MSA_TIMING_PARAM2(i)                         ((0x0560Du + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_MSA_TIMING_PARAM3(i)                         ((0x0560Eu + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_MSA_TIMING_PARAM4(i)                         ((0x0560Fu + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_PIXEL_FORMAT(i)                              ((0x055C9u + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_SEC_AUD_N(i)                                 ((0x055F1u + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_SEC_CNTL(i)                                  ((0x055EBu + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_SEC_TIMESTAMP(i)                             ((0x055F5u + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_STEER_FIFO(i)                                ((0x055CDu + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_VID_M(i)                                     ((0x055D2u + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_VID_N(i)                                     ((0x055D1u + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_VID_STREAM_CNTL(i)                           ((0x055CCu + (u32)(i) * 0x100u) * 4u)
#define D_DP_DP_VID_TIMING(i)                                ((0x055D0u + (u32)(i) * 0x100u) * 4u)
#define D_DSCL_DSCL_AUTOCAL(i)                               ((0x04135u + (u32)(i) * 0x11Bu) * 4u)
#define D_DSCL_DSCL_CONTROL(i)                               ((0x04126u + (u32)(i) * 0x11Bu) * 4u)
#define D_DSCL_LB_DATA_FORMAT(i)                             ((0x0413Du + (u32)(i) * 0x11Bu) * 4u)
#define D_DSCL_LB_MEMORY_CTRL(i)                             ((0x0413Eu + (u32)(i) * 0x11Bu) * 4u)
#define D_DSCL_MPC_SIZE(i)                                   ((0x0413Cu + (u32)(i) * 0x11Bu) * 4u)
#define D_DSCL_RECOUT_SIZE(i)                                ((0x0413Bu + (u32)(i) * 0x11Bu) * 4u)
#define D_DSCL_RECOUT_START(i)                               ((0x0413Au + (u32)(i) * 0x11Bu) * 4u)
#define D_DSCL_SCL_MODE(i)                                   ((0x04124u + (u32)(i) * 0x11Bu) * 4u)
#define D_FMT_FMT_BIT_DEPTH_CONTROL(i)                       ((0x04D01u + (u32)(i) * 0x5Au) * 4u)
#define D_FMT_FMT_CLAMP_CNTL(i)                              ((0x04D08u + (u32)(i) * 0x5Au) * 4u)
#define D_FMT_FMT_CONTROL(i)                                 ((0x04D00u + (u32)(i) * 0x5Au) * 4u)
#define D_FMT_FMT_DITHER_RAND_B_SEED(i)                      ((0x04D04u + (u32)(i) * 0x5Au) * 4u)
#define D_FMT_FMT_DITHER_RAND_G_SEED(i)                      ((0x04D03u + (u32)(i) * 0x5Au) * 4u)
#define D_FMT_FMT_DITHER_RAND_R_SEED(i)                      ((0x04D02u + (u32)(i) * 0x5Au) * 4u)
#define D_FMT_FMT_DYNAMIC_EXP_CNTL(i)                        ((0x04CFFu + (u32)(i) * 0x5Au) * 4u)
#define D_HUBPREQ_BLANK_OFFSET_0(i)                          ((0x03A76u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_BLANK_OFFSET_1(i)                          ((0x03A77u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_CUR0_TTU_CNTL0(i)                      ((0x03A62u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_CUR0_TTU_CNTL1(i)                      ((0x03A63u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_EXPANSION_MODE(i)                      ((0x03A5Bu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_GLOBAL_TTU_CNTL(i)                     ((0x03A5Du + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_SURF0_TTU_CNTL0(i)                     ((0x03A5Eu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_SURF0_TTU_CNTL1(i)                     ((0x03A5Fu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_SURF1_TTU_CNTL0(i)                     ((0x03A60u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_SURF1_TTU_CNTL1(i)                     ((0x03A61u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_TTU_QOS_WM(i)                          ((0x03A5Cu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_VM_CONTEXT0_PAGE_TABLE_BASE_ADDR_LSB(i) ((0x03A6Cu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_VM_CONTEXT0_PAGE_TABLE_BASE_ADDR_MSB(i) ((0x03A6Du + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_VM_CONTEXT0_PAGE_TABLE_END_ADDR_LSB(i) ((0x03A70u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_VM_CONTEXT0_PAGE_TABLE_END_ADDR_MSB(i) ((0x03A71u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_VM_CONTEXT0_PAGE_TABLE_START_ADDR_LSB(i) ((0x03A6Eu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_VM_CONTEXT0_PAGE_TABLE_START_ADDR_MSB(i) ((0x03A6Fu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_VM_CONTEXT0_PROTECTION_FAULT_DEFAULT_ADDR_LSB(i) ((0x03A6Au + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_VM_CONTEXT0_PROTECTION_FAULT_DEFAULT_ADDR_MSB(i) ((0x03A6Bu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_VM_MX_L1_TLB_CNTL(i)                   ((0x03A75u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_VM_SYSTEM_APERTURE_DEFAULT_ADDR_LSB(i) ((0x03A68u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_VM_SYSTEM_APERTURE_DEFAULT_ADDR_MSB(i) ((0x03A69u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_VM_SYSTEM_APERTURE_HIGH_ADDR_LSB(i)    ((0x03A66u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_VM_SYSTEM_APERTURE_HIGH_ADDR_MSB(i)    ((0x03A67u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_VM_SYSTEM_APERTURE_LOW_ADDR_LSB(i)     ((0x03A64u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCN_VM_SYSTEM_APERTURE_LOW_ADDR_MSB(i)     ((0x03A65u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCSURF_FLIP_CONTROL(i)                     ((0x03A4Eu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCSURF_PRIMARY_META_SURFACE_ADDRESS(i)     ((0x03A45u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCSURF_PRIMARY_META_SURFACE_ADDRESS_HIGH(i) ((0x03A46u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS(i)          ((0x03A3Du + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH(i)     ((0x03A3Eu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCSURF_SURFACE_CONTROL(i)                  ((0x03A4Du + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCSURF_SURFACE_PITCH(i)                    ((0x03A3Bu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DCSURF_SURFACE_PITCH_C(i)                  ((0x03A3Cu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DST_AFTER_SCALER(i)                        ((0x03A79u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_DST_DIMENSIONS(i)                          ((0x03A78u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_NOM_PARAMETERS_0(i)                        ((0x03A81u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_NOM_PARAMETERS_1(i)                        ((0x03A82u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_NOM_PARAMETERS_2(i)                        ((0x03A83u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_NOM_PARAMETERS_3(i)                        ((0x03A84u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_NOM_PARAMETERS_4(i)                        ((0x03A85u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_NOM_PARAMETERS_5(i)                        ((0x03A86u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_NOM_PARAMETERS_6(i)                        ((0x03A87u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_NOM_PARAMETERS_7(i)                        ((0x03A88u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_PER_LINE_DELIVERY(i)                       ((0x03A8Au + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_PER_LINE_DELIVERY_PRE(i)                   ((0x03A89u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_PREFETCH_SETTINS(i)                        ((0x03A7Au + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_PREFETCH_SETTINS_C(i)                      ((0x03A7Bu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_REF_FREQ_TO_PIX_FREQ(i)                    ((0x03A8Cu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_VBLANK_PARAMETERS_0(i)                     ((0x03A7Cu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_VBLANK_PARAMETERS_1(i)                     ((0x03A7Du + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_VBLANK_PARAMETERS_2(i)                     ((0x03A7Eu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_VBLANK_PARAMETERS_3(i)                     ((0x03A7Fu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPREQ_VBLANK_PARAMETERS_4(i)                     ((0x03A80u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBPRET_HUBPRET_CONTROL(i)                         ((0x03AA0u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_DCHUBP_CNTL(i)                                ((0x03A26u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_DCHUBP_REQ_SIZE_CONFIG(i)                     ((0x03A24u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_DCHUBP_REQ_SIZE_CONFIG_C(i)                   ((0x03A25u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_DCSURF_ADDR_CONFIG(i)                         ((0x03A1Au + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_DCSURF_PRI_VIEWPORT_DIMENSION(i)              ((0x03A1Du + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_DCSURF_PRI_VIEWPORT_DIMENSION_C(i)            ((0x03A1Fu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_DCSURF_PRI_VIEWPORT_START(i)                  ((0x03A1Cu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_DCSURF_PRI_VIEWPORT_START_C(i)                ((0x03A1Eu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_DCSURF_SEC_VIEWPORT_DIMENSION(i)              ((0x03A21u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_DCSURF_SEC_VIEWPORT_DIMENSION_C(i)            ((0x03A23u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_DCSURF_SEC_VIEWPORT_START(i)                  ((0x03A20u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_DCSURF_SEC_VIEWPORT_START_C(i)                ((0x03A22u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_DCSURF_SURFACE_CONFIG(i)                      ((0x03A19u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_DCSURF_TILING_CONFIG(i)                       ((0x03A1Bu + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_HUBPREQ_DEBUG_DB(i)                           ((0x03A29u + (u32)(i) * 0xC4u) * 4u)
#define D_HUBP_HUBP_CLK_CNTL(i)                              ((0x03A27u + (u32)(i) * 0xC4u) * 4u)
#define D_MPCC_MPCC_BG_B_CB(i)                               ((0x04AFBu + (u32)(i) * 0x1Bu) * 4u)
#define D_MPCC_MPCC_BG_G_Y(i)                                ((0x04AFAu + (u32)(i) * 0x1Bu) * 4u)
#define D_MPCC_MPCC_BG_R_CR(i)                               ((0x04AF9u + (u32)(i) * 0x1Bu) * 4u)
#define D_MPCC_MPCC_BOT_SEL(i)                               ((0x04AF1u + (u32)(i) * 0x1Bu) * 4u)
#define D_MPCC_MPCC_CONTROL(i)                               ((0x04AF3u + (u32)(i) * 0x1Bu) * 4u)
#define D_MPCC_MPCC_OPP_ID(i)                                ((0x04AF2u + (u32)(i) * 0x1Bu) * 4u)
#define D_MPCC_MPCC_STATUS(i)                                ((0x04AFDu + (u32)(i) * 0x1Bu) * 4u)
#define D_MPCC_MPCC_TOP_SEL(i)                               ((0x04AF0u + (u32)(i) * 0x1Bu) * 4u)
#define D_MPCC_MPCC_UPDATE_LOCK_SEL(i)                       ((0x04AF5u + (u32)(i) * 0x1Bu) * 4u)
#define D_MPC_OUT_MUX(i)                                     ((0x04BEFu + (u32)(i) * 0x1u) * 4u)
#define D_ODM_OPTC_DATA_SOURCE_SELECT(i)                     ((0x04F8Bu + (u32)(i) * 0x10u) * 4u)
#define D_ODM_OPTC_INPUT_CLOCK_CONTROL(i)                    ((0x04F8Du + (u32)(i) * 0x10u) * 4u)
#define D_ODM_OPTC_INPUT_GLOBAL_CONTROL(i)                   ((0x04F8Au + (u32)(i) * 0x10u) * 4u)
#define D_OPP_PIPE_OPP_PIPE_CONTROL(i)                       ((0x04D4Cu + (u32)(i) * 0x5Au) * 4u)
#define D_OTG_OTG_BLACK_COLOR(i)                             ((0x05023u + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_BLANK_CONTROL(i)                           ((0x05002u + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_CLOCK_CONTROL(i)                           ((0x05048u + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_CONTROL(i)                                 ((0x05001u + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_DOUBLE_BUFFER_CONTROL(i)                   ((0x0501Bu + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_GLOBAL_CONTROL0(i)                         ((0x05052u + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_H_BLANK_START_END(i)                       ((0x04FEBu + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_H_SYNC_A(i)                                ((0x04FECu + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_H_SYNC_A_CNTL(i)                           ((0x04FEDu + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_H_TIMING_CNTL(i)                           ((0x04FEEu + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_H_TOTAL(i)                                 ((0x04FEAu + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_INTERLACE_CONTROL(i)                       ((0x05004u + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_MASTER_UPDATE_LOCK(i)                      ((0x0504Du + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_VREADY_PARAM(i)                            ((0x0504Bu + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_VSTARTUP_PARAM(i)                          ((0x05049u + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_VUPDATE_PARAM(i)                           ((0x0504Au + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_V_BLANK_START_END(i)                       ((0x04FF6u + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_V_SYNC_A(i)                                ((0x04FF7u + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_V_SYNC_A_CNTL(i)                           ((0x04FF8u + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_V_TOTAL(i)                                 ((0x04FEFu + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_V_TOTAL_MAX(i)                             ((0x04FF1u + (u32)(i) * 0x80u) * 4u)
#define D_OTG_OTG_V_TOTAL_MIN(i)                             ((0x04FF0u + (u32)(i) * 0x80u) * 4u)
#define D_PHYPLLA_PIXCLK_RESYNC_CNTL                           (0x00100u * 4u)
#define D_PHYPLLB_PIXCLK_RESYNC_CNTL                           (0x00101u * 4u)
#define D_PHYPLLC_PIXCLK_RESYNC_CNTL                           (0x00102u * 4u)
#define D_PHYPLLD_PIXCLK_RESYNC_CNTL                           (0x00103u * 4u)
#define D_PHYPLLE_PIXCLK_RESYNC_CNTL                           (0x0010Cu * 4u)
#define D_PHYPLLF_PIXCLK_RESYNC_CNTL                           (0x0013Eu * 4u)
#define D_VGA_TEST_CONTROL                                     (0x000D5u * 4u)
#define D_VTG_CONTROL(i)                                     ((0x039E8u + (u32)(i) * 0x1u) * 4u)
#define F_AZALIA_F0_CODEC_FUNCTION_PARAMETER_POWER_STATES__CLKSTOP     30u, 0x40000000u
#define F_AZALIA_F0_CODEC_FUNCTION_PARAMETER_POWER_STATES__EPSS        31u, 0x80000000u
#define F_AZALIA_F0_CODEC_FUNCTION_PARAMETER_SUPPORTED_SIZE_RATES__AUDIO_RATE_CAPABILITIES  0u, 0x00000FFFu
#define F_AZF0ENDPOINT_AZALIA_F0_CODEC_ENDPOINT_INDEX__AZALIA_ENDPOINT_REG_INDEX  0u, 0x00003FFFu
#define F_CM_CM_BNS_VALUES_R__CM_BNS_SCALE_R                           16u, 0xFFFF0000u
#define F_CM_CM_CMOUT_CONTROL__CM_CMOUT_ROUND_TRUNC_MODE                0u, 0x0000000Fu
#define F_CM_CM_CONTROL__CM_BYPASS_EN                                   0u, 0x00000001u
#define F_CM_CM_DGAM_CONTROL__CM_DGAM_LUT_MODE                          0u, 0x00000007u
#define F_CM_CM_HDR_MULT_COEF__CM_HDR_MULT_COEF                         0u, 0x0007FFFFu
#define F_CM_CM_IGAM_CONTROL__CM_IGAM_INPUT_FORMAT                     26u, 0x0C000000u
#define F_CM_CM_IGAM_CONTROL__CM_IGAM_LUT_MODE                          0u, 0x00000003u
#define F_CM_CM_OCSC_C11_C12__CM_OCSC_C11                               0u, 0x0000FFFFu
#define F_CM_CM_OCSC_C11_C12__CM_OCSC_C12                              16u, 0xFFFF0000u
#define F_CM_CM_OCSC_CONTROL__CM_OCSC_MODE                              0u, 0x00000007u
#define F_CM_CM_RGAM_CONTROL__CM_RGAM_LUT_MODE                          0u, 0x00000007u
#define F_CM_CM_TEST_DEBUG_INDEX__CM_TEST_DEBUG_INDEX                   0u, 0x000000FFu
#define F_CNVC_CFG_CNVC_SURFACE_PIXEL_FORMAT__CNVC_SURFACE_PIXEL_FORMAT  0u, 0x0000007Fu
#define F_CNVC_CFG_FORMAT_CONTROL__ALPHA_EN                             8u, 0x00000100u
#define F_CNVC_CFG_FORMAT_CONTROL__CNVC_BYPASS                         12u, 0x00001000u
#define F_CNVC_CFG_FORMAT_CONTROL__FORMAT_EXPANSION_MODE                0u, 0x00000001u
#define F_CNVC_CFG_FORMAT_CONTROL__OUTPUT_FP                           16u, 0x00010000u
#define F_CURSOR_CURSOR_CONTROL__CURSOR_ENABLE                          0u, 0x00000001u
#define F_D1VGA_CONTROL__D1VGA_MODE_ENABLE                              0u, 0x00000001u
#define F_DCCG_AUDIO_DTO0_MODULE__DCCG_AUDIO_DTO0_MODULE                0u, 0xFFFFFFFFu
#define F_DCCG_AUDIO_DTO0_PHASE__DCCG_AUDIO_DTO0_PHASE                  0u, 0xFFFFFFFFu
#define F_DCCG_AUDIO_DTO1_MODULE__DCCG_AUDIO_DTO1_MODULE                0u, 0xFFFFFFFFu
#define F_DCCG_AUDIO_DTO1_PHASE__DCCG_AUDIO_DTO1_PHASE                  0u, 0xFFFFFFFFu
#define F_DCCG_AUDIO_DTO_SOURCE__DCCG_AUDIO_DTO0_SOURCE_SEL             0u, 0x00000007u
#define F_DCCG_AUDIO_DTO_SOURCE__DCCG_AUDIO_DTO_SEL                     4u, 0x00000030u
#define F_DCFCLK_CNTL__DCFCLK_GATE_DIS                                 31u, 0x80000000u
#define F_DCHUBBUB_ARB_DF_REQ_OUTSTAND__DCHUBBUB_ARB_MIN_REQ_OUTSTAND  16u, 0x01FF0000u
#define F_DCHUBBUB_ARB_DRAM_STATE_CNTL__DCHUBBUB_ARB_ALLOW_SELF_REFRESH_FORCE_ENABLE  1u, 0x00000002u
#define F_DCHUBBUB_ARB_DRAM_STATE_CNTL__DCHUBBUB_ARB_ALLOW_SELF_REFRESH_FORCE_VALUE  0u, 0x00000001u
#define F_DCHUBBUB_ARB_SAT_LEVEL__DCHUBBUB_ARB_SAT_LEVEL                0u, 0xFFFFFFFFu
#define F_DCHUBBUB_SDPIF_FB_BASE__SDPIF_FB_BASE                         0u, 0x00FFFFFFu
#define F_DCHUBBUB_SDPIF_FB_OFFSET__SDPIF_FB_OFFSET                     0u, 0x00FFFFFFu
#define F_DC_IP_REQUEST_CNTL__IP_REQUEST_EN                             0u, 0x00000001u
#define F_DIG_AFMT_60958_0__AFMT_60958_CS_CHANNEL_NUMBER_L             20u, 0x00F00000u
#define F_DIG_AFMT_60958_0__AFMT_60958_CS_CLOCK_ACCURACY               28u, 0x30000000u
#define F_DIG_AFMT_60958_1__AFMT_60958_CS_CHANNEL_NUMBER_R             20u, 0x00F00000u
#define F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_2              0u, 0x0000000Fu
#define F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_3              4u, 0x000000F0u
#define F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_4              8u, 0x00000F00u
#define F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_5             12u, 0x0000F000u
#define F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_6             16u, 0x000F0000u
#define F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_7             20u, 0x00F00000u
#define F_DIG_AFMT_AUDIO_PACKET_CONTROL__AFMT_60958_CS_UPDATE          26u, 0x04000000u
#define F_DIG_AFMT_AUDIO_PACKET_CONTROL__AFMT_AUDIO_SAMPLE_SEND         0u, 0x00000001u
#define F_DIG_AFMT_AUDIO_PACKET_CONTROL2__AFMT_60958_OSF_OVRD          28u, 0x10000000u
#define F_DIG_AFMT_AUDIO_PACKET_CONTROL2__AFMT_AUDIO_CHANNEL_ENABLE     8u, 0x0000FF00u
#define F_DIG_AFMT_AUDIO_PACKET_CONTROL2__AFMT_AUDIO_LAYOUT_OVRD        0u, 0x00000001u
#define F_DIG_AFMT_AUDIO_SRC_CONTROL__AFMT_AUDIO_SRC_SELECT             0u, 0x00000007u
#define F_DIG_AFMT_CNTL__AFMT_AUDIO_CLOCK_EN                            0u, 0x00000001u
#define F_DIG_AFMT_INFOFRAME_CONTROL0__AFMT_AUDIO_INFO_UPDATE           7u, 0x00000080u
#define F_DIG_AFMT_VBI_PACKET_CONTROL__AFMT_GENERIC_CONFLICT           16u, 0x00010000u
#define F_DIG_AFMT_VBI_PACKET_CONTROL__AFMT_GENERIC_CONFLICT_CLR       17u, 0x00020000u
#define F_DIG_AFMT_VBI_PACKET_CONTROL__AFMT_GENERIC_INDEX              28u, 0xF0000000u
#define F_DIG_AFMT_VBI_PACKET_CONTROL1__AFMT_GENERIC0_IMMEDIATE_UPDATE  2u, 0x00000004u
#define F_DIG_DIG_BE_CNTL__DIG_FE_SOURCE_SELECT                         8u, 0x00007F00u
#define F_DIG_DIG_BE_CNTL__DIG_MODE                                    16u, 0x00070000u
#define F_DIG_DIG_BE_EN_CNTL__DIG_ENABLE                                0u, 0x00000001u
#define F_DIG_DIG_FE_CNTL__DIG_SOURCE_SELECT                            0u, 0x00000007u
#define F_DIG_DIG_FE_CNTL__DIG_START                                   10u, 0x00000400u
#define F_DIG_DIG_FE_CNTL__TMDS_COLOR_FORMAT                           30u, 0xC0000000u
#define F_DIG_DIG_FE_CNTL__TMDS_PIXEL_ENCODING                         28u, 0x10000000u
#define F_DIG_HDMI_ACR_32_0__HDMI_ACR_CTS_32                           12u, 0xFFFFF000u
#define F_DIG_HDMI_ACR_32_1__HDMI_ACR_N_32                              0u, 0x000FFFFFu
#define F_DIG_HDMI_ACR_44_0__HDMI_ACR_CTS_44                           12u, 0xFFFFF000u
#define F_DIG_HDMI_ACR_44_1__HDMI_ACR_N_44                              0u, 0x000FFFFFu
#define F_DIG_HDMI_ACR_48_0__HDMI_ACR_CTS_48                           12u, 0xFFFFF000u
#define F_DIG_HDMI_ACR_48_1__HDMI_ACR_N_48                              0u, 0x000FFFFFu
#define F_DIG_HDMI_ACR_PACKET_CONTROL__HDMI_ACR_AUDIO_PRIORITY         31u, 0x80000000u
#define F_DIG_HDMI_ACR_PACKET_CONTROL__HDMI_ACR_AUTO_SEND              12u, 0x00001000u
#define F_DIG_HDMI_ACR_PACKET_CONTROL__HDMI_ACR_SOURCE                  8u, 0x00000100u
#define F_DIG_HDMI_AUDIO_PACKET_CONTROL__HDMI_AUDIO_DELAY_EN            4u, 0x00000030u
#define F_DIG_HDMI_CONTROL__HDMI_CLOCK_CHANNEL_RATE                     2u, 0x00000004u
#define F_DIG_HDMI_CONTROL__HDMI_DATA_SCRAMBLE_EN                       1u, 0x00000002u
#define F_DIG_HDMI_CONTROL__HDMI_DEEP_COLOR_DEPTH                      28u, 0x30000000u
#define F_DIG_HDMI_CONTROL__HDMI_DEEP_COLOR_ENABLE                     24u, 0x01000000u
#define F_DIG_HDMI_CONTROL__HDMI_KEEPOUT_MODE                           0u, 0x00000001u
#define F_DIG_HDMI_CONTROL__HDMI_NO_EXTRA_NULL_PACKET_FILLED            3u, 0x00000008u
#define F_DIG_HDMI_CONTROL__HDMI_PACKET_GEN_VERSION                     4u, 0x00000010u
#define F_DIG_HDMI_DB_CONTROL__HDMI_DB_DISABLE                         12u, 0x00001000u
#define F_DIG_HDMI_GC__HDMI_GC_AVMUTE                                   0u, 0x00000001u
#define F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC0_CONT          1u, 0x00000002u
#define F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC0_LINE         16u, 0x003F0000u
#define F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC0_SEND          0u, 0x00000001u
#define F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC1_CONT          5u, 0x00000020u
#define F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC1_LINE         24u, 0x3F000000u
#define F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC1_SEND          4u, 0x00000010u
#define F_DIG_HDMI_INFOFRAME_CONTROL0__HDMI_AUDIO_INFO_SEND             4u, 0x00000010u
#define F_DIG_HDMI_INFOFRAME_CONTROL1__HDMI_AUDIO_INFO_LINE             8u, 0x00003F00u
#define F_DIG_HDMI_VBI_PACKET_CONTROL__HDMI_ACP_SEND                   12u, 0x00001000u
#define F_DIG_HDMI_VBI_PACKET_CONTROL__HDMI_GC_CONT                     5u, 0x00000020u
#define F_DIG_HDMI_VBI_PACKET_CONTROL__HDMI_GC_SEND                     4u, 0x00000010u
#define F_DIG_HDMI_VBI_PACKET_CONTROL__HDMI_NULL_SEND                   0u, 0x00000001u
#define F_DOMAIN0_PG_CONFIG__DOMAIN0_POWER_FORCEON                      0u, 0x00000001u
#define F_DOMAIN0_PG_CONFIG__DOMAIN0_POWER_GATE                         8u, 0x00000100u
#define F_DOMAIN0_PG_STATUS__DOMAIN0_PGFSM_PWR_STATUS                  30u, 0xC0000000u
#define F_DPP_TOP_DPP_CONTROL__DPPCLK_RATE_CONTROL                     24u, 0x01000000u
#define F_DPP_TOP_DPP_CONTROL__DPP_CLOCK_ENABLE                         4u, 0x00000010u
#define F_DP_DP_CONFIG__DP_UDI_LANES                                    0u, 0x00000003u
#define F_DP_DP_DPHY_CNTL__DPHY_BYPASS                                 16u, 0x00010000u
#define F_DP_DP_DPHY_PRBS_CNTL__DPHY_PRBS_EN                            0u, 0x00000001u
#define F_DP_DP_DPHY_SCRAM_CNTL__DPHY_SCRAMBLER_ADVANCE                 4u, 0x00000010u
#define F_DP_DP_DPHY_SCRAM_CNTL__DPHY_SCRAMBLER_BS_COUNT                8u, 0x0003FF00u
#define F_DP_DP_LINK_CNTL__DP_LINK_TRAINING_COMPLETE                    4u, 0x00000010u
#define F_DP_DP_LINK_FRAMING_CNTL__DP_IDLE_BS_INTERVAL                  0u, 0x0003FFFFu
#define F_DP_DP_LINK_FRAMING_CNTL__DP_VBID_DISABLE                     24u, 0x01000000u
#define F_DP_DP_LINK_FRAMING_CNTL__DP_VID_ENHANCED_FRAME_MODE          28u, 0x10000000u
#define F_DP_DP_MSA_COLORIMETRY__DP_MSA_MISC0                          24u, 0xFF000000u
#define F_DP_DP_MSA_TIMING_PARAM1__DP_MSA_HTOTAL                       16u, 0xFFFF0000u
#define F_DP_DP_MSA_TIMING_PARAM1__DP_MSA_VTOTAL                        0u, 0x0000FFFFu
#define F_DP_DP_MSA_TIMING_PARAM2__DP_MSA_HSTART                       16u, 0xFFFF0000u
#define F_DP_DP_MSA_TIMING_PARAM2__DP_MSA_VSTART                        0u, 0x0000FFFFu
#define F_DP_DP_MSA_TIMING_PARAM3__DP_MSA_HSYNCPOLARITY                31u, 0x80000000u
#define F_DP_DP_MSA_TIMING_PARAM3__DP_MSA_HSYNCWIDTH                   16u, 0x7FFF0000u
#define F_DP_DP_MSA_TIMING_PARAM3__DP_MSA_VSYNCPOLARITY                15u, 0x00008000u
#define F_DP_DP_MSA_TIMING_PARAM3__DP_MSA_VSYNCWIDTH                    0u, 0x00007FFFu
#define F_DP_DP_MSA_TIMING_PARAM4__DP_MSA_HWIDTH                       16u, 0xFFFF0000u
#define F_DP_DP_MSA_TIMING_PARAM4__DP_MSA_VHEIGHT                       0u, 0x0000FFFFu
#define F_DP_DP_PIXEL_FORMAT__DP_COMPONENT_DEPTH                       24u, 0x07000000u
#define F_DP_DP_PIXEL_FORMAT__DP_PIXEL_ENCODING                         0u, 0x00000007u
#define F_DP_DP_SEC_AUD_N__DP_SEC_AUD_N                                 0u, 0x00FFFFFFu
#define F_DP_DP_SEC_CNTL__DP_SEC_ACM_ENABLE                            16u, 0x00010000u
#define F_DP_DP_SEC_CNTL__DP_SEC_AIP_ENABLE                            12u, 0x00001000u
#define F_DP_DP_SEC_CNTL__DP_SEC_ASP_ENABLE                             4u, 0x00000010u
#define F_DP_DP_SEC_CNTL__DP_SEC_ATP_ENABLE                             8u, 0x00000100u
#define F_DP_DP_SEC_CNTL__DP_SEC_STREAM_ENABLE                          0u, 0x00000001u
#define F_DP_DP_SEC_TIMESTAMP__DP_SEC_TIMESTAMP_MODE                    0u, 0x00000001u
#define F_DP_DP_STEER_FIFO__DP_STEER_FIFO_RESET                         0u, 0x00000001u
#define F_DP_DP_VID_M__DP_VID_M                                         0u, 0x00FFFFFFu
#define F_DP_DP_VID_N__DP_VID_N                                         0u, 0x00FFFFFFu
#define F_DP_DP_VID_STREAM_CNTL__DP_VID_STREAM_DIS_DEFER                8u, 0x00000300u
#define F_DP_DP_VID_STREAM_CNTL__DP_VID_STREAM_ENABLE                   0u, 0x00000001u
#define F_DP_DP_VID_STREAM_CNTL__DP_VID_STREAM_STATUS                  16u, 0x00010000u
#define F_DP_DP_VID_TIMING__DP_VID_M_N_GEN_EN                           8u, 0x00000100u
#define F_DP_DP_VID_TIMING__DP_VID_N_MUL                               10u, 0x00000C00u
#define F_DSCL_LB_DATA_FORMAT__PIXEL_DEPTH                              0u, 0x00000003u
#define F_DSCL_LB_DATA_FORMAT__PIXEL_REDUCE_MODE                       12u, 0x00001000u
#define F_DSCL_LB_MEMORY_CTRL__LB_MAX_PARTITIONS                        8u, 0x00003F00u
#define F_DSCL_LB_MEMORY_CTRL__MEMORY_CONFIG                            0u, 0x00000003u
#define F_DSCL_MPC_SIZE__MPC_HEIGHT                                    16u, 0x3FFF0000u
#define F_DSCL_MPC_SIZE__MPC_WIDTH                                      0u, 0x00003FFFu
#define F_DSCL_RECOUT_SIZE__RECOUT_HEIGHT                              16u, 0x3FFF0000u
#define F_DSCL_RECOUT_SIZE__RECOUT_WIDTH                                0u, 0x00003FFFu
#define F_DSCL_RECOUT_START__RECOUT_START_X                             0u, 0x00001FFFu
#define F_DSCL_SCL_MODE__DSCL_MODE                                      0u, 0x00000007u
#define F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_FRAME_RANDOM_ENABLE           13u, 0x00002000u
#define F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_HIGHPASS_RANDOM_ENABLE        15u, 0x00008000u
#define F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_RGB_RANDOM_ENABLE             14u, 0x00004000u
#define F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_SPATIAL_DITHER_DEPTH          11u, 0x00001800u
#define F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_SPATIAL_DITHER_EN              8u, 0x00000100u
#define F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_SPATIAL_DITHER_MODE            9u, 0x00000600u
#define F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_TEMPORAL_DITHER_EN            16u, 0x00010000u
#define F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_TRUNCATE_DEPTH                 4u, 0x00000030u
#define F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_TRUNCATE_EN                    0u, 0x00000001u
#define F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_TRUNCATE_MODE                  1u, 0x00000002u
#define F_FMT_FMT_CLAMP_CNTL__FMT_CLAMP_COLOR_FORMAT                   16u, 0x00070000u
#define F_FMT_FMT_CLAMP_CNTL__FMT_CLAMP_DATA_EN                         0u, 0x00000001u
#define F_FMT_FMT_CONTROL__FMT_CBCR_BIT_REDUCTION_BYPASS               21u, 0x00200000u
#define F_FMT_FMT_CONTROL__FMT_PIXEL_ENCODING                          16u, 0x00030000u
#define F_FMT_FMT_CONTROL__FMT_SPATIAL_DITHER_FRAME_COUNTER_BIT_SWAP   12u, 0x00003000u
#define F_FMT_FMT_CONTROL__FMT_SPATIAL_DITHER_FRAME_COUNTER_MAX         8u, 0x00000F00u
#define F_FMT_FMT_CONTROL__FMT_SUBSAMPLING_MODE                        18u, 0x000C0000u
#define F_FMT_FMT_DYNAMIC_EXP_CNTL__FMT_DYNAMIC_EXP_EN                  0u, 0x00000001u
#define F_FMT_FMT_DYNAMIC_EXP_CNTL__FMT_DYNAMIC_EXP_MODE                4u, 0x00000010u
#define F_HUBPREQ_BLANK_OFFSET_0__DLG_V_BLANK_END                      16u, 0x7FFF0000u
#define F_HUBPREQ_BLANK_OFFSET_0__REFCYC_H_BLANK_END                    0u, 0x00001FFFu
#define F_HUBPREQ_BLANK_OFFSET_1__MIN_DST_Y_NEXT_START                  0u, 0x0003FFFFu
#define F_HUBPREQ_DCN_CUR0_TTU_CNTL0__QoS_LEVEL_FIXED                  24u, 0x0F000000u
#define F_HUBPREQ_DCN_EXPANSION_MODE__CRQ_EXPANSION_MODE                2u, 0x0000000Cu
#define F_HUBPREQ_DCN_EXPANSION_MODE__DRQ_EXPANSION_MODE                0u, 0x00000003u
#define F_HUBPREQ_DCN_EXPANSION_MODE__MRQ_EXPANSION_MODE                4u, 0x00000030u
#define F_HUBPREQ_DCN_EXPANSION_MODE__PRQ_EXPANSION_MODE                6u, 0x000000C0u
#define F_HUBPREQ_DCN_GLOBAL_TTU_CNTL__MIN_TTU_VBLANK                   0u, 0x00FFFFFFu
#define F_HUBPREQ_DCN_GLOBAL_TTU_CNTL__QoS_LEVEL_FLIP                  28u, 0xF0000000u
#define F_HUBPREQ_DCN_SURF0_TTU_CNTL0__QoS_LEVEL_FIXED                 24u, 0x0F000000u
#define F_HUBPREQ_DCN_SURF0_TTU_CNTL0__REFCYC_PER_REQ_DELIVERY          0u, 0x007FFFFFu
#define F_HUBPREQ_DCN_SURF0_TTU_CNTL1__REFCYC_PER_REQ_DELIVERY_PRE      0u, 0x007FFFFFu
#define F_HUBPREQ_DCN_SURF1_TTU_CNTL0__QoS_LEVEL_FIXED                 24u, 0x0F000000u
#define F_HUBPREQ_DCN_TTU_QOS_WM__QoS_LEVEL_HIGH_WM                    16u, 0x3FFF0000u
#define F_HUBPREQ_DCN_TTU_QOS_WM__QoS_LEVEL_LOW_WM                      0u, 0x00003FFFu
#define F_HUBPREQ_DCN_VM_CONTEXT0_PROTECTION_FAULT_DEFAULT_ADDR_MSB__VM_CONTEXT0_PROTECTION_FAULT_DEFAULT_ADDR_MSB  0u, 0x0000000Fu
#define F_HUBPREQ_DCN_VM_MX_L1_TLB_CNTL__ENABLE_L1_TLB                  0u, 0x00000001u
#define F_HUBPREQ_DCN_VM_MX_L1_TLB_CNTL__SYSTEM_ACCESS_MODE             3u, 0x00000018u
#define F_HUBPREQ_DCN_VM_SYSTEM_APERTURE_DEFAULT_ADDR_MSB__MC_VM_SYSTEM_APERTURE_DEFAULT_ADDR_MSB  0u, 0x0000000Fu
#define F_HUBPREQ_DCN_VM_SYSTEM_APERTURE_DEFAULT_ADDR_MSB__MC_VM_SYSTEM_APERTURE_DEFAULT_SYSTEM 28u, 0x10000000u
#define F_HUBPREQ_DCSURF_FLIP_CONTROL__SURFACE_FLIP_IN_STEREOSYNC      16u, 0x00010000u
#define F_HUBPREQ_DCSURF_FLIP_CONTROL__SURFACE_FLIP_MODE_FOR_STEREOSYNC 12u, 0x00003000u
#define F_HUBPREQ_DCSURF_FLIP_CONTROL__SURFACE_FLIP_TYPE                1u, 0x00000002u
#define F_HUBPREQ_DCSURF_SURFACE_CONTROL__PRIMARY_META_SURFACE_TMZ     16u, 0x00010000u
#define F_HUBPREQ_DCSURF_SURFACE_CONTROL__PRIMARY_SURFACE_DCC_EN        1u, 0x00000002u
#define F_HUBPREQ_DCSURF_SURFACE_CONTROL__PRIMARY_SURFACE_DCC_IND_64B_BLK  2u, 0x00000004u
#define F_HUBPREQ_DCSURF_SURFACE_CONTROL__PRIMARY_SURFACE_TMZ           0u, 0x00000001u
#define F_HUBPREQ_DCSURF_SURFACE_CONTROL__SECONDARY_SURFACE_DCC_EN      9u, 0x00000200u
#define F_HUBPREQ_DCSURF_SURFACE_CONTROL__SECONDARY_SURFACE_DCC_IND_64B_BLK 10u, 0x00000400u
#define F_HUBPREQ_DCSURF_SURFACE_PITCH__META_PITCH                     16u, 0x3FFF0000u
#define F_HUBPREQ_DCSURF_SURFACE_PITCH__PITCH                           0u, 0x00003FFFu
#define F_HUBPREQ_DCSURF_SURFACE_PITCH_C__META_PITCH_C                 16u, 0x3FFF0000u
#define F_HUBPREQ_DCSURF_SURFACE_PITCH_C__PITCH_C                       0u, 0x00003FFFu
#define F_HUBPREQ_DST_AFTER_SCALER__DST_Y_AFTER_SCALER                 16u, 0x00070000u
#define F_HUBPREQ_DST_AFTER_SCALER__REFCYC_X_AFTER_SCALER               0u, 0x00001FFFu
#define F_HUBPREQ_DST_DIMENSIONS__REFCYC_PER_HTOTAL                     0u, 0x001FFFFFu
#define F_HUBPREQ_NOM_PARAMETERS_0__DST_Y_PER_PTE_ROW_NOM_L             0u, 0x0001FFFFu
#define F_HUBPREQ_NOM_PARAMETERS_1__REFCYC_PER_PTE_GROUP_NOM_L          0u, 0x007FFFFFu
#define F_HUBPREQ_NOM_PARAMETERS_4__DST_Y_PER_META_ROW_NOM_L            0u, 0x0001FFFFu
#define F_HUBPREQ_NOM_PARAMETERS_5__REFCYC_PER_META_CHUNK_NOM_L         0u, 0x007FFFFFu
#define F_HUBPREQ_NOM_PARAMETERS_6__DST_Y_PER_META_ROW_NOM_C            0u, 0x0001FFFFu
#define F_HUBPREQ_PER_LINE_DELIVERY__REFCYC_PER_LINE_DELIVERY_L         0u, 0x00001FFFu
#define F_HUBPREQ_PER_LINE_DELIVERY_PRE__REFCYC_PER_LINE_DELIVERY_PRE_L  0u, 0x00001FFFu
#define F_HUBPREQ_PREFETCH_SETTINS__DST_Y_PREFETCH                     24u, 0xFF000000u
#define F_HUBPREQ_PREFETCH_SETTINS__VRATIO_PREFETCH                     0u, 0x001FFFFFu
#define F_HUBPREQ_PREFETCH_SETTINS_C__VRATIO_PREFETCH_C                 0u, 0x001FFFFFu
#define F_HUBPREQ_REF_FREQ_TO_PIX_FREQ__REF_FREQ_TO_PIX_FREQ            0u, 0x001FFFFFu
#define F_HUBPREQ_VBLANK_PARAMETERS_0__DST_Y_PER_ROW_VBLANK             8u, 0x00003F00u
#define F_HUBPREQ_VBLANK_PARAMETERS_0__DST_Y_PER_VM_VBLANK              0u, 0x0000001Fu
#define F_HUBPREQ_VBLANK_PARAMETERS_1__REFCYC_PER_PTE_GROUP_VBLANK_L    0u, 0x007FFFFFu
#define F_HUBPREQ_VBLANK_PARAMETERS_3__REFCYC_PER_META_CHUNK_VBLANK_L   0u, 0x007FFFFFu
#define F_HUBPREQ_VBLANK_PARAMETERS_4__REFCYC_PER_META_CHUNK_VBLANK_C   0u, 0x007FFFFFu
#define F_HUBPRET_HUBPRET_CONTROL__CROSSBAR_SRC_CB_B                   20u, 0x00300000u
#define F_HUBPRET_HUBPRET_CONTROL__CROSSBAR_SRC_CR_R                   22u, 0x00C00000u
#define F_HUBPRET_HUBPRET_CONTROL__DET_BUF_PLANE1_BASE_ADDRESS          0u, 0x00000FFFu
#define F_HUBP_DCHUBP_CNTL__HUBP_BLANK_EN                               0u, 0x00000001u
#define F_HUBP_DCHUBP_CNTL__HUBP_NO_OUTSTANDING_REQ                     1u, 0x00000002u
#define F_HUBP_DCHUBP_CNTL__HUBP_TTU_DISABLE                           12u, 0x00001000u
#define F_HUBP_DCHUBP_CNTL__HUBP_VTG_SEL                                4u, 0x000000F0u
#define F_HUBP_DCSURF_ADDR_CONFIG__MAX_COMPRESSED_FRAGS                12u, 0x00003000u
#define F_HUBP_DCSURF_ADDR_CONFIG__NUM_BANKS                            3u, 0x00000038u
#define F_HUBP_DCSURF_ADDR_CONFIG__NUM_PIPES                            0u, 0x00000007u
#define F_HUBP_DCSURF_ADDR_CONFIG__NUM_RB_PER_SE                       10u, 0x00000C00u
#define F_HUBP_DCSURF_ADDR_CONFIG__NUM_SE                               8u, 0x00000300u
#define F_HUBP_DCSURF_ADDR_CONFIG__PIPE_INTERLEAVE                      6u, 0x000000C0u
#define F_HUBP_DCSURF_PRI_VIEWPORT_DIMENSION__PRI_VIEWPORT_HEIGHT      16u, 0x3FFF0000u
#define F_HUBP_DCSURF_PRI_VIEWPORT_DIMENSION__PRI_VIEWPORT_WIDTH        0u, 0x00003FFFu
#define F_HUBP_DCSURF_PRI_VIEWPORT_START__PRI_VIEWPORT_X_START          0u, 0x00003FFFu
#define F_HUBP_DCSURF_SURFACE_CONFIG__H_MIRROR_EN                      10u, 0x00000400u
#define F_HUBP_DCSURF_SURFACE_CONFIG__ROTATION_ANGLE                    8u, 0x00000300u
#define F_HUBP_DCSURF_SURFACE_CONFIG__SURFACE_PIXEL_FORMAT              0u, 0x0000007Fu
#define F_HUBP_DCSURF_TILING_CONFIG__META_LINEAR                        9u, 0x00000200u
#define F_HUBP_DCSURF_TILING_CONFIG__PIPE_ALIGNED                      11u, 0x00000800u
#define F_HUBP_DCSURF_TILING_CONFIG__RB_ALIGNED                        10u, 0x00000400u
#define F_HUBP_DCSURF_TILING_CONFIG__SW_MODE                            0u, 0x0000001Fu
#define F_HUBP_HUBP_CLK_CNTL__HUBP_CLOCK_ENABLE                         0u, 0x00000001u
#define F_MPCC_MPCC_BOT_SEL__MPCC_BOT_SEL                               0u, 0x0000000Fu
#define F_MPCC_MPCC_CONTROL__MPCC_ALPHA_BLND_MODE                       4u, 0x00000030u
#define F_MPCC_MPCC_CONTROL__MPCC_ALPHA_MULTIPLIED_MODE                 6u, 0x00000040u
#define F_MPCC_MPCC_CONTROL__MPCC_BLND_ACTIVE_OVERLAP_ONLY              7u, 0x00000080u
#define F_MPCC_MPCC_CONTROL__MPCC_GLOBAL_ALPHA                         16u, 0x00FF0000u
#define F_MPCC_MPCC_CONTROL__MPCC_GLOBAL_GAIN                          24u, 0xFF000000u
#define F_MPCC_MPCC_CONTROL__MPCC_MODE                                  0u, 0x00000003u
#define F_MPCC_MPCC_OPP_ID__MPCC_OPP_ID                                 0u, 0x0000000Fu
#define F_MPCC_MPCC_STATUS__MPCC_IDLE                                   0u, 0x00000001u
#define F_MPCC_MPCC_TOP_SEL__MPCC_TOP_SEL                               0u, 0x0000000Fu
#define F_MPCC_MPCC_UPDATE_LOCK_SEL__MPCC_UPDATE_LOCK_SEL               0u, 0x0000000Fu
#define F_MPC_OUT_MUX__MPC_OUT_MUX                                      0u, 0x0000000Fu
#define F_ODM_OPTC_DATA_SOURCE_SELECT__OPTC_SRC_SEL                     8u, 0x00000700u
#define F_ODM_OPTC_INPUT_CLOCK_CONTROL__OPTC_INPUT_CLK_EN               1u, 0x00000002u
#define F_ODM_OPTC_INPUT_CLOCK_CONTROL__OPTC_INPUT_CLK_GATE_DIS         0u, 0x00000001u
#define F_ODM_OPTC_INPUT_CLOCK_CONTROL__OPTC_INPUT_CLK_ON               2u, 0x00000004u
#define F_ODM_OPTC_INPUT_GLOBAL_CONTROL__OPTC_UNDERFLOW_CLEAR          12u, 0x00001000u
#define F_ODM_OPTC_INPUT_GLOBAL_CONTROL__OPTC_UNDERFLOW_OCCURRED_STATUS 10u, 0x00000400u
#define F_OPP_PIPE_OPP_PIPE_CONTROL__OPP_PIPE_CLOCK_EN                  0u, 0x00000001u
#define F_OTG_OTG_BLACK_COLOR__OTG_BLACK_COLOR_B_CB                     0u, 0x000003FFu
#define F_OTG_OTG_BLACK_COLOR__OTG_BLACK_COLOR_G_Y                     10u, 0x000FFC00u
#define F_OTG_OTG_BLACK_COLOR__OTG_BLACK_COLOR_R_CR                    20u, 0x3FF00000u
#define F_OTG_OTG_BLANK_CONTROL__OTG_BLANK_DATA_EN                      8u, 0x00000100u
#define F_OTG_OTG_BLANK_CONTROL__OTG_BLANK_DE_MODE                     16u, 0x00010000u
#define F_OTG_OTG_CLOCK_CONTROL__OTG_BUSY                              16u, 0x00010000u
#define F_OTG_OTG_CLOCK_CONTROL__OTG_CLOCK_EN                           0u, 0x00000001u
#define F_OTG_OTG_CLOCK_CONTROL__OTG_CLOCK_GATE_DIS                     1u, 0x00000002u
#define F_OTG_OTG_CLOCK_CONTROL__OTG_CLOCK_ON                           8u, 0x00000100u
#define F_OTG_OTG_CONTROL__OTG_DISABLE_POINT_CNTL                       8u, 0x00000300u
#define F_OTG_OTG_CONTROL__OTG_FIELD_NUMBER_CNTL                       13u, 0x00002000u
#define F_OTG_OTG_CONTROL__OTG_MASTER_EN                                0u, 0x00000001u
#define F_OTG_OTG_CONTROL__OTG_START_POINT_CNTL                        12u, 0x00001000u
#define F_OTG_OTG_DOUBLE_BUFFER_CONTROL__OTG_BLANK_DATA_DOUBLE_BUFFER_EN 16u, 0x00010000u
#define F_OTG_OTG_GLOBAL_CONTROL0__OTG_MASTER_UPDATE_LOCK_SEL          25u, 0x0E000000u
#define F_OTG_OTG_H_BLANK_START_END__OTG_H_BLANK_END                   16u, 0x7FFF0000u
#define F_OTG_OTG_H_BLANK_START_END__OTG_H_BLANK_START                  0u, 0x00007FFFu
#define F_OTG_OTG_H_SYNC_A__OTG_H_SYNC_A_END                           16u, 0x7FFF0000u
#define F_OTG_OTG_H_SYNC_A__OTG_H_SYNC_A_START                          0u, 0x00007FFFu
#define F_OTG_OTG_H_SYNC_A_CNTL__OTG_H_SYNC_A_POL                       0u, 0x00000001u
#define F_OTG_OTG_H_TIMING_CNTL__OTG_H_TIMING_DIV_BY2                   0u, 0x00000001u
#define F_OTG_OTG_H_TOTAL__OTG_H_TOTAL                                  0u, 0x00007FFFu
#define F_OTG_OTG_INTERLACE_CONTROL__OTG_INTERLACE_ENABLE               0u, 0x00000001u
#define F_OTG_OTG_MASTER_UPDATE_LOCK__OTG_MASTER_UPDATE_LOCK            0u, 0x00000001u
#define F_OTG_OTG_MASTER_UPDATE_LOCK__UPDATE_LOCK_STATUS                8u, 0x00000100u
#define F_OTG_OTG_VREADY_PARAM__VREADY_OFFSET                           0u, 0x0000FFFFu
#define F_OTG_OTG_VSTARTUP_PARAM__VSTARTUP_START                        0u, 0x000003FFu
#define F_OTG_OTG_VUPDATE_PARAM__VUPDATE_OFFSET                         0u, 0x0000FFFFu
#define F_OTG_OTG_VUPDATE_PARAM__VUPDATE_WIDTH                         16u, 0x03FF0000u
#define F_OTG_OTG_V_BLANK_START_END__OTG_V_BLANK_END                   16u, 0x7FFF0000u
#define F_OTG_OTG_V_BLANK_START_END__OTG_V_BLANK_START                  0u, 0x00007FFFu
#define F_OTG_OTG_V_SYNC_A__OTG_V_SYNC_A_END                           16u, 0x7FFF0000u
#define F_OTG_OTG_V_SYNC_A__OTG_V_SYNC_A_START                          0u, 0x00007FFFu
#define F_OTG_OTG_V_SYNC_A_CNTL__OTG_V_SYNC_A_POL                       0u, 0x00000001u
#define F_OTG_OTG_V_TOTAL__OTG_V_TOTAL                                  0u, 0x00007FFFu
#define F_OTG_OTG_V_TOTAL_MAX__OTG_V_TOTAL_MAX                          0u, 0x00007FFFu
#define F_OTG_OTG_V_TOTAL_MIN__OTG_V_TOTAL_MIN                          0u, 0x00007FFFu
#define F_PHYPLLA_PIXCLK_RESYNC_CNTL__PHYPLLA_DCCG_DEEP_COLOR_CNTL      4u, 0x00000030u
#define F_VGA_TEST_CONTROL__VGA_TEST_ENABLE                             0u, 0x00000001u
#define F_VGA_TEST_CONTROL__VGA_TEST_RENDER_START                       8u, 0x00000100u
#define F_VTG_CONTROL__VTG0_ENABLE                                     31u, 0x80000000u
#define F_VTG_CONTROL__VTG0_FP2                                         0u, 0x00007FFFu
#define F_VTG_CONTROL__VTG0_VCOUNT_INIT                                15u, 0x3FFF8000u
