/* Camada do Metal sobre a ABI de GPU.
 *
 * O contrato é kernel/include/mos_gpu.h e docs/gpu-abi.md (agente K-GPU).
 * Este arquivo não descreve ioctl nem estrutura de syscall: só o que o
 * libSystem chama. Enquanto o header do kernel não está na árvore, as
 * mesmas operações rodam no backend de CPU. Quando o header entra,
 * gpucall.c passa a usar SYS_GPU e cai no CPU se a syscall falhar.
 */
#pragma once
#include "sys.h"

/* 0 = backend de CPU; senão bits MOS_GPU_CAP_* do kernel. */
unsigned mos_gpu_caps(void);

/* Consulta a syscall uma vez. Pode ser chamada de novo; não muda o resultado. */
void mos_gpu_boot(void);

/* Uma operação da ABI. `op` é MOS_GPU_*; `st` é a estrutura correspondente;
 * `n` é o tamanho que o chamador preencheu. 0 ou -errno. */
int mos_gpu_op(unsigned op, void *st, unsigned n);

/* Cópia BGRA para uma IOSurface da ABI (BO + id). No CPU o BO é memória
 * alinhada deste processo. Devolve o id, ou 0 se não couber. Não troca o
 * modo de vídeo: KMS_SET é tela inteira e fica a cargo de quem compõe. */
unsigned mos_gpu_present_bgra(const void *px, unsigned w, unsigned h, unsigned stride);

/* Se o kernel anunciou IOSurface, publica o quadro BGRA na ABI.
 * Sem essa capacidade não faz nada: o QuartzCore segue com o IOSurface de software. */
void mos_gpu_present_layer(const void *px, unsigned w, unsigned h, unsigned stride);
