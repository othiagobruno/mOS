/* Teste de host: blocos de verdade (clang -fblocks) sobre o runtime de blocos do libSystem e a libdispatch do mOS.
 *   sh tools/blocks_hosttest.sh */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define C(n) char n[16] __asm__("OBJC_CLASS_$_" #n)
C(OS_dispatch_queue_main); C(OS_dispatch_queue_global); C(OS_dispatch_queue_serial); C(OS_dispatch_queue_concurrent);
C(OS_dispatch_group); C(OS_dispatch_semaphore); C(OS_dispatch_queue_attr); C(OS_dispatch_workloop);
C(OS_dispatch_source); C(OS_dispatch_data); C(OS_dispatch_io);

typedef void (^blk_t)(void);
void *dispatch_queue_create(const char *, void *);
void dispatch_async(void *, blk_t);
void dispatch_sync(void *, blk_t);
void *dispatch_group_create(void);
void dispatch_group_async(void *, void *, blk_t);
long dispatch_group_wait(void *, unsigned long long);

static int fails, total;
#define CHECK(c, ...) do { total++; if (c) printf("  ok    "); else { fails++; printf("  FALHA "); } printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)

int main(void) {
    printf("blocks_hosttest: runtime de blocos + libdispatch\n");
    void *q = dispatch_queue_create("t.laco", 0);

    /* __block lida antes e depois de um bloco ser copiado de dentro de outro */
    __block int k = 0;
    static char ordem[8];
    char *ord = ordem;
    dispatch_async(q, ^{ ord[k++] = 'a'; dispatch_async(q, ^{ ord[k++] = 'c'; }); ord[k++] = 'b'; });
    dispatch_sync(q, ^{ ord[k++] = 'd'; });
    dispatch_sync(q, ^{});
    CHECK(ordem[0] == 'a' && ordem[1] == 'b' && strlen(ordem) == 4 && k == 4,
          "async aninhado na fila serial: a antes de b, quatro escritas (%s, k=%d)", ordem, k);

    /* bloco que se reagenda 100 mil vezes; o grupo espera o último */
    void *g = dispatch_group_create();
    __block int voltas = 0;
    __block void (^passo)(void);
    passo = ^{ if (++voltas < 100000) dispatch_group_async(g, q, passo); };
    dispatch_group_async(g, q, passo);
    dispatch_group_wait(g, ~0ull);
    CHECK(voltas == 100000, "bloco que se reagenda 100 mil vezes (%d)", voltas);

    /* a mesma __block vista por dois blocos copiados em momentos diferentes */
    __block int n = 0;
    blk_t inc = ^{ n++; };
    dispatch_async(q, inc);
    dispatch_async(q, ^{ dispatch_async(q, inc); n += 10; });
    dispatch_sync(q, ^{});
    dispatch_sync(q, ^{});
    CHECK(n == 12, "__block compartilhada entre cópias (%d)", n);

    printf("blocks_hosttest: %d de %d ok\n", total - fails, total);
    return fails;
}
