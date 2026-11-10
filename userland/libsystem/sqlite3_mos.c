/* SQLite 3.54.1 (domínio público) dentro do libSystem.
 * SQLITE_OS_OTHER: o VFS abaixo usa o stdio do mOS. Sem extensão carregável.
 * sqlite3_open_v2 / sqlite3_exec / sqlite3_close saem daqui; o stub de
 * compat_agentes.c foi retirado para não duplicar o símbolo.
 */
#define SQLITE_OS_OTHER 1
#define SQLITE_OMIT_LOAD_EXTENSION 1
#define SQLITE_THREADSAFE 0
#define SQLITE_MAX_MMAP_SIZE 0
#define SQLITE_OMIT_WAL 1
#define SQLITE_OMIT_DATETIME_FUNCS 1
#include "../third_party/sqlite/sqlite3.c"
#include "sqlite_vfs.inc"
