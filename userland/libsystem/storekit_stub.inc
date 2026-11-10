/* STUB LOCAL. Não é a App Store.
 * Não valida recibo, não fala com a Apple, não contorna FairPlay nem assinatura.
 * O retorno -1 é a falha de verificação, de propósito.
 * canMakePayments é não. Um pedido de produto devolve zero produtos.
 * Um pagamento termina falho (estado 2). Nunca há compra (estado 1) nem recibo.
 */
__attribute__((unused)) static int sk_policy_can_pay(void) { return 0; }
__attribute__((unused)) static int sk_policy_product_count(void) { return 0; }
__attribute__((unused)) static int sk_policy_purchased(void) { return 0; }
__attribute__((unused)) static int sk_policy_fail_state(void) { return 2; } /* SKPaymentTransactionStateFailed */
__attribute__((unused)) static int sk_policy_receipt_len(void) { return 0; }
static int sk_verify(const void *bytes, size_t n, char *err, size_t cap) {
    (void)bytes;
    (void)n;
    const char *m = "recibo não verificado: o mOS não valida recibo da App Store nem fala com a Apple";
    if (err && cap) {
        size_t i = 0;
        for (; m[i] && i + 1 < cap; i++) err[i] = m[i];
        err[i] = 0;
    }
    return -1;
}
