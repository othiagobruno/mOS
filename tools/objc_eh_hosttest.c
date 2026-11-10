/*
 * mOS — reteste no host do matcher de @catch (exc_can_catch) e do caminho sem libc++abi.
 *
 * Liga o objc_exc.c de produção (clang -c -O2 -ffreestanding, sem -Dstatic=). O símbolo
 * local exc_can_catch é globalizado só no objeto de teste (objcopy); o fonte não muda.
 * Não desenrola: @finally e o @catch que captura dependem do __gxx_personality_v0 da
 * libc++abi do disco e do libunwind, cobertos na VM por userland/tests-a64libsys/abi.mm.
 *
 *   clang -c -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -O2 \
 *     -I userland/libsystem userland/libsystem/objc_exc.c -o /tmp/objc_exc.o
 *   objcopy --globalize-symbol=exc_can_catch /tmp/objc_exc.o /tmp/objc_exc_host.o
 *   clang -std=c11 -O2 tools/objc_eh_hosttest.c /tmp/objc_exc_host.o -o /tmp/objc_eh_hosttest
 *   /tmp/objc_eh_hosttest
 *
 * Saída: "ok"/"FALHA" por item; código de saída = número de falhas.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

typedef struct cx_class cx_class_t;
typedef struct objc_object { cx_class_t *isa; } *id;
struct cx_class {
    cx_class_t *isa;
    cx_class_t *super;
    void *cache;
    void *vtable;
    unsigned long long data;
};
typedef struct { void **vtable; const char *name; cx_class_t *cls; } objc_typeinfo_t;
typedef struct { id obj; objc_typeinfo_t tinfo; } objc_exception_t;

extern void *objc_ehtype_vtable[];
extern objc_typeinfo_t OBJC_EHTYPE_id;
long exc_can_catch(const objc_typeinfo_t *catch_ti, const objc_typeinfo_t *throw_ti, void **adjusted);
int __objc_personality_v0(int version, int actions, unsigned long long cls, void *exc, void *ctx);
void *objc_begin_catch(void *exc);
void objc_end_catch(void);
__attribute__((noreturn)) void objc_exception_throw(id e);

/* símbolos que o .o de objc_exc.c referencia; o caminho testado não os chama, exceto dlopen */
int cx_fdprintf(int fd, const char *fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (n < 0) return n;
    int w = n < (int)sizeof b ? n : (int)sizeof b - 1;
    if (write(fd, b, (size_t)w) < 0) return -1;
    return n;
}
void *cx_objc_exception_preprocessor(void) { return 0; }
int cx_responds(cx_class_t *c, const char *sel) { (void)c; (void)sel; return 0; }
id objc_msgSend(id self, const char *op, ...) { (void)self; (void)op; return 0; }
const char *sel_registerName(const char *name) { return name; }
id objc_retain(id o) { return o; }
void objc_release(id o) { (void)o; }
const char *object_getClassName(id o) { (void)o; return "nil"; }
void *NSGetUncaughtExceptionHandler(void) { return 0; }
__attribute__((noreturn)) void __cxa_throw(void *o, void *t, void (*d)(void *)) {
    (void)o; (void)t; (void)d;
    fprintf(stderr, "hosttest: __cxa_throw do libSystem foi chamado (a dylib não deveria estar pronta)\n");
    abort();
}

static int falhas;
#define CHECK(nome, cond) do { \
    if (cond) printf("ok    %s\n", nome); \
    else { printf("FALHA %s (linha %d)\n", nome, __LINE__); falhas++; } \
} while (0)

static objc_typeinfo_t tipo(const char *nome, cx_class_t *cls) {
    objc_typeinfo_t t;
    t.vtable = objc_ehtype_vtable + 2;
    t.name = nome;
    t.cls = cls;
    return t;
}

