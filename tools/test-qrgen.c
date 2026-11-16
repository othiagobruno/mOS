/* Host: imprime a matriz do qrgen.c (uso: test-qrgen nível versão máscara hex_da_mensagem) para o tools/test-qrgen.sh. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qrgen.h"
int main(int argc, char **argv) {
    if (argc < 5) return 2;
    static uint8_t msg[4096], out[177 * 177];
    int len = (int)strlen(argv[4]) / 2, size = 0;
    for (int i = 0; i < len; i++) { unsigned v; sscanf(argv[4] + 2 * i, "%2x", &v); msg[i] = (uint8_t)v; }
    int v = mos_qr_encode(msg, len, atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), out, &size);
    printf("%d %d\n", v, size);
    for (int y = 0; y < size; y++) { for (int x = 0; x < size; x++) putchar('0' + out[y * size + x]); putchar('\n'); }
    return v < 0;
}
