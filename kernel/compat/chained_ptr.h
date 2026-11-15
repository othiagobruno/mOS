/*
 * mOS — decodificação de um ponteiro encadeado (LC_DYLD_CHAINED_FIXUPS), separada do carregador
 * para ter teste de host (tools/chained_ptr_test.c). Usado por run_chained em kernel/compat/macho.c.
 *
 * Valores de pointer_format (os números da enum pública do dyld):
 *    1  ARM64E             passo de 8; rebase comum por vmaddr, rebase autenticado por deslocamento
 *    2  64                 passo de 4; rebase por vmaddr
 *    6  64_OFFSET          passo de 4; rebase por deslocamento a partir do cabeçalho
 *    9  ARM64E_USERLAND    passo de 8; todo rebase por deslocamento; bind com ordinal de 16 bits
 *   12  ARM64E_USERLAND24  passo de 8; como o 9, bind com ordinal de 24 bits
 * O 13 (ARM64E_SHARED_CACHE) tem outro desenho de bits e só aparece no cache compartilhado; os de kernel e
 * firmware (3, 4, 5, 7, 8, 10, 11) não são de processo de usuário. Ficam recusados.
 *
 * Desenho de 8 bytes do arm64e (bit 63 = auth, 62 = bind, 61..51 = next):
 *   rebase       target[42:0]  high8[50:43]
 *   bind         ordinal[15:0] (24 bits no formato 12), addend[50:32] com sinal (19 bits)
 *   auth rebase  target[31:0] (deslocamento) + diversidade/chave, que o PAC "identidade" ignora
 *   auth bind    ordinal[15:0] (24 bits no formato 12), sem addend
 * Desenho de 8 bytes de 64/64_OFFSET (bit 63 = bind, 62..51 = next):
 *   rebase       target[35:0]  high8[43:36]
 *   bind         ordinal[23:0] addend[31:24]
 */
#ifndef MOS_CHAINED_PTR_H
#define MOS_CHAINED_PTR_H

enum {
    CHPTR_ARM64E = 1, CHPTR_64 = 2, CHPTR_64_OFFSET = 6,
    CHPTR_ARM64E_USERLAND = 9, CHPTR_ARM64E_USERLAND24 = 12, CHPTR_ARM64E_SHARED_CACHE = 13,
};

static inline bool chptr_supported(unsigned fmt) {
    return fmt == CHPTR_64 || fmt == CHPTR_64_OFFSET || fmt == CHPTR_ARM64E ||
           fmt == CHPTR_ARM64E_USERLAND || fmt == CHPTR_ARM64E_USERLAND24;
}

static inline bool chptr_is_arm64e(unsigned fmt) {
    return fmt == CHPTR_ARM64E || fmt == CHPTR_ARM64E_USERLAND || fmt == CHPTR_ARM64E_USERLAND24;
}

/* Passo, em bytes, de uma unidade do campo next. */
static inline unsigned chptr_stride(unsigned fmt) { return chptr_is_arm64e(fmt) ? 8u : 4u; }

typedef struct {
    bool bind;
    u32 ordinal;     /* bind */
    i64 addend;      /* bind */
    u64 target;      /* rebase: vmaddr (target_is_vmaddr) ou deslocamento a partir do cabeçalho */
    bool target_is_vmaddr;
    u64 high8;       /* rebase: byte alto já deslocado para os bits 63:56 */
    u32 next;        /* em unidades de chptr_stride(fmt) */
} chptr_t;

/* Decodifica v no formato fmt (que tem de ser chptr_supported). */
static inline chptr_t chptr_decode(unsigned fmt, u64 v) {
    chptr_t r = {0};
    if (chptr_is_arm64e(fmt)) {
        bool auth = (v >> 63) & 1;
        r.bind = (v >> 62) & 1;
        r.next = (u32)((v >> 51) & 0x7FF);
        if (r.bind) {
            r.ordinal = (u32)(fmt == CHPTR_ARM64E_USERLAND24 ? (v & 0xFFFFFF) : (v & 0xFFFF));
            r.addend = auth ? 0 : ((i64)(((v >> 32) & 0x7FFFF) << 45) >> 45);
        } else if (auth) {
            r.target = v & 0xFFFFFFFFULL;                   /* runtimeOffset em todos os formatos arm64e */
        } else {
            r.target = v & 0x7FFFFFFFFFFULL;
            r.high8 = ((v >> 43) & 0xFF) << 56;
            r.target_is_vmaddr = fmt == CHPTR_ARM64E;       /* 9 e 12: deslocamento */
        }
        return r;
    }
    r.bind = (v >> 63) & 1;
    r.next = (u32)((v >> 51) & 0xFFF);
    if (r.bind) {
        r.ordinal = (u32)(v & 0xFFFFFF);
        r.addend = (i64)((v >> 24) & 0xFF);
    } else {
        r.target = v & 0xFFFFFFFFFULL;
        r.high8 = ((v >> 36) & 0xFF) << 56;
        r.target_is_vmaddr = fmt == CHPTR_64;
    }
    return r;
}

#endif
