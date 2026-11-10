/* Freestanding. O SQLite chama funções que só existem com SQLITE_DEBUG. */
#pragma once
#define assert(c) ((void)0)
