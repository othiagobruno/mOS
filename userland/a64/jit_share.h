/* Cache de blocos traduzidos de código de arquivo somente leitura, compartilhado entre processos.
 * A tabela é dados (nunca executável): cada processo copia o bloco para a própria arena RX.
 * A invalidação acompanha a64_jit_invalidate, que a64rt_invalidate_range já chama. */
#pragma once
#include <stdint.h>

#define JIT_SHARE_MAGIC 0x4A49545348415245ULL /* "EARHSTIJ" little-endian de JITSHARE */
#define JIT_SHARE_N     48
#define JIT_SHARE_HOST  768
#define JIT_SHARE_BYTES (64u * 1024u)
/* mesma faixa que A64RT_BASE + A64RT_SPAN_MAX: o kernel mapeia JIT_SHARE_BYTES, NX */
#define JIT_SHARE_VA    0x0000400044000000ULL

typedef struct jit_share_ent {
    uint64_t file_id, file_off, pc, hash;
    uint32_t host_len, guest_len;
    uint8_t host[JIT_SHARE_HOST];
} jit_share_ent;

typedef struct jit_share_tab {
    uint64_t magic;
    volatile uint32_t lock;
    uint32_t n, clock;
    jit_share_ent ent[JIT_SHARE_N];
} jit_share_tab;

void jit_share_bind(void *mem, uint32_t bytes);
uint64_t jit_share_hash(const void *p, uint32_t n);
/* 1 se há entrada. O chamador confere hash dos bytes atuais do guest. */
int jit_share_peek(uint64_t file_id, uint64_t file_off, uint64_t pc,
                   uint32_t *guest_len, uint64_t *hash, void *host, uint32_t cap, uint32_t *host_len);
void jit_share_publish(uint64_t file_id, uint64_t file_off, uint64_t pc, uint64_t hash,
                       const void *host, uint32_t host_len, uint32_t guest_len);
void jit_share_invalidate(uint64_t lo, uint64_t hi);
/* a tabela compartilhada não é código: sempre 0 (W^X) */
int jit_share_executable(void);

/* 1 se a página guest é código de arquivo imutável. id/off identificam o arquivo entre processos. */
int a64_host_ro_file(uint64_t page, uint64_t *file_id, uint64_t *file_off);
