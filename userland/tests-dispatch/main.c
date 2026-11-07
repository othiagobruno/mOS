/* mOS — teste do libdispatch em C (userland/libsystem/dispatch.c, extra.c) como um app Mach-O o usa: filas, grupos,
 * semáforos, dispatch_after, fonte de timer, retain/release (também pelo lado ObjC: objc_retain/objc_release, -retain/
 * -release, autorelease pool), valores por fila e o isa ObjC dos objetos (OS_dispatch_*).
 * Roda no boot de teste (build.ps1 -Tests: o "teste" nativo o chama, como o posixteste). Sai com o número de falhas.
 *   zig cc -target x86_64-macos -O1 userland/tests-dispatch/main.c -o dispatchteste -Wl,-undefined,dynamic_lookup */
#include <dispatch/dispatch.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const char *object_getClassName(void *o);
void *objc_getClass(const char *name);
void *objc_retain(void *o);
void objc_release(void *o);
void *objc_autorelease(void *o);
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *p);
void *sel_registerName(const char *n);
void *objc_msgSend(void *, void *, ...);

static int fails, total;
#define CHECK(c, ...) do { total++; if (c) printf("  ok   "); else { fails++; printf("  FALHA "); } printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)

static int fcount;
static void inc_f(void *ctx) { (void)ctx; fcount++; }
static char chave;

