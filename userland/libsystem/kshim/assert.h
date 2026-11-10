#pragma once
/* Freestanding. Não avalia o argumento: o sqlite chama funções de debug dentro do assert. */
#define assert(cond) ((void)0)
