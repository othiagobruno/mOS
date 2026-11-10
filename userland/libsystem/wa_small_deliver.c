/*
 * O delegate do CallKit entra na fila pedida por dispatch_async_f.
 * Sem fila, a fila é a serial com.mos.callkit.provider.
 */
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_queue_create(const char *label, void *attr);

static void *wa_priv_q;

void wa_cx_deliver(void *queue, void *ctx, void (*fn)(void *)) {
    void *q = queue;
    if (!q) {
        if (!wa_priv_q) wa_priv_q = dispatch_queue_create("com.mos.callkit.provider", 0);
        q = wa_priv_q;
    }
    dispatch_async_f(q, ctx, fn);
}
