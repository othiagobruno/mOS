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

/* Mensagem MSI a partir do LAPIC real (base do MADT, não um 0xFEE00000 chumbado).
 * Destino físico: APIC ID nos bits 19:12. Com x2APIC, os bits altos do ID vão em addr_hi.
 * 0 = ok; -1 se a base, o vetor ou os ponteiros não servem. */
int nvme_msi_compose(u64 lapic_base, u32 apic_id, int x2apic, u32 vector,
                     u32 *addr_lo, u32 *addr_hi, u32 *data);

/* Filas de E/S por CPU. Antes do SMP (`smp_done` == 0) devolve 0: o chamador não cria o conjunto ainda.
 * Depois, min(ncpus, granted, maxq). */
int nvme_ioq_count(int smp_done, int ncpus, int granted, int maxq);

/* Vários comandos em voo. A conclusão casa tag (cid+fila), fora de ordem. */
#define NVME_FLIGHT_MAX 31
typedef struct {
    u16 cid;
    u16 qid;
    u16 status;
    u32 dw0;
    u8 used;
    u8 done;
} nvme_flight_slot_t;

typedef struct {
    nvme_flight_slot_t slot[NVME_FLIGHT_MAX];
    int max;
    u16 next_cid;
} nvme_flights_t;

void nvme_flights_init(nvme_flights_t *f, int max);
int  nvme_flight_claim(nvme_flights_t *f, u16 qid); /* índice ou -1 se a janela está cheia */
int  nvme_flight_complete(nvme_flights_t *f, u16 qid, u16 cid, u16 status, u32 dw0);
int  nvme_flight_outstanding(const nvme_flights_t *f);
void nvme_flight_release(nvme_flights_t *f, int idx);
