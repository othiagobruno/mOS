/* mOS — RSA do crypto.subtle (kernel/web/web_rsa.c). Todos os números em big-endian, kmalloc. */
#ifndef WEB_RSA_H
#define WEB_RSA_H
#include "kernel.h"
typedef struct { u8 *n, *e, *d, *p, *q, *dp, *dq, *qi; u32 nl, el, dl, pl, ql, dpl, dql, qil; } wrsa_key_t;
bool wrsa_generate(u32 bits, const u8 *e, u32 el, wrsa_key_t *k);   /* 512..4096 bits, expoente de até 32 bits */
bool wrsa_public(const u8 *n, u32 nl, const u8 *e, u32 el, const u8 *in, u32 inl, u8 *out);   /* out: nl bytes */
bool wrsa_private(const wrsa_key_t *k, const u8 *in, u32 inl, u8 *out);                        /* CRT; out: nl bytes */
void wrsa_free(wrsa_key_t *k);
#endif
