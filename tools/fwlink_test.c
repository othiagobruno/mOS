/* Host: reconhece o texto que o Windows grava no lugar do symlink de um .framework. */
#include <stdio.h>
#include <string.h>
#include "../kernel/include/fwlink.h"

static int fails;

static void expect(int cond, const char *what) {
    if (cond) return;
    fprintf(stderr, "FALHOU %s\n", what);
    fails++;
}

static int target_is(const char *in, const char *want) {
    char out[512];
    int ok = fwlink_target(in, strlen(in), out, sizeof out);
    if (!want) return !ok;
    return ok && !strcmp(out, want);
}

int main(void) {
    char out[560];
    unsigned char macho[4] = {0xCF, 0xFA, 0xED, 0xFE};
    unsigned char fat[4] = {0xCA, 0xFE, 0xBA, 0xBE};
    unsigned long long n;
    expect(target_is("A", "A"), "componente A");
    expect(target_is("A\n", "A"), "A com newline");
    expect(target_is("Versions/Current/Electron Framework", "Versions/Current/Electron Framework"), "caminho com espaço");
    expect(target_is("#mOS-link:Versions/A/X\n", "Versions/A/X"), "já no formato do VFS");
    expect(target_is("../Cellar/tree/2.3.2", "../Cellar/tree/2.3.2"), "relativo do Homebrew");
    expect(target_is("", NULL), "vazio");
    expect(target_is(".", NULL), "ponto");
    expect(!fwlink_target(macho, 4, out, sizeof out), "Mach-O");
    expect(!fwlink_target(fat, 4, out, sizeof out), "FAT");
    expect(target_is("<plist version=\"1.0\">", NULL), "plist xml");
    expect(target_is("bplist00", NULL), "bplist");
    expect(target_is("https://x", NULL), "url");
    expect(target_is("{", NULL), "json");
    n = fwlink_canonicalize("A\n", 2, out, sizeof out);
    expect(n == strlen("#mOS-link:A") && !strcmp(out, "#mOS-link:A"), "canon A");
    expect(fwlink_canonicalize(out, n, out, sizeof out) == 0, "canon é idempotente");
    if (fails) { fprintf(stderr, "%d falha(s)\n", fails); return 1; }
    printf("fwlink: ok\n");
    return 0;
}
