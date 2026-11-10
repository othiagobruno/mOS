/* Espelho da aritmética de _Block_copy / _Block_release em userland/libsystem/objc.c.
 * O objc.c não é alterado aqui. Este processo não é o Electron, o Chromium nem o VS Code:
 * não há binário desses apps nesta árvore, e este ambiente não sobe o kernel. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int fails;
static int freed;
static void expect(int cond, const char *what) {
    if (cond) return;
    fprintf(stderr, "FALHOU %s\n", what);
    fails++;
}

typedef struct { void *isa; int flags, reserved; void *invoke; void *desc; } block_t;

static block_t *copy_block(block_t *b) {
    block_t *n;
    int f;
    if (!b) return NULL;
    if (b->flags & (1 << 28)) return b;
    if (b->flags & (1 << 24)) {
        do { f = b->flags; if ((f & 0xfffe) == 0xfffe) return b; } while (!__sync_bool_compare_and_swap(&b->flags, f, f + 2));
        return b;
    }
    n = malloc(sizeof *n);
    if (!n) return NULL;
    memcpy(n, b, sizeof *n);
    n->flags = (n->flags & ~0xffff) | (1 << 24) | 2;
    return n;
}

static void release_block(block_t *b) {
    int f;
    if (!b || (b->flags & (1 << 28)) || !(b->flags & (1 << 24))) return;
    do {
        f = b->flags;
        if ((f & 0xfffe) == 0xfffe || (f & 0xfffe) == 0) return;
    } while (!__sync_bool_compare_and_swap(&b->flags, f, f - 2));
    if (((f - 2) & 0xfffe) == 0) { freed++; free(b); }
}

static int source_has(const char *path, const char *needle) {
    FILE *f = fopen(path, "rb");
    char *buf;
    long n;
    int ok;
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return 0; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return 0; }
    buf[n] = 0;
    fclose(f);
    ok = strstr(buf, needle) != NULL;
    free(buf);
    return ok;
}

int main(void) {
    block_t stack;
    block_t global;
    block_t *heap;
    const char *src = "userland/libsystem/objc.c";
    memset(&stack, 0, sizeof stack);
    memset(&global, 0, sizeof global);
    global.flags = 1 << 28;
    expect(copy_block(&global) == &global, "global não copia");
    release_block(&global);
    expect(freed == 0, "soltar global não libera");
    release_block(&stack);
    expect(freed == 0, "soltar bloco de pilha não libera");
    heap = copy_block(&stack);
    expect(heap && (heap->flags & (1 << 24)) && (heap->flags & 0xfffe) == 2, "cópia da pilha nasce com NEEDS_FREE e ref 1");
    expect(copy_block(heap) == heap && (heap->flags & 0xfffe) == 4, "segunda cópia soma 2");
    release_block(heap);
    expect(freed == 0 && (heap->flags & 0xfffe) == 2, "uma liberação ainda segura o bloco");
    release_block(heap);
    expect(freed == 1, "a segunda liberação libera uma vez");
    expect(source_has(src, "(1 << 24)") && source_has(src, "f + 2") && source_has(src, "f - 2"),
           "objc.c ainda tem a aritmética que este espelho copia");
    if (fails) { fprintf(stderr, "%d falha(s)\n", fails); return 1; }
    printf("block refcount: ok (espelho; Electron/Chromium/VS Code não rodaram aqui)\n");
    return 0;
}
