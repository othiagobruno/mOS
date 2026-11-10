#ifndef MOS_METAL_GATE_H
#define MOS_METAL_GATE_H
/* 1 = o processo pode receber o MTLDevice de software. images e argv terminam em NULL. */
int cx_chromium_metal_ok(const char *const *images, const char *const *argv);
#endif
