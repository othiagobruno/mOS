/* Host: _Unwind_Backtrace visita o caller e _Unwind_GetIPInfo marca retorno, não sinal. */
#include <stdio.h>
#include <stdlib.h>

extern unsigned long long _Unwind_GetIP(void *);
extern unsigned long long _Unwind_GetIPInfo(void *, int *);
extern unsigned long long _Unwind_GetCFA(void *);
extern int _Unwind_Backtrace(int (*)(void *, void *), void *);
struct uw_exc { unsigned long long cls; void (*cleanup)(int, void *); unsigned long long p1, p2; };
extern void _Unwind_DeleteException(struct uw_exc *);

static int saw_mid, deleted;

static int walk(void *ctx, void *arg) {
    int *n = arg;
    int before = 1;
    unsigned long long ip = _Unwind_GetIPInfo(ctx, &before);
    if (!ip || before != 0 || ip != _Unwind_GetIP(ctx)) return 2;
    if (!_Unwind_GetCFA(ctx)) return 2;
    (*n)++;
    return 0;
}

__attribute__((noinline)) static void mid(void) {
    int n = 0;
    int rc = _Unwind_Backtrace(walk, &n);
    saw_mid = n;
    if (rc != 5 || n < 2) abort();
}

static void on_delete(int reason, void *e) {
    (void)e;
    if (reason == 1) deleted = 1;
}

int main(void) {
    mid();
    if (saw_mid < 2) abort();
    struct uw_exc e;
    e.cls = 0;
    e.cleanup = on_delete;
    e.p1 = e.p2 = 0;
    _Unwind_DeleteException(&e);
    if (!deleted) abort();
    printf("backtrace frames=%d delete=1\n", saw_mid);
    return 0;
}
