/* mOS — processo x86_64 faz execve de um binário arm64.
 * O hello-arm64 da imagem de teste sai com 7. O posix_spawn arm64 → este binário → hello-arm64
 * cobre o exec de arm64 a partir de x86_64 (tools/build-a64rt-teste.ps1). */
#include <unistd.h>
int main(void) {
    char *av[] = { "/Sistema/Programas/a64teste/hello-arm64", 0 };
    execv(av[0], av);
    return 127;
}
