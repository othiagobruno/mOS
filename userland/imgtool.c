/* mOS — imgtool: monta e desmonta imagens de disco (.dmg) pelo leitor do kernel (fs/dmg.c, só leitura).
 * Usado pelo /usr/bin/hdiutil de mentira (tools/brew-shims/hdiutil) para os casks .dmg do Homebrew.
 *   imgtool attach <caminho absoluto do .dmg>    imprime os pontos de montagem, um por linha
 *   imgtool detach <ponto de montagem>
 * Saída 0 = ok; 1 = erro (mensagem no stderr). */
#include "mos.h"

static int starts(const char *s, const char *p) { while (*p) if (*s++ != *p++) return 0; return 1; }
static void err(const char *s) { write(2, s, strlen(s)); }

void _start(const char *arg) {
    if (!arg) arg = "";
    if (starts(arg, "attach ")) {
        char out[1024];
        out[0] = 0;
        i64 n = sys3(SYS_IMGATTACH, (u64)(arg + 7), (u64)out, sizeof out);
        if (n <= 0) { err("imgtool: não foi possível montar a imagem\n"); exit(1); }
        puts(out);
        exit(0);
    }
    if (starts(arg, "detach ")) {
        i64 r = sys3(SYS_IMGDETACH, (u64)(arg + 7), 0, 0);
        if (r < 0) { err("imgtool: não é um volume montado\n"); exit(1); }
        exit(0);
    }
    err("uso: imgtool attach <imagem.dmg> | detach <ponto de montagem>\n");
    exit(2);
}
