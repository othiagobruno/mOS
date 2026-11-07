/*
 * mOS - machosym: traduz endereços de uma imagem Mach-O x86_64 no símbolo mais próximo (LC_SYMTAB).
 *   machosym <arquivo Mach-O> <deslocamento>...      deslocamento = endereço - base de carga (o mOS carrega
 *                                                      o executável em 0x400001000000; vmaddr do __TEXT somado)
 * Compilar: zig cc -O2 -target x86_64-windows-gnu tools/machosym.c -o build/machosym.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { uint32_t strx; uint8_t type, sect; uint16_t desc; uint64_t value; } nlist64;

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "uso: machosym <mach-o> <deslocamento>...\n"); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) return 1;
    fclose(f);
    if (*(uint32_t *)b != 0xFEEDFACF) { fprintf(stderr, "não é Mach-O 64 (fat não suportado)\n"); return 1; }
    uint32_t ncmds = *(uint32_t *)(b + 16);
    uint8_t *c = b + 32;
    uint64_t text_vm = 0;
    nlist64 *syms = NULL;
    uint32_t nsyms = 0;
    const char *strs = NULL;
    for (uint32_t i = 0; i < ncmds; i++) {
        uint32_t cmd = *(uint32_t *)c, sz = *(uint32_t *)(c + 4);
        if (cmd == 0x19 && !strncmp((char *)c + 8, "__TEXT", 16)) text_vm = *(uint64_t *)(c + 24);
        if (cmd == 0x2) {
            syms = (nlist64 *)(b + *(uint32_t *)(c + 8));
            nsyms = *(uint32_t *)(c + 12);
            strs = (const char *)b + *(uint32_t *)(c + 16);
        }
        c += sz;
    }
    if (!syms) { fprintf(stderr, "sem LC_SYMTAB\n"); return 1; }
    for (int a = 2; a < argc; a++) {
        uint64_t off = strtoull(argv[a], NULL, 16), va = text_vm + off;
        const nlist64 *best = NULL;
        for (uint32_t i = 0; i < nsyms; i++) {
            const nlist64 *s = &syms[i];
            if ((s->type & 0xE0) || (s->type & 0x0E) != 0x0E || !s->sect) continue;   /* só símbolos definidos em seção */
            if (s->value <= va && (!best || s->value > best->value)) best = s;
        }
        if (best) printf("%s -> %s+0x%llx\n", argv[a], strs + best->strx, (unsigned long long)(va - best->value));
        else printf("%s -> ?\n", argv[a]);
    }
    return 0;
}
