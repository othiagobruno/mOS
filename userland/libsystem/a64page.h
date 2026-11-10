/*
 * mOS — página de 16 KiB do processo arm64 traduzido, sobre as páginas de 4 KiB do kernel.
 * Contrato: docs/pagesize-arm64.md. O processo x86_64 não inclui este caminho.
 *
 * Estas funções decidem os argumentos das syscalls SYS_MMAP/SYS_MUNMAP. O teste de host
 * (tools/a64page_test.c) as executa contra um modelo do cursor do kernel; o libSystem arm64
 * as executa contra o kernel de verdade. Não há outro lugar com esta política.
 */
#pragma once
#include "../../kernel/include/syscall.h"
#include "../a64rt/a64rt_abi.h"   /* A64_GUEST_PAGE: mesma constante do sysctl e do loader */

typedef long (*a64_sys_fn)(long n, long a, long b, long c, long d, void *ctx);

typedef struct {
    int err;                     /* 0, ou errno positivo (EINVAL/ENOMEM); nada foi pedido ao kernel */
    unsigned long long len;      /* comprimento arredondado para cima, múltiplo de 16 KiB */
} a64_mmap_adj_t;

/* Regras do mmap(2) do Apple Silicon: comprimento 0 é EINVAL; MAP_FIXED exige endereço múltiplo
 * de 16 KiB; mapeamento de arquivo exige deslocamento múltiplo de 16 KiB. O comprimento sobe
 * até a página guest seguinte. MAP_ANON (0x1000) ignora o deslocamento. */
static inline a64_mmap_adj_t a64_mmap_adjust(unsigned long long addr, unsigned long long len, int flags, long long off) {
    a64_mmap_adj_t r;
    r.err = 0;
    r.len = 0;
    if (!len) { r.err = MOS_EINVAL; return r; }
    if ((flags & 0x10) && (addr & A64_GUEST_PAGE_MASK)) { r.err = MOS_EINVAL; return r; }
    if (!(flags & 0x1000) && (off & (long long)A64_GUEST_PAGE_MASK)) { r.err = MOS_EINVAL; return r; }
    if (len > ~0ull - (unsigned long long)A64_GUEST_PAGE_MASK) { r.err = MOS_ENOMEM; return r; }
    r.len = (len + A64_GUEST_PAGE_MASK) & ~(unsigned long long)A64_GUEST_PAGE_MASK;
    return r;
}

typedef struct {
    int err;
    unsigned long long addr, len;
} a64_span_t;

/* munmap/mprotect/madvise: endereço não alinhado é EINVAL; comprimento 0 é sucesso;
 * o comprimento sobe até cobrir as páginas guest inteiras. */
static inline a64_span_t a64_span(unsigned long long addr, unsigned long long len) {
    a64_span_t s;
    s.err = 0;
    s.addr = addr;
    s.len = 0;
    if (!len) return s;
    if (addr & A64_GUEST_PAGE_MASK) { s.err = MOS_EINVAL; return s; }
    if (len > ~0ull - (unsigned long long)A64_GUEST_PAGE_MASK) { s.err = MOS_ENOMEM; return s; }
    s.len = (len + A64_GUEST_PAGE_MASK) & ~(unsigned long long)A64_GUEST_PAGE_MASK;
    return s;
}

/* Tamanho de guarda de pilha: 0 continua 0 (sem guarda); o resto sobe até 16 KiB. */
static inline unsigned long long a64_guard_round(unsigned long long s) {
    if (!s) return 0;
    if (s > ~0ull - (unsigned long long)A64_GUEST_PAGE_MASK) return ~0ull - (unsigned long long)A64_GUEST_PAGE_MASK;
    return (s + A64_GUEST_PAGE_MASK) & ~(unsigned long long)A64_GUEST_PAGE_MASK;
}

/* mach_vm_deallocate arredonda como o Mach: o início desce até a página guest e o fim sobe.
 * end < base só acontece se addr+size estourar. O POSIX (munmap) recusa endereço desalinhado;
 * esta função não. */
static inline void a64_dealloc_span(unsigned long long addr, unsigned long long size,
                                    unsigned long long *base, unsigned long long *end) {
    unsigned long long b = addr & ~(unsigned long long)A64_GUEST_PAGE_MASK;
    unsigned long long e = addr + size;
    if (e < addr) { *base = 1; *end = 0; return; }   /* estouro: o chamador vê end < base */
    *base = b;
    *end = (e + A64_GUEST_PAGE_MASK) & ~(unsigned long long)A64_GUEST_PAGE_MASK;
}

