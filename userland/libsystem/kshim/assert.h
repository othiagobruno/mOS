/* Freestanding: o zig 0.17 não entrega assert.h no alvo none. O sqlite3 do libSystem inclui este cabeçalho. */
#pragma once
#ifdef NDEBUG
#define assert(cond) ((void)0)
#else
#define assert(cond) ((void)(cond))
#endif
