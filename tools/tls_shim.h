/* mOS — shim para compilar kernel/net/crypto.c e kernel/net/tls.c no host
 * (tools/tlstest.c). Fornece os tipos do kernel e a API de TCP/TLS de net.h;
 * as funções tcp_* e timer_ticks são implementadas pelo próprio tlstest.c. */
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef size_t usize;

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#define kmalloc(n) malloc(n)
#define kfree(p) free(p)
#define kprintf(...) fprintf(stderr, __VA_ARGS__)

static inline u64 rdtsc(void) {
    u32 lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (u64)hi << 32 | lo;
}
u64 timer_ticks(void);

/* relógio de parede (kernel.h: rtc_now); tlstest.c usa o relógio UTC do Windows,
 * com possibilidade de forçar uma data para testes */
typedef struct { int year, month, day, hour, minute, second, weekday; } datetime_t;
void rtc_now(datetime_t *dt);
void rtc_now_utc(datetime_t *dt);

/* espelho de kernel/include/net.h (parte TCP/TLS) */
enum { TCP_CLOSED, TCP_CONNECTING, TCP_OPEN, TCP_PEER_CLOSED, TCP_ERROR };
int  tcp_state(int s);
int  tcp_send(int s, const void *data, int len);
int  tcp_recv(int s, void *buf, int max);

typedef struct tls_conn tls_conn_t;
tls_conn_t *tls_new(int tcp_sock, const char *sni);
int tls_set_alpn(tls_conn_t *c, const u8 *list, u32 len);
const char *tls_get_alpn(const tls_conn_t *c);
int tls_get_version(const tls_conn_t *c);
int tls_get_suite(const tls_conn_t *c);
int  tls_handshake(tls_conn_t *c);
int  tls_send(tls_conn_t *c, const void *data, int len);
int  tls_recv(tls_conn_t *c, void *buf, int max);
int  tls_pending(const tls_conn_t *c);   /* 1: tls_recv entrega sem ler mais do TCP */
/* certificado de cliente: cadeia como em tls_copy_peer_chain (folha primeiro) e chave privada DER (PKCS#1, SEC1 ou
 * PKCS#8, RSA ou EC P-256/P-384, que tem de ser a da folha); antes do handshake. 0 ok */
int  tls_set_client_identity(tls_conn_t *c, const u8 *chain, u32 chain_len, const u8 *key, u32 key_len);
void tls_free(tls_conn_t *c);
const char *tls_error(tls_conn_t *c);
