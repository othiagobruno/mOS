/* mOS — imitação mínima de proc.h para testar kernel/compat/macho.c no Windows (tools/machotest.c) */
#pragma once
#include "host_shim.h"

#define USER_BASE       0x0000400000000000ULL
#define USER_STACK_TOP  (USER_BASE + 0x40000000ULL)
#define USER_STACK_SIZE (256u << 10)

typedef struct process {
    char path[128];
    u64 brk, brk_start;
    u64 mem_pages;
    u64 mmap_next;          /* região de mmap (o dlopen põe as imagens novas aqui) */
    void *dyld;             /* grafo de imagens do carregador (macho.c) */
} process_t;

#define USER_END        0x0000408000000000ULL
#define USER_MMAP_BASE  (USER_BASE + 0x100000000ULL)

bool uvm_map(process_t *p, u64 va, u64 len);
bool uvm_copy_out(process_t *p, u64 va, const void *src, usize len);
bool uvm_copy_in(process_t *p, void *dst, u64 va, usize len);
