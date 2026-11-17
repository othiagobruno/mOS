/*
 * mOS — teste de host do Keychain de userland/libsystem/security.c (SecItem*, API antiga SecKeychain*, buscas
 * SecKeychainSearch/SecIdentitySearch, atributos de senha de internet) e das constantes kSec* e CSSMOID_*.
 * Não sobe a VM: security.c é compilado para o host com um CoreFoundation mínimo (abaixo) no lugar do Foundation
 * do mOS. Os arquivos do Keychain ficam num $HOME temporário (tools/keychain-hosttest.sh).
 *
 *   sh tools/keychain-hosttest.sh        -> "keychain: N/N ok"
 *
 * O shim só implementa o que security.c usa: CFString (mesmo layout das constantes @"..."), CFData, CFNumber,
 * CFBoolean, CFDate, CFArray, CFDictionary e os seletores UTF8String / stringWithUTF8String: / stringValue.
 */
#define open mos_open
#define lstat mos_lstat
#include "../userland/libsystem/security.c"
#undef open
#undef lstat
int printf(const char *f, ...);
int snprintf(char *b, size_t n, const char *f, ...);
typedef struct FILE FILE;
FILE *fopen(const char *p, const char *m);
int fprintf(FILE *f, const char *fmt, ...);
int fclose(FILE *f);
id_ t_mkstr(const char *s);
int t_str_is(id_ v, const char *s);
long long t_num(id_ v);
#define T_DATA 20
#define T_NUM 22
/* ------------------------------------------------------------------ o que se testa */
const char kCFTypeArrayCallBacks[1], kCFTypeDictionaryKeyCallBacks[1], kCFTypeDictionaryValueCallBacks[1];
const void *kCFErrorDomainOSStatus, *kCFErrorLocalizedDescriptionKey;
/* certificados, chaves e confiança: fora deste teste (seccerttest/seckeytest cobrem) */
int cx_cert_fields(const u8 *d, u32 n, const u8 **a, u32 *b, const u8 **c, u32 *e, const u8 **f, u32 *g, char *l, u32 lc) {
    (void)d; (void)n; (void)a; (void)b; (void)c; (void)e; (void)f; (void)g; (void)l; (void)lc; return -1;
}
int cx_seckey_describe(const u8 *e, u32 n, int *p, int *t, u8 k[20]) { (void)e; (void)n; (void)p; (void)t; (void)k; return -1; }
id_ cx_seckey_import(const u8 *d, u32 n, int p) { (void)d; (void)n; (void)p; return 0; }
id_ SecKeyCopyExternalRepresentation(id_ k, id_ *e) { (void)k; (void)e; return 0; }
int cx_sectrust_eval(const u8 *const *ders, const u32 *lens, unsigned n, const u8 *const *anc, const u32 *alens, unsigned na,
                     int sys, int pol, const char *host, long long at, char *err, unsigned el) {
    (void)ders; (void)lens; (void)n; (void)anc; (void)alens; (void)na; (void)sys; (void)pol; (void)host; (void)at; (void)err; (void)el;
    return -1;
}
int cx_ts_anchors(const u8 **ders, u32 *lens, int max, u8 ***own) { (void)ders; (void)lens; (void)max; (void)own; return 0; }
int cx_ts_denied(const u8 *der, u32 n) { (void)der; (void)n; return 0; }

static int ok, total;
static void check(int c, const char *what) { total++; if (c) ok++; else printf("FALHA: %s\n", what); }
#define S(lit) t_mkstr(lit)
#define str_is t_str_is
static int data_is(id_ v, const char *s) { return v && CFGetTypeID(v) == T_DATA && CFDataGetLength(v) == (long)strlen(s) && !memcmp(CFDataGetBytePtr(v), s, strlen(s)); }
static id_ num(int v) { return CFNumberCreate(0, 9, &v); }
static id_ inet_q(const char *srv, const char *acc) {
    id_ q = CFDictionaryCreateMutable(0, 0, 0, 0);
    CFDictionarySetValue(q, K_(kSecClass), K_(kSecClassInternetPassword));
    if (srv) CFDictionarySetValue(q, K_(kSecAttrServer), t_mkstr(srv));
    if (acc) CFDictionarySetValue(q, K_(kSecAttrAccount), t_mkstr(acc));
    return q;
}
#define FC(s) FOURCC(s[0], s[1], s[2], s[3])

