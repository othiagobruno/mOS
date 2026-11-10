/* Teste de host do runtime Combine. Compila combine_rt.c com -DCX_HOST. */
#include <stdio.h>
#include <stdlib.h>
typedef struct cx_sub cx_sub;
typedef struct cx_pub cx_pub;
typedef struct cx_can cx_can;
typedef struct cx_set cx_set;
#define CX_UNLIMITED (-1L)
cx_pub *cx_passthrough(void);
cx_pub *cx_current(long v);
long cx_value(cx_pub *p);
void cx_send(cx_pub *p, long v);
void cx_complete(cx_pub *p);
void cx_fail(cx_pub *p, int err);
void cx_request(cx_sub *s, long d);
void cx_cancel(cx_can *c);
cx_set *cx_set_new(void);
int cx_store(cx_can *c, cx_set *set);
void cx_set_cancel(cx_set *set);
cx_can *cx_sink(cx_pub *p, void (*fn)(long, void *), void *ctx);
cx_can *cx_assign(cx_pub *p, long *slot);
cx_sub *cx_subscribe(cx_pub *p, void (*got_sub)(void *, cx_sub *), void *ctx,
                     long (*got)(void *, long), void (*done)(void *, int));
cx_pub *cx_map(cx_pub *up, long (*fn)(long, void *), void *ctx);
cx_pub *cx_filter(cx_pub *up, int (*pred)(long, void *), void *ctx);
cx_pub *cx_remove_duplicates(cx_pub *up);
cx_pub *cx_debounce(cx_pub *up);
void cx_debounce_fire(cx_pub *p);
cx_pub *cx_receive(cx_pub *up, void (*hop)(void (*)(void *), void *));
cx_pub *cx_combine_latest(cx_pub *a, cx_pub *b);
void cx_latest_pair(cx_pub *p, long *a, long *b);
cx_pub *cx_drop(cx_pub *up, long n);
cx_pub *cx_first(cx_pub *up);
cx_pub *cx_compact_map(cx_pub *up, int (*fn)(long, long *, void *), void *ctx);
cx_pub *cx_merge(cx_pub *a, cx_pub *b);
cx_pub *cx_zip(cx_pub *a, cx_pub *b);
cx_pub *cx_scan(cx_pub *up, long (*fn)(long, long, void *), void *ctx);
cx_pub *cx_throttle(cx_pub *up);
void cx_throttle_open(cx_pub *p);
cx_pub *cx_just(long v);
cx_pub *cx_empty(void);
cx_pub *cx_fail_pub(int err);
cx_pub *cx_flat_map(cx_pub *up, cx_pub *(*fn)(long, void *), void *ctx);
cx_pub *cx_handle(cx_pub *up, void (*hv)(long, void *), void (*hd)(int, void *), void *ctx);
cx_pub *cx_catch(cx_pub *up, cx_pub *repl);
cx_pub *cx_retry(cx_pub *up, long n);
cx_pub *cx_delay(cx_pub *up);
void cx_delay_flush(cx_pub *p);
cx_pub *cx_buffer(cx_pub *up, long n);

static int falhas;
static void espera(const char *n, int cond) {
    if (cond) printf("ok %s\n", n);
    else { printf("falhou %s\n", n); falhas++; }
}

static long buf[8];
static int nb;
static void guarda(long v, void *ctx) { (void)ctx; if (nb < 8) buf[nb++] = v; }

static long vezes2(long v, void *c) { (void)c; return v * 2; }
static int par(long v, void *c) { (void)c; return (v % 2) == 0; }
static long soma(long a, long v, void *c) { (void)c; return a + v; }
static int cmap(long v, long *o, void *c) { (void)c; if (v < 0) return 0; *o = v + 1; return 1; }
static cx_pub *flat_um(long v, void *c) { (void)c; return cx_just(v + 10); }

static void (*qfn)(void *);
static void *qarg;
static void hop_guarda(void (*fn)(void *), void *arg) { qfn = fn; qarg = arg; }

