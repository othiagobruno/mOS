/* Freestanding: o amalgama do SQLite inclui <assert.h>. Sem aborto. */
#pragma once
#define assert(cond) ((void)(cond))
