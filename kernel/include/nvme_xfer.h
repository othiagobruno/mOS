/* mOS — montagem de PRP/SGL e escolha da fila de E/S NVMe (sem MMIO). */
#pragma once
#ifdef MOS_HOST_TEST
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#else
#include "types.h"
#endif

#define NVME_PAGE 4096u

enum {
    NVME_INTR_POLL = 0,
    NVME_INTR_MSI  = 1,
    NVME_INTR_MSIX = 2
};

typedef struct {
    u64 prp1;
    u64 prp2;          /* página seguinte, ou 0 se a lista ainda não tem endereço físico */
    int list_entries;  /* páginas depois da primeira, gravadas em `list` */
} nvme_prp_t;

typedef struct {
    u64 addr;
    u32 len;
    u32 id;            /* byte de identificação do descritor SGL (tipo 0 = data block) */
} nvme_sgl_t;

/* 0 = ok. PRP1 é o primeiro endereço; com 2 páginas PRP2 é a segunda; com mais, a lista recebe o resto. */
int nvme_prp_build(u64 addr, u32 bytes, u64 *list, int list_max, nvme_prp_t *out);

/* Um descritor SGL Data Block cobrindo `bytes` a partir de `addr` (ambos múltiplos de 4). */
int nvme_sgl_build(u64 addr, u32 bytes, nvme_sgl_t *out);

/* Fila de E/S deste núcleo. `nq` é o número de filas de E/S (não inclui a de admin). */
int nvme_q_for_cpu(int cpu, int nq);

/* MSI-X se a programação da tabela deu certo; senão MSI; senão polling. */
int nvme_intr_pick(int has_msix, int msix_ok, int has_msi, int msi_ok);
