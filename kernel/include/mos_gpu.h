/* mOS — ABI de GPU para o Metal da Sistemas.
 *
 * Contrato estável. O número da syscall é SYS_GPU (62) em syscall.h.
 * O rascunho chegou a dizer 61; a main ocupou 61 com SYS_UNIX. 62 não muda.
 * Chamada: int 0x80, rax = SYS_GPU, rdi = MOS_GPU_*, rsi = ponteiro da
 * estrutura desta operação, rdx = tamanho em bytes que o chamador preencheu.
 * Resultado em rax: 0 ou -errno (MOS_* de syscall.h). Handles saem nos
 * campos da estrutura, não em rax.
 *
 * Estruturas de syscall só crescem no fim. O kernel lê min(rdx, sizeof atual)
 * e trata o resto como zero. Um campo novo no fim não muda os anteriores.
 * MOS_GPU_ABI 2 não muda essas estruturas: passa a exigir o lote descrito
 * abaixo em MOS_GPU_SUBMIT. A revisão 1 não tinha layout de lote.
 *
 * Detalhe de cada operação: docs/gpu-abi.md.
 */
#pragma once

#define MOS_GPU_ABI 3u

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
    MOS_GPU_KMS_SET,           /* mos_gpu_kms_t         framebuffer + modo; sem 3D */
    MOS_GPU_DISPLAY_INFO,      /* mos_gpu_display_t     revisão 3: EDID ao vivo do monitor index (0 = primeiro) */
    MOS_GPU_DISPLAY_BRIGHTNESS /* mos_gpu_display_t     revisão 3: flags SET grava brightness; sempre devolve o atual */
};

/* mos_gpu_info_t.caps */
#define MOS_GPU_CAP_VIRGL   (1u << 0)
#define MOS_GPU_CAP_VENUS   (1u << 1)
#define MOS_GPU_CAP_KMS     (1u << 2)
#define MOS_GPU_CAP_IOSURF  (1u << 3)
#define MOS_GPU_CAP_AMD     (1u << 4)   /* revisão 3: GFX9 nativo (PM4 do userland, VM por processo) */

/* mos_gpu_ctx_t.kind */
#define MOS_GPU_CTX_VIRGL 1u
#define MOS_GPU_CTX_VENUS 2u
#define MOS_GPU_CTX_AMD   3u          /* revisão 3: contexto com VMID próprio */

/* mos_gpu_bo_t.flags */
#define MOS_GPU_BO_READ    (1u << 0)
#define MOS_GPU_BO_WRITE   (1u << 1)
#define MOS_GPU_BO_SHARED  (1u << 2)   /* exportável a outro processo */
#define MOS_GPU_BO_GPU     (1u << 3)   /* memória da GPU (VRAM, stolen ou recurso 3D) */
#define MOS_GPU_BO_EXEC    (1u << 4)   /* revisão 3: código de shader (bit de execução no PTE da GPU) */

/* mos_gpu_submit_t.flags */
#define MOS_GPU_SUBMIT_3D  (1u << 0)   /* fluxo de comandos do contexto (virgl ou venus) */
#define MOS_GPU_SUBMIT_AMD (1u << 1)   /* revisão 3: cmds aponta para um mos_gpu_amd_batch_t */

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
    /* revisão 3 (só com MOS_GPU_CAP_AMD; quem passa a estrutura da revisão 2 recebe só até name) */
    unsigned amd_family;       /* 9 = GFX9 */
    unsigned num_cu;
    unsigned long long vram_bytes;
    unsigned long long gtt_bytes;
} mos_gpu_info_t;
#define MOS_GPU_INFO_V2_SIZE 48u

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
    unsigned long long gpu_va; /* revisão 3: saída do CREATE e do IMPORT num contexto AMD; 0 = sem VA */
} mos_gpu_bo_t;
#define MOS_GPU_BO_V2_SIZE 24u

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

/* Lote de MOS_GPU_SUBMIT (MOS_GPU_ABI 2). Little-endian, múltiplo de 4.
 * Ordem: mos_gpu_batch_hdr_t, handles[nbo] u32, mos_gpu_reloc_t[nreloc],
 * comandos mos_gpu_cmd_hdr_t + carga. bytes do cabeçalho = cmd_bytes.
 * kind igual ao do contexto. pad0 e pad1 são zero.
 * O índice na tabela de handles é o que a reloc e a carga citam.
 * O kernel não executa shader. Sem cap virgl/venus o lote válido só
 * sinaliza a fence. */
/* revisão 3: lote AMD (cmd_bytes até MOS_GPU_AMD_BATCH_MAX). Depois do cabeçalho vêm nib mos_gpu_amd_ib_t e
 * nbo handles (u32) dos BOs que os IBs usam. */
