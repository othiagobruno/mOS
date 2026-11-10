/* Núcleo do StoreKit. Não fala com a Apple, não cria produto, recibo nem
 * transação comprada/restaurada. Consulta e compra falham com o código de
 * rede da Apple. A fila, os observers e os dois estados (purchasing, failed)
 * são reais. Quem inclui este arquivo fica com o estado estático dele.
 */
#ifndef MOS_STOREKIT_CORE_INC
#define MOS_STOREKIT_CORE_INC

#ifdef SK_HOST
#include <string.h>
#else
#include <stddef.h>
#include <string.h>
#endif

#define SK_TX_PURCHASING 0
#define SK_TX_PURCHASED 1
#define SK_TX_FAILED 2
#define SK_TX_RESTORED 3
#define SK_TX_DEFERRED 4
/* SKErrorCode.cloudServiceNetworkConnectionFailed */
#define SK_ERR_CLOUD_NETWORK 7
/* StoreKitError.networkError — a loja não respondeu, então não é notAvailableInStorefront */
#define SK2_ERR_NETWORK 2
#define SK2_URL_NOT_CONNECTED (-1009)
#define SK_TX_MAX 32
#define SK_OBS_MAX 8
#define SK_NOTE_MAX 96

typedef void (*sk_tx_fn)(void *ctx, int tx, int state, int err);
typedef void (*sk_restore_fn)(void *ctx, int err);
typedef void (*sk_fail_fn)(void *ctx, int err);
typedef void (*sk2_cont)(void *ctx, int error_kind);
typedef int (*sk_tid_fn)(void);
typedef void (*sk_bind_fn)(int tx);

typedef struct {
    int id, state, err, qty, live;
    char pid[160];
} sk_tx;

typedef struct {
    void *ctx;
    sk_tx_fn on_tx;
    sk_restore_fn on_restore;
    int live;
} sk_ob;

typedef struct {
    void *ctx;
    int kind; /* 1 transação, 2 restauração, 3 pedido de produto */
    int tx, state, err;
    sk_tx_fn on_tx;
    sk_restore_fn on_restore;
    sk_fail_fn on_fail;
} sk_note;

static sk_tx sk_txs[SK_TX_MAX];
static sk_ob sk_obv[SK_OBS_MAX];
static sk_note sk_notes[SK_NOTE_MAX];
static int sk_ntx, sk_nnote, sk_next_id = 1, sk_queue_tid = 1;
static sk_tid_fn sk_tid;
static sk_bind_fn sk_bind;

static int sk_on_queue(void) {
    if (!sk_tid) return 1;
    return sk_tid() == sk_queue_tid;
}

static void sk_copy(char *d, int cap, const char *s) {
    int i = 0;
    if (!d || cap <= 0) return;
    if (!s) s = "";
    for (; s[i] && i + 1 < cap; i++) d[i] = s[i];
    d[i] = 0;
}

static void sk_core_reset(void) {
    memset(sk_txs, 0, sizeof sk_txs);
    memset(sk_obv, 0, sizeof sk_obv);
    memset(sk_notes, 0, sizeof sk_notes);
    sk_ntx = 0;
    sk_nnote = 0;
    sk_next_id = 1;
    sk_queue_tid = 1;
    sk_tid = 0;
    sk_bind = 0;
}

static void sk_core_set_tid(sk_tid_fn fn) { sk_tid = fn; }
static void sk_core_set_bind(sk_bind_fn fn) { sk_bind = fn; }
static void sk_core_bind_queue(int tid) { sk_queue_tid = tid; }
static int sk_core_pending(void) { return sk_nnote; }
static int sk_core_can_pay(void) { return 1; }

static sk_tx *sk_tx_find(int id) {
    int i;
    for (i = 0; i < sk_ntx; i++) if (sk_txs[i].live && sk_txs[i].id == id) return &sk_txs[i];
    return 0;
}

static int sk_push(sk_note n) {
    if (sk_nnote >= SK_NOTE_MAX) return 0;
    sk_notes[sk_nnote++] = n;
    return 1;
}

static void sk_emit_tx(sk_ob *o, int tx, int state, int err) {
    sk_note n;
    if (!o || !o->live || !o->on_tx) return;
    if (state == SK_TX_PURCHASED || state == SK_TX_RESTORED) return;
    if (sk_on_queue()) {
        o->on_tx(o->ctx, tx, state, err);
        return;
    }
    memset(&n, 0, sizeof n);
    n.ctx = o->ctx;
    n.kind = 1;
    n.tx = tx;
    n.state = state;
    n.err = err;
    n.on_tx = o->on_tx;
    sk_push(n);
}

static void sk_emit_restore(sk_ob *o, int err) {
    sk_note n;
    if (!o || !o->live || !o->on_restore) return;
    if (sk_on_queue()) {
        o->on_restore(o->ctx, err);
        return;
    }
    memset(&n, 0, sizeof n);
    n.ctx = o->ctx;
    n.kind = 2;
    n.err = err;
    n.on_restore = o->on_restore;
    sk_push(n);
}

static void sk_core_drain(void) {
    sk_note local[SK_NOTE_MAX];
    int n, i;
    if (!sk_on_queue()) return;
    n = sk_nnote;
    if (n > SK_NOTE_MAX) n = SK_NOTE_MAX;
    memcpy(local, sk_notes, (size_t)n * sizeof(sk_note));
    sk_nnote = 0;
    for (i = 0; i < n; i++) {
        if (local[i].kind == 1 && local[i].on_tx) local[i].on_tx(local[i].ctx, local[i].tx, local[i].state, local[i].err);
        else if (local[i].kind == 2 && local[i].on_restore) local[i].on_restore(local[i].ctx, local[i].err);
        else if (local[i].kind == 3 && local[i].on_fail) local[i].on_fail(local[i].ctx, local[i].err);
    }
}

