#include <stdio.h>
#include <string.h>
typedef struct cx_pub cx_pub;
typedef struct cx_can cx_can;
typedef struct cx_set cx_set;
typedef struct { long v; cx_pub *p; } pubd_t;
cx_set *cx_set_new(void);
void cx_set_cancel(cx_set *s);
void cx_send(cx_pub *p, long v);
void cx_throttle_open(cx_pub *p);
int mos_combine_meta_check(void);

void mos_any_cancel(cx_can *c);
int mos_any_store(cx_can *c, cx_set *set);
cx_can *mos_any_init(void (*fn)(void));
void mos_pass_send(cx_pub *p, long v);
cx_pub *mos_pass_init(void);
pubd_t *mos_pub_init(long v);
cx_pub *mos_pub_proj(pubd_t *o);
void mos_pub_proj_set(pubd_t *o, cx_pub *p);
long mos_pub_get(long *slot);
void mos_pub_set(long *slot, long v);
void mos_pub_mod(long *slot, long v);
cx_pub *mos_erase(cx_pub *p);
cx_pub *mos_cmap(cx_pub *p, int (*fn)(long, long *, void *), void *ctx);
cx_pub *mos_first(cx_pub *p);
cx_pub *mos_filter(cx_pub *p, int (*pred)(long, void *), void *ctx);
cx_pub *mos_receive(cx_pub *p, void (*hop)(void (*)(void *), void *));
cx_pub *mos_throttle(cx_pub *p);
cx_pub *mos_drop(cx_pub *p, long n);
cx_pub *mos_dedup(cx_pub *p);
cx_can *mos_sink(cx_pub *p, void (*fn)(long, void *), void *ctx);
cx_pub *mos_owc(cx_pub **slot);

static int falhas, nb, cancelou;
static long buf[8];
static void guarda(long v, void *c) { (void)c; if (nb < 8) buf[nb++] = v; }
static void marca(void) { cancelou = 1; }
static int par(long v, void *c) { (void)c; return (v & 1) == 0; }
static int mais(long v, long *o, void *c) { (void)c; if (v < 0) return 0; *o = v + 1; return 1; }
static void (*qfn)(void *);
static void *qarg;
static void hop(void (*fn)(void *), void *a) { qfn = fn; qarg = a; }
static void espera(const char *n, int ok) {
    if (ok) printf("ok %s\n", n);
    else { printf("falhou %s\n", n); falhas++; }
}

int main(void) {
    cx_pub *p = mos_pass_init();
    cx_can *c = mos_sink(p, guarda, 0);
    cx_set *set = cx_set_new();
    cx_pub *f, *d, *fr, *dd, *th, *rc, *cm, *ow;
    pubd_t *pb;
    long slot = 3;
    cx_pub *slotp = 0;
    mos_pass_send(p, 1);
    espera("sink", nb == 1 && buf[0] == 1);
    mos_any_store(c, set);
    cx_set_cancel(set);
    mos_pass_send(p, 9);
    espera("store cancel", nb == 1);
    c = mos_any_init(marca);
    mos_any_cancel(c);
    espera("init cancel", cancelou == 1);

    p = mos_pass_init();
    f = mos_filter(p, par, 0);
    nb = 0;
    mos_sink(f, guarda, 0);
    mos_pass_send(p, 1);
    mos_pass_send(p, 2);
    espera("filter", nb == 1 && buf[0] == 2);

    p = mos_pass_init();
    d = mos_drop(p, 1);
    nb = 0;
    mos_sink(d, guarda, 0);
    mos_pass_send(p, 4);
    mos_pass_send(p, 5);
    espera("drop", nb == 1 && buf[0] == 5);

    p = mos_pass_init();
    fr = mos_first(p);
    nb = 0;
    mos_sink(fr, guarda, 0);
    mos_pass_send(p, 7);
    mos_pass_send(p, 8);
    espera("first", nb == 1 && buf[0] == 7);

    p = mos_pass_init();
    dd = mos_dedup(p);
    nb = 0;
    mos_sink(dd, guarda, 0);
    mos_pass_send(p, 1);
    mos_pass_send(p, 1);
    mos_pass_send(p, 2);
    espera("dedup", nb == 2 && buf[1] == 2);

    p = mos_pass_init();
    th = mos_throttle(p);
    nb = 0;
    mos_sink(th, guarda, 0);
    mos_pass_send(p, 1);
    mos_pass_send(p, 2);
    cx_throttle_open(th);
    mos_pass_send(p, 3);
    espera("throttle", nb == 2 && buf[1] == 3);

    p = mos_pass_init();
    rc = mos_receive(p, hop);
    nb = 0;
    mos_sink(rc, guarda, 0);
    mos_pass_send(p, 11);
    espera("receive espera", nb == 0);
    qfn(qarg);
    espera("receive", nb == 1 && buf[0] == 11);

    p = mos_pass_init();
    cm = mos_cmap(p, mais, 0);
    nb = 0;
    mos_sink(mos_erase(cm), guarda, 0);
    mos_pass_send(p, -1);
    mos_pass_send(p, 4);
    espera("compactMap", nb == 1 && buf[0] == 5);

    pb = mos_pub_init(4);
    nb = 0;
    mos_sink(mos_pub_proj(pb), guarda, 0);
    espera("published", nb == 1 && buf[0] == 4 && pb->v == 4);
    mos_pub_set(&slot, 8);
    mos_pub_mod(&slot, 9);
    espera("wrapped", mos_pub_get(&slot) == 9);
    mos_pub_proj_set(pb, mos_pass_init());

    ow = mos_owc(&slotp);
    nb = 0;
    mos_sink(ow, guarda, 0);
    mos_pass_send(ow, 6);
    espera("objectWillChange", ow && nb == 1 && buf[0] == 6);
    espera("meta", mos_combine_meta_check() == 0);
    if (falhas) { printf("%d falhas\n", falhas); return 1; }
    printf("ok (0)\n");
    return 0;
}
