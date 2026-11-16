/* stdio para os codecs de mídia (FDK AAC: genericStds.cpp). Fica fora do shim comum porque o
 * sqlite_vfs.inc declara fopen/fread/... com void* e o SQLite compila com o shim comum. */
#pragma once
#include "../shim/stdio.h"
#ifdef __cplusplus
extern "C" {
#endif
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define EOF (-1)
typedef __builtin_va_list __mos_va_list;
extern FILE *__stdinp, *__stdoutp, *__stderrp;
#define stdin __stdinp
#define stdout __stdoutp
#define stderr __stderrp
FILE *fopen(const char *p, const char *m);
int fclose(FILE *f);
int fseek(FILE *f, long off, int whence);
long ftell(FILE *f);
int fflush(FILE *f);
size_t fwrite(const void *p, size_t z, size_t n, FILE *f);
size_t fread(void *p, size_t z, size_t n, FILE *f);
char *fgets(char *s, int n, FILE *f);
int feof(FILE *f);
int getchar(void);
int vprintf(const char *fmt, __mos_va_list ap);
int vfprintf(FILE *f, const char *fmt, __mos_va_list ap);
int vsprintf(char *s, const char *fmt, __mos_va_list ap);
int snprintf(char *s, size_t n, const char *fmt, ...);
#ifdef __cplusplus
}
#endif