static void constantes(void) {
    check(str_is(K_(kSecAttrProtocolHTTPS), "htps") && str_is(K_(kSecAttrProtocolFTP), "ftp ") &&
          str_is(K_(kSecAttrAuthenticationTypeHTMLForm), "form") && str_is(K_(kSecAttrPort), "port"), "valores de kSecAttr*");
    check(str_is(K_(kSecOIDBasicConstraints), "2.5.29.19") && str_is(K_(kSecOIDX509V1ValidityNotBefore), "2.16.840.1.113741.2.1.1.1.6") &&
          str_is(K_(kSecOIDServerAuth), "1.3.6.1.5.5.7.3.1") && str_is(K_(kSecOIDAPPLE_EXTENSION_CODE_SIGNING), "1.2.840.113635.100.6.1"),
          "kSecOID* pontilhados");
    check(CSSMOID_SHA1WithRSA.Length == 9 && !memcmp(CSSMOID_SHA1WithRSA.Data, "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05", 9) &&
          CSSMOID_KeyUsage.Length == 3 && !memcmp(CSSMOID_KeyUsage.Data, "\x55\x1d\x0f", 3), "CSSMOID_* em DER");
}

static void senha_internet(void) {
    id_ q = inet_q("example.com", "ana");
    CFDictionarySetValue(q, K_(kSecAttrProtocol), K_(kSecAttrProtocolHTTPS));
    CFDictionarySetValue(q, K_(kSecAttrPort), num(443));
    CFDictionarySetValue(q, K_(kSecAttrPath), S("/login"));
    CFDictionarySetValue(q, K_(kSecAttrAuthenticationType), K_(kSecAttrAuthenticationTypeHTMLForm));
    CFDictionarySetValue(q, K_(kSecAttrSecurityDomain), S("reino"));
    CFDictionarySetValue(q, K_(kSecAttrComment), S("comentário"));
    CFDictionarySetValue(q, K_(kSecAttrIsInvisible), kCFBooleanTrue);
    CFDictionarySetValue(q, K_(kSecValueData), cx_data_new("s443", 4));
    check(SecItemAdd(q, 0) == 0, "SecItemAdd inet com protocolo/porta/caminho");
    check(SecItemAdd(q, 0) == errSecDuplicateItem, "mesma chave primária = duplicado");
    CFDictionarySetValue(q, K_(kSecAttrPort), num(8443));
    CFDictionarySetValue(q, K_(kSecValueData), cx_data_new("s8443", 5));
    check(SecItemAdd(q, 0) == 0, "outra porta = outro item");

    id_ m = inet_q("example.com", "ana");
    CFDictionarySetValue(m, K_(kSecAttrPort), num(443));
    CFDictionarySetValue(m, K_(kSecReturnAttributes), kCFBooleanTrue);
    CFDictionarySetValue(m, K_(kSecReturnData), kCFBooleanTrue);
    id_ r = 0;
    check(SecItemCopyMatching(m, &r) == 0 && r, "busca pela porta 443");
    id_ port = r ? CFDictionaryGetValue(r, K_(kSecAttrPort)) : 0;
    check(port && t_num(port) == 443, "kSecAttrPort volta CFNumber 443");
    check(r && str_is(CFDictionaryGetValue(r, K_(kSecAttrProtocol)), "htps") && str_is(CFDictionaryGetValue(r, K_(kSecAttrPath)), "/login") &&
          str_is(CFDictionaryGetValue(r, K_(kSecAttrAuthenticationType)), "form") && str_is(CFDictionaryGetValue(r, K_(kSecAttrSecurityDomain)), "reino"),
          "protocolo, caminho, autenticação e domínio");
    check(r && CFDictionaryGetValue(r, K_(kSecAttrIsInvisible)) == kCFBooleanTrue && data_is(CFDictionaryGetValue(r, K_(kSecValueData)), "s443"),
          "invisível = kCFBooleanTrue e o segredo certo");
    check(r && str_is(CFDictionaryGetValue(r, K_(kSecAttrComment)), "comentário"), "comentário em UTF-8");
    CFDictionarySetValue(m, K_(kSecAttrPort), num(80));
    check(SecItemCopyMatching(m, &r) == errSecItemNotFound, "porta 80 não acha");

    /* API antiga: o que vem 0/NULL não filtra; o que vem preenchido filtra */
    u32 pl = 0; void *pw = 0; id_ item = 0;
    check(SecKeychainFindInternetPassword(0, 11, "example.com", 0, 0, 3, "ana", 0, 0, 8443, FC("htps"), 0, &pl, &pw, &item) == 0 &&
          pl == 5 && !memcmp(pw, "s8443", 5), "FindInternetPassword pela porta 8443");
    SecKeychainItemFreeContent(0, pw);
    check(SecKeychainFindInternetPassword(0, 11, "example.com", 0, 0, 3, "ana", 0, 0, 443, FC("ftp "), 0, &pl, &pw, 0) == errSecItemNotFound,
          "protocolo diferente não acha");
    check(SecKeychainFindInternetPassword(0, 11, "example.com", 0, 0, 3, "ana", 6, "/login", 0, 0, FC("form"), &pl, 0, 0) == 0,
          "caminho + autenticação acham sem porta");
    check(SecKeychainAddInternetPassword(0, 8, "host.dev", 0, 0, 2, "bo", 4, "/api", 21, FC("ftps"), FC("dflt"), 3, "ftp", &item) == 0 && item,
          "AddInternetPassword grava porta/protocolo/caminho");
    id_ m2 = inet_q("host.dev", 0);
    CFDictionarySetValue(m2, K_(kSecAttrPath), S("/api"));
    CFDictionarySetValue(m2, K_(kSecAttrProtocol), K_(kSecAttrProtocolFTPS));
    CFDictionarySetValue(m2, K_(kSecReturnAttributes), kCFBooleanTrue);
    r = 0;
    check(SecItemCopyMatching(m2, &r) == 0 && r && t_num(CFDictionaryGetValue(r, K_(kSecAttrPort))) == 21 &&
          str_is(CFDictionaryGetValue(r, K_(kSecAttrLabel)), "host.dev"), "o item da API antiga aparece no SecItem");

    /* SecKeychainItemCopyAttributesAndData: porta, protocolo e autenticação voltam como UInt32 */
    u32 tags[] = {FC("port"), FC("ptcl"), FC("atyp"), FC("srvr"), FC("cdat"), FC("icmt")}, fmts[6] = {0};
    kcattrinfo_t info = {6, tags, fmts};
    kcattrlist_t *al = 0; u32 cls = 0, dl = 0; void *data = 0;
    check(SecKeychainItemCopyAttributesAndData(item, &info, &cls, (void **)&al, &dl, &data) == 0 && al && al->count == 6, "CopyAttributesAndData");
    if (al) {
        u32 v0 = 0, v1 = 0, v2 = 0;
        if (al->attr[0].length == 4) memcpy(&v0, al->attr[0].data, 4);
        if (al->attr[1].length == 4) memcpy(&v1, al->attr[1].data, 4);
        if (al->attr[2].length == 4) memcpy(&v2, al->attr[2].data, 4);
        check(cls == FC("inet") && v0 == 21 && v1 == FC("ftps") && v2 == FC("dflt"), "porta 21, 'ftps' e 'dflt' em UInt32");
        check(al->attr[3].length == 8 && !memcmp(al->attr[3].data, "host.dev", 8) && al->attr[4].length == 16 &&
              ((char *)al->attr[4].data)[14] == 'Z' && al->attr[5].length == 0 && !al->attr[5].data, "servidor, data e atributo ausente");
        check(dl == 3 && !memcmp(data, "ftp", 3), "segredo junto");
        SecKeychainItemFreeAttributesAndData(al, data);
    }
    /* mudar atributos pela API antiga */
    u32 np = 2121;
    kcattr_t mods[2] = {{FC("port"), 4, &np}, {FC("icmt"), 5, "novo!"}};
    kcattrlist_t ml = {2, mods};
    check(SecKeychainItemModifyAttributesAndData(item, &ml, 0, 0) == 0, "ModifyAttributesAndData com atributos");
    kcattr_t bad[1] = {{FC("zzzz"), 1, "x"}};
    kcattrlist_t bl = {1, bad};
    check(SecKeychainItemModifyContent(item, &bl, 0, 0) == errSecNoSuchAttr, "tag desconhecida = errSecNoSuchAttr");
    r = 0;
    id_ m3 = inet_q("host.dev", "bo");
    CFDictionarySetValue(m3, K_(kSecAttrPort), num(2121));
    CFDictionarySetValue(m3, K_(kSecReturnAttributes), kCFBooleanTrue);
    check(SecItemCopyMatching(m3, &r) == 0 && r && str_is(CFDictionaryGetValue(r, K_(kSecAttrComment)), "novo!"), "porta e comentário mudados");
}

