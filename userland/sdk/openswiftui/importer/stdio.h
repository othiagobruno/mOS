/*
 * mOS SDK (importador) — stdio.h mínimo para os cabeçalhos C que o Swift importa (veja pthread.h). Funções do libSystem.
 */
#pragma once
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct __sFILE FILE;
extern FILE *__stdinp;
extern FILE *__stdoutp;
extern FILE *__stderrp;
#define stdin __stdinp
#define stdout __stdoutp
#define stderr __stderrp
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int snprintf(char *buffer, size_t size, const char *format, ...);
int vsnprintf(char *buffer, size_t size, const char *format, va_list args);
int puts(const char *s);
int fputs(const char *s, FILE *stream);
int fflush(FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream);
#ifdef __cplusplus
}
#endif