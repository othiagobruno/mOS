/* Host (Linux): +[NSException raise:format:] capturável, e _objc_msgForward
 * entregando 3 argumentos ao forwardInvocation:. Não sobe a VM.
 *   cc -O2 -std=c11 tools/nsexc_forward_hosttest.c userland/libsystem/nsexc_raise.c \
 *      userland/libsystem/objc_forward.c userland/libsystem/objc_forward.S -o /tmp/nsexc_fwd
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>

struct cx_class { struct cx_class *isa, *super; void *cache, *vtable; unsigned long long data; };
typedef struct cx_class cx_class_t;
typedef struct objc_object { cx_class_t *isa; } *id;
typedef const char *SEL;
typedef id (*IMP)(id, SEL, ...);

static int fails;
static jmp_buf jb;
static const char *got_name, *got_reason;

#define CHECK(m, c) do { \
    if (!(c)) { fprintf(stderr, "FALHA: %s\n", m); fails++; } \
    else printf("ok: %s\n", m); \
} while (0)

id cx_str_vfmt(id fmt, va_list ap) {
    char buf[256];
    vsnprintf(buf, sizeof buf, (const char *)fmt, ap);
    return (id)strdup(buf);
}
id cx_exception_with(id cls, id name, id reason, id info) {
    (void)cls; (void)info;
    static struct { const char *n, *r; } e;
    e.n = (const char *)name;
    e.r = (const char *)reason;
    return (id)&e;
}
void objc_exception_throw(id e) {
    struct { const char *n, *r; } *x = (void *)e;
    got_name = x->n;
    got_reason = x->r;
    longjmp(jb, 1);
}
void cx_nsexception_raise(id cls, id name, id fmt, ...);

SEL sel_registerName(const char *n) { return n; }

IMP cx_lookup(cx_class_t *c, SEL s) {
    (void)c;
    if (!s) return 0;
    if (!strcmp(s, "methodSignatureForSelector:") || !strcmp(s, "forwardInvocation:")) return (IMP)1;
    return 0;
}
cx_class_t *cx_class_named(const char *n) {
    static cx_class_t fake;
    (void)n;
    return &fake;
}
void cx_x64_call(void *fn, void *frame) {
    (void)fn; (void)frame;
    fprintf(stderr, "FALHA: cx_x64_call não devia rodar neste teste\n");
    fails++;
}

static unsigned long long args[8];
static int nargs_seen;
static unsigned long long ret_word = 0xA1B2C3D4E5F60718ULL;

unsigned long long objc_msgSend(unsigned long long self, unsigned long long sel, unsigned long long a,
                                unsigned long long b, unsigned long long c, unsigned long long d) {
    const char *s = (const char *)sel;
    (void)d;
    if (!strcmp(s, "methodSignatureForSelector:")) return 1;
    if (!strcmp(s, "invocationWithMethodSignature:")) return 2;
    if (!strcmp(s, "numberOfArguments")) return 5;
    if (!strcmp(s, "getArgumentTypeAtIndex:")) return (unsigned long long)"@";
    if (!strcmp(s, "setArgument:atIndex:")) {
        unsigned long long slot = 0;
        memcpy(&slot, (void *)a, 8);
        if (b < 8) args[b] = slot;
        if ((int)b + 1 > nargs_seen) nargs_seen = (int)b + 1;
        return 0;
    }
    if (!strcmp(s, "forwardInvocation:")) {
        (void)self; (void)c;
        return 0;
    }
    if (!strcmp(s, "methodReturnLength")) return 8;
    if (!strcmp(s, "getReturnValue:")) {
        memcpy((void *)a, &ret_word, 8);
        return 0;
    }
    fprintf(stderr, "FALHA: objc_msgSend inesperado: %s\n", s ? s : "(nil)");
    fails++;
    return 0;
}

unsigned long long _objc_msgForward(void *self, const char *sel, unsigned long long a, unsigned long long b, unsigned long long c);

int main(void) {
    if (setjmp(jb) == 0) {
        cx_nsexception_raise(0, (id)"FmtErro", (id)"n=%d", 7);
        CHECK("+[NSException raise:format:] lançou", 0);
    } else {
        CHECK("+[NSException raise:format:] capturável", got_name && !strcmp(got_name, "FmtErro") && got_reason && !strcmp(got_reason, "n=7"));
    }

    static struct { cx_class_t *isa; } obj;
    static cx_class_t cls;
    obj.isa = &cls;
    unsigned long long r = _objc_msgForward(&obj, "take:b:c:", 11, 22, 33);
    CHECK("forwardInvocation: recebeu self, _cmd e 3 argumentos",
          nargs_seen >= 5 && args[0] == (unsigned long long)&obj && !strcmp((char *)args[1], "take:b:c:") &&
          args[2] == 11 && args[3] == 22 && args[4] == 33);
    CHECK("_objc_msgForward devolveu o valor do invocation", r == ret_word);
    return fails ? 1 : 0;
}
