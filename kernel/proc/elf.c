/* mOS — carregador de executáveis ELF64 (x86-64, estáticos, ET_EXEC) na região de usuário */
#include "proc.h"

typedef struct PACKED {
    u8  ident[16];
    u16 type, machine;
    u32 version;
    u64 entry, phoff, shoff;
    u32 flags;
    u16 ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} elf64_ehdr_t;

typedef struct PACKED {
    u32 type, flags;
    u64 offset, vaddr, paddr, filesz, memsz, align;
} elf64_phdr_t;

#define PT_LOAD 1

bool elf_load(process_t *p, const u8 *img, usize len, u64 *entry, char *err, usize errlen) {
    const elf64_ehdr_t *h = (const elf64_ehdr_t *)img;
#define FAIL(...) do { snprintf(err, errlen, __VA_ARGS__); return false; } while (0)
    if (len < sizeof *h || memcmp(h->ident, "\x7F" "ELF", 4)) FAIL("não é um executável ELF");
    if (h->ident[4] != 2 || h->ident[5] != 1) FAIL("ELF precisa ser de 64 bits, little-endian");
    if (h->machine != 0x3E) FAIL("ELF não é x86-64 (machine %u)", h->machine);
    if (h->type != 2) FAIL("só executáveis estáticos (ET_EXEC) são suportados (tipo %u)", h->type);
    if (h->phentsize != sizeof(elf64_phdr_t) || h->phoff > len || (u64)h->phnum * sizeof(elf64_phdr_t) > len - h->phoff)
        FAIL("cabeçalhos de programa inválidos");
    u64 hi = 0;
    int loads = 0;
    for (int i = 0; i < h->phnum; i++) {
        const elf64_phdr_t *ph = (const elf64_phdr_t *)(img + h->phoff + (u64)i * sizeof *ph);
        if (ph->type != PT_LOAD || !ph->memsz) continue;
        if (ph->filesz > ph->memsz || ph->offset > len || ph->filesz > len - ph->offset) FAIL("segmento %d fora do arquivo", i);
        if (ph->vaddr < USER_BASE || ph->vaddr + ph->memsz > USER_STACK_TOP - USER_STACK_SIZE - 0x100000)
            FAIL("segmento %d fora da região de usuário (0x%llx)", i, ph->vaddr);
        if (!uvm_map(p, ph->vaddr, ph->memsz)) FAIL("sem memória para o segmento %d", i);
        if (ph->filesz && !uvm_copy_out(p, ph->vaddr, img + ph->offset, ph->filesz)) FAIL("falha ao copiar o segmento %d", i);
        if (ph->vaddr + ph->memsz > hi) hi = ph->vaddr + ph->memsz;
        loads++;
    }
    if (!loads) FAIL("nenhum segmento carregável");
    if (h->entry < USER_BASE || h->entry >= hi) FAIL("ponto de entrada inválido (0x%llx)", h->entry);
    *entry = h->entry;
    p->brk = p->brk_start = (hi + 0xFFF) & ~0xFFFULL;
    return true;
#undef FAIL
}