int main(void) {
    cx_class_t raiz, sonda, erro, irma;
    memset(&raiz, 0, sizeof raiz);
    memset(&sonda, 0, sizeof sonda);
    memset(&erro, 0, sizeof erro);
    memset(&irma, 0, sizeof irma);
    sonda.super = &raiz;
    erro.super = &sonda;
    irma.super = &raiz;

    struct objc_object obj;
    obj.isa = &erro;
    objc_exception_t lancada;
    memset(&lancada, 0, sizeof lancada);
    lancada.obj = (id)&obj;
    lancada.tinfo = tipo("LibsysAbiErro", &erro);
    void *adj = &lancada;
    void *adj_antes = adj;

    objc_typeinfo_t c_erro = tipo("LibsysAbiErro", &erro);
    objc_typeinfo_t c_sonda = tipo("LibsysAbiSonda", &sonda);
    objc_typeinfo_t c_raiz = tipo("NSObject", &raiz);
    objc_typeinfo_t c_irma = tipo("NSException", &irma);

    CHECK("vtable: OBJC_EHTYPE_id aponta para objc_ehtype_vtable+2 e cls é nil",
          OBJC_EHTYPE_id.vtable == objc_ehtype_vtable + 2 && OBJC_EHTYPE_id.cls == 0 &&
          OBJC_EHTYPE_id.name && !strcmp(OBJC_EHTYPE_id.name, "id"));
    CHECK("slot Itanium do can_catch é objc_ehtype_vtable[6]",
          objc_ehtype_vtable[6] == (void *)exc_can_catch);

    CHECK("@catch da classe exata", exc_can_catch(&c_erro, &lancada.tinfo, &adj) == 1);
    CHECK("@catch da superclasse", exc_can_catch(&c_sonda, &lancada.tinfo, &adj) == 1);
    CHECK("@catch da raiz da hierarquia", exc_can_catch(&c_raiz, &lancada.tinfo, &adj) == 1);
    CHECK("@catch de classe irmã não casa", exc_can_catch(&c_irma, &lancada.tinfo, &adj) == 0);
    CHECK("@catch (id) casa", exc_can_catch(&OBJC_EHTYPE_id, &lancada.tinfo, &adj) == 1);
    CHECK("can_catch não reescreve o ponteiro ajustado (quem desembrulha é objc_begin_catch)",
          adj == adj_antes);

    lancada.obj = 0;
    CHECK("@catch (id) de objeto nil casa", exc_can_catch(&OBJC_EHTYPE_id, &lancada.tinfo, &adj) == 1);
    CHECK("@catch de classe com objeto nil não casa", exc_can_catch(&c_erro, &lancada.tinfo, &adj) == 0);
    lancada.obj = (id)&obj;

    objc_typeinfo_t cxx;
    void *vt_cxx[4] = {0, 0, 0, 0};
    cxx.vtable = vt_cxx + 2;
    cxx.name = "St13runtime_error";
    cxx.cls = 0;
    CHECK("exceção C++ não casa com @catch (id)", exc_can_catch(&OBJC_EHTYPE_id, &cxx, &adj) == 0);
    CHECK("typeinfo nulo não casa", exc_can_catch(&OBJC_EHTYPE_id, 0, &adj) == 0);

    int p1 = __objc_personality_v0(1, 1, 0, 0, 0);
    int p2 = __objc_personality_v0(1, 6, 0x474e5543432b2b00ull, (void *)1, (void *)2);
    CHECK("sem libc++.1.dylib a personalidade continua o desenrolar (_URC_CONTINUE_UNWIND = 8)",
          p1 == 8 && p2 == 8);

    void *marca = (void *)(unsigned long)0x1234;
    CHECK("objc_begin_catch sem libc++abi devolve o ponteiro cru", objc_begin_catch(marca) == marca);
    objc_end_catch();
    CHECK("objc_end_catch sem libc++abi não aborta", 1);

    int fds[2];
    if (pipe(fds) != 0) { perror("pipe"); return 1; }
    pid_t filho = fork();
    if (filho < 0) { perror("fork"); return 1; }
    if (filho == 0) {
        dup2(fds[1], 2);
        close(fds[0]);
        close(fds[1]);
        objc_exception_throw(0);
        _exit(99);
    }
    close(fds[1]);
    char buf[800];
    int n = 0, r;
    while (n < (int)sizeof buf - 1 && (r = (int)read(fds[0], buf + n, sizeof buf - 1 - n)) > 0) n += r;
    buf[n] = 0;
    close(fds[0]);
    int st = 0;
    waitpid(filho, &st, 0);
    CHECK("objc_exception_throw sem libc++abi aborta", WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT);
    CHECK("o aviso do throw sem destino nomeia a exceção",
          strstr(buf, "Terminating app due to uncaught exception") != 0);

    printf("objc_eh_hosttest: %d falhas\n", falhas);
    return falhas;
}
