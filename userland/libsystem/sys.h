/*
 * mOS libSystem — interface com o kernel. A ABI (números, flags O_* do Linux, mos_stat_t) é a
 * de kernel/include/syscall.h: int 0x80, número em rax, argumentos em rdi, rsi, rdx, r10, r8;
 * retorno em rax (negativo = -errno).
 */
#pragma once

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned long uintptr_t;
typedef long intptr_t;
typedef signed char i8;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef unsigned long size_t;
#ifndef MOS_SSIZE_T_DEFINED
#define MOS_SSIZE_T_DEFINED
typedef long ssize_t;
#endif
#ifndef MOS_OFF_T_DEFINED
#define MOS_OFF_T_DEFINED
typedef long off_t;
#endif
typedef unsigned long usize;

#include "../../kernel/include/syscall.h"

/* errno é por thread, como no macOS: __error() devolve o slot TSD_ERRNO (103) da TSD da thread (GS,
 * extra.c: cx_tsd), e cx_errno é só um apelido. Com um int global, o pool de threads do libuv fazia
 * "errno = 0" no meio do stat que falhou em outra thread, o -1 virava sucesso com o buffer vazio e o
 * host de extensões do VS Code caía no stat de node_modules. */
int *__error(void);
#define cx_errno (*__error())

#if defined(MOS_LIBSYS_SYSCALL)
/* Só o teste de host tools/dyld_remove_test.c: o int 0x80 é uma função. Não é um modo do sistema. */
long cx_syscall(long n, long a, long b, long c, long d, long e);
#elif defined(__aarch64__)
/* o "pause" das esperas ativas do x86 vira o yield do arm64: código novo que usa __builtin_ia32_pause compila nos dois */
#define __builtin_ia32_pause() __asm__ volatile("yield" ::: "memory")
/* libSystem arm64 (docs/tradutor-arm64.md): svc #0x4d4f, número em x8, argumentos x0..x4, resultado em x0; o runtime
 * do tradutor repassa 1:1 para o int 0x80 do kernel */
static inline long cx_syscall(long n, long a, long b, long c, long d, long e) {
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    register long x4 __asm__("x4") = e;
    __asm__ volatile("svc #0x4d4f" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4) : "memory");
    return x0;
}
#else
static inline long cx_syscall(long n, long a, long b, long c, long d, long e) {
    long r;
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8) : "rcx", "r11", "memory");
    return r;
}
#endif
#define SYS0(n) cx_syscall(n, 0, 0, 0, 0, 0)
#define SYS1(n, a) cx_syscall(n, (long)(a), 0, 0, 0, 0)
#define SYS2(n, a, b) cx_syscall(n, (long)(a), (long)(b), 0, 0, 0)
#define SYS3(n, a, b, c) cx_syscall(n, (long)(a), (long)(b), (long)(c), 0, 0)
#define SYS4(n, a, b, c, d) cx_syscall(n, (long)(a), (long)(b), (long)(c), (long)(d), 0)
#define SYS5(n, a, b, c, d, e) cx_syscall(n, (long)(a), (long)(b), (long)(c), (long)(d), (long)(e))

/* bloco de partida montado pelo loader (kernel/compat/macho.c) e passado em rdi */
typedef struct {
    u64 main;               /* endereço de main() */
    u64 init_funcs;         /* ponteiros de inicializadores (__mod_init_func) */
    u64 n_init_funcs;
    u64 init_offsets;       /* __init_offsets (u32 relativos a image_base) */
    u64 n_init_offsets;
    u64 image_base;
    u64 argc;
    u64 argv, envp, apple;
    u64 objc_sects;          /* {addr, size, kind}[n]: seções Objective-C da imagem */
    u64 n_objc_sects;
} cx_start_t;

/* errno do macOS (BSD) */
enum {
    CX_EPERM = 1, CX_ENOENT = 2, CX_ESRCH = 3, CX_EIO = 5, CX_EBADF = 9, CX_ENOMEM = 12,
    CX_EACCES = 13, CX_EFAULT = 14, CX_EEXIST = 17, CX_ENOTDIR = 20, CX_EISDIR = 21,
    CX_EINVAL = 22, CX_EMFILE = 24, CX_ENOTTY = 25, CX_ENOSPC = 28, CX_ESPIPE = 29,
    CX_EROFS = 30, CX_ERANGE = 34, CX_ENOSYS = 78,
};

/* flags O_* do macOS (diferem das do Linux usadas pelo kernel) */
#define MAC_O_WRONLY 0x0001
#define MAC_O_RDWR   0x0002
#define MAC_O_APPEND 0x0008
#define MAC_O_CREAT  0x0200
#define MAC_O_TRUNC  0x0400
#define MAC_O_EXCL   0x0800
