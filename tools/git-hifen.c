/* mOS — nome com hífen do git (git-upload-pack, git-remote-https...) em /usr/libexec/git-core.
 * A imagem não guarda hardlink nem symlink; uma cópia do git por nome custaria alguns MiB cada.
 * Este programa faz exec de ALVO (definido por tools/build-git.sh) mantendo argv: o git vê
 * argv[0] = "git-upload-pack" e roda o builtin "upload-pack"; o git-remote-http ignora argv[0]. */
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    (void)argc;
    execv(ALVO, argv);
    perror(ALVO);
    return 127;
}