/* mach_vm_region sem faixa registrada: o kernel responde uma página de 4 KiB com proteção 7 (RWX).
 * Para o guest isso é a página de 16 KiB que contém o endereço. Faixa registrada (outro tamanho ou
 * outra proteção) passa intacta — o libSystem e o loader só registram múltiplos de 16 KiB. */
static inline void a64_region_guest(unsigned long long *addr, unsigned long long *size, unsigned prot) {
    if (!addr || !size) return;
    if (*size == 4096 && prot == 7) {
        *addr &= ~(unsigned long long)A64_GUEST_PAGE_MASK;
        *size = A64_GUEST_PAGE;
    }
}

/* O runtime (rt_alloc) e o kernel compartilham mmap_next em passos de 4 KiB. Esta sonda pede
 * 16 KiB + 12 KiB sob demanda (abaixo de 2 MiB, então o kernel não aplica o alinhamento grande),
 * apara para um múltiplo de 16 KiB e solta o miolo: o cursor do kernel volta a um múltiplo de 16 KiB.
 * O buraco de endereço abaixo do cursor não é reutilizado pelo próximo mmap (só por MAP_FIXED). */
static inline long a64_align_cursor(a64_sys_fn sys, void *ctx) {
    long b = sys(SYS_MMAP, (long)(A64_GUEST_PAGE + 0x3000), 3, 0, 0, ctx);
    if (b < 0) return b;
    unsigned long long at = (unsigned long long)b;
    unsigned long long a = (at + A64_GUEST_PAGE_MASK) & ~(unsigned long long)A64_GUEST_PAGE_MASK;
    if (a > at) {
        long r = sys(SYS_MUNMAP, (long)at, (long)(a - at), 0, 0, ctx);
        if (r < 0) return r;
    }
    {
        unsigned long long tail = at + A64_GUEST_PAGE + 0x3000;
        unsigned long long keep = a + A64_GUEST_PAGE;
        if (tail > keep) {
            long r = sys(SYS_MUNMAP, (long)keep, (long)(tail - keep), 0, 0, ctx);
            if (r < 0) return r;
        }
    }
    return sys(SYS_MUNMAP, (long)a, (long)A64_GUEST_PAGE, 0, 0, ctx);
}

/* O kernel soma 4 KiB a mmap_next depois da faixa. Soltar esses 4 KiB (não fazem parte do
 * mapeamento) devolve o cursor para o fim da faixa, que é múltiplo de 16 KiB. */
static inline long a64_reclaim_slop(a64_sys_fn sys, void *ctx, unsigned long long at, unsigned long long len) {
    return sys(SYS_MUNMAP, (long)(at + len), 4096, 0, 0, ctx);
}

/* Faixa nova: cursor alinhado, mmap do kernel, sobra de 4 KiB recolhida. mode é o b do SYS_MMAP
 * (0 zerada, 1 só reserva, 3 sob demanda, 4 arquivo — o 4 o libSystem emite à parte, com fd). */
static inline long a64_map_fresh(a64_sys_fn sys, void *ctx, unsigned long long len, long mode, unsigned long long *out) {
    long al = a64_align_cursor(sys, ctx);
    if (al < 0) return al;
    long at = sys(SYS_MMAP, (long)len, mode, 0, 0, ctx);
    if (at < 0) return at;
    long rc = a64_reclaim_slop(sys, ctx, (unsigned long long)at, len);
    if (rc < 0) return rc;
    if (out) *out = (unsigned long long)at;
    return 0;
}

/* MAP_FIXED|MAP_ANON: troca as páginas guest inteiras. prot 0 deixa o trecho sem páginas (guarda).
 * MOS_MUNMAP_KEEP não devolve o topo ao alocador. */
static inline long a64_map_fixed_anon(a64_sys_fn sys, void *ctx, unsigned long long addr, unsigned long long len, int prot) {
    long r = sys(SYS_MUNMAP, (long)addr, (long)len, MOS_MUNMAP_KEEP, 0, ctx);
    if (r < 0) return r;
    if (prot) {
        r = sys(SYS_MMAP, (long)len, 3, (long)addr, 0, ctx);
        if (r < 0) return r;
    }
    return (long)addr;
}
