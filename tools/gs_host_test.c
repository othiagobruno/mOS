/* O clone do Linux copia o GS do pai. Uma thread nova do kernel nasce com gs_base 0
 * (process.c). Este teste trava o contrato que o machotest reproduz em thread_main. */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned long parent_gs;
static unsigned long child_inherited;
static unsigned long child_cleared;

static unsigned long rdgs(void) {
    unsigned long v;
    __asm__ volatile("rdgsbase %0" : "=r"(v));
    return v;
}

static void *child(void *arg) {
    (void)arg;
    child_inherited = rdgs();
    __asm__ volatile("wrgsbase %0" :: "r"((unsigned long)0) : "memory");
    child_cleared = rdgs();
    return NULL;
}

int main(void) {
    pthread_t th;
    parent_gs = (unsigned long)&parent_gs;
    __asm__ volatile("wrgsbase %0" :: "r"(parent_gs) : "memory");
    if (pthread_create(&th, NULL, child, NULL) != 0) return 2;
    pthread_join(th, NULL);
    __asm__ volatile("wrgsbase %0" :: "r"((unsigned long)0) : "memory");
    if (child_inherited != parent_gs) {
        fprintf(stderr, "filho não herdou o GS (%lx != %lx)\n", child_inherited, parent_gs);
        return 1;
    }
    if (child_cleared != 0) {
        fprintf(stderr, "wrgsbase 0 não zerou (%lx)\n", child_cleared);
        return 1;
    }
    printf("gs-zero ok\n");
    return 0;
}
