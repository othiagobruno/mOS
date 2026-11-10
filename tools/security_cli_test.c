/* Keychain falso em memória para o teste de host de userland/cli-macos/security.c (tools/security-cli-teste.sh).
 * Implementa só a API SecKeychain* / SecTrustSettings* / CF* que o /usr/bin/security chama, com a mesma semântica
 * de retorno do userland/libsystem/security.c (errSecItemNotFound, errSecDuplicateItem). O Keychain persiste em
 * um arquivo texto ($SECURITY_TESTE_KC) para que várias execuções do binário vejam os mesmos itens. Raízes do
 * sistema: o DER em $SECURITY_TESTE_DER (uma raiz só). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int32_t OSStatus;
typedef uint32_t u32;
typedef long CFIndex;
typedef struct { u32 tag; u32 length; void *data; } SecKeychainAttribute;
typedef struct { u32 count; SecKeychainAttribute *attr; } SecKeychainAttributeList;

#define FOURCC(a, b, c, d) (((u32)(a) << 24) | ((u32)(b) << 16) | ((u32)(c) << 8) | (u32)(d))
#define NITEM 64

typedef struct { int usado, inet; char chave[256], conta[256], senha[256]; } item_t;
static item_t itens[NITEM];

static const char *arq(void) { const char *p = getenv("SECURITY_TESTE_KC"); return p ? p : "/tmp/security-teste.kc"; }
static void carrega(void) {
    memset(itens, 0, sizeof itens);
    FILE *f = fopen(arq(), "r");
    if (!f) return;
    for (int i = 0; i < NITEM; i++) {
        item_t *it = &itens[i];
        if (fscanf(f, "%d %255s %255s %255s\n", &it->inet, it->chave, it->conta, it->senha) != 4) break;
        it->usado = 1;
    }
    fclose(f);
}
static void grava(void) {
    FILE *f = fopen(arq(), "w");
    if (!f) return;
    for (int i = 0; i < NITEM; i++)
        if (itens[i].usado) fprintf(f, "%d %s %s %s\n", itens[i].inet, itens[i].chave, itens[i].conta, itens[i].senha);
    fclose(f);
}
static void copia(char *dst, const char *src, u32 n) {
    if (!src || !n) { strcpy(dst, "-"); return; }
    if (n > 255) n = 255;
    memcpy(dst, src, n);
    dst[n] = 0;
}
static int bate(const item_t *it, int inet, const char *k, u32 kl, const char *a, u32 al) {
    char kk[256], aa[256];
    if (!it->usado || it->inet != inet) return 0;
    if (k && kl) { copia(kk, k, kl); if (strcmp(kk, it->chave)) return 0; }
    if (a && al) { copia(aa, a, al); if (strcmp(aa, it->conta)) return 0; }
    return 1;
}
static OSStatus acha(int inet, const char *k, u32 kl, const char *a, u32 al, u32 *pl, void **pw, void **item) {
    carrega();
    for (int i = 0; i < NITEM; i++) {
        if (!bate(&itens[i], inet, k, kl, a, al)) continue;
        if (pl) *pl = (u32)strlen(itens[i].senha);
        if (pw) *pw = strdup(itens[i].senha);
        if (item) *item = (void *)(intptr_t)(i + 1);
        return 0;
    }
    return -25300;
}
static OSStatus poe(int inet, const char *k, u32 kl, const char *a, u32 al, u32 pl, const void *pw, void **item) {
    carrega();
    for (int i = 0; i < NITEM; i++) if (bate(&itens[i], inet, k, kl, a, al)) return -25299;
    for (int i = 0; i < NITEM; i++) {
        if (itens[i].usado) continue;
        itens[i].usado = 1;
        itens[i].inet = inet;
        copia(itens[i].chave, k, kl);
        copia(itens[i].conta, a, al);
        copia(itens[i].senha, pw, pl);
        grava();
        if (item) *item = (void *)(intptr_t)(i + 1);
        return 0;
    }
    return -108;
}

OSStatus SecKeychainFindGenericPassword(void *kc, u32 sl, const char *svc, u32 al, const char *acc, u32 *pl, void **pw, void **item) {
    (void)kc; return acha(0, svc, sl, acc, al, pl, pw, item);
}
OSStatus SecKeychainAddGenericPassword(void *kc, u32 sl, const char *svc, u32 al, const char *acc, u32 pl, const void *pw, void **item) {
    (void)kc; return poe(0, svc, sl, acc, al, pl, pw, item);
}
OSStatus SecKeychainFindInternetPassword(void *kc, u32 srvl, const char *srv, u32 dl, const char *dom, u32 al, const char *acc,
                                         u32 pathl, const char *path, unsigned short port, u32 proto, u32 auth,
                                         u32 *pl, void **pw, void **item) {
    (void)kc; (void)dl; (void)dom; (void)pathl; (void)path; (void)port; (void)proto; (void)auth;
    return acha(1, srv, srvl, acc, al, pl, pw, item);
}
OSStatus SecKeychainAddInternetPassword(void *kc, u32 srvl, const char *srv, u32 dl, const char *dom, u32 al, const char *acc,
                                        u32 pathl, const char *path, unsigned short port, u32 proto, u32 auth,
                                        u32 pl, const void *pw, void **item) {
    (void)kc; (void)dl; (void)dom; (void)pathl; (void)path; (void)port; (void)proto; (void)auth;
    return poe(1, srv, srvl, acc, al, pl, pw, item);
}
static item_t *do_ref(void *item) {
    intptr_t i = (intptr_t)item - 1;
    carrega();
    return i >= 0 && i < NITEM && itens[i].usado ? &itens[i] : 0;
}
OSStatus SecKeychainItemModifyAttributesAndData(void *item, const void *attrs, u32 len, const void *data) {
    (void)attrs;
    item_t *it = do_ref(item);
    if (!it) return -25300;
    if (data) copia(it->senha, data, len);
    grava();
    return 0;
}
OSStatus SecKeychainItemDelete(void *item) {
    item_t *it = do_ref(item);
    if (!it) return -25300;
    it->usado = 0;
    grava();
    return 0;
}
OSStatus SecKeychainItemCopyContent(void *item, u32 *cls, SecKeychainAttributeList *al, u32 *len, void **data) {
    item_t *it = do_ref(item);
    if (!it) return -25300;
    if (cls) *cls = it->inet ? FOURCC('i','n','e','t') : FOURCC('g','e','n','p');
    if (al)
        for (u32 i = 0; i < al->count; i++) {
            SecKeychainAttribute *a = &al->attr[i];
            const char *v = 0;
            if (a->tag == FOURCC('a','c','c','t')) v = it->conta;
            else if (a->tag == (it->inet ? FOURCC('s','r','v','r') : FOURCC('s','v','c','e'))) v = it->chave;
            else if (a->tag == 7) v = it->chave;
            if (v) { a->data = strdup(v); a->length = (u32)strlen(v); }
        }
    if (len) *len = (u32)strlen(it->senha);
    if (data) *data = strdup(it->senha);
    return 0;
}
OSStatus SecKeychainItemFreeContent(SecKeychainAttributeList *al, void *data) {
    if (al) for (u32 i = 0; i < al->count; i++) free(al->attr[i].data);
    free(data);
    return 0;
}
OSStatus SecKeychainGetPath(void *kc, u32 *len, char *path) {
    (void)kc;
    const char *p = "/Users/teste/Library/Keychains/login.keychain-db";
    if (!len || *len <= strlen(p)) return -50;
    strcpy(path, p);
    *len = (u32)strlen(p);
    return 0;
}

/* CF mínimo: um array de um certificado; o certificado e o CFData são o mesmo buffer */
typedef struct { long n; uint8_t *b; } dado_t;
static dado_t raiz;
static const void *lista[1] = {&raiz};
OSStatus SecTrustSettingsCopyCertificates(int domain, const void **certs) {
    if (domain != 2) return -25263;                           /* kSecTrustSettingsDomainSystem */
    const char *p = getenv("SECURITY_TESTE_DER");
    FILE *f = p ? fopen(p, "rb") : 0;
    if (!f) return -25263;
    raiz.b = malloc(1 << 16);
    raiz.n = (long)fread(raiz.b, 1, 1 << 16, f);
    fclose(f);
    *certs = lista;
    return 0;
}
void *SecCertificateCopyData(const void *cert) { return (void *)cert; }
CFIndex CFArrayGetCount(const void *a) { return a == lista ? 1 : 0; }
const void *CFArrayGetValueAtIndex(const void *a, CFIndex i) { return a == lista && i == 0 ? lista[0] : 0; }
CFIndex CFDataGetLength(const void *d) { return ((const dado_t *)d)->n; }
const uint8_t *CFDataGetBytePtr(const void *d) { return ((const dado_t *)d)->b; }
void CFRelease(const void *o) { (void)o; }
