/* Filho do vforktest: só confirma que o exec trocou a imagem. */
#include "mos.h"
void _start(void) { exit(9); }
