/* mOS: constantes do <fenv.h> da Apple (iguais às do musl nas duas arquiteturas). As funções estão em
 * userland/libsystem/fenv.c, com o fenv_t da Apple. */
#ifndef _FENV_H
#define _FENV_H
#if defined(__aarch64__)
#define FE_INVALID    0x01
#define FE_DIVBYZERO  0x02
#define FE_OVERFLOW   0x04
#define FE_UNDERFLOW  0x08
#define FE_INEXACT    0x10
#define FE_ALL_EXCEPT 0x1f
#define FE_TONEAREST  0x00000000
#define FE_UPWARD     0x00400000
#define FE_DOWNWARD   0x00800000
#define FE_TOWARDZERO 0x00c00000
#else
#define FE_INVALID    0x01
#define FE_DENORMALOPERAND 0x02
#define FE_DIVBYZERO  0x04
#define FE_OVERFLOW   0x08
#define FE_UNDERFLOW  0x10
#define FE_INEXACT    0x20
#define FE_ALL_EXCEPT 0x3f
#define FE_TONEAREST  0x0000
#define FE_DOWNWARD   0x0400
#define FE_UPWARD     0x0800
#define FE_TOWARDZERO 0x0c00
#endif
int feclearexcept(int);
int feraiseexcept(int);
int fetestexcept(int);
int fegetround(void);
int fesetround(int);
#endif
