/* Prazo, política e o timeval que o NSURLSession aplica no transporte do pedido. */
#include <stdio.h>
#include "../userland/libsystem/urlrequest_fields.h"

static int fails;
static void ok(int c, const char *m) {
    if (!c) { fprintf(stderr, "falhou: %s\n", m); fails++; }
}

int main(void) {
    urlreq_opts_t o;
    long sec = 0, usec = 0;
    urlreq_init_opts(&o);
    ok(urlreq_timeout(&o) == 60, "prazo padrão");
    ok(urlreq_policy(&o) == 0, "política padrão");
    urlreq_set_timeout(&o, 12.5);
    ok(urlreq_timeout(&o) == 12.5, "setTimeoutInterval");
    urlreq_set_timeout(&o, 0);
    ok(urlreq_timeout(&o) == 60, "prazo zero volta ao padrão");
    urlreq_set_timeout(&o, 12.5);
    urlreq_timeval(urlreq_timeout(&o), &sec, &usec);
    ok(sec == 12 && usec == 500000, "timeval do transporte");
    urlreq_set_policy(&o, 2);
    ok(urlreq_policy(&o) == 2, "cachePolicy");
    if (fails) return 1;
    printf("nsurlrequest: ok\n");
    return 0;
}