static int sk_core_add_observer(void *ctx, sk_tx_fn on_tx, sk_restore_fn on_restore) {
    int i, slot = -1;
    if (!ctx) return 0;
    for (i = 0; i < SK_OBS_MAX; i++) {
        if (sk_obv[i].live && sk_obv[i].ctx == ctx) return 1;
        if (!sk_obv[i].live && slot < 0) slot = i;
    }
    if (slot < 0) return 0;
    sk_obv[slot].ctx = ctx;
    sk_obv[slot].on_tx = on_tx;
    sk_obv[slot].on_restore = on_restore;
    sk_obv[slot].live = 1;
    for (i = 0; i < sk_ntx; i++)
        if (sk_txs[i].live) sk_emit_tx(&sk_obv[slot], sk_txs[i].id, sk_txs[i].state, sk_txs[i].err);
    return 2;
}

static void sk_core_remove_observer(void *ctx) {
    int i;
    for (i = 0; i < SK_OBS_MAX; i++) if (sk_obv[i].live && sk_obv[i].ctx == ctx) sk_obv[i].live = 0;
}

static int sk_core_add_payment(const char *pid, int qty) {
    sk_tx *t = 0;
    int i;
    if (qty <= 0) qty = 1;
    for (i = 0; i < sk_ntx; i++) if (!sk_txs[i].live) { t = &sk_txs[i]; break; }
    if (!t) {
        if (sk_ntx >= SK_TX_MAX) return 0;
        t = &sk_txs[sk_ntx++];
    }
    memset(t, 0, sizeof *t);
    t->id = sk_next_id++;
    t->live = 1;
    t->qty = qty;
    t->state = SK_TX_PURCHASING;
    t->err = 0;
    sk_copy(t->pid, (int)sizeof t->pid, pid);
    if (sk_bind) sk_bind(t->id);
    for (i = 0; i < SK_OBS_MAX; i++) if (sk_obv[i].live) sk_emit_tx(&sk_obv[i], t->id, SK_TX_PURCHASING, 0);
    t->state = SK_TX_FAILED;
    t->err = SK_ERR_CLOUD_NETWORK;
    for (i = 0; i < SK_OBS_MAX; i++) if (sk_obv[i].live) sk_emit_tx(&sk_obv[i], t->id, SK_TX_FAILED, SK_ERR_CLOUD_NETWORK);
    return t->id;
}

static void sk_core_finish(int tx) {
    sk_tx *t = sk_tx_find(tx);
    if (!t) return;
    if (t->state != SK_TX_FAILED) return;
    t->live = 0;
}

static void sk_core_restore(void) {
    int i;
    for (i = 0; i < SK_OBS_MAX; i++) if (sk_obv[i].live) sk_emit_restore(&sk_obv[i], SK_ERR_CLOUD_NETWORK);
}

static int sk_core_tx_count(void) {
    int i, n = 0;
    for (i = 0; i < sk_ntx; i++) if (sk_txs[i].live) n++;
    return n;
}

static int sk_core_tx_get(int id, int *state, int *err, char *pid, int cap) {
    sk_tx *t = sk_tx_find(id);
    if (!t) return 0;
    if (state) *state = t->state;
    if (err) *err = t->err;
    if (pid) sk_copy(pid, cap, t->pid);
    return 1;
}

static void sk_core_request(void *ctx, sk_fail_fn fail, int cancelled) {
    sk_note n;
    if (cancelled || !fail) return;
    if (sk_on_queue()) {
        fail(ctx, SK_ERR_CLOUD_NETWORK);
        return;
    }
    memset(&n, 0, sizeof n);
    n.ctx = ctx;
    n.kind = 3;
    n.err = SK_ERR_CLOUD_NETWORK;
    n.on_fail = fail;
    sk_push(n);
}

static void sk2_fail(sk2_cont cont, void *ctx) {
    if (cont) cont(ctx, SK2_ERR_NETWORK);
}

static void sk_core_products(int n_ids, sk2_cont cont, void *ctx) {
    (void)n_ids;
    sk2_fail(cont, ctx);
}

static void sk_core_purchase(const char *pid, sk2_cont cont, void *ctx) {
    (void)pid;
    sk2_fail(cont, ctx);
}

static void sk_core_sync(sk2_cont cont, void *ctx) { sk2_fail(cont, ctx); }

/* Prefixo do AsyncContext do Swift: Parent em +0, ResumeParent em +8.
 * O erro não é um objeto Swift (não há witness table de StoreKitError aqui).
 * Antes de retomar, grava o discriminante 2 (networkError) e o URLError -1009
 * no frame que a continuação recebe. notAvailableInStorefront (4) não entra:
 * a loja não respondeu, então não há storefront. */
typedef struct sk2_frame {
    struct sk2_frame *parent;
    void (*resume)(struct sk2_frame *);
    int error_kind;
    int url_error;
} sk2_frame;

static void sk_core_throw_frame(sk2_frame *frame) {
    sk2_frame *dst;
    if (!frame || !frame->resume) return;
    dst = frame->parent ? frame->parent : frame;
    dst->error_kind = SK2_ERR_NETWORK;
    dst->url_error = SK2_URL_NOT_CONNECTED;
    frame->resume(dst);
}

/* currentEntitlements e updates: nenhuma transação. Não há o que emitir. */
static int sk_core_entitlements_next(void) { return 0; }
static int sk_core_updates_next(void) { return 0; }

#endif