static void buscas(void) {
    id_ g = CFDictionaryCreateMutable(0, 0, 0, 0);
    CFDictionarySetValue(g, K_(kSecClass), K_(kSecClassGenericPassword));
    CFDictionarySetValue(g, K_(kSecAttrService), S("svc"));
    CFDictionarySetValue(g, K_(kSecAttrAccount), S("u1"));
    CFDictionarySetValue(g, K_(kSecValueData), cx_data_new("g1", 2));
    check(SecItemAdd(g, 0) == 0, "genp para a busca");

    kcattr_t a[1] = {{FC("srvr"), 11, "example.com"}};
    kcattrlist_t l = {1, a};
    id_ s = 0, it = 0;
    check(SecKeychainSearchCreateFromAttributes(0, FC("inet"), &l, &s) == 0 && s, "SearchCreateFromAttributes inet");
    int n = 0;
    while (SecKeychainSearchCopyNext(s, &it) == 0) n++;
    check(n == 2, "acha os 2 itens de example.com");
    check(SecKeychainSearchCopyNext(s, &it) == errSecItemNotFound, "depois do fim = errSecItemNotFound");

    unsigned short port = 443;
    kcattr_t a2[2] = {{FC("srvr"), 11, "example.com"}, {FC("port"), 2, &port}};
    kcattrlist_t l2 = {2, a2};
    check(SecKeychainSearchCreateFromAttributes(0, FC("inet"), &l2, &s) == 0 && SecKeychainSearchCopyNext(s, &it) == 0 &&
          SecKeychainSearchCopyNext(s, &it) == errSecItemNotFound, "porta como UInt16 filtra (1 item)");
    check(SecKeychainSearchCreateFromAttributes(0, FC("genp"), 0, &s) == 0 && SecKeychainSearchCopyNext(s, &it) == 0, "genp sem atributos");
    u32 cls = 0, pl = 0; void *pw = 0;
    check(SecKeychainItemCopyContent(it, &cls, 0, &pl, &pw) == 0 && cls == FC("genp") && pl == 2 && !memcmp(pw, "g1", 2),
          "o item da busca serve para CopyContent");
    check(SecKeychainSearchCreateFromAttributes(0, 0, 0, &s) == 0, "classe 0 = qualquer senha");
    n = 0;
    while (SecKeychainSearchCopyNext(s, &it) == 0) n++;
    check(n == 4, "4 senhas no total");
    kcattr_t a3[1] = {{FC("zzzz"), 1, "x"}};
    kcattrlist_t l3 = {1, a3};
    check(SecKeychainSearchCreateFromAttributes(0, FC("inet"), &l3, &s) == errSecNoSuchAttr, "tag ruim na busca");

    check(SecIdentitySearchCreate(0, 0, &s) == 0 && SecIdentitySearchCopyNext(s, &it) == errSecItemNotFound, "sem identidade");
    id_ idq = CFDictionaryCreateMutable(0, 0, 0, 0);
    CFDictionarySetValue(idq, K_(kSecClass), K_(kSecClassIdentity));
    CFDictionarySetValue(idq, K_(kSecValueData), cx_data_new("\x30\x03\x02\x01\x05", 5));
    check(SecItemAdd(idq, 0) == 0, "identidade gravada");
    id_ cert = 0;
    check(SecIdentitySearchCreate(0, 0, &s) == 0 && SecIdentitySearchCopyNext(s, &it) == 0 &&
          SecIdentityCopyCertificate(it, &cert) == 0 && CFDataGetLength(cert) == 5, "SecIdentitySearch acha e dá o certificado");
    check(SecKeychainSearchCopyNext(s, &it) == errSecParam, "busca de identidade não serve para SecKeychainSearchCopyNext");

    id_ list = 0;
    check(SecKeychainCopySearchList(&list) == 0 && CFArrayGetCount(list) == 1 && SecKeychainCopyDomainSearchList(1, &list) == 0, "lista de busca");
}