/* revisão 3: monitor ligado à GPU (DDC por hardware). brightness por DDC/CI (VCP 0x10); sem DDC/CI,
 * DISPLAY_BRIGHTNESS devolve -MOS_ENOENT. */
#define MOS_GPU_DISPLAY_SET 1u
typedef struct {
    unsigned index;            /* entrada: 0..n-1; -MOS_ENOENT depois do último */
    unsigned flags;            /* MOS_GPU_DISPLAY_SET em DISPLAY_BRIGHTNESS */
    unsigned edid_len;         /* bytes válidos em edid (múltiplo de 128) */
    unsigned brightness;       /* atual (e o novo, com SET) */
    unsigned brightness_max;
    unsigned connector;        /* linha DDC (1..6) */
    unsigned char edid[512];
} mos_gpu_display_t;

#define MOS_GPU_AMD_BATCH_VER 1u
#define MOS_GPU_AMD_BATCH_MAX 4096u
#define MOS_GPU_AMD_RING_GFX     0u
#define MOS_GPU_AMD_RING_COMPUTE 1u
typedef struct {
    unsigned version;          /* MOS_GPU_AMD_BATCH_VER */
    unsigned nib;              /* 1..16 */
    unsigned nbo;              /* 0..256 */
    unsigned pad;              /* zero */
} mos_gpu_amd_batch_t;
typedef struct {
    unsigned bo;               /* handle do BO que contém o IB */
    unsigned ring;             /* MOS_GPU_AMD_RING_*; o mesmo em todo o lote */
    unsigned offset_dw;
    unsigned size_dw;
} mos_gpu_amd_ib_t;

#define MOS_GPU_BATCH_VER 1u
#define MOS_GPU_BATCH_MAX_BO 8u
#define MOS_GPU_BATCH_MAX_RELOC 8u
#define MOS_GPU_BATCH_MAX_CMD 8u

enum {
    MOS_GPU_CMD_BIND = 1,      /* mos_gpu_bind_t */
    MOS_GPU_CMD_SHADER,        /* mos_gpu_shader_t: bytes do shader num BO */
    MOS_GPU_CMD_DISPATCH       /* mos_gpu_dispatch_t */
};

/* Números de comando do virgl_protocol.h para o compute mínimo. */
#define MOS_GPU_VIRGL_BIND_SHADER        31u
#define MOS_GPU_VIRGL_SET_SHADER_BUFFERS 32u
#define MOS_GPU_VIRGL_LAUNCH_GRID        35u
/* Subconjunto mOS do compute (bind, shader, dispatch). Cada comando no
 * SUBMIT_3D vem precedido do número de u32 seguintes. Não é o vn_cs inteiro. */
#define MOS_GPU_VENUS_BIND     1u
#define MOS_GPU_VENUS_SHADER   2u
#define MOS_GPU_VENUS_DISPATCH 3u

typedef struct {
    unsigned version;          /* MOS_GPU_BATCH_VER */
    unsigned kind;             /* MOS_GPU_CTX_VIRGL ou VENUS */
    unsigned bytes;            /* tamanho total do lote */
    unsigned ncmd;
    unsigned nbo;              /* tabela de handles */
    unsigned nreloc;
    unsigned pad0, pad1;       /* zero */
} mos_gpu_batch_hdr_t;

typedef struct {
    unsigned cmd;              /* índice do comando */
    unsigned off;              /* byte dentro do comando, >= 16 e alinhado a 4 */
    unsigned bo_index;         /* índice na tabela, não o handle */
    unsigned bo_off;           /* deslocamento no BO, menor que o tamanho */
} mos_gpu_reloc_t;

typedef struct {
    unsigned op;               /* MOS_GPU_CMD_* */
    unsigned bytes;            /* comando inteiro, com este cabeçalho */
    unsigned virgl;            /* opcode virgl, ou 0 no lote venus */
    unsigned venus;            /* opcode venus, ou 0 no lote virgl */
} mos_gpu_cmd_hdr_t;

typedef struct {
    unsigned slot;             /* 0..31 */
    unsigned bo_index;
    unsigned offset;
    unsigned range;            /* > 0 e cabe no BO */
} mos_gpu_bind_t;

typedef struct {
    unsigned bo_index;
    unsigned offset;
    unsigned length;
    unsigned local_x, local_y, local_z; /* os três > 0 */
} mos_gpu_shader_t;

typedef struct {
    unsigned x, y, z;          /* 1..65535 */
    unsigned pad;
} mos_gpu_dispatch_t;
