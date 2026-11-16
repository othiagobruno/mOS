/* mOS — QR Code (ISO/IEC 18004), modo byte. level: 0 L, 1 M, 2 Q, 3 H. version 0 = a menor que cabe; mask -1 = a de
 * menor penalidade. out: size*size módulos (1 escuro), linha 0 em cima; até 177*177. Devolve a versão ou -1. */
#pragma once
#include <stdint.h>
int mos_qr_encode(const uint8_t *msg, int len, int level, int version, int mask, uint8_t *out, int *size);
/* módulos de função (fn[size*size], 1 = função) e blocos da versão v no nível lvl; devolve size ou -1 */
int mos_qr_layout(int v, int lvl, uint8_t *fn, int *nblocks, int *ecc_per_block, int *raw_codewords);
