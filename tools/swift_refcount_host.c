/* Teste de host da contagem de referência Swift. Não usa QEMU. */
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void *swift_allocObject(const void *metadata, size_t size, size_t align_mask);
void swift_deallocObject(void *object, size_t size, size_t align_mask);
void swift_deallocUninitializedObject(void *object, size_t size, size_t align_mask);
void *swift_initStackObject(const void *metadata, void *object);
void *swift_retain(void *object);
void *swift_retain_n(void *object, uint32_t n);
void swift_release(void *object);
void swift_release_n(void *object, uint32_t n);
void *swift_tryRetain(void *object);
int swift_isDeallocating(void *object);
void *swift_unknownObjectRetain(void *object);
void swift_unknownObjectRelease(void *object);
void *swift_bridgeObjectRetain(void *object);
void swift_bridgeObjectRelease(void *object);
void *swift_nonatomic_retain(void *object);
void swift_nonatomic_release(void *object);
void swift_weakInit(void *ref, void *value);
void swift_weakAssign(void *ref, void *value);
void *swift_weakLoadStrong(void *ref);
void *swift_weakTakeStrong(void *ref);
void swift_weakDestroy(void *ref);
void swift_weakCopyInit(void *dest, void *src);
void swift_unownedRetain(void *object);
void swift_unownedRelease(void *object);
void *swift_unownedRetainStrong(void *object);
void swift_unownedCheck(void *object);
uint64_t mos_swift_rc_frees(void);
void mos_swift_rc_make_immortal(void *object);

static int g_destroys;
static int g_fail;

static void expect(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "falhou: %s\n", msg);
        g_fail = 1;
    }
}

static void destroy_keep(void *obj) {
    g_destroys++;
    swift_deallocObject(obj, 32, 15);
}

static void destroy_abort_strong(void *obj) {
    swift_unownedRetainStrong(obj);
    swift_deallocObject(obj, 32, 15);
}

static void destroy_abort_check(void *obj) {
    swift_unownedCheck(obj);
    swift_deallocObject(obj, 32, 15);
}

static void destroy_check_flag(void *obj) {
    g_destroys++;
    if (!swift_isDeallocating(obj)) g_fail = 1;
    if (swift_tryRetain(obj) != 0) g_fail = 1;
    swift_deallocObject(obj, 32, 15);
}

static const void *meta_keep[] = { (const void *)destroy_keep };
static const void *meta_flag[] = { (const void *)destroy_check_flag };

static void *worker(void *arg) {
    void *obj = arg;
    for (int i = 0; i < 2000; i++) {
        swift_retain(obj);
        swift_release(obj);
        swift_retain_n(obj, 3);
        swift_release_n(obj, 3);
    }
    return 0;
}

static void *last_release(void *arg) {
    swift_release(arg);
    return 0;
}

