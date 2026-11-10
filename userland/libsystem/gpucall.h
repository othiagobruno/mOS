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

/* Um [[buffer(slot)]] do dispatch. `bytes` é a memória que entra no BO.
 * `length` é o tamanho dessa memória. `offset` e `range` são o BIND. */
typedef struct {
    const void *bytes;
    unsigned length;
    unsigned offset;
    unsigned range;
    unsigned slot;
} mos_gpu_bind_in_t;

/* Compute mínimo: até 5 binds, um SHADER e um DISPATCH. Cabe em 400 bytes.
 * `batch_out`, se não for nulo, recebe a cópia do lote enviado.
 * `*fence_out` é a fence que o submit sinaliza. `*on_device` é 1 só quando
 * o cap do kind (virgl ou venus) está ligado; nesse caso o kernel é quem
 * segue com o lote. Sem o cap a fence sinaliza e o shader não roda aqui. */
typedef struct {
    const void *shader;
    unsigned shader_len;
    unsigned local_x, local_y, local_z;
    unsigned groups_x, groups_y, groups_z;
    const mos_gpu_bind_in_t *binds;
    unsigned nbind;
    unsigned char *batch_out;
    unsigned batch_cap;
    unsigned *batch_n;
} mos_gpu_compute_in_t;

int mos_gpu_compute_submit(const mos_gpu_compute_in_t *in, unsigned *fence_out, int *on_device);
