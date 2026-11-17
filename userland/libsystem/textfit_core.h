/* mOS — encaixe de texto de uma fonte só, sem libc: quebra de linha (por palavra ou por caractere), contagem de linhas e
 * truncamento com reticências no início, no meio ou no fim (NSLineBreakMode). UTF-8: nunca corta no meio de um caractere.
 * A largura vem de quem usa (measure). Usado pelo NSTextField (appkit.c). Teste: userland/tests-appkit/textfit_host.c.
 * Código original do mOS. */
#ifndef TEXTFIT_CORE_H
#define TEXTFIT_CORE_H

typedef double (*tfit_measure_fn)(const char *s, int n, void *ctx);

/* NSLineBreakMode */
enum { TFIT_WORD = 0, TFIT_CHAR = 1, TFIT_CLIP = 2, TFIT_HEAD = 3, TFIT_TAIL = 4, TFIT_MIDDLE = 5 };

/* próxima linha a partir de s (até \n ou o fim): devolve quantos bytes cabem em w (pelo menos um caractere) e, em *skip,
 * quantos bytes pular depois dela (espaços da quebra e o \n). mode: TFIT_WORD ou TFIT_CHAR. */
int tfit_break(const char *s, double w, int mode, tfit_measure_fn m, void *ctx, int *skip);
/* número de linhas do texto quebrado em w */
int tfit_lines(const char *s, double w, int mode, tfit_measure_fn m, void *ctx);
/* texto de uma linha que cabe em w com "…" (HEAD, MIDDLE ou TAIL); devolve o tamanho escrito em out (sem o 0 final).
 * Se o texto já cabe, é copiado igual. */
int tfit_truncate(const char *s, int n, double w, int mode, tfit_measure_fn m, void *ctx, char *out, int cap);
#endif
