/*
 * Quem do Chromium/Electron pode ver o MTLDevice de software.
 *
 * O navegador compõe por software no processo principal. O helper de GPU, sem device,
 * tenta ANGLE/Metal, falha, tenta OpenGL (CGL devolve formato inválido) e o SwiftShader
 * (sem libvulkan) e sai com "errors during initialization". O navegador recria esse
 * helper várias vezes e, nos apps Electron, a janela cai junto. O helper de GPU pede
 * --type=gpu e o executável traz "Helper (GPU)" no caminho. Renderer e utility
 * continuam sem device: o quadro deles passa pelo processo de GPU.
 */
#include "metal_gate.h"

static int has(const char *s, const char *n) {
    if (!s || !n) return 0;
    for (; *s; s++) {
        const char *a = s, *b = n;
        while (*a && *b && *a == *b) { a++; b++; }
        if (!*b) return 1;
    }
    return 0;
}

int cx_chromium_metal_ok(const char *const *images, const char *const *argv) {
    if (images) {
        for (int i = 0; images[i]; i++)
            if (has(images[i], "Helper (GPU)")) return 1;
    }
    if (argv) {
        for (int i = 0; argv[i]; i++)
            if (argv[i][0] == '-' && argv[i][1] == '-' && has(argv[i], "type=gpu")) return 1;
    }
    if (images) {
        for (int i = 0; images[i]; i++) {
            const char *nm = images[i];
            if (has(nm, "Electron Framework") || has(nm, "Chromium Framework") ||
                has(nm, "Chromium Embedded Framework") || has(nm, "Google Chrome Framework") ||
                has(nm, "Microsoft Edge Framework"))
                return 0;
        }
    }
    return 1;
}
