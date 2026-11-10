#define _POSIX_C_SOURCE 200809L
/* gcc -std=c11 -Wall -Wextra -O1 tools/sktest.c -o /tmp/sktest && /tmp/sktest
 * Teste de host da fila do StoreKit. Não chama a App Store e não valida recibo. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define SK_HOST 1
#include "../userland/libsystem/storekit_stub.inc"
#include "../userland/libsystem/storekit_core.inc"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int g_tid, g_binds;

static void on_bind(int tx) {
    (void)tx;
    g_binds++;
}

static int host_tid(void) { return g_tid; }

typedef struct {
    int n;
    int state[8];
    int err[8];
    int tx[8];
    int restore_n;
    int restore_err;
    int saw_purchased;
    int saw_restored;
} obs_log;

static void on_tx(void *ctx, int tx, int state, int err) {
    obs_log *l = ctx;
    if (l->n < 8) {
        l->tx[l->n] = tx;
        l->state[l->n] = state;
        l->err[l->n] = err;
        l->n++;
    }
    if (state == SK_TX_PURCHASED) l->saw_purchased = 1;
    if (state == SK_TX_RESTORED) l->saw_restored = 1;
}

static void on_restore(void *ctx, int err) {
    obs_log *l = ctx;
    l->restore_n++;
    l->restore_err = err;
}

typedef struct {
    int n, err, products;
} req_log;

static void on_fail(void *ctx, int err) {
    req_log *r = ctx;
    r->n++;
    r->err = err;
}

typedef struct {
    int n, kind;
} cont_log;

static void on_cont(void *ctx, int kind) {
    cont_log *c = ctx;
    c->n++;
    c->kind = kind;
}

static int fn_frame;

static void on_frame(sk2_frame *f) {
    fn_frame++;
    (void)f;
}

void sk_host_throw(sk2_frame *f) { sk_core_throw_frame(f); }

#if defined(__x86_64__)
__asm__(
".text\n"
".globl sk2_r14_entry\n"
"sk2_r14_entry:\n"
"  movq %r14, %rdi\n"
"  jmp sk_host_throw\n"
);
void sk2_r14_entry(void);
static void via_r14(sk2_frame *f) {
    __asm__ volatile ("movq %0, %%r14\n\tcall sk2_r14_entry" :: "r"(f) : "r14", "rdi", "rax", "memory", "cc");
}
#endif

static void setup(void) {
    sk_core_reset();
    g_tid = 1;
    sk_core_set_tid(host_tid);
    sk_core_bind_queue(1);
    sk_core_set_bind(on_bind);
}

int main(void) {
    char err[200];
    char pid[64];
    int st = -1, ec = -1, id, id2;
    obs_log a, b;
    req_log req;
    cont_log cont;

    CHECK(sk_policy_can_pay() == 1);
    CHECK(sk_policy_product_count() == 0);
    CHECK(sk_policy_purchased() == 0);
    CHECK(sk_policy_fail_state() == 2);
    CHECK(sk_policy_network_code() == 7);
    CHECK(sk_policy_storekit2_network() == 2);
    CHECK(sk_policy_receipt_len() == 0);
    CHECK(sk_policy_fail_state() != 1);
    CHECK(sk_policy_fail_state() != 3);
    err[0] = 0;
    CHECK(sk_verify("recibo", 6, err, sizeof err) == -1);
    CHECK(strcmp(err, "recibo não verificado: o mOS não valida recibo da App Store nem fala com a Apple") == 0);

    setup();
    CHECK(sk_core_can_pay() == 1);
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    CHECK(sk_core_add_observer(&a, on_tx, on_restore) == 2);
    CHECK(sk_core_add_observer(&a, on_tx, on_restore) == 1);
    g_binds = 0;
    id = sk_core_add_payment("com.mos.premium", 1);
    CHECK(id > 0);
    CHECK(g_binds == 1);
    CHECK(a.n == 2);
    CHECK(a.state[0] == SK_TX_PURCHASING);
    CHECK(a.err[0] == 0);
    CHECK(a.state[1] == SK_TX_FAILED);
    CHECK(a.err[1] == SK_ERR_CLOUD_NETWORK);
    CHECK(a.tx[0] == id && a.tx[1] == id);
    CHECK(!a.saw_purchased && !a.saw_restored);
    CHECK(sk_core_tx_get(id, &st, &ec, pid, (int)sizeof pid) == 1);
    CHECK(st == SK_TX_FAILED);
    CHECK(ec == SK_ERR_CLOUD_NETWORK);
    CHECK(strcmp(pid, "com.mos.premium") == 0);
    CHECK(sk_core_tx_count() == 1);
    CHECK(sk_core_pending() == 0);

    /* observer que entra depois vê o estado atual (falha), não um purchasing novo */
    CHECK(sk_core_add_observer(&b, on_tx, on_restore) == 2);
    CHECK(b.n == 1);
    CHECK(b.state[0] == SK_TX_FAILED);
    CHECK(b.err[0] == SK_ERR_CLOUD_NETWORK);

    sk_core_finish(id);
    CHECK(sk_core_tx_count() == 0);
    CHECK(sk_core_tx_get(id, &st, &ec, 0, 0) == 0);
    CHECK(a.n == 2); /* finish não avisa de novo */

    /* fora da thread da fila: nada chega até o drain na thread certa */
    memset(&a, 0, sizeof a);
    setup();
    g_tid = 2;
    CHECK(sk_core_add_observer(&a, on_tx, on_restore) == 2);
    id2 = sk_core_add_payment("com.mos.outro", 2);
    CHECK(id2 > 0);
    CHECK(a.n == 0);
    CHECK(sk_core_pending() == 2);
    g_tid = 1;
    sk_core_drain();
    CHECK(sk_core_pending() == 0);
    CHECK(a.n == 2);
    CHECK(a.state[0] == SK_TX_PURCHASING && a.err[0] == 0);
    CHECK(a.state[1] == SK_TX_FAILED && a.err[1] == SK_ERR_CLOUD_NETWORK);
    CHECK(!a.saw_purchased && !a.saw_restored);

    sk_core_remove_observer(&a);
    memset(&a, 0, sizeof a);
    sk_core_add_payment("com.mos.terceiro", 1);
    CHECK(a.n == 0);

    memset(&a, 0, sizeof a);
    sk_core_add_observer(&a, on_tx, on_restore);
    sk_core_restore();
    CHECK(a.restore_n == 1);
    CHECK(a.restore_err == SK_ERR_CLOUD_NETWORK);
    CHECK(a.saw_restored == 0);
    CHECK(a.n == 2); /* as duas falhas que ainda estão na fila, sem estado restaurado */

    /* pedido de produto: só didFail, com o código de rede */
    memset(&req, 0, sizeof req);
    sk_core_request(&req, on_fail, 0);
    CHECK(req.n == 1);
    CHECK(req.err == SK_ERR_CLOUD_NETWORK);
    CHECK(req.products == 0);
    req.n = 0;
    sk_core_request(&req, on_fail, 1);
    CHECK(req.n == 0);
    g_tid = 2;
    sk_core_request(&req, on_fail, 0);
    CHECK(req.n == 0);
    CHECK(sk_core_pending() == 1);
    g_tid = 1;
    sk_core_drain();
    CHECK(req.n == 1);
    CHECK(req.err == SK_ERR_CLOUD_NETWORK);

    /* StoreKit 2: a continuação recebe networkError e nenhum produto ou transação */
    memset(&cont, 0, sizeof cont);
    sk_core_products(3, on_cont, &cont);
    CHECK(cont.n == 1);
    CHECK(cont.kind == SK2_ERR_NETWORK);
    CHECK(sk_policy_product_count() == 0);
    cont.n = 0;
    sk_core_purchase("com.mos.premium", on_cont, &cont);
    CHECK(cont.n == 1 && cont.kind == SK2_ERR_NETWORK);
    cont.n = 0;
    sk_core_sync(on_cont, &cont);
    CHECK(cont.n == 1 && cont.kind == SK2_ERR_NETWORK);
    CHECK(sk_core_entitlements_next() == 0);
    CHECK(sk_core_updates_next() == 0);

    /* Continuação no formato do AsyncContext: parent em +0, resume em +8.
     * O frame Swift gerado pelo swift-frontend não existe neste host. */
    {
        sk2_frame parent, child;
        memset(&parent, 0, sizeof parent);
        memset(&child, 0, sizeof child);
        child.parent = &parent;
        child.resume = on_frame;
        fn_frame = 0;
        sk_core_throw_frame(&child);
        CHECK(fn_frame == 1);
        CHECK(parent.error_kind == SK2_ERR_NETWORK);
        CHECK(parent.url_error == SK2_URL_NOT_CONNECTED);
        CHECK(child.error_kind == 0);
        CHECK(sk_core_entitlements_next() == 0);
        memset(&child, 0, sizeof child);
        child.resume = on_frame;
        fn_frame = 0;
        sk_core_throw_frame(&child);
        CHECK(fn_frame == 1);
        CHECK(child.error_kind == SK2_ERR_NETWORK);
        CHECK(child.url_error == SK2_URL_NOT_CONNECTED);
#if defined(__x86_64__)
        memset(&parent, 0, sizeof parent);
        memset(&child, 0, sizeof child);
        child.parent = &parent;
        child.resume = on_frame;
        fn_frame = 0;
        via_r14(&child);
        CHECK(fn_frame == 1);
        CHECK(parent.error_kind == SK2_ERR_NETWORK);
        CHECK(parent.url_error == SK2_URL_NOT_CONNECTED);
#endif
    }

    printf("%d checks, %d fails\n", checks, fails);
    return fails ? 1 : 0;
}
