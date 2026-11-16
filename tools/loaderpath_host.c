/* Teste de host do caminho do carregador EFI (kernel/fs/loaderpath.c).
 * gcc -O2 -Wall -Wextra -DLOADERPATH_HOST -Itools -o /tmp/loaderpath tools/loaderpath_host.c kernel/fs/loaderpath.c && /tmp/loaderpath */
#include <stdio.h>
#include "loaderpath_host.h"

static int falhas, total;
static usize no_arquivo(u8 *d, const char *s) {           /* nó MEDIA_FILEPATH (4/4) com CHAR16 e terminador */
    usize n = strlen(s), len = 4 + (n + 1) * 2;
    d[0] = 4; d[1] = 4; d[2] = (u8)len; d[3] = (u8)(len >> 8);
    for (usize i = 0; i <= n; i++) { d[4 + i * 2] = (u8)s[i]; d[5 + i * 2] = 0; }
    return len;
}
static usize fim(u8 *d) { d[0] = 0x7F; d[1] = 0xFF; d[2] = 4; d[3] = 0; return 4; }
static void caso(const char *nome, const u8 *fp, usize len, bool espera, const char *saida) {
    char out[128];
    bool ok = origin_parse_loader_path(fp, len, out, sizeof out);
    total++;
    if (ok != espera || (espera && strcmp(out, saida)) || (!espera && out[0])) {
        falhas++;
        printf("FALHA %s: ok=%d saída '%s'\n", nome, ok, out);
    }
}

int main(void) {
    u8 b[512];
    usize n;
    n = no_arquivo(b, "\\EFI\\mOS\\BOOTX64.EFI"); n += fim(b + n);
    caso("ESP compartilhada", b, n, true, "/EFI/mOS/BOOTX64.EFI");
    n = no_arquivo(b, "\\EFI\\BOOT\\BOOTX64.EFI"); n += fim(b + n);
    caso("pendrive", b, n, true, "/EFI/BOOT/BOOTX64.EFI");
    n = no_arquivo(b, "\\EFI\\"); n += no_arquivo(b + n, "\\mOS"); n += no_arquivo(b + n, "BOOTX64.EFI"); n += fim(b + n);
    caso("nós separados (firmware antigo)", b, n, true, "/EFI/mOS/BOOTX64.EFI");
    n = no_arquivo(b, "EFI\\mOS\\BOOTX64.EFI"); n += fim(b + n);
    caso("sem barra inicial (GRUB chainloader)", b, n, true, "/EFI/mOS/BOOTX64.EFI");
    n = no_arquivo(b, "\\EFI\\mOS\\..\\ubuntu\\shimx64.efi"); n += fim(b + n);
    caso("com ..", b, n, false, "");
    n = no_arquivo(b, "\\BOOTX64.EFI"); n += fim(b + n);
    caso("na raiz", b, n, false, "");
    n = no_arquivo(b, "\\EFI\\mOS\\sub\\BOOTX64.EFI"); n += fim(b + n);
    caso("pasta funda", b, n, false, "");
    n = no_arquivo(b, "\\EFI\\mOS\\kernel.bin"); n += fim(b + n);
    caso("sem .EFI", b, n, false, "");
    n = no_arquivo(b, "\\EFI\\mOS\\BOOTX64.EFI");
    caso("sem nó final", b, n, false, "");
    b[0] = 4; b[1] = 1; b[2] = 4; b[3] = 0; n = 4 + fim(b + 4);
    caso("nó que não é arquivo", b, n, false, "");
    n = no_arquivo(b, "\\EFI\\mOS\\BOOTX64.EFI"); b[10] = 0xE9; b[11] = 0x00; n += fim(b + n);
    caso("caractere fora do ASCII", b, n, false, "");
    n = no_arquivo(b, "\\EFI\\mOS\\BOOTX64.EFI"); b[2] = 200; n += fim(b + n);
    caso("tamanho de nó além do buffer", b, n, false, "");
    caso("caminho ausente", NULL, 0, false, "");
    printf("loaderpath: %d/%d ok\n", total - falhas, total);
    return falhas != 0;
}
