/* Teste de host de userland/libsystem/fdn_consts.c:
 *   gcc -O2 -Wall -DFDN_CONSTS_HOST userland/libsystem/fdn_consts.c tools/fdn_consts_test.c -o /tmp/fdn_consts_test && /tmp/fdn_consts_test
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
typedef struct { void *isa; uint64_t flags; const char *s; uint64_t n; } k_t;
extern const void *NSLocaleIdentifier, *NSLocaleCurrencyCode, *NSStringTransformToLatin, *kCFStringTransformToLatin,
    *NSURLTotalFileSizeKey, *kCFURLFileSizeKey, *kCFErrorLocalizedFailureReasonKey, *NSMallocException;
extern char cx_cfconst_isa[];
const void *__CFStringMakeConstantString(const char *);
static int ok, bad;
static void chk(int c, const char *m) { if (c) ok++; else { bad++; printf("FALHA: %s\n", m); } }
static void val(const void *p, const char *want, const char *m) {
    const k_t *k = p;
    chk(k && k->isa == cx_cfconst_isa && k->flags == 0x7c8 && k->n == strlen(want) && !strcmp(k->s, want), m);
}
int main(void) {
    val(NSLocaleIdentifier, "locale:id", "NSLocaleIdentifier");
    val(NSLocaleCurrencyCode, "currency", "NSLocaleCurrencyCode");
    val(NSStringTransformToLatin, "Any-Latin", "NSStringTransformToLatin");
    val(kCFStringTransformToLatin, "Any-Latin", "kCFStringTransformToLatin");
    val(NSURLTotalFileSizeKey, "NSURLTotalFileSizeKey", "NSURLTotalFileSizeKey");
    val(kCFURLFileSizeKey, "NSURLFileSizeKey", "kCFURLFileSizeKey");
    val(kCFErrorLocalizedFailureReasonKey, "NSLocalizedFailureReason", "kCFErrorLocalizedFailureReasonKey");
    val(NSMallocException, "NSMallocException", "NSMallocException");
    const void *a = __CFStringMakeConstantString("CFBundleIdentifier");
    char buf[32]; strcpy(buf, "CFBundleIdentifier");
    const void *b = __CFStringMakeConstantString(buf);
    chk(a && a == b, "CFSTR interna: mesmo texto, mesmo objeto");
    val(a, "CFBundleIdentifier", "CFSTR conteúdo");
    chk(__CFStringMakeConstantString("outra") != a, "CFSTR textos diferentes");
    val(__CFStringMakeConstantString(""), "", "CFSTR vazio");
    chk(__CFStringMakeConstantString(0) == 0, "CFSTR nulo");
    for (int i = 0; i < 2000; i++) { char t[16]; snprintf(t, sizeof t, "k%d", i); __CFStringMakeConstantString(t); }
    chk(__CFStringMakeConstantString("k1234") == __CFStringMakeConstantString("k1234"), "CFSTR com 2000 entradas");
    printf("fdn-consts: %d/%d ok\n", ok, ok + bad);
    return bad != 0;
}
