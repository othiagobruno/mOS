/* mOS — o que o AVAssetExportSession precisa de um AVVideoComposition (preenchido por cx_vcomp_describe, avvideocomp.c) */
#pragma once
#include "avvcomp_core.h"
#define VCD_MAX_INST 64
#define VCD_MAX_LAYERS 8
typedef struct { double t0, t1; uint32_t bg; int nlayers; vc_layer layers[VCD_MAX_LAYERS]; } vcd_inst;   /* layers[0] é a de cima */
typedef struct { int w, h; double frame; int ninst; vcd_inst inst[VCD_MAX_INST]; } vcomp_desc;
int cx_vcomp_describe(void *vc, vcomp_desc *d);   /* 0 ok; -2 instruções de compositor próprio do app */
