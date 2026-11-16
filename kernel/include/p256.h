/*
 * mOS — NIST P-256 (kernel/bt/p256.c): API de baixo nível usada pelo WPA3-SAE (kernel/wifi/sae.c).
 * Elementos do corpo em 8 palavras de 32 bits, a palavra 0 é a menos significativa.
 */
#pragma once
#ifdef MOS_HOST_TEST
#include "host_shim.h"
#else
#include "kernel.h"
#endif

typedef struct { u32 v[8]; } p256_fe;
void p256_fe_mul(p256_fe *r, const p256_fe *a, const p256_fe *b);
void p256_fe_add(p256_fe *r, const p256_fe *a, const p256_fe *b);
void p256_fe_sub(p256_fe *r, const p256_fe *a, const p256_fe *b);
void p256_fe_inv(p256_fe *r, const p256_fe *a);
void p256_fe_pow(p256_fe *r, const p256_fe *a, const u32 e[8]);     /* tempo constante no expoente */
int  p256_fe_cmp(const p256_fe *a, const p256_fe *b);
bool p256_fe_is_zero(const p256_fe *a);
const p256_fe *p256_prime(void);
const p256_fe *p256_b(void);
const p256_fe *p256_gx(void);
const p256_fe *p256_gy(void);
bool p256_on_curve_fe(const p256_fe *x, const p256_fe *y);
bool p256_pt_mul(p256_fe *ox, p256_fe *oy, const u32 k[8], const p256_fe *x, const p256_fe *y);   /* false = infinito */
bool p256_pt_add(p256_fe *ox, p256_fe *oy, const p256_fe *ax, const p256_fe *ay, const p256_fe *bx, const p256_fe *by);
