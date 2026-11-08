/*
 * mOS — tradutor arm64: acesso à memória do guest (endereço do guest = endereço do host).
 * Regras do ARM que estes helpers respeitam:
 *  - acessos alinhados ao próprio tamanho são atômicos (single-copy atomic) → load/store nativo
 *    do x86 com __atomic relaxed (no x86 isso é um mov comum);
 *  - desalinhados são permitidos para loads/stores normais (o M1 não faz trap) → memcpy byte a byte
 *    via __builtin_memcpy, que vira mov desalinhado;
 *  - acquire/release: no x86 (TSO) loads já são acquire e stores já são release; só o
 *    seq_cst de STLR seguido de LDAR exige barreira (ver a64_st_rel*).
 *  - acessos exclusivos e atômicos de LSE ficam em interp_int.c (precisam de cmpxchg).
 */
#pragma once
#include "a64.h"

#define A64_LD(T, a) ({ T _v; __builtin_memcpy(&_v, (const void *)(uintptr_t)(a), sizeof(T)); _v; })
#define A64_ST(T, a, v) do { T _v = (T)(v); __builtin_memcpy((void *)(uintptr_t)(a), &_v, sizeof(T)); } while (0)

static inline a64_u8  a64_ld8 (a64_u64 a) { return *(volatile a64_u8 *)(uintptr_t)a; }
static inline a64_u16 a64_ld16(a64_u64 a) { return (a & 1) ? A64_LD(a64_u16, a) : __atomic_load_n((a64_u16 *)(uintptr_t)a, __ATOMIC_RELAXED); }
static inline a64_u32 a64_ld32(a64_u64 a) { return (a & 3) ? A64_LD(a64_u32, a) : __atomic_load_n((a64_u32 *)(uintptr_t)a, __ATOMIC_RELAXED); }
static inline a64_u64 a64_ld64(a64_u64 a) { return (a & 7) ? A64_LD(a64_u64, a) : __atomic_load_n((a64_u64 *)(uintptr_t)a, __ATOMIC_RELAXED); }
static inline a64_u128 a64_ld128(a64_u64 a) { return A64_LD(a64_u128, a); }  /* LDR Q: atômico só por 64 bits */

static inline void a64_st8 (a64_u64 a, a64_u8 v)  { *(volatile a64_u8 *)(uintptr_t)a = v; }
static inline void a64_st16(a64_u64 a, a64_u16 v) { if (a & 1) A64_ST(a64_u16, a, v); else __atomic_store_n((a64_u16 *)(uintptr_t)a, v, __ATOMIC_RELAXED); }
static inline void a64_st32(a64_u64 a, a64_u32 v) { if (a & 3) A64_ST(a64_u32, a, v); else __atomic_store_n((a64_u32 *)(uintptr_t)a, v, __ATOMIC_RELAXED); }
static inline void a64_st64(a64_u64 a, a64_u64 v) { if (a & 7) A64_ST(a64_u64, a, v); else __atomic_store_n((a64_u64 *)(uintptr_t)a, v, __ATOMIC_RELAXED); }
static inline void a64_st128(a64_u64 a, a64_u128 v) { A64_ST(a64_u128, a, v); }

/* STLR*: release + ordem com LDAR posterior (RCsc). mfence depois do store dá seq_cst no x86. */
static inline void a64_fence_full(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }

/* leitura de N bytes (1,2,4,8) zero-extended */
static inline a64_u64 a64_ldn(a64_u64 a, int n) {
    switch (n) { case 1: return a64_ld8(a); case 2: return a64_ld16(a); case 4: return a64_ld32(a); default: return a64_ld64(a); }
}
static inline void a64_stn(a64_u64 a, a64_u64 v, int n) {
    switch (n) { case 1: a64_st8(a, (a64_u8)v); break; case 2: a64_st16(a, (a64_u16)v); break;
                 case 4: a64_st32(a, (a64_u32)v); break; default: a64_st64(a, v); break; }
}
