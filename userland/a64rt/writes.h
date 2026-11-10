/*
 * mOS — intervalos de memória do guest que uma syscall do mOS escreve sem passar pela PTE.
 * O kernel (uvm_copy_out) grava na página física; a proteção de escrita do JIT não vê isso.
 * Quem chama invalida as traduções desses intervalos. Só registradores e, quando o ponteiro
 * do buffer só existe depois da syscall, uma leitura da memória do guest (peek).
 */
#pragma once

typedef unsigned long long a64rt_u64;
typedef long long a64rt_i64;

typedef struct {
    a64rt_u64 addr, len;
} a64rt_span;

/* lê `n` bytes do guest em `dst`. 0 = não deu (não acrescenta o intervalo). */
typedef int (*a64rt_peek_fn)(void *ctx, a64rt_u64 addr, void *dst, a64rt_u64 n);

/* devolve quantos intervalos gravou em o[0..max). peek pode ser nulo: aí ficam de fora
 * UDP_RECVFROM (payload), MACH_MSG (buffer recebido) e MACH_VM_MAP (faixa mapeada). */
int a64rt_guest_writes(long nr, a64rt_u64 x0, a64rt_u64 x1, a64rt_u64 x2, a64rt_u64 x3, a64rt_i64 ret,
                       a64rt_peek_fn peek, void *ctx, a64rt_span *o, int max);

/* pilha do host: `raw` é o mmap de 2*stack bytes. use = metade alinhada a stack.
 * free_base/free_len = o bloco inteiro, para soltar se a pilha não entrar na tabela
 * (nunca o meio: um munmap no endereço alinhado fura a faixa e o próximo mmap cai nela). */
typedef struct {
    a64rt_u64 use, free_base, free_len;
} a64rt_hstack;

int a64rt_hstack_plan(a64rt_u64 raw, a64rt_u64 stack, a64rt_hstack *o);
