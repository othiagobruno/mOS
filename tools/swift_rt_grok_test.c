/* O stdatomic do runtime do Grok compila sem wchar_t e o once só corre uma vez. */
#include <stdio.h>
#include <string.h>

void *swift_allocObject(const void *metadata, unsigned long size, unsigned long align_mask);
void *swift_retain(void *object);
void swift_release(void *object);
void swift_once(long *token, void (*fn)(void *), void *ctx);

static int ran;

static void bump(void *ctx) { ran += *(int *)ctx; }

int main(void) {
    struct { void (*destroy)(void *); } meta = { 0 };
    void *o = swift_allocObject(&meta, 32, 15);
    if (!o) { fprintf(stderr, "alloc\n"); return 1; }
    if (*(const void **)o != &meta) { fprintf(stderr, "metadata\n"); return 1; }
    if (swift_retain(o) != o) { fprintf(stderr, "retain\n"); return 1; }
    swift_release(o);
    swift_release(o);
    long token = 0;
    int n = 3;
    swift_once(&token, bump, &n);
    swift_once(&token, bump, &n);
    if (ran != 3 || token != 1) { fprintf(stderr, "once %d %ld\n", ran, token); return 1; }
    puts("swift-rt-grok: ok");
    return 0;
}
