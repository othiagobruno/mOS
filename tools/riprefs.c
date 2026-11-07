/* riprefs <arquivo> <texto_ini> <texto_fim> <alvo1> [alvo2...]
 * Procura em [texto_ini, texto_fim) instruções com operando rip-relativo (modrm mod=00 rm=101) cujo alvo é
 * um dos endereços dados. Supõe __TEXT com vmaddr 0 = deslocamento no arquivo. Também acha ponteiros
 * de 8 bytes (opcional, alvo com prefixo p:) em todo o arquivo, comparando os 36 bits baixos (chained fixups).
 *
 * Uso típico (binário sem símbolos, ex.: Electron Framework): o deslocamento de um nome de TRACE_EVENT
 * (grep -boa "Display::DrawAndSwap" arquivo) -> riprefs acha quem o carrega -> o "push rbp; mov rbp,rsp" anterior
 * é o início da função, que vira tracepoint (tools\vscode-teste.ps1 -Tracepoints).
 * Compilar: .tools\zig-*\zig.exe cc -target x86_64-windows-gnu -O2 tools\riprefs.c -o build\riprefs.exe */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "uso: riprefs arq ini fim alvo...\n"); return 1; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fprintf(stderr, "leitura\n"); return 1; }
    uint64_t lo = strtoull(argv[2], 0, 0), hi = strtoull(argv[3], 0, 0);
    int nt = argc - 4;
    uint64_t t[32]; int ptr[32];
    for (int i = 0; i < nt && i < 32; i++) {
        ptr[i] = argv[4 + i][0] == 'p' && argv[4 + i][1] == ':';
        t[i] = strtoull(argv[4 + i] + (ptr[i] ? 2 : 0), 0, 0);
    }
    for (uint64_t i = lo; i + 7 < hi && i + 7 < (uint64_t)n; i++) {
        /* opcode de 1 byte (lea 8d / mov 8b / mov 89 / cmp 39,3b / push ff) seguido de modrm 00 reg 101 */
        uint8_t op = b[i], m = b[i + 1];
        if ((m & 0xC7) != 0x05) continue;
        if (op != 0x8D && op != 0x8B && op != 0x89 && op != 0x3B && op != 0x39 && op != 0xFF && op != 0x0B && op != 0x33) continue;
        int32_t d; memcpy(&d, b + i + 2, 4);
        uint64_t next = i + 6;
        uint64_t tg = next + (int64_t)d;
        for (int k = 0; k < nt; k++) if (!ptr[k] && tg == t[k]) {
            int rex = i > 0 && (b[i - 1] & 0xF0) == 0x40;
            printf("ref 0x%llx -> 0x%llx (insn em 0x%llx, op %02x)\n", (unsigned long long)i, (unsigned long long)tg,
                   (unsigned long long)(rex ? i - 1 : i), op);
        }
    }
    for (int k = 0; k < nt; k++) if (ptr[k]) {
        for (long i = 0; i + 8 <= n; i += 8) {
            uint64_t v; memcpy(&v, b + i, 8);
            if ((v & 0xFFFFFFFFFULL) == t[k]) printf("ptr em arquivo 0x%lx = %016llx -> 0x%llx\n", i, (unsigned long long)v, (unsigned long long)t[k]);
        }
    }
    return 0;
}
