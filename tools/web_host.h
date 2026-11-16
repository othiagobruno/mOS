/* mOS — teste de host do motor web: contadores, CHECK e registro de casos (tools/web_host_*.c) */
#pragma once
#include "kernel.h"
int  host_printf(const char *fmt, ...);
u8  *host_read_file(const char *path, usize *n);
char *getenv(const char *);
extern int web_host_ok, web_host_fail;
#define CHECK(c, ...) do { if (c) web_host_ok++; else { web_host_fail++; host_printf("FALHOU %s:%d: ", __FILE__, __LINE__); \
    host_printf(__VA_ARGS__); host_printf("\n"); } } while (0)
void web_host_register(const char *name, void (*fn)(void));
#define WEB_HOST_CASE(fn) static void fn(void); __attribute__((constructor)) static void reg_##fn(void) { web_host_register(#fn, fn); } static void fn(void)