/* segunda execução (outro processo): os atributos novos saem do arquivo cifrado */
static void reler(void) {
    id_ m = inet_q("example.com", "ana");
    CFDictionarySetValue(m, K_(kSecAttrPort), num(8443));
    CFDictionarySetValue(m, K_(kSecReturnAttributes), kCFBooleanTrue);
    CFDictionarySetValue(m, K_(kSecReturnData), kCFBooleanTrue);
    id_ r = 0;
    check(SecItemCopyMatching(m, &r) == 0 && r && data_is(CFDictionaryGetValue(r, K_(kSecValueData)), "s8443") &&
          CFDictionaryGetValue(r, K_(kSecAttrIsInvisible)) == kCFBooleanTrue && str_is(CFDictionaryGetValue(r, K_(kSecAttrPath)), "/login"),
          "relido do arquivo: porta, invisível e caminho");
}

/* arquivo versão 1 (texto, 7 campos por linha, de antes destes atributos) continua legível */
static void formato_antigo(const char *home) {
    char p[600];
    snprintf(p, sizeof p, "%s/Library/Keychains/mos-keychain.txt", home);
    FILE *f = fopen(p, "w");
    /* classe id cdat mdat dados svce acct labl desc gena srvr agrp */
    fprintf(f, "mOS-keychain 1\ninet 7 0 0 7365 - 616e61 - - - 6f6c642e6f7267 -\n");
    fclose(f);
    u32 pl = 0; void *pw = 0;
    check(SecKeychainFindInternetPassword(0, 7, "old.org", 0, 0, 3, "ana", 0, 0, 0, 0, 0, &pl, &pw, 0) == 0 && pl == 2 && !memcmp(pw, "se", 2),
          "linha antiga de 7 campos");
    check(SecKeychainFindInternetPassword(0, 7, "old.org", 0, 0, 3, "ana", 0, 0, 443, 0, 0, &pl, &pw, 0) == errSecItemNotFound,
          "item antigo não tem porta");
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "antigo")) formato_antigo(getenv("HOME"));
    else if (argc > 1 && !strcmp(argv[1], "reler")) reler();
    else { constantes(); senha_internet(); buscas(); }
    printf("keychain: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