static int eventos, fins, ultimo_err, tentativas;
static void hv(long v, void *c) { (void)v; (void)c; eventos++; }
static void hd(int e, void *c) { (void)e; (void)c; fins++; }
static long conta_fail(void *c, long v) { (void)c; (void)v; tentativas++; return 0; }
static void done_fail(void *c, int e) { (void)c; tentativas++; ultimo_err = e; }

static void pede1(void *c, cx_sub *s) { (void)c; cx_request(s, 1); }
static long guarda_dem(void *c, long v) { (void)c; guarda(v, 0); return 0; }

int main(void) {
    cx_pub *p = cx_passthrough();
    cx_can *c = cx_sink(p, guarda, 0);
    cx_set *set = cx_set_new();
    long slot = 0;
    cx_pub *cur, *m, *f, *d, *deb, *rec, *a, *b, *cl, *dr, *fr, *cm, *mg, *z, *sc, *th, *j, *em, *fl, *h, *ca, *re, *dl, *bu;
    long x = 0, y = 0;
    cx_sub *sub;

    cx_send(p, 1);
    cx_send(p, 2);
    espera("sink", nb == 2 && buf[0] == 1 && buf[1] == 2);
    cx_store(c, set);
    cx_set_cancel(set);
    cx_send(p, 3);
    espera("cancel", nb == 2);
    p = cx_passthrough();

    slot = -1;
    c = cx_assign(p, &slot);
    cx_send(p, 9);
    espera("assign", slot == 9);
    cx_cancel(c);
    cx_send(p, 4);
    espera("assign cancel", slot == 9);

    cur = cx_current(7);
    nb = 0;
    c = cx_sink(cur, guarda, 0);
    espera("current inicial", nb == 1 && buf[0] == 7);
    cx_send(cur, 8);
    espera("current send", cx_value(cur) == 8 && buf[1] == 8);

    nb = 0;
    sub = cx_subscribe(p, pede1, 0, guarda_dem, 0);
    cx_send(p, 5);
    cx_send(p, 6);
    espera("demand 1", nb == 1 && buf[0] == 5);
    (void)sub;

    p = cx_passthrough();
    m = cx_map(p, vezes2, 0);
    nb = 0;
    c = cx_sink(m, guarda, 0);
    cx_send(p, 3);
    espera("map", nb == 1 && buf[0] == 6);

    p = cx_passthrough();
    f = cx_filter(p, par, 0);
    nb = 0;
    cx_sink(f, guarda, 0);
    cx_send(p, 1);
    cx_send(p, 2);
    espera("filter", nb == 1 && buf[0] == 2);

    p = cx_passthrough();
    d = cx_remove_duplicates(p);
    nb = 0;
    cx_sink(d, guarda, 0);
    cx_send(p, 1);
    cx_send(p, 1);
    cx_send(p, 2);
    espera("dedup", nb == 2 && buf[0] == 1 && buf[1] == 2);

    p = cx_passthrough();
    deb = cx_debounce(p);
    nb = 0;
    cx_sink(deb, guarda, 0);
    cx_send(p, 4);
    cx_send(p, 5);
    espera("debounce segura", nb == 0);
    cx_debounce_fire(deb);
    espera("debounce solta", nb == 1 && buf[0] == 5);

    p = cx_passthrough();
    rec = cx_receive(p, hop_guarda);
    nb = 0;
    cx_sink(rec, guarda, 0);
    cx_send(p, 11);
    espera("receive espera", nb == 0 && qfn);
    qfn(qarg);
    espera("receive entrega", nb == 1 && buf[0] == 11);

    a = cx_passthrough();
    b = cx_passthrough();
    cl = cx_combine_latest(a, b);
    nb = 0;
    cx_sink(cl, guarda, 0);
    cx_send(a, 2);
    espera("combine espera", nb == 0);
    cx_send(b, 3);
    cx_latest_pair(cl, &x, &y);
    espera("combine", nb == 1 && x == 2 && y == 3);

    p = cx_passthrough();
    dr = cx_drop(p, 1);
    nb = 0;
    cx_sink(dr, guarda, 0);
    cx_send(p, 8);
    cx_send(p, 9);
    espera("drop", nb == 1 && buf[0] == 9);

    p = cx_passthrough();
    fr = cx_first(p);
    nb = 0;
    cx_sink(fr, guarda, 0);
    cx_send(p, 1);
    cx_send(p, 2);
    espera("first", nb == 1 && buf[0] == 1);

    p = cx_passthrough();
    cm = cx_compact_map(p, cmap, 0);
    nb = 0;
    cx_sink(cm, guarda, 0);
    cx_send(p, -3);
    cx_send(p, 4);
    espera("compactMap", nb == 1 && buf[0] == 5);

    a = cx_passthrough();
    b = cx_passthrough();
    mg = cx_merge(a, b);
    nb = 0;
    cx_sink(mg, guarda, 0);
    cx_send(a, 1);
    cx_send(b, 2);
    espera("merge", nb == 2 && buf[0] == 1 && buf[1] == 2);

    a = cx_passthrough();
    b = cx_passthrough();
    z = cx_zip(a, b);
    nb = 0;
    cx_sink(z, guarda, 0);
    cx_send(a, 4);
    cx_send(a, 5);
    cx_send(b, 6);
    cx_latest_pair(z, &x, &y);
    espera("zip", nb == 1 && x == 4 && y == 6);

    p = cx_passthrough();
    sc = cx_scan(p, soma, 0);
    nb = 0;
    cx_sink(sc, guarda, 0);
    cx_send(p, 2);
    cx_send(p, 3);
    espera("scan", nb == 2 && buf[0] == 2 && buf[1] == 5);

    p = cx_passthrough();
    th = cx_throttle(p);
    nb = 0;
    cx_sink(th, guarda, 0);
    cx_send(p, 1);
    cx_send(p, 2);
    espera("throttle fecha", nb == 1 && buf[0] == 1);
    cx_throttle_open(th);
    cx_send(p, 3);
    espera("throttle abre", buf[1] == 3);

    nb = 0;
    j = cx_just(42);
    cx_sink(j, guarda, 0);
    espera("just", nb == 1 && buf[0] == 42);
    nb = 0;
    em = cx_empty();
    cx_sink(em, guarda, 0);
    espera("empty", nb == 0);

    p = cx_passthrough();
    fl = cx_flat_map(p, flat_um, 0);
    nb = 0;
    cx_sink(fl, guarda, 0);
    cx_send(p, 5);
    espera("flatMap", nb == 1 && buf[0] == 15);

    p = cx_passthrough();
    h = cx_handle(p, hv, hd, 0);
    nb = 0;
    cx_sink(h, guarda, 0);
    cx_send(p, 1);
    cx_complete(p);
    espera("handleEvents", eventos == 1 && nb == 1);

    a = cx_fail_pub(3);
    b = cx_just(9);
    ca = cx_catch(a, b);
    nb = 0;
    cx_sink(ca, guarda, 0);
    espera("catch", nb == 1 && buf[0] == 9);

    re = cx_retry(cx_fail_pub(7), 2);
    tentativas = 0;
    ultimo_err = 0;
    cx_subscribe(re, 0, 0, conta_fail, done_fail);
    espera("retry", ultimo_err == 7 && tentativas >= 1);

    p = cx_passthrough();
    dl = cx_delay(p);
    nb = 0;
    cx_sink(dl, guarda, 0);
    cx_send(p, 6);
    espera("delay segura", nb == 0);
    cx_delay_flush(dl);
    espera("delay solta", nb == 1 && buf[0] == 6);

    p = cx_passthrough();
    bu = cx_buffer(p, 2);
    nb = 0;
    cx_sink(bu, guarda, 0);
    cx_send(p, 1);
    espera("buffer espera", nb == 0);
    cx_send(p, 2);
    espera("buffer", nb == 1 && buf[0] == 2);

    (void)fins;
    if (falhas) printf("%d falhas\n", falhas);
    else printf("ok (0)\n");
    return falhas ? 1 : 0;
}
