/* STUB LOCAL. Não é a App Store.
 * Não valida recibo, não fala com a Apple, não contorna FairPlay nem assinatura.
 * O retorno -1 é a falha de verificação, de propósito.
 */
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
