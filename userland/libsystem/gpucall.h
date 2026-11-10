/* Cliente do Metal para a ABI de GPU.
 *
 * Os tipos e o número da syscall vêm de kernel/include/mos_gpu.h e de
 * kernel/include/syscall.h (SYS_GPU). Este arquivo só declara o que o
 * libSystem chama.
 */
#pragma once
#include "sys.h"
#include "../../kernel/include/mos_gpu.h"

/* Bits MOS_GPU_CAP_* de MOS_GPU_INFO. 0 se a syscall falhou. */
unsigned mos_gpu_caps(void);

/* Consulta MOS_GPU_INFO uma vez. */
void mos_gpu_boot(void);

/* Uma operação. `op` é MOS_GPU_*; `st` é a estrutura; `n` é o tamanho
 * preenchido. 0 ou -errno. */
int mos_gpu_op(unsigned op, void *st, unsigned n);

/* Pixels BGRA de uma IOSurface passam a ser o BO da ABI.
 * `*px` é o mapeamento, `*id` o id da superfície, `*bo` o handle.
 * Sem MOS_GPU_CAP_IOSURF devolve -MOS_ENOENT. */
int mos_gpu_bind_iosurface(unsigned w, unsigned h, unsigned stride, void **px, unsigned *id, unsigned *bo);
void mos_gpu_unbind_iosurface(unsigned id, unsigned bo);
