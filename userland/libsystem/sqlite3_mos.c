/* SQLite 3.54.1 (domínio público) dentro do libSystem.
 * SQLITE_OS_OTHER: o VFS abaixo usa o stdio do mOS. Sem extensão carregável.
 * sqlite3_open_v2 / sqlite3_exec / sqlite3_close saem daqui; o stub de
 * compat_agentes.c foi retirado para não duplicar o símbolo.
 */
#define NDEBUG 1
#define SQLITE_OS_OTHER 1
#define SQLITE_OMIT_LOAD_EXTENSION 1
#define SQLITE_THREADSAFE 0
#define SQLITE_MAX_MMAP_SIZE 0
#define SQLITE_OMIT_WAL 1
#define SQLITE_OMIT_DATETIME_FUNCS 1
/* kshim/string.h e kshim/math.h não têm o que o amálgama usa. fsinc tem, mas o -I
 * do kshim vem primeiro. */
#include <stddef.h>
void *memchr(const void *s, int c, size_t n);
int strncmp(const char *a, const char *b, size_t n);
char *strrchr(const char *s, int c);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
#ifndef INFINITY
#define INFINITY (__builtin_inff())
#endif
#include "../third_party/sqlite/sqlite3.c"
#include "sqlite_vfs.inc"