int main(void) {
    printf("dispatchteste: libdispatch em C\n");

    dispatch_queue_t q = dispatch_queue_create("mos.teste", DISPATCH_QUEUE_SERIAL);
    CHECK(q != NULL && strcmp(dispatch_queue_get_label(q), "mos.teste") == 0, "dispatch_queue_create + label");
    /* um app com ObjC (Chromium) tem o runtime de pé; aqui ele sobe agora, com a fila já criada */
    CHECK(objc_getClass("OS_dispatch_queue_serial") != NULL, "runtime ObjC com as classes OS_dispatch_*");
    CHECK(strcmp(object_getClassName(q), "OS_dispatch_queue_serial") == 0, "isa da fila serial (%s)", object_getClassName(q));
    CHECK(strcmp(object_getClassName(dispatch_get_main_queue()), "OS_dispatch_queue_main") == 0, "isa da fila principal");

    __block int n = 0;
    dispatch_sync(q, ^{ n = 1; });
    dispatch_async(q, ^{ n++; });
    dispatch_sync(q, ^{ n++; });
    CHECK(n == 3, "async/sync na fila serial (%d)", n);
    dispatch_async_f(q, 0, inc_f);
    dispatch_sync_f(q, 0, inc_f);
    CHECK(fcount == 2, "dispatch_async_f/sync_f");

    dispatch_queue_t c = dispatch_queue_create("mos.conc", DISPATCH_QUEUE_CONCURRENT);
    CHECK(strcmp(object_getClassName(c), "OS_dispatch_queue_concurrent") == 0, "isa da fila concorrente");

    dispatch_group_t g = dispatch_group_create();
    __block int gn = 0;
    for (int i = 0; i < 4; i++) dispatch_group_async(g, dispatch_get_global_queue(0, 0), ^{ __sync_fetch_and_add(&gn, 1); });
    dispatch_group_enter(g);
    dispatch_async(c, ^{ __sync_fetch_and_add(&gn, 1); dispatch_group_leave(g); });
    long gw = dispatch_group_wait(g, dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC));
    CHECK(gw == 0 && gn == 5, "dispatch_group async/enter/leave/wait (%d)", gn);
    CHECK(strcmp(object_getClassName(g), "OS_dispatch_group") == 0, "isa do grupo");

    dispatch_semaphore_t s = dispatch_semaphore_create(0);
    CHECK(strcmp(object_getClassName(s), "OS_dispatch_semaphore") == 0, "isa do semáforo");
    __block int depois = 0;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 50 * NSEC_PER_MSEC), dispatch_get_global_queue(0, 0), ^{
        depois = 1; dispatch_semaphore_signal(s);
    });
    long sw = dispatch_semaphore_wait(s, dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC));
    CHECK(sw == 0 && depois, "dispatch_after + semáforo");
    CHECK(dispatch_semaphore_wait(s, dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_MSEC)) != 0, "semáforo vazio expira");

    dispatch_source_t t = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_global_queue(0, 0));
    CHECK(t != NULL && strcmp(object_getClassName(t), "OS_dispatch_source") == 0, "fonte de timer + isa");
    __block int ticks = 0;
    dispatch_semaphore_t st = dispatch_semaphore_create(0);
    dispatch_source_set_event_handler(t, ^{ if (++ticks == 2) dispatch_semaphore_signal(st); });
    dispatch_source_set_timer(t, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_MSEC), 20 * NSEC_PER_MSEC, 0);
    dispatch_resume(t);
    long tw = dispatch_semaphore_wait(st, dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC));
    dispatch_source_cancel(t);
    CHECK(tw == 0 && ticks >= 2, "timer repete (%d)", ticks);
    CHECK(dispatch_source_testcancel(t) != 0, "dispatch_source_cancel");

    dispatch_queue_set_specific(q, &chave, (void *)0x1234, 0);
    __block void *dentro = 0;
    dispatch_sync(q, ^{ dentro = dispatch_get_specific(&chave); });
    CHECK(dispatch_queue_get_specific(q, &chave) == (void *)0x1234 && dentro == (void *)0x1234 && dispatch_get_specific(&chave) == 0,
          "set/get_specific");

    /* retain/release: o objeto segue usável enquanto houver referência */
    dispatch_retain(q);
    dispatch_release(q);
    __block int vivo = 0;
    dispatch_sync(q, ^{ vivo = 1; });
    CHECK(vivo, "retain/release mantém a fila");
    dispatch_release(dispatch_get_main_queue());          /* estática: não pode ser liberada */
    dispatch_release(dispatch_get_global_queue(0, 0));
    __block int ok2 = 0;
    dispatch_sync(dispatch_get_global_queue(0, 0), ^{ ok2 = 1; });
    CHECK(ok2, "release de fila global é inócuo");

    __block int once = 0;
    static dispatch_once_t pred;
    for (int i = 0; i < 3; i++) dispatch_once(&pred, ^{ once++; });
    CHECK(once == 1, "dispatch_once");

    /* retain/release pelo lado ObjC (ARC do Chromium com OS_OBJECT_USE_OBJC, Swift): objc_retain/objc_release, as
     * mensagens -retain/-release e um autorelease pool têm de usar a contagem do dispatch, nunca o dealloc do NSObject
     * (estes objetos não têm o cabeçalho dos objetos ObjC do mOS) */
    void *objs[4] = { dispatch_queue_create("mos.arc", 0), dispatch_group_create(), dispatch_semaphore_create(1),
                      dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_global_queue(0, 0)) };
    const char *nomes[4] = { "fila", "grupo", "semáforo", "fonte" };
    void *sel_retain = sel_registerName("retain"), *sel_release = sel_registerName("release");
    void *sel_rc = sel_registerName("retainCount"), *sel_ar = sel_registerName("autorelease");
    for (int i = 0; i < 4; i++) {
        void *o = objs[i];
        for (int k = 0; k < 100; k++) { objc_retain(o); objc_release(o); }
        ((void *(*)(void *, void *))objc_msgSend)(o, sel_retain);
        ((void (*)(void *, void *))objc_msgSend)(o, sel_release);
        void *pool = objc_autoreleasePoolPush();
        objc_retain(o);
        ((void *(*)(void *, void *))objc_msgSend)(o, sel_ar);
        objc_autorelease(objc_retain(o));
        objc_autoreleasePoolPop(pool);
        unsigned long rc = ((unsigned long (*)(void *, void *))objc_msgSend)(o, sel_rc);
        objc_retain(o);
        unsigned long rc2 = ((unsigned long (*)(void *, void *))objc_msgSend)(o, sel_rc);
        objc_release(o);
        CHECK(rc == 1 && rc2 == 2 && strncmp(object_getClassName(o), "OS_dispatch_", 12) == 0,
              "%s: objc_retain/release, -retain/-release e autorelease pool (rc %lu, %lu)", nomes[i], rc, rc2);
    }
    __block int usou = 0;
    dispatch_sync(objs[0], ^{ usou = 1; });
    dispatch_group_enter(objs[1]); dispatch_group_leave(objs[1]);
    long sw2 = dispatch_semaphore_wait(objs[2], dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC));
    dispatch_source_cancel(objs[3]);
    CHECK(usou && sw2 == 0 && dispatch_group_wait(objs[1], DISPATCH_TIME_NOW) == 0, "objetos seguem vivos depois do pool");
    /* soltar a referência de criação pelo objc_release libera pelo dispatch.c (sem cair no free do NSObject) */
    for (int i = 0; i < 4; i++) objc_release(objs[i]);
    void *depois2 = dispatch_queue_create("mos.depois", 0);
    __block int ok3 = 0;
    dispatch_sync(depois2, ^{ ok3 = 1; });
    CHECK(ok3, "objc_release final libera e o dispatch continua funcionando");

    printf("dispatchteste: %d/%d ok\n", total - fails, total);
    return fails;
}
