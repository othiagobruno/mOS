/* mOS — aparência sem libc: a tabela das cores do sistema (clara e escura, as de destaque derivadas da cor de destaque
 * do window server) e a escolha da aparência mais parecida (bestMatchFromAppearancesWithNames:). Usado por
 * appkit_appearance.c. Teste: userland/tests-appkit/appearance_host.c. Código original do mOS. */
#ifndef APPEARANCE_CORE_H
#define APPEARANCE_CORE_H
int ac_count(void);
const char *ac_name(int i);              /* seletor da cor (ex.: "labelColor") */
int ac_index(const char *sel);           /* -1 se não é cor do sistema */
/* componentes r, g, b, a da cor i no tema claro (dark 0) ou escuro (1); accent_rgb = 0xRRGGBB */
void ac_resolve(int i, int dark, unsigned accent_rgb, double out[4]);
int ac_is_dark(const char *appearance_name);
/* índice do nome de names mais parecido com mine (o mesmo; senão do mesmo lado, vibração e contraste); -1 se nenhum */
int ac_best_match(const char *mine, const char *const *names, int n);
#endif
