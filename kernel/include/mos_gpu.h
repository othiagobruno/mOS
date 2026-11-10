/* mOS — ABI de GPU para o Metal da Sistemas.
 *
 * Contrato estável. O número da syscall é SYS_GPU (61) em syscall.h.
 * Chamada: int 0x80, rax = SYS_GPU, rdi = MOS_GPU_*, rsi = ponteiro da
 * estrutura desta operação, rdx = tamanho em bytes que o chamador preencheu.
 * Resultado em rax: 0 ou -errno (MOS_* de syscall.h). Handles saem nos
 * campos da estrutura, não em rax.
 *
 * Estruturas só crescem no fim. O kernel lê min(rdx, sizeof atual) e trata
 * o resto como zero. Um campo novo no fim não muda os anteriores.
 * MOS_GPU_ABI sobe só quando um campo existente muda de sentido.
 *
 * Detalhe de cada operação: docs/gpu-abi.md.
 */
#pragma once

#define MOS_GPU_ABI 1u

enum {
    MOS_GPU_INFO = 1,          /* mos_gpu_info_t        consulta */
    MOS_GPU_CTX_CREATE,        /* mos_gpu_ctx_t         cria; ctx sai preenchido */
    MOS_GPU_CTX_DESTROY,       /* mos_gpu_ctx_t         só o campo ctx */
    MOS_GPU_BO_CREATE,         /* mos_gpu_bo_t */
    MOS_GPU_BO_DESTROY,        /* mos_gpu_bo_t          só handle */
    MOS_GPU_BO_MAP,            /* mos_gpu_map_t         devolve addr */
    MOS_GPU_BO_UNMAP,          /* mos_gpu_map_t         addr + len */
    MOS_GPU_BO_EXPORT,         /* mos_gpu_bo_t          preenche token */
    MOS_GPU_BO_IMPORT,         /* mos_gpu_bo_t          token entra, handle sai */
    MOS_GPU_SUBMIT,            /* mos_gpu_submit_t */
    MOS_GPU_FENCE_CREATE,      /* mos_gpu_fence_t */
    MOS_GPU_FENCE_WAIT,        /* mos_gpu_fence_t       timeout_ms; ~0u sem prazo */
    MOS_GPU_FENCE_SIGNAL,      /* mos_gpu_fence_t       sinal explícito (evento compartilhado) */
    MOS_GPU_FENCE_DESTROY,     /* mos_gpu_fence_t */
    MOS_GPU_IOSURF_CREATE,     /* mos_gpu_iosurface_t   BO já criado vira a memória da superfície */
    MOS_GPU_IOSURF_LOOKUP,     /* mos_gpu_iosurface_t   por id ou por token */
    MOS_GPU_IOSURF_EXPORT,     /* mos_gpu_iosurface_t   preenche token */
    MOS_GPU_IOSURF_DESTROY,    /* mos_gpu_iosurface_t   não libera o BO */
    MOS_GPU_KMS_GET,           /* mos_gpu_kms_t         modo atual */
    MOS_GPU_KMS_SET            /* mos_gpu_kms_t         framebuffer + modo; sem 3D */
};

/* mos_gpu_info_t.caps */
#define MOS_GPU_CAP_VIRGL   (1u << 0)
#define MOS_GPU_CAP_VENUS   (1u << 1)
#define MOS_GPU_CAP_KMS     (1u << 2)
#define MOS_GPU_CAP_IOSURF  (1u << 3)

/* mos_gpu_ctx_t.kind */
#define MOS_GPU_CTX_VIRGL 1u
#define MOS_GPU_CTX_VENUS 2u

/* mos_gpu_bo_t.flags */
#define MOS_GPU_BO_READ    (1u << 0)
#define MOS_GPU_BO_WRITE   (1u << 1)
#define MOS_GPU_BO_SHARED  (1u << 2)   /* exportável a outro processo */
#define MOS_GPU_BO_GPU     (1u << 3)   /* memória da GPU (VRAM, stolen ou recurso 3D) */

/* mos_gpu_submit_t.flags */
#define MOS_GPU_SUBMIT_3D  (1u << 0)   /* fluxo de comandos do contexto (virgl ou venus) */

/* mos_gpu_fence_t.flags */
#define MOS_GPU_FENCE_SHARED (1u << 0)

/* mos_gpu_iosurface_t.format — pixels na memória do BO, linha a linha */
#define MOS_GPU_FMT_BGRA8  1u
#define MOS_GPU_FMT_RGBA8  2u

/* mos_gpu_kms_t.connector */
#define MOS_GPU_CONN_INTERNAL 0u
#define MOS_GPU_CONN_EXTERNAL 1u

typedef struct {
    unsigned abi;              /* MOS_GPU_ABI */
    unsigned caps;             /* MOS_GPU_CAP_* */
    unsigned vendor;           /* PCI, 0 se não houver dispositivo */
    unsigned device;
    char name[32];             /* terminada em zero */
} mos_gpu_info_t;

typedef struct {
    unsigned ctx;              /* saída em CREATE; entrada nas outras */
    unsigned kind;             /* MOS_GPU_CTX_* */
    unsigned flags;            /* reservado, 0 */
} mos_gpu_ctx_t;

typedef struct {
    unsigned handle;
    unsigned flags;            /* MOS_GPU_BO_* */
    unsigned ctx;              /* 0 = BO sem contexto 3D */
    unsigned token;            /* EXPORT preenche; IMPORT consome */
    unsigned long long size;   /* bytes, múltiplo de 4096 na criação */
} mos_gpu_bo_t;

typedef struct {
    unsigned handle;
    unsigned pad;
    unsigned long long addr;   /* endereço de usuário desta mapeação */
    unsigned long long len;
} mos_gpu_map_t;

typedef struct {
    unsigned ctx;
    unsigned cmd_bytes;
    unsigned fence;            /* 0 = não sinalizar; senão fence deste processo */
    unsigned flags;            /* MOS_GPU_SUBMIT_* */
    unsigned long long cmds;   /* bytes do comando no espaço do chamador */
} mos_gpu_submit_t;

typedef struct {
    unsigned fence;
    unsigned timeout_ms;       /* WAIT: ~0u espera sem prazo */
    unsigned flags;            /* MOS_GPU_FENCE_* */
    unsigned seq;              /* valor atual depois de WAIT ou SIGNAL */
} mos_gpu_fence_t;

typedef struct {
    unsigned id;               /* único no sistema enquanto a superfície existe */
    unsigned bo;               /* handle no processo chamador */
    unsigned width;
    unsigned height;
    unsigned stride;           /* bytes por linha */
    unsigned format;           /* MOS_GPU_FMT_* */
    unsigned token;            /* EXPORT / LOOKUP entre processos */
    unsigned pad;
} mos_gpu_iosurface_t;

typedef struct {
    unsigned connector;        /* MOS_GPU_CONN_* */
    unsigned width;
    unsigned height;
    unsigned stride;           /* bytes */
    unsigned format;           /* MOS_GPU_FMT_* */
    unsigned bo;               /* 0 = framebuffer do firmware */
} mos_gpu_kms_t;
