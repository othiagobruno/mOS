#pragma once
/* NDEBUG: o argumento some. O amalgamation do SQLite chama, dentro do assert,
 * símbolos que só existem com SQLITE_DEBUG. */
#define assert(cond) ((void)0)