int main(void) {
    uint64_t frees0 = mos_swift_rc_frees();
    g_destroys = 0;
    void *obj = swift_allocObject(meta_flag, 32, 15);
    expect(obj != 0, "alloc");
    expect(!swift_isDeallocating(obj), "vivo nao esta em deinit");
    swift_retain(obj);
    swift_release(obj);
    expect(g_destroys == 0, "ainda ha uma forte");
    swift_release(obj);
    expect(g_destroys == 1, "destrutor uma vez");
    expect(mos_swift_rc_frees() == frees0 + 1, "memoria liberada");

    g_destroys = 0;
    obj = swift_allocObject(meta_keep, 32, 15);
    swift_retain_n(obj, 4);
    swift_release_n(obj, 4);
    expect(g_destroys == 0, "retain_n/release_n equilibrado");
    swift_release(obj);
    expect(g_destroys == 1, "release final depois do _n");

    g_destroys = 0;
    uint64_t frees1 = mos_swift_rc_frees();
    obj = swift_allocObject(meta_keep, 32, 15);
    mos_swift_rc_make_immortal(obj);
    swift_retain_n(obj, 8);
    swift_release_n(obj, 8);
    swift_release(obj);
    swift_unownedRetain(obj);
    swift_unownedRelease(obj);
    expect(g_destroys == 0, "imortal nao chama destrutor");
    expect(mos_swift_rc_frees() == frees1, "imortal nao libera");
    expect(!swift_isDeallocating(obj), "imortal nao esta em deinit");

    g_destroys = 0;
    obj = swift_allocObject(meta_keep, 32, 15);
    struct { void *side; } w, w2;
    memset(&w, 0, sizeof w);
    memset(&w2, 0, sizeof w2);
    swift_weakInit(&w, obj);
    void *loaded = swift_weakLoadStrong(&w);
    expect(loaded == obj, "weak carrega forte");
    swift_release(loaded);
    loaded = swift_weakLoadStrong(&w);
    swift_release(loaded);
    swift_weakCopyInit(&w2, &w);
    swift_release(obj);
    expect(g_destroys == 1, "weak nao impede o destrutor");
    expect(swift_weakLoadStrong(&w) == 0, "weak vira nil");
    expect(swift_weakLoadStrong(&w2) == 0, "copia do weak vira nil");
    void *taken = swift_weakTakeStrong(&w);
    expect(taken == 0, "take depois do dealloc");
    swift_weakDestroy(&w2);

    g_destroys = 0;
    obj = swift_allocObject(meta_keep, 64, 15);
    swift_unownedRetain(obj);
    swift_release(obj);
    expect(g_destroys == 1, "destrutor com unowned vivo");
    expect(mos_swift_rc_frees() == frees1 + 1, "unowned segura a memoria");
    swift_unownedRelease(obj);
    expect(mos_swift_rc_frees() == frees1 + 2, "unowned zero libera");

    obj = swift_allocObject(meta_keep, 32, 15);
    void *tagged = (void *)((uintptr_t)obj | 2u);
    expect(swift_bridgeObjectRetain(tagged) == tagged, "bridge devolve o tag");
    swift_bridgeObjectRelease(tagged);
    swift_unknownObjectRetain(obj);
    swift_unknownObjectRelease(obj);
    swift_nonatomic_retain(obj);
    swift_nonatomic_release(obj);
    swift_release(obj);

    obj = swift_allocObject(meta_keep, 32, 15);
    uint64_t before_uninit = mos_swift_rc_frees();
    g_destroys = 0;
    swift_deallocUninitializedObject(obj, 32, 15);
    expect(g_destroys == 0, "nao inicializado nao chama destrutor");
    expect(mos_swift_rc_frees() == before_uninit + 1, "nao inicializado libera");

    unsigned char stackbuf[64];
    memset(stackbuf, 0, sizeof stackbuf);
    g_destroys = 0;
    uint64_t before_stack = mos_swift_rc_frees();
    void *stack = swift_initStackObject(meta_keep, stackbuf);
    swift_release(stack);
    expect(g_destroys == 1, "pilha chama destrutor");
    expect(mos_swift_rc_frees() == before_stack, "pilha nao da free");

    g_destroys = 0;
    obj = swift_allocObject(meta_keep, 32, 15);
    pthread_t th[4];
    for (int i = 0; i < 4; i++) pthread_create(&th[i], 0, worker, obj);
    for (int i = 0; i < 4; i++) pthread_join(th[i], 0);
    expect(g_destroys == 0, "concorrencia nao destruiu cedo");
    swift_release(obj);
    expect(g_destroys == 1, "uma destruicao depois das threads");

    g_destroys = 0;
    obj = swift_allocObject(meta_keep, 32, 15);
    swift_retain_n(obj, 3);
    pthread_t last[4];
    for (int i = 0; i < 4; i++) pthread_create(&last[i], 0, last_release, obj);
    for (int i = 0; i < 4; i++) pthread_join(last[i], 0);
    expect(g_destroys == 1, "quatro releases concorrentes, um destrutor");

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        const void *meta[] = { (const void *)destroy_abort_strong };
        void *dying = swift_allocObject(meta, 32, 15);
        swift_release(dying);
        _exit(0);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    expect(WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT, "unowned strong check aborta");

    pid = fork();
    if (pid == 0) {
        const void *meta[] = { (const void *)destroy_abort_check };
        void *dying = swift_allocObject(meta, 32, 15);
        swift_release(dying);
        _exit(0);
    }
    waitpid(pid, &st, 0);
    expect(WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT, "unownedCheck aborta");

    if (g_fail) return 1;
    puts("swift_refcount_host: ok");
    return 0;
}
