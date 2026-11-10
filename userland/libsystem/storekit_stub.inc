/* Recibo. Não valida, não fala com a Apple, não contorna FairPlay.
 * O retorno -1 é a falha de verificação, de propósito.
 * canMakePayments é sim: não há controle parental aqui. A compra em si
 * falha com SKErrorCloudServiceNetworkConnectionFailed (código 7).
 * Não há produto, compra (estado 1) nem recibo.
 */
__attribute__((unused)) static int sk_policy_can_pay(void) { return 1; }
__attribute__((unused)) static int sk_policy_product_count(void) { return 0; }
__attribute__((unused)) static int sk_policy_purchased(void) { return 0; }
__attribute__((unused)) static int sk_policy_fail_state(void) { return 2; } /* SKPaymentTransactionStateFailed */
__attribute__((unused)) static int sk_policy_network_code(void) { return 7; } /* SKErrorCloudServiceNetworkConnectionFailed */
__attribute__((unused)) static int sk_policy_storekit2_network(void) { return 2; } /* StoreKitError.networkError */
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
