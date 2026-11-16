/* mOS — IOMMU AMD-Vi (IVRS): formato da tabela de páginas e do DTE, sem MMIO (testado no host por
 * tools/iommu_host.c) e a API do driver (dev/iommu.c). */
#pragma once
#ifdef MOS_HOST_TEST
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
#else
#include "types.h"
#endif

/* PDE/PTE (AMD IOMMU spec 48882, 2.2.3): PR bit 0, NextLevel 9..11, endereço 12..51, IR 61, IW 62.
 * Mode 3 = três níveis (39 bits, 512 GiB): raiz com entradas de 1 GiB, nível 2 com páginas de 2 MiB
 * (PTE com NextLevel 0 dentro da tabela de nível 2). */
#define IOPTE_PR   (1ull << 0)
#define IOPTE_IR   (1ull << 61)
#define IOPTE_IW   (1ull << 62)
#define IOPTE_NL(n) ((u64)(n) << 9)
#define IOPTE_ADDR 0x000FFFFFFFFFF000ull

typedef struct { u64 base, len; } iommu_range_t;
typedef void *(*iommu_alloc_fn)(u64 *phys);   /* página de 4 KiB zerada */

/* monta a tabela identidade das faixas (arredondadas para 2 MiB); devolve o físico da raiz ou 0 */
u64  iommu_build_identity(const iommu_range_t *r, int n, iommu_alloc_fn alloc, u64 *tables_used);
/* tradução que o hardware faria (para testes): devolve true e o físico se mapeado com leitura e escrita */
bool iommu_walk(u64 root, u64 iova, u64 *pa, void *(*phys_to_ptr)(u64));
/* DTE de 256 bits: V, TV, Mode 3, raiz, IR/IW, domínio; IV = 0 (interrupções passam sem remapear) */
void iommu_dte(u64 dte[4], u64 root, u16 domain);

/* IVRS: escolhe o IVHD mais novo (0x40 > 0x11 > 0x10) e chama setdev para cada requisitante */
typedef struct { u64 base, efr; u16 devid, cap; u8 type; u32 ndev; } iommu_ivhd_t;
bool iommu_ivrs_parse(const u8 *ivrs, iommu_ivhd_t *out, void (*setdev)(u32 id, void *ctx), void *ctx);

/* driver */
void iommu_init(void);
void iommu_poll(void);          /* log de eventos (falhas de página, etc.) */
void iommu_resume(void);        /* depois do S3: o hardware perdeu os registradores */
const char *iommu_status(void);
