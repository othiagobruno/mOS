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

/* espelho de kernel/include/net.h (parte TCP/TLS) */
enum { TCP_CLOSED, TCP_CONNECTING, TCP_OPEN, TCP_PEER_CLOSED, TCP_ERROR };
int  tcp_state(int s);
int  tcp_send(int s, const void *data, int len);
int  tcp_recv(int s, void *buf, int max);

typedef struct tls_conn tls_conn_t;
tls_conn_t *tls_new(int tcp_sock, const char *sni);
int  tls_handshake(tls_conn_t *c);
int  tls_send(tls_conn_t *c, const void *data, int len);
int  tls_recv(tls_conn_t *c, void *buf, int max);
void tls_free(tls_conn_t *c);
const char *tls_error(tls_conn_t *c);
