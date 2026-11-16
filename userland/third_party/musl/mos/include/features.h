/* mOS: features.h para compilar a libm do musl 1.2.5 dentro do libSystem (x86_64 e arm64 Darwin).
 * O triplo do Darwin não tem __attribute__((alias)): os aliases fracos do musl viram funções em ../aliases.c.
 * signgam é a variável global da Apple; o musl grava em __signgam, que aqui é o mesmo objeto. */
#ifndef _FEATURES_H
#define _FEATURES_H
#define _GNU_SOURCE 1
#define _BSD_SOURCE 1
#define _XOPEN_SOURCE 700
#define __restrict restrict
#define __inline inline
#define __REDIR(x,y) __typeof__(x) x __asm__(#y)
#define weak __attribute__((__weak__))
#define hidden __attribute__((__visibility__("hidden")))
#define weak_alias(old, new) extern __typeof(old) new
#define __signgam signgam
#endif
