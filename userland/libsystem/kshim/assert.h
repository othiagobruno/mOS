/* Freestanding. O SQLite chama funções que só existem com SQLITE_DEBUG. */
#pragma once
#define assert(c) ((void)0)
#ifndef static_assert
#define static_assert(cond, msg) _Static_assert(cond, msg)
#endif
